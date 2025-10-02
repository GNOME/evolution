/*
 * e-book-shell-view-actions.c
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

#include "e-book-shell-view-private.h"
#include "addressbook/gui/widgets/e-bulk-edit-contacts.h"
#include "addressbook/gui/widgets/gal-view-minicard.h"

#include <e-util/e-util.h>

static void
action_address_book_copy_cb (EUIAction *action,
			     GVariant *parameter,
			     gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;
	EBookShellContent *book_shell_content;
	EAddressbookView *view;

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	e_addressbook_view_copy_to_folder (view, TRUE);
}

static void
action_address_book_delete_cb (EUIAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EBookShellSidebar *book_shell_sidebar;
	ESource *source;
	ESourceSelector *selector;
	gint response;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	book_shell_sidebar = book_shell_view->priv->book_shell_sidebar;
	selector = e_book_shell_sidebar_get_selector (book_shell_sidebar);

	source = e_source_selector_ref_primary_selection (selector);
	g_return_if_fail (source != NULL);

	if (e_source_get_remote_deletable (source)) {
		response = e_alert_run_dialog_for_args (
			GTK_WINDOW (shell_window),
			"addressbook:ask-delete-remote-addressbook",
			e_source_get_display_name (source), NULL);

		if (response == GTK_RESPONSE_YES)
			e_shell_view_remote_delete_source (shell_view, source);

	} else {
		response = e_alert_run_dialog_for_args (
			GTK_WINDOW (shell_window),
			"addressbook:ask-delete-addressbook",
			e_source_get_display_name (source), NULL);

		if (response == GTK_RESPONSE_YES)
			e_shell_view_remove_source (shell_view, source);
	}

	g_object_unref (source);
}

static void
action_address_book_manage_groups_cb (EUIAction *action,
				      GVariant *parameter,
				      gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;
	EShellView *shell_view;
	ESourceSelector *selector;

	shell_view = E_SHELL_VIEW (book_shell_view);
	selector = e_book_shell_sidebar_get_selector (book_shell_view->priv->book_shell_sidebar);

	if (e_source_selector_manage_groups (selector) &&
	    e_source_selector_save_groups_setup (selector, e_shell_view_get_state_key_file (shell_view)))
		e_shell_view_set_state_dirty (shell_view);
}

static void
action_address_book_move_cb (EUIAction *action,
			     GVariant *parameter,
			     gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;
	EBookShellContent *book_shell_content;
	EAddressbookView *view;

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	e_addressbook_view_move_to_folder (view, TRUE);
}

static void
action_address_book_new_cb (EUIAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;
	EShellView *shell_view;
	EShellWindow *shell_window;
	ESourceRegistry *registry;
	GtkWidget *config;
	GtkWidget *dialog;
	const gchar *icon_name;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	registry = book_shell_view->priv->registry;
	config = e_book_source_config_new (registry, NULL);

	e_book_shell_view_preselect_source_config (shell_view, config);

	dialog = e_source_config_dialog_new (E_SOURCE_CONFIG (config));

	gtk_window_set_transient_for (
		GTK_WINDOW (dialog), GTK_WINDOW (shell_window));

	icon_name = e_ui_action_get_icon_name (action);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);

	gtk_window_set_title (GTK_WINDOW (dialog), _("New Address Book"));

	gtk_widget_show (dialog);
}

static void
action_address_book_print_cb (EUIAction *action,
			      GVariant *parameter,
			      gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;
	EBookShellContent *book_shell_content;
	EAddressbookView *view;
	GtkPrintOperationAction print_action;

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	print_action = GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG;
	e_addressbook_view_print (view, FALSE, print_action);
}

static void
action_address_book_print_preview_cb (EUIAction *action,
				      GVariant *parameter,
				      gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;
	EBookShellContent *book_shell_content;
	EAddressbookView *view;
	GtkPrintOperationAction print_action;

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	print_action = GTK_PRINT_OPERATION_ACTION_PREVIEW;
	e_addressbook_view_print (view, FALSE, print_action);
}

static void
action_address_book_properties_cb (EUIAction *action,
				   GVariant *parameter,
				   gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EBookShellSidebar *book_shell_sidebar;
	ESource *source;
	ESourceSelector *selector;
	ESourceRegistry *registry;
	GtkWidget *config;
	GtkWidget *dialog;
	const gchar *icon_name;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	book_shell_sidebar = book_shell_view->priv->book_shell_sidebar;
	selector = e_book_shell_sidebar_get_selector (book_shell_sidebar);
	source = e_source_selector_ref_primary_selection (selector);
	g_return_if_fail (source != NULL);

	registry = e_source_selector_get_registry (selector);
	config = e_book_source_config_new (registry, source);

	g_object_unref (source);

	dialog = e_source_config_dialog_new (E_SOURCE_CONFIG (config));

	gtk_window_set_transient_for (
		GTK_WINDOW (dialog), GTK_WINDOW (shell_window));

	icon_name = e_ui_action_get_icon_name (action);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);

	gtk_window_set_title (
		GTK_WINDOW (dialog), _("Address Book Properties"));

	gtk_widget_show (dialog);
}

static void
address_book_refresh_done_cb (GObject *source_object,
                              GAsyncResult *result,
                              gpointer user_data)
{
	EClient *client;
	ESource *source;
	EActivity *activity;
	EAlertSink *alert_sink;
	const gchar *display_name;
	GError *local_error = NULL;

	g_return_if_fail (E_IS_CLIENT (source_object));

	client = E_CLIENT (source_object);
	source = e_client_get_source (client);
	activity = user_data;

	e_client_refresh_finish (client, result, &local_error);

	alert_sink = e_activity_get_alert_sink (activity);
	display_name = e_source_get_display_name (source);

	if (e_activity_handle_cancellation (activity, local_error)) {
		/* nothing to do */

	} else if (local_error != NULL) {
		e_alert_submit (
			alert_sink,
			"addressbook:refresh-error",
			display_name, local_error->message, NULL);

	} else {
		e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
	}

	g_clear_object (&activity);
	g_clear_error (&local_error);
}

