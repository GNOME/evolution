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
#include <gtk/gtksignal.h>
#include "e-table-selection-model.h"
#include "gal/util/e-util.h"
#include <gdk/gdkkeysyms.h>

#define ETSM_CLASS(e) ((ETableSelectionModelClass *)((GtkObject *)e)->klass)

#define PARENT_TYPE e_selection_model_get_type ()

static ESelectionModel *parent_class;

static gint etsm_get_row_count (ESelectionModel *esm);

enum {
	ARG_0,
	ARG_MODEL,
};

static void
model_changed(ETableModel *etm, ETableSelectionModel *etsm)
{
	e_selection_model_clear(E_SELECTION_MODEL(etsm));
}

#if 1
static void
model_row_inserted(ETableModel *etm, int row, ETableSelectionModel *etsm)
{
	e_selection_model_insert_row(E_SELECTION_MODEL(etsm), row);
}

static void
model_row_deleted(ETableModel *etm, int row, ETableSelectionModel *etsm)
{
	e_selection_model_delete_row(E_SELECTION_MODEL(etsm), row);
}

#else

static void
model_row_inserted(ETableModel *etm, int row, ETableSelectionModel *etsm)
{
	model_changed(etm, etsm);
}

static void
model_row_deleted(ETableModel *etm, int row, ETableSelectionModel *etsm)
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
		etsm->model_changed_id = gtk_signal_connect(GTK_OBJECT(model), "model_changed",
							    GTK_SIGNAL_FUNC(model_changed), etsm);
		etsm->model_row_inserted_id = gtk_signal_connect(GTK_OBJECT(model), "model_row_inserted",
								 GTK_SIGNAL_FUNC(model_row_inserted), etsm);
		etsm->model_row_deleted_id = gtk_signal_connect(GTK_OBJECT(model), "model_row_deleted",
								GTK_SIGNAL_FUNC(model_row_deleted), etsm);
	}
}

inline static void
drop_model(ETableSelectionModel *etsm)
{
	if (etsm->model) {
		gtk_signal_disconnect(GTK_OBJECT(etsm->model),
				      etsm->model_changed_id);
		gtk_signal_disconnect(GTK_OBJECT(etsm->model),
				      etsm->model_row_inserted_id);
		gtk_signal_disconnect(GTK_OBJECT(etsm->model),
				      etsm->model_row_deleted_id);
		gtk_object_unref(GTK_OBJECT(etsm->model));
	}
	etsm->model = NULL;
}

static void
etsm_destroy (GtkObject *object)
{
	ETableSelectionModel *etsm;

	etsm = E_TABLE_SELECTION_MODEL (object);

	drop_model(etsm);

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
}

static void
e_table_selection_model_class_init (ETableSelectionModelClass *klass)
{
	GtkObjectClass *object_class;
	ESelectionModelClass *esm_class;

	parent_class             = gtk_type_class (PARENT_TYPE);

	object_class             = GTK_OBJECT_CLASS(klass);
	esm_class                = E_SELECTION_MODEL_CLASS(klass);

	object_class->destroy    = etsm_destroy;
	object_class->get_arg    = etsm_get_arg;
	object_class->set_arg    = etsm_set_arg;

	esm_class->get_row_count = etsm_get_row_count;

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
etsm_get_row_count (ESelectionModel *esm)
{
	ETableSelectionModel *etsm = E_TABLE_SELECTION_MODEL(esm);

	return e_table_model_row_count (etsm->model);
}
