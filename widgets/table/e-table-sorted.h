/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_SORTED_H_
#define _E_TABLE_SORTED_H_

#include <gtk/gtkobject.h>
#include <gal/e-table/e-table-model.h>
#include <gal/e-table/e-table-subset.h>
#include <gal/e-table/e-table-sort-info.h>
#include <gal/e-table/e-table-header.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define E_TABLE_SORTED_TYPE        (e_table_sorted_get_type ())
#define E_TABLE_SORTED(o)          (GTK_CHECK_CAST ((o), E_TABLE_SORTED_TYPE, ETableSorted))
#define E_TABLE_SORTED_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_SORTED_TYPE, ETableSortedClass))
#define E_IS_TABLE_SORTED(o)       (GTK_CHECK_TYPE ((o), E_TABLE_SORTED_TYPE))
#define E_IS_TABLE_SORTED_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_SORTED_TYPE))

typedef struct {
	ETableSubset base;

	ETableSortInfo *sort_info;
	
	ETableHeader *full_header;

	int              sort_info_changed_id;
	int              sort_idle_id;
	int		 insert_idle_id;
	int		 insert_count;

} ETableSorted;

typedef struct {
	ETableSubsetClass parent_class;
} ETableSortedClass;

GtkType      e_table_sorted_get_type (void);
ETableModel *e_table_sorted_new      (ETableModel *etm, ETableHeader *header, ETableSortInfo *sort_info);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_TABLE_SORTED_H_ */
