/*
 * e-book-shell-view.c
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

#include "addressbook/gui/widgets/gal-view-minicard.h"

enum {
	PROP_0,
	PROP_CLICKED_SOURCE
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EBookShellView, e_book_shell_view, E_TYPE_SHELL_VIEW, 0,
	G_ADD_PRIVATE_DYNAMIC (EBookShellView))

static void
book_shell_view_get_property (GObject *object,
			      guint property_id,
			      GValue *value,
			      GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CLICKED_SOURCE:
			g_value_set_object (
				value, e_book_shell_view_get_clicked_source (
				E_SHELL_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
book_shell_view_dispose (GObject *object)
{
	EBookShellView *book_shell_view;

	book_shell_view = E_BOOK_SHELL_VIEW (object);
	e_book_shell_view_private_dispose (book_shell_view);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_book_shell_view_parent_class)->dispose (object);
}

static void
book_shell_view_finalize (GObject *object)
{
	EBookShellView *book_shell_view;

	book_shell_view = E_BOOK_SHELL_VIEW (object);
	e_book_shell_view_private_finalize (book_shell_view);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_book_shell_view_parent_class)->finalize (object);
}

static void
book_shell_view_constructed (GObject *object)
{
	EBookShellView *book_shell_view;
	EUIManager *ui_manager;
	EUICustomizer *customizer;

	ui_manager = e_shell_view_get_ui_manager (E_SHELL_VIEW (object));

	e_ui_manager_freeze (ui_manager);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_book_shell_view_parent_class)->constructed (object);

	book_shell_view = E_BOOK_SHELL_VIEW (object);
	e_book_shell_view_private_constructed (book_shell_view);

	e_ui_manager_thaw (ui_manager);

	customizer = e_ui_manager_get_customizer (ui_manager);
	e_ui_customizer_register (customizer, "address-book-popup", _("Address Book Context Menu"));
	e_ui_customizer_register (customizer, "contact-popup", _("Contact Context Menu"));
}

static void
book_shell_view_execute_search (EShellView *shell_view)
{
	EBookShellView *self;
	EBookShellContent *book_shell_content;
	EShellContent *shell_content;
	EShellSearchbar *searchbar;
	EActionComboBox *combo_box;
	EUIAction *action;
	EAddressbookView *view;
	GVariant *state;
	gchar *query;
	gchar *temp;
	gchar *selected_category;
	gint filter_id, search_id;
	gchar *search_text = NULL;
	EFilterRule *advanced_search = NULL;

	self = E_BOOK_SHELL_VIEW (shell_view);

	if (self->priv->search_locked)
		return;

	shell_content = e_shell_view_get_shell_content (shell_view);

	book_shell_content = E_BOOK_SHELL_CONTENT (shell_content);
	searchbar = e_book_shell_content_get_searchbar (book_shell_content);

	action = ACTION (CONTACT_SEARCH_ANY_FIELD_CONTAINS);
	state = g_action_get_state (G_ACTION (action));
	search_id = g_variant_get_int32 (state);
	g_clear_pointer (&state, g_variant_unref);

	if (search_id == CONTACT_SEARCH_ADVANCED) {
		query = e_shell_view_get_search_query (shell_view);

		if (query == NULL)
			query = g_strdup ("");

		/* internal pointer, no need to free it */
		advanced_search = e_shell_view_get_search_rule (shell_view);
	} else {
		const gchar *text;
		const gchar *format;
		GString *string;

		text = e_shell_searchbar_get_search_text (searchbar);

		if (text == NULL || *text == '\0') {
			text = "";
			search_id = CONTACT_SEARCH_ANY_FIELD_CONTAINS;
		}

		search_text = text && *text ? g_strdup (text) : NULL;

		switch (search_id) {
			case CONTACT_SEARCH_NAME_CONTAINS:
				format = "(contains \"full_name\" %s)";
				break;

			case CONTACT_SEARCH_EMAIL_BEGINS_WITH:
				format = "(beginswith \"email\" %s)";
				break;

			case CONTACT_SEARCH_EMAIL_CONTAINS:
				format = "(contains \"email\" %s)";
				break;

			case CONTACT_SEARCH_PHONE_CONTAINS:
				format = "(contains \"phone\" %s)";
				break;

			default:
				text = "";
				/* fall through */

			case CONTACT_SEARCH_ANY_FIELD_CONTAINS:
				format = "(contains \"x-evolution-any-field\" %s)";
				break;
		}

		/* Build the query. */
		string = g_string_new ("");
		e_sexp_encode_string (string, text);
		query = g_strdup_printf (format, string->str);
		g_string_free (string, TRUE);
	}

	/* Apply selected filter. */
	combo_box = e_shell_searchbar_get_filter_combo_box (searchbar);
	filter_id = e_action_combo_box_get_current_value (combo_box);
	switch (filter_id) {
		case CONTACT_FILTER_ANY_CATEGORY:
			break;

		case CONTACT_FILTER_UNMATCHED:
			temp = g_strdup_printf (
				"(and (not (and (exists \"CATEGORIES\") "
				"(not (is \"CATEGORIES\" \"\")))) %s)",
				query);
			g_free (query);
			query = temp;
			break;

		default:
		{
			GList *categories;
			const gchar *category_name;

			categories = e_util_dup_searchable_categories ();
			category_name = g_list_nth_data (categories, filter_id);

			temp = g_strdup_printf (
				"(and (is \"category_list\" \"%s\") %s)",
				category_name, query);
			g_free (query);
			query = temp;

			g_list_free_full (categories, g_free);
			break;
		}
	}

	selected_category = e_addressbook_selector_dup_selected_category (E_ADDRESSBOOK_SELECTOR (
		e_book_shell_sidebar_get_selector (E_BOOK_SHELL_SIDEBAR (e_shell_view_get_shell_sidebar (shell_view)))));

	if (selected_category && *selected_category) {
		temp = g_strdup_printf (
			"(and (is \"category_list\" \"%s\") %s)",
			selected_category, query);
		g_free (query);
		query = temp;
	}
	g_free (selected_category);

	/* Submit the query. */
	view = e_book_shell_content_get_current_view (book_shell_content);
	e_addressbook_view_set_search (view, query, filter_id, search_id, search_text, advanced_search);
	g_free (query);
	g_free (search_text);
}

