/*
 * gal-view-etable.c
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

#include "gal-view-etable.h"

struct _GalViewEtablePrivate {
	gchar *state_filename;

	ETable *table;
	guint table_state_changed_id;

	ETree *tree;
	guint tree_state_changed_id;
};

G_DEFINE_TYPE_WITH_PRIVATE (GalViewEtable, gal_view_etable, GAL_TYPE_VIEW)

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
	GalViewEtable *view_etable;

	view_etable = GAL_VIEW_ETABLE (view);

	/* Just note the filename.  We'll do the actual load
	 * when an ETable or ETree gets attached since we need
	 * its ETableSpecification to create an ETableState. */
	g_free (view_etable->priv->state_filename);
	view_etable->priv->state_filename = g_strdup (filename);
}

static void
gal_view_etable_save (GalView *view,
                      const gchar *filename)
{
	GalViewEtable *view_etable;

	view_etable = GAL_VIEW_ETABLE (view);

	if (view_etable->priv->table != NULL) {
		ETableState *state;

		state = e_table_get_state_object (view_etable->priv->table);
		e_table_state_save_to_file (state, filename);
		g_object_unref (state);
	}

	if (view_etable->priv->tree != NULL) {
		ETableState *state;

		state = e_tree_get_state_object (view_etable->priv->tree);
		e_table_state_save_to_file (state, filename);
		g_object_unref (state);
	}

	/* Remember the filename, it may eventually change */
	gal_view_etable_load (view, filename);
}

static GalView *
gal_view_etable_clone (GalView *view)
{
	GalViewEtable *gve;
	GalView *clone;

	/* Chain up to parent's clone() method. */
	clone = GAL_VIEW_CLASS (gal_view_etable_parent_class)->clone (view);

	gve = GAL_VIEW_ETABLE (view);

	/* do this before setting state_filename, to not overwrite current
	 * state changes in the 'attach' function */
	if (gve->priv->table)
		gal_view_etable_attach_table (GAL_VIEW_ETABLE (clone), gve->priv->table);
	else if (gve->priv->tree)
		gal_view_etable_attach_tree (GAL_VIEW_ETABLE (clone), gve->priv->tree);

	GAL_VIEW_ETABLE (clone)->priv->state_filename =
		g_strdup (gve->priv->state_filename);

	return clone;
}

static void
gal_view_etable_dispose (GObject *object)
{
	GalViewEtable *view = GAL_VIEW_ETABLE (object);

	gal_view_etable_detach (view);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (gal_view_etable_parent_class)->dispose (object);
}

static void
gal_view_etable_finalize (GObject *object)
{
	GalViewEtable *view = GAL_VIEW_ETABLE (object);

	g_free (view->priv->state_filename);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (gal_view_etable_parent_class)->finalize (object);
}

static void
gal_view_etable_class_init (GalViewEtableClass *class)
{
	GObjectClass *object_class;
	GalViewClass *gal_view_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = gal_view_etable_dispose;
	object_class->finalize = gal_view_etable_finalize;

	gal_view_class = GAL_VIEW_CLASS (class);
	gal_view_class->type_code = "etable";
	gal_view_class->load = gal_view_etable_load;
	gal_view_class->save = gal_view_etable_save;
	gal_view_class->clone = gal_view_etable_clone;
}

static void
gal_view_etable_init (GalViewEtable *view)
{
	view->priv = gal_view_etable_get_instance_private (view);
}

/**
 * gal_view_etable_new
 * @title: The name of the new view.
 *
 * Returns a new GalViewEtable.  This is primarily for use by
 * GalViewFactoryEtable.
 *
 * Returns: The new GalViewEtable.
 */
GalView *
gal_view_etable_new (const gchar *title)
{
	return g_object_new (GAL_TYPE_VIEW_ETABLE, "title", title, NULL);
}

static void
table_state_changed (ETable *table,
                     GalView *view)
{
	gal_view_changed (view);
}

static void
tree_state_changed (ETree *tree,
                    GalView *view)
{
	gal_view_changed (view);
}

void
gal_view_etable_attach_table (GalViewEtable *view,
                              ETable *table)
{
	g_return_if_fail (GAL_IS_VIEW_ETABLE (view));
	g_return_if_fail (E_IS_TABLE (table));

	gal_view_etable_detach (view);

	/* Load the state file now. */
	if (view->priv->state_filename != NULL) {
		ETableSpecification *specification;
		ETableState *state;

		specification = table->spec;
		state = e_table_state_new (specification);
		e_table_state_load_from_file (
			state, view->priv->state_filename);

		e_table_set_state_object (table, state);

		g_object_unref (state);
	}

	view->priv->table = g_object_ref (table);

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

	/* Load the state file now. */
	if (view->priv->state_filename != NULL) {
		ETableSpecification *specification;
		ETableState *state;

		specification = e_tree_get_spec (tree);
		state = e_table_state_new (specification);
		e_table_state_load_from_file (
			state, view->priv->state_filename);

		e_tree_set_state_object (tree, state);

		g_object_unref (state);
	}

	view->priv->tree = g_object_ref (tree);

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

ETable *
gal_view_etable_get_table (GalViewEtable *view)
{
	g_return_val_if_fail (GAL_IS_VIEW_ETABLE (view), NULL);

	return view->priv->table;
}

ETree *
gal_view_etable_get_tree (GalViewEtable *view)
{
	g_return_val_if_fail (GAL_IS_VIEW_ETABLE (view), NULL);

	return view->priv->tree;
}
