/*
 * Evolution calendar - Low-level alarm timer mechanism
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
 *		Miguel de Icaza <miguel@ximian.com>
 *      Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef ALARM_H
#define ALARM_H

#include <time.h>
#include <glib.h>

typedef void (* AlarmFunction) (gpointer alarm_id, time_t trigger, gpointer data);
typedef void (* AlarmDestroyNotify) (gpointer alarm_id, gpointer data);

void alarm_done (void);

gpointer alarm_add (time_t trigger, AlarmFunction alarm_fn, gpointer data,
		    AlarmDestroyNotify destroy_notify_fn);
void alarm_remove (gpointer alarm);

void alarm_reschedule_timeout (void);

#endif
