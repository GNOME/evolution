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

#include <gal-view-factory-etable.h>
#include <gal-view-factory-minicard.h>

#include <addressbook.h>

static void
set_status_message (EAddressbookView *view,
                    const gchar *message,
                    EBookShellView *book_shell_view)
{
	/* XXX Give EAddressbookView an EShellView pointer
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
	} else if (activity_id == 0)
		activity_id = e_activity_handler_operation_started (
			activity_handler, message, TRUE);
	else
		e_activity_handler_operation_progressing (
			activity_handler, activity_id, message, -1.0);

	book_shell_view->priv->activity_id = activity_id;
}

static void
preview_contact (EBookShellView *book_shell_view,
                 EContact *contact,
                 EAddressbookView *view)
{
	EAddressbookView *current_view;

	current_view = e_book_shell_view_get_current_view (book_shell_view);

	if (view != current_view)
		return;

	eab_contact_display_render (
		EAB_CONTACT_DISPLAY (book_shell_view->priv->preview),
		contact, EAB_CONTACT_DISPLAY_RENDER_NORMAL);
}

static void
book_open_cb (EBook *book,
              EBookStatus status,
              gpointer user_data)
{
	EAddressbookView *view = user_data;
	EAddressbookModel *model;
	ESource *source;

	source = e_book_get_source (book);
	model = e_addressbook_view_get_model (view);

	/* We always set the "source" property on the EAddressbookView
	 * since we use it to reload a previously failed book. */
	g_object_set (view, "source", source, NULL);

	if (status == E_BOOK_ERROR_OK) {
		e_addressbook_model_set_book (model, book);
		e_addressbook_model_force_folder_bar_message (model);
	} else if (status != E_BOOK_ERROR_CANCELLED)
		eab_load_error_dialog (NULL /* XXX */, source, status);
}

static void
book_shell_view_activate_selected_source (EBookShellView *book_shell_view,
                                          ESourceSelector *selector)
{
	EAddressbookView *view;
	EAddressbookModel *model;
	ESource *source;
	GHashTable *hash_table;
	GtkNotebook *notebook;
	GtkWidget *widget;
	const gchar *uid;
	gint page_num;

	notebook = GTK_NOTEBOOK (book_shell_view->priv->notebook);
	source = e_source_selector_peek_primary_selection (selector);

	if (source == NULL)
		return;

	/* XXX Add some get/set functions to EAddressbookView:
	 * 
	 *       eab_view_get_book()   / eab_view_set_book()
	 *       eab_view_get_type()   / eab_view_set_type()
	 *       eab_view_get_source() / eab_view_set_source()
	 */

	uid = e_source_peek_uid (source);
	hash_table = book_shell_view->priv->uid_to_view;
	widget = g_hash_table_lookup (hash_table, uid);

	if (widget != NULL) {
		EBook *book;

		/* There is a view for this UID.  Make sure the view
		 * actually contains an EBook.  The absence of an EBook
		 * suggests a previous load failed, so try again. */
		view = E_ADDRESSBOOK_VIEW (widget);
		model = e_addressbook_view_get_model (view);
		book = e_addressbook_model_get_book (model);

		if (book != NULL)
			g_object_unref (book);
		else {
			g_object_get (view, "source", &source, NULL);

			/* Source can be NULL if a previous load
			 * has not yet reached book_open_cb(). */
			if (source != NULL) {
				book = e_book_new (source, NULL);

				if (book != NULL)
					addressbook_load (book, book_open_cb, view);

				g_object_unref (source);
			}
		}

	} else {
		EShellView *shell_view;
		EBook *book;

		/* Create a view for this UID. */
		shell_view = E_SHELL_VIEW (book_shell_view);
		widget = e_addressbook_view_new (shell_view);
		g_object_set (widget, "type", E_ADDRESSBOOK_VIEW_TABLE, NULL);
		gtk_widget_show (widget);

		g_object_ref_sink (widget);
		gtk_notebook_append_page (notebook, widget, NULL);
		g_hash_table_insert (hash_table, g_strdup (uid), widget);

		g_signal_connect (
			widget, "status-message",
			G_CALLBACK (set_status_message), book_shell_view);

		g_signal_connect_swapped (
			widget, "command-state-change",
			G_CALLBACK (e_book_shell_view_actions_update),
			book_shell_view);

		g_signal_connect_swapped (
			widget, "preview-contact",
			G_CALLBACK (preview_contact), book_shell_view);

		book = e_book_new (source, NULL);

		if (book != NULL)
			addressbook_load (book, book_open_cb, widget);

		view = E_ADDRESSBOOK_VIEW (widget);
		model = e_addressbook_view_get_model (view);
	}

	page_num = gtk_notebook_page_num (notebook, widget);
	gtk_notebook_set_current_page (notebook, page_num);

	e_addressbook_model_force_folder_bar_message (model);
}

