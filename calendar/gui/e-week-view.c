/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Authors :
 *  Damon Chaplin <damon@ximian.com>
 *  Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright 1999, Ximian, Inc.
 * Copyright 2001, Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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

/*
 * EWeekView - displays the Week & Month views of the calendar.
 */

#include <config.h>

#include "e-week-view.h"

#include <math.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkselection.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkvscrollbar.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkmain.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-exec.h>
#include <libgnome/gnome-util.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnomecanvas/gnome-canvas-pixbuf.h>
#include <gal/e-text/e-text.h>
#include <gal/widgets/e-canvas-utils.h>
#include <gal/widgets/e-gui-utils.h>
#include <gal/widgets/e-unicode.h>
#include <e-util/e-categories-config.h>
#include <e-util/e-dialog-utils.h>
#include "dialogs/delete-comp.h"
#include "dialogs/delete-error.h"
#include "dialogs/send-comp.h"
#include "dialogs/cancel-comp.h"
#include "dialogs/recur-comp.h"
#include "comp-util.h"
#include "itip-utils.h"
#include "cal-util/timeutil.h"
#include "calendar-commands.h"
#include "calendar-config.h"
#include "print.h"
#include "goto.h"
#include "e-week-view-event-item.h"
#include "e-week-view-layout.h"
#include "e-week-view-main-item.h"
#include "e-week-view-titles-item.h"
#include "misc.h"

/* Images */
#include "art/bell.xpm"
#include "art/recur.xpm"
#include "art/timezone-16.xpm"
#include "art/jump.xpm"

#define E_WEEK_VIEW_SMALL_FONT_PTSIZE 7

#define E_WEEK_VIEW_JUMP_BUTTON_WIDTH	16
#define E_WEEK_VIEW_JUMP_BUTTON_HEIGHT	8

#define E_WEEK_VIEW_JUMP_BUTTON_X_PAD	3
#define E_WEEK_VIEW_JUMP_BUTTON_Y_PAD	3

/* The timeout before we do a layout, so we don't do a layout for each event
   we get from the server. */
#define E_WEEK_VIEW_LAYOUT_TIMEOUT	100


static void e_week_view_class_init (EWeekViewClass *class);
static void e_week_view_init (EWeekView *week_view);
static void e_week_view_destroy (GtkObject *object);
static void e_week_view_realize (GtkWidget *widget);
static void e_week_view_unrealize (GtkWidget *widget);
static void e_week_view_style_set (GtkWidget *widget,
				   GtkStyle  *previous_style);
static void e_week_view_size_allocate (GtkWidget *widget,
				       GtkAllocation *allocation);
static void e_week_view_recalc_cell_sizes (EWeekView *week_view);
static gint e_week_view_focus_in (GtkWidget *widget,
				  GdkEventFocus *event);
static gint e_week_view_focus_out (GtkWidget *widget,
				   GdkEventFocus *event);
static gint e_week_view_expose_event (GtkWidget *widget,
				      GdkEventExpose *event);
static gboolean e_week_view_get_next_tab_event (EWeekView *week_view,
						GtkDirectionType direction,
						gint current_event_num,
						gint current_span_num,
						gint *next_event_num,
						gint *next_span_num);
static gboolean e_week_view_focus (GtkWidget *widget,
				   GtkDirectionType direction);
static GList *e_week_view_get_selected_events (ECalView *cal_view);
static void e_week_view_get_selected_time_range (EWeekView *cal_view, time_t *start_time, time_t *end_time);
static void e_week_view_set_selected_time_range (EWeekView *cal_view, time_t start_time, time_t end_time);
static void e_week_view_update_query (EWeekView *week_view);
static void e_week_view_draw_shadow (EWeekView *week_view);

static gboolean e_week_view_on_button_press (GtkWidget *widget,
					     GdkEventButton *event,
					     EWeekView *week_view);
static gboolean e_week_view_on_button_release (GtkWidget *widget,
					       GdkEventButton *event,
					       EWeekView *week_view);
static gboolean e_week_view_on_scroll (GtkWidget *widget,
				       GdkEventScroll *scroll,
				       EWeekView *week_view);
static gboolean e_week_view_on_motion (GtkWidget *widget,
				       GdkEventMotion *event,
				       EWeekView *week_view);
static gint e_week_view_convert_position_to_day (EWeekView *week_view,
						 gint x,
						 gint y);
static void e_week_view_update_selection (EWeekView *week_view,
					  gint day);

static void e_week_view_free_events (EWeekView *week_view);
static gboolean e_week_view_add_event (CalComponent *comp,
				       time_t	  start,
				       time_t	  end,
				       gpointer	  data);
static void e_week_view_check_layout (EWeekView *week_view);
static void e_week_view_ensure_events_sorted (EWeekView *week_view);
static void e_week_view_reshape_events (EWeekView *week_view);
static void e_week_view_reshape_event_span (EWeekView *week_view,
					    gint event_num,
					    gint span_num);
static void e_week_view_recalc_day_starts (EWeekView *week_view,
					   time_t lower);
static void e_week_view_on_adjustment_changed (GtkAdjustment *adjustment,
					       EWeekView *week_view);
static void e_week_view_on_editing_started (EWeekView *week_view,
					    GnomeCanvasItem *item);
static void e_week_view_on_editing_stopped (EWeekView *week_view,
					    GnomeCanvasItem *item);
static gboolean e_week_view_find_event_from_item (EWeekView	  *week_view,
						  GnomeCanvasItem *item,
						  gint		  *event_num,
						  gint		  *span_num);
static gboolean e_week_view_find_event_from_uid (EWeekView	  *week_view,
						 const gchar	  *uid,
						 gint		  *event_num_return);
typedef gboolean (* EWeekViewForeachEventCallback) (EWeekView *week_view,
						    gint event_num,
						    gpointer data);

static void e_week_view_foreach_event_with_uid (EWeekView *week_view,
						const gchar *uid,
						EWeekViewForeachEventCallback callback,
						gpointer data);
static gboolean e_week_view_on_text_item_event (GnomeCanvasItem *item,
						GdkEvent *event,
						EWeekView *week_view);
static gboolean e_week_view_on_jump_button_event (GnomeCanvasItem *item,
						  GdkEvent *event,
						  EWeekView *week_view);
static gboolean e_week_view_key_press (GtkWidget *widget, GdkEventKey *event);
static gboolean e_week_view_do_key_press (GtkWidget *widget,
					  GdkEventKey *event);
static gboolean e_week_view_popup_menu (GtkWidget *widget);

static void e_week_view_on_new_appointment (GtkWidget *widget,
					    gpointer data);
static void e_week_view_on_new_event       (GtkWidget *widget,
					   gpointer data);
static void e_week_view_on_new_meeting (GtkWidget *widget,
					gpointer data);
static void e_week_view_on_new_task (GtkWidget *widget,
				     gpointer data);
static void e_week_view_on_goto_today      (GtkWidget *widget,
					   gpointer data);
static void e_week_view_on_goto_date       (GtkWidget *widget,
					   gpointer data);
static void e_week_view_on_edit_appointment (GtkWidget *widget,
					     gpointer data);
static void e_week_view_on_save_as (GtkWidget *widget,
				    gpointer data);
static void e_week_view_on_print (GtkWidget *widget,
				  gpointer data);
static void e_week_view_on_print_event (GtkWidget *widget,
					gpointer data);
static void e_week_view_on_meeting (GtkWidget *widget,
				    gpointer data);
static void e_week_view_on_forward (GtkWidget *widget,
				    gpointer data);
static void e_week_view_on_publish (GtkWidget *widget,
				    gpointer data);
static void e_week_view_on_settings (GtkWidget *widget,
				     gpointer data);
static void e_week_view_on_delete_occurrence (GtkWidget *widget,
					      gpointer data);
static void e_week_view_on_delete_appointment (GtkWidget *widget,
					       gpointer data);
static void e_week_view_on_cut (GtkWidget *widget, gpointer data);
static void e_week_view_on_copy (GtkWidget *widget, gpointer data);
static void e_week_view_on_paste (GtkWidget *widget, gpointer data);
static void e_week_view_on_unrecur_appointment (GtkWidget *widget,
						gpointer data);

static gboolean e_week_view_update_event_cb (EWeekView *week_view,
					     gint event_num,
					     gpointer data);
static gboolean e_week_view_remove_event_cb (EWeekView *week_view,
					     gint event_num,
					     gpointer data);
static gboolean e_week_view_recalc_display_start_day	(EWeekView	*week_view);

static void e_week_view_queue_layout (EWeekView *week_view);
static void e_week_view_cancel_layout (EWeekView *week_view);
static gboolean e_week_view_layout_timeout_cb (gpointer data);

static ECalViewClass *parent_class;

E_MAKE_TYPE (e_week_view, "EWeekView", EWeekView, e_week_view_class_init,
	     e_week_view_init, e_cal_view_get_type ());

static void
e_week_view_class_init (EWeekViewClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	ECalViewClass *view_class;

	parent_class = g_type_class_peek_parent (class);
	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;
	view_class = (ECalViewClass *) class;

	/* Method override */
	object_class->destroy		= e_week_view_destroy;

	widget_class->realize		= e_week_view_realize;
	widget_class->unrealize		= e_week_view_unrealize;
	widget_class->style_set		= e_week_view_style_set;
 	widget_class->size_allocate	= e_week_view_size_allocate;
	widget_class->focus_in_event	= e_week_view_focus_in;
	widget_class->focus_out_event	= e_week_view_focus_out;
	widget_class->key_press_event	= e_week_view_key_press;
	widget_class->popup_menu        = e_week_view_popup_menu;
	widget_class->expose_event	= e_week_view_expose_event;
	widget_class->focus             = e_week_view_focus;

	view_class->get_selected_events = e_week_view_get_selected_events;
	view_class->get_selected_time_range = e_week_view_get_selected_time_range;
	view_class->set_selected_time_range = e_week_view_set_selected_time_range;
	view_class->update_query        = e_week_view_update_query;
}


static void
e_week_view_init (EWeekView *week_view)
{
	GnomeCanvasGroup *canvas_group;
	GtkObject *adjustment;
	GdkPixbuf *pixbuf;
	gint i;

	GTK_WIDGET_SET_FLAGS (week_view, GTK_CAN_FOCUS);

	week_view->query = NULL;

	week_view->events = g_array_new (FALSE, FALSE,
					 sizeof (EWeekViewEvent));
	week_view->events_sorted = TRUE;
	week_view->events_need_layout = FALSE;
	week_view->events_need_reshape = FALSE;

	week_view->layout_timeout_id = 0;

	week_view->spans = NULL;

	week_view->zone = NULL;
	week_view->multi_week_view = FALSE;
	week_view->weeks_shown = 6;
	week_view->rows = 6;
	week_view->columns = 2;
	week_view->compress_weekend = TRUE;
	week_view->show_event_end_times = TRUE;
	week_view->week_start_day = 0;		/* Monday. */
	week_view->display_start_day = 0;	/* Monday. */

	g_date_clear (&week_view->base_date, 1);
	g_date_clear (&week_view->first_day_shown, 1);

	week_view->row_height = 10;
	week_view->rows_per_cell = 1;

	week_view->selection_start_day = -1;
	week_view->selection_drag_pos = E_WEEK_VIEW_DRAG_NONE;

	week_view->pressed_event_num = -1;
	week_view->editing_event_num = -1;

	week_view->main_gc = NULL;

	week_view->default_category = NULL;

	/* Create the small font. */
	week_view->use_small_font = TRUE;

	week_view->small_font_desc =
		pango_font_description_copy (gtk_widget_get_style (GTK_WIDGET (week_view))->font_desc);
	pango_font_description_set_size (week_view->small_font_desc,
					 E_WEEK_VIEW_SMALL_FONT_PTSIZE * PANGO_SCALE);

	/* String to use in 12-hour time format for times in the morning. */
	week_view->am_string = _("am");

	/* String to use in 12-hour time format for times in the afternoon. */
	week_view->pm_string = _("pm");


	/*
	 * Titles Canvas. Note that we don't show it is only shown in the
	 * Month view.
	 */
	week_view->titles_canvas = e_canvas_new ();
	gtk_table_attach (GTK_TABLE (week_view), week_view->titles_canvas,
			  1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

	canvas_group = GNOME_CANVAS_GROUP (GNOME_CANVAS (week_view->titles_canvas)->root);

	week_view->titles_canvas_item =
		gnome_canvas_item_new (canvas_group,
				       e_week_view_titles_item_get_type (),
				       "EWeekViewTitlesItem::week_view", week_view,
				       NULL);

	/*
	 * Main Canvas
	 */
	week_view->main_canvas = e_canvas_new ();
	gtk_table_attach (GTK_TABLE (week_view), week_view->main_canvas,
			  1, 2, 1, 2,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 1, 1);
	gtk_widget_show (week_view->main_canvas);

	canvas_group = GNOME_CANVAS_GROUP (GNOME_CANVAS (week_view->main_canvas)->root);

	week_view->main_canvas_item =
		gnome_canvas_item_new (canvas_group,
				       e_week_view_main_item_get_type (),
				       "EWeekViewMainItem::week_view", week_view,
				       NULL);

	g_signal_connect_after (week_view->main_canvas, "button_press_event",
				G_CALLBACK (e_week_view_on_button_press), week_view);
	g_signal_connect (week_view->main_canvas, "button_release_event",
			  G_CALLBACK (e_week_view_on_button_release), week_view);
	g_signal_connect (week_view->main_canvas, "scroll_event",
			  G_CALLBACK (e_week_view_on_scroll), week_view);
	g_signal_connect (week_view->main_canvas, "motion_notify_event",
			  G_CALLBACK (e_week_view_on_motion), week_view);

	/* Create the buttons to jump to each days. */
	pixbuf = gdk_pixbuf_new_from_xpm_data ((const char**) jump_xpm);

	for (i = 0; i < E_WEEK_VIEW_MAX_WEEKS * 7; i++) {
		week_view->jump_buttons[i] = gnome_canvas_item_new
			(canvas_group,
			 gnome_canvas_pixbuf_get_type (),
			 "GnomeCanvasPixbuf::pixbuf", pixbuf,
			 NULL);

		g_signal_connect (week_view->jump_buttons[i], "event",
				  G_CALLBACK (e_week_view_on_jump_button_event), week_view);
	}
	gdk_pixbuf_unref (pixbuf);

	/*
	 * Scrollbar.
	 */
	adjustment = gtk_adjustment_new (0, -52, 52, 1, 1, 1);
	gtk_signal_connect (adjustment, "value_changed",
			    G_CALLBACK (e_week_view_on_adjustment_changed),
			    week_view);

	week_view->vscrollbar = gtk_vscrollbar_new (GTK_ADJUSTMENT (adjustment));
	gtk_table_attach (GTK_TABLE (week_view), week_view->vscrollbar,
			  2, 3, 1, 2, 0, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show (week_view->vscrollbar);

	/* Create the cursors. */
	week_view->normal_cursor = gdk_cursor_new (GDK_LEFT_PTR);
	week_view->move_cursor = gdk_cursor_new (GDK_FLEUR);
	week_view->resize_width_cursor = gdk_cursor_new (GDK_SB_H_DOUBLE_ARROW);
	week_view->last_cursor_set = NULL;
}


/**
 * e_week_view_new:
 * @Returns: a new #EWeekView.
 *
 * Creates a new #EWeekView.
 **/
GtkWidget *
e_week_view_new (void)
{
	GtkWidget *week_view;

	week_view = GTK_WIDGET (g_object_new (e_week_view_get_type (), NULL));

	return week_view;
}


static void
e_week_view_destroy (GtkObject *object)
{
	EWeekView *week_view;

	week_view = E_WEEK_VIEW (object);

	e_week_view_cancel_layout (week_view);

	if (week_view->events) {
		e_week_view_free_events (week_view);
		g_array_free (week_view->events, TRUE);
		week_view->events = NULL;
	}
	
	if (e_cal_view_get_cal_client (E_CAL_VIEW (week_view))) {
		g_signal_handlers_disconnect_matched (
			e_cal_view_get_cal_client (E_CAL_VIEW (week_view)), G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, week_view);
	}

	if (week_view->query) {
		g_signal_handlers_disconnect_matched (week_view->query, G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, week_view);
		g_object_unref (week_view->query);
		week_view->query = NULL;
	}

	if (week_view->small_font_desc) {
		pango_font_description_free (week_view->small_font_desc);
		week_view->small_font_desc = NULL;
	}

	if (week_view->default_category) {
		g_free (week_view->default_category);
		week_view->default_category = NULL;
	}

	if (week_view->normal_cursor) {
		gdk_cursor_unref (week_view->normal_cursor);
		week_view->normal_cursor = NULL;
	}
	if (week_view->move_cursor) {
		gdk_cursor_unref (week_view->move_cursor);
		week_view->move_cursor = NULL;
	}
	if (week_view->resize_width_cursor) {
		gdk_cursor_unref (week_view->resize_width_cursor);
		week_view->resize_width_cursor = NULL;
	}

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}


static void
e_week_view_realize (GtkWidget *widget)
{
	EWeekView *week_view;
	GdkColormap *colormap;
	gboolean success[E_WEEK_VIEW_COLOR_LAST];
	gint nfailed;

	if (GTK_WIDGET_CLASS (parent_class)->realize)
		(*GTK_WIDGET_CLASS (parent_class)->realize)(widget);

	week_view = E_WEEK_VIEW (widget);
	week_view->main_gc = gdk_gc_new (widget->window);

	colormap = gtk_widget_get_colormap (widget);

	/* Allocate the colors. */
	week_view->colors[E_WEEK_VIEW_COLOR_EVEN_MONTHS].red   = 0xe0e0;
	week_view->colors[E_WEEK_VIEW_COLOR_EVEN_MONTHS].green = 0xe0e0;
	week_view->colors[E_WEEK_VIEW_COLOR_EVEN_MONTHS].blue  = 0xe0e0;

	week_view->colors[E_WEEK_VIEW_COLOR_ODD_MONTHS].red   = 65535;
	week_view->colors[E_WEEK_VIEW_COLOR_ODD_MONTHS].green = 65535;
	week_view->colors[E_WEEK_VIEW_COLOR_ODD_MONTHS].blue  = 65535;

	week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BACKGROUND].red   = 213 * 257;
	week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BACKGROUND].green = 213 * 257;
	week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BACKGROUND].blue  = 213 * 257;

	week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BORDER].red   = 0;
	week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BORDER].green = 0;
	week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BORDER].blue  = 0;

	week_view->colors[E_WEEK_VIEW_COLOR_EVENT_TEXT].red   = 0;
	week_view->colors[E_WEEK_VIEW_COLOR_EVENT_TEXT].green = 0;
	week_view->colors[E_WEEK_VIEW_COLOR_EVENT_TEXT].blue  = 0;

	week_view->colors[E_WEEK_VIEW_COLOR_GRID].red   = 0 * 257;
	week_view->colors[E_WEEK_VIEW_COLOR_GRID].green = 0 * 257;
	week_view->colors[E_WEEK_VIEW_COLOR_GRID].blue  = 0 * 257;

	week_view->colors[E_WEEK_VIEW_COLOR_SELECTED].red   = 0 * 257;
	week_view->colors[E_WEEK_VIEW_COLOR_SELECTED].green = 0 * 257;
	week_view->colors[E_WEEK_VIEW_COLOR_SELECTED].blue  = 156 * 257;

	week_view->colors[E_WEEK_VIEW_COLOR_SELECTED_UNFOCUSSED].red   = 16 * 257;
	week_view->colors[E_WEEK_VIEW_COLOR_SELECTED_UNFOCUSSED].green = 78 * 257;
	week_view->colors[E_WEEK_VIEW_COLOR_SELECTED_UNFOCUSSED].blue  = 139 * 257;

	week_view->colors[E_WEEK_VIEW_COLOR_DATES].red   = 0 * 257;
	week_view->colors[E_WEEK_VIEW_COLOR_DATES].green = 0 * 257;
	week_view->colors[E_WEEK_VIEW_COLOR_DATES].blue  = 0 * 257;

	week_view->colors[E_WEEK_VIEW_COLOR_DATES_SELECTED].red   = 65535;
	week_view->colors[E_WEEK_VIEW_COLOR_DATES_SELECTED].green = 65535;
	week_view->colors[E_WEEK_VIEW_COLOR_DATES_SELECTED].blue  = 65535;

	week_view->colors[E_WEEK_VIEW_COLOR_TODAY].red   = 65535;
	week_view->colors[E_WEEK_VIEW_COLOR_TODAY].green = 0;
	week_view->colors[E_WEEK_VIEW_COLOR_TODAY].blue  = 0;

	nfailed = gdk_colormap_alloc_colors (colormap, week_view->colors,
					     E_WEEK_VIEW_COLOR_LAST, FALSE,
					     TRUE, success);
	if (nfailed)
		g_warning ("Failed to allocate all colors");


	/* Create the pixmaps. */
	week_view->reminder_icon = gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap, &week_view->reminder_mask, NULL, bell_xpm);
	week_view->recurrence_icon = gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap, &week_view->recurrence_mask, NULL, recur_xpm);
	week_view->timezone_icon = gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap, &week_view->timezone_mask, NULL, timezone_16_xpm);
}


