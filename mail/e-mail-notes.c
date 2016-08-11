/*
 * Copyright (C) 2015 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdio.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <camel/camel.h>
#include <e-util/e-util.h>
#include <libemail-engine/libemail-engine.h>

#include "e-mail-notes.h"

#define E_TYPE_MAIL_NOTES_EDITOR \
	(e_mail_notes_editor_get_type ())
#define E_MAIL_NOTES_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_NOTES_EDITOR, EMailNotesEditor))
#define E_IS_MAIL_NOTES_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_NOTES_EDITOR))

typedef struct _EMailNotesEditor EMailNotesEditor;
typedef struct _EMailNotesEditorClass EMailNotesEditorClass;

struct _EMailNotesEditor {
	GtkWindow parent;

	EHTMLEditor *editor; /* not referenced */
	EAttachmentPaned *attachment_paned; /* not referenced */
	EFocusTracker *focus_tracker;
	GtkActionGroup *action_group;

	gboolean had_message;
	CamelMimeMessage *message;
	CamelFolder *folder;
	gchar *uid;
};

struct _EMailNotesEditorClass {
	GtkWindowClass parent_class;
};

GType e_mail_notes_editor_get_type (void);

G_DEFINE_TYPE (EMailNotesEditor, e_mail_notes_editor, GTK_TYPE_WINDOW)

static gchar *
e_mail_notes_extract_text_content (CamelMimePart *part)
{
	CamelDataWrapper *content;
	CamelStream *stream;
	GByteArray *byte_array;
	gchar *text = NULL;

	g_return_val_if_fail (CAMEL_IS_MIME_PART (part), NULL);

	content = camel_medium_get_content (CAMEL_MEDIUM (part));
	g_return_val_if_fail (content != NULL, NULL);

	stream = camel_stream_mem_new ();
	camel_data_wrapper_decode_to_stream_sync (content, stream, NULL, NULL);
	camel_stream_close (stream, NULL, NULL);

	byte_array = camel_stream_mem_get_byte_array (CAMEL_STREAM_MEM (stream));

	if (byte_array->data)
		text = g_strndup ((const gchar *) byte_array->data, byte_array->len);

	g_object_unref (stream);

	return text;
}

static void
e_mail_notes_extract_text_from_multipart_alternative (EContentEditor *cnt_editor,
						      CamelMultipart *in_multipart)
{
	guint ii, nparts;

	g_return_if_fail (E_IS_CONTENT_EDITOR (cnt_editor));
	g_return_if_fail (CAMEL_IS_MULTIPART (in_multipart));

	nparts = camel_multipart_get_number (in_multipart);

	for (ii = 0; ii < nparts; ii++) {
		CamelMimePart *part;
		CamelContentType *ct;

		/* Traverse from the end, where the best format available is stored */
		part = camel_multipart_get_part (in_multipart, nparts - ii - 1);
		if (!part)
			continue;

		ct = camel_mime_part_get_content_type (part);
		if (!ct)
			continue;

		if (camel_content_type_is (ct, "text", "html")) {
			gchar *text;

			text = e_mail_notes_extract_text_content (part);
			if (text) {
				e_content_editor_set_html_mode (cnt_editor, TRUE);
				e_content_editor_insert_content (
					cnt_editor,
					text,
					E_CONTENT_EDITOR_INSERT_TEXT_HTML |
					E_CONTENT_EDITOR_INSERT_REPLACE_ALL);
				g_free (text);
				break;
			}
		} else if (camel_content_type_is (ct, "text", "plain")) {
			gchar *text;

			text = e_mail_notes_extract_text_content (part);
			if (text) {
				e_content_editor_insert_content (
					cnt_editor,
					text,
					E_CONTENT_EDITOR_INSERT_TEXT_PLAIN |
					E_CONTENT_EDITOR_INSERT_REPLACE_ALL);
				g_free (text);
			}
			break;
		}
	}
}

static void
e_mail_notes_editor_extract_text_from_multipart_related (EMailNotesEditor *notes_editor,
							 CamelMultipart *multipart)
{
	EContentEditor *cnt_editor;
	guint ii, nparts;

	g_return_if_fail (E_IS_MAIL_NOTES_EDITOR (notes_editor));
	g_return_if_fail (CAMEL_IS_MULTIPART (multipart));

	cnt_editor = e_html_editor_get_content_editor (notes_editor->editor);
	nparts = camel_multipart_get_number (multipart);

	for (ii = 0; ii < nparts; ii++) {
		CamelMimePart *part;
		CamelContentType *ct;
		CamelDataWrapper *content;

		part = camel_multipart_get_part (multipart, ii);
		if (!part)
			continue;

		ct = camel_mime_part_get_content_type (part);
		if (!ct)
			continue;

		if (camel_content_type_is (ct, "image", "*")) {
			e_content_editor_insert_image_from_mime_part (cnt_editor, part);
		} else if (camel_content_type_is (ct, "multipart", "alternative")) {
			content = camel_medium_get_content (CAMEL_MEDIUM (part));
			if (CAMEL_IS_MULTIPART (content))
				e_mail_notes_extract_text_from_multipart_alternative (cnt_editor, CAMEL_MULTIPART (content));
		}
	}
}

