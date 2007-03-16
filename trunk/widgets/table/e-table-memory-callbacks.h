/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-memory-callbacks.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
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

typedef int         (*ETableMemoryCalbacksColumnCountFn)     (ETableModel *etm, void *data);
typedef void        (*ETableMemoryCalbacksAppendRowFn)       (ETableModel *etm, ETableModel *model, int row, void *data);

typedef	void       *(*ETableMemoryCalbacksValueAtFn)         (ETableModel *etm, int col, int row, void *data);
typedef	void        (*ETableMemoryCalbacksSetValueAtFn)      (ETableModel *etm, int col, int row, const void *val, void *data);
typedef	gboolean    (*ETableMemoryCalbacksIsCellEditableFn)  (ETableModel *etm, int col, int row, void *data);

typedef	void       *(*ETableMemoryCalbacksDuplicateValueFn)  (ETableModel *etm, int col, const void *val, void *data);
typedef	void        (*ETableMemoryCalbacksFreeValueFn)       (ETableModel *etm, int col, void *val, void *data);
typedef void       *(*ETableMemoryCalbacksInitializeValueFn) (ETableModel *etm, int col, void *data);
typedef gboolean    (*ETableMemoryCalbacksValueIsEmptyFn)    (ETableModel *etm, int col, const void *val, void *data);
typedef char       *(*ETableMemoryCalbacksValueToStringFn)   (ETableModel *etm, int col, const void *val, void *data);

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
	void *data;
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
				 void *data);

G_END_DECLS

#endif /* _E_TABLE_MEMORY_CALLBACKS_H_ */

