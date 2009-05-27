/*
 * Evolution Conduits - Pilot Map routines
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
 *		JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_PILOT_MAP_H
#define E_PILOT_MAP_H

#include <glib.h>
#include <time.h>

typedef struct _EPilotMap EPilotMap;

struct _EPilotMap
{
	GHashTable *pid_map;
	GHashTable *uid_map;

	time_t since;

	gboolean write_touched_only;
};

gboolean e_pilot_map_pid_is_archived (EPilotMap *map, guint32 pid);
gboolean e_pilot_map_uid_is_archived (EPilotMap *map, const gchar *uid);

void e_pilot_map_insert (EPilotMap *map, guint32 pid, const gchar *uid, gboolean archived);
void e_pilot_map_remove_by_pid (EPilotMap *map, guint32 pid);
void e_pilot_map_remove_by_uid (EPilotMap *map, const gchar *uid);

guint32 e_pilot_map_lookup_pid (EPilotMap *map, const gchar *uid, gboolean touch);
const gchar * e_pilot_map_lookup_uid (EPilotMap *map, guint32 pid, gboolean touch);

gint e_pilot_map_read (const gchar *filename, EPilotMap **map);
gint e_pilot_map_write (const gchar *filename, EPilotMap *map);

void e_pilot_map_clear (EPilotMap *map);

void e_pilot_map_destroy (EPilotMap *map);

#endif /* E_PILOT_MAP_H */
