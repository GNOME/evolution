/* e-storage-set-store.c
 * Copyright (C) 2002 Ximian, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <string.h>

#include <gtk/gtktreemodel.h>
#include <gtk/gtktreednd.h>

#include "e-icon-factory.h"
#include "e-storage-set-store.h"
#include "e-shell-constants.h"

struct _EStorageSetStorePrivate {
	EStorageSet *storage_set;
	GHashTable *checkboxes;
	GHashTable *path_to_node;
	GHashTable *type_name_to_pixbuf;
	GNode *root;
	gint stamp;
	EStorageSetStoreHasCheckBoxFunc has_checkbox_func;
	gpointer has_checkbox_func_data;
};

#define G_NODE(node) ((GNode *)(node))
#define VALID_ITER(iter, store) ((iter) != NULL && (iter)->user_data != NULL && (store)->priv->stamp == (iter)->stamp)
#define VALID_COL(col) ((col) >= 0 && (col) < E_STORAGE_SET_STORE_COLUMN_COUNT)

static GObjectClass *parent_class = NULL;

static gboolean
has_checkbox (EStorageSetStore *store, const gchar *folder_path)
{
	EStorageSetStorePrivate *priv = store->priv;

	g_return_val_if_fail (folder_path != NULL, FALSE);

	if (strchr (folder_path + 1, '/') == NULL) {
		/* If it's a toplevel, never allow checking it.  */
		return FALSE;
	}

#if 0
	if (priv->has_checkbox_func)
		return (* priv->has_checkbox_func) (priv->storage_set,
						    folder_path,
						    priv->has_checkbox_func_data);
#endif

	return TRUE;
}


/* GtkTreeModel interface implementation */

static guint
esss_get_flags(GtkTreeModel *tree_model)
{
	g_return_val_if_fail(E_IS_STORAGE_SET_STORE(tree_model), 0);

	return GTK_TREE_MODEL_ITERS_PERSIST;
}

static gint
esss_get_n_columns(GtkTreeModel *tree_model)
{
	g_return_val_if_fail(E_IS_STORAGE_SET_STORE(tree_model), 0);

	return E_STORAGE_SET_STORE_COLUMN_COUNT;
}

static GType
esss_get_column_type(GtkTreeModel *tree_model, gint index)
{
	EStorageSetStore *store = (EStorageSetStore *) tree_model;
	GType retval;

	g_return_val_if_fail(E_IS_STORAGE_SET_STORE(tree_model), G_TYPE_INVALID);
	g_return_val_if_fail(VALID_COL(index), G_TYPE_INVALID);

	switch (index) {
	case E_STORAGE_SET_STORE_COLUMN_NAME:
		retval = G_TYPE_STRING;
		break;
	case E_STORAGE_SET_STORE_COLUMN_HIGHLIGHT:
		retval = G_TYPE_INT;
		break;
	case E_STORAGE_SET_STORE_COLUMN_CHECKED:
	case E_STORAGE_SET_STORE_COLUMN_CHECKABLE:
		retval = G_TYPE_BOOLEAN;
		break;
	case E_STORAGE_SET_STORE_COLUMN_ICON:
		retval = GDK_TYPE_PIXBUF;
		break;
	default:
		g_assert_not_reached ();
	}

	return retval;
}

static gboolean
esss_get_iter(GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreePath *path)
{
	EStorageSetStore *store = (EStorageSetStore *) tree_model;
	GtkTreeIter parent;
	gint *indices;
	gint depth, i;

	g_return_val_if_fail(E_IS_STORAGE_SET_STORE(tree_model), FALSE);
	g_return_val_if_fail(iter != NULL, FALSE);

	indices = gtk_tree_path_get_indices(path);
	depth = gtk_tree_path_get_depth(path);

	g_return_val_if_fail(depth > 0, FALSE);

	parent.stamp = store->priv->stamp;
	parent.user_data = store->priv->root;

	for (i = 0; i < depth; i++) {
		if (!gtk_tree_model_iter_nth_child (tree_model, iter, &parent, indices[i]))
			return FALSE;
		parent = *iter;
	}

	return TRUE;
}

static GtkTreePath *
tree_path_from_node (EStorageSetStore *store, GNode *node)
{
	GtkTreePath *retval;
	GNode *tmp_node, *curr_node;
	gint i = 0;

	if (node == store->priv->root)
		return NULL;

	retval = gtk_tree_path_new();

	for (curr_node = node; curr_node != store->priv->root; curr_node = curr_node->parent) {

		if (curr_node->parent == NULL) {
			gtk_tree_path_free(retval);
			return NULL;
		}

		for (i = 0, tmp_node = curr_node->parent->children; 
		     tmp_node && tmp_node != curr_node;
		     i++, tmp_node = tmp_node->next);

		if (tmp_node == NULL) {
			gtk_tree_path_free(retval);
			return NULL;
		}

		gtk_tree_path_prepend_index(retval, i);
	}

	return retval;
}

static GtkTreePath *
esss_get_path(GtkTreeModel *tree_model, GtkTreeIter *iter)
{
	EStorageSetStore *store = (EStorageSetStore *) tree_model;

	g_return_val_if_fail(E_IS_STORAGE_SET_STORE(tree_model), NULL);
	g_return_val_if_fail(VALID_ITER(iter, store), NULL);

	if (iter->user_data == store->priv->root)
		return NULL;

	return tree_path_from_node (store, G_NODE (iter->user_data));
}

