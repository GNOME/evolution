#ifndef _E_TABLE_SUBSET_H_
#define _E_TABLE_SUBSET_H_

#include <gtk/gtkobject.h>
#include "e-table-model.h"
#include "e-table-header.h"

#define E_TABLE_SUBSET_TYPE        (e_table_subset_get_type ())
#define E_TABLE_SUBSET(o)          (GTK_CHECK_CAST ((o), E_TABLE_SUBSET_TYPE, ETableSubset))
#define E_TABLE_SUBSET_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_SUBSET_TYPE, ETableSubsetClass))
#define E_IS_TABLE_SUBSET(o)       (GTK_CHECK_TYPE ((o), E_TABLE_SUBSET_TYPE))
#define E_IS_TABLE_SUBSET_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_SUBSET_TYPE))

typedef struct {
	ETableModel base;

	ETableModel  *source;
	int  subset_count;
	int *subset_table;
} ETableSubset;

typedef struct {
	ETableModelClass parent_class;
} ETableSubsetClass;

GtkType      e_table_subset_get_type (void);
ETableModel *e_table_subset_new      (ETableModel *etm,
				      int n_vals, int *ptrs);
#endif /* _E_TABLE_SUBSET_H_ */
