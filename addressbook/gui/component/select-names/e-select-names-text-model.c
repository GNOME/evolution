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

#include "e-select-names-text-model.h"

/* Object argument IDs */
enum {
	ARG_0,
	ARG_SOURCE,
};

static void e_select_names_text_model_init       (ESelectNamesTextModel *model);
static void e_select_names_text_model_class_init (ESelectNamesTextModelClass *klass);

static void e_select_names_text_model_destroy    (GtkObject *object);
static void e_select_names_text_model_set_arg    (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_select_names_text_model_get_arg    (GtkObject *object, GtkArg *arg, guint arg_id);

static void  e_select_names_text_model_set_text      (ETextModel *model, gchar *text);
static void  e_select_names_text_model_insert        (ETextModel *model, gint position, gchar *text);
static void  e_select_names_text_model_insert_length (ETextModel *model, gint position, gchar *text, gint length);
static void  e_select_names_text_model_delete        (ETextModel *model, gint position, gint length);

static void e_select_names_text_model_model_changed (ESelectNamesModel *source,
					      ESelectNamesTextModel *model);


ETextModelClass *parent_class;
#define PARENT_TYPE e_text_model_get_type()

/**
 * e_select_names_text_model_get_type:
 * @void: 
 * 
 * Registers the &ESelectNamesTextModel class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &ESelectNamesTextModel class.
 **/
GtkType
e_select_names_text_model_get_type (void)
{
	static GtkType model_type = 0;

	if (!model_type) {
		GtkTypeInfo model_info = {
			"ESelectNamesTextModel",
			sizeof (ESelectNamesTextModel),
			sizeof (ESelectNamesTextModelClass),
			(GtkClassInitFunc) e_select_names_text_model_class_init,
			(GtkObjectInitFunc) e_select_names_text_model_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		model_type = gtk_type_unique (PARENT_TYPE, &model_info);
	}

	return model_type;
}

/**
 * e_select_names_text_model_new:
 * @VCard: a string in vCard format
 *
 * Returns: a new #ESelectNamesTextModel that wraps the @VCard.
 */
ETextModel *
e_select_names_text_model_new (ESelectNamesModel *source)
{
	ETextModel *model = E_TEXT_MODEL(gtk_type_new(e_select_names_text_model_get_type()));
	gtk_object_set(GTK_OBJECT(model),
		       "source", source,
		       NULL);
	return model;
}

static void
e_select_names_text_model_class_init (ESelectNamesTextModelClass *klass)
{
	GtkObjectClass *object_class;
	ETextModelClass *text_model_class;

	object_class = GTK_OBJECT_CLASS(klass);
	text_model_class = E_TEXT_MODEL_CLASS(klass);

	parent_class = gtk_type_class(PARENT_TYPE);

	gtk_object_add_arg_type ("ESelectNamesTextModel::source",
				 GTK_TYPE_OBJECT, GTK_ARG_READWRITE, ARG_SOURCE);

	object_class->destroy = e_select_names_text_model_destroy;
	object_class->get_arg = e_select_names_text_model_get_arg;
	object_class->set_arg = e_select_names_text_model_set_arg;

	text_model_class->set_text      = e_select_names_text_model_set_text;
	text_model_class->insert        = e_select_names_text_model_insert;
	text_model_class->insert_length = e_select_names_text_model_insert_length;
	text_model_class->delete        = e_select_names_text_model_delete;
}

static void
e_select_names_text_model_set_text    	(ETextModel *model, gchar *text)
{
	e_select_names_model_clear(model);
}

static void
e_select_names_text_model_insert      	(ETextModel *model, gint position, gchar *text)
{
}

static void
e_select_names_text_model_insert_length (ETextModel *model, gint position, gchar *text, gint length)
{
}

static void
e_select_names_text_model_delete        (ETextModel *model, gint position, gint length)
{
}

static void
e_select_names_text_model_model_changed (ESelectNamesModel     *source,
					 ESelectNamesTextModel *model)
{
	EList *list = e_select_names_model_get_data(source);
	EIterator *iterator = e_list_get_iterator(list);
	int length = 0;
	int length_count = 0;
	int *lengthsp;
	char *string;
	char *stringp;
	for (e_iterator_reset(iterator); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
		const ESelectNamesModelData *data = e_iterator_get(iterator);
		length += strlen(data->string);
		length_count++;
	}

	g_free(model->lengths);
	model->lengths = g_new(int, length_count + 1);
	lengthsp = model->lengths;

	string = g_new(char, length + 1);
	stringp = string;
	*stringp = 0;
	for (e_iterator_reset(iterator); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
		const ESelectNamesModelData *data = e_iterator_get(iterator);
		int this_length;

		strcpy(stringp, data->string);
		this_length = strlen(stringp);
		stringp += this_length;
		*(lengthsp++) = this_length;
	}
	*stringp = 0;
	*lengthsp = -1;
	g_free(E_TEXT_MODEL(model)->text);
	E_TEXT_MODEL(model)->text = string;
}


static void
e_select_names_text_model_add_source (ESelectNamesTextModel *model,
				      ESelectNamesModel     *source)
{
	model->source = source;
	if (model->source)
		gtk_object_ref(GTK_OBJECT(model->source));
	model->source_changed_id = gtk_signal_connect(GTK_OBJECT(model->source), "changed",
						      GTK_SIGNAL_FUNC(e_select_names_text_model_model_changed),
						      model);
}

static void
e_select_names_text_model_drop_source (ESelectNamesTextModel *model)
{
	if (model->source_changed_id)
		gtk_signal_disconnect(GTK_OBJECT(model->source), model->source_changed_id);
	if (model->source)
		gtk_object_unref(GTK_OBJECT(model->source));
	model->source = NULL;
	model->source_changed_id = 0;
}

/*
 * ESelectNamesTextModel lifecycle management and vcard loading/saving.
 */

static void
e_select_names_text_model_destroy (GtkObject *object)
{
	ESelectNamesTextModel *model;
	
	model = E_SELECT_NAMES_TEXT_MODEL (object);
	
	e_select_names_text_model_drop_source(model);
	g_free(model->lengths);
	
	if (GTK_OBJECT_CLASS(parent_class)->destroy)
		GTK_OBJECT_CLASS(parent_class)->destroy(object);
}


/* Set_arg handler for the model */
static void
e_select_names_text_model_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ESelectNamesTextModel *model;
	
	model = E_SELECT_NAMES_TEXT_MODEL (object);

	switch (arg_id) {
	case ARG_SOURCE:
		e_select_names_text_model_drop_source(model);
		e_select_names_text_model_add_source(model, E_SELECT_NAMES_MODEL(GTK_VALUE_OBJECT(*arg)));
		break;
	default:
		return;
	}
}

/* Get_arg handler for the model */
static void
e_select_names_text_model_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ESelectNamesTextModel *model;

	model = E_SELECT_NAMES_TEXT_MODEL (object);

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
 * e_select_names_text_model_init:
 */
static void
e_select_names_text_model_init (ESelectNamesTextModel *model)
{
	model->source = NULL;
	model->source_changed_id = 0;
	model->lengths = NULL;
}

