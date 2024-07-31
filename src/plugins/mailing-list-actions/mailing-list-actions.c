/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2004 Meilof Veeningen <meilof@wanadoo.nl>
 *
 */

#include "evolution-config.h"

#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include <e-util/e-util.h>

#include "shell/e-shell-view.h"
#include "shell/e-shell-window.h"
#include "shell/e-shell-window-actions.h"

#include "composer/e-msg-composer.h"

#include "mail/e-mail-browser.h"
#include "mail/e-mail-reader.h"
#include "mail/e-mail-view.h"
#include "mail/em-composer-utils.h"
#include "mail/em-config.h"
#include "mail/em-utils.h"
#include "mail/message-list.h"

/* EAlert Message IDs */
#define MESSAGE_PREFIX			"org.gnome.mailing-list-actions:"
#define MESSAGE_NO_ACTION		MESSAGE_PREFIX "no-action"
#define MESSAGE_NO_HEADER		MESSAGE_PREFIX "no-header"
#define MESSAGE_ASK_SEND_MESSAGE	MESSAGE_PREFIX "ask-send-message"
#define MESSAGE_MALFORMED_HEADER	MESSAGE_PREFIX "malformed-header"
#define MESSAGE_POSTING_NOT_ALLOWED	MESSAGE_PREFIX "posting-not-allowed"

typedef enum {
	EMLA_ACTION_HELP,
	EMLA_ACTION_UNSUBSCRIBE,
	EMLA_ACTION_SUBSCRIBE,
	EMLA_ACTION_POST,
	EMLA_ACTION_OWNER,
	EMLA_ACTION_ARCHIVE,
	EMLA_ACTION_ARCHIVED_AT
} EmlaAction;

typedef struct {
	/* action enumeration */
	EmlaAction action;

	/* whether the user needs to edit a mailto:
	 * message (e.g. for post action) */
	gboolean interactive;

	/* header representing the action */
	const gchar *header;
} EmlaActionHeader;

const EmlaActionHeader emla_action_headers[] = {
	{ EMLA_ACTION_HELP,        FALSE, "List-Help" },
	{ EMLA_ACTION_UNSUBSCRIBE, TRUE,  "List-Unsubscribe" },
	{ EMLA_ACTION_SUBSCRIBE,   FALSE, "List-Subscribe" },
	{ EMLA_ACTION_POST,        TRUE,  "List-Post" },
	{ EMLA_ACTION_OWNER,       TRUE,  "List-Owner" },
	{ EMLA_ACTION_ARCHIVE,     FALSE, "List-Archive" },
	{ EMLA_ACTION_ARCHIVED_AT, FALSE, "Archived-At" }
};

gboolean	mailing_list_actions_mail_browser_init
						(EUIManager *ui_manager,
						 EMailBrowser *browser);
gboolean	mailing_list_actions_mail_shell_view_init
						(EUIManager *ui_manager,
						 EShellView *shell_view);
gint		e_plugin_lib_enable		(EPlugin *ep,
						 gint enable);

gint
e_plugin_lib_enable (EPlugin *ep,
                     gint enable)
{
	return 0;
}

typedef struct _AsyncContext AsyncContext;

struct _AsyncContext {
	EActivity *activity;
	EMailReader *reader;
	EmlaAction action;
	const gchar *message_uid; /* In the Camel string pool */
};

static void
async_context_free (AsyncContext *context)
{
	if (context->activity != NULL)
		g_object_unref (context->activity);

	if (context->reader != NULL)
		g_object_unref (context->reader);

	camel_pstring_free (context->message_uid);

	g_slice_free (AsyncContext, context);
}

typedef struct _SendMessageData {
	gchar *url;
	gchar *uid;
} SendMessageData;

