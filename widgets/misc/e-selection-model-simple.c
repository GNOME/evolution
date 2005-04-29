/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-selection-model-simple.c
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

#include <config.h>

#include "gal/util/e-util.h"

#include "e-selection-model-array.h"
#include "e-selection-model-simple.h"

#define PARENT_TYPE e_selection_model_array_get_type ()

static ESelectionModelArray *parent_class;

static gint esms_get_row_count (ESelectionModelArray *esma);

static void
e_selection_model_simple_init (ESelectionModelSimple *selection)
{
	selection->row_count = 0;
}

static void
e_selection_model_simple_class_init (ESelectionModelSimpleClass *klass)
{
	ESelectionModelArrayClass *esma_class;

	parent_class              = g_type_class_ref (PARENT_TYPE);

	esma_class                = E_SELECTION_MODEL_ARRAY_CLASS(klass);

	esma_class->get_row_count = esms_get_row_count;
}

E_MAKE_TYPE(e_selection_model_simple, "ESelectionModelSimple", ESelectionModelSimple,
	    e_selection_model_simple_class_init, e_selection_model_simple_init, PARENT_TYPE)

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
					int row_count)
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
							int                    row,
							int                    count)
{
	esms->row_count += count;
	e_selection_model_array_insert_rows (E_SELECTION_MODEL_ARRAY(esms), row, count);
}

void
e_selection_model_simple_delete_rows          (ESelectionModelSimple *esms,
					       int                    row,
					       int                    count)
{
	esms->row_count -= count;
	e_selection_model_array_delete_rows (E_SELECTION_MODEL_ARRAY(esms), row, count);
}

void
e_selection_model_simple_move_row            (ESelectionModelSimple *esms,
					      int                    old_row,
					      int                    new_row)
{
	e_selection_model_array_move_row (E_SELECTION_MODEL_ARRAY(esms), old_row, new_row);
}
