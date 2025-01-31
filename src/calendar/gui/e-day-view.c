/*
 * EDayView - displays the Day & Work-Week views of the calendar.
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
 * Authors:
 *      Damon Chaplin <damon@ximian.com>
 *      Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#include "evolution-config.h"

#include "e-day-view.h"

#include <math.h>
#include <time.h>

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "libgnomecanvas/libgnomecanvas.h"

#include "e-cal-dialogs.h"
#include "e-util/e-util.h"

#include "calendar-config.h"
#include "comp-util.h"
#include "e-cal-ops.h"
#include "e-cal-model-calendar.h"
#include "e-day-view-layout.h"
#include "e-day-view-main-item.h"
#include "e-day-view-time-item.h"
#include "e-day-view-top-item.h"
#include "ea-calendar.h"
#include "itip-utils.h"
#include "print.h"
#include "ea-day-view.h"

/* The minimum amount of space wanted on each side of the date string. */
#define E_DAY_VIEW_DATE_X_PAD	4

#define E_DAY_VIEW_LARGE_FONT_PTSIZE 18
#define E_DAY_VIEW_SMALL_FONT_PTSIZE 10

/* The offset from the top/bottom of the canvas before auto-scrolling starts.*/
#define E_DAY_VIEW_AUTO_SCROLL_OFFSET	16

/* The time between each auto-scroll, in milliseconds. */
#define E_DAY_VIEW_AUTO_SCROLL_TIMEOUT	50

/* The number of timeouts we skip before we start scrolling. */
#define E_DAY_VIEW_AUTO_SCROLL_DELAY	5

/* The amount we scroll the main canvas when the Page Up/Down keys are pressed,
 * as a fraction of the page size. */
#define E_DAY_VIEW_PAGE_STEP		0.5

/* The amount we scroll the main canvas when the mouse wheel buttons are
 * pressed, as a fraction of the page size. */
#define E_DAY_VIEW_WHEEL_MOUSE_STEP_SIZE	0.25

/* The timeout before we do a layout, so we don't do a layout for each event
 * we get from the server. */
#define E_DAY_VIEW_LAYOUT_TIMEOUT	100

/* How many rows can be shown at a top_canvas; there will be always + 2 for
 * caption item and DnD space */
#define E_DAY_VIEW_MAX_ROWS_AT_TOP     6

struct _EDayViewPrivate {
	ECalModel *model;
	gulong notify_work_day_monday_handler_id;
	gulong notify_work_day_tuesday_handler_id;
	gulong notify_work_day_wednesday_handler_id;
	gulong notify_work_day_thursday_handler_id;
	gulong notify_work_day_friday_handler_id;
	gulong notify_work_day_saturday_handler_id;
	gulong notify_work_day_sunday_handler_id;
	gulong notify_week_start_day_handler_id;
	gulong notify_work_day_start_hour_handler_id;
	gulong notify_work_day_start_minute_handler_id;
	gulong notify_work_day_end_hour_handler_id;
	gulong notify_work_day_end_minute_handler_id;
	gulong notify_work_day_start_mon_handler_id;
	gulong notify_work_day_end_mon_handler_id;
	gulong notify_work_day_start_tue_handler_id;
	gulong notify_work_day_end_tue_handler_id;
	gulong notify_work_day_start_wed_handler_id;
	gulong notify_work_day_end_wed_handler_id;
	gulong notify_work_day_start_thu_handler_id;
	gulong notify_work_day_end_thu_handler_id;
	gulong notify_work_day_start_fri_handler_id;
	gulong notify_work_day_end_fri_handler_id;
	gulong notify_work_day_start_sat_handler_id;
	gulong notify_work_day_end_sat_handler_id;
	gulong notify_work_day_start_sun_handler_id;
	gulong notify_work_day_end_sun_handler_id;
	gulong time_range_changed_handler_id;
	gulong model_row_changed_handler_id;
	gulong model_cell_changed_handler_id;
	gulong model_rows_inserted_handler_id;
	gulong comps_deleted_handler_id;
	gulong timezone_changed_handler_id;

	/* "top_canvas" signal handlers */
	gulong top_canvas_button_press_event_handler_id;
	gulong top_canvas_button_release_event_handler_id;
	gulong top_canvas_scroll_event_handler_id;
	gulong top_canvas_motion_notify_event_handler_id;
	gulong top_canvas_drag_motion_handler_id;
	gulong top_canvas_drag_leave_handler_id;
	gulong top_canvas_drag_begin_handler_id;
	gulong top_canvas_drag_end_handler_id;
	gulong top_canvas_drag_data_get_handler_id;
	gulong top_canvas_drag_data_received_handler_id;

	/* "main_canvas" signal handlers */
	gulong main_canvas_realize_handler_id;
	gulong main_canvas_button_press_event_handler_id;
	gulong main_canvas_button_release_event_handler_id;
	gulong main_canvas_scroll_event_handler_id;
	gulong main_canvas_motion_notify_event_handler_id;
	gulong main_canvas_drag_motion_handler_id;
	gulong main_canvas_drag_leave_handler_id;
	gulong main_canvas_drag_begin_handler_id;
	gulong main_canvas_drag_end_handler_id;
	gulong main_canvas_drag_data_get_handler_id;
	gulong main_canvas_drag_data_received_handler_id;

	/* "time_canvas" signal handlers */
	gulong time_canvas_scroll_event_handler_id;

	/* Whether we are showing the work-week view. */
	gboolean work_week_view;

	/* The number of days we are shoing.  Usually 1 or 5, but can be
	 * up to E_DAY_VIEW_MAX_DAYS, e.g. when the user selects a range
	 * of days in the date navigator. */
	gint days_shown;

	/* Work days.  Indices are based on GDateWeekday.
	 * The first element (G_DATE_BAD_WEEKDAY) is unused. */
	gboolean work_days[G_DATE_SUNDAY + 1];

	/* Whether we show the Marcus Bains Line in the main
	 * canvas and time canvas, and the colors for each. */
	gboolean marcus_bains_show_line;
	gboolean marcus_bains_fix_timeout;
	gint marcus_bains_update_id;
	gchar *marcus_bains_day_view_color;
	gchar *marcus_bains_time_bar_color;

	GtkWidget *timezone_name_1_label; /* not referenced */
	GtkWidget *timezone_name_2_label; /* not referenced */

	GdkDragContext *drag_context;

	gchar *today_background_color;

	gboolean draw_flat_events;
	gboolean drag_event_is_recurring;
};

typedef struct {
	EDayView *day_view;
	ECalModelComponent *comp_data;
} AddEventData;

/* Drag and Drop stuff. */
static GtkTargetEntry target_table[] = {
	{ (gchar *) "application/x-e-calendar-event", 0, 0 }
};

static void e_day_view_set_colors (EDayView *day_view);
static gboolean e_day_view_update_scroll_regions (EDayView *day_view);
static gboolean e_day_view_get_next_tab_event (EDayView *day_view,
					       GtkDirectionType direction,
					       gint *day, gint *event_num);
static gboolean e_day_view_get_extreme_long_event (EDayView *day_view,
						   gboolean first,
						   gint *day_out,
						   gint *event_num_out);
static gboolean e_day_view_get_extreme_event (EDayView *day_view,
					      gint start_day,
					      gint end_day,
					      gboolean first,
					      gint *day_out,
					      gint *event_num_out);
static gboolean e_day_view_do_key_press (GtkWidget *widget,
					 GdkEventKey *event);
static void e_day_view_update_query (EDayView *day_view);
static void e_day_view_goto_start_of_work_day (EDayView *day_view);
static void e_day_view_goto_end_of_work_day (EDayView *day_view);
static void e_day_view_change_duration_to_start_of_work_day (EDayView *day_view);
static void e_day_view_change_duration_to_end_of_work_day (EDayView *day_view);
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
static void e_day_view_scroll	(EDayView	*day_view,
				 gfloat		 pages_to_scroll);

static void e_day_view_top_scroll (EDayView	*day_view,
				   gfloat	 pages_to_scroll);

static void e_day_view_update_top_scroll (EDayView *day_view, gboolean scroll_to_top);

static void e_day_view_on_canvas_realized (GtkWidget *widget,
					   EDayView *day_view);

static gboolean e_day_view_on_top_canvas_button_press (GtkWidget *widget,
						       GdkEvent *button_event,
						       EDayView *day_view);
static gboolean e_day_view_on_top_canvas_button_release (GtkWidget *widget,
							 GdkEvent *button_event,
							 EDayView *day_view);
static gboolean e_day_view_on_top_canvas_motion (GtkWidget *widget,
						 GdkEventMotion *event,
						 EDayView *day_view);

static gboolean e_day_view_on_main_canvas_button_press (GtkWidget *widget,
							GdkEvent *button_event,
							EDayView *day_view);
static gboolean e_day_view_on_main_canvas_button_release (GtkWidget *widget,
							  GdkEvent *button_event,
							  EDayView *day_view);

static gboolean e_day_view_on_top_canvas_scroll (GtkWidget *widget,
						 GdkEventScroll *scroll,
						 EDayView *day_view);

static gboolean e_day_view_on_main_canvas_scroll (GtkWidget *widget,
						  GdkEventScroll *scroll,
						  EDayView *day_view);
static gboolean e_day_view_on_time_canvas_scroll (GtkWidget *widget,
						  GdkEventScroll *scroll,
						  EDayView *day_view);
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
static void e_day_view_abort_resize (EDayView *day_view);

static gboolean e_day_view_on_long_event_button_press (EDayView		*day_view,
						       gint		 event_num,
						       GdkEvent *button_event,
						       ECalendarViewPosition  pos,
						       gint		 event_x,
						       gint		 event_y);
static gboolean e_day_view_on_event_button_press (EDayView	 *day_view,
						  gint		  day,
						  gint		  event_num,
						  GdkEvent *button_event,
						  ECalendarViewPosition pos,
						  gint		  event_x,
						  gint		  event_y);
static void e_day_view_on_long_event_click (EDayView *day_view,
					    gint event_num,
					    GdkEvent *button_event,
					    ECalendarViewPosition pos,
					    gint	     event_x,
					    gint	     event_y);
static void e_day_view_on_event_click (EDayView *day_view,
				       gint day,
				       gint event_num,
				       GdkEvent *button_event,
				       ECalendarViewPosition pos,
				       gint		event_x,
				       gint		event_y);
static void e_day_view_on_event_double_click (EDayView *day_view,
					      gint day,
					      gint event_num);
static void e_day_view_on_event_right_click (EDayView *day_view,
					     GdkEvent *button_event,
					     gint day,
					     gint event_num);
static void e_day_view_show_popup_menu (EDayView *day_view,
					GdkEvent *button_event,
					gint day,
					gint event_num);

static void e_day_view_recalc_day_starts (EDayView *day_view,
					  time_t start_time);
static void e_day_view_recalc_num_rows	(EDayView	*day_view);
static void e_day_view_recalc_cell_sizes	(EDayView	*day_view);

static ECalendarViewPosition e_day_view_convert_position_in_top_canvas (EDayView *day_view,
								   gint x,
								   gint y,
								   gint *day_return,
								   gint *event_num_return);
static ECalendarViewPosition e_day_view_convert_position_in_main_canvas (EDayView *day_view,
								    gint x,
								    gint y,
								    gint *day_return,
								    gint *row_return,
								    gint *event_num_return);
static gboolean e_day_view_find_event_from_uid (EDayView *day_view,
						ECalClient *client,
						const gchar *uid,
						const gchar *rid,
						gint *day_return,
						gint *event_num_return);

typedef gboolean (* EDayViewForeachEventCallback) (EDayView *day_view,
						   gint day,
						   gint event_num,
						   gpointer data);

static void e_day_view_foreach_event		(EDayView	*day_view,
						 EDayViewForeachEventCallback callback,
						 gpointer	 data);
static void e_day_view_foreach_event_with_uid (EDayView *day_view,
					       const gchar *uid,
					       EDayViewForeachEventCallback callback,
					       gpointer data);

static void e_day_view_free_events (EDayView *day_view);
static void e_day_view_free_event_array (EDayView *day_view,
					 GArray *array);
static void e_day_view_add_event (ESourceRegistry *registry,
				 ECalClient *client,
				 ECalComponent *comp,
				 time_t	  start,
				 time_t	  end,
				 gpointer data);
static void e_day_view_update_event_label (EDayView *day_view,
					   gint day,
					   gint event_num);
static void e_day_view_update_long_event_label (EDayView *day_view,
						gint event_num);

static void e_day_view_reshape_long_events (EDayView *day_view);
static void e_day_view_reshape_long_event (EDayView *day_view,
					   gint event_num);
static void e_day_view_reshape_day_events (EDayView *day_view,
					   gint day);
static void e_day_view_reshape_day_event (EDayView *day_view,
					  gint	day,
					  gint	event_num);
static void e_day_view_reshape_main_canvas_resize_bars (EDayView *day_view);

static void e_day_view_ensure_events_sorted (EDayView *day_view);

static void e_day_view_start_editing_event (EDayView *day_view,
					    gint day,
					    gint event_num,
					    GdkEventKey *key_event);
static void e_day_view_stop_editing_event (EDayView *day_view);
static void cancel_editing (EDayView *day_view);
static gboolean e_day_view_on_text_item_event (GnomeCanvasItem *item,
					       GdkEvent *event,
					       EDayView *day_view);
static gboolean e_day_view_event_move (ECalendarView *cal_view, ECalViewMoveDirection direction);
static void e_day_view_change_event_time (EDayView *day_view, time_t start_dt,
time_t end_dt);
static void e_day_view_change_event_end_time_up (EDayView *day_view);
static void e_day_view_change_event_end_time_down (EDayView *day_view);
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

static gboolean e_day_view_on_top_canvas_drag_motion (GtkWidget      *widget,
						      GdkDragContext *context,
						      gint            x,
						      gint            y,
						      guint           time,
						      EDayView	  *day_view);
static void e_day_view_update_top_canvas_drag (EDayView *day_view,
					       gint day);
static void e_day_view_reshape_top_canvas_drag_item (EDayView *day_view);
static gboolean e_day_view_on_main_canvas_drag_motion (GtkWidget      *widget,
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

static gboolean e_day_view_remove_event_cb (EDayView *day_view,
					    gint day,
					    gint event_num,
					    gpointer data);
static void e_day_view_normalize_selection (EDayView *day_view);
static gboolean e_day_view_set_show_times_cb	(EDayView	*day_view,
						 gint		 day,
						 gint		 event_num,
						 gpointer	 data);
static time_t e_day_view_find_work_week_start	(EDayView	*day_view,
						 time_t		 start_time);
static void e_day_view_recalc_work_week		(EDayView	*day_view);
static void e_day_view_recalc_work_week_days_shown	(EDayView	*day_view);

static void e_day_view_precalc_visible_time_range (ECalendarView *cal_view,
						   time_t in_start_time,
						   time_t in_end_time,
						   time_t *out_start_time,
						   time_t *out_end_time);
static void e_day_view_queue_layout (EDayView *day_view);
static void e_day_view_cancel_layout (EDayView *day_view);
static gboolean e_day_view_layout_timeout_cb (gpointer data);

enum {
	PROP_0,
	PROP_DRAW_FLAT_EVENTS,
	PROP_MARCUS_BAINS_SHOW_LINE,
	PROP_MARCUS_BAINS_DAY_VIEW_COLOR,
	PROP_MARCUS_BAINS_TIME_BAR_COLOR,
	PROP_TODAY_BACKGROUND_COLOR,
	PROP_IS_EDITING
};

G_DEFINE_TYPE_WITH_PRIVATE (EDayView, e_day_view, E_TYPE_CALENDAR_VIEW)

static gboolean
day_view_refresh_marcus_bains_line (gpointer user_data)
{
	EDayView *day_view = user_data;
	ICalTime *now;
	gint hh, mm, seconds = 0;

	if (!day_view->priv->marcus_bains_show_line ||
	    !E_CALENDAR_VIEW (day_view)->in_focus) {
		if (day_view->priv->marcus_bains_update_id) {
			g_source_remove (day_view->priv->marcus_bains_update_id);
			day_view->priv->marcus_bains_update_id = 0;
			day_view->priv->marcus_bains_fix_timeout = FALSE;
		}
		return FALSE;
	}

	e_day_view_marcus_bains_update (day_view);

	now = i_cal_time_new_current_with_zone (e_cal_model_get_timezone (day_view->priv->model));
	i_cal_time_get_time (now, &hh, &mm, &seconds);
	g_clear_object (&now);

	if (seconds > 1) {
		if (day_view->priv->marcus_bains_update_id)
			g_source_remove (day_view->priv->marcus_bains_update_id);

		day_view->priv->marcus_bains_fix_timeout = TRUE;
		day_view->priv->marcus_bains_update_id = g_timeout_add_seconds (60 - seconds, day_view_refresh_marcus_bains_line, day_view);
	} else if (day_view->priv->marcus_bains_fix_timeout || !day_view->priv->marcus_bains_update_id) {
		day_view->priv->marcus_bains_fix_timeout = FALSE;
		day_view->priv->marcus_bains_update_id = g_timeout_add_seconds (60, day_view_refresh_marcus_bains_line, day_view);
	} else {
		return TRUE;
	}

	return FALSE;
}

static void
e_day_view_set_popup_event (EDayView *day_view,
			    gint day,
			    gint event_num)
{
	if (day_view->popup_event_day != day ||
	    day_view->popup_event_num != event_num) {
		day_view->popup_event_day = day;
		day_view->popup_event_num = event_num;

		g_signal_emit_by_name (day_view, "selection-changed");
	}
}

static void
day_view_notify_time_divisions_cb (EDayView *day_view)
{
	gint day;

	e_day_view_recalc_num_rows (day_view);

	/* If we aren't visible, we'll sort it out later. */
	if (!E_CALENDAR_VIEW (day_view)->in_focus) {
		e_day_view_free_events (day_view);
		day_view->requires_update = TRUE;
		return;
	}

	for (day = 0; day < E_DAY_VIEW_MAX_DAYS; day++)
		day_view->need_layout[day] = TRUE;

	/* We need to update all the day event labels since the start & end
	 * times may or may not be on row boundaries any more. */
	e_day_view_foreach_event (day_view,
				  e_day_view_set_show_times_cb, NULL);

	/* We must layout the events before updating the scroll region, since
	 * that will result in a redraw which would crash otherwise. */
	e_day_view_check_layout (day_view);
	gtk_widget_queue_draw (day_view->time_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);

	e_day_view_update_scroll_regions (day_view);
}

static void
day_view_notify_week_start_day_cb (EDayView *day_view)
{
	/* FIXME Write an EWorkWeekView subclass, like EMonthView. */

	if (day_view->priv->work_week_view)
		e_day_view_recalc_work_week (day_view);
}

static void
day_view_notify_work_day_cb (ECalModel *model,
                             GParamSpec *pspec,
                             EDayView *day_view)
{
	/* FIXME Write an EWorkWeekView subclass, like EMonthView. */

	if (day_view->priv->work_week_view)
		e_day_view_recalc_work_week (day_view);

	/* We have to do this, as the new working days may have no effect on
	 * the days shown, but we still want the background color to change. */
	gtk_widget_queue_draw (day_view->main_canvas);
}

static void
e_day_view_get_work_day_range_for_day (EDayView *day_view,
				       gint day,
				       gint *start_hour,
				       gint *start_minute,
				       gint *end_hour,
				       gint *end_minute)
{
	ECalModel *model;

	g_return_if_fail (E_IS_DAY_VIEW (day_view));
	g_return_if_fail (start_hour != NULL);
	g_return_if_fail (start_minute != NULL);
	g_return_if_fail (end_hour != NULL);
	g_return_if_fail (end_minute != NULL);

	model = e_calendar_view_get_model (E_CALENDAR_VIEW (day_view));

	if (day >= 0 && day < e_day_view_get_days_shown (day_view)) {
		GDateWeekday weekday;
		ICalTime *tt;

		tt = i_cal_time_new_from_timet_with_zone (day_view->day_starts[day], FALSE,
			e_calendar_view_get_timezone (E_CALENDAR_VIEW (day_view)));

		switch (i_cal_time_day_of_week (tt)) {
			case 1:
				weekday = G_DATE_SUNDAY;
				break;
			case 2:
				weekday = G_DATE_MONDAY;
				break;
			case 3:
				weekday = G_DATE_TUESDAY;
				break;
			case 4:
				weekday = G_DATE_WEDNESDAY;
				break;
			case 5:
				weekday = G_DATE_THURSDAY;
				break;
			case 6:
				weekday = G_DATE_FRIDAY;
				break;
			case 7:
				weekday = G_DATE_SATURDAY;
				break;
			default:
				weekday = G_DATE_BAD_WEEKDAY;
				break;
		}

		g_clear_object (&tt);

		e_cal_model_get_work_day_range_for (model, weekday,
			start_hour, start_minute,
			end_hour, end_minute);
	} else {
		*start_hour = e_cal_model_get_work_day_start_hour (model);
		*start_minute = e_cal_model_get_work_day_start_minute (model);
		*end_hour = e_cal_model_get_work_day_end_hour (model);
		*end_minute = e_cal_model_get_work_day_end_minute (model);
	}
}

static void
e_day_view_recalc_main_canvas_size (EDayView *day_view)
{
	gint day, scroll_y;
	gboolean need_reshape;

	/* Set the scroll region of the top canvas */
	e_day_view_update_top_scroll (day_view, TRUE);

	need_reshape = e_day_view_update_scroll_regions (day_view);

	e_day_view_recalc_cell_sizes (day_view);

	/* Scroll to the start of the working day, if this is the initial
	 * allocation. */
	if (day_view->scroll_to_work_day) {
		gint work_day_start_hour;
		gint work_day_start_minute;
		gint work_day_end_hour;
		gint work_day_end_minute;

		e_day_view_get_work_day_range_for_day (day_view, 0,
			&work_day_start_hour, &work_day_start_minute,
			&work_day_end_hour, &work_day_end_minute);

		scroll_y = e_day_view_convert_time_to_position (
			day_view, work_day_start_hour, work_day_start_minute);
		gnome_canvas_scroll_to (
			GNOME_CANVAS (day_view->main_canvas), 0, scroll_y);
		day_view->scroll_to_work_day = FALSE;
	}

	/* Flag that we need to reshape the events. Note that changes in height
	 * don't matter, since the rows are always the same height. */
	if (need_reshape) {
		day_view->long_events_need_reshape = TRUE;
		for (day = 0; day < E_DAY_VIEW_MAX_DAYS; day++)
			day_view->need_reshape[day] = TRUE;

		e_day_view_check_layout (day_view);
	}
}

static GdkRGBA
e_day_view_get_text_color (EDayView *day_view,
                           EDayViewEvent *event)
{
	GdkRGBA rgba;

	if (!is_comp_data_valid (event) ||
	    !e_cal_model_get_rgba_for_component (e_calendar_view_get_model (E_CALENDAR_VIEW (day_view)), event->comp_data, &rgba)) {
		rgba = day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND];
	}

	return e_utils_get_text_color_for_background (&rgba);
}

/* Returns the selected time range. */
static gboolean
day_view_get_selected_time_range (ECalendarView *cal_view,
                                  time_t *start_time,
                                  time_t *end_time)
{
	gint start_col, start_row, end_col, end_row;
	time_t start, end;
	EDayView *day_view = E_DAY_VIEW (cal_view);

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
	 * we can simply use the day_starts array. */
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

	return TRUE;
}

typedef struct {
	EDayView *day_view;
	GdkEventKey *key_event;
	time_t dtstart, dtend;
	gboolean in_top_canvas;
	gboolean paste_clipboard;
} NewEventInRangeData;

static void
new_event_in_rage_data_free (gpointer ptr)
{
	NewEventInRangeData *ned = ptr;

	if (ned) {
		g_clear_object (&ned->day_view);
		g_slice_free (GdkEventKey, ned->key_event);
		g_slice_free (NewEventInRangeData, ned);
	}
}

static void
day_view_new_event_in_selected_range_cb (ECalModel *model,
					 ECalClient *client,
					 ICalComponent *default_component,
					 gpointer user_data)
{
	NewEventInRangeData *ned = user_data;
	ECalComponent *comp = NULL;
	gint day, event_num;
	ECalComponentDateTime *start_dt, *end_dt;
	ICalTime *start_tt, *end_tt;
	const gchar *uid, *use_tzid;
	AddEventData add_event_data;
	ESourceRegistry *registry;
	ICalTimezone *zone;

	g_return_if_fail (ned != NULL);
	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_CAL_CLIENT (client));
	g_return_if_fail (default_component != NULL);

	/* Check if the client is read only */
	if (e_client_is_readonly (E_CLIENT (client)))
		return;

	registry = e_cal_model_get_registry (model);
	zone = e_cal_model_get_timezone (model);
	uid = i_cal_component_get_uid (default_component);

	comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (default_component));
	g_return_if_fail (comp != NULL);

	start_tt = i_cal_time_new_from_timet_with_zone (ned->dtstart, FALSE, zone);
	end_tt = i_cal_time_new_from_timet_with_zone (ned->dtend, FALSE, zone);

	if (ned->in_top_canvas) {
		use_tzid = NULL;
		i_cal_time_set_is_date (start_tt, 1);
		i_cal_time_set_is_date (end_tt, 1);

		/* Editor default in day/work-week view - top canvas */
		e_cal_component_set_transparency (comp, E_CAL_COMPONENT_TRANSP_TRANSPARENT);
	} else {
		use_tzid = i_cal_timezone_get_tzid (zone);

		/* Editor default in day/work-week view - main canvas */
		e_cal_component_set_transparency (comp, E_CAL_COMPONENT_TRANSP_OPAQUE);
	}

	start_dt = e_cal_component_datetime_new_take (start_tt, g_strdup (use_tzid));
	end_dt = e_cal_component_datetime_new_take (end_tt, g_strdup (use_tzid));

	e_cal_component_set_dtstart (comp, start_dt);
	e_cal_component_set_dtend (comp, end_dt);

	e_cal_component_datetime_free (start_dt);
	e_cal_component_datetime_free (end_dt);

	/* We add the event locally and start editing it. We don't send it
	 * to the server until the user finishes editing it. */
	add_event_data.day_view = ned->day_view;
	add_event_data.comp_data = NULL;
	e_day_view_add_event (registry, client, comp, ned->dtstart, ned->dtend, &add_event_data);
	e_day_view_check_layout (ned->day_view);
	gtk_widget_queue_draw (ned->day_view->top_canvas);
	gtk_widget_queue_draw (ned->day_view->main_canvas);

	if (!e_day_view_find_event_from_uid (ned->day_view, client, uid, NULL, &day, &event_num)) {
		g_warning ("Couldn't find event to start editing.\n");
	} else {
		e_day_view_start_editing_event (ned->day_view, day, event_num, ned->key_event);

		if (ned->paste_clipboard) {
			EDayViewEvent *event;

			g_clear_object (&comp);

			if (ned->day_view->editing_event_day == E_DAY_VIEW_LONG_EVENT) {
				if (!is_array_index_in_bounds (ned->day_view->long_events, ned->day_view->editing_event_num))
					goto out;

				event = &g_array_index (ned->day_view->long_events,
							EDayViewEvent,
							ned->day_view->editing_event_num);
			} else {
				if (!is_array_index_in_bounds (ned->day_view->events[ned->day_view->editing_event_day], ned->day_view->editing_event_num))
					goto out;

				event = &g_array_index (ned->day_view->events[ned->day_view->editing_event_day],
							EDayViewEvent,
							ned->day_view->editing_event_num);
			}

			if (event->canvas_item &&
			    E_IS_TEXT (event->canvas_item) &&
			    E_TEXT (event->canvas_item)->editing) {
				e_text_paste_clipboard (E_TEXT (event->canvas_item));
			}
		}
	}

 out:
	g_clear_object (&comp);
}

static void
e_day_view_add_new_event_in_selected_range (EDayView *day_view,
                                            GdkEventKey *key_event,
					    gboolean paste_clipboard)
{
	NewEventInRangeData *ned;
	ECalModel *model;
	const gchar *source_uid;

	ned = g_slice_new0 (NewEventInRangeData);
	ned->day_view = g_object_ref (day_view);
	if (key_event) {
		ned->key_event = g_slice_new0 (GdkEventKey);
		*ned->key_event = *key_event;
	}
	day_view_get_selected_time_range (E_CALENDAR_VIEW (day_view), &ned->dtstart, &ned->dtend);
	ned->in_top_canvas = day_view->selection_in_top_canvas;
	ned->paste_clipboard = paste_clipboard;

	model = e_calendar_view_get_model (E_CALENDAR_VIEW (day_view));
	source_uid = e_cal_model_get_default_source_uid (model);

	e_cal_ops_get_default_component	 (model, source_uid, ned->in_top_canvas,
		day_view_new_event_in_selected_range_cb, ned, new_event_in_rage_data_free);
}

static void
day_view_set_property (GObject *object,
                       guint property_id,
                       const GValue *value,
                       GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DRAW_FLAT_EVENTS:
			e_day_view_set_draw_flat_events (
				E_DAY_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_MARCUS_BAINS_SHOW_LINE:
			e_day_view_marcus_bains_set_show_line (
				E_DAY_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_MARCUS_BAINS_DAY_VIEW_COLOR:
			e_day_view_marcus_bains_set_day_view_color (
				E_DAY_VIEW (object),
				g_value_get_string (value));
			return;

		case PROP_MARCUS_BAINS_TIME_BAR_COLOR:
			e_day_view_marcus_bains_set_time_bar_color (
				E_DAY_VIEW (object),
				g_value_get_string (value));
			return;

		case PROP_TODAY_BACKGROUND_COLOR:
			e_day_view_set_today_background_color (
				E_DAY_VIEW (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
day_view_get_property (GObject *object,
                       guint property_id,
                       GValue *value,
                       GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DRAW_FLAT_EVENTS:
			g_value_set_boolean (
				value,
				e_day_view_get_draw_flat_events (
				E_DAY_VIEW (object)));
			return;

		case PROP_MARCUS_BAINS_SHOW_LINE:
			g_value_set_boolean (
				value,
				e_day_view_marcus_bains_get_show_line (
				E_DAY_VIEW (object)));
			return;

		case PROP_MARCUS_BAINS_DAY_VIEW_COLOR:
			g_value_set_string (
				value,
				e_day_view_marcus_bains_get_day_view_color (
				E_DAY_VIEW (object)));
			return;

		case PROP_MARCUS_BAINS_TIME_BAR_COLOR:
			g_value_set_string (
				value,
				e_day_view_marcus_bains_get_time_bar_color (
				E_DAY_VIEW (object)));
			return;

		case PROP_TODAY_BACKGROUND_COLOR:
			g_value_set_string (
				value,
				e_day_view_get_today_background_color (
				E_DAY_VIEW (object)));
			return;

		case PROP_IS_EDITING:
			g_value_set_boolean (value, e_day_view_is_editing (E_DAY_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
day_view_dispose (GObject *object)
{
	EDayView *day_view;
	gint day;

	day_view = E_DAY_VIEW (object);

	if (day_view->priv->marcus_bains_update_id) {
		g_source_remove (day_view->priv->marcus_bains_update_id);
		day_view->priv->marcus_bains_update_id = 0;
	}

	e_day_view_cancel_layout (day_view);

	e_day_view_stop_auto_scroll (day_view);

	g_clear_pointer (&day_view->large_font_desc, pango_font_description_free);
	g_clear_pointer (&day_view->small_font_desc, pango_font_description_free);
	g_clear_object (&day_view->normal_cursor);
	g_clear_object (&day_view->move_cursor);
	g_clear_object (&day_view->resize_width_cursor);
	g_clear_object (&day_view->resize_height_cursor);

	if (day_view->long_events) {
		e_day_view_free_events (day_view);
		g_array_free (day_view->long_events, TRUE);
		day_view->long_events = NULL;
	}

	for (day = 0; day < E_DAY_VIEW_MAX_DAYS; day++) {
		if (day_view->events[day]) {
			g_array_free (day_view->events[day], TRUE);
			day_view->events[day] = NULL;
		}
	}

	if (day_view->grabbed_pointer != NULL) {
		gdk_device_ungrab (
			day_view->grabbed_pointer,
			GDK_CURRENT_TIME);
		g_object_unref (day_view->grabbed_pointer);
		day_view->grabbed_pointer = NULL;
	}

	#define disconnect_model_handler(x) G_STMT_START { \
		if ((x) > 0) { \
			g_signal_handler_disconnect (day_view->priv->model, (x)); \
			(x) = 0; \
		} \
	} G_STMT_END

	disconnect_model_handler (day_view->priv->notify_work_day_monday_handler_id);
	disconnect_model_handler (day_view->priv->notify_work_day_tuesday_handler_id);
	disconnect_model_handler (day_view->priv->notify_work_day_wednesday_handler_id);
	disconnect_model_handler (day_view->priv->notify_work_day_thursday_handler_id);
	disconnect_model_handler (day_view->priv->notify_work_day_friday_handler_id);
	disconnect_model_handler (day_view->priv->notify_work_day_saturday_handler_id);
	disconnect_model_handler (day_view->priv->notify_work_day_sunday_handler_id);
	disconnect_model_handler (day_view->priv->notify_week_start_day_handler_id);
	disconnect_model_handler (day_view->priv->notify_work_day_start_hour_handler_id);
	disconnect_model_handler (day_view->priv->notify_work_day_start_minute_handler_id);
	disconnect_model_handler (day_view->priv->notify_work_day_end_hour_handler_id);
	disconnect_model_handler (day_view->priv->notify_work_day_end_minute_handler_id);
	disconnect_model_handler (day_view->priv->notify_work_day_start_mon_handler_id);
	disconnect_model_handler (day_view->priv->notify_work_day_end_mon_handler_id);
	disconnect_model_handler (day_view->priv->notify_work_day_start_tue_handler_id);
	disconnect_model_handler (day_view->priv->notify_work_day_end_tue_handler_id);
	disconnect_model_handler (day_view->priv->notify_work_day_start_wed_handler_id);
	disconnect_model_handler (day_view->priv->notify_work_day_end_wed_handler_id);
	disconnect_model_handler (day_view->priv->notify_work_day_start_thu_handler_id);
	disconnect_model_handler (day_view->priv->notify_work_day_end_thu_handler_id);
	disconnect_model_handler (day_view->priv->notify_work_day_start_fri_handler_id);
	disconnect_model_handler (day_view->priv->notify_work_day_end_fri_handler_id);
	disconnect_model_handler (day_view->priv->notify_work_day_start_sat_handler_id);
	disconnect_model_handler (day_view->priv->notify_work_day_end_sat_handler_id);
	disconnect_model_handler (day_view->priv->notify_work_day_start_sun_handler_id);
	disconnect_model_handler (day_view->priv->notify_work_day_end_sun_handler_id);
	disconnect_model_handler (day_view->priv->time_range_changed_handler_id);
	disconnect_model_handler (day_view->priv->model_row_changed_handler_id);
	disconnect_model_handler (day_view->priv->model_cell_changed_handler_id);
	disconnect_model_handler (day_view->priv->model_rows_inserted_handler_id);
	disconnect_model_handler (day_view->priv->comps_deleted_handler_id);
	disconnect_model_handler (day_view->priv->timezone_changed_handler_id);

	#undef disconnect_model_handler

	if (day_view->priv->top_canvas_button_press_event_handler_id > 0) {
		g_signal_handler_disconnect (
			day_view->top_canvas,
			day_view->priv->top_canvas_button_press_event_handler_id);
		day_view->priv->top_canvas_button_press_event_handler_id = 0;
	}

	if (day_view->priv->top_canvas_button_release_event_handler_id > 0) {
		g_signal_handler_disconnect (
			day_view->top_canvas,
			day_view->priv->top_canvas_button_release_event_handler_id);
		day_view->priv->top_canvas_button_release_event_handler_id = 0;
	}

	if (day_view->priv->top_canvas_scroll_event_handler_id > 0) {
		g_signal_handler_disconnect (
			day_view->top_canvas,
			day_view->priv->top_canvas_scroll_event_handler_id);
		day_view->priv->top_canvas_scroll_event_handler_id = 0;
	}

	if (day_view->priv->top_canvas_motion_notify_event_handler_id > 0) {
		g_signal_handler_disconnect (
			day_view->top_canvas,
			day_view->priv->top_canvas_motion_notify_event_handler_id);
		day_view->priv->top_canvas_motion_notify_event_handler_id = 0;
	}

	if (day_view->priv->top_canvas_drag_motion_handler_id > 0) {
		g_signal_handler_disconnect (
			day_view->top_canvas,
			day_view->priv->top_canvas_drag_motion_handler_id);
		day_view->priv->top_canvas_drag_motion_handler_id = 0;
	}

	if (day_view->priv->top_canvas_drag_leave_handler_id > 0) {
		g_signal_handler_disconnect (
			day_view->top_canvas,
			day_view->priv->top_canvas_drag_leave_handler_id);
		day_view->priv->top_canvas_drag_leave_handler_id = 0;
	}

	if (day_view->priv->top_canvas_drag_begin_handler_id > 0) {
		g_signal_handler_disconnect (
			day_view->top_canvas,
			day_view->priv->top_canvas_drag_begin_handler_id);
		day_view->priv->top_canvas_drag_begin_handler_id = 0;
	}

	if (day_view->priv->top_canvas_drag_end_handler_id > 0) {
		g_signal_handler_disconnect (
			day_view->top_canvas,
			day_view->priv->top_canvas_drag_end_handler_id);
		day_view->priv->top_canvas_drag_end_handler_id = 0;
	}

	if (day_view->priv->top_canvas_drag_data_get_handler_id > 0) {
		g_signal_handler_disconnect (
			day_view->top_canvas,
			day_view->priv->top_canvas_drag_data_get_handler_id);
		day_view->priv->top_canvas_drag_data_get_handler_id = 0;
	}

	if (day_view->priv->top_canvas_drag_data_received_handler_id > 0) {
		g_signal_handler_disconnect (
			day_view->top_canvas,
			day_view->priv->top_canvas_drag_data_received_handler_id);
		day_view->priv->top_canvas_drag_data_received_handler_id = 0;
	}

	if (day_view->priv->main_canvas_realize_handler_id > 0) {
		g_signal_handler_disconnect (
			day_view->main_canvas,
			day_view->priv->main_canvas_realize_handler_id);
		day_view->priv->main_canvas_realize_handler_id = 0;
	}

	if (day_view->priv->main_canvas_button_press_event_handler_id > 0) {
		g_signal_handler_disconnect (
			day_view->main_canvas,
			day_view->priv->main_canvas_button_press_event_handler_id);
		day_view->priv->main_canvas_button_press_event_handler_id = 0;
	}

	if (day_view->priv->main_canvas_button_release_event_handler_id > 0) {
		g_signal_handler_disconnect (
			day_view->main_canvas,
			day_view->priv->main_canvas_button_release_event_handler_id);
		day_view->priv->main_canvas_button_release_event_handler_id = 0;
	}

	if (day_view->priv->main_canvas_scroll_event_handler_id > 0) {
		g_signal_handler_disconnect (
			day_view->main_canvas,
			day_view->priv->main_canvas_scroll_event_handler_id);
		day_view->priv->main_canvas_scroll_event_handler_id = 0;
	}

	if (day_view->priv->main_canvas_motion_notify_event_handler_id > 0) {
		g_signal_handler_disconnect (
			day_view->main_canvas,
			day_view->priv->main_canvas_motion_notify_event_handler_id);
		day_view->priv->main_canvas_motion_notify_event_handler_id = 0;
	}

	if (day_view->priv->main_canvas_drag_motion_handler_id > 0) {
		g_signal_handler_disconnect (
			day_view->main_canvas,
			day_view->priv->main_canvas_drag_motion_handler_id);
		day_view->priv->main_canvas_drag_motion_handler_id = 0;
	}

	if (day_view->priv->main_canvas_drag_leave_handler_id > 0) {
		g_signal_handler_disconnect (
			day_view->main_canvas,
			day_view->priv->main_canvas_drag_leave_handler_id);
		day_view->priv->main_canvas_drag_leave_handler_id = 0;
	}

	if (day_view->priv->main_canvas_drag_begin_handler_id > 0) {
		g_signal_handler_disconnect (
			day_view->main_canvas,
			day_view->priv->main_canvas_drag_begin_handler_id);
		day_view->priv->main_canvas_drag_begin_handler_id = 0;
	}

	if (day_view->priv->main_canvas_drag_end_handler_id > 0) {
		g_signal_handler_disconnect (
			day_view->main_canvas,
			day_view->priv->main_canvas_drag_end_handler_id);
		day_view->priv->main_canvas_drag_end_handler_id = 0;
	}

	if (day_view->priv->main_canvas_drag_data_get_handler_id > 0) {
		g_signal_handler_disconnect (
			day_view->main_canvas,
			day_view->priv->main_canvas_drag_data_get_handler_id);
		day_view->priv->main_canvas_drag_data_get_handler_id = 0;
	}

	if (day_view->priv->main_canvas_drag_data_received_handler_id > 0) {
		g_signal_handler_disconnect (
			day_view->main_canvas,
			day_view->priv->main_canvas_drag_data_received_handler_id);
		day_view->priv->main_canvas_drag_data_received_handler_id = 0;
	}

	if (day_view->priv->time_canvas_scroll_event_handler_id > 0) {
		g_signal_handler_disconnect (
			day_view->time_canvas,
			day_view->priv->time_canvas_scroll_event_handler_id);
		day_view->priv->time_canvas_scroll_event_handler_id = 0;
	}

	g_clear_object (&day_view->top_canvas);
	g_clear_object (&day_view->main_canvas);
	g_clear_object (&day_view->time_canvas);
	g_clear_object (&day_view->priv->model);
	g_clear_object (&day_view->priv->drag_context);

	g_clear_pointer (&day_view->priv->marcus_bains_day_view_color, g_free);
	g_clear_pointer (&day_view->priv->marcus_bains_time_bar_color, g_free);
	g_clear_pointer (&day_view->priv->today_background_color, g_free);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_day_view_parent_class)->dispose (object);
}

static void
day_view_notify (GObject *object,
                 GParamSpec *pspec)
{
	/* Don't chain up.  None of our parent classes, not
	 * even GObjectClass itself, implements this method. */

	if (g_str_equal (pspec->name, "time-divisions"))
		day_view_notify_time_divisions_cb (E_DAY_VIEW (object));
}

static void
day_view_constructed (GObject *object)
{
	EDayView *day_view;
	ECalModel *model;
	gulong handler_id;

	day_view = E_DAY_VIEW (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_day_view_parent_class)->constructed (object);

	model = e_calendar_view_get_model (E_CALENDAR_VIEW (day_view));

	/* Keep our own model reference so we can
	 * disconnect signal handlers in dispose(). */
	day_view->priv->model = g_object_ref (model);

	handler_id = e_signal_connect_notify (
		model, "notify::work-day-monday",
		G_CALLBACK (day_view_notify_work_day_cb), day_view);
	day_view->priv->notify_work_day_monday_handler_id = handler_id;

	handler_id = e_signal_connect_notify (
		model, "notify::work-day-tuesday",
		G_CALLBACK (day_view_notify_work_day_cb), day_view);
	day_view->priv->notify_work_day_tuesday_handler_id = handler_id;

	handler_id = e_signal_connect_notify (
		model, "notify::work-day-wednesday",
		G_CALLBACK (day_view_notify_work_day_cb), day_view);
	day_view->priv->notify_work_day_wednesday_handler_id = handler_id;

	handler_id = e_signal_connect_notify (
		model, "notify::work-day-thursday",
		G_CALLBACK (day_view_notify_work_day_cb), day_view);
	day_view->priv->notify_work_day_thursday_handler_id = handler_id;

	handler_id = e_signal_connect_notify (
		model, "notify::work-day-friday",
		G_CALLBACK (day_view_notify_work_day_cb), day_view);
	day_view->priv->notify_work_day_friday_handler_id = handler_id;

	handler_id = e_signal_connect_notify (
		model, "notify::work-day-saturday",
		G_CALLBACK (day_view_notify_work_day_cb), day_view);
	day_view->priv->notify_work_day_saturday_handler_id = handler_id;

	handler_id = e_signal_connect_notify (
		model, "notify::work-day-sunday",
		G_CALLBACK (day_view_notify_work_day_cb), day_view);
	day_view->priv->notify_work_day_sunday_handler_id = handler_id;

	handler_id = e_signal_connect_notify_swapped (
		model, "notify::week-start-day",
		G_CALLBACK (day_view_notify_week_start_day_cb), day_view);
	day_view->priv->notify_week_start_day_handler_id = handler_id;

	handler_id = e_signal_connect_notify_swapped (
		model, "notify::work-day-start-hour",
		G_CALLBACK (gtk_widget_queue_draw), day_view->main_canvas);
	day_view->priv->notify_work_day_start_hour_handler_id = handler_id;

	handler_id = e_signal_connect_notify_swapped (
		model, "notify::work-day-start-minute",
		G_CALLBACK (gtk_widget_queue_draw), day_view->main_canvas);
	day_view->priv->notify_work_day_start_minute_handler_id = handler_id;

	handler_id = e_signal_connect_notify_swapped (
		model, "notify::work-day-end-hour",
		G_CALLBACK (gtk_widget_queue_draw), day_view->main_canvas);
	day_view->priv->notify_work_day_end_hour_handler_id = handler_id;

	handler_id = e_signal_connect_notify_swapped (
		model, "notify::work-day-end-minute",
		G_CALLBACK (gtk_widget_queue_draw), day_view->main_canvas);
	day_view->priv->notify_work_day_end_minute_handler_id = handler_id;

	handler_id = e_signal_connect_notify_swapped (
		model, "notify::work-day-start-mon",
		G_CALLBACK (gtk_widget_queue_draw), day_view->main_canvas);
	day_view->priv->notify_work_day_start_mon_handler_id = handler_id;

	handler_id = e_signal_connect_notify_swapped (
		model, "notify::work-day-end-mon",
		G_CALLBACK (gtk_widget_queue_draw), day_view->main_canvas);
	day_view->priv->notify_work_day_end_mon_handler_id = handler_id;

	handler_id = e_signal_connect_notify_swapped (
		model, "notify::work-day-start-tue",
		G_CALLBACK (gtk_widget_queue_draw), day_view->main_canvas);
	day_view->priv->notify_work_day_start_tue_handler_id = handler_id;

	handler_id = e_signal_connect_notify_swapped (
		model, "notify::work-day-end-tue",
		G_CALLBACK (gtk_widget_queue_draw), day_view->main_canvas);
	day_view->priv->notify_work_day_end_tue_handler_id = handler_id;

	handler_id = e_signal_connect_notify_swapped (
		model, "notify::work-day-start-wed",
		G_CALLBACK (gtk_widget_queue_draw), day_view->main_canvas);
	day_view->priv->notify_work_day_start_wed_handler_id = handler_id;

	handler_id = e_signal_connect_notify_swapped (
		model, "notify::work-day-end-wed",
		G_CALLBACK (gtk_widget_queue_draw), day_view->main_canvas);
	day_view->priv->notify_work_day_end_wed_handler_id = handler_id;

	handler_id = e_signal_connect_notify_swapped (
		model, "notify::work-day-start-thu",
		G_CALLBACK (gtk_widget_queue_draw), day_view->main_canvas);
	day_view->priv->notify_work_day_start_thu_handler_id = handler_id;

	handler_id = e_signal_connect_notify_swapped (
		model, "notify::work-day-end-thu",
		G_CALLBACK (gtk_widget_queue_draw), day_view->main_canvas);
	day_view->priv->notify_work_day_end_thu_handler_id = handler_id;

	handler_id = e_signal_connect_notify_swapped (
		model, "notify::work-day-start-fri",
		G_CALLBACK (gtk_widget_queue_draw), day_view->main_canvas);
	day_view->priv->notify_work_day_start_fri_handler_id = handler_id;

	handler_id = e_signal_connect_notify_swapped (
		model, "notify::work-day-end-fri",
		G_CALLBACK (gtk_widget_queue_draw), day_view->main_canvas);
	day_view->priv->notify_work_day_end_fri_handler_id = handler_id;

	handler_id = e_signal_connect_notify_swapped (
		model, "notify::work-day-start-sat",
		G_CALLBACK (gtk_widget_queue_draw), day_view->main_canvas);
	day_view->priv->notify_work_day_start_sat_handler_id = handler_id;

	handler_id = e_signal_connect_notify_swapped (
		model, "notify::work-day-end-sat",
		G_CALLBACK (gtk_widget_queue_draw), day_view->main_canvas);
	day_view->priv->notify_work_day_end_sat_handler_id = handler_id;

	handler_id = e_signal_connect_notify_swapped (
		model, "notify::work-day-start-sun",
		G_CALLBACK (gtk_widget_queue_draw), day_view->main_canvas);
	day_view->priv->notify_work_day_start_sun_handler_id = handler_id;

	handler_id = e_signal_connect_notify_swapped (
		model, "notify::work-day-end-sun",
		G_CALLBACK (gtk_widget_queue_draw), day_view->main_canvas);
	day_view->priv->notify_work_day_end_sun_handler_id = handler_id;

	e_day_view_update_timezone_name_labels (day_view);
}

static void
day_view_update_style_settings (EDayView *day_view)
{
	gint hour;
	gint minute, max_minute_width, i;
	gint month, day, width;
	gint longest_month_width, longest_abbreviated_month_width;
	gint longest_weekday_width, longest_abbreviated_weekday_width;
	gchar buffer[128];
	const gchar *name;
	gint times_width;
	PangoFontDescription *font_desc;
	PangoContext *pango_context;
	PangoFontMetrics *font_metrics;
	PangoLayout *layout;
	gint week_day, event_num;
	GtkAdjustment *adjustment;
	EDayViewEvent *event;
	GdkRGBA rgba;

	g_return_if_fail (E_IS_DAY_VIEW (day_view));

	e_day_view_set_colors (day_view);

	for (week_day = 0; week_day < E_DAY_VIEW_MAX_DAYS; week_day++) {
		for (event_num = 0; event_num < day_view->events[week_day]->len; event_num++) {
			event = &g_array_index (day_view->events[week_day], EDayViewEvent, event_num);
			if (event->canvas_item) {
				rgba = e_day_view_get_text_color (day_view, event);
				gnome_canvas_item_set (
					event->canvas_item,
					"fill-color", &rgba,
					NULL);
			}
		}
	}
	for (event_num = 0; event_num < day_view->long_events->len; event_num++) {
		event = &g_array_index (day_view->long_events, EDayViewEvent, event_num);
		if (event->canvas_item) {
			rgba = e_day_view_get_text_color (day_view, event);
			gnome_canvas_item_set (
				event->canvas_item,
				"fill-color", &rgba,
				NULL);
		}
	}

	/* Set up Pango prerequisites */
	pango_context = gtk_widget_get_pango_context (GTK_WIDGET (day_view));
	font_desc = pango_context_get_font_description (pango_context);
	font_metrics = pango_context_get_metrics (
		pango_context, font_desc,
		pango_context_get_language (pango_context));
	layout = pango_layout_new (pango_context);

	/* Create the large font. */
	if (day_view->large_font_desc != NULL)
		pango_font_description_free (day_view->large_font_desc);

	day_view->large_font_desc = pango_font_description_copy (font_desc);
	pango_font_description_set_size (
		day_view->large_font_desc,
		E_DAY_VIEW_LARGE_FONT_PTSIZE * PANGO_SCALE);

	/* Create the small fonts. */
	if (day_view->small_font_desc != NULL)
		pango_font_description_free (day_view->small_font_desc);

	day_view->small_font_desc = pango_font_description_copy (font_desc);
	pango_font_description_set_size (
		day_view->small_font_desc,
		E_DAY_VIEW_SMALL_FONT_PTSIZE * PANGO_SCALE);

	/* Recalculate the height of each row based on the font size. */
	day_view->row_height =
		PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics)) +
		PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics)) +
		E_DAY_VIEW_EVENT_BORDER_HEIGHT + E_DAY_VIEW_EVENT_Y_PAD * 2 + 2 /* FIXME */;
	day_view->row_height = MAX (
		day_view->row_height,
		E_DAY_VIEW_ICON_HEIGHT + E_DAY_VIEW_ICON_Y_PAD + 2);

	adjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (day_view->main_canvas));
	gtk_adjustment_set_step_increment (adjustment, day_view->row_height);

	day_view->top_row_height =
		PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics)) +
		PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics)) +
		E_DAY_VIEW_LONG_EVENT_BORDER_HEIGHT * 2 + E_DAY_VIEW_LONG_EVENT_Y_PAD * 2 +
		E_DAY_VIEW_TOP_CANVAS_Y_GAP;
	day_view->top_row_height =
		MAX (
			day_view->top_row_height,
		E_DAY_VIEW_ICON_HEIGHT + E_DAY_VIEW_ICON_Y_PAD + 2 +
		E_DAY_VIEW_TOP_CANVAS_Y_GAP);

	adjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (day_view->top_canvas));
	gtk_adjustment_set_step_increment (adjustment, day_view->top_row_height);
	gtk_widget_set_size_request (day_view->top_dates_canvas, -1, day_view->top_row_height - 2);

	e_day_view_update_top_scroll (day_view, TRUE);

	/* Find the longest full & abbreviated month names. */
	longest_month_width = 0;
	longest_abbreviated_month_width = 0;
	for (month = 0; month < 12; month++) {
		name = e_get_month_name (month + 1, FALSE);
		pango_layout_set_text (layout, name, -1);
		pango_layout_get_pixel_size (layout, &width, NULL);

		if (width > longest_month_width) {
			longest_month_width = width;
			day_view->longest_month_name = month;
		}

		name = e_get_month_name (month + 1, TRUE);
		pango_layout_set_text (layout, name, -1);
		pango_layout_get_pixel_size (layout, &width, NULL);

		if (width > longest_abbreviated_month_width) {
			longest_abbreviated_month_width = width;
			day_view->longest_abbreviated_month_name = month;
		}
	}

	/* Find the longest full & abbreviated weekday names. */
	longest_weekday_width = 0;
	longest_abbreviated_weekday_width = 0;
	for (day = 0; day < 7; day++) {
		name = e_get_weekday_name (day + 1, FALSE);
		pango_layout_set_text (layout, name, -1);
		pango_layout_get_pixel_size (layout, &width, NULL);

		if (width > longest_weekday_width) {
			longest_weekday_width = width;
			day_view->longest_weekday_name = day;
		}

		name = e_get_weekday_name (day + 1, TRUE);
		pango_layout_set_text (layout, name, -1);
		pango_layout_get_pixel_size (layout, &width, NULL);

		if (width > longest_abbreviated_weekday_width) {
			longest_abbreviated_weekday_width = width;
			day_view->longest_abbreviated_weekday_name = day;
		}
	}

	/* Calculate the widths of all the time strings necessary. */
	day_view->max_small_hour_width = 0;
	for (hour = 0; hour < 24; hour++) {
		g_snprintf (buffer, sizeof (buffer), "%02i", hour);
		pango_layout_set_text (layout, buffer, -1);
		pango_layout_get_pixel_size (layout, &day_view->small_hour_widths[hour], NULL);

		day_view->max_small_hour_width = MAX (day_view->max_small_hour_width, day_view->small_hour_widths[hour]);
	}

	max_minute_width = 0;
	for (minute = 0, i = 0; minute < 60; minute += 5, i++) {
		gint minute_width;

		g_snprintf (buffer, sizeof (buffer), "%02i", minute);
		pango_layout_set_text (layout, buffer, -1);
		pango_layout_get_pixel_size (layout, &minute_width, NULL);

		max_minute_width = MAX (max_minute_width, minute_width);
	}
	day_view->max_minute_width = max_minute_width;

	pango_layout_set_text (layout, ":", 1);
	pango_layout_get_pixel_size (layout, &day_view->colon_width, NULL);
	pango_layout_set_text (layout, "0", 1);
	pango_layout_get_pixel_size (layout, &day_view->digit_width, NULL);

	pango_layout_set_text (layout, day_view->am_string, -1);
	pango_layout_get_pixel_size (layout, &day_view->am_string_width, NULL);
	pango_layout_set_text (layout, day_view->pm_string, -1);
	pango_layout_get_pixel_size (layout, &day_view->pm_string_width, NULL);

	/* Calculate the width of the time column. */
	times_width = e_day_view_time_item_get_column_width (E_DAY_VIEW_TIME_ITEM (day_view->time_canvas_item));
	gtk_widget_set_size_request (day_view->time_canvas, times_width, -1);

	g_object_unref (layout);
	pango_font_metrics_unref (font_metrics);
}

static void
day_view_realize (GtkWidget *widget)
{
	EDayView *day_view;

	if (GTK_WIDGET_CLASS (e_day_view_parent_class)->realize)
		(*GTK_WIDGET_CLASS (e_day_view_parent_class)->realize)(widget);

	day_view = E_DAY_VIEW (widget);

	day_view_update_style_settings (day_view);

	/* Create the pixmaps. */
	day_view->reminder_icon = e_icon_factory_get_icon ("stock_bell", GTK_ICON_SIZE_MENU);
	day_view->recurrence_icon = e_icon_factory_get_icon ("view-refresh", GTK_ICON_SIZE_MENU);
	day_view->detached_recurrence_icon = e_icon_factory_get_icon ("view-pin", GTK_ICON_SIZE_MENU);
	day_view->timezone_icon = e_icon_factory_get_icon ("stock_timezone", GTK_ICON_SIZE_MENU);
	day_view->meeting_icon = e_icon_factory_get_icon ("stock_people", GTK_ICON_SIZE_MENU);
	day_view->attach_icon = e_icon_factory_get_icon ("mail-attachment", GTK_ICON_SIZE_MENU);

	/* Set the canvas item colors. */
	gnome_canvas_item_set (
		day_view->drag_long_event_rect_item,
		"fill-color", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND],
		"outline-color", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER],
		NULL);

	gnome_canvas_item_set (
		day_view->drag_rect_item,
		"fill-color", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND],
		"outline-color", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER],
		NULL);

	gnome_canvas_item_set (
		day_view->drag_bar_item,
		"fill-color", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR],
		"outline-color", &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER],
		NULL);
}

static void
day_view_unrealize (GtkWidget *widget)
{
	EDayView *day_view;

	day_view = E_DAY_VIEW (widget);

	g_clear_object (&day_view->reminder_icon);
	g_clear_object (&day_view->recurrence_icon);
	g_clear_object (&day_view->detached_recurrence_icon);
	g_clear_object (&day_view->timezone_icon);
	g_clear_object (&day_view->meeting_icon);
	g_clear_object (&day_view->attach_icon);

	if (GTK_WIDGET_CLASS (e_day_view_parent_class)->unrealize)
		(*GTK_WIDGET_CLASS (e_day_view_parent_class)->unrealize)(widget);
}

static gboolean
e_day_view_handle_size_allocate_idle_cb (gpointer user_data)
{
	GWeakRef *weakref = user_data;
	EDayView *day_view = g_weak_ref_get (weakref);

	if (day_view) {
		e_day_view_recalc_main_canvas_size (day_view);
		g_object_unref (day_view);
	}

	return G_SOURCE_REMOVE;
}

static void
day_view_size_allocate (GtkWidget *widget,
                        GtkAllocation *allocation)
{
	(*GTK_WIDGET_CLASS (e_day_view_parent_class)->size_allocate) (widget, allocation);

	g_idle_add_full (G_PRIORITY_HIGH_IDLE, e_day_view_handle_size_allocate_idle_cb,
		e_weak_ref_new (widget), (GDestroyNotify) e_weak_ref_free);
}

static void
day_view_style_updated (GtkWidget *widget)
{
	if (GTK_WIDGET_CLASS (e_day_view_parent_class)->style_updated)
		(*GTK_WIDGET_CLASS (e_day_view_parent_class)->style_updated) (widget);

	day_view_update_style_settings (E_DAY_VIEW (widget));
}

static gboolean
day_view_focus (GtkWidget *widget,
                GtkDirectionType direction)
{
	EDayView *day_view;
	gint new_day;
	gint new_event_num;
	gint start_row, end_row;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (E_IS_DAY_VIEW (widget), FALSE);
	day_view = E_DAY_VIEW (widget);

	if (!e_day_view_get_next_tab_event (day_view, direction,
					    &new_day, &new_event_num))
		return FALSE;

	if ((new_day == -1) && (new_event_num == -1)) {
		/* focus should go to the day view widget itself
		 */
		gtk_widget_grab_focus (GTK_WIDGET (day_view));
		return TRUE;
	}

	if (new_day != E_DAY_VIEW_LONG_EVENT && new_day != -1) {
		if (e_day_view_get_event_rows (day_view, new_day,
					       new_event_num,
					       &start_row, &end_row))
			/* ensure the event to be seen */
			e_day_view_ensure_rows_visible (
				day_view,
				start_row, end_row);
	} else if (new_day != -1) {
		e_day_view_start_editing_event (
			day_view, new_day,
			new_event_num, NULL);
	}

	return TRUE;
}

static gboolean
day_view_key_press (GtkWidget *widget,
                    GdkEventKey *event)
{
	gboolean handled = FALSE;
	handled = e_day_view_do_key_press (widget, event);

	/* if not handled, try key bindings */
	if (!handled)
		handled = GTK_WIDGET_CLASS (e_day_view_parent_class)->key_press_event (widget, event);
	return handled;
}

static gint
day_view_focus_in (GtkWidget *widget,
                   GdkEventFocus *event)
{
	EDayView *day_view;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (E_IS_DAY_VIEW (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	day_view = E_DAY_VIEW (widget);

	/* XXX Can't access flags directly anymore, but is it really needed?
	 *     If so, could we call gtk_widget_send_focus_change() instead? */
#if 0
	GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
#endif

	if (E_CALENDAR_VIEW (day_view)->in_focus && day_view->requires_update) {
		time_t my_start = 0, my_end = 0, model_start = 0, model_end = 0;

		day_view->requires_update = FALSE;

		e_cal_model_get_time_range (e_calendar_view_get_model (E_CALENDAR_VIEW (day_view)), &model_start, &model_end);

		if (e_calendar_view_get_visible_time_range (E_CALENDAR_VIEW (day_view), &my_start, &my_end) &&
		    model_start == my_start && model_end == my_end) {
			/* update only when the same time range is set in a view and in a model;
			 * otherwise time range change invokes also query update */
			e_day_view_recalc_day_starts (day_view, day_view->lower);
			e_day_view_update_query (day_view);
		}
	}

	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);

	if (!day_view->priv->marcus_bains_update_id)
		day_view_refresh_marcus_bains_line (day_view);

	return FALSE;
}

static gint
day_view_focus_out (GtkWidget *widget,
                    GdkEventFocus *event)
{
	EDayView *day_view;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (E_IS_DAY_VIEW (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	day_view = E_DAY_VIEW (widget);

	/* XXX Can't access flags directly anymore, but is it really needed?
	 *     If so, could we call gtk_widget_send_focus_change() instead? */
#if 0
	GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);
#endif

	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);

	return FALSE;
}

static gboolean
day_view_popup_menu (GtkWidget *widget)
{
	EDayView *day_view = E_DAY_VIEW (widget);
	e_day_view_show_popup_menu (
		day_view, NULL,
		day_view->editing_event_day,
		day_view->editing_event_num);
	return TRUE;
}

/* Returns the currently-selected event, or NULL if none */
static GSList *
day_view_get_selected_events (ECalendarView *cal_view)
{
	EDayViewEvent *event = NULL;
	GSList *selection = NULL;
	EDayView *day_view = (EDayView *) cal_view;

	g_return_val_if_fail (E_IS_DAY_VIEW (day_view), NULL);

	if (day_view->editing_event_num != -1) {
		if (day_view->editing_event_day == E_DAY_VIEW_LONG_EVENT) {
			if (!is_array_index_in_bounds (day_view->long_events, day_view->editing_event_num))
				return NULL;

			event = &g_array_index (day_view->long_events,
						EDayViewEvent,
						day_view->editing_event_num);
		} else {
			if (!is_array_index_in_bounds (day_view->events[day_view->editing_event_day], day_view->editing_event_num))
				return NULL;

			event = &g_array_index (day_view->events[day_view->editing_event_day],
						EDayViewEvent,
						day_view->editing_event_num);
		}
	} else if (day_view->popup_event_num != -1) {
		if (day_view->popup_event_day == E_DAY_VIEW_LONG_EVENT) {
			if (!is_array_index_in_bounds (day_view->long_events, day_view->popup_event_num))
				return NULL;

			event = &g_array_index (day_view->long_events,
						EDayViewEvent,
						day_view->popup_event_num);
		} else {
			if (!is_array_index_in_bounds (day_view->events[day_view->popup_event_day], day_view->popup_event_num))
				return NULL;

			event = &g_array_index (day_view->events[day_view->popup_event_day],
						EDayViewEvent,
						day_view->popup_event_num);
		}
	}

	if (event && event->comp_data) {
		selection = g_slist_prepend (selection,
			e_calendar_view_selection_data_new (event->comp_data->client, event->comp_data->icalcomp));
	}

	return selection;
}

/* This sets the selected time range. If the start_time & end_time are not equal
 * and are both visible in the view, then the selection is set to those times,
 * otherwise it is set to 1 hour from the start of the working day. */
static void
day_view_set_selected_time_range (ECalendarView *cal_view,
                                  time_t start_time,
                                  time_t end_time)
{
	EDayView *day_view;
	gint work_day_start_hour;
	gint work_day_start_minute;
	gint work_day_end_hour;
	gint work_day_end_minute;
	gint start_row, start_col, end_row, end_col;
	gboolean need_redraw = FALSE, start_in_grid, end_in_grid;

	day_view = E_DAY_VIEW (cal_view);

	if (start_time == end_time)
		end_time += e_calendar_view_get_time_divisions (cal_view) * 60;

	/* Set the selection. */
	start_in_grid = e_day_view_convert_time_to_grid_position (
		day_view,
		start_time,
		&start_col,
		&start_row);
	end_in_grid = e_day_view_convert_time_to_grid_position (
		day_view,
		end_time - 60,
		&end_col,
		&end_row);

	e_day_view_get_work_day_range_for_day (day_view, start_col,
		&work_day_start_hour, &work_day_start_minute,
		&work_day_end_hour, &work_day_end_minute);

	/* If either of the times isn't in the grid, or the selection covers
	 * an entire day, we set the selection to 1 row from the start of the
	 * working day, in the day corresponding to the start time. */
	if (!start_in_grid || !end_in_grid
	    || (start_row == 0 && end_row == day_view->rows - 1)) {
		end_col = start_col;

		start_row = e_day_view_convert_time_to_row (
			day_view, work_day_start_hour, work_day_start_minute);
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
		gtk_widget_queue_draw (day_view->top_dates_canvas);
		gtk_widget_queue_draw (day_view->main_canvas);

		e_day_view_ensure_rows_visible (day_view, day_view->selection_start_row, day_view->selection_end_row);
	}

	if (!day_view->priv->marcus_bains_update_id)
		day_view_refresh_marcus_bains_line (day_view);
}

/* Gets the visible time range. Returns FALSE if no time range has been set. */
static gboolean
day_view_get_visible_time_range (ECalendarView *cal_view,
                                 time_t *start_time,
                                 time_t *end_time)
{
	EDayView *day_view = E_DAY_VIEW (cal_view);
	gint days_shown;

	/* If the date isn't set, return FALSE. */
	if (day_view->lower == 0 && day_view->upper == 0)
		return FALSE;

	days_shown = e_day_view_get_days_shown (day_view);
	if (days_shown <= 0)
		return FALSE;

	*start_time = day_view->day_starts[0];
	*end_time = day_view->day_starts[days_shown];

	return TRUE;
}

static void
day_view_paste_text (ECalendarView *cal_view)
{
	EDayView *day_view;
	EDayViewEvent *event;

	g_return_if_fail (E_IS_DAY_VIEW (cal_view));

	day_view = E_DAY_VIEW (cal_view);

	if (day_view->editing_event_num == -1) {
		e_day_view_add_new_event_in_selected_range (day_view, NULL, TRUE);
		return;
	}

	if (day_view->editing_event_day == E_DAY_VIEW_LONG_EVENT) {
		if (!is_array_index_in_bounds (day_view->long_events, day_view->editing_event_num))
			return;

		event = &g_array_index (day_view->long_events,
					EDayViewEvent,
					day_view->editing_event_num);
	} else {
		if (!is_array_index_in_bounds (day_view->events[day_view->editing_event_day], day_view->editing_event_num))
			return;

		event = &g_array_index (day_view->events[day_view->editing_event_day],
					EDayViewEvent,
					day_view->editing_event_num);
	}

	if (event->canvas_item &&
	    E_IS_TEXT (event->canvas_item) &&
	    E_TEXT (event->canvas_item)->editing) {
		e_text_paste_clipboard (E_TEXT (event->canvas_item));
	}
}

static EDayViewEvent *
e_day_view_get_event (EDayView *day_view,
		      gint day,
		      gint event_num)
{
	EDayViewEvent *pevent;

	if (day == E_DAY_VIEW_LONG_EVENT) {
		if (!is_array_index_in_bounds (day_view->long_events, event_num))
			return NULL;

		pevent = &g_array_index (day_view->long_events, EDayViewEvent, event_num);
	} else {
		if (!is_array_index_in_bounds (day_view->events[day], event_num))
			return NULL;

		pevent = &g_array_index (day_view->events[day], EDayViewEvent, event_num);
	}

	return pevent;
}

static gboolean
e_day_view_query_tooltip (EDayView *day_view,
			  gint day,
			  gint event_num,
			  GtkTooltip *tooltip)
{
	EDayViewEvent *event;
	ECalComponent *new_comp;
	ECalModel *model;
	gchar *markup;

	event = e_day_view_get_event (day_view, day, event_num);
	if (!event || !event->comp_data)
		return FALSE;

	new_comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (event->comp_data->icalcomp));
	if (!new_comp)
		return FALSE;

	model = e_calendar_view_get_model (E_CALENDAR_VIEW (day_view));

	markup = cal_comp_util_dup_tooltip (new_comp, event->comp_data->client,
		e_cal_model_get_registry (model),
		e_cal_model_get_timezone (model));

	gtk_tooltip_set_markup (tooltip, markup);

	g_free (markup);
	g_object_unref (new_comp);

	return TRUE;
}

static gboolean
e_day_view_top_canvas_query_tooltip_cb (GtkWidget *widget,
					gint x,
					gint y,
					gboolean keyboard_mode,
					GtkTooltip *tooltip,
					gpointer user_data)
{
	EDayView *day_view = user_data;
	ECalendarViewPosition pos;
	gint day, event_num;

	g_return_val_if_fail (E_IS_DAY_VIEW (day_view), FALSE);

	if (keyboard_mode)
		return FALSE;

	y += gtk_adjustment_get_value (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (day_view->top_canvas)));

	pos = e_day_view_convert_position_in_top_canvas (day_view, x, y, &day, &event_num);

	if (pos == E_CALENDAR_VIEW_POS_OUTSIDE)
		return FALSE;

	if (pos == E_CALENDAR_VIEW_POS_NONE)
		return FALSE;

	return e_day_view_query_tooltip (day_view, E_DAY_VIEW_LONG_EVENT, event_num, tooltip);
}

static gboolean
e_day_view_main_canvas_query_tooltip_cb (GtkWidget *widget,
					 gint x,
					 gint y,
					 gboolean keyboard_mode,
					 GtkTooltip *tooltip,
					 gpointer user_data)
{
	EDayView *day_view = user_data;
	ECalendarViewPosition pos;
	gint row, day, event_num;

	g_return_val_if_fail (E_IS_DAY_VIEW (day_view), FALSE);

	if (keyboard_mode)
		return FALSE;

	y += gtk_adjustment_get_value (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (day_view->main_canvas)));

	pos = e_day_view_convert_position_in_main_canvas (day_view, x, y, &day, &row, &event_num);

	if (pos == E_CALENDAR_VIEW_POS_OUTSIDE)
		return FALSE;

	if (pos == E_CALENDAR_VIEW_POS_NONE)
		return FALSE;

	return e_day_view_query_tooltip (day_view, day, event_num, tooltip);
}

static void
e_day_view_class_init (EDayViewClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	ECalendarViewClass *view_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = day_view_set_property;
	object_class->get_property = day_view_get_property;
	object_class->constructed = day_view_constructed;
	object_class->dispose = day_view_dispose;
	object_class->notify = day_view_notify;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->realize = day_view_realize;
	widget_class->unrealize = day_view_unrealize;
	widget_class->size_allocate = day_view_size_allocate;
	widget_class->style_updated = day_view_style_updated;
	widget_class->focus = day_view_focus;
	widget_class->key_press_event = day_view_key_press;
	widget_class->focus_in_event = day_view_focus_in;
	widget_class->focus_out_event = day_view_focus_out;
	widget_class->popup_menu = day_view_popup_menu;

	view_class = E_CALENDAR_VIEW_CLASS (class);
	view_class->get_selected_events = day_view_get_selected_events;
	view_class->get_selected_time_range = day_view_get_selected_time_range;
	view_class->set_selected_time_range = day_view_set_selected_time_range;
	view_class->get_visible_time_range = day_view_get_visible_time_range;
	view_class->precalc_visible_time_range = e_day_view_precalc_visible_time_range;
	view_class->paste_text = day_view_paste_text;

	g_object_class_install_property (
		object_class,
		PROP_DRAW_FLAT_EVENTS,
		g_param_spec_boolean (
			"draw-flat-events",
			"Draw Flat Events",
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_MARCUS_BAINS_SHOW_LINE,
		g_param_spec_boolean (
			"marcus-bains-show-line",
			"Marcus Bains Show Line",
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_MARCUS_BAINS_DAY_VIEW_COLOR,
		g_param_spec_string (
			"marcus-bains-day-view-color",
			"Marcus Bains Day View Color",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_MARCUS_BAINS_TIME_BAR_COLOR,
		g_param_spec_string (
			"marcus-bains-time-bar-color",
			"Marcus Bains Time Bar Color",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_TODAY_BACKGROUND_COLOR,
		g_param_spec_string (
			"today-background-color",
			"Today Background Color",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_override_property (
		object_class,
		PROP_IS_EDITING,
		"is-editing");

	/* init the accessibility support for e_day_view */
	gtk_widget_class_set_accessible_type (widget_class, EA_TYPE_DAY_VIEW);
}

static void
e_day_view_init (EDayView *day_view)
{
	gint day;
	GdkRGBA black_rgba = { .red = 0.0, .green = 0.0, .blue = 0.0, .alpha = 1.0 };
	GnomeCanvasGroup *canvas_group;
	GtkAdjustment *adjustment;
	GtkScrollable *scrollable;
	GtkWidget *container;
	GtkWidget *widget;
	gulong handler_id;

	day_view->priv = e_day_view_get_instance_private (day_view);

	gtk_widget_set_can_focus (GTK_WIDGET (day_view), TRUE);

	day_view->long_events = g_array_new (
		FALSE, FALSE,
		sizeof (EDayViewEvent));
	day_view->long_events_sorted = TRUE;
	day_view->long_events_need_layout = FALSE;
	day_view->long_events_need_reshape = FALSE;

	day_view->layout_timeout_id = 0;

	for (day = 0; day < E_DAY_VIEW_MAX_DAYS; day++) {
		day_view->events[day] = g_array_new (
			FALSE, FALSE,
			sizeof (EDayViewEvent));
		day_view->events_sorted[day] = TRUE;
		day_view->need_layout[day] = FALSE;
		day_view->need_reshape[day] = FALSE;
	}

	/* These indicate that the times haven't been set. */
	day_view->lower = 0;
	day_view->upper = 0;

	day_view->priv->days_shown = 1;

	day_view->date_format = E_DAY_VIEW_DATE_FULL;
	day_view->rows_in_top_display = 0;

	/* Note that these don't work yet. It would need a few fixes to the
	 * way event->start_minute and event->end_minute are used, and there
	 * may be problems with events that go outside the visible times. */
	day_view->first_hour_shown = 0;
	day_view->first_minute_shown = 0;
	day_view->last_hour_shown = 24;
	day_view->last_minute_shown = 0;

	e_day_view_recalc_num_rows (day_view);

	day_view->show_event_end_times = TRUE;
	day_view->scroll_to_work_day = TRUE;

	day_view->editing_event_day = -1;
	day_view->editing_event_num = -1;

	day_view->resize_event_num = -1;
	day_view->resize_bars_event_day = -1;
	day_view->resize_bars_event_num = -1;

	day_view->last_edited_comp_string = NULL;

	day_view->selection_start_row = -1;
	day_view->selection_start_day = -1;
	day_view->selection_end_row = -1;
	day_view->selection_end_day = -1;
	day_view->selection_is_being_dragged = FALSE;
	day_view->selection_drag_pos = E_DAY_VIEW_DRAG_END;
	day_view->selection_in_top_canvas = FALSE;
	day_view->drag_last_day = -1;
	day_view->drag_event_day = -1;
	day_view->drag_event_num = -1;
	day_view->priv->drag_event_is_recurring = FALSE;
	day_view->resize_drag_pos = E_CALENDAR_VIEW_POS_NONE;
	day_view->priv->drag_context = NULL;

	day_view->pressed_event_day = -1;

	day_view->auto_scroll_timeout_id = 0;

	day_view->large_font_desc = NULL;
	day_view->small_font_desc = NULL;

	/* String to use in 12-hour time format for times in the morning. */
	day_view->am_string = _("am");

	/* String to use in 12-hour time format for times in the afternoon. */
	day_view->pm_string = _("pm");

	day_view->bc_event_time = 0;
	day_view->before_click_dtstart = 0;
	day_view->before_click_dtend = 0;

	gtk_widget_set_margin_top (GTK_WIDGET (day_view), 1);

	day_view->week_number_label = gtk_label_new ("");

	widget = gtk_label_new (NULL);
	gtk_label_set_ellipsize (GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_label_set_yalign (GTK_LABEL (widget), 1.0);
	day_view->priv->timezone_name_1_label = widget;

	widget = gtk_label_new (NULL);
	gtk_label_set_ellipsize (GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_label_set_yalign (GTK_LABEL (widget), 1.0);
	day_view->priv->timezone_name_2_label = widget;

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

	gtk_grid_attach (GTK_GRID (day_view), widget, 0, 0, 1, 1);
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"vexpand", FALSE,
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_FILL,
		NULL);

	container = widget;

	gtk_box_pack_start (GTK_BOX (container), day_view->week_number_label, TRUE, TRUE, 2);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_box_pack_end (GTK_BOX (container), widget, TRUE, TRUE, 2);

	gtk_widget_show_all (container);

	container = widget;

	gtk_box_set_homogeneous (GTK_BOX (container), TRUE);
	gtk_box_pack_start (GTK_BOX (container), day_view->priv->timezone_name_1_label, TRUE, TRUE, 2);
	gtk_box_pack_start (GTK_BOX (container), day_view->priv->timezone_name_2_label, TRUE, TRUE, 2);

	gtk_widget_show_all (container);

	/*
	 * Top Canvas
	 */
	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_grid_attach (GTK_GRID (day_view), widget, 1, 0, 1, 1);
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"vexpand", FALSE,
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_FILL,
		NULL);
	gtk_widget_show (widget);

	container = widget;

	widget = e_canvas_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	day_view->top_dates_canvas = widget;
	gtk_widget_show (widget);

	/* Keep our own canvas reference so we can
	 * disconnect signal handlers in dispose(). */
	widget = e_canvas_new ();
	gtk_box_pack_end (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_set_has_tooltip (widget, TRUE);
	day_view->top_canvas = g_object_ref (widget);
	gtk_widget_show (widget);

	handler_id = g_signal_connect_after (
		day_view->top_canvas, "button_press_event",
		G_CALLBACK (e_day_view_on_top_canvas_button_press), day_view);
	day_view->priv->top_canvas_button_press_event_handler_id = handler_id;

	handler_id = g_signal_connect (
		day_view->top_canvas, "button_release_event",
		G_CALLBACK (e_day_view_on_top_canvas_button_release), day_view);
	day_view->priv->top_canvas_button_release_event_handler_id = handler_id;

	handler_id = g_signal_connect (
		day_view->top_canvas, "scroll_event",
		G_CALLBACK (e_day_view_on_top_canvas_scroll), day_view);
	day_view->priv->top_canvas_scroll_event_handler_id = handler_id;

	handler_id = g_signal_connect (
		day_view->top_canvas, "motion_notify_event",
		G_CALLBACK (e_day_view_on_top_canvas_motion), day_view);
	day_view->priv->top_canvas_motion_notify_event_handler_id = handler_id;

	handler_id = g_signal_connect (
		day_view->top_canvas, "drag_motion",
		G_CALLBACK (e_day_view_on_top_canvas_drag_motion), day_view);
	day_view->priv->top_canvas_drag_motion_handler_id = handler_id;

	handler_id = g_signal_connect (
		day_view->top_canvas, "drag_leave",
		G_CALLBACK (e_day_view_on_top_canvas_drag_leave), day_view);
	day_view->priv->top_canvas_drag_leave_handler_id = handler_id;

	handler_id = g_signal_connect (
		day_view->top_canvas, "drag_begin",
		G_CALLBACK (e_day_view_on_drag_begin), day_view);
	day_view->priv->top_canvas_drag_begin_handler_id = handler_id;

	handler_id = g_signal_connect (
		day_view->top_canvas, "drag_end",
		G_CALLBACK (e_day_view_on_drag_end), day_view);
	day_view->priv->top_canvas_drag_end_handler_id = handler_id;

	handler_id = g_signal_connect (
		day_view->top_canvas, "drag_data_get",
		G_CALLBACK (e_day_view_on_drag_data_get), day_view);
	day_view->priv->top_canvas_drag_data_get_handler_id = handler_id;

	handler_id = g_signal_connect (
		day_view->top_canvas, "drag_data_received",
		G_CALLBACK (e_day_view_on_top_canvas_drag_data_received), day_view);
	day_view->priv->top_canvas_drag_data_received_handler_id = handler_id;

	g_signal_connect_object (day_view->top_canvas, "query-tooltip",
		G_CALLBACK (e_day_view_top_canvas_query_tooltip_cb), day_view, 0);

	canvas_group = GNOME_CANVAS_GROUP (GNOME_CANVAS (day_view->top_dates_canvas)->root);

	day_view->top_dates_canvas_item =
		gnome_canvas_item_new (
			canvas_group,
			e_day_view_top_item_get_type (),
			"EDayViewTopItem::day_view", day_view,
			"EDayViewTopItem::show_dates", TRUE,
			NULL);
	gtk_widget_set_size_request (day_view->top_dates_canvas, -1, day_view->top_row_height);

	canvas_group = GNOME_CANVAS_GROUP (GNOME_CANVAS (day_view->top_canvas)->root);

	day_view->top_canvas_item =
		gnome_canvas_item_new (
			canvas_group,
			e_day_view_top_item_get_type (),
			"EDayViewTopItem::day_view", day_view,
			"EDayViewTopItem::show_dates", FALSE,
			NULL);

	day_view->drag_long_event_rect_item =
		gnome_canvas_item_new (
			canvas_group,
			gnome_canvas_rect_get_type (),
			"line_width", 1.0,
			NULL);
	gnome_canvas_item_hide (day_view->drag_long_event_rect_item);

	day_view->drag_long_event_item =
		gnome_canvas_item_new (
			canvas_group,
			e_text_get_type (),
			"line_wrap", TRUE,
			"clip", TRUE,
			"max_lines", 1,
			"editable", TRUE,
			"fill-color", &black_rgba,
			NULL);
	gnome_canvas_item_hide (day_view->drag_long_event_item);

	/*
	 * Main Canvas
	 */

	/* Keep our own canvas reference so we can
	 * disconnect signal handlers in dispose(). */
	widget = e_canvas_new ();
	gtk_grid_attach (GTK_GRID (day_view), widget, 1, 1, 1, 1);
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"vexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_FILL,
		"has-tooltip", TRUE,
		NULL);
	day_view->main_canvas = g_object_ref (widget);
	gtk_widget_show (widget);

	handler_id = g_signal_connect (
		day_view->main_canvas, "realize",
		G_CALLBACK (e_day_view_on_canvas_realized), day_view);
	day_view->priv->main_canvas_realize_handler_id = handler_id;

	handler_id = g_signal_connect (
		day_view->main_canvas, "button_press_event",
		G_CALLBACK (e_day_view_on_main_canvas_button_press), day_view);
	day_view->priv->main_canvas_button_press_event_handler_id = handler_id;

	handler_id = g_signal_connect (
		day_view->main_canvas, "button_release_event",
		G_CALLBACK (e_day_view_on_main_canvas_button_release),
		day_view);
	day_view->priv->main_canvas_button_release_event_handler_id = handler_id;

	handler_id = g_signal_connect (
		day_view->main_canvas, "scroll_event",
		G_CALLBACK (e_day_view_on_main_canvas_scroll), day_view);
	day_view->priv->main_canvas_scroll_event_handler_id = handler_id;

	handler_id = g_signal_connect (
		day_view->main_canvas, "motion_notify_event",
		G_CALLBACK (e_day_view_on_main_canvas_motion), day_view);
	day_view->priv->main_canvas_motion_notify_event_handler_id = handler_id;

	handler_id = g_signal_connect (
		day_view->main_canvas, "drag_motion",
		G_CALLBACK (e_day_view_on_main_canvas_drag_motion), day_view);
	day_view->priv->main_canvas_drag_motion_handler_id = handler_id;

	handler_id = g_signal_connect (
		day_view->main_canvas, "drag_leave",
		G_CALLBACK (e_day_view_on_main_canvas_drag_leave), day_view);
	day_view->priv->main_canvas_drag_leave_handler_id = handler_id;

	handler_id = g_signal_connect (
		day_view->main_canvas, "drag_begin",
		G_CALLBACK (e_day_view_on_drag_begin), day_view);
	day_view->priv->main_canvas_drag_begin_handler_id = handler_id;

	handler_id = g_signal_connect (
		day_view->main_canvas, "drag_end",
		G_CALLBACK (e_day_view_on_drag_end), day_view);
	day_view->priv->main_canvas_drag_end_handler_id = handler_id;

	handler_id = g_signal_connect (
		day_view->main_canvas, "drag_data_get",
		G_CALLBACK (e_day_view_on_drag_data_get), day_view);
	day_view->priv->main_canvas_drag_data_get_handler_id = handler_id;

	handler_id = g_signal_connect (
		day_view->main_canvas, "drag_data_received",
		G_CALLBACK (e_day_view_on_main_canvas_drag_data_received), day_view);
	day_view->priv->main_canvas_drag_data_received_handler_id = handler_id;

	g_signal_connect_object (day_view->main_canvas, "query-tooltip",
		G_CALLBACK (e_day_view_main_canvas_query_tooltip_cb), day_view, 0);

	canvas_group = GNOME_CANVAS_GROUP (GNOME_CANVAS (day_view->main_canvas)->root);

	day_view->main_canvas_item =
		gnome_canvas_item_new (
			canvas_group,
			e_day_view_main_item_get_type (),
			"EDayViewMainItem::day_view", day_view,
			NULL);

	day_view->drag_rect_item =
		gnome_canvas_item_new (
			canvas_group,
			gnome_canvas_rect_get_type (),
			"line_width", 1.0,
			NULL);
	gnome_canvas_item_hide (day_view->drag_rect_item);

	day_view->drag_bar_item =
		gnome_canvas_item_new (
			canvas_group,
			gnome_canvas_rect_get_type (),
			"line_width", 1.0,
			NULL);
	gnome_canvas_item_hide (day_view->drag_bar_item);

	day_view->drag_item =
		gnome_canvas_item_new (
			canvas_group,
			e_text_get_type (),
			"line_wrap", TRUE,
			"clip", TRUE,
			"editable", TRUE,
			"fill-color", &black_rgba,
			NULL);
	gnome_canvas_item_hide (day_view->drag_item);

	/*
	 * Times Canvas
	 */

	/* Keep our own canvas reference so we can
	 * disconnect signal handlers in dispose(). */
	widget = e_canvas_new ();
	scrollable = GTK_SCROLLABLE (day_view->main_canvas);
	adjustment = gtk_scrollable_get_vadjustment (scrollable);
	scrollable = GTK_SCROLLABLE (widget);
	gtk_scrollable_set_vadjustment (scrollable, adjustment);
	gtk_grid_attach (GTK_GRID (day_view), widget, 0, 1, 1, 1);
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"vexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_FILL,
		NULL);
	day_view->time_canvas = g_object_ref (widget);
	gtk_widget_show (widget);

	handler_id = g_signal_connect_after (
		day_view->time_canvas, "scroll_event",
		G_CALLBACK (e_day_view_on_time_canvas_scroll), day_view);
	day_view->priv->time_canvas_scroll_event_handler_id = handler_id;

	canvas_group = GNOME_CANVAS_GROUP (GNOME_CANVAS (day_view->time_canvas)->root);

	day_view->time_canvas_item =
		gnome_canvas_item_new (
			canvas_group,
			e_day_view_time_item_get_type (),
			"EDayViewTimeItem::day_view", day_view,
			NULL);

	/*
	 * Scrollbar.
	 */
	scrollable = GTK_SCROLLABLE (day_view->main_canvas);
	adjustment = gtk_scrollable_get_hadjustment (scrollable);
	day_view->mc_hscrollbar = gtk_scrollbar_new (
		GTK_ORIENTATION_HORIZONTAL, adjustment);
	gtk_grid_attach (GTK_GRID (day_view), day_view->mc_hscrollbar, 1, 2, 1, 1);
	g_object_set (G_OBJECT (day_view->mc_hscrollbar),
		"hexpand", FALSE,
		"vexpand", FALSE,
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_START,
		NULL);
	gtk_widget_show (day_view->mc_hscrollbar);

	scrollable = GTK_SCROLLABLE (day_view->top_canvas);
	adjustment = gtk_scrollable_get_vadjustment (scrollable);
	day_view->tc_vscrollbar = gtk_scrollbar_new (
		GTK_ORIENTATION_VERTICAL, adjustment);
	gtk_grid_attach (GTK_GRID (day_view), day_view->tc_vscrollbar, 2, 0, 1, 1);
	g_object_set (G_OBJECT (day_view->tc_vscrollbar),
		"hexpand", FALSE,
		"vexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"valign", GTK_ALIGN_FILL,
		NULL);
	/* gtk_widget_show (day_view->tc_vscrollbar); */

	scrollable = GTK_SCROLLABLE (day_view->main_canvas);
	adjustment = gtk_scrollable_get_vadjustment (scrollable);
	day_view->vscrollbar = gtk_scrollbar_new (
		GTK_ORIENTATION_VERTICAL, adjustment);
	gtk_grid_attach (GTK_GRID (day_view), day_view->vscrollbar, 2, 1, 1, 1);
	g_object_set (G_OBJECT (day_view->vscrollbar),
		"hexpand", FALSE,
		"vexpand", TRUE,
		"halign", GTK_ALIGN_START,
		"valign", GTK_ALIGN_FILL,
		NULL);
	gtk_widget_show (day_view->vscrollbar);

	/* Create the cursors. */
	day_view->normal_cursor = gdk_cursor_new_from_name (gdk_display_get_default (), "default");
	day_view->move_cursor = gdk_cursor_new_from_name (gdk_display_get_default (), "move");
	day_view->resize_width_cursor = gdk_cursor_new_from_name (gdk_display_get_default (), "ew-resize");
	day_view->resize_height_cursor = gdk_cursor_new_from_name (gdk_display_get_default (), "ns-resize");
	day_view->last_cursor_set_in_top_canvas = NULL;
	day_view->last_cursor_set_in_main_canvas = NULL;

	/* Set up the drop sites. */
	gtk_drag_dest_set (
		day_view->top_canvas, GTK_DEST_DEFAULT_ALL,
		target_table, G_N_ELEMENTS (target_table),
		GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_ASK);

	e_drag_dest_add_calendar_targets (day_view->top_canvas);

	gtk_drag_dest_set (
		day_view->main_canvas, GTK_DEST_DEFAULT_ALL,
		target_table, G_N_ELEMENTS (target_table),
		GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_ASK);

	e_drag_dest_add_calendar_targets (day_view->main_canvas);

	day_view->requires_update = FALSE;
}

static void
e_day_view_precalc_visible_time_range (ECalendarView *cal_view,
				       time_t in_start_time,
				       time_t in_end_time,
				       time_t *out_start_time,
				       time_t *out_end_time)
{
	EDayView *day_view;
	gint days_shown;
	time_t lower;
	ICalTimezone *zone;

	g_return_if_fail (E_IS_DAY_VIEW (cal_view));
	g_return_if_fail (out_start_time != NULL);
	g_return_if_fail (out_end_time != NULL);

	day_view = E_DAY_VIEW (cal_view);
	days_shown = e_day_view_get_days_shown (day_view);
	zone = e_calendar_view_get_timezone (cal_view);

	/* Calculate the first day that should be shown, based on start_time
	 * and the days_shown setting. If we are showing 1 day it is just the
	 * start of the day given by start_time, otherwise it is the previous
	 * work-week start day. */
	if (!e_day_view_get_work_week_view (day_view)) {
		lower = time_day_begin_with_zone (in_start_time, zone);
	} else {
		lower = e_day_view_find_work_week_start (day_view, in_start_time);
	}

	/* See if we need to change the days shown. */
	if (lower == day_view->lower) {
		*out_start_time = day_view->lower;
		*out_end_time = day_view->upper;
	} else {
		gint day;

		*out_start_time = lower;
		*out_end_time = lower;

		for (day = 1; day <= days_shown; day++) {
			*out_end_time = time_add_day_with_zone (*out_end_time, 1, zone);
		}
	}
}

static void
time_range_changed_cb (ECalModel *model,
                       gint64 i64_start_time,
                       gint64 i64_end_time,
                       gpointer user_data)
{
	EDayView *day_view = E_DAY_VIEW (user_data);
	EDayViewTimeItem *eti;
	gint days_shown;
	time_t lower, start_time = (time_t) i64_start_time, end_time = (time_t) i64_end_time;

	g_return_if_fail (E_IS_DAY_VIEW (day_view));

	days_shown = e_day_view_get_days_shown (day_view);

	/* Calculate the first day that should be shown, based on start_time
	 * and the days_shown setting. If we are showing 1 day it is just the
	 * start of the day given by start_time, otherwise it is the previous
	 * work-week start day. */
	if (!e_day_view_get_work_week_view (day_view)) {
		lower = time_day_begin_with_zone (start_time, e_calendar_view_get_timezone (E_CALENDAR_VIEW (day_view)));
	} else {
		lower = e_day_view_find_work_week_start (day_view, start_time);
	}

	/* See if we need to change the days shown. */
	if (lower != day_view->lower)
		e_day_view_recalc_day_starts (day_view, lower);

	if (!E_CALENDAR_VIEW (day_view)->in_focus) {
		e_day_view_free_events (day_view);
		day_view->requires_update = TRUE;
		return;
	}

	/* If we don't show the new selection, don't preserve it */
	if (day_view->selection_start_day == -1 || days_shown <= day_view->selection_start_day)
		day_view_set_selected_time_range (E_CALENDAR_VIEW (day_view), start_time, end_time);

	if (day_view->selection_start_row != -1)
		e_day_view_ensure_rows_visible (day_view, day_view->selection_start_row, day_view->selection_start_row);

	/* update the time canvas to show proper date in it */
	eti = E_DAY_VIEW_TIME_ITEM (day_view->time_canvas_item);
	if (eti && e_day_view_time_item_get_second_zone (eti))
		gtk_widget_queue_draw (day_view->time_canvas);
}

static void
process_component (EDayView *day_view,
                   ECalModelComponent *comp_data)
{
	ECalModel *model;
	ECalComponent *comp;
	ESourceRegistry *registry;
	AddEventData add_event_data;

	model = e_calendar_view_get_model (E_CALENDAR_VIEW (day_view));
	registry = e_cal_model_get_registry (model);

	/* If our time hasn't been set yet, just return. */
	if (day_view->lower == 0 && day_view->upper == 0)
		return;

	comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (comp_data->icalcomp));
	if (!comp) {
		g_message (G_STRLOC ": Could not set ICalComponent on ECalComponent");
		return;
	}

	/* Add the object */
	add_event_data.day_view = day_view;
	add_event_data.comp_data = comp_data;
	e_day_view_add_event (
		registry, comp_data->client, comp, comp_data->instance_start,
		comp_data->instance_end, &add_event_data);

	g_object_unref (comp);
}

static void
update_row (EDayView *day_view,
	    gint row,
	    gboolean do_cancel_editing)
{
	ECalModelComponent *comp_data;
	ECalModel *model;
	gint day, event_num;
	const gchar *uid;
	gchar *rid;

	if (do_cancel_editing)
		cancel_editing (day_view);
	else
		e_day_view_stop_editing_event (day_view);

	model = e_calendar_view_get_model (E_CALENDAR_VIEW (day_view));
	comp_data = e_cal_model_get_component_at (model, row);
	g_return_if_fail (comp_data != NULL);

	uid = i_cal_component_get_uid (comp_data->icalcomp);
	rid = e_cal_util_component_get_recurid_as_string (comp_data->icalcomp);

	if (e_day_view_find_event_from_uid (day_view, comp_data->client, uid, rid, &day, &event_num))
		e_day_view_remove_event_cb (day_view, day, event_num, NULL);

	g_free (rid);

	process_component (day_view, comp_data);

	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);
	e_day_view_queue_layout (day_view);
}

static void
model_row_changed_cb (ETableModel *etm,
                      gint row,
                      gpointer user_data)
{
	EDayView *day_view = E_DAY_VIEW (user_data);

	if (!E_CALENDAR_VIEW (day_view)->in_focus) {
		e_day_view_free_events (day_view);
		day_view->requires_update = TRUE;
		return;
	}

	update_row (day_view, row, TRUE);
}

static void
model_cell_changed_cb (ETableModel *etm,
                       gint col,
                       gint row,
                       gpointer user_data)
{
	EDayView *day_view = E_DAY_VIEW (user_data);

	if (!E_CALENDAR_VIEW (day_view)->in_focus) {
		e_day_view_free_events (day_view);
		day_view->requires_update = TRUE;
		return;
	}

	update_row (day_view, row, FALSE);
}

static void
model_rows_inserted_cb (ETableModel *etm,
                        gint row,
                        gint count,
                        gpointer user_data)
{
	EDayView *day_view = E_DAY_VIEW (user_data);
	ECalModel *model;
	gint i;

	if (!E_CALENDAR_VIEW (day_view)->in_focus) {
		e_day_view_free_events (day_view);
		day_view->requires_update = TRUE;
		return;
	}

	e_day_view_stop_editing_event (day_view);

	model = e_calendar_view_get_model (E_CALENDAR_VIEW (day_view));
	for (i = 0; i < count; i++) {
		ECalModelComponent *comp_data;

		comp_data = e_cal_model_get_component_at (model, row + i);
		if (comp_data == NULL) {
			g_warning ("comp_data is NULL\n");
			continue;
		}
		process_component (day_view, comp_data);
	}

	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);
	e_day_view_queue_layout (day_view);

}

static void
model_comps_deleted_cb (ETableModel *etm,
                        gpointer data,
                        gpointer user_data)
{
	EDayView *day_view = E_DAY_VIEW (user_data);
	GSList *l, *list = data;

	if (!E_CALENDAR_VIEW (day_view)->in_focus) {
		e_day_view_free_events (day_view);
		day_view->requires_update = TRUE;
		return;
	}

	e_day_view_stop_editing_event (day_view);

	for (l = list; l != NULL; l = g_slist_next (l)) {
		ECalModelComponent *comp_data = l->data;
		gint day, event_num;
		const gchar *uid;
		gchar *rid;

		uid = i_cal_component_get_uid (comp_data->icalcomp);
		rid = e_cal_util_component_get_recurid_as_string (comp_data->icalcomp);

		if (e_day_view_find_event_from_uid (day_view, comp_data->client, uid, rid, &day, &event_num))
			e_day_view_remove_event_cb (day_view, day, event_num, NULL);

		g_free (rid);
	}

	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);
	e_day_view_queue_layout (day_view);
}

static void
timezone_changed_cb (ECalModel *cal_model,
		     ICalTimezone *old_zone,
		     ICalTimezone *new_zone,
		     gpointer user_data)
{
	ICalTime *tt;
	time_t lower;
	EDayView *day_view = (EDayView *) user_data;
	ECalendarView *cal_view = (ECalendarView *) day_view;

	g_return_if_fail (E_IS_DAY_VIEW (day_view));

	e_day_view_update_timezone_name_labels (day_view);

	if (!cal_view->in_focus) {
		e_day_view_free_events (day_view);
		day_view->requires_update = TRUE;
		return;
	}

	/* If our time hasn't been set yet, just return. */
	if (day_view->lower == 0 && day_view->upper == 0)
		return;

	/* Recalculate the new start of the first day. We just use exactly
	 * the same time, but with the new timezone. */
	tt = i_cal_time_new_from_timet_with_zone (
		day_view->lower, FALSE,
		old_zone);

	lower = i_cal_time_as_timet_with_zone (tt, new_zone);

	g_clear_object (&tt);

	e_day_view_recalc_day_starts (day_view, lower);
	e_day_view_update_query (day_view);
}

static void
init_model (EDayView *day_view,
            ECalModel *model)
{
	gulong handler_id;

	handler_id = g_signal_connect (
		model, "time_range_changed",
		G_CALLBACK (time_range_changed_cb), day_view);
	day_view->priv->time_range_changed_handler_id = handler_id;

	handler_id = g_signal_connect (
		model, "model_row_changed",
		G_CALLBACK (model_row_changed_cb), day_view);
	day_view->priv->model_row_changed_handler_id = handler_id;

	handler_id = g_signal_connect (
		model, "model_cell_changed",
		G_CALLBACK (model_cell_changed_cb), day_view);
	day_view->priv->model_cell_changed_handler_id = handler_id;

	handler_id = g_signal_connect (
		model, "model_rows_inserted",
		G_CALLBACK (model_rows_inserted_cb), day_view);
	day_view->priv->model_rows_inserted_handler_id = handler_id;

	handler_id = g_signal_connect (
		model, "comps_deleted",
		G_CALLBACK (model_comps_deleted_cb), day_view);
	day_view->priv->comps_deleted_handler_id = handler_id;

	handler_id = g_signal_connect (
		model, "timezone_changed",
		G_CALLBACK (timezone_changed_cb), day_view);
	day_view->priv->timezone_changed_handler_id = handler_id;
}

/* Turn off the background of the canvas windows. This reduces flicker
 * considerably when scrolling. (Why isn't it in GnomeCanvas?). */
static void
e_day_view_on_canvas_realized (GtkWidget *widget,
                               EDayView *day_view)
{
	GdkWindow *window;

	window = gtk_layout_get_bin_window (GTK_LAYOUT (widget));
	gdk_window_set_background_pattern (window, NULL);
}

/**
 * e_day_view_new:
 * @Returns: a new #EDayView.
 *
 * Creates a new #EDayView.
 **/
ECalendarView *
e_day_view_new (ECalModel *model)
{
	ECalendarView *day_view;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	day_view = g_object_new (E_TYPE_DAY_VIEW, "model", model, NULL);
	init_model (E_DAY_VIEW (day_view), model);

	return day_view;
}

static void
e_day_view_set_colors (EDayView *day_view)
{
	GtkWidget *widget = GTK_WIDGET (day_view);
	GdkRGBA base_bg, bg_bg, selected_bg, unfocused_selected_bg, dark_bg, light_bg;

	e_utils_get_theme_color (widget, "theme_base_color", E_UTILS_DEFAULT_THEME_BASE_COLOR, &base_bg);
	e_utils_get_theme_color (widget, "theme_bg_color", E_UTILS_DEFAULT_THEME_BG_COLOR, &bg_bg);
	e_utils_get_theme_color (widget, "theme_selected_bg_color", E_UTILS_DEFAULT_THEME_SELECTED_BG_COLOR, &selected_bg);
	e_utils_get_theme_color (widget, "theme_unfocused_selected_bg_color,theme_selected_bg_color", E_UTILS_DEFAULT_THEME_UNFOCUSED_SELECTED_BG_COLOR, &unfocused_selected_bg);

	e_utils_shade_color (&bg_bg, &dark_bg, E_UTILS_DARKNESS_MULT);
	e_utils_shade_color (&bg_bg, &light_bg, E_UTILS_LIGHTNESS_MULT);

	day_view->colors[E_DAY_VIEW_COLOR_BG_WORKING] = base_bg;
	day_view->colors[E_DAY_VIEW_COLOR_BG_NOT_WORKING] = bg_bg;
	day_view->colors[E_DAY_VIEW_COLOR_BG_SELECTED] = selected_bg;
	day_view->colors[E_DAY_VIEW_COLOR_BG_SELECTED_UNFOCUSSED] = unfocused_selected_bg;
	day_view->colors[E_DAY_VIEW_COLOR_BG_GRID] = dark_bg;
	day_view->colors[E_DAY_VIEW_COLOR_BG_TOP_CANVAS] = dark_bg;
	day_view->colors[E_DAY_VIEW_COLOR_BG_TOP_CANVAS_SELECTED] = selected_bg;
	day_view->colors[E_DAY_VIEW_COLOR_BG_TOP_CANVAS_GRID] = light_bg;
	day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR] = selected_bg;
	day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND] = base_bg;
	day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER] = dark_bg;
	day_view->colors[E_DAY_VIEW_COLOR_LONG_EVENT_BACKGROUND] = base_bg;
	day_view->colors[E_DAY_VIEW_COLOR_LONG_EVENT_BORDER] = dark_bg;

	if (!day_view->priv->today_background_color)
		day_view->colors[E_DAY_VIEW_COLOR_BG_MULTIDAY_TODAY] = get_today_background (day_view->colors[E_DAY_VIEW_COLOR_BG_WORKING]);

	bg_bg.red = 0.5;
	bg_bg.green = 1.0;
	bg_bg.blue = 1.0;
	bg_bg.alpha = 1.0;

	day_view->colors[E_DAY_VIEW_COLOR_MARCUS_BAINS_LINE] = bg_bg;
}

static void
e_day_view_update_top_scroll (EDayView *day_view,
                              gboolean scroll_to_top)
{
	GtkAllocation allocation;
	gint top_rows, top_canvas_height;
	gdouble old_x2, old_y2, new_x2, new_y2;

	/* Set the height of the top canvas based on the row height and the
	 * number of rows needed (min 1 + 1 for the dates + 1 space for DnD).*/
	top_rows = MAX (1, day_view->rows_in_top_display);
	top_canvas_height = (top_rows + 1) * day_view->top_row_height;
	if (top_rows <= E_DAY_VIEW_MAX_ROWS_AT_TOP) {
		gtk_widget_set_size_request (day_view->top_canvas, -1, top_canvas_height);
		gtk_widget_hide (day_view->tc_vscrollbar);
	} else {
		gtk_widget_set_size_request (day_view->top_canvas, -1, (E_DAY_VIEW_MAX_ROWS_AT_TOP + 1) * day_view->top_row_height);
		gtk_widget_show (day_view->tc_vscrollbar);
	}

	/* Set the scroll region of the top canvas */
	gnome_canvas_get_scroll_region (
		GNOME_CANVAS (day_view->top_canvas),
		NULL, NULL, &old_x2, &old_y2);
	gtk_widget_get_allocation (day_view->top_canvas, &allocation);
	new_x2 = allocation.width - 1;
	new_y2 = (MAX (1, day_view->rows_in_top_display) + 1) * day_view->top_row_height - 1;
	if (old_x2 != new_x2 || old_y2 != new_y2) {
		gnome_canvas_set_scroll_region (
			GNOME_CANVAS (day_view->top_canvas),
			0, 0, new_x2, new_y2);

		if (scroll_to_top)
			gnome_canvas_scroll_to (GNOME_CANVAS (day_view->top_canvas), 0, 0);
	}
	new_y2 = day_view->top_row_height - 1 - 2;
	gnome_canvas_get_scroll_region (GNOME_CANVAS (day_view->top_dates_canvas), NULL, NULL, &old_x2, &old_y2);

	if (old_x2 != new_x2 || old_y2 != new_y2) {
		gnome_canvas_set_scroll_region (GNOME_CANVAS (day_view->top_dates_canvas), 0, 0, new_x2, new_y2);
		gnome_canvas_scroll_to (GNOME_CANVAS (day_view->top_dates_canvas), 0, 0);
	}
}

static void
e_day_view_recalc_cell_sizes (EDayView *day_view)
{
	/* An array of dates, one for each month in the year 2000. They must
	 * all be Sundays. */
	static const gint days[12] = { 23, 20, 19, 23, 21, 18,
				      23, 20, 17, 22, 19, 24 };
	gfloat width, offset;
	gint day, max_width;
	struct tm date_tm;
	gchar buffer[128];
	GtkAllocation allocation;
	PangoContext *pango_context;
	PangoLayout *layout;
	gint pango_width;
	gint days_shown;

	days_shown = e_day_view_get_days_shown (day_view);

	gtk_widget_get_allocation (day_view->main_canvas, &allocation);

	/* Set up Pango prerequisites */
	pango_context = gtk_widget_get_pango_context (GTK_WIDGET (day_view));
	layout = pango_layout_new (pango_context);

	/* Calculate the column sizes, using floating point so that pixels
	 * get divided evenly. Note that we use one more element than the
	 * number of columns, to make it easy to get the column widths. */
	width = allocation.width;
	if (days_shown == 1)
		width = MAX (width, day_view->max_cols * (E_DAY_VIEW_MIN_DAY_COL_WIDTH + E_DAY_VIEW_GAP_WIDTH) - E_DAY_VIEW_MIN_DAY_COL_WIDTH - 1);
	width /= days_shown;
	offset = 0;
	for (day = 0; day <= days_shown; day++) {
		day_view->day_offsets[day] = floor (offset + 0.5);
		offset += width;
	}

	/* Calculate the days widths based on the offsets. */
	for (day = 0; day < days_shown; day++) {
		day_view->day_widths[day] = day_view->day_offsets[day + 1] - day_view->day_offsets[day];
	}

	/* Determine which date format to use, based on the column widths.
	 * We want to check the widths using the longest full or abbreviated
	 * month name and the longest full or abbreviated weekday name, as
	 * appropriate. */
	max_width = day_view->day_widths[0];

	memset (&date_tm, 0, sizeof (date_tm));
	date_tm.tm_year = 100;

	/* Try "Thursday 21 January". */
	date_tm.tm_mon = day_view->longest_month_name;
	date_tm.tm_mday = days[date_tm.tm_mon]
		+ day_view->longest_weekday_name;
	date_tm.tm_wday = day_view->longest_weekday_name;
	date_tm.tm_isdst = -1;
	/* strftime format %A = full weekday name, %d = day of month,
	 * %B = full month name. Don't use any other specifiers. */
	e_utf8_strftime (buffer, sizeof (buffer), _("%A %d %B"), &date_tm);
	pango_layout_set_text (layout, buffer, -1);
	pango_layout_get_pixel_size (layout, &pango_width, NULL);

	if (pango_width < max_width) {
		day_view->date_format = E_DAY_VIEW_DATE_FULL;
		goto exit;
	}

	/* Try "Thu 21 Jan". */
	date_tm.tm_mon = day_view->longest_abbreviated_month_name;
	date_tm.tm_mday = days[date_tm.tm_mon]
		+ day_view->longest_abbreviated_weekday_name;
	date_tm.tm_wday = day_view->longest_abbreviated_weekday_name;
	date_tm.tm_isdst = -1;
	/* strftime format %a = abbreviated weekday name, %d = day of month,
	 * %b = abbreviated month name. Don't use any other specifiers.
	 * xgettext:no-c-format */
	e_utf8_strftime (buffer, sizeof (buffer), _("%a %d %b"), &date_tm);
	pango_layout_set_text (layout, buffer, -1);
	pango_layout_get_pixel_size (layout, &pango_width, NULL);

	if (pango_width < max_width) {
		day_view->date_format = E_DAY_VIEW_DATE_ABBREVIATED;
		goto exit;
	}

	/* Try "23 Jan". */
	date_tm.tm_mon = day_view->longest_abbreviated_month_name;
	date_tm.tm_mday = 23;
	date_tm.tm_wday = 0;
	date_tm.tm_isdst = -1;
	/* strftime format %d = day of month, %b = abbreviated month name.
	 * Don't use any other specifiers.
	 * xgettext:no-c-format */
	e_utf8_strftime (buffer, sizeof (buffer), _("%d %b"), &date_tm);
	pango_layout_set_text (layout, buffer, -1);
	pango_layout_get_pixel_size (layout, &pango_width, NULL);

	if (pango_width < max_width)
		day_view->date_format = E_DAY_VIEW_DATE_NO_WEEKDAY;
	else
		day_view->date_format = E_DAY_VIEW_DATE_SHORT;

exit:
	g_object_unref (layout);
}

/* This calls a given function for each event instance (in both views).
 * If the callback returns FALSE the iteration is stopped.
 * Note that it is safe for the callback to remove the event (since we
 * step backwards through the arrays). */
static void
e_day_view_foreach_event (EDayView *day_view,
                          EDayViewForeachEventCallback callback,
                          gpointer data)
{
	gint day, event_num;
	gint days_shown;

	days_shown = e_day_view_get_days_shown (day_view);

	for (day = 0; day < days_shown; day++) {
		for (event_num = day_view->events[day]->len - 1;
		     event_num >= 0;
		     event_num--) {
			if (!(*callback) (day_view, day, event_num, data))
				return;
		}
	}

	for (event_num = day_view->long_events->len - 1;
	     event_num >= 0;
	     event_num--) {
		if (!(*callback) (day_view, E_DAY_VIEW_LONG_EVENT, event_num,
				  data))
			return;
	}
}

/* This calls a given function for each event instance that matches the given
 * uid. If the callback returns FALSE the iteration is stopped.
 * Note that it is safe for the callback to remove the event (since we
 * step backwards through the arrays). */
static void
e_day_view_foreach_event_with_uid (EDayView *day_view,
                                   const gchar *uid,
                                   EDayViewForeachEventCallback callback,
                                   gpointer data)
{
	EDayViewEvent *event;
	gint day, event_num;
	gint days_shown;
	const gchar *u;

	if (!uid)
		return;

	days_shown = e_day_view_get_days_shown (day_view);

	for (day = 0; day < days_shown; day++) {
		for (event_num = day_view->events[day]->len - 1;
		     event_num >= 0;
		     event_num--) {
			event = &g_array_index (day_view->events[day],
						EDayViewEvent, event_num);

			if (!is_comp_data_valid (event))
				continue;

			u = i_cal_component_get_uid (event->comp_data->icalcomp);
			if (u && !strcmp (uid, u)) {
				if (!(*callback) (day_view, day, event_num, data))
					return;
			}
		}
	}

	for (event_num = day_view->long_events->len - 1;
	     event_num >= 0;
	     event_num--) {
		event = &g_array_index (day_view->long_events,
					EDayViewEvent, event_num);

		if (!is_comp_data_valid (event))
			continue;

		u = i_cal_component_get_uid (event->comp_data->icalcomp);
		if (u && !strcmp (uid, u)) {
			if (!(*callback) (day_view, E_DAY_VIEW_LONG_EVENT, event_num, data))
				return;
		}
	}
}

static gboolean
e_day_view_remove_event_cb (EDayView *day_view,
                            gint day,
                            gint event_num,
                            gpointer data)
{
	EDayViewEvent *event;

	if (day == E_DAY_VIEW_LONG_EVENT) {
		if (!is_array_index_in_bounds (day_view->long_events, event_num))
			return TRUE;

		event = &g_array_index (day_view->long_events,
					EDayViewEvent, event_num);
	} else {
		if (!is_array_index_in_bounds (day_view->events[day], event_num))
			return TRUE;

		event = &g_array_index (day_view->events[day],
					EDayViewEvent, event_num);
	}

	/* If we were editing this event, set editing_event_day to -1 so
	 * on_editing_stopped doesn't try to update the event. */
	if (day_view->editing_event_num == event_num && day_view->editing_event_day == day) {
		cancel_editing (day_view);
		day_view->editing_event_num = -1;
		day_view->editing_event_day = -1;
		g_object_notify (G_OBJECT (day_view), "is-editing");
	} else if (day_view->editing_event_num > event_num && day_view->editing_event_day == day) {
		day_view->editing_event_num--;
	}

	if (day_view->popup_event_num == event_num && day_view->popup_event_day == day) {
		e_day_view_set_popup_event (day_view, -1, -1);
	} else if (day_view->popup_event_num > event_num && day_view->popup_event_day == day) {
		day_view->popup_event_num--;
	}

	if (day_view->resize_bars_event_num >= event_num && day_view->resize_bars_event_day == day) {
		if (day_view->resize_bars_event_num == event_num) {
			day_view->resize_bars_event_num = -1;
			day_view->resize_bars_event_day = -1;
		} else {
			day_view->resize_bars_event_num--;
		}
	}

	if (day_view->resize_event_num >= event_num && day_view->resize_event_day == day) {
		if (day_view->resize_event_num == event_num) {
			e_day_view_abort_resize (day_view);
			day_view->resize_event_num = -1;
			day_view->resize_event_day = -1;
		} else {
			day_view->resize_event_num--;
		}
	}

	if (day_view->pressed_event_num >= event_num && day_view->pressed_event_day == day) {
		if (day_view->pressed_event_num == event_num) {
			day_view->pressed_event_num = -1;
			day_view->pressed_event_day = -1;
		} else {
			day_view->pressed_event_num--;
		}
	}

	if (day_view->drag_event_num >= event_num && day_view->drag_event_day == day) {
		if (day_view->drag_event_num == event_num) {
			day_view->drag_event_num = -1;
			day_view->drag_event_day = -1;
			day_view->priv->drag_event_is_recurring = FALSE;
			if (day_view->priv->drag_context) {
				gtk_drag_cancel (day_view->priv->drag_context);
			}
		} else {
			day_view->drag_event_num--;
		}
	}

	if (event->canvas_item)
		g_object_run_dispose (G_OBJECT (event->canvas_item));

	if (is_comp_data_valid (event))
		g_object_unref (event->comp_data);
	event->comp_data = NULL;

	if (day == E_DAY_VIEW_LONG_EVENT) {
		g_array_remove_index (day_view->long_events, event_num);
		day_view->long_events_need_layout = TRUE;
		gtk_widget_grab_focus (GTK_WIDGET (day_view->top_canvas));
	} else {
		g_array_remove_index (day_view->events[day], event_num);
		day_view->need_layout[day] = TRUE;
		gtk_widget_grab_focus (GTK_WIDGET (day_view->main_canvas));
	}

	return TRUE;
}

/* Checks if the users participation status is NEEDS-ACTION and shows the summary as bold text */
static void
set_style_from_attendee (EDayViewEvent *event,
			 ESourceRegistry *registry)
{
	ECalComponent *comp;
	GSList *attendees = NULL, *l;
	gchar *address;
	ICalParameterPartstat partstat = I_CAL_PARTSTAT_NONE;

	if (!is_comp_data_valid (event))
		return;

	comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (event->comp_data->icalcomp));
	if (!comp)
		return;

	address = itip_get_comp_attendee (registry, comp, event->comp_data->client);
	attendees = e_cal_component_get_attendees (comp);
	for (l = attendees; l && address; l = l->next) {
		ECalComponentAttendee *attendee = l->data;
		const gchar *value, *sentby;

		value = e_cal_util_get_attendee_email (attendee);
		sentby = e_cal_component_attendee_get_sentby (attendee);

		if (e_cal_util_email_addresses_equal (value, address) ||
		    e_cal_util_email_addresses_equal (sentby, address)) {
			partstat = e_cal_component_attendee_get_partstat (attendee);
			break;
		}
	}

	if (i_cal_component_get_status (event->comp_data->icalcomp) == I_CAL_STATUS_CANCELLED)
		gnome_canvas_item_set (event->canvas_item, "strikeout", TRUE, NULL);

	/* The attendee has not yet accepted the meeting, display the summary as bolded.
	 * If the attendee is not present, it might have come through a mailing list.
	 * In that case, we never show the meeting as bold even if it is unaccepted. */
	if (partstat == I_CAL_PARTSTAT_NEEDSACTION)
		gnome_canvas_item_set (event->canvas_item, "bold", TRUE, NULL);
	else if (partstat == I_CAL_PARTSTAT_DECLINED)
		gnome_canvas_item_set (event->canvas_item, "strikeout", TRUE, NULL);
	else if (partstat == I_CAL_PARTSTAT_TENTATIVE)
		gnome_canvas_item_set (event->canvas_item, "italic", TRUE, NULL);
	else if (partstat == I_CAL_PARTSTAT_DELEGATED)
		gnome_canvas_item_set (event->canvas_item, "italic", TRUE, "strikeout", TRUE, NULL);

	g_slist_free_full (attendees, e_cal_component_attendee_free);
	g_free (address);
	g_object_unref (comp);
}

/* This updates the text shown for an event. If the event start or end do not
 * lie on a row boundary, the time is displayed before the summary. */
static void
e_day_view_update_event_label (EDayView *day_view,
                               gint day,
                               gint event_num)
{
	EDayViewEvent *event;
	ECalendarView *cal_view;
	ESourceRegistry *registry;
	ECalModel *model;
	gboolean free_text = FALSE, editing_event = FALSE, short_event = FALSE;
	ICalProperty *prop;
	const gchar *summary;
	gchar *text;
	gint time_divisions;
	gint interval;

	if (!is_array_index_in_bounds (day_view->events[day], event_num))
		return;

	event = &g_array_index (day_view->events[day], EDayViewEvent, event_num);

	/* If the event isn't visible just return. */
	if (!event->canvas_item || !is_comp_data_valid (event))
		return;

	prop = e_cal_util_component_find_property_for_locale (event->comp_data->icalcomp, I_CAL_SUMMARY_PROPERTY, NULL);
	summary = prop ? i_cal_property_get_summary (prop) : NULL;
	text = summary ? (gchar *) summary : (gchar *) "";

	if (day_view->editing_event_day == day
	    && day_view->editing_event_num == event_num)
		editing_event = TRUE;

	interval = event->end_minute - event->start_minute;

	cal_view = E_CALENDAR_VIEW (day_view);
	model = e_calendar_view_get_model (cal_view);
	time_divisions = e_calendar_view_get_time_divisions (cal_view);

	registry = e_cal_model_get_registry (model);

	if ((interval / time_divisions) >= 2)
		short_event = FALSE;
	else if ((interval % time_divisions) == 0) {
		if (((event->end_minute % time_divisions) == 0) ||
		    ((event->start_minute % time_divisions) == 0)) {
			short_event = TRUE;
		}
	} else
		short_event = FALSE;

	if (!editing_event) {
		if (!short_event) {
			ICalProperty *prop_description;
			const gchar *description, *location;
			gint days_shown;

			days_shown = e_day_view_get_days_shown (day_view);
			prop_description = e_cal_util_component_find_property_for_locale (event->comp_data->icalcomp, I_CAL_DESCRIPTION_PROPERTY, NULL);
			description = prop_description ? i_cal_property_get_description (prop_description) : NULL;
			location = i_cal_component_get_location (event->comp_data->icalcomp);

			if (description && *description) {
				if (location && *location)
					text = g_strdup_printf (" \n%s%c(%s)\n\n%s", text, days_shown == 1 ? ' ' : '\n', location, description);
				else
					text = g_strdup_printf (" \n%s\n\n%s", text, description);
			} else if (location && *location)
				text = g_strdup_printf (" \n%s%c(%s)", text, days_shown == 1 ? ' ' : '\n', location);
			else
				text = g_strdup_printf (" \n%s", text);

			free_text = TRUE;

			g_clear_object (&prop_description);
		}
	}

	gnome_canvas_item_set (
		event->canvas_item,
		"text", text,
		NULL);

	if (e_cal_util_component_has_attendee (event->comp_data->icalcomp))
		set_style_from_attendee (event, registry);
	else if (i_cal_component_get_status (event->comp_data->icalcomp) == I_CAL_STATUS_CANCELLED)
		gnome_canvas_item_set (event->canvas_item, "strikeout", TRUE, NULL);

	if (free_text)
		g_free (text);
	g_clear_object (&prop);
}

static void
e_day_view_update_long_event_label (EDayView *day_view,
                                    gint event_num)
{
	EDayViewEvent *event;
	ECalendarView *cal_view;
	ECalModel *model;
	ESourceRegistry *registry;
	gchar *summary;

	cal_view = E_CALENDAR_VIEW (day_view);
	model = e_calendar_view_get_model (cal_view);

	registry = e_cal_model_get_registry (model);

	if (!is_array_index_in_bounds (day_view->long_events, event_num))
		return;

	event = &g_array_index (day_view->long_events, EDayViewEvent,
				event_num);

	/* If the event isn't visible just return. */
	if (!event->canvas_item || !is_comp_data_valid (event))
		return;

	summary = e_calendar_view_dup_component_summary (event->comp_data->icalcomp);

	gnome_canvas_item_set (
		event->canvas_item,
		"text", summary ? summary : "",
		NULL);

	g_free (summary);

	if (e_cal_util_component_has_attendee (event->comp_data->icalcomp))
		set_style_from_attendee (event, registry);
	else if (i_cal_component_get_status (event->comp_data->icalcomp) == I_CAL_STATUS_CANCELLED)
		gnome_canvas_item_set (event->canvas_item, "strikeout", TRUE, NULL);
}

/* Finds the day and index of the event with the given canvas item.
 * If it is a long event, -1 is returned as the day.
 * Returns TRUE if the event was found. */
gboolean
e_day_view_find_event_from_item (EDayView *day_view,
                                 GnomeCanvasItem *item,
                                 gint *day_return,
                                 gint *event_num_return)
{
	EDayViewEvent *event;
	gint day, event_num;
	gint days_shown;

	days_shown = e_day_view_get_days_shown (day_view);

	for (day = 0; day < days_shown; day++) {
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
 * If it is a long event, E_DAY_VIEW_LONG_EVENT is returned as the day.
 * Returns TRUE if an event with the uid was found.
 * Note that for recurring events there may be several EDayViewEvents, one
 * for each instance, all with the same iCalObject and uid. So only use this
 * function if you know the event doesn't recur or you are just checking to
 * see if any events with the uid exist. */
static gboolean
e_day_view_find_event_from_uid (EDayView *day_view,
                                ECalClient *client,
                                const gchar *uid,
                                const gchar *rid,
                                gint *day_return,
                                gint *event_num_return)
{
	EDayViewEvent *event;
	gint day, event_num;
	gint days_shown;
	const gchar *u;
	gchar *r = NULL;

	if (!uid)
		return FALSE;

	days_shown = e_day_view_get_days_shown (day_view);

	for (day = 0; day < days_shown; day++) {
		for (event_num = 0; event_num < day_view->events[day]->len;
		     event_num++) {
			event = &g_array_index (day_view->events[day],
						EDayViewEvent, event_num);

			if (!is_comp_data_valid (event))
				continue;

			if (event->comp_data->client != client)
				continue;

			u = i_cal_component_get_uid (event->comp_data->icalcomp);
			if (u && !strcmp (uid, u)) {
				if (rid && *rid) {
					r = e_cal_util_component_get_recurid_as_string (event->comp_data->icalcomp);

					if (!r || !*r || strcmp (rid, r) != 0) {
						g_free (r);
						continue;
					}

					g_free (r);
				}

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

		if (!is_comp_data_valid (event))
			continue;

		if (event->comp_data->client != client)
			continue;

		u = i_cal_component_get_uid (event->comp_data->icalcomp);
		if (u && !strcmp (uid, u)) {
			*day_return = E_DAY_VIEW_LONG_EVENT;
			*event_num_return = event_num;
			return TRUE;
		}
	}

	return FALSE;
}

static void
e_day_view_set_selected_time_range_in_top_visible (EDayView *day_view,
                                                   time_t start_time,
                                                   time_t end_time)
{
	gint start_row, start_col, end_row, end_col;
	gboolean need_redraw = FALSE, start_in_grid, end_in_grid;

	g_return_if_fail (E_IS_DAY_VIEW (day_view));

	/* Set the selection. */
	start_in_grid = e_day_view_convert_time_to_grid_position (
		day_view,
		start_time,
		&start_col,
		&start_row);
	end_in_grid = e_day_view_convert_time_to_grid_position (
		day_view,
		end_time - 60,
		&end_col,
		&end_row);

	if (!start_in_grid)
		start_col = 0;
	if (!end_in_grid)
		end_col = e_day_view_get_days_shown (day_view) - 1;

	if (start_row != day_view->selection_start_row
	    || start_col != day_view->selection_start_day) {
		need_redraw = TRUE;
		day_view->selection_in_top_canvas = TRUE;
		day_view->selection_start_row = -1;
		day_view->selection_start_day = start_col;
	}

	if (end_row != day_view->selection_end_row
	    || end_col != day_view->selection_end_day) {
		need_redraw = TRUE;
		day_view->selection_in_top_canvas = TRUE;
		day_view->selection_end_row = -1;
		day_view->selection_end_day = end_col;
	}

	if (need_redraw) {
		gtk_widget_queue_draw (day_view->top_canvas);
		gtk_widget_queue_draw (day_view->top_dates_canvas);
		gtk_widget_queue_draw (day_view->main_canvas);
	}
}

static void
e_day_view_set_selected_time_range_visible (EDayView *day_view,
                                            time_t start_time,
                                            time_t end_time)
{
	gint work_day_start_hour;
	gint work_day_start_minute;
	gint work_day_end_hour;
	gint work_day_end_minute;
	gint start_row, start_col, end_row, end_col;
	gboolean need_redraw = FALSE, start_in_grid, end_in_grid;

	g_return_if_fail (E_IS_DAY_VIEW (day_view));

	/* Set the selection. */
	start_in_grid = e_day_view_convert_time_to_grid_position (
		day_view,
		start_time,
		&start_col,
		&start_row);
	end_in_grid = e_day_view_convert_time_to_grid_position (
		day_view,
		end_time - 60,
		&end_col,
		&end_row);

	e_day_view_get_work_day_range_for_day (day_view, start_col,
		&work_day_start_hour, &work_day_start_minute,
		&work_day_end_hour, &work_day_end_minute);

	/* If either of the times isn't in the grid, or the selection covers
	 * an entire day, we set the selection to 1 row from the start of the
	 * working day, in the day corresponding to the start time. */
	if (!start_in_grid || !end_in_grid
	    || (start_row == 0 && end_row == day_view->rows - 1)) {
		end_col = start_col;

		start_row = e_day_view_convert_time_to_row (
			day_view, work_day_start_hour, work_day_start_minute);
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
		gtk_widget_queue_draw (day_view->top_dates_canvas);
		gtk_widget_queue_draw (day_view->main_canvas);
	}
}

/* Finds the start of the working week which includes the given time. */
static time_t
e_day_view_find_work_week_start (EDayView *day_view,
                                 time_t start_time)
{
	GDate date;
	ECalModel *model;
	guint offset;
	GDateWeekday weekday;
	GDateWeekday first_work_day;
	ICalTime *tt = NULL;
	ICalTimezone *zone;
	time_t res;

	model = e_calendar_view_get_model (E_CALENDAR_VIEW (day_view));
	zone = e_cal_model_get_timezone (model);

	time_to_gdate_with_zone (&date, start_time, zone);

	/* The start of the work-week is the first working day after the
	 * week start day. */

	/* Get the weekday corresponding to start_time. */
	weekday = g_date_get_weekday (&date);

	/* Calculate the first working day of the week. */
	first_work_day = e_cal_model_get_work_day_first (model);
	if (first_work_day == G_DATE_BAD_WEEKDAY)
		first_work_day = e_cal_model_get_week_start_day (model);

	/* Calculate how many days we need to go back to the first workday. */
	if (weekday < first_work_day)
		offset = (weekday + 7) - first_work_day;
	else
		offset = weekday - first_work_day;

	if (offset > 0)
		g_date_subtract_days (&date, offset);

	tt = i_cal_time_new_null_time ();

	i_cal_time_set_date (tt,
		g_date_get_year (&date),
		g_date_get_month (&date),
		g_date_get_day (&date));

	res = i_cal_time_as_timet_with_zone (tt, zone);

	g_clear_object (&tt);

	return res;
}

static void
e_day_view_recalc_day_starts (EDayView *day_view,
                              time_t start_time)
{
	gint day;
	gchar *str;
	gint days_shown;
	ICalTime *tt;
	GDate dt;

	days_shown = e_day_view_get_days_shown (day_view);
	if (days_shown <= 0)
		return;

	day_view->day_starts[0] = start_time;
	for (day = 1; day <= days_shown; day++) {
		day_view->day_starts[day] = time_add_day_with_zone (day_view->day_starts[day - 1], 1, e_calendar_view_get_timezone (E_CALENDAR_VIEW (day_view)));
	}

	day_view->lower = start_time;
	day_view->upper = day_view->day_starts[days_shown];

	tt = i_cal_time_new_from_timet_with_zone (day_view->day_starts[0], FALSE, e_calendar_view_get_timezone (E_CALENDAR_VIEW (day_view)));
	g_date_clear (&dt, 1);
	g_date_set_dmy (&dt, i_cal_time_get_day (tt), i_cal_time_get_month (tt), i_cal_time_get_year (tt));
	/* To Translators: the %d stands for a week number, it's value between 1 and 52/53 */
	str = g_strdup_printf (_("Week %d"), g_date_get_iso8601_week_of_year (&dt));
	gtk_label_set_text (GTK_LABEL (day_view->week_number_label), str);
	g_free (str);

	e_day_view_recalc_work_week (day_view);

	g_clear_object (&tt);
}

/* Whether we are displaying a work-week, in which case the display always
 * starts on the first day of the working week. */
gboolean
e_day_view_get_work_week_view (EDayView *day_view)
{
	g_return_val_if_fail (E_IS_DAY_VIEW (day_view), FALSE);

	return day_view->priv->work_week_view;
}

void
e_day_view_set_work_week_view (EDayView *day_view,
                               gboolean work_week_view)
{
	g_return_if_fail (E_IS_DAY_VIEW (day_view));

	if (work_week_view == day_view->priv->work_week_view)
		return;

	day_view->priv->work_week_view = work_week_view;

	e_day_view_recalc_work_week (day_view);
}

gint
e_day_view_get_days_shown (EDayView *day_view)
{
	g_return_val_if_fail (E_IS_DAY_VIEW (day_view), -1);

	return day_view->priv->days_shown;
}

void
e_day_view_set_days_shown (EDayView *day_view,
                           gint days_shown)
{
	g_return_if_fail (E_IS_DAY_VIEW (day_view));
	g_return_if_fail (days_shown >= 1);
	g_return_if_fail (days_shown <= E_DAY_VIEW_MAX_DAYS);

	if (days_shown == day_view->priv->days_shown)
		return;

	day_view->priv->days_shown = days_shown;

	/* If the date isn't set, just return. */
	if (day_view->lower == 0 && day_view->upper == 0)
		return;

	e_day_view_recalc_day_starts (day_view, day_view->lower);
	e_day_view_recalc_cell_sizes (day_view);

	e_day_view_update_query (day_view);
}

static void
e_day_view_recalc_work_week_days_shown (EDayView *day_view)
{
	ECalModel *model;
	GDateWeekday first_work_day;
	GDateWeekday last_work_day;
	gint days_shown;

	model = e_calendar_view_get_model (E_CALENDAR_VIEW (day_view));

	/* Find the first working day in the week. */
	first_work_day = e_cal_model_get_work_day_first (model);

	if (first_work_day != G_DATE_BAD_WEEKDAY) {
		last_work_day = e_cal_model_get_work_day_last (model);

		/* Now calculate the days we need to show to include all the
		 * working days in the week. Add 1 to make it inclusive. */
		days_shown = e_weekday_get_days_between (
			first_work_day, last_work_day) + 1;
	} else {
		/* If no working days are set, just use 7. */
		days_shown = 7;
	}

	e_day_view_set_days_shown (day_view, days_shown);
}

/* Force a redraw of the Marcus Bains Lines */
void
e_day_view_marcus_bains_update (EDayView *day_view)
{
	g_return_if_fail (E_IS_DAY_VIEW (day_view));
	gtk_widget_queue_draw (day_view->main_canvas);
	gtk_widget_queue_draw (day_view->time_canvas);
}

gboolean
e_day_view_marcus_bains_get_show_line (EDayView *day_view)
{
	g_return_val_if_fail (E_IS_DAY_VIEW (day_view), FALSE);

	return day_view->priv->marcus_bains_show_line;
}

void
e_day_view_marcus_bains_set_show_line (EDayView *day_view,
                                       gboolean show_line)
{
	g_return_if_fail (E_IS_DAY_VIEW (day_view));

	if (show_line == day_view->priv->marcus_bains_show_line)
		return;

	day_view->priv->marcus_bains_show_line = show_line;

	e_day_view_marcus_bains_update (day_view);

	if (!day_view->priv->marcus_bains_update_id)
		day_view_refresh_marcus_bains_line (day_view);

	g_object_notify (G_OBJECT (day_view), "marcus-bains-show-line");
}

gboolean
e_day_view_get_draw_flat_events (EDayView *day_view)
{
	g_return_val_if_fail (E_IS_DAY_VIEW (day_view), FALSE);

	return day_view->priv->draw_flat_events;
}

void
e_day_view_set_draw_flat_events (EDayView *day_view,
				 gboolean draw_flat_events)
{
	g_return_if_fail (E_IS_DAY_VIEW (day_view));

	if ((day_view->priv->draw_flat_events ? 1 : 0) == (draw_flat_events ? 1 : 0))
		return;

	day_view->priv->draw_flat_events = draw_flat_events;

	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->top_dates_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);

	g_object_notify (G_OBJECT (day_view), "draw-flat-events");
}

const gchar *
e_day_view_marcus_bains_get_day_view_color (EDayView *day_view)
{
	g_return_val_if_fail (E_IS_DAY_VIEW (day_view), NULL);

	return day_view->priv->marcus_bains_day_view_color;
}

void
e_day_view_marcus_bains_set_day_view_color (EDayView *day_view,
                                            const gchar *day_view_color)
{
	g_return_if_fail (E_IS_DAY_VIEW (day_view));

	g_free (day_view->priv->marcus_bains_day_view_color);
	day_view->priv->marcus_bains_day_view_color = g_strdup (day_view_color);

	e_day_view_marcus_bains_update (day_view);

	g_object_notify (G_OBJECT (day_view), "marcus-bains-day-view-color");
}

const gchar *
e_day_view_marcus_bains_get_time_bar_color (EDayView *day_view)
{
	g_return_val_if_fail (E_IS_DAY_VIEW (day_view), NULL);

	return day_view->priv->marcus_bains_time_bar_color;
}

void
e_day_view_marcus_bains_set_time_bar_color (EDayView *day_view,
                                            const gchar *time_bar_color)
{
	g_return_if_fail (E_IS_DAY_VIEW (day_view));

	g_free (day_view->priv->marcus_bains_time_bar_color);
	day_view->priv->marcus_bains_time_bar_color = g_strdup (time_bar_color);

	e_day_view_marcus_bains_update (day_view);

	g_object_notify (G_OBJECT (day_view), "marcus-bains-time-bar-color");
}

const gchar *
e_day_view_get_today_background_color (EDayView *day_view)
{
	g_return_val_if_fail (E_IS_DAY_VIEW (day_view), NULL);

	return day_view->priv->today_background_color;
}

void
e_day_view_set_today_background_color (EDayView *day_view,
				       const gchar *color)
{
	GdkRGBA rgba;

	g_return_if_fail (E_IS_DAY_VIEW (day_view));

	if (g_strcmp0 (color, day_view->priv->today_background_color) == 0)
		return;

	if (color && gdk_rgba_parse (&rgba, color)) {
		g_free (day_view->priv->today_background_color);
		day_view->priv->today_background_color = g_strdup (color);
		day_view->colors[E_DAY_VIEW_COLOR_BG_MULTIDAY_TODAY].red = 0xFFFF * rgba.red;
		day_view->colors[E_DAY_VIEW_COLOR_BG_MULTIDAY_TODAY].green = 0xFFFF * rgba.green;
		day_view->colors[E_DAY_VIEW_COLOR_BG_MULTIDAY_TODAY].blue = 0xFFFF * rgba.blue;
	} else if (day_view->priv->today_background_color) {
		g_free (day_view->priv->today_background_color);
		day_view->priv->today_background_color = NULL;
		day_view->colors[E_DAY_VIEW_COLOR_BG_MULTIDAY_TODAY] = get_today_background (day_view->colors[E_DAY_VIEW_COLOR_BG_WORKING]);
	} else {
		return;
	}

	gtk_widget_queue_draw (day_view->main_canvas);

	g_object_notify (G_OBJECT (day_view), "today-background-color");
}

/* Whether we display event end times in the main canvas. */
gboolean
e_day_view_get_show_event_end_times (EDayView *day_view)
{
	g_return_val_if_fail (E_IS_DAY_VIEW (day_view), TRUE);

	return day_view->show_event_end_times;
}

void
e_day_view_set_show_event_end_times (EDayView *day_view,
                                     gboolean show)
{
	g_return_if_fail (E_IS_DAY_VIEW (day_view));

	if (day_view->show_event_end_times != show) {
		day_view->show_event_end_times = show;
		e_day_view_foreach_event (day_view,
					  e_day_view_set_show_times_cb, NULL);
	}
}

/* This is a callback used to update all day event labels. */
static gboolean
e_day_view_set_show_times_cb (EDayView *day_view,
                              gint day,
                              gint event_num,
                              gpointer data)
{
	if (day != E_DAY_VIEW_LONG_EVENT) {
		e_day_view_update_event_label (day_view, day, event_num);
	}

	return TRUE;
}

static void
e_day_view_recalc_work_week (EDayView *day_view)
{
	time_t lower;

	/* If we aren't showing the work week, just return. */
	if (!e_day_view_get_work_week_view (day_view))
		return;

	e_day_view_recalc_work_week_days_shown (day_view);

	/* If the date isn't set, just return. */
	if (day_view->lower == 0 && day_view->upper == 0)
		return;

	lower = e_day_view_find_work_week_start (day_view, day_view->lower);
	if (lower != day_view->lower) {
		/* Reset the selection, as it may disappear. */
		day_view->selection_start_day = -1;

		e_day_view_recalc_day_starts (day_view, lower);
		e_day_view_update_query (day_view);

		/* This updates the date navigator. */
		e_day_view_update_calendar_selection_time (day_view);
	}
}

static gboolean
e_day_view_update_scroll_regions (EDayView *day_view)
{
	GtkAllocation main_canvas_allocation;
	GtkAllocation time_canvas_allocation;
	gdouble old_x2, old_y2, new_x2, new_y2;
	gboolean need_reshape = FALSE;

	gtk_widget_get_allocation (
		day_view->main_canvas, &main_canvas_allocation);
	gtk_widget_get_allocation (
		day_view->time_canvas, &time_canvas_allocation);

	/* Set the scroll region of the time canvas to its allocated width,
	 * but with the height the same as the main canvas. */
	gnome_canvas_get_scroll_region (
		GNOME_CANVAS (day_view->time_canvas),
		NULL, NULL, &old_x2, &old_y2);
	new_x2 = time_canvas_allocation.width - 1;
	new_y2 = MAX (
		day_view->rows * day_view->row_height,
		main_canvas_allocation.height - 1);
	if (old_x2 != new_x2 || old_y2 != new_y2)
		gnome_canvas_set_scroll_region (GNOME_CANVAS (day_view->time_canvas),
						0, 0, new_x2, new_y2);

	/* Set the scroll region of the main canvas to its allocated width,
	 * but with the height depending on the number of rows needed. */
	gnome_canvas_get_scroll_region (
		GNOME_CANVAS (day_view->main_canvas),
		NULL, NULL, &old_x2, &old_y2);
	new_x2 = main_canvas_allocation.width - 1;

	if (e_day_view_get_days_shown (day_view) == 1)
		new_x2 = MAX (new_x2, day_view->max_cols * (E_DAY_VIEW_MIN_DAY_COL_WIDTH + E_DAY_VIEW_GAP_WIDTH) - E_DAY_VIEW_MIN_DAY_COL_WIDTH - 1);

	if (old_x2 != new_x2 || old_y2 != new_y2) {
		need_reshape = TRUE;
		gnome_canvas_set_scroll_region (
			GNOME_CANVAS (day_view->main_canvas),
			0, 0, new_x2, new_y2);
	}

	if (new_x2 <= main_canvas_allocation.width - 1)
		gtk_widget_hide (day_view->mc_hscrollbar);
	else
		gtk_widget_show (day_view->mc_hscrollbar);

	return need_reshape;
}

/* This recalculates the number of rows to display, based on the time range
 * shown and the minutes per row. */
static void
e_day_view_recalc_num_rows (EDayView *day_view)
{
	ECalendarView *cal_view;
	gint time_divisions;
	gint hours, minutes, total_minutes;

	cal_view = E_CALENDAR_VIEW (day_view);
	time_divisions = e_calendar_view_get_time_divisions (cal_view);

	hours = day_view->last_hour_shown - day_view->first_hour_shown;
	/* This could be negative but it works out OK. */
	minutes = day_view->last_minute_shown - day_view->first_minute_shown;
	total_minutes = hours * 60 + minutes;
	day_view->rows = total_minutes / time_divisions;
}

/* Converts an hour and minute to a row in the canvas. Note that if we aren't
 * showing all 24 hours of the day, the returned row may be negative or
 * greater than day_view->rows. */
gint
e_day_view_convert_time_to_row (EDayView *day_view,
                                gint hour,
                                gint minute)
{
	ECalendarView *cal_view;
	gint time_divisions;
	gint total_minutes, start_minute, offset;

	cal_view = E_CALENDAR_VIEW (day_view);
	time_divisions = e_calendar_view_get_time_divisions (cal_view);

	total_minutes = hour * 60 + minute;
	start_minute = day_view->first_hour_shown * 60
		+ day_view->first_minute_shown;
	offset = total_minutes - start_minute;
	if (offset < 0)
		return -1;
	else
		return offset / time_divisions;
}

/* Converts an hour and minute to a y coordinate in the canvas. */
gint
e_day_view_convert_time_to_position (EDayView *day_view,
                                     gint hour,
                                     gint minute)
{
	ECalendarView *cal_view;
	gint time_divisions;
	gint total_minutes, start_minute, offset;

	cal_view = E_CALENDAR_VIEW (day_view);
	time_divisions = e_calendar_view_get_time_divisions (cal_view);

	total_minutes = hour * 60 + minute;
	start_minute = day_view->first_hour_shown * 60
		+ day_view->first_minute_shown;
	offset = total_minutes - start_minute;

	return offset * day_view->row_height / time_divisions;
}

static gboolean
e_day_view_on_top_canvas_button_press (GtkWidget *widget,
                                       GdkEvent *button_event,
                                       EDayView *day_view)
{
	gint event_x, event_y, day, event_num;
	ECalendarViewPosition pos;
	GtkLayout *layout;
	GdkWindow *window;
	GdkDevice *event_device;
	guint event_button = 0;
	guint32 event_time;

	layout = GTK_LAYOUT (widget);
	window = gtk_layout_get_bin_window (layout);

	gdk_event_get_button (button_event, &event_button);
	event_device = gdk_event_get_device (button_event);
	event_time = gdk_event_get_time (button_event);

	if (day_view->resize_event_num != -1)
		day_view->resize_event_num = -1;

	if (day_view->drag_event_num != -1)
		day_view->drag_event_num = -1;

	/* Convert the coords to the main canvas window, or return if the
	 * window is not found. */
	if (!e_day_view_convert_event_coords (
		day_view, button_event, window, &event_x, &event_y))
		return FALSE;

	pos = e_day_view_convert_position_in_top_canvas (
		day_view,
		event_x, event_y,
		&day, &event_num);

	if (pos == E_CALENDAR_VIEW_POS_OUTSIDE)
		return FALSE;

	if (pos != E_CALENDAR_VIEW_POS_NONE)
		return e_day_view_on_long_event_button_press (
			day_view,
			event_num,
			button_event,
			pos,
			event_x,
			event_y);

	e_day_view_stop_editing_event (day_view);

	if (event_button == 1) {
		GdkGrabStatus grab_status;

		if (button_event->type == GDK_2BUTTON_PRESS) {
			time_t dtstart, dtend;

			day_view_get_selected_time_range ((ECalendarView *) day_view, &dtstart, &dtend);
			if (dtstart < day_view->before_click_dtend && dtend > day_view->before_click_dtstart) {
				dtstart = day_view->before_click_dtstart;
				dtend = day_view->before_click_dtend;
				day_view_set_selected_time_range ((ECalendarView *) day_view, dtstart, dtend);
			}

			e_cal_ops_new_component_editor_from_model (
				e_calendar_view_get_model (E_CALENDAR_VIEW (day_view)), NULL,
				dtstart, dtend, calendar_config_get_prefer_meeting (), TRUE);
			return TRUE;
		}

		if (!gtk_widget_has_focus (GTK_WIDGET (day_view)))
			gtk_widget_grab_focus (GTK_WIDGET (day_view));

		grab_status = gdk_device_grab (
			event_device,
			window,
			GDK_OWNERSHIP_NONE,
			FALSE,
			GDK_POINTER_MOTION_MASK |
			GDK_BUTTON_RELEASE_MASK,
			NULL,
			event_time);

		if (grab_status == GDK_GRAB_SUCCESS) {
			g_warn_if_fail (day_view->grabbed_pointer == NULL);
			day_view->grabbed_pointer = g_object_ref (event_device);

			if (event_time - day_view->bc_event_time > 250)
				day_view_get_selected_time_range (
					E_CALENDAR_VIEW (day_view),
					&day_view->before_click_dtstart,
					&day_view->before_click_dtend);
			day_view->bc_event_time = event_time;
			e_day_view_start_selection (day_view, day, -1);
		}
	} else if (event_button == 3) {
		if (!gtk_widget_has_focus (GTK_WIDGET (day_view)))
			gtk_widget_grab_focus (GTK_WIDGET (day_view));

		if (day < day_view->selection_start_day || day > day_view->selection_end_day) {
			e_day_view_start_selection (day_view, day, -1);
			e_day_view_finish_selection (day_view);
		}

		e_day_view_on_event_right_click (day_view, button_event, -1, -1);
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
	GdkWindow *event_window;

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
	case GDK_ENTER_NOTIFY:
	case GDK_LEAVE_NOTIFY:
		event_x = event->crossing.x;
		event_y = event->crossing.y;
		event_window = event->crossing.window;
		break;
	default:
		/* Shouldn't get here. */
		g_return_val_if_reached (FALSE);
	}

	while (event_window && event_window != window
	       && event_window != gdk_get_default_root_window ()) {
		gdk_window_get_position (event_window, &win_x, &win_y);
		event_x += win_x;
		event_y += win_y;
		event_window = gdk_window_get_parent (event_window);
	}

	*x_return = event_x;
	*y_return = event_y;

	return (event_window == window) ? TRUE : FALSE;
}

static gboolean
e_day_view_on_main_canvas_button_press (GtkWidget *widget,
                                        GdkEvent *button_event,
                                        EDayView *day_view)
{
	gint event_x, event_y, row, day, event_num;
	ECalendarViewPosition pos;
	GtkLayout *layout;
	GdkWindow *window;
	GdkDevice *event_device;
	guint event_button = 0;
	guint32 event_time;

	layout = GTK_LAYOUT (widget);
	window = gtk_layout_get_bin_window (layout);

	gdk_event_get_button (button_event, &event_button);
	event_device = gdk_event_get_device (button_event);
	event_time = gdk_event_get_time (button_event);

	if (day_view->resize_event_num != -1)
		day_view->resize_event_num = -1;

	if (day_view->drag_event_num != -1)
		day_view->drag_event_num = -1;

	/* Convert the coords to the main canvas window, or return if the
	 * window is not found. */
	if (!e_day_view_convert_event_coords (
		day_view, button_event, window, &event_x, &event_y))
		return FALSE;

	/* Find out where the mouse is. */
	pos = e_day_view_convert_position_in_main_canvas (
		day_view,
		event_x, event_y,
		&day, &row,
		&event_num);

	if (pos == E_CALENDAR_VIEW_POS_OUTSIDE)
		return FALSE;

	if (pos != E_CALENDAR_VIEW_POS_NONE)
		return e_day_view_on_event_button_press (
			day_view,
			day,
			event_num,
			button_event,
			pos,
			event_x,
			event_y);

	e_day_view_stop_editing_event (day_view);

	/* Start the selection drag. */
	if (event_button == 1) {
		GdkGrabStatus grab_status;

		if (button_event->type == GDK_2BUTTON_PRESS) {
			time_t dtstart, dtend;

			day_view_get_selected_time_range ((ECalendarView *) day_view, &dtstart, &dtend);
			if (dtstart < day_view->before_click_dtend && dtend > day_view->before_click_dtstart) {
				dtstart = day_view->before_click_dtstart;
				dtend = day_view->before_click_dtend;
				day_view_set_selected_time_range ((ECalendarView *) day_view, dtstart, dtend);
			}
			e_cal_ops_new_component_editor_from_model (
				e_calendar_view_get_model (E_CALENDAR_VIEW (day_view)), NULL,
				dtstart, dtend, calendar_config_get_prefer_meeting (), FALSE);
			return TRUE;
		}

		if (!gtk_widget_has_focus (GTK_WIDGET (day_view)) && !gtk_widget_has_focus (GTK_WIDGET (day_view->main_canvas)))
			gtk_widget_grab_focus (GTK_WIDGET (day_view));

		grab_status = gdk_device_grab (
			event_device,
			window,
			GDK_OWNERSHIP_NONE,
			FALSE,
			GDK_POINTER_MOTION_MASK |
			GDK_BUTTON_RELEASE_MASK,
			NULL,
			event_time);

		if (grab_status == GDK_GRAB_SUCCESS) {
			g_warn_if_fail (day_view->grabbed_pointer == NULL);
			day_view->grabbed_pointer = g_object_ref (event_device);

			if (event_time - day_view->bc_event_time > 250)
				day_view_get_selected_time_range (
					E_CALENDAR_VIEW (day_view),
					&day_view->before_click_dtstart,
					&day_view->before_click_dtend);
			day_view->bc_event_time = event_time;
			e_day_view_start_selection (day_view, day, row);
			g_signal_emit_by_name (day_view, "selected_time_changed");
		}
	} else if (event_button == 3) {
		if (!gtk_widget_has_focus (GTK_WIDGET (day_view)))
			gtk_widget_grab_focus (GTK_WIDGET (day_view));

		if ((day < day_view->selection_start_day || day > day_view->selection_end_day)
		    || (day == day_view->selection_start_day && row < day_view->selection_start_row)
		    || (day == day_view->selection_end_day && row > day_view->selection_end_row)) {
			e_day_view_start_selection (day_view, day, row);
			e_day_view_finish_selection (day_view);
		}

		e_day_view_on_event_right_click (day_view, button_event, -1, -1);
	}

	return TRUE;
}

static gboolean
e_day_view_on_main_canvas_scroll (GtkWidget *widget,
                                  GdkEventScroll *scroll,
                                  EDayView *day_view)
{
	switch (scroll->direction) {
	case GDK_SCROLL_UP:
		e_day_view_scroll (day_view, E_DAY_VIEW_WHEEL_MOUSE_STEP_SIZE);
		return TRUE;
	case GDK_SCROLL_DOWN:
		e_day_view_scroll (day_view, -E_DAY_VIEW_WHEEL_MOUSE_STEP_SIZE);
		return TRUE;
	case GDK_SCROLL_SMOOTH:
		if (scroll->delta_y < -0.001 || scroll->delta_y > 0.001) {
			e_day_view_scroll (day_view, -E_DAY_VIEW_WHEEL_MOUSE_STEP_SIZE * scroll->delta_y);
			return TRUE;
		}
		break;
	default:
		break;
	}

	return FALSE;
}

static gboolean
e_day_view_on_top_canvas_scroll (GtkWidget *widget,
                                  GdkEventScroll *scroll,
                                  EDayView *day_view)
{
	switch (scroll->direction) {
	case GDK_SCROLL_UP:
		e_day_view_top_scroll (day_view, E_DAY_VIEW_WHEEL_MOUSE_STEP_SIZE);
		return TRUE;
	case GDK_SCROLL_DOWN:
		e_day_view_top_scroll (day_view, -E_DAY_VIEW_WHEEL_MOUSE_STEP_SIZE);
		return TRUE;
	case GDK_SCROLL_SMOOTH:
		if (scroll->delta_y < -0.001 || scroll->delta_y > 0.001) {
			e_day_view_top_scroll (day_view, -E_DAY_VIEW_WHEEL_MOUSE_STEP_SIZE * scroll->delta_y);
			return TRUE;
		}
		break;
	default:
		break;
	}

	return FALSE;
}

static gboolean
e_day_view_on_time_canvas_scroll (GtkWidget *widget,
                                  GdkEventScroll *scroll,
                                  EDayView *day_view)
{
	switch (scroll->direction) {
	case GDK_SCROLL_UP:
		e_day_view_scroll (day_view, E_DAY_VIEW_WHEEL_MOUSE_STEP_SIZE);
		return TRUE;
	case GDK_SCROLL_DOWN:
		e_day_view_scroll (day_view, -E_DAY_VIEW_WHEEL_MOUSE_STEP_SIZE);
		return TRUE;
	case GDK_SCROLL_SMOOTH:
		if (scroll->delta_y < -0.001 || scroll->delta_y > 0.001) {
			e_day_view_scroll (day_view, -E_DAY_VIEW_WHEEL_MOUSE_STEP_SIZE * scroll->delta_y);
			return TRUE;
		}
		break;
	default:
		break;
	}

	return FALSE;
}

static gboolean
e_day_view_on_long_event_button_press (EDayView *day_view,
                                       gint event_num,
                                       GdkEvent *button_event,
                                       ECalendarViewPosition pos,
                                       gint event_x,
                                       gint event_y)
{
	guint event_button = 0;

	gdk_event_get_button (button_event, &event_button);

	if (event_button == 1) {
		if (button_event->type == GDK_BUTTON_PRESS) {
			e_day_view_on_long_event_click (
				day_view, event_num,
				button_event, pos,
				event_x, event_y);
			return TRUE;
		} else if (button_event->type == GDK_2BUTTON_PRESS) {
			e_day_view_on_event_double_click (
				day_view, -1,
				event_num);
			g_signal_stop_emission_by_name (day_view->top_canvas, "button_press_event");
			return TRUE;
		}
	} else if (event_button == 3) {
		EDayViewEvent *e;

		if (!is_array_index_in_bounds (day_view->long_events, event_num))
			return TRUE;

		e = &g_array_index (day_view->long_events, EDayViewEvent, event_num);

		e_day_view_set_selected_time_range_in_top_visible (day_view, e->start, e->end);

		e_day_view_on_event_right_click (
			day_view, button_event,
			E_DAY_VIEW_LONG_EVENT,
			event_num);

		return TRUE;
	}
	return FALSE;
}

static gboolean
e_day_view_on_event_button_press (EDayView *day_view,
                                  gint day,
                                  gint event_num,
                                  GdkEvent *button_event,
                                  ECalendarViewPosition pos,
                                  gint event_x,
                                  gint event_y)
{
	guint event_button = 0;

	gdk_event_get_button (button_event, &event_button);

	if (event_button == 1) {
		if (button_event->type == GDK_BUTTON_PRESS) {
			e_day_view_on_event_click (
				day_view, day, event_num,
				button_event, pos,
				event_x, event_y);
			return TRUE;
		} else if (button_event->type == GDK_2BUTTON_PRESS) {
			e_day_view_on_event_double_click (
				day_view, day,
				event_num);

			g_signal_stop_emission_by_name (day_view->main_canvas, "button_press_event");
			return TRUE;
		}
	} else if (event_button == 3) {
		EDayViewEvent *e;

		if (!is_array_index_in_bounds (day_view->events[day], event_num))
			return TRUE;

		e = &g_array_index (day_view->events[day], EDayViewEvent, event_num);

		e_day_view_set_selected_time_range_visible (day_view, e->start, e->end);

		e_day_view_on_event_right_click (
			day_view, button_event, day, event_num);

		return TRUE;
	}
	return FALSE;
}

static void
e_day_view_on_long_event_click (EDayView *day_view,
                                gint event_num,
                                GdkEvent *button_event,
                                ECalendarViewPosition pos,
                                gint event_x,
                                gint event_y)
{
	EDayViewEvent *event;
	GtkLayout *layout;
	GdkWindow *window;
	gint start_day, end_day, day;
	gint item_x, item_y, item_w, item_h;

	if (!is_array_index_in_bounds (day_view->long_events, event_num))
		return;

	event = &g_array_index (day_view->long_events, EDayViewEvent,
				event_num);

	if (!is_comp_data_valid (event))
		return;

	/* Ignore clicks on the EText while editing. */
	if (pos == E_CALENDAR_VIEW_POS_EVENT
	    && E_TEXT (event->canvas_item)->editing) {
		GNOME_CANVAS_ITEM_GET_CLASS (event->canvas_item)->event (
			event->canvas_item, button_event);
		return;
	}

	e_day_view_set_popup_event (day_view, E_DAY_VIEW_LONG_EVENT, event_num);

	if ((e_cal_util_component_is_instance (event->comp_data->icalcomp) ||
	     !e_cal_util_component_has_recurrences (event->comp_data->icalcomp))
	    && (pos == E_CALENDAR_VIEW_POS_LEFT_EDGE
		|| pos == E_CALENDAR_VIEW_POS_RIGHT_EDGE)) {
		GdkGrabStatus grab_status;
		GdkDevice *event_device;
		guint32 event_time;

		if (!e_calendar_view_get_allow_event_dnd (E_CALENDAR_VIEW (day_view)))
			return;

		if (!e_day_view_find_long_event_days (
			event,
			e_day_view_get_days_shown (day_view),
			day_view->day_starts,
			&start_day, &end_day))
			return;

		/* Grab the keyboard focus, so the event being edited is saved
		 * and we can use the Escape key to abort the resize. */
		if (!gtk_widget_has_focus (GTK_WIDGET (day_view)))
			gtk_widget_grab_focus (GTK_WIDGET (day_view));

		layout = GTK_LAYOUT (day_view->top_canvas);
		window = gtk_layout_get_bin_window (layout);

		event_device = gdk_event_get_device (button_event);
		event_time = gdk_event_get_time (button_event);

		grab_status = gdk_device_grab (
			event_device,
			window,
			GDK_OWNERSHIP_NONE,
			FALSE,
			GDK_POINTER_MOTION_MASK |
			GDK_BUTTON_RELEASE_MASK,
			NULL,
			event_time);

		if (grab_status == GDK_GRAB_SUCCESS) {
			g_warn_if_fail (day_view->grabbed_pointer == NULL);
			day_view->grabbed_pointer = g_object_ref (event_device);

			day_view->resize_event_day = E_DAY_VIEW_LONG_EVENT;
			day_view->resize_event_num = event_num;
			day_view->resize_drag_pos = pos;
			day_view->resize_start_row = start_day;
			day_view->resize_end_row = end_day;

			/* Raise the event's item, above the rect as well. */
			gnome_canvas_item_raise_to_top (event->canvas_item);
		}
	} else if (e_day_view_get_long_event_position (day_view, event_num,
						       &start_day, &end_day,
						       &item_x, &item_y,
						       &item_w, &item_h)) {
		/* Remember the item clicked and the mouse position,
		 * so we can start a drag if the mouse moves. */
		day_view->pressed_event_day = E_DAY_VIEW_LONG_EVENT;
		day_view->pressed_event_num = event_num;

		day_view->drag_event_x = event_x;
		day_view->drag_event_y = event_y;

		pos = e_day_view_convert_position_in_top_canvas (
			day_view,
			event_x, event_y,
			&day, NULL);

		if (pos != E_CALENDAR_VIEW_POS_NONE && pos != E_CALENDAR_VIEW_POS_OUTSIDE)
			day_view->drag_event_offset = day - start_day;
	}
}

static void
e_day_view_on_event_click (EDayView *day_view,
                           gint day,
                           gint event_num,
                           GdkEvent *button_event,
                           ECalendarViewPosition pos,
                           gint event_x,
                           gint event_y)
{
	EDayViewEvent *event;
	ECalendarView *cal_view;
	GtkLayout *layout;
	GdkWindow *window;
	gint time_divisions;
	gint tmp_day, row, start_row;

	cal_view = E_CALENDAR_VIEW (day_view);
	time_divisions = e_calendar_view_get_time_divisions (cal_view);

	if (!is_array_index_in_bounds (day_view->events[day], event_num))
		return;

	event = &g_array_index (day_view->events[day], EDayViewEvent,
				event_num);

	if (!is_comp_data_valid (event))
		return;

	/* Ignore clicks on the EText while editing. */
	if (pos == E_CALENDAR_VIEW_POS_EVENT
	    && E_TEXT (event->canvas_item)->editing) {
		GNOME_CANVAS_ITEM_GET_CLASS (event->canvas_item)->event (
			event->canvas_item, button_event);
		return;
	}

	e_day_view_set_popup_event (day_view, day, event_num);

	if ((e_cal_util_component_is_instance (event->comp_data->icalcomp) ||
	     !e_cal_util_component_has_recurrences (event->comp_data->icalcomp))
	    && (pos == E_CALENDAR_VIEW_POS_TOP_EDGE
		|| pos == E_CALENDAR_VIEW_POS_BOTTOM_EDGE)) {
		GdkGrabStatus grab_status;
		GdkDevice *event_device;
		guint32 event_time;

		if (!e_calendar_view_get_allow_event_dnd (E_CALENDAR_VIEW (day_view)))
			return;

		if (event && (!event->is_editable || e_client_is_readonly (E_CLIENT (event->comp_data->client)))) {
			return;
		}

		/* Grab the keyboard focus, so the event being edited is saved
		 * and we can use the Escape key to abort the resize. */
		if (!gtk_widget_has_focus (GTK_WIDGET (day_view)))
			gtk_widget_grab_focus (GTK_WIDGET (day_view));

		layout = GTK_LAYOUT (day_view->main_canvas);
		window = gtk_layout_get_bin_window (layout);

		event_device = gdk_event_get_device (button_event);
		event_time = gdk_event_get_time (button_event);

		grab_status = gdk_device_grab (
			event_device,
			window,
			GDK_OWNERSHIP_NONE,
			FALSE,
			GDK_POINTER_MOTION_MASK |
			GDK_BUTTON_RELEASE_MASK,
			NULL,
			event_time);

		if (grab_status == GDK_GRAB_SUCCESS) {
			g_warn_if_fail (day_view->grabbed_pointer == NULL);
			day_view->grabbed_pointer = g_object_ref (event_device);

			day_view->resize_event_day = day;
			day_view->resize_event_num = event_num;
			day_view->resize_drag_pos = pos;
			day_view->resize_start_row = event->start_minute / time_divisions;
			day_view->resize_end_row = (event->end_minute - 1) / time_divisions;
			if (day_view->resize_end_row < day_view->resize_start_row)
				day_view->resize_end_row = day_view->resize_start_row;

			day_view->resize_bars_event_day = day;
			day_view->resize_bars_event_num = event_num;

			e_day_view_reshape_main_canvas_resize_bars (day_view);

			/* Raise the event's item, above the rect as well. */
			gnome_canvas_item_raise_to_top (event->canvas_item);
		}

	} else {
		/* Remember the item clicked and the mouse position,
		 * so we can start a drag if the mouse moves. */
		day_view->pressed_event_day = day;
		day_view->pressed_event_num = event_num;

		day_view->drag_event_x = event_x;
		day_view->drag_event_y = event_y;

		pos = e_day_view_convert_position_in_main_canvas (
			day_view,
			event_x, event_y,
			&tmp_day, &row,
			NULL);

		if (pos != E_CALENDAR_VIEW_POS_NONE && pos != E_CALENDAR_VIEW_POS_OUTSIDE) {
			start_row = event->start_minute / time_divisions;
			day_view->drag_event_offset = row - start_row;
		}
	}
}

static void
e_day_view_on_event_double_click (EDayView *day_view,
                                  gint day,
                                  gint event_num)
{
	EDayViewEvent *event;

	if (day == -1) {
		if (!is_array_index_in_bounds (day_view->long_events, event_num))
			return;

		event = &g_array_index (day_view->long_events, EDayViewEvent,
				event_num);
	} else {
		if (!is_array_index_in_bounds (day_view->events[day], event_num))
			return;

		event = &g_array_index (day_view->events[day], EDayViewEvent,
				event_num);
	}

	if (!is_comp_data_valid (event))
		return;

	e_calendar_view_edit_appointment ((ECalendarView *) day_view, event->comp_data->client, event->comp_data->icalcomp, EDIT_EVENT_AUTODETECT);
}

static void
e_day_view_show_popup_menu (EDayView *day_view,
                            GdkEvent *button_event,
                            gint day,
                            gint event_num)
{
	e_day_view_set_popup_event (day_view, day, event_num);

	e_calendar_view_popup_event (E_CALENDAR_VIEW (day_view), button_event);
}

/* Restarts a query for the day view */
static void
e_day_view_update_query (EDayView *day_view)
{
	gint rows, r;

	if (!E_CALENDAR_VIEW (day_view)->in_focus) {
		e_day_view_free_events (day_view);
		day_view->requires_update = TRUE;
		return;
	}

	if (!day_view->priv->marcus_bains_update_id)
		day_view_refresh_marcus_bains_line (day_view);

	day_view->requires_update = FALSE;

	e_day_view_stop_editing_event (day_view);

	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->top_dates_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);
	e_day_view_free_events (day_view);
	e_day_view_queue_layout (day_view);

	rows = e_table_model_row_count (E_TABLE_MODEL (e_calendar_view_get_model (E_CALENDAR_VIEW (day_view))));
	for (r = 0; r < rows; r++) {
		ECalModelComponent *comp_data;

		comp_data = e_cal_model_get_component_at (e_calendar_view_get_model (E_CALENDAR_VIEW (day_view)), r);
		g_return_if_fail (comp_data != NULL);
		process_component (day_view, comp_data);
	}
}

static void
e_day_view_on_event_right_click (EDayView *day_view,
                                 GdkEvent *button_event,
                                 gint day,
                                 gint event_num)
{
	e_day_view_show_popup_menu (day_view, button_event, day, event_num);
}

static gboolean
e_day_view_on_top_canvas_button_release (GtkWidget *widget,
                                         GdkEvent *button_event,
                                         EDayView *day_view)
{
	GdkDevice *event_device;
	guint32 event_time;

	event_device = gdk_event_get_device (button_event);
	event_time = gdk_event_get_time (button_event);

	if (day_view->grabbed_pointer == event_device) {
		gdk_device_ungrab (day_view->grabbed_pointer, event_time);
		g_object_unref (day_view->grabbed_pointer);
		day_view->grabbed_pointer = NULL;
	}

	if (day_view->selection_is_being_dragged) {
		e_day_view_finish_selection (day_view);
	} else if (day_view->resize_drag_pos != E_CALENDAR_VIEW_POS_NONE) {
		e_day_view_finish_long_event_resize (day_view);
	} else if (day_view->pressed_event_day != -1 &&
		   e_calendar_view_get_allow_direct_summary_edit (E_CALENDAR_VIEW (day_view))) {
		e_day_view_start_editing_event (
			day_view,
			day_view->pressed_event_day,
			day_view->pressed_event_num,
			NULL);
	}

	day_view->pressed_event_day = -1;

	return FALSE;
}

static gboolean
e_day_view_on_main_canvas_button_release (GtkWidget *widget,
                                          GdkEvent *button_event,
                                          EDayView *day_view)
{
	GdkDevice *event_device;
	guint32 event_time;

	event_device = gdk_event_get_device (button_event);
	event_time = gdk_event_get_time (button_event);

	if (day_view->grabbed_pointer == event_device) {
		gdk_device_ungrab (day_view->grabbed_pointer, event_time);
		g_object_unref (day_view->grabbed_pointer);
		day_view->grabbed_pointer = NULL;
	}

	if (day_view->selection_is_being_dragged) {
		e_day_view_finish_selection (day_view);
		e_day_view_stop_auto_scroll (day_view);
	} else if (day_view->resize_drag_pos != E_CALENDAR_VIEW_POS_NONE) {
		e_day_view_finish_resize (day_view);
		e_day_view_stop_auto_scroll (day_view);
	} else if (day_view->pressed_event_day != -1 &&
		   e_calendar_view_get_allow_direct_summary_edit (E_CALENDAR_VIEW (day_view))) {
		e_day_view_start_editing_event (
			day_view,
			day_view->pressed_event_day,
			day_view->pressed_event_num,
			NULL);
	}

	day_view->pressed_event_day = -1;

	return FALSE;
}

void
e_day_view_update_calendar_selection_time (EDayView *day_view)
{
	time_t start, end;

	day_view_get_selected_time_range ((ECalendarView *) day_view, &start, &end);
}

static gboolean
e_day_view_on_top_canvas_motion (GtkWidget *widget,
                                 GdkEventMotion *mevent,
                                 EDayView *day_view)
{
	EDayViewEvent *event = NULL;
	ECalendarViewPosition pos;
	gint event_x, event_y, canvas_x, canvas_y;
	gint day, event_num;
	GdkCursor *cursor;
	GdkWindow *window;

	window = gtk_layout_get_bin_window (GTK_LAYOUT (widget));

	/* Convert the coords to the main canvas window, or return if the
	 * window is not found. */
	if (!e_day_view_convert_event_coords (
		day_view, (GdkEvent *) mevent, window, &event_x, &event_y))
		return FALSE;

	canvas_x = event_x;
	canvas_y = event_y;

	pos = e_day_view_convert_position_in_top_canvas (
		day_view,
		canvas_x, canvas_y,
		&day, &event_num);
	if (event_num != -1) {
		if (!is_array_index_in_bounds (day_view->long_events, event_num))
			return FALSE;

		event = &g_array_index (day_view->long_events, EDayViewEvent,
					event_num);
	}

	if (day_view->selection_is_being_dragged) {
		e_day_view_update_selection (day_view, day, -1);
		return TRUE;
	} else if (day_view->resize_drag_pos != E_CALENDAR_VIEW_POS_NONE) {
		if (pos != E_CALENDAR_VIEW_POS_OUTSIDE) {
			e_day_view_update_long_event_resize (day_view, day);
			return TRUE;
		}
	} else if (day_view->pressed_event_day == E_DAY_VIEW_LONG_EVENT) {
		GtkTargetList *target_list;

		if (!is_array_index_in_bounds (day_view->long_events, day_view->pressed_event_num))
			return FALSE;

		event = &g_array_index (day_view->long_events, EDayViewEvent,
					day_view->pressed_event_num);

		if (!is_comp_data_valid (event))
			return FALSE;

		if (!e_cal_util_component_has_recurrences (event->comp_data->icalcomp) &&
		    e_calendar_view_get_allow_event_dnd (E_CALENDAR_VIEW (day_view)) &&
		    gtk_drag_check_threshold (widget, day_view->drag_event_x, day_view->drag_event_y, canvas_x, canvas_y)) {
			day_view->drag_event_day = day_view->pressed_event_day;
			day_view->drag_event_num = day_view->pressed_event_num;
			day_view->priv->drag_event_is_recurring =
				e_cal_util_component_is_instance (event->comp_data->icalcomp) ||
				e_cal_util_component_has_recurrences (event->comp_data->icalcomp);
			day_view->pressed_event_day = -1;

			/* Hide the horizontal bars. */
			if (day_view->resize_bars_event_day != -1) {
				day_view->resize_bars_event_day = -1;
				day_view->resize_bars_event_num = -1;
			}

			target_list = gtk_target_list_new (
				target_table, G_N_ELEMENTS (target_table));
			e_target_list_add_calendar_targets (target_list, 0);
			g_clear_object (&day_view->priv->drag_context);
			day_view->priv->drag_context = gtk_drag_begin (
				widget, target_list,
				(day_view->priv->drag_event_is_recurring ? 0 : GDK_ACTION_COPY) |
				GDK_ACTION_MOVE,
				1, (GdkEvent *) mevent);
			gtk_target_list_unref (target_list);

			if (day_view->priv->drag_context)
				g_object_ref (day_view->priv->drag_context);
		}
	} else {
		cursor = day_view->normal_cursor;

		/* Recurring events can't be resized. */
		if (event && is_comp_data_valid (event) &&
		    !e_cal_util_component_has_recurrences (event->comp_data->icalcomp) &&
		    e_calendar_view_get_allow_event_dnd (E_CALENDAR_VIEW (day_view))) {
			switch (pos) {
			case E_CALENDAR_VIEW_POS_LEFT_EDGE:
			case E_CALENDAR_VIEW_POS_RIGHT_EDGE:
				cursor = day_view->resize_width_cursor;
				break;
			default:
				break;
			}
		}

		/* Only set the cursor if it is different to last one set. */
		if (day_view->last_cursor_set_in_top_canvas != cursor) {
			day_view->last_cursor_set_in_top_canvas = cursor;

			gdk_window_set_cursor (gtk_widget_get_window (widget), cursor);
		}

		if (event && E_IS_TEXT (event->canvas_item) && E_TEXT (event->canvas_item)->editing) {
			GNOME_CANVAS_ITEM_GET_CLASS (event->canvas_item)->event (event->canvas_item, (GdkEvent *) mevent);
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
	ECalendarViewPosition pos;
	gint event_x, event_y, canvas_x, canvas_y;
	gint row, day, event_num;
	GdkWindow *window;
	GdkCursor *cursor;

	window = gtk_layout_get_bin_window (GTK_LAYOUT (widget));

	/* Convert the coords to the main canvas window, or return if the
	 * window is not found. */
	if (!e_day_view_convert_event_coords (
		day_view, (GdkEvent *) mevent, window, &event_x, &event_y))
		return FALSE;

	canvas_x = event_x;
	canvas_y = event_y;

	pos = e_day_view_convert_position_in_main_canvas (
		day_view,
		canvas_x, canvas_y,
		&day, &row,
		&event_num);
	if (event_num != -1) {
		if (!is_array_index_in_bounds (day_view->events[day], event_num))
			return FALSE;

		event = &g_array_index (day_view->events[day], EDayViewEvent,
					event_num);
	}

	if (day_view->selection_is_being_dragged) {
		if (pos != E_CALENDAR_VIEW_POS_OUTSIDE) {
			e_day_view_update_selection (day_view, day, row);
			e_day_view_check_auto_scroll (
				day_view,
				event_x, event_y);
			return TRUE;
		}
	} else if (day_view->resize_drag_pos != E_CALENDAR_VIEW_POS_NONE) {
		if (pos != E_CALENDAR_VIEW_POS_OUTSIDE) {
			e_day_view_update_resize (day_view, row);
			e_day_view_check_auto_scroll (
				day_view,
				event_x, event_y);
			return TRUE;
		}
	} else if (day_view->pressed_event_day != -1
		   && day_view->pressed_event_day != E_DAY_VIEW_LONG_EVENT) {
		GtkTargetList *target_list;

		if (e_calendar_view_get_allow_event_dnd (E_CALENDAR_VIEW (day_view)) &&
		    gtk_drag_check_threshold (widget, day_view->drag_event_x, day_view->drag_event_y, canvas_x, canvas_y)) {
			day_view->drag_event_day = day_view->pressed_event_day;
			day_view->drag_event_num = day_view->pressed_event_num;
			day_view->priv->drag_event_is_recurring =
				event && is_comp_data_valid (event) &&
				((e_cal_util_component_is_instance (event->comp_data->icalcomp) ||
				e_cal_util_component_has_recurrences (event->comp_data->icalcomp)));
			day_view->pressed_event_day = -1;

			/* Hide the horizontal bars. */
			if (day_view->resize_bars_event_day != -1) {
				day_view->resize_bars_event_day = -1;
				day_view->resize_bars_event_num = -1;
			}

			target_list = gtk_target_list_new (
				target_table, G_N_ELEMENTS (target_table));
			e_target_list_add_calendar_targets (target_list, 0);
			g_clear_object (&day_view->priv->drag_context);
			day_view->priv->drag_context = gtk_drag_begin (
				widget, target_list,
				(day_view->priv->drag_event_is_recurring ? 0 : GDK_ACTION_COPY) |
				GDK_ACTION_MOVE,
				1, (GdkEvent *) mevent);
			gtk_target_list_unref (target_list);

			if (day_view->priv->drag_context)
				g_object_ref (day_view->priv->drag_context);
		}
	} else {
		cursor = day_view->normal_cursor;

		/* Check if the event is editable and client is not readonly while changing the cursor */
		if (event && event->is_editable && is_comp_data_valid (event) &&
		    e_calendar_view_get_allow_event_dnd (E_CALENDAR_VIEW (day_view)) &&
		    !e_client_is_readonly (E_CLIENT (event->comp_data->client))) {

			switch (pos) {
			case E_CALENDAR_VIEW_POS_LEFT_EDGE:
				cursor = day_view->move_cursor;
				break;
			case E_CALENDAR_VIEW_POS_TOP_EDGE:
			case E_CALENDAR_VIEW_POS_BOTTOM_EDGE:
				cursor = day_view->resize_height_cursor;
				break;
			default:
				break;
			}
		}

		/* Only set the cursor if it is different to last one set. */
		if (day_view->last_cursor_set_in_main_canvas != cursor) {
			day_view->last_cursor_set_in_main_canvas = cursor;

			gdk_window_set_cursor (gtk_widget_get_window (widget), cursor);
		}

		if (event && E_IS_TEXT (event->canvas_item) && E_TEXT (event->canvas_item)->editing) {
			GNOME_CANVAS_ITEM_GET_CLASS (event->canvas_item)->event (event->canvas_item, (GdkEvent *) mevent);
		}
	}

	return FALSE;
}

/* This sets the selection to a single cell. If day is -1 then the current
 * start day is reused. If row is -1 then the selection is in the top canvas.
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
 * unchanged. */
void
e_day_view_update_selection (EDayView *day_view,
                             gint day,
                             gint row)
{
	gboolean need_redraw = FALSE;

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
	gint event_num;
	gboolean need_reshape = FALSE;

	event_num = day_view->resize_event_num;

	if (day_view->resize_drag_pos == E_CALENDAR_VIEW_POS_LEFT_EDGE) {
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
		gtk_widget_queue_draw (day_view->top_canvas);
	}
}

static void
e_day_view_update_resize (EDayView *day_view,
                          gint row)
{
	/* Same thing again? */
	EDayViewEvent *event;
	gint day, event_num;
	gboolean need_reshape = FALSE;

	if (day_view->resize_event_num == -1)
		return;

	day = day_view->resize_event_day;
	event_num = day_view->resize_event_num;

	if (!is_array_index_in_bounds (day_view->events[day], event_num))
		return;

	event = &g_array_index (day_view->events[day], EDayViewEvent,
				event_num);

	if (event && (!event->is_editable || !is_comp_data_valid (event) || e_client_is_readonly (E_CLIENT (event->comp_data->client)))) {
		return;
	}

	if (day_view->resize_drag_pos == E_CALENDAR_VIEW_POS_TOP_EDGE) {
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
		e_day_view_reshape_main_canvas_resize_bars (day_view);
		gtk_widget_queue_draw (day_view->main_canvas);
	}
}

/* This converts the resize start or end row back to a time and updates the
 * event. */
static void
e_day_view_finish_long_event_resize (EDayView *day_view)
{
	EDayViewEvent *event;
	gint event_num;
	ECalComponent *comp;
	ECalComponentDateTime *date = NULL;
	time_t dt;
	ECalModel *model;
	ECalClient *client;
	ESourceRegistry *registry;
	ECalObjModType mod = E_CAL_OBJ_MOD_ALL;
	ICalTimezone *zone;
	gint is_date;

	model = e_calendar_view_get_model (E_CALENDAR_VIEW (day_view));
	registry = e_cal_model_get_registry (model);

	event_num = day_view->resize_event_num;

	if (!is_array_index_in_bounds (day_view->long_events, event_num))
		return;

	event = &g_array_index (day_view->long_events, EDayViewEvent,
				event_num);

	if (!is_comp_data_valid (event))
		return;

	client = event->comp_data->client;

	/* We use a temporary copy of the comp since we don't want to
	 * change the original comp here. Otherwise we would not detect that
	 * the event's time had changed in the "update_event" callback. */
	comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (event->comp_data->icalcomp));
	if (!comp)
		return;

	if (e_cal_component_has_attendees (comp) &&
	    !itip_organizer_is_user (registry, comp, client)) {
		g_object_unref (comp);
		e_day_view_abort_resize (day_view);
		return;
	}

	zone = e_calendar_view_get_timezone (E_CALENDAR_VIEW (day_view));

	if (day_view->resize_drag_pos == E_CALENDAR_VIEW_POS_LEFT_EDGE) {
		ECalComponentDateTime *ecdt;

		ecdt = e_cal_component_get_dtstart (comp);
		is_date = ecdt && e_cal_component_datetime_get_value (ecdt) &&
			  i_cal_time_is_date (e_cal_component_datetime_get_value (ecdt));

		dt = day_view->day_starts[day_view->resize_start_row];
		date = e_cal_component_datetime_new_take (
			i_cal_time_new_from_timet_with_zone (dt, is_date, zone),
			(zone && !is_date) ? g_strdup (i_cal_timezone_get_tzid (zone)) : NULL);
		cal_comp_set_dtstart_with_oldzone (client, comp, date);

		e_cal_component_datetime_free (ecdt);

		/* do not reuse it later */
		e_cal_component_datetime_set_tzid (date, NULL);
	} else {
		ECalComponentDateTime *ecdt;

		ecdt = e_cal_component_get_dtend (comp);
		is_date = ecdt && e_cal_component_datetime_get_value (ecdt) &&
			  i_cal_time_is_date (e_cal_component_datetime_get_value (ecdt));

		dt = day_view->day_starts[day_view->resize_end_row + 1];
		date = e_cal_component_datetime_new_take (
			i_cal_time_new_from_timet_with_zone (dt, is_date, zone),
			(zone && !is_date) ? g_strdup (i_cal_timezone_get_tzid (zone)) : NULL);
		cal_comp_set_dtend_with_oldzone (client, comp, date);

		e_cal_component_datetime_free (ecdt);

		/* do not reuse it later */
		e_cal_component_datetime_set_tzid (date, NULL);
	}

	e_cal_component_commit_sequence (comp);
	if (e_cal_component_has_recurrences (comp)) {
		if (!e_cal_dialogs_recur_component (client, comp, &mod, NULL, FALSE)) {
			gtk_widget_queue_draw (day_view->top_canvas);
			goto out;
		}

		if (mod == E_CAL_OBJ_MOD_THIS) {
			/* set the correct DTSTART/DTEND on the individual recurrence */
			if (day_view->resize_drag_pos == E_CALENDAR_VIEW_POS_TOP_EDGE) {
				e_cal_component_datetime_take_value (date,
					i_cal_time_new_from_timet_with_zone (event->comp_data->instance_end, FALSE, zone));
				cal_comp_set_dtend_with_oldzone (client, comp, date);
			} else {
				e_cal_component_datetime_take_value (date,
					i_cal_time_new_from_timet_with_zone (event->comp_data->instance_start, FALSE, zone));
				cal_comp_set_dtstart_with_oldzone (client, comp, date);
			}

			e_cal_component_set_rdates (comp, NULL);
			e_cal_component_set_rrules (comp, NULL);
			e_cal_component_set_exdates (comp, NULL);
			e_cal_component_set_exrules (comp, NULL);
		}
	} else if (e_cal_component_is_instance (comp)) {
		mod = E_CAL_OBJ_MOD_THIS;
	}

	e_cal_component_commit_sequence (comp);

	e_cal_ops_modify_component (e_cal_model_get_data_model (model), client, e_cal_component_get_icalcomponent (comp),
		mod, E_CAL_OPS_SEND_FLAG_ASK | E_CAL_OPS_SEND_FLAG_IS_NEW_COMPONENT);

 out:
	day_view->resize_drag_pos = E_CALENDAR_VIEW_POS_NONE;

	e_cal_component_datetime_free (date);
	g_object_unref (comp);
}

/* This converts the resize start or end row back to a time and updates the
 * event. */
static void
e_day_view_finish_resize (EDayView *day_view)
{
	EDayViewEvent *event;
	gint day, event_num;
	ECalComponent *comp;
	ECalComponentDateTime *date = NULL;
	ICalTimezone *zone;
	time_t dt;
	ECalModel *model;
	ECalClient *client;
	ESourceRegistry *registry;
	ECalObjModType mod = E_CAL_OBJ_MOD_ALL;
	GtkWindow *toplevel;
	GtkResponseType send = GTK_RESPONSE_NO;
	gboolean only_new_attendees = FALSE;
	gboolean strip_alarms = TRUE;

	model = e_calendar_view_get_model (E_CALENDAR_VIEW (day_view));
	registry = e_cal_model_get_registry (model);

	if (day_view->resize_event_num == -1)
		return;

	day = day_view->resize_event_day;
	event_num = day_view->resize_event_num;

	if (!is_array_index_in_bounds (day_view->events[day], event_num))
		return;

	event = &g_array_index (day_view->events[day], EDayViewEvent,
				event_num);

	if (!is_comp_data_valid (event))
		return;

	client = event->comp_data->client;

	/* We use a temporary shallow copy of the ico since we don't want to
	 * change the original ico here. Otherwise we would not detect that
	 * the event's time had changed in the "update_event" callback. */
	comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (event->comp_data->icalcomp));
	if (!comp)
		return;

	if (e_cal_component_has_attendees (comp) &&
	    !itip_organizer_is_user (registry, comp, client)) {
		g_object_unref (comp);
		e_day_view_abort_resize (day_view);
		return;
	}

	toplevel = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (day_view)));

	if (itip_has_any_attendees (comp) &&
	    (itip_organizer_is_user (registry, comp, client) ||
	     itip_sentby_is_user (registry, comp, client)))
		send = e_cal_dialogs_send_dragged_or_resized_component (
				toplevel, client, comp, &strip_alarms, &only_new_attendees);

	if (send == GTK_RESPONSE_CANCEL) {
		e_day_view_abort_resize (day_view);
		goto out;
	}

	zone = e_calendar_view_get_timezone (E_CALENDAR_VIEW (day_view));

	if (day_view->resize_drag_pos == E_CALENDAR_VIEW_POS_TOP_EDGE) {
		dt = e_day_view_convert_grid_position_to_time (day_view, day, day_view->resize_start_row);
		date = e_cal_component_datetime_new_take (
			i_cal_time_new_from_timet_with_zone (dt, FALSE, zone),
			zone ? g_strdup (i_cal_timezone_get_tzid (zone)) : NULL);
		cal_comp_set_dtstart_with_oldzone (client, comp, date);
	} else {
		dt = e_day_view_convert_grid_position_to_time (day_view, day, day_view->resize_end_row + 1);
		date = e_cal_component_datetime_new_take (
			i_cal_time_new_from_timet_with_zone (dt, FALSE, zone),
			zone ? g_strdup (i_cal_timezone_get_tzid (zone)) : NULL);
		cal_comp_set_dtend_with_oldzone (client, comp, date);
	}

	e_cal_component_commit_sequence (comp);

	g_clear_pointer (&day_view->last_edited_comp_string, g_free);

	day_view->last_edited_comp_string = e_cal_component_get_as_string (comp);

	/* Hide the horizontal bars. */
	day_view->resize_bars_event_day = -1;
	day_view->resize_bars_event_num = -1;

	day_view->resize_drag_pos = E_CALENDAR_VIEW_POS_NONE;

	if (e_cal_component_has_recurrences (comp)) {
		if (!e_cal_dialogs_recur_component (client, comp, &mod, NULL, FALSE)) {
			gtk_widget_queue_draw (day_view->main_canvas);
			goto out;
		}

		if (mod == E_CAL_OBJ_MOD_THIS) {
			/* set the correct DTSTART/DTEND on the individual recurrence */
			if (day_view->resize_drag_pos == E_CALENDAR_VIEW_POS_TOP_EDGE) {
				e_cal_component_datetime_take_value (date,
					i_cal_time_new_from_timet_with_zone (event->comp_data->instance_end, FALSE, zone));
				cal_comp_set_dtend_with_oldzone (client, comp, date);
			} else {
				e_cal_component_datetime_take_value (date,
					i_cal_time_new_from_timet_with_zone (event->comp_data->instance_start, FALSE, zone));
				cal_comp_set_dtstart_with_oldzone (client, comp, date);
			}

			e_cal_component_set_rdates (comp, NULL);
			e_cal_component_set_rrules (comp, NULL);
			e_cal_component_set_exdates (comp, NULL);
			e_cal_component_set_exrules (comp, NULL);
		}
	} else if (e_cal_component_is_instance (comp)) {
		mod = E_CAL_OBJ_MOD_THIS;
	}

	e_cal_component_commit_sequence (comp);

	e_cal_ops_modify_component (e_cal_model_get_data_model (model), client, e_cal_component_get_icalcomponent (comp), mod,
		(send == GTK_RESPONSE_YES ? E_CAL_OPS_SEND_FLAG_SEND : E_CAL_OPS_SEND_FLAG_DONT_SEND) |
		(strip_alarms ? E_CAL_OPS_SEND_FLAG_STRIP_ALARMS : 0) |
		(only_new_attendees ? E_CAL_OPS_SEND_FLAG_ONLY_NEW_ATTENDEES : 0));

 out:
	e_cal_component_datetime_free (date);
	g_object_unref (comp);
}

static void
e_day_view_abort_resize (EDayView *day_view)
{
	GdkWindow *window;
	gint day, event_num;

	if (day_view->resize_drag_pos == E_CALENDAR_VIEW_POS_NONE)
		return;

	day_view->resize_drag_pos = E_CALENDAR_VIEW_POS_NONE;

	day = day_view->resize_event_day;
	event_num = day_view->resize_event_num;

	if (day == E_DAY_VIEW_LONG_EVENT) {
		e_day_view_reshape_long_event (day_view, event_num);
		gtk_widget_queue_draw (day_view->top_canvas);

		day_view->last_cursor_set_in_top_canvas = day_view->normal_cursor;
		window = gtk_widget_get_window (day_view->top_canvas);
		gdk_window_set_cursor (window, day_view->normal_cursor);
	} else {
		e_day_view_reshape_day_event (day_view, day, event_num);
		e_day_view_reshape_main_canvas_resize_bars (day_view);
		gtk_widget_queue_draw (day_view->main_canvas);

		day_view->last_cursor_set_in_main_canvas = day_view->normal_cursor;
		window = gtk_widget_get_window (day_view->main_canvas);
		gdk_window_set_cursor (window, day_view->normal_cursor);
	}
}

static void
e_day_view_free_events (EDayView *day_view)
{
	gint day;
	gboolean did_editing = day_view->editing_event_day != -1;

	/* Reset all our indices. */
	day_view->editing_event_day = -1;
	day_view->popup_event_day = -1;
	day_view->resize_bars_event_day = -1;
	day_view->resize_event_day = -1;
	day_view->pressed_event_day = -1;
	day_view->drag_event_day = -1;
	day_view->priv->drag_event_is_recurring = FALSE;
	day_view->editing_event_num = -1;
	day_view->popup_event_num = -1;

	g_clear_object (&day_view->priv->drag_context);

	e_day_view_free_event_array (day_view, day_view->long_events);

	for (day = 0; day < E_DAY_VIEW_MAX_DAYS; day++)
		e_day_view_free_event_array (day_view, day_view->events[day]);

	if (did_editing)
		g_object_notify (G_OBJECT (day_view), "is-editing");
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
			g_object_run_dispose (G_OBJECT (event->canvas_item));

		if (is_comp_data_valid (event))
			g_object_unref (event->comp_data);
	}

	g_array_set_size (array, 0);
}

/* This adds one event to the view, adding it to the appropriate array. */
static void
e_day_view_add_event (ESourceRegistry *registry,
		      ECalClient *client,
                      ECalComponent *comp,
                      time_t start,
                      time_t end,
                      gpointer data)

{
	EDayViewEvent event;
	gint day, offset;
	gint days_shown;
	ICalTime *start_tt, *end_tt;
	ICalTimezone *zone;
	AddEventData *add_event_data;

	add_event_data = data;

	/*if (end < start || start >= add_event_data->day_view->upper || end < add_event_data->day_view->lower) {
		g_print ("%s: day_view: %p\n", G_STRFUNC, add_event_data->day_view);
		g_print ("\tDay view lower: %s", ctime (&add_event_data->day_view->lower));
		g_print ("\tDay view upper: %s", ctime (&add_event_data->day_view->upper));
		g_print ("\tEvent start: %s", ctime (&start));
		g_print ("\tEvent end  : %s\n", ctime (&end));
	}*/

	/* Check that the event times are valid. */
	g_return_if_fail (start <= end);
	g_return_if_fail (start < add_event_data->day_view->upper);

	if (end != start || end < add_event_data->day_view->lower)
		g_return_if_fail (end > add_event_data->day_view->lower);

	zone = e_calendar_view_get_timezone (E_CALENDAR_VIEW (add_event_data->day_view));
	start_tt = i_cal_time_new_from_timet_with_zone (start, FALSE, zone);
	end_tt = i_cal_time_new_from_timet_with_zone (end, FALSE, zone);

	if (add_event_data->comp_data) {
		event.comp_data = g_object_ref (add_event_data->comp_data);
	} else {
		event.comp_data = g_object_new (E_TYPE_CAL_MODEL_COMPONENT, NULL);
		event.comp_data->is_new_component = TRUE;
		event.comp_data->client = g_object_ref (client);
		e_cal_component_abort_sequence (comp);
		event.comp_data->icalcomp = i_cal_component_clone (e_cal_component_get_icalcomponent (comp));
	}

	event.start = start;
	event.color = NULL;
	event.end = end;
	event.canvas_item = NULL;
	event.comp_data->instance_start = start;
	event.comp_data->instance_end = end;

	/* Calculate the start & end minute, relative to the top of the
	 * display. */
	offset = add_event_data->day_view->first_hour_shown * 60
		+ add_event_data->day_view->first_minute_shown;
	event.start_minute = i_cal_time_get_hour (start_tt) * 60 + i_cal_time_get_minute (start_tt) - offset;
	event.end_minute = i_cal_time_get_hour (end_tt) * 60 + i_cal_time_get_minute (end_tt) - offset;

	g_clear_object (&start_tt);
	g_clear_object (&end_tt);

	event.start_row_or_col = 0;
	event.num_columns = 0;

	event.different_timezone = FALSE;
	if (!cal_comp_util_compare_event_timezones (comp, event.comp_data->client, zone))
		event.different_timezone = TRUE;

	if (!e_cal_component_has_attendees (comp) ||
	    itip_organizer_is_user (registry, comp, event.comp_data->client) ||
	    itip_sentby_is_user (registry, comp, event.comp_data->client))
		event.is_editable = TRUE;
	else
		event.is_editable = FALSE;

	days_shown = e_day_view_get_days_shown (add_event_data->day_view);

	/* Find out which array to add the event to. */
	for (day = 0; day < days_shown; day++) {
		if (start >= add_event_data->day_view->day_starts[day]
		    && end <= add_event_data->day_view->day_starts[day + 1]) {

			if (start == end && start == add_event_data->day_view->day_starts[day + 1])
				continue;

			/* Special case for when the appointment ends at
			 * midnight, i.e. the start of the next day. */
			if (end == add_event_data->day_view->day_starts[day + 1] && start != end) {

				/* If the event last the entire day, then we
				 * skip it here so it gets added to the top
				 * canvas. */
				if (start == add_event_data->day_view->day_starts[day])
					break;

				event.end_minute = 24 * 60;
			}

			g_array_append_val (add_event_data->day_view->events[day], event);
			add_event_data->day_view->events_sorted[day] = FALSE;
			add_event_data->day_view->need_layout[day] = TRUE;
			return;
		}
	}

	/* The event wasn't within one day so it must be a long event,
	 * i.e. shown in the top canvas. */
	g_array_append_val (add_event_data->day_view->long_events, event);
	add_event_data->day_view->long_events_sorted = FALSE;
	add_event_data->day_view->long_events_need_layout = TRUE;
}

/* This lays out the short (less than 1 day) events in the columns.
 * Any long events are simply skipped. */
void
e_day_view_check_layout (EDayView *day_view)
{
	ECalendarView *cal_view;
	gint time_divisions;
	gint day, rows_in_top_display;
	gint days_shown;
	gint max_cols = -1;

	days_shown = e_day_view_get_days_shown (day_view);

	cal_view = E_CALENDAR_VIEW (day_view);
	time_divisions = e_calendar_view_get_time_divisions (cal_view);

	/* Don't bother if we aren't visible. */
	if (!E_CALENDAR_VIEW (day_view)->in_focus) {
		e_day_view_free_events (day_view);
		day_view->requires_update = TRUE;
		return;
	}

	/* Make sure the events are sorted (by start and size). */
	e_day_view_ensure_events_sorted (day_view);

	for (day = 0; day < days_shown; day++) {
		if (day_view->need_layout[day]) {
			gint cols;

			cols = e_day_view_layout_day_events (
				day_view->events[day],
				day_view->rows,
				time_divisions,
				day_view->cols_per_row[day],
				days_shown == 1 ? -1 :
				E_DAY_VIEW_MULTI_DAY_MAX_COLUMNS);

			max_cols = MAX (cols, max_cols);
		}

		if (day_view->need_layout[day]
		    || day_view->need_reshape[day]) {
			e_day_view_reshape_day_events (day_view, day);

			if (day_view->resize_bars_event_day == day)
				e_day_view_reshape_main_canvas_resize_bars (day_view);
		}

		day_view->need_layout[day] = FALSE;
		day_view->need_reshape[day] = FALSE;
	}

	if (day_view->long_events_need_layout) {
		e_day_view_layout_long_events (
			day_view->long_events,
			days_shown,
			day_view->day_starts,
			&rows_in_top_display);
	}

	if (day_view->long_events_need_layout
	    || day_view->long_events_need_reshape)
		e_day_view_reshape_long_events (day_view);

	if (day_view->long_events_need_layout
			&& day_view->rows_in_top_display != rows_in_top_display) {
		day_view->rows_in_top_display = rows_in_top_display;
		e_day_view_update_top_scroll (day_view, FALSE);
	}

	day_view->long_events_need_layout = FALSE;
	day_view->long_events_need_reshape = FALSE;

	if (max_cols != -1 && max_cols != day_view->max_cols) {
		day_view->max_cols = max_cols;
		e_day_view_recalc_main_canvas_size (day_view);
	}
}

static void
e_day_view_on_text_item_notify_text_width (GObject *etext,
					   GParamSpec *param,
					   gpointer user_data)
{
	EDayView *day_view = user_data;
	gint event_num, day;

	g_return_if_fail (E_IS_DAY_VIEW (day_view));

	event_num = GPOINTER_TO_INT (g_object_get_data (etext, "event-num"));
	day = GPOINTER_TO_INT (g_object_get_data (etext, "event-day"));

	if (day == E_DAY_VIEW_LONG_EVENT)
		e_day_view_reshape_long_event (day_view, event_num);
	else
		e_day_view_reshape_day_event (day_view, day, event_num);
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
				g_object_run_dispose (G_OBJECT (event->canvas_item));
				event->canvas_item = NULL;
			}
		} else {
			e_day_view_reshape_long_event (day_view, event_num);
		}
	}
}

static void
e_day_view_reshape_long_event (EDayView *day_view,
                               gint event_num)
{
	EDayViewEvent *event;
	gint start_day, end_day, item_x, item_y, item_w, item_h;
	gint text_x, num_icons, icons_width, width, time_width;
	ECalComponent *comp;
	gint min_text_x, text_width, line_len;
	gchar *text, *end_of_line;
	gboolean show_icons = TRUE, use_max_width = FALSE;
	PangoContext *pango_context;
	PangoLayout *layout;

	if (!is_array_index_in_bounds (day_view->long_events, event_num))
		return;

	event = &g_array_index (day_view->long_events, EDayViewEvent,
				event_num);

	if (!e_day_view_get_long_event_position (day_view, event_num,
						 &start_day, &end_day,
						 &item_x, &item_y,
						 &item_w, &item_h)) {
		if (event->canvas_item) {
			g_object_run_dispose (G_OBJECT (event->canvas_item));
			event->canvas_item = NULL;
		}
		return;
	}

	if (!is_comp_data_valid (event))
		return;

	/* Take off the border and padding. */
	item_x += E_DAY_VIEW_LONG_EVENT_BORDER_WIDTH + E_DAY_VIEW_LONG_EVENT_X_PAD;
	item_w -= (E_DAY_VIEW_LONG_EVENT_BORDER_WIDTH + E_DAY_VIEW_LONG_EVENT_X_PAD) * 2;
	item_y += E_DAY_VIEW_LONG_EVENT_BORDER_HEIGHT + E_DAY_VIEW_LONG_EVENT_Y_PAD;
	item_h -= (E_DAY_VIEW_LONG_EVENT_BORDER_HEIGHT + E_DAY_VIEW_LONG_EVENT_Y_PAD) * 2;

	/* We don't show the icons while resizing, since we'd have to
	 * draw them on top of the resize rect. Nor when editing. */
	num_icons = 0;
	comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (event->comp_data->icalcomp));
	if (!comp)
		return;

	/* Set up Pango prerequisites */
	pango_context = gtk_widget_get_pango_context (GTK_WIDGET (day_view));
	layout = pango_layout_new (pango_context);

	if (day_view->resize_drag_pos != E_CALENDAR_VIEW_POS_NONE
	    && day_view->resize_event_day == E_DAY_VIEW_LONG_EVENT
	    && day_view->resize_event_num == event_num)
		show_icons = FALSE;

	if (day_view->editing_event_day == E_DAY_VIEW_LONG_EVENT
	    && day_view->editing_event_num == event_num) {
		show_icons = FALSE;
		use_max_width = TRUE;
	}

	if (show_icons) {
		if (e_cal_component_has_alarms (comp))
			num_icons++;
		if (e_cal_component_has_recurrences (comp) || e_cal_component_is_instance (comp))
			num_icons++;
		if (event->different_timezone)
			num_icons++;

		if (e_cal_component_has_attendees (comp))
			num_icons++;
		if (e_cal_component_has_attachments (comp))
			num_icons++;
		num_icons += cal_comp_util_get_n_icons (comp, NULL);
	}

	if (!event->canvas_item) {
		GdkRGBA rgba;

		rgba = e_day_view_get_text_color (day_view, event);
		event->canvas_item =
			gnome_canvas_item_new (
				GNOME_CANVAS_GROUP (GNOME_CANVAS (day_view->top_canvas)->root),
				e_text_get_type (),
				"clip", TRUE,
				"max_lines", 1,
				"editable", TRUE,
				"use_ellipsis", TRUE,
				"fill-color", &rgba,
				"im_context", E_CANVAS (day_view->top_canvas)->im_context,
				NULL);
		g_object_set_data (G_OBJECT (event->canvas_item), "event-num", GINT_TO_POINTER (event_num));
		g_object_set_data (G_OBJECT (event->canvas_item), "event-day", GINT_TO_POINTER (E_DAY_VIEW_LONG_EVENT));
		g_signal_connect (
			event->canvas_item, "event",
			G_CALLBACK (e_day_view_on_text_item_event), day_view);
		g_signal_connect (
			event->canvas_item, "notify::text-width",
			G_CALLBACK (e_day_view_on_text_item_notify_text_width), day_view);
		g_signal_emit_by_name (day_view, "event_added", event);

		e_day_view_update_long_event_label (day_view, event_num);
	} else if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (event->canvas_item), "event-num")) != event_num) {
		g_object_set_data (G_OBJECT (event->canvas_item), "event-num", GINT_TO_POINTER (event_num));
	}

	/* Calculate its position. We first calculate the ideal position which
	 * is centered with the icons. We then make sure we haven't gone off
	 * the left edge of the available space. Finally we make sure we don't
	 * go off the right edge. */
	icons_width = (E_DAY_VIEW_ICON_WIDTH + E_DAY_VIEW_ICON_X_PAD)
		* num_icons + E_DAY_VIEW_LONG_EVENT_ICON_R_PAD;
	time_width = e_day_view_get_time_string_width (day_view);

	if (use_max_width) {
		text_x = item_x;
	} else {
		gdouble item_text_width = 0;

		/* Get the requested size of the label. */
		g_object_get (event->canvas_item, "text-width", &item_text_width, NULL);

		text_width = (gint) item_text_width;

		if (text_width <= 0) {
			g_object_get (event->canvas_item, "text", &text, NULL);
			text_width = 0;
			if (text) {
				end_of_line = strchr (text, '\n');
				if (end_of_line)
					line_len = end_of_line - text;
				else
					line_len = strlen (text);
				pango_layout_set_text (layout, text, line_len);
				pango_layout_get_pixel_size (layout, &text_width, NULL);
				g_free (text);
			}
		}

		width = text_width + icons_width;
		text_x = item_x + (item_w - width) / 2;

		min_text_x = item_x;
		if (event->start > day_view->day_starts[start_day])
			min_text_x += time_width + E_DAY_VIEW_LONG_EVENT_TIME_X_PAD;

		text_x = MAX (text_x, min_text_x);

		/* Now take out the space for the icons. */
		text_x += icons_width;
	}

	gnome_canvas_item_set (
		event->canvas_item,
		"x_offset", (gdouble) MAX (0, text_x - item_x),
		"clip_width", (gdouble) MAX (0, item_w - (E_DAY_VIEW_LONG_EVENT_TIME_X_PAD * 2)),
		"clip_height", (gdouble) item_h,
		NULL);
	e_canvas_item_move_absolute (
		event->canvas_item,
		item_x, item_y);

	g_object_unref (layout);
	g_object_unref (comp);
}

/* This creates or updates the sizes of the canvas items for one day of the
 * main canvas. */
static void
e_day_view_reshape_day_events (EDayView *day_view,
                               gint day)
{
	gint event_num;

	for (event_num = 0; event_num < day_view->events[day]->len;
	     event_num++) {
		EDayViewEvent *event;
		gchar *current_comp_string;

		e_day_view_reshape_day_event (day_view, day, event_num);
		event = &g_array_index (day_view->events[day], EDayViewEvent, event_num);

		if (!is_comp_data_valid (event))
			continue;

		current_comp_string = i_cal_component_as_ical_string (event->comp_data->icalcomp);
		if (day_view->last_edited_comp_string == NULL) {
			g_free (current_comp_string);
			continue;
		}

		if (strncmp (current_comp_string, day_view->last_edited_comp_string, 50) == 0) {
			if (e_calendar_view_get_allow_direct_summary_edit (E_CALENDAR_VIEW (day_view)))
				e_canvas_item_grab_focus (event->canvas_item, TRUE);

			g_free (day_view->last_edited_comp_string);
			day_view-> last_edited_comp_string = NULL;
		}
		g_free (current_comp_string);
	}
}

static void
e_day_view_reshape_day_event (EDayView *day_view,
                              gint day,
                              gint event_num)
{
	EDayViewEvent *event;
	gint item_x, item_y, item_w, item_h;
	gint num_icons, icons_offset;

	if (!is_array_index_in_bounds (day_view->events[day], event_num))
		return;

	event = &g_array_index (day_view->events[day], EDayViewEvent,
				event_num);

	if (!e_day_view_get_event_position (day_view, day, event_num,
					    &item_x, &item_y,
					    &item_w, &item_h)) {
		if (event->canvas_item) {
			g_object_run_dispose (G_OBJECT (event->canvas_item));
			event->canvas_item = NULL;
		}
	} else {
		/* Skip the border and padding. */
		item_x += E_DAY_VIEW_BAR_WIDTH + E_DAY_VIEW_EVENT_X_PAD;
		item_w -= E_DAY_VIEW_BAR_WIDTH + E_DAY_VIEW_EVENT_X_PAD * 2;
		item_y += E_DAY_VIEW_EVENT_BORDER_HEIGHT + E_DAY_VIEW_EVENT_Y_PAD;
		item_h -= (E_DAY_VIEW_EVENT_BORDER_HEIGHT + E_DAY_VIEW_EVENT_Y_PAD) * 2;

		/* We don't show the icons while resizing, since we'd have to
		 * draw them on top of the resize rect. */
		icons_offset = 0;
		num_icons = 0;
		if (is_comp_data_valid (event) && (day_view->resize_drag_pos == E_CALENDAR_VIEW_POS_NONE
		    || day_view->resize_event_day != day
		    || day_view->resize_event_num != event_num)) {
			ECalComponent *comp;

			comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (event->comp_data->icalcomp));
			if (comp) {
				if (e_cal_component_has_alarms (comp))
					num_icons++;
				if (e_cal_component_has_recurrences (comp) || e_cal_component_is_instance (comp))
					num_icons++;
				if (e_cal_component_has_attachments (comp))
					num_icons++;
				if (event->different_timezone)
					num_icons++;
				if (e_cal_component_has_attendees (comp))
					num_icons++;

				num_icons += cal_comp_util_get_n_icons (comp, NULL);
				g_object_unref (comp);
			}
		}

		if (num_icons > 1) {
			if (item_h >= (E_DAY_VIEW_ICON_HEIGHT + E_DAY_VIEW_ICON_Y_PAD) * num_icons)
				icons_offset = E_DAY_VIEW_ICON_WIDTH + E_DAY_VIEW_ICON_X_PAD * 2;
			else if (item_h <= (E_DAY_VIEW_ICON_HEIGHT + E_DAY_VIEW_ICON_Y_PAD) * 2)
				icons_offset = (E_DAY_VIEW_ICON_WIDTH + E_DAY_VIEW_ICON_X_PAD) * num_icons + E_DAY_VIEW_ICON_X_PAD;
			else
				icons_offset = E_DAY_VIEW_ICON_X_PAD;
		} else if (num_icons == 1) {
			if (item_h < (E_DAY_VIEW_ICON_HEIGHT + E_DAY_VIEW_ICON_Y_PAD) * 2)
				icons_offset = E_DAY_VIEW_ICON_WIDTH + E_DAY_VIEW_ICON_X_PAD * 2;
			else
				icons_offset = E_DAY_VIEW_ICON_X_PAD;
		}

		if (!event->canvas_item) {
			GdkRGBA rgba;

			rgba = e_day_view_get_text_color (day_view, event);
			event->canvas_item = gnome_canvas_item_new (
				GNOME_CANVAS_GROUP (GNOME_CANVAS (day_view->main_canvas)->root),
				e_text_get_type (),
				"line_wrap", TRUE,
				"editable", TRUE,
				"clip", TRUE,
				"use_ellipsis", TRUE,
				"fill-color", &rgba,
				"im_context", E_CANVAS (day_view->main_canvas)->im_context,
				NULL);
			g_object_set_data (G_OBJECT (event->canvas_item), "event-num", GINT_TO_POINTER (event_num));
			g_object_set_data (G_OBJECT (event->canvas_item), "event-day", GINT_TO_POINTER (day));
			g_signal_connect (
				event->canvas_item, "event",
				G_CALLBACK (e_day_view_on_text_item_event), day_view);
			g_signal_emit_by_name (day_view, "event_added", event);

			e_day_view_update_event_label (day_view, day, event_num);
		} else if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (event->canvas_item), "event-num")) != event_num) {
			g_object_set_data (G_OBJECT (event->canvas_item), "event-num", GINT_TO_POINTER (event_num));
		}

		item_w = MAX (item_w, 0);
		gnome_canvas_item_set (
			event->canvas_item,
			"clip_width", (gdouble) item_w,
			"clip_height", (gdouble) item_h,
			"x_offset", (gdouble) icons_offset,
			NULL);
		e_canvas_item_move_absolute (
			event->canvas_item,
			item_x, item_y);
	}
}

/* This creates or resizes the horizontal bars used to resize events in the
 * main canvas. */
static void
e_day_view_reshape_main_canvas_resize_bars (EDayView *day_view)
{
	gint day, event_num;
	gint item_x, item_y, item_w, item_h;
	gdouble x, y, w, h;

	day = day_view->resize_bars_event_day;
	event_num = day_view->resize_bars_event_num;

	/* If we're not editing an event, or the event is not shown,
	 * hide the resize bars. */
	if (day != -1 && day == day_view->drag_event_day
	    && event_num == day_view->drag_event_num) {
		g_object_get (
			day_view->drag_rect_item,
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

		gtk_widget_queue_draw (day_view->main_canvas);
	} else {
		return;
	}
}

static void
e_day_view_ensure_events_sorted (EDayView *day_view)
{
	gint day;
	gint days_shown;

	days_shown = e_day_view_get_days_shown (day_view);

	/* Sort the long events. */
	if (!day_view->long_events_sorted) {
		qsort (
			day_view->long_events->data,
			day_view->long_events->len,
			sizeof (EDayViewEvent),
			e_day_view_event_sort_func);
		day_view->long_events_sorted = TRUE;
	}

	/* Sort the events for each day. */
	for (day = 0; day < days_shown; day++) {
		if (!day_view->events_sorted[day]) {
			qsort (
				day_view->events[day]->data,
				day_view->events[day]->len,
				sizeof (EDayViewEvent),
				e_day_view_event_sort_func);
			day_view->events_sorted[day] = TRUE;
		}
	}
}

gint
e_day_view_event_sort_func (gconstpointer arg1,
                            gconstpointer arg2)
{
	EDayViewEvent *event1, *event2;

	event1 = (EDayViewEvent *) arg1;
	event2 = (EDayViewEvent *) arg2;

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

static gboolean
e_day_view_do_key_press (GtkWidget *widget,
                         GdkEventKey *event)
{
	EDayView *day_view;
	guint keyval;
	gboolean stop_emission;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (E_IS_DAY_VIEW (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	day_view = E_DAY_VIEW (widget);
	keyval = event->keyval;

	/* The Escape key aborts a resize operation. */
	if (day_view->resize_drag_pos != E_CALENDAR_VIEW_POS_NONE) {
		if (keyval == GDK_KEY_Escape) {
			if (day_view->grabbed_pointer != NULL) {
				gdk_device_ungrab (
					day_view->grabbed_pointer,
					event->time);
				g_object_unref (day_view->grabbed_pointer);
				day_view->grabbed_pointer = NULL;
			}
			e_day_view_abort_resize (day_view);
		}
		return FALSE;
	}

	/* Alt + Arrow Keys to move a selected event through time lines */
	if (((event->state & GDK_SHIFT_MASK) != GDK_SHIFT_MASK)
		&&((event->state & GDK_CONTROL_MASK) != GDK_CONTROL_MASK)
		&&((event->state & GDK_MOD1_MASK) == GDK_MOD1_MASK)) {
		if (keyval == GDK_KEY_Up || keyval == GDK_KEY_KP_Up)
			return e_day_view_event_move ((ECalendarView *) day_view, E_CAL_VIEW_MOVE_UP);
		else if (keyval == GDK_KEY_Down || keyval == GDK_KEY_KP_Down)
			return e_day_view_event_move ((ECalendarView *) day_view, E_CAL_VIEW_MOVE_DOWN);
		else if (keyval == GDK_KEY_Left || keyval == GDK_KEY_KP_Left)
			return e_day_view_event_move ((ECalendarView *) day_view, E_CAL_VIEW_MOVE_LEFT);
		else if (keyval == GDK_KEY_Right || keyval == GDK_KEY_KP_Right)
			return e_day_view_event_move ((ECalendarView *) day_view, E_CAL_VIEW_MOVE_RIGHT);
	}

	/*Go to the start/end of a work day*/
	if ((keyval == GDK_KEY_Home)
			&&((event->state & GDK_SHIFT_MASK) != GDK_SHIFT_MASK)
			&&((event->state & GDK_CONTROL_MASK) != GDK_CONTROL_MASK)
			&&((event->state & GDK_MOD1_MASK) != GDK_MOD1_MASK)) {
		e_day_view_goto_start_of_work_day (day_view);
		return TRUE;
	}
	if ((keyval == GDK_KEY_End)
	    &&((event->state & GDK_SHIFT_MASK) != GDK_SHIFT_MASK)
	    &&((event->state & GDK_CONTROL_MASK) != GDK_CONTROL_MASK)
	    &&((event->state & GDK_MOD1_MASK) != GDK_MOD1_MASK)) {
		e_day_view_goto_end_of_work_day (day_view);
		return TRUE;
	}

	/* In DayView, Shift+Home/End, Change the duration to the time that begins/ends the current work day */
	if ((keyval == GDK_KEY_Home)
	    &&((event->state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK)
	    &&((event->state & GDK_CONTROL_MASK) != GDK_CONTROL_MASK)
	    &&((event->state & GDK_MOD1_MASK) != GDK_MOD1_MASK)) {
		e_day_view_change_duration_to_start_of_work_day (day_view);
		return TRUE;
	}
	if ((keyval == GDK_KEY_End)
	    &&((event->state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK)
	    &&((event->state & GDK_CONTROL_MASK) != GDK_CONTROL_MASK)
	    &&((event->state & GDK_MOD1_MASK) != GDK_MOD1_MASK)) {
		e_day_view_change_duration_to_end_of_work_day (day_view);
		return TRUE;
	}

	/* Handle the cursor keys for moving & extending the selection. */
	stop_emission = TRUE;
	if (event->state & GDK_SHIFT_MASK) {
		switch (keyval) {
		case GDK_KEY_Up:
			e_day_view_cursor_key_up_shifted (day_view, event);
			break;
		case GDK_KEY_Down:
			e_day_view_cursor_key_down_shifted (day_view, event);
			break;
		case GDK_KEY_Left:
			e_day_view_cursor_key_left_shifted (day_view, event);
			break;
		case GDK_KEY_Right:
			e_day_view_cursor_key_right_shifted (day_view, event);
			break;
		default:
			stop_emission = FALSE;
			break;
		}
	} else if (!(event->state & GDK_MOD1_MASK) &&
		   !(event->state & GDK_CONTROL_MASK)) {
		switch (keyval) {
		case GDK_KEY_Up:
			e_day_view_cursor_key_up (day_view, event);
			break;
		case GDK_KEY_Down:
			e_day_view_cursor_key_down (day_view, event);
			break;
		case GDK_KEY_Left:
			e_day_view_cursor_key_left (day_view, event);
			break;
		case GDK_KEY_Right:
			e_day_view_cursor_key_right (day_view, event);
			break;
		case GDK_KEY_Page_Up:
			e_day_view_scroll (day_view, E_DAY_VIEW_PAGE_STEP);
			break;
		case GDK_KEY_Page_Down:
			e_day_view_scroll (day_view, -E_DAY_VIEW_PAGE_STEP);
			break;
		default:
			stop_emission = FALSE;
			break;
		}
	}
	else
		stop_emission = FALSE;
	if (stop_emission)
		return TRUE;

	if (day_view->selection_start_day == -1)
		return FALSE;

	/* We only want to start an edit with a return key or a simple
	 * character. */
	if ((keyval != GDK_KEY_Return && keyval != GDK_KEY_KP_Enter) &&
	    (((keyval >= 0x20) && (keyval <= 0xFF)
	      && (event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)))
	     || (event->length == 0)
	     || (keyval == GDK_KEY_Tab)
	     || (keyval == GDK_KEY_Escape)
	     || (keyval == GDK_KEY_Delete)
	     || (keyval == GDK_KEY_KP_Delete))) {
		return FALSE;
	}

	e_day_view_add_new_event_in_selected_range (day_view, event, FALSE);

	return TRUE;
}

/* Select the time that begins a work day*/
static void
e_day_view_goto_start_of_work_day (EDayView *day_view)
{
	gint work_day_start_hour;
	gint work_day_start_minute;
	gint work_day_end_hour;
	gint work_day_end_minute;

	if (day_view->selection_in_top_canvas)
		return;

	e_day_view_get_work_day_range_for_day (day_view, day_view->selection_start_day,
		&work_day_start_hour, &work_day_start_minute,
		&work_day_end_hour, &work_day_end_minute);

	day_view->selection_start_row =
		e_day_view_convert_time_to_row (
		day_view, work_day_start_hour, work_day_start_minute);
	day_view->selection_end_row = day_view->selection_start_row;
	day_view->selection_end_day = day_view->selection_start_day;

	e_day_view_ensure_rows_visible (
		day_view,
		day_view->selection_start_row,
		day_view->selection_end_row);

	e_day_view_update_calendar_selection_time (day_view);

	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->top_dates_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);
}

/* Select the time that ends a work day*/
static void
e_day_view_goto_end_of_work_day (EDayView *day_view)
{
	gint work_day_start_hour;
	gint work_day_start_minute;
	gint work_day_end_hour;
	gint work_day_end_minute;

	if (day_view->selection_in_top_canvas)
		return;

	e_day_view_get_work_day_range_for_day (day_view, day_view->selection_end_day,
		&work_day_start_hour, &work_day_start_minute,
		&work_day_end_hour, &work_day_end_minute);

	day_view->selection_start_row =
		e_day_view_convert_time_to_row (
		day_view, work_day_end_hour - 1, work_day_end_minute + 30);
	day_view->selection_end_row = day_view->selection_start_row;
	day_view->selection_start_day = day_view->selection_end_day;

	e_day_view_ensure_rows_visible (
		day_view,
		day_view->selection_start_row,
		day_view->selection_end_row);

	e_day_view_update_calendar_selection_time (day_view);

	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->top_dates_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);
}

/* Change the duration to the time that begins the current work day */
static void
e_day_view_change_duration_to_start_of_work_day (EDayView *day_view)
{
	gint work_start_row;
	gint work_day_start_hour;
	gint work_day_start_minute;
	gint work_day_end_hour;
	gint work_day_end_minute;

	g_return_if_fail (day_view != NULL);

	if (day_view->selection_in_top_canvas)
		return;

	e_day_view_get_work_day_range_for_day (day_view, day_view->selection_start_day,
		&work_day_start_hour, &work_day_start_minute,
		&work_day_end_hour, &work_day_end_minute);

	work_start_row = e_day_view_convert_time_to_row (day_view, work_day_start_hour, work_day_start_minute);

	if (day_view->selection_start_row < work_start_row)
		day_view->selection_end_row = work_start_row - 1;
	else
		day_view->selection_start_row = work_start_row;

	e_day_view_ensure_rows_visible (
		day_view,
		day_view->selection_start_row,
		day_view->selection_end_row);

	e_day_view_update_calendar_selection_time (day_view);

	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->top_dates_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);
}

/* Change the duration to the time that ends the current work day */
static void
e_day_view_change_duration_to_end_of_work_day (EDayView *day_view)
{
	gint selection_start_row, work_end_row;
	gint work_day_start_hour;
	gint work_day_start_minute;
	gint work_day_end_hour;
	gint work_day_end_minute;

	g_return_if_fail (day_view != NULL);

	if (day_view->selection_in_top_canvas)
		return;

	e_day_view_get_work_day_range_for_day (day_view, day_view->selection_start_day,
		&work_day_start_hour, &work_day_start_minute,
		&work_day_end_hour, &work_day_end_minute);

	work_end_row = e_day_view_convert_time_to_row (
		day_view, work_day_end_hour - 1, work_day_end_minute + 30);
	selection_start_row = day_view->selection_start_row;

	if (selection_start_row <= work_end_row) {
		day_view->selection_end_row = work_end_row;
	} else {
		day_view->selection_start_row = work_end_row + 1;
		day_view->selection_end_row = selection_start_row;
	}

	e_day_view_ensure_rows_visible (
		day_view,
		day_view->selection_start_row,
		day_view->selection_end_row);

	e_day_view_update_calendar_selection_time (day_view);

	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->top_dates_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);
}

static void
e_day_view_cursor_key_up_shifted (EDayView *day_view,
                                  GdkEventKey *event)
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

/**
 * e_day_view_get_extreme_event
 * @day_view: the day view widget operates on
 * @start_day, @end_day: range of search, both inclusive
 * @first: %TURE indicate to return the data for the first event in the range,
 *         %FALSE to return data for the last event in the range.
 * @day_out: out value, day of the event found. -1 for no event found.
 * @event_num_out: out value, event number of the event found.
 *                  -1 for no event found.
 *
 * Get day and event_num value for the first or last event found in the day range.
 *
 * Return value: %TRUE, if a event found.
 **/
static gboolean
e_day_view_get_extreme_event (EDayView *day_view,
                              gint start_day,
                              gint end_day,
                              gboolean first,
                              gint *day_out,
                              gint *event_num_out)
{
	gint loop_day;

	g_return_val_if_fail (day_view != NULL, FALSE);
	g_return_val_if_fail (start_day >= 0, FALSE);
	g_return_val_if_fail (end_day <= E_DAY_VIEW_LONG_EVENT, FALSE);
	g_return_val_if_fail (day_out && event_num_out, FALSE);

	if (start_day > end_day)
		return FALSE;
	if (first) {
		for (loop_day = start_day; loop_day <= end_day; ++loop_day)
			if (day_view->events[loop_day]->len > 0) {
				*day_out = loop_day;
				*event_num_out = 0;
				return TRUE;
			}
	}
	else {
		for (loop_day = end_day; loop_day >= start_day; --loop_day)
			if (day_view->events[loop_day]->len > 0) {
				*day_out = loop_day;
				*event_num_out =
					day_view->events[loop_day]->len - 1;
				return TRUE;
			}
	}
	*day_out = -1;
	*event_num_out = -1;
	return FALSE;
}

/**
 * e_day_view_get_extreme_long_event
 * @day_view: the day view widget operates on
 * @first: %TURE indicate to return the data for the first event in the range,
 *         %FALSE to return data for the last event in the range.
 * @event_num_out: out value, event number of the event found.
 *                  -1 for no event found.
 *
 * Similar to e_day_view_get_extreme_event, but run for long events.
 *
 * Return value: %TRUE, if a event found.
 **/
static gboolean
e_day_view_get_extreme_long_event (EDayView *day_view,
                                   gboolean first,
                                   gint *day_out,
                                   gint *event_num_out)
{
	g_return_val_if_fail (day_view != NULL, FALSE);
	g_return_val_if_fail (day_out && event_num_out, FALSE);

	if (first && (day_view->long_events->len > 0)) {
		*day_out = E_DAY_VIEW_LONG_EVENT;
		*event_num_out = 0;
		return TRUE;
	}
	if ((!first) && (day_view->long_events->len > 0)) {
		*day_out = E_DAY_VIEW_LONG_EVENT;
		*event_num_out = day_view->long_events->len - 1;
		return TRUE;
	}
	*day_out = -1;
	*event_num_out = -1;
	return FALSE;
}

/**
 * e_day_view_get_next_tab_event
 * @day_view: the day view widget operates on
 * @direction: GTK_DIR_TAB_BACKWARD or GTK_DIR_TAB_FORWARD
 * @day_out: out value, day of the event found. -1 for no event found.
 * @event_num_out: out value, event number of the event found.
 *                  -1 for no event found.
 *
 * Decide on which event the focus should go next.
 * if ((day_out == -1) && (event_num_out == -1)) is true, focus should go
 * to day_view widget itself.
 *
 * Return value: %TRUE, if a event found.
 **/
static gboolean
e_day_view_get_next_tab_event (EDayView *day_view,
                               GtkDirectionType direction,
                               gint *day_out,
                               gint *event_num_out)
{
	gint new_day;
	gint new_event_num;
	gint days_shown;

	g_return_val_if_fail (day_view != NULL, FALSE);
	g_return_val_if_fail (day_out != NULL, FALSE);
	g_return_val_if_fail (event_num_out != NULL, FALSE);

	days_shown = e_day_view_get_days_shown (day_view);
	*day_out = -1;
	*event_num_out = -1;

	g_return_val_if_fail (days_shown > 0, FALSE);

	switch (direction) {
	case GTK_DIR_TAB_BACKWARD:
		new_event_num = day_view->editing_event_num - 1;
		break;
	case GTK_DIR_TAB_FORWARD:
		new_event_num = day_view->editing_event_num + 1;
		break;
	default:
		return FALSE;
	}

	new_day = day_view->editing_event_day;

	/* not current editing event, set to first long event if there is one
	 */
	if (new_day == -1) {
		if (direction == GTK_DIR_TAB_FORWARD) {
			if (e_day_view_get_extreme_long_event (day_view, TRUE,
							       day_out,
							       event_num_out))
				return TRUE;

			/* no long event, set to first event if there is
			 */
			e_day_view_get_extreme_event (
				day_view, 0,
				days_shown - 1, TRUE,
				day_out, event_num_out);
			/* go to event if found, or day view widget
			 */
			return TRUE;
		}
		else {
			if (e_day_view_get_extreme_event (day_view, 0,
							  days_shown - 1, FALSE,
							  day_out, event_num_out))
				return TRUE;

			/* no event, set to last long event if there is
			 */
			e_day_view_get_extreme_long_event (
				day_view, FALSE,
				day_out,
				event_num_out);

			/* go to long event if found, or day view widget
			 */
			return TRUE;
		}
	}
	/* go backward from the first long event */
	else if ((new_day == E_DAY_VIEW_LONG_EVENT) && (new_event_num < 0)) {
		/* let focus go to day view widget in this case
		 */
		return TRUE;
	}
	/* go forward from the last long event */
	else if ((new_day == E_DAY_VIEW_LONG_EVENT) &&
		 (new_event_num >= day_view->long_events->len)) {
		e_day_view_get_extreme_event (
			day_view, 0,
			days_shown - 1, TRUE,
			day_out, event_num_out);
		/* go to the next main item event if found or day view widget
		 */
		return TRUE;
	}

	/* go backward from the first event in current editting day */
	else if ((new_day < E_DAY_VIEW_LONG_EVENT) && (new_event_num < 0)) {
		/* try to find a event from the previous day in days shown
		 */
		if (e_day_view_get_extreme_event (day_view, 0,
						  new_day - 1, FALSE,
						  day_out, event_num_out))
			return TRUE;
		/* try to find a long event
		 */
		e_day_view_get_extreme_long_event (
			day_view, FALSE,
			day_out, event_num_out);
		/* go to a long event if found, or day view widget
		 */
		return TRUE;
	}
	/* go forward from the last event in current editting day */
	else if ((new_day < E_DAY_VIEW_LONG_EVENT) &&
		 (new_event_num >= day_view->events[new_day]->len)) {
		/* try to find a event from the next day in days shown
		 */
		e_day_view_get_extreme_event (
			day_view, (new_day + 1),
			days_shown - 1, TRUE,
			day_out, event_num_out);
		/* go to a event found, or day view widget
		 */
		return TRUE;
	}
	/* in the normal case
	 */
	*day_out = new_day;
	*event_num_out = new_event_num;
	return TRUE;
}

static void
e_day_view_cursor_key_down_shifted (EDayView *day_view,
                                    GdkEventKey *event)
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
e_day_view_cursor_key_left_shifted (EDayView *day_view,
                                    GdkEventKey *event)
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
e_day_view_cursor_key_right_shifted (EDayView *day_view,
                                     GdkEventKey *event)
{
	gint *day;

	if (day_view->selection_drag_pos == E_DAY_VIEW_DRAG_START)
		day = &day_view->selection_start_day;
	else
		day = &day_view->selection_end_day;

	if (*day >= e_day_view_get_days_shown (day_view) - 1)
		return;

	*day = *day + 1;

	e_day_view_normalize_selection (day_view);

	e_day_view_update_calendar_selection_time (day_view);

	/* FIXME: Optimise? */
	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);
}

static void
e_day_view_cursor_key_up (EDayView *day_view,
                          GdkEventKey *event)
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
		e_day_view_ensure_rows_visible (
			day_view,
			day_view->selection_start_row,
			day_view->selection_end_row);

	g_signal_emit_by_name (day_view, "selected_time_changed");
	e_day_view_update_calendar_selection_time (day_view);

	/* FIXME: Optimise? */
	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);
}

static void
e_day_view_cursor_key_down (EDayView *day_view,
                            GdkEventKey *event)
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
		e_day_view_ensure_rows_visible (
			day_view,
						day_view->selection_start_row,
						day_view->selection_end_row);

	g_signal_emit_by_name (day_view, "selected_time_changed");
	e_day_view_update_calendar_selection_time (day_view);

	/* FIXME: Optimise? */
	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);
}

static void
e_day_view_cursor_key_left (EDayView *day_view,
                            GdkEventKey *event)
{
	if (day_view->selection_start_day == 0) {
		e_calendar_view_move_view_range (E_CALENDAR_VIEW (day_view), E_CALENDAR_VIEW_MOVE_PREVIOUS, 0);
	} else {
		day_view->selection_start_day--;
		day_view->selection_end_day--;

		e_day_view_update_calendar_selection_time (day_view);

		/* FIXME: Optimise? */
		gtk_widget_queue_draw (day_view->top_canvas);
		gtk_widget_queue_draw (day_view->main_canvas);
	}
	g_signal_emit_by_name (day_view, "selected_time_changed");
}

static void
e_day_view_cursor_key_right (EDayView *day_view,
                             GdkEventKey *event)
{
	gint days_shown;

	days_shown = e_day_view_get_days_shown (day_view);

	if (day_view->selection_end_day == days_shown - 1) {
		e_calendar_view_move_view_range (E_CALENDAR_VIEW (day_view), E_CALENDAR_VIEW_MOVE_NEXT, 0);
	} else {
		day_view->selection_start_day++;
		day_view->selection_end_day++;

		e_day_view_update_calendar_selection_time (day_view);

		/* FIXME: Optimise? */
		gtk_widget_queue_draw (day_view->top_canvas);
		gtk_widget_queue_draw (day_view->main_canvas);
	}
	g_signal_emit_by_name (day_view, "selected_time_changed");
}

/* Scrolls the main canvas up or down. The pages_to_scroll argument
 * is multiplied with the adjustment's page size and added to the adjustment's
 * value, while ensuring we stay within the bounds. A positive value will
 * scroll the canvas down and a negative value will scroll it up. */
static void
e_day_view_scroll (EDayView *day_view,
                   gfloat pages_to_scroll)
{
	GtkAdjustment *adjustment;
	GtkScrollable *scrollable;
	gdouble new_value;
	gdouble page_size;
	gdouble lower;
	gdouble upper;
	gdouble value;

	scrollable = GTK_SCROLLABLE (day_view->main_canvas);
	adjustment = gtk_scrollable_get_vadjustment (scrollable);

	page_size = gtk_adjustment_get_page_size (adjustment);
	lower = gtk_adjustment_get_lower (adjustment);
	upper = gtk_adjustment_get_upper (adjustment);
	value = gtk_adjustment_get_value (adjustment);

	new_value = value - page_size * pages_to_scroll;
	new_value = CLAMP (new_value, lower, upper - page_size);
	gtk_adjustment_set_value (adjustment, new_value);
}

static void
e_day_view_top_scroll (EDayView *day_view,
                       gfloat pages_to_scroll)
{
	GtkAdjustment *adjustment;
	GtkScrollable *scrollable;
	gdouble new_value;
	gdouble page_size;
	gdouble lower;
	gdouble upper;
	gdouble value;

	scrollable = GTK_SCROLLABLE (day_view->top_canvas);
	adjustment = gtk_scrollable_get_vadjustment (scrollable);

	page_size = gtk_adjustment_get_page_size (adjustment);
	lower = gtk_adjustment_get_lower (adjustment);
	upper = gtk_adjustment_get_upper (adjustment);
	value = gtk_adjustment_get_value (adjustment);

	new_value = value - page_size * pages_to_scroll;
	new_value = CLAMP (new_value, lower, upper - page_size);
	gtk_adjustment_set_value (adjustment, new_value);
}

void
e_day_view_ensure_rows_visible (EDayView *day_view,
                                gint start_row,
                                gint end_row)
{
	GtkAdjustment *adjustment;
	GtkScrollable *scrollable;
	gdouble max_value;
	gdouble min_value;
	gdouble page_size;
	gdouble value;

	scrollable = GTK_SCROLLABLE (day_view->main_canvas);
	adjustment = gtk_scrollable_get_vadjustment (scrollable);

	value = gtk_adjustment_get_value (adjustment);
	page_size = gtk_adjustment_get_page_size (adjustment);

	min_value = (end_row + 1) * day_view->row_height - page_size;
	if (value < min_value)
		value = min_value;

	max_value = start_row * day_view->row_height;
	if (value > max_value)
		value = max_value;

	gtk_adjustment_set_value (adjustment, value);
}

static void
e_day_view_start_editing_event (EDayView *day_view,
                                gint day,
                                gint event_num,
                                GdkEventKey *key_event)
{
	EDayViewEvent *event;
	ETextEventProcessor *event_processor = NULL;
	ETextEventProcessorCommand command;

	/* If we are already editing the event, just return. */
	if (day == day_view->editing_event_day
	    && event_num == day_view->editing_event_num)
		return;

	event = e_day_view_get_event (day_view, day, event_num);
	if (!is_comp_data_valid (event))
		return;

	if (e_client_is_readonly (E_CLIENT (event->comp_data->client)) ||
	    (!key_event && !e_calendar_view_get_allow_direct_summary_edit (E_CALENDAR_VIEW (day_view))))
		return;

	/* If the event is not shown, don't try to edit it. */
	if (!event->canvas_item)
		return;

	/* We must grab the focus before setting the initial text, since
	 * grabbing the focus will result in a call to
	 * e_day_view_on_editing_started (), which will reset the text to get
	 * rid of the start and end times. */
	e_canvas_item_grab_focus (event->canvas_item, TRUE);

	if (key_event) {
		if (gtk_im_context_filter_keypress (((EText *)(event->canvas_item))->im_context, key_event)) {
			((EText *)(event->canvas_item))->need_im_reset = TRUE;
		} else if (key_event->keyval != GDK_KEY_Return && key_event->keyval != GDK_KEY_KP_Enter) {
			gchar *initial_text;

			initial_text = e_utf8_from_gtk_event_key (GTK_WIDGET (day_view), key_event->keyval, key_event->string);
			gnome_canvas_item_set (
				event->canvas_item,
				"text", initial_text,
				NULL);
			g_free (initial_text);
		}
	}

	/* Try to move the cursor to the end of the text. */
	g_object_get (
		event->canvas_item,
		"event_processor", &event_processor,
		NULL);
	if (event_processor) {
		command.action = E_TEP_MOVE;
		command.position = E_TEP_END_OF_BUFFER;
		g_signal_emit_by_name (
			event_processor,
			"command", &command);
	}
}

/* This stops the current edit. If accept is TRUE the event summary is updated,
 * else the edit is cancelled. */
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

/* Cancels the current edition by resetting the appointment's text to its original value */
static void
cancel_editing (EDayView *day_view)
{
	gint day, event_num;
	EDayViewEvent *event;
	const gchar *summary;

	day = day_view->editing_event_day;
	event_num = day_view->editing_event_num;

	if (day == -1)
		return;

	event = e_day_view_get_event (day_view, day, event_num);
	if (!is_comp_data_valid (event))
		return;

	/* Reset the text to what was in the component */

	summary = i_cal_component_get_summary (event->comp_data->icalcomp);
	g_object_set (
		event->canvas_item,
		"text", summary ? summary : "",
		NULL);

	/* Stop editing */
	e_day_view_stop_editing_event (day_view);
}

static gboolean
e_day_view_on_text_item_event (GnomeCanvasItem *item,
                               GdkEvent *event,
                               EDayView *day_view)
{
	switch (event->type) {
	case GDK_KEY_PRESS:
		if (!E_TEXT (item)->preedit_len && event && (
		     event->key.keyval == GDK_KEY_Return ||
		     event->key.keyval == GDK_KEY_KP_Enter)) {
			day_view->resize_event_num = -1;

			/* We set the keyboard focus to the EDayView, so the
			 * EText item loses it and stops the edit. */
			gtk_widget_grab_focus (GTK_WIDGET (day_view));

			/* Stop the signal last or we will also stop any
			 * other events getting to the EText item. */
			g_signal_stop_emission_by_name (item, "event");
			return TRUE;
		} else if (event->key.keyval == GDK_KEY_Escape) {
			cancel_editing (day_view);
			g_signal_stop_emission_by_name (item, "event");
			/* focus should go to day view when stop editing */
			gtk_widget_grab_focus (GTK_WIDGET (day_view));
			return TRUE;
	       } else if ((event->key.keyval == GDK_KEY_Up)
			  && (event->key.state & GDK_SHIFT_MASK)
			  && (event->key.state & GDK_CONTROL_MASK)
			  && !(event->key.state & GDK_MOD1_MASK)) {
		       e_day_view_change_event_end_time_up (day_view);
		       return TRUE;
	       } else if ((event->key.keyval == GDK_KEY_Down)
			  && (event->key.state & GDK_SHIFT_MASK)
			  && (event->key.state & GDK_CONTROL_MASK)
			  && !(event->key.state & GDK_MOD1_MASK)) {
		       e_day_view_change_event_end_time_down (day_view);
		       return TRUE;
		}
		break;
	case GDK_2BUTTON_PRESS:
		break;

	case GDK_BUTTON_RELEASE:
		if (day_view->resize_event_num != -1)
			day_view->resize_event_num = -1;

		if (day_view->drag_event_num != -1)
			day_view->drag_event_num = -1;
		/* Only let the EText handle the event while editing. */
		if (!E_TEXT (item)->editing)
			g_signal_stop_emission_by_name (item, "event");
		break;

	case GDK_BUTTON_PRESS:
		/* Only let the EText handle the event while editing. */
		if (!E_TEXT (item)->editing)
			g_signal_stop_emission_by_name (item, "event");
		break;
	case GDK_FOCUS_CHANGE:
		if (event->focus_change.in)
			e_day_view_on_editing_started (day_view, item);
		else
			e_day_view_on_editing_stopped (day_view, item);

		return FALSE;
	case GDK_ENTER_NOTIFY:
		{
			EDayViewEvent *pevent;
			gint event_x, event_y, row, day, event_num;
			ECalendarViewPosition pos;
			gboolean main_canvas = TRUE;
			GdkWindow *window;
			GtkLayout *layout;

			if (day_view->editing_event_num != -1)
				break;

			if (day_view->resize_event_num != -1)
				break;

			if (day_view->drag_event_num != -1)
				break;

			/* Convert the coords to the main canvas window, or return if the
			 * window is not found. */
			layout = GTK_LAYOUT (day_view->main_canvas);
			window = gtk_layout_get_bin_window (layout);
			if (!e_day_view_convert_event_coords (
				day_view, (GdkEvent *) event,
				window, &event_x, &event_y)) {

				main_canvas = FALSE;

				layout = GTK_LAYOUT (day_view->top_canvas);
				window = gtk_layout_get_bin_window (layout);
				if (!e_day_view_convert_event_coords (
					day_view, (GdkEvent *) event,
					window, &event_x, &event_y))  {
					return FALSE;
				}
			}
			/* Find out where the mouse is. */
			if (main_canvas) {
				pos = e_day_view_convert_position_in_main_canvas (
					day_view,
					event_x, event_y,
					&day, &row,
					&event_num);
			} else {
				gint tmp;

				pos = e_day_view_convert_position_in_top_canvas (
					day_view,
					event_x, event_y,
					&tmp, &event_num);
				day = E_DAY_VIEW_LONG_EVENT;
			}

			if (pos == E_CALENDAR_VIEW_POS_OUTSIDE)
				break;

			/* even when returns position inside, or other, then the day and/or event_num
			 * can be unknown, thus check for this here, otherwise it will crash later */
			if (day == -1 || event_num == -1)
				break;

			pevent = e_day_view_get_event (day_view, day, event_num);
			if (!pevent)
				break;

			g_object_set_data (G_OBJECT (item), "event-num", GINT_TO_POINTER (event_num));
			g_object_set_data (G_OBJECT (item), "event-day", GINT_TO_POINTER (day));

			pevent->x = ((GdkEventCrossing *) event)->x_root;
			pevent->y = ((GdkEventCrossing *) event)->y_root;

		return TRUE;
		}
	case GDK_LEAVE_NOTIFY:
		return TRUE;
	case GDK_MOTION_NOTIFY:
		{
			EDayViewEvent *pevent;
			gint event_num, day;

			e_day_view_check_layout (day_view);

			event_num = GPOINTER_TO_INT (g_object_get_data ((GObject *) item, "event-num"));
			day = GPOINTER_TO_INT (g_object_get_data ((GObject *) item, "event-day"));

			pevent = e_day_view_get_event (day_view, day, event_num);
			if (!pevent)
				break;

			pevent->x = ((GdkEventMotion *) event)->x_root;
			pevent->y = ((GdkEventMotion *) event)->y_root;

			return TRUE;
		}
	default:
		break;
	}

	return FALSE;
}

static gboolean
e_day_view_event_move (ECalendarView *cal_view,
                       ECalViewMoveDirection direction)
{
	EDayViewEvent *event;
	EDayView *day_view;
	gint time_divisions;
	gint day, event_num, resize_start_row, resize_end_row;
	time_t start_dt, end_dt;
	ICalTime *start_time, *end_time;

	day_view = E_DAY_VIEW (cal_view);
	day = day_view->editing_event_day;
	event_num = day_view->editing_event_num;

	time_divisions = e_calendar_view_get_time_divisions (cal_view);

	if ((day == -1) || (day == E_DAY_VIEW_LONG_EVENT))
		return FALSE;

	if (!is_array_index_in_bounds (day_view->events[day], event_num))
		return FALSE;

	event = &g_array_index (day_view->events[day], EDayViewEvent,
				event_num);
	day_view->resize_event_day = day;
	day_view->resize_event_num = event_num;
	day_view->resize_bars_event_day = day;
	day_view->resize_bars_event_num = event_num;
	resize_start_row = event->start_minute / time_divisions;
	resize_end_row = (event->end_minute - 1) / time_divisions;
	if (resize_end_row < resize_start_row)
		resize_end_row = resize_start_row;

	switch (direction) {
	case E_CAL_VIEW_MOVE_UP:
		if (resize_start_row <= 0)
			return FALSE;
		resize_start_row--;
		resize_end_row--;
		start_dt = e_day_view_convert_grid_position_to_time (day_view, day, resize_start_row);
		end_dt = e_day_view_convert_grid_position_to_time (day_view, day, resize_end_row + 1);
		break;
	case E_CAL_VIEW_MOVE_DOWN:
		if (resize_end_row >= day_view->rows - 1)
			return FALSE;
		resize_start_row++;
		resize_end_row++;
		start_dt = e_day_view_convert_grid_position_to_time (day_view, day, resize_start_row);
		end_dt = e_day_view_convert_grid_position_to_time (day_view, day, resize_end_row + 1);
		break;
	case E_CAL_VIEW_MOVE_LEFT:
		if (day <= 0)
			return TRUE;
		start_dt = e_day_view_convert_grid_position_to_time (day_view, day, resize_start_row);
		end_dt = e_day_view_convert_grid_position_to_time (day_view, day, resize_end_row + 1);
		start_time = i_cal_time_new_from_timet_with_zone (start_dt, 0, NULL);
		end_time = i_cal_time_new_from_timet_with_zone (end_dt, 0, NULL);
		i_cal_time_adjust (start_time, -1, 0, 0, 0);
		i_cal_time_adjust (end_time, -1, 0, 0, 0);
		start_dt = i_cal_time_as_timet (start_time);
		end_dt = i_cal_time_as_timet (end_time);
		g_clear_object (&start_time);
		g_clear_object (&end_time);
		break;
	case E_CAL_VIEW_MOVE_RIGHT:
		if (day + 1 >= e_day_view_get_days_shown (day_view))
			return TRUE;
		start_dt = e_day_view_convert_grid_position_to_time (day_view, day, resize_start_row);
		end_dt = e_day_view_convert_grid_position_to_time (day_view, day, resize_end_row + 1);
		start_time = i_cal_time_new_from_timet_with_zone (start_dt, 0, NULL);
		end_time = i_cal_time_new_from_timet_with_zone (end_dt, 0, NULL);
		i_cal_time_adjust (start_time ,1,0,0,0);
		i_cal_time_adjust (end_time ,1,0,0,0);
		start_dt = i_cal_time_as_timet (start_time);
		end_dt = i_cal_time_as_timet (end_time);
		g_clear_object (&start_time);
		g_clear_object (&end_time);
		break;
	default:
		return FALSE;
	}

	e_day_view_change_event_time (day_view, start_dt, end_dt);
	e_day_view_ensure_rows_visible (day_view, resize_start_row, resize_end_row);

	return TRUE;
}

static void
e_day_view_change_event_time (EDayView *day_view,
                              time_t start_dt,
                              time_t end_dt)
{
	EDayViewEvent *event;
	gint day, event_num;
	ECalComponent *comp;
	ECalComponentDateTime *date;
	ICalTimezone *zone;
	ECalModel *model;
	ECalClient *client;
	ESourceRegistry *registry;
	ECalObjModType mod = E_CAL_OBJ_MOD_ALL;

	day = day_view->editing_event_day;
	event_num = day_view->editing_event_num;

	model = e_calendar_view_get_model (E_CALENDAR_VIEW (day_view));
	registry = e_cal_model_get_registry (model);

	if (!is_array_index_in_bounds (day_view->events[day], event_num))
		return;

	event = &g_array_index (day_view->events[day], EDayViewEvent,
				event_num);

	if (!is_comp_data_valid (event))
		return;

	client = event->comp_data->client;

	/* We use a temporary shallow copy of the ico since we don't want to
	 * change the original ico here. Otherwise we would not detect that
	 * the event's time had changed in the "update_event" callback. */
	comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (event->comp_data->icalcomp));

	if (e_cal_component_has_attendees (comp) &&
	    !itip_organizer_is_user (registry, comp, client)) {
		g_object_unref (comp);
		return;
	}

	zone = e_calendar_view_get_timezone (E_CALENDAR_VIEW (day_view));

	date = e_cal_component_datetime_new_take (
		i_cal_time_new_from_timet_with_zone (start_dt, FALSE, zone),
		zone ? g_strdup (i_cal_timezone_get_tzid (zone)) : NULL);
	cal_comp_set_dtstart_with_oldzone (client, comp, date);

	e_cal_component_datetime_take_value (date,
		i_cal_time_new_from_timet_with_zone (end_dt, FALSE, zone));
	cal_comp_set_dtend_with_oldzone (client, comp, date);

	e_cal_component_datetime_free (date);

	e_cal_component_commit_sequence (comp);

	g_clear_pointer (&day_view->last_edited_comp_string, g_free);

	day_view->last_edited_comp_string = e_cal_component_get_as_string (comp);

	day_view->resize_drag_pos = E_CALENDAR_VIEW_POS_NONE;

	if (e_cal_component_has_recurrences (comp)) {
		if (!e_cal_dialogs_recur_component (client, comp, &mod, NULL, FALSE)) {
			gtk_widget_queue_draw (day_view->top_canvas);
			goto out;
		}

		if (mod == E_CAL_OBJ_MOD_THIS) {
			e_cal_component_set_rdates (comp, NULL);
			e_cal_component_set_rrules (comp, NULL);
			e_cal_component_set_exdates (comp, NULL);
			e_cal_component_set_exrules (comp, NULL);
		}
	} else if (e_cal_component_is_instance (comp))
		mod = E_CAL_OBJ_MOD_THIS;

	e_cal_component_commit_sequence (comp);

	e_cal_ops_modify_component (e_cal_model_get_data_model (model), client, e_cal_component_get_icalcomponent (comp),
		mod, E_CAL_OPS_SEND_FLAG_ASK | E_CAL_OPS_SEND_FLAG_IS_NEW_COMPONENT);

out:
	g_object_unref (comp);
}

static void
e_day_view_change_event_end_time_up (EDayView *day_view)
{
	EDayViewEvent *event;
	ECalendarView *cal_view;
	gint time_divisions;
	gint day, event_num, resize_start_row, resize_end_row;

	if (!e_calendar_view_get_allow_event_dnd (E_CALENDAR_VIEW (day_view)))
		return;

	day = day_view->editing_event_day;
	event_num = day_view->editing_event_num;

	if ((day == -1) || (day == E_DAY_VIEW_LONG_EVENT))
		return;

	if (!is_array_index_in_bounds (day_view->events[day], event_num))
		return;

	cal_view = E_CALENDAR_VIEW (day_view);
	time_divisions = e_calendar_view_get_time_divisions (cal_view);

	event = &g_array_index (day_view->events[day], EDayViewEvent,
				event_num);
	day_view->resize_event_day = day;
	day_view->resize_event_num = event_num;
	day_view->resize_bars_event_day = day;
	day_view->resize_bars_event_num = event_num;
	resize_start_row = event->start_minute / time_divisions;
	resize_end_row = (event->end_minute - 1) / time_divisions;
	if (resize_end_row < resize_start_row)
		resize_end_row = resize_start_row;
	if (resize_end_row == resize_start_row)
		return;
	day_view->resize_drag_pos = E_CALENDAR_VIEW_POS_BOTTOM_EDGE;
	resize_end_row--;
	day_view->resize_start_row = resize_start_row;
	day_view->resize_end_row = resize_end_row;
	e_day_view_finish_resize (day_view);
	e_day_view_ensure_rows_visible (day_view, resize_start_row, resize_end_row);
}

static void
e_day_view_change_event_end_time_down (EDayView *day_view)
{
	EDayViewEvent *event;
	ECalendarView *cal_view;
	gint time_divisions;
	gint day, event_num, resize_start_row, resize_end_row;

	cal_view = E_CALENDAR_VIEW (day_view);
	if (!e_calendar_view_get_allow_event_dnd (cal_view))
		return;
	time_divisions = e_calendar_view_get_time_divisions (cal_view);

	day = day_view->editing_event_day;
	event_num = day_view->editing_event_num;
	if ((day == -1) || (day == E_DAY_VIEW_LONG_EVENT))
		return;

	if (!is_array_index_in_bounds (day_view->events[day], event_num))
		return;

	event = &g_array_index (day_view->events[day], EDayViewEvent,
				event_num);
	day_view->resize_event_day = day;
	day_view->resize_event_num = event_num;
	day_view->resize_bars_event_day = day;
	day_view->resize_bars_event_num = event_num;
	resize_start_row = event->start_minute / time_divisions;
	resize_end_row = (event->end_minute - 1) / time_divisions;
	if (resize_end_row < resize_start_row)
		resize_end_row = resize_start_row;
	if (resize_end_row == day_view->rows -1)
		return;
	day_view->resize_drag_pos = E_CALENDAR_VIEW_POS_BOTTOM_EDGE;
	resize_end_row++;
	day_view->resize_start_row = resize_start_row;
	day_view->resize_end_row = resize_end_row;
	e_day_view_finish_resize (day_view);
	e_day_view_ensure_rows_visible (day_view, resize_start_row, resize_end_row);
}

static void
e_day_view_on_editing_started (EDayView *day_view,
                               GnomeCanvasItem *item)
{
	GtkAllocation allocation;
	gint day, event_num;

	if (!e_day_view_find_event_from_item (day_view, item,
					      &day, &event_num))
		return;

	/* FIXME: This is a temporary workaround for a bug which seems to stop
	 * us getting focus_out signals. It is not a complete fix since if we
	 * don't get focus_out signals we don't save the appointment text so
	 * this may be lost. */
	if (day_view->editing_event_day == day
	    && day_view->editing_event_num == event_num)
		return;

	day_view->editing_event_day = day;
	day_view->editing_event_num = event_num;

	gtk_widget_get_allocation (day_view->top_canvas, &allocation);

	if (day == E_DAY_VIEW_LONG_EVENT) {
		gint item_x, item_y, item_w, item_h, scroll_y;
		gint start_day, end_day;

		e_day_view_reshape_long_event (day_view, event_num);

		if (e_day_view_get_long_event_position (day_view, event_num,
						       &start_day, &end_day,
						       &item_x, &item_y,
						       &item_w, &item_h)) {
			GtkAdjustment *adjustment;
			GtkScrollable *scrollable;

			scrollable = GTK_SCROLLABLE (day_view->top_canvas);
			adjustment = gtk_scrollable_get_vadjustment (scrollable);

			/* and ensure it's visible too */
			/*item_y = (event_num * (day_view->top_row_height + 1)) - 1;*/
			scroll_y = gtk_adjustment_get_value (adjustment);
			if (item_y + day_view->top_row_height > allocation.height + scroll_y || item_y < scroll_y)
				gnome_canvas_scroll_to (GNOME_CANVAS (day_view->top_canvas), 0, item_y);
		}
	} else {
		day_view->resize_bars_event_day = day;
		day_view->resize_bars_event_num = event_num;
		e_day_view_update_event_label (day_view, day, event_num);
		e_day_view_reshape_main_canvas_resize_bars (day_view);
	}

	g_signal_emit_by_name (day_view, "selection_changed");

	g_object_notify (G_OBJECT (day_view), "is-editing");
}

static void
e_day_view_on_editing_stopped (EDayView *day_view,
                               GnomeCanvasItem *item)
{
	gint day, event_num;
	EDayViewEvent *event;
	gchar *text = NULL;
	ECalComponentText *summary = NULL;
	ECalComponent *comp;
	ECalClient *client;
	gboolean on_server;

	/* Note: the item we are passed here isn't reliable, so we just stop
	 * the edit of whatever item was being edited. We also receive this
	 * event twice for some reason. */
	day = day_view->editing_event_day;
	event_num = day_view->editing_event_num;

	/* If no item is being edited, just return. */
	if (day == -1)
		return;

	event = e_day_view_get_event (day_view, day, event_num);

	if (!is_comp_data_valid (event))
		return;

	/* Reset the edit fields. */
	day_view->editing_event_day = -1;
	day_view->editing_event_num = -1;

	day_view->resize_bars_event_day = -1;
	day_view->resize_bars_event_num = -1;

	g_object_set (event->canvas_item, "handle_popup", FALSE, NULL);
	g_object_get (
		event->canvas_item,
		"text", &text,
		NULL);
	g_return_if_fail (text != NULL);

	comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (event->comp_data->icalcomp));
	if (!comp) {
		g_free (text);
		return;
	}

	client = event->comp_data->client;
	on_server = !event->comp_data->is_new_component;

	if (e_str_is_empty (text) && !on_server) {
		const gchar *uid;

		uid = e_cal_component_get_uid (comp);

		e_day_view_foreach_event_with_uid (day_view, uid,
						   e_day_view_remove_event_cb, NULL);
		e_day_view_check_layout (day_view);
		gtk_widget_queue_draw (day_view->top_canvas);
		gtk_widget_queue_draw (day_view->main_canvas);
		goto out;
	}

	/* Only update the summary if necessary. */
	summary = e_cal_component_get_summary (comp);
	if (summary && !g_strcmp0 (text, e_cal_component_text_get_value (summary))) {
		if (day == E_DAY_VIEW_LONG_EVENT)
			e_day_view_reshape_long_event (day_view, event_num);
		else
			e_day_view_update_event_label (
				day_view, day,
				event_num);
	} else if ((summary && e_cal_component_text_get_value (summary)) || !e_str_is_empty (text)) {
		ICalComponent *icomp = e_cal_component_get_icalcomponent (comp);

		if (summary)
			e_cal_component_text_free (summary);
		summary = e_cal_component_text_new (text, NULL);
		e_cal_component_set_summary (comp, summary);
		e_cal_component_commit_sequence (comp);

		if (!on_server) {
			e_cal_ops_create_component (e_calendar_view_get_model (E_CALENDAR_VIEW (day_view)), client, icomp,
				e_calendar_view_component_created_cb, g_object_ref (day_view), g_object_unref);

			/* we remove the object since we either got the update from the server or failed */
			e_day_view_remove_event_cb (day_view, day, event_num, NULL);
		} else {
			ECalObjModType mod = E_CAL_OBJ_MOD_ALL;

			if (e_cal_component_has_recurrences (comp)) {
				if (!e_cal_dialogs_recur_component (client, comp, &mod, NULL, FALSE)) {
					goto out;
				}

				if (mod == E_CAL_OBJ_MOD_THIS) {
					ECalComponentDateTime *olddt, *dt;

					olddt = e_cal_component_get_dtstart (comp);

					if (olddt && e_cal_component_datetime_get_value (olddt) &&
					    i_cal_time_get_timezone (e_cal_component_datetime_get_value (olddt))) {
						ICalTime *itt;

						itt = e_cal_component_datetime_get_value (olddt);

						dt = e_cal_component_datetime_new_take (
							i_cal_time_new_from_timet_with_zone (event->comp_data->instance_start,
								i_cal_time_is_date (itt), i_cal_time_get_timezone (itt)),
							g_strdup (e_cal_component_datetime_get_tzid (olddt)));
					} else {
						ICalTime *itt;
						ICalTimezone *zone;

						zone = e_calendar_view_get_timezone (E_CALENDAR_VIEW (day_view));
						itt = olddt ? e_cal_component_datetime_get_value (olddt) : NULL;

						dt = e_cal_component_datetime_new_take (
							i_cal_time_new_from_timet_with_zone (event->comp_data->instance_start,
								itt ? i_cal_time_is_date (itt) : FALSE, zone),
							zone ? g_strdup (i_cal_timezone_get_tzid (zone)) : NULL);
					}
					e_cal_component_set_dtstart (comp, dt);

					e_cal_component_datetime_free (olddt);
					e_cal_component_datetime_free (dt);

					olddt = e_cal_component_get_dtend (comp);

					if (olddt && e_cal_component_datetime_get_value (olddt) &&
					    i_cal_time_get_timezone (e_cal_component_datetime_get_value (olddt))) {
						ICalTime *itt;

						itt = e_cal_component_datetime_get_value (olddt);

						dt = e_cal_component_datetime_new_take (
							i_cal_time_new_from_timet_with_zone (event->comp_data->instance_end,
								i_cal_time_is_date (itt), i_cal_time_get_timezone (itt)),
							g_strdup (e_cal_component_datetime_get_tzid (olddt)));
					} else {
						ICalTime *itt;
						ICalTimezone *zone;

						zone = e_calendar_view_get_timezone (E_CALENDAR_VIEW (day_view));
						itt = olddt ? e_cal_component_datetime_get_value (olddt) : NULL;

						dt = e_cal_component_datetime_new_take (
							i_cal_time_new_from_timet_with_zone (event->comp_data->instance_end,
								itt ? i_cal_time_is_date (itt) : FALSE, zone),
							zone ? g_strdup (i_cal_timezone_get_tzid (zone)) : NULL);
					}
					e_cal_component_set_dtend (comp, dt);

					e_cal_component_datetime_free (olddt);
					e_cal_component_datetime_free (dt);

					e_cal_component_set_rdates (comp, NULL);
					e_cal_component_set_rrules (comp, NULL);
					e_cal_component_set_exdates (comp, NULL);
					e_cal_component_set_exrules (comp, NULL);

					e_cal_component_commit_sequence (comp);
				}
			} else if (e_cal_component_is_instance (comp))
				mod = E_CAL_OBJ_MOD_THIS;

			e_cal_ops_modify_component (e_cal_model_get_data_model (e_calendar_view_get_model (E_CALENDAR_VIEW (day_view))),
				client, e_cal_component_get_icalcomponent (comp), mod, E_CAL_OPS_SEND_FLAG_ASK);
		}

	}
	gtk_widget_queue_draw (day_view->main_canvas);

 out:

	e_cal_component_text_free (summary);
	g_object_unref (comp);
	g_free (text);

	g_signal_emit_by_name (day_view, "selection_changed");

	g_object_notify (G_OBJECT (day_view), "is-editing");
}

/* FIXME: It is possible that we may produce an invalid time due to daylight
 * saving times (i.e. when clocks go forward there is a range of time which
 * is not valid). I don't know the best way to handle daylight saving time. */
static time_t
e_day_view_convert_grid_position_to_time (EDayView *day_view,
                                          gint col,
                                          gint row)
{
	ECalendarView *cal_view;
	gint time_divisions;
	ICalTime *tt;
	time_t val;
	gint minutes;

	cal_view = E_CALENDAR_VIEW (day_view);
	time_divisions = e_calendar_view_get_time_divisions (cal_view);

	/* Calulate the number of minutes since the start of the day. */
	minutes = day_view->first_hour_shown * 60
		+ day_view->first_minute_shown
		+ row * time_divisions;

	/* A special case for midnight, where we can use the start of the
	 * next day. */
	if (minutes == 60 * 24)
		return day_view->day_starts[col + 1];

	/* Create an ICalTime and convert to a time_t. */
	tt = i_cal_time_new_from_timet_with_zone (
		day_view->day_starts[col],
		FALSE, e_calendar_view_get_timezone (E_CALENDAR_VIEW (day_view)));
	i_cal_time_set_hour (tt, minutes / 60);
	i_cal_time_set_minute (tt, minutes % 60);
	i_cal_time_set_second (tt, 0);

	val = i_cal_time_as_timet_with_zone (tt, e_calendar_view_get_timezone (E_CALENDAR_VIEW (day_view)));

	g_clear_object (&tt);

	return val;
}

static gboolean
e_day_view_convert_time_to_grid_position (EDayView *day_view,
                                          time_t time,
                                          gint *col,
                                          gint *row)
{
	ECalendarView *cal_view;
	ICalTime *tt;
	gint time_divisions;
	gint day, minutes;
	gint days_shown;

	*col = *row = 0;

	cal_view = E_CALENDAR_VIEW (day_view);
	time_divisions = e_calendar_view_get_time_divisions (cal_view);

	if (time < day_view->lower || time >= day_view->upper)
		return FALSE;

	days_shown = e_day_view_get_days_shown (day_view);

	/* We can find the column easily using the day_starts array. */
	for (day = 1; day <= days_shown; day++) {
		if (time < day_view->day_starts[day]) {
			*col = day - 1;
			break;
		}
	}

	/* To find the row we need to convert the time to an ICalTime,
	 * calculate the offset in minutes from the top of the display and
	 * divide it by the mins per row setting. */
	tt = i_cal_time_new_from_timet_with_zone (time, FALSE, e_calendar_view_get_timezone (E_CALENDAR_VIEW (day_view)));

	minutes = i_cal_time_get_hour (tt) * 60 + i_cal_time_get_minute (tt);
	minutes -= day_view->first_hour_shown * 60 + day_view->first_minute_shown;

	g_clear_object (&tt);

	*row = minutes / time_divisions;

	if (*row < 0 || *row >= day_view->rows)
		return FALSE;

	return TRUE;
}

/* This starts or stops auto-scrolling when dragging a selection or resizing
 * an event. */
void
e_day_view_check_auto_scroll (EDayView *day_view,
                              gint event_x,
                              gint event_y)
{
	GtkAllocation allocation;
	gint scroll_x, scroll_y;

	gnome_canvas_get_scroll_offsets (
		GNOME_CANVAS (day_view->main_canvas),
		&scroll_x, &scroll_y);

	event_x -= scroll_x;
	event_y -= scroll_y;

	day_view->last_mouse_x = event_x;
	day_view->last_mouse_y = event_y;

	gtk_widget_get_allocation (day_view->main_canvas, &allocation);

	if (event_y < E_DAY_VIEW_AUTO_SCROLL_OFFSET)
		e_day_view_start_auto_scroll (day_view, TRUE);
	else if (event_y >= allocation.height - E_DAY_VIEW_AUTO_SCROLL_OFFSET)
		e_day_view_start_auto_scroll (day_view, FALSE);
	else
		e_day_view_stop_auto_scroll (day_view);
}

static void
e_day_view_start_auto_scroll (EDayView *day_view,
                              gboolean scroll_up)
{
	if (day_view->auto_scroll_timeout_id == 0) {
		day_view->auto_scroll_timeout_id = e_named_timeout_add (
			E_DAY_VIEW_AUTO_SCROLL_TIMEOUT,
			e_day_view_auto_scroll_handler, day_view);
		day_view->auto_scroll_delay = E_DAY_VIEW_AUTO_SCROLL_DELAY;
	}
	day_view->auto_scroll_up = scroll_up;
}

void
e_day_view_stop_auto_scroll (EDayView *day_view)
{
	if (day_view->auto_scroll_timeout_id != 0) {
		g_source_remove (day_view->auto_scroll_timeout_id);
		day_view->auto_scroll_timeout_id = 0;
	}
}

static gboolean
e_day_view_auto_scroll_handler (gpointer data)
{
	EDayView *day_view;
	ECalendarViewPosition pos;
	gint scroll_x, scroll_y, new_scroll_y, canvas_x, canvas_y, row, day;
	GtkAdjustment *adjustment;
	GtkScrollable *scrollable;
	gdouble step_increment;
	gdouble page_size;
	gdouble upper;

	g_return_val_if_fail (E_IS_DAY_VIEW (data), FALSE);

	day_view = E_DAY_VIEW (data);

	if (day_view->auto_scroll_delay > 0) {
		day_view->auto_scroll_delay--;
		return TRUE;
	}

	gnome_canvas_get_scroll_offsets (
		GNOME_CANVAS (day_view->main_canvas),
		&scroll_x, &scroll_y);

	scrollable = GTK_SCROLLABLE (day_view->main_canvas);
	adjustment = gtk_scrollable_get_vadjustment (scrollable);

	step_increment = gtk_adjustment_get_step_increment (adjustment);
	page_size = gtk_adjustment_get_page_size (adjustment);
	upper = gtk_adjustment_get_upper (adjustment);

	if (day_view->auto_scroll_up)
		new_scroll_y = MAX (scroll_y - step_increment, 0);
	else
		new_scroll_y = MIN (
			scroll_y + step_increment,
			upper - page_size);

	if (new_scroll_y != scroll_y) {
		/* NOTE: This reduces flicker, but only works if we don't use
		 * canvas items which have X windows. */
		gnome_canvas_scroll_to (
			GNOME_CANVAS (day_view->main_canvas),
			scroll_x, new_scroll_y);
	}

	canvas_x = day_view->last_mouse_x + scroll_x;
	canvas_y = day_view->last_mouse_y + new_scroll_y;

	/* The last_mouse_x position is set to -1 when we are selecting using
	 * the time column. In this case we set canvas_x to 0 and we ignore
	 * the resulting day. */
	if (day_view->last_mouse_x == -1)
		canvas_x = 0;

	/* Update the selection/resize/drag if necessary. */
	pos = e_day_view_convert_position_in_main_canvas (
		day_view,
		canvas_x, canvas_y,
		&day, &row, NULL);

	if (day_view->last_mouse_x == -1)
		day = -1;

	if (pos != E_CALENDAR_VIEW_POS_OUTSIDE) {
		if (day_view->selection_is_being_dragged) {
			e_day_view_update_selection (day_view, day, row);
		} else if (day_view->resize_drag_pos != E_CALENDAR_VIEW_POS_NONE) {
			e_day_view_update_resize (day_view, row);
		} else if (day_view->drag_item->flags & GNOME_CANVAS_ITEM_VISIBLE) {
			e_day_view_update_main_canvas_drag (day_view, row, day);
		}
	}

	return TRUE;
}

gboolean
e_day_view_get_event_rows (EDayView *day_view,
                           gint day,
                           gint event_num,
                           gint *start_row_out,
                           gint *end_row_out)
{
	ECalendarView *cal_view;
	EDayViewEvent *event;
	gint time_divisions;
	gint start_row, end_row;

	g_return_val_if_fail (day >= 0, FALSE);
	g_return_val_if_fail (day < E_DAY_VIEW_LONG_EVENT, FALSE);
	g_return_val_if_fail (event_num >= 0, FALSE);

	if (!is_array_index_in_bounds (day_view->events[day], event_num))
		return FALSE;

	cal_view = E_CALENDAR_VIEW (day_view);
	time_divisions = e_calendar_view_get_time_divisions (cal_view);

	event = &g_array_index (day_view->events[day], EDayViewEvent,
				event_num);
	start_row = event->start_minute / time_divisions;
	end_row = (event->end_minute - 1) / time_divisions;
	if (end_row < start_row)
		end_row = start_row;

	*start_row_out = start_row;
	*end_row_out = end_row;
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

	if (!is_array_index_in_bounds (day_view->events[day], event_num))
		return FALSE;

	event = &g_array_index (day_view->events[day], EDayViewEvent,
				event_num);

	/* If the event is flagged as not displayed, return FALSE. */
	if (event->num_columns == 0)
		return FALSE;

	e_day_view_get_event_rows (day_view, day, event_num, &start_row, &end_row);

	cols_in_row = day_view->cols_per_row[day][start_row];
	start_col = event->start_row_or_col;
	num_columns = event->num_columns;

	if (cols_in_row == 0)
		return FALSE;

	/* If the event is being resize, use the resize position. */
	if (day_view->resize_drag_pos != E_CALENDAR_VIEW_POS_NONE
	    && day_view->resize_event_day == day
	    && day_view->resize_event_num == event_num) {
		if (day_view->resize_drag_pos == E_CALENDAR_VIEW_POS_TOP_EDGE)
			start_row = day_view->resize_start_row;
		else if (day_view->resize_drag_pos == E_CALENDAR_VIEW_POS_BOTTOM_EDGE)
			end_row = day_view->resize_end_row;
	}

	*item_x = day_view->day_offsets[day]
		+ day_view->day_widths[day] * start_col / cols_in_row;
	*item_w = day_view->day_widths[day] * num_columns / cols_in_row
		- E_DAY_VIEW_GAP_WIDTH;
	*item_w = MAX (*item_w, 0);
	*item_y = start_row * day_view->row_height;

	/* This makes the event end on the grid line of the next row,
	 * which maybe looks nicer if you have 2 events on consecutive rows. */
	*item_h = (end_row - start_row + 1) * day_view->row_height + 1;

	return TRUE;
}

gboolean
e_day_view_get_long_event_position (EDayView *day_view,
                                    gint event_num,
                                    gint *start_day,
                                    gint *end_day,
                                    gint *item_x,
                                    gint *item_y,
                                    gint *item_w,
                                    gint *item_h)
{
	EDayViewEvent *event;
	gint days_shown;

	days_shown = e_day_view_get_days_shown (day_view);

	if (!is_array_index_in_bounds (day_view->long_events, event_num))
		return FALSE;

	event = &g_array_index (day_view->long_events, EDayViewEvent,
				event_num);

	/* If the event is flagged as not displayed, return FALSE. */
	if (event->num_columns == 0)
		return FALSE;

	if (!e_day_view_find_long_event_days (event,
					      days_shown,
					      day_view->day_starts,
					      start_day, end_day))
		return FALSE;

	/* If the event is being resize, use the resize position. */
	if (day_view->resize_drag_pos != E_CALENDAR_VIEW_POS_NONE
	    && day_view->resize_event_day == E_DAY_VIEW_LONG_EVENT
	    && day_view->resize_event_num == event_num) {
		if (day_view->resize_drag_pos == E_CALENDAR_VIEW_POS_LEFT_EDGE)
			*start_day = day_view->resize_start_row;
		else if (day_view->resize_drag_pos == E_CALENDAR_VIEW_POS_RIGHT_EDGE)
			*end_day = day_view->resize_end_row;
	}

	*item_x = day_view->day_offsets[*start_day] + E_DAY_VIEW_BAR_WIDTH;
	if (days_shown == 1) {
		GtkAllocation allocation;

		gtk_widget_get_allocation (day_view->top_canvas, &allocation);
		*item_w = allocation.width;
	} else
		*item_w = day_view->day_offsets[*end_day + 1];
	*item_w = MAX (*item_w - *item_x - E_DAY_VIEW_GAP_WIDTH, 0);
	*item_y = (event->start_row_or_col) * day_view->top_row_height;
	*item_h = day_view->top_row_height - E_DAY_VIEW_TOP_CANVAS_Y_GAP;
	return TRUE;
}

/* Converts a position within the entire top canvas to a day & event and
 * a place within the event if appropriate. If event_num_return is NULL, it
 * simply returns the grid position without trying to find the event. */
static ECalendarViewPosition
e_day_view_convert_position_in_top_canvas (EDayView *day_view,
                                           gint x,
                                           gint y,
                                           gint *day_return,
                                           gint *event_num_return)
{
	EDayViewEvent *event;
	gint day, row, col;
	gint event_num, start_day, end_day, item_x, item_y, item_w, item_h;
	gint days_shown;

	days_shown = e_day_view_get_days_shown (day_view);

	*day_return = -1;
	if (event_num_return)
		*event_num_return = -1;

	if (x < 0 || y < 0)
		return E_CALENDAR_VIEW_POS_OUTSIDE;

	row = y / day_view->top_row_height;

	day = -1;
	for (col = 1; col <= days_shown; col++) {
		if (x < day_view->day_offsets[col]) {
			day = col - 1;
			break;
		}
	}
	if (day == -1)
		return E_CALENDAR_VIEW_POS_OUTSIDE;

	*day_return = day;

	/* If only the grid position is wanted, return. */
	if (event_num_return == NULL)
		return E_CALENDAR_VIEW_POS_NONE;

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
			return E_CALENDAR_VIEW_POS_LEFT_EDGE;

		if (x >= item_x + item_w - E_DAY_VIEW_LONG_EVENT_BORDER_WIDTH
		    - E_DAY_VIEW_LONG_EVENT_X_PAD)
			return E_CALENDAR_VIEW_POS_RIGHT_EDGE;

		return E_CALENDAR_VIEW_POS_EVENT;
	}

	return E_CALENDAR_VIEW_POS_NONE;
}

/* Converts a position within the entire main canvas to a day, row, event and
 * a place within the event if appropriate. If event_num_return is NULL, it
 * simply returns the grid position without trying to find the event. */
static ECalendarViewPosition
e_day_view_convert_position_in_main_canvas (EDayView *day_view,
                                            gint x,
                                            gint y,
                                            gint *day_return,
                                            gint *row_return,
                                            gint *event_num_return)
{
	gint day, row, col, event_num;
	gint item_x, item_y, item_w, item_h;
	gint days_shown;

	days_shown = e_day_view_get_days_shown (day_view);

	*day_return = -1;
	*row_return = -1;
	if (event_num_return)
		*event_num_return = -1;

	/* Check the position is inside the canvas, and determine the day
	 * and row. */
	if (x < 0 || y < 0)
		return E_CALENDAR_VIEW_POS_OUTSIDE;

	row = y / day_view->row_height;
	if (row >= day_view->rows)
		return E_CALENDAR_VIEW_POS_OUTSIDE;

	day = -1;
	for (col = 1; col <= days_shown; col++) {
		if (x < day_view->day_offsets[col]) {
			day = col - 1;
			break;
		}
	}
	if (day == -1)
		return E_CALENDAR_VIEW_POS_OUTSIDE;

	*day_return = day;
	*row_return = row;

	/* If only the grid position is wanted, return. */
	if (event_num_return == NULL)
		return E_CALENDAR_VIEW_POS_NONE;

	/* Check the selected item first, since the horizontal resizing bars
	 * may be above other events. */
	if (day_view->resize_bars_event_day == day) {
		if (e_day_view_get_event_position (day_view, day,
						   day_view->resize_bars_event_num,
						   &item_x, &item_y,
						   &item_w, &item_h)) {
			if (x >= item_x && x < item_x + item_w) {
				*event_num_return = day_view->resize_bars_event_num;
				if (y >= item_y - E_DAY_VIEW_BAR_HEIGHT
				    && y < item_y + E_DAY_VIEW_EVENT_BORDER_HEIGHT)
					return E_CALENDAR_VIEW_POS_TOP_EDGE;
				if (y >= item_y + item_h - E_DAY_VIEW_EVENT_BORDER_HEIGHT
				    && y < item_y + item_h + E_DAY_VIEW_BAR_HEIGHT)
					return E_CALENDAR_VIEW_POS_BOTTOM_EDGE;
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
			return E_CALENDAR_VIEW_POS_LEFT_EDGE;

		if (y < item_y + E_DAY_VIEW_EVENT_BORDER_HEIGHT
		    + E_DAY_VIEW_EVENT_Y_PAD)
			return E_CALENDAR_VIEW_POS_TOP_EDGE;

		if (y >= item_y + item_h - E_DAY_VIEW_EVENT_BORDER_HEIGHT
		    - E_DAY_VIEW_EVENT_Y_PAD)
			return E_CALENDAR_VIEW_POS_BOTTOM_EDGE;

		return E_CALENDAR_VIEW_POS_EVENT;
	}

	return E_CALENDAR_VIEW_POS_NONE;
}

static void
e_day_view_maybe_update_drag_status (EDayView *day_view,
				     GdkDragContext *context,
				     guint time)
{
	if (!day_view->priv->drag_event_is_recurring &&
	    day_view->drag_event_day != -1 &&
	    day_view->drag_event_num != -1) {
		GdkDragAction drag_action = GDK_ACTION_MOVE;
		GdkModifierType mask;

		gdk_window_get_pointer (gtk_widget_get_window (GTK_WIDGET (day_view)), NULL, NULL, &mask);

		if ((mask & GDK_CONTROL_MASK) != 0)
			drag_action = GDK_ACTION_COPY;

		gdk_drag_status (context, drag_action, time);
	}
}

static gboolean
e_day_view_on_top_canvas_drag_motion (GtkWidget *widget,
                                      GdkDragContext *context,
                                      gint x,
                                      gint y,
                                      guint time,
                                      EDayView *day_view)
{
	gint scroll_x, scroll_y;

	gnome_canvas_get_scroll_offsets (
		GNOME_CANVAS (widget),
		&scroll_x, &scroll_y);
	day_view->drag_event_x = x + scroll_x;
	day_view->drag_event_y = y + scroll_y;

	e_day_view_reshape_top_canvas_drag_item (day_view);
	e_day_view_maybe_update_drag_status (day_view, context, time);

	return TRUE;
}

static void
e_day_view_reshape_top_canvas_drag_item (EDayView *day_view)
{
	ECalendarViewPosition pos;
	gint x, y, day;

	/* Calculate the day & start row of the event being dragged, using
	 * the current mouse position. */
	x = day_view->drag_event_x;
	y = day_view->drag_event_y;
	pos = e_day_view_convert_position_in_top_canvas (
		day_view, x, y,
		&day, NULL);
	/* This shouldn't really happen in a drag. */
	if (pos == E_CALENDAR_VIEW_POS_OUTSIDE)
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
	gint days_shown;
	gchar *text;

	days_shown = e_day_view_get_days_shown (day_view);

	/* Calculate the event's position. If the event is in the same
	 * position we started in, we use the same columns. */
	row = day_view->rows_in_top_display + 1;
	num_days = 1;

	if (day_view->drag_event_day == E_DAY_VIEW_LONG_EVENT) {
		if (!is_array_index_in_bounds (day_view->long_events, day_view->drag_event_num))
			return;

		event = &g_array_index (day_view->long_events, EDayViewEvent,
					day_view->drag_event_num);
		row = event->start_row_or_col + 1;

		if (!e_day_view_find_long_event_days (event,
						      days_shown,
						      day_view->day_starts,
						      &start_day, &end_day))
			return;

		num_days = end_day - start_day + 1;

		/* Make sure we don't go off the screen. */
		day = MIN (day, days_shown - num_days);

	} else if (day_view->drag_event_day != -1) {
		if (!is_array_index_in_bounds (day_view->events[day_view->drag_event_day], day_view->drag_event_num))
			return;

		event = &g_array_index (day_view->events[day_view->drag_event_day],
					EDayViewEvent,
					day_view->drag_event_num);
	}

	/* If the position hasn't changed, just return. */
	if (day_view->drag_last_day == day
	    && (day_view->drag_long_event_item->flags & GNOME_CANVAS_ITEM_VISIBLE))
		return;

	day_view->drag_last_day = day;

	item_x = day_view->day_offsets[day] + E_DAY_VIEW_BAR_WIDTH;
	item_w = day_view->day_offsets[day + num_days] - item_x
		- E_DAY_VIEW_GAP_WIDTH;
	item_y = row * day_view->top_row_height;
	item_h = day_view->top_row_height - E_DAY_VIEW_TOP_CANVAS_Y_GAP;

	/* Set the positions of the event & associated items. */
	gnome_canvas_item_set (
		day_view->drag_long_event_rect_item,
		"x1", item_x,
		"y1", item_y,
		"x2", item_x + item_w - 1,
		"y2", item_y + item_h - 1,
		NULL);

	gnome_canvas_item_set (
		day_view->drag_long_event_item,
		"clip_width", item_w - (E_DAY_VIEW_LONG_EVENT_BORDER_WIDTH + E_DAY_VIEW_LONG_EVENT_X_PAD) * 2,
		"clip_height", item_h - (E_DAY_VIEW_LONG_EVENT_BORDER_HEIGHT + E_DAY_VIEW_LONG_EVENT_Y_PAD) * 2,
		NULL);
	e_canvas_item_move_absolute (
		day_view->drag_long_event_item,
		item_x + E_DAY_VIEW_LONG_EVENT_BORDER_WIDTH + E_DAY_VIEW_LONG_EVENT_X_PAD,
		item_y + E_DAY_VIEW_LONG_EVENT_BORDER_HEIGHT + E_DAY_VIEW_LONG_EVENT_Y_PAD);

	if (!(day_view->drag_long_event_rect_item->flags & GNOME_CANVAS_ITEM_VISIBLE)) {
		gnome_canvas_item_raise_to_top (day_view->drag_long_event_rect_item);
		gnome_canvas_item_show (day_view->drag_long_event_rect_item);
	}

	/* Set the text, if necessary. We don't want to set the text every
	 * time it moves, so we check if it is currently invisible and only
	 * set the text then. */
	if (!(day_view->drag_long_event_item->flags & GNOME_CANVAS_ITEM_VISIBLE)) {
		const gchar *summary;

		if (event && is_comp_data_valid (event)) {
			summary = i_cal_component_get_summary (event->comp_data->icalcomp);
			text = g_strdup (summary);
		} else {
			text = NULL;
		}

		gnome_canvas_item_set (
			day_view->drag_long_event_item,
			"text", text ? text : "",
			NULL);
		gnome_canvas_item_raise_to_top (day_view->drag_long_event_item);
		gnome_canvas_item_show (day_view->drag_long_event_item);

		g_free (text);
	}
}

static gboolean
e_day_view_on_main_canvas_drag_motion (GtkWidget *widget,
                                       GdkDragContext *context,
                                       gint x,
                                       gint y,
                                       guint time,
                                       EDayView *day_view)
{
	gint scroll_x, scroll_y;

	gnome_canvas_get_scroll_offsets (
		GNOME_CANVAS (widget),
		&scroll_x, &scroll_y);

	day_view->drag_event_x = x + scroll_x;
	day_view->drag_event_y = y + scroll_y;

	e_day_view_reshape_main_canvas_drag_item (day_view);
	e_day_view_reshape_main_canvas_resize_bars (day_view);

	e_day_view_check_auto_scroll (day_view, day_view->drag_event_x, day_view->drag_event_y);
	e_day_view_maybe_update_drag_status (day_view, context, time);

	return TRUE;
}

static void
e_day_view_reshape_main_canvas_drag_item (EDayView *day_view)
{
	ECalendarViewPosition pos;
	gint x, y, day, row;

	/* Calculate the day & start row of the event being dragged, using
	 * the current mouse position. */
	x = day_view->drag_event_x;
	y = day_view->drag_event_y;
	pos = e_day_view_convert_position_in_main_canvas (
		day_view, x, y,
		&day, &row, NULL);
	/* This shouldn't really happen in a drag. */
	if (pos == E_CALENDAR_VIEW_POS_OUTSIDE)
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
	ECalendarView *cal_view;
	gint time_divisions;
	gint cols_in_row, start_col, num_columns, num_rows, end_row;
	gdouble item_x, item_y, item_w, item_h;
	gchar *text;

	cal_view = E_CALENDAR_VIEW (day_view);
	time_divisions = e_calendar_view_get_time_divisions (cal_view);

	/* If the position hasn't changed, just return. */
	if (day_view->drag_last_day == day
	    && day_view->drag_last_row == row
	    && (day_view->drag_item->flags & GNOME_CANVAS_ITEM_VISIBLE))
		return;

	day_view->drag_last_day = day;
	day_view->drag_last_row = row;

	/* Calculate the event's position. If the event is in the same
	 * position we started in, we use the same columns. */
	cols_in_row = 1;
	start_col = 0;
	num_columns = 1;
	num_rows = 1;

	if (day_view->drag_event_day == E_DAY_VIEW_LONG_EVENT) {
		if (!is_array_index_in_bounds (day_view->long_events, day_view->drag_event_num))
			return;

		event = &g_array_index (day_view->long_events, EDayViewEvent,
					day_view->drag_event_num);
	} else if (day_view->drag_event_day != -1) {
		gint start_row;

		if (!is_array_index_in_bounds (day_view->events[day_view->drag_event_day], day_view->drag_event_num))
			return;

		event = &g_array_index (day_view->events[day_view->drag_event_day],
					EDayViewEvent,
					day_view->drag_event_num);
		start_row = event->start_minute / time_divisions;
		end_row = (event->end_minute - 1) / time_divisions;
		if (end_row < start_row)
			end_row = start_row;

		num_rows = end_row - start_row + 1;

		if (day_view->drag_event_day == day && start_row == row) {
			cols_in_row = day_view->cols_per_row[day][row];
			start_col = event->start_row_or_col;
			num_columns = event->num_columns;
		}
	}

	item_x = day_view->day_offsets[day]
		+ day_view->day_widths[day] * start_col / cols_in_row;
	item_w = day_view->day_widths[day] * num_columns / cols_in_row
		- E_DAY_VIEW_GAP_WIDTH;
	item_y = row * day_view->row_height;
	item_h = num_rows * day_view->row_height;

	/* Set the positions of the event & associated items. */
	gnome_canvas_item_set (
		day_view->drag_rect_item,
		"x1", item_x + E_DAY_VIEW_BAR_WIDTH - 1,
		"y1", item_y,
		"x2", item_x + item_w - 1,
		"y2", item_y + item_h - 1,
		NULL);

	gnome_canvas_item_set (
		day_view->drag_bar_item,
		"x1", item_x,
		"y1", item_y,
		"x2", item_x + E_DAY_VIEW_BAR_WIDTH - 1,
		"y2", item_y + item_h - 1,
		NULL);

	gnome_canvas_item_set (
		day_view->drag_item,
		"clip_width", item_w - E_DAY_VIEW_BAR_WIDTH - E_DAY_VIEW_EVENT_X_PAD * 2,
		"clip_height", item_h - (E_DAY_VIEW_EVENT_BORDER_HEIGHT + E_DAY_VIEW_EVENT_Y_PAD) * 2,
		NULL);
	e_canvas_item_move_absolute (
		day_view->drag_item,
		item_x + E_DAY_VIEW_BAR_WIDTH + E_DAY_VIEW_EVENT_X_PAD,
		item_y + E_DAY_VIEW_EVENT_BORDER_HEIGHT + E_DAY_VIEW_EVENT_Y_PAD);

	if (!(day_view->drag_bar_item->flags & GNOME_CANVAS_ITEM_VISIBLE)) {
		gnome_canvas_item_raise_to_top (day_view->drag_bar_item);
		gnome_canvas_item_show (day_view->drag_bar_item);
	}

	if (!(day_view->drag_rect_item->flags & GNOME_CANVAS_ITEM_VISIBLE)) {
		gnome_canvas_item_raise_to_top (day_view->drag_rect_item);
		gnome_canvas_item_show (day_view->drag_rect_item);
	}

	/* Set the text, if necessary. We don't want to set the text every
	 * time it moves, so we check if it is currently invisible and only
	 * set the text then. */
	if (!(day_view->drag_item->flags & GNOME_CANVAS_ITEM_VISIBLE)) {
		const gchar *summary;

		if (event && is_comp_data_valid (event)) {
			summary = i_cal_component_get_summary (event->comp_data->icalcomp);
			text = g_strdup (summary);
		} else {
			text = NULL;
		}

		gnome_canvas_item_set (
			day_view->drag_item,
			"text", text ? text : "",
			NULL);
		gnome_canvas_item_raise_to_top (day_view->drag_item);
		gnome_canvas_item_show (day_view->drag_item);

		g_free (text);
	}
}

static void
e_day_view_on_top_canvas_drag_leave (GtkWidget *widget,
                                     GdkDragContext *context,
                                     guint time,
                                     EDayView *day_view)
{
	day_view->drag_last_day = -1;

	gnome_canvas_item_hide (day_view->drag_long_event_rect_item);
	gnome_canvas_item_hide (day_view->drag_long_event_item);
}

static void
e_day_view_on_main_canvas_drag_leave (GtkWidget *widget,
                                      GdkDragContext *context,
                                      guint time,
                                      EDayView *day_view)
{
	day_view->drag_last_day = -1;

	e_day_view_stop_auto_scroll (day_view);

	gnome_canvas_item_hide (day_view->drag_rect_item);
	gnome_canvas_item_hide (day_view->drag_bar_item);
	gnome_canvas_item_hide (day_view->drag_item);

	/* Hide the resize bars if they are being used in the drag. */
	if (day_view->drag_event_day == day_view->resize_bars_event_day
	    && day_view->drag_event_num == day_view->resize_bars_event_num) {
	}
}

static void
e_day_view_on_drag_begin (GtkWidget *widget,
                          GdkDragContext *context,
                          EDayView *day_view)
{
	EDayViewEvent *event;
	gint day, event_num;

	day = day_view->drag_event_day;
	event_num = day_view->drag_event_num;

	/* These should both be set. */
	if (day == -1) {
		g_warn_if_reached ();
		return;
	}

	g_return_if_fail (event_num != -1);

	event = e_day_view_get_event (day_view, day, event_num);

	if (!event)
		return;

	/* Hide the text item, since it will be shown in the special drag
	 * items. */
	gnome_canvas_item_hide (event->canvas_item);
}

static void
e_day_view_on_drag_end (GtkWidget *widget,
                        GdkDragContext *context,
                        EDayView *day_view)
{
	EDayViewEvent *event;
	gint day, event_num;

	day = day_view->drag_event_day;
	event_num = day_view->drag_event_num;

	/* If the calendar has already been updated in drag_data_received()
	 * we just return. */
	if (day == -1 || event_num == -1)
		return;

	event = e_day_view_get_event (day_view, day, event_num);
	if (!event)
		return;

	if (day == E_DAY_VIEW_LONG_EVENT)
		gtk_widget_queue_draw (day_view->top_canvas);
	else
		gtk_widget_queue_draw (day_view->main_canvas);

	/* Show the text item again. */
	gnome_canvas_item_show (event->canvas_item);

	day_view->drag_event_day = -1;
	day_view->drag_event_num = -1;
	day_view->priv->drag_event_is_recurring = FALSE;
	g_clear_object (&day_view->priv->drag_context);
}

static void
e_day_view_on_drag_data_get (GtkWidget *widget,
                             GdkDragContext *context,
                             GtkSelectionData *selection_data,
                             guint info,
                             guint time,
                             EDayView *day_view)
{
	EDayViewEvent *event;
	ICalComponent *vcal;
	gint day, event_num;
	gchar *comp_str;

	day = day_view->drag_event_day;
	event_num = day_view->drag_event_num;

	/* These should both be set. */
	if (day == -1) {
		g_warn_if_reached ();
		return;
	}

	g_return_if_fail (event_num != -1);

	if (day == E_DAY_VIEW_LONG_EVENT) {
		if (!is_array_index_in_bounds (day_view->long_events, event_num))
			return;

		event = &g_array_index (day_view->long_events,
					EDayViewEvent, event_num);
	} else {
		if (!is_array_index_in_bounds (day_view->events[day], event_num))
			return;

		event = &g_array_index (day_view->events[day],
					EDayViewEvent, event_num);
	}

	if (!is_comp_data_valid (event))
		return;

	vcal = e_cal_util_new_top_level ();
	e_cal_util_add_timezones_from_component (
		vcal, event->comp_data->icalcomp);
	i_cal_component_take_component (
		vcal, i_cal_component_clone (event->comp_data->icalcomp));

	comp_str = i_cal_component_as_ical_string (vcal);
	if (comp_str) {
		ESource *source;
		const gchar *source_uid;
		GdkAtom target;
		gchar *tmp;

		source = e_client_get_source (E_CLIENT (event->comp_data->client));
		source_uid = e_source_get_uid (source);

		tmp = g_strconcat (source_uid, "\n", comp_str, NULL);
		target = gtk_selection_data_get_target (selection_data);
		gtk_selection_data_set (
			selection_data, target, 8,
			(guchar *) tmp, strlen (tmp));

		g_free (tmp);
	}

	g_clear_object (&vcal);
	g_free (comp_str);
}

static void
e_day_view_on_top_canvas_drag_data_received (GtkWidget *widget,
                                             GdkDragContext *context,
                                             gint x,
                                             gint y,
                                             GtkSelectionData *selection_data,
                                             guint info,
                                             guint time,
                                             EDayView *day_view)
{
	EDayViewEvent *event = NULL;
	ECalendarViewPosition pos;
	gint day, start_day, end_day, num_days;
	gint start_offset, end_offset;
	ECalComponent *comp;
	ESourceRegistry *registry;
	time_t dt;
	gboolean all_day_event;
	ECalModel *model;
	ECalendarView *cal_view;
	gboolean drag_from_same_window;
	const guchar *data;
	gint format, length;
	gint days_shown;
	GtkResponseType send = GTK_RESPONSE_NO;
	gboolean only_new_attendees = FALSE;
	gboolean strip_alarms = TRUE;

	data = gtk_selection_data_get_data (selection_data);
	format = gtk_selection_data_get_format (selection_data);
	length = gtk_selection_data_get_length (selection_data);

	days_shown = e_day_view_get_days_shown (day_view);

	if (day_view->drag_event_day != -1)
		drag_from_same_window = TRUE;
	else
		drag_from_same_window = FALSE;

	cal_view = E_CALENDAR_VIEW (day_view);
	model = e_calendar_view_get_model (cal_view);

	registry = e_cal_model_get_registry (model);

	/* Note that we only support DnD within the EDayView at present. */
	if (length >= 0 && format == 8 && day_view->drag_event_day != -1) {
		/* We are dragging in the same window */

		pos = e_day_view_convert_position_in_top_canvas (
			day_view,
			x, y, &day,
			NULL);
		if (pos != E_CALENDAR_VIEW_POS_OUTSIDE) {
			ECalComponentDateTime *date;
			ECalObjModType mod = E_CAL_OBJ_MOD_ALL;
			ECalClient *client;
			GtkWindow *toplevel;
			ICalTime *itt;
			ICalTimezone *zone;
			gboolean drag_event_is_recurring;

			num_days = 1;
			start_offset = 0;
			end_offset = 0;

			if (day_view->drag_event_day == E_DAY_VIEW_LONG_EVENT) {
				if (!is_array_index_in_bounds (day_view->long_events, day_view->drag_event_num))
					return;

				event = &g_array_index (day_view->long_events, EDayViewEvent,
							day_view->drag_event_num);

				if (!is_comp_data_valid (event))
					return;

				day -= day_view->drag_event_offset;
				day = MAX (day, 0);

				e_day_view_find_long_event_days (
					event,
					days_shown,
					day_view->day_starts,
					&start_day,
					&end_day);
				num_days = end_day - start_day + 1;
				/* Make sure we don't go off the screen. */
				day = MIN (day, days_shown - num_days);

				start_offset = event->start_minute;
				end_offset = event->end_minute;
			} else {
				if (!is_array_index_in_bounds (day_view->events[day_view->drag_event_day], day_view->drag_event_num))
					return;

				event = &g_array_index (day_view->events[day_view->drag_event_day],
							EDayViewEvent,
							day_view->drag_event_num);

				if (!is_comp_data_valid (event))
					return;
			}

			client = event->comp_data->client;

			/* We clone the event since we don't want to change
			 * the original comp here.
			 * Otherwise we would not detect that the event's time
			 * had changed in the "update_event" callback. */

			comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (event->comp_data->icalcomp));
			if (!comp)
				return;

			if (e_cal_component_has_attendees (comp) &&
			    !itip_organizer_is_user (registry, comp, client)) {
				g_object_unref (comp);
				return;
			}

			toplevel = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (day_view)));

			if (itip_has_any_attendees (comp) &&
			    (itip_organizer_is_user (registry, comp, client) ||
			     itip_sentby_is_user (registry, comp, client)))
				send = e_cal_dialogs_send_dragged_or_resized_component (
						toplevel, client, comp, &strip_alarms, &only_new_attendees);

			if (send == GTK_RESPONSE_CANCEL) {
				e_day_view_abort_resize (day_view);
				g_object_unref (comp);
				return;
			}

			if (start_offset == 0 && end_offset == 0)
				all_day_event = TRUE;
			else
				all_day_event = FALSE;

			zone = e_calendar_view_get_timezone (E_CALENDAR_VIEW (day_view));
			dt = day_view->day_starts[day] + start_offset * 60;
			itt = i_cal_time_new_from_timet_with_zone (dt, FALSE, zone);
			if (all_day_event) {
				i_cal_time_set_is_date (itt, TRUE);
				date = e_cal_component_datetime_new_take (itt, NULL);
			} else {
				date = e_cal_component_datetime_new_take (itt,
					zone ? g_strdup (i_cal_timezone_get_tzid (zone)) : NULL);
			}
			cal_comp_set_dtstart_with_oldzone (client, comp, date);
			e_cal_component_datetime_free (date);

			if (end_offset == 0)
				dt = day_view->day_starts[day + num_days];
			else
				dt = day_view->day_starts[day + num_days - 1] + end_offset * 60;
			itt = i_cal_time_new_from_timet_with_zone (dt, FALSE, zone);
			if (all_day_event) {
				i_cal_time_set_is_date (itt, TRUE);
				date = e_cal_component_datetime_new_take (itt, NULL);
			} else {
				date = e_cal_component_datetime_new_take (itt,
					zone ? g_strdup (i_cal_timezone_get_tzid (zone)) : NULL);
			}
			cal_comp_set_dtend_with_oldzone (client, comp, date);
			e_cal_component_datetime_free (date);

			drag_event_is_recurring = day_view->priv->drag_event_is_recurring;

			gtk_drag_finish (context, TRUE, TRUE, time);

			/* Reset this since it will be invalid. */
			day_view->drag_event_day = -1;
			day_view->priv->drag_event_is_recurring = FALSE;
			g_clear_object (&day_view->priv->drag_context);

			/* Show the text item again, just in case it hasn't
			 * moved. If we don't do this it may not appear. */
			if (event->canvas_item)
				gnome_canvas_item_show (event->canvas_item);

			e_cal_component_commit_sequence (comp);
			if (e_cal_component_has_recurrences (comp)) {
				if (!e_cal_dialogs_recur_component (client, comp, &mod, NULL, FALSE)) {
					gtk_widget_queue_draw (day_view->top_canvas);
					g_object_unref (comp);
					return;
				}

				if (mod == E_CAL_OBJ_MOD_THIS) {
					e_cal_component_set_rdates (comp, NULL);
					e_cal_component_set_rrules (comp, NULL);
					e_cal_component_set_exdates (comp, NULL);
					e_cal_component_set_exrules (comp, NULL);
				}
			} else if (e_cal_component_is_instance (comp))
				mod = E_CAL_OBJ_MOD_THIS;

			e_cal_component_commit_sequence (comp);

			if (gdk_drag_context_get_selected_action (context) == GDK_ACTION_COPY &&
			    !drag_event_is_recurring) {
				gchar *uid;

				uid = e_util_generate_uid ();
				e_cal_component_set_uid (comp, uid);
				g_free (uid);

				e_cal_ops_create_component (model, client, e_cal_component_get_icalcomponent (comp),
					e_calendar_view_component_created_cb, g_object_ref (day_view), g_object_unref);
			} else {
				e_cal_ops_modify_component (e_cal_model_get_data_model (model), client, e_cal_component_get_icalcomponent (comp), mod,
					(send == GTK_RESPONSE_YES ? E_CAL_OPS_SEND_FLAG_SEND : E_CAL_OPS_SEND_FLAG_DONT_SEND) |
					(strip_alarms ? E_CAL_OPS_SEND_FLAG_STRIP_ALARMS : 0) |
					(only_new_attendees ? E_CAL_OPS_SEND_FLAG_ONLY_NEW_ATTENDEES : 0));
			}

			g_object_unref (comp);

			return;
		}
	}

	if (length >= 0 && format == 8 && !drag_from_same_window) {
		/* We are dragging between different window */
		ICalComponent *icomp;
		ICalComponentKind kind;

		pos = e_day_view_convert_position_in_top_canvas (
			day_view,
			x, y, &day,
			NULL);
		if (pos == E_CALENDAR_VIEW_POS_OUTSIDE)
			goto error;

		icomp = i_cal_parser_parse_string ((const gchar *) data);
		if (!icomp)
			goto error;

		/* check the type of the component */
		kind = i_cal_component_isa (icomp);
		g_clear_object (&icomp);

		if (kind != I_CAL_VCALENDAR_COMPONENT && kind != I_CAL_VEVENT_COMPONENT)
			goto error;

		e_cal_ops_paste_components (model, (const gchar *) data);

		gtk_drag_finish (context, TRUE, TRUE, time);
		return;
	}

error:
	gtk_drag_finish (context, FALSE, FALSE, time);
}

static void
e_day_view_on_main_canvas_drag_data_received (GtkWidget *widget,
                                              GdkDragContext *context,
                                              gint x,
                                              gint y,
                                              GtkSelectionData *selection_data,
                                              guint info,
                                              guint time,
                                              EDayView *day_view)
{
	ECalendarView *cal_view;
	EDayViewEvent *event = NULL;
	ECalendarViewPosition pos;
	gint time_divisions;
	gint day, row, start_row, end_row, num_rows, scroll_x, scroll_y;
	gint start_offset, end_offset;
	ECalModel *model;
	ECalComponent *comp;
	ESourceRegistry *registry;
	time_t dt;
	gboolean drag_from_same_window;
	const guchar *data;
	gint format, length;
	GtkResponseType send = GTK_RESPONSE_NO;
	gboolean only_new_attendees = FALSE;
	gboolean strip_alarms = TRUE;

	cal_view = E_CALENDAR_VIEW (day_view);
	model = e_calendar_view_get_model (cal_view);
	time_divisions = e_calendar_view_get_time_divisions (cal_view);

	registry = e_cal_model_get_registry (model);

	data = gtk_selection_data_get_data (selection_data);
	format = gtk_selection_data_get_format (selection_data);
	length = gtk_selection_data_get_length (selection_data);

	if (day_view->drag_event_day != -1)
		drag_from_same_window = TRUE;
	else
		drag_from_same_window = FALSE;

	gnome_canvas_get_scroll_offsets (
		GNOME_CANVAS (widget),
		&scroll_x, &scroll_y);
	x += scroll_x;
	y += scroll_y;

	/* Note that we only support DnD within the EDayView at present. */
	if (length >= 0 && format == 8 && (day_view->drag_event_day != -1)) {
		/* We are dragging in the same window */

		pos = e_day_view_convert_position_in_main_canvas (
			day_view,
			x, y, &day,
			&row, NULL);
		if (pos != E_CALENDAR_VIEW_POS_OUTSIDE) {
			ECalComponentDateTime *date;
			ECalObjModType mod = E_CAL_OBJ_MOD_ALL;
			ECalClient *client;
			GtkWindow *toplevel;
			ICalTimezone *zone;
			gboolean drag_event_is_recurring;

			num_rows = 1;
			start_offset = 0;
			end_offset = 0;

			if (day_view->drag_event_day == E_DAY_VIEW_LONG_EVENT) {
				if (!is_array_index_in_bounds (day_view->long_events, day_view->drag_event_num))
					return;

				event = &g_array_index (day_view->long_events, EDayViewEvent,
							day_view->drag_event_num);

				if (!is_comp_data_valid (event))
					return;
			} else {
				if (!is_array_index_in_bounds (day_view->events[day_view->drag_event_day], day_view->drag_event_num))
					return;

				event = &g_array_index (day_view->events[day_view->drag_event_day],
							EDayViewEvent,
							day_view->drag_event_num);

				if (!is_comp_data_valid (event))
					return;

				row -= day_view->drag_event_offset;

				/* Calculate time offset from start row. */
				start_row = event->start_minute / time_divisions;
				end_row = (event->end_minute - 1) / time_divisions;
				if (end_row < start_row)
					end_row = start_row;

				num_rows = end_row - start_row + 1;

				start_offset = event->start_minute % time_divisions;
				end_offset = event->end_minute % time_divisions;
				if (end_offset != 0)
					end_offset = time_divisions - end_offset;
			}

			client = event->comp_data->client;

			/* We use a temporary shallow copy of comp since we
			 * don't want to change the original comp here.
			 * Otherwise we would not detect that the event's time
			 * had changed in the "update_event" callback. */
			comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (event->comp_data->icalcomp));
			if (!comp)
				return;

			if (e_cal_component_has_attendees (comp) &&
			    !itip_organizer_is_user (registry, comp, client)) {
				g_object_unref (comp);
				return;
			}

			toplevel = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (day_view)));

			if (itip_has_any_attendees (comp) &&
			    (itip_organizer_is_user (registry, comp, client) ||
			     itip_sentby_is_user (registry, comp, client)))
				send = e_cal_dialogs_send_dragged_or_resized_component (
						toplevel, client, comp, &strip_alarms, &only_new_attendees);

			if (send == GTK_RESPONSE_CANCEL) {
				e_day_view_abort_resize (day_view);
				g_object_unref (comp);
				return;
			}

			zone = e_calendar_view_get_timezone (E_CALENDAR_VIEW (day_view));

			dt = e_day_view_convert_grid_position_to_time (day_view, day, row) + start_offset * 60;
			date = e_cal_component_datetime_new_take (
				i_cal_time_new_from_timet_with_zone (dt, FALSE, zone),
				zone ? g_strdup (i_cal_timezone_get_tzid (zone)) : NULL);
			cal_comp_set_dtstart_with_oldzone (client, comp, date);
			e_cal_component_datetime_free (date);

			dt = e_day_view_convert_grid_position_to_time (day_view, day, row + num_rows) - end_offset * 60;
			date = e_cal_component_datetime_new_take (
				i_cal_time_new_from_timet_with_zone (dt, FALSE, zone),
				zone ? g_strdup (i_cal_timezone_get_tzid (zone)) : NULL);
			cal_comp_set_dtend_with_oldzone (client, comp, date);
			e_cal_component_datetime_free (date);

			e_cal_component_abort_sequence (comp);

			drag_event_is_recurring = day_view->priv->drag_event_is_recurring;

			gtk_drag_finish (context, TRUE, TRUE, time);

			/* Reset this since it will be invalid. */
			day_view->drag_event_day = -1;
			day_view->priv->drag_event_is_recurring = FALSE;
			g_clear_object (&day_view->priv->drag_context);

			/* Show the text item again, just in case it hasn't
			 * moved. If we don't do this it may not appear. */
			if (event->canvas_item)
				gnome_canvas_item_show (event->canvas_item);

			e_cal_component_commit_sequence (comp);
			if (e_cal_component_has_recurrences (comp)) {
				if (!e_cal_dialogs_recur_component (client, comp, &mod, NULL, FALSE)) {
					gtk_widget_queue_draw (day_view->main_canvas);
					g_object_unref (comp);
					return;
				}

				if (mod == E_CAL_OBJ_MOD_THIS) {
					e_cal_component_set_rdates (comp, NULL);
					e_cal_component_set_rrules (comp, NULL);
					e_cal_component_set_exdates (comp, NULL);
					e_cal_component_set_exrules (comp, NULL);
				}
			} else if (e_cal_component_is_instance (comp))
				mod = E_CAL_OBJ_MOD_THIS;

			e_cal_component_commit_sequence (comp);

			if (gdk_drag_context_get_selected_action (context) == GDK_ACTION_COPY &&
			    !drag_event_is_recurring) {
				gchar *uid;

				uid = e_util_generate_uid ();
				e_cal_component_set_uid (comp, uid);
				g_free (uid);

				e_cal_ops_create_component (model, client, e_cal_component_get_icalcomponent (comp),
					e_calendar_view_component_created_cb, g_object_ref (day_view), g_object_unref);
			} else {
				e_cal_ops_modify_component (e_cal_model_get_data_model (model), client, e_cal_component_get_icalcomponent (comp), mod,
					(send == GTK_RESPONSE_YES ? E_CAL_OPS_SEND_FLAG_SEND : E_CAL_OPS_SEND_FLAG_DONT_SEND) |
					(strip_alarms ? E_CAL_OPS_SEND_FLAG_STRIP_ALARMS : 0) |
					(only_new_attendees ? E_CAL_OPS_SEND_FLAG_ONLY_NEW_ATTENDEES : 0));
			}

			g_object_unref (comp);

			return;
		}
	}

	if (length >= 0 && format == 8 && !drag_from_same_window) {
		/* We are dragging between different window */
		ICalComponent *icomp;
		ICalComponentKind kind;

		pos = e_day_view_convert_position_in_main_canvas (
			day_view,
			x, y, &day,
			&row, NULL);
		if (pos == E_CALENDAR_VIEW_POS_OUTSIDE)
			goto error;

		icomp = i_cal_parser_parse_string ((const gchar *) data);
		if (!icomp)
			goto error;

		/* check the type of the component */
		kind = i_cal_component_isa (icomp);
		g_object_unref (&icomp);

		if (kind != I_CAL_VCALENDAR_COMPONENT && kind != I_CAL_VEVENT_COMPONENT)
			goto error;

		e_cal_ops_paste_components (model, (const gchar *) data);

		gtk_drag_finish (context, TRUE, TRUE, time);
		return;
	}

error:
	gtk_drag_finish (context, FALSE, FALSE, time);
}

/* Converts an hour from 0-23 to the preferred time format, and returns the
 * suffix to add and the width of it in the normal font. */
void
e_day_view_convert_time_to_display (EDayView *day_view,
                                    gint hour,
                                    gint *display_hour,
                                    const gchar **suffix,
                                    gint *suffix_width)
{
	ECalModel *model;

	model = e_calendar_view_get_model (E_CALENDAR_VIEW (day_view));

	/* Calculate the actual hour number to display. For 12-hour
	 * format we convert 0-23 to 12-11am/12-11pm. */
	*display_hour = hour;
	if (e_cal_model_get_use_24_hour_format (model)) {
		*suffix = "";
		*suffix_width = 0;
	} else {
		if (hour < 12) {
			*suffix = day_view->am_string;
			*suffix_width = day_view->am_string_width;
		} else {
			*display_hour -= 12;
			*suffix = day_view->pm_string;
			*suffix_width = day_view->pm_string_width;
		}

		/* 12-hour uses 12:00 rather than 0:00. */
		if (*display_hour == 0)
			*display_hour = 12;
	}
}

gint
e_day_view_get_time_string_width (EDayView *day_view)
{
	ECalModel *model;
	gint time_width;

	model = e_calendar_view_get_model (E_CALENDAR_VIEW (day_view));
	time_width = day_view->digit_width * 4 + day_view->colon_width;

	if (!e_cal_model_get_use_24_hour_format (model))
		time_width += MAX (day_view->am_string_width,
				   day_view->pm_string_width);

	return time_width;
}

/* Queues a layout, unless one is already queued. */
static void
e_day_view_queue_layout (EDayView *day_view)
{
	if (day_view->layout_timeout_id == 0) {
		day_view->layout_timeout_id = e_named_timeout_add (
			E_DAY_VIEW_LAYOUT_TIMEOUT,
			e_day_view_layout_timeout_cb, day_view);
	}
}

/* Removes any queued layout. */
static void
e_day_view_cancel_layout (EDayView *day_view)
{
	if (day_view->layout_timeout_id != 0) {
		g_source_remove (day_view->layout_timeout_id);
		day_view->layout_timeout_id = 0;
	}
}

static gboolean
e_day_view_layout_timeout_cb (gpointer data)
{
	EDayView *day_view = E_DAY_VIEW (data);

	gtk_widget_queue_draw (day_view->top_canvas);
	gtk_widget_queue_draw (day_view->top_dates_canvas);
	gtk_widget_queue_draw (day_view->main_canvas);
	e_day_view_check_layout (day_view);

	day_view->layout_timeout_id = 0;
	return FALSE;
}

/* Returns the number of selected events (0 or 1 at present). */
gint
e_day_view_get_num_events_selected (EDayView *day_view)
{
	g_return_val_if_fail (E_IS_DAY_VIEW (day_view), 0);

	return (day_view->editing_event_day != -1) ? 1 : 0;
}

gboolean
e_day_view_is_editing (EDayView *day_view)
{
	g_return_val_if_fail (E_IS_DAY_VIEW (day_view), FALSE);

	return day_view->editing_event_day != -1;
}

static void
day_view_update_timezone_name_label (GtkWidget *label,
				     ICalTimezone *zone)
{
	const gchar *location, *dash;
	gchar *markup;

	g_return_if_fail (GTK_IS_LABEL (label));

	if (!zone) {
		location = NULL;
	} else {
		location = i_cal_timezone_get_location (zone);
		if (location && *location)
			location = _(location);
		if (!location || !*location)
			location = i_cal_timezone_get_tzid (zone);
	}

	if (!location)
		location = "";

	gtk_widget_set_tooltip_text (label, location);

	dash = strrchr (location, '/');
	if (dash && *dash && dash[1])
		location = dash + 1;

	markup = g_markup_printf_escaped ("<small>%s</small>", location);
	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (markup);
}

void
e_day_view_update_timezone_name_labels (EDayView *day_view)
{
	ICalTimezone *zone;

	g_return_if_fail (E_IS_DAY_VIEW (day_view));

	zone = e_cal_model_get_timezone (day_view->priv->model);
	day_view_update_timezone_name_label (day_view->priv->timezone_name_1_label, zone);

	zone = e_day_view_time_item_get_second_zone (E_DAY_VIEW_TIME_ITEM (day_view->time_canvas_item));
	if (!zone) {
		gtk_widget_hide (day_view->priv->timezone_name_2_label);
	} else {
		day_view_update_timezone_name_label (day_view->priv->timezone_name_2_label, zone);
		gtk_widget_show (day_view->priv->timezone_name_2_label);
	}
}
