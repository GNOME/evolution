/*
 * e-mail-shell-view.c
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

#include "e-mail-shell-view-private.h"

static gpointer parent_class;
static GType mail_shell_view_type;

static void
mail_shell_view_dispose (GObject *object)
{
	e_mail_shell_view_private_dispose (E_MAIL_SHELL_VIEW (object));

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
mail_shell_view_finalize (GObject *object)
{
	e_mail_shell_view_private_finalize (E_MAIL_SHELL_VIEW (object));

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
mail_shell_view_constructed (GObject *object)
{
	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	e_mail_shell_view_private_constructed (E_MAIL_SHELL_VIEW (object));
}

static void
mail_shell_view_toggled (EShellView *shell_view)
{
	EMailShellViewPrivate *priv;
	EShellWindow *shell_window;
	GtkUIManager *ui_manager;
	const gchar *basename;
	gboolean view_is_active;

	priv = E_MAIL_SHELL_VIEW_GET_PRIVATE (shell_view);

	shell_window = e_shell_view_get_shell_window (shell_view);
	ui_manager = e_shell_window_get_ui_manager (shell_window);
	view_is_active = e_shell_view_is_active (shell_view);
	basename = E_MAIL_READER_UI_DEFINITION;

	if (view_is_active && priv->merge_id == 0) {
		priv->merge_id = e_load_ui_manager_definition (
			ui_manager, basename);
		e_mail_reader_create_charset_menu (
			E_MAIL_READER (priv->mail_shell_content),
			ui_manager, priv->merge_id);
	} else if (!view_is_active && priv->merge_id != 0) {
		gtk_ui_manager_remove_ui (ui_manager, priv->merge_id);
		priv->merge_id = 0;
	}

	/* Chain up to parent's toggled() method. */
	E_SHELL_VIEW_CLASS (parent_class)->toggled (shell_view);
}

static void
mail_shell_view_execute_search (EShellView *shell_view)
{
	EMailShellViewPrivate *priv;
	EShell *shell;
	EShellWindow *shell_window;
	EShellContent *shell_content;
	EShellSettings *shell_settings;
	EMFormatHTMLDisplay *html_display;
	EMailShellContent *mail_shell_content;
	MessageList *message_list;
	EFilterRule *rule;
	EMailReader *reader;
	CamelFolder *folder;
	GtkAction *action;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter tree_iter;
	GString *string;
	GList *iter;
	GSList *search_strings = NULL;
	const gchar *folder_uri;
	const gchar *text;
	gboolean valid;
	gchar *query;
	gchar *temp;
	gchar *tag;
	gint value;

	priv = E_MAIL_SHELL_VIEW_GET_PRIVATE (shell_view);

	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);

	shell = e_shell_window_get_shell (shell_window);
	shell_settings = e_shell_get_shell_settings (shell);

	mail_shell_content = E_MAIL_SHELL_CONTENT (shell_content);

	reader = E_MAIL_READER (shell_content);
	html_display = e_mail_reader_get_html_display (reader);
	message_list = e_mail_reader_get_message_list (reader);

	folder_uri = message_list->folder_uri;
	folder = message_list->folder;

	if (folder_uri != NULL) {
		GKeyFile *key_file;
		const gchar *key;
		const gchar *string;
		gchar *group_name;

		key_file = e_shell_view_get_state_key_file (shell_view);

		key = STATE_KEY_SEARCH_TEXT;
		string = e_shell_content_get_search_text (shell_content);
		group_name = g_strdup_printf ("Folder %s", folder_uri);

		if (string != NULL && *string != '\0')
			g_key_file_set_string (
				key_file, group_name, key, string);
		else
			g_key_file_remove_key (
				key_file, group_name, key, NULL);
		e_shell_view_set_state_dirty (shell_view);

		g_free (group_name);
	}

	/* This returns a new object reference. */
	model = e_shell_settings_get_object (
		shell_settings, "mail-label-list-store");

	action = ACTION (MAIL_SEARCH_SUBJECT_OR_ADDRESSES_CONTAIN);
	value = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (action));

	text = e_shell_content_get_search_text (shell_content);
	if (value == MAIL_SEARCH_ADVANCED || text == NULL || *text == '\0') {
		query = e_shell_content_get_search_rule_as_string (shell_content);

		if (!query)
			query = g_strdup ("");

		goto filter;
	}

	/* Replace variables in the selected rule with the
	 * current search text and extract a query string. */

	g_return_if_fail (value >= 0 && value < MAIL_NUM_SEARCH_RULES);
	rule = priv->search_rules[value];

	/* Set the search rule in EShellContent so that "Create
	 * Search Folder from Search" works for quick searches. */
	e_shell_content_set_search_rule (shell_content, rule);

	for (iter = rule->parts; iter != NULL; iter = iter->next) {
		EFilterPart *part = iter->data;
		EFilterElement *element = NULL;

		if (strcmp (part->name, "subject") == 0)
			element = e_filter_part_find_element (part, "subject");
		else if (strcmp (part->name, "body") == 0)
			element = e_filter_part_find_element (part, "word");
		else if (strcmp (part->name, "sender") == 0)
			element = e_filter_part_find_element (part, "sender");
		else if (strcmp (part->name, "to") == 0)
			element = e_filter_part_find_element (part, "recipient");

		if (strcmp (part->name, "body") == 0) {
			struct _camel_search_words *words;
			gint ii;

			words = camel_search_words_split ((guchar *) text);
			for (ii = 0; ii < words->len; ii++)
				search_strings = g_slist_prepend (
					search_strings, g_strdup (
					words->words[ii]->word));
			camel_search_words_free (words);
		}

		if (element != NULL) {
			EFilterInput *input = E_FILTER_INPUT (element);
			e_filter_input_set_value (input, text);
		}
	}

	string = g_string_sized_new (1024);
	e_filter_rule_build_code (rule, string);
	query = g_string_free (string, FALSE);