static void
e_mail_notes_editor_extract_text_from_part (EMailNotesEditor *notes_editor,
					    CamelMimePart *part)
{
	CamelContentType *ct;
	CamelDataWrapper *content;
	EContentEditor *cnt_editor;

	g_return_if_fail (E_IS_MAIL_NOTES_EDITOR (notes_editor));
	g_return_if_fail (CAMEL_IS_MIME_PART (part));

	content = camel_medium_get_content (CAMEL_MEDIUM (part));
	ct = camel_data_wrapper_get_mime_type_field (content);

	g_return_if_fail (content != NULL);
	g_return_if_fail (ct != NULL);

	cnt_editor = e_html_editor_get_content_editor (notes_editor->editor);

	if (camel_content_type_is (ct, "multipart", "related")) {
		g_return_if_fail (CAMEL_IS_MULTIPART (content));

		e_mail_notes_editor_extract_text_from_multipart_related (notes_editor, CAMEL_MULTIPART (content));
	} else if (camel_content_type_is (ct, "multipart", "alternative")) {
		if (CAMEL_IS_MULTIPART (content)) {
			e_mail_notes_extract_text_from_multipart_alternative (cnt_editor, CAMEL_MULTIPART (content));
		}
	} else if (camel_content_type_is (ct, "text", "plain")) {
		gchar *text;

		text = e_mail_notes_extract_text_content (part);
		if (text) {
			e_content_editor_insert_content (
				cnt_editor,
				text,
				E_CONTENT_EDITOR_INSERT_TEXT_PLAIN |
				E_CONTENT_EDITOR_INSERT_REPLACE_ALL);
			g_free (text);
		}
	}
}

static void
e_mail_notes_editor_extract_text_from_message (EMailNotesEditor *notes_editor,
					       CamelMimeMessage *message)
{
	CamelContentType *ct;
	CamelDataWrapper *content;
	EContentEditor *cnt_editor;

	g_return_if_fail (E_IS_MAIL_NOTES_EDITOR (notes_editor));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	content = camel_medium_get_content (CAMEL_MEDIUM (message));
	ct = camel_data_wrapper_get_mime_type_field (content);

	g_return_if_fail (content != NULL);
	g_return_if_fail (ct != NULL);

	cnt_editor = e_html_editor_get_content_editor (notes_editor->editor);

	if (camel_content_type_is (ct, "multipart", "mixed")) {
		EAttachmentStore *attachment_store;
		CamelMultipart *multipart;
		guint ii, nparts;

		g_return_if_fail (CAMEL_IS_MULTIPART (content));

		attachment_store = e_attachment_view_get_store (E_ATTACHMENT_VIEW (notes_editor->attachment_paned));
		multipart = CAMEL_MULTIPART (content);
		nparts = camel_multipart_get_number (multipart);

		/* The first part is the note text, the rest are attachments */
		for (ii = 0; ii < nparts; ii++) {
			CamelMimePart *part;

			part = camel_multipart_get_part (multipart, ii);
			if (!part)
				continue;

			ct = camel_mime_part_get_content_type (part);
			if (!ct)
				continue;

			if (ii == 0) {
				e_mail_notes_editor_extract_text_from_part (notes_editor, part);
			} else {
				EAttachment *attachment;

				attachment = e_attachment_new ();

				e_attachment_set_mime_part (attachment, part);
				e_attachment_store_add_attachment (attachment_store, attachment);
				e_attachment_load_async (attachment, (GAsyncReadyCallback)
					e_attachment_load_handle_error, notes_editor);

				g_object_unref (attachment);
			}
		}
	} else {
		e_mail_notes_editor_extract_text_from_part (notes_editor, CAMEL_MIME_PART (message));
	}

	e_content_editor_set_changed (cnt_editor, FALSE);
}

