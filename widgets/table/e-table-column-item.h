#ifndef _E_TABLE_COLUMN_VIEW_H
#defein _E_TABLE_COLUMN_VIEW_H

#include "e-table-column.h"

typedef struct {
	GnomeCanvasItem  parent;
	ETableColumn    *etc;

	GdkGC           *gc;
	GdkCursor       *change_cursor, *normal_cursor;
} ETableColumnView;

typedef struct {
	GnomeCanvasItemClass parent_class;
} ETableColumnViewClass;

GtkType    e_table_column_item_get_type (void);

#endif /* _E_TABLE_COLUMN_VIEW_H */
