/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@helixcode.com>
 *
 * Copyright 1999, Helix Code, Inc.
 *
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

/*
 * EWeekView - displays the Week & Month views of the calendar.
 */

#include <config.h>
#include <math.h>
#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gnome-canvas-pixbuf.h>
#include "calendar-commands.h"
#include "e-week-view.h"
#include "e-week-view-event-item.h"
#include "e-week-view-main-item.h"
#include "e-week-view-titles-item.h"
#include <cal-util/timeutil.h>
#include "popup-menu.h"
#include "../e-util/e-canvas.h"
#include "../widgets/e-text/e-text.h"
#include "e-util/e-canvas-utils.h"

/* Images */
#include "bell.xpm"
#include "recur.xpm"

#include "jump.xpm"

#define E_WEEK_VIEW_SMALL_FONT	\
	"-adobe-utopia-regular-r-normal-*-*-100-*-*-p-*-iso8859-*"
#define E_WEEK_VIEW_SMALL_FONT_FALLBACK	\
	"-adobe-helvetica-medium-r-normal-*-*-80-*-*-p-*-iso8859-*"

/* We use a 7-bit field to store row numbers in EWeekViewEventSpan, so the
   maximum number or rows we can allow is 127. It is very unlikely to be
   reached anyway. */
#define E_WEEK_VIEW_MAX_ROWS_PER_CELL	127

#define E_WEEK_VIEW_JUMP_BUTTON_WIDTH	16
#define E_WEEK_VIEW_JUMP_BUTTON_HEIGHT	8

#define E_WEEK_VIEW_JUMP_BUTTON_X_PAD	3
#define E_WEEK_VIEW_JUMP_BUTTON_Y_PAD	3

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
static void e_week_view_draw (GtkWidget    *widget,
			      GdkRectangle *area);
static void e_week_view_draw_shadow (EWeekView *week_view);

static gboolean e_week_view_on_button_press (GtkWidget *widget,
					     GdkEventButton *event,
					     EWeekView *week_view);
static gboolean e_week_view_on_button_release (GtkWidget *widget,
					       GdkEventButton *event,
					       EWeekView *week_view);
static gboolean e_week_view_on_motion (GtkWidget *widget,
				       GdkEventMotion *event,
				       EWeekView *week_view);
static gint e_week_view_convert_position_to_day (EWeekView *week_view,
						 gint x,
						 gint y);
static void e_week_view_update_selection (EWeekView *week_view,
					  gint day);

static void e_week_view_reload_events (EWeekView *week_view);
static void e_week_view_free_events (EWeekView *week_view);
static gboolean e_week_view_add_event (CalComponent *comp,
				       time_t	  start,
				       time_t	  end,
				       gpointer	  data);
static void e_week_view_check_layout (EWeekView *week_view);
static void e_week_view_layout_events (EWeekView *week_view);
static void e_week_view_layout_event (EWeekView	   *week_view,
				      EWeekViewEvent   *event,
				      guint8	   *grid,
				      GArray	   *spans);
static void e_week_view_ensure_events_sorted (EWeekView *week_view);
static gint e_week_view_event_sort_func (const void *arg1,
					 const void *arg2);
static void e_week_view_reshape_events (EWeekView *week_view);
static void e_week_view_reshape_event_span (EWeekView *week_view,
					    gint event_num,
					    gint span_num);
static gint e_week_view_find_day (EWeekView *week_view,
				  time_t time_to_find,
				  gboolean include_midnight_in_prev_day);
static gint e_week_view_find_span_end (EWeekView *week_view,
				       gint day);
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
static gint e_week_view_key_press (GtkWidget *widget, GdkEventKey *event);
static void e_week_view_on_new_appointment (GtkWidget *widget,
					    gpointer data);
static void e_week_view_on_edit_appointment (GtkWidget *widget,
					     gpointer data);
static void e_week_view_on_delete_occurrence (GtkWidget *widget,
					      gpointer data);
static void e_week_view_on_delete_appointment (GtkWidget *widget,
					       gpointer data);
static void e_week_view_on_unrecur_appointment (GtkWidget *widget,
						gpointer data);

#ifndef NO_WARNINGS
static gboolean e_week_view_update_event_cb (EWeekView *week_view,
					     gint event_num,
					     gpointer data);
#endif
static gboolean e_week_view_remove_event_cb (EWeekView *week_view,
					     gint event_num,
					     gpointer data);

static GtkTableClass *parent_class;


GtkType
e_week_view_get_type (void)
{
	static GtkType e_week_view_type = 0;

	if (!e_week_view_type){
		GtkTypeInfo e_week_view_info = {
			"EWeekView",
			sizeof (EWeekView),
			sizeof (EWeekViewClass),
			(GtkClassInitFunc) e_week_view_class_init,
			(GtkObjectInitFunc) e_week_view_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		parent_class = gtk_type_class (GTK_TYPE_TABLE);
		e_week_view_type = gtk_type_unique (GTK_TYPE_TABLE,
						    &e_week_view_info);
	}

	return e_week_view_type;
}


static void
e_week_view_class_init (EWeekViewClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	/* Method override */
	object_class->destroy		= e_week_view_destroy;

	widget_class->realize		= e_week_view_realize;
	widget_class->unrealize		= e_week_view_unrealize;
	widget_class->style_set		= e_week_view_style_set;
 	widget_class->size_allocate	= e_week_view_size_allocate;
	widget_class->focus_in_event	= e_week_view_focus_in;
	widget_class->focus_out_event	= e_week_view_focus_out;
	widget_class->key_press_event	= e_week_view_key_press;
	widget_class->expose_event	= e_week_view_expose_event;
	widget_class->draw		= e_week_view_draw;
}


static void
e_week_view_init (EWeekView *week_view)
{
	GdkColormap *colormap;
	gboolean success[E_WEEK_VIEW_COLOR_LAST];
	gint nfailed;
	GnomeCanvasGroup *canvas_group;
	GtkObject *adjustment;
	GdkPixbuf *pixbuf;
	gint i;

	GTK_WIDGET_SET_FLAGS (week_view, GTK_CAN_FOCUS);

	colormap = gtk_widget_get_colormap (GTK_WIDGET (week_view));

	week_view->calendar = NULL;

	week_view->events = g_array_new (FALSE, FALSE,
					 sizeof (EWeekViewEvent));
	week_view->events_sorted = TRUE;
	week_view->events_need_layout = FALSE;
	week_view->events_need_reshape = FALSE;

	week_view->spans = NULL;

	week_view->display_month = FALSE;
	week_view->rows = 6;
	week_view->columns = 2;
	week_view->compress_weekend = TRUE;

	g_date_clear (&week_view->base_date, 1);
	g_date_clear (&week_view->first_day_shown, 1);

	week_view->row_height = 10;
	week_view->rows_per_cell = 1;

	week_view->selection_start_day = -1;
	week_view->selection_drag_pos = E_WEEK_VIEW_DRAG_NONE;

	week_view->pressed_event_num = -1;
	week_view->editing_event_num = -1;
	week_view->editing_new_event = FALSE;

	/* Create the small font. */
	week_view->use_small_font = TRUE;
	week_view->small_font = gdk_font_load (E_WEEK_VIEW_SMALL_FONT);
	if (!week_view->small_font)
		week_view->small_font = gdk_font_load (E_WEEK_VIEW_SMALL_FONT_FALLBACK);
	if (!week_view->small_font)
		g_warning ("Couldn't load font");

	/* Allocate the colors. */
	week_view->colors[E_WEEK_VIEW_COLOR_EVEN_MONTHS].red   = 0xeded;
	week_view->colors[E_WEEK_VIEW_COLOR_EVEN_MONTHS].green = 0xeded;
	week_view->colors[E_WEEK_VIEW_COLOR_EVEN_MONTHS].blue  = 0xeded;

	week_view->colors[E_WEEK_VIEW_COLOR_ODD_MONTHS].red   = 65535;
	week_view->colors[E_WEEK_VIEW_COLOR_ODD_MONTHS].green = 65535;
	week_view->colors[E_WEEK_VIEW_COLOR_ODD_MONTHS].blue  = 65535;

	week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BACKGROUND].red   = 0xd6d6;
	week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BACKGROUND].green = 0xd6d6;
	week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BACKGROUND].blue  = 0xd6d6;

	week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BORDER].red   = 0;
	week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BORDER].green = 0;
	week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BORDER].blue  = 0;

	nfailed = gdk_colormap_alloc_colors (colormap, week_view->colors,
					     E_WEEK_VIEW_COLOR_LAST, FALSE,
					     TRUE, success);
	if (nfailed)
		g_warning ("Failed to allocate all colors");


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

	gtk_signal_connect_after (GTK_OBJECT (week_view->main_canvas),
				  "button_press_event",
				  GTK_SIGNAL_FUNC (e_week_view_on_button_press),
				  week_view);
	gtk_signal_connect_after (GTK_OBJECT (week_view->main_canvas),
				  "button_release_event",
				  GTK_SIGNAL_FUNC (e_week_view_on_button_release),
				  week_view);
	gtk_signal_connect_after (GTK_OBJECT (week_view->main_canvas),
				  "motion_notify_event",
				  GTK_SIGNAL_FUNC (e_week_view_on_motion),
				  week_view);

	/* Create the buttons to jump to each days. */
	pixbuf = gdk_pixbuf_new_from_xpm_data ((const char**) jump_xpm);

	for (i = 0; i < E_WEEK_VIEW_MAX_WEEKS * 7; i++) {
		week_view->jump_buttons[i] = gnome_canvas_item_new
			(canvas_group,
			 gnome_canvas_pixbuf_get_type (),
			 "GnomeCanvasPixbuf::pixbuf", pixbuf,
			 NULL);

		gtk_signal_connect (GTK_OBJECT (week_view->jump_buttons[i]),
				    "event",
				    GTK_SIGNAL_FUNC (e_week_view_on_jump_button_event),
				    week_view);
	}


	/*
	 * Scrollbar.
	 */
	adjustment = gtk_adjustment_new (0, -52, 52, 1, 1, 1);
	gtk_signal_connect (adjustment, "value_changed",
			    GTK_SIGNAL_FUNC (e_week_view_on_adjustment_changed),
			    week_view);

	week_view->vscrollbar = gtk_vscrollbar_new (GTK_ADJUSTMENT (adjustment));
	gtk_table_attach (GTK_TABLE (week_view), week_view->vscrollbar,
			  2, 3, 1, 2, 0, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show (week_view->vscrollbar);


	/* Create the pixmaps. */
	week_view->reminder_icon = gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap, &week_view->reminder_mask, NULL, bell_xpm);
	week_view->recurrence_icon = gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap, &week_view->recurrence_mask, NULL, recur_xpm);


	/* Create the cursors. */
	week_view->normal_cursor = gdk_cursor_new (GDK_TOP_LEFT_ARROW);
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

	week_view = GTK_WIDGET (gtk_type_new (e_week_view_get_type ()));

	return week_view;
}


