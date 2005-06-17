/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-selection-model-array.h
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

#ifndef _E_SELECTION_MODEL_ARRAY_H_
#define _E_SELECTION_MODEL_ARRAY_H_

#include <gtk/gtkobject.h>
#include <widgets/misc/e-selection-model.h>
#include <e-util/e-bit-array.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define E_SELECTION_MODEL_ARRAY_TYPE        (e_selection_model_array_get_type ())
#define E_SELECTION_MODEL_ARRAY(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_SELECTION_MODEL_ARRAY_TYPE, ESelectionModelArray))
#define E_SELECTION_MODEL_ARRAY_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_SELECTION_MODEL_ARRAY_TYPE, ESelectionModelArrayClass))
#define E_IS_SELECTION_MODEL_ARRAY(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_SELECTION_MODEL_ARRAY_TYPE))
#define E_IS_SELECTION_MODEL_ARRAY_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_SELECTION_MODEL_ARRAY_TYPE))
#define E_SELECTION_MODEL_ARRAY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), E_SELECTION_MODEL_ARRAY_TYPE, ESelectionModelArrayClass))

typedef struct {
	ESelectionModel base;

	EBitArray *eba;

	gint cursor_row;
	gint cursor_col;
	gint selection_start_row;

	guint model_changed_id;
	guint model_row_inserted_id, model_row_deleted_id;

	/* Anything other than -1 means that the selection is a single
	 * row.  This being -1 does not impart any information. */
	gint        selected_row;
	/* Anything other than -1 means that the selection is a all
	 * rows between selection_start_path and cursor_path where
	 * selected_range_end is the rwo number of cursor_path.  This
	 * being -1 does not impart any information. */
	gint        selected_range_end;

	guint frozen : 1;
	guint selection_model_changed : 1;
	guint group_info_changed : 1;
} ESelectionModelArray;

typedef struct {
	ESelectionModelClass parent_class;

	gint (*get_row_count)     (ESelectionModelArray *selection);
} ESelectionModelArrayClass;

GType    e_selection_model_array_get_type           (void);

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
