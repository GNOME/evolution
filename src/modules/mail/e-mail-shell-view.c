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

#include "mail/e-mail-paned-view.h"

#include "e-mail-shell-view-private.h"

enum {
	PROP_0,
	PROP_VFOLDER_ALLOW_EXPUNGE
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EMailShellView, e_mail_shell_view, E_TYPE_SHELL_VIEW, 0,
	G_ADD_PRIVATE_DYNAMIC (EMailShellView))

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
add_folders_from_store (GPtrArray *folders,
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
					g_ptr_array_add (folders, fldr);
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

	MessageList *message_list;
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
	GPtrArray *folders;
	GList *link;

	folders = g_ptr_array_new_with_free_func (g_object_unref);

	for (link = msg->stores_list; link; link = g_list_next (link)) {
		CamelStore *store = CAMEL_STORE (link->data);

		if (g_cancellable_is_cancelled (cancellable))
			break;

		add_folders_from_store (folders, store, cancellable, error);
	}

	if (!g_cancellable_is_cancelled (cancellable)) {
		CamelVeeFolder *vfolder = CAMEL_VEE_FOLDER (msg->folder);

		camel_vee_folder_set_folders_sync (vfolder, folders, CAMEL_VEE_FOLDER_OP_FLAG_NONE, cancellable, error);
	}

	g_ptr_array_unref (folders);
}

static void
search_results_done (SearchResultsMsg *msg)
{
	message_list_dec_setting_up_search_folder (msg->message_list);
}

