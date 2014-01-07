/*
 *
 * Evolution calendar - Miscellaneous utility functions
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

#ifndef MISC_H
#define MISC_H

#include <glib.h>
#include <time.h>

gboolean string_is_empty (const gchar *value);
gint get_position_in_array (GPtrArray *objects, gpointer item);
gchar * calculate_time (time_t start, time_t end);
#endif
