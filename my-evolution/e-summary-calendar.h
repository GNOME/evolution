/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary-calendar.h
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Iain Holmes  <iain@ximian.com>
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
