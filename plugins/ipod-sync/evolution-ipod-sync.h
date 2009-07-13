/*
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2004 Justin Wake <jwake@iinet.net.au>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <libhal.h>
#include <signal.h>

gboolean check_hal (void);
gboolean ipod_check_status (gboolean silent);
gboolean try_umount (gchar *device);

gchar *find_ipod_mount_point (LibHalContext *ctx);
gchar *ipod_get_mount (void);

