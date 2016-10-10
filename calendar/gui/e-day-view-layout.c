/*
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
 *
 * Authors:
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
 * Lays out events for the Day & Work-Week views of the calendar. It is also
 * used for printing.
 */

#include "evolution-config.h"

#include "e-day-view-layout.h"

static void e_day_view_layout_long_event (EDayViewEvent	  *event,
					  guint8	  *grid,
					  gint		   days_shown,
					  time_t	  *day_starts,
					  gint		  *rows_in_top_display);

static void e_day_view_layout_day_event (EDayViewEvent    *event,
					 EBitArray       **grid,
					 guint16	  *group_starts,
					 guint8		  *cols_per_row,
					 gint		   rows,
					 gint		   mins_per_row,
					 gint              max_cols);
static void e_day_view_expand_day_event (EDayViewEvent    *event,
					 EBitArray       **grid,
					 guint8		  *cols_per_row,
					 gint		   mins_per_row);
static void e_day_view_recalc_cols_per_row (gint           rows,
					    guint8	  *cols_per_row,
					    guint16       *group_starts);

void
e_day_view_layout_long_events (GArray *events,
                               gint days_shown,
                               time_t *day_starts,
                               gint *rows_in_top_display)
{
	EDayViewEvent *event;
	gint event_num;
	guint8 *grid;

	/* This is a temporary 2-d grid which is used to place events.
	 * Each element is 0 if the position is empty, or 1 if occupied.
	 * We allocate the maximum size possible here, assuming that each
	 * event will need its own row. */
	grid = g_new0 (guint8, events->len * E_DAY_VIEW_MAX_DAYS);

	/* Reset the number of rows in the top display to 0. It will be
	 * updated as events are layed out below. */
	*rows_in_top_display = 0;

	/* Iterate over the events, finding which days they cover, and putting
	 * them in the first free row available. */
	for (event_num = 0; event_num < events->len; event_num++) {
		event = &g_array_index (events, EDayViewEvent, event_num);
		e_day_view_layout_long_event (
			event, grid,
			days_shown, day_starts,
			rows_in_top_display);
	}

	/* Free the grid. */
	g_free (grid);
}

