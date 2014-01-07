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
 *		Bolian Yin <bolian.yin@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/* Evolution Accessibility
 */

#ifndef _EA_CALENDAR_HELPERS_H__
#define _EA_CALENDAR_HELPERS_H__

#include "ea-cal-view.h"

AtkObject *
ea_calendar_helpers_get_accessible_for (GnomeCanvasItem *canvas_item);

ECalendarView *
ea_calendar_helpers_get_cal_view_from (GnomeCanvasItem *canvas_item);

ECalendarViewEvent *
ea_calendar_helpers_get_cal_view_event_from (GnomeCanvasItem *canvas_item);

#endif /* _EA_CALENDAR_HELPERS_H__ */
