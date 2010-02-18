/*
 *
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
 *		Miguel de Icaza <miguel@gnu.org>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_TABLE_ITEM_H_
#define _E_TABLE_ITEM_H_

#include <libgnomecanvas/gnome-canvas.h>
#include <table/e-table-model.h>
#include <table/e-table-header.h>
#include <table/e-table-defines.h>
#include <misc/e-selection-model.h>
#include <misc/e-printable.h>

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

	gint              x1, y1;
	gint              minimum_width, width, height;

	gint              cols, rows;

	gint              click_count;

	/*
	 * Ids for the signals we connect to
	 */
	gint              header_dim_change_id;
	gint              header_structure_change_id;
	gint              header_request_width_id;
	gint              table_model_pre_change_id;
	gint              table_model_no_change_id;
	gint              table_model_change_id;
	gint              table_model_row_change_id;
	gint              table_model_cell_change_id;
	gint              table_model_rows_inserted_id;
	gint              table_model_rows_deleted_id;

	gint              selection_change_id;
	gint              selection_row_change_id;
	gint              cursor_change_id;
	gint              cursor_activated_id;

	guint            cursor_idle_id;

	/* View row, -1 means unknown */
	gint              old_cursor_row;

	GdkGC           *fill_gc;
	GdkGC           *grid_gc;
	GdkGC           *focus_gc;
	GdkBitmap       *stipple;

	guint		 alternating_row_colors:1;
	guint		 horizontal_draw_grid:1;
	guint		 vertical_draw_grid:1;
	guint		 draw_focus:1;
	guint		 uniform_row_height:1;
	guint		 cell_views_realized:1;

	guint		 needs_redraw : 1;
	guint		 needs_compute_height : 1;
	guint		 needs_compute_width : 1;

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

	gint              frozen_count;

	gint              cursor_x1;
	gint              cursor_y1;
	gint              cursor_x2;
	gint              cursor_y2;

	gint		 drag_col;
	gint		 drag_row;
	gint		 drag_x;
	gint		 drag_y;
	guint            drag_state;

	/*
	 * Realized views, per column
	 */
	ECellView      **cell_views;
	gint              n_cells;

	gint             *height_cache;
	gint              uniform_row_height_cache;
	gint              height_cache_idle_id;
	gint              height_cache_idle_count;

	/*
	 * Lengh Threshold: above this, we stop computing correctly
	 * the size
	 */
	gint              length_threshold;

	gint             row_guess;
	ECursorMode      cursor_mode;

	gint              motion_col, motion_row;

	/*
	 * During editing
	 */
	gint              editing_col, editing_row;
	void            *edit_ctx;

	gint              save_col, save_row;
	void            *save_state;

	gint grabbed_col, grabbed_row;
	gint grabbed_count;

} ETableItem;

typedef struct {
	GnomeCanvasItemClass parent_class;

	void        (*cursor_change)    (ETableItem *eti, gint row);
	void        (*cursor_activated) (ETableItem *eti, gint row);
	void        (*double_click)     (ETableItem *eti, gint row, gint col, GdkEvent *event);
	gboolean    (*right_click)      (ETableItem *eti, gint row, gint col, GdkEvent *event);
	gboolean    (*click)            (ETableItem *eti, gint row, gint col, GdkEvent *event);
	gboolean    (*key_press)        (ETableItem *eti, gint row, gint col, GdkEvent *event);
	gboolean    (*start_drag)       (ETableItem *eti, gint row, gint col, GdkEvent *event);
	void        (*style_set)        (ETableItem *eti, GtkStyle *previous_style);
	void        (*selection_model_removed)    (ETableItem *eti, ESelectionModel *selection);
	void        (*selection_model_added)    (ETableItem *eti, ESelectionModel *selection);
} ETableItemClass;
GType       e_table_item_get_type            (void);

/*
 * Focus
 */
void        e_table_item_set_cursor          (ETableItem        *eti,
					      gint                col,
					      gint                row);

gint        e_table_item_get_focused_column  (ETableItem        *eti);

void        e_table_item_leave_edit          (ETableItem        *eti);
void        e_table_item_enter_edit          (ETableItem        *eti,
					      gint                col,
					      gint                row);

void        e_table_item_redraw_range        (ETableItem        *eti,
					      gint                start_col,
					      gint                start_row,
					      gint                end_col,
					      gint                end_row);

EPrintable *e_table_item_get_printable       (ETableItem        *eti);
void        e_table_item_compute_location    (ETableItem        *eti,
					      gint               *x,
					      gint               *y,
					      gint               *row,
					      gint               *col);
void        e_table_item_compute_mouse_over  (ETableItem        *eti,
					      gint                x,
					      gint                y,
					      gint               *row,
					      gint               *col);
void        e_table_item_get_cell_geometry   (ETableItem        *eti,
					      gint               *row,
					      gint               *col,
					      gint               *x,
					      gint               *y,
					      gint               *width,
					      gint               *height);

gint	    e_table_item_row_diff	     (ETableItem	*eti,
					      gint		 start_row,
					      gint		 end_row);

G_END_DECLS

#endif /* _E_TABLE_ITEM_H_ */
