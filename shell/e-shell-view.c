/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 * e-shell-view.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "e-shell-view.h"

#include <glib/gi18n.h>

#include "e-shell-window.h"
#include "e-shell-window-actions.h"

#define E_SHELL_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SHELL_VIEW, EShellViewPrivate))

struct _EShellViewPrivate {
	gchar *title;
	gpointer window;  /* weak pointer */
};

enum {
	PROP_0,
	PROP_TITLE,
	PROP_WINDOW
};

static gpointer parent_class;

static gint
shell_view_compare_actions (GtkAction *action1,
                            GtkAction *action2)
{
	gchar *label1, *label2;
	gint result;

	/* XXX This is really inefficient, but we're only sorting
	 *     a small number of actions (repeatedly, though). */

	g_object_get (action1, "label", &label1, NULL);
	g_object_get (action2, "label", &label2, NULL);

	result = g_utf8_collate (label1, label2);

	g_free (label1);
	g_free (label2);

	return result;
}

static void
shell_view_extract_actions (EShellView *shell_view,
                            GList **source_list,
			    GList **destination_list)
{
	GList *match_list = NULL;
	GList *iter;

	/* Pick out the actions from the source list that are tagged
	 * as belonging to the given EShellView and move them to the
	 * destination list. */

	/* Example: Suppose [A] and [C] are tagged for this EShellView.
	 *
	 *        source_list = [A] -> [B] -> [C]
	 *                       ^             ^
	 *                       |             |
	 *         match_list = [ ] --------> [ ]
	 *
	 *  
	 *   destination_list = [1] -> [2]  (other actions)
	 */
	for (iter = *source_list; iter != NULL; iter = iter->next) {
		GtkAction *action = iter->data;
		EShellView *action_shell_view;

		action_shell_view = g_object_get_data (
			G_OBJECT (action), "shell-view");

		if (action_shell_view != shell_view)
			continue;

		match_list = g_list_append (match_list, iter);
	}

	/* source_list = [B]   match_list = [A] -> [C] */
	for (iter = match_list; iter != NULL; iter = iter->next) {
		GList *link = iter->data;

		iter->data = link->data;
		*source_list = g_list_delete_link (*source_list, link);
	}

	/* destination_list = [1] -> [2] -> [A] -> [C] */
	*destination_list = g_list_concat (*destination_list, match_list);
}

static void
shell_view_register_new_actions (EShellView *shell_view,
                                 GtkActionGroup *action_group,
                                 const GtkActionEntry *entries,
                                 guint n_entries)
{
	guint ii;

	gtk_action_group_add_actions (
		action_group, entries, n_entries, shell_view);

	/* Tag each action with the shell view that registered it.
	 * This is used to help sort items in the "New" menu. */

	for (ii = 0; ii < n_entries; ii++) {
		const gchar *action_name;
		GtkAction *action;

		action_name = entries[ii].name;

		action = gtk_action_group_get_action (
			action_group, action_name);

		g_object_set_data (
			G_OBJECT (action), "shell-view", shell_view);
	}
}

static void
shell_view_set_window (EShellView *shell_view,
                       GtkWidget *window)
{
	g_return_if_fail (GTK_IS_WINDOW (window));

	shell_view->priv->window = window;

	g_object_add_weak_pointer (
		G_OBJECT (window), &shell_view->priv->window);
}

