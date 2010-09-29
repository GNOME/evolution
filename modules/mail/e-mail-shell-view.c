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
#include "filter/e-filter-input.h"

static gpointer parent_class;
static GType mail_shell_view_type;

/* ETable spec for search results */
static const gchar *SEARCH_RESULTS_STATE =
"<ETableState>"
"  <column source=\"0\"/>"
"  <column source=\"3\"/>"
"  <column source=\"1\"/>"
"  <column source=\"14\"/>"
"  <column source=\"5\"/>"
"  <column source=\"7\"/>"
"  <column source=\"13\"/>"
"  <grouping>"
"    <leaf column=\"7\" ascending=\"false\"/>"
"  </grouping>"
"</ETableState>";

typedef struct {
	MailMsg base;

	CamelFolder *folder;
	CamelOperation *cancel;
	GList *folder_list;
} SearchResultsMsg;

static gchar *
search_results_desc (SearchResultsMsg *msg)
{
	return g_strdup (_("Searching"));
}

static void
search_results_exec (SearchResultsMsg *msg)
{
	GList *copied_list;

	camel_operation_register (msg->cancel);

	copied_list = g_list_copy (msg->folder_list);
	g_list_foreach (copied_list, (GFunc) g_object_ref, NULL);

	camel_vee_folder_set_folders (
		CAMEL_VEE_FOLDER (msg->folder), copied_list);

	g_list_foreach (copied_list, (GFunc) g_object_unref, NULL);
	g_list_free (copied_list);
}

static void
search_results_done (SearchResultsMsg *msg)
{
}

static void
search_results_free (SearchResultsMsg *msg)
{
	g_object_unref (msg->folder);

	g_list_foreach (msg->folder_list, (GFunc) g_object_unref, NULL);
	g_list_free (msg->folder_list);
}

static MailMsgInfo search_results_setup_info = {
	sizeof (SearchResultsMsg),
	(MailMsgDescFunc) search_results_desc,
	(MailMsgExecFunc) search_results_exec,
	(MailMsgDoneFunc) search_results_done,
	(MailMsgFreeFunc) search_results_free
};

static gint
mail_shell_view_setup_search_results_folder (CamelFolder *folder,
                                             GList *folder_list,
                                             CamelOperation *cancel)
{
	SearchResultsMsg *msg;
	gint id;

	g_object_ref (folder);

	msg = mail_msg_new (&search_results_setup_info);
	msg->folder = folder;
	msg->cancel = cancel;
	msg->folder_list = folder_list;

	id = msg->base.seq;
	mail_msg_slow_ordered_push (msg);

	return id;
}