static void
search_results_free (SearchResultsMsg *msg)
{
	g_object_unref (msg->message_list);
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
mail_shell_view_setup_search_results_folder (MessageList *message_list,
					     CamelFolder *folder,
                                             GList *stores,
                                             GCancellable *cancellable)
{
	SearchResultsMsg *msg;
	gint id;

	g_object_ref (folder);

	msg = mail_msg_new (&search_results_setup_info);
	msg->message_list = g_object_ref (message_list);
	msg->folder = folder;
	msg->cancellable = cancellable;
	msg->stores_list = stores;

	message_list_inc_setting_up_search_folder (message_list);

	id = msg->base.seq;
	mail_msg_slow_ordered_push (msg);

	return id;
}

typedef struct {
	MailMsg base;

	MessageList *message_list;
	CamelFolder *vfolder;
	GCancellable *cancellable;
	CamelFolder *root_folder;
} SearchResultsWithSubfoldersMsg;

static gchar *
search_results_with_subfolders_desc (SearchResultsWithSubfoldersMsg *msg)
{
	return g_strdup (_("Searching"));
}

static void
search_results_with_subfolders_exec (SearchResultsWithSubfoldersMsg *msg,
				     GCancellable *cancellable,
				     GError **error)
{
	GPtrArray *folders;
	CamelStore *root_store;
	CamelFolderInfo *fi;
	const CamelFolderInfo *cur;
	const gchar *root_folder_name;

	root_store = camel_folder_get_parent_store (msg->root_folder);
	if (!root_store)
		return;

	folders = g_ptr_array_new_with_free_func (g_object_unref);
	root_folder_name = camel_folder_get_full_name (msg->root_folder);

	fi = camel_store_get_folder_info_sync (root_store, root_folder_name,
		CAMEL_STORE_FOLDER_INFO_RECURSIVE, cancellable, NULL);

	cur = fi;
	while (cur && !g_cancellable_is_cancelled (cancellable)) {
		if ((cur->flags & CAMEL_FOLDER_NOSELECT) == 0) {
			CamelFolder *folder;

			folder = camel_store_get_folder_sync (root_store, cur->full_name, 0, cancellable, NULL);
			if (folder)
				g_ptr_array_add (folders, folder);
		}

		/* move to the next fi */
		if (cur->child) {
			cur = cur->child;
		} else if (cur->next) {
			cur = cur->next;
		} else {
			while (cur && !cur->next) {
				cur = cur->parent;
			}

			if (cur)
				cur = cur->next;
		}
	}

	camel_folder_info_free (fi);

	if (!g_cancellable_is_cancelled (cancellable)) {
		CamelVeeFolder *vfolder = CAMEL_VEE_FOLDER (msg->vfolder);

		camel_vee_folder_set_folders_sync (vfolder, folders, CAMEL_VEE_FOLDER_OP_FLAG_NONE, cancellable, error);
	}

	g_ptr_array_unref (folders);
}

static void
search_results_with_subfolders_done (SearchResultsWithSubfoldersMsg *msg)
{
	message_list_dec_setting_up_search_folder (msg->message_list);
}

static void
search_results_with_subfolders_free (SearchResultsWithSubfoldersMsg *msg)
{
	g_object_unref (msg->message_list);
	g_object_unref (msg->vfolder);
	g_object_unref (msg->root_folder);
}

static MailMsgInfo search_results_with_subfolders_setup_info = {
	sizeof (SearchResultsWithSubfoldersMsg),
	(MailMsgDescFunc) search_results_with_subfolders_desc,
	(MailMsgExecFunc) search_results_with_subfolders_exec,
	(MailMsgDoneFunc) search_results_with_subfolders_done,
	(MailMsgFreeFunc) search_results_with_subfolders_free
};

static gint
mail_shell_view_setup_search_results_folder_and_subfolders (MessageList *message_list,
							    CamelFolder *vfolder,
							    CamelFolder *root_folder,
							    GCancellable *cancellable)
{
	SearchResultsWithSubfoldersMsg *msg;
	gint id;

	if (!root_folder)
		return 0;

	msg = mail_msg_new (&search_results_with_subfolders_setup_info);
	msg->message_list = g_object_ref (message_list);
	msg->vfolder = g_object_ref (vfolder);
	msg->cancellable = cancellable;
	msg->root_folder = g_object_ref (root_folder);

	message_list_inc_setting_up_search_folder (message_list);

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
e_mail_shell_view_cleanup_state_key_file (EShellView *shell_view)
{
	EShellBackend *shell_backend;
	EMailSession *mail_session;
	CamelSession *session;
	GKeyFile *key_file;
	gchar **groups;
	gboolean changed = FALSE;
	gint ii;

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (shell_view));

	key_file = e_shell_view_get_state_key_file (shell_view);
	if (!key_file)
		return;

	shell_backend = e_shell_view_get_shell_backend (shell_view);
	mail_session = e_mail_backend_get_session (E_MAIL_BACKEND (shell_backend));

	if (!mail_session)
		return;

	session = CAMEL_SESSION (mail_session);

	groups = g_key_file_get_groups (key_file, NULL);

	if (!groups)
		return;

	for (ii = 0; groups[ii]; ii++) {
		const gchar *group_name = groups[ii];

		if (g_str_has_prefix (group_name, "Store ")) {
			CamelService *service;
			const gchar *uid = group_name + 6;

			service = camel_session_ref_service (session, uid);

			if (CAMEL_IS_STORE (service)) {
				g_object_unref (service);
			} else {
				changed = TRUE;
				g_key_file_remove_group (key_file, group_name, NULL);
			}
		} else if (g_str_has_prefix (group_name, "Folder ")) {
			CamelStore *store = NULL;
			gchar *folder_name = NULL;
			const gchar *uri = group_name + 7;

			if (e_mail_folder_uri_parse (session, uri, &store, &folder_name, NULL)) {
				if (!g_str_has_prefix (uri, "folder:")) {
					gchar *new_style_uri;

					new_style_uri = e_mail_folder_uri_build (store, folder_name);
					if (new_style_uri) {
						if (!g_key_file_has_group (key_file, new_style_uri)) {
							gchar **keys;
							gint jj;

							keys = g_key_file_get_keys (key_file, group_name, NULL, NULL);

							for (jj = 0; keys && keys[jj]; jj++) {
								const gchar *key = keys[jj];
								gchar *value;

								value = g_key_file_get_value (key_file, group_name, key, NULL);

								if (value) {
									g_key_file_set_value (key_file, group_name, key, value);
									g_free (value);
								}
							}

							g_strfreev (keys);
						}

						changed = TRUE;
						g_key_file_remove_group (key_file, group_name, NULL);
					}
				}

				g_clear_object (&store);
				g_free (folder_name);

			/* One non-Folder section is named "Folder Tree", thus avoid erasing it and others not looking like URI */
			} else if (strstr (group_name, ":/")) {
				changed = TRUE;
				g_key_file_remove_group (key_file, group_name, NULL);
			}
		}
	}

	g_strfreev (groups);

	if (changed)
		e_shell_view_set_state_dirty (shell_view);
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
mail_shell_view_notify_active_view_cb (GObject *object,
				       GParamSpec *param,
				       gpointer user_data)
{
	EMailShellView *self = user_data;
	EMailDisplay *mail_display;
	EUIManager *ui_manager;
	GtkAccelGroup *accel_group;

	mail_display = e_mail_reader_get_mail_display (E_MAIL_READER (e_mail_shell_content_get_mail_view (self->priv->mail_shell_content)));
	if (!mail_display)
		return;

	ui_manager = e_web_view_get_ui_manager (E_WEB_VIEW (mail_display));
	if (!ui_manager)
		return;

	accel_group = e_ui_manager_get_accel_group (ui_manager);

	/* to enable attachment inline toggle actions' accels from the mail display (like "<Primary><Alt>1") */
	if (e_shell_view_is_active (E_SHELL_VIEW (self))) {
		if (!self->priv->web_view_accel_group_added) {
			self->priv->web_view_accel_group_added = TRUE;
			gtk_window_add_accel_group (GTK_WINDOW (object), accel_group);
		}
	} else if (self->priv->web_view_accel_group_added) {
		self->priv->web_view_accel_group_added = FALSE;
		gtk_window_remove_accel_group (GTK_WINDOW (object), accel_group);
	}
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
mail_shell_view_customize_toolbar_activate_cb (GtkWidget *toolbar,
					       const gchar *id,
					       gpointer user_data)
{
	EMailShellView *self = user_data;

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (self));

	e_shell_view_run_ui_customize_dialog (E_SHELL_VIEW (self), id);
}

static void
mail_shell_view_constructed (GObject *object)
{
	EMailShellView *self = E_MAIL_SHELL_VIEW (object);
	EShellView *shell_view = E_SHELL_VIEW (self);
	EShellSearchbar *searchbar;
	EMailView *mail_view;
	EUICustomizer *customizer;
	EUIManager *ui_manager;
	EActionComboBox *combo_box;
	GObject *ui_item;
	GtkWidget *to_do_pane;

	ui_manager = e_shell_view_get_ui_manager (shell_view);

	e_ui_manager_freeze (ui_manager);

	self->priv->mail_shell_content = g_object_ref_sink (E_MAIL_SHELL_CONTENT (e_mail_shell_content_new (shell_view)));

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_shell_view_parent_class)->constructed (object);

	e_mail_shell_view_private_constructed (E_MAIL_SHELL_VIEW (object));
	e_mail_shell_view_cleanup_state_key_file (E_SHELL_VIEW (object));

	mail_view = e_mail_shell_content_get_mail_view (self->priv->mail_shell_content);
	searchbar = e_mail_shell_content_get_searchbar (self->priv->mail_shell_content);

	combo_box = e_shell_searchbar_get_scope_combo_box (searchbar);
	e_action_combo_box_set_action (combo_box, ACTION (MAIL_SCOPE_ALL_ACCOUNTS));
	e_shell_searchbar_set_scope_visible (searchbar, TRUE);

	/* Advanced Search Action */
	e_shell_searchbar_set_search_option (searchbar, ACTION (MAIL_SEARCH_ADVANCED_HIDDEN));

	e_binding_bind_property (
		ACTION (MAIL_PREVIEW), "active",
		mail_view, "preview-visible",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		ACTION (MAIL_SHOW_PREVIEW_TOOLBAR), "active",
		mail_view, "preview-toolbar-visible",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	to_do_pane = e_mail_shell_content_get_to_do_pane (self->priv->mail_shell_content);

	e_binding_bind_property (
		ACTION (MAIL_TO_DO_BAR), "active",
		to_do_pane, "visible",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		ACTION (MAIL_SHOW_DELETED), "active",
		mail_view, "show-deleted",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		ACTION (MAIL_SHOW_JUNK), "active",
		mail_view, "show-junk",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		ACTION (MAIL_THREADS_GROUP_BY), "active",
		mail_view, "group-by-threads",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	/* Keep the sensitivity of "Create Search Folder from Search"
	 * in sync with "Save Search" so that its only selectable when
	 * showing search results. */
	e_binding_bind_property (
		E_SHELL_VIEW_ACTION (shell_view, "search-save"), "sensitive",
		ACTION (MAIL_CREATE_SEARCH_FOLDER), "sensitive",
		G_BINDING_SYNC_CREATE);

	/* WebKitGTK does not support print preview, thus hide the option from the menu;
	   maybe it'll be supported in the future */
	e_ui_action_set_visible (ACTION (MAIL_PRINT_PREVIEW), FALSE);

	customizer = e_ui_manager_get_customizer (ui_manager);

	ui_item = e_ui_manager_create_item (ui_manager, "mail-preview-toolbar");
	e_util_setup_toolbar_icon_size (GTK_TOOLBAR (ui_item), GTK_ICON_SIZE_SMALL_TOOLBAR);
	e_mail_paned_view_take_preview_toolbar (E_MAIL_PANED_VIEW (e_mail_shell_content_get_mail_view (self->priv->mail_shell_content)), GTK_WIDGET (ui_item));

	e_ui_customizer_util_attach_toolbar_context_menu (GTK_WIDGET (ui_item), "mail-preview-toolbar",
		mail_shell_view_customize_toolbar_activate_cb, self);

	e_ui_customizer_register (customizer, "mail-preview-toolbar", _("Preview Toolbar"));
	e_ui_customizer_register (customizer, "mail-preview-popup", _("Preview Context Menu"));
	e_ui_customizer_register (customizer, "mail-folder-popup", _("Folder Context Menu"));
	e_ui_customizer_register (customizer, "mail-message-popup", _("Message Context Menu"));

	e_ui_manager_thaw (ui_manager);

	e_signal_connect_notify_object (e_shell_view_get_shell_window (shell_view), "notify::active-view",
		G_CALLBACK (mail_shell_view_notify_active_view_cb), self, 0);
}

static gchar *
mail_shell_view_construct_filter_message_thread (EMailShellView *mail_shell_view,
						 const gchar *with_query)
{
	EMailShellView *self;
	GString *query;
	GSList *link;

	g_return_val_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view), NULL);

	self = E_MAIL_SHELL_VIEW (mail_shell_view);

	if (!self->priv->selected_uids) {
		EShellContent *shell_content;
		EMailView *mail_view;
		GPtrArray *uids;

		shell_content = e_shell_view_get_shell_content (E_SHELL_VIEW (mail_shell_view));
		mail_view = e_mail_shell_content_get_mail_view (E_MAIL_SHELL_CONTENT (shell_content));
		uids = e_mail_reader_get_selected_uids (E_MAIL_READER (mail_view));

		if (uids) {
			gint ii;

			for (ii = 0; ii < uids->len; ii++) {
				self->priv->selected_uids = g_slist_prepend (self->priv->selected_uids, (gpointer) camel_pstring_strdup (uids->pdata[ii]));
			}

			g_ptr_array_unref (uids);
		}

		if (!self->priv->selected_uids)
			self->priv->selected_uids = g_slist_prepend (self->priv->selected_uids, (gpointer) camel_pstring_strdup (""));
	}

	query = g_string_new ("");

	if (with_query && *with_query) {
		if (g_str_has_prefix (with_query, "(match-all ") || strstr (with_query, "(match-threads "))
			g_string_append_printf (query, "(and %s ", with_query);
		else
			g_string_append_printf (query, "(and (match-all %s) ", with_query);
	}

	g_string_append (query, "(match-threads \"all\" (match-all (uid");

	for (link = self->priv->selected_uids; link; link = g_slist_next (link)) {
		const gchar *uid = link->data;

		g_string_append_c (query, ' ');
		g_string_append_c (query, '\"');
		g_string_append (query, uid);
		g_string_append_c (query, '\"');
	}

	g_string_append (query, ")))");

	if (with_query && *with_query)
		g_string_append_c (query, ')');

	return g_string_free (query, FALSE);
}

static void
mail_shell_view_restore_selected_folder (EShellView *shell_view)
{
	EShellSidebar *shell_sidebar;
	EMailReader *reader;
	EMFolderTree *folder_tree;
	CamelStore *selected_store = NULL;
	gchar *selected_folder_name = NULL;

	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	folder_tree = e_mail_shell_sidebar_get_folder_tree (E_MAIL_SHELL_SIDEBAR (shell_sidebar));

	reader = E_MAIL_READER (e_mail_shell_content_get_mail_view (E_MAIL_SHELL_CONTENT (e_shell_view_get_shell_content (shell_view))));

	/* Reset the message list to the current folder tree
	 * selection.  This needs to happen synchronously to
	 * avoid search conflicts, so we can't just grab the
	 * folder URI and let the asynchronous callbacks run
	 * after we've already kicked off the search. */
	em_folder_tree_get_selected (folder_tree, &selected_store, &selected_folder_name);
	if (selected_store != NULL && selected_folder_name != NULL) {
		CamelFolder *sel_folder;

		sel_folder = camel_store_get_folder_sync (
			selected_store, selected_folder_name,
			0, NULL, NULL);
		e_mail_reader_set_folder (reader, sel_folder);
		g_clear_object (&sel_folder);
	}

	g_clear_object (&selected_store);
	g_free (selected_folder_name);
}

static void
mail_shell_view_stop_and_clear_search_vfolders (EMailShellView *mail_shell_view)
{
	if (mail_shell_view->priv->search_account_cancel) {
		g_cancellable_cancel (mail_shell_view->priv->search_account_cancel);
		g_clear_object (&mail_shell_view->priv->search_account_cancel);
	}

	g_clear_object (&mail_shell_view->priv->search_folder_and_subfolders);
	g_clear_object (&mail_shell_view->priv->search_account_all);
	g_clear_object (&mail_shell_view->priv->search_account_current);
}

static gboolean
mail_shell_view_option_is_contains (EFilterElement *elem)
{
	EFilterOption *opt;

	if (!E_IS_FILTER_OPTION (elem))
		return FALSE;

	opt = E_FILTER_OPTION (elem);

	return opt->current && g_strcmp0 (opt->current->value, "contains") == 0;
}

static gchar *
mail_shell_view_dup_input_text (EFilterElement *elem)
{
	EFilterInput *inpt;
	GString *str;
	GList *link;

	if (!E_IS_FILTER_INPUT (elem))
		return NULL;

	inpt = E_FILTER_INPUT (elem);
	str = g_string_new ("");

	for (link = inpt->values; link; link = g_list_next (link)) {
		const gchar *value = link->data;

		if (value && *value) {
			if (str->len)
				g_string_append_c (str, ' ');
			g_string_append (str, value);
		}
	}

	return g_string_free (str, FALSE);
}

static void
mail_shell_view_custom_search (EShellView *shell_view,
			       EFilterRule *custom_rule)
{
	gboolean processed = FALSE;

	/* Make sure the <ruleset> from src/mail/searchtypes.xml.in matches below code */

	if (custom_rule && custom_rule->threading == E_FILTER_THREAD_NONE &&
	    custom_rule->grouping == E_FILTER_GROUP_ANY &&
	    custom_rule->parts && custom_rule->parts->data) {
		EShellSearchbar *searchbar = E_SHELL_SEARCHBAR (e_shell_view_get_searchbar (shell_view));
		EFilterPart *part = custom_rule->parts->data;
		EUIAction *search_action = NULL;
		gchar *search_text = NULL;

		if (!custom_rule->parts->next && g_list_length (part->elements) == 2) {
			EFilterElement *elem0, *elem1;

			elem0 = part->elements->data;
			elem1 = part->elements->next->data;

			if (mail_shell_view_option_is_contains (elem0)) {
				if (g_strcmp0 (part->name, "sender") == 0)
					search_action = ACTION (MAIL_SEARCH_SENDER_CONTAINS);
				else if (g_strcmp0 (part->name, "subject") == 0)
					search_action = ACTION (MAIL_SEARCH_SUBJECT_CONTAINS);
				else if (g_strcmp0 (part->name, "to") == 0)
					search_action = ACTION (MAIL_SEARCH_RECIPIENTS_CONTAIN);
				else if (g_strcmp0 (part->name, "body") == 0)
					search_action = ACTION (MAIL_SEARCH_BODY_CONTAINS);

				if (search_action)
					search_text = mail_shell_view_dup_input_text (elem1);
			}
		} else if (!custom_rule->parts->next && g_list_length (part->elements) == 1 &&
			   g_strcmp0 (part->name, "mail-free-form-exp") == 0) {
			EFilterElement *elem;

			elem = part->elements->data;

			search_action = ACTION (MAIL_SEARCH_FREE_FORM_EXPR);
			search_text = mail_shell_view_dup_input_text (elem);
		} else if (g_list_length (custom_rule->parts) == 3) {
			GList *link;
			gboolean has_subject = FALSE, has_sender = FALSE, has_to = FALSE;

			for (link = custom_rule->parts; link; link = g_list_next (link)) {
				part = link->data;

				if (!part || g_list_length (part->elements) != 2 ||
				    !mail_shell_view_option_is_contains (part->elements->data))
					continue;

				if (!has_subject && g_strcmp0 (part->name, "subject") == 0)
					has_subject = TRUE;
				else if (!has_sender  && g_strcmp0 (part->name, "sender") == 0)
					has_sender = TRUE;
				else if (!has_to && g_strcmp0 (part->name, "to") == 0)
					has_to = TRUE;
			}

			if (has_subject && has_sender && has_to) {
				for (link = custom_rule->parts; link; link = g_list_next (link)) {
					EFilterElement *elem;
					gchar *text;

					part = link->data;

					if (!part || !part->elements || !part->elements->next)
						continue;

					elem = part->elements->next->data;
					text = mail_shell_view_dup_input_text (elem);

					/* all three options should have set the same text to search for */
					if (!text || (search_text && g_strcmp0 (search_text, text) != 0)) {
						g_clear_pointer (&search_text, g_free);
						g_free (text);
						break;
					}

					if (search_text)
						g_free (text);
					else
						search_text = text;
				}

				if (search_text)
					search_action = ACTION (MAIL_SEARCH_SUBJECT_OR_ADDRESSES_CONTAIN);
			}
		} else if (g_list_length (custom_rule->parts) == 4) {
			GList *link;
			gboolean has_subject = FALSE, has_sender = FALSE, has_to = FALSE, has_body = FALSE;

			for (link = custom_rule->parts; link; link = g_list_next (link)) {
				part = link->data;

				if (!part || g_list_length (part->elements) != 2 ||
				    !mail_shell_view_option_is_contains (part->elements->data))
					continue;

				if (!has_subject && g_strcmp0 (part->name, "subject") == 0)
					has_subject = TRUE;
				else if (!has_sender  && g_strcmp0 (part->name, "sender") == 0)
					has_sender = TRUE;
				else if (!has_to && g_strcmp0 (part->name, "to") == 0)
					has_to = TRUE;
				else if (!has_body && g_strcmp0 (part->name, "body") == 0)
					has_body = TRUE;
			}

			if (has_subject && has_sender && has_to && has_body) {
				for (link = custom_rule->parts; link; link = g_list_next (link)) {
					EFilterElement *elem;
					gchar *text;

					part = link->data;

					if (!part || !part->elements || !part->elements->next)
						continue;

					elem = part->elements->next->data;
					text = mail_shell_view_dup_input_text (elem);

					/* all four options should have set the same text to search for */
					if (!text || (search_text && g_strcmp0 (search_text, text) != 0)) {
						g_clear_pointer (&search_text, g_free);
						g_free (text);
						break;
					}

					if (search_text)
						g_free (text);
					else
						search_text = text;
				}

				if (search_text)
					search_action = ACTION (MAIL_SEARCH_MESSAGE_CONTAINS);
			}
		}

		if (search_action && search_text) {
			e_shell_view_block_execute_search (shell_view);
			e_shell_view_set_search_rule (shell_view, NULL);
			e_ui_action_set_active (search_action, TRUE);
			e_shell_searchbar_set_search_text (searchbar, search_text);
			e_shell_view_unblock_execute_search (shell_view);
			e_shell_view_execute_search (shell_view);

			processed = TRUE;
		}

		g_free (search_text);
	}

	if (!processed)
		E_SHELL_VIEW_CLASS (e_mail_shell_view_parent_class)->custom_search (shell_view, custom_rule);
}

static void
mail_shell_view_execute_search (EShellView *shell_view)
{
	EMailShellView *self = E_MAIL_SHELL_VIEW (shell_view);
	EMailShellContent *mail_shell_content;
	EMailShellSidebar *mail_shell_sidebar;
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
	EUIAction *action;
	EMailLabelListStore *label_store;
	GVariant *state;
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
	state = g_action_get_state (G_ACTION (action));
	value = g_variant_get_int32 (state);
	g_clear_pointer (&state, g_variant_unref);

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
	rule = self->priv->search_rules[value];

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

	if (query && *query && !strstr (query, "(match-all ") && !strstr (query, "(match-threads ")) {
		/* Make sure the query is enclosed in "(match-all ...)", to traverse the folders' content */
		temp = g_strconcat ("(match-all ", query, ")", NULL);
		g_free (query);
		query = temp;
	}

	combo_box = e_shell_searchbar_get_filter_combo_box (searchbar);
	value = e_action_combo_box_get_current_value (combo_box);

	if (value != MAIL_FILTER_MESSAGE_THREAD) {
		g_slist_free_full (self->priv->selected_uids, (GDestroyNotify) camel_pstring_free);
		self->priv->selected_uids = NULL;
	}

	switch (value) {
		case MAIL_FILTER_ALL_MESSAGES:
			break;

		case MAIL_FILTER_UNREAD_MESSAGES:
			if (query && *query) {
				temp = g_strdup_printf ("(and %s (match-all (not (system-flag \"Seen\"))))", query);
			} else {
				temp = g_strdup ("(match-all (not (system-flag \"Seen\")))");
			}
			g_free (query);
			query = temp;
			break;

		case MAIL_FILTER_NO_LABEL:
			string = g_string_sized_new (1024);
			if (query && *query)
				g_string_append_printf (string, "(and %s (match-all ", query);
			else
				g_string_append (string, "(match-all ");
			g_string_append (string, "(and ");
			valid = gtk_tree_model_get_iter_first (
				GTK_TREE_MODEL (label_store), &tree_iter);
			while (valid) {
				tag = e_mail_label_list_store_get_tag (
					label_store, &tree_iter);
				use_tag = tag;
				if (g_str_has_prefix (use_tag, "$Label"))
					use_tag += 6;
				g_string_append_printf (
					string, " (not (or "
					"(= (user-tag \"label\") \"%s\") "
					"(user-flag \"$Label%s\") "
					"(user-flag \"%s\")))",
					use_tag, use_tag, use_tag);
				g_free (tag);

				valid = gtk_tree_model_iter_next (
					GTK_TREE_MODEL (label_store),
					&tree_iter);
			}
			if (query && *query)
				g_string_append (string, ")))");
			else
				g_string_append (string, "))");
			g_free (query);
			query = g_string_free (string, FALSE);
			break;

		case MAIL_FILTER_READ_MESSAGES:
			if (query && *query)
				temp = g_strdup_printf ("(and %s (match-all (system-flag \"Seen\")))", query);
			else
				temp = g_strdup ("(match-all (system-flag \"Seen\"))");
			g_free (query);
			query = temp;
			break;

		case MAIL_FILTER_LAST_5_DAYS_MESSAGES: {
			const gchar *date_ident;

			if (em_utils_folder_is_sent (registry, folder))
				date_ident = "get-sent-date";
			else
				date_ident = "get-received-date";

			if (query && *query)
				temp = g_strdup_printf ("(and %s (match-all (> (%s) (- (get-current-date) 432000))))", query, date_ident);
			else
				temp = g_strdup_printf ("(match-all (> (%s) (- (get-current-date) 432000)))", date_ident);
			g_free (query);
			query = temp;
			} break;

		case MAIL_FILTER_MESSAGES_WITH_ATTACHMENTS:
			if (query && *query)
				temp = g_strdup_printf ("(and %s (match-all (system-flag \"Attachments\")))", query);
			else
				temp = g_strdup ("(match-all (system-flag \"Attachments\"))");
			g_free (query);
			query = temp;
			break;

		case MAIL_FILTER_MESSAGES_WITH_NOTES:
			if (query && *query)
				temp = g_strdup_printf ("(and %s (match-all (user-flag \"$has_note\")))", query);
			else
				temp = g_strdup ("(match-all (user-flag \"$has_note\"))");
			g_free (query);
			query = temp;
			break;

		case MAIL_FILTER_IMPORTANT_MESSAGES:
			if (query && *query)
				temp = g_strdup_printf ("(and %s (match-all (system-flag \"Flagged\")))", query);
			else
				temp = g_strdup ("(match-all (system-flag \"Flagged\"))");
			g_free (query);
			query = temp;
			break;

		case MAIL_FILTER_MESSAGES_NOT_JUNK:
			if (query && *query)
				temp = g_strdup_printf ("(and %s (match-all (not (system-flag \"junk\"))))", query);
			else
				temp = g_strdup ("(match-all (not (system-flag \"junk\")))");
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
			g_warn_if_fail (gtk_tree_model_get_iter (GTK_TREE_MODEL (label_store), &tree_iter, path));
			gtk_tree_path_free (path);

			tag = e_mail_label_list_store_get_tag (label_store, &tree_iter);
			use_tag = tag;
			if (g_str_has_prefix (use_tag, "$Label"))
				use_tag += 6;
			if (query && *query) {
				temp = g_strdup_printf (
					"(and %s (match-all (or "
					"(= (user-tag \"label\") \"%s\") "
					"(user-flag \"$Label%s\") "
					"(user-flag \"%s\"))))",
					query, use_tag, use_tag, use_tag);
			} else {
				temp = g_strdup_printf (
					"(match-all (or "
					"(= (user-tag \"label\") \"%s\") "
					"(user-flag \"$Label%s\") "
					"(user-flag \"%s\")))",
					use_tag, use_tag, use_tag);
			}
			g_free (tag);

			g_free (query);
			query = temp;
			break;
	}

	gtk_widget_set_sensitive (GTK_WIDGET (folder_tree), TRUE);

	/* Apply selected scope. */

	combo_box = e_shell_searchbar_get_scope_combo_box (searchbar);
	value = e_action_combo_box_get_current_value (combo_box);

	/* virtual Trash/Junk folders cannot have subfolders, thus
	   switch internally to "Current Folder" only */
	if (value == MAIL_SCOPE_CURRENT_FOLDER_AND_SUBFOLDERS &&
	    CAMEL_IS_VTRASH_FOLDER (folder))
		value = MAIL_SCOPE_CURRENT_FOLDER;

	switch (value) {
		case MAIL_SCOPE_CURRENT_FOLDER:
			mail_shell_view_stop_and_clear_search_vfolders (E_MAIL_SHELL_VIEW (shell_view));
			mail_shell_view_restore_selected_folder (shell_view);
			goto execute;

		case MAIL_SCOPE_CURRENT_FOLDER_AND_SUBFOLDERS:
			goto current_and_subfolders;

		case MAIL_SCOPE_CURRENT_ACCOUNT:
			goto current_account;

		case MAIL_SCOPE_ALL_ACCOUNTS:
			goto all_accounts;

		default:
			g_warn_if_reached ();
			goto execute;
	}

 current_and_subfolders:

	/* Prepare search folder for current folder and its subfolders. */

	/* If the search text is empty, cancel any
	 * account-wide searches still in progress. */
	text = e_shell_searchbar_get_search_text (searchbar);
	if ((text == NULL || *text == '\0') && !e_shell_view_get_search_rule (shell_view)) {
		mail_shell_view_stop_and_clear_search_vfolders (E_MAIL_SHELL_VIEW (shell_view));
		mail_shell_view_restore_selected_folder (shell_view);
		goto execute;
	}

	search_folder = self->priv->search_folder_and_subfolders;

	/* Skip the search if we already have the results. */
	if (search_folder != NULL) {
		const gchar *vf_query;

		vf_query = camel_vee_folder_get_expression (search_folder);
		if (g_strcmp0 (query, vf_query) == 0)
			goto current_folder_and_subfolders_setup;
	}

	/* If we already have a search folder, reuse it. */
	if (search_folder != NULL) {
		if (self->priv->search_account_cancel != NULL) {
			g_cancellable_cancel (self->priv->search_account_cancel);
			g_clear_object (&self->priv->search_account_cancel);
		}

		camel_vee_folder_set_expression_sync (search_folder, query, CAMEL_VEE_FOLDER_OP_FLAG_SKIP_REBUILD, NULL, NULL);

		goto current_folder_and_subfolders_setup;
	}

	/* Create a new search folder. */

	/* FIXME Complete lack of error checking here. */
	service = camel_session_ref_service (CAMEL_SESSION (session), E_MAIL_SESSION_VFOLDER_UID);
	camel_service_connect_sync (service, NULL, NULL);

	search_folder = (CamelVeeFolder *) camel_vee_folder_new (
		CAMEL_STORE (service),
		_("Current Folder and Subfolders Search"),
		CAMEL_STORE_FOLDER_PRIVATE);
	self->priv->search_folder_and_subfolders = search_folder;

	g_object_unref (service);

	camel_vee_folder_set_expression_sync (search_folder, query, CAMEL_VEE_FOLDER_OP_FLAG_SKIP_REBUILD, NULL, NULL);

 current_folder_and_subfolders_setup:

	gtk_widget_set_sensitive (GTK_WIDGET (folder_tree), FALSE);

	if (folder != NULL && folder != CAMEL_FOLDER (search_folder)) {
		/* Just use the folder */
	} else {
		CamelStore *selected_store = NULL;
		gchar *selected_folder_name = NULL;

		g_clear_object (&folder);

		em_folder_tree_get_selected (folder_tree, &selected_store, &selected_folder_name);
		if (selected_store != NULL && selected_folder_name != NULL) {
			folder = camel_store_get_folder_sync (selected_store, selected_folder_name, 0, NULL, NULL);
		}

		g_clear_object (&selected_store);
		g_free (selected_folder_name);
	}

	self->priv->search_account_cancel = camel_operation_new ();

	mail_shell_view_setup_search_results_folder_and_subfolders (
		MESSAGE_LIST (message_list),
		CAMEL_FOLDER (search_folder), folder,
		self->priv->search_account_cancel);

	mail_shell_view_show_search_results_folder (
		E_MAIL_SHELL_VIEW (shell_view),
		CAMEL_FOLDER (search_folder));

	goto execute;

all_accounts:

	/* Prepare search folder for all accounts. */

	/* If the search text is empty, cancel any
	 * account-wide searches still in progress. */
	text = e_shell_searchbar_get_search_text (searchbar);
	if ((text == NULL || *text == '\0') && !e_shell_view_get_search_rule (shell_view)) {
		mail_shell_view_stop_and_clear_search_vfolders (E_MAIL_SHELL_VIEW (shell_view));
		mail_shell_view_restore_selected_folder (shell_view);
		goto execute;
	}

	search_folder = self->priv->search_account_all;

	/* Skip the search if we already have the results. */
	if (search_folder != NULL) {
		const gchar *vf_query;

		vf_query = camel_vee_folder_get_expression (search_folder);
		if (g_strcmp0 (query, vf_query) == 0)
			goto all_accounts_setup;
	}

	/* If we already have a search folder, reuse it. */
	if (search_folder != NULL) {
		if (self->priv->search_account_cancel != NULL) {
			g_cancellable_cancel (self->priv->search_account_cancel);
			g_clear_object (&self->priv->search_account_cancel);
		}

		camel_vee_folder_set_expression_sync (search_folder, query, CAMEL_VEE_FOLDER_OP_FLAG_SKIP_REBUILD, NULL, NULL);

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
	self->priv->search_account_all = search_folder;

	g_object_unref (service);

	camel_vee_folder_set_expression_sync (search_folder, query, CAMEL_VEE_FOLDER_OP_FLAG_SKIP_REBUILD, NULL, NULL);

all_accounts_setup:

	gtk_widget_set_sensitive (GTK_WIDGET (folder_tree), FALSE);

	list = em_folder_tree_model_list_stores (EM_FOLDER_TREE_MODEL (
		gtk_tree_view_get_model (GTK_TREE_VIEW (folder_tree))));
	g_list_foreach (list, (GFunc) g_object_ref, NULL);

	self->priv->search_account_cancel = camel_operation_new ();

	/* This takes ownership of the stores list. */
	mail_shell_view_setup_search_results_folder (
		MESSAGE_LIST (message_list),
		CAMEL_FOLDER (search_folder), list,
		self->priv->search_account_cancel);

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
		mail_shell_view_stop_and_clear_search_vfolders (E_MAIL_SHELL_VIEW (shell_view));
		mail_shell_view_restore_selected_folder (shell_view);
		goto execute;
	}

	search_folder = self->priv->search_account_current;

	/* Skip the search if we already have the results. */
	if (search_folder != NULL) {
		const gchar *vf_query;

		vf_query = camel_vee_folder_get_expression (search_folder);
		if (g_strcmp0 (query, vf_query) == 0)
			goto current_accout_setup;
	}

	/* If we already have a search folder, reuse it. */
	if (search_folder != NULL) {
		if (self->priv->search_account_cancel != NULL) {
			g_cancellable_cancel (self->priv->search_account_cancel);
			g_clear_object (&self->priv->search_account_cancel);
		}

		camel_vee_folder_set_expression_sync (search_folder, query, CAMEL_VEE_FOLDER_OP_FLAG_SKIP_REBUILD, NULL, NULL);

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
	self->priv->search_account_current = search_folder;

	g_object_unref (service);

	camel_vee_folder_set_expression_sync (search_folder, query, CAMEL_VEE_FOLDER_OP_FLAG_SKIP_REBUILD, NULL, NULL);

current_accout_setup:

	gtk_widget_set_sensitive (GTK_WIDGET (folder_tree), FALSE);

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

	self->priv->search_account_cancel = camel_operation_new ();

	/* This takes ownership of the stores list. */
	mail_shell_view_setup_search_results_folder (
		MESSAGE_LIST (message_list),
		CAMEL_FOLDER (search_folder), list,
		self->priv->search_account_cancel);

	mail_shell_view_show_search_results_folder (
		E_MAIL_SHELL_VIEW (shell_view),
		CAMEL_FOLDER (search_folder));

execute:

	/* Finally, execute the search. */

	message_list_set_search (MESSAGE_LIST (message_list), query);

	e_mail_view_set_search_strings (mail_view, search_strings);

	g_slist_free_full (search_strings, g_free);

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
	EMFolderTree *folder_tree;
	EMFolderTreeModel *model;
	EMailReader *reader;
	EMailView *mail_view;
	EMailDisplay *display;
	EUIAction *action;
	CamelStore *store = NULL;
	GList *list, *link;
	gchar *folder_name = NULL;
	gboolean sensitive;
	guint32 state;

	/* Be descriptive. */
	gboolean folder_allows_children;
	gboolean folder_can_be_deleted;
	gboolean folder_is_junk;
	gboolean folder_is_outbox;
	gboolean folder_is_selected = FALSE;
	gboolean folder_is_store;
	gboolean folder_is_trash;
	gboolean folder_is_virtual;
	gboolean folder_has_unread = FALSE;
	gboolean folder_has_unread_rec = FALSE;
	gboolean store_is_builtin;
	gboolean store_is_subscribable;
	gboolean store_can_be_disabled;
	gboolean any_store_is_subscribable = FALSE;

	/* Chain up to parent's update_actions() method. */
	E_SHELL_VIEW_CLASS (e_mail_shell_view_parent_class)->update_actions (shell_view);

	mail_shell_view = E_MAIL_SHELL_VIEW (shell_view);
	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);

	display = e_mail_reader_get_mail_display (reader);
	if (gtk_widget_is_visible (GTK_WIDGET (display)))
		e_web_view_update_actions (E_WEB_VIEW (display));

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
	folder_is_junk =
		(state & E_MAIL_SIDEBAR_FOLDER_IS_JUNK);
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

		folder_is_selected = TRUE;

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
		CamelStore *tmp_store = CAMEL_STORE (link->data);

		if (CAMEL_IS_SUBSCRIBABLE (tmp_store)) {
			any_store_is_subscribable = TRUE;
			break;
		}
	}
	g_list_free (list);

	action = ACTION (MAIL_ACCOUNT_DISABLE);
	sensitive = folder_is_store && store_can_be_disabled;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_ACCOUNT_EXPUNGE);
	sensitive = folder_is_trash;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_ACCOUNT_EMPTY_JUNK);
	sensitive = folder_is_junk;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_ACCOUNT_PROPERTIES);
	sensitive = folder_is_store && !store_is_builtin;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_ACCOUNT_REFRESH);
	sensitive = folder_is_store;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FLUSH_OUTBOX);
	sensitive = folder_is_outbox;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_COPY);
	sensitive = folder_is_selected;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_DELETE);
	sensitive = folder_is_selected && folder_can_be_deleted;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_EDIT_SORT_ORDER);
	sensitive = folder_is_selected || folder_is_store;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_EXPUNGE);
	sensitive = folder_is_selected && (!folder_is_virtual || mail_shell_view->priv->vfolder_allow_expunge);
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_MOVE);
	sensitive = folder_is_selected && folder_can_be_deleted;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_NEW);
	sensitive = folder_allows_children;
	e_ui_action_set_sensitive (action, sensitive);
	e_ui_action_set_sensitive (ACTION (MAIL_FOLDER_NEW_FULL), sensitive);

	action = ACTION (MAIL_FOLDER_PROPERTIES);
	sensitive = folder_is_selected;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_REFRESH);
	sensitive = folder_is_selected;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_RENAME);
	sensitive =
		folder_is_selected &&
		folder_can_be_deleted;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_SELECT_THREAD);
	sensitive = folder_is_selected;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_SELECT_SUBTHREAD);
	sensitive = folder_is_selected;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_UNSUBSCRIBE);
	sensitive =
		folder_is_selected &&
		store_is_subscribable &&
		!folder_is_virtual;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_FOLDER_MARK_ALL_AS_READ);
	sensitive = folder_is_selected && folder_has_unread;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_POPUP_FOLDER_MARK_ALL_AS_READ);
	sensitive = folder_is_selected && folder_has_unread_rec;
	e_ui_action_set_visible (action, sensitive);

	action = ACTION (MAIL_MANAGE_SUBSCRIPTIONS);
	sensitive = folder_is_store && store_is_subscribable;
	e_ui_action_set_sensitive (action, sensitive);

	action = ACTION (MAIL_TOOLS_SUBSCRIPTIONS);
	sensitive = any_store_is_subscribable;
	e_ui_action_set_sensitive (action, sensitive);
}

