/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-entry.c - An EText-based entry widget
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey     <clahey@ximian.com>
 *   Jon Trowbridge  <trow@ximian.com>
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktypebuiltins.h>
#include <libxml/parser.h>
#include <libgnomecanvas/gnome-canvas.h>
#include "gal/util/e-util.h"
#include "gal/util/e-i18n.h"
#include "gal/widgets/e-canvas.h"
#include "gal/widgets/e-canvas-utils.h"
#include "e-completion-view.h"
#include "e-text.h"
#include "e-entry.h"

#define MIN_ENTRY_WIDTH  150
#define INNER_BORDER 2

#define d(x)

#define PARENT_TYPE gtk_table_get_type ()

static GtkObjectClass *parent_class;

enum {
	E_ENTRY_CHANGED,
	E_ENTRY_ACTIVATE,
	E_ENTRY_POPULATE_POPUP,
	E_ENTRY_COMPLETION_POPUP,
	E_ENTRY_LAST_SIGNAL
};

static guint e_entry_signals[E_ENTRY_LAST_SIGNAL] = { 0 };

/* Object argument IDs */
enum {
	PROP_0,
	PROP_MODEL,
	PROP_EVENT_PROCESSOR,
	PROP_TEXT,
	PROP_FONT,
        PROP_FONTSET,
	PROP_FONT_GDK,
	PROP_ANCHOR,
	PROP_JUSTIFICATION,
	PROP_X_OFFSET,
	PROP_Y_OFFSET,
	PROP_FILL_COLOR,
	PROP_FILL_COLOR_GDK,
	PROP_FILL_COLOR_RGBA,
	PROP_FILL_STIPPLE,
	PROP_EDITABLE,
	PROP_USE_ELLIPSIS,
	PROP_ELLIPSIS,
	PROP_LINE_WRAP,
	PROP_BREAK_CHARACTERS,
	PROP_MAX_LINES,
	PROP_ALLOW_NEWLINES,
	PROP_DRAW_BORDERS,
	PROP_DRAW_BACKGROUND,
	PROP_DRAW_BUTTON,
	PROP_EMULATE_LABEL_RESIZE,
	PROP_CURSOR_POS
};

typedef struct _EEntryPrivate EEntryPrivate;
struct _EEntryPrivate {
	GtkJustification justification;

	guint changed_proxy_tag;
	guint activate_proxy_tag;
	guint populate_popup_proxy_tag;
	/* Data related to completions */
	ECompletion *completion;
	EEntryCompletionHandler handler;
	GtkWidget *completion_view;
	guint nonempty_signal_id;
	guint added_signal_id;
	guint full_signal_id;
	guint browse_signal_id;
	guint unbrowse_signal_id;
	guint activate_signal_id;
	GtkWidget *completion_view_popup;
	gboolean popup_is_visible;
	gchar *pre_browse_text;
	gint completion_delay;
	guint completion_delay_tag;
	gboolean ptr_grab;
	gboolean changed_since_keypress;
	guint changed_since_keypress_tag;
	gint last_completion_pos;

	guint draw_borders : 1;
	guint emulate_label_resize : 1;
	guint have_set_transient : 1;
	guint item_chosen : 1;
	gint last_width;
};

static gboolean e_entry_is_empty              (EEntry *entry);
static void e_entry_show_popup                (EEntry *entry, gboolean x);
static void e_entry_start_completion          (EEntry *entry);
static void e_entry_start_delayed_completion  (EEntry *entry, gint delay);
static void e_entry_cancel_delayed_completion (EEntry *entry);

static void
canvas_size_allocate (GtkWidget *widget, GtkAllocation *alloc,
		      EEntry *entry)
{
	gnome_canvas_set_scroll_region (entry->canvas,
					0, 0, alloc->width, alloc->height);
	g_object_set (entry->item,
		      "clip_width", (double) (alloc->width),
		      "clip_height", (double) (alloc->height),
		      NULL);

	switch (entry->priv->justification) {
	case GTK_JUSTIFY_RIGHT:
		e_canvas_item_move_absolute(GNOME_CANVAS_ITEM(entry->item),
					    alloc->width, 0);
		break;
	case GTK_JUSTIFY_CENTER:
		e_canvas_item_move_absolute(GNOME_CANVAS_ITEM(entry->item),
					    alloc->width / 2, 0);
		break;
	default:
		e_canvas_item_move_absolute(GNOME_CANVAS_ITEM(entry->item),
					    0, 0);
		break;
	}
}

#if 0
static void
get_borders (EEntry   *entry,
             gint     *xborder,
             gint     *yborder)
{
  GtkWidget *widget = GTK_WIDGET (entry);
  gint focus_width;
  gboolean interior_focus;

  gtk_widget_style_get (widget,
			"interior-focus", &interior_focus,
			"focus-line-width", &focus_width,
			NULL);

  *xborder = widget->style->xthickness;
  *yborder = widget->style->ythickness;

  if (!interior_focus)
    {
      *xborder += focus_width;
      *yborder += focus_width;
    }
}
#endif

