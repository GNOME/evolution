/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_SELECTION_MODEL_SIMPLE_H_
#define _E_SELECTION_MODEL_SIMPLE_H_

#include <misc/e-selection-model-array.h>

G_BEGIN_DECLS

#define E_SELECTION_MODEL_SIMPLE_TYPE        (e_selection_model_simple_get_type ())
#define E_SELECTION_MODEL_SIMPLE(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_SELECTION_MODEL_SIMPLE_TYPE, ESelectionModelSimple))
#define E_SELECTION_MODEL_SIMPLE_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_SELECTION_MODEL_SIMPLE_TYPE, ESelectionModelSimpleClass))
#define E_IS_SELECTION_MODEL_SIMPLE(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_SELECTION_MODEL_SIMPLE_TYPE))
#define E_IS_SELECTION_MODEL_SIMPLE_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_SELECTION_MODEL_SIMPLE_TYPE))

typedef struct {
	ESelectionModelArray parent;

	gint row_count;
} ESelectionModelSimple;

typedef struct {
	ESelectionModelArrayClass parent_class;
} ESelectionModelSimpleClass;

GType                  e_selection_model_simple_get_type       (void);
ESelectionModelSimple *e_selection_model_simple_new            (void);

void                   e_selection_model_simple_insert_rows     (ESelectionModelSimple *esms,
								 gint                    row,
								 gint count);
void                   e_selection_model_simple_delete_rows     (ESelectionModelSimple *esms,
								 gint                    row,
								 gint count);
void                   e_selection_model_simple_move_row       (ESelectionModelSimple *esms,
								gint                    old_row,
								gint                    new_row);

void                   e_selection_model_simple_set_row_count  (ESelectionModelSimple *selection,
								gint                    row_count);

G_END_DECLS

#endif /* _E_SELECTION_MODEL_SIMPLE_H_ */

