/*
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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_TABLE_SORTING_UTILS_H_
#define _E_TABLE_SORTING_UTILS_H_

#include <e-util/e-table-header.h>
#include <e-util/e-table-model.h>
#include <e-util/e-table-sort-info.h>
#include <e-util/e-tree-model.h>

G_BEGIN_DECLS

gboolean	e_table_sorting_utils_affects_sort
						(ETableSortInfo *sort_info,
						 ETableHeader *full_header,
						 gint compare_col);

void		e_table_sorting_utils_sort	(ETableModel *source,
						 ETableSortInfo *sort_info,
						 ETableHeader *full_header,
						 gint *map_table,
						 gint rows);
gint		e_table_sorting_utils_insert	(ETableModel *source,
						 ETableSortInfo *sort_info,
						 ETableHeader *full_header,
						 gint *map_table,
						 gint rows,
						 gint row);
gint		e_table_sorting_utils_check_position
						(ETableModel *source,
						 ETableSortInfo *sort_info,
						 ETableHeader *full_header,
						 gint *map_table,
						 gint rows,
						 gint view_row);

void		e_table_sorting_utils_tree_sort	(ETreeModel *source,
						 ETableSortInfo *sort_info,
						 ETableHeader *full_header,
						 ETreePath *map_table,
						 gint count);
gint		e_table_sorting_utils_tree_check_position
						(ETreeModel *source,
						 ETableSortInfo *sort_info,
						 ETableHeader *full_header,
						 ETreePath *map_table,
						 gint count,
						 gint old_index);
gint		e_table_sorting_utils_tree_insert
						(ETreeModel *source,
						 ETableSortInfo *sort_info,
						 ETableHeader *full_header,
						 ETreePath *map_table,
						 gint count,
						 ETreePath path);

gpointer	e_table_sorting_utils_create_cmp_cache
						(void);
void		e_table_sorting_utils_free_cmp_cache
						(gpointer cmp_cache);
void		e_table_sorting_utils_add_to_cmp_cache
						(gpointer cmp_cache,
						 const gchar *key,
						 gchar *value);
const gchar *	e_table_sorting_utils_lookup_cmp_cache
						(gpointer cmp_cache,
						 const gchar *key);

G_END_DECLS

#endif /* _E_TABLE_SORTING_UTILS_H_ */
