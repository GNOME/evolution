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
 * EDayView - displays the Day & Work-Week views of the calendar.
 */

#include <config.h>
#include <math.h>
#include <time.h>
#include <gnome.h>
#include <gdk/gdkx.h>
#include <cal-util/timeutil.h>
#include "e-day-view.h"
#include "e-day-view-time-item.h"
#include "e-day-view-top-item.h"
#include "e-day-view-main-item.h"
#include "calendar-commands.h"
#include "popup-menu.h"
#include "../e-util/e-canvas.h"
#include "../widgets/e-text/e-text.h"
#include "e-util/e-canvas-utils.h"

/* Images */
#include "bell.xpm"
#include "recur.xpm"

/* The minimum amount of space wanted on each side of the date string. */
#define E_DAY_VIEW_DATE_X_PAD	4

#define E_DAY_VIEW_LARGE_FONT	\
	"-adobe-utopia-regular-r-normal-*-*-240-*-*-p-*-iso8859-*"
#define E_DAY_VIEW_LARGE_FONT_FALLBACK	\
	"-adobe-helvetica-bold-r-normal-*-*-240-*-*-p-*-iso8859-*"

/* The offset from the top/bottom of the canvas before auto-scrolling starts.*/
#define E_DAY_VIEW_AUTO_SCROLL_OFFSET	16

/* The time between each auto-scroll, in milliseconds. */
#define E_DAY_VIEW_AUTO_SCROLL_TIMEOUT	50

/* The number of timeouts we skip before we start scrolling. */
#define E_DAY_VIEW_AUTO_SCROLL_DELAY	5

/* The number of pixels the mouse has to be moved with the button down before
   we start a drag. */
#define E_DAY_VIEW_DRAG_START_OFFSET	4

/* Drag and Drop stuff. */
enum {
	TARGET_CALENDAR_EVENT
};
static GtkTargetEntry target_table[] = {
	{ "application/x-e-calendar-event",     0, TARGET_CALENDAR_EVENT }
};
static guint n_targets = sizeof(target_table) / sizeof(target_table[0]);

static void e_day_view_class_init (EDayViewClass *class);
static void e_day_view_init (EDayView *day_view);
static void e_day_view_destroy (GtkObject *object);
static void e_day_view_realize (GtkWidget *widget);
static void e_day_view_unrealize (GtkWidget *widget);
static void e_day_view_style_set (GtkWidget *widget,
				  GtkStyle  *previous_style);
static void e_day_view_size_allocate (GtkWidget *widget,
				      GtkAllocation *allocation);
static gboolean e_day_view_update_scroll_regions (EDayView *day_view);
static gint e_day_view_focus_in (GtkWidget *widget,
				 GdkEventFocus *event);
static gint e_day_view_focus_out (GtkWidget *widget,
				  GdkEventFocus *event);
static gint e_day_view_key_press (GtkWidget *widget,
				  GdkEventKey *event);
static void e_day_view_cursor_key_up_shifted (EDayView *day_view,
					      GdkEventKey *event);
static void e_day_view_cursor_key_down_shifted (EDayView *day_view,
						GdkEventKey *event);
static void e_day_view_cursor_key_left_shifted (EDayView *day_view,
						GdkEventKey *event);
static void e_day_view_cursor_key_right_shifted (EDayView *day_view,
						 GdkEventKey *event);
static void e_day_view_cursor_key_up (EDayView *day_view,
				      GdkEventKey *event);
static void e_day_view_cursor_key_down (EDayView *day_view,
					GdkEventKey *event);
static void e_day_view_cursor_key_left (EDayView *day_view,
					GdkEventKey *event);
static void e_day_view_cursor_key_right (EDayView *day_view,
					 GdkEventKey *event);
static void e_day_view_ensure_rows_visible (EDayView *day_view,
					    gint start_row,
					    gint end_row);

static gboolean e_day_view_check_if_new_event_fits (EDayView *day_view);

static void e_day_view_on_canvas_realized (GtkWidget *widget,
					   EDayView *day_view);

static gboolean e_day_view_on_top_canvas_button_press (GtkWidget *widget,
						       GdkEventButton *event,
						       EDayView *day_view);
static gboolean e_day_view_on_top_canvas_button_release (GtkWidget *widget,
							 GdkEventButton *event,
							 EDayView *day_view);
static gboolean e_day_view_on_top_canvas_motion (GtkWidget *widget,
						 GdkEventMotion *event,
						 EDayView *day_view);

static gboolean e_day_view_on_main_canvas_button_press (GtkWidget *widget,
							GdkEventButton *event,
							EDayView *day_view);
static gboolean e_day_view_on_main_canvas_button_release (GtkWidget *widget,
							  GdkEventButton *event,
							  EDayView *day_view);

static gboolean e_day_view_on_time_canvas_button_press (GtkWidget *widget,
							GdkEventButton *event,
							EDayView *day_view);

static void e_day_view_update_calendar_selection_time (EDayView *day_view);
static gboolean e_day_view_on_main_canvas_motion (GtkWidget *widget,
						  GdkEventMotion *event,
						  EDayView *day_view);
static gboolean e_day_view_convert_event_coords (EDayView *day_view,
						 GdkEvent *event,
						 GdkWindow *window,
						 gint *x_return,
						 gint *y_return);
static void e_day_view_update_long_event_resize (EDayView *day_view,
						 gint day);
static void e_day_view_update_resize (EDayView *day_view,
				      gint row);
static void e_day_view_finish_long_event_resize (EDayView *day_view);
static void e_day_view_finish_resize (EDayView *day_view);
static void e_day_view_abort_resize (EDayView *day_view,
				     guint32 time);


static gboolean e_day_view_on_long_event_button_press (EDayView		*day_view,
						       gint		 event_num,
						       GdkEventButton	*event,
						       EDayViewPosition  pos,
						       gint		 event_x,
						       gint		 event_y);
static gboolean e_day_view_on_event_button_press (EDayView	 *day_view,
						  gint		  day,
						  gint		  event_num,
						  GdkEventButton *event,
						  EDayViewPosition pos,
						  gint		  event_x,
						  gint		  event_y);
static void e_day_view_on_long_event_click (EDayView *day_view,
					    gint event_num,
					    GdkEventButton  *bevent,
					    EDayViewPosition pos,
					    gint	     event_x,
					    gint	     event_y);
static void e_day_view_on_event_click (EDayView *day_view,
				       gint day,
				       gint event_num,
				       GdkEventButton  *event,
				       EDayViewPosition pos,
				       gint		event_x,
				       gint		event_y);
static void e_day_view_on_event_double_click (EDayView *day_view,
					      gint day,
					      gint event_num);
static void e_day_view_on_event_right_click (EDayView *day_view,
					     GdkEventButton *bevent,
					     gint day,
					     gint event_num);

static void e_day_view_recalc_day_starts (EDayView *day_view,
					  time_t start_time);
static void e_day_view_recalc_num_rows	(EDayView	*day_view);

static EDayViewPosition e_day_view_convert_position_in_top_canvas (EDayView *day_view,
								   gint x,
								   gint y,
								   gint *day_return,
								   gint *event_num_return);
static EDayViewPosition e_day_view_convert_position_in_main_canvas (EDayView *day_view,
								    gint x,
								    gint y,
								    gint *day_return,
								    gint *row_return,
								    gint *event_num_return);
static gboolean e_day_view_find_event_from_item (EDayView *day_view,
						 GnomeCanvasItem *item,
						 gint *day_return,
						 gint *event_num_return);
static gboolean e_day_view_find_event_from_uid (EDayView *day_view,
						const gchar *uid,
						gint *day_return,
						gint *event_num_return);

typedef gboolean (* EDayViewForeachEventCallback) (EDayView *day_view,
						   gint day,
						   gint event_num,
						   gpointer data);

static void e_day_view_foreach_event_with_uid (EDayView *day_view,
					       const gchar *uid,
					       EDayViewForeachEventCallback callback,
					       gpointer data);

static void e_day_view_reload_events (EDayView *day_view);
static void e_day_view_free_events (EDayView *day_view);
static void e_day_view_free_event_array (EDayView *day_view,
					 GArray *array);
static int e_day_view_add_event (CalComponent *comp,
				 time_t	  start,
				 time_t	  end,
				 gpointer data);
static void e_day_view_update_event_label (EDayView *day_view,
					   gint day,
					   gint event_num);
static void e_day_view_update_long_event_label (EDayView *day_view,
						gint event_num);

static void e_day_view_layout_long_events (EDayView *day_view);
static void e_day_view_layout_long_event (EDayView	   *day_view,
					  EDayViewEvent *event,
					  guint8	   *grid);
static void e_day_view_reshape_long_events (EDayView *day_view);
static void e_day_view_reshape_long_event (EDayView *day_view,
					   gint event_num);
static void e_day_view_layout_day_events (EDayView *day_view,
					  gint	day);
static void e_day_view_layout_day_event (EDayView   *day_view,
					 gint	    day,
					 EDayViewEvent *event,
					 guint8	   *grid,
					 guint16   *group_starts);
static void e_day_view_expand_day_event (EDayView	   *day_view,
					 gint	    day,
					 EDayViewEvent *event,
					 guint8	   *grid);
static void e_day_view_recalc_cols_per_row (EDayView *day_view,
					    gint      day,
					    guint16  *group_starts);
static void e_day_view_reshape_day_events (EDayView *day_view,
					   gint day);
static void e_day_view_reshape_day_event (EDayView *day_view,
					  gint	day,
					  gint	event_num);
static void e_day_view_reshape_main_canvas_resize_bars (EDayView *day_view);
static void e_day_view_reshape_resize_long_event_rect_item (EDayView *day_view);
static void e_day_view_reshape_resize_rect_item (EDayView *day_view);

static void e_day_view_ensure_events_sorted (EDayView *day_view);
static gint e_day_view_event_sort_func (const void *arg1,
					const void *arg2);

static void e_day_view_start_editing_event (EDayView *day_view,
					    gint day,
					    gint event_num,
					    gchar *initial_text);
static void e_day_view_stop_editing_event (EDayView *day_view);
static gboolean e_day_view_on_text_item_event (GnomeCanvasItem *item,
					       GdkEvent *event,
					       EDayView *day_view);
static void e_day_view_on_editing_started (EDayView *day_view,
					   GnomeCanvasItem *item);
static void e_day_view_on_editing_stopped (EDayView *day_view,
					   GnomeCanvasItem *item);

static time_t e_day_view_convert_grid_position_to_time (EDayView *day_view,
							gint col,
							gint row);
static gboolean e_day_view_convert_time_to_grid_position (EDayView *day_view,
							  time_t time,
							  gint *col,
							  gint *row);

static void e_day_view_start_auto_scroll (EDayView *day_view,
					  gboolean scroll_up);
static gboolean e_day_view_auto_scroll_handler (gpointer data);

static void e_day_view_on_new_appointment (GtkWidget *widget,
					   gpointer data);
static void e_day_view_on_edit_appointment (GtkWidget *widget,
					    gpointer data);
static void e_day_view_on_delete_occurrence (GtkWidget *widget,
					     gpointer data);
static void e_day_view_on_delete_appointment (GtkWidget *widget,
					      gpointer data);
static void e_day_view_on_unrecur_appointment (GtkWidget *widget,
					       gpointer data);
static EDayViewEvent* e_day_view_get_popup_menu_event (EDayView *day_view);

static gint e_day_view_on_top_canvas_drag_motion (GtkWidget      *widget,
						  GdkDragContext *context,
						  gint            x,
						  gint            y,
						  guint           time,
						  EDayView	  *day_view);
static void e_day_view_update_top_canvas_drag (EDayView *day_view,
					       gint day);
static void e_day_view_reshape_top_canvas_drag_item (EDayView *day_view);
static gint e_day_view_on_main_canvas_drag_motion (GtkWidget      *widget,
						   GdkDragContext *context,
						   gint            x,
						   gint            y,
						   guint           time,
						   EDayView	  *day_view);
static void e_day_view_reshape_main_canvas_drag_item (EDayView *day_view);
static void e_day_view_update_main_canvas_drag (EDayView *day_view,
						gint row,
						gint day);
static void e_day_view_on_top_canvas_drag_leave (GtkWidget      *widget,
						 GdkDragContext *context,
						 guint           time,
						 EDayView	     *day_view);
static void e_day_view_on_main_canvas_drag_leave (GtkWidget      *widget,
						  GdkDragContext *context,
						  guint           time,
						  EDayView	 *day_view);
static void e_day_view_on_drag_begin (GtkWidget      *widget,
				      GdkDragContext *context,
				      EDayView	   *day_view);
static void e_day_view_on_drag_end (GtkWidget      *widget,
				    GdkDragContext *context,
				    EDayView       *day_view);
static void e_day_view_on_drag_data_get (GtkWidget          *widget,
					 GdkDragContext     *context,
					 GtkSelectionData   *selection_data,
					 guint               info,
					 guint               time,
					 EDayView		*day_view);
static void e_day_view_on_top_canvas_drag_data_received (GtkWidget	*widget,
							 GdkDragContext *context,
							 gint            x,
							 gint            y,
							 GtkSelectionData *data,
							 guint           info,
							 guint           time,
							 EDayView	*day_view);
static void e_day_view_on_main_canvas_drag_data_received (GtkWidget      *widget,
							  GdkDragContext *context,
							  gint            x,
							  gint            y,
							  GtkSelectionData *data,
							  guint           info,
							  guint           time,
							  EDayView	 *day_view);
static gboolean e_day_view_update_event_cb (EDayView *day_view,
					    gint day,
					    gint event_num,
					    gpointer data);
static gboolean e_day_view_remove_event_cb (EDayView *day_view,
					    gint day,
					    gint event_num,
					    gpointer data);
static void e_day_view_normalize_selection (EDayView *day_view);


static GtkTableClass *parent_class;