static void
e_week_view_destroy (GtkObject *object)
{
	EWeekView *week_view;

	week_view = E_WEEK_VIEW (object);

	/* FIXME: free the colors. In EDayView as well. */
	/* FIXME: free the events and the spans. In EDayView as well? */

	if (week_view->small_font)
		gdk_font_unref (week_view->small_font);

	gdk_cursor_destroy (week_view->normal_cursor);
	gdk_cursor_destroy (week_view->move_cursor);
	gdk_cursor_destroy (week_view->resize_width_cursor);

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}


static void
e_week_view_realize (GtkWidget *widget)
{
	EWeekView *week_view;

	if (GTK_WIDGET_CLASS (parent_class)->realize)
		(*GTK_WIDGET_CLASS (parent_class)->realize)(widget);

	week_view = E_WEEK_VIEW (widget);
	week_view->main_gc = gdk_gc_new (widget->window);
}


static void
e_week_view_unrealize (GtkWidget *widget)
{
	EWeekView *week_view;

	week_view = E_WEEK_VIEW (widget);

	gdk_gc_unref (week_view->main_gc);
	week_view->main_gc = NULL;

	if (GTK_WIDGET_CLASS (parent_class)->unrealize)
		(*GTK_WIDGET_CLASS (parent_class)->unrealize)(widget);
}


static void
e_week_view_style_set (GtkWidget *widget,
		      GtkStyle  *previous_style)
{
	EWeekView *week_view;
	GdkFont *font;
	gint day, day_width, max_day_width, max_abbr_day_width;
	gint month, month_width, max_month_width, max_abbr_month_width;
	GDate date;
	gchar buffer[128];

	if (GTK_WIDGET_CLASS (parent_class)->style_set)
		(*GTK_WIDGET_CLASS (parent_class)->style_set)(widget, previous_style);

	week_view = E_WEEK_VIEW (widget);
	font = widget->style->font;

	/* Recalculate the height of each row based on the font size. */
	week_view->row_height = font->ascent + font->descent + E_WEEK_VIEW_EVENT_BORDER_HEIGHT * 2 + E_WEEK_VIEW_EVENT_TEXT_Y_PAD * 2;
	week_view->row_height = MAX (week_view->row_height, E_WEEK_VIEW_ICON_HEIGHT + E_WEEK_VIEW_ICON_Y_PAD + E_WEEK_VIEW_EVENT_BORDER_HEIGHT * 2);

	/* Set the height of the top canvas. */
	gtk_widget_set_usize (week_view->titles_canvas, -1,
			      font->ascent + font->descent + 5);

	/* Save the sizes of various strings in the font, so we can quickly
	   decide which date formats to use. */
	g_date_clear (&date, 1);
	g_date_set_dmy (&date, 27, 3, 2000);	/* Must be a Monday. */

	max_day_width = 0;
	max_abbr_day_width = 0;
	for (day = 0; day < 7; day++) {
		g_date_strftime (buffer, 128, "%A", &date);
		day_width = gdk_string_width (font, buffer);
		week_view->day_widths[day] = day_width;
		max_day_width = MAX (max_day_width, day_width);
				       
		g_date_strftime (buffer, 128, "%a", &date);
		day_width = gdk_string_width (font, buffer);
		week_view->abbr_day_widths[day] = day_width;
		max_abbr_day_width = MAX (max_abbr_day_width, day_width);

		g_date_add_days (&date, 1);
	}

	max_month_width = 0;
	max_abbr_month_width = 0;
	for (month = 0; month < 12; month++) {
		g_date_set_month (&date, month + 1);

		g_date_strftime (buffer, 128, "%B", &date);
		month_width = gdk_string_width (font, buffer);
		week_view->month_widths[month] = month_width;
		max_month_width = MAX (max_month_width, month_width);
				       
		g_date_strftime (buffer, 128, "%b", &date);
		month_width = gdk_string_width (font, buffer);
		week_view->abbr_month_widths[month] = month_width;
		max_abbr_month_width = MAX (max_abbr_month_width, month_width);
	}

	week_view->space_width = gdk_string_width (font, " ");
	week_view->colon_width = gdk_string_width (font, ":");
	week_view->slash_width = gdk_string_width (font, "/");
	week_view->digit_width = gdk_string_width (font, "5");
	if (week_view->small_font)
		week_view->small_digit_width = gdk_string_width (week_view->small_font, "5");
	week_view->max_day_width = max_day_width;
	week_view->max_abbr_day_width = max_abbr_day_width;
	week_view->max_month_width = max_month_width;
	week_view->max_abbr_month_width = max_abbr_month_width;
}


