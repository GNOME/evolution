/* Evolution calendar - alarm notification dialog
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: Federico Mena-Quintero <federico@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
#include <cal-util/cal-component.h>



typedef enum {
	ALARM_NOTIFY_CLOSE,
	ALARM_NOTIFY_SNOOZE,
	ALARM_NOTIFY_EDIT
} AlarmNotifyResult;

typedef void (* AlarmNotifyFunc) (AlarmNotifyResult result, int snooze_mins, gpointer data);

gboolean alarm_notify_dialog (time_t trigger, time_t occur, CalComponent *comp,
			      AlarmNotifyFunc func, gpointer func_data);



#endif
