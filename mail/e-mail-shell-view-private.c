/*
 * e-mail-shell-view-private.c
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

#include "widgets/menus/gal-view-factory-etable.h"

static void
mail_shell_view_folder_tree_selected_cb (EMailShellView *mail_shell_view,
                                         const gchar *full_name,
                                         const gchar *uri,
                                         guint32 flags,
                                         EMFolderTree *folder_tree)
{
	EShellView *shell_view;
	EMailReader *reader;
	gboolean folder_selected;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	reader = E_MAIL_READER (mail_shell_view->priv->mail_shell_content);

	folder_selected =
		!(flags & CAMEL_FOLDER_NOSELECT) &&
		full_name != NULL;

	if (folder_selected)
		e_mail_reader_set_folder_uri (reader, uri);
	else
		e_mail_reader_set_folder (reader, NULL, NULL);

	e_shell_view_update_actions (shell_view);
}

static void
mail_shell_view_folder_tree_popup_event_cb (EShellView *shell_view,
                                            GdkEventButton *event)
{
	const gchar *widget_path;

	widget_path = "/mail-folder-popup";
	e_shell_view_show_popup_menu (shell_view, widget_path, event);
}

static gboolean
mail_shell_view_key_press_event_cb (EMailShellView *mail_shell_view,
                                    GdkEventKey *event)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkAction *action;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	if ((event->state & GDK_CONTROL_MASK) != 0)
		return FALSE;

	switch (event->keyval) {
		case GDK_space:
			action = ACTION (MAIL_SMART_FORWARD);
			break;

		case GDK_BackSpace:
			action = ACTION (MAIL_SMART_BACKWARD);
			break;

		default:
			return FALSE;
	}

	gtk_action_activate (action);

	return TRUE;
}

static gint
mail_shell_view_message_list_key_press_cb (EMailShellView *mail_shell_view,
                                           gint row,
                                           ETreePath path,
                                           gint col,
                                           GdkEvent *event)
{
	return mail_shell_view_key_press_event_cb (
		mail_shell_view, &event->key);
}

static gboolean
mail_shell_view_message_list_right_click_cb (EShellView *shell_view,
                                             gint row,
                                             ETreePath path,
                                             gint col,
                                             GdkEventButton *event)
{
	const gchar *widget_path;

	widget_path = "/mail-message-popup";
	e_shell_view_show_popup_menu (shell_view, widget_path, event);

	return TRUE;
}

static void
mail_shell_view_reader_changed_cb (EMailShellView *mail_shell_view,
                                   EMailReader *reader)
{
	EMailShellContent *mail_shell_content;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	e_mail_shell_content_update_view_instance (mail_shell_content);
	e_mail_shell_view_update_sidebar (mail_shell_view);
}

static void
mail_shell_view_reader_status_message_cb (EMailShellView *mail_shell_view,
                                          const gchar *status_message)
{
	EShellView *shell_view;
	EShellTaskbar *shell_taskbar;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_taskbar = e_shell_view_get_shell_taskbar (shell_view);

	e_shell_taskbar_set_message (shell_taskbar, status_message);
}

static void
mail_shell_view_load_view_collection (EShellViewClass *shell_view_class)
{
	GalViewCollection *collection;
	GalViewFactory *factory;
	ETableSpecification *spec;
	const gchar *base_dir;
	gchar *filename;

	collection = shell_view_class->view_collection;

	base_dir = EVOLUTION_ETSPECDIR;
	spec = e_table_specification_new ();
	filename = g_build_filename (base_dir, ETSPEC_FILENAME, NULL);
	if (!e_table_specification_load_from_file (spec, filename))
		g_critical ("Unable to load ETable specification file "
			    "for mail");
	g_free (filename);

	factory = gal_view_factory_etable_new (spec);
	gal_view_collection_add_factory (collection, factory);
	g_object_unref (factory);
	g_object_unref (spec);

	gal_view_collection_load (collection);
}

static void
mail_shell_view_notify_view_id_cb (EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	GalViewInstance *view_instance;
	const gchar *view_id;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	view_instance = NULL;  /* FIXME */
	view_id = e_shell_view_get_view_id (E_SHELL_VIEW (mail_shell_view));

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
e_mail_shell_view_private_init (EMailShellView *mail_shell_view,
                                EShellViewClass *shell_view_class)
{
	if (!gal_view_collection_loaded (shell_view_class->view_collection))
		mail_shell_view_load_view_collection (shell_view_class);

	g_signal_connect (
		mail_shell_view, "notify::view-id",
		G_CALLBACK (mail_shell_view_notify_view_id_cb), NULL);
}

