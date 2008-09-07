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

#include <addressbook.h>

static void
set_status_message (EABView *view,
                    const gchar *message,
                    EBookShellView *book_shell_view)
{
	/* XXX Give EABView an EShellView pointer
	 *     and have it handle this directly. */

	EActivityHandler *activity_handler;
	guint activity_id;

	activity_handler = book_shell_view->priv->activity_handler;
	activity_id = book_shell_view->priv->activity_id;

	if (message == NULL || *message == '\0') {
		if (activity_id > 0) {
			e_activity_handler_operation_finished (
				activity_handler, activity_id);
			activity_id = 0;
		}
	} else if (activity_id == 0) {
		gchar *client_id = g_strdup_printf ("%p", book_shell_view);

		activity_id = e_activity_handler_operation_started (
			activity_handler, client_id, message, TRUE);
	} else
		e_activity_handler_operation_progressing (
			activity_handler, activity_id, message, -1.0);

	book_shell_view->priv->activity_id = activity_id;
}

static void
search_result (EABView *view,
               EBookViewStatus status,
               EBookShellView *book_shell_view)
{
	/* XXX Give EABView an EShellView pointer
	 *     and have it handle this directly. */

	eab_search_result_dialog (NULL /* XXX */, status);
}

static void
set_folder_bar_message (EABView *view,
                        const gchar *message,
                        EBookShellView *book_shell_view)
{
	/* XXX Give EABView an EShellView pointer
	 *     and have it handle this directly. */

	EShellView *shell_view;
	EABView *current_view;
	GtkWidget *widget;
	const gchar *name;

	shell_view = E_SHELL_VIEW (book_shell_view);

	current_view = e_book_shell_view_get_current_view (book_shell_view);
	if (view != current_view || view->source == NULL)
		return;

	name = e_source_peek_name (view->source);
	widget = e_shell_view_get_sidebar_widget (shell_view);

	e_shell_sidebar_set_primary_text (E_SHELL_SIDEBAR (widget), name);
	e_shell_sidebar_set_secondary_text (E_SHELL_SIDEBAR (widget), message);
}

static void
book_open_cb (EBook *book,
              EBookStatus status,
              gpointer user_data)
{
	EABView *view = user_data;
	ESource *source;

	source = e_book_get_source (book);

	/* We always set the "source" property on the EABView
	 * since we use it to reload a previously failed book. */
	g_object_set (view, "source", source, NULL);

	if (status == E_BOOK_ERROR_OK) {
		g_object_set (view, "book", book, NULL);
		if (view->model)
			eab_model_force_folder_bar_message (view->model);
	} else if (status != E_BOOK_ERROR_CANCELLED)
		eab_load_error_dialog (NULL /* XXX */, source, status);
}

static void
book_shell_view_activate_selected_source (EBookShellView *book_shell_view)
{
	ESource *source;
	ESourceSelector *selector;
	GHashTable *hash_table;
	GtkNotebook *notebook;
	GtkWidget *uid_view;
	const gchar *uid;
	gint page_num;

	notebook = GTK_NOTEBOOK (book_shell_view->priv->notebook);
	selector = E_SOURCE_SELECTOR (book_shell_view->priv->selector);
	source = e_source_selector_peek_primary_selection (selector);

	if (source == NULL)
		return;

	/* XXX Add some get/set functions to EABView:
	 * 
	 *       eab_view_get_book()   / eab_view_set_book()
	 *       eab_view_get_type()   / eab_view_set_type()
	 *       eab_view_get_source() / eab_view_set_source()
	 */

	uid = e_source_peek_uid (source);
	hash_table = book_shell_view->priv->uid_to_view;
	uid_view = g_hash_table_lookup (hash_table, uid);

	if (uid_view != NULL) {
		EBook *book;

		/* There is a view for this UID.  Make sure the view
		 * actually contains an EBook.  The absence of an EBook
		 * suggests a previous load failed, so try again. */
		g_object_get (uid_view, "book", &book, NULL);

		if (book != NULL)
			g_object_unref (book);
		else {
			g_object_get (uid_view, "source", &source, NULL);

			/* Source can be NULL if a previous load
			 * has not yet reached book_open_cb(). */
			if (source != NULL) {
				book = e_book_new (source, NULL);

				if (book != NULL)
					addressbook_load (book, book_open_cb, uid_view);

				g_object_unref (source);
			}
		}

	} else {
		EBook *book;

		/* Create a view for this UID. */
		uid_view = eab_view_new ();
		g_object_set (uid_view, "type", EAB_VIEW_TABLE, NULL);
		gtk_widget_show (uid_view);

		gtk_notebook_append_page (notebook, uid_view, NULL);
		g_hash_table_insert (hash_table, g_strdup (uid), uid_view);

		g_signal_connect (
			uid_view, "status-message",
			G_CALLBACK (set_status_message), book_shell_view);

		g_signal_connect (
			uid_view, "search-result",
			G_CALLBACK (search_result), book_shell_view);

		g_signal_connect (
			uid_view, "folder-bar-message",
			G_CALLBACK (set_folder_bar_message), book_shell_view);

		g_signal_connect_swapped (
			uid_view, "command-state-change",
			G_CALLBACK (e_book_shell_view_update_actions),
			book_shell_view);

		book = e_book_new (source, NULL);

		if (book != NULL)
			addressbook_load (book, book_open_cb, uid_view);
	}

	page_num = gtk_notebook_page_num (notebook, uid_view);
	gtk_notebook_set_current_page (notebook, page_num);

	if (EAB_VIEW (uid_view)->model)
		eab_model_force_folder_bar_message (EAB_VIEW (uid_view)->model);
}

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

