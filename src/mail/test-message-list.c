/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <camel/camel.h>
#include <e-util/e-util.h>

#include <libemail-engine/e-mail-session.h>

#include "e-message-list.h"

/* Tree store columns for the folder tree */
enum {
	COL_DISPLAY_NAME,	/* gchararray */
	COL_STORE,		/* gpointer (CamelStore *) */
	COL_FULL_NAME,		/* gchararray - NULL for account rows */
	COL_LOADED,		/* gboolean - TRUE if children are populated */
	N_COLUMNS
};

typedef struct {
	EMailSession *session;
	GtkTreeStore *folder_store;
	GtkTreeView *folder_tree;
	EMessageList *message_list;
	GtkLabel *status_label;
	GtkButton *threading_btn;
	GtkButton *group_btn;

	GCancellable *folder_cancel;
	CamelVeeFolder *vee_folder;
	GHashTable *opened_folders;
	gboolean in_selection_changed;
} AppData;

static void
update_status_label (AppData *app)
{
	EVirtualTree *vtree;
	EVirtualTreeModel *model;
	CamelFolder *folder;
	GPtrArray *selected;
	guint total = 0, sel = 0;
	gchar *text;

	if (!app->status_label)
		return;

	vtree = e_message_list_get_virtual_tree (app->message_list);
	selected = e_virtual_tree_get_selected_rows (vtree);
	sel = selected->len;
	g_ptr_array_unref (selected);

	model = e_virtual_tree_get_model (vtree);
	total = model ? e_virtual_tree_model_get_row_count (model) : 0;

	folder = e_message_list_ref_folder (app->message_list);

	if (folder) {
		text = g_strdup_printf ("Total: %u  Selected: %u  Folder: %s",
			total, sel, G_OBJECT_TYPE_NAME (folder));
		g_object_unref (folder);
	} else {
		text = g_strdup_printf ("Total: %u  Selected: %u", total, sel);
	}

	gtk_label_set_text (app->status_label, text);
	g_free (text);
}

/* Folder tree population */
static void
insert_folder_info (GtkTreeStore *store,
		    GtkTreeIter *parent,
		    CamelStore *camel_store,
		    CamelFolderInfo *fi)
{
	while (fi) {
		GtkTreeIter iter;

		gtk_tree_store_append (store, &iter, parent);
		gtk_tree_store_set (store, &iter,
			COL_DISPLAY_NAME, fi->display_name,
			COL_STORE, camel_store,
			COL_FULL_NAME, fi->full_name,
			COL_LOADED, TRUE,
			-1);

		if (fi->child)
			insert_folder_info (store, &iter, camel_store, fi->child);

		fi = fi->next;
	}
}

static void
populate_folder_tree (AppData *app)
{
	GList *services, *link;

	services = camel_session_list_services (CAMEL_SESSION (app->session));

	for (link = services; link; link = link->next) {
		CamelService *service = link->data;
		CamelProvider *provider;
		const gchar *uid;
		GtkTreeIter iter, dummy;

		if (!CAMEL_IS_STORE (service))
			continue;

		uid = camel_service_get_uid (service);

		/* Skip the built-in vfolder store */
		if (g_strcmp0 (uid, E_MAIL_SESSION_VFOLDER_UID) == 0)
			continue;

		/* Skip remote stores - no trust_prompt implementation */
		provider = camel_service_get_provider (service);
		if (provider && (provider->flags & CAMEL_PROVIDER_IS_REMOTE) != 0)
			continue;

		gtk_tree_store_append (app->folder_store, &iter, NULL);
		gtk_tree_store_set (app->folder_store, &iter,
			COL_DISPLAY_NAME, camel_service_get_display_name (service),
			COL_STORE, CAMEL_STORE (service),
			COL_FULL_NAME, NULL,
			COL_LOADED, FALSE,
			-1);

		/* Dummy child so the expander arrow shows */
		gtk_tree_store_append (app->folder_store, &dummy, &iter);
		gtk_tree_store_set (app->folder_store, &dummy,
			COL_DISPLAY_NAME, "Loading...",
			COL_STORE, NULL,
			COL_FULL_NAME, NULL,
			COL_LOADED, FALSE,
			-1);
	}

	g_list_free_full (services, g_object_unref);
}

