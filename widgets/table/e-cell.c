/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-cell.c: base class for cell renderers in e-table
 *
 * Author:
 *   Miguel de Icaza (miguel@kernel.org)
 *
 * (C) 1999 Ximian, Inc
 */
#include <config.h>
#include "e-cell.h"
#include "gal/util/e-util.h"

#define PARENT_TYPE gtk_object_get_type ()

static ECellView *
ec_new_view (ECell *ecell, ETableModel *table_model, void *e_table_item_view)
{
	return NULL;
}

static void
ec_realize (ECellView *e_cell)
{
}

static void
ec_kill_view (ECellView *ecell_view)
{
}

static void
ec_unrealize (ECellView *e_cell)
{
}

static void
ec_draw (ECellView *ecell_view, GdkDrawable *drawable,
	 int model_col, int view_col, int row, ECellFlags flags,
	 int x1, int y1, int x2, int y2)
{
	g_error ("e-cell-draw invoked\n");
}

static gint
ec_event (ECellView *ecell_view, GdkEvent *event, int model_col, int view_col, int row, ECellFlags flags, ECellActions *actions)
{
	g_error ("e-cell-event invoked\n");
	return 0;
}

static gint
ec_height (ECellView *ecell_view, int model_col, int view_col, int row)
{
	g_error ("e-cell-height invoked\n");
	return 0;
}