static void
mail_shell_view_show_search_results_folder (EMailShellView *mail_shell_view,
                                            CamelFolder *folder,
                                            const gchar *folder_uri)
{
	EMailShellContent *mail_shell_content;
	GtkWidget *message_list;
	EMailView *mail_view;
	EMailReader *reader;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);
	reader = E_MAIL_READER (mail_view);

	message_list = e_mail_reader_get_message_list (reader);

	message_list_freeze (MESSAGE_LIST (message_list));

	e_mail_reader_set_folder (reader, folder, folder_uri);
	e_tree_set_state (E_TREE (message_list), SEARCH_RESULTS_STATE);

	message_list_thaw (MESSAGE_LIST (message_list));
}

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
		EMailView *mail_view;

		priv->merge_id = e_ui_manager_add_ui_from_file (
			E_UI_MANAGER (ui_manager), basename);
		mail_view = e_mail_shell_content_get_mail_view (
			priv->mail_shell_content);
		e_mail_reader_create_charset_menu (
			E_MAIL_READER (mail_view),
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
	EMailShellContent *mail_shell_content;
	EMailShellSidebar *mail_shell_sidebar;
	EShell *shell;
	EShellWindow *shell_window;
	EShellBackend *shell_backend;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EShellSettings *shell_settings;
	EShellSearchbar *searchbar;
	EActionComboBox *combo_box;
	EMFolderTree *folder_tree;
	GtkTreeSelection *selection;
	GtkWidget *message_list;
	EFilterRule *rule;
	EMailReader *reader;
	EMailView *mail_view;
	CamelVeeFolder *search_folder;
	CamelFolder *folder;
	CamelStore *store;
	GtkAction *action;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter tree_iter;
	GString *string;
	GList *list, *iter;
	GSList *search_strings = NULL;
	const gchar *folder_uri;
	const gchar *data_dir;
	const gchar *text;
	gboolean valid;
	gchar *query;
	gchar *temp;
	gchar *tag;
	gchar *uri;
	const gchar *use_tag;
	gint value;

	priv = E_MAIL_SHELL_VIEW_GET_PRIVATE (shell_view);

	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	shell = e_shell_window_get_shell (shell_window);
	shell_settings = e_shell_get_shell_settings (shell);

	mail_shell_content = E_MAIL_SHELL_CONTENT (shell_content);
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);
	searchbar = e_mail_shell_content_get_searchbar (mail_shell_content);

	mail_shell_sidebar = E_MAIL_SHELL_SIDEBAR (shell_sidebar);
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (folder_tree));

	reader = E_MAIL_READER (mail_view);
	folder = e_mail_reader_get_folder (reader);
	folder_uri = e_mail_reader_get_folder_uri (reader);
	message_list = e_mail_reader_get_message_list (reader);

	data_dir = e_shell_backend_get_data_dir (shell_backend);

	/* This returns a new object reference. */
	model = e_shell_settings_get_object (
		shell_settings, "mail-label-list-store");

	action = ACTION (MAIL_SEARCH_SUBJECT_OR_ADDRESSES_CONTAIN);
	value = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (action));

	text = e_shell_searchbar_get_search_text (searchbar);
	if (value == MAIL_SEARCH_ADVANCED || text == NULL || *text == '\0') {
		if (value != MAIL_SEARCH_ADVANCED)
			e_shell_view_set_search_rule (shell_view, NULL);

		query = e_shell_view_get_search_query (shell_view);

		if (!query)
			query = g_strdup ("");

		goto filter;
	}

	/* Replace variables in the selected rule with the
	 * current search text and extract a query string. */

	g_return_if_fail (value >= 0 && value < MAIL_NUM_SEARCH_RULES);
	rule = priv->search_rules[value];

	/* Set the search rule in EShellView so that "Create
	 * Search Folder from Search" works for quick searches. */
	e_shell_view_set_search_rule (shell_view, rule);

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

	combo_box = e_shell_searchbar_get_filter_combo_box (searchbar);
	value = e_action_combo_box_get_current_value (combo_box);
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
				use_tag = tag;
				if (g_str_has_prefix (use_tag, "$Label"))
					use_tag += 6;
				g_string_append_printf (
					string, " (match-all (not (or "
					"(= (user-tag \"label\") \"%s\") "
					"(user-flag \"$Label%s\") "
					"(user-flag \"%s\"))))",
					use_tag, use_tag, use_tag);
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
			use_tag = tag;
			if (g_str_has_prefix (use_tag, "$Label"))
				use_tag += 6;
			temp = g_strdup_printf (
				"(and %s (match-all (or "
				"(= (user-tag \"label\") \"%s\") "
				"(user-flag \"$Label%s\") "
				"(user-flag \"%s\"))))",
				query, use_tag, use_tag, use_tag);
			g_free (tag);

			g_free (query);
			query = temp;
			break;
	}

	/* Apply selected scope. */

	combo_box = e_shell_searchbar_get_scope_combo_box (searchbar);
	value = e_action_combo_box_get_current_value (combo_box);
	switch (value) {
		case MAIL_SCOPE_CURRENT_FOLDER:
			goto execute;

		case MAIL_SCOPE_CURRENT_ACCOUNT:
			goto current_account;

		case MAIL_SCOPE_ALL_ACCOUNTS:
			goto all_accounts;

		default:
			g_warn_if_reached ();
			goto execute;
	}