GtkType
e_day_view_get_type (void)
{
	static GtkType e_day_view_type = 0;

	if (!e_day_view_type){
		GtkTypeInfo e_day_view_info = {
			"EDayView",
			sizeof (EDayView),
			sizeof (EDayViewClass),
			(GtkClassInitFunc) e_day_view_class_init,
			(GtkObjectInitFunc) e_day_view_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		parent_class = gtk_type_class (GTK_TYPE_TABLE);
		e_day_view_type = gtk_type_unique (GTK_TYPE_TABLE,
						   &e_day_view_info);
	}

	return e_day_view_type;
}


static void
e_day_view_class_init (EDayViewClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	/* Method override */
	object_class->destroy		= e_day_view_destroy;

	widget_class->realize		= e_day_view_realize;
	widget_class->unrealize		= e_day_view_unrealize;
	widget_class->style_set		= e_day_view_style_set;
 	widget_class->size_allocate	= e_day_view_size_allocate;
	widget_class->focus_in_event	= e_day_view_focus_in;
	widget_class->focus_out_event	= e_day_view_focus_out;
	widget_class->key_press_event	= e_day_view_key_press;
}


static void
e_day_view_init (EDayView *day_view)
{
	GdkColormap *colormap;
	gboolean success[E_DAY_VIEW_COLOR_LAST];
	gint day, nfailed;
	GnomeCanvasGroup *canvas_group;

	GTK_WIDGET_SET_FLAGS (day_view, GTK_CAN_FOCUS);

	colormap = gtk_widget_get_colormap (GTK_WIDGET (day_view));

	day_view->calendar = NULL;

	day_view->long_events = g_array_new (FALSE, FALSE,
					     sizeof (EDayViewEvent));
	day_view->long_events_sorted = TRUE;
	day_view->long_events_need_layout = FALSE;
	day_view->long_events_need_reshape = FALSE;

	for (day = 0; day < E_DAY_VIEW_MAX_DAYS; day++) {
		day_view->events[day] = g_array_new (FALSE, FALSE,
						     sizeof (EDayViewEvent));
		day_view->events_sorted[day] = TRUE;
		day_view->need_layout[day] = FALSE;
		day_view->need_reshape[day] = FALSE;
	}

	/* These indicate that the times haven't been set. */
	day_view->lower = 0;
	day_view->upper = 0;

	/* FIXME: Initialize day_starts. */
	day_view->days_shown = 1;

	day_view->mins_per_row = 30;
	day_view->date_format = E_DAY_VIEW_DATE_FULL;
	day_view->rows_in_top_display = 0;

	/* Note that these don't work yet. It would need a few fixes to the
	   way event->start_minute and event->end_minute are used, and there
	   may be problems with events that go outside the visible times. */
	day_view->first_hour_shown = 0;
	day_view->first_minute_shown = 0;
	day_view->last_hour_shown = 24;
	day_view->last_minute_shown = 0;

	day_view->main_gc = NULL;
	e_day_view_recalc_num_rows (day_view);

	day_view->work_day_start_hour = 9;
	day_view->work_day_start_minute = 0;
	day_view->work_day_end_hour = 17;
	day_view->work_day_end_minute = 0;
	day_view->scroll_to_work_day = TRUE;

	day_view->editing_event_day = -1;
	day_view->editing_event_num = -1;
	day_view->editing_new_event = FALSE;

	day_view->resize_bars_event_day = -1;
	day_view->resize_bars_event_num = -1;

	day_view->selection_start_row = -1;
	day_view->selection_start_day = -1;
	day_view->selection_end_row = -1;
	day_view->selection_end_day = -1;
	day_view->selection_is_being_dragged = FALSE;
	day_view->selection_drag_pos = E_DAY_VIEW_DRAG_END;
	day_view->selection_in_top_canvas = FALSE;

	day_view->resize_drag_pos = E_DAY_VIEW_POS_NONE;

	day_view->pressed_event_day = -1;

	day_view->drag_event_day = -1;
	day_view->drag_last_day = -1;

	day_view->auto_scroll_timeout_id = 0;

	/* Create the large font. */
	day_view->large_font = gdk_font_load (E_DAY_VIEW_LARGE_FONT);
	if (!day_view->large_font)
		day_view->large_font = gdk_font_load (E_DAY_VIEW_LARGE_FONT_FALLBACK);
	if (!day_view->large_font)
		g_warning ("Couldn't load font");


	/* Allocate the colors. */
#if 1
	day_view->colors[E_DAY_VIEW_COLOR_BG_WORKING].red   = 247 * 257;
	day_view->colors[E_DAY_VIEW_COLOR_BG_WORKING].green = 247 * 257;
	day_view->colors[E_DAY_VIEW_COLOR_BG_WORKING].blue  = 244 * 257;

	day_view->colors[E_DAY_VIEW_COLOR_BG_NOT_WORKING].red   = 216 * 257;
	day_view->colors[E_DAY_VIEW_COLOR_BG_NOT_WORKING].green = 216 * 257;
	day_view->colors[E_DAY_VIEW_COLOR_BG_NOT_WORKING].blue  = 214 * 257;
#else

	/* FG: MistyRose1, LightPink3 | RosyBrown | MistyRose3. */

	day_view->colors[E_DAY_VIEW_COLOR_BG_WORKING].red   = 255 * 257;
	day_view->colors[E_DAY_VIEW_COLOR_BG_WORKING].green = 228 * 257;
	day_view->colors[E_DAY_VIEW_COLOR_BG_WORKING].blue  = 225 * 257;

	day_view->colors[E_DAY_VIEW_COLOR_BG_NOT_WORKING].red   = 238 * 257;
	day_view->colors[E_DAY_VIEW_COLOR_BG_NOT_WORKING].green = 162 * 257;
	day_view->colors[E_DAY_VIEW_COLOR_BG_NOT_WORKING].blue  = 173 * 257;
#endif

	day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR].red   = 0;
	day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR].green = 0;
	day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR].blue  = 65535;

	day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND].red   = 65535;
	day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND].green = 65535;
	day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND].blue  = 65535;

	day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER].red   = 0;
	day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER].green = 0;
	day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER].blue  = 0;

	nfailed = gdk_colormap_alloc_colors (colormap, day_view->colors,
					     E_DAY_VIEW_COLOR_LAST, FALSE,
					     TRUE, success);
	if (nfailed)
		g_warning ("Failed to allocate all colors");



	/*
	 * Top Canvas
	 */
	day_view->top_canvas = e_canvas_new ();
	gtk_table_attach (GTK_TABLE (day_view), day_view->top_canvas,
			  1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show (day_view->top_canvas);
	gtk_signal_connect_after (GTK_OBJECT (day_view->top_canvas), "button_press_event",
				  GTK_SIGNAL_FUNC (e_day_view_on_top_canvas_button_press),
				  day_view);
	gtk_signal_connect_after (GTK_OBJECT (day_view->top_canvas), "button_release_event",
				  GTK_SIGNAL_FUNC (e_day_view_on_top_canvas_button_release),
				  day_view);
	gtk_signal_connect_after (GTK_OBJECT (day_view->top_canvas), "motion_notify_event",
				  GTK_SIGNAL_FUNC (e_day_view_on_top_canvas_motion),
				  day_view);
	gtk_signal_connect_after (GTK_OBJECT (day_view->top_canvas),
				  "drag_motion",
				  GTK_SIGNAL_FUNC (e_day_view_on_top_canvas_drag_motion),
				  day_view);
	gtk_signal_connect_after (GTK_OBJECT (day_view->top_canvas),
				  "drag_leave",
				  GTK_SIGNAL_FUNC (e_day_view_on_top_canvas_drag_leave),
				  day_view);
	gtk_signal_connect (GTK_OBJECT (day_view->top_canvas),
			    "drag_begin",
			    GTK_SIGNAL_FUNC (e_day_view_on_drag_begin),
			    day_view);
	gtk_signal_connect (GTK_OBJECT (day_view->top_canvas),
			    "drag_end",
			    GTK_SIGNAL_FUNC (e_day_view_on_drag_end),
			    day_view);
	gtk_signal_connect (GTK_OBJECT (day_view->top_canvas),
			    "drag_data_get",
			    GTK_SIGNAL_FUNC (e_day_view_on_drag_data_get),
			    day_view);
	gtk_signal_connect (GTK_OBJECT (day_view->top_canvas),
			    "drag_data_received",
			    GTK_SIGNAL_FUNC (e_day_view_on_top_canvas_drag_data_received),
			    day_view);

	canvas_group = GNOME_CANVAS_GROUP (GNOME_CANVAS (day_view->top_canvas)->root);

	day_view->top_canvas_item =
		gnome_canvas_item_new (canvas_group,
				       e_day_view_top_item_get_type (),
				       "EDayViewTopItem::day_view", day_view,
				       NULL);

	day_view->resize_long_event_rect_item =
		gnome_canvas_item_new (canvas_group,
				       gnome_canvas_rect_get_type(),
				       "fill_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND],
				       "outline_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER],
				       NULL);
	gnome_canvas_item_hide (day_view->resize_long_event_rect_item);

	day_view->drag_long_event_rect_item =
		gnome_canvas_item_new (canvas_group,
				       gnome_canvas_rect_get_type (),
				       "width_pixels", 1,
				       "fill_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND],
				       "outline_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER],
				       NULL);
	gnome_canvas_item_hide (day_view->drag_long_event_rect_item);

	day_view->drag_long_event_item =
		gnome_canvas_item_new (canvas_group,
				       e_text_get_type (),
				       "anchor", GTK_ANCHOR_NW,
				       "line_wrap", TRUE,
				       "clip", TRUE,
				       "max_lines", 1,
				       "editable", TRUE,
				       NULL);
	gnome_canvas_item_hide (day_view->drag_long_event_item);

	/*
	 * Main Canvas
	 */
	day_view->main_canvas = e_canvas_new ();
	gtk_table_attach (GTK_TABLE (day_view), day_view->main_canvas,
			  1, 2, 1, 2,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show (day_view->main_canvas);
	gtk_signal_connect (GTK_OBJECT (day_view->main_canvas), "realize",
			    GTK_SIGNAL_FUNC (e_day_view_on_canvas_realized),
			    day_view);
	gtk_signal_connect_after (GTK_OBJECT (day_view->main_canvas),
				  "button_press_event",
				  GTK_SIGNAL_FUNC (e_day_view_on_main_canvas_button_press),
				  day_view);
	gtk_signal_connect_after (GTK_OBJECT (day_view->main_canvas),
				  "button_release_event",
				  GTK_SIGNAL_FUNC (e_day_view_on_main_canvas_button_release),
				  day_view);
	gtk_signal_connect_after (GTK_OBJECT (day_view->main_canvas),
				  "motion_notify_event",
				  GTK_SIGNAL_FUNC (e_day_view_on_main_canvas_motion),
				  day_view);
	gtk_signal_connect_after (GTK_OBJECT (day_view->main_canvas),
				  "drag_motion",
				  GTK_SIGNAL_FUNC (e_day_view_on_main_canvas_drag_motion),
				  day_view);
	gtk_signal_connect_after (GTK_OBJECT (day_view->main_canvas),
				  "drag_leave",
				  GTK_SIGNAL_FUNC (e_day_view_on_main_canvas_drag_leave),
				  day_view);
	gtk_signal_connect (GTK_OBJECT (day_view->main_canvas),
			    "drag_begin",
			    GTK_SIGNAL_FUNC (e_day_view_on_drag_begin),
			    day_view);
	gtk_signal_connect (GTK_OBJECT (day_view->main_canvas),
			    "drag_end",
			    GTK_SIGNAL_FUNC (e_day_view_on_drag_end),
			    day_view);
	gtk_signal_connect (GTK_OBJECT (day_view->main_canvas),
			    "drag_data_get",
			    GTK_SIGNAL_FUNC (e_day_view_on_drag_data_get),
			    day_view);
	gtk_signal_connect (GTK_OBJECT (day_view->main_canvas),
			    "drag_data_received",
			    GTK_SIGNAL_FUNC (e_day_view_on_main_canvas_drag_data_received),
			    day_view);

	canvas_group = GNOME_CANVAS_GROUP (GNOME_CANVAS (day_view->main_canvas)->root);

	day_view->main_canvas_item =
		gnome_canvas_item_new (canvas_group,
				       e_day_view_main_item_get_type (),
				       "EDayViewMainItem::day_view", day_view,
				       NULL);

	day_view->resize_rect_item =
		gnome_canvas_item_new (canvas_group,
				       gnome_canvas_rect_get_type(),
				       "fill_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND],
				       "outline_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER],
				       NULL);
	gnome_canvas_item_hide (day_view->resize_rect_item);

	day_view->resize_bar_item =
		gnome_canvas_item_new (canvas_group,
				       gnome_canvas_rect_get_type(),
				       "fill_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR],
				       "outline_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER],
				       NULL);
	gnome_canvas_item_hide (day_view->resize_bar_item);

	day_view->main_canvas_top_resize_bar_item =
		gnome_canvas_item_new (canvas_group,
				       gnome_canvas_rect_get_type (),
				       "width_pixels", 1,
				       "fill_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR],
				       "outline_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER],
				       NULL);
	gnome_canvas_item_hide (day_view->main_canvas_top_resize_bar_item);

	day_view->main_canvas_bottom_resize_bar_item =
		gnome_canvas_item_new (canvas_group,
				       gnome_canvas_rect_get_type (),
				       "width_pixels", 1,
				       "fill_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR],
				       "outline_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER],
				       NULL);
	gnome_canvas_item_hide (day_view->main_canvas_bottom_resize_bar_item);


	day_view->drag_rect_item =
		gnome_canvas_item_new (canvas_group,
				       gnome_canvas_rect_get_type (),
				       "width_pixels", 1,
				       "fill_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND],
				       "outline_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER],
				       NULL);
	gnome_canvas_item_hide (day_view->drag_rect_item);

	day_view->drag_bar_item =
		gnome_canvas_item_new (canvas_group,
				       gnome_canvas_rect_get_type (),
				       "width_pixels", 1,
				       "fill_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR],
				       "outline_color_gdk", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER],
				       NULL);
	gnome_canvas_item_hide (day_view->drag_bar_item);

	day_view->drag_item =
		gnome_canvas_item_new (canvas_group,
				       e_text_get_type (),
				       "anchor", GTK_ANCHOR_NW,
				       "line_wrap", TRUE,
				       "clip", TRUE,
				       "editable", TRUE,
				       NULL);
	gnome_canvas_item_hide (day_view->drag_item);


	/*
	 * Times Canvas
	 */
	day_view->time_canvas = e_canvas_new ();
	gtk_layout_set_vadjustment (GTK_LAYOUT (day_view->time_canvas),
				    GTK_LAYOUT (day_view->main_canvas)->vadjustment);
	gtk_table_attach (GTK_TABLE (day_view), day_view->time_canvas,
			  0, 1, 1, 2,
			  GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show (day_view->time_canvas);
	gtk_signal_connect_after (GTK_OBJECT (day_view->time_canvas),
				  "button_press_event",
				  GTK_SIGNAL_FUNC (e_day_view_on_time_canvas_button_press),
				  day_view);
	
	canvas_group = GNOME_CANVAS_GROUP (GNOME_CANVAS (day_view->time_canvas)->root);

	day_view->time_canvas_item =
		gnome_canvas_item_new (canvas_group,
				       e_day_view_time_item_get_type (),
				       "EDayViewTimeItem::day_view", day_view,
				       NULL);


	/*
	 * Scrollbar.
	 */
	day_view->vscrollbar = gtk_vscrollbar_new (GTK_LAYOUT (day_view->main_canvas)->vadjustment);
	gtk_table_attach (GTK_TABLE (day_view), day_view->vscrollbar,
			  2, 3, 1, 2, 0, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show (day_view->vscrollbar);


	/* Create the pixmaps. */
	day_view->reminder_icon = gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap, &day_view->reminder_mask, NULL, bell_xpm);
	day_view->recurrence_icon = gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap, &day_view->recurrence_mask, NULL, recur_xpm);


	/* Create the cursors. */
	day_view->normal_cursor = gdk_cursor_new (GDK_TOP_LEFT_ARROW);
	day_view->move_cursor = gdk_cursor_new (GDK_FLEUR);
	day_view->resize_width_cursor = gdk_cursor_new (GDK_SB_H_DOUBLE_ARROW);
	day_view->resize_height_cursor = gdk_cursor_new (GDK_SB_V_DOUBLE_ARROW);
	day_view->last_cursor_set_in_top_canvas = NULL;
	day_view->last_cursor_set_in_main_canvas = NULL;

	/* Set up the drop sites. */
	gtk_drag_dest_set (day_view->top_canvas,
			   GTK_DEST_DEFAULT_ALL,
			   target_table, n_targets,
			   GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_drag_dest_set (day_view->main_canvas,
			   GTK_DEST_DEFAULT_ALL,
			   target_table, n_targets,
			   GDK_ACTION_COPY | GDK_ACTION_MOVE);
}


/* Turn off the background of the canvas windows. This reduces flicker
   considerably when scrolling. (Why isn't it in GnomeCanvas?). */
static void
e_day_view_on_canvas_realized (GtkWidget *widget,
			       EDayView *day_view)
{
	gdk_window_set_back_pixmap (GTK_LAYOUT (widget)->bin_window,
				    NULL, FALSE);
}


/**
 * e_day_view_new:
 * @Returns: a new #EDayView.
 *
 * Creates a new #EDayView.
 **/
GtkWidget *
e_day_view_new (void)
{
	GtkWidget *day_view;

	day_view = GTK_WIDGET (gtk_type_new (e_day_view_get_type ()));

	return day_view;
}


static void
e_day_view_destroy (GtkObject *object)
{
	EDayView *day_view;
	gint day;

	day_view = E_DAY_VIEW (object);

	e_day_view_stop_auto_scroll (day_view);

	if (day_view->large_font)
		gdk_font_unref (day_view->large_font);

	gdk_cursor_destroy (day_view->normal_cursor);
	gdk_cursor_destroy (day_view->move_cursor);
	gdk_cursor_destroy (day_view->resize_width_cursor);
	gdk_cursor_destroy (day_view->resize_height_cursor);

	e_day_view_free_events (day_view);
	g_array_free (day_view->long_events, TRUE);
	for (day = 0; day < E_DAY_VIEW_MAX_DAYS; day++)
		g_array_free (day_view->events[day], TRUE);

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}


static void
e_day_view_realize (GtkWidget *widget)
{
	EDayView *day_view;

	if (GTK_WIDGET_CLASS (parent_class)->realize)
		(*GTK_WIDGET_CLASS (parent_class)->realize)(widget);

	day_view = E_DAY_VIEW (widget);
	day_view->main_gc = gdk_gc_new (widget->window);
}


static void
e_day_view_unrealize (GtkWidget *widget)
{
	EDayView *day_view;

	day_view = E_DAY_VIEW (widget);

	gdk_gc_unref (day_view->main_gc);
	day_view->main_gc = NULL;

	if (GTK_WIDGET_CLASS (parent_class)->unrealize)
		(*GTK_WIDGET_CLASS (parent_class)->unrealize)(widget);
}


static void
e_day_view_style_set (GtkWidget *widget,
		      GtkStyle  *previous_style)
{
	EDayView *day_view;
	GdkFont *font;
	gint top_rows, top_canvas_height;
	gint month, max_month_width, max_abbr_month_width, number_width;
	gint hour, max_large_hour_width, month_width;
	gint minute, max_minute_width, i;
	GDate date;
	gchar buffer[128];
	gint times_width;

	if (GTK_WIDGET_CLASS (parent_class)->style_set)
		(*GTK_WIDGET_CLASS (parent_class)->style_set)(widget, previous_style);

	day_view = E_DAY_VIEW (widget);
	font = widget->style->font;

	/* Recalculate the height of each row based on the font size. */
	day_view->row_height = font->ascent + font->descent + E_DAY_VIEW_EVENT_BORDER_HEIGHT * 2 + E_DAY_VIEW_EVENT_Y_PAD * 2 + 2 /* FIXME */;
	day_view->row_height = MAX (day_view->row_height, E_DAY_VIEW_ICON_HEIGHT + E_DAY_VIEW_ICON_Y_PAD + 2);
	GTK_LAYOUT (day_view->main_canvas)->vadjustment->step_increment = day_view->row_height;

	day_view->top_row_height = font->ascent + font->descent + E_DAY_VIEW_LONG_EVENT_BORDER_HEIGHT * 2 + E_DAY_VIEW_LONG_EVENT_Y_PAD * 2 + E_DAY_VIEW_TOP_CANVAS_Y_GAP;
	day_view->top_row_height = MAX (day_view->top_row_height, E_DAY_VIEW_ICON_HEIGHT + E_DAY_VIEW_ICON_Y_PAD + 2 + E_DAY_VIEW_TOP_CANVAS_Y_GAP);

	/* Set the height of the top canvas based on the row height and the
	   number of rows needed (min 1 + 1 for the dates + 1 space for DnD).*/
	top_rows = MAX (1, day_view->rows_in_top_display);
	top_canvas_height = (top_rows + 2) * day_view->top_row_height;
	gtk_widget_set_usize (day_view->top_canvas, -1, top_canvas_height);

	/* Find the biggest full month name. */
	g_date_clear (&date, 1);
	g_date_set_dmy (&date, 20, 1, 2000);
	max_month_width = 0;
	max_abbr_month_width = 0;
	for (month = 1; month <= 12; month++) {
		g_date_set_month (&date, month);

		g_date_strftime (buffer, 128, "%B", &date);
		month_width = gdk_string_width (font, buffer);
		max_month_width = MAX (max_month_width, month_width);

		g_date_strftime (buffer, 128, "%b", &date);
		month_width = gdk_string_width (font, buffer);
		max_abbr_month_width = MAX (max_abbr_month_width, month_width);
	}
	number_width = gdk_string_width (font, "31 ");
	day_view->long_format_width = number_width + max_month_width
		+ E_DAY_VIEW_DATE_X_PAD;
	day_view->abbreviated_format_width = number_width
		+ max_abbr_month_width + E_DAY_VIEW_DATE_X_PAD;

	/* Calculate the widths of all the time strings necessary. */
	day_view->max_small_hour_width = 0;
	max_large_hour_width = 0;
	for (hour = 0; hour < 24; hour++) {
		sprintf (buffer, "%02i", hour);
		day_view->small_hour_widths[hour] = gdk_string_width (font, buffer);
		day_view->large_hour_widths[hour] = gdk_string_width (day_view->large_font, buffer);
		day_view->max_small_hour_width = MAX (day_view->max_small_hour_width, day_view->small_hour_widths[hour]);
		max_large_hour_width = MAX (max_large_hour_width, day_view->large_hour_widths[hour]);
	}
	day_view->max_large_hour_width = max_large_hour_width;

	max_minute_width = 0;
	for (minute = 0, i = 0; minute < 60; minute += 5, i++) {
		sprintf (buffer, "%02i", minute);
		day_view->minute_widths[i] = gdk_string_width (font, buffer);
		max_minute_width = MAX (max_minute_width, day_view->minute_widths[i]);
	}
	day_view->max_minute_width = max_minute_width;
	day_view->colon_width = gdk_string_width (font, ":");

	/* Calculate the width of the time column. */
	times_width = e_day_view_time_item_get_column_width (E_DAY_VIEW_TIME_ITEM (day_view->time_canvas_item));
	gtk_widget_set_usize (day_view->time_canvas, times_width, -1);
}


/* This recalculates the sizes of each column. */
static void
e_day_view_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	EDayView *day_view;
	gfloat width, offset;
	gint col, day, scroll_y;
	gboolean need_reshape;
	gdouble old_x2, old_y2, new_x2, new_y2;

#if 0
	g_print ("In e_day_view_size_allocate\n");
#endif
	day_view = E_DAY_VIEW (widget);

	(*GTK_WIDGET_CLASS (parent_class)->size_allocate) (widget, allocation);

	/* Calculate the column sizes, using floating point so that pixels
	   get divided evenly. Note that we use one more element than the
	   number of columns, to make it easy to get the column widths. */
	width = day_view->main_canvas->allocation.width;
	width /= day_view->days_shown;
	offset = 0;
	for (col = 0; col <= day_view->days_shown; col++) {
		day_view->day_offsets[col] = floor (offset + 0.5);
		offset += width;
	}

	/* Calculate the days widths based on the offsets. */
	for (col = 0; col < day_view->days_shown; col++) {
		day_view->day_widths[col] = day_view->day_offsets[col + 1] - day_view->day_offsets[col];
	}

	/* Determine which date format to use, based on the column widths. */
	if (day_view->day_widths[0] > day_view->long_format_width)
		day_view->date_format = E_DAY_VIEW_DATE_FULL;
	else if (day_view->day_widths[0] > day_view->abbreviated_format_width)
		day_view->date_format = E_DAY_VIEW_DATE_ABBREVIATED;
	else
		day_view->date_format = E_DAY_VIEW_DATE_SHORT;

	/* Set the scroll region of the top canvas to its allocated size. */
	gnome_canvas_get_scroll_region (GNOME_CANVAS (day_view->top_canvas),
					NULL, NULL, &old_x2, &old_y2);
	new_x2 = day_view->top_canvas->allocation.width - 1;
	new_y2 = day_view->top_canvas->allocation.height - 1;
	if (old_x2 != new_x2 || old_y2 != new_y2)
		gnome_canvas_set_scroll_region (GNOME_CANVAS (day_view->top_canvas),
						0, 0, new_x2, new_y2);

	need_reshape = e_day_view_update_scroll_regions (day_view);

	/* Scroll to the start of the working day, if this is the initial
	   allocation. */
	if (day_view->scroll_to_work_day) {
		scroll_y = e_day_view_convert_time_to_position (day_view, day_view->work_day_start_hour, day_view->work_day_start_minute);
		gnome_canvas_scroll_to (GNOME_CANVAS (day_view->main_canvas),
					0, scroll_y);
		day_view->scroll_to_work_day = FALSE;
	}

	/* Flag that we need to reshape the events. Note that changes in height
	   don't matter, since the rows are always the same height. */
	if (need_reshape) {
		day_view->long_events_need_reshape = TRUE;
		for (day = 0; day < E_DAY_VIEW_MAX_DAYS; day++)
			day_view->need_reshape[day] = TRUE;

		e_day_view_check_layout (day_view);
	}
}


static gint
e_day_view_focus_in (GtkWidget *widget, GdkEventFocus *event)
{
	EDayView *day_view;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (E_IS_DAY_VIEW (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	day_view = E_DAY_VIEW (widget);

	GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);

	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);

	return FALSE;
}


static gint
e_day_view_focus_out (GtkWidget *widget, GdkEventFocus *event)
{
	EDayView *day_view;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (E_IS_DAY_VIEW (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	day_view = E_DAY_VIEW (widget);

	GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);

	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);

	return FALSE;
}


void
e_day_view_set_calendar		(EDayView	*day_view,
				 GnomeCalendar	*calendar)
{
	g_return_if_fail (E_IS_DAY_VIEW (day_view));

	day_view->calendar = calendar;

	/* FIXME: free current events? */
}


/* This reloads all calendar events. */
void
e_day_view_update_all_events	(EDayView	*day_view)
{
	e_day_view_reload_events (day_view);
}


/* This is called when one event has been added or updated. */
void
e_day_view_update_event		(EDayView	*day_view,
				 const gchar	*uid)
{
	EDayViewEvent *event;
	CalComponent *comp;
	CalClientGetStatus status;
	gint day, event_num;

	g_return_if_fail (E_IS_DAY_VIEW (day_view));

	g_print ("In e_day_view_update_event day_view:%p uid:%s\n",
		 day_view, uid);

	/* If our calendar or time hasn't been set yet, just return. */
	if (!day_view->calendar
	    || (day_view->lower == 0 && day_view->upper == 0))
		return;

	/* Get the event from the server. */
	status = cal_client_get_object (day_view->calendar->client, uid, &comp);

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
	if (e_day_view_find_event_from_uid (day_view, uid, &day, &event_num)) {
		if (day == E_DAY_VIEW_LONG_EVENT)
			event = &g_array_index (day_view->long_events,
						EDayViewEvent, event_num);
		else
			event = &g_array_index (day_view->events[day],
						EDayViewEvent, event_num);
#warning "FIX ME"
		/* Do this the long way every time for now */
#if 0
		if (ical_object_compare_dates (event->ico, ico)) {
			g_print ("updated object's dates unchanged\n");
			e_day_view_foreach_event_with_uid (day_view, uid, e_day_view_update_event_cb, ico);
			ical_object_unref (ico);
			gtk_widget_queue_draw (day_view->top_canvas);
			gtk_widget_queue_draw (day_view->main_canvas);
			return;
		}
#endif
		/* The dates have changed, so we need to remove the
		   old occurrrences before adding the new ones. */
		g_print ("dates changed - removing occurrences\n");
		e_day_view_foreach_event_with_uid (day_view, uid,
						   e_day_view_remove_event_cb,
						   NULL);
	}

	/* Add the occurrences of the event. */
	cal_recur_generate_instances (comp, day_view->lower, 
				      day_view->upper,
				      e_day_view_add_event, 
				      day_view);
	gtk_object_unref (GTK_OBJECT (comp));

	e_day_view_check_layout (day_view);

	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);
}


static gboolean
e_day_view_update_event_cb (EDayView *day_view,
			    gint day,
			    gint event_num,
			    gpointer data)
{
	EDayViewEvent *event;
	CalComponent *comp;

	comp = data;
#if 0
	g_print ("In e_day_view_update_event_cb day:%i event_num:%i\n",
		 day, event_num);
#endif
	if (day == E_DAY_VIEW_LONG_EVENT) {
		event = &g_array_index (day_view->long_events, EDayViewEvent,
					event_num);
	} else {
		event = &g_array_index (day_view->events[day], EDayViewEvent,
					event_num);
	}

	gtk_object_unref (GTK_OBJECT (event->comp));
	event->comp = comp;
	gtk_object_ref (GTK_OBJECT (comp));

	/* If we are editing an event which we have just created, we will get
	   an update_event callback from the server. But we need to ignore it
	   or we will lose the text the user has already typed in. */
	if (day_view->editing_new_event
	    && day_view->editing_event_day == day
	    && day_view->editing_event_num == event_num) {
		return TRUE;
	}

	if (day == E_DAY_VIEW_LONG_EVENT) {
		e_day_view_update_long_event_label (day_view, event_num);
		e_day_view_reshape_long_event (day_view, event_num);
	} else {
		e_day_view_update_event_label (day_view, day, event_num);
		e_day_view_reshape_day_event (day_view, day, event_num);
	}
	return TRUE;
}


/* This calls a given function for each event instance that matches the given
   uid. Note that it is safe for the callback to remove the event (since we
   step backwards through the arrays). */
static void
e_day_view_foreach_event_with_uid (EDayView *day_view,
				   const gchar *uid,
				   EDayViewForeachEventCallback callback,
				   gpointer data)
{
	EDayViewEvent *event;
	gint day, event_num;
	const char *u;
	
	for (day = 0; day < day_view->days_shown; day++) {
		for (event_num = day_view->events[day]->len - 1;
		     event_num >= 0;
		     event_num--) {
			event = &g_array_index (day_view->events[day],
						EDayViewEvent, event_num);

			cal_component_get_uid (event->comp, &u);
			if (uid && !strcmp (uid, u)) {
				if (!(*callback) (day_view, day, event_num,
						  data))
					return;
			}
		}
	}

	for (event_num = day_view->long_events->len - 1;
	     event_num >= 0;
	     event_num--) {
		event = &g_array_index (day_view->long_events,
					EDayViewEvent, event_num);

		cal_component_get_uid (event->comp, &u);
		if (u && !strcmp (uid, u)) {
			if (!(*callback) (day_view, E_DAY_VIEW_LONG_EVENT,
					  event_num, data))
				return;
		}
	}
}


/* This removes all the events associated with the given uid. Note that for
   recurring events there may be more than one. If any events are found and
   removed we need to layout the events again. */
void
e_day_view_remove_event	(EDayView	*day_view,
			 const gchar	*uid)
{
	g_return_if_fail (E_IS_DAY_VIEW (day_view));

#if 1
	g_print ("In e_day_view_remove_event day_view:%p uid:%s\n",
		 day_view, uid);
#endif

	e_day_view_foreach_event_with_uid (day_view, uid,
					   e_day_view_remove_event_cb, NULL);

	e_day_view_check_layout (day_view);
	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);
}


