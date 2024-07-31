/*
 * e-tree-view-frame.c
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

/**
 * SECTION: e-tree-view-frame
 * @include: e-util/e-util.h
 * @short_description: A frame for #GtkTreeView
 *
 * #ETreeViewFrame embeds a #GtkTreeView in a scrolled window and adds an
 * inline-style toolbar beneath the scrolled window which can be hidden.
 *
 * The inline-style toolbar supports "add" and "remove" actions, as well
 * as move actions if the tree view is reorderable and selection actions
 * if the tree view supports multiple selections.  The action set can be
 * extended through e_tree_view_frame_insert_toolbar_action().
 **/

#include "evolution-config.h"

#include <libebackend/libebackend.h>

#include "e-misc-utils.h"

#include "e-tree-view-frame.h"

/**
 * E_TREE_VIEW_FRAME_ACTION_ADD:
 *
 * The #EUIAction name for the "add" toolbar button.
 *
 * Use e_tree_view_frame_lookup_toolbar_action() to obtain the #EUAction.
 **/

/**
 * E_TREE_VIEW_FRAME_ACTION_REMOVE:
 *
 * The #EUIAction name for the "remove" toolbar button.
 *
 * Use e_tree_view_frame_lookup_toolbar_action() to obtain the #EUIAction.
 **/

/**
 * E_TREE_VIEW_FRAME_ACTION_MOVE_TOP:
 *
 * The #EUIAction name for the "move selected items to top" button.
 *
 * Use e_tree_view_frame_lookup_toolbar_action() to obtain the #EUIAction.
 **/

/**
 * E_TREE_VIEW_FRAME_ACTION_MOVE_UP:
 *
 * The #EUIAction name for the "move selected items up" button.
 *
 * Use e_tree_view_frame_lookup_toolbar_action() to obtain the #EUIAction.
 **/

/**
 * E_TREE_VIEW_FRAME_ACTION_MOVE_DOWN:
 *
 * The #EUIAction name for the "move selected items down" button.
 *
 * Use e_tree_view_frame_lookup_toolbar_action() to obtain the #EUIAction.
 **/

/**
 * E_TREE_VIEW_FRAME_ACTION_MOVE_BOTTOM:
 *
 * The #EUIAction name for the "move selected items to bottom" button.
 *
 * Use e_tree_view_frame_lookup_toolbar_action() to obtain the #EUIAction.
 **/

/**
 * E_TREE_VIEW_FRAME_ACTION_SELECT_ALL:
 *
 * The #EUIAction name for the "select all" button.
 *
 * Use e_tree_view_frame_lookup_toolbar_action() to obtain the #EUIAction.
 **/

struct _ETreeViewFramePrivate {
	GtkTreeView *tree_view;
	gulong notify_reorderable_handler_id;
	gulong notify_select_mode_handler_id;
	gulong selection_changed_handler_id;

	GtkWidget *scrolled_window;
	GtkWidget *inline_toolbar;

	GHashTable *actions_ht; /* gchar *name ~> EUIAction * */

	GtkPolicyType hscrollbar_policy;
	GtkPolicyType vscrollbar_policy;

	gboolean toolbar_visible;
};

enum {
	PROP_0,
	PROP_HSCROLLBAR_POLICY,
	PROP_TREE_VIEW,
	PROP_TOOLBAR_VISIBLE,
	PROP_VSCROLLBAR_POLICY
};

