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

#include <e-card-list.h>
#include <e-card-list-iterator.h>

#define ECL_CLASS(object) (E_CARD_LIST_CLASS(GTK_OBJECT((object))->klass))

static void e_card_list_init (ECardList *card);
static void e_card_list_class_init (ECardListClass *klass);
static void e_card_list_destroy (GtkObject *object);

#define PARENT_TYPE (gtk_object_get_type ())

static GtkObjectClass *parent_class;

/**
 * e_card_list_get_type:
 * @void: 
 * 
 * Registers the &ECardList class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &ECardList class.
 **/
GtkType
e_card_list_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		GtkTypeInfo info = {
			"ECardList",
			sizeof (ECardList),
			sizeof (ECardListClass),
			(GtkClassInitFunc) e_card_list_class_init,
			(GtkObjectInitFunc) e_card_list_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (PARENT_TYPE, &info);
	}

	return type;
}

static void
e_card_list_class_init (ECardListClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS(klass);

	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = e_card_list_destroy;
}

/**
 * e_card_list_init:
 */
static void
e_card_list_init (ECardList *list)
{
	list->list = NULL;
	list->iterators = NULL;
}

ECardList *
e_card_list_new          (ECardListCopyFunc copy, ECardListFreeFunc free, void *closure)
{
	ECardList *list = gtk_type_new(e_card_list_get_type());
	list->copy    = copy;
	list->free    = free;
	list->closure = closure;
	return list;
}

ECardIterator *
e_card_list_get_iterator (ECardList *list)
{
	ECardIterator *iterator = e_card_list_iterator_new(list);
	list->iterators = g_list_append(list->iterators, iterator);
	return iterator;
}

void
e_card_list_append       (ECardList *list, const void *data)
{
	e_card_list_invalidate_iterators(list, NULL);
	if (list->copy)
		list->list = g_list_append(list->list, list->copy(data, list->closure));
	else
		list->list = g_list_append(list->list, (void *) data);
}

void
e_card_list_invalidate_iterators (ECardList *list, ECardIterator *skip)
{
	GList *iterators = list->iterators;
	for (; iterators; iterators = iterators->next) {
		if (iterators->data != skip) {
			e_card_iterator_invalidate(E_CARD_ITERATOR(iterators->data));
		}
	}
}

void
e_card_list_remove_iterator (ECardList *list, ECardIterator *iterator)
{
	list->iterators = g_list_remove(list->iterators, iterator);
}

/* 
 * Virtual functions 
 */
static void
e_card_list_destroy (GtkObject *object)
{
	ECardList *list = E_CARD_LIST(object);
	g_list_foreach(list->list, (GFunc) list->free, list->closure);
	g_list_free(list->list);
}
