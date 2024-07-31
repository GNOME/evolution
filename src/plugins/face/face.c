/*
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
 * Authors:
 *		Sankar P <psankar@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "composer/e-msg-composer.h"
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <mail/em-event.h>

#define d(x)

#define SETTINGS_KEY "insert-face-picture"

/* see http://quimby.gnus.org/circus/face/ */
#define MAX_PNG_DATA_LENGTH 723

static gboolean
get_include_face_by_default (void)
{
	GSettings *settings = e_util_ref_settings ("org.gnome.evolution.plugin.face-picture");
	gboolean res;

	res = g_settings_get_boolean (settings, SETTINGS_KEY);

	g_object_unref (settings);

	return res;
}

static void
set_include_face_by_default (gboolean value)
{
	GSettings *settings = e_util_ref_settings ("org.gnome.evolution.plugin.face-picture");

	g_settings_set_boolean (settings, SETTINGS_KEY, value);

	g_object_unref (settings);
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
set_face_raw (gchar *content,
              gsize length)
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
get_active_face (gsize *image_data_length)
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
		if (res) {
			g_object_ref (res);
			if (image_data_length)
				*image_data_length = data_len;
		}
	}

	g_object_unref (loader);

	g_free (data);

	return res;
}

static gboolean
prepare_image (const gchar *image_filename,
               gchar **file_contents,
               gsize *length,
               GdkPixbuf **use_pixbuf,
               gboolean can_claim)
{
	gboolean res = FALSE;

	g_return_val_if_fail (image_filename != NULL, FALSE);
	g_return_val_if_fail (file_contents != NULL, FALSE);
	g_return_val_if_fail (length != NULL, FALSE);

	if (e_util_can_preview_filename (image_filename) &&
	    g_file_get_contents (image_filename, file_contents, length, NULL)) {
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

			if (error != NULL)
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
update_preview_cb (GtkFileChooser *file_chooser,
                   gpointer data)
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
choose_new_face (GtkWidget *parent,
		 gsize *image_data_length)
{
	GtkFileChooserNative *native;
	GdkPixbuf *res = NULL;
	GtkWidget *preview;
	GtkFileFilter *filter;

	native = gtk_file_chooser_native_new (
		_("Select a Face Picture"),
		GTK_IS_WINDOW (parent) ? GTK_WINDOW (parent) : NULL,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		_("_Open"), _("_Cancel"));

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("Image files"));
	gtk_file_filter_add_mime_type (filter, "image/*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (native), filter);

	preview = gtk_image_new ();
	gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (native), preview);
	g_signal_connect (
		native, "update-preview",
		G_CALLBACK (update_preview_cb), preview);

	if (GTK_RESPONSE_ACCEPT == gtk_native_dialog_run (GTK_NATIVE_DIALOG (native))) {
		gchar *image_filename, *file_contents = NULL;
		gsize length = 0;

		image_filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (native));
		g_object_unref (native);

		if (prepare_image (image_filename, &file_contents, &length, &res, TRUE)) {
			set_face_raw (file_contents, length);
			if (image_data_length)
				*image_data_length = length;
		}

		g_free (file_contents);
		g_free (image_filename);
	} else {
		g_object_unref (native);
	}

	return res;
}

static void
toggled_check_include_by_default_cb (GtkWidget *widget,
                                     gpointer data)
{
	set_include_face_by_default (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
}

static EAlert *
face_create_byte_size_alert (gsize byte_size)
{
	EAlert *alert;
	gchar *str;

	str = g_strdup_printf ("%" G_GSIZE_FORMAT, byte_size);
	alert = e_alert_new ("org.gnome.evolution.plugins.face:incorrect-image-byte-size", str, NULL);
	g_free (str);

	return alert;
}

static void
click_load_face_cb (GtkButton *butt,
                    GtkImage *image)
{
	EAlertBar *alert_bar;
	GdkPixbuf *face;
	gsize image_data_length = 0;

	alert_bar = g_object_get_data (G_OBJECT (butt), "alert-bar");
	e_alert_bar_clear (alert_bar);

	face = choose_new_face (gtk_widget_get_toplevel (GTK_WIDGET (butt)), &image_data_length);

	if (face) {
		gtk_image_set_from_pixbuf (image, face);
		g_object_unref (face);

		if (image_data_length > MAX_PNG_DATA_LENGTH) {
			EAlert *alert;

			alert = face_create_byte_size_alert (image_data_length);
			e_alert_bar_add_alert (alert_bar, alert);
			g_clear_object (&alert);
		}
	}
}

static GtkWidget *
get_cfg_widget (void)
{
	GtkWidget *vbox, *check, *img, *butt, *alert_bar;
	GdkPixbuf *face;
	gsize image_data_length = 0;

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

	check = gtk_check_button_new_with_mnemonic (_("_Insert Face picture by default"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), get_include_face_by_default ());
	g_signal_connect (
		check, "toggled",
		G_CALLBACK (toggled_check_include_by_default_cb), NULL);

	gtk_box_pack_start (GTK_BOX (vbox), check, FALSE, FALSE, 0);

	face = get_active_face (&image_data_length);
	img = gtk_image_new_from_pixbuf (face);
	if (face)
		g_object_unref (face);

	butt = gtk_button_new_with_mnemonic (_("Load new _Face picture"));
	g_signal_connect (
		butt, "clicked",
		G_CALLBACK (click_load_face_cb), img);

	alert_bar = e_alert_bar_new ();
	g_object_set_data (G_OBJECT (butt), "alert-bar", alert_bar);

	gtk_box_pack_start (GTK_BOX (vbox), butt, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), img, FALSE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (vbox), alert_bar, FALSE, FALSE, 0);

	gtk_widget_show_all (vbox);
	gtk_widget_hide (alert_bar);

	if (image_data_length > MAX_PNG_DATA_LENGTH) {
		EAlert *alert;

		alert = face_create_byte_size_alert (image_data_length);
		e_alert_bar_add_alert (E_ALERT_BAR (alert_bar), alert);
		g_clear_object (&alert);
	}

	return vbox;
}

