/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-FileCopyrightText: (C) 2008 - Diego Escalante Urrelo
 * SPDX-FileCopyrightText: (C) 2018 Red Hat, Inc. (www.redhat.com)
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileContributor: Diego Escalante Urrelo <diegoe@gnome.org>
 * SPDX-FileContributor: Bharath Acharya <abharath@novell.com>
 */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "e-util/e-util.h"

#include "shell/e-shell-view.h"

#include "mail/e-mail-browser.h"
#include "mail/e-mail-paned-view.h"
#include "mail/e-mail-reader.h"
#include "mail/e-mail-reader-utils.h"
#include "mail/e-mail-ui-session.h"
#include "mail/e-mail-view.h"
#include "mail/e-mail-templates.h"
#include "mail/e-mail-templates-store.h"
#include "mail/em-composer-utils.h"
#include "mail/em-utils.h"
#include "mail/message-list.h"

#include "composer/e-msg-composer.h"

typedef struct _AsyncContext AsyncContext;

struct _AsyncContext {
	EActivity *activity;
	EMailReader *reader;
	CamelMimeMessage *source_message;
	CamelMimeMessage *new_message;
	CamelFolder *template_folder;
	CamelFolder *source_folder;
	gchar *source_folder_uri;
	gchar *source_message_uid;
	gchar *orig_source_message_uid;
	gchar *template_message_uid;
	gboolean selection_is_html;
	EMailPartValidityFlags validity_pgp_sum;
	EMailPartValidityFlags validity_smime_sum;
};

#define TEMPLATES_DATA_KEY "templates::data"

typedef struct _TemplatesData {
	GWeakRef mail_reader_weakref;
	EMailTemplatesStore *templates_store;
	GMenu *reply_template_menu;
	gulong changed_handler_id;
	guint update_menu_id;
	gboolean changed;
	gboolean update_immediately;
} TemplatesData;

static void
templates_data_free (gpointer ptr)
{
	TemplatesData *td = ptr;

	if (td) {
		if (td->templates_store && td->changed_handler_id) {
			g_signal_handler_disconnect (td->templates_store, td->changed_handler_id);
			td->changed_handler_id = 0;
		}

		if (td->update_menu_id) {
			g_source_remove (td->update_menu_id);
			td->update_menu_id = 0;
		}

		g_clear_object (&td->templates_store);
		g_weak_ref_clear (&td->mail_reader_weakref);
		g_clear_object (&td->reply_template_menu);
		g_free (td);
	}
}

static void
async_context_free (AsyncContext *context)
{
	g_clear_object (&context->activity);
	g_clear_object (&context->reader);
	g_clear_object (&context->source_message);
	g_clear_object (&context->new_message);
	g_clear_object (&context->source_folder);
	g_clear_object (&context->template_folder);

	g_free (context->source_folder_uri);
	g_free (context->source_message_uid);
	g_free (context->orig_source_message_uid);
	g_free (context->template_message_uid);

	g_slice_free (AsyncContext, context);
}

static void
create_new_message_composer_created_cb (GObject *source_object,
					GAsyncResult *result,
					gpointer user_data)
{
	AsyncContext *context = user_data;
	EAlertSink *alert_sink;
	EMsgComposer *composer;
	GError *error = NULL;

	g_return_if_fail (context != NULL);

	alert_sink = e_activity_get_alert_sink (context->activity);

	composer = e_msg_composer_new_finish (result, &error);

	if (e_activity_handle_cancellation (context->activity, error)) {
		async_context_free (context);
		g_error_free (error);
		return;

	} else if (error != NULL) {
		e_alert_submit (
			alert_sink, "mail:no-retrieve-message",
			error->message, NULL);
		async_context_free (context);
		g_error_free (error);
		return;
	}

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	/* Create the composer */
	em_utils_edit_message (composer, context->template_folder, context->new_message, context->source_message_uid, TRUE, FALSE);

	em_composer_utils_update_security (composer, context->validity_pgp_sum, context->validity_smime_sum);

	if (context->source_folder_uri && context->source_message_uid)
		e_msg_composer_set_source_headers (
			composer, context->source_folder_uri,
			context->source_message_uid, CAMEL_MESSAGE_ANSWERED | CAMEL_MESSAGE_SEEN);

	async_context_free (context);
}

