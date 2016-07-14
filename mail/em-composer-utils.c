/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <e-util/e-util.h>

#include <libemail-engine/libemail-engine.h>

#include <em-format/e-mail-parser.h>
#include <em-format/e-mail-part-utils.h>
#include <em-format/e-mail-formatter-quote.h>

#include <shell/e-shell.h>

#include <composer/e-msg-composer.h>
#include <composer/e-composer-actions.h>
#include <composer/e-composer-post-header.h>

#include "e-mail-printer.h"
#include "e-mail-ui-session.h"
#include "em-utils.h"
#include "em-composer-utils.h"
#include "em-folder-selector.h"
#include "em-folder-tree.h"
#include "em-event.h"
#include "mail-send-recv.h"

#ifdef G_OS_WIN32
#ifdef gmtime_r
#undef gmtime_r
#endif

/* The gmtime() in Microsoft's C library is MT-safe */
#define gmtime_r(tp,tmp) (gmtime(tp)?(*(tmp)=*gmtime(tp),(tmp)):0)
#endif

typedef struct _AsyncContext AsyncContext;
typedef struct _ForwardData ForwardData;

struct _AsyncContext {
	CamelMimeMessage *message;
	EMailSession *session;
	EMsgComposer *composer;
	EActivity *activity;
	gchar *folder_uri;
	gchar *message_uid;
	gulong num_loading_handler_id;
	gulong cancelled_handler_id;
};

struct _ForwardData {
	EShell *shell;
	CamelFolder *folder;
	GPtrArray *uids;
	EMailForwardStyle style;
};

static void
async_context_free (AsyncContext *async_context)
{
	if (async_context->cancelled_handler_id) {
		GCancellable *cancellable;

		cancellable = e_activity_get_cancellable (async_context->activity);
		/* Cannot use g_cancellable_disconnect(), because when this is called
		   from inside the cancelled handler, then the GCancellable deadlocks. */
		g_signal_handler_disconnect (cancellable, async_context->cancelled_handler_id);
		async_context->cancelled_handler_id = 0;
	}

	if (async_context->num_loading_handler_id) {
		EAttachmentView *view;
		EAttachmentStore *store;

		view = e_msg_composer_get_attachment_view (async_context->composer);
		store = e_attachment_view_get_store (view);

		e_signal_disconnect_notify_handler (store, &async_context->num_loading_handler_id);
	}

	g_clear_object (&async_context->message);
	g_clear_object (&async_context->session);
	g_clear_object (&async_context->composer);
	g_clear_object (&async_context->activity);

	g_free (async_context->folder_uri);
	g_free (async_context->message_uid);

	g_slice_free (AsyncContext, async_context);
}

static void
forward_data_free (ForwardData *data)
{
	if (data->shell != NULL)
		g_object_unref (data->shell);

	if (data->folder != NULL)
		g_object_unref (data->folder);

	if (data->uids != NULL)
		g_ptr_array_unref (data->uids);

	g_slice_free (ForwardData, data);
}

static gboolean
ask_confirm_for_unwanted_html_mail (EMsgComposer *composer,
                                    EDestination **recipients)
{
	gboolean res;
	GString *str;
	gint i;

	str = g_string_new ("");
	for (i = 0; recipients[i] != NULL; ++i) {
		if (!e_destination_get_html_mail_pref (recipients[i])) {
			const gchar *name;

			name = e_destination_get_textrep (recipients[i], FALSE);

			g_string_append_printf (str, "     %s\n", name);
		}
	}

	if (str->len)
		res = e_util_prompt_user (
			GTK_WINDOW (composer),
			"org.gnome.evolution.mail", "prompt-on-unwanted-html",
			"mail:ask-send-html", str->str, NULL);
	else
		res = TRUE;

	g_string_free (str, TRUE);

	return res;
}

static gboolean
ask_confirm_for_empty_subject (EMsgComposer *composer)
{
	return e_util_prompt_user (
		GTK_WINDOW (composer),
		"org.gnome.evolution.mail",
		"prompt-on-empty-subject",
		"mail:ask-send-no-subject", NULL);
}

static gboolean
ask_confirm_for_only_bcc (EMsgComposer *composer,
                          gboolean hidden_list_case)
{
	/* If the user is mailing a hidden contact list, it is possible for
	 * them to create a message with only Bcc recipients without really
	 * realizing it.  To try to avoid being totally confusing, I've changed
	 * this dialog to provide slightly different text in that case, to
	 * better explain what the hell is going on. */

	return e_util_prompt_user (
		GTK_WINDOW (composer),
		"org.gnome.evolution.mail",
		"prompt-on-only-bcc",
		hidden_list_case ?
		"mail:ask-send-only-bcc-contact" :
		"mail:ask-send-only-bcc", NULL);
}

static gboolean
is_group_definition (const gchar *str)
{
	const gchar *colon;

	if (!str || !*str)
		return FALSE;

	colon = strchr (str, ':');
	return colon > str && strchr (str, ';') > colon;
}

static gboolean
composer_presend_check_recipients (EMsgComposer *composer,
                                   EMailSession *session)
{
	EDestination **recipients;
	EDestination **recipients_bcc;
	CamelInternetAddress *cia;
	EComposerHeaderTable *table;
	EComposerHeader *post_to_header;
	GString *invalid_addrs = NULL;
	GSettings *settings;
	gboolean check_passed = FALSE;
	gint hidden = 0;
	gint shown = 0;
	gint num = 0;
	gint num_to_cc = 0;
	gint num_bcc = 0;
	gint num_post = 0;
	gint ii;

	/* We should do all of the validity checks based on the composer,
	 * and not on the created message, as extra interaction may occur
	 * when we get the message (e.g. passphrase to sign a message). */

	table = e_msg_composer_get_header_table (composer);

	recipients = e_composer_header_table_get_destinations_to (table);
	if (recipients) {
		for (ii = 0; recipients[ii] != NULL; ii++) {
			const gchar *addr;

			addr = e_destination_get_address (recipients[ii]);
			if (addr == NULL || *addr == '\0')
				continue;

			num_to_cc++;
		}

		e_destination_freev (recipients);
	}

	recipients = e_composer_header_table_get_destinations_cc (table);
	if (recipients) {
		for (ii = 0; recipients[ii] != NULL; ii++) {
			const gchar *addr;

			addr = e_destination_get_address (recipients[ii]);
			if (addr == NULL || *addr == '\0')
				continue;

			num_to_cc++;
		}

		e_destination_freev (recipients);
	}

	recipients = e_composer_header_table_get_destinations (table);

	cia = camel_internet_address_new ();

	/* See which ones are visible, present, etc. */
	for (ii = 0; recipients != NULL && recipients[ii] != NULL; ii++) {
		const gchar *addr;
		gint len, j;

		addr = e_destination_get_address (recipients[ii]);
		if (addr == NULL || *addr == '\0')
			continue;

		camel_address_decode (CAMEL_ADDRESS (cia), addr);
		len = camel_address_length (CAMEL_ADDRESS (cia));

		if (len > 0) {
			if (!e_destination_is_evolution_list (recipients[ii])) {
				for (j = 0; j < len; j++) {
					const gchar *name = NULL, *eml = NULL;

					if (!camel_internet_address_get (cia, j, &name, &eml) ||
					    !eml ||
					    strchr (eml, '@') <= eml) {
						if (!invalid_addrs)
							invalid_addrs = g_string_new ("");
						else
							g_string_append (invalid_addrs, ", ");

						if (name)
							g_string_append (invalid_addrs, name);
						if (eml) {
							g_string_append (invalid_addrs, name ? " <" : "");
							g_string_append (invalid_addrs, eml);
							g_string_append (invalid_addrs, name ? ">" : "");
						}
					}
				}
			}

			camel_address_remove (CAMEL_ADDRESS (cia), -1);
			num++;
			if (e_destination_is_evolution_list (recipients[ii])
			    && !e_destination_list_show_addresses (recipients[ii])) {
				hidden++;
			} else {
				shown++;
			}
		} else if (is_group_definition (addr)) {
			/* like an address, it will not claim on only-bcc */
			shown++;
			num++;
		} else if (!invalid_addrs) {
			invalid_addrs = g_string_new (addr);
		} else {
			g_string_append (invalid_addrs, ", ");
			g_string_append (invalid_addrs, addr);
		}
	}

	recipients_bcc = e_composer_header_table_get_destinations_bcc (table);
	if (recipients_bcc) {
		for (ii = 0; recipients_bcc[ii] != NULL; ii++) {
			const gchar *addr;

			addr = e_destination_get_address (recipients_bcc[ii]);
			if (addr == NULL || *addr == '\0')
				continue;

			camel_address_decode (CAMEL_ADDRESS (cia), addr);
			if (camel_address_length (CAMEL_ADDRESS (cia)) > 0) {
				camel_address_remove (CAMEL_ADDRESS (cia), -1);
				num_bcc++;
			}
		}

		e_destination_freev (recipients_bcc);
	}

	g_object_unref (cia);

	post_to_header = e_composer_header_table_get_header (
		table, E_COMPOSER_HEADER_POST_TO);
	if (e_composer_header_get_visible (post_to_header)) {
		GList *postlist;

		postlist = e_composer_header_table_get_post_to (table);
		num_post = g_list_length (postlist);
		g_list_foreach (postlist, (GFunc) g_free, NULL);
		g_list_free (postlist);
	}

	/* I'm sensing a lack of love, er, I mean recipients. */
	if (num == 0 && num_post == 0) {
		EHTMLEditor *editor;

		editor = e_msg_composer_get_editor (composer);
		e_alert_submit (E_ALERT_SINK (editor), "mail:send-no-recipients", NULL);

		goto finished;
	}

	if (invalid_addrs) {
		if (!e_util_prompt_user (
			GTK_WINDOW (composer),
			"org.gnome.evolution.mail",
			"prompt-on-invalid-recip",
			strstr (invalid_addrs->str, ", ") ?
				"mail:ask-send-invalid-recip-multi" :
				"mail:ask-send-invalid-recip-one",
			invalid_addrs->str, NULL)) {
			g_string_free (invalid_addrs, TRUE);
			goto finished;
		}

		g_string_free (invalid_addrs, TRUE);
	}

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	if (num_to_cc > 1 && num_to_cc >= g_settings_get_int (settings, "composer-many-to-cc-recips-num")) {
		gchar *head;
		gchar *msg;

		g_clear_object (&settings);

		head = g_strdup_printf (ngettext (
			/* Translators: The %d is replaced with the actual count of recipients, which is always more than one. */
			"Are you sure you want to send a message with %d To and CC recipients?",
			"Are you sure you want to send a message with %d To and CC recipients?",
			num_to_cc), num_to_cc);

		msg = g_strdup_printf (ngettext (
			/* Translators: The %d is replaced with the actual count of recipients, which is always more than one. */
			"You are trying to send a message to %d recipients in To and CC fields."
			" This would result in all recipients seeing the email addresses of each"
			" other. In some cases this behaviour is undesired, especially if they"
			" do not know each other or if privacy is a concern. Consider adding"
			" recipients to the BCC field instead.",
			"You are trying to send a message to %d recipients in To and CC fields."
			" This would result in all recipients seeing the email addresses of each"
			" other. In some cases this behaviour is undesired, especially if they"
			" do not know each other or if privacy is a concern. Consider adding"
			" recipients to the BCC field instead.",
			num_to_cc), num_to_cc);

		if (!e_util_prompt_user (
			GTK_WINDOW (composer),
			"org.gnome.evolution.mail",
			"prompt-on-many-to-cc-recips",
			"mail:ask-many-to-cc-recips",
			head, msg, NULL)) {
			GtkAction *action;

			g_free (head);
			g_free (msg);

			action = E_COMPOSER_ACTION_VIEW_BCC (composer);
			gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

			goto finished;
		}

		g_free (head);
		g_free (msg);
	}
	g_clear_object (&settings);

	if (num > 0 && (num == num_bcc || shown == 0)) {
		/* this means that the only recipients are Bcc's */
		if (!ask_confirm_for_only_bcc (composer, shown == 0))
			goto finished;
	}

	check_passed = TRUE;

finished:
	if (recipients != NULL)
		e_destination_freev (recipients);

	return check_passed;
}

static gboolean
composer_presend_check_identity (EMsgComposer *composer,
                                 EMailSession *session)
{
	EComposerHeaderTable *table;
	EClientCache *client_cache;
	ESourceRegistry *registry;
	ESource *source;
	const gchar *uid;
	gboolean success = TRUE;

	table = e_msg_composer_get_header_table (composer);

	uid = e_composer_header_table_get_identity_uid (table);
	source = e_composer_header_table_ref_source (table, uid);
	g_return_val_if_fail (source != NULL, FALSE);

	client_cache = e_composer_header_table_ref_client_cache (table);
	registry = e_client_cache_ref_registry (client_cache);

	if (!e_source_registry_check_enabled (registry, source)) {
		e_alert_submit (
			E_ALERT_SINK (composer),
			"mail:send-no-account-enabled", NULL);
		success = FALSE;
	}

	g_object_unref (client_cache);
	g_object_unref (registry);
	g_object_unref (source);

	return success;
}

static gboolean
composer_presend_check_plugins (EMsgComposer *composer,
                                EMailSession *session)
{
	EMEvent *eme;
	EMEventTargetComposer *target;
	gpointer data;

	/** @Event: composer.presendchecks
	 * @Title: Composer PreSend Checks
	 * @Target: EMEventTargetMessage
	 *
	 * composer.presendchecks is emitted during pre-checks for the
	 * message just before sending.  Since the e-plugin framework
	 * doesn't provide a way to return a value from the plugin,
	 * use 'presend_check_status' to set whether the check passed.
	 */
	eme = em_event_peek ();
	target = em_event_target_new_composer (eme, composer, 0);

	e_event_emit (
		(EEvent *) eme, "composer.presendchecks",
		(EEventTarget *) target);

	/* A non-NULL value for this key means the check failed. */
	data = g_object_get_data (G_OBJECT (composer), "presend_check_status");

	/* Clear the value in case we have to run these checks again. */
	g_object_set_data (G_OBJECT (composer), "presend_check_status", NULL);

	return (data == NULL);
}

