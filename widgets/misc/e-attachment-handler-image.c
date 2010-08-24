/*
 * e-attachment-handler-image.c
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

#include "e-attachment-handler-image.h"

#include <glib/gi18n.h>
#include <gconf/gconf-client.h>

#include <e-util/e-util.h>

#define E_ATTACHMENT_HANDLER_IMAGE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ATTACHMENT_HANDLER_IMAGE, EAttachmentHandlerImagePrivate))

struct _EAttachmentHandlerImagePrivate {
	gint placeholder;
};

static const gchar *ui =
"<ui>"
"  <popup name='context'>"
"    <placeholder name='custom-actions'>"
"      <menuitem action='image-set-as-background'/>"
"    </placeholder>"
"  </popup>"
"</ui>";

G_DEFINE_TYPE (
	EAttachmentHandlerImage,
	e_attachment_handler_image,
	E_TYPE_ATTACHMENT_HANDLER)

static void
action_image_set_as_background_saved_cb (EAttachment *attachment,
                                         GAsyncResult *result,
                                         EAttachmentHandler *handler)
{
	EAttachmentView *view;
	GConfClient *client;
	GtkWidget *dialog;
	GFile *file;
	const gchar *key;
	gpointer parent;
	gchar *value;
	GError *error = NULL;

	client = gconf_client_get_default ();
	view = e_attachment_handler_get_view (handler);

	file = e_attachment_save_finish (attachment, result, &error);

	if (error != NULL)
		goto error;

	value = g_file_get_path (file);
	g_object_unref (file);

	key = "/desktop/gnome/background/picture_filename";
	gconf_client_set_string (client, key, value, &error);
	g_free (value);

	if (error != NULL)
		goto error;

	/* Ignore errors for this part. */
	key = "/desktop/gnome/background/picture_options";
	value = gconf_client_get_string (client, key, NULL);
	if (g_strcmp0 (value, "none") == 0)
		gconf_client_set_string (client, key, "wallpaper", NULL);
	g_free (value);

	goto exit;

error:
	parent = gtk_widget_get_toplevel (GTK_WIDGET (view));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	dialog = gtk_message_dialog_new_with_markup (
		parent, GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
		"<big><b>%s</b></big>",
		_("Could not set as background"));

	gtk_message_dialog_format_secondary_text (
		GTK_MESSAGE_DIALOG (dialog), "%s", error->message);

	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
	g_error_free (error);

exit:
	g_object_unref (client);
	g_object_unref (handler);
}

static void
action_image_set_as_background_cb (GtkAction *action,
                                   EAttachmentHandler *handler)
{
	EAttachmentView *view;
	EAttachment *attachment;
	GFile *destination;
	GList *selected;
	gchar *path;

	view = e_attachment_handler_get_view (handler);
	selected = e_attachment_view_get_selected_attachments (view);
	g_return_if_fail (g_list_length (selected) == 1);
	attachment = E_ATTACHMENT (selected->data);

	/* Save the image under ~/.gnome2/wallpapers/. */
	path = g_build_filename (
		e_get_gnome2_user_dir (), "wallpapers", NULL);
	destination = g_file_new_for_path (path);
	g_mkdir_with_parents (path, 0755);
	g_free (path);

	e_attachment_save_async (
		attachment, destination, (GAsyncReadyCallback)
		action_image_set_as_background_saved_cb,
		g_object_ref (handler));

	g_object_unref (destination);

	g_list_foreach (selected, (GFunc) g_object_unref, NULL);
	g_list_free (selected);
}

static GtkActionEntry standard_entries[] = {

	{ "image-set-as-background",
	  NULL,
	  N_("Set as _Background"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_image_set_as_background_cb) }
};

static void
attachment_handler_image_update_actions_cb (EAttachmentView *view,
                                            EAttachmentHandler *handler)
{
	EAttachment *attachment;
	GFileInfo *file_info;
	GtkActionGroup *action_group;
	const gchar *content_type;
	gchar *mime_type;
	GList *selected;
	gboolean visible = FALSE;

	selected = e_attachment_view_get_selected_attachments (view);

	if (g_list_length (selected) != 1)
		goto exit;

	attachment = E_ATTACHMENT (selected->data);
	file_info = e_attachment_get_file_info (attachment);

	if (file_info == NULL)
		goto exit;

	if (e_attachment_get_loading (attachment))
		goto exit;

	if (e_attachment_get_saving (attachment))
		goto exit;

	content_type = g_file_info_get_content_type (file_info);

	mime_type = g_content_type_get_mime_type (content_type);
	visible = (g_ascii_strncasecmp (mime_type, "image/", 6) == 0);
	g_free (mime_type);

exit:
	action_group = e_attachment_view_get_action_group (view, "image");
	gtk_action_group_set_visible (action_group, visible);

	g_list_foreach (selected, (GFunc) g_object_unref, NULL);
	g_list_free (selected);
}

static void
attachment_handler_image_constructed (GObject *object)
{
	EAttachmentHandler *handler;
	EAttachmentView *view;
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	GError *error = NULL;

	handler = E_ATTACHMENT_HANDLER (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_attachment_handler_image_parent_class)->
		constructed (object);

	view = e_attachment_handler_get_view (handler);

	action_group = e_attachment_view_add_action_group (view, "image");
	gtk_action_group_add_actions (
		action_group, standard_entries,
		G_N_ELEMENTS (standard_entries), object);

	ui_manager = e_attachment_view_get_ui_manager (view);
	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	g_signal_connect (
		view, "update-actions",
		G_CALLBACK (attachment_handler_image_update_actions_cb),
		object);
}

static void
e_attachment_handler_image_class_init (EAttachmentHandlerImageClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EAttachmentHandlerImagePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = attachment_handler_image_constructed;
}

static void
e_attachment_handler_image_init (EAttachmentHandlerImage *handler)
{
	handler->priv = E_ATTACHMENT_HANDLER_IMAGE_GET_PRIVATE (handler);
}
