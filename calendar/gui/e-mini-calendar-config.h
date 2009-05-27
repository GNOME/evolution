/*
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
 *		JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_MINI_CALENDAR_CONFIG_H_
#define _E_MINI_CALENDAR_CONFIG_H_

#include <misc/e-calendar.h>

G_BEGIN_DECLS

#define E_MINI_CALENDAR_CONFIG(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, e_mini_calendar_config_get_type (), EMiniCalendarConfig)
#define E_MINI_CALENDAR_CONFIG_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, e_mini_calendar_config_get_type (), EMiniCalendarConfigClass)
#define E_IS_MINI_CALENDAR_CONFIG(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, e_mini_calendar_config_get_type ())

typedef struct _EMiniCalendarConfig        EMiniCalendarConfig;
typedef struct _EMiniCalendarConfigClass   EMiniCalendarConfigClass;
typedef struct _EMiniCalendarConfigPrivate EMiniCalendarConfigPrivate;

struct _EMiniCalendarConfig {
	GObject parent;

	EMiniCalendarConfigPrivate *priv;
};

struct _EMiniCalendarConfigClass {
	GObjectClass parent_class;
};

GType          e_mini_calendar_config_get_type (void);
EMiniCalendarConfig *e_mini_calendar_config_new (ECalendar *mini_cal);
ECalendar *e_mini_calendar_config_get_calendar (EMiniCalendarConfig *mini_config);
void e_mini_calendar_config_set_calendar (EMiniCalendarConfig *mini_config, ECalendar *mini_cal);

G_END_DECLS

#endif