static gboolean
composer_presend_check_subject (EMsgComposer *composer,
                                EMailSession *session)
{
	EComposerHeaderTable *table;
	const gchar *subject;
	gboolean check_passed = TRUE;

	table = e_msg_composer_get_header_table (composer);
	subject = e_composer_header_table_get_subject (table);

	if (subject == NULL || subject[0] == '\0') {
		if (!ask_confirm_for_empty_subject (composer))
			check_passed = FALSE;
	}

	return check_passed;
}

static gboolean
composer_presend_check_unwanted_html (EMsgComposer *composer,
                                      EMailSession *session)
{
	EDestination **recipients;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	EComposerHeaderTable *table;
	GSettings *settings;
	gboolean check_passed = TRUE;
	gboolean html_mode;
	gboolean send_html;
	gboolean confirm_html;
	gint ii;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);
	html_mode = e_content_editor_get_html_mode (cnt_editor);

	table = e_msg_composer_get_header_table (composer);
	recipients = e_composer_header_table_get_destinations (table);

	send_html = g_settings_get_boolean (settings, "composer-send-html");
	confirm_html = g_settings_get_boolean (settings, "prompt-on-unwanted-html");

	/* Only show this warning if our default is to send html.  If it
	 * isn't, we've manually switched into html mode in the composer
	 * and (presumably) had a good reason for doing this. */
	if (html_mode && send_html && confirm_html && recipients != NULL) {
		gboolean html_problem = FALSE;

		for (ii = 0; recipients[ii] != NULL; ii++) {
			if (!e_destination_get_html_mail_pref (recipients[ii])) {
				html_problem = TRUE;
				break;
			}
		}

		if (html_problem) {
			if (!ask_confirm_for_unwanted_html_mail (
				composer, recipients))
				check_passed = FALSE;
		}
	}

	if (recipients != NULL)
		e_destination_freev (recipients);

	g_object_unref (settings);

	return check_passed;
}

static void
composer_send_completed (GObject *source_object,
                         GAsyncResult *result,
                         gpointer user_data)
{
	EActivity *activity;
	gboolean service_unavailable;
	gboolean set_changed = FALSE;
	AsyncContext *async_context;
	GError *local_error = NULL;

	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;

	e_mail_session_send_to_finish (
		E_MAIL_SESSION (source_object), result, &local_error);

	if (e_activity_handle_cancellation (activity, local_error)) {
		set_changed = TRUE;
		goto exit;
	}

	/* Check for error codes which may indicate we're offline
	 * or name resolution failed or connection attempt failed. */
	service_unavailable =
		g_error_matches (
			local_error, CAMEL_SERVICE_ERROR,
			CAMEL_SERVICE_ERROR_UNAVAILABLE) ||
		/* name resolution failed */
		g_error_matches (
			local_error, G_RESOLVER_ERROR,
			G_RESOLVER_ERROR_NOT_FOUND) ||
		g_error_matches (
			local_error, G_RESOLVER_ERROR,
			G_RESOLVER_ERROR_TEMPORARY_FAILURE) ||
		/* something internal to Camel failed */
		g_error_matches (
			local_error, CAMEL_SERVICE_ERROR,
			CAMEL_SERVICE_ERROR_URL_INVALID);
	if (service_unavailable) {
		/* Inform the user. */
		e_alert_run_dialog_for_args (
			GTK_WINDOW (async_context->composer),
			"mail-composer:saving-to-outbox", NULL);
		e_msg_composer_save_to_outbox (async_context->composer);
		goto exit;
	}

	/* Post-processing errors are shown in the shell window. */
	if (g_error_matches (
		local_error, E_MAIL_ERROR,
		E_MAIL_ERROR_POST_PROCESSING)) {
		EAlert *alert;
		EShell *shell;

		shell = e_msg_composer_get_shell (async_context->composer);

		alert = e_alert_new (
			"mail-composer:send-post-processing-error",
			local_error->message, NULL);
		e_shell_submit_alert (shell, alert);
		g_object_unref (alert);

	/* All other errors are shown in the composer window. */
	} else if (local_error != NULL) {
		gint response;

		/* Clear the activity bar before
		 * presenting the error dialog. */
		g_clear_object (&async_context->activity);
		activity = async_context->activity;

		response = e_alert_run_dialog_for_args (
			GTK_WINDOW (async_context->composer),
			"mail-composer:send-error",
			local_error->message, NULL);
		if (response == GTK_RESPONSE_OK)  /* Try Again */
			e_msg_composer_send (async_context->composer);
		if (response == GTK_RESPONSE_ACCEPT)  /* Save to Outbox */
			e_msg_composer_save_to_outbox (async_context->composer);
		set_changed = TRUE;
		goto exit;
	}

	e_activity_set_state (activity, E_ACTIVITY_COMPLETED);

	/* Wait for the EActivity's completion message to
	 * time out and then destroy the composer window. */
	g_object_weak_ref (
		G_OBJECT (activity), (GWeakNotify)
		gtk_widget_destroy, async_context->composer);

exit:
	g_clear_error (&local_error);

	if (set_changed) {
		EHTMLEditor *editor;
		EContentEditor *cnt_editor;

		editor = e_msg_composer_get_editor (async_context->composer);
		cnt_editor = e_html_editor_get_content_editor (editor);

		e_content_editor_set_changed (cnt_editor, TRUE);

		gtk_window_present (GTK_WINDOW (async_context->composer));
	}

	async_context_free (async_context);
}

static void
em_utils_composer_real_send (EMsgComposer *composer,
			     CamelMimeMessage *message,
			     EActivity *activity,
			     EMailSession *session)
{
	AsyncContext *async_context;
	GCancellable *cancellable;
	GSettings *settings;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	if (g_settings_get_boolean (settings, "composer-use-outbox")) {
		e_msg_composer_save_to_outbox (composer);
		g_object_unref (settings);
		return;
	}

	g_object_unref (settings);

	if (!camel_session_get_online (CAMEL_SESSION (session))) {
		e_alert_run_dialog_for_args (
			GTK_WINDOW (composer),
			"mail-composer:saving-to-outbox", NULL);
		e_msg_composer_save_to_outbox (composer);
		return;
	}

	async_context = g_slice_new0 (AsyncContext);
	async_context->message = g_object_ref (message);
	async_context->composer = g_object_ref (composer);
	async_context->activity = g_object_ref (activity);

	cancellable = e_activity_get_cancellable (activity);

	e_mail_session_send_to (
		session, message,
		G_PRIORITY_DEFAULT,
		cancellable, NULL, NULL,
		composer_send_completed,
		async_context);
}

static void
composer_num_loading_notify_cb (EAttachmentStore *store,
				GParamSpec *param,
				AsyncContext *async_context)
{
	if (e_attachment_store_get_num_loading (store) > 0)
		return;

	em_utils_composer_real_send (
		async_context->composer,
		async_context->message,
		async_context->activity,
		async_context->session);

	async_context_free (async_context);
}

static void
composer_wait_for_attachment_load_cancelled_cb (GCancellable *cancellable,
						AsyncContext *async_context)
{
	async_context_free (async_context);
}

static void
em_utils_composer_send_cb (EMsgComposer *composer,
                           CamelMimeMessage *message,
                           EActivity *activity,
                           EMailSession *session)
{
	AsyncContext *async_context;
	GCancellable *cancellable;
	EAttachmentView *view;
	EAttachmentStore *store;

	view = e_msg_composer_get_attachment_view (composer);
	store = e_attachment_view_get_store (view);

	if (e_attachment_store_get_num_loading (store) <= 0) {
		em_utils_composer_real_send (composer, message, activity, session);
		return;
	}

	async_context = g_slice_new0 (AsyncContext);
	async_context->session = g_object_ref (session);
	async_context->message = g_object_ref (message);
	async_context->composer = g_object_ref (composer);
	async_context->activity = g_object_ref (activity);

	cancellable = e_activity_get_cancellable (activity);
	/* This message is never removed from the camel operation, otherwise the GtkInfoBar
	   hides itself and the user sees no feedback. */
	camel_operation_push_message (cancellable, "%s", _("Waiting for attachments to load..."));

	async_context->num_loading_handler_id = e_signal_connect_notify (store, "notify::num-loading",
		G_CALLBACK (composer_num_loading_notify_cb), async_context);
	/* Cannot use g_cancellable_connect() here, see async_context_free() */
	async_context->cancelled_handler_id = g_signal_connect (cancellable, "cancelled",
		G_CALLBACK (composer_wait_for_attachment_load_cancelled_cb), async_context);
}

static void
composer_set_no_change (EMsgComposer *composer)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	g_return_if_fail (composer != NULL);

	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_set_changed (cnt_editor, FALSE);
}

/* delete original messages from Outbox folder */
static void
manage_x_evolution_replace_outbox (EMsgComposer *composer,
                                   EMailSession *session,
                                   CamelMimeMessage *message,
                                   GCancellable *cancellable)
{
	const gchar *message_uid;
	const gchar *header;
	CamelFolder *outbox;

	g_return_if_fail (composer != NULL);
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	header = "X-Evolution-Replace-Outbox-UID";
	message_uid = camel_medium_get_header (CAMEL_MEDIUM (message), header);
	e_msg_composer_remove_header (composer, header);

	if (!message_uid)
		return;

	outbox = e_mail_session_get_local_folder (
		session, E_MAIL_LOCAL_FOLDER_OUTBOX);
	g_return_if_fail (outbox != NULL);

	camel_folder_set_message_flags (
		outbox, message_uid,
		CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_SEEN,
		CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_SEEN);

	/* ignore errors here */
	camel_folder_synchronize_message_sync (
		outbox, message_uid, cancellable, NULL);
}

static void
composer_save_to_drafts_complete (GObject *source_object,
                                  GAsyncResult *result,
                                  gpointer user_data)
{
	EActivity *activity;
	AsyncContext *async_context;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	GError *local_error = NULL;

	async_context = (AsyncContext *) user_data;

	editor = e_msg_composer_get_editor (async_context->composer);
	cnt_editor = e_html_editor_get_content_editor (editor);

	/* We don't really care if this failed.  If something other than
	 * cancellation happened, emit a runtime warning so the error is
	 * not completely lost. */
	e_mail_session_handle_draft_headers_finish (
		E_MAIL_SESSION (source_object), result, &local_error);

	activity = async_context->activity;

	if (e_activity_handle_cancellation (activity, local_error)) {
		e_content_editor_set_changed (cnt_editor, TRUE);
		g_error_free (local_error);

	} else if (local_error != NULL) {
		e_content_editor_set_changed (cnt_editor, TRUE);
		g_warning ("%s", local_error->message);
		g_error_free (local_error);
	} else
		e_activity_set_state (activity, E_ACTIVITY_COMPLETED);

	/* Encode the draft message we just saved into the EMsgComposer
	 * as X-Evolution-Draft headers.  The message will be marked for
	 * deletion if the user saves a newer draft message or sends the
	 * composed message. */
	e_msg_composer_set_draft_headers (
		async_context->composer,
		async_context->folder_uri,
		async_context->message_uid);

	async_context_free (async_context);
}

static void
composer_save_to_drafts_cleanup (GObject *source_object,
                                 GAsyncResult *result,
                                 gpointer user_data)
{
	CamelSession *session;
	EActivity *activity;
	EAlertSink *alert_sink;
	GCancellable *cancellable;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	AsyncContext *async_context;
	GError *local_error = NULL;

	async_context = (AsyncContext *) user_data;

	editor = e_msg_composer_get_editor (async_context->composer);
	cnt_editor = e_html_editor_get_content_editor (editor);

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);
	cancellable = e_activity_get_cancellable (activity);

	e_mail_folder_append_message_finish (
		CAMEL_FOLDER (source_object), result,
		&async_context->message_uid, &local_error);

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_warn_if_fail (async_context->message_uid == NULL);
		e_content_editor_set_changed (cnt_editor, TRUE);
		async_context_free (async_context);
		g_error_free (local_error);
		return;

	} else if (local_error != NULL) {
		g_warn_if_fail (async_context->message_uid == NULL);
		e_alert_submit (
			alert_sink,
			"mail-composer:save-to-drafts-error",
			local_error->message, NULL);
		e_content_editor_set_changed (cnt_editor, TRUE);
		async_context_free (async_context);
		g_error_free (local_error);
		return;
	}

	session = e_msg_composer_ref_session (async_context->composer);

	/* Mark the previously saved draft message for deletion.
	 * Note: This is just a nice-to-have; ignore failures. */
	e_mail_session_handle_draft_headers (
		E_MAIL_SESSION (session),
		async_context->message,
		G_PRIORITY_DEFAULT, cancellable,
		composer_save_to_drafts_complete,
		async_context);

	g_object_unref (session);
}

static void
composer_save_to_drafts_append_mail (AsyncContext *async_context,
                                     CamelFolder *drafts_folder)
{
	CamelFolder *local_drafts_folder;
	GCancellable *cancellable;
	CamelMessageInfo *info;

	local_drafts_folder =
		e_mail_session_get_local_folder (
		async_context->session, E_MAIL_LOCAL_FOLDER_DRAFTS);

	if (drafts_folder == NULL)
		drafts_folder = g_object_ref (local_drafts_folder);

	cancellable = e_activity_get_cancellable (async_context->activity);

	info = camel_message_info_new (NULL);

	camel_message_info_set_flags (info, CAMEL_MESSAGE_DRAFT | CAMEL_MESSAGE_SEEN |
		(camel_mime_message_has_attachment (async_context->message) ? CAMEL_MESSAGE_ATTACHMENTS : 0), ~0);

	camel_medium_remove_header (
		CAMEL_MEDIUM (async_context->message),
		"X-Evolution-Replace-Outbox-UID");

	e_mail_folder_append_message (
		drafts_folder, async_context->message,
		info, G_PRIORITY_DEFAULT, cancellable,
		composer_save_to_drafts_cleanup,
		async_context);

	camel_message_info_unref (info);

	g_object_unref (drafts_folder);
}

