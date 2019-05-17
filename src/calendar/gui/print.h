/*
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
 *
 * Authors:
 *		Michael Zucchi <notzed@ximian.com>
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
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
