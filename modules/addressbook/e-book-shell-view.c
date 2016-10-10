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

G_DEFINE_DYNAMIC_TYPE (
	EBookShellView,
	e_book_shell_view,
	E_TYPE_SHELL_VIEW);

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

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_book_shell_view_parent_class)->constructed (object);

	book_shell_view = E_BOOK_SHELL_VIEW (object);
	e_book_shell_view_private_constructed (book_shell_view);
}

static void
book_shell_view_execute_search (EShellView *shell_view)
{
	EBookShellViewPrivate *priv;
	EBookShellContent *book_shell_content;
	EShellWindow *shell_window;
	EShellContent *shell_content;
	EShellSearchbar *searchbar;
	EActionComboBox *combo_box;
	GtkRadioAction *action;
	EAddressbookView *view;
	EAddressbookModel *model;
	gchar *query;
	gchar *temp;
	gint filter_id, search_id;
	gchar *search_text = NULL;
	EFilterRule *advanced_search = NULL;

	priv = E_BOOK_SHELL_VIEW_GET_PRIVATE (shell_view);

	if (priv->search_locked)
		return;

	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);

	book_shell_content = E_BOOK_SHELL_CONTENT (shell_content);
	searchbar = e_book_shell_content_get_searchbar (book_shell_content);

	action = GTK_RADIO_ACTION (ACTION (CONTACT_SEARCH_ANY_FIELD_CONTAINS));
	search_id = gtk_radio_action_get_current_value (action);

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

	/* Submit the query. */
	view = e_book_shell_content_get_current_view (book_shell_content);
	model = e_addressbook_view_get_model (view);
	e_addressbook_model_set_query (model, query);
	e_addressbook_view_set_search (
		view, filter_id, search_id, search_text, advanced_search);
	g_free (query);
	g_free (search_text);
}

static void
book_shell_view_update_actions (EShellView *shell_view)
{
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EShellWindow *shell_window;
	GtkAction *action;
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

	/* Chain up to parent's update_actions() method. */
	E_SHELL_VIEW_CLASS (e_book_shell_view_parent_class)->
		update_actions (shell_view);

	shell_window = e_shell_view_get_shell_window (shell_view);

	shell_content = e_shell_view_get_shell_content (shell_view);
	state = e_shell_content_check_state (shell_content);

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

	any_contacts_selected =
		(single_contact_selected || multiple_contacts_selected);

	action = ACTION (ADDRESS_BOOK_MOVE);
	sensitive = source_is_editable;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (ADDRESS_BOOK_DELETE);
	sensitive =
		primary_source_is_removable ||
		primary_source_is_remote_deletable;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (ADDRESS_BOOK_PRINT);
	sensitive = has_primary_source;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (ADDRESS_BOOK_PRINT_PREVIEW);
	sensitive = has_primary_source;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (ADDRESS_BOOK_PROPERTIES);
	sensitive = primary_source_is_writable;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (ADDRESS_BOOK_REFRESH);
	sensitive = refresh_supported;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (ADDRESS_BOOK_RENAME);
	sensitive =
		primary_source_is_writable &&
		!primary_source_in_collection;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (ADDRESS_BOOK_STOP);
	sensitive = source_is_busy;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_COPY);
	sensitive = any_contacts_selected;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_DELETE);
	sensitive = source_is_editable && any_contacts_selected;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_FIND);
	sensitive = single_contact_selected;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_FORWARD);
	sensitive = any_contacts_selected;
	gtk_action_set_sensitive (action, sensitive);
	if (multiple_contacts_selected)
		label = _("_Forward Contacts");
	else
		label = _("_Forward Contact");
	gtk_action_set_label (action, label);

	action = ACTION (CONTACT_MOVE);
	sensitive = source_is_editable && any_contacts_selected;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_NEW);
	sensitive = source_is_editable;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_NEW_LIST);
	sensitive = source_is_editable;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_OPEN);
	sensitive = any_contacts_selected;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_PRINT);
	sensitive = any_contacts_selected;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_SAVE_AS);
	sensitive = any_contacts_selected;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CONTACT_SEND_MESSAGE);
	sensitive = any_contacts_selected && selection_has_email;
	gtk_action_set_sensitive (action, sensitive);
	if (multiple_contacts_selected)
		label = _("_Send Message to Contacts");
	else if (selection_is_contact_list)
		label = _("_Send Message to List");
	else
		label = _("_Send Message to Contact");
	gtk_action_set_label (action, label);
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

	g_type_class_add_private (class, sizeof (EBookShellViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = book_shell_view_dispose;
	object_class->finalize = book_shell_view_finalize;
	object_class->constructed = book_shell_view_constructed;

	shell_view_class = E_SHELL_VIEW_CLASS (class);
	shell_view_class->label = _("Contacts");
	shell_view_class->icon_name = "x-office-address-book";
	shell_view_class->ui_definition = "evolution-contacts.ui";
	shell_view_class->ui_manager_id = "org.gnome.evolution.contacts";
	shell_view_class->search_options = "/contact-search-options";
	shell_view_class->search_rules = "addresstypes.xml";
	shell_view_class->new_shell_content = e_book_shell_content_new;
	shell_view_class->new_shell_sidebar = e_book_shell_sidebar_new;
	shell_view_class->execute_search = book_shell_view_execute_search;
	shell_view_class->update_actions = book_shell_view_update_actions;

	/* Ensure the GalView types we need are registered. */
	g_type_ensure (GAL_TYPE_VIEW_ETABLE);
	g_type_ensure (GAL_TYPE_VIEW_MINICARD);
}

static void
e_book_shell_view_init (EBookShellView *book_shell_view)
{
	book_shell_view->priv =
		E_BOOK_SHELL_VIEW_GET_PRIVATE (book_shell_view);

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
