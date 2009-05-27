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
 *		Miguel de Icaza (miguel@kernel.org)
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef GNOME_APP_LIBS_COLOR_H
#define GNOME_APP_LIBS_COLOR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

void     e_color_init       (void);

/* Return the pixel value for the given red, green and blue */
gulong   e_color_alloc      (gushort red, gushort green, gushort blue);
void     e_color_alloc_name (GtkWidget *widget, const gchar *name, GdkColor *color);
void     e_color_alloc_gdk  (GtkWidget *widget, GdkColor *color);

extern GdkColor e_white, e_dark_gray, e_black;

G_END_DECLS

#endif /* GNOME_APP_LIBS_COLOR_H */
