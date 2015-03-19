/*
 * e-mail-shell-view-actions.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-mail-shell-view-private.h"

static void
mail_shell_view_folder_created_cb (EMailFolderCreateDialog *dialog,
                                   CamelStore *store,
                                   const gchar *folder_name,
                                   GWeakRef *folder_tree_weak_ref)
{
	EMFolderTree *folder_tree;

	folder_tree = g_weak_ref_get (folder_tree_weak_ref);

	if (folder_tree != NULL) {
		gchar *folder_uri;

		/* Select the newly created folder. */
		folder_uri = e_mail_folder_uri_build (store, folder_name);
		em_folder_tree_set_selected (folder_tree, folder_uri, FALSE);
		g_free (folder_uri);

		g_object_unref (folder_tree);
	}
}

static void
action_mail_account_disable_cb (GtkAction *action,
                                EMailShellView *mail_shell_view)
{
	EMailShellSidebar *mail_shell_sidebar;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellBackend *shell_backend;
	EMailBackend *backend;
	EMailSession *session;
	EMailAccountStore *account_store;
	EMFolderTree *folder_tree;
	CamelStore *store;

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);
	account_store = e_mail_ui_session_get_account_store (
		E_MAIL_UI_SESSION (session));

	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);
	store = em_folder_tree_ref_selected_store (folder_tree);
	g_return_if_fail (store != NULL);

	e_mail_account_store_disable_service (
		account_store,
		GTK_WINDOW (shell_window),
		CAMEL_SERVICE (store));

	e_shell_view_update_actions (shell_view);

	g_object_unref (store);
}

static void
action_mail_account_properties_cb (GtkAction *action,
                                   EMailShellView *mail_shell_view)
{
	EMailShellSidebar *mail_shell_sidebar;
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellBackend *shell_backend;
	ESourceRegistry *registry;
	ESource *source;
	EMFolderTree *folder_tree;
	CamelService *service;
	CamelStore *store;
	const gchar *uid;

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_backend_get_shell (shell_backend);

	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);
	store = em_folder_tree_ref_selected_store (folder_tree);
	g_return_if_fail (store != NULL);

	service = CAMEL_SERVICE (store);
	uid = camel_service_get_uid (service);
	registry = e_shell_get_registry (shell);
	source = e_source_registry_ref_source (registry, uid);
	g_return_if_fail (source != NULL);

	e_mail_shell_backend_edit_account (
		E_MAIL_SHELL_BACKEND (shell_backend),
		GTK_WINDOW (shell_window), source);

	g_object_unref (source);
	g_object_unref (store);
}

static void
account_refresh_folder_info_received_cb (GObject *source,
                                         GAsyncResult *result,
                                         gpointer user_data)
{
	CamelStore *store;
	CamelFolderInfo *info;
	EActivity *activity;
	GError *error = NULL;

	store = CAMEL_STORE (source);
	activity = E_ACTIVITY (user_data);

	info = camel_store_get_folder_info_finish (store, result, &error);

	/* Provider takes care of notifications of new/removed
	 * folders, thus it's enough to free the returned list. */
	camel_folder_info_free (info);

	if (e_activity_handle_cancellation (activity, error)) {
		g_error_free (error);

	} else if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	g_clear_object (&activity);
}

static void
action_mail_account_refresh_cb (GtkAction *action,
                                EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	EMailShellSidebar *mail_shell_sidebar;
	EMFolderTree *folder_tree;
	EMailView *mail_view;
	EActivity *activity;
	ESourceRegistry *registry;
	ESource *source;
	EShell *shell;
	CamelStore *store;
	GCancellable *cancellable;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;

	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);
	store = em_folder_tree_ref_selected_store (folder_tree);
	g_return_if_fail (store != NULL);

	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);
	activity = e_mail_reader_new_activity (E_MAIL_READER (mail_view));
	cancellable = e_activity_get_cancellable (activity);

	shell = e_shell_backend_get_shell (e_shell_view_get_shell_backend (E_SHELL_VIEW (mail_shell_view)));
	registry = e_shell_get_registry (shell);
	source = e_source_registry_ref_source (registry, camel_service_get_uid (CAMEL_SERVICE (store)));
	g_return_if_fail (source != NULL);

	e_shell_allow_auth_prompt_for (shell, source);

	camel_store_get_folder_info (
		store, NULL,
		CAMEL_STORE_FOLDER_INFO_RECURSIVE,
		G_PRIORITY_DEFAULT, cancellable,
		account_refresh_folder_info_received_cb, activity);

	g_clear_object (&source);
	g_clear_object (&store);
}

static void
action_mail_create_search_folder_cb (GtkAction *action,
                                     EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	EMailReader *reader;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EShellSearchbar *searchbar;
	EFilterRule *search_rule;
	EMVFolderRule *vfolder_rule;
	EMailBackend *backend;
	EMailSession *session;
	EMailView *mail_view;
	CamelFolder *folder;
	const gchar *search_text;
	gchar *folder_uri;
	gchar *rule_name;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);
	searchbar = e_mail_shell_content_get_searchbar (mail_shell_content);

	search_rule = e_shell_view_get_search_rule (shell_view);
	g_return_if_fail (search_rule != NULL);

	search_text = e_shell_searchbar_get_search_text (searchbar);
	if (search_text == NULL || *search_text == '\0')
		search_text = "''";

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);

	search_rule = vfolder_clone_rule (session, search_rule);
	g_return_if_fail (search_rule != NULL);

	rule_name = g_strdup_printf ("%s %s", search_rule->name, search_text);
	e_filter_rule_set_source (search_rule, E_FILTER_SOURCE_INCOMING);
	e_filter_rule_set_name (search_rule, rule_name);
	g_free (rule_name);

	reader = E_MAIL_READER (mail_view);
	folder = e_mail_reader_ref_folder (reader);
	folder_uri = e_mail_folder_uri_from_folder (folder);

	vfolder_rule = EM_VFOLDER_RULE (search_rule);
	em_vfolder_rule_add_source (vfolder_rule, folder_uri);
	vfolder_gui_add_rule (vfolder_rule);

	g_clear_object (&folder);
	g_free (folder_uri);
}

static void
action_mail_download_finished_cb (CamelStore *store,
                                  GAsyncResult *result,
                                  EActivity *activity)
{
	EAlertSink *alert_sink;
	GError *error = NULL;

	alert_sink = e_activity_get_alert_sink (activity);

	e_mail_store_prepare_for_offline_finish (store, result, &error);

	if (e_activity_handle_cancellation (activity, error)) {
		g_error_free (error);

	} else if (error != NULL) {
		e_alert_submit (
			alert_sink, "mail:prepare-for-offline",
			error->message, NULL);
		g_error_free (error);
	}

	g_object_unref (activity);
}

static void
action_mail_download_cb (GtkAction *action,
                         EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	EMailView *mail_view;
	EMailReader *reader;
	EMailBackend *backend;
	EMailSession *session;
	GList *list, *link;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	backend = e_mail_reader_get_backend (reader);
	session = e_mail_backend_get_session (backend);

	list = camel_session_list_services (CAMEL_SESSION (session));

	for (link = list; link != NULL; link = g_list_next (link)) {
		EActivity *activity;
		CamelService *service;
		GCancellable *cancellable;

		service = CAMEL_SERVICE (link->data);

		if (!CAMEL_IS_STORE (service))
			continue;

		activity = e_mail_reader_new_activity (reader);
		cancellable = e_activity_get_cancellable (activity);

		e_mail_store_prepare_for_offline (
			CAMEL_STORE (service), G_PRIORITY_DEFAULT,
			cancellable, (GAsyncReadyCallback)
			action_mail_download_finished_cb, activity);
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);
}

static void
action_mail_flush_outbox_cb (GtkAction *action,
                             EMailShellView *mail_shell_view)
{
	EShellBackend *shell_backend;
	EShellView *shell_view;
	EMailBackend *backend;
	EMailSession *session;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);

	mail_send (session);
}

