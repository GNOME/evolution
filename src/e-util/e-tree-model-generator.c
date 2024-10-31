/* e-tree-model-generator.c - Model wrapper that permutes underlying rows.
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Authors: Hans Petter Jansson <hpj@novell.com>
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include "e-tree-model-generator.h"

#define ETMG_DEBUG(x)

#define ITER_IS_VALID(tree_model_generator, iter) \
	((iter)->stamp == (tree_model_generator)->priv->stamp)
#define ITER_GET(iter, group, index) \
	G_STMT_START { \
	*(group) = (iter)->user_data; \
	*(index) = GPOINTER_TO_INT ((iter)->user_data2); \
	} G_STMT_END

#define ITER_SET(tree_model_generator, iter, group, index) \
	G_STMT_START { \
	(iter)->stamp = (tree_model_generator)->priv->stamp; \
	(iter)->user_data = group; \
	(iter)->user_data2 = GINT_TO_POINTER (index); \
	} G_STMT_END

struct _ETreeModelGeneratorPrivate {
	GtkTreeModel *child_model;
	GArray *root_nodes;
	gint stamp;

	ETreeModelGeneratorGenerateFunc generate_func;
	gpointer generate_func_data;

	ETreeModelGeneratorModifyFunc modify_func;
	gpointer modify_func_data;
	GSList *offset_cache;
};

typedef struct {
	gint offset;
	gint index;
} CacheItem;

static void e_tree_model_generator_tree_model_init (GtkTreeModelIface *iface);

G_DEFINE_TYPE_WITH_CODE (ETreeModelGenerator, e_tree_model_generator, G_TYPE_OBJECT,
	G_ADD_PRIVATE (ETreeModelGenerator)
	G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL, e_tree_model_generator_tree_model_init))

static GtkTreeModelFlags e_tree_model_generator_get_flags       (GtkTreeModel       *tree_model);
static gint         e_tree_model_generator_get_n_columns   (GtkTreeModel       *tree_model);
static GType        e_tree_model_generator_get_column_type (GtkTreeModel       *tree_model,
							    gint                index);
static gboolean     e_tree_model_generator_get_iter        (GtkTreeModel       *tree_model,
							    GtkTreeIter        *iter,
							    GtkTreePath        *path);
static GtkTreePath *e_tree_model_generator_get_path        (GtkTreeModel       *tree_model,
							    GtkTreeIter        *iter);
static void         e_tree_model_generator_get_value       (GtkTreeModel       *tree_model,
							    GtkTreeIter        *iter,
							    gint                column,
							    GValue             *value);
static gboolean     e_tree_model_generator_iter_next       (GtkTreeModel       *tree_model,
							    GtkTreeIter        *iter);
static gboolean     e_tree_model_generator_iter_children   (GtkTreeModel       *tree_model,
							    GtkTreeIter        *iter,
							    GtkTreeIter        *parent);
static gboolean     e_tree_model_generator_iter_has_child  (GtkTreeModel       *tree_model,
							    GtkTreeIter        *iter);
static gint         e_tree_model_generator_iter_n_children (GtkTreeModel       *tree_model,
							    GtkTreeIter        *iter);
static gboolean     e_tree_model_generator_iter_nth_child  (GtkTreeModel       *tree_model,
							    GtkTreeIter        *iter,
							    GtkTreeIter        *parent,
							    gint                n);
static gboolean     e_tree_model_generator_iter_parent     (GtkTreeModel       *tree_model,
							    GtkTreeIter        *iter,
							    GtkTreeIter        *child);

static GArray *build_node_map     (ETreeModelGenerator *tree_model_generator, GtkTreeIter *parent_iter,
				   GArray *parent_group, gint parent_index);
static void    release_node_map   (GArray *group);

static void    child_row_changed  (ETreeModelGenerator *tree_model_generator, GtkTreePath *path, GtkTreeIter *iter);
static void    child_row_inserted (ETreeModelGenerator *tree_model_generator, GtkTreePath *path, GtkTreeIter *iter);
static void    child_row_deleted  (ETreeModelGenerator *tree_model_generator, GtkTreePath *path);

typedef struct {
	GArray *parent_group;
	gint    parent_index;

	gint    n_generated;
	GArray *child_nodes;
}
Node;

enum {
	PROP_0,
	PROP_CHILD_MODEL
};

/* ------------------ *
 * Class/object setup *
 * ------------------ */

