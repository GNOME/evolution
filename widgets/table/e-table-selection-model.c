/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-table-selection-model.c: a Table Selection Model
 *
 * Author:
 *   Christopher James Lahey <clahey@ximian.com>
 *
 * (C) 2000, 2001 Ximian, Inc.
 */
#include <config.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtksignal.h>
#include "e-table-selection-model.h"
#include "gal/util/e-util.h"

#define ETSM_CLASS(e) ((ETableSelectionModelClass *)((GtkObject *)e)->klass)

#define PARENT_TYPE e_selection_model_array_get_type ()

static ESelectionModelArray *parent_class;

static gint etsm_get_row_count (ESelectionModelArray *esm);

enum {
	ARG_0,
	ARG_MODEL,
};

#if 0
static void
save_to_hash(int model_row, gpointer closure)
{
	ETableSelectionModel *etsm = closure;
	gchar *key = e_table_model_get_save_id(etsm->model, model_row);

	g_hash_table_insert(etsm->hash, key, key);
}
#endif

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
	g_free(etsm->cursor_id);
	etsm->cursor_id = NULL;
}

static void
model_pre_change (ETableModel *etm, ETableSelectionModel *etsm)
{
	free_hash(etsm);

#if 0
	if (etsm->model && (!etsm->hash) && e_table_model_has_save_id(etsm->model)) {
		gint cursor_row;
		etsm->hash = g_hash_table_new(g_str_hash, g_str_equal);
		e_selection_model_foreach(E_SELECTION_MODEL(etsm), save_to_hash, etsm);
		gtk_object_get(GTK_OBJECT(etsm),
			       "cursor_row", &cursor_row,
			       NULL);
		if (cursor_row != -1) {
			etsm->cursor_id = e_table_model_get_save_id(etm, cursor_row);
		}
	}
#endif
}

#if 0
static gint
model_changed_idle(ETableSelectionModel *etsm)
{
	ETableModel *etm = etsm->model;

	e_selection_model_clear(E_SELECTION_MODEL(etsm));

	if (etsm->hash && etm && e_table_model_has_save_id(etm)) {
		int row_count = e_table_model_row_count(etm);
		int i;
		e_selection_model_array_confirm_row_count(E_SELECTION_MODEL_ARRAY(etsm));
		for (i = 0; i < row_count; i++) {
			char *save_id = e_table_model_get_save_id(etm, i);
			if (g_hash_table_lookup(etsm->hash, save_id))
				e_selection_model_change_one_row(E_SELECTION_MODEL(etsm), i, TRUE);
			if (etsm->cursor_id && !strcmp(etsm->cursor_id, save_id)) {
				e_selection_model_change_cursor(E_SELECTION_MODEL(etsm), i, e_selection_model_cursor_row(E_SELECTION_MODEL(etsm)));
				g_free(etsm->cursor_id);
				etsm->cursor_id = NULL;
			}
			g_free(save_id);
		}
		free_hash(etsm);
	}
	etsm->model_changed_idle_id = 0;
	return FALSE;
}
#endif

static void
model_changed(ETableModel *etm, ETableSelectionModel *etsm)
{
	e_selection_model_clear(E_SELECTION_MODEL(etsm));
#if 0
	if (!etsm->model_changed_idle_id && etm && e_table_model_has_save_id(etm)) {
		etsm->model_changed_idle_id = g_idle_add_full(G_PRIORITY_HIGH, (GSourceFunc) model_changed_idle, etsm, NULL);
	}
#endif
}

static void
model_row_changed(ETableModel *etm, int row, ETableSelectionModel *etsm)
{
	if (etsm->hash)
		free_hash(etsm);
}

static void
model_cell_changed(ETableModel *etm, int col, int row, ETableSelectionModel *etsm)
{
	if (etsm->hash)
		free_hash(etsm);
}

#if 1
static void
model_rows_inserted(ETableModel *etm, int row, int count, ETableSelectionModel *etsm)
{
	e_selection_model_array_insert_rows(E_SELECTION_MODEL_ARRAY(etsm), row, count);
	if (etsm->hash)
		free_hash(etsm);
}