static CamelMimeMessage *
e_mail_notes_editor_encode_text_to_message (EMailNotesEditor *notes_editor)
{
	EContentEditor *cnt_editor;
	EAttachmentStore *attachment_store;
	CamelMimeMessage *message = NULL;
	gchar *message_uid;
	const gchar *username;
	CamelInternetAddress *address;
	gboolean has_text = FALSE, has_attachments;

	g_return_val_if_fail (E_IS_MAIL_NOTES_EDITOR (notes_editor), NULL);
	g_return_val_if_fail (notes_editor->editor, NULL);

	cnt_editor = e_html_editor_get_content_editor (notes_editor->editor);
	g_return_val_if_fail (E_IS_CONTENT_EDITOR (cnt_editor), NULL);

	message = camel_mime_message_new ();
	username = g_get_user_name ();
	if (!username || !*username)
		username = g_get_real_name ();
	address = camel_internet_address_new ();
	camel_internet_address_add (address, NULL, username);

	message_uid = camel_header_msgid_generate (g_get_host_name ());

	camel_mime_message_set_from (message, address);
	camel_mime_message_set_date (message, CAMEL_MESSAGE_DATE_CURRENT, 0);
	camel_mime_message_set_subject (message, _("Message Note"));
	camel_mime_message_set_message_id (message, message_uid);

	g_object_unref (address);
	g_free (message_uid);

	attachment_store = e_attachment_view_get_store (E_ATTACHMENT_VIEW (notes_editor->attachment_paned));
	has_attachments = e_attachment_store_get_num_attachments (attachment_store) > 0;

	if (e_content_editor_get_html_mode (cnt_editor)) {
		CamelMultipart *multipart_alternative;
		CamelMultipart *multipart_body;
		CamelMimePart *part;
		GSList *inline_images_parts = NULL;
		gchar *text;

		multipart_alternative = camel_multipart_new ();
		camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (multipart_alternative), "multipart/alternative");
		camel_multipart_set_boundary (multipart_alternative, NULL);

		text = e_content_editor_get_content (
			cnt_editor,
			E_CONTENT_EDITOR_GET_TEXT_PLAIN |
			E_CONTENT_EDITOR_GET_PROCESSED,
			NULL, NULL);

		if (text && *text) {
			part = camel_mime_part_new ();
			camel_mime_part_set_content (part, text, strlen (text), "text/plain");
			camel_multipart_add_part (multipart_alternative, part);

			g_object_unref (part);

			has_text = TRUE;
		}

		g_free (text);

		text = e_content_editor_get_content (
			cnt_editor,
			E_CONTENT_EDITOR_GET_PROCESSED |
			E_CONTENT_EDITOR_GET_TEXT_HTML |
			E_CONTENT_EDITOR_GET_INLINE_IMAGES,
			g_get_host_name (),
			&inline_images_parts);

		if (has_attachments && !has_text && (!text || !*text)) {
			/* Text is required, thus if there are attachments,
			   but no text, then store at least a space. */
			g_free (text);
			text = g_strdup (" ");
		}

		if (text && *text) {
			part = camel_mime_part_new ();
			camel_mime_part_set_content (part, text, strlen (text), "text/html");
			camel_multipart_add_part (multipart_alternative, part);

			g_object_unref (part);

			has_text = TRUE;
		} else {
			g_slist_free_full (inline_images_parts, g_object_unref);
			inline_images_parts = NULL;
		}

		g_free (text);

		if (inline_images_parts) {
			GSList *link;

			multipart_body = camel_multipart_new ();
			camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (multipart_body), "multipart/related");
			camel_multipart_set_boundary (multipart_body, NULL);

			part = camel_mime_part_new ();
			camel_medium_set_content (CAMEL_MEDIUM (part), CAMEL_DATA_WRAPPER (multipart_alternative));
			camel_multipart_add_part (multipart_body, part);
			g_object_unref (part);

			for (link = inline_images_parts; link; link = g_slist_next (link)) {
				CamelMimePart *part = link->data;

				if (!part)
					continue;

				camel_multipart_add_part (multipart_body, part);
			}
		} else {
			multipart_body = multipart_alternative;
			multipart_alternative = NULL;
		}

		if (has_attachments) {
			CamelMultipart *multipart;

			multipart = camel_multipart_new ();
			camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (multipart), "multipart/mixed");
			camel_multipart_set_boundary (multipart, NULL);

			part = camel_mime_part_new ();
			camel_medium_set_content (CAMEL_MEDIUM (part), CAMEL_DATA_WRAPPER (multipart_body));
			camel_multipart_add_part (multipart, part);
			g_object_unref (part);

			e_attachment_store_add_to_multipart (attachment_store, multipart, "UTF-8");

			g_object_unref (multipart_body);
			multipart_body = multipart;
		}

		camel_medium_set_content (CAMEL_MEDIUM (message), CAMEL_DATA_WRAPPER (multipart_body));

		g_slist_free_full (inline_images_parts, g_object_unref);
		g_clear_object (&multipart_alternative);
		g_clear_object (&multipart_body);
	} else {
		gchar *text;

		text = e_content_editor_get_content (
			cnt_editor,
			E_CONTENT_EDITOR_GET_TEXT_PLAIN |
			E_CONTENT_EDITOR_GET_PROCESSED,
			NULL, NULL);

		if (has_attachments && !has_text && (!text || !*text)) {
			/* Text is required, thus if there are attachments,
			   but no text, then store at least a space. */
			g_free (text);
			text = g_strdup (" ");
		}

		if (text && *text) {
			if (has_attachments) {
				CamelMultipart *multipart;
				CamelMimePart *part;

				multipart = camel_multipart_new ();
				camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (multipart), "multipart/mixed");
				camel_multipart_set_boundary (multipart, NULL);

				part = camel_mime_part_new ();
				camel_mime_part_set_content (part, text, strlen (text), "text/plain");
				camel_multipart_add_part (multipart, part);
				g_object_unref (part);

				e_attachment_store_add_to_multipart (attachment_store, multipart, "UTF-8");

				camel_medium_set_content (CAMEL_MEDIUM (message), CAMEL_DATA_WRAPPER (multipart));

				g_object_unref (multipart);
			} else {
				camel_mime_part_set_content (CAMEL_MIME_PART (message), text, strlen (text), "text/plain");
			}
			has_text = TRUE;
		}

		g_free (text);
	}

	if (has_text) {
		camel_mime_message_encode_8bit_parts (message);
	} else {
		g_clear_object (&message);
	}

	return message;
}