filter:

	/* Apply selected filter. */

	value = e_shell_content_get_filter_value (shell_content);
	switch (value) {
		case MAIL_FILTER_ALL_MESSAGES:
			break;

		case MAIL_FILTER_UNREAD_MESSAGES:
			temp = g_strdup_printf (
				"(and %s (match-all (not "
				"(system-flag \"Seen\"))))", query);
			g_free (query);
			query = temp;
			break;

		case MAIL_FILTER_NO_LABEL:
			string = g_string_sized_new (1024);
			g_string_append_printf (
				string, "(and %s (and ", query);
			valid = gtk_tree_model_get_iter_first (
				model, &tree_iter);
			while (valid) {
				tag = e_mail_label_list_store_get_tag (
					E_MAIL_LABEL_LIST_STORE (model),
					&tree_iter);
				g_string_append_printf (
					string, " (match-all (not (or "
					"(= (user-tag \"label\") \"%s\") "
					"(user-flag \"$Label%s\") "
					"(user-flag \"%s\"))))",
					tag, tag, tag);
				g_free (tag);

				valid = gtk_tree_model_iter_next (
					model, &tree_iter);
			}
			g_string_append_len (string, "))", 2);
			g_free (query);
			query = g_string_free (string, FALSE);
			break;

		case MAIL_FILTER_READ_MESSAGES:
			temp = g_strdup_printf (
				"(and %s (match-all "
				"(system-flag \"Seen\")))", query);
			g_free (query);
			query = temp;
			break;

		case MAIL_FILTER_RECENT_MESSAGES:
			if (em_utils_folder_is_sent (folder, folder_uri))
				temp = g_strdup_printf (
					"(and %s (match-all "
					"(> (get-sent-date) "
					"(- (get-current-date) 86400))))",
					query);
			else
				temp = g_strdup_printf (
					"(and %s (match-all "
					"(> (get-received-date) "
					"(- (get-current-date) 86400))))",
					query);
			g_free (query);
			query = temp;
			break;

		case MAIL_FILTER_LAST_5_DAYS_MESSAGES:
			if (em_utils_folder_is_sent (folder, folder_uri))
				temp = g_strdup_printf (
					"(and %s (match-all "
					"(> (get-sent-date) "
					"(- (get-current-date) 432000))))",
					query);
			else
				temp = g_strdup_printf (
					"(and %s (match-all "
					"(> (get-received-date) "
					"(- (get-current-date) 432000))))",
					query);
			g_free (query);
			query = temp;
			break;

		case MAIL_FILTER_MESSAGES_WITH_ATTACHMENTS:
			temp = g_strdup_printf (
				"(and %s (match-all "
				"(system-flag \"Attachments\")))", query);
			g_free (query);
			query = temp;
			break;

		case MAIL_FILTER_IMPORTANT_MESSAGES:
			temp = g_strdup_printf (
				"(and %s (match-all "
				"(system-flag \"Flagged\")))", query);
			g_free (query);
			query = temp;
			break;

		case MAIL_FILTER_MESSAGES_NOT_JUNK:
			temp = g_strdup_printf (
				"(and %s (match-all (not "
				"(system-flag \"junk\"))))", query);
			g_free (query);
			query = temp;
			break;

		default:
			/* The action value also serves as a path for
			 * the label list store.  That's why we number
			 * the label actions from zero. */
			path = gtk_tree_path_new_from_indices (value, -1);
			gtk_tree_model_get_iter (model, &tree_iter, path);
			gtk_tree_path_free (path);

			tag = e_mail_label_list_store_get_tag (
				E_MAIL_LABEL_LIST_STORE (model), &tree_iter);
			temp = g_strdup_printf (
				"(and %s (match-all (or "
				"(= (user-tag \"label\") \"%s\") "
				"(user-flag \"$Label%s\") "
				"(user-flag \"%s\"))))",
				query, tag, tag, tag);
			g_free (tag);

			g_free (query);
			query = temp;
			break;
	}

	message_list_set_search (message_list, query);

	e_mail_shell_content_set_search_strings (
		mail_shell_content, search_strings);

	g_slist_foreach (search_strings, (GFunc) g_free, NULL);
	g_slist_free (search_strings);

	g_object_unref (model);
	g_free (query);
}

