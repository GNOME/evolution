/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-image-chooser.c
 * Copyright (C) 2004  Novell, Inc.
 * Author: Chris Toshok <toshok@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <stdio.h>
#include <string.h>

#include <gtk/gtkalignment.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkdnd.h>

#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnome/gnome-i18n.h>

#include "e-image-chooser.h"
#include "e-util/e-util-marshal.h"

struct _EImageChooserPrivate {

	GtkWidget *frame;
	GtkWidget *image;
	GtkWidget *browse_button;

	char *image_buf;
	int   image_buf_size;
	int   image_width;
	int   image_height;

	gboolean editable;
};

enum {
	CHANGED,
	LAST_SIGNAL
};


static gint image_chooser_signals [LAST_SIGNAL] = { 0 };

static void e_image_chooser_init	 (EImageChooser		 *chooser);
static void e_image_chooser_class_init	 (EImageChooserClass	 *klass);
#if 0
static void e_image_chooser_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void e_image_chooser_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
#endif
static void e_image_chooser_dispose      (GObject *object);

static gboolean image_drag_motion_cb (GtkWidget *widget,
				      GdkDragContext *context,
				      gint x, gint y, guint time, EImageChooser *chooser);
static void image_drag_leave_cb (GtkWidget *widget,
				 GdkDragContext *context,
				 guint time, EImageChooser *chooser);
static gboolean image_drag_drop_cb (GtkWidget *widget,
				    GdkDragContext *context,
				    gint x, gint y, guint time, EImageChooser *chooser);
static void image_drag_data_received_cb (GtkWidget *widget,
					 GdkDragContext *context,
					 gint x, gint y,
					 GtkSelectionData *selection_data,
					 guint info, guint time, EImageChooser *chooser);

static GtkObjectClass *parent_class = NULL;
#define PARENT_TYPE GTK_TYPE_VBOX

enum DndTargetType {
	DND_TARGET_TYPE_URI_LIST
};
#define URI_LIST_TYPE "text/uri-list"

static GtkTargetEntry image_drag_types[] = {
	{ URI_LIST_TYPE, 0, DND_TARGET_TYPE_URI_LIST },
};
static const int num_image_drag_types = sizeof (image_drag_types) / sizeof (image_drag_types[0]);

GtkWidget *
e_image_chooser_new (void)
{
	return g_object_new (E_TYPE_IMAGE_CHOOSER, NULL);
}

GType
e_image_chooser_get_type (void)
{
	static GType eic_type = 0;

	if (!eic_type) {
		static const GTypeInfo eic_info =  {
			sizeof (EImageChooserClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_image_chooser_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EImageChooser),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_image_chooser_init,
		};

		eic_type = g_type_register_static (PARENT_TYPE, "EImageChooser", &eic_info, 0);
	}

	return eic_type;
}


static void
e_image_chooser_class_init (EImageChooserClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_ref (PARENT_TYPE);

	image_chooser_signals [CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EImageChooserClass, changed),
			      NULL, NULL,
			      e_util_marshal_NONE__NONE,
			      GTK_TYPE_NONE, 0);

	/*
	object_class->set_property = e_image_chooser_set_property;
	object_class->get_property = e_image_chooser_get_property;
	*/
	object_class->dispose = e_image_chooser_dispose;
}

#if UI_CHANGE_OK
static void
browse_for_image_cb (GtkWidget *button, gpointer data)
{
}
#endif

static void
e_image_chooser_init (EImageChooser *chooser)
{
	EImageChooserPrivate *priv;
	GtkWidget *alignment;

	priv = chooser->priv = g_new0 (EImageChooserPrivate, 1);

	alignment = gtk_alignment_new (0, 0, 0, 0);
	priv->frame = gtk_frame_new ("");
	priv->image = gtk_image_new ();

	gtk_container_add (GTK_CONTAINER (alignment), priv->image);

#if UI_CHANGE_OK
	priv->browse_button = gtk_button_new_with_label (_("Choose Image"));
#endif

	gtk_frame_set_shadow_type (GTK_FRAME (priv->frame), GTK_SHADOW_NONE);

	gtk_container_add (GTK_CONTAINER (priv->frame), alignment);
	gtk_box_set_homogeneous (GTK_BOX (chooser), FALSE);
	gtk_box_pack_start (GTK_BOX (chooser), priv->frame, TRUE, TRUE, 0);
#if UI_CHANGE_OK
	gtk_box_pack_start (GTK_BOX (chooser), priv->browse_button, FALSE, FALSE, 0);

	g_signal_connect (priv->browse_button, "clicked", G_CALLBACK (browse_for_image_cb), NULL);
#endif

	gtk_drag_dest_set (priv->image, 0, image_drag_types, num_image_drag_types, GDK_ACTION_COPY);
	g_signal_connect (priv->image,
			  "drag_motion", G_CALLBACK (image_drag_motion_cb), chooser);
	g_signal_connect (priv->image,
			  "drag_leave", G_CALLBACK (image_drag_leave_cb), chooser);
	g_signal_connect (priv->image,
			  "drag_drop", G_CALLBACK (image_drag_drop_cb), chooser);
	g_signal_connect (priv->image,
			  "drag_data_received", G_CALLBACK (image_drag_data_received_cb), chooser);

	gtk_widget_show_all (priv->frame);
#if UI_CHANGE_OK
	gtk_widget_show (priv->browse_button);
#endif

	/* we default to being editable */
	priv->editable = TRUE;
}

