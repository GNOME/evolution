#ifndef _E_TABLE_TREE_H_
#define _E_TABLE_TREE_H_

#include "e-table-model.h"

typedef struct {
	char *title;

	union {
		ETableModel *table;
		GSList *children;
	} u;

	guint expanded :1;
	guint is_leaf  :1;
} ETableGroup;

ETableGroup *e_table_group_new          (const char *title);
ETableGroup *e_table_group_new_leaf     (const char *title, ETableModel *table);
void         e_table_group_destroy      (ETableGroup *etg);

int          e_table_group_size         (ETableGroup *egroup);
void         e_table_group_append_child (ETableGroup *etg, ETableGroup *child);

#endif /* _E_TABLE_TREE_H_ */
