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


/* View initialization.  */

static const char *
get_storage_set_path_from_uri (const char *uri)
{
	const char *colon;

	if (g_path_is_absolute (uri))
		return NULL;

	colon = strchr (uri, ':');
	if (colon == NULL || colon == uri || colon[1] == '\0')
		return NULL;

	if (! g_path_is_absolute (colon + 1))
		return NULL;

	if (g_strncasecmp (uri, "evolution", colon - uri) != 0)
		return NULL;

	return colon + 1;
}

static void
load_group (EShortcutsView *shortcuts_view,
	    const char *group_title,
	    int group_num)
{
	EShortcutsViewPrivate *priv;
	EShortcuts *shortcuts;
	EStorageSet *storage_set;
	GList *shortcut_list;
	GList *p;

	priv = shortcuts_view->priv;
	shortcuts = priv->shortcuts;

	storage_set = e_shortcuts_get_storage_set (shortcuts);
	g_assert (storage_set != NULL);

	shortcut_list = e_shortcuts_get_shortcuts_in_group (shortcuts, group_title);
	if (shortcut_list == NULL)
		return;

	for (p = shortcut_list; p != NULL; p = p->next) {
		EFolder *folder;
		const char *path;
		const char *uri;
		const char *name;

		uri = (const char *) p->data;
		path = get_storage_set_path_from_uri (uri);
		if (path != NULL)
			folder = e_storage_set_get_folder (storage_set, path);

		if (path == NULL || folder == NULL) {
			/* FIXME */
			g_warning ("Invalid link while loading shortcut bar view -- %s\n",
				   uri);
			continue;
		}

		name = e_folder_get_name (folder);
		e_shortcut_bar_add_item (E_SHORTCUT_BAR (shortcuts_view), group_num, uri, name);
	}

	e_free_string_list (shortcut_list);
}

static void
load_all_shortcuts (EShortcutsView *shortcuts_view)
{
	EShortcutsViewPrivate *priv;
	EShortcuts *shortcuts;
	GList *group_titles;
	GList *p;
	int group_num;

	priv = shortcuts_view->priv;
	shortcuts = priv->shortcuts;

	group_titles = e_shortcuts_get_group_titles (shortcuts);

	for (p = group_titles; p != NULL; p = p->next) {
		const char *group_title;

		group_title = (const char *) p->data;
		group_num = e_shortcut_bar_add_group (E_SHORTCUT_BAR (shortcuts_view),
						      group_title);

		load_group (shortcuts_view, group_title, group_num);
	}

	e_free_string_list (group_titles);
}

/* Icon callback for the shortcut bar.  */
static GdkPixbuf *
icon_callback (EShortcutBar *shortcut_bar,
	       const char *uri,
	       gpointer data)
{
	EFolderTypeRegistry *folder_type_registry;
	EShortcuts *shortcuts;
	EStorageSet *storage_set;
	EFolder *folder;
	GdkPixbuf *pixbuf;
	const char *type;

	shortcuts = E_SHORTCUTS (data);

	storage_set = e_shortcuts_get_storage_set (shortcuts);
	folder_type_registry = e_storage_set_get_folder_type_registry (storage_set);

	folder = e_storage_set_get_folder (storage_set,
					   get_storage_set_path_from_uri (uri));

	if (folder == NULL)
		return NULL;

	type = e_folder_get_type_string (folder);
	if (type == NULL)
		return NULL;

	/* FIXME mini icons?  */
	pixbuf = e_folder_type_registry_get_icon_for_type (folder_type_registry, type, FALSE);
	if (pixbuf != NULL)
		gdk_pixbuf_ref (pixbuf);

	return pixbuf;
}


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

	gtk_object_ref (GTK_OBJECT (shortcuts));
	priv->shortcuts = shortcuts;

	e_shortcut_bar_set_icon_callback (E_SHORTCUT_BAR (shortcuts_view), icon_callback,
					  shortcuts);

	load_all_shortcuts (shortcuts_view);
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
