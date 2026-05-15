/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Chris Lahey <clahey@ximian.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_TABLE_SUBSET_VARIABLE_H_
#define _E_TABLE_SUBSET_VARIABLE_H_

#include <e-util/e-table-subset.h>

/* Standard GObject macros */
#define E_TYPE_TABLE_SUBSET_VARIABLE \
	(e_table_subset_variable_get_type ())
#define E_TABLE_SUBSET_VARIABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TABLE_SUBSET_VARIABLE, ETableSubsetVariable))
#define E_TABLE_SUBSET_VARIABLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TABLE_SUBSET_VARIABLE, ETableSubsetVariableClass))
#define E_IS_TABLE_SUBSET_VARIABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TABLE_SUBSET_VARIABLE))
#define E_IS_TABLE_SUBSET_VARIABLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TABLE_SUBSET_VARIABLE))
#define E_TABLE_SUBSET_VARIABLE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TABLE_SUBSET_VARIABLE, ETableSubsetVariableClass))

G_BEGIN_DECLS

typedef struct _ETableSubsetVariable ETableSubsetVariable;
typedef struct _ETableSubsetVariableClass ETableSubsetVariableClass;

struct _ETableSubsetVariable {
	ETableSubset parent;
	gint n_vals_allocated;
};

struct _ETableSubsetVariableClass {
	ETableSubsetClass parent_class;

	void		(*add)			(ETableSubsetVariable *ets,
						 gint row);
	void		(*add_array)		(ETableSubsetVariable *ets,
						 const gint *array,
						 gint count);
	void		(*add_all)		(ETableSubsetVariable *ets);
	gboolean	(*remove)		(ETableSubsetVariable *ets,
						 gint row);
};

GType		e_table_subset_variable_get_type
						(void) G_GNUC_CONST;
ETableModel *	e_table_subset_variable_new	(ETableModel *etm);
ETableModel *	e_table_subset_variable_construct
						(ETableSubsetVariable *etssv,
						 ETableModel *source);
void		e_table_subset_variable_add	(ETableSubsetVariable *ets,
						 gint row);
void		e_table_subset_variable_add_array
						(ETableSubsetVariable *ets,
						 const gint *array,
						 gint count);
void		e_table_subset_variable_add_all	(ETableSubsetVariable *ets);
gboolean	e_table_subset_variable_remove	(ETableSubsetVariable *ets,
						 gint row);
void		e_table_subset_variable_clear	(ETableSubsetVariable *ets);
void		e_table_subset_variable_increment
						(ETableSubsetVariable *ets,
						 gint position,
						 gint amount);
void		e_table_subset_variable_decrement
						(ETableSubsetVariable *ets,
						 gint position,
						 gint amount);
void		e_table_subset_variable_set_allocation
						(ETableSubsetVariable *ets,
						 gint total);

G_END_DECLS

#endif /* _E_TABLE_SUBSET_VARIABLE_H_ */