static void
templates_template_applied_cb (GObject *source_object,
			       GAsyncResult *result,
			       gpointer user_data)
{
	AsyncContext *context = user_data;
	EAlertSink *alert_sink;
	EMailBackend *backend;
	EShell *shell;
	GError *error = NULL;

	g_return_if_fail (context != NULL);

	alert_sink = e_activity_get_alert_sink (context->activity);

	context->new_message = e_mail_templates_apply_finish (source_object, result, &error);

	if (e_activity_handle_cancellation (context->activity, error)) {
		g_warn_if_fail (context->new_message == NULL);
		async_context_free (context);
		g_error_free (error);
		return;

	} else if (error != NULL) {
		g_warn_if_fail (context->new_message == NULL);
		e_alert_submit (
			alert_sink, "mail:no-retrieve-message",
			error->message, NULL);
		async_context_free (context);
		g_error_free (error);
		return;
	}

	g_warn_if_fail (context->new_message != NULL);

	backend = e_mail_reader_get_backend (context->reader);
	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (backend));

	e_msg_composer_new (shell, create_new_message_composer_created_cb, context);
}

static void
template_got_message_cb (GObject *source_object,
			 GAsyncResult *result,
			 gpointer user_data)
{
	AsyncContext *context = user_data;
	EAlertSink *alert_sink;
	CamelFolder *folder = NULL;
	CamelMimeMessage *message;
	GError *error = NULL;

	alert_sink = e_activity_get_alert_sink (context->activity);

	message = e_mail_reader_utils_get_selection_or_message_finish (E_MAIL_READER (source_object), result,
			NULL, &folder, NULL, NULL, &context->validity_pgp_sum, &context->validity_smime_sum, &error);

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

	context->source_message = message;

	e_mail_templates_apply (context->source_message, folder, context->orig_source_message_uid,
		context->template_folder, context->template_message_uid,
		e_activity_get_cancellable (context->activity), templates_template_applied_cb, context);
}

static void
action_reply_with_template_cb (EMailTemplatesStore *templates_store,
			       CamelFolder *template_folder,
			       const gchar *template_message_uid,
			       gpointer user_data)
{
	EMailReader *reader = user_data;
	EActivity *activity;
	AsyncContext *context;
	GCancellable *cancellable;
	CamelFolder *folder;
	GPtrArray *uids;
	const gchar *message_uid;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	uids = e_mail_reader_get_selected_uids (reader);
	g_return_if_fail (uids != NULL && uids->len == 1);
	message_uid = g_ptr_array_index (uids, 0);

	activity = e_mail_reader_new_activity (reader);
	cancellable = e_activity_get_cancellable (activity);

	context = g_slice_new0 (AsyncContext);
	context->activity = activity;
	context->reader = g_object_ref (reader);
	context->orig_source_message_uid = g_strdup (message_uid);
	context->template_folder = g_object_ref (template_folder);
	context->template_message_uid = g_strdup (template_message_uid);

	folder = e_mail_reader_ref_folder (reader);

	em_utils_get_real_folder_uri_and_message_uid (
		folder, message_uid,
		&context->source_folder_uri,
		&context->source_message_uid);

	if (context->source_message_uid == NULL)
		context->source_message_uid = g_strdup (message_uid);

	e_mail_reader_utils_get_selection_or_message (reader, NULL, cancellable,
		template_got_message_cb, context);

	g_clear_object (&folder);
	g_ptr_array_unref (uids);
}