static void
canvas_size_request (GtkWidget *widget, GtkRequisition *requisition,
		     EEntry *entry)
{
	int xthick, ythick;
	PangoContext *context;
	PangoFontMetrics *metrics;
	
	g_return_if_fail (widget != NULL);
	g_return_if_fail (GNOME_IS_CANVAS (widget));
	g_return_if_fail (requisition != NULL);

	if (entry->priv->draw_borders) {
		/* get_borders (entry, &xthick, &ythick); */
		xthick = ythick = 3;
	} else {
		xthick = ythick = 0;
	}
	
	if (entry->priv->emulate_label_resize) {
		gdouble width;
		g_object_get (entry->item,
			      "text_width", &width,
			      NULL);
		requisition->width = 2*xthick + width;
	} else {
		requisition->width = MIN_ENTRY_WIDTH + 2*xthick;
	}
	if (entry->priv->last_width != requisition->width)
		gtk_widget_queue_resize (widget);
	entry->priv->last_width = requisition->width;

	d(g_print("%s: width = %d\n", __FUNCTION__, requisition->width));

	context = gtk_widget_get_pango_context (widget);
	metrics = pango_context_get_metrics (context, gtk_widget_get_style (widget)->font_desc,
					     pango_context_get_language (context));

	requisition->height = (PANGO_PIXELS (pango_font_metrics_get_ascent (metrics) +
					     pango_font_metrics_get_descent (metrics)) +
			       2 * ythick);

	pango_font_metrics_unref (metrics);
}

static gint
canvas_focus_in_event (GtkWidget *widget, GdkEventFocus *focus, EEntry *entry)
{
	if (entry->canvas->focused_item != GNOME_CANVAS_ITEM(entry->item))
		gnome_canvas_item_grab_focus(GNOME_CANVAS_ITEM(entry->item));

	return FALSE;
}

static void
e_entry_text_keypress (EText *text, guint keyval, guint state, EEntry *entry)
{
	if (entry->priv->changed_since_keypress_tag) {
		gtk_timeout_remove (entry->priv->changed_since_keypress_tag);
		entry->priv->changed_since_keypress_tag = 0;
	}
	
	if (entry->priv->changed_since_keypress
	    || (entry->priv->popup_is_visible && e_entry_get_position (entry) != entry->priv->last_completion_pos)) {
		if (e_entry_is_empty (entry)) {
			e_entry_cancel_delayed_completion (entry);
			e_entry_show_popup (entry, FALSE);
		} else if (entry->priv->completion_delay >= 0) {
			int delay;
			delay = entry->priv->popup_is_visible 
				? 1 
				: entry->priv->completion_delay;
			e_entry_start_delayed_completion (entry, delay);
		}
	}
	entry->priv->changed_since_keypress = FALSE;
}

static gint
changed_since_keypress_timeout_fn (gpointer user_data)
{
	EEntry *entry = E_ENTRY (user_data);
	entry->priv->changed_since_keypress = FALSE;
	entry->priv->changed_since_keypress_tag = 0;
	return FALSE;
}

static void
proxy_changed (EText *text, EEntry *entry)
{
	if (entry->priv->changed_since_keypress_tag)
		gtk_timeout_remove (entry->priv->changed_since_keypress_tag);
	entry->priv->changed_since_keypress = TRUE;
	entry->priv->changed_since_keypress_tag = gtk_timeout_add (20, changed_since_keypress_timeout_fn, entry);
	
	g_signal_emit (entry, e_entry_signals [E_ENTRY_CHANGED], 0);
}

static void
proxy_activate (EText *text, EEntry *entry)
{
	g_signal_emit (entry, e_entry_signals [E_ENTRY_ACTIVATE], 0);
}

static void
proxy_populate_popup (EText *text, GdkEventButton *ev, gint pos, GtkWidget *menu, EEntry *entry)
{
	g_signal_emit (entry, e_entry_signals [E_ENTRY_POPULATE_POPUP], 0, ev, pos, menu);
}

static void
e_entry_init (GtkObject *object)
{
	EEntry *entry = E_ENTRY (object);
	GtkTable *gtk_table = GTK_TABLE (object);

	entry->priv = g_new0 (EEntryPrivate, 1);

	entry->priv->emulate_label_resize = FALSE;
	
	entry->canvas = GNOME_CANVAS (e_canvas_new ());

	g_signal_connect (entry->canvas,
			  "size_allocate",
			  G_CALLBACK (canvas_size_allocate),
			  entry);

	g_signal_connect (entry->canvas,
			  "size_request",
			  G_CALLBACK (canvas_size_request),
			  entry);

	g_signal_connect (entry->canvas,
			  "focus_in_event",
			  G_CALLBACK(canvas_focus_in_event),
			  entry);

	entry->priv->draw_borders = TRUE;
	entry->priv->last_width = -1;

	entry->item = E_TEXT(gnome_canvas_item_new(
		gnome_canvas_root (entry->canvas),
		e_text_get_type(),
		"clip", TRUE,
		"fill_clip_rectangle", TRUE,
		"anchor", GTK_ANCHOR_NW,
		"draw_borders", TRUE,
		"draw_background", TRUE,
		"draw_button", FALSE,
		"max_lines", 1,
		"editable", TRUE,
		"allow_newlines", FALSE,
		"im_context", E_CANVAS (entry->canvas)->im_context,
		"handle_popup", TRUE,
		NULL));

	g_signal_connect (entry->item,
			  "keypress",
			  G_CALLBACK (e_entry_text_keypress),
			  entry);

	entry->priv->justification = GTK_JUSTIFY_LEFT;
	gtk_table_attach (gtk_table, GTK_WIDGET (entry->canvas),
			  0, 1, 0, 1,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  0, 0);
	gtk_widget_show (GTK_WIDGET (entry->canvas));

	/*
	 * Proxy functions: we proxy the changed and activate signals
	 * from the item to ourselves
	 */
	entry->priv->changed_proxy_tag = g_signal_connect (entry->item,
							   "changed",
							   G_CALLBACK (proxy_changed),
							   entry);
	entry->priv->activate_proxy_tag = g_signal_connect (entry->item,
							    "activate",
							    G_CALLBACK (proxy_activate),
							    entry);
	entry->priv->populate_popup_proxy_tag = g_signal_connect (entry->item,
								  "populate_popup",
								  G_CALLBACK (proxy_populate_popup),
								  entry);

	entry->priv->completion_delay = 1;
}

