/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-url.c : utility functions to parse URLs */


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



/* XXX TODO:
 * recover the words between #'s or ?'s after the path
 * % escapes
 */

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include "camel-url.h"
#include "camel-exception.h"

/**
 * camel_url_new: create a CamelURL object from a string
 * @url_string: The string containing the URL to scan
 * 
 * This routine takes a string and parses it as a URL of the form:
 *
 *   protocol://user;AUTH=mech:password@host:port/path
 *
 * The protocol, followed by a ":" is required. If it is followed by
 * "//", there must be an "authority" containing at least a host,
 * which ends at the end of the string or at the next "/". If there
 * is an "@" in the authority, there must be a username before it,
 * and the host comes after it. The authmech, password, and port are
 * optional, and the punctuation that preceeds them is omitted if
 * they are. Everything after the authority (or everything after the
 * protocol if there was no authority) is the path. We consider the
 * "/" between the authority and the path to be part of the path,
 * although this is incorrect according to RFC 1738.
 *
 * The port, if present, must be numeric.
 * 
 * Return value: a CamelURL structure containing the URL items.
 **/
CamelURL *
camel_url_new (const char *url_string, CamelException *ex)
{
	CamelURL *url;
	char *semi, *colon, *at, *slash;

	/* Find protocol: initial substring until ":" */
	colon = strchr (url_string, ':');
	if (!colon) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_URL_INVALID,
				      "URL string `%s' contains no protocol",
				      url_string);
		return NULL;
	}

	url = g_new0 (CamelURL, 1);
	url->protocol = g_strndup (url_string, colon - url_string);

	if (strncmp (colon, "://", 3) != 0) {
		if (*(colon + 1))
			url->path = g_strdup (colon + 1);
		return url;
	}

	url_string = colon + 3;

	/* If there is an @ sign in the authority, look for user,
	 * authmech, and password before it.
	 */
	slash = strchr (url_string, '/');
	at = strchr (url_string, '@');
	if (at && (!slash || at < slash)) {
		colon = strchr (url_string, ':');
		if (colon && colon < at)
			url->passwd = g_strndup (colon + 1, at - colon - 1);
		else {
			url->passwd = NULL;
			colon = at;
		}

		semi = strchr(url_string, ';');
		if (semi && (semi < colon || (!colon && semi < at)) &&
		    !strncasecmp (semi, ";auth=", 6)) {
			url->authmech = g_strndup (semi + 6,
						     colon - semi - 6);
		} else {
			url->authmech = NULL;
			semi = colon;
		}

		url->user = g_strndup (url_string, semi - url_string);
		url_string = at + 1;
	} else
		url->user = url->passwd = url->authmech = NULL;

	/* Find host and port. */
	slash = strchr (url_string, '/');
	colon = strchr (url_string, ':');
	if (slash && colon > slash)
		colon = NULL;

	if (colon) {
		url->host = g_strndup (url_string, colon - url_string);
		url->port = strtoul (colon + 1, &colon, 10);
		if (*colon && colon != slash) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_URL_INVALID,
					      "Port number in URL `%s' is non-"
					      "numeric", url_string);
			camel_url_free (url);
			return NULL;
		}
	} else if (slash) {
		url->host = g_strndup (url_string, slash - url_string);
		url->port = 0;
	} else {
		url->host = g_strdup (url_string);
		url->port = 0;
	}

	if (!slash)
		slash = "/";
	url->path = g_strdup (slash);

	return url;
}

char *
camel_url_to_string (CamelURL *url, gboolean show_passwd)
{
	char port[20];

	if (url->port)
		g_snprintf (port, sizeof (port), "%d", url->port);
	else
		*port = '\0';

	return g_strdup_printf ("%s:%s%s%s%s%s%s%s%s%s%s%s",
				url->protocol,
				url->host ? "//" : "",
				url->user ? url->user : "",
				url->authmech ? ";auth=" : "",
				url->authmech ? url->authmech : "",
				url->passwd && show_passwd ? ":" : "",
				url->passwd && show_passwd ? url->passwd : "",
				url->user ? "@" : "",
				url->host ? url->host : "",
				url->port ? ":" : "",
				port,
				url->path ? url->path : "");
}

void
camel_url_free (CamelURL *url)
{
	g_assert (url);

	g_free (url->protocol);
	g_free (url->user);
	g_free (url->authmech);
	g_free (url->passwd);
	g_free (url->host);
	g_free (url->path);

	g_free (url);
}
