/*
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
 *		Damon Chaplin <damon@ximian.com>
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
 * EWeekView - displays the Week & Month views of the calendar.
 */

#include "evolution-config.h"

#include "e-week-view.h"

#include <math.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <libgnomecanvas/libgnomecanvas.h>

#include "calendar-config.h"
#include "comp-util.h"
#include "e-cal-dialogs.h"
#include "e-cal-model-calendar.h"
#include "e-cal-ops.h"
#include "e-week-view-event-item.h"
#include "e-week-view-layout.h"
#include "e-week-view-main-item.h"
#include "e-week-view-titles-item.h"
#include "ea-calendar.h"
#include "itip-utils.h"
#include "misc.h"
#include "print.h"
#include "ea-week-view.h"

/* Images */
#include "data/xpm/jump.xpm"

#define E_WEEK_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_WEEK_VIEW, EWeekViewPrivate))

#define E_WEEK_VIEW_SMALL_FONT_PTSIZE 7

#define E_WEEK_VIEW_JUMP_BUTTON_WIDTH	16
#define E_WEEK_VIEW_JUMP_BUTTON_HEIGHT	8

#define E_WEEK_VIEW_JUMP_BUTTON_X_PAD	3
#define E_WEEK_VIEW_JUMP_BUTTON_Y_PAD	3

#define E_WEEK_VIEW_JUMP_BUTTON_NO_FOCUS -1

/* The timeout before we do a layout, so we don't do a layout for each event
 * we get from the server. */
#define E_WEEK_VIEW_LAYOUT_TIMEOUT	100

struct _EWeekViewPrivate {
	/* The first day shown in the view. */
	GDate first_day_shown;

	/* If we are displaying multiple weeks in rows.  If this is
	 * FALSE only one week is shown, with a different layout. */
	gboolean multi_week_view;

	/* How many weeks we are showing.  This is only relevant if
	 * multi_week_view is TRUE. */
	gint weeks_shown;

	/* If Sat & Sun are compressed.  Only applicable in month view,
	 * since they are always compressed into 1 cell in week view. */
	gboolean compress_weekend;

	/* Whether we show event end times. */
	gboolean show_event_end_times;

	/* Whether to update the base date when the time range changes. */
	gboolean update_base_date;

	/* The first day of the week we display.  This will usually be
	 * week_start_day, but if the Sat & Sun are compressed and the
	 * week starts on Sunday then we have to use Saturday. */
	GDateWeekday display_start_day;

	gulong notify_week_start_day_id;

	gboolean show_icons_month_view;
	gboolean draw_flat_events;
	gboolean days_left_to_right;
};

typedef struct {
	EWeekView *week_view;
	ECalModelComponent *comp_data;
} AddEventData;

static void e_week_view_set_colors (EWeekView *week_view);
static void e_week_view_recalc_cell_sizes (EWeekView *week_view);
static gboolean e_week_view_get_next_tab_event (EWeekView *week_view,
						GtkDirectionType direction,
						gint current_event_num,
						gint current_span_num,
						gint *next_event_num,
						gint *next_span_num);
static void e_week_view_update_query (EWeekView *week_view);

static gboolean e_week_view_on_button_press (GtkWidget *widget,
					     GdkEvent *button_event,
					     EWeekView *week_view);
static gboolean e_week_view_on_button_release (GtkWidget *widget,
					       GdkEvent *button_event,
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
static void e_week_view_add_event (ECalClient *client,
				   ECalComponent *comp,
				   time_t start,
				   time_t end,
				   gboolean prepend,
				   gpointer data);
static void e_week_view_check_layout (EWeekView *week_view);
static void e_week_view_ensure_events_sorted (EWeekView *week_view);
static void e_week_view_reshape_events (EWeekView *week_view);
static void e_week_view_reshape_event_span (EWeekView *week_view,
					    gint event_num,
					    gint span_num);
static void e_week_view_recalc_day_starts (EWeekView *week_view,
					   time_t lower);
static void e_week_view_on_editing_started (EWeekView *week_view,
					    GnomeCanvasItem *item);
static void e_week_view_on_editing_stopped (EWeekView *week_view,
					    GnomeCanvasItem *item);
static gboolean e_week_view_find_event_from_uid (EWeekView	  *week_view,
						 ECalClient             *client,
						 const gchar	  *uid,
						 const gchar      *rid,
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
static gboolean e_week_view_event_move (ECalendarView *cal_view, ECalViewMoveDirection direction);
static gint e_week_view_get_day_offset_of_event (EWeekView *week_view, time_t event_time);
static void e_week_view_change_event_time (EWeekView *week_view, time_t start_dt, time_t end_dt, gboolean is_all_day);
static gboolean e_week_view_on_jump_button_event (GnomeCanvasItem *item,
						  GdkEvent *event,
						  EWeekView *week_view);
static gboolean e_week_view_do_key_press (GtkWidget *widget,
					  GdkEventKey *event);
static gint e_week_view_get_adjust_days_for_move_up (EWeekView *week_view, gint
current_day);
static gint e_week_view_get_adjust_days_for_move_down (EWeekView *week_view,gint current_day);
static gint e_week_view_get_adjust_days_for_move_left (EWeekView *week_view,gint current_day);
static gint e_week_view_get_adjust_days_for_move_right (EWeekView *week_view,gint current_day);

static gboolean e_week_view_remove_event_cb (EWeekView *week_view,
					     gint event_num,
					     gpointer data);
static gboolean e_week_view_recalc_display_start_day	(EWeekView	*week_view);

static void e_week_view_queue_layout (EWeekView *week_view);
static void e_week_view_cancel_layout (EWeekView *week_view);
static gboolean e_week_view_layout_timeout_cb (gpointer data);

G_DEFINE_TYPE (EWeekView, e_week_view, E_TYPE_CALENDAR_VIEW)

enum {
	PROP_0,
	PROP_COMPRESS_WEEKEND,
	PROP_DRAW_FLAT_EVENTS,
	PROP_DAYS_LEFT_TO_RIGHT,
	PROP_SHOW_EVENT_END_TIMES,
	PROP_SHOW_ICONS_MONTH_VIEW,
	PROP_IS_EDITING
};

static gint map_left[] = {0, 1, 2, 0, 1, 2, 2};
static gint map_right[] = {3, 4, 5, 3, 4, 5, 6};

static void
week_view_process_component (EWeekView *week_view,
                             ECalModelComponent *comp_data)
{
	ECalComponent *comp = NULL;
	AddEventData add_event_data;
	/* rid is never used in this function? */
	const gchar *uid;
	gchar *rid = NULL;

	/* If we don't have a valid date set yet, just return. */
	if (!g_date_valid (&week_view->priv->first_day_shown))
		return;

	comp = e_cal_component_new ();
	if (!e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (comp_data->icalcomp))) {
		g_object_unref (comp);

		g_message (G_STRLOC ": Could not set icalcomponent on ECalComponent");
		return;
	}

	e_cal_component_get_uid (comp, &uid);
	if (e_cal_component_is_instance (comp))
		rid = e_cal_component_get_recurid_as_string (comp);
	else
		rid = NULL;

	/* Add the object */
	add_event_data.week_view = week_view;
	add_event_data.comp_data = comp_data;
	e_week_view_add_event (comp_data->client, comp, comp_data->instance_start, comp_data->instance_end, FALSE, &add_event_data);

	g_object_unref (comp);
	g_free (rid);
}

static void
week_view_update_row (EWeekView *week_view,
                      gint row)
{
	ECalModelComponent *comp_data;
	ECalModel *model;
	gint event_num;
	const gchar *uid;
	gchar *rid = NULL;

	model = e_calendar_view_get_model (E_CALENDAR_VIEW (week_view));
	comp_data = e_cal_model_get_component_at (model, row);
	g_return_if_fail (comp_data != NULL);

	uid = icalcomponent_get_uid (comp_data->icalcomp);
	if (e_cal_util_component_is_instance (comp_data->icalcomp)) {
		icalproperty *prop;

		prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_RECURRENCEID_PROPERTY);
		if (prop)
			rid = icaltime_as_ical_string_r (icalcomponent_get_recurrenceid (comp_data->icalcomp));
	}

	if (e_week_view_find_event_from_uid (week_view, comp_data->client, uid, rid, &event_num))
		e_week_view_remove_event_cb (week_view, event_num, NULL);

	g_free (rid);

	week_view_process_component (week_view, comp_data);

	gtk_widget_queue_draw (week_view->main_canvas);
	e_week_view_queue_layout (week_view);
}

static void
week_view_model_cell_changed_cb (EWeekView *week_view,
                                 gint col,
                                 gint row)
{
	if (!E_CALENDAR_VIEW (week_view)->in_focus) {
		e_week_view_free_events (week_view);
		week_view->requires_update = TRUE;
		return;
	}

	week_view_update_row (week_view, row);
}

static void
week_view_model_comps_deleted_cb (EWeekView *week_view,
                                  gpointer data)
{
	GSList *l, *list = data;

	/* FIXME Stop editing? */
	if (!E_CALENDAR_VIEW (week_view)->in_focus) {
		e_week_view_free_events (week_view);
		week_view->requires_update = TRUE;
		return;
	}

	for (l = list; l != NULL; l = g_slist_next (l)) {
		gint event_num;
		const gchar *uid;
		gchar *rid = NULL;
		ECalModelComponent *comp_data = l->data;

		uid = icalcomponent_get_uid (comp_data->icalcomp);
		if (e_cal_util_component_is_instance (comp_data->icalcomp)) {
			icalproperty *prop;

			prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_RECURRENCEID_PROPERTY);
			if (prop)
				rid = icaltime_as_ical_string_r (icalcomponent_get_recurrenceid (comp_data->icalcomp));
		}

		if (e_week_view_find_event_from_uid (week_view, comp_data->client, uid, rid, &event_num))
			e_week_view_remove_event_cb (week_view, event_num, NULL);
		g_free (rid);
	}

	gtk_widget_queue_draw (week_view->main_canvas);
	e_week_view_queue_layout (week_view);
}

static void
week_view_model_row_changed_cb (EWeekView *week_view,
                                gint row)
{
	if (!E_CALENDAR_VIEW (week_view)->in_focus) {
		e_week_view_free_events (week_view);
		week_view->requires_update = TRUE;
		return;
	}

	week_view_update_row (week_view, row);
}

static void
week_view_model_rows_inserted_cb (EWeekView *week_view,
                                  gint row,
                                  gint count)
{
	ECalModel *model;
	gint i;

	if (!E_CALENDAR_VIEW (week_view)->in_focus) {
		e_week_view_free_events (week_view);
		week_view->requires_update = TRUE;
		return;
	}

	model = e_calendar_view_get_model (E_CALENDAR_VIEW (week_view));

	for (i = 0; i < count; i++) {
		ECalModelComponent *comp_data;

		comp_data = e_cal_model_get_component_at (model, row + i);
		if (comp_data == NULL) {
			g_warning ("comp_data is NULL\n");
			continue;
		}
		week_view_process_component (week_view, comp_data);
	}

	gtk_widget_queue_draw (week_view->main_canvas);
	e_week_view_queue_layout (week_view);
}

static void
e_week_view_precalc_visible_time_range (ECalendarView *cal_view,
					time_t in_start_time,
					time_t in_end_time,
					time_t *out_start_time,
					time_t *out_end_time)
{
	EWeekView *week_view;
	GDate date, base_date;
	GDateWeekday weekday;
	GDateWeekday display_start_day;
	guint day_offset, week_start_offset;
	gint num_days;
	icaltimezone *zone;

	g_return_if_fail (E_IS_WEEK_VIEW (cal_view));
	g_return_if_fail (out_start_time != NULL);
	g_return_if_fail (out_end_time != NULL);

	week_view = E_WEEK_VIEW (cal_view);
	zone = e_calendar_view_get_timezone (cal_view);

	time_to_gdate_with_zone (&date, in_start_time, e_calendar_view_get_timezone (E_CALENDAR_VIEW (week_view)));

	weekday = g_date_get_weekday (&date);
	display_start_day = e_week_view_get_display_start_day (week_view);

	/* Convert it to an offset from the start of the display. */
	week_start_offset = e_weekday_get_days_between (display_start_day, weekday);

	/* Set the day_offset to the result, so we move back to the
	 * start of the week. */
	day_offset = week_start_offset;

	/* Calculate the base date, i.e. the first day shown when the
	 * scrollbar adjustment value is 0. */
	base_date = date;
	g_date_subtract_days (&base_date, day_offset);

	num_days = e_week_view_get_weeks_shown (week_view) * 7;

	/* See if we need to update the first day shown. */
	if (!g_date_valid (&week_view->priv->first_day_shown)
	    || g_date_compare (&week_view->priv->first_day_shown, &base_date)) {
		GDate end_date, in_end_date;
		gint day;

		end_date = date;
		g_date_add_days (&end_date, num_days);
		g_date_subtract_days (&end_date, day_offset);

		time_to_gdate_with_zone (&in_end_date, in_end_time, e_calendar_view_get_timezone (E_CALENDAR_VIEW (week_view)));

		while (g_date_days_between (&end_date, &in_end_date) >= 6) {
			g_date_add_days (&end_date, 7);
			num_days += 7;
		}

		in_start_time = time_add_day_with_zone (in_start_time, -((gint) day_offset), zone);
		in_start_time = time_day_begin_with_zone (in_start_time, zone);

		*out_start_time = in_start_time;
		*out_end_time = in_start_time;

		for (day = 1; day <= num_days; day++) {
			*out_end_time = time_add_day_with_zone (*out_end_time, 1, zone);
		}
	} else {
		*out_start_time = week_view->day_starts[0];
		*out_end_time = week_view->day_starts[num_days];
	}
}

static void
week_view_time_range_changed_cb (EWeekView *week_view,
                                 gint64 i64_start_time,
                                 gint64 i64_end_time,
                                 ECalModel *model)
{
	time_t start_time = (time_t) i64_start_time;
	GDate date, base_date;
	GDateWeekday weekday;
	GDateWeekday display_start_day;
	guint day_offset, week_start_offset;
	gint num_days;
	gboolean update_adjustment_value = FALSE;

	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	time_to_gdate_with_zone (&date, start_time, e_calendar_view_get_timezone (E_CALENDAR_VIEW (week_view)));

	weekday = g_date_get_weekday (&date);
	display_start_day = e_week_view_get_display_start_day (week_view);

	/* Convert it to an offset from the start of the display. */
	week_start_offset = e_weekday_get_days_between (
		display_start_day, weekday);

	/* Set the day_offset to the result, so we move back to the
	 * start of the week. */
	day_offset = week_start_offset;

	/* Calculate the base date, i.e. the first day shown when the
	 * scrollbar adjustment value is 0. */
	base_date = date;
	g_date_subtract_days (&base_date, day_offset);

	/* See if we need to update the base date. */
	if (!g_date_valid (&week_view->base_date)
	    || e_week_view_get_update_base_date (week_view)) {
		week_view->base_date = base_date;
		update_adjustment_value = TRUE;
	}

	/* See if we need to update the first day shown. */
	if (!g_date_valid (&week_view->priv->first_day_shown)
	    || g_date_compare (&week_view->priv->first_day_shown, &base_date)) {
		week_view->priv->first_day_shown = base_date;
		start_time = time_add_day_with_zone (
			start_time, -((gint) day_offset),
			e_calendar_view_get_timezone (E_CALENDAR_VIEW (week_view)));
		start_time = time_day_begin_with_zone (
			start_time,
			e_calendar_view_get_timezone (E_CALENDAR_VIEW (week_view)));
		e_week_view_recalc_day_starts (week_view, start_time);
	}

	/* Reset the adjustment value to 0 if the base address has changed.
	 * Note that we do this after updating first_day_shown so that our
	 * signal handler will not try to reload the events. */
	if (update_adjustment_value) {
		GtkRange *range;
		GtkAdjustment *adjustment;

		range = GTK_RANGE (week_view->vscrollbar);
		adjustment = gtk_range_get_adjustment (range);
		gtk_adjustment_set_value (adjustment, 0);
	}

	if (!E_CALENDAR_VIEW (week_view)->in_focus) {
		e_week_view_free_events (week_view);
		week_view->requires_update = TRUE;
		return;
	}

	gtk_widget_queue_draw (week_view->main_canvas);

	num_days = e_week_view_get_weeks_shown (week_view) * 7;

	/* FIXME Preserve selection if possible */
	if (week_view->selection_start_day == -1 ||
	    num_days <= week_view->selection_start_day)
		e_calendar_view_set_selected_time_range (
			E_CALENDAR_VIEW (week_view), start_time, start_time);
}

static void
timezone_changed_cb (ECalModel *cal_model,
                     icaltimezone *old_zone,
                     icaltimezone *new_zone,
                     gpointer user_data)
{
	ECalendarView *cal_view = (ECalendarView *) user_data;
	GDate *first_day_shown;
	struct icaltimetype tt = icaltime_null_time ();
	time_t lower;
	EWeekView *week_view = (EWeekView *) cal_view;

	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	first_day_shown = &week_view->priv->first_day_shown;

	if (!cal_view->in_focus) {
		e_week_view_free_events (week_view);
		week_view->requires_update = TRUE;
		return;
	}

	/* If we don't have a valid date set yet, just return. */
	if (!g_date_valid (first_day_shown))
		return;

	/* Recalculate the new start of the first week. We just use exactly
	 * the same time, but with the new timezone. */
	tt.year = g_date_get_year (first_day_shown);
	tt.month = g_date_get_month (first_day_shown);
	tt.day = g_date_get_day (first_day_shown);

	lower = icaltime_as_timet_with_zone (tt, new_zone);

	e_week_view_recalc_day_starts (week_view, lower);
	e_week_view_update_query (week_view);
}

static void
week_view_notify_week_start_day_cb (EWeekView *week_view)
{
	GDate *first_day_shown;

	first_day_shown = &week_view->priv->first_day_shown;

	e_week_view_recalc_display_start_day (week_view);

	/* Recalculate the days shown and reload if necessary. */
	if (g_date_valid (first_day_shown))
		e_week_view_set_first_day_shown (week_view, first_day_shown);

	gtk_widget_queue_draw (week_view->titles_canvas);
	gtk_widget_queue_draw (week_view->main_canvas);
}

