/* Evolution calendar - Configuration values for the alarm notification daemon
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef CONFIG_DATA_H
#define CONFIG_DATA_H

#include <glib.h>
#include <libical/ical.h>
#include <gconf/gconf-client.h>

GConfClient  *config_data_get_conf_client (void);

icaltimezone *config_data_get_timezone (void);
gboolean      config_data_get_24_hour_format (void);
gboolean      config_data_get_notify_with_tray (void);
void          config_data_set_last_notification_time (time_t t);
time_t        config_data_get_last_notification_time (void);
void          config_data_save_blessed_program (const char *program);
gboolean      config_data_is_blessed_program (const char *program);

#endif
