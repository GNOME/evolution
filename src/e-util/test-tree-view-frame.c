#include <gtk/gtk.h>
#include <libedataserver/libedataserver.h>

#include "e-misc-utils.h"
#include "e-tree-view-frame.h"

static GtkTreeView *glob_tree_view;
static ETreeViewFrame *glob_tree_view_frame;

static gboolean
delete_event_cb (GtkWidget *widget,
                 GdkEvent *event)
{
	gtk_main_quit ();

	return FALSE;
}

static void
action_add_cb (ETreeViewFrame *tree_view_frame,
	       EUIAction *action)
{
	GtkTreeView *tree_view;
	GtkTreeModel *tree_model;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreePath *path;
	GtkTreeIter iter;
	GList *list;

	tree_view = e_tree_view_frame_get_tree_view (tree_view_frame);

	tree_model = gtk_tree_view_get_model (tree_view);
	gtk_list_store_append (GTK_LIST_STORE (tree_model), &iter);
	path = gtk_tree_model_get_path (tree_model, &iter);

	column = gtk_tree_view_get_column (tree_view, 0);
	list = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
	renderer = GTK_CELL_RENDERER (list->data);
	g_list_free (list);

	g_object_set (renderer, "editable", TRUE, NULL);
	gtk_tree_view_set_cursor_on_cell (
		tree_view, path, column, renderer, TRUE);
	g_object_set (renderer, "editable", FALSE, NULL);

	gtk_tree_path_free (path);
}

static void
action_remove_cb (ETreeViewFrame *tree_view_frame,
		  EUIAction *action)
{
	GtkTreeView *tree_view;
	GtkTreeModel *tree_model;
	GtkTreeSelection *selection;
	GtkListStore *list_store;
	GList *list, *link;

	tree_view = e_tree_view_frame_get_tree_view (tree_view_frame);

	selection = gtk_tree_view_get_selection (tree_view);
	list = gtk_tree_selection_get_selected_rows (selection, &tree_model);

	/* Reverse the list so we don't invalidate paths. */
	list = g_list_reverse (list);

	list_store = GTK_LIST_STORE (tree_model);

	for (link = list; link != NULL; link = g_list_next (link)) {
		GtkTreePath *path = link->data;
		GtkTreeIter iter;

		if (gtk_tree_model_get_iter (tree_model, &iter, path))
			gtk_list_store_remove (list_store, &iter);
	}

	g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);
}

static void
update_toolbar_actions_cb (ETreeViewFrame *tree_view_frame)
{
	EUIAction *action;
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	gint n_selected_rows;

	tree_view = e_tree_view_frame_get_tree_view (tree_view_frame);

	selection = gtk_tree_view_get_selection (tree_view);
	n_selected_rows = gtk_tree_selection_count_selected_rows (selection);

	action = e_tree_view_frame_lookup_toolbar_action (
		tree_view_frame, E_TREE_VIEW_FRAME_ACTION_REMOVE);
	e_ui_action_set_sensitive (action, n_selected_rows > 0);
}

static void
cell_edited_cb (GtkCellRendererText *renderer,
                const gchar *path_string,
                const gchar *new_text,
                GtkTreeView *tree_view)
{
	GtkTreeModel *tree_model;
	GtkTreePath *path;
	GtkTreeIter iter;

	path = gtk_tree_path_new_from_string (path_string);

	tree_model = gtk_tree_view_get_model (tree_view);
	gtk_tree_model_get_iter (tree_model, &iter, path);
	gtk_list_store_set (
		GTK_LIST_STORE (tree_model), &iter, 0, new_text, -1);

	gtk_tree_path_free (path);
}

static void
editing_canceled_cb (GtkCellRenderer *renderer,
                     GtkTreeView *tree_view)
{
	GtkTreeModel *tree_model;
	GtkTreePath *path;
	GtkTreeIter iter;

	gtk_tree_view_get_cursor (tree_view, &path, NULL);

	tree_model = gtk_tree_view_get_model (tree_view);
	gtk_tree_model_get_iter (tree_model, &iter, path);
	gtk_list_store_remove (GTK_LIST_STORE (tree_model), &iter);

	gtk_tree_path_free (path);
}

static void
build_tree_view (void)
{
	GtkListStore *list_store;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeIter iter;
	guint ii;

	/* Somebody's a child of the 80's */
	const gchar *items[] = {
		"Cherry",
		"Strawberry",
		"Peach",
		"Pretzel",
		"Apple",
		"Pear",
		"Banana"
	};

	glob_tree_view = (GtkTreeView *) gtk_tree_view_new ();
	gtk_tree_view_set_headers_visible (glob_tree_view, FALSE);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (
		"Bonus Item", renderer, "text", 0, NULL);
	gtk_tree_view_append_column (glob_tree_view, column);

	g_signal_connect (
		renderer, "edited",
		G_CALLBACK (cell_edited_cb), glob_tree_view);

	g_signal_connect (
		renderer, "editing-canceled",
		G_CALLBACK (editing_canceled_cb), glob_tree_view);

	list_store = gtk_list_store_new (1, G_TYPE_STRING);
	for (ii = 0; ii < G_N_ELEMENTS (items); ii++) {
		gtk_list_store_append (list_store, &iter);
		gtk_list_store_set (list_store, &iter, 0, items[ii], -1);
	}
	gtk_tree_view_set_model (glob_tree_view, GTK_TREE_MODEL (list_store));
	g_object_unref (list_store);
}