static void
month_scroll_by_week_changed_cb (GSettings *settings,
                                 const gchar *key,
                                 gpointer user_data)
{
	EWeekView *week_view = user_data;

	g_return_if_fail (week_view != NULL);
	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	if (e_week_view_get_multi_week_view (week_view) &&
	    week_view->month_scroll_by_week != calendar_config_get_month_scroll_by_week ()) {
		week_view->priv->multi_week_view = FALSE;
		e_week_view_set_multi_week_view (week_view, TRUE);
	}
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

static gint
get_string_width (PangoLayout *layout,
                  const gchar *string)
{
	gint width;

	pango_layout_set_text (layout, string, -1);
	pango_layout_get_pixel_size (layout, &width, NULL);
	return width;
}

typedef struct {
	EWeekView *week_view;
	time_t dtstart, dtend;
	gchar *initial_text;
	gboolean paste_clipboard;
} NewEventInRangeData;

static void
new_event_in_rage_data_free (gpointer ptr)
{
	NewEventInRangeData *ned = ptr;

	if (ned) {
		g_clear_object (&ned->week_view);
		g_free (ned->initial_text);
		g_free (ned);
	}
}

static void
week_view_new_event_in_selected_range_cb (ECalModel *model,
					  ECalClient *client,
					  icalcomponent *default_component,
					  gpointer user_data)
{
	NewEventInRangeData *ned = user_data;
	ECalComponent *comp = NULL;
	gint event_num;
	ECalComponentDateTime date;
	struct icaltimetype itt;
	const gchar *uid;
	AddEventData add_event_data;
	EWeekViewEvent *wvevent;
	EWeekViewEventSpan *span;
	icaltimezone *zone;

	/* Check if the client is read only */
	if (e_client_is_readonly (E_CLIENT (client)))
		goto exit;

	/* Add a new event covering the selected range. */
	comp = e_cal_component_new_from_icalcomponent (icalcomponent_new_clone (default_component));
	g_return_if_fail (comp != NULL);

	uid = icalcomponent_get_uid (default_component);

	date.value = &itt;
	date.tzid = NULL;

	zone = e_cal_model_get_timezone (model);

	/* We use DATE values now, so we don't need the timezone. */
	/*date.tzid = icaltimezone_get_tzid (e_calendar_view_get_timezone (E_CALENDAR_VIEW (week_view)));*/

	*date.value = icaltime_from_timet_with_zone (ned->dtstart, TRUE, zone);
	e_cal_component_set_dtstart (comp, &date);

	*date.value = icaltime_from_timet_with_zone (ned->dtend, TRUE, zone);
	e_cal_component_set_dtend (comp, &date);

	/* Editor default in week/month view */
	e_cal_component_set_transparency (comp, E_CAL_COMPONENT_TRANSP_TRANSPARENT);

	/* We add the event locally and start editing it. We don't send it
	 * to the server until the user finishes editing it. */
	add_event_data.week_view = ned->week_view;
	add_event_data.comp_data = NULL;
	e_week_view_add_event (client, comp, ned->dtstart, ned->dtend, TRUE, &add_event_data);
	e_week_view_check_layout (ned->week_view);
	gtk_widget_queue_draw (ned->week_view->main_canvas);

	if (!e_week_view_find_event_from_uid (ned->week_view, client, uid, NULL, &event_num)) {
		g_warning ("Couldn't find event to start editing.\n");
		goto exit;
	}

	if (!is_array_index_in_bounds (ned->week_view->events, event_num))
		goto exit;

	wvevent = &g_array_index (ned->week_view->events, EWeekViewEvent, event_num);

	if (!is_array_index_in_bounds (ned->week_view->spans, wvevent->spans_index + 0))
		goto exit;

	span = &g_array_index (ned->week_view->spans, EWeekViewEventSpan, wvevent->spans_index + 0);

	/* If the event can't be fit on the screen, don't try to edit it. */
	if (!span->text_item) {
		e_week_view_foreach_event_with_uid (ned->week_view, uid, e_week_view_remove_event_cb, NULL);
		goto exit;
	}

	e_week_view_start_editing_event (ned->week_view, event_num, 0, ned->initial_text);

	if (ned->paste_clipboard) {
		wvevent = &g_array_index (ned->week_view->events, EWeekViewEvent, ned->week_view->editing_event_num);

		if (!is_array_index_in_bounds (ned->week_view->spans, wvevent->spans_index + ned->week_view->editing_span_num))
			return;

		span = &g_array_index (ned->week_view->spans, EWeekViewEventSpan, wvevent->spans_index + ned->week_view->editing_span_num);

		if (span->text_item &&
		    E_IS_TEXT (span->text_item) &&
		    E_TEXT (span->text_item)->editing) {
			e_text_paste_clipboard (E_TEXT (span->text_item));
		}
	}

 exit:
	g_clear_object (&comp);
}

static void
e_week_view_add_new_event_in_selected_range (EWeekView *week_view,
                                             const gchar *initial_text,
					     gboolean paste_clipboard)
{
	NewEventInRangeData *ned;
	ECalModel *model;
	const gchar *source_uid;

	ned = g_new0 (NewEventInRangeData, 1);
	ned->week_view = g_object_ref (week_view);
	ned->initial_text = g_strdup (initial_text);
	ned->dtstart = week_view->day_starts[week_view->selection_start_day];
	ned->dtend = week_view->day_starts[week_view->selection_end_day + 1];
	ned->paste_clipboard = paste_clipboard;

	model = e_calendar_view_get_model (E_CALENDAR_VIEW (week_view));
	source_uid = e_cal_model_get_default_source_uid (model);

	e_cal_ops_get_default_component	 (model, source_uid, TRUE,
		week_view_new_event_in_selected_range_cb, ned, new_event_in_rage_data_free);
}

static void
week_view_set_property (GObject *object,
                        guint property_id,
                        const GValue *value,
                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_COMPRESS_WEEKEND:
			e_week_view_set_compress_weekend (
				E_WEEK_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_DAYS_LEFT_TO_RIGHT:
			e_week_view_set_days_left_to_right (
				E_WEEK_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_DRAW_FLAT_EVENTS:
			e_week_view_set_draw_flat_events (
				E_WEEK_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHOW_EVENT_END_TIMES:
			e_week_view_set_show_event_end_times (
				E_WEEK_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHOW_ICONS_MONTH_VIEW:
			e_week_view_set_show_icons_month_view (
				E_WEEK_VIEW (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
week_view_get_property (GObject *object,
                        guint property_id,
                        GValue *value,
                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_COMPRESS_WEEKEND:
			g_value_set_boolean (
				value,
				e_week_view_get_compress_weekend (
				E_WEEK_VIEW (object)));
			return;

		case PROP_DAYS_LEFT_TO_RIGHT:
			g_value_set_boolean (
				value,
				e_week_view_get_days_left_to_right (
				E_WEEK_VIEW (object)));
			return;

		case PROP_DRAW_FLAT_EVENTS:
			g_value_set_boolean (
				value,
				e_week_view_get_draw_flat_events (
				E_WEEK_VIEW (object)));
			return;

		case PROP_SHOW_EVENT_END_TIMES:
			g_value_set_boolean (
				value,
				e_week_view_get_show_event_end_times (
				E_WEEK_VIEW (object)));
			return;

		case PROP_SHOW_ICONS_MONTH_VIEW:
			g_value_set_boolean (
				value,
				e_week_view_get_show_icons_month_view (
				E_WEEK_VIEW (object)));
			return;

		case PROP_IS_EDITING:
			g_value_set_boolean (value, e_week_view_is_editing (E_WEEK_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
week_view_dispose (GObject *object)
{
	EWeekView *week_view;
	ECalModel *model;

	week_view = E_WEEK_VIEW (object);
	model = e_calendar_view_get_model (E_CALENDAR_VIEW (object));

	e_week_view_cancel_layout (week_view);

	if (model) {
		g_signal_handlers_disconnect_by_data (model, object);
		e_signal_disconnect_notify_handler (model, &week_view->priv->notify_week_start_day_id);
	}

	if (week_view->events) {
		e_week_view_free_events (week_view);
		g_array_free (week_view->events, TRUE);
		week_view->events = NULL;
	}

	if (week_view->small_font_desc) {
		pango_font_description_free (week_view->small_font_desc);
		week_view->small_font_desc = NULL;
	}

	if (week_view->normal_cursor) {
		g_object_unref (week_view->normal_cursor);
		week_view->normal_cursor = NULL;
	}
	if (week_view->move_cursor) {
		g_object_unref (week_view->move_cursor);
		week_view->move_cursor = NULL;
	}
	if (week_view->resize_width_cursor) {
		g_object_unref (week_view->resize_width_cursor);
		week_view->resize_width_cursor = NULL;
	}

	calendar_config_remove_notification (
		month_scroll_by_week_changed_cb, week_view);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_week_view_parent_class)->dispose (object);
}

static void
week_view_constructed (GObject *object)
{
	EWeekView *week_view;
	ECalModel *model;
	ECalendarView *calendar_view;
	PangoContext *pango_context;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_week_view_parent_class)->constructed (object);

	week_view = E_WEEK_VIEW (object);
	calendar_view = E_CALENDAR_VIEW (object);
	model = e_calendar_view_get_model (calendar_view);

	pango_context = gtk_widget_get_pango_context (GTK_WIDGET (week_view));
	g_warn_if_fail (pango_context != NULL);
	week_view->small_font_desc = pango_font_description_copy (pango_context_get_font_description (pango_context));
	pango_font_description_set_size (
		week_view->small_font_desc,
		E_WEEK_VIEW_SMALL_FONT_PTSIZE * PANGO_SCALE);

	e_week_view_recalc_display_start_day (E_WEEK_VIEW (object));

	week_view->priv->notify_week_start_day_id = e_signal_connect_notify_swapped (
		model, "notify::week-start-day",
		G_CALLBACK (week_view_notify_week_start_day_cb), object);

	g_signal_connect_swapped (
		model, "comps-deleted",
		G_CALLBACK (week_view_model_comps_deleted_cb), object);

	g_signal_connect_swapped (
		model, "model-cell-changed",
		G_CALLBACK (week_view_model_cell_changed_cb), object);

	g_signal_connect_swapped (
		model, "model-row-changed",
		G_CALLBACK (week_view_model_row_changed_cb), object);

	g_signal_connect_swapped (
		model, "model-rows-inserted",
		G_CALLBACK (week_view_model_rows_inserted_cb), object);

	g_signal_connect_swapped (
		model, "time-range-changed",
		G_CALLBACK (week_view_time_range_changed_cb), object);
}

static GdkColor
e_week_view_get_text_color (EWeekView *week_view,
                            EWeekViewEvent *event)
{
	GdkColor color;
	GdkRGBA bg_rgba;

	if (is_comp_data_valid (event) &&
	    e_cal_model_get_rgba_for_component (e_calendar_view_get_model (E_CALENDAR_VIEW (week_view)), event->comp_data, &bg_rgba)) {
	} else {
		gdouble	cc = 65535.0;

		bg_rgba.red = week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BACKGROUND].red / cc;
		bg_rgba.green = week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BACKGROUND].green / cc;
		bg_rgba.blue = week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BACKGROUND].blue / cc;
		bg_rgba.alpha = 1.0;

	}

	color.pixel = 0;

	if ((bg_rgba.red > 0.7) || (bg_rgba.green > 0.7) || (bg_rgba.blue > 0.7)) {
		color.red = 0.0;
		color.green = 0.0;
		color.blue = 0.0;
	} else {
		color.red = 65535.0f;
		color.green = 65535.0f;
		color.blue = 65535.0f;
	}

	return color;
}

static void
week_view_update_style_settings (EWeekView *week_view)
{
	gint day, day_width, max_day_width, max_abbr_day_width;
	gint month, month_width, max_month_width, max_abbr_month_width;
	gint span_num;
	const gchar *name;
	PangoFontDescription *font_desc;
	PangoContext *pango_context;
	PangoFontMetrics *font_metrics;
	PangoLayout *layout;
	EWeekViewEventSpan *span;

	e_week_view_set_colors (week_view);
	e_week_view_check_layout (week_view);

	if (week_view->spans) {
		for (span_num = 0; span_num < week_view->spans->len; span_num++) {
			span = &g_array_index (week_view->spans, EWeekViewEventSpan, span_num);
			if (span->text_item && span->background_item) {
				gint event_num = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (span->background_item), "event-num"));
				EWeekViewEvent *event = NULL;

				if (is_array_index_in_bounds (week_view->events, event_num))
					event = &g_array_index (week_view->events, EWeekViewEvent, event_num);

				if (event) {
					GdkColor text_color;

					text_color = e_week_view_get_text_color (week_view, event);

					gnome_canvas_item_set (
						span->text_item,
						"fill_color_gdk", &text_color,
						NULL);
				}
			}
		}
	}

	/* Set up Pango prerequisites */
	pango_context = gtk_widget_get_pango_context (GTK_WIDGET (week_view));
	font_desc = pango_font_description_copy (pango_context_get_font_description (pango_context));
	font_metrics = pango_context_get_metrics (
		pango_context, font_desc,
		pango_context_get_language (pango_context));
	layout = pango_layout_new (pango_context);

	/* Recalculate the height of each row based on the font size. */
	week_view->row_height = PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics)) +
		PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics)) +
		E_WEEK_VIEW_EVENT_BORDER_HEIGHT * 2 + E_WEEK_VIEW_EVENT_TEXT_Y_PAD * 2;
	week_view->row_height = MAX (week_view->row_height, E_WEEK_VIEW_ICON_HEIGHT + E_WEEK_VIEW_ICON_Y_PAD + E_WEEK_VIEW_EVENT_BORDER_HEIGHT * 2);

	/* Check that the small font is smaller than the default font.
	 * If it isn't, we won't use it. */
	if (week_view->small_font_desc) {
		if (PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics)) +
		    PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics))
		    <= E_WEEK_VIEW_SMALL_FONT_PTSIZE)
			week_view->use_small_font = FALSE;
	}

	/* Set the height of the top canvas. */
	gtk_widget_set_size_request (
		week_view->titles_canvas, -1,
		PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics)) +
		PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics)) + 5);

	/* Save the sizes of various strings in the font, so we can quickly
	 * decide which date formats to use. */

	max_day_width = 0;
	max_abbr_day_width = 0;
	for (day = 0; day < 7; day++) {
		name = e_get_weekday_name (day + 1, FALSE);
		day_width = get_string_width (layout, name);
		week_view->day_widths[day] = day_width;
		max_day_width = MAX (max_day_width, day_width);

		name = e_get_weekday_name (day + 1, TRUE);
		day_width = get_string_width (layout, name);
		week_view->abbr_day_widths[day] = day_width;
		max_abbr_day_width = MAX (max_abbr_day_width, day_width);
	}

	max_month_width = 0;
	max_abbr_month_width = 0;
	for (month = 0; month < 12; month++) {
		name = e_get_month_name (month + 1, FALSE);
		month_width = get_string_width (layout, name);
		week_view->month_widths[month] = month_width;
		max_month_width = MAX (max_month_width, month_width);

		name = e_get_month_name (month + 1, TRUE);
		month_width = get_string_width (layout, name);
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
		pango_layout_set_font_description (layout, font_desc);
	}
	week_view->max_day_width = max_day_width;
	week_view->max_abbr_day_width = max_abbr_day_width;
	week_view->max_month_width = max_month_width;
	week_view->max_abbr_month_width = max_abbr_month_width;

	week_view->am_string_width = get_string_width (
		layout,
		week_view->am_string);
	week_view->pm_string_width = get_string_width (
		layout,
		week_view->pm_string);

	g_object_unref (layout);
	pango_font_metrics_unref (font_metrics);
	pango_font_description_free (font_desc);
}

static void
week_view_realize (GtkWidget *widget)
{
	EWeekView *week_view;

	if (GTK_WIDGET_CLASS (e_week_view_parent_class)->realize)
		(*GTK_WIDGET_CLASS (e_week_view_parent_class)->realize)(widget);

	week_view = E_WEEK_VIEW (widget);

	week_view_update_style_settings (week_view);

	/* Create the pixmaps. */
	week_view->reminder_icon =
		e_icon_factory_get_icon ("stock_bell", GTK_ICON_SIZE_MENU);
	week_view->recurrence_icon =
		e_icon_factory_get_icon ("view-refresh", GTK_ICON_SIZE_MENU);
	week_view->timezone_icon =
		e_icon_factory_get_icon ("stock_timezone", GTK_ICON_SIZE_MENU);
	week_view->attach_icon =
		e_icon_factory_get_icon ("mail-attachment", GTK_ICON_SIZE_MENU);
	week_view->meeting_icon =
		e_icon_factory_get_icon ("stock_people", GTK_ICON_SIZE_MENU);
}

static void
week_view_unrealize (GtkWidget *widget)
{
	EWeekView *week_view;

	week_view = E_WEEK_VIEW (widget);

	g_object_unref (week_view->reminder_icon);
	week_view->reminder_icon = NULL;
	g_object_unref (week_view->recurrence_icon);
	week_view->recurrence_icon = NULL;
	g_object_unref (week_view->timezone_icon);
	week_view->timezone_icon = NULL;
	g_object_unref (week_view->attach_icon);
	week_view->attach_icon = NULL;
	g_object_unref (week_view->meeting_icon);
	week_view->meeting_icon = NULL;

	if (GTK_WIDGET_CLASS (e_week_view_parent_class)->unrealize)
		(*GTK_WIDGET_CLASS (e_week_view_parent_class)->unrealize)(widget);
}

static void
week_view_style_updated (GtkWidget *widget)
{
	if (GTK_WIDGET_CLASS (e_week_view_parent_class)->style_updated)
		(*GTK_WIDGET_CLASS (e_week_view_parent_class)->style_updated) (widget);

	week_view_update_style_settings (E_WEEK_VIEW (widget));
}

static void
week_view_size_allocate (GtkWidget *widget,
                         GtkAllocation *allocation)
{
	EWeekView *week_view;
	GtkAllocation canvas_allocation;
	gdouble old_x2, old_y2, new_x2, new_y2;

	week_view = E_WEEK_VIEW (widget);

	(*GTK_WIDGET_CLASS (e_week_view_parent_class)->size_allocate) (widget, allocation);

	e_week_view_recalc_cell_sizes (week_view);

	/* Set the scroll region of the top canvas to its allocated size. */
	gnome_canvas_get_scroll_region (
		GNOME_CANVAS (week_view->titles_canvas),
		NULL, NULL, &old_x2, &old_y2);
	gtk_widget_get_allocation (
		week_view->titles_canvas, &canvas_allocation);
	new_x2 = canvas_allocation.width - 1;
	new_y2 = canvas_allocation.height - 1;
	if (old_x2 != new_x2 || old_y2 != new_y2)
		gnome_canvas_set_scroll_region (
			GNOME_CANVAS (week_view->titles_canvas),
			0, 0, new_x2, new_y2);

	/* Set the scroll region of the main canvas to its allocated width,
	 * but with the height depending on the number of rows needed. */
	gnome_canvas_get_scroll_region (
		GNOME_CANVAS (week_view->main_canvas),
		NULL, NULL, &old_x2, &old_y2);
	gtk_widget_get_allocation (
		week_view->main_canvas, &canvas_allocation);
	new_x2 = canvas_allocation.width - 1;
	new_y2 = canvas_allocation.height - 1;
	if (old_x2 != new_x2 || old_y2 != new_y2)
		gnome_canvas_set_scroll_region (
			GNOME_CANVAS (week_view->main_canvas),
			0, 0, new_x2, new_y2);

	/* Flag that we need to reshape the events. */
	if (old_x2 != new_x2 || old_y2 != new_y2) {
		week_view->events_need_reshape = TRUE;
		e_week_view_check_layout (week_view);
	}
}

