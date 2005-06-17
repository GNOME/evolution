/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-memory-store.h
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

#ifndef _E_TABLE_MEMORY_STORE_H_
#define _E_TABLE_MEMORY_STORE_H_

#include <table/e-table-memory.h>
#include <table/e-table-memory-callbacks.h>

G_BEGIN_DECLS

#define E_TABLE_MEMORY_STORE_TYPE        (e_table_memory_store_get_type ())
#define E_TABLE_MEMORY_STORE(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_MEMORY_STORE_TYPE, ETableMemoryStore))
#define E_TABLE_MEMORY_STORE_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_MEMORY_STORE_TYPE, ETableMemoryStoreClass))
#define E_IS_TABLE_MEMORY_STORE(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_MEMORY_STORE_TYPE))
#define E_IS_TABLE_MEMORY_STORE_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_MEMORY_STORE_TYPE))
#define E_TABLE_MEMORY_STORE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((obj), E_TABLE_MEMORY_STORE_TYPE, ETableMemoryStoreClass))

typedef enum {
	E_TABLE_MEMORY_STORE_COLUMN_TYPE_TERMINATOR,
	E_TABLE_MEMORY_STORE_COLUMN_TYPE_INTEGER,
	E_TABLE_MEMORY_STORE_COLUMN_TYPE_STRING,
	E_TABLE_MEMORY_STORE_COLUMN_TYPE_PIXBUF,
	E_TABLE_MEMORY_STORE_COLUMN_TYPE_OBJECT,
	E_TABLE_MEMORY_STORE_COLUMN_TYPE_CUSTOM
} ETableMemoryStoreColumnType;

typedef struct {
	ETableMemoryCalbacksDuplicateValueFn  duplicate_value;
	ETableMemoryCalbacksFreeValueFn       free_value;
	ETableMemoryCalbacksInitializeValueFn initialize_value;
	ETableMemoryCalbacksValueIsEmptyFn    value_is_empty;
	ETableMemoryCalbacksValueToStringFn   value_to_string;
} ETableMemoryStoreCustomColumn;

typedef struct {
	ETableMemoryStoreColumnType   type;
	ETableMemoryStoreCustomColumn custom;
	guint                         editable : 1;
} ETableMemoryStoreColumnInfo;

#define E_TABLE_MEMORY_STORE_TERMINATOR { E_TABLE_MEMORY_STORE_COLUMN_TYPE_TERMINATOR, { NULL }, FALSE }
#define E_TABLE_MEMORY_STORE_INTEGER { E_TABLE_MEMORY_STORE_COLUMN_TYPE_INTEGER, { NULL }, FALSE }
#define E_TABLE_MEMORY_STORE_STRING { E_TABLE_MEMORY_STORE_COLUMN_TYPE_STRING, { NULL }, FALSE }
#define E_TABLE_MEMORY_STORE_PIXBUF { E_TABLE_MEMORY_STORE_COLUMN_TYPE_PIXBUF, { NULL }, FALSE }
#define E_TABLE_MEMORY_STORE_EDITABLE_STRING { E_TABLE_MEMORY_STORE_COLUMN_TYPE_STRING, { NULL }, TRUE }
#define E_TABLE_MEMORY_STORE_CUSTOM(editable, duplicate, free, initialize, empty, string) \
        { E_TABLE_MEMORY_STORE_COLUMN_TYPE_CUSTOM, \
             { (duplicate), (free), (initialize), (empty), (string) }, editable }
#define E_TABLE_MEMORY_STORE_OBJECT(editable, initialize, empty, string) \
        { E_TABLE_MEMORY_STORE_COLUMN_TYPE_CUSTOM, \
             { NULL, NULL, (initialize), (empty), (string) }, editable }

typedef struct _ETableMemoryStorePrivate ETableMemoryStorePrivate;

typedef struct {
	ETableMemory parent;

	ETableMemoryStorePrivate *priv;
} ETableMemoryStore;

typedef struct {
	ETableMemoryClass parent_class;
} ETableMemoryStoreClass;

GType        e_table_memory_store_get_type            (void);

/* Object Creation */
ETableModel *e_table_memory_store_new                 (ETableMemoryStoreColumnInfo  *columns);
ETableModel *e_table_memory_store_construct           (ETableMemoryStore            *store,
						       ETableMemoryStoreColumnInfo  *columns);

/* Adopt a value instead of copying it. */
void         e_table_memory_store_adopt_value_at      (ETableMemoryStore            *etms,
						       int                           col,
						       int                           row,
						       void                         *value);

/* The size of these arrays is the number of columns. */
void         e_table_memory_store_insert_array        (ETableMemoryStore            *etms,
						       int                           row,
						       void                        **store,
						       gpointer                      data);
void         e_table_memory_store_insert              (ETableMemoryStore            *etms,
						       int                           row,
						       gpointer                      data,
						       ...);
void         e_table_memory_store_insert_adopt        (ETableMemoryStore            *etms,
						       int                           row,
						       gpointer                      data,
						       ...);
void         e_table_memory_store_insert_adopt_array  (ETableMemoryStore            *etms,
						       int                           row,
						       void                        **store,
						       gpointer                      data);
void         e_table_memory_store_change_array        (ETableMemoryStore            *etms,
						       int                           row,
						       void                        **store,
						       gpointer                      data);
void         e_table_memory_store_change              (ETableMemoryStore            *etms,
						       int                           row,
						       gpointer                      data,
						       ...);
void         e_table_memory_store_change_adopt        (ETableMemoryStore            *etms,
						       int                           row,
						       gpointer                      data,
						       ...);
void         e_table_memory_store_change_adopt_array  (ETableMemoryStore            *etms,
						       int                           row,
						       void                        **store,
						       gpointer                      data);
void         e_table_memory_store_remove              (ETableMemoryStore            *etms,
						       int                           row);
void         e_table_memory_store_clear               (ETableMemoryStore            *etms);

G_END_DECLS

#endif /* _E_TABLE_MEMORY_STORE_H_ */
