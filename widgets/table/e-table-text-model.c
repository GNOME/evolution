/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* ETableTextModel - Text item model for evolution.
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: Chris Lahey <clahey@umich.edu>
 *
 * A majority of code taken from:
 *
 * Text item type for GnomeCanvas widget
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent
 * canvas widget.  Tk is copyrighted by the Regents of the University
 * of California, Sun Microsystems, and other parties.
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx> */

#include <config.h>
#include <ctype.h>
#include "e-table-text-model.h"

static void e_table_text_model_class_init (ETableTextModelClass *class);
static void e_table_text_model_init (ETableTextModel *model);
static void e_table_text_model_destroy (GtkObject *object);

static gchar *e_table_text_model_get_text(ETextModel *model);
static void e_table_text_model_set_text(ETextModel *model, gchar *text);
static void e_table_text_model_insert(ETextModel *model, gint postion, gchar *text);
static void e_table_text_model_insert_length(ETextModel *model, gint postion, gchar *text, gint length);
static void e_table_text_model_delete(ETextModel *model, gint postion, gint length);

static GtkObject *parent_class;



/**
 * e_table_text_model_get_type:
 * @void: 
 * 
 * Registers the &ETableTextModel class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &ETableTextModel class.
 **/
GtkType
e_table_text_model_get_type (void)
{
	static GtkType model_type = 0;

	if (!model_type) {
		GtkTypeInfo model_info = {
			"ETableTextModel",
			sizeof (ETableTextModel),
			sizeof (ETableTextModelClass),
			(GtkClassInitFunc) e_table_text_model_class_init,
			(GtkObjectInitFunc) e_table_text_model_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		model_type = gtk_type_unique (e_text_model_get_type (), &model_info);
	}

	return model_type;
}

/* Class initialization function for the text item */
static void
e_table_text_model_class_init (ETableTextModelClass *klass)
{
	GtkObjectClass *object_class;
	ETextModelClass *model_class;

	object_class = (GtkObjectClass *) klass;
	model_class = (ETextModelClass *) klass;

	parent_class = gtk_type_class (e_text_model_get_type ());

	model_class->get_text = e_table_text_model_get_text;
	model_class->set_text = e_table_text_model_set_text;
	model_class->insert = e_table_text_model_insert;
	model_class->insert_length = e_table_text_model_insert_length;
	model_class->delete = e_table_text_model_delete;
	
	object_class->destroy = e_table_text_model_destroy;
}

/* Object initialization function for the text item */
static void
e_table_text_model_init (ETableTextModel *model)
{
	model->model = NULL;
	model->row = 0;
	model->model_col = 0;
	model->cell_changed_signal_id = 0;
	model->row_changed_signal_id = 0;
}

/* Destroy handler for the text item */
static void
e_table_text_model_destroy (GtkObject *object)
{
	ETableTextModel *model;

	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_TEXT_MODEL (object));

	model = E_TABLE_TEXT_MODEL (object);

	if (model->cell_changed_signal_id)
		gtk_signal_disconnect(GTK_OBJECT(model->model), 
				      model->cell_changed_signal_id);

	if (model->row_changed_signal_id)
		gtk_signal_disconnect(GTK_OBJECT(model->model), 
				      model->row_changed_signal_id);

	if (model->model)
		gtk_object_unref(GTK_OBJECT(model->model));

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}
static gchar *
e_table_text_model_get_text(ETextModel *text_model)
{
	ETableTextModel *model = E_TABLE_TEXT_MODEL(text_model);
	if ( model->model )
		return (gchar *)e_table_model_value_at(model->model, model->model_col, model->row);
	else
		return "";
}

static void
e_table_text_model_set_text(ETextModel *text_model, gchar *text)
{
	ETableTextModel *model = E_TABLE_TEXT_MODEL(text_model);
	if ( model->model )
		e_table_model_set_value_at(model->model, model->model_col, model->row, (void *) text);
}

static void
e_table_text_model_insert(ETextModel *text_model, gint position, gchar *text)
{
	ETableTextModel *model = E_TABLE_TEXT_MODEL(text_model);
	if ( model->model ) {
		gchar *temp = (gchar *)e_table_model_value_at(model->model, model->model_col, model->row);
		temp = g_strdup_printf("%.*s%s%s", position, temp, text, temp + position);
		e_table_model_set_value_at(model->model, model->model_col, model->row, temp);
		g_free(temp);
	}
}

static void
e_table_text_model_insert_length(ETextModel *text_model, gint position, gchar *text, gint length)
{
	ETableTextModel *model = E_TABLE_TEXT_MODEL(text_model);
	if ( model->model ) {
		gchar *temp = (gchar *)e_table_model_value_at(model->model, model->model_col, model->row);
		temp = g_strdup_printf("%.*s%.*s%s", position, temp, length, text, temp + position);
		e_table_model_set_value_at(model->model, model->model_col, model->row, temp);
		g_free(temp);
	}
}

static void
e_table_text_model_delete(ETextModel *text_model, gint position, gint length)
{
	ETableTextModel *model = E_TABLE_TEXT_MODEL(text_model);
	if ( model->model ) {
		gchar *temp = (gchar *)e_table_model_value_at(model->model, model->model_col, model->row);
		temp = g_strdup_printf("%.*s%s", position, temp, temp + position + length);
		e_table_model_set_value_at(model->model, model->model_col, model->row, temp);
		g_free(temp);
	}
}

static void
cell_changed(ETableModel *table_model, int model_col, int row, ETableTextModel *model)
{
	if (model->model_col == model_col &&
	    model->row == row)
		e_text_model_changed(E_TEXT_MODEL(model));
}

static void
row_changed(ETableModel *table_model, int row, ETableTextModel *model)
{
	if (model->row == row)
		e_text_model_changed(E_TEXT_MODEL(model));
}

ETableTextModel *
e_table_text_model_new(ETableModel *table_model, int row, int model_col)
{
	ETableTextModel *model = gtk_type_new (e_table_text_model_get_type ());
	model->model = table_model;
	if ( model->model ) {
		gtk_object_ref(GTK_OBJECT(model->model));
		model->cell_changed_signal_id = 
			gtk_signal_connect(GTK_OBJECT(model->model),
					   "model_cell_changed",
					   GTK_SIGNAL_FUNC(cell_changed),
					   model);
		model->row_changed_signal_id = 
			gtk_signal_connect(GTK_OBJECT(model->model),
					   "model_row_changed",
					   GTK_SIGNAL_FUNC(row_changed),
					   model);
	}
	model->row = row;
	model->model_col = model_col;
	return model;
}

