/* Week view composite widget for gncal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Arturo Espinosa <arturo@nuclecu.unam.mx>
 * 
 * Heavily based on Federico Mena's week view.
 * 
 */

#ifndef YEAR_VIEW_H
#define YEAR_VIEW_H

#include <time.h>
#include <gtk/gtktable.h>
#include <libgnome/gnome-defs.h>
#include <libgnomeui/gtkcalendar.h>

#include "gnome-cal.h"

BEGIN_GNOME_DECLS


#define GNCAL_YEAR_VIEW(obj)         GTK_CHECK_CAST (obj, gncal_year_view_get_type (), GncalYearView)
#define GNCAL_YEAR_VIEW_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, gncal_year_view_get_type (), GncalYearViewClass)
#define GNCAL_IS_YEAR_VIEW(obj)      GTK_CHECK_TYPE (obj, gncal_year_view_get_type ())


typedef struct _GncalYearView GncalYearView;
typedef struct _GncalYearViewClass GncalYearViewClass;

struct _GncalYearView {
	GtkTable table;

	GnomeCalendar *gcal;        /* The calendar we are associated to */
	GtkWidget *calendar[12];    /* one calendar per month */
        guint       handler[12];    /* for (un)blocking the calendars */
	
	GtkWidget *year_label;
	gint year;
};

struct _GncalYearViewClass {
	GtkTableClass parent_class;
};


guint      gncal_year_view_get_type       (void);
GtkWidget *gncal_year_view_new            (GnomeCalendar *calendar, time_t date);
void       gncal_year_view_set            (GncalYearView *yview,    time_t date);
void       gncal_year_view_update         (GncalYearView *yview, iCalObject *ico, int flags);


END_GNOME_DECLS

#endif
