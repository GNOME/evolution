/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shortcuts-view-model.c
 *
 * Copyright (C) 2000  Ximian, Inc.
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
 * Author: Ettore Perazzoli
 */

/* FIXME.  This really sucks.  We are using the model/view approach in the
   dumbest possible way.  */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-shortcuts-view-model.h"

#include "e-icon-factory.h"

#include <glib.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>

#include <gal/util/e-util.h>


#define PARENT_TYPE e_shortcut_model_get_type ()
static EShortcutModelClass *parent_class = NULL;

struct _EShortcutsViewModelPrivate {
	EShortcuts *shortcuts;
};


/* Utility functions.  */

static GdkPixbuf *
get_icon_for_item (EShortcutsViewModel *shortcuts_view_model,
		   const EShortcutItem *item,
		   gboolean want_mini)
{
	EShortcutsViewModelPrivate *priv;

	priv = shortcuts_view_model->priv;

	if (item->custom_icon_name != NULL)
		return e_icon_factory_get_icon (item->custom_icon_name, want_mini);

	if (item->type != NULL) {
		EStorageSet *storage_set;
		EFolderTypeRegistry *folder_type_registry;

		storage_set = e_shortcuts_get_storage_set (priv->shortcuts);
		folder_type_registry = e_storage_set_get_folder_type_registry (storage_set);

		return e_folder_type_registry_get_icon_for_type (folder_type_registry,
								 item->type,
								 want_mini);
	}

	return NULL;
}


/* View initialization.  */

static char *
get_name_with_unread (const EShortcutItem *item)
{
	if (item->unread_count > 0)
		return g_strdup_printf ("%s (%d)", item->name, item->unread_count);
	else
		return g_strdup (item->name);
}

static void
load_group_into_model (EShortcutsViewModel *shortcuts_view_model,
		       int group_num)
{
	EShortcutsViewModelPrivate *priv;
	const GSList *shortcut_list;
	const GSList *p;

	priv = shortcuts_view_model->priv;

	shortcut_list = e_shortcuts_get_shortcuts_in_group (priv->shortcuts, group_num);
	if (shortcut_list == NULL)
		return;

	for (p = shortcut_list; p != NULL; p = p->next) {
		const EShortcutItem *item;
		char *name_with_unread;

		item = (const EShortcutItem *) p->data;
		name_with_unread = get_name_with_unread (item);

		e_shortcut_model_add_item (E_SHORTCUT_MODEL (shortcuts_view_model),
					   group_num, -1,
					   item->uri,
					   name_with_unread,
					   get_icon_for_item (shortcuts_view_model, item, FALSE));

		g_free (name_with_unread);
	}
}

static void
load_all_shortcuts_into_model (EShortcutsViewModel *shortcuts_view_model)
{
	EShortcutsViewModelPrivate *priv;
	const GSList *group_titles;
	const GSList *p;
	int group_num;

	priv = shortcuts_view_model->priv;

	group_titles = e_shortcuts_get_group_titles (priv->shortcuts);

	for (p = group_titles; p != NULL; p = p->next) {
		const char *group_title;

		group_title = (const char *) p->data;
		group_num = e_shortcut_model_add_group (E_SHORTCUT_MODEL (shortcuts_view_model), -1, group_title);

		load_group_into_model (shortcuts_view_model, group_num);
	}
}


/* EShortcuts callbacks.  */

static void
shortcuts_new_group_cb (EShortcuts *shortcuts,
			int group_num,
			void *data)
{
	EShortcutsViewModel *shortcuts_view_model;
	EShortcutsViewModelPrivate *priv;
	const char *title;

	shortcuts_view_model = E_SHORTCUTS_VIEW_MODEL (data);
	priv = shortcuts_view_model->priv;

	title = e_shortcuts_get_group_title (priv->shortcuts, group_num);
	e_shortcut_model_add_group (E_SHORTCUT_MODEL (shortcuts_view_model), group_num, title);
}

static void
shortcuts_remove_group_cb (EShortcuts *shortcuts,
			   int group_num,
			   void *data)
{
	EShortcutsViewModel *shortcuts_view_model;

	shortcuts_view_model = E_SHORTCUTS_VIEW_MODEL (data);
	e_shortcut_model_remove_group (E_SHORTCUT_MODEL (shortcuts_view_model), group_num);
}

static void
shortcuts_rename_group_cb (EShortcuts *shortcuts,
			   int group_num,
			   const char *new_title,
			   void *data)
{
	EShortcutsViewModel *shortcuts_view_model;

	shortcuts_view_model = E_SHORTCUTS_VIEW_MODEL (data);

	/* FIXME: Ideally there should be an
           e_shortcut_model_rename_group(), removing then re-add
           actually causes a flip to the next group, which we work
           around in e-shortcuts-view.c */
	e_shortcut_model_remove_group (E_SHORTCUT_MODEL (shortcuts_view_model), group_num);
	e_shortcut_model_add_group (E_SHORTCUT_MODEL (shortcuts_view_model), group_num, new_title);
	load_group_into_model (shortcuts_view_model, group_num);
}

