/*
 *
 * Evolution calendar - utility functions
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

#ifndef UTIL_H
#define UTIL_H

#include <libecal/libecal.h>

#define DATETIME_CHECK_DTSTART TRUE
#define DATETIME_CHECK_DTEND FALSE

gboolean datetime_is_date_only (ECalComponent *comp, gboolean datetime_check);
gchar *timet_to_str_with_zone (time_t t, icaltimezone *zone, gboolean date_only);
gchar *calculate_time (time_t start, time_t end);
#endif