static gboolean
e_day_view_remove_event_cb (EDayView *day_view,
			    gint day,
			    gint event_num,
			    gpointer data)
{
	EDayViewEvent *event;

#if 1
	g_print ("In e_day_view_remove_event_cb day:%i event_num:%i\n",
		 day, event_num);
#endif

	if (day == E_DAY_VIEW_LONG_EVENT)
		event = &g_array_index (day_view->long_events,
					EDayViewEvent, event_num);
	else
		event = &g_array_index (day_view->events[day],
					EDayViewEvent, event_num);

	/* If we were editing this event, set editing_event_num to -1 so
	   on_editing_stopped doesn't try to update the event. */
	if (day_view->editing_event_day == day
	    && day_view->editing_event_num == event_num)
		day_view->editing_event_day = -1;

	if (event->canvas_item)
		gtk_object_destroy (GTK_OBJECT (event->canvas_item));
	gtk_object_unref (GTK_OBJECT (event->comp));

	if (day == E_DAY_VIEW_LONG_EVENT) {
		g_array_remove_index (day_view->long_events, event_num);
		day_view->long_events_need_layout = TRUE;
	} else {
		g_array_remove_index (day_view->events[day], event_num);
		day_view->need_layout[day] = TRUE;
	}
	return TRUE;
}


/* This updates the text shown for an event. If the event start or end do not
   lie on a row boundary, the time is displayed before the summary. */
static void
e_day_view_update_event_label (EDayView *day_view,
			       gint day,
			       gint event_num)
{
	EDayViewEvent *event;
	char *text;
	gboolean free_text = FALSE, editing_event = FALSE;
	gint offset, start_minute, end_minute;
	CalComponentText summary;
	
	event = &g_array_index (day_view->events[day], EDayViewEvent,
				event_num);

	/* If the event isn't visible just return. */
	if (!event->canvas_item)
		return;

	cal_component_get_summary (event->comp, &summary);
	text = summary.value ? (char *) summary.value : "";

	if (day_view->editing_event_day == day
	    && day_view->editing_event_num == event_num)
		editing_event = TRUE;

	if (!editing_event
	    && (event->start_minute % day_view->mins_per_row != 0
		|| event->end_minute % day_view->mins_per_row != 0)) {
		offset = day_view->first_hour_shown * 60
			+ day_view->first_minute_shown;
		start_minute = offset + event->start_minute;
		end_minute = offset + event->end_minute;
		text = g_strdup_printf ("%02i:%02i-%02i:%02i %s",
					start_minute / 60,
					start_minute % 60,
					end_minute / 60,
					end_minute % 60,
					text);

		free_text = TRUE;
	}

	gnome_canvas_item_set (event->canvas_item,
			       "text", text,
			       NULL);

	if (free_text)
		g_free (text);
}


static void
e_day_view_update_long_event_label (EDayView *day_view,
				    gint      event_num)
{
	EDayViewEvent *event;
	CalComponentText summary;
	
	event = &g_array_index (day_view->long_events, EDayViewEvent,
				event_num);

	/* If the event isn't visible just return. */
	if (!event->canvas_item)
		return;

	cal_component_get_summary (event->comp, &summary);
	gnome_canvas_item_set (event->canvas_item,
			       "text", summary.value ? summary.value : "",
			       NULL);
}


/* Finds the day and index of the event with the given canvas item.
   If is is a long event, -1 is returned as the day.
   Returns TRUE if the event was found. */
static gboolean
e_day_view_find_event_from_item (EDayView *day_view,
				 GnomeCanvasItem *item,
				 gint *day_return,
				 gint *event_num_return)
{
	EDayViewEvent *event;
	gint day, event_num;

	for (day = 0; day < day_view->days_shown; day++) {
		for (event_num = 0; event_num < day_view->events[day]->len;
		     event_num++) {
			event = &g_array_index (day_view->events[day],
						EDayViewEvent, event_num);
			if (event->canvas_item == item) {
				*day_return = day;
				*event_num_return = event_num;
				return TRUE;
			}
		}
	}

	for (event_num = 0; event_num < day_view->long_events->len;
	     event_num++) {
		event = &g_array_index (day_view->long_events,
					EDayViewEvent, event_num);
		if (event->canvas_item == item) {
			*day_return = E_DAY_VIEW_LONG_EVENT;
			*event_num_return = event_num;
			return TRUE;
		}
	}

	return FALSE;
}


/* Finds the day and index of the event with the given uid.
   If is is a long event, E_DAY_VIEW_LONG_EVENT is returned as the day.
   Returns TRUE if an event with the uid was found.
   Note that for recurring events there may be several EDayViewEvents, one
   for each instance, all with the same iCalObject and uid. So only use this
   function if you know the event doesn't recur or you are just checking to
   see if any events with the uid exist. */
static gboolean
e_day_view_find_event_from_uid (EDayView *day_view,
				const gchar *uid,
				gint *day_return,
				gint *event_num_return)
{
	EDayViewEvent *event;
	gint day, event_num;
	const char *u;
	
	for (day = 0; day < day_view->days_shown; day++) {
		for (event_num = 0; event_num < day_view->events[day]->len;
		     event_num++) {
			event = &g_array_index (day_view->events[day],
						EDayViewEvent, event_num);

			cal_component_get_uid (event->comp, &u);
			if (u && !strcmp (uid, u)) {
				*day_return = day;
				*event_num_return = event_num;
				return TRUE;
			}
		}
	}

	for (event_num = 0; event_num < day_view->long_events->len;
	     event_num++) {
		event = &g_array_index (day_view->long_events,
					EDayViewEvent, event_num);

		cal_component_get_uid (event->comp, &u);
		if (u && !strcmp (uid, u)) {
			*day_return = E_DAY_VIEW_LONG_EVENT;
			*event_num_return = event_num;
			return TRUE;
		}
	}

	return FALSE;
}


/* This sets the selected time range. The EDayView will show the day or week
   corresponding to the start time. If the start_time & end_time are not equal
   and are both visible in the view, then the selection is set to those times,
   otherwise it is set to 1 hour from the start of the working day. */
void
e_day_view_set_selected_time_range	(EDayView	*day_view,
					 time_t		 start_time,
					 time_t		 end_time)
{
	GDate date;
	time_t lower;
	gint start_row, start_col, end_row, end_col;
	gboolean need_redraw = FALSE, start_in_grid, end_in_grid;

	g_return_if_fail (E_IS_DAY_VIEW (day_view));

	/* Calculate the first day that should be shown, based on start_time
	   and the days_shown setting. If we are showing 1 day it is just the
	   start of the day given by start_time, otherwise it is the previous
	   Monday. */
	if (day_view->days_shown == 1)
		lower = time_day_begin (start_time);
	else {
		g_date_clear (&date, 1);
		g_date_set_time (&date, start_time);
		g_date_subtract_days (&date, g_date_weekday (&date) - 1);
		lower = time_from_day (g_date_year (&date),
				       g_date_month (&date) - 1,
				       g_date_day (&date));
	}

	/* See if we need to change the days shown. */
	if (lower != day_view->lower) {
		e_day_view_recalc_day_starts (day_view, lower);
		e_day_view_reload_events (day_view);
		need_redraw = TRUE;
	}

	/* Set the selection. */
	start_in_grid = e_day_view_convert_time_to_grid_position (day_view,
								  start_time,
								  &start_col,
								  &start_row);
	end_in_grid = e_day_view_convert_time_to_grid_position (day_view,
								end_time - 60,
								&end_col,
								&end_row);

	/* If either of the times isn't in the grid, or the selection covers
	   an entire day, we set the selection to 1 row from the start of the
	   working day, in the day corresponding to the start time. */
	if (!start_in_grid || !end_in_grid
	    || (start_row == 0 && end_row == day_view->rows - 1)) {
		end_col = start_col;

		start_row = e_day_view_convert_time_to_row (day_view, day_view->work_day_start_hour, day_view->work_day_start_minute);
		start_row = CLAMP (start_row, 0, day_view->rows - 1);
		end_row = start_row;
	}

	if (start_row != day_view->selection_start_row
	    || start_col != day_view->selection_start_day) {
		need_redraw = TRUE;
		day_view->selection_in_top_canvas = FALSE;
		day_view->selection_start_row = start_row;
		day_view->selection_start_day = start_col;
	}

	if (end_row != day_view->selection_end_row
	    || end_col != day_view->selection_end_day) {
		need_redraw = TRUE;
		day_view->selection_in_top_canvas = FALSE;
		day_view->selection_end_row = end_row;
		day_view->selection_end_day = end_col;
	}

	if (need_redraw) {
		gtk_widget_queue_draw (day_view->top_canvas);
		gtk_widget_queue_draw (day_view->main_canvas);
	}
}


/* Returns the selected time range. */
void
e_day_view_get_selected_time_range	(EDayView	*day_view,
					 time_t		*start_time,
					 time_t		*end_time)
{
	gint start_col, start_row, end_col, end_row;
	time_t start, end;

	start_col = day_view->selection_start_day;
	start_row = day_view->selection_start_row;
	end_col = day_view->selection_end_day;
	end_row = day_view->selection_end_row;

	if (start_col == -1) {
		start_col = 0;
		start_row = 0;
		end_col = 0;
		end_row = 0;
	}

	/* Check if the selection is only in the top canvas, in which case
	   we can simply use the day_starts array. */
	if (day_view->selection_in_top_canvas) {
		start = day_view->day_starts[start_col];
		end = day_view->day_starts[end_col + 1];
	} else {
		/* Convert the start col + row into a time. */
		start = e_day_view_convert_grid_position_to_time (day_view, start_col, start_row);
		end = e_day_view_convert_grid_position_to_time (day_view, end_col, end_row + 1);
	}

	if (start_time)
		*start_time = start;

	if (end_time)
		*end_time = end;
}


static void
e_day_view_recalc_day_starts (EDayView *day_view,
			      time_t start_time)
{
	gint day;

	day_view->day_starts[0] = start_time;
	for (day = 1; day <= day_view->days_shown; day++) {
		day_view->day_starts[day] = time_add_day (day_view->day_starts[day - 1], 1);
	}

	day_view->lower = start_time;
	day_view->upper = day_view->day_starts[day_view->days_shown];
}


gint
e_day_view_get_days_shown	(EDayView	*day_view)
{
	g_return_val_if_fail (E_IS_DAY_VIEW (day_view), -1);

	return day_view->days_shown;
}


void
e_day_view_set_days_shown	(EDayView	*day_view,
				 gint		 days_shown)
{
	g_return_if_fail (E_IS_DAY_VIEW (day_view));
	g_return_if_fail (days_shown >= 1);
	g_return_if_fail (days_shown <= E_DAY_VIEW_MAX_DAYS);

	if (day_view->days_shown != days_shown) {
		day_view->days_shown = days_shown;

		/* FIXME: Update everything. */
	}
}


gint
e_day_view_get_mins_per_row	(EDayView	*day_view)
{
	g_return_val_if_fail (E_IS_DAY_VIEW (day_view), -1);

	return day_view->mins_per_row;
}


void
e_day_view_set_mins_per_row	(EDayView	*day_view,
				 gint		 mins_per_row)
{
	gint day;

	g_return_if_fail (E_IS_DAY_VIEW (day_view));

	if (mins_per_row != 5 && mins_per_row != 10 && mins_per_row != 15
	    && mins_per_row != 30 && mins_per_row != 60) {
		g_warning ("Invalid minutes per row setting");
		return;
	}

	if (day_view->mins_per_row == mins_per_row)
		return;

	day_view->mins_per_row = mins_per_row;
	e_day_view_recalc_num_rows (day_view);

	/* If we aren't visible, we'll sort it out later. */
	if (!GTK_WIDGET_VISIBLE (day_view))
	    return;

	for (day = 0; day < E_DAY_VIEW_MAX_DAYS; day++)
		day_view->need_layout[day] = TRUE;

	/* We must layout the events before updating the scroll region, since
	   that will result in a redraw which would crash otherwise. */
	e_day_view_check_layout (day_view);
	gtk_widget_queue_draw (day_view->time_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);

	e_day_view_update_scroll_regions (day_view);
}


static gboolean
e_day_view_update_scroll_regions (EDayView *day_view)
{
	gdouble old_x2, old_y2, new_x2, new_y2;
	gboolean need_reshape = FALSE;

	/* Set the scroll region of the time canvas to its allocated width,
	   but with the height the same as the main canvas. */
	gnome_canvas_get_scroll_region (GNOME_CANVAS (day_view->time_canvas),
					NULL, NULL, &old_x2, &old_y2);
	new_x2 = day_view->time_canvas->allocation.width - 1;
	new_y2 = MAX (day_view->rows * day_view->row_height,
		      day_view->main_canvas->allocation.height) - 1;
	if (old_x2 != new_x2 || old_y2 != new_y2)
		gnome_canvas_set_scroll_region (GNOME_CANVAS (day_view->time_canvas),
						0, 0, new_x2, new_y2);

	/* Set the scroll region of the main canvas to its allocated width,
	   but with the height depending on the number of rows needed. */
	gnome_canvas_get_scroll_region (GNOME_CANVAS (day_view->main_canvas),
					NULL, NULL, &old_x2, &old_y2);
	new_x2 = day_view->main_canvas->allocation.width - 1;
	if (old_x2 != new_x2 || old_y2 != new_y2) {
		need_reshape = TRUE;
		gnome_canvas_set_scroll_region (GNOME_CANVAS (day_view->main_canvas),
						0, 0, new_x2, new_y2);
	}

	return need_reshape;
}


/* This recalculates the number of rows to display, based on the time range
   shown and the minutes per row. */
static void
e_day_view_recalc_num_rows	(EDayView	*day_view)
{
	gint hours, minutes, total_minutes;

	hours = day_view->last_hour_shown - day_view->first_hour_shown;
	/* This could be negative but it works out OK. */
	minutes = day_view->last_minute_shown - day_view->first_minute_shown;
	total_minutes = hours * 60 + minutes;
	day_view->rows = total_minutes / day_view->mins_per_row;
}


/* Converts an hour and minute to a row in the canvas. Note that if we aren't
   showing all 24 hours of the day, the returned row may be negative or
   greater than day_view->rows. */
gint
e_day_view_convert_time_to_row	(EDayView	*day_view,
				 gint		 hour,
				 gint		 minute)
{
	gint total_minutes, start_minute, offset;

	total_minutes = hour * 60 + minute;
	start_minute = day_view->first_hour_shown * 60
		+ day_view->first_minute_shown;
	offset = total_minutes - start_minute;
	if (offset < 0)
		return -1;
	else
		return offset / day_view->mins_per_row;
}


/* Converts an hour and minute to a y coordinate in the canvas. */
gint
e_day_view_convert_time_to_position (EDayView	*day_view,
				     gint	 hour,
				     gint	 minute)
{
	gint total_minutes, start_minute, offset;

	total_minutes = hour * 60 + minute;
	start_minute = day_view->first_hour_shown * 60
		+ day_view->first_minute_shown;
	offset = total_minutes - start_minute;

	return offset * day_view->row_height / day_view->mins_per_row;
}


static gboolean
e_day_view_on_top_canvas_button_press (GtkWidget *widget,
				       GdkEventButton *event,
				       EDayView *day_view)
{
	gint event_x, event_y, scroll_x, scroll_y, day, event_num;
	EDayViewPosition pos;

	/* Convert the coords to the main canvas window, or return if the
	   window is not found. */
	if (!e_day_view_convert_event_coords (day_view, (GdkEvent*) event,
					      GTK_LAYOUT (widget)->bin_window,
					      &event_x, &event_y))
		return FALSE;

	/* The top canvas doesn't scroll, but just in case. */
	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (widget),
					 &scroll_x, &scroll_y);
	event_x += scroll_x;
	event_y += scroll_y;

	pos = e_day_view_convert_position_in_top_canvas (day_view,
							 event_x, event_y,
							 &day, &event_num);

	if (pos == E_DAY_VIEW_POS_OUTSIDE)
		return FALSE;

	if (pos != E_DAY_VIEW_POS_NONE)
		return e_day_view_on_long_event_button_press (day_view,
							      event_num,
							      event, pos,
							      event_x,
							      event_y);

	e_day_view_stop_editing_event (day_view);

	if (event->button == 1) {
		if (!GTK_WIDGET_HAS_FOCUS (day_view))
			gtk_widget_grab_focus (GTK_WIDGET (day_view));

		if (gdk_pointer_grab (GTK_LAYOUT (widget)->bin_window, FALSE,
				      GDK_POINTER_MOTION_MASK
				      | GDK_BUTTON_RELEASE_MASK,
				      FALSE, NULL, event->time) == 0) {
			e_day_view_start_selection (day_view, day, -1);
		}
	} else if (event->button == 3) {
		if (!GTK_WIDGET_HAS_FOCUS (day_view))
			gtk_widget_grab_focus (GTK_WIDGET (day_view));

		e_day_view_on_event_right_click (day_view, event, -1, -1);
	}

	return TRUE;
}


static gboolean
e_day_view_convert_event_coords (EDayView *day_view,
				 GdkEvent *event,
				 GdkWindow *window,
				 gint *x_return,
				 gint *y_return)
{
	gint event_x, event_y, win_x, win_y;
	GdkWindow *event_window;;

	/* Get the event window, x & y from the appropriate event struct. */
	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
		event_x = event->button.x;
		event_y = event->button.y;
		event_window = event->button.window;
		break;
	case GDK_MOTION_NOTIFY:
		event_x = event->motion.x;
		event_y = event->motion.y;
		event_window = event->motion.window;
		break;
	default:
		/* Shouldn't get here. */
		g_assert_not_reached ();
		return FALSE;
	}

	while (event_window && event_window != window
	       && event_window != GDK_ROOT_PARENT()) {
		gdk_window_get_position (event_window, &win_x, &win_y);
		event_x += win_x;
		event_y += win_y;
		event_window = gdk_window_get_parent (event_window);
	}

	*x_return = event_x;
	*y_return = event_y;

	if (event_window != window)
		g_warning ("Couldn't find event window\n");

	return (event_window == window) ? TRUE : FALSE;
}


static gboolean
e_day_view_on_main_canvas_button_press (GtkWidget *widget,
					GdkEventButton *event,
					EDayView *day_view)
{
	gint event_x, event_y, scroll_x, scroll_y, row, day, event_num;
	EDayViewPosition pos;

	/* Handle scroll wheel events */
	if (event->button == 4 || event->button == 5) {
		GtkAdjustment *adj = GTK_LAYOUT (day_view->main_canvas)->vadjustment;
		gfloat new_value;

		new_value = adj->value + ((event->button == 4) ?
					  -adj->page_increment / 2:
					  adj->page_increment / 2);
		new_value = CLAMP (new_value, adj->lower, adj->upper - adj->page_size);
		gtk_adjustment_set_value (adj, new_value);

		return TRUE;
	}
	
	/* Convert the coords to the main canvas window, or return if the
	   window is not found. */
	if (!e_day_view_convert_event_coords (day_view, (GdkEvent*) event,
					      GTK_LAYOUT (widget)->bin_window,
					      &event_x, &event_y))
		return FALSE;

	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (widget),
					 &scroll_x, &scroll_y);
	event_x += scroll_x;
	event_y += scroll_y;

	/* Find out where the mouse is. */
	pos = e_day_view_convert_position_in_main_canvas (day_view,
							  event_x, event_y,
							  &day, &row,
							  &event_num);

	if (pos == E_DAY_VIEW_POS_OUTSIDE)
		return FALSE;

	if (pos != E_DAY_VIEW_POS_NONE)
		return e_day_view_on_event_button_press (day_view, day,
							 event_num, event, pos,
							 event_x, event_y);

	e_day_view_stop_editing_event (day_view);

	/* Start the selection drag. */
	if (event->button == 1) {
		if (!GTK_WIDGET_HAS_FOCUS (day_view))
			gtk_widget_grab_focus (GTK_WIDGET (day_view));

		if (gdk_pointer_grab (GTK_LAYOUT (widget)->bin_window, FALSE,
				      GDK_POINTER_MOTION_MASK
				      | GDK_BUTTON_RELEASE_MASK,
				      FALSE, NULL, event->time) == 0) {
			e_day_view_start_selection (day_view, day, row);
		}
	} else if (event->button == 3) {
		if (!GTK_WIDGET_HAS_FOCUS (day_view))
			gtk_widget_grab_focus (GTK_WIDGET (day_view));

		e_day_view_on_event_right_click (day_view, event, -1, -1);
	}

	return TRUE;
}


static gboolean
e_day_view_on_time_canvas_button_press (GtkWidget      *widget,
					GdkEventButton *event,
					EDayView       *day_view)
{
	/* Handle scroll wheel events */
	if (event->button == 4 || event->button == 5) {
		GtkAdjustment *adj = GTK_LAYOUT (day_view->main_canvas)->vadjustment;
		gfloat new_value;
		
		new_value = adj->value + ((event->button == 4) ?
					  -adj->page_increment / 2:
					  adj->page_increment / 2);
		new_value = CLAMP (new_value, adj->lower, adj->upper - adj->page_size);
		gtk_adjustment_set_value (adj, new_value);
		
		return TRUE;
	}
	
	return FALSE;
}


static gboolean
e_day_view_on_long_event_button_press (EDayView		*day_view,
				       gint		 event_num,
				       GdkEventButton	*event,
				       EDayViewPosition  pos,
				       gint		 event_x,
				       gint		 event_y)
{
	if (event->button == 1) {
		if (event->type == GDK_BUTTON_PRESS) {
			e_day_view_on_long_event_click (day_view, event_num,
							event, pos,
							event_x, event_y);
			return TRUE;
		} else if (event->type == GDK_2BUTTON_PRESS) {
			e_day_view_on_event_double_click (day_view, -1,
							  event_num);
			return TRUE;
		}
	} else if (event->button == 3) {
		if (!GTK_WIDGET_HAS_FOCUS (day_view))
			gtk_widget_grab_focus (GTK_WIDGET (day_view));

		e_day_view_on_event_right_click (day_view, event,
						 E_DAY_VIEW_LONG_EVENT,
						 event_num);
		return TRUE;
	}
	return FALSE;
}


static gboolean
e_day_view_on_event_button_press (EDayView	  *day_view,
				  gint		   day,
				  gint		   event_num,
				  GdkEventButton  *event,
				  EDayViewPosition pos,
				  gint		   event_x,
				  gint		   event_y)
{
	if (event->button == 1) {
		if (event->type == GDK_BUTTON_PRESS) {
			e_day_view_on_event_click (day_view, day, event_num,
						   event, pos,
						   event_x, event_y);
			return TRUE;
		} else if (event->type == GDK_2BUTTON_PRESS) {
			e_day_view_on_event_double_click (day_view, day,
							  event_num);
			return TRUE;
		}
	} else if (event->button == 3) {
		if (!GTK_WIDGET_HAS_FOCUS (day_view))
			gtk_widget_grab_focus (GTK_WIDGET (day_view));

		e_day_view_on_event_right_click (day_view, event,
						 day, event_num);
		return TRUE;
	}
	return FALSE;
}


