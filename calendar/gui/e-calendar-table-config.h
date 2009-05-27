/*
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
 *		JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_CALENDAR_TABLE_CONFIG_H_
#define _E_CALENDAR_TABLE_CONFIG_H_

#include "e-calendar-table.h"

G_BEGIN_DECLS

#define E_CALENDAR_TABLE_CONFIG(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, e_calendar_table_config_get_type (), ECalendarTableConfig)
#define E_CALENDAR_TABLE_CONFIG_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, e_calendar_table_config_get_type (), ECalendarTableConfigClass)
#define E_IS_CALENDAR_TABLE_CONFIG(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, e_calendar_table_config_get_type ())

typedef struct _ECalendarTableConfig        ECalendarTableConfig;
typedef struct _ECalendarTableConfigClass   ECalendarTableConfigClass;
typedef struct _ECalendarTableConfigPrivate ECalendarTableConfigPrivate;

struct _ECalendarTableConfig {
	GObject parent;

	ECalendarTableConfigPrivate *priv;
};

struct _ECalendarTableConfigClass {
	GObjectClass parent_class;
};

GType          e_calendar_table_config_get_type (void);
ECalendarTableConfig *e_calendar_table_config_new (ECalendarTable *table);
ECalendarTable *e_calendar_table_config_get_table (ECalendarTableConfig *view_config);
void e_calendar_table_config_set_table (ECalendarTableConfig *view_config, ECalendarTable *table);

G_END_DECLS

#endif
