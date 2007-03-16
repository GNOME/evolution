/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-completion-view.c - A text completion selection widget
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Miguel de Icaza <miguel@ximian.com>
 *   Adapted by Jon Trowbridge <trow@ximian.com>
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

#include <math.h>

#include <gdk/gdkkeysyms.h>

#include "table/e-table-scrolled.h"
#include "table/e-table-simple.h"
#include "e-util/e-i18n.h"
#include "e-util/e-util-marshal.h"

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
static void    e_completion_view_dispose    (GObject *object);

#define PARENT_TYPE GTK_TYPE_EVENT_BOX
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

#if 0
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
#endif

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
			gtk_widget_send_expose (bin->child, (GdkEvent*) &child_event);
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

	requisition->height = MAX (100, requisition->height);
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

E_MAKE_TYPE (e_completion_view,
	     "ECompletionView",
	     ECompletionView,
	     e_completion_view_class_init,
	     e_completion_view_init,
	     PARENT_TYPE)
	     
static void
e_completion_view_class_init (ECompletionViewClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	parent_class = g_type_class_ref (PARENT_TYPE);

	e_completion_view_signals[E_COMPLETION_VIEW_NONEMPTY] =
		g_signal_new ("nonempty",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECompletionViewClass, nonempty),
			      NULL, NULL,
			      e_util_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	e_completion_view_signals[E_COMPLETION_VIEW_ADDED] =
		g_signal_new ("added",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECompletionViewClass, added),
			      NULL, NULL,
			      e_util_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	e_completion_view_signals[E_COMPLETION_VIEW_FULL] =
		g_signal_new ("full",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECompletionViewClass, full),
			      NULL, NULL,
			      e_util_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	e_completion_view_signals[E_COMPLETION_VIEW_BROWSE] =
		g_signal_new ("browse",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECompletionViewClass, browse),
			      NULL, NULL,
			      e_util_marshal_NONE__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);

	e_completion_view_signals[E_COMPLETION_VIEW_UNBROWSE] =
		g_signal_new ("unbrowse",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECompletionViewClass, unbrowse),
			      NULL, NULL,
			      e_util_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	e_completion_view_signals[E_COMPLETION_VIEW_ACTIVATE] =
		g_signal_new ("activate",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECompletionViewClass, activate),
			      NULL, NULL,
			      e_util_marshal_NONE__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);

	object_class->dispose = e_completion_view_dispose;

	widget_class->key_press_event = e_completion_view_local_key_press_handler;
	widget_class->expose_event = e_completion_view_expose_event;
	widget_class->size_request = e_completion_view_size_request;
	widget_class->size_allocate = e_completion_view_size_allocate;
}

static void
e_completion_view_init (ECompletionView *completion)
{
	completion->border_width = 2;
	completion->choices = g_ptr_array_new ();
}

static void
e_completion_view_dispose (GObject *object)
{
	ECompletionView *cv = E_COMPLETION_VIEW (object);

	e_completion_view_disconnect (cv);

	if (cv->choices) {
		e_completion_view_clear_choices (cv);

		g_ptr_array_free (cv->choices, TRUE);
		cv->choices = NULL;
	}

	if (cv->key_widget) {
		g_signal_handler_disconnect (cv->key_widget, cv->key_signal_id);
		g_object_unref (cv->key_widget);
		cv->key_widget = NULL;
	}

	if (cv->completion)
		g_object_unref (cv->completion);
	cv->completion = NULL;

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
e_completion_view_disconnect (ECompletionView *cv)
{
	g_return_if_fail (cv != NULL);
	g_return_if_fail (E_IS_COMPLETION_VIEW (cv));

	if (cv->begin_signal_id)
		g_signal_handler_disconnect (cv->completion, cv->begin_signal_id);
	if (cv->comp_signal_id)
		g_signal_handler_disconnect (cv->completion, cv->comp_signal_id);
	if (cv->end_signal_id)
		g_signal_handler_disconnect (cv->completion, cv->end_signal_id);

	cv->begin_signal_id   = 0;
	cv->comp_signal_id    = 0;
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
	ECompletionMatch *match;
	GPtrArray *m;
	int i;

	g_return_if_fail (E_IS_COMPLETION_VIEW (cv));

	m = cv->choices;
	for (i = 0; i < m->len; i++) {
		match = g_ptr_array_index (m, i);
		e_completion_match_unref (match);
	}
	g_ptr_array_set_size (m, 0);
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
#ifndef G_DISABLE_CHECKS
	/* choices->len is unsigned, but it is reasonable for r to be
	 * < 0 */
	if (r > 0) {
		g_return_if_fail (r < cv->choices->len);
	}
#endif

	adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (cv->table));	

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
	} else if (r == cv->choices->len - 1) {		
		gtk_adjustment_set_value (adj, adj->upper - adj->page_size);
		return;
	}
	
	fracline = ((adj->upper - adj->lower - adj->page_size) / (gint)cv->choices->len) / 4;

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
	ECompletionMatch *match;

	match = g_ptr_array_index (cv->choices, r);

	cv->selection = r;
	e_completion_view_set_cursor_row (cv, r);
	g_signal_emit (cv, e_completion_view_signals[E_COMPLETION_VIEW_ACTIVATE], 0, match);
}

