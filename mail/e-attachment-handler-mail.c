/*
 * e-attachment-handler-mail.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>  
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-attachment-handler-mail.h"

#include <config.h>
#include <glib/gi18n.h>
#include <camel/camel-stream-mem.h>

#include "mail/em-composer-utils.h"

#define E_ATTACHMENT_HANDLER_MAIL_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ATTACHMENT_HANDLER_MAIL, EAttachmentHandlerMailPrivate))

struct _EAttachmentHandlerMailPrivate {
	gint placeholder;
};

static gpointer parent_class;

static const gchar *ui =
"<ui>"
"  <popup name='context'>"
"    <placeholder name='custom-actions'>"
"      <menuitem action='mail-reply-sender'/>"
"      <menuitem action='mail-reply-all'/>"
"      <menuitem action='mail-forward'/>"
"    </placeholder>"
"  </popup>"
"</ui>";

/* Note: Do not use the info field. */
static GtkTargetEntry target_table[] = {
	{ "message/rfc822",	0, 0 },
	{ "x-uid-list",		0, 0 }
};

static void
attachment_handler_mail_forward (GtkAction *action,
                                 EAttachmentView *view)
{
	EAttachment *attachment;
	CamelMimePart *mime_part;
	CamelDataWrapper *wrapper;
	GList *selected;

	selected = e_attachment_view_get_selected_attachments (view);
	g_return_if_fail (g_list_length (selected) == 1);

	attachment = E_ATTACHMENT (selected->data);
	mime_part = e_attachment_get_mime_part (attachment);
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));

	em_utils_forward_message (CAMEL_MIME_MESSAGE (wrapper), NULL);

	g_list_foreach (selected, (GFunc) g_object_unref, NULL);
	g_list_free (selected);
}

static void
attachment_handler_mail_reply_all (GtkAction *action,
                                   EAttachmentView *view)
{
	EAttachment *attachment;
	CamelMimePart *mime_part;
	CamelDataWrapper *wrapper;
	GList *selected;

	selected = e_attachment_view_get_selected_attachments (view);
	g_return_if_fail (g_list_length (selected) == 1);

	attachment = E_ATTACHMENT (selected->data);
	mime_part = e_attachment_get_mime_part (attachment);
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));

	em_utils_reply_to_message (
		NULL, NULL, CAMEL_MIME_MESSAGE (wrapper),
		REPLY_MODE_ALL, NULL);

	g_list_foreach (selected, (GFunc) g_object_unref, NULL);
	g_list_free (selected);
}

static void
attachment_handler_mail_reply_sender (GtkAction *action,
                                      EAttachmentView *view)
{
	EAttachment *attachment;
	CamelMimePart *mime_part;
	CamelDataWrapper *wrapper;
	GList *selected;

	selected = e_attachment_view_get_selected_attachments (view);
	g_return_if_fail (g_list_length (selected) == 1);

	attachment = E_ATTACHMENT (selected->data);
	mime_part = e_attachment_get_mime_part (attachment);
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));

	em_utils_reply_to_message (
		NULL, NULL, CAMEL_MIME_MESSAGE (wrapper),
		REPLY_MODE_SENDER, NULL);

	g_list_foreach (selected, (GFunc) g_object_unref, NULL);
	g_list_free (selected);
}