static void
e_mail_notes_retrieve_message_thread (EAlertSinkThreadJobData *job_data,
				      gpointer user_data,
				      GCancellable *cancellable,
				      GError **error)
{
	EMailNotesEditor *notes_editor = user_data;
	CamelMimeMessage *message;

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return;

	g_return_if_fail (E_IS_MAIL_NOTES_EDITOR (notes_editor));

	message = camel_folder_get_message_sync (notes_editor->folder, notes_editor->uid, cancellable, error);
	if (!g_cancellable_is_cancelled (cancellable))
		notes_editor->message = message;
	else
		g_clear_object (&message);
}

static void
e_mail_notes_retrieve_message_done (gpointer ptr)
{
	EMailNotesEditor *notes_editor = ptr;

	g_return_if_fail (E_IS_MAIL_NOTES_EDITOR (notes_editor));

	if (notes_editor->message) {
		EActivityBar *activity_bar;
		CamelDataWrapper *content;
		CamelContentType *ct;

		content = camel_medium_get_content (CAMEL_MEDIUM (notes_editor->message));
		ct = camel_data_wrapper_get_mime_type_field (CAMEL_DATA_WRAPPER (notes_editor->message));

		if (ct && camel_content_type_is (ct, "multipart", "mixed") && CAMEL_IS_MULTIPART (content)) {
			CamelMultipart *multipart = CAMEL_MULTIPART (content);
			guint nparts, ii;

			nparts = camel_multipart_get_number (multipart);
			for (ii = 0; ii < nparts; ii++) {
				CamelMimePart *part;
				CamelContentType *ct;
				const gchar *x_evolution_note;

				part = camel_multipart_get_part (multipart, ii);
				if (!part)
					continue;

				ct = camel_mime_part_get_content_type (part);
				if (!ct || !camel_content_type_is (ct, "message", "rfc822"))
					continue;

				x_evolution_note = camel_medium_get_header (CAMEL_MEDIUM (part), E_MAIL_NOTES_HEADER);
				if (x_evolution_note) {
					content = camel_medium_get_content (CAMEL_MEDIUM (part));
					if (CAMEL_IS_MIME_MESSAGE (content)) {
						e_mail_notes_editor_extract_text_from_message (notes_editor,
							CAMEL_MIME_MESSAGE (content));
					}
					break;
				}
			}
		}

		g_clear_object (&notes_editor->message);
		notes_editor->had_message = TRUE;

		activity_bar = e_html_editor_get_activity_bar (notes_editor->editor);
		e_activity_bar_set_activity (activity_bar, NULL);
	} else {
		GtkAction *action;

		action = gtk_action_group_get_action (notes_editor->action_group, "save-and-close");
		gtk_action_set_sensitive (action, FALSE);
	}

	g_object_unref (notes_editor);
}

static gboolean
mail_notes_editor_delete_event_cb (EMailNotesEditor *notes_editor,
				   GdkEvent *event)
{
	GtkActionGroup *action_group;
	GtkAction *action;

	action_group = notes_editor->action_group;
	action = gtk_action_group_get_action (action_group, "close");
	gtk_action_activate (action);

	return TRUE;
}

