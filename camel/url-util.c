/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* url-util.c : utility functions to parse URLs */


/* 
 * Authors : 
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



/* 
 * Here we deal with URLs following the general scheme:
 *   protocol://user;AUTH=mech:password@host:port/name
 * where name is a path-like string (ie dir1/dir2/....) See RFC 1738
 * for the complete description of Uniform Resource Locators. The
 * ";AUTH=mech" addition comes from RFC 2384, "POP URL Scheme".
 */

/* XXX TODO:
 * recover the words between #'s or ?'s after the path
 * % escapes
 */

#include <string.h>
#include <config.h>
#include "url-util.h"

/**
 * g_url_new: create a Gurl object from a string
 *
 * @url_string: The string containing the URL to scan
 * 
 * This routine takes a gchar and parses it as a
 * URL of the form:
 *   protocol://user;AUTH=mech:password@host:port/path
 * There is no test on the values. For example,
 * "port" can be a string, not only a number!
 * The Gurl structure fields are filled with
 * the scan results. When a member of the 
 * general URL can not be found, the corresponding
 * Gurl member is NULL.
 * Fields filled in the Gurl structure are allocated
 * and url_string is not modified. 
 * 
 * Return value: a Gurl structure containing the URL items.
 **/
Gurl *g_url_new (const gchar* url_string)
{
	Gurl *g_url;
	char *semi, *colon, *at, *slash;

	g_url = g_new (Gurl,1);

	/* Find protocol: initial substring until "://" */
	colon = strchr (url_string, ':');
	if (colon && !strncmp (colon, "://", 3)) {
		g_url->protocol = g_strndup (url_string, colon - url_string);
		url_string = colon + 3;
	} else
		g_url->protocol = NULL;

	/* If there is an @ sign, look for user, authmech, and
	 * password before it.
	 */
	at = strchr (url_string, '@');
	if (at) {
		colon = strchr (url_string, ':');
		if (colon && colon < at)
			g_url->passwd = g_strndup (colon + 1, at - colon - 1);
		else {
			g_url->passwd = NULL;
			colon = at;
		}

		semi = strchr(url_string, ';');
		if (semi && semi < colon && !strncasecmp (semi, ";auth=", 6))
			g_url->authmech = g_strndup (semi + 6, colon - semi - 6);
		else {
			g_url->authmech = NULL;
			semi = colon;
		}

		g_url->user = g_strndup (url_string, semi - url_string);
		url_string = at + 1;
	} else
		g_url->user = g_url->passwd = g_url->authmech = NULL;

	/* Find host (required) and port. */
	slash = strchr (url_string, '/');
	colon = strchr (url_string, ':');
	if (slash && colon > slash)
		colon = 0;

	if (colon) {
		g_url->host = g_strndup (url_string, colon - url_string);
		if (slash)
			g_url->port = g_strndup (colon + 1, slash - colon - 1);
		else
			g_url->port = g_strdup (colon + 1);
	} else if (slash) {
		g_url->host = g_strndup (url_string, slash - url_string);
		g_url->port = NULL;
	} else {
		g_url->host = g_strdup (url_string);
		g_url->port = NULL;
	}

	/* setup a fallback, if relative, then empty string, else
	   it will be from root */
	if (slash == NULL) {
		slash = "/";
	}
	if (slash && *slash && g_url->protocol == NULL)
		slash++;

	g_url->path = g_strdup (slash);

	return g_url;
}

gchar *
g_url_to_string (const Gurl *url, gboolean show_passwd)
{
	return g_strdup_printf("%s%s%s%s%s%s%s%s%s%s%s%s",
			       url->protocol ? url->protocol : "",
			       url->protocol ? "://" : "",
			       url->user ? url->user : "",
			       url->authmech ? ";auth=" : "",
			       url->authmech ? url->authmech : "",
			       url->passwd && show_passwd ? ":" : "",
			       url->passwd && show_passwd ? url->passwd : "",
			       url->user ? "@" : "",
			       url->host,
			       url->port ? ":" : "",
			       url->port ? url->port : "",
			       url->path ? url->path : "");
}

void
g_url_free (Gurl *url)
{
	g_assert (url);

	g_free (url->protocol);
	g_free (url->user);
	g_free (url->authmech);
	g_free (url->passwd);
	g_free (url->host);
	g_free (url->port);
	g_free (url->path);

	g_free (url);
}