static void
send_message_composer_created_cb (GObject *source_object,
				  GAsyncResult *result,
				  gpointer user_data)
{
	SendMessageData *smd = user_data;
	EMsgComposer *composer;
	GError *error = NULL;

	g_return_if_fail (smd != NULL);

	composer = e_msg_composer_new_finish (result, &error);
	if (error) {
		g_warning ("%s: Failed to create msg composer: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
	} else {
		EComposerHeaderTable *table;

		/* directly send message */
		e_msg_composer_setup_from_url (composer, smd->url);
		table = e_msg_composer_get_header_table (composer);

		if (smd->uid)
			e_composer_header_table_set_identity_uid (table, smd->uid, NULL, NULL);

		e_msg_composer_send (composer);
	}

	g_free (smd->url);
	g_free (smd->uid);
	g_slice_free (SendMessageData, smd);
}

static void
emla_list_action_cb (CamelFolder *folder,
                     GAsyncResult *result,
                     AsyncContext *context)
{
	const gchar *header = NULL, *headerpos;
	gchar *end, *url = NULL;
	gint t;
	EAlertSink *alert_sink;
	CamelMimeMessage *message;
	gint send_message_response;
	EShell *shell;
	ESource *source;
	EMailBackend *backend;
	ESourceRegistry *registry;
	EShellBackend *shell_backend;
	GtkWindow *window;
	CamelStore *store;
	const gchar *uid;
	GError *error = NULL;

	window = e_mail_reader_get_window (context->reader);
	backend = e_mail_reader_get_backend (context->reader);
	alert_sink = e_activity_get_alert_sink (context->activity);

	shell_backend = E_SHELL_BACKEND (backend);
	shell = e_shell_backend_get_shell (shell_backend);

	registry = e_shell_get_registry (shell);

	message = camel_folder_get_message_finish (folder, result, &error);

	if (e_activity_handle_cancellation (context->activity, error)) {
		g_warn_if_fail (message == NULL);
		async_context_free (context);
		g_error_free (error);
		return;

	} else if (error != NULL) {
		g_warn_if_fail (message == NULL);
		e_alert_submit (
			alert_sink, "mail:no-retrieve-message",
			error->message, NULL);
		async_context_free (context);
		g_error_free (error);
		return;
	}

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	/* Finalize the activity here so we don't leave a
	 * message in the task bar while display a dialog. */
	e_activity_set_state (context->activity, E_ACTIVITY_COMPLETED);
	g_object_unref (context->activity);
	context->activity = NULL;

	store = camel_folder_get_parent_store (folder);
	uid = camel_service_get_uid (CAMEL_SERVICE (store));
	source = e_source_registry_ref_source (registry, uid);

	/* Reuse this to hold the mail identity UID. */
	uid = NULL;

	if (source != NULL) {
		ESourceMailAccount *extension;
		const gchar *extension_name;

		extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
		extension = e_source_get_extension (source, extension_name);
		uid = e_source_mail_account_get_identity_uid (extension);
	}

	for (t = 0; t < G_N_ELEMENTS (emla_action_headers); t++) {
		if (emla_action_headers[t].action == context->action) {
			header = camel_medium_get_header (
				CAMEL_MEDIUM (message),
				emla_action_headers[t].header);
			if (header != NULL)
				break;
		}
	}

	if (!header) {
		/* there was no header matching the action */
		e_alert_run_dialog_for_args (window, MESSAGE_NO_HEADER, NULL);
		goto exit;
	}

	headerpos = header;

	if (context->action == EMLA_ACTION_POST) {
		while (*headerpos == ' ') headerpos++;
		if (g_ascii_strcasecmp (headerpos, "NO") == 0) {
			e_alert_run_dialog_for_args (
				window, MESSAGE_POSTING_NOT_ALLOWED, NULL);
			goto exit;
		}
	}

	/* parse the action value */
	while (*headerpos) {
		/* skip whitespace */
		while (*headerpos == ' ') headerpos++;
		if (*headerpos != '<' || (end = strchr (headerpos++, '>')) == NULL) {
			e_alert_run_dialog_for_args (
				window, MESSAGE_MALFORMED_HEADER,
				emla_action_headers[t].header, header, NULL);
			goto exit;
		}

		/* get URL portion */
		url = g_strndup (headerpos, end - headerpos);

		if (url && strncmp (url, "mailto:", 6) == 0) {
			if (emla_action_headers[t].interactive)
				send_message_response = GTK_RESPONSE_NO;
			else
				send_message_response = e_alert_run_dialog_for_args (
					window, MESSAGE_ASK_SEND_MESSAGE,
					url, NULL);

			if (send_message_response == GTK_RESPONSE_YES) {
				SendMessageData *smd;

				smd = g_slice_new0 (SendMessageData);
				smd->url = g_strdup (url);
				smd->uid = g_strdup (uid);

				e_msg_composer_new (shell, send_message_composer_created_cb, smd);
			} else if (send_message_response == GTK_RESPONSE_NO) {
				/* show composer */
				em_utils_compose_new_message_with_mailto_and_selection (shell, url, folder, context->message_uid);
			}

			goto exit;
		} else if (url && *url) {
			if (context->action == EMLA_ACTION_ARCHIVED_AT) {
				GtkClipboard *clipboard;

				clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
				gtk_clipboard_set_text (clipboard, url, -1);
			} else {
				e_show_uri (window, url);
			}
			goto exit;
		}
		g_free (url);
		url = NULL;
		headerpos = end++;

		/* ignore everything 'till next comma */
		headerpos = strchr (headerpos, ',');
		if (!headerpos)
			break;
		headerpos++;
	}

	/* if we got here, there's no valid action */
	e_alert_run_dialog_for_args (window, MESSAGE_NO_ACTION, header, NULL);

exit:
	if (source != NULL)
		g_object_unref (source);

	g_object_unref (message);
	g_free (url);

	async_context_free (context);
}

static void
emla_list_action (EMailReader *reader,
                  EmlaAction action)
{
	EActivity *activity;
	AsyncContext *context;
	GCancellable *cancellable;
	CamelFolder *folder;
	GPtrArray *uids;
	const gchar *message_uid;

	uids = e_mail_reader_get_selected_uids (reader);
	g_return_if_fail (uids != NULL && uids->len == 1);
	message_uid = g_ptr_array_index (uids, 0);

	activity = e_mail_reader_new_activity (reader);
	cancellable = e_activity_get_cancellable (activity);

	context = g_slice_new0 (AsyncContext);
	context->activity = activity;
	context->reader = g_object_ref (reader);
	context->action = action;
	context->message_uid = camel_pstring_strdup (message_uid);

	folder = e_mail_reader_ref_folder (reader);

	camel_folder_get_message (
		folder, message_uid, G_PRIORITY_DEFAULT,
		cancellable, (GAsyncReadyCallback)
		emla_list_action_cb, context);

	g_clear_object (&folder);

	g_ptr_array_unref (uids);
}

static void
action_mailing_list_archive_cb (EUIAction *action,
				GVariant *parameter,
				gpointer user_data)
{
	EMailReader *reader = user_data;
	emla_list_action (reader, EMLA_ACTION_ARCHIVE);
}

static void
action_mailing_list_archived_at_cb (EUIAction *action,
				    GVariant *parameter,
				    gpointer user_data)
{
	EMailReader *reader = user_data;
	emla_list_action (reader, EMLA_ACTION_ARCHIVED_AT);
}

static void
action_mailing_list_help_cb (EUIAction *action,
			     GVariant *parameter,
			     gpointer user_data)
{
	EMailReader *reader = user_data;
	emla_list_action (reader, EMLA_ACTION_HELP);
}

static void
action_mailing_list_owner_cb (EUIAction *action,
			      GVariant *parameter,
			      gpointer user_data)
{
	EMailReader *reader = user_data;
	emla_list_action (reader, EMLA_ACTION_OWNER);
}

static void
action_mailing_list_post_cb (EUIAction *action,
			     GVariant *parameter,
			     gpointer user_data)
{
	EMailReader *reader = user_data;
	emla_list_action (reader, EMLA_ACTION_POST);
}

static void
action_mailing_list_subscribe_cb (EUIAction *action,
				  GVariant *parameter,
				  gpointer user_data)
{
	EMailReader *reader = user_data;
	emla_list_action (reader, EMLA_ACTION_SUBSCRIBE);
}

static void
action_mailing_list_unsubscribe_cb (EUIAction *action,
				    GVariant *parameter,
				    gpointer user_data)
{
	EMailReader *reader = user_data;
	emla_list_action (reader, EMLA_ACTION_UNSUBSCRIBE);
}

static void
update_actions_cb (EMailReader *reader,
                   guint32 state,
                   EUIManager *ui_manager)
{
	EUIActionGroup *action_group;
	gboolean sensitive;

	action_group = e_ui_manager_get_action_group (ui_manager, "mailing-list");

	sensitive = (state & E_MAIL_READER_SELECTION_IS_MAILING_LIST) != 0
		 && (state & E_MAIL_READER_SELECTION_SINGLE) != 0;

	e_ui_action_group_set_sensitive (action_group, sensitive);

	if (sensitive) {
		EMailDisplay *mail_display;
		EMailPartList *part_list;
		CamelMimeMessage *message;

		mail_display = e_mail_reader_get_mail_display (reader);
		part_list = mail_display ? e_mail_display_get_part_list (mail_display) : NULL;
		message = part_list ? e_mail_part_list_get_message (part_list) : NULL;

		if (message) {
			const gchar *header;

			header = camel_medium_get_header (CAMEL_MEDIUM (message), "Archived-At");
			sensitive = header && *header;
		}

		e_ui_action_set_sensitive (e_ui_action_group_get_action (action_group, "mailing-list-archived-at"), message && sensitive);
	}
}

static void
setup_actions (EMailReader *reader,
               EUIManager *ui_manager)
{
	static const gchar *eui =
		"<eui>"
		  "<menu id='main-menu'>"
		    "<placeholder id='custom-menus'>"
		      "<submenu action='mail-message-menu'>"
			"<placeholder id='mail-message-custom-menus'>"
			  "<submenu action='mailing-list-menu'>"
			    "<item action='mailing-list-help'/>"
			    "<item action='mailing-list-subscribe'/>"
			    "<item action='mailing-list-unsubscribe'/>"
			    "<item action='mailing-list-post'/>"
			    "<item action='mailing-list-owner'/>"
			    "<item action='mailing-list-archive'/>"
			    "<separator />"
			    "<item action='mailing-list-archived-at'/>"
			  "</submenu>"
			"</placeholder>"
		      "</submenu>"
		    "</placeholder>"
		  "</menu>"
		"</eui>";

	static const EUIActionEntry entries[] = {

		{ "mailing-list-archive",
		  NULL,
		  N_("Get List _Archive"),
		  NULL,
		  N_("Get an archive of the list this message belongs to"),
		  action_mailing_list_archive_cb, NULL, NULL, NULL },

		{ "mailing-list-archived-at",
		  NULL,
		  N_("Copy _Message Archive URL"),
		  NULL,
		  N_("Copy direct URL for the selected message in its archive"),
		  action_mailing_list_archived_at_cb, NULL, NULL, NULL },

		{ "mailing-list-help",
		  NULL,
		  N_("Get List _Usage Information"),
		  NULL,
		  N_("Get information about the usage of the list this message belongs to"),
		  action_mailing_list_help_cb, NULL, NULL, NULL },

		{ "mailing-list-owner",
		  NULL,
		  N_("Contact List _Owner"),
		  NULL,
		  N_("Contact the owner of the mailing list this message belongs to"),
		  action_mailing_list_owner_cb, NULL, NULL, NULL },

		{ "mailing-list-post",
		  NULL,
		  N_("_Post Message to List"),
		  NULL,
		  N_("Post a message to the mailing list this message belongs to"),
		  action_mailing_list_post_cb, NULL, NULL, NULL },

		{ "mailing-list-subscribe",
		  NULL,
		  N_("_Subscribe to List"),
		  NULL,
		  N_("Subscribe to the mailing list this message belongs to"),
		  action_mailing_list_subscribe_cb, NULL, NULL, NULL },

		{ "mailing-list-unsubscribe",
		  NULL,
		  N_("_Unsubscribe from List"),
		  NULL,
		  N_("Unsubscribe from the mailing list this message belongs to"),
		  action_mailing_list_unsubscribe_cb, NULL, NULL, NULL },

		/*** Menus ***/

		{ "mailing-list-menu",
		  NULL,
		  N_("Mailing _List"),
		  NULL,
		  NULL,
		  NULL, NULL, NULL, NULL }
	};

	e_ui_manager_add_actions_with_eui_data (ui_manager, "mailing-list", NULL,
		entries, G_N_ELEMENTS (entries), reader, eui);

	g_signal_connect_object (
		reader, "update-actions",
		G_CALLBACK (update_actions_cb), ui_manager, 0);
}

gboolean
mailing_list_actions_mail_browser_init (EUIManager *ui_manager,
					EMailBrowser *browser)
{
	setup_actions (E_MAIL_READER (browser), ui_manager);

	return TRUE;
}

gboolean
mailing_list_actions_mail_shell_view_init (EUIManager *ui_manager,
					   EShellView *shell_view)
{
	EShellContent *shell_content;
	EMailView *mail_view = NULL;

	shell_content = e_shell_view_get_shell_content (shell_view);
	g_object_get (shell_content, "mail-view", &mail_view, NULL);

	if (mail_view) {
		setup_actions (E_MAIL_READER (mail_view), ui_manager);
		g_clear_object (&mail_view);
	}

	return TRUE;
}
