/* Month view display for gncal
 *
 * Copyright (C) 1998 Red Hat Software, Inc.
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#ifndef MONTH_VIEW_H
#define MONTH_VIEW_H

#include <libgnome/gnome-defs.h>
#include "gnome-cal.h"
#include "gnome-month-item.h"


BEGIN_GNOME_DECLS


#define TYPE_MONTH_VIEW            (month_view_get_type ())
#define MONTH_VIEW(obj)            (GTK_CHECK_CAST ((obj), TYPE_MONTH_VIEW, MonthView))
#define MONTH_VIEW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_MONTH_VIEW, MonthViewClass))
#define IS_MONTH_VIEW(obj)         (GTK_CHECK_TYPE ((obj), TYPE_MONTH_VIEW))
#define IS_MONTH_VIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_MONTH_VIEW))


typedef struct _MonthView MonthView;
typedef struct _MonthViewClass MonthViewClass;

struct _MonthView {
	GnomeCanvas canvas;

	GnomeCalendar *calendar;	/* The calendar we are associated to */

	GnomeCanvasItem *mitem;		/* The canvas month item used by this month view */
};

struct _MonthViewClass {
	GnomeCanvasClass parent_class;
};


/* Standard Gtk function */
GtkType month_view_get_type (void);

/* Creates a new month view widget associated to the specified calendar */
GtkWidget *month_view_new (GnomeCalendar *calendar);


END_GNOME_DECLS

#endif