/**
 * e_entry_construct
 * 
 * Constructs the given EEntry.
 * 
 **/
void
e_entry_construct (EEntry *entry)
{
	/* Do nothing */
}


/**
 * e_entry_new
 * 
 * Creates a new EEntry.
 * 
 * Returns: The new EEntry
 **/
GtkWidget *
e_entry_new (void)
{
	EEntry *entry;
	entry = g_object_new (E_ENTRY_TYPE, NULL);
	e_entry_construct (entry);

	return GTK_WIDGET (entry);
}

const gchar *
e_entry_get_text (EEntry *entry)
{
	g_return_val_if_fail (entry != NULL && E_IS_ENTRY (entry), NULL);

	return e_text_model_get_text (entry->item->model);
}

void
e_entry_set_text (EEntry *entry, const gchar *txt)
{
	g_return_if_fail (entry != NULL && E_IS_ENTRY (entry));

	e_text_model_set_text (entry->item->model, txt);
}

static void
e_entry_set_text_quiet (EEntry *entry, const gchar *txt)
{
	g_return_if_fail (entry != NULL && E_IS_ENTRY (entry));

	g_signal_handler_block (entry->item, entry->priv->changed_proxy_tag);
	e_entry_set_text (entry, txt);
	g_signal_handler_unblock (entry->item, entry->priv->changed_proxy_tag);
}


void
e_entry_set_editable (EEntry *entry, gboolean am_i_editable)
{
	g_return_if_fail (entry != NULL && E_IS_ENTRY (entry));

	g_object_set (entry->item, "editable", am_i_editable, NULL);
}

gint
e_entry_get_position (EEntry *entry)
{
	g_return_val_if_fail (entry != NULL && E_IS_ENTRY (entry), -1);

	return entry->item->selection_start;
}

void
e_entry_set_position (EEntry *entry, gint pos)
{
	g_return_if_fail (entry != NULL && E_IS_ENTRY (entry));
	if (pos < 0)
		pos = 0;
	else if (pos > e_text_model_get_text_length (entry->item->model))
		pos = e_text_model_get_text_length (entry->item->model);

	entry->item->selection_start = entry->item->selection_end = pos;
}

void
e_entry_select_region (EEntry *entry, gint pos1, gint pos2)
{
	gint len;

	g_return_if_fail (entry != NULL && E_IS_ENTRY (entry));
	
	len = e_text_model_get_text_length (entry->item->model);
	pos1 = CLAMP (pos1, 0, len);
	pos2 = CLAMP (pos2, 0, len);

	entry->item->selection_start = MIN (pos1, pos2);
	entry->item->selection_end   = MAX (pos1, pos2);
}

/** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **/

/*** Completion-related code ***/

static gboolean
e_entry_is_empty (EEntry *entry)
{
	const gchar *txt = e_entry_get_text (entry);

	if (txt == NULL)
		return TRUE;

	while (*txt) {
		if (!isspace ((gint) *txt))
			return FALSE;
		++txt;
	}
	
	return TRUE;
}