static gint
e_completion_view_key_press_handler (GtkWidget *w, GdkEventKey *key_event, gpointer user_data)
{
	ECompletionView *cv = E_COMPLETION_VIEW (user_data);
	gint dir = 0;
	gboolean key_handled = TRUE, complete_key = FALSE, uncomplete_key = FALSE, is_space = FALSE;

	/* FIXME: This is totally lame.
	   The ECompletionView should be able to specify multiple completion/uncompletion keys, or just
	   have sensible defaults. */
	
	if ((cv->complete_key && key_event->keyval == cv->complete_key)
	    || ((key_event->keyval == GDK_n || key_event->keyval == GDK_N) && (key_event->state & GDK_CONTROL_MASK)))
		complete_key = TRUE;

	if ((cv->uncomplete_key && key_event->keyval == cv->uncomplete_key)
	    || ((key_event->keyval == GDK_p || key_event->keyval == GDK_P) && (key_event->state & GDK_CONTROL_MASK)))
		uncomplete_key = TRUE;
	
	/* Start up a completion.*/
	if (complete_key && !cv->editable) {
		g_signal_emit (cv, e_completion_view_signals[E_COMPLETION_VIEW_BROWSE], 0, NULL);
		goto stop_emission;
	}

	/* Stop our completion. */
	if (uncomplete_key && cv->editable && cv->selection < 0) {
		e_completion_view_set_cursor_row (cv, -1);
		g_signal_emit (cv, e_completion_view_signals[E_COMPLETION_VIEW_UNBROWSE], 0);
		goto stop_emission;
	}

	if (!cv->editable)
		return FALSE;

	switch (key_event->keyval) {

	case GDK_n:
	case GDK_N:
		/* We (heart) emacs: treat ctrl-n as down */
		if (! (key_event->state & GDK_CONTROL_MASK))
			return FALSE;

	case GDK_Down:
	case GDK_KP_Down:
		dir = 1;
		break;

	case GDK_p:
	case GDK_P:
		/* Treat ctrl-p as up */
		if (! (key_event->state & GDK_CONTROL_MASK))
			return FALSE;
		
	case GDK_Up:
	case GDK_KP_Up:
		dir = -1;
		break;
		
	case GDK_Tab:
		/* If our cursor is still up in the entry, move down into
		   the popup.  Otherwise unbrowse. */
		if (cv->choices->len > 0) {
			if (cv->selection < 0) {
				cv->selection = 0;
				dir = 0;
			} else {
				cv->selection = -1;
				dir = 0;
				key_handled = FALSE;
			}
		}
		break;

	case GDK_space:
	case GDK_KP_Space:
		is_space = TRUE;

	case GDK_Return:
	case GDK_KP_Enter:
		if (cv->selection < 0) {
			/* We don't have a selection yet, move to the first selection if there is
			   more than one option.  If there is only one option, select it automatically. */

			/* Let space pass through. */
			if (is_space)
				return FALSE;
			
			if (cv->choices->len == 1) {
				e_completion_view_select (cv, 0);
				goto stop_emission;
			} else {
				cv->selection = 0;
				dir = 0;
			}

		} else {
			/* Our cursor is down in the pop-up, so we make our selection. */
			e_completion_view_select (cv, cv->selection);
			goto stop_emission;
		}
		break;

	case GDK_Escape:
		/* Unbrowse hack */
		cv->selection = -1;
		dir = 0;
		break;

	default:
		return FALSE;
	}

	cv->selection += dir;

	if (cv->selection >= (int)cv->choices->len) {
		cv->selection = cv->choices->len - 1;
		/* Don't re-emit the browse signal */
		goto stop_emission;
	}

	e_completion_view_set_cursor_row (cv, cv->selection);

	if (cv->selection >= 0)
		g_signal_emit (cv, e_completion_view_signals[E_COMPLETION_VIEW_BROWSE], 0,
			       g_ptr_array_index (cv->choices, cv->selection));
	else
		g_signal_emit (cv, e_completion_view_signals[E_COMPLETION_VIEW_UNBROWSE], 0);

 stop_emission:
	
	if (key_handled)
		g_signal_stop_emission_by_name (w, "key_press_event");

	return key_handled;
}

