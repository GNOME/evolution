/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@novell.com>
 *
 *  Copyright 2004 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <pthread.h>

#include <gtk/gtkicontheme.h>
#include <gtk/gtkimage.h>

#include "e-icon-factory.h"
#include "e-util-private.h"

#include "art/broken-image-16.xpm"
#include "art/broken-image-24.xpm"

static int sizes[E_ICON_NUM_SIZES] = {
	16, /* menu */
	20, /* button */
	18, /* small toolbar */
	24, /* large toolbar */
	32, /* dnd */
	48, /* dialog */
};


typedef struct {
	char *name;
	GdkPixbuf *pixbuf;
} Icon;

static GdkPixbuf *broken16_pixbuf = NULL;
static GdkPixbuf *broken24_pixbuf = NULL;

static GHashTable *name_to_icon = NULL;
static GtkIconTheme *icon_theme = NULL;
static GStaticMutex mutex = G_STATIC_MUTEX_INIT;


/* Note: takes ownership of the pixbufs (eg. does not ref them) */
static Icon *
icon_new (const char *name, GdkPixbuf *pixbuf)
{
	Icon *icon;

	icon = g_slice_new (Icon);
	icon->name = g_strdup (name);
	icon->pixbuf = pixbuf;

	return icon;
}

static void
icon_free (Icon *icon)
{
	g_free (icon->name);
	if (icon->pixbuf)
		g_object_unref (icon->pixbuf);
	g_slice_free (Icon, icon);
}

static Icon *
load_icon (const char *icon_key, const char *icon_name, int size, int scale)
{
	GdkPixbuf *pixbuf, *unscaled = NULL;
	char *basename, *filename = NULL;

	if (g_path_is_absolute (icon_name))
		filename = g_strdup (icon_name);
	else {
		GtkIconInfo *icon_info;

		icon_info = gtk_icon_theme_lookup_icon (
			icon_theme, icon_name, size, 0);
		if (icon_info != NULL) {
			filename = g_strdup (
				gtk_icon_info_get_filename (icon_info));
			gtk_icon_info_free (icon_info);
		}
	}

	if (!filename || !(unscaled = gdk_pixbuf_new_from_file (filename, NULL))) {
		if (scale) {
			const char *dent;
			int width;
			GDir *dir;
			char *x;
			
			if (!(dir = g_dir_open (EVOLUTION_ICONSDIR, 0, NULL))) {
				goto done;
			}
			
			/* scan icon directories looking for an icon with a size >= the size we need. */
			while ((dent = g_dir_read_name (dir))) {
				if (!(dent[0] >= '1' && dent[0] <= '9'))
					continue;
				
				if (((width = strtol (dent, &x, 10)) < size) || *x != 'x')
					continue;
				
				if (((strtol (x + 1, &x, 10)) != width) || *x != '\0')
					continue;
				
				/* if the icon exists in this directory, we can [use/scale] it */
				g_free (filename);
				basename = g_strconcat (icon_name, ".png", NULL);
				filename = g_build_filename (EVOLUTION_ICONSDIR,
							     dent,
							     basename,
							     NULL);
				g_free (basename);
				if ((unscaled = gdk_pixbuf_new_from_file (filename, NULL)))
					break;
			}
			
			g_dir_close (dir);
		} else {
			gchar *size_x_size;

			size_x_size = g_strdup_printf ("%dx%d", size, size);
			basename = g_strconcat (icon_name, ".png", NULL);
			g_free (filename);
			filename = g_build_filename (EVOLUTION_ICONSDIR,
						     size_x_size,
						     basename,
						     NULL);
			g_free (basename);
			g_free (size_x_size);
			unscaled = gdk_pixbuf_new_from_file (filename, NULL);
		}
	}

 done:

	g_free (filename);
	if (unscaled != NULL) {
		if(gdk_pixbuf_get_width(unscaled) != size || gdk_pixbuf_get_height(unscaled) != size)
		{
			pixbuf = gdk_pixbuf_scale_simple (unscaled, size, size, GDK_INTERP_BILINEAR);
			g_object_unref (unscaled);
		} else
			pixbuf = unscaled;
	} else {
		pixbuf = NULL;
	}

	return icon_new (icon_key, pixbuf);
}


/* temporary workaround for code that has not yet been ported to the new icon_size API */
static int
pixel_size_to_icon_size (int pixel_size)
{
	int i, icon_size = -1;

	for (i = 0; i < E_ICON_NUM_SIZES; i++) {
		if (pixel_size == sizes[i]) {
			icon_size = i;
			break;
		}
	}

	return icon_size;
}

static void
icon_theme_changed_cb (GtkIconTheme *icon_theme, gpointer user_data)
{
	g_hash_table_remove_all (name_to_icon);
}

/**
 * e_icon_factory_init:
 *
 * Initialises the icon factory.
 **/
void
e_icon_factory_init (void)
{
	if (name_to_icon != NULL)
		return;

	name_to_icon = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) icon_free);

	icon_theme = gtk_icon_theme_get_default ();
	gtk_icon_theme_append_search_path (icon_theme,
                                   EVOLUTION_DATADIR G_DIR_SEPARATOR_S "icons");
	g_signal_connect (
		icon_theme, "changed",
		G_CALLBACK (icon_theme_changed_cb), NULL);

	broken16_pixbuf = gdk_pixbuf_new_from_xpm_data (
		(const char **) broken_image_16_xpm);
	broken24_pixbuf = gdk_pixbuf_new_from_xpm_data (
		(const char **) broken_image_24_xpm);
}

/**
 * e_icon_factory_shutdown:
 *
 * Shuts down the icon factory (cleans up all cached icons, etc).
 **/
