/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_SUBSET_VARIABLE_H_
#define _E_TABLE_SUBSET_VARIABLE_H_

#include <gtk/gtkobject.h>
#include "e-table-subset.h"

#define E_TABLE_SUBSET_VARIABLE_TYPE        (e_table_subset_variable_get_type ())
#define E_TABLE_SUBSET_VARIABLE(o)          (GTK_CHECK_CAST ((o), E_TABLE_SUBSET_VARIABLE_TYPE, ETableSubsetVariable))
#define E_TABLE_SUBSET_VARIABLE_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_SUBSET_VARIABLE_TYPE, ETableSubsetVariableClass))
#define E_IS_TABLE_SUBSET_VARIABLE(o)       (GTK_CHECK_TYPE ((o), E_TABLE_SUBSET_VARIABLE_TYPE))
#define E_IS_TABLE_SUBSET_VARIABLE_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_SUBSET_VARIABLE_TYPE))

typedef struct {
	ETableSubset base;

	int n_vals_allocated;
} ETableSubsetVariable;

typedef struct {
	ETableSubsetClass parent_class;
	
	void     (*add)     (ETableSubsetVariable *ets,
			     gint                  row);
	void     (*add_all) (ETableSubsetVariable *ets);
	gboolean (*remove)  (ETableSubsetVariable *ets,
			     gint                  row);
} ETableSubsetVariableClass;

GtkType      e_table_subset_variable_get_type  (void);
ETableModel *e_table_subset_variable_new       (ETableModel          *etm);
ETableModel *e_table_subset_variable_construct (ETableSubsetVariable *etssv,
						ETableModel          *source);
void         e_table_subset_variable_add       (ETableSubsetVariable *ets,
						gint                  row);
void         e_table_subset_variable_add_all   (ETableSubsetVariable *ets);
gboolean     e_table_subset_variable_remove    (ETableSubsetVariable *ets,
						gint                  row);
void         e_table_subset_variable_increment (ETableSubsetVariable *ets,
						gint                  position,
						gint                  amount);
void         e_table_subset_variable_set_allocation (ETableSubsetVariable *ets,
						     gint                  total);
#endif /* _E_TABLE_SUBSET_VARIABLE_H_ */