static void
action_address_book_refresh_cb (EUIAction *action,
				GVariant *parameter,
				gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;
	EBookShellSidebar *book_shell_sidebar;
	ESourceSelector *selector;
	EClient *client = NULL;
	ESource *source;
	EActivity *activity;
	EAlertSink *alert_sink;
	EShellBackend *shell_backend;
	EShellContent *shell_content;
	EShellView *shell_view;
	EShell *shell;
	GCancellable *cancellable;

	book_shell_sidebar = book_shell_view->priv->book_shell_sidebar;
	selector = e_book_shell_sidebar_get_selector (book_shell_sidebar);

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell = e_shell_backend_get_shell (shell_backend);

	source = e_source_selector_ref_primary_selection (selector);

	if (source != NULL) {
		client = e_client_selector_ref_cached_client (
			E_CLIENT_SELECTOR (selector), source);
		if (!client) {
			ESource *primary;

			e_shell_allow_auth_prompt_for (shell, source);

			primary = e_source_selector_ref_primary_selection (selector);
			if (primary == source)
				e_source_selector_set_primary_selection (selector, source);

			g_clear_object (&primary);
		}

		g_object_unref (source);
	}

	if (client == NULL)
		return;

	g_return_if_fail (e_client_check_refresh_supported (client));

	alert_sink = E_ALERT_SINK (shell_content);
	activity = e_activity_new ();
	cancellable = g_cancellable_new ();

	e_activity_set_alert_sink (activity, alert_sink);
	e_activity_set_cancellable (activity, cancellable);

	e_shell_allow_auth_prompt_for (shell, source);

	e_client_refresh (client, cancellable, address_book_refresh_done_cb, activity);

	e_shell_backend_add_activity (shell_backend, activity);

	g_object_unref (cancellable);
	g_object_unref (client);
}

static void
book_shell_view_refresh_backend_done_cb (GObject *source_object,
					 GAsyncResult *result,
					 gpointer user_data)
{
	ESourceRegistry *registry;
	EActivity *activity = user_data;
	EAlertSink *alert_sink;
	GError *local_error = NULL;

	g_return_if_fail (E_IS_SOURCE_REGISTRY (source_object));

	registry = E_SOURCE_REGISTRY (source_object);
	alert_sink = e_activity_get_alert_sink (activity);

	e_source_registry_refresh_backend_finish (registry, result, &local_error);

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_error_free (local_error);

	} else if (local_error != NULL) {
		e_alert_submit (alert_sink, "addressbook:refresh-backend-failed", local_error->message, NULL);
		g_error_free (local_error);

	} else {
		e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
	}

	g_clear_object (&activity);
}

static void
action_address_book_refresh_backend_cb (EUIAction *action,
					GVariant *parameter,
					gpointer user_data)
{
	EShellView *shell_view = user_data;
	EShellBackend *shell_backend;
	EShellContent *shell_content;
	EShell *shell;
	ESource *source;
	EActivity *activity;
	EAlertSink *alert_sink;
	ESourceRegistry *registry;
	GCancellable *cancellable;

	g_return_if_fail (E_IS_BOOK_SHELL_VIEW (shell_view));

	source = e_book_shell_view_get_clicked_source (shell_view);
	if (!source || !e_source_has_extension (source, E_SOURCE_EXTENSION_COLLECTION))
		return;

	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell = e_shell_backend_get_shell (shell_backend);

	alert_sink = E_ALERT_SINK (shell_content);
	activity = e_activity_new ();
	cancellable = g_cancellable_new ();

	e_activity_set_alert_sink (activity, alert_sink);
	e_activity_set_cancellable (activity, cancellable);

	registry = e_shell_get_registry (shell);

	e_source_registry_refresh_backend (registry, e_source_get_uid (source), cancellable,
		book_shell_view_refresh_backend_done_cb, activity);

	e_shell_backend_add_activity (shell_backend, activity);

	g_object_unref (cancellable);
}

#ifdef ENABLE_CONTACT_MAPS
static void
contact_editor_contact_modified_cb (EABEditor *editor,
                                    const GError *error,
                                    EContact *contact,
                                    gpointer user_data)
{
	EContactMapWindow *window = user_data;
	EContactMap *map;
	const gchar *contact_uid;

	if (error != NULL) {
		g_warning ("Error modifying contact: %s", error->message);
		return;
	}

	map = e_contact_map_window_get_map (window);

	contact_uid = e_contact_get_const (contact, E_CONTACT_UID);

	e_contact_map_remove_contact (map, contact_uid);
	e_contact_map_add_contact (map, contact);
}

static void
map_window_show_contact_editor_cb (EContactMapWindow *window,
                                   const gchar *contact_uid,
                                   gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;
	EBookShellSidebar *book_shell_sidebar;
	EShell *shell;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	ESource *source;
	ESourceSelector *selector;
	EClient *client;
	EClientCache *client_cache;
	EContact *contact;
	EABEditor *editor;
	GError *error = NULL;

	book_shell_sidebar = book_shell_view->priv->book_shell_sidebar;
	selector = e_book_shell_sidebar_get_selector (book_shell_sidebar);
	source = e_source_selector_ref_primary_selection (selector);
	g_return_if_fail (source != NULL);

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell = e_shell_backend_get_shell (shell_backend);
	client_cache = e_shell_get_client_cache (shell);

	/* FIXME This blocks.  Needs to be asynchronous. */
	client = e_client_cache_get_client_sync (
		client_cache, source,
		E_SOURCE_EXTENSION_ADDRESS_BOOK, (guint32) -1,
		NULL, &error);

	g_object_unref (source);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (error != NULL) {
		g_warning ("Error loading addressbook: %s", error->message);
		g_error_free (error);
		return;
	}

	e_book_client_get_contact_sync (
		E_BOOK_CLIENT (client), contact_uid, &contact, NULL, &error);
	if (error != NULL) {
		g_warning ("Error getting contact from addressbook: %s", error->message);
		g_error_free (error);
		g_object_unref (client);
		return;
	}

	editor = e_contact_editor_new (
		shell, E_BOOK_CLIENT (client), contact, FALSE, TRUE);
	gtk_window_set_transient_for (eab_editor_get_window (editor), GTK_WINDOW (window));

	g_signal_connect (
		editor, "contact-modified",
		G_CALLBACK (contact_editor_contact_modified_cb), window);

	eab_editor_show (editor);
	g_object_unref (client);
}
#endif

