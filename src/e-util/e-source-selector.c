/* e-source-selector.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * published by the Free Software Foundation; either the version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser  General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n-lib.h>

#include <libedataserverui/libedataserverui.h>

#include "e-source-selector.h"

typedef struct _AsyncContext AsyncContext;

struct _ESourceSelectorPrivate {
	ESourceRegistry *registry;
	gulong source_added_handler_id;
	gulong source_changed_handler_id;
	gulong source_removed_handler_id;
	gulong source_enabled_handler_id;
	gulong source_disabled_handler_id;

	GHashTable *source_index;
	gchar *extension_name;

	GtkTreeRowReference *saved_primary_selection;

	/* ESource -> GSource */
	GHashTable *pending_writes;
	GMainContext *main_context;

	gboolean toggled_last;
	gboolean show_colors;
	gboolean show_icons;
	gboolean show_toggles;

	GtkCellRenderer *busy_renderer;
	guint n_busy_sources;
	gulong update_busy_renderer_id;

	GHashTable *hidden_groups;
	GSList *groups_order;
};

struct _AsyncContext {
	ESourceSelector *selector;
	ESource *source;
};

enum {
	PROP_0,
	PROP_EXTENSION_NAME,
	PROP_PRIMARY_SELECTION,
	PROP_REGISTRY,
	PROP_SHOW_COLORS,
	PROP_SHOW_ICONS,
	PROP_SHOW_TOGGLES
};

enum {
	SELECTION_CHANGED,
	PRIMARY_SELECTION_CHANGED,
	POPUP_EVENT,
	DATA_DROPPED,
	SOURCE_SELECTED,
	SOURCE_UNSELECTED,
	FILTER_SOURCE,
	SOURCE_CHILD_SELECTED,
	NUM_SIGNALS
};

enum {
	COLUMN_NAME,
	COLUMN_COLOR,
	COLUMN_ACTIVE,
	COLUMN_ICON_NAME,
	COLUMN_SHOW_COLOR,
	COLUMN_SHOW_ICONS,
	COLUMN_SHOW_TOGGLE,
	COLUMN_WEIGHT,
	COLUMN_SOURCE,
	COLUMN_TOOLTIP,
	COLUMN_IS_BUSY,
	COLUMN_CONNECTION_STATUS,
	COLUMN_SORT_ORDER,
	COLUMN_CHILD_DATA,
	NUM_COLUMNS
};

static guint signals[NUM_SIGNALS];

G_DEFINE_TYPE_WITH_PRIVATE (ESourceSelector, e_source_selector, GTK_TYPE_TREE_VIEW)

/* ESafeToggleRenderer does not emit 'toggled' signal
 * on 'activate' when mouse is not over the toggle. */

typedef GtkCellRendererToggle ECellRendererSafeToggle;
typedef GtkCellRendererToggleClass ECellRendererSafeToggleClass;

/* Forward Declarations */
GType		e_cell_renderer_safe_toggle_get_type
						(void) G_GNUC_CONST;
static void	selection_changed_callback	(GtkTreeSelection *selection,
						 ESourceSelector *selector);

G_DEFINE_TYPE (ECellRendererSafeToggle, e_cell_renderer_safe_toggle, GTK_TYPE_CELL_RENDERER_TOGGLE)

static gboolean
safe_toggle_activate (GtkCellRenderer *cell,
                      GdkEvent *event,
                      GtkWidget *widget,
                      const gchar *path,
                      const GdkRectangle *background_area,
                      const GdkRectangle *cell_area,
                      GtkCellRendererState flags)
{
	gboolean point_in_cell_area = TRUE;

	if (event && event->type == GDK_BUTTON_PRESS && cell_area != NULL) {
		cairo_region_t *region;

		region = cairo_region_create_rectangle (cell_area);
		point_in_cell_area = cairo_region_contains_point (
			region, event->button.x, event->button.y);
		cairo_region_destroy (region);
	}

	if (!point_in_cell_area)
		return FALSE;

	return GTK_CELL_RENDERER_CLASS (
		e_cell_renderer_safe_toggle_parent_class)->activate (
		cell, event, widget, path, background_area, cell_area, flags);
}

static void
e_cell_renderer_safe_toggle_class_init (ECellRendererSafeToggleClass *class)
{
	GtkCellRendererClass *cell_renderer_class;

	cell_renderer_class = GTK_CELL_RENDERER_CLASS (class);
	cell_renderer_class->activate = safe_toggle_activate;
}

static void
e_cell_renderer_safe_toggle_init (ECellRendererSafeToggle *obj)
{
}

static GtkCellRenderer *
e_cell_renderer_safe_toggle_new (void)
{
	return g_object_new (e_cell_renderer_safe_toggle_get_type (), NULL);
}

static gboolean
source_selector_pulse_busy_renderer_cb (gpointer user_data)
{
	ESourceSelector *selector = user_data;
	GObject *busy_renderer;
	guint pulse;
	GHashTableIter iter;
	gpointer key, value;

	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), FALSE);

	if (!selector->priv->busy_renderer)
		return FALSE;

	busy_renderer = G_OBJECT (selector->priv->busy_renderer);

	g_object_get (busy_renderer, "pulse", &pulse, NULL);

	pulse++;

	g_object_set (busy_renderer, "pulse", pulse, NULL);

	g_hash_table_iter_init (&iter, selector->priv->source_index);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		GtkTreeRowReference *reference = value;
		GtkTreeModel *model;
		GtkTreePath *path;
		GtkTreeIter tree_iter;

		if (reference && gtk_tree_row_reference_valid (reference)) {
			gboolean is_busy = FALSE;

			model = gtk_tree_row_reference_get_model (reference);
			path = gtk_tree_row_reference_get_path (reference);
			gtk_tree_model_get_iter (model, &tree_iter, path);

			gtk_tree_model_get (
				model, &tree_iter,
				COLUMN_IS_BUSY, &is_busy,
				-1);

			if (is_busy)
				gtk_tree_model_row_changed (model, path, &tree_iter);

			gtk_tree_path_free (path);
		}
	}

	return TRUE;
}

static void
source_selector_inc_busy_sources (ESourceSelector *selector)
{
	selector->priv->n_busy_sources++;

	if (selector->priv->busy_renderer && !selector->priv->update_busy_renderer_id)
		selector->priv->update_busy_renderer_id =
			e_named_timeout_add (123, source_selector_pulse_busy_renderer_cb, selector);
}

static void
source_selector_dec_busy_sources (ESourceSelector *selector)
{
	g_return_if_fail (selector->priv->n_busy_sources > 0);

	selector->priv->n_busy_sources--;

	if (selector->priv->n_busy_sources == 0 && selector->priv->update_busy_renderer_id) {
		g_source_remove (selector->priv->update_busy_renderer_id);
		selector->priv->update_busy_renderer_id = 0;
	}
}

static void
clear_saved_primary_selection (ESourceSelector *selector)
{
	gtk_tree_row_reference_free (selector->priv->saved_primary_selection);
	selector->priv->saved_primary_selection = NULL;
}

static void
async_context_free (AsyncContext *async_context)
{
	if (async_context->selector != NULL)
		g_object_unref (async_context->selector);

	if (async_context->source != NULL)
		g_object_unref (async_context->source);

	g_slice_free (AsyncContext, async_context);
}

static void
pending_writes_destroy_source (GSource *source)
{
	g_source_destroy (source);
	g_source_unref (source);
}

static void
source_selector_write_done_cb (GObject *source_object,
                               GAsyncResult *result,
                               gpointer user_data)
{
	ESource *source;
	ESourceSelector *selector;
	GError *error = NULL;

	source = E_SOURCE (source_object);
	selector = E_SOURCE_SELECTOR (user_data);

	e_source_write_finish (source, result, &error);

	/* FIXME Display the error in the selector somehow? */
	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	g_object_unref (selector);
}

static gboolean
source_selector_write_idle_cb (gpointer user_data)
{
	AsyncContext *async_context = user_data;
	GHashTable *pending_writes;

	/* XXX This operation is not cancellable. */
	e_source_write (
		async_context->source, NULL,
		source_selector_write_done_cb,
		g_object_ref (async_context->selector));

	pending_writes = async_context->selector->priv->pending_writes;
	g_hash_table_remove (pending_writes, async_context->source);

	return FALSE;
}

static void
source_selector_cancel_write (ESourceSelector *selector,
                              ESource *source)
{
	GHashTable *pending_writes;

	/* Cancel any pending writes for this ESource so as not
	 * to overwrite whatever change we're being notified of. */
	pending_writes = selector->priv->pending_writes;
	g_hash_table_remove (pending_writes, source);
}

static const gchar *
source_selector_get_icon_name (ESourceSelector *selector,
                               ESource *source)
{
	const gchar *extension_name;
	const gchar *icon_name = NULL;

	/* XXX These are the same icons used in EShellView subclasses.
	 *     We should really centralize these icon names somewhere. */

	extension_name = E_SOURCE_EXTENSION_ADDRESS_BOOK;
	if (e_source_has_extension (source, extension_name))
		icon_name = "x-office-address-book";

	extension_name = E_SOURCE_EXTENSION_CALENDAR;
	if (e_source_has_extension (source, extension_name))
		icon_name = "x-office-calendar";

	extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
	if (e_source_has_extension (source, extension_name))
		icon_name = "evolution-mail";

	extension_name = E_SOURCE_EXTENSION_MAIL_TRANSPORT;
	if (e_source_has_extension (source, extension_name))
		icon_name = "mail-send";

	extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
	if (e_source_has_extension (source, extension_name))
		icon_name = "evolution-memos";

	extension_name = E_SOURCE_EXTENSION_TASK_LIST;
	if (e_source_has_extension (source, extension_name))
		icon_name = "evolution-tasks";

	return icon_name;
}

static gboolean
source_selector_source_is_enabled_and_selected (ESource *source,
						const gchar *extension_name)
{
	gpointer extension;

	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);

	if (!extension_name ||
	    !e_source_get_enabled (source))
		return e_source_get_enabled (source);

	if (!e_source_has_extension (source, extension_name))
		return FALSE;

	extension = e_source_get_extension (source, extension_name);
	if (!E_IS_SOURCE_SELECTABLE (extension))
		return TRUE;

	return e_source_selectable_get_selected (extension);
}

typedef struct {
	const gchar *extension_name;
	gboolean show_toggles;
	gboolean any_selected;
} LookupSelectedData;

static gboolean
source_selector_lookup_selected_cb (GNode *node,
				    gpointer user_data)
{
	LookupSelectedData *data = user_data;
	ESource *source;

	g_return_val_if_fail (data != NULL, TRUE);
	g_return_val_if_fail (data->extension_name != NULL, TRUE);

	source = node->data;
	if (!E_IS_SOURCE (source))
		return TRUE;

	data->any_selected = data->show_toggles && source_selector_source_is_enabled_and_selected (source, data->extension_name);

	return data->any_selected;
}

static gboolean
source_selector_node_is_hidden (ESourceSelector *selector,
				GNode *main_node)
{
	GNode *node;
	ESource *source;
	const gchar *extension_name;
	LookupSelectedData data;
	gboolean hidden;

	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), FALSE);
	g_return_val_if_fail (main_node != NULL, FALSE);

	if (G_NODE_IS_ROOT (main_node))
		return FALSE;

	extension_name = e_source_selector_get_extension_name (selector);
	data.show_toggles = e_source_selector_get_show_toggles (selector);
	hidden = FALSE;

	/* Check the path to the root, any is hidden, this one can be also hidden */
	node = main_node;
	while (node) {
		gboolean hidden_by_filter = FALSE;

		source = node->data;

		if (!source || G_NODE_IS_ROOT (node))
			break;

		g_signal_emit (selector, signals[FILTER_SOURCE], 0, source, &hidden_by_filter);

		if (hidden_by_filter)
			return TRUE;

		if (data.show_toggles && source_selector_source_is_enabled_and_selected (source, extension_name)) {
			hidden = FALSE;
			break;
		}

		hidden = hidden || g_hash_table_contains (selector->priv->hidden_groups, e_source_get_uid (source));

		node = node->parent;
	}

	if (!hidden)
		return FALSE;

	/* If any source in this subtree/group is enabled and selected,
	   then the group cannot be hidden */

	node = main_node;
	if (node->parent && !G_NODE_IS_ROOT (node->parent)) {
		node = node->parent;
	}

	data.extension_name = extension_name;
	data.any_selected = FALSE;

	g_node_traverse (node, G_IN_ORDER, G_TRAVERSE_ALL, -1, source_selector_lookup_selected_cb, &data);

	return !data.any_selected;
}

static gboolean
source_selector_traverse (GNode *node,
                          ESourceSelector *selector)
{
	ESource *source;
	GHashTable *source_index;
	GtkTreeRowReference *reference = NULL;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;

	/* Skip the root node. */
	if (G_NODE_IS_ROOT (node))
		return FALSE;

	if (source_selector_node_is_hidden (selector, node))
		return FALSE;

	source_index = selector->priv->source_index;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (selector));

	if (node->parent != NULL && node->parent->data != NULL)
		reference = g_hash_table_lookup (
			source_index, node->parent->data);

	if (gtk_tree_row_reference_valid (reference)) {
		GtkTreeIter parent;

		path = gtk_tree_row_reference_get_path (reference);
		gtk_tree_model_get_iter (model, &parent, path);
		gtk_tree_path_free (path);

		gtk_tree_store_append (GTK_TREE_STORE (model), &iter, &parent);
	} else
		gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);

	source = E_SOURCE (node->data);

	path = gtk_tree_model_get_path (model, &iter);
	reference = gtk_tree_row_reference_new (model, path);
	g_hash_table_insert (source_index, g_object_ref (source), reference);
	gtk_tree_path_free (path);

	e_source_selector_update_row (selector, source);

	return FALSE;
}

static void
source_selector_save_expanded (GtkTreeView *tree_view,
                               GtkTreePath *path,
                               GQueue *queue)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	ESource *source;

	model = gtk_tree_view_get_model (tree_view);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, COLUMN_SOURCE, &source, -1);
	if (source)
		g_queue_push_tail (queue, source);
}

typedef struct _SavedChildData {
	gchar *display_name;
	gchar *child_data;
	gboolean selected;
} SavedChildData;

static SavedChildData *
saved_child_data_new (gchar *display_name, /* (transfer full) */
		      gchar *child_data, /* (transfer full) */
		      gboolean selected)
{
	SavedChildData *scd;

	scd = g_slice_new (SavedChildData);
	scd->display_name = display_name;
	scd->child_data = child_data;
	scd->selected = selected;

	return scd;
}