static GtkActionEntry standard_entries[] = {

	{ "mail-forward",
	  "mail-forward",
	  N_("_Forward"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (attachment_handler_mail_forward) },

	{ "mail-reply-all",
	  "mail-reply-all",
	  N_("Reply to _All"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (attachment_handler_mail_reply_all) },

	{ "mail-reply-sender",
	  "mail-reply-sender",
	  N_("_Reply to Sender"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (attachment_handler_mail_reply_sender) }
};

static void
attachment_handler_mail_message_rfc822 (EAttachmentView *view,
                                        GdkDragContext *drag_context,
                                        gint x,
                                        gint y,
                                        GtkSelectionData *selection_data,
                                        guint info,
                                        guint time)
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
	camel_stream_write (stream, data, length);
	camel_stream_reset (stream);

	message = camel_mime_message_new ();
	wrapper = CAMEL_DATA_WRAPPER (message);

	if (camel_data_wrapper_construct_from_stream (wrapper, stream) == -1)
		goto exit;

	store = e_attachment_view_get_store (view);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (view));
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

	attachment = e_attachment_new_for_message (message);
	e_attachment_store_add_attachment (store, attachment);
	e_attachment_load_async (
		attachment, (GAsyncReadyCallback)
		e_attachment_load_handle_error, parent);
	g_object_unref (attachment);

	success = TRUE;

exit:
	camel_object_unref (message);
	camel_object_unref (stream);

	gtk_drag_finish (drag_context, success, FALSE, time);
}

static void
attachment_handler_mail_x_uid_list (EAttachmentView *view,
                                    GdkDragContext *drag_context,
                                    gint x,
                                    gint y,
                                    GtkSelectionData *selection_data,
                                    guint info,
                                    guint time)
{
	static GdkAtom atom = GDK_NONE;

	if (G_UNLIKELY (atom == GDK_NONE))
		atom = gdk_atom_intern_static_string ("x-uid-list");

	if (gtk_selection_data_get_target (selection_data) != atom)
		return;

	return;  /* REMOVE ME */

	g_signal_stop_emission_by_name (view, "drag-data-received");
}

static void
attachment_handler_mail_update_actions (EAttachmentView *view)
{
	EAttachment *attachment;
	CamelMimePart *mime_part;
	CamelDataWrapper *wrapper;
	GtkActionGroup *action_group;
	GList *selected;
	gboolean visible = FALSE;

	selected = e_attachment_view_get_selected_attachments (view);

	if (g_list_length (selected) != 1)
		goto exit;

	attachment = E_ATTACHMENT (selected->data);
	mime_part = e_attachment_get_mime_part (attachment);

	if (!CAMEL_IS_MIME_PART (mime_part))
		goto exit;

	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));

	visible = CAMEL_IS_MIME_MESSAGE (wrapper);

exit:
	action_group = e_attachment_view_get_action_group (view, "mail");
	gtk_action_group_set_visible (action_group, visible);

	g_list_foreach (selected, (GFunc) g_object_unref, NULL);
	g_list_free (selected);
}

static void
attachment_handler_mail_constructed (GObject *object)
{
	EAttachmentHandler *handler;
	EAttachmentView *view;
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	const gchar *domain = GETTEXT_PACKAGE;
	GError *error = NULL;

	handler = E_ATTACHMENT_HANDLER (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	view = e_attachment_handler_get_view (handler);
	ui_manager = e_attachment_view_get_ui_manager (view);

	action_group = gtk_action_group_new ("mail");
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_action_group_add_actions (
		action_group, standard_entries,
		G_N_ELEMENTS (standard_entries), view);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group);

	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	g_signal_connect (
		view, "update-actions",
		G_CALLBACK (attachment_handler_mail_update_actions),
		NULL);

	g_signal_connect (
		view, "drag-data-received",
		G_CALLBACK (attachment_handler_mail_message_rfc822),
		NULL);

	g_signal_connect (
		view, "drag-data-received",
		G_CALLBACK (attachment_handler_mail_x_uid_list),
		NULL);
}

static GdkDragAction
attachment_handler_mail_get_drag_actions (EAttachmentHandler *handler)
{
	return GDK_ACTION_COPY;
}

static const GtkTargetEntry *
attachment_handler_mail_get_target_table (EAttachmentHandler *handler,
                                          guint *n_targets)
{
	if (n_targets != NULL)
		*n_targets = G_N_ELEMENTS (target_table);

	return target_table;
}

static void
attachment_handler_mail_class_init (EAttachmentHandlerMailClass *class)
{
	GObjectClass *object_class;
	EAttachmentHandlerClass *handler_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EAttachmentHandlerMailPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = attachment_handler_mail_constructed;

	handler_class = E_ATTACHMENT_HANDLER_CLASS (class);
	handler_class->get_drag_actions = attachment_handler_mail_get_drag_actions;
	handler_class->get_target_table = attachment_handler_mail_get_target_table;
}

static void
attachment_handler_mail_init (EAttachmentHandlerMail *handler)
{
	handler->priv = E_ATTACHMENT_HANDLER_MAIL_GET_PRIVATE (handler);
}

GType
e_attachment_handler_mail_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EAttachmentHandlerMailClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) attachment_handler_mail_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EAttachmentHandlerMail),
			0,     /* n_preallocs */
			(GInstanceInitFunc) attachment_handler_mail_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_ATTACHMENT_HANDLER,
			"EAttachmentHandlerMail", &type_info, 0);
	}

	return type;
}
