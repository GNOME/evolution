/*
 * e-book-shell-view-private.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-util/e-util-private.h"

#include "e-book-shell-view-private.h"

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
	EBookClient *book;
	gboolean editable;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	model = e_addressbook_view_get_model (view);
	book = e_addressbook_model_get_client (model);
	editable = e_addressbook_model_get_editable (model);

	if (e_contact_get (contact, E_CONTACT_IS_LIST))
		editor = e_contact_list_editor_new (
			shell, book, contact, is_new_contact, editable);
	else
		editor = e_contact_editor_new (
			shell, book, contact, is_new_contact, editable);

	gtk_window_set_transient_for (eab_editor_get_window (editor), GTK_WINDOW (shell_window));

	eab_editor_show (editor);
}

static void
popup_event (EBookShellView *book_shell_view,
             GdkEvent *button_event)
{
	EShellView *shell_view;
	const gchar *widget_path;

	widget_path = "/contact-popup";
	shell_view = E_SHELL_VIEW (book_shell_view);

	e_shell_view_show_popup_menu (shell_view, widget_path, button_event);
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

	if (contact)
		g_object_unref (contact);
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
model_query_changed_cb (EBookShellView *book_shell_view,
                        GParamSpec *param,
                        EAddressbookModel *model)
{
	EBookShellContent *book_shell_content;
	EAddressbookView *current_view;

	book_shell_content = book_shell_view->priv->book_shell_content;
	current_view = e_book_shell_content_get_current_view (book_shell_content);

	if (!current_view || e_addressbook_view_get_model (current_view) != model)
		return;

	/* clear the contact preview when model's query changed */
	e_book_shell_content_set_preview_contact (book_shell_content, NULL);
	book_shell_view->priv->preview_index = -1;
}

static void
book_shell_view_client_connect_cb (GObject *source_object,
                                   GAsyncResult *result,
                                   gpointer user_data)
{
	EAddressbookView *view = user_data;
	EClient *client;
	EAddressbookModel *model;
	GError *error = NULL;

	client = e_client_selector_get_client_finish (
		E_CLIENT_SELECTOR (source_object), result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);
		goto exit;

	} else if (error != NULL) {
		EShellView *shell_view;
		EShellContent *shell_content;
		EAlertSink *alert_sink;
		ESource *source;

		source = e_addressbook_view_get_source (view);
		shell_view = e_addressbook_view_get_shell_view (view);
		shell_content = e_shell_view_get_shell_content (shell_view);
		alert_sink = E_ALERT_SINK (shell_content);

		eab_load_error_dialog (NULL, alert_sink, source, error);

		g_error_free (error);
		goto exit;
	}

	model = e_addressbook_view_get_model (view);
	e_addressbook_model_set_client (model, E_BOOK_CLIENT (client));
	e_addressbook_model_force_folder_bar_message (model);

exit:
	g_object_unref (view);
}

static void
book_shell_view_activate_selected_source (EBookShellView *book_shell_view,
                                          ESourceSelector *selector)
{
	EShellView *shell_view;
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

	book_shell_content = book_shell_view->priv->book_shell_content;
	source = e_source_selector_ref_primary_selection (selector);

	if (source == NULL)
		return;

	uid = e_source_get_uid (source);
	hash_table = book_shell_view->priv->uid_to_view;
	widget = g_hash_table_lookup (hash_table, uid);

	if (widget != NULL) {
		view = E_ADDRESSBOOK_VIEW (widget);
		model = e_addressbook_view_get_model (view);

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

		g_signal_connect_object (
			model, "contact-changed",
			G_CALLBACK (contact_changed),
			book_shell_view, G_CONNECT_SWAPPED);

		g_signal_connect_object (
			model, "contacts-removed",
			G_CALLBACK (contacts_removed),
			book_shell_view, G_CONNECT_SWAPPED);

		e_signal_connect_notify_object (
			model, "notify::query",
			G_CALLBACK (model_query_changed_cb),
			book_shell_view, G_CONNECT_SWAPPED);
	}

	/* XXX No way to cancel this? */
	e_client_selector_get_client (
		E_CLIENT_SELECTOR (selector),
		source, TRUE, (guint32) -1, NULL,
		book_shell_view_client_connect_cb,
		g_object_ref (view));

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

	g_object_unref (source);
}

static gboolean
book_shell_view_show_popup_menu (GdkEvent *button_event,
                                 EShellView *shell_view)
{
	const gchar *widget_path;

	widget_path = "/address-book-popup";
	e_shell_view_show_popup_menu (shell_view, widget_path, button_event);

	return TRUE;
}

static gboolean
book_shell_view_selector_button_press_event_cb (EShellView *shell_view,
                                                GdkEvent *button_event)
{
	guint event_button = 0;

	/* XXX Use ESourceSelector's "popup-event" signal instead. */

	gdk_event_get_button (button_event, &event_button);

	if (button_event->type != GDK_BUTTON_PRESS || event_button != 3)
		return FALSE;

	return book_shell_view_show_popup_menu (button_event, shell_view);
}