static gboolean
e_mail_shell_view_ui_manager_create_item_cb (EUIManager *ui_manager,
					     EUIElement *elem,
					     EUIAction *action,
					     EUIElementKind for_kind,
					     GObject **out_item,
					     gpointer user_data)
{
	EMailShellView *self = user_data;
	const gchar *name;

	g_return_val_if_fail (E_IS_MAIL_SHELL_VIEW (self), FALSE);

	name = g_action_get_name (G_ACTION (action));

	if (!g_str_has_prefix (name, "EMailShellView::"))
		return FALSE;

	#define is_action(_nm) (g_strcmp0 (name, (_nm)) == 0)

	if (is_action ("EMailShellView::mail-send-receive")) {
		EUIAction *button_action = e_ui_manager_get_action (ui_manager, "mail-send-receive");
		*out_item = e_ui_manager_create_item_from_menu_model (ui_manager, elem, button_action, for_kind, G_MENU_MODEL (self->priv->send_receive_menu));
	} else if (for_kind == E_UI_ELEMENT_KIND_MENU) {
		g_warning ("%s: Unhandled menu action '%s'", G_STRFUNC, name);
	} else if (for_kind == E_UI_ELEMENT_KIND_TOOLBAR) {
		g_warning ("%s: Unhandled toolbar action '%s'", G_STRFUNC, name);
	} else if (for_kind == E_UI_ELEMENT_KIND_HEADERBAR) {
		g_warning ("%s: Unhandled headerbar action '%s'", G_STRFUNC, name);
	} else {
		g_warning ("%s: Unhandled element kind '%d' for action '%s'", G_STRFUNC, (gint) for_kind, name);
	}

	#undef is_action

	return TRUE;
}

