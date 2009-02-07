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

#include <widgets/menus/gal-view-factory-etable.h>

static void
mail_shell_view_folder_tree_selected_cb (EMailShellView *mail_shell_view,
                                         const gchar *full_name,
                                         const gchar *uri,
                                         guint32 flags,
                                         EMFolderTree *folder_tree)
{
	EMailReader *reader;
	gboolean folder_selected;

	reader = E_MAIL_READER (mail_shell_view->priv->mail_shell_content);

	folder_selected =
		!(flags & CAMEL_FOLDER_NOSELECT) &&
		full_name != NULL;

	if (folder_selected) {
		EMFolderTreeModel *model;

		model = em_folder_tree_get_model (folder_tree);
		em_folder_tree_model_set_selected (model, uri);
		em_folder_tree_model_save_state (model);

		e_mail_reader_set_folder_uri (reader, uri);
	} else
		e_mail_reader_set_folder (reader, NULL, NULL);

	e_shell_view_update_actions (E_SHELL_VIEW (mail_shell_view));
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
mail_shell_view_reader_changed_cb (EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	e_mail_shell_content_update_view_instance (mail_shell_content);
	e_mail_shell_view_update_sidebar (mail_shell_view);
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
	EShellContent *shell_content;
	EShellSettings *shell_settings;
	EShellSidebar *shell_sidebar;
	EShellWindow *shell_window;
	EMFolderTreeModel *folder_tree_model;
	EMFolderTree *folder_tree;
	GtkTreeModel *tree_model;
	GtkUIManager *ui_manager;
	MessageList *message_list;
	EMailReader *reader;
	guint merge_id;
	gchar *uri;

	shell_view = E_SHELL_VIEW (mail_shell_view);
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
	priv->mail_shell_content = g_object_ref (shell_content);
	priv->mail_shell_sidebar = g_object_ref (shell_sidebar);

	reader = E_MAIL_READER (shell_content);
	message_list = e_mail_reader_get_message_list (reader);

	mail_shell_sidebar = E_MAIL_SHELL_SIDEBAR (shell_sidebar);
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	g_signal_connect_swapped (
		folder_tree, "folder-selected",
		G_CALLBACK (mail_shell_view_folder_tree_selected_cb),
		mail_shell_view);

	g_signal_connect_swapped (
		folder_tree, "popup-event",
		G_CALLBACK (mail_shell_view_folder_tree_popup_event_cb),
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

	e_mail_shell_view_actions_init (mail_shell_view);
	e_mail_shell_view_update_search_filter (mail_shell_view);
	e_mail_reader_init (reader);

	/* Restore the previously selected folder. */
	folder_tree_model = em_folder_tree_get_model (folder_tree);
	uri = em_folder_tree_model_get_selected (folder_tree_model);
	if (uri != NULL) {
		gboolean expanded;

		expanded = em_folder_tree_model_get_expanded_uri (
			folder_tree_model, uri);
		em_folder_tree_set_selected (folder_tree, uri, FALSE);
		e_mail_reader_set_folder_uri (reader, uri);

		if (!expanded)
			em_folder_tree_model_set_expanded_uri (
				folder_tree_model, uri, expanded);

		g_free (uri);
	}
}

void
e_mail_shell_view_private_dispose (EMailShellView *mail_shell_view)
{
	EMailShellViewPrivate *priv = mail_shell_view->priv;

	DISPOSE (priv->mail_shell_content);
	DISPOSE (priv->mail_shell_sidebar);
}

void
e_mail_shell_view_private_finalize (EMailShellView *mail_shell_view)
{
}

void
e_mail_shell_view_execute_search (EMailShellView *mail_shell_view)
{
	/* FIXME */
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
	EShellSidebar *shell_sidebar;
	EShellModule *shell_module;
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

	/* FIXME The sidebar should handle icon name and primary text. */

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_module = e_shell_view_get_shell_module (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	local_store = e_mail_shell_module_get_local_store (shell_module);

	reader = E_MAIL_READER (mail_shell_view->priv->mail_shell_content);
	message_list = e_mail_reader_get_message_list (reader);
	folder_uri = message_list->folder_uri;
	folder = message_list->folder;

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