static void
book_shell_view_update_actions (EShellView *shell_view)
{
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EUIAction *action;
	const gchar *label;
	gboolean sensitive;
	guint32 state;

	/* Be descriptive. */
	gboolean any_contacts_selected;
	gboolean has_primary_source;
	gboolean multiple_contacts_selected;
	gboolean primary_source_is_writable;
	gboolean primary_source_is_removable;
	gboolean primary_source_is_remote_deletable;
	gboolean primary_source_in_collection;
	gboolean refresh_supported;
	gboolean single_contact_selected;
	gboolean selection_is_contact_list;
	gboolean selection_has_email;
	gboolean source_is_busy;
	gboolean source_is_editable;
	gboolean clicked_source_is_primary;
	gboolean clicked_source_is_collection;

	/* Chain up to parent's update_actions() method. */
	E_SHELL_VIEW_CLASS (e_book_shell_view_parent_class)->
		update_actions (shell_view);

	shell_content = e_shell_view_get_shell_content (shell_view);
	state = e_shell_content_check_state (shell_content);

	if (e_book_shell_content_get_preview_visible (E_BOOK_SHELL_CONTENT (shell_content)))
		e_web_view_update_actions (e_preview_pane_get_web_view (e_book_shell_content_get_preview_pane (E_BOOK_SHELL_CONTENT (shell_content))));

	single_contact_selected =
		(state & E_BOOK_SHELL_CONTENT_SELECTION_SINGLE);
	multiple_contacts_selected =
		(state & E_BOOK_SHELL_CONTENT_SELECTION_MULTIPLE);
	selection_has_email =
		(state & E_BOOK_SHELL_CONTENT_SELECTION_HAS_EMAIL);
	selection_is_contact_list =
		(state & E_BOOK_SHELL_CONTENT_SELECTION_IS_CONTACT_LIST);
	source_is_busy =
		(state & E_BOOK_SHELL_CONTENT_SOURCE_IS_BUSY);
	source_is_editable =
		(state & E_BOOK_SHELL_CONTENT_SOURCE_IS_EDITABLE);

	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	state = e_shell_sidebar_check_state (shell_sidebar);

	has_primary_source =
		(state & E_BOOK_SHELL_SIDEBAR_HAS_PRIMARY_SOURCE);
	primary_source_is_writable =
		(state & E_BOOK_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_WRITABLE);
	primary_source_is_removable =
		(state & E_BOOK_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_REMOVABLE);
	primary_source_is_remote_deletable =
		(state & E_BOOK_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_REMOTE_DELETABLE);
	primary_source_in_collection =
		(state & E_BOOK_SHELL_SIDEBAR_PRIMARY_SOURCE_IN_COLLECTION);
	refresh_supported =
		(state & E_BOOK_SHELL_SIDEBAR_SOURCE_SUPPORTS_REFRESH);
	clicked_source_is_primary =
		(state & E_BOOK_SHELL_SIDEBAR_CLICKED_SOURCE_IS_PRIMARY) != 0;
	clicked_source_is_collection =
		(state & E_BOOK_SHELL_SIDEBAR_CLICKED_SOURCE_IS_COLLECTION) != 0;

	any_contacts_selected =
		(single_contact_selected || multiple_contacts_selected);

	action = ACTION (ADDRESS_BOOK_COPY);
	sensitive = has_primary_source;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (ADDRESS_BOOK_MOVE);
	sensitive = has_primary_source && source_is_editable;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (ADDRESS_BOOK_DELETE);
	sensitive =
		primary_source_is_removable ||
		primary_source_is_remote_deletable;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (ADDRESS_BOOK_PRINT);
	sensitive = has_primary_source;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (ADDRESS_BOOK_PRINT_PREVIEW);
	sensitive = has_primary_source;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (ADDRESS_BOOK_PROPERTIES);
	sensitive = clicked_source_is_primary && primary_source_is_writable;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (ADDRESS_BOOK_REFRESH);
	sensitive = clicked_source_is_primary && refresh_supported;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (ADDRESS_BOOK_REFRESH_BACKEND);
	sensitive = clicked_source_is_collection;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (ADDRESS_BOOK_RENAME);
	sensitive = clicked_source_is_primary && (
		primary_source_is_writable &&
		!primary_source_in_collection);
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (ADDRESS_BOOK_SAVE_AS);
	sensitive = has_primary_source;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (ADDRESS_BOOK_MAP_POPUP);
	sensitive = clicked_source_is_primary;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (ADDRESS_BOOK_STOP);
	sensitive = source_is_busy;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_BULK_EDIT);
	sensitive = any_contacts_selected && !selection_is_contact_list;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_COPY);
	sensitive = any_contacts_selected;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_DELETE);
	sensitive = source_is_editable && any_contacts_selected;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_FIND);
	sensitive = single_contact_selected;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_FORWARD);
	sensitive = any_contacts_selected;
	e_ui_action_set_sensitive (action, sensitive);
	if (multiple_contacts_selected)
		label = _("_Forward Contacts");
	else
		label = _("_Forward Contact");
	e_ui_action_set_label (action, label);

	action = ACTION (CONTACT_MOVE);
	sensitive = source_is_editable && any_contacts_selected;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_NEW);
	sensitive = source_is_editable;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_NEW_LIST);
	sensitive = source_is_editable;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_OPEN);
	sensitive = any_contacts_selected;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_PRINT);
	sensitive = any_contacts_selected;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_SAVE_AS);
	sensitive = any_contacts_selected;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_SEND_MESSAGE);
	sensitive = any_contacts_selected && selection_has_email;
	e_ui_action_set_sensitive (action, sensitive);
	if (multiple_contacts_selected)
		label = _("_Send Message to Contacts");
	else if (selection_is_contact_list)
		label = _("_Send Message to List");
	else
		label = _("_Send Message to Contact");
	e_ui_action_set_label (action, label);

