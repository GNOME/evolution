/*
 * Copyright (C) 2019 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "em-folder-tree.h"
#include "em-folder-tree-model.h"

#include "e-mail-folder-sort-order-dialog.h"

struct _EMailFolderSortOrderDialogPrivate {
	CamelStore *store;
	gchar *folder_uri;

	GtkWidget *folder_tree;

	guint autoscroll_id;
	GtkTreeRowReference *drag_row;
	gboolean drag_changed;
	GHashTable *drag_state; /* gchar *folder_uri ~> guint sort_order */

	EUIAction *reset_current_level_action;
};

enum {
	PROP_0,
	PROP_FOLDER_URI,
	PROP_STORE
};

static void
e_mail_folder_sort_order_dialog_alert_sink_init (EAlertSinkInterface *interface);

G_DEFINE_TYPE_WITH_CODE (EMailFolderSortOrderDialog, e_mail_folder_sort_order_dialog, GTK_TYPE_DIALOG,
	G_ADD_PRIVATE (EMailFolderSortOrderDialog)
	G_IMPLEMENT_INTERFACE (E_TYPE_ALERT_SINK, e_mail_folder_sort_order_dialog_alert_sink_init))

static void
sort_order_dialog_selection_changed_cb (GtkTreeSelection *selection,
					gpointer user_data)
{
	EMailFolderSortOrderDialog *dialog = user_data;
	GtkTreeModel *model;
	GtkTreeIter iter, parent;
	gboolean can;

	g_return_if_fail (E_IS_MAIL_FOLDER_SORT_ORDER_DIALOG (dialog));

	can = gtk_tree_selection_get_selected (selection, &model, &iter) &&
	      gtk_tree_model_iter_parent (model, &parent, &iter) &&
	      gtk_tree_model_iter_children (model, &iter, &parent);

	if (can) {
		do {
			guint sort_order = 0;

			gtk_tree_model_get (model, &iter,
				COL_UINT_SORT_ORDER, &sort_order,
				-1);

			can = sort_order > 0;
		} while (!can && gtk_tree_model_iter_next (model, &iter));
	}

	e_ui_action_set_sensitive (dialog->priv->reset_current_level_action, can);
}

static void
sort_order_dialog_reset_current_level_activate_cb (EUIAction *action,
						   GVariant *parameter,
						   gpointer user_data)
{
	EMailFolderSortOrderDialog *dialog = user_data;
	EMailFolderTweaks *tweaks;
	GtkTreeModel *model = NULL;
	GtkTreeSelection *selection;
	GtkTreeIter iter, parent;

	g_return_if_fail (E_IS_MAIL_FOLDER_SORT_ORDER_DIALOG (dialog));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->priv->folder_tree));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	if (!gtk_tree_model_iter_parent (model, &parent, &iter) ||
	    !gtk_tree_model_iter_children (model, &iter, &parent))
		return;

	tweaks = em_folder_tree_model_get_folder_tweaks (EM_FOLDER_TREE_MODEL (model));

	/* Disable sorting */
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
		GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID, GTK_SORT_ASCENDING);

	do {
		gchar *folder_uri = NULL;

		gtk_tree_model_get (model, &iter,
			COL_STRING_FOLDER_URI, &folder_uri,
			-1);

		if (folder_uri) {
			e_mail_folder_tweaks_set_sort_order (tweaks, folder_uri, 0);
			g_free (folder_uri);
		}
	} while (gtk_tree_model_iter_next (model, &iter));

	/* Enable sorting */
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
		GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING);

	sort_order_dialog_selection_changed_cb (selection, dialog);
}

