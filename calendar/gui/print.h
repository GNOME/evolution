/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
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

#include <table/e-table.h>
#include "calendar/gui/gnome-cal.h"

typedef enum {
	PRINT_VIEW_DAY,
	PRINT_VIEW_WEEK,
	PRINT_VIEW_MONTH,
	PRINT_VIEW_YEAR,
	PRINT_VIEW_LIST
} PrintView;

void		print_calendar                (GnomeCalendar *gcal,
                                               GtkPrintOperationAction action,
                                               time_t start);
void		print_comp                    (ECalComponent *comp,
                                               ECal *client,
                                               GtkPrintOperationAction action);
void		print_table                   (ETable *table,
                                               const gchar *dialog_title,
                                               const gchar *print_header,
                                               GtkPrintOperationAction action);

#endif
