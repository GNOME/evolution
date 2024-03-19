/*
 * e-mail-attachment-handler.c
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-mail-attachment-handler.h"

#include <glib/gi18n.h>

#include "mail/e-mail-backend.h"
#include "mail/e-mail-reader.h"
#include "mail/em-composer-utils.h"
#include "mail/em-utils.h"

struct _EMailAttachmentHandlerPrivate {
	EMailBackend *backend;
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EMailAttachmentHandler, e_mail_attachment_handler, E_TYPE_ATTACHMENT_HANDLER, 0,
	G_ADD_PRIVATE_DYNAMIC (EMailAttachmentHandler))

static const gchar *ui =
"<ui>"
"  <popup name='context'>"
"    <placeholder name='custom-actions'>"
"      <menuitem action='mail-import-pgp-key'/>"
"      <separator/>"
"      <menuitem action='mail-message-edit'/>"
"      <separator/>"
"      <menuitem action='mail-reply-sender'/>"
"      <menuitem action='mail-reply-list'/>"
"      <menuitem action='mail-reply-all'/>"
"      <menuitem action='mail-forward'/>"
"      <menu action='mail-forward-as-menu'>"
"        <menuitem action='mail-forward-attached'/>"
"        <menuitem action='mail-forward-inline'/>"
"        <menuitem action='mail-forward-quoted'/>"
"        <separator/>"
"        <menuitem action='mail-redirect'/>"
"      </menu>"
"    </placeholder>"
"  </popup>"
"</ui>";

/* Note: Do not use the info field. */
static GtkTargetEntry target_table[] = {
	{ (gchar *) "message/rfc822",	0, 0 },
	{ (gchar *) "x-uid-list",	0, 0 }
};

static CamelFolder *
mail_attachment_handler_guess_folder_ref (EAttachmentHandler *handler)
{
	EAttachmentView *view;
	GtkWidget *widget;

	view = e_attachment_handler_get_view (handler);

	if (!view || !GTK_IS_WIDGET (view))
		return NULL;

	widget = GTK_WIDGET (view);
	while (widget) {
		if (E_IS_MAIL_READER (widget)) {
			return e_mail_reader_ref_folder (E_MAIL_READER (widget));
		}

		widget = gtk_widget_get_parent (widget);
	}

	return NULL;
}

static CamelMimeMessage *
mail_attachment_handler_get_selected_message (EAttachmentHandler *handler)
{
	EAttachment *attachment;
	EAttachmentView *view;
	CamelMimePart *mime_part;
	CamelMimeMessage *message = NULL;
	CamelDataWrapper *outer_wrapper;
	CamelContentType *outer_content_type;
	CamelDataWrapper *inner_wrapper;
	CamelContentType *inner_content_type;
	GList *selected;
	gboolean inner_and_outer_content_types_match;

	view = e_attachment_handler_get_view (handler);

	selected = e_attachment_view_get_selected_attachments (view);
	g_return_val_if_fail (g_list_length (selected) == 1, NULL);

	attachment = E_ATTACHMENT (selected->data);
	mime_part = e_attachment_ref_mime_part (attachment);

	outer_wrapper =
		camel_medium_get_content (CAMEL_MEDIUM (mime_part));
	outer_content_type =
		camel_data_wrapper_get_mime_type_field (outer_wrapper);

	if (!camel_content_type_is (outer_content_type, "message", "rfc822"))
		goto exit;

	inner_wrapper =
		camel_medium_get_content (CAMEL_MEDIUM (outer_wrapper));
	inner_content_type =
		camel_data_wrapper_get_mime_type_field (inner_wrapper);

	inner_and_outer_content_types_match =
		camel_content_type_is (
			inner_content_type,
			outer_content_type->type,
			outer_content_type->subtype);

	if (!inner_and_outer_content_types_match) {
		CamelStream *mem;
		gboolean success;

		/* Create a message copy in case the inner content
		 * type doesn't match the mime_part's content type,
		 * which can happen for multipart/digest, where it
		 * confuses the formatter on reply, which skips all
		 * rfc822 subparts. */
		mem = camel_stream_mem_new ();
		camel_data_wrapper_write_to_stream_sync (
			CAMEL_DATA_WRAPPER (outer_wrapper), mem, NULL, NULL);

		g_seekable_seek (
			G_SEEKABLE (mem), 0, G_SEEK_SET, NULL, NULL);
		message = camel_mime_message_new ();
		success = camel_data_wrapper_construct_from_stream_sync (
			CAMEL_DATA_WRAPPER (message), mem, NULL, NULL);
		if (!success)
			g_clear_object (&message);

		g_object_unref (mem);
	}

exit:
	if (message == NULL)
		message = CAMEL_MIME_MESSAGE (g_object_ref (outer_wrapper));

	g_clear_object (&mime_part);

	g_list_free_full (selected, (GDestroyNotify) g_object_unref);

	return message;
}

