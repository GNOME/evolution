/*
 * Evolution calendar - Commands for the calendar GUI control
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
 *		Miguel de Icaza <miguel@ximian.com>
 *      Federico Mena-Quintero <federico@ximian.com>
 *      Seth Alves <alves@hungry.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef CALENDAR_COMMANDS_H
#define CALENDAR_COMMANDS_H

#include "gnome-cal.h"

#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-ui-component.h>

void calendar_control_activate (BonoboControl *control, GnomeCalendar *gcal);
void calendar_control_deactivate (BonoboControl *control, GnomeCalendar *gcal);

void calendar_control_sensitize_calendar_commands (BonoboControl *control, GnomeCalendar *gcal, gboolean enable);

void calendar_goto_today (GnomeCalendar *gcal);

void calendar_command_print (GnomeCalendar *gcal, GtkPrintOperationAction action);

#endif /* CALENDAR_COMMANDS_H */
