/*
 *
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
 *		Miguel de Icaza <miguel@gnu.org>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_TABLE_ITEM_H_
#define _E_TABLE_ITEM_H_

#include <libgnomecanvas/libgnomecanvas.h>

#include <e-util/e-printable.h>
#include <e-util/e-selection-model.h>
#include <e-util/e-table-defines.h>
#include <e-util/e-table-header.h>
#include <e-util/e-table-model.h>

/* Standard GObject macros */
#define E_TYPE_TABLE_ITEM \
	(e_table_item_get_type ())
#define E_TABLE_ITEM(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TABLE_ITEM, ETableItem))
#define E_TABLE_ITEM_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TABLE_ITEM, ETableItemClass))
#define E_IS_TABLE_ITEM(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TABLE_ITEM))
#define E_IS_TABLE_ITEM_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TABLE_ITEM))
#define E_TABLE_ITEM_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TABLE_ITEM, ETableItemClass))

G_BEGIN_DECLS

typedef struct _ETableItem ETableItem;
typedef struct _ETableItemClass ETableItemClass;

struct _ETableItem {
	GnomeCanvasItem parent;
	ETableModel *table_model;
	ETableHeader *header;

	ETableModel *source_model;
	ESelectionModel *selection;

	gint minimum_width, width, height;

	gint cols, rows;

	gint click_count;

	/*
	 * Ids for the signals we connect to
	 */
	gint header_dim_change_id;
	gint header_structure_change_id;
	gint header_request_width_id;
	gint table_model_pre_change_id;
	gint table_model_no_change_id;
	gint table_model_change_id;
	gint table_model_row_change_id;
	gint table_model_cell_change_id;
	gint table_model_rows_inserted_id;
	gint table_model_rows_deleted_id;

	gint selection_change_id;
	gint selection_row_change_id;
	gint cursor_change_id;
	gint cursor_activated_id;

	guint cursor_idle_id;

	/* View row, -1 means unknown */
	gint old_cursor_row;

	guint alternating_row_colors : 1;
	guint horizontal_draw_grid : 1;
	guint vertical_draw_grid : 1;
	guint draw_focus : 1;
	guint uniform_row_height : 1;
	guint cell_views_realized : 1;

	guint needs_redraw : 1;
	guint needs_compute_height : 1;
	guint needs_compute_width : 1;

	guint uses_source_model : 1;

	guint in_key_press : 1;

	guint maybe_in_drag : 1;
	guint in_drag : 1;
	guint unused__grabbed : 1; /* this one is not used in the code */

	guint maybe_did_something : 1;

	guint cursor_on_screen : 1;
	guint gtk_grabbed : 1;

	guint queue_show_cursor : 1;
	guint grab_cancelled : 1;

	gint frozen_count;

	gint cursor_x1;
	gint cursor_y1;
	gint cursor_x2;
	gint cursor_y2;

	gint drag_col;
	gint drag_row;
	gint drag_x;
	gint drag_y;
	guint drag_state;

	/*
	 * Realized views, per column
	 */
	ECellView **cell_views;
	gint n_cells;

	gint *height_cache;
	gint uniform_row_height_cache;
	gint height_cache_idle_id;
	gint height_cache_idle_count;

	/*
	 * Lengh Threshold: above this, we stop computing correctly
	 * the size
	 */
	gint length_threshold;

	gint row_guess;
	ECursorMode cursor_mode;

	gint motion_col, motion_row;

	/*
	 * During editing
	 */
	gint editing_col, editing_row;
	gpointer edit_ctx;

	gint save_col, save_row;
	gpointer save_state;

	gint grabbed_col, grabbed_row;
	gint grabbed_count;
};

struct _ETableItemClass {
	GnomeCanvasItemClass parent_class;

	void		(*cursor_change)	(ETableItem *eti,
						 gint row);
	void		(*cursor_activated)	(ETableItem *eti,
						 gint row);
	void		(*double_click)		(ETableItem *eti,
						 gint row,
						 gint col,
						 GdkEvent *event);
	gboolean	(*right_click)		(ETableItem *eti,
						 gint row,
						 gint col,
						 GdkEvent *event);
	gboolean	(*click)		(ETableItem *eti,
						 gint row,
						 gint col,
						 GdkEvent *event);
	gboolean	(*key_press)		(ETableItem *eti,
						 gint row,
						 gint col,
						 GdkEvent *event);
	gboolean	(*start_drag)		(ETableItem *eti,
						 gint row,
						 gint col,
						 GdkEvent *event);
	void		(*style_updated)	(ETableItem *eti);
	void		(*selection_model_removed)
						(ETableItem *eti,
						 ESelectionModel *selection);
	void		(*selection_model_added)
						(ETableItem *eti,
						 ESelectionModel *selection);
	void		(*get_bg_color)		(ETableItem *eti,
						 gint row,
						 gint col,
						 GdkRGBA *inout_background);
};

GType		e_table_item_get_type		(void) G_GNUC_CONST;

/*
 * Focus
 */
void		e_table_item_set_cursor		(ETableItem *eti,
						 gint col,
						 gint row);

gint		e_table_item_get_focused_column	(ETableItem *eti);

void		e_table_item_leave_edit		(ETableItem *eti);
void		e_table_item_enter_edit		(ETableItem *eti,
						 gint col,
						 gint row);

void		e_table_item_redraw_range	(ETableItem *eti,
						 gint start_col,
						 gint start_row,
						 gint end_col,
						 gint end_row);

EPrintable *	e_table_item_get_printable	(ETableItem *eti);
void		e_table_item_compute_location	(ETableItem *eti,
						 gint *x,
						 gint *y,
						 gint *row,
						 gint *col);
void		e_table_item_compute_mouse_over	(ETableItem *eti,
						 gint x,
						 gint y,
						 gint *row,
						 gint *col);
void		e_table_item_get_cell_geometry	(ETableItem *eti,
						 gint *row,
						 gint *col,
						 gint *x,
						 gint *y,
						 gint *width,
						 gint *height);

gint		e_table_item_row_diff		(ETableItem *eti,
						 gint start_row,
						 gint end_row);

gboolean	e_table_item_is_editing		(ETableItem *eti);

void		e_table_item_cursor_scrolled	(ETableItem *eti);

void		e_table_item_cancel_scroll_to_cursor
						(ETableItem *eti);
gboolean	e_table_item_get_row_selected	(ETableItem *eti,
						 gint row);
void		e_table_item_freeze		(ETableItem *eti);
void		e_table_item_thaw		(ETableItem *eti);

G_END_DECLS

#endif /* _E_TABLE_ITEM_H_ */
