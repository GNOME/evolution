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

#include "e-select-names-model.h"

enum {
	E_SELECT_NAMES_MODEL_CHANGED,
	E_SELECT_NAMES_MODEL_LAST_SIGNAL
};

static guint e_select_names_model_signals[E_SELECT_NAMES_MODEL_LAST_SIGNAL] = { 0 };

/* Object argument IDs */
enum {
	ARG_0,
	ARG_CARD,
};

static void e_select_names_model_init (ESelectNamesModel *model);
static void e_select_names_model_class_init (ESelectNamesModelClass *klass);

static void e_select_names_model_destroy (GtkObject *object);
static void e_select_names_model_set_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_select_names_model_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);

/**
 * e_select_names_model_get_type:
 * @void: 
 * 
 * Registers the &ESelectNamesModel class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &ESelectNamesModel class.
 **/
GtkType
e_select_names_model_get_type (void)
{
	static GtkType model_type = 0;

	if (!model_type) {
		GtkTypeInfo model_info = {
			"ESelectNamesModel",
			sizeof (ESelectNamesModel),
			sizeof (ESelectNamesModelClass),
			(GtkClassInitFunc) e_select_names_model_class_init,
			(GtkObjectInitFunc) e_select_names_model_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		model_type = gtk_type_unique (gtk_object_get_type (), &model_info);
	}

	return model_type;
}

/**
 * e_select_names_model_new:
 * @VCard: a string in vCard format
 *
 * Returns: a new #ESelectNamesModel that wraps the @VCard.
 */
ESelectNamesModel *
e_select_names_model_new (void)
{
	ESelectNamesModel *model = E_SELECT_NAMES_MODEL(gtk_type_new(e_select_names_model_get_type()));
	return model;
}

static void
e_select_names_model_class_init (ESelectNamesModelClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS(klass);

	e_select_names_model_signals[E_SELECT_NAMES_MODEL_CHANGED] =
		gtk_signal_new ("changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ESelectNamesModelClass, changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, e_select_names_model_signals, E_SELECT_NAMES_MODEL_LAST_SIGNAL);

	gtk_object_add_arg_type ("ESelectNamesModel::card",
				 GTK_TYPE_OBJECT, GTK_ARG_READWRITE, ARG_CARD);

	klass->changed = NULL;

	object_class->destroy = e_select_names_model_destroy;
	object_class->get_arg = e_select_names_model_get_arg;
	object_class->set_arg = e_select_names_model_set_arg;
}

/*
 * ESelectNamesModel lifecycle management and vcard loading/saving.
 */

static void
e_select_names_model_destroy (GtkObject *object)
{
	ESelectNamesModel *model;
	
	model = E_SELECT_NAMES_MODEL (object);

	gtk_object_unref(GTK_OBJECT(model->data));
}


/* Set_arg handler for the model */
static void
e_select_names_model_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ESelectNamesModel *model;
	
	model = E_SELECT_NAMES_MODEL (object);

	switch (arg_id) {
	case ARG_CARD:
		break;
	default:
		return;
	}
}

/* Get_arg handler for the model */
static void
e_select_names_model_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ESelectNamesModel *model;

	model = E_SELECT_NAMES_MODEL (object);

	switch (arg_id) {
	case ARG_CARD:
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void *
data_copy(const void *sec, void *data)
{
	const ESelectNamesModelData *section = sec;
	ESelectNamesModelData *newsec;
	
	newsec = g_new(ESelectNamesModelData, 1);
	newsec->type = section->type;
	newsec->card = section->card;
	if (newsec->card)
		gtk_object_ref(GTK_OBJECT(newsec->card));
	newsec->string = g_strdup(section->string);
	return newsec;
}

static void
data_free(void *sec, void *data)
{
	ESelectNamesModelData *section = sec;
	if (section->card)
		gtk_object_unref(GTK_OBJECT(section->card));
	g_free(section->string);
	g_free(section);
}

/**
 * e_select_names_model_init:
 */
static void
e_select_names_model_init (ESelectNamesModel *model)
{
	model->data = e_list_new(data_copy, data_free, model);
}

/* Of type ECard */
EList                    *e_select_names_model_get_cards                 (ESelectNamesModel *model)
{
	return NULL;
}

EList                    *e_select_names_model_get_data                 (ESelectNamesModel *model)
{
	return model->data;
}