typedef struct _CreateComposerData {
	CamelMimeMessage *message;
	CamelFolder *folder;
	gboolean is_redirect;

	gboolean is_reply;
	EMailReplyType reply_type;

	gboolean is_forward;
	EMailForwardStyle forward_style;
} CreateComposerData;

static void
create_composer_data_free (CreateComposerData *ccd)
{
	if (ccd) {
		g_clear_object (&ccd->message);
		g_clear_object (&ccd->folder);
		g_slice_free (CreateComposerData, ccd);
	}
}

static void
mail_attachment_handler_composer_created_cb (GObject *source_object,
					     GAsyncResult *result,
					     gpointer user_data)
{
	CreateComposerData *ccd = user_data;
	EMsgComposer *composer;
	GError *error = NULL;

	g_return_if_fail (ccd != NULL);

	composer = e_msg_composer_new_finish (result, &error);
	if (error) {
		g_warning ("%s: Failed to create msg composer: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
	} else {
		if (ccd->is_redirect) {
			em_utils_redirect_message (composer, ccd->message);
		} else if (ccd->is_reply) {
			GSettings *settings;
			EMailReplyStyle style;

			settings = e_util_ref_settings ("org.gnome.evolution.mail");
			style = g_settings_get_enum (settings, "reply-style-name");
			g_object_unref (settings);

			em_utils_reply_to_message (composer, ccd->message, NULL, NULL, ccd->reply_type, style, NULL, NULL, E_MAIL_REPLY_FLAG_NONE);
		} else if (ccd->is_forward) {
			em_utils_forward_message (composer, ccd->message, ccd->forward_style, ccd->folder, NULL, FALSE);
		} else {
			em_utils_edit_message (composer, ccd->folder, ccd->message, NULL, TRUE, FALSE);
		}
	}

	create_composer_data_free (ccd);
}

static void
mail_attachment_handler_forward_with_style (EAttachmentHandler *handler,
					    EMailForwardStyle style)
{
	EMailAttachmentHandler *self = E_MAIL_ATTACHMENT_HANDLER (handler);
	CamelMimeMessage *message;
	CamelFolder *folder;
	CreateComposerData *ccd;
	EShell *shell;

	message = mail_attachment_handler_get_selected_message (handler);
	g_return_if_fail (message != NULL);

	folder = mail_attachment_handler_guess_folder_ref (handler);
	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (self->priv->backend));

	ccd = g_slice_new0 (CreateComposerData);
	ccd->message = message;
	ccd->folder = folder;
	ccd->is_forward = TRUE;
	ccd->forward_style = style;

	e_msg_composer_new (shell, mail_attachment_handler_composer_created_cb, ccd);
}

static void
mail_attachment_handler_forward (GtkAction *action,
                                 EAttachmentHandler *handler)
{
	GSettings *settings;
	EMailForwardStyle style;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	style = g_settings_get_enum (settings, "forward-style-name");
	g_object_unref (settings);

	mail_attachment_handler_forward_with_style (handler, style);
}

static void
mail_attachment_handler_reply (EAttachmentHandler *handler,
                               EMailReplyType reply_type)
{
	EMailAttachmentHandler *self = E_MAIL_ATTACHMENT_HANDLER (handler);
	CamelMimeMessage *message;
	CreateComposerData *ccd;
	EShellBackend *shell_backend;
	EShell *shell;

	message = mail_attachment_handler_get_selected_message (handler);
	g_return_if_fail (message != NULL);

	shell_backend = E_SHELL_BACKEND (self->priv->backend);
	shell = e_shell_backend_get_shell (shell_backend);

	ccd = g_slice_new0 (CreateComposerData);
	ccd->message = message;
	ccd->reply_type = reply_type;
	ccd->is_reply = TRUE;

	e_msg_composer_new (shell, mail_attachment_handler_composer_created_cb, ccd);
}