static void
sort_order_dialog_reset_all_levels_activate_cb (EUIAction *action,
						GVariant *parameter,
						gpointer user_data)
{
	EMailFolderSortOrderDialog *dialog = user_data;
	EMailFolderTweaks *tweaks;
	GtkTreeModel *model;
	gchar *top_folder_uri;

	g_return_if_fail (E_IS_MAIL_FOLDER_SORT_ORDER_DIALOG (dialog));

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->folder_tree));
	tweaks = em_folder_tree_model_get_folder_tweaks (EM_FOLDER_TREE_MODEL (model));

	/* Disable sorting */
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
		GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID, GTK_SORT_ASCENDING);

	top_folder_uri = e_mail_folder_uri_build (dialog->priv->store, "");
	e_mail_folder_tweaks_remove_sort_order_for_folders (tweaks, top_folder_uri);
	g_free (top_folder_uri);

	/* Enable sorting */
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
		GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING);

	sort_order_dialog_selection_changed_cb (gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->priv->folder_tree)), dialog);
}

static void
sort_order_tree_finish_drag (EMailFolderSortOrderDialog *dialog,
			     gboolean is_drop)
{
	GtkTreeModel *model = NULL;
	GtkTreePath *path = NULL;
	GtkTreeIter iter;
	gboolean has_level_top = FALSE;

	if (dialog->priv->drag_row) {
		GtkTreeIter parent;

		model = gtk_tree_row_reference_get_model (dialog->priv->drag_row);
		path = gtk_tree_row_reference_get_path (dialog->priv->drag_row);

		has_level_top = gtk_tree_model_get_iter (model, &iter, path) &&
				gtk_tree_model_iter_parent (model, &parent, &iter) &&
				gtk_tree_model_iter_children (model, &iter, &parent);
	}

	if (is_drop && has_level_top && model) {
		/* Save new order */
		EMailFolderTweaks *tweaks = em_folder_tree_model_get_folder_tweaks (EM_FOLDER_TREE_MODEL (model));

		do {
			gchar *folder_uri = NULL;
			guint sort_order = 0;

			gtk_tree_model_get (model, &iter,
				COL_STRING_FOLDER_URI, &folder_uri,
				COL_UINT_SORT_ORDER, &sort_order,
				-1);

			if (folder_uri) {
				e_mail_folder_tweaks_set_sort_order (tweaks, folder_uri, sort_order);

				g_free (folder_uri);
			}
		} while (gtk_tree_model_iter_next (model, &iter));

		sort_order_dialog_selection_changed_cb (gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->priv->folder_tree)), dialog);
	} else if (has_level_top && model && dialog->priv->drag_state) {
		/* Restore order to that before drag begun */
		GtkTreeStore *tree_store = GTK_TREE_STORE (model);

		do {
			gchar *folder_uri = NULL;

			gtk_tree_model_get (model, &iter,
				COL_STRING_FOLDER_URI, &folder_uri,
				-1);

			if (folder_uri) {
				guint sort_order;

				sort_order = GPOINTER_TO_UINT (g_hash_table_lookup (dialog->priv->drag_state, folder_uri));

				gtk_tree_store_set (tree_store, &iter,
					COL_UINT_SORT_ORDER, sort_order,
					-1);

				g_free (folder_uri);
			}
		} while (gtk_tree_model_iter_next (model, &iter));
	}

	if (model) {
		gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING);
	}

	gtk_tree_path_free (path);

	if (dialog->priv->autoscroll_id) {
		g_source_remove (dialog->priv->autoscroll_id);
		dialog->priv->autoscroll_id = 0;
	}

	g_clear_pointer (&dialog->priv->drag_row, gtk_tree_row_reference_free);
	g_clear_pointer (&dialog->priv->drag_state, g_hash_table_destroy);
}

#define SCROLL_EDGE_SIZE 15

