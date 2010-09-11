/*
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
 * Authors:
 *		Sankar P <psankar@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "composer/e-msg-composer.h"
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <mail/em-event.h>
#include <e-util/e-alert-dialog.h>
#include <e-util/e-util.h>
#include <e-util/e-icon-factory.h>

#define d(x)

#define SETTINGS_KEY "/apps/evolution/eplugin/face/insert_by_default"

static gboolean
get_include_face_by_default (void)
{
	GConfClient *gconf = gconf_client_get_default ();
	gboolean res;

	res = gconf_client_get_bool (gconf, SETTINGS_KEY, NULL);

	g_object_unref (gconf);

	return res;
}

static void
set_include_face_by_default (gboolean value)
{
	GConfClient *gconf = gconf_client_get_default ();

	gconf_client_set_bool (gconf, SETTINGS_KEY, value, NULL);

	g_object_unref (gconf);
}

static gchar *
get_filename (void)
{
	return g_build_filename (e_get_user_data_dir (), "faces", NULL);
}

static gchar *
get_face_base64 (void)
{
	gchar *filename = get_filename (), *file_contents = NULL;
	gsize length = 0;

	if (g_file_get_contents (filename, &file_contents, &length, NULL)) {
		if (length > 0) {
			file_contents = g_realloc (file_contents, length + 1);
			file_contents[length] = 0;
		} else {
			g_free (file_contents);
			file_contents = NULL;
		}
	} else {
		file_contents = NULL;
	}

	g_free (filename);

	return file_contents;
}

static void
set_face_raw (gchar *content, gsize length)
{
	gchar *filename = get_filename ();

	if (content) {
		gchar *file_contents;

		file_contents = g_base64_encode ((guchar *) content, length);
		g_file_set_contents (filename, file_contents, -1, NULL);
		g_free (file_contents);
	} else {
		g_file_set_contents (filename, "", -1, NULL);
	}

	g_free (filename);
}

/* g_object_unref returned pointer when done with it */
static GdkPixbuf *
get_active_face (void)
{
	GdkPixbufLoader *loader;
	GdkPixbuf *res = NULL;
	gchar *face;
	guchar *data;
	gsize data_len = 0;

	face = get_face_base64 ();

	if (!face || !*face) {
		g_free (face);
		return NULL;
	}

	data = g_base64_decode (face, &data_len);
	if (!data || !data_len) {
		g_free (face);
		g_free (data);
		return NULL;
	}

	g_free (face);

	loader = gdk_pixbuf_loader_new ();

	if (gdk_pixbuf_loader_write (loader, data, data_len, NULL)
	    && gdk_pixbuf_loader_close (loader, NULL)) {
		res = gdk_pixbuf_loader_get_pixbuf (loader);
		if (res)
			g_object_ref (res);
	}

	g_object_unref (loader);

	g_free (data);

	return res;
}