static void
composer_save_to_drafts_got_folder (GObject *source_object,
                                    GAsyncResult *result,
                                    gpointer user_data)
{
	EActivity *activity;
	CamelFolder *drafts_folder;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	AsyncContext *async_context;
	GError *local_error = NULL;

	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;

	editor = e_msg_composer_get_editor (async_context->composer);
	cnt_editor = e_html_editor_get_content_editor (editor);

	drafts_folder = e_mail_session_uri_to_folder_finish (
		E_MAIL_SESSION (source_object), result, &local_error);

	/* Sanity check. */
	g_return_if_fail (
		((drafts_folder != NULL) && (local_error == NULL)) ||
		((drafts_folder == NULL) && (local_error != NULL)));

	if (e_activity_handle_cancellation (activity, local_error)) {
		e_content_editor_set_changed (cnt_editor, TRUE);
		async_context_free (async_context);
		g_error_free (local_error);
		return;

	} else if (local_error != NULL) {
		gint response;

		/* XXX Not showing the error message in the dialog? */
		g_error_free (local_error);

		/* If we can't retrieve the Drafts folder for the
		 * selected account, ask the user if he wants to
		 * save to the local Drafts folder instead. */
		response = e_alert_run_dialog_for_args (
			GTK_WINDOW (async_context->composer),
			"mail:ask-default-drafts", NULL);
		if (response != GTK_RESPONSE_YES) {
			e_content_editor_set_changed (cnt_editor, TRUE);
			async_context_free (async_context);
			return;
		}
	}

	composer_save_to_drafts_append_mail (async_context, drafts_folder);
}

static void
em_utils_composer_save_to_drafts_cb (EMsgComposer *composer,
                                     CamelMimeMessage *message,
                                     EActivity *activity,
                                     EMailSession *session)
{
	AsyncContext *async_context;
	EComposerHeaderTable *table;
	ESource *source;
	const gchar *local_drafts_folder_uri;
	const gchar *identity_uid;
	gchar *drafts_folder_uri = NULL;

	async_context = g_slice_new0 (AsyncContext);
	async_context->message = g_object_ref (message);
	async_context->session = g_object_ref (session);
	async_context->composer = g_object_ref (composer);
	async_context->activity = g_object_ref (activity);

	table = e_msg_composer_get_header_table (composer);

	identity_uid = e_composer_header_table_get_identity_uid (table);
	source = e_composer_header_table_ref_source (table, identity_uid);

	/* Get the selected identity's preferred Drafts folder. */
	if (source != NULL) {
		ESourceMailComposition *extension;
		const gchar *extension_name;
		gchar *uri;

		extension_name = E_SOURCE_EXTENSION_MAIL_COMPOSITION;
		extension = e_source_get_extension (source, extension_name);
		uri = e_source_mail_composition_dup_drafts_folder (extension);

		drafts_folder_uri = uri;

		g_object_unref (source);
	}

	local_drafts_folder_uri =
		e_mail_session_get_local_folder_uri (
		session, E_MAIL_LOCAL_FOLDER_DRAFTS);

	if (drafts_folder_uri == NULL) {
		composer_save_to_drafts_append_mail (async_context, NULL);
		async_context->folder_uri = g_strdup (local_drafts_folder_uri);
	} else {
		GCancellable *cancellable;

		cancellable = e_activity_get_cancellable (activity);
		async_context->folder_uri = g_strdup (drafts_folder_uri);

		e_mail_session_uri_to_folder (
			session, drafts_folder_uri, 0,
			G_PRIORITY_DEFAULT, cancellable,
			composer_save_to_drafts_got_folder,
			async_context);

		g_free (drafts_folder_uri);
	}
}

static void
composer_save_to_outbox_completed (GObject *source_object,
                                   GAsyncResult *result,
                                   gpointer user_data)
{
	EMailSession *session;
	EActivity *activity;
	EAlertSink *alert_sink;
	GCancellable *cancellable;
	AsyncContext *async_context;
	GSettings *settings;
	GError *local_error = NULL;

	session = E_MAIL_SESSION (source_object);
	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);
	cancellable = e_activity_get_cancellable (activity);

	e_mail_session_append_to_local_folder_finish (
		session, result, NULL, &local_error);

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_error_free (local_error);
		goto exit;

	} else if (local_error != NULL) {
		e_alert_submit (
			alert_sink,
			"mail-composer:append-to-outbox-error",
			local_error->message, NULL);
		g_error_free (local_error);
		goto exit;
	}

	/* special processing for Outbox folder */
	manage_x_evolution_replace_outbox (
		async_context->composer,
		session,
		async_context->message,
		cancellable);

	e_activity_set_state (activity, E_ACTIVITY_COMPLETED);

	/* Wait for the EActivity's completion message to
	 * time out and then destroy the composer window. */
	g_object_weak_ref (
		G_OBJECT (activity), (GWeakNotify)
		gtk_widget_destroy, async_context->composer);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	if (g_settings_get_boolean (settings, "composer-use-outbox")) {
		gint delay_flush = g_settings_get_int (settings, "composer-delay-outbox-flush");

		if (delay_flush == 0) {
			e_mail_session_flush_outbox (session);
		} else if (delay_flush > 0) {
			e_mail_session_schedule_outbox_flush (session, delay_flush);
		}
	}
	g_object_unref (settings);

exit:
	async_context_free (async_context);
}

static void
em_utils_composer_save_to_outbox_cb (EMsgComposer *composer,
                                     CamelMimeMessage *message,
                                     EActivity *activity,
                                     EMailSession *session)
{
	AsyncContext *async_context;
	CamelMessageInfo *info;
	GCancellable *cancellable;

	async_context = g_slice_new0 (AsyncContext);
	async_context->message = g_object_ref (message);
	async_context->composer = g_object_ref (composer);
	async_context->activity = g_object_ref (activity);

	cancellable = e_activity_get_cancellable (activity);

	info = camel_message_info_new (NULL);
	camel_message_info_set_flags (info, CAMEL_MESSAGE_SEEN, ~0);

	e_mail_session_append_to_local_folder (
		session, E_MAIL_LOCAL_FOLDER_OUTBOX,
		message, info, G_PRIORITY_DEFAULT, cancellable,
		composer_save_to_outbox_completed,
		async_context);

	camel_message_info_unref (info);
}

typedef struct _PrintAsyncContext {
	GMainLoop *main_loop;
	GError *error;
} PrintAsyncContext;

static void
em_composer_utils_print_done_cb (GObject *source_object,
				 GAsyncResult *result,
				 gpointer user_data)
{
	PrintAsyncContext *async_context = user_data;

	g_return_if_fail (E_IS_MAIL_PRINTER (source_object));
	g_return_if_fail (async_context != NULL);
	g_return_if_fail (async_context->main_loop != NULL);

	e_mail_printer_print_finish (E_MAIL_PRINTER (source_object), result, &(async_context->error));

	g_main_loop_quit (async_context->main_loop);
}

static void
em_utils_composer_print_cb (EMsgComposer *composer,
                            GtkPrintOperationAction action,
                            CamelMimeMessage *message,
                            EActivity *activity,
                            EMailSession *session)
{
	EMailParser *parser;
	EMailPartList *parts, *reserved_parts;
	EMailPrinter *printer;
	EMailBackend *mail_backend;
	const gchar *message_id;
	GCancellable *cancellable;
	CamelObjectBag *parts_registry;
	gchar *mail_uri;
	PrintAsyncContext async_context;

	mail_backend = E_MAIL_BACKEND (e_shell_get_backend_by_name (e_msg_composer_get_shell (composer), "mail"));
	g_return_if_fail (mail_backend != NULL);

	cancellable = e_activity_get_cancellable (activity);
	parser = e_mail_parser_new (CAMEL_SESSION (session));

	message_id = camel_mime_message_get_message_id (message);
	parts = e_mail_parser_parse_sync (parser, NULL, message_id, message, cancellable);
	if (!parts) {
		g_clear_object (&parser);
		return;
	}

	parts_registry = e_mail_part_list_get_registry ();

	mail_uri = e_mail_part_build_uri (NULL, message_id, NULL, NULL);
	reserved_parts = camel_object_bag_reserve (parts_registry, mail_uri);
	g_clear_object (&reserved_parts);

	camel_object_bag_add (parts_registry, mail_uri, parts);

	printer = e_mail_printer_new (parts, e_mail_backend_get_remote_content (mail_backend));

	async_context.error = NULL;
	async_context.main_loop = g_main_loop_new (NULL, FALSE);

	/* Cannot use EAsyncClosure here, it blocks the main context, which is not good here. */
	e_mail_printer_print (printer, action, NULL, cancellable, em_composer_utils_print_done_cb, &async_context);

	g_main_loop_run (async_context.main_loop);

	camel_object_bag_remove (parts_registry, parts);
	g_main_loop_unref (async_context.main_loop);
	g_object_unref (printer);
	g_object_unref (parts);
	g_free (mail_uri);

	if (e_activity_handle_cancellation (activity, async_context.error)) {
		g_error_free (async_context.error);
	} else if (async_context.error != NULL) {
		e_alert_submit (
			e_activity_get_alert_sink (activity),
			"mail-composer:no-build-message",
			async_context.error->message, NULL);
		g_error_free (async_context.error);
	}
}

/* Composing messages... */

static void
set_up_new_composer (EMsgComposer *composer,
		     const gchar *subject,
		     CamelFolder *folder)
{
	EClientCache *client_cache;
	ESourceRegistry *registry;
	EComposerHeaderTable *table;
	ESource *source = NULL;
	gchar *identity = NULL;

	table = e_msg_composer_get_header_table (composer);

	client_cache = e_composer_header_table_ref_client_cache (table);
	registry = e_client_cache_ref_registry (client_cache);

	if (folder != NULL) {
		CamelStore *store;
		gchar *folder_uri;
		GList *list;

		store = camel_folder_get_parent_store (folder);
		source = em_utils_ref_mail_identity_for_store (registry, store);

		folder_uri = e_mail_folder_uri_from_folder (folder);

		list = g_list_prepend (NULL, folder_uri);
		e_composer_header_table_set_post_to_list (table, list);
		g_list_free (list);

		g_free (folder_uri);
	}

	if (source != NULL) {
		identity = e_source_dup_uid (source);
		g_object_unref (source);
	}

	e_composer_header_table_set_subject (table, subject);
	e_composer_header_table_set_identity_uid (table, identity);

	em_utils_apply_send_account_override_to_composer (composer, folder);

	g_free (identity);

	g_object_unref (client_cache);
	g_object_unref (registry);
}

/**
 * em_utils_compose_new_message:
 * @composer: an #EMsgComposer
 * @folder: (nullable): a #CamelFolder, or %NULL
 *
 * Sets up a new @composer window.
 *
 * Since: 3.22
 **/
void
em_utils_compose_new_message (EMsgComposer *composer,
                              CamelFolder *folder)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	if (folder != NULL)
		g_return_if_fail (CAMEL_IS_FOLDER (folder));

	set_up_new_composer (composer, "", folder);
	composer_set_no_change (composer);

	gtk_widget_show (GTK_WIDGET (composer));
}

static CamelMimeMessage *
em_utils_get_composer_recipients_as_message (EMsgComposer *composer)
{
	CamelMimeMessage *message;
	EComposerHeaderTable *table;
	EComposerHeader *header;
	EDestination **destv;
	CamelInternetAddress *to_addr, *cc_addr, *bcc_addr, *dest_addr;
	const gchar *text_addr;
	gint ii;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	table = e_msg_composer_get_header_table (composer);
	header = e_composer_header_table_get_header (table, E_COMPOSER_HEADER_TO);

	if (!e_composer_header_get_visible (header))
		return NULL;

	message = camel_mime_message_new ();

	to_addr = camel_internet_address_new ();
	cc_addr = camel_internet_address_new ();
	bcc_addr = camel_internet_address_new ();

	/* To */
	dest_addr = to_addr;
	destv = e_composer_header_table_get_destinations_to (table);
	for (ii = 0; destv != NULL && destv[ii] != NULL; ii++) {
		text_addr = e_destination_get_address (destv[ii]);
		if (text_addr && *text_addr) {
			if (camel_address_decode (CAMEL_ADDRESS (dest_addr), text_addr) <= 0)
				camel_internet_address_add (dest_addr, "", text_addr);
		}
	}
	e_destination_freev (destv);

	/* CC */
	dest_addr = cc_addr;
	destv = e_composer_header_table_get_destinations_cc (table);
	for (ii = 0; destv != NULL && destv[ii] != NULL; ii++) {
		text_addr = e_destination_get_address (destv[ii]);
		if (text_addr && *text_addr) {
			if (camel_address_decode (CAMEL_ADDRESS (dest_addr), text_addr) <= 0)
				camel_internet_address_add (dest_addr, "", text_addr);
		}
	}
	e_destination_freev (destv);

	/* Bcc */
	dest_addr = bcc_addr;
	destv = e_composer_header_table_get_destinations_bcc (table);
	for (ii = 0; destv != NULL && destv[ii] != NULL; ii++) {
		text_addr = e_destination_get_address (destv[ii]);
		if (text_addr && *text_addr) {
			if (camel_address_decode (CAMEL_ADDRESS (dest_addr), text_addr) <= 0)
				camel_internet_address_add (dest_addr, "", text_addr);
		}
	}
	e_destination_freev (destv);

	if (camel_address_length (CAMEL_ADDRESS (to_addr)) > 0)
		camel_mime_message_set_recipients (message, CAMEL_RECIPIENT_TYPE_TO, to_addr);

	if (camel_address_length (CAMEL_ADDRESS (cc_addr)) > 0)
		camel_mime_message_set_recipients (message, CAMEL_RECIPIENT_TYPE_CC, cc_addr);

	if (camel_address_length (CAMEL_ADDRESS (bcc_addr)) > 0)
		camel_mime_message_set_recipients (message, CAMEL_RECIPIENT_TYPE_BCC, bcc_addr);

	g_object_unref (to_addr);
	g_object_unref (cc_addr);
	g_object_unref (bcc_addr);

	return message;
}