#ifndef ENABLE_CONTACT_MAPS
	e_ui_action_set_visible (ACTION (ADDRESS_BOOK_MAP), FALSE);
	e_ui_action_set_visible (ACTION (ADDRESS_BOOK_MAP_POPUP), FALSE);
#endif
}

static void
book_shell_view_init_ui_data (EShellView *shell_view)
{
	g_return_if_fail (E_IS_BOOK_SHELL_VIEW (shell_view));

	e_book_shell_view_actions_init (E_BOOK_SHELL_VIEW (shell_view));
}

static void
e_book_shell_view_class_finalize (EBookShellViewClass *class)
{
}

static void
e_book_shell_view_class_init (EBookShellViewClass *class)
{
	GObjectClass *object_class;
	EShellViewClass *shell_view_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = book_shell_view_get_property;
	object_class->dispose = book_shell_view_dispose;
	object_class->finalize = book_shell_view_finalize;
	object_class->constructed = book_shell_view_constructed;

	shell_view_class = E_SHELL_VIEW_CLASS (class);
	shell_view_class->label = _("Contacts");
	shell_view_class->icon_name = "x-office-address-book";
	shell_view_class->ui_definition = "evolution-contacts.eui";
	shell_view_class->ui_manager_id = "org.gnome.evolution.contacts";
	shell_view_class->search_rules = "addresstypes.xml";
	shell_view_class->new_shell_content = e_book_shell_content_new;
	shell_view_class->new_shell_sidebar = e_book_shell_sidebar_new;
	shell_view_class->execute_search = book_shell_view_execute_search;
	shell_view_class->update_actions = book_shell_view_update_actions;
	shell_view_class->init_ui_data = book_shell_view_init_ui_data;

	g_object_class_install_property (
		object_class,
		PROP_CLICKED_SOURCE,
		g_param_spec_object (
			"clicked-source",
			"Clicked Source",
			"An ESource which had been clicked in the source selector before showing context menu",
			E_TYPE_SOURCE,
			G_PARAM_READABLE));

	/* Ensure the GalView types we need are registered. */
	g_type_ensure (GAL_TYPE_VIEW_ETABLE);
	g_type_ensure (GAL_TYPE_VIEW_MINICARD);
}