static void
on_row_expanded (GtkTreeView *tree_view,
		 GtkTreeIter *iter,
		 GtkTreePath *path,
		 gpointer user_data)
{
	AppData *app = user_data;
	CamelFolderInfo *fi;
	GtkTreeIter child;
	GError *error = NULL;
	gboolean loaded = FALSE;
	CamelStore *store = NULL;

	gtk_tree_model_get (GTK_TREE_MODEL (app->folder_store), iter,
		COL_LOADED, &loaded,
		COL_STORE, &store,
		-1);

	if (loaded || !store)
		return;

	/* Mark as loaded */
	gtk_tree_store_set (app->folder_store, iter,
		COL_LOADED, TRUE, -1);

	/* Fetch folder hierarchy first, then remove the dummy child.
	 * Removing the dummy before adding real children would leave
	 * the row with zero children, causing GtkTreeView to collapse it. */
	fi = camel_store_get_folder_info_sync (store, NULL,
		CAMEL_STORE_FOLDER_INFO_RECURSIVE, NULL, &error);

	if (fi) {
		insert_folder_info (app->folder_store, iter, store, fi);
		camel_folder_info_free (fi);
	} else if (error) {
		g_warning ("Failed to get folder info: %s", error->message);
		g_error_free (error);
	}

	/* Now remove the dummy child - real children are already in place */
	if (gtk_tree_model_iter_children (
		GTK_TREE_MODEL (app->folder_store), &child, iter)) {
		gchar *full_name = NULL;

		gtk_tree_model_get (GTK_TREE_MODEL (app->folder_store),
			&child, COL_FULL_NAME, &full_name, -1);

		if (!full_name)
			gtk_tree_store_remove (app->folder_store, &child);
		else
			g_free (full_name);
	}
}

/* Folder opening (async) */
typedef struct {
	AppData *app;
	GPtrArray *folders;  /* CamelFolder refs */
	guint pending;
} OpenFoldersData;

static void
open_folders_finish (OpenFoldersData *ofd)
{
	AppData *app = ofd->app;
	CamelFolder *folder = NULL;

	if (g_cancellable_is_cancelled (app->folder_cancel)) {
		g_ptr_array_unref (ofd->folders);
		g_free (ofd);
		return;
	}

	if (ofd->folders->len == 0) {
		e_message_list_set_empty_message (app->message_list, "No folders could be opened.");
	} else if (ofd->folders->len == 1) {
		folder = g_object_ref (g_ptr_array_index (ofd->folders, 0));
	} else {
		/* Multi-select: create VeeFolder */
		CamelService *service;
		guint ii;

		service = camel_session_ref_service (
			CAMEL_SESSION (app->session),
			E_MAIL_SESSION_VFOLDER_UID);

		if (service) {
			camel_service_connect_sync (service, NULL, NULL);

			if (app->vee_folder) {
				g_object_unref (app->vee_folder);
				app->vee_folder = NULL;
			}

			app->vee_folder = CAMEL_VEE_FOLDER (camel_vee_folder_new (
				CAMEL_STORE (service), "test-message-list-vfolder", CAMEL_STORE_FOLDER_PRIVATE));

			for (ii = 0; ii < ofd->folders->len; ii++) {
				camel_vee_folder_add_folder_sync (
					app->vee_folder,
					g_ptr_array_index (ofd->folders, ii),
					CAMEL_VEE_FOLDER_OP_FLAG_NONE,
					NULL, NULL);
			}

			camel_vee_folder_set_expression_sync (
				app->vee_folder, "#t",
				CAMEL_VEE_FOLDER_OP_FLAG_NONE,
				NULL, NULL);

			folder = g_object_ref (CAMEL_FOLDER (app->vee_folder));

			g_object_unref (service);
		}
	}

	if (folder) {
		e_message_list_set_folder (app->message_list, folder);
		g_object_unref (folder);
	}

	update_status_label (app);

	g_ptr_array_unref (ofd->folders);
	g_free (ofd);
}

