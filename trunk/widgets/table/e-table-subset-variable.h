/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-subset-variable.h
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

#ifndef _E_TABLE_SUBSET_VARIABLE_H_
#define _E_TABLE_SUBSET_VARIABLE_H_

#include <glib-object.h>
#include <table/e-table-subset.h>

G_BEGIN_DECLS

#define E_TABLE_SUBSET_VARIABLE_TYPE        (e_table_subset_variable_get_type ())
#define E_TABLE_SUBSET_VARIABLE(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_SUBSET_VARIABLE_TYPE, ETableSubsetVariable))
#define E_TABLE_SUBSET_VARIABLE_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_SUBSET_VARIABLE_TYPE, ETableSubsetVariableClass))
#define E_IS_TABLE_SUBSET_VARIABLE(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_SUBSET_VARIABLE_TYPE))
#define E_IS_TABLE_SUBSET_VARIABLE_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_SUBSET_VARIABLE_TYPE))
#define E_TABLE_SUBSET_VARIABLE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), E_TABLE_SUBSET_VARIABLE_TYPE, ETableSubsetVariableClass))

typedef struct {
	ETableSubset base;

	int n_vals_allocated;
} ETableSubsetVariable;

typedef struct {
	ETableSubsetClass parent_class;
	
	void     (*add)       (ETableSubsetVariable *ets,
			       gint                  row);
	void     (*add_array) (ETableSubsetVariable *ets,
			       const gint           *array,
			       gint                  count);
	void     (*add_all)   (ETableSubsetVariable *ets);
	gboolean (*remove)    (ETableSubsetVariable *ets,
			       gint                  row);
} ETableSubsetVariableClass;

GType        e_table_subset_variable_get_type        (void);
ETableModel *e_table_subset_variable_new             (ETableModel          *etm);
ETableModel *e_table_subset_variable_construct       (ETableSubsetVariable *etssv,
						      ETableModel          *source);
void         e_table_subset_variable_add             (ETableSubsetVariable *ets,
						      gint                  row);
void         e_table_subset_variable_add_array       (ETableSubsetVariable *ets,
						      const gint           *array,
						      gint                  count);
void         e_table_subset_variable_add_all         (ETableSubsetVariable *ets);
gboolean     e_table_subset_variable_remove          (ETableSubsetVariable *ets,
						      gint                  row);
void         e_table_subset_variable_clear           (ETableSubsetVariable *ets);
void         e_table_subset_variable_increment       (ETableSubsetVariable *ets,
						      gint                  position,
						      gint                  amount);
void         e_table_subset_variable_decrement       (ETableSubsetVariable *ets,
						      gint                  position,
						      gint                  amount);
void         e_table_subset_variable_set_allocation  (ETableSubsetVariable *ets,
						      gint                  total);
G_END_DECLS

#endif /* _E_TABLE_SUBSET_VARIABLE_H_ */