static GdkPixbuf *
get_pixbuf_for_folder (EStorageSetStore *store, EFolder *folder)
{
	const char *type_name;
	EStorageSetStorePrivate *priv;
	EFolderTypeRegistry *folder_type_registry;
	EStorageSet *storage_set;
	GdkPixbuf *icon_pixbuf;
	GdkPixbuf *scaled_pixbuf;
	const char *custom_icon_name;
	int icon_pixbuf_width, icon_pixbuf_height;

	priv = store->priv;

	custom_icon_name = e_folder_get_custom_icon_name (folder);
	if (custom_icon_name != NULL)
		return e_icon_factory_get_icon (custom_icon_name, TRUE);

	type_name = e_folder_get_type_string (folder);

	scaled_pixbuf = g_hash_table_lookup (priv->type_name_to_pixbuf, type_name);
	if (scaled_pixbuf != NULL)
		return scaled_pixbuf;

	storage_set = priv->storage_set;
	folder_type_registry = e_storage_set_get_folder_type_registry (storage_set);

	icon_pixbuf = e_folder_type_registry_get_icon_for_type (folder_type_registry,
								type_name, TRUE);

	if (icon_pixbuf == NULL)
		return NULL;

	icon_pixbuf_width = gdk_pixbuf_get_width (icon_pixbuf);
	icon_pixbuf_height = gdk_pixbuf_get_height (icon_pixbuf);

	if (icon_pixbuf_width == E_SHELL_MINI_ICON_SIZE && icon_pixbuf_height == E_SHELL_MINI_ICON_SIZE) {
		scaled_pixbuf = g_object_ref (icon_pixbuf);
	} else {
		scaled_pixbuf = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (icon_pixbuf),
						gdk_pixbuf_get_has_alpha (icon_pixbuf),
						gdk_pixbuf_get_bits_per_sample (icon_pixbuf),
						E_SHELL_MINI_ICON_SIZE, E_SHELL_MINI_ICON_SIZE);

		gdk_pixbuf_scale (icon_pixbuf, scaled_pixbuf,
				  0, 0, E_SHELL_MINI_ICON_SIZE, E_SHELL_MINI_ICON_SIZE,
				  0.0, 0.0,
				  (double) E_SHELL_MINI_ICON_SIZE / gdk_pixbuf_get_width (icon_pixbuf),
				  (double) E_SHELL_MINI_ICON_SIZE / gdk_pixbuf_get_height (icon_pixbuf),
				  GDK_INTERP_HYPER);
	}

	g_hash_table_insert (priv->type_name_to_pixbuf, g_strdup (type_name), scaled_pixbuf);

	return scaled_pixbuf;
}

static void
esss_get_value(GtkTreeModel *tree_model, GtkTreeIter *iter, gint column, GValue *value)
{
	gchar *folder_path;
	const gchar *folder_name;
	int unread_count;
	EStorageSetStore *store = (EStorageSetStore *) tree_model;
	EFolder *folder;
	GNode *node;
	gboolean is_storage;

	g_return_if_fail(E_IS_STORAGE_SET_STORE(tree_model));
	g_return_if_fail(VALID_ITER(iter, store));
	g_return_if_fail(VALID_COL(column));

	node = G_NODE (iter->user_data);
	g_value_init(value, esss_get_column_type(tree_model, column));

	is_storage = (node->parent == store->priv->root);

	if (is_storage)
		folder_path = g_strconcat ("/", node->data, NULL);
	else
		folder_path = g_strdup (node->data);

	folder = e_storage_set_get_folder(store->priv->storage_set, folder_path);
	g_free (folder_path);

	switch (column) {
	case E_STORAGE_SET_STORE_COLUMN_NAME:
		if (!folder) {
			g_value_set_string(value, "?");
			return;
		}
		folder_name = e_folder_get_name(folder);
		unread_count = e_folder_get_unread_count(folder);
		if (unread_count > 0) {
			gchar *with_unread = g_strdup_printf("%s (%d)", folder_name, unread_count);
			g_object_set_data_full(G_OBJECT(folder), "name_with_unread", 
					       with_unread, g_free);
			g_value_set_string(value, with_unread);
		} else
			g_value_set_string(value, folder_name);
		break;

	case E_STORAGE_SET_STORE_COLUMN_HIGHLIGHT:
		if (!folder) {
			g_value_set_boolean(value, FALSE);
			return;
		}
		g_value_set_int(value, is_storage || e_folder_get_highlighted(folder) ? PANGO_WEIGHT_BOLD : 0);
		break;

	case E_STORAGE_SET_STORE_COLUMN_CHECKED:
		if (is_storage || !store->priv->checkboxes) {
			g_value_set_boolean(value, FALSE);
			return;
		}
		g_value_set_boolean(value, 
			g_hash_table_lookup(store->priv->checkboxes, folder_path) ? TRUE : FALSE);
		break;

	case E_STORAGE_SET_STORE_COLUMN_CHECKABLE:
		g_value_set_boolean(value, !is_storage && has_checkbox(store, (gchar *)node->data));
		break;

	case E_STORAGE_SET_STORE_COLUMN_ICON:
		if (!is_storage)
			g_value_set_object (value, get_pixbuf_for_folder (store, folder));
		break;

	default:
		g_assert_not_reached ();
		break;
	}
}