static void
notes_editor_activity_notify_cb (EActivityBar *activity_bar,
				 GParamSpec *param,
				 EMailNotesEditor *notes_editor)
{
	EContentEditor *cnt_editor;
	GtkAction *action;
	gboolean can_edit;

	g_return_if_fail (E_IS_ACTIVITY_BAR (activity_bar));
	g_return_if_fail (E_IS_MAIL_NOTES_EDITOR (notes_editor));

	cnt_editor = e_html_editor_get_content_editor (notes_editor->editor);
	can_edit = notes_editor->had_message && !e_activity_bar_get_activity (activity_bar);

	g_object_set (cnt_editor, "editable", can_edit, NULL);

	action = gtk_action_group_get_action (notes_editor->action_group, "save-and-close");
	gtk_action_set_sensitive (action, can_edit);
}

static gboolean
e_mail_notes_replace_message_in_folder_sync (CamelFolder *folder,
					     const gchar *uid,
					     CamelMimeMessage *message,
					     gboolean has_note,
					     GCancellable *cancellable,
					     GError **error)
{
	CamelMessageInfo *mi;
	gboolean success = FALSE;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);
	g_return_val_if_fail (uid != NULL, FALSE);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), FALSE);

	mi = camel_folder_get_message_info (folder, uid);
	if (mi) {
		CamelMessageInfo *clone;
		gchar *appended_uid = NULL;

		clone = camel_message_info_clone (mi);
		camel_message_info_set_user_flag (clone, E_MAIL_NOTES_USER_FLAG, has_note);

		success = camel_folder_append_message_sync (folder, message, clone,
			&appended_uid, cancellable, error);

		if (success)
			camel_message_info_set_flags (mi, CAMEL_MESSAGE_DELETED, CAMEL_MESSAGE_DELETED);

		camel_message_info_unref (clone);
		camel_message_info_unref (mi);
		g_free (appended_uid);
	} else {
		g_set_error_literal (error, CAMEL_ERROR, CAMEL_ERROR_GENERIC, _("Cannot find message in its folder summary"));
	}

	return success;
}

static gboolean
e_mail_notes_replace_note (CamelMimeMessage *message,
			   CamelMimeMessage *note)
{
	CamelMultipart *multipart;
	CamelMimePart *part;
	CamelDataWrapper *orig_content;
	CamelContentType *ct;

	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), FALSE);
	if (note)
		g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (note), FALSE);

	orig_content = camel_medium_get_content (CAMEL_MEDIUM (message));
	ct = camel_data_wrapper_get_mime_type_field (CAMEL_DATA_WRAPPER (message));
	if (ct && camel_content_type_is (ct, "multipart", "mixed") && CAMEL_IS_MULTIPART (orig_content)) {
		CamelMimePart *content_adept = NULL;
		CamelMultipart *multipart = CAMEL_MULTIPART (orig_content);
		guint nparts, ii;

		nparts = camel_multipart_get_number (multipart);
		for (ii = 0; ii < nparts; ii++) {
			CamelMimePart *part;
			CamelContentType *ct;
			const gchar *x_evolution_note;

			part = camel_multipart_get_part (multipart, ii);
			if (!part)
				continue;

			ct = camel_mime_part_get_content_type (part);
			if (!ct || !camel_content_type_is (ct, "message", "rfc822")) {
				if (content_adept) {
					content_adept = NULL;
					break;
				}
				content_adept = part;
				continue;
			}

			x_evolution_note = camel_medium_get_header (CAMEL_MEDIUM (part), E_MAIL_NOTES_HEADER);
			if (x_evolution_note)
				break;

			if (content_adept) {
				content_adept = NULL;
				break;
			}
			content_adept = part;
		}

		if (content_adept)
			orig_content = camel_medium_get_content (CAMEL_MEDIUM (content_adept));
	}

	if (!orig_content)
		return FALSE;

	g_object_ref (orig_content);

	if (note) {
		multipart = camel_multipart_new ();
		camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (multipart), "multipart/mixed");
		camel_multipart_set_boundary (multipart, NULL);

		part = camel_mime_part_new ();
		camel_medium_set_content (CAMEL_MEDIUM (part), CAMEL_DATA_WRAPPER (orig_content));
		camel_multipart_add_part (multipart, part);
		g_object_unref (part);

		part = camel_mime_part_new ();
		/* Value doesn't matter, it's checked for an existence only */
		camel_medium_add_header (CAMEL_MEDIUM (part), E_MAIL_NOTES_HEADER, "True");
		camel_mime_part_set_disposition (CAMEL_MIME_PART (part), "inline");
		camel_mime_part_set_description (CAMEL_MIME_PART (part), _("Message Note"));
		camel_medium_set_content (CAMEL_MEDIUM (part), CAMEL_DATA_WRAPPER (note));

		camel_mime_part_set_content_type (part, "message/rfc822");

		camel_multipart_add_part (multipart, part);
		g_object_unref (part);

		camel_medium_set_content (CAMEL_MEDIUM (message), CAMEL_DATA_WRAPPER (multipart));
	} else {
		camel_medium_set_content (CAMEL_MEDIUM (message), CAMEL_DATA_WRAPPER (orig_content));
	}

	g_clear_object (&orig_content);

	return TRUE;
}