static void
mail_attachment_handler_reply_all (GtkAction *action,
                                   EAttachmentHandler *handler)
{
	mail_attachment_handler_reply (handler, E_MAIL_REPLY_TO_ALL);
}

static void
mail_attachment_handler_reply_list (GtkAction *action,
                                    EAttachmentHandler *handler)
{
	mail_attachment_handler_reply (handler, E_MAIL_REPLY_TO_LIST);
}

static void
mail_attachment_handler_reply_sender (GtkAction *action,
                                      EAttachmentHandler *handler)
{
	mail_attachment_handler_reply (handler, E_MAIL_REPLY_TO_SENDER);
}

static void
mail_attachment_handler_message_edit (GtkAction *action,
				      EAttachmentHandler *handler)
{
	EMailAttachmentHandler *self = E_MAIL_ATTACHMENT_HANDLER (handler);
	CamelMimeMessage *message;
	CamelFolder *folder;
	CreateComposerData *ccd;
	EShell *shell;

	message = mail_attachment_handler_get_selected_message (handler);
	g_return_if_fail (message != NULL);

	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (self->priv->backend));
	folder = mail_attachment_handler_guess_folder_ref (handler);

	ccd = g_slice_new0 (CreateComposerData);
	ccd->message = message;
	ccd->folder = folder;

	e_msg_composer_new (shell, mail_attachment_handler_composer_created_cb, ccd);
}

static void
mail_attachment_handler_forward_attached (GtkAction *action,
					  EAttachmentHandler *handler)
{
	mail_attachment_handler_forward_with_style (handler, E_MAIL_FORWARD_STYLE_ATTACHED);
}
static void
mail_attachment_handler_forward_inline (GtkAction *action,
					     EAttachmentHandler *handler)
{
	mail_attachment_handler_forward_with_style (handler, E_MAIL_FORWARD_STYLE_INLINE);
}

static void
mail_attachment_handler_forward_quoted (GtkAction *action,
					EAttachmentHandler *handler)
{
	mail_attachment_handler_forward_with_style (handler, E_MAIL_FORWARD_STYLE_QUOTED);
}

static void
mail_attachment_handler_redirect (GtkAction *action,
				  EAttachmentHandler *handler)
{
	EMailAttachmentHandler *self = E_MAIL_ATTACHMENT_HANDLER (handler);
	CamelMimeMessage *message;
	CreateComposerData *ccd;
	EShell *shell;

	message = mail_attachment_handler_get_selected_message (handler);
	g_return_if_fail (message != NULL);

	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (self->priv->backend));

	ccd = g_slice_new0 (CreateComposerData);
	ccd->message = message;
	ccd->folder = NULL;
	ccd->is_redirect = TRUE;

	e_msg_composer_new (shell, mail_attachment_handler_composer_created_cb, ccd);
}

