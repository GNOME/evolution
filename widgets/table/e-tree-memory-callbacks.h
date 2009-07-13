/*
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
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_TREE_MEMORY_CALLBACKS_H_
#define _E_TREE_MEMORY_CALLBACKS_H_

#include <table/e-tree-memory.h>

G_BEGIN_DECLS

#define E_TREE_MEMORY_CALLBACKS_TYPE        (e_tree_memory_callbacks_get_type ())
#define E_TREE_MEMORY_CALLBACKS(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TREE_MEMORY_CALLBACKS_TYPE, ETreeMemoryCallbacks))
#define E_TREE_MEMORY_CALLBACKS_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TREE_MEMORY_CALLBACKS_TYPE, ETreeMemoryCallbacksClass))
#define E_IS_TREE_MEMORY_CALLBACKS(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TREE_MEMORY_CALLBACKS_TYPE))
#define E_IS_TREE_MEMORY_CALLBACKS_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TREE_MEMORY_CALLBACKS_TYPE))

typedef GdkPixbuf* (*ETreeMemoryCallbacksIconAtFn)             (ETreeModel *etree, ETreePath path, gpointer model_data);

typedef gint       (*ETreeMemoryCallbacksColumnCountFn)        (ETreeModel *etree, gpointer model_data);

typedef gboolean   (*ETreeMemoryCallbacksHasSaveIdFn)          (ETreeModel *etree, gpointer model_data);
typedef gchar     *(*ETreeMemoryCallbacksGetSaveIdFn)          (ETreeModel *etree, ETreePath path, gpointer model_data);

typedef gboolean   (*ETreeMemoryCallbacksHasGetNodeByIdFn)     (ETreeModel *etree, gpointer model_data);
typedef ETreePath  (*ETreeMemoryCallbacksGetNodeByIdFn)        (ETreeModel *etree, const gchar *save_id, gpointer model_data);

typedef gpointer       (*ETreeMemoryCallbacksValueAtFn)            (ETreeModel *etree, ETreePath path, gint col, gpointer model_data);
typedef void       (*ETreeMemoryCallbacksSetValueAtFn)         (ETreeModel *etree, ETreePath path, gint col, gconstpointer val, gpointer model_data);
typedef gboolean   (*ETreeMemoryCallbacksIsEditableFn)         (ETreeModel *etree, ETreePath path, gint col, gpointer model_data);

typedef	void      *(*ETreeMemoryCallbacksDuplicateValueFn)     (ETreeModel *etm, gint col, gconstpointer val, gpointer data);
typedef	void       (*ETreeMemoryCallbacksFreeValueFn)          (ETreeModel *etm, gint col, gpointer val, gpointer data);
typedef void      *(*ETreeMemoryCallbacksInitializeValueFn)    (ETreeModel *etm, gint col, gpointer data);
typedef gboolean   (*ETreeMemoryCallbacksValueIsEmptyFn)       (ETreeModel *etm, gint col, gconstpointer val, gpointer data);
typedef gchar      *(*ETreeMemoryCallbacksValueToStringFn)      (ETreeModel *etm, gint col, gconstpointer val, gpointer data);

typedef struct {
	ETreeMemory parent;

	ETreeMemoryCallbacksIconAtFn icon_at;

	ETreeMemoryCallbacksColumnCountFn     column_count;

	ETreeMemoryCallbacksHasSaveIdFn       has_save_id;
	ETreeMemoryCallbacksGetSaveIdFn       get_save_id;

	ETreeMemoryCallbacksHasGetNodeByIdFn  has_get_node_by_id;
	ETreeMemoryCallbacksGetNodeByIdFn     get_node_by_id;

	ETreeMemoryCallbacksValueAtFn	      sort_value_at;
	ETreeMemoryCallbacksValueAtFn         value_at;
	ETreeMemoryCallbacksSetValueAtFn      set_value_at;
	ETreeMemoryCallbacksIsEditableFn      is_editable;

	ETreeMemoryCallbacksDuplicateValueFn  duplicate_value;
	ETreeMemoryCallbacksFreeValueFn       free_value;
	ETreeMemoryCallbacksInitializeValueFn initialize_value;
	ETreeMemoryCallbacksValueIsEmptyFn    value_is_empty;
	ETreeMemoryCallbacksValueToStringFn   value_to_string;

	gpointer model_data;
} ETreeMemoryCallbacks;

typedef struct {
	ETreeMemoryClass parent_class;
} ETreeMemoryCallbacksClass;

GType   e_tree_memory_callbacks_get_type (void);

ETreeModel *e_tree_memory_callbacks_new  (ETreeMemoryCallbacksIconAtFn icon_at,

					  ETreeMemoryCallbacksColumnCountFn        column_count,

					  ETreeMemoryCallbacksHasSaveIdFn          has_save_id,
					  ETreeMemoryCallbacksGetSaveIdFn          get_save_id,

					  ETreeMemoryCallbacksHasGetNodeByIdFn     has_get_node_by_id,
					  ETreeMemoryCallbacksGetNodeByIdFn        get_node_by_id,

					  ETreeMemoryCallbacksValueAtFn		   sort_value_at,
					  ETreeMemoryCallbacksValueAtFn            value_at,
					  ETreeMemoryCallbacksSetValueAtFn         set_value_at,
					  ETreeMemoryCallbacksIsEditableFn         is_editable,

					  ETreeMemoryCallbacksDuplicateValueFn     duplicate_value,
					  ETreeMemoryCallbacksFreeValueFn          free_value,
					  ETreeMemoryCallbacksInitializeValueFn    initialize_value,
					  ETreeMemoryCallbacksValueIsEmptyFn       value_is_empty,
					  ETreeMemoryCallbacksValueToStringFn      value_to_string,

					  gpointer                                 model_data);

G_END_DECLS

#endif /* _E_TREE_MEMORY_CALLBACKS_H_ */