static void
saved_child_data_free (gpointer ptr)
{
	SavedChildData *scd = ptr;

	if (scd) {
		g_free (scd->display_name);
		g_free (scd->child_data);
		g_slice_free (SavedChildData, scd);
	}
}

typedef struct _SavedStatus
{
	gboolean is_busy;
	gchar *tooltip;
	GSList *children; /* SavedChildData * */
} SavedStatusData;

static void
saved_status_data_free (gpointer ptr)
{
	SavedStatusData *data = ptr;

	if (data) {
		g_free (data->tooltip);
		g_slist_free_full (data->children, saved_child_data_free);
		g_slice_free (SavedStatusData, data);
	}
}

static GHashTable *
source_selector_save_sources_status (ESourceSelector *selector)
{
	GHashTable *status;
	GHashTableIter iter;
	gpointer key, value;

	status = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, saved_status_data_free);

	g_hash_table_iter_init (&iter, selector->priv->source_index);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		ESource *source = key;
		GtkTreeRowReference *reference = value;
		GtkTreeModel *model;
		GtkTreePath *path;
		GtkTreeIter tree_iter;

		if (reference && gtk_tree_row_reference_valid (reference)) {
			SavedStatusData *data;
			GtkTreeIter child;

			model = gtk_tree_row_reference_get_model (reference);
			path = gtk_tree_row_reference_get_path (reference);
			gtk_tree_model_get_iter (model, &tree_iter, path);

			data = g_slice_new0 (SavedStatusData);

			gtk_tree_model_get (
				model, &tree_iter,
				COLUMN_IS_BUSY, &data->is_busy,
				COLUMN_TOOLTIP, &data->tooltip,
				-1);

			if (data->is_busy)
				source_selector_dec_busy_sources (selector);

			gtk_tree_path_free (path);

			if (gtk_tree_model_iter_children (model, &child, &tree_iter)) {
				gchar *child_data = NULL;

				gtk_tree_model_get (model, &child, COLUMN_CHILD_DATA, &child_data, -1);

				if (child_data) {
					GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (selector));
					gchar *display_name = NULL;

					gtk_tree_model_get (model, &child, COLUMN_NAME, &display_name, -1);

					data->children = g_slist_prepend (data->children,
						saved_child_data_new (display_name, child_data,
							gtk_tree_selection_iter_is_selected (selection, &child)));

					display_name = NULL;
					child_data = NULL;

					while (gtk_tree_model_iter_next (model, &child)) {
						gtk_tree_model_get (model, &child,
							COLUMN_NAME, &display_name,
							COLUMN_CHILD_DATA, &child_data,
							-1);

						if (child_data) {
							data->children = g_slist_prepend (data->children,
								saved_child_data_new (display_name, child_data,
									gtk_tree_selection_iter_is_selected (selection, &child)));

							display_name = NULL;
							child_data = NULL;
						} else {
							g_clear_pointer (&display_name, g_free);
						}
					}

					data->children = g_slist_reverse (data->children);
				}
			}

			g_hash_table_insert (status, g_strdup (e_source_get_uid (source)), data);
		}
	}

	return status;
}

static void
source_selector_load_sources_status (ESourceSelector *selector,
				     GHashTable *status)
{
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init (&iter, selector->priv->source_index);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		ESource *source = key;
		GtkTreeRowReference *reference = value;
		GtkTreeModel *model;
		GtkTreePath *path;
		GtkTreeIter tree_iter;

		if (reference && gtk_tree_row_reference_valid (reference)) {
			SavedStatusData *data;
			GtkTreeStore *tree_store;
			GSList *link;

			model = gtk_tree_row_reference_get_model (reference);
			path = gtk_tree_row_reference_get_path (reference);
			gtk_tree_model_get_iter (model, &tree_iter, path);

			data = g_hash_table_lookup (status, e_source_get_uid (source));
			if (!data) {
				gtk_tree_path_free (path);
				continue;
			}

			tree_store = GTK_TREE_STORE (model);

			gtk_tree_store_set (
				tree_store, &tree_iter,
				COLUMN_IS_BUSY, data->is_busy,
				COLUMN_TOOLTIP, data->tooltip,
				-1);

			if (data->is_busy)
				source_selector_inc_busy_sources (selector);

			for (link = data->children; link; link = g_slist_next (link)) {
				SavedChildData *sdc = link->data;
				GtkTreeIter child;

				gtk_tree_store_append (tree_store, &child, &tree_iter);
				gtk_tree_store_set (tree_store, &child,
					COLUMN_NAME, sdc->display_name,
					COLUMN_CHILD_DATA, sdc->child_data,
					COLUMN_WEIGHT, PANGO_WEIGHT_NORMAL,
					-1);

				if (sdc->selected) {
					GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (selector));

					gtk_tree_view_expand_row (GTK_TREE_VIEW (selector), path, TRUE);
					gtk_tree_selection_select_iter (selection, &child);
				}
			}

			gtk_tree_path_free (path);
		}
	}
}

static void
source_selector_sort_groups (ESourceSelector *selector,
			     GNode *root)
{
	GHashTable *groups; /* gchar *uid, GUINT index into node_sources */
	GPtrArray *node_sources; /* GNode * as stored in the root first sub-level */
	ESource *source;
	GNode *node;
	GSList *link;
	guint ii;

	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));
	g_return_if_fail (G_NODE_IS_ROOT (root));

	if (!selector->priv->groups_order ||
	    !g_node_n_children (root))
		return;

	groups = g_hash_table_new (g_str_hash, g_str_equal);
	node_sources = g_ptr_array_sized_new (g_node_n_children (root));

	node = g_node_first_child (root);
	while (node) {
		GNode *next_node = g_node_next_sibling (node);

		source = node->data;

		if (source) {
			g_node_unlink (node);

			g_hash_table_insert (groups, (gpointer) e_source_get_uid (source), GUINT_TO_POINTER (node_sources->len));
			g_ptr_array_add (node_sources, node);
		}

		node = next_node;
	}

	/* First add known nodes as defined by the user... */
	for (link = selector->priv->groups_order; link; link = g_slist_next (link)) {
		const gchar *uid = link->data;

		if (!uid || !g_hash_table_contains (groups, uid))
			continue;

		ii = GPOINTER_TO_UINT (g_hash_table_lookup (groups, uid));
		g_warn_if_fail (ii < node_sources->len);

		node = node_sources->pdata[ii];
		node_sources->pdata[ii] = NULL;

		if (node)
			g_node_append (root, node);
	}

	/* ... then add all unknown (new) sources in the order
	   as they were in the passed-in tree */
	for (ii = 0; ii < node_sources->len; ii++) {
		node = node_sources->pdata[ii];

		if (node)
			g_node_append (root, node);
	}

	g_ptr_array_unref (node_sources);
	g_hash_table_destroy (groups);
}

static void
source_selector_build_model (ESourceSelector *selector)
{
	ESourceRegistry *registry;
	GQueue queue = G_QUEUE_INIT;
	GHashTable *source_index;
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GHashTable *saved_status;
	ESource *selected;
	const gchar *extension_name;
	GNode *root;

	tree_view = GTK_TREE_VIEW (selector);

	registry = e_source_selector_get_registry (selector);
	extension_name = e_source_selector_get_extension_name (selector);

	/* Make sure we have what we need to build the model, since
	 * this can get called early in the initialization phase. */
	if (registry == NULL || extension_name == NULL)
		return;

	source_index = selector->priv->source_index;
	selected = e_source_selector_ref_primary_selection (selector);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (selector));
	saved_status = source_selector_save_sources_status (selector);

	/* Signal is blocked to avoid "primary-selection-changed" signal
	 * on model clear. */
	g_signal_handlers_block_matched (
		selection, G_SIGNAL_MATCH_FUNC,
		0, 0, NULL, selection_changed_callback, NULL);

	/* Save expanded sources to restore later. */
	gtk_tree_view_map_expanded_rows (
		tree_view, (GtkTreeViewMappingFunc)
		source_selector_save_expanded, &queue);

	model = gtk_tree_view_get_model (tree_view);
	gtk_tree_store_clear (GTK_TREE_STORE (model));

	g_hash_table_remove_all (source_index);

	root = e_source_registry_build_display_tree (registry, extension_name);

	source_selector_sort_groups (selector, root);

	g_node_traverse (
		root, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
		(GNodeTraverseFunc) source_selector_traverse,
		selector);

	e_source_registry_free_display_tree (root);

	/* Restore previously expanded sources. */
	while (!g_queue_is_empty (&queue)) {
		GtkTreeRowReference *reference;
		ESource *source;

		source = g_queue_pop_head (&queue);
		reference = g_hash_table_lookup (source_index, source);

		if (gtk_tree_row_reference_valid (reference)) {
			GtkTreePath *path;

			path = gtk_tree_row_reference_get_path (reference);
			gtk_tree_view_expand_to_path (tree_view, path);
			gtk_tree_path_free (path);
		}

		g_object_unref (source);
	}

	/* Restore the primary selection. */
	if (selected != NULL) {
		e_source_selector_set_primary_selection (selector, selected);
		g_object_unref (selected);
	}

	/* If the first succeeded, then there is no selection change,
	 * thus no need for notification; notify about the change in
	 * any other cases. */
	g_signal_handlers_unblock_matched (
		selection, G_SIGNAL_MATCH_FUNC,
		0, 0, NULL, selection_changed_callback, NULL);

	/* Make sure we have a primary selection.  If not, pick one. */
	selected = e_source_selector_ref_primary_selection (selector);
	if (selected == NULL) {
		selected = e_source_registry_ref_default_for_extension_name (
			registry, extension_name);
	}
	if (selected != NULL) {
		e_source_selector_set_primary_selection (selector, selected);
		g_object_unref (selected);
	}

	source_selector_load_sources_status (selector, saved_status);
	g_hash_table_destroy (saved_status);
}

static void
source_selector_expand_to_source (ESourceSelector *selector,
                                  ESource *source)
{
	GHashTable *source_index;
	GtkTreeRowReference *reference;
	GtkTreePath *path;

	source_index = selector->priv->source_index;
	reference = g_hash_table_lookup (source_index, source);

	/* If the ESource is not in our tree model then return silently. */
	if (reference == NULL)
		return;

	/* If we do have a row reference, it should be valid. */
	g_return_if_fail (gtk_tree_row_reference_valid (reference));

	/* Expand the tree view to the path containing the ESource */
	path = gtk_tree_row_reference_get_path (reference);
	gtk_tree_view_expand_to_path (GTK_TREE_VIEW (selector), path);
	gtk_tree_path_free (path);
}

static void
source_selector_source_added_cb (ESourceRegistry *registry,
                                 ESource *source,
                                 ESourceSelector *selector)
{
	const gchar *extension_name;

	extension_name = e_source_selector_get_extension_name (selector);

	if (extension_name == NULL)
		return;

	if (!e_source_has_extension (source, extension_name))
		return;

	source_selector_build_model (selector);

	source_selector_expand_to_source (selector, source);

	if (e_source_selector_source_is_selected (selector, source))
		g_signal_emit (selector, signals[SOURCE_SELECTED], 0, source);
}

static void
source_selector_source_changed_cb (ESourceRegistry *registry,
                                   ESource *source,
                                   ESourceSelector *selector)
{
	const gchar *extension_name;

	extension_name = e_source_selector_get_extension_name (selector);

	if (extension_name == NULL)
		return;

	if (!e_source_has_extension (source, extension_name))
		return;

	source_selector_cancel_write (selector, source);

	e_source_selector_update_row (selector, source);

	if (e_source_selector_source_is_selected (selector, source))
		g_signal_emit (selector, signals[SOURCE_SELECTED], 0, source);
	else
		g_signal_emit (selector, signals[SOURCE_UNSELECTED], 0, source);
}

static void
source_selector_source_removed_cb (ESourceRegistry *registry,
                                   ESource *source,
                                   ESourceSelector *selector)
{
	const gchar *extension_name;

	extension_name = e_source_selector_get_extension_name (selector);

	if (extension_name == NULL)
		return;

	if (!e_source_has_extension (source, extension_name))
		return;

	if (e_source_selector_get_source_is_busy (selector, source))
		source_selector_dec_busy_sources (selector);

	/* Always notify about deselect, regardless whether it is really selected,
	   because listeners won't always know the source is gone otherwise. */
	g_signal_emit (selector, signals[SOURCE_UNSELECTED], 0, source);

	source_selector_build_model (selector);
}

static void
source_selector_source_enabled_cb (ESourceRegistry *registry,
                                   ESource *source,
                                   ESourceSelector *selector)
{
	const gchar *extension_name;

	extension_name = e_source_selector_get_extension_name (selector);

	if (extension_name == NULL)
		return;

	if (!e_source_has_extension (source, extension_name))
		return;

	source_selector_build_model (selector);

	source_selector_expand_to_source (selector, source);

	if (e_source_selector_source_is_selected (selector, source))
		g_signal_emit (selector, signals[SOURCE_SELECTED], 0, source);
}

static void
source_selector_source_disabled_cb (ESourceRegistry *registry,
                                    ESource *source,
                                    ESourceSelector *selector)
{
	const gchar *extension_name;

	extension_name = e_source_selector_get_extension_name (selector);

	if (extension_name == NULL)
		return;

	if (!e_source_has_extension (source, extension_name))
		return;

	if (e_source_selector_get_source_is_busy (selector, source))
		source_selector_dec_busy_sources (selector);

	/* Always notify about deselect, regardless whether it is really selected,
	   because listeners won't always know the source is gone otherwise. */
	g_signal_emit (selector, signals[SOURCE_UNSELECTED], 0, source);

	source_selector_build_model (selector);
}

static gboolean
same_source_name_exists (ESourceSelector *selector,
                         const gchar *display_name)
{
	GHashTable *source_index;
	GHashTableIter iter;
	gpointer key;

	source_index = selector->priv->source_index;
	g_hash_table_iter_init (&iter, source_index);

	while (g_hash_table_iter_next (&iter, &key, NULL)) {
		ESource *source = E_SOURCE (key);
		const gchar *source_name;

		source_name = e_source_get_display_name (source);
		if (g_strcmp0 (display_name, source_name) == 0)
			return TRUE;
	}

	return FALSE;
}

