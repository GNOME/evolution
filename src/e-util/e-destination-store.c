/* e-destination-store.c - EDestination store with GtkTreeModel interface.
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

#include "e-destination-store.h"

#define ITER_IS_VALID(destination_store, iter) \
	((iter)->stamp == (destination_store)->priv->stamp)
#define ITER_GET(iter) \
	GPOINTER_TO_INT (iter->user_data)
#define ITER_SET(destination_store, iter, index) \
	G_STMT_START { \
	(iter)->stamp = (destination_store)->priv->stamp; \
	(iter)->user_data = GINT_TO_POINTER (index); \
	} G_STMT_END

struct _EDestinationStorePrivate {
	GPtrArray *destinations;
	gint stamp;
};

static GType column_types[E_DESTINATION_STORE_NUM_COLUMNS];

static void e_destination_store_tree_model_init (GtkTreeModelIface *iface);

G_DEFINE_TYPE_EXTENDED (EDestinationStore, e_destination_store, G_TYPE_OBJECT, 0,
	G_ADD_PRIVATE (EDestinationStore)
	G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL, e_destination_store_tree_model_init)
	column_types[E_DESTINATION_STORE_COLUMN_NAME]    = G_TYPE_STRING;
	column_types[E_DESTINATION_STORE_COLUMN_EMAIL]   = G_TYPE_STRING;
	column_types[E_DESTINATION_STORE_COLUMN_ADDRESS] = G_TYPE_STRING;
)

static GtkTreeModelFlags e_destination_store_get_flags       (GtkTreeModel       *tree_model);
static gint         e_destination_store_get_n_columns   (GtkTreeModel       *tree_model);
static GType        e_destination_store_get_column_type (GtkTreeModel       *tree_model,
							 gint                index);
static gboolean     e_destination_store_get_iter        (GtkTreeModel       *tree_model,
							 GtkTreeIter        *iter,
							 GtkTreePath        *path);
static void         e_destination_store_get_value       (GtkTreeModel       *tree_model,
							 GtkTreeIter        *iter,
							 gint                column,
							 GValue             *value);
static gboolean     e_destination_store_iter_next       (GtkTreeModel       *tree_model,
							 GtkTreeIter        *iter);
static gboolean     e_destination_store_iter_children   (GtkTreeModel       *tree_model,
							 GtkTreeIter        *iter,
							 GtkTreeIter        *parent);
static gboolean     e_destination_store_iter_has_child  (GtkTreeModel       *tree_model,
							 GtkTreeIter        *iter);
static gint         e_destination_store_iter_n_children (GtkTreeModel       *tree_model,
							 GtkTreeIter        *iter);
static gboolean     e_destination_store_iter_nth_child  (GtkTreeModel       *tree_model,
							 GtkTreeIter        *iter,
							 GtkTreeIter        *parent,
							 gint                n);
static gboolean     e_destination_store_iter_parent     (GtkTreeModel       *tree_model,
							 GtkTreeIter        *iter,
							 GtkTreeIter        *child);

static void destination_changed (EDestinationStore *destination_store, EDestination *destination);
static void stop_destination    (EDestinationStore *destination_store, EDestination *destination);

static void
destination_store_dispose (GObject *object)
{
	EDestinationStore *self = E_DESTINATION_STORE (object);
	gint ii;

	for (ii = 0; ii < self->priv->destinations->len; ii++) {
		EDestination *destination;

		destination = g_ptr_array_index (self->priv->destinations, ii);
		stop_destination (E_DESTINATION_STORE (object), destination);
		g_object_unref (destination);
	}
	g_ptr_array_set_size (self->priv->destinations, 0);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_destination_store_parent_class)->dispose (object);
}

static void
destination_store_finalize (GObject *object)
{
	EDestinationStore *self = E_DESTINATION_STORE (object);

	g_ptr_array_free (self->priv->destinations, TRUE);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_destination_store_parent_class)->finalize (object);
}

static void
e_destination_store_class_init (EDestinationStoreClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = destination_store_dispose;
	object_class->finalize = destination_store_finalize;
}

static void
e_destination_store_tree_model_init (GtkTreeModelIface *iface)
{
	iface->get_flags = e_destination_store_get_flags;
	iface->get_n_columns = e_destination_store_get_n_columns;
	iface->get_column_type = e_destination_store_get_column_type;
	iface->get_iter = e_destination_store_get_iter;
	iface->get_path = e_destination_store_get_path;
	iface->get_value = e_destination_store_get_value;
	iface->iter_next = e_destination_store_iter_next;
	iface->iter_children = e_destination_store_iter_children;
	iface->iter_has_child = e_destination_store_iter_has_child;
	iface->iter_n_children = e_destination_store_iter_n_children;
	iface->iter_nth_child = e_destination_store_iter_nth_child;
	iface->iter_parent = e_destination_store_iter_parent;
}

static void
e_destination_store_init (EDestinationStore *destination_store)
{
	destination_store->priv = e_destination_store_get_instance_private (destination_store);

	destination_store->priv->destinations = g_ptr_array_new ();
	destination_store->priv->stamp = g_random_int ();
}

/**
 * e_destination_store_new:
 *
 * Creates a new #EDestinationStore.
 *
 * Returns: A new #EDestinationStore.
 **/
