/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-text-model.c
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
#include <ctype.h>
#include <gtk/gtksignal.h>
#include <gal/util/e-util.h>
#include "e-table-text-model.h"

static void e_table_text_model_class_init (ETableTextModelClass *class);
static void e_table_text_model_init (ETableTextModel *model);
static void e_table_text_model_destroy (GtkObject *object);

static const gchar *e_table_text_model_get_text (ETextModel *model);
static void e_table_text_model_set_text (ETextModel *model, const gchar *text);
static void e_table_text_model_insert (ETextModel *model, gint postion, const gchar *text);
static void e_table_text_model_insert_length (ETextModel *model, gint postion, const gchar *text, gint length);
static void e_table_text_model_delete (ETextModel *model, gint postion, gint length);

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
	g_return_if_fail (E_IS_TABLE_TEXT_MODEL (object));

	model = E_TABLE_TEXT_MODEL (object);
	
	if (model->model)
		g_assert (GTK_IS_OBJECT (model->model));

	if (model->cell_changed_signal_id)
		gtk_signal_disconnect (GTK_OBJECT(model->model), 
				      model->cell_changed_signal_id);
	model->cell_changed_signal_id = 0;

	if (model->row_changed_signal_id)
		gtk_signal_disconnect (GTK_OBJECT(model->model), 
				      model->row_changed_signal_id);
	model->row_changed_signal_id = 0;

	if (model->model)
		gtk_object_unref (GTK_OBJECT(model->model));
	model->model = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}
static const gchar *
e_table_text_model_get_text (ETextModel *text_model)
{
	ETableTextModel *model = E_TABLE_TEXT_MODEL(text_model);
	if (model->model)
		return (gchar *)e_table_model_value_at (model->model, model->model_col, model->row);
	else
		return "";
}

static void
e_table_text_model_set_text (ETextModel *text_model, const gchar *text)
{
	ETableTextModel *model = E_TABLE_TEXT_MODEL(text_model);
	if (model->model)
		e_table_model_set_value_at (model->model, model->model_col, model->row, (void *) text);
}

static void
e_table_text_model_insert (ETextModel *text_model, gint position, const gchar *text)
{
	ETableTextModel *model = E_TABLE_TEXT_MODEL(text_model);
	if (model->model){
		gchar *temp = (gchar *)e_table_model_value_at (model->model, model->model_col, model->row);
		/* Can't use g_strdup_printf here because on some
		   systems printf ("%.*s"); is locale dependent. */
		temp = e_strdup_append_strings (temp, position,
						text, -1,
						temp + position, -1,
						NULL);
		e_table_model_set_value_at (model->model, model->model_col, model->row, temp);
		g_free (temp);
	}
}

static void
e_table_text_model_insert_length (ETextModel *text_model, gint position, const gchar *text, gint length)
{
	ETableTextModel *model = E_TABLE_TEXT_MODEL(text_model);
	if (model->model){
		gchar *temp = (gchar *)e_table_model_value_at (model->model, model->model_col, model->row);
		/* Can't use g_strdup_printf here because on some
		   systems printf ("%.*s"); is locale dependent. */
		temp = e_strdup_append_strings (temp, position,
						text, length,
						temp + position, -1,
						NULL);
		e_table_model_set_value_at (model->model, model->model_col, model->row, temp);
		g_free (temp);
	}
}

static void
e_table_text_model_delete (ETextModel *text_model, gint position, gint length)
{
	ETableTextModel *model = E_TABLE_TEXT_MODEL(text_model);
	if (model->model){
		gchar *temp = (gchar *)e_table_model_value_at (model->model, model->model_col, model->row);
		/* Can't use g_strdup_printf here because on some
		   systems printf ("%.*s"); is locale dependent. */
		temp = e_strdup_append_strings (temp, position,
						temp + position + length, -1,
						NULL);
		e_table_model_set_value_at (model->model, model->model_col, model->row, temp);
		g_free (temp);
	}
}

static void
cell_changed (ETableModel *table_model, int model_col, int row, ETableTextModel *model)
{
	if (model->model_col == model_col &&
	    model->row == row)
		e_text_model_changed (E_TEXT_MODEL(model));
}

static void
row_changed (ETableModel *table_model, int row, ETableTextModel *model)
{
	if (model->row == row)
		e_text_model_changed (E_TEXT_MODEL(model));
}

ETableTextModel *
e_table_text_model_new (ETableModel *table_model, int row, int model_col)
{
	ETableTextModel *model;

	g_return_val_if_fail(table_model != NULL, NULL);
	g_return_val_if_fail(E_IS_TABLE_MODEL(table_model), NULL);

	model = gtk_type_new (e_table_text_model_get_type ());
	model->model = table_model;
	if (model->model){
		gtk_object_ref (GTK_OBJECT(model->model));
		model->cell_changed_signal_id = 
			gtk_signal_connect (GTK_OBJECT(model->model),
					   "model_cell_changed",
					   GTK_SIGNAL_FUNC(cell_changed),
					   model);
		model->row_changed_signal_id = 
			gtk_signal_connect (GTK_OBJECT(model->model),
					   "model_row_changed",
					   GTK_SIGNAL_FUNC(row_changed),
					   model);
	}
	model->row = row;
	model->model_col = model_col;
	return model;
}

