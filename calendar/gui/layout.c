/* Event layout engine for Gnomecal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Authors: Miguel de Icaza <miguel@nuclecu.unam.mx>
 *          Federico Mena <federico@nuclecu.unam.mx>
 */

#include <config.h>
#include <stdlib.h>
#include "layout.h"


/* This structure is used to pass around layout information among the internal layout functions */
struct layout_info {
	GList *events;			/* List of events from client */
	int num_events;			/* The number of events (length of the list) */
	LayoutQueryTimeFunc func;	/* Function to convert a list item to a start/end time pair */
	int num_rows;			/* Size of the time partition */
	time_t *partition;		/* The time partition containing start and end time values */
	int *array;			/* Working array of free and allocated time slots */
	int *allocations;		/* Returned array of slot allocations */
	int *slots;			/* Returned array of slots used */
	int num_slots;			/* Number of slots used */
};


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
static void
build_partition (struct layout_info *li)
{
	time_t *rows, *p, *q;
	GList *list;
	int i, unique_vals;

	/* This is the maximum number of rows we would need */

	li->num_rows = li->num_events * 2;

	/* Fill the rows with the times */

	rows = g_new (time_t, li->num_rows);

	for (list = li->events, p = rows; list; list = list->next) {
		(* li->func) (list, &p[0], &p[1]);
		p += 2;
	}

	/* Do a sort | uniq on the array */

	qsort (rows, li->num_rows, sizeof (time_t), compare_time_t);

	p = rows;
	q = rows + 1;
	unique_vals = 1;

	for (i = 1; i < li->num_rows; i++, q++)
		if (*q != *p) {
			unique_vals++;
			p++;
			*p = *q;
		}

	/* Return the number of unique values in the partition and the partition array itself */

	li->num_rows = unique_vals;
	li->partition = rows;
}

/* Returns the index of the element in the partition that corresponds to the specified time */
int
find_index (struct layout_info *li, time_t t)
{
	int i;

	for (i = 0; ; i++)
		if (li->partition[i] == t)
			return i;

	g_assert_not_reached ();
}

#define xy(li, x, y) li->array[(y * MAX_EVENTS_PER_ROW) + (x)]

/* Checks that all the cells in the slot array at the specified slot column are free to use by an
 * event that has the specified range.
 */
static int
range_is_empty (struct layout_info *li, int slot, time_t start, time_t end)
{
	int i;

	for (i = find_index (li, start); li->partition[i] < end; i++)
		if (xy (li, slot, i) != -1)
			return FALSE;

	return TRUE;
}

/* Allocates a time in the slot array for the specified event's index */
static void
range_allocate (struct layout_info *li, int slot, time_t start, time_t end, int ev_num)
{
	int i;

	for (i = find_index (li, start); li->partition[i] < end; i++)
		xy (li, slot, i) = ev_num;
}

/* Performs the initial allocation of slots for events.  Each event gets one column; they will be
 * expanded in a later stage.  Returns the number of columns used.
 */
static void
initial_allocate (struct layout_info *li)
{
	GList *events;
	int i;
	int slot;
	int num_slots;
	time_t start, end;

	num_slots = 0;

	for (i = 0, events = li->events; events; events = events->next, i++) {
		(* li->func) (events, &start, &end);

		/* Start with no allocation, no columns */

		li->allocations[i] = -1;
		li->slots[i] = 0;

		/* Find a free column for the event */

		for (slot = 0; slot < MAX_EVENTS_PER_ROW; slot++)
			if (range_is_empty (li, slot, start, end)) {
				range_allocate (li, slot, start, end, i);

				li->allocations[i] = slot;
				li->slots[i] = 1;

				if ((slot + 1) > num_slots)
					num_slots = slot + 1;

				break;
			}
	}

	li->num_slots = num_slots;
}

/* Returns the maximum number of columns that an event can expanded by in the slot array */
static int
columns_to_expand (struct layout_info *li, int ev_num, time_t start, time_t end)
{
	int cols;
	int slot;
	int i_start;
	int i;

	cols = 0;

	i_start = find_index (li, start);

	for (slot = li->allocations[ev_num] + 1; slot < li->num_slots; slot++) {
		for (i = i_start; li->partition[i] < end; i++)
			if (xy (li, slot, i) != -1)
				return cols;

		cols++;
	}

	return cols;
}

/* Expands an event by the specified number of columns */
static void
do_expansion (struct layout_info *li, int ev_num, time_t start, time_t end, int num_cols)
{
	int i, j;
	int slot;

	for (i = find_index (li, start); li->partition[i] < end; i++) {
		slot = li->allocations[ev_num] + 1;

		for (j = 0; j < num_cols; j++)
			xy (li, slot + j, i) = ev_num;
	}
}

/* Expands the events in the slot array to occupy as many columns as possible.  This is the second
 * pass of the layout algorithm.
 */
static void
expand_events (struct layout_info *li)
{
	GList *events;
	time_t start, end;
	int i;
	int cols;

	for (i = 0, events = li->events; events; events = events->next, i++) {
		(* li->func) (events, &start, &end);

		cols = columns_to_expand (li, i, start, end);

		if (cols == 0)
			continue; /* We can't expand this event */

		do_expansion (li, i, start, end, cols);

		li->slots[i] += cols;
	}
}

void
layout_events (GList *events, LayoutQueryTimeFunc func, int *num_slots, int **allocations, int **slots)
{
	struct layout_info li;
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

	li.events = events;
	li.num_events = g_list_length (events);
	li.func = func;

	/* Build the partition of the time range, and then build the array of slots */

	build_partition (&li);

	li.array = g_new (int, li.num_rows * MAX_EVENTS_PER_ROW);
	for (i = 0; i < (li.num_rows * MAX_EVENTS_PER_ROW); i++)
		li.array[i] = -1; /* This is our 'empty' value */

	/* Build the arrays for allocations and columns used */

	li.allocations = g_new (int, li.num_events);
	li.slots = g_new (int, li.num_events);

	/* Perform initial allocation and then expand the events to as many slots as they can occupy */

	initial_allocate (&li);
	expand_events (&li);

	/* Clean up and return values */

	g_free (li.array);

	*num_slots = li.num_slots;
	*allocations = li.allocations;
	*slots = li.slots;
}