static void
on_folder_opened (GObject *source,
		  GAsyncResult *result,
		  gpointer user_data)
{
	OpenFoldersData *ofd = user_data;
	CamelFolder *folder;
	GError *error = NULL;
	gchar *key;

	folder = camel_store_get_folder_finish (CAMEL_STORE (source), result, &error);

	if (folder) {
		g_ptr_array_add (ofd->folders, folder);

		key = g_strdup_printf ("%p/%s", (gpointer) source, camel_folder_get_full_name (folder));
		g_hash_table_replace (ofd->app->opened_folders, key, g_object_ref (folder));
	} else if (error) {
		g_warning ("Failed to open folder: %s", error->message);
		g_error_free (error);
	}

	ofd->pending--;
	if (ofd->pending == 0)
		open_folders_finish (ofd);
}

/* Folder selection changed */
static void
on_folder_selection_changed (GtkTreeSelection *selection,
			     gpointer user_data)
{
	AppData *app = user_data;
	GList *rows, *link;
	GPtrArray *to_open;     /* (CamelStore *, gchar *full_name) pairs */
	GPtrArray *cached;      /* already-opened CamelFolder refs */
	guint folder_count = 0;
	guint ii;
	OpenFoldersData *ofd;

	if (app->in_selection_changed)
		return;

	/* Cancel any in-progress folder opening */
	if (app->folder_cancel) {
		g_cancellable_cancel (app->folder_cancel);
		g_object_unref (app->folder_cancel);
	}
	app->folder_cancel = g_cancellable_new ();

	/* Clear old vee folder */
	if (app->vee_folder) {
		g_object_unref (app->vee_folder);
		app->vee_folder = NULL;
	}

	/* Collect selected folder rows (skip account rows) */
	rows = gtk_tree_selection_get_selected_rows (selection, NULL);

	to_open = g_ptr_array_new ();
	cached = g_ptr_array_new_with_free_func (g_object_unref);

	for (link = rows; link; link = link->next) {
		GtkTreePath *path = link->data;
		GtkTreeIter iter;
		gchar *full_name = NULL;
		gchar *key;
		CamelStore *store = NULL;
		CamelFolder *cached_folder;

		if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (app->folder_store), &iter, path))
			continue;

		gtk_tree_model_get (GTK_TREE_MODEL (app->folder_store), &iter,
			COL_STORE, &store,
			COL_FULL_NAME, &full_name,
			-1);

		/* Store (account) row - auto-expand it */
		if (!full_name) {
			if (!gtk_tree_view_row_expanded (app->folder_tree, path)) {
				app->in_selection_changed = TRUE;
				gtk_tree_view_expand_row (app->folder_tree, path, FALSE);
				app->in_selection_changed = FALSE;
			}
			continue;
		}

		if (!store) {
			g_free (full_name);
			continue;
		}

		folder_count++;

		key = g_strdup_printf ("%p/%s", (gpointer) store, full_name);
		cached_folder = g_hash_table_lookup (app->opened_folders, key);

		if (cached_folder) {
			g_ptr_array_add (cached, g_object_ref (cached_folder));
			g_free (key);
			g_free (full_name);
		} else {
			g_free (key);
			g_ptr_array_add (to_open, store);
			g_ptr_array_add (to_open, full_name);
		}
	}

	g_list_free_full (rows, (GDestroyNotify) gtk_tree_path_free);

	if (folder_count == 0) {
		/* Only store rows or nothing selected */
		e_message_list_set_folder (app->message_list, NULL);
		update_status_label (app);
		g_ptr_array_unref (to_open);
		g_ptr_array_unref (cached);
		return;
	}

	/* Clear current display */
	e_message_list_set_folder (app->message_list, NULL);
	e_message_list_set_empty_message (app->message_list, folder_count > 1 ? "Opening folders..." : "Opening folder...");

	/* Build the async open request */
	ofd = g_new0 (OpenFoldersData, 1);
	ofd->app = app;
	ofd->folders = g_ptr_array_new_with_free_func (g_object_unref);

	/* Add cached folders immediately */
	for (ii = 0; ii < cached->len; ii++) {
		g_ptr_array_add (ofd->folders,
			g_object_ref (g_ptr_array_index (cached, ii)));
	}
	g_ptr_array_unref (cached);

	/* Count how many need async opening */
	ofd->pending = to_open->len / 2;

	if (ofd->pending == 0) {
		open_folders_finish (ofd);
	} else {
		for (ii = 0; ii < to_open->len; ii += 2) {
			CamelStore *store = g_ptr_array_index (to_open, ii);
			gchar *full_name = g_ptr_array_index (to_open, ii + 1);

			camel_store_get_folder (store, full_name,
				CAMEL_STORE_FOLDER_NONE,
				G_PRIORITY_DEFAULT,
				app->folder_cancel,
				on_folder_opened, ofd);

			g_free (full_name);
		}
	}

	g_ptr_array_free (to_open, TRUE);
}

