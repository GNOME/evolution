/*
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
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_ICON_FACTORY_H_
#define _E_ICON_FACTORY_H_

#include <gtk/gtk.h>

gchar *		e_icon_factory_get_icon_filename (const gchar *icon_name,
						 GtkIconSize icon_size);
GdkPixbuf *	e_icon_factory_get_icon		(const gchar *icon_name,
						 GtkIconSize icon_size);
GdkPixbuf *	e_icon_factory_pixbuf_scale	(GdkPixbuf *pixbuf,
						 gint width,
						 gint height);

gchar *		e_icon_factory_create_thumbnail (const gchar *filename);

#endif /* _E_ICON_FACTORY_H_ */