static gint
week_view_focus_in (GtkWidget *widget,
                    GdkEventFocus *event)
{
	EWeekView *week_view;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (E_IS_WEEK_VIEW (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	week_view = E_WEEK_VIEW (widget);

	/* XXX Can't access flags directly anymore, but is it really needed?
	 *     If so, could we call gtk_widget_send_focus_change() instead? */
#if 0
	GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
#endif

	if (E_CALENDAR_VIEW (week_view)->in_focus && week_view->requires_update) {
		time_t my_start = 0, my_end = 0, model_start = 0, model_end = 0;

		week_view->requires_update = FALSE;

		e_cal_model_get_time_range (e_calendar_view_get_model (E_CALENDAR_VIEW (week_view)), &model_start, &model_end);

		if (e_calendar_view_get_visible_time_range (E_CALENDAR_VIEW (week_view), &my_start, &my_end) &&
		    model_start == my_start && model_end == my_end) {
			/* update only when the same time range is set in a view and in a model;
			 * otherwise time range change invokes also query update */
			e_week_view_update_query (week_view);
		}
	}

	gtk_widget_queue_draw (week_view->main_canvas);

	return FALSE;
}

static gint
week_view_focus_out (GtkWidget *widget,
                     GdkEventFocus *event)
{
	EWeekView *week_view;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (E_IS_WEEK_VIEW (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	week_view = E_WEEK_VIEW (widget);

	/* XXX Can't access flags directly anymore, but is it really needed?
	 *     If so, could we call gtk_widget_send_focus_change() instead? */
#if 0
	GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);
#endif

	gtk_widget_queue_draw (week_view->main_canvas);

	return FALSE;
}

static gboolean
week_view_key_press (GtkWidget *widget,
                     GdkEventKey *event)
{
	gboolean handled = FALSE;
	handled = e_week_view_do_key_press (widget, event);

	/* if not handled, try key bindings */
	if (!handled)
		handled = GTK_WIDGET_CLASS (e_week_view_parent_class)->key_press_event (widget, event);
	return handled;
}

static gboolean
week_view_focus (GtkWidget *widget,
                 GtkDirectionType direction)
{
	EWeekView *week_view;
	gint new_event_num;
	gint new_span_num;
	gint event_loop;
	gboolean editable = FALSE;
	static gint last_focus_event_num = -1, last_focus_span_num = -1;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (E_IS_WEEK_VIEW (widget), FALSE);

	week_view = E_WEEK_VIEW (widget);

	e_week_view_check_layout (week_view);

	if (week_view->focused_jump_button == E_WEEK_VIEW_JUMP_BUTTON_NO_FOCUS) {
		last_focus_event_num = week_view->editing_event_num;
		last_focus_span_num = week_view->editing_span_num;
	}

	/* if there is not event, just grab week_view */
	if (week_view->events->len == 0) {
		gtk_widget_grab_focus (widget);
		return TRUE;
	}

	for (event_loop = 0; event_loop < week_view->events->len;
	     ++event_loop) {
		if (!e_week_view_get_next_tab_event (week_view, direction,
						     last_focus_event_num,
						     last_focus_span_num,
						     &new_event_num,
						     &new_span_num))
			return FALSE;

		if (new_event_num == -1) {
			/* focus should go to week_view widget
			 */
			gtk_widget_grab_focus (widget);
			return TRUE;
		}

		editable = e_week_view_start_editing_event (
			week_view,
			new_event_num,
			new_span_num,
			NULL);
		last_focus_event_num = new_event_num;
		last_focus_span_num = new_span_num;

		if (editable)
			break;
		else {
			/* check if we should go to the jump button */

			EWeekViewEvent *event;
			EWeekViewEventSpan *span;
			gint current_day;

			if (!is_array_index_in_bounds (week_view->events, new_event_num))
				break;

			event = &g_array_index (week_view->events,
						EWeekViewEvent,
						new_event_num);

			if (!is_array_index_in_bounds (week_view->spans, event->spans_index + new_span_num))
				break;

			span = &g_array_index (week_view->spans,
					       EWeekViewEventSpan,
					       event->spans_index + new_span_num);
			current_day = span->start_day;

			if ((week_view->focused_jump_button != current_day) &&
			    e_week_view_is_jump_button_visible (week_view, current_day)) {

				/* focus go to the jump button */
				e_week_view_stop_editing_event (week_view);
				gnome_canvas_item_grab_focus (week_view->jump_buttons[current_day]);
				return TRUE;
			}
		}
	}
	return editable;
}

static gboolean
week_view_popup_menu (GtkWidget *widget)
{
	EWeekView *week_view = E_WEEK_VIEW (widget);

	e_week_view_show_popup_menu (
		week_view, NULL,
		week_view->editing_event_num);

	return TRUE;
}

static GList *
week_view_get_selected_events (ECalendarView *cal_view)
{
	EWeekViewEvent *event = NULL;
	GList *list = NULL;
	EWeekView *week_view = (EWeekView *) cal_view;

	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), NULL);

	if (week_view->editing_event_num != -1) {
		if (!is_array_index_in_bounds (week_view->events, week_view->editing_event_num)) {
			week_view->editing_event_num = -1;
			g_object_notify (G_OBJECT (week_view), "is-editing");
			return NULL;
		}

		event = &g_array_index (week_view->events, EWeekViewEvent,
					week_view->editing_event_num);
	} else if (week_view->popup_event_num != -1) {
		if (!is_array_index_in_bounds (week_view->events, week_view->popup_event_num))
			return NULL;

		event = &g_array_index (week_view->events, EWeekViewEvent,
					week_view->popup_event_num);
	}

	if (event)
		list = g_list_prepend (list, event);

	return list;
}

static gboolean
week_view_get_selected_time_range (ECalendarView *cal_view,
                                   time_t *start_time,
                                   time_t *end_time)
{
	gint start_day, end_day;
	EWeekView *week_view = E_WEEK_VIEW (cal_view);

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

	return  TRUE;
}

/* This sets the selected time range. The EWeekView will show the corresponding
 * month and the days between start_time and end_time will be selected.
 * To select a single day, use the same value for start_time & end_time. */
static void
week_view_set_selected_time_range (ECalendarView *cal_view,
                                   time_t start_time,
                                   time_t end_time)
{
	GDate date, end_date;
	gint num_days;
	EWeekView *week_view = E_WEEK_VIEW (cal_view);

	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	if (!g_date_valid (&week_view->base_date)) {
		/* This view has not been initialized/shown yet, thus skip this. */
		return;
	}

	time_to_gdate_with_zone (&date, start_time, e_calendar_view_get_timezone (E_CALENDAR_VIEW (week_view)));

	/* Set the selection to the given days. */
	week_view->selection_start_day = g_date_get_julian (&date)
		- g_date_get_julian (&week_view->base_date);
	if (end_time == start_time
	    || end_time <= time_add_day_with_zone (start_time, 1,
						   e_calendar_view_get_timezone (E_CALENDAR_VIEW (week_view))))
		week_view->selection_end_day = week_view->selection_start_day;
	else {
		time_to_gdate_with_zone (&end_date, end_time - 60, e_calendar_view_get_timezone (E_CALENDAR_VIEW (week_view)));
		week_view->selection_end_day = g_date_get_julian (&end_date)
			- g_date_get_julian (&week_view->base_date);
	}

	/* Make sure the selection is valid. */
	num_days = (e_week_view_get_weeks_shown (week_view) * 7) - 1;
	week_view->selection_start_day = CLAMP (
		week_view->selection_start_day, 0, num_days);
	week_view->selection_end_day = CLAMP (
		week_view->selection_end_day,
		week_view->selection_start_day,
		num_days);

	gtk_widget_queue_draw (week_view->main_canvas);
}

static gboolean
week_view_get_visible_time_range (ECalendarView *cal_view,
                                  time_t *start_time,
                                  time_t *end_time)
{
	gint num_days;
	EWeekView *week_view = E_WEEK_VIEW (cal_view);

	/* If we don't have a valid date set yet, return FALSE. */
	if (!g_date_valid (&week_view->priv->first_day_shown))
		return FALSE;

	num_days = e_week_view_get_weeks_shown (week_view) * 7;

	*start_time = week_view->day_starts[0];
	*end_time = week_view->day_starts[num_days];

	return TRUE;
}

static void
week_view_paste_text (ECalendarView *cal_view)
{
	EWeekViewEvent *event;
	EWeekViewEventSpan *span;
	EWeekView *week_view;

	g_return_if_fail (E_IS_WEEK_VIEW (cal_view));

	week_view = E_WEEK_VIEW (cal_view);

	if (week_view->editing_event_num == -1) {
		e_week_view_add_new_event_in_selected_range (week_view, NULL, TRUE);
		return;
	}

	if (!is_array_index_in_bounds (week_view->events, week_view->editing_event_num))
		return;

	event = &g_array_index (week_view->events, EWeekViewEvent,
				week_view->editing_event_num);

	if (!is_array_index_in_bounds (week_view->spans, event->spans_index + week_view->editing_span_num))
		return;

	span = &g_array_index (week_view->spans, EWeekViewEventSpan,
			       event->spans_index + week_view->editing_span_num);

	if (span->text_item &&
	    E_IS_TEXT (span->text_item) &&
	    E_TEXT (span->text_item)->editing) {
		e_text_paste_clipboard (E_TEXT (span->text_item));
	}
}

static void
week_view_cursor_key_up (EWeekView *week_view)
{
	if (week_view->selection_start_day == -1)
		return;

	week_view->selection_start_day--;

	if (week_view->selection_start_day < 0) {
		e_week_view_scroll_a_step (week_view, E_CAL_VIEW_MOVE_UP);
		week_view->selection_start_day = 6;
	}

	week_view->selection_end_day = week_view->selection_start_day;
	g_signal_emit_by_name (week_view, "selected_time_changed");
	gtk_widget_queue_draw (week_view->main_canvas);
}

static void
week_view_cursor_key_down (EWeekView *week_view)
{
	if (week_view->selection_start_day == -1)
		return;

	week_view->selection_start_day++;

	if (week_view->selection_start_day > 6) {
		e_week_view_scroll_a_step (week_view, E_CAL_VIEW_MOVE_DOWN);
		week_view->selection_start_day = 0;
	}

	week_view->selection_end_day = week_view->selection_start_day;
	g_signal_emit_by_name (week_view, "selected_time_changed");
	gtk_widget_queue_draw (week_view->main_canvas);
}

static void
week_view_cursor_key_left (EWeekView *week_view)
{
	if (week_view->selection_start_day == -1)
		return;

	week_view->selection_start_day = map_left[week_view->selection_start_day];
	week_view->selection_end_day = week_view->selection_start_day;
	g_signal_emit_by_name (week_view, "selected_time_changed");
	gtk_widget_queue_draw (week_view->main_canvas);
}

static void
week_view_cursor_key_right (EWeekView *week_view)
{
	if (week_view->selection_start_day == -1)
		return;

	week_view->selection_start_day = map_right[week_view->selection_start_day];
	week_view->selection_end_day = week_view->selection_start_day;
	g_signal_emit_by_name (week_view, "selected_time_changed");
	gtk_widget_queue_draw (week_view->main_canvas);
}

static void
e_week_view_class_init (EWeekViewClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	ECalendarViewClass *view_class;

	g_type_class_add_private (class, sizeof (EWeekViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = week_view_set_property;
	object_class->get_property = week_view_get_property;
	object_class->dispose = week_view_dispose;
	object_class->constructed = week_view_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->realize = week_view_realize;
	widget_class->unrealize = week_view_unrealize;
	widget_class->style_updated = week_view_style_updated;
	widget_class->size_allocate = week_view_size_allocate;
	widget_class->focus_in_event = week_view_focus_in;
	widget_class->focus_out_event = week_view_focus_out;
	widget_class->key_press_event = week_view_key_press;
	widget_class->focus = week_view_focus;
	widget_class->popup_menu = week_view_popup_menu;

	view_class = E_CALENDAR_VIEW_CLASS (class);
	view_class->get_selected_events = week_view_get_selected_events;
	view_class->get_selected_time_range = week_view_get_selected_time_range;
	view_class->set_selected_time_range = week_view_set_selected_time_range;
	view_class->get_visible_time_range = week_view_get_visible_time_range;
	view_class->precalc_visible_time_range = e_week_view_precalc_visible_time_range;
	view_class->paste_text = week_view_paste_text;

	class->cursor_key_up = week_view_cursor_key_up;
	class->cursor_key_down = week_view_cursor_key_down;
	class->cursor_key_left = week_view_cursor_key_left;
	class->cursor_key_right = week_view_cursor_key_right;

	/* XXX This property really belongs in EMonthView,
	 *     but too much drawing code is tied to it. */
	g_object_class_install_property (
		object_class,
		PROP_COMPRESS_WEEKEND,
		g_param_spec_boolean (
			"compress-weekend",
			"Compress Weekend",
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_DAYS_LEFT_TO_RIGHT,
		g_param_spec_boolean (
			"days-left-to-right",
			"Days Left To Right",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_DRAW_FLAT_EVENTS,
		g_param_spec_boolean (
			"draw-flat-events",
			"Draw Flat Events",
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_EVENT_END_TIMES,
		g_param_spec_boolean (
			"show-event-end-times",
			"Show Event End Times",
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_ICONS_MONTH_VIEW,
		g_param_spec_boolean (
			"show-icons-month-view",
			"Show Icons Month View",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_override_property (
		object_class,
		PROP_IS_EDITING,
		"is-editing");

	/* init the accessibility support for e_week_view */
	gtk_widget_class_set_accessible_type (widget_class, EA_TYPE_WEEK_VIEW);
}

static void
e_week_view_init (EWeekView *week_view)
{
	GnomeCanvasGroup *canvas_group;
	GtkAdjustment *adjustment;
	GdkPixbuf *pixbuf;
	gint i;

	week_view->priv = E_WEEK_VIEW_GET_PRIVATE (week_view);
	week_view->priv->weeks_shown = 6;
	week_view->priv->compress_weekend = TRUE;
	week_view->priv->days_left_to_right = FALSE;
	week_view->priv->draw_flat_events = TRUE;
	week_view->priv->show_event_end_times = TRUE;
	week_view->priv->update_base_date = TRUE;
	week_view->priv->display_start_day = G_DATE_MONDAY;

	gtk_widget_set_can_focus (GTK_WIDGET (week_view), TRUE);

	week_view->event_destroyed = FALSE;
	week_view->events = g_array_new (
		FALSE, FALSE,
		sizeof (EWeekViewEvent));
	week_view->events_sorted = TRUE;
	week_view->events_need_layout = FALSE;
	week_view->events_need_reshape = FALSE;

	week_view->layout_timeout_id = 0;

	week_view->spans = NULL;

	week_view->month_scroll_by_week = FALSE;
	week_view->scroll_by_week_notif_id = 0;
	week_view->rows = 6;
	week_view->columns = 2;

	g_date_clear (&week_view->base_date, 1);
	g_date_clear (&week_view->priv->first_day_shown, 1);

	week_view->row_height = 10;
	week_view->rows_per_cell = 1;

	week_view->selection_start_day = -1;
	week_view->selection_drag_pos = E_WEEK_VIEW_DRAG_NONE;

	week_view->pressed_event_num = -1;
	week_view->editing_event_num = -1;

	week_view->last_edited_comp_string = NULL;

	/* Create the small font in constructed. */
	week_view->use_small_font = TRUE;
	week_view->small_font_desc = NULL;

	/* String to use in 12-hour time format for times in the morning. */
	week_view->am_string = _("am");

	/* String to use in 12-hour time format for times in the afternoon. */
	week_view->pm_string = _("pm");

	week_view->bc_event_time = 0;
	week_view->before_click_dtstart = 0;
	week_view->before_click_dtend = 0;

	gtk_widget_set_margin_top (GTK_WIDGET (week_view), 1);

	/*
	 * Titles Canvas. Note that we don't show it is only shown in the
	 * Month view.
	 */
	week_view->titles_canvas = e_canvas_new ();
	gtk_grid_attach (GTK_GRID (week_view), week_view->titles_canvas, 1, 0, 1, 1);
	g_object_set (G_OBJECT (week_view->titles_canvas),
		"hexpand", TRUE,
		"vexpand", FALSE,
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_FILL,
		NULL);

	canvas_group = GNOME_CANVAS_GROUP (GNOME_CANVAS (week_view->titles_canvas)->root);

	week_view->titles_canvas_item =
		gnome_canvas_item_new (
			canvas_group,
			e_week_view_titles_item_get_type (),
			"EWeekViewTitlesItem::week_view", week_view,
			NULL);

	/*
	 * Main Canvas
	 */
	week_view->main_canvas = e_canvas_new ();
	gtk_grid_attach (GTK_GRID (week_view), week_view->main_canvas, 1, 1, 1, 1);
	g_object_set (G_OBJECT (week_view->main_canvas),
		"hexpand", TRUE,
		"vexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_FILL,
		NULL);
	gtk_widget_show (week_view->main_canvas);

	canvas_group = GNOME_CANVAS_GROUP (GNOME_CANVAS (week_view->main_canvas)->root);

	week_view->main_canvas_item =
		gnome_canvas_item_new (
			canvas_group,
			e_week_view_main_item_get_type (),
			"EWeekViewMainItem::week_view", week_view,
			NULL);

	g_signal_connect_after (
		week_view->main_canvas, "button_press_event",
		G_CALLBACK (e_week_view_on_button_press), week_view);
	g_signal_connect (
		week_view->main_canvas, "button_release_event",
		G_CALLBACK (e_week_view_on_button_release), week_view);
	g_signal_connect (
		week_view->main_canvas, "scroll_event",
		G_CALLBACK (e_week_view_on_scroll), week_view);
	g_signal_connect (
		week_view->main_canvas, "motion_notify_event",
		G_CALLBACK (e_week_view_on_motion), week_view);

	/* Create the buttons to jump to each days. */
	pixbuf = gdk_pixbuf_new_from_xpm_data ((const gchar **) jump_xpm);

	for (i = 0; i < E_WEEK_VIEW_MAX_WEEKS * 7; i++) {
		week_view->jump_buttons[i] = gnome_canvas_item_new
			(canvas_group,
			 gnome_canvas_pixbuf_get_type (),
			 "GnomeCanvasPixbuf::pixbuf", pixbuf,
			 NULL);

		g_signal_connect (
			week_view->jump_buttons[i], "event",
			G_CALLBACK (e_week_view_on_jump_button_event), week_view);
	}
	week_view->focused_jump_button = E_WEEK_VIEW_JUMP_BUTTON_NO_FOCUS;

	g_object_unref (pixbuf);

	/*
	 * Scrollbar.
	 */
	adjustment = GTK_ADJUSTMENT (gtk_adjustment_new (0, -52, 52, 1, 1, 1));

	week_view->vscrollbar = gtk_scrollbar_new (
		GTK_ORIENTATION_VERTICAL, adjustment);
	gtk_grid_attach (GTK_GRID (week_view), week_view->vscrollbar, 2, 1, 1, 1);
	g_object_set (G_OBJECT (week_view->vscrollbar),
		"hexpand", FALSE,
		"vexpand", TRUE,
		"halign", GTK_ALIGN_START,
		"valign", GTK_ALIGN_FILL,
		NULL);
	gtk_widget_show (week_view->vscrollbar);

	/* Create the cursors. */
	week_view->normal_cursor = gdk_cursor_new (GDK_LEFT_PTR);
	week_view->move_cursor = gdk_cursor_new (GDK_FLEUR);
	week_view->resize_width_cursor = gdk_cursor_new (GDK_SB_H_DOUBLE_ARROW);
	week_view->last_cursor_set = NULL;

	week_view->requires_update = FALSE;
}

/**
 * e_week_view_new:
 * @Returns: a new #EWeekView.
 *
 * Creates a new #EWeekView.
 **/
ECalendarView *
e_week_view_new (ECalModel *model)
{
	ECalendarView *view;
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	view = g_object_new (E_TYPE_WEEK_VIEW, "model", model, NULL);

	g_signal_connect (
		model, "timezone_changed",
		G_CALLBACK (timezone_changed_cb), view);

	return view;
}

static GdkColor
color_inc (GdkColor c,
           gint amount)
{
	#define dec(x) \
		if (x + amount >= 0 \
		    && x + amount <= 0xFFFF) \
			x += amount; \
		else if (amount <= 0) \
			x = 0; \
		else \
			x = 0xFFFF;

	dec (c.red);
	dec (c.green);
	dec (c.blue);

	#undef dec

	return c;
}

static void
e_week_view_set_colors (EWeekView *week_view)
{
	GtkWidget *widget = GTK_WIDGET (week_view);
	GdkRGBA base_bg, bg_bg, text_fg, selected_bg, selected_fg, unfocused_selected_bg, dark_bg, light_bg;

	e_utils_get_theme_color (widget, "theme_base_color", E_UTILS_DEFAULT_THEME_BASE_COLOR, &base_bg);
	e_utils_get_theme_color (widget, "theme_bg_color", E_UTILS_DEFAULT_THEME_BG_COLOR, &bg_bg);
	e_utils_get_theme_color (widget, "theme_text_color,theme_fg_color", E_UTILS_DEFAULT_THEME_TEXT_COLOR, &text_fg);
	e_utils_get_theme_color (widget, "theme_selected_bg_color", E_UTILS_DEFAULT_THEME_SELECTED_BG_COLOR, &selected_bg);
	e_utils_get_theme_color (widget, "theme_selected_fg_color", E_UTILS_DEFAULT_THEME_SELECTED_FG_COLOR, &selected_fg);
	e_utils_get_theme_color (widget, "theme_unfocused_selected_bg_color,theme_selected_bg_color", E_UTILS_DEFAULT_THEME_UNFOCUSED_SELECTED_BG_COLOR, &unfocused_selected_bg);

	e_utils_shade_color (&bg_bg, &dark_bg, E_UTILS_DARKNESS_MULT);
	e_utils_shade_color (&bg_bg, &light_bg, E_UTILS_LIGHTNESS_MULT);

	e_rgba_to_color (&bg_bg, &week_view->colors[E_WEEK_VIEW_COLOR_EVEN_MONTHS]);
	e_rgba_to_color (&base_bg, &week_view->colors[E_WEEK_VIEW_COLOR_ODD_MONTHS]);
	e_rgba_to_color (&base_bg, &week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BACKGROUND]);
	e_rgba_to_color (&dark_bg, &week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BORDER]);
	e_rgba_to_color (&text_fg, &week_view->colors[E_WEEK_VIEW_COLOR_EVENT_TEXT]);
	e_rgba_to_color (&dark_bg, &week_view->colors[E_WEEK_VIEW_COLOR_GRID]);
	e_rgba_to_color (&selected_bg, &week_view->colors[E_WEEK_VIEW_COLOR_SELECTED]);
	e_rgba_to_color (&unfocused_selected_bg, &week_view->colors[E_WEEK_VIEW_COLOR_SELECTED_UNFOCUSSED]);
	e_rgba_to_color (&text_fg, &week_view->colors[E_WEEK_VIEW_COLOR_DATES]);
	e_rgba_to_color (&selected_fg, &week_view->colors[E_WEEK_VIEW_COLOR_DATES_SELECTED]);
	e_rgba_to_color (&selected_bg, &week_view->colors[E_WEEK_VIEW_COLOR_TODAY]);

	week_view->colors[E_WEEK_VIEW_COLOR_TODAY_BACKGROUND] = get_today_background (week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BACKGROUND]);
	week_view->colors[E_WEEK_VIEW_COLOR_MONTH_NONWORKING_DAY] = color_inc (week_view->colors[E_WEEK_VIEW_COLOR_EVEN_MONTHS], -0x0A0A);
}

static void
e_week_view_recalc_cell_sizes (EWeekView *week_view)
{
	gfloat canvas_width, canvas_height, offset;
	gint row, col;
	GtkAllocation allocation;
	GtkWidget *widget;
	gint width, height, time_width;
	PangoContext *pango_context;
	PangoFontMetrics *font_metrics;

	if (e_week_view_get_multi_week_view (week_view)) {
		week_view->rows =
			e_week_view_get_weeks_shown (week_view) * 2;
		week_view->columns =
			e_week_view_get_compress_weekend (week_view) ? 6 : 7;
	} else {
		week_view->rows = 6;
		week_view->columns = 2;
	}

	gtk_widget_get_allocation (week_view->main_canvas, &allocation);

	/* Calculate the column sizes, using floating point so that pixels
	 * get divided evenly. Note that we use one more element than the
	 * number of columns, to make it easy to get the column widths.
	 * We also add one to the width so that the right border of the last
	 * column is off the edge of the displayed area. */
	canvas_width = allocation.width + 1;
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
	canvas_height = allocation.height + 1;
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

	widget = GTK_WIDGET (week_view);

	pango_context = gtk_widget_get_pango_context (widget);
	if (!pango_context)
		return;

	font_metrics = pango_context_get_metrics (
		pango_context, NULL,
		pango_context_get_language (pango_context));

	/* Calculate the number of rows of events in each cell, for the large
	 * cells and the compressed weekend cells. */
	if (e_week_view_get_multi_week_view (week_view)) {
		week_view->events_y_offset = E_WEEK_VIEW_DATE_T_PAD
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
	week_view->rows_per_cell =
		(height * 2 - week_view->events_y_offset) /
		(week_view->row_height + E_WEEK_VIEW_EVENT_Y_SPACING);
	week_view->rows_per_cell = MIN (
		week_view->rows_per_cell,
		E_WEEK_VIEW_MAX_ROWS_PER_CELL);

	week_view->rows_per_compressed_cell =
		(height - week_view->events_y_offset) /
		(week_view->row_height + E_WEEK_VIEW_EVENT_Y_SPACING);
	week_view->rows_per_compressed_cell = MIN (
		week_view->rows_per_compressed_cell,
		E_WEEK_VIEW_MAX_ROWS_PER_CELL);

	/* Determine which time format to use, based on the width of the cells.
	 * We only allow the time to take up about half of the width. */
	width = week_view->col_widths[0];

	time_width = e_week_view_get_time_string_width (week_view);

	week_view->time_format = E_WEEK_VIEW_TIME_NONE;
	if (week_view->use_small_font && week_view->small_font_desc) {
		if (e_week_view_get_show_event_end_times (week_view) &&
		    width / 2 > time_width * 2 + E_WEEK_VIEW_EVENT_TIME_SPACING)
			week_view->time_format = E_WEEK_VIEW_TIME_BOTH_SMALL_MIN;
		else if (width / 2 > time_width)
			week_view->time_format = E_WEEK_VIEW_TIME_START_SMALL_MIN;
	} else {
		if (e_week_view_get_show_event_end_times (week_view) &&
		    width / 2 > time_width * 2 + E_WEEK_VIEW_EVENT_TIME_SPACING)
			week_view->time_format = E_WEEK_VIEW_TIME_BOTH;
		else if (width / 2 > time_width)
			week_view->time_format = E_WEEK_VIEW_TIME_START;
	}

	pango_font_metrics_unref (font_metrics);
}

/**
 * e_week_view_get_next_tab_event
 * @week_view: the week_view widget operate on
 * @direction: GTK_DIR_TAB_BACKWARD or GTK_DIR_TAB_FORWARD.
 * @current_event_num and @current_span_num: current status.
 * @next_event_num: the event number focus should go next.
 *                  -1 indicates focus should go to week_view widget.
 * @next_span_num: always return 0.
 **/
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

	if (event_num == -1)
		/* backward, out of event range, go to week view widget
		 */
		*next_event_num = -1;
	else if (event_num < -1)
		/* backward from week_view, go to the last event
		 */
		*next_event_num = week_view->events->len - 1;
	else if (event_num >= week_view->events->len)
		/* forward, out of event range, go to week view widget
		 */
		*next_event_num = -1;
	else
		*next_event_num = event_num;
	return TRUE;
}

/* Restarts a query for the week view */
static void
e_week_view_update_query (EWeekView *week_view)
{
	gint rows, r;

	if (!E_CALENDAR_VIEW (week_view)->in_focus) {
		e_week_view_free_events (week_view);
		week_view->requires_update = TRUE;
		return;
	}

	gtk_widget_queue_draw (week_view->main_canvas);
	e_week_view_free_events (week_view);
	e_week_view_queue_layout (week_view);

	rows = e_table_model_row_count (E_TABLE_MODEL (e_calendar_view_get_model (E_CALENDAR_VIEW (week_view))));
	for (r = 0; r < rows; r++) {
		ECalModelComponent *comp_data;

		comp_data = e_cal_model_get_component_at (e_calendar_view_get_model (E_CALENDAR_VIEW (week_view)), r);
		if (comp_data == NULL) {
			g_warning ("comp_data is NULL\n");
			continue;
		}
		week_view_process_component (week_view, comp_data);
	}
}

void
e_week_view_set_selected_time_range_visible (EWeekView *week_view,
                                             time_t start_time,
                                             time_t end_time)
{
	GDate *first_day_shown;
	GDate date, end_date;
	gint num_days;

	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	first_day_shown = &week_view->priv->first_day_shown;

	time_to_gdate_with_zone (&date, start_time, e_calendar_view_get_timezone (E_CALENDAR_VIEW (week_view)));

	/* Set the selection to the given days. */
	week_view->selection_start_day =
		g_date_get_julian (&date) -
		g_date_get_julian (first_day_shown);
	if (end_time == start_time
	    || end_time <= time_add_day_with_zone (start_time, 1,
						   e_calendar_view_get_timezone (E_CALENDAR_VIEW (week_view))))
		week_view->selection_end_day = week_view->selection_start_day;
	else {
		time_to_gdate_with_zone (&end_date, end_time - 60, e_calendar_view_get_timezone (E_CALENDAR_VIEW (week_view)));
		week_view->selection_end_day =
			g_date_get_julian (&end_date) -
			g_date_get_julian (first_day_shown);
	}

	/* Make sure the selection is valid. */
	num_days = (e_week_view_get_weeks_shown (week_view) * 7) - 1;
	week_view->selection_start_day = CLAMP (
		week_view->selection_start_day, 0, num_days);
	week_view->selection_end_day = CLAMP (
		week_view->selection_end_day,
		week_view->selection_start_day,
		num_days);

	gtk_widget_queue_draw (week_view->main_canvas);
}

/* Note that the returned date may be invalid if no date has been set yet. */
void
e_week_view_get_first_day_shown (EWeekView *week_view,
                                 GDate *date)
{
	*date = week_view->priv->first_day_shown;
}

/* This sets the first day shown in the view. It will be rounded down to the
 * nearest week. */
void
e_week_view_set_first_day_shown (EWeekView *week_view,
                                 GDate *date)
{
	GDate base_date;
	GDateWeekday weekday;
	GDateWeekday display_start_day;
	guint day_offset;
	gint num_days;
	gboolean update_adjustment_value = FALSE;
	guint32 old_selection_start_julian = 0, old_selection_end_julian = 0;
	struct icaltimetype start_tt = icaltime_null_time ();
	time_t start_time;

	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	/* Calculate the old selection range. */
	if (week_view->selection_start_day != -1) {
		old_selection_start_julian =
			g_date_get_julian (&week_view->base_date)
			+ week_view->selection_start_day;
		old_selection_end_julian =
			g_date_get_julian (&week_view->base_date)
			+ week_view->selection_end_day;
	}

	weekday = g_date_get_weekday (date);
	display_start_day = e_week_view_get_display_start_day (week_view);

	/* Convert it to an offset from the start of the display. */
	day_offset = e_weekday_get_days_between (display_start_day, weekday);

	/* Calculate the base date, i.e. the first day shown when the
	 * scrollbar adjustment value is 0. */
	base_date = *date;
	g_date_subtract_days (&base_date, day_offset);

	/* See if we need to update the base date. */
	if (!g_date_valid (&week_view->base_date)
	    || g_date_compare (&week_view->base_date, &base_date)) {
		week_view->base_date = base_date;
		update_adjustment_value = TRUE;
	}

	/* See if we need to update the first day shown. */
	if (!g_date_valid (&week_view->priv->first_day_shown)
	    || g_date_compare (&week_view->priv->first_day_shown, &base_date)) {
		week_view->priv->first_day_shown = base_date;

		start_tt.year = g_date_get_year (&base_date);
		start_tt.month = g_date_get_month (&base_date);
		start_tt.day = g_date_get_day (&base_date);

		start_time = icaltime_as_timet_with_zone (
			start_tt,
			e_calendar_view_get_timezone (E_CALENDAR_VIEW (week_view)));

		e_week_view_recalc_day_starts (week_view, start_time);
		e_week_view_update_query (week_view);
	}

	/* Try to keep the previous selection, but if it is no longer shown
	 * just select the first day. */
	if (week_view->selection_start_day != -1) {
		week_view->selection_start_day = old_selection_start_julian
			- g_date_get_julian (&base_date);
		week_view->selection_end_day = old_selection_end_julian
			- g_date_get_julian (&base_date);

		/* Make sure the selection is valid. */
		num_days = (e_week_view_get_weeks_shown (week_view) * 7) - 1;
		week_view->selection_start_day = CLAMP (
			week_view->selection_start_day, 0, num_days);
		week_view->selection_end_day = CLAMP (
			week_view->selection_end_day,
			week_view->selection_start_day,
			num_days);
	}

	/* Reset the adjustment value to 0 if the base address has changed.
	 * Note that we do this after updating first_day_shown so that our
	 * signal handler will not try to reload the events. */
	if (update_adjustment_value) {
		GtkRange *range;
		GtkAdjustment *adjustment;

		range = GTK_RANGE (week_view->vscrollbar);
		adjustment = gtk_range_get_adjustment (range);
		gtk_adjustment_set_value (adjustment, 0);
	}

	e_week_view_update_query (week_view);
	gtk_widget_queue_draw (week_view->main_canvas);
}

GDateWeekday
e_week_view_get_display_start_day (EWeekView *week_view)
{
	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), G_DATE_BAD_WEEKDAY);

	return week_view->priv->display_start_day;
}

/* Recalculates the time_t corresponding to the start of each day. */
static void
e_week_view_recalc_day_starts (EWeekView *week_view,
                               time_t lower)
{
	gint num_days, day;
	time_t tmp_time;

	num_days = E_WEEK_VIEW_MAX_WEEKS * 7;

	tmp_time = lower;
	week_view->day_starts[0] = tmp_time;
	for (day = 1; day <= num_days; day++) {
		tmp_time = time_add_day_with_zone (
			tmp_time, 1,
			e_calendar_view_get_timezone (E_CALENDAR_VIEW (week_view)));
		week_view->day_starts[day] = tmp_time;
	}
}

gboolean
e_week_view_get_multi_week_view (EWeekView *week_view)
{
	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), FALSE);

	return week_view->priv->multi_week_view;
}

void
e_week_view_set_multi_week_view (EWeekView *week_view,
                                 gboolean multi_week_view)
{
	GtkRange *range;
	GtkAdjustment *adjustment;
	gint page_increment, page_size;

	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	if (multi_week_view == week_view->priv->multi_week_view)
		return;

	week_view->priv->multi_week_view = multi_week_view;

	if (multi_week_view) {
		gtk_widget_show (week_view->titles_canvas);
		week_view->month_scroll_by_week = calendar_config_get_month_scroll_by_week ();

		calendar_config_add_notification_month_scroll_by_week (
			month_scroll_by_week_changed_cb, week_view);

		if (week_view->month_scroll_by_week) {
			page_increment = 1;
			page_size = 5;
		} else {
			page_increment = 4;
			page_size = 5;
		}
	} else {
		gtk_widget_hide (week_view->titles_canvas);
		page_increment = page_size = 1;

		if (week_view->scroll_by_week_notif_id) {
			calendar_config_remove_notification (
				month_scroll_by_week_changed_cb, week_view);
			week_view->scroll_by_week_notif_id = 0;
		}
	}

	range = GTK_RANGE (week_view->vscrollbar);
	adjustment = gtk_range_get_adjustment (range);
	gtk_adjustment_set_page_increment (adjustment, page_increment);
	gtk_adjustment_set_page_size (adjustment, page_size);

	e_week_view_recalc_display_start_day (week_view);
	e_week_view_recalc_cell_sizes (week_view);

	if (g_date_valid (&week_view->priv->first_day_shown))
		e_week_view_set_first_day_shown (
			week_view,
			&week_view->priv->first_day_shown);
}

gboolean
e_week_view_get_update_base_date (EWeekView *week_view)
{
	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), FALSE);

	return week_view->priv->update_base_date;
}

