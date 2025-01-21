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
#include "addressbook/gui/widgets/gal-view-minicard.h"

#include "e-book-shell-view-private.h"

static gboolean
book_shell_view_cleanup_clicked_source_idle_cb (gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;

	g_return_val_if_fail (E_IS_BOOK_SHELL_VIEW (book_shell_view), FALSE);

	g_clear_object (&book_shell_view->priv->clicked_source);
	g_clear_object (&book_shell_view);

	return FALSE;
}

static void
book_shell_view_popup_menu_hidden_cb (GObject *object,
				      GParamSpec *param,
				      gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;

	g_return_if_fail (E_IS_BOOK_SHELL_VIEW (book_shell_view));

	/* Cannot do the clean up immediately, because the menu is hidden before
	   the action is executed. */
	g_idle_add (book_shell_view_cleanup_clicked_source_idle_cb, book_shell_view);

	g_signal_handlers_disconnect_by_func (object, book_shell_view_popup_menu_hidden_cb, user_data);
}

static GtkWidget *
e_book_shell_view_show_popup_menu (EShellView *shell_view,
				   const gchar *widget_path,
				   GdkEvent *button_event,
				   ESource *clicked_source)
{
	EBookShellView *book_shell_view;
	GtkWidget *menu;

	g_return_val_if_fail (E_IS_BOOK_SHELL_VIEW (shell_view), NULL);
	g_return_val_if_fail (widget_path != NULL, NULL);
	if (clicked_source)
		g_return_val_if_fail (E_IS_SOURCE (clicked_source), NULL);

	book_shell_view = E_BOOK_SHELL_VIEW (shell_view);

	g_clear_object (&book_shell_view->priv->clicked_source);
	if (clicked_source)
		book_shell_view->priv->clicked_source = g_object_ref (clicked_source);

	menu = e_shell_view_show_popup_menu (shell_view, widget_path, button_event);

	if (menu) {
		g_signal_connect (menu, "notify::visible",
			G_CALLBACK (book_shell_view_popup_menu_hidden_cb), g_object_ref (shell_view));
	} else {
		g_clear_object (&book_shell_view->priv->clicked_source);
	}

	return menu;
}

static void
open_contact (EBookShellView *book_shell_view,
              EContact *contact,
              gboolean is_new_contact,
              EAddressbookView *view)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EABEditor *editor;
	EBookClient *book;
	gboolean editable;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	book = e_addressbook_view_get_client (view);
	editable = e_addressbook_view_get_editable (view);

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
popup_event (EShellView *shell_view,
             GdkEvent *button_event)
{
	e_book_shell_view_show_popup_menu (shell_view, "contact-popup", button_event, NULL);
}

static void
selection_change (EBookShellView *book_shell_view,
                  EAddressbookView *view)
{
	EBookShellContent *book_shell_content;
	EAddressbookView *current_view;
	EShellView *shell_view;
	EContact *contact = NULL;

	shell_view = E_SHELL_VIEW (book_shell_view);
	book_shell_content = book_shell_view->priv->book_shell_content;
	current_view = e_book_shell_content_get_current_view (book_shell_content);

	if (view != current_view)
		return;

	if (e_addressbook_view_get_n_selected (view) == 1) {
		GPtrArray *contacts;

		contacts = e_addressbook_view_peek_selected_contacts (view);

		if (contacts) {
			if (contacts->len == 1)
				contact = g_object_ref (g_ptr_array_index (contacts, 0));

			g_ptr_array_unref (contacts);
		}
	}

	e_shell_view_update_actions_in_idle (shell_view);

	e_book_shell_content_set_preview_contact (book_shell_content, contact);

	g_clear_object (&contact);
}

static void
book_shell_view_client_connect_cb (GObject *source_object,
                                   GAsyncResult *result,
                                   gpointer user_data)
{
	EAddressbookView *view = user_data;
	EClient *client;
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

	e_addressbook_view_set_client (view, E_BOOK_CLIENT (client));

exit:
	g_object_unref (view);
}

static void
view_status_message_cb (EAddressbookView *view,
			const gchar *message,
			gint percent,
			gpointer user_data)
{
	EShellView *shell_view = user_data;
	EBookClient *book_client;
	ESource *source;
	ESourceSelector *selector;

	g_return_if_fail (E_IS_BOOK_SHELL_VIEW (shell_view));

	book_client = e_addressbook_view_get_client (view);
	source = e_client_get_source (E_CLIENT (book_client));

	if (!source)
		return;

	selector = e_book_shell_sidebar_get_selector (E_BOOK_SHELL_SIDEBAR (e_shell_view_get_shell_sidebar (shell_view)));

	if (message && *message) {
		gchar *tooltip = NULL;

		if (percent > 0) {
			/* Translators: This is a running activity whose percent complete is known. */
			tooltip = g_strdup_printf (_("%s (%d%% complete)"), message, percent);
		}

		e_source_selector_set_source_is_busy (selector, source, TRUE);
		e_source_selector_set_source_tooltip (selector, source, tooltip ? tooltip : message);

		g_free (tooltip);
	} else {
		e_source_selector_set_source_is_busy (selector, source, FALSE);
		e_source_selector_set_source_tooltip (selector, source, NULL);
	}
}

