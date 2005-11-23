/* Evolution calendar - Miscellaneous utility functions
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef MISC_H
#define MISC_H

#include <glib.h>
#include <time.h>

gboolean string_is_empty (const char *value);
char    *get_uri_without_password (const char *uri);
gint get_position_in_array (GPtrArray *objects, gpointer item);
char * calculate_time (time_t start, time_t end);
#endif