static void
action_mail_folder_copy_cb (GtkAction *action,
                            EMailShellView *mail_shell_view)
{
	EMailShellSidebar *mail_shell_sidebar;
	EShellContent *shell_content;
	EShellWindow *shell_window;
	EShellView *shell_view;
	EMFolderTree *folder_tree;
	EMailSession *session;
	gchar *selected_uri;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);
	selected_uri = em_folder_tree_get_selected_uri (folder_tree);
	session = em_folder_tree_get_session (folder_tree);
	g_return_if_fail (selected_uri != NULL);

	em_folder_utils_copy_folder (
		GTK_WINDOW (shell_window),
		session,
		E_ALERT_SINK (shell_content),
		selected_uri, FALSE);

	g_free (selected_uri);
}

static void
action_mail_folder_delete_cb (GtkAction *action,
                              EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	EMailShellSidebar *mail_shell_sidebar;
	EMailView *mail_view;
	EMFolderTree *folder_tree;
	CamelStore *selected_store = NULL;
	gchar *selected_folder_name = NULL;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	em_folder_tree_get_selected (
		folder_tree, &selected_store, &selected_folder_name);
	g_return_if_fail (CAMEL_IS_STORE (selected_store));
	g_return_if_fail (selected_folder_name != NULL);

	e_mail_reader_delete_folder_name (
		E_MAIL_READER (mail_view),
		selected_store, selected_folder_name);

	g_object_unref (selected_store);
	g_free (selected_folder_name);
}

static void
action_mail_folder_expunge_cb (GtkAction *action,
                               EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	EMailShellSidebar *mail_shell_sidebar;
	EMailView *mail_view;
	EMFolderTree *folder_tree;
	CamelStore *selected_store = NULL;
	gchar *selected_folder_name = NULL;

	/* This handles both the "folder-expunge" and "account-expunge"
	 * actions. */

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	/* Get the folder from the folder tree, not the message list.
	 * This correctly handles the use case of right-clicking on
	 * the "Trash" folder and selecting "Empty Trash" without
	 * actually selecting the folder.  In that case the message
	 * list would not contain the correct folder to expunge. */
	em_folder_tree_get_selected (
		folder_tree, &selected_store, &selected_folder_name);
	g_return_if_fail (CAMEL_IS_STORE (selected_store));
	g_return_if_fail (selected_folder_name != NULL);

	e_mail_reader_expunge_folder_name (
		E_MAIL_READER (mail_view),
		selected_store, selected_folder_name);

	g_object_unref (selected_store);
	g_free (selected_folder_name);
}

typedef struct _AsyncContext {
	EActivity *activity;
	EMailShellView *mail_shell_view;
	gboolean can_subfolders;
	GQueue folder_names;
} AsyncContext;

static void
async_context_free (AsyncContext *context)
{
	if (context->activity != NULL)
		g_object_unref (context->activity);

	if (context->mail_shell_view != NULL)
		g_object_unref (context->mail_shell_view);

	/* This should be empty already, unless an error occurred... */
	while (!g_queue_is_empty (&context->folder_names))
		g_free (g_queue_pop_head (&context->folder_names));

	g_slice_free (AsyncContext, context);
}

static void
mark_all_read_thread (GSimpleAsyncResult *simple,
                      GObject *object,
                      GCancellable *cancellable)
{
	AsyncContext *context;
	CamelStore *store;
	CamelFolder *folder;
	GPtrArray *uids;
	gint ii;
	GError *error = NULL;

	context = g_simple_async_result_get_op_res_gpointer (simple);
	store = CAMEL_STORE (object);

	while (!g_queue_is_empty (&context->folder_names) && !error) {
		gchar *folder_name;

		folder_name = g_queue_pop_head (&context->folder_names);
		folder = camel_store_get_folder_sync (
			store, folder_name, 0, cancellable, &error);
		g_free (folder_name);

		if (folder == NULL)
			break;

		camel_folder_freeze (folder);

		uids = camel_folder_get_uids (folder);

		for (ii = 0; ii < uids->len; ii++)
			camel_folder_set_message_flags (
				folder, uids->pdata[ii],
				CAMEL_MESSAGE_SEEN,
				CAMEL_MESSAGE_SEEN);

		camel_folder_thaw (folder);

		/* Save changes to the server immediately. */
		camel_folder_synchronize_sync (folder, FALSE, cancellable, &error);

		camel_folder_free_uids (folder, uids);
		g_object_unref (folder);
	}

	if (error != NULL)
		g_simple_async_result_take_error (simple, error);
}

static void
mark_all_read_done_cb (GObject *source,
                       GAsyncResult *result,
                       gpointer user_data)
{
	GSimpleAsyncResult *simple;
	AsyncContext *context;
	GError *local_error = NULL;

	g_return_if_fail (
		g_simple_async_result_is_valid (
		result, source, mark_all_read_thread));

	simple = G_SIMPLE_ASYNC_RESULT (result);
	context = g_simple_async_result_get_op_res_gpointer (simple);

	if (g_simple_async_result_propagate_error (simple, &local_error) &&
	    local_error &&
	    !g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		EAlertSink *alert_sink;

		alert_sink = e_activity_get_alert_sink (context->activity);

		e_alert_submit (
			alert_sink, "mail:mark-all-read",
			local_error->message, NULL);
	}

	g_clear_error (&local_error);

	e_activity_set_state (context->activity, E_ACTIVITY_COMPLETED);
}

static void
mark_all_read_collect_folder_names (GQueue *folder_names,
                                    CamelFolderInfo *folder_info)
{
	while (folder_info != NULL) {
		if (folder_info->child != NULL)
			mark_all_read_collect_folder_names (
				folder_names, folder_info->child);

		g_queue_push_tail (
			folder_names, g_strdup (folder_info->full_name));

		folder_info = folder_info->next;
	}
}

enum {
	MARK_ALL_READ_CANCEL,
	MARK_ALL_READ_CURRENT_ONLY,
	MARK_ALL_READ_WITH_SUBFOLDERS
};

static gint
mark_all_read_prompt_user (EMailShellView *mail_shell_view,
                           gboolean with_subfolders)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkWindow *parent;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	parent = GTK_WINDOW (shell_window);

	if (with_subfolders) {
		switch (e_alert_run_dialog_for_args (parent,
			"mail:ask-mark-all-read-sub", NULL)) {
			case GTK_RESPONSE_YES:
				return MARK_ALL_READ_WITH_SUBFOLDERS;
			case GTK_RESPONSE_NO:
				return MARK_ALL_READ_CURRENT_ONLY;
			default:
				break;
		}
	} else if (e_util_prompt_user (parent,
			"org.gnome.evolution.mail",
			"prompt-on-mark-all-read",
			"mail:ask-mark-all-read", NULL))
		return MARK_ALL_READ_CURRENT_ONLY;

	return MARK_ALL_READ_CANCEL;
}

static void
mark_all_read_got_folder_info (GObject *source,
                               GAsyncResult *result,
                               gpointer user_data)
{
	CamelStore *store = CAMEL_STORE (source);
	AsyncContext *context = user_data;
	EAlertSink *alert_sink;
	GCancellable *cancellable;
	GSimpleAsyncResult *simple;
	CamelFolderInfo *folder_info;
	gint response;
	GError *error = NULL;

	alert_sink = e_activity_get_alert_sink (context->activity);
	cancellable = e_activity_get_cancellable (context->activity);

	folder_info = camel_store_get_folder_info_finish (
		store, result, &error);

	if (e_activity_handle_cancellation (context->activity, error)) {
		g_warn_if_fail (folder_info == NULL);
		async_context_free (context);
		g_error_free (error);
		return;

	} else if (error != NULL) {
		g_warn_if_fail (folder_info == NULL);
		e_alert_submit (
			alert_sink, "mail:mark-all-read",
			error->message, NULL);
		async_context_free (context);
		g_error_free (error);
		return;
	}

	if (!folder_info) {
		/* Otherwise the operation is stuck and the Evolution cannot be quit */
		g_warn_if_fail (folder_info != NULL);
		e_activity_set_state (context->activity, E_ACTIVITY_COMPLETED);
		async_context_free (context);
		return;
	}

	response = mark_all_read_prompt_user (
		context->mail_shell_view,
		context->can_subfolders && folder_info->child != NULL);

	if (response == MARK_ALL_READ_CURRENT_ONLY)
		g_queue_push_tail (
			&context->folder_names,
			g_strdup (folder_info->full_name));

	if (response == MARK_ALL_READ_WITH_SUBFOLDERS)
		mark_all_read_collect_folder_names (
			&context->folder_names, folder_info);

	camel_folder_info_free (folder_info);

	if (g_queue_is_empty (&context->folder_names)) {
		e_activity_set_state (context->activity, E_ACTIVITY_COMPLETED);
		async_context_free (context);
		return;
	}

	simple = g_simple_async_result_new (
		source, mark_all_read_done_cb,
		context, mark_all_read_thread);

	g_simple_async_result_set_op_res_gpointer (
		simple, context, (GDestroyNotify) async_context_free);

	g_simple_async_result_run_in_thread (
		simple, mark_all_read_thread,
		G_PRIORITY_DEFAULT, cancellable);

	g_object_unref (simple);
}