static void
shortcuts_new_shortcut_cb (EShortcuts *shortcuts,
			   int group_num,
			   int item_num,
			   void *data)
{
	EShortcutsViewModel *shortcuts_view_model;
	EShortcutsViewModelPrivate *priv;
	const EShortcutItem *shortcut_item;
	char *name_with_unread;

	shortcuts_view_model = E_SHORTCUTS_VIEW_MODEL (data);
	priv = shortcuts_view_model->priv;

	shortcut_item = e_shortcuts_get_shortcut (priv->shortcuts, group_num, item_num);
	g_assert (shortcut_item != NULL);

	name_with_unread = get_name_with_unread (shortcut_item);
	e_shortcut_model_add_item (E_SHORTCUT_MODEL (shortcuts_view_model),
				   group_num, item_num,
				   shortcut_item->uri,
				   name_with_unread,
				   get_icon_for_item (shortcuts_view_model, shortcut_item, FALSE));

	g_free (name_with_unread);
}

static void
shortcuts_remove_shortcut_cb (EShortcuts *shortcuts,
			      int group_num,
			      int item_num,
			      void *data)
{
	EShortcutsViewModel *shortcuts_view_model;

	shortcuts_view_model = E_SHORTCUTS_VIEW_MODEL (data);
	e_shortcut_model_remove_item (E_SHORTCUT_MODEL (shortcuts_view_model), group_num, item_num);
}

static void
shortcuts_update_shortcut_cb (EShortcuts *shortcuts,
			      int group_num,
			      int item_num,
			      void *data)
{
	EShortcutsViewModel *shortcuts_view_model;
	EShortcutsViewModelPrivate *priv;
	const EShortcutItem *shortcut_item;
	char *name_with_unread;

	shortcuts_view_model = E_SHORTCUTS_VIEW_MODEL (data);
	priv = shortcuts_view_model->priv;

	shortcut_item = e_shortcuts_get_shortcut (priv->shortcuts, group_num, item_num);
	g_assert (shortcut_item != NULL);

	name_with_unread = get_name_with_unread (shortcut_item);
	e_shortcut_model_update_item (E_SHORTCUT_MODEL (shortcuts_view_model),
				      group_num, item_num,
				      shortcut_item->uri,
				      name_with_unread,
				      get_icon_for_item (shortcuts_view_model, shortcut_item, FALSE));

	g_free (name_with_unread);
}


/* GtkObject methods.  */

static void
impl_destroy (GtkObject *object)
{
	EShortcutsViewModel *view_model;
	EShortcutsViewModelPrivate *priv;

	view_model = E_SHORTCUTS_VIEW_MODEL (object);
	priv = view_model->priv;

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (EShortcutsViewModelClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = impl_destroy;

	parent_class = gtk_type_class (e_shortcut_model_get_type ());
}

static void
init (EShortcutsViewModel *shortcuts_view_model)
{
	EShortcutsViewModelPrivate *priv;

	priv = g_new (EShortcutsViewModelPrivate, 1);
	priv->shortcuts      = NULL;

	shortcuts_view_model->priv = priv;
}


void
e_shortcuts_view_model_construct (EShortcutsViewModel *model,
				  EShortcuts *shortcuts)
{
	EShortcutsViewModelPrivate *priv;

	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_SHORTCUTS_VIEW_MODEL (model));
	g_return_if_fail (shortcuts != NULL);
	g_return_if_fail (E_IS_SHORTCUTS (shortcuts));

	priv = model->priv;
	g_return_if_fail (priv->shortcuts == NULL);

	priv->shortcuts = shortcuts;

	load_all_shortcuts_into_model (model);

	gtk_signal_connect_while_alive (GTK_OBJECT (priv->shortcuts),
					"new_group", GTK_SIGNAL_FUNC (shortcuts_new_group_cb), model,
					GTK_OBJECT (model));
	gtk_signal_connect_while_alive (GTK_OBJECT (priv->shortcuts),
					"remove_group", GTK_SIGNAL_FUNC (shortcuts_remove_group_cb), model,
					GTK_OBJECT (model));
	gtk_signal_connect_while_alive (GTK_OBJECT (priv->shortcuts),
					"rename_group", GTK_SIGNAL_FUNC (shortcuts_rename_group_cb), model,
					GTK_OBJECT (model));
	gtk_signal_connect_while_alive (GTK_OBJECT (priv->shortcuts),
					"new_shortcut", GTK_SIGNAL_FUNC (shortcuts_new_shortcut_cb), model,
					GTK_OBJECT (model));
	gtk_signal_connect_while_alive (GTK_OBJECT (priv->shortcuts),
					"remove_shortcut", GTK_SIGNAL_FUNC (shortcuts_remove_shortcut_cb), model,
					GTK_OBJECT (model));
	gtk_signal_connect_while_alive (GTK_OBJECT (priv->shortcuts),
					"update_shortcut", GTK_SIGNAL_FUNC (shortcuts_update_shortcut_cb), model,
					GTK_OBJECT (model));
}

EShortcutsViewModel *
e_shortcuts_view_model_new (EShortcuts *shortcuts)
{
	EShortcutsViewModel *new;

	g_return_val_if_fail (shortcuts != NULL, NULL);
	g_return_val_if_fail (E_IS_SHORTCUTS (shortcuts), NULL);

	new = gtk_type_new (e_shortcuts_view_model_get_type ());

	e_shortcuts_view_model_construct (new, shortcuts);

	return new;
}


E_MAKE_TYPE (e_shortcuts_view_model, "EShortcutsViewModel", EShortcutsViewModel, class_init, init, PARENT_TYPE)
