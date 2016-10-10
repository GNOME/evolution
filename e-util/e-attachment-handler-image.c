/*
 * e-attachment-handler-image.c
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

#include "e-attachment-handler-image.h"

#include <glib/gi18n.h>
#include <gdesktop-enums.h>

#include "e-misc-utils.h"

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
	GDesktopBackgroundStyle style;
	EAttachmentView *view;
	GSettings *settings;
	GtkWidget *dialog;
	GFile *file;
	gpointer parent;
	gchar *uri;
	GError *error = NULL;

	view = e_attachment_handler_get_view (handler);
	settings = e_util_ref_settings ("org.gnome.desktop.background");

	file = e_attachment_save_finish (attachment, result, &error);

	if (error != NULL)
		goto error;

	uri = g_file_get_uri (file);
	g_settings_set_string (settings, "picture-uri", uri);
	g_free (uri);

	style = g_settings_get_enum (settings, "picture-options");
	if (style == G_DESKTOP_BACKGROUND_STYLE_NONE)
		g_settings_set_enum (
			settings, "picture-options",
			G_DESKTOP_BACKGROUND_STYLE_WALLPAPER);

	g_object_unref (file);

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
	g_object_unref (settings);
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
	const gchar *path;

	view = e_attachment_handler_get_view (handler);
	selected = e_attachment_view_get_selected_attachments (view);
	g_return_if_fail (g_list_length (selected) == 1);
	attachment = E_ATTACHMENT (selected->data);

	/* Save the image under the user's Pictures directory. */
	path = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
	destination = g_file_new_for_path (path);
	g_mkdir_with_parents (path, 0755);

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
	GtkActionGroup *action_group;
	gchar *mime_type;
	GList *selected;
	gboolean visible = FALSE;

	selected = e_attachment_view_get_selected_attachments (view);

	if (g_list_length (selected) != 1)
		goto exit;

	attachment = E_ATTACHMENT (selected->data);

	if (e_attachment_get_loading (attachment))
		goto exit;

	if (e_attachment_get_saving (attachment))
		goto exit;

	mime_type = e_attachment_dup_mime_type (attachment);
	visible =
		(mime_type != NULL) &&
		(g_ascii_strncasecmp (mime_type, "image/", 6) == 0);
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
	G_OBJECT_CLASS (e_attachment_handler_image_parent_class)->constructed (object);

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