static void
build_test_window (void)
{
	GtkTreeSelection *selection;
	GtkWidget *widget;
	GtkWidget *container;
	GtkWidget *grid;
	const gchar *text;

	selection = gtk_tree_view_get_selection (glob_tree_view);

	widget = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (widget), 500, 300);
	gtk_window_set_title (GTK_WINDOW (widget), "ETreeViewFrame");
	gtk_widget_show (widget);

	g_signal_connect (
		widget, "delete-event",
		G_CALLBACK (delete_event_cb), NULL);

	container = widget;

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 12);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = e_tree_view_frame_new ();
	e_tree_view_frame_set_tree_view (
		E_TREE_VIEW_FRAME (widget), glob_tree_view);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	glob_tree_view_frame = E_TREE_VIEW_FRAME (widget);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	text = "Inline toolbar is visible";
	widget = gtk_check_button_new_with_label (text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	e_binding_bind_property (
		glob_tree_view_frame, "toolbar-visible",
		widget, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	text = "Tree view is reorderable";
	widget = gtk_check_button_new_with_label (text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	e_binding_bind_property (
		glob_tree_view, "reorderable",
		widget, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	gtk_widget_set_margin_bottom (widget, 6);

	widget = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
	gtk_widget_set_margin_bottom (widget, 6);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	grid = widget;

	widget = gtk_label_new ("Tree view selection mode:");
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_widget_set_halign (GTK_WIDGET (widget), GTK_ALIGN_END);
	gtk_grid_attach (GTK_GRID (grid), widget, 0, 0, 1, 1);
	gtk_widget_show (widget);

	widget = gtk_combo_box_text_new ();
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		"none", "None");
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		"single", "Single");
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		"browse", "Browse");
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		"multiple", "Multiple");
	gtk_grid_attach (GTK_GRID (grid), widget, 1, 0, 1, 1);
	gtk_widget_show (widget);

	e_binding_bind_property_full (
		selection, "mode",
		widget, "active-id",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		e_binding_transform_enum_value_to_nick,
		e_binding_transform_enum_nick_to_value,
		NULL, (GDestroyNotify) NULL);

	widget = gtk_label_new ("Horizontal scrollbar policy:");
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_widget_set_halign (GTK_WIDGET (widget), GTK_ALIGN_END);
	gtk_grid_attach (GTK_GRID (grid), widget, 0, 1, 1, 1);
	gtk_widget_show (widget);

	widget = gtk_combo_box_text_new ();
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		"always", "Always");
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		"automatic", "Automatic");
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		"never", "Never");
	gtk_grid_attach (GTK_GRID (grid), widget, 1, 1, 1, 1);
	gtk_widget_show (widget);

	e_binding_bind_property_full (
		glob_tree_view_frame, "hscrollbar-policy",
		widget, "active-id",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		e_binding_transform_enum_value_to_nick,
		e_binding_transform_enum_nick_to_value,
		NULL, (GDestroyNotify) NULL);

	widget = gtk_label_new ("Vertical scrollbar policy:");
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_widget_set_halign (GTK_WIDGET (widget), GTK_ALIGN_END);
	gtk_grid_attach (GTK_GRID (grid), widget, 0, 2, 1, 1);
	gtk_widget_show (widget);

	widget = gtk_combo_box_text_new ();
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		"always", "Always");
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		"automatic", "Automatic");
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget),
		"never", "Never");
	gtk_grid_attach (GTK_GRID (grid), widget, 1, 2, 1, 1);
	gtk_widget_show (widget);

	e_binding_bind_property_full (
		glob_tree_view_frame, "vscrollbar-policy",
		widget, "active-id",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		e_binding_transform_enum_value_to_nick,
		e_binding_transform_enum_nick_to_value,
		NULL, (GDestroyNotify) NULL);

	g_signal_connect (
		glob_tree_view_frame,
		"toolbar-action-activate::"
		E_TREE_VIEW_FRAME_ACTION_ADD,
		G_CALLBACK (action_add_cb), NULL);

	g_signal_connect (
		glob_tree_view_frame,
		"toolbar-action-activate::"
		E_TREE_VIEW_FRAME_ACTION_REMOVE,
		G_CALLBACK (action_remove_cb), NULL);

	g_signal_connect (
		glob_tree_view_frame, "update-toolbar-actions",
		G_CALLBACK (update_toolbar_actions_cb), NULL);

	e_tree_view_frame_update_toolbar_actions (glob_tree_view_frame);
}

gint
main (gint argc,
      gchar **argv)
{
	gtk_init (&argc, &argv);

	build_tree_view ();
	build_test_window ();

	gtk_main ();

	e_misc_util_free_global_memory ();

	return 0;
}

