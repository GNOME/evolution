/*
 * e-book-shell-view-actions.c
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

#include "e-book-shell-view-private.h"

#include <e-util/e-alert-dialog.h>
#include <e-util/e-util.h>
#include <filter/e-filter-rule.h>

#include <addressbook-config.h>

static void
action_address_book_copy_cb (GtkAction *action,
                             EBookShellView *book_shell_view)
{
	EBookShellContent *book_shell_content;
	EAddressbookView *view;

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	e_addressbook_view_copy_to_folder (view, TRUE);
}

static void
action_address_book_delete_cb (GtkAction *action,
                               EBookShellView *book_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EBookShellBackend *book_shell_backend;
	EBookShellSidebar *book_shell_sidebar;
	ESource *source;
	ESourceSelector *selector;
	ESourceGroup *source_group;
	ESourceList *source_list;
	EBook *book;
	gint response;
	GError *error = NULL;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	book_shell_backend = book_shell_view->priv->book_shell_backend;
	source_list = e_book_shell_backend_get_source_list (book_shell_backend);

	book_shell_sidebar = book_shell_view->priv->book_shell_sidebar;
	selector = e_book_shell_sidebar_get_selector (book_shell_sidebar);
	source = e_source_selector_peek_primary_selection (selector);
	g_return_if_fail (source != NULL);

	response = e_alert_run_dialog_for_args (
		GTK_WINDOW (shell_window),
		"addressbook:ask-delete-addressbook",
		e_source_peek_name (source), NULL);

	if (response != GTK_RESPONSE_YES)
		return;

	book = e_book_new (source, &error);
	if (error != NULL) {
		g_warning ("Error removing addressbook: %s", error->message);
		g_error_free (error);
		return;
	}

	if (!e_book_remove (book, NULL)) {
		e_alert_run_dialog_for_args (
			GTK_WINDOW (shell_window),
			"addressbook:remove-addressbook", NULL);
		g_object_unref (book);
		return;
	}

	if (e_source_selector_source_is_selected (selector, source))
		e_source_selector_unselect_source (selector, source);

	source_group = e_source_peek_group (source);
	e_source_group_remove_source (source_group, source);

	e_source_list_sync (source_list, NULL);

	g_object_unref (book);
}

static void
action_address_book_move_cb (GtkAction *action,
                             EBookShellView *book_shell_view)
{
	EBookShellContent *book_shell_content;
	EAddressbookView *view;

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	e_addressbook_view_move_to_folder (view, TRUE);
}

static void
action_address_book_new_cb (GtkAction *action,
                            EBookShellView *book_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	addressbook_config_create_new_source (GTK_WIDGET (shell_window));
}

static void
action_address_book_print_cb (GtkAction *action,
                              EBookShellView *book_shell_view)
{
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
action_address_book_print_preview_cb (GtkAction *action,
                                      EBookShellView *book_shell_view)
{
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
action_address_book_properties_cb (GtkAction *action,
                                   EBookShellView *book_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EBookShellSidebar *book_shell_sidebar;
	ESource *source;
	ESourceSelector *selector;
	EditorUidClosure *closure;
	GHashTable *uid_to_editor;
	const gchar *uid;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	book_shell_sidebar = book_shell_view->priv->book_shell_sidebar;
	selector = e_book_shell_sidebar_get_selector (book_shell_sidebar);
	source = e_source_selector_peek_primary_selection (selector);
	g_return_if_fail (source != NULL);

	uid = e_source_peek_uid (source);
	uid_to_editor = book_shell_view->priv->uid_to_editor;

	closure = g_hash_table_lookup (uid_to_editor, uid);
	if (closure == NULL) {
		GtkWidget *editor;

		editor = addressbook_config_edit_source (
			GTK_WIDGET (shell_window), source);

		closure = g_new (EditorUidClosure, 1);
		closure->editor = editor;
		closure->uid = g_strdup (uid);
		closure->view = book_shell_view;

		g_hash_table_insert (uid_to_editor, closure->uid, closure);

		g_object_weak_ref (
			G_OBJECT (closure->editor), (GWeakNotify)
			e_book_shell_view_editor_weak_notify, closure);
	}

	gtk_window_present (GTK_WINDOW (closure->editor));
}

static void
action_address_book_rename_cb (GtkAction *action,
                               EBookShellView *book_shell_view)
{
	EBookShellSidebar *book_shell_sidebar;
	ESourceSelector *selector;

	book_shell_sidebar = book_shell_view->priv->book_shell_sidebar;
	selector = e_book_shell_sidebar_get_selector (book_shell_sidebar);

	e_source_selector_edit_primary_selection (selector);
}

static void
action_address_book_save_as_cb (GtkAction *action,
                                EBookShellView *book_shell_view)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellBackend *shell_backend;
	EBookShellContent *book_shell_content;
	EAddressbookModel *model;
	EAddressbookView *view;
	EActivity *activity;
	EBookQuery *query;
	EBook *book;
	GList *list = NULL;
	GFile *file;
	gchar *string;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	model = e_addressbook_view_get_model (view);
	book = e_addressbook_model_get_book (model);

	query = e_book_query_any_field_contains ("");
	e_book_get_contacts (book, query, &list, NULL);
	e_book_query_unref (query);

	if (list == NULL)
		goto exit;

	string = eab_suggest_filename (list);
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
	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);
}

static void
action_address_book_stop_cb (GtkAction *action,
                             EBookShellView *book_shell_view)
{
	EBookShellContent *book_shell_content;
	EAddressbookView *view;

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	e_addressbook_view_stop (view);
}

static void
action_contact_copy_cb (GtkAction *action,
                        EBookShellView *book_shell_view)
{
	EBookShellContent *book_shell_content;
	EAddressbookView *view;

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	e_addressbook_view_copy_to_folder (view, FALSE);
}

static void
action_contact_delete_cb (GtkAction *action,
                          EBookShellView *book_shell_view)
{
	EBookShellContent *book_shell_content;
	EAddressbookView *view;

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	e_selectable_delete_selection (E_SELECTABLE (view));
}

static void
action_contact_find_cb (GtkAction *action,
                        EBookShellView *book_shell_view)
{
	EBookShellContent *book_shell_content;
	EPreviewPane *preview_pane;

	book_shell_content = book_shell_view->priv->book_shell_content;
	preview_pane = e_book_shell_content_get_preview_pane (book_shell_content);

	e_preview_pane_show_search_bar (preview_pane);
}

static void
action_contact_forward_cb (GtkAction *action,
                           EBookShellView *book_shell_view)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EBookShellContent *book_shell_content;
	EAddressbookView *view;
	GList *list, *iter;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	list = e_addressbook_view_get_selected (view);
	g_return_if_fail (list != NULL);

	/* Convert the list of contacts to a list of destinations. */
	for (iter = list; iter != NULL; iter = iter->next) {
		EContact *contact = iter->data;
		EDestination *destination;

		destination = e_destination_new ();
		e_destination_set_contact (destination, contact, 0);
		g_object_unref (contact);

		iter->data = destination;
	}

	eab_send_as_attachment (shell, list);

	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);
}

