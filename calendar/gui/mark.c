/* Evolution calendar - Functions to mark calendars
 *
 * Copyright (C) 1998 Red Hat Software, Inc.
 *
 * Author: Federico Mena-Quintero <federico@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <cal-util/timeutil.h>
#include "gnome-cal.h"
#include "calendar-commands.h"
#include "mark.h"

/* Closure data */
struct minfo
{
	GnomeMonthItem *mitem;
	time_t start;
	time_t end;
};



/* Frees the specified data when an object is destroyed */
static void
free_data (GtkObject *object, gpointer data)
{
	g_free (data);
}

/* If the array of "marked" attributes for the days in a a month item has not been created yet, this
 * function creates the array and clears it.  Otherwise, it just returns the existing array.
 */
static char *
get_attributes (GnomeMonthItem *mitem)
{
	char *attrs;

	attrs = gtk_object_get_data (GTK_OBJECT (mitem), "day_mark_attributes");

	if (!attrs) {
		attrs = g_new0 (char, 42);
		gtk_object_set_data (GTK_OBJECT (mitem), "day_mark_attributes", attrs);
		gtk_signal_connect (GTK_OBJECT (mitem), "destroy",
				    (GtkSignalFunc) free_data,
				    attrs);
	}

	return attrs;
}

void
colorify_month_item (GnomeMonthItem *mitem, GetColorFunc func, gpointer func_data)
{
	g_return_if_fail (mitem != NULL);
	g_return_if_fail (GNOME_IS_MONTH_ITEM (mitem));
	g_return_if_fail (func != NULL);

	unmark_month_item (mitem);

	/* We have to do this in several calls to gnome_canvas_item_set(), as color_spec_from_prop()
	 * returns a pointer to a static string -- and we need several values.
	 */

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (mitem),
			       "heading_color", (* func) (COLOR_PROP_HEADING_COLOR, func_data),
			       NULL);

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (mitem),
			       "outline_color", (* func) (COLOR_PROP_OUTLINE_COLOR, func_data),
			       NULL);

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (mitem),
			       "day_box_color", (* func) (COLOR_PROP_EMPTY_DAY_BG, func_data),
			       NULL);

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (mitem),
			       "day_color", (* func) (COLOR_PROP_DAY_FG, func_data),
			       NULL);
}

/* In the month item, marks all the days that are touched by the specified time span.  Assumes that
 * the time span is completely contained within the month.  The array of day attributes is modified
 * accordingly.
 */
static void
mark_event_in_month (GnomeMonthItem *mitem, time_t start, time_t end)
{
	struct tm tm;
	int day_index;

	tm = *localtime (&start);

	for (; start <= end; start += 60 * 60 * 24) {
		mktime (&tm); /* normalize the time */

		/* Figure out the day index that corresponds to this time */

		day_index = gnome_month_item_day2index (mitem, tm.tm_mday);
		g_assert (day_index >= 0);

		/* Mark the day box */

		mark_month_item_index (mitem, day_index, default_color_func, NULL);

		/* Next day */

		tm.tm_mday++;
	}
}

static gboolean
mark_month_item_cb (CalComponent *comp, time_t istart, time_t iend, gpointer data)
{
	struct minfo *mi = (struct minfo *)data;

	mark_event_in_month (mi->mitem, MAX (istart, mi->start), MIN (iend, mi->end));

	return TRUE;
}

void
mark_month_item (GnomeMonthItem *mitem, GnomeCalendar *gcal)
{
	CalClient *client;
	struct minfo mi;

	g_return_if_fail (mitem != NULL);
	g_return_if_fail (GNOME_IS_MONTH_ITEM (mitem));
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	client = gnome_calendar_get_cal_client (gcal);

	mi.mitem = mitem;
	mi.start = time_month_begin (time_from_day (mitem->year, mitem->month, 1));
	mi.end = time_month_end (mi.start);
	
	cal_client_generate_instances (client, CALOBJ_TYPE_EVENT, mi.start, mi.end,
				       mark_month_item_cb, &mi);
}


