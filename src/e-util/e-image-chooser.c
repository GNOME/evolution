/*
 * e-image-chooser.c
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

#include <stdio.h>
#include <string.h>

#include <glib/gi18n.h>

#include "e-image-chooser.h"

#include "e-icon-factory.h"

struct _EImageChooserPrivate {
	GtkWidget *image;

	gchar *image_buf;
	gint image_buf_size;

	/* Default Image */
	gchar *icon_name;

	gboolean has_image;
	guint pixel_size;
};

enum {
	PROP_0,
	PROP_ICON_NAME,
	PROP_HAS_IMAGE,
	PROP_PIXEL_SIZE,
	N_PROPS
};

enum {
	CHANGED,
	LAST_SIGNAL
};

static GParamSpec *properties[N_PROPS] = { NULL, };
static guint signals[LAST_SIGNAL];

#define URI_LIST_TYPE "text/uri-list"

G_DEFINE_TYPE_WITH_PRIVATE (EImageChooser, e_image_chooser, GTK_TYPE_BOX)

static gboolean
set_image_from_data (EImageChooser *chooser,
                     gchar *data,
                     gint length)
{
	GdkPixbufLoader *loader;
	GdkPixbuf *pixbuf;
	gfloat scale;
	gint new_height;
	gint new_width;

	loader = gdk_pixbuf_loader_new ();
	gdk_pixbuf_loader_write (loader, (guchar *) data, length, NULL);
	gdk_pixbuf_loader_close (loader, NULL);

	pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
	if (pixbuf)
		g_object_ref (pixbuf);

	g_object_unref (loader);

	if (pixbuf == NULL)
		return FALSE;

	new_height = gdk_pixbuf_get_height (pixbuf);
	new_width = gdk_pixbuf_get_width (pixbuf);

	if (chooser->priv->pixel_size == 0) {
		gint width, height = 64;

		gtk_icon_size_lookup (GTK_ICON_SIZE_DIALOG, &width, &height);

		chooser->priv->pixel_size = height;
	}

	if (chooser->priv->pixel_size == new_height &&
	    chooser->priv->pixel_size == new_width) {
		scale = 1.0;
	} else if (chooser->priv->pixel_size < new_height ||
		   chooser->priv->pixel_size < new_width) {
		/* we need to scale down */
		if (new_height > new_width)
			scale = (gfloat) chooser->priv->pixel_size / new_height;
		else
			scale = (gfloat) chooser->priv->pixel_size / new_width;
	} else {
		/* we need to scale up */
		if (new_height > new_width)
			scale = (gfloat) new_height / chooser->priv->pixel_size;
		else
			scale = (gfloat) new_width / chooser->priv->pixel_size;
	}

	if (scale == 1.0) {
		gtk_image_set_from_pixbuf (GTK_IMAGE (chooser->priv->image), pixbuf);
	} else {
		GdkPixbuf *scaled;
		GdkPixbuf *composite;

		new_width *= scale;
		new_height *= scale;
		new_width = MIN (new_width, chooser->priv->pixel_size);
		new_height = MIN (new_height, chooser->priv->pixel_size);

		scaled = gdk_pixbuf_scale_simple (
			pixbuf, new_width, new_height,
			GDK_INTERP_BILINEAR);

		composite = gdk_pixbuf_new (
			GDK_COLORSPACE_RGB, TRUE,
			gdk_pixbuf_get_bits_per_sample (pixbuf),
			chooser->priv->pixel_size,
			chooser->priv->pixel_size);

		gdk_pixbuf_fill (composite, 0x00000000);

		gdk_pixbuf_copy_area (
			scaled, 0, 0, new_width, new_height,
			composite,
			chooser->priv->pixel_size / 2 - new_width / 2,
			chooser->priv->pixel_size / 2 - new_height / 2);

		gtk_image_set_from_pixbuf (
			GTK_IMAGE (chooser->priv->image), composite);

		g_object_unref (scaled);
		g_object_unref (composite);
	}

	g_object_unref (pixbuf);

	g_free (chooser->priv->image_buf);
	chooser->priv->image_buf = data;
	chooser->priv->image_buf_size = length;

	g_signal_emit (chooser, signals[CHANGED], 0);

	return TRUE;
}

static gboolean
image_drag_motion_cb (GtkWidget *widget,
                      GdkDragContext *context,
                      gint x,
                      gint y,
                      guint time,
                      EImageChooser *chooser)
{
	GList *targets, *p;

	targets = gdk_drag_context_list_targets (context);

	for (p = targets; p != NULL; p = p->next) {
		gchar *possible_type;

		possible_type = gdk_atom_name (GDK_POINTER_TO_ATOM (p->data));
		if (!strcmp (possible_type, URI_LIST_TYPE)) {
			g_free (possible_type);
			gdk_drag_status (context, GDK_ACTION_COPY, time);
			return TRUE;
		}

		g_free (possible_type);
	}

	return FALSE;
}

