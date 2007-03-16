/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-table-selection-model.c
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

#include <string.h>

#include <gdk/gdkkeysyms.h>

#include "e-util/e-i18n.h"
#include "e-util/e-util.h"

#include "e-table-selection-model.h"

#define PARENT_TYPE e_selection_model_array_get_type ()

static ESelectionModelArray *parent_class;

static gint etsm_get_row_count (ESelectionModelArray *esm);

enum {
	PROP_0,
	PROP_MODEL,
	PROP_HEADER
};

static void
save_to_hash(int model_row, gpointer closure)
{
	ETableSelectionModel *etsm = closure;
	gchar *key = e_table_model_get_save_id(etsm->model, model_row);

	g_hash_table_insert(etsm->hash, key, key);
}

static void
free_key(gpointer key, gpointer value, gpointer closure)
{
	g_free(key);
}

static void
free_hash(ETableSelectionModel *etsm)
{
	if (etsm->hash) {
		g_hash_table_foreach(etsm->hash, free_key, NULL);
		g_hash_table_destroy(etsm->hash);
		etsm->hash = NULL;
	}
	if (etsm->cursor_id)
		g_free(etsm->cursor_id);
	etsm->cursor_id = NULL;
}

static void
model_pre_change (ETableModel *etm, ETableSelectionModel *etsm)
{
	free_hash(etsm);

	if (etsm->model && e_table_model_has_save_id (etsm->model)) {
		gint cursor_row;

		etsm->hash = g_hash_table_new(g_str_hash, g_str_equal);
		e_selection_model_foreach(E_SELECTION_MODEL(etsm), save_to_hash, etsm);

		g_object_get(etsm,
			     "cursor_row", &cursor_row,
			     NULL);
		g_free (etsm->cursor_id);
		if (cursor_row != -1)
			etsm->cursor_id = e_table_model_get_save_id(etm, cursor_row);
		else
			etsm->cursor_id = NULL;
	}
}

static gint
model_changed_idle(ETableSelectionModel *etsm)
{
	ETableModel *etm = etsm->model;

	e_selection_model_clear(E_SELECTION_MODEL(etsm));

	if (etsm->cursor_id && etm && e_table_model_has_save_id(etm)) {
		int row_count = e_table_model_row_count(etm);
		int cursor_row = -1;
		int cursor_col = -1;
		int i;
		e_selection_model_array_confirm_row_count(E_SELECTION_MODEL_ARRAY(etsm));
		for (i = 0; i < row_count; i++) {
			char *save_id = e_table_model_get_save_id(etm, i);
			if (g_hash_table_lookup(etsm->hash, save_id))
				e_selection_model_change_one_row(E_SELECTION_MODEL(etsm), i, TRUE);
			
			if (etsm->cursor_id && !strcmp(etsm->cursor_id, save_id)) {
				cursor_row = i;
				cursor_col = e_selection_model_cursor_col(E_SELECTION_MODEL(etsm));
				if (cursor_col == -1) {
					if (etsm->eth) {
						cursor_col = e_table_header_prioritized_column (etsm->eth);
					} else
						cursor_col = 0;
				}
				e_selection_model_change_cursor(E_SELECTION_MODEL(etsm), cursor_row, cursor_col);
				g_free(etsm->cursor_id);
				etsm->cursor_id = NULL;
			}
			g_free(save_id);
		}
		free_hash(etsm);
		e_selection_model_cursor_changed (E_SELECTION_MODEL(etsm), cursor_row, cursor_col);
		e_selection_model_selection_changed (E_SELECTION_MODEL(etsm));
	}
	etsm->model_changed_idle_id = 0;
	return FALSE;
}

static void
model_changed(ETableModel *etm, ETableSelectionModel *etsm)
{
	e_selection_model_clear(E_SELECTION_MODEL(etsm));
	if (!etsm->model_changed_idle_id && etm && e_table_model_has_save_id(etm)) {
		etsm->model_changed_idle_id = g_idle_add_full(G_PRIORITY_HIGH, (GSourceFunc) model_changed_idle, etsm, NULL);
	}
}

static void
model_row_changed(ETableModel *etm, int row, ETableSelectionModel *etsm)
{
	free_hash(etsm);
}

static void
model_cell_changed(ETableModel *etm, int col, int row, ETableSelectionModel *etsm)
{
	free_hash(etsm);
}

#if 1
static void
model_rows_inserted(ETableModel *etm, int row, int count, ETableSelectionModel *etsm)
{
	e_selection_model_array_insert_rows(E_SELECTION_MODEL_ARRAY(etsm), row, count);
	free_hash(etsm);
}

static void
model_rows_deleted(ETableModel *etm, int row, int count, ETableSelectionModel *etsm)
{
	e_selection_model_array_delete_rows(E_SELECTION_MODEL_ARRAY(etsm), row, count);
	free_hash(etsm);
}

#else

static void
model_rows_inserted(ETableModel *etm, int row, int count, ETableSelectionModel *etsm)
{
	model_changed(etm, etsm);
}

static void
model_rows_deleted(ETableModel *etm, int row, int count, ETableSelectionModel *etsm)
{
	model_changed(etm, etsm);
}
#endif