static gboolean
esss_iter_next(GtkTreeModel *tree_model, GtkTreeIter *iter)
{
	g_return_val_if_fail(E_IS_STORAGE_SET_STORE(tree_model), FALSE);
	g_return_val_if_fail(VALID_ITER(iter, E_STORAGE_SET_STORE(tree_model)), FALSE);

	if (G_NODE(iter->user_data)->next) {
		iter->user_data = G_NODE(iter->user_data)->next;
		return TRUE;
	} else
		return FALSE;
}

static gboolean
esss_iter_children(GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent)
{
	GNode *children;
	EStorageSetStore *store = (EStorageSetStore *) tree_model;

	g_return_val_if_fail(E_IS_STORAGE_SET_STORE(tree_model), FALSE);
	g_return_val_if_fail(iter != NULL, FALSE);
	g_return_val_if_fail(parent == NULL || parent->user_data != NULL, FALSE);
	g_return_val_if_fail(parent == NULL || parent->stamp == store->priv->stamp, FALSE);

	if (parent)
		children = G_NODE(parent->user_data)->children;
	else
		children =
		    G_NODE(store->priv->root)->children;

	if (children) {
		iter->stamp = store->priv->stamp;
		iter->user_data = children;
		return TRUE;
	} else
		return FALSE;
}

static gboolean
esss_iter_has_child(GtkTreeModel *tree_model, GtkTreeIter *iter)
{
	EStorageSetStore *store = (EStorageSetStore *) tree_model;

	g_return_val_if_fail(E_IS_STORAGE_SET_STORE(tree_model), FALSE);
	g_return_val_if_fail(VALID_ITER(iter, store), FALSE);

	return G_NODE(iter->user_data)->children != NULL;
}

static gint
esss_iter_n_children(GtkTreeModel *tree_model, GtkTreeIter *iter)
{
	GNode *node;
	gint i = 0;
	EStorageSetStore *store = (EStorageSetStore *) tree_model;

	g_return_val_if_fail(E_IS_STORAGE_SET_STORE(tree_model), 0);
	g_return_val_if_fail(iter == NULL || iter->user_data != NULL, FALSE);

	if (iter == NULL)
		node = G_NODE(store->priv->root)->children;
	else
		node = G_NODE(iter->user_data)->children;

	while (node) {
		i++;
		node = node->next;
	}

	return i;
}

static gboolean
esss_iter_nth_child(GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent, gint n)
{
	GNode *parent_node;
	GNode *child;
	EStorageSetStore *store = (EStorageSetStore *) tree_model;

	g_return_val_if_fail(E_IS_STORAGE_SET_STORE(tree_model), FALSE);
	g_return_val_if_fail(iter != NULL, FALSE);
	g_return_val_if_fail(parent == NULL || parent->user_data != NULL, FALSE);

	if (parent == NULL)
		parent_node = store->priv->root;
	else
		parent_node = parent->user_data;

	child = g_node_nth_child(parent_node, n);

	if (child) {
		iter->user_data = child;
		iter->stamp = store->priv->stamp;
		return TRUE;
	} else
		return FALSE;
}

static gboolean
esss_iter_parent(GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *child)
{
	GNode *parent;
	EStorageSetStore *store = (EStorageSetStore *) tree_model;

	g_return_val_if_fail(E_IS_STORAGE_SET_STORE(tree_model), FALSE);
	g_return_val_if_fail(iter != NULL, FALSE);
	g_return_val_if_fail(VALID_ITER(child, store), FALSE);

	parent = G_NODE(child->user_data)->parent;

	g_assert(parent != NULL);

	if (parent != store->priv->root) {
		iter->user_data = parent;
		iter->stamp = store->priv->stamp;
		return TRUE;
	} else
		return FALSE;
}

static void
esss_tree_model_init(GtkTreeModelIface *iface)
{
	iface->get_flags = esss_get_flags;
	iface->get_n_columns = esss_get_n_columns;
	iface->get_column_type = esss_get_column_type;
	iface->get_iter = esss_get_iter;
	iface->get_path = esss_get_path;
	iface->get_value = esss_get_value;
	iface->iter_next = esss_iter_next;
	iface->iter_children = esss_iter_children;
	iface->iter_has_child = esss_iter_has_child;
	iface->iter_n_children = esss_iter_n_children;
	iface->iter_nth_child = esss_iter_nth_child;
	iface->iter_parent = esss_iter_parent;
}

/* GtkTreeDragSource interface implementation */

static gboolean
esss_drag_data_delete(GtkTreeDragSource *source, GtkTreePath *path)
{
	GtkTreeIter iter;

	g_return_val_if_fail(E_IS_STORAGE_SET_STORE(source), FALSE);

	if (gtk_tree_model_get_iter(GTK_TREE_MODEL(source), &iter, path)) {
#if 0
		e_storage_set_store_remove(E_STORAGE_SET_STORE(source), &iter);
#endif
		return TRUE;
	} else {
		return FALSE;
	}
}

static gboolean
esss_drag_data_get(GtkTreeDragSource *source, GtkTreePath *path, GtkSelectionData *selection_data)
{
	g_return_val_if_fail(E_IS_STORAGE_SET_STORE(source), FALSE);

	/* Note that we don't need to handle the GTK_TREE_MODEL_ROW
	 * target, because the default handler does it for us, but
	 * we do anyway for the convenience of someone maybe overriding the
	 * default handler.
	 */

	if (gtk_tree_set_row_drag_data(selection_data, GTK_TREE_MODEL(source), path)) {
		return TRUE;
	} else {
		/* FIXME handle text targets at least. */
	}

	return FALSE;
}

