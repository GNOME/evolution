#ifndef _E_TABLE_TREE_H_
#define _E_TABLE_TREE_H_

G_BEGIN_DECLS

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

G_END_DECLS

#endif /* _E_TABLE_TREE_H_ */