static gchar *
get_account_templates_folder_uri (EMsgComposer *composer)
{
	EComposerHeaderTable *table;
	ESource *source;
	gchar *identity_uid;
	gchar *templates_folder_uri = NULL;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	table = e_msg_composer_get_header_table (composer);
	identity_uid = e_composer_header_table_dup_identity_uid (table, NULL, NULL);
	source = e_composer_header_table_ref_source (table, identity_uid);

	/* Get the selected identity's preferred Templates folder. */
	if (source != NULL) {
		ESourceMailComposition *extension;
		const gchar *extension_name;

		extension_name = E_SOURCE_EXTENSION_MAIL_COMPOSITION;
		extension = e_source_get_extension (source, extension_name);
		templates_folder_uri = e_source_mail_composition_dup_templates_folder (extension);

		g_object_unref (source);
	}

	g_free (identity_uid);

	return templates_folder_uri;
}

typedef struct _SaveTemplateAsyncData {
	EMsgComposer *composer;
	EMailSession *session;
	CamelMimeMessage *message;
	CamelMessageInfo *info;
	gchar *templates_folder_uri;
	gchar *delete_message_uid;
	gchar *new_message_uid;
} SaveTemplateAsyncData;

static void
save_template_async_data_free (gpointer ptr)
{
	SaveTemplateAsyncData *sta = ptr;

	if (sta) {
		if (sta->templates_folder_uri && sta->new_message_uid) {
			EHTMLEditor *editor;
			EUIAction *action;

			e_msg_composer_set_header (sta->composer, "X-Evolution-Templates-Folder", sta->templates_folder_uri);
			e_msg_composer_set_header (sta->composer, "X-Evolution-Templates-Message", sta->new_message_uid);

			editor = e_msg_composer_get_editor (sta->composer);
			action = e_html_editor_get_action (editor, "template-replace");
			if (action) {
				e_ui_action_set_visible (action, TRUE);
				e_ui_action_set_sensitive (action, TRUE);
			}
		}

		g_clear_object (&sta->composer);
		g_clear_object (&sta->session);
		g_clear_object (&sta->message);
		g_clear_object (&sta->info);
		g_free (sta->templates_folder_uri);
		g_free (sta->delete_message_uid);
		g_free (sta->new_message_uid);
		g_slice_free (SaveTemplateAsyncData, sta);
	}
}

static void
save_template_thread (EAlertSinkThreadJobData *job_data,
		      gpointer user_data,
		      GCancellable *cancellable,
		      GError **error)
{
	SaveTemplateAsyncData *sta = user_data;
	CamelFolder *templates_folder = NULL;
	gboolean success;

	if (sta->templates_folder_uri && *sta->templates_folder_uri) {
		templates_folder = e_mail_session_uri_to_folder_sync (sta->session,
			sta->templates_folder_uri, 0, cancellable, error);
		if (!templates_folder)
			return;
	}

	if (!templates_folder) {
		g_clear_pointer (&sta->templates_folder_uri, g_free);
		sta->templates_folder_uri = g_strdup (e_mail_session_get_local_folder_uri (sta->session, E_MAIL_LOCAL_FOLDER_TEMPLATES));

		success = e_mail_session_append_to_local_folder_sync (
			sta->session, E_MAIL_LOCAL_FOLDER_TEMPLATES,
			sta->message, sta->info,
			&sta->new_message_uid, cancellable, error);
	} else {
		success = e_mail_folder_append_message_sync (
			templates_folder, sta->message, sta->info,
			&sta->new_message_uid, cancellable, error);
	}

	if (success && sta->delete_message_uid && templates_folder)
		camel_folder_set_message_flags (templates_folder, sta->delete_message_uid, CAMEL_MESSAGE_DELETED, CAMEL_MESSAGE_DELETED);

	g_clear_object (&templates_folder);
}