inline static void
add_model(ETableSelectionModel *etsm, ETableModel *model)
{
	etsm->model = model;
	if (model) {
		g_object_ref(model);
		etsm->model_pre_change_id = g_signal_connect(G_OBJECT(model), "model_pre_change",
							G_CALLBACK(model_pre_change), etsm);
		etsm->model_changed_id = g_signal_connect(G_OBJECT(model), "model_changed",
							G_CALLBACK(model_changed), etsm);
		etsm->model_row_changed_id = g_signal_connect(G_OBJECT(model), "model_row_changed",
							G_CALLBACK(model_row_changed), etsm);
		etsm->model_cell_changed_id = g_signal_connect(G_OBJECT(model), "model_cell_changed",
							G_CALLBACK(model_cell_changed), etsm);
		etsm->model_rows_inserted_id = g_signal_connect(G_OBJECT(model), "model_rows_inserted",
							G_CALLBACK(model_rows_inserted), etsm);
		etsm->model_rows_deleted_id = g_signal_connect(G_OBJECT(model), "model_rows_deleted",
							G_CALLBACK(model_rows_deleted), etsm);
	}
	e_selection_model_array_confirm_row_count(E_SELECTION_MODEL_ARRAY(etsm));
}

inline static void
drop_model(ETableSelectionModel *etsm)
{
	if (etsm->model) {
		g_signal_handler_disconnect(G_OBJECT(etsm->model),
					    etsm->model_pre_change_id);
		g_signal_handler_disconnect(G_OBJECT(etsm->model),
					    etsm->model_changed_id);
		g_signal_handler_disconnect(G_OBJECT(etsm->model),
					    etsm->model_row_changed_id);
		g_signal_handler_disconnect(G_OBJECT(etsm->model),
					    etsm->model_cell_changed_id);
		g_signal_handler_disconnect(G_OBJECT(etsm->model),
					    etsm->model_rows_inserted_id);
		g_signal_handler_disconnect(G_OBJECT(etsm->model),
					    etsm->model_rows_deleted_id);

		g_object_unref(etsm->model);
	}
	etsm->model = NULL;
}

static void
etsm_dispose (GObject *object)
{
	ETableSelectionModel *etsm;

	etsm = E_TABLE_SELECTION_MODEL (object);

	if (etsm->model_changed_idle_id)
		g_source_remove (etsm->model_changed_idle_id);
	etsm->model_changed_idle_id = 0;

	drop_model(etsm);
	free_hash(etsm);

	if (G_OBJECT_CLASS(parent_class)->dispose)
		G_OBJECT_CLASS(parent_class)->dispose (object);
}

static void
etsm_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	ETableSelectionModel *etsm = E_TABLE_SELECTION_MODEL (object);

	switch (prop_id){
	case PROP_MODEL:
		g_value_set_object (value, etsm->model);
		break;
	case PROP_HEADER:
		g_value_set_object (value, etsm->eth);
		break;
	}
}

static void
etsm_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	ETableSelectionModel *etsm = E_TABLE_SELECTION_MODEL (object);
	
	switch (prop_id){
	case PROP_MODEL:
		drop_model(etsm);
		add_model(etsm, g_value_get_object (value) ? E_TABLE_MODEL(g_value_get_object (value)) : NULL);
		break;
	case PROP_HEADER:
		etsm->eth = E_TABLE_HEADER (g_value_get_object (value));
		break;
	}
}

static void
e_table_selection_model_init (ETableSelectionModel *selection)
{
	selection->model = NULL;
	selection->hash = NULL;
	selection->cursor_id = NULL;

	selection->model_changed_idle_id = 0;
}

static void
e_table_selection_model_class_init (ETableSelectionModelClass *klass)
{
	GObjectClass *object_class;
	ESelectionModelArrayClass *esma_class;

	parent_class             = g_type_class_ref (PARENT_TYPE);

	object_class             = G_OBJECT_CLASS(klass);
	esma_class               = E_SELECTION_MODEL_ARRAY_CLASS(klass);

	object_class->dispose      = etsm_dispose;
	object_class->get_property = etsm_get_property;
	object_class->set_property = etsm_set_property;

	esma_class->get_row_count = etsm_get_row_count;

	g_object_class_install_property (object_class, PROP_MODEL, 
					 g_param_spec_object ("model",
							      _("Model"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TABLE_MODEL_TYPE,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_HEADER, 
					 g_param_spec_object ("header",
							      _("Header"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TABLE_HEADER_TYPE,
							      G_PARAM_READWRITE));
}

E_MAKE_TYPE(e_table_selection_model, "ETableSelectionModel", ETableSelectionModel,
	    e_table_selection_model_class_init, e_table_selection_model_init, PARENT_TYPE)

/** 
 * e_table_selection_model_new
 *
 * This routine creates a new #ETableSelectionModel.
 *
 * Returns: The new #ETableSelectionModel.
 */
ETableSelectionModel *
e_table_selection_model_new (void)
{
	return g_object_new (E_TABLE_SELECTION_MODEL_TYPE, NULL);
}

static gint
etsm_get_row_count (ESelectionModelArray *esma)
{
	ETableSelectionModel *etsm = E_TABLE_SELECTION_MODEL(esma);

	if (etsm->model)
		return e_table_model_row_count (etsm->model);
	else
		return 0;
}
