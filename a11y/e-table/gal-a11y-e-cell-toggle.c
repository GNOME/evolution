#include <gtk/gtk.h>
#include "gal-a11y-e-cell-toggle.h"
#include <gal/e-table/e-cell-toggle.h>
#include <gal/e-table/e-table-model.h>

#define PARENT_TYPE  (gal_a11y_e_cell_get_type ())
static GObjectClass *parent_class;

static void gal_a11y_e_cell_toggle_class_init (GalA11yECellToggleClass *klass);

static void
gal_a11y_e_cell_toggle_dispose (GObject *object)
{
	GalA11yECellToggle *a11y = GAL_A11Y_E_CELL_TOGGLE (object);

	ETableModel *e_table_model = GAL_A11Y_E_CELL (a11y)->item->table_model;

	if (e_table_model)
		g_signal_handler_disconnect (e_table_model, a11y->model_id);

	if (parent_class->dispose)
		parent_class->dispose (object);
}

GType
gal_a11y_e_cell_toggle_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      static const GTypeInfo tinfo =
      {
        sizeof (GalA11yECellToggleClass),
        (GBaseInitFunc) NULL, /* base init */
        (GBaseFinalizeFunc) NULL, /* base finalize */
        (GClassInitFunc) gal_a11y_e_cell_toggle_class_init, /* class init */
        (GClassFinalizeFunc) NULL, /* class finalize */
        NULL, /* class data */
        sizeof (GalA11yECellToggle), /* instance size */
        0, /* nb preallocs */
        NULL, /* instance init */
        NULL /* value table */
      };
                                                                                

      type = g_type_register_static (GAL_A11Y_TYPE_E_CELL,
                                     "GalA11yECellToggle", &tinfo, 0);
      gal_a11y_e_cell_type_add_action_interface (type);
	
    }
  return type;
}


static void 
gal_a11y_e_cell_toggle_class_init (GalA11yECellToggleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose      = gal_a11y_e_cell_toggle_dispose;
	parent_class               = g_type_class_ref (PARENT_TYPE);
}

static void
toggle_cell_action (GalA11yECell *cell)
{
	gint finished;
	GdkEventButton event;
	gint x, y, width, height;
	gint row, col;

	row = cell->row;
	col = cell->view_col;

	e_table_item_get_cell_geometry (cell->item, &row, &col,
					&x, &y, &width, &height);

	event.x = x + width / 2 + (int)(GNOME_CANVAS_ITEM (cell->item)->x1);
	event.y = y + height / 2 + (int)(GNOME_CANVAS_ITEM (cell->item)->y1);

	event.type = GDK_BUTTON_PRESS;
	event.window = GTK_LAYOUT(GNOME_CANVAS_ITEM(cell->item)->canvas)->bin_window;
        event.button = 1;
        event.send_event = TRUE;
        event.time = GDK_CURRENT_TIME;
        event.axes = NULL;

	g_signal_emit_by_name (cell->item, "event", &event, &finished);
}

static void
model_change_cb (ETableModel *etm,
		 gint row,
		 gint col,
		 GalA11yECell *cell)
{
	gint value;

	if (col == cell->model_col && row == cell->row) {

	        value = GPOINTER_TO_INT (
			e_table_model_value_at (cell->cell_view->e_table_model,
						cell->model_col, cell->row));
		if (value)
			gal_a11y_e_cell_add_state (cell, ATK_STATE_CHECKED, TRUE);
		else
			gal_a11y_e_cell_remove_state (cell, ATK_STATE_CHECKED, TRUE);
	}
}


AtkObject* 
gal_a11y_e_cell_toggle_new (ETableItem *item,
			    ECellView  *cell_view,
			    AtkObject  *parent,
			    int         model_col,
			    int         view_col,
			    int         row)
{
	AtkObject *a11y;
	GalA11yECell *cell;
	GalA11yECellToggle *toggle_cell;
	gint value;

	a11y = ATK_OBJECT(g_object_new (GAL_A11Y_TYPE_E_CELL_TOGGLE, NULL));

	g_return_val_if_fail (a11y != NULL, NULL);

	cell = GAL_A11Y_E_CELL(a11y);
	toggle_cell = GAL_A11Y_E_CELL_TOGGLE(a11y);
	a11y->role  = ATK_ROLE_TABLE_CELL;

        gal_a11y_e_cell_construct (a11y,
                                   item,
                                   cell_view,
                                   parent,
                                   model_col,
                                   view_col,
                                   row);

	gal_a11y_e_cell_add_action (cell, 
				    "toggle",	       /* action name*/
				    "toggle the cell", /* action description */
				    NULL,              /* action keybinding */
				    toggle_cell_action);

	toggle_cell->model_id = g_signal_connect (item->table_model,
						  "model_cell_changed",
						  (GCallback) model_change_cb,
						  a11y);

	value = GPOINTER_TO_INT (
			e_table_model_value_at (cell->cell_view->e_table_model,
                                                cell->model_col, cell->row));
	if (value)
		gal_a11y_e_cell_add_state (cell, ATK_STATE_CHECKED, FALSE);
	else
		gal_a11y_e_cell_remove_state (cell, ATK_STATE_CHECKED, FALSE);

	return a11y;
}
