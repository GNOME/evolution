/* Miscellaneous utility functions for the calendar view widgets
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Authors: Federico Mena <quartic@gimp.org>
 *          Miguel de Icaza <miguel@kernel.org>
 */

#include <string.h>
#include "view-utils.h"
#include <libgnomeui/gnome-icon-text.h>

int am_pm_flag = 0;

static char *
nicetime (struct tm *tm)
{
	static char buf [20];
	char *p = buf;
	
	if (am_pm_flag){
		if (tm->tm_min)
			strftime (buf, sizeof (buf), "%l:%M%p", tm);
		else
			strftime (buf, sizeof (buf), "%l%p", tm);
	} else {
		if (tm->tm_min)
			strftime (buf, sizeof (buf), "%H:%M", tm);
		else
			strftime (buf, sizeof (buf), "%H", tm);
	}
	while (*p == ' ')
		p++;
	return p;
}

typedef struct {
	GnomeIconTextInfo *layout;
	int lines;
	int assigned_lines;
} line_info_t;

void
view_utils_draw_events (GtkWidget *widget, GdkWindow *window, GdkGC *gc, GdkRectangle *area,
			int flags, GList *events, time_t start, time_t end)
{
	GdkFont *font = widget->style->font;
	int font_height;
	int y, max_y, items, i, need_more, nlines, base, extra;
	GList *list;
	line_info_t *lines;

	if (events == NULL)
		return;
	
	items = g_list_length (events);
	lines = g_new0 (line_info_t, items);
	
	font_height = font->ascent + font->descent;
	max_y = area->y + area->height - font_height * ((flags & VIEW_UTILS_DRAW_SPLIT) ? 2 : 1);

	/*
	 * Layout all the lines, measure the space needs
	 */
	for (i = 0, list = events; list; list = list->next, i++){
		CalendarObject *co = list->data;
		struct tm tm_start, tm_end;
		iCalObject *ico = co->ico;
		char buf [60];
		char *full_text;

		tm_start = *localtime (&co->ev_start);
		tm_end = *localtime   (&co->ev_end);

		strcpy (buf, nicetime (&tm_start));
			
		if (flags & VIEW_UTILS_DRAW_END){
			strcat (buf, "-");
			strcat (buf, nicetime (&tm_end));
		}

		full_text = g_strconcat (buf, ": ", ico->summary, NULL);
		lines [i].layout = gnome_icon_layout_text (
			font, full_text, "\n -,.;:=#", area->width, TRUE);
		lines [i].lines = g_list_length (lines [i].layout->rows);

		g_free (full_text);
	}

	/*
	 * Compute how many lines we will give to each row
	 */
	nlines = 1 + max_y / font_height;
	base = nlines / items;
	extra = nlines % items;
	need_more = 0;
	
	for (i = 0; i < items; i++){
		if (lines [i].lines <= base){
			extra += base - lines [i].lines;
			lines [i].assigned_lines = lines [i].lines;
		} else {
			need_more++;
			lines [i].assigned_lines = base;
		}
	}

	/*
	 * use any extra space
	 */
	while (need_more && extra > 0){
		need_more = 0;
		
		for (i = 0; i < items; i++){
			if (lines [i].lines > lines [i].assigned_lines){
				lines [i].assigned_lines++;
				extra--;
			}

			if (extra == 0)
				break;
			
			if (lines [i].lines > lines [i].assigned_lines)
				need_more = 1;
		}
	}

	/*
	 * Draw the information
	 */
	gdk_gc_set_clip_rectangle (gc, area);
	y = area->y;
	for (i = 0; i < items; i++){
		int line;

		list = lines [i].layout->rows;
		
		for (line = 0; line < lines [i].assigned_lines; line++){
			GnomeIconTextInfoRow *row = list->data;

			list = list->next;

			if (row)
				gdk_draw_string (
					window, font, gc,
					area->x, y + font->ascent,
					row->text);
			y += font_height;
		}
	}

	gdk_gc_set_clip_rectangle (gc, NULL);

	/*
	 * Free resources.
	 */

	for (i = 0; i < items; i++)
		gnome_icon_text_info_free (lines [i].layout);
	g_free (lines);	
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