EDestinationStore *
e_destination_store_new (void)
{
	return g_object_new (E_TYPE_DESTINATION_STORE, NULL);
}

/* ------------------ *
 * Row update helpers *
 * ------------------ */

static void
row_deleted (EDestinationStore *destination_store,
             gint n)
{
	GtkTreePath *path;

	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, n);
	gtk_tree_model_row_deleted (GTK_TREE_MODEL (destination_store), path);
	gtk_tree_path_free (path);
}

static void
row_inserted (EDestinationStore *destination_store,
              gint n)
{
	GtkTreePath *path;
	GtkTreeIter  iter;

	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, n);

	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (destination_store), &iter, path))
		gtk_tree_model_row_inserted (GTK_TREE_MODEL (destination_store), path, &iter);

	gtk_tree_path_free (path);
}

static void
row_changed (EDestinationStore *destination_store,
             gint n)
{
	GtkTreePath *path;
	GtkTreeIter  iter;

	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, n);

	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (destination_store), &iter, path))
		gtk_tree_model_row_changed (GTK_TREE_MODEL (destination_store), path, &iter);

	gtk_tree_path_free (path);
}

/* ------------------- *
 * Destination helpers *
 * ------------------- */

static gint
find_destination_by_pointer (EDestinationStore *destination_store,
                             EDestination *destination)
{
	GPtrArray *array;
	gint i;

	array = destination_store->priv->destinations;

	for (i = 0; i < array->len; i++) {
		EDestination *destination_here;

		destination_here = g_ptr_array_index (array, i);

		if (destination_here == destination)
			return i;
	}

	return -1;
}

static gint
find_destination_by_email (EDestinationStore *destination_store,
                           EDestination *destination)
{
	GPtrArray *array;
	gint i;
	const gchar *e_mail = e_destination_get_email (destination);

	array = destination_store->priv->destinations;

	for (i = 0; i < array->len; i++) {
		EDestination *destination_here;
		const gchar *mail;

		destination_here = g_ptr_array_index (array, i);
		mail = e_destination_get_email (destination_here);

		if (g_str_equal (e_mail, mail))
			return i;
	}

	return -1;
}

static void
start_destination (EDestinationStore *destination_store,
                   EDestination *destination)
{
	g_signal_connect_swapped (
		destination, "changed",
		G_CALLBACK (destination_changed), destination_store);
}

static void
stop_destination (EDestinationStore *destination_store,
                  EDestination *destination)
{
	g_signal_handlers_disconnect_matched (
		destination, G_SIGNAL_MATCH_DATA,
		0, 0, NULL, NULL, destination_store);
}

/* --------------- *
 * Signal handlers *
 * --------------- */

static void
destination_changed (EDestinationStore *destination_store,
                     EDestination *destination)
{
	gint n;

	n = find_destination_by_pointer (destination_store, destination);
	if (n < 0) {
		g_warning ("EDestinationStore got change from unknown EDestination!");
		return;
	}

	row_changed (destination_store, n);
}