static void
e_week_view_unrealize (GtkWidget *widget)
{
	EWeekView *week_view;
	GdkColormap *colormap;

	week_view = E_WEEK_VIEW (widget);

	gdk_gc_unref (week_view->main_gc);
	week_view->main_gc = NULL;

	colormap = gtk_widget_get_colormap (widget);
	gdk_colormap_free_colors (colormap, week_view->colors, E_WEEK_VIEW_COLOR_LAST);

	gdk_pixmap_unref (week_view->reminder_icon);
	week_view->reminder_icon = NULL;
	gdk_pixmap_unref (week_view->recurrence_icon);
	week_view->recurrence_icon = NULL;

	if (GTK_WIDGET_CLASS (parent_class)->unrealize)
		(*GTK_WIDGET_CLASS (parent_class)->unrealize)(widget);
}

static gint
get_string_width (PangoLayout *layout, const gchar *string)
{
	gint width;

	pango_layout_set_text (layout, string, -1);
	pango_layout_get_pixel_size (layout, &width, NULL);
	return width;
}

/* FIXME: This is also needed in e-day-view-time-item.c. We should probably use
 * pango's approximation function, but it needs a language tag. Find out how to
 * get one of those properly. */
static gint
get_digit_width (PangoLayout *layout)
{
	gint digit;
	gint max_digit_width = 1;

	for (digit = '0'; digit <= '9'; digit++) {
		gchar digit_char;
		gint  digit_width;

		digit_char = digit;

		pango_layout_set_text (layout, &digit_char, 1);
		pango_layout_get_pixel_size (layout, &digit_width, NULL);

		max_digit_width = MAX (max_digit_width, digit_width);
	}

	return max_digit_width;
}

static void
e_week_view_style_set (GtkWidget *widget,
		       GtkStyle  *previous_style)
{
	EWeekView *week_view;
	GtkStyle *style;
	gint day, day_width, max_day_width, max_abbr_day_width;
	gint month, month_width, max_month_width, max_abbr_month_width;
	GDate date;
	gchar buffer[128];
	PangoFontDescription *font_desc;
	PangoContext *pango_context;
	PangoFontMetrics *font_metrics;
	PangoLayout *layout;

	if (GTK_WIDGET_CLASS (parent_class)->style_set)
		(*GTK_WIDGET_CLASS (parent_class)->style_set)(widget, previous_style);

	week_view = E_WEEK_VIEW (widget);
	style = gtk_widget_get_style (widget);

	/* Set up Pango prerequisites */
	font_desc = style->font_desc;
	pango_context = gtk_widget_get_pango_context (widget);
	font_metrics = pango_context_get_metrics (pango_context, font_desc,
						  pango_context_get_language (pango_context));
	layout = pango_layout_new (pango_context);

	/* Recalculate the height of each row based on the font size. */
	week_view->row_height = PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics)) +
		PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics)) +
		E_WEEK_VIEW_EVENT_BORDER_HEIGHT * 2 + E_WEEK_VIEW_EVENT_TEXT_Y_PAD * 2;
	week_view->row_height = MAX (week_view->row_height, E_WEEK_VIEW_ICON_HEIGHT + E_WEEK_VIEW_ICON_Y_PAD + E_WEEK_VIEW_EVENT_BORDER_HEIGHT * 2);

	/* Check that the small font is smaller than the default font.
	   If it isn't, we won't use it. */
	if (week_view->small_font_desc) {
		if (PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics)) +
		    PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics))
		    <= E_WEEK_VIEW_SMALL_FONT_PTSIZE)
			week_view->use_small_font = FALSE;
	}

	/* Set the height of the top canvas. */
	gtk_widget_set_usize (week_view->titles_canvas, -1,
			      PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics)) +
			      PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics)) + 5);

	/* Save the sizes of various strings in the font, so we can quickly
	   decide which date formats to use. */
	g_date_clear (&date, 1);
	g_date_set_dmy (&date, 27, 3, 2000);	/* Must be a Monday. */

	max_day_width = 0;
	max_abbr_day_width = 0;
	for (day = 0; day < 7; day++) {
		g_date_strftime (buffer, 128, "%A", &date);
		day_width = get_string_width (layout, buffer);
		week_view->day_widths[day] = day_width;
		max_day_width = MAX (max_day_width, day_width);

		g_date_strftime (buffer, 128, "%a", &date);
		day_width = get_string_width (layout, buffer);
		week_view->abbr_day_widths[day] = day_width;
		max_abbr_day_width = MAX (max_abbr_day_width, day_width);

		g_date_add_days (&date, 1);
	}

	max_month_width = 0;
	max_abbr_month_width = 0;
	for (month = 0; month < 12; month++) {
		g_date_set_month (&date, month + 1);

		g_date_strftime (buffer, 128, "%B", &date);
		month_width = get_string_width (layout, buffer);
		week_view->month_widths[month] = month_width;
		max_month_width = MAX (max_month_width, month_width);

		g_date_strftime (buffer, 128, "%b", &date);
		month_width = get_string_width (layout, buffer);
		week_view->abbr_month_widths[month] = month_width;
		max_abbr_month_width = MAX (max_abbr_month_width, month_width);
	}

	week_view->space_width = get_string_width (layout, " ");
	week_view->colon_width = get_string_width (layout, ":");
	week_view->slash_width = get_string_width (layout, "/");
	week_view->digit_width = get_digit_width (layout);
	if (week_view->small_font_desc) {
		pango_layout_set_font_description (layout, week_view->small_font_desc);
		week_view->small_digit_width = get_digit_width (layout);
		pango_layout_set_font_description (layout, style->font_desc);
	}
	week_view->max_day_width = max_day_width;
	week_view->max_abbr_day_width = max_abbr_day_width;
	week_view->max_month_width = max_month_width;
	week_view->max_abbr_month_width = max_abbr_month_width;

	week_view->am_string_width = get_string_width (layout,
						       week_view->am_string);
	week_view->pm_string_width = get_string_width (layout,
						       week_view->pm_string);

	g_object_unref (layout);
	pango_font_metrics_unref (font_metrics);
}


/* This recalculates the sizes of each column. */
static void
e_week_view_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	EWeekView *week_view;
	gdouble old_x2, old_y2, new_x2, new_y2;

	week_view = E_WEEK_VIEW (widget);

	(*GTK_WIDGET_CLASS (parent_class)->size_allocate) (widget, allocation);

	e_week_view_recalc_cell_sizes (week_view);

	/* Set the scroll region of the top canvas to its allocated size. */
	gnome_canvas_get_scroll_region (GNOME_CANVAS (week_view->titles_canvas),
					NULL, NULL, &old_x2, &old_y2);
	new_x2 = week_view->titles_canvas->allocation.width - 1;
	new_y2 = week_view->titles_canvas->allocation.height - 1;
	if (old_x2 != new_x2 || old_y2 != new_y2)
		gnome_canvas_set_scroll_region (GNOME_CANVAS (week_view->titles_canvas),
						0, 0, new_x2, new_y2);


	/* Set the scroll region of the main canvas to its allocated width,
	   but with the height depending on the number of rows needed. */
	gnome_canvas_get_scroll_region (GNOME_CANVAS (week_view->main_canvas),
					NULL, NULL, &old_x2, &old_y2);
	new_x2 = week_view->main_canvas->allocation.width - 1;
	new_y2 = week_view->main_canvas->allocation.height - 1;
	if (old_x2 != new_x2 || old_y2 != new_y2)
		gnome_canvas_set_scroll_region (GNOME_CANVAS (week_view->main_canvas),
						0, 0, new_x2, new_y2);

	/* Flag that we need to reshape the events. */
	if (old_x2 != new_x2 || old_y2 != new_y2) {
		week_view->events_need_reshape = TRUE;
		e_week_view_check_layout (week_view);
	}
}


static void
e_week_view_recalc_cell_sizes (EWeekView *week_view)
{
	gfloat canvas_width, canvas_height, offset;
	gint row, col;
	GtkWidget *widget;
	GtkStyle *style;
	gint width, height, time_width;
	PangoFontDescription *font_desc;
	PangoContext *pango_context;
	PangoFontMetrics *font_metrics;

	if (week_view->multi_week_view) {
		week_view->rows = week_view->weeks_shown * 2;
		week_view->columns = week_view->compress_weekend ? 6 : 7;
	} else {
		week_view->rows = 6;
		week_view->columns = 2;
	}

	/* Calculate the column sizes, using floating point so that pixels
	   get divided evenly. Note that we use one more element than the
	   number of columns, to make it easy to get the column widths.
	   We also add one to the width so that the right border of the last
	   column is off the edge of the displayed area. */
	canvas_width = week_view->main_canvas->allocation.width + 1;
	canvas_width /= week_view->columns;
	offset = 0;
	for (col = 0; col <= week_view->columns; col++) {
		week_view->col_offsets[col] = floor (offset + 0.5);
		offset += canvas_width;
	}

	/* Calculate the cell widths based on the offsets. */
	for (col = 0; col < week_view->columns; col++) {
		week_view->col_widths[col] = week_view->col_offsets[col + 1]
			- week_view->col_offsets[col];
	}

	/* Now do the same for the row heights. */
	canvas_height = week_view->main_canvas->allocation.height + 1;
	canvas_height /= week_view->rows;
	offset = 0;
	for (row = 0; row <= week_view->rows; row++) {
		week_view->row_offsets[row] = floor (offset + 0.5);
		offset += canvas_height;
	}

	/* Calculate the cell heights based on the offsets. */
	for (row = 0; row < week_view->rows; row++) {
		week_view->row_heights[row] = week_view->row_offsets[row + 1]
			- week_view->row_offsets[row];
	}


	/* If the font hasn't been set yet just return. */
	widget = GTK_WIDGET (week_view);
	style = gtk_widget_get_style (widget);
	if (!style)
		return;
	font_desc = style->font_desc;
	if (!font_desc)
		return;

	pango_context = gtk_widget_get_pango_context (widget);
	font_metrics = pango_context_get_metrics (pango_context, font_desc,
						  pango_context_get_language (pango_context));


	/* Calculate the number of rows of events in each cell, for the large
	   cells and the compressed weekend cells. */
	if (week_view->multi_week_view) {
		week_view->events_y_offset = E_WEEK_VIEW_DATE_T_PAD +
			+ PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics))
			+ PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics))
			+ E_WEEK_VIEW_DATE_B_PAD;
	} else {
		week_view->events_y_offset = E_WEEK_VIEW_DATE_T_PAD
			+ PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics))
			+ PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics))
			+ E_WEEK_VIEW_DATE_LINE_T_PAD + 1
			+ E_WEEK_VIEW_DATE_LINE_B_PAD;
	}

	height = week_view->row_heights[0];
	week_view->rows_per_cell = (height * 2 - week_view->events_y_offset)
		/ (week_view->row_height + E_WEEK_VIEW_EVENT_Y_SPACING);
	week_view->rows_per_cell = MIN (week_view->rows_per_cell,
					E_WEEK_VIEW_MAX_ROWS_PER_CELL);

	week_view->rows_per_compressed_cell =
		(height - week_view->events_y_offset)
		/ (week_view->row_height + E_WEEK_VIEW_EVENT_Y_SPACING);
	week_view->rows_per_compressed_cell = MIN (week_view->rows_per_compressed_cell,
						   E_WEEK_VIEW_MAX_ROWS_PER_CELL);

	/* Determine which time format to use, based on the width of the cells.
	   We only allow the time to take up about half of the width. */
	width = week_view->col_widths[0];

	time_width = e_week_view_get_time_string_width (week_view);

	week_view->time_format = E_WEEK_VIEW_TIME_NONE;
	if (week_view->use_small_font && week_view->small_font_desc) {
		if (week_view->show_event_end_times
		    && width / 2 > time_width * 2 + E_WEEK_VIEW_EVENT_TIME_SPACING)
			week_view->time_format = E_WEEK_VIEW_TIME_BOTH_SMALL_MIN;
		else if (width / 2 > time_width)
			week_view->time_format = E_WEEK_VIEW_TIME_START_SMALL_MIN;
	} else {
		if (week_view->show_event_end_times
		    && width / 2 > time_width * 2 + E_WEEK_VIEW_EVENT_TIME_SPACING)
			week_view->time_format = E_WEEK_VIEW_TIME_BOTH;
		else if (width / 2 > time_width)
			week_view->time_format = E_WEEK_VIEW_TIME_START;
	}

	pango_font_metrics_unref (font_metrics);
}


