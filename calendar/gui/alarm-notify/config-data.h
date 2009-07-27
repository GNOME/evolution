/*
 *
 * Evolution calendar - Configuration values for the alarm notification daemon
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
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef CONFIG_DATA_H
#define CONFIG_DATA_H

#include <glib.h>
#include <libical/ical.h>
#include <libecal/e-cal.h>
#include <gconf/gconf-client.h>
#include <libedataserver/e-source-list.h>

GConfClient  *config_data_get_conf_client (void);

icaltimezone *config_data_get_timezone (void);
gboolean      config_data_get_24_hour_format (void);
gboolean      config_data_get_notify_with_tray (void);
void          config_data_set_last_notification_time (ECal *cal, time_t t);
time_t        config_data_get_last_notification_time (ECal *cal);
void          config_data_save_blessed_program (const gchar *program);
gboolean      config_data_is_blessed_program (const gchar *program);
ESourceList  *config_data_get_calendars (const gchar *);
void	      config_data_replace_string_list (const gchar *, const gchar *, const gchar *);

#endif
