/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-book-shell-view-actions.c
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

#include <e-util/e-error.h>
#include <e-util/e-util.h>
#include <e-util/gconf-bridge.h>

#include <addressbook-config.h>

static void
action_address_book_copy_cb (GtkAction *action,
                             EBookShellView *book_shell_view)
{
	EABView *view;

	view = e_book_shell_view_get_current_view (book_shell_view);
	g_return_if_fail (view != NULL);
	eab_view_copy_to_folder (view, TRUE);
}

static void
action_address_book_delete_cb (GtkAction *action,
                               EBookShellView *book_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	ESource *source;
	ESourceSelector *selector;
	ESourceGroup *source_group;
	ESourceList *source_list;
	EBook *book;
	gint response;
	GError *error = NULL;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	selector = E_SOURCE_SELECTOR (book_shell_view->priv->selector);
	source = e_source_selector_peek_primary_selection (selector);
	g_return_if_fail (source != NULL);

	response = e_error_run (
		GTK_WINDOW (shell_window),
		"addressbook:ask-delete-addressbook",
		e_source_peek_name (source));

	if (response != GTK_RESPONSE_YES)
		return;

	book = e_book_new (source, &error);
	if (error != NULL) {
		g_warning ("Error removing addressbook: %s", error->message);
		g_error_free (error);
		return;
	}

	if (!e_book_remove (book, NULL)) {
		e_error_run (
			GTK_WINDOW (shell_window),
			"addressbook:remove-addressbook", NULL);
		g_object_unref (book);
		return;
	}

	if (e_source_selector_source_is_selected (selector, source))
		e_source_selector_unselect_source (selector, source);

	source_group = e_source_peek_group (source);
	e_source_group_remove_source (source_group, source);

	source_list = book_shell_view->priv->source_list;
	e_source_list_sync (source_list, NULL);

	g_object_unref (book);
}

