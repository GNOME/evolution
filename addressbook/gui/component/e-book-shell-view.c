/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-book-shell-view.c
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

#include "e-book-shell-view-private.h"

GType e_book_shell_view_type = 0;
static gpointer parent_class;

static ESource *
book_shell_view_load_primary_source (EBookShellView *book_shell_view)
{
	GConfClient *client;
	ESourceList *source_list;
	ESource *source = NULL;
	const gchar *key;
	gchar *uid;

	/* XXX If ESourceSelector had a "primary-uid" property,
	 *     we could just bind the GConf key to it. */

	source_list = book_shell_view->priv->source_list;

	client = gconf_client_get_default ();
	key = "/apps/evolution/addressbook/display/primary_addressbook";
	uid = gconf_client_get_string (client, key, NULL);
	g_object_unref (client);

	if (uid != NULL) {
		source = e_source_list_peek_source_by_uid (source_list, uid);
		g_free (uid);
	} else {
		GSList *groups;

		/* Dig up the first source in the source list.
		 * XXX libedataserver should provide API for this. */
		groups = e_source_list_peek_groups (source_list);
		while (groups != NULL) {
			ESourceGroup *source_group = groups->data;
			GSList *sources;

			sources = e_source_group_peek_sources (source_group);
			if (sources != NULL) {
				source = sources->data;
				break;
			}

			groups = g_slist_next (groups);
		}
	}

	return source;
}

static void
book_shell_view_save_primary_source (EBookShellView *book_shell_view)
{
	GConfClient *client;
	ESourceSelector *selector;
	ESource *source;
	const gchar *key;
	const gchar *string;

	/* XXX If ESourceSelector had a "primary-uid" property,
	 *     we could just bind the GConf key to it. */

	selector = E_SOURCE_SELECTOR (book_shell_view->priv->selector);
	source = e_source_selector_peek_primary_selection (selector);
	if (source == NULL)
		return;

	client = gconf_client_get_default ();
	key = "/apps/evolution/addressbook/display/primary_addressbook";
	string = e_source_peek_uid (source);
	gconf_client_set_string (client, key, string, NULL);
	g_object_unref (client);
}

static void
book_shell_view_source_list_changed_cb (EBookShellView *book_shell_view,
                                        ESourceList *source_list)
{
	EBookShellViewPrivate *priv = book_shell_view->priv;
	GtkNotebook *notebook;
	GList *keys, *iter;
	EABView *view;

	notebook = GTK_NOTEBOOK (priv->notebook);

	keys = g_hash_table_get_keys (priv->uid_to_view);
	for (iter = keys; iter != NULL; iter = iter->next) {
		gchar *uid = iter->data;
		GtkWidget *widget;
		gint page_num;

		/* If the source still exists, move on. */
		if (e_source_list_peek_source_by_uid (source_list, uid))
			continue;

		/* Remove the view for the deleted source. */
		widget = g_hash_table_lookup (priv->uid_to_view, uid);
		page_num = gtk_notebook_page_num (notebook, widget);
		gtk_notebook_remove_page (notebook, page_num);
		g_hash_table_remove (priv->uid_to_view, uid);
	}
	g_list_free (keys);

	keys = g_hash_table_get_keys (priv->uid_to_editor);
	for (iter = keys; iter != NULL; iter = iter->next) {
		gchar *uid = iter->data;
		EditorUidClosure *closure;

		/* If the source still exists, move on. */
		if (e_source_list_peek_source_by_uid (source_list, uid))
			continue;

		/* Remove the editor for the deleted source. */
		closure = g_hash_table_lookup (priv->uid_to_editor, uid);
		g_object_weak_unref (
			G_OBJECT (closure->editor), (GWeakNotify)
			e_book_shell_view_editor_weak_notify, closure);
		gtk_widget_destroy (closure->editor);
		g_hash_table_remove (priv->uid_to_editor, uid);
	}
	g_list_free (keys);

	/* Select and update the current view. */
	view = e_book_shell_view_get_current_view (book_shell_view);
	if (view != NULL) {
#if 0
		eab_view_setup_menus (view, bonobo_uic);
#endif
		e_book_shell_view_update_actions (book_shell_view, view);
	}
}

