/*
 * GUI utility functions
 *
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
 *		Miguel de Icaza (miguel@ximian.com)
 *		Chris Toshok (toshok@ximian.com)
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "e-gui-utils.h"

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
