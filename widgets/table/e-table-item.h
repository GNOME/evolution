#ifndef _E_TABLE_ITEM_H_
#define _E_TABLE_ITEM_H_

#include <libgnomeui/gnome-canvas.h>
#include "e-table-model.h"
#include "e-table-header.h"

#define E_TABLE_ITEM_TYPE        (e_table_item_get_type ())
#define E_TABLE_ITEM(o)          (GTK_CHECK_CAST ((o), E_TABLE_ITEM_TYPE, ETableItem))
#define E_TABLE_ITEM_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_ITEM_TYPE, ETableItemClass))
#define E_IS_TABLE_ITEM(o)       (GTK_CHECK_TYPE ((o), E_TABLE_ITEM_TYPE))
#define E_IS_TABLE_ITEM_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_ITEM_TYPE))

typedef struct {
	GnomeCanvasItem  parent;
	ETableModel     *table_model;
	ETableHeader    *header;

	int              x1, y1;
	int              height;

	int              top_item;
	
	/*
	 * Ids for the signals we connect to
	 */
	int              header_dim_change_id;
	int              header_structure_change_id;
	int              table_model_change_id;

	GdkGC           *fill_gc;
	GdkGC           *grid_gc;

	unsigned int     draw_grid:1;
} ETableItem;

typedef struct {
	GnomeCanvasItemClass parent_class;
} ETableItemClass;

GtkType    e_table_item_get_type (void);

#endif /* _E_TABLE_ITEM_H_ */
