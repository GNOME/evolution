/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * ECompletionView - A text completion selection widget
 * Copyright (C) 2000, 2001 Ximian, Inc.
 *
 * Author: Jon Trowbridge <trow@ximian.com>
 * Adapted from code by Miguel de Icaza <miguel@ximian.com>
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


#include <config.h>
#include <math.h>
#include <gdk/gdkkeysyms.h>
#include <gal/e-table/e-table-simple.h>
#include <gal/e-table/e-table-scrolled.h>
#include "e-completion-view.h"

enum {
	E_COMPLETION_VIEW_NONEMPTY,
	E_COMPLETION_VIEW_ADDED,
	E_COMPLETION_VIEW_FULL,
	E_COMPLETION_VIEW_BROWSE,
	E_COMPLETION_VIEW_UNBROWSE,
	E_COMPLETION_VIEW_ACTIVATE,
	E_COMPLETION_VIEW_LAST_SIGNAL
};

static guint e_completion_view_signals[E_COMPLETION_VIEW_LAST_SIGNAL] = { 0 };

static void    e_completion_view_disconnect     (ECompletionView *cv);
static ETable *e_completion_view_table          (ECompletionView *cv);
static void    e_completion_view_clear_choices  (ECompletionView *cv);
static void    e_completion_view_set_cursor_row (ECompletionView *cv, gint r);
static void    e_completion_view_select         (ECompletionView *cv, gint r);

static gint    e_completion_view_key_press_handler (GtkWidget *w, GdkEventKey *key_event, gpointer user_data);

static void    e_completion_view_class_init (ECompletionViewClass *klass);
static void    e_completion_view_init       (ECompletionView *completion);
static void    e_completion_view_destroy    (GtkObject *object);

static GtkObjectClass *parent_class;



static gint
e_completion_view_local_key_press_handler (GtkWidget *w, GdkEventKey *ev)
{
	return e_completion_view_key_press_handler (w, ev, w);
}

static void
e_completion_view_paint (GtkWidget *widget, GdkRectangle *area)
{
	gint i;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (E_IS_COMPLETION_VIEW (widget));
	g_return_if_fail (area != NULL);

	if (!GTK_WIDGET_DRAWABLE (widget))
		return;

	for (i = 0; i < E_COMPLETION_VIEW (widget)->border_width; ++i) {

		gdk_draw_rectangle (widget->window,
				    widget->style->black_gc,
				    FALSE, i, i, 
				    widget->allocation.width-1-2*i,
				    widget->allocation.height-1-2*i);

	}
	
}

static void
e_completion_view_draw (GtkWidget *widget, GdkRectangle *area)
{
	GtkBin *bin;
	GdkRectangle child_area;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (E_IS_COMPLETION_VIEW (widget));
	g_return_if_fail (area != NULL);

	if (GTK_WIDGET_DRAWABLE (widget)) {
		bin = GTK_BIN (widget);

		e_completion_view_paint (widget, area);

		if (bin->child && gtk_widget_intersect (bin->child, area, &child_area))
			gtk_widget_draw (bin->child, &child_area);
	}
}