static void
e_mail_shell_view_actions_mark_all_read (EMailShellView *mail_shell_view,
                                         CamelStore *store,
                                         const gchar *folder_name,
                                         gboolean can_subfolders)
{
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EShellContent *shell_content;
	EAlertSink *alert_sink;
	GCancellable *cancellable;
	AsyncContext *context;

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (folder_name != NULL);

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);

	context = g_slice_new0 (AsyncContext);
	context->mail_shell_view = g_object_ref (mail_shell_view);
	context->can_subfolders = can_subfolders;
	context->activity = e_activity_new ();
	g_queue_init (&context->folder_names);

	alert_sink = E_ALERT_SINK (shell_content);
	e_activity_set_alert_sink (context->activity, alert_sink);

	cancellable = camel_operation_new ();
	e_activity_set_cancellable (context->activity, cancellable);

	camel_operation_push_message (
		cancellable, _("Marking messages as read..."));

	e_shell_backend_add_activity (shell_backend, context->activity);

	camel_store_get_folder_info (
		store, folder_name,
		can_subfolders ? CAMEL_STORE_FOLDER_INFO_RECURSIVE : 0,
		G_PRIORITY_DEFAULT, cancellable,
		mark_all_read_got_folder_info, context);

	g_object_unref (cancellable);
}

static void
action_mail_folder_mark_all_as_read_cb (GtkAction *action,
                                        EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	EMailReader *reader;
	EMailView *mail_view;
	CamelFolder *folder;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);

	folder = e_mail_reader_ref_folder (reader);
	g_return_if_fail (folder != NULL);

	if (folder->summary != NULL &&
	    camel_folder_summary_get_unread_count (folder->summary) == 0) {
		g_object_unref (folder);
		return;
	}

	e_mail_shell_view_actions_mark_all_read (
		mail_shell_view,
		camel_folder_get_parent_store (folder),
		camel_folder_get_full_name (folder),
		FALSE);

	g_object_unref (folder);
}

static void
action_mail_popup_folder_mark_all_as_read_cb (GtkAction *action,
                                              EMailShellView *mail_shell_view)
{
	EMailShellSidebar *mail_shell_sidebar;
	EMFolderTree *folder_tree;
	CamelStore *store = NULL;
	gchar *folder_name = NULL;

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);
	em_folder_tree_get_selected (folder_tree, &store, &folder_name);

	/* This action should only be activatable if a folder is selected. */
	g_return_if_fail (store != NULL && folder_name != NULL);

	e_mail_shell_view_actions_mark_all_read (
		mail_shell_view, store, folder_name, TRUE);

	g_object_unref (store);
	g_free (folder_name);
}

static void
action_mail_folder_move_cb (GtkAction *action,
                            EMailShellView *mail_shell_view)
{
	EMailShellSidebar *mail_shell_sidebar;
	EShellContent *shell_content;
	EShellWindow *shell_window;
	EShellView *shell_view;
	EMFolderTree *folder_tree;
	EMailSession *session;
	gchar *selected_uri;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);
	selected_uri = em_folder_tree_get_selected_uri (folder_tree);
	session = em_folder_tree_get_session (folder_tree);
	g_return_if_fail (selected_uri != NULL);

	em_folder_utils_copy_folder (
		GTK_WINDOW (shell_window),
		session,
		E_ALERT_SINK (shell_content),
		selected_uri, TRUE);

	g_free (selected_uri);
}

static void
action_mail_folder_new_cb (GtkAction *action,
                           EMailShellView *mail_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EMailSession *session;
	EMailShellSidebar *mail_shell_sidebar;
	EMFolderTree *folder_tree;
	GtkWidget *dialog;
	CamelStore *store = NULL;
	gchar *folder_name = NULL;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	session = em_folder_tree_get_session (folder_tree);

	dialog = e_mail_folder_create_dialog_new (
		GTK_WINDOW (shell_window),
		E_MAIL_UI_SESSION (session));

	g_signal_connect_data (
		dialog, "folder-created",
		G_CALLBACK (mail_shell_view_folder_created_cb),
		e_weak_ref_new (folder_tree),
		(GClosureNotify) e_weak_ref_free, 0);

	if (em_folder_tree_get_selected (folder_tree, &store, &folder_name)) {
		em_folder_selector_set_selected (
			EM_FOLDER_SELECTOR (dialog), store, folder_name);
		g_object_unref (store);
		g_free (folder_name);
	}

	gtk_widget_show (GTK_WIDGET (dialog));
}

static void
action_mail_folder_properties_cb (GtkAction *action,
                                  EMailShellView *mail_shell_view)
{
	EMailShellSidebar *mail_shell_sidebar;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellContent *shell_content;
	EMFolderTree *folder_tree;
	CamelStore *store;
	gchar *folder_name;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	if (!em_folder_tree_get_selected (folder_tree, &store, &folder_name))
		g_return_if_reached ();

	em_folder_properties_show (
		store, folder_name,
		E_ALERT_SINK (shell_content),
		GTK_WINDOW (shell_window));

	g_object_unref (store);
	g_free (folder_name);
}

static void
action_mail_folder_refresh_cb (GtkAction *action,
                               EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	EMailShellSidebar *mail_shell_sidebar;
	EMailView *mail_view;
	EMFolderTree *folder_tree;
	CamelStore *selected_store = NULL;
	gchar *selected_folder_name = NULL;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	em_folder_tree_get_selected (
		folder_tree, &selected_store, &selected_folder_name);
	g_return_if_fail (CAMEL_IS_STORE (selected_store));
	g_return_if_fail (selected_folder_name != NULL);

	e_mail_reader_refresh_folder_name (
		E_MAIL_READER (mail_view),
		selected_store, selected_folder_name);

	g_object_unref (selected_store);
	g_free (selected_folder_name);
}

static void
action_mail_folder_rename_cb (GtkAction *action,
                              EMailShellView *mail_shell_view)
{
	EMailShellSidebar *mail_shell_sidebar;
	EMFolderTree *folder_tree;

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	em_folder_tree_edit_selected (folder_tree);
}

static void
action_mail_folder_select_thread_cb (GtkAction *action,
                                     EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	GtkWidget *message_list;
	EMailReader *reader;
	EMailView *mail_view;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	message_list = e_mail_reader_get_message_list (reader);

	message_list_select_thread (MESSAGE_LIST (message_list));
}

static void
action_mail_folder_select_subthread_cb (GtkAction *action,
                                        EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	GtkWidget *message_list;
	EMailReader *reader;
	EMailView *mail_view;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	message_list = e_mail_reader_get_message_list (reader);

	message_list_select_subthread (MESSAGE_LIST (message_list));
}

static void
action_mail_folder_unsubscribe_cb (GtkAction *action,
                                   EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	EMailShellSidebar *mail_shell_sidebar;
	EMailView *mail_view;
	EMFolderTree *folder_tree;
	CamelStore *selected_store = NULL;
	gchar *selected_folder_name = NULL;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	em_folder_tree_get_selected (
		folder_tree, &selected_store, &selected_folder_name);
	g_return_if_fail (CAMEL_IS_STORE (selected_store));
	g_return_if_fail (selected_folder_name != NULL);

	e_mail_reader_unsubscribe_folder_name (
		E_MAIL_READER (mail_view),
		selected_store, selected_folder_name);

	g_object_unref (selected_store);
	g_free (selected_folder_name);
}