static void
book_shell_view_categories_changed_cb (EBookShellView *book_shell_view)
{
	e_book_shell_view_update_search_filter (book_shell_view);
}

static gboolean
book_shell_view_selector_button_press_event_cb (EShellView *shell_view,
                                                GdkEventButton *event)
{
	/* XXX Use ESourceSelector's "popup-event" signal instead. */

	if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
		return book_shell_view_show_popup_menu (event, shell_view);

	return FALSE;
}

static gboolean
book_shell_view_selector_popup_menu_cb (EShellView *shell_view)
{
	/* XXX Use ESourceSelector's "popup-event" signal instead. */

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

static void
book_shell_view_primary_selection_changed_cb (EBookShellView *book_shell_view,
                                              ESourceSelector *selector)
{
	book_shell_view_activate_selected_source (book_shell_view);
}

void
e_book_shell_view_private_init (EBookShellView *book_shell_view)
{
	EBookShellViewPrivate *priv = book_shell_view->priv;
	EShellView *shell_view;
	GHashTable *uid_to_view;
	GHashTable *uid_to_editor;
	GtkWidget *container;
	GtkWidget *widget;

	shell_view = E_SHELL_VIEW (book_shell_view);

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

	/* Construct view widgets. */

	widget = gtk_notebook_new ();
	container = e_shell_view_get_content_widget (shell_view);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (widget), FALSE);
	gtk_container_add (GTK_CONTAINER (container), widget);
	priv->notebook = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = e_shell_view_get_taskbar_widget (shell_view);
	e_activity_handler_attach_task_bar (
		priv->activity_handler, E_TASK_BAR (widget));

	widget = gtk_scrolled_window_new (NULL, NULL);
	container = e_shell_view_get_sidebar_widget (shell_view);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = e_addressbook_selector_new (priv->source_list);
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

	g_signal_connect_swapped (
		widget, "primary-selection-changed",
		G_CALLBACK (book_shell_view_primary_selection_changed_cb),
		book_shell_view);

	book_shell_view_activate_selected_source (book_shell_view);

	e_categories_register_change_listener (
		G_CALLBACK (book_shell_view_categories_changed_cb),
		book_shell_view);
	e_book_shell_view_update_search_filter (book_shell_view);
}

void
e_book_shell_view_private_dispose (EBookShellView *book_shell_view)
{
	EBookShellViewPrivate *priv = book_shell_view->priv;

	DISPOSE (priv->contact_actions);

	DISPOSE (priv->notebook);
	DISPOSE (priv->selector);

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

void
e_book_shell_view_update_search_filter (EBookShellView *book_shell_view)
{
	EShellView *shell_view;
	GtkRadioAction *action;
	GtkWidget *widget;
	GList *list, *iter;
	GSList *group = NULL;
	gint ii;

	/* Dig up the search bar. */
	shell_view = E_SHELL_VIEW (book_shell_view);
	widget = e_shell_view_get_content_widget (shell_view);
	widget = e_shell_content_get_search_bar (E_SHELL_CONTENT (widget));

	action = gtk_radio_action_new (
		"category-any", _("Any Category"), NULL, NULL, -1);

	gtk_radio_action_set_group (action, group);
	group = gtk_radio_action_get_group (action);

	list = e_categories_get_list ();
	for (iter = list, ii = 0; iter != NULL; iter = iter->next, ii++) {
		const gchar *category_name = iter->data;
		gchar *action_name;

		action_name = g_strdup_printf ("category-%d", ii);
		action = gtk_radio_action_new (
			action_name, category_name, NULL, NULL, ii);
		g_free (action_name);

		gtk_radio_action_set_group (action, group);
		group = gtk_radio_action_get_group (action);
	}
	g_list_free (list);

	/* Use any action in the group; doesn't matter which. */
	e_search_bar_set_filter_action (E_SEARCH_BAR (widget), action);
}