static gboolean
sort_order_tree_autoscroll (gpointer user_data)
{
	EMailFolderSortOrderDialog *dialog = user_data;
	GtkAdjustment *adjustment;
	GtkTreeView *tree_view;
	GtkScrollable *scrollable;
	GdkRectangle rect;
	GdkWindow *window;
	GdkDisplay *display;
	GdkDeviceManager *device_manager;
	GdkDevice *device;
	gdouble value;
	gint offset, y;

	/* Get the y pointer position relative to the treeview. */
	tree_view = GTK_TREE_VIEW (dialog->priv->folder_tree);
	window = gtk_tree_view_get_bin_window (tree_view);
	display = gdk_window_get_display (window);
	device_manager = gdk_display_get_device_manager (display);
	device = gdk_device_manager_get_client_pointer (device_manager);
	gdk_window_get_device_position (window, device, NULL, &y, NULL);

	/* Rect is in coorinates relative to the scrolled window,
	 * relative to the treeview. */
	gtk_tree_view_get_visible_rect (tree_view, &rect);

	/* Move y into the same coordinate system as rect. */
	y += rect.y;

	/* See if we are near the top edge. */
	offset = y - (rect.y + 2 * SCROLL_EDGE_SIZE);
	if (offset > 0) {
		/* See if we are near the bottom edge. */
		offset = y - (rect.y + rect.height - 2 * SCROLL_EDGE_SIZE);
		if (offset < 0)
			return TRUE;
	}

	scrollable = GTK_SCROLLABLE (tree_view);
	adjustment = gtk_scrollable_get_vadjustment (scrollable);
	value = gtk_adjustment_get_value (adjustment);
	gtk_adjustment_set_value (adjustment, MAX (value + offset, 0.0));

	return TRUE;
}

static void
sort_order_tree_drag_begin_cb (GtkWidget *widget,
			       GdkDragContext *context,
			       gpointer user_data)
{
	EMailFolderSortOrderDialog *dialog = user_data;
	GtkTreeSelection *selection;
	cairo_surface_t *surface;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeView *tree_view;
	GtkTreeIter iter;
	gboolean is_folder = FALSE;

	g_return_if_fail (dialog != NULL);

	sort_order_tree_finish_drag (dialog, FALSE);

	tree_view = GTK_TREE_VIEW (dialog->priv->folder_tree);

	selection = gtk_tree_view_get_selection (tree_view);
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return;
	}

	gtk_tree_model_get (model, &iter, COL_BOOL_IS_FOLDER, &is_folder, -1);
	if (!is_folder) {
		return;
	}

	path = gtk_tree_model_get_path (model, &iter);

	dialog->priv->drag_row = gtk_tree_row_reference_new (model, path);
	dialog->priv->drag_changed = FALSE;

	surface = gtk_tree_view_create_row_drag_icon (tree_view, path);
	gtk_drag_set_icon_surface (context, surface);
	cairo_surface_destroy (surface);

	gtk_tree_path_free (path);

	if (dialog->priv->drag_row) {
		GtkTreeIter parent;

		if (gtk_tree_model_iter_parent (model, &parent, &iter) &&
		    gtk_tree_model_iter_children (model, &iter, &parent)) {
			GtkTreeStore *tree_store = GTK_TREE_STORE (model);
			guint index = 1;

			dialog->priv->drag_state = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

			do {
				guint sort_order = 0;
				gchar *folder_uri = NULL;

				gtk_tree_model_get (model, &iter,
					COL_STRING_FOLDER_URI, &folder_uri,
					COL_UINT_SORT_ORDER, &sort_order,
					-1);

				if (folder_uri) {
					g_hash_table_insert (dialog->priv->drag_state, folder_uri, GUINT_TO_POINTER (sort_order));

					gtk_tree_store_set (tree_store, &iter,
						COL_UINT_SORT_ORDER, index,
						-1);
				}

				index++;
			} while (gtk_tree_model_iter_next (model, &iter));
		}

		gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
			GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID, GTK_SORT_ASCENDING);
	}
}

