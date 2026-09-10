/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

/**
 * SECTION: e-virtual-tree-model
 * @include: e-util/e-util.h
 * @short_description: Interface for providing row data to #EVirtualTree
 *
 * #EVirtualTreeModel is an interface that supplies row objects and
 * tree-structure metadata to an #EVirtualTree widget. Implementors
 * provide the total row count, row retrieval, depth/expand state, and
 * an opaque key system used for selection tracking.
 *
 * The key system is defined by two vfuncs:
 *
 * - get_row_key() returns a transfer-none pointer that uniquely identifies
 *   a row object. The pointer must remain valid while the row object is
 *   alive.
 * - get_key_type() returns an #EVirtualTreeKeyType struct that supplies
 *   hash, equal, copy, and free functions for the keys. The defaults use
 *   pointer identity (g_direct_hash / g_direct_equal, no copy/free).
 *
 * Models notify the tree of data changes via
 * e_virtual_tree_model_emit_before_rebuild(),
 * e_virtual_tree_model_emit_after_rebuild(),
 * e_virtual_tree_model_emit_rows_changed(),
 * e_virtual_tree_model_emit_rows_inserted(),
 * e_virtual_tree_model_emit_rows_removed(), and
 * e_virtual_tree_model_emit_row_count_changed().
 **/

#include "evolution-config.h"

#include <libedataserver/libedataserver.h>

#include "e-virtual-tree-model.h"

#define ROW_COUNT_CHANGED_THROTTLE_MS 250

enum {
	BEFORE_REBUILD,
	AFTER_REBUILD,
	ROWS_CHANGED,
	ROWS_INSERTED,
	ROWS_REMOVED,
	ROW_COUNT_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static gconstpointer
virtual_tree_model_default_get_row_key (EVirtualTreeModel *self,
					GObject *row_object)
{
	return row_object;
}

static EVirtualTreeKeyType
virtual_tree_model_default_get_key_type (EVirtualTreeModel *self)
{
	EVirtualTreeKeyType kt = {
		g_direct_hash,
		g_direct_equal,
		NULL,
		NULL
	};
	return kt;
}

static GObject *
virtual_tree_model_default_dup_row (EVirtualTreeModel *self,
				    guint row_index)
{
	GPtrArray *rows;
	GObject *result = NULL;

	rows = e_virtual_tree_model_dup_rows (self, row_index, row_index, FALSE);
	if (rows && rows->len > 0)
		result = g_object_ref (g_ptr_array_index (rows, 0));
	g_clear_pointer (&rows, g_ptr_array_unref);

	return result;
}

static guint
virtual_tree_model_default_find_row_by_key (EVirtualTreeModel *self,
					    gconstpointer key)
{
	EVirtualTreeKeyType key_type;
	guint total, ii;

	key_type = e_virtual_tree_model_get_key_type (self);
	total = e_virtual_tree_model_get_row_count (self);

	for (ii = 0; ii < total; ii++) {
		GObject *row_obj = e_virtual_tree_model_dup_row (self, ii);
		gconstpointer row_key;

		if (!row_obj)
			continue;

		row_key = e_virtual_tree_model_get_row_key (self, row_obj);
		g_object_unref (row_obj);

		if (row_key && key_type.equal_func (row_key, key))
			return ii;
	}

	return G_MAXUINT;
}

G_DEFINE_INTERFACE (EVirtualTreeModel, e_virtual_tree_model, G_TYPE_OBJECT)

static void
e_virtual_tree_model_default_init (EVirtualTreeModelInterface *iface)
{
	iface->get_row_key = virtual_tree_model_default_get_row_key;
	iface->get_key_type = virtual_tree_model_default_get_key_type;
	iface->dup_row = virtual_tree_model_default_dup_row;
	iface->find_row_by_key = virtual_tree_model_default_find_row_by_key;

	signals[BEFORE_REBUILD] = g_signal_new (
		"before-rebuild",
		G_TYPE_FROM_INTERFACE (iface),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EVirtualTreeModelInterface, before_rebuild),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0);

