/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-book-shell-view-private.c
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

static gboolean
book_shell_view_show_popup_menu (GdkEventButton *event,
                                 EShellView *shell_view)
{
	GtkWidget *menu;
	EShellWindow *shell_window;
	const gchar *widget_path;

	widget_path = "/address-book-popup";
	shell_window = e_shell_view_get_window (shell_view);
	menu = e_shell_window_get_managed_widget (shell_window, widget_path);

	if (event != NULL)
		gtk_menu_popup (
			GTK_MENU (menu), NULL, NULL, NULL, NULL,
			event->button, event->time);
	else
		gtk_menu_popup (
			GTK_MENU (menu), NULL, NULL, NULL, NULL,
			0, gtk_get_current_event_time ());

	return TRUE;
}

static gboolean
book_shell_view_selector_button_press_event_cb (EShellView *shell_view,
                                                GdkEventButton *event)
{
	if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
		return book_shell_view_show_popup_menu (event, shell_view);

	return FALSE;
}

static gboolean
book_shell_view_selector_popup_menu_cb (EShellView *shell_view)
{
	return book_shell_view_show_popup_menu (NULL, shell_view);
}

static gboolean
book_shell_view_selector_key_press_event_cb (EShellView *shell_view,
                                             GdkEventKey *event)
{
	EShellWindow *shell_window;

	/* Needed for the ACTION() macro. */
	shell_window = e_shell_view_get_window (shell_view);

	if (event->keyval == GDK_Delete) {
		gtk_action_activate (ACTION (ADDRESS_BOOK_DELETE));
		return TRUE;
	}

	return FALSE;
}

void
e_book_shell_view_private_init (EBookShellView *book_shell_view)
{
	EBookShellViewPrivate *priv = book_shell_view->priv;
	GHashTable *uid_to_view;
	GHashTable *uid_to_editor;
	GtkWidget *container;
	GtkWidget *widget;

	uid_to_view = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_object_unref);

	uid_to_editor = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

	priv->contact_actions = gtk_action_group_new ("contacts");
	priv->activity_handler = e_activity_handler_new ();
	priv->uid_to_view = uid_to_view;
	priv->uid_to_editor = uid_to_editor;

	e_book_get_addressbooks (&priv->source_list, NULL);

	widget = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (widget), FALSE);
	priv->notebook = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	widget = e_task_bar_new ();
	e_activity_handler_attach_task_bar (
		priv->activity_handler, E_TASK_BAR (widget));
	priv->task_bar = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	priv->scrolled_window = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	container = widget;

	widget = e_source_selector_new (priv->source_list);
	e_source_selector_show_selection (E_SOURCE_SELECTOR (widget), FALSE);
	gtk_container_add (GTK_CONTAINER (container), widget);
	priv->selector = g_object_ref (widget);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "button-press-event",
		G_CALLBACK (book_shell_view_selector_button_press_event_cb),
		book_shell_view);

	g_signal_connect_swapped (
		widget, "key-press-event",
		G_CALLBACK (book_shell_view_selector_key_press_event_cb),
		book_shell_view);

	g_signal_connect_swapped (
		widget, "popup-menu",
		G_CALLBACK (book_shell_view_selector_popup_menu_cb),
		book_shell_view);
}

void
e_book_shell_view_private_dispose (EBookShellView *book_shell_view)
{
	EBookShellViewPrivate *priv = book_shell_view->priv;

	DISPOSE (priv->contact_actions);

	DISPOSE (priv->notebook);
	DISPOSE (priv->scrolled_window);
	DISPOSE (priv->selector);
	DISPOSE (priv->task_bar);

	DISPOSE (priv->activity_handler);

	g_hash_table_remove_all (priv->uid_to_view);
	g_hash_table_remove_all (priv->uid_to_editor);

	DISPOSE (priv->book);
	DISPOSE (priv->source_list);
}

void
e_book_shell_view_private_finalize (EBookShellView *book_shell_view)
{
	EBookShellViewPrivate *priv = book_shell_view->priv;

	g_hash_table_destroy (priv->uid_to_view);
	g_hash_table_destroy (priv->uid_to_editor);

	g_free (priv->password);
}

EABView *
e_book_shell_view_get_current_view (EBookShellView *book_shell_view)
{
	GtkNotebook *notebook;
	GtkWidget *widget;
	gint page_num;

	g_return_val_if_fail (E_IS_BOOK_SHELL_VIEW (book_shell_view), NULL);

	notebook = GTK_NOTEBOOK (book_shell_view->priv->notebook);
	page_num = gtk_notebook_get_current_page (notebook);
	widget = gtk_notebook_get_nth_page (notebook, page_num);

	return EAB_VIEW (widget);
}

void
e_book_shell_view_editor_weak_notify (EditorUidClosure *closure,
                                      GObject *where_the_object_was)
{
	GHashTable *hash_table;

	hash_table = closure->view->priv->uid_to_editor;
	g_hash_table_remove (hash_table, closure->uid);
}
