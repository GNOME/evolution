/*
 * e-attachment-handler-sendto.c
 *
 * Copyright (C) 2009 Matthew Barnes
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#include "e-attachment-handler-sendto.h"

#include <config.h>
#include <errno.h>

#include <glib/gi18n-lib.h>

static gpointer parent_class;

static const gchar *ui =
"<ui>"
"  <popup name='context'>"
"    <placeholder name='custom-actions'>"
"      <menuitem action='sendto'/>"
"    </placeholder>"
"  </popup>"
"</ui>";

static void
sendto_save_finished_cb (EAttachment *attachment,
                         GAsyncResult *result,
                         EAttachmentHandler *handler)
{
	EAttachmentView *view;
	EAttachmentStore *store;
	GtkWidget *dialog;
	gchar **uris;
	gpointer parent;
	gchar *arguments;
	gchar *command_line;
	guint n_uris = 1;
	GError *error = NULL;

	view = e_attachment_handler_get_view (handler);
	store = e_attachment_view_get_store (view);

	uris = e_attachment_store_get_uris_finish (store, result, &error);

	if (uris != NULL)
		n_uris = g_strv_length (uris);

	if (error != NULL)
		goto error;

	arguments = g_strjoinv (" ", uris);
	command_line = g_strdup_printf ("nautilus-sendto %s", arguments);

	g_message ("Command: %s", command_line);
	g_spawn_command_line_async (command_line, &error);

	g_free (command_line);
	g_free (arguments);

	if (error != NULL)
		goto error;

	goto exit;

error:
	parent = gtk_widget_get_toplevel (GTK_WIDGET (view));
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

	dialog = gtk_message_dialog_new_with_markup (
		parent, GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
		"<big><b>%s</b></big>",
		ngettext ("Could not send attachment",
		"Could not send attachments", n_uris));

	gtk_message_dialog_format_secondary_text (
		GTK_MESSAGE_DIALOG (dialog), "%s", error->message);

	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
	g_error_free (error);

exit:
	g_object_unref (handler);
	g_strfreev (uris);
}

static void
action_sendto_cb (GtkAction *action,
                  EAttachmentHandler *handler)
{
	EAttachmentView *view;
	EAttachmentStore *store;
	GList *selected;

	view = e_attachment_handler_get_view (handler);
	store = e_attachment_view_get_store (view);

	selected = e_attachment_view_get_selected_attachments (view);
	g_return_if_fail (selected != NULL);

	e_attachment_store_get_uris_async (
		store, selected, (GAsyncReadyCallback)
		sendto_save_finished_cb, g_object_ref (handler));

	g_list_foreach (selected, (GFunc) g_object_unref, NULL);
	g_list_free (selected);
}

static GtkActionEntry standard_entries[] = {

	{ "sendto",
	  "document-send",
	  N_("_Send To..."),
	  NULL,
	  N_("Send the selected attachments somewhere"),
	  G_CALLBACK (action_sendto_cb) }
};

static void
attachment_handler_sendto_update_actions_cb (EAttachmentView *view,
                                             EAttachmentHandler *handler)
{
	GtkActionGroup *action_group;
	GList *selected, *iter;
	gboolean visible = FALSE;
	gchar *program;

	program = g_find_program_in_path ("nautilus-sendto");
	selected = e_attachment_view_get_selected_attachments (view);

	if (program == NULL || selected == NULL)
		goto exit;

	/* Make sure no file transfers are in progress. */
	for (iter = selected; iter != NULL; iter = iter->next) {
		EAttachment *attachment = iter->data;

		if (e_attachment_get_loading (attachment))
			goto exit;

		if (e_attachment_get_saving (attachment))
			goto exit;
	}

	visible = TRUE;

exit:
	action_group = e_attachment_view_get_action_group (view, "sendto");
	gtk_action_group_set_visible (action_group, visible);

	g_list_foreach (selected, (GFunc) g_object_unref, NULL);
	g_list_free (selected);

	g_free (program);
}

static void
attachment_handler_sendto_constructed (GObject *object)
{
	EAttachmentHandler *handler;
	EAttachmentView *view;
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	GError *error = NULL;

	handler = E_ATTACHMENT_HANDLER (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	view = e_attachment_handler_get_view (handler);
	ui_manager = e_attachment_view_get_ui_manager (view);

	action_group = gtk_action_group_new ("sendto");
	gtk_action_group_set_translation_domain (
		action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (
		action_group, standard_entries,
		G_N_ELEMENTS (standard_entries), object);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);

	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	g_signal_connect (
		view, "update-actions",
		G_CALLBACK (attachment_handler_sendto_update_actions_cb),
		object);
}

static void
attachment_handler_sendto_class_init (EAttachmentHandlerSendtoClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = attachment_handler_sendto_constructed;
}

GType
e_attachment_handler_sendto_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EAttachmentHandlerSendtoClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) attachment_handler_sendto_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EAttachmentHandlerSendto),
			0,     /* n_preallocs */
			(GInstanceInitFunc) NULL,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_ATTACHMENT_HANDLER,
			"EAttachmentHandlerSendto",
			&type_info, 0);
	}

	return type;
}