static gint
e_week_view_focus_in (GtkWidget *widget, GdkEventFocus *event)
{
	EWeekView *week_view;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (E_IS_WEEK_VIEW (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	week_view = E_WEEK_VIEW (widget);

	GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);

	gtk_widget_queue_draw (week_view->main_canvas);

	return FALSE;
}


static gint
e_week_view_focus_out (GtkWidget *widget, GdkEventFocus *event)
{
	EWeekView *week_view;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (E_IS_WEEK_VIEW (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	week_view = E_WEEK_VIEW (widget);

	GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);

	gtk_widget_queue_draw (week_view->main_canvas);

	return FALSE;
}


/* This draws a shadow around the top display and main display. */
static gint
e_week_view_expose_event (GtkWidget *widget,
			  GdkEventExpose *event)
{
	EWeekView *week_view;

	week_view = E_WEEK_VIEW (widget);

	e_week_view_draw_shadow (week_view);

	if (GTK_WIDGET_CLASS (parent_class)->expose_event)
		(*GTK_WIDGET_CLASS (parent_class)->expose_event)(widget, event);

	return FALSE;
}


static gboolean
e_week_view_get_next_tab_event (EWeekView *week_view,
				GtkDirectionType direction,
				gint current_event_num,
				gint current_span_num,
				gint *next_event_num,
				gint *next_span_num)
{
	gint event_num;

	g_return_val_if_fail (week_view != NULL, FALSE);
	g_return_val_if_fail (next_event_num != NULL, FALSE);
	g_return_val_if_fail (next_span_num != NULL, FALSE);

	if (week_view->events->len <= 0)
		return FALSE;

	/* we only tab through events not spans */
	*next_span_num = 0;

	switch (direction) {
	case GTK_DIR_TAB_BACKWARD:
		event_num = current_event_num - 1;
		break;
	case GTK_DIR_TAB_FORWARD:
		event_num = current_event_num + 1;
		break;
	default:
		return FALSE;
	}

	if (event_num < 0)
		*next_event_num = week_view->events->len - 1;
	else if (event_num >= week_view->events->len)
		*next_event_num = 0;
	else
		*next_event_num = event_num;
	return TRUE;
}

static gboolean
e_week_view_focus (GtkWidget *widget, GtkDirectionType direction)
{
	EWeekView *week_view;
	gint new_event_num;
	gint new_span_num;
	gint current_event_num;
	gint current_span_num;
	gint event_loop;
	gboolean editable = FALSE;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (E_IS_WEEK_VIEW (widget), FALSE);

	week_view = E_WEEK_VIEW (widget);
	current_event_num = week_view->editing_event_num;
	current_span_num = week_view->editing_span_num;
	for (event_loop = 0; event_loop < week_view->events->len;
	     ++event_loop) {
		if (!e_week_view_get_next_tab_event (week_view, direction,
						     current_event_num,
						     current_span_num,
						     &new_event_num,
						     &new_span_num))
			return FALSE;

		editable = e_week_view_start_editing_event (week_view,
							    new_event_num,
							    new_span_num,
							    NULL);
		if (editable)
			break;
		current_event_num = new_event_num;
		current_span_num = new_span_num;
	}
	return editable;
}

/* Returns the currently-selected event, or NULL if none */
static GList *
e_week_view_get_selected_events (ECalView *cal_view)
{
	EWeekViewEvent *event = NULL;
	GList *list = NULL;
	EWeekView *week_view = (EWeekView *) cal_view;

	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), NULL);

	if (week_view->editing_event_num != -1) {
		event = &g_array_index (week_view->events, EWeekViewEvent,
					week_view->editing_event_num);
	} else if (week_view->popup_event_num != -1) {
		event = &g_array_index (week_view->events, EWeekViewEvent,
					week_view->popup_event_num);
	}

	if (event)
		list = g_list_prepend (list, event->comp);

	return list;
}

/* Callback used when a component is updated in the live query */
static void
query_obj_updated_cb (CalQuery *query, const char *uid,
		      gboolean query_in_progress, int n_scanned, int total,
		      gpointer data)
{
	EWeekView *week_view;
	EWeekViewEvent *event;
	gint event_num, num_days;
	CalComponent *comp;
	CalClientGetStatus status;

	week_view = E_WEEK_VIEW (data);

	/* If we don't have a valid date set yet, just return. */
	if (!g_date_valid (&week_view->first_day_shown))
		return;

	/* Get the event from the server. */
	status = cal_client_get_object (e_cal_view_get_cal_client (E_CAL_VIEW (week_view)), uid, &comp);

	switch (status) {
	case CAL_CLIENT_GET_SUCCESS:
		/* Everything is fine */
		break;

	case CAL_CLIENT_GET_SYNTAX_ERROR:
		g_message ("obj_updated_cb(): Syntax error when getting object `%s'", uid);
		return;

	case CAL_CLIENT_GET_NOT_FOUND:
		/* The object is no longer in the server, so do nothing */
		return;
	}

	/* If the event already exists and the dates didn't change, we can
	   update the event fairly easily without changing the events arrays
	   or computing a new layout. */
	if (e_week_view_find_event_from_uid (week_view, uid, &event_num)) {
		event = &g_array_index (week_view->events, EWeekViewEvent,
					event_num);

		if (!cal_component_has_recurrences (comp)
		    && !cal_component_has_recurrences (event->comp)
		    && cal_component_event_dates_match (comp, event->comp)) {
#if 0
			g_print ("updated object's dates unchanged\n");
#endif
			e_week_view_foreach_event_with_uid (week_view, uid, e_week_view_update_event_cb, comp);
			g_object_unref (comp);
			gtk_widget_queue_draw (week_view->main_canvas);
			return;
		}

		/* The dates have changed, so we need to remove the
		   old occurrrences before adding the new ones. */
#if 0
		g_print ("dates changed - removing occurrences\n");
#endif
		e_week_view_foreach_event_with_uid (week_view, uid,
						    e_week_view_remove_event_cb,
						    NULL);
	}

	/* Add the occurrences of the event. */
	num_days = week_view->multi_week_view ? week_view->weeks_shown * 7 : 7;

	cal_recur_generate_instances (comp,
				      week_view->day_starts[0],
				      week_view->day_starts[num_days],
				      e_week_view_add_event, week_view,
				      cal_client_resolve_tzid_cb,
				      e_cal_view_get_cal_client (E_CAL_VIEW (week_view)),
				      week_view->zone);

	g_object_unref (comp);

	e_week_view_queue_layout (week_view);
}

/* Callback used when a component is removed from the live query */
static void
query_obj_removed_cb (CalClient *client, const char *uid, gpointer data)
{
	EWeekView *week_view;

	week_view = E_WEEK_VIEW (data);

	e_week_view_foreach_event_with_uid (week_view, uid,
					    e_week_view_remove_event_cb, NULL);

	gtk_widget_queue_draw (week_view->main_canvas);
	e_week_view_check_layout (week_view);
}

/* Callback used when a query ends */
static void
query_query_done_cb (CalQuery *query, CalQueryDoneStatus status, const char *error_str, gpointer data)
{
	EWeekView *week_view;

	week_view = E_WEEK_VIEW (data);

	/* FIXME */

	e_cal_view_set_status_message (E_CAL_VIEW (week_view), NULL);

	if (status != CAL_QUERY_DONE_SUCCESS)
		fprintf (stderr, "query done: %s\n", error_str);

	gtk_widget_queue_draw (week_view->main_canvas);
}

/* Callback used when an evaluation error occurs when running a query */
static void
query_eval_error_cb (CalQuery *query, const char *error_str, gpointer data)
{
	EWeekView *week_view;

	week_view = E_WEEK_VIEW (data);

	/* FIXME */

	e_cal_view_set_status_message (E_CAL_VIEW (week_view), NULL);

	fprintf (stderr, "eval error: %s\n", error_str);

	gtk_widget_queue_draw (week_view->main_canvas);
}

/* Builds a complete query sexp for the week view by adding the predicates to
 * filter only for VEVENTS that fit in the week view's time range.
 */
static char *
adjust_query_sexp (EWeekView *week_view, const char *sexp)
{
	int num_days;
	char *start, *end;
	char *new_sexp;

	/* If the dates have not been set yet, we just want an empty query. */
	if (!g_date_valid (&week_view->first_day_shown))
		return NULL;

	num_days = week_view->multi_week_view ? week_view->weeks_shown * 7 : 7;

	start = isodate_from_time_t (week_view->day_starts[0]);
	end = isodate_from_time_t (week_view->day_starts[num_days]);

	new_sexp = g_strdup_printf ("(and (= (get-vtype) \"VEVENT\")"
				    "     (occur-in-time-range? (make-time \"%s\")"
				    "                           (make-time \"%s\"))"
				    "     %s)",
				    start, end,
				    sexp);

	g_free (start);
	g_free (end);

	return new_sexp;
}

/* Restarts a query for the week view */
static void
e_week_view_update_query (EWeekView *week_view)
{
	CalQuery *old_query;
	char *real_sexp;

	gtk_widget_queue_draw (week_view->main_canvas);
	e_week_view_free_events (week_view);
	e_week_view_queue_layout (week_view);

	old_query = week_view->query;
	week_view->query = NULL;

	if (old_query) {
		g_signal_handlers_disconnect_matched (old_query, G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, week_view);
		g_object_unref (old_query);
	}

	g_assert (e_cal_view_get_query (E_CAL_VIEW (week_view)) != NULL);

	real_sexp = adjust_query_sexp (week_view, e_cal_view_get_query (E_CAL_VIEW (week_view)));
	if (!real_sexp) {
		return; /* No time range is set, so don't start a query */
	}

	e_cal_view_set_status_message (E_CAL_VIEW (week_view), _("Searching"));
	week_view->query = cal_client_get_query (e_cal_view_get_cal_client (E_CAL_VIEW (week_view)), real_sexp);
	g_free (real_sexp);

	if (!week_view->query) {
		g_message ("e_week_view_update_query(): Could not create the query");
		return;
	}

	g_signal_connect (week_view->query, "obj_updated",
			  G_CALLBACK (query_obj_updated_cb), week_view);
	g_signal_connect (week_view->query, "obj_removed",
			  G_CALLBACK (query_obj_removed_cb), week_view);
	g_signal_connect (week_view->query, "query_done",
			  G_CALLBACK (query_query_done_cb), week_view);
	g_signal_connect (week_view->query, "eval_error",
			  G_CALLBACK (query_eval_error_cb), week_view);
}

static void
e_week_view_draw_shadow (EWeekView *week_view)
{
	gint x1, y1, x2, y2;
	GtkStyle *style;
	GdkGC *light_gc, *dark_gc;
	GdkWindow *window;

	/* Draw the shadow around the graphical displays. */
	x1 = week_view->main_canvas->allocation.x - 1;
	y1 = week_view->main_canvas->allocation.y - 1;
	x2 = x1 + week_view->main_canvas->allocation.width + 2;
	y2 = y1 + week_view->main_canvas->allocation.height + 2;

	style = GTK_WIDGET (week_view)->style;
	dark_gc = style->dark_gc[GTK_STATE_NORMAL];
	light_gc = style->light_gc[GTK_STATE_NORMAL];

	window = GTK_WIDGET (week_view)->window;
	gdk_draw_line (window, dark_gc, x1, y1, x1, y2);
	gdk_draw_line (window, dark_gc, x1, y1, x2, y1);
	gdk_draw_line (window, light_gc, x2, y1, x2, y2);
	gdk_draw_line (window, light_gc, x1, y2, x2, y2);
}

static void
e_week_view_new_appointment (EWeekView *week_view, gboolean meeting)
{
	time_t dtstart, dtend;
	struct icaltimetype itt;
	gboolean all_day = FALSE;
	
	/* Edit a new event. If only one day is selected we set the time to
	   the first 1/2-hour of the working day. */
	if (week_view->selection_start_day == week_view->selection_end_day) {
		dtstart = week_view->day_starts[week_view->selection_start_day];
		itt = icaltime_from_timet_with_zone (dtstart, FALSE,
						     week_view->zone);
		itt.hour = calendar_config_get_day_start_hour ();
		itt.minute = calendar_config_get_day_start_minute ();
		dtstart = icaltime_as_timet_with_zone (itt, week_view->zone);

		icaltime_adjust (&itt, 0, 0, 30, 0);
		dtend = icaltime_as_timet_with_zone (itt, week_view->zone);
	} else {
		dtstart = week_view->day_starts[week_view->selection_start_day];
		dtend = week_view->day_starts[week_view->selection_end_day + 1];
		all_day = TRUE;
	}

	gnome_calendar_new_appointment_for (e_cal_view_get_calendar (E_CAL_VIEW (week_view)),
					    dtstart, dtend, all_day, meeting);
}

/**
 * e_week_view_set_default_category:
 * @week_view: A week view.
 * @category: Default category name or NULL for no category.
 *
 * Sets the default category that will be used when creating new calendar
 * components from the week view.
 **/
void
e_week_view_set_default_category (EWeekView *week_view, const char *category)
{
	g_return_if_fail (week_view != NULL);
	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	if (week_view->default_category)
		g_free (week_view->default_category);

	week_view->default_category = g_strdup (category);
}


/* This sets the selected time range. The EWeekView will show the corresponding
   month and the days between start_time and end_time will be selected.
   To select a single day, use the same value for start_time & end_time. */
static void
e_week_view_set_selected_time_range	(EWeekView	*week_view,
					 time_t		 start_time,
					 time_t		 end_time)
{
	GDate date, base_date, end_date;
	gint day_offset, weekday, week_start_offset, num_days;
	gboolean update_adjustment_value = FALSE;

	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	time_to_gdate_with_zone (&date, start_time, week_view->zone);

	if (week_view->multi_week_view) {
		/* Find the number of days since the start of the month. */
		day_offset = g_date_day (&date) - 1;

		/* Find the 1st week which starts at or before the start of
		   the month. */
		base_date = date;
		g_date_set_day (&base_date, 1);

		/* Calculate the weekday of the 1st of the month, 0 = Mon. */
		weekday = g_date_weekday (&base_date) - 1;

		/* Convert it to an offset from the start of the display. */
		week_start_offset = (weekday + 7 - week_view->display_start_day) % 7;

		/* Add it to the day offset so we go back to the 1st week at
		   or before the start of the month. */
		day_offset += week_start_offset;
	} else {
		/* Calculate the weekday of the given date, 0 = Mon. */
		weekday = g_date_weekday (&date) - 1;

		/* Convert it to an offset from the start of the display. */
		week_start_offset = (weekday + 7 - week_view->display_start_day) % 7;

		/* Set the day_offset to the result, so we move back to the
		   start of the week. */
		day_offset = week_start_offset;
	}

	/* Calculate the base date, i.e. the first day shown when the
	   scrollbar adjustment value is 0. */
	base_date = date;
	g_date_subtract_days (&base_date, day_offset);

	/* See if we need to update the base date. */
	if (!g_date_valid (&week_view->base_date)
	    || g_date_compare (&week_view->base_date, &base_date)) {
		week_view->base_date = base_date;
		update_adjustment_value = TRUE;
	}

	/* See if we need to update the first day shown. */
	if (!g_date_valid (&week_view->first_day_shown)
	    || g_date_compare (&week_view->first_day_shown, &base_date)) {
		week_view->first_day_shown = base_date;
		start_time = time_add_day_with_zone (start_time, -day_offset,
						     week_view->zone);
		start_time = time_day_begin_with_zone (start_time,
						       week_view->zone);
		e_week_view_recalc_day_starts (week_view, start_time);
		e_week_view_update_query (week_view);
	}

	/* Set the selection to the given days. */
	week_view->selection_start_day = g_date_julian (&date)
		- g_date_julian (&base_date);
	if (end_time == start_time
	    || end_time <= time_add_day_with_zone (start_time, 1,
						   week_view->zone))
		week_view->selection_end_day = week_view->selection_start_day;
	else {
		time_to_gdate_with_zone (&end_date, end_time - 60, week_view->zone);
		week_view->selection_end_day = g_date_julian (&end_date)
			- g_date_julian (&base_date);
	}

	/* Make sure the selection is valid. */
	num_days = week_view->multi_week_view ? week_view->weeks_shown * 7 : 7;
	num_days--;
	week_view->selection_start_day = CLAMP (week_view->selection_start_day,
						0, num_days);
	week_view->selection_end_day = CLAMP (week_view->selection_end_day,
					      week_view->selection_start_day,
					      num_days);

	/* Reset the adjustment value to 0 if the base address has changed.
	   Note that we do this after updating first_day_shown so that our
	   signal handler will not try to reload the events. */
	if (update_adjustment_value)
		gtk_adjustment_set_value (GTK_RANGE (week_view->vscrollbar)->adjustment, 0);

	gtk_widget_queue_draw (week_view->main_canvas);
}

void
e_week_view_set_selected_time_range_visible	(EWeekView	*week_view,
						 time_t		 start_time,
						 time_t		 end_time)
{
	GDate date, end_date;
	gint num_days;

	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	time_to_gdate_with_zone (&date, start_time, week_view->zone);
	
	/* Set the selection to the given days. */
	week_view->selection_start_day = g_date_julian (&date)
		- g_date_julian (&week_view->first_day_shown);
	if (end_time == start_time
	    || end_time <= time_add_day_with_zone (start_time, 1,
						   week_view->zone))
		week_view->selection_end_day = week_view->selection_start_day;
	else {
		time_to_gdate_with_zone (&end_date, end_time - 60, week_view->zone);
		week_view->selection_end_day = g_date_julian (&end_date)
			- g_date_julian (&week_view->first_day_shown);
	}

	/* Make sure the selection is valid. */
	num_days = week_view->multi_week_view ? week_view->weeks_shown * 7 : 7;
	num_days--;
	week_view->selection_start_day = CLAMP (week_view->selection_start_day,
						0, num_days);
	week_view->selection_end_day = CLAMP (week_view->selection_end_day,
					      week_view->selection_start_day,
					      num_days);

	gtk_widget_queue_draw (week_view->main_canvas);
}