static void
action_contact_move_cb (GtkAction *action,
                        EBookShellView *book_shell_view)
{
	EBookShellContent *book_shell_content;
	EAddressbookView *view;

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	e_addressbook_view_move_to_folder (view, FALSE);
}

static void
action_contact_new_cb (GtkAction *action,
                       EBookShellView *book_shell_view)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EBookShellContent *book_shell_content;
	EAddressbookView *view;
	EAddressbookModel *model;
	EContact *contact;
	EABEditor *editor;
	EBook *book;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	model = e_addressbook_view_get_model (view);
	book = e_addressbook_model_get_book (model);
	g_return_if_fail (book != NULL);

	contact = e_contact_new ();
	editor = e_contact_editor_new (shell, book, contact, TRUE, TRUE);
	eab_editor_show (editor);
	g_object_unref (contact);
}

static void
action_contact_new_list_cb (GtkAction *action,
                            EBookShellView *book_shell_view)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EBookShellContent *book_shell_content;
	EAddressbookView *view;
	EAddressbookModel *model;
	EContact *contact;
	EABEditor *editor;
	EBook *book;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	model = e_addressbook_view_get_model (view);
	book = e_addressbook_model_get_book (model);
	g_return_if_fail (book != NULL);

	contact = e_contact_new ();
	editor = e_contact_list_editor_new (shell, book, contact, TRUE, TRUE);
	eab_editor_show (editor);
	g_object_unref (contact);
}

