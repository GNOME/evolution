/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-selection-model-simple.c: a Table Selection Model
 *
 * Author:
 *   Christopher James Lahey <clahey@ximian.com>
 *
 * (C) 2000, 2001 Ximian, Inc.
 */
#include <config.h>
#include <gal/util/e-util.h>
#include "e-selection-model-simple.h"

#define ESMS_CLASS(e) ((ESelectionModelSimpleClass *)((GtkObject *)e)->klass)

#define PARENT_TYPE e_selection_model_get_type ()

static ESelectionModel *parent_class;

static gint esms_get_row_count (ESelectionModel *esm);

static void
e_selection_model_simple_init (ESelectionModelSimple *selection)
{
	selection->row_count = 0;
}

static void
e_selection_model_simple_class_init (ESelectionModelSimpleClass *klass)
{
	ESelectionModelClass *esm_class;

	parent_class             = gtk_type_class (PARENT_TYPE);

	esm_class                = E_SELECTION_MODEL_CLASS(klass);

	esm_class->get_row_count = esms_get_row_count;
}

E_MAKE_TYPE(e_selection_model_simple, "ESelectionModelSimple", ESelectionModelSimple,
	    e_selection_model_simple_class_init, e_selection_model_simple_init, PARENT_TYPE);

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
	return gtk_type_new (e_selection_model_simple_get_type ());
}

void
e_selection_model_simple_set_row_count (ESelectionModelSimple *esms,
					int row_count)
{
	if (esms->row_count != row_count) {
		ESelectionModel *esm = E_SELECTION_MODEL(esms);
		g_free(esm->selection);
		esm->selection = NULL;
		esm->row_count = -1;
	}
	esms->row_count = row_count;
}

static gint
esms_get_row_count (ESelectionModel *esm)
{
	ESelectionModelSimple *esms = E_SELECTION_MODEL_SIMPLE(esm);

	return esms->row_count;
}

void      e_selection_model_simple_insert_rows         (ESelectionModelSimple *esms,
							int                    row,
							int                    count)
{
	esms->row_count += count;
	e_selection_model_insert_rows (E_SELECTION_MODEL(esms), row, count);
}

void
e_selection_model_simple_delete_rows          (ESelectionModelSimple *esms,
					       int                    row,
					       int                    count)
{
	esms->row_count -= count;
	e_selection_model_delete_rows (E_SELECTION_MODEL(esms), row, count);
}

void
e_selection_model_simple_move_row            (ESelectionModelSimple *esms,
					      int                    old_row,
					      int                    new_row)
{
	e_selection_model_move_row (E_SELECTION_MODEL(esms), old_row, new_row);
}
