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

#ifndef _E_CALENDAR_TABLE_CONFIG_H_
#define _E_CALENDAR_TABLE_CONFIG_H_

#include "e-calendar-table.h"

G_BEGIN_DECLS

#define E_CALENDAR_TABLE_CONFIG(obj)          GTK_CHECK_CAST (obj, e_calendar_table_config_get_type (), ECalendarTableConfig)
#define E_CALENDAR_TABLE_CONFIG_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, e_calendar_table_config_get_type (), ECalendarTableConfigClass)
#define E_IS_CALENDAR_TABLE_CONFIG(obj)       GTK_CHECK_TYPE (obj, e_calendar_table_config_get_type ())
        
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