void
e_week_view_set_update_base_date (EWeekView *week_view,
                                  gboolean update_base_date)
{
	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	week_view->priv->update_base_date = update_base_date;
}

gint
e_week_view_get_weeks_shown (EWeekView *week_view)
{
	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), 1);

	/* Give a sensible answer for single-week view. */
	if (!e_week_view_get_multi_week_view (week_view))
		return 1;

	return week_view->priv->weeks_shown;
}

void
e_week_view_set_weeks_shown (EWeekView *week_view,
                             gint weeks_shown)
{
	GtkRange *range;
	GtkAdjustment *adjustment;
	gint page_increment, page_size;

	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	weeks_shown = MIN (weeks_shown, E_WEEK_VIEW_MAX_WEEKS);

	if (weeks_shown == week_view->priv->weeks_shown)
		return;

	week_view->priv->weeks_shown = weeks_shown;

	if (e_week_view_get_multi_week_view (week_view)) {
		if (week_view->month_scroll_by_week) {
			page_increment = 1;
			page_size = 5;
		} else {
			page_increment = 4;
			page_size = 5;
		}

		range = GTK_RANGE (week_view->vscrollbar);
		adjustment = gtk_range_get_adjustment (range);
		gtk_adjustment_set_page_increment (adjustment, page_increment);
		gtk_adjustment_set_page_size (adjustment, page_size);

		e_week_view_recalc_cell_sizes (week_view);

		if (g_date_valid (&week_view->priv->first_day_shown))
			e_week_view_set_first_day_shown (
				week_view,
				&week_view->priv->first_day_shown);

		e_week_view_update_query (week_view);
	}
}

gboolean
e_week_view_get_compress_weekend (EWeekView *week_view)
{
	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), FALSE);

	return week_view->priv->compress_weekend;
}

void
e_week_view_set_compress_weekend (EWeekView *week_view,
                                  gboolean compress_weekend)
{
	gboolean need_reload = FALSE;

	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	if (compress_weekend == week_view->priv->compress_weekend)
		return;

	week_view->priv->compress_weekend = compress_weekend;

	/* The option only affects the month view. */
	if (!e_week_view_get_multi_week_view (week_view))
		return;

	e_week_view_recalc_cell_sizes (week_view);

	need_reload = e_week_view_recalc_display_start_day (week_view);

	/* If the display_start_day has changed we need to recalculate the
	 * date range shown and reload all events, otherwise we only need to
	 * do a reshape. */
	if (need_reload) {
		/* Recalculate the days shown and reload if necessary. */
		if (g_date_valid (&week_view->priv->first_day_shown))
			e_week_view_set_first_day_shown (
				week_view,
				&week_view->priv->first_day_shown);
	} else {
		week_view->events_need_reshape = TRUE;
		e_week_view_check_layout (week_view);
	}

	gtk_widget_queue_draw (week_view->titles_canvas);
	gtk_widget_queue_draw (week_view->main_canvas);

	g_object_notify (G_OBJECT (week_view), "compress-weekend");
}

gboolean
e_week_view_get_draw_flat_events (EWeekView *week_view)
{
	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), FALSE);

	return week_view->priv->draw_flat_events;
}

void
e_week_view_set_draw_flat_events (EWeekView *week_view,
				  gboolean draw_flat_events)
{
	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	if ((week_view->priv->draw_flat_events ? 1 : 0) == (draw_flat_events ? 1 : 0))
		return;

	week_view->priv->draw_flat_events = draw_flat_events;

	gtk_widget_queue_draw (week_view->titles_canvas);
	gtk_widget_queue_draw (week_view->main_canvas);

	g_object_notify (G_OBJECT (week_view), "draw-flat-events");
}

gboolean
e_week_view_get_days_left_to_right (EWeekView *week_view)
{
	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), FALSE);

	return week_view->priv->days_left_to_right;
}

void
e_week_view_set_days_left_to_right (EWeekView *week_view,
				    gboolean days_left_to_right)
{
	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	if ((week_view->priv->days_left_to_right ? 1 : 0) == (days_left_to_right ? 1 : 0))
		return;

	week_view->priv->days_left_to_right = days_left_to_right;

	week_view->events_need_layout = TRUE;
	week_view->events_need_reshape = TRUE;

	gtk_widget_queue_draw (week_view->main_canvas);
	e_week_view_queue_layout (week_view);

	g_object_notify (G_OBJECT (week_view), "days-left-to-right");
}

/* Whether we display event end times. */
gboolean
e_week_view_get_show_event_end_times (EWeekView *week_view)
{
	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), TRUE);

	return week_view->priv->show_event_end_times;
}

void
e_week_view_set_show_event_end_times (EWeekView *week_view,
                                      gboolean show_event_end_times)
{
	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	if (show_event_end_times == week_view->priv->show_event_end_times)
		return;

	week_view->priv->show_event_end_times = show_event_end_times;
	e_week_view_recalc_cell_sizes (week_view);
	week_view->events_need_reshape = TRUE;
	e_week_view_check_layout (week_view);

	gtk_widget_queue_draw (week_view->titles_canvas);
	gtk_widget_queue_draw (week_view->main_canvas);

	g_object_notify (G_OBJECT (week_view), "show-event-end-times");
}

gboolean
e_week_view_get_show_icons_month_view (EWeekView *week_view)
{
	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), TRUE);

	return week_view->priv->show_icons_month_view;
}

