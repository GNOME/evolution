/*
 * e-colors.c - General color allocation utilities
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
 *		Miguel de Icaza <miguel@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/* We keep our own color context, as the color allocation might take
 * place before things are realized.
 */

#include <config.h>

#include "e-colors.h"

GdkColor e_white, e_dark_gray, e_black;

void
e_color_alloc_gdk (GtkWidget *widget, GdkColor *c)
{
	GdkColormap *map;

	e_color_init ();

	if (widget)
		map = gtk_widget_get_colormap (widget);
	else /* FIXME: multi depth broken ? */
		map = gtk_widget_get_default_colormap ();

	gdk_rgb_find_color (map, c);
}

void
e_color_alloc_name (GtkWidget *widget, const gchar *name, GdkColor *c)
{
	GdkColormap *map;

	e_color_init ();

	gdk_color_parse (name, c);

	if (widget)
		map = gtk_widget_get_colormap (widget);
	else /* FIXME: multi depth broken ? */
		map = gtk_widget_get_default_colormap ();

	gdk_rgb_find_color (map, c);
}

void
e_color_init (void)
{
	static gboolean e_color_inited = FALSE;

	/* It's surprisingly easy to end up calling this twice.  Survive.  */
	if (e_color_inited)
		return;

	e_color_inited = TRUE;

	/* Allocate the default colors */
	e_white.red   = 65535;
	e_white.green = 65535;
	e_white.blue  = 65535;
	e_color_alloc_gdk (NULL, &e_white);

	e_black.red   = 0;
	e_black.green = 0;
	e_black.blue  = 0;
	e_color_alloc_gdk (NULL, &e_black);

	e_color_alloc_name (NULL, "gray20",  &e_dark_gray);
}