static void
e_book_shell_view_init (EBookShellView *book_shell_view)
{
	book_shell_view->priv = e_book_shell_view_get_instance_private (book_shell_view);

	e_book_shell_view_private_init (book_shell_view);
}

void
e_book_shell_view_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_book_shell_view_register_type (type_module);
}

void
e_book_shell_view_disable_searching (EBookShellView *book_shell_view)
{
	EBookShellViewPrivate *priv;

	g_return_if_fail (book_shell_view != NULL);
	g_return_if_fail (E_IS_BOOK_SHELL_VIEW (book_shell_view));

	priv = book_shell_view->priv;
	priv->search_locked++;
}

void
e_book_shell_view_enable_searching (EBookShellView *book_shell_view)
{
	EBookShellViewPrivate *priv;

	g_return_if_fail (book_shell_view != NULL);
	g_return_if_fail (E_IS_BOOK_SHELL_VIEW (book_shell_view));

	priv = book_shell_view->priv;
	g_return_if_fail (priv->search_locked > 0);

	priv->search_locked--;
}

static void
e_book_shell_view_open_list_editor_with_prefill_contacts (EShellView *shell_view,
							  EBookClient *destination_book,
							  GPtrArray *contacts, /* EContact *, (nullable) */
							  EBookClient *source_book)
{
	EABEditor *editor;
	EContact *new_contact;
	EShellWindow *shell_window;

	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (E_IS_BOOK_CLIENT (destination_book));

	shell_window = e_shell_view_get_shell_window (shell_view);

	new_contact = eab_new_contact_for_book (destination_book);

	if (contacts) {
		EVCard *vcard = E_VCARD (new_contact);
		gboolean any_added = FALSE;
		guint ii;

		for (ii = 0; ii < contacts->len; ii++) {
			EContact *contact = g_ptr_array_index (contacts, ii);
			GList *emails;
			gint jj, len;
			gboolean is_list;

			emails = e_contact_get (contact, E_CONTACT_EMAIL);
			len = g_list_length (emails);
			g_list_free_full (emails, g_free);

			is_list = e_contact_get (contact, E_CONTACT_IS_LIST) != NULL;

			if (len > 0) {
				if (is_list)
					e_contact_set (contact, E_CONTACT_IS_LIST, GINT_TO_POINTER (FALSE));

				any_added = TRUE;

				for (jj = 0; jj < len; jj++) {
					EDestination *dest;
					EVCardAttribute *attr;

					dest = e_destination_new ();

					if (source_book)
						e_destination_set_client (dest, source_book);

					e_destination_set_contact (dest, contact, jj);

					attr = e_vcard_attribute_new (NULL, EVC_EMAIL);
					e_destination_export_to_vcard_attribute (dest, attr);

					e_vcard_append_attribute (vcard, attr);

					g_object_unref (dest);
				}

				if (is_list)
					e_contact_set (contact, E_CONTACT_IS_LIST, GINT_TO_POINTER (TRUE));
			}
		}

		if (any_added)
			e_contact_set (new_contact, E_CONTACT_IS_LIST, GINT_TO_POINTER (TRUE));
	}

	editor = e_contact_list_editor_new (e_shell_window_get_shell (shell_window), destination_book, new_contact, TRUE, TRUE);
	gtk_window_set_transient_for (eab_editor_get_window (editor), GTK_WINDOW (e_shell_view_get_shell_window (shell_view)));
	eab_editor_show (editor);

	g_object_unref (new_contact);
}