static void
image_drag_leave_cb (GtkWidget *widget,
                     GdkDragContext *context,
                     guint time,
                     EImageChooser *chooser)
{
}

static gboolean
image_drag_drop_cb (GtkWidget *widget,
                    GdkDragContext *context,
                    gint x,
                    gint y,
                    guint time,
                    EImageChooser *chooser)
{
	GList *targets, *p;

	targets = gdk_drag_context_list_targets (context);

	if (targets == NULL)
		return FALSE;

	for (p = targets; p != NULL; p = p->next) {
		gchar *possible_type;

		possible_type = gdk_atom_name (GDK_POINTER_TO_ATOM (p->data));
		if (!strcmp (possible_type, URI_LIST_TYPE)) {
			g_free (possible_type);
			gtk_drag_get_data (
				widget, context,
				GDK_POINTER_TO_ATOM (p->data), time);
			return TRUE;
		}

		g_free (possible_type);
	}

	return FALSE;
}

static void
image_chooser_file_loaded_cb (GFile *file,
                              GAsyncResult *result,
                              EImageChooser *chooser)
{
	gchar *contents;
	gsize length;
	GError *error = NULL;

	g_file_load_contents_finish (
		file, result, &contents, &length, NULL, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
		goto exit;
	}

	if (!set_image_from_data (chooser, contents, length))
		g_free (contents);

exit:
	g_object_unref (chooser);
}

static void
image_drag_data_received_cb (GtkWidget *widget,
                             GdkDragContext *context,
                             gint x,
                             gint y,
                             GtkSelectionData *selection_data,
                             guint info,
                             guint time,
                             EImageChooser *chooser)
{
	GFile *file;
	gboolean handled = FALSE;
	gchar **uris;

	uris = gtk_selection_data_get_uris (selection_data);

	if (uris == NULL)
		goto exit;

	file = g_file_new_for_uri (uris[0]);

	/* XXX Not cancellable. */
	g_file_load_contents_async (
		file, NULL, (GAsyncReadyCallback)
		image_chooser_file_loaded_cb,
		g_object_ref (chooser));

	g_object_unref (file);
	g_strfreev (uris);

	/* Assume success.  We won't know til later. */
	handled = TRUE;

exit:
	gtk_drag_finish (context, handled, FALSE, time);
}

static void
image_chooser_set_icon_name (EImageChooser *chooser,
                             const gchar *icon_name)
{
	GtkIconTheme *icon_theme;
	GtkIconInfo *icon_info;
	const gchar *filename;

	if (g_strcmp0 (chooser->priv->icon_name, icon_name) != 0) {
		g_free (chooser->priv->icon_name);
		chooser->priv->icon_name = g_strdup (icon_name);
	}

	icon_theme = gtk_icon_theme_get_default ();
	if (chooser->priv->pixel_size == 0) {
		gint width, height;
		gtk_icon_size_lookup (GTK_ICON_SIZE_DIALOG, &width, &height);
		chooser->priv->pixel_size = height;
	}

	icon_info = gtk_icon_theme_lookup_icon (icon_theme, icon_name, chooser->priv->pixel_size, 0);
	g_return_if_fail (icon_info != NULL);

	filename = gtk_icon_info_get_filename (icon_info);
	e_image_chooser_set_from_file (chooser, filename);
	g_object_unref (icon_info);
}

static void
image_chooser_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ICON_NAME:
			image_chooser_set_icon_name (
				E_IMAGE_CHOOSER (object),
				g_value_get_string (value));
			return;
		case PROP_PIXEL_SIZE:
			E_IMAGE_CHOOSER (object)->priv->pixel_size = g_value_get_uint (value);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
image_chooser_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ICON_NAME:
			g_value_set_string (
				value,
				e_image_chooser_get_icon_name (
				E_IMAGE_CHOOSER (object)));
			return;
		case PROP_HAS_IMAGE:
			g_value_set_boolean (value,
				e_image_chooser_has_image (E_IMAGE_CHOOSER (object)));
			return;
		case PROP_PIXEL_SIZE:
			g_value_set_uint (value,
				e_image_chooser_get_pixel_size (E_IMAGE_CHOOSER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
image_chooser_dispose (GObject *object)
{
	EImageChooser *self = E_IMAGE_CHOOSER (object);

	g_clear_object (&self->priv->image);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_image_chooser_parent_class)->dispose (object);
}

static void
image_chooser_finalize (GObject *object)
{
	EImageChooser *self = E_IMAGE_CHOOSER (object);

	g_free (self->priv->image_buf);
	g_free (self->priv->icon_name);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_image_chooser_parent_class)->finalize (object);
}

