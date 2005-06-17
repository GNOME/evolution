/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-header-utils.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *          Miguel de Icaza <miguel@ximian.com>
 *          Federico Mena-Quintero <federico@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef E_TABLE_HEADER_UTILS_H
#define E_TABLE_HEADER_UTILS_H

#include <table/e-table-col.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

double  e_table_header_compute_height (ETableCol *ecol,
				       GtkWidget *widget);
double  e_table_header_width_extras    (GtkStyle       *style);
void    e_table_header_draw_button     (GdkDrawable    *drawable,
					ETableCol      *ecol,
					GtkStyle       *style,
					GtkStateType    state,
					GtkWidget      *widget,
					int             x,
					int             y,
					int             width,
					int             height,
					int             button_width,
					int             button_height,
					ETableColArrow  arrow);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
