/*
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
 * Authors:
 *		Chenthill Palanisamy <pchenthill@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <libemail-engine/e-mail-folder-utils.h>

#include <mail/em-folder-tree.h>
#include <mail/em-utils.h>

#include <shell/e-shell-sidebar.h>
#include <shell/e-shell-view.h>
#include <shell/e-shell-window.h>

#define PRIMARY_TEXT \
	N_("Also mark messages in subfolders?")
#define SECONDARY_TEXT \
	N_("Do you want to mark messages as read in the current folder " \
	   "only, or in the current folder as well as all subfolders?")

typedef struct _AsyncContext AsyncContext;

struct _AsyncContext {
	EActivity *activity;
	GQueue folder_names;
};

enum {
	MARK_ALL_READ_CANCEL,
	MARK_ALL_READ_CURRENT_FOLDER,
	MARK_ALL_READ_WITH_SUBFOLDERS
};

gboolean	e_plugin_ui_init		(GtkUIManager *ui_manager,
						 EShellView *shell_view);
gint		e_plugin_lib_enable		(EPlugin *ep,
						 gint enable);

static void
async_context_free (AsyncContext *context)
{
	if (context->activity != NULL)
		g_object_unref (context->activity);

	/* This should be empty already, but just to be sure... */
	while (!g_queue_is_empty (&context->folder_names))
		g_free (g_queue_pop_head (&context->folder_names));

	g_slice_free (AsyncContext, context);
}

gint
e_plugin_lib_enable (EPlugin *ep,
                     gint enable)
{
	return 0;
}

static void
button_clicked_cb (GtkButton *button,
                   GtkDialog *dialog)
{
	gpointer response;

	response = g_object_get_data (G_OBJECT (button), "response");
	gtk_dialog_response (dialog, GPOINTER_TO_INT (response));
}

static gint
prompt_user (gboolean has_subfolders)
{
	GtkWidget *container;
	GtkWidget *dialog;
	GtkWidget *grid;
	GtkWidget *widget;
	GtkWidget *vbox;
	gchar *markup;
	gint response, ret;

	if (!has_subfolders) {
		EShell *shell;
		GtkWindow *parent;

		shell = e_shell_get_default ();
		parent = e_shell_get_active_window (shell);

		return em_utils_prompt_user (
			parent, "prompt-on-mark-all-read", "mail:ask-mark-all-read", NULL) ?
			MARK_ALL_READ_CURRENT_FOLDER : MARK_ALL_READ_CANCEL;
	}

	dialog = gtk_dialog_new ();
	widget = gtk_dialog_get_action_area (GTK_DIALOG (dialog));
	gtk_widget_hide (widget);
	gtk_window_set_title (GTK_WINDOW (dialog), "");
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 12);
	vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	/* Grid */
	widget = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (widget), 12);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 12);
	gtk_box_pack_start (GTK_BOX (vbox), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	grid = widget;

	/* Question Icon */
	widget = gtk_image_new_from_stock (
		GTK_STOCK_DIALOG_QUESTION, GTK_ICON_SIZE_DIALOG);
	gtk_widget_set_valign (widget, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), widget, 0, 0, 1, 3);
	gtk_widget_show (widget);

	/* Primary Text */
	markup = g_markup_printf_escaped (
		"<big><b>%s</b></big>", gettext (PRIMARY_TEXT));
	widget = gtk_label_new (markup);
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.0);
	gtk_grid_attach (GTK_GRID (grid), widget, 1, 0, 1, 1);
	gtk_widget_show (widget);
	g_free (markup);

	/* Secondary Text */
	widget = gtk_label_new (gettext (SECONDARY_TEXT));
	gtk_widget_set_vexpand (widget, TRUE);
	gtk_widget_set_valign (widget, GTK_ALIGN_START);
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.0);
	gtk_grid_attach (GTK_GRID (grid), widget, 1, 1, 1, 1);
	gtk_widget_show (widget);

	/* Action Area */
	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_halign (widget, GTK_ALIGN_END);
	gtk_grid_attach (GTK_GRID (grid), widget, 1, 2, 1, 1);
	gtk_widget_show (widget);

	container = widget;

	/* Cancel button */
	widget = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	g_object_set_data (
		G_OBJECT (widget), "response",
		GINT_TO_POINTER (GTK_RESPONSE_CANCEL));
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (button_clicked_cb), dialog);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	/* To Translators: It's a response button caption on a question
	 * "Do you want to mark messages as read in the current folder
	 * only, or in the current folder as well as all subfolders?" */
	widget = gtk_button_new_with_mnemonic (
		_("In Current Folder and _Subfolders"));
	g_object_set_data (
		G_OBJECT (widget), "response",
		GINT_TO_POINTER (GTK_RESPONSE_YES));
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (button_clicked_cb), dialog);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	/* To Translators: It's a response button caption on a question
	 * "Do you want to mark messages as read in the current folder
	 * only, or in the current folder as well as all subfolders?" */
	widget = gtk_button_new_with_mnemonic (
		_("In Current _Folder Only"));
	g_object_set_data (
		G_OBJECT (widget), "response",
		GINT_TO_POINTER (GTK_RESPONSE_NO));
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (button_clicked_cb), dialog);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);

	if (response == GTK_RESPONSE_YES)
		ret = MARK_ALL_READ_WITH_SUBFOLDERS;
	else if (response == GTK_RESPONSE_NO)
		ret = MARK_ALL_READ_CURRENT_FOLDER;
	else
		ret = MARK_ALL_READ_CANCEL;

	return ret;
}

