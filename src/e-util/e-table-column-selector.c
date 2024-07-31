/*
 * e-table-column-selector.c
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
 * SECTION: e-table-column-selector
 * @include: e-util/e-util.h
 * @short_description: Select columns for an #ETable or #ETree
 *
 * #ETableColumnSelector is a widget for choosing and ordering the
 * available columns of an #ETable or #ETree.
 **/

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "e-table-specification.h"
#include "e-table-column-selector.h"

struct _ETableColumnSelectorPrivate {
	ETableState *state;
};

enum {
	PROP_0,
	PROP_STATE
};

enum {
	COLUMN_ACTIVE,
	COLUMN_TITLE,
	COLUMN_SPECIFICATION,
	COLUMN_EXPANSION,
	NUM_COLUMNS
};

G_DEFINE_TYPE_WITH_PRIVATE (ETableColumnSelector, e_table_column_selector, E_TYPE_TREE_VIEW_FRAME)

static void
table_column_selector_toggled_cb (GtkCellRendererToggle *renderer,
                                  const gchar *path_string,
                                  GtkTreeView *tree_view)
{
	GtkTreeModel *tree_model;
	GtkTreeIter iter;
	gboolean active;

	tree_model = gtk_tree_view_get_model (tree_view);
	gtk_tree_model_get_iter_from_string (tree_model, &iter, path_string);

	gtk_tree_model_get (tree_model, &iter, COLUMN_ACTIVE, &active, -1);

	gtk_list_store_set (
		GTK_LIST_STORE (tree_model),
		&iter, COLUMN_ACTIVE, !active, -1);
}

static GtkTreeModel *
table_column_selector_build_model (ETableColumnSelector *selector)
{
	GtkListStore *list_store;
	GtkTreeIter iter;
	ETableState *state;
	ETableSpecification *specification;
	GPtrArray *columns;
	GHashTable *columns_added;
	guint ii;

	state = e_table_column_selector_get_state (selector);
	specification = e_table_state_ref_specification (state);
	columns = e_table_specification_ref_columns (specification);

	/* Set of ETableColumnSpecifications to help keep track
	 * of which ones are already added to the list store. */
	columns_added = g_hash_table_new (NULL, NULL);

	list_store = gtk_list_store_new (
		NUM_COLUMNS,
		G_TYPE_BOOLEAN,
		G_TYPE_STRING,
		E_TYPE_TABLE_COLUMN_SPECIFICATION,
		G_TYPE_DOUBLE);

	/* Add selected columns from ETableState first. */

	for (ii = 0; ii < state->col_count; ii++) {
		ETableColumnSpecification *column_spec;
		gdouble expansion;

		column_spec = state->column_specs[ii];
		expansion = state->expansions[ii];

		gtk_list_store_append (list_store, &iter);

		gtk_list_store_set (
			list_store, &iter,
			COLUMN_ACTIVE, TRUE,
			COLUMN_TITLE, _(column_spec->title),
			COLUMN_SPECIFICATION, column_spec,
			COLUMN_EXPANSION, expansion,
			-1);

		g_hash_table_add (columns_added, column_spec);
	}

	/* Add the rest of the columns from ETableSpecification. */

	for (ii = 0; ii < columns->len; ii++) {
		ETableColumnSpecification *column_spec;

		column_spec = g_ptr_array_index (columns, ii);

		if (g_hash_table_contains (columns_added, column_spec))
			continue;

		/* XXX We have this unfortunate "disabled" flag because
		 *     past developers made the mistake of having table
		 *     config files reference columns by number instead
		 *     of name so removing a column would break all the
		 *     table config files out in the wild. */
		if (column_spec->disabled)
			continue;

		gtk_list_store_append (list_store, &iter);

		gtk_list_store_set (
			list_store, &iter,
			COLUMN_ACTIVE, FALSE,
			COLUMN_TITLE, _(column_spec->title),
			COLUMN_SPECIFICATION, column_spec,
			COLUMN_EXPANSION, 1.0,
			-1);

		g_hash_table_add (columns_added, column_spec);
	}

	g_hash_table_destroy (columns_added);

	g_object_unref (specification);
	g_ptr_array_unref (columns);

	return GTK_TREE_MODEL (list_store);
}

