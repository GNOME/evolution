/* Week view display for gncal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#ifndef WEEK_VIEW_H
#define WEEK_VIEW_H

#include <libgnome/gnome-defs.h>
#include "gnome-cal.h"

BEGIN_GNOME_DECLS


#define TYPE_WEEK_VIEW            (week_view_get_type ())
#define WEEK_VIEW(obj)            (GTK_CHECK_CAST ((obj), TYPE_WEEK_VIEW, WeekView))
#define WEEK_VIEW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_WEEK_VIEW, WeekViewClass))
#define IS_WEEK_VIEW(obj)         (GTK_CHECK_TYPE ((obj), TYPE_WEEK_VIEW))
#define IS_WEEK_VIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_WEEK_VIEW))


typedef struct _WeekView WeekView;
typedef struct _WeekViewClass WeekViewClass;

struct _WeekView {
	GnomeCanvas canvas;

	GnomeCalendar *calendar;	/* The calendar we are associated to */

	time_t week;			/* Start of the week we are viewing */

	GnomeCanvasItem *title;		/* The title of the week view */
};

struct _WeekViewClass {
	GnomeCanvasClass parent_class;
};


/* Standard Gtk function */
GtkType week_view_get_type (void);

/* Creates a new week view associated to the specified calendar */
GtkWidget *week_view_new (GnomeCalendar *calendar, time_t week);

/* Notifies the week view that a calendar object has changed */
void week_view_update (WeekView *wv, iCalObject *ico, int flags);

/* Notifies the week view about a change of date */
void week_view_set (WeekView *wv, time_t week);

/* Notifies the week view that the time format has changed */
void week_view_time_format_changed (WeekView *wv);

/* Notifies the week view that the colors have changed */
void week_view_colors_changed (WeekView *wv);


END_GNOME_DECLS

#endif
