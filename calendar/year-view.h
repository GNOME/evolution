/* Year view display for Gnomecal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Authors: Arturo Espinosa <arturo@nuclecu.unam.mx>
 *          Federico Mena <federico@nuclecu.unam.mx>
 */

#ifndef YEAR_VIEW_H
#define YEAR_VIEW_H

#include <libgnome/gnome-defs.h>
#include "gnome-cal.h"
#include "gnome-month-item.h"


BEGIN_GNOME_DECLS


#define TYPE_YEAR_VIEW            (year_view_get_type ())
#define YEAR_VIEW(obj)            (GTK_CHECK_CAST ((obj), TYPE_YEAR_VIEW, YearView))
#define YEAR_VIEW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_YEAR_VIEW, YearViewClass))
#define IS_YEAR_VIEW(obj)         (GTK_CHECK_TYPE ((obj), TYPE_YEAR_VIEW))
#define IS_YEAR_VIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_YEAR_VIEW))


typedef struct _YearView YearView;
typedef struct _YearViewClass YearViewClass;

struct _YearView {
	GnomeCanvas canvas;

	GnomeCalendar *calendar;	/* The calendar we are associated to */

	int year;			/* The year we are displaying */

	GnomeCanvasItem *heading;	/* Big heading with year */
	GnomeCanvasItem *titles[12];	/* Titles for months */
	GnomeCanvasItem *mitems[12];	/* Month items */

	int old_marked_day;		/* The day that is marked as the current day */

	int min_width;			/* Minimum dimensions of year view, used for size_request*/
	int min_height;

	guint idle_id;			/* ID of idle handler for resize */

	int need_resize : 1;		/* Specifies whether we need to resize the canvas items or not */
};

struct _YearViewClass {
	GnomeCanvasClass parent_class;
};


/* Standard Gtk function */
GtkType year_view_get_type (void);

/* Creates a new year view widget associated to the specified calendar */
GtkWidget *year_view_new (GnomeCalendar *calendar, time_t year);

/* Notifies the year view that a calendar object has changed */
void year_view_update (YearView *yv, iCalObject *ico, int flags);

/* Notifies the year view about a change of date */
void year_view_set (YearView *yv, time_t year);

/* Notifies the year view that the time format has changed */
void year_view_time_format_changed (YearView *yv);

/* Notifies the year view that colors have changed */
void year_view_colors_changed (YearView *yv);


END_GNOME_DECLS

#endif
