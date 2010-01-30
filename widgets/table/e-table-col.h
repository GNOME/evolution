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
 *		Miguel de Icaza <miguel@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_TABLE_COL_H
#define E_TABLE_COL_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <table/e-cell.h>

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

	gchar *text;
	gchar *icon_name;
	GdkPixbuf *pixbuf;
	gint min_width;
	gint width;
	gdouble expansion;
	gshort x;
	GCompareDataFunc compare;
	ETableSearchFunc search;

	guint selected:1;
	guint resizable:1;
	guint disabled:1;
	guint sortable:1;
	guint groupable:1;

	gint col_idx;
	gint compare_col;
	gint priority;

	GtkJustification justification;

	ECell *ecell;
};

struct _ETableColClass {
	GObjectClass parent_class;
};

GType		e_table_col_get_type		(void);
ETableCol *	e_table_col_new			(gint col_idx,
						 const gchar *text,
						 const gchar *icon_name,
						 gdouble expansion,
						 gint min_width,
						 ECell *ecell,
						 GCompareDataFunc compare,
						 gboolean resizable,
						 gboolean disabled,
						 gint priority);

G_END_DECLS

#endif /* E_TABLE_COL_H */