typedef struct _CreateComposerData {
	gchar *mailto;
	CamelFolder *folder;
} CreateComposerData;

static void
create_composer_data_free (gpointer ptr)
{
	CreateComposerData *ccd = ptr;

	if (ccd) {
		g_clear_object (&ccd->folder);
		g_free (ccd->mailto);
		g_free (ccd);
	}
}

static void
msg_composer_created_with_mailto_cb (GObject *source_object,
				     GAsyncResult *result,
				     gpointer user_data)
{
	CreateComposerData *ccd = user_data;
	EMsgComposer *composer;
	EComposerHeaderTable *table;
	EClientCache *client_cache;
	ESourceRegistry *registry;
	GError *error = NULL;

	g_return_if_fail (ccd != NULL);

	composer = e_msg_composer_new_finish (result, &error);
	if (error) {
		g_warning ("%s: Failed to create msg composer: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
		create_composer_data_free (ccd);

		return;
	}

	if (ccd->mailto)
		e_msg_composer_setup_from_url (composer, ccd->mailto);

	em_utils_apply_send_account_override_to_composer (composer, ccd->folder);

	table = e_msg_composer_get_header_table (composer);

	client_cache = e_composer_header_table_ref_client_cache (table);
	registry = e_client_cache_ref_registry (client_cache);

	composer_set_no_change (composer);

	/* If a CamelFolder was given, we need to backtrack and find
	 * the corresponding ESource with a Mail Identity extension. */

	if (ccd->folder) {
		ESource *source;
		CamelStore *store;

		store = camel_folder_get_parent_store (ccd->folder);
		source = em_utils_ref_mail_identity_for_store (registry, store);

		if (source != NULL) {
			const gchar *uid = e_source_get_uid (source);
			e_composer_header_table_set_identity_uid (table, uid);
			g_object_unref (source);
		}
	}

	g_object_unref (client_cache);
	g_object_unref (registry);

	gtk_window_present (GTK_WINDOW (composer));

	create_composer_data_free (ccd);
}

/**
 * em_utils_compose_new_message_with_mailto:
 * @shell: an #EShell
 * @mailto: a mailto URL
 * @folder: a #CamelFolder, or %NULL
 *
 * Opens a new composer window as a child window of @parent's toplevel
 * window. If @mailto is non-NULL, the composer fields will be filled in
 * according to the values in the mailto URL.
 **/
void
em_utils_compose_new_message_with_mailto (EShell *shell,
                                          const gchar *mailto,
                                          CamelFolder *folder)
{
	CreateComposerData *ccd;

	g_return_if_fail (E_IS_SHELL (shell));

	if (folder != NULL)
		g_return_if_fail (CAMEL_IS_FOLDER (folder));

	ccd = g_new0 (CreateComposerData, 1);
	ccd->folder = folder ? g_object_ref (folder) : NULL;
	ccd->mailto = g_strdup (mailto);

	e_msg_composer_new (shell, msg_composer_created_with_mailto_cb, ccd);
}

static gboolean
replace_variables (GSList *clues,
                   CamelMimeMessage *message,
                   gchar **pstr)
{
	gint i;
	gboolean string_changed = FALSE, count1 = FALSE;
	gchar *str;

	g_return_val_if_fail (pstr != NULL, FALSE);
	g_return_val_if_fail (*pstr != NULL, FALSE);
	g_return_val_if_fail (message != NULL, FALSE);

	str = *pstr;

	for (i = 0; i < strlen (str); i++) {
		const gchar *cur = str + i;
		if (!g_ascii_strncasecmp (cur, "$", 1)) {
			const gchar *end = cur + 1;
			gchar *out;
			gchar **temp_str;
			GSList *list;

			while (*end && (g_unichar_isalnum (*end) || *end == '_'))
				end++;

			out = g_strndup ((const gchar *) cur, end - cur);

			temp_str = g_strsplit (str, out, 2);

			for (list = clues; list; list = g_slist_next (list)) {
				gchar **temp = g_strsplit (list->data, "=", 2);
				if (!g_ascii_strcasecmp (temp[0], out + 1)) {
					g_free (str);
					str = g_strconcat (temp_str[0], temp[1], temp_str[1], NULL);
					count1 = TRUE;
					string_changed = TRUE;
				} else
					count1 = FALSE;
				g_strfreev (temp);
			}

			if (!count1) {
				if (getenv (out + 1)) {
					g_free (str);
					str = g_strconcat (
						temp_str[0],
						getenv (out + 1),
						temp_str[1], NULL);
					count1 = TRUE;
					string_changed = TRUE;
				} else
					count1 = FALSE;
			}

			if (!count1) {
				CamelInternetAddress *to;
				const gchar *name, *addr;

				to = camel_mime_message_get_recipients (
					message, CAMEL_RECIPIENT_TYPE_TO);
				if (!camel_internet_address_get (to, 0, &name, &addr))
					continue;

				if (name && g_ascii_strcasecmp ("sender_name", out + 1) == 0) {
					g_free (str);
					str = g_strconcat (temp_str[0], name, temp_str[1], NULL);
					count1 = TRUE;
					string_changed = TRUE;
				} else if (addr && g_ascii_strcasecmp ("sender_email", out + 1) == 0) {
					g_free (str);
					str = g_strconcat (temp_str[0], addr, temp_str[1], NULL);
					count1 = TRUE;
					string_changed = TRUE;
				}
			}

			g_strfreev (temp_str);
			g_free (out);
		}
	}

	*pstr = str;

	return string_changed;
}

static void
traverse_parts (GSList *clues,
                CamelMimeMessage *message,
                CamelDataWrapper *content)
{
	g_return_if_fail (message != NULL);

	if (!content)
		return;

	if (CAMEL_IS_MULTIPART (content)) {
		guint i, n;
		CamelMultipart *multipart = CAMEL_MULTIPART (content);
		CamelMimePart *part;

		n = camel_multipart_get_number (multipart);
		for (i = 0; i < n; i++) {
			part = camel_multipart_get_part (multipart, i);
			if (!part)
				continue;

			traverse_parts (clues, message, CAMEL_DATA_WRAPPER (part));
		}
	} else if (CAMEL_IS_MIME_PART (content)) {
		CamelMimePart *part = CAMEL_MIME_PART (content);
		CamelContentType *type;
		CamelStream *stream;
		GByteArray *byte_array;
		gchar *str;

		content = camel_medium_get_content (CAMEL_MEDIUM (part));
		if (!content)
			return;

		if (CAMEL_IS_MULTIPART (content)) {
			traverse_parts (clues, message, CAMEL_DATA_WRAPPER (content));
			return;
		}

		type = camel_mime_part_get_content_type (part);
		if (!camel_content_type_is (type, "text", "*"))
			return;

		byte_array = g_byte_array_new ();
		stream = camel_stream_mem_new_with_byte_array (byte_array);
		camel_data_wrapper_decode_to_stream_sync (
			content, stream, NULL, NULL);

		str = g_strndup ((gchar *) byte_array->data, byte_array->len);
		g_object_unref (stream);

		if (replace_variables (clues, message, &str)) {
			stream = camel_stream_mem_new_with_buffer (str, strlen (str));
			camel_data_wrapper_construct_from_stream_sync (
				content, stream, NULL, NULL);
			g_object_unref (stream);
		}

		g_free (str);
	}
}

/* Editing messages... */

typedef enum {
	QUOTING_ATTRIBUTION,
	QUOTING_FORWARD,
	QUOTING_ORIGINAL
} QuotingTextEnum;

static struct {
	const gchar * conf_key;
	const gchar * message;
} conf_messages[] = {
	[QUOTING_ATTRIBUTION] =
		{ "composer-message-attribution",
		/* Note to translators: this is the attribution string used
		 * when quoting messages.  Each ${Variable} gets replaced
		 * with a value.  To see a full list of available variables,
		 * see mail/em-composer-utils.c:attribvars array. */
		  N_("On ${AbbrevWeekdayName}, ${Year}-${Month}-${Day} at "
		     "${24Hour}:${Minute} ${TimeZone}, ${Sender} wrote:")
		},

	[QUOTING_FORWARD] =
		{ "composer-message-forward",
		  N_("-------- Forwarded Message --------")
		},

	[QUOTING_ORIGINAL] =
		{ "composer-message-original",
		  N_("-----Original Message-----")
		}
};

static gchar *
quoting_text (QuotingTextEnum type)
{
	GSettings *settings;
	gchar *text;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	text = g_settings_get_string (settings, conf_messages[type].conf_key);
	g_object_unref (settings);

	if (text && *text)
		return text;

	g_free (text);

	return g_strdup (_(conf_messages[type].message));
}

/**
 * em_utils_edit_message:
 * @composer: an #EMsgComposer
 * @folder: a #CamelFolder
 * @message: a #CamelMimeMessage
 * @message_uid: UID of @message, or %NULL
 *
 * Sets up the @composer with the headers/mime-parts/etc of the @message.
 *
 * Since: 3.22
 **/
void
em_utils_edit_message (EMsgComposer *composer,
                       CamelFolder *folder,
                       CamelMimeMessage *message,
                       const gchar *message_uid,
                       gboolean keep_signature)
{
	ESourceRegistry *registry;
	ESource *source;
	gboolean folder_is_sent;
	gboolean folder_is_drafts;
	gboolean folder_is_outbox;
	gboolean folder_is_templates;
	gchar *override_identity_uid = NULL;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));
	if (folder)
		g_return_if_fail (CAMEL_IS_FOLDER (folder));

	registry = e_shell_get_registry (e_msg_composer_get_shell (composer));

	if (folder) {
		folder_is_sent = em_utils_folder_is_sent (registry, folder);
		folder_is_drafts = em_utils_folder_is_drafts (registry, folder);
		folder_is_outbox = em_utils_folder_is_outbox (registry, folder);
		folder_is_templates = em_utils_folder_is_templates (registry, folder);
	} else {
		folder_is_sent = FALSE;
		folder_is_drafts = FALSE;
		folder_is_outbox = FALSE;
		folder_is_templates = FALSE;
	}

	/* Template specific code follows. */
	if (folder_is_templates) {
		CamelDataWrapper *content;
		GSettings *settings;
		gchar **strv;
		gint i;
		GSList *clue_list = NULL;

		settings = e_util_ref_settings ("org.gnome.evolution.plugin.templates");

		/* Get the list from GSettings */
		strv = g_settings_get_strv (settings, "template-placeholders");
		for (i = 0; strv[i] != NULL; i++)
			clue_list = g_slist_append (clue_list, g_strdup (strv[i]));
		g_object_unref (settings);
		g_strfreev (strv);

		content = camel_medium_get_content (CAMEL_MEDIUM (message));
		traverse_parts (clue_list, message, content);

		g_slist_foreach (clue_list, (GFunc) g_free, NULL);
		g_slist_free (clue_list);
	}

	if (folder) {
		if (!folder_is_sent && !folder_is_drafts && !folder_is_outbox && !folder_is_templates) {
			CamelStore *store;

			store = camel_folder_get_parent_store (folder);
			source = em_utils_ref_mail_identity_for_store (registry, store);

			if (source) {
				g_free (override_identity_uid);
				override_identity_uid = e_source_dup_uid (source);
				g_object_unref (source);
			}
		}

		source = em_utils_check_send_account_override (e_msg_composer_get_shell (composer), message, folder);
		if (source) {
			g_free (override_identity_uid);
			override_identity_uid = e_source_dup_uid (source);
			g_object_unref (source);
		}
	}

	e_msg_composer_setup_with_message (composer, message, keep_signature, override_identity_uid, NULL);

	g_free (override_identity_uid);

	/* Override PostTo header only if the folder is a regular folder */
	if (folder && !folder_is_sent && !folder_is_drafts && !folder_is_outbox && !folder_is_templates) {
		EComposerHeaderTable *table;
		gchar *folder_uri;
		GList *list;

		table = e_msg_composer_get_header_table (composer);

		folder_uri = e_mail_folder_uri_from_folder (folder);

		list = g_list_prepend (NULL, folder_uri);
		e_composer_header_table_set_post_to_list (table, list);
		g_list_free (list);

		g_free (folder_uri);
	}

	e_msg_composer_remove_header (
		composer, "X-Evolution-Replace-Outbox-UID");

	if (message_uid != NULL && folder_is_drafts && folder) {
		gchar *folder_uri;

		folder_uri = e_mail_folder_uri_from_folder (folder);

		e_msg_composer_set_draft_headers (
			composer, folder_uri, message_uid);

		g_free (folder_uri);

	} else if (message_uid != NULL && folder_is_outbox) {
		e_msg_composer_set_header (
			composer, "X-Evolution-Replace-Outbox-UID",
			message_uid);
	}

	composer_set_no_change (composer);

	gtk_widget_show (GTK_WIDGET (composer));
}