static gboolean
sort_order_tree_drag_motion_cb (GtkWidget *widget,
				GdkDragContext *context,
				gint x,
				gint y,
				guint time,
				gpointer user_data)
{
	EMailFolderSortOrderDialog *dialog = user_data;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path = NULL, *drag_path;
	gboolean can, different_paths;

	g_return_val_if_fail (dialog != NULL, FALSE);

	tree_view = GTK_TREE_VIEW (dialog->priv->folder_tree);

	if (!dialog->priv->drag_row ||
	    !gtk_tree_view_get_dest_row_at_pos (tree_view, x, y, &path, NULL)) {
		gdk_drag_status (context, 0, time);
		return FALSE;
	}

	if (!dialog->priv->autoscroll_id) {
		dialog->priv->autoscroll_id = e_named_timeout_add (
			150, sort_order_tree_autoscroll, dialog);
	}

	model = gtk_tree_view_get_model (tree_view);

	g_warn_if_fail (gtk_tree_model_get_iter (model, &iter, path));

	drag_path = gtk_tree_row_reference_get_path (dialog->priv->drag_row);

	different_paths = drag_path && gtk_tree_path_compare (drag_path, path) != 0;

	/* The paths are not the same or the order was already changed, but they have the same parent */
	can = drag_path && (dialog->priv->drag_changed || different_paths) &&
	      gtk_tree_path_get_depth (drag_path) > 1 &&
	      gtk_tree_path_get_depth (path) > 1 &&
	      gtk_tree_path_up (drag_path) &&
	      gtk_tree_path_up (path) &&
	      gtk_tree_path_compare (drag_path, path) == 0;

	gtk_tree_path_free (drag_path);
	gtk_tree_path_free (path);

	if (can && different_paths) {
		GtkTreeStore *tree_store = GTK_TREE_STORE (model);
		GtkTreeIter drag_iter;
		guint drop_sort_order = 0, drag_sort_order = 0;

		drag_path = gtk_tree_row_reference_get_path (dialog->priv->drag_row);
		g_warn_if_fail (gtk_tree_model_get_iter (model, &drag_iter, drag_path));

		gtk_tree_path_free (drag_path);

		gtk_tree_model_get (model, &drag_iter,
			COL_UINT_SORT_ORDER, &drag_sort_order,
			-1);

		gtk_tree_model_get (model, &iter,
			COL_UINT_SORT_ORDER, &drop_sort_order,
			-1);

		if (drag_sort_order < drop_sort_order) {
			do {
				guint curr_sort_order = 0;

				gtk_tree_model_get (model, &drag_iter,
					COL_UINT_SORT_ORDER, &curr_sort_order,
					-1);

				if (curr_sort_order == drag_sort_order) {
					gtk_tree_store_set (tree_store, &drag_iter,
						COL_UINT_SORT_ORDER, drop_sort_order,
						-1);
				} else {
					g_warn_if_fail (curr_sort_order > 1);

					gtk_tree_store_set (tree_store, &drag_iter,
						COL_UINT_SORT_ORDER, curr_sort_order - 1,
						-1);

					if (curr_sort_order == drop_sort_order)
						break;
				}
			} while (gtk_tree_model_iter_next (model, &drag_iter));
		} else {
			do {
				guint curr_sort_order = 0;

				gtk_tree_model_get (model, &drag_iter,
					COL_UINT_SORT_ORDER, &curr_sort_order,
					-1);

				if (curr_sort_order == drag_sort_order) {
					gtk_tree_store_set (tree_store, &drag_iter,
						COL_UINT_SORT_ORDER, drop_sort_order,
						-1);
				} else {
					gtk_tree_store_set (tree_store, &drag_iter,
						COL_UINT_SORT_ORDER, curr_sort_order + 1,
						-1);

					if (curr_sort_order == drop_sort_order)
						break;
				}
			} while (gtk_tree_model_iter_previous (model, &drag_iter));
		}

		/* Re-sort after change by enable and immediately disable the sorting */
		gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING);

		gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
			GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID, GTK_SORT_ASCENDING);

		dialog->priv->drag_changed = TRUE;
	}

	gdk_drag_status (context, (can || dialog->priv->drag_changed) ? GDK_ACTION_MOVE : 0, time);

	return TRUE;
}

static void
sort_order_tree_drag_leave_cb (GtkWidget *widget,
			       GdkDragContext *context,
			       guint time,
			       gpointer user_data)
{
	EMailFolderSortOrderDialog *dialog = user_data;
	GtkTreeView *tree_view;

	tree_view = GTK_TREE_VIEW (dialog->priv->folder_tree);

	if (dialog->priv->autoscroll_id) {
		g_source_remove (dialog->priv->autoscroll_id);
		dialog->priv->autoscroll_id = 0;
	}

	gtk_tree_view_set_drag_dest_row (tree_view, NULL, GTK_TREE_VIEW_DROP_BEFORE);
}

