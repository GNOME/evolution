#ifndef _E_TABLE_ITEM_H_
#define _E_TABLE_ITEM_H_

#include <libgnomeui/gnome-canvas.h>
#include "e-table-model.h"
#include "e-table-header.h"

#define E_TABLE_ITEM_TYPE        (e_table_item_get_type ())
#define E_TABLE_ITEM(o)          (GTK_CHECK_CAST ((o), E_TABLE_ITEM_TYPE, ETableItem))
#define E_TABLE_ITEM_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_ITEM_TYPE, ETableItemClass))
#define E_IS_TABLE_ITEM(o)       (GTK_CHECK_TYPE ((o), E_TABLE_ITEM_TYPE))
#define E_IS_TABLE_ITEM_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_ITEM_TYPE))

typedef struct {
	GnomeCanvasItem  parent;
	ETableModel     *table_model;
	ETableHeader    *header;

	int              x1, y1;
	int              width, height;

	int              top_item;
	int              cols, rows;
	
	/*
	 * Ids for the signals we connect to
	 */
	int              header_dim_change_id;
	int              header_structure_change_id;
	int              table_model_change_id;
	
	GdkGC           *fill_gc;
	GdkGC           *grid_gc;
	GdkGC           *focus_gc;

	unsigned int     draw_grid:1;
	unsigned int     draw_focus:1;
	
	int              focused_col, focused_row;

	/*
	 * Realized views, per column
	 */
	ECellView      **cell_views;
	int              n_cells;

	/*
	 * Lengh Threshold: above this, we stop computing correctly
	 * the size
	 */
	int              length_threshold;

	GSList          *selection;
	GtkSelectionMode selection_mode;

	/*
	 * During edition
	 */
	int              editing_col, editing_row;
	void            *edit_ctx;
} ETableItem;

typedef struct {
	GnomeCanvasItemClass parent_class;

	void        (*row_selection)      (ETableItem *eti, int row, gboolean selected);
} ETableItemClass;

GtkType    e_table_item_get_type (void);

/*
 * Focus
 */
void       e_table_item_focus    (ETableItem *eti, int col, int row);
void       e_table_item_unfocus  (ETableItem *eti);

/*
 * Selection
 */
void        e_table_item_select_row    (ETableItem *e_table_Item, int row);
void        e_table_item_unselect_row  (ETableItem *e_table_Item, int row);

/*
 * Handling the selection
 */
const GSList*e_table_item_get_selection (ETableItem *e_table_Item);

GtkSelectionMode e_table_item_get_selection_mode (ETableItem *e_table_Item);
void             e_table_item_set_selection_mode (ETableItem *e_table_Item,
						  GtkSelectionMode selection_mode);
gboolean         e_table_item_is_row_selected    (ETableItem *e_table_Item,
						  int row);

void             e_table_item_leave_edit         (ETableItem *eti);
void             e_table_item_enter_edit         (ETableItem *eti, int col, int row);

void             e_table_item_redraw_range       (ETableItem *eti,
						  int start_col, int start_row,
						  int end_col, int end_row);

#endif /* _E_TABLE_ITEM_H_ */
