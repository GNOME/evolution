/*
 * e-mail-shell-view.c
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

#include "e-mail-shell-view-private.h"

enum {
	PROP_0,
	PROP_VFOLDER_ALLOW_EXPUNGE
};

G_DEFINE_DYNAMIC_TYPE (
	EMailShellView,
	e_mail_shell_view,
	E_TYPE_SHELL_VIEW)

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

static void
add_folders_from_store (GList **folders,
                        CamelStore *store,
                        GCancellable *cancellable,
                        GError **error)
{
	CamelFolderInfo *root, *fi;

	g_return_if_fail (folders != NULL);
	g_return_if_fail (store != NULL);

	if (CAMEL_IS_VEE_STORE (store))
		return;

	root = camel_store_get_folder_info_sync (
		store, NULL,
		CAMEL_STORE_FOLDER_INFO_RECURSIVE, cancellable, error);
	fi = root;
	while (fi && !g_cancellable_is_cancelled (cancellable)) {
		CamelFolderInfo *next;

		if ((fi->flags & CAMEL_FOLDER_NOSELECT) == 0) {
			CamelFolder *fldr;

			fldr = camel_store_get_folder_sync (
				store, fi->full_name, 0, cancellable, error);
			if (fldr) {
				if (CAMEL_IS_VEE_FOLDER (fldr)) {
					g_object_unref (fldr);
				} else {
					*folders = g_list_prepend (*folders, fldr);
				}
			}
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

	camel_folder_info_free (root);
}

typedef struct {
	MailMsg base;

	CamelFolder *folder;
	GCancellable *cancellable;
	GList *stores_list;
} SearchResultsMsg;

static gchar *
search_results_desc (SearchResultsMsg *msg)
{
	return g_strdup (_("Searching"));
}

static void
search_results_exec (SearchResultsMsg *msg,
                     GCancellable *cancellable,
                     GError **error)
{
	GList *folders = NULL, *link;

	for (link = msg->stores_list; link != NULL; link = link->next) {
		CamelStore *store = CAMEL_STORE (link->data);

		if (g_cancellable_is_cancelled (cancellable))
			break;

		add_folders_from_store (&folders, store, cancellable, error);
	}

	if (!g_cancellable_is_cancelled (cancellable)) {
		CamelVeeFolder *vfolder = CAMEL_VEE_FOLDER (msg->folder);

		folders = g_list_reverse (folders);

		camel_vee_folder_set_folders (vfolder, folders, cancellable);
	}

	g_list_free_full (folders, g_object_unref);
}

static void
search_results_done (SearchResultsMsg *msg)
{
}

static void
search_results_free (SearchResultsMsg *msg)
{
	g_object_unref (msg->folder);
	g_list_free_full (msg->stores_list, g_object_unref);
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
                                             GList *stores,
                                             GCancellable *cancellable)
{
	SearchResultsMsg *msg;
	gint id;

	g_object_ref (folder);

	msg = mail_msg_new (&search_results_setup_info);
	msg->folder = folder;
	msg->cancellable = cancellable;
	msg->stores_list = stores;

	id = msg->base.seq;
	mail_msg_slow_ordered_push (msg);

	return id;
}

static void
mail_shell_view_show_search_results_folder (EMailShellView *mail_shell_view,
                                            CamelFolder *folder)
{
	EMailShellContent *mail_shell_content;
	GtkWidget *message_list;
	EMailView *mail_view;
	EMailReader *reader;
	GalViewInstance *view_instance;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);
	reader = E_MAIL_READER (mail_view);

	message_list = e_mail_reader_get_message_list (reader);

	message_list_freeze (MESSAGE_LIST (message_list));

	e_mail_reader_set_folder (reader, folder);
	view_instance = e_mail_view_get_view_instance (mail_view);

	if (!view_instance || !gal_view_instance_exists (view_instance)) {
		ETree *tree;
		ETableState *state;
		ETableSpecification *specification;

		tree = E_TREE (message_list);
		specification = e_tree_get_spec (tree);
		state = e_table_state_new (specification);
		e_table_state_load_from_string (state, SEARCH_RESULTS_STATE);
		e_tree_set_state_object (tree, state);
		g_object_unref (state);
	}

	message_list_thaw (MESSAGE_LIST (message_list));
}

static void
mail_shell_view_set_vfolder_allow_expunge (EMailShellView *mail_shell_view,
					   gboolean value)
{
	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	if ((mail_shell_view->priv->vfolder_allow_expunge ? 1 : 0) == (value ? 1 : 0))
		return;

	mail_shell_view->priv->vfolder_allow_expunge = value;

	g_object_notify (G_OBJECT (mail_shell_view), "vfolder-allow-expunge");
}

static gboolean
mail_shell_view_get_vfolder_allow_expunge (EMailShellView *mail_shell_view)
{
	g_return_val_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view), FALSE);

	return mail_shell_view->priv->vfolder_allow_expunge;
}

static void
mail_shell_view_set_property (GObject *object,
			      guint property_id,
			      const GValue *value,
			      GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_VFOLDER_ALLOW_EXPUNGE:
			mail_shell_view_set_vfolder_allow_expunge (
				E_MAIL_SHELL_VIEW (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_shell_view_get_property (GObject *object,
			      guint property_id,
			      GValue *value,
			      GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_VFOLDER_ALLOW_EXPUNGE:
			g_value_set_boolean (
				value,
				mail_shell_view_get_vfolder_allow_expunge (
				E_MAIL_SHELL_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_shell_view_dispose (GObject *object)
{
	e_mail_shell_view_private_dispose (E_MAIL_SHELL_VIEW (object));

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_shell_view_parent_class)->dispose (object);
}

static void
mail_shell_view_finalize (GObject *object)
{
	e_mail_shell_view_private_finalize (E_MAIL_SHELL_VIEW (object));

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_shell_view_parent_class)->finalize (object);
}

static void
mail_shell_view_constructed (GObject *object)
{
	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_shell_view_parent_class)->constructed (object);

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

		priv->merge_id = e_load_ui_manager_definition (
			ui_manager, basename);
		mail_view = e_mail_shell_content_get_mail_view (
			priv->mail_shell_content);
		e_mail_reader_create_charset_menu (
			E_MAIL_READER (mail_view),
			ui_manager, priv->merge_id);
	} else if (!view_is_active && priv->merge_id != 0) {
		gtk_ui_manager_remove_ui (ui_manager, priv->merge_id);
		gtk_ui_manager_ensure_update (ui_manager);
		priv->merge_id = 0;
	}

	/* Chain up to parent's toggled() method. */
	E_SHELL_VIEW_CLASS (e_mail_shell_view_parent_class)->
		toggled (shell_view);
}

