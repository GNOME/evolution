/*
 * caldav-browse-server.h
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef CALDAV_BROWSE_SERVER_H
#define CALDAV_BROWSE_SERVER_H

#include <glib.h>
#include <gtk/gtk.h>

/* Opens a window with a list of available calendars for a given server;
   Returns server URL of a calendar user chose, or NULL to let it be as is. */
gchar *caldav_browse_server (GtkWindow *parent, const gchar *server_url, const gchar *username, gboolean use_ssl, gint source_type);

#endif /* CALDAV_BROWSE_SERVER_H */
