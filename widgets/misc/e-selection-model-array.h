/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_SELECTION_MODEL_ARRAY_H_
#define _E_SELECTION_MODEL_ARRAY_H_

#include <gtk/gtkobject.h>
#include <gal/util/e-sorter.h>
#include <gdk/gdktypes.h>
#include <gal/widgets/e-selection-model.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define E_SELECTION_MODEL_ARRAY_TYPE        (e_selection_model_array_get_type ())
#define E_SELECTION_MODEL_ARRAY(o)          (GTK_CHECK_CAST ((o), E_SELECTION_MODEL_ARRAY_TYPE, ESelectionModelArray))
#define E_SELECTION_MODEL_ARRAY_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_SELECTION_MODEL_ARRAY_TYPE, ESelectionModelArrayClass))
#define E_IS_SELECTION_MODEL_ARRAY(o)       (GTK_CHECK_TYPE ((o), E_SELECTION_MODEL_ARRAY_TYPE))
#define E_IS_SELECTION_MODEL_ARRAY_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_SELECTION_MODEL_ARRAY_TYPE))

typedef struct {
	ESelectionModel base;

	gint row_count;
        guint32 *selection;

	gint cursor_row;
	gint cursor_col;
	gint selection_start_row;

	guint model_changed_id;
	guint model_row_inserted_id, model_row_deleted_id;

	guint frozen : 1;
	guint selection_model_changed : 1;
	guint group_info_changed : 1;
} ESelectionModelArray;

typedef struct {
	ESelectionModelClass parent_class;

	gint (*get_row_count)     (ESelectionModelArray *selection);
} ESelectionModelArrayClass;

GtkType  e_selection_model_array_get_type           (void);

/* Protected Functions */
void     e_selection_model_array_insert_rows        (ESelectionModelArray *esm,
						     int                   row,
						     int                   count);
void     e_selection_model_array_delete_rows        (ESelectionModelArray *esm,
						     int                   row,
						     int                   count);
void     e_selection_model_array_move_row           (ESelectionModelArray *esm,
						     int                   old_row,
						     int                   new_row);
void     e_selection_model_array_confirm_row_count  (ESelectionModelArray *esm);

/* Protected Virtual Function */
gint     e_selection_model_array_get_row_count      (ESelectionModelArray *esm);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* _E_SELECTION_MODEL_ARRAY_H_ */