	signals[AFTER_REBUILD] = g_signal_new (
		"after-rebuild",
		G_TYPE_FROM_INTERFACE (iface),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EVirtualTreeModelInterface, after_rebuild),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0);

	signals[ROWS_CHANGED] = g_signal_new (
		"rows-changed",
		G_TYPE_FROM_INTERFACE (iface),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EVirtualTreeModelInterface, rows_changed),
		NULL, NULL, NULL,
		G_TYPE_NONE, 2,
		G_TYPE_UINT,
		G_TYPE_UINT);

	signals[ROWS_INSERTED] = g_signal_new (
		"rows-inserted",
		G_TYPE_FROM_INTERFACE (iface),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EVirtualTreeModelInterface, rows_inserted),
		NULL, NULL, NULL,
		G_TYPE_NONE, 2,
		G_TYPE_UINT,
		G_TYPE_UINT);

	signals[ROWS_REMOVED] = g_signal_new (
		"rows-removed",
		G_TYPE_FROM_INTERFACE (iface),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EVirtualTreeModelInterface, rows_removed),
		NULL, NULL, NULL,
		G_TYPE_NONE, 2,
		G_TYPE_UINT,
		G_TYPE_UINT);

	signals[ROW_COUNT_CHANGED] = g_signal_new (
		"row-count-changed",
		G_TYPE_FROM_INTERFACE (iface),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EVirtualTreeModelInterface, row_count_changed),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0);
}

/**
 * e_virtual_tree_model_get_row_count:
 * @self: an #EVirtualTreeModel
 *
 * Returns the total number of visible rows in the model.
 *
 * Returns: the row count
 *
 * Since: 3.64
 **/
guint
e_virtual_tree_model_get_row_count (EVirtualTreeModel *self)
{
	EVirtualTreeModelInterface *iface;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE_MODEL (self), 0);

	iface = E_VIRTUAL_TREE_MODEL_GET_IFACE (self);
	g_return_val_if_fail (iface->get_row_count != NULL, 0);

	return iface->get_row_count (self);
}

/**
 * e_virtual_tree_model_dup_rows:
 * @self: an #EVirtualTreeModel
 * @first_row: zero-based index of the first row to retrieve
 * @last_row: zero-based index of the last row to retrieve (inclusive)
 * @include_collapsed: whether to include rows inside collapsed nodes
 *
 * Returns a #GPtrArray of #GObject row objects for the given range.
 * Each element has its reference count incremented; the caller must
 * release each element with g_object_unref() and the array with
 * g_ptr_array_unref().
 *
 * Returns: (transfer container) (element-type GObject): the rows
 *
 * Since: 3.64
 **/
GPtrArray *
e_virtual_tree_model_dup_rows (EVirtualTreeModel *self,
			       guint first_row,
			       guint last_row,
			       gboolean include_collapsed)
{
	EVirtualTreeModelInterface *iface;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE_MODEL (self), NULL);

	iface = E_VIRTUAL_TREE_MODEL_GET_IFACE (self);
	g_return_val_if_fail (iface->dup_rows != NULL, NULL);

	return iface->dup_rows (self, first_row, last_row, include_collapsed);
}

/**
 * e_virtual_tree_model_dup_row:
 * @self: an #EVirtualTreeModel
 * @row_index: zero-based row index
 *
 * Returns the row object at @row_index with an added reference.
 * The caller must release the reference with g_object_unref() when done.
 *
 * The default implementation delegates to e_virtual_tree_model_dup_rows().
 *
 * Returns: (transfer full) (nullable): the row object, or %NULL
 *
 * Since: 3.64
 **/
GObject *
e_virtual_tree_model_dup_row (EVirtualTreeModel *self,
			      guint row_index)
{
	EVirtualTreeModelInterface *iface;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE_MODEL (self), NULL);

	iface = E_VIRTUAL_TREE_MODEL_GET_IFACE (self);
	g_return_val_if_fail (iface->dup_row != NULL, NULL);

	return iface->dup_row (self, row_index);
}

/**
 * e_virtual_tree_model_get_depth:
 * @self: an #EVirtualTreeModel
 * @row_object: a row #GObject
 *
 * Returns the nesting depth of @row_object. Top-level rows have depth 0.
 *
 * Returns: the depth
 *
 * Since: 3.64
 **/
