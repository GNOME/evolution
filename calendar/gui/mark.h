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


/* Takes a monthly calendar item and marks the days that have events scheduled for them in the
 * specified calendar.  It also highlights the current day.
 */
void mark_month_item (GnomeMonthItem *mitem, Calendar *cal);

/* Unmarks all the days in the specified month item */
void unmark_month_item (GnomeMonthItem *mitem);


#endif
