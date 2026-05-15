/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Michael Zucchi <notzed@ximian.com>
 * SPDX-FileContributor: Federico Mena-Quintero <federico@ximian.com>
 */

#ifndef PRINT_H
#define PRINT_H

#include <e-util/e-util.h>

#include "calendar/gui/e-calendar-view.h"

typedef enum {
	E_PRINT_VIEW_DAY,
	E_PRINT_VIEW_WORKWEEK,
	E_PRINT_VIEW_WEEK,
	E_PRINT_VIEW_MONTH,
	E_PRINT_VIEW_LIST
} EPrintView;

void		print_calendar			(ECalendarView *cal_view,
						 ETable *tasks_table,
						 EPrintView print_view_type,
						 GtkPrintOperationAction action,
						 time_t start);
void		print_comp			(ECalComponent *comp,
						 ECalClient *cal_client,
						 ICalTimezone *zone,
						 gboolean use_24_hour_format,
						 GtkPrintOperationAction action);
void		print_table			(ETable *table,
						 const gchar *dialog_title,
						 const gchar *print_header,
						 GtkPrintOperationAction action);

#endif