static gboolean
selection_func (GtkTreeSelection *selection,
                GtkTreeModel *model,
                GtkTreePath *path,
                gboolean path_currently_selected,
                ESourceSelector *selector)
{
	ESource *source;
	GtkTreeIter iter;
	gchar *child_data = NULL;
	const gchar *extension_name;

	if (selector->priv->toggled_last) {
		selector->priv->toggled_last = FALSE;
		return FALSE;
	}

	if (path_currently_selected)
		return TRUE;

	if (!gtk_tree_model_get_iter (model, &iter, path))
		return FALSE;

	extension_name = e_source_selector_get_extension_name (selector);
	gtk_tree_model_get (model, &iter,
		COLUMN_SOURCE, &source,
		COLUMN_CHILD_DATA, &child_data,
		-1);

	if (!source || !e_source_has_extension (source, extension_name)) {
		g_clear_object (&source);
		g_free (child_data);
		return child_data != NULL;
	}

	clear_saved_primary_selection (selector);

	g_object_unref (source);

	return TRUE;
}

static void
text_cell_edited_cb (ESourceSelector *selector,
                     const gchar *path_string,
                     const gchar *new_name)
{
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	ESource *source;

	if (new_name == NULL || *new_name == '\0')
		return;

	if (same_source_name_exists (selector, new_name))
		return;

	tree_view = GTK_TREE_VIEW (selector);
	model = gtk_tree_view_get_model (tree_view);

	path = gtk_tree_path_new_from_string (path_string);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, COLUMN_SOURCE, &source, -1);
	gtk_tree_path_free (path);

	if (!source)
		return;

	e_source_set_display_name (source, new_name);

	e_source_selector_queue_write (selector, source);

	g_object_unref (source);
}

static void
cell_toggled_callback (GtkCellRendererToggle *renderer,
                       const gchar *path_string,
                       ESourceSelector *selector)
{
	ESource *source;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (selector));
	path = gtk_tree_path_new_from_string (path_string);

	if (!gtk_tree_model_get_iter (model, &iter, path)) {
		gtk_tree_path_free (path);
		return;
	}

	gtk_tree_model_get (model, &iter, COLUMN_SOURCE, &source, -1);

	gtk_tree_path_free (path);

	if (!source)
		return;

	if (e_source_selector_source_is_selected (selector, source))
		e_source_selector_unselect_source (selector, source);
	else
		e_source_selector_select_source (selector, source);

	selector->priv->toggled_last = TRUE;

	g_object_unref (source);
}

static void
selection_changed_callback (GtkTreeSelection *selection,
                            ESourceSelector *selector)
{
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		ESource *source = NULL;

		gtk_tree_model_get (model, &iter, COLUMN_SOURCE, &source, -1);

		if (source) {
			g_signal_emit (selector, signals[PRIMARY_SELECTION_CHANGED], 0);
			g_object_notify (G_OBJECT (selector), "primary-selection");
		} else {
			gchar *child_data = NULL;

			gtk_tree_model_get (model, &iter, COLUMN_CHILD_DATA, &child_data, -1);

			if (child_data) {
				GtkTreeIter parent;

				if (gtk_tree_model_iter_parent (model, &parent, &iter)) {
					gtk_tree_model_get (model, &parent, COLUMN_SOURCE, &source, -1);

					if (source) {
						clear_saved_primary_selection (selector);
						g_signal_emit (selector, signals[PRIMARY_SELECTION_CHANGED], 0);
						g_object_notify (G_OBJECT (selector), "primary-selection");

						g_signal_emit (selector, signals[SOURCE_CHILD_SELECTED], 0, source, child_data);
					}
				}

				g_free (child_data);
			}
		}

		g_clear_object (&source);
	}
}

static void
source_selector_set_extension_name (ESourceSelector *selector,
                                    const gchar *extension_name)
{
	g_return_if_fail (extension_name != NULL);
	g_return_if_fail (selector->priv->extension_name == NULL);

	selector->priv->extension_name = g_strdup (extension_name);
}

static void
source_selector_set_registry (ESourceSelector *selector,
                              ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (selector->priv->registry == NULL);

	selector->priv->registry = g_object_ref (registry);
}

static void
source_selector_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EXTENSION_NAME:
			source_selector_set_extension_name (
				E_SOURCE_SELECTOR (object),
				g_value_get_string (value));
			return;

		case PROP_PRIMARY_SELECTION:
			e_source_selector_set_primary_selection (
				E_SOURCE_SELECTOR (object),
				g_value_get_object (value));
			return;

		case PROP_REGISTRY:
			source_selector_set_registry (
				E_SOURCE_SELECTOR (object),
				g_value_get_object (value));
			return;

		case PROP_SHOW_COLORS:
			e_source_selector_set_show_colors (
				E_SOURCE_SELECTOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHOW_ICONS:
			e_source_selector_set_show_icons (
				E_SOURCE_SELECTOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHOW_TOGGLES:
			e_source_selector_set_show_toggles (
				E_SOURCE_SELECTOR (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
source_selector_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EXTENSION_NAME:
			g_value_set_string (
				value,
				e_source_selector_get_extension_name (
				E_SOURCE_SELECTOR (object)));
			return;

		case PROP_PRIMARY_SELECTION:
			g_value_take_object (
				value,
				e_source_selector_ref_primary_selection (
				E_SOURCE_SELECTOR (object)));
			return;

		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_source_selector_get_registry (
				E_SOURCE_SELECTOR (object)));
			return;

		case PROP_SHOW_COLORS:
			g_value_set_boolean (
				value,
				e_source_selector_get_show_colors (
				E_SOURCE_SELECTOR (object)));
			return;

		case PROP_SHOW_ICONS:
			g_value_set_boolean (
				value,
				e_source_selector_get_show_icons (
				E_SOURCE_SELECTOR (object)));
			return;

		case PROP_SHOW_TOGGLES:
			g_value_set_boolean (
				value,
				e_source_selector_get_show_toggles (
				E_SOURCE_SELECTOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
source_selector_dispose (GObject *object)
{
	ESourceSelector *self = E_SOURCE_SELECTOR (object);

	if (self->priv->update_busy_renderer_id) {
		g_source_remove (self->priv->update_busy_renderer_id);
		self->priv->update_busy_renderer_id = 0;
	}

	if (self->priv->source_added_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->registry,
			self->priv->source_added_handler_id);
		self->priv->source_added_handler_id = 0;
	}

	if (self->priv->source_changed_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->registry,
			self->priv->source_changed_handler_id);
		self->priv->source_changed_handler_id = 0;
	}

	if (self->priv->source_removed_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->registry,
			self->priv->source_removed_handler_id);
		self->priv->source_removed_handler_id = 0;
	}

	if (self->priv->source_enabled_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->registry,
			self->priv->source_enabled_handler_id);
		self->priv->source_enabled_handler_id = 0;
	}

	if (self->priv->source_disabled_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->registry,
			self->priv->source_disabled_handler_id);
		self->priv->source_disabled_handler_id = 0;
	}

	g_clear_object (&self->priv->registry);
	g_clear_object (&self->priv->busy_renderer);

	g_hash_table_remove_all (self->priv->source_index);
	g_hash_table_remove_all (self->priv->pending_writes);
	g_hash_table_remove_all (self->priv->hidden_groups);

	g_slist_free_full (self->priv->groups_order, g_free);
	self->priv->groups_order = NULL;

	clear_saved_primary_selection (E_SOURCE_SELECTOR (object));

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_source_selector_parent_class)->dispose (object);
}

static void
source_selector_finalize (GObject *object)
{
	ESourceSelector *self = E_SOURCE_SELECTOR (object);

	g_hash_table_destroy (self->priv->source_index);
	g_hash_table_destroy (self->priv->pending_writes);
	g_hash_table_destroy (self->priv->hidden_groups);

	g_free (self->priv->extension_name);

	if (self->priv->main_context)
		g_main_context_unref (self->priv->main_context);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_source_selector_parent_class)->finalize (object);
}

static void
source_selector_constructed (GObject *object)
{
	ESourceRegistry *registry;
	ESourceSelector *selector;
	gulong handler_id;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_source_selector_parent_class)->constructed (object);

	selector = E_SOURCE_SELECTOR (object);
	registry = e_source_selector_get_registry (selector);

	handler_id = g_signal_connect (
		registry, "source-added",
		G_CALLBACK (source_selector_source_added_cb), selector);
	selector->priv->source_added_handler_id = handler_id;

	handler_id = g_signal_connect (
		registry, "source-changed",
		G_CALLBACK (source_selector_source_changed_cb), selector);
	selector->priv->source_changed_handler_id = handler_id;

	handler_id = g_signal_connect (
		registry, "source-removed",
		G_CALLBACK (source_selector_source_removed_cb), selector);
	selector->priv->source_removed_handler_id = handler_id;

	handler_id = g_signal_connect (
		registry, "source-enabled",
		G_CALLBACK (source_selector_source_enabled_cb), selector);
	selector->priv->source_enabled_handler_id = handler_id;

	handler_id = g_signal_connect (
		registry, "source-disabled",
		G_CALLBACK (source_selector_source_disabled_cb), selector);
	selector->priv->source_disabled_handler_id = handler_id;

	source_selector_build_model (selector);

	gtk_tree_view_expand_all (GTK_TREE_VIEW (selector));
}

static gboolean
source_selector_button_press_event (GtkWidget *widget,
                                    GdkEventButton *event)
{
	ESourceSelector *selector;
	GtkWidgetClass *widget_class;
	GtkTreePath *path = NULL;
	ESource *source = NULL;
	ESource *primary;
	gboolean right_click = FALSE;
	gboolean triple_click = FALSE;
	gboolean row_exists;
	gboolean res = FALSE;

	selector = E_SOURCE_SELECTOR (widget);

	selector->priv->toggled_last = FALSE;

	/* Triple-clicking a source selects it exclusively. */

	if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
		right_click = TRUE;
	else if (event->button == 1 && event->type == GDK_3BUTTON_PRESS)
		triple_click = TRUE;
	else
		goto chainup;

	row_exists = gtk_tree_view_get_path_at_pos (
		GTK_TREE_VIEW (widget), event->x, event->y,
		&path, NULL, NULL, NULL);

	/* Get the source/group */
	if (row_exists) {
		GtkTreeModel *model;
		GtkTreeIter iter;

		model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));

		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_model_get (model, &iter, COLUMN_SOURCE, &source, -1);

		if (!source) {
			gchar *child_data = NULL;

			gtk_tree_model_get (model, &iter, COLUMN_CHILD_DATA, &child_data, -1);

			if (child_data) {
				GtkTreeIter parent;

				if (gtk_tree_model_iter_parent (model, &parent, &iter))
					gtk_tree_model_get (model, &parent, COLUMN_SOURCE, &source, -1);

				g_free (child_data);
			}
		}
	}

	if (path != NULL)
		gtk_tree_path_free (path);

	if (source == NULL)
		goto chainup;

	primary = e_source_selector_ref_primary_selection (selector);
	if (source != primary)
		e_source_selector_set_primary_selection (selector, source);
	if (primary != NULL)
		g_object_unref (primary);

	if (right_click)
		g_signal_emit (
			widget, signals[POPUP_EVENT], 0, source, event, &res);

	if (triple_click) {
		e_source_selector_select_exclusive (selector, source);
		res = TRUE;
	}

	g_object_unref (source);

	return res;

chainup:

	/* Chain up to parent's button_press_event() method. */
	widget_class = GTK_WIDGET_CLASS (e_source_selector_parent_class);
	return widget_class->button_press_event (widget, event);
}

static void
source_selector_drag_leave (GtkWidget *widget,
                            GdkDragContext *context,
                            guint time_)
{
	GtkTreeView *tree_view;
	GtkTreeViewDropPosition pos;

	tree_view = GTK_TREE_VIEW (widget);
	pos = GTK_TREE_VIEW_DROP_BEFORE;

	gtk_tree_view_set_drag_dest_row (tree_view, NULL, pos);
}

static gboolean
source_selector_drag_motion (GtkWidget *widget,
                             GdkDragContext *context,
                             gint x,
                             gint y,
                             guint time_)
{
	ESource *source = NULL;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreePath *path = NULL;
	GtkTreeIter iter;
	GtkTreeViewDropPosition pos;
	GdkDragAction action = 0;

	tree_view = GTK_TREE_VIEW (widget);
	model = gtk_tree_view_get_model (tree_view);

	if (!gtk_tree_view_get_dest_row_at_pos (tree_view, x, y, &path, NULL))
		goto exit;

	if (!gtk_tree_model_get_iter (model, &iter, path))
		goto exit;

	gtk_tree_model_get (model, &iter, COLUMN_SOURCE, &source, -1);

	if (!source || !e_source_get_writable (source) ||
	    e_util_guess_source_is_readonly (source))
		goto exit;

	pos = GTK_TREE_VIEW_DROP_INTO_OR_BEFORE;
	gtk_tree_view_set_drag_dest_row (tree_view, path, pos);

	if (gdk_drag_context_get_actions (context) & GDK_ACTION_MOVE)
		action = GDK_ACTION_MOVE;
	else
		action = gdk_drag_context_get_suggested_action (context);

exit:
	if (path != NULL)
		gtk_tree_path_free (path);

	if (source != NULL)
		g_object_unref (source);

	gdk_drag_status (context, action, time_);

	return TRUE;
}

static gboolean
source_selector_drag_drop (GtkWidget *widget,
                           GdkDragContext *context,
                           gint x,
                           gint y,
                           guint time_)
{
	ESource *source;
	ESourceSelector *selector;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	const gchar *extension_name;
	gboolean drop_zone;
	gboolean valid;

	tree_view = GTK_TREE_VIEW (widget);
	model = gtk_tree_view_get_model (tree_view);

	if (!gtk_tree_view_get_path_at_pos (
		tree_view, x, y, &path, NULL, NULL, NULL))
		return FALSE;

	valid = gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);
	g_return_val_if_fail (valid, FALSE);

	gtk_tree_model_get (model, &iter, COLUMN_SOURCE, &source, -1);

	if (!source)
		return FALSE;

	selector = E_SOURCE_SELECTOR (widget);
	extension_name = e_source_selector_get_extension_name (selector);
	drop_zone = e_source_has_extension (source, extension_name);

	g_object_unref (source);

	return drop_zone;
}