static void
model_rows_deleted(ETableModel *etm, int row, int count, ETableSelectionModel *etsm)
{
	e_selection_model_array_delete_rows(E_SELECTION_MODEL_ARRAY(etsm), row, count);
	if (etsm->hash)
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
		gtk_object_ref(GTK_OBJECT(model));
		etsm->model_pre_change_id = gtk_signal_connect(GTK_OBJECT(model), "model_pre_change",
							       GTK_SIGNAL_FUNC(model_pre_change), etsm);
		etsm->model_changed_id = gtk_signal_connect(GTK_OBJECT(model), "model_changed",
							    GTK_SIGNAL_FUNC(model_changed), etsm);
		etsm->model_row_changed_id = gtk_signal_connect(GTK_OBJECT(model), "model_row_changed",
								GTK_SIGNAL_FUNC(model_row_changed), etsm);
		etsm->model_cell_changed_id = gtk_signal_connect(GTK_OBJECT(model), "model_cell_changed",
								 GTK_SIGNAL_FUNC(model_cell_changed), etsm);
		etsm->model_rows_inserted_id = gtk_signal_connect(GTK_OBJECT(model), "model_rows_inserted",
								  GTK_SIGNAL_FUNC(model_rows_inserted), etsm);
		etsm->model_rows_deleted_id = gtk_signal_connect(GTK_OBJECT(model), "model_rows_deleted",
								 GTK_SIGNAL_FUNC(model_rows_deleted), etsm);
	}
}

inline static void
drop_model(ETableSelectionModel *etsm)
{
	if (etsm->model) {
		gtk_signal_disconnect(GTK_OBJECT(etsm->model),
				      etsm->model_pre_change_id);
		gtk_signal_disconnect(GTK_OBJECT(etsm->model),
				      etsm->model_changed_id);
		gtk_signal_disconnect(GTK_OBJECT(etsm->model),
				      etsm->model_row_changed_id);
		gtk_signal_disconnect(GTK_OBJECT(etsm->model),
				      etsm->model_cell_changed_id);
		gtk_signal_disconnect(GTK_OBJECT(etsm->model),
				      etsm->model_rows_inserted_id);
		gtk_signal_disconnect(GTK_OBJECT(etsm->model),
				      etsm->model_rows_deleted_id);

		gtk_object_unref(GTK_OBJECT(etsm->model));
	}
	etsm->model = NULL;
}

static void
etsm_destroy (GtkObject *object)
{
	ETableSelectionModel *etsm;

	etsm = E_TABLE_SELECTION_MODEL (object);

	if (etsm->model_changed_idle_id) {
		g_source_remove(etsm->model_changed_idle_id);
	}
	drop_model(etsm);
	free_hash(etsm);

	if (GTK_OBJECT_CLASS(parent_class)->destroy)
		GTK_OBJECT_CLASS(parent_class)->destroy (object);
}

static void
etsm_get_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	ETableSelectionModel *etsm = E_TABLE_SELECTION_MODEL (o);

	switch (arg_id){
	case ARG_MODEL:
		GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(etsm->model);
		break;
	}
}

static void
etsm_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	ETableSelectionModel *etsm = E_TABLE_SELECTION_MODEL (o);
	
	switch (arg_id){
	case ARG_MODEL:
		drop_model(etsm);
		add_model(etsm, GTK_VALUE_OBJECT (*arg) ? E_TABLE_MODEL(GTK_VALUE_OBJECT (*arg)) : NULL);
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
	GtkObjectClass *object_class;
	ESelectionModelArrayClass *esma_class;

	parent_class             = gtk_type_class (PARENT_TYPE);

	object_class             = GTK_OBJECT_CLASS(klass);
	esma_class               = E_SELECTION_MODEL_ARRAY_CLASS(klass);

	object_class->destroy    = etsm_destroy;
	object_class->get_arg    = etsm_get_arg;
	object_class->set_arg    = etsm_set_arg;

	esma_class->get_row_count = etsm_get_row_count;

	gtk_object_add_arg_type ("ETableSelectionModel::model", GTK_TYPE_OBJECT,
				 GTK_ARG_READWRITE, ARG_MODEL);
}

E_MAKE_TYPE(e_table_selection_model, "ETableSelectionModel", ETableSelectionModel,
	    e_table_selection_model_class_init, e_table_selection_model_init, PARENT_TYPE);

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
	return gtk_type_new (e_table_selection_model_get_type ());
}

static gint
etsm_get_row_count (ESelectionModelArray *esma)
{
	ETableSelectionModel *etsm = E_TABLE_SELECTION_MODEL(esma);

	return e_table_model_row_count (etsm->model);
}
