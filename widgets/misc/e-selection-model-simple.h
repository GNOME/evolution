/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_SELECTION_MODEL_SIMPLE_H_
#define _E_SELECTION_MODEL_SIMPLE_H_

#include <gal/widgets/e-selection-model.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define E_SELECTION_MODEL_SIMPLE_TYPE        (e_selection_model_simple_get_type ())
#define E_SELECTION_MODEL_SIMPLE(o)          (GTK_CHECK_CAST ((o), E_SELECTION_MODEL_SIMPLE_TYPE, ESelectionModelSimple))
#define E_SELECTION_MODEL_SIMPLE_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_SELECTION_MODEL_SIMPLE_TYPE, ESelectionModelSimpleClass))
#define E_IS_SELECTION_MODEL_SIMPLE(o)       (GTK_CHECK_TYPE ((o), E_SELECTION_MODEL_SIMPLE_TYPE))
#define E_IS_SELECTION_MODEL_SIMPLE_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_SELECTION_MODEL_SIMPLE_TYPE))

typedef struct {
	ESelectionModel parent;

	int row_count;
} ESelectionModelSimple;

typedef struct {
	ESelectionModelClass parent_class;
} ESelectionModelSimpleClass;

GtkType                e_selection_model_simple_get_type       (void);
ESelectionModelSimple *e_selection_model_simple_new            (void);

void                   e_selection_model_simple_insert_rows     (ESelectionModelSimple *esms,
								 int                    row,
								 int count);
void                   e_selection_model_simple_delete_rows     (ESelectionModelSimple *esms,
								 int                    row,
								 int count);
void                   e_selection_model_simple_move_row       (ESelectionModelSimple *esms,
								int                    old_row,
								int                    new_row);

void                   e_selection_model_simple_set_row_count  (ESelectionModelSimple *selection,
								int                    row_count);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_SELECTION_MODEL_SIMPLE_H_ */