/* Returns the selected time range. */
static void
e_week_view_get_selected_time_range	(EWeekView	*week_view,
					 time_t		*start_time,
					 time_t		*end_time)
{
	gint start_day, end_day;

	start_day = week_view->selection_start_day;
	end_day = week_view->selection_end_day;

	if (start_day == -1) {
		start_day = 0;
		end_day = 0;
	}

	if (start_time)
		*start_time = week_view->day_starts[start_day];

	if (end_time)
		*end_time = week_view->day_starts[end_day + 1];
}


/* Note that the returned date may be invalid if no date has been set yet. */
void
e_week_view_get_first_day_shown		(EWeekView	*week_view,
					 GDate		*date)
{
	*date = week_view->first_day_shown;
}


/* This sets the first day shown in the view. It will be rounded down to the
   nearest week. */
void
e_week_view_set_first_day_shown		(EWeekView	*week_view,
					 GDate		*date)
{
	GDate base_date;
	gint weekday, day_offset, num_days;
	gboolean update_adjustment_value = FALSE;
	guint32 old_selection_start_julian = 0, old_selection_end_julian = 0;
	struct icaltimetype start_tt = icaltime_null_time ();
	time_t start_time;

	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	/* Calculate the old selection range. */
	if (week_view->selection_start_day != -1) {
		old_selection_start_julian =
			g_date_julian (&week_view->base_date)
			+ week_view->selection_start_day;
		old_selection_end_julian =
			g_date_julian (&week_view->base_date)
			+ week_view->selection_end_day;
	}

	/* Calculate the weekday of the given date, 0 = Mon. */
	weekday = g_date_weekday (date) - 1;

	/* Convert it to an offset from the start of the display. */
	day_offset = (weekday + 7 - week_view->display_start_day) % 7;

	/* Calculate the base date, i.e. the first day shown when the
	   scrollbar adjustment value is 0. */
	base_date = *date;
	g_date_subtract_days (&base_date, day_offset);

	/* See if we need to update the base date. */
	if (!g_date_valid (&week_view->base_date)
	    || g_date_compare (&week_view->base_date, &base_date)) {
		week_view->base_date = base_date;
		update_adjustment_value = TRUE;
	}

	/* See if we need to update the first day shown. */
	if (!g_date_valid (&week_view->first_day_shown)
	    || g_date_compare (&week_view->first_day_shown, &base_date)) {
		week_view->first_day_shown = base_date;

		start_tt.year = g_date_year (&base_date);
		start_tt.month = g_date_month (&base_date);
		start_tt.day = g_date_day (&base_date);

		start_time = icaltime_as_timet_with_zone (start_tt,
							  week_view->zone);

		e_week_view_recalc_day_starts (week_view, start_time);
		e_week_view_update_query (week_view);
	}

	/* Try to keep the previous selection, but if it is no longer shown
	   just select the first day. */
	if (week_view->selection_start_day != -1) {
		week_view->selection_start_day = old_selection_start_julian
			- g_date_julian (&base_date);
		week_view->selection_end_day = old_selection_end_julian
			- g_date_julian (&base_date);

		/* Make sure the selection is valid. */
		num_days = week_view->multi_week_view
			? week_view->weeks_shown * 7 : 7;
		num_days--;
		week_view->selection_start_day =
			CLAMP (week_view->selection_start_day, 0, num_days);
		week_view->selection_end_day =
			CLAMP (week_view->selection_end_day,
			       week_view->selection_start_day,
			       num_days);
	}

	/* Reset the adjustment value to 0 if the base address has changed.
	   Note that we do this after updating first_day_shown so that our
	   signal handler will not try to reload the events. */
	if (update_adjustment_value)
		gtk_adjustment_set_value (GTK_RANGE (week_view->vscrollbar)->adjustment, 0);

	gtk_widget_queue_draw (week_view->main_canvas);
}


/* Recalculates the time_t corresponding to the start of each day. */
static void
e_week_view_recalc_day_starts (EWeekView *week_view,
			       time_t lower)
{
	gint num_days, day;
	time_t tmp_time;

	num_days = week_view->multi_week_view ? week_view->weeks_shown * 7 : 7;

	tmp_time = lower;
	week_view->day_starts[0] = tmp_time;
	for (day = 1; day <= num_days; day++) {
		tmp_time = time_add_day_with_zone (tmp_time, 1,
						   week_view->zone);
		week_view->day_starts[day] = tmp_time;
	}
}


gboolean
e_week_view_get_multi_week_view	(EWeekView	*week_view)
{
	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), FALSE);

	return week_view->multi_week_view;
}


void
e_week_view_set_multi_week_view	(EWeekView	*week_view,
				 gboolean	 multi_week_view)
{
	GtkAdjustment *adjustment;
	gint page_increment, page_size;

	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	if (week_view->multi_week_view == multi_week_view)
		return;

	week_view->multi_week_view = multi_week_view;

	if (multi_week_view) {
		gtk_widget_show (week_view->titles_canvas);
		page_increment = 4;
		page_size = 5;
	} else {
		gtk_widget_hide (week_view->titles_canvas);
		page_increment = page_size = 1;
	}

	adjustment = GTK_RANGE (week_view->vscrollbar)->adjustment;
	adjustment->page_increment = page_increment;
	adjustment->page_size = page_size;
	gtk_adjustment_changed (adjustment);

	e_week_view_recalc_cell_sizes (week_view);

	if (g_date_valid (&week_view->first_day_shown))
		e_week_view_set_first_day_shown (week_view,
						 &week_view->first_day_shown);
}


gint
e_week_view_get_weeks_shown	(EWeekView	*week_view)
{
	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), 1);

	return week_view->weeks_shown;
}


void
e_week_view_set_weeks_shown	(EWeekView	*week_view,
				 gint		 weeks_shown)
{
	GtkAdjustment *adjustment;
	gint page_increment, page_size;

	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	weeks_shown = MIN (weeks_shown, E_WEEK_VIEW_MAX_WEEKS);

	if (week_view->weeks_shown == weeks_shown)
		return;

	week_view->weeks_shown = weeks_shown;

	if (week_view->multi_week_view) {
		page_increment = 4;
		page_size = 5;

		adjustment = GTK_RANGE (week_view->vscrollbar)->adjustment;
		adjustment->page_increment = page_increment;
		adjustment->page_size = page_size;
		gtk_adjustment_changed (adjustment);

		e_week_view_recalc_cell_sizes (week_view);

		if (g_date_valid (&week_view->first_day_shown))
			e_week_view_set_first_day_shown (week_view, &week_view->first_day_shown);

		e_week_view_update_query (week_view);
	}
}


gboolean
e_week_view_get_compress_weekend	(EWeekView	*week_view)
{
	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), FALSE);

	return week_view->compress_weekend;
}


void
e_week_view_set_compress_weekend	(EWeekView	*week_view,
					 gboolean	 compress)
{
	gboolean need_reload = FALSE;

	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	if (week_view->compress_weekend == compress)
		return;

	week_view->compress_weekend = compress;

	/* The option only affects the month view. */
	if (!week_view->multi_week_view)
		return;

	e_week_view_recalc_cell_sizes (week_view);

	need_reload = e_week_view_recalc_display_start_day (week_view);

	/* If the display_start_day has changed we need to recalculate the
	   date range shown and reload all events, otherwise we only need to
	   do a reshape. */
	if (need_reload) {
		/* Recalculate the days shown and reload if necessary. */
		if (g_date_valid (&week_view->first_day_shown))
			e_week_view_set_first_day_shown (week_view, &week_view->first_day_shown);
	} else {
		week_view->events_need_reshape = TRUE;
		e_week_view_check_layout (week_view);
	}
}


/* Whether we display event end times. */
gboolean
e_week_view_get_show_event_end_times	(EWeekView	*week_view)
{
	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), TRUE);

	return week_view->show_event_end_times;
}


void
e_week_view_set_show_event_end_times	(EWeekView	*week_view,
					 gboolean	 show)
{
	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	if (week_view->show_event_end_times != show) {
		week_view->show_event_end_times = show;
		e_week_view_recalc_cell_sizes (week_view);
		week_view->events_need_reshape = TRUE;
		e_week_view_check_layout (week_view);
	}
}


/* The first day of the week, 0 (Monday) to 6 (Sunday). */
gint
e_week_view_get_week_start_day	(EWeekView	*week_view)
{
	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), 0);

	return week_view->week_start_day;
}


void
e_week_view_set_week_start_day	(EWeekView	*week_view,
				 gint		 week_start_day)
{
	g_return_if_fail (E_IS_WEEK_VIEW (week_view));
	g_return_if_fail (week_start_day >= 0);
	g_return_if_fail (week_start_day < 7);

	if (week_view->week_start_day == week_start_day)
		return;

	week_view->week_start_day = week_start_day;

	e_week_view_recalc_display_start_day (week_view);

	/* Recalculate the days shown and reload if necessary. */
	if (g_date_valid (&week_view->first_day_shown))
		e_week_view_set_first_day_shown (week_view,
						 &week_view->first_day_shown);
}


/* Whether we use 12-hour or 24-hour format. */
gboolean
e_week_view_get_24_hour_format	(EWeekView	*week_view)
{
	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), FALSE);

	return week_view->use_24_hour_format;
}


void
e_week_view_set_24_hour_format	(EWeekView	*week_view,
				 gboolean	 use_24_hour)
{
	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	if (week_view->use_24_hour_format == use_24_hour)
		return;

	week_view->use_24_hour_format = use_24_hour;

	/* We need to re-layout the events since the time format affects the
	   sizes. */
	e_week_view_recalc_cell_sizes (week_view);
	week_view->events_need_reshape = TRUE;
	e_week_view_check_layout (week_view);
	gtk_widget_queue_draw (week_view->main_canvas);
}


/* The current timezone. */
icaltimezone*
e_week_view_get_timezone		(EWeekView	*week_view)
{
	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), NULL);

	return week_view->zone;
}


void
e_week_view_set_timezone		(EWeekView	*week_view,
					 icaltimezone	*zone)
{
	icaltimezone *old_zone;
	struct icaltimetype tt = icaltime_null_time ();
	time_t lower;

	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	old_zone = week_view->zone;
	if (old_zone == zone)
		return;

	week_view->zone = zone;

	/* If we don't have a valid date set yet, just return. */
	if (!g_date_valid (&week_view->first_day_shown))
		return;

	/* Recalculate the new start of the first week. We just use exactly
	   the same time, but with the new timezone. */
	tt.year = g_date_year (&week_view->first_day_shown);
	tt.month = g_date_month (&week_view->first_day_shown);
	tt.day = g_date_day (&week_view->first_day_shown);

	lower = icaltime_as_timet_with_zone (tt, zone);

	e_week_view_recalc_day_starts (week_view, lower);
	e_week_view_update_query (week_view);
}

static gboolean
e_week_view_recalc_display_start_day	(EWeekView	*week_view)
{
	gint display_start_day;

	/* The display start day defaults to week_start_day, but we have
	   to use Saturday if the weekend is compressed and week_start_day
	   is Sunday. */
	display_start_day = week_view->week_start_day;

	if (display_start_day == 6
	    && (!week_view->multi_week_view || week_view->compress_weekend))
		display_start_day = 5;

	if (week_view->display_start_day != display_start_day) {
		week_view->display_start_day = display_start_day;
		return TRUE;
	}

	return FALSE;
}


static gboolean
e_week_view_update_event_cb (EWeekView *week_view,
			     gint event_num,
			     gpointer data)
{
	EWeekViewEvent *event;
	EWeekViewEventSpan *span;
	gint span_num;
	gchar *text;
	CalComponent *comp;

	comp = data;

	event = &g_array_index (week_view->events, EWeekViewEvent, event_num);

	g_object_unref (event->comp);
	event->comp = comp;
	g_object_ref (comp);

	for (span_num = 0; span_num < event->num_spans; span_num++) {
		span = &g_array_index (week_view->spans, EWeekViewEventSpan,
				       event->spans_index + span_num);

		if (span->text_item) {
			CalComponentText t;

			cal_component_get_summary (event->comp, &t);
			text = (char*) t.value;
			gnome_canvas_item_set (span->text_item,
					       "text", text ? text : "",
					       NULL);

			e_week_view_reshape_event_span (week_view, event_num,
							span_num);
		}
	}

	return TRUE;
}


/* This calls a given function for each event instance that matches the given
   uid. Note that it is safe for the callback to remove the event (since we
   step backwards through the arrays). */
static void
e_week_view_foreach_event_with_uid (EWeekView *week_view,
				    const gchar *uid,
				    EWeekViewForeachEventCallback callback,
				    gpointer data)
{
	EWeekViewEvent *event;
	gint event_num;

	for (event_num = week_view->events->len - 1;
	     event_num >= 0;
	     event_num--) {
		const char *u;

		event = &g_array_index (week_view->events, EWeekViewEvent,
					event_num);

		cal_component_get_uid (event->comp, &u);
		if (u && !strcmp (uid, u)) {
			if (!(*callback) (week_view, event_num, data))
				return;
		}
	}
}


static gboolean
e_week_view_remove_event_cb (EWeekView *week_view,
			     gint event_num,
			     gpointer data)
{
	EWeekViewEvent *event;
	EWeekViewEventSpan *span;
	gint span_num;

	event = &g_array_index (week_view->events, EWeekViewEvent, event_num);

	/* If we were editing this event, set editing_event_num to -1 so
	   on_editing_stopped doesn't try to update the event. */
	if (week_view->editing_event_num == event_num)
		week_view->editing_event_num = -1;

	/* We leave the span elements in the array, but set the canvas item
	   pointers to NULL. */
	for (span_num = 0; span_num < event->num_spans; span_num++) {
		span = &g_array_index (week_view->spans, EWeekViewEventSpan,
				       event->spans_index + span_num);

		if (span->text_item) {
			gtk_object_destroy (GTK_OBJECT (span->text_item));
			span->text_item = NULL;
		}
		if (span->background_item) {
			gtk_object_destroy (GTK_OBJECT (span->background_item));
			span->background_item = NULL;
		}
	}

	g_object_unref (event->comp);

	g_array_remove_index (week_view->events, event_num);
	week_view->events_need_layout = TRUE;

	return TRUE;
}


void
e_week_view_get_day_position	(EWeekView	*week_view,
				 gint		 day,
				 gint		*day_x,
				 gint		*day_y,
				 gint		*day_w,
				 gint		*day_h)
{
	gint cell_x, cell_y, cell_h;

	e_week_view_layout_get_day_position (day,
					     week_view->multi_week_view,
					     week_view->weeks_shown,
					     week_view->display_start_day,
					     week_view->compress_weekend,
					     &cell_x, &cell_y, &cell_h);

	*day_x = week_view->col_offsets[cell_x];
	*day_y = week_view->row_offsets[cell_y];

	*day_w = week_view->col_widths[cell_x];
	*day_h = week_view->row_heights[cell_y];
	if (cell_h == 2)
		*day_h += week_view->row_heights[cell_y + 1];
}


/* Returns the bounding box for a span of an event. Usually this can easily
   be determined by the start & end days and row of the span, which are set in
   e_week_view_layout_event(). Though we need a special case for the weekends
   when they are compressed, since the span may not fit.
   The bounding box includes the entire width of the days in the view (but
   not the vertical line down the right of the last day), though the displayed
   event doesn't normally extend to the edges of the day.
   It returns FALSE if the span isn't visible. */
gboolean
e_week_view_get_span_position	(EWeekView	*week_view,
				 gint		 event_num,
				 gint		 span_num,
				 gint		*span_x,
				 gint		*span_y,
				 gint		*span_w)
{
	EWeekViewEvent *event;
	EWeekViewEventSpan *span;
	gint num_days;
	gint start_x, start_y, start_w, start_h;
	gint end_x, end_y, end_w, end_h;

	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), FALSE);
	g_return_val_if_fail (event_num < week_view->events->len, FALSE);

	event = &g_array_index (week_view->events, EWeekViewEvent, event_num);

	g_return_val_if_fail (span_num < event->num_spans, FALSE);

	span = &g_array_index (week_view->spans, EWeekViewEventSpan,
			       event->spans_index + span_num);

	if (!e_week_view_layout_get_span_position (event, span,
						   week_view->rows_per_cell,
						   week_view->rows_per_compressed_cell,
						   week_view->display_start_day,
						   week_view->multi_week_view,
						   week_view->compress_weekend,
						   &num_days)) {
		return FALSE;
	}

	e_week_view_get_day_position (week_view, span->start_day,
				      &start_x, &start_y, &start_w, &start_h);
	*span_y = start_y + week_view->events_y_offset
		+ span->row * (week_view->row_height
			       + E_WEEK_VIEW_EVENT_Y_SPACING);
	if (num_days == 1) {
		*span_x = start_x;
		*span_w = start_w - 1;
	} else {
		e_week_view_get_day_position (week_view,
					      span->start_day + num_days - 1,
					      &end_x, &end_y, &end_w, &end_h);
		*span_x = start_x;
		*span_w = end_x + end_w - start_x - 1;
	}

	return TRUE;
}