static void
action_contact_open_cb (GtkAction *action,
                        EBookShellView *book_shell_view)
{
	EBookShellContent *book_shell_content;
	EAddressbookView *view;

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	e_addressbook_view_view (view);
}

static void
action_contact_preview_cb (GtkToggleAction *action,
                           EBookShellView *book_shell_view)
{
	EBookShellContent *book_shell_content;
	gboolean visible;

	book_shell_content = book_shell_view->priv->book_shell_content;
	visible = gtk_toggle_action_get_active (action);
	e_book_shell_content_set_preview_visible (book_shell_content, visible);
}

static void
action_contact_print_cb (GtkAction *action,
                         EBookShellView *book_shell_view)
{
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
action_contact_save_as_cb (GtkAction *action,
                           EBookShellView *book_shell_view)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellBackend *shell_backend;
	EBookShellContent *book_shell_content;
	EAddressbookView *view;
	EActivity *activity;
	GList *list;
	GFile *file;
	gchar *string;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	list = e_addressbook_view_get_selected (view);

	if (list == NULL)
		goto exit;

	string = eab_suggest_filename (list);
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
	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);
}

static void
action_contact_send_message_cb (GtkAction *action,
                                EBookShellView *book_shell_view)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EBookShellContent *book_shell_content;
	EAddressbookView *view;
	GList *list, *iter;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	book_shell_content = book_shell_view->priv->book_shell_content;
	view = e_book_shell_content_get_current_view (book_shell_content);
	g_return_if_fail (view != NULL);

	list = e_addressbook_view_get_selected (view);
	g_return_if_fail (list != NULL);

	/* Convert the list of contacts to a list of destinations. */
	for (iter = list; iter != NULL; iter = iter->next) {
		EContact *contact = iter->data;
		EDestination *destination;

		destination = e_destination_new ();
		e_destination_set_contact (destination, contact, 0);
		g_object_unref (contact);

		iter->data = destination;
	}

	eab_send_as_to (shell, list);

	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);
}