/* We need this function to he defined all the time. */
static void
action_address_book_map_cb (EUIAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
#ifdef ENABLE_CONTACT_MAPS
	EBookShellView *book_shell_view = user_data;
	EShell *shell;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EContactMapWindow *map_window;
	EBookShellSidebar *book_shell_sidebar;
	ESource *source;
	ESourceSelector *selector;
	EClient *client;
	EClientCache *client_cache;
	GError *error = NULL;

	book_shell_sidebar = book_shell_view->priv->book_shell_sidebar;
	selector = e_book_shell_sidebar_get_selector (book_shell_sidebar);
	source = e_source_selector_ref_primary_selection (selector);
	g_return_if_fail (source != NULL);

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell = e_shell_backend_get_shell (shell_backend);
	client_cache = e_shell_get_client_cache (shell);

	/* FIXME This blocks.  Needs to be asynchronous. */
	client = e_client_cache_get_client_sync (
		client_cache, source,
		E_SOURCE_EXTENSION_ADDRESS_BOOK, (guint32) -1,
		NULL, &error);

	g_object_unref (source);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (error != NULL) {
		g_warning ("Error loading addressbook: %s", error->message);
		g_error_free (error);
		return;
	}

	map_window = e_contact_map_window_new ();
	e_contact_map_window_load_addressbook (
		map_window, E_BOOK_CLIENT (client));

	/* Free the map_window automatically when it is closed */
	g_signal_connect_swapped (
		map_window, "hide",
		G_CALLBACK (gtk_widget_destroy), GTK_WIDGET (map_window));
	g_signal_connect (
		map_window, "show-contact-editor",
		G_CALLBACK (map_window_show_contact_editor_cb), book_shell_view);

	gtk_widget_show_all (GTK_WIDGET (map_window));

	g_object_unref (client);
#endif
}

static void
action_address_book_rename_cb (EUIAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;
	EBookShellSidebar *book_shell_sidebar;
	ESourceSelector *selector;

	book_shell_sidebar = book_shell_view->priv->book_shell_sidebar;
	selector = e_book_shell_sidebar_get_selector (book_shell_sidebar);

	e_source_selector_edit_primary_selection (selector);
}

static void
action_address_book_save_as_cb (EUIAction *action,
				GVariant *parameter,
				gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellBackend *shell_backend;
	EBookShellContent *book_shell_content;
	EAddressbookView *view;
	EActivity *activity;
	EBookQuery *query;
	EBookClient *book;
	GSList *list = NULL;
	GFile *file;
	gchar *string;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	book = e_addressbook_view_get_client (view);

	query = e_book_query_any_field_contains ("");
	string = e_book_query_to_string (query);
	e_book_query_unref (query);

	e_book_client_get_contacts_sync (book, string, &list, NULL, NULL);
	g_free (string);

	if (list == NULL)
		goto exit;

	string = eab_suggest_filename ((!list || list->next) ? NULL : list->data);
	file = e_shell_run_save_dialog (
		/* Translators: This is a save dialog title */
		shell, _("Save as vCard"), string,
		"*.vcf:text/x-vcard,text/directory", NULL, NULL);
	g_free (string);

	if (file == NULL)
		goto exit;

	string = eab_contact_list_to_string (list);
	if (string == NULL) {
		g_warning ("Could not convert contact list to a string");
		g_object_unref (file);
		goto exit;
	}

	/* XXX No callback means errors are discarded.
	 *
	 *     There's an EAlert for this which I'm not using
	 *     until I figure out a better way to display errors:
	 *
	 *     "addressbook:save-error"
	 */
	activity = e_file_replace_contents_async (
		file, string, strlen (string), NULL, FALSE,
		G_FILE_CREATE_NONE, (GAsyncReadyCallback) NULL, NULL);
	e_shell_backend_add_activity (shell_backend, activity);

	/* Free the string when the activity is finalized. */
	g_object_set_data_full (
		G_OBJECT (activity),
		"file-content", string,
		(GDestroyNotify) g_free);

	g_object_unref (file);

exit:
	g_slist_free_full (list, (GDestroyNotify) g_object_unref);
}

static void
action_address_book_stop_cb (EUIAction *action,
			     GVariant *parameter,
			     gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;
	EBookShellContent *book_shell_content;
	EAddressbookView *view;

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	e_addressbook_view_stop (view);
}

typedef struct _RetrieveSelectedData {
	EShellBackend *shell_backend;
	EShellView *shell_view;
	EShell *shell;
	EActivity *activity;
} RetrieveSelectedData;

static RetrieveSelectedData *
retrieve_selected_data_new (EShellView *shell_view)
{
	RetrieveSelectedData *rsd;
	EActivity *activity;
	GCancellable *cancellable;

	activity = e_activity_new ();
	cancellable = camel_operation_new ();

	e_activity_set_alert_sink (activity, E_ALERT_SINK (e_shell_view_get_shell_content (shell_view)));
	e_activity_set_cancellable (activity, cancellable);
	e_activity_set_text (activity, _("Retrieving selected contacts…"));

	camel_operation_push_message (cancellable, "%s", e_activity_get_text (activity));

	e_shell_backend_add_activity (e_shell_view_get_shell_backend (shell_view), activity);

	rsd = g_new0 (RetrieveSelectedData, 1);
	rsd->shell_backend = g_object_ref (e_shell_view_get_shell_backend (shell_view));
	rsd->shell_view = g_object_ref (shell_view);
	rsd->shell = g_object_ref (e_shell_backend_get_shell (rsd->shell_backend));
	rsd->activity = activity;

	g_object_unref (cancellable);

	return rsd;
}

static void
retrieve_selected_data_free (RetrieveSelectedData *rsd)
{
	if (rsd) {
		g_clear_object (&rsd->shell_backend);
		g_clear_object (&rsd->shell_view);
		g_clear_object (&rsd->shell);
		g_free (rsd);
	}
}

static void
action_contact_bulk_edit_run (EShellView *shell_view,
			      EBookClient *book_client,
			      GPtrArray *contacts)
{
	GtkWidget *bulk_edit;

	bulk_edit = e_bulk_edit_contacts_new (GTK_WINDOW (e_shell_view_get_shell_window (shell_view)), book_client, contacts);

	gtk_widget_show (bulk_edit);
}