static void
emu_update_composers_security (EMsgComposer *composer,
                               guint32 validity_found)
{
	GtkAction *action;
	GSettings *settings;
	gboolean sign_by_default;

	g_return_if_fail (composer != NULL);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	sign_by_default =
		(validity_found & E_MAIL_PART_VALIDITY_SIGNED) != 0 &&
		/* FIXME This should be an EMsgComposer property. */
		g_settings_get_boolean (
			settings, "composer-sign-reply-if-signed");

	g_object_unref (settings);

	/* Pre-set only for encrypted messages, not for signed */
	if (sign_by_default) {
		action = NULL;

		if (validity_found & E_MAIL_PART_VALIDITY_SMIME) {
			if (!gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (E_COMPOSER_ACTION_PGP_SIGN (composer))) &&
			    !gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (E_COMPOSER_ACTION_PGP_ENCRYPT (composer))))
				action = E_COMPOSER_ACTION_SMIME_SIGN (composer);
		} else {
			if (!gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (E_COMPOSER_ACTION_SMIME_SIGN (composer))) &&
			    !gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (E_COMPOSER_ACTION_SMIME_ENCRYPT (composer))))
				action = E_COMPOSER_ACTION_PGP_SIGN (composer);
		}

		if (action)
			gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
	}

	if (validity_found & E_MAIL_PART_VALIDITY_ENCRYPTED) {
		action = NULL;

		if (validity_found & E_MAIL_PART_VALIDITY_SMIME) {
			if (!gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (E_COMPOSER_ACTION_PGP_SIGN (composer))) &&
			    !gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (E_COMPOSER_ACTION_PGP_ENCRYPT (composer))))
				action = E_COMPOSER_ACTION_SMIME_ENCRYPT (composer);
		} else {
			if (!gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (E_COMPOSER_ACTION_SMIME_SIGN (composer))) &&
			    !gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (E_COMPOSER_ACTION_SMIME_ENCRYPT (composer))))
				action = E_COMPOSER_ACTION_PGP_ENCRYPT (composer);
		}

		if (action)
			gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
	}
}

static void
em_utils_get_real_folder_and_message_uid (CamelFolder *folder,
					  const gchar *uid,
					  CamelFolder **out_real_folder,
					  gchar **folder_uri,
					  gchar **message_uid)
{
	g_return_if_fail (folder != NULL);
	g_return_if_fail (uid != NULL);

	if (out_real_folder)
		*out_real_folder = NULL;

	if (CAMEL_IS_VEE_FOLDER (folder)) {
		CamelMessageInfo *mi;

		mi = camel_folder_get_message_info (folder, uid);
		if (mi) {
			CamelFolder *real_folder;
			gchar *real_uid = NULL;

			real_folder = camel_vee_folder_get_location (
				CAMEL_VEE_FOLDER (folder),
				(CamelVeeMessageInfo *) mi,
				&real_uid);

			if (real_folder) {
				if (folder_uri)
					*folder_uri = e_mail_folder_uri_from_folder (real_folder);
				if (message_uid)
					*message_uid = real_uid;
				else
					g_free (real_uid);

				camel_message_info_unref (mi);

				if (out_real_folder)
					*out_real_folder = g_object_ref (real_folder);

				return;
			}

			camel_message_info_unref (mi);
		}
	}

	if (folder_uri)
		*folder_uri = e_mail_folder_uri_from_folder (folder);
	if (message_uid)
		*message_uid = g_strdup (uid);
}

void
em_utils_get_real_folder_uri_and_message_uid (CamelFolder *folder,
                                              const gchar *uid,
                                              gchar **folder_uri,
                                              gchar **message_uid)
{
	g_return_if_fail (folder != NULL);
	g_return_if_fail (uid != NULL);
	g_return_if_fail (folder_uri != NULL);
	g_return_if_fail (message_uid != NULL);

	em_utils_get_real_folder_and_message_uid (folder, uid, NULL, folder_uri, message_uid);
}

static void
emu_add_composer_references_from_message (EMsgComposer *composer,
					  CamelMimeMessage *message)
{
	const gchar *message_id_header;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	message_id_header = camel_mime_message_get_message_id (message);
	if (message_id_header && *message_id_header) {
		GString *references = g_string_new ("");
		gint ii = 0;
		const gchar *value;
		gchar *unfolded;

		while (value = e_msg_composer_get_header (composer, "References", ii), value) {
			ii++;

			if (references->len)
				g_string_append_c (references, ' ');
			g_string_append (references, value);
		}

		if (references->len)
			g_string_append_c (references, ' ');

		if (*message_id_header != '<')
			g_string_append_c (references, '<');

		g_string_append (references, message_id_header);

		if (*message_id_header != '<')
			g_string_append_c (references, '>');

		unfolded = camel_header_unfold (references->str);

		e_msg_composer_set_header (composer, "References", unfolded);

		g_string_free (references, TRUE);
		g_free (unfolded);
	}
}

static void
real_update_forwarded_flag (gpointer uid,
                            gpointer folder)
{
	if (uid && folder)
		camel_folder_set_message_flags (
			folder, uid, CAMEL_MESSAGE_FORWARDED,
			CAMEL_MESSAGE_FORWARDED);
}

static void
update_forwarded_flags_cb (EMsgComposer *composer,
                           ForwardData *data)
{
	if (data && data->uids && data->folder)
		g_ptr_array_foreach (
			data->uids, real_update_forwarded_flag, data->folder);
}

static void
setup_forward_attached_callbacks (EMsgComposer *composer,
                                  CamelFolder *folder,
                                  GPtrArray *uids)
{
	ForwardData *data;

	if (!composer || !folder || !uids || !uids->len)
		return;

	g_object_ref (folder);

	data = g_slice_new0 (ForwardData);
	data->folder = g_object_ref (folder);
	data->uids = g_ptr_array_ref (uids);

	g_signal_connect (
		composer, "send",
		G_CALLBACK (update_forwarded_flags_cb), data);
	g_signal_connect (
		composer, "save-to-drafts",
		G_CALLBACK (update_forwarded_flags_cb), data);

	g_object_set_data_full (
		G_OBJECT (composer), "forward-data", data,
		(GDestroyNotify) forward_data_free);
}

static void
forward_non_attached (EMsgComposer *composer,
                      CamelFolder *folder,
                      const gchar *uid,
                      CamelMimeMessage *message,
                      EMailForwardStyle style)
{
	CamelSession *session;
	gchar *text, *forward;
	guint32 validity_found = 0;
	guint32 flags;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	session = e_msg_composer_ref_session (composer);

	flags = E_MAIL_FORMATTER_QUOTE_FLAG_HEADERS |
		E_MAIL_FORMATTER_QUOTE_FLAG_KEEP_SIG;
	if (style == E_MAIL_FORWARD_STYLE_QUOTED)
		flags |= E_MAIL_FORMATTER_QUOTE_FLAG_CITE;

	forward = quoting_text (QUOTING_FORWARD);
	text = em_utils_message_to_html (session, message, forward, flags, NULL, NULL, NULL, &validity_found);

	if (text != NULL) {
		CamelDataWrapper *content;
		gchar *subject;

		subject = mail_tool_generate_forward_subject (message);
		set_up_new_composer (composer, subject, folder);
		g_free (subject);

		content = camel_medium_get_content (CAMEL_MEDIUM (message));

		if (CAMEL_IS_MULTIPART (content))
			e_msg_composer_add_message_attachments (
				composer, message, FALSE);

		e_msg_composer_set_body_text (composer, text, TRUE);

		emu_add_composer_references_from_message (composer, message);

		if (uid != NULL) {
			gchar *folder_uri = NULL, *tmp_message_uid = NULL;

			em_utils_get_real_folder_uri_and_message_uid (
				folder, uid, &folder_uri, &tmp_message_uid);

			e_msg_composer_set_source_headers (
				composer, folder_uri, tmp_message_uid,
				CAMEL_MESSAGE_FORWARDED);

			g_free (folder_uri);
			g_free (tmp_message_uid);
		}

		emu_update_composers_security (composer, validity_found);
		composer_set_no_change (composer);
		gtk_widget_show (GTK_WIDGET (composer));

		g_free (text);
	}

	g_clear_object (&session);
	g_free (forward);
}

/**
 * em_utils_forward_message:
 * @composer: an #EMsgComposer
 * @message: a #CamelMimeMessage to forward
 * @style: the forward style to use
 * @folder: (nullable):  a #CamelFolder, or %NULL
 * @uid: (nullable): the UID of %message, or %NULL
 *
 * Forwards @message in the given @style.
 *
 * If @style is #E_MAIL_FORWARD_STYLE_ATTACHED, the new message is
 * created as follows.  If there is more than a single message in @uids,
 * a multipart/digest will be constructed and attached to a new composer
 * window preset with the appropriate header defaults for forwarding the
 * first message in the list.  If only one message is to be forwarded,
 * it is forwarded as a simple message/rfc822 attachment.
 *
 * If @style is #E_MAIL_FORWARD_STYLE_INLINE, each message is forwarded
 * in its own composer window in 'inline' form.
 *
 * If @style is #E_MAIL_FORWARD_STYLE_QUOTED, each message is forwarded
 * in its own composer window in 'quoted' form (each line starting with
 * a "> ").
 **/
void
em_utils_forward_message (EMsgComposer *composer,
                          CamelMimeMessage *message,
                          EMailForwardStyle style,
                          CamelFolder *folder,
                          const gchar *uid)
{
	CamelMimePart *part;
	gchar *subject;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	switch (style) {
		case E_MAIL_FORWARD_STYLE_ATTACHED:
		default:
			part = mail_tool_make_message_attachment (message);
			subject = mail_tool_generate_forward_subject (message);

			em_utils_forward_attachment (composer, part, subject, NULL, NULL);

			g_object_unref (part);
			g_free (subject);
			break;

		case E_MAIL_FORWARD_STYLE_INLINE:
		case E_MAIL_FORWARD_STYLE_QUOTED:
			forward_non_attached (composer, folder, uid, message, style);
			break;
	}
}

void
em_utils_forward_attachment (EMsgComposer *composer,
                             CamelMimePart *part,
                             const gchar *subject,
                             CamelFolder *folder,
                             GPtrArray *uids)
{
	CamelDataWrapper *content;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (CAMEL_IS_MIME_PART (part));

	if (folder != NULL)
		g_return_if_fail (CAMEL_IS_FOLDER (folder));

	set_up_new_composer (composer, subject, folder);

	e_msg_composer_attach (composer, part);

	content = camel_medium_get_content (CAMEL_MEDIUM (part));
	if (CAMEL_IS_MIME_MESSAGE (content)) {
		emu_add_composer_references_from_message (composer, CAMEL_MIME_MESSAGE (content));
	} else if (CAMEL_IS_MULTIPART (content)) {
		const gchar *mime_type;

		mime_type = camel_data_wrapper_get_mime_type (content);
		if (mime_type && g_ascii_strcasecmp (mime_type, "multipart/digest") == 0) {
			/* This is the way evolution forwards multiple messages as attachment */
			CamelMultipart *multipart;
			guint ii, nparts;

			multipart = CAMEL_MULTIPART (content);
			nparts = camel_multipart_get_number (multipart);

			for (ii = 0; ii < nparts; ii++) {
				CamelMimePart *mpart;

				mpart = camel_multipart_get_part (multipart, ii);
				mime_type = camel_data_wrapper_get_mime_type (CAMEL_DATA_WRAPPER (mpart));

				if (mime_type && g_ascii_strcasecmp (mime_type, "message/rfc822") == 0) {
					content = camel_medium_get_content (CAMEL_MEDIUM (mpart));

					if (CAMEL_IS_MIME_MESSAGE (content))
						emu_add_composer_references_from_message (composer, CAMEL_MIME_MESSAGE (content));
				}
			}
		}
	}

	if (uids != NULL)
		setup_forward_attached_callbacks (composer, folder, uids);

	composer_set_no_change (composer);

	gtk_widget_show (GTK_WIDGET (composer));
}

static gint
compare_sources_with_uids_order_cb (gconstpointer a,
                                    gconstpointer b,
                                    gpointer user_data)
{
	ESource *asource = (ESource *) a;
	ESource *bsource = (ESource *) b;
	GHashTable *uids_order = user_data;
	gint aindex, bindex;

	aindex = GPOINTER_TO_INT (g_hash_table_lookup (uids_order, e_source_get_uid (asource)));
	bindex = GPOINTER_TO_INT (g_hash_table_lookup (uids_order, e_source_get_uid (bsource)));

	if (aindex <= 0)
		aindex = g_hash_table_size (uids_order);
	if (bindex <= 0)
		bindex = g_hash_table_size (uids_order);

	return aindex - bindex;
}

static void
sort_sources_by_ui (GList **psources,
                    gpointer user_data)
{
	EShell *shell = user_data;
	EShellBackend *shell_backend;
	EMailSession *mail_session;
	EMailAccountStore *account_store;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GHashTable *uids_order;
	gint index = 0;

	g_return_if_fail (psources != NULL);
	g_return_if_fail (E_IS_SHELL (shell));

	/* nothing to sort */
	if (!*psources || !g_list_next (*psources))
		return;

	shell_backend = e_shell_get_backend_by_name (shell, "mail");
	g_return_if_fail (shell_backend != NULL);

	mail_session = e_mail_backend_get_session (E_MAIL_BACKEND (shell_backend));
	g_return_if_fail (mail_session != NULL);

	account_store = e_mail_ui_session_get_account_store (E_MAIL_UI_SESSION (mail_session));
	g_return_if_fail (account_store != NULL);

	model = GTK_TREE_MODEL (account_store);
	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	uids_order = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	do {
		CamelService *service = NULL;

		gtk_tree_model_get (model, &iter, E_MAIL_ACCOUNT_STORE_COLUMN_SERVICE, &service, -1);

		if (service) {
			index++;
			g_hash_table_insert (uids_order, g_strdup (camel_service_get_uid (service)), GINT_TO_POINTER (index));
			g_object_unref (service);
		}
	} while (gtk_tree_model_iter_next (model, &iter));

	*psources = g_list_sort_with_data (*psources, compare_sources_with_uids_order_cb, uids_order);

	g_hash_table_destroy (uids_order);
}

