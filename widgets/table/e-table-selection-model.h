/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_SELECTION_MODEL_H_
#define _E_TABLE_SELECTION_MODEL_H_

#include <gtk/gtkobject.h>
#include <gal/widgets/e-selection-model.h>
#include <gal/e-table/e-table-model.h>
#include <gal/e-table/e-table-defines.h>
#include <gal/e-table/e-table-sorter.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define E_TABLE_SELECTION_MODEL_TYPE        (e_table_selection_model_get_type ())
#define E_TABLE_SELECTION_MODEL(o)          (GTK_CHECK_CAST ((o), E_TABLE_SELECTION_MODEL_TYPE, ETableSelectionModel))
#define E_TABLE_SELECTION_MODEL_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_SELECTION_MODEL_TYPE, ETableSelectionModelClass))
#define E_IS_TABLE_SELECTION_MODEL(o)       (GTK_CHECK_TYPE ((o), E_TABLE_SELECTION_MODEL_TYPE))
#define E_IS_TABLE_SELECTION_MODEL_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_SELECTION_MODEL_TYPE))

typedef struct {
	ESelectionModel base;

	ETableModel  *model;

	guint model_pre_change_id;
	guint model_changed_id;
	guint model_row_changed_id;
	guint model_cell_changed_id;
	guint model_rows_inserted_id;
	guint model_rows_deleted_id;

	guint frozen : 1;
	guint selection_model_changed : 1;
	guint group_info_changed : 1;

	GHashTable *hash;
	gchar      *cursor_id;
} ETableSelectionModel;

typedef struct {
	ESelectionModelClass parent_class;
} ETableSelectionModelClass;

GtkType               e_table_selection_model_get_type            (void);
ETableSelectionModel *e_table_selection_model_new                 (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* _E_TABLE_SELECTION_MODEL_H_ */
