/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_ITEM_H_
#define _E_TABLE_ITEM_H_

#include <libgnomeui/gnome-canvas.h>
#include "e-table-model.h"
#include "e-table-header.h"
#include <e-util/e-printable.h>

#define E_TABLE_ITEM_TYPE        (e_table_item_get_type ())
#define E_TABLE_ITEM(o)          (GTK_CHECK_CAST ((o), E_TABLE_ITEM_TYPE, ETableItem))
#define E_TABLE_ITEM_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_ITEM_TYPE, ETableItemClass))
#define E_IS_TABLE_ITEM(o)       (GTK_CHECK_TYPE ((o), E_TABLE_ITEM_TYPE))
#define E_IS_TABLE_ITEM_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_ITEM_TYPE))

/* list selection modes */
typedef enum
{
	E_TABLE_CURSOR_LINE,
	E_TABLE_CURSOR_SIMPLE,
} ETableCursorMode;

typedef struct {
	GnomeCanvasItem  parent;
	ETableModel     *table_model;
	ETableHeader    *header;

	ETableModel     *source_model;

	int              x1, y1;
	int              minimum_width, width, height;

	int              cols, rows;
	
	/*
	 * Ids for the signals we connect to
	 */
	int              header_dim_change_id;
	int              header_structure_change_id;
	int              table_model_change_id;
	int              table_model_row_change_id;
	int              table_model_cell_change_id;
	int              table_model_row_inserted_id;
	int              table_model_row_deleted_id;
	
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
	
	gint             cursor_row;
	gint             cursor_col;
	ETableCursorMode cursor_mode;

	GSList          *selection;

	/*
	 * During editing
	 */
	int              editing_col, editing_row;
	void            *edit_ctx;

} ETableItem;

typedef struct {
	GnomeCanvasItemClass parent_class;

	void        (*row_selection) (ETableItem *eti, int row, gboolean selected);
	void        (*cursor_change) (ETableItem *eti, int row);
	void        (*double_click)  (ETableItem *eti, int row);
	gint        (*right_click)   (ETableItem *eti, int row, int col, GdkEvent *event);
	gint        (*key_press)     (ETableItem *eti, int row, int col, GdkEvent *event);
} ETableItemClass;

GtkType    e_table_item_get_type (void);


/*
 * Focus
 */
void       e_table_item_focus    (ETableItem *eti, int col, int row);
void       e_table_item_unfocus  (ETableItem *eti);

gint       e_table_item_get_focused_column (ETableItem *eti);

/*
 * Handling the selection
 */
const GSList *e_table_item_get_selection   (ETableItem *e_table_Item);
gboolean      e_table_item_is_row_selected (ETableItem *e_table_Item,
					    int row);
	     				   
void          e_table_item_leave_edit      (ETableItem *eti);
void          e_table_item_enter_edit      (ETableItem *eti, int col, int row);
	     				   
void          e_table_item_redraw_range    (ETableItem *eti,
					    int start_col, int start_row,
					    int end_col, int end_row);
	     				   
EPrintable   *e_table_item_get_printable   (ETableItem        *eti);
void          e_table_item_print_height    (ETableItem        *eti,
					    GnomePrintContext *context,
					    gdouble            width);

#endif /* _E_TABLE_ITEM_H_ */