static void
source_selector_drag_data_received (GtkWidget *widget,
                                    GdkDragContext *context,
                                    gint x,
                                    gint y,
                                    GtkSelectionData *selection_data,
                                    guint info,
                                    guint time_)
{
	ESource *source = NULL;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreePath *path = NULL;
	GtkTreeIter iter;
	GdkDragAction action;
	gboolean delete;
	gboolean success = FALSE;

	tree_view = GTK_TREE_VIEW (widget);
	model = gtk_tree_view_get_model (tree_view);

	action = gdk_drag_context_get_selected_action (context);
	delete = (action == GDK_ACTION_MOVE);

	if (!gtk_tree_view_get_dest_row_at_pos (tree_view, x, y, &path, NULL))
		goto exit;

	if (!gtk_tree_model_get_iter (model, &iter, path))
		goto exit;

	gtk_tree_model_get (model, &iter, COLUMN_SOURCE, &source, -1);

	if (!source || !e_source_get_writable (source))
		goto exit;

	g_signal_emit (
		widget, signals[DATA_DROPPED], 0, selection_data,
		source, gdk_drag_context_get_selected_action (context),
		info, &success);

exit:
	if (path != NULL)
		gtk_tree_path_free (path);

	if (source != NULL)
		g_object_unref (source);

	gtk_drag_finish (context, success, delete, time_);
}

static gboolean
source_selector_popup_menu (GtkWidget *widget)
{
	ESourceSelector *selector;
	ESource *source;
	gboolean res = FALSE;

	selector = E_SOURCE_SELECTOR (widget);
	source = e_source_selector_ref_primary_selection (selector);
	g_signal_emit (selector, signals[POPUP_EVENT], 0, source, NULL, &res);

	if (source != NULL)
		g_object_unref (source);

	return res;
}

static gboolean
source_selector_test_collapse_row (GtkTreeView *tree_view,
                                   GtkTreeIter *iter,
                                   GtkTreePath *path)
{
	ESourceSelector *self = E_SOURCE_SELECTOR (tree_view);
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter child_iter;

	/* Clear this because something else has been clicked on now */
	self->priv->toggled_last = FALSE;

	if (self->priv->saved_primary_selection)
		return FALSE;

	selection = gtk_tree_view_get_selection (tree_view);

	if (!gtk_tree_selection_get_selected (selection, &model, &child_iter))
		return FALSE;

	if (gtk_tree_store_is_ancestor (GTK_TREE_STORE (model), iter, &child_iter)) {
		GtkTreeRowReference *reference;
		GtkTreePath *child_path;

		child_path = gtk_tree_model_get_path (model, &child_iter);
		reference = gtk_tree_row_reference_new (model, child_path);
		self->priv->saved_primary_selection = reference;
		gtk_tree_path_free (child_path);
	}

	return FALSE;
}

static void
source_selector_row_expanded (GtkTreeView *tree_view,
                              GtkTreeIter *iter,
                              GtkTreePath *path)
{
	ESourceSelector *self = E_SOURCE_SELECTOR (tree_view);
	GtkTreeModel *model;
	GtkTreePath *child_path;
	GtkTreeIter child_iter;

	if (!self->priv->saved_primary_selection)
		return;

	model = gtk_tree_view_get_model (tree_view);

	child_path = gtk_tree_row_reference_get_path (self->priv->saved_primary_selection);
	gtk_tree_model_get_iter (model, &child_iter, child_path);

	if (gtk_tree_store_is_ancestor (GTK_TREE_STORE (model), iter, &child_iter)) {
		GtkTreeSelection *selection;

		selection = gtk_tree_view_get_selection (tree_view);
		gtk_tree_selection_select_iter (selection, &child_iter);

		clear_saved_primary_selection (E_SOURCE_SELECTOR (tree_view));
	}

	gtk_tree_path_free (child_path);
}

static gboolean
source_selector_get_source_selected (ESourceSelector *selector,
                                     ESource *source)
{
	ESourceSelectable *extension;
	const gchar *extension_name;
	gboolean selected = TRUE;

	extension_name = e_source_selector_get_extension_name (selector);

	if (!e_source_has_extension (source, extension_name))
		return FALSE;

	extension = e_source_get_extension (source, extension_name);

	if (E_IS_SOURCE_SELECTABLE (extension))
		selected = e_source_selectable_get_selected (extension);

	return selected;
}

static gboolean
source_selector_set_source_selected (ESourceSelector *selector,
                                     ESource *source,
                                     gboolean selected)
{
	ESourceSelectable *extension;
	const gchar *extension_name;

	extension_name = e_source_selector_get_extension_name (selector);

	if (!e_source_has_extension (source, extension_name))
		return FALSE;

	extension = e_source_get_extension (source, extension_name);

	if (!E_IS_SOURCE_SELECTABLE (extension))
		return FALSE;

	if (selected != e_source_selectable_get_selected (extension)) {
		e_source_selectable_set_selected (extension, selected);
		e_source_selector_queue_write (selector, source);

		return TRUE;
	}

	return FALSE;
}

static gboolean
ess_bool_accumulator (GSignalInvocationHint *ihint,
                      GValue *out,
                      const GValue *in,
                      gpointer data)
{
	gboolean v_boolean;

	v_boolean = g_value_get_boolean (in);
	g_value_set_boolean (out, v_boolean);

	return !v_boolean;
}

static void
e_source_selector_class_init (ESourceSelectorClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkTreeViewClass *tree_view_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = source_selector_set_property;
	object_class->get_property = source_selector_get_property;
	object_class->dispose = source_selector_dispose;
	object_class->finalize = source_selector_finalize;
	object_class->constructed = source_selector_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->button_press_event = source_selector_button_press_event;
	widget_class->drag_leave = source_selector_drag_leave;
	widget_class->drag_motion = source_selector_drag_motion;
	widget_class->drag_drop = source_selector_drag_drop;
	widget_class->drag_data_received = source_selector_drag_data_received;
	widget_class->popup_menu = source_selector_popup_menu;

	tree_view_class = GTK_TREE_VIEW_CLASS (class);
	tree_view_class->test_collapse_row = source_selector_test_collapse_row;
	tree_view_class->row_expanded = source_selector_row_expanded;

	class->get_source_selected = source_selector_get_source_selected;
	class->set_source_selected = source_selector_set_source_selected;

	g_object_class_install_property (
		object_class,
		PROP_EXTENSION_NAME,
		g_param_spec_string (
			"extension-name",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_PRIMARY_SELECTION,
		g_param_spec_object (
			"primary-selection",
			NULL,
			NULL,
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_REGISTRY,
		g_param_spec_object (
			"registry",
			NULL,
			NULL,
			E_TYPE_SOURCE_REGISTRY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_COLORS,
		g_param_spec_boolean (
			"show-colors",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_ICONS,
		g_param_spec_boolean (
			"show-icons",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_TOGGLES,
		g_param_spec_boolean (
			"show-toggles",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	signals[SELECTION_CHANGED] = g_signal_new (
		"selection-changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ESourceSelectorClass, selection_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	/* XXX Consider this signal deprecated.  Connect
	 *     to "notify::primary-selection" instead. */
	signals[PRIMARY_SELECTION_CHANGED] = g_signal_new (
		"primary-selection-changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ESourceSelectorClass, primary_selection_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[POPUP_EVENT] = g_signal_new (
		"popup-event",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ESourceSelectorClass, popup_event),
		ess_bool_accumulator, NULL, NULL,
		G_TYPE_BOOLEAN, 2, G_TYPE_OBJECT,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	signals[DATA_DROPPED] = g_signal_new (
		"data-dropped",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ESourceSelectorClass, data_dropped),
		NULL, NULL, NULL,
		G_TYPE_BOOLEAN, 4,
		GTK_TYPE_SELECTION_DATA | G_SIGNAL_TYPE_STATIC_SCOPE,
		E_TYPE_SOURCE,
		GDK_TYPE_DRAG_ACTION,
		G_TYPE_UINT);

	signals[SOURCE_SELECTED] = g_signal_new (
		"source-selected",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ESourceSelectorClass, source_selected),
		NULL, NULL, NULL,
		G_TYPE_NONE, 1, E_TYPE_SOURCE);

	signals[SOURCE_UNSELECTED] = g_signal_new (
		"source-unselected",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ESourceSelectorClass, source_unselected),
		NULL, NULL, NULL,
		G_TYPE_NONE, 1, E_TYPE_SOURCE);

	/* Return TRUE when the source should be hidden, FALSE when not. */
	signals[FILTER_SOURCE] = g_signal_new (
		"filter-source",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (ESourceSelectorClass, filter_source),
		NULL, NULL, NULL,
		G_TYPE_BOOLEAN, 1, E_TYPE_SOURCE);

	signals[SOURCE_CHILD_SELECTED] = g_signal_new (
		"source-child-selected",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ESourceSelectorClass, source_child_selected),
		NULL, NULL, NULL,
		G_TYPE_NONE, 2, E_TYPE_SOURCE, G_TYPE_STRING);
}

static void
e_source_selector_init (ESourceSelector *selector)
{
	GHashTable *pending_writes;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GtkTreeStore *tree_store;
	GtkTreeView *tree_view;

	pending_writes = g_hash_table_new_full (
		(GHashFunc) g_direct_hash,
		(GEqualFunc) g_direct_equal,
		(GDestroyNotify) g_object_unref,
		(GDestroyNotify) pending_writes_destroy_source);

	selector->priv = e_source_selector_get_instance_private (selector);

	selector->priv->pending_writes = pending_writes;
	selector->priv->hidden_groups = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	selector->priv->main_context = g_main_context_get_thread_default ();
	if (selector->priv->main_context != NULL)
		g_main_context_ref (selector->priv->main_context);

	tree_view = GTK_TREE_VIEW (selector);

	gtk_tree_view_set_search_column (tree_view, COLUMN_SOURCE);
	gtk_tree_view_set_enable_search (tree_view, TRUE);

	selector->priv->toggled_last = FALSE;
	selector->priv->show_colors = TRUE;
	selector->priv->show_toggles = TRUE;

	selector->priv->source_index = g_hash_table_new_full (
		(GHashFunc) e_source_hash,
		(GEqualFunc) e_source_equal,
		(GDestroyNotify) g_object_unref,
		(GDestroyNotify) gtk_tree_row_reference_free);

	tree_store = gtk_tree_store_new (
		NUM_COLUMNS,
		G_TYPE_STRING,		/* COLUMN_NAME */
		GDK_TYPE_RGBA,		/* COLUMN_COLOR */
		G_TYPE_BOOLEAN,		/* COLUMN_ACTIVE */
		G_TYPE_STRING,		/* COLUMN_ICON_NAME */
		G_TYPE_BOOLEAN,		/* COLUMN_SHOW_COLOR */
		G_TYPE_BOOLEAN,		/* COLUMN_SHOW_ICON */
		G_TYPE_BOOLEAN,		/* COLUMN_SHOW_TOGGLE */
		G_TYPE_INT,		/* COLUMN_WEIGHT */
		E_TYPE_SOURCE,		/* COLUMN_SOURCE */
		G_TYPE_STRING,		/* COLUMN_TOOLTIP */
		G_TYPE_BOOLEAN,		/* COLUMN_IS_BUSY */
		G_TYPE_UINT,		/* COLUMN_CONNECTION_STATUS */
		G_TYPE_UINT,		/* COLUMN_SORT_ORDER */
		G_TYPE_STRING);		/* COLUMN_CHILD_DATA */

	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (tree_store));

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_append_column (tree_view, column);

	renderer = e_cell_renderer_color_new ();
	g_object_set (
		G_OBJECT (renderer), "mode",
		GTK_CELL_RENDERER_MODE_ACTIVATABLE, NULL);
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "rgba", COLUMN_COLOR);
	gtk_tree_view_column_add_attribute (
		column, renderer, "visible", COLUMN_SHOW_COLOR);

	renderer = e_cell_renderer_safe_toggle_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "active", COLUMN_ACTIVE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "visible", COLUMN_SHOW_TOGGLE);
	g_signal_connect (
		renderer, "toggled",
		G_CALLBACK (cell_toggled_callback), selector);

	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (
		G_OBJECT (renderer),
		"stock-size", GTK_ICON_SIZE_MENU, NULL);
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "icon-name", COLUMN_ICON_NAME);
	gtk_tree_view_column_add_attribute (
		column, renderer, "visible", COLUMN_SHOW_ICONS);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (
		G_OBJECT (renderer),
		"ellipsize", PANGO_ELLIPSIZE_END, NULL);
	g_signal_connect_swapped (
		renderer, "edited",
		G_CALLBACK (text_cell_edited_cb), selector);
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_attributes (
		column, renderer,
		"text", COLUMN_NAME,
		"weight", COLUMN_WEIGHT,
		NULL);

	renderer = gtk_cell_renderer_spinner_new ();
	selector->priv->busy_renderer = g_object_ref (renderer);
	gtk_tree_view_column_pack_end (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes (
		column, renderer,
		"visible", COLUMN_IS_BUSY,
		"active", COLUMN_IS_BUSY,
		NULL);

	selection = gtk_tree_view_get_selection (tree_view);
	gtk_tree_selection_set_select_function (
		selection, (GtkTreeSelectionFunc)
		selection_func, selector, NULL);
	g_signal_connect_object (
		selection, "changed",
		G_CALLBACK (selection_changed_callback),
		G_OBJECT (selector), 0);

	gtk_tree_view_set_headers_visible (tree_view, FALSE);
	gtk_tree_view_set_tooltip_column (tree_view, COLUMN_TOOLTIP);
	gtk_widget_set_has_tooltip (GTK_WIDGET (tree_view), TRUE);
}

/**
 * e_source_selector_new:
 * @registry: an #ESourceRegistry
 * @extension_name: the name of an #ESource extension
 *
 * Displays a list of sources from @registry having an extension named
 * @extension_name.  The sources are grouped by backend or groupware
 * account, which are described by the parent source.
 *
 * Returns: a new #ESourceSelector
 **/
GtkWidget *
e_source_selector_new (ESourceRegistry *registry,
                       const gchar *extension_name)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);
	g_return_val_if_fail (extension_name != NULL, NULL);

	return g_object_new (
		E_TYPE_SOURCE_SELECTOR, "registry", registry,
		"extension-name", extension_name, NULL);
}

/**
 * e_source_selector_get_registry:
 * @selector: an #ESourceSelector
 *
 * Returns the #ESourceRegistry that @selector is getting sources from.
 *
 * Returns: an #ESourceRegistry
 *
 * Since: 3.6
 **/
ESourceRegistry *
e_source_selector_get_registry (ESourceSelector *selector)
{
	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), NULL);

	return selector->priv->registry;
}