static void
esss_drag_source_init(GtkTreeDragSourceIface * iface)
{
	iface->drag_data_delete = esss_drag_data_delete;
	iface->drag_data_get = esss_drag_data_get;
}

/* GtkTreeDragDest interface implementation */

static void
copy_node_data(EStorageSetStore *store, GtkTreeIter *src_iter, GtkTreeIter *dest_iter)
{
}

static void
recursive_node_copy(EStorageSetStore * store,
		    GtkTreeIter * src_iter, GtkTreeIter * dest_iter)
{
}

static gboolean
esss_drag_data_received(GtkTreeDragDest * drag_dest,
				  GtkTreePath * dest,
				  GtkSelectionData * selection_data)
{
#if 0
	GtkTreeModel *tree_model;
	EStorageSetStore *store;
	GtkTreeModel *src_model = NULL;
	GtkTreePath *src_path = NULL;
	gboolean retval = FALSE;

	g_return_val_if_fail(E_IS_STORAGE_SET_STORE(drag_dest), FALSE);

	tree_model = GTK_TREE_MODEL(drag_dest);
	store = E_STORAGE_SET_STORE(drag_dest);

	validate_tree(store);

	if (gtk_tree_get_row_drag_data(selection_data,
				       &src_model,
				       &src_path) &&
	    src_model == tree_model) {
		/* Copy the given row to a new position */
		GtkTreeIter src_iter;
		GtkTreeIter dest_iter;
		GtkTreePath *prev;

		if (!gtk_tree_model_get_iter(src_model,
					     &src_iter, src_path)) {
			goto out;
		}

		/* Get the path to insert _after_ (dest is the path to insert _before_) */
		prev = gtk_tree_path_copy(dest);

		if (!gtk_tree_path_prev(prev)) {
			GtkTreeIter dest_parent;
			GtkTreePath *parent;
			GtkTreeIter *dest_parent_p;

			/* dest was the first spot at the current depth; which means
			 * we are supposed to prepend.
			 */

			/* Get the parent, NULL if parent is the root */
			dest_parent_p = NULL;
			parent = gtk_tree_path_copy(dest);
			if (gtk_tree_path_up(parent) &&
			    gtk_tree_path_get_depth(parent) > 0) {
				gtk_tree_model_get_iter(tree_model,
							&dest_parent,
							parent);
				dest_parent_p = &dest_parent;
			}
			gtk_tree_path_free(parent);
			parent = NULL;

			e_storage_set_store_prepend(E_STORAGE_SET_STORE(tree_model),
					       &dest_iter, dest_parent_p);

			retval = TRUE;
		} else {
			if (gtk_tree_model_get_iter
			    (GTK_TREE_MODEL(tree_model), &dest_iter,
			     prev)) {
				GtkTreeIter tmp_iter = dest_iter;

				if (GPOINTER_TO_INT
				    (g_object_get_data
				     (G_OBJECT(tree_model),
				      "gtk-tree-model-drop-append"))) {
					GtkTreeIter parent;

					if (gtk_tree_model_iter_parent
					    (GTK_TREE_MODEL(tree_model),
					     &parent, &tmp_iter))
						e_storage_set_store_append
						    (E_STORAGE_SET_STORE
						     (tree_model),
						     &dest_iter, &parent);
					else
						e_storage_set_store_append
						    (E_STORAGE_SET_STORE
						     (tree_model),
						     &dest_iter, NULL);
				} else
					e_storage_set_store_insert_after
					    (E_STORAGE_SET_STORE(tree_model),
					     &dest_iter, NULL, &tmp_iter);
				retval = TRUE;

			}
		}

		g_object_set_data(G_OBJECT(tree_model),
				  "gtk-tree-model-drop-append", NULL);

		gtk_tree_path_free(prev);

		/* If we succeeded in creating dest_iter, walk src_iter tree branch,
		 * duplicating it below dest_iter.
		 */

		if (retval) {
			recursive_node_copy(store,
					    &src_iter, &dest_iter);
		}
	} else {
		/* FIXME maybe add some data targets eventually, or handle text
		 * targets in the simple case.
		 */

	}

      out:

	if (src_path)
		gtk_tree_path_free(src_path);

	return retval;
#else
	return FALSE;
#endif
}

static gboolean
esss_row_drop_possible(GtkTreeDragDest * drag_dest,
				 GtkTreePath * dest_path,
				 GtkSelectionData * selection_data)
{
#if 0
	GtkTreeModel *src_model = NULL;
	GtkTreePath *src_path = NULL;
	GtkTreePath *tmp = NULL;
	gboolean retval = FALSE;

	if (!gtk_tree_get_row_drag_data(selection_data,
					&src_model, &src_path))
		goto out;

	/* can only drag to ourselves */
	if (src_model != GTK_TREE_MODEL(drag_dest))
		goto out;

	/* Can't drop into ourself. */
	if (gtk_tree_path_is_ancestor(src_path, dest_path))
		goto out;

	/* Can't drop if dest_path's parent doesn't exist */
	{
		GtkTreeIter iter;

		if (gtk_tree_path_get_depth(dest_path) > 1) {
			tmp = gtk_tree_path_copy(dest_path);
			gtk_tree_path_up(tmp);

			if (!gtk_tree_model_get_iter
			    (GTK_TREE_MODEL(drag_dest), &iter, tmp))
				goto out;
		}
	}

	/* Can otherwise drop anywhere. */
	retval = TRUE;

      out:

	if (src_path)
		gtk_tree_path_free(src_path);
	if (tmp)
		gtk_tree_path_free(tmp);

	return retval;
#else
	return FALSE;
#endif
}