static void
action_contact_view_cb (GtkRadioAction *action,
                        GtkRadioAction *current,
                        EBookShellView *book_shell_view)
{
	EBookShellContent *book_shell_content;
	GtkOrientable *orientable;
	GtkOrientation orientation;

	book_shell_content = book_shell_view->priv->book_shell_content;
	orientable = GTK_ORIENTABLE (book_shell_content);

	switch (gtk_radio_action_get_current_value (action)) {
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
}

static void
action_gal_save_custom_view_cb (GtkAction *action,
                                EBookShellView *book_shell_view)
{
	EBookShellContent *book_shell_content;
	EShellView *shell_view;
	EAddressbookView *address_view;
	GalViewInstance *view_instance;

	/* All shell views respond to the activation of this action,
	 * which is defined by EShellWindow.  But only the currently
	 * active shell view proceeds with saving the custom view. */
	shell_view = E_SHELL_VIEW (book_shell_view);
	if (!e_shell_view_is_active (shell_view))
		return;

	book_shell_content = book_shell_view->priv->book_shell_content;
	address_view = e_book_shell_content_get_current_view (book_shell_content);
	view_instance = e_addressbook_view_get_view_instance (address_view);
	gal_view_instance_save_as (view_instance);
}

static GtkActionEntry contact_entries[] = {

	{ "address-book-copy",
	  GTK_STOCK_COPY,
	  N_("Co_py All Contacts To..."),
	  NULL,
	  N_("Copy the contacts of the selected address book to another"),
	  G_CALLBACK (action_address_book_copy_cb) },

	{ "address-book-delete",
	  GTK_STOCK_DELETE,
	  N_("D_elete Address Book"),
	  NULL,
	  N_("Delete the selected address book"),
	  G_CALLBACK (action_address_book_delete_cb) },

	{ "address-book-move",
	  "folder-move",
	  N_("Mo_ve All Contacts To..."),
	  NULL,
	  N_("Move the contacts of the selected address book to another"),
	  G_CALLBACK (action_address_book_move_cb) },

	{ "address-book-new",
	  "address-book-new",
	  N_("_New Address Book"),
	  NULL,
	  N_("Create a new address book"),
	  G_CALLBACK (action_address_book_new_cb) },

	{ "address-book-properties",
	  GTK_STOCK_PROPERTIES,
	  N_("Address _Book Properties"),
	  NULL,
	  N_("Show properties of the selected address book"),
	  G_CALLBACK (action_address_book_properties_cb) },

	{ "address-book-rename",
	  NULL,
	  N_("_Rename..."),
	  "F2",
	  N_("Rename the selected address book"),
	  G_CALLBACK (action_address_book_rename_cb) },

	{ "address-book-save-as",
	  GTK_STOCK_SAVE_AS,
	  N_("S_ave Address Book as vCard"),
	  NULL,
	  N_("Save the contacts of the selected address book as a vCard"),
	  G_CALLBACK (action_address_book_save_as_cb) },

	{ "address-book-stop",
	  GTK_STOCK_STOP,
	  NULL,
	  NULL,
	  N_("Stop loading"),
	  G_CALLBACK (action_address_book_stop_cb) },

	{ "contact-copy",
	  NULL,
	  N_("_Copy Contact To..."),
	  "<Control><Shift>y",
	  N_("Copy selected contacts to another address book"),
	  G_CALLBACK (action_contact_copy_cb) },

	{ "contact-delete",
	  GTK_STOCK_DELETE,
	  N_("_Delete Contact"),
	  "<Control>d",
	  N_("Delete selected contacts"),
	  G_CALLBACK (action_contact_delete_cb) },

	{ "contact-find",
	  GTK_STOCK_FIND,
	  N_("_Find in Contact..."),
	  "<Shift><Control>f",
	  N_("Search for text in the displayed contact"),
	  G_CALLBACK (action_contact_find_cb) },

	{ "contact-forward",
	  "mail-forward",
	  N_("_Forward Contact..."),
	  NULL,
	  N_("Send selected contacts to another person"),
	  G_CALLBACK (action_contact_forward_cb) },

	{ "contact-move",
	  NULL,
	  N_("_Move Contact To..."),
	  "<Control><Shift>v",
	  N_("Move selected contacts to another address book"),
	  G_CALLBACK (action_contact_move_cb) },

	{ "contact-new",
	  "contact-new",
	  N_("_New Contact..."),
	  NULL,
	  N_("Create a new contact"),
	  G_CALLBACK (action_contact_new_cb) },

	{ "contact-new-list",
	  "stock_contact-list",
	  N_("New Contact _List..."),
	  NULL,
	  N_("Create a new contact list"),
	  G_CALLBACK (action_contact_new_list_cb) },

	{ "contact-open",
	  NULL,
	  N_("_Open Contact"),
	  "<Control>o",
	  N_("View the current contact"),
	  G_CALLBACK (action_contact_open_cb) },

	{ "contact-send-message",
	  "mail-message-new",
	  N_("_Send Message to Contact..."),
	  NULL,
	  N_("Send a message to the selected contacts"),
	  G_CALLBACK (action_contact_send_message_cb) },

	/*** Menus ***/

	{ "contact-actions-menu",
	  NULL,
	  N_("_Actions"),
	  NULL,
	  NULL,
	  NULL },

	{ "contact-preview-menu",
	  NULL,
	  N_("_Preview"),
	  NULL,
	  NULL,
	  NULL }
};

static EPopupActionEntry contact_popup_entries[] = {

	{ "address-book-popup-delete",
	  N_("_Delete"),
	  "address-book-delete" },

	{ "address-book-popup-properties",
	  N_("_Properties"),
	  "address-book-properties" },

	{ "address-book-popup-rename",
	  NULL,
	  "address-book-rename" },

	{ "address-book-popup-save-as",
	  /* Translators: This is an action label */
	  N_("_Save as vCard..."),
	  "address-book-save-as" },

	{ "contact-popup-copy",
	  NULL,
	  "contact-copy" },

	{ "contact-popup-forward",
	  NULL,
	  "contact-forward" },

	{ "contact-popup-move",
	  NULL,
	  "contact-move" },

	{ "contact-popup-open",
	  NULL,
	  "contact-open" },

	{ "contact-popup-send-message",
	  NULL,
	  "contact-send-message" },
};

static GtkToggleActionEntry contact_toggle_entries[] = {

	{ "contact-preview",
	  NULL,
	  N_("Contact _Preview"),
	  "<Control>m",
	  N_("Show contact preview window"),
	  G_CALLBACK (action_contact_preview_cb),
	  TRUE }
};

static GtkRadioActionEntry contact_view_entries[] = {

	/* This action represents the initial active contact view.
	 * It should not be visible in the UI, nor should it be
	 * possible to switch to it from another shell view. */
	{ "contact-view-initial",
	  NULL,
	  NULL,
	  NULL,
	  NULL,
	  -1 },

	{ "contact-view-classic",
	  NULL,
	  N_("_Classic View"),
	  NULL,
	  N_("Show contact preview below the contact list"),
	  0 },

	{ "contact-view-vertical",
	  NULL,
	  N_("_Vertical View"),
	  NULL,
	  N_("Show contact preview alongside the contact list"),
	  1 }
};

static GtkRadioActionEntry contact_filter_entries[] = {

	{ "contact-filter-any-category",
	  NULL,
	  N_("Any Category"),
	  NULL,
	  NULL,
	  CONTACT_FILTER_ANY_CATEGORY },

	{ "contact-filter-unmatched",
	  NULL,
	  N_("Unmatched"),
	  NULL,
	  NULL,
	  CONTACT_FILTER_UNMATCHED }
};

static GtkRadioActionEntry contact_search_entries[] = {

	{ "contact-search-advanced-hidden",
	  NULL,
	  N_("Advanced Search"),
	  NULL,
	  NULL,
	  CONTACT_SEARCH_ADVANCED },

	{ "contact-search-any-field-contains",
	  NULL,
	  N_("Any field contains"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  CONTACT_SEARCH_ANY_FIELD_CONTAINS },

	{ "contact-search-email-begins-with",
	  NULL,
	  N_("Email begins with"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  CONTACT_SEARCH_EMAIL_BEGINS_WITH },

	{ "contact-search-name-contains",
	  NULL,
	  N_("Name contains"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  CONTACT_SEARCH_NAME_CONTAINS }
};

static GtkActionEntry lockdown_printing_entries[] = {

	{ "address-book-print",
	  GTK_STOCK_PRINT,
	  NULL,
	  "<Control>p",
	  N_("Print all shown contacts"),
	  G_CALLBACK (action_address_book_print_cb) },

	{ "address-book-print-preview",
	  GTK_STOCK_PRINT_PREVIEW,
	  NULL,
	  NULL,
	  N_("Preview the contacts to be printed"),
	  G_CALLBACK (action_address_book_print_preview_cb) },

	{ "contact-print",
	  GTK_STOCK_PRINT,
	  NULL,
	  NULL,
	  N_("Print selected contacts"),
	  G_CALLBACK (action_contact_print_cb) }
};

static EPopupActionEntry lockdown_printing_popup_entries[] = {

	{ "contact-popup-print",
	  NULL,
	  "contact-print" }
};

static GtkActionEntry lockdown_save_to_disk_entries[] = {

	{ "contact-save-as",
	  GTK_STOCK_SAVE_AS,
	  /* Translators: This is an action label */
	  N_("_Save as vCard..."),
	  NULL,
	  N_("Save selected contacts as a vCard"),
	  G_CALLBACK (action_contact_save_as_cb) }
};

static EPopupActionEntry lockdown_save_to_disk_popup_entries[] = {

	{ "contact-popup-save-as",
	  NULL,
	  "contact-save-as" }
};

void
e_book_shell_view_actions_init (EBookShellView *book_shell_view)
{
	EBookShellContent *book_shell_content;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSearchbar *searchbar;
	EPreviewPane *preview_pane;
	EWebView *web_view;
	GtkActionGroup *action_group;
	GConfBridge *bridge;
	GtkAction *action;
	GObject *object;
	const gchar *key;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	book_shell_content = book_shell_view->priv->book_shell_content;
	searchbar = e_book_shell_content_get_searchbar (book_shell_content);
	preview_pane = e_book_shell_content_get_preview_pane (book_shell_content);
	web_view = e_preview_pane_get_web_view (preview_pane);

	/* Contact Actions */
	action_group = ACTION_GROUP (CONTACTS);
	gtk_action_group_add_actions (
		action_group, contact_entries,
		G_N_ELEMENTS (contact_entries), book_shell_view);
	e_action_group_add_popup_actions (
		action_group, contact_popup_entries,
		G_N_ELEMENTS (contact_popup_entries));
	gtk_action_group_add_toggle_actions (
		action_group, contact_toggle_entries,
		G_N_ELEMENTS (contact_toggle_entries), book_shell_view);
	gtk_action_group_add_radio_actions (
		action_group, contact_view_entries,
		G_N_ELEMENTS (contact_view_entries), -1,
		G_CALLBACK (action_contact_view_cb), book_shell_view);
	gtk_action_group_add_radio_actions (
		action_group, contact_search_entries,
		G_N_ELEMENTS (contact_search_entries),
		-1, NULL, NULL);

	/* Advanced Search Action */
	action = ACTION (CONTACT_SEARCH_ADVANCED_HIDDEN);
	gtk_action_set_visible (action, FALSE);
	e_shell_searchbar_set_search_option (
		searchbar, GTK_RADIO_ACTION (action));

	/* Lockdown Printing Actions */
	action_group = ACTION_GROUP (LOCKDOWN_PRINTING);
	gtk_action_group_add_actions (
		action_group, lockdown_printing_entries,
		G_N_ELEMENTS (lockdown_printing_entries),
		book_shell_view);
	e_action_group_add_popup_actions (
		action_group, lockdown_printing_popup_entries,
		G_N_ELEMENTS (lockdown_printing_popup_entries));

	/* Lockdown Save-to-Disk Actions */
	action_group = ACTION_GROUP (LOCKDOWN_SAVE_TO_DISK);
	gtk_action_group_add_actions (
		action_group, lockdown_save_to_disk_entries,
		G_N_ELEMENTS (lockdown_save_to_disk_entries),
		book_shell_view);
	e_action_group_add_popup_actions (
		action_group, lockdown_save_to_disk_popup_entries,
		G_N_ELEMENTS (lockdown_save_to_disk_popup_entries));

	/* Bind GObject properties to GConf keys. */

	bridge = gconf_bridge_get ();

	object = G_OBJECT (ACTION (CONTACT_PREVIEW));
	key = "/apps/evolution/addressbook/display/show_preview";
	gconf_bridge_bind_property (bridge, key, object, "active");

	object = G_OBJECT (ACTION (CONTACT_VIEW_VERTICAL));
	key = "/apps/evolution/addressbook/display/layout";
	gconf_bridge_bind_property (bridge, key, object, "current-value");

	/* Fine tuning. */

	g_signal_connect (
		ACTION (GAL_SAVE_CUSTOM_VIEW), "activate",
		G_CALLBACK (action_gal_save_custom_view_cb), book_shell_view);

	e_binding_new (
		ACTION (CONTACT_PREVIEW), "active",
		ACTION (CONTACT_VIEW_CLASSIC), "sensitive");

	e_binding_new (
		ACTION (CONTACT_PREVIEW), "active",
		ACTION (CONTACT_VIEW_VERTICAL), "sensitive");

	e_web_view_set_open_proxy (web_view, ACTION (CONTACT_OPEN));
	e_web_view_set_print_proxy (web_view, ACTION (CONTACT_PRINT));
	e_web_view_set_save_as_proxy (web_view, ACTION (CONTACT_SAVE_AS));
}

void
e_book_shell_view_update_search_filter (EBookShellView *book_shell_view)
{
	EBookShellContent *book_shell_content;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSearchbar *searchbar;
	EActionComboBox *combo_box;
	GtkActionGroup *action_group;
	GtkRadioAction *radio_action;
	GList *list, *iter;
	GSList *group;
	gint ii;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	action_group = ACTION_GROUP (CONTACTS_FILTER);
	e_action_group_remove_all_actions (action_group);

	/* Add the standard filter actions.  No callback is needed
	 * because changes in the EActionComboBox are detected and
	 * handled by EShellSearchbar. */
	gtk_action_group_add_radio_actions (
		action_group, contact_filter_entries,
		G_N_ELEMENTS (contact_filter_entries),
		CONTACT_FILTER_ANY_CATEGORY, NULL, NULL);

	/* Retrieve the radio group from an action we just added. */
	list = gtk_action_group_list_actions (action_group);
	radio_action = GTK_RADIO_ACTION (list->data);
	group = gtk_radio_action_get_group (radio_action);
	g_list_free (list);

	/* Build the category actions. */

	list = e_util_get_searchable_categories ();
	for (iter = list, ii = 0; iter != NULL; iter = iter->next, ii++) {
		const gchar *category_name = iter->data;
		const gchar *filename;
		GtkAction *action;
		gchar *action_name;

		action_name = g_strdup_printf (
			"contact-filter-category-%d", ii);
		radio_action = gtk_radio_action_new (
			action_name, category_name, NULL, NULL, ii);
		g_free (action_name);

		/* Convert the category icon file to a themed icon name. */
		filename = e_categories_get_icon_file_for (category_name);
		if (filename != NULL && *filename != '\0') {
			gchar *basename;
			gchar *cp;

			basename = g_path_get_basename (filename);

			/* Lose the file extension. */
			if ((cp = strrchr (basename, '.')) != NULL)
				*cp = '\0';

			g_object_set (
				radio_action, "icon-name", basename, NULL);

			g_free (basename);
		}

		gtk_radio_action_set_group (radio_action, group);
		group = gtk_radio_action_get_group (radio_action);

		/* The action group takes ownership of the action. */
		action = GTK_ACTION (radio_action);
		gtk_action_group_add_action (action_group, action);
		g_object_unref (radio_action);
	}
	g_list_free (list);

	book_shell_content = book_shell_view->priv->book_shell_content;
	searchbar = e_book_shell_content_get_searchbar (book_shell_content);
	combo_box = e_shell_searchbar_get_filter_combo_box (searchbar);

	e_shell_view_block_execute_search (shell_view);

	/* Use any action in the group; doesn't matter which. */
	e_action_combo_box_set_action (combo_box, radio_action);

	ii = CONTACT_FILTER_UNMATCHED;
	e_action_combo_box_add_separator_after (combo_box, ii);

	e_shell_view_unblock_execute_search (shell_view);
}
