/*
 *
 * Evolution calendar - Utilities for tagging ECalendar widgets
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
 *		Damon Chaplin <damon@ximian.com>
 *      Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
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
