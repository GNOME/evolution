/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-header-item.h
 * Copyright 1999, 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef _E_TABLE_HEADER_ITEM_H_
#define _E_TABLE_HEADER_ITEM_H_

#include <gal/e-table/e-table.h>
#include <gal/e-table/e-tree.h>
#include <libgnomecanvas/gnome-canvas.h>
#include <libxml/tree.h>
#include <gal/e-table/e-table-header.h>
#include <gal/e-table/e-table-sort-info.h>

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
	GdkFont         *font;

	/*
	 * Used during resizing;  Could be shorts
	 */
	int              resize_col;
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
	int              drag_col, drop_col, drag_mark;
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
	int last_drop_x;
	int last_drop_y;
	int last_drop_time;
	GdkDragContext *last_drop_context;
	int scroll_idle_id;

	/* For adding fields. */
	ETableHeader    *full_header;
	ETable          *table;
	ETree           *tree;
	GtkWidget       *etfcd;
	void            *config;

	/* For keyboard navigation*/
	int selected_col;

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