/**
 * e_source_selector_get_extension_name:
 * @selector: an #ESourceSelector
 *
 * Returns the extension name used to filter which sources are displayed.
 *
 * Returns: the #ESource extension name
 *
 * Since: 3.6
 **/
const gchar *
e_source_selector_get_extension_name (ESourceSelector *selector)
{
	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), NULL);

	return selector->priv->extension_name;
}

/**
 * e_source_selector_get_show_colors:
 * @selector: an #ESourceSelector
 *
 * Returns whether colors are shown next to data sources.
 *
 * Returns: %TRUE if colors are being shown
 *
 * Since: 3.6
 **/
gboolean
e_source_selector_get_show_colors (ESourceSelector *selector)
{
	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), FALSE);

	return selector->priv->show_colors;
}

/**
 * e_source_selector_set_show_colors:
 * @selector: an #ESourceSelector
 * @show_colors: whether to show colors
 *
 * Sets whether to show colors next to data sources.
 *
 * Since: 3.6
 **/
void
e_source_selector_set_show_colors (ESourceSelector *selector,
                                   gboolean show_colors)
{
	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));

	if (show_colors == selector->priv->show_colors)
		return;

	selector->priv->show_colors = show_colors;

	g_object_notify (G_OBJECT (selector), "show-colors");

	source_selector_build_model (selector);
}

/**
 * e_source_selector_get_show_icons:
 * @selector: an #ESourceSelector
 *
 * Returns whether icons are shown next to data sources.
 *
 * Generally the icon shown will be based on the presence of a backend-based
 * extension, such as #ESourceAddressBook or #ESourceCalendar.  For #ESource
 * instances with no such extension, no icon is shown.
 *
 * Returns: %TRUE if icons are being shown
 *
 * Since: 3.12
 **/
gboolean
e_source_selector_get_show_icons (ESourceSelector *selector)
{
	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), FALSE);

	return selector->priv->show_icons;
}

/**
 * e_source_selector_set_show_icons:
 * @selector: an #ESourceSelector
 * @show_icons: whether to show icons
 *
 * Sets whether to show icons next to data sources.
 *
 * Generally the icon shown will be based on the presence of a backend-based
 * extension, such as #ESourceAddressBook or #ESourceCalendar.  For #ESource
 * instances with no such extension, no icon is shown.
 *
 * Since: 3.12
 **/
void
e_source_selector_set_show_icons (ESourceSelector *selector,
                                  gboolean show_icons)
{
	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));

	if (show_icons == selector->priv->show_icons)
		return;

	selector->priv->show_icons = show_icons;

	g_object_notify (G_OBJECT (selector), "show-icons");

	source_selector_build_model (selector);
}

/**
 * e_source_selector_get_show_toggles:
 * @selector: an #ESourceSelector
 *
 * Returns whether toggles are shown next to data sources.
 *
 * Returns: %TRUE if toggles are being shown
 *
 * Since: 3.6
 **/
gboolean
e_source_selector_get_show_toggles (ESourceSelector *selector)
{
	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), FALSE);

	return selector->priv->show_toggles;
}

/**
 * e_source_selector_set_show_toggles:
 * @selector: an #ESourceSelector
 * @show_toggles: whether to show toggles
 *
 * Sets whether to show toggles next to data sources.
 *
 * Since: 3.6
 **/
void
e_source_selector_set_show_toggles (ESourceSelector *selector,
                                   gboolean show_toggles)
{
	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));

	if (show_toggles == selector->priv->show_toggles)
		return;

	selector->priv->show_toggles = show_toggles;

	g_object_notify (G_OBJECT (selector), "show-toggles");

	source_selector_build_model (selector);
}

/* Helper for e_source_selector_get_selection() */
static gboolean
source_selector_check_selected (GtkTreeModel *model,
                                GtkTreePath *path,
                                GtkTreeIter *iter,
                                gpointer user_data)
{
	ESource *source;

	struct {
		ESourceSelector *selector;
		GQueue queue;
	} *closure = user_data;

	gtk_tree_model_get (model, iter, COLUMN_SOURCE, &source, -1);

	if (source && e_source_selector_source_is_selected (closure->selector, source))
		g_queue_push_tail (&closure->queue, g_object_ref (source));

	g_clear_object (&source);

	return FALSE;
}

/**
 * e_source_selector_get_selection:
 * @selector: an #ESourceSelector
 *
 * Returns a list of selected sources, i.e. those that were enabled through
 * the corresponding checkboxes in the tree.  The sources are ordered as they
 * appear in @selector.
 *
 * The sources returned in the list are referenced for thread-safety.
 * They must each be unreferenced with g_object_unref() when finished
 * with them.  Free the returned list itself with g_list_free().
 *
 * An easy way to free the list properly in one step is as follows:
 *
 * |[
 *   g_list_free_full (list, g_object_unref);
 * ]|
 *
 * Returns: a ordered list of selected sources
 **/
GList *
e_source_selector_get_selection (ESourceSelector *selector)
{
	struct {
		ESourceSelector *selector;
		GQueue queue;
	} closure;

	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), NULL);

	closure.selector = selector;
	g_queue_init (&closure.queue);

	gtk_tree_model_foreach (
		gtk_tree_view_get_model (GTK_TREE_VIEW (selector)),
		(GtkTreeModelForeachFunc) source_selector_check_selected,
		&closure);

	return g_queue_peek_head_link (&closure.queue);
}

struct CountData {
	ESourceSelector *selector;
	guint count;
	gboolean selected;
};

static gboolean
source_selector_count_sources (GtkTreeModel *model,
			       GtkTreePath *path,
			       GtkTreeIter *iter,
			       gpointer user_data)
{
	struct CountData *cd = user_data;
	ESource *source;

	gtk_tree_model_get (model, iter, COLUMN_SOURCE, &source, -1);

	if (source && e_source_has_extension (source, e_source_selector_get_extension_name (cd->selector))) {
		if (cd->selected) {
			if (e_source_selector_source_is_selected (cd->selector, source))
				cd->count++;
		} else {
			cd->count++;
		}
	}

	g_clear_object (&source);

	return FALSE;
}

/**
 * e_source_selector_count_total:
 * @selector: an #ESourceSelector
 *
 * Counts how many ESource-s are shown in the @selector.
 *
 * Returns: How many ESource-s are shown in the @selector.
 *
 * Since: 3.20
 **/
guint
e_source_selector_count_total (ESourceSelector *selector)
{
	struct CountData cd;

	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), 0);

	cd.selector = selector;
	cd.count = 0;
	cd.selected = FALSE;

	gtk_tree_model_foreach (
		gtk_tree_view_get_model (GTK_TREE_VIEW (selector)),
		source_selector_count_sources, &cd);

	return cd.count;
}

/**
 * e_source_selector_count_selected:
 * @selector: an #ESourceSelector
 *
 * Counts how many ESource-s are selected in the @selector.
 *
 * Returns: How many ESource-s are selected in the @selector.
 *
 * Since: 3.20
 **/
guint
e_source_selector_count_selected (ESourceSelector *selector)
{
	struct CountData cd;

	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), 0);

	cd.selector = selector;
	cd.count = 0;
	cd.selected = TRUE;

	gtk_tree_model_foreach (
		gtk_tree_view_get_model (GTK_TREE_VIEW (selector)),
		source_selector_count_sources, &cd);

	return cd.count;
}

/**
 * e_source_selector_select_source:
 * @selector: An #ESourceSelector widget
 * @source: An #ESource.
 *
 * Select @source in @selector.
 **/
void
e_source_selector_select_source (ESourceSelector *selector,
                                 ESource *source)
{
	ESourceSelectorClass *class;
	GtkTreeRowReference *reference;
	GHashTable *source_index;

	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));
	g_return_if_fail (E_IS_SOURCE (source));

	/* Make sure the ESource is in our tree model. */
	source_index = selector->priv->source_index;
	reference = g_hash_table_lookup (source_index, source);
	g_return_if_fail (gtk_tree_row_reference_valid (reference));

	class = E_SOURCE_SELECTOR_GET_CLASS (selector);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->set_source_selected != NULL);

	if (class->set_source_selected (selector, source, TRUE)) {
		g_signal_emit (selector, signals[SOURCE_SELECTED], 0, source);
		g_signal_emit (selector, signals[SELECTION_CHANGED], 0);
	}
}

/**
 * e_source_selector_unselect_source:
 * @selector: An #ESourceSelector widget
 * @source: An #ESource.
 *
 * Unselect @source in @selector.
 **/
void
e_source_selector_unselect_source (ESourceSelector *selector,
                                   ESource *source)
{
	ESourceSelectorClass *class;
	GtkTreeRowReference *reference;
	GHashTable *source_index;

	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));
	g_return_if_fail (E_IS_SOURCE (source));

	/* Make sure the ESource is in our tree model. */
	source_index = selector->priv->source_index;
	reference = g_hash_table_lookup (source_index, source);

	/* can be NULL when the source was just removed */
	if (!reference)
		return;

	g_return_if_fail (gtk_tree_row_reference_valid (reference));

	class = E_SOURCE_SELECTOR_GET_CLASS (selector);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->set_source_selected != NULL);

	if (class->set_source_selected (selector, source, FALSE)) {
		g_signal_emit (selector, signals[SOURCE_UNSELECTED], 0, source);
		g_signal_emit (selector, signals[SELECTION_CHANGED], 0);
	}
}

/**
 * e_source_selector_select_exclusive:
 * @selector: An #ESourceSelector widget
 * @source: An #ESource.
 *
 * Select @source in @selector and unselect all others.
 *
 * Since: 2.30
 **/
void
e_source_selector_select_exclusive (ESourceSelector *selector,
                                    ESource *source)
{
	ESourceSelectorClass *class;
	GHashTable *source_index;
	GHashTableIter iter;
	gpointer key;
	gboolean any_changed = FALSE;

	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));
	g_return_if_fail (E_IS_SOURCE (source));

	class = E_SOURCE_SELECTOR_GET_CLASS (selector);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->set_source_selected != NULL);

	source_index = selector->priv->source_index;
	g_hash_table_iter_init (&iter, source_index);

	while (g_hash_table_iter_next (&iter, &key, NULL)) {
		gboolean selected = e_source_equal (key, source);
		if (class->set_source_selected (selector, key, selected)) {
			any_changed = TRUE;
			if (selected)
				g_signal_emit (selector, signals[SOURCE_SELECTED], 0, key);
			else
				g_signal_emit (selector, signals[SOURCE_UNSELECTED], 0, key);
		}
	}

	if (any_changed)
		g_signal_emit (selector, signals[SELECTION_CHANGED], 0);
}

/**
 * e_source_selector_select_all:
 * @selector: An #ESourceSelector widget
 *
 * Selects all ESource-s in the @selector.
 *
 * Since: 3.20
 **/
void
e_source_selector_select_all (ESourceSelector *selector)
{
	ESourceSelectorClass *class;
	GHashTable *source_index;
	GHashTableIter iter;
	gpointer key;
	gboolean any_changed = FALSE;

	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));

	class = E_SOURCE_SELECTOR_GET_CLASS (selector);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->set_source_selected != NULL);

	source_index = selector->priv->source_index;
	g_hash_table_iter_init (&iter, source_index);

	while (g_hash_table_iter_next (&iter, &key, NULL)) {
		if (class->set_source_selected (selector, key, TRUE)) {
			any_changed = TRUE;
			g_signal_emit (selector, signals[SOURCE_SELECTED], 0, key);
		}
	}

	if (any_changed)
		g_signal_emit (selector, signals[SELECTION_CHANGED], 0);
}

/**
 * e_source_selector_source_is_selected:
 * @selector: An #ESourceSelector widget
 * @source: An #ESource.
 *
 * Check whether @source is selected in @selector.
 *
 * Returns: %TRUE if @source is currently selected, %FALSE otherwise.
 **/
gboolean
e_source_selector_source_is_selected (ESourceSelector *selector,
                                      ESource *source)
{
	ESourceSelectorClass *class;
	GtkTreeRowReference *reference;
	GHashTable *source_index;

	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);

	/* Make sure the ESource is in our tree model. */
	source_index = selector->priv->source_index;
	reference = g_hash_table_lookup (source_index, source);

	/* Can be NULL when the source was just removed */
	if (!reference)
		return FALSE;

	g_return_val_if_fail (gtk_tree_row_reference_valid (reference), FALSE);

	class = E_SOURCE_SELECTOR_GET_CLASS (selector);
	g_return_val_if_fail (class != NULL, FALSE);
	g_return_val_if_fail (class->get_source_selected != NULL, FALSE);

	return class->get_source_selected (selector, source);
}

/**
 * e_source_selector_edit_primary_selection:
 * @selector: An #ESourceSelector widget
 *
 * Allows the user to rename the primary selected source by opening an
 * entry box directly in @selector.
 *
 * Since: 2.26
 **/
void
e_source_selector_edit_primary_selection (ESourceSelector *selector)
{
	GtkTreeRowReference *reference;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreePath *path = NULL;
	GtkTreeIter iter;
	GList *list;

	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));

	tree_view = GTK_TREE_VIEW (selector);
	column = gtk_tree_view_get_column (tree_view, 0);
	reference = selector->priv->saved_primary_selection;
	selection = gtk_tree_view_get_selection (tree_view);

	if (reference != NULL)
		path = gtk_tree_row_reference_get_path (reference);
	else if (gtk_tree_selection_get_selected (selection, &model, &iter))
		path = gtk_tree_model_get_path (model, &iter);

	if (path == NULL)
		return;

	/* XXX Because we stuff three renderers in a single column,
	 *     we have to manually hunt for the text renderer. */
	renderer = NULL;
	list = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
	while (list != NULL) {
		renderer = list->data;
		if (GTK_IS_CELL_RENDERER_TEXT (renderer))
			break;
		list = g_list_delete_link (list, list);
	}
	g_list_free (list);

	/* Make the text cell renderer editable, but only temporarily.
	 * We don't want editing to be activated by simply clicking on
	 * the source name.  Too easy for accidental edits to occur. */
	g_object_set (renderer, "editable", TRUE, NULL);
	gtk_tree_view_expand_to_path (tree_view, path);
	gtk_tree_view_set_cursor_on_cell (
		tree_view, path, column, renderer, TRUE);
	g_object_set (renderer, "editable", FALSE, NULL);

	gtk_tree_path_free (path);
}