static void
action_close_cb (GtkAction *action,
		 EMailNotesEditor *notes_editor)
{
	EContentEditor *cnt_editor;
	gboolean something_changed = FALSE;

	cnt_editor = e_html_editor_get_content_editor (notes_editor->editor);

	something_changed = e_content_editor_get_changed (cnt_editor);

	if (something_changed) {
		gint response;

		response = e_alert_run_dialog_for_args (
			GTK_WINDOW (notes_editor),
			"mail:ask-mail-note-changed", NULL);
		if (response == GTK_RESPONSE_YES) {
			GtkActionGroup *action_group;

			action_group = notes_editor->action_group;
			action = gtk_action_group_get_action (
				action_group, "save-and-close");
			gtk_action_activate (action);
			return;
		} else if (response == GTK_RESPONSE_CANCEL)
			return;
	}

	gtk_widget_destroy (GTK_WIDGET (notes_editor));
}

typedef struct {
	EMailNotesEditor *notes_editor;
	CamelMimeMessage *inner_message;
	gboolean success;
} SaveAndCloseData;

static void
save_and_close_data_free (gpointer ptr)
{
	SaveAndCloseData *scd = ptr;

	if (scd) {
		if (scd->success)
			gtk_widget_destroy (GTK_WIDGET (scd->notes_editor));
		else
			g_clear_object (&scd->notes_editor);
		g_clear_object (&scd->inner_message);
		g_free (scd);
	}
}

static void
e_mail_notes_store_changes_thread (EAlertSinkThreadJobData *job_data,
				   gpointer user_data,
				   GCancellable *cancellable,
				   GError **error)
{
	CamelMimeMessage *message;
	SaveAndCloseData *scd = user_data;

	g_return_if_fail (scd != NULL);

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return;

	if (!scd->inner_message) {
		scd->success = e_mail_notes_remove_sync (scd->notes_editor->folder,
			scd->notes_editor->uid, cancellable, error);
		return;
	}

	message = camel_folder_get_message_sync (scd->notes_editor->folder, scd->notes_editor->uid, cancellable, error);
	if (!message)
		return;

	e_mail_notes_replace_note (message, scd->inner_message);

	scd->success = e_mail_notes_replace_message_in_folder_sync (scd->notes_editor->folder,
		scd->notes_editor->uid, message, TRUE, cancellable, error);

	g_clear_object (&message);
}

static void
action_save_and_close_cb (GtkAction *action,
			  EMailNotesEditor *notes_editor)
{
	SaveAndCloseData *scd;
	gchar *full_display_name;
	EActivityBar *activity_bar;
	EActivity *activity;

	g_return_if_fail (E_IS_MAIL_NOTES_EDITOR (notes_editor));

	scd = g_new0 (SaveAndCloseData, 1);
	scd->notes_editor = g_object_ref (notes_editor);
	scd->inner_message = e_mail_notes_editor_encode_text_to_message (notes_editor);
	scd->success = FALSE;

	full_display_name = e_mail_folder_to_full_display_name (notes_editor->folder, NULL);

	activity_bar = e_html_editor_get_activity_bar (notes_editor->editor);
	activity = e_alert_sink_submit_thread_job (E_ALERT_SINK (notes_editor->editor),
		_("Storing changes..."), "mail:failed-store-note",
		full_display_name ? full_display_name : camel_folder_get_display_name (notes_editor->folder),
		e_mail_notes_store_changes_thread,
		scd, save_and_close_data_free);
	e_activity_bar_set_activity (activity_bar, activity);
	g_clear_object (&activity);

	g_free (full_display_name);
}

static void
e_mail_notes_editor_dispose (GObject *object)
{
	EMailNotesEditor *notes_editor = E_MAIL_NOTES_EDITOR (object);

	if (notes_editor->editor) {
		EActivityBar *activity_bar;

		activity_bar = e_html_editor_get_activity_bar (notes_editor->editor);
		g_signal_handlers_disconnect_by_func (activity_bar,
			G_CALLBACK (notes_editor_activity_notify_cb), notes_editor);

		notes_editor->editor = NULL;
	}

	g_clear_object (&notes_editor->focus_tracker);
	g_clear_object (&notes_editor->action_group);

	/* Chain up to parent's method */
	G_OBJECT_CLASS (e_mail_notes_editor_parent_class)->dispose (object);
}

