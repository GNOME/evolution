/* Functions to mark calendars
 *
 * Copyright (C) 1998 Red Hat Software, Inc.
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#ifndef MARK_H
#define MARK_H

#include "calendar.h"
#include "gnome-month-item.h"


/* These are the fonts used for the montly calendars */

#define NORMAL_DAY_FONT  "-adobe-helvetica-medium-r-normal--10-*-72-72-p-*-iso8859-1"
#define CURRENT_DAY_FONT "-adobe-helvetica-bold-r-normal--12-*-72-72-p-*-iso8859-1"


/* Takes a monthly calendar item and marks the days that have events scheduled for them in the
 * specified calendar.  It also highlights the current day.
 */
void mark_month_item (GnomeMonthItem *mitem, Calendar *cal);

/* Unmarks all the days in the specified month item */
void unmark_month_item (GnomeMonthItem *mitem);

/* Prepares a monthly calendar item to prelight when the mouse goes over the days.  If it is called
 * on a month item that had already been prepared, it updates the internal color buffers -- you need
 * to do this if you re-mark the month item, or if you change the global color configuration.  The
 * specified function is used to query the prelight colors; it must return a color spec.
 */

typedef char * (* GetPrelightColorFunc) (gpointer data);

void month_item_prepare_prelight (GnomeMonthItem *mitem, GetPrelightColorFunc func, gpointer func_data);

/* This is the default prelight function you can use for most puposes.  You can use NULL as the
 * func_data.
 */
char *default_prelight_func (gpointer data);


#endif