/* --------------------- *
 * EDestinationStore API *
 * --------------------- */

/**
 * e_destination_store_get_destination:
 * @destination_store: an #EDestinationStore
 * @iter: a #GtkTreeIter
 *
 * Gets the #EDestination from @destination_store at @iter.
 *
 * Returns: An #EDestination.
 **/
EDestination *
e_destination_store_get_destination (EDestinationStore *destination_store,
                                     GtkTreeIter *iter)
{
	GPtrArray *array;
	gint index;

	g_return_val_if_fail (E_IS_DESTINATION_STORE (destination_store), NULL);
	g_return_val_if_fail (ITER_IS_VALID (destination_store, iter), NULL);

	array = destination_store->priv->destinations;
	index = ITER_GET (iter);

	return g_ptr_array_index (array, index);
}

/**
 * e_destination_store_list_destinations:
 * @destination_store: an #EDestinationStore
 *
 * Gets a list of all the #EDestinations in @destination_store.
 *
 * Returns: A #GList of pointers to #EDestination. The list is owned
 * by the caller, but the #EDestination elements aren't.
 **/
GList *
e_destination_store_list_destinations (EDestinationStore *destination_store)
{
	GList *destination_list = NULL;
	GPtrArray *array;
	gint   i;

	g_return_val_if_fail (E_IS_DESTINATION_STORE (destination_store), NULL);

	array = destination_store->priv->destinations;

	for (i = 0; i < array->len; i++) {
		EDestination *destination;

		destination = g_ptr_array_index (array, i);
		destination_list = g_list_prepend (destination_list, destination);
	}

	destination_list = g_list_reverse (destination_list);

	return destination_list;
}

/**
 * e_destination_store_insert_destination:
 * @destination_store: an #EDestinationStore
 * @index: the index at which to insert
 * @destination: an #EDestination to insert
 *
 * Inserts @destination into @destination_store at the position
 * indicated by @index. @destination_store will ref @destination.
 **/
void
e_destination_store_insert_destination (EDestinationStore *destination_store,
                                        gint index,
                                        EDestination *destination)
{
	GPtrArray *array;

	g_return_if_fail (E_IS_DESTINATION_STORE (destination_store));
	g_return_if_fail (index >= 0);

	if (find_destination_by_pointer (destination_store, destination) >= 0) {
		g_warning ("Same destination added more than once to EDestinationStore!");
		return;
	}

	g_object_ref (destination);

	array = destination_store->priv->destinations;
	index = MIN (index, array->len);

	g_ptr_array_set_size (array, array->len + 1);

	if (array->len - 1 - index > 0) {
		memmove (
			array->pdata + index + 1,
			array->pdata + index,
			(array->len - 1 - index) * sizeof (gpointer));
	}

	array->pdata[index] = destination;
	start_destination (destination_store, destination);
	row_inserted (destination_store, index);
}

/**
 * e_destination_store_append_destination:
 * @destination_store: an #EDestinationStore
 * @destination: an #EDestination
 *
 * Appends @destination to the list of destinations in @destination_store.
 * @destination_store will ref @destination.
 **/
void
e_destination_store_append_destination (EDestinationStore *destination_store,
                                        EDestination *destination)
{
	GPtrArray *array;

	g_return_if_fail (E_IS_DESTINATION_STORE (destination_store));

	if (find_destination_by_email (destination_store, destination) >= 0 && !e_destination_is_evolution_list (destination)) {
		g_warning ("Same destination added more than once to EDestinationStore!");
		return;
	}

	array = destination_store->priv->destinations;
	g_object_ref (destination);

	g_ptr_array_add (array, destination);
	start_destination (destination_store, destination);
	row_inserted (destination_store, array->len - 1);
}

/**
 * e_destination_store_remove_destination:
 * @destination_store: an #EDestinationStore
 * @destination: an #EDestination to remove
 *
 * Removes @destination from @destination_store. @destination_store will
 * unref @destination.
 **/