static void
e_day_view_on_long_event_click (EDayView *day_view,
				gint event_num,
				GdkEventButton  *bevent,
				EDayViewPosition pos,
				gint	     event_x,
				gint	     event_y)
{
	EDayViewEvent *event;
	gint start_day, end_day, day;
	gint item_x, item_y, item_w, item_h;

	event = &g_array_index (day_view->long_events, EDayViewEvent,
				event_num);

	/* Ignore clicks on the EText while editing. */
	if (pos == E_DAY_VIEW_POS_EVENT
	    && E_TEXT (event->canvas_item)->editing)
		return;

	if (!(cal_component_has_rrules (event->comp)
	      && cal_component_has_rdates (event->comp))
	    && (pos == E_DAY_VIEW_POS_LEFT_EDGE
		|| pos == E_DAY_VIEW_POS_RIGHT_EDGE)) {
		if (!e_day_view_find_long_event_days (day_view, event,
						      &start_day, &end_day))
			return;

		/* Grab the keyboard focus, so the event being edited is saved
		   and we can use the Escape key to abort the resize. */
		if (!GTK_WIDGET_HAS_FOCUS (day_view))
			gtk_widget_grab_focus (GTK_WIDGET (day_view));

		if (gdk_pointer_grab (GTK_LAYOUT (day_view->top_canvas)->bin_window, FALSE,
				      GDK_POINTER_MOTION_MASK
				      | GDK_BUTTON_RELEASE_MASK,
				      FALSE, NULL, bevent->time) == 0) {

			day_view->resize_event_day = E_DAY_VIEW_LONG_EVENT;
			day_view->resize_event_num = event_num;
			day_view->resize_drag_pos = pos;
			day_view->resize_start_row = start_day;
			day_view->resize_end_row = end_day;

			/* Create the edit rect if necessary. */
			e_day_view_reshape_resize_long_event_rect_item (day_view);

			/* Make sure the text item is on top. */
			gnome_canvas_item_raise_to_top (day_view->resize_long_event_rect_item);

			/* Raise the event's item, above the rect as well. */
			gnome_canvas_item_raise_to_top (event->canvas_item);
		}
	} else if (e_day_view_get_long_event_position (day_view, event_num,
						       &start_day, &end_day,
						       &item_x, &item_y,
						       &item_w, &item_h)) {
		/* Remember the item clicked and the mouse position,
		   so we can start a drag if the mouse moves. */
		day_view->pressed_event_day = E_DAY_VIEW_LONG_EVENT;
		day_view->pressed_event_num = event_num;

		day_view->drag_event_x = event_x;
		day_view->drag_event_y = event_y;

		e_day_view_convert_position_in_top_canvas (day_view,
							   event_x, event_y,
							   &day, NULL);
		day_view->drag_event_offset = day - start_day;
	}
}


static void
e_day_view_on_event_click (EDayView *day_view,
			   gint day,
			   gint event_num,
			   GdkEventButton  *bevent,
			   EDayViewPosition pos,
			   gint		  event_x,
			   gint		  event_y)
{
	EDayViewEvent *event;
	gint tmp_day, row, start_row;

	event = &g_array_index (day_view->events[day], EDayViewEvent,
				event_num);

	/* Ignore clicks on the EText while editing. */
	if (pos == E_DAY_VIEW_POS_EVENT
	    && E_TEXT (event->canvas_item)->editing)
		return;

	if (!(cal_component_has_rrules (event->comp)
	      && cal_component_has_rdates (event->comp))
	    && (pos == E_DAY_VIEW_POS_TOP_EDGE
		|| pos == E_DAY_VIEW_POS_BOTTOM_EDGE)) {
		/* Grab the keyboard focus, so the event being edited is saved
		   and we can use the Escape key to abort the resize. */
		if (!GTK_WIDGET_HAS_FOCUS (day_view))
			gtk_widget_grab_focus (GTK_WIDGET (day_view));

		if (gdk_pointer_grab (GTK_LAYOUT (day_view->main_canvas)->bin_window, FALSE,
				      GDK_POINTER_MOTION_MASK
				      | GDK_BUTTON_RELEASE_MASK,
				      FALSE, NULL, bevent->time) == 0) {

			day_view->resize_event_day = day;
			day_view->resize_event_num = event_num;
			day_view->resize_drag_pos = pos;
			day_view->resize_start_row = event->start_minute / day_view->mins_per_row;
			day_view->resize_end_row = (event->end_minute - 1) / day_view->mins_per_row;

			day_view->resize_bars_event_day = day;
			day_view->resize_bars_event_num = event_num;

			/* Create the edit rect if necessary. */
			e_day_view_reshape_resize_rect_item (day_view);

			e_day_view_reshape_main_canvas_resize_bars (day_view);

			/* Make sure the text item is on top. */
			gnome_canvas_item_raise_to_top (day_view->resize_rect_item);
			gnome_canvas_item_raise_to_top (day_view->resize_bar_item);

			/* Raise the event's item, above the rect as well. */
			gnome_canvas_item_raise_to_top (event->canvas_item);
		}

	} else {
		/* Remember the item clicked and the mouse position,
		   so we can start a drag if the mouse moves. */
		day_view->pressed_event_day = day;
		day_view->pressed_event_num = event_num;

		day_view->drag_event_x = event_x;
		day_view->drag_event_y = event_y;

		e_day_view_convert_position_in_main_canvas (day_view,
							    event_x, event_y,
							    &tmp_day, &row,
							    NULL);
		start_row = event->start_minute / day_view->mins_per_row;
		day_view->drag_event_offset = row - start_row;
	}
}


static void
e_day_view_reshape_resize_long_event_rect_item (EDayView *day_view)
{
	gint day, event_num, start_day, end_day;
	gint item_x, item_y, item_w, item_h;
	gdouble x1, y1, x2, y2;

	day = day_view->resize_event_day;
	event_num = day_view->resize_event_num;

	/* If we're not resizing an event, or the event is not shown,
	   hide the resize bars. */
	if (day_view->resize_drag_pos == E_DAY_VIEW_POS_NONE
	    || !e_day_view_get_long_event_position (day_view, event_num,
						    &start_day, &end_day,
						    &item_x, &item_y,
						    &item_w, &item_h)) {
		gnome_canvas_item_hide (day_view->resize_long_event_rect_item);
		return;
	}

	x1 = item_x;
	y1 = item_y;
	x2 = item_x + item_w - 1;
	y2 = item_y + item_h - 1;

	gnome_canvas_item_set (day_view->resize_long_event_rect_item,
			       "x1", x1,
			       "y1", y1,
			       "x2", x2,
			       "y2", y2,
			       NULL);
	gnome_canvas_item_show (day_view->resize_long_event_rect_item);
}


static void
e_day_view_reshape_resize_rect_item (EDayView *day_view)
{
	gint day, event_num;
	gint item_x, item_y, item_w, item_h;
	gdouble x1, y1, x2, y2;

	day = day_view->resize_event_day;
	event_num = day_view->resize_event_num;

	/* If we're not resizing an event, or the event is not shown,
	   hide the resize bars. */
	if (day_view->resize_drag_pos == E_DAY_VIEW_POS_NONE
	    || !e_day_view_get_event_position (day_view, day, event_num,
					       &item_x, &item_y,
					       &item_w, &item_h)) {
		gnome_canvas_item_hide (day_view->resize_rect_item);
		return;
	}

	x1 = item_x;
	y1 = item_y;
	x2 = item_x + item_w - 1;
	y2 = item_y + item_h - 1;

	gnome_canvas_item_set (day_view->resize_rect_item,
			       "x1", x1 + E_DAY_VIEW_BAR_WIDTH - 1,
			       "y1", y1,
			       "x2", x2,
			       "y2", y2,
			       NULL);
	gnome_canvas_item_show (day_view->resize_rect_item);

	gnome_canvas_item_set (day_view->resize_bar_item,
			       "x1", x1,
			       "y1", y1,
			       "x2", x1 + E_DAY_VIEW_BAR_WIDTH - 1,
			       "y2", y2,
			       NULL);
	gnome_canvas_item_show (day_view->resize_bar_item);
}


static void
e_day_view_on_event_double_click (EDayView *day_view,
				  gint day,
				  gint event_num)
{
#if 0
	g_print ("In e_day_view_on_event_double_click\n");
#endif

}


static void
e_day_view_on_event_right_click (EDayView *day_view,
				 GdkEventButton *bevent,
				 gint day,
				 gint event_num)
{
	EDayViewEvent *event;
	int have_selection, not_being_edited, items, i;
	struct menu_item *context_menu;

	static struct menu_item main_items[] = {
		{ N_("New appointment..."), (GtkSignalFunc) e_day_view_on_new_appointment, NULL, TRUE }
	};

	static struct menu_item child_items[] = {
		{ N_("Edit this appointment..."), (GtkSignalFunc) e_day_view_on_edit_appointment, NULL, TRUE },
		{ N_("Delete this appointment"), (GtkSignalFunc) e_day_view_on_delete_appointment, NULL, TRUE },
		{ NULL, NULL, NULL, TRUE },
		{ N_("New appointment..."), (GtkSignalFunc) e_day_view_on_new_appointment, NULL, TRUE }
	};

	static struct menu_item recur_child_items[] = {
		{ N_("Edit this appointment..."), (GtkSignalFunc) e_day_view_on_edit_appointment, NULL, TRUE },
		{ N_("Make this appointment movable"), (GtkSignalFunc) e_day_view_on_unrecur_appointment, NULL, TRUE },
		{ N_("Delete this occurrence"), (GtkSignalFunc) e_day_view_on_delete_occurrence, NULL, TRUE },
		{ N_("Delete all occurrences"), (GtkSignalFunc) e_day_view_on_delete_appointment, NULL, TRUE },
		{ NULL, NULL, NULL, TRUE },
		{ N_("New appointment..."), (GtkSignalFunc) e_day_view_on_new_appointment, NULL, TRUE }
	};

	have_selection = GTK_WIDGET_HAS_FOCUS (day_view)
		&& day_view->selection_start_day != -1;

	if (event_num == -1) {
		items = 1;
		context_menu = &main_items[0];
		context_menu[0].sensitive = have_selection;
	} else {
		if (day == E_DAY_VIEW_LONG_EVENT)
			event = &g_array_index (day_view->long_events,
						EDayViewEvent, event_num);
		else
			event = &g_array_index (day_view->events[day],
						EDayViewEvent, event_num);

		/* This used to be set only if the event wasn't being edited
		   in the event editor, but we can't check that at present.
		   We could possibly set up another method of checking it. */
		not_being_edited = TRUE;

		if (cal_component_has_rrules (event->comp)
		    || cal_component_has_rdates (event->comp)) {
			items = 6;
			context_menu = &recur_child_items[0];
			context_menu[0].sensitive = not_being_edited;
			context_menu[1].sensitive = not_being_edited;
			context_menu[2].sensitive = not_being_edited;
			context_menu[3].sensitive = not_being_edited;
			context_menu[5].sensitive = have_selection;
		} else {
			items = 4;
			context_menu = &child_items[0];
			context_menu[0].sensitive = not_being_edited;
			context_menu[1].sensitive = not_being_edited;
			context_menu[3].sensitive = have_selection;
		}
	}

	for (i = 0; i < items; i++)
		context_menu[i].data = day_view;

	day_view->popup_event_day = day;
	day_view->popup_event_num = event_num;
	popup_menu (context_menu, items, bevent);
}


static void
e_day_view_on_new_appointment (GtkWidget *widget, gpointer data)
{
	EDayView *day_view;
	CalComponent *comp;
	CalComponentDateTime date;
	time_t dtstart, dtend;
	struct icaltimetype itt;
	
	day_view = E_DAY_VIEW (data);

	comp = cal_component_new ();
	cal_component_set_new_vtype (comp, CAL_COMPONENT_EVENT);
	e_day_view_get_selected_time_range (day_view, &dtstart, &dtend);
	
	date.value = &itt;
	date.tzid = NULL;

	*date.value = icaltimetype_from_timet (dtstart, FALSE);
	cal_component_set_dtstart (comp, &date);

	*date.value = icaltimetype_from_timet (dtend, FALSE);
	cal_component_set_dtend (comp, &date);

	cal_component_commit_sequence (comp);

	gnome_calendar_edit_object (day_view->calendar, comp);
	gtk_object_unref (GTK_OBJECT (comp));
}


static void
e_day_view_on_edit_appointment (GtkWidget *widget, gpointer data)
{
	EDayView *day_view;
	EDayViewEvent *event;

	day_view = E_DAY_VIEW (data);

	event = e_day_view_get_popup_menu_event (day_view);
	if (event == NULL)
		return;

	gnome_calendar_edit_object (day_view->calendar, event->comp);
}


static void
e_day_view_on_delete_occurrence (GtkWidget *widget, gpointer data)
{
	EDayView *day_view;
	EDayViewEvent *event;
	CalComponent *comp;
	CalComponentDateTime *date=NULL;
	GSList *list;

	day_view = E_DAY_VIEW (data);

	event = e_day_view_get_popup_menu_event (day_view);
	if (event == NULL)
		return;

	/* We must duplicate the CalComponent, or we won't know it has changed
	   when we get the "update_event" callback. */
	comp = cal_component_clone (event->comp);
	cal_component_get_exdate_list (comp, &list);
	list = g_slist_append (list, date);
	date = g_new0 (CalComponentDateTime, 1);
	date->value = g_new (struct icaltimetype, 1);
	*date->value = icaltimetype_from_timet (event->start, TRUE);
	cal_component_set_exdate_list (comp, list);
	cal_component_free_exdate_list (list);

	if (!cal_client_update_object (day_view->calendar->client, comp))
		g_message ("e_day_view_on_delete_occurrence(): Could not update the object!");

	gtk_object_unref (GTK_OBJECT (comp));
}


static void
e_day_view_on_delete_appointment (GtkWidget *widget, gpointer data)
{
	EDayView *day_view;
	EDayViewEvent *event;
	const char *uid;
	
	day_view = E_DAY_VIEW (data);

	event = e_day_view_get_popup_menu_event (day_view);
	if (event == NULL)
		return;

	if (day_view->editing_event_day >= 0)
		e_day_view_stop_editing_event (day_view);

	cal_component_get_uid (event->comp, &uid);
	if (!cal_client_remove_object (day_view->calendar->client, uid))
		g_message ("e_day_view_on_delete_appointment(): Could not remove the object!");
}


static void
e_day_view_on_unrecur_appointment (GtkWidget *widget, gpointer data)
{
	EDayView *day_view;
	EDayViewEvent *event;
	CalComponent *comp, *new_comp;
	CalComponentDateTime *date;
	GSList *list;

	day_view = E_DAY_VIEW (data);

	event = e_day_view_get_popup_menu_event (day_view);
	if (event == NULL)
		return;

	/* For the recurring object, we add a exception to get rid of the
	   instance. */
	comp = cal_component_clone (event->comp);
	cal_component_get_exdate_list (comp, &list);
	date = g_new0 (CalComponentDateTime, 1);
	date->value = g_new (struct icaltimetype, 1);
	*date->value = icaltimetype_from_timet (event->start, TRUE);
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

	*date->value = icaltimetype_from_timet (event->start, FALSE);
	cal_component_set_dtstart (new_comp, date);
	*date->value = icaltimetype_from_timet (event->end, FALSE);
	cal_component_set_dtend (new_comp, date);

	cal_component_free_datetime (date);
	
	/* Now update both CalComponents. Note that we do this last since at
	 * present the updates happen synchronously so our event may disappear.
	 */
	if (!cal_client_update_object (day_view->calendar->client, comp))
		g_message ("e_day_view_on_unrecur_appointment(): Could not update the object!");

	gtk_object_unref (GTK_OBJECT (comp));

	if (!cal_client_update_object (day_view->calendar->client, new_comp))
		g_message ("e_day_view_on_unrecur_appointment(): Could not update the object!");

	gtk_object_unref (GTK_OBJECT (new_comp));
}


static EDayViewEvent*
e_day_view_get_popup_menu_event (EDayView *day_view)
{
	if (day_view->popup_event_num == -1)
		return NULL;

	if (day_view->popup_event_day == E_DAY_VIEW_LONG_EVENT)
		return &g_array_index (day_view->long_events,
				       EDayViewEvent,
				       day_view->popup_event_num);
	else
		return &g_array_index (day_view->events[day_view->popup_event_day],
				       EDayViewEvent,
				       day_view->popup_event_num);
}


static gboolean
e_day_view_on_top_canvas_button_release (GtkWidget *widget,
					 GdkEventButton *event,
					 EDayView *day_view)
{
	if (day_view->selection_is_being_dragged) {
		gdk_pointer_ungrab (event->time);
		e_day_view_finish_selection (day_view);
	} else if (day_view->resize_drag_pos != E_DAY_VIEW_POS_NONE) {
		e_day_view_finish_long_event_resize (day_view);
		gdk_pointer_ungrab (event->time);
	} else if (day_view->pressed_event_day != -1) {
		e_day_view_start_editing_event (day_view,
						day_view->pressed_event_day,
						day_view->pressed_event_num,
						NULL);
	}

	day_view->pressed_event_day = -1;

	return FALSE;
}


static gboolean
e_day_view_on_main_canvas_button_release (GtkWidget *widget,
					  GdkEventButton *event,
					  EDayView *day_view)
{
	if (day_view->selection_is_being_dragged) {
		gdk_pointer_ungrab (event->time);
		e_day_view_finish_selection (day_view);
		e_day_view_stop_auto_scroll (day_view);
	} else if (day_view->resize_drag_pos != E_DAY_VIEW_POS_NONE) {
		e_day_view_finish_resize (day_view);
		gdk_pointer_ungrab (event->time);
		e_day_view_stop_auto_scroll (day_view);
	} else if (day_view->pressed_event_day != -1) {
		e_day_view_start_editing_event (day_view,
						day_view->pressed_event_day,
						day_view->pressed_event_num,
						NULL);
	}

	day_view->pressed_event_day = -1;

	return FALSE;
}


static void
e_day_view_update_calendar_selection_time (EDayView *day_view)
{
	time_t start, end;

	e_day_view_get_selected_time_range (day_view, &start, &end);
	gnome_calendar_set_selected_time_range (day_view->calendar,
						start, end);
}


static gboolean
e_day_view_on_top_canvas_motion (GtkWidget *widget,
				 GdkEventMotion *mevent,
				 EDayView *day_view)
{
	EDayViewEvent *event = NULL;
	EDayViewPosition pos;
	gint event_x, event_y, scroll_x, scroll_y, canvas_x, canvas_y;
	gint day, event_num;
	GdkCursor *cursor;

#if 0
	g_print ("In e_day_view_on_top_canvas_motion\n");
#endif

	/* Convert the coords to the main canvas window, or return if the
	   window is not found. */
	if (!e_day_view_convert_event_coords (day_view, (GdkEvent*) mevent,
					      GTK_LAYOUT (widget)->bin_window,
					      &event_x, &event_y))
		return FALSE;

	/* The top canvas doesn't scroll, but just in case. */
	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (widget),
					 &scroll_x, &scroll_y);
	canvas_x = event_x + scroll_x;
	canvas_y = event_y + scroll_y;

	pos = e_day_view_convert_position_in_top_canvas (day_view,
							 canvas_x, canvas_y,
							 &day, &event_num);
	if (event_num != -1)
		event = &g_array_index (day_view->long_events, EDayViewEvent,
					event_num);

	if (day_view->selection_is_being_dragged) {
		e_day_view_update_selection (day_view, day, -1);
		return TRUE;
	} else if (day_view->resize_drag_pos != E_DAY_VIEW_POS_NONE) {
		if (pos != E_DAY_VIEW_POS_OUTSIDE) {
			e_day_view_update_long_event_resize (day_view, day);
			return TRUE;
		}
	} else if (day_view->pressed_event_day == E_DAY_VIEW_LONG_EVENT) {
		GtkTargetList *target_list;

		event = &g_array_index (day_view->long_events, EDayViewEvent,
					day_view->pressed_event_num);

		if (!(cal_component_has_rdates (event->comp)
		      && cal_component_has_rrules (event->comp))
		    && (abs (canvas_x - day_view->drag_event_x) > E_DAY_VIEW_DRAG_START_OFFSET
			|| abs (canvas_y - day_view->drag_event_y) > E_DAY_VIEW_DRAG_START_OFFSET)) {
			day_view->drag_event_day = day_view->pressed_event_day;
			day_view->drag_event_num = day_view->pressed_event_num;
			day_view->pressed_event_day = -1;

			/* Hide the horizontal bars. */
			if (day_view->resize_bars_event_day != -1) {
				day_view->resize_bars_event_day = -1;
				day_view->resize_bars_event_num = -1;
				gnome_canvas_item_hide (day_view->main_canvas_top_resize_bar_item);
				gnome_canvas_item_hide (day_view->main_canvas_bottom_resize_bar_item);
			}

			target_list = gtk_target_list_new (target_table,
							   n_targets);
			gtk_drag_begin (widget, target_list,
					GDK_ACTION_COPY | GDK_ACTION_MOVE,
					1, (GdkEvent*)mevent);
			gtk_target_list_unref (target_list);
		}
	} else {
		cursor = day_view->normal_cursor;

		/* Recurring events can't be resized. */
		if (event && 
		    !(cal_component_has_rrules (event->comp) 
		      && cal_component_has_rdates (event->comp))) {
			switch (pos) {
			case E_DAY_VIEW_POS_LEFT_EDGE:
			case E_DAY_VIEW_POS_RIGHT_EDGE:
				cursor = day_view->resize_width_cursor;
				break;
			default:
				break;
			}
		}

		/* Only set the cursor if it is different to last one set. */
		if (day_view->last_cursor_set_in_top_canvas != cursor) {
			day_view->last_cursor_set_in_top_canvas = cursor;
			gdk_window_set_cursor (widget->window, cursor);
		}

	}

	return FALSE;
}