static void
ec_focus (ECellView *ecell_view, int model_col, int view_col, int row, int x1, int y1, int x2, int y2)
{
	ecell_view->focus_col = view_col; 
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

static void *
ec_enter_edit (ECellView *ecell_view, int model_col, int view_col, int row)
{
	return NULL;
}

static void
ec_leave_edit (ECellView *ecell_view, int model_col, int view_col, int row, void *context)
{
}

static void
ec_show_tooltip (ECellView *ecell_view, int model_col, int view_col, int row, int col_width, ETableTooltip *tooltip)
{
	/* Do nothing */
}

static void
e_cell_class_init (GtkObjectClass *object_class)
{
	ECellClass *ecc = (ECellClass *) object_class;

	ecc->realize = ec_realize;
	ecc->unrealize = ec_unrealize;
	ecc->new_view = ec_new_view;
	ecc->kill_view = ec_kill_view;
	ecc->draw = ec_draw;
	ecc->event = ec_event;
	ecc->focus = ec_focus;
	ecc->unfocus = ec_unfocus;
	ecc->height = ec_height;
	ecc->enter_edit = ec_enter_edit;
	ecc->leave_edit = ec_leave_edit;
	ecc->print = NULL;
	ecc->print_height = NULL;
	ecc->max_width = NULL;
	ecc->show_tooltip = ec_show_tooltip;
}

static void
e_cell_init (GtkObject *object)
{
}

E_MAKE_TYPE(e_cell, "ECell", ECell, e_cell_class_init, e_cell_init, PARENT_TYPE);

/**
 * e_cell_event:
 * @ecell_view: The ECellView where the event will be dispatched
 * @event: The GdkEvent.
 * @model_col: the column in the model
 * @view_col: the column in the view
 * @row: the row
 * @flags: flags about the current state
 * @actions: A second return value in case the cell wants to take some action (specifically grabbing & ungrabbing)
 *
 * Dispatches the event @event to the @ecell_view for.
 *
 * Returns: processing state from the GdkEvent handling.
 */
gint
e_cell_event (ECellView *ecell_view, GdkEvent *event, int model_col, int view_col, int row, ECellFlags flags, ECellActions *actions)
{
	return E_CELL_CLASS (GTK_OBJECT (ecell_view->ecell)->klass)->event (
		ecell_view, event, model_col, view_col, row, flags, actions);
}

/** 
 * e_cell_new_view:
 * @ecell: the Ecell that will create the new view
 * @table_model: the table model the ecell is bound to
 * @e_table_item_view: An ETableItem object (the CanvasItem that reprensents the view of the table)
 *
 * ECell renderers new to be bound to a table_model and to the actual view
 * during their life time to actually render the data.  This method is invoked
 * by the ETableItem canvas item to instatiate a new view of the ECell.
 *
 * This is invoked when the ETableModel is attached to the ETableItem (a CanvasItem
 * that can render ETableModels in the screen).
 *
 * Returns: a new ECellView for this @ecell on the @table_model displayed on the @e_table_item_view.
 */
ECellView *
e_cell_new_view (ECell *ecell, ETableModel *table_model, void *e_table_item_view)
{
	return E_CELL_CLASS (GTK_OBJECT (ecell)->klass)->new_view (
		ecell, table_model, e_table_item_view);
}

/**
 * e_cell_realize:
 * @ecell_view: The ECellView to be realized.
 *
 * This function is invoked to give a chance to the ECellView to allocate
 * any resources it needs from Gdk, equivalent to the GtkWidget::realize
 * signal.
 */
void
e_cell_realize (ECellView *ecell_view)
{
	return E_CELL_CLASS (GTK_OBJECT (ecell_view->ecell)->klass)->realize (ecell_view);
}

/**
 * e_cell_kill_view:
 * @ecell_view: view to be destroyed.
 *
 * This method it used to destroy a view of an ECell renderer
 */
void
e_cell_kill_view (ECellView *ecell_view)
{
	E_CELL_CLASS (GTK_OBJECT (ecell_view->ecell)->klass)->kill_view (ecell_view);
}

/**
 * e_cell_unrealize:
 * @ecell_view: The ECellView to be unrealized.
 *
 * This function is invoked to give a chance to the ECellView to
 * release any resources it allocated during the realize method,
 * equivalent to the GtkWidget::unrealize signal.
 */
void
e_cell_unrealize (ECellView *ecell_view)
{
	E_CELL_CLASS (GTK_OBJECT (ecell_view->ecell)->klass)->unrealize (ecell_view);
}

/**
 * e_cell_draw:
 * @ecell_view: the ECellView to redraw
 * @drawable: draw desination
 * @model_col: the column in the model being drawn.
 * @view_col: the column in the view being drawn (what the model maps to).
 * @row: the row being drawn
 * @flags: rendering flags.
 * @x1: boudary for the rendering
 * @y1: boudary for the rendering
 * @x2: boudary for the rendering
 * @y2: boudary for the rendering
 *
 * This instructs the ECellView to render itself into the drawable.  The
 * region to be drawn in given by (x1,y1)-(x2,y2).
 *
 * The most important flags are %E_CELL_SELECTED and %E_CELL_FOCUSED, other
 * flags include alignments and justifications.
 */
void
e_cell_draw (ECellView *ecell_view, GdkDrawable *drawable,
	     int model_col, int view_col, int row, ECellFlags flags,
	     int x1, int y1, int x2, int y2)
{
	E_CELL_CLASS (GTK_OBJECT (ecell_view->ecell)->klass)->draw (
		ecell_view, drawable, model_col, view_col, row, flags, x1, y1, x2, y2);
}

/**
 * e_cell_print:
 * @ecell_view: the ECellView to redraw
 * @context: The GnomePrintContext where we output our printed data.
 * @model_col: the column in the model being drawn.
 * @view_col: the column in the view being drawn (what the model maps to).
 * @row: the row being drawn
 * @width: width 
 * @height: height
 *
 * FIXME:
 */
void
e_cell_print (ECellView *ecell_view, GnomePrintContext *context, 
	      int model_col, int view_col, int row,
	      double width, double height)
{
	E_CELL_CLASS (GTK_OBJECT (ecell_view->ecell)->klass)->print
		(ecell_view, context, model_col, view_col, row, width, height);
}

/**
 * e_cell_print:
 *
 * FIXME:
 */
gdouble
e_cell_print_height (ECellView *ecell_view, GnomePrintContext *context, 
		     int model_col, int view_col, int row,
		     double width)
{
	if (E_CELL_CLASS (GTK_OBJECT (ecell_view->ecell)->klass)->print_height)
		return E_CELL_CLASS (GTK_OBJECT (ecell_view->ecell)->klass)->print_height
			(ecell_view, context, model_col, view_col, row, width);
	else
		return 0.0;
}

/**
 * e_cell_height:
 * @ecell_view: the ECellView.
 * @model_col: the column in the model
 * @view_col: the column in the view.
 * @row: the row to me measured
 *
 * Returns: the height of the cell at @model_col, @row rendered at
 * @view_col, @row.
 */
int
e_cell_height (ECellView *ecell_view, int model_col, int view_col, int row)
{
	return E_CELL_CLASS (GTK_OBJECT (ecell_view->ecell)->klass)->height (
		ecell_view, model_col, view_col, row);
}

/**
 * e_cell_enter_edit:
 * @ecell_view: the ECellView that will enter editing
 * @model_col: the column in the model
 * @view_col: the column in the view
 * @row: the row
 *
 * Notifies the ECellView that it is about to enter editing mode for
 * @model_col, @row rendered at @view_col, @row.
 */
void *
e_cell_enter_edit (ECellView *ecell_view, int model_col, int view_col, int row)
{
	return E_CELL_CLASS (GTK_OBJECT (ecell_view->ecell)->klass)->enter_edit (
		ecell_view, model_col, view_col, row);
}

/**
 * e_cell_leave_edit:
 * @ecell_view: the ECellView that will leave editing
 * @model_col: the column in the model
 * @view_col: the column in the view
 * @row: the row
 * @edit_context: the editing context
 *
 * Notifies the ECellView that editing is finished at @model_col, @row
 * rendered at @view_col, @row.
 */
void
e_cell_leave_edit (ECellView *ecell_view, int model_col, int view_col, int row, void *edit_context)
{
	E_CELL_CLASS (GTK_OBJECT (ecell_view->ecell)->klass)->leave_edit (
		ecell_view, model_col, view_col, row, edit_context);
}

/**
 * e_cell_max_width:
 * @ecell_view: the ECellView that will leave editing
 * @model_col: the column in the model
 * @view_col: the column in the view.
 *
 * Returns: the maximum width for the ECellview at @model_col which
 * is being rendered as @view_col
 */
int
e_cell_max_width (ECellView *ecell_view, int model_col, int view_col)
{
	return E_CELL_CLASS (GTK_OBJECT (ecell_view->ecell)->klass)->max_width 
		(ecell_view, model_col, view_col);
}
	      
void
e_cell_show_tooltip (ECellView *ecell_view, int model_col, int view_col, 
		     int row, int col_width, ETableTooltip *tooltip)
{
	return E_CELL_CLASS (GTK_OBJECT (ecell_view->ecell)->klass)->show_tooltip
		(ecell_view, model_col, view_col, row, col_width, tooltip);
}
