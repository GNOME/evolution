/* Full day widget for gncal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <quartic@gimp.org>
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

	GList *children;		/* container children */
	gpointer drag_info;		/* internal drag information */

	GdkCursor *up_down_cursor;	/* for dragging children */
	GdkCursor *beam_cursor;		/* for the text widgets */
	GdkGC     *recur_gc;            /* The gc used to draw the recur image */
	GdkGC     *bell_gc;             /* The gc used to draw on imlib windows */

};

struct _GncalFullDayClass {
	GtkContainerClass parent_class;

	void (* range_activated) (GncalFullDay *fullday);
};


guint      gncal_full_day_get_type              (void);
GtkWidget *gncal_full_day_new                   (GnomeCalendar *calendar, time_t lower, time_t upper);

void       gncal_full_day_update                (GncalFullDay *fullday, iCalObject *ico, int flags);
void       gncal_full_day_set_bounds            (GncalFullDay *fullday, time_t lower, time_t upper);

/* Returns the selected range in lower and upper.  If nothing is
 * selected, return value is FALSE, otherwise it is TRUE.
 * The lower and upper values are always set to proper values, regardless of
 * the selection value 
 */
int        gncal_full_day_selection_range       (GncalFullDay *fullday, time_t *lower, time_t *upper);

void       gncal_full_day_focus_child           (GncalFullDay *fullday, iCalObject *ico);

int        gncal_full_day_get_day_start_yoffset (GncalFullDay *fullday);

END_GNOME_DECLS

#endif
