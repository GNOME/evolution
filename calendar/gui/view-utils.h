/* Miscellaneous utility functions for the calendar view widgets
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <quartic@gimp.org>
 */

#ifndef VIEW_UTILS_H
#define VIEW_UTILS_H


#include <gtk/gtk.h>
#include "calendar.h"


enum {
	VIEW_UTILS_DRAW_END   = 1 << 0,
	VIEW_UTILS_DRAW_SPLIT = 1 << 1
};


void view_utils_draw_events (GtkWidget *widget, GdkWindow *window, GdkGC *gc, GdkRectangle *area,
			     int flags, GList *events, time_t start, time_t end);

void view_utils_draw_textured_frame (GtkWidget *widget, GdkWindow *window, GdkRectangle *rect, GtkShadowType shadow);


#endif
