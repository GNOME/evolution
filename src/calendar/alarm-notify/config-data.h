/*
 *
 * Evolution calendar - Configuration values for the alarm notification daemon
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
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef CONFIG_DATA_H
#define CONFIG_DATA_H

#include <libical/ical.h>
#include <libecal/libecal.h>

void		config_data_cleanup		(void);

icaltimezone *	config_data_get_timezone	(void);
gboolean	config_data_get_24_hour_format	(void);
gboolean	config_data_get_notify_with_tray
						(void);
gboolean	config_data_get_task_reminder_for_completed
						(void);
gint		config_data_get_default_snooze_minutes
						(void);
gboolean	config_data_get_allow_past_reminders
						(void);
void		config_data_set_last_notification_time
						(ECalClient *cal,
						 time_t t);
time_t		config_data_get_last_notification_time
						(ECalClient *cal);
void		config_data_save_blessed_program
						(const gchar *program);
gboolean	config_data_is_blessed_program	(const gchar *program);
gboolean	config_data_get_notify_window_on_top
						(void);

void		config_data_init_debugging	(void);
gboolean	config_data_start_debugging	(void);
void		config_data_stop_debugging	(void);

#define debug(x) G_STMT_START { \
	if (config_data_start_debugging ()) { \
		g_print ("%s (%s): ", G_STRFUNC, G_STRLOC); \
		g_print  x; \
		g_print ("\n"); \
 \
		config_data_stop_debugging (); \
	} \
	} G_STMT_END

#endif
