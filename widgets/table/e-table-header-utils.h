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
 *		Chris Lahey <clahey@ximian.com>
 *		Miguel de Icaza <miguel@ximian.com>
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_TABLE_HEADER_UTILS_H
#define E_TABLE_HEADER_UTILS_H

#include <table/e-table-col.h>

G_BEGIN_DECLS

gdouble  e_table_header_compute_height (ETableCol *ecol,
				       GtkWidget *widget);
gdouble  e_table_header_width_extras    (GtkStyle       *style);
void    e_table_header_draw_button     (GdkDrawable    *drawable,
					ETableCol      *ecol,
					GtkStyle       *style,
					GtkStateType    state,
					GtkWidget      *widget,
					gint             x,
					gint             y,
					gint             width,
					gint             height,
					gint             button_width,
					gint             button_height,
					ETableColArrow  arrow);

G_END_DECLS

#endif
