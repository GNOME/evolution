/*
 * em-subscription-editor.c
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
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n-lib.h>

#include <e-util/e-util.h>

#include "em-folder-utils.h"
#include "em-subscription-editor.h"

#define FOLDER_CAN_SELECT(folder_info) \
	((folder_info) != NULL && \
	((folder_info)->flags & CAMEL_FOLDER_NOSELECT) == 0)
#define FOLDER_SUBSCRIBED(folder_info) \
	((folder_info) != NULL && \
	((folder_info)->flags & CAMEL_FOLDER_SUBSCRIBED) != 0)

typedef struct _AsyncContext AsyncContext;
typedef struct _TreeRowData TreeRowData;
typedef struct _StoreData StoreData;

struct _EMSubscriptionEditorPrivate {
	EMailSession *session;
	CamelStore *initial_store;

	GtkWidget *combo_box;		/* not referenced */
	GtkWidget *entry;		/* not referenced */
	GtkWidget *notebook;		/* not referenced */
	GtkWidget *subscribe_button;	/* not referenced */
	GtkWidget *subscribe_arrow;	/* not referenced */
	GtkWidget *unsubscribe_button;	/* not referenced */
	GtkWidget *unsubscribe_arrow;	/* not referenced */
	GtkWidget *collapse_all_button;	/* not referenced */
	GtkWidget *expand_all_button;	/* not referenced */
	GtkWidget *refresh_button;	/* not referenced */
	GtkWidget *stop_button;		/* not referenced */

	/* Indicies coincide with the combo box. */
	GPtrArray *stores;

	/* Points at an item in the stores array. */
	StoreData *active;

	/* Casefolded search string. */
	gchar *search_string;

	guint timeout_id;
};

struct _TreeRowData {
	CamelFolderInfo *folder_info;
	GtkTreeRowReference *reference;
};

struct _AsyncContext {
	EMSubscriptionEditor *editor;
	GQueue *tree_rows;
};

struct _StoreData {
	CamelStore *store;
	GtkTreeView *tree_view;
	GtkTreeModel *list_store;
	GtkTreeModel *tree_store;
	GCancellable *cancellable;
	CamelFolderInfo *folder_info;
	gboolean filtered_view;
	gboolean needs_refresh;
};

enum {
	PROP_0,
	PROP_SESSION,
	PROP_STORE
};

enum {
	COL_CASEFOLDED,		/* G_TYPE_STRING  */
	COL_FOLDER_ICON,	/* G_TYPE_STRING  */
	COL_FOLDER_NAME,	/* G_TYPE_STRING  */
	COL_FOLDER_INFO,	/* G_TYPE_POINTER */
	N_COLUMNS
};

G_DEFINE_TYPE_WITH_PRIVATE (EMSubscriptionEditor, em_subscription_editor, GTK_TYPE_DIALOG)

static void
tree_row_data_free (TreeRowData *tree_row_data)
{
	g_return_if_fail (tree_row_data != NULL);

	gtk_tree_row_reference_free (tree_row_data->reference);
	g_slice_free (TreeRowData, tree_row_data);
}

static AsyncContext *
async_context_new (EMSubscriptionEditor *editor,
                   GQueue *tree_rows)
{
	AsyncContext *context;

	context = g_slice_new0 (AsyncContext);
	context->editor = g_object_ref (editor);

	/* Transfer GQueue contents. */
	context->tree_rows = g_queue_copy (tree_rows);
	g_queue_clear (tree_rows);

	return context;
}

static void
async_context_free (AsyncContext *context)
{
	while (!g_queue_is_empty (context->tree_rows))
		tree_row_data_free (g_queue_pop_head (context->tree_rows));

	g_object_unref (context->editor);
	g_queue_free (context->tree_rows);

	g_slice_free (AsyncContext, context);
}

static void
store_data_free (StoreData *data)
{
	if (data->store != NULL)
		g_object_unref (data->store);

	if (data->tree_view != NULL)
		g_object_unref (data->tree_view);

	if (data->list_store != NULL)
		g_object_unref (data->list_store);

	if (data->tree_store != NULL)
		g_object_unref (data->tree_store);

	if (data->cancellable != NULL) {
		g_cancellable_cancel (data->cancellable);
		g_object_unref (data->cancellable);
	}

	camel_folder_info_free (data->folder_info);

	g_slice_free (StoreData, data);
}

static void
subscription_editor_populate (EMSubscriptionEditor *editor,
                              CamelFolderInfo *folder_info,
                              GtkTreeIter *parent,
                              GList **expand_paths)
{
	GtkListStore *list_store;
	GtkTreeStore *tree_store;

	list_store = GTK_LIST_STORE (editor->priv->active->list_store);
	tree_store = GTK_TREE_STORE (editor->priv->active->tree_store);

	while (folder_info != NULL) {
		GtkTreeIter iter;
		const gchar *icon_name;
		gchar *casefolded;

		icon_name =
			em_folder_utils_get_icon_name (folder_info->flags);

		casefolded = g_utf8_casefold (folder_info->full_name, -1);

		gtk_list_store_append (list_store, &iter);

		gtk_list_store_set (
			list_store, &iter,
			COL_CASEFOLDED, casefolded,
			COL_FOLDER_ICON, icon_name,
			COL_FOLDER_NAME, folder_info->full_name,
			COL_FOLDER_INFO, folder_info, -1);

		gtk_tree_store_append (tree_store, &iter, parent);

		gtk_tree_store_set (
			tree_store, &iter,
			COL_CASEFOLDED, NULL,  /* not needed */
			COL_FOLDER_ICON, icon_name,
			COL_FOLDER_NAME, folder_info->display_name,
			COL_FOLDER_INFO, folder_info, -1);

		if (FOLDER_SUBSCRIBED (folder_info)) {
			GtkTreePath *path;

			path = gtk_tree_model_get_path (
				GTK_TREE_MODEL (tree_store), &iter);
			*expand_paths = g_list_prepend (*expand_paths, path);
		}

		g_free (casefolded);

		if (folder_info->child != NULL)
			subscription_editor_populate (
				editor, folder_info->child,
				&iter, expand_paths);

		folder_info = folder_info->next;
	}
}

static void
expand_paths_cb (gpointer path,
                 gpointer tree_view)
{
	gtk_tree_view_expand_to_path (tree_view, path);
}