static gboolean
e_week_view_on_button_press (GtkWidget *widget,
			     GdkEventButton *event,
			     EWeekView *week_view)
{
	gint x, y, day;

#if 0
	g_print ("In e_week_view_on_button_press\n");
	if (event->type == GDK_2BUTTON_PRESS)
		g_print (" is a double-click\n");
	if (week_view->pressed_event_num != -1)
		g_print (" item is pressed\n");
#endif

	/* Convert the mouse position to a week & day. */
	x = event->x;
	y = event->y;
	day = e_week_view_convert_position_to_day (week_view, x, y);
	if (day == -1)
		return FALSE;

	/* If an event is pressed just return. */
	if (week_view->pressed_event_num != -1)
		return FALSE;

	if (event->button == 1 && event->type == GDK_2BUTTON_PRESS) {
		e_week_view_new_appointment (week_view, FALSE);
		return TRUE;
	}

	if (event->button == 1) {
		/* Start the selection drag. */
		if (!GTK_WIDGET_HAS_FOCUS (week_view))
			gtk_widget_grab_focus (GTK_WIDGET (week_view));

		if (gdk_pointer_grab (GTK_LAYOUT (widget)->bin_window, FALSE,
				      GDK_POINTER_MOTION_MASK
				      | GDK_BUTTON_RELEASE_MASK,
				      FALSE, NULL, event->time) == 0) {
			week_view->selection_start_day = day;
			week_view->selection_end_day = day;
			week_view->selection_drag_pos = E_WEEK_VIEW_DRAG_END;

			/* FIXME: Optimise? */
			gtk_widget_queue_draw (week_view->main_canvas);
		}
	} else if (event->button == 3) {
		if (!GTK_WIDGET_HAS_FOCUS (week_view))
			gtk_widget_grab_focus (GTK_WIDGET (week_view));

		if (day < week_view->selection_start_day || day > week_view->selection_end_day) {
			week_view->selection_start_day = day;
			week_view->selection_end_day = day;
			week_view->selection_drag_pos = E_WEEK_VIEW_DRAG_NONE;

			/* FIXME: Optimise? */
			gtk_widget_queue_draw (week_view->main_canvas);
		}
		
		e_week_view_show_popup_menu (week_view, event, -1);
	}

	return TRUE;
}


static gboolean
e_week_view_on_button_release (GtkWidget *widget,
			       GdkEventButton *event,
			       EWeekView *week_view)
{
	time_t start, end;

#if 0
	g_print ("In e_week_view_on_button_release\n");
#endif

	if (week_view->selection_drag_pos != E_WEEK_VIEW_DRAG_NONE) {
		week_view->selection_drag_pos = E_WEEK_VIEW_DRAG_NONE;
		gdk_pointer_ungrab (event->time);
		start = week_view->day_starts[week_view->selection_start_day];
		end = week_view->day_starts[week_view->selection_end_day + 1];

		gnome_calendar_set_selected_time_range (e_cal_view_get_calendar (E_CAL_VIEW (week_view)),
							start, end);
	}

	return FALSE;
}

static gboolean
e_week_view_on_scroll (GtkWidget *widget,
		       GdkEventScroll *scroll,
		       EWeekView *week_view)
{
	GtkAdjustment *adj = GTK_RANGE (week_view->vscrollbar)->adjustment;
	gfloat new_value;
	
	switch (scroll->direction){
	case GDK_SCROLL_UP:
		new_value = adj->value - adj->page_increment;
		break;
	case GDK_SCROLL_DOWN:
		new_value = adj->value + adj->page_increment;
		break;
	default:
		return FALSE;
	}
	
	new_value = CLAMP (new_value, adj->lower, adj->upper - adj->page_size);
	gtk_adjustment_set_value (adj, new_value);
	
	return TRUE;
}


static gboolean
e_week_view_on_motion (GtkWidget *widget,
		       GdkEventMotion *mevent,
		       EWeekView *week_view)
{
	gint x, y, day;

#if 0
	g_print ("In e_week_view_on_motion\n");
#endif

	/* Convert the mouse position to a week & day. */
	x = mevent->x;
	y = mevent->y;
	day = e_week_view_convert_position_to_day (week_view, x, y);
	if (day == -1)
		return FALSE;

	if (week_view->selection_drag_pos != E_WEEK_VIEW_DRAG_NONE) {
		e_week_view_update_selection (week_view, day);
		return TRUE;
	}

	return FALSE;
}


/* Converts a position in the canvas window to a day offset from the first
   day displayed. Returns -1 if the position is outside the grid. */
static gint
e_week_view_convert_position_to_day (EWeekView *week_view,
				     gint x,
				     gint y)
{
	gint col, row, grid_x = -1, grid_y = -1, week, day;
	gint weekend_col, box, weekend_box;

	/* First we convert it to a grid position. */
	for (col = 0; col <= week_view->columns; col++) {
		if (x < week_view->col_offsets[col]) {
			grid_x = col - 1;
			break;
		}
	}

	for (row = 0; row <= week_view->rows; row++) {
		if (y < week_view->row_offsets[row]) {
			grid_y = row - 1;
			break;
		}
	}

	/* If the mouse is outside the grid return FALSE. */
	if (grid_x == -1 || grid_y == -1)
		return -1;

	/* Now convert the grid position to a week and day. */
	if (week_view->multi_week_view) {
		week = grid_y / 2;
		day = grid_x;

		if (week_view->compress_weekend) {
			weekend_col = (5 + 7 - week_view->display_start_day) % 7;
			if (grid_x > weekend_col
			    || (grid_x == weekend_col && grid_y % 2 == 1))
				day++;
		}
	} else {
		week = 0;

		box = grid_x * 3 + grid_y / 2;
		weekend_box = (5 + 7 - week_view->display_start_day) % 7;
		day = box;
		if (box > weekend_box
		    ||( box == weekend_box && grid_y % 2 == 1))
			day++;
	}

	return week * 7 + day;
}


static void
e_week_view_update_selection (EWeekView *week_view,
			      gint day)
{
	gint tmp_day;
	gboolean need_redraw = FALSE;

#if 0
	g_print ("Updating selection %i,%i\n", week, day);
#endif

	if (week_view->selection_drag_pos == E_WEEK_VIEW_DRAG_START) {
		if (day != week_view->selection_start_day) {
			need_redraw = TRUE;
			week_view->selection_start_day = day;
		}
	} else {
		if (day != week_view->selection_end_day) {
			need_redraw = TRUE;
			week_view->selection_end_day = day;
		}
	}

	/* Switch the drag position if necessary. */
	if (week_view->selection_start_day > week_view->selection_end_day) {
		tmp_day = week_view->selection_start_day;
		week_view->selection_start_day = week_view->selection_end_day;
		week_view->selection_end_day = tmp_day;
		if (week_view->selection_drag_pos == E_WEEK_VIEW_DRAG_START)
			week_view->selection_drag_pos = E_WEEK_VIEW_DRAG_END;
		else
			week_view->selection_drag_pos = E_WEEK_VIEW_DRAG_START;
	}

	/* FIXME: Optimise? */
	if (need_redraw) {
		gtk_widget_queue_draw (week_view->main_canvas);
	}
}


static void
e_week_view_free_events (EWeekView *week_view)
{
	EWeekViewEvent *event;
	EWeekViewEventSpan *span;
	gint event_num, span_num, num_days, day;

	/* Reset all our indices. */
	week_view->pressed_event_num = -1;
	week_view->pressed_span_num = -1;
	week_view->editing_event_num = -1;
	week_view->editing_span_num = -1;
	week_view->popup_event_num = -1;

	for (event_num = 0; event_num < week_view->events->len; event_num++) {
		event = &g_array_index (week_view->events, EWeekViewEvent,
					event_num);
		g_object_unref (event->comp);
	}

	g_array_set_size (week_view->events, 0);

	/* Destroy all the old canvas items. */
	if (week_view->spans) {
		for (span_num = 0; span_num < week_view->spans->len;
		     span_num++) {
			span = &g_array_index (week_view->spans,
					       EWeekViewEventSpan, span_num);
			if (span->background_item)
				gtk_object_destroy (GTK_OBJECT (span->background_item));
			if (span->text_item)
				gtk_object_destroy (GTK_OBJECT (span->text_item));
		}
		g_array_free (week_view->spans, TRUE);
		week_view->spans = NULL;
	}

	/* Clear the number of rows used per day. */
	num_days = week_view->multi_week_view ? week_view->weeks_shown * 7 : 7;
	for (day = 0; day <= num_days; day++) {
		week_view->rows_per_day[day] = 0;
	}

	/* Hide all the jump buttons. */
	for (day = 0; day < E_WEEK_VIEW_MAX_WEEKS * 7; day++) {
		gnome_canvas_item_hide (week_view->jump_buttons[day]);
	}
}


/* This adds one event to the view, adding it to the appropriate array. */
static gboolean
e_week_view_add_event (CalComponent *comp,
		       time_t	  start,
		       time_t	  end,
		       gpointer	  data)

{
	EWeekView *week_view;
	EWeekViewEvent event;
	gint num_days;
	struct icaltimetype start_tt, end_tt;

	week_view = E_WEEK_VIEW (data);

	/* Check that the event times are valid. */
	num_days = week_view->multi_week_view ? week_view->weeks_shown * 7 : 7;

#if 0
	g_print ("View start:%li end:%li  Event start:%li end:%li\n",
		 week_view->day_starts[0], week_view->day_starts[num_days],
		 start, end);
#endif

	g_return_val_if_fail (start <= end, TRUE);
	g_return_val_if_fail (start < week_view->day_starts[num_days], TRUE);
	g_return_val_if_fail (end > week_view->day_starts[0], TRUE);

	start_tt = icaltime_from_timet_with_zone (start, FALSE,
						  week_view->zone);
	end_tt = icaltime_from_timet_with_zone (end, FALSE,
						week_view->zone);

	event.comp = comp;
	g_object_ref (event.comp);
	event.start = start;
	event.end = end;
	event.spans_index = 0;
	event.num_spans = 0;

	event.start_minute = start_tt.hour * 60 + start_tt.minute;
	event.end_minute = end_tt.hour * 60 + end_tt.minute;
	if (event.end_minute == 0 && start != end)
		event.end_minute = 24 * 60;

	event.different_timezone = FALSE;
	if (!cal_comp_util_compare_event_timezones (comp, e_cal_view_get_cal_client (E_CAL_VIEW (week_view)),
						    week_view->zone))
		event.different_timezone = TRUE;

	g_array_append_val (week_view->events, event);
	week_view->events_sorted = FALSE;
	week_view->events_need_layout = TRUE;

	return TRUE;
}


/* This lays out the events, or reshapes them, as necessary. */
static void
e_week_view_check_layout (EWeekView *week_view)
{
	/* Don't bother if we aren't visible. */
	if (!GTK_WIDGET_VISIBLE (week_view))
	    return;

	/* Make sure the events are sorted (by start and size). */
	e_week_view_ensure_events_sorted (week_view);

	if (week_view->events_need_layout)
		week_view->spans = e_week_view_layout_events
			(week_view->events, week_view->spans,
			 week_view->multi_week_view,
			 week_view->weeks_shown,
			 week_view->compress_weekend,
			 week_view->display_start_day,
			 week_view->day_starts,
			 week_view->rows_per_day);

	if (week_view->events_need_layout || week_view->events_need_reshape)
		e_week_view_reshape_events (week_view);

	week_view->events_need_layout = FALSE;
	week_view->events_need_reshape = FALSE;
}


static void
e_week_view_ensure_events_sorted (EWeekView *week_view)
{
	if (!week_view->events_sorted) {
		qsort (week_view->events->data,
		       week_view->events->len,
		       sizeof (EWeekViewEvent),
		       e_week_view_event_sort_func);
		week_view->events_sorted = TRUE;
	}
}


gint
e_week_view_event_sort_func (const void *arg1,
			     const void *arg2)
{
	EWeekViewEvent *event1, *event2;

	event1 = (EWeekViewEvent*) arg1;
	event2 = (EWeekViewEvent*) arg2;

	if (event1->start < event2->start)
		return -1;
	if (event1->start > event2->start)
		return 1;

	if (event1->end > event2->end)
		return -1;
	if (event1->end < event2->end)
		return 1;

	return 0;
}


static void
e_week_view_reshape_events (EWeekView *week_view)
{
	EWeekViewEvent *event;
	gint event_num, span_num;
	gint num_days, day, day_x, day_y, day_w, day_h, max_rows;
	gboolean is_weekend;

	for (event_num = 0; event_num < week_view->events->len; event_num++) {
		event = &g_array_index (week_view->events, EWeekViewEvent,
					event_num);
		for (span_num = 0; span_num < event->num_spans; span_num++) {
			e_week_view_reshape_event_span (week_view, event_num,
							span_num);
		}
	}

	/* Reshape the jump buttons and show/hide them as appropriate. */
	num_days = week_view->multi_week_view ? week_view->weeks_shown * 7 : 7;
	for (day = 0; day < num_days; day++) {

		is_weekend = ((week_view->display_start_day + day) % 7 >= 5) ? TRUE : FALSE;
		if (!is_weekend || (week_view->multi_week_view
				    && !week_view->compress_weekend))
			max_rows = week_view->rows_per_cell;
		else
			max_rows = week_view->rows_per_compressed_cell;

		/* Determine whether the jump button should be shown. */
		if (week_view->rows_per_day[day] <= max_rows) {
			gnome_canvas_item_hide (week_view->jump_buttons[day]);
		} else {
			e_week_view_get_day_position (week_view, day,
						      &day_x, &day_y,
						      &day_w, &day_h);

			gnome_canvas_item_set (week_view->jump_buttons[day],
					       "GnomeCanvasPixbuf::x", (gdouble) (day_x + day_w - E_WEEK_VIEW_JUMP_BUTTON_X_PAD - E_WEEK_VIEW_JUMP_BUTTON_WIDTH),
					       "GnomeCanvasPixbuf::y", (gdouble) (day_y + day_h - E_WEEK_VIEW_JUMP_BUTTON_Y_PAD - E_WEEK_VIEW_JUMP_BUTTON_HEIGHT),
					       NULL);

			gnome_canvas_item_show (week_view->jump_buttons[day]);
			gnome_canvas_item_raise_to_top (week_view->jump_buttons[day]);
		}
	}

	for (day = num_days; day < E_WEEK_VIEW_MAX_WEEKS * 7; day++) {
		gnome_canvas_item_hide (week_view->jump_buttons[day]);
	}
}