static gchar *
mail_shell_view_construct_filter_message_thread (EMailShellView *mail_shell_view,
						 const gchar *with_query)
{
	EMailShellViewPrivate *priv;
	GString *query;
	GSList *link;

	g_return_val_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view), NULL);

	priv = E_MAIL_SHELL_VIEW_GET_PRIVATE (mail_shell_view);

	if (!priv->selected_uids) {
		EShellContent *shell_content;
		EMailView *mail_view;
		GPtrArray *uids;

		shell_content = e_shell_view_get_shell_content (E_SHELL_VIEW (mail_shell_view));
		mail_view = e_mail_shell_content_get_mail_view (E_MAIL_SHELL_CONTENT (shell_content));
		uids = e_mail_reader_get_selected_uids (E_MAIL_READER (mail_view));

		if (uids) {
			gint ii;

			for (ii = 0; ii < uids->len; ii++) {
				priv->selected_uids = g_slist_prepend (priv->selected_uids, (gpointer) camel_pstring_strdup (uids->pdata[ii]));
			}

			g_ptr_array_unref (uids);
		}

		if (!priv->selected_uids)
			priv->selected_uids = g_slist_prepend (priv->selected_uids, (gpointer) camel_pstring_strdup (""));
	}

	query = g_string_new ("");

	if (with_query)
		g_string_append_printf (query, "(and %s ", with_query);

	g_string_append (query, "(match-threads \"all\" (match-all (uid");

	for (link = priv->selected_uids; link; link = g_slist_next (link)) {
		const gchar *uid = link->data;

		g_string_append_c (query, ' ');
		g_string_append_c (query, '\"');
		g_string_append (query, uid);
		g_string_append_c (query, '\"');
	}

	g_string_append (query, ")))");

	if (with_query)
		g_string_append (query, ")");

	return g_string_free (query, FALSE);
}

