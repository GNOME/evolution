/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_SORTING_UTILS_H_
#define _E_TABLE_SORTING_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <gal/e-table/e-table-model.h>
#include <gal/e-table/e-tree-model.h>
#include <gal/e-table/e-table-sort-info.h>
#include <gal/e-table/e-table-header.h>
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