static void
table_column_selector_set_state (ETableColumnSelector *selector,
                                 ETableState *state)
{
	g_return_if_fail (E_IS_TABLE_STATE (state));
	g_return_if_fail (selector->priv->state == NULL);

	selector->priv->state = g_object_ref (state);
}

static void
table_column_selector_set_property (GObject *object,
                                    guint property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_STATE:
			table_column_selector_set_state (
				E_TABLE_COLUMN_SELECTOR (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
table_column_selector_get_property (GObject *object,
                                    guint property_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_STATE:
			g_value_set_object (
				value,
				e_table_column_selector_get_state (
				E_TABLE_COLUMN_SELECTOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
table_column_selector_dispose (GObject *object)
{
	ETableColumnSelector *self = E_TABLE_COLUMN_SELECTOR (object);

	g_clear_object (&self->priv->state);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_table_column_selector_parent_class)->dispose (object);
}

static void
table_column_selector_constructed (GObject *object)
{
	ETableColumnSelector *selector;
	ETreeViewFrame *tree_view_frame;
	EUIAction *action;
	GtkTreeView *tree_view;
	GtkTreeModel *tree_model;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	const gchar *tooltip;

	selector = E_TABLE_COLUMN_SELECTOR (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_table_column_selector_parent_class)->constructed (object);

	tree_view_frame = E_TREE_VIEW_FRAME (object);
	tree_view = e_tree_view_frame_get_tree_view (tree_view_frame);

	gtk_tree_view_set_reorderable (tree_view, TRUE);
	gtk_tree_view_set_headers_visible (tree_view, FALSE);

	selection = gtk_tree_view_get_selection (tree_view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	/* Configure the toolbar actions. */

	action = e_tree_view_frame_lookup_toolbar_action (
		tree_view_frame, E_TREE_VIEW_FRAME_ACTION_ADD);
	e_ui_action_set_visible (action, FALSE);

	action = e_tree_view_frame_lookup_toolbar_action (
		tree_view_frame, E_TREE_VIEW_FRAME_ACTION_REMOVE);
	e_ui_action_set_visible (action, FALSE);

	action = e_tree_view_frame_lookup_toolbar_action (
		tree_view_frame, E_TREE_VIEW_FRAME_ACTION_MOVE_TOP);
	tooltip = _("Move selected column names to top");
	e_ui_action_set_tooltip (action, tooltip);

	action = e_tree_view_frame_lookup_toolbar_action (
		tree_view_frame, E_TREE_VIEW_FRAME_ACTION_MOVE_UP);
	tooltip = _("Move selected column names up one row");
	e_ui_action_set_tooltip (action, tooltip);

	action = e_tree_view_frame_lookup_toolbar_action (
		tree_view_frame, E_TREE_VIEW_FRAME_ACTION_MOVE_DOWN);
	tooltip = _("Move selected column names down one row");
	e_ui_action_set_tooltip (action, tooltip);

	action = e_tree_view_frame_lookup_toolbar_action (
		tree_view_frame, E_TREE_VIEW_FRAME_ACTION_MOVE_BOTTOM);
	tooltip = _("Move selected column names to bottom");
	e_ui_action_set_tooltip (action, tooltip);

	action = e_tree_view_frame_lookup_toolbar_action (
		tree_view_frame, E_TREE_VIEW_FRAME_ACTION_SELECT_ALL);
	tooltip = _("Select all column names");
	e_ui_action_set_tooltip (action, tooltip);

	/* Configure the tree view columns. */

	column = gtk_tree_view_column_new ();
	renderer = gtk_cell_renderer_toggle_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "active", COLUMN_ACTIVE);
	gtk_tree_view_append_column (tree_view, column);

	g_signal_connect (
		renderer, "toggled",
		G_CALLBACK (table_column_selector_toggled_cb),
		tree_view);

	column = gtk_tree_view_column_new ();
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "text", COLUMN_TITLE);
	gtk_tree_view_append_column (tree_view, column);

	/* Create and populate the tree model. */

	tree_model = table_column_selector_build_model (selector);
	gtk_tree_view_set_model (tree_view, tree_model);
	g_object_unref (tree_model);
}

static void
e_table_column_selector_class_init (ETableColumnSelectorClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = table_column_selector_set_property;
	object_class->get_property = table_column_selector_get_property;
	object_class->dispose = table_column_selector_dispose;
	object_class->constructed = table_column_selector_constructed;

	g_object_class_install_property (
		object_class,
		PROP_STATE,
		g_param_spec_object (
			"state",
			"Table State",
			"Column state of the source table",
			E_TYPE_TABLE_STATE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_table_column_selector_init (ETableColumnSelector *selector)
{
	selector->priv = e_table_column_selector_get_instance_private (selector);
}

/**
 * e_table_column_selector_new:
 * @state: an #ETableState
 *
 * Creates a new #ETableColumnSelector, obtaining the initial column
 * selection content from @state.
 *
 * Note that @state remains unmodified until e_table_column_selector_apply()
 * is called.
 *
 * Returns: an #ETableColumnSelector
 **/
GtkWidget *
e_table_column_selector_new (ETableState *state)
{
	g_return_val_if_fail (E_IS_TABLE_STATE (state), NULL);

	return g_object_new (
		E_TYPE_TABLE_COLUMN_SELECTOR,
		"state", state, NULL);
}

/**
 * e_table_column_selector_get_state:
 * @selector: an #ETableColumnSelector
 *
 * Returns the #ETableState passed to e_table_column_selector_new().
 *
 * Returns: an #ETableState
 **/
ETableState *
e_table_column_selector_get_state (ETableColumnSelector *selector)
{
	g_return_val_if_fail (E_IS_TABLE_COLUMN_SELECTOR (selector), NULL);

	return selector->priv->state;
}

/**
 * e_table_column_selector_apply:
 * @selector: an #ETableColumnSelector
 *
 * Applies the user's column preferences to the @selector's
 * #ETableColumnSelector:state instance.
 **/
void
e_table_column_selector_apply (ETableColumnSelector *selector)
{
	ETableState *state;
	ETreeViewFrame *tree_view_frame;
	GtkTreeView *tree_view;
	GtkTreeModel *tree_model;
	GArray *active_iters;
	GtkTreeIter iter;
	gboolean iter_valid;
	guint ii;

	g_return_if_fail (E_IS_TABLE_COLUMN_SELECTOR (selector));

	tree_view_frame = E_TREE_VIEW_FRAME (selector);
	tree_view = e_tree_view_frame_get_tree_view (tree_view_frame);
	tree_model = gtk_tree_view_get_model (tree_view);

	/* Collect all the "active" rows into an array of iterators. */

	active_iters = g_array_new (FALSE, TRUE, sizeof (GtkTreeIter));

	iter_valid = gtk_tree_model_get_iter_first (tree_model, &iter);

	while (iter_valid) {
		gboolean active;

		gtk_tree_model_get (
			tree_model, &iter, COLUMN_ACTIVE, &active, -1);

		if (active)
			g_array_append_val (active_iters, iter);

		iter_valid = gtk_tree_model_iter_next (tree_model, &iter);
	}

	/* Reconstruct the ETableState from the array of iterators. */

	state = e_table_column_selector_get_state (selector);

	for (ii = 0; ii < state->col_count; ii++)
		g_object_unref (state->column_specs[ii]);
	g_free (state->column_specs);
	g_free (state->expansions);

	state->col_count = active_iters->len;
	state->column_specs = g_new0 (
		ETableColumnSpecification *, active_iters->len);
	state->expansions = g_new0 (gdouble, active_iters->len);

	for (ii = 0; ii < active_iters->len; ii++) {
		ETableColumnSpecification *column_spec;
		gdouble expansion;

		iter = g_array_index (active_iters, GtkTreeIter, ii);

		gtk_tree_model_get (
			tree_model, &iter,
			COLUMN_SPECIFICATION, &column_spec,
			COLUMN_EXPANSION, &expansion,
			-1);

		state->column_specs[ii] = g_object_ref (column_spec);
		state->expansions[ii] = expansion;

		g_object_unref (column_spec);
	}

	g_array_free (active_iters, TRUE);
}