static void
book_shell_view_activate_selected_source (EBookShellView *book_shell_view,
                                          ESourceSelector *selector)
{
	EShellView *shell_view;
	EBookShellContent *book_shell_content;
	EAddressbookView *view;
	ESource *source;
	GalViewInstance *view_instance;
	GHashTable *hash_table;
	GtkWidget *widget;
	const gchar *uid;
	gchar *selected_category;
	gchar *view_id;

	shell_view = E_SHELL_VIEW (book_shell_view);

	book_shell_content = book_shell_view->priv->book_shell_content;
	source = e_source_selector_ref_primary_selection (selector);

	if (source == NULL)
		return;

	selected_category = e_addressbook_selector_dup_selected_category (
		E_ADDRESSBOOK_SELECTOR (selector));

	uid = e_source_get_uid (source);

	if (g_strcmp0 (book_shell_view->priv->selected_source_uid, uid) == 0) {
		if (!selected_category || !*selected_category)
			e_shell_view_execute_search (shell_view);

		g_free (selected_category);
		g_object_unref (source);
		return;
	}

	g_clear_pointer (&book_shell_view->priv->selected_source_uid, g_free);
	book_shell_view->priv->selected_source_uid = g_strdup (uid);

	hash_table = book_shell_view->priv->uid_to_view;
	widget = g_hash_table_lookup (hash_table, uid);

	if (widget != NULL) {
		view = E_ADDRESSBOOK_VIEW (widget);
	} else {
		/* Create a view for this UID. */
		widget = e_addressbook_view_new (shell_view, source);
		gtk_widget_show (widget);

		/* Default searching options for a new view. */
		e_addressbook_view_set_search (
			E_ADDRESSBOOK_VIEW (widget),
			NULL, CONTACT_FILTER_ANY_CATEGORY,
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
			G_CALLBACK (e_shell_view_update_actions_in_idle),
			book_shell_view, G_CONNECT_SWAPPED);

		g_signal_connect_object (
			widget, "selection-change", G_CALLBACK (selection_change),
			book_shell_view, G_CONNECT_SWAPPED);

		g_signal_connect_object (
			widget, "status-message",
			G_CALLBACK (view_status_message_cb), book_shell_view, 0);

		view = E_ADDRESSBOOK_VIEW (widget);
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

	e_addressbook_view_force_folder_bar_message (view);
	selection_change (book_shell_view, view);

	/* Make sure change to no-category re-filters the view, like when switching
	   from another book to the source node, not the source child node. */
	if (!selected_category || !*selected_category)
		e_shell_view_execute_search (shell_view);

	g_free (selected_category);
	g_object_unref (source);
}

static gboolean
book_shell_view_selector_popup_event_cb (EShellView *shell_view,
                                         ESource *clicked_source,
                                         GdkEvent *button_event)
{
	e_book_shell_view_show_popup_menu (shell_view, "address-book-popup", button_event, clicked_source);

	return TRUE;
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
	EShellView *shell_view;
	EBookShellContent *book_shell_content;
	EAddressbookView *address_view;
	GalViewInstance *view_instance;
	GalView *gl_view;
	EUIAction *action;
	const gchar *view_id;

	shell_view = E_SHELL_VIEW (book_shell_view);
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

	gl_view = gal_view_instance_get_current_view (view_instance);

	action = ACTION (CONTACT_CARDS_SORT_BY_MENU);
	e_ui_action_set_visible (action, GAL_IS_VIEW_MINICARD (gl_view));
	e_ui_action_set_sensitive (action, e_ui_action_get_visible (action));

	if (GAL_IS_VIEW_MINICARD (gl_view)) {
		action = ACTION (CONTACT_CARDS_SORT_BY_FILE_AS);
		e_ui_action_set_state (action, g_variant_new_int32 (gal_view_minicard_get_sort_by (GAL_VIEW_MINICARD (gl_view))));
	}
}

static void
book_shell_view_contact_view_notify_state_cb (GObject *object,
					      GParamSpec *param,
					      gpointer user_data)
{
	GAction *action = G_ACTION (object);
	EBookShellView *self = user_data;
	EBookShellContent *book_shell_content;
	GVariant *state;
	GtkOrientable *orientable;
	GtkOrientation orientation;

	state = g_action_get_state (action);

	book_shell_content = self->priv->book_shell_content;
	orientable = GTK_ORIENTABLE (book_shell_content);

	switch (g_variant_get_int32 (state)) {
		case 0:
			orientation = GTK_ORIENTATION_VERTICAL;
			break;
		case 1:
			orientation = GTK_ORIENTATION_HORIZONTAL;
			break;
		default:
			g_return_if_reached ();
	}

	gtk_orientable_set_orientation (orientable, orientation);

	g_clear_pointer (&state, g_variant_unref);
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
	EShellSearchbar *searchbar;
	ESourceSelector *selector;
	EPreviewPane *preview_pane;
	EWebView *web_view;
	EUIAction *action;
	GSettings *settings;
	gulong handler_id;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	/* Cache these to avoid lots of awkward casting. */
	priv->book_shell_backend = E_BOOK_SHELL_BACKEND (g_object_ref (shell_backend));
	priv->book_shell_content = E_BOOK_SHELL_CONTENT (g_object_ref (shell_content));
	priv->book_shell_sidebar = E_BOOK_SHELL_SIDEBAR (g_object_ref (shell_sidebar));

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
		selector, "popup-event",
		G_CALLBACK (book_shell_view_selector_popup_event_cb),
		book_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		selector, "primary-selection-changed",
		G_CALLBACK (book_shell_view_activate_selected_source),
		book_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		selector, "source-child-selected",
		G_CALLBACK (e_shell_view_execute_search),
		book_shell_view, G_CONNECT_SWAPPED);

	e_categories_add_change_hook (
		(GHookFunc) e_book_shell_view_update_search_filter,
		book_shell_view);

	preview_pane = e_book_shell_content_get_preview_pane (book_shell_view->priv->book_shell_content);
	web_view = e_preview_pane_get_web_view (preview_pane);
	e_web_view_set_open_proxy (web_view, ACTION (CONTACT_OPEN));
	e_web_view_set_print_proxy (web_view, ACTION (CONTACT_PRINT));
	e_web_view_set_save_as_proxy (web_view, ACTION (CONTACT_SAVE_AS));

	/* Advanced Search Action */
	action = ACTION (CONTACT_SEARCH_ADVANCED_HIDDEN);
	e_ui_action_set_visible (action, FALSE);
	searchbar = e_book_shell_content_get_searchbar (book_shell_view->priv->book_shell_content);
	e_shell_searchbar_set_search_option (searchbar, action);

	/* Bind GObject properties to GSettings keys. */

	settings = e_util_ref_settings ("org.gnome.evolution.addressbook");

	g_settings_bind (
		settings, "preview-show-maps",
		ACTION (CONTACT_PREVIEW_SHOW_MAPS), "active",
		G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);

	action = ACTION (CONTACT_PREVIEW);

	g_settings_bind (
		settings, "show-preview",
		action, "active",
		G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);

	e_binding_bind_property (
		action, "active",
		book_shell_view->priv->book_shell_content, "preview-visible",
		G_BINDING_SYNC_CREATE);

	/* use the "classic" action, because it's the first in the group and
	   the group is not set yet, due to the UI manager being frozen */
	action = ACTION (CONTACT_VIEW_CLASSIC);

	g_settings_bind_with_mapping (
		settings, "layout",
		action, "state",
		G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY,
		e_shell_view_util_layout_to_state_cb,
		e_shell_view_util_state_to_layout_cb, NULL, NULL);

	g_object_unref (settings);

	g_signal_connect_object (action, "notify::state",
		G_CALLBACK (book_shell_view_contact_view_notify_state_cb), book_shell_view, 0);

	/* to propagate the loaded state */
	book_shell_view_contact_view_notify_state_cb (G_OBJECT (action), NULL, book_shell_view);

	e_shell_view_block_execute_search (shell_view);
	book_shell_view_activate_selected_source (book_shell_view, selector);
	e_shell_view_unblock_execute_search (shell_view);
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

	g_clear_object (&priv->clicked_source);
	g_clear_object (&priv->client_cache);
	g_clear_object (&priv->registry);

	g_hash_table_remove_all (priv->uid_to_view);
}

void
e_book_shell_view_private_finalize (EBookShellView *book_shell_view)
{
	EBookShellViewPrivate *priv = book_shell_view->priv;

	g_clear_pointer (&priv->selected_source_uid, g_free);
	g_hash_table_destroy (priv->uid_to_view);
}
