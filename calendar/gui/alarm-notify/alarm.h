/* Evolution calendar - alarm notification support
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Authors: Miguel de Icaza <miguel@helixcode.com>
 *          Federico Mena-Quintero <federico@helixcode.com>
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

#ifndef ALARM_H
#define ALARM_H

#include <time.h>



typedef void (* AlarmFunction) (gpointer alarm_id, time_t trigger, gpointer data);
typedef void (* AlarmDestroyNotify) (gpointer data);

void     alarm_init   (void);
gpointer alarm_add    (time_t trigger, AlarmFunction alarm_fn, gpointer data,
		       AlarmDestroyNotify destroy_notify_fn);
void     alarm_remove (gpointer alarm);



#endif
