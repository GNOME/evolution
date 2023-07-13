/*
 *
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
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_ICON_FACTORY_H_
#define _E_ICON_FACTORY_H_

#include <gtk/gtk.h>

void		e_icon_factory_set_prefer_symbolic_icons
						(gboolean prefer);
gboolean	e_icon_factory_get_prefer_symbolic_icons
						(void);
gchar *		e_icon_factory_get_icon_filename (const gchar *icon_name,
						 GtkIconSize icon_size);
GdkPixbuf *	e_icon_factory_get_icon		(const gchar *icon_name,
						 GtkIconSize icon_size);
GdkPixbuf *	e_icon_factory_pixbuf_scale	(GdkPixbuf *pixbuf,
						 gint width,
						 gint height);

gchar *		e_icon_factory_create_thumbnail (const gchar *filename);

#endif /* _E_ICON_FACTORY_H_ */
