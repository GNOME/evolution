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
#include <config.h>

#include <string.h>

#include "e-gui-utils.h"
#include <e-util/e-icon-factory.h>

#include <glib.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkhbox.h>

#include <libgnome/gnome-program.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

#ifdef HAVE_LIBGNOMEUI_GNOME_ICON_LOOKUP_H
#include <libgnomeui/gnome-icon-lookup.h>
#endif

GtkWidget *e_create_image_widget(gchar *name,
				 gchar *string1, gchar *string2,
				 gint int1, gint int2)
{
	GtkWidget *alignment = NULL;
	if (string1) {
		GtkWidget *w;
		GdkPixbuf *pixbuf;

		pixbuf = e_icon_factory_get_icon (string1, 48);

		w = gtk_image_new_from_pixbuf (pixbuf);
		g_object_unref (pixbuf);

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

GtkWidget *
e_button_new_with_stock_icon (const char *label_str, const char *stockid)
{
	GtkWidget *button, *hbox, *label, *align, *image;

	button = gtk_button_new ();

	label = gtk_label_new_with_mnemonic (label_str);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), button);

	image = gtk_image_new_from_stock (stockid, GTK_ICON_SIZE_BUTTON);
	hbox = gtk_hbox_new (FALSE, 2);

	align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
      
	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (hbox), label, FALSE, FALSE, 0);
      
	gtk_container_add (GTK_CONTAINER (button), align);
	gtk_container_add (GTK_CONTAINER (align), hbox);
	gtk_widget_show_all (align);

	return button;
}

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
	char *icon_name, *icon_path = NULL;
	GdkPixbuf *pixbuf = NULL;

#ifdef HAVE_LIBGNOMEUI_GNOME_ICON_LOOKUP_H
	static GnomeIconTheme *icon_theme = NULL;

	/* Try the icon theme. (GNOME 2.2 or Sun GNOME 2.0).
	 * This will also look in GNOME VFS.
	 */

	if (!icon_theme)
		icon_theme = gnome_icon_theme_new ();

	icon_name = gnome_icon_lookup (icon_theme, NULL, NULL, NULL, NULL,
				       mime_type, 0, NULL);
	if (icon_name) {
		icon_path = gnome_icon_theme_lookup_icon (
			icon_theme, icon_name, size_hint, NULL, NULL);
		g_free (icon_name);
	}

#else
	const char *vfs_icon_name;

	/* Try gnome-vfs. (gnome-vfs.mime itself doesn't offer much,
	 * but other software packages may define icons for
	 * themselves.
	 */
	vfs_icon_name = gnome_vfs_mime_get_icon (mime_type);
	if (vfs_icon_name) {
		icon_path = gnome_program_locate_file (
			NULL, GNOME_FILE_DOMAIN_PIXMAP,
			vfs_icon_name, TRUE, NULL);
	}

	if (!icon_path) {
		char *p;

		/* Try gnome-mime-data. */
		icon_name = g_strdup_printf ("document-icons/gnome-%s.png",
					     mime_type);
		p = strrchr (icon_name, '/');
		if (p)
			*p = '-';

		icon_path = gnome_program_locate_file (
			NULL, GNOME_FILE_DOMAIN_PIXMAP,
			icon_name, TRUE, NULL);
		g_free (icon_name);
	}

	if (!icon_path) {
		/* Use the generic document icon. */
		icon_path = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP,
						      "document-icons/i-regular.png", TRUE, NULL);
		if (!icon_path) {
			g_warning ("Could not get any icon for %s!",mime_type);
			return e_icon_factory_get_icon (NULL, size_hint);
		}
	}
#endif
	
	if (icon_path == NULL)
		return NULL;
	
	pixbuf = gdk_pixbuf_new_from_file (icon_path, NULL);
	g_free (icon_path);
	return pixbuf;
}