all_accounts:

	/* Prepare search folder for all accounts. */

	/* If the search text is empty, cancel any
	 * account-wide searches still in progress. */
	text = e_shell_searchbar_get_search_text (searchbar);
	if (text == NULL || *text == '\0') {
		if (priv->search_account_all != NULL) {
			g_object_unref (priv->search_account_all);
			priv->search_account_all = NULL;
		}

		if (priv->search_account_cancel != NULL) {
			camel_operation_cancel (priv->search_account_cancel);
			camel_operation_unref (priv->search_account_cancel);
			priv->search_account_cancel = NULL;
		}

		/* Reset the message list to the current folder tree
		 * selection.  This needs to happen synchronously to
		 * avoid search conflicts, so we can't just grab the
		 * folder URI and let the asynchronous callbacks run
		 * after we've already kicked off the search. */
		folder = em_folder_tree_get_selected_folder (folder_tree);
		uri = em_folder_tree_get_selected_uri (folder_tree);
		e_mail_reader_set_folder (reader, folder, uri);
		g_free (uri);

		gtk_widget_set_sensitive (GTK_WIDGET (combo_box), TRUE);

		goto execute;
	}

	search_folder = priv->search_account_all;

	/* Skip the search if we already have the results. */
	if (search_folder != NULL)
		if (g_strcmp0 (query, search_folder->expression) == 0)
			goto execute;

	/* Disable the scope combo while search is in progress. */
	gtk_widget_set_sensitive (GTK_WIDGET (combo_box), FALSE);

	/* If we already have a search folder, reuse it. */
	if (search_folder != NULL) {
		if (priv->search_account_cancel != NULL) {
			camel_operation_cancel (priv->search_account_cancel);
			camel_operation_unref (priv->search_account_cancel);
			priv->search_account_cancel = NULL;
		}

		camel_vee_folder_set_expression (search_folder, query);

		goto execute;
	}

	/* Create a new search folder. */

	list = NULL;  /* list of CamelFolders */

	/* FIXME Using data_dir like this is not portable. */
	uri = g_strdup_printf ("vfolder:%s/vfolder", data_dir);
	store = camel_session_get_store (session, uri, NULL);
	g_free (uri);

	search_folder = (CamelVeeFolder *) camel_vee_folder_new (
		store, _("All Account Search"), CAMEL_STORE_VEE_FOLDER_AUTO);
	priv->search_account_all = search_folder;

	/* Add local folders. */
	iter = mail_vfolder_get_sources_local ();
	while (iter != NULL) {
		folder_uri = iter->data;
		folder = mail_tool_uri_to_folder (folder_uri, 0, NULL);

		if (folder != NULL)
			list = g_list_append (list, folder);
		else
			g_warning ("Could not open vfolder source: %s", folder_uri);

		iter = g_list_next (iter);
	}

	/* Add remote folders. */
	iter = mail_vfolder_get_sources_remote ();
	while (iter != NULL) {
		folder_uri = iter->data;
		folder = mail_tool_uri_to_folder (folder_uri, 0, NULL);

		if (folder != NULL)
			list = g_list_append (list, folder);
		else
			g_warning ("Could not open vfolder source: %s", folder_uri);

		iter = g_list_next (iter);
	}

	camel_vee_folder_set_expression (search_folder, query);

	priv->search_account_cancel = camel_operation_new (NULL, NULL);

	/* This takes ownership of the folder list. */
	mail_shell_view_setup_search_results_folder (
		CAMEL_FOLDER (search_folder), list,
		priv->search_account_cancel);

	uri = mail_tools_folder_to_url (CAMEL_FOLDER (search_folder));

	mail_shell_view_show_search_results_folder (
		E_MAIL_SHELL_VIEW (shell_view),
		CAMEL_FOLDER (search_folder), uri);

	g_free (uri);

	goto execute;

