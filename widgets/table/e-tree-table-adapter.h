/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TREE_TABLE_ADAPTER_H_
#define _E_TREE_TABLE_ADAPTER_H_

#include <gtk/gtkobject.h>
#include <gal/e-table/e-table-model.h>
#include <gal/e-table/e-tree-model.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define E_TREE_TABLE_ADAPTER_TYPE        (e_tree_table_adapter_get_type ())
#define E_TREE_TABLE_ADAPTER(o)          (GTK_CHECK_CAST ((o), E_TREE_TABLE_ADAPTER_TYPE, ETreeTableAdapter))
#define E_TREE_TABLE_ADAPTER_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TREE_TABLE_ADAPTER_TYPE, ETreeTableAdapterClass))
#define E_IS_TREE_TABLE_ADAPTER(o)       (GTK_CHECK_TYPE ((o), E_TREE_TABLE_ADAPTER_TYPE))
#define E_IS_TREE_TABLE_ADAPTER_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TREE_TABLE_ADAPTER_TYPE))

typedef struct ETreeTableAdapterPriv ETreeTableAdapterPriv;

typedef struct {
	ETableModel  base;

	ETreeTableAdapterPriv *priv;
} ETreeTableAdapter;

typedef struct {
	ETableModelClass parent_class;
} ETreeTableAdapterClass;

GtkType      e_tree_table_adapter_get_type                   (void);
ETableModel *e_tree_table_adapter_new                        (ETreeModel        *source);
ETableModel *e_tree_table_adapter_construct                  (ETreeTableAdapter *ets,
							      ETreeModel        *source);

gboolean     e_tree_table_adapter_node_is_expanded           (ETreeTableAdapter *etta,
							      ETreePath          path);
void         e_tree_table_adapter_node_set_expanded          (ETreeTableAdapter *etta,
							      ETreePath          path,
							      gboolean           expanded);
void         e_tree_table_adapter_node_set_expanded_recurse  (ETreeTableAdapter *etta,
							      ETreePath          path,
							      gboolean           expanded);
void         e_tree_table_adapter_root_node_set_visible      (ETreeTableAdapter *etta,
							      gboolean           visible);
ETreePath    e_tree_table_adapter_node_at_row                (ETreeTableAdapter *etta,
							      int                row);
int          e_tree_table_adapter_row_of_node                (ETreeTableAdapter *etta,
							      ETreePath          path);
gboolean     e_tree_table_adapter_root_node_is_visible       (ETreeTableAdapter *etta);

void         e_tree_table_adapter_show_node                  (ETreeTableAdapter *etta,
							      ETreePath          path);

void         e_tree_table_adapter_save_expanded_state        (ETreeTableAdapter *etta,
							      const char        *filename);
void         e_tree_table_adapter_load_expanded_state        (ETreeTableAdapter *etta,
							      const char        *filename);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_TREE_TABLE_ADAPTER_H_ */