static void
got_message_draft_cb (GObject *source_object,
                      GAsyncResult *result,
		      gpointer user_data)
{
	EMsgComposer *composer = E_MSG_COMPOSER (source_object);
	gboolean replace_template = GPOINTER_TO_INT (user_data) == 1;
	EShell *shell;
	EShellBackend *shell_backend;
	EMailBackend *backend;
	EMailSession *session;
	EHTMLEditor *html_editor;
	CamelMimeMessage *message;
	CamelMessageInfo *info;
	SaveTemplateAsyncData *sta;
	EActivity *activity;
	GError *error = NULL;

	message = e_msg_composer_get_message_draft_finish (
		composer, result, &error);

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warn_if_fail (message == NULL);
		g_error_free (error);
		return;
	}

	if (error != NULL) {
		g_warn_if_fail (message == NULL);
		e_alert_run_dialog_for_args (
			GTK_WINDOW (composer),
			"mail-composer:no-build-message",
			error->message, NULL);
		g_error_free (error);
		return;
	}

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (shell, "mail");

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);

	info = camel_message_info_new (NULL);

	/* The last argument is a bit mask which tells the function
	 * which flags to modify.  In this case, ~0 means all flags.
	 * So it clears all the flags and then sets SEEN and DRAFT. */
	camel_message_info_set_flags (
		info, CAMEL_MESSAGE_SEEN | CAMEL_MESSAGE_DRAFT |
		(camel_mime_message_has_attachment (message) ? CAMEL_MESSAGE_ATTACHMENTS : 0), ~0);

	sta = g_slice_new0 (SaveTemplateAsyncData);
	sta->composer = g_object_ref (composer);
	sta->session = g_object_ref (session);
	sta->message = message;
	sta->info = info;

	if (replace_template) {
		const gchar *existing_folder_uri;
		const gchar *existing_message_uid;

		existing_folder_uri = e_msg_composer_get_header (composer, "X-Evolution-Templates-Folder", 0);
		existing_message_uid = e_msg_composer_get_header (composer, "X-Evolution-Templates-Message", 0);

		if (existing_folder_uri && *existing_folder_uri && existing_message_uid && *existing_message_uid) {
			sta->templates_folder_uri = g_strdup (existing_folder_uri);
			sta->delete_message_uid = g_strdup (existing_message_uid);
		}
	}

	if (!sta->templates_folder_uri)
		sta->templates_folder_uri = get_account_templates_folder_uri (composer);

	html_editor = e_msg_composer_get_editor (composer);

	activity = e_alert_sink_submit_thread_job (E_ALERT_SINK (html_editor),
			_("Saving message template"),
			"mail-composer:failed-save-template",
			NULL, save_template_thread, sta, save_template_async_data_free);

	g_clear_object (&activity);
}

static void
action_template_replace_cb (EUIAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	EMsgComposer *composer = user_data;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	/* XXX Pass a GCancellable */
	e_msg_composer_get_message_draft (
		composer, G_PRIORITY_DEFAULT, NULL,
		got_message_draft_cb, GINT_TO_POINTER (1));
}

static void
action_template_save_new_cb (EUIAction *action,
			     GVariant *parameter,
			     gpointer user_data)
{
	EMsgComposer *composer = user_data;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	/* XXX Pass a GCancellable */
	e_msg_composer_get_message_draft (
		composer, G_PRIORITY_DEFAULT, NULL,
		got_message_draft_cb, GINT_TO_POINTER (0));
}

static void
templates_composer_realize_cb (EMsgComposer *composer,
			       gpointer user_data)
{
	EHTMLEditor *editor;
	EUIAction *action;
	const gchar *existing_folder_uri;
	const gchar *existing_message_uid;

	editor = e_msg_composer_get_editor (composer);
	action = e_html_editor_get_action (editor, "template-replace");
	if (!action)
		return;

	existing_folder_uri = e_msg_composer_get_header (composer, "X-Evolution-Templates-Folder", 0);
	existing_message_uid = e_msg_composer_get_header (composer, "X-Evolution-Templates-Message", 0);

	e_ui_action_set_visible (action, existing_folder_uri && *existing_folder_uri && existing_message_uid && *existing_message_uid);
	e_ui_action_set_sensitive (action, e_ui_action_get_visible (action));
}

static void
templates_update_menu (TemplatesData *td)
{
	EMailReader *mail_reader;

	g_return_if_fail (td != NULL);

	td->changed = FALSE;

	mail_reader = g_weak_ref_get (&td->mail_reader_weakref);

	if (mail_reader) {
		e_mail_templates_store_update_menu (td->templates_store, td->reply_template_menu, e_mail_reader_get_ui_manager (mail_reader),
			action_reply_with_template_cb, mail_reader);

		g_clear_object (&mail_reader);
	}
}

