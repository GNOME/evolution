/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_ITEM_H_
#define _E_TABLE_ITEM_H_

#include <libgnomeui/gnome-canvas.h>
#include <gal/e-table/e-table-model.h>
#include <gal/e-table/e-table-header.h>
#include <gal/e-table/e-table-selection-model.h>
#include <gal/e-table/e-table-defines.h>
#include <gal/e-table/e-table-tooltip.h>
#include <gal/widgets/e-printable.h>

BEGIN_GNOME_DECLS

#define E_TABLE_ITEM_TYPE        (e_table_item_get_type ())
#define E_TABLE_ITEM(o)          (GTK_CHECK_CAST ((o), E_TABLE_ITEM_TYPE, ETableItem))
#define E_TABLE_ITEM_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_ITEM_TYPE, ETableItemClass))
#define E_IS_TABLE_ITEM(o)       (GTK_CHECK_TYPE ((o), E_TABLE_ITEM_TYPE))
#define E_IS_TABLE_ITEM_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_ITEM_TYPE))

typedef struct {
	GnomeCanvasItem  parent;
	ETableModel     *table_model;
	ETableHeader    *header;

	ETableModel     *source_model;
	ETableSelectionModel *selection;

	int              x1, y1;
	int              minimum_width, width, height;

	int              cols, rows;
	
	/*
	 * Ids for the signals we connect to
	 */
	int              header_dim_change_id;
	int              header_structure_change_id;
	int              header_request_width_id;
	int              table_model_pre_change_id;
	int              table_model_change_id;
	int              table_model_row_change_id;
	int              table_model_cell_change_id;
	int              table_model_row_inserted_id;
	int              table_model_row_deleted_id;

	int              selection_change_id;
	int              cursor_change_id;
	int              cursor_activated_id;
	
	GdkGC           *fill_gc;
	GdkGC           *grid_gc;
	GdkGC           *focus_gc;
	GdkBitmap       *stipple;

	guint 		 draw_grid:1;
	guint 		 draw_focus:1;
	guint 		 renderers_can_change_size:1;
	guint 		 cell_views_realized:1;
	      	    
	guint 		 needs_redraw : 1;
	guint 		 needs_compute_height : 1;
	guint 		 needs_compute_width : 1;

	guint            uses_source_model : 1;

	guint            in_key_press : 1;

	/*
	 * Realized views, per column
	 */
	ECellView      **cell_views;
	int              n_cells;

	int             *height_cache;
	int              height_cache_idle_id;
	int              height_cache_idle_count;

	/*
	 * Lengh Threshold: above this, we stop computing correctly
	 * the size
	 */
	int              length_threshold;
	
	gint             row_guess;
	ETableCursorMode cursor_mode;

	/*
	 * During editing
	 */
	int              editing_col, editing_row;
	void            *edit_ctx;

	int grabbed_col, grabbed_row;

	/*
	 * Tooltip
	 */
	ETableTooltip *tooltip;

} ETableItem;

typedef struct {
	GnomeCanvasItemClass parent_class;

	void        (*cursor_change) (ETableItem *eti, int row);
	void        (*cursor_activated) (ETableItem *eti, int row);
	void        (*double_click)  (ETableItem *eti, int row);
	gint        (*right_click)   (ETableItem *eti, int row, int col, GdkEvent *event);
	gint        (*click)   (ETableItem *eti, int row, int col, GdkEvent *event);
	gint        (*key_press)     (ETableItem *eti, int row, int col, GdkEvent *event);
} ETableItemClass;
GtkType     e_table_item_get_type            (void);


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


END_GNOME_DECLS

#endif /* _E_TABLE_ITEM_H_ */
