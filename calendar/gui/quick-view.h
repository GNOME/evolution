/* Quick view widget for Gnomecal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx
 */

#ifndef QUICK_VIEW_H
#define QUICK_VIEW_H

#include <libgnome/gnome-defs.h>
#include "gnome-cal.h"


BEGIN_GNOME_DECLS


#define TYPE_QUICK_VIEW            (quick_view_get_type ())
#define QUICK_VIEW(obj)            (GTK_CHECK_CAST ((obj), TYPE_QUICK_VIEW, QuickView))
#define QUICK_VIEW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_QUICK_VIEW, QuickViewClass))
#define IS_QUICK_VIEW(obj)         (GTK_CHECK_TYPE ((obj), TYPE_QUICK_VIEW))
#define IS_QUICK_VIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_QUICK_VIEW))


typedef struct _QuickView QuickView;
typedef struct _QuickViewClass QuickViewClass;

struct _QuickView {
	GtkWindow window;

	GnomeCalendar *calendar;	/* The calendar we are associated to */

	GtkWidget *canvas;		/* The canvas that displays the contents of the quick view */

	int button;			/* The button that was pressed to pop up the quick view */
};

struct _QuickViewClass {
	GtkWindowClass parent_class;
};


/* Standard Gtk function */
GtkType quick_view_get_type (void);

/* Creates a new quick view with the specified title and the specified event list.  It is associated
 * to the specified calendar.  The event list must be a list of CalendarObject structures.
 */
GtkWidget *quick_view_new (GnomeCalendar *calendar, char *title, GList *event_list);

/* Pops up the quick view widget modally and loops until the uses closes it by releasing the mouse
 * button.  You can destroy the quick view when this function returns.
 */
void quick_view_do_popup (QuickView *qv, GdkEventButton *event);


END_GNOME_DECLS

#endif
