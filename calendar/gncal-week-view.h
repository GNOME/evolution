/* Week view composite widget for gncal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <quartic@gimp.org>
 */

#ifndef WEEK_VIEW_H
#define WEEK_VIEW_H


#include <gtk/gtkvbox.h>
#include <gtk/gtktable.h>
#include <gtk/gtkvbox.h>
#include <libgnome/gnome-defs.h>
#include "gnome-cal.h"
#include "gncal-day-view.h"

BEGIN_GNOME_DECLS


#define GNCAL_WEEK_VIEW(obj)         GTK_CHECK_CAST (obj, gncal_week_view_get_type (), GncalWeekView)
#define GNCAL_WEEK_VIEW_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, gncal_week_view_get_type (), GncalWeekViewClass)
#define GNCAL_IS_WEEK_VIEW(obj)      GTK_CHECK_TYPE (obj, gncal_week_view_get_type ())


typedef struct _GncalWeekView GncalWeekView;
typedef struct _GncalWeekViewClass GncalWeekViewClass;

struct _GncalWeekView {
	GtkVBox      box;

	GnomeCalendar *calendar;	/* the calendar we are associated to */

	struct tm start_of_week;

	GncalDayView *days[7];		/* the day view widgets */
	GtkCalendar  *gtk_calendar;     /* At least for now; see the FIXME comments in the .c file */
	GtkWidget    *label;
};

struct _GncalWeekViewClass {
	GtkVBoxClass parent_class;
};


guint gncal_week_view_get_type (void);
GtkWidget *gncal_week_view_new (GnomeCalendar *calendar, time_t start_of_week);

void gncal_week_view_update (GncalWeekView *wview, iCalObject *ico, int flags);
void gncal_week_view_set (GncalWeekView *wview, time_t start_of_week);
void gncal_week_view_time_format_changed (GncalWeekView *wview);


END_GNOME_DECLS

#endif
