/* ETable widget - utilities for drawing table header buttons
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Authors: Chris Lahey <clahey@helixcode.com>
 *          Miguel de Icaza <miguel@helixcode.com>
 *          Federico Mena-Quintero <federico@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef E_TABLE_HEADER_UTILS_H
#define E_TABLE_HEADER_UTILS_H

#include <gal/e-table/e-table-col.h>
double  e_table_header_compute_height  (ETableCol      *ecol,
					GtkStyle       *style,
					GdkFont        *font);
double  e_table_header_width_extras    (GtkStyle       *style);
void    e_table_header_draw_button     (GdkDrawable    *drawable,
					ETableCol      *ecol,
					GtkStyle       *style,
					GdkFont        *font,
					GtkStateType    state,
					GtkWidget      *widget,
					GdkGC          *gc,
					int             x,
					int             y,
					int             width,
					int             height,
					int             button_width,
					int             button_height,
					ETableColArrow  arrow);
void    e_table_draw_elided_string     (GdkDrawable    *drawable,
					GdkFont        *font,
					GdkGC          *gc,
					int             x,
					int             y,
					const char     *str,
					int             max_width,
					gboolean        center);
				 


#endif
