/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-icon-factory.c - Icon factory for the Evolution components.
 *
 * Copyright (C) 2002 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>, Michael Terry <mterry@fastmail.fm>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <pthread.h>

#include <libgnomeui/gnome-icon-theme.h>
#include <e-util/e-icon-factory.h>
#include "art/empty.xpm"


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
	GdkPixbuf *pixbuf[E_ICON_NUM_SIZES];
} Icon;

/* Hash of all the icons.  */
static GHashTable     *name_to_icon = NULL;
static GnomeIconTheme *icon_theme   = NULL;
static GdkPixbuf      *empty_pixbuf = NULL;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;


/* Creating and destroying icons.  */

/* Note: takes ownership of the pixbufs (eg. does not ref them) */
static Icon *
icon_new (const char *name, GdkPixbuf **pixbufs)
{
	Icon *icon;
	int i;
	
	icon = g_malloc0 (sizeof (Icon));
	icon->name = g_strdup (name);
	
	if (pixbufs != NULL) {
		for (i = 0; i < E_ICON_NUM_SIZES; i++)
			icon->pixbuf[i] = pixbufs[i];
	}
	
	return icon;
}

#if 0

/* (This is not currently used since we never prune icons out of the
   cache.)  */
static void
icon_free (Icon *icon)
{
	int i;
	
	g_free (icon->name);
	
	for (i = 0; i < E_ICON_NUM_SIZES; i++) {
		if (icon->pixbuf[i] != NULL)
			g_object_unref (icon->pixbuf[i]);
	}
	
	g_free (icon);
}

#endif

/* Loading icons.  */

static Icon *
load_icon (const char *icon_name)
{
	GdkPixbuf *pixbufs[E_ICON_NUM_SIZES];
	char *filename;
	int i, j;
	
	for (i = 0; i < E_ICON_NUM_SIZES; i++) {
		GdkPixbuf *unscaled;
		int size = sizes[i];
		
		if (!(filename = gnome_icon_theme_lookup_icon (icon_theme, icon_name, size, NULL, NULL)))
			goto exception;
		
		unscaled = gdk_pixbuf_new_from_file (filename, NULL);
		pixbufs[i] = gdk_pixbuf_scale_simple (unscaled, size, size, GDK_INTERP_BILINEAR);
		g_object_unref (unscaled);
		g_free (filename);
	}
	
	return icon_new (icon_name, pixbufs);
	
 exception:
	
	for (j = 0; j < i; j++)
		g_object_unref (pixbufs[j]);
	
	return NULL;
}


/* termporary workaround for code that has not yet been ported to the new icon_size API */
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

/* Public API.  */

void
e_icon_factory_init (void)
{
	if (name_to_icon != NULL) {
		/* Already initialized.  */
		return;
	}
	
	name_to_icon = g_hash_table_new (g_str_hash, g_str_equal);
	icon_theme   = gnome_icon_theme_new ();
	empty_pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **) empty_xpm);
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
		g_warning ("e_icon_factory_get_icon_filename(): Invalid argument for icon_size: use one of the E_ICON_SIZE_* enum values");
		if ((icon_size = pixel_size_to_icon_size (icon_size)) == -1)
			return NULL;
	}
	
	pthread_mutex_lock (&lock);
	filename = gnome_icon_theme_lookup_icon (icon_theme, icon_name, sizes[icon_size], NULL, NULL);
	pthread_mutex_unlock (&lock);
	
	return filename;
}

/* Loads the themed version of the icon name at the appropriate size.
   The returned icon is guaranteed to be the requested size and exist.  If
   the themed icon cannot be found, an empty icon is returned. */
GdkPixbuf *
e_icon_factory_get_icon (const char *icon_name, int icon_size)
{
	GdkPixbuf *pixbuf;
	Icon *icon;
	int size;
	
	if (icon_size >= E_ICON_NUM_SIZES) {
		g_warning ("e_icon_factory_get_icon(): Invalid argument for icon_size: use one of the E_ICON_SIZE_* enum values");
		if ((icon_size = pixel_size_to_icon_size (icon_size)) == -1)
			return NULL;
	}
	
	if (icon_name == NULL || !strcmp (icon_name, "")) {
		size = sizes[icon_size];
		return gdk_pixbuf_scale_simple (empty_pixbuf, size, size, GDK_INTERP_NEAREST);
	}
	
	pthread_mutex_lock (&lock);
	
	if (!(icon = g_hash_table_lookup (name_to_icon, icon_name))) {
		if (!(icon = load_icon (icon_name))) {
			g_warning ("Icon not found -- %s", icon_name);
			
			/* Create an empty icon so that we don't keep spitting
			   out the same warning over and over, every time this
			   icon is requested.  */
			
			icon = icon_new (icon_name, NULL);
			g_hash_table_insert (name_to_icon, icon->name, icon);
		} else {
			g_hash_table_insert (name_to_icon, icon->name, icon);
		}
	}
	
	if ((pixbuf = icon->pixbuf[icon_size]))
		g_object_ref (pixbuf);
	
	pthread_mutex_unlock (&lock);
	
	return pixbuf;
}


GList *
e_icon_factory_get_icon_list (const char *icon_name)
{
	GList *list = NULL;
	Icon *icon;
	int i;
	
	if (!icon_name || !strcmp (icon_name, ""))
		return NULL;
	
	pthread_mutex_lock (&lock);
	
	if (!(icon = g_hash_table_lookup (name_to_icon, icon_name))) {
		if (!(icon = load_icon (icon_name))) {
			g_warning ("Icon not found -- %s", icon_name);
			
			/* Create an empty icon so that we don't keep spitting
			   out the same warning over and over, every time this
			   icon is requested.  */
			
			icon = icon_new (icon_name, NULL);
			g_hash_table_insert (name_to_icon, icon->name, icon);
			return NULL;
		} else {
			g_hash_table_insert (name_to_icon, icon->name, icon);
		}
	}
	
	for (i = 0; i < E_ICON_NUM_SIZES; i++) {
		if (icon->pixbuf[i]) {
			list = g_list_prepend (list, icon->pixbuf[i]);
			g_object_ref (icon->pixbuf[i]);
		}
	}
	
	pthread_mutex_unlock (&lock);
	
	return list;
}
