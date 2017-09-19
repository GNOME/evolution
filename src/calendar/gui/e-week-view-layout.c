/*
 * Lays out events for the Week & Month views of the calendar. It is also
 * used for printing.
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
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#include "evolution-config.h"

#include "e-week-view-layout.h"
#include "calendar-config.h"

static void e_week_view_layout_event	(EWeekViewEvent	*event,
					 guint8		*grid,
					 GArray		*spans,
					 GArray		*old_spans,
					 gboolean	 multi_week_view,
					 gint		 weeks_shown,
					 gboolean	 compress_weekend,
					 gint		 start_weekday,
					 time_t		*day_starts,
					 gint		*rows_per_day);
static gint e_week_view_find_day	(time_t		 time_to_find,
					 gboolean	 include_midnight_in_prev_day,
					 gint		 days_shown,
					 time_t		*day_starts);
static gint e_week_view_find_span_end	(gboolean	 multi_week_view,
					 gboolean	 compress_weekend,
					 GDateWeekday	 display_start_day,
					 gint		 day);

GArray *
e_week_view_layout_events (GArray *events,
                           GArray *old_spans,
                           gboolean multi_week_view,
                           gint weeks_shown,
                           gboolean compress_weekend,
                           gint start_weekday,
                           time_t *day_starts,
                           gint *rows_per_day)
{
	EWeekViewEvent *event;
	EWeekViewEventSpan *span;
	gint num_days, day, event_num, span_num;
	guint8 *grid;
	GArray *spans;

	/* This is a temporary 2-d grid which is used to place events.
	 * Each element is 0 if the position is empty, or 1 if occupied.
	 * We allocate the maximum size possible here, assuming that each
	 * event will need its own row. */
	grid = g_new0 (guint8, E_WEEK_VIEW_MAX_ROWS_PER_CELL * 7
		       * E_WEEK_VIEW_MAX_WEEKS);

	/* We create a new array of spans, which will replace the old one. */
	spans = g_array_new (FALSE, FALSE, sizeof (EWeekViewEventSpan));

	/* Clear the number of rows used per day. */
	num_days = multi_week_view ? weeks_shown * 7 : 7;
	for (day = 0; day < num_days; day++) {
		rows_per_day[day] = 0;
	}

	/* Iterate over the events, finding which weeks they cover, and putting
	 * them in the first free row available. */
	for (event_num = 0; event_num < events->len; event_num++) {
		event = &g_array_index (events, EWeekViewEvent, event_num);
		e_week_view_layout_event (
			event, grid, spans, old_spans,
			multi_week_view,
			weeks_shown, compress_weekend,
			start_weekday, day_starts,
			rows_per_day);
	}

	/* Free the grid. */
	g_free (grid);

	/* Destroy the old spans array, destroying any unused canvas items. */
	if (old_spans) {
		for (span_num = 0; span_num < old_spans->len; span_num++) {
			span = &g_array_index (old_spans, EWeekViewEventSpan,
					       span_num);
			if (span->background_item)
				g_object_run_dispose (G_OBJECT (span->background_item));
			if (span->text_item)
				g_object_run_dispose (G_OBJECT (span->text_item));
		}
		g_array_free (old_spans, TRUE);
	}

	return spans;
}