static gboolean
prepare_image (const gchar *image_filename, gchar **file_contents, gsize *length, GdkPixbuf **use_pixbuf, gboolean can_claim)
{
	gboolean res = FALSE;

	g_return_val_if_fail (image_filename != NULL, FALSE);
	g_return_val_if_fail (file_contents != NULL, FALSE);
	g_return_val_if_fail (length != NULL, FALSE);

	if (g_file_get_contents (image_filename, file_contents, length, NULL)) {
		GError *error = NULL;
		GdkPixbuf *pixbuf;
		GdkPixbufLoader *loader = gdk_pixbuf_loader_new ();

		if (!gdk_pixbuf_loader_write (loader, (const guchar *)(*file_contents), *length, &error)
		    || !gdk_pixbuf_loader_close (loader, &error)
		    || (pixbuf = gdk_pixbuf_loader_get_pixbuf (loader)) == NULL) {
			const gchar *err = _("Unknown error");

			if (error && error->message)
				err = error->message;

			if (can_claim)
				e_alert_run_dialog_for_args (NULL, "org.gnome.evolution.plugins.face:not-an-image", err, NULL);

			if (error)
				g_error_free (error);
		} else {
			gint width, height;

			height = gdk_pixbuf_get_height (pixbuf);
			width = gdk_pixbuf_get_width (pixbuf);

			if (height <= 0 || width <= 0) {
				if (can_claim)
					e_alert_run_dialog_for_args (NULL, "org.gnome.evolution.plugins.face:invalid-image-size", NULL, NULL);
			} else if (height != 48 || width != 48) {
				GdkPixbuf *copy, *scale;
				guchar *pixels;
				guint32 fill;

				if (width >= height) {
					if (width > 48) {
						gdouble ratio = (gdouble) width / 48.0;
						width = 48;
						height = height / ratio;

						if (height == 0)
							height = 1;
					}
				} else {
					if (height > 48) {
						gdouble ratio = (gdouble) height / 48.0;
						height = 48;
						width = width / ratio;
						if (width == 0)
							width = 1;
					}
				}

				scale = e_icon_factory_pixbuf_scale (pixbuf, width, height);
				copy = e_icon_factory_pixbuf_scale (pixbuf, 48, 48);

				width = gdk_pixbuf_get_width (scale);
				height = gdk_pixbuf_get_height (scale);

				pixels = gdk_pixbuf_get_pixels (scale);
				/* fill with a pixel color at [0,0] */
				fill = (pixels[0] << 24) | (pixels[1] << 16) | (pixels[2] << 8) | (pixels[0]);
				gdk_pixbuf_fill (copy, fill);

				gdk_pixbuf_copy_area (scale, 0, 0, width, height, copy, width < 48 ? (48 - width) / 2 : 0, height < 48 ? (48 - height) / 2 : 0);

				g_free (*file_contents);
				*file_contents = NULL;
				*length = 0;

				res = gdk_pixbuf_save_to_buffer (copy, file_contents, length, "png", NULL, "compression", "9", NULL);

				if (res && use_pixbuf)
					*use_pixbuf = g_object_ref (copy);
				g_object_unref (copy);
				g_object_unref (scale);
			} else {
				res = TRUE;
				if (use_pixbuf)
					*use_pixbuf = g_object_ref (pixbuf);
			}
		}

		g_object_unref (loader);
	} else {
		if (can_claim)
			e_alert_run_dialog_for_args (NULL, "org.gnome.evolution.plugins.face:file-not-found", NULL, NULL);
	}

	return res;
}

static void
update_preview_cb (GtkFileChooser *file_chooser, gpointer data)
{
	GtkWidget *preview;
	gchar *filename, *file_contents = NULL;
	GdkPixbuf *pixbuf = NULL;
	gboolean have_preview;
	gsize length = 0;

	preview = GTK_WIDGET (data);
	filename = gtk_file_chooser_get_preview_filename (file_chooser);

	have_preview = filename && prepare_image (filename, &file_contents, &length, &pixbuf, FALSE);
	if (have_preview) {
		g_free (file_contents);
		have_preview = pixbuf != NULL;
	}

	g_free (filename);

	gtk_image_set_from_pixbuf (GTK_IMAGE (preview), pixbuf);
	if (pixbuf)
		g_object_unref (pixbuf);

	gtk_file_chooser_set_preview_widget_active (file_chooser, have_preview);
}

static GdkPixbuf *
choose_new_face (void)
{
	GdkPixbuf *res = NULL;
	GtkWidget *filesel, *preview;
	GtkFileFilter *filter;

	filesel = gtk_file_chooser_dialog_new (_
				("Select a png picture (the best 48*48 of size < 720 bytes)"),
				NULL,
				GTK_FILE_CHOOSER_ACTION_OPEN,
				GTK_STOCK_CANCEL,
				GTK_RESPONSE_CANCEL,
				GTK_STOCK_OPEN, GTK_RESPONSE_OK, NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (filesel), GTK_RESPONSE_OK);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("Image files"));
	gtk_file_filter_add_mime_type (filter, "image/*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (filesel), filter);

	preview = gtk_image_new ();
	gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (filesel), preview);
	g_signal_connect (filesel, "update-preview", G_CALLBACK (update_preview_cb), preview);

	if (GTK_RESPONSE_OK == gtk_dialog_run (GTK_DIALOG (filesel))) {
		gchar *image_filename, *file_contents = NULL;
		gsize length = 0;

		image_filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filesel));
		gtk_widget_destroy (filesel);

		if (prepare_image (image_filename, &file_contents, &length, &res, TRUE)) {
			set_face_raw (file_contents, length);
		}

		g_free (file_contents);
		g_free (image_filename);
	} else {
		gtk_widget_destroy (filesel);
	}

	return res;
}