static gboolean
scan_folder_tree_for_unread_helper (GtkTreeModel *model,
                                    GtkTreeIter *iter,
                                    GtkTreePath *path,
                                    gboolean is_first_node,
                                    gint initial_depth,
                                    gint *relative_depth)
{
	/* This is based on gtk_tree_model_foreach().  Unfortunately
	 * that function insists on traversing the entire tree model. */

	do {
		GtkTreeIter child;
		gboolean folder_has_unread;
		gboolean is_draft = FALSE;
		gboolean is_store = FALSE;
		gboolean is_trash;
		gboolean is_virtual;
		guint unread = 0;
		guint folder_flags = 0;
		guint folder_type;

		gtk_tree_model_get (
			model, iter,
			COL_UINT_FLAGS, &folder_flags,
			COL_UINT_UNREAD, &unread,
			COL_BOOL_IS_STORE, &is_store,
			COL_BOOL_IS_DRAFT, &is_draft, -1);

		folder_type = (folder_flags & CAMEL_FOLDER_TYPE_MASK);
		is_virtual = ((folder_flags & CAMEL_FOLDER_VIRTUAL) != 0);
		is_trash = (folder_type == CAMEL_FOLDER_TYPE_TRASH);

		folder_has_unread =
			!is_store && !is_draft &&
			(!is_virtual || !is_trash) &&
			unread > 0 && unread != ~((guint) 0);

		if (folder_has_unread) {
			gint current_depth;

			current_depth = gtk_tree_path_get_depth (path);
			*relative_depth = current_depth - initial_depth + 1;

			/* If we find unread messages in a child of the
			 * selected folder, short-circuit the recursion. */
			if (*relative_depth > 1)
				return TRUE;
		}

		if (gtk_tree_model_iter_children (model, &child, iter)) {
			gtk_tree_path_down (path);

			if (scan_folder_tree_for_unread_helper (
				model, &child, path, FALSE,
				initial_depth, relative_depth))
				return TRUE;

			gtk_tree_path_up (path);
		}

		/* do not check sibling nodes of the selected folder */
		if (is_first_node)
			break;

		gtk_tree_path_next (path);

	} while (gtk_tree_model_iter_next (model, iter));

	return FALSE;
}

