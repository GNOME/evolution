/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_SUBSET_H_
#define _E_TABLE_SUBSET_H_

#include <gtk/gtkobject.h>
#include <gal/e-table/e-table-model.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define E_TABLE_SUBSET_TYPE        (e_table_subset_get_type ())
#define E_TABLE_SUBSET(o)          (GTK_CHECK_CAST ((o), E_TABLE_SUBSET_TYPE, ETableSubset))
#define E_TABLE_SUBSET_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_SUBSET_TYPE, ETableSubsetClass))
#define E_IS_TABLE_SUBSET(o)       (GTK_CHECK_TYPE ((o), E_TABLE_SUBSET_TYPE))
#define E_IS_TABLE_SUBSET_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_SUBSET_TYPE))

typedef struct {
	ETableModel base;

	ETableModel  *source;
	int  n_map;
	int *map_table;

	int last_access;

	int              table_model_pre_change_id;
	int              table_model_changed_id;
	int              table_model_row_changed_id;
	int              table_model_cell_changed_id;
	int              table_model_row_inserted_id;
	int              table_model_row_deleted_id;
} ETableSubset;

typedef struct {
	ETableModelClass parent_class;

	void (*proxy_model_pre_change)   (ETableSubset *etss, ETableModel *etm);
	void (*proxy_model_changed)      (ETableSubset *etss, ETableModel *etm);
	void (*proxy_model_row_changed)  (ETableSubset *etss, ETableModel *etm, int row);
	void (*proxy_model_cell_changed) (ETableSubset *etss, ETableModel *etm, int col, int row);
	void (*proxy_model_row_inserted) (ETableSubset *etss, ETableModel *etm, int row);
	void (*proxy_model_row_deleted)  (ETableSubset *etss, ETableModel *etm, int row);
} ETableSubsetClass;

GtkType      e_table_subset_get_type  (void);
ETableModel *e_table_subset_new       (ETableModel *etm, int n_vals);
ETableModel *e_table_subset_construct (ETableSubset *ets, ETableModel *source, int nvals);

ETableModel *e_table_subset_get_toplevel (ETableSubset *table_model);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_TABLE_SUBSET_H_ */