static void
book_shell_view_dispose (GObject *object)
{
	e_book_shell_view_private_dispose (E_BOOK_SHELL_VIEW (object));

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
book_shell_view_finalize (GObject *object)
{
	e_book_shell_view_private_finalize (E_BOOK_SHELL_VIEW (object));

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
book_shell_view_constructed (GObject *object)
{
	EBookShellView *book_shell_view;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EShellTaskbar *shell_taskbar;
	EShellView *shell_view;
	GtkWidget *widget;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	shell_view = E_SHELL_VIEW (object);
	book_shell_view = E_BOOK_SHELL_VIEW (object);

	widget = book_shell_view->priv->notebook;
	shell_content = e_shell_view_get_content (shell_view);
	gtk_container_add (GTK_CONTAINER (shell_content), widget);

	widget = book_shell_view->priv->scrolled_window;
	shell_sidebar = e_shell_view_get_sidebar (shell_view);
	gtk_container_add (GTK_CONTAINER (shell_sidebar), widget);

	shell_taskbar = e_shell_view_get_taskbar (shell_view);
	e_activity_handler_attach_task_bar (
		book_shell_view->priv->activity_handler, shell_taskbar);

	e_book_shell_view_actions_init (book_shell_view);
	e_book_shell_view_update_search_filter (book_shell_view);
}

static void
book_shell_view_changed (EShellView *shell_view)
{
	EBookShellViewPrivate *priv;
	GtkActionGroup *action_group;
	gboolean visible;

	priv = E_BOOK_SHELL_VIEW_GET_PRIVATE (shell_view);

	action_group = priv->contact_actions;
	visible = e_shell_view_is_selected (shell_view);
	gtk_action_group_set_visible (action_group, visible);
}

static void
book_shell_view_class_init (EBookShellViewClass *class,
                            GTypeModule *type_module)
{
	GObjectClass *object_class;
	EShellViewClass *shell_view_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EBookShellViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = book_shell_view_dispose;
	object_class->finalize = book_shell_view_finalize;
	object_class->constructed = book_shell_view_constructed;

	shell_view_class = E_SHELL_VIEW_CLASS (class);
	shell_view_class->label = N_("Contacts");
	shell_view_class->icon_name = "x-office-address-book";
	shell_view_class->type_module = type_module;
	shell_view_class->changed = book_shell_view_changed;
}

static void
book_shell_view_init (EBookShellView *book_shell_view)
{
	ESourceSelector *selector;
	ESource *source;

	book_shell_view->priv =
		E_BOOK_SHELL_VIEW_GET_PRIVATE (book_shell_view);

	e_book_shell_view_private_init (book_shell_view);

	g_signal_connect_swapped (
		book_shell_view->priv->source_list, "changed",
		G_CALLBACK (book_shell_view_source_list_changed_cb),
		book_shell_view);

	selector = E_SOURCE_SELECTOR (book_shell_view->priv->selector);
	source = book_shell_view_load_primary_source (book_shell_view);
	if (source != NULL)
		e_source_selector_set_primary_selection (selector, source);
	g_signal_connect_swapped (
		selector, "primary-selection-changed",
		G_CALLBACK (book_shell_view_save_primary_source),
		book_shell_view);
}

GType
e_book_shell_view_get_type (GTypeModule *type_module)
{
	if (e_book_shell_view_type == 0) {
		const GTypeInfo type_info = {
			sizeof (EBookShellViewClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) book_shell_view_class_init,
			(GClassFinalizeFunc) NULL,
			type_module,
			sizeof (EBookShellView),
			0,    /* n_preallocs */
			(GInstanceInitFunc) book_shell_view_init,
			NULL  /* value_table */
		};

		e_book_shell_view_type =
			g_type_module_register_type (
				type_module, E_TYPE_SHELL_VIEW,
				"EBookShellView", &type_info, 0);
	}

	return e_book_shell_view_type;
}