void
e_icon_factory_shutdown (void)
{
	if (name_to_icon == NULL)
		return;

	g_hash_table_destroy (name_to_icon);
	g_object_unref (broken16_pixbuf);
	g_object_unref (broken24_pixbuf);
	name_to_icon = NULL;
}


/**
 * e_icon_factory_get_icon_filename:
 * @icon_name: name of the icon
 * @size: MENU/SMALL_TOOLBAR/etc
 *
 * Looks up the icon to use based on name and size.
 *
 * Returns the requested icon pixbuf.
 **/
char *
e_icon_factory_get_icon_filename (const char *icon_name, int icon_size)
{
	GtkIconInfo *icon_info;
	char *filename;

	g_return_val_if_fail (icon_name != NULL, NULL);
	g_return_val_if_fail (strcmp (icon_name, ""), NULL);

	if (icon_size >= E_ICON_NUM_SIZES) {
		g_warning (
			"calling %s with unknown icon_size value (%d)",
			G_STRFUNC, icon_size);
		if ((icon_size = pixel_size_to_icon_size (icon_size)) == -1)
			return NULL;
	}

	g_static_mutex_lock (&mutex);
	icon_info = gtk_icon_theme_lookup_icon (
		icon_theme, icon_name, sizes[icon_size], 0);
	if (icon_info != NULL) {
		filename = g_strdup (
			gtk_icon_info_get_filename (icon_info));
		gtk_icon_info_free (icon_info);
	} else
		filename = NULL;
	g_static_mutex_unlock (&mutex);

	return filename;
}


/**
 * e_icon_factory_get_icon:
 * @icon_name: name of the icon
 * @icon_size: size of the icon (one of the E_ICON_SIZE_* enum values)
 *
 * Returns the specified icon of the requested size (may perform
 * scaling to achieve this). If @icon_name is a full path, that file
 * is used directly. Otherwise it is looked up in the user's current
 * icon theme. If the icon cannot be found in the icon theme, it falls
 * back to loading the requested icon from Evolution's icon set
 * installed from the art/ srcdir. If even that fails to find the
 * requested icon, then a "broken-image" icon is returned.
 **/
GdkPixbuf *
e_icon_factory_get_icon (const char *icon_name, int icon_size)
{
	GdkPixbuf *pixbuf;
	char *icon_key;
	Icon *icon;
	int size;

	if (icon_size >= E_ICON_NUM_SIZES) {
		g_warning (
			"calling %s with unknown icon_size value (%d)",
			G_STRFUNC, icon_size);
		if ((icon_size = pixel_size_to_icon_size (icon_size)) == -1)
			return NULL;
	}

	size = sizes[icon_size];

	if (icon_name == NULL || !strcmp (icon_name, "")) {
		if (size >= 24)
			return gdk_pixbuf_scale_simple (broken24_pixbuf, size, size, GDK_INTERP_NEAREST);
		else
			return gdk_pixbuf_scale_simple (broken16_pixbuf, size, size, GDK_INTERP_NEAREST);
	}

	icon_key = g_alloca (strlen (icon_name) + 7);
	sprintf (icon_key, "%dx%d/%s", size, size, icon_name);

	g_static_mutex_lock (&mutex);

	if (!(icon = g_hash_table_lookup (name_to_icon, icon_key))) {
		icon = load_icon (icon_key, icon_name, size, TRUE);
		g_hash_table_insert (name_to_icon, icon->name, icon);
	}

	if ((pixbuf = icon->pixbuf)) {
		g_object_ref (pixbuf);
	} else {
		if (size >= 24)
			pixbuf = gdk_pixbuf_scale_simple (broken24_pixbuf, size, size, GDK_INTERP_NEAREST);
		else
			pixbuf = gdk_pixbuf_scale_simple (broken16_pixbuf, size, size, GDK_INTERP_NEAREST);
	}

	g_static_mutex_unlock (&mutex);

	return pixbuf;
}

GtkWidget  *
e_icon_factory_get_image (const char *icon_name, int icon_size)
{
	GdkPixbuf *pixbuf;
	GtkWidget *image;

	pixbuf = e_icon_factory_get_icon  (icon_name, icon_size);
	image = gtk_image_new_from_pixbuf (pixbuf);
	g_object_unref (pixbuf);

	return image;
}

/**
 * e_icon_factory_get_icon_list:
 * @icon_name: name of the icon
 *
 * Returns a list of GdkPixbufs of the requested name suitable for
 * gtk_window_set_icon_list().
 **/
GList *
e_icon_factory_get_icon_list (const char *icon_name)
{
	static int icon_list_sizes[] = { 128, 64, 48, 32, 16 };
	GList *list = NULL;
	char *icon_key;
	Icon *icon;
	int size, i;

	if (!icon_name || !strcmp (icon_name, ""))
		return NULL;

	g_static_mutex_lock (&mutex);

	icon_key = g_alloca (strlen (icon_name) + 9);

	for (i = 0; i < G_N_ELEMENTS (icon_list_sizes); i++) {
		size = icon_list_sizes[i];
		sprintf (icon_key, "%dx%d/%s", size, size, icon_name);
		
		if (!(icon = g_hash_table_lookup (name_to_icon, icon_key))) {
			if ((icon = load_icon (icon_key, icon_name, size, FALSE)))
				g_hash_table_insert (name_to_icon, icon->name, icon);
		}
		
		if (icon && icon->pixbuf) {
			list = g_list_prepend (list, icon->pixbuf);
			g_object_ref (icon->pixbuf);
		}
	}

	g_static_mutex_unlock (&mutex);

	return list;
}