static void
action_mail_import_pgp_key_cb (GtkAction *action,
			       EAttachmentHandler *handler)
{
	EAttachmentView *view;
	EAttachment *attachment;
	EAttachmentStore *store;
	CamelMimePart *part;
	GtkTreePath *path;
	GtkTreeIter iter;
	GList *list;
	gpointer parent_window;

	view = e_attachment_handler_get_view (handler);
	parent_window = gtk_widget_get_toplevel (GTK_WIDGET (view));
	parent_window = gtk_widget_is_toplevel (parent_window) ? parent_window : NULL;

	list = e_attachment_view_get_selected_paths (view);
	g_return_if_fail (g_list_length (list) == 1);
	path = list->data;

	store = e_attachment_view_get_store (view);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, E_ATTACHMENT_STORE_COLUMN_ATTACHMENT, &attachment, -1);

	g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);

	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	part = e_attachment_ref_mime_part (attachment);

	if (part) {
		CamelMimePart *mime_part;
		CamelSession *session;
		CamelStream *stream;
		GByteArray *buffer;
		GError *error = NULL;

		session = CAMEL_SESSION (e_mail_backend_get_session (E_MAIL_ATTACHMENT_HANDLER (handler)->priv->backend));
		mime_part = e_attachment_ref_mime_part (attachment);

		buffer = g_byte_array_new ();
		stream = camel_stream_mem_new ();
		camel_stream_mem_set_byte_array (CAMEL_STREAM_MEM (stream), buffer);
		camel_data_wrapper_decode_to_stream_sync (camel_medium_get_content (CAMEL_MEDIUM (mime_part)), stream, NULL, NULL);
		g_object_unref (stream);

		if (!em_utils_import_pgp_key (parent_window, session, buffer->data, buffer->len, &error) &&
		    !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			GtkWidget *parent, *alert_sink = NULL;

			for (parent = gtk_widget_get_parent (GTK_WIDGET (view)); parent && !alert_sink; parent = gtk_widget_get_parent (parent)) {
				if (E_IS_ALERT_SINK (parent))
					alert_sink = parent;
			}

			if (alert_sink)
				e_alert_submit (E_ALERT_SINK (alert_sink), "mail:error-import-pgp-key", error ? error->message : _("Unknown error"), NULL);
			else
				g_warning ("Failed to import PGP key: %s", error ? error->message : "Unknown error");
		}

		g_byte_array_unref (buffer);
		g_clear_error (&error);
	}

	g_clear_object (&part);
}

static GtkActionEntry standard_entries[] = {

	{ "mail-forward",
	  "mail-forward",
	  N_("_Forward"),
	  NULL,
	  N_("Forward the selected message to someone"),
	  G_CALLBACK (mail_attachment_handler_forward) },

	{ "mail-reply-all",
	  "mail-reply-all",
	  N_("Reply to _All"),
	  NULL,
	  N_("Compose a reply to all the recipients of the selected message"),
	  G_CALLBACK (mail_attachment_handler_reply_all) },

	{ "mail-reply-list",
	  NULL,
	  N_("Reply to _List"),
	  NULL,
	  N_("Compose a reply to the mailing list of the selected message"),
	  G_CALLBACK (mail_attachment_handler_reply_list) },

	{ "mail-reply-sender",
	  "mail-reply-sender",
	  N_("_Reply to Sender"),
	  NULL,
	  N_("Compose a reply to the sender of the selected message"),
	  G_CALLBACK (mail_attachment_handler_reply_sender) },

	{ "mail-message-edit",
	  NULL,
	  N_("_Edit as New Message…"),
	  NULL,
	  N_("Open the selected messages in the composer for editing"),
	  G_CALLBACK (mail_attachment_handler_message_edit) },

	{ "mail-forward-as-menu",
	  NULL,
	  N_("F_orward As"),
	  NULL,
	  NULL,
	  NULL },

	{ "mail-forward-attached",
	  NULL,
	  N_("_Attached"),
	  NULL,
	  N_("Forward the selected message to someone as an attachment"),
	  G_CALLBACK (mail_attachment_handler_forward_attached) },

	{ "mail-forward-inline",
	  NULL,
	  N_("_Inline"),
	  NULL,
	  N_("Forward the selected message in the body of a new message"),
	  G_CALLBACK (mail_attachment_handler_forward_inline) },

	{ "mail-forward-quoted",
	  NULL,
	  N_("_Quoted"),
	  NULL,
	  N_("Forward the selected message quoted like a reply"),
	  G_CALLBACK (mail_attachment_handler_forward_quoted) },

	{ "mail-redirect",
	  NULL,
	  N_("Re_direct"),
	  NULL,
	  N_("Redirect (bounce) the selected message to someone"),
	  G_CALLBACK (mail_attachment_handler_redirect) }
};

static GtkActionEntry custom_entries[] = {

	{ "mail-import-pgp-key",
	  "stock_signature",
	  N_("Import OpenP_GP key…"),
	  NULL,
	  N_("Import Pretty Good Privacy (OpenPGP) key"),
	  G_CALLBACK (action_mail_import_pgp_key_cb) }
};

static void
call_attachment_load_handle_error (GObject *source_object,
				   GAsyncResult *result,
				   gpointer user_data)
{
	GtkWindow *window = user_data;

	g_return_if_fail (E_IS_ATTACHMENT (source_object));
	g_return_if_fail (!window || GTK_IS_WINDOW (window));

	e_attachment_load_handle_error (E_ATTACHMENT (source_object), result, window);

	g_clear_object (&window);
}