static void
esss_drag_dest_init(GtkTreeDragDestIface * iface)
{
	iface->drag_data_received = esss_drag_data_received;
	iface->row_drop_possible = esss_row_drop_possible;
}

typedef struct {
	gint offset;
	GNode *node;
} SortTuple;

static gint
folder_sort_callback (gconstpointer a, gconstpointer b, gpointer user_data)
{
	EStorageSetStore *store = (EStorageSetStore *) user_data;
        EStorageSetStorePrivate *priv = store->priv;
        EFolder *folder_1, *folder_2;
        char *folder_path_1, *folder_path_2;
        int priority_1, priority_2;


        folder_path_1 = (gchar *)((SortTuple *)a)->node->data;
        folder_path_2 = (gchar *)((SortTuple *)b)->node->data;

        folder_1 = e_storage_set_get_folder (priv->storage_set, folder_path_1);
        folder_2 = e_storage_set_get_folder (priv->storage_set, folder_path_2);

        priority_1 = e_folder_get_sorting_priority (folder_1);
        priority_2 = e_folder_get_sorting_priority (folder_2);

        if (priority_1 == priority_2)
                return g_utf8_collate (e_folder_get_name (folder_1), e_folder_get_name (folder_2));
        else if (priority_1 < priority_2)
                return -1;
        else                    /* priority_1 > priority_2 */
                return +1;
}

static gint
storage_sort_callback (gconstpointer a, gconstpointer b, gpointer user_data)
{
	char *folder_path_1;
	char *folder_path_2;
	gboolean path_1_local;
	gboolean path_2_local;

        folder_path_1 = (gchar *)((SortTuple *)a)->node->data;
        folder_path_2 = (gchar *)((SortTuple *)b)->node->data;

	/* FIXME bad hack to put the "my evolution" and "local" storages on
	 *            top.  */

	if (strcmp (folder_path_1, E_SUMMARY_STORAGE_NAME) == 0)
		return -1;
	if (strcmp (folder_path_2, E_SUMMARY_STORAGE_NAME) == 0)
		return +1;

	path_1_local = ! strcmp (folder_path_1, E_LOCAL_STORAGE_NAME);
	path_2_local = ! strcmp (folder_path_2, E_LOCAL_STORAGE_NAME);

	if (path_1_local && path_2_local)
		return 0;
	if (path_1_local)
		return -1;
	if (path_2_local)
		return 1;

	return g_utf8_collate (folder_path_1, folder_path_2);
}

static void
esss_sort (EStorageSetStore *store, GNode *parent, GCompareDataFunc callback)
{
	GtkTreeIter iter;
	GArray *sort_array;
	GNode *node;
	GNode *tmp_node;
	gint list_length;
	gint i;
	gint *new_order;
	GtkTreePath *path;

	node = parent->children;
	if (node == NULL || node->next == NULL)
		return;

	list_length = 0;
	for (tmp_node = node; tmp_node; tmp_node = tmp_node->next)
		list_length++;

	sort_array = g_array_sized_new(FALSE, FALSE, sizeof(SortTuple), list_length);

	i = 0;
	for (tmp_node = node; tmp_node; tmp_node = tmp_node->next) {
		SortTuple tuple;

		tuple.offset = i;
		tuple.node = tmp_node;
		g_array_append_val(sort_array, tuple);
		i++;
	}

	/* Sort the array */
	g_array_sort_with_data(sort_array, callback, store);

	for (i = 0; i < list_length - 1; i++) {
		g_array_index(sort_array, SortTuple, i).node->next =
		    g_array_index(sort_array, SortTuple, i + 1).node;
		g_array_index(sort_array, SortTuple, i + 1).node->prev =
		    g_array_index(sort_array, SortTuple, i).node;
	}
	g_array_index(sort_array, SortTuple, list_length - 1).node->next = NULL;
	g_array_index(sort_array, SortTuple, 0).node->prev = NULL;
	parent->children = g_array_index(sort_array, SortTuple, 0).node;

	/* Let the world know about our new order */
	new_order = g_new(gint, list_length);
	for (i = 0; i < list_length; i++)
		new_order[i] = g_array_index(sort_array, SortTuple, i).offset;

	iter.stamp = store->priv->stamp;
	iter.user_data = parent;
	path = esss_get_path(GTK_TREE_MODEL(store), &iter);
	gtk_tree_model_rows_reordered(GTK_TREE_MODEL(store), path, &iter, new_order);
	if (path)
		gtk_tree_path_free(path);
	g_free(new_order);
	g_array_free(sort_array, TRUE);
}

