/*
 * gal-view-etable.c
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
 */

#include "gal-view-etable.h"

#define GAL_VIEW_ETABLE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), GAL_TYPE_VIEW_ETABLE, GalViewEtablePrivate))

struct _GalViewEtablePrivate {
	ETableSpecification *spec;
	ETableState *state;

	ETable *table;
	guint table_state_changed_id;

	ETree *tree;
	guint tree_state_changed_id;
};

G_DEFINE_TYPE (GalViewEtable, gal_view_etable, GAL_TYPE_VIEW)

static void
detach_table (GalViewEtable *view)
{
	if (view->priv->table_state_changed_id > 0) {
		g_signal_handler_disconnect (
			view->priv->table,
			view->priv->table_state_changed_id);
		view->priv->table_state_changed_id = 0;
	}

	g_clear_object (&view->priv->table);
}

static void
detach_tree (GalViewEtable *view)
{
	if (view->priv->tree_state_changed_id > 0) {
		g_signal_handler_disconnect (
			view->priv->tree,
			view->priv->tree_state_changed_id);
		view->priv->tree_state_changed_id = 0;
	}

	g_clear_object (&view->priv->tree);
}

static void
gal_view_etable_load (GalView *view,
                      const gchar *filename)
{
	e_table_state_load_from_file (
		GAL_VIEW_ETABLE (view)->priv->state, filename);
}

static void
gal_view_etable_save (GalView *view,
                      const gchar *filename)
{
	e_table_state_save_to_file (
		GAL_VIEW_ETABLE (view)->priv->state, filename);
}

static const gchar *
gal_view_etable_get_type_code (GalView *view)
{
	return "etable";
}

static GalView *
gal_view_etable_clone (GalView *view)
{
	GalViewEtable *gve;
	GalView *clone;

	/* Chain up to parent's clone() method. */
	clone = GAL_VIEW_CLASS (gal_view_etable_parent_class)->clone (view);

	gve = GAL_VIEW_ETABLE (view);
	GAL_VIEW_ETABLE (clone)->priv->spec =
		g_object_ref (gve->priv->spec);
	GAL_VIEW_ETABLE (clone)->priv->state =
		e_table_state_duplicate (gve->priv->state);

	return clone;
}

static void
gal_view_etable_dispose (GObject *object)
{
	GalViewEtable *view = GAL_VIEW_ETABLE (object);

	gal_view_etable_detach (view);

	g_clear_object (&view->priv->spec);
	g_clear_object (&view->priv->state);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (gal_view_etable_parent_class)->dispose (object);
}

static void
gal_view_etable_class_init (GalViewEtableClass *class)
{
	GObjectClass *object_class;
	GalViewClass *gal_view_class;

	g_type_class_add_private (class, sizeof (GalViewEtablePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = gal_view_etable_dispose;

	gal_view_class = GAL_VIEW_CLASS (class);
	gal_view_class->load = gal_view_etable_load;
	gal_view_class->save = gal_view_etable_save;
	gal_view_class->get_type_code = gal_view_etable_get_type_code;
	gal_view_class->clone = gal_view_etable_clone;
}

static void
gal_view_etable_init (GalViewEtable *view)
{
	view->priv = GAL_VIEW_ETABLE_GET_PRIVATE (view);
}

/**
 * gal_view_etable_new
 * @spec: The ETableSpecification that this view will be based upon.
 * @title: The name of the new view.
 *
 * Returns a new GalViewEtable.  This is primarily for use by
 * GalViewFactoryEtable.
 *
 * Returns: The new GalViewEtable.
 */
GalView *
gal_view_etable_new (ETableSpecification *spec,
                     const gchar *title)
{
	GalViewEtable *view;

	g_return_val_if_fail (E_IS_TABLE_SPECIFICATION (spec), NULL);

	view = g_object_new (GAL_TYPE_VIEW_ETABLE, "title", title, NULL);

	return gal_view_etable_construct (view, spec);
}

/**
 * gal_view_etable_construct
 * @view: The view to construct.
 * @spec: The ETableSpecification that this view will be based upon.
 *
 * constructs the GalViewEtable.  To be used by subclasses and
 * language bindings.
 *
 * Returns: The GalViewEtable.
 */
GalView *
gal_view_etable_construct (GalViewEtable *view,
                           ETableSpecification *spec)
{
	g_return_val_if_fail (GAL_IS_VIEW_ETABLE (view), NULL);
	g_return_val_if_fail (E_IS_TABLE_SPECIFICATION (spec), NULL);

	view->priv->spec = g_object_ref (spec);

	g_clear_object (&view->priv->state);
	view->priv->state = e_table_state_duplicate (spec->state);

	return GAL_VIEW (view);
}

void
gal_view_etable_set_state (GalViewEtable *view,
                           ETableState *state)
{
	g_return_if_fail (GAL_IS_VIEW_ETABLE (view));
	g_return_if_fail (E_IS_TABLE_STATE (state));

	g_clear_object (&view->priv->state);
	view->priv->state = e_table_state_duplicate (state);

	gal_view_changed (GAL_VIEW (view));
}

static void
table_state_changed (ETable *table,
                     GalViewEtable *view)
{
	g_clear_object (&view->priv->state);
	view->priv->state = e_table_get_state_object (table);

	gal_view_changed (GAL_VIEW (view));
}

static void
tree_state_changed (ETree *tree,
                    GalViewEtable *view)
{
	g_clear_object (&view->priv->state);
	view->priv->state = e_tree_get_state_object (tree);

	gal_view_changed (GAL_VIEW (view));
}

void
gal_view_etable_attach_table (GalViewEtable *view,
                              ETable *table)
{
	g_return_if_fail (GAL_IS_VIEW_ETABLE (view));
	g_return_if_fail (E_IS_TABLE (table));

	gal_view_etable_detach (view);

	view->priv->table = g_object_ref (table);

	e_table_set_state_object (view->priv->table, view->priv->state);

	view->priv->table_state_changed_id = g_signal_connect (
		view->priv->table, "state_change",
		G_CALLBACK (table_state_changed), view);
}

void
gal_view_etable_attach_tree (GalViewEtable *view,
                             ETree *tree)
{
	g_return_if_fail (GAL_IS_VIEW_ETABLE (view));
	g_return_if_fail (E_IS_TREE (tree));

	gal_view_etable_detach (view);

	view->priv->tree = g_object_ref (tree);

	e_tree_set_state_object (view->priv->tree, view->priv->state);

	view->priv->tree_state_changed_id = g_signal_connect (
		view->priv->tree, "state_change",
		G_CALLBACK (tree_state_changed), view);
}

void
gal_view_etable_detach (GalViewEtable *view)
{
	g_return_if_fail (GAL_IS_VIEW_ETABLE (view));

	if (view->priv->table != NULL)
		detach_table (view);
	if (view->priv->tree != NULL)
		detach_tree (view);
}