static void
action_contact_bulk_edit_got_selected_cb (GObject *source_object,
					  GAsyncResult *result,
					  gpointer user_data)
{
	RetrieveSelectedData *rsd = user_data;
	EAddressbookView *view = E_ADDRESSBOOK_VIEW (source_object);
	GPtrArray *contacts;
	GError *error = NULL;

	g_return_if_fail (rsd != NULL);

	contacts = e_addressbook_view_dup_selected_contacts_finish (view, result, &error);
	if (contacts) {
		e_activity_set_state (rsd->activity, E_ACTIVITY_COMPLETED);

		action_contact_bulk_edit_run (rsd->shell_view, e_addressbook_view_get_client (view), contacts);
	} else if (!e_activity_handle_cancellation (rsd->activity, error)) {
		g_warning ("%s: Failed to retrieve selected contacts: %s", G_STRFUNC, error ? error->message : "Unknown error");
		e_activity_set_state (rsd->activity, E_ACTIVITY_COMPLETED);
	}

	g_clear_pointer (&contacts, g_ptr_array_unref);
	g_clear_error (&error);

	retrieve_selected_data_free (rsd);
}

static void
action_contact_bulk_edit_cb (EUIAction *action,
			     GVariant *parameter,
			     gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;
	EShellView *shell_view;
	EBookShellContent *book_shell_content;
	EAddressbookView *view;
	GPtrArray *contacts;

	shell_view = E_SHELL_VIEW (book_shell_view);

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	contacts = e_addressbook_view_peek_selected_contacts (view);
	if (!contacts) {
		RetrieveSelectedData *rsd;

		rsd = retrieve_selected_data_new (shell_view);

		e_addressbook_view_dup_selected_contacts (view, e_activity_get_cancellable (rsd->activity),
			action_contact_bulk_edit_got_selected_cb, rsd);

		return;
	}

	action_contact_bulk_edit_run (shell_view, e_addressbook_view_get_client (view), contacts);

	g_ptr_array_unref (contacts);
}

static void
action_contact_copy_cb (EUIAction *action,
			GVariant *parameter,
			gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;
	EBookShellContent *book_shell_content;
	EAddressbookView *view;

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	e_addressbook_view_copy_to_folder (view, FALSE);
}

static void
action_contact_delete_cb (EUIAction *action,
			  GVariant *parameter,
			  gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;
	EBookShellContent *book_shell_content;
	EAddressbookView *view;

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	e_selectable_delete_selection (E_SELECTABLE (view));
}

static void
action_contact_find_cb (EUIAction *action,
			GVariant *parameter,
			gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;
	EBookShellContent *book_shell_content;
	EPreviewPane *preview_pane;

	book_shell_content = book_shell_view->priv->book_shell_content;
	preview_pane = e_book_shell_content_get_preview_pane (book_shell_content);

	e_preview_pane_show_search_bar (preview_pane);
}

static void
action_contact_forward_run (EShell *shell,
			    GPtrArray *contacts)
{
	GSList *destinations = NULL;
	guint ii;

	for (ii = 0; ii < (contacts ? contacts->len : 0); ii++) {
		EContact *contact = g_ptr_array_index (contacts, ii);
		EDestination *destination;

		destination = e_destination_new ();
		e_destination_set_contact (destination, contact, 0);

		destinations = g_slist_prepend (destinations, destination);
	}

	destinations = g_slist_reverse (destinations);

	eab_send_as_attachment (shell, destinations);

	g_slist_free_full (destinations, g_object_unref);
}

static void
action_contact_forward_got_selected_cb (GObject *source_object,
					GAsyncResult *result,
					gpointer user_data)
{
	RetrieveSelectedData *rsd = user_data;
	GPtrArray *contacts;
	GError *error = NULL;

	g_return_if_fail (rsd != NULL);

	contacts = e_addressbook_view_dup_selected_contacts_finish (E_ADDRESSBOOK_VIEW (source_object), result, &error);
	if (contacts) {
		e_activity_set_state (rsd->activity, E_ACTIVITY_COMPLETED);

		action_contact_forward_run (rsd->shell, contacts);
	} else if (!e_activity_handle_cancellation (rsd->activity, error)) {
		g_warning ("%s: Failed to retrieve selected contacts: %s", G_STRFUNC, error ? error->message : "Unknown error");
		e_activity_set_state (rsd->activity, E_ACTIVITY_COMPLETED);
	}

	g_clear_pointer (&contacts, g_ptr_array_unref);
	g_clear_error (&error);

	retrieve_selected_data_free (rsd);
}

static void
action_contact_forward_cb (EUIAction *action,
			   GVariant *parameter,
			   gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EBookShellContent *book_shell_content;
	EAddressbookView *view;
	GPtrArray *contacts;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	contacts = e_addressbook_view_peek_selected_contacts (view);
	if (!contacts) {
		RetrieveSelectedData *rsd;

		rsd = retrieve_selected_data_new (shell_view);

		e_addressbook_view_dup_selected_contacts (view, e_activity_get_cancellable (rsd->activity),
			action_contact_forward_got_selected_cb, rsd);

		return;
	}

	action_contact_forward_run (shell, contacts);

	g_ptr_array_unref (contacts);
}

static void
action_contact_move_cb (EUIAction *action,
			GVariant *parameter,
			gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;
	EBookShellContent *book_shell_content;
	EAddressbookView *view;

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	e_addressbook_view_move_to_folder (view, FALSE);
}

static void
action_contact_new_cb (EUIAction *action,
		       GVariant *parameter,
		       gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EBookShellContent *book_shell_content;
	EAddressbookView *view;
	EContact *contact;
	EABEditor *editor;
	EBookClient *book;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	book = e_addressbook_view_get_client (view);
	g_return_if_fail (book != NULL);

	contact = eab_new_contact_for_book (book);
	editor = e_contact_editor_new (shell, book, contact, TRUE, TRUE);
	gtk_window_set_transient_for (eab_editor_get_window (editor), GTK_WINDOW (shell_window));
	eab_editor_show (editor);
	g_object_unref (contact);
}

static void
action_contact_new_list_cb (EUIAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;
	EShellView *shell_view;
	EBookShellContent *book_shell_content;
	EAddressbookView *view;
	EBookClient *book;

	shell_view = E_SHELL_VIEW (book_shell_view);

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	book = e_addressbook_view_get_client (view);
	g_return_if_fail (book != NULL);

	e_book_shell_view_open_list_editor_with_prefill (shell_view, book);
}