static gboolean
e_day_view_on_main_canvas_motion (GtkWidget *widget,
				  GdkEventMotion *mevent,
				  EDayView *day_view)
{
	EDayViewEvent *event = NULL;
	EDayViewPosition pos;
	gint event_x, event_y, scroll_x, scroll_y, canvas_x, canvas_y;
	gint row, day, event_num;
	GdkCursor *cursor;

#if 0
	g_print ("In e_day_view_on_main_canvas_motion\n");
#endif

	/* Convert the coords to the main canvas window, or return if the
	   window is not found. */
	if (!e_day_view_convert_event_coords (day_view, (GdkEvent*) mevent,
					      GTK_LAYOUT (widget)->bin_window,
					      &event_x, &event_y))
		return FALSE;

	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (widget),
					 &scroll_x, &scroll_y);
	canvas_x = event_x + scroll_x;
	canvas_y = event_y + scroll_y;

	pos = e_day_view_convert_position_in_main_canvas (day_view,
							  canvas_x, canvas_y,
							  &day, &row,
							  &event_num);
	if (event_num != -1)
		event = &g_array_index (day_view->events[day], EDayViewEvent,
					event_num);

	if (day_view->selection_is_being_dragged) {
		if (pos != E_DAY_VIEW_POS_OUTSIDE) {
			e_day_view_update_selection (day_view, day, row);
			e_day_view_check_auto_scroll (day_view,
						      event_x, event_y);
			return TRUE;
		}
	} else if (day_view->resize_drag_pos != E_DAY_VIEW_POS_NONE) {
		if (pos != E_DAY_VIEW_POS_OUTSIDE) {
			e_day_view_update_resize (day_view, row);
			e_day_view_check_auto_scroll (day_view,
						      event_x, event_y);
			return TRUE;
		}
	} else if (day_view->pressed_event_day != -1
		   && day_view->pressed_event_day != E_DAY_VIEW_LONG_EVENT) {
		GtkTargetList *target_list;

		event = &g_array_index (day_view->events[day_view->pressed_event_day], EDayViewEvent, day_view->pressed_event_num);

		if (!(cal_component_has_rrules (event->comp)
		      && cal_component_has_rdates (event->comp))
		    && (abs (canvas_x - day_view->drag_event_x) > E_DAY_VIEW_DRAG_START_OFFSET
			|| abs (canvas_y - day_view->drag_event_y) > E_DAY_VIEW_DRAG_START_OFFSET)) {
			day_view->drag_event_day = day_view->pressed_event_day;
			day_view->drag_event_num = day_view->pressed_event_num;
			day_view->pressed_event_day = -1;

			/* Hide the horizontal bars. */
			if (day_view->resize_bars_event_day != -1) {
				day_view->resize_bars_event_day = -1;
				day_view->resize_bars_event_num = -1;
				gnome_canvas_item_hide (day_view->main_canvas_top_resize_bar_item);
				gnome_canvas_item_hide (day_view->main_canvas_bottom_resize_bar_item);
			}

			target_list = gtk_target_list_new (target_table,
							   n_targets);
			gtk_drag_begin (widget, target_list,
					GDK_ACTION_COPY | GDK_ACTION_MOVE,
					1, (GdkEvent*)mevent);
			gtk_target_list_unref (target_list);
		}
	} else {
		cursor = day_view->normal_cursor;

		/* Recurring events can't be resized. */
		if (event && 
		    !(cal_component_has_rrules (event->comp)
		      && cal_component_has_rdates (event->comp))) {
			switch (pos) {
			case E_DAY_VIEW_POS_LEFT_EDGE:
				cursor = day_view->move_cursor;
				break;
			case E_DAY_VIEW_POS_TOP_EDGE:
			case E_DAY_VIEW_POS_BOTTOM_EDGE:
				cursor = day_view->resize_height_cursor;
				break;
			default:
				break;
			}
		}

		/* Only set the cursor if it is different to last one set. */
		if (day_view->last_cursor_set_in_main_canvas != cursor) {
			day_view->last_cursor_set_in_main_canvas = cursor;
			gdk_window_set_cursor (widget->window, cursor);
		}
	}

	return FALSE;
}


/* This sets the selection to a single cell. If day is -1 then the current
   start day is reused. If row is -1 then the selection is in the top canvas.
*/
void
e_day_view_start_selection (EDayView *day_view,
			    gint day,
			    gint row)
{
	if (day == -1) {
		day = day_view->selection_start_day;
		if (day == -1)
			day = 0;
	}

	day_view->selection_start_day = day;
	day_view->selection_end_day = day;

	day_view->selection_start_row = row;
	day_view->selection_end_row = row;

	day_view->selection_is_being_dragged = TRUE;
	day_view->selection_drag_pos = E_DAY_VIEW_DRAG_END;
	day_view->selection_in_top_canvas = (row == -1) ? TRUE : FALSE;

	/* FIXME: Optimise? */
	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);
}


/* Updates the selection during a drag. If day is -1 the selection day is
   unchanged. */
void
e_day_view_update_selection (EDayView *day_view,
			     gint day,
			     gint row)
{
	gboolean need_redraw = FALSE;

#if 0
	g_print ("Updating selection %i,%i\n", day, row);
#endif

	day_view->selection_in_top_canvas = (row == -1) ? TRUE : FALSE;

	if (day == -1)
		day = (day_view->selection_drag_pos == E_DAY_VIEW_DRAG_START)
			? day_view->selection_start_day
			: day_view->selection_end_day;

	if (day_view->selection_drag_pos == E_DAY_VIEW_DRAG_START) {
		if (row != day_view->selection_start_row
		    || day != day_view->selection_start_day) {
			need_redraw = TRUE;
			day_view->selection_start_row = row;
			day_view->selection_start_day = day;
		}
	} else {
		if (row != day_view->selection_end_row
		    || day != day_view->selection_end_day) {
			need_redraw = TRUE;
			day_view->selection_end_row = row;
			day_view->selection_end_day = day;
		}
	}

	e_day_view_normalize_selection (day_view);

	/* FIXME: Optimise? */
	if (need_redraw) {
		gtk_widget_queue_draw (day_view->top_canvas);
		gtk_widget_queue_draw (day_view->main_canvas);
	}
}


static void
e_day_view_normalize_selection (EDayView *day_view)
{
	gint tmp_row, tmp_day;

	/* Switch the drag position if necessary. */
	if (day_view->selection_start_day > day_view->selection_end_day
	    || (day_view->selection_start_day == day_view->selection_end_day
		&& day_view->selection_start_row > day_view->selection_end_row)) {
		tmp_row = day_view->selection_start_row;
		tmp_day = day_view->selection_start_day;
		day_view->selection_start_day = day_view->selection_end_day;
		day_view->selection_start_row = day_view->selection_end_row;
		day_view->selection_end_day = tmp_day;
		day_view->selection_end_row = tmp_row;
		if (day_view->selection_drag_pos == E_DAY_VIEW_DRAG_START)
			day_view->selection_drag_pos = E_DAY_VIEW_DRAG_END;
		else
			day_view->selection_drag_pos = E_DAY_VIEW_DRAG_START;
	}
}


void
e_day_view_finish_selection (EDayView *day_view)
{
	day_view->selection_is_being_dragged = FALSE;
	e_day_view_update_calendar_selection_time (day_view);
}


static void
e_day_view_update_long_event_resize (EDayView *day_view,
				     gint day)
{
	EDayViewEvent *event;
	gint event_num;
	gboolean need_reshape = FALSE;

#if 0
	g_print ("Updating resize Day:%i\n", day);
#endif

	event_num = day_view->resize_event_num;
	event = &g_array_index (day_view->long_events, EDayViewEvent,
				event_num);

	if (day_view->resize_drag_pos == E_DAY_VIEW_POS_LEFT_EDGE) {
		day = MIN (day, day_view->resize_end_row);
		if (day != day_view->resize_start_row) {
			need_reshape = TRUE;
			day_view->resize_start_row = day;

		}
	} else {
		day = MAX (day, day_view->resize_start_row);
		if (day != day_view->resize_end_row) {
			need_reshape = TRUE;
			day_view->resize_end_row = day;
		}
	}

	/* FIXME: Optimise? */
	if (need_reshape) {
		e_day_view_reshape_long_event (day_view, event_num);
		e_day_view_reshape_resize_long_event_rect_item (day_view);
		gtk_widget_queue_draw (day_view->top_canvas);
	}
}


static void
e_day_view_update_resize (EDayView *day_view,
			  gint row)
{
	EDayViewEvent *event;
	gint day, event_num;
	gboolean need_reshape = FALSE;

#if 0
	g_print ("Updating resize Row:%i\n", row);
#endif

	day = day_view->resize_event_day;
	event_num = day_view->resize_event_num;
	event = &g_array_index (day_view->events[day], EDayViewEvent,
				event_num);

	if (day_view->resize_drag_pos == E_DAY_VIEW_POS_TOP_EDGE) {
		row = MIN (row, day_view->resize_end_row);
		if (row != day_view->resize_start_row) {
			need_reshape = TRUE;
			day_view->resize_start_row = row;

		}
	} else {
		row = MAX (row, day_view->resize_start_row);
		if (row != day_view->resize_end_row) {
			need_reshape = TRUE;
			day_view->resize_end_row = row;
		}
	}

	/* FIXME: Optimise? */
	if (need_reshape) {
		e_day_view_reshape_day_event (day_view, day, event_num);
		e_day_view_reshape_resize_rect_item (day_view);
		e_day_view_reshape_main_canvas_resize_bars (day_view);
		gtk_widget_queue_draw (day_view->main_canvas);
	}
}


/* This converts the resize start or end row back to a time and updates the
   event. */
static void
e_day_view_finish_long_event_resize (EDayView *day_view)
{
	EDayViewEvent *event;
	gint event_num;
	CalComponent *comp;
	CalComponentDateTime date;
	time_t dt;
	
	event_num = day_view->resize_event_num;
	event = &g_array_index (day_view->long_events, EDayViewEvent,
				event_num);

	/* We use a temporary shallow copy of the ico since we don't want to
	   change the original ico here. Otherwise we would not detect that
	   the event's time had changed in the "update_event" callback. */
	comp = cal_component_clone (event->comp);

	date.value = g_new (struct icaltimetype, 1);
	if (day_view->resize_drag_pos == E_DAY_VIEW_POS_LEFT_EDGE) {
		dt = day_view->day_starts[day_view->resize_start_row];
		*date.value = icaltimetype_from_timet (dt, FALSE);
		cal_component_set_dtstart (comp, &date);
	} else {
		dt = day_view->day_starts[day_view->resize_end_row + 1];
		*date.value = icaltimetype_from_timet (dt, FALSE);
		cal_component_set_dtend (comp, &date);
	}
	g_free (date.value);
	
	gnome_canvas_item_hide (day_view->resize_long_event_rect_item);

	day_view->resize_drag_pos = E_DAY_VIEW_POS_NONE;

	if (!cal_client_update_object (day_view->calendar->client, comp))
		g_message ("e_day_view_finish_long_event_resize(): Could not update the object!");

	gtk_object_unref (GTK_OBJECT (comp));
}


/* This converts the resize start or end row back to a time and updates the
   event. */
static void
e_day_view_finish_resize (EDayView *day_view)
{
	EDayViewEvent *event;
	gint day, event_num;
	CalComponent *comp;
	CalComponentDateTime date;
	time_t dt;

	day = day_view->resize_event_day;
	event_num = day_view->resize_event_num;
	event = &g_array_index (day_view->events[day], EDayViewEvent,
				event_num);

	/* We use a temporary shallow copy of the ico since we don't want to
	   change the original ico here. Otherwise we would not detect that
	   the event's time had changed in the "update_event" callback. */
	comp = cal_component_clone (event->comp);

	date.value = g_new (struct icaltimetype, 1);
	if (day_view->resize_drag_pos == E_DAY_VIEW_POS_TOP_EDGE) {
		dt = e_day_view_convert_grid_position_to_time (day_view, day, day_view->resize_start_row);
		*date.value = icaltimetype_from_timet (dt, FALSE);
		cal_component_set_dtstart (comp, &date);
	} else {
		dt = e_day_view_convert_grid_position_to_time (day_view, day, day_view->resize_end_row + 1);
		*date.value = icaltimetype_from_timet (dt, FALSE);
		cal_component_set_dtend (comp, &date);
	}
	g_free (date.value);
	
	gnome_canvas_item_hide (day_view->resize_rect_item);
	gnome_canvas_item_hide (day_view->resize_bar_item);

	/* Hide the horizontal bars. */
	day_view->resize_bars_event_day = -1;
	day_view->resize_bars_event_num = -1;
	gnome_canvas_item_hide (day_view->main_canvas_top_resize_bar_item);
	gnome_canvas_item_hide (day_view->main_canvas_bottom_resize_bar_item);

	day_view->resize_drag_pos = E_DAY_VIEW_POS_NONE;

	if (!cal_client_update_object (day_view->calendar->client, comp))
		g_message ("e_day_view_finish_resize(): Could not update the object!");

	gtk_object_unref (GTK_OBJECT (comp));
}


static void
e_day_view_abort_resize (EDayView *day_view,
			 guint32 time)
{
	gint day, event_num;

	if (day_view->resize_drag_pos == E_DAY_VIEW_POS_NONE)
		return;

	day_view->resize_drag_pos = E_DAY_VIEW_POS_NONE;
	gdk_pointer_ungrab (time);

	day = day_view->resize_event_day;
	event_num = day_view->resize_event_num;

	if (day == E_DAY_VIEW_LONG_EVENT) {
		e_day_view_reshape_long_event (day_view, event_num);
		gtk_widget_queue_draw (day_view->top_canvas);

		day_view->last_cursor_set_in_top_canvas = day_view->normal_cursor;
		gdk_window_set_cursor (day_view->top_canvas->window,
				       day_view->normal_cursor);
		gnome_canvas_item_hide (day_view->resize_long_event_rect_item);
	} else {
		e_day_view_reshape_day_event (day_view, day, event_num);
		e_day_view_reshape_main_canvas_resize_bars (day_view);
		gtk_widget_queue_draw (day_view->main_canvas);

		day_view->last_cursor_set_in_main_canvas = day_view->normal_cursor;
		gdk_window_set_cursor (day_view->main_canvas->window,
				       day_view->normal_cursor);
		gnome_canvas_item_hide (day_view->resize_rect_item);
		gnome_canvas_item_hide (day_view->resize_bar_item);
	}
}


static void
e_day_view_reload_events (EDayView *day_view)
{
	e_day_view_free_events (day_view);

	/* Reset all our indices. */
	day_view->editing_event_day = -1;
	day_view->popup_event_day = -1;
	day_view->resize_bars_event_day = -1;
	day_view->resize_event_day = -1;
	day_view->pressed_event_day = -1;
	day_view->drag_event_day = -1;

	/* If both lower & upper are 0, then the time range hasn't been set,
	   so we don't try to load any events. */
	if (day_view->calendar
	    && (day_view->lower != 0 || day_view->upper != 0)) {
		cal_client_generate_instances (day_view->calendar->client,
					       CALOBJ_TYPE_EVENT,
					       day_view->lower,
					       day_view->upper,
					       e_day_view_add_event,
					       day_view);   
	}

	/* We need to do this to make sure the top canvas is resized. */
	day_view->long_events_need_layout = TRUE;

	e_day_view_check_layout (day_view);
	e_day_view_reshape_main_canvas_resize_bars (day_view);

	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);
}


static void
e_day_view_free_events (EDayView *day_view)
{
	gint day;

	e_day_view_free_event_array (day_view, day_view->long_events);

	for (day = 0; day < E_DAY_VIEW_MAX_DAYS; day++)
		e_day_view_free_event_array (day_view, day_view->events[day]);
}


static void
e_day_view_free_event_array (EDayView *day_view,
			     GArray *array)
{
	EDayViewEvent *event;
	gint event_num;

	for (event_num = 0; event_num < array->len; event_num++) {
		event = &g_array_index (array, EDayViewEvent, event_num);
		if (event->canvas_item)
			gtk_object_destroy (GTK_OBJECT (event->canvas_item));
		gtk_object_unref (GTK_OBJECT (event->comp));
	}

	g_array_set_size (array, 0);
}


/* This adds one event to the view, adding it to the appropriate array. */
static gboolean
e_day_view_add_event (CalComponent *comp,
		      time_t	  start,
		      time_t	  end,
		      gpointer	  data)

{
	EDayView *day_view;
	EDayViewEvent event;
	gint day, offset;
	struct tm start_tm, end_tm;

	day_view = E_DAY_VIEW (data);

	/* Check that the event times are valid. */
	g_return_val_if_fail (start <= end, TRUE);
	g_return_val_if_fail (start < day_view->upper, TRUE);
	g_return_val_if_fail (end > day_view->lower, TRUE);

	start_tm = *(localtime (&start));
	end_tm = *(localtime (&end));

	event.comp = comp;
	gtk_object_ref (GTK_OBJECT (comp));
	event.start = start;
	event.end = end;
	event.canvas_item = NULL;

	/* Calculate the start & end minute, relative to the top of the
	   display. */
	offset = day_view->first_hour_shown * 60
		+ day_view->first_minute_shown;
	event.start_minute = start_tm.tm_hour * 60 + start_tm.tm_min - offset;
	event.end_minute = end_tm.tm_hour * 60 + end_tm.tm_min - offset;

	event.start_row_or_col = -1;
	event.num_columns = -1;

	/* Find out which array to add the event to. */
	for (day = 0; day < day_view->days_shown; day++) {
		if (start >= day_view->day_starts[day]
		    && end <= day_view->day_starts[day + 1]) {

			/* Special case for when the appointment ends at
			   midnight, i.e. the start of the next day. */
			if (end == day_view->day_starts[day + 1]) {

				/* If the event last the entire day, then we
				   skip it here so it gets added to the top
				   canvas. */
				if (start == day_view->day_starts[day])
				    break;

				event.end_minute = 24 * 60;
			}

			g_array_append_val (day_view->events[day], event);
			day_view->events_sorted[day] = FALSE;
			day_view->need_layout[day] = TRUE;
			return TRUE;
		}
	}

	/* The event wasn't within one day so it must be a long event,
	   i.e. shown in the top canvas. */
	g_array_append_val (day_view->long_events, event);
	day_view->long_events_sorted = FALSE;
	day_view->long_events_need_layout = TRUE;
	return TRUE;
}


/* This lays out the short (less than 1 day) events in the columns.
   Any long events are simply skipped. */
void
e_day_view_check_layout (EDayView *day_view)
{
	gint day;

	/* Don't bother if we aren't visible. */
	if (!GTK_WIDGET_VISIBLE (day_view))
	    return;

	/* Make sure the events are sorted (by start and size). */
	e_day_view_ensure_events_sorted (day_view);

	for (day = 0; day < day_view->days_shown; day++) {
		if (day_view->need_layout[day])
			e_day_view_layout_day_events (day_view, day);

		if (day_view->need_layout[day]
		    || day_view->need_reshape[day]) {
			e_day_view_reshape_day_events (day_view, day);

			if (day_view->resize_bars_event_day == day)
				e_day_view_reshape_main_canvas_resize_bars (day_view);
		}

		day_view->need_layout[day] = FALSE;
		day_view->need_reshape[day] = FALSE;
	}

	if (day_view->long_events_need_layout)
		e_day_view_layout_long_events (day_view);

	if (day_view->long_events_need_layout
	    || day_view->long_events_need_reshape)
		e_day_view_reshape_long_events (day_view);

	day_view->long_events_need_layout = FALSE;
	day_view->long_events_need_reshape = FALSE;
}


static void
e_day_view_layout_long_events (EDayView *day_view)
{
	EDayViewEvent *event;
	gint event_num, old_rows_in_top_display, top_canvas_height, top_rows;
	guint8 *grid;

	/* This is a temporary 2-d grid which is used to place events.
	   Each element is 0 if the position is empty, or 1 if occupied.
	   We allocate the maximum size possible here, assuming that each
	   event will need its own row. */
	grid = g_new0 (guint8,
		       day_view->long_events->len * E_DAY_VIEW_MAX_DAYS);

	/* Reset the number of rows in the top display to 0. It will be
	   updated as events are layed out below. */
	old_rows_in_top_display = day_view->rows_in_top_display;
	day_view->rows_in_top_display = 0;

	/* Iterate over the events, finding which days they cover, and putting
	   them in the first free row available. */
	for (event_num = 0; event_num < day_view->long_events->len;
	     event_num++) {
		event = &g_array_index (day_view->long_events,
					EDayViewEvent, event_num);
		e_day_view_layout_long_event (day_view, event, grid);
	}

	/* Free the grid. */
	g_free (grid);

	/* Set the height of the top canvas based on the row height and the
	   number of rows needed (min 1 + 1 for the dates + 1 space for DnD).*/
	if (day_view->rows_in_top_display != old_rows_in_top_display) {
		top_rows = MAX (1, day_view->rows_in_top_display);
		top_canvas_height = (top_rows + 2) * day_view->top_row_height;
		gtk_widget_set_usize (day_view->top_canvas, -1,
				      top_canvas_height);
	}
}


static void
e_day_view_layout_long_event (EDayView	   *day_view,
			      EDayViewEvent *event,
			      guint8	   *grid)
{
	gint start_day, end_day, free_row, day, row;

	event->num_columns = 0;

	if (!e_day_view_find_long_event_days (day_view, event,
					      &start_day, &end_day))
		return;

	/* Try each row until we find a free one. */
	row = 0;
	do {
		free_row = row;
		for (day = start_day; day <= end_day; day++) {
			if (grid[row * E_DAY_VIEW_MAX_DAYS + day]) {
				free_row = -1;
				break;
			}
		}
		row++;
	} while (free_row == -1);

	event->start_row_or_col = free_row;
	event->num_columns = 1;

	/* Mark the cells as full. */
	for (day = start_day; day <= end_day; day++) {
		grid[free_row * E_DAY_VIEW_MAX_DAYS + day] = 1;
	}

	/* Update the number of rows in the top canvas if necessary. */
	day_view->rows_in_top_display = MAX (day_view->rows_in_top_display,
					     free_row + 1);
}


static void
e_day_view_reshape_long_events (EDayView *day_view)
{
	EDayViewEvent *event;
	gint event_num;

	for (event_num = 0; event_num < day_view->long_events->len;
	     event_num++) {
		event = &g_array_index (day_view->long_events, EDayViewEvent,
					event_num);

		if (event->num_columns == 0) {
			if (event->canvas_item) {
				gtk_object_destroy (GTK_OBJECT (event->canvas_item));
				event->canvas_item = NULL;
			}
		} else {
			e_day_view_reshape_long_event (day_view, event_num);
		}
	}
}


