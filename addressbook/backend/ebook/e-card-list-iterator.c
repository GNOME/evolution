/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: 
 *   Christopher James Lahey <clahey@umich.edu>
 *
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 1999 The Free Software Foundation
 */

#include <config.h>
#include <gtk/gtk.h>

#include "e-card-list-iterator.h"
#include "e-card-list.h"

static void e_card_list_iterator_init (ECardListIterator *card);
static void e_card_list_iterator_class_init (ECardListIteratorClass *klass);

static void e_card_list_iterator_invalidate (ECardIterator *iterator);
static gboolean e_card_list_iterator_is_valid (ECardIterator *iterator);
static void e_card_list_iterator_set      (ECardIterator *iterator,
					   const void    *object);
static void e_card_list_iterator_delete   (ECardIterator *iterator);
static gboolean e_card_list_iterator_prev     (ECardIterator *iterator);
static gboolean e_card_list_iterator_next     (ECardIterator *iterator);
static void e_card_list_iterator_reset    (ECardIterator *iterator);
static const void *e_card_list_iterator_get      (ECardIterator *iterator);
static void e_card_list_iterator_destroy (GtkObject *object);

#define PARENT_TYPE (e_card_iterator_get_type ())

static GtkObjectClass *parent_class;
#define PARENT_CLASS (E_CARD_LIST_ITERATOR_CLASS(parent_class))

/**
 * e_card_list_iterator_get_type:
 * @void: 
 * 
 * Registers the &ECardListIterator class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &ECardListIterator class.
 **/
GtkType
e_card_list_iterator_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		GtkTypeInfo info = {
			"ECardListIterator",
			sizeof (ECardListIterator),
			sizeof (ECardListIteratorClass),
			(GtkClassInitFunc) e_card_list_iterator_class_init,
			(GtkObjectInitFunc) e_card_list_iterator_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (PARENT_TYPE, &info);
	}

	return type;
}

static void
e_card_list_iterator_class_init (ECardListIteratorClass *klass)
{
	GtkObjectClass *object_class;
	ECardIteratorClass *iterator_class;

	object_class = GTK_OBJECT_CLASS(klass);
	iterator_class = E_CARD_ITERATOR_CLASS(klass);

	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = e_card_list_iterator_destroy;

	iterator_class->invalidate = e_card_list_iterator_invalidate;
	iterator_class->get        = e_card_list_iterator_get;
	iterator_class->reset      = e_card_list_iterator_reset;
	iterator_class->next       = e_card_list_iterator_next;
	iterator_class->prev       = e_card_list_iterator_prev;
	iterator_class->delete     = e_card_list_iterator_delete;
	iterator_class->set        = e_card_list_iterator_set;
	iterator_class->is_valid   = e_card_list_iterator_is_valid;
}



/**
 * e_card_list_iterator_init:
 */
static void
e_card_list_iterator_init (ECardListIterator *card)
{
}

ECardIterator *
e_card_list_iterator_new (ECardList *list)
{
	ECardListIterator *iterator = gtk_type_new(e_card_list_iterator_get_type());

	iterator->list = list;
	gtk_object_ref(GTK_OBJECT(list));
	iterator->iterator = list->list;

	return E_CARD_ITERATOR(iterator);
}

/*
 * Virtual functions: 
 */
static void
e_card_list_iterator_destroy (GtkObject *object)
{
	ECardListIterator *iterator = E_CARD_LIST_ITERATOR(object);
	e_card_list_remove_iterator(iterator->list, E_CARD_ITERATOR(iterator));
	gtk_object_unref(GTK_OBJECT(iterator->list));
}

static const void *
e_card_list_iterator_get      (ECardIterator *_iterator)
{
	ECardListIterator *iterator = E_CARD_LIST_ITERATOR(_iterator);
	if (iterator->iterator)
		return iterator->iterator->data;
	else
		return NULL;
}

static void
e_card_list_iterator_reset    (ECardIterator *_iterator)
{
	ECardListIterator *iterator = E_CARD_LIST_ITERATOR(_iterator);
	iterator->iterator = iterator->list->list;
}

static gboolean
e_card_list_iterator_next     (ECardIterator *_iterator)
{
	ECardListIterator *iterator = E_CARD_LIST_ITERATOR(_iterator);
	if (iterator->iterator)
		iterator->iterator = g_list_next(iterator->iterator);
	return (iterator->iterator != NULL);
}

static gboolean
e_card_list_iterator_prev     (ECardIterator *_iterator)
{
	ECardListIterator *iterator = E_CARD_LIST_ITERATOR(_iterator);
	if (iterator->iterator)
		iterator->iterator = g_list_previous(iterator->iterator);
	return (iterator->iterator != NULL);
}

static void
e_card_list_iterator_delete   (ECardIterator *_iterator)
{
	ECardListIterator *iterator = E_CARD_LIST_ITERATOR(_iterator);
	if (iterator->iterator) {
		GList *temp = iterator->iterator->next;
		if (iterator->list->free)
			iterator->list->free(iterator->iterator->data, iterator->list->closure);
		iterator->list->list = g_list_remove_link(iterator->list->list, iterator->iterator);
		iterator->iterator = temp;
		e_card_list_invalidate_iterators(iterator->list, E_CARD_ITERATOR(iterator));
	}
}

static void
e_card_list_iterator_set      (ECardIterator *_iterator,
			       const void    *object)
{
	ECardListIterator *iterator = E_CARD_LIST_ITERATOR(_iterator);
	if (iterator->iterator) {
		if (iterator->list->free)
			iterator->list->free(iterator->iterator->data, iterator->list->closure);
		if (iterator->list->copy)
			iterator->iterator->data = iterator->list->copy(object, iterator->list->closure);
		else
			iterator->iterator->data = (void *) object;
	}
}

static gboolean
e_card_list_iterator_is_valid (ECardIterator *_iterator)
{
	ECardListIterator *iterator = E_CARD_LIST_ITERATOR(_iterator);
	return iterator->iterator != NULL;
}

static void
e_card_list_iterator_invalidate (ECardIterator *_iterator)
{
	ECardListIterator *iterator = E_CARD_LIST_ITERATOR(_iterator);
	iterator->iterator = NULL;
}
