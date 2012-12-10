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

#ifndef _E_TABLE_WITHOUT_H_
#define _E_TABLE_WITHOUT_H_

#include <e-util/e-table-subset.h>

/* Standard GObject macros */
#define E_TYPE_TABLE_WITHOUT \
	(e_table_without_get_type ())
#define E_TABLE_WITHOUT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TABLE_WITHOUT, ETableWithout))
#define E_TABLE_WITHOUT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TABLE_WITHOUT, ETableWithoutClass))
#define E_IS_TABLE_WITHOUT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TABLE_WITHOUT))
#define E_IS_TABLE_WITHOUT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TABLE_WITHOUT))
#define E_TABLE_WITHOUT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TABLE_WITHOUT, ETableWithoutClass))

G_BEGIN_DECLS

typedef struct _ETableWithout ETableWithout;
typedef struct _ETableWithoutClass ETableWithoutClass;
typedef struct _ETableWithoutPrivate ETableWithoutPrivate;

typedef gpointer	(*ETableWithoutGetKeyFunc)	(ETableModel *source,
							 gint row,
							 gpointer closure);
typedef gpointer	(*ETableWithoutDuplicateKeyFunc)(gconstpointer key,
							 gpointer closure);
typedef void		(*ETableWithoutFreeKeyFunc)	(gpointer key,
							 gpointer closure);

struct _ETableWithout {
	ETableSubset parent;
	ETableWithoutPrivate *priv;
};

struct _ETableWithoutClass {
	ETableSubsetClass parent_class;
};

GType		e_table_without_get_type	(void) G_GNUC_CONST;
ETableModel *	e_table_without_new		(ETableModel *source,
						 GHashFunc hash_func,
						 GCompareFunc compare_func,
						 ETableWithoutGetKeyFunc get_key_func,
						 ETableWithoutDuplicateKeyFunc duplicate_key_func,
						 ETableWithoutFreeKeyFunc free_gotten_key_func,
						 ETableWithoutFreeKeyFunc free_duplicated_key_func,
						 gpointer closure);
ETableModel *	e_table_without_construct	(ETableWithout *etw,
						 ETableModel *source,
						 GHashFunc hash_func,
						 GCompareFunc compare_func,
						 ETableWithoutGetKeyFunc get_key_func,
						 ETableWithoutDuplicateKeyFunc duplicate_key_func,
						 ETableWithoutFreeKeyFunc free_gotten_key_func,
						 ETableWithoutFreeKeyFunc free_duplicated_key_func,
						 gpointer closure);
void		e_table_without_hide		(ETableWithout *etw,
						 gpointer key);
void		e_table_without_hide_adopt	(ETableWithout *etw,
						 gpointer key);
void		e_table_without_show		(ETableWithout *etw,
						 gpointer key);
void		e_table_without_show_all	(ETableWithout *etw);

G_END_DECLS

#endif /* _E_TABLE_WITHOUT_H_ */