static void
esss_init(EStorageSetStore *store)
{
	store->priv = g_new0(EStorageSetStorePrivate, 1);

	store->priv->storage_set = NULL;
	store->priv->checkboxes = NULL;
	store->priv->root = g_node_new(NULL);
	do {
		store->priv->stamp = g_random_int();
	} while (store->priv->stamp == 0);

	store->priv->path_to_node = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	store->priv->type_name_to_pixbuf = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
esss_dispose(GObject *object)
{
	EStorageSetStore *store = E_STORAGE_SET_STORE(object);

	if (store->priv->storage_set)
		g_object_unref(store->priv->storage_set);
	store->priv->storage_set = NULL;

	(*parent_class->dispose) (object);
}

static void
node_free (GNode *node, gpointer data)
{
	g_free (node->data);
}

static void
pixbuf_free_func (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	g_object_unref (value);
}

static void
esss_finalize(GObject *object)
{
	EStorageSetStore *store = E_STORAGE_SET_STORE(object);

	g_node_children_foreach(store->priv->root, G_TRAVERSE_ALL, node_free, NULL);

	g_hash_table_foreach (store->priv->type_name_to_pixbuf, pixbuf_free_func, NULL);
	g_hash_table_destroy (store->priv->type_name_to_pixbuf);
	g_hash_table_destroy (store->priv->path_to_node);
	if (store->priv->checkboxes)
		g_hash_table_destroy (store->priv->checkboxes);

	g_free (store->priv);

	(*parent_class->finalize) (object);
}

static void
esss_class_init(EStorageSetStoreClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent(class);
	object_class = (GObjectClass *) class;

	object_class->dispose = esss_dispose;
	object_class->finalize = esss_finalize;
}

GType
e_storage_set_store_get_type(void)
{
	static GType store_type = 0;

	if (!store_type) {
		static const GTypeInfo store_info = {
			sizeof(EStorageSetStoreClass),
			NULL,	/* base_init */
			NULL,	/* base_finalize */
			(GClassInitFunc) esss_class_init,
			NULL,	/* class_finalize */
			NULL,	/* class_data */
			sizeof(EStorageSetStore),
			0,	/* n_preallocs */
			(GInstanceInitFunc) esss_init
		};

		static const GInterfaceInfo tree_model_info = {
			(GInterfaceInitFunc)
			    esss_tree_model_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo drag_source_info = {
			(GInterfaceInitFunc)
			    esss_drag_source_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo drag_dest_info = {
			(GInterfaceInitFunc) esss_drag_dest_init,
			NULL,
			NULL
		};

		store_type = g_type_register_static(G_TYPE_OBJECT, "EStorageSetStore", &store_info, 0);

		g_type_add_interface_static(store_type, GTK_TYPE_TREE_MODEL, &tree_model_info);
		g_type_add_interface_static(store_type, GTK_TYPE_TREE_DRAG_SOURCE, &drag_source_info);
		g_type_add_interface_static(store_type, GTK_TYPE_TREE_DRAG_DEST, &drag_dest_info);
	}

	return store_type;
}

/* Handling of the "changed" signal in EFolders displayed in the EStorageSetStore.  */

typedef struct {
	EStorageSetStore *store;
	GNode * node;
} FolderChangedCallbackData;

static void
folder_changed_cb (EFolder *folder, void *data)
{
	FolderChangedCallbackData *callback_data;
	GtkTreePath *path;
	GtkTreeIter iter;

	callback_data = (FolderChangedCallbackData *) data;
	iter.user_data = callback_data->node;
	iter.stamp = callback_data->store->priv->stamp;
	path = esss_get_path (GTK_TREE_MODEL (callback_data->store), &iter);

	gtk_tree_model_row_changed (GTK_TREE_MODEL (callback_data->store), path, &iter);
	if (path)
		gtk_tree_path_free (path);
}

static void
folder_name_changed_cb (EFolder *folder, void *data)
{
	FolderChangedCallbackData *callback_data;

	callback_data = (FolderChangedCallbackData *) data;

	esss_sort (callback_data->store, callback_data->node->parent, folder_sort_callback);
}

static void
setup_folder_changed_callbacks (EStorageSetStore *store, EFolder *folder, GNode *node)
{
	FolderChangedCallbackData *callback_data = g_new0 (FolderChangedCallbackData, 1);
	callback_data->store = store;
	callback_data->node = node;

	g_signal_connect (G_OBJECT (folder), "changed",
			  G_CALLBACK (folder_changed_cb), callback_data);

	g_signal_connect_data (G_OBJECT (folder), "name_changed",
			       G_CALLBACK (folder_name_changed_cb), 
			       callback_data, (GClosureNotify)g_free, 0);
}

static void
insert_folders (EStorageSetStore *store, GNode *parent, EStorage *storage, const gchar *path)
{
	EStorageSetStorePrivate *priv;
	GList *folder_path_list, *p;
	const gchar *storage_name = e_storage_get_name (storage);

	priv = store->priv;

	folder_path_list = e_storage_get_subfolder_paths (storage, path);
	if (folder_path_list == NULL)
		return;

	for (p = folder_path_list; p != NULL; p = p->next) {
		EFolder *folder;
		const char *subpath = (const char *) p->data;
		char *folder_path = g_strconcat ("/", storage_name, subpath, NULL);
		gchar *key = g_strdup (folder_path+1);
		GNode *node = g_node_new (folder_path);

		g_node_append (parent, node);
		g_hash_table_replace (priv->path_to_node, key, node);

		folder = e_storage_get_folder (storage, subpath);
		setup_folder_changed_callbacks (store, folder, node);

		insert_folders (store, node, storage, subpath);
	}

	esss_sort (store, parent, folder_sort_callback);

	e_free_string_list (folder_path_list);
}

/* StorageSet signal handling.  */

static void
new_storage_cb (EStorageSet *storage_set, EStorage *storage, void *data)
{
	EStorageSetStore *store = E_STORAGE_SET_STORE (data);
	EStorageSetStorePrivate *priv = store->priv;
	gchar *storage_name = g_strdup (e_storage_get_name (storage));
	GNode *node = g_node_new (g_strdup (e_storage_get_name (storage)));
	GtkTreePath *path;
	GtkTreeIter iter;

	g_hash_table_replace (priv->path_to_node, storage_name, node);
	g_node_append (priv->root, node);
	esss_sort (store, priv->root, storage_sort_callback);

	iter.user_data = node;
	iter.stamp = store->priv->stamp;
	path = esss_get_path (GTK_TREE_MODEL (store), &iter);
	gtk_tree_model_row_inserted (GTK_TREE_MODEL (store), path, &iter);
	if (path)
		gtk_tree_path_free (path);
}

static void
removed_storage_cb (EStorageSet *storage_set, EStorage *storage, void *data)
{
	EStorageSetStore *store = E_STORAGE_SET_STORE (data);
	EStorageSetStorePrivate *priv = store->priv;
	const gchar *name = e_storage_get_name (storage);
	GNode *node = g_hash_table_lookup (priv->path_to_node, name);
	GtkTreePath *path;

	if (node == NULL) {
		g_warning ("EStorageSetStore: unknown storage removed -- %s", name);
		return;
	}

	g_hash_table_remove (priv->path_to_node, name);
	/* FIXME: subfolder hashtable entries might be leaked */

	path = tree_path_from_node (store, node);
	gtk_tree_model_row_deleted (GTK_TREE_MODEL (store), path);
	if (path)
		gtk_tree_path_free (path);
	g_node_destroy (node);
}

static void
new_folder_cb (EStorageSet *storage_set, const char *path, void *data)
{
	EStorageSetStore *store = E_STORAGE_SET_STORE (data);
	EStorageSetStorePrivate *priv = store->priv;
	GNode *parent_node, *new_node;
	const char *last_separator;
	char *parent_path;
	char *copy_of_path;
	GtkTreeIter iter;
	GtkTreePath *treepath;

	last_separator = strrchr (path, E_PATH_SEPARATOR);

	parent_path = g_strndup (path + 1, last_separator - path - 1);
	parent_node = g_hash_table_lookup (priv->path_to_node, parent_path);
	g_free (parent_path);
	if (parent_node == NULL) {
		g_warning ("EStorageSetStore: EStorageSet reported new subfolder for non-existing folder -- %s", parent_path);
		return;
	}

	copy_of_path = g_strdup (path);
	new_node = g_node_new (copy_of_path);
	g_node_append (parent_node, new_node);
	iter.user_data = new_node;
	iter.stamp = priv->stamp;
	treepath = esss_get_path (GTK_TREE_MODEL (store), &iter);
	gtk_tree_model_row_inserted (GTK_TREE_MODEL (store), treepath, &iter);
	if (treepath)
		gtk_tree_path_free (treepath);

	g_hash_table_replace (priv->path_to_node, g_strdup (path + 1), new_node);

	setup_folder_changed_callbacks (store, e_storage_set_get_folder (storage_set, path), new_node);
	esss_sort (store, parent_node, folder_sort_callback);
}

static void
updated_folder_cb (EStorageSet *storage_set, const char *path, void *data)
{
	EStorageSetStore *store = E_STORAGE_SET_STORE (data);
	EStorageSetStorePrivate *priv = store->priv;
	GNode *node;
	GtkTreeIter iter;
	GtkTreePath *treepath;

	node = g_hash_table_lookup (priv->path_to_node, path+1);
	if (node == NULL) {
		g_warning ("EStorageSetStore: unknown folder updated -- %s", path);
		return;
	}

	iter.user_data = node;
	iter.stamp = priv->stamp;
	treepath = esss_get_path (GTK_TREE_MODEL (store), &iter);
	gtk_tree_model_row_changed (GTK_TREE_MODEL (store), treepath, &iter);
	if (treepath)
		gtk_tree_path_free (treepath);
}

static void
removed_folder_cb (EStorageSet *storage_set, const char *path, void *data)
{
	EStorageSetStore *store = E_STORAGE_SET_STORE (data);
	EStorageSetStorePrivate *priv = store->priv;
	GNode *node;
	GtkTreePath *treepath;

	node = g_hash_table_lookup (priv->path_to_node, path+1);
	if (node == NULL) {
		g_warning ("EStorageSetStore: unknown folder removed -- %s", path);
		return;
	}

	g_hash_table_remove (priv->path_to_node, path+1);
	/* FIXME: subfolder hashtable entries might be leaked */

	treepath = tree_path_from_node (store, node);
	gtk_tree_model_row_deleted (GTK_TREE_MODEL (store), treepath);
	if (treepath)
		gtk_tree_path_free (treepath);
	g_node_destroy (node);
}

static void
close_folder_cb (EStorageSet *storage_set,
		 const char *path,
		 void *data)
{
	g_warning ("FIXME: EStorageSetStore: needs to handle close_folder properly");
#if 0
	EStorageSetStore *store;
	EStorageSetStorePrivate *priv;
	ETreeModel *etree;
	ETreePath node;

	store = E_STORAGE_SET_STORE (data);
	priv = store->priv;
	etree = priv->etree_model;

	node = lookup_node_in_hash (store, path);
	e_tree_model_node_request_collapse (priv->etree_model, node);
#endif
}

static void
connect_storage_set (EStorageSetStore *store, EStorageSet *storage_set, gboolean show_folders)
{
	EStorageSetStorePrivate *priv;
	GList *storage_list;
	GList *p;

	priv = store->priv;
	priv->storage_set = storage_set;
	g_object_ref (storage_set);

	storage_list = e_storage_set_get_storage_list (storage_set);

	for (p = storage_list; p != NULL; p = p->next) {
		EStorage *storage = E_STORAGE (p->data);
		const char *name = e_storage_get_name (storage);
		GNode *node = g_node_new (g_strdup (name));
		g_node_append (priv->root, node);
		g_hash_table_replace (priv->path_to_node, g_strdup (name), node);

		if (show_folders)
			insert_folders (store, node, storage, "/");
	}

	esss_sort (store, priv->root, storage_sort_callback);

	e_free_object_list (storage_list);

	g_signal_connect_object (storage_set, "new_storage", G_CALLBACK (new_storage_cb), store, 0);
	g_signal_connect_object (storage_set, "removed_storage", G_CALLBACK (removed_storage_cb), store, 0);
	if (!show_folders)
		return;

	g_signal_connect_object (storage_set, "new_folder", G_CALLBACK (new_folder_cb), store, 0);
	g_signal_connect_object (storage_set, "updated_folder", G_CALLBACK (updated_folder_cb), store, 0);
	g_signal_connect_object (storage_set, "removed_folder", G_CALLBACK (removed_folder_cb), store, 0);
	g_signal_connect_object (storage_set, "close_folder", G_CALLBACK (close_folder_cb), store, 0);
}

/**
 * e_storage_set_store_new:
 * @storage_set: the #EStorageSet that the store exposes
 * @show_folders: flag indicating if subfolders should be shown
 *
 * Creates a new tree store from the provided #EStorageSet.
 *
 * Return value: a new #EStorageSetStore
 **/
EStorageSetStore *
e_storage_set_store_new(EStorageSet *storage_set, gboolean show_folders)
{
	EStorageSetStore *store;
	g_return_val_if_fail (E_IS_STORAGE_SET(storage_set), NULL);

	store = E_STORAGE_SET_STORE (g_object_new (E_STORAGE_SET_STORE_TYPE, NULL));
	connect_storage_set (store, storage_set, show_folders);

	return store;
}

static gboolean
esss_real_set_value(EStorageSetStore *store, GtkTreeIter *iter, gint column, GValue *value)
{
	gchar *path;

	if (column != E_STORAGE_SET_STORE_COLUMN_CHECKED)
		return FALSE;

	path = G_NODE (iter->user_data)->data;

	if (g_value_get_boolean (value)) {
		g_hash_table_insert (store->priv->checkboxes, path, path);
	} else {
		g_hash_table_remove (store->priv->checkboxes, path);
	}

	return TRUE;
}

/**
 * e_storage_set_store_set_value:
 * @store: a #EStorageSetStore
 * @iter: A valid #GtkTreeIter for the row being modified
 * @column: column number to modify
 * @value: new value for the cell
 *
 * Sets the data in the cell specified by @iter and @column.
 * The type of @value must be convertible to the type of the
 * column.
 *
 **/
void
e_storage_set_store_set_value(EStorageSetStore *store, GtkTreeIter *iter, 
			      gint column, GValue *value)
{
	g_return_if_fail(E_IS_STORAGE_SET_STORE(store));
	g_return_if_fail(VALID_ITER(iter, store));
	g_return_if_fail(VALID_COL(column));
	g_return_if_fail(G_IS_VALUE(value));

	if (esss_real_set_value (store, iter, column, value)) {
		GtkTreePath *path;

		path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), iter);
		gtk_tree_model_row_changed(GTK_TREE_MODEL(store), path, iter);
		if (path)
			gtk_tree_path_free(path);
	}
}

/**
 * e_storage_set_store_get_tree_path:
 * @store: a #EStorageSetStore
 * @folder_path: a string representing the #EStorageSet folder path
 * 
 * Gets a #GtkTreePath corresponding to the folder path specified.
 *
 * Return value: the tree path of the folder
 **/
GtkTreePath *
e_storage_set_store_get_tree_path (EStorageSetStore *store, const gchar *folder_path)
{
	GNode *node;

	g_return_if_fail(E_IS_STORAGE_SET_STORE(store));

	node = g_hash_table_lookup (store->priv->path_to_node, folder_path+1);
	
	return tree_path_from_node (store, node);
}

/**
 * e_storage_set_store_get_folder_path:
 * @store: a #EStorageSetStore
 * @folder_path: a string representing the #EStorageSet folder path
 * 
 * Gets a #GtkTreePath corresponding to the folder path specified.
 *
 * Return value: the tree path of the folder
 **/
const gchar *
e_storage_set_store_get_folder_path (EStorageSetStore *store, GtkTreePath *tree_path)
{
	GtkTreeIter iter;
	GNode *node;

	g_return_if_fail(E_IS_STORAGE_SET_STORE(store));

	if (!esss_get_iter (GTK_TREE_MODEL (store), &iter, tree_path))
		return NULL;

	node = G_NODE (iter.user_data);

	return (const gchar *)node->data;
}

/**
 * e_storage_set_store_set_has_checkbox_func:
 * @store: a #EStorageSetStore
 * @func: a callback to determine if a row is checked
 * @data: callback data
 * 
 * Sets a callback function for checkbox visibility determination
 **/
void
e_storage_set_store_set_has_checkbox_func (EStorageSetStore *store, EStorageSetStoreHasCheckBoxFunc func, gpointer data)
{
	g_return_if_fail(E_IS_STORAGE_SET_STORE(store));

	store->priv->has_checkbox_func = func;
	store->priv->has_checkbox_func_data = data;
}