guint
e_virtual_tree_model_get_depth (EVirtualTreeModel *self,
				GObject *row_object)
{
	EVirtualTreeModelInterface *iface;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE_MODEL (self), 0);
	g_return_val_if_fail (G_IS_OBJECT (row_object), 0);

	iface = E_VIRTUAL_TREE_MODEL_GET_IFACE (self);
	g_return_val_if_fail (iface->get_depth != NULL, 0);

	return iface->get_depth (self, row_object);
}

/**
 * e_virtual_tree_model_is_expandable:
 * @self: an #EVirtualTreeModel
 * @row_object: a row #GObject
 *
 * Returns whether @row_object can be expanded to show children.
 *
 * Returns: %TRUE if the row is expandable
 *
 * Since: 3.64
 **/
gboolean
e_virtual_tree_model_is_expandable (EVirtualTreeModel *self,
				    GObject *row_object)
{
	EVirtualTreeModelInterface *iface;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE_MODEL (self), FALSE);
	g_return_val_if_fail (G_IS_OBJECT (row_object), FALSE);

	iface = E_VIRTUAL_TREE_MODEL_GET_IFACE (self);
	g_return_val_if_fail (iface->is_expandable != NULL, FALSE);

	return iface->is_expandable (self, row_object);
}

/**
 * e_virtual_tree_model_get_expanded:
 * @self: an #EVirtualTreeModel
 * @row_object: a row #GObject
 *
 * Returns whether @row_object is currently expanded.
 *
 * Returns: %TRUE if the row is expanded
 *
 * Since: 3.64
 **/
gboolean
e_virtual_tree_model_get_expanded (EVirtualTreeModel *self,
				   GObject *row_object)
{
	EVirtualTreeModelInterface *iface;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE_MODEL (self), FALSE);
	g_return_val_if_fail (G_IS_OBJECT (row_object), FALSE);

	iface = E_VIRTUAL_TREE_MODEL_GET_IFACE (self);
	g_return_val_if_fail (iface->get_expanded != NULL, FALSE);

	return iface->get_expanded (self, row_object);
}

/**
 * e_virtual_tree_model_set_expanded:
 * @self: an #EVirtualTreeModel
 * @row_object: a row #GObject
 * @expanded: %TRUE to expand, %FALSE to collapse
 *
 * Sets the expanded state of @row_object.
 *
 * Since: 3.64
 **/
void
e_virtual_tree_model_set_expanded (EVirtualTreeModel *self,
				   GObject *row_object,
				   gboolean expanded)
{
	EVirtualTreeModelInterface *iface;

	g_return_if_fail (E_IS_VIRTUAL_TREE_MODEL (self));
	g_return_if_fail (G_IS_OBJECT (row_object));

	iface = E_VIRTUAL_TREE_MODEL_GET_IFACE (self);
	g_return_if_fail (iface->set_expanded != NULL);

	iface->set_expanded (self, row_object, expanded);
}

/**
 * e_virtual_tree_model_get_row_key:
 * @self: an #EVirtualTreeModel
 * @row_object: a row #GObject
 *
 * Returns an opaque key that uniquely identifies @row_object for
 * selection tracking. The returned pointer is owned by @row_object
 * and remains valid only while @row_object is alive; callers must
 * not free it.
 *
 * The default implementation returns @row_object itself (pointer identity).
 *
 * Returns: (transfer none) (nullable): the key, or %NULL
 *
 * Since: 3.64
 **/
gconstpointer
e_virtual_tree_model_get_row_key (EVirtualTreeModel *self,
				  GObject *row_object)
{
	EVirtualTreeModelInterface *iface;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE_MODEL (self), NULL);
	g_return_val_if_fail (G_IS_OBJECT (row_object), NULL);

	iface = E_VIRTUAL_TREE_MODEL_GET_IFACE (self);
	g_return_val_if_fail (iface->get_row_key != NULL, NULL);

	return iface->get_row_key (self, row_object);
}