static gboolean
sort_order_tree_drag_drop_cb (GtkWidget *widget,
			      GdkDragContext *context,
			      gint x,
			      gint y,
			      guint time,
			      gpointer user_data)
{
	EMailFolderSortOrderDialog *dialog = user_data;

	g_return_val_if_fail (dialog != NULL, FALSE);

	sort_order_tree_finish_drag (dialog, TRUE);

	return TRUE;
}

static void
sort_order_tree_drag_end_cb (GtkWidget *widget,
			     GdkDragContext *context,
			     gpointer user_data)
{
	EMailFolderSortOrderDialog *dialog = user_data;

	g_return_if_fail (dialog != NULL);

	sort_order_tree_finish_drag (dialog, FALSE);
}

static void
e_mail_folder_sort_order_dialog_realize (GtkWidget *widget)
{
	EMailFolderSortOrderDialog *dialog;
	GtkTreePath *path;

	/* Chain up to parent's method. */
	GTK_WIDGET_CLASS (e_mail_folder_sort_order_dialog_parent_class)->realize (widget);

	g_return_if_fail (E_IS_MAIL_FOLDER_SORT_ORDER_DIALOG (widget));

	dialog = E_MAIL_FOLDER_SORT_ORDER_DIALOG (widget);

	path = gtk_tree_path_new_first ();
	gtk_tree_view_expand_to_path (GTK_TREE_VIEW (dialog->priv->folder_tree), path);
	gtk_tree_path_free (path);

	if (dialog->priv->folder_uri)
		em_folder_tree_set_selected (EM_FOLDER_TREE (dialog->priv->folder_tree), dialog->priv->folder_uri, FALSE);
}

static void
e_mail_folder_sort_order_dialog_submit_alert (EAlertSink *alert_sink,
					      EAlert *alert)
{
	/* This should not be called */
	g_warn_if_reached ();
}

static void
e_mail_folder_sort_order_dialog_set_folder_uri (EMailFolderSortOrderDialog *dialog,
						const gchar *folder_uri)
{
	g_return_if_fail (E_IS_MAIL_FOLDER_SORT_ORDER_DIALOG (dialog));

	if (g_strcmp0 (dialog->priv->folder_uri, folder_uri) != 0) {
		g_free (dialog->priv->folder_uri);
		dialog->priv->folder_uri = g_strdup (folder_uri);

		g_object_notify (G_OBJECT (dialog), "folder-uri");
	}
}

static const gchar *
e_mail_folder_sort_order_dialog_get_folder_uri (EMailFolderSortOrderDialog *dialog)
{
	g_return_val_if_fail (E_IS_MAIL_FOLDER_SORT_ORDER_DIALOG (dialog), NULL);

	return dialog->priv->folder_uri;
}

static void
e_mail_folder_sort_order_dialog_set_store (EMailFolderSortOrderDialog *dialog,
					   CamelStore *store)
{
	g_return_if_fail (E_IS_MAIL_FOLDER_SORT_ORDER_DIALOG (dialog));
	g_return_if_fail (CAMEL_IS_STORE (store));

	if (dialog->priv->store != store) {
		g_clear_object (&dialog->priv->store);
		dialog->priv->store = g_object_ref (store);

		g_object_notify (G_OBJECT (dialog), "store");
	}
}

static CamelStore *
e_mail_folder_sort_order_dialog_get_store (EMailFolderSortOrderDialog *dialog)
{
	g_return_val_if_fail (E_IS_MAIL_FOLDER_SORT_ORDER_DIALOG (dialog), NULL);

	return dialog->priv->store;
}

