/* Evolution calendar - alarm notification dialog
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
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

#ifndef ALARM_NOTIFY_DIALOG_H
#define ALARM_NOTIFY_DIALOG_H

#include <time.h>
#include <glib.h>
#include <libecal/e-cal-component.h>



typedef enum {
	ALARM_NOTIFY_CLOSE,
	ALARM_NOTIFY_SNOOZE,
	ALARM_NOTIFY_EDIT
} AlarmNotifyResult;

typedef void (* AlarmNotifyFunc) (AlarmNotifyResult result, int snooze_mins, gpointer data);

void alarm_notify_dialog (time_t trigger, time_t occur_start, time_t occur_end,
			  ECalComponentVType vtype, const char *summary,
			  const char *description, const char *location,
			  AlarmNotifyFunc func, gpointer func_data);


#endif
