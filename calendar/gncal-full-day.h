/* Full day widget for gncal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#ifndef GNCAL_FULL_DAY_H
#define GNCAL_FULL_DAY_H


#include <gtk/gtkcontainer.h>
#include <libgnome/gnome-defs.h>
#include "calendar.h"
#include "gnome-cal.h"


BEGIN_GNOME_DECLS


#define GNCAL_FULL_DAY(obj)         GTK_CHECK_CAST (obj, gncal_full_day_get_type (), GncalFullDay)
#define GNCAL_FULL_DAY_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, gncal_full_day_get_type (), GncalFullDayClass)
#define GNCAL_IS_FULL_DAY(obj)      GTK_CHECK_TYPE (obj, gncal_full_day_get_type ())


typedef struct _GncalFullDay GncalFullDay;
typedef struct _GncalFullDayClass GncalFullDayClass;

struct _GncalFullDay {
	GtkContainer container;

	GnomeCalendar *calendar;	/* the calendar we are associated to */

	time_t lower;			/* lower time to display */
	time_t upper;			/* upper time to display */
	int interval;			/* interval between rows in minutes */
};

struct _GncalFullDayClass {
	GtkContainerClass parent_class;
};


guint      gncal_full_day_get_type   (void);
GtkWidget *gncal_full_day_new        (GnomeCalendar *calendar, time_t lower, time_t upper);

void       gncal_full_day_update     (GncalFullDay *fullday);
void       gncal_full_day_set_bounds (GncalFullDay *fullday, time_t lower, time_t upper);

END_GNOME_DECLS

#endif
