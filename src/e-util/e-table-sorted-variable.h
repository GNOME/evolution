/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Chris Lahey <clahey@ximian.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_TABLE_SORTED_VARIABLE_H_
#define _E_TABLE_SORTED_VARIABLE_H_

#include <e-util/e-table-header.h>
#include <e-util/e-table-model.h>
#include <e-util/e-table-sort-info.h>
#include <e-util/e-table-subset-variable.h>

/* Standard GObject macros */
#define E_TYPE_TABLE_SORTED_VARIABLE \
	(e_table_sorted_variable_get_type ())
#define E_TABLE_SORTED_VARIABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TABLE_SORTED_VARIABLE, ETableSortedVariable))
#define E_TABLE_SORTED_VARIABLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TABLE_SORTED_VARIABLE, ETableSortedVariableClass))
#define E_IS_TABLE_SORTED_VARIABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TABLE_SORTED_VARIABLE))
#define E_IS_TABLE_SORTED_VARIABLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TABLE_SORTED_VARIABLE))
#define E_TABLE_SORTED_VARIABLE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TABLE_SORTED_VARIABLE, ETableSortedVariableClass))

G_BEGIN_DECLS

typedef struct _ETableSortedVariable ETableSortedVariable;
typedef struct _ETableSortedVariableClass ETableSortedVariableClass;

struct _ETableSortedVariable {
	ETableSubsetVariable parent;

	ETableSortInfo *sort_info;

	ETableHeader *full_header;

	gint sort_info_changed_id;
	gint sort_idle_id;
	gint insert_idle_id;
	gint insert_count;
};

struct _ETableSortedVariableClass {
	ETableSubsetVariableClass parent_class;
};

GType		e_table_sorted_variable_get_type
						(void) G_GNUC_CONST;
ETableModel *	e_table_sorted_variable_new	(ETableModel *etm,
						 ETableHeader *header,
						 ETableSortInfo *sort_info);

G_END_DECLS

#endif /* _E_TABLE_SORTED_VARIABLE_H_ */
