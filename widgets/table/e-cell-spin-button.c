/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-cell-spin-button.c: Spin button item for e-table.
 * Copyright 2001, CodeFactory AB
 * Copyright 2001, Mikael Hallendal <micke@codefactory.se>
 *
 * Authors:
 *   Mikael Hallendal <micke@codefactory.se>
 *
 * Celltype for drawing a spinbutton in a cell. 
 *
 * Used ECellPopup by Damon Chaplin <damon@ximian.com> as base for 
 * buttondrawings.
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

#include <gtk/gtk.h>

#include "gal/e-table/e-cell-float.h"
#include "gal/e-table/e-cell-number.h"
#include "gal/e-table/e-table-item.h"
#include "gal/e-table/e-table-model.h"
#include "gal/util/e-util.h"

#include "e-cell-spin-button.h"

#define E_CELL_SPIN_BUTTON_ARROW_WIDTH  16
#define PARENT_TYPE e_cell_get_type ()

static void e_cell_spin_button_class_init   (GObjectClass   *klass);
static void e_cell_spin_button_init         (GtkObject        *object);

static void         ecsb_dispose      (GObject	*object);

/* ECell Functions */
static ECellView *  ecsb_new_view     (ECell                 *ecell,
				       ETableModel           *etm,
				       void                  *eti_view);
static void         ecsb_realize      (ECellView             *ecv);
static void         ecsb_kill_view    (ECellView             *ecv);
static void         ecsb_unrealize    (ECellView             *ecv);
static void         ecsb_draw         (ECellView             *ecv, 
				       GdkDrawable           *drawable, 
				       int                    model_col, 
				       int                    view_col, 
				       int                    row, 
				       ECellFlags             flags, 
				       int                    x1, 
				       int                    y1, 
				       int                    x2, 
				       int                    y2); 

static gint         ecsb_event        (ECellView             *ecv, 
				       GdkEvent              *event, 
				       int                    model_col, 
				       int                    view_col, 
				       int                    row,
				       ECellFlags             flags, 
				       ECellActions          *actions); 

static gint         ecsb_height       (ECellView             *ecv,
				       int                    model_col,
				       int                    view_col,
				       int                    row);

static void *       ecsb_enter_edit   (ECellView             *ecv,
				       int                    model_col, 
				       int                    view_col, 
				       int                    row);

static void         ecsb_leave_edit   (ECellView             *ecv,
				       int                    model_col, 
				       int                    view_col, 
				       int                    row, 
				       void                  *context);
static void         ecsb_focus        (ECellView             *ecell_view,
				       int                    model_col,
				       int                    view_col,
				       int                    row,
				       int                    x1,
				       int                    y1,
				       int                    x2,
				       int                    y2);
static void         ecsb_unfocus      (ECellView             *ecell_view);

static void         ecsb_show_tooltip (ECellView             *ecv,
				       int                    model_col, 
				       int                    view_col, 
				       int                    row, 
				       int                    col_width, 
				       ETableTooltip          *tooltip);

typedef struct {
	ECellView    cell_view;
	
	ECellView   *child_view;
} ECellSpinButtonView;

enum {
	STEP,
	LAST_SIGNAL
};

static guint    signals[LAST_SIGNAL] = { 0 };
static ECell   *parent_class;

static void 
e_cell_spin_button_class_init     (GObjectClass   *klass)
{
        ECellClass             *ecc    = (ECellClass *) klass;  
	ECellSpinButtonClass   *ecsbc  = (ECellSpinButtonClass *) klass;
        
        klass->dispose     = ecsb_dispose;

	ecc->realize       = ecsb_realize;
	ecc->unrealize     = ecsb_unrealize;
	ecc->new_view      = ecsb_new_view;
	ecc->kill_view     = ecsb_kill_view;
	ecc->draw          = ecsb_draw;
	ecc->event         = ecsb_event;
	ecc->height        = ecsb_height;
	ecc->enter_edit    = ecsb_enter_edit;
	ecc->leave_edit    = ecsb_leave_edit;
	ecc->focus         = ecsb_focus;
	ecc->unfocus       = ecsb_unfocus;	ecc->print         = NULL;
	ecc->print_height  = NULL;
	ecc->max_width     = NULL;
	ecc->show_tooltip  = ecsb_show_tooltip;

	ecsbc->step        = NULL;

        parent_class       = g_type_class_ref (E_CELL_TYPE);

	signals[STEP] =
		g_signal_new ("step",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECellSpinButtonClass, step),
			      NULL, NULL,
			      e_marshal_NONE__POINTER_INT_INT_INT,
			      G_TYPE_NONE,
			      4, G_TYPE_POINTER, G_TYPE_INT, 
			      G_TYPE_INT, G_TYPE_INT);
}

