/* Miscellaneous utility functions for the calendar view widgets
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Authors: Federico Mena <quartic@gimp.org>
 *          Miguel de Icaza <miguel@kernel.org>
 */

#include <string.h>
#include "view-utils.h"

int am_pm_flag = 0;

static char *
nicetime (struct tm *tm)
{
	static char buf [20];
		
	if (am_pm_flag){
		if (tm->tm_min)
			strftime (buf, sizeof (buf), "%l:%M%p", tm);
		else
			strftime (buf, sizeof (buf), "%l%p", tm);
	} else {
		if (tm->tm_min)
			strftime (buf, sizeof (buf), "%k:%M", tm);
		else
			strftime (buf, sizeof (buf), "%k", tm);
	}
	return buf;
}

void
view_utils_draw_events (GtkWidget *widget, GdkWindow *window, GdkGC *gc, GdkRectangle *area,
			int flags, GList *events, time_t start, time_t end)
{
	int font_height;
	int x, y, max_y;
	char buf [40];
	struct tm tm_start, tm_end;
	char *str;
	iCalObject *ico;
	GList *list;

	gdk_gc_set_clip_rectangle (gc, area);

	font_height = widget->style->font->ascent + widget->style->font->descent;

	max_y = area->y + area->height - font_height * ((flags & VIEW_UTILS_DRAW_SPLIT) ? 2 : 1);

	for (y = area->y, list = events; (y < max_y) && list; y += font_height, list = list->next) {
		CalendarObject *co = list->data;
		ico = co->ico;

		tm_start = *localtime (&co->ev_start);
		tm_end = *localtime   (&co->ev_end);
		str = ico->summary;

		strcpy (buf, nicetime (&tm_start));
			
		if (flags & VIEW_UTILS_DRAW_END){
			strcat (buf, "-");
			strcat (buf, nicetime (&tm_end));
		}
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

void
popup_menu (struct menu_item *items, int nitems, guint32 time)
{
	GtkWidget *menu;
	GtkWidget *item;
	int i;

	menu = gtk_menu_new (); /* FIXME: this baby is never freed */

	for (i = 0; i < nitems; i++) {
		if (items[i].text) {
			item = gtk_menu_item_new_with_label (_(items[i].text));
			gtk_signal_connect (GTK_OBJECT (item), "activate",
					    items[i].callback,
					    items[i].data);
			gtk_widget_set_sensitive (item, items[i].sensitive);
		} else
			item = gtk_menu_item_new ();

		gtk_widget_show (item);
		gtk_menu_append (GTK_MENU (menu), item);
	}

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 3, time);
}