static void
e_mail_notes_editor_finalize (GObject *object)
{
	EMailNotesEditor *notes_editor = E_MAIL_NOTES_EDITOR (object);

	g_clear_object (&notes_editor->focus_tracker);
	g_clear_object (&notes_editor->folder);
	g_clear_object (&notes_editor->message);
	g_free (notes_editor->uid);

	/* Chain up to parent's method */
	G_OBJECT_CLASS (e_mail_notes_editor_parent_class)->finalize (object);
}

static void
e_mail_notes_editor_class_init (EMailNotesEditorClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose = e_mail_notes_editor_dispose;
	object_class->finalize = e_mail_notes_editor_finalize;
}

static void
e_mail_notes_editor_init (EMailNotesEditor *notes_editor)
{
}

static EMailNotesEditor *
e_mail_notes_editor_new_with_editor (EHTMLEditor *html_editor,
				     GtkWindow *parent,
				     CamelFolder *folder,
				     const gchar *uid)
{
	const gchar *ui =
		"<ui>\n"
		"  <menubar name='main-menu'>\n"
		"    <placeholder name='pre-edit-menu'>\n"
		"      <menu action='file-menu'>\n"
		"        <menuitem action='save-and-close'/>\n"
		"        <separator/>"
		"        <menuitem action='close'/>\n"
		"      </menu>\n"
		"    </placeholder>\n"
		"  </menubar>\n"
		"  <toolbar name='main-toolbar'>\n"
		"    <placeholder name='pre-main-toolbar'>\n"
		"      <toolitem action='save-and-close'/>\n"
		"    </placeholder>\n"
		"  </toolbar>\n"
		"</ui>";

	GtkActionEntry entries[] = {

		{ "close",
		  "window-close",
		  N_("_Close"),
		  "<Control>w",
		  N_("Close"),
		  G_CALLBACK (action_close_cb) },

		{ "save-and-close",
		  "document-save",
		  N_("_Save and Close"),
		  "<Control>Return",
		  N_("Save and Close"),
		  G_CALLBACK (action_save_and_close_cb) },

		{ "file-menu",
		  NULL,
		  N_("_File"),
		  NULL,
		  NULL,
		  NULL }
	};

	EMailNotesEditor *notes_editor;
	EContentEditor *cnt_editor;
	EFocusTracker *focus_tracker;
	EActivityBar *activity_bar;
	GtkUIManager *ui_manager;
	GtkWidget *widget, *content;
	GtkActionGroup *action_group;
	GtkAction *action;
	GSettings *settings;
	GError *local_error = NULL;

	notes_editor = g_object_new (E_TYPE_MAIL_NOTES_EDITOR, NULL);

	g_object_set (G_OBJECT (notes_editor),
		"transient-for", parent,
		"destroy-with-parent", TRUE,
		"window-position", GTK_WIN_POS_CENTER_ON_PARENT,
		"title", _("Edit Message Note"),
		NULL);

	gtk_window_set_default_size (GTK_WINDOW (notes_editor), 600, 440);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (notes_editor), widget);
	gtk_widget_show (widget);

	content = widget;

	widget = GTK_WIDGET (html_editor);

	notes_editor->editor = html_editor;
	cnt_editor = e_html_editor_get_content_editor (notes_editor->editor);
	ui_manager = e_html_editor_get_ui_manager (notes_editor->editor);

	/* Because we are loading from a hard-coded string, there is
	 * no chance of I/O errors.  Failure here implies a malformed
	 * UI definition.  Full stop. */
	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &local_error);
	if (local_error != NULL)
		g_error ("%s: Failed to load built-in UI definition: %s", G_STRFUNC, local_error->message);

	action_group = gtk_action_group_new ("notes");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (action_group, entries, G_N_ELEMENTS (entries), notes_editor);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	notes_editor->action_group = g_object_ref (action_group);

	/* Hide page properties because it is not inherited in the mail. */
	action = e_html_editor_get_action (notes_editor->editor, "properties-page");
	gtk_action_set_visible (action, FALSE);

	action = e_html_editor_get_action (notes_editor->editor, "context-properties-page");
	gtk_action_set_visible (action, FALSE);

	gtk_ui_manager_ensure_update (ui_manager);

	/* Construct the window content. */

	widget = e_html_editor_get_managed_widget (notes_editor->editor, "/main-menu");
	gtk_box_pack_start (GTK_BOX (content), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = e_html_editor_get_managed_widget (notes_editor->editor, "/main-toolbar");
	gtk_box_pack_start (GTK_BOX (content), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = GTK_WIDGET (notes_editor->editor);
	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		NULL);
	gtk_box_pack_start (GTK_BOX (content), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	widget = e_attachment_paned_new ();
	gtk_box_pack_start (GTK_BOX (content), widget, FALSE, FALSE, 0);
	notes_editor->attachment_paned = E_ATTACHMENT_PANED (widget);
	gtk_widget_show (widget);

	e_binding_bind_property (
		cnt_editor, "editable",
		widget, "sensitive",
		G_BINDING_SYNC_CREATE);

	/* Configure an EFocusTracker to manage selection actions. */
	focus_tracker = e_focus_tracker_new (GTK_WINDOW (notes_editor));

	action = e_html_editor_get_action (notes_editor->editor, "cut");
	e_focus_tracker_set_cut_clipboard_action (focus_tracker, action);

	action = e_html_editor_get_action (notes_editor->editor, "copy");
	e_focus_tracker_set_copy_clipboard_action (focus_tracker, action);

	action = e_html_editor_get_action (notes_editor->editor, "paste");
	e_focus_tracker_set_paste_clipboard_action (focus_tracker, action);

	action = e_html_editor_get_action (notes_editor->editor, "select-all");
	e_focus_tracker_set_select_all_action (focus_tracker, action);

	notes_editor->focus_tracker = focus_tracker;

	gtk_widget_grab_focus (GTK_WIDGET (cnt_editor));

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	e_content_editor_set_html_mode (
		cnt_editor, g_settings_get_boolean (settings, "composer-send-html"));
	g_object_unref (settings);

	g_signal_connect (
		notes_editor, "delete-event",
		G_CALLBACK (mail_notes_editor_delete_event_cb), NULL);

	activity_bar = e_html_editor_get_activity_bar (notes_editor->editor);

	g_signal_connect (activity_bar, "notify::activity",
		G_CALLBACK (notes_editor_activity_notify_cb), notes_editor);

	notes_editor->folder = g_object_ref (folder);
	notes_editor->uid = g_strdup (uid);
	notes_editor->had_message = FALSE;

	return notes_editor;
}

typedef struct _AsyncData {
	GtkWindow *parent;
	CamelFolder *folder;
	gchar *uid;
} AsyncData;

static void
async_data_free (gpointer ptr)
{
	AsyncData *ad = ptr;

	if (ad) {
		g_clear_object (&ad->parent);
		g_clear_object (&ad->folder);
		g_free (ad->uid);
		g_free (ad);
	}
}

static void
e_mail_notes_editor_ready_cb (GObject *source_object,
			      GAsyncResult *result,
			      gpointer user_data)
{
	AsyncData *ad = user_data;
	GtkWidget *html_editor;
	GError *error = NULL;

	g_return_if_fail (result != NULL);
	g_return_if_fail (ad != NULL);

	html_editor = e_html_editor_new_finish (result, &error);
	if (error) {
		g_warning ("%s: Failed to create HTML editor: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
	} else {
		EMailNotesEditor *notes_editor;
		EActivityBar *activity_bar;
		EActivity *activity;

		notes_editor = e_mail_notes_editor_new_with_editor (E_HTML_EDITOR (html_editor),
			ad->parent, ad->folder, ad->uid);

		activity_bar = e_html_editor_get_activity_bar (notes_editor->editor);
		activity = e_alert_sink_submit_thread_job (E_ALERT_SINK (notes_editor->editor),
			_("Retrieving message..."), "mail:no-retrieve-message", NULL,
			e_mail_notes_retrieve_message_thread,
			g_object_ref (notes_editor), e_mail_notes_retrieve_message_done);
		e_activity_bar_set_activity (activity_bar, activity);
		g_clear_object (&activity);

		gtk_widget_show (GTK_WIDGET (notes_editor));
	}

	async_data_free (ad);
}

void
e_mail_notes_edit (GtkWindow *parent,
		   CamelFolder *folder,
		   const gchar *uid)
{
	AsyncData *ad;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uid != NULL);

	ad = g_new0 (AsyncData, 1);
	ad->parent = parent ? g_object_ref (parent) : NULL;
	ad->folder = g_object_ref (folder);
	ad->uid = g_strdup (uid);

	e_html_editor_new (e_mail_notes_editor_ready_cb, ad);
}

gboolean
e_mail_notes_remove_sync (CamelFolder *folder,
			  const gchar *uid,
			  GCancellable *cancellable,
			  GError **error)
{
	CamelMimeMessage *message;
	gboolean success;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);
	g_return_val_if_fail (uid != NULL, FALSE);

	message = camel_folder_get_message_sync (folder, uid, cancellable, error);
	if (!message)
		return FALSE;

	success = e_mail_notes_replace_note (message, NULL);
	if (success) {
		success = e_mail_notes_replace_message_in_folder_sync (folder,
			uid, message, FALSE, cancellable, error);
	} else {
		/* There was no note found in the message, thus it was successfully removed */
		success = TRUE;
	}

	g_clear_object (&message);

	return success;
}