static void
tree_model_generator_set_property (GObject *object,
                                   guint prop_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
	ETreeModelGenerator *tree_model_generator = E_TREE_MODEL_GENERATOR (object);

	switch (prop_id)
	{
		case PROP_CHILD_MODEL:
			tree_model_generator->priv->child_model = g_value_get_object (value);
			g_object_ref (tree_model_generator->priv->child_model);

			if (tree_model_generator->priv->root_nodes)
				release_node_map (tree_model_generator->priv->root_nodes);
			tree_model_generator->priv->root_nodes =
				build_node_map (tree_model_generator, NULL, NULL, -1);

			g_signal_connect_swapped (
				tree_model_generator->priv->child_model, "row-changed",
				G_CALLBACK (child_row_changed), tree_model_generator);
			g_signal_connect_swapped (
				tree_model_generator->priv->child_model, "row-deleted",
				G_CALLBACK (child_row_deleted), tree_model_generator);
			g_signal_connect_swapped (
				tree_model_generator->priv->child_model, "row-inserted",
				G_CALLBACK (child_row_inserted), tree_model_generator);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
tree_model_generator_get_property (GObject *object,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
	ETreeModelGenerator *tree_model_generator = E_TREE_MODEL_GENERATOR (object);

	switch (prop_id)
	{
		case PROP_CHILD_MODEL:
			g_value_set_object (value, tree_model_generator->priv->child_model);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
tree_model_generator_finalize (GObject *object)
{
	ETreeModelGenerator *tree_model_generator = E_TREE_MODEL_GENERATOR (object);

	if (tree_model_generator->priv->child_model) {
		g_signal_handlers_disconnect_matched (
			tree_model_generator->priv->child_model,
			G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL,
			tree_model_generator);
		g_object_unref (tree_model_generator->priv->child_model);
	}

	if (tree_model_generator->priv->root_nodes)
		release_node_map (tree_model_generator->priv->root_nodes);

	g_slist_free_full (tree_model_generator->priv->offset_cache, g_free);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_tree_model_generator_parent_class)->finalize (object);
}

static void
e_tree_model_generator_class_init (ETreeModelGeneratorClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = tree_model_generator_get_property;
	object_class->set_property = tree_model_generator_set_property;
	object_class->finalize = tree_model_generator_finalize;

	g_object_class_install_property (
		object_class,
		PROP_CHILD_MODEL,
		g_param_spec_object (
			"child-model",
			"Child Model",
			"The child model to extend",
			G_TYPE_OBJECT,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
e_tree_model_generator_tree_model_init (GtkTreeModelIface *iface)
{
	iface->get_flags = e_tree_model_generator_get_flags;
	iface->get_n_columns = e_tree_model_generator_get_n_columns;
	iface->get_column_type = e_tree_model_generator_get_column_type;
	iface->get_iter = e_tree_model_generator_get_iter;
	iface->get_path = e_tree_model_generator_get_path;
	iface->get_value = e_tree_model_generator_get_value;
	iface->iter_next = e_tree_model_generator_iter_next;
	iface->iter_children = e_tree_model_generator_iter_children;
	iface->iter_has_child = e_tree_model_generator_iter_has_child;
	iface->iter_n_children = e_tree_model_generator_iter_n_children;
	iface->iter_nth_child = e_tree_model_generator_iter_nth_child;
	iface->iter_parent = e_tree_model_generator_iter_parent;
}

static void
e_tree_model_generator_init (ETreeModelGenerator *tree_model_generator)
{
	tree_model_generator->priv = e_tree_model_generator_get_instance_private (tree_model_generator);

	tree_model_generator->priv->stamp = g_random_int ();
	tree_model_generator->priv->root_nodes = g_array_new (FALSE, FALSE, sizeof (Node));
}

/* ------------------ *
 * Row update helpers *
 * ------------------ */

static void
row_deleted (ETreeModelGenerator *tree_model_generator,
             GtkTreePath *path)
{
	g_return_if_fail (path);

	ETMG_DEBUG (g_print ("row_deleted emitting\n"));
	gtk_tree_model_row_deleted (GTK_TREE_MODEL (tree_model_generator), path);
}

static void
row_inserted (ETreeModelGenerator *tree_model_generator,
              GtkTreePath *path)
{
	GtkTreeIter iter;

	g_return_if_fail (path);

	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (tree_model_generator), &iter, path)) {
		ETMG_DEBUG (g_print ("row_inserted emitting\n"));
		gtk_tree_model_row_inserted (GTK_TREE_MODEL (tree_model_generator), path, &iter);
	} else {
		ETMG_DEBUG (g_print ("row_inserted could not get iter!\n"));
	}
}

static void
row_changed (ETreeModelGenerator *tree_model_generator,
             GtkTreePath *path)
{
	GtkTreeIter iter;

	g_return_if_fail (path);

	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (tree_model_generator), &iter, path)) {
		ETMG_DEBUG (g_print ("row_changed emitting\n"));
		gtk_tree_model_row_changed (GTK_TREE_MODEL (tree_model_generator), path, &iter);
	} else {
		ETMG_DEBUG (g_print ("row_changed could not get iter!\n"));
	}
}

/* -------------------- *
 * Node map translation *
 * -------------------- */

static gint
generated_offset_to_child_offset (GArray *group,
                                  gint offset,
                                  gint *internal_offset,
                                  GSList **cache_p)
{
	gboolean success = FALSE;
	gint     accum_offset = 0;
	gint     i;
	GSList *cache, *cache_last;
	gint last_cached_offset;

	i = 0;
	cache = *cache_p;
	cache_last = NULL;
	last_cached_offset = 0;
	for (; cache; cache = cache->next) {
		CacheItem *item = cache->data;
		cache_last = cache;
		last_cached_offset = item->offset;
		if (item->offset <= offset) {
			i = item->index;
			accum_offset = item->offset;
		} else
			break;
	}

	for (; i < group->len; i++) {
		Node *node = &g_array_index (group, Node, i);

		if (accum_offset - last_cached_offset > 500) {
			CacheItem *item = g_malloc (sizeof (CacheItem));
			item->offset = accum_offset;
			item->index = i;
			last_cached_offset = accum_offset;
			if (cache_last)
				cache_last = g_slist_last (g_slist_append (cache_last, item));
			else
				*cache_p = cache_last = g_slist_append (NULL, item);
		}

		accum_offset += node->n_generated;
		if (accum_offset > offset) {
			accum_offset -= node->n_generated;
			success = TRUE;
			break;
		}
	}

	if (!success)
		return -1;

	if (internal_offset)
		*internal_offset = offset - accum_offset;

	return i;
}

static gint
child_offset_to_generated_offset (GArray *group,
                                  gint offset)
{
	gint accum_offset = 0;
	gint i;

	g_return_val_if_fail (group != NULL, -1);

	for (i = 0; i < group->len && i < offset; i++) {
		Node *node = &g_array_index (group, Node, i);

		accum_offset += node->n_generated;
	}

	return accum_offset;
}

static gint
count_generated_nodes (GArray *group)
{
	gint accum_offset = 0;
	gint i;

	for (i = 0; i < group->len; i++) {
		Node *node = &g_array_index (group, Node, i);

		accum_offset += node->n_generated;
	}

	return accum_offset;
}

/* ------------------- *
 * Node map management *
 * ------------------- */

static void
release_node_map (GArray *group)
{
	gint i;

	for (i = 0; i < group->len; i++) {
		Node *node = &g_array_index (group, Node, i);

		if (node->child_nodes)
			release_node_map (node->child_nodes);
	}

	g_array_free (group, TRUE);
}

static gint
append_node (GArray *group)
{
	g_array_set_size (group, group->len + 1);
	return group->len - 1;
}

static GArray *
build_node_map (ETreeModelGenerator *tree_model_generator,
                GtkTreeIter *parent_iter,
                GArray *parent_group,
                gint parent_index)
{
	GArray      *group;
	GtkTreeIter  iter;
	gboolean     result;

	g_slist_free_full (tree_model_generator->priv->offset_cache, g_free);
	tree_model_generator->priv->offset_cache = NULL;

	if (parent_iter)
		result = gtk_tree_model_iter_children (tree_model_generator->priv->child_model, &iter, parent_iter);
	else
		result = gtk_tree_model_get_iter_first (tree_model_generator->priv->child_model, &iter);

	if (!result)
		return NULL;

	group = g_array_new (FALSE, FALSE, sizeof (Node));

	do {
		Node *node;
		gint  i;

		i = append_node (group);
		node = &g_array_index (group, Node, i);

		node->parent_group = parent_group;
		node->parent_index = parent_index;

		if (tree_model_generator->priv->generate_func)
			node->n_generated =
				tree_model_generator->priv->generate_func (tree_model_generator->priv->child_model,
								     &iter, tree_model_generator->priv->generate_func_data);
		else
			node->n_generated = 1;

		node->child_nodes = build_node_map (tree_model_generator, &iter, group, i);
	} while (gtk_tree_model_iter_next (tree_model_generator->priv->child_model, &iter));

	return group;
}

static gint
get_first_visible_index_from (GArray *group,
                              gint index)
{
	gint i;

	for (i = index; i < group->len; i++) {
		Node *node = &g_array_index (group, Node, i);

		if (node->n_generated)
			break;
	}

	if (i >= group->len)
		i = -1;

	return i;
}

static Node *
get_node_by_child_path (ETreeModelGenerator *tree_model_generator,
                        GtkTreePath *path,
                        GArray **node_group)
{
	Node   *node = NULL;
	GArray *group;
	gint    depth;

	group = tree_model_generator->priv->root_nodes;

	for (depth = 0; depth < gtk_tree_path_get_depth (path); depth++) {
		gint  index;

		if (!group) {
			g_warning ("ETreeModelGenerator got unknown child element!");
			break;
		}

		index = gtk_tree_path_get_indices (path)[depth];
		node = &g_array_index (group, Node, index);

		if (depth + 1 < gtk_tree_path_get_depth (path))
		    group = node->child_nodes;
	}

	if (!node)
		group = NULL;

	if (node_group)
		*node_group = group;

	return node;
}

static Node *
create_node_at_child_path (ETreeModelGenerator *tree_model_generator,
                           GtkTreePath *path)
{
	GtkTreePath *parent_path;
	gint         parent_index;
	GArray      *parent_group;
	GArray      *group;
	gint         index;
	Node        *node;

	parent_path = gtk_tree_path_copy (path);
	gtk_tree_path_up (parent_path);
	node = get_node_by_child_path (tree_model_generator, parent_path, &parent_group);

	if (node) {
		if (!node->child_nodes)
			node->child_nodes = g_array_new (FALSE, FALSE, sizeof (Node));

		group = node->child_nodes;
		parent_index = gtk_tree_path_get_indices (parent_path)[gtk_tree_path_get_depth (parent_path) - 1];
	} else {
		if (!tree_model_generator->priv->root_nodes)
			tree_model_generator->priv->root_nodes = g_array_new (FALSE, FALSE, sizeof (Node));

		group = tree_model_generator->priv->root_nodes;
		parent_index = -1;
	}

	gtk_tree_path_free (parent_path);

	index = gtk_tree_path_get_indices (path)[gtk_tree_path_get_depth (path) - 1];
	ETMG_DEBUG (g_print ("Inserting index %d into group of length %d\n", index, group->len));
	index = MIN (index, group->len);

	append_node (group);

	g_slist_free_full (tree_model_generator->priv->offset_cache, g_free);
	tree_model_generator->priv->offset_cache = NULL;

	if (group->len - 1 - index > 0) {
		gint i;

		memmove (
			(Node *) group->data + index + 1,
			(Node *) group->data + index,
			(group->len - 1 - index) * sizeof (Node));

		/* Update parent pointers */
		for (i = index + 1; i < group->len; i++) {
			Node   *pnode = &g_array_index (group, Node, i);
			GArray *child_group;
			gint    j;

			child_group = pnode->child_nodes;
			if (!child_group)
				continue;

			for (j = 0; j < child_group->len; j++) {
				Node *child_node = &g_array_index (child_group, Node, j);
				child_node->parent_index = i;
			}
		}
	}

	node = &g_array_index (group, Node, index);
	node->parent_group = parent_group;
	node->parent_index = parent_index;
	node->n_generated = 0;
	node->child_nodes = NULL;

	ETMG_DEBUG (
		g_print ("Created node at offset %d, parent_group = %p, parent_index = %d\n",
		index, node->parent_group, node->parent_index));

	return node;
}

ETMG_DEBUG (

static void
dump_group (GArray *group)
{
	gint i;

	g_print ("\nGroup %p:\n", group);

	for (i = 0; i < group->len; i++) {
		Node *node = &g_array_index (group, Node, i);
		g_print (
			"  %04d: pgroup=%p, pindex=%d, n_generated=%d, child_nodes=%p\n",
			i, node->parent_group, node->parent_index, node->n_generated, node->child_nodes);
	}
}

)

static void
delete_node_at_child_path (ETreeModelGenerator *tree_model_generator,
                           GtkTreePath *path)
{
	GtkTreePath *parent_path;
	GArray      *parent_group;
	GArray      *group;
	gint         index;
	Node        *node;
	gint         i;

	g_slist_free_full (tree_model_generator->priv->offset_cache, g_free);
	tree_model_generator->priv->offset_cache = NULL;

	parent_path = gtk_tree_path_copy (path);
	gtk_tree_path_up (parent_path);
	node = get_node_by_child_path (tree_model_generator, parent_path, &parent_group);

	if (node) {
		group = node->child_nodes;
	} else {
		group = tree_model_generator->priv->root_nodes;
	}

	gtk_tree_path_free (parent_path);

	if (!group)
		return;

	index = gtk_tree_path_get_indices (path)[gtk_tree_path_get_depth (path) - 1];
	if (index >= group->len)
		return;

	node = &g_array_index (group, Node, index);
	if (node->child_nodes)
		release_node_map (node->child_nodes);
	g_array_remove_index (group, index);

	/* Update parent pointers */
	for (i = index; i < group->len; i++) {
		Node   *pnode = &g_array_index (group, Node, i);
		GArray *child_group;
		gint    j;

		child_group = pnode->child_nodes;
		if (!child_group)
			continue;

		for (j = 0; j < child_group->len; j++) {
			Node *child_node = &g_array_index (child_group, Node, j);
			child_node->parent_index = i;
		}
	}
}

static void
child_row_changed (ETreeModelGenerator *tree_model_generator,
                   GtkTreePath *path,
                   GtkTreeIter *iter)
{
	GtkTreePath *generated_path;
	Node        *node;
	gint         n_generated;
	gint         i;

	if (tree_model_generator->priv->generate_func)
		n_generated =
			tree_model_generator->priv->generate_func (tree_model_generator->priv->child_model,
							     iter, tree_model_generator->priv->generate_func_data);
	else
		n_generated = 1;

	node = get_node_by_child_path (tree_model_generator, path, NULL);
	if (!node)
		return;

	generated_path = e_tree_model_generator_convert_child_path_to_path (tree_model_generator, path);

	/* FIXME: Converting the path to an iter every time is inefficient */

	for (i = 0; i < n_generated && i < node->n_generated; i++) {
		row_changed (tree_model_generator, generated_path);
		gtk_tree_path_next (generated_path);
	}

	if (n_generated != node->n_generated) {
		g_slist_free_full (tree_model_generator->priv->offset_cache, g_free);
		tree_model_generator->priv->offset_cache = NULL;
	}

	for (; i < node->n_generated; ) {
		node->n_generated--;
		row_deleted (tree_model_generator, generated_path);
	}

	for (; i < n_generated; i++) {
		node->n_generated++;
		row_inserted (tree_model_generator, generated_path);
		gtk_tree_path_next (generated_path);
	}

	gtk_tree_path_free (generated_path);
}

static void
child_row_inserted (ETreeModelGenerator *tree_model_generator,
                    GtkTreePath *path,
                    GtkTreeIter *iter)
{
	GtkTreePath *generated_path;
	Node        *node;
	gint         n_generated;

	if (tree_model_generator->priv->generate_func)
		n_generated =
			tree_model_generator->priv->generate_func (tree_model_generator->priv->child_model,
							     iter, tree_model_generator->priv->generate_func_data);
	else
		n_generated = 1;

	node = create_node_at_child_path (tree_model_generator, path);
	if (!node)
		return;

	generated_path = e_tree_model_generator_convert_child_path_to_path (tree_model_generator, path);

	/* FIXME: Converting the path to an iter every time is inefficient */

	for (node->n_generated = 0; node->n_generated < n_generated; ) {
		node->n_generated++;
		row_inserted (tree_model_generator, generated_path);
		gtk_tree_path_next (generated_path);
	}

	gtk_tree_path_free (generated_path);
}

static void
child_row_deleted (ETreeModelGenerator *tree_model_generator,
                   GtkTreePath *path)
{
	GtkTreePath *generated_path;
	Node        *node;

	node = get_node_by_child_path (tree_model_generator, path, NULL);
	if (!node)
		return;

	generated_path = e_tree_model_generator_convert_child_path_to_path (tree_model_generator, path);

	/* FIXME: Converting the path to an iter every time is inefficient */

	for (; node->n_generated; ) {
		node->n_generated--;
		row_deleted (tree_model_generator, generated_path);
	}

	delete_node_at_child_path (tree_model_generator, path);
	gtk_tree_path_free (generated_path);
}

/* ----------------------- *
 * ETreeModelGenerator API *
 * ----------------------- */

/**
 * e_tree_model_generator_new:
 * @child_model: a #GtkTreeModel
 *
 * Creates a new #ETreeModelGenerator wrapping @child_model.
 *
 * Returns: A new #ETreeModelGenerator.
 **/
ETreeModelGenerator *
e_tree_model_generator_new (GtkTreeModel *child_model)
{
	g_return_val_if_fail (GTK_IS_TREE_MODEL (child_model), NULL);

	return E_TREE_MODEL_GENERATOR (
		g_object_new (E_TYPE_TREE_MODEL_GENERATOR,
		"child-model", child_model, NULL));
}

/**
 * e_tree_model_generator_get_model:
 * @tree_model_generator: an #ETreeModelGenerator
 *
 * Gets the child model being wrapped by @tree_model_generator.
 *
 * Returns: A #GtkTreeModel being wrapped.
 **/
GtkTreeModel *
e_tree_model_generator_get_model (ETreeModelGenerator *tree_model_generator)
{
	g_return_val_if_fail (E_IS_TREE_MODEL_GENERATOR (tree_model_generator), NULL);

	return tree_model_generator->priv->child_model;
}

/**
 * e_tree_model_generator_set_generate_func:
 * @tree_model_generator: an #ETreeModelGenerator
 * @func: an #ETreeModelGeneratorGenerateFunc, or %NULL
 * @data: user data to pass to @func
 * @destroy:
 *
 * Sets the callback function used to filter or generate additional rows
 * based on the child model's data. This function is called for each child
 * row, and returns a value indicating the number of rows that will be
 * used to represent the child row - 0 or more.
 *
 * If @func is %NULL, a filtering/generating function will not be applied.
 **/
void
e_tree_model_generator_set_generate_func (ETreeModelGenerator *tree_model_generator,
                                          ETreeModelGeneratorGenerateFunc func,
                                          gpointer data,
                                          GDestroyNotify destroy)
{
	g_return_if_fail (E_IS_TREE_MODEL_GENERATOR (tree_model_generator));

	tree_model_generator->priv->generate_func = func;
	tree_model_generator->priv->generate_func_data = data;
}

/**
 * e_tree_model_generator_set_modify_func:
 * @tree_model_generator: an #ETreeModelGenerator
 * @func: an @ETreeModelGeneratorModifyFunc, or %NULL
 * @data: user data to pass to @func
 * @destroy:
 *
 * Sets the callback function used to override values for the child row's
 * columns and specify values for generated rows' columns.
 *
 * If @func is %NULL, the child model's values will always be used.
 **/
void
e_tree_model_generator_set_modify_func (ETreeModelGenerator *tree_model_generator,
                                        ETreeModelGeneratorModifyFunc func,
                                        gpointer data,
                                        GDestroyNotify destroy)
{
	g_return_if_fail (E_IS_TREE_MODEL_GENERATOR (tree_model_generator));

	tree_model_generator->priv->modify_func = func;
	tree_model_generator->priv->modify_func_data = data;
}

/**
 * e_tree_model_generator_convert_child_path_to_path:
 * @tree_model_generator: an #ETreeModelGenerator
 * @child_path: a #GtkTreePath
 *
 * Convert a path to a child row to a path to a @tree_model_generator row.
 *
 * Returns: A new GtkTreePath, owned by the caller.
 **/
GtkTreePath *
e_tree_model_generator_convert_child_path_to_path (ETreeModelGenerator *tree_model_generator,
                                                   GtkTreePath *child_path)
{
	GtkTreePath *path;
	GArray      *group;
	gint         depth;

	g_return_val_if_fail (E_IS_TREE_MODEL_GENERATOR (tree_model_generator), NULL);
	g_return_val_if_fail (child_path != NULL, NULL);

	path = gtk_tree_path_new ();

	group = tree_model_generator->priv->root_nodes;

	for (depth = 0; depth < gtk_tree_path_get_depth (child_path); depth++) {
		Node *node;
		gint  index;
		gint  generated_index;

		if (!group) {
			g_warning ("ETreeModelGenerator was asked for path to unknown child element!");
			break;
		}

		index = gtk_tree_path_get_indices (child_path)[depth];
		generated_index = child_offset_to_generated_offset (group, index);
		node = &g_array_index (group, Node, index);
		group = node->child_nodes;

		gtk_tree_path_append_index (path, generated_index);
	}

	return path;
}

/**
 * e_tree_model_generator_convert_child_iter_to_iter:
 * @tree_model_generator: an #ETreeModelGenerator
 * @generator_iter: a #GtkTreeIter to set
 * @child_iter: a #GtkTreeIter to convert
 *
 * Convert @child_iter to a corresponding #GtkTreeIter for @tree_model_generator,
 * storing the result in @generator_iter.
 **/
void
e_tree_model_generator_convert_child_iter_to_iter (ETreeModelGenerator *tree_model_generator,
                                                   GtkTreeIter *generator_iter,
                                                   GtkTreeIter *child_iter)
{
	GtkTreePath *path;
	GArray      *group;
	gint         depth;
	gint         index = 0;

	g_return_if_fail (E_IS_TREE_MODEL_GENERATOR (tree_model_generator));

	path = gtk_tree_model_get_path (tree_model_generator->priv->child_model, child_iter);
	if (!path)
		return;

	group = tree_model_generator->priv->root_nodes;

	for (depth = 0; depth < gtk_tree_path_get_depth (path); depth++) {
		Node *node;

		index = gtk_tree_path_get_indices (path)[depth];
		node = &g_array_index (group, Node, index);

		if (depth + 1 < gtk_tree_path_get_depth (path))
			group = node->child_nodes;

		if (!group) {
			g_warning ("ETreeModelGenerator was asked for iter to unknown child element!");
			break;
		}
	}

	g_return_if_fail (group != NULL);

	index = child_offset_to_generated_offset (group, index);
	ITER_SET (tree_model_generator, generator_iter, group, index);
	gtk_tree_path_free (path);
}

/**
 * e_tree_model_generator_convert_path_to_child_path:
 * @tree_model_generator: an #ETreeModelGenerator
 * @generator_path: a #GtkTreePath to a @tree_model_generator row
 *
 * Converts @generator_path to a corresponding #GtkTreePath in the child model.
 *
 * Returns: A new #GtkTreePath, owned by the caller.
 **/
GtkTreePath *
e_tree_model_generator_convert_path_to_child_path (ETreeModelGenerator *tree_model_generator,
                                                   GtkTreePath *generator_path)
{
	GtkTreePath *path;
	GArray      *group;
	gint         depth;

	g_return_val_if_fail (E_IS_TREE_MODEL_GENERATOR (tree_model_generator), NULL);
	g_return_val_if_fail (generator_path != NULL, NULL);

	path = gtk_tree_path_new ();

	group = tree_model_generator->priv->root_nodes;

	for (depth = 0; depth < gtk_tree_path_get_depth (generator_path); depth++) {
		Node *node;
		gint  index;
		gint  child_index;

		if (!group) {
			g_warning ("ETreeModelGenerator was asked for path to unknown child element!");
			break;
		}

		index = gtk_tree_path_get_indices (generator_path)[depth];
		child_index = generated_offset_to_child_offset (group, index, NULL, &tree_model_generator->priv->offset_cache);
		node = &g_array_index (group, Node, child_index);
		group = node->child_nodes;

		gtk_tree_path_append_index (path, child_index);
	}

	return path;
}

/**
 * e_tree_model_generator_convert_iter_to_child_iter:
 * @tree_model_generator: an #ETreeModelGenerator
 * @child_iter: a #GtkTreeIter to set
 * @permutation_n: a permutation index to set
 * @generator_iter: a #GtkTreeIter indicating the row to convert
 *
 * Converts a @tree_model_generator row into a child row and permutation index.
 * The permutation index is the index of the generated row based on this
 * child row, with the first generated row based on this child row being 0.
 **/
gboolean
e_tree_model_generator_convert_iter_to_child_iter (ETreeModelGenerator *tree_model_generator,
                                                   GtkTreeIter *child_iter,
                                                   gint *permutation_n,
                                                   GtkTreeIter *generator_iter)
{
	GtkTreePath *path;
	GArray      *group;
	gint         index;
	gint         internal_offset = 0;
	gboolean     iter_is_valid = FALSE;

	g_return_val_if_fail (E_IS_TREE_MODEL_GENERATOR (tree_model_generator), iter_is_valid);
	g_return_val_if_fail (ITER_IS_VALID (tree_model_generator, generator_iter), iter_is_valid);

	path = gtk_tree_path_new ();
	ITER_GET (generator_iter, &group, &index);

	index = generated_offset_to_child_offset (group, index, &internal_offset, &tree_model_generator->priv->offset_cache);
	gtk_tree_path_prepend_index (path, index);

	while (group) {
		Node *node = &g_array_index (group, Node, index);

		group = node->parent_group;
		index = node->parent_index;

		if (group)
			gtk_tree_path_prepend_index (path, index);
	}

	if (child_iter)
		iter_is_valid = gtk_tree_model_get_iter (tree_model_generator->priv->child_model, child_iter, path);

	if (permutation_n)
		*permutation_n = internal_offset;

	gtk_tree_path_free (path);

	return iter_is_valid;
}

/* ---------------- *
 * GtkTreeModel API *
 * ---------------- */

static GtkTreeModelFlags
e_tree_model_generator_get_flags (GtkTreeModel *tree_model)
{
	ETreeModelGenerator *tree_model_generator = E_TREE_MODEL_GENERATOR (tree_model);

	g_return_val_if_fail (E_IS_TREE_MODEL_GENERATOR (tree_model), 0);

	return gtk_tree_model_get_flags (tree_model_generator->priv->child_model);
}

static gint
e_tree_model_generator_get_n_columns (GtkTreeModel *tree_model)
{
	ETreeModelGenerator *tree_model_generator = E_TREE_MODEL_GENERATOR (tree_model);

	g_return_val_if_fail (E_IS_TREE_MODEL_GENERATOR (tree_model), 0);

	return gtk_tree_model_get_n_columns (tree_model_generator->priv->child_model);
}

static GType
e_tree_model_generator_get_column_type (GtkTreeModel *tree_model,
                                        gint index)
{
	ETreeModelGenerator *tree_model_generator = E_TREE_MODEL_GENERATOR (tree_model);

	g_return_val_if_fail (E_IS_TREE_MODEL_GENERATOR (tree_model), G_TYPE_INVALID);

	return gtk_tree_model_get_column_type (tree_model_generator->priv->child_model, index);
}

static gboolean
e_tree_model_generator_get_iter (GtkTreeModel *tree_model,
                                 GtkTreeIter *iter,
                                 GtkTreePath *path)
{
	ETreeModelGenerator *tree_model_generator = E_TREE_MODEL_GENERATOR (tree_model);
	GArray              *group;
	gint                 depth;
	gint                 index = 0;

	g_return_val_if_fail (E_IS_TREE_MODEL_GENERATOR (tree_model), FALSE);
	g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

	group = tree_model_generator->priv->root_nodes;
	if (!group)
		return FALSE;

	for (depth = 0; depth < gtk_tree_path_get_depth (path); depth++) {
		Node *node;
		gint  child_index;

		index = gtk_tree_path_get_indices (path)[depth];
		child_index = generated_offset_to_child_offset (group, index, NULL, &tree_model_generator->priv->offset_cache);
		if (child_index < 0)
			return FALSE;

		node = &g_array_index (group, Node, child_index);

		if (depth + 1 < gtk_tree_path_get_depth (path)) {
			group = node->child_nodes;
			if (!group)
				return FALSE;
		}
	}

	ITER_SET (tree_model_generator, iter, group, index);
	return TRUE;
}

static GtkTreePath *
e_tree_model_generator_get_path (GtkTreeModel *tree_model,
                                 GtkTreeIter *iter)
{
	ETreeModelGenerator *tree_model_generator = E_TREE_MODEL_GENERATOR (tree_model);
	GtkTreePath         *path;
	GArray              *group;
	gint                 index;

	g_return_val_if_fail (E_IS_TREE_MODEL_GENERATOR (tree_model), NULL);
	g_return_val_if_fail (ITER_IS_VALID (tree_model_generator, iter), NULL);

	ITER_GET (iter, &group, &index);
	path = gtk_tree_path_new ();

	/* FIXME: Converting a path to an iter is a destructive operation, because
	 * we don't store a node for each generated entry... Doesn't matter for
	 * lists, not sure about trees. */

	gtk_tree_path_prepend_index (path, index);
	index = generated_offset_to_child_offset (group, index, NULL, &tree_model_generator->priv->offset_cache);

	while (group) {
		Node *node = &g_array_index (group, Node, index);
		gint  generated_index;

		group = node->parent_group;
		index = node->parent_index;
		if (group) {
			generated_index = child_offset_to_generated_offset (group, index);
			gtk_tree_path_prepend_index (path, generated_index);
		}
	}

	return path;
}

static gboolean
e_tree_model_generator_iter_next (GtkTreeModel *tree_model,
                                  GtkTreeIter *iter)
{
	ETreeModelGenerator *tree_model_generator = E_TREE_MODEL_GENERATOR (tree_model);
	Node                *node;
	GArray              *group;
	gint                 index;
	gint                 child_index;
	gint                 internal_offset = 0;

	g_return_val_if_fail (E_IS_TREE_MODEL_GENERATOR (tree_model), FALSE);
	g_return_val_if_fail (ITER_IS_VALID (tree_model_generator, iter), FALSE);

	ITER_GET (iter, &group, &index);
	child_index = generated_offset_to_child_offset (group, index, &internal_offset, &tree_model_generator->priv->offset_cache);
	node = &g_array_index (group, Node, child_index);

	if (internal_offset + 1 < node->n_generated ||
	    get_first_visible_index_from (group, child_index + 1) >= 0) {
		ITER_SET (tree_model_generator, iter, group, index + 1);
		return TRUE;
	}

	return FALSE;
}

static gboolean
e_tree_model_generator_iter_children (GtkTreeModel *tree_model,
                                      GtkTreeIter *iter,
                                      GtkTreeIter *parent)
{
	ETreeModelGenerator *tree_model_generator = E_TREE_MODEL_GENERATOR (tree_model);
	Node                *node;
	GArray              *group;
	gint                 index;

	g_return_val_if_fail (E_IS_TREE_MODEL_GENERATOR (tree_model), FALSE);

	if (!parent) {
		if (!tree_model_generator->priv->root_nodes ||
		    !count_generated_nodes (tree_model_generator->priv->root_nodes))
			return FALSE;

		ITER_SET (tree_model_generator, iter, tree_model_generator->priv->root_nodes, 0);
		return TRUE;
	}

	ITER_GET (parent, &group, &index);
	index = generated_offset_to_child_offset (group, index, NULL, &tree_model_generator->priv->offset_cache);
	if (index < 0)
		return FALSE;

	node = &g_array_index (group, Node, index);

	if (!node->child_nodes)
		return FALSE;

	if (!count_generated_nodes (node->child_nodes))
		return FALSE;

	ITER_SET (tree_model_generator, iter, node->child_nodes, 0);
	return TRUE;
}

static gboolean
e_tree_model_generator_iter_has_child (GtkTreeModel *tree_model,
                                       GtkTreeIter *iter)
{
	ETreeModelGenerator *tree_model_generator = E_TREE_MODEL_GENERATOR (tree_model);
	Node                *node;
	GArray              *group;
	gint                 index;

	g_return_val_if_fail (E_IS_TREE_MODEL_GENERATOR (tree_model), FALSE);

	if (iter == NULL) {
		if (!tree_model_generator->priv->root_nodes ||
		    !count_generated_nodes (tree_model_generator->priv->root_nodes))
			return FALSE;

		return TRUE;
	}

	ITER_GET (iter, &group, &index);
	index = generated_offset_to_child_offset (group, index, NULL, &tree_model_generator->priv->offset_cache);
	if (index < 0)
		return FALSE;

	node = &g_array_index (group, Node, index);

	if (!node->child_nodes)
		return FALSE;

	if (!count_generated_nodes (node->child_nodes))
		return FALSE;

	return TRUE;
}

static gint
e_tree_model_generator_iter_n_children (GtkTreeModel *tree_model,
                                        GtkTreeIter *iter)
{
	ETreeModelGenerator *tree_model_generator = E_TREE_MODEL_GENERATOR (tree_model);
	Node                *node;
	GArray              *group;
	gint                 index;

	g_return_val_if_fail (E_IS_TREE_MODEL_GENERATOR (tree_model), 0);

	if (iter == NULL)
		return tree_model_generator->priv->root_nodes ?
			count_generated_nodes (tree_model_generator->priv->root_nodes) : 0;

	ITER_GET (iter, &group, &index);
	index = generated_offset_to_child_offset (group, index, NULL, &tree_model_generator->priv->offset_cache);
	if (index < 0)
		return 0;

	node = &g_array_index (group, Node, index);

	if (!node->child_nodes)
		return 0;

	return count_generated_nodes (node->child_nodes);
}

static gboolean
e_tree_model_generator_iter_nth_child (GtkTreeModel *tree_model,
                                       GtkTreeIter *iter,
                                       GtkTreeIter *parent,
                                       gint n)
{
	ETreeModelGenerator *tree_model_generator = E_TREE_MODEL_GENERATOR (tree_model);
	Node                *node;
	GArray              *group;
	gint                 index;

	g_return_val_if_fail (E_IS_TREE_MODEL_GENERATOR (tree_model), FALSE);

	if (!parent) {
		if (!tree_model_generator->priv->root_nodes)
			return FALSE;

		if (n >= count_generated_nodes (tree_model_generator->priv->root_nodes))
			return FALSE;

		ITER_SET (tree_model_generator, iter, tree_model_generator->priv->root_nodes, n);
		return TRUE;
	}

	ITER_GET (parent, &group, &index);
	index = generated_offset_to_child_offset (group, index, NULL, &tree_model_generator->priv->offset_cache);
	if (index < 0)
		return FALSE;

	node = &g_array_index (group, Node, index);

	if (!node->child_nodes)
		return FALSE;

	if (n >= count_generated_nodes (node->child_nodes))
		return FALSE;

	ITER_SET (tree_model_generator, iter, node->child_nodes, n);
	return TRUE;
}

static gboolean
e_tree_model_generator_iter_parent (GtkTreeModel *tree_model,
                                    GtkTreeIter *iter,
                                    GtkTreeIter *child)
{
	ETreeModelGenerator *tree_model_generator = E_TREE_MODEL_GENERATOR (tree_model);
	Node                *node;
	GArray              *group;
	gint                 index;

	g_return_val_if_fail (E_IS_TREE_MODEL_GENERATOR (tree_model), FALSE);
	g_return_val_if_fail (ITER_IS_VALID (tree_model_generator, iter), FALSE);

	ITER_GET (child, &group, &index);
	index = generated_offset_to_child_offset (group, index, NULL, &tree_model_generator->priv->offset_cache);
	if (index < 0)
		return FALSE;

	node = &g_array_index (group, Node, index);

	group = node->parent_group;
	if (!group)
		return FALSE;

	ITER_SET (tree_model_generator, iter, group, node->parent_index);
	return TRUE;
}

static void
e_tree_model_generator_get_value (GtkTreeModel *tree_model,
                                  GtkTreeIter *iter,
                                  gint column,
                                  GValue *value)
{
	ETreeModelGenerator *tree_model_generator = E_TREE_MODEL_GENERATOR (tree_model);
	GtkTreeIter          child_iter;
	gint                 permutation_n;

	g_return_if_fail (E_IS_TREE_MODEL_GENERATOR (tree_model));
	g_return_if_fail (ITER_IS_VALID (tree_model_generator, iter));

	e_tree_model_generator_convert_iter_to_child_iter (
		tree_model_generator, &child_iter,
		&permutation_n, iter);

	if (tree_model_generator->priv->modify_func) {
		tree_model_generator->priv->modify_func (tree_model_generator->priv->child_model,
						   &child_iter, permutation_n,
						   column, value,
						   tree_model_generator->priv->modify_func_data);
		return;
	}

	gtk_tree_model_get_value (tree_model_generator->priv->child_model, &child_iter, column, value);
}