void
mark_month_item_index (GnomeMonthItem *mitem, int index, GetColorFunc func, gpointer func_data)
{
	char *attrs;
	GnomeCanvasItem *item;

	g_return_if_fail (mitem != NULL);
	g_return_if_fail (GNOME_IS_MONTH_ITEM (mitem));
	g_return_if_fail ((index >= 0) && (index < 42));
	g_return_if_fail (func != NULL);

	attrs = get_attributes (mitem);

	attrs[index] = TRUE;

	item = gnome_month_item_num2child (mitem, GNOME_MONTH_ITEM_DAY_BOX + index);
	gnome_canvas_item_set (item,
			       "fill_color", (* func) (COLOR_PROP_MARK_DAY_BG, func_data),
			       NULL);
}

void
unmark_month_item (GnomeMonthItem *mitem)
{
	int i;
	char *attrs;
	GnomeCanvasItem *item;

	g_return_if_fail (mitem != NULL);
	g_return_if_fail (GNOME_IS_MONTH_ITEM (mitem));

	attrs = get_attributes (mitem);

	/* Find marked days and unmark them by turning off their marked attribute flag and changing
	 * the color.
	 */

	for (i = 0; i < 42; i++)
		if (attrs[i]) {
			attrs[i] = FALSE;

			item = gnome_month_item_num2child (mitem, GNOME_MONTH_ITEM_DAY_BOX + i);
			gnome_canvas_item_set (item,
					       "fill_color", color_spec_from_prop (COLOR_PROP_EMPTY_DAY_BG),
					       NULL);
		}
}

/* Handles EnterNotify and LeaveNotify events from the month item's day groups, and performs
 * appropriate prelighting.
 */
static gint
day_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	GnomeMonthItem *mitem;
	GnomeCanvasItem *box;
	int child_num, day;
	GetColorFunc func;
	gpointer func_data;
	char *color;
	char *attrs;

	/* We only accept enters and leaves */

	if (!((event->type == GDK_ENTER_NOTIFY) || (event->type == GDK_LEAVE_NOTIFY)))
		return FALSE;

	/* Get index information */

	mitem = GNOME_MONTH_ITEM (data);
	child_num = gnome_month_item_child2num (mitem, item);
	day = gnome_month_item_num2day (mitem, child_num);

	if (day == 0)
		return FALSE; /* it was a day outside the month's range */

	child_num -= GNOME_MONTH_ITEM_DAY_GROUP;
	box = gnome_month_item_num2child (mitem, GNOME_MONTH_ITEM_DAY_BOX + child_num);

	/* Get colors */

	func = gtk_object_get_data (GTK_OBJECT (mitem), "prelight_color_func");
	func_data = gtk_object_get_data (GTK_OBJECT (mitem), "prelight_color_data");

	/* Now actually set the proper color in the item */

	switch (event->type) {
	case GDK_ENTER_NOTIFY:
		color = (* func) (COLOR_PROP_PRELIGHT_DAY_BG, func_data);
		gnome_canvas_item_set (box,
				       "fill_color", color,
				       NULL);
		break;

	case GDK_LEAVE_NOTIFY:
		attrs = get_attributes (mitem);
		color = (* func) (attrs[child_num] ? COLOR_PROP_MARK_DAY_BG : COLOR_PROP_EMPTY_DAY_BG,
				  func_data);
		gnome_canvas_item_set (box,
				       "fill_color", color,
				       NULL);
		break;

	default:
		g_assert_not_reached ();
	}

	return TRUE;
}

void
month_item_prepare_prelight (GnomeMonthItem *mitem, GetColorFunc func, gpointer func_data)
{
	GnomeCanvasItem *day_group;
	int i;

	g_return_if_fail (mitem != NULL);
	g_return_if_fail (GNOME_IS_MONTH_ITEM (mitem));
	g_return_if_fail (func != NULL);

	/* Store the function in the object data */

	gtk_object_set_data (GTK_OBJECT (mitem), "prelight_color_func", func);
	gtk_object_set_data (GTK_OBJECT (mitem), "prelight_color_data", func_data);

	/* Connect the appropriate signals to perform prelighting */

	for (i = 0; i < 42; i++) {
		day_group = gnome_month_item_num2child (GNOME_MONTH_ITEM (mitem), GNOME_MONTH_ITEM_DAY_GROUP + i);
		gtk_signal_connect (GTK_OBJECT (day_group), "event",
				    (GtkSignalFunc) day_event,
				    mitem);
	}
}

char *
default_color_func (ColorProp propnum, gpointer data)
{
	return color_spec_from_prop (propnum);
}