static void 
e_cell_spin_button_init           (GtkObject        *object)
{
        ECellSpinButton   *ecsb;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (M_IS_CELL_SPIN_BUTTON (object));
	
	ecsb = E_CELL_SPIN_BUTTON (object);

	ecsb->up_pressed    = FALSE;
	ecsb->down_pressed  = FALSE;
}

static ECellView *  
ecsb_new_view     (ECell            *ecell,
		   ETableModel      *etm,
		   void             *eti_view)
{
	ECellSpinButton       *ecsb = E_CELL_SPIN_BUTTON (ecell);
	ECellSpinButtonView   *ecsb_view;

	g_return_val_if_fail (ecsb->child != NULL, NULL);

	ecsb_view = g_new0 (ECellSpinButtonView, 1);

	ecsb_view->cell_view.ecell = ecell;
	ecsb_view->cell_view.e_table_model = etm;
	ecsb_view->cell_view.e_table_item_view = eti_view;

	ecsb_view->child_view = e_cell_new_view (ecsb->child, etm, eti_view);

	return (ECellView *) ecsb_view;
}

static void         
ecsb_realize      (ECellView        *ecv)
{ 
	ECellSpinButtonView   *ecsb_view;
	
	g_return_if_fail (ecv != NULL);

	ecsb_view = (ECellSpinButtonView *) ecv;

	e_cell_realize (ecsb_view->child_view);
} 

static void         
ecsb_kill_view    (ECellView        *ecv)
{
	ECellSpinButtonView   *ecsb_view;
	
	g_return_if_fail (ecv != NULL);

	ecsb_view = (ECellSpinButtonView *) ecv;

	if (ecsb_view->child_view) {
		e_cell_kill_view (ecsb_view->child_view);
	}
	
	g_free (ecsb_view);
}

static void         
ecsb_unrealize    (ECellView        *ecv)
{ 
	ECellSpinButtonView   *ecsb_view;
	
	g_return_if_fail (ecv != NULL);

	ecsb_view = (ECellSpinButtonView *) ecv;

	e_cell_unrealize (ecsb_view->child_view);
}

