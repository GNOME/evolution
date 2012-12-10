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

#ifndef _E_TABLE_MEMORY_CALLBACKS_H_
#define _E_TABLE_MEMORY_CALLBACKS_H_

#include <e-util/e-table-memory.h>

/* Standard GObject macros */
#define E_TYPE_TABLE_MEMORY_CALLBACKS \
	(e_table_memory_callbacks_get_type ())
#define E_TABLE_MEMORY_CALLBACKS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TABLE_MEMORY_CALLBACKS, ETableMemoryCallbacks))
#define E_TABLE_MEMORY_CALLBACKS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TABLE_MEMORY_CALLBACKS, ETableMemoryCallbacksClass))
#define E_IS_TABLE_MEMORY_CALLBACKS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TABLE_MEMORY_CALLBACKS))
#define E_IS_TABLE_MEMORY_CALLBACKS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TABLE_MEMORY_CALLBACKS))
#define E_TABLE_MEMORY_CALLBACKS_GET_CLASS(cls) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((cls), E_TYPE_TABLE_MEMORY_CALLBACKS, ETableMemoryCallbacksClass))

G_BEGIN_DECLS

typedef struct _ETableMemoryCallbacks ETableMemoryCallbacks;
typedef struct _ETableMemoryCallbacksClass ETableMemoryCallbacksClass;

typedef gint		(*ETableMemoryCallbacksColumnCountFn)
							(ETableModel *etm,
							 gpointer data);
typedef void		(*ETableMemoryCallbacksAppendRowFn)
							(ETableModel *etm,
							 ETableModel *model,
							 gint row,
							 gpointer data);

typedef gpointer	(*ETableMemoryCallbacksValueAtFn)
							(ETableModel *etm,
							 gint col,
							 gint row,
							 gpointer data);
typedef void		(*ETableMemoryCallbacksSetValueAtFn)
							(ETableModel *etm,
							 gint col,
							 gint row,
							 gconstpointer val,
							 gpointer data);
typedef gboolean	(*ETableMemoryCallbacksIsCellEditableFn)
							(ETableModel *etm,
							 gint col,
							 gint row,
							 gpointer data);

typedef gpointer	(*ETableMemoryCallbacksDuplicateValueFn)
							(ETableModel *etm,
							 gint col,
							 gconstpointer val,
							 gpointer data);
typedef void		(*ETableMemoryCallbacksFreeValueFn)
							(ETableModel *etm,
							 gint col,
							 gpointer val,
							 gpointer data);
typedef gpointer	(*ETableMemoryCallbacksInitializeValueFn)
							(ETableModel *etm,
							 gint col,
							 gpointer data);
typedef gboolean	(*ETableMemoryCallbacksValueIsEmptyFn)
							(ETableModel *etm,
							 gint col,
							 gconstpointer val,
							 gpointer data);
typedef gchar *		(*ETableMemoryCallbacksValueToStringFn)
							(ETableModel *etm,
							 gint col,
							 gconstpointer val,
							 gpointer data);

struct _ETableMemoryCallbacks {
	ETableMemory parent;

	ETableMemoryCallbacksColumnCountFn     col_count;
	ETableMemoryCallbacksAppendRowFn       append_row;

	ETableMemoryCallbacksValueAtFn         value_at;
	ETableMemoryCallbacksSetValueAtFn      set_value_at;
	ETableMemoryCallbacksIsCellEditableFn  is_cell_editable;

	ETableMemoryCallbacksDuplicateValueFn  duplicate_value;
	ETableMemoryCallbacksFreeValueFn       free_value;
	ETableMemoryCallbacksInitializeValueFn initialize_value;
	ETableMemoryCallbacksValueIsEmptyFn    value_is_empty;
	ETableMemoryCallbacksValueToStringFn   value_to_string;
	gpointer data;
};

struct _ETableMemoryCallbacksClass {
	ETableMemoryClass parent_class;
};

GType		e_table_memory_callbacks_get_type
			(void) G_GNUC_CONST;
ETableModel *	e_table_memory_callbacks_new
			(ETableMemoryCallbacksColumnCountFn col_count,

			 ETableMemoryCallbacksValueAtFn value_at,
			 ETableMemoryCallbacksSetValueAtFn set_value_at,
			 ETableMemoryCallbacksIsCellEditableFn is_cell_editable,

			 ETableMemoryCallbacksDuplicateValueFn duplicate_value,
			 ETableMemoryCallbacksFreeValueFn free_value,
			 ETableMemoryCallbacksInitializeValueFn initialize_value,
			 ETableMemoryCallbacksValueIsEmptyFn value_is_empty,
			 ETableMemoryCallbacksValueToStringFn value_to_string,
			 gpointer data);

G_END_DECLS

#endif /* _E_TABLE_MEMORY_CALLBACKS_H_ */

