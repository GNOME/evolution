/* Event layout engine for Gnomecal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Authors: Miguel de Icaza <miguel@nuclecu.unam.mx>
 *          Federico Mena <federico@nuclecu.unam.mx>
 */

#include "layout.h"


/* This defines the maximum number of events to overlap per row.  More than that number of events
 * will not be displayed.  This is not ideal, so sue me.
 */
#define MAX_EVENTS_PER_ROW 32


/* Compares two time_t values, used for qsort() */
static int
compare_time_t (const void *a, const void *b)
{
	time_t ta, tb;

	ta = *((time_t *) a);
	tb = *((time_t *) b);

	if (ta < tb)
		return -1;
	else if (ta > tb)
		return 1;
	else
		return 0;
}

/* Builds a partition of the time range occupied by the events in the list.  It returns an array
 * with the times that define the partition and the number of items in the partition.
 */
static time_t *
build_partition (GList *events, int num_events, int *num_rows)
{
	time_t *rows, *p, *q;
	GList *list;
	CalendarObject *co;
	int i, unique_vals;

	/* This is the maximum number of rows we would need */

	*num_rows = num_events * 2;

	/* Fill the rows with the times */

	rows = g_new (time_t, *num_rows);

	for (list = events, p = rows; list; list = list->next) {
		co = list->data;
		*p++ = co->ev_start;
		*p++ = co->ev_end;
	}

	/* Do a sort | uniq on the array */

	qsort (rows, *num_rows, sizeof (time_t), compare_time_t);

	p = rows;
	q = rows + 1;
	unique_vals = 1;

	for (i = 1; i < *num_rows; i++, q++)
		if (*q != *p) {
			unique_vals++;
			p++;
			*p = *q;
		}

	/* Return the number of unique values in the partition and the partition array itself */

	*num_rows = unique_vals;
	return rows;
}

/* Returns the index of the element in the partition that corresponds to the specified time */
int
find_index (time_t *partition, time_t t)
{
	int i;

	for (i = 0; ; i++)
		if (partition[i] == t)
			return i;
}

#define xy(slot_array, x, y) slot_array[(y * MAX_EVENTS_PER_ROW) + (x)]

/* Checks that all the cells in the slot array at the specified slot column are free to use by an
 * event that has the specified range.
 */
static int
range_is_empty (time_t *partition, int *slot_array, int slot, time_t start, time_t end)
{
	int i;

	for (i = find_index (partition, start); partition[i] < end; i++)
		if (xy (slot_array, slot, i) != -1)
			return FALSE;

	return TRUE;
}

/* Allocates a time in the slot array for the specified event's index */
static void
range_allocate (time_t *partition, int *slot_array, int slot, time_t start, time_t end, int ev_num)
{
	int i;

	for (i = find_index (partition, start); partition[i] < end; i++)
		xy (slot_array, slot, i) = ev_num;
}

/* Performs the initial allocation of slots for events.  Each event gets one column; they will be
 * expanded in a later stage.  Returns the number of columns used.
 */
static int
initial_allocate (GList *events, time_t *partition, int *slot_array, int *allocations, int *columns_used)
{
	CalendarObject *co;
	int i;
	int slot;
	int num_slots;

	num_slots = 0;

	for (i = 0; events; events = events->next, i++) {
		co = events->data;

		/* Start with no allocation, no columns */

		allocations[i] = -1;
		columns_used[i] = 0;

		/* Find a free column for the event */

		for (slot = 0; slot < MAX_EVENTS_PER_ROW; slot++)
			if (range_is_empty (partition, slot_array, slot, co->ev_start, co->ev_end)) {
				range_allocate (partition, slot_array, slot, co->ev_start, co->ev_end, i);

				allocations[i] = slot;
				columns_used[i] = 1;

				if ((slot + 1) > num_slots)
					num_slots = slot + 1;

				break;
			}
	}

	return num_slots;
}

/* Returns the maximum number of columns that an event can expanded by in the slot array */
static int
columns_to_expand (time_t *partition, int *slot_array, int *allocations, int num_slots, int ev_num,
		   time_t start, time_t end)
{
	int cols;
	int slot;
	int i_start;
	int i;

	cols = 0;

	i_start = find_index (partition, start);

	for (slot = allocations[ev_num] + 1; slot < num_slots; slot++) {
		for (i = i_start; partition[i] < end; i++)
			if (xy (slot_array, slot, i) != -1)
				return cols;

		cols++;
	}

	return cols;
}

/* Expands an event to occupy the specified number of columns */
static void
do_expansion (time_t *partition, int *slot_array, int *allocations, int ev_num, time_t start, time_t end, int num_cols)
{
	int i, j;
	int slot;

	for (i = find_index (partition, start); partition[i] < end; i++) {
		slot = allocations[ev_num] + 1;

		for (j = 0; j < num_cols; j++)
			xy (slot_array, slot + j, i) = ev_num;
	}
}

/* Expands the events in the slot array to occupy as many columns as possible.  This is the second
 * pass of the layout algorithm.
 */
static void
expand_events (GList *events, time_t *partition, int *slot_array, int num_slots, int *allocations, int *columns_used)
{
	int i;
	CalendarObject *co;
	int cols;

	for (i = 0; events; events = events->next, i++) {
		co = events->data;

		cols = columns_to_expand (partition, slot_array, allocations, num_slots, i, co->ev_start, co->ev_end);

		if (cols == 0)
			continue; /* We can't expand this event */

		do_expansion (partition, slot_array, allocations, i, co->ev_start, co->ev_end, cols);

		columns_used[i] += cols;
	}
}

void
layout_events (GList *events, int *num_slots, int **allocations, int **slots)
{
	time_t *time_partition;
	int *slot_array;
	int num_events;
	int num_rows;
	int i;

	g_return_if_fail (num_slots != NULL);
	g_return_if_fail (allocations != NULL);
	g_return_if_fail (slots != NULL);

	if (!events) {
		*num_slots = 0;
		*allocations = NULL;
		*slots = NULL;

		return;
	}

	num_events = g_list_length (events);

	/* Build the partition of the time range, and then build the array of slots */

	time_partition = build_partition (events, num_events, &num_rows);

	slot_array = g_new (int, num_rows * MAX_EVENTS_PER_ROW);
	for (i = 0; i < (num_rows * MAX_EVENTS_PER_ROW); i++)
		slot_array[i] = -1; /* This is our 'empty' value */

	/* Build the arrays for allocations and columns used */

	*allocations = g_new (int, num_events);
	*slots = g_new (int, num_events);

	/* Perform initial allocation -- each event gets one column */

	*num_slots = initial_allocate (events, time_partition, slot_array, *allocations, *slots);

	/* Expand the events to as many columns as possible */

	expand_events (events, time_partition, slot_array, *num_slots, *allocations, *slots);

	g_free (slot_array);
}