static void
mail_attachment_handler_message_rfc822 (EAttachmentView *view,
                                        GdkDragContext *drag_context,
                                        gint x,
                                        gint y,
                                        GtkSelectionData *selection_data,
                                        guint info,
                                        guint time,
                                        EAttachmentHandler *handler)
{
	static GdkAtom atom = GDK_NONE;
	EAttachmentStore *store;
	EAttachment *attachment;
	CamelMimeMessage *message;
	CamelDataWrapper *wrapper;
	CamelStream *stream;
	const gchar *data;
	gboolean success = FALSE;
	gpointer parent;
	gint length;

	if (G_UNLIKELY (atom == GDK_NONE))
		atom = gdk_atom_intern_static_string ("message/rfc822");

	if (gtk_selection_data_get_target (selection_data) != atom)
		return;

	g_signal_stop_emission_by_name (view, "drag-data-received");

	data = (const gchar *) gtk_selection_data_get_data (selection_data);
	length = gtk_selection_data_get_length (selection_data);

	stream = camel_stream_mem_new ();
	camel_stream_write (stream, data, length, NULL, NULL);
	g_seekable_seek (G_SEEKABLE (stream), 0, G_SEEK_SET, NULL, NULL);

	message = camel_mime_message_new ();
	wrapper = CAMEL_DATA_WRAPPER (message);

	if (!camel_data_wrapper_construct_from_stream_sync (
		wrapper, stream, NULL, NULL))
		goto exit;

	store = e_attachment_view_get_store (view);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (view));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	attachment = e_attachment_new_for_message (message);
	e_attachment_store_add_attachment (store, attachment);
	e_attachment_load_async (
		attachment, (GAsyncReadyCallback)
		call_attachment_load_handle_error, parent ? g_object_ref (parent) : NULL);
	g_object_unref (attachment);

	success = TRUE;

exit:
	g_object_unref (message);
	g_object_unref (stream);

	gtk_drag_finish (drag_context, success, FALSE, time);
}

static gboolean
gather_x_uid_list_messages_cb (CamelFolder *folder,
			       const GPtrArray *uids,
			       gpointer user_data,
			       GCancellable *cancellable,
			       GError **error)
{
	GSList **pmessages = user_data;
	guint ii;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);
	g_return_val_if_fail (uids != NULL, FALSE);
	g_return_val_if_fail (pmessages != NULL, FALSE);

	for (ii = 0; ii < uids->len; ii++) {
		CamelMimeMessage *message;

		message = camel_folder_get_message_sync (folder, uids->pdata[ii], cancellable, error);
		if (!message)
			return FALSE;

		*pmessages = g_slist_prepend (*pmessages, message);
	}

	return TRUE;
}