static void
e_entry_show_popup (EEntry *entry, gboolean visible)
{
	GtkWidget *pop = entry->priv->completion_view_popup;

	if (pop == NULL)
		return;

	/* The async query can give us a result after the focus was lost by the
	   widget.  In that case, we don't want to show the pop-up.   */
	if (! GTK_WIDGET_HAS_FOCUS (entry->canvas))
		return;

	if (visible) {
		GtkAllocation *dim = &(GTK_WIDGET (entry)->allocation);
		gint x, y, xo, yo, fudge;
		const GdkEventMask grab_mask = (GdkEventMask)GDK_BUTTON_PRESS_MASK | GDK_BUTTON_MOTION_MASK | GDK_BUTTON_RELEASE_MASK;

		/* Figure out where to put our popup. */
		gdk_window_get_origin (GTK_WIDGET (entry)->window, &xo, &yo);
		x = xo + dim->x;
		y = yo + dim->height + dim->y;

		fudge = 1;
		y -= fudge;

		gtk_widget_set_uposition (pop, x, y);
		e_completion_view_set_width (E_COMPLETION_VIEW (entry->priv->completion_view), dim->width);

		gtk_widget_set_sensitive(pop, TRUE);
		gtk_widget_show (pop);


		if (getenv ("GAL_E_ENTRY_NO_GRABS_HACK") == NULL && !entry->priv->ptr_grab) {
			entry->priv->ptr_grab = (0 == gdk_pointer_grab (GTK_WIDGET (entry->priv->completion_view)->window, TRUE,
									grab_mask, NULL, NULL, GDK_CURRENT_TIME));
			if (entry->priv->ptr_grab) {
 				gtk_grab_add (GTK_WIDGET (entry->priv->completion_view));
			}
		}
		
		
	} else {

		gtk_widget_hide (pop);
		/* hack to force the popup to lose focus, which it gets if you click on it */
		gtk_widget_set_sensitive(pop, FALSE);

		if (entry->priv->ptr_grab) {
			gdk_pointer_ungrab (GDK_CURRENT_TIME);
			gtk_grab_remove (GTK_WIDGET (entry->priv->completion_view));
		}

		entry->priv->ptr_grab = FALSE;

		entry->priv->last_completion_pos = -1;
	}

	e_completion_view_set_editable (E_COMPLETION_VIEW (entry->priv->completion_view), visible);

	if (entry->priv->popup_is_visible != visible) {
		entry->priv->popup_is_visible = visible;
		g_signal_emit (entry, e_entry_signals[E_ENTRY_COMPLETION_POPUP], 0, (gint) visible);
	}
}

static void
e_entry_start_completion (EEntry *entry)
{
	if (entry->priv->completion == NULL)
		return;

	if (e_entry_is_empty (entry))
		return;

	entry->priv->item_chosen = FALSE;

	e_completion_begin_search (entry->priv->completion,
				   e_entry_get_text (entry),
				   entry->priv->last_completion_pos = e_entry_get_position (entry),
				   0); /* No limit.  Probably a bad idea. */
}

static gboolean
start_delayed_cb (gpointer user_data)
{
	EEntry *entry = E_ENTRY (user_data);
	entry->priv->completion_delay_tag = 0;
	e_entry_start_completion (entry);
	return FALSE;
}

static void
e_entry_start_delayed_completion (EEntry *entry, gint delay)
{
	if (delay < 0)
		return;

	e_entry_cancel_delayed_completion (entry);
	entry->priv->completion_delay_tag = gtk_timeout_add (MAX (delay, 1), start_delayed_cb, entry);
}

static void
e_entry_cancel_delayed_completion (EEntry *entry)
{
	if (entry->priv->completion == NULL)
		return;

	if (entry->priv->completion_delay_tag) {
		gtk_timeout_remove (entry->priv->completion_delay_tag);
		entry->priv->completion_delay_tag = 0;
	}
}

static void
nonempty_cb (ECompletionView *view, gpointer user_data)
{
	EEntry *entry = E_ENTRY (user_data);

	e_entry_show_popup (entry, TRUE);
}

static void
full_cb (ECompletionView *view, gpointer user_data)
{
	EEntry *entry = E_ENTRY (user_data);
	gboolean show;

	show = GTK_WIDGET_HAS_FOCUS (GTK_WIDGET (entry->canvas)) && view->choices->len > 0 && !entry->priv->item_chosen;
	e_entry_show_popup (entry, show);
}

static void
browse_cb (ECompletionView *view, ECompletionMatch *match, gpointer user_data)
{
	EEntry *entry = E_ENTRY (user_data);
	
	if (match == NULL) {
		/* Requesting a completion. */
		e_entry_start_completion (entry);
		return;
	}

	if (entry->priv->pre_browse_text == NULL)
		entry->priv->pre_browse_text = g_strdup (e_entry_get_text (entry));

	/* If there is no other handler in place, echo the selected completion in
	   the entry. */
	if (entry->priv->handler == NULL)
		e_entry_set_text_quiet (entry, e_completion_match_get_match_text (match));
}

static void
unbrowse_cb (ECompletionView *view, gpointer user_data)
{
	EEntry *entry = E_ENTRY (user_data);

	if (entry->priv->pre_browse_text) {

		if (entry->priv->handler == NULL)
			e_entry_set_text_quiet (entry, entry->priv->pre_browse_text);

		g_free (entry->priv->pre_browse_text);
		entry->priv->pre_browse_text = NULL;
	}

	e_entry_show_popup (entry, FALSE);
}

static void
activate_cb (ECompletionView *view, ECompletionMatch *match, gpointer user_data)
{
	EEntry *entry = E_ENTRY (user_data);

	e_entry_cancel_delayed_completion (entry);
	
	g_free (entry->priv->pre_browse_text);
	entry->priv->pre_browse_text = NULL;
	e_entry_show_popup (entry, FALSE);

	if (entry->priv->handler)
		entry->priv->handler (entry, match);
	else
		e_entry_set_text (entry, match->match_text);

	entry->priv->item_chosen = TRUE;

	e_entry_cancel_delayed_completion (entry);
}

void
e_entry_enable_completion (EEntry *entry, ECompletion *completion)
{
	g_return_if_fail (entry != NULL && E_IS_ENTRY (entry));
	g_return_if_fail (completion != NULL && E_IS_COMPLETION (completion));

	e_entry_enable_completion_full (entry, completion, -1, NULL);
}

