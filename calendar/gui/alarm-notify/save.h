/* Evolution calendar - Functions to save alarm notification times
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

#ifndef SAVE_H
#define SAVE_H

#include <time.h>

void save_notification_time (time_t t);
time_t get_saved_notification_time (void);

void save_calendars_to_load (GPtrArray *uris);
GPtrArray *get_calendars_to_load (void);

void save_blessed_program (const char *program);
gboolean is_blessed_program (const char *program);

#endif