/* Toolbar callbacks */
static void
on_deleted_toggled (GtkToggleButton *btn,
		    gpointer user_data)
{
	AppData *app = user_data;
	e_message_list_set_show_deleted (app->message_list,
		gtk_toggle_button_get_active (btn));
}

static void
on_junk_toggled (GtkToggleButton *btn,
		 gpointer user_data)
{
	AppData *app = user_data;
	e_message_list_set_show_junk (app->message_list,
		gtk_toggle_button_get_active (btn));
}

static void
on_threading_clicked (GtkButton *btn,
		      gpointer user_data)
{
	AppData *app = user_data;
	CamelFolderViewThreading mode;

	mode = e_message_list_get_threading (app->message_list);

	switch (mode) {
	case CAMEL_FOLDER_VIEW_THREADING_NONE:
		mode = CAMEL_FOLDER_VIEW_THREADING_FLAT;
		gtk_button_set_label (btn, "Threading: Flat");
		break;
	case CAMEL_FOLDER_VIEW_THREADING_FLAT:
		mode = CAMEL_FOLDER_VIEW_THREADING_FULL;
		gtk_button_set_label (btn, "Threading: Full");
		break;
	case CAMEL_FOLDER_VIEW_THREADING_FULL:
		mode = CAMEL_FOLDER_VIEW_THREADING_COMPRESSED;
		gtk_button_set_label (btn, "Threading: Compressed");
		break;
	default:
		mode = CAMEL_FOLDER_VIEW_THREADING_NONE;
		gtk_button_set_label (btn, "Threading: None");
		break;
	}

	e_message_list_set_threading (app->message_list, mode);
}

static void
on_group_clicked (GtkButton *btn,
		  gpointer user_data)
{
	AppData *app = user_data;
	CamelFolderViewGroupBy group;

	group = e_message_list_get_group_by (app->message_list);

	switch (group) {
	case CAMEL_FOLDER_VIEW_GROUP_BY_NONE:
		group = CAMEL_FOLDER_VIEW_GROUP_BY_DATE_SENT;
		gtk_button_set_label (btn, "Group: Date Sent");
		break;
	case CAMEL_FOLDER_VIEW_GROUP_BY_DATE_SENT:
		group = CAMEL_FOLDER_VIEW_GROUP_BY_DATE_RECEIVED;
		gtk_button_set_label (btn, "Group: Date Received");
		break;
	default:
		group = CAMEL_FOLDER_VIEW_GROUP_BY_NONE;
		gtk_button_set_label (btn, "Group: None");
		break;
	}

	e_message_list_set_group_by (app->message_list, group);
}

static const EMessageListColumn default_single_line_columns[] = {
	E_MESSAGE_LIST_COLUMN_STATUS,
	E_MESSAGE_LIST_COLUMN_ATTACHMENT,
	E_MESSAGE_LIST_COLUMN_FLAGGED,
	E_MESSAGE_LIST_COLUMN_FROM,
	E_MESSAGE_LIST_COLUMN_SUBJECT,
	E_MESSAGE_LIST_COLUMN_DATE_SENT,
	E_MESSAGE_LIST_COLUMN_SIZE
};