/**
 * e_source_selector_ref_primary_selection:
 * @selector: An #ESourceSelector widget
 *
 * Get the primary selected source.  The primary selection is the one that is
 * highlighted through the normal #GtkTreeView selection mechanism (as opposed
 * to the "normal" selection, which is the set of source whose checkboxes are
 * checked).
 *
 * The returned #ESource is referenced for thread-safety and must be
 * unreferenced with g_object_unref() when finished with it.
 *
 * Returns: The selected source.
 *
 * Since: 3.6
 **/
ESource *
e_source_selector_ref_primary_selection (ESourceSelector *selector)
{
	ESource *source;
	GtkTreeRowReference *reference;
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreeIter iter;
	const gchar *extension_name;
	gboolean have_iter = FALSE;

	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), NULL);

	tree_view = GTK_TREE_VIEW (selector);
	model = gtk_tree_view_get_model (tree_view);
	selection = gtk_tree_view_get_selection (tree_view);

	reference = selector->priv->saved_primary_selection;

	if (gtk_tree_row_reference_valid (reference)) {
		GtkTreePath *path;

		path = gtk_tree_row_reference_get_path (reference);
		have_iter = gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_path_free (path);
	}

	if (!have_iter)
		have_iter = gtk_tree_selection_get_selected (
			selection, NULL, &iter);

	if (!have_iter)
		return NULL;

	gtk_tree_model_get (model, &iter, COLUMN_SOURCE, &source, -1);

	if (!source) {
		gchar *child_data = NULL;

		gtk_tree_model_get (model, &iter, COLUMN_CHILD_DATA, &child_data, -1);

		if (child_data) {
			GtkTreeIter parent;

			if (gtk_tree_model_iter_parent (model, &parent, &iter))
				gtk_tree_model_get (model, &parent, COLUMN_SOURCE, &source, -1);

			g_free (child_data);
		}

		if (!source)
			return NULL;
	}

	extension_name = e_source_selector_get_extension_name (selector);

	if (!e_source_has_extension (source, extension_name)) {
		g_object_unref (source);
		return NULL;
	}

	return source;
}

/**
 * e_source_selector_set_primary_selection:
 * @selector: an #ESourceSelector widget
 * @source: an #ESource to select
 *
 * Highlights @source in @selector.  The highlighted #ESource is called
 * the primary selection.
 *
 * Do not confuse this function with e_source_selector_select_source(),
 * which activates the check box next to an #ESource's display name in
 * @selector.  This function does not alter the check box.
 **/
void
e_source_selector_set_primary_selection (ESourceSelector *selector,
                                         ESource *source)
{
	GHashTable *source_index;
	GtkTreeRowReference *reference;
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreePath *child_path;
	GtkTreePath *parent_path;
	const gchar *extension_name;

	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));
	g_return_if_fail (E_IS_SOURCE (source));

	tree_view = GTK_TREE_VIEW (selector);
	selection = gtk_tree_view_get_selection (tree_view);

	source_index = selector->priv->source_index;
	reference = g_hash_table_lookup (source_index, source);

	/* XXX Maybe we should return a success/fail boolean? */
	if (!gtk_tree_row_reference_valid (reference))
		return;

	extension_name = e_source_selector_get_extension_name (selector);

	/* Return silently if attempting to select a parent node
	 * lacking the expected extension (e.g. On This Computer). */
	if (!e_source_has_extension (source, extension_name))
		return;

	/* We block the signal because this all needs to be atomic */
	g_signal_handlers_block_matched (
		selection, G_SIGNAL_MATCH_FUNC,
		0, 0, NULL, selection_changed_callback, NULL);
	gtk_tree_selection_unselect_all (selection);
	g_signal_handlers_unblock_matched (
		selection, G_SIGNAL_MATCH_FUNC,
		0, 0, NULL, selection_changed_callback, NULL);

	clear_saved_primary_selection (selector);

	child_path = gtk_tree_row_reference_get_path (reference);

	parent_path = gtk_tree_path_copy (child_path);
	gtk_tree_path_up (parent_path);

	if (gtk_tree_view_row_expanded (tree_view, parent_path)) {
		gtk_tree_selection_select_path (selection, child_path);
	} else {
		selector->priv->saved_primary_selection =
			gtk_tree_row_reference_copy (reference);
		g_signal_emit (selector, signals[PRIMARY_SELECTION_CHANGED], 0);
		g_object_notify (G_OBJECT (selector), "primary-selection");
	}

	gtk_tree_path_free (child_path);
	gtk_tree_path_free (parent_path);
}

/**
 * e_source_selector_ref_source_by_iter:
 * @selector: an #ESourceSelector
 * @iter: a #GtkTreeIter
 *
 * Returns the #ESource object at @iter.
 *
 * The returned #ESource is referenced for thread-safety and must be
 * unreferenced with g_object_unref() when finished with it.
 *
 * Returns: the #ESource object at @iter, or %NULL
 *
 * Since: 3.8
 **/
ESource *
e_source_selector_ref_source_by_iter (ESourceSelector *selector,
                                      GtkTreeIter *iter)
{
	ESource *source = NULL;
	GtkTreeModel *model;

	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), NULL);
	g_return_val_if_fail (iter != NULL, NULL);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (selector));

	gtk_tree_model_get (model, iter, COLUMN_SOURCE, &source, -1);

	return source;
}

/**
 * e_source_selector_ref_source_by_path:
 * @selector: an #ESourceSelector
 * @path: a #GtkTreePath
 *
 * Returns the #ESource object at @path, or %NULL if @path is invalid.
 *
 * The returned #ESource is referenced for thread-safety and must be
 * unreferenced with g_object_unref() when finished with it.
 *
 * Returns: the #ESource object at @path, or %NULL
 *
 * Since: 3.6
 **/
ESource *
e_source_selector_ref_source_by_path (ESourceSelector *selector,
                                      GtkTreePath *path)
{
	ESource *source = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), NULL);
	g_return_val_if_fail (path != NULL, NULL);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (selector));

	if (gtk_tree_model_get_iter (model, &iter, path))
		gtk_tree_model_get (model, &iter, COLUMN_SOURCE, &source, -1);

	return source;
}

/**
 * e_source_selector_queue_write:
 * @selector: an #ESourceSelector
 * @source: an #ESource with changes to be written
 *
 * Queues a main loop idle callback to write changes to @source back to
 * the D-Bus registry service.
 *
 * Since: 3.6
 **/
void
e_source_selector_queue_write (ESourceSelector *selector,
                               ESource *source)
{
	GSource *idle_source;
	GHashTable *pending_writes;
	GMainContext *main_context;
	AsyncContext *async_context;

	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));
	g_return_if_fail (E_IS_SOURCE (source));

	main_context = selector->priv->main_context;
	pending_writes = selector->priv->pending_writes;

	idle_source = g_hash_table_lookup (pending_writes, source);
	if (idle_source != NULL && !g_source_is_destroyed (idle_source))
		return;

	async_context = g_slice_new0 (AsyncContext);
	async_context->selector = g_object_ref (selector);
	async_context->source = g_object_ref (source);

	/* Set a higher priority so this idle source runs before our
	 * source_selector_cancel_write() signal handler, which will
	 * cancel this idle source.  Cancellation is the right thing
	 * to do when receiving changes from OTHER registry clients,
	 * but we don't want to cancel our own changes.
	 *
	 * XXX This might be an argument for using etags.
	 */
	idle_source = g_idle_source_new ();
	g_hash_table_insert (
		pending_writes,
		g_object_ref (source),
		g_source_ref (idle_source));
	g_source_set_callback (
		idle_source,
		source_selector_write_idle_cb,
		async_context,
		(GDestroyNotify) async_context_free);
	g_source_set_priority (idle_source, G_PRIORITY_HIGH_IDLE);
	g_source_attach (idle_source, main_context);
	g_source_unref (idle_source);
}

static void
source_selector_sort_sibling (ESourceSelector *selector,
			      GtkTreeModel *model,
			      GtkTreeIter *changed_child)
{
	GtkTreeIter iter, dest_iter;
	ESource *child_source = NULL;
	gboolean changed = FALSE, insert_before;

	gtk_tree_model_get (model, changed_child, COLUMN_SOURCE, &child_source, -1);

	if (!child_source)
		return;

	insert_before = TRUE;
	iter = *changed_child;

	while (gtk_tree_model_iter_previous (model, &iter)) {
		ESource *source = NULL;
		gint cmp_value;

		gtk_tree_model_get (model, &iter, COLUMN_SOURCE, &source, -1);

		cmp_value = e_util_source_compare_for_sort (source, child_source);

		g_clear_object (&source);

		if (cmp_value <= 0)
			break;

		dest_iter = iter;
		changed = TRUE;
	}

	if (!changed) {
		insert_before = FALSE;
		iter = *changed_child;

		while (gtk_tree_model_iter_next (model, &iter)) {
			ESource *source = NULL;
			gint cmp_value;

			gtk_tree_model_get (model, &iter, COLUMN_SOURCE, &source, -1);

			cmp_value = e_util_source_compare_for_sort (child_source, source);

			g_clear_object (&source);

			if (cmp_value <= 0)
				break;

			dest_iter = iter;
			changed = TRUE;
		}
	}

	if (changed) {
		GtkTreeStore *tree_store = GTK_TREE_STORE (model);
		GtkTreeSelection *selection;
		GtkTreeRowReference *reference;
		GtkTreeIter new_child;
		GtkTreePath *path;
		gboolean been_selected;

		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (selector));
		been_selected = gtk_tree_selection_iter_is_selected (selection, changed_child);

		gtk_tree_store_remove (tree_store, changed_child);

		if (insert_before)
			gtk_tree_store_insert_before (tree_store, &new_child, NULL, &dest_iter);
		else
			gtk_tree_store_insert_after (tree_store, &new_child, NULL, &dest_iter);

		g_hash_table_remove (selector->priv->source_index, child_source);

		path = gtk_tree_model_get_path (model, &new_child);
		reference = gtk_tree_row_reference_new (model, path);
		g_hash_table_insert (selector->priv->source_index, g_object_ref (child_source), reference);
		gtk_tree_path_free (path);

		e_source_selector_update_row (selector, child_source);

		if (been_selected)
			gtk_tree_selection_select_iter (selection, &new_child);
	}

	g_object_unref (child_source);
}

/**
 * e_source_selector_update_row:
 * @selector: an #ESourceSelector
 * @source: an #ESource
 *
 * Updates the corresponding #GtkTreeModel row for @source.
 *
 * This function is public so it can be called from subclasses like
 * #EClientSelector.
 *
 * Since: 3.8
 **/
void
e_source_selector_update_row (ESourceSelector *selector,
                              ESource *source)
{
	GHashTable *source_index;
	ESourceExtension *extension = NULL;
	GtkTreeRowReference *reference;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	const gchar *extension_name;
	const gchar *display_name;
	gboolean selected;

	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));
	g_return_if_fail (E_IS_SOURCE (source));

	source_index = selector->priv->source_index;
	reference = g_hash_table_lookup (source_index, source);

	/* This function runs when ANY ESource in the registry changes.
	 * If the ESource is not in our tree model then return silently. */
	if (reference == NULL)
		return;

	/* If we do have a row reference, it should be valid. */
	g_return_if_fail (gtk_tree_row_reference_valid (reference));

	model = gtk_tree_row_reference_get_model (reference);
	path = gtk_tree_row_reference_get_path (reference);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);

	display_name = e_source_get_display_name (source);

	extension_name = e_source_selector_get_extension_name (selector);
	selected = e_source_selector_source_is_selected (selector, source);

	if (e_source_has_extension (source, extension_name))
		extension = e_source_get_extension (source, extension_name);

	if (extension != NULL) {
		ESource *current_source = NULL;
		GdkRGBA rgba;
		const gchar *color_spec = NULL;
		const gchar *icon_name;
		gboolean show_color;
		gboolean show_icons;
		gboolean show_toggle;
		guint old_sort_order = 0, new_sort_order = 0;

		show_color =
			E_IS_SOURCE_SELECTABLE (extension) &&
			e_source_selector_get_show_colors (selector);

		if (show_color)
			color_spec = e_source_selectable_get_color (
				E_SOURCE_SELECTABLE (extension));

		if (color_spec != NULL && *color_spec != '\0')
			show_color = gdk_rgba_parse (&rgba, color_spec);

		show_icons = e_source_selector_get_show_icons (selector);
		icon_name = source_selector_get_icon_name (selector, source);

		show_toggle = e_source_selector_get_show_toggles (selector);

		gtk_tree_model_get (model, &iter,
			COLUMN_SORT_ORDER, &old_sort_order,
			COLUMN_SOURCE, &current_source,
			-1);

		if (E_IS_SOURCE_SELECTABLE (extension))
			new_sort_order = e_source_selectable_get_order (E_SOURCE_SELECTABLE (extension));
		else if (E_IS_SOURCE_ADDRESS_BOOK (extension))
			new_sort_order = e_source_address_book_get_order (E_SOURCE_ADDRESS_BOOK (extension));
		else
			new_sort_order = old_sort_order;

		gtk_tree_store_set (
			GTK_TREE_STORE (model), &iter,
			COLUMN_NAME, display_name,
			COLUMN_COLOR, show_color ? &rgba : NULL,
			COLUMN_ACTIVE, selected,
			COLUMN_ICON_NAME, icon_name,
			COLUMN_SHOW_COLOR, show_color,
			COLUMN_SHOW_ICONS, show_icons,
			COLUMN_SHOW_TOGGLE, show_toggle,
			COLUMN_WEIGHT, PANGO_WEIGHT_NORMAL,
			COLUMN_SOURCE, source,
			COLUMN_SORT_ORDER, new_sort_order,
			-1);

		/* The 'current_source' is NULL on newly added rows and non-NULL when refreshing existing row */
		if (new_sort_order != old_sort_order && current_source != NULL)
			source_selector_sort_sibling (selector, model, &iter);

		g_clear_object (&current_source);
	} else {
		gtk_tree_store_set (
			GTK_TREE_STORE (model), &iter,
			COLUMN_NAME, display_name,
			COLUMN_COLOR, NULL,
			COLUMN_ACTIVE, FALSE,
			COLUMN_ICON_NAME, NULL,
			COLUMN_SHOW_COLOR, FALSE,
			COLUMN_SHOW_ICONS, FALSE,
			COLUMN_SHOW_TOGGLE, FALSE,
			COLUMN_WEIGHT, PANGO_WEIGHT_BOLD,
			COLUMN_SOURCE, source,
			-1);
	}
}

/**
 * e_source_selector_update_all_rows:
 * @selector: an #ESourceSelector
 *
 * Calls e_source_selector_update_row() for each #ESource being shown by
 * @selector, according to the #ESourceSelector:extension_name property.
 *
 * Since: 3.10
 **/