static void
e_day_view_reshape_long_event (EDayView *day_view,
			       gint	 event_num)
{
	EDayViewEvent *event;
	GdkFont *font;
	gint start_day, end_day, item_x, item_y, item_w, item_h;
	gint text_x, text_w, num_icons, icons_width, width, time_width;
	CalComponent *comp;
	gint min_text_x, max_text_w, text_width, line_len;
	gchar *text, *end_of_line;
	gboolean show_icons = TRUE, use_max_width = FALSE;

	event = &g_array_index (day_view->long_events, EDayViewEvent,
				event_num);

	if (!e_day_view_get_long_event_position (day_view, event_num,
						 &start_day, &end_day,
						 &item_x, &item_y,
						 &item_w, &item_h)) {
		if (event->canvas_item) {
			gtk_object_destroy (GTK_OBJECT (event->canvas_item));
			event->canvas_item = NULL;
		}
		return;
	}

	/* Take off the border and padding. */
	item_x += E_DAY_VIEW_LONG_EVENT_BORDER_WIDTH + E_DAY_VIEW_LONG_EVENT_X_PAD;
	item_w -= (E_DAY_VIEW_LONG_EVENT_BORDER_WIDTH + E_DAY_VIEW_LONG_EVENT_X_PAD) * 2;
	item_y += E_DAY_VIEW_LONG_EVENT_BORDER_HEIGHT + E_DAY_VIEW_LONG_EVENT_Y_PAD;
	item_h -= (E_DAY_VIEW_LONG_EVENT_BORDER_HEIGHT + E_DAY_VIEW_LONG_EVENT_Y_PAD) * 2;

	/* We don't show the icons while resizing, since we'd have to
	   draw them on top of the resize rect. Nor when editing. */
	num_icons = 0;
	comp = event->comp;
	font = GTK_WIDGET (day_view)->style->font;

	if (day_view->resize_drag_pos != E_DAY_VIEW_POS_NONE
	    && day_view->resize_event_day == E_DAY_VIEW_LONG_EVENT
	    && day_view->resize_event_num == event_num)
		show_icons = FALSE;

	if (day_view->editing_event_day == E_DAY_VIEW_LONG_EVENT
	    && day_view->editing_event_num == event_num) {
		show_icons = FALSE;
		use_max_width = TRUE;
	}

#if 0
	if (show_icons) {
		if (ico->dalarm.enabled || ico->malarm.enabled
		    || ico->palarm.enabled || ico->aalarm.enabled)
			num_icons++;
		if (ico->recur)
			num_icons++;
	}
#endif

	if (!event->canvas_item) {
		event->canvas_item =
			gnome_canvas_item_new (GNOME_CANVAS_GROUP (GNOME_CANVAS (day_view->top_canvas)->root),
					       e_text_get_type (),
					       "font_gdk", GTK_WIDGET (day_view)->style->font,
					       "anchor", GTK_ANCHOR_NW,
					       "clip", TRUE,
					       "max_lines", 1,
					       "editable", TRUE,
					       "use_ellipsis", TRUE,
					       NULL);
		gtk_signal_connect (GTK_OBJECT (event->canvas_item), "event",
				    GTK_SIGNAL_FUNC (e_day_view_on_text_item_event),
				    day_view);
		e_day_view_update_long_event_label (day_view, event_num);
	}

	/* Calculate its position. We first calculate the ideal position which
	   is centered with the icons. We then make sure we haven't gone off
	   the left edge of the available space. Finally we make sure we don't
	   go off the right edge. */
	icons_width = (E_DAY_VIEW_ICON_WIDTH + E_DAY_VIEW_ICON_X_PAD)
		* num_icons;
	time_width = day_view->max_small_hour_width + day_view->colon_width
		+ day_view->max_minute_width;

	if (use_max_width) {
		text_x = item_x;
		text_w = item_w;
	} else {
		/* Get the requested size of the label. */
		gtk_object_get (GTK_OBJECT (event->canvas_item),
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

		width = text_width + icons_width;
		text_x = item_x + (item_w - width) / 2;

		min_text_x = item_x;
		if (event->start > day_view->day_starts[start_day])
			min_text_x += time_width + E_DAY_VIEW_LONG_EVENT_TIME_X_PAD;

		text_x = MAX (text_x, min_text_x);

		max_text_w = item_x + item_w - text_x;
		if (event->end < day_view->day_starts[end_day + 1])
			max_text_w -= time_width + E_DAY_VIEW_LONG_EVENT_TIME_X_PAD;

		text_w = MIN (width, max_text_w);

		/* Now take out the space for the icons. */
		text_x += icons_width;
		text_w -= icons_width;
	}

	text_w = MAX (text_w, 0);
	gnome_canvas_item_set (event->canvas_item,
			       "clip_width", (gdouble) text_w,
			       "clip_height", (gdouble) item_h,
			       NULL);
	e_canvas_item_move_absolute(event->canvas_item,
				    text_x, item_y);
}


/* Find the start and end days for the event. */
gboolean
e_day_view_find_long_event_days (EDayView	*day_view,
				 EDayViewEvent	*event,
				 gint		*start_day_return,
				 gint		*end_day_return)
{
	gint day, start_day, end_day;

	start_day = -1;
	end_day = -1;

	for (day = 0; day < day_view->days_shown; day++) {
		if (start_day == -1
		    && event->start < day_view->day_starts[day + 1])
			start_day = day;
		if (event->end > day_view->day_starts[day])
			end_day = day;
	}

	/* Sanity check. */
	if (start_day < 0 || start_day >= day_view->days_shown
	    || end_day < 0 || end_day >= day_view->days_shown
	    || end_day < start_day) {
		g_warning ("Invalid date range for event");
		return FALSE;
	}

	*start_day_return = start_day;
	*end_day_return = end_day;

	return TRUE;
}


static void
e_day_view_layout_day_events (EDayView *day_view,
			      gint	day)
{
	EDayViewEvent *event;
	gint row, event_num;
	guint8 *grid;

	/* This is a temporary array which keeps track of rows which are
	   connected. When an appointment spans multiple rows then the number
	   of columns in each of these rows must be the same (i.e. the maximum
	   of all of them). Each element in the array corresponds to one row
	   and contains the index of the first row in the group of connected
	   rows. */
	guint16 group_starts[12 * 24];

	/* Reset the cols_per_row array, and initialize the connected rows. */
	for (row = 0; row < day_view->rows; row++) {
		day_view->cols_per_row[day][row] = 0;
		group_starts[row] = row;
	}

	/* This is a temporary 2-d grid which is used to place events.
	   Each element is 0 if the position is empty, or 1 if occupied. */
	grid = g_new0 (guint8, day_view->rows * E_DAY_VIEW_MAX_COLUMNS);


	/* Iterate over the events, finding which rows they cover, and putting
	   them in the first free column available. Increment the number of
	   events in each of the rows it covers, and make sure they are all
	   in one group. */
	for (event_num = 0; event_num < day_view->events[day]->len;
	     event_num++) {
		event = &g_array_index (day_view->events[day], EDayViewEvent,
					event_num);

		e_day_view_layout_day_event (day_view, day, event,
					     grid, group_starts);
	}

	/* Recalculate the number of columns needed in each row. */
	e_day_view_recalc_cols_per_row (day_view, day, group_starts);

	/* Iterate over the events again, trying to expand events horizontally
	   if there is enough space. */
	for (event_num = 0; event_num < day_view->events[day]->len;
	     event_num++) {
		event = &g_array_index (day_view->events[day], EDayViewEvent,
					event_num);
		e_day_view_expand_day_event (day_view, day, event, grid);
	}

	/* Free the grid. */
	g_free (grid);
}


/* Finds the first free position to place the event in.
   Increments the number of events in each of the rows it covers, and makes
   sure they are all in one group. */
static void
e_day_view_layout_day_event (EDayView	   *day_view,
			     gint	    day,
			     EDayViewEvent *event,
			     guint8	   *grid,
			     guint16	   *group_starts)
{
	gint start_row, end_row, free_col, col, row, group_start;

	start_row = event->start_minute / day_view->mins_per_row;
	end_row = (event->end_minute - 1) / day_view->mins_per_row;

	event->num_columns = 0;

	/* If the event can't currently be seen, just return. */
	if (start_row >= day_view->rows || end_row < 0)
		return;

	/* Make sure we don't go outside the visible times. */
	start_row = CLAMP (start_row, 0, day_view->rows - 1);
	end_row = CLAMP (end_row, 0, day_view->rows - 1);

	/* Try each column until we find a free one. */
	for (col = 0; col < E_DAY_VIEW_MAX_COLUMNS; col++) {
		free_col = col;
		for (row = start_row; row <= end_row; row++) {
			if (grid[row * E_DAY_VIEW_MAX_COLUMNS + col]) {
				free_col = -1;
				break;
			}
		}

		if (free_col != -1)
			break;
	}

	/* If we can't find space for the event, just return. */
	if (free_col == -1)
		return;

	/* The event is assigned 1 col initially, but may be expanded later. */
	event->start_row_or_col = free_col;
	event->num_columns = 1;

	/* Determine the start index of the group. */
	group_start = group_starts[start_row];

	/* Increment number of events in each of the rows the event covers.
	   We use the cols_per_row array for this. It will be sorted out after
	   all the events have been layed out. Also make sure all the rows that
	   the event covers are in one group. */
	for (row = start_row; row <= end_row; row++) {
		grid[row * E_DAY_VIEW_MAX_COLUMNS + free_col] = 1;
		day_view->cols_per_row[day][row]++;
		group_starts[row] = group_start;
	}

	/* If any following rows should be in the same group, add them. */
	for (row = end_row + 1; row < day_view->rows; row++) {
		if (group_starts[row] > end_row)
			break;
		group_starts[row] = group_start;
	}
}


/* For each group of rows, find the max number of events in all the
   rows, and set the number of cols in each of the rows to that. */
static void
e_day_view_recalc_cols_per_row (EDayView *day_view,
				gint	  day,
				guint16  *group_starts)
{
	gint start_row = 0, row, next_start_row, max_events;

	while (start_row < day_view->rows) {

		max_events = 0;
		for (row = start_row; row < day_view->rows && group_starts[row] == start_row; row++)
			max_events = MAX (max_events, day_view->cols_per_row[day][row]);

		next_start_row = row;

		for (row = start_row; row < next_start_row; row++)
			day_view->cols_per_row[day][row] = max_events;

		start_row = next_start_row;
	}
}


/* Expands the event horizontally to fill any free space. */
static void
e_day_view_expand_day_event (EDayView	   *day_view,
			     gint	    day,
			     EDayViewEvent *event,
			     guint8	   *grid)
{
	gint start_row, end_row, col, row;
	gboolean clashed;

	start_row = event->start_minute / day_view->mins_per_row;
	end_row = (event->end_minute - 1) / day_view->mins_per_row;

	/* Try each column until we find a free one. */
	clashed = FALSE;
	for (col = event->start_row_or_col + 1; col < day_view->cols_per_row[day][start_row]; col++) {
		for (row = start_row; row <= end_row; row++) {
			if (grid[row * E_DAY_VIEW_MAX_COLUMNS + col]) {
				clashed = TRUE;
				break;
			}
		}

		if (clashed)
			break;

		event->num_columns++;
	}
}


/* This creates or updates the sizes of the canvas items for one day of the
   main canvas. */
static void
e_day_view_reshape_day_events (EDayView *day_view,
			       gint	 day)
{
	gint event_num;

	for (event_num = 0; event_num < day_view->events[day]->len;
	     event_num++) {
		e_day_view_reshape_day_event (day_view, day, event_num);
	}
}


static void
e_day_view_reshape_day_event (EDayView *day_view,
			      gint	day,
			      gint	event_num)
{
	EDayViewEvent *event;
	gint item_x, item_y, item_w, item_h;
	gint num_icons, icons_offset;
	CalComponent *comp;

	event = &g_array_index (day_view->events[day], EDayViewEvent,
				event_num);
	comp = event->comp;

	if (!e_day_view_get_event_position (day_view, day, event_num,
					    &item_x, &item_y,
					    &item_w, &item_h)) {
		if (event->canvas_item) {
			gtk_object_destroy (GTK_OBJECT (event->canvas_item));
			event->canvas_item = NULL;
		}
	} else {
		/* Skip the border and padding. */
		item_x += E_DAY_VIEW_BAR_WIDTH + E_DAY_VIEW_EVENT_X_PAD;
		item_w -= E_DAY_VIEW_BAR_WIDTH + E_DAY_VIEW_EVENT_X_PAD * 2;
		item_y += E_DAY_VIEW_EVENT_BORDER_HEIGHT + E_DAY_VIEW_EVENT_Y_PAD;
		item_h -= (E_DAY_VIEW_EVENT_BORDER_HEIGHT + E_DAY_VIEW_EVENT_Y_PAD) * 2;

		/* We don't show the icons while resizing, since we'd have to
		   draw them on top of the resize rect. */
		num_icons = 0;
		if (day_view->resize_drag_pos == E_DAY_VIEW_POS_NONE
		    || day_view->resize_event_day != day
		    || day_view->resize_event_num != event_num) {
#if 0
			if (ico->dalarm.enabled || ico->malarm.enabled
			    || ico->palarm.enabled || ico->aalarm.enabled)
				num_icons++;
#endif
			if (cal_component_has_rrules (comp)
			    || cal_component_has_rdates (comp))
				num_icons++;
		}

		if (num_icons > 0) {
			if (item_h >= (E_DAY_VIEW_ICON_HEIGHT + E_DAY_VIEW_ICON_Y_PAD) * num_icons)
				icons_offset = E_DAY_VIEW_ICON_WIDTH + E_DAY_VIEW_ICON_X_PAD * 2;
			else
				icons_offset = (E_DAY_VIEW_ICON_WIDTH + E_DAY_VIEW_ICON_X_PAD) * num_icons + E_DAY_VIEW_ICON_X_PAD;
			item_x += icons_offset;
			item_w -= icons_offset;
		}

		if (!event->canvas_item) {
			event->canvas_item =
				gnome_canvas_item_new (GNOME_CANVAS_GROUP (GNOME_CANVAS (day_view->main_canvas)->root),
						       e_text_get_type (),
						       "font_gdk", GTK_WIDGET (day_view)->style->font,
						       "anchor", GTK_ANCHOR_NW,
						       "line_wrap", TRUE,
						       "editable", TRUE,
						       "clip", TRUE,
						       "use_ellipsis", TRUE,
						       NULL);
			gtk_signal_connect (GTK_OBJECT (event->canvas_item),
					    "event",
					    GTK_SIGNAL_FUNC (e_day_view_on_text_item_event),
					    day_view);
			e_day_view_update_event_label (day_view, day,
						       event_num);
		}

		item_w = MAX (item_w, 0);
		gnome_canvas_item_set (event->canvas_item,
				       "clip_width", (gdouble) item_w,
				       "clip_height", (gdouble) item_h,
				       NULL);
		e_canvas_item_move_absolute(event->canvas_item,
					    item_x, item_y);
	}
}


/* This creates or resizes the horizontal bars used to resize events in the
   main canvas. */
static void
e_day_view_reshape_main_canvas_resize_bars (EDayView *day_view)
{
	gint day, event_num;
	gint item_x, item_y, item_w, item_h;
	gdouble x, y, w, h;

	day = day_view->resize_bars_event_day;
	event_num = day_view->resize_bars_event_num;

	/* If we're not editing an event, or the event is not shown,
	   hide the resize bars. */
	if (day != -1 && day == day_view->drag_event_day
	    && event_num == day_view->drag_event_num) {
		gtk_object_get (GTK_OBJECT (day_view->drag_rect_item),
				"x1", &x,
				"y1", &y,
				"x2", &w,
				"y2", &h,
				NULL);
		w -= x;
		x++;
		h -= y;
	} else if (day != -1
		   && e_day_view_get_event_position (day_view, day, event_num,
						     &item_x, &item_y,
						     &item_w, &item_h)) {
		x = item_x + E_DAY_VIEW_BAR_WIDTH;
		y = item_y;
		w = item_w - E_DAY_VIEW_BAR_WIDTH;
		h = item_h;
	} else {
		gnome_canvas_item_hide (day_view->main_canvas_top_resize_bar_item);
		gnome_canvas_item_hide (day_view->main_canvas_bottom_resize_bar_item);
		return;
	}

	gnome_canvas_item_set (day_view->main_canvas_top_resize_bar_item,
			       "x1", x - E_DAY_VIEW_BAR_WIDTH,
			       "y1", y - E_DAY_VIEW_BAR_HEIGHT,
			       "x2", x + w - 1,
			       "y2", y - 1,
			       NULL);
	gnome_canvas_item_show (day_view->main_canvas_top_resize_bar_item);

	gnome_canvas_item_set (day_view->main_canvas_bottom_resize_bar_item,
			       "x1", x - E_DAY_VIEW_BAR_WIDTH,
			       "y1", y + h,
			       "x2", x + w - 1,
			       "y2", y + h + E_DAY_VIEW_BAR_HEIGHT - 1,
			       NULL);
	gnome_canvas_item_show (day_view->main_canvas_bottom_resize_bar_item);
}


static void
e_day_view_ensure_events_sorted (EDayView *day_view)
{
	gint day;

	/* Sort the long events. */
	if (!day_view->long_events_sorted) {
		qsort (day_view->long_events->data,
		       day_view->long_events->len,
		       sizeof (EDayViewEvent),
		       e_day_view_event_sort_func);
		day_view->long_events_sorted = TRUE;
	}

	/* Sort the events for each day. */
	for (day = 0; day < day_view->days_shown; day++) {
		if (!day_view->events_sorted[day]) {
			qsort (day_view->events[day]->data,
			       day_view->events[day]->len,
			       sizeof (EDayViewEvent),
			       e_day_view_event_sort_func);
			day_view->events_sorted[day] = TRUE;
		}
	}
}


static gint
e_day_view_event_sort_func (const void *arg1,
			    const void *arg2)
{
	EDayViewEvent *event1, *event2;

	event1 = (EDayViewEvent*) arg1;
	event2 = (EDayViewEvent*) arg2;

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


static gint
e_day_view_key_press (GtkWidget *widget, GdkEventKey *event)
{
	EDayView *day_view;
	CalComponent *comp;
	gint day, event_num;
	gchar *initial_text;
	guint keyval;
	gboolean stop_emission;
	time_t dtstart, dtend;
	const char *uid;
	
	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (E_IS_DAY_VIEW (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	day_view = E_DAY_VIEW (widget);
	keyval = event->keyval;

	/* The Escape key aborts a resize operation. */
	if (day_view->resize_drag_pos != E_DAY_VIEW_POS_NONE) {
		if (keyval == GDK_Escape) {
			e_day_view_abort_resize (day_view, event->time);
		}
		return FALSE;
	}

	/* Handle the cursor keys for moving & extending the selection. */
	stop_emission = TRUE;
	if (event->state & GDK_SHIFT_MASK) {
		switch (keyval) {
		case GDK_Up:
			e_day_view_cursor_key_up_shifted (day_view, event);
			break;
		case GDK_Down:
			e_day_view_cursor_key_down_shifted (day_view, event);
			break;
		case GDK_Left:
			e_day_view_cursor_key_left_shifted (day_view, event);
			break;
		case GDK_Right:
			e_day_view_cursor_key_right_shifted (day_view, event);
			break;
		default:
			stop_emission = FALSE;
			break;
		}
	} else {
		switch (keyval) {
		case GDK_Up:
			e_day_view_cursor_key_up (day_view, event);
			break;
		case GDK_Down:
			e_day_view_cursor_key_down (day_view, event);
			break;
		case GDK_Left:
			e_day_view_cursor_key_left (day_view, event);
			break;
		case GDK_Right:
			e_day_view_cursor_key_right (day_view, event);
			break;
		default:
			stop_emission = FALSE;
			break;
		}
	}
	if (stop_emission)
		return TRUE;

	if (day_view->selection_start_day == -1)
		return FALSE;

	/* Check if there is room for a new event to be typed in. If there
	   isn't we don't want to add an event as we will then add a new
	   event for every key press. */
	if (!e_day_view_check_if_new_event_fits (day_view)) {
		return FALSE;
	}

	/* We only want to start an edit with a return key or a simple
	   character. */
	if (keyval == GDK_Return) {
		initial_text = NULL;
	} else if ((keyval < 0x20)
		   || (keyval > 0xFF)
		   || (event->length == 0)
		   || (event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK))) {
		return FALSE;
	} else {
		initial_text = event->string;
	}

	/* Add a new event covering the selected range.
	   Note that user_name is a global variable. */
	comp = cal_component_new ();

	e_day_view_get_selected_time_range (day_view, &dtstart, &dtend);

	/* We add the event locally and start editing it. When we get the
	   "update_event" callback from the server, we basically ignore it.
	   If we were to wait for the "update_event" callback it wouldn't be
	   as responsive and we may lose a few keystrokes. */
	e_day_view_add_event (comp, dtstart, dtend, day_view);
	e_day_view_check_layout (day_view);
	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);

	cal_component_get_uid (comp, &uid);
	if (e_day_view_find_event_from_uid (day_view, uid, &day, &event_num)) {
		e_day_view_start_editing_event (day_view, day, event_num,
						initial_text);
		day_view->editing_new_event = TRUE;
	} else {
		g_warning ("Couldn't find event to start editing.\n");
	}

	if (!cal_client_update_object (day_view->calendar->client, comp))
		g_message ("e_day_view_key_press(): Could not update the object!");

	gtk_object_unref (GTK_OBJECT (comp));

	return TRUE;
}


static void
e_day_view_cursor_key_up_shifted (EDayView *day_view, GdkEventKey *event)
{
	gint *row;

	if (day_view->selection_in_top_canvas)
		return;

	if (day_view->selection_drag_pos == E_DAY_VIEW_DRAG_START)
		row = &day_view->selection_start_row;
	else
		row = &day_view->selection_end_row;

	if (*row == 0)
		return;

	*row = *row - 1;

	e_day_view_ensure_rows_visible (day_view, *row, *row);

	e_day_view_normalize_selection (day_view);

	e_day_view_update_calendar_selection_time (day_view);

	/* FIXME: Optimise? */
	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);
}


static void
e_day_view_cursor_key_down_shifted (EDayView *day_view, GdkEventKey *event)
{
	gint *row;

	if (day_view->selection_in_top_canvas)
		return;

	if (day_view->selection_drag_pos == E_DAY_VIEW_DRAG_START)
		row = &day_view->selection_start_row;
	else
		row = &day_view->selection_end_row;

	if (*row >= day_view->rows - 1)
		return;

	*row = *row + 1;

	e_day_view_ensure_rows_visible (day_view, *row, *row);

	e_day_view_normalize_selection (day_view);

	e_day_view_update_calendar_selection_time (day_view);

	/* FIXME: Optimise? */
	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);
}


static void
e_day_view_cursor_key_left_shifted (EDayView *day_view, GdkEventKey *event)
{
	gint *day;

	if (day_view->selection_drag_pos == E_DAY_VIEW_DRAG_START)
		day = &day_view->selection_start_day;
	else
		day = &day_view->selection_end_day;

	if (*day == 0)
		return;

	*day = *day - 1;

	e_day_view_normalize_selection (day_view);

	e_day_view_update_calendar_selection_time (day_view);

	/* FIXME: Optimise? */
	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);
}


static void
e_day_view_cursor_key_right_shifted (EDayView *day_view, GdkEventKey *event)
{
	gint *day;

	if (day_view->selection_drag_pos == E_DAY_VIEW_DRAG_START)
		day = &day_view->selection_start_day;
	else
		day = &day_view->selection_end_day;

	if (*day >= day_view->days_shown - 1)
		return;

	*day = *day + 1;

	e_day_view_normalize_selection (day_view);

	e_day_view_update_calendar_selection_time (day_view);

	/* FIXME: Optimise? */
	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);
}


static void
e_day_view_cursor_key_up (EDayView *day_view, GdkEventKey *event)
{
	if (day_view->selection_start_day == -1) {
		day_view->selection_start_day = 0;
		day_view->selection_start_row = 0;
	}
	day_view->selection_end_day = day_view->selection_start_day;

	if (day_view->selection_in_top_canvas) {
		return;
	} else if (day_view->selection_start_row == 0) {
		day_view->selection_in_top_canvas = TRUE;
		day_view->selection_start_row = -1;
	} else {
		day_view->selection_start_row--;
	}
	day_view->selection_end_row = day_view->selection_start_row;

	if (!day_view->selection_in_top_canvas)
		e_day_view_ensure_rows_visible (day_view,
						day_view->selection_start_row,
						day_view->selection_end_row);

	e_day_view_update_calendar_selection_time (day_view);

	/* FIXME: Optimise? */
	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);
}