static void
e_week_view_reshape_event_span (EWeekView *week_view,
				gint event_num,
				gint span_num)
{
	EWeekViewEvent *event;
	EWeekViewEventSpan *span;
	gint span_x, span_y, span_w, num_icons, icons_width, time_width;
	gint min_text_x, max_text_w, width;
	gboolean show_icons = TRUE, use_max_width = FALSE;
	gboolean one_day_event;
	CalComponent *comp;
	gdouble text_x, text_y, text_w, text_h;
	gchar *text, *end_of_line;
	gint line_len, text_width;
	PangoFontDescription *font_desc;
	PangoContext *pango_context;
	PangoFontMetrics *font_metrics;
	PangoLayout *layout;

	event = &g_array_index (week_view->events, EWeekViewEvent, event_num);
	span = &g_array_index (week_view->spans, EWeekViewEventSpan,
			       event->spans_index + span_num);
	comp = event->comp;

	one_day_event = e_week_view_is_one_day_event (week_view, event_num);

	/* If the span will not be visible destroy the canvas items and
	   return. */
	if (!e_week_view_get_span_position (week_view, event_num, span_num,
					    &span_x, &span_y, &span_w)) {
		if (span->background_item)
			gtk_object_destroy (GTK_OBJECT (span->background_item));
		if (span->text_item)
			gtk_object_destroy (GTK_OBJECT (span->text_item));
		span->background_item = NULL;
		span->text_item = NULL;
		return;
	}

	/* Set up Pango prerequisites */
	font_desc = gtk_widget_get_style (GTK_WIDGET (week_view))->font_desc;
	pango_context = gtk_widget_get_pango_context (GTK_WIDGET (week_view));
	font_metrics = pango_context_get_metrics (pango_context, font_desc,
						  pango_context_get_language (pango_context));
	layout = pango_layout_new (pango_context);

	/* If we are editing a long event we don't show the icons and the EText
	   item uses the maximum width available. */
	if (!one_day_event && week_view->editing_event_num == event_num
	    && week_view->editing_span_num == span_num) {
		show_icons = FALSE;
		use_max_width = TRUE;
	}

	/* Calculate how many icons we need to show. */
	num_icons = 0;
	if (show_icons) {
		GSList *categories_list, *elem;

		if (cal_component_has_alarms (comp))
			num_icons++;
		if (cal_component_has_recurrences (comp))
			num_icons++;
		if (event->different_timezone)
			num_icons++;

		cal_component_get_categories_list (comp, &categories_list);
		for (elem = categories_list; elem; elem = elem->next) {
			char *category;
			GdkPixmap *pixmap = NULL;
			GdkBitmap *mask = NULL;

			category = (char *) elem->data;
			if (e_categories_config_get_icon_for (category, &pixmap, &mask))
				num_icons++;
		}

		cal_component_free_categories_list (categories_list);
	}

	/* Create the background canvas item if necessary. */
	if (!span->background_item) {
		span->background_item =
			gnome_canvas_item_new (GNOME_CANVAS_GROUP (GNOME_CANVAS (week_view->main_canvas)->root),
					       e_week_view_event_item_get_type (),
					       NULL);
	}

	gnome_canvas_item_set (span->background_item,
			       "event_num", event_num,
			       "span_num", span_num,
			       NULL);

	/* Create the text item if necessary. */
	if (!span->text_item) {
		CalComponentText text;

		cal_component_get_summary (comp, &text);
		span->text_item =
			gnome_canvas_item_new (GNOME_CANVAS_GROUP (GNOME_CANVAS (week_view->main_canvas)->root),
					       e_text_get_type (),
					       "anchor", GTK_ANCHOR_NW,
					       "clip", TRUE,
					       "max_lines", 1,
					       "editable", TRUE,
					       "text", text.value ? text.value : "",
					       "use_ellipsis", TRUE,
					       "fill_color_rgba", GNOME_CANVAS_COLOR(0, 0, 0),
					       "im_context", E_CANVAS (week_view->main_canvas)->im_context,
					       NULL);
		g_signal_connect (span->text_item, "event",
				  G_CALLBACK (e_week_view_on_text_item_event),
				  week_view);
	}

	/* Calculate the position of the text item.
	   For events < 1 day it starts after the times & icons and ends at the
	   right edge of the span.
	   For events >= 1 day we need to determine whether times are shown at
	   the start and end of the span, then try to center the text item with
	   the icons in the middle, but making sure we don't go over the times.
	*/


	/* Calculate the space necessary to display a time, e.g. "13:00". */
	time_width = e_week_view_get_time_string_width (week_view);

	/* Calculate the space needed for the icons. */
	icons_width = (E_WEEK_VIEW_ICON_WIDTH + E_WEEK_VIEW_ICON_X_PAD)
		* num_icons - E_WEEK_VIEW_ICON_X_PAD + E_WEEK_VIEW_ICON_R_PAD;

	/* The y position and height are the same for both event types. */
	text_y = span_y + E_WEEK_VIEW_EVENT_BORDER_HEIGHT
		+ E_WEEK_VIEW_EVENT_TEXT_Y_PAD;

	text_h =
		PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics)) +
		PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics));

	if (one_day_event) {
		/* Note that 1-day events don't have a border. Although we
		   still use the border height to position the events
		   vertically so they still line up neatly (see above),
		   we don't use the border width or edge padding at all. */
		text_x = span_x	+ E_WEEK_VIEW_EVENT_L_PAD;

		switch (week_view->time_format) {
		case E_WEEK_VIEW_TIME_BOTH_SMALL_MIN:
		case E_WEEK_VIEW_TIME_BOTH:
			/* These have 2 time strings with a small space between
			   them and some space before the EText item. */
			text_x += time_width * 2
				+ E_WEEK_VIEW_EVENT_TIME_SPACING
				+ E_WEEK_VIEW_EVENT_TIME_X_PAD;
			break;
		case E_WEEK_VIEW_TIME_START_SMALL_MIN:
		case E_WEEK_VIEW_TIME_START:
			/* These have just 1 time string with some space
			   before the EText item. */
			text_x += time_width + E_WEEK_VIEW_EVENT_TIME_X_PAD;
			break;
		case E_WEEK_VIEW_TIME_NONE:
			break;
		}

		/* The icons_width includes space on the right of the icons. */
		text_x += icons_width;

		/* The width of the EText item extends right to the edge of the
		   event, just inside the border. */
		text_w = span_x + span_w - E_WEEK_VIEW_EVENT_R_PAD - text_x;

	} else {
		if (use_max_width) {
			/* When we are editing the event we use all the
			   available width. */
			text_x = span_x + E_WEEK_VIEW_EVENT_L_PAD
				+ E_WEEK_VIEW_EVENT_BORDER_WIDTH
				+ E_WEEK_VIEW_EVENT_EDGE_X_PAD;
			text_w = span_x + span_w - E_WEEK_VIEW_EVENT_R_PAD
				- E_WEEK_VIEW_EVENT_BORDER_WIDTH
				- E_WEEK_VIEW_EVENT_EDGE_X_PAD - text_x;
		} else {
			/* Get the width of the text of the event. This is a
			   bit of a hack. It would be better if EText could
			   tell us this. */
			g_object_get (G_OBJECT (span->text_item), "text", &text, NULL);
			text_width = 0;
			if (text) {
				/* It should only have one line of text in it.
				   I'm not sure we need this any more. */
				end_of_line = strchr (text, '\n');
				if (end_of_line)
					line_len = end_of_line - text;
				else
					line_len = strlen (text);

				pango_layout_set_text (layout, text, line_len);
				pango_layout_get_pixel_size (layout, &text_width, NULL);
				g_free (text);
			}

			/* Add on the width of the icons and find the default
			   position, which centers the icons + text. */
			width = text_width + icons_width;
			text_x = span_x + (span_w - width) / 2;

			/* Now calculate the left-most valid position, and make
			   sure we don't go to the left of that. */
			min_text_x = span_x + E_WEEK_VIEW_EVENT_L_PAD
				+ E_WEEK_VIEW_EVENT_BORDER_WIDTH
				+ E_WEEK_VIEW_EVENT_EDGE_X_PAD;
			/* See if we will want to display the start time, and
			   if so take that into account. */
			if (event->start > week_view->day_starts[span->start_day])
				min_text_x += time_width
					+ E_WEEK_VIEW_EVENT_TIME_X_PAD;

			/* Now make sure we don't go to the left of the minimum
			   position. */
			text_x = MAX (text_x, min_text_x);

			/* Now calculate the largest valid width, using the
			   calculated x position, and make sure we don't
			   exceed that. */
			max_text_w = span_x + span_w - E_WEEK_VIEW_EVENT_R_PAD
				- E_WEEK_VIEW_EVENT_BORDER_WIDTH
				- E_WEEK_VIEW_EVENT_EDGE_X_PAD - text_x;
			if (event->end < week_view->day_starts[span->start_day
							      + span->num_days])
				max_text_w -= time_width
					+ E_WEEK_VIEW_EVENT_TIME_X_PAD;

			text_w = MIN (width, max_text_w);

			/* Now take out the space for the icons. */
			text_x += icons_width;
			text_w -= icons_width;
		}
	}

	/* Make sure we don't try to use a negative width. */
	text_w = MAX (text_w, 0);

	gnome_canvas_item_set (span->text_item,
			       "clip_width", (gdouble) text_w,
			       "clip_height", (gdouble) text_h,
			       NULL);
	e_canvas_item_move_absolute (span->text_item, text_x, text_y);

	g_object_unref (layout);
	pango_font_metrics_unref (font_metrics);
}


static void
e_week_view_on_adjustment_changed (GtkAdjustment *adjustment,
				   EWeekView *week_view)
{
	GDate date;
	gint week_offset;
	struct icaltimetype start_tt = icaltime_null_time ();
	time_t lower, start, end;
	guint32 old_first_day_julian, new_first_day_julian;

	/* If we don't have a valid date set yet, just return. */
	if (!g_date_valid (&week_view->first_day_shown))
		return;

	/* Determine the first date shown. */
	date = week_view->base_date;
	week_offset = floor (adjustment->value + 0.5);
	g_date_add_days (&date, week_offset * 7);

	/* Convert the old & new first days shown to julian values. */
	old_first_day_julian = g_date_julian (&week_view->first_day_shown);
	new_first_day_julian = g_date_julian (&date);

	/* If we are already showing the date, just return. */
	if (old_first_day_julian == new_first_day_julian)
		return;

	/* Set the new first day shown. */
	week_view->first_day_shown = date;

	/* Convert it to a time_t. */
	start_tt.year = g_date_year (&date);
	start_tt.month = g_date_month (&date);
	start_tt.day = g_date_day (&date);

	lower = icaltime_as_timet_with_zone (start_tt, week_view->zone);

	e_week_view_recalc_day_starts (week_view, lower);
	e_week_view_update_query (week_view);

	/* Update the selection, if needed. */
	if (week_view->selection_start_day != -1) {
		start = week_view->day_starts[week_view->selection_start_day];
		end = week_view->day_starts[week_view->selection_end_day + 1];
		gnome_calendar_set_selected_time_range (e_cal_view_get_calendar (E_CAL_VIEW (week_view)),
							start, end);
	}
}


gboolean
e_week_view_start_editing_event (EWeekView *week_view,
				 gint	    event_num,
				 gint	    span_num,
				 gchar     *initial_text)
{
	EWeekViewEvent *event;
	EWeekViewEventSpan *span;
	ETextEventProcessor *event_processor = NULL;
	ETextEventProcessorCommand command;

	/* If we are already editing the event, just return. */
	if (event_num == week_view->editing_event_num
	    && span_num == week_view->editing_span_num)
		return TRUE;

	event = &g_array_index (week_view->events, EWeekViewEvent, event_num);
	span = &g_array_index (week_view->spans, EWeekViewEventSpan,
			       event->spans_index + span_num);

	/* If the event is not shown, don't try to edit it. */
	if (!span->text_item)
		return FALSE;

	if (initial_text) {
		gnome_canvas_item_set (span->text_item,
				       "text", initial_text,
				       NULL);
	}

	/* FIXME: This implicitly stops any edit of another item, causing it
	   to be sent to the server and resulting in a call to obj_updated_cb()
	   which may reload all the events and so our span and text item may
	   actually be destroyed. So we often get a SEGV. */
	e_canvas_item_grab_focus (span->text_item, TRUE);

	/* Try to move the cursor to the end of the text. */
	g_object_get (G_OBJECT (span->text_item), "event_processor", &event_processor, NULL);
	if (event_processor) {
		command.action = E_TEP_MOVE;
		command.position = E_TEP_END_OF_BUFFER;
		g_signal_emit_by_name (event_processor,
				       "command", &command);
	}
	return TRUE;
}


/* This stops any current edit. */
void
e_week_view_stop_editing_event (EWeekView *week_view)
{
	GtkWidget *toplevel;

	/* Check we are editing an event. */
	if (week_view->editing_event_num == -1)
		return;

	/* Set focus to the toplevel so the item loses focus. */
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (week_view));
	if (toplevel && GTK_IS_WINDOW (toplevel))
		gtk_window_set_focus (GTK_WINDOW (toplevel), NULL);
}


/* Cancels the current edition by resetting the appointment's text to its original value */
static void
cancel_editing (EWeekView *week_view)
{
	int event_num, span_num;
	EWeekViewEvent *event;
	EWeekViewEventSpan *span;
	CalComponentText summary;

	event_num = week_view->editing_event_num;
	span_num = week_view->editing_span_num;

	g_assert (event_num != -1);

	event = &g_array_index (week_view->events, EWeekViewEvent, event_num);
	span = &g_array_index (week_view->spans, EWeekViewEventSpan, event->spans_index + span_num);

	/* Reset the text to what was in the component */

	cal_component_get_summary (event->comp, &summary);
	g_object_set (G_OBJECT (span->text_item), "text", summary.value ? summary.value : "", NULL);

	/* Stop editing */
	e_week_view_stop_editing_event (week_view);
}

/* Callback used when a component is destroyed.  Expects the closure data to be
 * a pointer to a boolean; will set it to TRUE.
 */
static void
comp_destroy_cb (gpointer data, GObject *deadbeef)
{
	gboolean *destroyed;

	destroyed = data;
	*destroyed = TRUE;
}


static gboolean
e_week_view_on_text_item_event (GnomeCanvasItem *item,
				GdkEvent *gdkevent,
				EWeekView *week_view)
{
	EWeekViewEvent *event;
	gint event_num, span_num;
	GnomeCalendar *calendar;

#if 0
	g_print ("In e_week_view_on_text_item_event\n");
#endif

	calendar = e_cal_view_get_calendar (E_CAL_VIEW (week_view));

	switch (gdkevent->type) {
	case GDK_KEY_PRESS:
		if (gdkevent && gdkevent->key.keyval == GDK_Return) {
			/* We set the keyboard focus to the EDayView, so the
			   EText item loses it and stops the edit. */
			gtk_widget_grab_focus (GTK_WIDGET (week_view));

			/* Stop the signal last or we will also stop any
			   other events getting to the EText item. */
			gtk_signal_emit_stop_by_name (GTK_OBJECT (item),
						      "event");
			return TRUE;
		} else if (gdkevent->key.keyval == GDK_Escape) {
			cancel_editing (week_view);
			gtk_signal_emit_stop_by_name (GTK_OBJECT (item), "event");
			/* focus should go to week view when stop editing */
			gtk_widget_grab_focus (GTK_WIDGET (week_view));
			return TRUE;
		}
		break;
	case GDK_2BUTTON_PRESS:
		if (!e_week_view_find_event_from_item (week_view, item,
						       &event_num, &span_num))
			return FALSE;

		event = &g_array_index (week_view->events, EWeekViewEvent,
					event_num);

		if (calendar)
			gnome_calendar_edit_object (calendar, event->comp, FALSE);
		else
			g_warning ("Calendar not set");

		gtk_signal_emit_stop_by_name (GTK_OBJECT (item), "event");
		return TRUE;
	case GDK_BUTTON_PRESS:
		if (!e_week_view_find_event_from_item (week_view, item,
						       &event_num, &span_num))
			return FALSE;

		if (gdkevent->button.button == 3 && !E_TEXT (item)->editing) {
			EWeekViewEvent *e;
			gboolean destroyed;

			e = &g_array_index (week_view->events, EWeekViewEvent, event_num);

			destroyed = FALSE;
			g_object_weak_ref ((GObject *) e->comp, comp_destroy_cb, &destroyed);

			if (!GTK_WIDGET_HAS_FOCUS (week_view))
				gtk_widget_grab_focus (GTK_WIDGET (week_view));

			if (!destroyed) {
				g_object_weak_unref ((GObject *) e->comp, comp_destroy_cb, &destroyed);
	
				e_week_view_set_selected_time_range_visible (week_view, e->start, e->end);

				e_week_view_show_popup_menu (week_view,
							     (GdkEventButton*) gdkevent,
							     event_num);
			}

			gtk_signal_emit_stop_by_name (GTK_OBJECT (item->canvas),
						      "button_press_event");
			return TRUE;
		}

		if (gdkevent->button.button != 3) {
			week_view->pressed_event_num = event_num;
			week_view->pressed_span_num = span_num;
		}

		/* Only let the EText handle the event while editing. */
		if (!E_TEXT (item)->editing) {
			gtk_signal_emit_stop_by_name (GTK_OBJECT (item),
						      "event");

			if (gdkevent) {
				week_view->drag_event_x = gdkevent->button.x;
				week_view->drag_event_y = gdkevent->button.y;
			} else
				g_warning ("No GdkEvent");

			/* FIXME: Remember the day offset from the start of
			   the event, for DnD. */

			return TRUE;
		}
		break;
	case GDK_BUTTON_RELEASE:
		if (!E_TEXT (item)->editing) {
			/* This shouldn't ever happen. */
			if (!e_week_view_find_event_from_item (week_view,
							       item,
							       &event_num,
							       &span_num))
				return FALSE;

			if (week_view->pressed_event_num != -1
			    && week_view->pressed_event_num == event_num
			    && week_view->pressed_span_num == span_num) {
				e_week_view_start_editing_event (week_view,
								 event_num,
								 span_num,
								 NULL);
				week_view->pressed_event_num = -1;
			}

			/* Stop the signal last or we will also stop any
			   other events getting to the EText item. */
			gtk_signal_emit_stop_by_name (GTK_OBJECT (item),
						      "event");
			return TRUE;
		}
		week_view->pressed_event_num = -1;
		break;
	case GDK_FOCUS_CHANGE:
		if (gdkevent->focus_change.in) {
			e_week_view_on_editing_started (week_view, item);
		} else {
			e_week_view_on_editing_stopped (week_view, item);
		}

		return FALSE;
	default:
		break;
	}

	return FALSE;
}


static void
e_week_view_on_editing_started (EWeekView *week_view,
				GnomeCanvasItem *item)
{
	gint event_num, span_num;

	if (!e_week_view_find_event_from_item (week_view, item,
					       &event_num, &span_num))
		return;

#if 0
	g_print ("In e_week_view_on_editing_started event_num:%i span_num:%i\n", event_num, span_num);
#endif

	week_view->editing_event_num = event_num;
	week_view->editing_span_num = span_num;

	/* We need to reshape long events so the whole width is used while
	   editing. */
	if (!e_week_view_is_one_day_event (week_view, event_num)) {
		e_week_view_reshape_event_span (week_view, event_num,
						span_num);
	}

	g_object_set (item, "handle_popup", TRUE, NULL);

	g_signal_emit_by_name (week_view, "selection_changed");
}


