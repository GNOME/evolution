/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * GUI utility functions
 *
 * Author:
 *   Miguel de Icaza (miguel@helixcode.com)
 *
 * (C) 1999 Miguel de Icaza
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include <gtk/gtksignal.h>
#include <gnome.h>
#include "e-gui-utils.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gnome-canvas-pixbuf.h>

GtkWidget *e_create_image_widget(gchar *name,
				 gchar *string1, gchar *string2,
				 gint int1, gint int2)
{
	char *filename;
	GdkPixbuf *pixbuf;
	double width, height;
	GtkWidget *canvas, *alignment;
	if (string1) {
		if (*string1 == '/')
			filename = g_strdup(string1);
		else
			filename = g_concat_dir_and_file(EVOLUTION_IMAGES, string1);
		pixbuf = gdk_pixbuf_new_from_file(filename);
		width = gdk_pixbuf_get_width(pixbuf);
		height = gdk_pixbuf_get_height(pixbuf);

		canvas = gnome_canvas_new_aa();
		GTK_OBJECT_UNSET_FLAGS(GTK_WIDGET(canvas), GTK_CAN_FOCUS);
		gnome_canvas_item_new(gnome_canvas_root(GNOME_CANVAS(canvas)),
				      gnome_canvas_pixbuf_get_type(),
				      "pixbuf", pixbuf,
				      NULL);

		alignment = gtk_widget_new(gtk_alignment_get_type(),
					   "child", canvas,
					   "xalign", (double) 0,
					   "yalign", (double) 0,
					   "xscale", (double) 0,
					   "yscale", (double) 0,
					   NULL);
	
		gtk_widget_set_usize(canvas, width, height);

		gdk_pixbuf_unref(pixbuf);

		gtk_widget_show(canvas);
		gtk_widget_show(alignment);
		g_free(filename);

		return alignment;
	} else
		return NULL;
}