static void
has_unread_mail (GtkTreeModel *model, GtkTreeIter *parent, gboolean is_root, gboolean *has_unread)
{
	guint unread = 0;
	GtkTreeIter iter, child;

	g_return_if_fail (model != NULL);
	g_return_if_fail (parent != NULL);
	g_return_if_fail (has_unread != NULL);

	if (is_root) {
		gboolean is_store = FALSE, is_draft = FALSE;

		gtk_tree_model_get (model, parent,
			COL_UINT_UNREAD, &unread,
			COL_BOOL_IS_STORE, &is_store,
			COL_BOOL_IS_DRAFT, &is_draft,
			-1);

		if (is_draft || is_store) {
			*has_unread = FALSE;
			return;
		}

		*has_unread = *has_unread || (unread > 0 && unread != ~((guint)0));

		if (*has_unread)
			return;

		if (!gtk_tree_model_iter_children (model, &iter, parent))
			return;
	} else {
		iter = *parent;
	}

	do {
		gtk_tree_model_get (model, &iter, COL_UINT_UNREAD, &unread, -1);

		*has_unread = *has_unread || (unread > 0 && unread != ~((guint)0));
		if (*has_unread)
			break;

		if (gtk_tree_model_iter_children (model, &child, &iter))
			has_unread_mail (model, &child, FALSE, has_unread);

	} while (gtk_tree_model_iter_next (model, &iter) && !*has_unread);
}

