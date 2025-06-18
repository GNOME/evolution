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

#include "evolution-config.h"

#include "mail/e-mail-folder-sort-order-dialog.h"
#include "e-mail-shell-view-private.h"

#include "e-mail-shell-view-actions.h"

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
action_mail_account_disable_cb (EUIAction *action,
				GVariant *parameter,
				gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
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
action_mail_account_properties_cb (EUIAction *action,
				   GVariant *parameter,
				   gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
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
action_mail_account_refresh_cb (EUIAction *action,
				GVariant *parameter,
				gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
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
		CAMEL_STORE_FOLDER_INFO_RECURSIVE | CAMEL_STORE_FOLDER_INFO_REFRESH,
		G_PRIORITY_DEFAULT, cancellable,
		account_refresh_folder_info_received_cb, activity);

	g_clear_object (&source);
	g_clear_object (&store);
}

static void
action_mail_create_search_folder_cb (EUIAction *action,
				     GVariant *parameter,
				     gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
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
action_mail_attachment_bar_cb (EUIAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
	EMailDisplay *mail_display;
	EAttachmentView *attachment_view;

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	e_ui_action_set_state (action, parameter);

	mail_display = e_mail_reader_get_mail_display (E_MAIL_READER (e_mail_shell_content_get_mail_view (mail_shell_view->priv->mail_shell_content)));
	attachment_view = e_mail_display_get_attachment_view (mail_display);
	if (e_ui_action_get_active (action)) {
		EAttachmentBar *bar;
		EAttachmentStore *store;
		guint num_attachments;

		bar = E_ATTACHMENT_BAR (attachment_view);
		store = e_attachment_bar_get_store (bar);
		num_attachments = e_attachment_store_get_num_attachments (store);
		e_attachment_bar_set_attachments_visible (bar, num_attachments > 0 || e_attachment_bar_get_n_possible_attachments (bar) > 0);
	} else {
		e_attachment_bar_set_attachments_visible (E_ATTACHMENT_BAR (attachment_view), FALSE);
	}
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
			camel_service_get_display_name (CAMEL_SERVICE (store)),
			error->message, NULL);
		g_error_free (error);
	}

	g_object_unref (activity);
}

