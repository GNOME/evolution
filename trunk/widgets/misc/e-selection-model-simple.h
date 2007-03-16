/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-selection-model-simple.h
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

#ifndef _E_SELECTION_MODEL_SIMPLE_H_
#define _E_SELECTION_MODEL_SIMPLE_H_

#include <misc/e-selection-model-array.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define E_SELECTION_MODEL_SIMPLE_TYPE        (e_selection_model_simple_get_type ())
#define E_SELECTION_MODEL_SIMPLE(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_SELECTION_MODEL_SIMPLE_TYPE, ESelectionModelSimple))
#define E_SELECTION_MODEL_SIMPLE_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_SELECTION_MODEL_SIMPLE_TYPE, ESelectionModelSimpleClass))
#define E_IS_SELECTION_MODEL_SIMPLE(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_SELECTION_MODEL_SIMPLE_TYPE))
#define E_IS_SELECTION_MODEL_SIMPLE_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_SELECTION_MODEL_SIMPLE_TYPE))

typedef struct {
	ESelectionModelArray parent;

	int row_count;
} ESelectionModelSimple;

typedef struct {
	ESelectionModelArrayClass parent_class;
} ESelectionModelSimpleClass;

GType                  e_selection_model_simple_get_type       (void);
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