static void
button_press_cb (GtkWidget *w, GdkEvent *ev, gpointer user_data)
{
	EEntry *entry = E_ENTRY (user_data);
	GtkWidget *child;

	/* Bail out if our click happened inside of our widget. */
	child = gtk_get_event_widget (ev);
	if (child != w) {
		while (child) {
			if (child == w)
				return;
			child = child->parent;
		}
	}

	/* Treat this as an unbrowse */
	unbrowse_cb (E_COMPLETION_VIEW (w), entry);
}

static void
cancel_completion_cb (ETextModel *model, gpointer user_data)
{
	EEntry *entry = E_ENTRY (user_data);

	/* If we get the signal from the underlying text model, unbrowse.
	   This usually means that the text model itself has done some
	   sort of completion, or has otherwise transformed its contents
	   in some way that would render any previous completion invalid. */
	unbrowse_cb (E_COMPLETION_VIEW (entry->priv->completion_view), entry);
}

static gint
key_press_cb (GtkWidget *w, GdkEventKey *ev, gpointer user_data)
{
	gint rv = 0;
	/* Forward signal */
	g_signal_emit_by_name (user_data, "key_press_event", ev, &rv);
	return rv;
}

static gint
key_release_cb (GtkWidget *w, GdkEventKey *ev, gpointer user_data)
{
	gint rv = 0;
	/* Forward signal */
	g_signal_emit_by_name (user_data, "key_release_event", ev, &rv);
	return rv;
}

static void
e_entry_make_completion_window_transient (EEntry *entry)
{
	GtkWidget *w;

	if (entry->priv->have_set_transient || entry->priv->completion_view_popup == NULL)
		return;
	
	w = GTK_WIDGET (entry)->parent;
	while (w && ! GTK_IS_WINDOW (w))
		w = w->parent;

	if (w) {
		gtk_window_set_transient_for (GTK_WINDOW (entry->priv->completion_view_popup),
					      GTK_WINDOW (w));
		entry->priv->have_set_transient = 1;
	}
}

void
e_entry_enable_completion_full (EEntry *entry, ECompletion *completion, gint delay, EEntryCompletionHandler handler)
{
	g_return_if_fail (entry != NULL && E_IS_ENTRY (entry));
	g_return_if_fail (completion != NULL && E_IS_COMPLETION (completion));

	/* For now, completion can't be changed mid-stream. */
	g_return_if_fail (entry->priv->completion == NULL);

	entry->priv->completion = completion;
	g_object_ref (completion);
	gtk_object_sink (GTK_OBJECT (completion));
	
	entry->priv->completion_delay = delay;
	entry->priv->handler = handler;

	entry->priv->completion_view = e_completion_view_new (completion);
	/* Make the up and down keys enable and disable completions. */
	e_completion_view_set_complete_key (E_COMPLETION_VIEW (entry->priv->completion_view), GDK_Down);
	e_completion_view_set_uncomplete_key (E_COMPLETION_VIEW (entry->priv->completion_view), GDK_Up);

	g_signal_connect_after (entry->priv->completion_view,
				"button_press_event",
				G_CALLBACK (button_press_cb),
				entry);

	entry->priv->nonempty_signal_id = g_signal_connect (entry->priv->completion_view,
							    "nonempty",
							    G_CALLBACK (nonempty_cb),
							    entry);

	entry->priv->full_signal_id = g_signal_connect (entry->priv->completion_view,
							"full",
							G_CALLBACK (full_cb),
							entry);

	entry->priv->browse_signal_id = g_signal_connect (entry->priv->completion_view,
							  "browse",
							  G_CALLBACK (browse_cb),
							  entry);

	entry->priv->unbrowse_signal_id = g_signal_connect (entry->priv->completion_view,
							    "unbrowse",
							    G_CALLBACK (unbrowse_cb),
							    entry);

	entry->priv->activate_signal_id = g_signal_connect (entry->priv->completion_view,
							    "activate",
							    G_CALLBACK (activate_cb),
							    entry);

	entry->priv->completion_view_popup = gtk_window_new (GTK_WINDOW_POPUP);

	e_entry_make_completion_window_transient (entry);

	g_signal_connect (entry->item->model,
			  "cancel_completion",
			  G_CALLBACK (cancel_completion_cb),
			  entry);

	g_signal_connect (entry->priv->completion_view_popup,
			  "key_press_event",
			  G_CALLBACK (key_press_cb),
			  entry->canvas);
	g_signal_connect (entry->priv->completion_view_popup,
			  "key_release_event",
			  G_CALLBACK (key_release_cb),
			  entry->canvas);

	e_completion_view_connect_keys (E_COMPLETION_VIEW (entry->priv->completion_view),
					GTK_WIDGET (entry->canvas));

	g_object_ref (entry->priv->completion_view_popup);
	gtk_object_sink (GTK_OBJECT (entry->priv->completion_view_popup));
	gtk_window_set_policy (GTK_WINDOW (entry->priv->completion_view_popup), TRUE, TRUE, TRUE);
	gtk_container_add (GTK_CONTAINER (entry->priv->completion_view_popup), entry->priv->completion_view);
	gtk_widget_show (entry->priv->completion_view);
}

gboolean
e_entry_completion_popup_is_visible (EEntry *entry)
{
	g_return_val_if_fail (E_IS_ENTRY (entry), FALSE);

	return entry->priv->popup_is_visible;
}

/** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **/

static void
et_get_property (GObject *object,
		 guint prop_id,
		 GValue *value,
		 GParamSpec *pspec)
{
	EEntry *entry = E_ENTRY (object);
	GtkObject *item = GTK_OBJECT (entry->item);
	
	switch (prop_id){
	case PROP_MODEL:
		g_object_get_property (G_OBJECT (item), "model", value);
		break;
	case PROP_EVENT_PROCESSOR:
		g_object_get_property (G_OBJECT (item), "event_processor", value);
		break;
	case PROP_TEXT:
		g_object_get_property (G_OBJECT (item), "text", value);
		break;

	case PROP_FONT_GDK:
		g_object_get_property (G_OBJECT (item), "font_gdk", value);
		break;

	case PROP_JUSTIFICATION:
		g_object_get_property (G_OBJECT (item), "justification", value);
		break;

	case PROP_FILL_COLOR_GDK:
		g_object_get_property (G_OBJECT (item), "fill_color_gdk", value);
		break;

	case PROP_FILL_COLOR_RGBA:
		g_object_get_property (G_OBJECT (item), "fill_color_rgba", value);
		break;

	case PROP_FILL_STIPPLE:
		g_object_get_property (G_OBJECT (item), "fill_stiple", value);
		break;

	case PROP_EDITABLE:
		g_object_get_property (G_OBJECT (item), "editable", value);
		break;

	case PROP_USE_ELLIPSIS:
		g_object_get_property (G_OBJECT (item), "use_ellipsis", value);
		break;

	case PROP_ELLIPSIS:
		g_object_get_property (G_OBJECT (item), "ellipsis", value);
		break;

	case PROP_LINE_WRAP:
		g_object_get_property (G_OBJECT (item), "line_wrap", value);
		break;
		
	case PROP_BREAK_CHARACTERS:
		g_object_get_property (G_OBJECT (item), "break_characters", value);
		break;

	case PROP_MAX_LINES:
		g_object_get_property (G_OBJECT (item), "max_lines", value);
		break;
	case PROP_ALLOW_NEWLINES:
		g_object_get_property (G_OBJECT (item), "allow_newlines", value);
		break;

	case PROP_DRAW_BORDERS:
		g_value_set_boolean (value, entry->priv->draw_borders);
		break;

	case PROP_DRAW_BACKGROUND:
		g_object_get_property (G_OBJECT (item), "draw_background", value);
		break;

	case PROP_DRAW_BUTTON:
		g_object_get_property (G_OBJECT (item), "draw_button", value);
		break;

	case PROP_EMULATE_LABEL_RESIZE:
		g_value_set_boolean (value, entry->priv->emulate_label_resize);
		break;

	case PROP_CURSOR_POS:
		g_object_get_property (G_OBJECT (item), "cursor_pos", value);
		
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
et_set_property (GObject *object,
		 guint prop_id,
		 const GValue *value,
		 GParamSpec *pspec)
{
	EEntry *entry = E_ENTRY (object);
	GtkObject *item = GTK_OBJECT (entry->item);
	GtkAnchorType anchor;
	double width, height;
	gint xthick;
	gint ythick;
	GtkWidget *widget = GTK_WIDGET(entry->canvas);
	
	d(g_print("%s: prop_id: %d\n", __FUNCTION__, prop_id));

	switch (prop_id){
	case PROP_MODEL:
		g_object_set_property (G_OBJECT (item), "model", value);
		if (entry->priv->emulate_label_resize)
			gtk_widget_queue_resize (widget);
		break;

	case PROP_EVENT_PROCESSOR:
		g_object_set_property (G_OBJECT (item), "event_processor", value);
		break;

	case PROP_TEXT:
		g_object_set_property (G_OBJECT (item), "text", value);
		d(g_print("%s: text: %s\n", __FUNCTION__, g_value_get_string (value)));
		if (entry->priv->emulate_label_resize)
			gtk_widget_queue_resize (widget);
		break;

	case PROP_FONT:
		g_object_set_property (G_OBJECT (item), "font", value);
		d(g_print("%s: font: %s\n", __FUNCTION__, g_value_get_string (value)));
		if (entry->priv->emulate_label_resize)
			gtk_widget_queue_resize (widget);
		break;

	case PROP_FONTSET:
		g_object_set_property (G_OBJECT (item), "fontset", value);
		if (entry->priv->emulate_label_resize)
			gtk_widget_queue_resize (widget);
		break;

	case PROP_FONT_GDK:
		g_object_set_property (G_OBJECT (item), "font_gdk", value);
		if (entry->priv->emulate_label_resize)
			gtk_widget_queue_resize (widget);
		break;

	case PROP_JUSTIFICATION:
		entry->priv->justification = g_value_get_enum (value);
		g_object_get(item,
			     "clip_width", &width,
			     "clip_height", &height,
			     NULL);

		if (entry->priv->draw_borders) {
			xthick = 0;
			ythick = 0;
		} else {
			xthick = widget->style->xthickness;
			ythick = widget->style->ythickness;
		}

		switch (entry->priv->justification) {
		case GTK_JUSTIFY_CENTER:
			anchor = GTK_ANCHOR_N;
			e_canvas_item_move_absolute(GNOME_CANVAS_ITEM(entry->item), width / 2, ythick);
			break;
		case GTK_JUSTIFY_RIGHT:
			anchor = GTK_ANCHOR_NE;
			e_canvas_item_move_absolute(GNOME_CANVAS_ITEM(entry->item), width - xthick, ythick);
			break;
		default:
			anchor = GTK_ANCHOR_NW;
			e_canvas_item_move_absolute(GNOME_CANVAS_ITEM(entry->item), xthick, ythick);
			break;
		}
		g_object_set(item,
			     "justification", entry->priv->justification,
			     "anchor", anchor,
			     NULL);
		break;

	case PROP_FILL_COLOR:
		g_object_set_property (G_OBJECT (item), "fill_color", value);
		break;

	case PROP_FILL_COLOR_GDK:
		g_object_set_property (G_OBJECT (item), "fill_color_gdk", value);
		break;

	case PROP_FILL_COLOR_RGBA:
		g_object_set_property (G_OBJECT (item), "fill_color_rgba", value);
		break;

	case PROP_FILL_STIPPLE:
		g_object_set_property (G_OBJECT (item), "fill_stiple", value);
		break;

	case PROP_EDITABLE:
		g_object_set_property (G_OBJECT (item), "editable", value);
		break;

	case PROP_USE_ELLIPSIS:
		g_object_set_property (G_OBJECT (item), "use_ellipsis", value);
		if (entry->priv->emulate_label_resize)
			gtk_widget_queue_resize (widget);
		break;

	case PROP_ELLIPSIS:
		g_object_set_property (G_OBJECT (item), "ellipsis", value);
		if (entry->priv->emulate_label_resize)
			gtk_widget_queue_resize (widget);
		break;

	case PROP_LINE_WRAP:
		g_object_set_property (G_OBJECT (item), "line_wrap", value);
		if (entry->priv->emulate_label_resize)
			gtk_widget_queue_resize (widget);
		break;
		
	case PROP_BREAK_CHARACTERS:
		g_object_set_property (G_OBJECT (item), "break_characters", value);
		if (entry->priv->emulate_label_resize)
			gtk_widget_queue_resize (widget);
		break;

	case PROP_MAX_LINES:
		g_object_set_property (G_OBJECT (item), "max_lines", value);
		if (entry->priv->emulate_label_resize)
			gtk_widget_queue_resize (widget);
		break;

	case PROP_ALLOW_NEWLINES:
		g_object_set_property (G_OBJECT (item), "allow_newlines", value);
		break;

	case PROP_DRAW_BORDERS:
		if (entry->priv->draw_borders != g_value_get_boolean (value)) {
			entry->priv->draw_borders = g_value_get_boolean (value);
			g_object_set (item,
				      "draw_borders", entry->priv->draw_borders,
				      NULL);
			gtk_widget_queue_resize (GTK_WIDGET (entry));
		}
		break;

	case PROP_CURSOR_POS:
		g_object_set_property (G_OBJECT (item), "cursor_pos", value);
		break;
		
	case PROP_DRAW_BACKGROUND:
		g_object_set_property (G_OBJECT (item), "draw_background", value);
		break;

	case PROP_DRAW_BUTTON:
		g_object_set_property (G_OBJECT (item), "draw_button", value);
		break;

	case PROP_EMULATE_LABEL_RESIZE:
		if (entry->priv->emulate_label_resize != g_value_get_boolean (value)) {
			entry->priv->emulate_label_resize = g_value_get_boolean (value);
			gtk_widget_queue_resize (widget);
		}		
		break;
	}
}

static void
e_entry_dispose (GObject *object)
{
	EEntry *entry = E_ENTRY (object);

	if (entry->priv) {
		if (entry->priv->completion_delay_tag)
			gtk_timeout_remove (entry->priv->completion_delay_tag);

		if (entry->priv->completion)
			g_object_unref (entry->priv->completion);

		if (entry->priv->ptr_grab) {
			gdk_pointer_ungrab (GDK_CURRENT_TIME);
			gtk_grab_remove (GTK_WIDGET (entry->priv->completion_view));
		}

		if (entry->priv->completion_view_popup) {
			gtk_widget_destroy (GTK_WIDGET (entry->priv->completion_view_popup));
			g_object_unref (entry->priv->completion_view_popup);
		}
		g_free (entry->priv->pre_browse_text);

		if (entry->priv->changed_since_keypress_tag)
			gtk_timeout_remove (entry->priv->changed_since_keypress_tag);

		g_free (entry->priv);
		entry->priv = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
e_entry_realize (GtkWidget *widget)
{
	EEntry *entry;

	if (GTK_WIDGET_CLASS (parent_class)->realize)
		(* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

	entry = E_ENTRY (widget);

	e_entry_make_completion_window_transient (entry);

	if (entry->priv->emulate_label_resize) {
		d(g_print("%s: queue_resize\n", __FUNCTION__));
		gtk_widget_queue_resize (GTK_WIDGET (entry->canvas));
	}
}

static void
e_entry_class_init (GObjectClass *object_class)
{
	EEntryClass *klass = E_ENTRY_CLASS(object_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(object_class);

	parent_class = g_type_class_ref (PARENT_TYPE);

	object_class->set_property = et_set_property;
	object_class->get_property = et_get_property;
	object_class->dispose = e_entry_dispose;

	widget_class->realize = e_entry_realize;

	klass->changed = NULL;
	klass->activate = NULL;

	e_entry_signals[E_ENTRY_CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EEntryClass, changed),
			      NULL, NULL,
			      e_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	e_entry_signals[E_ENTRY_ACTIVATE] =
		g_signal_new ("activate",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EEntryClass, activate),
			      NULL, NULL,
			      e_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	e_entry_signals[E_ENTRY_POPULATE_POPUP] =
		g_signal_new ("populate_popup",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EEntryClass, populate_popup),
			      NULL, NULL,
			      e_marshal_NONE__POINTER_INT_OBJECT,
			      G_TYPE_NONE, 3, G_TYPE_POINTER, G_TYPE_INT, GTK_TYPE_MENU);

	e_entry_signals[E_ENTRY_COMPLETION_POPUP] =
		g_signal_new ("completion_popup",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EEntryClass, completion_popup),
			      NULL, NULL,
			      gtk_marshal_NONE__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);

	g_object_class_install_property (object_class, PROP_MODEL,
					 g_param_spec_object ("model",
							      _( "Model" ),
							      _( "Model" ),
							      E_TYPE_TEXT_MODEL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_EVENT_PROCESSOR,
					 g_param_spec_object ("event_processor",
							      _( "Event Processor" ),
							      _( "Event Processor" ),
							      E_TEXT_EVENT_PROCESSOR_TYPE,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_TEXT,
					 g_param_spec_string ("text",
							      _( "Text" ),
							      _( "Text" ),
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_FONT,
					 g_param_spec_string ("font",
							      _( "Font" ),
							      _( "Font" ),
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_FONTSET,
					 g_param_spec_string ("fontset",
							      _( "Fontset" ),
							      _( "Fontset" ),
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_FONT_GDK,
					 g_param_spec_boxed ("font_gdk",
							     _( "GDKFont" ),
							     _( "GDKFont" ),
							     GDK_TYPE_FONT,
							     G_PARAM_WRITABLE));

	g_object_class_install_property (object_class, PROP_JUSTIFICATION,
					 g_param_spec_enum ("justification",
							    _( "Justification" ),
							    _( "Justification" ),
							    GTK_TYPE_JUSTIFICATION, GTK_JUSTIFY_LEFT,
							    G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_FILL_COLOR,
					 g_param_spec_string ("fill_color",
							      _( "Fill color" ),
							      _( "Fill color" ),
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_FILL_COLOR_GDK,
					 g_param_spec_boxed ("fill_color_gdk",
							     _( "GDK fill color" ),
							     _( "GDK fill color" ),
							     GDK_TYPE_COLOR,
							     G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_FILL_COLOR_RGBA,
					 g_param_spec_uint ("fill_color_rgba",
							    _( "GDK fill color" ),
							    _( "GDK fill color" ),
							    0, G_MAXUINT, 0,
							    G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_FILL_STIPPLE,
					 g_param_spec_object ("fill_stipple",
							      _( "Fill stipple" ),
							      _( "FIll stipple" ),
							      GDK_TYPE_WINDOW,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_EDITABLE,
					 g_param_spec_boolean ("editable",
							       _( "Editable" ),
							       _( "Editable" ),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_USE_ELLIPSIS,
					 g_param_spec_boolean ("use_ellipsis",
							       _( "Use ellipsis" ),
							       _( "Use ellipsis" ),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_ELLIPSIS,
					 g_param_spec_string ("ellipsis",
							      _( "Ellipsis" ),
							      _( "Ellipsis" ),
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_LINE_WRAP,
					 g_param_spec_boolean ("line_wrap",
							       _( "Line wrap" ),
							       _( "Line wrap" ),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_BREAK_CHARACTERS,
					 g_param_spec_string ("break_characters",
							      _( "Break characters" ),
							      _( "Break characters" ),
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_MAX_LINES,
					 g_param_spec_int ("max_lines",
							   _( "Max lines" ),
							   _( "Max lines" ),
							   0, G_MAXINT, 0,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_ALLOW_NEWLINES,
					 g_param_spec_boolean ("allow_newlines",
							       _( "Allow newlines" ),
							       _( "Allow newlines" ),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_DRAW_BORDERS,
					 g_param_spec_boolean ("draw_borders",
							       _( "Draw borders" ),
							       _( "Draw borders" ),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_DRAW_BACKGROUND,
					 g_param_spec_boolean ("draw_background",
							       _( "Draw background" ),
							       _( "Draw background" ),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_DRAW_BUTTON,
					 g_param_spec_boolean ("draw_button",
							       _( "Draw button" ),
							       _( "Draw button" ),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_CURSOR_POS,
					 g_param_spec_int ("cursor_pos",
							   _( "Cursor position" ),
							   _( "Cursor position" ),
							   0, G_MAXINT, 0,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_EMULATE_LABEL_RESIZE,
					 g_param_spec_boolean ("emulate_label_resize",
							       _( "Emulate label resize" ),
							       _( "Emulate label resize" ),
							       FALSE,
							       G_PARAM_READWRITE));
}

E_MAKE_TYPE(e_entry, "EEntry", EEntry, e_entry_class_init, e_entry_init, PARENT_TYPE)
