/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Author : 
 *  JP Rosevear <jpr@ximian.com>
 *
 * Copyright 2003, Ximian, Inc.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef _E_MINI_CALENDAR_CONFIG_H_
#define _E_MINI_CALENDAR_CONFIG_H_

#include <misc/e-calendar.h>

G_BEGIN_DECLS

#define E_MINI_CALENDAR_CONFIG(obj)          GTK_CHECK_CAST (obj, e_mini_calendar_config_get_type (), EMiniCalendarConfig)
#define E_MINI_CALENDAR_CONFIG_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, e_mini_calendar_config_get_type (), EMiniCalendarConfigClass)
#define E_IS_MINI_CALENDAR_CONFIG(obj)       GTK_CHECK_TYPE (obj, e_mini_calendar_config_get_type ())
        
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
