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

#include "e-select-names-table-model.h"

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
}

/*
 * ESelectNamesTableModel lifecycle management and vcard loading/saving.
 */

static void
e_select_names_table_model_destroy (GtkObject *object)
{
	ESelectNamesTableModel *model;
	
	model = E_SELECT_NAMES_TABLE_MODEL (object);

	if (model->source)
		gtk_object_unref(GTK_OBJECT(model->source));
}


/* Set_arg handler for the model */
static void
e_select_names_table_model_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ESelectNamesTableModel *model;
	
	model = E_SELECT_NAMES_TABLE_MODEL (object);

	switch (arg_id) {
	case ARG_SOURCE:
		if (model->source)
			gtk_object_unref(GTK_OBJECT(model->source));
		model->source = E_SELECT_NAMES_MODEL(GTK_VALUE_OBJECT(*arg));
		if (model->source)
			gtk_object_ref(GTK_OBJECT(model->source));
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
}
