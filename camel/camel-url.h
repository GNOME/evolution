/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-url.h : utility functions to parse URLs */

/* 
 * Authors:
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *  Dan Winship <danw@ximian.com>
 *
 * Copyright 1999, 2000 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
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

/* if this changes, remember to change camel_url_copy */
typedef struct _CamelURL {
	char  *protocol;
	char  *user;
	char  *authmech;
	char  *passwd;
	char  *host;
	int    port;
	char  *path;
	GData *params;
	char  *query;
	char  *fragment;
} CamelURL;

#define CAMEL_URL_HIDE_PASSWORD	(1 << 0)
#define CAMEL_URL_HIDE_PARAMS	(1 << 1)
#define CAMEL_URL_HIDE_AUTH	(1 << 2)

#define CAMEL_URL_HIDE_ALL (CAMEL_URL_HIDE_PASSWORD | CAMEL_URL_HIDE_PARAMS | CAMEL_URL_HIDE_AUTH)

CamelURL *camel_url_new_with_base (CamelURL *base, const char *url_string);
CamelURL *camel_url_new (const char *url_string, CamelException *ex);
char *camel_url_to_string (CamelURL *url, guint32 flags);
void camel_url_free (CamelURL *url);

char *camel_url_encode (const char *part, const char *escape_extra);
void camel_url_decode (char *part);

/* for editing url's */
void camel_url_set_protocol (CamelURL *url, const char *protocol);
void camel_url_set_user (CamelURL *url, const char *user);
void camel_url_set_authmech (CamelURL *url, const char *authmech);
void camel_url_set_passwd (CamelURL *url, const char *passwd);
void camel_url_set_host (CamelURL *url, const char *host);
void camel_url_set_port (CamelURL *url, int port);
void camel_url_set_path (CamelURL *url, const char *path);
void camel_url_set_param (CamelURL *url, const char *name, const char *value);
void camel_url_set_query (CamelURL *url, const char *query);
void camel_url_set_fragment (CamelURL *url, const char *fragment);

const char *camel_url_get_param (CamelURL *url, const char *name);

/* for putting url's into hash tables */
guint camel_url_hash (const void *v);
int camel_url_equal(const void *v, const void *v2);
CamelURL *camel_url_copy(const CamelURL *in);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* URL_UTIL_H */
