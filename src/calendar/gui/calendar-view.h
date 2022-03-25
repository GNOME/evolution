/*
 * calendar-view.h
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef CALENDAR_VIEW_H
#define CALENDAR_VIEW_H

#include <e-util/e-util.h>

/* Standard GObject macros */
#define GAL_TYPE_VIEW_CALENDAR_DAY \
	(gal_view_calendar_day_get_type ())
#define GAL_TYPE_VIEW_CALENDAR_WORK_WEEK \
	(gal_view_calendar_work_week_get_type ())
#define GAL_TYPE_VIEW_CALENDAR_WEEK \
	(gal_view_calendar_week_get_type ())
#define GAL_TYPE_VIEW_CALENDAR_MONTH \
	(gal_view_calendar_month_get_type ())
#define GAL_TYPE_VIEW_CALENDAR_YEAR \
	(gal_view_calendar_year_get_type ())

G_BEGIN_DECLS

typedef struct _GalView GalViewCalendarDay;
typedef struct _GalViewClass GalViewCalendarDayClass;

typedef struct _GalView GalViewCalendarWorkWeek;
typedef struct _GalViewClass GalViewCalendarWorkWeekClass;

typedef struct _GalView GalViewCalendarWeek;
typedef struct _GalViewClass GalViewCalendarWeekClass;

typedef struct _GalView GalViewCalendarMonth;
typedef struct _GalViewClass GalViewCalendarMonthClass;

typedef struct _GalView GalViewCalendarYear;
typedef struct _GalViewClass GalViewCalendarYearClass;

GType		gal_view_calendar_day_get_type		(void) G_GNUC_CONST;
GType		gal_view_calendar_work_week_get_type	(void) G_GNUC_CONST;
GType		gal_view_calendar_week_get_type		(void) G_GNUC_CONST;
GType		gal_view_calendar_month_get_type	(void) G_GNUC_CONST;
GType		gal_view_calendar_year_get_type		(void) G_GNUC_CONST;

G_END_DECLS

#endif /* CALENDAR_VIEW_H */
