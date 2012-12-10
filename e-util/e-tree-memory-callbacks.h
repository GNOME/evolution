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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_TREE_MEMORY_CALLBACKS_H_
#define _E_TREE_MEMORY_CALLBACKS_H_

#include <e-util/e-tree-memory.h>

/* Standard GObject macros */
#define E_TYPE_TREE_MEMORY_CALLBACKS \
	(e_tree_memory_callbacks_get_type ())
#define E_TREE_MEMORY_CALLBACKS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TREE_MEMORY_CALLBACKS, ETreeMemoryCallbacks))
#define E_TREE_MEMORY_CALLBACKS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TREE_MEMORY_CALLBACKS, ETreeMemoryCallbacksClass))
#define E_IS_TREE_MEMORY_CALLBACKS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TREE_MEMORY_CALLBACKS))
#define E_IS_TREE_MEMORY_CALLBACKS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TREE_MEMORY_CALLBACKS))
#define E_TREE_MEMORY_CALLBACKS_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TREE_MEMORY_CALLBACKS, ETreeMemoryCallbacksClass))

G_BEGIN_DECLS

typedef struct _ETreeMemoryCallbacks ETreeMemoryCallbacks;
typedef struct _ETreeMemoryCallbacksClass ETreeMemoryCallbacksClass;

typedef GdkPixbuf *	(*ETreeMemoryCallbacksIconAtFn)
						(ETreeModel *etree,
						 ETreePath path,
						 gpointer model_data);

typedef gint		(*ETreeMemoryCallbacksColumnCountFn)
						(ETreeModel *etree,
						 gpointer model_data);

typedef gboolean	(*ETreeMemoryCallbacksHasSaveIdFn)
						(ETreeModel *etree,
						 gpointer model_data);
typedef gchar *		(*ETreeMemoryCallbacksGetSaveIdFn)
						(ETreeModel *etree,
						 ETreePath path,
						 gpointer model_data);

typedef gboolean	(*ETreeMemoryCallbacksHasGetNodeByIdFn)
						(ETreeModel *etree,
						 gpointer model_data);
typedef ETreePath	(*ETreeMemoryCallbacksGetNodeByIdFn)
						(ETreeModel *etree,
						 const gchar *save_id,
						 gpointer model_data);

typedef gpointer	(*ETreeMemoryCallbacksValueAtFn)
						(ETreeModel *etree,
						 ETreePath path,
						 gint col,
						 gpointer model_data);
typedef void		(*ETreeMemoryCallbacksSetValueAtFn)
						(ETreeModel *etree,
						 ETreePath path,
						 gint col,
						 gconstpointer val,
						 gpointer model_data);
typedef gboolean	(*ETreeMemoryCallbacksIsEditableFn)
						(ETreeModel *etree,
						 ETreePath path,
						 gint col,
						 gpointer model_data);

typedef gpointer	(*ETreeMemoryCallbacksDuplicateValueFn)
						(ETreeModel *etm,
						 gint col,
						 gconstpointer val,
						 gpointer data);
typedef void		(*ETreeMemoryCallbacksFreeValueFn)
						(ETreeModel *etm,
						 gint col,
						 gpointer val,
						 gpointer data);
typedef gpointer	(*ETreeMemoryCallbacksInitializeValueFn)
						(ETreeModel *etm,
						 gint col,
						 gpointer data);
typedef gboolean	(*ETreeMemoryCallbacksValueIsEmptyFn)
						(ETreeModel *etm,
						 gint col,
						 gconstpointer val,
						 gpointer data);
typedef gchar *		(*ETreeMemoryCallbacksValueToStringFn)
						(ETreeModel *etm,
						 gint col,
						 gconstpointer val,
						 gpointer data);

struct _ETreeMemoryCallbacks {
	ETreeMemory parent;

	ETreeMemoryCallbacksIconAtFn icon_at;

	ETreeMemoryCallbacksColumnCountFn     column_count;

	ETreeMemoryCallbacksHasSaveIdFn       has_save_id;
	ETreeMemoryCallbacksGetSaveIdFn       get_save_id;

	ETreeMemoryCallbacksHasGetNodeByIdFn  has_get_node_by_id;
	ETreeMemoryCallbacksGetNodeByIdFn     get_node_by_id;

	ETreeMemoryCallbacksValueAtFn         sort_value_at;
	ETreeMemoryCallbacksValueAtFn         value_at;
	ETreeMemoryCallbacksSetValueAtFn      set_value_at;
	ETreeMemoryCallbacksIsEditableFn      is_editable;

	ETreeMemoryCallbacksDuplicateValueFn  duplicate_value;
	ETreeMemoryCallbacksFreeValueFn       free_value;
	ETreeMemoryCallbacksInitializeValueFn initialize_value;
	ETreeMemoryCallbacksValueIsEmptyFn    value_is_empty;
	ETreeMemoryCallbacksValueToStringFn   value_to_string;

	gpointer model_data;
};

struct _ETreeMemoryCallbacksClass {
	ETreeMemoryClass parent_class;
};

GType		e_tree_memory_callbacks_get_type
				(void) G_GNUC_CONST;
ETreeModel *	e_tree_memory_callbacks_new
				(ETreeMemoryCallbacksIconAtFn icon_at,

				 ETreeMemoryCallbacksColumnCountFn column_count,

				 ETreeMemoryCallbacksHasSaveIdFn has_save_id,
				 ETreeMemoryCallbacksGetSaveIdFn get_save_id,

				 ETreeMemoryCallbacksHasGetNodeByIdFn has_get_node_by_id,
				 ETreeMemoryCallbacksGetNodeByIdFn get_node_by_id,

				 ETreeMemoryCallbacksValueAtFn sort_value_at,
				 ETreeMemoryCallbacksValueAtFn value_at,
				 ETreeMemoryCallbacksSetValueAtFn set_value_at,
				 ETreeMemoryCallbacksIsEditableFn is_editable,

				 ETreeMemoryCallbacksDuplicateValueFn duplicate_value,
				 ETreeMemoryCallbacksFreeValueFn free_value,
				 ETreeMemoryCallbacksInitializeValueFn initialize_value,
				 ETreeMemoryCallbacksValueIsEmptyFn value_is_empty,
				 ETreeMemoryCallbacksValueToStringFn value_to_string,

				 gpointer model_data);

G_END_DECLS

#endif /* _E_TREE_MEMORY_CALLBACKS_H_ */
