/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * GUI utility functions
 *
 * Author:
 *   Miguel de Icaza (miguel@ximian.com)
 *
 * (C) 1999 Miguel de Icaza
 * (C) 2000 Ximian, Inc.
 */
#include <config.h>

#include "e-gui-utils.h"

#include <glib.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkimage.h>

GtkWidget *e_create_image_widget(gchar *name,
				 gchar *string1, gchar *string2,
				 gint int1, gint int2)
{
	char *filename;
	GtkWidget *alignment = NULL;
	if (string1) {
		GtkWidget *w;

		if (*string1 == '/')
			filename = g_strdup(string1);
		else
			filename = g_build_filename (EVOLUTION_IMAGES, string1, NULL);

		w = gtk_image_new_from_file (filename);

		alignment = gtk_widget_new(gtk_alignment_get_type(),
					   "child", w,
					   "xalign", (double) 0,
					   "yalign", (double) 0,
					   "xscale", (double) 0,
					   "yscale", (double) 0,
					   NULL);

		gtk_widget_show_all (alignment);
		g_free (filename);
	}

	return alignment;
}