void
e_mail_shell_view_private_constructed (EMailShellView *mail_shell_view)
{
	EMailShellViewPrivate *priv = mail_shell_view->priv;
	EMailShellSidebar *mail_shell_sidebar;
	EShell *shell;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EShellContent *shell_content;
	EShellSettings *shell_settings;
	EShellSidebar *shell_sidebar;
	EShellWindow *shell_window;
	EMFormatHTMLDisplay *html_display;
	EMFolderTree *folder_tree;
	RuleContext *context;
	FilterRule *rule = NULL;
	GtkTreeModel *tree_model;
	GtkUIManager *ui_manager;
	MessageList *message_list;
	EMailReader *reader;
	GtkHTML *html;
	const gchar *source;
	guint merge_id;
	gint ii = 0;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	ui_manager = e_shell_window_get_ui_manager (shell_window);

	shell = e_shell_window_get_shell (shell_window);
	shell_settings = e_shell_get_shell_settings (shell);

	tree_model = e_shell_settings_get_object (
		shell_settings, "mail-label-list-store");

	e_shell_window_add_action_group (shell_window, "mail");
	e_shell_window_add_action_group (shell_window, "mail-filter");
	e_shell_window_add_action_group (shell_window, "mail-label");

	merge_id = gtk_ui_manager_new_merge_id (ui_manager);
	priv->label_merge_id = merge_id;

	/* Cache these to avoid lots of awkward casting. */
	priv->mail_shell_backend = g_object_ref (shell_backend);
	priv->mail_shell_content = g_object_ref (shell_content);
	priv->mail_shell_sidebar = g_object_ref (shell_sidebar);

	reader = E_MAIL_READER (shell_content);
	html_display = e_mail_reader_get_html_display (reader);
	message_list = e_mail_reader_get_message_list (reader);

	mail_shell_sidebar = E_MAIL_SHELL_SIDEBAR (shell_sidebar);
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	html = EM_FORMAT_HTML (html_display)->html;

	g_signal_connect_swapped (
		folder_tree, "folder-selected",
		G_CALLBACK (mail_shell_view_folder_tree_selected_cb),
		mail_shell_view);

	g_signal_connect_swapped (
		folder_tree, "popup-event",
		G_CALLBACK (mail_shell_view_folder_tree_popup_event_cb),
		mail_shell_view);

	g_signal_connect_swapped (
		message_list->tree, "key-press",
		G_CALLBACK (mail_shell_view_message_list_key_press_cb),
		mail_shell_view);

	g_signal_connect_swapped (
		message_list->tree, "right-click",
		G_CALLBACK (mail_shell_view_message_list_right_click_cb),
		mail_shell_view);

	g_signal_connect_swapped (
		reader, "changed",
		G_CALLBACK (mail_shell_view_reader_changed_cb),
		mail_shell_view);

	/* Use the same callback as "changed". */
	g_signal_connect_swapped (
		reader, "folder-loaded",
		G_CALLBACK (mail_shell_view_reader_changed_cb),
		mail_shell_view);

	g_signal_connect_swapped (
		reader, "folder-loaded",
		G_CALLBACK (e_mail_shell_view_restore_state),
		mail_shell_view);

	g_signal_connect_swapped (
		tree_model, "row-changed",
		G_CALLBACK (e_mail_shell_view_update_search_filter),
		mail_shell_view);

	g_signal_connect_swapped (
		tree_model, "row-deleted",
		G_CALLBACK (e_mail_shell_view_update_search_filter),
		mail_shell_view);

	g_signal_connect_swapped (
		tree_model, "row-inserted",
		G_CALLBACK (e_mail_shell_view_update_search_filter),
		mail_shell_view);

	g_signal_connect_swapped (
		html, "key-press-event",
		G_CALLBACK (mail_shell_view_key_press_event_cb),
		mail_shell_view);

	g_signal_connect_swapped (
		html, "status-message",
		G_CALLBACK (mail_shell_view_reader_status_message_cb),
		mail_shell_view);

	e_mail_shell_view_actions_init (mail_shell_view);
	e_mail_shell_view_update_search_filter (mail_shell_view);
	e_mail_reader_init (reader);

	/* Populate built-in rules for search entry popup menu.
	 * Keep the assertions, please.  If the conditions aren't
	 * met we're going to crash anyway, just more mysteriously. */
	context = e_shell_content_get_search_context (shell_content);
	source = FILTER_SOURCE_DEMAND;
	while ((rule = rule_context_next_rule (context, rule, source))) {
		g_assert (ii < MAIL_NUM_SEARCH_RULES);
		priv->search_rules[ii++] = g_object_ref (rule);
	}
	g_assert (ii == MAIL_NUM_SEARCH_RULES);
}

