#ifndef _E_TABLE_HEADER_ITEM_H_
#define _E_TABLE_HEADER_ITEM_H_

#include <libgnomeui/gnome-canvas.h>
#include "e-table-header.h"

#define E_TABLE_HEADER_ITEM_TYPE        (e_table_header_item_get_type ())
#define E_TABLE_HEADER_ITEM(o)          (GTK_CHECK_CAST ((o), E_TABLE_HEADER_ITEM_TYPE, ETableHeaderItem))
#define E_TABLE_HEADER_ITEM_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_HEADER_ITEM_TYPE, ETableHeaderItemClass))
#define E_IS_TABLE_HEADER_ITEM(o)       (GTK_CHECK_TYPE ((o), E_TABLE_HEADER_ITEM_TYPE))
#define E_IS_TABLE_HEADER_ITEM_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_HEADER_ITEM_TYPE))

typedef struct {
	GnomeCanvasItem  parent;
	ETableHeader    *eth;

	GdkGC           *gc;
	GdkCursor       *change_cursor, *normal_cursor;
} ETableHeaderItem;

typedef struct {
	GnomeCanvasItemClass parent_class;
} ETableHeaderItemClass;

GtkType    e_table_header_item_get_type (void);

#endif /* _E_TABLE_HEADER_ITEM_H_ */