static void
e_week_view_on_editing_stopped (EWeekView *week_view,
				GnomeCanvasItem *item)
{
	gint event_num, span_num;
	EWeekViewEvent *event;
	EWeekViewEventSpan *span;
	gchar *text = NULL;
	CalComponentText summary;
	const char *uid;

	/* Note: the item we are passed here isn't reliable, so we just stop
	   the edit of whatever item was being edited. We also receive this
	   event twice for some reason. */
	event_num = week_view->editing_event_num;
	span_num = week_view->editing_span_num;

	/* If no item is being edited, just return. */
	if (event_num == -1)
		return;

	event = &g_array_index (week_view->events, EWeekViewEvent, event_num);
	span = &g_array_index (week_view->spans, EWeekViewEventSpan,
			       event->spans_index + span_num);

	/* Reset the edit fields. */
	week_view->editing_event_num = -1;

	/* Check that the event is still valid. */
	cal_component_get_uid (event->comp, &uid);
	if (!uid)
		return;

	g_object_set (span->text_item, "handle_popup", FALSE, NULL);
	g_object_get (G_OBJECT (span->text_item), "text", &text, NULL);
	g_assert (text != NULL);

	if (string_is_empty (text) &&
	    !cal_comp_is_on_server (event->comp,
				    e_cal_view_get_cal_client (E_CAL_VIEW (week_view)))) {
		const char *uid;
		
		cal_component_get_uid (event->comp, &uid);
		
		e_week_view_foreach_event_with_uid (week_view, uid,
						    e_week_view_remove_event_cb, NULL);
		gtk_widget_queue_draw (week_view->main_canvas);
		e_week_view_check_layout (week_view);
		goto out;
	}

	/* Only update the summary if necessary. */
	cal_component_get_summary (event->comp, &summary);
	if (summary.value && !strcmp (text, summary.value)) {
		if (!e_week_view_is_one_day_event (week_view, event_num))
			e_week_view_reshape_event_span (week_view, event_num,
							span_num);
	} else if (summary.value || !string_is_empty (text)) {
		CalClient *client;
		summary.value = text;
		summary.altrep = NULL;
		cal_component_set_summary (event->comp, &summary);

		client = e_cal_view_get_cal_client (E_CAL_VIEW (week_view));
		if (cal_component_is_instance (event->comp)) {
			CalObjModType mod;

			if (recur_component_dialog (event->comp, &mod, NULL)) {
				if (cal_client_update_object_with_mod (client, event->comp, mod)
				    == CAL_CLIENT_RESULT_SUCCESS) {
					if (itip_organizer_is_user (event->comp, client) 
					    && send_component_dialog (gtk_widget_get_toplevel (week_view),
								      client, event->comp, FALSE))
						itip_send_comp (CAL_COMPONENT_METHOD_REQUEST, event->comp, 
								client, NULL);
				} else {
					g_message ("e_week_view_on_editing_stopped(): Could not update the object!");
				}
			}
		} else if (cal_client_update_object (client, event->comp) == CAL_CLIENT_RESULT_SUCCESS) {
			if (itip_organizer_is_user (event->comp, client) &&
			    send_component_dialog (gtk_widget_get_toplevel (week_view),
						   client, event->comp, FALSE))
				itip_send_comp (CAL_COMPONENT_METHOD_REQUEST, event->comp,
						client, NULL);
		} else {
			g_message ("e_week_view_on_editing_stopped(): Could not update the object!");
		}
	}

 out:

	g_free (text);

	g_signal_emit_by_name (week_view, "selection_changed");
}


static gboolean
e_week_view_find_event_from_item (EWeekView	  *week_view,
				  GnomeCanvasItem *item,
				  gint		  *event_num_return,
				  gint		  *span_num_return)
{
	EWeekViewEvent *event;
	EWeekViewEventSpan *span;
	gint event_num, span_num, num_events;

	num_events = week_view->events->len;
	for (event_num = 0; event_num < num_events; event_num++) {
		event = &g_array_index (week_view->events, EWeekViewEvent,
					event_num);
		for (span_num = 0; span_num < event->num_spans; span_num++) {
			span = &g_array_index (week_view->spans,
					       EWeekViewEventSpan,
					       event->spans_index + span_num);
			if (span->text_item == item) {
				*event_num_return = event_num;
				*span_num_return = span_num;
				return TRUE;
			}
		}
	}

	return FALSE;
}


/* Finds the index of the event with the given uid.
   Returns TRUE if an event with the uid was found.
   Note that for recurring events there may be several EWeekViewEvents, one
   for each instance, all with the same iCalObject and uid. So only use this
   function if you know the event doesn't recur or you are just checking to
   see if any events with the uid exist. */
static gboolean
e_week_view_find_event_from_uid (EWeekView	  *week_view,
				 const gchar	  *uid,
				 gint		  *event_num_return)
{
	EWeekViewEvent *event;
	gint event_num, num_events;

	num_events = week_view->events->len;
	for (event_num = 0; event_num < num_events; event_num++) {
		const char *u;

		event = &g_array_index (week_view->events, EWeekViewEvent,
					event_num);

		cal_component_get_uid (event->comp, &u);
		if (u && !strcmp (uid, u)) {
			*event_num_return = event_num;
			return TRUE;
		}
	}

	return FALSE;
}


gboolean
e_week_view_is_one_day_event	(EWeekView	*week_view,
				 gint		 event_num)
{
	EWeekViewEvent *event;
	EWeekViewEventSpan *span;

	event = &g_array_index (week_view->events, EWeekViewEvent, event_num);
	if (event->num_spans != 1)
		return FALSE;

	span = &g_array_index (week_view->spans, EWeekViewEventSpan,
			       event->spans_index);

	if (event->start == week_view->day_starts[span->start_day]
	    && event->end == week_view->day_starts[span->start_day + 1])
		return FALSE;

	if (span->num_days == 1
	    && event->start >= week_view->day_starts[span->start_day]
	    && event->end <= week_view->day_starts[span->start_day + 1])
		return TRUE;

	return FALSE;
}

static gboolean
e_week_view_do_key_press (GtkWidget *widget, GdkEventKey *event)
{
	EWeekView *week_view;
	CalComponent *comp;
	gint event_num;
	gchar *initial_text;
	CalComponentDateTime date;
	struct icaltimetype itt;
	time_t dtstart, dtend;
	const char *uid;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (E_IS_WEEK_VIEW (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	week_view = E_WEEK_VIEW (widget);

	if (cal_client_get_load_state (e_cal_view_get_cal_client (E_CAL_VIEW (week_view)))
	    != CAL_CLIENT_LOAD_LOADED)
		return TRUE;

	/* The Escape key aborts a resize operation. */
#if 0
	if (week_view->resize_drag_pos != E_WEEK_VIEW_POS_NONE) {
		if (event->keyval == GDK_Escape) {
			e_week_view_abort_resize (week_view, event->time);
		}
		return FALSE;
	}
#endif

	if (week_view->selection_start_day == -1)
		return FALSE;

	/* We only want to start an edit with a return key or a simple
	   character. */
	if (event->keyval == GDK_Return) {
		initial_text = NULL;
	} else if (((event->keyval >= 0x20) && (event->keyval <= 0xFF)
		    && (event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)))
		   || (event->length == 0)
		   || (event->keyval == GDK_Tab)) {
		return FALSE;
	} else
		initial_text = e_utf8_from_gtk_event_key (widget, event->keyval, event->string);

	/* Add a new event covering the selected range. */
	comp = cal_comp_event_new_with_defaults (e_cal_view_get_cal_client (E_CAL_VIEW (week_view)));

	dtstart = week_view->day_starts[week_view->selection_start_day];
	dtend = week_view->day_starts[week_view->selection_end_day + 1];

	date.value = &itt;
	date.tzid = NULL;

	/* We use DATE values now, so we don't need the timezone. */
	/*date.tzid = icaltimezone_get_tzid (week_view->zone);*/

	*date.value = icaltime_from_timet_with_zone (dtstart, TRUE,
						     week_view->zone);
	cal_component_set_dtstart (comp, &date);

	*date.value = icaltime_from_timet_with_zone (dtend, TRUE,
						     week_view->zone);
	cal_component_set_dtend (comp, &date);

	cal_component_set_categories (comp, week_view->default_category);

	/* We add the event locally and start editing it. We don't send it
	   to the server until the user finishes editing it. */
	e_week_view_add_event (comp, dtstart, dtend, week_view);
	e_week_view_check_layout (week_view);
	gtk_widget_queue_draw (week_view->main_canvas);

	cal_component_get_uid (comp, &uid);
	if (e_week_view_find_event_from_uid (week_view, uid, &event_num)) {
		e_week_view_start_editing_event (week_view, event_num, 0,
						 initial_text);
	} else {
		g_warning ("Couldn't find event to start editing.\n");
	}

	if (initial_text)
		g_free (initial_text);

	g_object_unref (comp);

	return TRUE;
}

static gboolean
e_week_view_key_press (GtkWidget *widget, GdkEventKey *event)
{
	gboolean handled = FALSE;
	handled = e_week_view_do_key_press (widget, event);

	/* if not handled, try key bindings */
	if (!handled)
		handled = GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, event);
	return handled;
}

enum {
	/*
	 * This is used to "flag" events that can not be editted
	 */
	MASK_EDITABLE = 1,

	/*
	 * To disable recurring actions to be displayed
	 */
	MASK_RECURRING = 2,

	/*
	 * To disable actions for non-recurring items to be displayed
	 */
	MASK_SINGLE   = 4,

	/*
	 * This is used to when an event is currently being edited
	 * in another window and we want to disable the event
	 * from being edited twice
	 */
	MASK_EDITING  = 8,

	/*
	 * This is used to when an event is already a meeting and
	 * we want to disable the schedule meeting command
	 */
	MASK_MEETING  = 16,

	/*
	 * To disable cut and copy for meetings the user is not the
	 * organizer of
	 */
	MASK_MEETING_ORGANIZER = 32,

	/*
	 * To disable things not valid for instances
	 */
	MASK_INSTANCE = 64
};

static EPopupMenu main_items [] = {
	E_POPUP_ITEM (N_("New _Appointment..."), GTK_SIGNAL_FUNC (e_week_view_on_new_appointment), MASK_EDITABLE),
	E_POPUP_ITEM (N_("New All Day _Event"), GTK_SIGNAL_FUNC (e_week_view_on_new_event), MASK_EDITABLE),
	E_POPUP_ITEM (N_("New Meeting"), GTK_SIGNAL_FUNC (e_week_view_on_new_meeting), MASK_EDITABLE),
	E_POPUP_ITEM (N_("New Task"), GTK_SIGNAL_FUNC (e_week_view_on_new_task), MASK_EDITABLE),

	E_POPUP_SEPARATOR,

	E_POPUP_ITEM (N_("_Print..."), GTK_SIGNAL_FUNC (e_week_view_on_print), 0),

	E_POPUP_SEPARATOR,

	E_POPUP_ITEM (N_("_Paste"), GTK_SIGNAL_FUNC (e_week_view_on_paste), MASK_EDITABLE),

	E_POPUP_SEPARATOR,

	E_POPUP_SUBMENU (N_("Current View"), NULL, 0),
	
	E_POPUP_ITEM (N_("Go to _Today"), GTK_SIGNAL_FUNC (e_week_view_on_goto_today), 0),
	E_POPUP_ITEM (N_("_Go to Date..."), GTK_SIGNAL_FUNC (e_week_view_on_goto_date), 0),

	E_POPUP_SEPARATOR,

	E_POPUP_ITEM (N_("_Publish Free/Busy Information"), GTK_SIGNAL_FUNC (e_week_view_on_publish), 0),

	E_POPUP_SEPARATOR,

	E_POPUP_ITEM (N_("_Settings..."), GTK_SIGNAL_FUNC (e_week_view_on_settings), 0),

	E_POPUP_TERMINATOR
};

static EPopupMenu child_items [] = {
	E_POPUP_ITEM (N_("_Open"), GTK_SIGNAL_FUNC (e_week_view_on_edit_appointment), MASK_EDITING),
	E_POPUP_ITEM (N_("_Save As..."), GTK_SIGNAL_FUNC (e_week_view_on_save_as), MASK_EDITING),
	E_POPUP_ITEM (N_("_Print..."), GTK_SIGNAL_FUNC (e_week_view_on_print_event), MASK_EDITING),

	/* Only show this separator if one of the above is shown. */
	E_POPUP_SEPARATOR,

	E_POPUP_ITEM (N_("C_ut"), GTK_SIGNAL_FUNC (e_week_view_on_cut), MASK_EDITING | MASK_EDITABLE | MASK_MEETING_ORGANIZER),
	E_POPUP_ITEM (N_("_Copy"), GTK_SIGNAL_FUNC (e_week_view_on_copy), MASK_EDITING | MASK_MEETING_ORGANIZER),
	E_POPUP_ITEM (N_("_Paste"), GTK_SIGNAL_FUNC (e_week_view_on_paste), MASK_EDITABLE),

	E_POPUP_SEPARATOR,

	E_POPUP_ITEM (N_("_Schedule Meeting..."), GTK_SIGNAL_FUNC (e_week_view_on_meeting), MASK_EDITABLE | MASK_EDITING | MASK_MEETING),
	E_POPUP_ITEM (N_("_Forward as iCalendar..."), GTK_SIGNAL_FUNC (e_week_view_on_forward), MASK_EDITING),

	E_POPUP_SEPARATOR,

	E_POPUP_ITEM (N_("_Delete"), GTK_SIGNAL_FUNC (e_week_view_on_delete_appointment), MASK_EDITABLE | MASK_SINGLE | MASK_EDITING),
	E_POPUP_ITEM (N_("Make this Occurrence _Movable"), GTK_SIGNAL_FUNC (e_week_view_on_unrecur_appointment), MASK_RECURRING | MASK_EDITING | MASK_EDITABLE | MASK_INSTANCE),
	E_POPUP_ITEM (N_("Delete this _Occurrence"), GTK_SIGNAL_FUNC (e_week_view_on_delete_occurrence), MASK_RECURRING | MASK_EDITING | MASK_EDITABLE),
	E_POPUP_ITEM (N_("Delete _All Occurrences"), GTK_SIGNAL_FUNC (e_week_view_on_delete_appointment), MASK_RECURRING | MASK_EDITING | MASK_EDITABLE),

	E_POPUP_TERMINATOR
};

static void
free_view_popup (GtkWidget *widget, gpointer data)
{
	EWeekView *week_view = E_WEEK_VIEW (data);

	if (week_view->view_menu == NULL)
		return;
	
	gnome_calendar_discard_view_popup (e_cal_view_get_calendar (E_CAL_VIEW (week_view)),
					   week_view->view_menu);
	week_view->view_menu = NULL;
}

void
e_week_view_show_popup_menu (EWeekView	     *week_view,
			     GdkEventButton  *bevent,
			     gint	      event_num)
{
	EWeekViewEvent *event;
	int have_selection;
	gboolean being_edited;
	guint32 disable_mask = 0, hide_mask = 0;
	EPopupMenu *context_menu;
	GtkMenu *popup;
	
	have_selection = GTK_WIDGET_HAS_FOCUS (week_view)
		&& week_view->selection_start_day != -1;

	/*
	 * This used to be set only if the event wasn't being edited
	 * in the event editor, but we can't check that at present.
	 * We could possibly set up another method of checking it.
	 */
	being_edited = FALSE;

	if (event_num == -1) {
		week_view->view_menu = gnome_calendar_setup_view_popup (e_cal_view_get_calendar (E_CAL_VIEW (week_view)));
		main_items[9].submenu = week_view->view_menu;
		context_menu = main_items;
	} else {
		context_menu = child_items;
		event = &g_array_index (week_view->events,
					EWeekViewEvent, event_num);

		if (cal_component_has_recurrences (event->comp))
			hide_mask |= MASK_SINGLE;
		else
			hide_mask |= MASK_RECURRING;

		if (cal_component_is_instance (event->comp))
			hide_mask |= MASK_INSTANCE;

		if (cal_component_has_organizer (event->comp)) {
			disable_mask |= MASK_MEETING;

			if (!itip_organizer_is_user (event->comp,
						     e_cal_view_get_cal_client (E_CAL_VIEW (week_view))))
				disable_mask |= MASK_MEETING_ORGANIZER;
		}
	}

	if (cal_client_is_read_only (e_cal_view_get_cal_client (E_CAL_VIEW (week_view))))
		disable_mask |= MASK_EDITABLE;

	if (being_edited)
		disable_mask |= MASK_EDITING;
	week_view->popup_event_num = event_num;

	popup = e_popup_menu_create (context_menu, disable_mask, hide_mask, week_view);
	g_signal_connect (popup, "selection-done", G_CALLBACK (free_view_popup), week_view);
	e_popup_menu (popup, (GdkEvent *) bevent);
}

static gboolean
e_week_view_popup_menu (GtkWidget *widget)
{
	EWeekView *week_view = E_WEEK_VIEW (widget);
	e_week_view_show_popup_menu (week_view, NULL,
				     week_view->editing_event_num);
	return TRUE;
}

