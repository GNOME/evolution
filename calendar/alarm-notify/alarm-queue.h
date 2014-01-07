/*
 *
 * Evolution calendar - Alarm queueing engine
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

#ifndef ALARM_QUEUE_H
#define ALARM_QUEUE_H

#include <libecal/libecal.h>

void alarm_queue_init (gpointer);
void alarm_queue_done (void);

void alarm_queue_add_client (ECalClient *cal_client);
void alarm_queue_remove_client (ECalClient *cal_client, gboolean immediately);

#endif
