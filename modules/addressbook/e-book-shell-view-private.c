/*
 * e-book-shell-view-private.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-util/e-util-private.h"

#include "e-book-shell-view-private.h"

#include "widgets/menus/gal-view-factory-etable.h"
#include "addressbook/gui/widgets/gal-view-factory-minicard.h"

static void
open_contact (EBookShellView *book_shell_view,
              EContact *contact,
              gboolean is_new_contact,
              EAddressbookView *view)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EAddressbookModel *model;
	EABEditor *editor;
	EBook *book;
	gboolean editable;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	model = e_addressbook_view_get_model (view);
	book = e_addressbook_model_get_book (model);
	editable = e_addressbook_model_get_editable (model);

	if (e_contact_get (contact, E_CONTACT_IS_LIST))
		editor = e_contact_list_editor_new (
			shell, book, contact, is_new_contact, editable);
	else
		editor = e_contact_editor_new (
			shell, book, contact, is_new_contact, editable);

	eab_editor_show (editor);
}

static void
popup_event (EBookShellView *book_shell_view,
             GdkEventButton *event)
{
	EShellView *shell_view;
	const gchar *widget_path;

	widget_path = "/contact-popup";
	shell_view = E_SHELL_VIEW (book_shell_view);

	e_shell_view_show_popup_menu (shell_view, widget_path, event);
}

static void
book_shell_view_selection_change_foreach (gint row,
                                          EBookShellView *book_shell_view)
{
	EBookShellContent *book_shell_content;
	EAddressbookView *view;
	EAddressbookModel *model;
	EContact *contact;

	/* XXX A "foreach" function is kind of a silly way to retrieve
	 *     the one and only selected contact, but this is the only
	 *     means that ESelectionModel provides. */

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	model = e_addressbook_view_get_model (view);
	contact = e_addressbook_model_get_contact (model, row);

	e_book_shell_content_set_preview_contact (book_shell_content, contact);
	book_shell_view->priv->preview_index = row;
}

static void
selection_change (EBookShellView *book_shell_view,
                  EAddressbookView *view)
{
	EBookShellContent *book_shell_content;
	EAddressbookView *current_view;
	ESelectionModel *selection_model;
	EShellView *shell_view;
	gint n_selected;

	shell_view = E_SHELL_VIEW (book_shell_view);
	book_shell_content = book_shell_view->priv->book_shell_content;
	current_view = e_book_shell_content_get_current_view (book_shell_content);

	if (view != current_view)
		return;

	e_shell_view_update_actions (shell_view);

	selection_model = e_addressbook_view_get_selection_model (view);

	n_selected = (selection_model != NULL) ?
		e_selection_model_selected_count (selection_model) : 0;

	if (n_selected == 1)
		e_selection_model_foreach (
			selection_model, (EForeachFunc)
			book_shell_view_selection_change_foreach,
			book_shell_view);
	else {
		e_book_shell_content_set_preview_contact (
			book_shell_content, NULL);
		book_shell_view->priv->preview_index = -1;
	}
}

static void
contact_changed (EBookShellView *book_shell_view,
                 gint index,
                 EAddressbookModel *model)
{
	EBookShellContent *book_shell_content;
	EContact *contact;

	g_return_if_fail (E_IS_SHELL_VIEW (book_shell_view));
	g_return_if_fail (book_shell_view->priv != NULL);

	book_shell_content = book_shell_view->priv->book_shell_content;

	contact = e_addressbook_model_contact_at (model, index);

	if (book_shell_view->priv->preview_index != index)
		return;

	/* Re-render the same contact. */
	e_book_shell_content_set_preview_contact (book_shell_content, contact);
}

static void
contacts_removed (EBookShellView *book_shell_view,
                  GArray *removed_indices,
                  EAddressbookModel *model)
{
	EBookShellContent *book_shell_content;
	EContact *preview_contact;

	g_return_if_fail (E_IS_SHELL_VIEW (book_shell_view));
	g_return_if_fail (book_shell_view->priv != NULL);

	book_shell_content = book_shell_view->priv->book_shell_content;

	preview_contact =
		e_book_shell_content_get_preview_contact (book_shell_content);

	if (preview_contact == NULL)
		return;

	/* Is the displayed contact still in the model? */
	if (e_addressbook_model_find (model, preview_contact) < 0)
		return;

	/* If not, clear the contact display. */
	e_book_shell_content_set_preview_contact (book_shell_content, NULL);
	book_shell_view->priv->preview_index = -1;
}