void
e_mail_shell_view_private_dispose (EMailShellView *mail_shell_view)
{
	EMailShellViewPrivate *priv = mail_shell_view->priv;
	gint ii;

	DISPOSE (priv->mail_shell_backend);
	DISPOSE (priv->mail_shell_content);
	DISPOSE (priv->mail_shell_sidebar);

	for (ii = 0; ii < MAIL_NUM_SEARCH_RULES; ii++)
		DISPOSE (priv->search_rules[ii]);
}

void
e_mail_shell_view_private_finalize (EMailShellView *mail_shell_view)
{
	/* XXX Nothing to do? */
}

void
e_mail_shell_view_restore_state (EMailShellView *mail_shell_view)
{
	EShellView *shell_view;
	EShellContent *shell_content;
	EMailReader *reader;
	MessageList *message_list;
	const gchar *folder_uri;
	gchar *group_name;

	/* XXX Move this to EMailShellContent. */

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);

	reader = E_MAIL_READER (shell_content);
	message_list = e_mail_reader_get_message_list (reader);
	folder_uri = message_list->folder_uri;
	g_return_if_fail (folder_uri != NULL);

	group_name = g_strdup_printf ("Folder %s", folder_uri);
	e_shell_content_restore_state (shell_content, group_name);
	g_free (group_name);
}

void
e_mail_shell_view_execute_search (EMailShellView *mail_shell_view)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellContent *shell_content;
	EShellSettings *shell_settings;
	EMFormatHTMLDisplay *html_display;
	EMailShellContent *mail_shell_content;
	MessageList *message_list;
	FilterRule *rule;
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

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);

	shell = e_shell_window_get_shell (shell_window);
	shell_settings = e_shell_get_shell_settings (shell);

	mail_shell_content = mail_shell_view->priv->mail_shell_content;

	reader = E_MAIL_READER (shell_content);
	html_display = e_mail_reader_get_html_display (reader);
	message_list = e_mail_reader_get_message_list (reader);

	folder_uri = message_list->folder_uri;
	folder = message_list->folder;

	/* This returns a new object reference. */
	model = e_shell_settings_get_object (
		shell_settings, "mail-label-list-store");

	text = e_shell_content_get_search_text (shell_content);
	if (text == NULL || *text == '\0') {
		query = g_strdup ("");
		goto filter;
	}

	/* Replace variables in the selected rule with the
	 * current search text and extract a query string. */

	action = ACTION (MAIL_SEARCH_SUBJECT_OR_SENDER_CONTAINS);
	value = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (action));
	g_return_if_fail (value >= 0 && value < MAIL_NUM_SEARCH_RULES);
	rule = mail_shell_view->priv->search_rules[value];

	for (iter = rule->parts; iter != NULL; iter = iter->next) {
		FilterPart *part = iter->data;
		FilterElement *element = NULL;

		if (strcmp (part->name, "subject") == 0)
			element = filter_part_find_element (part, "subject");
		else if (strcmp (part->name, "body") == 0)
			element = filter_part_find_element (part, "word");
		else if (strcmp (part->name, "sender") == 0)
			element = filter_part_find_element (part, "sender");
		else if (strcmp (part->name, "to") == 0)
			element = filter_part_find_element (part, "recipient");

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
			FilterInput *input = FILTER_INPUT (element);
			filter_input_set_value (input, text);
		}
	}

	string = g_string_sized_new (1024);
	filter_rule_build_code (rule, string);
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