typedef struct _DupContactsData {
	EActivity *activity;
	EShellView *shell_view;
	EBookClient *destination_book;
	EBookClient *source_book;
} DupContactsData;

static void
e_book_shell_view_get_selected_contacts_for_list_editor_prefill_cb (GObject *source_object,
								    GAsyncResult *result,
								    gpointer user_data)
{
	DupContactsData *dcd = user_data;
	GPtrArray *contacts;
	GError *error = NULL;

	g_return_if_fail (dcd != NULL);

	contacts = e_addressbook_view_dup_selected_contacts_finish (E_ADDRESSBOOK_VIEW (source_object), result, &error);
	if (!contacts) {
		if (!e_activity_handle_cancellation (dcd->activity, error)) {
			g_warning ("%s: Failed to retrieve selected contacts: %s", G_STRFUNC, error ? error->message : "Unknown error");
			e_activity_set_state (dcd->activity, E_ACTIVITY_COMPLETED);
		}
	} else {
		e_activity_set_state (dcd->activity, E_ACTIVITY_COMPLETED);
	}

	e_book_shell_view_open_list_editor_with_prefill_contacts (dcd->shell_view, dcd->destination_book, contacts, dcd->source_book);

	g_clear_error (&error);
	g_clear_pointer (&contacts, g_ptr_array_unref);
	g_clear_object (&dcd->activity);
	g_clear_object (&dcd->shell_view);
	g_clear_object (&dcd->destination_book);
	g_clear_object (&dcd->source_book);
	g_free (dcd);
}