void
e_source_selector_update_all_rows (ESourceSelector *selector)
{
	ESourceRegistry *registry;
	GList *list, *link;
	const gchar *extension_name;

	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));

	registry = e_source_selector_get_registry (selector);
	extension_name = e_source_selector_get_extension_name (selector);

	list = e_source_registry_list_sources (registry, extension_name);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		e_source_selector_update_row (selector, source);
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);
}

/**
 * e_source_selector_get_source_iter:
 * @selector: an #ESourceSelector
 * @source: an #ESource
 * @iter: (out): a #GtkTreeIter to store the iterator to
 * @out_model: (out) (optional) (transfer none): a #GtkTreeModel to set to, or %NULL when not needed
 *
 * Gets an iterator for the @source, optionally also the model.
 *
 * Returns: whether the @source was found
 *
 * Since: 3.48
 **/
gboolean
e_source_selector_get_source_iter (ESourceSelector *selector,
				   ESource *source,
				   GtkTreeIter *iter,
				   GtkTreeModel **out_model)
{
	GtkTreeRowReference *reference;
	GtkTreeModel *model;
	GtkTreePath *path;
	gboolean found;

	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);
	g_return_val_if_fail (iter, FALSE);

	reference = g_hash_table_lookup (selector->priv->source_index, source);

	/* If the ESource is not in our tree model then return silently. */
	if (!reference)
		return FALSE;

	/* If we do have a row reference, it should be valid. */
	g_return_val_if_fail (gtk_tree_row_reference_valid (reference), FALSE);

	model = gtk_tree_row_reference_get_model (reference);
	path = gtk_tree_row_reference_get_path (reference);
	found = gtk_tree_model_get_iter (model, iter, path);
	gtk_tree_path_free (path);

	if (found && out_model)
		*out_model = model;

	return found;
}

/**
 * e_source_selector_set_source_tooltip:
 * @selector: an #ESourceSelector
 * @source: an #ESource for which to set the tooltip
 *
 * Updates tooltip for the given @source.
 *
 * Since: 3.16
 **/
void
e_source_selector_set_source_tooltip (ESourceSelector *selector,
				      ESource *source,
				      const gchar *tooltip)
{
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	gchar *current_tooltip = NULL;

	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));
	g_return_if_fail (E_IS_SOURCE (source));

	if (!e_source_selector_get_source_iter (selector, source, &iter, &model))
		return;

	gtk_tree_model_get (model, &iter,
		COLUMN_TOOLTIP, &current_tooltip,
		-1);

	/* This avoids possible recursion with ATK enabled */
	if (e_util_strcmp0 (current_tooltip, tooltip) != 0) {
		gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
			COLUMN_TOOLTIP, tooltip && *tooltip ? tooltip : NULL,
			-1);
	}

	g_free (current_tooltip);
}

/**
 * e_source_selector_dup_source_tooltip:
 * @selector: an #ESourceSelector
 * @source: an #ESource for which to read the tooltip
 *
 * Returns: Current tooltip for the given @source. Free the returned
 *    string with g_free() when done with it.
 *
 * Since: 3.16
 **/
gchar *
e_source_selector_dup_source_tooltip (ESourceSelector *selector,
				      ESource *source)
{
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	gchar *tooltip = NULL;

	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), NULL);
	g_return_val_if_fail (E_IS_SOURCE (source), NULL);

	if (!e_source_selector_get_source_iter (selector, source, &iter, &model))
		return NULL;

	gtk_tree_model_get (
		model, &iter,
		COLUMN_TOOLTIP, &tooltip,
		-1);

	return tooltip;
}

/**
 * e_source_selector_set_source_is_busy:
 * @selector: an #ESourceSelector
 * @source: an #ESource for which to set the is-busy status
 *
 * Updates the is-busy flag status for the given @source.
 *
 * Since: 3.16
 **/
void
e_source_selector_set_source_is_busy (ESourceSelector *selector,
				      ESource *source,
				      gboolean is_busy)
{
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	gboolean old_is_busy = FALSE;

	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));
	g_return_if_fail (E_IS_SOURCE (source));

	if (!e_source_selector_get_source_iter (selector, source, &iter, &model))
		return;

	gtk_tree_model_get (model, &iter,
		COLUMN_IS_BUSY, &old_is_busy,
		-1);

	if ((old_is_busy ? 1 : 0) == (is_busy ? 1 : 0))
		return;

	gtk_tree_store_set (
		GTK_TREE_STORE (model), &iter,
		COLUMN_IS_BUSY, is_busy,
		-1);

	if (is_busy)
		source_selector_inc_busy_sources (selector);
	else
		source_selector_dec_busy_sources (selector);
}

/**
 * e_source_selector_get_source_is_busy:
 * @selector: an #ESourceSelector
 * @source: an #ESource for which to read the is-busy status
 *
 * Returns: Current is-busy flag status for the given @source.
 *
 * Since: 3.16
 **/
gboolean
e_source_selector_get_source_is_busy (ESourceSelector *selector,
				      ESource *source)
{
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	gboolean is_busy = FALSE;

	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);

	if (!e_source_selector_get_source_iter (selector, source, &iter, &model))
		return FALSE;

	gtk_tree_model_get (
		model, &iter,
		COLUMN_IS_BUSY, &is_busy,
		-1);

	return is_busy;
}

/**
 * e_source_selector_set_source_connection_status:
 * @selector: an #ESourceSelector
 * @source: an #ESource for which to set the connection status
 * @value: the value to set
 *
 * Sets connection status for the @source. It's not interpreted by the @selector,
 * it only saves it into the model. Read it back with e_source_selector_get_source_connection_status().
 *
 * Since: 3.40
 **/
void
e_source_selector_set_source_connection_status (ESourceSelector *selector,
						ESource *source,
						guint value)
{
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	guint current_value = 0;

	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));
	g_return_if_fail (E_IS_SOURCE (source));

	if (!e_source_selector_get_source_iter (selector, source, &iter, &model))
		return;

	gtk_tree_model_get (
		model, &iter,
		COLUMN_CONNECTION_STATUS, &current_value,
		-1);

	/* This avoids possible recursion with ATK enabled */
	if (current_value != value) {
		gtk_tree_store_set (
			GTK_TREE_STORE (model), &iter,
			COLUMN_CONNECTION_STATUS, value,
			-1);
	}
}

/**
 * e_source_selector_get_source_connection_status:
 * @selector: an #ESourceSelector
 * @source: an #ESource for which to get the stored connection status
 *
 * Gets connection status for the @source. It's not interpreted by the @selector,
 * it only returns what had been saved into the model with e_source_selector_set_source_connection_status().
 *
 * Returns: Value previously stored with e_source_selector_set_source_connection_status(),
 *    or 0 when not set or when the @source was not found.
 *
 * Since: 3.40
 **/
guint
e_source_selector_get_source_connection_status (ESourceSelector *selector,
						ESource *source)
{
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	guint value = 0;

	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), 0);
	g_return_val_if_fail (E_IS_SOURCE (source), 0);

	if (!e_source_selector_get_source_iter (selector, source, &iter, &model))
		return 0;

	gtk_tree_model_get (
		model, &iter,
		COLUMN_CONNECTION_STATUS, &value,
		-1);

	return value;
}

static gboolean
source_selector_get_source_hidden (ESourceSelector *selector,
				   ESource *source)
{
	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);
	g_return_val_if_fail (e_source_get_uid (source) != NULL, FALSE);

	return g_hash_table_contains (selector->priv->hidden_groups, e_source_get_uid (source));
}

static void
tree_show_toggled (GtkCellRendererToggle *renderer,
		   gchar *path_str,
		   gpointer user_data)
{
	GtkWidget *table = user_data;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;

	path = gtk_tree_path_new_from_string (path_str);
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (table));

	if (gtk_tree_model_get_iter (model, &iter, path)) {
		gboolean shown = TRUE;

		gtk_tree_model_get (model, &iter, 2, &shown, -1);
		shown = !shown;
		gtk_list_store_set (GTK_LIST_STORE (model), &iter, 2, shown, -1);

		/* to have buttons synced with the change */
		g_signal_emit_by_name (table, "cursor-changed");
	}

	gtk_tree_path_free (path);
}

static GtkWidget *
create_tree (ESourceSelector *selector,
	     GtkWidget **tree)
{
	ESourceRegistry *registry;
	GtkWidget *table, *scrolled;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GtkListStore *model;
	GNode *root;

	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	model = gtk_list_store_new (3, G_TYPE_STRING, E_TYPE_SOURCE, G_TYPE_BOOLEAN);
	table = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (table), FALSE);

	renderer = gtk_cell_renderer_toggle_new ();
	g_object_set (G_OBJECT (renderer), "activatable", TRUE, NULL);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (table), -1,
						     _("Show"), renderer,
						     "active", 2, NULL);
	g_signal_connect (renderer, "toggled", G_CALLBACK (tree_show_toggled), table);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (table), -1,
						     _("Group name"), renderer,
						     "text", 0, NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (table));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	gtk_container_add (GTK_CONTAINER (scrolled), table);

	*tree = table;

	registry = e_source_selector_get_registry (selector);
	root = e_source_registry_build_display_tree (registry, e_source_selector_get_extension_name (selector));

	source_selector_sort_groups (selector, root);

	if (root) {
		GNode *node;

		for (node = g_node_first_child (root); node; node = g_node_next_sibling (node)) {
			GtkTreeIter iter;
			ESource *source;

			source = node->data;

			if (source) {
				gtk_list_store_append (model, &iter);
				gtk_list_store_set (model, &iter,
						0, e_source_get_display_name (source),
						1, source,
						2, !source_selector_get_source_hidden (selector, source),
						-1);
			}
		}
	}

	e_source_registry_free_display_tree (root);

	g_object_unref (model);

	return scrolled;
}

static void
deselect_sources_in_hidden_groups (ESourceSelector *selector)
{
	ESourceRegistry *registry;
	const gchar *extension_name;
	GNode *root;

	/* No group to hide */
	if (!g_hash_table_size (selector->priv->hidden_groups))
		return;

	/* Address books are not selectable */
	if (g_strcmp0 (e_source_selector_get_extension_name (selector), E_SOURCE_EXTENSION_ADDRESS_BOOK) == 0)
		return;

	extension_name = e_source_selector_get_extension_name (selector);
	registry = e_source_selector_get_registry (selector);
	root = e_source_registry_build_display_tree (registry, extension_name);

	if (root) {
		GNode *node;

		for (node = g_node_first_child (root); node; node = g_node_next_sibling (node)) {
			ESource *source;

			source = node->data;

			if (source && g_hash_table_contains (selector->priv->hidden_groups, e_source_get_uid (source))) {
				GNode *child_node;

				for (child_node = g_node_first_child (node); child_node; child_node = g_node_next_sibling (child_node)) {
					ESource *child = child_node->data;

					if (child && e_source_has_extension (child, extension_name)) {
						gpointer extension = e_source_get_extension (child, extension_name);

						if (E_IS_SOURCE_SELECTABLE (extension) &&
						    e_source_selectable_get_selected (extension)) {
							e_source_selector_unselect_source (selector, child);
						}
					}
				}
			}
		}
	}

	e_source_registry_free_display_tree (root);
}

static void
process_move_button (GtkButton *button,
		     GtkTreeView *tree,
		     gboolean is_up,
		     gboolean do_move)
{
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	gboolean enable = FALSE;

	g_return_if_fail (button != NULL);
	g_return_if_fail (tree != NULL);

	selection = gtk_tree_view_get_selection (tree);

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gpointer ptr = NULL, ptr2;
		GtkTreeIter iter2;
		int i, cnt = gtk_tree_model_iter_n_children (model, NULL);
		gboolean can_move = FALSE;

		gtk_tree_model_get (model, &iter, 1, &ptr, -1);

		for (i = 0; i < cnt; i++) {
			if (!gtk_tree_model_iter_nth_child (model, &iter2, NULL, i))
				break;

			ptr2 = NULL;
			gtk_tree_model_get (model, &iter2, 1, &ptr2, -1);

			if (ptr == ptr2 || (is_up && !do_move && i > 0)) {
				can_move = TRUE;
				break;
			}
		}

		if (can_move)
			can_move = ((is_up && i > 0) || (!is_up && i + 1 < cnt)) && i < cnt;

		if (can_move && do_move) {
			i = i + (is_up ? -1 : 1);
			if (gtk_tree_model_iter_nth_child (model, &iter2, NULL, i)) {
				GtkTreePath *path;

				gtk_list_store_swap (GTK_LIST_STORE (model), &iter, &iter2);
				gtk_tree_selection_select_iter (selection, &iter);

				/* scroll to the selected cell */
				path = gtk_tree_model_get_path (model, &iter);
				gtk_tree_view_scroll_to_cell (tree, path, NULL, FALSE, 0.0, 0.0);
				gtk_tree_path_free (path);

				/* cursor has been moved to the other row */
				can_move = (is_up && i > 0) || (!is_up && i + 1 < cnt);

				g_signal_emit_by_name (tree, "cursor-changed");
			}
		}

		enable = can_move;
	}

	if (!do_move)
		gtk_widget_set_sensitive (GTK_WIDGET (button), enable);
}

static void
up_clicked (GtkButton *button,
	    GtkTreeView *tree)
{
	process_move_button (button, tree, TRUE, TRUE);
}

static void
up_cursor_changed (GtkTreeView *tree,
		   GtkButton *button)
{
	process_move_button (button, tree, TRUE, FALSE);
}

static void
down_clicked (GtkButton *button,
	      GtkTreeView *tree)
{
	process_move_button (button, tree, FALSE, TRUE);
}

static void
down_cursor_changed (GtkTreeView *tree,
		     GtkButton *button)
{
	process_move_button (button, tree, FALSE, FALSE);
}

static void
show_hide_cursor_changed (GtkTreeView *tree,
			  GtkButton *button)
{
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;

	g_return_if_fail (button != NULL);
	g_return_if_fail (tree != NULL);

	selection = gtk_tree_view_get_selection (tree);

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gboolean shown = FALSE;

		gtk_tree_model_get (model, &iter, 2, &shown, -1);

		gtk_button_set_label (button, shown ? _("_Hide") : _("_Show"));
	}
}

