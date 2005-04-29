/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-cell.c - base class for cell renderers in e-table
 * Copyright 1999, 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Miguel de Icaza <miguel@ximian.com>
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include "gal/util/e-util.h"

#include "e-cell.h"

#define PARENT_TYPE GTK_TYPE_OBJECT

#define ECVIEW_EC_CLASS(v) (E_CELL_GET_CLASS (v->ecell))

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

static void *
ec_save_state (ECellView *ecell_view, int model_col, int view_col, int row, void *context)
{
	return NULL;
}

static void
ec_load_state (ECellView *ecell_view, int model_col, int view_col, int row, void *context, void *save_state)
{
}

static void
ec_free_state (ECellView *ecell_view, int model_col, int view_col, int row, void *save_state)
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
	ecc->save_state = ec_save_state;
	ecc->load_state = ec_load_state;
	ecc->free_state = ec_free_state;
	ecc->print = NULL;
	ecc->print_height = NULL;
	ecc->max_width = NULL;
	ecc->max_width_by_row = NULL;
	ecc->show_tooltip = ec_show_tooltip;
}

static void
e_cell_init (GtkObject *object)
{
}

E_MAKE_TYPE(e_cell, "ECell", ECell, e_cell_class_init, e_cell_init, PARENT_TYPE)

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
	return ECVIEW_EC_CLASS(ecell_view)->event (
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
	return E_CELL_GET_CLASS (ecell)->new_view (
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
	ECVIEW_EC_CLASS(ecell_view)->realize (ecell_view);
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
	ECVIEW_EC_CLASS(ecell_view)->kill_view (ecell_view);
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
	ECVIEW_EC_CLASS(ecell_view)->unrealize (ecell_view);
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
	g_return_if_fail (ecell_view != NULL);
	g_return_if_fail (row >= 0);
	g_return_if_fail (row < e_table_model_row_count(ecell_view->e_table_model));

	ECVIEW_EC_CLASS(ecell_view)->draw (ecell_view, drawable, model_col, view_col, row, flags, x1, y1, x2, y2);
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
	if (ECVIEW_EC_CLASS(ecell_view)->print)
		ECVIEW_EC_CLASS(ecell_view)->print (ecell_view, context, model_col, view_col, row, width, height);
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
	if (ECVIEW_EC_CLASS(ecell_view)->print_height)
		return ECVIEW_EC_CLASS(ecell_view)->print_height
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
	return ECVIEW_EC_CLASS(ecell_view)->height (ecell_view, model_col, view_col, row);
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
	return ECVIEW_EC_CLASS(ecell_view)->enter_edit (ecell_view, model_col, view_col, row);
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
	ECVIEW_EC_CLASS(ecell_view)->leave_edit (ecell_view, model_col, view_col, row, edit_context);
}

/**
 * e_cell_save_state:
 * @ecell_view: the ECellView to save
 * @model_col: the column in the model
 * @view_col: the column in the view
 * @row: the row
 * @edit_context: the editing context
 *
 * Returns: The save state.
 *
 * Requests that the ECellView return a void * representing the state
 * of the ECell.  This is primarily intended for things like selection
 * or scrolling.
 */
void *
e_cell_save_state (ECellView *ecell_view, int model_col, int view_col, int row, void *edit_context)
{
	if (ECVIEW_EC_CLASS(ecell_view)->save_state)
		return ECVIEW_EC_CLASS(ecell_view)->save_state (ecell_view, model_col, view_col, row, edit_context);
	else
		return NULL;
}

/**
 * e_cell_load_state:
 * @ecell_view: the ECellView to load
 * @model_col: the column in the model
 * @view_col: the column in the view
 * @row: the row
 * @edit_context: the editing context
 * @save_state: the save state to load from
 *
 * Requests that the ECellView load from the given save state.
 */
void
e_cell_load_state (ECellView *ecell_view, int model_col, int view_col, int row, void *edit_context, void *save_state)
{
	if (ECVIEW_EC_CLASS(ecell_view)->load_state)
		ECVIEW_EC_CLASS(ecell_view)->load_state (ecell_view, model_col, view_col, row, edit_context, save_state);
}

/**
 * e_cell_load_state:
 * @ecell_view: the ECellView
 * @model_col: the column in the model
 * @view_col: the column in the view
 * @row: the row
 * @edit_context: the editing context
 * @save_state: the save state to free
 *
 * Requests that the ECellView free the given save state.
 */
void
e_cell_free_state (ECellView *ecell_view, int model_col, int view_col, int row, void *save_state)
{
	if (ECVIEW_EC_CLASS(ecell_view)->free_state)
		ECVIEW_EC_CLASS(ecell_view)->free_state (ecell_view, model_col, view_col, row, save_state);
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
	return ECVIEW_EC_CLASS(ecell_view)->max_width 
		(ecell_view, model_col, view_col);
}

/**
 * e_cell_max_width_by_row:
 * @ecell_view: the ECellView that we are curious about
 * @model_col: the column in the model
 * @view_col: the column in the view.
 * @row: The row in the model.
 *
 * Returns: the maximum width for the ECellview at @model_col which
 * is being rendered as @view_col for the data in @row.
 */
int
e_cell_max_width_by_row (ECellView *ecell_view, int model_col, int view_col, int row)
{
	if (ECVIEW_EC_CLASS(ecell_view)->max_width_by_row)
		return ECVIEW_EC_CLASS(ecell_view)->max_width_by_row
			(ecell_view, model_col, view_col, row);
	else
		return e_cell_max_width (ecell_view, model_col, view_col);
}

/**
 * e_cell_max_width_by_row_implemented:
 * @ecell_view: the ECellView that we are curious about
 * @model_col: the column in the model
 * @view_col: the column in the view.
 * @row: The row in the model.
 *
 * Returns: the maximum width for the ECellview at @model_col which
 * is being rendered as @view_col for the data in @row.
 */
gboolean
e_cell_max_width_by_row_implemented (ECellView *ecell_view)
{
	return (ECVIEW_EC_CLASS(ecell_view)->max_width_by_row != NULL);
}
	      
void
e_cell_show_tooltip (ECellView *ecell_view, int model_col, int view_col, 
		     int row, int col_width, ETableTooltip *tooltip)
{
	ECVIEW_EC_CLASS(ecell_view)->show_tooltip
		(ecell_view, model_col, view_col, row, col_width, tooltip);
}

gchar *
e_cell_get_bg_color(ECellView *ecell_view, int row)
{
	if (ECVIEW_EC_CLASS(ecell_view)->get_bg_color)
		return ECVIEW_EC_CLASS(ecell_view)->get_bg_color (ecell_view, row);
	else
		return NULL;
}

void
e_cell_style_set(ECellView *ecell_view, GtkStyle *previous_style)
{
	if (ECVIEW_EC_CLASS(ecell_view)->style_set)
		ECVIEW_EC_CLASS(ecell_view)->style_set (ecell_view, previous_style);
}
		 		
