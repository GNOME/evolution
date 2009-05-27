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

#ifndef _E_TABLE_MEMORY_CALLBACKS_H_
#define _E_TABLE_MEMORY_CALLBACKS_H_

#include <table/e-table-memory.h>

G_BEGIN_DECLS

#define E_TABLE_MEMORY_CALLBACKS_TYPE        (e_table_memory_callbacks_get_type ())
#define E_TABLE_MEMORY_CALLBACKS(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_MEMORY_CALLBACKS_TYPE, ETableMemoryCalbacks))
#define E_TABLE_MEMORY_CALLBACKS_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_MEMORY_CALLBACKS_TYPE, ETableMemoryCalbacksClass))
#define E_IS_TABLE_MEMORY_CALLBACKS(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_MEMORY_CALLBACKS_TYPE))
#define E_IS_TABLE_MEMORY_CALLBACKS_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_MEMORY_CALLBACKS_TYPE))
#define E_TABLE_MEMORY_CALLBACKS_GET_CLASS(k) (G_TYPE_INSTANCE_GET_CLASS((k), E_TABLE_MEMORY_CALLBACKS_TYPE, ETableMemoryCalbacksClass))

typedef gint         (*ETableMemoryCalbacksColumnCountFn)     (ETableModel *etm, gpointer data);
typedef void        (*ETableMemoryCalbacksAppendRowFn)       (ETableModel *etm, ETableModel *model, gint row, gpointer data);

typedef	void       *(*ETableMemoryCalbacksValueAtFn)         (ETableModel *etm, gint col, gint row, gpointer data);
typedef	void        (*ETableMemoryCalbacksSetValueAtFn)      (ETableModel *etm, gint col, gint row, gconstpointer val, gpointer data);
typedef	gboolean    (*ETableMemoryCalbacksIsCellEditableFn)  (ETableModel *etm, gint col, gint row, gpointer data);

typedef	void       *(*ETableMemoryCalbacksDuplicateValueFn)  (ETableModel *etm, gint col, gconstpointer val, gpointer data);
typedef	void        (*ETableMemoryCalbacksFreeValueFn)       (ETableModel *etm, gint col, gpointer val, gpointer data);
typedef void       *(*ETableMemoryCalbacksInitializeValueFn) (ETableModel *etm, gint col, gpointer data);
typedef gboolean    (*ETableMemoryCalbacksValueIsEmptyFn)    (ETableModel *etm, gint col, gconstpointer val, gpointer data);
typedef gchar       *(*ETableMemoryCalbacksValueToStringFn)   (ETableModel *etm, gint col, gconstpointer val, gpointer data);

typedef struct {
	ETableMemory parent;

	ETableMemoryCalbacksColumnCountFn     col_count;
	ETableMemoryCalbacksAppendRowFn       append_row;

	ETableMemoryCalbacksValueAtFn         value_at;
	ETableMemoryCalbacksSetValueAtFn      set_value_at;
	ETableMemoryCalbacksIsCellEditableFn  is_cell_editable;

	ETableMemoryCalbacksDuplicateValueFn  duplicate_value;
	ETableMemoryCalbacksFreeValueFn       free_value;
	ETableMemoryCalbacksInitializeValueFn initialize_value;
	ETableMemoryCalbacksValueIsEmptyFn    value_is_empty;
	ETableMemoryCalbacksValueToStringFn   value_to_string;
	gpointer data;
} ETableMemoryCalbacks;

typedef struct {
	ETableMemoryClass parent_class;
} ETableMemoryCalbacksClass;

GType e_table_memory_callbacks_get_type (void);

ETableModel *e_table_memory_callbacks_new (ETableMemoryCalbacksColumnCountFn col_count,

				 ETableMemoryCalbacksValueAtFn value_at,
				 ETableMemoryCalbacksSetValueAtFn set_value_at,
				 ETableMemoryCalbacksIsCellEditableFn is_cell_editable,

				 ETableMemoryCalbacksDuplicateValueFn duplicate_value,
				 ETableMemoryCalbacksFreeValueFn free_value,
				 ETableMemoryCalbacksInitializeValueFn initialize_value,
				 ETableMemoryCalbacksValueIsEmptyFn value_is_empty,
				 ETableMemoryCalbacksValueToStringFn value_to_string,
				 gpointer data);

G_END_DECLS

#endif /* _E_TABLE_MEMORY_CALLBACKS_H_ */

