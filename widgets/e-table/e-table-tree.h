#ifndef _E_TABLE_TREE_H_
#define _E_TABLE_TREE_H_

typedef struct {
	char *title;

	union {
		ETableModel *table;
		GList *children;
	} u;

	guint expanded :1;
	guint is_leaf  :1;
} ETableGroup;

ETableGroup *e_table_group_new      (const char *title, ETableModel *table);
ETableGroup *e_table_group_new_leaf (const char *title);

#endif /* _E_TABLE_TREE_H_ */
