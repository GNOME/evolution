/* Evolution calendar - Utilities for tagging ECalendar widgets
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Authors: Damon Chaplin <damon@helixcode.com>
 *          Federico Mena-Quintero <federico@helixcode.com>
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

#ifndef TAG_CALENDAR_H
#define TAG_CALENDAR_H

#include <widgets/misc/e-calendar.h>
#include <cal-client/cal-client.h>

void tag_calendar_by_client (ECalendar *ecal, CalClient *client);
void tag_calendar_by_comp (ECalendar *ecal, CalComponent *comp);

#endif
