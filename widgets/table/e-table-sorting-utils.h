/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_SORTING_UTILS_H_
#define _E_TABLE_SORTING_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <gal/e-table/e-table-model.h>
#include <gal/e-table/e-table-sort-info.h>
#include <gal/e-table/e-table-header.h>

void      e_table_sorting_utils_sort          (ETableModel    *source,
					       ETableSortInfo *sort_info,
					       ETableHeader   *full_header,
					       int            *map_table,
					       int             rows);

gboolean  e_table_sorting_utils_affects_sort  (ETableModel    *source,
					       ETableSortInfo *sort_info,
					       ETableHeader   *full_header,
					       int             col);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_TABLE_SORTING_UTILS_H_ */