static void
show_hide_clicked (GtkButton *button,
		   GtkTreeView *tree)
{
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;

	g_return_if_fail (button != NULL);
	g_return_if_fail (tree != NULL);

	selection = gtk_tree_view_get_selection (tree);

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gboolean shown = TRUE;

		gtk_tree_model_get (model, &iter, 2, &shown, -1);
		shown = !shown;
		gtk_list_store_set (GTK_LIST_STORE (model), &iter, 2, shown, -1);

		show_hide_cursor_changed (tree, button);
	}
}

/**
 * e_source_selector_manage_groups:
 * @selector: an #ESourceSelector
 *
 * Manages list of groups, like their order in the source selector,
 * and a hidden property of the group.
 *
 * Returns: Whether user confirmed changes in the dialog.
 *
 * Since: 3.20
 **/
gboolean
e_source_selector_manage_groups (ESourceSelector *selector)
{
	GtkWidget *dlg, *box, *pbox, *tree, *w, *w2;
	gchar *txt;
	gboolean confirmed = FALSE;

	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), FALSE);

	w = gtk_widget_get_toplevel (GTK_WIDGET (selector));
	if (!w || !gtk_widget_is_toplevel (w))
		w = NULL;

	dlg = gtk_dialog_new_with_buttons (_("Manage Groups"),
			w ? GTK_WINDOW (w) : NULL,
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
			GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
			NULL);

	w = gtk_dialog_get_content_area (GTK_DIALOG (dlg));
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
	gtk_container_set_border_width (GTK_CONTAINER (box), 12);
	gtk_box_pack_start (GTK_BOX (w), box, TRUE, TRUE, 0);

	txt = g_strconcat ("<b>", _("Available Groups:"), "</b>", NULL);
	w = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (w), txt);
	g_free (txt);
	gtk_label_set_xalign (GTK_LABEL (w), 0);
	gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 2);

	pbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_box_pack_start (GTK_BOX (box), pbox, TRUE, TRUE, 2);

	/* space on the left */
	w = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (pbox), w, FALSE, FALSE, 6);

	w = create_tree (selector, &tree);
	gtk_widget_set_size_request (w, 200, 240);
	gtk_box_pack_start (GTK_BOX (pbox), w, TRUE, TRUE, 2);

	/* box of buttons */
	w2 = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (w2), GTK_BUTTONBOX_START);
	gtk_box_pack_start (GTK_BOX (pbox), w2, FALSE, FALSE, 2);

	#define add_button(_x,_y,_cb,_cb2) \
		w = (_x) ? gtk_button_new_from_icon_name (_x, GTK_ICON_SIZE_BUTTON) : gtk_button_new (); \
		gtk_button_set_label (GTK_BUTTON (w), _y); \
		gtk_button_set_use_underline (GTK_BUTTON (w), TRUE); \
		gtk_box_pack_start (GTK_BOX (w2), w, FALSE, FALSE, 2); \
		g_signal_connect (w, "clicked", (GCallback)_cb, tree); \
		g_signal_connect (tree, "cursor-changed", (GCallback)_cb2, w);

	add_button ("go-up", _("_Up"), up_clicked, up_cursor_changed);
	add_button ("go-down", _("_Down"), down_clicked, down_cursor_changed);

	add_button (NULL, _("_Show"), show_hide_clicked, show_hide_cursor_changed);
	gtk_button_set_use_underline (GTK_BUTTON (w), TRUE);

	#undef add_button

	gtk_widget_show_all (box);

	if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_ACCEPT) {
		GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree));
		GtkTreeIter iter;
		gint ii, cnt = gtk_tree_model_iter_n_children (model, NULL);

		g_hash_table_remove_all (selector->priv->hidden_groups);
		g_slist_free_full (selector->priv->groups_order, g_free);
		selector->priv->groups_order = NULL;

		for (ii = 0; ii < cnt; ii++) {
			gpointer group = NULL;
			gboolean shown = TRUE;

			if (!gtk_tree_model_iter_nth_child (model, &iter, NULL, ii))
				break;

			gtk_tree_model_get (model, &iter, 1, &group, 2, &shown, -1);

			if (group) {
				const gchar *uid = e_source_get_uid (group);

				selector->priv->groups_order = g_slist_prepend (selector->priv->groups_order, g_strdup (uid));

				if (!shown)
					g_hash_table_insert (selector->priv->hidden_groups, g_strdup (uid), GINT_TO_POINTER (1));
			}
		}

		selector->priv->groups_order = g_slist_reverse (selector->priv->groups_order);

		deselect_sources_in_hidden_groups (selector);
		source_selector_build_model (selector);

		confirmed = TRUE;
	}

	gtk_widget_destroy (dlg);

	return confirmed;
}

static gboolean
source_selector_store_value (GKeyFile *key_file,
			     const gchar *group_key,
			     const gchar * const *value,
			     gsize value_length)
{
	gchar **stored;
	gsize length = 0, ii;
	gboolean changed = FALSE;

	g_return_val_if_fail (key_file != NULL, FALSE);
	g_return_val_if_fail (group_key != NULL, FALSE);

	stored = g_key_file_get_string_list (key_file, E_SOURCE_SELECTOR_GROUPS_SETUP_NAME, group_key, &length, NULL);
	if (stored) {
		changed = value_length != length;
		if (!changed) {
			for (ii = 0; ii < length && !changed; ii++) {
				changed = g_strcmp0 (value[ii], stored[ii]) != 0;
			}
		}

		g_strfreev (stored);
	} else {
		changed = value != NULL;
	}

	if (changed) {
		if (value)
			g_key_file_set_string_list (key_file, E_SOURCE_SELECTOR_GROUPS_SETUP_NAME, group_key, value, value_length);
		else
			changed = g_key_file_remove_key (key_file, E_SOURCE_SELECTOR_GROUPS_SETUP_NAME, group_key, NULL);
	}

	return changed;
}

/**
 * e_source_selector_save_groups_setup:
 * @selector: an #ESourceSelector
 * @key_file: a #GKeyFile to store the sgroups setup to
 *
 * Stores current setup of the groups in the @key_file.
 *
 * Use e_source_selector_load_groups_setup() to pass the settings
 * back to the @selector.
 *
 * Returns: Whether the saved values are different, aka whether it's
 *    required to store the changes.
 *
 * Since: 3.20
 **/
gboolean
e_source_selector_save_groups_setup (ESourceSelector *selector,
				     GKeyFile *key_file)
{
	GPtrArray *value;
	const gchar *extension_name;
	gchar *group_key;
	gboolean changed;

	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), FALSE);
	g_return_val_if_fail (key_file != NULL, FALSE);

	extension_name = e_source_selector_get_extension_name (selector);
	g_return_val_if_fail (extension_name != NULL, FALSE);

	group_key = g_strconcat (extension_name, "-hidden-groups", NULL);

	if (g_hash_table_size (selector->priv->hidden_groups) > 0) {
		GHashTableIter iter;
		gpointer key, unused;

		value = g_ptr_array_sized_new (g_hash_table_size (selector->priv->hidden_groups));

		g_hash_table_iter_init (&iter, selector->priv->hidden_groups);
		while (g_hash_table_iter_next (&iter, &key, &unused)) {
			if (key)
				g_ptr_array_add (value, key);
		}

		/* expects NULL-terminated array of strings, thus terminate it */
		g_ptr_array_add (value, NULL);

		changed = source_selector_store_value (key_file, group_key, (const gchar * const *) value->pdata, value->len - 1);

		g_ptr_array_unref (value);
	} else {
		changed = source_selector_store_value (key_file, group_key, NULL, 0);
	}

	g_free (group_key);
	group_key = g_strconcat (extension_name, "-groups-order", NULL);

	if (selector->priv->groups_order) {
		GSList *link;

		value = g_ptr_array_sized_new (g_slist_length (selector->priv->groups_order));

		for (link = selector->priv->groups_order; link; link = g_slist_next (link)) {
			if (link->data)
				g_ptr_array_add (value, link->data);
		}

		/* expects NULL-terminated array of strings, thus terminate it */
		g_ptr_array_add (value, NULL);

		changed = source_selector_store_value (key_file, group_key, (const gchar * const *) value->pdata, value->len - 1) || changed;

		g_ptr_array_unref (value);
	} else {
		changed = source_selector_store_value (key_file, group_key, NULL, 0) || changed;
	}

	g_free (group_key);

	return changed;
}

/**
 * e_source_selector_load_groups_setup:
 * @selector: an #ESourceSelector
 * @key_file: a #GKeyFile to load the groups setup from
 *
 * Loads setup of the groups from the @key_file.
 *
 * Use e_source_selector_save_groups_setup() to store
 * the settings of the @selector.
 *
 * Since: 3.20
 **/
void
e_source_selector_load_groups_setup (ESourceSelector *selector,
				     GKeyFile *key_file)
{
	const gchar *extension_name;
	gchar **stored;
	gchar *group_key;
	gsize ii;

	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));

	extension_name = e_source_selector_get_extension_name (selector);
	g_return_if_fail (extension_name != NULL);

	g_hash_table_remove_all (selector->priv->hidden_groups);
	g_slist_free_full (selector->priv->groups_order, g_free);
	selector->priv->groups_order = NULL;

	group_key = g_strconcat (extension_name, "-hidden-groups", NULL);

	stored = g_key_file_get_string_list (key_file, E_SOURCE_SELECTOR_GROUPS_SETUP_NAME, group_key, NULL, NULL);
	if (stored) {
		for (ii = 0; stored[ii]; ii++) {
			g_hash_table_insert (selector->priv->hidden_groups, g_strdup (stored[ii]), GINT_TO_POINTER (1));
		}

		g_strfreev (stored);
	}

	g_free (group_key);
	group_key = g_strconcat (extension_name, "-groups-order", NULL);

	stored = g_key_file_get_string_list (key_file, E_SOURCE_SELECTOR_GROUPS_SETUP_NAME, group_key, NULL, NULL);
	if (stored) {
		for (ii = 0; stored[ii]; ii++) {
			selector->priv->groups_order = g_slist_prepend (selector->priv->groups_order, g_strdup (stored[ii]));
		}

		g_strfreev (stored);
	}

	g_free (group_key);

	selector->priv->groups_order = g_slist_reverse (selector->priv->groups_order);

	source_selector_build_model (selector);
}

/**
 * e_source_selector_add_source_child:
 * @selector: an #ESourceSelector
 * @source: a parent #ESource
 * @display_name: user-visible text for the child
 * @child_data: custom child data
 *
 * Adds a child node under the @source node, identified by @child_data
 * in functions e_source_selector_remove_source_child() and
 * e_source_selector_foreach_source_child(), the same as in the signal
 * callbacks. It's the caller's responsibility to choose a unique
 * @child_data.
 *
 * Listen to #ESourceSelector::after-rebuild signal to re-add the child
 * data after the content is rebuilt.
 *
 * Since: 3.48
 **/
void
e_source_selector_add_source_child (ESourceSelector *selector,
				    ESource *source,
				    const gchar *display_name,
				    const gchar *child_data)
{
	GtkTreeModel *model = NULL;
	GtkTreeStore *tree_store;
	GtkTreeIter iter, child;

	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));
	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (display_name != NULL);
	g_return_if_fail (child_data != NULL);

	if (!e_source_selector_get_source_iter (selector, source, &iter, &model))
		return;

	tree_store = GTK_TREE_STORE (model);

	gtk_tree_store_append (tree_store, &child, &iter);
	gtk_tree_store_set (tree_store, &child,
		COLUMN_NAME, display_name,
		COLUMN_CHILD_DATA, child_data,
		COLUMN_WEIGHT, PANGO_WEIGHT_NORMAL,
		-1);
}

static gboolean
e_source_selector_remove_all_children_cb (ESourceSelector *selector,
					  const gchar *display_name,
					  const gchar *child_data,
					  gpointer user_data)
{
	return TRUE;
}

/**
 * e_source_selector_remove_source_children:
 * @selector: an #ESourceSelector
 * @source: an #ESource
 *
 * Removes all children of the @source, previously added
 * by e_source_selector_add_source_child(). It does not remove
 * real ESource references.
 *
 * Since: 3.48
 **/
void
e_source_selector_remove_source_children (ESourceSelector *selector,
					  ESource *source)
{
	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));
	g_return_if_fail (E_IS_SOURCE (source));

	e_source_selector_foreach_source_child_remove (selector, source,
		e_source_selector_remove_all_children_cb, NULL);
}

/**
 * e_source_selector_foreach_source_child_remove:
 * @selector: an #ESourceSelector
 * @source: a parent #ESource
 * @func: (scope call): function to call for each child
 * @user_data: user data passed to the @func
 *
 * Traverses all @source children previously added by e_source_selector_add_source_child()
 * and removes those, for which the @func returns %TRUE.
 *
 * Since: 3.48
 **/
void
e_source_selector_foreach_source_child_remove (ESourceSelector *selector,
					       ESource *source,
					       ESourceSelectorForeachSourceChildFunc func,
					       gpointer user_data)
{
	GtkTreeModel *model = NULL;
	GtkTreeStore *tree_store;
	GtkTreeIter iter, child;
	gboolean has_more;

	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));
	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (func != NULL);

	if (!e_source_selector_get_source_iter (selector, source, &iter, &model))
		return;

	tree_store = GTK_TREE_STORE (model);

	has_more = gtk_tree_model_iter_children (model, &child, &iter);

	while (has_more) {
		gchar *display_name = NULL, *child_data = NULL;

		gtk_tree_model_get (model, &child,
			COLUMN_NAME, &display_name,
			COLUMN_CHILD_DATA, &child_data,
			-1);

		if (child_data) {
			if (func (selector, display_name, child_data, user_data))
				has_more = gtk_tree_store_remove (tree_store, &child);
			else
				has_more = gtk_tree_model_iter_next (model, &child);
		} else {
			has_more = gtk_tree_model_iter_next (model, &child);
		}

		g_free (display_name);
		g_free (child_data);
	}
}

/**
 * e_source_selector_dup_selected_child_data:
 * @selector: an #ESourceSelector
 *
 * Returns child data for the selected source child, as set
 * by e_source_selector_add_source_child().
 *
 * Free the returned string with g_free(), when no longer needed.
 *
 * Returns: (transfer full) (nullable): source child data of
 *    the selected node, or %NULL, when none set
 *
 * Since: 3.48
 **/
gchar *
e_source_selector_dup_selected_child_data (ESourceSelector *selector)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	gchar *child_data = NULL;

	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (selector));

	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return NULL;

	gtk_tree_model_get (model, &iter, COLUMN_CHILD_DATA, &child_data, -1);

	return child_data;
}