static void
action_mail_global_expunge_cb (GtkAction *action,
                               EMailShellView *mail_shell_view)
{
	EShellBackend *shell_backend;
	EShellWindow *shell_window;
	EShellView *shell_view;
	EMailBackend *backend;
	EMailSession *session;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);

	em_utils_empty_trash (
		GTK_WIDGET (shell_window), session);
}

static void
action_mail_label_cb (GtkToggleAction *action,
                      EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	EMailReader *reader;
	EMailView *mail_view;
	CamelFolder *folder;
	GPtrArray *uids;
	const gchar *tag;
	gint ii;

	tag = g_object_get_data (G_OBJECT (action), "tag");
	g_return_if_fail (tag != NULL);

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	folder = e_mail_reader_ref_folder (reader);
	uids = e_mail_reader_get_selected_uids (reader);

	camel_folder_freeze (folder);
	for (ii = 0; ii < uids->len; ii++) {
		if (gtk_toggle_action_get_active (action))
			camel_folder_set_message_user_flag (
				folder, uids->pdata[ii], tag, TRUE);
		else {
			camel_folder_set_message_user_flag (
				folder, uids->pdata[ii], tag, FALSE);
			camel_folder_set_message_user_tag (
				folder, uids->pdata[ii], "label", NULL);
		}
	}
	camel_folder_thaw (folder);

	g_clear_object (&folder);
	g_ptr_array_unref (uids);
}

static void
action_mail_label_new_cb (GtkAction *action,
                          EMailShellView *mail_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellBackend *shell_backend;
	EMailShellContent *mail_shell_content;
	EMailLabelDialog *label_dialog;
	EMailLabelListStore *label_store;
	EMailBackend *backend;
	EMailSession *session;
	EMailReader *reader;
	EMailView *mail_view;
	CamelFolder *folder;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *dialog;
	GPtrArray *uids;
	GdkColor label_color;
	const gchar *label_name;
	gchar *label_tag;
	gint n_children;
	guint ii;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	dialog = e_mail_label_dialog_new (GTK_WINDOW (shell_window));

	gtk_window_set_title (GTK_WINDOW (dialog), _("Add Label"));

	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK)
		goto exit;

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);
	label_store = e_mail_ui_session_get_label_store (
		E_MAIL_UI_SESSION (session));

	label_dialog = E_MAIL_LABEL_DIALOG (dialog);
	label_name = e_mail_label_dialog_get_label_name (label_dialog);
	e_mail_label_dialog_get_label_color (label_dialog, &label_color);

	e_mail_label_list_store_set (
		label_store, NULL, label_name, &label_color);

	/* XXX This is awkward.  We've added a new label to the list store
	 *     but we don't have the new label's tag nor an iterator to use
	 *     to fetch it.  We know the label was appended to the store,
	 *     so we have to dig it out manually.  EMailLabelListStore API
	 *     probably needs some rethinking. */
	model = GTK_TREE_MODEL (label_store);
	n_children = gtk_tree_model_iter_n_children (model, NULL);
	gtk_tree_model_iter_nth_child (model, &iter, NULL, n_children - 1);
	label_tag = e_mail_label_list_store_get_tag (label_store, &iter);

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	folder = e_mail_reader_ref_folder (reader);
	uids = e_mail_reader_get_selected_uids (reader);

	for (ii = 0; ii < uids->len; ii++)
		camel_folder_set_message_user_flag (
			folder, uids->pdata[ii], label_tag, TRUE);

	g_clear_object (&folder);
	g_ptr_array_unref (uids);

	g_free (label_tag);

exit:
	gtk_widget_destroy (dialog);
}

static void
action_mail_label_none_cb (GtkAction *action,
                           EMailShellView *mail_shell_view)
{
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EMailBackend *backend;
	EMailSession *session;
	EMailShellContent *mail_shell_content;
	EMailLabelListStore *label_store;
	EMailReader *reader;
	EMailView *mail_view;
	CamelFolder *folder;
	GtkTreeIter iter;
	GPtrArray *uids;
	gboolean valid;
	guint ii;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);
	label_store = e_mail_ui_session_get_label_store (
		E_MAIL_UI_SESSION (session));

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	folder = e_mail_reader_ref_folder (reader);
	uids = e_mail_reader_get_selected_uids (reader);

	valid = gtk_tree_model_get_iter_first (
		GTK_TREE_MODEL (label_store), &iter);

	while (valid) {
		gchar *tag;

		tag = e_mail_label_list_store_get_tag (label_store, &iter);

		for (ii = 0; ii < uids->len; ii++) {
			camel_folder_set_message_user_flag (
				folder, uids->pdata[ii], tag, FALSE);
			camel_folder_set_message_user_tag (
				folder, uids->pdata[ii], "label", NULL);
		}

		g_free (tag);

		valid = gtk_tree_model_iter_next (
			GTK_TREE_MODEL (label_store), &iter);
	}

	g_clear_object (&folder);
	g_ptr_array_unref (uids);
}

static void
action_mail_send_receive_cb (GtkAction *action,
                             EMailShellView *mail_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellBackend *shell_backend;
	EMailBackend *backend;
	EMailSession *session;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);

	mail_send_receive (GTK_WINDOW (shell_window), session);
}

static void
action_mail_send_receive_receive_all_cb (GtkAction *action,
                                         EMailShellView *mail_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellBackend *shell_backend;
	EMailBackend *backend;
	EMailSession *session;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);

	mail_receive (GTK_WINDOW (shell_window), session);
}

static void
action_mail_send_receive_send_all_cb (GtkAction *action,
                                      EMailShellView *mail_shell_view)
{
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EMailBackend *backend;
	EMailSession *session;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);

	mail_send (session);
}

static void
action_mail_smart_backward_cb (GtkAction *action,
                               EMailShellView *mail_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EMailShellContent *mail_shell_content;
	EMailShellSidebar *mail_shell_sidebar;
	EMFolderTree *folder_tree;
	EMailReader *reader;
	EMailView *mail_view;
	GtkWidget *message_list;
	GtkToggleAction *toggle_action;
	GtkWidget *window;
	GtkAdjustment *adj;
	EMailDisplay *display;
	GSettings *settings;
	gboolean caret_mode;
	gboolean magic_spacebar;
	gdouble value;

	/* This implements the so-called "Magic Backspace". */

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	reader = E_MAIL_READER (mail_view);
	display = e_mail_reader_get_mail_display (reader);
	message_list = e_mail_reader_get_message_list (reader);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	magic_spacebar = g_settings_get_boolean (settings, "magic-spacebar");
	g_object_unref (settings);

	toggle_action = GTK_TOGGLE_ACTION (ACTION (MAIL_CARET_MODE));
	caret_mode = gtk_toggle_action_get_active (toggle_action);

	window = gtk_widget_get_parent (GTK_WIDGET (display));
	if (!GTK_IS_SCROLLED_WINDOW (window))
		return;

	adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (window));
	value = gtk_adjustment_get_value (adj);
	if (value == 0) {

		if (caret_mode || !magic_spacebar)
			return;

		/* XXX Are two separate calls really necessary? */

		if (message_list_select (
		    MESSAGE_LIST (message_list),
		    MESSAGE_LIST_SELECT_PREVIOUS |
		    MESSAGE_LIST_SELECT_INCLUDE_COLLAPSED,
		    0, CAMEL_MESSAGE_SEEN))
			return;

		if (message_list_select (
		    MESSAGE_LIST (message_list),
		    MESSAGE_LIST_SELECT_PREVIOUS |
		    MESSAGE_LIST_SELECT_WRAP |
		    MESSAGE_LIST_SELECT_INCLUDE_COLLAPSED,
		    0, CAMEL_MESSAGE_SEEN))
			return;

		em_folder_tree_select_next_path (folder_tree, TRUE);

		gtk_widget_grab_focus (message_list);

	} else {

		gtk_adjustment_set_value (
			adj,
			value - gtk_adjustment_get_page_increment (adj));

		return;
	}
}