static void
templates_mail_reader_update_actions_cb (EMailReader *reader,
					 guint state,
					 gpointer user_data)
{
	TemplatesData *td;
	gboolean sensitive;

	td = g_object_get_data (G_OBJECT (reader), TEMPLATES_DATA_KEY);
	if (td && td->changed)
		templates_update_menu (td);

	sensitive = (state & E_MAIL_READER_SELECTION_SINGLE) != 0;

	e_ui_action_set_sensitive (e_mail_reader_get_action (reader, "EPluginTemplates::mail-reply-template"), sensitive);
	e_ui_action_set_sensitive (e_mail_reader_get_action (reader, "template-use-this"), sensitive);
}

static void
init_composer_actions (EUIManager *ui_manager,
                       EMsgComposer *composer)
{
	static const gchar *eui =
		"<eui>"
		  "<menu id='main-menu'>"
		    "<placeholder id='pre-edit-menu'>"
		      "<submenu action='file-menu'>"
			"<placeholder id='template-holder'>"
			  "<item action='template-replace'/>"
			  "<item action='template-save-new'/>"
			"</placeholder>"
                      "</submenu>"
		    "</placeholder>"
		  "</menu>"
		"</eui>";

	static const EUIActionEntry entries[] = {
		{ "template-replace",
		  "document-save",
		  N_("Save _Template"),
		  "<Shift><Control>t",
		  N_("Replace opened Template message"),
		  action_template_replace_cb, NULL, NULL, NULL },

		{ "template-save-new",
		  "document-save",
		  N_("Save as _New Template"),
		  NULL,
		  N_("Save as Template"),
		  action_template_save_new_cb, NULL, NULL, NULL }
	};

	e_ui_manager_add_actions_with_eui_data (ui_manager, "composer", GETTEXT_PACKAGE,
		entries, G_N_ELEMENTS (entries), composer, eui);

	g_signal_connect (composer, "realize",
		G_CALLBACK (templates_composer_realize_cb), NULL);
}

static gboolean
templates_update_menu_timeout_cb (gpointer user_data)
{
	TemplatesData *td = user_data;

	td->update_menu_id = 0;

	templates_update_menu (td);

	return G_SOURCE_REMOVE;
}

static void
templates_store_changed_cb (EMailTemplatesStore *templates_store,
			    gpointer user_data)
{
	TemplatesData *td = user_data;

	g_return_if_fail (td != NULL);

	td->changed = TRUE;

	if (td->update_immediately && !td->update_menu_id)
		td->update_menu_id = g_timeout_add (100, templates_update_menu_timeout_cb, td);
}

static gboolean
templates_ui_manager_create_item_cb (EUIManager *ui_manager,
				     EUIElement *elem,
				     EUIAction *action,
				     EUIElementKind for_kind,
				     GObject **out_item,
				     gpointer user_data)
{
	GMenuModel *reply_template_menu = user_data;
	const gchar *name;

	g_return_val_if_fail (G_IS_MENU (reply_template_menu), FALSE);

	name = g_action_get_name (G_ACTION (action));

	if (!g_str_has_prefix (name, "EPluginTemplates::"))
		return FALSE;

	#define is_action(_nm) (g_strcmp0 (name, (_nm)) == 0)

	if (is_action ("EPluginTemplates::mail-reply-template")) {
		*out_item = e_ui_manager_create_item_from_menu_model (ui_manager, elem, action, for_kind, reply_template_menu);
	} else if (for_kind == E_UI_ELEMENT_KIND_MENU) {
		g_warning ("%s: Unhandled menu action '%s'", G_STRFUNC, name);
	} else if (for_kind == E_UI_ELEMENT_KIND_TOOLBAR) {
		g_warning ("%s: Unhandled toolbar action '%s'", G_STRFUNC, name);
	} else if (for_kind == E_UI_ELEMENT_KIND_HEADERBAR) {
		g_warning ("%s: Unhandled headerbar action '%s'", G_STRFUNC, name);
	} else {
		g_warning ("%s: Unhandled element kind '%d' for action '%s'", G_STRFUNC, (gint) for_kind, name);
	}

	#undef is_action

	return TRUE;
}