static gboolean
book_shell_view_selector_popup_menu_cb (EShellView *shell_view)
{
	/* XXX Use ESourceSelector's "popup-event" signal instead. */

	return book_shell_view_show_popup_menu (NULL, shell_view);
}

static void
book_shell_view_backend_error_cb (EClientCache *client_cache,
                                  EClient *client,
                                  EAlert *alert,
                                  EBookShellView *book_shell_view)
{
	EBookShellContent *book_shell_content;
	ESource *source;
	const gchar *extension_name;

	book_shell_content = book_shell_view->priv->book_shell_content;

	source = e_client_get_source (client);
	extension_name = E_SOURCE_EXTENSION_ADDRESS_BOOK;

	/* Only submit alerts from address book backends. */
	if (e_source_has_extension (source, extension_name)) {
		EAlertSink *alert_sink;

		alert_sink = E_ALERT_SINK (book_shell_content);
		e_alert_sink_submit_alert (alert_sink, alert);
	}
}

static void
book_shell_view_source_removed_cb (ESourceRegistry *registry,
                                   ESource *source,
                                   EBookShellView *book_shell_view)
{
	EBookShellViewPrivate *priv = book_shell_view->priv;
	EBookShellContent *book_shell_content;
	EAddressbookView *view;
	const gchar *uid;

	uid = e_source_get_uid (source);

	book_shell_content = book_shell_view->priv->book_shell_content;

	/* Remove the EAddressbookView for the deleted source. */
	view = g_hash_table_lookup (priv->uid_to_view, uid);
	if (view != NULL) {
		e_book_shell_content_remove_view (book_shell_content, view);
		g_hash_table_remove (priv->uid_to_view, uid);
	}

	e_shell_view_update_actions (E_SHELL_VIEW (book_shell_view));
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
e_book_shell_view_private_init (EBookShellView *book_shell_view)
{
	EBookShellViewPrivate *priv = book_shell_view->priv;
	GHashTable *uid_to_view;

	uid_to_view = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_object_unref);

	priv->uid_to_view = uid_to_view;
	priv->preview_index = -1;

	e_signal_connect_notify (
		book_shell_view, "notify::view-id",
		G_CALLBACK (book_shell_view_notify_view_id_cb), NULL);
}

void
e_book_shell_view_private_constructed (EBookShellView *book_shell_view)
{
	EBookShellViewPrivate *priv = book_shell_view->priv;
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EShellBackend *shell_backend;
	ESourceSelector *selector;
	gulong handler_id;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	e_shell_window_add_action_group (shell_window, "contacts");
	e_shell_window_add_action_group (shell_window, "contacts-filter");

	/* Cache these to avoid lots of awkward casting. */
	priv->book_shell_backend = g_object_ref (shell_backend);
	priv->book_shell_content = g_object_ref (shell_content);
	priv->book_shell_sidebar = g_object_ref (shell_sidebar);

	/* Keep our own reference to this so we can
	 * disconnect our signal handler in dispose(). */
	priv->client_cache = g_object_ref (e_shell_get_client_cache (shell));

	/* Keep our own reference to this so we can
	 * disconnect our signal handler in dispose(). */
	priv->registry = g_object_ref (e_shell_get_registry (shell));

	selector = e_book_shell_sidebar_get_selector (
		E_BOOK_SHELL_SIDEBAR (shell_sidebar));

	handler_id = g_signal_connect (
		priv->client_cache, "backend-error",
		G_CALLBACK (book_shell_view_backend_error_cb),
		book_shell_view);
	priv->backend_error_handler_id = handler_id;

	handler_id = g_signal_connect (
		priv->registry, "source-removed",
		G_CALLBACK (book_shell_view_source_removed_cb),
		book_shell_view);
	priv->source_removed_handler_id = handler_id;

	g_signal_connect_object (
		selector, "button-press-event",
		G_CALLBACK (book_shell_view_selector_button_press_event_cb),
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

	if (priv->backend_error_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->client_cache,
			priv->backend_error_handler_id);
		priv->backend_error_handler_id = 0;
	}

	if (priv->source_removed_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->registry,
			priv->source_removed_handler_id);
		priv->source_removed_handler_id = 0;
	}

	g_clear_object (&priv->book_shell_backend);
	g_clear_object (&priv->book_shell_content);
	g_clear_object (&priv->book_shell_sidebar);

	g_clear_object (&priv->client_cache);
	g_clear_object (&priv->registry);

	g_hash_table_remove_all (priv->uid_to_view);
}

void
e_book_shell_view_private_finalize (EBookShellView *book_shell_view)
{
	EBookShellViewPrivate *priv = book_shell_view->priv;

	g_hash_table_destroy (priv->uid_to_view);
}
