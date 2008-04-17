/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * GUI utility functions
 *
 * Authors:
 *   Miguel de Icaza (miguel@ximian.com)
 *   Chris Toshok (toshok@ximian.com)
 *
 * Copyright (C) 1999 Miguel de Icaza
 * Copyright (C) 2000-2003 Ximian, Inc.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "e-gui-utils.h"
#include <e-util/e-icon-factory.h>

#include <glib.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkicontheme.h>

#include <libgnome/gnome-program.h>
#include <libgnomeui/gnome-icon-lookup.h>

/**
 * e_icon_for_mime_type:
 * @mime_type: a MIME type
 * @size_hint: the size the caller plans to display the icon at
 *
 * Tries to find an icon representing @mime_type that will display
 * nicely at @size_hint by @size_hint pixels. The returned icon
 * may or may not actually be that size.
 *
 * Return value: a pixbuf, which the caller must unref when it is done
 **/
GdkPixbuf *
e_icon_for_mime_type (const char *mime_type, int size_hint)
{
	gchar *icon_name;
	GdkPixbuf *pixbuf = NULL;

	icon_name = gnome_icon_lookup (
		gtk_icon_theme_get_default (),
		NULL, NULL, NULL, NULL, mime_type, 0, NULL);

	if (icon_name != NULL) {
		pixbuf = gtk_icon_theme_load_icon (
			gtk_icon_theme_get_default (),
			icon_name, size_hint, 0, NULL);
		g_free (icon_name);
	}

	return pixbuf;
}

GtkWidget *e_create_image_widget(gchar *name,
				 gchar *string1, gchar *string2,
				 gint int1, gint int2)
{
	GtkWidget *alignment = NULL;
	GtkWidget *w;

	if (string1) {
		w = e_icon_factory_get_image (string1, E_ICON_SIZE_DIALOG);

		gtk_misc_set_alignment (GTK_MISC (w), 0.5, 0.5);

		alignment = gtk_widget_new(gtk_alignment_get_type(),
					   "child", w,
					   "xalign", (double) 0,
					   "yalign", (double) 0,
					   "xscale", (double) 0,
					   "yscale", (double) 0,
					   NULL);

		gtk_widget_show_all (alignment);
	}

	return alignment;
}