static void
action_contact_open_cb (EUIAction *action,
			GVariant *parameter,
			gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;
	EBookShellContent *book_shell_content;
	EAddressbookView *view;

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	e_addressbook_view_view (view);
}

static void
action_contact_preview_show_maps_cb (EUIAction *action,
				     GVariant *parameter,
				     gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;
	EBookShellContent *book_shell_content;
	gboolean show_maps;

	e_ui_action_set_state (action, parameter);

	book_shell_content = book_shell_view->priv->book_shell_content;
	show_maps = e_ui_action_get_active (action);
	e_book_shell_content_set_preview_show_maps (book_shell_content, show_maps);
}

static void
action_contact_print_cb (EUIAction *action,
			 GVariant *parameter,
			 gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;
	EBookShellContent *book_shell_content;
	EAddressbookView *view;
	GtkPrintOperationAction print_action;

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	print_action = GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG;
	e_addressbook_view_print (view, TRUE, print_action);
}

static void
action_contact_save_as_run (EShell *shell,
			    EShellBackend *shell_backend,
			    GPtrArray *contacts)
{
	EActivity *activity;
	GFile *file;
	gchar *string;

	string = eab_suggest_filename ((!contacts || !contacts->len || contacts->len > 1) ? NULL : g_ptr_array_index (contacts, 0));

	file = e_shell_run_save_dialog (
		/* Translators: This is a save dialog title */
		shell, _("Save as vCard"), string,
		"*.vcf:text/x-vcard,text/directory", NULL, NULL);
	g_free (string);

	if (file == NULL)
		return;

	string = eab_contact_array_to_string (contacts);
	if (string == NULL) {
		g_warning ("Could not convert contact array to a string");
		g_object_unref (file);
		return;
	}

	/* XXX No callback means errors are discarded.
	 *
	 *     There's an EAlert for this which I'm not using
	 *     until I figure out a better way to display errors:
	 *
	 *     "addressbook:save-error"
	 */
	activity = e_file_replace_contents_async (
		file, string, strlen (string), NULL, FALSE,
		G_FILE_CREATE_NONE, (GAsyncReadyCallback) NULL, NULL);
	e_shell_backend_add_activity (shell_backend, activity);

	/* Free the string when the activity is finalized. */
	g_object_set_data_full (
		G_OBJECT (activity),
		"file-content", string,
		(GDestroyNotify) g_free);

	g_object_unref (file);
}

static void
action_contact_save_as_got_selected_cb (GObject *source_object,
					GAsyncResult *result,
					gpointer user_data)
{
	RetrieveSelectedData *rsd = user_data;
	GPtrArray *contacts;
	GError *error = NULL;

	g_return_if_fail (rsd != NULL);

	contacts = e_addressbook_view_dup_selected_contacts_finish (E_ADDRESSBOOK_VIEW (source_object), result, &error);
	if (!contacts) {
		if (!e_activity_handle_cancellation (rsd->activity, error)) {
			g_warning ("%s: Failed to retrieve selected contacts: %s", G_STRFUNC, error ? error->message : "Unknown error");
			e_activity_set_state (rsd->activity, E_ACTIVITY_COMPLETED);
		}
	} else {
		e_activity_set_state (rsd->activity, E_ACTIVITY_COMPLETED);

		action_contact_save_as_run (rsd->shell, rsd->shell_backend, contacts);
	}

	g_clear_pointer (&contacts, g_ptr_array_unref);
	g_clear_error (&error);

	retrieve_selected_data_free (rsd);
}

static void
action_contact_save_as_cb (EUIAction *action,
			   GVariant *parameter,
			   gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellBackend *shell_backend;
	EBookShellContent *book_shell_content;
	EAddressbookView *view;
	GPtrArray *contacts;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	contacts = e_addressbook_view_peek_selected_contacts (view);
	if (!contacts) {
		RetrieveSelectedData *rsd;

		rsd = retrieve_selected_data_new (shell_view);

		e_addressbook_view_dup_selected_contacts (view, e_activity_get_cancellable (rsd->activity),
			action_contact_save_as_got_selected_cb, rsd);

		return;
	}

	action_contact_save_as_run (shell, shell_backend, contacts);

	g_ptr_array_unref (contacts);
}

static void
action_contact_send_message_run (EShell *shell,
				 GPtrArray *contacts)
{
	GSList *destinations = NULL;
	guint ii;

	for (ii = 0; ii < (contacts ? contacts->len : 0); ii++) {
		EContact *contact = g_ptr_array_index (contacts, ii);
		EDestination *destination;

		destination = e_destination_new ();
		e_destination_set_contact (destination, contact, 0);

		destinations = g_slist_prepend (destinations, destination);
	}

	destinations = g_slist_reverse (destinations);

	eab_send_as_to (shell, destinations);

	g_slist_free_full (destinations, g_object_unref);
}

static void
action_contact_send_message_got_selected_cb (GObject *source_object,
					     GAsyncResult *result,
					     gpointer user_data)
{
	RetrieveSelectedData *rsd = user_data;
	GPtrArray *contacts;
	GError *error = NULL;

	g_return_if_fail (rsd != NULL);

	contacts = e_addressbook_view_dup_selected_contacts_finish (E_ADDRESSBOOK_VIEW (source_object), result, &error);
	if (contacts) {
		e_activity_set_state (rsd->activity, E_ACTIVITY_COMPLETED);

		action_contact_send_message_run (rsd->shell, contacts);
	} else {
		if (!e_activity_handle_cancellation (rsd->activity, error)) {
			g_warning ("%s: Failed to retrieve selected contacts: %s", G_STRFUNC, error ? error->message : "Unknown error");
			e_activity_set_state (rsd->activity, E_ACTIVITY_COMPLETED);
		}
	}

	g_clear_pointer (&contacts, g_ptr_array_unref);
	g_clear_error (&error);

	retrieve_selected_data_free (rsd);
}