static void
action_mail_smart_forward_cb (GtkAction *action,
                              EMailShellView *mail_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EMailShellContent *mail_shell_content;
	EMailShellSidebar *mail_shell_sidebar;
	EMFolderTree *folder_tree;
	EMailReader *reader;
	EMailView *mail_view;
	GtkWidget *message_list;
	GtkWidget *window;
	GtkAdjustment *adj;
	GtkToggleAction *toggle_action;
	EMailDisplay *display;
	GSettings *settings;
	gboolean caret_mode;
	gboolean magic_spacebar;
	gdouble value;
	gdouble upper;

	/* This implements the so-called "Magic Spacebar". */

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	reader = E_MAIL_READER (mail_view);
	display = e_mail_reader_get_mail_display (reader);
	message_list = e_mail_reader_get_message_list (reader);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	magic_spacebar = g_settings_get_boolean (settings, "magic-spacebar");
	g_object_unref (settings);

	toggle_action = GTK_TOGGLE_ACTION (ACTION (MAIL_CARET_MODE));
	caret_mode = gtk_toggle_action_get_active (toggle_action);

	window = gtk_widget_get_parent (GTK_WIDGET (display));
	if (!GTK_IS_SCROLLED_WINDOW (window))
		return;

	adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (window));
	value = gtk_adjustment_get_value (adj);
	upper = gtk_adjustment_get_upper (adj);
	if (value + gtk_adjustment_get_page_size (adj) >= upper) {

		if (caret_mode || !magic_spacebar)
			return;

		/* XXX Are two separate calls really necessary? */

		if (message_list_select (
		    MESSAGE_LIST (message_list),
		    MESSAGE_LIST_SELECT_NEXT |
		    MESSAGE_LIST_SELECT_INCLUDE_COLLAPSED,
		    0, CAMEL_MESSAGE_SEEN))
			return;

		if (message_list_select (
		    MESSAGE_LIST (message_list),
		    MESSAGE_LIST_SELECT_NEXT |
		    MESSAGE_LIST_SELECT_WRAP |
		    MESSAGE_LIST_SELECT_INCLUDE_COLLAPSED,
		    0, CAMEL_MESSAGE_SEEN))
			return;

		em_folder_tree_select_next_path (folder_tree, TRUE);

		gtk_widget_grab_focus (message_list);

	} else {

		gtk_adjustment_set_value (
			adj,
			value + gtk_adjustment_get_page_increment (adj));

		return;
	}
}

static void
action_mail_stop_cb (GtkAction *action,
                     EMailShellView *mail_shell_view)
{
	EShellView *shell_view;
	EShellBackend *shell_backend;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	e_shell_backend_cancel_all (shell_backend);
}

static void
action_mail_threads_collapse_all_cb (GtkAction *action,
                                     EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	GtkWidget *message_list;
	EMailReader *reader;
	EMailView *mail_view;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	message_list = e_mail_reader_get_message_list (reader);

	message_list_set_threaded_collapse_all (MESSAGE_LIST (message_list));
}

static void
action_mail_threads_expand_all_cb (GtkAction *action,
                                   EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	GtkWidget *message_list;
	EMailReader *reader;
	EMailView *mail_view;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	message_list = e_mail_reader_get_message_list (reader);

	message_list_set_threaded_expand_all (MESSAGE_LIST (message_list));
}

static void
action_mail_tools_filters_cb (GtkAction *action,
                              EMailShellView *mail_shell_view)
{
	EShellBackend *shell_backend;
	EShellContent *shell_content;
	EShellWindow *shell_window;
	EShellView *shell_view;
	EMailBackend *backend;
	EMailSession *session;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);

	em_utils_edit_filters (
		session,
		E_ALERT_SINK (shell_content),
		GTK_WINDOW (shell_window));
}

static void
action_mail_tools_search_folders_cb (GtkAction *action,
                                     EMailShellView *mail_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellBackend *shell_backend;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	vfolder_edit (
		E_MAIL_BACKEND (shell_backend),
		GTK_WINDOW (shell_window));
}

static void
action_mail_tools_subscriptions_cb (GtkAction *action,
                                    EMailShellView *mail_shell_view)
{
	EMailShellSidebar *mail_shell_sidebar;
	EShellWindow *shell_window;
	EShellView *shell_view;
	EMailSession *session;
	EMFolderTree *folder_tree;
	GtkWidget *dialog;
	CamelStore *store;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);
	session = em_folder_tree_get_session (folder_tree);

	/* The subscription editor's initial store can be NULL. */
	store = em_folder_tree_ref_selected_store (folder_tree);

	dialog = em_subscription_editor_new (
		GTK_WINDOW (shell_window), session, store);

	if (store != NULL)
		g_object_unref (store);

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static void
action_mail_view_cb (GtkRadioAction *action,
                     GtkRadioAction *current,
                     EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	GtkOrientation orientation;
	EMailView *mail_view;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

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

	e_mail_view_set_orientation (mail_view, orientation);
}

