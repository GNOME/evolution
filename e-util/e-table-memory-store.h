/*
 *
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

#ifndef _E_TABLE_MEMORY_STORE_H_
#define _E_TABLE_MEMORY_STORE_H_

#include <e-util/e-table-memory.h>
#include <e-util/e-table-memory-callbacks.h>

/* Standard GObject macros */
#define E_TYPE_TABLE_MEMORY_STORE \
	(e_table_memory_store_get_type ())
#define E_TABLE_MEMORY_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TABLE_MEMORY_STORE, ETableMemoryStore))
#define E_TABLE_MEMORY_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TABLE_MEMORY_STORE, ETableMemoryStoreClass))
#define E_IS_TABLE_MEMORY_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TABLE_MEMORY_STORE))
#define E_IS_TABLE_MEMORY_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TABLE_MEMORY_STORE))
#define E_TABLE_MEMORY_STORE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TABLE_MEMORY_STORE, ETableMemoryStoreClass))

G_BEGIN_DECLS

typedef enum {
	E_TABLE_MEMORY_STORE_COLUMN_TYPE_TERMINATOR,
	E_TABLE_MEMORY_STORE_COLUMN_TYPE_INTEGER,
	E_TABLE_MEMORY_STORE_COLUMN_TYPE_STRING,
	E_TABLE_MEMORY_STORE_COLUMN_TYPE_PIXBUF,
	E_TABLE_MEMORY_STORE_COLUMN_TYPE_OBJECT,
} ETableMemoryStoreColumnType;

typedef struct {
	ETableMemoryCallbacksDuplicateValueFn duplicate_value;
	ETableMemoryCallbacksFreeValueFn free_value;
	ETableMemoryCallbacksInitializeValueFn initialize_value;
	ETableMemoryCallbacksValueIsEmptyFn value_is_empty;
	ETableMemoryCallbacksValueToStringFn value_to_string;
} ETableMemoryStoreCustomColumn;

typedef struct {
	ETableMemoryStoreColumnType type;
	ETableMemoryStoreCustomColumn custom;
	guint editable : 1;
} ETableMemoryStoreColumnInfo;

#define E_TABLE_MEMORY_STORE_TERMINATOR \
	{ E_TABLE_MEMORY_STORE_COLUMN_TYPE_TERMINATOR, { NULL }, FALSE }
#define E_TABLE_MEMORY_STORE_INTEGER \
	{ E_TABLE_MEMORY_STORE_COLUMN_TYPE_INTEGER, { NULL }, FALSE }
#define E_TABLE_MEMORY_STORE_STRING \
	{ E_TABLE_MEMORY_STORE_COLUMN_TYPE_STRING, { NULL }, FALSE }

typedef struct _ETableMemoryStore ETableMemoryStore;
typedef struct _ETableMemoryStoreClass ETableMemoryStoreClass;
typedef struct _ETableMemoryStorePrivate ETableMemoryStorePrivate;

struct _ETableMemoryStore {
	ETableMemory parent;
	ETableMemoryStorePrivate *priv;
};

struct _ETableMemoryStoreClass {
	ETableMemoryClass parent_class;
};

GType		e_table_memory_store_get_type	(void) G_GNUC_CONST;
ETableModel *	e_table_memory_store_new	(ETableMemoryStoreColumnInfo *columns);
ETableModel *	e_table_memory_store_construct	(ETableMemoryStore *store,
						 ETableMemoryStoreColumnInfo *columns);

/* Adopt a value instead of copying it. */
void		e_table_memory_store_adopt_value_at
						(ETableMemoryStore *etms,
						 gint col,
						 gint row,
						 gpointer value);

/* The size of these arrays is the number of columns. */
void		e_table_memory_store_insert_array
						(ETableMemoryStore *etms,
						 gint row,
						 gpointer *store,
						 gpointer data);
void		e_table_memory_store_insert	(ETableMemoryStore *etms,
						 gint row,
						 gpointer data,
						 ...);
void		e_table_memory_store_insert_adopt
						(ETableMemoryStore *etms,
						 gint row,
						 gpointer data,
						 ...);
void		e_table_memory_store_insert_adopt_array
						(ETableMemoryStore *etms,
						 gint row,
						 gpointer *store,
						 gpointer data);
void		e_table_memory_store_change_array
						(ETableMemoryStore *etms,
						 gint row,
						 gpointer *store,
						 gpointer data);
void		e_table_memory_store_change	(ETableMemoryStore *etms,
						 gint row,
						 gpointer data,
						 ...);
void		e_table_memory_store_change_adopt
						(ETableMemoryStore *etms,
						 gint row,
						 gpointer data,
						 ...);
void		e_table_memory_store_change_adopt_array
						(ETableMemoryStore *etms,
						 gint row,
						 gpointer *store,
						 gpointer data);
void		e_table_memory_store_remove	(ETableMemoryStore *etms,
						 gint row);
void		e_table_memory_store_clear	(ETableMemoryStore *etms);

G_END_DECLS

#endif /* _E_TABLE_MEMORY_STORE_H_ */