static void
action_mail_download_cb (EUIAction *action,
			 GVariant *parameter,
			 gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
	EMailShellContent *mail_shell_content;
	EMailView *mail_view;
	EMailReader *reader;
	EMailBackend *backend;
	EMailSession *session;
	ESourceRegistry *registry;
	GList *list, *link;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	backend = e_mail_reader_get_backend (reader);
	session = e_mail_backend_get_session (backend);
	registry = e_mail_session_get_registry (session);

	list = camel_session_list_services (CAMEL_SESSION (session));

	for (link = list; link != NULL; link = g_list_next (link)) {
		EActivity *activity;
		ESource *source;
		CamelService *service;
		GCancellable *cancellable;

		service = CAMEL_SERVICE (link->data);

		if (!CAMEL_IS_STORE (service))
			continue;

		source = e_source_registry_ref_source (registry, camel_service_get_uid (service));
		if (!source || !e_source_registry_check_enabled (registry, source)) {
			g_clear_object (&source);
			continue;
		}

		g_clear_object (&source);

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
action_mail_flush_outbox_cb (EUIAction *action,
			     GVariant *parameter,
			     gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
	EShellBackend *shell_backend;
	EShellView *shell_view;
	EMailBackend *backend;
	EMailSession *session;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);

	mail_send_immediately (session);
}

static void
action_mail_folder_copy_cb (EUIAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
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
action_mail_folder_delete_cb (EUIAction *action,
			      GVariant *parameter,
			      gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
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
action_mail_folder_edit_sort_order_cb (EUIAction *action,
				       GVariant *parameter,
				       gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
	EMailView *mail_view;
	EMFolderTree *folder_tree;
	CamelStore *store;
	GtkWidget *dialog;
	GtkWindow *window;
	gchar *selected_uri;

	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_view->priv->mail_shell_sidebar);
	store = em_folder_tree_ref_selected_store (folder_tree);

	g_return_if_fail (store != NULL);

	selected_uri = em_folder_tree_get_selected_uri (folder_tree);

	mail_view = e_mail_shell_content_get_mail_view (mail_shell_view->priv->mail_shell_content);
	window = e_mail_reader_get_window (E_MAIL_READER (mail_view));

	dialog = e_mail_folder_sort_order_dialog_new (window, store, selected_uri);

	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
	g_object_unref (store);
	g_free (selected_uri);
}

static void
action_mail_folder_expunge_cb (EUIAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
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

static void
action_mail_folder_empty_junk_cb (EUIAction *action,
				  GVariant *parameter,
				  gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
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

	/* Get the folder from the folder tree, not the message list.
	 * This correctly handles the use case of right-clicking on
	 * the "Junk" folder and selecting "Empty Junk" without
	 * actually selecting the folder. In that case the message
	 * list would not contain the correct folder. */
	em_folder_tree_get_selected (folder_tree, &selected_store, &selected_folder_name);
	g_return_if_fail (CAMEL_IS_STORE (selected_store));
	g_return_if_fail (selected_folder_name != NULL);

	e_mail_reader_empty_junk_folder_name (E_MAIL_READER (mail_view), selected_store, selected_folder_name);

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
mark_all_read_thread (GTask *task,
                      gpointer source_object,
                      gpointer task_data,
                      GCancellable *cancellable)
{
	AsyncContext *context = task_data;
	CamelStore *store;
	CamelFolder *folder;
	GPtrArray *uids;
	gint ii;
	GError *error = NULL;

	store = CAMEL_STORE (source_object);

	while (!g_queue_is_empty (&context->folder_names) && !error) {
		gchar *folder_name;

		folder_name = g_queue_pop_head (&context->folder_names);
		folder = camel_store_get_folder_sync (
			store, folder_name, 0, cancellable, &error);
		g_free (folder_name);

		if (folder == NULL)
			break;

		camel_folder_freeze (folder);

		uids = camel_folder_dup_uids (folder);

		for (ii = 0; ii < uids->len; ii++)
			camel_folder_set_message_flags (
				folder, uids->pdata[ii],
				CAMEL_MESSAGE_SEEN,
				CAMEL_MESSAGE_SEEN);

		camel_folder_thaw (folder);

		/* Save changes to the server immediately. */
		camel_folder_synchronize_sync (folder, FALSE, cancellable, &error);

		g_ptr_array_unref (uids);
		g_object_unref (folder);
	}

	if (error != NULL)
		g_task_return_error (task, g_steal_pointer (&error));
	else
		g_task_return_boolean (task, TRUE);
}

static void
mark_all_read_done_cb (GObject *source,
                       GAsyncResult *result,
                       gpointer user_data)
{
	AsyncContext *context = user_data;
	GError *local_error = NULL;

	g_return_if_fail (g_task_is_valid (result, source));
	g_return_if_fail (g_async_result_is_tagged (result, mark_all_read_thread));

	if (!g_task_propagate_boolean (G_TASK (result), &local_error) &&
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
	g_clear_pointer (&context, async_context_free);
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
		GSettings *settings;
		GdkDisplay *display;
		GdkKeymap *keymap;

		display = gtk_widget_get_display (GTK_WIDGET (e_shell_view_get_shell_window (E_SHELL_VIEW (mail_shell_view))));
		keymap = gdk_keymap_get_for_display (display);

		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		if ((gdk_keymap_get_modifier_state (keymap) & (GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK)) != GDK_SHIFT_MASK &&
		    !g_settings_get_boolean (settings, "prompt-on-mark-all-read")) {
			g_object_unref (settings);
			return MARK_ALL_READ_CURRENT_ONLY;
		}

		switch (e_alert_run_dialog_for_args (parent,
			"mail:ask-mark-all-read-sub", NULL)) {
			case GTK_RESPONSE_YES:
				g_object_unref (settings);
				return MARK_ALL_READ_WITH_SUBFOLDERS;
			case GTK_RESPONSE_NO:
				g_object_unref (settings);
				return MARK_ALL_READ_CURRENT_ONLY;
			case GTK_RESPONSE_ACCEPT:
				g_settings_set_boolean (settings, "prompt-on-mark-all-read", FALSE);
				g_object_unref (settings);
				return MARK_ALL_READ_CURRENT_ONLY;
			default:
				break;
		}

		g_object_unref (settings);
	} else if (e_util_prompt_user (parent,
			"org.gnome.evolution.mail",
			"prompt-on-mark-all-read",
			"mail:ask-mark-all-read", NULL))
		return MARK_ALL_READ_CURRENT_ONLY;

	return MARK_ALL_READ_CANCEL;
}

static gboolean
mark_all_read_child_has_unread (CamelFolderInfo *folder_info)
{
	gboolean any_has = FALSE;

	if (!folder_info)
		return FALSE;

	while (!any_has && folder_info) {
		any_has = folder_info->unread > 0 || mark_all_read_child_has_unread (folder_info->child);

		folder_info = folder_info->next;
	}

	return any_has;
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
	GTask *task;
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
		context->can_subfolders && mark_all_read_child_has_unread (folder_info->child));

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

	task = g_task_new (source, cancellable, mark_all_read_done_cb, context);
	g_task_set_source_tag (task, mark_all_read_thread);
	g_task_set_task_data (task, context, NULL);

	g_task_run_in_thread (task, mark_all_read_thread);

	g_object_unref (task);
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
		cancellable, _("Marking messages as read…"));

	e_shell_backend_add_activity (shell_backend, context->activity);

	camel_store_get_folder_info (
		store, folder_name,
		can_subfolders ? CAMEL_STORE_FOLDER_INFO_RECURSIVE : 0,
		G_PRIORITY_DEFAULT, cancellable,
		mark_all_read_got_folder_info, context);

	g_object_unref (cancellable);
}

static void
action_mail_folder_mark_all_as_read_cb (EUIAction *action,
					GVariant *parameter,
					gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
	EMailShellContent *mail_shell_content;
	EMailReader *reader;
	EMailView *mail_view;
	CamelFolder *folder;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);

	folder = e_mail_reader_ref_folder (reader);
	g_return_if_fail (folder != NULL);

	if (camel_folder_get_folder_summary (folder) != NULL &&
	    camel_folder_summary_get_unread_count (camel_folder_get_folder_summary (folder)) == 0) {
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
action_mail_popup_folder_mark_all_as_read_cb (EUIAction *action,
					      GVariant *parameter,
					      gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
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
action_mail_folder_move_cb (EUIAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
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
action_mail_folder_new_cb (EUIAction *action,
			   GVariant *parameter,
			   gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
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
action_mail_folder_properties_cb (EUIAction *action,
				  GVariant *parameter,
				  gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
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
action_mail_folder_refresh_cb (EUIAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
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
action_mail_folder_rename_cb (EUIAction *action,
			      GVariant *parameter,
			      gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;

	e_mail_shell_view_rename_folder (mail_shell_view);
}

static void
action_mail_folder_select_thread_cb (EUIAction *action,
				     GVariant *parameter,
				     gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
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
action_mail_folder_select_subthread_cb (EUIAction *action,
					GVariant *parameter,
					gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
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

static gboolean
ask_can_unsubscribe_folder (GtkWindow *parent,
			    CamelFolder *folder)
{
	gchar *full_display_name;
	gboolean res;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);

	full_display_name = e_mail_folder_to_full_display_name (folder, NULL);

	res = GTK_RESPONSE_YES == e_alert_run_dialog_for_args (parent,
		"mail:ask-unsubscribe-folder", full_display_name ? full_display_name : camel_folder_get_full_name (folder), NULL);

	g_free (full_display_name);

	return res;
}

typedef struct _GetFolderData {
	EMailShellView *mail_shell_view;
	EActivity *activity;
	CamelStore *store;
	gchar *folder_name;
} GetFolderData;

static void
get_folder_data_free (GetFolderData *gfd)
{
	if (gfd) {
		g_clear_object (&gfd->mail_shell_view);
		g_clear_object (&gfd->activity);
		g_clear_object (&gfd->store);
		g_free (gfd->folder_name);
		g_slice_free (GetFolderData, gfd);
	}
}

static void
mail_folder_unsubscribe_got_folder_cb (GObject *source_object,
				       GAsyncResult *result,
				       gpointer user_data)
{
	GetFolderData *gfd = user_data;
	CamelFolder *folder;
	GError *local_error = NULL;

	g_return_if_fail (gfd != NULL);

	folder = camel_store_get_folder_finish (CAMEL_STORE (source_object), result, &local_error);

	if (e_activity_handle_cancellation (gfd->activity, local_error)) {
		g_error_free (local_error);

	} else if (local_error != NULL) {
		e_alert_submit (
			e_activity_get_alert_sink (gfd->activity), "mail:folder-open",
			local_error->message, NULL);
		g_error_free (local_error);

	} else {
		EShellWindow *shell_window;
		EMailView *mail_view;

		e_activity_set_state (gfd->activity, E_ACTIVITY_COMPLETED);

		shell_window = e_shell_view_get_shell_window (E_SHELL_VIEW (gfd->mail_shell_view));
		mail_view = e_mail_shell_content_get_mail_view (gfd->mail_shell_view->priv->mail_shell_content);

		if (ask_can_unsubscribe_folder (GTK_WINDOW (shell_window), folder))
			e_mail_reader_unsubscribe_folder_name (E_MAIL_READER (mail_view), gfd->store, gfd->folder_name);
	}

	get_folder_data_free (gfd);
	g_clear_object (&folder);
}

static void
action_mail_folder_unsubscribe_cb (EUIAction *action,
				   GVariant *parameter,
				   gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
	EMailShellContent *mail_shell_content;
	EMailShellSidebar *mail_shell_sidebar;
	EMailView *mail_view;
	EMFolderTree *folder_tree;
	CamelStore *selected_store = NULL;
	gchar *selected_folder_name = NULL;
	GetFolderData *gfd;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	em_folder_tree_get_selected (folder_tree, &selected_store, &selected_folder_name);
	g_return_if_fail (CAMEL_IS_STORE (selected_store));
	g_return_if_fail (selected_folder_name != NULL);

	gfd = g_slice_new0 (GetFolderData);
	gfd->mail_shell_view = g_object_ref (mail_shell_view);
	gfd->activity = e_mail_reader_new_activity (E_MAIL_READER (mail_view));
	gfd->store = selected_store;
	gfd->folder_name = selected_folder_name;

	camel_store_get_folder (gfd->store, gfd->folder_name, 0, G_PRIORITY_DEFAULT,
		e_activity_get_cancellable (gfd->activity),
		mail_folder_unsubscribe_got_folder_cb, gfd);
}

static void
action_mail_global_expunge_cb (EUIAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
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
action_mail_goto_folder_cb (EUIAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
	CamelFolder *folder;
	EMailReader *reader;
	EMailView *mail_view;
	EMFolderSelector *selector;
	EMFolderTree *folder_tree;
	EMFolderTreeModel *model;
	GtkWidget *dialog;
	GtkWindow *window;
	const gchar *uri;

	mail_view = e_mail_shell_content_get_mail_view (mail_shell_view->priv->mail_shell_content);
	reader = E_MAIL_READER (mail_view);

	folder = e_mail_reader_ref_folder (reader);
	window = e_mail_reader_get_window (reader);

	model = em_folder_tree_model_get_default ();

	dialog = em_folder_selector_new (window, model);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Go to Folder"));

	selector = EM_FOLDER_SELECTOR (dialog);
	em_folder_selector_set_can_create (selector, FALSE);
	em_folder_selector_set_default_button_label (selector, _("_Select"));

	folder_tree = em_folder_selector_get_folder_tree (selector);
	gtk_tree_view_expand_all (GTK_TREE_VIEW (folder_tree));
	em_folder_selector_maybe_collapse_archive_folders (selector);

	if (folder) {
		gchar *folder_uri = e_mail_folder_uri_from_folder (folder);

		if (folder_uri) {
			em_folder_tree_set_selected (folder_tree, folder_uri, FALSE);
			g_free (folder_uri);
		}
	}

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		uri = em_folder_selector_get_selected_uri (selector);

		if (uri != NULL) {
			folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_view->priv->mail_shell_sidebar);
			em_folder_tree_set_selected (folder_tree, uri, FALSE);
		}
	}

	gtk_widget_destroy (dialog);

	g_clear_object (&folder);
}

static void
action_mail_send_receive_cb (EUIAction *action,
			     GVariant *parameter,
			     gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
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
action_mail_send_receive_receive_all_cb (EUIAction *action,
					 GVariant *parameter,
					 gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
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
action_mail_send_receive_send_all_cb (EUIAction *action,
				      GVariant *parameter,
				      gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EMailBackend *backend;
	EMailSession *session;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);

	mail_send_immediately (session);
}

static void
mail_shell_view_magic_spacebar (EMailShellView *mail_shell_view,
				gboolean move_forward)
{
	EMailShellContent *mail_shell_content;
	EMailShellSidebar *mail_shell_sidebar;
	EMFolderTree *folder_tree;
	EMailReader *reader;
	EMailView *mail_view;
	GtkWidget *message_list;
	EMailDisplay *display;
	GSettings *settings;
	gboolean magic_spacebar;

	/* This implements the so-called "Magic Backspace". */
	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

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

	if (!e_mail_display_process_magic_spacebar (display, move_forward)) {
		guint32 direction = move_forward ? MESSAGE_LIST_SELECT_NEXT : MESSAGE_LIST_SELECT_PREVIOUS;
		gboolean selected;

		if (!magic_spacebar)
			return;

		if (message_list_select (MESSAGE_LIST (message_list),
		    direction | MESSAGE_LIST_SELECT_WRAP |
		    MESSAGE_LIST_SELECT_INCLUDE_COLLAPSED,
		    0, CAMEL_MESSAGE_SEEN))
			return;

		if (move_forward)
			selected = em_folder_tree_select_next_path (folder_tree, TRUE);
		else
			selected = em_folder_tree_select_prev_path (folder_tree, TRUE);

		if (selected)
			message_list_set_regen_selects_unread (MESSAGE_LIST (message_list), TRUE);

		gtk_widget_grab_focus (message_list);
	}
}

static void
action_mail_smart_backward_cb (EUIAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;

	mail_shell_view_magic_spacebar (mail_shell_view, FALSE);
}

static void
action_mail_smart_forward_cb (EUIAction *action,
			      GVariant *parameter,
			      gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;

	mail_shell_view_magic_spacebar (mail_shell_view, TRUE);
}

static void
action_mail_stop_cb (EUIAction *action,
		     GVariant *parameter,
		     gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
	EShellView *shell_view;
	EShellBackend *shell_backend;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	e_shell_backend_cancel_all (shell_backend);
}

static void
action_mail_threads_collapse_all_cb (EUIAction *action,
				     GVariant *parameter,
				     gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
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
action_mail_threads_expand_all_cb (EUIAction *action,
				   GVariant *parameter,
				   gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
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
action_mail_tools_filters_cb (EUIAction *action,
			      GVariant *parameter,
			      gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
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
action_mail_tools_search_folders_cb (EUIAction *action,
				     GVariant *parameter,
				     gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
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
action_mail_tools_subscriptions_cb (EUIAction *action,
				    GVariant *parameter,
				    gpointer user_data)
{
	EMailShellView *mail_shell_view = user_data;
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

void
e_mail_shell_view_actions_init (EMailShellView *mail_shell_view)
{
	static const EUIActionEntry mail_entries[] = {

		{ "mail-account-disable",
		  NULL,
		  N_("_Disable Account"),
		  NULL,
		  N_("Disable this account"),
		  action_mail_account_disable_cb, NULL, NULL, NULL },

		{ "mail-account-expunge",
		  NULL,
		  N_("_Empty Trash"),
		  NULL,
		  N_("Permanently remove all the deleted messages from all folders"),
		  action_mail_folder_expunge_cb, NULL, NULL, NULL },

		{ "mail-account-empty-junk",
		  NULL,
		  N_("Empty _Junk"),
		  NULL,
		  N_("Delete all Junk messages from all folders"),
		  action_mail_folder_empty_junk_cb, NULL, NULL, NULL },

		{ "mail-account-properties",
		  "document-properties",
		  N_("_Properties"),
		  NULL,
		  N_("Edit properties of this account"),
		  action_mail_account_properties_cb, NULL, NULL, NULL },

		{ "mail-account-refresh",
		  "view-refresh",
		  N_("_Refresh"),
		  NULL,
		  N_("Refresh list of folders of this account"),
		  action_mail_account_refresh_cb, NULL, NULL, NULL },

		{ "mail-download",
		  NULL,
		  N_("_Download Messages for Offline Usage"),
		  NULL,
		  N_("Download messages of accounts and folders marked for offline usage"),
		  action_mail_download_cb, NULL, NULL, NULL },

		{ "mail-flush-outbox",
		  "mail-send",
		  N_("Fl_ush Outbox"),
		  NULL,
		  NULL,
		  action_mail_flush_outbox_cb, NULL, NULL, NULL },

		{ "mail-folder-copy",
		  "folder-copy",
		  N_("_Copy Folder To…"),
		  NULL,
		  N_("Copy the selected folder into another folder"),
		  action_mail_folder_copy_cb, NULL, NULL, NULL },

		{ "mail-folder-delete",
		  "edit-delete",
		  N_("_Delete"),
		  NULL,
		  N_("Permanently remove this folder"),
		  action_mail_folder_delete_cb, NULL, NULL, NULL },

		{ "mail-folder-edit-sort-order",
		  NULL,
		  N_("Edit Sort _Order…"),
		  NULL,
		  N_("Change sort order of the folders in the folder tree"),
		  action_mail_folder_edit_sort_order_cb, NULL, NULL, NULL },

		{ "mail-folder-expunge",
		  NULL,
		  N_("E_xpunge"),
		  "<Control>e",
		  N_("Permanently remove all deleted messages from this folder"),
		  action_mail_folder_expunge_cb, NULL, NULL, NULL },

		{ "mail-folder-mark-all-as-read",
		  "mail-mark-read",
		  N_("Mar_k All Messages as Read"),
		  "<Control>slash",
		  N_("Mark all messages in the folder as read"),
		  action_mail_folder_mark_all_as_read_cb, NULL, NULL, NULL },

		{ "mail-folder-move",
		  "folder-move",
		  N_("_Move Folder To…"),
		  NULL,
		  N_("Move the selected folder into another folder"),
		  action_mail_folder_move_cb, NULL, NULL, NULL },

		{ "mail-folder-new",
		  "folder-new",
		  /* Translators: An action caption to create a new mail folder */
		  N_("_New…"),
		  NULL,
		  N_("Create a new folder for storing mail"),
		  action_mail_folder_new_cb, NULL, NULL, NULL },

		{ "mail-folder-new-full",
		  "folder-new",
		  N_("_New Folder…"),
		  NULL,
		  N_("Create a new folder for storing mail"),
		  action_mail_folder_new_cb, NULL, NULL, NULL },

		{ "mail-folder-properties",
		  "document-properties",
		  N_("_Properties"),
		  NULL,
		  N_("Change the properties of this folder"),
		  action_mail_folder_properties_cb, NULL, NULL, NULL },

		{ "mail-folder-refresh",
		  "view-refresh",
		  N_("_Refresh"),
		  "F5",
		  N_("Refresh the folder"),
		  action_mail_folder_refresh_cb, NULL, NULL, NULL },

		{ "mail-folder-rename",
		  NULL,
		  N_("_Rename…"),
		  NULL,
		  N_("Change the name of this folder"),
		  action_mail_folder_rename_cb, NULL, NULL, NULL },

		{ "mail-folder-select-thread",
		  NULL,
		  N_("Select Message _Thread"),
		  "<Control>h",
		  N_("Select all messages in the same thread as the selected message"),
		  action_mail_folder_select_thread_cb, NULL, NULL, NULL },

		{ "mail-folder-select-subthread",
		  NULL,
		  N_("Select Message S_ubthread"),
		  "<Shift><Control>h",
		  N_("Select all replies to the currently selected message"),
		  action_mail_folder_select_subthread_cb, NULL, NULL, NULL },

		{ "mail-folder-unsubscribe",
		  NULL,
		  N_("_Unsubscribe"),
		  NULL,
		  N_("Unsubscribe from the selected folder"),
		  action_mail_folder_unsubscribe_cb, NULL, NULL, NULL },

		{ "mail-global-expunge",
		  NULL,
		  N_("Empty _Trash"),
		  NULL,
		  N_("Permanently remove all the deleted messages from all accounts"),
		  action_mail_global_expunge_cb, NULL, NULL, NULL },

		{ "mail-goto-folder",
		  NULL,
		  N_("Go to _Folder"),
		  "<Control>g",
		  N_("Opens a dialog to select a folder to go to"),
		  action_mail_goto_folder_cb, NULL, NULL, NULL },

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
		  action_mail_tools_subscriptions_cb, NULL, NULL, NULL },

		{ "mail-popup-folder-mark-all-as-read",
		  "mail-mark-read",
		  N_("Mar_k All Messages as Read"),
		  NULL,
		  N_("Mark all messages in the folder as read"),
		  action_mail_popup_folder_mark_all_as_read_cb, NULL, NULL, NULL },

		{ "mail-send-receive",
		  "mail-send-receive",
		  N_("Send / _Receive"),
		  "F12",
		  N_("Send queued items and retrieve new items"),
		  action_mail_send_receive_cb, NULL, NULL, NULL },

		{ "mail-send-receive-receive-all",
		  NULL,
		  N_("R_eceive All"),
		  NULL,
		  N_("Receive new items from all accounts"),
		  action_mail_send_receive_receive_all_cb, NULL, NULL, NULL },

		{ "mail-send-receive-send-all",
		  "mail-send",
		  N_("_Send All"),
		  NULL,
		  N_("Send queued items in all accounts"),
		  action_mail_send_receive_send_all_cb, NULL, NULL, NULL },

		{ "mail-smart-backward",
		  "go-up", /* In case a user adds it to the UI */
		  "Mail smart backward",
		  "BackSpace",
		  NULL,
		  action_mail_smart_backward_cb, NULL, NULL, NULL },

		{ "mail-smart-forward",
		  "go-down", /* In case a user adds it to the UI */
		  "Mail smart forward",
		  "space",
		  NULL,
		  action_mail_smart_forward_cb, NULL, NULL, NULL },

		{ "mail-stop",
		  "process-stop",
		  N_("Cancel"),
		  NULL,
		  N_("Cancel the current mail operation"),
		  action_mail_stop_cb, NULL, NULL, NULL },

		{ "mail-threads-collapse-all",
		  NULL,
		  N_("Collapse All _Threads"),
		  "<Shift><Control>b",
		  N_("Collapse all message threads"),
		  action_mail_threads_collapse_all_cb, NULL, NULL, NULL },

		{ "mail-threads-expand-all",
		  NULL,
		  N_("E_xpand All Threads"),
		  NULL,
		  N_("Expand all message threads"),
		  action_mail_threads_expand_all_cb, NULL, NULL, NULL },

		{ "mail-tools-filters",
		  NULL,
		  N_("_Message Filters"),
		  NULL,
		  N_("Create or edit rules for filtering new mail"),
		  action_mail_tools_filters_cb, NULL, NULL, NULL },

		{ "mail-tools-subscriptions",
		  NULL,
		  N_("_Subscriptions…"),
		  NULL,
		  N_("Subscribe or unsubscribe to folders on remote servers"),
		  action_mail_tools_subscriptions_cb, NULL, NULL, NULL },

		/*** Menus ***/

		{ "mail-folder-menu", NULL, N_("F_older"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "mail-preview-menu", NULL, N_("_Preview"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "EMailShellView::mail-send-receive", "mail-send-receive", N_("Send / _Receive"), NULL, NULL, NULL, NULL, NULL, NULL }
	};

	static const EUIActionEntry search_folder_entries[] = {

		{ "mail-create-search-folder",
		  NULL,
		  N_("C_reate Search Folder From Search…"),
		  NULL,
		  NULL,
		  action_mail_create_search_folder_cb, NULL, NULL, NULL },

		{ "mail-tools-search-folders",
		  NULL,
		  N_("Search F_olders"),
		  NULL,
		  N_("Create or edit search folder definitions"),
		  action_mail_tools_search_folders_cb, NULL, NULL, NULL },
	};

	static const EUIActionEntry mail_toggle_entries[] = {

		{ "mail-preview",
		  NULL,
		  N_("Show Message _Preview"),
		  "<Control>m",
		  N_("Show message preview pane"),
		  NULL, NULL, "true", NULL /* Handled by property bindings */ },

		{ "mail-attachment-bar",
		  NULL,
		  N_("Show _Attachment Bar"),
		  NULL,
		  N_("Show Attachment Bar below the message preview pane when the message has attachments"),
		  NULL, NULL, "true", action_mail_attachment_bar_cb },

		{ "mail-show-deleted",
		  NULL,
		  N_("Show _Deleted Messages"),
		  NULL,
		  N_("Show deleted messages with a line through them"),
		  NULL, NULL, "false", NULL }, /* Handled by property bindings */

		{ "mail-show-junk",
		  NULL,
		  N_("Show _Junk Messages"),
		  NULL,
		  N_("Show junk messages with a red line through them"),
		  NULL, NULL, "false", NULL }, /* Handled by property bindings */

		{ "mail-show-preview-toolbar",
		  NULL,
		  N_("Show _Preview Tool Bar"),
		  NULL,
		  N_("Show tool bar above the preview panel"),
		  NULL, NULL, "true", NULL }, /* Handled by property bindings */

		{ "mail-threads-group-by",
		  NULL,
		  N_("_Group By Threads"),
		  "<Control>t",
		  N_("Threaded message list"),
		  NULL, NULL, "false", NULL }, /* Handled by property bindings */

		{ "mail-to-do-bar",
		  NULL,
		  N_("Show To _Do Bar"),
		  NULL,
		  N_("Show To Do bar with appointments and tasks"),
		  NULL, NULL, "true", NULL } /* Handled by property bindings */
	};

	static const EUIActionEnumEntry mail_view_entries[] = {

		{ "mail-view-classic",
		  NULL,
		  N_("_Classic View"),
		  NULL,
		  N_("Show message preview below the message list"),
		  NULL, 0 },

		{ "mail-view-vertical",
		  NULL,
		  N_("_Vertical View"),
		  NULL,
		  N_("Show message preview alongside the message list"),
		  NULL, 1 }
	};

	static const EUIActionEnumEntry mail_search_entries[] = {

		{ "mail-search-advanced-hidden",
		  NULL,
		  N_("Advanced Search"),
		  NULL,
		  NULL,
		  NULL, MAIL_SEARCH_ADVANCED },

		{ "mail-search-body-contains",
		  NULL,
		  N_("Body contains"),
		  NULL,
		  NULL,
		  NULL, MAIL_SEARCH_BODY_CONTAINS },

		{ "mail-search-free-form-expr",
		  NULL,
		  N_("Free form expression"),
		  NULL,
		  NULL,
		  NULL, MAIL_SEARCH_FREE_FORM_EXPR },

		{ "mail-search-message-contains",
		  NULL,
		  N_("Message contains"),
		  NULL,
		  NULL,
		  NULL, MAIL_SEARCH_MESSAGE_CONTAINS },

		{ "mail-search-recipients-contain",
		  NULL,
		  N_("Recipients contain"),
		  NULL,
		  NULL,
		  NULL, MAIL_SEARCH_RECIPIENTS_CONTAIN },

		{ "mail-search-sender-contains",
		  NULL,
		  N_("Sender contains"),
		  NULL,
		  NULL,
		  NULL, MAIL_SEARCH_SENDER_CONTAINS },

		{ "mail-search-subject-contains",
		  NULL,
		  N_("Subject contains"),
		  NULL,
		  NULL,
		  NULL, MAIL_SEARCH_SUBJECT_CONTAINS },

		{ "mail-search-subject-or-addresses-contain",
		  NULL,
		  N_("Subject or Addresses contain"),
		  NULL,
		  NULL,
		  NULL, MAIL_SEARCH_SUBJECT_OR_ADDRESSES_CONTAIN }
	};

	static const EUIActionEnumEntry mail_scope_entries[] = {

		{ "mail-scope-all-accounts",
		  NULL,
		  N_("All Accounts"),
		  NULL,
		  NULL,
		  NULL, MAIL_SCOPE_ALL_ACCOUNTS },

		{ "mail-scope-current-account",
		  NULL,
		  N_("Current Account"),
		  NULL,
		  NULL,
		  NULL, MAIL_SCOPE_CURRENT_ACCOUNT },

		{ "mail-scope-current-folder",
		  NULL,
		  N_("Current Folder"),
		  NULL,
		  NULL,
		  NULL, MAIL_SCOPE_CURRENT_FOLDER },

		{ "mail-scope-current-folder-and-subfolders",
		  NULL,
		  N_("Current Folder and Subfolders"),
		  NULL,
		  NULL,
		  NULL, MAIL_SCOPE_CURRENT_FOLDER_AND_SUBFOLDERS }
	};

	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellBackend *shell_backend;
	EShell *shell;
	EUIManager *ui_manager;
	GPtrArray *group;
	guint ii;

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell = e_shell_window_get_shell (shell_window);
	ui_manager = e_shell_view_get_ui_manager (shell_view);

	/* Mail Actions */
	e_ui_manager_add_actions (ui_manager, "mail", NULL,
		mail_entries, G_N_ELEMENTS (mail_entries), mail_shell_view);
	e_ui_manager_add_actions (ui_manager, "mail", NULL,
		mail_toggle_entries, G_N_ELEMENTS (mail_toggle_entries), mail_shell_view);
	e_ui_manager_add_actions_enum (ui_manager, "mail", NULL,
		mail_view_entries, G_N_ELEMENTS (mail_view_entries), mail_shell_view);
	e_ui_manager_add_actions_enum (ui_manager, "mail", NULL,
		mail_search_entries, G_N_ELEMENTS (mail_search_entries), mail_shell_view);
	e_ui_manager_add_actions_enum (ui_manager, "mail", NULL,
		mail_scope_entries, G_N_ELEMENTS (mail_scope_entries), mail_shell_view);

	/* Search Folder Actions */
	e_ui_manager_add_actions (ui_manager, "search-folders", NULL,
		search_folder_entries, G_N_ELEMENTS (search_folder_entries), mail_shell_view);

	/* Add scopes into a radio group */

	group = g_ptr_array_sized_new (G_N_ELEMENTS (mail_scope_entries));

	for (ii = 0; ii < G_N_ELEMENTS (mail_scope_entries); ii++) {
		EUIAction *action = e_ui_manager_get_action (ui_manager, mail_scope_entries[ii].name);

		e_ui_action_set_radio_group (action, group);
	}

	g_clear_pointer (&group, g_ptr_array_unref);

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
		ACTION (MAIL_VIEW_CLASSIC), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		ACTION (MAIL_PREVIEW), "active",
		ACTION (MAIL_VIEW_VERTICAL), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		shell_backend, "busy",
		ACTION (MAIL_STOP), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		shell, "online",
		ACTION (MAIL_DOWNLOAD), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_ui_manager_set_enum_entries_usable_for_kinds (ui_manager, 0,
		mail_search_entries, G_N_ELEMENTS (mail_search_entries));

	e_ui_manager_set_enum_entries_usable_for_kinds (ui_manager, 0,
		mail_scope_entries, G_N_ELEMENTS (mail_scope_entries));
}

void
e_mail_shell_view_update_search_filter (EMailShellView *mail_shell_view)
{
	static const EUIActionEnumEntry mail_filter_entries[] = {

		{ "mail-filter-all-messages",
		  NULL,
		  N_("All Messages"),
		  NULL,
		  NULL,
		  NULL, MAIL_FILTER_ALL_MESSAGES },

		{ "mail-filter-important-messages",
		  "emblem-important",
		  N_("Important Messages"),
		  NULL,
		  NULL,
		  NULL, MAIL_FILTER_IMPORTANT_MESSAGES },

		{ "mail-filter-last-5-days-messages",
		  NULL,
		  N_("Last 5 Days’ Messages"),
		  NULL,
		  NULL,
		  NULL, MAIL_FILTER_LAST_5_DAYS_MESSAGES },

		{ "mail-filter-messages-not-junk",
		  "mail-mark-notjunk",
		  N_("Messages Not Junk"),
		  NULL,
		  NULL,
		  NULL, MAIL_FILTER_MESSAGES_NOT_JUNK },

		{ "mail-filter-messages-with-attachments",
		  "mail-attachment",
		  N_("Messages with Attachments"),
		  NULL,
		  NULL,
		  NULL, MAIL_FILTER_MESSAGES_WITH_ATTACHMENTS },

		{ "mail-filter-messages-with-notes",
		  "evolution-memos",
		  N_("Messages with Notes"),
		  NULL,
		  NULL,
		  NULL, MAIL_FILTER_MESSAGES_WITH_NOTES },

		{ "mail-filter-no-label",
		  NULL,
		  N_("No Label"),
		  NULL,
		  NULL,
		  NULL, MAIL_FILTER_NO_LABEL },

		{ "mail-filter-read-messages",
		  "mail-read",
		  N_("Read Messages"),
		  NULL,
		  NULL,
		  NULL, MAIL_FILTER_READ_MESSAGES },

		{ "mail-filter-unread-messages",
		  "mail-unread",
		  N_("Unread Messages"),
		  NULL,
		  NULL,
		  NULL, MAIL_FILTER_UNREAD_MESSAGES },

		{ "mail-filter-message-thread",
		  NULL,
		  N_("Message Thread"),
		  NULL,
		  NULL,
		  NULL, MAIL_FILTER_MESSAGE_THREAD }
	};

	EMailShellContent *mail_shell_content;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EShellSearchbar *searchbar;
	EMailLabelListStore *label_store;
	EMailBackend *backend;
	EMailSession *session;
	EActionComboBox *combo_box;
	EUIActionGroup *action_group;
	EUIAction *action;
	GtkTreeIter iter;
	GPtrArray *radio_group;
	gboolean valid;
	gint ii;

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);
	label_store = e_mail_ui_session_get_label_store (
		E_MAIL_UI_SESSION (session));

	action_group = e_ui_manager_get_action_group (e_shell_view_get_ui_manager (shell_view), "mail-filter");
	e_ui_action_group_remove_all (action_group);

	/* Add the standard filter actions.  No callback is needed
	 * because changes in the EActionComboBox are detected and
	 * handled by EShellSearchbar. */
	e_ui_manager_add_actions_enum (e_shell_view_get_ui_manager (shell_view),
		e_ui_action_group_get_name (action_group), NULL,
		mail_filter_entries, G_N_ELEMENTS (mail_filter_entries), NULL);

	radio_group = g_ptr_array_new ();

	for (ii = 0; ii < G_N_ELEMENTS (mail_filter_entries); ii++) {
		action = e_ui_action_group_get_action (action_group, mail_filter_entries[ii].name);
		e_ui_action_set_usable_for_kinds (action, 0);
		e_ui_action_set_radio_group (action, radio_group);
	}

	ii = 0; /* start labels from zero, mail_shell_view_execute_search() expects it */
	valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (label_store), &iter);

	while (valid) {
		gchar action_name[128];
		gchar *icon_name;
		gchar *label;

		label = e_mail_label_list_store_get_name (label_store, &iter);
		icon_name = e_mail_label_list_store_dup_icon_name (label_store, &iter);

		g_warn_if_fail (g_snprintf (action_name, sizeof (action_name), "mail-filter-label-%d", ii) < sizeof (action_name));

		action = e_ui_action_new (e_ui_action_group_get_name (action_group), action_name, NULL);
		e_ui_action_set_label (action, label);
		e_ui_action_set_icon_name (action, icon_name);
		e_ui_action_set_state (action, g_variant_new_int32 (ii));
		e_ui_action_set_usable_for_kinds (action, 0);
		e_ui_action_set_radio_group (action, radio_group);

		e_ui_action_group_add (action_group, action);

		g_object_unref (action);
		g_free (label);
		g_free (icon_name);

		valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (label_store), &iter);
		ii++;
	}

	g_clear_pointer (&radio_group, g_ptr_array_unref);

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	searchbar = e_mail_shell_content_get_searchbar (mail_shell_content);
	combo_box = e_shell_searchbar_get_filter_combo_box (searchbar);

	e_shell_view_block_execute_search (shell_view);

	/* Use any action in the group; doesn't matter which. */
	e_action_combo_box_set_action (combo_box, action);

	ii = MAIL_FILTER_MESSAGES_NOT_JUNK;
	e_action_combo_box_add_separator_after (combo_box, ii);

	ii = MAIL_FILTER_READ_MESSAGES;
	e_action_combo_box_add_separator_after (combo_box, ii);

	e_shell_view_unblock_execute_search (shell_view);
}

