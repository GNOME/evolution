/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_SORTER_H_
#define _E_TABLE_SORTER_H_

#include <gtk/gtkobject.h>
#include "e-table-model.h"
#include "e-table-subset-variable.h"
#include "e-table-sort-info.h"
#include "e-table-header.h"

#define E_TABLE_SORTER_TYPE        (e_table_sorter_get_type ())
#define E_TABLE_SORTER(o)          (GTK_CHECK_CAST ((o), E_TABLE_SORTER_TYPE, ETableSorter))
#define E_TABLE_SORTER_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_SORTER_TYPE, ETableSorterClass))
#define E_IS_TABLE_SORTER(o)       (GTK_CHECK_TYPE ((o), E_TABLE_SORTER_TYPE))
#define E_IS_TABLE_SORTER_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_SORTER_TYPE))

typedef struct {
	GtkObject base;

	ETableModel    *source;
	ETableHeader   *full_header;
	ETableSortInfo *sort_info;

	int             needs_sorting;

	int            *sorted;
	int            *backsorted;

	int             table_model_changed_id;
	int             table_model_row_changed_id;
	int             table_model_cell_changed_id;
	int             sort_info_changed_id;
} ETableSorter;

typedef struct {
	GtkObjectClass parent_class;
} ETableSorterClass;

GtkType       e_table_sorter_get_type (void);
ETableSorter *e_table_sorter_new      (ETableModel *etm, ETableHeader *full_header, ETableSortInfo *sort_info);

gint          e_table_sorter_model_to_sorted (ETableSorter *sorter, int row);
gint          e_table_sorter_sorted_to_model (ETableSorter *sorter, int row);

gboolean      e_table_sorter_needs_sorting   (ETableSorter *sorter);

#endif /* _E_TABLE_SORTER_H_ */
