/* Evolution calendar - Utilities for tagging ECalendar widgets
 *
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Damon Chaplin <damon@ximian.com>
 *          Federico Mena-Quintero <federico@ximian.com>
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

#ifndef TAG_CALENDAR_H
#define TAG_CALENDAR_H

#include <misc/e-calendar.h>
#include <libecal/e-cal.h>

void tag_calendar_by_client (ECalendar *ecal, ECal *client);
void tag_calendar_by_comp (ECalendar *ecal, ECalComponent *comp,
			   ECal *client, icaltimezone *display_zone,
			   gboolean clear_first, gboolean comp_is_on_server);

#endif
