/* Evolution calendar - Commands for the calendar GUI control
 *
 * Copyright (C) 1998 The Free Software Foundation
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Miguel de Icaza <miguel@ximian.com>
 *          Federico Mena-Quintero <federico@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
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

#endif /* CALENDAR_COMMANDS_H */