static void
shell_view_set_property (GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_TITLE:
			e_shell_view_set_title (
				E_SHELL_VIEW (object),
				g_value_get_string (value));
			return;

		case PROP_WINDOW:
			shell_view_set_window (
				E_SHELL_VIEW (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_view_get_property (GObject *object,
                         guint property_id,
                         GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_TITLE:
			g_value_set_string (
				value, e_shell_view_get_title (
				E_SHELL_VIEW (object)));
			return;

		case PROP_WINDOW:
			g_value_set_object (
				value, e_shell_view_get_window (
				E_SHELL_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_view_dispose (GObject *object)
{
	EShellViewPrivate *priv;

	priv = E_SHELL_VIEW_GET_PRIVATE (object);

	if (priv->window != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (priv->window), &priv->window);
		priv->window = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
shell_view_finalize (GObject *object)
{
	EShellViewPrivate *priv;

	priv = E_SHELL_VIEW_GET_PRIVATE (object);

	g_free (priv->title);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GtkWidget *
shell_view_create_new_menu (EShellView *shell_view)
{
	GtkActionGroup *action_group;
	GList *new_item_actions;
	GList *new_source_actions;
	GList *iter, *list = NULL;
	GtkWidget *menu;
	GtkWidget *separator;
	GtkWidget *window;

	window = e_shell_view_get_window (shell_view);

	/* Get sorted lists of "new item" and "new source" actions. */

	action_group = E_SHELL_WINDOW_ACTION_GROUP_NEW_ITEM (window);

	new_item_actions = g_list_sort (
		gtk_action_group_list_actions (action_group),
		(GCompareFunc) shell_view_compare_actions);

	action_group = E_SHELL_WINDOW_ACTION_GROUP_NEW_SOURCE (window);

	new_source_actions = g_list_sort (
		gtk_action_group_list_actions (action_group),
		(GCompareFunc) shell_view_compare_actions);

	/* Give priority to actions that belong to this shell view. */

	shell_view_extract_actions (
		shell_view, &new_item_actions, &list);

	shell_view_extract_actions (
		shell_view, &new_source_actions, &list);

	/* Convert the actions to menu item proxy widgets. */

	for (iter = list; iter != NULL; iter = iter->next)
		iter->data = gtk_action_create_menu_item (iter->data);

	for (iter = new_item_actions; iter != NULL; iter = iter->next)
		iter->data = gtk_action_create_menu_item (iter->data);

	for (iter = new_source_actions; iter != NULL; iter = iter->next)
		iter->data = gtk_action_create_menu_item (iter->data);

	/* Add menu separators. */

	separator = gtk_separator_menu_item_new ();
	new_item_actions = g_list_prepend (new_item_actions, separator);

	separator = gtk_separator_menu_item_new ();
	new_source_actions = g_list_prepend (new_source_actions, separator);

	/* Merge everything into one list, reflecting the menu layout. */

	list = g_list_concat (list, new_item_actions);
	new_item_actions = NULL;    /* just for clarity */

	list = g_list_concat (list, new_source_actions);
	new_source_actions = NULL;  /* just for clarity */

	/* And finally, build the menu. */

	menu = gtk_menu_new ();

	for (iter = list; iter != NULL; iter = iter->next)
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), iter->data);

	g_list_free (list);

	return menu;
}

static void
shell_view_class_init (EShellViewClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EShellViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = shell_view_set_property;
	object_class->get_property = shell_view_get_property;
	object_class->dispose = shell_view_dispose;
	object_class->finalize = shell_view_finalize;

	class->create_new_menu = shell_view_create_new_menu;

	g_object_class_install_property (
		object_class,
		PROP_TITLE,
		g_param_spec_string (
			"title",
			_("Title"),
			_("The title of the shell view"),
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_WINDOW,
		g_param_spec_object (
			"window",
			_("Window"),
			_("The window to which the shell view belongs"),
			GTK_TYPE_WINDOW,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
shell_view_init (EShellView *shell_view)
{
	shell_view->priv = E_SHELL_VIEW_GET_PRIVATE (shell_view);
}

GType
e_shell_view_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		const GTypeInfo type_info = {
			sizeof (EShellViewClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) shell_view_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EShellView),
			0,     /* n_preallocs */
			(GInstanceInitFunc) shell_view_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			G_TYPE_OBJECT, "EShellView",
			&type_info, G_TYPE_FLAG_ABSTRACT);
	}

	return type;
}

const gchar *
e_shell_view_get_title (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->title;
}

void
e_shell_view_set_title (EShellView *shell_view,
                        const gchar *title)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	g_free (shell_view->priv->title);
	shell_view->priv->title = g_strdup (title);

	g_object_notify (G_OBJECT (shell_view), "title");
}

GtkWidget *
e_shell_view_get_window (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->window;
}

GtkWidget *
e_shell_view_create_new_menu (EShellView *shell_view)
{
	EShellViewClass *class;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	class = E_SHELL_VIEW_CLASS (shell_view);
	g_return_val_if_fail (class->create_new_menu != NULL, NULL);

	return class->create_new_menu (shell_view);
}

GtkWidget *
e_shell_view_get_content_widget (EShellView *shell_view)
{
	EShellViewClass *class;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	class = E_SHELL_VIEW_CLASS (shell_view);
	g_return_val_if_fail (class->get_content_widget != NULL, NULL);

	return class->get_content_widget (shell_view);
}

GtkWidget *
e_shell_view_get_sidebar_widget (EShellView *shell_view)
{
	EShellViewClass *class;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	class = E_SHELL_VIEW_CLASS (shell_view);
	g_return_val_if_fail (class->get_sidebar_widget != NULL, NULL);

	return class->get_sidebar_widget (shell_view);
}

GtkWidget *
e_shell_view_get_status_widget (EShellView *shell_view)
{
	EShellViewClass *class;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	class = E_SHELL_VIEW_CLASS (shell_view);
	g_return_val_if_fail (class->get_status_widget != NULL, NULL);

	return class->get_status_widget (shell_view);
}

void
e_shell_view_register_new_item_actions (EShellView *shell_view,
                                        const GtkActionEntry *entries,
                                        guint n_entries)
{
	GtkWidget *window;
	GtkActionGroup *action_group;

	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (entries != NULL);

	window = e_shell_view_get_window (shell_view);
	action_group = E_SHELL_WINDOW_ACTION_GROUP_NEW_ITEM (window);

	shell_view_register_new_actions (
		shell_view, action_group, entries, n_entries);
}

void
e_shell_view_register_new_source_actions (EShellView *shell_view,
                                          const GtkActionEntry *entries,
					  guint n_entries)
{
	GtkWidget *window;
	GtkActionGroup *action_group;

	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (entries != NULL);

	window = e_shell_view_get_window (shell_view);
	action_group = E_SHELL_WINDOW_ACTION_GROUP_NEW_SOURCE (window);

	shell_view_register_new_actions (
		shell_view, action_group, entries, n_entries);
}