void
e_destination_store_remove_destination (EDestinationStore *destination_store,
                                        EDestination *destination)
{
	GPtrArray *array;
	gint n;

	g_return_if_fail (E_IS_DESTINATION_STORE (destination_store));

	n = find_destination_by_pointer (destination_store, destination);
	if (n < 0) {
		g_warning ("Tried to remove unknown destination from EDestinationStore!");
		return;
	}

	stop_destination (destination_store, destination);
	g_object_unref (destination);

	array = destination_store->priv->destinations;
	g_ptr_array_remove_index (array, n);
	row_deleted (destination_store, n);
}

void
e_destination_store_remove_destination_nth (EDestinationStore *destination_store,
                                            gint n)
{
	EDestination *destination;
	GPtrArray *array;

	g_return_if_fail (n >= 0);

	array = destination_store->priv->destinations;
	destination = g_ptr_array_index (array, n);
	stop_destination (destination_store, destination);
	g_object_unref (destination);

	g_ptr_array_remove_index (array, n);
	row_deleted (destination_store, n);
}

guint
e_destination_store_get_destination_count (EDestinationStore *destination_store)
{
	return destination_store->priv->destinations->len;
}

/* ---------------- *
 * GtkTreeModel API *
 * ---------------- */

static GtkTreeModelFlags
e_destination_store_get_flags (GtkTreeModel *tree_model)
{
	g_return_val_if_fail (E_IS_DESTINATION_STORE (tree_model), 0);

	return GTK_TREE_MODEL_LIST_ONLY;
}

static gint
e_destination_store_get_n_columns (GtkTreeModel *tree_model)
{
	g_return_val_if_fail (E_IS_DESTINATION_STORE (tree_model), 0);

	return E_CONTACT_FIELD_LAST;
}

static GType
e_destination_store_get_column_type (GtkTreeModel *tree_model,
                                     gint index)
{
	g_return_val_if_fail (E_IS_DESTINATION_STORE (tree_model), G_TYPE_INVALID);
	g_return_val_if_fail (index >= 0 && index < E_DESTINATION_STORE_NUM_COLUMNS, G_TYPE_INVALID);

	return column_types[index];
}

static gboolean
e_destination_store_get_iter (GtkTreeModel *tree_model,
                              GtkTreeIter *iter,
                              GtkTreePath *path)
{
	EDestinationStore *destination_store;
	GPtrArray *array;
	gint index;

	g_return_val_if_fail (E_IS_DESTINATION_STORE (tree_model), FALSE);
	g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

	destination_store = E_DESTINATION_STORE (tree_model);

	index = gtk_tree_path_get_indices (path)[0];
	array = destination_store->priv->destinations;

	if (index >= array->len)
		return FALSE;

	ITER_SET (destination_store, iter, index);
	return TRUE;
}

GtkTreePath *
e_destination_store_get_path (GtkTreeModel *tree_model,
                              GtkTreeIter *iter)
{
	EDestinationStore *destination_store = E_DESTINATION_STORE (tree_model);
	GtkTreePath       *path;
	gint               index;

	g_return_val_if_fail (E_IS_DESTINATION_STORE (tree_model), NULL);
	g_return_val_if_fail (ITER_IS_VALID (destination_store, iter), NULL);

	index = ITER_GET (iter);
	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, index);

	return path;
}

static gboolean
e_destination_store_iter_next (GtkTreeModel *tree_model,
                               GtkTreeIter *iter)
{
	EDestinationStore *destination_store = E_DESTINATION_STORE (tree_model);
	gint           index;

	g_return_val_if_fail (E_IS_DESTINATION_STORE (tree_model), FALSE);
	g_return_val_if_fail (ITER_IS_VALID (destination_store, iter), FALSE);

	index = ITER_GET (iter);

	if (index + 1 < destination_store->priv->destinations->len) {
		ITER_SET (destination_store, iter, index + 1);
		return TRUE;
	}

	return FALSE;
}

static gboolean
e_destination_store_iter_children (GtkTreeModel *tree_model,
                                   GtkTreeIter *iter,
                                   GtkTreeIter *parent)
{
	EDestinationStore *destination_store = E_DESTINATION_STORE (tree_model);

	g_return_val_if_fail (E_IS_DESTINATION_STORE (tree_model), FALSE);

	/* This is a list, nodes have no children. */
	if (parent)
		return FALSE;

	/* But if parent == NULL we return the list itself as children of the root. */
	if (destination_store->priv->destinations->len <= 0)
		return FALSE;

	ITER_SET (destination_store, iter, 0);
	return TRUE;
}