static void
mail_shell_view_update_actions (EShellView *shell_view)
{
	EMailShellView *mail_shell_view;
	EMailShellContent *mail_shell_content;
	EMailShellSidebar *mail_shell_sidebar;
	EShellSidebar *shell_sidebar;
	EShellWindow *shell_window;
	EMFolderTree *folder_tree;
	EMailReader *reader;
	EAccount *account = NULL;
	GtkAction *action;
	const gchar *label;
	gchar *uri;
	gboolean sensitive;
	guint32 state;

	/* Be descriptive. */
	gboolean account_is_groupwise = FALSE;
	gboolean folder_allows_children;
	gboolean folder_can_be_deleted;
	gboolean folder_is_junk;
	gboolean folder_is_outbox;
	gboolean folder_is_store;
	gboolean folder_is_trash;
	gboolean folder_has_unread_rec = FALSE;
	gboolean folder_tree_and_message_list_agree = TRUE;

	mail_shell_view = E_MAIL_SHELL_VIEW (shell_view);

	shell_window = e_shell_view_get_shell_window (shell_view);

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	reader = E_MAIL_READER (mail_shell_content);
	e_mail_reader_update_actions (reader);

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	state = e_shell_sidebar_check_state (shell_sidebar);

	folder_allows_children =
		(state & E_MAIL_SHELL_SIDEBAR_FOLDER_ALLOWS_CHILDREN);
	folder_can_be_deleted =
		(state & E_MAIL_SHELL_SIDEBAR_FOLDER_CAN_DELETE);
	folder_is_junk =
		(state & E_MAIL_SHELL_SIDEBAR_FOLDER_IS_JUNK);
	folder_is_outbox =
		(state & E_MAIL_SHELL_SIDEBAR_FOLDER_IS_OUTBOX);
	folder_is_store =
		(state & E_MAIL_SHELL_SIDEBAR_FOLDER_IS_STORE);
	folder_is_trash =
		(state & E_MAIL_SHELL_SIDEBAR_FOLDER_IS_TRASH);

	uri = em_folder_tree_get_selected_uri (folder_tree);
	if (uri != NULL) {
		EMFolderTreeModel *model;
		MessageList *message_list;

		/* XXX If the user right-clicks on a folder other than what
		 *     the message list is showing, disable folder rename.
		 *     Between fetching the CamelFolder asynchronously and
		 *     knowing when NOT to move the folder tree selection
		 *     back to where it was to avoid cancelling the inline
		 *     folder tree editing, it's just too hairy to try to
		 *     get right.  So we're punting. */
		message_list = e_mail_reader_get_message_list (reader);
		folder_tree_and_message_list_agree =
			(g_strcmp0 (uri, message_list->folder_uri) == 0);

		account = mail_config_get_account_by_source_url (uri);

		/* FIXME This belongs in a GroupWise plugin. */
		account_is_groupwise =
			(g_strrstr (uri, "groupwise://") != NULL) &&
			account != NULL && account->parent_uid != NULL;

		model = em_folder_tree_model_get_default ();
		if (model) {
			GtkTreeRowReference *reference = em_folder_tree_model_lookup_uri (model, uri);
			if (reference != NULL) {
				GtkTreePath *path = gtk_tree_row_reference_get_path (reference);
				GtkTreeIter iter;

				gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);
				has_unread_mail (GTK_TREE_MODEL (model), &iter, TRUE, &folder_has_unread_rec);
				gtk_tree_path_free (path);
			}
		}

		g_free (uri);
	}

	action = ACTION (MAIL_ACCOUNT_DISABLE);
	sensitive = (account != NULL) && folder_is_store;
	if (account_is_groupwise)
		label = _("Proxy _Logout");
	else
		label = _("_Disable Account");
	gtk_action_set_sensitive (action, sensitive);
	g_object_set (action, "label", label, NULL);

	action = ACTION (MAIL_EMPTY_TRASH);
	sensitive = folder_is_trash;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FLUSH_OUTBOX);
	sensitive = folder_is_outbox;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_COPY);
	sensitive = !folder_is_store;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_DELETE);
	sensitive = !folder_is_store && folder_can_be_deleted;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_MOVE);
	sensitive = !folder_is_store && folder_can_be_deleted;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_NEW);
	sensitive = folder_allows_children;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_PROPERTIES);
	sensitive = !folder_is_store;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_REFRESH);
	sensitive = !folder_is_store;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_RENAME);
	sensitive =
		!folder_is_store && folder_can_be_deleted &&
		folder_tree_and_message_list_agree;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_SELECT_ALL);
	sensitive = !folder_is_store;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_SELECT_THREAD);
	sensitive = !folder_is_store;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_SELECT_SUBTHREAD);
	sensitive = !folder_is_store;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_UNSUBSCRIBE);
	sensitive = !folder_is_store && folder_can_be_deleted;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_MARK_ALL_AS_READ);
	sensitive = folder_has_unread_rec && !folder_is_store;
	gtk_action_set_sensitive (action, sensitive);

	e_mail_shell_view_update_popup_labels (mail_shell_view);
}

static void
mail_shell_view_class_init (EMailShellViewClass *class,
                            GTypeModule *type_module)
{
	GObjectClass *object_class;
	EShellViewClass *shell_view_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMailShellViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = mail_shell_view_dispose;
	object_class->finalize = mail_shell_view_finalize;
	object_class->constructed = mail_shell_view_constructed;

	shell_view_class = E_SHELL_VIEW_CLASS (class);
	shell_view_class->label = _("Mail");
	shell_view_class->icon_name = "evolution-mail";
	shell_view_class->ui_definition = "evolution-mail.ui";
	shell_view_class->ui_manager_id = "org.gnome.evolution.mail";
	shell_view_class->search_options = "/mail-search-options";
	shell_view_class->search_rules = "searchtypes.xml";
	shell_view_class->new_shell_content = e_mail_shell_content_new;
	shell_view_class->new_shell_sidebar = e_mail_shell_sidebar_new;
	shell_view_class->toggled = mail_shell_view_toggled;
	shell_view_class->execute_search = mail_shell_view_execute_search;
	shell_view_class->update_actions = mail_shell_view_update_actions;
}

static void
mail_shell_view_init (EMailShellView *mail_shell_view,
                      EShellViewClass *shell_view_class)
{
	mail_shell_view->priv =
		E_MAIL_SHELL_VIEW_GET_PRIVATE (mail_shell_view);

	e_mail_shell_view_private_init (mail_shell_view, shell_view_class);
}

GType
e_mail_shell_view_get_type (void)
{
	return mail_shell_view_type;
}

void
e_mail_shell_view_register_type (GTypeModule *type_module)
{
	const GTypeInfo type_info = {
		sizeof (EMailShellViewClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) mail_shell_view_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (EMailShellView),
		0,     /* n_preallocs */
		(GInstanceInitFunc) mail_shell_view_init,
		NULL   /* value_table */
	};

	mail_shell_view_type = g_type_module_register_type (
		type_module, E_TYPE_SHELL_VIEW,
		"EMailShellView", &type_info, 0);
}