static void
e_image_chooser_class_init (EImageChooserClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = image_chooser_set_property;
	object_class->get_property = image_chooser_get_property;
	object_class->dispose = image_chooser_dispose;
	object_class->finalize = image_chooser_finalize;

	properties[PROP_ICON_NAME] = g_param_spec_string ("icon-name", NULL, NULL, "avatar-default",
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS);

	properties[PROP_HAS_IMAGE] = g_param_spec_boolean ("has-image", NULL, NULL, FALSE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS |
			G_PARAM_EXPLICIT_NOTIFY);

	properties[PROP_PIXEL_SIZE] = g_param_spec_uint ("pixel-size", NULL, NULL, 0, G_MAXUINT, 48,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);

	signals[CHANGED] = g_signal_new (
		"changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EImageChooserClass, changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_image_chooser_init (EImageChooser *chooser)
{
	GtkWidget *container;
	GtkWidget *widget;
	gint width, height;

	chooser->priv = e_image_chooser_get_instance_private (chooser);

	gtk_icon_size_lookup (GTK_ICON_SIZE_DIALOG, &width, &height);
	chooser->priv->pixel_size = height;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (chooser), GTK_ORIENTATION_VERTICAL);

	container = GTK_WIDGET (chooser);

	widget = gtk_image_new ();
	gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
	gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	chooser->priv->image = g_object_ref (widget);
	gtk_widget_show (widget);

	gtk_drag_dest_set (widget, 0, NULL, 0, GDK_ACTION_COPY);
	gtk_drag_dest_add_uri_targets (widget);

	g_signal_connect (
		widget, "drag-motion",
		G_CALLBACK (image_drag_motion_cb), chooser);
	g_signal_connect (
		widget, "drag-leave",
		G_CALLBACK (image_drag_leave_cb), chooser);
	g_signal_connect (
		widget, "drag-drop",
		G_CALLBACK (image_drag_drop_cb), chooser);
	g_signal_connect (
		widget, "drag-data-received",
		G_CALLBACK (image_drag_data_received_cb), chooser);
}

const gchar *
e_image_chooser_get_icon_name (EImageChooser *chooser)
{
	g_return_val_if_fail (E_IS_IMAGE_CHOOSER (chooser), NULL);

	return chooser->priv->icon_name;
}

GtkWidget *
e_image_chooser_new (const gchar *icon_name)
{
	g_return_val_if_fail (icon_name != NULL, NULL);

	return g_object_new (
		E_TYPE_IMAGE_CHOOSER,
		"icon-name", icon_name, NULL);
}

gboolean
e_image_chooser_set_from_file (EImageChooser *chooser,
                               const gchar *filename)
{
	gchar *data;
	gsize data_length;

	g_return_val_if_fail (E_IS_IMAGE_CHOOSER (chooser), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);

	if (!g_file_get_contents (filename, &data, &data_length, NULL))
		return FALSE;

	if (!set_image_from_data (chooser, data, data_length))
		g_free (data);

	if (!chooser->priv->has_image) {
		chooser->priv->has_image = TRUE;
		g_object_notify_by_pspec (G_OBJECT (chooser), properties[PROP_HAS_IMAGE]);
	}

	return TRUE;
}

gboolean
e_image_chooser_get_image_data (EImageChooser *chooser,
                                gchar **data,
                                gsize *data_length)
{
	g_return_val_if_fail (E_IS_IMAGE_CHOOSER (chooser), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (data_length != NULL, FALSE);

	*data_length = chooser->priv->image_buf_size;
	*data = g_malloc (*data_length);
	memcpy (*data, chooser->priv->image_buf, *data_length);

	return TRUE;
}

gboolean
e_image_chooser_set_image_data (EImageChooser *chooser,
                                gchar *data,
                                gsize data_length)
{
	gchar *buf;

	g_return_val_if_fail (E_IS_IMAGE_CHOOSER (chooser), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);

	/* yuck, a copy... */
	buf = g_malloc (data_length);
	memcpy (buf, data, data_length);

	if (!set_image_from_data (chooser, buf, data_length)) {
		g_free (buf);
		return FALSE;
	}

	if (!chooser->priv->has_image) {
		chooser->priv->has_image = TRUE;
		g_object_notify_by_pspec (G_OBJECT (chooser), properties[PROP_HAS_IMAGE]);
	}

	return TRUE;
}

void
e_image_chooser_unset_image (EImageChooser *chooser)
{
	g_return_if_fail (E_IS_IMAGE_CHOOSER (chooser));

	image_chooser_set_icon_name (chooser, chooser->priv->icon_name);

	if (chooser->priv->has_image) {
		chooser->priv->has_image = FALSE;
		g_object_notify_by_pspec (G_OBJECT (chooser), properties[PROP_HAS_IMAGE]);
	}
}

gboolean
e_image_chooser_has_image (EImageChooser *chooser)
{
	g_return_val_if_fail (E_IS_IMAGE_CHOOSER (chooser), FALSE);

	return chooser->priv->has_image;
}

guint
e_image_chooser_get_pixel_size (EImageChooser *chooser)
{
	g_return_val_if_fail (E_IS_IMAGE_CHOOSER (chooser), 0);

	return chooser->priv->pixel_size;
}
