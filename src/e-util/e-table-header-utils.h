/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Chris Lahey <clahey@ximian.com>
 * SPDX-FileContributor: Miguel de Icaza <miguel@ximian.com>
 * SPDX-FileContributor: Federico Mena-Quintero <federico@ximian.com>
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