static gint
scan_folder_tree_for_unread (const gchar *folder_uri)
{
	GtkTreeRowReference *reference;
	EMFolderTreeModel *model;
	gint relative_depth = 0;

	/* Traverses the selected folder and its children and returns
	 * the relative depth of the furthest child folder containing
	 * unread mail.  Except, we abort the traversal as soon as we
	 * find a child folder with unread messages.  So the possible
	 * return values are:
	 *
	 *    Depth = 0:  No unread mail found.
	 *    Depth = 1:  Unread mail only in selected folder.
	 *    Depth = 2:  Unread mail in one of the child folders.
	 */

	if (folder_uri == NULL)
		return 0;

	model = em_folder_tree_model_get_default ();
	reference = em_folder_tree_model_lookup_uri (model, folder_uri);

	if (gtk_tree_row_reference_valid (reference)) {
		GtkTreePath *path;
		GtkTreeIter iter;

		path = gtk_tree_row_reference_get_path (reference);
		gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);

		scan_folder_tree_for_unread_helper (
			GTK_TREE_MODEL (model), &iter, path, TRUE,
			gtk_tree_path_get_depth (path), &relative_depth);

		gtk_tree_path_free (path);
	}

	return relative_depth;
}

static void
collect_folder_names (GQueue *folder_names,
                      CamelFolderInfo *folder_info)
{
	while (folder_info != NULL) {
		if (folder_info->child != NULL)
			collect_folder_names (
				folder_names, folder_info->child);

		g_queue_push_tail (
			folder_names, g_strdup (folder_info->full_name));

		folder_info = folder_info->next;
	}
}

static void
mar_got_folder (CamelStore *store,
                GAsyncResult *result,
                AsyncContext *context)
{
	EAlertSink *alert_sink;
	GCancellable *cancellable;
	CamelFolder *folder;
	gchar *folder_name;
	GError *error = NULL;
	GPtrArray *uids;
	gint ii;

	alert_sink = e_activity_get_alert_sink (context->activity);
	cancellable = e_activity_get_cancellable (context->activity);

	folder = camel_store_get_folder_finish (store, result, &error);

	if (e_activity_handle_cancellation (context->activity, error)) {
		g_warn_if_fail (folder == NULL);
		async_context_free (context);
		g_error_free (error);
		return;

	} else if (error != NULL) {
		g_warn_if_fail (folder == NULL);
		e_alert_submit (
			alert_sink, "mail:folder-open",
			error->message, NULL);
		async_context_free (context);
		g_error_free (error);
		return;
	}

	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	camel_folder_freeze (folder);

	uids = camel_folder_get_uids (folder);

	for (ii = 0; ii < uids->len; ii++)
		camel_folder_set_message_flags (
			folder, uids->pdata[ii],
			CAMEL_MESSAGE_SEEN,
			CAMEL_MESSAGE_SEEN);

	camel_folder_free_uids (folder, uids);

	camel_folder_thaw (folder);
	g_object_unref (folder);

	/* If the folder name queue is empty, we're done. */
	if (g_queue_is_empty (&context->folder_names)) {
		e_activity_set_state (context->activity, E_ACTIVITY_COMPLETED);
		async_context_free (context);
		return;
	}

	folder_name = g_queue_pop_head (&context->folder_names);

	camel_store_get_folder (
		store, folder_name, 0,
		G_PRIORITY_DEFAULT, cancellable,
		(GAsyncReadyCallback) mar_got_folder, context);

	g_free (folder_name);
}

static void
mar_got_folder_info (CamelStore *store,
                     GAsyncResult *result,
                     AsyncContext *context)
{
	EAlertSink *alert_sink;
	GCancellable *cancellable;
	CamelFolderInfo *folder_info;
	gchar *folder_name;
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

	/* XXX This EAlert primary text isn't technically correct since
	 *     we're just collecting folder tree info and haven't actually
	 *     opened any folders yet, but the user doesn't need to know. */
	} else if (error != NULL) {
		g_warn_if_fail (folder_info == NULL);
		e_alert_submit (
			alert_sink, "mail:folder-open",
			error->message, NULL);
		async_context_free (context);
		g_error_free (error);
		return;
	}

	g_return_if_fail (folder_info != NULL);

	response = prompt_user (folder_info->child != NULL);

	if (response == MARK_ALL_READ_CURRENT_FOLDER)
		g_queue_push_tail (
			&context->folder_names,
			g_strdup (folder_info->full_name));

	if (response == MARK_ALL_READ_WITH_SUBFOLDERS)
		collect_folder_names (&context->folder_names, folder_info);

	camel_store_free_folder_info (store, folder_info);

	/* If the user cancelled, we're done. */
	if (g_queue_is_empty (&context->folder_names)) {
		e_activity_set_state (context->activity, E_ACTIVITY_COMPLETED);
		async_context_free (context);
		return;
	}

	folder_name = g_queue_pop_head (&context->folder_names);

	camel_store_get_folder (
		store, folder_name, 0,
		G_PRIORITY_DEFAULT, cancellable,
		(GAsyncReadyCallback) mar_got_folder, context);

	g_free (folder_name);
}

