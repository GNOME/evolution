/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_GROUP_LEAF_H_
#define _E_TABLE_GROUP_LEAF_H_

#include <libgnomeui/gnome-canvas.h>
#include "e-table-group.h"
#include "e-table-sorted-variable.h"
#include "e-table-item.h"

#define E_TABLE_GROUP_LEAF_TYPE        (e_table_group_leaf_get_type ())
#define E_TABLE_GROUP_LEAF(o)          (GTK_CHECK_CAST ((o), E_TABLE_GROUP_LEAF_TYPE, ETableGroupLeaf))
#define E_TABLE_GROUP_LEAF_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_GROUP_LEAF_TYPE, ETableGroupLeafClass))
#define E_IS_TABLE_GROUP_LEAF(o)       (GTK_CHECK_TYPE ((o), E_TABLE_GROUP_LEAF_TYPE))
#define E_IS_TABLE_GROUP_LEAF_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_GROUP_LEAF_TYPE))

typedef struct {
	ETableGroup group;

	/* 
	 * Item.
	 */
	ETableItem *item;

	gdouble width;
  
	ETableSubsetVariable *subset;
} ETableGroupLeaf;

typedef struct {
	ETableGroupClass parent_class;
} ETableGroupLeafClass;

ETableGroup *e_table_group_leaf_new       (GnomeCanvasGroup *parent,
					   ETableHeader *full_header,
					   ETableHeader     *header,
					   ETableModel *model,
					   ETableSortInfo *sort_info);
GtkType          e_table_group_leaf_get_type  (void);

#endif /* _E_TABLE_GROUP_LEAF_H_ */