static gboolean
e_mail_shell_view_ui_manager_ignore_accel_cb (EUIManager *ui_manager,
					      EUIAction *action,
					      gpointer user_data)
{
	EMailShellView *self = user_data;
	EMailView *mail_view;
	EShellContent *shell_content;

	g_return_val_if_fail (E_IS_MAIL_SHELL_VIEW (self), FALSE);

	shell_content = e_shell_view_get_shell_content (E_SHELL_VIEW (self));
	mail_view = e_mail_shell_content_get_mail_view (E_MAIL_SHELL_CONTENT (shell_content));

	return e_mail_reader_ignore_accel (E_MAIL_READER (mail_view));
}

static GtkWidget *
e_mail_shell_view_ref_shell_content (EShellView *shell_view)
{
	EMailShellView *self;

	g_return_val_if_fail (E_IS_MAIL_SHELL_VIEW (shell_view), NULL);

	self = E_MAIL_SHELL_VIEW (shell_view);

	return g_object_ref (GTK_WIDGET (self->priv->mail_shell_content));
}

static void
mail_shell_view_init_ui_data (EShellView *shell_view)
{
	EMailShellView *self;

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (shell_view));

	self = E_MAIL_SHELL_VIEW (shell_view);

	g_signal_connect_object (e_shell_view_get_ui_manager (shell_view), "create-item",
		G_CALLBACK (e_mail_shell_view_ui_manager_create_item_cb), shell_view, 0);
	g_signal_connect_object (e_shell_view_get_ui_manager (shell_view), "ignore-accel",
		G_CALLBACK (e_mail_shell_view_ui_manager_ignore_accel_cb), shell_view, 0);

	e_mail_reader_init_ui_data (E_MAIL_READER (e_mail_shell_content_get_mail_view (self->priv->mail_shell_content)));
	e_mail_shell_view_actions_init (self);
	e_mail_shell_view_fill_send_receive_menu (self);
}