/**
 * em_utils_redirect_message:
 * @composer: an #EMsgComposer
 * @message: message to redirect
 *
 * Sets up the @composer to redirect @message (Note: only headers will be
 * editable). Adds Resent-From/Resent-To/etc headers.
 *
 * Since: 3.22
 **/
void
em_utils_redirect_message (EMsgComposer *composer,
                           CamelMimeMessage *message)
{
	ESourceRegistry *registry;
	ESource *source;
	EShell *shell;
	CamelMedium *medium;
	gchar *identity_uid = NULL;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	shell = e_msg_composer_get_shell (composer);
	medium = CAMEL_MEDIUM (message);

	/* QMail will refuse to send a message if it finds one of
	 * it's Delivered-To headers in the message, so remove all
	 * Delivered-To headers. Fixes bug #23635. */
	while (camel_medium_get_header (medium, "Delivered-To"))
		camel_medium_remove_header (medium, "Delivered-To");

	while (camel_medium_get_header (medium, "Bcc"))
		camel_medium_remove_header (medium, "Bcc");

	while (camel_medium_get_header (medium, "Resent-Bcc"))
		camel_medium_remove_header (medium, "Resent-Bcc");

	registry = e_shell_get_registry (shell);

	/* This returns a new ESource reference. */
	source = em_utils_check_send_account_override (shell, message, NULL);
	if (!source)
		source = em_utils_guess_mail_identity_with_recipients_and_sort (
			registry, message, NULL, NULL, sort_sources_by_ui, shell);

	if (source != NULL) {
		identity_uid = e_source_dup_uid (source);
		g_object_unref (source);
	}

	e_msg_composer_setup_redirect (composer, message, identity_uid, NULL);

	g_free (identity_uid);

	gtk_widget_show (GTK_WIDGET (composer));

	composer_set_no_change (composer);
}

/* Replying to messages... */

EDestination **
em_utils_camel_address_to_destination (CamelInternetAddress *iaddr)
{
	EDestination *dest, **destv;
	gint n, i, j;

	if (iaddr == NULL)
		return NULL;

	if ((n = camel_address_length ((CamelAddress *) iaddr)) == 0)
		return NULL;

	destv = g_malloc (sizeof (EDestination *) * (n + 1));
	for (i = 0, j = 0; i < n; i++) {
		const gchar *name, *addr;

		if (camel_internet_address_get (iaddr, i, &name, &addr)) {
			dest = e_destination_new ();
			e_destination_set_name (dest, name);
			e_destination_set_email (dest, addr);

			destv[j++] = dest;
		}
	}

	if (j == 0) {
		g_free (destv);
		return NULL;
	}

	destv[j] = NULL;

	return destv;
}

static void
reply_setup_composer (EMsgComposer *composer,
		      CamelMimeMessage *message,
		      const gchar *identity_uid,
		      CamelInternetAddress *to,
		      CamelInternetAddress *cc,
		      CamelFolder *folder,
		      const gchar *message_uid,
		     CamelNNTPAddress *postto)
{
	gchar *message_id, *references;
	EDestination **tov, **ccv;
	EComposerHeaderTable *table;
	CamelMedium *medium;
	gchar *subject;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	if (to != NULL)
		g_return_if_fail (CAMEL_IS_INTERNET_ADDRESS (to));

	if (cc != NULL)
		g_return_if_fail (CAMEL_IS_INTERNET_ADDRESS (cc));

	/* construct the tov/ccv */
	tov = em_utils_camel_address_to_destination (to);
	ccv = em_utils_camel_address_to_destination (cc);

	/* Set the subject of the new message. */
	if ((subject = (gchar *) camel_mime_message_get_subject (message))) {
		gboolean skip_len = -1;

		if (em_utils_is_re_in_subject (subject, &skip_len, NULL) && skip_len > 0)
			subject = subject + skip_len;

		subject = g_strdup_printf ("Re: %s", subject);
	} else {
		subject = g_strdup ("");
	}

	table = e_msg_composer_get_header_table (composer);
	e_composer_header_table_set_subject (table, subject);
	e_composer_header_table_set_destinations_to (table, tov);
	e_composer_header_table_set_identity_uid (table, identity_uid);

	/* Add destinations instead of setting, so we don't remove
	 * automatic CC addresses that have already been added. */
	e_composer_header_table_add_destinations_cc (table, ccv);

	e_destination_freev (tov);
	e_destination_freev (ccv);
	g_free (subject);

	/* add post-to, if nessecary */
	if (postto && camel_address_length ((CamelAddress *) postto)) {
		CamelFolder *use_folder = folder, *temp_folder = NULL;
		gchar *store_url = NULL;
		gchar *post;

		if (use_folder && CAMEL_IS_VEE_FOLDER (use_folder) && message_uid) {
			em_utils_get_real_folder_and_message_uid (use_folder, message_uid, &temp_folder, NULL, NULL);

			if (temp_folder)
				use_folder = temp_folder;
		}

		if (use_folder) {
			CamelStore *parent_store;
			CamelService *service;
			CamelURL *url;

			parent_store = camel_folder_get_parent_store (use_folder);

			service = CAMEL_SERVICE (parent_store);
			url = camel_service_new_camel_url (service);

			store_url = camel_url_to_string (
				url, CAMEL_URL_HIDE_ALL);
			if (store_url[strlen (store_url) - 1] == '/')
				store_url[strlen (store_url) - 1] = '\0';

			camel_url_free (url);
		}

		post = camel_address_encode ((CamelAddress *) postto);
		e_composer_header_table_set_post_to_base (
			table, store_url ? store_url : "", post);
		g_free (post);
		g_free (store_url);
		g_clear_object (&temp_folder);
	}

	/* Add In-Reply-To and References. */

	medium = CAMEL_MEDIUM (message);
	message_id = camel_header_unfold (camel_medium_get_header (medium, "Message-ID"));
	references = camel_header_unfold (camel_medium_get_header (medium, "References"));

	if (message_id != NULL) {
		gchar *reply_refs;

		e_msg_composer_add_header (
			composer, "In-Reply-To", message_id);

		if (references)
			reply_refs = g_strdup_printf (
				"%s %s", references, message_id);
		else
			reply_refs = g_strdup (message_id);

		e_msg_composer_add_header (
			composer, "References", reply_refs);
		g_free (reply_refs);

	} else if (references != NULL) {
		e_msg_composer_add_header (
			composer, "References", references);
	}

	g_free (message_id);
	g_free (references);
}

static gboolean
get_reply_list (CamelMimeMessage *message,
                CamelInternetAddress *to)
{
	const gchar *header, *p;
	gchar *addr;

	/* Examples:
	 *
	 * List-Post: <mailto:list@host.com>
	 * List-Post: <mailto:moderator@host.com?subject=list%20posting>
	 * List-Post: NO (posting not allowed on this list)
	 */
	if (!(header = camel_medium_get_header ((CamelMedium *) message, "List-Post")))
		return FALSE;

	while (*header == ' ' || *header == '\t')
		header++;

	/* check for NO */
	if (!g_ascii_strncasecmp (header, "NO", 2))
		return FALSE;

	/* Search for the first mailto angle-bracket enclosed URL.
	 * (See rfc2369, Section 2, paragraph 3 for details) */
	if (!(header = camel_strstrcase (header, "<mailto:")))
		return FALSE;

	header += 8;

	p = header;
	while (*p && !strchr ("?>", *p))
		p++;

	addr = g_strndup (header, p - header);
	camel_internet_address_add (to, NULL, addr);
	g_free (addr);

	return TRUE;
}

gboolean
em_utils_is_munged_list_message (CamelMimeMessage *message)
{
	CamelInternetAddress *reply_to, *list;
	gboolean result = FALSE;

	reply_to = camel_mime_message_get_reply_to (message);
	if (reply_to) {
		list = camel_internet_address_new ();

		if (get_reply_list (message, list) &&
		    camel_address_length (CAMEL_ADDRESS (list)) ==
		    camel_address_length (CAMEL_ADDRESS (reply_to))) {
			gint i;
			const gchar *r_name, *r_addr;
			const gchar *l_name, *l_addr;

			for (i = 0; i < camel_address_length (CAMEL_ADDRESS (list)); i++) {
				if (!camel_internet_address_get (reply_to, i, &r_name, &r_addr))
					break;
				if (!camel_internet_address_get (list, i, &l_name, &l_addr))
					break;
				if (strcmp (l_addr, r_addr))
					break;
			}
			if (i == camel_address_length (CAMEL_ADDRESS (list)))
				result = TRUE;
		}
		g_object_unref (list);
	}
	return result;
}

static CamelInternetAddress *
get_reply_to (CamelMimeMessage *message)
{
	CamelInternetAddress *reply_to;

	reply_to = camel_mime_message_get_reply_to (message);
	if (reply_to) {
		GSettings *settings;
		gboolean ignore_list_reply_to;

		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		ignore_list_reply_to = g_settings_get_boolean (
			settings, "composer-ignore-list-reply-to");
		g_object_unref (settings);

		if (ignore_list_reply_to && em_utils_is_munged_list_message (message))
			reply_to = NULL;
	}
	if (!reply_to)
		reply_to = camel_mime_message_get_from (message);

	return reply_to;
}

static void
get_reply_sender (CamelMimeMessage *message,
                  CamelInternetAddress *to,
                  CamelNNTPAddress *postto)
{
	CamelInternetAddress *reply_to;
	CamelMedium *medium;
	const gchar *posthdr = NULL;

	medium = CAMEL_MEDIUM (message);

	/* check whether there is a 'Newsgroups: ' header in there */
	if (postto != NULL && posthdr == NULL)
		posthdr = camel_medium_get_header (medium, "Followup-To");

	if (postto != NULL && posthdr == NULL)
		posthdr = camel_medium_get_header (medium, "Newsgroups");

	if (postto != NULL && posthdr != NULL) {
		camel_address_decode (CAMEL_ADDRESS (postto), posthdr);
		return;
	}

	reply_to = get_reply_to (message);

	if (reply_to != NULL) {
		const gchar *name;
		const gchar *addr;
		gint ii = 0;

		while (camel_internet_address_get (reply_to, ii++, &name, &addr))
			camel_internet_address_add (to, name, addr);
	}
}

void
em_utils_get_reply_sender (CamelMimeMessage *message,
                           CamelInternetAddress *to,
                           CamelNNTPAddress *postto)
{
	get_reply_sender (message, to, postto);
}

static void
get_reply_from (CamelMimeMessage *message,
                CamelInternetAddress *to,
                CamelNNTPAddress *postto)
{
	CamelInternetAddress *from;
	CamelMedium *medium;
	const gchar *name, *addr;
	const gchar *posthdr = NULL;

	medium = CAMEL_MEDIUM (message);

	/* check whether there is a 'Newsgroups: ' header in there */
	if (postto != NULL && posthdr == NULL)
		posthdr = camel_medium_get_header (medium, "Followup-To");

	if (postto != NULL && posthdr == NULL)
		posthdr = camel_medium_get_header (medium, "Newsgroups");

	if (postto != NULL && posthdr != NULL) {
		camel_address_decode (CAMEL_ADDRESS (postto), posthdr);
		return;
	}

	from = camel_mime_message_get_from (message);

	if (from != NULL) {
		gint ii = 0;

		while (camel_internet_address_get (from, ii++, &name, &addr))
			camel_internet_address_add (to, name, addr);
	}
}

static void
get_reply_recipient (CamelMimeMessage *message,
                     CamelInternetAddress *to,
                     CamelNNTPAddress *postto,
                     CamelInternetAddress *address)
{
	CamelMedium *medium;
	const gchar *posthdr = NULL;

	medium = CAMEL_MEDIUM (message);

	/* check whether there is a 'Newsgroups: ' header in there */
	if (postto != NULL && posthdr == NULL)
		posthdr = camel_medium_get_header (medium, "Followup-To");

	if (postto != NULL && posthdr == NULL)
		 posthdr = camel_medium_get_header (medium, "Newsgroups");

	if (postto != NULL && posthdr != NULL) {
		camel_address_decode (CAMEL_ADDRESS (postto), posthdr);
		return;
	}

	if (address != NULL) {
		const gchar *name;
		const gchar *addr;
		gint ii = 0;

		while (camel_internet_address_get (address, ii++, &name, &addr))
			camel_internet_address_add (to, name, addr);
	}

}

static void
concat_unique_addrs (CamelInternetAddress *dest,
                     CamelInternetAddress *src,
                     GHashTable *rcpt_hash)
{
	const gchar *name, *addr;
	gint i;

	for (i = 0; camel_internet_address_get (src, i, &name, &addr); i++) {
		if (!g_hash_table_contains (rcpt_hash, addr)) {
			camel_internet_address_add (dest, name, addr);
			g_hash_table_add (rcpt_hash, (gpointer) addr);
		}
	}
}