static void
mail_attachment_handler_x_uid_list (EAttachmentView *view,
                                    GdkDragContext *drag_context,
                                    gint x,
                                    gint y,
                                    GtkSelectionData *selection_data,
                                    guint info,
                                    guint time,
                                    EAttachmentHandler *handler)
{
	static GdkAtom atom = GDK_NONE;
	EMailAttachmentHandler *self = E_MAIL_ATTACHMENT_HANDLER (handler);
	CamelDataWrapper *wrapper;
	CamelMultipart *multipart;
	CamelMimePart *mime_part;
	EAttachment *attachment;
	EAttachmentStore *store;
	EMailSession *session;
	GSList *messages = NULL, *link;
	gchar *description;
	gpointer parent;
	GError *local_error = NULL;

	if (G_UNLIKELY (atom == GDK_NONE))
		atom = gdk_atom_intern_static_string ("x-uid-list");

	if (gtk_selection_data_get_target (selection_data) != atom)
		return;

	store = e_attachment_view_get_store (view);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (view));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	session = e_mail_backend_get_session (self->priv->backend);

	em_utils_selection_uidlist_foreach_sync (selection_data, session,
		gather_x_uid_list_messages_cb, &messages, NULL, &local_error);

	if (local_error || !messages)
		goto exit;

	/* Handle one message. */
	if (!messages->next) {
		attachment = e_attachment_new_for_message (messages->data);
		e_attachment_store_add_attachment (store, attachment);
		e_attachment_load_async (
			attachment, (GAsyncReadyCallback)
			call_attachment_load_handle_error, parent ? g_object_ref (parent) : NULL);

		g_object_unref (attachment);
	} else {
		GSettings *settings;

		messages = g_slist_reverse (messages);
		settings = e_util_ref_settings ("org.gnome.evolution.mail");

		if (g_settings_get_boolean (settings, "composer-attach-separate-messages")) {
			for (link = messages; link; link = g_slist_next (link)) {
				CamelMimeMessage *message = link->data;

				mime_part = mail_tool_make_message_attachment (message);
				/* remove it, no need to have "Forwarded Message - $SUBJECT" there */
				camel_medium_remove_header (CAMEL_MEDIUM (mime_part), "Content-Description");
				attachment = e_attachment_new ();
				e_attachment_set_mime_part (attachment, mime_part);
				e_attachment_store_add_attachment (store, attachment);
				e_attachment_load_async (
					attachment, (GAsyncReadyCallback)
					call_attachment_load_handle_error, parent ? g_object_ref (parent) : NULL);
				g_object_unref (attachment);
				g_object_unref (mime_part);
			}
		} else {
			gint n_messages = g_slist_length (messages);

			/* Build a multipart/digest message out of the UIDs. */
			multipart = camel_multipart_new ();
			wrapper = CAMEL_DATA_WRAPPER (multipart);
			camel_data_wrapper_set_mime_type (wrapper, "multipart/digest");
			camel_multipart_set_boundary (multipart, NULL);

			for (link = messages; link; link = g_slist_next (link)) {
				mime_part = camel_mime_part_new ();
				wrapper = CAMEL_DATA_WRAPPER (link->data);
				camel_mime_part_set_disposition (mime_part, "inline");
				camel_medium_set_content (
					CAMEL_MEDIUM (mime_part), wrapper);
				camel_mime_part_set_content_type (mime_part, "message/rfc822");
				camel_multipart_add_part (multipart, mime_part);
				g_object_unref (mime_part);
			}

			mime_part = camel_mime_part_new ();
			wrapper = CAMEL_DATA_WRAPPER (multipart);
			camel_medium_set_content (CAMEL_MEDIUM (mime_part), wrapper);

			description = g_strdup_printf (
				ngettext (
					"%d attached message",
					"%d attached messages",
					n_messages),
				n_messages);
			camel_mime_part_set_description (mime_part, description);
			g_free (description);

			attachment = e_attachment_new ();
			e_attachment_set_mime_part (attachment, mime_part);
			e_attachment_store_add_attachment (store, attachment);
			e_attachment_load_async (
				attachment, (GAsyncReadyCallback)
				call_attachment_load_handle_error, parent ? g_object_ref (parent) : NULL);
			g_object_unref (attachment);

			g_object_unref (mime_part);
			g_object_unref (multipart);
		}

		g_clear_object (&settings);
	}

 exit:
	if (local_error != NULL) {
		const gchar *folder_name = (const gchar *) gtk_selection_data_get_data (selection_data);

		e_alert_run_dialog_for_args (
			parent, "mail-composer:attach-nomessages",
			folder_name, local_error->message, NULL);

		g_clear_error (&local_error);
	}

	g_slist_free_full (messages, g_object_unref);

	g_signal_stop_emission_by_name (view, "drag-data-received");
}