static gint
e_completion_view_expose_event (GtkWidget *widget, GdkEventExpose *event)			  
{
	GtkBin *bin;
	GdkEventExpose child_event;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (E_IS_COMPLETION_VIEW (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	if (GTK_WIDGET_DRAWABLE (widget)) {
		bin = GTK_BIN (widget);

		e_completion_view_paint (widget, &event->area);

		child_event = *event;
		if (bin->child &&
		    GTK_WIDGET_NO_WINDOW (bin->child) &&
		    gtk_widget_intersect (bin->child, &event->area, &child_event.area))
			gtk_widget_event (bin->child, (GdkEvent*) &child_event);
	}

	return FALSE;
}

static void
e_completion_view_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	GtkBin *bin;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (E_IS_COMPLETION_VIEW (widget));
	g_return_if_fail (requisition != NULL);

	bin = GTK_BIN (widget);
	
	requisition->width  = 2 * E_COMPLETION_VIEW (widget)->border_width;
	requisition->height = 2 * E_COMPLETION_VIEW (widget)->border_width;

	if (bin->child && GTK_WIDGET_VISIBLE (bin->child)) {
		GtkRequisition child_requisition;
		
		gtk_widget_size_request (bin->child, &child_requisition);
		
		requisition->width += child_requisition.width;
		requisition->height += child_requisition.height;
	}
}

static void
e_completion_view_size_allocate (GtkWidget *widget, GtkAllocation *allocation)				 
{
	GtkBin *bin;
	GtkAllocation child_allocation;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (E_IS_COMPLETION_VIEW (widget));
	g_return_if_fail (allocation != NULL);

	bin = GTK_BIN (widget);
	widget->allocation = *allocation;

	child_allocation.x = E_COMPLETION_VIEW (widget)->border_width;
	child_allocation.width = MAX(0, (gint)allocation->width - child_allocation.x * 2);

	child_allocation.y = E_COMPLETION_VIEW (widget)->border_width;
	child_allocation.height = MAX (0, (gint)allocation->height - child_allocation.y * 2);

	if (GTK_WIDGET_REALIZED (widget)) {
		gdk_window_move_resize (widget->window,
					allocation->x,
					allocation->y,
					allocation->width,
					allocation->height);
	}
	
	if (bin->child) {
		gtk_widget_size_allocate (bin->child, &child_allocation);
	}
}

GtkType
e_completion_view_get_type (void)
{
	static GtkType completion_view_type = 0;
  
	if (!completion_view_type) {
		GtkTypeInfo completion_view_info = {
			"ECompletionView",
			sizeof (ECompletionView),
			sizeof (ECompletionViewClass),
			(GtkClassInitFunc) e_completion_view_class_init,
			(GtkObjectInitFunc) e_completion_view_init,
			NULL, NULL, /* reserved */
			(GtkClassInitFunc) NULL
		};

		completion_view_type = gtk_type_unique (gtk_event_box_get_type (), &completion_view_info);
	}

	return completion_view_type;
}

static void
e_completion_view_class_init (ECompletionViewClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	parent_class = GTK_OBJECT_CLASS (gtk_type_class (gtk_event_box_get_type ()));

	e_completion_view_signals[E_COMPLETION_VIEW_NONEMPTY] =
		gtk_signal_new ("nonempty",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ECompletionViewClass, nonempty),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	e_completion_view_signals[E_COMPLETION_VIEW_ADDED] =
		gtk_signal_new ("added",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ECompletionViewClass, added),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	e_completion_view_signals[E_COMPLETION_VIEW_FULL] =
		gtk_signal_new ("full",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ECompletionViewClass, full),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	e_completion_view_signals[E_COMPLETION_VIEW_BROWSE] =
		gtk_signal_new ("browse",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ECompletionViewClass, browse),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);

	e_completion_view_signals[E_COMPLETION_VIEW_UNBROWSE] =
		gtk_signal_new ("unbrowse",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ECompletionViewClass, unbrowse),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	e_completion_view_signals[E_COMPLETION_VIEW_ACTIVATE] =
		gtk_signal_new ("activate",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ECompletionViewClass, activate),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, e_completion_view_signals, E_COMPLETION_VIEW_LAST_SIGNAL);

	object_class->destroy = e_completion_view_destroy;

	widget_class->key_press_event = e_completion_view_local_key_press_handler;
	widget_class->draw = e_completion_view_draw;
	widget_class->expose_event = e_completion_view_expose_event;
	widget_class->size_request = e_completion_view_size_request;
	widget_class->size_allocate = e_completion_view_size_allocate;
}

static void
e_completion_view_init (ECompletionView *completion)
{
	completion->border_width = 2;
}

static void
e_completion_view_destroy (GtkObject *object)
{
	ECompletionView *cv = E_COMPLETION_VIEW (object);

	e_completion_view_disconnect (cv);
	e_completion_view_clear_choices (cv);

	if (cv->key_widget) {
		gtk_signal_disconnect (GTK_OBJECT (cv->key_widget), cv->key_signal_id);
		gtk_object_unref (GTK_OBJECT (cv->key_widget));
	}

	if (cv->completion)
		gtk_object_unref (GTK_OBJECT (cv->completion));


	if (parent_class->destroy)
		(parent_class->destroy) (object);
}

