/* Miscellaneous utility functions for the calendar view widgets
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#include <string.h>
#include "view-utils.h"


/* FIXME: remove this function later */

static GList *
calendar_get_events_in_range (Calendar *cal, time_t start, time_t end, GCompareFunc sort_func)
{
	static iCalObject objs[24];
	static int ready = 0;
	int i;
	GList *list;

	if (!ready) {
		struct tm tm;
		time_t tim;

		ready = 1;

		for (i = 0; i < 24; i++) {
			tim = time (NULL);
			tm = *localtime (&tim);

			tm.tm_hour = i;
			tm.tm_min = 0;
			tm.tm_sec = 0;
			objs[i].dtstart = mktime (&tm);

			tm.tm_hour = i;
			tm.tm_min = 30;
			tm.tm_sec = 0;
			objs[i].dtend = mktime (&tm);

			objs[i].summary = "Ir a chingar a tu madre";
		}
	}

	list = NULL;

	for (i = 0; i < 8; i++)
		list = g_list_append (list, &objs[i]);

	return list;
}

void
view_utils_draw_events (GtkWidget *widget, GdkWindow *window, GdkGC *gc, GdkRectangle *area,
			int flags, Calendar *calendar, time_t start, time_t end)
{
	int font_height;
	int x, y, max_y;
	char buf[512];
	int len;
	struct tm tm_start, tm_end;
	char *str;
	iCalObject *ico;
	GList *the_list, *list;

	gdk_gc_set_clip_rectangle (gc, area);

	font_height = widget->style->font->ascent + widget->style->font->descent;

	max_y = area->y + area->height - font_height * ((flags & VIEW_UTILS_DRAW_SPLIT) ? 2 : 1);

	the_list = calendar_get_events_in_range (calendar, start, end, calendar_compare_by_dtstart);

	for (y = area->y, list = the_list; (y < max_y) && list; y += font_height, list = list->next) {
		ico = list->data;

		tm_start = *localtime (&ico->dtstart);
		tm_end = *localtime (&ico->dtend);
		str = ico->summary;

		if (flags & VIEW_UTILS_DRAW_END) {
			strftime (buf, 512, "%X - ", &tm_start);
			len = strlen (buf);
			strftime (buf + len, 512 - len, "%X    ", &tm_end);
		} else
			strftime (buf, 512, "%X    ", &tm_start);

		gdk_draw_string (window,
				 widget->style->font,
				 gc,
				 area->x,
				 y + widget->style->font->ascent,
				 buf);

		if (flags & VIEW_UTILS_DRAW_SPLIT) {
			y += font_height;
			x = widget->style->font->ascent; /* some indentation */
		} else
			x = gdk_string_width (widget->style->font, buf);

		gdk_draw_string (window,
				 widget->style->font,
				 gc,
				 x,
				 y + widget->style->font->ascent,
				 str);
	}

	g_list_free (the_list);

	gdk_gc_set_clip_rectangle (gc, NULL);
}