static void
book_shell_view_loaded_cb (ESource *source,
                           GAsyncResult *result,
                           EAddressbookView *view)
{
	EBook *book;
	GError *error = NULL;

	book = e_load_book_source_finish (source, result, &error);

	if (book != NULL) {
		EAddressbookModel *model;

		g_warn_if_fail (error == NULL);
		model = e_addressbook_view_get_model (view);
		e_addressbook_model_set_book (model, book);
		e_addressbook_model_force_folder_bar_message (model);

	} else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		eab_load_error_dialog (NULL /* XXX */, source, error);

	if (error != NULL)
		g_error_free (error);

	g_object_unref (view);
}

static void
book_shell_view_activate_selected_source (EBookShellView *book_shell_view,
                                          ESourceSelector *selector)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EBookShellContent *book_shell_content;
	EAddressbookView *view;
	EAddressbookModel *model;
	ESource *source;
	GalViewInstance *view_instance;
	GHashTable *hash_table;
	GtkWidget *widget;
	const gchar *uid;
	gchar *view_id;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	book_shell_content = book_shell_view->priv->book_shell_content;
	source = e_source_selector_peek_primary_selection (selector);

	if (source == NULL)
		return;

	uid = e_source_peek_uid (source);
	hash_table = book_shell_view->priv->uid_to_view;
	widget = g_hash_table_lookup (hash_table, uid);

	if (widget != NULL) {
		/* There is a view for this UID.  Make sure the view
		 * actually contains an EBook.  The absence of an EBook
		 * suggests a previous load failed, so try again. */
		view = E_ADDRESSBOOK_VIEW (widget);
		model = e_addressbook_view_get_model (view);
		source = e_addressbook_view_get_source (view);

		if (e_addressbook_model_get_book (model) == NULL)
			/* XXX No way to cancel this? */
			e_load_book_source_async (
				source,
				GTK_WINDOW (shell_window),
				NULL, (GAsyncReadyCallback)
				book_shell_view_loaded_cb,
				g_object_ref (view));

	} else {
		/* Create a view for this UID. */
		widget = e_addressbook_view_new (shell_view, source);
		gtk_widget_show (widget);

		/* Default searching options for a new view. */
		e_addressbook_view_set_search (
			E_ADDRESSBOOK_VIEW (widget),
			CONTACT_FILTER_ANY_CATEGORY,
			CONTACT_SEARCH_NAME_CONTAINS,
			NULL, NULL);

		e_book_shell_content_insert_view (
			book_shell_content,
			E_ADDRESSBOOK_VIEW (widget));

		g_hash_table_insert (
			hash_table, g_strdup (uid),
			g_object_ref (widget));

		g_signal_connect_object (
			widget, "open-contact", G_CALLBACK (open_contact),
			book_shell_view, G_CONNECT_SWAPPED);

		g_signal_connect_object (
			widget, "popup-event", G_CALLBACK (popup_event),
			book_shell_view, G_CONNECT_SWAPPED);

		g_signal_connect_object (
			widget, "command-state-change",
			G_CALLBACK (e_shell_view_update_actions),
			book_shell_view, G_CONNECT_SWAPPED);

		g_signal_connect_object (
			widget, "selection-change", G_CALLBACK (selection_change),
			book_shell_view, G_CONNECT_SWAPPED);

		view = E_ADDRESSBOOK_VIEW (widget);
		model = e_addressbook_view_get_model (view);

		/* XXX No way to cancel this? */
		e_load_book_source_async (
			source, GTK_WINDOW (shell_window), NULL,
			(GAsyncReadyCallback) book_shell_view_loaded_cb,
			g_object_ref (view));

		g_signal_connect_object (
			model, "contact-changed",
			G_CALLBACK (contact_changed),
			book_shell_view, G_CONNECT_SWAPPED);

		g_signal_connect_object (
			model, "contacts-removed",
			G_CALLBACK (contacts_removed),
			book_shell_view, G_CONNECT_SWAPPED);
	}

	e_book_shell_content_set_current_view (
		book_shell_content, E_ADDRESSBOOK_VIEW (widget));

	/* XXX We have to keep the addressbook selector informed of the
	 *     current view so it can move contacts via drag-and-drop. */
	e_addressbook_selector_set_current_view (
		E_ADDRESSBOOK_SELECTOR (selector),
		E_ADDRESSBOOK_VIEW (widget));

	view_instance = e_addressbook_view_get_view_instance (view);

	/* This must come after e_book_shell_content_set_current_view()
	 * because book_shell_view_notify_view_id_cb() relies on it. */
	gal_view_instance_load (view_instance);

	view_id = gal_view_instance_get_current_view_id (view_instance);
	e_shell_view_set_view_id (shell_view, view_id);
	g_free (view_id);

	e_addressbook_model_force_folder_bar_message (model);
	selection_change (book_shell_view, view);
}

