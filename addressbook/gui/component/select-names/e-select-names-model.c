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
#include "e-util/e-util.h"
#include "addressbook/backend/ebook/e-card-simple.h"

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

/**
 * e_select_names_model_new:
 * @VCard: a string in vCard format
 *
 * Returns: a new #ESelectNamesModel that wraps the @VCard.
 */
ESelectNamesModel *
e_select_names_model_duplicate (ESelectNamesModel *old)
{
	ESelectNamesModel *model = E_SELECT_NAMES_MODEL(gtk_type_new(e_select_names_model_get_type()));
	model->data = e_list_duplicate(old->data);
	model->id = g_strdup(old->id);
	model->title = g_strdup(old->title);
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

static void *
copy_func(const void *data, void *user_data)
{
	GtkObject *object = (void *) data;
	if (object)
		gtk_object_ref(object);
	return object;
}

static void
free_func(void *data, void *user_data)
{
	GtkObject *object = data;
	if (object)
		gtk_object_unref(object);
}

/* Of type ECard */
EList                    *e_select_names_model_get_cards                 (ESelectNamesModel *model)
{
	EList *list = e_list_new(copy_func, free_func, NULL);
	EIterator *iterator = e_list_get_iterator(model->data);
	EIterator *new_iterator = e_list_get_iterator(list);

	for (e_iterator_reset(iterator); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
		ESelectNamesModelData *node = (void *) e_iterator_get(iterator);
		ECard *card;
		ECardSimple *simple;
		if (node->card) {
			card = node->card;
			gtk_object_ref(GTK_OBJECT(card));
		} else {
			card = e_card_new("");
		}
		simple = e_card_simple_new(card);
		e_card_simple_set_arbitrary(simple, "text_version", "string", node->string);
		e_iterator_insert(new_iterator, card, FALSE);
		gtk_object_unref(GTK_OBJECT(card));
		gtk_object_unref(GTK_OBJECT(simple));
	}
	return list;
}

EList                    *e_select_names_model_get_data                 (ESelectNamesModel *model)
{
	return model->data;
}

static void
e_select_names_model_changed           (ESelectNamesModel *model)
{
	gtk_signal_emit(GTK_OBJECT(model),
			e_select_names_model_signals[E_SELECT_NAMES_MODEL_CHANGED]);
}

void
e_select_names_model_insert            (ESelectNamesModel *model,
					EIterator *iterator, /* Must be one of the iterators in the model, or NULL if the list is empty. */
					int index,
					char *data)
{
	gchar **strings = e_strsplit(data, ",", -1);
	int i;
	if (iterator == NULL) {
		ESelectNamesModelData new = {E_SELECT_NAMES_MODEL_DATA_TYPE_STRING_ADDRESS, NULL, ""};

		e_list_append(model->data, &new);
		iterator = e_list_get_iterator(model->data);

		index = 0;
	} else {
		gtk_object_ref(GTK_OBJECT(iterator));
	}
	if (strings[0]) {
		ESelectNamesModelData *node = (void *) e_iterator_get(iterator);
		gchar *temp = g_strdup_printf("%.*s%s%s", index, node->string, strings[0], node->string + index);
		g_free(node->string);
		node->string = temp;
	}
	for (i = 1; strings[i]; i++) {
		ESelectNamesModelData *node = (void *) e_iterator_get(iterator);
		gchar *temp = g_strdup_printf("%.*s", index, node->string);
		gchar *temp2 = g_strdup_printf("%s%s", strings[0], node->string + index);

		g_free(node->string);
		node->type = E_SELECT_NAMES_MODEL_DATA_TYPE_STRING_ADDRESS;
		node->string = temp;
		if (node->card)
			gtk_object_unref(GTK_OBJECT(node->card));
		node->card = NULL;

		node = g_new(ESelectNamesModelData, 1);
		node->type = E_SELECT_NAMES_MODEL_DATA_TYPE_STRING_ADDRESS;
		node->card = NULL;
		node->string = temp2;
		e_iterator_insert(iterator, node, 0);
		g_free(node->string);
		g_free(node);
	}
	e_select_names_model_changed(model);
	gtk_object_unref(GTK_OBJECT(iterator));
}

void
e_select_names_model_insert_length     (ESelectNamesModel *model,
					EIterator *iterator, /* Must be one of the iterators in the model. */
					int index,
					char *data,
					int length)
{
	gchar *string = g_new(char, length + 1);
	strncpy(string, data, length);
	string[length] = 0;
	e_select_names_model_insert(model, iterator, index, string);
	g_free(string);
}

void
e_select_names_model_delete            (ESelectNamesModel *model,
					EIterator *iterator, /* Must be one of the iterators in the model. */
					int index,
					int length)
{
	while (length > 0 && e_iterator_is_valid(iterator)) {
		ESelectNamesModelData *node = (void *) e_iterator_get(iterator);
		int this_length = strlen(node->string);
		if (this_length <= index + length) {
			gchar *temp = g_strdup_printf("%.*s", index, node->string);
			g_free(node->string);
			node->string = temp;
			length -= this_length - index;
		} else {
			gchar *temp = g_strdup_printf("%.*s%s", index, node->string, node->string + index + length);
			g_free(node->string);
			node->string = temp;
			break;
		}
		
		if (length > 0) {
			e_iterator_next(iterator);
			if (e_iterator_is_valid(iterator)) {
				ESelectNamesModelData *node2 = (void *) e_iterator_get(iterator);
				gchar *temp = g_strdup_printf("%s%s", node->string, node2->string);
				g_free(node2->string);
				node2->string = temp;
				e_iterator_prev(iterator);
				e_iterator_delete(iterator);
				length --;
			}
		}
	}
	e_select_names_model_changed(model);
}

void
e_select_names_model_replace           (ESelectNamesModel *model,
					EIterator *iterator, /* Must be one of the iterators in the model. */
					int index,
					int length,
					char *data)
{
	if (iterator == NULL) {
		ESelectNamesModelData new = {E_SELECT_NAMES_MODEL_DATA_TYPE_STRING_ADDRESS, NULL, ""};

		e_list_append(model->data, &new);
		iterator = e_list_get_iterator(model->data);

		index = 0;
	} else {
		gtk_object_ref(GTK_OBJECT(iterator));
	}
	while (length > 0 && e_iterator_is_valid(iterator)) {
		ESelectNamesModelData *node = (void *) e_iterator_get(iterator);
		int this_length = strlen(node->string);
		if (this_length <= index + length) {
			gchar *temp = g_strdup_printf("%.*s", index, node->string);
			g_free(node->string);
			node->string = temp;
			length -= this_length - index;
		} else {
			gchar *temp = g_strdup_printf("%.*s%s", index, node->string, node->string + index + length);
			g_free(node->string);
			node->string = temp;
			length = 0;
		}

		if (length > 0) {
			e_iterator_next(iterator);
			if (e_iterator_is_valid(iterator)) {
				ESelectNamesModelData *node2 = (void *) e_iterator_get(iterator);
				gchar *temp = g_strdup_printf("%s%s", node->string, node2->string);
				g_free(node2->string);
				node2->string = temp;
				e_iterator_prev(iterator);
				e_iterator_delete(iterator);
			}
		}
	}
	if (!e_iterator_is_valid(iterator)) {
		ESelectNamesModelData *node;
		e_iterator_last(iterator);
		if (e_iterator_is_valid(iterator)) {
			node = (void *) e_iterator_get(iterator);
			index = strlen(node->string);
		} else
			index = 0;
	}
	e_select_names_model_insert (model, iterator, index, data);
	gtk_object_unref(GTK_OBJECT(iterator));
}


void
e_select_names_model_add_item          (ESelectNamesModel *model,
					EIterator *iterator, /* NULL for at the beginning. */
					ESelectNamesModelData *data)
{
	e_iterator_insert(iterator, data, FALSE);
	e_select_names_model_changed(model);
}

void
e_select_names_model_remove_item       (ESelectNamesModel *model,
					EIterator *iterator)
{
	e_iterator_delete(iterator);
	e_select_names_model_changed(model);
}