static void
e_image_chooser_dispose (GObject *object)
{
	EImageChooser *eic = E_IMAGE_CHOOSER (object);

	if (eic->priv) {
		EImageChooserPrivate *priv = eic->priv;

		if (priv->image_buf) {
			g_free (priv->image_buf);
			priv->image_buf = NULL;
		}

		g_free (eic->priv);
		eic->priv = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}


static gboolean
set_image_from_data (EImageChooser *chooser,
		     char *data, int length)
{
	gboolean rv = FALSE;
	GdkPixbufLoader *loader = gdk_pixbuf_loader_new ();
	GdkPixbuf *pixbuf;

	gdk_pixbuf_loader_write (loader, data, length, NULL);

	pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
	if (pixbuf)
		g_object_ref (pixbuf);
	gdk_pixbuf_loader_close (loader, NULL);
	g_object_unref (loader);

	if (pixbuf) {
		GdkPixbuf *scaled;
		GdkPixbuf *composite;

		float scale;
		int new_height, new_width;

		new_height = gdk_pixbuf_get_height (pixbuf);
		new_width = gdk_pixbuf_get_width (pixbuf);

		printf ("new dimensions = (%d,%d)\n", new_width, new_height);

		if (chooser->priv->image_height == 0
		    && chooser->priv->image_width == 0) {
			printf ("initial setting of an image.  no scaling\n");
			scale = 1.0;
		}
		else if (chooser->priv->image_height < new_height
			 || chooser->priv->image_width < new_width) {
			/* we need to scale down */
			printf ("we need to scale down\n");
			if (new_height > new_width)
				scale = (float)chooser->priv->image_height / new_height;
			else
				scale = (float)chooser->priv->image_width / new_width;
		}
		else {
			/* we need to scale up */
			printf ("we need to scale up\n");
			if (new_height > new_width)
				scale = (float)new_height / chooser->priv->image_height;
			else
				scale = (float)new_width / chooser->priv->image_width;
		}

		printf ("scale = %g\n", scale);

		if (scale == 1.0) {
			gtk_image_set_from_pixbuf (GTK_IMAGE (chooser->priv->image), pixbuf);

			chooser->priv->image_width = new_width;
			chooser->priv->image_height = new_height;
		}
		else {
			new_width *= scale;
			new_height *= scale;
			new_width = MIN (new_width, chooser->priv->image_width);
			new_height = MIN (new_height, chooser->priv->image_height);

			printf ("new scaled dimensions = (%d,%d)\n", new_width, new_height);

			scaled = gdk_pixbuf_scale_simple (pixbuf,
							  new_width, new_height,
							  GDK_INTERP_BILINEAR);

			composite = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, gdk_pixbuf_get_bits_per_sample (pixbuf),
						    chooser->priv->image_width, chooser->priv->image_height);

			gdk_pixbuf_fill (composite, 0x00000000);

			gdk_pixbuf_copy_area (scaled, 0, 0, new_width, new_height,
					      composite,
					      chooser->priv->image_width / 2 - new_width / 2,
					      chooser->priv->image_height / 2 - new_height / 2);

			gtk_image_set_from_pixbuf (GTK_IMAGE (chooser->priv->image), composite);
			g_object_unref (scaled);
			g_object_unref (composite);
		}

		g_object_unref (pixbuf);

		g_free (chooser->priv->image_buf);
		chooser->priv->image_buf = data;
		chooser->priv->image_buf_size = length;

		g_signal_emit (chooser,
			       image_chooser_signals [CHANGED], 0);

		rv = TRUE;
	}

	return rv;
}

static gboolean
image_drag_motion_cb (GtkWidget *widget,
		      GdkDragContext *context,
		      gint x, gint y, guint time, EImageChooser *chooser)
{
	GList *p;

	if (!chooser->priv->editable)
		return FALSE;

	for (p = context->targets; p != NULL; p = p->next) {
		char *possible_type;

		possible_type = gdk_atom_name (GDK_POINTER_TO_ATOM (p->data));
		if (!strcmp (possible_type, URI_LIST_TYPE)) {
			g_free (possible_type);
			gdk_drag_status (context, GDK_ACTION_COPY, time);
			gtk_frame_set_shadow_type (GTK_FRAME (chooser->priv->frame), GTK_SHADOW_IN);
			return TRUE;
		}

		g_free (possible_type);
	}

	gtk_frame_set_shadow_type (GTK_FRAME (chooser->priv->frame), GTK_SHADOW_NONE);
	return FALSE;
}

