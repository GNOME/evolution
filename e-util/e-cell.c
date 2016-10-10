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
 *		Miguel de Icaza <miguel@ximian.com>
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <gtk/gtk.h>

#include "e-cell.h"

G_DEFINE_TYPE (ECell, e_cell, G_TYPE_OBJECT)

static ECellView *
ec_new_view (ECell *ecell,
             ETableModel *table_model,
             gpointer e_table_item_view)
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
ec_draw (ECellView *ecell_view,
         cairo_t *cr,
         gint model_col,
         gint view_col,
         gint row,
         ECellFlags flags,
         gint x1,
         gint y1,
         gint x2,
         gint y2)
{
	g_critical ("e-cell-draw invoked");
}

static gint
ec_event (ECellView *ecell_view,
          GdkEvent *event,
          gint model_col,
          gint view_col,
          gint row,
          ECellFlags flags,
          ECellActions *actions)
{
	g_critical ("e-cell-event invoked");

	return 0;
}

static gint
ec_height (ECellView *ecell_view,
           gint model_col,
           gint view_col,
           gint row)
{
	g_critical ("e-cell-height invoked");

	return 0;
}

static void
ec_focus (ECellView *ecell_view,
          gint model_col,
          gint view_col,
          gint row,
          gint x1,
          gint y1,
          gint x2,
          gint y2)
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

static gpointer
ec_enter_edit (ECellView *ecell_view,
               gint model_col,
               gint view_col,
               gint row)
{
	return NULL;
}

static void
ec_leave_edit (ECellView *ecell_view,
               gint model_col,
               gint view_col,
               gint row,
               gpointer context)
{
}

static gpointer
ec_save_state (ECellView *ecell_view,
               gint model_col,
               gint view_col,
               gint row,
               gpointer context)
{
	return NULL;
}

static void
ec_load_state (ECellView *ecell_view,
               gint model_col,
               gint view_col,
               gint row,
               gpointer context,
               gpointer save_state)
{
}

static void
ec_free_state (ECellView *ecell_view,
               gint model_col,
               gint view_col,
               gint row,
               gpointer save_state)
{
}

static void
e_cell_class_init (ECellClass *class)
{
	class->realize = ec_realize;
	class->unrealize = ec_unrealize;
	class->new_view = ec_new_view;
	class->kill_view = ec_kill_view;
	class->draw = ec_draw;
	class->event = ec_event;
	class->focus = ec_focus;
	class->unfocus = ec_unfocus;
	class->height = ec_height;
	class->enter_edit = ec_enter_edit;
	class->leave_edit = ec_leave_edit;
	class->save_state = ec_save_state;
	class->load_state = ec_load_state;
	class->free_state = ec_free_state;
	class->print = NULL;
	class->print_height = NULL;
	class->max_width = NULL;
	class->max_width_by_row = NULL;
}

static void
e_cell_init (ECell *cell)
{
}

/**
 * e_cell_event:
 * @ecell_view: The ECellView where the event will be dispatched
 * @event: The GdkEvent.
 * @model_col: the column in the model
 * @view_col: the column in the view
 * @row: the row
 * @flags: flags about the current state
 * @actions: a second return value in case the cell wants to take some action
 *           (specifically grabbing & ungrabbing)
 *
 * Dispatches the event @event to the @ecell_view for.
 *
 * Returns: processing state from the GdkEvent handling.
 */
gint
e_cell_event (ECellView *ecell_view,
              GdkEvent *event,
              gint model_col,
              gint view_col,
              gint row,
              ECellFlags flags,
              ECellActions *actions)
{
	ECellClass *class;

	class = E_CELL_GET_CLASS (ecell_view->ecell);

	return class->event (
		ecell_view, event, model_col,
		view_col, row, flags, actions);
}

/**
 * e_cell_new_view:
 * @ecell: the Ecell that will create the new view
 * @table_model: the table model the ecell is bound to
 * @e_table_item_view: an ETableItem object (the CanvasItem that
 *                     reprensents the view of the table)
 *
 * ECell renderers new to be bound to a table_model and to the actual view
 * during their life time to actually render the data.  This method is invoked
 * by the ETableItem canvas item to instatiate a new view of the ECell.
 *
 * This is invoked when the ETableModel is attached to the ETableItem
 * (a CanvasItem that can render ETableModels in the screen).
 *
 * Returns: a new ECellView for this @ecell on the @table_model displayed
 * on the @e_table_item_view.
 */