static void
e_completion_view_disconnect (ECompletionView *cv)
{
	g_return_if_fail (cv != NULL);
	g_return_if_fail (E_IS_COMPLETION_VIEW (cv));

	if (cv->begin_signal_id)
		gtk_signal_disconnect (GTK_OBJECT (cv->completion), cv->begin_signal_id);
	if (cv->comp_signal_id)
		gtk_signal_disconnect (GTK_OBJECT (cv->completion), cv->comp_signal_id);
	if (cv->restart_signal_id)
		gtk_signal_disconnect (GTK_OBJECT (cv->completion), cv->restart_signal_id);
	if (cv->cancel_signal_id)
		gtk_signal_disconnect (GTK_OBJECT (cv->completion), cv->cancel_signal_id);
	if (cv->end_signal_id)
		gtk_signal_disconnect (GTK_OBJECT (cv->completion), cv->end_signal_id);
	
	cv->begin_signal_id   = 0;
	cv->comp_signal_id    = 0;
	cv->restart_signal_id = 0;
	cv->end_signal_id     = 0;
}

static ETable *
e_completion_view_table (ECompletionView *cv)
{
	return e_table_scrolled_get_table (E_TABLE_SCROLLED (cv->table));
}

static void
e_completion_view_clear_choices (ECompletionView *cv)
{
	GList *i;

	g_return_if_fail (cv != NULL);
	g_return_if_fail (E_IS_COMPLETION_VIEW (cv));

	for (i = cv->choices; i != NULL; i = g_list_next (i)) {
		e_completion_match_unref ((ECompletionMatch *) i->data);
	}

	g_list_free (cv->choices);
	cv->choices = NULL;

	cv->choice_count = 0;
}

static void
e_completion_view_set_cursor_row (ECompletionView *cv, gint r)
{
	ETable *table;
	GtkAdjustment *adj;
	gint x, y1, y2, r1, r2, c;
	double fracline;
	gint iteration_count=0;

	g_return_if_fail (cv != NULL);
	g_return_if_fail (E_IS_COMPLETION_VIEW (cv));
	g_return_if_fail (r < cv->choice_count);

	adj = e_scroll_frame_get_vadjustment (E_SCROLL_FRAME (cv->table));	

	table = e_completion_view_table (cv);

	if (r < 0) {
		e_selection_model_clear (E_SELECTION_MODEL(table->selection));
		
		/* Move back to the top when we clear the selection */
		gtk_adjustment_set_value (adj, adj->lower);
		return;
	}

	e_table_set_cursor_row (table, r);

	/* OK, now the tricky bit.  We try to insure that this row is
	   visible. */
	
	/* If we are selecting the first or last row, then it is easy.  We just
	   cram the vadjustment all the way up/down. */
	if (r == 0) {
		gtk_adjustment_set_value (adj, adj->lower);
		return;
	} else if (r == cv->choice_count - 1) {		
		gtk_adjustment_set_value (adj, adj->upper - adj->page_size);
		return;
	}
	
	fracline = ((adj->upper - adj->lower - adj->page_size) / cv->choice_count) / 4;

	while (iteration_count < 100) {
		x = GTK_LAYOUT(table->table_canvas)->hadjustment->value;
		y1 = GTK_LAYOUT(table->table_canvas)->vadjustment->value;
		
		y2 = y1 + cv->table->allocation.height;
		
		e_table_group_compute_location (e_completion_view_table (cv)->group, &x, &y1, &r1, &c);
		e_table_group_compute_location (e_completion_view_table (cv)->group, &x, &y2, &r2, &c);
	
		if (r <= r1) {
			gtk_adjustment_set_value (adj, adj->value - fracline);
		} else if (r >= r2) {
			gtk_adjustment_set_value (adj, adj->value + fracline);
		} else 
			return;

		++iteration_count;
	}

	g_assert_not_reached ();
}

static void
e_completion_view_select (ECompletionView *cv, gint r)
{
	ECompletionMatch *match = (ECompletionMatch *) g_list_nth_data (cv->choices, r);

	cv->selection = r;
	e_completion_view_set_cursor_row (cv, r);
	gtk_signal_emit (GTK_OBJECT (cv), e_completion_view_signals[E_COMPLETION_VIEW_ACTIVATE], match);
}