static void
image_drag_leave_cb (GtkWidget *widget,
		     GdkDragContext *context,
		     guint time, EImageChooser *chooser)
{
	gtk_frame_set_shadow_type (GTK_FRAME (chooser->priv->frame), GTK_SHADOW_NONE);
}

static gboolean
image_drag_drop_cb (GtkWidget *widget,
		    GdkDragContext *context,
		    gint x, gint y, guint time, EImageChooser *chooser)
{
	GList *p;

	if (!chooser->priv->editable)
		return FALSE;

	if (context->targets == NULL) {
		gtk_frame_set_shadow_type (GTK_FRAME (chooser->priv->frame), GTK_SHADOW_NONE);
		return FALSE;
	}

	for (p = context->targets; p != NULL; p = p->next) {
		char *possible_type;

		possible_type = gdk_atom_name (GDK_POINTER_TO_ATOM (p->data));
		if (!strcmp (possible_type, URI_LIST_TYPE)) {
			g_free (possible_type);
			gtk_drag_get_data (widget, context,
					   GDK_POINTER_TO_ATOM (p->data),
					   time);
			gtk_frame_set_shadow_type (GTK_FRAME (chooser->priv->frame), GTK_SHADOW_NONE);
			return TRUE;
		}

		g_free (possible_type);
	}

	gtk_frame_set_shadow_type (GTK_FRAME (chooser->priv->frame), GTK_SHADOW_NONE);
	return FALSE;
}

static void
image_drag_data_received_cb (GtkWidget *widget,
			     GdkDragContext *context,
			     gint x, gint y,
			     GtkSelectionData *selection_data,
			     guint info, guint time, EImageChooser *chooser)
{
	char *target_type;
	gboolean handled = FALSE;

	target_type = gdk_atom_name (selection_data->target);

	if (!strcmp (target_type, URI_LIST_TYPE)) {
		GnomeVFSResult result;
		GnomeVFSHandle *handle;
		char *uri;
		char *nl = strstr (selection_data->data, "\r\n");
		char *buf = NULL;
		GnomeVFSFileInfo info;

		if (nl)
			uri = g_strndup (selection_data->data, nl - (char*)selection_data->data);
		else
			uri = g_strdup (selection_data->data);

		result = gnome_vfs_open (&handle, uri, GNOME_VFS_OPEN_READ);
		if (result == GNOME_VFS_OK) {
			result = gnome_vfs_get_file_info_from_handle (handle, &info, GNOME_VFS_FILE_INFO_DEFAULT);
			if (result == GNOME_VFS_OK) {
				GnomeVFSFileSize num_read;

				buf = g_malloc (info.size);

				if ((result = gnome_vfs_read (handle, buf, info.size, &num_read)) == GNOME_VFS_OK) {
					if (set_image_from_data (chooser, buf, num_read)) {
						handled = TRUE;
					} else {
						/* XXX we should pop up a warning dialog here */
						g_free (buf);
					}
				} else {
					g_free (buf);
				}
			}

			gnome_vfs_close (handle);
		}
		else {
			printf ("gnome_vfs_open failed (%s)\n", gnome_vfs_result_to_string (result));
		}

		g_free (uri);
	}

	gtk_drag_finish (context, handled, FALSE, time);
}



gboolean
e_image_chooser_set_from_file (EImageChooser *chooser, const char *filename)
{
	gchar *data;
	gsize data_length;

	g_return_val_if_fail (E_IS_IMAGE_CHOOSER (chooser), FALSE);
	g_return_val_if_fail (filename, FALSE);

	if (!g_file_get_contents (filename, &data, &data_length, NULL)) {
		return FALSE;
	}

	if (!set_image_from_data (chooser, data, data_length))
		g_free (data);

	return TRUE;
}

void
e_image_chooser_set_editable (EImageChooser *chooser, gboolean editable)
{
	g_return_if_fail (E_IS_IMAGE_CHOOSER (chooser));

	chooser->priv->editable = editable;

	gtk_widget_set_sensitive (chooser->priv->browse_button, editable);
}

gboolean
e_image_chooser_get_image_data (EImageChooser *chooser, char **data, gsize *data_length)
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
e_image_chooser_set_image_data (EImageChooser *chooser, char *data, gsize data_length)
{
	char *buf;

	g_return_val_if_fail (E_IS_IMAGE_CHOOSER (chooser), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);

	/* yuck, a copy... */
	buf = g_malloc (data_length);
	memcpy (buf, data, data_length);

	if (!set_image_from_data (chooser, buf, data_length)) {
		g_free (buf);
		return FALSE;
	}

	return TRUE;
}