ECellView *
e_cell_new_view (ECell *ecell,
                 ETableModel *table_model,
                 gpointer e_table_item_view)
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
	ECellClass *class;

	class = E_CELL_GET_CLASS (ecell_view->ecell);
	g_return_if_fail (class->realize != NULL);

	class->realize (ecell_view);
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
	ECellClass *class;

	class = E_CELL_GET_CLASS (ecell_view->ecell);
	g_return_if_fail (class->kill_view != NULL);

	class->kill_view (ecell_view);
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
	ECellClass *class;

	class = E_CELL_GET_CLASS (ecell_view->ecell);
	g_return_if_fail (class->unrealize != NULL);

	class->unrealize (ecell_view);
}

/**
 * e_cell_draw:
 * @ecell_view: the ECellView to redraw
 * @cr: a Cairo context
 * @model_col: the column in the model being drawn.
 * @view_col: the column in the view being drawn (what the model maps to).
 * @row: the row being drawn
 * @flags: rendering flags.
 * @x1: boudary for the rendering
 * @y1: boudary for the rendering
 * @x2: boudary for the rendering
 * @y2: boudary for the rendering
 *
 * This instructs the ECellView to render itself into the Cairo context.
 * The region to be drawn in given by (x1,y1)-(x2,y2).
 *
 * The most important flags are %E_CELL_SELECTED and %E_CELL_FOCUSED, other
 * flags include alignments and justifications.
 */
void
e_cell_draw (ECellView *ecell_view,
             cairo_t *cr,
             gint model_col,
             gint view_col,
             gint row,
             ECellFlags flags,
             gint x1,
             gint y1,
             gint x2,
             gint y2)
{
	ECellClass *class;

	g_return_if_fail (ecell_view != NULL);
	g_return_if_fail (row >= 0);
	g_return_if_fail (row < e_table_model_row_count (ecell_view->e_table_model));

	class = E_CELL_GET_CLASS (ecell_view->ecell);
	g_return_if_fail (class->draw != NULL);

	cairo_save (cr);

	class->draw (
		ecell_view, cr,
		model_col, view_col,
		row, flags, x1, y1, x2, y2);

	cairo_restore (cr);
}

/**
 * e_cell_print:
 * @ecell_view: the ECellView to redraw
 * @context: The GtkPrintContext where we output our printed data.
 * @model_col: the column in the model being drawn.
 * @view_col: the column in the view being drawn (what the model maps to).
 * @row: the row being drawn
 * @width: width
 * @height: height
 *
 * FIXME:
 */
void
e_cell_print (ECellView *ecell_view,
              GtkPrintContext *context,
              gint model_col,
              gint view_col,
              gint row,
              gdouble width,
              gdouble height)
{
	ECellClass *class;

	class = E_CELL_GET_CLASS (ecell_view->ecell);

	if (class->print != NULL)
		class->print (
			ecell_view, context,
			model_col, view_col,
			row, width, height);
}

/**
 * e_cell_print:
 *
 * FIXME:
 */