static GHashTable *
generate_recipient_hash (ESourceRegistry *registry)
{
	GHashTable *rcpt_hash;
	ESource *default_source;
	GList *list, *link;
	const gchar *extension_name;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	rcpt_hash = g_hash_table_new (
		(GHashFunc) camel_strcase_hash,
		(GEqualFunc) camel_strcase_equal);

	default_source = e_source_registry_ref_default_mail_identity (registry);

	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
	list = e_source_registry_list_sources (registry, extension_name);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		ESource *cached_source;
		ESourceMailIdentity *extension;
		const gchar *address;
		gboolean insert_source;
		gboolean cached_is_default;
		gboolean cached_is_enabled;
		gboolean source_is_default;
		gboolean source_is_enabled;

		/* No default mail identity implies there are no mail
		 * identities at all and so we should never get here. */
		g_warn_if_fail (default_source != NULL);

		source_is_default =
			e_source_equal (source, default_source);
		source_is_enabled =
			e_source_registry_check_enabled (registry, source);

		extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
		extension = e_source_get_extension (source, extension_name);

		address = e_source_mail_identity_get_address (extension);

		if (address == NULL)
			continue;

		cached_source = g_hash_table_lookup (rcpt_hash, address);

		if (cached_source != NULL) {
			cached_is_default = e_source_equal (
				cached_source, default_source);
			cached_is_enabled = e_source_registry_check_enabled (
				registry, cached_source);
		} else {
			cached_is_default = FALSE;
			cached_is_enabled = FALSE;
		}

		/* Accounts with identical email addresses that are enabled
		 * take precedence over disabled accounts.  If all accounts
		 * with matching email addresses are disabled, the first
		 * one in the list takes precedence.  The default account
		 * always takes precedence no matter what. */
		insert_source =
			source_is_default ||
			cached_source == NULL ||
			(source_is_enabled &&
			 !cached_is_enabled &&
			 !cached_is_default);

		if (insert_source)
			g_hash_table_insert (
				rcpt_hash, (gchar *) address, source);
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	if (default_source != NULL)
		g_object_unref (default_source);

	return rcpt_hash;
}

void
em_utils_get_reply_all (ESourceRegistry *registry,
                        CamelMimeMessage *message,
                        CamelInternetAddress *to,
                        CamelInternetAddress *cc,
                        CamelNNTPAddress *postto)
{
	CamelInternetAddress *reply_to;
	CamelInternetAddress *to_addrs;
	CamelInternetAddress *cc_addrs;
	CamelMedium *medium;
	const gchar *name, *addr;
	const gchar *posthdr = NULL;
	GHashTable *rcpt_hash;

	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));
	g_return_if_fail (CAMEL_IS_INTERNET_ADDRESS (to));
	g_return_if_fail (CAMEL_IS_INTERNET_ADDRESS (cc));

	medium = CAMEL_MEDIUM (message);

	/* check whether there is a 'Newsgroups: ' header in there */
	if (postto != NULL && posthdr == NULL)
		posthdr = camel_medium_get_header (medium, "Followup-To");

	if (postto != NULL && posthdr == NULL)
		posthdr = camel_medium_get_header (medium, "Newsgroups");

	if (postto != NULL && posthdr != NULL)
		camel_address_decode (CAMEL_ADDRESS (postto), posthdr);

	rcpt_hash = generate_recipient_hash (registry);

	reply_to = get_reply_to (message);
	to_addrs = camel_mime_message_get_recipients (
		message, CAMEL_RECIPIENT_TYPE_TO);
	cc_addrs = camel_mime_message_get_recipients (
		message, CAMEL_RECIPIENT_TYPE_CC);

	if (reply_to != NULL) {
		gint ii = 0;

		while (camel_internet_address_get (reply_to, ii++, &name, &addr)) {
			/* Ignore references to the Reply-To address
			 * in the To and Cc lists. */
			if (addr && !g_hash_table_contains (rcpt_hash, addr)) {
				/* In the case we are doing a Reply-To-All,
				 * we do not want to include the user's email
				 * address because replying to oneself is
				 * kinda silly. */
				camel_internet_address_add (to, name, addr);
				g_hash_table_add (rcpt_hash, (gpointer) addr);
			}
		}
	}

	concat_unique_addrs (to, to_addrs, rcpt_hash);
	concat_unique_addrs (cc, cc_addrs, rcpt_hash);

	/* Promote the first Cc: address to To: if To: is empty. */
	if (camel_address_length ((CamelAddress *) to) == 0 &&
			camel_address_length ((CamelAddress *) cc) > 0) {
		camel_internet_address_get (cc, 0, &name, &addr);
		camel_internet_address_add (to, name, addr);
		camel_address_remove ((CamelAddress *) cc, 0);
	}

	/* If To: is still empty, may we removed duplicates (i.e. ourself),
	 * so add the original To if it was set. */
	if (camel_address_length ((CamelAddress *) to) == 0
	    && (camel_internet_address_get (to_addrs, 0, &name, &addr)
		|| camel_internet_address_get (cc_addrs, 0, &name, &addr))) {
		camel_internet_address_add (to, name, addr);
	}

	g_hash_table_destroy (rcpt_hash);
}

enum {
	ATTRIB_UNKNOWN,
	ATTRIB_CUSTOM,
	ATTRIB_TIMEZONE,
	ATTRIB_STRFTIME,
	ATTRIB_TM_SEC,
	ATTRIB_TM_MIN,
	ATTRIB_TM_24HOUR,
	ATTRIB_TM_12HOUR,
	ATTRIB_TM_MDAY,
	ATTRIB_TM_MON,
	ATTRIB_TM_YEAR,
	ATTRIB_TM_2YEAR,
	ATTRIB_TM_WDAY, /* not actually used */
	ATTRIB_TM_YDAY
};

typedef void		(*AttribFormatter)	(GString *str,
						 const gchar *attr,
						 CamelMimeMessage *message);

static void
format_sender (GString *str,
               const gchar *attr,
               CamelMimeMessage *message)
{
	CamelInternetAddress *sender;
	const gchar *name, *addr = NULL;

	sender = camel_mime_message_get_from (message);
	if (sender != NULL && camel_address_length (CAMEL_ADDRESS (sender)) > 0) {
		camel_internet_address_get (sender, 0, &name, &addr);
	} else {
		name = _("an unknown sender");
	}

	if (name && !strcmp (attr, "{SenderName}")) {
		g_string_append (str, name);
	} else if (addr && !strcmp (attr, "{SenderEMail}")) {
		g_string_append (str, addr);
	} else if (name && *name) {
		g_string_append (str, name);
	} else if (addr) {
		g_string_append (str, addr);
	}
}

static struct {
	const gchar *name;
	gint type;
	struct {
		const gchar *format;         /* strftime or printf format */
		AttribFormatter formatter;  /* custom formatter */
	} v;
} attribvars[] = {
	{ "{Sender}", ATTRIB_CUSTOM, { NULL, format_sender } },
	{ "{SenderName}", ATTRIB_CUSTOM, { NULL, format_sender } },
	{ "{SenderEMail}", ATTRIB_CUSTOM, { NULL, format_sender } },
	{ "{AbbrevWeekdayName}", ATTRIB_STRFTIME, { "%a", NULL } },
	{ "{WeekdayName}", ATTRIB_STRFTIME, { "%A", NULL } },
	{ "{AbbrevMonthName}", ATTRIB_STRFTIME, { "%b", NULL } },
	{ "{MonthName}", ATTRIB_STRFTIME, { "%B", NULL } },
	{ "{AmPmUpper}", ATTRIB_STRFTIME, { "%p", NULL } },
	{ "{AmPmLower}", ATTRIB_STRFTIME, { "%P", NULL } },
	{ "{Day}", ATTRIB_TM_MDAY, { "%02d", NULL } },  /* %d  01-31 */
	{ "{ Day}", ATTRIB_TM_MDAY, { "% 2d", NULL } },  /* %e   1-31 */
	{ "{24Hour}", ATTRIB_TM_24HOUR, { "%02d", NULL } },  /* %H  00-23 */
	{ "{12Hour}", ATTRIB_TM_12HOUR, { "%02d", NULL } },  /* %I  00-12 */
	{ "{DayOfYear}", ATTRIB_TM_YDAY, { "%d", NULL } },  /* %j  1-366 */
	{ "{Month}", ATTRIB_TM_MON, { "%02d", NULL } },  /* %m  01-12 */
	{ "{Minute}", ATTRIB_TM_MIN, { "%02d", NULL } },  /* %M  00-59 */
	{ "{Seconds}", ATTRIB_TM_SEC, { "%02d", NULL } },  /* %S  00-61 */
	{ "{2DigitYear}", ATTRIB_TM_2YEAR, { "%02d", NULL } },  /* %y */
	{ "{Year}", ATTRIB_TM_YEAR, { "%04d", NULL } },  /* %Y */
	{ "{TimeZone}", ATTRIB_TIMEZONE, { "%+05d", NULL } }
};

static gchar *
attribution_format (CamelMimeMessage *message)
{
	register const gchar *inptr;
	const gchar *start;
	gint tzone, len, i;
	gchar buf[64], *s;
	GString *str;
	struct tm tm;
	time_t date;
	gint type;
	gchar *format = quoting_text (QUOTING_ATTRIBUTION);

	str = g_string_new ("");

	date = camel_mime_message_get_date (message, &tzone);

	if (date == CAMEL_MESSAGE_DATE_CURRENT) {
		/* The message has no Date: header, look at Received: */
		date = camel_mime_message_get_date_received (message, &tzone);
	}
	if (date == CAMEL_MESSAGE_DATE_CURRENT) {
		/* That didn't work either, use current time */
		time (&date);
		tzone = 0;
	}

	/* Convert to UTC */
	date += (tzone / 100) * 60 * 60;
	date += (tzone % 100) * 60;

	gmtime_r (&date, &tm);

	inptr = format;
	while (*inptr != '\0') {
		start = inptr;
		while (*inptr && strncmp (inptr, "${", 2) != 0)
			inptr++;

		g_string_append_len (str, start, inptr - start);

		if (*inptr == '\0')
			break;

		start = ++inptr;
		while (*inptr && *inptr != '}')
			inptr++;

		if (*inptr != '}') {
			/* broken translation */
			g_string_append_len (str, "${", 2);
			inptr = start + 1;
			continue;
		}

		inptr++;
		len = inptr - start;
		type = ATTRIB_UNKNOWN;
		for (i = 0; i < G_N_ELEMENTS (attribvars); i++) {
			if (!strncmp (attribvars[i].name, start, len)) {
				type = attribvars[i].type;
				break;
			}
		}

		switch (type) {
		case ATTRIB_CUSTOM:
			attribvars[i].v.formatter (
				str, attribvars[i].name, message);
			break;
		case ATTRIB_TIMEZONE:
			g_string_append_printf (
				str, attribvars[i].v.format, tzone);
			break;
		case ATTRIB_STRFTIME:
			e_utf8_strftime_match_lc_messages (
				buf, sizeof (buf), attribvars[i].v.format, &tm);
			g_string_append (str, buf);
			break;
		case ATTRIB_TM_SEC:
			g_string_append_printf (
				str, attribvars[i].v.format, tm.tm_sec);
			break;
		case ATTRIB_TM_MIN:
			g_string_append_printf (
				str, attribvars[i].v.format, tm.tm_min);
			break;
		case ATTRIB_TM_24HOUR:
			g_string_append_printf (
				str, attribvars[i].v.format, tm.tm_hour);
			break;
		case ATTRIB_TM_12HOUR:
			g_string_append_printf (
				str, attribvars[i].v.format,
				(tm.tm_hour + 1) % 13);
			break;
		case ATTRIB_TM_MDAY:
			g_string_append_printf (
				str, attribvars[i].v.format, tm.tm_mday);
			break;
		case ATTRIB_TM_MON:
			g_string_append_printf (
				str, attribvars[i].v.format, tm.tm_mon + 1);
			break;
		case ATTRIB_TM_YEAR:
			g_string_append_printf (
				str, attribvars[i].v.format, tm.tm_year + 1900);
			break;
		case ATTRIB_TM_2YEAR:
			g_string_append_printf (
				str, attribvars[i].v.format, tm.tm_year % 100);
			break;
		case ATTRIB_TM_WDAY:
			/* not actually used */
			g_string_append_printf (
				str, attribvars[i].v.format, tm.tm_wday);
			break;
		case ATTRIB_TM_YDAY:
			g_string_append_printf (
				str, attribvars[i].v.format, tm.tm_yday + 1);
			break;
		default:
			/* Misspelled variable?  Drop the
			 * format argument and continue. */
			break;
		}
	}

	s = str->str;
	g_string_free (str, FALSE);
	g_free (format);

	return s;
}

static void
composer_set_body (EMsgComposer *composer,
                   CamelMimeMessage *message,
                   EMailReplyStyle style,
                   EMailPartList *parts_list)
{
	gchar *text, *credits, *original;
	CamelMimePart *part;
	CamelSession *session;
	guint32 validity_found = 0;

	session = e_msg_composer_ref_session (composer);

	switch (style) {
	case E_MAIL_REPLY_STYLE_DO_NOT_QUOTE:
		/* do nothing */
		break;
	case E_MAIL_REPLY_STYLE_ATTACH:
		/* attach the original message as an attachment */
		part = mail_tool_make_message_attachment (message);
		e_msg_composer_attach (composer, part);
		g_object_unref (part);
		break;
	case E_MAIL_REPLY_STYLE_OUTLOOK:
		original = quoting_text (QUOTING_ORIGINAL);
		text = em_utils_message_to_html (
			session, message, original, E_MAIL_FORMATTER_QUOTE_FLAG_HEADERS,
			parts_list, NULL, NULL, &validity_found);
		e_msg_composer_set_body_text (composer, text, TRUE);
		g_free (text);
		g_free (original);
		emu_update_composers_security (composer, validity_found);
		break;

	case E_MAIL_REPLY_STYLE_QUOTED:
	default:
		/* do what any sane user would want when replying... */
		credits = attribution_format (message);
		text = em_utils_message_to_html (
			session, message, credits, E_MAIL_FORMATTER_QUOTE_FLAG_CITE,
			parts_list, NULL, NULL, &validity_found);
		g_free (credits);
		e_msg_composer_set_body_text (composer, text, TRUE);
		g_free (text);
		emu_update_composers_security (composer, validity_found);
		break;
	}

	g_object_unref (session);
}

