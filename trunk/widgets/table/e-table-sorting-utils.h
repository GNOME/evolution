/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-sorting-utils.h
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

#ifndef _E_TABLE_SORTING_UTILS_H_
#define _E_TABLE_SORTING_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <table/e-table-model.h>
#include <table/e-tree-model.h>
#include <table/e-table-sort-info.h>
#include <table/e-table-header.h>
gboolean  e_table_sorting_utils_affects_sort         (ETableSortInfo *sort_info,
						      ETableHeader   *full_header,
						      int             col);



void      e_table_sorting_utils_sort                 (ETableModel    *source,
						      ETableSortInfo *sort_info,
						      ETableHeader   *full_header,
						      int            *map_table,
						      int             rows);
int       e_table_sorting_utils_insert               (ETableModel    *source,
						      ETableSortInfo *sort_info,
						      ETableHeader   *full_header,
						      int            *map_table,
						      int             rows,
						      int             row);
int       e_table_sorting_utils_check_position       (ETableModel    *source,
						      ETableSortInfo *sort_info,
						      ETableHeader   *full_header,
						      int            *map_table,
						      int             rows,
						      int             view_row);



void      e_table_sorting_utils_tree_sort            (ETreeModel     *source,
						      ETableSortInfo *sort_info,
						      ETableHeader   *full_header,
						      ETreePath      *map_table,
						      int             count);
int       e_table_sorting_utils_tree_check_position  (ETreeModel     *source,
						      ETableSortInfo *sort_info,
						      ETableHeader   *full_header,
						      ETreePath      *map_table,
						      int             count,
						      int             old_index);
int       e_table_sorting_utils_tree_insert          (ETreeModel     *source,
						      ETableSortInfo *sort_info,
						      ETableHeader   *full_header,
						      ETreePath      *map_table,
						      int             count,
						      ETreePath       path);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_TABLE_SORTING_UTILS_H_ */
