/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_HEADER_ITEM_H_
#define _E_TABLE_HEADER_ITEM_H_

#include <libgnomeui/gnome-canvas.h>
#include <gnome-xml/tree.h>
#include "e-table-header.h"
#include "e-table-sort-info.h"

#define E_TABLE_HEADER_ITEM_TYPE        (e_table_header_item_get_type ())
#define E_TABLE_HEADER_ITEM(o)          (GTK_CHECK_CAST ((o), E_TABLE_HEADER_ITEM_TYPE, ETableHeaderItem))
#define E_TABLE_HEADER_ITEM_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_HEADER_ITEM_TYPE, ETableHeaderItemClass))
#define E_IS_TABLE_HEADER_ITEM(o)       (GTK_CHECK_TYPE ((o), E_TABLE_HEADER_ITEM_TYPE))
#define E_IS_TABLE_HEADER_ITEM_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_HEADER_ITEM_TYPE))

typedef struct {
	GnomeCanvasItem  parent;
	ETableHeader    *eth;

	GdkGC           *gc;
	GdkCursor       *change_cursor;

	short            x1, y1, height, width;
	GdkFont         *font;

	/*
	 * Used during resizing;  Could be shorts
	 */
	int              resize_col;
	int 	         resize_width;
	int              resize_start_pos;
	int              resize_min_width;
	
	GtkObject       *resize_guide;

	int              group_indent_width;

	/*
	 * Ids
	 */
	int structure_change_id, dimension_change_id;

	/*
	 * For dragging columns
	 */
	guint            maybe_drag:1;
	guint            dnd_ready:1;
	int              click_x, click_y;
	int              drag_col, drag_mark;
        guint            drag_motion_id, drag_end_id, drag_leave_id, drag_drop_id;
	guint            sort_info_changed_id;
	GnomeCanvasItem *drag_mark_item, *remove_item;
	GdkBitmap       *stipple;

	/*
	 * For column sorting info
	 */
	ETableSortInfo  *sort_info;
} ETableHeaderItem;

typedef struct {
	GnomeCanvasItemClass parent_class;
} ETableHeaderItemClass;

GtkType    e_table_header_item_get_type (void);

#endif /* _E_TABLE_HEADER_ITEM_H_ */
