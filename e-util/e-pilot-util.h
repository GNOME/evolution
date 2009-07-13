/*
 *
 *Evolution Conduits - Pilot Map routines
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

#include <libedataserver/e-source-list.h>
#include <libedataserver/e-source.h>

#ifndef E_PILOT_UTIL_H
#define E_PILOT_UTIL_H

gchar *e_pilot_utf8_to_pchar (const gchar *string);
gchar *e_pilot_utf8_from_pchar (const gchar *string);

ESource *e_pilot_get_sync_source (ESourceList *source_list);
void e_pilot_set_sync_source (ESourceList *source_list, ESource *source);

#endif /* E_PILOT_UTIL_H */