static void
mail_shell_view_execute_search (EShellView *shell_view)
{
	EMailShellViewPrivate *priv;
	EMailShellContent *mail_shell_content;
	EMailShellSidebar *mail_shell_sidebar;
	EShellWindow *shell_window;
	EShellBackend *shell_backend;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EShellSearchbar *searchbar;
	EActionComboBox *combo_box;
	EMailBackend *backend;
	EMailSession *session;
	ESourceRegistry *registry;
	EMFolderTree *folder_tree;
	GtkWidget *message_list;
	EFilterRule *rule;
	EMailReader *reader;
	EMailView *mail_view;
	CamelVeeFolder *search_folder;
	CamelFolder *folder;
	CamelService *service;
	CamelStore *store;
	GtkAction *action;
	EMailLabelListStore *label_store;
	GtkTreePath *path;
	GtkTreeIter tree_iter;
	GString *string;
	GList *list, *iter;
	GSList *search_strings = NULL;
	const gchar *text;
	gboolean valid;
	gchar *query;
	gchar *temp;
	gchar *tag;
	const gchar *use_tag;
	gint value;

	priv = E_MAIL_SHELL_VIEW_GET_PRIVATE (shell_view);

	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);

	mail_shell_content = E_MAIL_SHELL_CONTENT (shell_content);
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);
	searchbar = e_mail_shell_content_get_searchbar (mail_shell_content);

	mail_shell_sidebar = E_MAIL_SHELL_SIDEBAR (shell_sidebar);
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	reader = E_MAIL_READER (mail_view);
	folder = e_mail_reader_ref_folder (reader);
	message_list = e_mail_reader_get_message_list (reader);

	registry = e_mail_session_get_registry (session);
	label_store = e_mail_ui_session_get_label_store (
		E_MAIL_UI_SESSION (session));

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
		else if (strcmp (part->name, "mail-free-form-exp") == 0)
			element = e_filter_part_find_element (part, "ffe");

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

	if (value != MAIL_FILTER_MESSAGE_THREAD) {
		g_slist_free_full (priv->selected_uids, (GDestroyNotify) camel_pstring_free);
		priv->selected_uids = NULL;
	}

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
				GTK_TREE_MODEL (label_store), &tree_iter);
			while (valid) {
				tag = e_mail_label_list_store_get_tag (
					label_store, &tree_iter);
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
					GTK_TREE_MODEL (label_store),
					&tree_iter);
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

		case MAIL_FILTER_LAST_5_DAYS_MESSAGES:
			if (em_utils_folder_is_sent (registry, folder))
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

		case MAIL_FILTER_MESSAGES_WITH_NOTES:
			temp = g_strdup_printf (
				"(and %s (match-all (user-flag \"$has_note\")))", query);
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

		case MAIL_FILTER_MESSAGE_THREAD:
			temp = mail_shell_view_construct_filter_message_thread (
				E_MAIL_SHELL_VIEW (shell_view), query);
			g_free (query);
			query = temp;
			break;

		default:
			/* The action value also serves as a path for
			 * the label list store.  That's why we number
			 * the label actions from zero. */
			path = gtk_tree_path_new_from_indices (value, -1);
			gtk_tree_model_get_iter (
				GTK_TREE_MODEL (label_store),
				&tree_iter, path);
			gtk_tree_path_free (path);

			tag = e_mail_label_list_store_get_tag (
				label_store, &tree_iter);
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
	if ((text == NULL || *text == '\0') && !e_shell_view_get_search_rule (shell_view)) {
		CamelStore *selected_store = NULL;
		gchar *selected_folder_name = NULL;

		if (priv->search_account_all != NULL) {
			g_object_unref (priv->search_account_all);
			priv->search_account_all = NULL;
		}

		if (priv->search_account_cancel != NULL) {
			g_cancellable_cancel (priv->search_account_cancel);
			g_object_unref (priv->search_account_cancel);
			priv->search_account_cancel = NULL;
		}

		/* Reset the message list to the current folder tree
		 * selection.  This needs to happen synchronously to
		 * avoid search conflicts, so we can't just grab the
		 * folder URI and let the asynchronous callbacks run
		 * after we've already kicked off the search. */
		em_folder_tree_get_selected (
			folder_tree, &selected_store, &selected_folder_name);
		if (selected_store != NULL && selected_folder_name != NULL) {
			folder = camel_store_get_folder_sync (
				selected_store, selected_folder_name,
				0, NULL, NULL);
			e_mail_reader_set_folder (reader, folder);
			g_object_unref (folder);
		}

		g_clear_object (&selected_store);
		g_free (selected_folder_name);

		gtk_widget_set_sensitive (GTK_WIDGET (combo_box), TRUE);

		goto execute;
	}

	search_folder = priv->search_account_all;

	/* Skip the search if we already have the results. */
	if (search_folder != NULL) {
		const gchar *vf_query;

		vf_query = camel_vee_folder_get_expression (search_folder);
		if (g_strcmp0 (query, vf_query) == 0)
			goto all_accounts_setup;
	}

	/* Disable the scope combo while search is in progress. */
	gtk_widget_set_sensitive (GTK_WIDGET (combo_box), FALSE);

	/* If we already have a search folder, reuse it. */
	if (search_folder != NULL) {
		if (priv->search_account_cancel != NULL) {
			g_cancellable_cancel (priv->search_account_cancel);
			g_object_unref (priv->search_account_cancel);
			priv->search_account_cancel = NULL;
		}

		camel_vee_folder_set_expression (search_folder, query);

		goto all_accounts_setup;
	}

	/* Create a new search folder. */

	/* FIXME Complete lack of error checking here. */
	service = camel_session_ref_service (
		CAMEL_SESSION (session), E_MAIL_SESSION_VFOLDER_UID);
	camel_service_connect_sync (service, NULL, NULL);

	search_folder = (CamelVeeFolder *) camel_vee_folder_new (
		CAMEL_STORE (service),
		_("All Account Search"),
		CAMEL_STORE_FOLDER_PRIVATE);
	priv->search_account_all = search_folder;

	g_object_unref (service);

	camel_vee_folder_set_expression (search_folder, query);

