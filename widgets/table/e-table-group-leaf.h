/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_GROUP_LEAF_H_
#define _E_TABLE_GROUP_LEAF_H_

#include <libgnomeui/gnome-canvas.h>
#include <gal/e-table/e-table-group.h>
#include <gal/e-table/e-table-subset.h>
#include <gal/e-table/e-table-item.h>

BEGIN_GNOME_DECLS

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

	gdouble height;
	gdouble width;
	gdouble minimum_width;

	int length_threshold;

	ETableSubset *ets;
	guint is_grouped : 1;

	guint draw_grid : 1;
	guint draw_focus : 1;
	ETableCursorMode cursor_mode;

	ETableSelectionModel *table_selection_model;
} ETableGroupLeaf;

typedef struct {
	ETableGroupClass parent_class;
} ETableGroupLeafClass;

ETableGroup *e_table_group_leaf_new       (GnomeCanvasGroup *parent,
					   ETableHeader *full_header,
					   ETableHeader     *header,
					   ETableModel *model,
					   ETableSortInfo *sort_info);
GtkType      e_table_group_leaf_get_type  (void);


END_GNOME_DECLS

#endif /* _E_TABLE_GROUP_LEAF_H_ */