static void
on_multiline_toggled (GtkToggleButton *btn,
		      gpointer user_data)
{
	AppData *app = user_data;
	gboolean multiline;
	guint ii;

	multiline = gtk_toggle_button_get_active (btn);

	gtk_tree_view_column_set_visible (
		e_message_list_get_column (app->message_list, E_MESSAGE_LIST_COLUMN_COMPOSITE),
		multiline);

	for (ii = 0; ii < G_N_ELEMENTS (default_single_line_columns); ii++) {
		gtk_tree_view_column_set_visible (
			e_message_list_get_column (app->message_list, default_single_line_columns[ii]),
			!multiline);
	}
}

/* Signal handlers for status updates */
static void
on_vtree_selection_changed (EVirtualTree *vtree,
			    gpointer user_data)
{
	update_status_label (user_data);
}

static void
on_vtree_model_changed (EVirtualTree *vtree,
			GParamSpec *pspec,
			gpointer user_data)
{
	AppData *app = user_data;
	EVirtualTreeModel *model;

	model = e_virtual_tree_get_model (vtree);
	if (model) {
		g_signal_handlers_disconnect_by_func (model, update_status_label, app);
		g_signal_connect_swapped (model, "row-count-changed",
			G_CALLBACK (update_status_label), app);
	}
}

/* Main */
static void
connect_status_signals (AppData *app)
{
	EVirtualTree *vtree;

	vtree = e_message_list_get_virtual_tree (app->message_list);

	g_signal_connect (vtree, "selection-changed",
		G_CALLBACK (on_vtree_selection_changed), app);

	g_signal_connect (vtree, "notify::model",
		G_CALLBACK (on_vtree_model_changed), app);
}

static GtkWidget *
create_toolbar (AppData *app)
{
	GtkWidget *box;
	GtkWidget *btn;

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_container_set_border_width (GTK_CONTAINER (box), 2);

	/* Deleted toggle */
	btn = gtk_toggle_button_new_with_label ("Deleted");
	g_signal_connect (btn, "toggled",
		G_CALLBACK (on_deleted_toggled), app);
	gtk_box_pack_start (GTK_BOX (box), btn, FALSE, FALSE, 0);

	/* Junk toggle */
	btn = gtk_toggle_button_new_with_label ("Junk");
	g_signal_connect (btn, "toggled",
		G_CALLBACK (on_junk_toggled), app);
	gtk_box_pack_start (GTK_BOX (box), btn, FALSE, FALSE, 0);

	/* Threading cycle */
	btn = gtk_button_new_with_label ("Threading: None");
	app->threading_btn = GTK_BUTTON (btn);
	g_signal_connect (btn, "clicked",
		G_CALLBACK (on_threading_clicked), app);
	gtk_box_pack_start (GTK_BOX (box), btn, FALSE, FALSE, 0);

	/* Group cycle */
	btn = gtk_button_new_with_label ("Group: None");
	app->group_btn = GTK_BUTTON (btn);
	g_signal_connect (btn, "clicked",
		G_CALLBACK (on_group_clicked), app);
	gtk_box_pack_start (GTK_BOX (box), btn, FALSE, FALSE, 0);

	/* Multi-line (composite) toggle */
	btn = gtk_toggle_button_new_with_label ("Multi-line");
	g_signal_connect (btn, "toggled",
		G_CALLBACK (on_multiline_toggled), app);
	gtk_box_pack_start (GTK_BOX (box), btn, FALSE, FALSE, 0);

	return box;
}