static void
mail_attachment_handler_update_actions (EAttachmentView *view,
                                        EAttachmentHandler *handler)
{
	EAttachment *attachment;
	CamelMimePart *mime_part;
	GtkActionGroup *action_group;
	GtkAction *action;
	GList *selected;
	gboolean visible = FALSE, has_list_post = FALSE, can_import_pgp_key = FALSE;

	selected = e_attachment_view_get_selected_attachments (view);

	if (g_list_length (selected) != 1)
		goto exit;

	attachment = E_ATTACHMENT (selected->data);

	if (e_attachment_get_loading (attachment) ||
	    e_attachment_get_saving (attachment))
		goto exit;

	mime_part = e_attachment_ref_mime_part (attachment);

	if (mime_part != NULL) {
		CamelMedium *medium;
		CamelDataWrapper *content;
		gchar *mime_type;

		medium = CAMEL_MEDIUM (mime_part);
		content = camel_medium_get_content (medium);
		visible = CAMEL_IS_MIME_MESSAGE (content);

		if (visible)
			has_list_post = camel_medium_get_header (CAMEL_MEDIUM (content), "List-Post") != NULL;

		mime_type = e_attachment_dup_mime_type (attachment);
		can_import_pgp_key = mime_type && g_ascii_strcasecmp (mime_type, "application/pgp-keys") == 0;

		g_clear_pointer (&mime_type, g_free);
		g_object_unref (mime_part);
	}

exit:
	action_group = e_attachment_view_get_action_group (view, "mail");
	gtk_action_group_set_visible (action_group, visible);

	action = gtk_action_group_get_action (action_group, "mail-reply-list");
	gtk_action_set_visible (action, has_list_post);

	action = e_attachment_view_get_action (view, "mail-import-pgp-key");
	gtk_action_set_visible (action, can_import_pgp_key);

	g_list_foreach (selected, (GFunc) g_object_unref, NULL);
	g_list_free (selected);
}

static void
mail_attachment_handler_dispose (GObject *object)
{
	EMailAttachmentHandler *self = E_MAIL_ATTACHMENT_HANDLER (object);

	g_clear_object (&self->priv->backend);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_attachment_handler_parent_class)->dispose (object);
}

static void
mail_attachment_handler_constructed (GObject *object)
{
	EMailAttachmentHandler *self = E_MAIL_ATTACHMENT_HANDLER (object);
	EShell *shell;
	EShellBackend *shell_backend;
	EAttachmentHandler *handler;
	EAttachmentView *view;
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	GError *error = NULL;

	handler = E_ATTACHMENT_HANDLER (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_attachment_handler_parent_class)->constructed (object);

	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (shell, "mail");
	self->priv->backend = E_MAIL_BACKEND (g_object_ref (shell_backend));

	view = e_attachment_handler_get_view (handler);

	action_group = e_attachment_view_add_action_group (view, "mail");
	gtk_action_group_add_actions (
		action_group, standard_entries,
		G_N_ELEMENTS (standard_entries), handler);

	action_group = e_attachment_view_add_action_group (view, "mail-custom");
	gtk_action_group_add_actions (
		action_group, custom_entries,
		G_N_ELEMENTS (custom_entries), handler);

	ui_manager = e_attachment_view_get_ui_manager (view);
	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	g_signal_connect (
		view, "update-actions",
		G_CALLBACK (mail_attachment_handler_update_actions),
		handler);

	g_signal_connect (
		view, "drag-data-received",
		G_CALLBACK (mail_attachment_handler_message_rfc822),
		handler);

	g_signal_connect (
		view, "drag-data-received",
		G_CALLBACK (mail_attachment_handler_x_uid_list),
		handler);
}

static GdkDragAction
mail_attachment_handler_get_drag_actions (EAttachmentHandler *handler)
{
	return GDK_ACTION_COPY;
}

static const GtkTargetEntry *
mail_attachment_handler_get_target_table (EAttachmentHandler *handler,
                                          guint *n_targets)
{
	if (n_targets != NULL)
		*n_targets = G_N_ELEMENTS (target_table);

	return target_table;
}

static void
e_mail_attachment_handler_class_init (EMailAttachmentHandlerClass *class)
{
	GObjectClass *object_class;
	EAttachmentHandlerClass *handler_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = mail_attachment_handler_dispose;
	object_class->constructed = mail_attachment_handler_constructed;

	handler_class = E_ATTACHMENT_HANDLER_CLASS (class);
	handler_class->get_drag_actions = mail_attachment_handler_get_drag_actions;
	handler_class->get_target_table = mail_attachment_handler_get_target_table;
}

static void
e_mail_attachment_handler_class_finalize (EMailAttachmentHandlerClass *klass)
{
}

static void
e_mail_attachment_handler_init (EMailAttachmentHandler *handler)
{
	handler->priv = e_mail_attachment_handler_get_instance_private (handler);
}

void
e_mail_attachment_handler_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_mail_attachment_handler_register_type (type_module);
}
