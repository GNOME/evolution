/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-memory.h
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

#ifndef _E_TABLE_MEMORY_H_
#define _E_TABLE_MEMORY_H_

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <table/e-table-model.h>

G_BEGIN_DECLS

#define E_TABLE_MEMORY_TYPE        (e_table_memory_get_type ())
#define E_TABLE_MEMORY(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_MEMORY_TYPE, ETableMemory))
#define E_TABLE_MEMORY_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_MEMORY_TYPE, ETableMemoryClass))
#define E_IS_TABLE_MEMORY(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_MEMORY_TYPE))
#define E_IS_TABLE_MEMORY_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_MEMORY_TYPE))
#define E_TABLE_MEMORY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), E_TABLE_MEMORY_TYPE, ETableMemoryClass))

typedef struct ETableMemory ETableMemory;
typedef struct ETableMemoryPriv ETableMemoryPriv;
typedef struct ETableMemoryClass ETableMemoryClass;

struct ETableMemory {
	ETableModel base;
	ETableMemoryPriv *priv;
};

struct ETableMemoryClass {
	ETableModelClass parent_class;
};


GType         e_table_memory_get_type     (void);
void          e_table_memory_construct    (ETableMemory *etable);
ETableMemory *e_table_memory_new          (void);

/* row operations */
void          e_table_memory_insert  (ETableMemory *etable,
				      int           row,
				      gpointer      data);
gpointer      e_table_memory_remove  (ETableMemory *etable,
				      int           row);
void          e_table_memory_clear   (ETableMemory *etable);

/* Freeze and thaw */
void          e_table_memory_freeze       (ETableMemory *etable);
void          e_table_memory_thaw         (ETableMemory *etable);
gpointer      e_table_memory_get_data     (ETableMemory *etm,
					   int           row);
void          e_table_memory_set_data     (ETableMemory *etm,
					   int           row,
					   gpointer      data);

G_END_DECLS

#endif /* _E_TABLE_MEMORY_H */
