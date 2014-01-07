/*
 *
 * Evolution calendar - Utilities for tagging ECalendar widgets
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
 *		Damon Chaplin <damon@ximian.com>
 *      Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef TAG_CALENDAR_H
#define TAG_CALENDAR_H

#include <libecal/libecal.h>
#include <e-util/e-util.h>

void tag_calendar_by_client (ECalendar *ecal, ECalClient *client, GCancellable *cancellable);
void tag_calendar_by_comp (ECalendar *ecal, ECalComponent *comp,
			   ECalClient *client, icaltimezone *display_zone,
			   gboolean clear_first, gboolean comp_is_on_server,
			   gboolean can_recur_events_italic, GCancellable *cancellable);

#endif
