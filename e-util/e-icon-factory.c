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
#include <libgnomeui/gnome-icon-theme.h>
#include <e-util/e-icon-factory.h>
#include "art/empty.xpm"


/* One icon.  Keep both a small (16x16) and a large (48x48) version around.  */
struct _Icon {
	char *name;
	GdkPixbuf *pixbuf_16;
	GdkPixbuf *pixbuf_24;
	GdkPixbuf *pixbuf_48;
};
typedef struct _Icon Icon;

/* Hash of all the icons.  */
static GHashTable     *name_to_icon = NULL;
static GnomeIconTheme *icon_theme   = NULL;
static GdkPixbuf      *empty_pixbuf = NULL;


/* Creating and destroying icons.  */

static Icon *
icon_new (const gchar *name,
	  GdkPixbuf *pixbuf_16,
	  GdkPixbuf *pixbuf_24,
	  GdkPixbuf *pixbuf_48)
{
	Icon *icon;

	icon = g_new (Icon, 1);
	icon->name      = g_strdup (name);
	icon->pixbuf_16 = pixbuf_16;
	icon->pixbuf_24 = pixbuf_24;
	icon->pixbuf_48 = pixbuf_48;

	if (pixbuf_16 != NULL)
		g_object_ref (pixbuf_16);
	if (pixbuf_24 != NULL)
		g_object_ref (pixbuf_24);
	if (pixbuf_48 != NULL)
		g_object_ref (pixbuf_48);

	return icon;
}

#if 0

/* (This is not currently used since we never prune icons out of the
   cache.)  */
static void
icon_free (Icon *icon)
{
	g_free (icon->name);

	if (icon->pixbuf_16 != NULL)
		g_object_unref (icon->pixbuf_16);
	if (icon->pixbuf_24 != NULL)
		g_object_unref (icon->pixbuf_24);
	if (icon->pixbuf_48 != NULL)
		g_object_unref (icon->pixbuf_48);

	g_free (icon);
}

#endif


/* Loading icons.  */

static Icon *
load_icon (const gchar *icon_name)
{
	GdkPixbuf *unscaled;
	GdkPixbuf *pixbuf_16;
	GdkPixbuf *pixbuf_24;
	GdkPixbuf *pixbuf_48;
	gchar *filename;
	Icon *icon;

	filename = gnome_icon_theme_lookup_icon (icon_theme, icon_name, 16, 
	                                         NULL, NULL);

	if (filename == NULL)
		return NULL;
	unscaled = gdk_pixbuf_new_from_file (filename, NULL);
	pixbuf_16 = gdk_pixbuf_scale_simple (unscaled, 16, 16, GDK_INTERP_BILINEAR);
	g_object_unref (unscaled);
	g_free (filename);
	
	filename = gnome_icon_theme_lookup_icon (icon_theme, icon_name, 24, 
	                                         NULL, NULL);

	if (filename == NULL)
		return NULL;
	unscaled = gdk_pixbuf_new_from_file (filename, NULL);
	pixbuf_24 = gdk_pixbuf_scale_simple (unscaled, 24, 24, GDK_INTERP_BILINEAR);
	g_object_unref (unscaled);
	g_free (filename);
	
	filename = gnome_icon_theme_lookup_icon (icon_theme, icon_name, 48, 
	                                         NULL, NULL);

	if (filename == NULL)
		return NULL;
	unscaled = gdk_pixbuf_new_from_file (filename, NULL);
	pixbuf_48 = gdk_pixbuf_scale_simple (unscaled, 48, 48, GDK_INTERP_BILINEAR);
	g_object_unref (unscaled);
	g_free (filename);

	icon = icon_new (icon_name, pixbuf_16, pixbuf_24, pixbuf_48);

	g_object_unref (pixbuf_16);
	g_object_unref (pixbuf_24);
	g_object_unref (pixbuf_48);

	return icon;
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

gchar *
e_icon_factory_get_icon_filename (const gchar *icon_name,
			          gint        icon_size)
{
	gchar *filename;
	
	g_return_val_if_fail (icon_name != NULL, NULL);
	g_return_val_if_fail (strcmp (icon_name, ""), NULL);
	
	filename = gnome_icon_theme_lookup_icon (icon_theme,
	                                         icon_name,
	                                         icon_size,
	                                         NULL,
	                                         NULL);
	
	return filename;
}

/* Loads the themed version of the icon name at the appropriate size.
   The returned icon is guaranteed to be the requested size and exist.  If
   the themed icon cannot be found, an empty icon is returned. */
GdkPixbuf *
e_icon_factory_get_icon (const gchar *icon_name,
			 gint        icon_size)
{
	if (icon_name != NULL && strcmp (icon_name, "")) {
		Icon *icon;

		icon = g_hash_table_lookup (name_to_icon, icon_name);
		if (icon == NULL) {
			icon = load_icon (icon_name);
			if (icon == NULL) {
				g_warning ("Icon not found -- %s", icon_name);

				/* Create an empty icon so that we don't keep spitting
				   out the same warning over and over, every time this
				   icon is requested.  */

				icon = icon_new (icon_name, NULL, NULL, NULL);
				g_hash_table_insert (name_to_icon, icon->name, icon);
			}
			else {
				g_hash_table_insert (name_to_icon, icon->name, icon);
			}
		}

		if (icon->pixbuf_16) {
			gchar *filename;
			GdkPixbuf *pixbuf, *scaled;
			
			switch (icon_size) {
			case 16:
				return g_object_ref (icon->pixbuf_16);
			
			case 24:
				return g_object_ref (icon->pixbuf_24);
			
			case 48:
				return g_object_ref (icon->pixbuf_48);
			
			default:
				/* Non-standard size.  Do a non-cached load. */
				
				filename = gnome_icon_theme_lookup_icon (icon_theme,
				                                         icon_name,
				                                         icon_size,
				                                         NULL,
				                                         NULL);
				if (filename == NULL)
					break;
				
				pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
				g_free (filename);
				if (pixbuf == NULL)
					break;
				
				scaled = gdk_pixbuf_scale_simple (pixbuf, icon_size, icon_size, GDK_INTERP_BILINEAR);
				g_object_unref (pixbuf);
				
				return scaled;
			}
		}
	}
	
	/* icon not found -- create an empty icon */
	return gdk_pixbuf_scale_simple (empty_pixbuf, icon_size, icon_size, GDK_INTERP_NEAREST);
}

GList *
e_icon_factory_get_icon_list (const gchar *icon_name)
{
	if (icon_name != NULL && strcmp (icon_name, "")) {
		Icon *icon;

		icon = g_hash_table_lookup (name_to_icon, icon_name);
		if (icon == NULL) {
			icon = load_icon (icon_name);
			if (icon == NULL) {
				g_warning ("Icon not found -- %s", icon_name);

				/* Create an empty icon so that we don't keep spitting
				   out the same warning over and over, every time this
				   icon is requested.  */

				icon = icon_new (icon_name, NULL, NULL, NULL);
				g_hash_table_insert (name_to_icon, icon->name, icon);
			}
			else {
				g_hash_table_insert (name_to_icon, icon->name, icon);
			}
		}
		
		if (icon->pixbuf_16) {
			GList *list = NULL;
			
			list = g_list_append (list, g_object_ref (icon->pixbuf_48));
			list = g_list_append (list, g_object_ref (icon->pixbuf_24));
			list = g_list_append (list, g_object_ref (icon->pixbuf_16));
			
			return list;
		}
	}
	
	/* icons not found */
	return NULL;
}
