/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: 
 *   Christopher James Lahey <clahey@umich.edu>
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 1999 The Free Software Foundation
 */

#include <config.h>

#include "e-list.h"
#include "e-list-iterator.h"

static void e_list_init (EList *list);
static void e_list_class_init (EListClass *klass);
static void e_list_dispose (GObject *object);

static GObjectClass *parent_class;

/**
 * e_list_get_type:
 * @void: 
 * 
 * Registers the &EList class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &EList class.
 **/
GType
e_list_get_type (void)
{
	static GType type = 0;

	if (! type) {
		GTypeInfo info = {
			sizeof (EListClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc)  e_list_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (EList),
			0,    /* n_preallocs */
			(GInstanceInitFunc) e_list_init
		};

		type = g_type_register_static (G_TYPE_OBJECT, "EList", &info, 0);
	}

	return type;
}

static void
e_list_class_init (EListClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_ref (G_TYPE_OBJECT);

	object_class->dispose = e_list_dispose;
}

/**
 * e_list_init:
 */
static void
e_list_init (EList *list)
{
	list->list = NULL;
	list->iterators = NULL;
}

EList *
e_list_new (EListCopyFunc copy, EListFreeFunc free, void *closure)
{
	EList *list = g_object_new (E_TYPE_LIST, NULL);
	e_list_construct (list, copy, free, closure);
	return list;
}

void
e_list_construct (EList *list, EListCopyFunc copy, EListFreeFunc free, void *closure)
{
	list->copy    = copy;
	list->free    = free;
	list->closure = closure;
}

EList *
e_list_duplicate (EList *old)
{
	EList *list = g_object_new (E_TYPE_LIST, NULL);

	list->copy    = old->copy;
	list->free    = old->free;
	list->closure = old->closure;
	list->list    = g_list_copy(old->list);
	if (list->copy) {
		GList *listlist;
		for (listlist = list->list; listlist; listlist = listlist->next) {
			listlist->data = list->copy (listlist->data, list->closure);
		}
	}
	return list;
}

EIterator *
e_list_get_iterator (EList *list)
{
	EIterator *iterator = e_list_iterator_new(list);
	list->iterators = g_list_append(list->iterators, iterator);
	return iterator;
}

int
e_list_length (EList *list)
{
	return g_list_length(list->list);
}

void
e_list_append (EList *list, const void *data)
{
	e_list_invalidate_iterators(list, NULL);
	if (list->copy)
		list->list = g_list_append(list->list, list->copy(data, list->closure));
	else
		list->list = g_list_append(list->list, (void *) data);
}

void
e_list_remove (EList *list, const void *data)
{
	GList *link;
	link = g_list_find (list->list, data);
	if (link)
		e_list_remove_link(list, link);
}

void
e_list_invalidate_iterators (EList *list, EIterator *skip)
{
	GList *iterators = list->iterators;
	for (; iterators; iterators = iterators->next) {
		if (iterators->data != skip) {
			e_iterator_invalidate(E_ITERATOR(iterators->data));
		}
	}
}

/* FIXME: This doesn't work properly if the iterator is the first
   iterator in the list.  Well, the iterator doesn't continue on after
   the next time next is called, at least. */
void
e_list_remove_link (EList *list, GList *link)
{
	GList *iterators = list->iterators;
	for (; iterators; iterators = iterators->next) {
		if (((EListIterator *)iterators->data)->iterator == link) {
			e_iterator_prev(iterators->data);
		}
	}
	if (list->free)
		list->free(link->data, list->closure);
	list->list = g_list_remove_link(list->list, link);
	g_list_free_1(link);
}

void
e_list_remove_iterator (EList *list, EIterator *iterator)
{
	list->iterators = g_list_remove(list->iterators, iterator);
}

/* 
 * Virtual functions 
 */
static void
e_list_dispose (GObject *object)
{
	EList *list = E_LIST(object);
	if (list->free)
		g_list_foreach(list->list, (GFunc) list->free, list->closure);
	g_list_free(list->list);

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

