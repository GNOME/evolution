/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-item.h
 * Copyright 1999, 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *   Miguel de Icaza <miguel@gnu.org>
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

#ifndef _E_TABLE_ITEM_H_
#define _E_TABLE_ITEM_H_

#include <libgnomecanvas/gnome-canvas.h>
#include <table/e-table-model.h>
#include <table/e-table-header.h>
#include <table/e-table-defines.h>
#include <table/e-table-tooltip.h>
#include <widgets/misc/e-selection-model.h>
#include <widgets/misc/e-printable.h>

G_BEGIN_DECLS

#define E_TABLE_ITEM_TYPE        (e_table_item_get_type ())
#define E_TABLE_ITEM(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_ITEM_TYPE, ETableItem))
#define E_TABLE_ITEM_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_ITEM_TYPE, ETableItemClass))
#define E_IS_TABLE_ITEM(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_ITEM_TYPE))
#define E_IS_TABLE_ITEM_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_ITEM_TYPE))

typedef struct {
	GnomeCanvasItem  parent;
	ETableModel     *table_model;
	ETableHeader    *header;

	ETableModel     *source_model;
	ESelectionModel *selection;

	int              x1, y1;
	int              minimum_width, width, height;

	int              cols, rows;

	int              click_count;
	
	/*
	 * Ids for the signals we connect to
	 */
	int              header_dim_change_id;
	int              header_structure_change_id;
	int              header_request_width_id;
	int              table_model_pre_change_id;
	int              table_model_no_change_id;
	int              table_model_change_id;
	int              table_model_row_change_id;
	int              table_model_cell_change_id;
	int              table_model_rows_inserted_id;
	int              table_model_rows_deleted_id;

	int              selection_change_id;
	int              selection_row_change_id;
	int              cursor_change_id;
	int              cursor_activated_id;

	guint            cursor_idle_id;

	/* View row, -1 means unknown */
	int              old_cursor_row;

	int              hadjustment_change_id;
	int              hadjustment_value_change_id;
	int              vadjustment_change_id;
	int              vadjustment_value_change_id;
	
	GdkGC           *fill_gc;
	GdkGC           *grid_gc;
	GdkGC           *focus_gc;
	GdkBitmap       *stipple;

	guint		 alternating_row_colors:1;
	guint 		 horizontal_draw_grid:1;
	guint 		 vertical_draw_grid:1;
	guint 		 draw_focus:1;
	guint 		 uniform_row_height:1;
	guint 		 cell_views_realized:1;
	      	    
	guint 		 needs_redraw : 1;
	guint 		 needs_compute_height : 1;
	guint 		 needs_compute_width : 1;

	guint            uses_source_model : 1;

	guint            in_key_press : 1;

	guint            maybe_in_drag : 1;
	guint            in_drag : 1;
	guint            grabbed : 1;

	guint            maybe_did_something : 1;

	guint            cursor_on_screen : 1;
	guint            gtk_grabbed : 1;

	guint            queue_show_cursor : 1;
	guint            grab_cancelled : 1;

	int              frozen_count;

	int              cursor_x1;
	int              cursor_y1;
	int              cursor_x2;
	int              cursor_y2;

	int    		 drag_col;
	int    		 drag_row;
	int    		 drag_x;
	int    		 drag_y;
	guint            drag_state;

	/*
	 * Realized views, per column
	 */
	ECellView      **cell_views;
	int              n_cells;

	int             *height_cache;
	int              uniform_row_height_cache;
	int              height_cache_idle_id;
	int              height_cache_idle_count;

	/*
	 * Lengh Threshold: above this, we stop computing correctly
	 * the size
	 */
	int              length_threshold;
	
	gint             row_guess;
	ECursorMode      cursor_mode;

	int              motion_col, motion_row;

	/*
	 * During editing
	 */
	int              editing_col, editing_row;
	void            *edit_ctx;

	int              save_col, save_row;
	void            *save_state;

	int grabbed_col, grabbed_row;
	int grabbed_count;

	/*
	 * Tooltip
	 */
	ETableTooltip *tooltip;

} ETableItem;

typedef struct {
	GnomeCanvasItemClass parent_class;

	void        (*cursor_change)    (ETableItem *eti, int row);
	void        (*cursor_activated) (ETableItem *eti, int row);
	void        (*double_click)     (ETableItem *eti, int row, int col, GdkEvent *event);
	gint        (*right_click)      (ETableItem *eti, int row, int col, GdkEvent *event);
	gint        (*click)            (ETableItem *eti, int row, int col, GdkEvent *event);
	gint        (*key_press)        (ETableItem *eti, int row, int col, GdkEvent *event);
	gint        (*start_drag)       (ETableItem *eti, int row, int col, GdkEvent *event);
	void        (*style_set)        (ETableItem *eti, GtkStyle *previous_style);
	void        (*selection_model_removed)    (ETableItem *eti, ESelectionModel *selection);
	void        (*selection_model_added)    (ETableItem *eti, ESelectionModel *selection);
} ETableItemClass;
GType       e_table_item_get_type            (void);


/*
 * Focus
 */
void        e_table_item_set_cursor          (ETableItem        *eti,
					      int                col,
					      int                row);

gint        e_table_item_get_focused_column  (ETableItem        *eti);

void        e_table_item_leave_edit          (ETableItem        *eti);
void        e_table_item_enter_edit          (ETableItem        *eti,
					      int                col,
					      int                row);

void        e_table_item_redraw_range        (ETableItem        *eti,
					      int                start_col,
					      int                start_row,
					      int                end_col,
					      int                end_row);

EPrintable *e_table_item_get_printable       (ETableItem        *eti);
void        e_table_item_compute_location    (ETableItem        *eti,
					      int               *x,
					      int               *y,
					      int               *row,
					      int               *col);
void        e_table_item_get_cell_geometry   (ETableItem        *eti,
					      int               *row,
					      int               *col,
					      int               *x,
					      int               *y,
					      int               *width,
					      int               *height);

int	    e_table_item_row_diff	     (ETableItem	*eti,
					      int		 start_row,
					      int		 end_row);

G_END_DECLS

#endif /* _E_TABLE_ITEM_H_ */