static gboolean
book_shell_view_show_popup_menu (GdkEventButton *event,
                                 EShellView *shell_view)
{
	GtkWidget *menu;
	EShellWindow *shell_window;
	const gchar *widget_path;

	widget_path = "/address-book-popup";
	shell_window = e_shell_view_get_shell_window (shell_view);
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
	shell_window = e_shell_view_get_shell_window (shell_view);

	if (event->keyval == GDK_Delete) {
		gtk_action_activate (ACTION (ADDRESS_BOOK_DELETE));
		return TRUE;
	}

	return FALSE;
}

static void
book_shell_view_load_view_collection (EShellViewClass *shell_view_class)
{
	GalViewCollection *collection;
	GalViewFactory *factory;
	ETableSpecification *spec;
	const gchar *base_dir;
	gchar *filename;

	collection = shell_view_class->view_collection;

	base_dir = EVOLUTION_ETSPECDIR;
	spec = e_table_specification_new ();
	filename = g_build_filename (base_dir, ETSPEC_FILENAME, NULL);
	if (!e_table_specification_load_from_file (spec, filename))
		g_critical ("Unable to load ETable specification file "
			    "for address book");
	g_free (filename);

	factory = gal_view_factory_etable_new (spec);
	gal_view_collection_add_factory (collection, factory);
	g_object_unref (factory);
	g_object_unref (spec);

	factory = gal_view_factory_minicard_new ();
	gal_view_collection_add_factory (collection, factory);
	g_object_unref (factory);

	gal_view_collection_load (collection);
}

void
e_book_shell_view_private_init (EBookShellView *book_shell_view,
                                EShellViewClass *shell_view_class)
{
	EBookShellViewPrivate *priv = book_shell_view->priv;
	ESourceList *source_list;
	GHashTable *uid_to_view;
	GHashTable *uid_to_editor;
	GObject *object;

	object = G_OBJECT (shell_view_class->type_module);
	source_list = g_object_get_data (object, "source-list");
	g_return_if_fail (E_IS_SOURCE_LIST (source_list));

	uid_to_view = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_object_unref);

	uid_to_editor = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

	priv->source_list = g_object_ref (source_list);
	priv->contact_actions = gtk_action_group_new ("contacts");
	priv->activity_handler = e_activity_handler_new ();
	priv->uid_to_view = uid_to_view;
	priv->uid_to_editor = uid_to_editor;

	if (!gal_view_collection_loaded (shell_view_class->view_collection))
		book_shell_view_load_view_collection (shell_view_class);
}

