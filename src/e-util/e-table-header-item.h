/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *		Miguel de Icaza (miguel@gnu.org)
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_TABLE_HEADER_ITEM_H_
#define _E_TABLE_HEADER_ITEM_H_

#include <libxml/tree.h>
#include <libgnomecanvas/libgnomecanvas.h>

#include <e-util/e-table-header.h>
#include <e-util/e-table-sort-info.h>
#include <e-util/e-table.h>
#include <e-util/e-tree.h>

/* Standard GObject macros */
#define E_TYPE_TABLE_HEADER_ITEM \
	(e_table_header_item_get_type ())
#define E_TABLE_HEADER_ITEM(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TABLE_HEADER_ITEM, ETableHeaderItem))
#define E_TABLE_HEADER_ITEM_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TABLE_HEADER_ITEM, ETableHeaderItemClass))
#define E_IS_TABLE_HEADER_ITEM(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TABLE_HEADER_ITEM))
#define E_IS_TABLE_HEADER_ITEM_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TABLE_HEADER_ITEM))
#define E_TABLE_HEADER_ITEM_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TABLE_HEADER_ITEM, ETableHeaderItemClass))

G_BEGIN_DECLS

typedef enum {
	E_TABLE_HEADER_ITEM_SORT_FLAG_NONE = 0,
	E_TABLE_HEADER_ITEM_SORT_FLAG_ADD_AS_FIRST = (1 << 0),
	E_TABLE_HEADER_ITEM_SORT_FLAG_ADD_AS_LAST = (1 << 1)
} ETableHeaderItemSortFlag;

typedef struct _ETableHeaderItem ETableHeaderItem;
typedef struct _ETableHeaderItemClass ETableHeaderItemClass;

struct _ETableHeaderItem {
	GnomeCanvasItem parent;
	ETableHeader *eth;

	GdkCursor *change_cursor;
	GdkCursor *resize_cursor;

	gshort height, width;
	PangoFontDescription *font_desc;

	/*
	 * Used during resizing; Could be shorts
	 */
	gint resize_col;
	gint resize_start_pos;
	gint resize_min_width;

	gpointer resize_guide;

	gint group_indent_width;

	/*
	 * Ids
	 */
	gint structure_change_id, dimension_change_id;

	/*
	 * For dragging columns
	 */
	guint maybe_drag : 1;
	guint dnd_ready : 1;
	gint click_x, click_y;
	gint drag_col, drop_col, drag_mark;
	guint drag_motion_id;
	guint drag_end_id;
	guint drag_leave_id;
	guint drag_drop_id;
	guint drag_data_received_id;
	guint drag_data_get_id;
	guint sort_info_changed_id, group_info_changed_id;
	GnomeCanvasItem *remove_item;

	gchar *dnd_code;

	/*
	 * For column sorting info
	 */
	ETableSortInfo *sort_info;

	guint scroll_direction : 4;
	gint last_drop_x;
	gint last_drop_y;
	gint last_drop_time;
	GdkDragContext *last_drop_context;
	gint scroll_idle_id;

	/* For adding fields. */
	ETableHeader *full_header;
	ETable *table;
	ETree *tree;
	gpointer config;

	union {
		GtkWidget *widget;
		gpointer pointer;
	} etfcd;

	/* For keyboard navigation*/
	gint selected_col;
};

struct _ETableHeaderItemClass {
	GnomeCanvasItemClass parent_class;

	/* Signals */
	void		(*button_pressed)	(ETableHeaderItem *ethi,
						 GdkEvent *button_event);
};

GType		e_table_header_item_get_type	(void) G_GNUC_CONST;
void		ethi_change_sort_state		(ETableHeaderItem *ethi,
						 ETableCol *col,
						 ETableHeaderItemSortFlag flag);
void		e_table_header_item_customize_view
						(ETableHeaderItem *ethi);

G_END_DECLS

#endif /* _E_TABLE_HEADER_ITEM_H_ */
