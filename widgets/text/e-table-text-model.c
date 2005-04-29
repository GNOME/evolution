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

#include <gtk/gtk.h>

#include "gal/util/e-util.h"

#include "e-table-text-model.h"

static void e_table_text_model_class_init (ETableTextModelClass *class);
static void e_table_text_model_init (ETableTextModel *model);
static void e_table_text_model_dispose (GObject *object);

static const gchar *e_table_text_model_get_text (ETextModel *model);
static void e_table_text_model_set_text (ETextModel *model, const gchar *text);
static void e_table_text_model_insert (ETextModel *model, gint postion, const gchar *text);
static void e_table_text_model_insert_length (ETextModel *model, gint postion, const gchar *text, gint length);
static void e_table_text_model_delete (ETextModel *model, gint postion, gint length);

#define PARENT_TYPE E_TYPE_TEXT_MODEL
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
E_MAKE_TYPE (e_table_text_model,
	     "ETableTextModel",
	     ETableTextModel,
	     e_table_text_model_class_init,
	     e_table_text_model_init,
	     PARENT_TYPE)
	     
/* Class initialization function for the text item */
static void
e_table_text_model_class_init (ETableTextModelClass *klass)
{
	GObjectClass *object_class;
	ETextModelClass *model_class;

	object_class = (GObjectClass *) klass;
	model_class = (ETextModelClass *) klass;

	parent_class = g_type_class_ref (PARENT_TYPE);

	model_class->get_text = e_table_text_model_get_text;
	model_class->set_text = e_table_text_model_set_text;
	model_class->insert = e_table_text_model_insert;
	model_class->insert_length = e_table_text_model_insert_length;
	model_class->delete = e_table_text_model_delete;
	
	object_class->dispose = e_table_text_model_dispose;
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

/* Dispose handler for the text item */
static void
e_table_text_model_dispose (GObject *object)
{
	ETableTextModel *model;

	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_TABLE_TEXT_MODEL (object));

	model = E_TABLE_TEXT_MODEL (object);
	
	if (model->model)
		g_assert (GTK_IS_OBJECT (model->model));

	if (model->cell_changed_signal_id)
		g_signal_handler_disconnect (model->model, 
					     model->cell_changed_signal_id);
	model->cell_changed_signal_id = 0;

	if (model->row_changed_signal_id)
		g_signal_handler_disconnect (model->model, 
					     model->row_changed_signal_id);
	model->row_changed_signal_id = 0;

	if (model->model)
		g_object_unref (model->model);
	model->model = NULL;

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
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

	model = g_object_new (E_TYPE_TABLE_TEXT_MODEL, NULL);
	model->model = table_model;
	if (model->model){
		g_object_ref (model->model);
		model->cell_changed_signal_id = 
			g_signal_connect (model->model,
					  "model_cell_changed",
					  G_CALLBACK(cell_changed),
					  model);
		model->row_changed_signal_id = 
			g_signal_connect (model->model,
					  "model_row_changed",
					  G_CALLBACK(row_changed),
					  model);
	}
	model->row = row;
	model->model_col = model_col;
	return model;
}