void
e_week_view_set_show_icons_month_view (EWeekView *week_view,
				       gboolean show_icons_month_view)
{
	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	if (show_icons_month_view == week_view->priv->show_icons_month_view)
		return;

	week_view->priv->show_icons_month_view = show_icons_month_view;

	if (e_week_view_get_multi_week_view (week_view)) {
		e_week_view_recalc_cell_sizes (week_view);
		week_view->events_need_reshape = TRUE;
		e_week_view_check_layout (week_view);

		gtk_widget_queue_draw (week_view->titles_canvas);
		gtk_widget_queue_draw (week_view->main_canvas);
	}

	g_object_notify (G_OBJECT (week_view), "show-icons-month-view");
}

static gboolean
e_week_view_recalc_display_start_day (EWeekView *week_view)
{
	ECalModel *model;
	GDateWeekday week_start_day;
	GDateWeekday display_start_day;
	gboolean changed;

	model = e_calendar_view_get_model (E_CALENDAR_VIEW (week_view));
	week_start_day = e_cal_model_get_week_start_day (model);

	/* The display start day defaults to week_start_day, but we have
	 * to use Saturday if the weekend is compressed and week_start_day
	 * is Sunday. */
	display_start_day = week_start_day;

	if (display_start_day == G_DATE_SUNDAY) {
		if (!e_week_view_get_multi_week_view (week_view))
			display_start_day = G_DATE_SATURDAY;

		if (e_week_view_get_compress_weekend (week_view))
			display_start_day = G_DATE_SATURDAY;
	}

	changed = (display_start_day != week_view->priv->display_start_day);

	week_view->priv->display_start_day = display_start_day;

	return changed;
}

/* Checks if the users participation status is NEEDS-ACTION and shows the summary as bold text */
static void
set_style_from_attendee (EWeekViewEvent *event,
			 EWeekViewEventSpan *span,
			 ESourceRegistry *registry)
{
	ECalComponent *comp;
	GSList *attendees = NULL, *l;
	gchar *address;
	ECalComponentAttendee *at = NULL;

	if (!is_comp_data_valid (event))
		return;

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));
	address = itip_get_comp_attendee (
		registry, comp, event->comp_data->client);
	e_cal_component_get_attendee_list (comp, &attendees);
	for (l = attendees; l; l = l->next) {
		ECalComponentAttendee *attendee = l->data;

		if ((attendee->value && g_strcmp0 (itip_strip_mailto (attendee->value), address) == 0)
		 || (attendee->sentby && g_strcmp0 (itip_strip_mailto (attendee->sentby), address) == 0)) {
			at = attendee;
			break;
		}
	}

	/* The attendee has not yet accepted the meeting, display the summary as bolded.
	 * If the attendee is not present, it might have come through a mailing list.
	 * In that case, we never show the meeting as bold even if it is unaccepted. */
	if (at && at->status == ICAL_PARTSTAT_NEEDSACTION)
		gnome_canvas_item_set (span->text_item, "bold", TRUE, NULL);
	else if (at && at->status == ICAL_PARTSTAT_DECLINED)
		gnome_canvas_item_set (span->text_item, "strikeout", TRUE, NULL);
	else if (at && at->status == ICAL_PARTSTAT_TENTATIVE)
		gnome_canvas_item_set (span->text_item, "italic", TRUE, NULL);
	else if (at && at->status == ICAL_PARTSTAT_DELEGATED)
		gnome_canvas_item_set (span->text_item, "italic", TRUE, "strikeout", TRUE, NULL);

	e_cal_component_free_attendee_list (attendees);
	g_free (address);
	g_object_unref (comp);
}

/* This calls a given function for each event instance that matches the given
 * uid. Note that it is safe for the callback to remove the event (since we
 * step backwards through the arrays). */
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
		const gchar *u;

		event = &g_array_index (week_view->events, EWeekViewEvent,
					event_num);

		if (!is_comp_data_valid (event))
			continue;

		u = icalcomponent_get_uid (event->comp_data->icalcomp);
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

	if (!is_array_index_in_bounds (week_view->events, event_num))
		return TRUE;

	event = &g_array_index (week_view->events, EWeekViewEvent, event_num);
	if (!event)
		return TRUE;

	/* If we were editing this event, set editing_event_num to -1 so
	 * on_editing_stopped doesn't try to update the event. */
	if (week_view->editing_event_num == event_num) {
		week_view->editing_event_num = -1;
		g_object_notify (G_OBJECT (week_view), "is-editing");
	}

	if (week_view->popup_event_num == event_num)
		week_view->popup_event_num = -1;

	if (is_comp_data_valid (event))
		g_object_unref (event->comp_data);
	event->comp_data = NULL;

	if (week_view->spans) {
		/* We leave the span elements in the array, but set the canvas item
		 * pointers to NULL. */
		for (span_num = 0; span_num < event->num_spans; span_num++) {
			if (!is_array_index_in_bounds (week_view->spans, event->spans_index + span_num))
				break;

			span = &g_array_index (week_view->spans, EWeekViewEventSpan,
					       event->spans_index + span_num);

			if (span->text_item) {
				g_object_run_dispose (G_OBJECT (span->text_item));
				span->text_item = NULL;
			}
			if (span->background_item) {
				g_object_run_dispose (G_OBJECT (span->background_item));
				span->background_item = NULL;
			}
		}

		/* Update event_num numbers for already created spans with event_num higher than our event_num */
		for (span_num = 0; span_num < week_view->spans->len; span_num++) {
			span = &g_array_index (week_view->spans, EWeekViewEventSpan, span_num);

			if (span && span->background_item && E_IS_WEEK_VIEW_EVENT_ITEM (span->background_item)) {
				EWeekViewEventItem *wveitem = E_WEEK_VIEW_EVENT_ITEM (span->background_item);
				gint wveitem_event_num;

				wveitem_event_num =
					e_week_view_event_item_get_event_num (wveitem);

				if (wveitem_event_num > event_num)
					e_week_view_event_item_set_event_num (
						wveitem, wveitem_event_num - 1);
			}
		}
	}

	g_array_remove_index (week_view->events, event_num);

	week_view->events_need_layout = TRUE;

	return TRUE;
}

void
e_week_view_get_day_position (EWeekView *week_view,
                              gint day,
                              gint *day_x,
                              gint *day_y,
                              gint *day_w,
                              gint *day_h)
{
	gint cell_x, cell_y, cell_h;

	e_week_view_layout_get_day_position (
		day,
		e_week_view_get_multi_week_view (week_view),
		e_week_view_get_weeks_shown (week_view),
		e_week_view_get_display_start_day (week_view),
		e_week_view_get_compress_weekend (week_view),
		&cell_x, &cell_y, &cell_h);

	*day_x = week_view->col_offsets[cell_x];
	*day_y = week_view->row_offsets[cell_y];

	*day_w = week_view->col_widths[cell_x];
	*day_h = week_view->row_heights[cell_y];

	while (cell_h > 1) {
		*day_h += week_view->row_heights[cell_y + 1];
		cell_h--;
		cell_y++;
	}
}

/* Returns the bounding box for a span of an event. Usually this can easily
 * be determined by the start & end days and row of the span, which are set in
 * e_week_view_layout_event (). Though we need a special case for the weekends
 * when they are compressed, since the span may not fit.
 * The bounding box includes the entire width of the days in the view (but
 * not the vertical line down the right of the last day), though the displayed
 * event doesn't normally extend to the edges of the day.
 * It returns FALSE if the span isn't visible. */
gboolean
e_week_view_get_span_position (EWeekView *week_view,
                               gint event_num,
                               gint span_num,
                               gint *span_x,
                               gint *span_y,
                               gint *span_w)
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

	if (!is_array_index_in_bounds (week_view->spans, event->spans_index + span_num))
		return FALSE;

	span = &g_array_index (week_view->spans, EWeekViewEventSpan,
			       event->spans_index + span_num);

	if (!e_week_view_layout_get_span_position (
		event, span,
		week_view->rows_per_cell,
		week_view->rows_per_compressed_cell,
		e_week_view_get_display_start_day (week_view),
		e_week_view_get_multi_week_view (week_view),
		e_week_view_get_compress_weekend (week_view),
		&num_days)) {
		return FALSE;
	}

	e_week_view_get_day_position (
		week_view, span->start_day,
		&start_x, &start_y, &start_w, &start_h);
	*span_y = start_y + week_view->events_y_offset
		+ span->row * (week_view->row_height
			       + E_WEEK_VIEW_EVENT_Y_SPACING);
	if (num_days == 1) {
		*span_x = start_x;
		*span_w = start_w - 1;
	} else {
		e_week_view_get_day_position (
			week_view,
			span->start_day + num_days - 1,
			&end_x, &end_y, &end_w, &end_h);
		*span_x = start_x;
		*span_w = end_x + end_w - start_x - 1;
	}

	return TRUE;
}

static gboolean
ewv_pass_gdkevent_to_etext (EWeekView *week_view,
                            GdkEvent *gevent)
{
	g_return_val_if_fail (week_view != NULL, FALSE);
	g_return_val_if_fail (gevent != NULL, FALSE);

	if (week_view->editing_event_num != -1 && week_view->editing_span_num != -1) {
		EWeekViewEvent *event;
		EWeekViewEventSpan *span;

		if (!is_array_index_in_bounds (week_view->events, week_view->editing_event_num))
			return FALSE;

		event = &g_array_index (week_view->events, EWeekViewEvent, week_view->editing_event_num);

		if (!is_array_index_in_bounds (week_view->spans, event->spans_index + week_view->editing_span_num))
			return FALSE;

		span = &g_array_index (week_view->spans, EWeekViewEventSpan, event->spans_index + week_view->editing_span_num);

		if (span->text_item && E_IS_TEXT (span->text_item)) {
			gdouble x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0, ex = 0.0, ey = 0.0;

			gdk_event_get_coords (gevent, &ex, &ey);

			gnome_canvas_item_get_bounds (span->text_item, &x1, &y1, &x2, &y2);

			if (ex >= x1 && ex <= x2 && ey >= y1 && ey <= y2) {
				GNOME_CANVAS_ITEM_GET_CLASS (span->text_item)->event (span->text_item, gevent);
				return TRUE;
			}
		}
	}

	return FALSE;
}

static gboolean
e_week_view_on_button_press (GtkWidget *widget,
                             GdkEvent *button_event,
                             EWeekView *week_view)
{
	guint event_button = 0;
	gdouble event_x_win = 0;
	gdouble event_y_win = 0;
	gint x, y, day;

	gdk_event_get_button (button_event, &event_button);
	gdk_event_get_coords (button_event, &event_x_win, &event_y_win);

	/* Convert the mouse position to a week & day. */
	x = (gint) event_x_win;
	y = (gint) event_y_win;
	day = e_week_view_convert_position_to_day (week_view, x, y);
	if (day == -1)
		return FALSE;

	if (ewv_pass_gdkevent_to_etext (week_view, button_event))
		return TRUE;

	/* If an event is pressed just return. */
	if (week_view->pressed_event_num != -1)
		return FALSE;

	e_week_view_stop_editing_event (week_view);

	if (event_button == 1 && button_event->type == GDK_2BUTTON_PRESS) {
		time_t dtstart, dtend;

		e_calendar_view_get_selected_time_range ((ECalendarView *) week_view, &dtstart, &dtend);
		if (dtstart < week_view->before_click_dtend && dtend > week_view->before_click_dtstart) {
			e_calendar_view_set_selected_time_range (
				E_CALENDAR_VIEW (week_view),
				week_view->before_click_dtstart,
				week_view->before_click_dtend);
		}
		e_calendar_view_new_appointment_full (E_CALENDAR_VIEW (week_view), FALSE, calendar_config_get_prefer_meeting (), FALSE);
		return TRUE;
	}

	if (event_button == 1) {
		GdkGrabStatus grab_status;
		GdkWindow *window;
		GdkDevice *event_device;
		guint32 event_time;

		/* Start the selection drag. */
		if (!gtk_widget_has_focus (GTK_WIDGET (week_view)) && !gtk_widget_has_focus (GTK_WIDGET (week_view->main_canvas)))
			gtk_widget_grab_focus (GTK_WIDGET (week_view));

		window = gtk_layout_get_bin_window (GTK_LAYOUT (widget));

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
			if (event_time - week_view->bc_event_time > 250)
				e_calendar_view_get_selected_time_range (
					E_CALENDAR_VIEW (week_view),
					&week_view->before_click_dtstart,
					&week_view->before_click_dtend);
			week_view->bc_event_time = event_time;
			week_view->selection_start_day = day;
			week_view->selection_end_day = day;
			week_view->selection_drag_pos = E_WEEK_VIEW_DRAG_END;
			g_signal_emit_by_name (week_view, "selected_time_changed");

			/* FIXME: Optimise? */
			gtk_widget_queue_draw (week_view->main_canvas);
		}
	} else if (event_button == 3) {
		GnomeCanvasItem *item;
		gint event_num = -1, span_num = -1;

		if (!gtk_widget_has_focus (GTK_WIDGET (week_view)))
			gtk_widget_grab_focus (GTK_WIDGET (week_view));

		if (day < week_view->selection_start_day || day > week_view->selection_end_day) {
			week_view->selection_start_day = day;
			week_view->selection_end_day = day;
			week_view->selection_drag_pos = E_WEEK_VIEW_DRAG_NONE;

			/* FIXME: Optimise? */
			gtk_widget_queue_draw (week_view->main_canvas);
		}

		item = gnome_canvas_get_item_at (GNOME_CANVAS (widget), x, y);
		if (!item || !e_week_view_find_event_from_item (week_view, item, &event_num, &span_num))
			event_num = -1;

		e_week_view_show_popup_menu (week_view, button_event, event_num);
	}

	return TRUE;
}

static gboolean
e_week_view_on_button_release (GtkWidget *widget,
                               GdkEvent *button_event,
                               EWeekView *week_view)
{
	GdkDevice *event_device;
	guint32 event_time;

	event_device = gdk_event_get_device (button_event);
	event_time = gdk_event_get_time (button_event);

	if (week_view->selection_drag_pos != E_WEEK_VIEW_DRAG_NONE) {
		week_view->selection_drag_pos = E_WEEK_VIEW_DRAG_NONE;
		gdk_device_ungrab (event_device, event_time);
	} else {
		ewv_pass_gdkevent_to_etext (week_view, button_event);
	}

	return FALSE;
}

static gboolean
e_week_view_on_scroll (GtkWidget *widget,
                       GdkEventScroll *scroll,
                       EWeekView *week_view)
{
	GtkRange *range;
	GtkAdjustment *adjustment;
	gdouble page_increment;
	gdouble new_value;
	gdouble page_size;
	gdouble lower;
	gdouble upper;
	gdouble value;
	GtkWidget *tool_window = g_object_get_data (G_OBJECT (week_view), "tooltip-window");
	guint timeout;

	timeout = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (week_view), "tooltip-timeout"));
	if (timeout) {
		g_source_remove (timeout);
		g_object_set_data (G_OBJECT (week_view), "tooltip-timeout", NULL);
	}

	if (tool_window) {
		gtk_widget_destroy (tool_window);
		g_object_set_data (G_OBJECT (week_view), "tooltip-window", NULL);
	}

	range = GTK_RANGE (week_view->vscrollbar);
	adjustment = gtk_range_get_adjustment (range);

	page_increment = gtk_adjustment_get_page_increment (adjustment);
	page_size = gtk_adjustment_get_page_size (adjustment);
	lower = gtk_adjustment_get_lower (adjustment);
	upper = gtk_adjustment_get_upper (adjustment);
	value = gtk_adjustment_get_value (adjustment);

	switch (scroll->direction) {
		case GDK_SCROLL_UP:
			new_value = value - page_increment;
			break;
		case GDK_SCROLL_DOWN:
			new_value = value + page_increment;
			break;
		case GDK_SCROLL_SMOOTH:
			if (scroll->delta_y < -0.001 || scroll->delta_y > 0.001) {
				new_value = value + scroll->delta_y * page_increment;
				break;
			}
			return FALSE;
		default:
			return FALSE;
	}

	new_value = CLAMP (new_value, lower, upper - page_size);
	gtk_adjustment_set_value (adjustment, new_value);

	return TRUE;
}

static gboolean
e_week_view_on_motion (GtkWidget *widget,
                       GdkEventMotion *mevent,
                       EWeekView *week_view)
{
	gint x, y, day;

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

	ewv_pass_gdkevent_to_etext (week_view, (GdkEvent *) mevent);

	return FALSE;
}

/* Converts a position in the canvas window to a day offset from the first
 * day displayed. Returns -1 if the position is outside the grid. */
static gint
e_week_view_convert_position_to_day (EWeekView *week_view,
                                     gint x,
                                     gint y)
{
	GDateWeekday display_start_day;
	gint col, row, grid_x = -1, grid_y = -1, week, day;
	gint weekend_col;

	display_start_day = e_week_view_get_display_start_day (week_view);

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
	if (e_week_view_get_multi_week_view (week_view)) {
		week = grid_y / 2;
		day = grid_x;

		if (e_week_view_get_compress_weekend (week_view)) {
			weekend_col = e_weekday_get_days_between (
				display_start_day, G_DATE_SATURDAY);
			if (grid_x > weekend_col
			    || (grid_x == weekend_col && grid_y % 2 == 1))
				day++;
		}
	} else {
		week = 0;

		for (day = 0; day < 7; day++) {
			gint day_x = 0, day_y = 0, rows = 0;
			e_week_view_layout_get_day_position (
				day, FALSE, 1,
				e_week_view_get_display_start_day (week_view),
				e_week_view_get_compress_weekend (week_view),
				&day_x, &day_y, &rows);

			if (grid_x == day_x && grid_y >= day_y && grid_y < day_y + rows)
				break;
		}

		if (day == 7)
			return -1;
	}

	return week * 7 + day;
}

static void
e_week_view_update_selection (EWeekView *week_view,
                              gint day)
{
	gint tmp_day;
	gboolean need_redraw = FALSE;

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
	gboolean did_editing = week_view->editing_event_num != -1;
	guint timeout;

	/* Reset all our indices. */
	week_view->pressed_event_num = -1;
	week_view->pressed_span_num = -1;
	week_view->editing_event_num = -1;
	week_view->editing_span_num = -1;
	week_view->popup_event_num = -1;

	for (event_num = 0; event_num < week_view->events->len; event_num++) {
		event = &g_array_index (week_view->events, EWeekViewEvent,
					event_num);

		if (is_comp_data_valid (event))
			g_object_unref (event->comp_data);
	}

	g_array_set_size (week_view->events, 0);

	/* Destroy all the old canvas items. */
	if (week_view->spans) {
		for (span_num = 0; span_num < week_view->spans->len;
		     span_num++) {
			span = &g_array_index (week_view->spans,
					       EWeekViewEventSpan, span_num);
			if (span->background_item)
				g_object_run_dispose (G_OBJECT (span->background_item));
			if (span->text_item)
				g_object_run_dispose (G_OBJECT (span->text_item));
		}
		g_array_free (week_view->spans, TRUE);
		week_view->spans = NULL;
	}

	/* Clear the number of rows used per day. */
	num_days = e_week_view_get_weeks_shown (week_view) * 7;
	for (day = 0; day <= num_days; day++) {
		week_view->rows_per_day[day] = 0;
	}

	/* Hide all the jump buttons. */
	for (day = 0; day < E_WEEK_VIEW_MAX_WEEKS * 7; day++) {
		gnome_canvas_item_hide (week_view->jump_buttons[day]);
	}

	if (did_editing)
		g_object_notify (G_OBJECT (week_view), "is-editing");

	timeout = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (week_view), "tooltip-timeout"));
	if (timeout) {
		g_source_remove (timeout);
		g_object_set_data (G_OBJECT (week_view), "tooltip-timeout", NULL);
	}
}

/* This adds one event to the view, adding it to the appropriate array. */
static void
e_week_view_add_event (ECalClient *client,
		       ECalComponent *comp,
                       time_t start,
                       time_t end,
                       gboolean prepend,
                       gpointer data)