/* Helper for e_mail_shell_view_create_filter_from_selected() */
static void
mail_shell_view_create_filter_cb (CamelFolder *folder,
                                  const gchar *uid,
                                  CamelMimeMessage *message,
                                  gpointer user_data)
{
	struct {
		const gchar *source;
		gint type;
	} *filter_data = user_data;

	if (message != NULL)
		filter_gui_add_from_message (
			message, filter_data->source, filter_data->type);

	g_free (filter_data);
}

void
e_mail_shell_view_create_filter_from_selected (EMailShellView *mail_shell_view,
                                               gint filter_type)
{
	EMailReader *reader;
	MessageList *message_list;
	CamelFolder *folder;
	const gchar *filter_source;
	const gchar *folder_uri;
	GPtrArray *uids;

	struct {
		const gchar *source;
		gint type;
	} *filter_data;

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	reader = E_MAIL_READER (mail_shell_view->priv->mail_shell_content);
	message_list = e_mail_reader_get_message_list (reader);
	folder_uri = message_list->folder_uri;
	folder = message_list->folder;

	if (em_utils_folder_is_sent (folder, folder_uri))
		filter_source = FILTER_SOURCE_OUTGOING;
	else if (em_utils_folder_is_outbox (folder, folder_uri))
		filter_source = FILTER_SOURCE_OUTGOING;
	else
		filter_source = FILTER_SOURCE_INCOMING;

	uids = message_list_get_selected (message_list);

	if (uids->len == 1) {
		filter_data = g_malloc (sizeof (*filter_data));
		filter_data->source = filter_source;
		filter_data->type = filter_type;

		mail_get_message (
			folder, uids->pdata[0],
			mail_shell_view_create_filter_cb,
			filter_data, mail_msg_unordered_push);
	}

	em_utils_uids_free (uids);
}

/* Helper for e_mail_shell_view_create_vfolder_from_selected() */
static void
mail_shell_view_create_vfolder_cb (CamelFolder *folder,
                                   const gchar *uid,
                                   CamelMimeMessage *message,
                                   gpointer user_data)
{
	struct {
		gchar *uri;
		gint type;
	} *vfolder_data = user_data;

	if (message != NULL)
		vfolder_gui_add_from_message (
			message, vfolder_data->type, vfolder_data->uri);

	g_free (vfolder_data->uri);
	g_free (vfolder_data);
}

void
e_mail_shell_view_create_vfolder_from_selected (EMailShellView *mail_shell_view,
                                                gint vfolder_type)
{
	EMailReader *reader;
	MessageList *message_list;
	CamelFolder *folder;
	const gchar *folder_uri;
	GPtrArray *uids;

	struct {
		gchar *uri;
		gint type;
	} *vfolder_data;

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	reader = E_MAIL_READER (mail_shell_view->priv->mail_shell_content);
	message_list = e_mail_reader_get_message_list (reader);
	folder_uri = message_list->folder_uri;
	folder = message_list->folder;

	uids = message_list_get_selected (message_list);

	if (uids->len == 1) {
		vfolder_data = g_malloc (sizeof (*vfolder_data));
		vfolder_data->uri = g_strdup (folder_uri);
		vfolder_data->type = vfolder_type;

		mail_get_message (
			folder, uids->pdata[0],
			mail_shell_view_create_vfolder_cb,
			vfolder_data, mail_msg_unordered_push);
	}

	em_utils_uids_free (uids);
}

