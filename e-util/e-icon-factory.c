/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Jeffrey Stedfast <fejj@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <pthread.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnomeui/gnome-desktop-thumbnail.h>
#undef GNOME_DESKTOP_USE_UNSTABLE_API

#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "e-icon-factory.h"
#include "e-util-private.h"

#include "art/broken-image-16.xpm"
#include "art/broken-image-24.xpm"

#define d(x)

typedef struct {
	gchar *name;
	GdkPixbuf *pixbuf;
} Icon;

static GdkPixbuf *broken16_pixbuf = NULL;
static GdkPixbuf *broken24_pixbuf = NULL;

static GHashTable *name_to_icon = NULL;
static GtkIconTheme *icon_theme = NULL;
static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

/* Note: takes ownership of the pixbufs (eg. does not ref them) */
static Icon *
icon_new (const gchar *name, GdkPixbuf *pixbuf)
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
load_icon (const gchar *icon_key, const gchar *icon_name, gint size, gint scale)
{
	GdkPixbuf *pixbuf, *unscaled = NULL;
	gchar *basename, *filename = NULL;

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
			const gchar *dent;
			gint width;
			GDir *dir;
			gchar *x;

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
		if (gdk_pixbuf_get_width(unscaled) != size || gdk_pixbuf_get_height(unscaled) != size)
		{
			pixbuf = e_icon_factory_pixbuf_scale (unscaled, size, size);
			g_object_unref (unscaled);
		} else
			pixbuf = unscaled;
	} else {
		pixbuf = NULL;
	}

	return icon_new (icon_key, pixbuf);
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
	gchar *path;

	if (name_to_icon != NULL)
		return;

	name_to_icon = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) icon_free);

	icon_theme = gtk_icon_theme_get_default ();
	path = g_build_filename (EVOLUTION_DATADIR,
				 "evolution",
				 BASE_VERSION,
				 "icons",
				 NULL);
	gtk_icon_theme_append_search_path (icon_theme, path);
	g_free (path);
	g_signal_connect (
		icon_theme, "changed",
		G_CALLBACK (icon_theme_changed_cb), NULL);

	broken16_pixbuf = gdk_pixbuf_new_from_xpm_data (
		(const gchar **) broken_image_16_xpm);
	broken24_pixbuf = gdk_pixbuf_new_from_xpm_data (
		(const gchar **) broken_image_24_xpm);
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
 * @size: size of the icon
 *
 * Looks up the icon to use based on name and size.
 *
 * Returns the requested icon pixbuf.
 **/
gchar *
e_icon_factory_get_icon_filename (const gchar *icon_name,
                                  GtkIconSize icon_size)
{
	GtkIconTheme *icon_theme;
	GtkIconInfo *icon_info;
	gchar *filename = NULL;
	gint width, height;

	g_return_val_if_fail (icon_name != NULL, NULL);

	icon_theme = gtk_icon_theme_get_default ();

	if (!gtk_icon_size_lookup (icon_size, &width, &height))
		return NULL;

	icon_info = gtk_icon_theme_lookup_icon (
		icon_theme, icon_name, height, 0);
	if (icon_info != NULL) {
		filename = g_strdup (
			gtk_icon_info_get_filename (icon_info));
		gtk_icon_info_free (icon_info);
	}

	return filename;
}

/**
 * e_icon_factory_get_icon:
 * @icon_name: name of the icon
 * @icon_size: size of the icon
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
e_icon_factory_get_icon (const gchar *icon_name,
                         GtkIconSize icon_size)
{
	GdkPixbuf *pixbuf;
	gchar *icon_key;
	Icon *icon;
	gint size, width, height;

	g_return_val_if_fail (icon_name != NULL, NULL);

	if (!gtk_icon_size_lookup (icon_size, &width, &height))
		return NULL;

	size = height;

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

/**
 * e_icon_factory_pixbuf_scale
 * Scales pixbuf to desired size.
 * @param pixbuf Pixbuf to be scaled.
 * @param width Desired width, if less or equal to 0, then changed to 1.
 * @param height Desired height, if less or equal to 0, then changed to 1.
 * @return Scaled pixbuf.
 **/
GdkPixbuf *
e_icon_factory_pixbuf_scale (GdkPixbuf *pixbuf, gint width, gint height)
{
	g_return_val_if_fail (pixbuf != NULL, NULL);

	if (width <= 0)
		width = 1;

	if (height <= 0)
		height = 1;

	/* because this can only scale down, not up */
	if (gdk_pixbuf_get_width (pixbuf) > width && gdk_pixbuf_get_height (pixbuf) > height)
		return gnome_desktop_thumbnail_scale_down_pixbuf (pixbuf, width, height);

	return gdk_pixbuf_scale_simple (pixbuf, width, height, GDK_INTERP_BILINEAR);
}

/**
 * e_icon_factory_create_thumbnail
 * Creates system thumbnail for a file filename.
 * @param filename The file name to create the thumbnail for.
 * @return Path to system thumbnail of the file; NULL if couldn't create it. Free it with g_free.
 **/
gchar *
e_icon_factory_create_thumbnail (const gchar *filename)
{
	static GnomeDesktopThumbnailFactory *thumbnail_factory = NULL;
        struct stat file_stat;
        gchar *thumbnail = NULL;

	g_return_val_if_fail (filename != NULL, NULL);

	if (thumbnail_factory == NULL) {
		thumbnail_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL);
	}

	if (g_stat (filename, &file_stat) != -1 && S_ISREG (file_stat.st_mode)) {
		gchar *content_type, *mime = NULL;
		gboolean uncertain = FALSE;

		content_type = g_content_type_guess (filename, NULL, 0, &uncertain);
		if (content_type)
			mime = g_content_type_get_mime_type (content_type);

		if (mime) {
			gchar *uri = g_filename_to_uri (filename, NULL, NULL);

			g_return_val_if_fail (uri != NULL, NULL);

			thumbnail = gnome_desktop_thumbnail_factory_lookup (thumbnail_factory, uri, file_stat.st_mtime);
			if (!thumbnail && gnome_desktop_thumbnail_factory_can_thumbnail (thumbnail_factory, uri, mime, file_stat.st_mtime)) {
				GdkPixbuf *pixbuf;

				pixbuf = gnome_desktop_thumbnail_factory_generate_thumbnail (thumbnail_factory, uri, mime);

				if (pixbuf) {
					gnome_desktop_thumbnail_factory_save_thumbnail (thumbnail_factory, pixbuf, uri, file_stat.st_mtime);
					g_object_unref (pixbuf);

					thumbnail = gnome_desktop_thumbnail_factory_lookup (thumbnail_factory, uri, file_stat.st_mtime);
				}
			}

			g_free (uri);
		}

		g_free (content_type);
		g_free (mime);
	}

	return thumbnail;
}
