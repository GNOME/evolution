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

typedef struct _ESummaryCalendar ESummaryCalendar;

const char *e_summary_calendar_get_html (ESummary *summary);
void e_summary_calendar_init (ESummary *summary);
#endif