static void
subscription_editor_get_folder_info_done (CamelStore *store,
                                          GAsyncResult *result,
                                          EMSubscriptionEditor *editor)
{
	GtkTreePath *path;
	GtkTreeView *tree_view;
	GtkTreeModel *list_store;
	GtkTreeModel *tree_store;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	CamelFolderInfo *folder_info;
	GdkWindow *window;
	GList *expand_paths = NULL;
	GError *error = NULL;

	folder_info = camel_store_get_folder_info_finish (
		store, result, &error);

	/* Just return quietly if we were cancelled. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warn_if_fail (folder_info == NULL);
		g_error_free (error);
		goto exit;
	}

	gtk_widget_set_sensitive (editor->priv->notebook, TRUE);
	gtk_widget_set_sensitive (editor->priv->refresh_button, TRUE);
	gtk_widget_set_sensitive (editor->priv->stop_button, FALSE);

	window = gtk_widget_get_window (GTK_WIDGET (editor));
	gdk_window_set_cursor (window, NULL);

	/* XXX Do something smarter with errors. */
	if (error != NULL) {
		g_warn_if_fail (folder_info == NULL);
		e_notice (GTK_WINDOW (editor), GTK_MESSAGE_ERROR, "%s", error->message);
		g_error_free (error);
		goto exit;
	}

	g_return_if_fail (folder_info != NULL);

	camel_folder_info_free (editor->priv->active->folder_info);
	editor->priv->active->folder_info = folder_info;

	tree_view = editor->priv->active->tree_view;
	list_store = editor->priv->active->list_store;
	tree_store = editor->priv->active->tree_store;

	gtk_list_store_clear (GTK_LIST_STORE (list_store));
	gtk_tree_store_clear (GTK_TREE_STORE (tree_store));

	model = gtk_tree_view_get_model (tree_view);
	gtk_tree_view_set_model (tree_view, NULL);
	subscription_editor_populate (editor, folder_info, NULL, &expand_paths);
	gtk_tree_view_set_model (tree_view, model);
	gtk_tree_view_set_search_column (tree_view, COL_FOLDER_NAME);

	g_list_foreach (expand_paths, expand_paths_cb, tree_view);
	g_list_foreach (expand_paths, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (expand_paths);

	path = gtk_tree_path_new_first ();
	selection = gtk_tree_view_get_selection (tree_view);
	gtk_tree_selection_select_path (selection, path);
	gtk_tree_path_free (path);

	gtk_widget_grab_focus (GTK_WIDGET (tree_view));

exit:
	g_object_unref (editor);
}

static void
subscription_editor_subscribe_folder_done (CamelSubscribable *subscribable,
                                           GAsyncResult *result,
                                           AsyncContext *context)
{
	GtkTreeView *tree_view;
	GtkTreeModel *tree_model;
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeIter iter;
	GdkWindow *window;
	GError *error = NULL;
	TreeRowData *tree_row_data;

	camel_subscribable_subscribe_folder_finish (
		subscribable, result, &error);

	/* Just return quietly if we were cancelled. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);
		goto exit;
	}

	tree_row_data = g_queue_pop_head (context->tree_rows);

	/* XXX Do something smarter with errors. */
	if (error == NULL)
		tree_row_data->folder_info->flags |= CAMEL_FOLDER_SUBSCRIBED;
	else {
		e_notice (
			GTK_WINDOW (context->editor),
			GTK_MESSAGE_ERROR, "%s", error->message);
		g_error_free (error);
		tree_row_data_free (tree_row_data);
		goto exit;
	}

	/* Update the toggle renderer in the selected row. */
	tree_model = gtk_tree_row_reference_get_model (tree_row_data->reference);
	path = gtk_tree_row_reference_get_path (tree_row_data->reference);
	gtk_tree_model_get_iter (tree_model, &iter, path);
	gtk_tree_model_row_changed (tree_model, path, &iter);
	gtk_tree_path_free (path);

	tree_row_data_free (tree_row_data);

	if (!g_queue_is_empty (context->tree_rows)) {
		GCancellable *cancellable;

		/* continue with the next to subscribe */
		tree_row_data = g_queue_peek_head (context->tree_rows);
		g_return_if_fail (tree_row_data != NULL);

		cancellable = context->editor->priv->active->cancellable;

		camel_subscribable_subscribe_folder (
			subscribable, tree_row_data->folder_info->full_name,
			G_PRIORITY_DEFAULT, cancellable, (GAsyncReadyCallback)
			subscription_editor_subscribe_folder_done, context);
		return;
	}

exit:
	gtk_widget_set_sensitive (context->editor->priv->notebook, TRUE);
	gtk_widget_set_sensitive (context->editor->priv->refresh_button, TRUE);
	gtk_widget_set_sensitive (context->editor->priv->stop_button, FALSE);

	window = gtk_widget_get_window (GTK_WIDGET (context->editor));
	gdk_window_set_cursor (window, NULL);

	/* Update the Subscription/Unsubscription buttons. */
	tree_view = context->editor->priv->active->tree_view;
	selection = gtk_tree_view_get_selection (tree_view);
	g_signal_emit_by_name (selection, "changed");

	async_context_free (context);

	gtk_widget_grab_focus (GTK_WIDGET (tree_view));
}

static void
subscription_editor_subscribe_many (EMSubscriptionEditor *editor,
                                    GQueue *tree_rows)
{
	TreeRowData *tree_row_data;
	GCancellable *cancellable;
	CamelStore *active_store;
	AsyncContext *context;
	GdkCursor *cursor;

	g_return_if_fail (editor != NULL);

	if (g_queue_is_empty (tree_rows))
		return;

	tree_row_data = g_queue_peek_head (tree_rows);
	g_return_if_fail (tree_row_data != NULL);

	/* Cancel any operation on this store still in progress. */
	gtk_button_clicked (GTK_BUTTON (editor->priv->stop_button));

	/* Start a new 'subscription' operation. */
	editor->priv->active->cancellable = g_cancellable_new ();

	gtk_widget_set_sensitive (editor->priv->notebook, FALSE);
	gtk_widget_set_sensitive (editor->priv->subscribe_button, FALSE);
	gtk_widget_set_sensitive (editor->priv->subscribe_arrow, FALSE);
	gtk_widget_set_sensitive (editor->priv->unsubscribe_button, FALSE);
	gtk_widget_set_sensitive (editor->priv->unsubscribe_arrow, FALSE);
	gtk_widget_set_sensitive (editor->priv->refresh_button, FALSE);
	gtk_widget_set_sensitive (editor->priv->stop_button, TRUE);

	cursor = gdk_cursor_new_from_name (gtk_widget_get_display (GTK_WIDGET (editor)), "wait");
	if (cursor) {
		GdkWindow *window;

		window = gtk_widget_get_window (GTK_WIDGET (editor));
		gdk_window_set_cursor (window, cursor);
		g_object_unref (cursor);
	}

	context = async_context_new (editor, tree_rows);

	active_store = editor->priv->active->store;
	cancellable = editor->priv->active->cancellable;

	camel_subscribable_subscribe_folder (
		CAMEL_SUBSCRIBABLE (active_store),
		tree_row_data->folder_info->full_name,
		G_PRIORITY_DEFAULT, cancellable, (GAsyncReadyCallback)
		subscription_editor_subscribe_folder_done, context);
}

static void
subscription_editor_unsubscribe_folder_done (CamelSubscribable *subscribable,
                                             GAsyncResult *result,
                                             AsyncContext *context)
{
	GtkTreeView *tree_view;
	GtkTreeModel *tree_model;
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeIter iter;
	GdkWindow *window;
	GError *error = NULL;
	TreeRowData *tree_row_data;

	camel_subscribable_unsubscribe_folder_finish (
		subscribable, result, &error);

	/* Just return quietly if we were cancelled. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);
		goto exit;
	}

	tree_row_data = g_queue_pop_head (context->tree_rows);

	/* XXX Do something smarter with errors. */
	if (error == NULL)
		tree_row_data->folder_info->flags &= ~CAMEL_FOLDER_SUBSCRIBED;
	else {
		e_notice (
			GTK_WINDOW (context->editor),
			GTK_MESSAGE_ERROR, "%s", error->message);
		g_error_free (error);
		tree_row_data_free (tree_row_data);
		goto exit;
	}

	/* Update the toggle renderer in the selected row. */
	tree_model = gtk_tree_row_reference_get_model (tree_row_data->reference);
	path = gtk_tree_row_reference_get_path (tree_row_data->reference);
	gtk_tree_model_get_iter (tree_model, &iter, path);
	gtk_tree_model_row_changed (tree_model, path, &iter);
	gtk_tree_path_free (path);

	tree_row_data_free (tree_row_data);

	if (!g_queue_is_empty (context->tree_rows)) {
		GCancellable *cancellable;

		/* continue with the next to unsubscribe */
		tree_row_data = g_queue_peek_head (context->tree_rows);
		g_return_if_fail (tree_row_data != NULL);

		cancellable = context->editor->priv->active->cancellable;

		camel_subscribable_unsubscribe_folder (
			subscribable, tree_row_data->folder_info->full_name,
			G_PRIORITY_DEFAULT, cancellable, (GAsyncReadyCallback)
			subscription_editor_unsubscribe_folder_done, context);
		return;
	}

exit:
	gtk_widget_set_sensitive (context->editor->priv->notebook, TRUE);
	gtk_widget_set_sensitive (context->editor->priv->refresh_button, TRUE);
	gtk_widget_set_sensitive (context->editor->priv->stop_button, FALSE);

	window = gtk_widget_get_window (GTK_WIDGET (context->editor));
	gdk_window_set_cursor (window, NULL);

	/* Update the Subscription/Unsubscription buttons. */
	tree_view = context->editor->priv->active->tree_view;
	selection = gtk_tree_view_get_selection (tree_view);
	g_signal_emit_by_name (selection, "changed");

	async_context_free (context);

	gtk_widget_grab_focus (GTK_WIDGET (tree_view));
}

static void
subscription_editor_unsubscribe_many (EMSubscriptionEditor *editor,
                                      GQueue *tree_rows)
{
	TreeRowData *tree_row_data;
	AsyncContext *context;
	CamelStore *active_store;
	GdkCursor *cursor;

	g_return_if_fail (editor != NULL);

	if (g_queue_is_empty (tree_rows))
		return;

	tree_row_data = g_queue_peek_head (tree_rows);
	g_return_if_fail (tree_row_data != NULL);

	/* Cancel any operation on this store still in progress. */
	gtk_button_clicked (GTK_BUTTON (editor->priv->stop_button));

	/* Start a new 'subscription' operation. */
	editor->priv->active->cancellable = g_cancellable_new ();

	gtk_widget_set_sensitive (editor->priv->notebook, FALSE);
	gtk_widget_set_sensitive (editor->priv->subscribe_button, FALSE);
	gtk_widget_set_sensitive (editor->priv->subscribe_arrow, FALSE);
	gtk_widget_set_sensitive (editor->priv->unsubscribe_button, FALSE);
	gtk_widget_set_sensitive (editor->priv->unsubscribe_arrow, FALSE);
	gtk_widget_set_sensitive (editor->priv->refresh_button, FALSE);
	gtk_widget_set_sensitive (editor->priv->stop_button, TRUE);

	cursor = gdk_cursor_new_from_name (gtk_widget_get_display (GTK_WIDGET (editor)), "wait");
	if (cursor) {
		GdkWindow *window;

		window = gtk_widget_get_window (GTK_WIDGET (editor));
		gdk_window_set_cursor (window, cursor);
		g_object_unref (cursor);
	}

	context = async_context_new (editor, tree_rows);

	active_store = editor->priv->active->store;

	camel_subscribable_unsubscribe_folder (
		CAMEL_SUBSCRIBABLE (active_store),
		tree_row_data->folder_info->full_name, G_PRIORITY_DEFAULT,
		editor->priv->active->cancellable, (GAsyncReadyCallback)
		subscription_editor_unsubscribe_folder_done, context);
}

static GtkWidget *
subscription_editor_create_menu_item (const gchar *label,
                                      gboolean sensitive,
                                      GCallback activate_cb,
                                      EMSubscriptionEditor *editor)
{
	GtkWidget *item;

	item = gtk_menu_item_new_with_mnemonic (label);
	gtk_widget_set_sensitive (item, sensitive);

	gtk_widget_show (item);

	g_signal_connect_swapped (
		item, "activate", activate_cb, editor);

	return item;
}

static TreeRowData *
subscription_editor_tree_row_data_from_iter (GtkTreeView *tree_view,
                                             GtkTreeModel *model,
                                             GtkTreeIter *iter,
                                             gboolean *is_expanded)
{
	TreeRowData *tree_row_data;
	CamelFolderInfo *folder_info = NULL;
	GtkTreeRowReference *reference;
	GtkTreePath *path;

	gtk_tree_model_get (
		model, iter, COL_FOLDER_INFO, &folder_info, -1);

	if (!FOLDER_CAN_SELECT (folder_info))
		return NULL;

	path = gtk_tree_model_get_path (model, iter);
	reference = gtk_tree_row_reference_new (model, path);
	if (is_expanded)
		*is_expanded = gtk_tree_view_row_expanded (tree_view, path);
	gtk_tree_path_free (path);

	tree_row_data = g_slice_new0 (TreeRowData);
	tree_row_data->folder_info = folder_info;
	tree_row_data->reference = reference;

	return tree_row_data;
}

typedef enum {
	PICK_ALL,
	PICK_SUBSCRIBED,
	PICK_UNSUBSCRIBED
} EPickMode;

static gboolean
can_pick_folder_info (CamelFolderInfo *fi,
                      EPickMode mode)
{
	if (!FOLDER_CAN_SELECT (fi))
		return FALSE;

	if (mode == PICK_ALL)
		return TRUE;

	return (FOLDER_SUBSCRIBED (fi) ? 1 : 0) == (mode == PICK_SUBSCRIBED ? 1 : 0);
}

struct PickAllData {
	GtkTreeView *tree_view;
	EPickMode mode;
	GHashTable *skip_folder_infos;
	GQueue *out_tree_rows;
};

static gboolean
pick_all_cb (GtkTreeModel *model,
             GtkTreePath *path,
             GtkTreeIter *iter,
             gpointer user_data)
{
	struct PickAllData *data = user_data;
	TreeRowData *tree_row_data;

	tree_row_data = subscription_editor_tree_row_data_from_iter (
		data->tree_view, model, iter, NULL);
	if (tree_row_data == NULL)
		return FALSE;

	if (can_pick_folder_info (tree_row_data->folder_info, data->mode) &&
	    (data->skip_folder_infos == NULL ||
	    !g_hash_table_contains (
			data->skip_folder_infos,
			tree_row_data->folder_info))) {
		g_queue_push_tail (data->out_tree_rows, tree_row_data);
	} else
		tree_row_data_free (tree_row_data);

	return FALSE;
}

/* skip_folder_infos contains CamelFolderInfo-s to skip;
 * these should come from the tree view; can be NULL
 * to include everything.
*/
static void
subscription_editor_pick_all (EMSubscriptionEditor *editor,
                              EPickMode mode,
                              GHashTable *skip_folder_infos,
                              GQueue *out_tree_rows)
{
	GtkTreeView *tree_view;
	GtkTreeModel *tree_model;
	struct PickAllData data;

	tree_view = editor->priv->active->tree_view;
	tree_model = gtk_tree_view_get_model (tree_view);

	data.tree_view = tree_view;
	data.mode = mode;
	data.skip_folder_infos = skip_folder_infos;
	data.out_tree_rows = out_tree_rows;

	gtk_tree_model_foreach (tree_model, pick_all_cb, &data);
}

static void
subscription_editor_pick_shown (EMSubscriptionEditor *editor,
                                EPickMode mode,
                                GQueue *out_tree_rows)
{
	GtkTreeView *tree_view;
	GtkTreeModel *tree_model;
	GtkTreeIter iter, iter2;
	gboolean found = TRUE;

	tree_view = editor->priv->active->tree_view;
	tree_model = gtk_tree_view_get_model (tree_view);

	if (!gtk_tree_model_get_iter_first (tree_model, &iter))
		return;

	while (found) {
		TreeRowData *tree_row_data;
		gboolean is_expanded = FALSE;

		tree_row_data = subscription_editor_tree_row_data_from_iter (
			tree_view, tree_model, &iter, &is_expanded);

		if (tree_row_data != NULL) {
			if (can_pick_folder_info (tree_row_data->folder_info, mode))
				g_queue_push_tail (out_tree_rows, tree_row_data);
			else
				tree_row_data_free (tree_row_data);
		}

		if (is_expanded && gtk_tree_model_iter_children (
		    tree_model, &iter2, &iter)) {
			iter = iter2;
			found = TRUE;
		} else {
			iter2 = iter;
			if (gtk_tree_model_iter_next (tree_model, &iter2)) {
				iter = iter2;
				found = TRUE;
			} else {
				while (found = gtk_tree_model_iter_parent (
				       tree_model, &iter2, &iter), found) {
					iter = iter2;
					if (gtk_tree_model_iter_next (
					    tree_model, &iter2)) {
						iter = iter2;
						break;
					}
				}
			}
		}
	}
}

static void
subscription_editor_subscribe (EMSubscriptionEditor *editor)
{
	GtkTreeSelection *selection;
	GtkTreeModel *tree_model;
	GtkTreeView *tree_view;
	GtkTreeIter iter;
	gboolean have_selection;
	GQueue tree_rows = G_QUEUE_INIT;
	TreeRowData *tree_row_data;

	tree_view = editor->priv->active->tree_view;
	selection = gtk_tree_view_get_selection (tree_view);

	have_selection = gtk_tree_selection_get_selected (
		selection, &tree_model, &iter);
	g_return_if_fail (have_selection);

	tree_row_data = subscription_editor_tree_row_data_from_iter (
		tree_view, tree_model, &iter, NULL);

	g_queue_push_tail (&tree_rows, tree_row_data);
	subscription_editor_subscribe_many (editor, &tree_rows);
	g_warn_if_fail (g_queue_is_empty (&tree_rows));
}

static void
subscription_editor_subscribe_shown (EMSubscriptionEditor *editor)
{
	GQueue tree_rows = G_QUEUE_INIT;

	subscription_editor_pick_shown (
		editor, PICK_UNSUBSCRIBED, &tree_rows);
	subscription_editor_subscribe_many (editor, &tree_rows);
}

static void
subscription_editor_subscribe_all (EMSubscriptionEditor *editor)
{
	GQueue tree_rows = G_QUEUE_INIT;

	subscription_editor_pick_all (
		editor, PICK_UNSUBSCRIBED, NULL, &tree_rows);
	subscription_editor_subscribe_many (editor, &tree_rows);
}

static void
subscription_editor_subscribe_popup_cb (EMSubscriptionEditor *editor)
{
	GtkWidget *menu;
	GtkTreeIter iter;
	gboolean tree_filled;

	tree_filled = editor->priv->active &&
		gtk_tree_model_get_iter_first (
			editor->priv->active->filtered_view
			? editor->priv->active->list_store
			: editor->priv->active->tree_store,
			&iter);

	menu = gtk_menu_new ();

	gtk_menu_shell_append (
		GTK_MENU_SHELL (menu),
		subscription_editor_create_menu_item (
			_("_Subscribe"),
			gtk_widget_get_sensitive (
				editor->priv->subscribe_button),
			G_CALLBACK (subscription_editor_subscribe),
			editor));

	gtk_menu_shell_append (
		GTK_MENU_SHELL (menu),
		subscription_editor_create_menu_item (
			_("Su_bscribe To Shown"),
			tree_filled,
			G_CALLBACK (subscription_editor_subscribe_shown),
			editor));

	gtk_menu_shell_append (
		GTK_MENU_SHELL (menu),
		subscription_editor_create_menu_item (
			_("Subscribe To _All"),
			tree_filled,
			G_CALLBACK (subscription_editor_subscribe_all),
			editor));

	gtk_menu_attach_to_widget (GTK_MENU (menu), GTK_WIDGET (editor), NULL);
	g_signal_connect (menu, "deactivate", G_CALLBACK (gtk_menu_detach), NULL);

	g_object_set (menu,
	              "anchor-hints", (GDK_ANCHOR_FLIP_Y |
	                               GDK_ANCHOR_SLIDE |
	                               GDK_ANCHOR_RESIZE),
	              NULL);

	gtk_menu_popup_at_widget (GTK_MENU (menu),
	                          editor->priv->subscribe_button,
	                          GDK_GRAVITY_SOUTH_WEST,
	                          GDK_GRAVITY_NORTH_WEST,
	                          NULL);
}

static void
subscription_editor_unsubscribe_hidden (EMSubscriptionEditor *editor)
{
	GQueue tree_rows = G_QUEUE_INIT;
	GHashTable *skip_shown;

	subscription_editor_pick_shown (editor, PICK_ALL, &tree_rows);
	g_return_if_fail (!g_queue_is_empty (&tree_rows));

	skip_shown = g_hash_table_new (g_direct_hash, g_direct_equal);

	while (!g_queue_is_empty (&tree_rows)) {
		TreeRowData *tree_row_data;

		tree_row_data = g_queue_pop_head (&tree_rows);

		if (tree_row_data == NULL)
			continue;

		g_hash_table_add (skip_shown, tree_row_data->folder_info);

		tree_row_data_free (tree_row_data);
	}

	subscription_editor_pick_all (
		editor, PICK_SUBSCRIBED, skip_shown, &tree_rows);
	subscription_editor_unsubscribe_many (editor, &tree_rows);

	g_hash_table_destroy (skip_shown);
}

static void
subscription_editor_unsubscribe_all (EMSubscriptionEditor *editor)
{
	GQueue tree_rows = G_QUEUE_INIT;

	subscription_editor_pick_all (
		editor, PICK_SUBSCRIBED, NULL, &tree_rows);
	subscription_editor_unsubscribe_many (editor, &tree_rows);
}

static void
subscription_editor_unsubscribe (EMSubscriptionEditor *editor)
{
	GtkTreeSelection *selection;
	GtkTreeModel *tree_model;
	GtkTreeView *tree_view;
	GtkTreeIter iter;
	gboolean have_selection;
	GQueue tree_rows = G_QUEUE_INIT;
	TreeRowData *tree_row_data;

	tree_view = editor->priv->active->tree_view;
	selection = gtk_tree_view_get_selection (tree_view);

	have_selection = gtk_tree_selection_get_selected (
		selection, &tree_model, &iter);
	g_return_if_fail (have_selection);

	tree_row_data = subscription_editor_tree_row_data_from_iter (
		tree_view, tree_model, &iter, NULL);

	g_queue_push_tail (&tree_rows, tree_row_data);
	subscription_editor_unsubscribe_many (editor, &tree_rows);
}

static void
subscription_editor_unsubscribe_popup_cb (EMSubscriptionEditor *editor)
{
	GtkWidget *menu;
	GtkTreeIter iter;
	gboolean tree_filled;

	tree_filled = editor->priv->active &&
		gtk_tree_model_get_iter_first (
			editor->priv->active->filtered_view
			? editor->priv->active->list_store
			: editor->priv->active->tree_store,
			&iter);

	menu = gtk_menu_new ();

	gtk_menu_shell_append (
		GTK_MENU_SHELL (menu),
		subscription_editor_create_menu_item (
			_("_Unsubscribe"),
			gtk_widget_get_sensitive (
				editor->priv->unsubscribe_button),
			G_CALLBACK (subscription_editor_unsubscribe),
			editor));

	gtk_menu_shell_append (
		GTK_MENU_SHELL (menu),
		subscription_editor_create_menu_item (
			_("Unsu_bscribe From Hidden"),
			tree_filled,
			G_CALLBACK (subscription_editor_unsubscribe_hidden),
			editor));

	gtk_menu_shell_append (
		GTK_MENU_SHELL (menu),
		subscription_editor_create_menu_item (
			_("Unsubscribe From _All"),
			tree_filled,
			G_CALLBACK (subscription_editor_unsubscribe_all),
			editor));

	gtk_menu_attach_to_widget (GTK_MENU (menu), GTK_WIDGET (editor), NULL);
	g_signal_connect (menu, "deactivate", G_CALLBACK (gtk_menu_detach), NULL);

	g_object_set (menu,
	              "anchor-hints", (GDK_ANCHOR_FLIP_Y |
	                               GDK_ANCHOR_SLIDE |
	                               GDK_ANCHOR_RESIZE),
	              NULL);

	gtk_menu_popup_at_widget (GTK_MENU (menu),
	                          editor->priv->unsubscribe_button,
	                          GDK_GRAVITY_SOUTH_WEST,
	                          GDK_GRAVITY_NORTH_WEST,
	                          NULL);
}

static void
subscription_editor_collapse_all (EMSubscriptionEditor *editor)
{
	gtk_tree_view_collapse_all (editor->priv->active->tree_view);
}

static void
subscription_editor_expand_all (EMSubscriptionEditor *editor)
{
	gtk_tree_view_expand_all (editor->priv->active->tree_view);
}

static void
subscription_editor_refresh (EMSubscriptionEditor *editor)
{
	GdkCursor *cursor;

	/* Cancel any operation on this store still in progress. */
	gtk_button_clicked (GTK_BUTTON (editor->priv->stop_button));

	/* Start a new 'refresh' operation. */
	editor->priv->active->cancellable = g_cancellable_new ();

	gtk_widget_set_sensitive (editor->priv->notebook, FALSE);
	gtk_widget_set_sensitive (editor->priv->subscribe_button, FALSE);
	gtk_widget_set_sensitive (editor->priv->subscribe_arrow, FALSE);
	gtk_widget_set_sensitive (editor->priv->unsubscribe_button, FALSE);
	gtk_widget_set_sensitive (editor->priv->unsubscribe_arrow, FALSE);
	gtk_widget_set_sensitive (editor->priv->refresh_button, FALSE);
	gtk_widget_set_sensitive (editor->priv->stop_button, TRUE);

	cursor = gdk_cursor_new_from_name (gtk_widget_get_display (GTK_WIDGET (editor)), "wait");
	if (cursor) {
		GdkWindow *window;

		window = gtk_widget_get_window (GTK_WIDGET (editor));
		gdk_window_set_cursor (window, cursor);
		g_object_unref (cursor);
	}

	camel_store_get_folder_info (
		editor->priv->active->store, NULL,
		CAMEL_STORE_FOLDER_INFO_RECURSIVE |
		CAMEL_STORE_FOLDER_INFO_NO_VIRTUAL |
		CAMEL_STORE_FOLDER_INFO_SUBSCRIPTION_LIST,
		G_PRIORITY_DEFAULT, editor->priv->active->cancellable,
		(GAsyncReadyCallback) subscription_editor_get_folder_info_done,
		g_object_ref (editor));
}

static void
subscription_editor_stop (EMSubscriptionEditor *editor)
{
	GdkWindow *window;

	if (editor->priv->active->cancellable != NULL) {
		g_cancellable_cancel (editor->priv->active->cancellable);
		g_object_unref (editor->priv->active->cancellable);
		editor->priv->active->cancellable = NULL;
	}

	gtk_widget_set_sensitive (editor->priv->notebook, TRUE);
	gtk_widget_set_sensitive (editor->priv->subscribe_button, TRUE);
	gtk_widget_set_sensitive (editor->priv->subscribe_arrow, TRUE);
	gtk_widget_set_sensitive (editor->priv->unsubscribe_button, TRUE);
	gtk_widget_set_sensitive (editor->priv->unsubscribe_arrow, TRUE);
	gtk_widget_set_sensitive (editor->priv->refresh_button, TRUE);
	gtk_widget_set_sensitive (editor->priv->stop_button, FALSE);
	gtk_widget_grab_focus (GTK_WIDGET (editor->priv->active->tree_view));

	window = gtk_widget_get_window (GTK_WIDGET (editor));
	gdk_window_set_cursor (window, NULL);
}

static gboolean
subscription_editor_filter_cb (GtkTreeModel *tree_model,
                               GtkTreeIter *iter,
                               EMSubscriptionEditor *editor)
{
	CamelFolderInfo *folder_info;
	gchar *casefolded;
	gboolean match;

	/* If there's no search string let everything through. */
	if (editor->priv->search_string == NULL)
		return TRUE;

	gtk_tree_model_get (
		tree_model, iter,
		COL_CASEFOLDED, &casefolded,
		COL_FOLDER_INFO, &folder_info, -1);

	match = FOLDER_CAN_SELECT (folder_info) &&
		(casefolded != NULL) && (*casefolded != '\0') &&
		(strstr (casefolded, editor->priv->search_string) != NULL);

	g_free (casefolded);

	return match;
}

static void
subscription_editor_update_view (EMSubscriptionEditor *editor)
{
	GtkEntry *entry;
	GtkTreeView *tree_view;
	GtkTreeModel *tree_model;
	const gchar *text;

	entry = GTK_ENTRY (editor->priv->entry);
	tree_view = editor->priv->active->tree_view;

	editor->priv->timeout_id = 0;

	text = gtk_entry_get_text (entry);

	if (text != NULL && *text != '\0') {
		g_free (editor->priv->search_string);
		editor->priv->search_string = g_utf8_casefold (text, -1);

		/* Install the list store in the tree view if needed. */
		if (!editor->priv->active->filtered_view) {
			GtkTreeSelection *selection;
			GtkTreePath *path;

			tree_model = gtk_tree_model_filter_new (
				editor->priv->active->list_store, NULL);
			gtk_tree_model_filter_set_visible_func (
				GTK_TREE_MODEL_FILTER (tree_model),
				(GtkTreeModelFilterVisibleFunc)
				subscription_editor_filter_cb, editor,
				(GDestroyNotify) NULL);
			gtk_tree_view_set_model (tree_view, tree_model);
			gtk_tree_view_set_search_column (tree_view, COL_FOLDER_NAME);
			g_object_unref (tree_model);

			path = gtk_tree_path_new_first ();
			selection = gtk_tree_view_get_selection (tree_view);
			gtk_tree_selection_select_path (selection, path);
			gtk_tree_path_free (path);

			editor->priv->active->filtered_view = TRUE;
		}

		tree_model = gtk_tree_view_get_model (tree_view);
		gtk_tree_model_filter_refilter (
			GTK_TREE_MODEL_FILTER (tree_model));

		gtk_entry_set_icon_sensitive (
			entry, GTK_ENTRY_ICON_SECONDARY, TRUE);

		gtk_widget_set_sensitive (
			editor->priv->collapse_all_button, FALSE);
		gtk_widget_set_sensitive (
			editor->priv->expand_all_button, FALSE);

	} else {
		/* Install the tree store in the tree view if needed. */
		if (editor->priv->active->filtered_view) {
			GtkTreeSelection *selection;
			GtkTreePath *path;

			tree_model = editor->priv->active->tree_store;
			gtk_tree_view_set_model (tree_view, tree_model);
			gtk_tree_view_set_search_column (tree_view, COL_FOLDER_NAME);

			path = gtk_tree_path_new_first ();
			selection = gtk_tree_view_get_selection (tree_view);
			gtk_tree_selection_select_path (selection, path);
			gtk_tree_path_free (path);

			editor->priv->active->filtered_view = FALSE;
		}

		gtk_entry_set_icon_sensitive (
			entry, GTK_ENTRY_ICON_SECONDARY, FALSE);

		gtk_widget_set_sensitive (
			editor->priv->collapse_all_button, TRUE);
		gtk_widget_set_sensitive (
			editor->priv->expand_all_button, TRUE);
	}
}

static gboolean
subscription_editor_timeout_cb (gpointer user_data)
{
	EMSubscriptionEditor *editor;

	editor = EM_SUBSCRIPTION_EDITOR (user_data);
	subscription_editor_update_view (editor);
	editor->priv->timeout_id = 0;

	return FALSE;
}

static void
subscription_editor_combo_box_changed_cb (GtkComboBox *combo_box,
                                          EMSubscriptionEditor *editor)
{
	StoreData *data;
	gint index;

	index = gtk_combo_box_get_active (combo_box);
	g_return_if_fail (index < editor->priv->stores->len);

	data = g_ptr_array_index (editor->priv->stores, index);
	g_return_if_fail (data != NULL);

	editor->priv->active = data;

	subscription_editor_stop (editor);
	subscription_editor_update_view (editor);

	g_object_notify (G_OBJECT (editor), "store");

	if (data->needs_refresh) {
		subscription_editor_refresh (editor);
		data->needs_refresh = FALSE;
	}
}

static void
subscription_editor_entry_changed_cb (GtkEntry *entry,
                                      EMSubscriptionEditor *editor)
{
	const gchar *text;

	if (editor->priv->timeout_id > 0) {
		g_source_remove (editor->priv->timeout_id);
		editor->priv->timeout_id = 0;
	}

	text = gtk_entry_get_text (entry);

	if (text != NULL && *text != '\0') {
		editor->priv->timeout_id = e_named_timeout_add_seconds (
			1, subscription_editor_timeout_cb, editor);
	} else {
		subscription_editor_update_view (editor);
	}
}

static void
subscription_editor_icon_release_cb (GtkEntry *entry,
                                     GtkEntryIconPosition icon_pos,
                                     GdkEvent *event,
                                     EMSubscriptionEditor *editor)
{
	if (icon_pos == GTK_ENTRY_ICON_SECONDARY)
		gtk_entry_set_text (entry, "");
}

static void
subscription_editor_renderer_toggled_cb (GtkCellRendererToggle *renderer,
                                         const gchar *path_string,
                                         EMSubscriptionEditor *editor)
{
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreePath *path;

	tree_view = editor->priv->active->tree_view;
	selection = gtk_tree_view_get_selection (tree_view);

	path = gtk_tree_path_new_from_string (path_string);
	gtk_tree_selection_select_path (selection, path);
	gtk_tree_path_free (path);

	if (gtk_cell_renderer_toggle_get_active (renderer))
		subscription_editor_unsubscribe (editor);
	else
		subscription_editor_subscribe (editor);
}

static void
subscription_editor_render_toggle_cb (GtkCellLayout *cell_layout,
                                      GtkCellRenderer *renderer,
                                      GtkTreeModel *tree_model,
                                      GtkTreeIter *iter)
{
	CamelFolderInfo *folder_info;

	gtk_tree_model_get (
		tree_model, iter, COL_FOLDER_INFO, &folder_info, -1);

	g_object_set (
		renderer, "active", FOLDER_SUBSCRIBED (folder_info),
		"visible", FOLDER_CAN_SELECT (folder_info), NULL);
}

static void
subscription_editor_selection_changed_cb (GtkTreeSelection *selection,
                                          EMSubscriptionEditor *editor)
{
	GtkTreeModel *tree_model;
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected (selection, &tree_model, &iter)) {
		CamelFolderInfo *folder_info;

		gtk_tree_model_get (
			tree_model, &iter,
			COL_FOLDER_INFO, &folder_info, -1);
		gtk_widget_set_sensitive (
			editor->priv->subscribe_button,
			FOLDER_CAN_SELECT (folder_info) &&
			!FOLDER_SUBSCRIBED (folder_info));
		gtk_widget_set_sensitive (
			editor->priv->unsubscribe_button,
			FOLDER_CAN_SELECT (folder_info) &&
			FOLDER_SUBSCRIBED (folder_info));
	} else {
		gtk_widget_set_sensitive (
			editor->priv->subscribe_button, FALSE);
		gtk_widget_set_sensitive (
			editor->priv->unsubscribe_button, FALSE);
	}

	gtk_widget_set_sensitive (editor->priv->subscribe_arrow, TRUE);
	gtk_widget_set_sensitive (editor->priv->unsubscribe_arrow, TRUE);
}

