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
 *		Miguel de Icaza <miguel@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_TABLE_COL_H
#define E_TABLE_COL_H

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <e-util/e-cell.h>
#include <e-util/e-table-column-specification.h>

/* Standard GObject macros */
#define E_TYPE_TABLE_COL \
	(e_table_col_get_type ())
#define E_TABLE_COL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TABLE_COL, ETableCol))
#define E_TABLE_COL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TABLE_COL, ETableColClass))
#define E_IS_TABLE_COL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TABLE_COL))
#define E_IS_TABLE_COL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TABLE_COL))
#define E_TABLE_COL_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TABLE_COL, ETableColClass))

G_BEGIN_DECLS

typedef enum {
	E_TABLE_COL_ARROW_NONE = 0,
	E_TABLE_COL_ARROW_UP,
	E_TABLE_COL_ARROW_DOWN
} ETableColArrow;

typedef struct _ETableCol ETableCol;
typedef struct _ETableColClass ETableColClass;

/*
 * Information about a single column
 */
struct _ETableCol {
	GObject parent;

	ETableColumnSpecification *spec;

	gchar *text;
	gchar *icon_name;
	cairo_surface_t *surface;
	gint surface_width;
	gint surface_height;
	gint surface_scale;
	gint min_width;
	gint width;
	gdouble expansion;
	gshort x;
	GCompareDataFunc compare;
	ETableSearchFunc search;

	gboolean selected;

	GtkJustification justification;

	ECell *ecell;
};

struct _ETableColClass {
	GObjectClass parent_class;
};

GType		e_table_col_get_type	(void) G_GNUC_CONST;
ETableCol *	e_table_col_new		(ETableColumnSpecification *spec,
					 const gchar *text,
					 const gchar *icon_name,
					 ECell *ecell,
					 GCompareDataFunc compare);
void		e_table_col_ensure_surface
					(ETableCol *etc,
					 GtkWidget *widget);
G_END_DECLS

#endif /* E_TABLE_COL_H */