enum {
	TOOLBAR_ACTION_ACTIVATE,
	UPDATE_TOOLBAR_ACTIONS,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE (ETreeViewFrame, e_tree_view_frame, GTK_TYPE_BOX,
	G_ADD_PRIVATE (ETreeViewFrame)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

static void
tree_view_frame_append_action (ETreeViewFrame *tree_view_frame,
                               const gchar *action_name,
                               const gchar *icon_name)
{
	EUIAction *action;

	action = e_ui_action_new ("tree-view-frame", action_name, NULL);

	e_ui_action_set_icon_name (action, icon_name);

	e_tree_view_frame_insert_toolbar_action (tree_view_frame, action, -1);

	g_object_unref (action);
}

static void
tree_view_frame_dispose_tree_view (ETreeViewFramePrivate *priv)
{
	if (priv->notify_reorderable_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->tree_view,
			priv->notify_reorderable_handler_id);
		priv->notify_reorderable_handler_id = 0;
	}

	if (priv->notify_select_mode_handler_id > 0) {
		g_signal_handler_disconnect (
			gtk_tree_view_get_selection (priv->tree_view),
			priv->notify_select_mode_handler_id);
		priv->notify_select_mode_handler_id = 0;
	}

	if (priv->selection_changed_handler_id > 0) {
		g_signal_handler_disconnect (
			gtk_tree_view_get_selection (priv->tree_view),
			priv->selection_changed_handler_id);
		priv->selection_changed_handler_id = 0;
	}

	g_clear_object (&priv->tree_view);
}

static gboolean
tree_view_frame_first_row_selected (GtkTreeView *tree_view)
{
	GtkTreeModel *tree_model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;

	tree_model = gtk_tree_view_get_model (tree_view);
	selection = gtk_tree_view_get_selection (tree_view);

	if (tree_model == NULL)
		return FALSE;

	if (!gtk_tree_model_iter_nth_child (tree_model, &iter, NULL, 0))
		return FALSE;

	return gtk_tree_selection_iter_is_selected (selection, &iter);
}

static gboolean
tree_view_frame_last_row_selected (GtkTreeView *tree_view)
{
	GtkTreeModel *tree_model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	gint last;

	tree_model = gtk_tree_view_get_model (tree_view);
	selection = gtk_tree_view_get_selection (tree_view);

	if (tree_model == NULL)
		return FALSE;

	last = gtk_tree_model_iter_n_children (tree_model, NULL) - 1;
	if (last < 0)
		return FALSE;

	if (!gtk_tree_model_iter_nth_child (tree_model, &iter, NULL, last))
		return FALSE;

	return gtk_tree_selection_iter_is_selected (selection, &iter);
}

static gboolean
tree_view_frame_move_selection_up (GtkTreeView *tree_view)
{
	GtkTreeModel *tree_model;
	GtkListStore *list_store;
	GtkTreeSelection *selection;
	GList *list, *link;

	tree_model = gtk_tree_view_get_model (tree_view);
	if (!GTK_IS_LIST_STORE (tree_model))
		return FALSE;

	if (tree_view_frame_first_row_selected (tree_view))
		return FALSE;

	list_store = GTK_LIST_STORE (tree_model);

	selection = gtk_tree_view_get_selection (tree_view);
	list = gtk_tree_selection_get_selected_rows (selection, NULL);

	/* Move all selected rows up one, even
	 * if the selection is not contiguous. */

	for (link = list; link != NULL; link = g_list_next (link)) {
		GtkTreePath *path = link->data;
		GtkTreeIter iter;
		GtkTreeIter prev;

		if (!gtk_tree_model_get_iter (tree_model, &iter, path)) {
			g_warn_if_reached ();
			continue;
		}

		prev = iter;
		if (!gtk_tree_model_iter_previous (tree_model, &prev)) {
			g_warn_if_reached ();
			continue;
		}

		gtk_list_store_swap (list_store, &iter, &prev);
	}

	g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);

	return TRUE;
}

static gboolean
tree_view_frame_move_selection_down (GtkTreeView *tree_view)
{
	GtkTreeModel *tree_model;
	GtkListStore *list_store;
	GtkTreeSelection *selection;
	GList *list, *link;

	tree_model = gtk_tree_view_get_model (tree_view);
	if (!GTK_IS_LIST_STORE (tree_model))
		return FALSE;

	if (tree_view_frame_last_row_selected (tree_view))
		return FALSE;

	list_store = GTK_LIST_STORE (tree_model);

	selection = gtk_tree_view_get_selection (tree_view);
	list = gtk_tree_selection_get_selected_rows (selection, NULL);

	/* Reverse the list so we don't disturb rows we've already moved. */
	list = g_list_reverse (list);

	for (link = list; link != NULL; link = g_list_next (link)) {
		GtkTreePath *path = link->data;
		GtkTreeIter iter;
		GtkTreeIter next;

		if (!gtk_tree_model_get_iter (tree_model, &iter, path)) {
			g_warn_if_reached ();
			continue;
		}

		next = iter;
		if (!gtk_tree_model_iter_next (tree_model, &next)) {
			g_warn_if_reached ();
			continue;
		}

		gtk_list_store_swap (list_store, &iter, &next);
	}

	g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);

	return TRUE;
}

static void
tree_view_frame_scroll_to_cursor (GtkTreeView *tree_view)
{
	GtkTreePath *path = NULL;

	gtk_tree_view_get_cursor (tree_view, &path, NULL);

	if (path != NULL) {
		gtk_tree_view_scroll_to_cell (
			tree_view, path, NULL, FALSE, 0.0, 0.0);
		gtk_tree_path_free (path);
	}
}

static void
tree_view_frame_action_go_top (ETreeViewFrame *tree_view_frame)
{
	GtkTreeView *tree_view;

	tree_view = e_tree_view_frame_get_tree_view (tree_view_frame);

	/* Not the most efficient method, but it's simple and works. */
	while (tree_view_frame_move_selection_up (tree_view))
		;

	tree_view_frame_scroll_to_cursor (tree_view);
	e_tree_view_frame_update_toolbar_actions (tree_view_frame);
}

