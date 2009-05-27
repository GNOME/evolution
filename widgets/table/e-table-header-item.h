/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *		Miguel de Icaza (miguel@gnu.org)
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_TABLE_HEADER_ITEM_H_
#define _E_TABLE_HEADER_ITEM_H_

#include <table/e-table.h>
#include <table/e-tree.h>
#include <libgnomecanvas/gnome-canvas.h>
#include <libxml/tree.h>
#include <table/e-table-header.h>
#include <table/e-table-sort-info.h>

G_BEGIN_DECLS

#define E_TABLE_HEADER_ITEM_TYPE        (e_table_header_item_get_type ())
#define E_TABLE_HEADER_ITEM(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_HEADER_ITEM_TYPE, ETableHeaderItem))
#define E_TABLE_HEADER_ITEM_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_HEADER_ITEM_TYPE, ETableHeaderItemClass))
#define E_IS_TABLE_HEADER_ITEM(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_HEADER_ITEM_TYPE))
#define E_IS_TABLE_HEADER_ITEM_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_HEADER_ITEM_TYPE))

typedef struct {
	GnomeCanvasItem  parent;
	ETableHeader    *eth;

	GdkCursor       *change_cursor;

	short            height, width;
	PangoFontDescription *font_desc;

	/*
	 * Used during resizing;  Could be shorts
	 */
	gint              resize_col;
	gint              resize_start_pos;
	gint              resize_min_width;

	GtkObject       *resize_guide;

	gint              group_indent_width;

	/*
	 * Ids
	 */
	gint structure_change_id, dimension_change_id;

	/*
	 * For dragging columns
	 */
	guint            maybe_drag:1;
	guint            dnd_ready:1;
	gint              click_x, click_y;
	gint              drag_col, drop_col, drag_mark;
        guint            drag_motion_id, drag_end_id, drag_leave_id, drag_drop_id, drag_data_received_id, drag_data_get_id;
	guint            sort_info_changed_id, group_info_changed_id;
	GnomeCanvasItem *remove_item;
	GdkBitmap       *stipple;

	gchar           *dnd_code;

	/*
	 * For column sorting info
	 */
	ETableSortInfo  *sort_info;

	guint scroll_direction : 4;
	gint last_drop_x;
	gint last_drop_y;
	gint last_drop_time;
	GdkDragContext *last_drop_context;
	gint scroll_idle_id;

	/* For adding fields. */
	ETableHeader    *full_header;
	ETable          *table;
	ETree           *tree;
	void            *config;

	union {
		GtkWidget *widget;
		gpointer pointer;
	} etfcd;

	/* For keyboard navigation*/
	gint selected_col;

} ETableHeaderItem;

typedef struct {
	GnomeCanvasItemClass parent_class;

	/*
	 * signals
	 */
	void (*button_pressed) (ETableHeaderItem *ethi, GdkEventButton *button);
} ETableHeaderItemClass;

void
ethi_change_sort_state (ETableHeaderItem *ethi, ETableCol *col);

GType      e_table_header_item_get_type (void);

G_END_DECLS

#endif /* _E_TABLE_HEADER_ITEM_H_ */