{
	AddEventData *add_event_data;
	EWeekViewEvent event;
	gint num_days;
	struct icaltimetype start_tt, end_tt;

	add_event_data = data;

	/* Check that the event times are valid. */
	num_days = e_week_view_get_weeks_shown (add_event_data->week_view) * 7;

	g_return_if_fail (start <= end);
	g_return_if_fail (start < add_event_data->week_view->day_starts[num_days]);

	if (end != start || end < add_event_data->week_view->day_starts[0])
		g_return_if_fail (end > add_event_data->week_view->day_starts[0]);

	start_tt = icaltime_from_timet_with_zone (
		start, FALSE,
		e_calendar_view_get_timezone (E_CALENDAR_VIEW (add_event_data->week_view)));
	end_tt = icaltime_from_timet_with_zone (
		end, FALSE,
		e_calendar_view_get_timezone (E_CALENDAR_VIEW (add_event_data->week_view)));

	if (add_event_data->comp_data) {
		event.comp_data = g_object_ref (add_event_data->comp_data);
	} else {
		event.comp_data = g_object_new (E_TYPE_CAL_MODEL_COMPONENT, NULL);
		event.comp_data->is_new_component = TRUE;
		event.comp_data->client = g_object_ref (client);
		e_cal_component_abort_sequence (comp);
		event.comp_data->icalcomp = icalcomponent_new_clone (e_cal_component_get_icalcomponent (comp));
	}
	event.start = start;
	event.end = end;
	event.tooltip = NULL;
	event.timeout = -1;
	event.color = NULL;
	event.spans_index = 0;
	event.num_spans = 0;
	event.comp_data->instance_start = start;
	event.comp_data->instance_end = end;

	event.start_minute = start_tt.hour * 60 + start_tt.minute;
	event.end_minute = end_tt.hour * 60 + end_tt.minute;
	if (event.end_minute == 0 && start != end)
		event.end_minute = 24 * 60;

	event.different_timezone = FALSE;
	if (!cal_comp_util_compare_event_timezones (
		    comp,
		    event.comp_data->client,
		    e_calendar_view_get_timezone (E_CALENDAR_VIEW (add_event_data->week_view))))
		event.different_timezone = TRUE;

	if (prepend)
		g_array_prepend_val (add_event_data->week_view->events, event);
	else
		g_array_append_val (add_event_data->week_view->events, event);
	add_event_data->week_view->events_sorted = FALSE;
	add_event_data->week_view->events_need_layout = TRUE;
}

/* This lays out the events, or reshapes them, as necessary. */
static void
e_week_view_check_layout (EWeekView *week_view)
{
	/* Don't bother if we aren't visible. */
	if (!E_CALENDAR_VIEW (week_view)->in_focus) {
		e_week_view_free_events (week_view);
		week_view->requires_update = TRUE;
		return;
	}

	/* Make sure the events are sorted (by start and size). */
	e_week_view_ensure_events_sorted (week_view);

	if (week_view->events_need_layout)
		week_view->spans = e_week_view_layout_events (
			week_view->events,
			week_view->spans,
			e_week_view_get_multi_week_view (week_view),
			e_week_view_get_weeks_shown (week_view),
			e_week_view_get_compress_weekend (week_view),
			e_week_view_get_display_start_day (week_view),
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
		qsort (
			week_view->events->data,
			week_view->events->len,
			sizeof (EWeekViewEvent),
			e_week_view_event_sort_func);
		week_view->events_sorted = TRUE;
	}
}

gint
e_week_view_event_sort_func (gconstpointer arg1,
                             gconstpointer arg2)
{
	EWeekViewEvent *event1, *event2;

	event1 = (EWeekViewEvent *) arg1;
	event2 = (EWeekViewEvent *) arg2;

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
	GDateWeekday display_start_day;
	gint event_num, span_num;
	gint num_days, day, day_x, day_y, day_w, day_h, max_rows;
	gboolean is_weekend;

	for (event_num = 0; event_num < week_view->events->len; event_num++) {
		event = &g_array_index (week_view->events, EWeekViewEvent,
					event_num);
		if (!is_comp_data_valid (event))
			continue;

		for (span_num = 0; span_num < event->num_spans; span_num++) {
			gchar *current_comp_string;

			e_week_view_reshape_event_span (
				week_view, event_num, span_num);
			if (week_view->last_edited_comp_string == NULL)
				continue;
			current_comp_string = icalcomponent_as_ical_string_r (event->comp_data->icalcomp);
			if (strncmp (current_comp_string, week_view->last_edited_comp_string,50) == 0) {
				EWeekViewEventSpan *span;

				if (!is_array_index_in_bounds (week_view->spans, event->spans_index + span_num)) {
					g_free (current_comp_string);
					continue;
				}

				span = &g_array_index (week_view->spans, EWeekViewEventSpan, event->spans_index + span_num);
				e_canvas_item_grab_focus (span->text_item, TRUE);
				g_free (week_view->last_edited_comp_string);
				week_view->last_edited_comp_string = NULL;
			}
			g_free (current_comp_string);
		}
	}

	/* Reshape the jump buttons and show/hide them as appropriate. */
	num_days = e_week_view_get_weeks_shown (week_view) * 7;
	display_start_day = e_week_view_get_display_start_day (week_view);
	for (day = 0; day < num_days; day++) {
		switch (e_weekday_add_days (display_start_day, day)) {
			case G_DATE_SATURDAY:
			case G_DATE_SUNDAY:
				is_weekend = TRUE;
				break;
			default:
				is_weekend = FALSE;
				break;
		}

		if (!is_weekend || (
		    e_week_view_get_multi_week_view (week_view)
		    && !e_week_view_get_compress_weekend (week_view)))
			max_rows = week_view->rows_per_cell;
		else
			max_rows = week_view->rows_per_compressed_cell;

		/* Determine whether the jump button should be shown. */
		if (week_view->rows_per_day[day] <= max_rows) {
			gnome_canvas_item_hide (week_view->jump_buttons[day]);
		} else {
			cairo_matrix_t matrix;

			e_week_view_get_day_position (
				week_view, day,
				&day_x, &day_y,
				&day_w, &day_h);

			cairo_matrix_init_translate (
				&matrix,
				day_x + day_w - E_WEEK_VIEW_JUMP_BUTTON_X_PAD - E_WEEK_VIEW_JUMP_BUTTON_WIDTH,
				day_y + day_h - E_WEEK_VIEW_JUMP_BUTTON_Y_PAD - E_WEEK_VIEW_JUMP_BUTTON_HEIGHT);
			gnome_canvas_item_set_matrix (week_view->jump_buttons[day], &matrix);

			gnome_canvas_item_show (week_view->jump_buttons[day]);
			gnome_canvas_item_raise_to_top (week_view->jump_buttons[day]);
		}
	}

	for (day = num_days; day < E_WEEK_VIEW_MAX_WEEKS * 7; day++) {
		gnome_canvas_item_hide (week_view->jump_buttons[day]);
	}
}

static EWeekViewEvent *
tooltip_get_view_event (EWeekView *week_view,
                        gint day,
                        gint event_num)
{
	EWeekViewEvent *pevent;

	if (!is_array_index_in_bounds (week_view->events, event_num))
		return NULL;

	pevent = &g_array_index (week_view->events, EWeekViewEvent, event_num);

	return pevent;
}

static void
tooltip_destroy (EWeekView *week_view,
                 GnomeCanvasItem *item)
{
	gint event_num;
	EWeekViewEvent *pevent;
	guint timeout;

	e_week_view_check_layout (week_view);

	event_num = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "event-num"));

	timeout = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (week_view), "tooltip-timeout"));
	if (timeout) {
		g_source_remove (timeout);
		g_object_set_data (G_OBJECT (week_view), "tooltip-timeout", NULL);
	}

	pevent = tooltip_get_view_event (week_view, -1, event_num);
	if (pevent) {
		if (pevent->tooltip && g_object_get_data (G_OBJECT (week_view), "tooltip-window")) {
			gtk_widget_destroy (pevent->tooltip);
			pevent->tooltip = NULL;
		}

		g_object_set_data (G_OBJECT (week_view), "tooltip-window", NULL);
	}
}

static gboolean
e_week_view_handle_tooltip_timeout (gpointer user_data)
{
	ECalendarViewEventData *data = user_data;

	g_return_val_if_fail (data != NULL, FALSE);

	return e_calendar_view_get_tooltips (data);
}

static void
e_week_view_destroy_tooltip_timeout_data (gpointer ptr)
{
	ECalendarViewEventData *data = ptr;

	if (data) {
		g_object_set_data ((GObject *) data->cal_view, "tooltip-timeout", NULL);
		g_object_unref (data->cal_view);
		g_free (data);
	}
}

static gboolean
tooltip_event_cb (GnomeCanvasItem *item,
                  GdkEvent *event,
                  EWeekView *view)
{
	gint event_num;
	EWeekViewEvent *pevent;

	e_week_view_check_layout (view);

	event_num = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "event-num"));
	pevent = tooltip_get_view_event (view, -1, event_num);

	switch (event->type) {
		case GDK_ENTER_NOTIFY:
		if (view->editing_event_num == -1) {
			ECalendarViewEventData *data;

			g_return_val_if_fail (pevent != NULL, FALSE);
			data = g_malloc (sizeof (ECalendarViewEventData));

			pevent->x = ((GdkEventCrossing *) event)->x_root;
			pevent->y = ((GdkEventCrossing *) event)->y_root;
			pevent->tooltip = NULL;

			data->cal_view = g_object_ref (view);
			data->day = -1;
			data->event_num = event_num;
			data->get_view_event = (ECalendarViewEvent * (*)(ECalendarView *, int, gint)) tooltip_get_view_event;
			pevent->timeout = e_named_timeout_add_full (
				G_PRIORITY_DEFAULT, 500,
				e_week_view_handle_tooltip_timeout,
				data, e_week_view_destroy_tooltip_timeout_data);
			g_object_set_data ((GObject *) view, "tooltip-timeout", GUINT_TO_POINTER (pevent->timeout));

			return TRUE;
		} else {
			return FALSE;
		}
		case GDK_MOTION_NOTIFY:
			g_return_val_if_fail (pevent != NULL, FALSE);

			pevent->x = ((GdkEventMotion *) event)->x_root;
			pevent->y = ((GdkEventMotion *) event)->y_root;
			pevent->tooltip = (GtkWidget *) g_object_get_data (G_OBJECT (view), "tooltip-window");

			if (pevent->tooltip) {
				e_calendar_view_move_tip (pevent->tooltip, pevent->x + 16, pevent->y + 16);
			}

			return TRUE;
		case GDK_LEAVE_NOTIFY:
		case GDK_KEY_PRESS:
		case GDK_BUTTON_PRESS:
			tooltip_destroy (view, item);
		default:
			return FALSE;
	}
}

static const gchar *
get_comp_summary (ECalClient *client,
                  icalcomponent *icalcomp,
                  gboolean *free_text)
{
	const gchar *my_summary, *location;
	const gchar *summary;
	gboolean my_free_text = FALSE;

	g_return_val_if_fail (icalcomp != NULL && free_text != NULL, NULL);

	my_summary = e_calendar_view_get_icalcomponent_summary (client, icalcomp, &my_free_text);

	location = icalcomponent_get_location (icalcomp);
	if (location && *location) {
		*free_text = TRUE;
		summary = g_strdup_printf ("%s (%s)", my_summary, location);

		if (my_free_text)
			g_free ((gchar *) my_summary);
	} else {
		*free_text = my_free_text;
		summary = my_summary;
	}

	return summary;
}