static void
action_address_book_move_cb (GtkAction *action,
                             EBookShellView *book_shell_view)
{
	EABView *view;

	view = e_book_shell_view_get_current_view (book_shell_view);
	g_return_if_fail (view != NULL);
	eab_view_move_to_folder (view, TRUE);
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
action_address_book_properties_cb (GtkAction *action,
                                   EBookShellView *book_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	ESource *source;
	ESourceSelector *selector;
	EditorUidClosure *closure;
	GHashTable *uid_to_editor;
	const gchar *uid;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	selector = E_SOURCE_SELECTOR (book_shell_view->priv->selector);
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
action_address_book_save_as_cb (GtkAction *action,
                                EBookShellView *book_shell_view)
{
	EABView *view;

	view = e_book_shell_view_get_current_view (book_shell_view);
	g_return_if_fail (view != NULL);
	eab_view_save_as (view, TRUE);
}

static void
action_address_book_stop_cb (GtkAction *action,
                             EBookShellView *book_shell_view)
{
	EABView *view;

	view = e_book_shell_view_get_current_view (book_shell_view);
	g_return_if_fail (view != NULL);
	eab_view_stop (view);
}

static void
action_contact_clipboard_copy_cb (GtkAction *action,
                                  EBookShellView *book_shell_view)
{
	EABView *view;

	view = e_book_shell_view_get_current_view (book_shell_view);
	g_return_if_fail (view != NULL);
	eab_view_copy (view);
}

static void
action_contact_clipboard_cut_cb (GtkAction *action,
                                 EBookShellView *book_shell_view)
{
	EABView *view;

	view = e_book_shell_view_get_current_view (book_shell_view);
	g_return_if_fail (view != NULL);
	eab_view_cut (view);
}

static void
action_contact_clipboard_paste_cb (GtkAction *action,
                                   EBookShellView *book_shell_view)
{
	EABView *view;

	view = e_book_shell_view_get_current_view (book_shell_view);
	g_return_if_fail (view != NULL);
	eab_view_paste (view);
}

static void
action_contact_copy_cb (GtkAction *action,
                        EBookShellView *book_shell_view)
{
	EABView *view;

	view = e_book_shell_view_get_current_view (book_shell_view);
	g_return_if_fail (view != NULL);
	eab_view_copy_to_folder (view, FALSE);
}

static void
action_contact_delete_cb (GtkAction *action,
                          EBookShellView *book_shell_view)
{
	EABView *view;

	view = e_book_shell_view_get_current_view (book_shell_view);
	g_return_if_fail (view != NULL);
	eab_view_delete_selection (view, TRUE);
}

static void
action_contact_forward_cb (GtkAction *action,
                           EBookShellView *book_shell_view)
{
	EABView *view;

	view = e_book_shell_view_get_current_view (book_shell_view);
	g_return_if_fail (view != NULL);
	eab_view_send (view);
}

static void
action_contact_move_cb (GtkAction *action,
                        EBookShellView *book_shell_view)
{
	EABView *view;

	view = e_book_shell_view_get_current_view (book_shell_view);
	g_return_if_fail (view != NULL);
	eab_view_move_to_folder (view, FALSE);
}

static void
action_contact_new_cb (GtkAction *action,
                       EBookShellView *book_shell_view)
{
	EContact *contact;
	EBook *book;

	contact = e_contact_new ();
	book = book_shell_view->priv->book;
	eab_show_contact_editor (book, contact, TRUE, TRUE);
	g_object_unref (contact);
}

static void
action_contact_new_list_cb (GtkAction *action,
                            EBookShellView *book_shell_view)
{
	EContact *contact;
	EBook *book;

	contact = e_contact_new ();
	book = book_shell_view->priv->book;
	eab_show_contact_list_editor (book, contact, TRUE, TRUE);
	g_object_unref (contact);
}

static void
action_contact_open_cb (GtkAction *action,
                        EBookShellView *book_shell_view)
{
	EABView *view;

	view = e_book_shell_view_get_current_view (book_shell_view);
	g_return_if_fail (view != NULL);
	eab_view_view (view);
}

static void
action_contact_preview_cb (GtkToggleAction *action,
                           EBookShellView *book_shell_view)
{
	EABView *view;
	gboolean active;

	view = e_book_shell_view_get_current_view (book_shell_view);
	active = gtk_toggle_action_get_active (action);
	eab_view_show_contact_preview (view, active);
}

static void
action_contact_print_cb (GtkAction *action,
                         EBookShellView *book_shell_view)
{
	EABView *view;

	view = e_book_shell_view_get_current_view (book_shell_view);
	g_return_if_fail (view != NULL);
	eab_view_print (view, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);
}

static void
action_contact_print_preview_cb (GtkAction *action,
                                 EBookShellView *book_shell_view)
{
	EABView *view;

	view = e_book_shell_view_get_current_view (book_shell_view);
	g_return_if_fail (view != NULL);
	eab_view_print (view, GTK_PRINT_OPERATION_ACTION_PREVIEW);
}

static void
action_contact_save_as_cb (GtkAction *action,
                           EBookShellView *book_shell_view)
{
	EABView *view;

	view = e_book_shell_view_get_current_view (book_shell_view);
	g_return_if_fail (view != NULL);
	eab_view_save_as (view, FALSE);
}

static void
action_contact_select_all_cb (GtkAction *action,
                              EBookShellView *book_shell_view)
{
	EABView *view;

	view = e_book_shell_view_get_current_view (book_shell_view);
	g_return_if_fail (view != NULL);
	eab_view_select_all (view);
}

static void
action_contact_send_message_cb (GtkAction *action,
                                EBookShellView *book_shell_view)
{
	EABView *view;

	view = e_book_shell_view_get_current_view (book_shell_view);
	g_return_if_fail (view != NULL);
	eab_view_send_to (view);
}

static void
action_search_execute_cb (GtkAction *action,
                          EBookShellView *book_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellContent *shell_content;
	GtkWidget *widget;
	GString *string;
	EABView *view;
	const gchar *search_format;
	const gchar *search_text;
	gchar *search_query;
	gint value;

	shell_view = E_SHELL_VIEW (book_shell_view);
	if (!e_shell_view_is_selected (shell_view))
		return;

	shell_content = e_shell_view_get_content (shell_view);
	search_text = e_shell_content_get_search_text (shell_content);

	shell_window = e_shell_view_get_shell_window (shell_view);
	action = ACTION (CONTACT_SEARCH_ANY_FIELD_CONTAINS);
	value = gtk_radio_action_get_current_value (
		GTK_RADIO_ACTION (action));

	if (search_text == NULL || *search_text == '\0') {
		search_text = "\"\"";
		value = CONTACT_SEARCH_ANY_FIELD_CONTAINS;
	}

	switch (value) {
		case CONTACT_SEARCH_NAME_CONTAINS:
			search_format = "(contains \"full_name\" %s)";
			break;

		case CONTACT_SEARCH_EMAIL_BEGINS_WITH:
			search_format = "(beginswith \"email\" %s)";
			break;

		default:
			search_text = "\"\"";
			/* fall through */

		case CONTACT_SEARCH_ANY_FIELD_CONTAINS:
			search_format =
				"(contains \"x-evolution-any-field\" %s)";
			break;
	}

	/* Build the query. */
	string = g_string_new ("");
	e_sexp_encode_string (string, search_text);
	search_query = g_strdup_printf (search_format, string->str);
	g_string_free (string, TRUE);

	/* Filter by category. */
	value = e_search_bar_get_filter_value (E_SEARCH_BAR (widget));
	if (value >= 0) {
		GList *categories;
		const gchar *category_name;
		gchar *temp;

		categories = e_categories_get_list ();
		category_name = g_list_nth_data (categories, value);
		g_list_free (categories);

		temp = g_strdup_printf (
			"(and (is \"category_list\" \"%s\") %s)",
			category_name, search_query);
		g_free (search_query);
		search_query = temp;
	}

	/* Submit the query. */
	view = e_book_shell_view_get_current_view (book_shell_view);
	g_object_set (view, "query", search_query, NULL);
	g_free (search_query);

	view->displayed_contact = -1;
	eab_contact_display_render (
		EAB_CONTACT_DISPLAY (view->contact_display),
		NULL, EAB_CONTACT_DISPLAY_RENDER_NORMAL);
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
	  N_("Del_ete Address Book"),
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

	{ "contact-clipboard-copy",
	  GTK_STOCK_COPY,
	  NULL,
	  NULL,
	  N_("Copy the selection"),
	  G_CALLBACK (action_contact_clipboard_copy_cb) },

	{ "contact-clipboard-cut",
	  GTK_STOCK_CUT,
	  NULL,
	  NULL,
	  N_("Cut the selection"),
	  G_CALLBACK (action_contact_clipboard_cut_cb) },

	{ "contact-clipboard-paste",
	  GTK_STOCK_PASTE,
	  NULL,
	  NULL,
	  N_("Paste the clipboard"),
	  G_CALLBACK (action_contact_clipboard_paste_cb) },

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
	  N_("_Open"),
	  "<Control>o",
	  N_("View the current contact"),
	  G_CALLBACK (action_contact_open_cb) },

	{ "contact-print",
	  GTK_STOCK_PRINT,
	  NULL,
	  NULL,
	  N_("Print selected contacts"),
	  G_CALLBACK (action_contact_print_cb) },

	{ "contact-print-preview",
	  GTK_STOCK_PRINT_PREVIEW,
	  NULL,
	  NULL,
	  N_("Preview the contacts to be printed"),
	  G_CALLBACK (action_contact_print_preview_cb) },

	{ "contact-save-as",
	  GTK_STOCK_SAVE_AS,
	  N_("Save as vCard..."),
	  NULL,
	  N_("Save selected contacts as a vCard"),
	  G_CALLBACK (action_contact_save_as_cb) },

	{ "contact-select-all",
	  GTK_STOCK_SELECT_ALL,
	  NULL,
	  NULL,
	  N_("Select all contacts"),
	  G_CALLBACK (action_contact_select_all_cb) },

	{ "contact-send-message",
	  "mail-message-new",
	  N_("_Send Message to Contact..."),
	  NULL,
	  N_("Send a message to the selected contacts"),
	  G_CALLBACK (action_contact_send_message_cb) },

	/*** Menus ***/

	{ "actions-menu",
	  NULL,
	  N_("_Actions"),
	  NULL,
	  NULL,
	  NULL },

	/*** Address Book Popup Actions ***/

	{ "address-book-popup-delete",
	  GTK_STOCK_DELETE,
	  NULL,
	  NULL,
	  N_("Delete this address book"),
	  G_CALLBACK (action_address_book_delete_cb) },

	{ "address-book-popup-properties",
	  GTK_STOCK_PROPERTIES,
	  NULL,
	  NULL,
	  N_("Show properties of this address book"),
	  G_CALLBACK (action_address_book_properties_cb) },

	{ "address-book-popup-save-as",
	  GTK_STOCK_SAVE_AS,
	  N_("_Save as vCard..."),
	  NULL,
	  N_("Save the contents of this address book as a vCard"),
	  G_CALLBACK (action_address_book_save_as_cb) }
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

static GtkRadioActionEntry contact_search_entries[] = {

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

void
e_book_shell_view_actions_init (EBookShellView *book_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkActionGroup *action_group;
	GtkUIManager *manager;
	GConfBridge *bridge;
	GtkAction *action;
	GObject *object;
	const gchar *domain;
	const gchar *key;

	shell_view = E_SHELL_VIEW (book_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	manager = e_shell_window_get_ui_manager (shell_window);
	domain = GETTEXT_PACKAGE;

	e_load_ui_definition (manager, "evolution-contacts.ui");

	action_group = book_shell_view->priv->contact_actions;
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_action_group_add_actions (
		action_group, contact_entries,
		G_N_ELEMENTS (contact_entries), book_shell_view);
	gtk_action_group_add_toggle_actions (
		action_group, contact_toggle_entries,
		G_N_ELEMENTS (contact_toggle_entries), book_shell_view);
	gtk_action_group_add_radio_actions (
		action_group, contact_search_entries,
		G_N_ELEMENTS (contact_search_entries),
		CONTACT_SEARCH_NAME_CONTAINS,
		NULL, NULL);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);

	/* Bind GObject properties to GConf keys. */

	bridge = gconf_bridge_get ();

	object = G_OBJECT (ACTION (CONTACT_PREVIEW));
	key = "/apps/evolution/addressbook/display/show_preview";
	gconf_bridge_bind_property (bridge, key, object, "active");

	/* Fine tuning. */

	action = ACTION (CONTACT_DELETE);
	g_object_set (action, "short-label", _("Delete"), NULL);

	g_signal_connect (
		ACTION (SEARCH_EXECUTE), "activate",
		G_CALLBACK (action_search_execute_cb), book_shell_view);
}

void
e_book_shell_view_update_actions (EBookShellView *book_shell_view,
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
	shell_window = e_shell_view_get_shell_window (shell_view);

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