static void         
ecsb_draw         (ECellView        *ecv, 
		   GdkDrawable      *drawable, 
		   int               model_col, 
		   int               view_col, 
		   int               row, 
		   ECellFlags        flags, 
		   int               x1, 
		   int               y1, 
		   int               x2, 
		   int               y2)
{
	ECellSpinButton        *ecsb;
	ECellSpinButtonView    *ecsb_view;
	ETableItem             *eti;
	GtkWidget              *canvas;
	GtkShadowType           shadow = GTK_SHADOW_OUT;
	GdkRectangle            rect;
	
	g_return_if_fail (ecv != NULL);
	
	ecsb_view   = (ECellSpinButtonView *) ecv;
	ecsb        = E_CELL_SPIN_BUTTON (ecsb_view->cell_view.ecell);
	
	eti         = E_TABLE_ITEM (ecsb_view->cell_view.e_table_item_view);
	canvas      = GTK_WIDGET (GNOME_CANVAS_ITEM (eti)->canvas);
	
	if (eti->editing_col == view_col && 
	    eti->editing_row == row) {

		/* Draw child (Whats shown under the buttons) */
		e_cell_draw (ecsb_view->child_view, 
			     drawable, model_col, view_col,
			     row, flags, 
			     x1, y1, 
			     x2 - E_CELL_SPIN_BUTTON_ARROW_WIDTH, y2); 

		/* Draw down-arrow */
		rect.x       = x2 - E_CELL_SPIN_BUTTON_ARROW_WIDTH;
		rect.y       = y1 + (y2 - y1) / 2;
		rect.width   = E_CELL_SPIN_BUTTON_ARROW_WIDTH;
		rect.height  = (y2 - y1) / 2;
		
		if (ecsb->down_pressed) {
			shadow = GTK_SHADOW_IN;
		} else {
			shadow = GTK_SHADOW_OUT;
		}
		
		gtk_paint_box (canvas->style, drawable, 
			       GTK_STATE_NORMAL, shadow,
			       &rect, canvas, "ecellspinbutton_down",
			       rect.x, rect.y, rect.width, rect.height);

		gtk_paint_arrow (canvas->style, drawable,
				 GTK_STATE_NORMAL, GTK_SHADOW_NONE,
				 &rect, canvas, NULL,
				 GTK_ARROW_DOWN, TRUE,
				 rect.x,
				 rect.y,
				 rect.width,
				 rect.height);

		/* Draw up-arrow */
		rect.y       = y1;
		
		if (ecsb->up_pressed) {
			shadow = GTK_SHADOW_IN;
		} else {
			shadow = GTK_SHADOW_OUT;
		}

		gtk_paint_box (canvas->style, drawable, 
			       GTK_STATE_NORMAL, shadow,
			       &rect, canvas, "ecellspinbutton_up",
			       rect.x, rect.y, rect.width, rect.height);

		gtk_paint_arrow (canvas->style, drawable,
				 GTK_STATE_NORMAL, GTK_SHADOW_NONE,
				 &rect, canvas, NULL,
				 GTK_ARROW_UP, TRUE,
				 rect.x,
				 rect.y,
				 rect.width,
				 rect.height);
	} else {
		/* Draw child */
		e_cell_draw (ecsb_view->child_view, 
			     drawable, model_col, view_col,
			     row, flags, 
			     x1, y1, 
			     x2, y2); 
	}
}

static gint         
ecsb_event        (ECellView        *ecv, 
		   GdkEvent         *event, 
		   int               model_col, 
		   int               view_col, 
		   int               row,
		   ECellFlags        flags, 
		   ECellActions     *actions)
{
	ECellSpinButton       *ecsb;
	ECellSpinButtonClass  *ecsb_class;
	ECellSpinButtonView   *ecsb_view;
	ETableItem            *eti;
	gint                   height, width;
	
	g_return_val_if_fail (ecv != NULL, FALSE);
		
	ecsb_view   = (ECellSpinButtonView *) ecv;
	ecsb        = E_CELL_SPIN_BUTTON (ecsb_view->cell_view.ecell);
	ecsb_class  = E_CELL_SPIN_BUTTON_CLASS (GTK_OBJECT_GET_CLASS (ecsb));
	eti         = E_TABLE_ITEM (ecsb_view->cell_view.e_table_item_view);
	
	switch (event->type) {
	case GDK_BUTTON_PRESS:
		if (eti->editing_col == view_col &&
		    eti->editing_row == row) {
			width = e_table_header_col_diff (eti->header,
							 view_col,
							 view_col + 1);
			height = e_table_item_row_diff (eti, row, row + 1);

			/* Check if inside a button */
			if (event->button.x >= width - E_CELL_SPIN_BUTTON_ARROW_WIDTH) {
				/* Yep, which one? */
				if (event->button.y <= height / 2) {
					ecsb->up_pressed = TRUE;
					g_signal_emit (ecsb,
						       signals[STEP], 0,
						       ecv,
						       STEP_UP,
						       view_col,
						       row);
				} else {
					ecsb->down_pressed = TRUE;
					g_signal_emit (ecsb,
						       signals[STEP], 0,
						       ecv,
						       STEP_DOWN,
						       view_col,
						       row);
				}

				e_table_item_redraw_range (eti,
							   view_col,
							   row,
							   view_col,
							   row);
				
			}
		} 
		
		break;
	case GDK_BUTTON_RELEASE:
		ecsb->up_pressed = FALSE;
		ecsb->down_pressed = FALSE;
		e_table_item_redraw_range (eti,
					   view_col,
					   row,
					   view_col,
					   row);
		break;
	case GDK_KEY_PRESS:
		break;
	default:
		break;
	}

	return e_cell_event (ecsb_view->child_view, event, model_col,
			     view_col, row, flags, actions);
}

