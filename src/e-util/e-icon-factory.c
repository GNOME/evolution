/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Jeffrey Stedfast <fejj@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_GNOME_DESKTOP
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-desktop-thumbnail.h>
#undef GNOME_DESKTOP_USE_UNSTABLE_API
#endif

#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "e-icon-factory.h"
#include "e-util-private.h"

#define d(x)

static gboolean prefer_symbolic_icons = FALSE;

void
e_icon_factory_set_prefer_symbolic_icons (gboolean prefer)
{
	prefer_symbolic_icons = prefer;
}

gboolean
e_icon_factory_get_prefer_symbolic_icons (void)
{
	return prefer_symbolic_icons;
}

/**
 * e_icon_factory_get_icon_filename:
 * @icon_name: name of the icon
 * @icon_size: size of the icon
 *
 * Returns the filename of the requested icon in the default icon theme.
 *
 * Returns: the filename of the requested icon
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
		icon_theme, icon_name, height, prefer_symbolic_icons ? GTK_ICON_LOOKUP_FORCE_SYMBOLIC : GTK_ICON_LOOKUP_FORCE_REGULAR);
	if (!icon_info)
		icon_info = gtk_icon_theme_lookup_icon (icon_theme, icon_name, height, 0);
	if (icon_info != NULL) {
		filename = g_strdup (
			gtk_icon_info_get_filename (icon_info));
		g_object_unref (icon_info);
	}

	return filename;
}

/**
 * e_icon_factory_get_icon:
 * @icon_name: name of the icon
 * @icon_size: size of the icon
 *
 * Loads the requested icon from the default icon theme and renders it
 * to a pixbuf.
 *
 * Returns: the rendered icon
 **/
GdkPixbuf *
e_icon_factory_get_icon (const gchar *icon_name,
                         GtkIconSize icon_size)
{
	GtkIconTheme *icon_theme;
	GdkPixbuf *pixbuf;
	gint width, height;
	GError *error = NULL;

	g_return_val_if_fail (icon_name != NULL, NULL);

	icon_theme = gtk_icon_theme_get_default ();

	if (!gtk_icon_size_lookup (icon_size, &width, &height))
		width = height = 16;

	pixbuf = gtk_icon_theme_load_icon (
		icon_theme, icon_name, height, GTK_ICON_LOOKUP_FORCE_SIZE |
		(prefer_symbolic_icons ? GTK_ICON_LOOKUP_FORCE_SYMBOLIC : GTK_ICON_LOOKUP_FORCE_REGULAR), &error);
	if (!pixbuf) {
		pixbuf = gtk_icon_theme_load_icon (icon_theme, icon_name, height, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
		if (pixbuf)
			g_clear_error (&error);
	}

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_clear_error (&error);

		/* Fallback to missing image */
		pixbuf = gtk_icon_theme_load_icon (
			icon_theme, "image-missing",
			height, GTK_ICON_LOOKUP_FORCE_SIZE, &error);

		if (error != NULL) {
			g_error ("%s", error->message);
			g_clear_error (&error);
		}
	}

	return pixbuf;
}

/**
 * e_icon_factory_pixbuf_scale
 * @pixbuf: a #GdkPixbuf
 * @width: desired width, if less or equal to 0, then changed to 1
 * @height: desired height, if less or equal to 0, then changed to 1
 *
 * Scales @pixbuf to desired size.
 *
 * Returns: a scaled #GdkPixbuf
 **/
GdkPixbuf *
e_icon_factory_pixbuf_scale (GdkPixbuf *pixbuf,
                             gint width,
                             gint height)
{
	g_return_val_if_fail (pixbuf != NULL, NULL);

	if (width <= 0)
		width = 1;

	if (height <= 0)
		height = 1;

	/* because this can only scale down, not up */
	if (gdk_pixbuf_get_width (pixbuf) > width && gdk_pixbuf_get_height (pixbuf) > height)
		return gdk_pixbuf_scale_simple (pixbuf, width, height, GDK_INTERP_HYPER);

	return gdk_pixbuf_scale_simple (pixbuf, width, height, GDK_INTERP_BILINEAR);
}

/**
 * e_icon_factory_create_thumbnail
 * @filename: the file name to create the thumbnail for
 *
 * Creates system thumbnail for @filename.
 *
 * Returns: Path to system thumbnail of the file; %NULL if couldn't
 *          create it. Free it with g_free().
 **/
gchar *
e_icon_factory_create_thumbnail (const gchar *filename)
{
#ifdef HAVE_GNOME_DESKTOP
	static GnomeDesktopThumbnailFactory *thumbnail_factory = NULL;
	struct stat file_stat;
	gchar *thumbnail = NULL;
#if defined(GNOME_DESKTOP_PLATFORM_VERSION) && GNOME_DESKTOP_PLATFORM_VERSION >= 43
	GError *error = NULL;
#endif

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

#if defined(GNOME_DESKTOP_PLATFORM_VERSION) && GNOME_DESKTOP_PLATFORM_VERSION >= 43
				pixbuf = gnome_desktop_thumbnail_factory_generate_thumbnail (thumbnail_factory, uri, mime, NULL, &error);
				if (!pixbuf) {
					g_warning ("Failed to generate thumbnail for %s: %s", uri, error ? error->message : "Unknown error");
					g_clear_error (&error);
				}
#else
				pixbuf = gnome_desktop_thumbnail_factory_generate_thumbnail (thumbnail_factory, uri, mime);
#endif

				if (pixbuf) {
#if defined(GNOME_DESKTOP_PLATFORM_VERSION) && GNOME_DESKTOP_PLATFORM_VERSION >= 43
					gnome_desktop_thumbnail_factory_save_thumbnail (thumbnail_factory, pixbuf, uri, file_stat.st_mtime, NULL, &error);
					if (error) {
						g_warning ("Failed to save thumbnail for %s: %s", uri, error ? error->message : "Unknown error");
						g_clear_error (&error);
					}
#else
					gnome_desktop_thumbnail_factory_save_thumbnail (thumbnail_factory, pixbuf, uri, file_stat.st_mtime);
#endif
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
#else
	return NULL;
#endif /* HAVE_GNOME_DESKTOP */
}