/**
 * e_virtual_tree_model_get_key_type:
 * @self: an #EVirtualTreeModel
 *
 * Returns an #EVirtualTreeKeyType describing how to hash, compare,
 * copy, and free the keys returned by e_virtual_tree_model_get_row_key().
 *
 * The default returns g_direct_hash/g_direct_equal with %NULL copy
 * and free functions (pointer identity, no allocation).
 *
 * Returns: the key type descriptor
 *
 * Since: 3.64
 **/
EVirtualTreeKeyType
e_virtual_tree_model_get_key_type (EVirtualTreeModel *self)
{
	EVirtualTreeModelInterface *iface;
	EVirtualTreeKeyType empty = { NULL, NULL, NULL, NULL };

	g_return_val_if_fail (E_IS_VIRTUAL_TREE_MODEL (self), empty);

	iface = E_VIRTUAL_TREE_MODEL_GET_IFACE (self);
	g_return_val_if_fail (iface->get_key_type != NULL, empty);

	return iface->get_key_type (self);
}

/**
 * e_virtual_tree_model_find_row_by_key:
 * @self: an #EVirtualTreeModel
 * @key: the key to search for
 *
 * Searches the model for a row whose key (as returned by
 * e_virtual_tree_model_get_row_key()) equals @key according to the
 * model's key type equal function.
 *
 * The default implementation performs a linear scan.
 *
 * Returns: the row index, or %G_MAXUINT if not found
 *
 * Since: 3.64
 **/
guint
e_virtual_tree_model_find_row_by_key (EVirtualTreeModel *self,
				      gconstpointer key)
{
	EVirtualTreeModelInterface *iface;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE_MODEL (self), G_MAXUINT);
	g_return_val_if_fail (key != NULL, G_MAXUINT);

	iface = E_VIRTUAL_TREE_MODEL_GET_IFACE (self);
	g_return_val_if_fail (iface->find_row_by_key != NULL, G_MAXUINT);

	return iface->find_row_by_key (self, key);
}

/**
 * e_virtual_tree_model_emit_before_rebuild:
 * @self: an #EVirtualTreeModel
 *
 * Emits the #EVirtualTreeModel::before-rebuild signal. Model
 * implementations should call this before bulk data changes that
 * invalidate all existing row indices. Call
 * e_virtual_tree_model_emit_after_rebuild() when done.
 *
 * Since: 3.64
 **/
void
e_virtual_tree_model_emit_before_rebuild (EVirtualTreeModel *self)
{
	g_return_if_fail (E_IS_VIRTUAL_TREE_MODEL (self));

	g_signal_emit (self, signals[BEFORE_REBUILD], 0);
}

/**
 * e_virtual_tree_model_emit_after_rebuild:
 * @self: an #EVirtualTreeModel
 *
 * Emits the #EVirtualTreeModel::after-rebuild signal. Finishes
 * a rebuild previously announced with
 * e_virtual_tree_model_emit_before_rebuild(), so the tree can
 * re-query row data.
 *
 * Since: 3.64
 **/
void
e_virtual_tree_model_emit_after_rebuild (EVirtualTreeModel *self)
{
	g_return_if_fail (E_IS_VIRTUAL_TREE_MODEL (self));

	g_signal_emit (self, signals[AFTER_REBUILD], 0);
}

/**
 * e_virtual_tree_model_emit_rows_changed:
 * @self: an #EVirtualTreeModel
 * @first_row: zero-based index of the first changed row
 * @last_row: zero-based index of the last changed row (inclusive)
 *
 * Emits the #EVirtualTreeModel::rows-changed signal to notify the
 * tree that existing rows in the range [@first_row, @last_row] have
 * new data and need to be re-rendered.
 *
 * Since: 3.64
 **/
void
e_virtual_tree_model_emit_rows_changed (EVirtualTreeModel *self,
					guint first_row,
					guint last_row)
{
	g_return_if_fail (E_IS_VIRTUAL_TREE_MODEL (self));

	g_signal_emit (self, signals[ROWS_CHANGED], 0, first_row, last_row);
}