static void
init_actions_for_mail_backend (EMailBackend *mail_backend,
			       EUIManager *ui_manager,
			       EMailReader *mail_reader,
			       gboolean update_immediately)
{
	static const gchar *eui =
		"<eui>"
		  "<menu id='main-menu'>"
		    "<placeholder id='custom-menus'>"
		      "<submenu action='mail-message-menu'>"
			"<placeholder id='mail-reply-template'>"
			  "<item action='EPluginTemplates::mail-reply-template'/>"
			"</placeholder>"
                      "</submenu>"
		    "</placeholder>"
		  "</menu>"
		  "<menu id='mail-message-popup' is-popup='true'>"
		    "<placeholder id='mail-message-popup-common-actions'>"
		      "<placeholder id='mail-reply-template'>"
			"<item action='EPluginTemplates::mail-reply-template'/>"
		      "</placeholder>"
		    "</placeholder>"
		  "</menu>"
		  "<menu id='mail-preview-popup' is-popup='true'>"
		    "<placeholder id='mail-reply-template'>"
		      "<item action='EPluginTemplates::mail-reply-template'/>"
		    "</placeholder>"
		  "</menu>"
		  "<menu id='mail-reply-group-menu'>"
		    "<placeholder id='mail-reply-template'>"
		      "<item action='EPluginTemplates::mail-reply-template'/>"
		    "</placeholder>"
		  "</menu>"
		"</eui>";

	static const EUIActionEntry entries[] = {
		{ "EPluginTemplates::mail-reply-template", NULL, N_("Repl_y with Template"), NULL, NULL, NULL, NULL, NULL, NULL }
	};

	EMailSession *session;
	TemplatesData *td;

	session = e_mail_backend_get_session (mail_backend);

	td = g_new0 (TemplatesData, 1);
	td->templates_store = e_mail_templates_store_ref_default (e_mail_ui_session_get_account_store (E_MAIL_UI_SESSION (session)));
	g_weak_ref_init (&td->mail_reader_weakref, mail_reader);
	td->reply_template_menu = g_menu_new ();
	td->changed_handler_id = g_signal_connect (td->templates_store, "changed", G_CALLBACK (templates_store_changed_cb), td);
	td->changed = TRUE;
	td->update_immediately = update_immediately;

	g_object_set_data_full (G_OBJECT (mail_reader), TEMPLATES_DATA_KEY, td, templates_data_free);

	g_signal_connect_data (ui_manager, "create-item",
		G_CALLBACK (templates_ui_manager_create_item_cb), g_object_ref (td->reply_template_menu),
		(GClosureNotify) g_object_unref, 0);

	e_ui_manager_add_actions_with_eui_data (ui_manager, "templates", NULL,
		entries, G_N_ELEMENTS (entries), td, eui);

	templates_update_menu (td);
}

static void
init_mail_browser_actions (EUIManager *ui_manager,
			   EMailBrowser *mail_browser)
{
	EMailReader *reader = E_MAIL_READER (mail_browser);

	init_actions_for_mail_backend (e_mail_reader_get_backend (reader), ui_manager, reader, TRUE);
}

/* -------------------------------------------------------------------  */
/*                     EExtension types                                  */
/* -------------------------------------------------------------------  */

typedef struct _ETemplatesMailPanedView ETemplatesMailPanedView;
typedef struct _ETemplatesMailPanedViewClass ETemplatesMailPanedViewClass;
struct _ETemplatesMailPanedView { EExtension parent; };
struct _ETemplatesMailPanedViewClass { EExtensionClass parent_class; };

typedef struct _ETemplatesComposer ETemplatesComposer;
typedef struct _ETemplatesComposerClass ETemplatesComposerClass;
struct _ETemplatesComposer { EExtension parent; };
struct _ETemplatesComposerClass { EExtensionClass parent_class; };