static gint
e_completion_view_key_press_handler (GtkWidget *w, GdkEventKey *key_event, gpointer user_data)
{
	ECompletionView *cv = E_COMPLETION_VIEW (user_data);
	gint dir = 0;
	gboolean key_handled = TRUE;
	
	/* Start up a completion.*/
	if (cv->complete_key && key_event->keyval == cv->complete_key && !cv->editable) {
		gtk_signal_emit (GTK_OBJECT (cv), e_completion_view_signals[E_COMPLETION_VIEW_BROWSE], NULL);
		goto stop_emission;
	}

	/* Stop our completion. */
	if (cv->uncomplete_key && key_event->keyval == cv->uncomplete_key && cv->editable && cv->selection < 0) {
		e_completion_view_set_cursor_row (cv, -1);
		gtk_signal_emit (GTK_OBJECT (cv), e_completion_view_signals[E_COMPLETION_VIEW_UNBROWSE]);
		goto stop_emission;
	}

	if (!cv->editable)
		return FALSE;

	switch (key_event->keyval) {
	case GDK_Down:
	case GDK_KP_Down:
		dir = 1;
		break;

	case GDK_Up:
	case GDK_KP_Up:
		dir = -1;
		break;
		
	case GDK_Tab:
		/* Unbrowse, unhandled. */
		cv->selection = -1;
		dir = 0;
		key_handled = FALSE;
		break;

	case GDK_Return:
	case GDK_KP_Enter:
	case GDK_space:
	case GDK_KP_Space:
		/* Only handle these key presses if we have an active selection;
		   otherwise, pass them on. */
		if (cv->selection >= 0) {
			e_completion_view_select (cv, cv->selection);
			goto stop_emission;
		}
		return FALSE;

	case GDK_Escape:
		/* Unbrowse hack */
		cv->selection = -1;
		dir = 0;
		break;

	default:
		return FALSE;
	}

	cv->selection += dir;

	if (cv->selection >= cv->choice_count) {
		cv->selection = cv->choice_count - 1;
		/* Don't re-emit the browse signal */
		goto stop_emission;
	}

	e_completion_view_set_cursor_row (cv, cv->selection);

	if (cv->selection >= 0)
		gtk_signal_emit (GTK_OBJECT (cv), e_completion_view_signals[E_COMPLETION_VIEW_BROWSE], 
				 g_list_nth_data (cv->choices, cv->selection));
	else
		gtk_signal_emit (GTK_OBJECT (cv), e_completion_view_signals[E_COMPLETION_VIEW_UNBROWSE]);

 stop_emission:
	
	if (key_handled)
		gtk_signal_emit_stop_by_name (GTK_OBJECT (w), "key_press_event");

	return key_handled;
}

static void
begin_completion_cb (ECompletion *completion, const gchar *txt, gint pos, gint limit, gpointer user_data)
{
	ECompletionView *cv = E_COMPLETION_VIEW (user_data);

	e_completion_view_clear_choices (cv);
	cv->have_all_choices = FALSE;

	e_table_model_changed (cv->model);
}

static void
restart_completion_cb (ECompletion *completion, gpointer user_data)
{
	/* For now, handle restarts like the beginning of a new completion. */
	begin_completion_cb (completion, NULL, 0, 0, user_data);
}

static void
cancel_completion_cb (ECompletion *completion, gpointer user_data)
{
	ECompletionView *cv = E_COMPLETION_VIEW (user_data);

	/* On a cancel, clear our choices and issue an "unbrowse" signal. */
	e_completion_view_clear_choices (cv);
	cv->have_all_choices = TRUE;
	e_completion_view_set_cursor_row (cv, -1);
	e_table_model_changed (cv->model);

	gtk_signal_emit (GTK_OBJECT (cv), e_completion_view_signals[E_COMPLETION_VIEW_UNBROWSE]);
}

static void
completion_cb (ECompletion *completion, ECompletionMatch *match, gpointer user_data)
{
	ECompletionView *cv = E_COMPLETION_VIEW (user_data);
	gint r = cv->choice_count;
	gboolean first = (cv->choices == NULL);

	cv->choices = g_list_append (cv->choices, match);
	e_completion_match_ref (match);
	++cv->choice_count;

	e_table_model_row_inserted (cv->model, r);

	if (first)
		gtk_signal_emit (GTK_OBJECT (cv), e_completion_view_signals[E_COMPLETION_VIEW_NONEMPTY]);

	gtk_signal_emit (GTK_OBJECT (cv), e_completion_view_signals[E_COMPLETION_VIEW_ADDED]);
}