/**
 * e_virtual_tree_model_emit_rows_inserted:
 * @self: an #EVirtualTreeModel
 * @first_row: zero-based index of the first inserted row
 * @last_row: zero-based index of the last inserted row (inclusive)
 *
 * Emits the #EVirtualTreeModel::rows-inserted signal to notify the
 * tree that new rows have been inserted at indices [@first_row, @last_row].
 *
 * Since: 3.64
 **/
void
e_virtual_tree_model_emit_rows_inserted (EVirtualTreeModel *self,
					 guint first_row,
					 guint last_row)
{
	g_return_if_fail (E_IS_VIRTUAL_TREE_MODEL (self));

	g_signal_emit (self, signals[ROWS_INSERTED], 0, first_row, last_row);
}

/**
 * e_virtual_tree_model_emit_rows_removed:
 * @self: an #EVirtualTreeModel
 * @first_row: zero-based index of the first removed row
 * @last_row: zero-based index of the last removed row (inclusive)
 *
 * Emits the #EVirtualTreeModel::rows-removed signal to notify the
 * tree that rows at indices [@first_row, @last_row] have been removed.
 *
 * Since: 3.64
 **/
void
e_virtual_tree_model_emit_rows_removed (EVirtualTreeModel *self,
					guint first_row,
					guint last_row)
{
	g_return_if_fail (E_IS_VIRTUAL_TREE_MODEL (self));

	g_signal_emit (self, signals[ROWS_REMOVED], 0, first_row, last_row);
}

typedef struct _RowCountThrottle {
	guint timeout_id;
	gboolean pending;
} RowCountThrottle;

static void
row_count_throttle_free (gpointer data)
{
	RowCountThrottle *throttle = data;

	if (throttle->timeout_id)
		g_source_remove (throttle->timeout_id);

	g_free (throttle);
}

static RowCountThrottle *
row_count_throttle_get (EVirtualTreeModel *self)
{
	RowCountThrottle *throttle;

	throttle = g_object_get_data (G_OBJECT (self), "e-virtual-tree-model-row-count-throttle");

	if (!throttle) {
		throttle = g_new0 (RowCountThrottle, 1);
		g_object_set_data_full (G_OBJECT (self), "e-virtual-tree-model-row-count-throttle", throttle, row_count_throttle_free);
	}

	return throttle;
}

static gboolean
row_count_throttle_timeout_cb (gpointer user_data)
{
	EVirtualTreeModel *self = user_data;
	RowCountThrottle *throttle = row_count_throttle_get (self);

	throttle->timeout_id = 0;

	if (throttle->pending) {
		throttle->pending = FALSE;
		g_signal_emit (self, signals[ROW_COUNT_CHANGED], 0);
		throttle->timeout_id = e_named_timeout_add (ROW_COUNT_CHANGED_THROTTLE_MS, row_count_throttle_timeout_cb, self);
	}

	return G_SOURCE_REMOVE;
}

/**
 * e_virtual_tree_model_emit_row_count_changed:
 * @self: an #EVirtualTreeModel
 *
 * Emits the #EVirtualTreeModel::row-count-changed signal to notify
 * the tree that the total number of rows has changed.
 *
 * Since: 3.64
 **/
void
e_virtual_tree_model_emit_row_count_changed (EVirtualTreeModel *self)
{
	RowCountThrottle *throttle;

	g_return_if_fail (E_IS_VIRTUAL_TREE_MODEL (self));

	throttle = row_count_throttle_get (self);

	if (throttle->timeout_id) {
		throttle->pending = TRUE;
		return;
	}

	g_signal_emit (self, signals[ROW_COUNT_CHANGED], 0);

	throttle->timeout_id = e_named_timeout_add (ROW_COUNT_CHANGED_THROTTLE_MS, row_count_throttle_timeout_cb, self);
}

typedef struct _PendingRowNotify {
	gpointer key;
	gboolean is_added;
} PendingRowNotify;

typedef struct _RowNotifyThrottle {
	guint timeout_id;
	GPtrArray *pending; /* PendingRowNotify *, owned */
	EVirtualTreeKeyType key_type;
} RowNotifyThrottle;

static void
pending_row_notify_free_one (RowNotifyThrottle *throttle,
			     PendingRowNotify *notify)
{
	if (throttle->key_type.free_func)
		throttle->key_type.free_func (notify->key);

	g_free (notify);
}

