/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-history.c
 *
 * Copyright (C) 2002  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-history.h"

#include <gal/util/e-util.h>


#define PARENT_TYPE gtk_object_get_type ()
static GtkObjectClass *parent_class = NULL;

struct _EHistoryPrivate {
	EHistoryItemFreeFunc item_free_function;

	GList *items;
	GList *current_item;
};


/* GtkObject methods.  */

static void
impl_destroy (GtkObject *object)
{
	EHistory *history;
	EHistoryPrivate *priv;
	GList *p;

	history = E_HISTORY (object);
	priv = history->priv;

	for (p = priv->items; p != NULL; p = p->next)
		(* priv->item_free_function) (p->data);

	g_list_free (priv->items);

	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (GtkObjectClass *object_class)
{
	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = impl_destroy;
}

static void
init (EHistory *history)
{
	EHistoryPrivate *priv;

	priv = g_new (EHistoryPrivate, 1);
	priv->items        = NULL;
	priv->current_item = NULL;

	history->priv = priv;

	GTK_OBJECT_UNSET_FLAGS (history, GTK_FLOATING);
}


void
e_history_construct  (EHistory *history,
		      EHistoryItemFreeFunc item_free_function)
{
	EHistoryPrivate *priv;

	g_return_if_fail (history != NULL);
	g_return_if_fail (E_IS_HISTORY (history));

	priv = history->priv;

	priv->item_free_function = item_free_function;
}

EHistory *
e_history_new (EHistoryItemFreeFunc item_free_function)
{
	EHistory *history;

	history = gtk_type_new (e_history_get_type ());
	e_history_construct (history, item_free_function);

	return history;
}

void *
e_history_prev (EHistory *history)
{
	EHistoryPrivate *priv;

	g_return_val_if_fail (history != NULL, NULL);
	g_return_val_if_fail (E_IS_HISTORY (history), NULL);

	priv = history->priv;

	if (! e_history_has_prev (history))
		return NULL;

	priv->current_item = priv->current_item->prev;
	return e_history_get_current (history);
}

gboolean
e_history_has_prev (EHistory *history)
{
	EHistoryPrivate *priv;

	g_return_val_if_fail (history != NULL, FALSE);
	g_return_val_if_fail (E_IS_HISTORY (history), FALSE);

	priv = history->priv;

	if (priv->current_item == NULL)
		return FALSE;

	if (priv->current_item->prev == NULL)
		return FALSE;
	else
		return TRUE;
}

void *
e_history_next (EHistory *history)
{
	EHistoryPrivate *priv;

	g_return_val_if_fail (history != NULL, NULL);
	g_return_val_if_fail (E_IS_HISTORY (history), NULL);

	priv = history->priv;

	if (! e_history_has_next (history))
		return NULL;

	priv->current_item = priv->current_item->next;
	return e_history_get_current (history);
}

gboolean
e_history_has_next (EHistory *history)
{
	EHistoryPrivate *priv;

	g_return_val_if_fail (history != NULL, FALSE);
	g_return_val_if_fail (E_IS_HISTORY (history), FALSE);

	priv = history->priv;

	if (priv->current_item == NULL)
		return FALSE;

	if (priv->current_item->next == NULL)
		return FALSE;
	else
		return TRUE;
}

void *
e_history_get_current (EHistory *history)
{
	EHistoryPrivate *priv;

	g_return_val_if_fail (history != NULL, NULL);
	g_return_val_if_fail (E_IS_HISTORY (history), NULL);

	priv = history->priv;

	if (priv->current_item == NULL)
		return NULL;

	return priv->current_item->data;
}

void
e_history_add (EHistory *history,
	       void     *data)
{
	EHistoryPrivate *priv;

	g_return_if_fail (history != NULL);
	g_return_if_fail (E_IS_HISTORY (history));

	priv = history->priv;

	if (priv->current_item == NULL) {
		priv->items = g_list_prepend (priv->items, data);
		priv->current_item = priv->items;

		return;
	}

	if (priv->current_item->next != NULL) {
		GList *p;

		for (p = priv->current_item->next; p != NULL; p = p->next)
			(* priv->item_free_function) (p->data);

		priv->current_item->next->prev = NULL;
		g_list_free (priv->current_item->next);

		priv->current_item->next = NULL;
	}

	g_list_append (priv->current_item, data);
	priv->current_item = priv->current_item->next;
}


E_MAKE_TYPE (e_history, "EHistory", EHistory, class_init, init, GTK_TYPE_OBJECT)
