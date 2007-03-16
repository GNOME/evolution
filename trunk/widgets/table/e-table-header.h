/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-header.h
 * Copyright 1999, 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
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

#ifndef _E_TABLE_COLUMN_H_
#define _E_TABLE_COLUMN_H_

#include <glib-object.h>
#include <gdk/gdk.h>
#include <table/e-table-sort-info.h>
#include <table/e-table-col.h>

G_BEGIN_DECLS

typedef struct _ETableHeader ETableHeader;

#define E_TABLE_HEADER_TYPE        (e_table_header_get_type ())
#define E_TABLE_HEADER(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_HEADER_TYPE, ETableHeader))
#define E_TABLE_HEADER_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_HEADER_TYPE, ETableHeaderClass))
#define E_IS_TABLE_HEADER(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_HEADER_TYPE))
#define E_IS_TABLE_HEADER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_HEADER_TYPE))
#define E_TABLE_HEADER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), E_TABLE_HEADER_TYPE, ETableHeaderClass))

typedef gboolean (*ETableColCheckFunc) (ETableCol *col, gpointer user_data);

/*
 * A Columnar header.
 */
struct _ETableHeader {
	GObject base;

	int col_count;
	int width;
	int nominal_width;
	int width_extras;

	ETableSortInfo *sort_info;
	int sort_info_group_change_id;

	ETableCol **columns;
	
	GSList *change_queue, *change_tail;
	gint idle;
};

typedef struct {
	GObjectClass parent_class;

	void (*structure_change) (ETableHeader *eth);
	void (*dimension_change) (ETableHeader *eth, int width);
	void (*expansion_change) (ETableHeader *eth);
	int (*request_width) (ETableHeader *eth, int col);
} ETableHeaderClass;

GType         e_table_header_get_type                     (void);
ETableHeader *e_table_header_new                          (void);

void          e_table_header_add_column                   (ETableHeader       *eth,
							   ETableCol          *tc,
							   int                 pos);
ETableCol    *e_table_header_get_column                   (ETableHeader       *eth,
							   int                 column);
ETableCol    *e_table_header_get_column_by_col_idx        (ETableHeader       *eth,
							   int                 col_idx);
int           e_table_header_count                        (ETableHeader       *eth);
int           e_table_header_index                        (ETableHeader       *eth,
							   int                 col);
int           e_table_header_get_index_at                 (ETableHeader       *eth,
							   int                 x_offset);
ETableCol   **e_table_header_get_columns            (ETableHeader *eth);
int           e_table_header_get_selected                 (ETableHeader       *eth);

int           e_table_header_total_width                  (ETableHeader       *eth);
int           e_table_header_min_width                    (ETableHeader       *eth);
void          e_table_header_move                         (ETableHeader       *eth,
							   int                 source_index,
							   int                 target_index);
void          e_table_header_remove                       (ETableHeader       *eth,
							   int                 idx);
void          e_table_header_set_size                     (ETableHeader       *eth,
							   int                 idx,
							   int                 size);
void          e_table_header_set_selection                (ETableHeader       *eth,
							   gboolean            allow_selection);
int           e_table_header_col_diff                     (ETableHeader       *eth,
							   int                 start_col,
							   int                 end_col);

void          e_table_header_calc_widths                  (ETableHeader       *eth);
GList        *e_table_header_get_selected_indexes         (ETableHeader       *eth);
void          e_table_header_update_horizontal            (ETableHeader       *eth);
int           e_table_header_prioritized_column           (ETableHeader       *eth);
ETableCol    *e_table_header_prioritized_column_selected  (ETableHeader       *eth,
							   ETableColCheckFunc  check_func,
							   gpointer            user_data);

G_END_DECLS

#endif /* _E_TABLE_HEADER_H_ */

