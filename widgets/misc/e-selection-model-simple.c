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

#include <config.h>

#include "e-util/e-util.h"

#include "e-selection-model-array.h"
#include "e-selection-model-simple.h"

static gint esms_get_row_count (ESelectionModelArray *esma);

G_DEFINE_TYPE (ESelectionModelSimple, e_selection_model_simple, e_selection_model_array_get_type())

static void
e_selection_model_simple_init (ESelectionModelSimple *selection)
{
	selection->row_count = 0;
}

static void
e_selection_model_simple_class_init (ESelectionModelSimpleClass *klass)
{
	ESelectionModelArrayClass *esma_class;

	esma_class                = E_SELECTION_MODEL_ARRAY_CLASS(klass);

	esma_class->get_row_count = esms_get_row_count;
}

/**
 * e_selection_model_simple_new
 *
 * This routine creates a new #ESelectionModelSimple.
 *
 * Returns: The new #ESelectionModelSimple.
 */
ESelectionModelSimple *
e_selection_model_simple_new (void)
{
	return g_object_new (E_SELECTION_MODEL_SIMPLE_TYPE, NULL);
}

void
e_selection_model_simple_set_row_count (ESelectionModelSimple *esms,
					gint row_count)
{
	if (esms->row_count != row_count) {
		ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY(esms);
		if (esma->eba)
			g_object_unref(esma->eba);
		esma->eba = NULL;
		esma->selected_row = -1;
		esma->selected_range_end = -1;
	}
	esms->row_count = row_count;
}

static gint
esms_get_row_count (ESelectionModelArray *esma)
{
	ESelectionModelSimple *esms = E_SELECTION_MODEL_SIMPLE(esma);

	return esms->row_count;
}

void      e_selection_model_simple_insert_rows         (ESelectionModelSimple *esms,
							gint                    row,
							gint                    count)
{
	esms->row_count += count;
	e_selection_model_array_insert_rows (E_SELECTION_MODEL_ARRAY(esms), row, count);
}

void
e_selection_model_simple_delete_rows          (ESelectionModelSimple *esms,
					       gint                    row,
					       gint                    count)
{
	esms->row_count -= count;
	e_selection_model_array_delete_rows (E_SELECTION_MODEL_ARRAY(esms), row, count);
}

void
e_selection_model_simple_move_row            (ESelectionModelSimple *esms,
					      gint                    old_row,
					      gint                    new_row)
{
	e_selection_model_array_move_row (E_SELECTION_MODEL_ARRAY(esms), old_row, new_row);
}