static void
tree_view_frame_action_go_up (ETreeViewFrame *tree_view_frame)
{
	GtkTreeView *tree_view;

	tree_view = e_tree_view_frame_get_tree_view (tree_view_frame);

	tree_view_frame_move_selection_up (tree_view);

	tree_view_frame_scroll_to_cursor (tree_view);
	e_tree_view_frame_update_toolbar_actions (tree_view_frame);
}

static void
tree_view_frame_action_go_down (ETreeViewFrame *tree_view_frame)
{
	GtkTreeView *tree_view;

	tree_view = e_tree_view_frame_get_tree_view (tree_view_frame);

	tree_view_frame_move_selection_down (tree_view);

	tree_view_frame_scroll_to_cursor (tree_view);
	e_tree_view_frame_update_toolbar_actions (tree_view_frame);
}

static void
tree_view_frame_action_go_bottom (ETreeViewFrame *tree_view_frame)
{
	GtkTreeView *tree_view;

	tree_view = e_tree_view_frame_get_tree_view (tree_view_frame);

	/* Not the most efficient method, but it's simple and works. */
	while (tree_view_frame_move_selection_down (tree_view))
		;

	tree_view_frame_scroll_to_cursor (tree_view);
	e_tree_view_frame_update_toolbar_actions (tree_view_frame);
}

static void
tree_view_frame_action_select_all (ETreeViewFrame *tree_view_frame)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;

	tree_view = e_tree_view_frame_get_tree_view (tree_view_frame);
	selection = gtk_tree_view_get_selection (tree_view);

	gtk_tree_selection_select_all (selection);
}

static void
tree_view_frame_notify_reorderable_cb (GtkTreeView *tree_view,
                                       GParamSpec *pspec,
                                       ETreeViewFrame *tree_view_frame)
{
	e_tree_view_frame_update_toolbar_actions (tree_view_frame);
}

static void
tree_view_frame_notify_select_mode_cb (GtkTreeSelection *selection,
                                       GParamSpec *pspec,
                                       ETreeViewFrame *tree_view_frame)
{
	e_tree_view_frame_update_toolbar_actions (tree_view_frame);
}

static void
tree_view_frame_selection_changed_cb (GtkTreeSelection *selection,
                                      ETreeViewFrame *tree_view_frame)
{
	e_tree_view_frame_update_toolbar_actions (tree_view_frame);
}