void
e_mail_shell_view_update_sidebar (EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	EShellSidebar *shell_sidebar;
	EShellView *shell_view;
	EMailReader *reader;
	MessageList *message_list;
	CamelStore *local_store;
	CamelFolder *folder;
	GPtrArray *selected;
	GString *buffer;
	const gchar *display_name;
	const gchar *folder_uri;
	gchar *folder_name;
	gchar *title;
	guint32 num_deleted;
	guint32 num_junked;
	guint32 num_junked_not_deleted;
	guint32 num_unread;
	guint32 num_visible;

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	mail_shell_content = mail_shell_view->priv->mail_shell_content;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	reader = E_MAIL_READER (mail_shell_content);
	message_list = e_mail_reader_get_message_list (reader);
	folder_uri = message_list->folder_uri;
	folder = message_list->folder;

	local_store = e_mail_local_get_store ();

	/* If no folder is selected, reset the sidebar banners
	 * to their default values and stop. */
	if (folder == NULL) {
		GtkAction *action;
		gchar *label;

		action = e_shell_view_get_action (shell_view);

		g_object_get (action, "label", &label, NULL);
		e_shell_sidebar_set_secondary_text (shell_sidebar, NULL);
		e_shell_view_set_title (shell_view, label);
		g_free (label);

		return;
	}

	camel_object_get (
		folder, NULL,
		CAMEL_FOLDER_NAME, &folder_name,
		CAMEL_FOLDER_DELETED, &num_deleted,
		CAMEL_FOLDER_JUNKED, &num_junked,
		CAMEL_FOLDER_JUNKED_NOT_DELETED, &num_junked_not_deleted,
		CAMEL_FOLDER_UNREAD, &num_unread,
		CAMEL_FOLDER_VISIBLE, &num_visible,
		NULL);

	buffer = g_string_sized_new (256);
	selected = message_list_get_selected (message_list);

	if (selected->len > 1)
		g_string_append_printf (
			buffer, ngettext ("%d selected, ", "%d selected, ",
			selected->len), selected->len);

	if (CAMEL_IS_VTRASH_FOLDER (folder)) {
		CamelVTrashFolder *trash_folder;

		trash_folder = (CamelVTrashFolder *) folder;

		/* "Trash" folder */
		if (trash_folder->type == CAMEL_VTRASH_FOLDER_TRASH)
			g_string_append_printf (
				buffer, ngettext ("%d deleted",
				"%d deleted", num_deleted), num_deleted);

		/* "Junk" folder (hide deleted messages) */
		else if (e_mail_reader_get_hide_deleted (reader))
			g_string_append_printf (
				buffer, ngettext ("%d junk",
				"%d junk", num_junked_not_deleted),
				num_junked_not_deleted);

		/* "Junk" folder (show deleted messages) */
		else
			g_string_append_printf (
				buffer, ngettext ("%d junk", "%d junk",
				num_junked), num_junked);

	/* "Drafts" folder */
	} else if (em_utils_folder_is_drafts (folder, folder_uri)) {
		g_string_append_printf (
			buffer, ngettext ("%d draft", "%d drafts",
			num_visible), num_visible);

	/* "Outbox" folder */
	} else if (em_utils_folder_is_outbox (folder, folder_uri)) {
		g_string_append_printf (
			buffer, ngettext ("%d unsent", "%d unsent",
			num_visible), num_visible);

	/* "Sent" folder */
	} else if (em_utils_folder_is_sent (folder, folder_uri)) {
		g_string_append_printf (
			buffer, ngettext ("%d sent", "%d sent",
			num_visible), num_visible);

	/* Normal folder */
	} else {
		if (!e_mail_reader_get_hide_deleted (reader))
			num_visible +=
				num_deleted - num_junked +
				num_junked_not_deleted;

		if (num_unread > 0 && selected->len <= 1)
			g_string_append_printf (
				buffer, ngettext ("%d unread, ",
				"%d unread, ", num_unread), num_unread);
		g_string_append_printf (
			buffer, ngettext ("%d total", "%d total",
			num_visible), num_visible);
	}

	message_list_free_uids (message_list, selected);

	/* Choose a suitable folder name for displaying. */
	if (folder->parent_store == local_store && (
		strcmp (folder_name, "Drafts") == 0 ||
		strcmp (folder_name, "Inbox") == 0 ||
		strcmp (folder_name, "Outbox") == 0 ||
		strcmp (folder_name, "Sent") == 0 ||
		strcmp (folder_name, "Templates") == 0))
		display_name = _(folder_name);
	else if (strcmp (folder_name, "INBOX") == 0)
		display_name = _("Inbox");
	else
		display_name = folder_name;

	title = g_strdup_printf ("%s (%s)", display_name, buffer->str);
	e_shell_sidebar_set_secondary_text (shell_sidebar, buffer->str);
	e_shell_view_set_title (shell_view, title);
	g_free (title);

	camel_object_free (folder, CAMEL_FOLDER_NAME, folder_name);
	g_string_free (buffer, TRUE);
}