gchar *
em_utils_construct_composer_text (CamelSession *session,
                                  CamelMimeMessage *message,
                                  EMailPartList *parts_list)
{
	gchar *text, *credits;

	g_return_val_if_fail (CAMEL_IS_SESSION (session), NULL);

	credits = attribution_format (message);
	text = em_utils_message_to_html (
		session, message, credits, E_MAIL_FORMATTER_QUOTE_FLAG_CITE,
		parts_list, NULL, NULL, NULL);
	g_free (credits);

	return text;
}

static gboolean
emcu_folder_is_inbox (CamelFolder *folder)
{
	CamelSession *session;
	CamelStore *store;
	gboolean is_inbox = FALSE;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);

	store = camel_folder_get_parent_store (folder);
	if (!store)
		return FALSE;

	session = camel_service_ref_session (CAMEL_SERVICE (store));
	if (!session)
		return FALSE;

	if (E_IS_MAIL_SESSION (session)) {
		MailFolderCache *folder_cache;
		CamelFolderInfoFlags flags = 0;

		folder_cache = e_mail_session_get_folder_cache (E_MAIL_SESSION (session));
		if (folder_cache && mail_folder_cache_get_folder_info_flags (
			folder_cache, store, camel_folder_get_full_name (folder), &flags)) {
			is_inbox = (flags & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_INBOX;
		}
	}

	g_object_unref (session);

	return is_inbox;
}

/**
 * em_utils_reply_to_message:
 * @shell: an #EShell
 * @message: a #CamelMimeMessage
 * @folder: a #CamelFolder, or %NULL
 * @message_uid: the UID of @message, or %NULL
 * @type: the type of reply to create
 * @style: the reply style to use
 * @source: source to inherit view settings from
 * @address: used for E_MAIL_REPLY_TO_RECIPIENT @type
 *
 * Creates a new composer ready to reply to @message.
 *
 * @folder and @message_uid may be supplied in order to update the message
 * flags once it has been replied to.
 **/
void
em_utils_reply_to_message (EMsgComposer *composer,
                           CamelMimeMessage *message,
                           CamelFolder *folder,
                           const gchar *message_uid,
                           EMailReplyType type,
                           EMailReplyStyle style,
                           EMailPartList *parts_list,
                           CamelInternetAddress *address)
{
	ESourceRegistry *registry;
	CamelInternetAddress *to, *cc;
	CamelNNTPAddress *postto = NULL;
	EShell *shell;
	ESourceMailCompositionReplyStyle prefer_reply_style = E_SOURCE_MAIL_COMPOSITION_REPLY_STYLE_DEFAULT;
	ESource *source;
	gchar *identity_uid = NULL;
	guint32 flags;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	to = camel_internet_address_new ();
	cc = camel_internet_address_new ();

	shell = e_msg_composer_get_shell (composer);
	registry = e_shell_get_registry (shell);

	/* This returns a new ESource reference. */
	source = em_utils_check_send_account_override (shell, message, folder);
	if (!source)
		source = em_utils_guess_mail_identity_with_recipients_and_sort (
			registry, message, folder, message_uid, sort_sources_by_ui, shell);
	if (source != NULL) {
		identity_uid = e_source_dup_uid (source);
		if (e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_COMPOSITION)) {
			ESourceMailComposition *extension;

			extension = e_source_get_extension (source, E_SOURCE_EXTENSION_MAIL_COMPOSITION);
			prefer_reply_style = e_source_mail_composition_get_reply_style (extension);
		}

		g_object_unref (source);
	}

	flags = CAMEL_MESSAGE_ANSWERED | CAMEL_MESSAGE_SEEN;

	if (!address && (type == E_MAIL_REPLY_TO_FROM || type == E_MAIL_REPLY_TO_SENDER) &&
	    folder && !emcu_folder_is_inbox (folder) && em_utils_folder_is_sent (registry, folder))
		type = E_MAIL_REPLY_TO_ALL;

	switch (type) {
	case E_MAIL_REPLY_TO_FROM:
		if (folder)
			postto = camel_nntp_address_new ();

		get_reply_from (message, to, postto);
		break;
	case E_MAIL_REPLY_TO_RECIPIENT:
		if (folder)
			postto = camel_nntp_address_new ();

		get_reply_recipient (message, to, postto, address);
		break;
	case E_MAIL_REPLY_TO_SENDER:
		if (folder)
			postto = camel_nntp_address_new ();

		get_reply_sender (message, to, postto);
		break;
	case E_MAIL_REPLY_TO_LIST:
		flags |= CAMEL_MESSAGE_ANSWERED_ALL;
		if (get_reply_list (message, to))
			break;
		/* falls through */
	case E_MAIL_REPLY_TO_ALL:
		flags |= CAMEL_MESSAGE_ANSWERED_ALL;
		if (folder)
			postto = camel_nntp_address_new ();

		em_utils_get_reply_all (registry, message, to, cc, postto);
		break;
	}

	reply_setup_composer (composer, message, identity_uid, to, cc, folder, message_uid, postto);
	e_msg_composer_add_message_attachments (composer, message, TRUE);

	if (postto)
		g_object_unref (postto);
	g_object_unref (to);
	g_object_unref (cc);

	/* If there was no send-account override */
	if (!identity_uid) {
		EComposerHeaderTable *header_table;
		const gchar *used_identity_uid;

		header_table = e_msg_composer_get_header_table (composer);
		used_identity_uid = e_composer_header_table_get_identity_uid (header_table);

		if (used_identity_uid) {
			source = e_source_registry_ref_source (e_shell_get_registry (shell), used_identity_uid);
			if (source) {
				if (e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_COMPOSITION)) {
					ESourceMailComposition *extension;

					extension = e_source_get_extension (source, E_SOURCE_EXTENSION_MAIL_COMPOSITION);
					prefer_reply_style = e_source_mail_composition_get_reply_style (extension);
				}

				g_object_unref (source);
			}
		}
	}

	switch (prefer_reply_style) {
		case E_SOURCE_MAIL_COMPOSITION_REPLY_STYLE_DEFAULT:
			/* Do nothing, keep the passed-in reply style. */
			break;
		case E_SOURCE_MAIL_COMPOSITION_REPLY_STYLE_QUOTED:
			style = E_MAIL_REPLY_STYLE_QUOTED;
			break;
		case E_SOURCE_MAIL_COMPOSITION_REPLY_STYLE_DO_NOT_QUOTE:
			style = E_MAIL_REPLY_STYLE_DO_NOT_QUOTE;
			break;
		case E_SOURCE_MAIL_COMPOSITION_REPLY_STYLE_ATTACH:
			style = E_MAIL_REPLY_STYLE_ATTACH;
			break;
		case E_SOURCE_MAIL_COMPOSITION_REPLY_STYLE_OUTLOOK:
			style = E_MAIL_REPLY_STYLE_OUTLOOK;
			break;
	}

	composer_set_body (composer, message, style, parts_list);

	if (folder != NULL) {
		gchar *folder_uri = NULL, *tmp_message_uid = NULL;

		em_utils_get_real_folder_uri_and_message_uid (folder, message_uid, &folder_uri, &tmp_message_uid);

		e_msg_composer_set_source_headers (
			composer, folder_uri, tmp_message_uid, flags);

		g_free (folder_uri);
		g_free (tmp_message_uid);
	}

	/* because some reply types can change recipients after the composer is populated */
	em_utils_apply_send_account_override_to_composer (composer, folder);

	composer_set_no_change (composer);

	gtk_widget_show (GTK_WIDGET (composer));

	g_free (identity_uid);
}

static void
post_header_clicked_cb (EComposerPostHeader *header,
                        EMailSession *session)
{
	GtkTreeSelection *selection;
	EMFolderSelector *selector;
	EMFolderTreeModel *model;
	EMFolderTree *folder_tree;
	GtkWidget *dialog;
	GList *list;
	const gchar *caption;

	/* FIXME Limit the folder tree to the NNTP account? */
	model = em_folder_tree_model_get_default ();

	dialog = em_folder_selector_new (
		/* FIXME GTK_WINDOW (composer) */ NULL, model);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Posting destination"));

	selector = EM_FOLDER_SELECTOR (dialog);
	em_folder_selector_set_can_create (selector, TRUE);

	caption = _("Choose folders to post the message to.");
	em_folder_selector_set_caption (selector, caption);

	folder_tree = em_folder_selector_get_folder_tree (selector);

	em_folder_tree_set_excluded (
		folder_tree,
		EMFT_EXCLUDE_NOSELECT |
		EMFT_EXCLUDE_VIRTUAL |
		EMFT_EXCLUDE_VTRASH);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (folder_tree));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	list = e_composer_post_header_get_folders (header);
	em_folder_tree_set_selected_list (folder_tree, list, FALSE);
	g_list_foreach (list, (GFunc) g_free, NULL);
	g_list_free (list);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK) {
		/* Prevent the header's "custom" flag from being reset,
		 * which is what the default method will do next. */
		g_signal_stop_emission_by_name (header, "clicked");
		goto exit;
	}

	list = em_folder_tree_get_selected_uris (folder_tree);
	e_composer_post_header_set_folders (header, list);
	g_list_foreach (list, (GFunc) g_free, NULL);
	g_list_free (list);

exit:
	gtk_widget_destroy (dialog);
}

/**
 * em_configure_new_composer:
 * @composer: a newly created #EMsgComposer
 *
 * Integrates a newly created #EMsgComposer into the mail backend.  The
 * composer can't link directly to the mail backend without introducing
 * circular library dependencies, so this function finishes configuring
 * things the #EMsgComposer instance can't do itself.
 **/
void
em_configure_new_composer (EMsgComposer *composer,
                           EMailSession *session)
{
	EComposerHeaderTable *table;
	EComposerHeaderType header_type;
	EComposerHeader *header;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (E_IS_MAIL_SESSION (session));

	header_type = E_COMPOSER_HEADER_POST_TO;
	table = e_msg_composer_get_header_table (composer);
	header = e_composer_header_table_get_header (table, header_type);

	g_signal_connect (
		composer, "presend",
		G_CALLBACK (composer_presend_check_recipients), session);

	g_signal_connect (
		composer, "presend",
		G_CALLBACK (composer_presend_check_identity), session);

	g_signal_connect (
		composer, "presend",
		G_CALLBACK (composer_presend_check_plugins), session);

	g_signal_connect (
		composer, "presend",
		G_CALLBACK (composer_presend_check_subject), session);

	g_signal_connect (
		composer, "presend",
		G_CALLBACK (composer_presend_check_unwanted_html), session);

	g_signal_connect (
		composer, "send",
		G_CALLBACK (em_utils_composer_send_cb), session);

	g_signal_connect (
		composer, "save-to-drafts",
		G_CALLBACK (em_utils_composer_save_to_drafts_cb), session);

	g_signal_connect (
		composer, "save-to-outbox",
		G_CALLBACK (em_utils_composer_save_to_outbox_cb), session);

	g_signal_connect (
		composer, "print",
		G_CALLBACK (em_utils_composer_print_cb), session);

	/* Handle "Post To:" button clicks, which displays a folder tree
	 * widget.  The composer doesn't know about folder tree widgets,
	 * so it can't handle this itself.
	 *
	 * Note: This is a G_SIGNAL_RUN_LAST signal, which allows us to
	 *       stop the signal emission if the user cancels or closes
	 *       the folder selector dialog.  See the handler function. */
	g_signal_connect (
		header, "clicked",
		G_CALLBACK (post_header_clicked_cb), session);
}

/* free returned pointer with g_object_unref(), if not NULL */
ESource *
em_utils_check_send_account_override (EShell *shell,
                                      CamelMimeMessage *message,
                                      CamelFolder *folder)
{
	EMailBackend *mail_backend;
	EMailSendAccountOverride *account_override;
	CamelInternetAddress *to = NULL, *cc = NULL, *bcc = NULL;
	gchar *folder_uri = NULL, *account_uid;
	ESource *account_source = NULL;
	ESourceRegistry *source_registry;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	if (!message && !folder)
		return NULL;

	if (message) {
		to = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO);
		cc = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_CC);
		bcc = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_BCC);
	}

	mail_backend = E_MAIL_BACKEND (e_shell_get_backend_by_name (shell, "mail"));
	g_return_val_if_fail (mail_backend != NULL, NULL);

	if (folder)
		folder_uri = e_mail_folder_uri_from_folder (folder);

	source_registry = e_shell_get_registry (shell);
	account_override = e_mail_backend_get_send_account_override (mail_backend);
	account_uid = e_mail_send_account_override_get_account_uid (account_override, folder_uri, to, cc, bcc);

	while (account_uid) {
		account_source = e_source_registry_ref_source (source_registry, account_uid);
		if (account_source)
			break;

		/* stored send account override settings contain a reference
		 * to a dropped account, thus cleanup it now */
		e_mail_send_account_override_remove_for_account_uid (account_override, account_uid);

		g_free (account_uid);
		account_uid = e_mail_send_account_override_get_account_uid (account_override, folder_uri, to, cc, bcc);
	}

	g_free (folder_uri);
	g_free (account_uid);

	return account_source;
}

void
em_utils_apply_send_account_override_to_composer (EMsgComposer *composer,
                                                  CamelFolder *folder)
{
	CamelMimeMessage *message;
	EComposerHeaderTable *header_table;
	EShell *shell;
	ESource *source;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	shell = e_msg_composer_get_shell (composer);
	message = em_utils_get_composer_recipients_as_message (composer);
	source = em_utils_check_send_account_override (shell, message, folder);
	g_clear_object (&message);

	if (!source)
		return;

	header_table = e_msg_composer_get_header_table (composer);
	e_composer_header_table_set_identity_uid (header_table, e_source_get_uid (source));

	g_object_unref (source);
}