void
e_book_shell_view_open_list_editor_with_prefill (EShellView *shell_view,
						 EBookClient *destination_book)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (E_IS_BOOK_CLIENT (destination_book));

	if (E_IS_BOOK_SHELL_VIEW (shell_view)) {
		EBookShellView *book_shell_view;
		EBookShellContent *book_shell_content;
		EAddressbookView *current_view;

		book_shell_view = E_BOOK_SHELL_VIEW (shell_view);
		book_shell_content = book_shell_view->priv->book_shell_content;
		current_view = e_book_shell_content_get_current_view (book_shell_content);

		if (current_view && e_addressbook_view_get_n_selected (current_view) >= 1) {
			GPtrArray *contacts;

			contacts = e_addressbook_view_peek_selected_contacts (current_view);

			if (contacts) {
				e_book_shell_view_open_list_editor_with_prefill_contacts (shell_view, destination_book, contacts,
					e_addressbook_view_get_client (current_view));

				g_ptr_array_unref (contacts);
			} else {
				DupContactsData *dcd;
				EActivity *activity;
				GCancellable *cancellable;

				activity = e_activity_new ();
				cancellable = camel_operation_new ();

				e_activity_set_alert_sink (activity, E_ALERT_SINK (e_shell_view_get_shell_content (shell_view)));
				e_activity_set_cancellable (activity, cancellable);
				e_activity_set_text (activity, _("Retrieving selected contacts…"));

				camel_operation_push_message (cancellable, "%s", e_activity_get_text (activity));

				e_shell_backend_add_activity (e_shell_view_get_shell_backend (shell_view), activity);

				dcd = g_new0 (DupContactsData, 1);
				dcd->activity = activity;
				dcd->shell_view = g_object_ref (shell_view);
				dcd->destination_book = g_object_ref (destination_book);
				dcd->source_book = e_addressbook_view_get_client (current_view);
				if (dcd->source_book)
					g_object_ref (dcd->source_book);

				e_addressbook_view_dup_selected_contacts (current_view, cancellable,
					e_book_shell_view_get_selected_contacts_for_list_editor_prefill_cb, dcd);

				g_object_unref (cancellable);
			}

			return;
		}
	}

	e_book_shell_view_open_list_editor_with_prefill_contacts (shell_view, destination_book, NULL, NULL);
}

ESource *
e_book_shell_view_get_clicked_source (EShellView *shell_view)
{
	EBookShellView *book_shell_view;

	g_return_val_if_fail (E_IS_BOOK_SHELL_VIEW (shell_view), NULL);

	book_shell_view = E_BOOK_SHELL_VIEW (shell_view);

	return book_shell_view->priv->clicked_source;
}

void
e_book_shell_view_preselect_source_config (EShellView *shell_view,
					   GtkWidget *source_config)
{
	ESource *clicked_source, *primary_source, *use_source = NULL;

	g_return_if_fail (E_IS_BOOK_SHELL_VIEW (shell_view));
	g_return_if_fail (E_IS_SOURCE_CONFIG (source_config));

	clicked_source = e_book_shell_view_get_clicked_source (shell_view);
	primary_source = e_source_selector_ref_primary_selection (e_book_shell_sidebar_get_selector (
		E_BOOK_SHELL_SIDEBAR (e_shell_view_get_shell_sidebar (shell_view))));

	if (clicked_source && clicked_source != primary_source)
		use_source = clicked_source;
	else if (primary_source)
		use_source = primary_source;

	if (use_source) {
		ESourceBackend *source_backend = NULL;

		if (e_source_has_extension (use_source, E_SOURCE_EXTENSION_COLLECTION))
			source_backend = e_source_get_extension (use_source, E_SOURCE_EXTENSION_COLLECTION);
		else if (e_source_has_extension (use_source, E_SOURCE_EXTENSION_ADDRESS_BOOK))
			source_backend = e_source_get_extension (use_source, E_SOURCE_EXTENSION_ADDRESS_BOOK);

		if (source_backend)
			e_source_config_set_preselect_type (E_SOURCE_CONFIG (source_config), e_source_backend_get_backend_name (source_backend));
		else if (use_source == clicked_source)
			e_source_config_set_preselect_type (E_SOURCE_CONFIG (source_config), e_source_get_uid (use_source));
	}

	g_clear_object (&primary_source);
}