static void
e_week_view_reshape_event_span (EWeekView *week_view,
                                gint event_num,
                                gint span_num)
{
	ECalendarView *cal_view;
	ECalModel *model;
	ESourceRegistry *registry;
	EWeekViewEvent *event;
	EWeekViewEventSpan *span;
	gint span_x, span_y, span_w, num_icons, icons_width, time_width;
	gint min_text_x, max_text_w, width;
	gboolean show_icons = TRUE, use_max_width = FALSE;
	gboolean one_day_event;
	ECalComponent *comp;
	gdouble text_x, text_y, text_w, text_h;
	gchar *text, *end_of_line;
	gint line_len, text_width;
	PangoContext *pango_context;
	PangoFontMetrics *font_metrics;
	PangoLayout *layout;

	cal_view = E_CALENDAR_VIEW (week_view);
	model = e_calendar_view_get_model (cal_view);

	registry = e_cal_model_get_registry (model);

	if (!is_array_index_in_bounds (week_view->events, event_num))
		return;

	event = &g_array_index (week_view->events, EWeekViewEvent, event_num);

	if (!is_comp_data_valid (event))
		return;

	if (!is_array_index_in_bounds (week_view->spans, event->spans_index + span_num))
		return;

	span = &g_array_index (week_view->spans, EWeekViewEventSpan,
			       event->spans_index + span_num);
	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));

	one_day_event = e_week_view_is_one_day_event (week_view, event_num);

	/* If the span will not be visible destroy the canvas items and
	 * return. */
	if (!e_week_view_get_span_position (week_view, event_num, span_num,
					    &span_x, &span_y, &span_w)) {
		if (span->background_item)
			g_object_run_dispose (G_OBJECT (span->background_item));
		if (span->text_item)
			g_object_run_dispose (G_OBJECT (span->text_item));
		span->background_item = NULL;
		span->text_item = NULL;

		g_object_unref (comp);
		return;
	}

	/* Set up Pango prerequisites */
	pango_context = gtk_widget_get_pango_context (GTK_WIDGET (week_view));
	font_metrics = pango_context_get_metrics (
		pango_context, NULL,
		pango_context_get_language (pango_context));
	layout = pango_layout_new (pango_context);

	/* If we are editing a long event we don't show the icons and the EText
	 * item uses the maximum width available. */
	if (!one_day_event && week_view->editing_event_num == event_num
	    && week_view->editing_span_num == span_num) {
		show_icons = FALSE;
		use_max_width = TRUE;
	} else if (e_week_view_get_multi_week_view (week_view)) {
		show_icons = e_week_view_get_show_icons_month_view (week_view);
	}

	/* Calculate how many icons we need to show. */
	num_icons = 0;
	if (show_icons) {
		if (e_cal_component_has_alarms (comp))
			num_icons++;
		if (e_cal_component_has_recurrences (comp) || e_cal_component_is_instance (comp))
			num_icons++;
		if (e_cal_component_has_attachments (comp))
			num_icons++;
		if (e_cal_component_has_attendees (comp))
			num_icons++;
		if (event->different_timezone)
			num_icons++;
		num_icons += cal_comp_util_get_n_icons (comp, NULL);
	}

	/* Create the background canvas item if necessary. */
	if (!span->background_item) {
		span->background_item =
			gnome_canvas_item_new (
				GNOME_CANVAS_GROUP (GNOME_CANVAS (week_view->main_canvas)->root),
				e_week_view_event_item_get_type (),
				NULL);
	}

	g_object_set_data ((GObject *) span->background_item, "event-num", GINT_TO_POINTER (event_num));
	g_signal_connect (
		span->background_item, "event",
		G_CALLBACK (tooltip_event_cb), week_view);

	gnome_canvas_item_set (
		span->background_item,
		"event_num", event_num,
		"span_num", span_num,
		NULL);

	/* Create the text item if necessary. */
	if (!span->text_item) {
		const gchar *summary;
		GdkColor color;
		gboolean free_text = FALSE;

		color = e_week_view_get_text_color (week_view, event);
		summary = get_comp_summary (event->comp_data->client, event->comp_data->icalcomp, &free_text);

		span->text_item =
			gnome_canvas_item_new (
				GNOME_CANVAS_GROUP (GNOME_CANVAS (week_view->main_canvas)->root),
				e_text_get_type (),
				"clip", TRUE,
				"max_lines", 1,
				"editable", TRUE,
				"text", summary ? summary : "",
				"use_ellipsis", TRUE,
				"fill_color_gdk", &color,
				"im_context", E_CANVAS (week_view->main_canvas)->im_context,
				NULL);

		if (free_text)
			g_free ((gchar *) summary);

		if (e_cal_util_component_has_attendee (event->comp_data->icalcomp))
			set_style_from_attendee (event, span, registry);

		g_signal_connect (
			span->text_item, "event",
			G_CALLBACK (e_week_view_on_text_item_event), week_view);
		g_signal_emit_by_name (
			G_OBJECT (week_view),
			"event_added", event);

	}

	g_object_set_data (G_OBJECT (span->text_item), "event-num", GINT_TO_POINTER (event_num));

	/* Calculate the position of the text item.
	 * For events < 1 day it starts after the times & icons and ends at the
	 * right edge of the span.
	 * For events >= 1 day we need to determine whether times are shown at
	 * the start and end of the span, then try to center the text item with
	 * the icons in the middle, but making sure we don't go over the times.
	*/

	/* Calculate the space necessary to display a time, e.g. "13:00". */
	time_width = e_week_view_get_time_string_width (week_view);

	/* Calculate the space needed for the icons. */
	if (num_icons > 0) {
		icons_width = (E_WEEK_VIEW_ICON_WIDTH + E_WEEK_VIEW_ICON_X_PAD)
			* num_icons - E_WEEK_VIEW_ICON_X_PAD + E_WEEK_VIEW_ICON_R_PAD;
	} else {
		icons_width = 0;
	}

	/* The y position and height are the same for both event types. */
	text_y = span_y + E_WEEK_VIEW_EVENT_BORDER_HEIGHT
		+ E_WEEK_VIEW_EVENT_TEXT_Y_PAD;

	text_h =
		PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics)) +
		PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics));

	if (one_day_event) {
		gboolean hide_end_time;

		hide_end_time = event->start_minute == event->end_minute;

		/* Note that 1-day events don't have a border. Although we
		 * still use the border height to position the events
		 * vertically so they still line up neatly (see above),
		 * we don't use the border width or edge padding at all. */
		text_x = span_x + E_WEEK_VIEW_EVENT_L_PAD;

		switch (week_view->time_format) {
		case E_WEEK_VIEW_TIME_BOTH_SMALL_MIN:
		case E_WEEK_VIEW_TIME_BOTH:
			/* These have 2 time strings with a small space between
			 * them and some space before the EText item. */
			text_x += time_width * (hide_end_time ? 1 : 2)
				+ (hide_end_time ? 0 : E_WEEK_VIEW_EVENT_TIME_SPACING)
				+ E_WEEK_VIEW_EVENT_TIME_X_PAD;
			break;
		case E_WEEK_VIEW_TIME_START_SMALL_MIN:
		case E_WEEK_VIEW_TIME_START:
			/* These have just 1 time string with some space
			 * before the EText item. */
			text_x += time_width + E_WEEK_VIEW_EVENT_TIME_X_PAD;
			break;
		case E_WEEK_VIEW_TIME_NONE:
			break;
		}

		/* The icons_width includes space on the right of the icons. */
		text_x += icons_width;

		/* The width of the EText item extends right to the edge of the
		 * event, just inside the border. */
		text_w = span_x + span_w - E_WEEK_VIEW_EVENT_R_PAD - text_x;

	} else {
		if (use_max_width) {
			/* When we are editing the event we use all the
			 * available width. */
			text_x = span_x + E_WEEK_VIEW_EVENT_L_PAD
				+ E_WEEK_VIEW_EVENT_BORDER_WIDTH
				+ E_WEEK_VIEW_EVENT_EDGE_X_PAD;
			text_w = span_x + span_w - E_WEEK_VIEW_EVENT_R_PAD
				- E_WEEK_VIEW_EVENT_BORDER_WIDTH
				- E_WEEK_VIEW_EVENT_EDGE_X_PAD - text_x;
		} else {
			text = NULL;
			/* Get the width of the text of the event. This is a
			 * bit of a hack. It would be better if EText could
			 * tell us this. */
			g_object_get (span->text_item, "text", &text, NULL);
			text_width = 0;
			if (text) {
				/* It should only have one line of text in it.
				 * I'm not sure we need this any more. */
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
			 * position, which centers the icons + text. */
			width = text_width + icons_width;
			text_x = span_x + (span_w - width) / 2;

			/* Now calculate the left-most valid position, and make
			 * sure we don't go to the left of that. */
			min_text_x = span_x + E_WEEK_VIEW_EVENT_L_PAD
				+ E_WEEK_VIEW_EVENT_BORDER_WIDTH
				+ E_WEEK_VIEW_EVENT_EDGE_X_PAD;
			/* See if we will want to display the start time, and
			 * if so take that into account. */
			if (event->start > week_view->day_starts[span->start_day])
				min_text_x += time_width
					+ E_WEEK_VIEW_EVENT_TIME_X_PAD;

			/* Now make sure we don't go to the left of the minimum
			 * position. */
			text_x = MAX (text_x, min_text_x);

			/* Now calculate the largest valid width, using the
			 * calculated x position, and make sure we don't
			 * exceed that. */
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

	gnome_canvas_item_set (
		span->text_item,
		"clip_width", (gdouble) text_w,
		"clip_height", (gdouble) text_h,
		NULL);
	e_canvas_item_move_absolute (span->text_item, text_x, text_y);
	gnome_canvas_item_request_update (span->background_item);

	g_object_unref (comp);
	g_object_unref (layout);
	pango_font_metrics_unref (font_metrics);
}

gboolean
e_week_view_start_editing_event (EWeekView *week_view,
                                 gint event_num,
                                 gint span_num,
                                 gchar *initial_text)
{
	EWeekViewEvent *event;
	EWeekViewEventSpan *span;
	ETextEventProcessor *event_processor = NULL;
	ETextEventProcessorCommand command;
	ECalModelComponent *comp_data;

	/* If we are already editing the event, just return. */
	if (event_num == week_view->editing_event_num
	    && span_num == week_view->editing_span_num)
		return TRUE;

	if (!is_array_index_in_bounds (week_view->events, event_num))
		return FALSE;

	event = &g_array_index (week_view->events, EWeekViewEvent, event_num);

	if (!is_comp_data_valid (event))
		return FALSE;

	if (!is_array_index_in_bounds (week_view->spans, event->spans_index + span_num))
		return FALSE;

	span = &g_array_index (week_view->spans, EWeekViewEventSpan,
			       event->spans_index + span_num);

	if (e_client_is_readonly (E_CLIENT (event->comp_data->client)))
		return FALSE;

	/* If the event is not shown, don't try to edit it. */
	if (!span->text_item)
		return FALSE;

	if (week_view->editing_event_num >= 0) {
		EWeekViewEvent *editing;

		if (!is_array_index_in_bounds (week_view->events, week_view->editing_event_num))
			return FALSE;

		editing = &g_array_index (week_view->events, EWeekViewEvent, week_view->editing_event_num);

		/* do not change to other part of same component - the event is spread into more days */
		if (editing && editing->comp_data == event->comp_data)
			return FALSE;
	}

	gnome_canvas_item_set (
		span->text_item,
		"text", initial_text ? initial_text : icalcomponent_get_summary (event->comp_data->icalcomp),
		NULL);

	/* Save the comp_data value because we use that as our invariant */
	comp_data = event->comp_data;

	e_canvas_item_grab_focus (span->text_item, TRUE);

	/* If the above focus caused things to redraw, then find the
	 * the event and the span again */
	if (event_num < week_view->events->len)
		event = &g_array_index (week_view->events, EWeekViewEvent, event_num);

	if (event_num >= week_view->events->len || event->comp_data != comp_data) {
		/* When got in because of other comp_data, then be sure we go through all events */
		event_num = week_view->events->len;

		/* Unfocussing can cause a removal but not a new
		 * addition so just run backwards through the
		 * events */
		for (event_num--; event_num >= 0; event_num--) {
			event = &g_array_index (week_view->events, EWeekViewEvent, event_num);
			if (event->comp_data == comp_data)
				break;
		}
		g_return_val_if_fail (event_num >= 0, FALSE);
	}

	if (!is_array_index_in_bounds (week_view->spans, event->spans_index + span_num))
		return FALSE;

	span = &g_array_index (week_view->spans, EWeekViewEventSpan,  event->spans_index + span_num);

	/* Try to move the cursor to the end of the text. */
	g_object_get (span->text_item, "event_processor", &event_processor, NULL);
	if (event_processor) {
		command.action = E_TEP_MOVE;
		command.position = E_TEP_END_OF_BUFFER;
		g_signal_emit_by_name (
			event_processor,
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
	gint event_num, span_num;
	EWeekViewEvent *event;
	EWeekViewEventSpan *span;
	const gchar *summary;

	event_num = week_view->editing_event_num;
	span_num = week_view->editing_span_num;

	g_return_if_fail (event_num != -1);

	if (!is_array_index_in_bounds (week_view->events, event_num))
		return;

	event = &g_array_index (week_view->events, EWeekViewEvent, event_num);

	if (!is_comp_data_valid (event))
		return;

	if (!is_array_index_in_bounds (week_view->spans, event->spans_index + span_num))
		return;

	span = &g_array_index (week_view->spans, EWeekViewEventSpan, event->spans_index + span_num);

	/* Reset the text to what was in the component */

	summary = icalcomponent_get_summary (event->comp_data->icalcomp);
	g_object_set (span->text_item, "text", summary ? summary : "", NULL);

	/* Stop editing */
	e_week_view_stop_editing_event (week_view);
}

static gboolean
e_week_view_on_text_item_event (GnomeCanvasItem *item,
                                GdkEvent *gdk_event,
                                EWeekView *week_view)
{
	EWeekViewEvent *event;
	gint event_num, span_num;
	gint nevent;
	EWeekViewEvent *pevent;
	guint event_button = 0;
	guint event_keyval = 0;
	gdouble event_x_root = 0;
	gdouble event_y_root = 0;

	e_week_view_check_layout (week_view);

	nevent = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "event-num"));
	pevent = tooltip_get_view_event (week_view, -1, nevent);

	switch (gdk_event->type) {
	case GDK_KEY_PRESS:
		tooltip_destroy (week_view, item);
		gdk_event_get_keyval (gdk_event, &event_keyval);

		if (!E_TEXT (item)->preedit_len && (event_keyval == GDK_KEY_Return || event_keyval == GDK_KEY_KP_Enter)) {
			/* We set the keyboard focus to the EDayView, so the
			 * EText item loses it and stops the edit. */
			gtk_widget_grab_focus (GTK_WIDGET (week_view));

			/* Stop the signal last or we will also stop any
			 * other events getting to the EText item. */
			g_signal_stop_emission_by_name (item, "event");
			return TRUE;
		} else if (event_keyval == GDK_KEY_Escape) {
			cancel_editing (week_view);
			g_signal_stop_emission_by_name (item, "event");
			/* focus should go to week view when stop editing */
			gtk_widget_grab_focus (GTK_WIDGET (week_view));
			return TRUE;
		}
		break;
	case GDK_2BUTTON_PRESS:
		if (!e_week_view_find_event_from_item (week_view, item,
						       &event_num, &span_num))
			return FALSE;

		if (!is_array_index_in_bounds (week_view->events, event_num))
			return FALSE;

		event = &g_array_index (week_view->events, EWeekViewEvent,
					event_num);

		if (!is_comp_data_valid (event))
			return FALSE;

		/* if we started to editing new item on the canvas, then do not open editing dialog until it's saved,
		 * because the save of the event recalculates event numbers and you can edit different one */
		if (event->comp_data->is_new_component)
			return TRUE;

		e_calendar_view_edit_appointment (
			E_CALENDAR_VIEW (week_view),
			event->comp_data->client,
			event->comp_data->icalcomp, EDIT_EVENT_AUTODETECT);

		g_signal_stop_emission_by_name (item, "event");
		return TRUE;
	case GDK_BUTTON_PRESS:
		tooltip_destroy (week_view, item);
		if (!e_week_view_find_event_from_item (week_view, item,
						       &event_num, &span_num))
			return FALSE;

		gdk_event_get_button (gdk_event, &event_button);
		if (event_button == 3) {
			EWeekViewEvent *e;

			if (E_TEXT (item)->editing) {
				e_week_view_stop_editing_event (week_view);
				gtk_widget_grab_focus (GTK_WIDGET (week_view));
				return FALSE;
			}

			if (!is_array_index_in_bounds (week_view->events, event_num))
				return FALSE;

			e = &g_array_index (week_view->events, EWeekViewEvent, event_num);

			if (!gtk_widget_has_focus (GTK_WIDGET (week_view)))
				gtk_widget_grab_focus (GTK_WIDGET (week_view));

			e_week_view_set_selected_time_range_visible (week_view, e->start, e->end);

			e_week_view_show_popup_menu (
				week_view, gdk_event, event_num);

			g_signal_stop_emission_by_name (
				item->canvas, "button_press_event");
			return TRUE;
		}

		if (event_button != 3) {
			week_view->pressed_event_num = event_num;
			week_view->pressed_span_num = span_num;
		}

		/* Only let the EText handle the event while editing. */
		if (!E_TEXT (item)->editing) {
			gdouble event_x_win = 0;
			gdouble event_y_win = 0;

			g_signal_stop_emission_by_name (item, "event");

			gdk_event_get_coords (
				gdk_event, &event_x_win, &event_y_win);

			week_view->drag_event_x = (gint) event_x_win;
			week_view->drag_event_y = (gint) event_y_win;

			/* FIXME: Remember the day offset from the start of
			 * the event, for DnD. */

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
				e_week_view_start_editing_event (
					week_view,
					event_num,
					span_num,
					NULL);
				week_view->pressed_event_num = -1;
			}

			/* Stop the signal last or we will also stop any
			 * other events getting to the EText item. */
			g_signal_stop_emission_by_name (item, "event");
			return TRUE;
		}
		week_view->pressed_event_num = -1;
		break;
	case GDK_ENTER_NOTIFY:
	{
		ECalendarViewEventData *data;
		gint nspan;

		if (week_view->editing_event_num != -1
		    || !e_week_view_find_event_from_item (week_view, item, &nevent, &nspan))
			return FALSE;

		g_object_set_data ((GObject *) item, "event-num", GINT_TO_POINTER (nevent));

		pevent = tooltip_get_view_event (week_view, -1, nevent);
		g_return_val_if_fail (pevent != NULL, FALSE);

		data = g_malloc (sizeof (ECalendarViewEventData));

		gdk_event_get_root_coords (
			gdk_event, &event_x_root, &event_y_root);

		pevent->x = (gint) event_x_root;
		pevent->y = (gint) event_y_root;
		pevent->tooltip = NULL;

		data->cal_view = g_object_ref (week_view);
		data->day = -1;
		data->event_num = nevent;
		data->get_view_event = (ECalendarViewEvent * (*)(ECalendarView *, int, gint)) tooltip_get_view_event;
		pevent->timeout = e_named_timeout_add_full (
			G_PRIORITY_DEFAULT, 500,
			e_week_view_handle_tooltip_timeout,
			data, e_week_view_destroy_tooltip_timeout_data);
		g_object_set_data ((GObject *) week_view, "tooltip-timeout", GUINT_TO_POINTER (pevent->timeout));

		return TRUE;
	}
	case GDK_LEAVE_NOTIFY:
		tooltip_destroy (week_view, item);

		return FALSE;
	case GDK_MOTION_NOTIFY:
		g_return_val_if_fail (pevent != NULL, FALSE);

		gdk_event_get_root_coords (
			gdk_event, &event_x_root, &event_y_root);

		pevent->x = (gint) event_x_root;
		pevent->y = (gint) event_y_root;
		pevent->tooltip = (GtkWidget *) g_object_get_data (G_OBJECT (week_view), "tooltip-window");

		if (pevent->tooltip) {
			e_calendar_view_move_tip (pevent->tooltip, pevent->x + 16, pevent->y + 16);
		}
		return TRUE;
	case GDK_FOCUS_CHANGE:
		if (gdk_event->focus_change.in) {
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

static gboolean
e_week_view_event_move (ECalendarView *cal_view,
                        ECalViewMoveDirection direction)
{
	EWeekViewEvent *event;
	gint event_num, adjust_days, current_start_day, current_end_day;
	time_t start_dt, end_dt;
	struct icaltimetype start_time,end_time;
	EWeekView *week_view = E_WEEK_VIEW (cal_view);
	gboolean is_all_day = FALSE;

	event_num = week_view->editing_event_num;
	adjust_days = 0;

	/* If no item is being edited, just return. */
	if (event_num == -1)
		return FALSE;

	if (!is_array_index_in_bounds (week_view->events, event_num))
		return FALSE;

	event = &g_array_index (week_view->events, EWeekViewEvent, event_num);

	if (!is_comp_data_valid (event))
		return FALSE;

	end_dt = event->end;
	start_time = icalcomponent_get_dtstart (event->comp_data->icalcomp);
	end_time = icalcomponent_get_dtend (event->comp_data->icalcomp);

	if (start_time.is_date && end_time.is_date)
		is_all_day = TRUE;

	current_end_day = e_week_view_get_day_offset_of_event (week_view,end_dt);

	switch (direction) {
	case E_CAL_VIEW_MOVE_UP:
		adjust_days = e_week_view_get_adjust_days_for_move_up (week_view,current_end_day);
		break;
	case E_CAL_VIEW_MOVE_DOWN:
		adjust_days = e_week_view_get_adjust_days_for_move_down (week_view,current_end_day);
		break;
	case E_CAL_VIEW_MOVE_LEFT:
		adjust_days = e_week_view_get_adjust_days_for_move_left (week_view,current_end_day);
		break;
	case E_CAL_VIEW_MOVE_RIGHT:
		adjust_days = e_week_view_get_adjust_days_for_move_right (week_view,current_end_day);
		break;
	default:
		break;
	}

	icaltime_adjust (&start_time ,adjust_days,0,0,0);
	icaltime_adjust (&end_time ,adjust_days,0,0,0);
	start_dt = icaltime_as_timet_with_zone (
		start_time,
		e_calendar_view_get_timezone (E_CALENDAR_VIEW (week_view)));
	end_dt = icaltime_as_timet_with_zone (
		end_time,
		e_calendar_view_get_timezone (E_CALENDAR_VIEW (week_view)));

	current_start_day = e_week_view_get_day_offset_of_event (week_view,start_dt);
	current_end_day = e_week_view_get_day_offset_of_event (week_view,end_dt);
	if (is_all_day)
		current_end_day--;

	if (current_start_day < 0)
		return TRUE;

	if (current_end_day >= e_week_view_get_weeks_shown (week_view) * 7)
		return TRUE;

	e_week_view_change_event_time (week_view, start_dt, end_dt, is_all_day);

	return TRUE;
}

static gint
e_week_view_get_day_offset_of_event (EWeekView *week_view,
                                     time_t event_time)
{
	time_t first_day = week_view->day_starts[0];

	if (event_time - first_day < 0)
		return -1;
	else
		return (event_time - first_day) / (24 * 60 * 60);
}

void
e_week_view_scroll_a_step (EWeekView *week_view,
                           ECalViewMoveDirection direction)
{
	GtkAdjustment *adjustment;
	GtkRange *range;
	gdouble step_increment;
	gdouble page_size;
	gdouble new_value;
	gdouble lower;
	gdouble upper;
	gdouble value;

	range = GTK_RANGE (week_view->vscrollbar);
	adjustment = gtk_range_get_adjustment (range);

	step_increment = gtk_adjustment_get_step_increment (adjustment);
	page_size = gtk_adjustment_get_page_size (adjustment);
	lower = gtk_adjustment_get_lower (adjustment);
	upper = gtk_adjustment_get_upper (adjustment);
	value = gtk_adjustment_get_value (adjustment);

	switch (direction) {
		case E_CAL_VIEW_MOVE_UP:
			new_value = value - step_increment;
			break;
		case E_CAL_VIEW_MOVE_DOWN:
			new_value = value + step_increment;
			break;
		case E_CAL_VIEW_MOVE_PAGE_UP:
			new_value = value - page_size;
			break;
		case E_CAL_VIEW_MOVE_PAGE_DOWN:
			new_value = value + page_size;
			break;
		default:
			return;
	}

	new_value = CLAMP (new_value, lower, upper - page_size);
	gtk_adjustment_set_value (adjustment, new_value);
}

static void
e_week_view_change_event_time (EWeekView *week_view,
                               time_t start_dt,
                               time_t end_dt,
                               gboolean is_all_day)
{
	EWeekViewEvent *event;
	gint event_num;
	ECalComponent *comp;
	ECalComponentDateTime date;
	struct icaltimetype itt;
	ECalClient *client;
	ECalObjModType mod = E_CAL_OBJ_MOD_ALL;

	event_num = week_view->editing_event_num;

	/* If no item is being edited, just return. */
	if (event_num == -1)
		return;

	if (!is_array_index_in_bounds (week_view->events, event_num))
		return;

	event = &g_array_index (week_view->events, EWeekViewEvent, event_num);

	if (!is_comp_data_valid (event))
		return;

	client = event->comp_data->client;

	/* We use a temporary shallow copy of the ico since we don't want to
	 * change the original ico here. Otherwise we would not detect that
	 * the event's time had changed in the "update_event" callback. */
	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));
	date.value = &itt;
	/* FIXME: Should probably keep the timezone of the original start
	 * and end times. */
	date.tzid = icaltimezone_get_tzid (e_calendar_view_get_timezone (E_CALENDAR_VIEW (week_view)));

	*date.value = icaltime_from_timet_with_zone (start_dt, is_all_day,
						     e_calendar_view_get_timezone (E_CALENDAR_VIEW (week_view)));
	cal_comp_set_dtstart_with_oldzone (client, comp, &date);
	*date.value = icaltime_from_timet_with_zone (end_dt, is_all_day,
						     e_calendar_view_get_timezone (E_CALENDAR_VIEW (week_view)));
	cal_comp_set_dtend_with_oldzone (client, comp, &date);

	e_cal_component_commit_sequence (comp);

	if (week_view->last_edited_comp_string != NULL) {
		g_free (week_view->last_edited_comp_string);
		week_view->last_edited_comp_string = NULL;
	}

	week_view->last_edited_comp_string = e_cal_component_get_as_string (comp);

	if (e_cal_component_has_recurrences (comp)) {
		if (!e_cal_dialogs_recur_component (client, comp, &mod, NULL, FALSE)) {
			gtk_widget_queue_draw (week_view->main_canvas);
			goto out;
		}

		if (mod == E_CAL_OBJ_MOD_THIS) {
			e_cal_component_set_rdate_list (comp, NULL);
			e_cal_component_set_rrule_list (comp, NULL);
			e_cal_component_set_exdate_list (comp, NULL);
			e_cal_component_set_exrule_list (comp, NULL);
		}
	} else if (e_cal_component_is_instance (comp))
		mod = E_CAL_OBJ_MOD_THIS;

	e_cal_component_commit_sequence (comp);

	e_cal_ops_modify_component (e_calendar_view_get_model (E_CALENDAR_VIEW (week_view)),
		client, e_cal_component_get_icalcomponent (comp),
		mod, E_CAL_OPS_SEND_FLAG_ASK | E_CAL_OPS_SEND_FLAG_IS_NEW_COMPONENT);

out:
	g_object_unref (comp);
}

static void
e_week_view_on_editing_started (EWeekView *week_view,
                                GnomeCanvasItem *item)
{
	gint event_num = -1, span_num = -1;

	if (!e_week_view_find_event_from_item (week_view, item,
					       &event_num, &span_num))
		return;

	week_view->editing_event_num = event_num;
	week_view->editing_span_num = span_num;

	/* We need to reshape long events so the whole width is used while
	 * editing. */
	if (!e_week_view_is_one_day_event (week_view, event_num)) {
		e_week_view_reshape_event_span (
			week_view, event_num, span_num);
	}

	if (event_num != -1) {
		EWeekViewEvent *event;
		EWeekViewEventSpan *span;

		if (is_array_index_in_bounds (week_view->events, event_num)) {
			event = &g_array_index (week_view->events, EWeekViewEvent, event_num);

			if (is_comp_data_valid (event) &&
			    is_array_index_in_bounds (week_view->spans, event->spans_index + span_num)) {
				span = &g_array_index (week_view->spans, EWeekViewEventSpan,
						       event->spans_index + span_num);

				gnome_canvas_item_set (
					span->text_item,
					"text", icalcomponent_get_summary (event->comp_data->icalcomp),
					NULL);
			}
		}
	}

	g_signal_emit_by_name (week_view, "selection_changed");

	g_object_notify (G_OBJECT (week_view), "is-editing");
}

static void
e_week_view_on_editing_stopped (EWeekView *week_view,
                                GnomeCanvasItem *item)
{
	gint event_num, span_num;
	EWeekViewEvent *event;
	EWeekViewEventSpan *span;
	gchar *text = NULL;
	ECalComponent *comp;
	ECalComponentText summary;
	ECalClient *client;
	const gchar *uid;
	gboolean on_server;

	/* Note: the item we are passed here isn't reliable, so we just stop
	 * the edit of whatever item was being edited. We also receive this
	 * event twice for some reason. */
	event_num = week_view->editing_event_num;
	span_num = week_view->editing_span_num;

	/* If no item is being edited, just return. */
	if (event_num == -1)
		return;

	if (!is_array_index_in_bounds (week_view->events, event_num))
		return;

	event = &g_array_index (week_view->events, EWeekViewEvent, event_num);

	if (!is_comp_data_valid (event))
		return;

	if (!is_array_index_in_bounds (week_view->spans, event->spans_index + span_num))
		return;

	span = &g_array_index (week_view->spans, EWeekViewEventSpan,
			       event->spans_index + span_num);

	/* Reset the edit fields. */
	week_view->editing_event_num = -1;

	/* Check that the event is still valid. */
	uid = icalcomponent_get_uid (event->comp_data->icalcomp);
	if (!uid) {
		g_object_notify (G_OBJECT (week_view), "is-editing");
		return;
	}

	text = NULL;
	g_object_set (span->text_item, "handle_popup", FALSE, NULL);
	g_object_get (span->text_item, "text", &text, NULL);

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));

	client = event->comp_data->client;
	on_server = !event->comp_data->is_new_component;

	if (string_is_empty (text) && !on_server) {
		e_cal_component_get_uid (comp, &uid);
		g_signal_handlers_disconnect_by_func (item, e_week_view_on_text_item_event, week_view);
		e_week_view_foreach_event_with_uid (week_view, uid,
						    e_week_view_remove_event_cb, NULL);
		week_view->event_destroyed = TRUE;
		gtk_widget_queue_draw (week_view->main_canvas);
		e_week_view_check_layout (week_view);
		goto out;
	}

	/* Only update the summary if necessary. */
	e_cal_component_get_summary (comp, &summary);
	if (summary.value && !strcmp (text, summary.value)) {
		gboolean free_text = FALSE;
		const gchar *summary;

		summary = get_comp_summary (event->comp_data->client, event->comp_data->icalcomp, &free_text);
		g_object_set (span->text_item, "text", summary ? summary : "", NULL);

		if (free_text)
			g_free ((gchar *) summary);

		if (!e_week_view_is_one_day_event (week_view, event_num))
			e_week_view_reshape_event_span (week_view, event_num, span_num);
	} else if (summary.value || !string_is_empty (text)) {
		icalcomponent *icalcomp = e_cal_component_get_icalcomponent (comp);

		summary.value = text;
		summary.altrep = NULL;
		e_cal_component_set_summary (comp, &summary);
		e_cal_component_commit_sequence (comp);

		if (!on_server) {
			e_cal_ops_create_component (e_calendar_view_get_model (E_CALENDAR_VIEW (week_view)), client, icalcomp,
				e_calendar_view_component_created_cb, g_object_ref (week_view), g_object_unref);

			/* we remove the object since we either got the update from the server or failed */
			e_week_view_remove_event_cb (week_view, event_num, NULL);
		} else {
			ECalObjModType mod = E_CAL_OBJ_MOD_ALL;

			if (e_cal_component_has_recurrences (comp)) {
				if (!e_cal_dialogs_recur_component (client, comp, &mod, NULL, FALSE)) {
					goto out;
				}

				if (mod == E_CAL_OBJ_MOD_THIS) {
					ECalComponentDateTime dt;
					struct icaltimetype tt;
					gchar *tzid;

					e_cal_component_get_dtstart (comp, &dt);
					if (dt.value->zone) {
						tt = icaltime_from_timet_with_zone (
							event->comp_data->instance_start,
							dt.value->is_date,
							dt.value->zone);
					} else {
						tt = icaltime_from_timet_with_zone (
							event->comp_data->instance_start,
							dt.value->is_date,
							e_calendar_view_get_timezone (E_CALENDAR_VIEW (week_view)));
					}
					tzid = g_strdup (dt.tzid);
					e_cal_component_free_datetime (&dt);
					dt.value = &tt;
					dt.tzid = tzid;
					e_cal_component_set_dtstart (comp, &dt);
					g_free (tzid);

					e_cal_component_get_dtend (comp, &dt);
					if (dt.value->zone) {
						tt = icaltime_from_timet_with_zone (
							event->comp_data->instance_end,
							dt.value->is_date,
							dt.value->zone);
					} else {
						tt = icaltime_from_timet_with_zone (
							event->comp_data->instance_end,
							dt.value->is_date,
							e_calendar_view_get_timezone (E_CALENDAR_VIEW (week_view)));
					}
					tzid = g_strdup (dt.tzid);
					e_cal_component_free_datetime (&dt);
					dt.value = &tt;
					dt.tzid = tzid;
					e_cal_component_set_dtend (comp, &dt);
					g_free (tzid);

					e_cal_component_set_rdate_list (comp, NULL);
					e_cal_component_set_rrule_list (comp, NULL);
					e_cal_component_set_exdate_list (comp, NULL);
					e_cal_component_set_exrule_list (comp, NULL);
				}
			} else if (e_cal_component_is_instance (comp))
				mod = E_CAL_OBJ_MOD_THIS;

			e_cal_component_commit_sequence (comp);
			e_cal_ops_modify_component (e_calendar_view_get_model (E_CALENDAR_VIEW (week_view)),
				client, e_cal_component_get_icalcomponent (comp), mod, E_CAL_OPS_SEND_FLAG_ASK);
		}
	}

 out:

	g_free (text);
	g_object_unref (comp);

	g_signal_emit_by_name (week_view, "selection_changed");

	g_object_notify (G_OBJECT (week_view), "is-editing");
}

