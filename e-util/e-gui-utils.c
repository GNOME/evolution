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

#include "e-gui-utils.h"

#include <gtk/gtksignal.h>
#include <gtk/gtkalignment.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-util.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gnome-canvas-pixbuf.h>
#include <bonobo/bonobo-ui-util.h>

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

static GSList *inited_arrays=NULL;

static void
free_pixmaps (void)
{
	int i;
	GSList *li;

	for (li = inited_arrays; li != NULL; li = li->next) {
		EPixmap *pixcache = li->data;
		for (i = 0; pixcache [i].path; i++)
			g_free (pixcache [i].pixbuf);
	}
	
	g_slist_free(inited_arrays);
}

void e_pixmaps_update (BonoboUIComponent *uic, EPixmap *pixcache)
{
	static int done_init = 0;
	int i;

	if (!done_init) {
		g_atexit (free_pixmaps);
		done_init = 1;
	}

	if (g_slist_find(inited_arrays, pixcache) == NULL)
		inited_arrays = g_slist_prepend (inited_arrays, pixcache);

	for (i = 0; pixcache [i].path; i++) {
		if (!pixcache [i].pixbuf) {
			char *path;
			GdkPixbuf *pixbuf;

			path = g_concat_dir_and_file (EVOLUTION_IMAGES,
						      pixcache [i].fname);
			
			pixbuf = gdk_pixbuf_new_from_file (path);
			if (pixbuf == NULL) {
				g_warning ("Cannot load image -- %s", path);
			} else {
				pixcache [i].pixbuf = bonobo_ui_util_pixbuf_to_xml (pixbuf);
				gdk_pixbuf_unref (pixbuf);
			}
			
			g_free (path);
		}
		bonobo_ui_component_set_prop (uic, pixcache [i].path, "pixname",
					      pixcache [i].pixbuf, NULL);
	}
}