static void
action_contact_send_message_cb (EUIAction *action,
				GVariant *parameter,
				gpointer user_data)
{
	EBookShellView *book_shell_view = user_data;
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EBookShellContent *book_shell_content;
	EAddressbookView *view;
	GPtrArray *contacts;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	contacts = e_addressbook_view_peek_selected_contacts (view);
	if (!contacts) {
		RetrieveSelectedData *rsd;

		rsd = retrieve_selected_data_new (shell_view);

		e_addressbook_view_dup_selected_contacts (view, e_activity_get_cancellable (rsd->activity),
			action_contact_send_message_got_selected_cb, rsd);

		return;
	}

	action_contact_send_message_run (shell, contacts);

	g_ptr_array_unref (contacts);
}

static void
action_contact_cards_sort_by_cb (EUIAction *action,
				 GVariant *parameter,
				 gpointer user_data)
{
	EBookShellView *self = user_data;
	GalViewInstance *view_instance;
	GalView *gl_view;
	EAddressbookView *addr_view;

	e_ui_action_set_state (action, parameter);

	addr_view = e_book_shell_content_get_current_view (self->priv->book_shell_content);
	view_instance = e_addressbook_view_get_view_instance (addr_view);
	gl_view = gal_view_instance_get_current_view (view_instance);

	if (GAL_IS_VIEW_MINICARD (gl_view))
		gal_view_minicard_set_sort_by (GAL_VIEW_MINICARD (gl_view), g_variant_get_int32 (parameter));
	else
		g_warn_if_reached ();
}

