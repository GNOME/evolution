/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-sorter.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
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

#ifndef _E_TABLE_SORTER_H_
#define _E_TABLE_SORTER_H_

#include <glib-object.h>
#include <e-util/e-sorter.h>
#include <table/e-table-model.h>
#include <table/e-table-subset-variable.h>
#include <table/e-table-sort-info.h>
#include <table/e-table-header.h>

G_BEGIN_DECLS

#define E_TABLE_SORTER_TYPE        (e_table_sorter_get_type ())
#define E_TABLE_SORTER(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_SORTER_TYPE, ETableSorter))
#define E_TABLE_SORTER_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_SORTER_TYPE, ETableSorterClass))
#define E_IS_TABLE_SORTER(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_SORTER_TYPE))
#define E_IS_TABLE_SORTER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_SORTER_TYPE))

typedef struct {
	ESorter base;

	ETableModel    *source;
	ETableHeader   *full_header;
	ETableSortInfo *sort_info;

	/* If needs_sorting is 0, then model_to_sorted and sorted_to_model are no-ops. */
	int             needs_sorting;

	int            *sorted;
	int            *backsorted;

	int             table_model_changed_id;
	int             table_model_row_changed_id;
	int             table_model_cell_changed_id;
	int             table_model_rows_inserted_id;
	int             table_model_rows_deleted_id;
	int             sort_info_changed_id;
	int             group_info_changed_id;
} ETableSorter;

typedef struct {
	ESorterClass parent_class;
} ETableSorterClass;

GType         e_table_sorter_get_type                   (void);
ETableSorter *e_table_sorter_new                        (ETableModel     *etm,
							 ETableHeader    *full_header,
							 ETableSortInfo  *sort_info);
G_END_DECLS

#endif /* _E_TABLE_SORTER_H_ */
