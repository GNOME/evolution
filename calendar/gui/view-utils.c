/* Miscellaneous utility functions for the calendar view widgets
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#include <string.h>
#include "view-utils.h"


/* FIXME: remove this function later */

#if 0
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
#endif
void
view_utils_draw_events (GtkWidget *widget, GdkWindow *window, GdkGC *gc, GdkRectangle *area,
			int flags, GList *events, time_t start, time_t end)
{
	int font_height;
	int x, y, max_y;
	char buf[512];
	int len;
	struct tm tm_start, tm_end;
	char *str;
	iCalObject *ico;
	GList *list;

	gdk_gc_set_clip_rectangle (gc, area);

	font_height = widget->style->font->ascent + widget->style->font->descent;

	max_y = area->y + area->height - font_height * ((flags & VIEW_UTILS_DRAW_SPLIT) ? 2 : 1);

	for (y = area->y, list = events; (y < max_y) && list; y += font_height, list = list->next) {
		ico = list->data;

		tm_start = *localtime (&ico->dtstart);
		tm_end = *localtime (&ico->dtend);
		str = ico->summary;

		if (flags & VIEW_UTILS_DRAW_END) {
			strftime (buf, 512, "%R%p - ", &tm_start);
			len = strlen (buf);
			strftime (buf + len, 512 - len, "%R%p    ", &tm_end);
		} else
			strftime (buf, 512, "%R%p    ", &tm_start);

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
	gdk_gc_set_clip_rectangle (gc, NULL);
}

void
view_utils_draw_textured_frame (GtkWidget *widget, GdkWindow *window, GdkRectangle *rect, GtkShadowType shadow)
{
	int x, y;
	int xthick, ythick;
	GdkGC *light_gc, *dark_gc;

	gdk_draw_rectangle (window,
			    widget->style->bg_gc[GTK_STATE_NORMAL],
			    TRUE,
			    rect->x, rect->y,
			    rect->width, rect->height);

	light_gc = widget->style->light_gc[GTK_STATE_NORMAL];
	dark_gc = widget->style->dark_gc[GTK_STATE_NORMAL];

	xthick = widget->style->klass->xthickness;
	ythick = widget->style->klass->ythickness;

	gdk_gc_set_clip_rectangle (light_gc, rect);
	gdk_gc_set_clip_rectangle (dark_gc, rect);

	for (y = rect->y + ythick; y < (rect->y + rect->height - ythick); y += 3)
		for (x = rect->x + xthick; x < (rect->x + rect->width - xthick); x += 6) {
			gdk_draw_point (window, light_gc, x, y);
			gdk_draw_point (window, dark_gc, x + 1, y + 1);

			gdk_draw_point (window, light_gc, x + 3, y + 1);
			gdk_draw_point (window, dark_gc, x + 4, y + 2);
		}
	
	gdk_gc_set_clip_rectangle (light_gc, NULL);
	gdk_gc_set_clip_rectangle (dark_gc, NULL);

	gtk_draw_shadow (widget->style, window,
			 GTK_STATE_NORMAL, shadow,
			 rect->x, rect->y,
			 rect->width, rect->height);
}