static void
e_mail_folder_sort_order_dialog_set_property (GObject *object,
					      guint property_id,
					      const GValue *value,
					      GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FOLDER_URI:
			e_mail_folder_sort_order_dialog_set_folder_uri (
				E_MAIL_FOLDER_SORT_ORDER_DIALOG (object),
				g_value_get_string (value));
			return;

		case PROP_STORE:
			e_mail_folder_sort_order_dialog_set_store (
				E_MAIL_FOLDER_SORT_ORDER_DIALOG (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_mail_folder_sort_order_dialog_get_property (GObject *object,
					      guint property_id,
					      GValue *value,
					      GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FOLDER_URI:
			g_value_set_string (
				value,
				e_mail_folder_sort_order_dialog_get_folder_uri (
				E_MAIL_FOLDER_SORT_ORDER_DIALOG (object)));
			return;

		case PROP_STORE:
			g_value_set_object (
				value,
				e_mail_folder_sort_order_dialog_get_store (
				E_MAIL_FOLDER_SORT_ORDER_DIALOG (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_mail_folder_sort_order_dialog_constructed (GObject *object)
{
	const GtkTargetEntry row_targets[] = {
		{ (gchar *) "EMailFolderSortOrderDialog", GTK_TARGET_SAME_WIDGET, 0 }
	};
	EMailFolderSortOrderDialog *dialog = E_MAIL_FOLDER_SORT_ORDER_DIALOG (object);
	GtkWidget *folder_tree, *vbox, *widget;
	EMFolderTreeModel *model;
	ETreeViewFrame *tree_view_frame;
	GtkTreeSelection *selection;
	CamelSession *session;
	EUIAction *action;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_mail_folder_sort_order_dialog_parent_class)->constructed (object);

	g_return_if_fail (CAMEL_IS_STORE (dialog->priv->store));

	session = camel_service_ref_session (CAMEL_SERVICE (dialog->priv->store));
	g_return_if_fail (E_IS_MAIL_SESSION (session));

	gtk_window_set_default_size (GTK_WINDOW (dialog), 400, 500);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Folder Sort Order"));
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 5);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	vbox = widget;

	model = em_folder_tree_model_new ();
	em_folder_tree_model_set_session (model, E_MAIL_SESSION (session));
	em_folder_tree_model_add_store (model, dialog->priv->store);

	folder_tree = em_folder_tree_new_with_model (E_MAIL_SESSION (session), E_ALERT_SINK (dialog), model);
	gtk_widget_show (folder_tree);

	dialog->priv->folder_tree = folder_tree;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (folder_tree));

	g_signal_connect (selection, "changed",
		G_CALLBACK (sort_order_dialog_selection_changed_cb), dialog);

	widget = e_tree_view_frame_new ();
	gtk_box_pack_start (GTK_BOX (vbox), widget, TRUE, TRUE, 0);
	gtk_widget_set_size_request (widget, -1, 240);
	gtk_widget_show (widget);

	tree_view_frame = E_TREE_VIEW_FRAME (widget);
	e_tree_view_frame_set_tree_view (tree_view_frame, GTK_TREE_VIEW (folder_tree));
	e_tree_view_frame_set_toolbar_visible (tree_view_frame, TRUE);
	gtk_widget_grab_focus (folder_tree);

	action = e_tree_view_frame_lookup_toolbar_action (tree_view_frame, E_TREE_VIEW_FRAME_ACTION_ADD);
	e_ui_action_set_visible (action, FALSE);

	action = e_tree_view_frame_lookup_toolbar_action (tree_view_frame, E_TREE_VIEW_FRAME_ACTION_REMOVE);
	e_ui_action_set_visible (action, FALSE);

	action = e_ui_action_new ("FolderSortOrder", "reset-current", NULL);
	e_ui_action_set_label (action, _("Reset current level"));
	e_ui_action_set_tooltip (action, _("Reset sort order in the current level to the defaults"));

	dialog->priv->reset_current_level_action = action;

	g_signal_connect (action, "activate",
		G_CALLBACK (sort_order_dialog_reset_current_level_activate_cb), dialog);

	e_tree_view_frame_insert_toolbar_action (tree_view_frame, action, 0);

	action = e_ui_action_new ("FolderSortOrder", "reset-all", NULL);
	e_ui_action_set_label (action, _("Reset all levels"));
	e_ui_action_set_tooltip (action, _("Reset sort order in all levels to their defaults"));

	g_signal_connect (action, "activate",
		G_CALLBACK (sort_order_dialog_reset_all_levels_activate_cb), dialog);

	e_tree_view_frame_insert_toolbar_action (tree_view_frame, action, 1);

	g_clear_object (&session);
	g_clear_object (&model);

	if (!e_util_get_use_header_bar ())
		gtk_dialog_add_buttons (GTK_DIALOG (dialog), _("_Close"), GTK_RESPONSE_CANCEL, NULL);

	gtk_drag_source_set (dialog->priv->folder_tree, GDK_BUTTON1_MASK, row_targets, G_N_ELEMENTS (row_targets), GDK_ACTION_MOVE);
	gtk_drag_dest_set (dialog->priv->folder_tree, GTK_DEST_DEFAULT_MOTION, row_targets, G_N_ELEMENTS (row_targets), GDK_ACTION_MOVE);

	g_signal_connect (dialog->priv->folder_tree, "drag-begin",
		G_CALLBACK (sort_order_tree_drag_begin_cb), dialog);
	g_signal_connect (dialog->priv->folder_tree, "drag-motion",
		G_CALLBACK (sort_order_tree_drag_motion_cb), dialog);
	g_signal_connect (dialog->priv->folder_tree, "drag-leave",
		G_CALLBACK (sort_order_tree_drag_leave_cb), dialog);
	g_signal_connect (dialog->priv->folder_tree, "drag-drop",
		G_CALLBACK (sort_order_tree_drag_drop_cb), dialog);
	g_signal_connect (dialog->priv->folder_tree, "drag-end",
		G_CALLBACK (sort_order_tree_drag_end_cb), dialog);
}

static void
e_mail_folder_sort_order_dialog_dispose (GObject *object)
{
	EMailFolderSortOrderDialog *dialog = E_MAIL_FOLDER_SORT_ORDER_DIALOG (object);

	if (dialog->priv->autoscroll_id) {
		g_source_remove (dialog->priv->autoscroll_id);
		dialog->priv->autoscroll_id = 0;
	}

	g_clear_pointer (&dialog->priv->drag_row, gtk_tree_row_reference_free);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_mail_folder_sort_order_dialog_parent_class)->dispose (object);
}

static void
e_mail_folder_sort_order_dialog_finalize (GObject *object)
{
	EMailFolderSortOrderDialog *dialog = E_MAIL_FOLDER_SORT_ORDER_DIALOG (object);

	g_clear_object (&dialog->priv->store);
	g_clear_pointer (&dialog->priv->folder_uri, g_free);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_mail_folder_sort_order_dialog_parent_class)->finalize (object);
}

static void
e_mail_folder_sort_order_dialog_class_init (EMailFolderSortOrderDialogClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->realize = e_mail_folder_sort_order_dialog_realize;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = e_mail_folder_sort_order_dialog_set_property;
	object_class->get_property = e_mail_folder_sort_order_dialog_get_property;
	object_class->constructed = e_mail_folder_sort_order_dialog_constructed;
	object_class->dispose = e_mail_folder_sort_order_dialog_dispose;
	object_class->finalize = e_mail_folder_sort_order_dialog_finalize;

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

	g_object_class_install_property (
		object_class,
		PROP_FOLDER_URI,
		g_param_spec_string (
			"folder-uri",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_folder_sort_order_dialog_alert_sink_init (EAlertSinkInterface *interface)
{
	interface->submit_alert = e_mail_folder_sort_order_dialog_submit_alert;
}

static void
e_mail_folder_sort_order_dialog_init (EMailFolderSortOrderDialog *dialog)
{
	dialog->priv = e_mail_folder_sort_order_dialog_get_instance_private (dialog);
}

GtkWidget *
e_mail_folder_sort_order_dialog_new (GtkWindow *parent,
				     CamelStore *store,
				     const gchar *folder_uri)
{
	g_return_val_if_fail (CAMEL_IS_STORE (store), NULL);

	return g_object_new (E_TYPE_MAIL_FOLDER_SORT_ORDER_DIALOG,
		"transient-for", parent,
		"use-header-bar", e_util_get_use_header_bar (),
		"store", store,
		"folder-uri", folder_uri,
		NULL);
}
