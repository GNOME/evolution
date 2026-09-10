/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_VIRTUAL_TREE_MODEL_H
#define E_VIRTUAL_TREE_MODEL_H

#include <glib-object.h>

G_BEGIN_DECLS

#define E_TYPE_VIRTUAL_TREE_MODEL (e_virtual_tree_model_get_type ())
G_DECLARE_INTERFACE (EVirtualTreeModel, e_virtual_tree_model, E, VIRTUAL_TREE_MODEL, GObject)

typedef struct {
	GHashFunc      hash_func;
	GEqualFunc     equal_func;
	GBoxedCopyFunc copy_func;
	GDestroyNotify free_func;
} EVirtualTreeKeyType;

struct _EVirtualTreeModelInterface {
	GTypeInterface parent_interface;

	guint		(*get_row_count)	(EVirtualTreeModel *self);

	GPtrArray *	(*dup_rows)		(EVirtualTreeModel *self,
						 guint first_row,
						 guint last_row,
						 gboolean include_collapsed);

	GObject *	(*dup_row)		(EVirtualTreeModel *self,
						 guint row_index);

	guint		(*get_depth)		(EVirtualTreeModel *self,
						 GObject *row_object);
	gboolean	(*is_expandable)	(EVirtualTreeModel *self,
						 GObject *row_object);
	gboolean	(*get_expanded)		(EVirtualTreeModel *self,
						 GObject *row_object);
	void		(*set_expanded)		(EVirtualTreeModel *self,
						 GObject *row_object,
						 gboolean expanded);

	gconstpointer	(*get_row_key)		(EVirtualTreeModel *self,
						 GObject *row_object);
	EVirtualTreeKeyType (*get_key_type)	(EVirtualTreeModel *self);

	guint		(*find_row_by_key)	(EVirtualTreeModel *self,
						 gconstpointer key);

	/* Signals */
	void		(*before_rebuild)	(EVirtualTreeModel *self);
	void		(*after_rebuild)	(EVirtualTreeModel *self);
	void		(*rows_changed)		(EVirtualTreeModel *self,
						 guint first_row,
						 guint last_row);
	void		(*rows_inserted)	(EVirtualTreeModel *self,
						 guint first_row,
						 guint last_row);
	void		(*rows_removed)		(EVirtualTreeModel *self,
						 guint first_row,
						 guint last_row);
	void		(*row_count_changed)	(EVirtualTreeModel *self);
};

guint		e_virtual_tree_model_get_row_count	(EVirtualTreeModel *self);
GPtrArray *	e_virtual_tree_model_dup_rows		(EVirtualTreeModel *self,
							 guint first_row,
							 guint last_row,
							 gboolean include_collapsed);
GObject *	e_virtual_tree_model_dup_row		(EVirtualTreeModel *self,
							 guint row_index);
guint		e_virtual_tree_model_get_depth		(EVirtualTreeModel *self,
							 GObject *row_object);
gboolean	e_virtual_tree_model_is_expandable	(EVirtualTreeModel *self,
							 GObject *row_object);
gboolean	e_virtual_tree_model_get_expanded	(EVirtualTreeModel *self,
							 GObject *row_object);
void		e_virtual_tree_model_set_expanded	(EVirtualTreeModel *self,
							 GObject *row_object,
							 gboolean expanded);

gconstpointer	e_virtual_tree_model_get_row_key	(EVirtualTreeModel *self,
							 GObject *row_object);
EVirtualTreeKeyType e_virtual_tree_model_get_key_type	(EVirtualTreeModel *self);

guint		e_virtual_tree_model_find_row_by_key	(EVirtualTreeModel *self,
							 gconstpointer key);

void		e_virtual_tree_model_emit_before_rebuild
							(EVirtualTreeModel *self);
void		e_virtual_tree_model_emit_after_rebuild	(EVirtualTreeModel *self);

void		e_virtual_tree_model_emit_rows_changed	(EVirtualTreeModel *self,
							 guint first_row,
							 guint last_row);
void		e_virtual_tree_model_emit_rows_inserted	(EVirtualTreeModel *self,
							 guint first_row,
							 guint last_row);
void		e_virtual_tree_model_emit_rows_removed	(EVirtualTreeModel *self,
							 guint first_row,
							 guint last_row);
void		e_virtual_tree_model_emit_row_count_changed
							(EVirtualTreeModel *self);

void		e_virtual_tree_model_notify_row_changed	(EVirtualTreeModel *self,
								 gconstpointer key,
								 gboolean is_added);

G_END_DECLS

#endif /* E_VIRTUAL_TREE_MODEL_H */