static void
action_mail_mark_read_recursive_cb (GtkAction *action,
                                    EShellView *shell_view)
{
	EAlertSink *alert_sink;
	GCancellable *cancellable;
	EShellBackend *shell_backend;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EMFolderTree *folder_tree;
	AsyncContext *context;
	CamelStore *store;
	gchar *folder_name;

	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	g_object_get (shell_sidebar, "folder-tree", &folder_tree, NULL);

	/* This action should only be activatable if a folder is selected. */
	if (!em_folder_tree_get_selected (folder_tree, &store, &folder_name))
		g_return_if_reached ();

	g_object_unref (folder_tree);

	/* Open the selected folder asynchronously. */

	context = g_slice_new0 (AsyncContext);
	context->activity = e_activity_new ();
	g_queue_init (&context->folder_names);

	alert_sink = E_ALERT_SINK (shell_content);
	e_activity_set_alert_sink (context->activity, alert_sink);

	cancellable = camel_operation_new ();
	e_activity_set_cancellable (context->activity, cancellable);

	e_shell_backend_add_activity (shell_backend, context->activity);

	camel_store_get_folder_info (
		store, folder_name,
		CAMEL_STORE_FOLDER_INFO_FAST |
		CAMEL_STORE_FOLDER_INFO_RECURSIVE,
		G_PRIORITY_DEFAULT, cancellable,
		(GAsyncReadyCallback) mar_got_folder_info, context);

	g_object_unref (cancellable);

	g_object_unref (store);
	g_free (folder_name);
}

static GtkActionEntry entries[] = {

	{ "mail-mark-read-recursive",
	  "mail-mark-read",
	  N_("Mark Me_ssages as Read"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_mail_mark_read_recursive_cb) }
};

static void
update_actions_cb (EShellView *shell_view,
                   gpointer user_data)
{
	GtkActionGroup *action_group;
	EShellSidebar *shell_sidebar;
	EShellWindow *shell_window;
	EMFolderTree *folder_tree;
	GtkAction *action;
	gchar *folder_uri;
	gboolean visible;

	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	shell_window = e_shell_view_get_shell_window (shell_view);
	action_group = e_shell_window_get_action_group (shell_window, "mail");

	action = gtk_action_group_get_action (action_group, entries[0].name);
	g_return_if_fail (action != NULL);

	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	g_object_get (shell_sidebar, "folder-tree", &folder_tree, NULL);
	folder_uri = em_folder_tree_get_selected_uri (folder_tree);

	visible = em_folder_tree_get_selected (folder_tree, NULL, NULL)
		  && scan_folder_tree_for_unread (folder_uri) > 0;
	gtk_action_set_visible (action, visible);

	g_object_unref (folder_tree);
	g_free (folder_uri);
}

gboolean
e_plugin_ui_init (GtkUIManager *ui_manager,
                  EShellView *shell_view)
{
	EShellWindow *shell_window;
	GtkActionGroup *action_group;

	shell_window = e_shell_view_get_shell_window (shell_view);
	action_group = e_shell_window_get_action_group (shell_window, "mail");

	/* Add actions to the "mail" action group. */
	gtk_action_group_add_actions (
		action_group, entries,
		G_N_ELEMENTS (entries), shell_view);

	g_signal_connect (
		shell_view, "update-actions",
		G_CALLBACK (update_actions_cb), NULL);

	return TRUE;
}
