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
#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <e-util/e-plugin-ui.h>
#include <mail/em-folder-tree.h>
#include <mail/em-utils.h>
#include <mail/mail-ops.h>
#include <mail/mail-mt.h>

#include <shell/e-shell-sidebar.h>
#include <shell/e-shell-view.h>
#include <shell/e-shell-window.h>

#define PRIMARY_TEXT \
	N_("Also mark messages in subfolders?")
#define SECONDARY_TEXT \
	N_("Do you want to mark messages as read in the current folder " \
	   "only, or in the current folder as well as all subfolders?")

enum {
	MARK_ALL_READ_CANCEL,
	MARK_ALL_READ_CURRENT_FOLDER,
	MARK_ALL_READ_WITH_SUBFOLDERS
};

gboolean	e_plugin_ui_init		(GtkUIManager *ui_manager,
						 EShellView *shell_view);
gint e_plugin_lib_enable (EPlugin *ep, gint enable);

gint
e_plugin_lib_enable (EPlugin *ep, gint enable)
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

static void
box_mapped_cb (GtkWidget *box,
               GtkWidget *label)
{
	GtkRequisition requisition;

	/* In order to get decent line wrapping we need to wait until the
	 * box containing the buttons is mapped, and then resize the label
	 * to the same width as the box. */
	gtk_widget_size_request (box, &requisition);
	gtk_widget_set_size_request (label, requisition.width, -1);
}

static gint
prompt_user (gboolean has_subfolders)
{
	GtkWidget *container;
	GtkWidget *dialog;
	GtkWidget *label1;
	GtkWidget *label2;
	GtkWidget *table;
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
			parent, NULL, "mail:ask-mark-all-read", NULL) ?
			MARK_ALL_READ_CURRENT_FOLDER : MARK_ALL_READ_CANCEL;
	}

	dialog = gtk_dialog_new ();
	widget = gtk_dialog_get_action_area (GTK_DIALOG (dialog));
	gtk_widget_hide (widget);
#if !GTK_CHECK_VERSION(2,90,7)
	g_object_set (dialog, "has-separator", FALSE, NULL);
#endif
	gtk_window_set_title (GTK_WINDOW (dialog), "");
	g_signal_connect (
		dialog, "map",
		G_CALLBACK (gtk_widget_queue_resize), NULL);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 12);
	vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	/* Table */
	widget = gtk_table_new (3, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (widget), 12);
	gtk_table_set_col_spacings (GTK_TABLE (widget), 12);
	gtk_box_pack_start (GTK_BOX (vbox), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	table = widget;

	/* Question Icon */
	widget = gtk_image_new_from_stock (
		GTK_STOCK_DIALOG_QUESTION, GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.5, 0.0);
	gtk_table_attach (
		GTK_TABLE (table), widget, 0, 1, 0, 3,
		0, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show (widget);

	/* Primary Text */
	markup = g_markup_printf_escaped (
		"<big><b>%s</b></big>", gettext (PRIMARY_TEXT));
	widget = gtk_label_new (markup);
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.0);
	gtk_table_attach (
		GTK_TABLE (table), widget, 1, 2, 0, 1,
		0, 0, 0, 0);
	gtk_widget_show (widget);
	g_free (markup);
	label1 = widget;

	/* Secondary Text */
	widget = gtk_label_new (gettext (SECONDARY_TEXT));
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.0);
	gtk_table_attach (
		GTK_TABLE (table), widget, 1, 2, 1, 2,
		0, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show (widget);
	label2 = widget;

	/* Action Area */
	widget = gtk_hbox_new (FALSE, 6);
	g_signal_connect (
		widget, "map",
		G_CALLBACK (box_mapped_cb), label1);
	g_signal_connect (
		widget, "map",
		G_CALLBACK (box_mapped_cb), label2);
	gtk_table_attach (
		GTK_TABLE (table), widget, 1, 2, 2, 3,
		GTK_EXPAND | GTK_FILL, 0, 0, 0);
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
	   "Do you want to mark messages as read in the current folder
	   only, or in the current folder as well as all subfolders?" */
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
	   "Do you want to mark messages as read in the current folder
	   only, or in the current folder as well as all subfolders?" */
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
		guint unread = 0;

		gtk_tree_model_get (
			model, iter,
			COL_UINT_UNREAD, &unread,
			COL_BOOL_IS_STORE, &is_store,
			COL_BOOL_IS_DRAFT, &is_draft, -1);

		folder_has_unread =
			!is_store && !is_draft &&
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
				model, &child, path,
				initial_depth, relative_depth))
				return TRUE;

			gtk_tree_path_up (path);
		}

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
			GTK_TREE_MODEL (model), &iter, path,
			gtk_tree_path_get_depth (path), &relative_depth);

		gtk_tree_path_free (path);
	}

	return relative_depth;
}

static void
mark_all_as_read (CamelFolder *folder)
{
	GPtrArray *uids;
	gint i;

	uids =  camel_folder_get_uids (folder);
	camel_folder_freeze(folder);
	for (i=0;i<uids->len;i++)
		camel_folder_set_message_flags (
			folder, uids->pdata[i],
			CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
	camel_folder_thaw(folder);
	camel_folder_free_uids (folder, uids);
}

static gboolean
mar_all_sub_folders (CamelStore *store,
                     CamelFolderInfo *fi,
                     GError **error)
{
	while (fi) {
		CamelFolder *folder;

		if (fi->child) {
			if (!mar_all_sub_folders (store, fi->child, error))
				return FALSE;
		}

		folder = camel_store_get_folder (
			store, fi->full_name, 0, error);
		if (folder == NULL)
			return FALSE;

		if (!CAMEL_IS_VEE_FOLDER (folder))
			mark_all_as_read (folder);

		fi = fi->next;
	}

	return TRUE;
}

static void
mar_got_folder (gchar *folder_uri,
                CamelFolder *folder,
                gpointer data)
{
	CamelFolderInfo *folder_info;
	CamelStore *parent_store;
	const gchar *full_name;
	gint response;

	/* FIXME we have to disable the menu item */
	if (!folder)
		return;

	full_name = camel_folder_get_full_name (folder);
	parent_store = camel_folder_get_parent_store (folder);

	folder_info = camel_store_get_folder_info (
		parent_store, full_name,
		CAMEL_STORE_FOLDER_INFO_RECURSIVE |
		CAMEL_STORE_FOLDER_INFO_FAST, NULL);

	if (folder_info == NULL)
		goto exit;

	response = prompt_user (folder_info->child != NULL);

	if (response == MARK_ALL_READ_CANCEL)
		return;

	if (response == MARK_ALL_READ_CURRENT_FOLDER)
		mark_all_as_read (folder);
	else if (response == MARK_ALL_READ_WITH_SUBFOLDERS)
		mar_all_sub_folders (parent_store, folder_info, NULL);

exit:
	camel_store_free_folder_info (parent_store, folder_info);
}

static void
action_mail_mark_read_recursive_cb (GtkAction *action,
                                    EShellView *shell_view)
{
	EShellSidebar *shell_sidebar;
	EMFolderTree *folder_tree;
	gchar *folder_uri;

	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	g_object_get (shell_sidebar, "folder-tree", &folder_tree, NULL);
	folder_uri = em_folder_tree_get_selected_uri (folder_tree);
	g_return_if_fail (folder_uri != NULL);

	mail_get_folder (
		folder_uri, 0, mar_got_folder,
		NULL, mail_msg_unordered_push);

	g_object_unref (folder_tree);
	g_free (folder_uri);
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

	visible = (scan_folder_tree_for_unread (folder_uri) > 0);
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