static void
e_week_view_on_new_appointment (GtkWidget *widget, gpointer data)
{
	EWeekView *week_view = E_WEEK_VIEW (data);

	e_week_view_new_appointment (week_view, FALSE);
}


static void
e_week_view_on_new_event (GtkWidget *widget, gpointer data)
{
	EWeekView *week_view = E_WEEK_VIEW (data);
	time_t dtstart, dtend;

	dtstart = week_view->day_starts[week_view->selection_start_day];
	dtend = week_view->day_starts[week_view->selection_end_day + 1];
	gnome_calendar_new_appointment_for (
		e_cal_view_get_calendar (E_CAL_VIEW (week_view)), dtstart, dtend, TRUE, FALSE);
}

static void
e_week_view_on_new_meeting (GtkWidget *widget, gpointer data)
{
	EWeekView *week_view = E_WEEK_VIEW (data);

	e_week_view_new_appointment (week_view, TRUE);
}

static void
e_week_view_on_new_task (GtkWidget *widget, gpointer data)
{
	EWeekView *week_view = E_WEEK_VIEW (data);

	gnome_calendar_new_task (e_cal_view_get_calendar (E_CAL_VIEW (week_view)));
}

static void
e_week_view_on_goto_date (GtkWidget *widget, gpointer data)
{
	EWeekView *week_view = E_WEEK_VIEW (data);

	goto_dialog (e_cal_view_get_calendar (E_CAL_VIEW (week_view)));
}

static void
e_week_view_on_goto_today (GtkWidget *widget, gpointer data)
{
	EWeekView *week_view = E_WEEK_VIEW (data);

	calendar_goto_today (e_cal_view_get_calendar (E_CAL_VIEW (week_view)));
}

static void
e_week_view_on_edit_appointment (GtkWidget *widget, gpointer data)
{
	EWeekView *week_view;
	EWeekViewEvent *event;
	GnomeCalendar *calendar;

	week_view = E_WEEK_VIEW (data);

	if (week_view->popup_event_num == -1)
		return;

	event = &g_array_index (week_view->events, EWeekViewEvent,
				week_view->popup_event_num);

	calendar = e_cal_view_get_calendar (E_CAL_VIEW (week_view));
	if (calendar)
		gnome_calendar_edit_object (calendar, event->comp, FALSE);
	else
		g_warning ("Calendar not set");
}

static void
e_week_view_on_print (GtkWidget *widget, gpointer data)
{
	EWeekView *week_view;
	time_t start;
	GnomeCalendarViewType view_type;
	PrintView print_view;

	week_view = E_WEEK_VIEW (data);

	gnome_calendar_get_current_time_range (e_cal_view_get_calendar (E_CAL_VIEW (week_view)), &start, NULL);
	view_type = gnome_calendar_get_view (e_cal_view_get_calendar (E_CAL_VIEW (week_view)));

	switch (view_type) {
	case GNOME_CAL_WEEK_VIEW:
		print_view = PRINT_VIEW_WEEK;
		break;

	case GNOME_CAL_MONTH_VIEW:
		print_view = PRINT_VIEW_MONTH;
		break;

	default:
		g_assert_not_reached ();
		return;
	}

	print_calendar (e_cal_view_get_calendar (E_CAL_VIEW (week_view)), FALSE, start, print_view);
}

static void
e_week_view_on_save_as (GtkWidget *widget, gpointer data)
{
	EWeekView *week_view;
	EWeekViewEvent *event;
	char *filename;
	char *ical_string;
	FILE *file;

	week_view = E_WEEK_VIEW (data);

	if (week_view->popup_event_num == -1)
		return;

	event = &g_array_index (week_view->events, EWeekViewEvent,
				week_view->popup_event_num);
	
	filename = e_file_dialog_save (_("Save as..."));
	if (filename == NULL)
		return;
	
	ical_string = cal_client_get_component_as_string (e_cal_view_get_cal_client (E_CAL_VIEW (week_view)),
							  event->comp);
	if (ical_string == NULL) {
		g_warning ("Couldn't convert item to a string");
		return;
	}
	
	file = fopen (filename, "w");
	if (file == NULL) {
		g_warning ("Couldn't save item");
		return;
	}
	
	fprintf (file, ical_string);
	g_free (ical_string);
	fclose (file);
}

static void
e_week_view_on_print_event (GtkWidget *widget, gpointer data)
{
	EWeekView *week_view;
	EWeekViewEvent *event;

	week_view = E_WEEK_VIEW (data);

	if (week_view->popup_event_num == -1)
		return;

	event = &g_array_index (week_view->events, EWeekViewEvent,
				week_view->popup_event_num);

	print_comp (event->comp, e_cal_view_get_cal_client (E_CAL_VIEW (week_view)), FALSE);
}

static void
e_week_view_on_meeting (GtkWidget *widget, gpointer data)
{
	EWeekView *week_view;
	EWeekViewEvent *event;
	GnomeCalendar *calendar;

	week_view = E_WEEK_VIEW (data);

	if (week_view->popup_event_num == -1)
		return;

	event = &g_array_index (week_view->events, EWeekViewEvent,
				week_view->popup_event_num);

	calendar = e_cal_view_get_calendar (E_CAL_VIEW (week_view));
	if (calendar)
		gnome_calendar_edit_object (calendar, event->comp, TRUE);
	else
		g_warning ("Calendar not set");
}

static void
e_week_view_on_forward (GtkWidget *widget, gpointer data)
{
	EWeekView *week_view;
	EWeekViewEvent *event;

	week_view = E_WEEK_VIEW (data);

	if (week_view->popup_event_num == -1)
		return;

	event = &g_array_index (week_view->events, EWeekViewEvent,
				week_view->popup_event_num);

	itip_send_comp (CAL_COMPONENT_METHOD_PUBLISH, event->comp, 
			e_cal_view_get_cal_client (E_CAL_VIEW (week_view)), NULL);
}

static void
e_week_view_on_publish (GtkWidget *widget, gpointer data)
{
	EWeekView *week_view;
	icaltimezone *utc;
	time_t start = time (NULL), end;
	GList *comp_list;

	week_view = E_WEEK_VIEW (data);

	utc = icaltimezone_get_utc_timezone ();
	start = time_day_begin_with_zone (start, utc);
	end = time_add_week_with_zone (start, 6, utc);

	comp_list = cal_client_get_free_busy (e_cal_view_get_cal_client (E_CAL_VIEW (week_view)), NULL,
					      start, end);
	if (comp_list) {
		GList *l;

		for (l = comp_list; l; l = l->next) {
			CalComponent *comp = CAL_COMPONENT (l->data);
			itip_send_comp (CAL_COMPONENT_METHOD_PUBLISH, comp, 
					e_cal_view_get_cal_client (E_CAL_VIEW (week_view)), NULL);

			g_object_unref (comp);
		}

 		g_list_free (comp_list);
	}
}

static void
e_week_view_on_settings (GtkWidget *widget, gpointer data)
{
	EWeekView *week_view;

	week_view = E_WEEK_VIEW (data);

	control_util_show_settings (e_cal_view_get_calendar (E_CAL_VIEW (week_view)));
}

static void
e_week_view_delete_event_internal (EWeekView *week_view, gint event_num)
{
	CalComponentVType vtype;
	EWeekViewEvent *event;

	event = &g_array_index (week_view->events, EWeekViewEvent,
				event_num);

	vtype = cal_component_get_vtype (event->comp);

	if (delete_component_dialog (event->comp, FALSE, 1, vtype,
				     GTK_WIDGET (week_view))) {
		const char *uid;

		if (itip_organizer_is_user (event->comp,
					    e_cal_view_get_cal_client (E_CAL_VIEW (week_view))) 
		    && cancel_component_dialog ((GtkWindow *) gtk_widget_get_toplevel (week_view),
						e_cal_view_get_cal_client (E_CAL_VIEW (week_view)),
						event->comp, TRUE))
			itip_send_comp (CAL_COMPONENT_METHOD_CANCEL, event->comp,
					e_cal_view_get_cal_client (E_CAL_VIEW (week_view)), NULL);

		cal_component_get_uid (event->comp, &uid);

		delete_error_dialog (
			cal_client_remove_object (e_cal_view_get_cal_client (E_CAL_VIEW (week_view)),
						  uid), CAL_COMPONENT_EVENT);
	}
}


static void
e_week_view_on_delete_appointment (GtkWidget *widget, gpointer data)
{
	EWeekView *week_view;

	week_view = E_WEEK_VIEW (data);

	if (week_view->popup_event_num == -1)
		return;

	e_week_view_delete_event_internal (week_view,
					   week_view->popup_event_num);
}


void
e_week_view_delete_event		(EWeekView       *week_view)
{
	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	if (week_view->editing_event_num == -1)
		return;

	e_week_view_delete_event_internal (week_view,
					   week_view->editing_event_num);
}


static void
e_week_view_delete_occurrence_internal (EWeekView *week_view, gint event_num)
{
	EWeekViewEvent *event;
	CalComponent *comp;

	event = &g_array_index (week_view->events, EWeekViewEvent,
				event_num);

	if (cal_component_is_instance (event->comp)) {
		const char *uid;

		cal_component_get_uid (event->comp, &uid);
		delete_error_dialog (cal_client_remove_object_with_mod (
					     e_cal_view_get_cal_client (E_CAL_VIEW (week_view)),
					     uid, CALOBJ_MOD_THIS),
				     CAL_COMPONENT_EVENT);
		return;
	}

	/* We must duplicate the CalComponent, or we won't know it has changed
	   when we get the "update_event" callback. */

	comp = cal_component_clone (event->comp);
	cal_comp_util_add_exdate (comp, event->start, week_view->zone);

	if (cal_client_update_object (e_cal_view_get_cal_client (E_CAL_VIEW (week_view)), comp)
	    != CAL_CLIENT_RESULT_SUCCESS)
		g_message ("e_week_view_on_delete_occurrence(): Could not update the object!");

	g_object_unref (comp);
}

static void
e_week_view_on_delete_occurrence (GtkWidget *widget, gpointer data)
{
	EWeekView *week_view;

	week_view = E_WEEK_VIEW (data);

	if (week_view->popup_event_num == -1)
		return;

	e_week_view_delete_occurrence_internal (week_view, week_view->popup_event_num);
}

void
e_week_view_delete_occurrence (EWeekView *week_view)
{
	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	if (week_view->editing_event_num == -1)
		return;

	e_week_view_delete_occurrence_internal (week_view, week_view->editing_event_num);
}

static void
e_week_view_on_cut (GtkWidget *widget, gpointer data)
{
	EWeekView *week_view = E_WEEK_VIEW (data);

	e_cal_view_cut_clipboard (E_CAL_VIEW (week_view));
}

static void
e_week_view_on_copy (GtkWidget *widget, gpointer data)
{
	EWeekView *week_view = E_WEEK_VIEW (data);

	e_cal_view_copy_clipboard (E_CAL_VIEW (week_view));
}

static void
e_week_view_on_paste (GtkWidget *widget, gpointer data)
{
	EWeekView *week_view = E_WEEK_VIEW (data);

	e_cal_view_paste_clipboard (E_CAL_VIEW (week_view));
}

static void
e_week_view_on_unrecur_appointment (GtkWidget *widget, gpointer data)
{
	EWeekView *week_view;
	EWeekViewEvent *event;
	CalComponent *comp, *new_comp;
	CalComponentDateTime date;
	struct icaltimetype itt;

	week_view = E_WEEK_VIEW (data);

	if (week_view->popup_event_num == -1)
		return;

	event = &g_array_index (week_view->events, EWeekViewEvent,
				week_view->popup_event_num);

	/* For the recurring object, we add a exception to get rid of the
	   instance. */
	comp = cal_component_clone (event->comp);
	cal_comp_util_add_exdate (comp, event->start, week_view->zone);

	/* For the unrecurred instance we duplicate the original object,
	   create a new uid for it, get rid of the recurrence rules, and set
	   the start & end times to the instances times. */
	new_comp = cal_component_clone (event->comp);
	cal_component_set_uid (new_comp, cal_component_gen_uid ());
	cal_component_set_rdate_list (new_comp, NULL);
	cal_component_set_rrule_list (new_comp, NULL);
	cal_component_set_exdate_list (new_comp, NULL);
	cal_component_set_exrule_list (new_comp, NULL);

	date.value = &itt;
	date.tzid = icaltimezone_get_tzid (week_view->zone);

	*date.value = icaltime_from_timet_with_zone (event->start, FALSE,
						     week_view->zone);
	cal_component_set_dtstart (new_comp, &date);
	*date.value = icaltime_from_timet_with_zone (event->end, FALSE,
						     week_view->zone);
	cal_component_set_dtend (new_comp, &date);

	/* Now update both CalComponents. Note that we do this last since at
	   present the updates happen synchronously so our event may disappear.
	*/
	if (cal_client_update_object (e_cal_view_get_cal_client (E_CAL_VIEW (week_view)), comp)
	    != CAL_CLIENT_RESULT_SUCCESS)
		g_message ("e_week_view_on_unrecur_appointment(): Could not update the object!");

	g_object_unref (comp);

	if (cal_client_update_object (e_cal_view_get_cal_client (E_CAL_VIEW (week_view)), new_comp)
	    != CAL_CLIENT_RESULT_SUCCESS)
		g_message ("e_week_view_on_unrecur_appointment(): Could not update the object!");

	g_object_unref (new_comp);
}


static gboolean
e_week_view_on_jump_button_event (GnomeCanvasItem *item,
				  GdkEvent *event,
				  EWeekView *week_view)
{
	gint day;

	if (event->type == GDK_BUTTON_PRESS) {
		for (day = 0; day < E_WEEK_VIEW_MAX_WEEKS * 7; day++) {
			if (item == week_view->jump_buttons[day]) {
				GnomeCalendar *calendar;

				calendar = e_cal_view_get_calendar (E_CAL_VIEW (week_view));
				if (calendar)
					gnome_calendar_dayjump
						(calendar,
						 week_view->day_starts[day]);
				else
					g_warning ("Calendar not set");
				return TRUE;
			}
		}

	}

	return FALSE;
}


/* Converts an hour from 0-23 to the preferred time format, and returns the
   suffix to add and the width of it in the normal font. */
void
e_week_view_convert_time_to_display	(EWeekView	*week_view,
					 gint		 hour,
					 gint		*display_hour,
					 gchar	       **suffix,
					 gint		*suffix_width)
{
	/* Calculate the actual hour number to display. For 12-hour
	   format we convert 0-23 to 12-11am/12-11pm. */
	*display_hour = hour;
	if (week_view->use_24_hour_format) {
		*suffix = "";
		*suffix_width = 0;
	} else {
		if (hour < 12) {
			*suffix = week_view->am_string;
			*suffix_width = week_view->am_string_width;
		} else {
			*display_hour -= 12;
			*suffix = week_view->pm_string;
			*suffix_width = week_view->pm_string_width;
		}

		/* 12-hour uses 12:00 rather than 0:00. */
		if (*display_hour == 0)
			*display_hour = 12;
	}
}


gint
e_week_view_get_time_string_width	(EWeekView	*week_view)
{
	gint time_width;

	if (week_view->use_small_font && week_view->small_font_desc)
		time_width = week_view->digit_width * 2
			+ week_view->small_digit_width * 2;
	else
		time_width = week_view->digit_width * 4
			+ week_view->colon_width;

	if (!week_view->use_24_hour_format)
		time_width += MAX (week_view->am_string_width,
				   week_view->pm_string_width);

	return time_width;
}

/* Gets the visible time range. Returns FALSE if no time range has been set. */
gboolean
e_week_view_get_visible_time_range	(EWeekView	*week_view,
					 time_t		*start_time,
					 time_t		*end_time)
{
	gint num_days;

	/* If we don't have a valid date set yet, return FALSE. */
	if (!g_date_valid (&week_view->first_day_shown))
		return FALSE;

	num_days = week_view->multi_week_view ? week_view->weeks_shown * 7 : 7;
	*start_time = week_view->day_starts[0];
	*end_time = week_view->day_starts[num_days];

	return TRUE;
}


/* Queues a layout, unless one is already queued. */
static void
e_week_view_queue_layout (EWeekView *week_view)
{
	if (week_view->layout_timeout_id == 0) {
		week_view->layout_timeout_id = g_timeout_add (E_WEEK_VIEW_LAYOUT_TIMEOUT, e_week_view_layout_timeout_cb, week_view);
	}
}


/* Removes any queued layout. */
static void
e_week_view_cancel_layout (EWeekView *week_view)
{
	if (week_view->layout_timeout_id != 0) {
		gtk_timeout_remove (week_view->layout_timeout_id);
		week_view->layout_timeout_id = 0;
	}
}


static gboolean
e_week_view_layout_timeout_cb (gpointer data)
{
	EWeekView *week_view = E_WEEK_VIEW (data);

	gtk_widget_queue_draw (week_view->main_canvas);
	e_week_view_check_layout (week_view);

	week_view->layout_timeout_id = 0;
	return FALSE;
}


/* Returns the number of selected events (0 or 1 at present). */
gint
e_week_view_get_num_events_selected (EWeekView *week_view)
{
	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), 0);

	return (week_view->editing_event_num != -1) ? 1 : 0;
}