static GtkActionEntry mail_entries[] = {

	{ "mail-account-disable",
	  NULL,
	  N_("_Disable Account"),
	  NULL,
	  N_("Disable this account"),
	  G_CALLBACK (action_mail_account_disable_cb) },

	{ "mail-account-expunge",
	  NULL,
	  N_("_Empty Trash"),
	  NULL,
	  N_("Permanently remove all the deleted messages from all folders"),
	  G_CALLBACK (action_mail_folder_expunge_cb) },

	{ "mail-account-properties",
	  "document-properties",
	  N_("_Properties"),
	  NULL,
	  N_("Edit properties of this account"),
	  G_CALLBACK (action_mail_account_properties_cb) },

	{ "mail-account-refresh",
	  "view-refresh",
	  N_("_Refresh"),
	  NULL,
	  N_("Refresh list of folders of this account"),
	  G_CALLBACK (action_mail_account_refresh_cb) },

	{ "mail-download",
	  NULL,
	  N_("_Download Messages for Offline Usage"),
	  NULL,
	  N_("Download messages of accounts and folders marked for offline usage"),
	  G_CALLBACK (action_mail_download_cb) },

	{ "mail-flush-outbox",
	  "mail-send",
	  N_("Fl_ush Outbox"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_mail_flush_outbox_cb) },

	{ "mail-folder-copy",
	  "folder-copy",
	  N_("_Copy Folder To..."),
	  NULL,
	  N_("Copy the selected folder into another folder"),
	  G_CALLBACK (action_mail_folder_copy_cb) },

	{ "mail-folder-delete",
	  "edit-delete",
	  N_("_Delete"),
	  NULL,
	  N_("Permanently remove this folder"),
	  G_CALLBACK (action_mail_folder_delete_cb) },

	{ "mail-folder-expunge",
	  NULL,
	  N_("E_xpunge"),
	  "<Control>e",
	  N_("Permanently remove all deleted messages from this folder"),
	  G_CALLBACK (action_mail_folder_expunge_cb) },

	{ "mail-folder-mark-all-as-read",
	  "mail-mark-read",
	  N_("Mar_k All Messages as Read"),
	  "<Control>slash",
	  N_("Mark all messages in the folder as read"),
	  G_CALLBACK (action_mail_folder_mark_all_as_read_cb) },

	{ "mail-folder-move",
	  "folder-move",
	  N_("_Move Folder To..."),
	  NULL,
	  N_("Move the selected folder into another folder"),
	  G_CALLBACK (action_mail_folder_move_cb) },

	{ "mail-folder-new",
	  "folder-new",
	  /* Translators: An action caption to create a new mail folder */
	  N_("_New..."),
	  NULL,
	  N_("Create a new folder for storing mail"),
	  G_CALLBACK (action_mail_folder_new_cb) },

	{ "mail-folder-properties",
	  "document-properties",
	  N_("_Properties"),
	  NULL,
	  N_("Change the properties of this folder"),
	  G_CALLBACK (action_mail_folder_properties_cb) },

	{ "mail-folder-refresh",
	  "view-refresh",
	  N_("_Refresh"),
	  "F5",
	  N_("Refresh the folder"),
	  G_CALLBACK (action_mail_folder_refresh_cb) },

	{ "mail-folder-rename",
	  NULL,
	  N_("_Rename..."),
	  "F2",
	  N_("Change the name of this folder"),
	  G_CALLBACK (action_mail_folder_rename_cb) },

	{ "mail-folder-select-thread",
	  NULL,
	  N_("Select Message _Thread"),
	  "<Control>h",
	  N_("Select all messages in the same thread as the selected message"),
	  G_CALLBACK (action_mail_folder_select_thread_cb) },

	{ "mail-folder-select-subthread",
	  NULL,
	  N_("Select Message S_ubthread"),
	  "<Shift><Control>h",
	  N_("Select all replies to the currently selected message"),
	  G_CALLBACK (action_mail_folder_select_subthread_cb) },

	{ "mail-folder-unsubscribe",
	  NULL,
	  N_("_Unsubscribe"),
	  NULL,
	  N_("Unsubscribe from the selected folder"),
	  G_CALLBACK (action_mail_folder_unsubscribe_cb) },

	{ "mail-global-expunge",
	  NULL,
	  N_("Empty _Trash"),
	  NULL,
	  N_("Permanently remove all the deleted messages from all accounts"),
	  G_CALLBACK (action_mail_global_expunge_cb) },

	{ "mail-label-new",
	  NULL,
	  N_("_New Label"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_mail_label_new_cb) },

	/* Translators: "None" is used in the message label context menu.
	 *              It removes all labels from the selected messages. */
	{ "mail-label-none",
	  NULL,
	  N_("N_one"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_mail_label_none_cb) },

	/* This is the same as "mail-tools-subscriptions" but only
	 * appears in the sidebar context menu when right-clicking
	 * on a store that supports folder subscriptions.  No need
	 * for a special callback because Folder->Subscriptions...
	 * already tries to open the "Folder Subscriptions" dialog
	 * according to the highlighted item in the sidebar, which
	 * is exactly the behavior we want here. */
	{ "mail-manage-subscriptions",
	  NULL,
	  N_("_Manage Subscriptions"),
	  NULL,
	  N_("Subscribe or unsubscribe to folders on remote servers"),
	  G_CALLBACK (action_mail_tools_subscriptions_cb) },

	{ "mail-popup-folder-mark-all-as-read",
	  "mail-mark-read",
	  N_("Mar_k All Messages as Read"),
	  NULL,
	  N_("Mark all messages in the folder as read"),
	  G_CALLBACK (action_mail_popup_folder_mark_all_as_read_cb) },

	{ "mail-send-receive",
	  "mail-send-receive",
	  N_("Send / _Receive"),
	  "F12",
	  N_("Send queued items and retrieve new items"),
	  G_CALLBACK (action_mail_send_receive_cb) },

        { "mail-send-receive-receive-all",
	  NULL,
	  N_("R_eceive All"),
	  NULL,
	  N_("Receive new items from all accounts"),
	  G_CALLBACK (action_mail_send_receive_receive_all_cb) },

        { "mail-send-receive-send-all",
	  "mail-send",
	  N_("_Send All"),
	  NULL,
	  N_("Send queued items in all accounts"),
	  G_CALLBACK (action_mail_send_receive_send_all_cb) },

        { "mail-send-receive-submenu",
	  "mail-send-receive",
	  N_("Send / _Receive"),
	  NULL,
	  NULL,
	  NULL },

	{ "mail-smart-backward",
	  NULL,
	  NULL,  /* No menu item; key press only */
	  NULL,
	  NULL,
	  G_CALLBACK (action_mail_smart_backward_cb) },

	{ "mail-smart-forward",
	  NULL,
	  NULL,  /* No menu item; key press only */
	  NULL,
	  NULL,
	  G_CALLBACK (action_mail_smart_forward_cb) },

	{ "mail-stop",
	  "process-stop",
	  N_("Cancel"),
	  NULL,
	  N_("Cancel the current mail operation"),
	  G_CALLBACK (action_mail_stop_cb) },

	{ "mail-threads-collapse-all",
	  NULL,
	  N_("Collapse All _Threads"),
	  "<Shift><Control>b",
	  N_("Collapse all message threads"),
	  G_CALLBACK (action_mail_threads_collapse_all_cb) },

	{ "mail-threads-expand-all",
	  NULL,
	  N_("E_xpand All Threads"),
	  NULL,
	  N_("Expand all message threads"),
	  G_CALLBACK (action_mail_threads_expand_all_cb) },

	{ "mail-tools-filters",
	  NULL,
	  N_("_Message Filters"),
	  NULL,
	  N_("Create or edit rules for filtering new mail"),
	  G_CALLBACK (action_mail_tools_filters_cb) },

	{ "mail-tools-subscriptions",
	  NULL,
	  N_("_Subscriptions..."),
	  NULL,
	  N_("Subscribe or unsubscribe to folders on remote servers"),
	  G_CALLBACK (action_mail_tools_subscriptions_cb) },

	/*** Menus ***/

	{ "mail-folder-menu",
	  NULL,
	  N_("F_older"),
	  NULL,
	  NULL,
	  NULL },

	{ "mail-label-menu",
	  NULL,
	  N_("_Label"),
	  NULL,
	  NULL,
	  NULL },

	{ "mail-preview-menu",
	  NULL,
	  N_("_Preview"),
	  NULL,
	  NULL,
	  NULL }
};

