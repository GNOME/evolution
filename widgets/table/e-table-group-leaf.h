/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-group-leaf.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
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

#ifndef _E_TABLE_GROUP_LEAF_H_
#define _E_TABLE_GROUP_LEAF_H_

#include <libgnomecanvas/gnome-canvas.h>
#include <table/e-table-group.h>
#include <table/e-table-subset.h>
#include <table/e-table-item.h>

G_BEGIN_DECLS

#define E_TABLE_GROUP_LEAF_TYPE        (e_table_group_leaf_get_type ())
#define E_TABLE_GROUP_LEAF(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_GROUP_LEAF_TYPE, ETableGroupLeaf))
#define E_TABLE_GROUP_LEAF_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_GROUP_LEAF_TYPE, ETableGroupLeafClass))
#define E_IS_TABLE_GROUP_LEAF(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_GROUP_LEAF_TYPE))
#define E_IS_TABLE_GROUP_LEAF_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_GROUP_LEAF_TYPE))

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

	guint alternating_row_colors : 1;
	guint horizontal_draw_grid : 1;
	guint vertical_draw_grid : 1;
	guint draw_focus : 1;
	guint uniform_row_height : 1;
	ECursorMode cursor_mode;

	int etgl_cursor_change_id;
	int etgl_cursor_activated_id;
	int etgl_double_click_id;
	int etgl_right_click_id;
	int etgl_click_id;
	int etgl_key_press_id;
	int etgl_start_drag_id;

	ESelectionModel *selection_model;
} ETableGroupLeaf;

typedef struct {
	ETableGroupClass parent_class;
} ETableGroupLeafClass;

ETableGroup *e_table_group_leaf_new       (GnomeCanvasGroup *parent,
					   ETableHeader *full_header,
					   ETableHeader     *header,
					   ETableModel *model,
					   ETableSortInfo *sort_info);
GType        e_table_group_leaf_get_type  (void);


G_END_DECLS

#endif /* _E_TABLE_GROUP_LEAF_H_ */