static void
em_subscription_editor_get_unread_total_text_cb (GtkTreeViewColumn *tree_column,
						 GtkCellRenderer *cell,
						 GtkTreeModel *tree_model,
						 GtkTreeIter *iter,
						 gpointer user_data)
{
	CamelFolderInfo *folder_info = NULL;
	GString *text = NULL;

	g_return_if_fail (GTK_IS_CELL_RENDERER_TEXT (cell));
	g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
	g_return_if_fail (iter != NULL);

	gtk_tree_model_get (tree_model, iter, COL_FOLDER_INFO, &folder_info, -1);

	if (folder_info && folder_info->total > 0 && folder_info->unread >= 0 && folder_info->unread <= folder_info->total) {
		text = g_string_new ("");

		if (folder_info->unread > 0)
			g_string_append_printf (
				text, ngettext ("%d unread, ",
				"%d unread, ", folder_info->unread), folder_info->unread);

		g_string_append_printf (
			text, ngettext ("%d total", "%d total",
			folder_info->total), folder_info->total);
	}

	g_object_set (G_OBJECT (cell), "text", text ? text->str : NULL, NULL);

	if (text)
		g_string_free (text, TRUE);
}

static void
subscription_editor_add_store (EMSubscriptionEditor *editor,
                               CamelStore *store)
{
	StoreData *data;
	CamelService *service;
	GtkListStore *list_store;
	GtkTreeStore *tree_store;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GtkComboBoxText *combo_box;
	GtkWidget *container;
	GtkWidget *widget;
	const gchar *display_name;

	service = CAMEL_SERVICE (store);
	display_name = camel_service_get_display_name (service);

	combo_box = GTK_COMBO_BOX_TEXT (editor->priv->combo_box);
	gtk_combo_box_text_append_text (combo_box, display_name);

	tree_store = gtk_tree_store_new (
		N_COLUMNS,
		/* COL_CASEFOLDED */	G_TYPE_STRING,
		/* COL_FOLDER_ICON */	G_TYPE_STRING,
		/* COL_FOLDER_NAME */	G_TYPE_STRING,
		/* COL_FOLDER_INFO */	G_TYPE_POINTER);

	list_store = gtk_list_store_new (
		N_COLUMNS,
		/* COL_CASEFOLDED */	G_TYPE_STRING,
		/* COL_FOLDER_ICON */	G_TYPE_STRING,
		/* COL_FOLDER_NAME */	G_TYPE_STRING,
		/* COL_FOLDER_INFO */	G_TYPE_POINTER);

	container = editor->priv->notebook;

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_notebook_append_page (GTK_NOTEBOOK (container), widget, NULL);
	gtk_container_child_set (
		GTK_CONTAINER (container), widget,
		"tab-fill", FALSE, "tab-expand", FALSE, NULL);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_tree_view_new_with_model (GTK_TREE_MODEL (tree_store));
	gtk_tree_view_set_enable_search (GTK_TREE_VIEW (widget), TRUE);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (widget), FALSE);
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (widget), TRUE);
	gtk_tree_view_set_search_column (
		GTK_TREE_VIEW (widget), COL_FOLDER_NAME);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);

	renderer = gtk_cell_renderer_toggle_new ();
	g_object_set (renderer, "activatable", TRUE, NULL);
	gtk_tree_view_column_pack_start (column, renderer, FALSE);

	gtk_cell_layout_set_cell_data_func (
		GTK_CELL_LAYOUT (column), renderer,
		(GtkCellLayoutDataFunc) subscription_editor_render_toggle_cb,
		NULL, (GDestroyNotify) NULL);

	g_signal_connect (
		renderer, "toggled",
		G_CALLBACK (subscription_editor_renderer_toggled_cb), editor);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);
	gtk_tree_view_set_expander_column (GTK_TREE_VIEW (widget), column);

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "icon-name", COL_FOLDER_ICON);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "text", COL_FOLDER_NAME);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_cell_data_func (column, renderer,
		em_subscription_editor_get_unread_total_text_cb, NULL, NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));

	g_signal_connect (
		selection, "changed",
		G_CALLBACK (subscription_editor_selection_changed_cb), editor);

	data = g_slice_new0 (StoreData);
	data->store = g_object_ref (store);
	data->tree_view = GTK_TREE_VIEW (g_object_ref (widget));
	data->list_store = GTK_TREE_MODEL (list_store);
	data->tree_store = GTK_TREE_MODEL (tree_store);
	data->needs_refresh = TRUE;

	g_ptr_array_add (editor->priv->stores, data);
}

