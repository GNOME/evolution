/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-search.c
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>
#include <gtk/gtksignal.h>
#include "e-table-search.h"
#include "gal/util/e-util.h"

#include <string.h>

#define ETS_CLASS(e) ((ETableSearchClass *)((GtkObject *)e)->klass)

#define PARENT_TYPE gtk_object_get_type ()

#define d(x)

d(static gint depth = 0);

struct _ETableSearchPrivate {
	guint timeout_id;

	char *search_string;
	gunichar last_character;
};

static GtkObjectClass *e_table_search_parent_class;

enum {
	SEARCH_SEARCH,
	SEARCH_ACCEPT,
	LAST_SIGNAL
};

static guint e_table_search_signals [LAST_SIGNAL] = { 0, };

static gboolean
e_table_search_search (ETableSearch *e_table_search, char *string, ETableSearchFlags flags)
{
	gboolean ret_val;
	g_return_val_if_fail (e_table_search != NULL, FALSE);
	g_return_val_if_fail (E_IS_TABLE_SEARCH (e_table_search), FALSE);
	
	gtk_signal_emit (GTK_OBJECT (e_table_search),
			 e_table_search_signals [SEARCH_SEARCH], string, flags, &ret_val);

	return ret_val;
}

static void
e_table_search_accept (ETableSearch *e_table_search)
{
	g_return_if_fail (e_table_search != NULL);
	g_return_if_fail (E_IS_TABLE_SEARCH (e_table_search));
	
	gtk_signal_emit (GTK_OBJECT (e_table_search),
			 e_table_search_signals [SEARCH_ACCEPT]);
}

static gboolean
ets_accept (gpointer data)
{
	ETableSearch *ets = data;
	e_table_search_accept (ets);
	g_free (ets->priv->search_string);

	ets->priv->timeout_id = 0;
	ets->priv->search_string = g_strdup ("");
	ets->priv->last_character = 0;

	return FALSE;
}

static void
drop_timeout (ETableSearch *ets)
{
	if (ets->priv->timeout_id) {
		g_source_remove (ets->priv->timeout_id);
	}
	ets->priv->timeout_id = 0;
}

static void
add_timeout (ETableSearch *ets)
{
	drop_timeout (ets);
	ets->priv->timeout_id = g_timeout_add (1000, ets_accept, ets);
}

static void
e_table_search_destroy (GtkObject *object)
{
	ETableSearch *ets = (ETableSearch *) object;
	
	/* FIXME: do we need to unregister the timeout? bad things
           might happen if a timeout is still active. */
	g_free (ets->priv->search_string);
	g_free (ets->priv);
	
	if (e_table_search_parent_class->destroy)
		(*e_table_search_parent_class->destroy)(object);
}

static void
e_table_search_class_init (GtkObjectClass *object_class)
{
	ETableSearchClass *klass = E_TABLE_SEARCH_CLASS(object_class);
	e_table_search_parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = e_table_search_destroy;

	e_table_search_signals [SEARCH_SEARCH] =
		gtk_signal_new ("search",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ETableSearchClass, search),
				e_marshal_BOOL__STRING_ENUM,
				GTK_TYPE_BOOL, 2, GTK_TYPE_STRING, GTK_TYPE_ENUM);

	e_table_search_signals [SEARCH_ACCEPT] =
		gtk_signal_new ("accept",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ETableSearchClass, accept),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	E_OBJECT_CLASS_ADD_SIGNALS (object_class, e_table_search_signals, LAST_SIGNAL);

	klass->search = NULL;
	klass->accept = NULL;
}

static void
e_table_search_init (ETableSearch *ets)
{
	ets->priv = g_new (ETableSearchPrivate, 1);

	ets->priv->timeout_id = 0;
	ets->priv->search_string = g_strdup ("");
	ets->priv->last_character = 0;
}


guint
e_table_search_get_type (void)
{
	static guint type = 0;
	
	if (!type)
	{
		GtkTypeInfo info =
		{
			"ETableSearch",
			sizeof (ETableSearch),
			sizeof (ETableSearchClass),
			(GtkClassInitFunc) e_table_search_class_init,
			(GtkObjectInitFunc) e_table_search_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		type = gtk_type_unique (PARENT_TYPE, &info);
	}

  return type;
}

ETableSearch *
e_table_search_new (void)
{
	ETableSearch *ets = gtk_type_new (e_table_search_get_type());

	return ets;
}

/**
 * e_table_search_column_count:
 * @e_table_search: The e-table-search to operate on
 *
 * Returns: the number of columns in the table search.
 */
void
e_table_search_input_character (ETableSearch *ets, gunichar character)
{
	char character_utf8[7];
	char *temp_string;

	g_return_if_fail (ets != NULL);
	g_return_if_fail (E_IS_TABLE_SEARCH (ets));

	character_utf8 [g_unichar_to_utf8 (character, character_utf8)] = 0;

	temp_string = g_strdup_printf ("%s%s", ets->priv->search_string, character_utf8);
	if (e_table_search_search (ets, temp_string,
				   ets->priv->last_character != 0 ? E_TABLE_SEARCH_FLAGS_CHECK_CURSOR_FIRST : 0)) {
		g_free (ets->priv->search_string);
		ets->priv->search_string = temp_string;
		add_timeout (ets);
		ets->priv->last_character = character;
		return;
	} else {
		g_free (temp_string);
	}

	if (character == ets->priv->last_character) {
		if (ets->priv->search_string && e_table_search_search (ets, ets->priv->search_string, 0)) {
			add_timeout (ets);
		}
	}
}

gboolean
e_table_search_backspace (ETableSearch *ets)
{
	char *end;

	g_return_val_if_fail (ets != NULL, FALSE);
	g_return_val_if_fail (E_IS_TABLE_SEARCH (ets), FALSE);

	if (!ets->priv->search_string ||
	    !*ets->priv->search_string)
		return FALSE;

	end = ets->priv->search_string + strlen (ets->priv->search_string);
	end = g_utf8_prev_char (end);
	*end = 0;
	ets->priv->last_character = 0;
	add_timeout (ets);
	return TRUE;
}