static void
end_completion_cb (ECompletion *completion, gpointer user_data)
{
	ECompletionView *cv = E_COMPLETION_VIEW (user_data);

	/* Do a final refresh of the table. */
	e_table_model_changed (cv->model);

	cv->have_all_choices = TRUE;
	gtk_signal_emit (GTK_OBJECT (cv), e_completion_view_signals[E_COMPLETION_VIEW_FULL]);
}

/*** Table Callbacks ***/

static char *simple_spec = 
"<ETableSpecification no-headers=\"true\" draw-grid=\"false\" cursor-mode=\"line\" alternating-row-colors=\"false\">"
"  <ETableColumn model_col=\"0\" _title=\"Node\" expansion=\"1.0\" "
"         minimum_width=\"16\" resizable=\"true\" cell=\"string\" "
"         compare=\"string\"/> "
"       <ETableState>                  "
"	        <column source=\"0\"/> "
"	        <grouping></grouping>  "
"        </ETableState>                "
"</ETableSpecification>";

static gint
table_col_count (ETableModel *etm, gpointer data)
{
	return 1;
}

static gint
table_row_count (ETableModel *etm, gpointer data)
{
	ECompletionView *cv = E_COMPLETION_VIEW (data);
	return cv->choice_count;
}

static gboolean
table_is_cell_editable (ETableModel *etm, gint c, gint r, gpointer data)
{
	return FALSE;
}

static gpointer
table_value_at (ETableModel *etm, gint c, gint r, gpointer data)
{
	ECompletionView *cv = E_COMPLETION_VIEW (data);
	ECompletionMatch *match;

	match = (ECompletionMatch *) g_list_nth_data (cv->choices, r);

	return (gpointer) e_completion_match_get_menu_text (match);
}

static gchar *
table_value_to_string (ETableModel *em, gint col, gconstpointer val, gpointer data)
{
	return (gchar *) val;
}

static void
table_click_cb (ETable *et, gint r, gint c, GdkEvent *ev, gpointer data)
{
	ECompletionView *cv = E_COMPLETION_VIEW (data);

	e_completion_view_select (cv, r);
}

