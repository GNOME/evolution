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

#ifndef _E_TABLE_WITHOUT_H_
#define _E_TABLE_WITHOUT_H_

#include <glib-object.h>
#include <table/e-table-subset.h>

G_BEGIN_DECLS

#define E_TABLE_WITHOUT_TYPE        (e_table_without_get_type ())
#define E_TABLE_WITHOUT(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_WITHOUT_TYPE, ETableWithout))
#define E_TABLE_WITHOUT_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_WITHOUT_TYPE, ETableWithoutClass))
#define E_IS_TABLE_WITHOUT(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_WITHOUT_TYPE))
#define E_IS_TABLE_WITHOUT_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_WITHOUT_TYPE))

typedef struct _ETableWithoutPrivate ETableWithoutPrivate;
typedef gpointer (*ETableWithoutGetKeyFunc)       (ETableModel *source,
						gint          row,
						void        *closure);
typedef gpointer (*ETableWithoutDuplicateKeyFunc) (const void  *key,
						void        *closure);
typedef void  (*ETableWithoutFreeKeyFunc)      (void        *key,
						void        *closure);

typedef struct {
	ETableSubset base;

	ETableWithoutPrivate *priv;
} ETableWithout;

typedef struct {
	ETableSubsetClass parent_class;

} ETableWithoutClass;

GType        e_table_without_get_type   (void);
ETableModel *e_table_without_new        (ETableModel                   *source,
					 GHashFunc                      hash_func,
					 GCompareFunc                   compare_func,
					 ETableWithoutGetKeyFunc        get_key_func,
					 ETableWithoutDuplicateKeyFunc  duplicate_key_func,
					 ETableWithoutFreeKeyFunc       free_gotten_key_func,
					 ETableWithoutFreeKeyFunc       free_duplicated_key_func,
					 void                          *closure);
ETableModel *e_table_without_construct  (ETableWithout                 *etw,
					 ETableModel                   *source,
					 GHashFunc                      hash_func,
					 GCompareFunc                   compare_func,
					 ETableWithoutGetKeyFunc        get_key_func,
					 ETableWithoutDuplicateKeyFunc  duplicate_key_func,
					 ETableWithoutFreeKeyFunc       free_gotten_key_func,
					 ETableWithoutFreeKeyFunc       free_duplicated_key_func,
					 void                          *closure);
void         e_table_without_hide       (ETableWithout                 *etw,
					 void                          *key);
void         e_table_without_hide_adopt (ETableWithout                 *etw,
					 void                          *key);
void         e_table_without_show       (ETableWithout                 *etw,
					 void                          *key);
void         e_table_without_show_all   (ETableWithout                 *etw);
G_END_DECLS

#endif /* _E_TABLE_WITHOUT_H_ */