all_accounts_setup:

	list = em_folder_tree_model_list_stores (EM_FOLDER_TREE_MODEL (
		gtk_tree_view_get_model (GTK_TREE_VIEW (folder_tree))));
	g_list_foreach (list, (GFunc) g_object_ref, NULL);

	priv->search_account_cancel = camel_operation_new ();

	/* This takes ownership of the stores list. */
	mail_shell_view_setup_search_results_folder (
		CAMEL_FOLDER (search_folder), list,
		priv->search_account_cancel);

	mail_shell_view_show_search_results_folder (
		E_MAIL_SHELL_VIEW (shell_view),
		CAMEL_FOLDER (search_folder));

	goto execute;

current_account:

	/* Prepare search folder for current account only. */

	/* If the search text is empty, cancel any
	 * account-wide searches still in progress. */
	text = e_shell_searchbar_get_search_text (searchbar);
	if ((text == NULL || *text == '\0') && !e_shell_view_get_search_rule (shell_view)) {
		CamelStore *selected_store = NULL;
		gchar *selected_folder_name = NULL;

		if (priv->search_account_current != NULL) {
			g_object_unref (priv->search_account_current);
			priv->search_account_current = NULL;
		}

		if (priv->search_account_cancel != NULL) {
			g_cancellable_cancel (priv->search_account_cancel);
			g_object_unref (priv->search_account_cancel);
			priv->search_account_cancel = NULL;
		}

		/* Reset the message list to the current folder tree
		 * selection.  This needs to happen synchronously to
		 * avoid search conflicts, so we can't just grab the
		 * folder URI and let the asynchronous callbacks run
		 * after we've already kicked off the search. */
		em_folder_tree_get_selected (
			folder_tree, &selected_store, &selected_folder_name);
		if (selected_store != NULL && selected_folder_name != NULL) {
			folder = camel_store_get_folder_sync (
				selected_store, selected_folder_name,
				0, NULL, NULL);
			e_mail_reader_set_folder (reader, folder);
			g_object_unref (folder);
		}

		g_clear_object (&selected_store);
		g_free (selected_folder_name);

		gtk_widget_set_sensitive (GTK_WIDGET (combo_box), TRUE);

		goto execute;
	}

	search_folder = priv->search_account_current;

	/* Skip the search if we already have the results. */
	if (search_folder != NULL) {
		const gchar *vf_query;

		vf_query = camel_vee_folder_get_expression (search_folder);
		if (g_strcmp0 (query, vf_query) == 0)
			goto current_accout_setup;
	}

	/* Disable the scope combo while search is in progress. */
	gtk_widget_set_sensitive (GTK_WIDGET (combo_box), FALSE);

	/* If we already have a search folder, reuse it. */
	if (search_folder != NULL) {
		if (priv->search_account_cancel != NULL) {
			g_cancellable_cancel (priv->search_account_cancel);
			g_object_unref (priv->search_account_cancel);
			priv->search_account_cancel = NULL;
		}

		camel_vee_folder_set_expression (search_folder, query);

		goto current_accout_setup;
	}

	/* Create a new search folder. */

	/* FIXME Complete lack of error checking here. */
	service = camel_session_ref_service (
		CAMEL_SESSION (session), E_MAIL_SESSION_VFOLDER_UID);
	camel_service_connect_sync (service, NULL, NULL);

	search_folder = (CamelVeeFolder *) camel_vee_folder_new (
		CAMEL_STORE (service),
		_("Account Search"),
		CAMEL_STORE_FOLDER_PRIVATE);
	priv->search_account_current = search_folder;

	g_object_unref (service);

	camel_vee_folder_set_expression (search_folder, query);

