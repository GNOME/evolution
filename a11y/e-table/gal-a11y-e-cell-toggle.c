#include <gtk/gtk.h>
#include "gal-a11y-e-cell-toggle.h"
#include <gal/e-table/e-cell-toggle.h>
#include <gal/e-table/e-table-model.h>

static void gal_a11y_e_cell_toggle_class_init (GalA11yECellToggleClass *klass);

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
}

static void
toggle_cell_action (GalA11yECell *cell)
{
	ECellToggle * ect;
	gint finished;
	GdkEventButton event;
	gint x, y, width, height;
	gint row, col;

	row = cell->row;
	col = cell->view_col;

	e_table_item_get_cell_geometry (cell->item, &row, &col,
					&x, &y, &width, &height);
	event.x = x ;
	event.y = y ;
	event.type = GDK_BUTTON_PRESS;
	event.window = GTK_LAYOUT(GNOME_CANVAS_ITEM(cell->item)->canvas)->bin_window;
        event.button = 1;
        event.send_event = TRUE;
        event.time = GDK_CURRENT_TIME;
        event.axes = NULL;

	g_signal_emit_by_name (cell->item, "event", &event, &finished);
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

	return a11y;
}
