/* Evolution calendar - Commands for the calendar GUI control
 *
 * Copyright (C) 1998 The Free Software Foundation
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Miguel de Icaza <miguel@ximian.com>
 *          Federico Mena-Quintero <federico@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include <bonobo/bonobo-control.h>
#include "gnome-cal.h"

/* This tells all the calendars to reload the config settings. */
void update_all_config_settings (void);

GnomeCalendar *new_calendar (void);

void calendar_control_activate (BonoboControl *control,
				GnomeCalendar *cal);
void calendar_control_deactivate (BonoboControl *control);

void calendar_goto_today (GnomeCalendar *gcal);

#endif /* CALENDAR_COMMANDS_H */
