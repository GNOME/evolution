/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-col.h
 * Copyright 1999, 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Miguel de Icaza <miguel@ximian.com>
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
	char             *text;
	GdkPixbuf        *pixbuf;
	int               min_width;
	int               width;
	double            expansion;
	short             x;
	GCompareFunc      compare;
	ETableSearchFunc  search;
	unsigned int      is_pixbuf:1;
	unsigned int      selected:1;
	unsigned int      resizable:1;
	unsigned int      disabled:1;
	unsigned int      sortable:1;
	unsigned int      groupable:1;
	int               col_idx;
	int               compare_col;
	int               priority;

	GtkJustification  justification;

	ECell            *ecell;
} ETableCol;

typedef struct {
	GObjectClass parent_class;
} ETableColClass;

GType      e_table_col_get_type         (void);
ETableCol *e_table_col_new              (int           col_idx,
					 const char   *text,
					 double        expansion,
					 int           min_width,
					 ECell        *ecell,
					 GCompareFunc  compare,
					 gboolean      resizable,
					 gboolean      disabled,
					 int           priority);
ETableCol *e_table_col_new_with_pixbuf  (int           col_idx,
					 const char   *text,
					 GdkPixbuf    *pixbuf,
					 double        expansion,
					 int           min_width,
					 ECell        *ecell,
					 GCompareFunc  compare,
					 gboolean      resizable,
					 gboolean      disabled,
					 int           priority);

G_END_DECLS

#endif /* _E_TABLE_COL_H_ */

