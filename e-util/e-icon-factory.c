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
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <pthread.h>

#include <libgnomeui/gnome-icon-theme.h>
#include <e-util/e-icon-factory.h>

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
static GnomeIconTheme *icon_theme = NULL;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;


/* Note: takes ownership of the pixbufs (eg. does not ref them) */
static Icon *
icon_new (const char *name, GdkPixbuf *pixbuf)
{
	Icon *icon;
	
	icon = g_malloc0 (sizeof (Icon));
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
	g_free (icon);
}

static Icon *
load_icon (const char *icon_name, int size, int scale)
{
	GdkPixbuf *pixbuf, *unscaled = NULL;
	char *filename = NULL;
	
	if (icon_name[0] == '/')
		filename = g_strdup (icon_name);
	else
		filename = gnome_icon_theme_lookup_icon (icon_theme, icon_name, size, NULL, NULL);

	if (!filename || !(unscaled = gdk_pixbuf_new_from_file (filename, NULL))) {
		if (scale) {
			struct dirent *dent;
			int width, height;
			size_t baselen;
			GString *path;
			DIR *dir;
			char *x;
			
			path = g_string_new (EVOLUTION_ICONSDIR);
			if (path->str[path->len - 1] != '/')
				g_string_append_c (path, '/');
			
			baselen = path->len;
			
			if (!(dir = opendir (path->str))) {
				g_string_free (path, TRUE);
				goto done;
			}
			
			/* scan icon directories looking for an icon with a size >= the size we need. */
			while ((dent = readdir (dir))) {
				if (!(dent->d_name[0] >= '1' && dent->d_name[0] <= '9'))
					continue;
				
				if (((width = strtol (dent->d_name, &x, 10)) < size) || *x != 'x')
					continue;
				
				if (((height = strtol (x + 1, &x, 10)) != width) || *x != '\0')
					continue;
				
				/* if the icon exists in this directory, we can [use/scale] it */
				g_string_truncate (path, baselen);
				g_string_append_printf (path, "%s/%s.png", dent->d_name, icon_name);
				if ((unscaled = gdk_pixbuf_new_from_file (path->str, NULL)))
					break;
			}
			
			g_string_free (path, TRUE);
			closedir (dir);
		} else {
			g_free (filename);
			filename = g_strdup_printf (EVOLUTION_ICONSDIR "/%dx%d/%s.png", size, size, icon_name);
			unscaled = gdk_pixbuf_new_from_file (filename, NULL);
		}
	}
	
 done:
	
	g_free (filename);
	if (unscaled != NULL) {
		pixbuf = gdk_pixbuf_scale_simple (unscaled, size, size, GDK_INTERP_BILINEAR);
		g_object_unref (unscaled);
	} else {
		pixbuf = NULL;
	}
	
	return icon_new (icon_name, pixbuf);
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
	
	icon_theme = gnome_icon_theme_new ();
	name_to_icon = g_hash_table_new (g_str_hash, g_str_equal);
	
	broken16_pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **) broken_image_16_xpm);
	broken24_pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **) broken_image_24_xpm);
}


static void
icon_foreach_free (gpointer key, gpointer value, gpointer user_data)
{
	icon_free (value);
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
	
	g_hash_table_foreach (name_to_icon, (GHFunc) icon_foreach_free, NULL);
	g_hash_table_destroy (name_to_icon);
	g_object_unref (broken16_pixbuf);
	g_object_unref (broken24_pixbuf);
	g_object_unref (icon_theme);
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
	char *filename;
	
	g_return_val_if_fail (icon_name != NULL, NULL);
	g_return_val_if_fail (strcmp (icon_name, ""), NULL);
	
	if (icon_size >= E_ICON_NUM_SIZES) {
		g_warning ("calling e_icon_factory_get_icon_filename with unknown icon_size value (%d)", icon_size);
		if ((icon_size = pixel_size_to_icon_size (icon_size)) == -1)
			return NULL;
	}
	
	pthread_mutex_lock (&lock);
	filename = gnome_icon_theme_lookup_icon (icon_theme, icon_name, sizes[icon_size], NULL, NULL);
	pthread_mutex_unlock (&lock);
	
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
		g_warning ("calling e_icon_factory_get_icon with unknown icon_size value (%d)", icon_size);
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
	
	pthread_mutex_lock (&lock);
	
	if (!(icon = g_hash_table_lookup (name_to_icon, icon_key))) {
		if (!(icon = load_icon (icon_name, size, TRUE))) {
			g_warning ("Icon not found -- %s", icon_name);
			
			/* Create an empty icon so that we don't keep spitting
			   out the same warning over and over, every time this
			   icon is requested.  */
			
			icon = icon_new (icon_key, NULL);
			g_hash_table_insert (name_to_icon, icon->name, icon);
		} else {
			g_hash_table_insert (name_to_icon, icon->name, icon);
		}
	}
	
	if ((pixbuf = icon->pixbuf)) {
		g_object_ref (pixbuf);
	} else {
		if (size >= 24)
			pixbuf = gdk_pixbuf_scale_simple (broken24_pixbuf, size, size, GDK_INTERP_NEAREST);
		else
			pixbuf = gdk_pixbuf_scale_simple (broken16_pixbuf, size, size, GDK_INTERP_NEAREST);
	}
	
	pthread_mutex_unlock (&lock);
	
	return pixbuf;
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
	
	pthread_mutex_lock (&lock);
	
	icon_key = g_alloca (strlen (icon_name) + 9);
	
	for (i = 0; i < G_N_ELEMENTS (icon_list_sizes); i++) {
		size = icon_list_sizes[i];
		sprintf (icon_key, "%dx%d/%s", size, size, icon_name);
		
		if (!(icon = g_hash_table_lookup (name_to_icon, icon_key))) {
			if ((icon = load_icon (icon_name, size, FALSE)))
				g_hash_table_insert (name_to_icon, icon->name, icon);
		}
		
		if (icon && icon->pixbuf) {
			list = g_list_prepend (list, icon->pixbuf);
			g_object_ref (icon->pixbuf);
		}
	}
	
	pthread_mutex_unlock (&lock);
	
	return list;
}