static void
e_day_view_cursor_key_down (EDayView *day_view, GdkEventKey *event)
{
	if (day_view->selection_start_day == -1) {
		day_view->selection_start_day = 0;
		day_view->selection_start_row = 0;
	}
	day_view->selection_end_day = day_view->selection_start_day;

	if (day_view->selection_in_top_canvas) {
		day_view->selection_in_top_canvas = FALSE;
		day_view->selection_start_row = 0;
	} else if (day_view->selection_start_row >= day_view->rows - 1) {
		return;
	} else {
		day_view->selection_start_row++;
	}
	day_view->selection_end_row = day_view->selection_start_row;

	if (!day_view->selection_in_top_canvas)
		e_day_view_ensure_rows_visible (day_view,
						day_view->selection_start_row,
						day_view->selection_end_row);

	e_day_view_update_calendar_selection_time (day_view);

	/* FIXME: Optimise? */
	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);
}


static void
e_day_view_cursor_key_left (EDayView *day_view, GdkEventKey *event)
{
	if (day_view->selection_start_day == 0) {
		gnome_calendar_previous (day_view->calendar);
	} else {
		day_view->selection_start_day--;
		day_view->selection_end_day--;

		e_day_view_update_calendar_selection_time (day_view);

		/* FIXME: Optimise? */
		gtk_widget_queue_draw (day_view->top_canvas);
		gtk_widget_queue_draw (day_view->main_canvas);
	}
}


static void
e_day_view_cursor_key_right (EDayView *day_view, GdkEventKey *event)
{
	if (day_view->selection_end_day == day_view->days_shown - 1) {
		gnome_calendar_next (day_view->calendar);
	} else {
		day_view->selection_start_day++;
		day_view->selection_end_day++;

		e_day_view_update_calendar_selection_time (day_view);

		/* FIXME: Optimise? */
		gtk_widget_queue_draw (day_view->top_canvas);
		gtk_widget_queue_draw (day_view->main_canvas);
	}
}


static gboolean
e_day_view_check_if_new_event_fits (EDayView *day_view)
{
	gint day, start_row, end_row, row;

	day = day_view->selection_start_day;
	start_row = day_view->selection_start_row;
	end_row = day_view->selection_end_row;

	/* Long events always fit, since we keep adding rows to the top
	   canvas. */
	if (day != day_view->selection_end_day)
		return FALSE;
	if (start_row == 0 && end_row == day_view->rows)
		return FALSE;

	/* If any of the rows already have E_DAY_VIEW_MAX_COLUMNS columns,
	   return FALSE. */
	for (row = start_row; row <= end_row; row++) {
		if (day_view->cols_per_row[day][row] >= E_DAY_VIEW_MAX_COLUMNS)
			return FALSE;
	}

	return TRUE;
}


static void
e_day_view_ensure_rows_visible (EDayView *day_view,
				gint start_row,
				gint end_row)
{
	GtkAdjustment *adj;
	gfloat value, min_value, max_value;

	adj = GTK_LAYOUT (day_view->main_canvas)->vadjustment;

	value = adj->value;

	min_value = (end_row + 1) * day_view->row_height - adj->page_size;
	if (value < min_value)
		value = min_value;

	max_value = start_row * day_view->row_height;
	if (value > max_value)
		value = max_value;

	if (value != adj->value) {
		adj->value = value;
		gtk_adjustment_value_changed (adj);
	}
}


static void
e_day_view_start_editing_event (EDayView *day_view,
				gint	  day,
				gint	  event_num,
				gchar    *initial_text)
{
	EDayViewEvent *event;
	ETextEventProcessor *event_processor = NULL;
	ETextEventProcessorCommand command;

	/* If we are already editing the event, just return. */
	if (day == day_view->editing_event_day
	    && event_num == day_view->editing_event_num)
		return;

	if (day == E_DAY_VIEW_LONG_EVENT) {
		event = &g_array_index (day_view->long_events, EDayViewEvent,
					event_num);
	} else {
		event = &g_array_index (day_view->events[day], EDayViewEvent,
					event_num);
	}

	/* If the event is not shown, don't try to edit it. */
	if (!event->canvas_item)
		return;

	/* We must grab the focus before setting the initial text, since
	   grabbing the focus will result in a call to
	   e_day_view_on_editing_started(), which will reset the text to get
	   rid of the start and end times. */
	e_canvas_item_grab_focus (event->canvas_item);

	if (initial_text) {
		gnome_canvas_item_set (event->canvas_item,
				       "text", initial_text,
				       NULL);
	}

	/* Try to move the cursor to the end of the text. */
	gtk_object_get (GTK_OBJECT (event->canvas_item),
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
static void
e_day_view_stop_editing_event (EDayView *day_view)
{
	GtkWidget *toplevel;

	/* Check we are editing an event. */
	if (day_view->editing_event_day == -1)
		return;

	/* Set focus to the toplevel so the item loses focus. */
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (day_view));
	if (toplevel && GTK_IS_WINDOW (toplevel))
		gtk_window_set_focus (GTK_WINDOW (toplevel), NULL);
}


static gboolean
e_day_view_on_text_item_event (GnomeCanvasItem *item,
			       GdkEvent *event,
			       EDayView *day_view)
{
	switch (event->type) {
	case GDK_KEY_PRESS:
		if (event && event->key.keyval == GDK_Return) {
			/* We set the keyboard focus to the EDayView, so the
			   EText item loses it and stops the edit. */
			gtk_widget_grab_focus (GTK_WIDGET (day_view));

			/* Stop the signal last or we will also stop any
			   other events getting to the EText item. */
			gtk_signal_emit_stop_by_name (GTK_OBJECT (item),
						      "event");
			return TRUE;
		}
		break;
	case GDK_BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
		/* Only let the EText handle the event while editing. */
		if (!E_TEXT (item)->editing)
			gtk_signal_emit_stop_by_name (GTK_OBJECT (item),
						      "event");
		break;
	case GDK_FOCUS_CHANGE:
		if (event->focus_change.in)
			e_day_view_on_editing_started (day_view, item);
		else
			e_day_view_on_editing_stopped (day_view, item);

		return FALSE;
	default:
		break;
	}

	return FALSE;
}


static void
e_day_view_on_editing_started (EDayView *day_view,
			       GnomeCanvasItem *item)
{
	gint day, event_num;

	if (!e_day_view_find_event_from_item (day_view, item,
					      &day, &event_num))
		return;

#if 0
	g_print ("In e_day_view_on_editing_started Day:%i Event:%i\n",
		 day, event_num);
#endif

	/* FIXME: This is a temporary workaround for a bug which seems to stop
	   us getting focus_out signals. It is not a complete fix since if we
	   don't get focus_out signals we don't save the appointment text so
	   this may be lost. */
	if (day_view->editing_event_day == day
	    && day_view->editing_event_num == event_num)
		return;

	day_view->editing_event_day = day;
	day_view->editing_event_num = event_num;

	if (day == E_DAY_VIEW_LONG_EVENT) {
		e_day_view_reshape_long_event (day_view, event_num);
	} else {
		day_view->resize_bars_event_day = day;
		day_view->resize_bars_event_num = event_num;
		e_day_view_update_event_label (day_view, day, event_num);
		e_day_view_reshape_main_canvas_resize_bars (day_view);
	}
}


static void
e_day_view_on_editing_stopped (EDayView *day_view,
			       GnomeCanvasItem *item)
{
	gint day, event_num;
	gboolean editing_long_event = FALSE;
	EDayViewEvent *event;
	gchar *text = NULL;
	CalComponentText summary;
	const char *uid;
	
	/* Note: the item we are passed here isn't reliable, so we just stop
	   the edit of whatever item was being edited. We also receive this
	   event twice for some reason. */
	day = day_view->editing_event_day;
	event_num = day_view->editing_event_num;

	/* If no item is being edited, just return. */
	if (day == -1)
		return;

#if 0
	g_print ("In e_day_view_on_editing_stopped Day:%i Event:%i\n",
		 day, event_num);
#endif

	if (day == E_DAY_VIEW_LONG_EVENT) {
		editing_long_event = TRUE;
		event = &g_array_index (day_view->long_events, EDayViewEvent,
					event_num);
	} else {
		event = &g_array_index (day_view->events[day], EDayViewEvent,
					event_num);

		/* Hide the horizontal bars. */
		gnome_canvas_item_hide (day_view->main_canvas_top_resize_bar_item);
		gnome_canvas_item_hide (day_view->main_canvas_bottom_resize_bar_item);
	}

	/* Reset the edit fields. */
	day_view->editing_event_day = -1;
	day_view->editing_event_num = -1;
	day_view->editing_new_event = FALSE;

	day_view->resize_bars_event_day = -1;
	day_view->resize_bars_event_num = -1;

	/* Check that the event is still valid. */
	cal_component_get_uid (event->comp, &uid);
	if (!uid)
		return;

	gtk_object_get (GTK_OBJECT (event->canvas_item),
			"text", &text,
			NULL);

	/* Only update the summary if necessary. */
	cal_component_get_summary (event->comp, &summary);
	if (text && summary.value && !strcmp (text, summary.value)) {
		g_free (text);

		if (day == E_DAY_VIEW_LONG_EVENT)
			e_day_view_reshape_long_event (day_view, event_num);
		return;
	}

	cal_component_set_summary (event->comp, &summary);
	g_free (text);
	
	if (!cal_client_update_object (day_view->calendar->client, event->comp))
		g_message ("e_day_view_on_editing_stopped(): Could not update the object!");
}


/* FIXME: It is possible that we may produce an invalid time due to daylight
   saving times (i.e. when clocks go forward there is a range of time which
   is not valid). I don't know the best way to handle daylight saving time. */
static time_t
e_day_view_convert_grid_position_to_time (EDayView *day_view,
					  gint col,
					  gint row)
{
	struct tm *tmp_tm;
	time_t val;
	gint minutes;

	/* Calulate the number of minutes since the start of the day. */
	minutes = day_view->first_hour_shown * 60
		+ day_view->first_minute_shown
		+ row * day_view->mins_per_row;

	/* A special case for midnight, where we can use the start of the
	   next day. */
	if (minutes == 60 * 24)
		return day_view->day_starts[col + 1];

	/* We convert the start of the day to a struct tm, then set the
	   hour and minute, then convert it back to a time_t. */
	tmp_tm = localtime (&day_view->day_starts[col]);

	tmp_tm->tm_hour = minutes / 60;
	tmp_tm->tm_min = minutes % 60;
	tmp_tm->tm_isdst = -1;

	val = mktime (tmp_tm);
	return val;
}


static gboolean
e_day_view_convert_time_to_grid_position (EDayView *day_view,
					  time_t time,
					  gint *col,
					  gint *row)
{
	struct tm *tmp_tm;
	gint day, minutes;

	*col = *row = 0;

	if (time < day_view->lower || time >= day_view->upper)
		return FALSE;

	/* We can find the column easily using the day_starts array. */
	for (day = 1; day <= day_view->days_shown; day++) {
		if (time < day_view->day_starts[day]) {
			*col = day - 1;
			break;
		}
	}

	/* To find the row we need to convert the time to a struct tm,
	   calculate the offset in minutes from the top of the display and
	   divide it by the mins per row setting. */
	tmp_tm = localtime (&time);
	minutes = tmp_tm->tm_hour * 60 + tmp_tm->tm_min;
	minutes -= day_view->first_hour_shown * 60
		+ day_view->first_minute_shown;

	*row = minutes / day_view->mins_per_row;

	if (*row < 0 || *row >= day_view->rows)
		return FALSE;

	return TRUE;
}


/* This starts or stops auto-scrolling when dragging a selection or resizing
   an event. */
void
e_day_view_check_auto_scroll (EDayView *day_view,
			      gint event_x,
			      gint event_y)
{
	day_view->last_mouse_x = event_x;
	day_view->last_mouse_y = event_y;

	if (event_y < E_DAY_VIEW_AUTO_SCROLL_OFFSET)
		e_day_view_start_auto_scroll (day_view, TRUE);
	else if (event_y >= day_view->main_canvas->allocation.height
		 - E_DAY_VIEW_AUTO_SCROLL_OFFSET)
		e_day_view_start_auto_scroll (day_view, FALSE);
	else
		e_day_view_stop_auto_scroll (day_view);
}


static void
e_day_view_start_auto_scroll (EDayView *day_view,
			      gboolean scroll_up)
{
	if (day_view->auto_scroll_timeout_id == 0) {
		day_view->auto_scroll_timeout_id = g_timeout_add (E_DAY_VIEW_AUTO_SCROLL_TIMEOUT, e_day_view_auto_scroll_handler, day_view);
		day_view->auto_scroll_delay = E_DAY_VIEW_AUTO_SCROLL_DELAY;
	}
	day_view->auto_scroll_up = scroll_up;
}


void
e_day_view_stop_auto_scroll (EDayView *day_view)
{
	if (day_view->auto_scroll_timeout_id != 0) {
		gtk_timeout_remove (day_view->auto_scroll_timeout_id);
		day_view->auto_scroll_timeout_id = 0;
	}
}


static gboolean
e_day_view_auto_scroll_handler (gpointer data)
{
	EDayView *day_view;
	EDayViewPosition pos;
	gint scroll_x, scroll_y, new_scroll_y, canvas_x, canvas_y, row, day;
	GtkAdjustment *adj;

	g_return_val_if_fail (E_IS_DAY_VIEW (data), FALSE);

	day_view = E_DAY_VIEW (data);

	GDK_THREADS_ENTER ();

	if (day_view->auto_scroll_delay > 0) {
		day_view->auto_scroll_delay--;
		GDK_THREADS_LEAVE ();
		return TRUE;
	}

	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (day_view->main_canvas),
					 &scroll_x, &scroll_y);

	adj = GTK_LAYOUT (day_view->main_canvas)->vadjustment;

	if (day_view->auto_scroll_up)
		new_scroll_y = MAX (scroll_y - adj->step_increment, 0);
	else
		new_scroll_y = MIN (scroll_y + adj->step_increment,
				    adj->upper - adj->page_size);

	if (new_scroll_y != scroll_y) {
		/* NOTE: This reduces flicker, but only works if we don't use
		   canvas items which have X windows. */
		gtk_layout_freeze (GTK_LAYOUT (day_view->main_canvas));

		gnome_canvas_scroll_to (GNOME_CANVAS (day_view->main_canvas),
					scroll_x, new_scroll_y);

		gtk_layout_thaw (GTK_LAYOUT (day_view->main_canvas));
	}

	canvas_x = day_view->last_mouse_x + scroll_x;
	canvas_y = day_view->last_mouse_y + new_scroll_y;

	/* The last_mouse_x position is set to -1 when we are selecting using
	   the time column. In this case we set canvas_x to 0 and we ignore
	   the resulting day. */
	if (day_view->last_mouse_x == -1)
		canvas_x = 0;

	/* Update the selection/resize/drag if necessary. */
	pos = e_day_view_convert_position_in_main_canvas (day_view,
							  canvas_x, canvas_y,
							  &day, &row, NULL);

	if (day_view->last_mouse_x == -1)
		day = -1;

	if (pos != E_DAY_VIEW_POS_OUTSIDE) {
		if (day_view->selection_is_being_dragged) {
			e_day_view_update_selection (day_view, day, row);
		} else if (day_view->resize_drag_pos != E_DAY_VIEW_POS_NONE) {
			e_day_view_update_resize (day_view, row);
		} else if (day_view->drag_item->object.flags
			   & GNOME_CANVAS_ITEM_VISIBLE) {
			e_day_view_update_main_canvas_drag (day_view, row,
							    day);
		}
	}

	GDK_THREADS_LEAVE ();
	return TRUE;
}


gboolean
e_day_view_get_event_position (EDayView *day_view,
			       gint day,
			       gint event_num,
			       gint *item_x,
			       gint *item_y,
			       gint *item_w,
			       gint *item_h)
{
	EDayViewEvent *event;
	gint start_row, end_row, cols_in_row, start_col, num_columns;

	event = &g_array_index (day_view->events[day], EDayViewEvent,
				event_num);

	/* If the event is flagged as not displayed, return FALSE. */
	if (event->num_columns == 0)
		return FALSE;

	start_row = event->start_minute / day_view->mins_per_row;
	end_row = (event->end_minute - 1) / day_view->mins_per_row;
	cols_in_row = day_view->cols_per_row[day][start_row];
	start_col = event->start_row_or_col;
	num_columns = event->num_columns;

	if (cols_in_row == 0)
		return FALSE;

	/* If the event is being resize, use the resize position. */
	if (day_view->resize_drag_pos != E_DAY_VIEW_POS_NONE
	    && day_view->resize_event_day == day
	    && day_view->resize_event_num == event_num) {
		if (day_view->resize_drag_pos == E_DAY_VIEW_POS_TOP_EDGE)
			start_row = day_view->resize_start_row;
		else if (day_view->resize_drag_pos == E_DAY_VIEW_POS_BOTTOM_EDGE)
			end_row = day_view->resize_end_row;
	}

	*item_x = day_view->day_offsets[day] + day_view->day_widths[day] * start_col / cols_in_row;
	*item_w = day_view->day_widths[day] * num_columns / cols_in_row - E_DAY_VIEW_GAP_WIDTH;
	*item_w = MAX (*item_w, 0);
	*item_y = start_row * day_view->row_height;
	*item_h = (end_row - start_row + 1) * day_view->row_height;

	return TRUE;
}


gboolean
e_day_view_get_long_event_position	(EDayView	*day_view,
					 gint		 event_num,
					 gint		*start_day,
					 gint		*end_day,
					 gint		*item_x,
					 gint		*item_y,
					 gint		*item_w,
					 gint		*item_h)
{
	EDayViewEvent *event;

	event = &g_array_index (day_view->long_events, EDayViewEvent,
				event_num);

	/* If the event is flagged as not displayed, return FALSE. */
	if (event->num_columns == 0)
		return FALSE;

	if (!e_day_view_find_long_event_days (day_view, event,
					      start_day, end_day))
		return FALSE;

	/* If the event is being resize, use the resize position. */
	if (day_view->resize_drag_pos != E_DAY_VIEW_POS_NONE
	    && day_view->resize_event_day == E_DAY_VIEW_LONG_EVENT
	    && day_view->resize_event_num == event_num) {
		if (day_view->resize_drag_pos == E_DAY_VIEW_POS_LEFT_EDGE)
			*start_day = day_view->resize_start_row;
		else if (day_view->resize_drag_pos == E_DAY_VIEW_POS_RIGHT_EDGE)
			*end_day = day_view->resize_end_row;
	}

	*item_x = day_view->day_offsets[*start_day] + E_DAY_VIEW_BAR_WIDTH;
	*item_w = day_view->day_offsets[*end_day + 1] - *item_x
		- E_DAY_VIEW_GAP_WIDTH;
	*item_w = MAX (*item_w, 0);
	*item_y = (event->start_row_or_col + 1) * day_view->top_row_height;
	*item_h = day_view->top_row_height - E_DAY_VIEW_TOP_CANVAS_Y_GAP;
	return TRUE;
}


/* Converts a position within the entire top canvas to a day & event and
   a place within the event if appropriate. If event_num_return is NULL, it
   simply returns the grid position without trying to find the event. */
static EDayViewPosition
e_day_view_convert_position_in_top_canvas (EDayView *day_view,
					   gint x,
					   gint y,
					   gint *day_return,
					   gint *event_num_return)
{
	EDayViewEvent *event;
	gint day, row, col;
	gint event_num, start_day, end_day, item_x, item_y, item_w, item_h;

	*day_return = -1;
	if (event_num_return)
		*event_num_return = -1;

	if (x < 0 || y < 0)
		return E_DAY_VIEW_POS_OUTSIDE;

	row = y / day_view->top_row_height - 1;

	day = -1;
	for (col = 1; col <= day_view->days_shown; col++) {
		if (x < day_view->day_offsets[col]) {
			day = col - 1;
			break;
		}
	}
	if (day == -1)
		return E_DAY_VIEW_POS_OUTSIDE;

	*day_return = day;

	/* If only the grid position is wanted, return. */
	if (event_num_return == NULL)
		return E_DAY_VIEW_POS_NONE;

	for (event_num = 0; event_num < day_view->long_events->len;
	     event_num++) {
		event = &g_array_index (day_view->long_events, EDayViewEvent,
					event_num);

		if (event->start_row_or_col != row)
			continue;

		if (!e_day_view_get_long_event_position (day_view, event_num,
							 &start_day, &end_day,
							 &item_x, &item_y,
							 &item_w, &item_h))
			continue;

		if (x < item_x)
			continue;

		if (x >= item_x + item_w)
			continue;

		*event_num_return = event_num;

		if (x < item_x + E_DAY_VIEW_LONG_EVENT_BORDER_WIDTH
		    + E_DAY_VIEW_LONG_EVENT_X_PAD)
			return E_DAY_VIEW_POS_LEFT_EDGE;

		if (x >= item_x + item_w - E_DAY_VIEW_LONG_EVENT_BORDER_WIDTH
		    - E_DAY_VIEW_LONG_EVENT_X_PAD)
			return E_DAY_VIEW_POS_RIGHT_EDGE;

		return E_DAY_VIEW_POS_EVENT;
	}

	return E_DAY_VIEW_POS_NONE;
}


/* Converts a position within the entire main canvas to a day, row, event and
   a place within the event if appropriate. If event_num_return is NULL, it
   simply returns the grid position without trying to find the event. */
static EDayViewPosition
e_day_view_convert_position_in_main_canvas (EDayView *day_view,
					    gint x,
					    gint y,
					    gint *day_return,
					    gint *row_return,
					    gint *event_num_return)
{
	gint day, row, col, event_num;
	gint item_x, item_y, item_w, item_h;

	*day_return = -1;
	*row_return = -1;
	if (event_num_return)
		*event_num_return = -1;

	/* Check the position is inside the canvas, and determine the day
	   and row. */
	if (x < 0 || y < 0)
		return E_DAY_VIEW_POS_OUTSIDE;

	row = y / day_view->row_height;
	if (row >= day_view->rows)
		return E_DAY_VIEW_POS_OUTSIDE;

	day = -1;
	for (col = 1; col <= day_view->days_shown; col++) {
		if (x < day_view->day_offsets[col]) {
			day = col - 1;
			break;
		}
	}
	if (day == -1)
		return E_DAY_VIEW_POS_OUTSIDE;

	*day_return = day;
	*row_return = row;

	/* If only the grid position is wanted, return. */
	if (event_num_return == NULL)
		return E_DAY_VIEW_POS_NONE;

	/* Check the selected item first, since the horizontal resizing bars
	   may be above other events. */
	if (day_view->resize_bars_event_day == day) {
		if (e_day_view_get_event_position (day_view, day,
						   day_view->resize_bars_event_num,
						   &item_x, &item_y,
						   &item_w, &item_h)) {
			if (x >= item_x && x < item_x + item_w) {
				*event_num_return = day_view->resize_bars_event_num;
				if (y >= item_y - E_DAY_VIEW_BAR_HEIGHT
				    && y < item_y + E_DAY_VIEW_EVENT_BORDER_HEIGHT)
					return E_DAY_VIEW_POS_TOP_EDGE;
				if (y >= item_y + item_h - E_DAY_VIEW_EVENT_BORDER_HEIGHT
				    && y < item_y + item_h + E_DAY_VIEW_BAR_HEIGHT)
					return E_DAY_VIEW_POS_BOTTOM_EDGE;
			}
		}
	}

	/* Try to find the event at the found position. */
	*event_num_return = -1;
	for (event_num = 0; event_num < day_view->events[day]->len;
	     event_num++) {
		if (!e_day_view_get_event_position (day_view, day, event_num,
						    &item_x, &item_y,
						    &item_w, &item_h))
			continue;

		if (x < item_x || x >= item_x + item_w
		    || y < item_y || y >= item_y + item_h)
			continue;

		*event_num_return = event_num;

		if (x < item_x + E_DAY_VIEW_BAR_WIDTH)
			return E_DAY_VIEW_POS_LEFT_EDGE;

		if (y < item_y + E_DAY_VIEW_EVENT_BORDER_HEIGHT
		    + E_DAY_VIEW_EVENT_Y_PAD)
			return E_DAY_VIEW_POS_TOP_EDGE;

		if (y >= item_y + item_h - E_DAY_VIEW_EVENT_BORDER_HEIGHT
		    - E_DAY_VIEW_EVENT_Y_PAD)
			return E_DAY_VIEW_POS_BOTTOM_EDGE;

		return E_DAY_VIEW_POS_EVENT;
	}

	return E_DAY_VIEW_POS_NONE;
}