static gint         
ecsb_height       (ECellView        *ecv,
		   int               model_col,
		   int               view_col,
		   int               row)
{
	ECellSpinButtonView   *ecsb_view;
	
	g_return_val_if_fail (ecv != NULL, -1);
	
	ecsb_view = (ECellSpinButtonView *) ecv;

	return e_cell_height (ecsb_view->child_view, model_col, view_col, row);
}

static void *       
ecsb_enter_edit   (ECellView        *ecv,
		   int               model_col, 
		   int               view_col, 
		   int               row)
{
	ECellSpinButtonView   *ecsb_view;
	
	g_return_val_if_fail (ecv != NULL, NULL);

	ecsb_view = (ECellSpinButtonView *) ecv;

	return e_cell_enter_edit (ecsb_view->child_view, model_col,
				  view_col, row);
}


static void         
ecsb_leave_edit   (ECellView        *ecv,
		   int               model_col, 
		   int               view_col, 
		   int               row, 
		   void             *context)
{
	ECellSpinButtonView   *ecsb_view;
	
	g_return_if_fail (ecv != NULL);
	
	ecsb_view = (ECellSpinButtonView *) ecv;

	e_cell_leave_edit (ecsb_view->child_view, model_col, view_col, 
			   row, context);
}

static void
ecsb_focus        (ECellView        *ecell_view,
		   int               model_col,
		   int               view_col,
		   int               row,
		   int		     x1,
		   int		     y1,
		   int		     x2,
		   int		     y2)
{
	ECellClass  *klass;
	ECellSpinButtonView   *ecsb_view;
	
	ecsb_view = (ECellSpinButtonView *) ecell_view;

	klass = E_CELL_GET_CLASS (ecell_view->ecell);

	if (klass->focus)
		klass->focus (ecell_view, model_col, view_col, row, 
			      x1, y1, x2, y2);
}

static void
ecsb_unfocus      (ECellView        *ecell_view)
{
	ECellClass  *klass;
	ECellSpinButtonView   *ecsb_view;
	
	ecsb_view = (ECellSpinButtonView *) ecell_view;
	klass = E_CELL_GET_CLASS (ecell_view->ecell);

	if (klass->unfocus)
		klass->unfocus (ecell_view);
}

static void         
ecsb_show_tooltip (ECellView        *ecv,
		   int               model_col, 
		   int               view_col, 
		   int               row, 
		   int               col_width, 
		   ETableTooltip    *tooltip)
{
	ECellSpinButtonView   *ecsb_view;
	
	g_return_if_fail (ecv != NULL);

	ecsb_view = (ECellSpinButtonView *) ecv;
	
	e_cell_show_tooltip (ecsb_view->child_view, model_col, view_col, 
			     row, col_width, tooltip);
}

