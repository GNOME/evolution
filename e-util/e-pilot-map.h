/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution Conduits - Pilot Map routines
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Authors: JP Rosevear <jpr@helixcode.com>
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
};

gboolean e_pilot_map_pid_is_archived (EPilotMap *map, guint32 pid);
gboolean e_pilot_map_uid_is_archived (EPilotMap *map, const char *uid);

void e_pilot_map_insert (EPilotMap *map, guint32 pid, const char *uid, gboolean archived);
guint32 e_pilot_map_lookup_pid (EPilotMap *map, const char *uid);
const char * e_pilot_map_lookup_uid (EPilotMap *map, guint32 pid);

int e_pilot_map_read (const char *filename, EPilotMap **map);
int e_pilot_map_write (const char *filename, EPilotMap *map);

void e_pilot_map_destroy (EPilotMap *map);

#endif /* E_PILOT_MAP_H */
