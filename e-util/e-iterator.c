/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: 
 *   Christopher James Lahey <clahey@umich.edu>
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 1999 The Free Software Foundation
 */

#include <config.h>

#include "e-iterator.h"
#include "e-util-marshal.h"

static void e_iterator_init (EIterator *card);
static void e_iterator_class_init (EIteratorClass *klass);

#define PARENT_TYPE G_TYPE_OBJECT

static GObjectClass *parent_class;

enum {
	INVALIDATE,
	LAST_SIGNAL
};

static guint e_iterator_signals [LAST_SIGNAL] = { 0, };

/**
 * e_iterator_get_type:
 * @void: 
 * 
 * Registers the &EIterator class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &EIterator class.
 **/
GType
e_iterator_get_type (void)
{
	static GType type = 0;

	if (! type) {
		GTypeInfo info = {
			sizeof (EIteratorClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc)  e_iterator_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (EIterator),
			0,    /* n_preallocs */
			(GInstanceInitFunc) e_iterator_init
		};

		type = g_type_register_static (PARENT_TYPE, "EIterator", &info, 0);
	}

	return type;
}

static void
e_iterator_class_init (EIteratorClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_ref (PARENT_TYPE);

	e_iterator_signals [INVALIDATE] =
		g_signal_new ("invalidate",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EIteratorClass, invalidate),
			      NULL, NULL,
			      e_util_marshal_NONE__NONE, /* XXX need a new marshaller here */
			      G_TYPE_NONE, 0);

	klass->invalidate = NULL;
	klass->get        = NULL;
	klass->reset      = NULL;
	klass->last       = NULL;
	klass->next       = NULL;
	klass->prev       = NULL;
	klass->remove     = NULL;
	klass->insert     = NULL;
	klass->set        = NULL;
	klass->is_valid   = NULL;
}

/**
 * e_iterator_init:
 */
static void
e_iterator_init (EIterator *card)
{
}

/*
 * Virtual functions: 
 */
const void *
e_iterator_get      (EIterator *iterator)
{
	if (E_ITERATOR_GET_CLASS(iterator)->get)
		return E_ITERATOR_GET_CLASS(iterator)->get(iterator);
	else
		return NULL;
}

void
e_iterator_reset    (EIterator *iterator)
{
	if (E_ITERATOR_GET_CLASS(iterator)->reset)
		E_ITERATOR_GET_CLASS(iterator)->reset(iterator);
}

void
e_iterator_last     (EIterator *iterator)
{
	if (E_ITERATOR_GET_CLASS(iterator)->last)
		E_ITERATOR_GET_CLASS(iterator)->last(iterator);
}

gboolean
e_iterator_next     (EIterator *iterator)
{
	if (E_ITERATOR_GET_CLASS(iterator)->next)
		return E_ITERATOR_GET_CLASS(iterator)->next(iterator);
	else
		return FALSE;
}

gboolean
e_iterator_prev     (EIterator *iterator)
{
	if (E_ITERATOR_GET_CLASS(iterator)->prev)
		return E_ITERATOR_GET_CLASS(iterator)->prev(iterator);
	else
		return FALSE;
}

void
e_iterator_delete   (EIterator *iterator)
{
	if (E_ITERATOR_GET_CLASS(iterator)->remove)
		E_ITERATOR_GET_CLASS(iterator)->remove(iterator);
}

void           e_iterator_insert     (EIterator  *iterator,
				      const void *object,
				      gboolean    before)
{
	if (E_ITERATOR_GET_CLASS(iterator)->insert)
		E_ITERATOR_GET_CLASS(iterator)->insert(iterator, object, before);
}

void
e_iterator_set      (EIterator *iterator,
			  const void    *object)
{
	if (E_ITERATOR_GET_CLASS(iterator)->set)
		E_ITERATOR_GET_CLASS(iterator)->set(iterator, object);
}

gboolean
e_iterator_is_valid (EIterator *iterator)
{
	if (E_ITERATOR_GET_CLASS(iterator)->is_valid)
		return E_ITERATOR_GET_CLASS(iterator)->is_valid(iterator);
	else
		return FALSE;
}

void
e_iterator_invalidate (EIterator *iterator)
{
	g_return_if_fail (iterator != NULL);
	g_return_if_fail (E_IS_ITERATOR (iterator));

	g_signal_emit (iterator, e_iterator_signals [INVALIDATE], 0);
}