static void
e_week_view_layout_event (EWeekViewEvent *event,
                                 guint8 *grid,
                                 GArray *spans,
                                 GArray *old_spans,
                                 gboolean multi_week_view,
                                 gint weeks_shown,
                                 gboolean compress_weekend,
                                 gint start_weekday,
                                 time_t *day_starts,
                                 gint *rows_per_day)
{
	gint start_day, end_day, span_start_day, span_end_day, rows_per_cell;
	gint free_row, row, day, span_num, spans_index, num_spans, days_shown;
	EWeekViewEventSpan span, *old_span;

	days_shown = multi_week_view ? weeks_shown * 7 : 7;
	start_day = e_week_view_find_day (
		event->start, FALSE, days_shown,
		day_starts);
	end_day = e_week_view_find_day (
		event->end, event->start != event->end, days_shown,
					day_starts);
	start_day = CLAMP (start_day, 0, days_shown - 1);
	end_day = CLAMP (end_day, 0, days_shown - 1);

#if 0
	g_print (
		"In e_week_view_layout_event Start:%i End: %i\n",
		start_day, end_day);
#endif

	/* Iterate through each of the spans of the event, where each span
	 * is a sequence of 1 or more days displayed next to each other. */
	span_start_day = start_day;
	rows_per_cell = E_WEEK_VIEW_MAX_ROWS_PER_CELL;
	span_num = 0;
	spans_index = spans->len;
	num_spans = 0;
	while (span_start_day <= end_day) {
		span_end_day = e_week_view_find_span_end (
			multi_week_view,
			compress_weekend,
			start_weekday,
			span_start_day);
		span_end_day = MIN (span_end_day, end_day);
#if 0
		g_print (
			"  Span start:%i end:%i\n", span_start_day,
			span_end_day);
#endif
		/* Try each row until we find a free one or we fall off the
		 * bottom of the available rows. */
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
				rows_per_day[day] = MAX (
					rows_per_day[day],
					free_row + 1);
			}
#if 0
			g_print (
				"  Span start:%i end:%i row:%i\n",
				span_start_day, span_end_day, free_row);
#endif
			/* Add the span to the array, and try to reuse any
			 * canvas items from the old spans. */
			span.start_day = span_start_day;
			span.num_days = span_end_day - span_start_day + 1;
			span.row = free_row;
			span.background_item = NULL;
			span.text_item = NULL;
			if (event->num_spans > span_num) {
				old_span = &g_array_index (
					old_spans, EWeekViewEventSpan,
					event->spans_index + span_num);
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

/* Finds the day containing the given time.
 * If include_midnight_in_prev_day is TRUE then if the time exactly
 * matches the start of a day the previous day is returned. This is useful
 * when calculating the end day of an event. */
static gint
e_week_view_find_day (time_t time_to_find,
                      gboolean include_midnight_in_prev_day,
                      gint days_shown,
                      time_t *day_starts)
{
	gint day;

	if (time_to_find < day_starts[0])
		return -1;
	if (time_to_find > day_starts[days_shown])
		return days_shown;

	for (day = 1; day <= days_shown; day++) {
		if (time_to_find <= day_starts[day]) {
			if (time_to_find == day_starts[day]
			    && !include_midnight_in_prev_day)
				return day;
			return day - 1;
		}
	}

	g_return_val_if_reached (days_shown);
}

/* This returns the last possible day in the same span as the given day.
 * A span is all the days which are displayed next to each other from left to
 * right. In the week view all spans are only 1 day, since Tuesday is below
 * Monday rather than beside it etc. In the month view, if the weekends are not
 * compressed then each week is a span, otherwise we have to break a span up
 * on Saturday, use a separate span for Sunday, and start again on Monday. */
static gint
e_week_view_find_span_end (gboolean multi_week_view,
                           gboolean compress_weekend,
                           GDateWeekday display_start_day,
                           gint day)
{
	gint week, col, sat_col, end_col;

	if (multi_week_view) {
		week = day / 7;
		col = day % 7;

		/* We default to the last column in the row. */
		end_col = 6;

		/* If the weekend is compressed we must end any spans on
		 * Saturday and Sunday. */
		if (compress_weekend) {
			sat_col = e_weekday_get_days_between (
				display_start_day, G_DATE_SATURDAY);
			if (col <= sat_col)
				end_col = sat_col;
			else if (col == sat_col + 1)
				end_col = sat_col + 1;
		}

		return week * 7 + end_col;
	} else {
		return day;
	}
}

void
e_week_view_layout_get_day_position (gint day,
                                     gboolean multi_week_view,
                                     gint weeks_shown,
                                     GDateWeekday display_start_day,
                                     gboolean compress_weekend,
                                     gint *day_x,
                                     gint *day_y,
                                     gint *rows)
{
	GDateWeekday day_of_week;
	gint week, col, weekend_col;

	*day_x = *day_y = *rows = 0;
	g_return_if_fail (day >= 0);

	if (multi_week_view) {
		g_return_if_fail (day < weeks_shown * 7);

		week = day / 7;
		col = day % 7;
		day_of_week = e_weekday_add_days (display_start_day, day);
		if (compress_weekend && day_of_week >= G_DATE_SATURDAY) {
			/* In the compressed view Saturday is above Sunday and
			 * both have just one row as opposed to 2 for all the
			 * other days. */
			if (day_of_week == G_DATE_SATURDAY) {
				*day_y = week * 2;
				*rows = 1;
			} else {
				*day_y = week * 2 + 1;
				*rows = 1;
				col--;
			}
			/* Both Saturday and Sunday are in the same column. */
			*day_x = col;
		} else {
			/* If the weekend is compressed and the day is after
			 * the weekend we have to move back a column. */
			if (compress_weekend) {
				/* Calculate where the weekend column is. */
				weekend_col = e_weekday_get_days_between (
					display_start_day, G_DATE_SATURDAY);
				if (col > weekend_col)
					col--;
			}

			*day_y = week * 2;
			*rows = 2;
			*day_x = col;
		}
	} else {
		GSettings *settings;
		gint arr[4] = {1, 1, 1, 1};
		gint edge, i, wd, m, M;
		gboolean any = TRUE;
		gboolean days_left_to_right;
		gint n_work_days_mon_wed = 0;
		gint n_work_days_thu_sun = 0;

		/* 0 = Monday, 6 = Sunday */
		gint work_days[7] = { 0, 0, 0, 0, 0, 0, 0 };

		g_return_if_fail (day < 7);

		settings = e_util_ref_settings ("org.gnome.evolution.calendar");

		days_left_to_right = g_settings_get_boolean (settings, "week-view-days-left-to-right");

		if (g_settings_get_boolean (settings, "work-day-monday"))
			work_days[0] = 1, n_work_days_mon_wed++;
		if (g_settings_get_boolean (settings, "work-day-tuesday"))
			work_days[1] = 1, n_work_days_mon_wed++;
		if (g_settings_get_boolean (settings, "work-day-wednesday"))
			work_days[2] = 1, n_work_days_mon_wed++;
		if (g_settings_get_boolean (settings, "work-day-thursday"))
			work_days[3] = 1, n_work_days_thu_sun++;
		if (g_settings_get_boolean (settings, "work-day-friday"))
			work_days[4] = 1, n_work_days_thu_sun++;
		if (g_settings_get_boolean (settings, "work-day-saturday"))
			work_days[5] = 1, n_work_days_thu_sun++;
		if (g_settings_get_boolean (settings, "work-day-sunday"))
			work_days[6] = 1, n_work_days_thu_sun++;

		g_object_unref (settings);

		if (n_work_days_mon_wed < n_work_days_thu_sun)
			edge = 4;  /* Friday */
		else
			edge = 3;  /* Thursday */

		if (days_left_to_right) {
			/* The default view is always with two columns:
			     Mon  Thu
			     Tue  Fri
			     Wed  Sat/Sun
			   eventually:
			     Mon  Fri
			     Tue  Sat
			     Wed  Sun
			     Thu
			*/
			if (edge == 3) {
				const gint transform[] = { 0, 3, 1, 4, 2, 5, 6 };
				day = transform[day];
			} else {
				const gint transform[] = { 0, 4, 1, 5, 2, 3, 6 };
				day = transform[day];
			}
		}

		if (day < edge) {
			*day_x = 0;
			m = 0;
			M = edge;
		} else {
			*day_x = 1;
			m = edge;
			M = 7;
		}

		wd = 0; /* number of used rows in column */
		for (i = m; i < M; i++) {
			arr[i - m] += work_days[i];
			wd += arr[i - m];
		}

		while (wd != 6 && any) {
			any = FALSE;

			for (i = M - 1; i >= m; i--) {
				if (arr[i - m] > 1) {
					any = TRUE;

					/* too many rows, make last shorter */
					if (wd > 6) {
						arr[i - m] --;
						wd--;

					/* free rows left, enlarge those bigger */
					} else if (wd < 6) {
						arr[i - m] ++;
						wd++;
					}

					if (wd == 6)
						break;
				}
			}

			if (!any && wd != 6) {
				any = TRUE;

				for (i = m; i < M; i++) {
					arr[i - m] += 3;
					wd += 3;
				}
			}
		}

		*rows = arr [day - m];

		*day_y = 0;
		for (i = m; i < day; i++) {
			*day_y += arr [i - m];
		}

		#undef wk
	}
}

/* Returns TRUE if the event span is visible or FALSE if it isn't.
 * It also returns the number of days of the span that are visible.
 * Usually this can easily be determined by the start & end days and row of
 * the span, which are set in e_week_view_layout_event (). Though we need a
 * special case for the weekends when they are compressed, since the span may
 * not fit. */
gboolean
e_week_view_layout_get_span_position (EWeekViewEvent *event,
                                      EWeekViewEventSpan *span,
                                      gint rows_per_cell,
                                      gint rows_per_compressed_cell,
                                      GDateWeekday display_start_day,
                                      gboolean multi_week_view,
                                      gboolean compress_weekend,
                                      gint *span_num_days)
{
	GDateWeekday end_day_of_week;
	guint n_days;

	if (multi_week_view && span->row >= rows_per_cell)
		return FALSE;

	n_days = span->start_day + span->num_days - 1;
	end_day_of_week = e_weekday_add_days (display_start_day, n_days);

	*span_num_days = span->num_days;
	/* Check if the row will not be visible in compressed cells. */
	if (span->row >= rows_per_compressed_cell) {
		if (multi_week_view) {
			if (compress_weekend) {
				/* If it ends on a Saturday and is 1 day glong
				 * we skip it, else we shorten it. If it ends
				 * on a Sunday it must be 1 day long and we
				 * skip it. */
				if (end_day_of_week == G_DATE_SATURDAY) {
					if (*span_num_days == 1) {
						return FALSE;
					} else {
						(*span_num_days)--;
					}
				} else if (end_day_of_week == G_DATE_SUNDAY) {
					return FALSE;
				}
			}
		} else {
			gint day_x, day_y, rows = 0;
			e_week_view_layout_get_day_position (
				end_day_of_week - 1, multi_week_view, 1,
				display_start_day, compress_weekend,
				&day_x, &day_y, &rows);

			if (((rows / 2) * rows_per_cell) + ((rows % 2) *
				rows_per_compressed_cell) <= span->row)
				return FALSE;
		}
	}

	return TRUE;
}