static void
row_notify_throttle_free (gpointer data)
{
	RowNotifyThrottle *throttle = data;
	guint ii;

	if (throttle->timeout_id)
		g_source_remove (throttle->timeout_id);

	for (ii = 0; ii < throttle->pending->len; ii++)
		pending_row_notify_free_one (throttle, g_ptr_array_index (throttle->pending, ii));

	g_ptr_array_free (throttle->pending, TRUE);
	g_free (throttle);
}

static RowNotifyThrottle *
row_notify_throttle_get (EVirtualTreeModel *self)
{
	RowNotifyThrottle *throttle;

	throttle = g_object_get_data (G_OBJECT (self), "e-virtual-tree-model-row-notify-throttle");

	if (!throttle) {
		throttle = g_new0 (RowNotifyThrottle, 1);
		throttle->pending = g_ptr_array_new ();
		throttle->key_type = e_virtual_tree_model_get_key_type (self);
		g_object_set_data_full (G_OBJECT (self), "e-virtual-tree-model-row-notify-throttle", throttle, row_notify_throttle_free);
	}

	return throttle;
}

static void
row_notify_emit_for_key (EVirtualTreeModel *self,
			 gconstpointer key,
			 gboolean is_added)
{
	guint row_index = e_virtual_tree_model_find_row_by_key (self, key);

	if (row_index == G_MAXUINT)
		return;

	if (is_added)
		e_virtual_tree_model_emit_rows_inserted (self, row_index, row_index);
	else
		e_virtual_tree_model_emit_rows_changed (self, row_index, row_index);
}

static gboolean
row_notify_throttle_timeout_cb (gpointer user_data)
{
	EVirtualTreeModel *self = user_data;
	RowNotifyThrottle *throttle = row_notify_throttle_get (self);
	guint ii;

	throttle->timeout_id = 0;

	if (throttle->pending->len == 0)
		return G_SOURCE_REMOVE;

	for (ii = 0; ii < throttle->pending->len; ii++) {
		PendingRowNotify *notify = g_ptr_array_index (throttle->pending, ii);

		row_notify_emit_for_key (self, notify->key, notify->is_added);
		pending_row_notify_free_one (throttle, notify);
	}

	g_ptr_array_set_size (throttle->pending, 0);

	throttle->timeout_id = e_named_timeout_add (ROW_COUNT_CHANGED_THROTTLE_MS, row_notify_throttle_timeout_cb, self);

	return G_SOURCE_REMOVE;
}

/**
 * e_virtual_tree_model_notify_row_changed:
 * @self: an #EVirtualTreeModel
 * @key: the row key of the added/changed row, as returned by e_virtual_tree_model_get_row_key()
 * @is_added: %TRUE when the row was newly added, %FALSE when an existing row changed
 *
 * Notifies the tree that a single row identified by @key was added or
 * changed, without the caller having to know its current row index.
 * The index is resolved via e_virtual_tree_model_find_row_by_key() at
 * the time the notification is actually emitted, which is throttled to
 * a few times per second per model instance, so bursts of many calls
 * (e.g. from a bulk data load) coalesce instead of each one triggering
 * a full tree refill.
 *
 * Since: 3.64
 **/
void
e_virtual_tree_model_notify_row_changed (EVirtualTreeModel *self,
					 gconstpointer key,
					 gboolean is_added)
{
	RowNotifyThrottle *throttle;

	g_return_if_fail (E_IS_VIRTUAL_TREE_MODEL (self));

	throttle = row_notify_throttle_get (self);

	if (throttle->timeout_id) {
		PendingRowNotify *notify;

		notify = g_new0 (PendingRowNotify, 1);
		notify->key = throttle->key_type.copy_func ? throttle->key_type.copy_func ((gpointer) key) : (gpointer) key;
		notify->is_added = is_added;

		g_ptr_array_add (throttle->pending, notify);

		return;
	}

	row_notify_emit_for_key (self, key, is_added);

	throttle->timeout_id = e_named_timeout_add (ROW_COUNT_CHANGED_THROTTLE_MS, row_notify_throttle_timeout_cb, self);
}