void
e_book_shell_view_actions_init (EBookShellView *self)
{
	static const EUIActionEntry contact_entries[] = {

		{ "address-book-copy",
		  "edit-copy",
		  N_("Co_py All Contacts To…"),
		  NULL,
		  N_("Copy the contacts of the selected address book to another"),
		  action_address_book_copy_cb, NULL, NULL, NULL },

		{ "address-book-delete",
		  "edit-delete",
		  N_("D_elete Address Book"),
		  NULL,
		  N_("Delete the selected address book"),
		  action_address_book_delete_cb, NULL, NULL, NULL },

		{ "address-book-delete-popup",
		  "edit-delete",
		  N_("_Delete"),
		  NULL,
		  N_("Delete the selected address book"),
		  action_address_book_delete_cb, NULL, NULL, NULL },

		{ "address-book-manage-groups",
		  NULL,
		  N_("_Manage Address Book groups…"),
		  NULL,
		  N_("Manage address book groups order and visibility"),
		  action_address_book_manage_groups_cb, NULL, NULL, NULL },

		{ "address-book-manage-groups-popup",
		  NULL,
		  N_("_Manage groups…"),
		  NULL,
		  N_("Manage address book groups order and visibility"),
		  action_address_book_manage_groups_cb, NULL, NULL, NULL },

		{ "address-book-move",
		  "folder-move",
		  N_("Mo_ve All Contacts To…"),
		  NULL,
		  N_("Move the contacts of the selected address book to another"),
		  action_address_book_move_cb, NULL, NULL, NULL },

		{ "address-book-new",
		  "address-book-new",
		  N_("_New Address Book"),
		  NULL,
		  N_("Create a new address book"),
		  action_address_book_new_cb, NULL, NULL, NULL },

		{ "address-book-properties",
		  "document-properties",
		  N_("Address _Book Properties"),
		  NULL,
		  N_("Show properties of the selected address book"),
		  action_address_book_properties_cb, NULL, NULL, NULL },

		{ "address-book-properties-popup",
		  "document-properties",
		  N_("_Properties"),
		  NULL,
		  N_("Show properties of the selected address book"),
		  action_address_book_properties_cb, NULL, NULL, NULL },

		{ "address-book-refresh",
		  "view-refresh",
		  N_("Re_fresh"),
		  NULL,
		  N_("Refresh the selected address book"),
		  action_address_book_refresh_cb, NULL, NULL, NULL },

		{ "address-book-refresh-backend",
		  "view-refresh",
		  N_("Re_fresh list of account address books"),
		  NULL,
		  NULL,
		  action_address_book_refresh_backend_cb, NULL, NULL, NULL },

		{ "address-book-map",
		  NULL,
		  N_("Address Book _Map"),
		  NULL,
		  N_("Show map with all contacts from selected address book"),
		  action_address_book_map_cb, NULL, NULL, NULL },

		{ "address-book-map-popup",
		  NULL,
		  N_("Address Book Map"),
		  NULL,
		  N_("Show map with all contacts from selected address book"),
		  action_address_book_map_cb, NULL, NULL, NULL },

		{ "address-book-rename",
		  NULL,
		  N_("_Rename…"),
		  NULL,
		  N_("Rename the selected address book"),
		  action_address_book_rename_cb, NULL, NULL, NULL },

		{ "address-book-stop",
		  "process-stop",
		  N_("_Stop"),
		  NULL,
		  N_("Stop loading"),
		  action_address_book_stop_cb, NULL, NULL, NULL },

		{ "contact-bulk-edit",
		  NULL,
		  N_("_Bulk Edit…"),
		  "<Control>b",
		  N_("Edit selected contacts in a bulk"),
		  action_contact_bulk_edit_cb, NULL, NULL, NULL },

		{ "contact-copy",
		  NULL,
		  N_("_Copy Contact To…"),
		  "<Control><Shift>y",
		  N_("Copy selected contacts to another address book"),
		  action_contact_copy_cb, NULL, NULL, NULL },

		{ "contact-delete",
		  "edit-delete",
		  N_("_Delete Contact"),
		  "<Control>d",
		  N_("Delete selected contacts"),
		  action_contact_delete_cb, NULL, NULL, NULL },

		{ "contact-find",
		  "edit-find",
		  N_("_Find in Contact…"),
		  "<Shift><Control>f",
		  N_("Search for text in the displayed contact"),
		  action_contact_find_cb, NULL, NULL, NULL },

		{ "contact-forward",
		  "mail-forward",
		  N_("_Forward Contact…"),
		  NULL,
		  N_("Send selected contacts to another person"),
		  action_contact_forward_cb, NULL, NULL, NULL },

		{ "contact-move",
		  NULL,
		  N_("_Move Contact To…"),
		  "<Control><Shift>v",
		  N_("Move selected contacts to another address book"),
		  action_contact_move_cb, NULL, NULL, NULL },

		{ "contact-new",
		  "contact-new",
		  N_("_New Contact…"),
		  NULL,
		  N_("Create a new contact"),
		  action_contact_new_cb, NULL, NULL, NULL },

		{ "contact-new-list",
		  "stock_contact-list",
		  N_("New Contact _List…"),
		  NULL,
		  N_("Create a new contact list"),
		  action_contact_new_list_cb, NULL, NULL, NULL },

		{ "contact-open",
		  NULL,
		  N_("_Open Contact"),
		  "<Control>o",
		  N_("View the current contact"),
		  action_contact_open_cb, NULL, NULL, NULL },

		{ "contact-send-message",
		  "mail-message-new",
		  N_("_Send Message to Contact…"),
		  NULL,
		  N_("Send a message to the selected contacts"),
		  action_contact_send_message_cb, NULL, NULL, NULL },

		/*** Menus ***/

		{ "contact-actions-menu",
		  NULL,
		  N_("_Actions"),
		  NULL,
		  NULL,
		  NULL, NULL, NULL, NULL },

		{ "contact-preview-menu",
		  NULL,
		  N_("_Preview"),
		  NULL,
		  NULL,
		  NULL, NULL, NULL, NULL },

		{ "contact-cards-sort-by-menu",
		  NULL,
		  N_("_Sort Cards By"),
		  NULL,
		  NULL,
		  NULL, NULL, NULL, NULL }
	};

	static const EUIActionEntry contact_toggle_entries[] = {

		{ "contact-preview",
		  NULL,
		  N_("Contact _Preview"),
		  "<Control>m",
		  N_("Show contact preview window"),
		  NULL, NULL, "true", NULL },

		{ "contact-preview-show-maps",
		  NULL,
		  N_("Show _Maps"),
		  NULL,
		  N_("Show maps in contact preview window"),
		  NULL, NULL, "false", action_contact_preview_show_maps_cb }
	};

	static const EUIActionEnumEntry contact_view_entries[] = {

		{ "contact-view-classic",
		  NULL,
		  N_("_Classic View"),
		  NULL,
		  N_("Show contact preview below the contact list"),
		  NULL, 0 },

		{ "contact-view-vertical",
		  NULL,
		  N_("_Vertical View"),
		  NULL,
		  N_("Show contact preview alongside the contact list"),
		  NULL, 1 }
	};

	static const EUIActionEnumEntry contact_search_entries[] = {

		{ "contact-search-advanced-hidden",
		  NULL,
		  N_("Advanced Search"),
		  NULL,
		  NULL,
		  NULL, CONTACT_SEARCH_ADVANCED },

		{ "contact-search-any-field-contains",
		  NULL,
		  N_("Any field contains"),
		  NULL,
		  NULL,
		  NULL, CONTACT_SEARCH_ANY_FIELD_CONTAINS },

		{ "contact-search-email-begins-with",
		  NULL,
		  N_("Email begins with"),
		  NULL,
		  NULL,
		  NULL, CONTACT_SEARCH_EMAIL_BEGINS_WITH },

		{ "contact-search-email-contains",
		  NULL,
		  N_("Email contains"),
		  NULL,
		  NULL,
		  NULL, CONTACT_SEARCH_EMAIL_CONTAINS },

		{ "contact-search-phone-contains",
		  NULL,
		  N_("Phone contains"),
		  NULL,
		  NULL,
		  NULL, CONTACT_SEARCH_PHONE_CONTAINS },

		{ "contact-search-name-contains",
		  NULL,
		  N_("Name contains"),
		  NULL,
		  NULL,
		  NULL, CONTACT_SEARCH_NAME_CONTAINS }
	};

	static const EUIActionEnumEntry contact_cards_sort_by_entries[] = {

		{ "contact-cards-sort-by-file-as",
		  NULL,
		  N_("_File Under"),
		  NULL,
		  NULL,
		  action_contact_cards_sort_by_cb, E_CARDS_SORT_BY_FILE_AS },

		{ "contact-cards-sort-by-given-name",
		  NULL,
		  N_("_Given Name"),
		  NULL,
		  NULL,
		  action_contact_cards_sort_by_cb, E_CARDS_SORT_BY_GIVEN_NAME },

		{ "contact-cards-sort-by-family-name",
		  NULL,
		  N_("Family _Name"),
		  NULL,
		  NULL,
		  action_contact_cards_sort_by_cb, E_CARDS_SORT_BY_FAMILY_NAME },
	};

	static const EUIActionEntry lockdown_printing_entries[] = {

		{ "address-book-print",
		  "document-print",
		  N_("_Print…"),
		  "<Control>p",
		  N_("Print all shown contacts"),
		  action_address_book_print_cb, NULL, NULL, NULL },

		{ "address-book-print-preview",
		  "document-print-preview",
		  N_("Pre_view…"),
		  NULL,
		  N_("Preview the contacts to be printed"),
		  action_address_book_print_preview_cb, NULL, NULL, NULL },

		{ "contact-print",
		  "document-print",
		  N_("_Print…"),
		  NULL,
		  N_("Print selected contacts"),
		  action_contact_print_cb, NULL, NULL, NULL }
	};

	static const EUIActionEntry lockdown_save_to_disk_entries[] = {

		{ "address-book-save-as",
		  "document-save-as",
		  N_("S_ave Address Book as vCard"),
		  NULL,
		  N_("Save the contacts of the selected address book as a vCard"),
		  action_address_book_save_as_cb, NULL, NULL, NULL },

		{ "address-book-save-as-popup",
		  "document-save-as",
		  /* Translators: This is an action label */
		  N_("_Save as vCard…"),
		  NULL,
		  N_("Save the contacts of the selected address book as a vCard"),
		  action_address_book_save_as_cb, NULL, NULL, NULL },

		{ "contact-save-as",
		  "document-save-as",
		  /* Translators: This is an action label */
		  N_("_Save as vCard…"),
		  NULL,
		  N_("Save selected contacts as a vCard"),
		  action_contact_save_as_cb, NULL, NULL, NULL }
	};

	EShellView *shell_view;
	EUIManager *ui_manager;

	shell_view = E_SHELL_VIEW (self);
	ui_manager = e_shell_view_get_ui_manager (shell_view);

	/* Contact Actions */
	e_ui_manager_add_actions (ui_manager, "contacts", NULL,
		contact_entries, G_N_ELEMENTS (contact_entries), self);
	e_ui_manager_add_actions (ui_manager, "contacts", NULL,
		contact_toggle_entries, G_N_ELEMENTS (contact_toggle_entries), self);
	e_ui_manager_add_actions_enum (ui_manager, "contacts", NULL,
		contact_view_entries, G_N_ELEMENTS (contact_view_entries), self);
	e_ui_manager_add_actions_enum (ui_manager, "contacts", NULL,
		contact_search_entries, G_N_ELEMENTS (contact_search_entries), self);
	e_ui_manager_add_actions_enum (ui_manager, "contacts", NULL,
		contact_cards_sort_by_entries, G_N_ELEMENTS (contact_cards_sort_by_entries), self);

	/* Lockdown Printing Actions */
	e_ui_manager_add_actions (ui_manager, "lockdown-printing", NULL,
		lockdown_printing_entries, G_N_ELEMENTS (lockdown_printing_entries), self);

	/* Lockdown Save-to-Disk Actions */
	e_ui_manager_add_actions (ui_manager, "lockdown-save-to-disk", NULL,
		lockdown_save_to_disk_entries, G_N_ELEMENTS (lockdown_save_to_disk_entries), self);

	/* Fine tuning. */

	e_binding_bind_property (
		ACTION (CONTACT_PREVIEW), "active",
		ACTION (CONTACT_VIEW_CLASSIC), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		ACTION (CONTACT_PREVIEW), "active",
		ACTION (CONTACT_VIEW_VERTICAL), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		ACTION (CONTACT_PREVIEW), "active",
		ACTION (CONTACT_PREVIEW_SHOW_MAPS), "sensitive",
		G_BINDING_SYNC_CREATE);

	/* Never show the action for the preview panel, the feature required
	   WebKit1 functionality (gtk+ widgets inside webview).
	   Re-enable once there is a good replacement.
	   See also accum_address_map() in eab-contact-formatter.cpp.
	*/
	e_ui_action_set_visible (ACTION (CONTACT_PREVIEW_SHOW_MAPS), FALSE);

	/* Hide it from the start */
	e_ui_action_set_visible (ACTION (CONTACT_CARDS_SORT_BY_MENU), FALSE);

	e_ui_manager_set_enum_entries_usable_for_kinds (ui_manager, E_UI_ELEMENT_KIND_MENU,
		contact_cards_sort_by_entries, G_N_ELEMENTS (contact_cards_sort_by_entries));

	e_ui_manager_set_enum_entries_usable_for_kinds (ui_manager, 0,
		contact_search_entries, G_N_ELEMENTS (contact_search_entries));
}