static void
face_change_image_in_composer_cb (GtkButton *button,
				  EMsgComposer *composer);

static void
face_manage_composer_alert (EMsgComposer *composer,
			    gsize image_data_length)
{
	EHTMLEditor *editor;
	EAlert *alert;

	editor = e_msg_composer_get_editor (composer);

	if (image_data_length > MAX_PNG_DATA_LENGTH) {
		GtkWidget *button;

		alert = face_create_byte_size_alert (image_data_length);

		button = gtk_button_new_with_label (_("Change Face Image"));
		gtk_widget_show (button);
		g_signal_connect (button, "clicked", G_CALLBACK (face_change_image_in_composer_cb), composer);
		e_alert_add_widget (alert, button);

		e_alert_sink_submit_alert (E_ALERT_SINK (editor), alert);
		g_object_set_data_full (G_OBJECT (editor), "face-image-alert", alert, g_object_unref);
	} else {
		alert = g_object_get_data (G_OBJECT (editor), "face-image-alert");
		if (alert) {
			e_alert_response (alert, GTK_RESPONSE_CLOSE);
			g_object_set_data (G_OBJECT (editor), "face-image-alert", NULL);
		}
	}
}

static void
face_change_image_in_composer_cb (GtkButton *button,
				  EMsgComposer *composer)
{
	GdkPixbuf *pixbuf;
	gsize image_data_length = 0;

	/* Hide any previous alerts first */
	face_manage_composer_alert (composer, 0);

	pixbuf = choose_new_face (GTK_WIDGET (composer), &image_data_length);

	if (pixbuf) {
		g_object_unref (pixbuf);

		face_manage_composer_alert (composer, image_data_length);
	}
}

static void
action_toggle_face_cb (EUIAction *action,
		       GVariant *parameter,
		       gpointer user_data)
{
	EMsgComposer *composer = user_data;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	e_ui_action_set_state (action, parameter);

	if (e_ui_action_get_active (action)) {
		gsize image_data_length = 0;
		gchar *face = get_face_base64 ();

		if (!face) {
			GdkPixbuf *pixbuf = choose_new_face (GTK_WIDGET (composer), &image_data_length);

			if (pixbuf) {
				g_object_unref (pixbuf);
			} else {
				/* cannot load a face image, uncheck the option */
				e_ui_action_set_active (action, FALSE);
			}
		} else {
			g_free (g_base64_decode (face, &image_data_length));
			g_free (face);
		}

		face_manage_composer_alert (composer, image_data_length);
	} else {
		face_manage_composer_alert (composer, 0);
	}
}

/* ----------------------------------------------------------------- */

gint e_plugin_lib_enable (EPlugin *ep, gint enable);
gboolean e_plugin_ui_init (EUIManager *manager, EMsgComposer *composer);
GtkWidget *e_plugin_lib_get_configure_widget (EPlugin *epl);
void face_handle_send (EPlugin *ep, EMEventTargetComposer *target);

/* ----------------------------------------------------------------- */

gint
e_plugin_lib_enable (EPlugin *ep,
                     gint enable)
{
	return 0;
}

gboolean
e_plugin_ui_init (EUIManager *manager,
                  EMsgComposer *composer)
{
	static const gchar *eui =
		"<eui>"
		  "<menu id='main-menu'>"
		    "<submenu action='insert-menu'>"
		      "<placeholder id='insert-menu-top'>"
			"<item action='face-plugin'/>"
		      "</placeholder>"
		    "</submenu>"
		  "</menu>"
		"</eui>";

	static const EUIActionEntry entries[] = {
		{ "face-plugin",
		NULL,
		N_("Include _Face"),
		NULL,
		NULL,
		NULL, NULL, "false", action_toggle_face_cb }
	};

	EHTMLEditor *editor;
	EUIManager *ui_manager;

	editor = e_msg_composer_get_editor (composer);
	ui_manager = e_html_editor_get_ui_manager (editor);

	e_ui_manager_add_actions_with_eui_data (ui_manager, "composer", GETTEXT_PACKAGE,
		entries, G_N_ELEMENTS (entries), composer, eui);

	if (get_include_face_by_default ()) {
		EUIAction *action;
		gsize image_data_length = 0;
		gchar *face = get_face_base64 ();

		if (face) {
			action = e_html_editor_get_action (editor, "face-plugin");
			e_ui_action_set_active (action, TRUE);

			g_free (g_base64_decode (face, &image_data_length));
			g_free (face);
		}

		face_manage_composer_alert (composer, image_data_length);
	}

	return TRUE;
}

GtkWidget *
e_plugin_lib_get_configure_widget (EPlugin *epl)
{
	return get_cfg_widget ();
}

void
face_handle_send (EPlugin *ep,
                  EMEventTargetComposer *target)
{
	EHTMLEditor *editor;
	EUIAction *action;

	editor = e_msg_composer_get_editor (target->composer);
	action = e_html_editor_get_action (editor, "face-plugin");

	g_return_if_fail (action != NULL);

	if (e_ui_action_get_active (action)) {
		gchar *face = get_face_base64 ();

		if (face)
			e_msg_composer_set_header (target->composer, "Face", face);

		g_free (face);
	}
}