current_accout_setup:

	if (folder != NULL && folder != CAMEL_FOLDER (search_folder)) {
		store = camel_folder_get_parent_store (folder);
		if (store != NULL)
			g_object_ref (store);
	} else {
		store = NULL;
		em_folder_tree_get_selected (folder_tree, &store, NULL);
	}

	list = NULL;  /* list of CamelStore-s */

	if (store != NULL)
		list = g_list_append (NULL, store);

	priv->search_account_cancel = camel_operation_new ();

	/* This takes ownership of the stores list. */
	mail_shell_view_setup_search_results_folder (
		CAMEL_FOLDER (search_folder), list,
		priv->search_account_cancel);

	mail_shell_view_show_search_results_folder (
		E_MAIL_SHELL_VIEW (shell_view),
		CAMEL_FOLDER (search_folder));

execute:

	/* Finally, execute the search. */

	message_list_set_search (MESSAGE_LIST (message_list), query);

	e_mail_view_set_search_strings (mail_view, search_strings);

	g_slist_foreach (search_strings, (GFunc) g_free, NULL);
	g_slist_free (search_strings);

	g_free (query);

	g_clear_object (&folder);
}

static void
has_unread_mail (GtkTreeModel *model,
                 GtkTreeIter *parent,
                 gboolean is_root,
                 gboolean *has_unread_root,
                 gboolean *has_unread)
{
	guint unread = 0;
	GtkTreeIter iter, child;

	g_return_if_fail (model != NULL);
	g_return_if_fail (parent != NULL);
	g_return_if_fail (has_unread != NULL);

	if (is_root) {
		gboolean is_store = FALSE, is_draft = FALSE;

		gtk_tree_model_get (
			model, parent,
			COL_UINT_UNREAD, &unread,
			COL_BOOL_IS_STORE, &is_store,
			COL_BOOL_IS_DRAFT, &is_draft,
			-1);

		if (is_draft || is_store) {
			*has_unread = FALSE;
			return;
		}

		*has_unread = *has_unread || (unread > 0 && unread != ~((guint)0));

		if (*has_unread) {
			if (has_unread_root)
				*has_unread_root = TRUE;
			return;
		}

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
			has_unread_mail (model, &child, FALSE, NULL, has_unread);

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
	EMFolderTreeModel *model;
	EMailReader *reader;
	EMailView *mail_view;
	GtkAction *action;
	CamelStore *store = NULL;
	GList *list, *link;
	gchar *folder_name = NULL;
	gboolean sensitive;
	guint32 state;

	/* Be descriptive. */
	gboolean folder_allows_children;
	gboolean folder_can_be_deleted;
	gboolean folder_is_outbox;
	gboolean folder_is_selected = FALSE;
	gboolean folder_is_store;
	gboolean folder_is_trash;
	gboolean folder_is_virtual;
	gboolean folder_has_unread = FALSE;
	gboolean folder_has_unread_rec = FALSE;
	gboolean folder_tree_and_message_list_agree = TRUE;
	gboolean store_is_builtin;
	gboolean store_is_subscribable;
	gboolean store_can_be_disabled;
	gboolean any_store_is_subscribable = FALSE;

	/* Chain up to parent's update_actions() method. */
	E_SHELL_VIEW_CLASS (e_mail_shell_view_parent_class)->
		update_actions (shell_view);

	shell_window = e_shell_view_get_shell_window (shell_view);

	mail_shell_view = E_MAIL_SHELL_VIEW (shell_view);
	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	state = e_mail_reader_check_state (reader);
	e_mail_reader_update_actions (reader, state);

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	state = e_shell_sidebar_check_state (shell_sidebar);

	model = em_folder_tree_model_get_default ();

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
	folder_is_virtual =
		(state & E_MAIL_SIDEBAR_FOLDER_IS_VIRTUAL);
	store_is_builtin =
		(state & E_MAIL_SIDEBAR_STORE_IS_BUILTIN);
	store_is_subscribable =
		(state & E_MAIL_SIDEBAR_STORE_IS_SUBSCRIBABLE);
	store_can_be_disabled =
		(state & E_MAIL_SIDEBAR_STORE_CAN_BE_DISABLED);

	if (em_folder_tree_get_selected (folder_tree, &store, &folder_name)) {
		GtkTreeRowReference *reference;
		CamelFolder *folder;

		folder_is_selected = TRUE;

		folder = e_mail_reader_ref_folder (reader);

		/* XXX If the user right-clicks on a folder other than what
		 *     the message list is showing, disable folder rename.
		 *     Between fetching the CamelFolder asynchronously and
		 *     knowing when NOT to move the folder tree selection
		 *     back to where it was to avoid cancelling the inline
		 *     folder tree editing, it's just too hairy to try to
		 *     get right.  So we're punting. */
		if (folder != NULL) {
			gchar *uri1, *uri2;

			uri1 = e_mail_folder_uri_from_folder (folder);
			uri2 = e_mail_folder_uri_build (store, folder_name);

			folder_tree_and_message_list_agree =
				(g_strcmp0 (uri1, uri2) == 0);

			g_free (uri1);
			g_free (uri2);

			g_object_unref (folder);
		}

		reference = em_folder_tree_model_get_row_reference (
			model, store, folder_name);
		if (reference != NULL) {
			GtkTreePath *path;
			GtkTreeIter iter;

			path = gtk_tree_row_reference_get_path (reference);
			gtk_tree_model_get_iter (
				GTK_TREE_MODEL (model), &iter, path);
			has_unread_mail (
				GTK_TREE_MODEL (model), &iter,
				TRUE, &folder_has_unread,
				&folder_has_unread_rec);
			gtk_tree_path_free (path);
		}

		g_clear_object (&store);
		g_free (folder_name);
		folder_name = NULL;
	}

	/* Look for a CamelStore that supports subscriptions. */
	list = em_folder_tree_model_list_stores (model);
	for (link = list; link != NULL; link = g_list_next (link)) {
		CamelStore *store = CAMEL_STORE (link->data);

		if (CAMEL_IS_SUBSCRIBABLE (store)) {
			any_store_is_subscribable = TRUE;
			break;
		}
	}
	g_list_free (list);

	action = ACTION (MAIL_ACCOUNT_DISABLE);
	sensitive = folder_is_store && store_can_be_disabled;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_ACCOUNT_EXPUNGE);
	sensitive = folder_is_trash;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_ACCOUNT_PROPERTIES);
	sensitive = folder_is_store && !store_is_builtin;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_ACCOUNT_REFRESH);
	sensitive = folder_is_store;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FLUSH_OUTBOX);
	sensitive = folder_is_outbox;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_COPY);
	sensitive = folder_is_selected;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_DELETE);
	sensitive = folder_is_selected && folder_can_be_deleted;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_EXPUNGE);
	sensitive = folder_is_selected && (!folder_is_virtual || mail_shell_view->priv->vfolder_allow_expunge);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_MOVE);
	sensitive = folder_is_selected && folder_can_be_deleted;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_NEW);
	sensitive = folder_allows_children;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_PROPERTIES);
	sensitive = folder_is_selected;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_REFRESH);
	sensitive = folder_is_selected;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_RENAME);
	sensitive =
		folder_is_selected &&
		folder_can_be_deleted &&
		folder_tree_and_message_list_agree;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_SELECT_THREAD);
	sensitive = folder_is_selected;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_SELECT_SUBTHREAD);
	sensitive = folder_is_selected;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_UNSUBSCRIBE);
	sensitive =
		folder_is_selected &&
		store_is_subscribable &&
		!folder_is_virtual;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_MARK_ALL_AS_READ);
	sensitive = folder_is_selected && folder_has_unread;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_POPUP_FOLDER_MARK_ALL_AS_READ);
	sensitive = folder_is_selected && folder_has_unread_rec;
	gtk_action_set_visible (action, sensitive);

	action = ACTION (MAIL_MANAGE_SUBSCRIPTIONS);
	sensitive = folder_is_store && store_is_subscribable;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_TOOLS_SUBSCRIPTIONS);
	sensitive = any_store_is_subscribable;
	gtk_action_set_sensitive (action, sensitive);

	/* folder_is_store + folder_is_virtual == "Search Folders" */
	action = ACTION (MAIL_VFOLDER_UNMATCHED_ENABLE);
	gtk_action_set_visible (action, folder_is_store && folder_is_virtual);

	e_mail_shell_view_update_popup_labels (mail_shell_view);
}

static void
e_mail_shell_view_class_init (EMailShellViewClass *class)
{
	GObjectClass *object_class;
	EShellViewClass *shell_view_class;

	g_type_class_add_private (class, sizeof (EMailShellViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_shell_view_set_property;
	object_class->get_property = mail_shell_view_get_property;
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

	/* Ensure the GalView types we need are registered. */
	g_type_ensure (GAL_TYPE_VIEW_ETABLE);

	g_object_class_install_property (
		object_class,
		PROP_VFOLDER_ALLOW_EXPUNGE,
		g_param_spec_boolean (
			"vfolder-allow-expunge",
			"vFolder Allow Expunge",
			"Allow expunge in virtual folders",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_shell_view_class_finalize (EMailShellViewClass *class)
{
}

static void
e_mail_shell_view_init (EMailShellView *mail_shell_view)
{
	mail_shell_view->priv =
		E_MAIL_SHELL_VIEW_GET_PRIVATE (mail_shell_view);

	e_mail_shell_view_private_init (mail_shell_view);
}

void
e_mail_shell_view_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_mail_shell_view_register_type (type_module);
}