static void
tree_view_frame_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_HSCROLLBAR_POLICY:
			e_tree_view_frame_set_hscrollbar_policy (
				E_TREE_VIEW_FRAME (object),
				g_value_get_enum (value));
			return;

		case PROP_TREE_VIEW:
			e_tree_view_frame_set_tree_view (
				E_TREE_VIEW_FRAME (object),
				g_value_get_object (value));
			return;

		case PROP_TOOLBAR_VISIBLE:
			e_tree_view_frame_set_toolbar_visible (
				E_TREE_VIEW_FRAME (object),
				g_value_get_boolean (value));
			return;

		case PROP_VSCROLLBAR_POLICY:
			e_tree_view_frame_set_vscrollbar_policy (
				E_TREE_VIEW_FRAME (object),
				g_value_get_enum (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
tree_view_frame_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_HSCROLLBAR_POLICY:
			g_value_set_enum (
				value,
				e_tree_view_frame_get_hscrollbar_policy (
				E_TREE_VIEW_FRAME (object)));
			return;

		case PROP_TREE_VIEW:
			g_value_set_object (
				value,
				e_tree_view_frame_get_tree_view (
				E_TREE_VIEW_FRAME (object)));
			return;

		case PROP_TOOLBAR_VISIBLE:
			g_value_set_boolean (
				value,
				e_tree_view_frame_get_toolbar_visible (
				E_TREE_VIEW_FRAME (object)));
			return;

		case PROP_VSCROLLBAR_POLICY:
			g_value_set_enum (
				value,
				e_tree_view_frame_get_vscrollbar_policy (
				E_TREE_VIEW_FRAME (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
tree_view_frame_dispose (GObject *object)
{
	ETreeViewFrame *self = E_TREE_VIEW_FRAME (object);

	tree_view_frame_dispose_tree_view (self->priv);

	g_clear_object (&self->priv->scrolled_window);
	g_clear_object (&self->priv->inline_toolbar);

	g_hash_table_remove_all (self->priv->actions_ht);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_tree_view_frame_parent_class)->dispose (object);
}

static void
tree_view_frame_finalize (GObject *object)
{
	ETreeViewFrame *self = E_TREE_VIEW_FRAME (object);

	g_hash_table_destroy (self->priv->actions_ht);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_tree_view_frame_parent_class)->finalize (object);
}

static void
tree_view_frame_constructed (GObject *object)
{
	ETreeViewFrame *tree_view_frame;
	GtkStyleContext *style_context;
	GtkWidget *widget;

	tree_view_frame = E_TREE_VIEW_FRAME (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_tree_view_frame_parent_class)->constructed (object);

	gtk_orientable_set_orientation (
		GTK_ORIENTABLE (tree_view_frame),
		GTK_ORIENTATION_VERTICAL);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (tree_view_frame), widget, TRUE, TRUE, 0);
	tree_view_frame->priv->scrolled_window = g_object_ref (widget);
	gtk_widget_show (widget);

	e_binding_bind_property (
		tree_view_frame, "hscrollbar-policy",
		widget, "hscrollbar-policy",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		tree_view_frame, "vscrollbar-policy",
		widget, "vscrollbar-policy",
		G_BINDING_SYNC_CREATE);

	widget = gtk_toolbar_new ();
	gtk_toolbar_set_show_arrow (GTK_TOOLBAR (widget), FALSE);
	gtk_toolbar_set_style (GTK_TOOLBAR (widget), GTK_TOOLBAR_ICONS);
	e_util_setup_toolbar_icon_size (GTK_TOOLBAR (widget), GTK_ICON_SIZE_MENU);
	gtk_box_pack_start (GTK_BOX (tree_view_frame), widget, FALSE, FALSE, 0);
	tree_view_frame->priv->inline_toolbar = g_object_ref (widget);
	gtk_widget_show (widget);

	style_context = gtk_widget_get_style_context (widget);
	gtk_style_context_add_class (
		style_context, GTK_STYLE_CLASS_INLINE_TOOLBAR);
	gtk_style_context_set_junction_sides (
		style_context, GTK_JUNCTION_TOP);

	e_binding_bind_property (
		tree_view_frame, "toolbar-visible",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	/* Define actions for toolbar items. */
	tree_view_frame_append_action (
		tree_view_frame,
		E_TREE_VIEW_FRAME_ACTION_ADD,
		"list-add-symbolic");
	tree_view_frame_append_action (
		tree_view_frame,
		E_TREE_VIEW_FRAME_ACTION_REMOVE,
		"list-remove-symbolic");
	tree_view_frame_append_action (
		tree_view_frame,
		E_TREE_VIEW_FRAME_ACTION_MOVE_TOP,
		"go-top-symbolic");
	tree_view_frame_append_action (
		tree_view_frame,
		E_TREE_VIEW_FRAME_ACTION_MOVE_UP,
		"go-up-symbolic");
	tree_view_frame_append_action (
		tree_view_frame,
		E_TREE_VIEW_FRAME_ACTION_MOVE_DOWN,
		"go-down-symbolic");
	tree_view_frame_append_action (
		tree_view_frame,
		E_TREE_VIEW_FRAME_ACTION_MOVE_BOTTOM,
		"go-bottom-symbolic");
	tree_view_frame_append_action (
		tree_view_frame,
		E_TREE_VIEW_FRAME_ACTION_SELECT_ALL,
		"edit-select-all-symbolic");

	/* Install a default GtkTreeView. */
	e_tree_view_frame_set_tree_view (tree_view_frame, NULL);
}

static gboolean
tree_view_frame_toolbar_action_activate (ETreeViewFrame *tree_view_frame,
					 EUIAction *action)
{
	const gchar *action_name;

	action_name = g_action_get_name (G_ACTION (action));
	g_return_val_if_fail (action_name != NULL, FALSE);

	if (g_str_equal (action_name, E_TREE_VIEW_FRAME_ACTION_MOVE_TOP)) {
		tree_view_frame_action_go_top (tree_view_frame);
		return TRUE;
	}

	if (g_str_equal (action_name, E_TREE_VIEW_FRAME_ACTION_MOVE_UP)) {
		tree_view_frame_action_go_up (tree_view_frame);
		return TRUE;
	}

	if (g_str_equal (action_name, E_TREE_VIEW_FRAME_ACTION_MOVE_DOWN)) {
		tree_view_frame_action_go_down (tree_view_frame);
		return TRUE;
	}

	if (g_str_equal (action_name, E_TREE_VIEW_FRAME_ACTION_MOVE_BOTTOM)) {
		tree_view_frame_action_go_bottom (tree_view_frame);
		return TRUE;
	}

	if (g_str_equal (action_name, E_TREE_VIEW_FRAME_ACTION_SELECT_ALL)) {
		tree_view_frame_action_select_all (tree_view_frame);
		return TRUE;
	}

	return FALSE;
}

static void
tree_view_frame_update_toolbar_actions (ETreeViewFrame *tree_view_frame)
{
	EUIAction *action;
	GtkTreeView *tree_view;
	GtkTreeModel *tree_model;
	GtkTreeSelection *selection;
	GtkSelectionMode selection_mode;
	gboolean first_row_selected;
	gboolean last_row_selected;
	gboolean sensitive;
	gboolean visible;
	gint n_selected_rows;
	gint n_rows = 0;

	/* XXX This implementation assumes the tree model is a list store.
	 *     A tree store will require special handling, although I don't
	 *     yet know if there's even a use case for a tree store here. */

	tree_view = e_tree_view_frame_get_tree_view (tree_view_frame);

	tree_model = gtk_tree_view_get_model (tree_view);
	if (tree_model != NULL)
		n_rows = gtk_tree_model_iter_n_children (tree_model, NULL);

	selection = gtk_tree_view_get_selection (tree_view);
	selection_mode = gtk_tree_selection_get_mode (selection);
	n_selected_rows = gtk_tree_selection_count_selected_rows (selection);

	first_row_selected = tree_view_frame_first_row_selected (tree_view);
	last_row_selected = tree_view_frame_last_row_selected (tree_view);

	action = e_tree_view_frame_lookup_toolbar_action (
		tree_view_frame, E_TREE_VIEW_FRAME_ACTION_MOVE_TOP);
	visible = gtk_tree_view_get_reorderable (tree_view);
	sensitive = (n_selected_rows > 0 && !first_row_selected);
	e_ui_action_set_visible (action, visible);
	e_ui_action_set_sensitive (action, sensitive);

	action = e_tree_view_frame_lookup_toolbar_action (
		tree_view_frame, E_TREE_VIEW_FRAME_ACTION_MOVE_UP);
	visible = gtk_tree_view_get_reorderable (tree_view);
	sensitive = (n_selected_rows > 0 && !first_row_selected);
	e_ui_action_set_visible (action, visible);
	e_ui_action_set_sensitive (action, sensitive);

	action = e_tree_view_frame_lookup_toolbar_action (
		tree_view_frame, E_TREE_VIEW_FRAME_ACTION_MOVE_DOWN);
	visible = gtk_tree_view_get_reorderable (tree_view);
	sensitive = (n_selected_rows > 0 && !last_row_selected);
	e_ui_action_set_visible (action, visible);
	e_ui_action_set_sensitive (action, sensitive);

	action = e_tree_view_frame_lookup_toolbar_action (
		tree_view_frame, E_TREE_VIEW_FRAME_ACTION_MOVE_BOTTOM);
	visible = gtk_tree_view_get_reorderable (tree_view);
	sensitive = (n_selected_rows > 0 && !last_row_selected);
	e_ui_action_set_visible (action, visible);
	e_ui_action_set_sensitive (action, sensitive);

	action = e_tree_view_frame_lookup_toolbar_action (
		tree_view_frame, E_TREE_VIEW_FRAME_ACTION_SELECT_ALL);
	visible = (selection_mode == GTK_SELECTION_MULTIPLE);
	sensitive = (n_selected_rows < n_rows);
	e_ui_action_set_visible (action, visible);
	e_ui_action_set_sensitive (action, sensitive);
}

static void
e_tree_view_frame_class_init (ETreeViewFrameClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = tree_view_frame_set_property;
	object_class->get_property = tree_view_frame_get_property;
	object_class->dispose = tree_view_frame_dispose;
	object_class->finalize = tree_view_frame_finalize;
	object_class->constructed = tree_view_frame_constructed;

	class->toolbar_action_activate = tree_view_frame_toolbar_action_activate;
	class->update_toolbar_actions = tree_view_frame_update_toolbar_actions;

	g_object_class_install_property (
		object_class,
		PROP_HSCROLLBAR_POLICY,
		g_param_spec_enum (
			"hscrollbar-policy",
			"Horizontal Scrollbar Policy",
			"When the horizontal scrollbar is displayed",
			GTK_TYPE_POLICY_TYPE,
			GTK_POLICY_AUTOMATIC,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	/* Don't use G_PARAM_CONSTRUCT here.  Our constructed() method
	 * will install a default GtkTreeView once the scrolled window
	 * is set up. */
	g_object_class_install_property (
		object_class,
		PROP_TREE_VIEW,
		g_param_spec_object (
			"tree-view",
			"Tree View",
			"The tree view widget",
			GTK_TYPE_TREE_VIEW,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_TOOLBAR_VISIBLE,
		g_param_spec_boolean (
			"toolbar-visible",
			"Toolbar Visible",
			"Whether to show the inline toolbar",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_VSCROLLBAR_POLICY,
		g_param_spec_enum (
			"vscrollbar-policy",
			"Vertical Scrollbar Policy",
			"When the vertical scrollbar is displayed",
			GTK_TYPE_POLICY_TYPE,
			GTK_POLICY_AUTOMATIC,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	/**
	 * ETreeViewFrame::toolbar-action-activate:
	 * @tree_view_frame: the #ETreeViewFrame that received the signal
	 * @action: the #EUIAction that was activated
	 *
	 * Emitted when a toolbar action is activated.
	 *
	 * This signal supports "::detail" appendices to the signal name,
	 * where the "detail" part is the action name.  So
	 * you can connect a signal handler to a particular action.
	 **/
	signals[TOOLBAR_ACTION_ACTIVATE] = g_signal_new (
		"toolbar-action-activate",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
		G_STRUCT_OFFSET (
			ETreeViewFrameClass,
			toolbar_action_activate),
		g_signal_accumulator_true_handled,
		NULL, NULL,
		G_TYPE_BOOLEAN, 1,
		E_TYPE_UI_ACTION);

	/**
	 * ETreeViewFrame::update-toolbar-actions:
	 * @tree_view_frame: the #ETreeViewFrame that received the signal
	 *
	 * Requests toolbar actions be updated, usually in response to a
	 * #GtkTreeSelection change.  Handlers should update action
	 * properties like #EUIAction:visible and #EUIAction:sensitive
	 * based on the current #ETreeViewFrame:tree-view state.
	 **/
	signals[UPDATE_TOOLBAR_ACTIONS] = g_signal_new (
		"update-toolbar-actions",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (
			ETreeViewFrameClass,
			update_toolbar_actions),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0);
}

static void
e_tree_view_frame_init (ETreeViewFrame *tree_view_frame)
{
	tree_view_frame->priv = e_tree_view_frame_get_instance_private (tree_view_frame);
	tree_view_frame->priv->actions_ht = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
}

/**
 * e_tree_view_frame_new:
 *
 * Creates a new #ETreeViewFrame.
 *
 * Returns: an #ETreeViewFrame
 **/
GtkWidget *
e_tree_view_frame_new (void)
{
	return g_object_new (E_TYPE_TREE_VIEW_FRAME, NULL);
}

/**
 * e_tree_view_frame_get_tree_view:
 * @tree_view_frame: an #ETreeViewFrame
 *
 * Returns the #ETreeViewFrame:tree-view for @tree_view_frame.
 *
 * The @tree_view_frame creates its own #GtkTreeView by default, but
 * that instance can be replaced with e_tree_view_frame_set_tree_view().
 *
 * Returns: a #GtkTreeView
 **/
GtkTreeView *
e_tree_view_frame_get_tree_view (ETreeViewFrame *tree_view_frame)
{
	g_return_val_if_fail (E_IS_TREE_VIEW_FRAME (tree_view_frame), NULL);

	return tree_view_frame->priv->tree_view;
}

/**
 * e_tree_view_frame_set_tree_view:
 * @tree_view_frame: an #ETreeViewFrame
 * @tree_view: a #GtkTreeView, or %NULL
 *
 * Replaces the previous #ETreeViewFrame:tree-view with the given @tree_view.
 * If @tree_view is %NULL, the @tree_view_frame creates a new #GtkTreeView.
 **/
void
e_tree_view_frame_set_tree_view (ETreeViewFrame *tree_view_frame,
                                 GtkTreeView *tree_view)
{
	GtkTreeSelection *selection;
	GtkWidget *scrolled_window;
	gulong handler_id;

	g_return_if_fail (E_IS_TREE_VIEW_FRAME (tree_view_frame));

	if (tree_view != NULL) {
		g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
		g_object_ref (tree_view);
	} else {
		tree_view = (GtkTreeView *) gtk_tree_view_new ();
		g_object_ref_sink (tree_view);
	}

	scrolled_window = tree_view_frame->priv->scrolled_window;

	if (tree_view_frame->priv->tree_view != NULL) {
		gtk_container_remove (
			GTK_CONTAINER (scrolled_window),
			GTK_WIDGET (tree_view_frame->priv->tree_view));
		tree_view_frame_dispose_tree_view (tree_view_frame->priv);
	}

	tree_view_frame->priv->tree_view = tree_view;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

	handler_id = e_signal_connect_notify (
		tree_view, "notify::reorderable",
		G_CALLBACK (tree_view_frame_notify_reorderable_cb),
		tree_view_frame);
	tree_view_frame->priv->notify_reorderable_handler_id = handler_id;

	handler_id = e_signal_connect_notify (
		selection, "notify::mode",
		G_CALLBACK (tree_view_frame_notify_select_mode_cb),
		tree_view_frame);
	tree_view_frame->priv->notify_select_mode_handler_id = handler_id;

	handler_id = g_signal_connect (
		selection, "changed",
		G_CALLBACK (tree_view_frame_selection_changed_cb),
		tree_view_frame);
	tree_view_frame->priv->selection_changed_handler_id = handler_id;

	gtk_container_add (
		GTK_CONTAINER (scrolled_window),
		GTK_WIDGET (tree_view));

	gtk_widget_show (GTK_WIDGET (tree_view));

	g_object_notify (G_OBJECT (tree_view_frame), "tree-view");

	e_tree_view_frame_update_toolbar_actions (tree_view_frame);
}

/**
 * e_tree_view_frame_get_toolbar_visible:
 * @tree_view_frame: an #ETreeViewFrame
 *
 * Returns whether the inline toolbar in @tree_view_frame is visible.
 *
 * Returns: %TRUE if the toolbar is visible, %FALSE if invisible
 **/
gboolean
e_tree_view_frame_get_toolbar_visible (ETreeViewFrame *tree_view_frame)
{
	g_return_val_if_fail (E_IS_TREE_VIEW_FRAME (tree_view_frame), FALSE);

	return tree_view_frame->priv->toolbar_visible;
}

/**
 * e_tree_view_frame_set_toolbar_visible:
 * @tree_view_frame: an #ETreeViewFrame
 * @toolbar_visible: whether to make the toolbar visible
 *
 * Shows or hides the inline toolbar in @tree_view_frame.
 **/
void
e_tree_view_frame_set_toolbar_visible (ETreeViewFrame *tree_view_frame,
                                       gboolean toolbar_visible)
{
	g_return_if_fail (E_IS_TREE_VIEW_FRAME (tree_view_frame));

	if (toolbar_visible == tree_view_frame->priv->toolbar_visible)
		return;

	tree_view_frame->priv->toolbar_visible = toolbar_visible;

	g_object_notify (G_OBJECT (tree_view_frame), "toolbar-visible");
}

/**
 * e_tree_view_frame_get_hscrollbar_policy:
 * @tree_view_frame: an #ETreeViewFrame
 *
 * Returns the policy for the horizontal scrollbar in @tree_view_frame.
 *
 * Returns: the policy for the horizontal scrollbar
 **/
GtkPolicyType
e_tree_view_frame_get_hscrollbar_policy (ETreeViewFrame *tree_view_frame)
{
	g_return_val_if_fail (E_IS_TREE_VIEW_FRAME (tree_view_frame), 0);

	return tree_view_frame->priv->hscrollbar_policy;
}

/**
 * e_tree_view_frame_set_hscrollbar_policy:
 * @tree_view_frame: an #ETreeViewFrame
 * @hscrollbar_policy: the policy for the horizontal scrollbar
 *
 * Sets the policy for the horizontal scrollbar in @tree_view_frame.
 **/
void
e_tree_view_frame_set_hscrollbar_policy (ETreeViewFrame *tree_view_frame,
                                         GtkPolicyType hscrollbar_policy)
{
	g_return_if_fail (E_IS_TREE_VIEW_FRAME (tree_view_frame));

	if (hscrollbar_policy == tree_view_frame->priv->hscrollbar_policy)
		return;

	tree_view_frame->priv->hscrollbar_policy = hscrollbar_policy;

	g_object_notify (G_OBJECT (tree_view_frame), "hscrollbar-policy");
}

/**
 * e_tree_view_frame_get_vscrollbar_policy:
 * @tree_view_frame: an #ETreeViewFrame
 *
 * Returns the policy for the vertical scrollbar in @tree_view_frame.
 *
 * Returns: the policy for the vertical scrollbar
 **/
GtkPolicyType
e_tree_view_frame_get_vscrollbar_policy (ETreeViewFrame *tree_view_frame)
{
	g_return_val_if_fail (E_IS_TREE_VIEW_FRAME (tree_view_frame), 0);

	return tree_view_frame->priv->vscrollbar_policy;
}

/**
 * e_tree_view_frame_set_vscrollbar_policy:
 * @tree_view_frame: an #ETreeViewFrame
 * @vscrollbar_policy: the policy for the vertical scrollbar
 *
 * Sets the policy for the vertical scrollbar in @tree_view_frame.
 **/
void
e_tree_view_frame_set_vscrollbar_policy (ETreeViewFrame *tree_view_frame,
                                         GtkPolicyType vscrollbar_policy)
{
	g_return_if_fail (E_IS_TREE_VIEW_FRAME (tree_view_frame));

	if (vscrollbar_policy == tree_view_frame->priv->vscrollbar_policy)
		return;

	tree_view_frame->priv->vscrollbar_policy = vscrollbar_policy;

	g_object_notify (G_OBJECT (tree_view_frame), "vscrollbar-policy");
}

static void
tree_view_frame_tool_item_clicked_cb (GtkToolButton *tool_button,
				      gpointer user_data)
{
	ETreeViewFrame *self = user_data;
	EUIAction *action;
	GQuark detail;
	const gchar *action_name;
	gboolean handled = FALSE;

	action = g_object_get_data (G_OBJECT (tool_button), "tree-view-frame-action");
	g_return_if_fail (action != NULL);

	action_name = g_action_get_name (G_ACTION (action));
	detail = g_quark_from_string (action_name);

	g_signal_emit (self, signals[TOOLBAR_ACTION_ACTIVATE], detail, action, &handled);

	if (!handled)
		g_action_activate (G_ACTION (action), NULL);
}

/**
 * e_tree_view_frame_insert_toolbar_action:
 * @tree_view_frame: an #ETreeViewFrame
 * @action: an #EUIAction
 * @position: the position of the new action
 *
 * Generates a #GtkToolItem from @action and inserts it into the inline
 * toolbar at the given @position.  If @position is zero, the item is
 * prepended to the start of the toolbar.  If @position is negative,
 * the item is appended to the end of the toolbar.
 **/
void
e_tree_view_frame_insert_toolbar_action (ETreeViewFrame *tree_view_frame,
					 EUIAction *action,
					 gint position)
{
	GtkToolbar *toolbar;
	GtkToolItem *tool_item;
	const gchar *action_name;

	g_return_if_fail (E_IS_TREE_VIEW_FRAME (tree_view_frame));
	g_return_if_fail (E_IS_UI_ACTION (action));

	action_name = g_action_get_name (G_ACTION (action));
	g_return_if_fail (action_name != NULL);

	toolbar = GTK_TOOLBAR (tree_view_frame->priv->inline_toolbar);

	if (g_hash_table_contains (tree_view_frame->priv->actions_ht, action_name)) {
		g_warning ("%s: Duplicate action name '%s'", G_STRFUNC, action_name);
		return;
	}

	tool_item = gtk_tool_button_new (NULL, NULL);
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (tool_item), e_ui_action_get_icon_name (action));
	gtk_tool_button_set_use_underline (GTK_TOOL_BUTTON (tool_item), TRUE);
	g_object_set_data_full (G_OBJECT (tool_item), "tree-view-frame-action",
		g_object_ref (action), g_object_unref);

	e_binding_bind_property (
		action, "label",
		tool_item, "label",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		action, "tooltip",
		tool_item, "tooltip-text",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		action, "sensitive",
		tool_item, "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		action, "visible",
		tool_item, "visible",
		G_BINDING_SYNC_CREATE);

	gtk_toolbar_insert (toolbar, GTK_TOOL_ITEM (tool_item), position);

	g_hash_table_insert (tree_view_frame->priv->actions_ht, (gpointer) g_action_get_name (G_ACTION (action)), g_object_ref (action));

	g_signal_connect (
		tool_item, "clicked",
		G_CALLBACK (tree_view_frame_tool_item_clicked_cb),
		tree_view_frame);
}

/**
 * e_tree_view_frame_lookup_toolbar_action:
 * @tree_view_frame: an #ETreeViewFrame
 * @action_name: an #EUIAction name
 *
 * Returns the toolbar action named @action_name, or %NULL if no such
 * toolbar action exists.
 *
 * Returns: an #EUIAction, or %NULL
 **/
EUIAction *
e_tree_view_frame_lookup_toolbar_action (ETreeViewFrame *tree_view_frame,
                                         const gchar *action_name)
{
	g_return_val_if_fail (E_IS_TREE_VIEW_FRAME (tree_view_frame), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	return g_hash_table_lookup (tree_view_frame->priv->actions_ht, action_name);
}

/**
 * e_tree_view_frame_update_toolbar_actions:
 * @tree_view_frame: an #ETreeViewFrame
 *
 * Emits the #ETreeViewFrame::update-toolbar-actions signal.
 *
 * See the signal description for more details.
 **/
void
e_tree_view_frame_update_toolbar_actions (ETreeViewFrame *tree_view_frame)
{
	g_return_if_fail (E_IS_TREE_VIEW_FRAME (tree_view_frame));

	g_signal_emit (tree_view_frame, signals[UPDATE_TOOLBAR_ACTIONS], 0);
}