void
e_book_shell_view_private_constructed (EBookShellView *book_shell_view)
{
	EBookShellViewPrivate *priv = book_shell_view->priv;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EShellTaskbar *shell_taskbar;
	EShellView *shell_view;
	EShellWindow *shell_window;
	ESourceSelector *selector;
	GConfBridge *bridge;
	GtkWidget *container;
	GtkWidget *widget;
	GObject *object;
	const gchar *key;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	/* Construct view widgets. */

	shell_content = e_shell_view_get_shell_content (shell_view);
	container = GTK_WIDGET (shell_content);

	widget = gtk_vpaned_new ();
	gtk_container_add (GTK_CONTAINER (container), widget);
	priv->paned = g_object_ref (widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (widget), FALSE);
	gtk_paned_add1 (GTK_PANED (container), widget);
	priv->notebook = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_paned_add2 (GTK_PANED (container), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = eab_contact_display_new ();
	gtk_container_add (GTK_CONTAINER (container), widget);
	priv->preview = g_object_ref (widget);
	gtk_widget_show (widget);

	shell_taskbar = e_shell_view_get_shell_taskbar (shell_view);
	e_activity_handler_attach_task_bar (
		priv->activity_handler, shell_taskbar);

	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	selector = e_book_shell_sidebar_get_selector (
		E_BOOK_SHELL_SIDEBAR (shell_sidebar));

	g_signal_connect_swapped (
		selector, "button-press-event",
		G_CALLBACK (book_shell_view_selector_button_press_event_cb),
		book_shell_view);

	g_signal_connect_swapped (
		selector, "key-press-event",
		G_CALLBACK (book_shell_view_selector_key_press_event_cb),
		book_shell_view);

	g_signal_connect_swapped (
		selector, "popup-menu",
		G_CALLBACK (book_shell_view_selector_popup_menu_cb),
		book_shell_view);

	g_signal_connect_swapped (
		selector, "primary-selection-changed",
		G_CALLBACK (book_shell_view_activate_selected_source),
		book_shell_view);

	book_shell_view_activate_selected_source (book_shell_view, selector);

	e_categories_register_change_listener (
		G_CALLBACK (book_shell_view_categories_changed_cb),
		book_shell_view);

	e_book_shell_view_actions_init (book_shell_view);
	e_book_shell_view_update_search_filter (book_shell_view);

	/* Bind GObject properties to GConf keys. */

	bridge = gconf_bridge_get ();

	object = G_OBJECT (ACTION (CONTACT_PREVIEW));
	key = "/apps/evolution/addressbook/display/vpane_position";
	gconf_bridge_bind_property_delayed (bridge, key, object, "position");
}

void
e_book_shell_view_private_dispose (EBookShellView *book_shell_view)
{
	EBookShellViewPrivate *priv = book_shell_view->priv;

	DISPOSE (priv->source_list);

	DISPOSE (priv->contact_actions);

	DISPOSE (priv->paned);
	DISPOSE (priv->notebook);
	DISPOSE (priv->preview);

	DISPOSE (priv->activity_handler);

	g_hash_table_remove_all (priv->uid_to_view);
	g_hash_table_remove_all (priv->uid_to_editor);

	DISPOSE (priv->book);
}

void
e_book_shell_view_private_finalize (EBookShellView *book_shell_view)
{
	EBookShellViewPrivate *priv = book_shell_view->priv;

	g_hash_table_destroy (priv->uid_to_view);
	g_hash_table_destroy (priv->uid_to_editor);

	g_free (priv->password);
}

EAddressbookView *
e_book_shell_view_get_current_view (EBookShellView *book_shell_view)
{
	GtkNotebook *notebook;
	GtkWidget *widget;
	gint page_num;

	g_return_val_if_fail (E_IS_BOOK_SHELL_VIEW (book_shell_view), NULL);

	notebook = GTK_NOTEBOOK (book_shell_view->priv->notebook);
	page_num = gtk_notebook_get_current_page (notebook);
	widget = gtk_notebook_get_nth_page (notebook, page_num);

	return E_ADDRESSBOOK_VIEW (widget);
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
	EShellContent *shell_content;
	EShellView *shell_view;
	GtkRadioAction *action;
	GList *list, *iter;
	GSList *group = NULL;
	gint ii;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);

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
	e_shell_content_set_filter_action (shell_content, action);
}