static gboolean
book_shell_view_show_popup_menu (GdkEventButton *event,
                                 EShellView *shell_view)
{
	const gchar *widget_path;

	widget_path = "/address-book-popup";
	e_shell_view_show_popup_menu (shell_view, widget_path, event);

	return TRUE;
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

static void
book_shell_view_notify_view_id_cb (EBookShellView *book_shell_view)
{
	EBookShellContent *book_shell_content;
	EAddressbookView *address_view;
	GalViewInstance *view_instance;
	const gchar *view_id;

	book_shell_content = book_shell_view->priv->book_shell_content;
	address_view = e_book_shell_content_get_current_view (book_shell_content);
	view_instance = e_addressbook_view_get_view_instance (address_view);
	view_id = e_shell_view_get_view_id (E_SHELL_VIEW (book_shell_view));

	/* A NULL view ID implies we're in a custom view.  But you can
	 * only get to a custom view via the "Define Views" dialog, which
	 * would have already modified the view instance appropriately.
	 * Furthermore, there's no way to refer to a custom view by ID
	 * anyway, since custom views have no IDs. */
	if (view_id == NULL)
		return;

	gal_view_instance_set_current_view_id (view_instance, view_id);
}

void
e_book_shell_view_private_init (EBookShellView *book_shell_view,
                                EShellViewClass *shell_view_class)
{
	EBookShellViewPrivate *priv = book_shell_view->priv;
	GHashTable *uid_to_view;
	GHashTable *uid_to_editor;

	uid_to_view = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_object_unref);

	uid_to_editor = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

	priv->uid_to_view = uid_to_view;
	priv->uid_to_editor = uid_to_editor;
	priv->preview_index = -1;

	if (!gal_view_collection_loaded (shell_view_class->view_collection))
		book_shell_view_load_view_collection (shell_view_class);

	g_signal_connect (
		book_shell_view, "notify::view-id",
		G_CALLBACK (book_shell_view_notify_view_id_cb), NULL);
}

void
e_book_shell_view_private_constructed (EBookShellView *book_shell_view)
{
	EBookShellViewPrivate *priv = book_shell_view->priv;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EShellBackend *shell_backend;
	EShellView *shell_view;
	EShellWindow *shell_window;
	ESourceSelector *selector;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	e_shell_window_add_action_group (shell_window, "contacts");
	e_shell_window_add_action_group (shell_window, "contacts-filter");

	/* Cache these to avoid lots of awkward casting. */
	priv->book_shell_backend = g_object_ref (shell_backend);
	priv->book_shell_content = g_object_ref (shell_content);
	priv->book_shell_sidebar = g_object_ref (shell_sidebar);

	selector = e_book_shell_sidebar_get_selector (
		E_BOOK_SHELL_SIDEBAR (shell_sidebar));

	g_signal_connect_object (
		selector, "button-press-event",
		G_CALLBACK (book_shell_view_selector_button_press_event_cb),
		book_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		selector, "key-press-event",
		G_CALLBACK (book_shell_view_selector_key_press_event_cb),
		book_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		selector, "popup-menu",
		G_CALLBACK (book_shell_view_selector_popup_menu_cb),
		book_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		selector, "primary-selection-changed",
		G_CALLBACK (book_shell_view_activate_selected_source),
		book_shell_view, G_CONNECT_SWAPPED);

	e_categories_add_change_hook (
		(GHookFunc) e_book_shell_view_update_search_filter,
		book_shell_view);

	e_book_shell_view_actions_init (book_shell_view);
	book_shell_view_activate_selected_source (book_shell_view, selector);
	e_book_shell_view_update_search_filter (book_shell_view);
}

void
e_book_shell_view_private_dispose (EBookShellView *book_shell_view)
{
	EBookShellViewPrivate *priv = book_shell_view->priv;

	DISPOSE (priv->book_shell_backend);
	DISPOSE (priv->book_shell_content);
	DISPOSE (priv->book_shell_sidebar);

	g_hash_table_remove_all (priv->uid_to_view);
	g_hash_table_remove_all (priv->uid_to_editor);
}

void
e_book_shell_view_private_finalize (EBookShellView *book_shell_view)
{
	EBookShellViewPrivate *priv = book_shell_view->priv;

	g_hash_table_destroy (priv->uid_to_view);
	g_hash_table_destroy (priv->uid_to_editor);
}

void
e_book_shell_view_editor_weak_notify (EditorUidClosure *closure,
                                      GObject *where_the_object_was)
{
	GHashTable *hash_table;

	hash_table = closure->view->priv->uid_to_editor;
	g_hash_table_remove (hash_table, closure->uid);
}
