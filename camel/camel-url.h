/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-url.h : utility functions to parse URLs */

/* 
 * Authors:
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *  Dan Winship <danw@helixcode.com>
 *
 * Copyright 1999, 2000 Helix Code, Inc. (http://www.helixcode.com)
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


#ifndef CAMEL_URL_H
#define CAMEL_URL_H 1

#include <glib.h>
#include <camel/camel-types.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef struct {
	char *protocol;
	char *user;
	char *authmech;
	char *passwd;
	char *host;
	int   port;
	char *path;

	/* This is set if the URL contained only a protocol. */
	gboolean empty;
} CamelURL;

CamelURL *camel_url_new (const char *url_string, CamelException *ex);
char *camel_url_to_string (CamelURL *url, gboolean show_password);
void camel_url_free (CamelURL *url);

char *camel_url_encode (char *part, gboolean escape_unsafe,
			char *escape_extra);
void camel_url_decode (char *part);

/* for putting url's into hash tables */
guint camel_url_hash (const void *v);
int camel_url_equal(const void *v, const void *v2);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* URL_UTIL_H */