static void
toggled_check_include_by_default_cb (GtkWidget *widget, gpointer data)
{
	set_include_face_by_default (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
}

static void
click_load_face_cb (GtkButton *butt, gpointer data)
{
	GdkPixbuf *face;
	GtkWidget *img;

	img = gtk_button_get_image (butt);
	g_return_if_fail (img != NULL);

	face = choose_new_face ();

	if (face) {
		gtk_image_set_from_pixbuf (GTK_IMAGE (img), face);
		g_object_unref (face);
	}
}

static GtkWidget *
get_cfg_widget (void)
{
	GtkWidget *vbox, *check, *img, *butt;
	GdkPixbuf *face;

	vbox = gtk_vbox_new (FALSE, 6);

	check = gtk_check_button_new_with_mnemonic (_("_Insert Face picture by default"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), get_include_face_by_default ());
	g_signal_connect (check, "toggled", G_CALLBACK (toggled_check_include_by_default_cb), NULL);

	gtk_box_pack_start (GTK_BOX (vbox), check, FALSE, FALSE, 0);

	face = get_active_face ();
	img = gtk_image_new_from_pixbuf (face);
	if (face)
		g_object_unref (face);

	butt = gtk_button_new_with_mnemonic (_("Load new _Face picture"));
	gtk_button_set_image (GTK_BUTTON (butt), img);
	g_signal_connect (butt, "clicked", G_CALLBACK (click_load_face_cb), NULL);

	gtk_box_pack_start (GTK_BOX (vbox), butt, FALSE, FALSE, 0);

	gtk_widget_show_all (vbox);

	return vbox;
}

static void
action_toggle_face_cb (GtkToggleAction *action, EMsgComposer *composer)
{
	if (gtk_toggle_action_get_active (action)) {
		gchar *face = get_face_base64 ();

		if (!face) {
			GdkPixbuf *pixbuf = choose_new_face ();

			if (pixbuf) {
				g_object_unref (pixbuf);
			} else {
				/* cannot load a face image, uncheck the option */
				gtk_toggle_action_set_active (action, FALSE);
			}
		} else {
			g_free (face);
		}
	}
}

/* ----------------------------------------------------------------- */

gint e_plugin_lib_enable (EPlugin *ep, gint enable);
gboolean e_plugin_ui_init (GtkUIManager *ui_manager, EMsgComposer *composer);
GtkWidget *e_plugin_lib_get_configure_widget (EPlugin *epl);
void face_handle_send (EPlugin *ep, EMEventTargetComposer *target);

/* ----------------------------------------------------------------- */

gint
e_plugin_lib_enable (EPlugin *ep, gint enable)
{
	return 0;
}

gboolean
e_plugin_ui_init (GtkUIManager *ui_manager,
		  EMsgComposer *composer)
{
	GtkhtmlEditor *editor;

	static GtkToggleActionEntry entries[] = {
		{ "face-plugin",
		NULL,
		N_("Include _Face"),
		NULL,
		NULL,
		G_CALLBACK (action_toggle_face_cb),
		FALSE }
	};

	if (get_include_face_by_default ()) {
		gchar *face = get_face_base64 ();

		/* activate it only if has a face image available */
		entries[0].is_active = face && *face;

		g_free (face);
	}

	editor = GTKHTML_EDITOR (composer);

	/* Add actions to the "composer" action group. */
	gtk_action_group_add_toggle_actions (
		gtkhtml_editor_get_action_group (editor, "composer"),
		entries, G_N_ELEMENTS (entries), composer);

	return TRUE;
}

GtkWidget *
e_plugin_lib_get_configure_widget (EPlugin *epl)
{
	return get_cfg_widget ();
}

void
face_handle_send (EPlugin *ep, EMEventTargetComposer *target)
{
	GtkhtmlEditor *editor;
	GtkAction *action;

	editor = GTKHTML_EDITOR (target->composer);
	action = gtkhtml_editor_get_action (editor, "face-plugin");

	g_return_if_fail (action != NULL);

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
		gchar *face = get_face_base64 ();

		if (face)
			e_msg_composer_modify_header (target->composer, "Face", face);

		g_free (face);
	}
}
