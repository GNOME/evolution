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
				       "font", CURRENT_DAY_FONT,
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

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (mitem),
			       "day_font", NORMAL_DAY_FONT,
			       NULL);
}

/* Frees the prelight information in the month item when it is destroyed */
static void
free_prelight_info (GtkObject *object, gpointer data)
{
	g_free (gtk_object_get_data (object, "prelight_info"));
}

/* Handles EnterNotify and LeaveNotify events from the month item's day groups, and performs
 * appropriate prelighting.
 */
static gint
day_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	GnomeCanvasItem *mitem;
	GnomeCanvasItem *box;
	int child_num, day;
	gulong *day_pixels;
	GetPrelightColorFunc func;
	gpointer func_data;
	char *spec;
	GdkColor color;

	mitem = data;
	child_num = gnome_month_item_child2num (GNOME_MONTH_ITEM (mitem), item);
	day = gnome_month_item_num2day (GNOME_MONTH_ITEM (mitem), child_num);

	if (day == 0)
		return FALSE; /* it was a day outside the month's range */

	child_num -= GNOME_MONTH_ITEM_DAY_GROUP;
	box = gnome_month_item_num2child (GNOME_MONTH_ITEM (mitem), GNOME_MONTH_ITEM_DAY_BOX + child_num);

	day_pixels = gtk_object_get_data (GTK_OBJECT (mitem), "prelight_info_pixels");
	func = gtk_object_get_data (GTK_OBJECT (mitem), "prelight_info_func");
	func_data = gtk_object_get_data (GTK_OBJECT (mitem), "prelight_info_data");

	switch (event->type) {
	case GDK_ENTER_NOTIFY:
		spec = (* func) (func_data);
		gnome_canvas_item_set (box,
				       "fill_color", spec,
				       NULL);
		break;

	case GDK_LEAVE_NOTIFY:
		color.pixel = day_pixels[child_num];
		gnome_canvas_item_set (box,
				       "fill_color_gdk", &color,
				       NULL);
		break;

	default:
		break;
	}

	return FALSE;
}

void
month_item_prepare_prelight (GnomeMonthItem *mitem, GetPrelightColorFunc func, gpointer func_data)
{
	gulong *day_pixels;
	GnomeCanvasItem *day_group;
	GnomeCanvasItem *box;
	GtkArg arg;
	GdkColor *color;
	int i;

	day_pixels = gtk_object_get_data (GTK_OBJECT (mitem), "prelight_info_pixels");

	/* Set up the buffer for day background colors and attach it to the month item, if necessary */

	if (!day_pixels) {
		/* Create the buffer and attach it */

		day_pixels = g_new (gulong, 42);
		gtk_object_set_data (GTK_OBJECT (mitem), "prelight_info_pixels", day_pixels);
		gtk_object_set_data (GTK_OBJECT (mitem), "prelight_info_func", func);
		gtk_object_set_data (GTK_OBJECT (mitem), "prelight_info_data", func_data);
		gtk_signal_connect (GTK_OBJECT (mitem), "destroy",
				    (GtkSignalFunc) free_prelight_info,
				    NULL);

		/* Connect the appropriate signals to perform prelighting */

		for (i = 0; i < 42; i++) {
			day_group = gnome_month_item_num2child (GNOME_MONTH_ITEM (mitem), GNOME_MONTH_ITEM_DAY_GROUP + i);
			gtk_signal_connect (GTK_OBJECT (day_group), "event",
					    (GtkSignalFunc) day_event,
					    mitem);
		}
	}

	/* Fetch the background colors from the day boxes and store them in the prelight info */

	for (i = 0; i < 42; i++) {
		box = gnome_month_item_num2child (mitem, GNOME_MONTH_ITEM_DAY_BOX + i);

		arg.name = "fill_color_gdk";
		gtk_object_getv (GTK_OBJECT (box), 1, &arg);

		color = GTK_VALUE_BOXED (arg);
		day_pixels[i] = color->pixel;
		g_free (color);
	}
}

char *
default_prelight_func (gpointer data)
{
	return color_spec_from_prop (COLOR_PROP_PRELIGHT_DAY_BG);
}
