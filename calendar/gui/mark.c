/* Functions to mark calendars
 *
 * Copyright (C) 1998 Red Hat Software, Inc.
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#include <config.h>
#include "gnome-cal.h"
#include "main.h"
#include "mark.h"
#include "timeutil.h"


/* In the month item, marks all the days that are touched by the specified time span.  Assumes that
 * the time span is completely contained within the month.
 */
static void
mark_event_in_month (GnomeMonthItem *mitem, time_t start, time_t end)
{
	struct tm tm;
	GnomeCanvasItem *item;
	int day_index;
	
	tm = *localtime (&start);

	for (; start <= end; start += 60 * 60 * 24) {
		mktime (&tm); /* normalize the time */

		/* Figure out the day index that corresponds to this time */

		day_index = gnome_month_item_day2index (mitem, tm.tm_mday);
		g_assert (day_index >= 0);

		/* Mark the day box */

		item = gnome_month_item_num2child (mitem, GNOME_MONTH_ITEM_DAY_BOX + day_index);
		gnome_canvas_item_set (item,
				       "fill_color", color_spec_from_prop (COLOR_PROP_MARK_DAY_BG),
				       NULL);

		/* Next day */

		tm.tm_mday++;
	}
}

static void
mark_current_day (GnomeMonthItem *mitem)
{
	struct tm *tm;
	time_t t;
	int day_index;
	GnomeCanvasItem *item;

	t = time (NULL);
	tm = localtime (&t);

	if (((tm->tm_year + 1900) == mitem->year) && (tm->tm_mon == mitem->month)) {
		day_index = gnome_month_item_day2index (mitem, tm->tm_mday);
		item = gnome_month_item_num2child (mitem, GNOME_MONTH_ITEM_DAY_LABEL + day_index);
		gnome_canvas_item_set (item,
				       "fill_color", color_spec_from_prop (COLOR_PROP_CURRENT_DAY_FG),
				       NULL);
	}
}

void
mark_month_item (GnomeMonthItem *mitem, Calendar *cal)
{
	time_t month_begin, month_end;
	GList *list, *l;
	CalendarObject *co;

	month_begin = time_month_begin (time_from_day (mitem->year, mitem->month, 1));
	month_end = time_month_end (month_begin);

	list = calendar_get_events_in_range (cal, month_begin, month_end);

	for (l = list; l; l = l->next) {
		co = l->data;

		/* We clip the event's start and end times to the month's limits */

		mark_event_in_month (mitem, MAX (co->ev_start, month_begin), MIN (co->ev_end, month_end));
	}

	calendar_destroy_event_list (list);

	mark_current_day (mitem);
}

void
unmark_month_item (GnomeMonthItem *mitem)
{
	g_return_if_fail (mitem != NULL);
	g_return_if_fail (GNOME_IS_MONTH_ITEM (mitem));

	/* We have to do this in several calls to gnome_canvas_item_set(), as color_spec_from_prop()
	 * returns a pointer to a static string -- and we need several values.
	 */

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (mitem),
			       "heading_color", color_spec_from_prop (COLOR_PROP_HEADING_COLOR),
			       NULL);

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (mitem),
			       "outline_color", color_spec_from_prop (COLOR_PROP_OUTLINE_COLOR),
			       NULL);

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (mitem),
			       "day_box_color", color_spec_from_prop (COLOR_PROP_EMPTY_DAY_BG),
			       NULL);

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (mitem),
			       "day_color", color_spec_from_prop (COLOR_PROP_DAY_FG),
			       NULL);
}
