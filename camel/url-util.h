/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* url-util.h : utility functions to parse URLs */

/* 
 * Author : 
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999, 2000 HelixCode (http://www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


#ifndef URL_UTIL_H
#define URL_UTIL_H 1

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef struct {
	gchar *protocol;
	gchar *user;
	gchar *authmech;
	gchar *passwd;
	gchar *host;
	gchar *port;
	gchar *path;

} Gurl;

/* the cache system has been disabled because it would 
   need the user to use accessors instead of modifying the 
   structure field. As the speed is not so important here, 
   I chose not to use it */

Gurl *g_url_new (const gchar *url_string);
gchar *g_url_to_string (const Gurl *url, gboolean show_password);
void g_url_free (Gurl *url);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* URL_UTIL_H */
