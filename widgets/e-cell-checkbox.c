/*
 * e-cell-checkbox.c: Checkbox cell renderer
 *
 * Author:
 *   Miguel de Icaza (miguel@kernel.org)
 *
 * (C) 1999 Helix Code, Inc
 */
#include <config.h>
#include <gtk/gtkenums.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtksignal.h>
#include <gdk/gdkkeysyms.h>
#include <libgnomeui/gnome-canvas.h>
#include "e-cell-checkbox.h"
#include "e-util.h"
#include "e-table-item.h"

#define PARENT_TYPE e_cell_get_type()

typedef struct {
	ECellView    cell_view;
	GdkGC       *gc;
	GnomeCanvas *canvas;
	ETableItem  *eti;
} ECellCheckboxView;

static ECellClass *parent_class;

static void
eccb_queue_redraw (ECellCheckboxView *text_view, int col, int row)
{
	e_table_item_redraw_range (text_view->eti, col, row, col, row);
}

/*
 * ECell::realize method
 */
static ECellView *
eecb_realize (ECell *ecell, void *view)
{
	ECellCheckbox *eccb = E_CELL_CHECKBOX (ecell);
	ECellCheckboxView *check_view = g_new0 (ECellCheckboxView, 1);
	ETableItem *eti = E_TABLE_ITEM (view);
	GnomeCanvas *canvas = GNOME_CANVAS_ITEM (eti)->canvas;
	
	check_view->cell_view.ecell = ecell;
	check_view->gc = gdk_gc_new (GTK_WIDGET (canvas)->window);
	check_view->eti = eti;
	check_view->canvas = canvas;

	return (ECellView *) check_view;
}

/*
 * ECell::unrealize method
 */
static void
eecb_unrealize (ECellView *ecv)
{
	ECellCheckboxView *check_view = (ECellCheckboxView *) ecv;

	gdk_gc_unref (check_view->gc);
	text_view->gc = NULL;

	g_free (check_view);
}

/*
 * ECell::draw method
 */
static void
eecb_draw (ECellView *ecell_view, GdkDrawable *drawable,
	  int col, int row, gboolean selected,
	  int x1, int y1, int x2, int y2)
{
}

/*
 * ECell::event method
 */
static gint
eecb_event (ECellView *ecell_view, GdkEvent *event, int col, int row)
{
	ECellCheckboxView *text_view = (ECellCheckboxView *) ecell_view;
	
	switch (event->type){
	case GDK_BUTTON_PRESS:
		/*
		 * Adjust for the border we use
		 */
		event->button.x++;
		
		printf ("Button pressed at %g %g\n", event->button.x, event->button.y);
		if (text_view->edit){
			printf ("FIXME: Should handle click here\n");
		} else 
			e_table_item_enter_edit (text_view->eti, col, row);
		break;

	case GDK_BUTTON_RELEASE:
		/*
		 * Adjust for the border we use
		 */
		event->button.x++;
		printf ("Button released at %g %g\n", event->button.x, event->button.y);
		return TRUE;

	case GDK_KEY_PRESS:
		if (event->key.keyval == GDK_Escape){
			eecb_cancel_edit (text_view);
			return TRUE;
		}
		
		if (!text_view->edit){
			e_table_item_enter_edit (text_view->eti, col, row);
			eecb_edit_seleecb_all (text_view);
		}

		gtk_widget_event (GTK_WIDGET (text_view->edit->entry), event);
		eecb_queue_redraw (text_view, col, row);
		break;
		
	case GDK_KEY_RELEASE:
		break;
		
	default:
		return FALSE;
	}
	return TRUE;
}

/*
 * ECell::height method
 */
static int
eecb_height (ECellView *ecell_view, int col, int row)
{
	return 10;
}

/*
 * ECellView::enter_edit method
 */
static void *
eecb_enter_edit (ECellView *ecell_view, int col, int row)
{
}

/*
 * ECellView::leave_edit method
 */
static void
eecb_leave_edit (ECellView *ecell_view, int col, int row, void *edit_context)
{
}

static void
e_cell_checkbox_class_init (GtkObjectClass *object_class)
{
	ECellClass *ecc = (ECellClass *) object_class;

	ecc->realize    = eecb_realize;
	ecc->unrealize  = eecb_unrealize;
	ecc->draw       = eecb_draw;
	ecc->event      = eecb_event;
	ecc->height     = eecb_height;
	ecc->enter_edit = eecb_enter_edit;
	ecc->leave_edit = eecb_leave_edit;

	parent_class = gtk_type_class (PARENT_TYPE);
}

E_MAKE_TYPE(e_cell_text, "ECellCheckbox", ECellCheckbox, e_cell_checkbox_class_init, NULL, PARENT_TYPE);

ECell *
e_cell_checkbox_new (ETableModel *etm)
{
	ECellCheckbox *eccb = gtk_type_new (e_cell_checkbox_get_type ());

	E_CELL (eccb)->table_model = etm;
      
	return (ECell *) eccb;
}