current_account:

	/* Prepare search folder for current account only. */

	/* If the search text is empty, cancel any
	 * account-wide searches still in progress. */
	text = e_shell_searchbar_get_search_text (searchbar);
	if (text == NULL || *text == '\0') {
		if (priv->search_account_current != NULL) {
			g_object_unref (priv->search_account_current);
			priv->search_account_current = NULL;
		}

		if (priv->search_account_cancel != NULL) {
			camel_operation_cancel (priv->search_account_cancel);
			camel_operation_unref (priv->search_account_cancel);
			priv->search_account_cancel = NULL;
		}

		/* Reset the message list to the current folder tree
		 * selection.  This needs to happen synchronously to
		 * avoid search conflicts, so we can't just grab the
		 * folder URI and let the asynchronous callbacks run
		 * after we've already kicked off the search. */
		folder = em_folder_tree_get_selected_folder (folder_tree);
		uri = em_folder_tree_get_selected_uri (folder_tree);
		e_mail_reader_set_folder (reader, folder, uri);
		g_free (uri);

		gtk_widget_set_sensitive (GTK_WIDGET (combo_box), TRUE);

		goto execute;
	}

	search_folder = priv->search_account_current;

	/* Skip the search if we already have the results. */
	if (search_folder != NULL)
		if (g_strcmp0 (query, search_folder->expression) == 0)
			goto execute;

	/* Disable the scope combo while search is in progress. */
	gtk_widget_set_sensitive (GTK_WIDGET (combo_box), FALSE);

	/* If we already have a search folder, reuse it. */
	if (search_folder != NULL) {
		if (priv->search_account_cancel != NULL) {
			camel_operation_cancel (priv->search_account_cancel);
			camel_operation_unref (priv->search_account_cancel);
			priv->search_account_cancel = NULL;
		}

		camel_vee_folder_set_expression (search_folder, query);

		goto execute;
	}

	/* Create a new search folder. */

	if (folder) {
		store = camel_folder_get_parent_store (folder);
	} else {
		GtkTreeView *tree_view;
		GtkTreeSelection *selection;
		GtkTreeModel *model;
		GtkTreeIter iter;
	
		store = NULL;
		tree_view = GTK_TREE_VIEW (folder_tree);
		selection = gtk_tree_view_get_selection (tree_view);

		if (gtk_tree_selection_get_selected (selection, &model, &iter))
			gtk_tree_model_get (model, &iter, COL_POINTER_CAMEL_STORE, &store, -1);
	}

	list = NULL;  /* list of CamelFolders */

	if (store) {
		CamelFolderInfo *root, *fi;

		root = camel_store_get_folder_info (store, NULL, CAMEL_STORE_FOLDER_INFO_RECURSIVE, NULL);
		fi = root;
		while (fi) {
			CamelFolderInfo *next;

			if ((fi->flags & CAMEL_FOLDER_NOSELECT) == 0) {
				CamelFolder *fldr;

				fldr = camel_store_get_folder (store, fi->full_name, 0, NULL);
				if (fldr)
					list = g_list_prepend (list, fldr);
			}

			/* pick the next */
			next = fi->child;
			if (!next)
				next = fi->next;
			if (!next) {
				next = fi->parent;
				while (next) {
					if (next->next) {
						next = next->next;
						break;
					}

					next = next->parent;
				}
			}

			fi = next;
		}

		if (root)
			camel_store_free_folder_info_full (store, root);
	}

	list = g_list_reverse (list);

	/* FIXME Using data_dir like this is not portable. */
	uri = g_strdup_printf ("vfolder:%s/vfolder", data_dir);
	store = camel_session_get_store (session, uri, NULL);
	g_free (uri);

	search_folder = (CamelVeeFolder *) camel_vee_folder_new (
		store, _("Account Search"), CAMEL_STORE_VEE_FOLDER_AUTO);
	priv->search_account_current = search_folder;

	camel_vee_folder_set_expression (search_folder, query);

	priv->search_account_cancel = camel_operation_new (NULL, NULL);

	/* This takes ownership of the folder list. */
	mail_shell_view_setup_search_results_folder (
		CAMEL_FOLDER (search_folder), list,
		priv->search_account_cancel);

	uri = mail_tools_folder_to_url (CAMEL_FOLDER (search_folder));

	mail_shell_view_show_search_results_folder (
		E_MAIL_SHELL_VIEW (shell_view),
		CAMEL_FOLDER (search_folder), uri);

	g_free (uri);

