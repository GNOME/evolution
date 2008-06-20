/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* vim:expandtab:shiftwidth=8:tabstop=8:
 */
/* Evolution Accessibility: ea-calendar-helpers.h
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Bolian Yin <bolian.yin@sun.com> Sun Microsystem Inc., 2003
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