gdouble
e_cell_print_height (ECellView *ecell_view,
                     GtkPrintContext *context,
                     gint model_col,
                     gint view_col,
                     gint row,
                     gdouble width)
{
	ECellClass *class;

	class = E_CELL_GET_CLASS (ecell_view->ecell);

	if (class->print_height == NULL)
		return 0.0;

	return class->print_height (
		ecell_view, context,
		model_col, view_col,
		row, width);
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
gint
e_cell_height (ECellView *ecell_view,
               gint model_col,
               gint view_col,
               gint row)
{
	ECellClass *class;

	class = E_CELL_GET_CLASS (ecell_view->ecell);
	g_return_val_if_fail (class->height != NULL, 0);

	return class->height (ecell_view, model_col, view_col, row);
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
gpointer
e_cell_enter_edit (ECellView *ecell_view,
                   gint model_col,
                   gint view_col,
                   gint row)
{
	ECellClass *class;

	class = E_CELL_GET_CLASS (ecell_view->ecell);
	g_return_val_if_fail (class->enter_edit != NULL, NULL);

	return class->enter_edit (ecell_view, model_col, view_col, row);
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
e_cell_leave_edit (ECellView *ecell_view,
                   gint model_col,
                   gint view_col,
                   gint row,
                   gpointer edit_context)
{
	ECellClass *class;

	class = E_CELL_GET_CLASS (ecell_view->ecell);
	g_return_if_fail (class->leave_edit != NULL);

	class->leave_edit (ecell_view, model_col, view_col, row, edit_context);
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
 * Requests that the ECellView return a gpointer  representing the state
 * of the ECell.  This is primarily intended for things like selection
 * or scrolling.
 */
gpointer
e_cell_save_state (ECellView *ecell_view,
                   gint model_col,
                   gint view_col,
                   gint row,
                   gpointer edit_context)
{
	ECellClass *class;

	class = E_CELL_GET_CLASS (ecell_view->ecell);

	if (class->save_state == NULL)
		return NULL;

	return class->save_state (
		ecell_view, model_col, view_col, row, edit_context);
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
e_cell_load_state (ECellView *ecell_view,
                   gint model_col,
                   gint view_col,
                   gint row,
                   gpointer edit_context,
                   gpointer save_state)
{
	ECellClass *class;

	class = E_CELL_GET_CLASS (ecell_view->ecell);

	if (class->load_state != NULL)
		class->load_state (
			ecell_view, model_col, view_col,
			row, edit_context, save_state);
}

/**
 * e_cell_free_state:
 * @ecell_view: the ECellView
 * @model_col: the column in the model
 * @view_col: the column in the view
 * @row: the row
 * @save_state: the save state to free
 *
 * Requests that the ECellView free the given save state.
 */
void
e_cell_free_state (ECellView *ecell_view,
                   gint model_col,
                   gint view_col,
                   gint row,
                   gpointer save_state)
{
	ECellClass *class;

	class = E_CELL_GET_CLASS (ecell_view->ecell);

	if (class->free_state != NULL)
		class->free_state (
			ecell_view, model_col, view_col, row, save_state);
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
gint
e_cell_max_width (ECellView *ecell_view,
                  gint model_col,
                  gint view_col)
{
	ECellClass *class;

	class = E_CELL_GET_CLASS (ecell_view->ecell);
	g_return_val_if_fail (class->max_width != NULL, 0);

	return class->max_width (ecell_view, model_col, view_col);
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
gint
e_cell_max_width_by_row (ECellView *ecell_view,
                         gint model_col,
                         gint view_col,
                         gint row)
{
	ECellClass *class;

	class = E_CELL_GET_CLASS (ecell_view->ecell);

	if (class->max_width_by_row == NULL)
		return e_cell_max_width (ecell_view, model_col, view_col);

	return class->max_width_by_row (ecell_view, model_col, view_col, row);
}

/**
 * e_cell_max_width_by_row_implemented:
 * @ecell_view: the ECellView that we are curious about
 *
 * Returns: the maximum width for the ECellview at @model_col which
 * is being rendered as @view_col for the data in @row.
 */
gboolean
e_cell_max_width_by_row_implemented (ECellView *ecell_view)
{
	ECellClass *class;

	class = E_CELL_GET_CLASS (ecell_view->ecell);

	return (class->max_width_by_row != NULL);
}

gchar *
e_cell_get_bg_color (ECellView *ecell_view,
                     gint row)
{
	ECellClass *class;

	class = E_CELL_GET_CLASS (ecell_view->ecell);

	if (class->get_bg_color == NULL)
		return NULL;

	return class->get_bg_color (ecell_view, row);
}

void
e_cell_style_updated (ECellView *ecell_view)
{
	ECellClass *class;

	class = E_CELL_GET_CLASS (ecell_view->ecell);

	if (class->style_updated != NULL)
		class->style_updated (ecell_view);
}