void
e_completion_view_construct (ECompletionView *cv, ECompletion *completion)
{
	g_return_if_fail (cv != NULL);
	g_return_if_fail (E_IS_COMPLETION_VIEW (cv));
	g_return_if_fail (completion != NULL);
	g_return_if_fail (E_IS_COMPLETION (completion));

	/* Make sure we don't call construct twice. */
	g_return_if_fail (cv->completion == NULL);

	GTK_WIDGET_SET_FLAGS (GTK_WIDGET (cv), GTK_CAN_FOCUS);

	cv->completion = completion;
	gtk_object_ref (GTK_OBJECT (completion));

	cv->begin_signal_id   = gtk_signal_connect (GTK_OBJECT (completion),
						    "begin_completion",
						    GTK_SIGNAL_FUNC (begin_completion_cb),
						    cv);
	cv->comp_signal_id    = gtk_signal_connect (GTK_OBJECT (completion),
						    "completion",
						    GTK_SIGNAL_FUNC (completion_cb),
						    cv);
	cv->restart_signal_id = gtk_signal_connect (GTK_OBJECT (completion),
						    "restart_completion",
						    GTK_SIGNAL_FUNC (restart_completion_cb),
						    cv);
	cv->cancel_signal_id  = gtk_signal_connect (GTK_OBJECT (completion),
						    "cancel_completion",
						    GTK_SIGNAL_FUNC (cancel_completion_cb),
						    cv);
	cv->end_signal_id     = gtk_signal_connect (GTK_OBJECT (completion),
						    "end_completion",
						    GTK_SIGNAL_FUNC (end_completion_cb),
						    cv);

	cv->model = e_table_simple_new (table_col_count,
					table_row_count,
					table_value_at,
					NULL,
					table_is_cell_editable,
					NULL, NULL, NULL, NULL,
					table_value_to_string,
					cv);

	cv->table = e_table_scrolled_new (cv->model, NULL, simple_spec, NULL);

	e_scroll_frame_set_shadow_type (E_SCROLL_FRAME (cv->table), GTK_SHADOW_NONE);
	e_scroll_frame_set_scrollbar_spacing (E_SCROLL_FRAME (cv->table), 0);
	e_scroll_frame_set_policy (E_SCROLL_FRAME (cv->table), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

#if 0
	frame = gtk_frame_new (NULL);

	gtk_container_add (GTK_CONTAINER (cv), frame);
	gtk_container_add (GTK_CONTAINER (frame), cv->table);
	gtk_widget_show_all (frame);
#else
	gtk_container_add (GTK_CONTAINER (cv), cv->table);
	gtk_widget_show_all (cv->table);
#endif
	gtk_signal_connect (GTK_OBJECT (e_completion_view_table (cv)),
			    "click",
			    GTK_SIGNAL_FUNC (table_click_cb),
			    cv);

	cv->selection = -1;
}

GtkWidget *
e_completion_view_new (ECompletion *completion)
{
	gpointer p;

	g_return_val_if_fail (completion != NULL, NULL);
	g_return_val_if_fail (E_IS_COMPLETION (completion), NULL);

	p = gtk_type_new (e_completion_view_get_type ());

	e_completion_view_construct (E_COMPLETION_VIEW (p), completion);

	return GTK_WIDGET (p);
}

void
e_completion_view_connect_keys (ECompletionView *cv, GtkWidget *w)
{
	g_return_if_fail (cv != NULL);
	g_return_if_fail (E_IS_COMPLETION_VIEW (cv));
	g_return_if_fail (w == NULL || GTK_IS_WIDGET (w));

	if (cv->key_widget) {
		gtk_signal_disconnect (GTK_OBJECT (cv->key_widget), cv->key_signal_id);
		gtk_object_unref (GTK_OBJECT (cv->key_widget));
	}

	if (w) {
		cv->key_widget = w;
		gtk_object_ref (GTK_OBJECT (w));

		cv->key_signal_id = gtk_signal_connect (GTK_OBJECT (w),
							"key_press_event",
							GTK_SIGNAL_FUNC (e_completion_view_key_press_handler),
							cv);
	} else {
		cv->key_widget = NULL;
		cv->key_signal_id = 0;
	}
}

void
e_completion_view_set_complete_key (ECompletionView *cv, gint keyval)
{
	g_return_if_fail (cv != NULL);
	g_return_if_fail (E_IS_COMPLETION_VIEW (cv));

	cv->complete_key = keyval;
}

void
e_completion_view_set_uncomplete_key (ECompletionView *cv, gint keyval)
{
	g_return_if_fail (cv != NULL);
	g_return_if_fail (E_IS_COMPLETION_VIEW (cv));

	cv->uncomplete_key = keyval;
}

void
e_completion_view_set_width (ECompletionView *cv, gint width)
{
	GtkWidget *w;
	gint y, r, dummy, line_height;
	double drop_room, lines;

	g_return_if_fail (cv != NULL);
	g_return_if_fail (E_IS_COMPLETION_VIEW (cv));
	g_return_if_fail (width > 0);

	w = GTK_WIDGET (cv);

	if (! GTK_WIDGET_REALIZED (w)) {
		gtk_widget_set_usize (w, width, -1);
		return;
	}

	/* A Horrible Hack(tm) to figure out the height of a single table row */
	
	for (line_height=5, r=0; r == 0 && line_height < 1000; line_height += 2) {
		dummy = 0;
		e_table_group_compute_location (e_completion_view_table (cv)->group,
						&dummy, &line_height, &r, &dummy);
	}

	if (line_height >= 1000) {
		/* Something went wrong, so we make a (possibly very lame) guess */
		line_height = 30;
	}


	gdk_window_get_origin (w->window, NULL, &y);
	y += w->allocation.y;

	lines = 5; /* default maximum */
	lines = MIN (lines, cv->choice_count);
	
	drop_room = (gdk_screen_height () - y) / (double)line_height;
	drop_room = MAX (drop_room, 1);

	lines = MIN (lines, drop_room);

	/* We reduce the total height by a bit; in practice, this seems to work out well. */
	gtk_widget_set_usize (w, width, (gint) floor (line_height * (0.5 + (float)lines) * 0.97));
}

void
e_completion_view_set_editable (ECompletionView *cv, gboolean x)
{
	g_return_if_fail (cv != NULL);
	g_return_if_fail (E_IS_COMPLETION_VIEW (cv));

	if (x == cv->editable)
		return;

	cv->editable = x;
	cv->selection = -1;
	e_completion_view_set_cursor_row (cv, -1);
}