static gint
e_day_view_on_top_canvas_drag_motion (GtkWidget      *widget,
				      GdkDragContext *context,
				      gint            x,
				      gint            y,
				      guint           time,
				      EDayView	     *day_view)
{
	gint scroll_x, scroll_y;

	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (widget),
					 &scroll_x, &scroll_y);
	day_view->drag_event_x = x + scroll_x;
	day_view->drag_event_y = y + scroll_y;

	e_day_view_reshape_top_canvas_drag_item (day_view);

	return TRUE;
}


static void
e_day_view_reshape_top_canvas_drag_item (EDayView *day_view)
{
	EDayViewPosition pos;
	gint x, y, day;

	/* Calculate the day & start row of the event being dragged, using
	   the current mouse position. */
	x = day_view->drag_event_x;
	y = day_view->drag_event_y;
	pos = e_day_view_convert_position_in_top_canvas (day_view, x, y,
							 &day, NULL);
	/* This shouldn't really happen in a drag. */
	if (pos == E_DAY_VIEW_POS_OUTSIDE)
		return;

	if (day_view->drag_event_day == E_DAY_VIEW_LONG_EVENT)
		day -= day_view->drag_event_offset;
	day = MAX (day, 0);

	e_day_view_update_top_canvas_drag (day_view, day);
}


static void
e_day_view_update_top_canvas_drag (EDayView *day_view,
				   gint day)
{
	EDayViewEvent *event = NULL;
	gint row, num_days, start_day, end_day;
	gdouble item_x, item_y, item_w, item_h;
	GdkFont *font;
	gchar *text;


	/* Calculate the event's position. If the event is in the same
	   position we started in, we use the same columns. */
	row = day_view->rows_in_top_display + 1;
	num_days = 1;

	if (day_view->drag_event_day == E_DAY_VIEW_LONG_EVENT) {
		event = &g_array_index (day_view->long_events, EDayViewEvent,
					day_view->drag_event_num);
		row = event->start_row_or_col + 1;

		if (!e_day_view_find_long_event_days (day_view, event,
						      &start_day, &end_day))
			return;

		num_days = end_day - start_day + 1;

		/* Make sure we don't go off the screen. */
		day = MIN (day, day_view->days_shown - num_days);

	} else if (day_view->drag_event_day != -1) {
		event = &g_array_index (day_view->events[day_view->drag_event_day],
					EDayViewEvent,
					day_view->drag_event_num);
	}

	/* If the position hasn't changed, just return. */
	if (day_view->drag_last_day == day
	    && (day_view->drag_long_event_item->object.flags
		& GNOME_CANVAS_ITEM_VISIBLE))
		return;

	day_view->drag_last_day = day;


	item_x = day_view->day_offsets[day] + E_DAY_VIEW_BAR_WIDTH;
	item_w = day_view->day_offsets[day + num_days] - item_x
		- E_DAY_VIEW_GAP_WIDTH;
	item_y = row * day_view->top_row_height;
	item_h = day_view->top_row_height - E_DAY_VIEW_TOP_CANVAS_Y_GAP;


	/* Set the positions of the event & associated items. */
	gnome_canvas_item_set (day_view->drag_long_event_rect_item,
			       "x1", item_x,
			       "y1", item_y,
			       "x2", item_x + item_w - 1,
			       "y2", item_y + item_h - 1,
			       NULL);

	font = GTK_WIDGET (day_view)->style->font;
	gnome_canvas_item_set (day_view->drag_long_event_item,
			       "font_gdk", font,
			       "clip_width", item_w - (E_DAY_VIEW_LONG_EVENT_BORDER_WIDTH + E_DAY_VIEW_LONG_EVENT_X_PAD) * 2,
			       "clip_height", item_h - (E_DAY_VIEW_LONG_EVENT_BORDER_HEIGHT + E_DAY_VIEW_LONG_EVENT_Y_PAD) * 2,
			       NULL);
	e_canvas_item_move_absolute(day_view->drag_long_event_item,
				    item_x + E_DAY_VIEW_LONG_EVENT_BORDER_WIDTH + E_DAY_VIEW_LONG_EVENT_X_PAD,
				    item_y + E_DAY_VIEW_LONG_EVENT_BORDER_HEIGHT + E_DAY_VIEW_LONG_EVENT_Y_PAD);

	if (!(day_view->drag_long_event_rect_item->object.flags & GNOME_CANVAS_ITEM_VISIBLE)) {
		gnome_canvas_item_raise_to_top (day_view->drag_long_event_rect_item);
		gnome_canvas_item_show (day_view->drag_long_event_rect_item);
	}

	/* Set the text, if necessary. We don't want to set the text every
	   time it moves, so we check if it is currently invisible and only
	   set the text then. */
	if (!(day_view->drag_long_event_item->object.flags
	      & GNOME_CANVAS_ITEM_VISIBLE)) {
		CalComponentText summary;
		
		cal_component_get_summary (event->comp, &summary);
		if (event) {
			cal_component_get_summary (event->comp, &summary);
			text = g_strdup (summary.value);
		} else {	
			text = NULL;
		}
		
		gnome_canvas_item_set (day_view->drag_long_event_item,
				       "text", text ? text : "",
				       NULL);
		gnome_canvas_item_raise_to_top (day_view->drag_long_event_item);
		gnome_canvas_item_show (day_view->drag_long_event_item);

		g_free (text);
	}
}


static gint
e_day_view_on_main_canvas_drag_motion (GtkWidget      *widget,
				       GdkDragContext *context,
				       gint            x,
				       gint            y,
				       guint           time,
				       EDayView	      *day_view)
{
	gint scroll_x, scroll_y;

	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (widget),
					 &scroll_x, &scroll_y);
	day_view->drag_event_x = x + scroll_x;
	day_view->drag_event_y = y + scroll_y;

	e_day_view_reshape_main_canvas_drag_item (day_view);
	e_day_view_reshape_main_canvas_resize_bars (day_view);

	e_day_view_check_auto_scroll (day_view, x, y);

	return TRUE;
}


static void
e_day_view_reshape_main_canvas_drag_item (EDayView *day_view)
{
	EDayViewPosition pos;
	gint x, y, day, row;

	/* Calculate the day & start row of the event being dragged, using
	   the current mouse position. */
	x = day_view->drag_event_x;
	y = day_view->drag_event_y;
	pos = e_day_view_convert_position_in_main_canvas (day_view, x, y,
							  &day, &row, NULL);
	/* This shouldn't really happen in a drag. */
	if (pos == E_DAY_VIEW_POS_OUTSIDE)
		return;

	if (day_view->drag_event_day != -1
	    && day_view->drag_event_day != E_DAY_VIEW_LONG_EVENT)
		row -= day_view->drag_event_offset;
	row = MAX (row, 0);

	e_day_view_update_main_canvas_drag (day_view, row, day);
}


static void
e_day_view_update_main_canvas_drag (EDayView *day_view,
				    gint row,
				    gint day)
{
	EDayViewEvent *event = NULL;
	gint cols_in_row, start_col, num_columns, num_rows, start_row, end_row;
	gdouble item_x, item_y, item_w, item_h;
	GdkFont *font;
	gchar *text;

	/* If the position hasn't changed, just return. */
	if (day_view->drag_last_day == day
	    && day_view->drag_last_row == row
	    && (day_view->drag_item->object.flags & GNOME_CANVAS_ITEM_VISIBLE))
		return;

	day_view->drag_last_day = day;
	day_view->drag_last_row = row;

	/* Calculate the event's position. If the event is in the same
	   position we started in, we use the same columns. */
	cols_in_row = 1;
	start_row = 0;
	start_col = 0;
	num_columns = 1;
	num_rows = 1;

	if (day_view->drag_event_day == E_DAY_VIEW_LONG_EVENT) {
		event = &g_array_index (day_view->long_events, EDayViewEvent,
					day_view->drag_event_num);
	} else if (day_view->drag_event_day != -1) {
		event = &g_array_index (day_view->events[day_view->drag_event_day],
					EDayViewEvent,
					day_view->drag_event_num);
		start_row = event->start_minute / day_view->mins_per_row;
		end_row = (event->end_minute - 1) / day_view->mins_per_row;
		num_rows = end_row - start_row + 1;
	}

	if (day_view->drag_event_day == day && start_row == row) {
		cols_in_row = day_view->cols_per_row[day][row];
		start_col = event->start_row_or_col;
		num_columns = event->num_columns;
	}

	item_x = day_view->day_offsets[day]
		+ day_view->day_widths[day] * start_col / cols_in_row;
	item_w = day_view->day_widths[day] * num_columns / cols_in_row
		- E_DAY_VIEW_GAP_WIDTH;
	item_y = row * day_view->row_height;
	item_h = num_rows * day_view->row_height;

	/* Set the positions of the event & associated items. */
	gnome_canvas_item_set (day_view->drag_rect_item,
			       "x1", item_x + E_DAY_VIEW_BAR_WIDTH - 1,
			       "y1", item_y,
			       "x2", item_x + item_w - 1,
			       "y2", item_y + item_h - 1,
			       NULL);

	gnome_canvas_item_set (day_view->drag_bar_item,
			       "x1", item_x,
			       "y1", item_y,
			       "x2", item_x + E_DAY_VIEW_BAR_WIDTH - 1,
			       "y2", item_y + item_h - 1,
			       NULL);

	font = GTK_WIDGET (day_view)->style->font;
	gnome_canvas_item_set (day_view->drag_item,
			       "font_gdk", font,
			       "clip_width", item_w - E_DAY_VIEW_BAR_WIDTH - E_DAY_VIEW_EVENT_X_PAD * 2,
			       "clip_height", item_h - (E_DAY_VIEW_EVENT_BORDER_HEIGHT + E_DAY_VIEW_EVENT_Y_PAD) * 2,
			       NULL);
	e_canvas_item_move_absolute(event->canvas_item,
				    item_x + E_DAY_VIEW_BAR_WIDTH + E_DAY_VIEW_EVENT_X_PAD,
				    item_y + E_DAY_VIEW_EVENT_BORDER_HEIGHT + E_DAY_VIEW_EVENT_Y_PAD);

	if (!(day_view->drag_bar_item->object.flags & GNOME_CANVAS_ITEM_VISIBLE)) {
		gnome_canvas_item_raise_to_top (day_view->drag_bar_item);
		gnome_canvas_item_show (day_view->drag_bar_item);
	}

	if (!(day_view->drag_rect_item->object.flags & GNOME_CANVAS_ITEM_VISIBLE)) {
		gnome_canvas_item_raise_to_top (day_view->drag_rect_item);
		gnome_canvas_item_show (day_view->drag_rect_item);
	}

	/* Set the text, if necessary. We don't want to set the text every
	   time it moves, so we check if it is currently invisible and only
	   set the text then. */
	if (!(day_view->drag_item->object.flags & GNOME_CANVAS_ITEM_VISIBLE)) {
		CalComponentText summary;
		
		if (event) {
			cal_component_get_summary (event->comp, &summary);
			text = g_strdup (summary.value);
		} else {	
			text = NULL;
		}
		
		gnome_canvas_item_set (day_view->drag_item,
				       "text", text ? text : "",
				       NULL);
		gnome_canvas_item_raise_to_top (day_view->drag_item);
		gnome_canvas_item_show (day_view->drag_item);

		g_free (text);
	}
}


static void
e_day_view_on_top_canvas_drag_leave (GtkWidget      *widget,
				     GdkDragContext *context,
				     guint           time,
				     EDayView	     *day_view)
{
	day_view->drag_last_day = -1;

	gnome_canvas_item_hide (day_view->drag_long_event_rect_item);
	gnome_canvas_item_hide (day_view->drag_long_event_item);
}


static void
e_day_view_on_main_canvas_drag_leave (GtkWidget      *widget,
				      GdkDragContext *context,
				      guint           time,
				      EDayView	     *day_view)
{
	day_view->drag_last_day = -1;

	e_day_view_stop_auto_scroll (day_view);

	gnome_canvas_item_hide (day_view->drag_rect_item);
	gnome_canvas_item_hide (day_view->drag_bar_item);
	gnome_canvas_item_hide (day_view->drag_item);

	/* Hide the resize bars if they are being used in the drag. */
	if (day_view->drag_event_day == day_view->resize_bars_event_day
	    && day_view->drag_event_num == day_view->resize_bars_event_num) {
		gnome_canvas_item_hide (day_view->main_canvas_top_resize_bar_item);
		gnome_canvas_item_hide (day_view->main_canvas_bottom_resize_bar_item);
	}
}


static void
e_day_view_on_drag_begin (GtkWidget      *widget,
			  GdkDragContext *context,
			  EDayView	 *day_view)
{
	EDayViewEvent *event;
	gint day, event_num;

	day = day_view->drag_event_day;
	event_num = day_view->drag_event_num;

	/* These should both be set. */
	g_return_if_fail (day != -1);
	g_return_if_fail (event_num != -1);

	if (day == E_DAY_VIEW_LONG_EVENT)
		event = &g_array_index (day_view->long_events, EDayViewEvent,
					event_num);
	else
		event = &g_array_index (day_view->events[day], EDayViewEvent,
					event_num);

	/* Hide the text item, since it will be shown in the special drag
	   items. */
	gnome_canvas_item_hide (event->canvas_item);
}


static void
e_day_view_on_drag_end (GtkWidget      *widget,
			GdkDragContext *context,
			EDayView       *day_view)
{
	EDayViewEvent *event;
	gint day, event_num;

	day = day_view->drag_event_day;
	event_num = day_view->drag_event_num;

	/* If the calendar has already been updated in drag_data_received()
	   we just return. */
	if (day == -1 || event_num == -1)
		return;

	if (day == E_DAY_VIEW_LONG_EVENT) {
		event = &g_array_index (day_view->long_events, EDayViewEvent,
					event_num);
		gtk_widget_queue_draw (day_view->top_canvas);
	} else {
		event = &g_array_index (day_view->events[day], EDayViewEvent,
					event_num);
		gtk_widget_queue_draw (day_view->main_canvas);
	}

	/* Show the text item again. */
	gnome_canvas_item_show (event->canvas_item);

	day_view->drag_event_day = -1;
	day_view->drag_event_num = -1;
}


static void
e_day_view_on_drag_data_get (GtkWidget          *widget,
			     GdkDragContext     *context,
			     GtkSelectionData   *selection_data,
			     guint               info,
			     guint               time,
			     EDayView		*day_view)
{
	EDayViewEvent *event;
	gint day, event_num;
	const char *event_uid;

	day = day_view->drag_event_day;
	event_num = day_view->drag_event_num;

	/* These should both be set. */
	g_return_if_fail (day != -1);
	g_return_if_fail (event_num != -1);

	if (day == E_DAY_VIEW_LONG_EVENT)
		event = &g_array_index (day_view->long_events,
					EDayViewEvent, event_num);
	else
		event = &g_array_index (day_view->events[day],
					EDayViewEvent, event_num);

	
	cal_component_get_uid (event->comp, &event_uid);

	g_return_if_fail (event_uid != NULL);

	if (info == TARGET_CALENDAR_EVENT) {
		gtk_selection_data_set (selection_data,	selection_data->target,
					8, event_uid, strlen (event_uid));
	}
}


static void
e_day_view_on_top_canvas_drag_data_received  (GtkWidget          *widget,
					      GdkDragContext     *context,
					      gint                x,
					      gint                y,
					      GtkSelectionData   *data,
					      guint               info,
					      guint               time,
					      EDayView	         *day_view)
{
	EDayViewEvent *event=NULL;
	EDayViewPosition pos;
	gint day, start_day, end_day, num_days;
	gint start_offset, end_offset;
	gchar *event_uid;
	CalComponent *comp;
	CalComponentDateTime date;
	time_t dt;
	
	if ((data->length >= 0) && (data->format == 8)) {
		pos = e_day_view_convert_position_in_top_canvas (day_view,
								 x, y, &day,
								 NULL);
		if (pos != E_DAY_VIEW_POS_OUTSIDE) {
			const char *uid;
			num_days = 1;
			start_offset = 0;
			end_offset = -1;
			
			if (day_view->drag_event_day == E_DAY_VIEW_LONG_EVENT) {
				event = &g_array_index (day_view->long_events, EDayViewEvent,
							day_view->drag_event_num);
				day -= day_view->drag_event_offset;
				day = MAX (day, 0);

				e_day_view_find_long_event_days (day_view,
								 event,
								 &start_day,
								 &end_day);
				num_days = end_day - start_day + 1;
				/* Make sure we don't go off the screen. */
				day = MIN (day, day_view->days_shown - num_days);

				start_offset = event->start_minute;
				end_offset = event->end_minute;
			} else if (day_view->drag_event_day != -1) {
				event = &g_array_index (day_view->events[day_view->drag_event_day],
							EDayViewEvent,
							day_view->drag_event_num);
			}

			event_uid = data->data;

			cal_component_get_uid (event->comp, &uid);
			
			if (!event_uid || !uid || strcmp (event_uid, uid))
				g_warning ("Unexpected event UID");

			/* We use a temporary shallow of the comp since we
			   don't want to change the original comp here.
			   Otherwise we would not detect that the event's time
			   had changed in the "update_event" callback. */
			
			comp = cal_component_clone (event->comp);

			date.value = g_new (struct icaltimetype, 1);
			dt = day_view->day_starts[day] + start_offset * 60;
			*date.value = icaltimetype_from_timet (dt, FALSE);
			cal_component_set_dtstart (comp, &date);
			if (end_offset == -1 || end_offset == 0)
				dt = day_view->day_starts[day + num_days];
			else
				dt = day_view->day_starts[day + num_days - 1] + end_offset * 60;
			*date.value = icaltimetype_from_timet (dt, FALSE);
			cal_component_set_dtend (comp, &date);
			g_free (date.value);
			
			gtk_drag_finish (context, TRUE, TRUE, time);

			/* Reset this since it will be invalid. */
			day_view->drag_event_day = -1;

			/* Show the text item again, just in case it hasn't
			   moved. If we don't do this it may not appear. */
			if (event->canvas_item)
				gnome_canvas_item_show (event->canvas_item);

			if (!cal_client_update_object (day_view->calendar->client, comp))
				g_message ("e_day_view_on_top_canvas_drag_data_received(): Could "
					   "not update the object!");

			gtk_object_unref (GTK_OBJECT (comp));
			
			return;
		}
	}

	gtk_drag_finish (context, FALSE, FALSE, time);
}


static void
e_day_view_on_main_canvas_drag_data_received  (GtkWidget          *widget,
					       GdkDragContext     *context,
					       gint                x,
					       gint                y,
					       GtkSelectionData   *data,
					       guint               info,
					       guint               time,
					       EDayView		  *day_view)
{
	EDayViewEvent *event = NULL;
	EDayViewPosition pos;
	gint day, row, start_row, end_row, num_rows, scroll_x, scroll_y;
	gint start_offset, end_offset;
	gchar *event_uid;
	CalComponent *comp;
	CalComponentDateTime date;
	time_t dt;
	
	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (widget),
					 &scroll_x, &scroll_y);
	x += scroll_x;
	y += scroll_y;

	if ((data->length >= 0) && (data->format == 8)) {
		pos = e_day_view_convert_position_in_main_canvas (day_view,
								  x, y, &day,
								  &row, NULL);
		if (pos != E_DAY_VIEW_POS_OUTSIDE) {
			const char *uid;
			num_rows = 1;
			start_offset = 0;
			end_offset = 0;
			
			if (day_view->drag_event_day == E_DAY_VIEW_LONG_EVENT) {
				event = &g_array_index (day_view->long_events, EDayViewEvent,
							day_view->drag_event_num);
			} else if (day_view->drag_event_day != -1) {
				event = &g_array_index (day_view->events[day_view->drag_event_day],
							EDayViewEvent,
							day_view->drag_event_num);
				row -= day_view->drag_event_offset;

				/* Calculate time offset from start row. */
				start_row = event->start_minute / day_view->mins_per_row;
				end_row = (event->end_minute - 1) / day_view->mins_per_row;
				num_rows = end_row - start_row + 1;

				start_offset = event->start_minute % day_view->mins_per_row;
				end_offset = event->end_minute % day_view->mins_per_row;
				if (end_offset != 0)
					end_offset = day_view->mins_per_row - end_offset;
			}

			event_uid = data->data;

			cal_component_get_uid (event->comp, &uid);
			if (!event_uid || !uid || strcmp (event_uid, uid))
				g_warning ("Unexpected event UID");

			/* We use a temporary shallow copy of comp since we
			   don't want to change the original comp here.
			   Otherwise we would not detect that the event's time
			   had changed in the "update_event" callback. */
			comp = cal_component_clone (event->comp);

			date.value = g_new (struct icaltimetype, 1);
			dt = e_day_view_convert_grid_position_to_time (day_view, day, row) + start_offset * 60;
			*date.value = icaltimetype_from_timet (dt, FALSE);
			cal_component_set_dtstart (comp, &date);
			dt = e_day_view_convert_grid_position_to_time (day_view, day, row + num_rows) - end_offset * 60;
			*date.value = icaltimetype_from_timet (dt, FALSE);
			cal_component_set_dtend (comp, &date);
			g_free (date.value);
			
			gtk_drag_finish (context, TRUE, TRUE, time);

			/* Reset this since it will be invalid. */
			day_view->drag_event_day = -1;

			/* Show the text item again, just in case it hasn't
			   moved. If we don't do this it may not appear. */
			if (event->canvas_item)
				gnome_canvas_item_show (event->canvas_item);

			if (!cal_client_update_object (day_view->calendar->client, comp))
				g_message ("e_day_view_on_main_canvas_drag_data_received(): "
					   "Could not update the object!");

			gtk_object_unref (GTK_OBJECT (comp));
			
			return;
		}
	}

	gtk_drag_finish (context, FALSE, FALSE, time);
}





