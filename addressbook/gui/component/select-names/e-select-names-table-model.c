/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: 
 *   Chris Lahey     <clahey@helixcode.com>
 *
 * Copyright (C) 2000 Helix Code, Inc.
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include "e-util/e-util.h"
#include "e-select-names-table-model.h"
#include "addressbook/backend/ebook/e-card-simple.h"

/* Object argument IDs */
enum {
	ARG_0,
	ARG_SOURCE,
};

static void e_select_names_table_model_init       (ESelectNamesTableModel *model);
static void e_select_names_table_model_class_init (ESelectNamesTableModelClass *klass);

static void e_select_names_table_model_destroy    (GtkObject *object);
static void e_select_names_table_model_set_arg    (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_select_names_table_model_get_arg    (GtkObject *object, GtkArg *arg, guint arg_id);

static void e_select_names_table_model_model_changed (ESelectNamesModel *source,
						      ESelectNamesTableModel *model);


static void
e_select_names_table_model_add_source (ESelectNamesTableModel *model,
				       ESelectNamesModel     *source)
{
	model->source = source;
	if (model->source)
		gtk_object_ref(GTK_OBJECT(model->source));
	model->source_changed_id = gtk_signal_connect(GTK_OBJECT(model->source), "changed",
						      GTK_SIGNAL_FUNC(e_select_names_table_model_model_changed),
						      model);
}

static void
e_select_names_table_model_drop_source (ESelectNamesTableModel *model)
{
	if (model->source_changed_id)
		gtk_signal_disconnect(GTK_OBJECT(model->source), model->source_changed_id);
	if (model->source)
		gtk_object_unref(GTK_OBJECT(model->source));
	model->source = NULL;
	model->source_changed_id = 0;
}

/**
 * e_select_names_table_model_get_type:
 * @void: 
 * 
 * Registers the &ESelectNamesTableModel class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &ESelectNamesTableModel class.
 **/
GtkType
e_select_names_table_model_get_type (void)
{
	static GtkType model_type = 0;

	if (!model_type) {
		GtkTypeInfo model_info = {
			"ESelectNamesTableModel",
			sizeof (ESelectNamesTableModel),
			sizeof (ESelectNamesTableModelClass),
			(GtkClassInitFunc) e_select_names_table_model_class_init,
			(GtkObjectInitFunc) e_select_names_table_model_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		model_type = gtk_type_unique (e_table_model_get_type (), &model_info);
	}

	return model_type;
}

/**
 * e_select_names_table_model_new:
 * @VCard: a string in vCard format
 *
 * Returns: a new #ESelectNamesTableModel that wraps the @VCard.
 */
ETableModel *
e_select_names_table_model_new (ESelectNamesModel *source)
{
	ETableModel *model = E_TABLE_MODEL(gtk_type_new(e_select_names_table_model_get_type()));
	gtk_object_set(GTK_OBJECT(model),
		       "source", source,
		       NULL);
	return model;
}

static void
fill_in_info (ESelectNamesTableModel *model)
{
	if (model->source) {
		EList *list = e_select_names_model_get_data(model->source);
		EIterator *iterator = e_list_get_iterator(list);
		int count = 0;
		for (e_iterator_reset(iterator); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
			count ++;
		}
		model->count = count;
		model->data = g_new(ESelectNamesTableModelData, count);
		count = 0;
		for (e_iterator_reset(iterator); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
			const ESelectNamesModelData *data = e_iterator_get(iterator);
			switch (data->type) {
			case E_SELECT_NAMES_MODEL_DATA_TYPE_CARD: {
				ECardSimple *simple = e_card_simple_new(data->card);
				model->data[count].name =  e_card_simple_get(simple, E_CARD_SIMPLE_FIELD_FULL_NAME);
				if ((model->data[count].name == 0) || *model->data[count].name == 0) {
					model->data[count].name =  e_card_simple_get(simple, E_CARD_SIMPLE_FIELD_ORG);
				}
				if (model->data[count].name == 0)
					model->data[count].name = g_strdup("");
				model->data[count].email = e_card_simple_get(simple, E_CARD_SIMPLE_FIELD_EMAIL);
				if (model->data[count].email == 0)
					model->data[count].email = g_strdup("");
				gtk_object_unref(GTK_OBJECT(simple));
				count ++;
				break;
			}
			case E_SELECT_NAMES_MODEL_DATA_TYPE_STRING_ADDRESS:
				model->data[count].name =  e_strdup_strip(data->string);
				model->data[count].email = e_strdup_strip(data->string);
				count ++;
				break;
			}
		}
	} else {
		model->count = 0;
	}
}

static void
clear_info (ESelectNamesTableModel *model)
{
	int i;
	for (i = 0; i < model->count; i++) {
		g_free(model->data[i].name);
		g_free(model->data[i].email);
	}
	g_free(model->data);
	model->data = NULL;
	model->count = -1;
}

/*
 * ESelectNamesTableModel lifecycle management and vcard loading/saving.
 */

static void
e_select_names_table_model_destroy (GtkObject *object)
{
	ESelectNamesTableModel *model;
	
	model = E_SELECT_NAMES_TABLE_MODEL (object);

	e_select_names_table_model_drop_source (model);
	clear_info(model);
}

/* This function returns the number of columns in our ETableModel. */
static int
e_select_names_table_model_col_count (ETableModel *etc)
{
	return 2;
}

/* This function returns the number of rows in our ETableModel. */
static int
e_select_names_table_model_row_count (ETableModel *etc)
{
	ESelectNamesTableModel *e_select_names_table_model = E_SELECT_NAMES_TABLE_MODEL(etc);
	if (e_select_names_table_model->count == -1) {
		if (e_select_names_table_model->source) {
			fill_in_info(e_select_names_table_model);
		} else {
			return 0;
		}
	}
	return e_select_names_table_model->count;
}

/* This function returns the value at a particular point in our ETableModel. */
static void *
e_select_names_table_model_value_at (ETableModel *etc, int col, int row)
{
	ESelectNamesTableModel *e_select_names_table_model = E_SELECT_NAMES_TABLE_MODEL(etc);
	if (e_select_names_table_model->data == NULL) {
		fill_in_info(e_select_names_table_model);
	}
	switch (col) {
	case 0:
		if (e_select_names_table_model->data[row].name == NULL) {
			fill_in_info(e_select_names_table_model);
		}
		return e_select_names_table_model->data[row].name;
		break;
	case 1:
		if (e_select_names_table_model->data[row].email == NULL) {
			fill_in_info(e_select_names_table_model);
		}
		return e_select_names_table_model->data[row].email;
		break;
	}
	return "";
}

/* This function sets the value at a particular point in our ETableModel. */
static void
e_select_names_table_model_set_value_at (ETableModel *etc, int col, int row, const void *val)
{
}

/* This function returns whether a particular cell is editable. */
static gboolean
e_select_names_table_model_is_cell_editable (ETableModel *etc, int col, int row)
{
	return FALSE;
}

/* This function duplicates the value passed to it. */
static void *
e_select_names_table_model_duplicate_value (ETableModel *etc, int col, const void *value)
{
	return g_strdup(value);
}

/* This function frees the value passed to it. */
static void
e_select_names_table_model_free_value (ETableModel *etc, int col, void *value)
{
	g_free(value);
}

static void *
e_select_names_table_model_initialize_value (ETableModel *etc, int col)
{
	return g_strdup("");
}

static gboolean
e_select_names_table_model_value_is_empty (ETableModel *etc, int col, const void *value)
{
	return !(value && *(char *)value);
}

static char *
e_select_names_table_model_value_to_string (ETableModel *etc, int col, const void *value)
{
	return g_strdup(value);
}

static void
e_select_names_table_model_model_changed (ESelectNamesModel     *source,
					  ESelectNamesTableModel *model)
{
	clear_info(model);
	e_table_model_changed(E_TABLE_MODEL(model));
}

/* Set_arg handler for the model */
static void
e_select_names_table_model_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ESelectNamesTableModel *model;
	
	model = E_SELECT_NAMES_TABLE_MODEL (object);

	switch (arg_id) {
	case ARG_SOURCE:
		e_select_names_table_model_drop_source (model);
		e_select_names_table_model_add_source (model, E_SELECT_NAMES_MODEL(GTK_VALUE_OBJECT (*arg)));
		break;
	default:
		return;
	}
}

/* Get_arg handler for the model */
static void
e_select_names_table_model_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ESelectNamesTableModel *model;

	model = E_SELECT_NAMES_TABLE_MODEL (object);

	switch (arg_id) {
	case ARG_SOURCE:
		GTK_VALUE_OBJECT(*arg) = GTK_OBJECT(model->source);
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

/**
 * e_select_names_table_model_init:
 */
static void
e_select_names_table_model_init (ESelectNamesTableModel *model)
{
	model->source = NULL;
	model->source_changed_id = 0;

	model->count = -1;
	model->data = NULL;
}

static void
e_select_names_table_model_class_init (ESelectNamesTableModelClass *klass)
{
	GtkObjectClass *object_class;
	ETableModelClass *table_model_class;

	object_class = GTK_OBJECT_CLASS(klass);
	table_model_class = E_TABLE_MODEL_CLASS(klass);

	gtk_object_add_arg_type ("ESelectNamesTableModel::source",
				 GTK_TYPE_OBJECT, GTK_ARG_READWRITE, ARG_SOURCE);

	object_class->destroy = e_select_names_table_model_destroy;
	object_class->get_arg = e_select_names_table_model_get_arg;
	object_class->set_arg = e_select_names_table_model_set_arg;

	table_model_class->column_count = e_select_names_table_model_col_count;
	table_model_class->row_count = e_select_names_table_model_row_count;
	table_model_class->value_at = e_select_names_table_model_value_at;
	table_model_class->set_value_at = e_select_names_table_model_set_value_at;
	table_model_class->is_cell_editable = e_select_names_table_model_is_cell_editable;
	table_model_class->duplicate_value = e_select_names_table_model_duplicate_value;
	table_model_class->free_value = e_select_names_table_model_free_value;
	table_model_class->initialize_value = e_select_names_table_model_initialize_value;
	table_model_class->value_is_empty = e_select_names_table_model_value_is_empty;
	table_model_class->value_to_string = e_select_names_table_model_value_to_string;
}