void
e_book_shell_view_update_search_filter (EBookShellView *book_shell_view)
{
	static const EUIActionEnumEntry contact_filter_entries[] = {

		{ "contact-filter-any-category",
		  NULL,
		  N_("Any Category"),
		  NULL,
		  NULL,
		  NULL, CONTACT_FILTER_ANY_CATEGORY },

		{ "contact-filter-unmatched",
		  NULL,
		  N_("Unmatched"),
		  NULL,
		  NULL,
		  NULL, CONTACT_FILTER_UNMATCHED }
	};

	EBookShellContent *book_shell_content;
	EShellView *shell_view;
	EShellSearchbar *searchbar;
	EActionComboBox *combo_box;
	EUIActionGroup *action_group;
	EUIAction *action;
	GList *list, *iter;
	GPtrArray *radio_group;
	gint ii;

	shell_view = E_SHELL_VIEW (book_shell_view);

	action_group = e_ui_manager_get_action_group (e_shell_view_get_ui_manager (shell_view), "contacts-filter");
	e_ui_action_group_remove_all (action_group);

	/* Add the standard filter actions.  No callback is needed
	 * because changes in the EActionComboBox are detected and
	 * handled by EShellSearchbar. */
	e_ui_manager_add_actions_enum (e_shell_view_get_ui_manager (shell_view),
		e_ui_action_group_get_name (action_group), NULL,
		contact_filter_entries, G_N_ELEMENTS (contact_filter_entries), NULL);

	radio_group = g_ptr_array_new ();

	for (ii = 0; ii < G_N_ELEMENTS (contact_filter_entries); ii++) {
		action = e_ui_action_group_get_action (action_group, contact_filter_entries[ii].name);
		e_ui_action_set_usable_for_kinds (action, 0);
		e_ui_action_set_radio_group (action, radio_group);
	}

	/* Build the category actions. */

	list = e_util_dup_searchable_categories ();
	for (iter = list, ii = 0; iter != NULL; iter = iter->next, ii++) {
		const gchar *category_name = iter->data;
		gchar *filename;
		gchar action_name[128];

		g_warn_if_fail (g_snprintf (action_name, sizeof (action_name), "contact-filter-category-%d", ii) < sizeof (action_name));

		action = e_ui_action_new (e_ui_action_group_get_name (action_group), action_name, NULL);
		e_ui_action_set_label (action, category_name);
		e_ui_action_set_state (action, g_variant_new_int32 (ii));
		e_ui_action_set_usable_for_kinds (action, 0);
		e_ui_action_set_radio_group (action, radio_group);

		/* Convert the category icon file to a themed icon name. */
		filename = e_categories_dup_icon_file_for (category_name);
		if (filename != NULL && *filename != '\0') {
			gchar *basename;
			gchar *cp;

			basename = g_path_get_basename (filename);

			/* Lose the file extension. */
			if ((cp = strrchr (basename, '.')) != NULL)
				*cp = '\0';

			e_ui_action_set_icon_name (action, basename);

			g_free (basename);
		}

		g_free (filename);

		e_ui_action_group_add (action_group, action);

		g_object_unref (action);
	}
	g_list_free_full (list, g_free);

	book_shell_content = book_shell_view->priv->book_shell_content;
	searchbar = e_book_shell_content_get_searchbar (book_shell_content);
	combo_box = e_shell_searchbar_get_filter_combo_box (searchbar);

	e_shell_view_block_execute_search (shell_view);

	/* Use any action in the group; doesn't matter which. */
	e_action_combo_box_set_action (combo_box, action);

	ii = CONTACT_FILTER_UNMATCHED;
	e_action_combo_box_add_separator_after (combo_box, ii);

	e_shell_view_unblock_execute_search (shell_view);

	g_ptr_array_unref (radio_group);
}