static void
begin_completion_cb (ECompletion *completion, const gchar *txt, gint pos, gint limit, gpointer user_data)
{
	ECompletionView *cv = E_COMPLETION_VIEW (user_data);

	e_table_model_pre_change (cv->model);
	e_completion_view_clear_choices (cv);
	cv->have_all_choices = FALSE;

	e_table_model_changed (cv->model);
}

static void
completion_cb (ECompletion *completion, ECompletionMatch *match, gpointer user_data)
{
	ECompletionView *cv = E_COMPLETION_VIEW (user_data);
	gint r = cv->choices->len;
	gboolean first = (cv->choices->len == 0);

	e_table_model_pre_change (cv->model);

	e_completion_match_ref (match);
	g_ptr_array_add (cv->choices, match);

	e_table_model_row_inserted (cv->model, r);

	if (first)
		g_signal_emit (cv, e_completion_view_signals[E_COMPLETION_VIEW_NONEMPTY], 0);

	g_signal_emit (cv, e_completion_view_signals[E_COMPLETION_VIEW_ADDED], 0);
}

static void
end_completion_cb (ECompletion *completion, gpointer user_data)
{
	ECompletionView *cv = E_COMPLETION_VIEW (user_data);

	/* Do a final refresh of the table. */
	e_table_model_pre_change (cv->model);
	e_table_model_changed (cv->model);

	cv->have_all_choices = TRUE;
	g_signal_emit (cv, e_completion_view_signals[E_COMPLETION_VIEW_FULL], 0);
}

/*** Table Callbacks ***/

/* XXX toshok - we need to add sorting to this etable, through the use
   of undisplayed fields of all the sort keys we want to use */
static char *simple_spec = 
"<ETableSpecification no-headers=\"true\" draw-grid=\"false\" cursor-mode=\"line\" alternating-row-colors=\"false\" gettext-domain=\"" E_I18N_DOMAIN "\">"
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
	return cv->choices->len;
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

	match = g_ptr_array_index (cv->choices, r);

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
	g_object_ref (completion);

	cv->begin_signal_id   = g_signal_connect (completion,
						  "completion_started",
						  G_CALLBACK (begin_completion_cb),
						  cv);
	cv->comp_signal_id    = g_signal_connect (completion,
						  "completion_found",
						  G_CALLBACK (completion_cb),
						  cv);
	cv->end_signal_id     = g_signal_connect (completion,
						  "completion_finished",
						  G_CALLBACK (end_completion_cb),
						  cv);

	cv->model = e_table_simple_new (table_col_count,
					table_row_count,
					NULL,

					table_value_at,
					NULL,
					table_is_cell_editable,

					NULL, NULL,

					NULL, NULL, NULL, NULL,
					table_value_to_string,
					cv);

	cv->table = e_table_scrolled_new (cv->model, NULL, simple_spec, NULL);
	g_object_unref (cv->model);

	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (cv->table), GTK_SHADOW_NONE);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (cv->table), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	gtk_container_add (GTK_CONTAINER (cv), cv->table);
	gtk_widget_show_all (cv->table);

	g_signal_connect (e_completion_view_table (cv),
			  "click",
			  G_CALLBACK (table_click_cb),
			  cv);

	cv->selection = -1;
}

GtkWidget *
e_completion_view_new (ECompletion *completion)
{
	gpointer p;

	g_return_val_if_fail (completion != NULL, NULL);
	g_return_val_if_fail (E_IS_COMPLETION (completion), NULL);

	p = g_object_new (E_COMPLETION_VIEW_TYPE, NULL);

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
		g_signal_handler_disconnect (cv->key_widget, cv->key_signal_id);
		g_object_unref (cv->key_widget);
	}

	if (w) {
		cv->key_widget = w;
		g_object_ref (w);

		cv->key_signal_id = g_signal_connect (w,
						      "key_press_event",
						      G_CALLBACK (e_completion_view_key_press_handler),
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
	gint y, r, dummy, line_height, final_height;
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
	lines = MIN (lines, cv->choices->len);
	
	drop_room = (gdk_screen_height () - y) / (double)line_height;
	drop_room = MAX (drop_room, 1);

	lines = MIN (lines, drop_room);

	/* We reduce the total height by a bit; in practice, this seems to work out well. */
	final_height = (gint) floor (line_height * (0.5 + (float)lines) * 0.97);
	gtk_widget_set_usize (w, width, final_height);
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


