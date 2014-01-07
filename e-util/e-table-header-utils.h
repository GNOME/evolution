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
 *		Chris Lahey <clahey@ximian.com>
 *		Miguel de Icaza <miguel@ximian.com>
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_TABLE_HEADER_UTILS_H
#define E_TABLE_HEADER_UTILS_H

#include <e-util/e-table-col.h>

G_BEGIN_DECLS

gdouble		e_table_header_compute_height	(ETableCol *ecol,
						 GtkWidget *widget);
gdouble		e_table_header_width_extras	(GtkWidget *widget);
void		e_table_header_draw_button	(cairo_t *cr,
						 ETableCol *ecol,
						 GtkWidget *widget,
						 gint x,
						 gint y,
						 gint width,
						 gint height,
						 gint button_width,
						 gint button_height,
						 ETableColArrow arrow);

G_END_DECLS

#endif /* E_TABLE_HEADER_UTILS_H */
