/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-history.c
 *
 * Copyright (C) 2002  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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

struct _EHistoryPrivate {
	EHistoryItemFreeFunc item_free_function;

	GList *items;
	GList *current_item;
};

G_DEFINE_TYPE (EHistory, e_history, GTK_TYPE_OBJECT)


/* GObject methods.  */

static void
impl_finalize (GObject *object)
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

	(* G_OBJECT_CLASS (e_history_parent_class)->finalize) (object);
}


static void
e_history_class_init (EHistoryClass *klass)
{
	GObjectClass *object_class;
	
	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = impl_finalize;
}

static void
e_history_init (EHistory *history)
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

	history = g_object_new (e_history_get_type (), NULL);
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

void
e_history_remove_matching (EHistory *history,
			   const void *data,
			   GCompareFunc compare_func)
{
	EHistoryPrivate *priv;
	GList *p;

	g_return_if_fail (history != NULL);
	g_return_if_fail (E_IS_HISTORY (history));
	g_return_if_fail (compare_func != NULL);

	priv = history->priv;

	for (p = priv->items; p != NULL; p = p->next) {
		if ((* compare_func) (data, p->data) == 0) {
			if (priv->items == priv->current_item)
				priv->items = priv->current_item = g_list_remove_link (priv->items, p);
			else
				priv->items = g_list_remove_link (priv->items, p);
		}
	}
}

