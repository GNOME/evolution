/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shortcuts-view.c
 *
 * Copyright (C) 2000  Helix Code, Inc.
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
 * Author: Ettore Perazzoli
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>

#include "e-util/e-util.h"

#include "e-shortcuts-view.h"


#define PARENT_TYPE E_TYPE_SHORTCUT_BAR
static EShortcutBarClass *parent_class = NULL;

struct _EShortcutsViewPrivate {
	EShortcuts *shortcuts;
};

enum {
	ACTIVATE_SHORTCUT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


static void
destroy (GtkObject *object)
{
	EShortcutsViewPrivate *priv;
	EShortcutsView *shortcuts_view;

	shortcuts_view = E_SHORTCUTS_VIEW (object);

	priv = shortcuts_view->priv;
	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
selected_item (EShortcutBar *shortcut_bar,
	       GdkEvent *event,
	       int group_num,
	       int item_num)
{
	EShortcuts *shortcuts;
	const char *uri;

	shortcuts = E_SHORTCUTS_VIEW (shortcut_bar)->priv->shortcuts;

	uri = e_shortcuts_get_uri (shortcuts, group_num, item_num);

	/* Lame EShortcutBar.  This can happen.  */
	if (uri == NULL)
		return;

	gtk_signal_emit (GTK_OBJECT (shortcut_bar), signals[ACTIVATE_SHORTCUT],
			 shortcuts, uri);
}


static void
class_init (EShortcutsViewClass *klass)
{
	GtkObjectClass *object_class;
	EShortcutBarClass *shortcut_bar_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = destroy;

	shortcut_bar_class = E_SHORTCUT_BAR_CLASS (klass);
	shortcut_bar_class->selected_item = selected_item;

	parent_class = gtk_type_class (e_shortcut_bar_get_type ());

	signals[ACTIVATE_SHORTCUT] =
		gtk_signal_new ("activate_shortcut",
				GTK_RUN_LAST | GTK_RUN_ACTION,
				object_class->type,
				GTK_SIGNAL_OFFSET (EShortcutsViewClass, activate_shortcut),
				gtk_marshal_NONE__POINTER_POINTER,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_POINTER,
				GTK_TYPE_STRING);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
init (EShortcutsView *shortcuts_view)
{
	EShortcutsViewPrivate *priv;

	priv = g_new (EShortcutsViewPrivate, 1);
	priv->shortcuts = NULL;

	shortcuts_view->priv = priv;
}


void
e_shortcuts_view_construct (EShortcutsView *shortcuts_view,
			    EShortcuts *shortcuts)
{
	EShortcutsViewPrivate *priv;

	g_return_if_fail (shortcuts != NULL);
	g_return_if_fail (E_IS_SHORTCUTS (shortcuts));

	priv = shortcuts_view->priv;
	priv->shortcuts = shortcuts;

	gtk_object_ref (GTK_OBJECT (shortcuts));
}

GtkWidget *
e_shortcuts_view_new (EShortcuts *shortcuts)
{
	GtkWidget *new;

	g_return_val_if_fail (shortcuts != NULL, NULL);
	g_return_val_if_fail (E_IS_SHORTCUTS (shortcuts), NULL);

	new = gtk_type_new (e_shortcuts_view_get_type ());
	e_shortcuts_view_construct (E_SHORTCUTS_VIEW (new), shortcuts);

	return new;
}


E_MAKE_TYPE (e_shortcuts_view, "EShortcutsView", EShortcutsView, class_init, init, PARENT_TYPE)