static GtkWidget *
create_folder_tree (AppData *app)
{
	GtkWidget *scroll;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *col;
	GtkTreeSelection *sel;

	app->folder_store = gtk_tree_store_new (N_COLUMNS,
		G_TYPE_STRING,   /* display name */
		G_TYPE_POINTER,  /* CamelStore */
		G_TYPE_STRING,   /* full_name */
		G_TYPE_BOOLEAN); /* loaded */

	app->folder_tree = GTK_TREE_VIEW (gtk_tree_view_new_with_model (GTK_TREE_MODEL (app->folder_store)));
	gtk_tree_view_set_headers_visible (app->folder_tree, FALSE);

	col = gtk_tree_view_column_new ();
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, renderer, TRUE);
	gtk_tree_view_column_add_attribute (col, renderer, "text", COL_DISPLAY_NAME);
	gtk_tree_view_append_column (app->folder_tree, col);

	sel = gtk_tree_view_get_selection (app->folder_tree);
	gtk_tree_selection_set_mode (sel, GTK_SELECTION_MULTIPLE);
	g_signal_connect (sel, "changed",
		G_CALLBACK (on_folder_selection_changed), app);

	g_signal_connect (app->folder_tree, "row-expanded",
		G_CALLBACK (on_row_expanded), app);

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scroll), GTK_WIDGET (app->folder_tree));

	return scroll;
}

static void
on_window_destroy (GtkWidget *widget,
		   gpointer user_data)
{
	AppData *app = user_data;

	app->status_label = NULL;
	app->message_list = NULL;
	app->folder_tree = NULL;

	gtk_main_quit ();
}

int
main (int argc,
      char **argv)
{
	AppData app = { 0 };
	GtkWidget *window, *vbox, *paned, *toolbar, *status_box;
	GtkWidget *folder_scroll, *message_list;
	ESourceRegistry *registry;
	GError *error = NULL;

	gtk_init (&argc, &argv);
	e_util_init_main_thread (NULL);

	/* Set up session */
	registry = e_source_registry_new_sync (NULL, &error);
	if (!registry) {
		g_printerr ("Failed to create source registry: %s\n",
			error ? error->message : "unknown error");
		g_clear_error (&error);
		return 1;
	}

	app.session = e_mail_session_new (registry);
	g_object_unref (registry);

	app.opened_folders = g_hash_table_new_full (
		g_str_hash, g_str_equal, g_free, g_object_unref);

	/* Window */
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (window), "test-message-list");
	gtk_window_set_default_size (GTK_WINDOW (window), 900, 600);
	g_signal_connect (window, "destroy", G_CALLBACK (on_window_destroy), &app);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	/* Toolbar */
	toolbar = create_toolbar (&app);
	gtk_box_pack_start (GTK_BOX (vbox), toolbar, FALSE, FALSE, 0);

	/* Status label */
	status_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_container_set_border_width (GTK_CONTAINER (status_box), 2);
	app.status_label = GTK_LABEL (gtk_label_new ("Total: 0  Selected: 0"));
	gtk_label_set_xalign (app.status_label, 0.0f);
	gtk_box_pack_start (GTK_BOX (status_box),
		GTK_WIDGET (app.status_label), TRUE, TRUE, 4);
	gtk_box_pack_start (GTK_BOX (vbox), status_box, FALSE, FALSE, 0);

	/* Paned: folder tree + message list */
	paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_paned_set_position (GTK_PANED (paned), 250);

	folder_scroll = create_folder_tree (&app);
	gtk_paned_pack1 (GTK_PANED (paned), folder_scroll, FALSE, FALSE);

	message_list = e_message_list_new (app.session);
	app.message_list = E_MESSAGE_LIST (message_list);
	e_message_list_set_empty_message (app.message_list, "Select a folder to view messages.");
	gtk_paned_pack2 (GTK_PANED (paned), message_list, TRUE, TRUE);

	gtk_box_pack_start (GTK_BOX (vbox), paned, TRUE, TRUE, 0);

	/* Connect status update signals */
	connect_status_signals (&app);

	/* Populate folder tree */
	populate_folder_tree (&app);

	gtk_widget_show_all (window);
	gtk_main ();

	/* Cleanup */
	g_clear_object (&app.folder_cancel);
	g_clear_object (&app.vee_folder);
	g_hash_table_destroy (app.opened_folders);
	g_object_unref (app.session);

	return 0;
}