static void
mail_shell_view_add_ui_customizers (EShellView *shell_view,
				    EUICustomizeDialog *dialog)
{
	EMailShellView *self;
	EMailReader *reader;
	EMailDisplay *mail_display;
	EUIManager *ui_manager;

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (shell_view));

	self = E_MAIL_SHELL_VIEW (shell_view);
	reader = E_MAIL_READER (e_mail_shell_content_get_mail_view (self->priv->mail_shell_content));
	mail_display = e_mail_reader_get_mail_display (reader);
	ui_manager = e_web_view_get_ui_manager (E_WEB_VIEW (mail_display));

	e_ui_customize_dialog_add_customizer (dialog, e_ui_manager_get_customizer (ui_manager));
}

static void
e_mail_shell_view_class_init (EMailShellViewClass *class)
{
	GObjectClass *object_class;
	EShellViewClass *shell_view_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_shell_view_set_property;
	object_class->get_property = mail_shell_view_get_property;
	object_class->dispose = mail_shell_view_dispose;
	object_class->finalize = mail_shell_view_finalize;
	object_class->constructed = mail_shell_view_constructed;

	shell_view_class = E_SHELL_VIEW_CLASS (class);
	shell_view_class->label = _("Mail");
	shell_view_class->icon_name = "evolution-mail";
	shell_view_class->ui_definition = "evolution-mail.eui";
	shell_view_class->ui_manager_id = "org.gnome.evolution.mail";
	shell_view_class->search_context_type = EM_SEARCH_TYPE_CONTEXT;
	shell_view_class->search_rules = "searchtypes.xml";
	shell_view_class->new_shell_content = e_mail_shell_view_ref_shell_content;
	shell_view_class->new_shell_sidebar = e_mail_shell_sidebar_new;
	shell_view_class->custom_search = mail_shell_view_custom_search;
	shell_view_class->execute_search = mail_shell_view_execute_search;
	shell_view_class->update_actions = mail_shell_view_update_actions;
	shell_view_class->init_ui_data = mail_shell_view_init_ui_data;
	shell_view_class->add_ui_customizers = mail_shell_view_add_ui_customizers;

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
	mail_shell_view->priv = e_mail_shell_view_get_instance_private (mail_shell_view);

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