static void 
ecsb_dispose (GObject	*object)
{
	ECellSpinButton *mcsp;

	g_return_if_fail (object != NULL);
	g_return_if_fail (M_IS_CELL_SPIN_BUTTON (object));
	
	mcsp = E_CELL_SPIN_BUTTON (object);
	
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

ECell *
e_cell_spin_button_new (gint     min,
			gint     max,
			gint     step,
			ECell   *child_cell)
{
	ECellSpinButton   *ecsb;
	
	ecsb = g_object_new (E_CELL_SPIN_BUTTON_TYPE, NULL);

	if (!child_cell) {
		child_cell = e_cell_number_new (NULL, 
						GTK_JUSTIFY_LEFT);
		
		g_signal_connect (ecsb, "step",
				  G_CALLBACK (e_cell_spin_button_step),
				  NULL);
	}
	
	ecsb->child   = child_cell;
	ecsb->min.i   = min;
	ecsb->max.i   = max;
	ecsb->step.i  = step;

	return E_CELL (ecsb);
}

ECell *
e_cell_spin_button_new_float (gfloat    min,
			      gfloat    max,
			      gfloat    step,
			      ECell    *child_cell)
{
	ECellSpinButton   *ecsb;
	
	ecsb = g_object_new (E_CELL_SPIN_BUTTON_TYPE, NULL);

	if (!child_cell) {
		child_cell = e_cell_float_new (NULL, GTK_JUSTIFY_LEFT);
		g_signal_connect (ecsb, "step",
				  G_CALLBACK (e_cell_spin_button_step_float),
				  NULL);
	}
	
	ecsb->child   = child_cell;
	ecsb->min.f   = min;
	ecsb->max.f   = max;
	ecsb->step.f  = step;

	return E_CELL (ecsb);
}

void
e_cell_spin_button_step   (ECellSpinButton       *ecsb,
			   ECellView             *ecv,
			   ECellSpinButtonStep    direction,
			   gint                   col,
			   gint                   row)
{
 	ECellSpinButtonView   *ecsb_view; 
	
	ETableModel           *etm;
	gint                   value;
	gint                   new_value;
	gchar                 *str_value;
	
	g_return_if_fail (ecsb != NULL);
	g_return_if_fail (M_IS_CELL_SPIN_BUTTON (ecsb));
	g_return_if_fail (ecv != NULL);
	
 	ecsb_view  = (ECellSpinButtonView *) ecv; 
	etm        = ecsb_view->cell_view.e_table_model;
	
	value  = GPOINTER_TO_INT (e_table_model_value_at (etm, col, row));
	new_value = value;
	
	switch (direction) {
	case STEP_UP:
		new_value = CLAMP (value + ecsb->step.i, 
				   ecsb->min.i, ecsb->max.i);
		break;
	case STEP_DOWN:
		new_value = CLAMP (value - ecsb->step.i, 
				   ecsb->min.i, ecsb->max.i);
		break;
	default:
		break;
	};

	str_value = g_strdup_printf ("%d", new_value);

	e_table_model_set_value_at (etm, col, row, str_value);

	g_free (str_value);
}

void
e_cell_spin_button_step_float  (ECellSpinButton       *ecsb,
				ECellView             *ecv,
				ECellSpinButtonStep    direction,
				gint                   col,
				gint                   row)
{
 	ECellSpinButtonView   *ecsb_view; 
	
	ETableModel           *etm;
	gfloat                 value;
	gfloat                 new_value;
	gchar                 *str_value;
	
	g_return_if_fail (ecsb != NULL);
	g_return_if_fail (M_IS_CELL_SPIN_BUTTON (ecsb));
	g_return_if_fail (ecv != NULL);
	
 	ecsb_view  = (ECellSpinButtonView *) ecv; 
	etm        = ecsb_view->cell_view.e_table_model;
	
	value  = *(gfloat *) e_table_model_value_at (etm, col, row);

	switch (direction) {
	case STEP_UP:
		new_value = CLAMP (value + ecsb->step.f, 
				   ecsb->min.f, ecsb->max.f);
		break;
	case STEP_DOWN:
		new_value = CLAMP (value - ecsb->step.f, 
				   ecsb->min.f, ecsb->max.f);
		break;
	default:
		new_value = value;
		break;
	};
	
	str_value = g_strdup_printf ("%f", new_value);

	e_table_model_set_value_at (etm, col, row, str_value);
	
	g_free (str_value);
}

E_MAKE_TYPE (e_cell_spin_button, "ECellSpinButton", ECellSpinButton, 
	     e_cell_spin_button_class_init, e_cell_spin_button_init, 
	     PARENT_TYPE)

