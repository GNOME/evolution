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

#define HEADING_FONTSET         "-adobe-helvetica-medium-r-*-*-14-*-*-*-p-*-*-*,-cronyx-helvetica-medium-r-normal-*-17-*-*-*-p-*-koi8-r,-*-*-medium-r-*-*-14-*-*-*-*-*-ksc5601.1987-0,*"
#define TITLE_FONTSET           "-adobe-helvetica-bold-r-normal--12-*-*-*-p-*-*-*,-cronyx-helvetica-medium-r-normal-*-14-*-*-*-p-*-koi8-r,-*-*-bold-r-normal--12-*-*-*-*-*-ksc5601.1987-0,*"
#define DAY_HEADING_FONTSET     "-adobe-helvetica-medium-r-normal--10-*-*-*-p-*-*-*,-cronyx-helvetica-medium-r-normal-*-11-*-*-*-p-*-koi8-r,-*-*-medium-r-normal--10-*-*-*-*-*-ksc5601.1987-0,*"
#define NORMAL_DAY_FONTSET      "-adobe-helvetica-medium-r-normal--10-*-*-*-p-*-*-*,-cronyx-helvetica-medium-r-normal-*-11-*-*-*-p-*-koi8-r,-*-*-medium-r-normal--10-*-*-*-*-*-ksc5601.1987-0,*"
#define CURRENT_DAY_FONTSET     "-adobe-helvetica-bold-r-normal--12-*-*-*-p-*-*-*,-cronyx-helvetica-medium-r-normal-*-14-*-*-*-p-*-koi8-r,-*-*-bold-r-normal--12-*-*-*-*-*-ksc5601.1987-0,*"
#define BIG_DAY_HEADING_FONTSET "-adobe-helvetica-bold-r-normal--14-*-*-*-p-*-*-*,-cronyx-helvetica-bold-r-normal-*-17-*-*-*-p-*-koi8-r,-*-*-bold-r-normal--14-*-*-*-*-*-ksc5601.1987-0,*"
#define BIG_NORMAL_DAY_FONTSET  "-adobe-helvetica-medium-r-normal--14-*-*-*-p-*-*-*,-cronyx-helvetica-medium-r-normal-*-17-*-*-*-p-*-koi8-r,-*-*-medium-r-normal--14-*-*-*-*-*-ksc5601.1987-0,*"
#define BIG_CURRENT_DAY_FONTSET "-adobe-helvetica-bold-r-normal--14-*-*-*-p-*-*-*,-cronyx-helvetica-bold-r-normal-*-17-*-*-*-p-*-koi8-r,-*-*-bold-r-normal--14-*-*-*-*-*-ksc5601.1987-0,*"
#define EVENT_FONTSET           "-adobe-helvetica-medium-r-*-*-10-*-*-*-p-*-*-*,-cronyx-helvetica-medium-r-normal-*-11-*-*-*-p-*-koi8-r,-*-*-medium-r-normal--10-*-*-*-*-*-ksc5601.1987-0,*"


/* Functions of this type are used by the marking functions to fetch color specifications.  Such
 * a function must return a color spec based on the property passed to it.
 */
typedef char * (* GetColorFunc) (ColorProp propnum, gpointer data);


/* Sets the user-configured colors and font for a month item.  It also tags the days as unmarked. */
void colorify_month_item (GnomeMonthItem *month, GetColorFunc func, gpointer func_data);

/* Takes a monthly calendar item and marks the days that have events scheduled for them in the
 * specified calendar.  It also highlights the current day.
 */
void mark_month_item (GnomeMonthItem *mitem, Calendar *cal);

/* Marks a day specified by index, not by day number */
void mark_month_item_index (GnomeMonthItem *mitem, int index, GetColorFunc func, gpointer func_data);

/* Unmarks all the days in the specified month item */
void unmark_month_item (GnomeMonthItem *mitem);

/* Prepares a monthly calendar item to prelight when the mouse goes over the days. */

void month_item_prepare_prelight (GnomeMonthItem *mitem, GetColorFunc func, gpointer func_data);

/* This is the default prelight function you can use for most puposes.  You can use NULL as the
 * func_data.
 */
char *default_color_func (ColorProp prop_num, gpointer data);


#endif
