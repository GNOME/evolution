/*
 * e-cell.c: base class for cell renderers in e-table
 *
 * Author:
 *   Miguel de Icaza (miguel@kernel.org)
 *
 * (C) 1999 Helix Code, Inc
 */
#include <config.h>
#include "e-cell.h"
#include "e-util.h"

#define PARENT_TYPE gtk_object_get_type()

static ECellView *
ec_realize (ECell *e_cell, void *view)
{
	return NULL;
}

static void
ec_unrealize (ECellView *e_cell)
{
}

static void
ec_draw (ECellView *ecell_view, GdkDrawable *drawable,
	 int col, int row, gboolean selected,
	 int x1, int y1, int x2, int y2)
{
	g_error ("e-cell-draw invoked\n");
}

static gint
ec_event (ECellView *ecell_view, GdkEvent *event, int col, int row)
{
	g_error ("e-cell-event invoked\n");
	return 0;
}

static gint
ec_height (ECellView *ecell_view, int col, int row)
{
	g_error ("e-cell-event invoked\n");
	return 0;
}

static void
ec_focus (ECellView *ecell_view, int col, int row, int x1, int y1, int x2, int y2)
{
	ecell_view->focus_col = col;
	ecell_view->focus_row = row;
	ecell_view->focus_x1 = x1;
	ecell_view->focus_y1 = y1;
	ecell_view->focus_x2 = x2;
	ecell_view->focus_y2 = y2;
}

static void
ec_unfocus (ECellView *ecell_view)
{
	ecell_view->focus_col = -1;
	ecell_view->focus_row = -1;
	ecell_view->focus_x1 = -1;
	ecell_view->focus_y1 = -1;
	ecell_view->focus_x2 = -1;
	ecell_view->focus_y2 = -1;
}

static void
e_cell_class_init (GtkObjectClass *object_class)
{
	ECellClass *ecc = (ECellClass *) object_class;

	ecc->realize = ec_realize;
	ecc->unrealize = ec_unrealize;
	ecc->draw = ec_draw;
	ecc->event = ec_event;
	ecc->focus = ec_focus;
	ecc->unfocus = ec_unfocus;
	ecc->height = ec_height;
}

static void
e_cell_init (GtkObject *object)
{
	ECell *e_cell = E_CELL (object);
}

E_MAKE_TYPE(e_cell, "ECell", ECell, e_cell_class_init, e_cell_init, PARENT_TYPE);

	
void
e_cell_event (ECellView *ecell_view, GdkEvent *event, int col, int row)
{
	E_CELL_CLASS (GTK_OBJECT (ecell_view->ecell)->klass)->event (
		ecell_view, event, col, row);
}

ECellView *
e_cell_realize (ECell *ecell, void *view)
{
	return E_CELL_CLASS (GTK_OBJECT (ecell)->klass)->realize (
		ecell, view);
}

void
e_cell_unrealize (ECellView *ecell_view)
{
	E_CELL_CLASS (GTK_OBJECT (ecell_view->ecell)->klass)->unrealize (ecell_view);
}
	
void
e_cell_draw (ECellView *ecell_view, GdkDrawable *drawable,
	     int col, int row, gboolean selected, int x1, int y1, int x2, int y2)
{
	E_CELL_CLASS (GTK_OBJECT (ecell_view->ecell)->klass)->draw (
		ecell_view, drawable, col, row, selected, x1, y1, x2, y2);
}

int
e_cell_height (ECellView *ecell_view, int col, int row)
{
	return E_CELL_CLASS (GTK_OBJECT (ecell_view->ecell)->klass)->height (
		ecell_view, col, row);
}

void *
e_cell_enter_edit (ECellView *ecell_view, int col, int row)
{
	return E_CELL_CLASS (GTK_OBJECT (ecell_view->ecell)->klass)->enter_edit (
		ecell_view, col, row);
}

void
e_cell_leave_edit (ECellView *ecell_view, int col, int row, void *edit_context)
{
	E_CELL_CLASS (GTK_OBJECT (ecell_view->ecell)->klass)->leave_edit (
		ecell_view, col, row, edit_context);
}