gboolean
e_week_view_find_event_from_item (EWeekView *week_view,
                                  GnomeCanvasItem *item,
                                  gint *event_num_return,
                                  gint *span_num_return)
{
	EWeekViewEvent *event;
	EWeekViewEventSpan *span;
	gint event_num, span_num, num_events;

	num_events = week_view->events->len;
	for (event_num = 0; event_num < num_events; event_num++) {
		event = &g_array_index (week_view->events, EWeekViewEvent,
					event_num);
		for (span_num = 0; span_num < event->num_spans; span_num++) {
			if (!is_array_index_in_bounds (week_view->spans, event->spans_index + span_num))
				continue;

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
 * Returns TRUE if an event with the uid was found.
 * Note that for recurring events there may be several EWeekViewEvents, one
 * for each instance, all with the same iCalObject and uid. So only use this
 * function if you know the event doesn't recur or you are just checking to
 * see if any events with the uid exist. */
static gboolean
e_week_view_find_event_from_uid (EWeekView *week_view,
                                 ECalClient *client,
                                 const gchar *uid,
                                 const gchar *rid,
                                 gint *event_num_return)
{
	EWeekViewEvent *event;
	gint event_num, num_events;

	*event_num_return = -1;
	if (!uid)
		return FALSE;

	num_events = week_view->events->len;
	for (event_num = 0; event_num < num_events; event_num++) {
		const gchar *u;
		gchar *r = NULL;

		event = &g_array_index (week_view->events, EWeekViewEvent,
					event_num);

		if (!is_comp_data_valid (event))
			continue;

		if (event->comp_data->client != client)
			continue;

		u = icalcomponent_get_uid (event->comp_data->icalcomp);
		if (u && !strcmp (uid, u)) {
			if (rid && *rid) {
				r = icaltime_as_ical_string_r (icalcomponent_get_recurrenceid (event->comp_data->icalcomp));
				if (!r || !*r)
					continue;
				if (strcmp (rid, r) != 0) {
					g_free (r);
					continue;
				}
				g_free (r);
			}

			*event_num_return = event_num;
			return TRUE;
		}
	}

	return FALSE;
}

gboolean
e_week_view_is_one_day_event (EWeekView *week_view,
                              gint event_num)
{
	EWeekViewEvent *event;
	EWeekViewEventSpan *span;

	if (!is_array_index_in_bounds (week_view->events, event_num))
		return FALSE;

	event = &g_array_index (week_view->events, EWeekViewEvent, event_num);
	if (event->num_spans != 1)
		return FALSE;

	if (!is_array_index_in_bounds (week_view->spans, event->spans_index))
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

static void
e_week_view_cursor_key_up (EWeekView *week_view)
{
	EWeekViewClass *week_view_class;

	week_view_class = E_WEEK_VIEW_GET_CLASS (week_view);
	g_return_if_fail (week_view_class->cursor_key_up != NULL);

	week_view_class->cursor_key_up (week_view);
}

static void
e_week_view_cursor_key_down (EWeekView *week_view)
{
	EWeekViewClass *week_view_class;

	week_view_class = E_WEEK_VIEW_GET_CLASS (week_view);
	g_return_if_fail (week_view_class->cursor_key_down != NULL);

	week_view_class->cursor_key_down (week_view);
}

static void
e_week_view_cursor_key_left (EWeekView *week_view)
{
	EWeekViewClass *week_view_class;

	week_view_class = E_WEEK_VIEW_GET_CLASS (week_view);
	g_return_if_fail (week_view_class->cursor_key_left != NULL);

	week_view_class->cursor_key_left (week_view);
}

static void
e_week_view_cursor_key_right (EWeekView *week_view)
{
	EWeekViewClass *week_view_class;

	week_view_class = E_WEEK_VIEW_GET_CLASS (week_view);
	g_return_if_fail (week_view_class->cursor_key_right != NULL);

	week_view_class->cursor_key_right (week_view);
}

static gboolean
e_week_view_do_key_press (GtkWidget *widget,
                          GdkEventKey *event)
{
	EWeekView *week_view;
	gchar *initial_text;
	guint keyval;
	gboolean stop_emission;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (E_IS_WEEK_VIEW (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	week_view = E_WEEK_VIEW (widget);
	keyval = event->keyval;

	/* The Escape key aborts a resize operation. */
#if 0
	if (week_view->resize_drag_pos != E_CALENDAR_VIEW_POS_NONE) {
		if (event->keyval == GDK_KEY_Escape) {
			e_week_view_abort_resize (week_view, event->time);
		}
		return FALSE;
	}
#endif

	/* Handle the cursor keys for moving the selection */
	stop_emission = FALSE;
	if (!(event->state & GDK_SHIFT_MASK)
		&& !(event->state & GDK_MOD1_MASK)) {
		stop_emission = TRUE;
		switch (keyval) {
		case GDK_KEY_Page_Up:
			if (!e_week_view_get_multi_week_view (week_view))
				e_week_view_scroll_a_step (week_view, E_CAL_VIEW_MOVE_UP);
			else
				e_week_view_scroll_a_step (week_view, E_CAL_VIEW_MOVE_PAGE_UP);
			break;
		case GDK_KEY_Page_Down:
			if (!e_week_view_get_multi_week_view (week_view))
				e_week_view_scroll_a_step (week_view, E_CAL_VIEW_MOVE_DOWN);
			else
				e_week_view_scroll_a_step (week_view, E_CAL_VIEW_MOVE_PAGE_DOWN);
			break;
		case GDK_KEY_Up:
			e_week_view_cursor_key_up (week_view);
			break;
		case GDK_KEY_Down:
			e_week_view_cursor_key_down (week_view);
			break;
		case GDK_KEY_Left:
			e_week_view_cursor_key_left (week_view);
			break;
		case GDK_KEY_Right:
			e_week_view_cursor_key_right (week_view);
			break;
		default:
			stop_emission = FALSE;
			break;
		}
	}
	if (stop_emission)
		return TRUE;

	/*Navigation through days with arrow keys*/
	if (((event->state & GDK_SHIFT_MASK) != GDK_SHIFT_MASK)
		&&((event->state & GDK_CONTROL_MASK) != GDK_CONTROL_MASK)
		&&((event->state & GDK_MOD1_MASK) == GDK_MOD1_MASK)) {
		if (keyval == GDK_KEY_Up || keyval == GDK_KEY_KP_Up)
			return e_week_view_event_move ((ECalendarView *) week_view, E_CAL_VIEW_MOVE_UP);
		else if (keyval == GDK_KEY_Down || keyval == GDK_KEY_KP_Down)
			return e_week_view_event_move ((ECalendarView *) week_view, E_CAL_VIEW_MOVE_DOWN);
		else if (keyval == GDK_KEY_Left || keyval == GDK_KEY_KP_Left)
			return e_week_view_event_move ((ECalendarView *) week_view, E_CAL_VIEW_MOVE_LEFT);
		else if (keyval == GDK_KEY_Right || keyval == GDK_KEY_KP_Right)
			return e_week_view_event_move ((ECalendarView *) week_view, E_CAL_VIEW_MOVE_RIGHT);
	}

	if (week_view->selection_start_day == -1)
		return FALSE;

	/* We only want to start an edit with a return key or a simple
	 * character. */
	if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
		initial_text = NULL;
	} else if (((event->keyval >= 0x20) && (event->keyval <= 0xFF)
		    && (event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)))
		   || (event->length == 0)
		   || (event->keyval == GDK_KEY_Tab)
		   || (event->keyval == GDK_KEY_Escape)
		   || (event->keyval == GDK_KEY_Delete)
		   || (event->keyval == GDK_KEY_KP_Delete)) {
		return FALSE;
	} else
		initial_text = e_utf8_from_gtk_event_key (widget, event->keyval, event->string);

	e_week_view_add_new_event_in_selected_range (week_view, initial_text, FALSE);

	if (initial_text)
		g_free (initial_text);

	return TRUE;
}

static gint
e_week_view_get_adjust_days_for_move_up (EWeekView *week_view,
                                         gint current_day)
{
	return e_week_view_get_multi_week_view (week_view) ? -7 : 0;
}

static gint
e_week_view_get_adjust_days_for_move_down (EWeekView *week_view,
                                           gint current_day)
{
	return e_week_view_get_multi_week_view (week_view) ? 7 : 0;
}

static gint
e_week_view_get_adjust_days_for_move_left (EWeekView *week_view,
                                           gint current_day)
{
	return -1;
}

static gint
e_week_view_get_adjust_days_for_move_right (EWeekView *week_view,
                                            gint current_day)
{
	return 1;
}

void
e_week_view_show_popup_menu (EWeekView *week_view,
                             GdkEvent *button_event,
                             gint event_num)
{
	guint timeout;

	timeout = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (week_view), "tooltip-timeout"));
	if (timeout) {
		g_source_remove (timeout);
		g_object_set_data (G_OBJECT (week_view), "tooltip-timeout", NULL);
	}

	week_view->popup_event_num = event_num;

	e_calendar_view_popup_event (E_CALENDAR_VIEW (week_view), button_event);
}

void
e_week_view_jump_to_button_item (EWeekView *week_view,
                                 GnomeCanvasItem *item)
{
	gint day;

	for (day = 0; day < E_WEEK_VIEW_MAX_WEEKS * 7; ++day) {
		if (item == week_view->jump_buttons[day]) {
			e_calendar_view_move_view_range (E_CALENDAR_VIEW (week_view), E_CALENDAR_VIEW_MOVE_TO_EXACT_DAY, week_view->day_starts[day]);
			return;
		}
	}
}

static gboolean
e_week_view_on_jump_button_event (GnomeCanvasItem *item,
                                  GdkEvent *event,
                                  EWeekView *week_view)
{
	gint day;

	if (event->type == GDK_BUTTON_PRESS) {
		e_week_view_jump_to_button_item (week_view, item);
		return TRUE;
	}
	else if (event->type == GDK_KEY_PRESS) {
		/* return, if Tab, Control or Alt is pressed */
		if ((event->key.keyval == GDK_KEY_Tab) ||
		    (event->key.state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)))
			return FALSE;
		/* with a return key or a simple character (from 0x20 to 0xff),
		 * jump to the day
		 */
		if ((event->key.keyval == GDK_KEY_Return || event->key.keyval == GDK_KEY_KP_Enter) ||
		    ((event->key.keyval >= 0x20) &&
		     (event->key.keyval <= 0xFF))) {
			e_week_view_jump_to_button_item (week_view, item);
			return TRUE;
		}
	}
	else if (event->type == GDK_FOCUS_CHANGE) {
		GdkEventFocus *focus_event = (GdkEventFocus *) event;
		GdkPixbuf *pixbuf = NULL;

		for (day = 0; day < E_WEEK_VIEW_MAX_WEEKS * 7; day++) {
			if (item == week_view->jump_buttons[day])
				break;
		}

		if (day >= E_WEEK_VIEW_MAX_WEEKS * 7) {
			g_warn_if_reached ();
			return FALSE;
		}

		if (focus_event->in) {
			week_view->focused_jump_button = day;
			pixbuf = gdk_pixbuf_new_from_xpm_data ((const gchar **) jump_xpm_focused);
			gnome_canvas_item_set (
				week_view->jump_buttons[day],
				"GnomeCanvasPixbuf::pixbuf",
				pixbuf, NULL);
		}
		else {
			week_view->focused_jump_button = E_WEEK_VIEW_JUMP_BUTTON_NO_FOCUS;
			pixbuf = gdk_pixbuf_new_from_xpm_data ((const gchar **) jump_xpm);
			gnome_canvas_item_set (
				week_view->jump_buttons[day],
				"GnomeCanvasPixbuf::pixbuf",
				pixbuf, NULL);
		}
		if (pixbuf)
			g_object_unref (pixbuf);
	}

	return FALSE;
}

/* Converts an hour from 0-23 to the preferred time format, and returns the
 * suffix to add and the width of it in the normal font. */
void
e_week_view_convert_time_to_display (EWeekView *week_view,
                                     gint hour,
                                     gint *display_hour,
                                     const gchar **suffix,
                                     gint *suffix_width)
{
	ECalModel *model;

	model = e_calendar_view_get_model (E_CALENDAR_VIEW (week_view));

	/* Calculate the actual hour number to display. For 12-hour
	 * format we convert 0-23 to 12-11am/12-11pm. */
	*display_hour = hour;
	if (e_cal_model_get_use_24_hour_format (model)) {
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
e_week_view_get_time_string_width (EWeekView *week_view)
{
	ECalModel *model;
	gint time_width;

	model = e_calendar_view_get_model (E_CALENDAR_VIEW (week_view));

	if (week_view->use_small_font && week_view->small_font_desc)
		time_width = week_view->digit_width * 2
			+ week_view->small_digit_width * 2;
	else
		time_width = week_view->digit_width * 4
			+ week_view->colon_width;

	if (!e_cal_model_get_use_24_hour_format (model))
		time_width += MAX (week_view->am_string_width,
				   week_view->pm_string_width);

	return time_width;
}

/* Queues a layout, unless one is already queued. */
static void
e_week_view_queue_layout (EWeekView *week_view)
{
	if (week_view->layout_timeout_id == 0) {
		week_view->layout_timeout_id = e_named_timeout_add (
			E_WEEK_VIEW_LAYOUT_TIMEOUT,
			e_week_view_layout_timeout_cb, week_view);
	}
}

/* Removes any queued layout. */
static void
e_week_view_cancel_layout (EWeekView *week_view)
{
	if (week_view->layout_timeout_id != 0) {
		g_source_remove (week_view->layout_timeout_id);
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

gboolean
e_week_view_is_jump_button_visible (EWeekView *week_view,
                                    gint day)
{
	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), FALSE);

	if ((day >= 0) && (day < E_WEEK_VIEW_MAX_WEEKS * 7))
		return week_view->jump_buttons[day]->flags & GNOME_CANVAS_ITEM_VISIBLE;
	return FALSE;
}

gboolean
e_week_view_is_editing (EWeekView *week_view)
{
	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), FALSE);

	return week_view->editing_event_num != -1;
}
