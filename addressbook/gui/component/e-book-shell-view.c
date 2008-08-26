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

static void
book_shell_view_update_actions (EBookShellView *book_shell_view,
                                EABView *view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	ESource *source;
	ESourceSelector *selector;
	GtkAction *action;
	gboolean sensitive;

	if (e_book_shell_view_get_current_view (book_shell_view) != view)
		return;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_window (shell_view);

	selector = E_SOURCE_SELECTOR (book_shell_view->priv->selector);
	source = e_source_selector_peek_primary_selection (selector);

	action = ACTION (ADDRESS_BOOK_STOP);
	sensitive = eab_view_can_stop (view);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_CLIPBOARD_COPY);
	sensitive = eab_view_can_copy (view);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_CLIPBOARD_CUT);
	sensitive = eab_view_can_cut (view);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_CLIPBOARD_PASTE);
	sensitive = eab_view_can_paste (view);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_COPY);
	sensitive = eab_view_can_copy_to_folder (view);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_DELETE);
	sensitive = eab_view_can_delete (view);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_FORWARD);
	sensitive = eab_view_can_send (view);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_MOVE);
	sensitive = eab_view_can_move_to_folder (view);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_OPEN);
	sensitive = eab_view_can_view (view);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_PRINT);
	sensitive = eab_view_can_print (view);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_PRINT_PREVIEW);
	sensitive = eab_view_can_print (view);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_SAVE_AS);
	sensitive = eab_view_can_save_as (view);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_SELECT_ALL);
	sensitive = eab_view_can_select_all (view);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_SEND_MESSAGE);
	sensitive = eab_view_can_send_to (view);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (ADDRESS_BOOK_DELETE);
	if (source != NULL) {
		const gchar *uri;

		uri = e_source_peek_relative_uri (source);
		sensitive = (uri == NULL || strcmp ("system", uri) != 0);
	} else
		sensitive = FALSE;
	gtk_action_set_sensitive (action, sensitive);
}

static void
book_shell_view_editor_weak_notify (EditorUidClosure *closure,
                                    GObject *where_the_object_was)
{
	GHashTable *hash_table;

	hash_table = closure->view->priv->uid_to_editor;
	g_hash_table_remove (hash_table, closure->uid);
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
			book_shell_view_editor_weak_notify, closure);
		gtk_widget_destroy (closure->editor);
		g_hash_table_remove (priv->uid_to_editor, uid);
	}
	g_list_free (keys);

	/* Select and update the current view. */
	view = e_book_shell_view_get_current_view (book_shell_view);
	if (view != NULL) {
#if 0
		eab_view_setup_menus (view, bonobo_uic);
		update_command_state (view, book_shell_view);
#endif
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

static GtkWidget *
book_shell_view_get_content_widget (EShellView *shell_view)
{
}

static GtkWidget *
book_shell_view_get_sidebar_widget (EShellView *shell_view)
{
}

static GtkWidget *
book_shell_view_get_status_widget (EShellView *shell_view)
{
}

static void
book_shell_view_class_init (EBookShellViewClass *class,
                            GTypeModule *type_module)
{
	GObjectClass *object_class;
	EShellViewClass *shell_view_class;

	parent_class = g_type_class_peek-parent (class);
	g_type_class_add_private (class, sizeof (EBookShellViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = book_shell_view_dispose;
	object_class->finalize = book_shell_view_finalize;

	shell_view_class = E_SHELL_VIEW_CLASS (class);
	shell_view_class->label = N_("Contacts");
	shell_view_class->icon_name = "x-office-address-book";
	shell_view_class->type_module = type_module;

	shell_view_class->get_content_widget =
		book_shell_view_get_content_widget;
	shell_view_class->get_sidebar_widget =
		book_shell_view_get_sidebar_widget;
	shell_view_class->get_status_widget =
		book_shell_view_get_status_widget;
}

static void
book_shell_view_init (EBookShellView *book_shell_view)
{
	book_shell_view->priv =
		E_BOOK_SHELL_VIEW_GET_PRIVATE (book_shell_view);

	e_book_shell_view_private_init (book_shell_view);

	g_signal_connect_swapped (
		priv->source_list, "changed",
		G_CALLBACK (book_shell_view_source_list_changed_cb),
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