execute:

	/* Finally, execute the search. */

	message_list_set_search (MESSAGE_LIST (message_list), query);

	e_mail_view_set_search_strings (mail_view, search_strings);

	g_slist_foreach (search_strings, (GFunc) g_free, NULL);
	g_slist_free (search_strings);

	g_object_unref (model);
	g_free (query);
}

static void
has_unread_mail (GtkTreeModel *model,
                 GtkTreeIter *parent,
                 gboolean is_root,
                 gboolean *has_unread)
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
	EMailView *mail_view;
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
	gboolean folder_is_outbox;
	gboolean folder_is_store;
	gboolean folder_is_trash;
	gboolean folder_has_unread_rec = FALSE;
	gboolean folder_tree_and_message_list_agree = TRUE;
	gboolean have_enabled_account;

	/* Chain up to parent's update_actions() method. */
	E_SHELL_VIEW_CLASS (parent_class)->update_actions (shell_view);

	mail_shell_view = E_MAIL_SHELL_VIEW (shell_view);

	shell_window = e_shell_view_get_shell_window (shell_view);

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	state = e_mail_reader_check_state (reader);
	e_mail_reader_update_actions (reader, state);

	have_enabled_account =
		(state & E_MAIL_READER_HAVE_ENABLED_ACCOUNT);

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	state = e_shell_sidebar_check_state (shell_sidebar);

	folder_allows_children =
		(state & E_MAIL_SIDEBAR_FOLDER_ALLOWS_CHILDREN);
	folder_can_be_deleted =
		(state & E_MAIL_SIDEBAR_FOLDER_CAN_DELETE);
	folder_is_outbox =
		(state & E_MAIL_SIDEBAR_FOLDER_IS_OUTBOX);
	folder_is_store =
		(state & E_MAIL_SIDEBAR_FOLDER_IS_STORE);
	folder_is_trash =
		(state & E_MAIL_SIDEBAR_FOLDER_IS_TRASH);

	uri = em_folder_tree_get_selected_uri (folder_tree);
	if (uri != NULL) {
		GtkTreeRowReference *reference;
		EMFolderTreeModel *model;
		const gchar *folder_uri;

		folder_uri = e_mail_reader_get_folder_uri (reader);

		/* XXX If the user right-clicks on a folder other than what
		 *     the message list is showing, disable folder rename.
		 *     Between fetching the CamelFolder asynchronously and
		 *     knowing when NOT to move the folder tree selection
		 *     back to where it was to avoid cancelling the inline
		 *     folder tree editing, it's just too hairy to try to
		 *     get right.  So we're punting. */
		folder_tree_and_message_list_agree =
			(g_strcmp0 (uri, folder_uri) == 0);

		account = mail_config_get_account_by_source_url (uri);

		/* FIXME This belongs in a GroupWise plugin. */
		account_is_groupwise =
			(g_strrstr (uri, "groupwise://") != NULL) &&
			account != NULL && account->parent_uid != NULL;

		model = em_folder_tree_model_get_default ();
		reference = em_folder_tree_model_lookup_uri (model, uri);
		if (reference != NULL) {
			GtkTreePath *path;
			GtkTreeIter iter;

			path = gtk_tree_row_reference_get_path (reference);
			gtk_tree_model_get_iter (
				GTK_TREE_MODEL (model), &iter, path);
			has_unread_mail (
				GTK_TREE_MODEL (model), &iter,
				TRUE, &folder_has_unread_rec);
			gtk_tree_path_free (path);
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

	action = ACTION (MAIL_ACCOUNT_EXPUNGE);
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

	action = ACTION (MAIL_TOOLS_SUBSCRIPTIONS);
	sensitive = have_enabled_account;
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
	shell_view_class->search_context_type = EM_SEARCH_TYPE_CONTEXT;
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