static gboolean
e_destination_store_iter_has_child (GtkTreeModel *tree_model,
                                    GtkTreeIter *iter)
{
	g_return_val_if_fail (E_IS_DESTINATION_STORE (tree_model), FALSE);

	if (iter == NULL)
		return TRUE;

	return FALSE;
}

static gint
e_destination_store_iter_n_children (GtkTreeModel *tree_model,
                                     GtkTreeIter *iter)
{
	EDestinationStore *destination_store = E_DESTINATION_STORE (tree_model);

	g_return_val_if_fail (E_IS_DESTINATION_STORE (tree_model), -1);

	if (iter == NULL)
		return destination_store->priv->destinations->len;

	g_return_val_if_fail (ITER_IS_VALID (destination_store, iter), -1);
	return 0;
}

static gboolean
e_destination_store_iter_nth_child (GtkTreeModel *tree_model,
                                    GtkTreeIter *iter,
                                    GtkTreeIter *parent,
                                    gint n)
{
	EDestinationStore *destination_store = E_DESTINATION_STORE (tree_model);

	g_return_val_if_fail (E_IS_DESTINATION_STORE (tree_model), FALSE);

	if (parent)
		return FALSE;

	if (n < destination_store->priv->destinations->len) {
		ITER_SET (destination_store, iter, n);
		return TRUE;
	}

	return FALSE;
}

static gboolean
e_destination_store_iter_parent (GtkTreeModel *tree_model,
                                 GtkTreeIter *iter,
                                 GtkTreeIter *child)
{
	return FALSE;
}

static void
e_destination_store_get_value (GtkTreeModel *tree_model,
                               GtkTreeIter *iter,
                               gint column,
                               GValue *value)
{
	EDestinationStore *destination_store = E_DESTINATION_STORE (tree_model);
	EDestination *destination;
	GString *string_new;
	EContact *contact;
	GPtrArray *array;
	const gchar *string;
	gint row;

	g_return_if_fail (E_IS_DESTINATION_STORE (tree_model));
	g_return_if_fail (column < E_DESTINATION_STORE_NUM_COLUMNS);
	g_return_if_fail (ITER_IS_VALID (destination_store, iter));

	g_value_init (value, column_types[column]);

	array = destination_store->priv->destinations;

	row = ITER_GET (iter);
	if (row >= array->len)
		return;

	destination = g_ptr_array_index (array, row);
	g_return_if_fail (destination);

	switch (column) {
		case E_DESTINATION_STORE_COLUMN_NAME:
			string = e_destination_get_name (destination);
			g_value_set_string (value, string);
			break;

		case E_DESTINATION_STORE_COLUMN_EMAIL:
			string = e_destination_get_email (destination);
			g_value_set_string (value, string);
			break;

		case E_DESTINATION_STORE_COLUMN_ADDRESS:
			contact = e_destination_get_contact (destination);
			if (contact && E_IS_CONTACT (contact)) {
				if (e_contact_get (contact, E_CONTACT_IS_LIST)) {
					string = e_destination_get_name (destination);
					string_new = g_string_new (string);
					g_string_append (string_new, " mailing list");
					g_value_set_string (value, string_new->str);
					g_string_free (string_new, TRUE);
				}
				else {
					string = e_destination_get_address (destination);
					g_value_set_string (value, string);
				}
			}
			else {
				string = e_destination_get_address (destination);
				g_value_set_string (value, string);

			}
			break;

		default:
			g_warn_if_reached ();
			break;
	}
}

/**
 * e_destination_store_get_stamp:
 * @destination_store: an #EDestinationStore
 *
 * Since: 2.32
 **/
gint
e_destination_store_get_stamp (EDestinationStore *destination_store)
{
	g_return_val_if_fail (E_IS_DESTINATION_STORE (destination_store), 0);

	return destination_store->priv->stamp;
}