static GtkActionEntry search_folder_entries[] = {

	{ "mail-create-search-folder",
	  NULL,
	  N_("C_reate Search Folder From Search..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_mail_create_search_folder_cb) },

	{ "mail-tools-search-folders",
	  NULL,
	  N_("Search F_olders"),
	  NULL,
	  N_("Create or edit search folder definitions"),
	  G_CALLBACK (action_mail_tools_search_folders_cb) },
};

static EPopupActionEntry mail_popup_entries[] = {

	{ "mail-popup-account-disable",
	  NULL,
	  "mail-account-disable" },

	{ "mail-popup-account-expunge",
	  NULL,
	  "mail-account-expunge" },

	{ "mail-popup-account-refresh",
	  NULL,
	  "mail-account-refresh" },

	{ "mail-popup-account-properties",
	  NULL,
	  "mail-account-properties" },

	{ "mail-popup-flush-outbox",
	  NULL,
	  "mail-flush-outbox" },

	{ "mail-popup-folder-copy",
	  NULL,
	  "mail-folder-copy" },

	{ "mail-popup-folder-delete",
	  NULL,
	  "mail-folder-delete" },

	{ "mail-popup-folder-move",
	  NULL,
	  "mail-folder-move" },

	{ "mail-popup-folder-new",
	  N_("_New Folder..."),
	  "mail-folder-new" },

	{ "mail-popup-folder-properties",
	  NULL,
	  "mail-folder-properties" },

	{ "mail-popup-folder-refresh",
	  NULL,
	  "mail-folder-refresh" },

	{ "mail-popup-folder-rename",
	  NULL,
	  "mail-folder-rename" },

	{ "mail-popup-folder-unsubscribe",
	  NULL,
	  "mail-folder-unsubscribe" },

	{ "mail-popup-manage-subscriptions",
	  NULL,
	  "mail-manage-subscriptions" }
};

static GtkToggleActionEntry mail_toggle_entries[] = {

	{ "mail-preview",
	  NULL,
	  N_("Show Message _Preview"),
	  "<Control>m",
	  N_("Show message preview pane"),
	  NULL,  /* Handled by property bindings */
	  TRUE },

	{ "mail-show-deleted",
	  NULL,
	  N_("Show _Deleted Messages"),
	  NULL,
	  N_("Show deleted messages with a line through them"),
	  NULL,  /* Handled by property bindings */
	  FALSE },

	{ "mail-threads-group-by",
	  NULL,
	  N_("_Group By Threads"),
	  "<Control>t",
	  N_("Threaded message list"),
	  NULL,  /* Handled by property bindings */
	  FALSE },

	{ "mail-vfolder-unmatched-enable",
	  NULL,
	  N_("_Unmatched Folder Enabled"),
	  NULL,
	  N_("Toggles whether Unmatched search folder is enabled"),
	  NULL }
};

static GtkRadioActionEntry mail_view_entries[] = {

	/* This action represents the initial active mail view.
	 * It should not be visible in the UI, nor should it be
	 * possible to switch to it from another shell view. */
	{ "mail-view-initial",
	  NULL,
	  NULL,
	  NULL,
	  NULL,
	  -1 },

	{ "mail-view-classic",
	  NULL,
	  N_("_Classic View"),
	  NULL,
	  N_("Show message preview below the message list"),
	  0 },

	{ "mail-view-vertical",
	  NULL,
	  N_("_Vertical View"),
	  NULL,
	  N_("Show message preview alongside the message list"),
	  1 }
};

static GtkRadioActionEntry mail_filter_entries[] = {

	{ "mail-filter-all-messages",
	  NULL,
	  N_("All Messages"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_FILTER_ALL_MESSAGES },

	{ "mail-filter-important-messages",
	  "emblem-important",
	  N_("Important Messages"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_FILTER_IMPORTANT_MESSAGES },

	{ "mail-filter-last-5-days-messages",
	  NULL,
	  N_("Last 5 Days' Messages"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_FILTER_LAST_5_DAYS_MESSAGES },

	{ "mail-filter-messages-not-junk",
	  "mail-mark-notjunk",
	  N_("Messages Not Junk"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_FILTER_MESSAGES_NOT_JUNK },

	{ "mail-filter-messages-with-attachments",
	  "mail-attachment",
	  N_("Messages with Attachments"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_FILTER_MESSAGES_WITH_ATTACHMENTS },

	{ "mail-filter-no-label",
	  NULL,
	  N_("No Label"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_FILTER_NO_LABEL },

	{ "mail-filter-read-messages",
	  "mail-read",
	  N_("Read Messages"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_FILTER_READ_MESSAGES },

	{ "mail-filter-unread-messages",
	  "mail-unread",
	  N_("Unread Messages"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_FILTER_UNREAD_MESSAGES }
};

static GtkRadioActionEntry mail_search_entries[] = {

	{ "mail-search-advanced-hidden",
	  NULL,
	  N_("Advanced Search"),
	  NULL,
	  NULL,
	  MAIL_SEARCH_ADVANCED },

	{ "mail-search-body-contains",
	  NULL,
	  N_("Body contains"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_SEARCH_BODY_CONTAINS },

	{ "mail-search-free-form-expr",
	  NULL,
	  N_("Free form expression"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_SEARCH_FREE_FORM_EXPR },

	{ "mail-search-message-contains",
	  NULL,
	  N_("Message contains"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_SEARCH_MESSAGE_CONTAINS },

	{ "mail-search-recipients-contain",
	  NULL,
	  N_("Recipients contain"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_SEARCH_RECIPIENTS_CONTAIN },

	{ "mail-search-sender-contains",
	  NULL,
	  N_("Sender contains"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_SEARCH_SENDER_CONTAINS },

	{ "mail-search-subject-contains",
	  NULL,
	  N_("Subject contains"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_SEARCH_SUBJECT_CONTAINS },

	{ "mail-search-subject-or-addresses-contain",
	  NULL,
	  N_("Subject or Addresses contain"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_SEARCH_SUBJECT_OR_ADDRESSES_CONTAIN }
};

static GtkRadioActionEntry mail_scope_entries[] = {

	{ "mail-scope-all-accounts",
	  NULL,
	  N_("All Accounts"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_SCOPE_ALL_ACCOUNTS },

	{ "mail-scope-current-account",
	  NULL,
	  N_("Current Account"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_SCOPE_CURRENT_ACCOUNT },

	{ "mail-scope-current-folder",
	  NULL,
	  N_("Current Folder"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_SCOPE_CURRENT_FOLDER }
};

void
e_mail_shell_view_actions_init (EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellBackend *shell_backend;
	EShell *shell;
	EShellSearchbar *searchbar;
	EActionComboBox *combo_box;
	EMailView *mail_view;
	GtkActionGroup *action_group;
	GtkAction *action;
	GSettings *settings;

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);
	searchbar = e_mail_shell_content_get_searchbar (mail_shell_content);

	/* Mail Actions */
	action_group = ACTION_GROUP (MAIL);
	gtk_action_group_add_actions (
		action_group, mail_entries,
		G_N_ELEMENTS (mail_entries), mail_shell_view);
	gtk_action_group_add_toggle_actions (
		action_group, mail_toggle_entries,
		G_N_ELEMENTS (mail_toggle_entries), mail_shell_view);
	gtk_action_group_add_radio_actions (
		action_group, mail_view_entries,
		G_N_ELEMENTS (mail_view_entries), -1,
		G_CALLBACK (action_mail_view_cb), mail_shell_view);
	gtk_action_group_add_radio_actions (
		action_group, mail_search_entries,
		G_N_ELEMENTS (mail_search_entries),
		-1, NULL, NULL);
	gtk_action_group_add_radio_actions (
		action_group, mail_scope_entries,
		G_N_ELEMENTS (mail_scope_entries),
		MAIL_SCOPE_CURRENT_FOLDER, NULL, NULL);
	e_action_group_add_popup_actions (
		action_group, mail_popup_entries,
		G_N_ELEMENTS (mail_popup_entries));

	/* Search Folder Actions */
	action_group = ACTION_GROUP (SEARCH_FOLDERS);
	gtk_action_group_add_actions (
		action_group, search_folder_entries,
		G_N_ELEMENTS (search_folder_entries), mail_shell_view);

	action = ACTION (MAIL_SCOPE_ALL_ACCOUNTS);
	combo_box = e_shell_searchbar_get_scope_combo_box (searchbar);
	e_action_combo_box_set_action (combo_box, GTK_RADIO_ACTION (action));
	e_shell_searchbar_set_scope_visible (searchbar, TRUE);

	/* Advanced Search Action */
	action = ACTION (MAIL_SEARCH_ADVANCED_HIDDEN);
	gtk_action_set_visible (action, FALSE);
	e_shell_searchbar_set_search_option (
		searchbar, GTK_RADIO_ACTION (action));

	g_object_set (ACTION (MAIL_SEND_RECEIVE), "is-important", TRUE, NULL);

	/* Bind GObject properties for GSettings keys. */

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	g_settings_bind (
		settings, "show-deleted",
		ACTION (MAIL_SHOW_DELETED), "active",
		G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (
		settings, "layout",
		ACTION (MAIL_VIEW_VERTICAL), "current-value",
		G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (
		settings, "enable-unmatched",
		ACTION (MAIL_VFOLDER_UNMATCHED_ENABLE), "active",
		G_SETTINGS_BIND_DEFAULT);

	g_object_unref (settings);

	/* Fine tuning. */

	e_binding_bind_property (
		ACTION (MAIL_THREADS_GROUP_BY), "active",
		ACTION (MAIL_FOLDER_SELECT_THREAD), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		ACTION (MAIL_THREADS_GROUP_BY), "active",
		ACTION (MAIL_FOLDER_SELECT_SUBTHREAD), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		ACTION (MAIL_THREADS_GROUP_BY), "active",
		ACTION (MAIL_THREADS_COLLAPSE_ALL), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		ACTION (MAIL_THREADS_GROUP_BY), "active",
		ACTION (MAIL_THREADS_EXPAND_ALL), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		ACTION (MAIL_PREVIEW), "active",
		mail_view, "preview-visible",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		ACTION (MAIL_THREADS_GROUP_BY), "active",
		mail_shell_content, "group-by-threads",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		ACTION (MAIL_PREVIEW), "active",
		ACTION (MAIL_VIEW_CLASSIC), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		ACTION (MAIL_PREVIEW), "active",
		ACTION (MAIL_VIEW_VERTICAL), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		ACTION (MAIL_SHOW_DELETED), "active",
		mail_view, "show-deleted",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		shell_backend, "busy",
		ACTION (MAIL_STOP), "sensitive",
		G_BINDING_SYNC_CREATE);

	/* Keep the sensitivity of "Create Search Folder from Search"
	 * in sync with "Save Search" so that its only selectable when
	 * showing search results. */
	e_binding_bind_property (
		ACTION (SEARCH_SAVE), "sensitive",
		ACTION (MAIL_CREATE_SEARCH_FOLDER), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		shell, "online",
		ACTION (MAIL_DOWNLOAD), "sensitive",
		G_BINDING_SYNC_CREATE);
}

/* Helper for e_mail_shell_view_update_popup_labels() */
static void
mail_shell_view_update_label_action (GtkToggleAction *action,
                                     EMailReader *reader,
                                     GPtrArray *uids,
                                     const gchar *label_tag)
{
	CamelFolder *folder;
	gboolean exists = FALSE;
	gboolean not_exists = FALSE;
	gboolean sensitive;
	guint ii;

	folder = e_mail_reader_ref_folder (reader);

	/* Figure out the proper label action state for the selected
	 * messages.  If all the selected messages have the given label,
	 * make the toggle action active.  If all the selected message
	 * DO NOT have the given label, make the toggle action inactive.
	 * If some do and some don't, make the action insensitive. */

	for (ii = 0; ii < uids->len && (!exists || !not_exists); ii++) {
		const gchar *old_label;
		gchar *new_label;

		/* Check for new-style labels. */
		if (camel_folder_get_message_user_flag (
			folder, uids->pdata[ii], label_tag)) {
			exists = TRUE;
			continue;
		}

		/* Check for old-style labels. */
		old_label = camel_folder_get_message_user_tag (
			folder, uids->pdata[ii], "label");
		if (old_label == NULL) {
			not_exists = TRUE;
			continue;
		}

		/* Convert old-style labels ("<name>") to "$Label<name>". */
		new_label = g_alloca (strlen (old_label) + 10);
		g_stpcpy (g_stpcpy (new_label, "$Label"), old_label);

		if (strcmp (new_label, label_tag) == 0)
			exists = TRUE;
		else
			not_exists = TRUE;
	}

	sensitive = !(exists && not_exists);
	gtk_toggle_action_set_active (action, exists);
	gtk_action_set_sensitive (GTK_ACTION (action), sensitive);

	g_clear_object (&folder);
}

void
e_mail_shell_view_update_popup_labels (EMailShellView *mail_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellBackend *shell_backend;
	EMailShellContent *mail_shell_content;
	EMailLabelListStore *label_store;
	EMailBackend *backend;
	EMailSession *session;
	EMailReader *reader;
	EMailView *mail_view;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	GtkTreeIter iter;
	GPtrArray *uids;
	const gchar *path;
	gboolean valid;
	guint merge_id;
	gint ii = 0;

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	ui_manager = e_shell_window_get_ui_manager (shell_window);

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);
	label_store = e_mail_ui_session_get_label_store (
		E_MAIL_UI_SESSION (session));

	action_group = ACTION_GROUP (MAIL_LABEL);
	merge_id = mail_shell_view->priv->label_merge_id;
	path = "/mail-message-popup/mail-label-menu/mail-label-actions";

	/* Unmerge the previous menu items. */
	gtk_ui_manager_remove_ui (ui_manager, merge_id);
	e_action_group_remove_all_actions (action_group);
	gtk_ui_manager_ensure_update (ui_manager);

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	uids = e_mail_reader_get_selected_uids (reader);

	valid = gtk_tree_model_get_iter_first (
		GTK_TREE_MODEL (label_store), &iter);

	while (valid) {
		EMailLabelAction *label_action;
		GtkAction *action;
		gchar *action_name;
		gchar *stock_id;
		gchar *label;
		gchar *tag;

		label = e_mail_label_list_store_get_name (
			label_store, &iter);
		stock_id = e_mail_label_list_store_get_stock_id (
			label_store, &iter);
		tag = e_mail_label_list_store_get_tag (
			label_store, &iter);
		action_name = g_strdup_printf ("mail-label-%d", ii);

		/* XXX Add a tooltip! */
		label_action = e_mail_label_action_new (
			action_name, label, NULL, stock_id);

		g_object_set_data_full (
			G_OBJECT (label_action), "tag",
			tag, (GDestroyNotify) g_free);

		/* Configure the action before we connect to signals. */
		mail_shell_view_update_label_action (
			GTK_TOGGLE_ACTION (label_action),
			reader, uids, tag);

		g_signal_connect (
			label_action, "toggled",
			G_CALLBACK (action_mail_label_cb), mail_shell_view);

		/* The action group takes ownership of the action. */
		action = GTK_ACTION (label_action);
		gtk_action_group_add_action (action_group, action);
		g_object_unref (label_action);

		gtk_ui_manager_add_ui (
			ui_manager, merge_id, path, action_name,
			action_name, GTK_UI_MANAGER_AUTO, FALSE);

		g_free (label);
		g_free (stock_id);
		g_free (action_name);

		valid = gtk_tree_model_iter_next (
			GTK_TREE_MODEL (label_store), &iter);
		ii++;
	}

	g_ptr_array_unref (uids);
}

void
e_mail_shell_view_update_search_filter (EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellBackend *shell_backend;
	EShellSearchbar *searchbar;
	EMailLabelListStore *label_store;
	EMailBackend *backend;
	EMailSession *session;
	EActionComboBox *combo_box;
	GtkActionGroup *action_group;
	GtkRadioAction *radio_action;
	GtkTreeIter iter;
	GList *list;
	GSList *group;
	gboolean valid;
	gint ii = 0;

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);
	label_store = e_mail_ui_session_get_label_store (
		E_MAIL_UI_SESSION (session));

	action_group = ACTION_GROUP (MAIL_FILTER);
	e_action_group_remove_all_actions (action_group);

	/* Add the standard filter actions.  No callback is needed
	 * because changes in the EActionComboBox are detected and
	 * handled by EShellSearchbar. */
	gtk_action_group_add_radio_actions (
		action_group, mail_filter_entries,
		G_N_ELEMENTS (mail_filter_entries),
		MAIL_FILTER_ALL_MESSAGES, NULL, NULL);

	/* Retrieve the radio group from an action we just added. */
	list = gtk_action_group_list_actions (action_group);
	radio_action = GTK_RADIO_ACTION (list->data);
	group = gtk_radio_action_get_group (radio_action);
	g_list_free (list);

	valid = gtk_tree_model_get_iter_first (
		GTK_TREE_MODEL (label_store), &iter);

	while (valid) {
		GtkAction *action;
		gchar *action_name;
		gchar *stock_id;
		gchar *label;

		label = e_mail_label_list_store_get_name (
			label_store, &iter);
		stock_id = e_mail_label_list_store_get_stock_id (
			label_store, &iter);

		action_name = g_strdup_printf ("mail-filter-label-%d", ii);
		radio_action = gtk_radio_action_new (
			action_name, label, NULL, stock_id, ii);
		g_free (action_name);

		gtk_radio_action_set_group (radio_action, group);
		group = gtk_radio_action_get_group (radio_action);

		/* The action group takes ownership of the action. */
		action = GTK_ACTION (radio_action);
		gtk_action_group_add_action (action_group, action);
		g_object_unref (radio_action);

		g_free (label);
		g_free (stock_id);

		valid = gtk_tree_model_iter_next (
			GTK_TREE_MODEL (label_store), &iter);
		ii++;
	}

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	searchbar = e_mail_shell_content_get_searchbar (mail_shell_content);
	combo_box = e_shell_searchbar_get_filter_combo_box (searchbar);

	e_shell_view_block_execute_search (shell_view);

	/* Use any action in the group; doesn't matter which. */
	e_action_combo_box_set_action (combo_box, radio_action);

	ii = MAIL_FILTER_UNREAD_MESSAGES;
	e_action_combo_box_add_separator_after (combo_box, ii);

	ii = MAIL_FILTER_READ_MESSAGES;
	e_action_combo_box_add_separator_after (combo_box, ii);

	e_shell_view_unblock_execute_search (shell_view);
}