/* This recalculates the sizes of each column. */
static void
e_week_view_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	EWeekView *week_view;
	gint width, height, time_width;
	gdouble old_x2, old_y2, new_x2, new_y2;
	GdkFont *font;

	week_view = E_WEEK_VIEW (widget);
	font = widget->style->font;

	(*GTK_WIDGET_CLASS (parent_class)->size_allocate) (widget, allocation);

	e_week_view_recalc_cell_sizes (week_view);

	/* Calculate the number of rows of events in each cell, for the large
	   cells and the compressed weekend cells. */
	if (week_view->display_month) {
		week_view->events_y_offset = E_WEEK_VIEW_DATE_T_PAD
			+ font->ascent + font->descent
			+ E_WEEK_VIEW_DATE_B_PAD;
	} else {
		week_view->events_y_offset = E_WEEK_VIEW_DATE_T_PAD
			+ font->ascent + font->descent
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

	week_view->time_format = E_WEEK_VIEW_TIME_NONE;
	if (week_view->use_small_font && week_view->small_font) {
		time_width = week_view->digit_width * 2
			+ week_view->small_digit_width * 2;
		if (width / 2 > time_width * 2 + week_view->space_width)
			week_view->time_format = E_WEEK_VIEW_TIME_BOTH_SMALL_MIN;
		else if (width / 2 > time_width)
			week_view->time_format = E_WEEK_VIEW_TIME_START_SMALL_MIN;
	} else {
		time_width = week_view->digit_width * 4
			+ week_view->colon_width;
		if (width / 2 > time_width * 2 + week_view->space_width)
			week_view->time_format = E_WEEK_VIEW_TIME_BOTH;
		else if (width / 2 > time_width)
			week_view->time_format = E_WEEK_VIEW_TIME_START;
	}

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
	gfloat width, height, offset;
	gint row, col;

	if (week_view->display_month) {
		week_view->rows = 10;
		week_view->columns = 6;
	} else {
		week_view->rows = 6;
		week_view->columns = 2;
	}

	/* Calculate the column sizes, using floating point so that pixels
	   get divided evenly. Note that we use one more element than the
	   number of columns, to make it easy to get the column widths.
	   We also add one to the width so that the right border of the last
	   column is off the edge of the displayed area. */
	width = week_view->main_canvas->allocation.width + 1;
	width /= week_view->columns;
	offset = 0;
	for (col = 0; col <= week_view->columns; col++) {
		week_view->col_offsets[col] = floor (offset + 0.5);
		offset += width;
	}

	/* Calculate the cell widths based on the offsets. */
	for (col = 0; col < week_view->columns; col++) {
		week_view->col_widths[col] = week_view->col_offsets[col + 1]
			- week_view->col_offsets[col];
	}

	/* Now do the same for the row heights. */
	height = week_view->main_canvas->allocation.height + 1;
	height /= week_view->rows;
	offset = 0;
	for (row = 0; row <= week_view->rows; row++) {
		week_view->row_offsets[row] = floor (offset + 0.5);
		offset += height;
	}

	/* Calculate the cell heights based on the offsets. */
	for (row = 0; row < week_view->rows; row++) {
		week_view->row_heights[row] = week_view->row_offsets[row + 1]
			- week_view->row_offsets[row];
	}
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


static void
e_week_view_draw (GtkWidget    *widget,
		  GdkRectangle *area)
{
	EWeekView *week_view;

	week_view = E_WEEK_VIEW (widget);

	e_week_view_draw_shadow (week_view);

	if (GTK_WIDGET_CLASS (parent_class)->draw)
		(*GTK_WIDGET_CLASS (parent_class)->draw)(widget, area);
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


void
e_week_view_set_calendar	(EWeekView	*week_view,
				 GnomeCalendar	*calendar)
{
	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	week_view->calendar = calendar;

	/* FIXME: free current events? */
}


/* This sets the selected time range. The EWeekView will show the corresponding
   month and the days between start_time and end_time will be selected.
   To select a single day, use the same value for start_time & end_time. */
void
e_week_view_set_selected_time_range	(EWeekView	*week_view,
					 time_t		 start_time,
					 time_t		 end_time)
{
	GDate date, base_date, end_date;
	gint day_offset, num_days;
	gboolean update_adjustment_value = FALSE;

	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	g_date_clear (&date, 1);
	g_date_set_time (&date, start_time);

	if (week_view->display_month) {
		/* Find the number of days since the start of the month. */
		day_offset = g_date_day (&date) - 1;

		/* Find the 1st Monday at or before the start of the month. */
		base_date = date;
		g_date_set_day (&base_date, 1);
		day_offset += g_date_weekday (&base_date) - 1;
	} else {
		/* Find the 1st Monday at or before the given day. */
		day_offset = g_date_weekday (&date) - 1;
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
		start_time = time_add_day (start_time, -day_offset);
		start_time = time_day_begin (start_time);
		e_week_view_recalc_day_starts (week_view, start_time);
		e_week_view_reload_events (week_view);
	}

	/* Set the selection to the given days. */
	week_view->selection_start_day = g_date_julian (&date)
		- g_date_julian (&base_date);
	if (end_time == start_time
	    || end_time <= time_add_day (start_time, 1))
		week_view->selection_end_day = week_view->selection_start_day;
	else {
		g_date_clear (&end_date, 1);
		g_date_set_time (&end_date, end_time - 60);
		week_view->selection_end_day = g_date_julian (&end_date)
			- g_date_julian (&base_date);
	}

	/* Make sure the selection is valid. */
	num_days = week_view->display_month ? E_WEEK_VIEW_MAX_WEEKS * 7 : 7;
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


/* Returns the selected time range. */
void
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


/* Recalculates the time_t corresponding to the start of each day. */
static void
e_week_view_recalc_day_starts (EWeekView *week_view,
			       time_t lower)
{
	gint num_days, day;
	time_t tmp_time;

	num_days = week_view->display_month ? E_WEEK_VIEW_MAX_WEEKS * 7 : 7;

	tmp_time = lower;
	week_view->day_starts[0] = tmp_time;
	for (day = 1; day <= num_days; day++) {
		/* FIXME: There is a bug in time_add_day(). */
#if 0
		g_print ("Day:%i - %s\n", day, ctime (&tmp_time));
#endif
		tmp_time = time_add_day (tmp_time, 1);
		week_view->day_starts[day] = tmp_time;
	}
}


gboolean
e_week_view_get_display_month	(EWeekView	*week_view)
{
	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), FALSE);

	return week_view->display_month;
}


void
e_week_view_set_display_month	(EWeekView	*week_view,
				 gboolean	 display_month)
{
	GtkAdjustment *adjustment;
	gint page_increment, page_size;

	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	if (week_view->display_month == display_month)
		return;

	week_view->display_month = display_month;

	if (display_month) {
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

	/* FIXME: Need to change start date and adjustment value? */

	e_week_view_recalc_day_starts (week_view, week_view->day_starts[0]);
	e_week_view_recalc_cell_sizes (week_view);
	e_week_view_reload_events (week_view);
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
	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	if (week_view->compress_weekend == compress)
		return;

	week_view->compress_weekend = compress;

	/* The option only affects the month view. */
	if (!week_view->display_month)
		return;

	/* FIXME: Need to update layout. */
}


/* This reloads all calendar events. */
void
e_week_view_update_all_events	(EWeekView	*week_view)
{
	e_week_view_reload_events (week_view);
}


/* This is called when one event has been added or updated. */
void
e_week_view_update_event		(EWeekView	*week_view,
					 const gchar	*uid)
{
	gint event_num, num_days;
	CalComponent *comp;
	CalClientGetStatus status;

	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

#if 0
	g_print ("In e_week_view_update_event\n");
#endif

	/* If we don't have a calendar or valid date set yet, just return. */
	if (!week_view->calendar
	    || !g_date_valid (&week_view->first_day_shown))
		return;

	/* Get the event from the server. */
	status = cal_client_get_object (week_view->calendar->client, uid, &comp);

	switch (status) {
	case CAL_CLIENT_GET_SUCCESS:
		/* Do nothing. */
		break;
	case CAL_CLIENT_GET_SYNTAX_ERROR:
		g_warning ("syntax error uid=%s\n", uid);
		return;
	case CAL_CLIENT_GET_NOT_FOUND:
		g_warning ("obj not found uid=%s\n", uid);
		return;
	}

	/* We only care about events. */
	if (comp && cal_component_get_vtype (comp) != CAL_COMPONENT_EVENT) {
		gtk_object_unref (GTK_OBJECT (comp));
		return;
	}

	/* If the event already exists and the dates didn't change, we can
	   update the event fairly easily without changing the events arrays
	   or computing a new layout. */
	if (e_week_view_find_event_from_uid (week_view, uid, &event_num)) {
#ifndef NO_WARNINGS
#warning "FIX ME"
#endif
		/* Do this the long way every time for now */
#if 0
		event = &g_array_index (week_view->events, EWeekViewEvent,
					event_num);

		if (ical_object_compare_dates (event->ico, ico)) {
			e_week_view_foreach_event_with_uid (week_view, uid, e_week_view_update_event_cb, comp);
			gtk_object_unref (GTK_OBJECT (comp));
			gtk_widget_queue_draw (week_view->main_canvas);
			return;
		}
#endif
		/* The dates have changed, so we need to remove the
		   old occurrrences before adding the new ones. */
		e_week_view_foreach_event_with_uid (week_view, uid,
						    e_week_view_remove_event_cb,
						    NULL);
	}

	/* Add the occurrences of the event. */
	num_days = week_view->display_month ? E_WEEK_VIEW_MAX_WEEKS * 7 : 7;

	cal_recur_generate_instances (comp, 
				      week_view->day_starts[0],
				      week_view->day_starts[num_days],
				      e_week_view_add_event,
				      week_view);

	gtk_object_unref (GTK_OBJECT (comp));

	e_week_view_check_layout (week_view);

	gtk_widget_queue_draw (week_view->main_canvas);
}


#ifndef NO_WARNINGS
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
	event->comp = comp;
	gtk_object_ref (GTK_OBJECT (comp));

	/* If we are editing an event which we have just created, we will get
	   an update_event callback from the server. But we need to ignore it
	   or we will lose the text the user has already typed in. */
	if (week_view->editing_new_event
	    && week_view->editing_event_num == event_num) {
		return TRUE;
	}

	for (span_num = 0; span_num < event->num_spans; span_num++) {
		span = &g_array_index (week_view->spans, EWeekViewEventSpan,
				       event->spans_index + span_num);

		if (span->text_item) {
			CalComponentText t;
			
			cal_component_get_summary (event->comp, &t);
			text = g_strdup (t.value);
			gnome_canvas_item_set (span->text_item,
					       "text", text ? text : "",
					       NULL);
			g_free (text);
			
			e_week_view_reshape_event_span (week_view, event_num,
							span_num);
		}
	}

	return TRUE;
}
#endif


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


/* This removes all the events associated with the given uid. Note that for
   recurring events there may be more than one. If any events are found and
   removed we need to layout the events again. */
void
e_week_view_remove_event	(EWeekView	*week_view,
				 const gchar	*uid)
{
	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	e_week_view_foreach_event_with_uid (week_view, uid,
					    e_week_view_remove_event_cb, NULL);

	e_week_view_check_layout (week_view);
	gtk_widget_queue_draw (week_view->main_canvas);
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

	gtk_object_unref (GTK_OBJECT (event->comp));

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
	gint week, day_of_week, row;

	*day_x = *day_y = *day_w = *day_h = 0;
	g_return_if_fail (day >= 0);

	if (week_view->display_month) {
		g_return_if_fail (day < E_WEEK_VIEW_MAX_WEEKS * 7);

		week = day / 7;
		day_of_week = day % 7;
		if (week_view->compress_weekend && day_of_week >= 5) {
			/* In the compressed view Saturday is above Sunday and
			   both have just one row as opposed to 2 for all the
			   other days. */
			if (day_of_week == 5) {
				*day_y = week_view->row_offsets[week * 2];
				*day_h = week_view->row_heights[week * 2];
			} else {
				*day_y = week_view->row_offsets[week * 2 + 1];
				*day_h = week_view->row_heights[week * 2 + 1];
			}
			/* Both Saturday and Sunday are in the 6th column. */
			*day_x = week_view->col_offsets[5];
			*day_w = week_view->col_widths[5];
		} else {
			*day_y = week_view->row_offsets[week * 2];
			*day_h = week_view->row_heights[week * 2]
				+ week_view->row_heights[week * 2 + 1];
			*day_x = week_view->col_offsets[day_of_week];
			*day_w = week_view->col_widths[day_of_week];
		}
	} else {
		g_return_if_fail (day < 7);

		/* The week view has Mon, Tue & Wed down the left column and
		   Thu, Fri & Sat/Sun down the right. */
		if (day < 3) {
			*day_x = week_view->col_offsets[0];
			*day_w = week_view->col_widths[0];
		} else {
			*day_x = week_view->col_offsets[1];
			*day_w = week_view->col_widths[1];
		}

		if (day < 5) {
			row = (day % 3) * 2;
			*day_y = week_view->row_offsets[row];
			*day_h = week_view->row_heights[row]
				+ week_view->row_heights[row + 1];
		} else {
			/* Saturday & Sunday. */
			*day_y = week_view->row_offsets[day - 1];
			*day_h = week_view->row_heights[day - 1];
		}
	}
}


/* Returns the bounding box for a span of an event. Usually this can easily
   be determined by the start & end days and row of the span, which are set in
   e_week_view_layout_event(). Though we need a special case for the weekends
   when they are compressed, since the span may not fit. */
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
	gint end_day_of_week, num_days;
	gint start_x, start_y, start_w, start_h;
	gint end_x, end_y, end_w, end_h;

	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), FALSE);
	g_return_val_if_fail (event_num < week_view->events->len, FALSE);

	event = &g_array_index (week_view->events, EWeekViewEvent, event_num);

	g_return_val_if_fail (span_num < event->num_spans, FALSE);

	span = &g_array_index (week_view->spans, EWeekViewEventSpan,
			       event->spans_index + span_num);

	if (span->row >= week_view->rows_per_cell)
		return FALSE;

	end_day_of_week = (span->start_day + span->num_days - 1) % 7;
	num_days = span->num_days;
	/* Check if the row will not be visible in compressed cells. */
	if (span->row >= week_view->rows_per_compressed_cell) {
		if (week_view->display_month) {
			if (week_view->compress_weekend) {
				/* If it ends on a Saturday and is 1 day long
				   we skip it, else we shorten it. If it ends
				   on a Sunday it must be 1 day long and we
				   skip it. */
				if (end_day_of_week == 5) {	   /* Sat */
					if (num_days == 1) {
						return FALSE;
					} else {
						num_days--;
					}
				} else if (end_day_of_week == 6) { /* Sun */
					return FALSE;
				}
			}
		} else {
			/* All spans are 1 day long in the week view, so we
			   just skip it. */
			if (end_day_of_week > 4)
				return FALSE;
		}
	}

	e_week_view_get_day_position (week_view, span->start_day,
				      &start_x, &start_y, &start_w, &start_h);
	*span_y = start_y + week_view->events_y_offset
		+ span->row * (week_view->row_height
			       + E_WEEK_VIEW_EVENT_Y_SPACING);
	if (num_days == 1) {
		*span_x = start_x;
		*span_w = start_w;
	} else {
		e_week_view_get_day_position (week_view,
					      span->start_day + num_days - 1,
					      &end_x, &end_y, &end_w, &end_h);
		*span_x = start_x;
		*span_w = end_x - start_x + end_w;
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
#endif

	/* Handle scroll wheel events */
	if (event->button == 4 || event->button == 5) {
		GtkAdjustment *adj = GTK_RANGE (week_view->vscrollbar)->adjustment;
		gfloat new_value;

		new_value = adj->value + ((event->button == 4) ?
					  -adj->page_increment:
					  adj->page_increment);
		new_value = CLAMP (new_value, adj->lower, adj->upper - adj->page_size);
		gtk_adjustment_set_value (adj, new_value);

		return TRUE;
	}

	/* If an event is pressed just return. */
	if (week_view->pressed_event_num != -1)
		return FALSE;

	/* Convert the mouse position to a week & day. */
	x = event->x;
	y = event->y;
	day = e_week_view_convert_position_to_day (week_view, x, y);
	if (day == -1)
		return FALSE;

	/* Start the selection drag. */
	if (event->button == 1) {
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
		e_week_view_show_popup_menu (week_view, event, -1);
	}

	return FALSE;
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
		gnome_calendar_set_selected_time_range (week_view->calendar,
							start, end);
	}

	return FALSE;
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
	if (week_view->display_month) {
		week = grid_y / 2;
		if (week_view->compress_weekend && grid_x == 5
		    && grid_y % 2 == 1)
			day = 6;
		else
			day = grid_x;
	} else {
		week = 0;
		if (grid_x == 0)
			day = grid_y / 2;
		else if (grid_y == 5)
			day = 6;
		else
			day = grid_y / 2 + 3;
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
e_week_view_reload_events (EWeekView *week_view)
{
	gint num_days;

	e_week_view_free_events (week_view);

	if (week_view->calendar
	    && g_date_valid (&week_view->first_day_shown)) {
		num_days = week_view->display_month
			? E_WEEK_VIEW_MAX_WEEKS * 7 : 7;
		
		cal_client_generate_instances (week_view->calendar->client,
					       CALOBJ_TYPE_EVENT,
					       week_view->day_starts[0],
					       week_view->day_starts[num_days],
					       e_week_view_add_event,
					       week_view);
	}

	e_week_view_check_layout (week_view);

	gtk_widget_queue_draw (week_view->main_canvas);
}


static void
e_week_view_free_events (EWeekView *week_view)
{
	EWeekViewEvent *event;
	EWeekViewEventSpan *span;
	gint event_num, span_num;

	for (event_num = 0; event_num < week_view->events->len; event_num++) {
		event = &g_array_index (week_view->events, EWeekViewEvent,
					event_num);
		gtk_object_unref (GTK_OBJECT (event->comp));
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
	struct tm start_tm, end_tm;

	week_view = E_WEEK_VIEW (data);

	/* Check that the event times are valid. */
	num_days = week_view->display_month ? E_WEEK_VIEW_MAX_WEEKS * 7 : 7;

#if 0
	g_print ("View start:%li end:%li  Event start:%li end:%li\n",
		 week_view->day_starts[0], week_view->day_starts[num_days],
		 start, end);
#endif

	g_return_val_if_fail (start <= end, TRUE);
	g_return_val_if_fail (start < week_view->day_starts[num_days], TRUE);
	g_return_val_if_fail (end > week_view->day_starts[0], TRUE);

	start_tm = *(localtime (&start));
	end_tm = *(localtime (&end));

	event.comp = comp;
	gtk_object_ref (GTK_OBJECT (event.comp));
	event.start = start;
	event.end = end;
	event.spans_index = 0;
	event.num_spans = 0;

	event.start_minute = start_tm.tm_hour * 60 + start_tm.tm_min;
	event.end_minute = end_tm.tm_hour * 60 + end_tm.tm_min;
	if (event.end_minute == 0 && start != end)
		event.end_minute = 24 * 60;

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
		e_week_view_layout_events (week_view);

	if (week_view->events_need_layout || week_view->events_need_reshape)
		e_week_view_reshape_events (week_view);

	week_view->events_need_layout = FALSE;
	week_view->events_need_reshape = FALSE;
}


static void
e_week_view_layout_events (EWeekView *week_view)
{
	EWeekViewEvent *event;
	EWeekViewEventSpan *span;
	gint num_days, day, event_num, span_num;
	guint8 *grid;
	GArray *spans, *old_spans;

	/* This is a temporary 2-d grid which is used to place events.
	   Each element is 0 if the position is empty, or 1 if occupied.
	   We allocate the maximum size possible here, assuming that each
	   event will need its own row. */
	grid = g_new0 (guint8, E_WEEK_VIEW_MAX_ROWS_PER_CELL * 7
		       * E_WEEK_VIEW_MAX_WEEKS);

	/* We create a new array of spans, which will replace the old one. */
	spans = g_array_new (FALSE, FALSE, sizeof (EWeekViewEventSpan));

	/* Clear the number of rows used per day. */
	num_days = week_view->display_month ? E_WEEK_VIEW_MAX_WEEKS * 7 : 7;
	for (day = 0; day <= num_days; day++) {
		week_view->rows_per_day[day] = 0;
	}

	/* Iterate over the events, finding which weeks they cover, and putting
	   them in the first free row available. */
	for (event_num = 0; event_num < week_view->events->len; event_num++) {
		event = &g_array_index (week_view->events, EWeekViewEvent,
					event_num);
		e_week_view_layout_event (week_view, event, grid, spans);
	}

	/* Free the grid. */
	g_free (grid);

	/* Replace the spans array. */
	old_spans = week_view->spans;
	week_view->spans = spans;

	/* Destroy the old spans array, destroying any unused canvas items. */
	if (old_spans) {
		for (span_num = 0; span_num < old_spans->len; span_num++) {
			span = &g_array_index (old_spans, EWeekViewEventSpan,
					       span_num);
			if (span->background_item)
				gtk_object_destroy (GTK_OBJECT (span->background_item));
			if (span->text_item)
				gtk_object_destroy (GTK_OBJECT (span->text_item));
		}
		g_array_free (old_spans, TRUE);
	}
}


static void
e_week_view_layout_event (EWeekView	   *week_view,
			  EWeekViewEvent   *event,
			  guint8	   *grid,
			  GArray	   *spans)
{
	gint start_day, end_day, span_start_day, span_end_day, rows_per_cell;
	gint free_row, row, day, span_num, spans_index, num_spans, max_day;
	EWeekViewEventSpan span, *old_span;

	start_day = e_week_view_find_day (week_view, event->start, FALSE);
	end_day = e_week_view_find_day (week_view, event->end, TRUE);
	max_day = week_view->display_month ? E_WEEK_VIEW_MAX_WEEKS * 7 - 1
		: 7 - 1;
	start_day = CLAMP (start_day, 0, max_day);
	end_day = CLAMP (end_day, 0, max_day);

#if 0
	g_print ("In e_week_view_layout_event Start:%i End: %i\n",
		 start_day, end_day);
#endif

	/* Iterate through each of the spans of the event, where each span
	   is a sequence of 1 or more days displayed next to each other. */
	span_start_day = start_day;
	rows_per_cell = E_WEEK_VIEW_MAX_ROWS_PER_CELL;
	span_num = 0;
	spans_index = spans->len;
	num_spans = 0;
	while (span_start_day <= end_day) {
		span_end_day = e_week_view_find_span_end (week_view,
							  span_start_day);
		span_end_day = MIN (span_end_day, end_day);
#if 0
		g_print ("  Span start:%i end:%i\n", span_start_day,
			 span_end_day);
#endif
		/* Try each row until we find a free one or we fall off the
		   bottom of the available rows. */
		row = 0;
		free_row = -1;
		while (free_row == -1 && row < rows_per_cell) {
			free_row = row;
			for (day = span_start_day; day <= span_end_day;
			     day++) {
				if (grid[day * rows_per_cell + row]) {
					free_row = -1;
					break;
				}
			}
			row++;
		};

		if (free_row != -1) {
			/* Mark the cells as full. */
			for (day = span_start_day; day <= span_end_day;
			     day++) {
				grid[day * rows_per_cell + free_row] = 1;
				week_view->rows_per_day[day] = MAX (week_view->rows_per_day[day], free_row + 1);
			}
#if 0
			g_print ("  Span start:%i end:%i row:%i\n",
				 span_start_day, span_end_day, free_row);
#endif
			/* Add the span to the array, and try to reuse any
			   canvas items from the old spans. */
			span.start_day = span_start_day;
			span.num_days = span_end_day - span_start_day + 1;
			span.row = free_row;
			span.background_item = NULL;
			span.text_item = NULL;
			if (event->num_spans > span_num) {
				old_span = &g_array_index (week_view->spans, EWeekViewEventSpan, event->spans_index + span_num);
				span.background_item = old_span->background_item;
				span.text_item = old_span->text_item;
				old_span->background_item = NULL;
				old_span->text_item = NULL;
			}

			g_array_append_val (spans, span);
			num_spans++;
		}

		span_start_day = span_end_day + 1;
		span_num++;
	}

	/* Set the event's spans. */
	event->spans_index = spans_index;
	event->num_spans = num_spans;
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


static gint
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
	num_days = week_view->display_month ? E_WEEK_VIEW_MAX_WEEKS * 7 : 7;
	for (day = 0; day < num_days; day++) {

		is_weekend = (day % 7 >= 5) ? TRUE : FALSE;
		if (!is_weekend || (week_view->display_month
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
	GdkFont *font;
	gint span_x, span_y, span_w, num_icons, icons_width, time_width;
	gint min_text_x, max_text_w, width;
	gboolean show_icons = TRUE, use_max_width = FALSE;
	gboolean one_day_event;
	CalComponent *comp;
	gdouble text_x, text_y, text_w, text_h;
	gchar *text, *end_of_line;
	gint line_len, text_width;

	event = &g_array_index (week_view->events, EWeekViewEvent, event_num);
	span = &g_array_index (week_view->spans, EWeekViewEventSpan,
			       event->spans_index + span_num);
	comp = event->comp;
	font = GTK_WIDGET (week_view)->style->font;

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

	if (!one_day_event && week_view->editing_event_num == event_num
	    && week_view->editing_span_num == span_num) {
		show_icons = FALSE;
		use_max_width = TRUE;
	}

	num_icons = 0;
#if 0
	if (show_icons) {
		if (ico->dalarm.enabled || ico->malarm.enabled
		    || ico->palarm.enabled || ico->aalarm.enabled)
			num_icons++;
		if (ico->recur)
			num_icons++;
	}
#endif

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
					       "font_gdk", GTK_WIDGET (week_view)->style->font,
					       "anchor", GTK_ANCHOR_NW,
					       "clip", TRUE,
#if 0
					       "max_lines", 1,
#endif
					       "editable", TRUE,
					       "text", text.value ? text.value : "",
					       "use_ellipsis", TRUE,
					       NULL);
		gtk_signal_connect (GTK_OBJECT (span->text_item), "event",
				    GTK_SIGNAL_FUNC (e_week_view_on_text_item_event),
				    week_view);
	}

	/* Calculate the position of the text item.
	   For events < 1 day it starts after the times & icons and ends at the
	   right edge of the span.
	   For events > 1 day we need to determine whether times are shown at
	   the start and end of the span, then try to center the text item with
	   the icons in the middle, but making sure we don't go over the times.
	*/


	/* Calculate the space necessary to display a time, e.g. "13:00". */
	if (week_view->use_small_font && week_view->small_font)
		time_width = week_view->digit_width * 2
			+ week_view->small_digit_width * 2;
	else
		time_width = week_view->digit_width * 4
			+ week_view->colon_width;

	/* Calculate the space needed for the icons. */
	icons_width = (E_WEEK_VIEW_ICON_WIDTH + E_WEEK_VIEW_ICON_X_PAD)
		* num_icons;

	/* The y position and height are the same for both event types. */
	text_y = span_y + E_WEEK_VIEW_EVENT_BORDER_HEIGHT
		+ E_WEEK_VIEW_EVENT_TEXT_Y_PAD;
	text_h = font->ascent + font->descent;

	if (one_day_event) {
		text_x = span_x + E_WEEK_VIEW_EVENT_L_PAD + icons_width;

		switch (week_view->time_format) {
		case E_WEEK_VIEW_TIME_BOTH_SMALL_MIN:
		case E_WEEK_VIEW_TIME_BOTH:
			text_x += time_width * 2 + week_view->space_width
				+ E_WEEK_VIEW_EVENT_TIME_R_PAD;
			break;
		case E_WEEK_VIEW_TIME_START_SMALL_MIN:
		case E_WEEK_VIEW_TIME_START:
			text_x += time_width + E_WEEK_VIEW_EVENT_TIME_R_PAD;
			break;
		case E_WEEK_VIEW_TIME_NONE:
			break;
		}
		text_w = span_x + span_w - E_WEEK_VIEW_EVENT_BORDER_WIDTH
			- E_WEEK_VIEW_EVENT_R_PAD - text_x;

	} else {
		if (use_max_width) {
			text_x = span_x + E_WEEK_VIEW_EVENT_L_PAD
				+ E_WEEK_VIEW_EVENT_BORDER_WIDTH
				+ E_WEEK_VIEW_EVENT_TEXT_X_PAD;
			text_w = span_x + span_w - E_WEEK_VIEW_EVENT_R_PAD
				- E_WEEK_VIEW_EVENT_BORDER_WIDTH
				- E_WEEK_VIEW_EVENT_TEXT_X_PAD - text_x;
		} else {
			/* Get the requested size of the label. */
			gtk_object_get (GTK_OBJECT (span->text_item),
					"text", &text,
					NULL);
			text_width = 0;
			if (text) {
				end_of_line = strchr (text, '\n');
				if (end_of_line)
					line_len = end_of_line - text;
				else
					line_len = strlen (text);
				text_width = gdk_text_width (font, text, line_len);
				g_free (text);
			}

			/* Add on the width of the icons and find the default
			   position. */
			width = text_width + icons_width;
			text_x = span_x + (span_w - width) / 2;

			/* Now calculate the left-most valid position, and make
			   sure we don't go to the left of that. */
			min_text_x = span_x + E_WEEK_VIEW_EVENT_L_PAD
				+ E_WEEK_VIEW_EVENT_BORDER_WIDTH
				+ E_WEEK_VIEW_EVENT_TEXT_X_PAD;
			if (event->start > week_view->day_starts[span->start_day])
				min_text_x += time_width
					+ E_WEEK_VIEW_EVENT_TIME_R_PAD;

			text_x = MAX (text_x, min_text_x);

			/* Now calculate the largest valid width, using the
			   calculated x position, and make sure we don't
			   exceed that. */
			max_text_w = span_x + span_w - E_WEEK_VIEW_EVENT_R_PAD
				- E_WEEK_VIEW_EVENT_BORDER_WIDTH
				- E_WEEK_VIEW_EVENT_TEXT_X_PAD - text_x;
			if (event->end < week_view->day_starts[span->start_day
							      + span->num_days])
				max_text_w -= time_width
					+ E_WEEK_VIEW_EVENT_TIME_R_PAD;

			text_w = MIN (width, max_text_w);

			/* Now take out the space for the icons. */
			text_x += icons_width;
			text_w -= icons_width;
		}
	}

	text_w = MAX (text_w, 0);
	gnome_canvas_item_set (span->text_item,
			       "clip_width", (gdouble) text_w,
			       "clip_height", (gdouble) text_h,
			       NULL);
	e_canvas_item_move_absolute(span->text_item,
				    text_x, text_y);
}


/* Finds the day containing the given time.
   If include_midnight_in_prev_day is TRUE then if the time exactly
   matches the start of a day the previous day is returned. This is useful
   when calculating the end day of an event. */
static gint
e_week_view_find_day (EWeekView *week_view,
		      time_t time_to_find,
		      gboolean include_midnight_in_prev_day)
{
	gint num_days, day;
	time_t *day_starts;

	num_days = week_view->display_month ? E_WEEK_VIEW_MAX_WEEKS * 7 : 7;
	day_starts = week_view->day_starts;

	if (time_to_find < day_starts[0])
		return -1;
	if (time_to_find > day_starts[num_days])
		return num_days;

	for (day = 1; day <= num_days; day++) {
		if (time_to_find <= day_starts[day]) {
			if (time_to_find == day_starts[day]
			    && !include_midnight_in_prev_day)
				return day;
			return day - 1;
		}
	}

	g_assert_not_reached ();
	return num_days;
}


/* This returns the last day in the same span as the given day. A span is all
   the days which are displayed next to each other from left to right. 
   In the week view all spans are only 1 day, since Tuesday is below Monday
   rather than beside it etc. In the month view, if the weekends are not
   compressed then each week is a span, otherwise Monday to Saturday of each
   week is a span, and the Sundays are separate spans. */
static gint
e_week_view_find_span_end (EWeekView *week_view,
			   gint day)
{
	gint week, day_of_week, end_day;

	if (week_view->display_month) {
		week = day / 7;
		day_of_week = day % 7;
		if (week_view->compress_weekend && day_of_week <= 5)
			end_day = 5;
		else
			end_day = 6;
		return week * 7 + end_day;
	} else {
		return day;
	}
}


static void
e_week_view_on_adjustment_changed (GtkAdjustment *adjustment,
				   EWeekView *week_view)
{
	GDate date;
	gint week_offset;
	struct tm tm;
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
	g_date_to_struct_tm (&date, &tm);
	lower = mktime (&tm);
	lower = time_day_begin (lower);

	e_week_view_recalc_day_starts (week_view, lower);
	e_week_view_reload_events (week_view);

	/* Update the selection, if needed. */
	if (week_view->selection_start_day != -1) {
		start = week_view->day_starts[week_view->selection_start_day];
		end = week_view->day_starts[week_view->selection_end_day + 1];
		gnome_calendar_set_selected_time_range (week_view->calendar,
							start, end);
	}

	gtk_widget_queue_draw (week_view->main_canvas);
}


void
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
		return;

	event = &g_array_index (week_view->events, EWeekViewEvent, event_num);
	span = &g_array_index (week_view->spans, EWeekViewEventSpan,
			       event->spans_index + span_num);

	/* If the event is not shown, don't try to edit it. */
	if (!span->text_item)
		return;

	if (initial_text) {
		gnome_canvas_item_set (span->text_item,
				       "text", initial_text,
				       NULL);
	}

	e_canvas_item_grab_focus (span->text_item);

	/* Try to move the cursor to the end of the text. */
	gtk_object_get (GTK_OBJECT (span->text_item),
			"event_processor", &event_processor,
			NULL);
	if (event_processor) {
		command.action = E_TEP_MOVE;
		command.position = E_TEP_END_OF_BUFFER;
		gtk_signal_emit_by_name (GTK_OBJECT (event_processor),
					 "command", &command);
	}
}


/* This stops the current edit. If accept is TRUE the event summary is update,
   else the edit is cancelled. */
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


static gboolean
e_week_view_on_text_item_event (GnomeCanvasItem *item,
				GdkEvent *event,
				EWeekView *week_view)
{
	gint event_num, span_num;

#if 0
	g_print ("In e_week_view_on_text_item_event\n");
#endif

	switch (event->type) {
	case GDK_KEY_PRESS:
		if (event && event->key.keyval == GDK_Return) {
			/* We set the keyboard focus to the EDayView, so the
			   EText item loses it and stops the edit. */
			gtk_widget_grab_focus (GTK_WIDGET (week_view));

			/* Stop the signal last or we will also stop any
			   other events getting to the EText item. */
			gtk_signal_emit_stop_by_name (GTK_OBJECT (item),
						      "event");
			return TRUE;
		}
		break;
	case GDK_BUTTON_PRESS:
		if (!e_week_view_find_event_from_item (week_view, item,
						       &event_num, &span_num))
			return FALSE;

		if (event->button.button == 3) {
			if (!GTK_WIDGET_HAS_FOCUS (week_view))
				gtk_widget_grab_focus (GTK_WIDGET (week_view));
			e_week_view_show_popup_menu (week_view,
						     (GdkEventButton*) event,
						     event_num);
			gtk_signal_emit_stop_by_name (GTK_OBJECT (item->canvas),
						      "button_press_event");
			return TRUE;
		}

		week_view->pressed_event_num = event_num;
		week_view->pressed_span_num = span_num;

		/* Only let the EText handle the event while editing. */
		if (!E_TEXT (item)->editing) {
			gtk_signal_emit_stop_by_name (GTK_OBJECT (item),
						      "event");

			if (event) {
				week_view->drag_event_x = event->button.x;
				week_view->drag_event_y = event->button.y;
			} else
				g_warning ("No GdkEvent");

			/* FIXME: Remember the day offset from the start of
			   the event. */

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
		if (event->focus_change.in) {
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
	week_view->editing_new_event = FALSE;

	/* Check that the event is still valid. */
	cal_component_get_uid (event->comp, &uid);
	if (!uid)
		return;

	gtk_object_get (GTK_OBJECT (span->text_item),
			"text", &text,
			NULL);

	/* Only update the summary if necessary. */
	cal_component_get_summary (event->comp, &summary);
	if (text && summary.value && !strcmp (text, summary.value)) {
		g_free (text);
		if (!e_week_view_is_one_day_event (week_view, event_num))
			e_week_view_reshape_event_span (week_view, event_num,
							span_num);
		return;
	}

	summary.value = text;
	cal_component_set_summary (event->comp, &summary);
	g_free (text);
	
	if (!cal_client_update_object (week_view->calendar->client, event->comp))
		g_message ("e_week_view_on_editing_stopped(): Could not update the object!");
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


static gint
e_week_view_key_press (GtkWidget *widget, GdkEventKey *event)
{
	EWeekView *week_view;
	CalComponent *comp;
	gint event_num;
	gchar *initial_text;
	CalComponentDateTime date;
	time_t dtstart, dtend;
	const char *uid;
	
	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (E_IS_WEEK_VIEW (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	week_view = E_WEEK_VIEW (widget);

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
	} else if ((event->keyval < 0x20)
		   || (event->keyval > 0xFF)
		   || (event->length == 0)
		   || (event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK))) {
		return FALSE;
	} else {
		initial_text = event->string;
	}

	/* Add a new event covering the selected range. */
	comp = cal_component_new ();
	cal_component_set_new_vtype (comp, CAL_COMPONENT_EVENT);
	dtstart = week_view->day_starts[week_view->selection_start_day];
	dtend = week_view->day_starts[week_view->selection_end_day + 1];
	
	date.value = g_new0 (struct icaltimetype, 1);
	*date.value = icaltime_from_timet (dtstart, FALSE, TRUE);
	cal_component_set_dtstart (comp, &date);
	*date.value = icaltime_from_timet (dtend, FALSE, TRUE);
	cal_component_set_dtend (comp, &date);
	g_free (date.value);

	/* We add the event locally and start editing it. When we get the
	   "update_event" callback from the server, we basically ignore it.
	   If we were to wait for the "update_event" callback it wouldn't be
	   as responsive and we may lose a few keystrokes. */ 
	e_week_view_add_event (comp, dtstart, dtend, week_view);
	e_week_view_check_layout (week_view);
	gtk_widget_queue_draw (week_view->main_canvas);

	cal_component_get_uid (comp, &uid);
	if (e_week_view_find_event_from_uid (week_view, uid, &event_num)) {
		e_week_view_start_editing_event (week_view, event_num, 0,
						 initial_text);
		week_view->editing_new_event = TRUE;
	} else {
		g_warning ("Couldn't find event to start editing.\n");
	}

	if (!cal_client_update_object (week_view->calendar->client, comp))
		g_message ("e_week_view_key_press(): Could not update the object!");

	gtk_object_unref (GTK_OBJECT (comp));

	return TRUE;
}


void
e_week_view_show_popup_menu (EWeekView	     *week_view,
			     GdkEventButton  *bevent,
			     gint	      event_num)
{
	EWeekViewEvent *event;
	int have_selection, not_being_edited, num_items, i;
	struct menu_item *context_menu;

	static struct menu_item items[] = {
		{ N_("New appointment..."), (GtkSignalFunc) e_week_view_on_new_appointment, NULL, TRUE }
	};

	static struct menu_item child_items[] = {
		{ N_("Edit this appointment..."), (GtkSignalFunc) e_week_view_on_edit_appointment, NULL, TRUE },
		{ N_("Delete this appointment"), (GtkSignalFunc) e_week_view_on_delete_appointment, NULL, TRUE },
		{ NULL, NULL, NULL, TRUE },
		{ N_("New appointment..."), (GtkSignalFunc) e_week_view_on_new_appointment, NULL, TRUE }
	};

	static struct menu_item recur_child_items[] = {
		{ N_("Edit this appointment..."), (GtkSignalFunc) e_week_view_on_edit_appointment, NULL, TRUE },
		{ N_("Make this appointment movable"), (GtkSignalFunc) e_week_view_on_unrecur_appointment, NULL, TRUE },
		{ N_("Delete this occurrence"), (GtkSignalFunc) e_week_view_on_delete_occurrence, NULL, TRUE },
		{ N_("Delete all occurrences"), (GtkSignalFunc) e_week_view_on_delete_appointment, NULL, TRUE },
		{ NULL, NULL, NULL, TRUE },
		{ N_("New appointment..."), (GtkSignalFunc) e_week_view_on_new_appointment, NULL, TRUE }
	};

	have_selection = GTK_WIDGET_HAS_FOCUS (week_view)
		&& week_view->selection_start_day != -1;

	if (event_num == -1) {
		num_items = 1;
		context_menu = &items[0];
		context_menu[0].sensitive = have_selection;
	} else {
		event = &g_array_index (week_view->events,
					EWeekViewEvent, event_num);

		/* This used to be set only if the event wasn't being edited
		   in the event editor, but we can't check that at present.
		   We could possibly set up another method of checking it. */
		not_being_edited = TRUE;

		if (cal_component_has_rrules (event->comp) 
		    || cal_component_has_rdates (event->comp)) {
			num_items = 6;
			context_menu = &recur_child_items[0];
			context_menu[0].sensitive = not_being_edited;
			context_menu[1].sensitive = not_being_edited;
			context_menu[2].sensitive = not_being_edited;
			context_menu[3].sensitive = not_being_edited;
			context_menu[5].sensitive = have_selection;
		} else {
			num_items = 4;
			context_menu = &child_items[0];
			context_menu[0].sensitive = not_being_edited;
			context_menu[1].sensitive = not_being_edited;
			context_menu[3].sensitive = have_selection;
		}
	}

	for (i = 0; i < num_items; i++)
		context_menu[i].data = week_view;

	week_view->popup_event_num = event_num;
	popup_menu (context_menu, num_items, bevent);
}


static void
e_week_view_on_new_appointment (GtkWidget *widget, gpointer data)
{
	EWeekView *week_view;
	CalComponent *comp;
	CalComponentDateTime date;
	struct icaltimetype itt;
	time_t dt;
	
	week_view = E_WEEK_VIEW (data);

	comp = cal_component_new ();
	cal_component_set_new_vtype (comp, CAL_COMPONENT_EVENT);

	date.value = &itt;
	date.tzid = NULL;

	dt = week_view->day_starts[week_view->selection_start_day];
	*date.value = icaltime_from_timet (dt, FALSE, FALSE);
	cal_component_set_dtstart (comp, &date);

	dt = week_view->day_starts[week_view->selection_end_day + 1];
	*date.value = icaltime_from_timet (dt, FALSE, FALSE);
	cal_component_set_dtend (comp, &date);

	gnome_calendar_edit_object (week_view->calendar, comp);
	gtk_object_unref (GTK_OBJECT (comp));
}


static void
e_week_view_on_edit_appointment (GtkWidget *widget, gpointer data)
{
	EWeekView *week_view;
	EWeekViewEvent *event;

	week_view = E_WEEK_VIEW (data);

	if (week_view->popup_event_num == -1)
		return;

	event = &g_array_index (week_view->events, EWeekViewEvent,
				week_view->popup_event_num);

	gnome_calendar_edit_object (week_view->calendar, event->comp);
}


static void
e_week_view_on_delete_occurrence (GtkWidget *widget, gpointer data)
{
	EWeekView *week_view;
	EWeekViewEvent *event;
	CalComponent *comp;
	CalComponentDateTime *date=NULL;
	GSList *list;
	
	week_view = E_WEEK_VIEW (data);

	if (week_view->popup_event_num == -1)
		return;

	event = &g_array_index (week_view->events, EWeekViewEvent,
				week_view->popup_event_num);

	/* We must duplicate the CalComponent, or we won't know it has changed
	   when we get the "update_event" callback. */
	
	comp = cal_component_clone (event->comp);
	cal_component_get_exdate_list (comp, &list);
	list = g_slist_append (list, date);
	date = g_new0 (CalComponentDateTime, 1);
	date->value = g_new (struct icaltimetype, 1);
	*date->value = icaltime_from_timet (event->start, TRUE, TRUE);
	cal_component_set_exdate_list (comp, list);
	cal_component_free_exdate_list (list);

	if (!cal_client_update_object (week_view->calendar->client, comp))
		g_message ("e_week_view_on_delete_occurrence(): Could not update the object!");

	gtk_object_unref (GTK_OBJECT (comp));
}


static void
e_week_view_on_delete_appointment (GtkWidget *widget, gpointer data)
{
	EWeekView *week_view;
	EWeekViewEvent *event;
	const char *uid;
	
	week_view = E_WEEK_VIEW (data);

	if (week_view->popup_event_num == -1)
		return;

	event = &g_array_index (week_view->events, EWeekViewEvent,
				week_view->popup_event_num);
	
	cal_component_get_uid (event->comp, &uid);
	if (!cal_client_remove_object (week_view->calendar->client, uid))
		g_message ("e_week_view_on_delete_appointment(): Could not remove the object!");
}


static void
e_week_view_on_unrecur_appointment (GtkWidget *widget, gpointer data)
{
	EWeekView *week_view;
	EWeekViewEvent *event;
	CalComponent *comp, *new_comp;
	CalComponentDateTime *date;
	GSList *list;

	week_view = E_WEEK_VIEW (data);
	
	if (week_view->popup_event_num == -1)
		return;

	event = &g_array_index (week_view->events, EWeekViewEvent,
				week_view->popup_event_num);
	
	/* For the recurring object, we add a exception to get rid of the
	   instance. */

	comp = cal_component_clone (event->comp);
	cal_component_get_exdate_list (comp, &list);
	date = g_new0 (CalComponentDateTime, 1);
	date->value = g_new (struct icaltimetype, 1);
	*date->value = icaltime_from_timet (event->start, TRUE, TRUE);
	list = g_slist_append (list, date);
	cal_component_set_exdate_list (comp, list);
	cal_component_free_exdate_list (list);

	/* For the unrecurred instance we duplicate the original object,
	   create a new uid for it, get rid of the recurrence rules, and set
	   the start & end times to the instances times. */
	new_comp = cal_component_clone (event->comp);
	cal_component_set_uid (new_comp, cal_component_gen_uid ());
	cal_component_set_exdate_list (new_comp, NULL);
	cal_component_set_exrule_list (new_comp, NULL);

	date = g_new0 (CalComponentDateTime, 1);
	date->value = g_new (struct icaltimetype, 1);

	*date->value = icaltime_from_timet (event->start, TRUE, TRUE);
	cal_component_set_dtstart (new_comp, date);
	*date->value = icaltime_from_timet (event->end, TRUE, TRUE);
	cal_component_set_dtend (new_comp, date);

	cal_component_free_datetime (date);
	
	/* Now update both CalComponents. Note that we do this last since at
	   present the updates happen synchronously so our event may disappear.
	*/
	if (!cal_client_update_object (week_view->calendar->client, comp))
		g_message ("e_week_view_on_unrecur_appointment(): Could not update the object!");

	gtk_object_unref (GTK_OBJECT (comp));

	if (!cal_client_update_object (week_view->calendar->client, new_comp))
		g_message ("e_week_view_on_unrecur_appointment(): Could not update the object!");

	gtk_object_unref (GTK_OBJECT (new_comp));
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
				gnome_calendar_dayjump (week_view->calendar,
							week_view->day_starts[day]);
				/* A quick hack to make the 'Day' toolbar
				   button active. */
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (week_view->calendar->view_toolbar_buttons[0]), TRUE);
				return TRUE;
			}
		}

	}

	return FALSE;
}