static void
emse_notebook_sensitive_changed_cb (GtkWidget *notebook,
                                    GParamSpec *param,
                                    GtkDialog *editor)
{
	gtk_dialog_set_response_sensitive (
		editor, GTK_RESPONSE_CLOSE,
		gtk_widget_get_sensitive (notebook));
}

static gboolean
subscription_editor_delete_event_cb (EMSubscriptionEditor *editor,
                                     GdkEvent *event,
                                     gpointer user_data)
{
	/* stop processing if the button is insensitive */
	return !gtk_widget_get_sensitive (editor->priv->notebook);
}

static void
subscription_editor_response_cb (EMSubscriptionEditor *editor,
                                 gint response_id,
                                 gpointer user_data)
{
	if (!gtk_widget_get_sensitive (editor->priv->notebook))
		g_signal_stop_emission_by_name (editor, "response");
}

static void
subscription_editor_set_store (EMSubscriptionEditor *editor,
                               CamelStore *store)
{
	g_return_if_fail (editor->priv->initial_store == NULL);

	if (CAMEL_IS_SUBSCRIBABLE (store))
		editor->priv->initial_store = g_object_ref (store);
}

static void
subscription_editor_set_session (EMSubscriptionEditor *editor,
                                 EMailSession *session)
{
	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (editor->priv->session == NULL);

	editor->priv->session = g_object_ref (session);
}