typedef struct _ETemplatesBrowser ETemplatesBrowser;
typedef struct _ETemplatesBrowserClass ETemplatesBrowserClass;
struct _ETemplatesBrowser { EExtension parent; };
struct _ETemplatesBrowserClass { EExtensionClass parent_class; };

GType e_templates_mail_paned_view_get_type (void);
G_DEFINE_DYNAMIC_TYPE (ETemplatesMailPanedView, e_templates_mail_paned_view, E_TYPE_EXTENSION)
GType e_templates_composer_get_type (void);
G_DEFINE_DYNAMIC_TYPE (ETemplatesComposer, e_templates_composer, E_TYPE_EXTENSION)
GType e_templates_browser_get_type (void);
G_DEFINE_DYNAMIC_TYPE (ETemplatesBrowser, e_templates_browser, E_TYPE_EXTENSION)

static void
e_templates_mail_paned_view_constructed (GObject *object)
{
	EMailView *mail_view;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EUIManager *ui_manager;

	G_OBJECT_CLASS (e_templates_mail_paned_view_parent_class)->constructed (object);

	mail_view = E_MAIL_VIEW (e_extension_get_extensible (E_EXTENSION (object)));
	shell_view = e_mail_view_get_shell_view (mail_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	ui_manager = e_mail_reader_get_ui_manager (E_MAIL_READER (mail_view));

	init_actions_for_mail_backend (E_MAIL_BACKEND (shell_backend), ui_manager, E_MAIL_READER (mail_view), FALSE);

	g_signal_connect (
		mail_view, "update-actions",
		G_CALLBACK (templates_mail_reader_update_actions_cb), NULL);
}

static void
e_templates_mail_paned_view_class_init (ETemplatesMailPanedViewClass *class)
{
	G_OBJECT_CLASS (class)->constructed = e_templates_mail_paned_view_constructed;
	E_EXTENSION_CLASS (class)->extensible_type = E_TYPE_MAIL_PANED_VIEW;
}

static void e_templates_mail_paned_view_class_finalize (ETemplatesMailPanedViewClass *class) { }
static void e_templates_mail_paned_view_init (ETemplatesMailPanedView *self) { }

static void
e_templates_composer_constructed (GObject *object)
{
	EMsgComposer *composer;
	EHTMLEditor *editor;
	EUIManager *ui_manager;

	G_OBJECT_CLASS (e_templates_composer_parent_class)->constructed (object);

	composer = E_MSG_COMPOSER (e_extension_get_extensible (E_EXTENSION (object)));
	editor = e_msg_composer_get_editor (composer);
	ui_manager = e_html_editor_get_ui_manager (editor);

	init_composer_actions (ui_manager, composer);
}

static void
e_templates_composer_class_init (ETemplatesComposerClass *class)
{
	G_OBJECT_CLASS (class)->constructed = e_templates_composer_constructed;
	E_EXTENSION_CLASS (class)->extensible_type = E_TYPE_MSG_COMPOSER;
}

static void e_templates_composer_class_finalize (ETemplatesComposerClass *class) { }
static void e_templates_composer_init (ETemplatesComposer *self) { }

static void
e_templates_browser_constructed (GObject *object)
{
	EMailBrowser *browser;
	EUIManager *ui_manager;

	G_OBJECT_CLASS (e_templates_browser_parent_class)->constructed (object);

	browser = E_MAIL_BROWSER (e_extension_get_extensible (E_EXTENSION (object)));
	ui_manager = e_mail_reader_get_ui_manager (E_MAIL_READER (browser));

	init_mail_browser_actions (ui_manager, browser);
}

static void
e_templates_browser_class_init (ETemplatesBrowserClass *class)
{
	G_OBJECT_CLASS (class)->constructed = e_templates_browser_constructed;
	E_EXTENSION_CLASS (class)->extensible_type = E_TYPE_MAIL_BROWSER;
}

static void e_templates_browser_class_finalize (ETemplatesBrowserClass *class) { }
static void e_templates_browser_init (ETemplatesBrowser *self) { }

void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_templates_mail_paned_view_register_type (type_module);
	e_templates_composer_register_type (type_module);
	e_templates_browser_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
