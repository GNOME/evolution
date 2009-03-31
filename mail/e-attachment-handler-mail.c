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

static void
action_mail_forward_cb (GtkAction *action,
                        EAttachmentHandler *handler)
{
	EAttachmentView *view;
	EAttachment *attachment;
	CamelMimePart *mime_part;
	CamelDataWrapper *wrapper;
	GList *selected;

	view = e_attachment_handler_get_view (handler);
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
action_mail_reply_all_cb (GtkAction *action,
                          EAttachmentHandler *handler)
{
	EAttachmentView *view;
	EAttachment *attachment;
	CamelMimePart *mime_part;
	CamelDataWrapper *wrapper;
	GList *selected;

	view = e_attachment_handler_get_view (handler);
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
action_mail_reply_sender_cb (GtkAction *action,
                             EAttachmentHandler *handler)
{
	EAttachmentView *view;
	EAttachment *attachment;
	CamelMimePart *mime_part;
	CamelDataWrapper *wrapper;
	GList *selected;

	view = e_attachment_handler_get_view (handler);
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
	  G_CALLBACK (action_mail_forward_cb) },

	{ "mail-reply-all",
	  "mail-reply-all",
	  N_("Reply to _All"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_mail_reply_all_cb) },

	{ "mail-reply-sender",
	  "mail-reply-sender",
	  N_("_Reply to Sender"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_mail_reply_sender_cb) }
};

static void
attachment_handler_mail_update_actions_cb (EAttachmentView *view,
                                           EAttachmentHandler *handler)
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
	EAttachmentHandlerMailPrivate *priv;
	EAttachmentHandler *handler;
	EAttachmentView *view;
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	const gchar *domain = GETTEXT_PACKAGE;
	GError *error = NULL;

	handler = E_ATTACHMENT_HANDLER (object);
	priv = E_ATTACHMENT_HANDLER_MAIL_GET_PRIVATE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	view = e_attachment_handler_get_view (handler);
	ui_manager = e_attachment_view_get_ui_manager (view);

	action_group = gtk_action_group_new ("mail");
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_action_group_add_actions (
		action_group, standard_entries,
		G_N_ELEMENTS (standard_entries), object);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group);

	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	g_signal_connect (
		view, "update-actions",
		G_CALLBACK (attachment_handler_mail_update_actions_cb),
		object);
}

static void
attachment_handler_mail_class_init (EAttachmentHandlerMailClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EAttachmentHandlerMailPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = attachment_handler_mail_constructed;
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
			"EAttachmentHandlerMail",
			&type_info, 0);
	}

	return type;
}
