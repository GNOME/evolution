/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: 
 *   Christopher James Lahey <clahey@umich.edu>
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 1999 The Free Software Foundation
 */

#include <config.h>

#include "e-list-iterator.h"
#include "e-list.h"


static void        e_list_iterator_init       (EListIterator *list);
static void        e_list_iterator_class_init (EListIteratorClass *klass);
		   
static void        e_list_iterator_invalidate (EIterator *iterator);
static gboolean    e_list_iterator_is_valid   (EIterator *iterator);
static void        e_list_iterator_set        (EIterator  *iterator,
	           			       const void *object);
static void        e_list_iterator_remove     (EIterator  *iterator);
static void        e_list_iterator_insert     (EIterator  *iterator,
		   			       const void *object,
		   			       gboolean    before);
static gboolean    e_list_iterator_prev       (EIterator  *iterator);
static gboolean    e_list_iterator_next       (EIterator  *iterator);
static void        e_list_iterator_reset      (EIterator *iterator);
static void        e_list_iterator_last       (EIterator *iterator);
static const void *e_list_iterator_get        (EIterator *iterator);
static void        e_list_iterator_dispose    (GObject *object);

#define PARENT_TYPE E_TYPE_ITERATOR

static EIteratorClass *parent_class;

/**
 * e_list_iterator_get_type:
 * @void: 
 * 
 * Registers the &EListIterator class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &EListIterator class.
 **/
GType
e_list_iterator_get_type (void)
{
	static GType type = 0;

	if (! type) {
		GTypeInfo info = {
			sizeof (EListIteratorClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc)  e_list_iterator_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (EListIterator),
			0,    /* n_preallocs */
			(GInstanceInitFunc) e_list_iterator_init
		};

		type = g_type_register_static (PARENT_TYPE, "EListIterator", &info, 0);
	}

	return type;
}

static void
e_list_iterator_class_init (EListIteratorClass *klass)
{
	GObjectClass *object_class;
	EIteratorClass *iterator_class;

	object_class = G_OBJECT_CLASS(klass);
	iterator_class = E_ITERATOR_CLASS(klass);

	parent_class = g_type_class_ref (PARENT_TYPE);

	object_class->dispose = e_list_iterator_dispose;

	iterator_class->invalidate = e_list_iterator_invalidate;
	iterator_class->get        = e_list_iterator_get;
	iterator_class->reset      = e_list_iterator_reset;
	iterator_class->last       = e_list_iterator_last;
	iterator_class->next       = e_list_iterator_next;
	iterator_class->prev       = e_list_iterator_prev;
	iterator_class->remove     = e_list_iterator_remove;
	iterator_class->insert     = e_list_iterator_insert;
	iterator_class->set        = e_list_iterator_set;
	iterator_class->is_valid   = e_list_iterator_is_valid;
}



/**
 * e_list_iterator_init:
 */
static void
e_list_iterator_init (EListIterator *list)
{
}

EIterator *
e_list_iterator_new (EList *list)
{
	EListIterator *iterator = g_object_new (E_TYPE_LIST_ITERATOR, NULL);

	iterator->list = list;
	g_object_ref(list);
	iterator->iterator = list->list;

	return E_ITERATOR(iterator);
}

/*
 * Virtual functions: 
 */
static void
e_list_iterator_dispose (GObject *object)
{
	EListIterator *iterator = E_LIST_ITERATOR(object);
	e_list_remove_iterator(iterator->list, E_ITERATOR(iterator));
	g_object_unref(iterator->list);

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static const void *
e_list_iterator_get      (EIterator *_iterator)
{
	EListIterator *iterator = E_LIST_ITERATOR(_iterator);
	if (iterator->iterator)
		return iterator->iterator->data;
	else
		return NULL;
}

static void
e_list_iterator_reset    (EIterator *_iterator)
{
	EListIterator *iterator = E_LIST_ITERATOR(_iterator);
	iterator->iterator = iterator->list->list;
}

static void
e_list_iterator_last     (EIterator *_iterator)
{
	EListIterator *iterator = E_LIST_ITERATOR(_iterator);
	iterator->iterator = g_list_last(iterator->list->list);
}

static gboolean
e_list_iterator_next     (EIterator *_iterator)
{
	EListIterator *iterator = E_LIST_ITERATOR(_iterator);
	if (iterator->iterator)
		iterator->iterator = g_list_next(iterator->iterator);
	else
		iterator->iterator = iterator->list->list;
	return (iterator->iterator != NULL);
}

static gboolean
e_list_iterator_prev     (EIterator *_iterator)
{
	EListIterator *iterator = E_LIST_ITERATOR(_iterator);
	if (iterator->iterator)
		iterator->iterator = g_list_previous(iterator->iterator);
	else
		iterator->iterator = g_list_last(iterator->list->list);
	return (iterator->iterator != NULL);
}

static void
e_list_iterator_insert   (EIterator  *_iterator,
			  const void *object,
			  gboolean    before)
{
	EListIterator *iterator = E_LIST_ITERATOR(_iterator);
	void *data;
	if (iterator->list->copy)
		data = iterator->list->copy(object, iterator->list->closure);
	else
		data = (void *) object;
	if (iterator->iterator) {
		if (before) {
			iterator->list->list = g_list_first(g_list_prepend(iterator->iterator, data));
			iterator->iterator = iterator->iterator->prev;
		} else {
			if (iterator->iterator->next)
				g_list_prepend(iterator->iterator->next, data);
			else
				g_list_append(iterator->iterator, data);
			iterator->iterator = iterator->iterator->next;
		}
		e_list_invalidate_iterators(iterator->list, E_ITERATOR(iterator));
	} else {
		if (before) {
			iterator->list->list = g_list_append(iterator->list->list, data);
			iterator->iterator = g_list_last(iterator->list->list);
		} else {
			iterator->list->list = g_list_prepend(iterator->list->list, data);
			iterator->iterator = iterator->list->list;
		}
		e_list_invalidate_iterators(iterator->list, E_ITERATOR(iterator));
	}
}

static void
e_list_iterator_remove   (EIterator *_iterator)
{
	EListIterator *iterator = E_LIST_ITERATOR(_iterator);
	if (iterator->iterator) {
		e_list_remove_link (iterator->list, iterator->iterator);
	}
}

static void
e_list_iterator_set      (EIterator  *_iterator,
			  const void *object)
{
	EListIterator *iterator = E_LIST_ITERATOR(_iterator);
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
e_list_iterator_is_valid (EIterator *_iterator)
{
	EListIterator *iterator = E_LIST_ITERATOR(_iterator);
	return iterator->iterator != NULL;
}

static void
e_list_iterator_invalidate (EIterator *_iterator)
{
	EListIterator *iterator = E_LIST_ITERATOR(_iterator);
	iterator->iterator = NULL;
}
