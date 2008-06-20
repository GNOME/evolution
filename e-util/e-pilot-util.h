/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution Conduits - Pilot Map routines
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * Authors: JP Rosevear <jpr@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <libedataserver/e-source-list.h>
#include <libedataserver/e-source.h>

#ifndef E_PILOT_UTIL_H
#define E_PILOT_UTIL_H

char *e_pilot_utf8_to_pchar (const char *string);
char *e_pilot_utf8_from_pchar (const char *string);

ESource *e_pilot_get_sync_source (ESourceList *source_list);
void e_pilot_set_sync_source (ESourceList *source_list, ESource *source);


#endif /* E_PILOT_UTIL_H */