static void
subscription_editor_set_property (GObject *object,
                                  guint property_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SESSION:
			subscription_editor_set_session (
				EM_SUBSCRIPTION_EDITOR (object),
				g_value_get_object (value));
			return;

		case PROP_STORE:
			subscription_editor_set_store (
				EM_SUBSCRIPTION_EDITOR (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
subscription_editor_get_property (GObject *object,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SESSION:
			g_value_set_object (
				value,
				em_subscription_editor_get_session (
				EM_SUBSCRIPTION_EDITOR (object)));
			return;

		case PROP_STORE:
			g_value_set_object (
				value,
				em_subscription_editor_get_store (
				EM_SUBSCRIPTION_EDITOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
subscription_editor_dispose (GObject *object)
{
	EMSubscriptionEditor *self = EM_SUBSCRIPTION_EDITOR (object);

	g_clear_object (&self->priv->session);
	g_clear_object (&self->priv->initial_store);

	if (self->priv->timeout_id > 0) {
		g_source_remove (self->priv->timeout_id);
		self->priv->timeout_id = 0;
	}

	g_ptr_array_set_size (self->priv->stores, 0);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (em_subscription_editor_parent_class)->dispose (object);
}

static void
subscription_editor_finalize (GObject *object)
{
	EMSubscriptionEditor *self = EM_SUBSCRIPTION_EDITOR (object);

	g_ptr_array_free (self->priv->stores, TRUE);

	g_free (self->priv->search_string);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (em_subscription_editor_parent_class)->finalize (object);
}

static void
subscription_editor_constructed (GObject *object)
{
	EMSubscriptionEditor *editor;

	editor = EM_SUBSCRIPTION_EDITOR (object);

	/* Pick an initial store based on the default mail account, if
	 * one wasn't already given in em_subscription_editor_new(). */
	if (editor->priv->initial_store == NULL) {
		ESource *source;
		ESourceRegistry *registry;
		CamelService *service;
		EMailSession *session;

		session = em_subscription_editor_get_session (editor);
		registry = e_mail_session_get_registry (session);

		source = e_source_registry_ref_default_mail_account (registry);

		service = camel_session_ref_service (
			CAMEL_SESSION (session),
			e_source_get_uid (source));

		if (CAMEL_IS_SUBSCRIBABLE (service))
			editor->priv->initial_store = CAMEL_STORE (g_object_ref (service));

		if (service != NULL)
			g_object_unref (service);

		g_object_unref (source);
	}

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (em_subscription_editor_parent_class)->constructed (object);

	g_signal_connect (
		editor, "delete-event",
		G_CALLBACK (subscription_editor_delete_event_cb), NULL);
	g_signal_connect (
		editor, "response",
		G_CALLBACK (subscription_editor_response_cb), NULL);
}

static void
subscription_editor_realize (GtkWidget *widget)
{
	EMSubscriptionEditor *editor;
	EMFolderTreeModel *model;
	GtkComboBox *combo_box;
	GList *list, *link;
	gint initial_index = 0;

	editor = EM_SUBSCRIPTION_EDITOR (widget);

	/* Chain up to parent's realize() method. */
	GTK_WIDGET_CLASS (em_subscription_editor_parent_class)->realize (widget);

	/* Find stores to display, and watch for the initial store. */

	model = em_folder_tree_model_get_default ();
	list = em_folder_tree_model_list_stores (model);

	for (link = list; link != NULL; link = g_list_next (link)) {
		CamelStore *store = CAMEL_STORE (link->data);

		if (!CAMEL_IS_SUBSCRIBABLE (store))
			continue;

		if (store == editor->priv->initial_store)
			initial_index = editor->priv->stores->len;

		subscription_editor_add_store (editor, store);
	}

	g_list_free (list);

	/* The subscription editor should only be accessible if
	 * at least one enabled store supports subscriptions. */
	g_return_if_fail (editor->priv->stores->len > 0);

	combo_box = GTK_COMBO_BOX (editor->priv->combo_box);
	gtk_combo_box_set_active (combo_box, initial_index);

	g_signal_connect (
		combo_box, "changed",
		G_CALLBACK (subscription_editor_combo_box_changed_cb), editor);

	subscription_editor_combo_box_changed_cb (combo_box, editor);
}

static void
em_subscription_editor_class_init (EMSubscriptionEditorClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = subscription_editor_set_property;
	object_class->get_property = subscription_editor_get_property;
	object_class->dispose = subscription_editor_dispose;
	object_class->finalize = subscription_editor_finalize;
	object_class->constructed = subscription_editor_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->realize = subscription_editor_realize;

	g_object_class_install_property (
		object_class,
		PROP_SESSION,
		g_param_spec_object (
			"session",
			NULL,
			NULL,
			E_TYPE_MAIL_SESSION,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_STORE,
		g_param_spec_object (
			"store",
			NULL,
			NULL,
			CAMEL_TYPE_STORE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
em_subscription_editor_init (EMSubscriptionEditor *editor)
{
	GtkWidget *container;
	GtkWidget *widget;
	GtkWidget *box;
	const gchar *tooltip;

	editor->priv = em_subscription_editor_get_instance_private (editor);

	editor->priv->stores = g_ptr_array_new_with_free_func (
		(GDestroyNotify) store_data_free);

	gtk_container_set_border_width (GTK_CONTAINER (editor), 5);
	gtk_window_set_title (GTK_WINDOW (editor), _("Folder Subscriptions"));
	gtk_window_set_default_size (GTK_WINDOW (editor), 600, 400);

	e_restore_window (
		GTK_WINDOW (editor),
		"/org/gnome/evolution/mail/subscription-window/",
		E_RESTORE_WINDOW_SIZE);

	if (!e_util_get_use_header_bar ()) {
		gtk_dialog_add_button (
			GTK_DIALOG (editor),
			_("_Close"), GTK_RESPONSE_CLOSE);
	}

	container = gtk_dialog_get_content_area (GTK_DIALOG (editor));

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 5);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = box = widget;

	widget = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_combo_box_text_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 0, 1, 1);
	editor->priv->combo_box = widget;
	gtk_widget_show (widget);

	widget = gtk_label_new_with_mnemonic (_("_Account:"));
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (widget), editor->priv->combo_box);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 1, 1);
	gtk_widget_show (widget);

	widget = gtk_entry_new ();
	gtk_entry_set_icon_from_icon_name (
		GTK_ENTRY (widget),
		GTK_ENTRY_ICON_SECONDARY, "edit-clear");
	gtk_entry_set_icon_tooltip_text (
		GTK_ENTRY (widget),
		GTK_ENTRY_ICON_SECONDARY, _("Clear Search"));
	gtk_entry_set_icon_sensitive (
		GTK_ENTRY (widget),
		GTK_ENTRY_ICON_SECONDARY, FALSE);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 1, 1, 1);
	editor->priv->entry = widget;
	gtk_widget_show (widget);

	g_signal_connect (
		widget, "changed",
		G_CALLBACK (subscription_editor_entry_changed_cb), editor);

	g_signal_connect (
		widget, "icon-release",
		G_CALLBACK (subscription_editor_icon_release_cb), editor);

	widget = gtk_label_new_with_mnemonic (_("Sho_w items that contain:"));
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (widget), editor->priv->entry);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 1, 1);
	gtk_widget_show (widget);

	container = box;

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (widget), FALSE);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	editor->priv->notebook = widget;
	gtk_widget_show (widget);

	e_binding_bind_property (
		editor->priv->combo_box, "active",
		editor->priv->notebook, "page",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	if (!e_util_get_use_header_bar ()) {
		e_signal_connect_notify (
			widget, "notify::sensitive",
			G_CALLBACK (emse_notebook_sensitive_changed_cb), editor);
	}

	widget = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	gtk_box_set_spacing (GTK_BOX (widget), 6);
	gtk_button_box_set_layout (
		GTK_BUTTON_BOX (widget), GTK_BUTTONBOX_START);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, TRUE, 0);
	gtk_widget_show (widget);

	container = box = widget;

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, TRUE, 0);
	gtk_style_context_add_class (gtk_widget_get_style_context (widget), "linked");
	gtk_widget_show (widget);

	container = widget;

	tooltip = _("Subscribe to the selected folder");
	widget = gtk_button_new_with_mnemonic (_("Su_bscribe"));
	gtk_widget_set_sensitive (widget, FALSE);
	gtk_widget_set_tooltip_text (widget, tooltip);
	editor->priv->subscribe_button = widget;
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (subscription_editor_subscribe), editor);

	widget = gtk_button_new_from_icon_name ("pan-down-symbolic", GTK_ICON_SIZE_BUTTON);
	editor->priv->subscribe_arrow = widget;
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (subscription_editor_subscribe_popup_cb), editor);

	if (gtk_widget_get_direction (container) == GTK_TEXT_DIR_LTR) {
		gtk_box_pack_start (
			GTK_BOX (container),
			editor->priv->subscribe_button, TRUE, TRUE, 0);
		gtk_box_pack_start (
			GTK_BOX (container),
			editor->priv->subscribe_arrow, FALSE, FALSE, 0);
	} else {
		gtk_box_pack_start (
			GTK_BOX (container),
			editor->priv->subscribe_arrow, FALSE, FALSE, 0);
		gtk_box_pack_start (
			GTK_BOX (container),
			editor->priv->subscribe_button, TRUE, TRUE, 0);
	}

	container = box;

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_style_context_add_class (gtk_widget_get_style_context (widget), "linked");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	tooltip = _("Unsubscribe from the selected folder");
	widget = gtk_button_new_with_mnemonic (_("_Unsubscribe"));
	gtk_widget_set_sensitive (widget, FALSE);
	gtk_widget_set_tooltip_text (widget, tooltip);
	editor->priv->unsubscribe_button = widget;
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (subscription_editor_unsubscribe), editor);

	widget = gtk_button_new_from_icon_name ("pan-down-symbolic", GTK_ICON_SIZE_BUTTON);
	editor->priv->unsubscribe_arrow = widget;
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (subscription_editor_unsubscribe_popup_cb), editor);

	if (gtk_widget_get_direction (container) == GTK_TEXT_DIR_LTR) {
		gtk_box_pack_start (
			GTK_BOX (container),
			editor->priv->unsubscribe_button, TRUE, TRUE, 0);
		gtk_box_pack_start (
			GTK_BOX (container),
			editor->priv->unsubscribe_arrow, FALSE, FALSE, 0);
	} else {
		gtk_box_pack_start (
			GTK_BOX (container),
			editor->priv->unsubscribe_arrow, FALSE, FALSE, 0);
		gtk_box_pack_start (
			GTK_BOX (container),
			editor->priv->unsubscribe_button, TRUE, TRUE, 0);
	}

	container = box;

	tooltip = _("Collapse all folders");
	widget = gtk_button_new_with_mnemonic (_("C_ollapse All"));
	gtk_widget_set_tooltip_text (widget, tooltip);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	editor->priv->collapse_all_button = widget;
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (subscription_editor_collapse_all), editor);

	tooltip = _("Expand all folders");
	widget = gtk_button_new_with_mnemonic (_("E_xpand All"));
	gtk_widget_set_tooltip_text (widget, tooltip);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	editor->priv->expand_all_button = widget;
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (subscription_editor_expand_all), editor);

	tooltip = _("Refresh the folder list");
	widget = e_dialog_button_new_with_icon ("view-refresh", _("_Refresh"));
	gtk_widget_set_tooltip_text (widget, tooltip);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_set_sensitive (widget, FALSE);
	editor->priv->refresh_button = widget;
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (subscription_editor_refresh), editor);

	tooltip = _("Stop the current operation");
	widget = e_dialog_button_new_with_icon ("process-stop", _("_Stop"));
	gtk_widget_set_tooltip_text (widget, tooltip);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_set_sensitive (widget, FALSE);
	editor->priv->stop_button = widget;
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (subscription_editor_stop), editor);
}

GtkWidget *
em_subscription_editor_new (GtkWindow *parent,
                            EMailSession *session,
                            CamelStore *initial_store)
{
	g_return_val_if_fail (GTK_IS_WINDOW (parent), NULL);
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	return g_object_new (
		EM_TYPE_SUBSCRIPTION_EDITOR,
		"session", session,
		"store", initial_store,
		"use-header-bar", e_util_get_use_header_bar (),
		"transient-for", parent,
		NULL);
}

EMailSession *
em_subscription_editor_get_session (EMSubscriptionEditor *editor)
{
	g_return_val_if_fail (EM_IS_SUBSCRIPTION_EDITOR (editor), NULL);

	return editor->priv->session;
}

CamelStore *
em_subscription_editor_get_store (EMSubscriptionEditor *editor)
{
	g_return_val_if_fail (EM_IS_SUBSCRIPTION_EDITOR (editor), NULL);

	if (editor->priv->active == NULL)
		return NULL;

	return editor->priv->active->store;
}
