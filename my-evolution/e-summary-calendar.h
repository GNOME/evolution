/*
 * e-summary-calendar.h
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Iain Holmes  <iain@ximian.com>
 */

#ifndef __E_SUMMARY_CALENDAR_H__
#define __E_SUMMARY_CALENDAR_H__

#include "e-summary-type.h"

typedef enum _ESummaryCalendarDays {
	E_SUMMARY_CALENDAR_ONE_DAY,
	E_SUMMARY_CALENDAR_FIVE_DAYS,
	E_SUMMARY_CALENDAR_ONE_WEEK,
	E_SUMMARY_CALENDAR_ONE_MONTH
} ESummaryCalendarDays;

typedef enum _ESummaryCalendarNumTasks {
	E_SUMMARY_CALENDAR_ALL_TASKS,
	E_SUMMARY_CALENDAR_TODAYS_TASKS
} ESummaryCalendarNumTasks;

typedef struct _ESummaryCalendar ESummaryCalendar;

const char *e_summary_calendar_get_html (ESummary *summary);
void e_summary_calendar_init (ESummary *summary);
void e_summary_calendar_reconfigure (ESummary *summary);
void e_summary_calendar_free (ESummary *summary);
#endif