static void
e_day_view_layout_long_event (EDayViewEvent *event,
                              guint8 *grid,
                              gint days_shown,
                              time_t *day_starts,
                              gint *rows_in_top_display)
{
	gint start_day, end_day, free_row, day, row;

	event->num_columns = 0;

	if (!e_day_view_find_long_event_days (event,
					      days_shown, day_starts,
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
	*rows_in_top_display = MAX (*rows_in_top_display, free_row + 1);
}

/* returns maximum number of columns among all rows */
gint
e_day_view_layout_day_events (GArray *events,
                              gint rows,
                              gint mins_per_row,
                              guint8 *cols_per_row,
                              gint max_cols)
{
	EDayViewEvent *event;
	gint row, event_num, res;
	EBitArray **grid;

	/* This is a temporary array which keeps track of rows which are
	 * connected. When an appointment spans multiple rows then the number
	 * of columns in each of these rows must be the same (i.e. the maximum
	 * of all of them). Each element in the array corresponds to one row
	 * and contains the index of the first row in the group of connected
	 * rows. */
	guint16 group_starts[12 * 24];

	/* This is a temporary 2-d grid which is used to place events.
	 * Each element is 0 if the position is empty, or 1 if occupied. */
	grid = g_new0 (EBitArray *, rows);

	/* Reset the cols_per_row array, and initialize the connected rows so
	 * that all rows are not connected - each row is the start of a new
	 * group. */
	for (row = 0; row < rows; row++) {
		cols_per_row[row] = 0;
		group_starts[row] = row;

		/* row doesn't contain any event at the moment */
		grid[row] = e_bit_array_new (0);
	}

	/* Iterate over the events, finding which rows they cover, and putting
	 * them in the first free column available. Increment the number of
	 * events in each of the rows it covers, and make sure they are all
	 * in one group. */
	for (event_num = 0; event_num < events->len; event_num++) {
		event = &g_array_index (events, EDayViewEvent, event_num);

		e_day_view_layout_day_event (
			event, grid, group_starts,
			cols_per_row, rows, mins_per_row, max_cols);
	}

	/* Recalculate the number of columns needed in each row. */
	e_day_view_recalc_cols_per_row (rows, cols_per_row, group_starts);

	/* Iterate over the events again, trying to expand events horizontally
	 * if there is enough space. */
	for (event_num = 0; event_num < events->len; event_num++) {
		event = &g_array_index (events, EDayViewEvent, event_num);
		e_day_view_expand_day_event (
			event, grid, cols_per_row,
			mins_per_row);
	}

	/* Free the grid and compute maximum number of columns used. */
	res = 0;
	for (row = 0; row < rows; row++) {
		res = MAX (res, e_bit_array_bit_count (grid[row]));
		g_object_unref (grid[row]);
	}
	g_free (grid);

	return res;
}

/* Finds the first free position to place the event in.
 * Increments the number of events in each of the rows it covers, and makes
 * sure they are all in one group. */
static void
e_day_view_layout_day_event (EDayViewEvent *event,
                             EBitArray **grid,
                             guint16 *group_starts,
                             guint8 *cols_per_row,
                             gint rows,
                             gint mins_per_row,
                             gint max_cols)
{
	gint start_row, end_row, free_col, col, row, group_start;

	start_row = event->start_minute / mins_per_row;
	end_row = (event->end_minute - 1) / mins_per_row;
	if (end_row < start_row)
		end_row = start_row;

	event->num_columns = 0;

	/* If the event can't currently be seen, just return. */
	if (start_row >= rows || end_row < 0)
		return;

	/* Make sure we don't go outside the visible times. */
	start_row = CLAMP (start_row, 0, rows - 1);
	end_row = CLAMP (end_row, 0, rows - 1);

	/* Try each column until we find a free one. */
	for (col = 0; max_cols <= 0 || col < max_cols; col++) {
		free_col = col;
		for (row = start_row; row <= end_row; row++) {
			if (e_bit_array_bit_count (grid[row]) > col &&
			    e_bit_array_value_at (grid[row], col)) {
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
	 * We use the cols_per_row array for this. It will be sorted out after
	 * all the events have been layed out. Also make sure all the rows that
	 * the event covers are in one group. */
	for (row = start_row; row <= end_row; row++) {
		/* resize the array if necessary */
		if (e_bit_array_bit_count (grid[row]) <= free_col)
			e_bit_array_insert (
				grid[row], e_bit_array_bit_count (grid[row]),
				free_col - e_bit_array_bit_count (grid[row]) + 1);

		e_bit_array_change_one_row (grid[row], free_col, TRUE);
		cols_per_row[row]++;
		group_starts[row] = group_start;
	}

	/* If any following rows should be in the same group, add them. */
	for (row = end_row + 1; row < rows; row++) {
		if (group_starts[row] > end_row)
			break;
		group_starts[row] = group_start;
	}
}

/* For each group of rows, find the max number of events in all the
 * rows, and set the number of cols in each of the rows to that. */
static void
e_day_view_recalc_cols_per_row (gint rows,
                                guint8 *cols_per_row,
                                guint16 *group_starts)
{
	gint start_row = 0, row, next_start_row, max_events;

	while (start_row < rows) {
		max_events = 0;
		for (row = start_row; row < rows && group_starts[row] == start_row; row++)
			max_events = MAX (max_events, cols_per_row[row]);

		next_start_row = row;

		for (row = start_row; row < next_start_row; row++)
			cols_per_row[row] = max_events;

		start_row = next_start_row;
	}
}

/* Expands the event horizontally to fill any free space. */
static void
e_day_view_expand_day_event (EDayViewEvent *event,
                             EBitArray **grid,
                             guint8 *cols_per_row,
                             gint mins_per_row)
{
	gint start_row, end_row, col, row;
	gboolean clashed;

	start_row = event->start_minute / mins_per_row;
	end_row = (event->end_minute - 1) / mins_per_row;
	if (end_row < start_row)
		end_row = start_row;

	/* Try each column until we find a free one. */
	clashed = FALSE;
	for (col = event->start_row_or_col + 1; col < cols_per_row[start_row]; col++) {
		for (row = start_row; row <= end_row; row++) {
			if (e_bit_array_bit_count (grid[row]) > col &&
			    e_bit_array_value_at (grid[row], col)) {
				clashed = TRUE;
				break;
			}
		}

		if (clashed)
			break;

		event->num_columns++;
	}
}

/* Find the start and end days for the event. */
gboolean
e_day_view_find_long_event_days (EDayViewEvent *event,
                                 gint days_shown,
                                 time_t *day_starts,
                                 gint *start_day_return,
                                 gint *end_day_return)
{
	gint day, start_day, end_day;

	start_day = -1;
	end_day = -1;

	for (day = 0; day < days_shown; day++) {
		if (start_day == -1
		    && event->start < day_starts[day + 1])
			start_day = day;
		if (event->end > day_starts[day])
			end_day = day;
	}

	if (event->start == event->end)
		end_day = start_day;

	/* Sanity check. */
	if (start_day < 0 || start_day >= days_shown
	    || end_day < 0 || end_day >= days_shown
	    || end_day < start_day) {
		g_warning ("Invalid date range for event, start/end days: %d / %d", start_day, end_day);
		return FALSE;
	}

	*start_day_return = start_day;
	*end_day_return = end_day;

	return TRUE;
}

