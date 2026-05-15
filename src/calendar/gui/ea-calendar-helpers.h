/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Bolian Yin <bolian.yin@sun.com>
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
