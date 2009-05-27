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

#ifndef _E_TABLE_COL_H_
#define _E_TABLE_COL_H_

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <table/e-cell.h>

G_BEGIN_DECLS

#define E_TABLE_COL_TYPE        (e_table_col_get_type ())
#define E_TABLE_COL(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_COL_TYPE, ETableCol))
#define E_TABLE_COL_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_COL_TYPE, ETableColClass))
#define E_IS_TABLE_COL(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_COL_TYPE))
#define E_IS_TABLE_COL_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_COL_TYPE))
#define E_TABLE_COL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), E_TABLE_COL_TYPE, ETableColClass))

typedef enum {
	E_TABLE_COL_ARROW_NONE = 0,
	E_TABLE_COL_ARROW_UP,
	E_TABLE_COL_ARROW_DOWN
} ETableColArrow;

/*
 * Information about a single column
 */
typedef struct {
	GObject         base;
	gchar             *text;
	GdkPixbuf        *pixbuf;
	gint               min_width;
	gint               width;
	double            expansion;
	short             x;
	GCompareFunc      compare;
	ETableSearchFunc  search;
	guint      is_pixbuf:1;
	guint      selected:1;
	guint      resizable:1;
	guint      disabled:1;
	guint      sortable:1;
	guint      groupable:1;
	gint               col_idx;
	gint               compare_col;
	gint               priority;

	GtkJustification  justification;

	ECell            *ecell;
} ETableCol;

typedef struct {
	GObjectClass parent_class;
} ETableColClass;

GType      e_table_col_get_type         (void);
ETableCol *e_table_col_new              (gint           col_idx,
					 const gchar   *text,
					 double        expansion,
					 gint           min_width,
					 ECell        *ecell,
					 GCompareFunc  compare,
					 gboolean      resizable,
					 gboolean      disabled,
					 gint           priority);
ETableCol *e_table_col_new_with_pixbuf  (gint           col_idx,
					 const gchar   *text,
					 GdkPixbuf    *pixbuf,
					 double        expansion,
					 gint           min_width,
					 ECell        *ecell,
					 GCompareFunc  compare,
					 gboolean      resizable,
					 gboolean      disabled,
					 gint           priority);

G_END_DECLS

#endif /* _E_TABLE_COL_H_ */

