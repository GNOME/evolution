/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-url.c : utility functions to parse URLs */


/* 
 * Authors:
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *  Dan Winship <danw@helixcode.com>
 *  Tiago Antào <tiagoantao@bigfoot.com>
 *  Jeffrey Stedfast <fejj@helixcode.com>
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

#include <config.h>

#include <ctype.h>
#include <stdio.h>
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
 * The protocol, followed by a ":" is required. If it is followed by * "//",
 * there must be an "authority" containing at least a host,
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
 * If nothing but the protocol (and the ":") is present, the "empty"
 * flag will be set on the returned URL.
 *
 * Return value: a CamelURL structure containing the URL items.
 **/
CamelURL *
camel_url_new (const char *url_string, CamelException *ex)
{
	CamelURL *url;
	char *semi, *colon, *at, *slash;
	char *p;

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
	g_strdown (url->protocol);

	/* Check protocol */
	p = url->protocol;
	while (*p) {
		if (!((*p >= 'a' && *p <= 'z') ||
		      (*p == '-') || (*p == '+') || (*p == '.'))) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_URL_INVALID,
					      "URL string `%s' contains an invalid protocol",
					      url_string);
			return NULL;
		}
		p++;
	}

	if (strncmp (colon, "://", 3) != 0) {
		if (*(colon + 1)) {
			url->path = g_strdup (colon + 1);
			camel_url_decode (url->path);
		} else
			url->empty = TRUE;
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
		if (colon && colon < at) {
			url->passwd = g_strndup (colon + 1, at - colon - 1);
			camel_url_decode (url->passwd);
		} else {
			url->passwd = NULL;
			colon = at;
		}

		semi = strchr(url_string, ';');
		if (semi && (semi < colon || (!colon && semi < at)) &&
		    !strncasecmp (semi, ";auth=", 6)) {
			url->authmech = g_strndup (semi + 6,
						     colon - semi - 6);
			camel_url_decode (url->authmech);
		} else {
			url->authmech = NULL;
			semi = colon;
		}

		url->user = g_strndup (url_string, semi - url_string);
		camel_url_decode (url->user);
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
		camel_url_decode (url->host);
		url->port = 0;
	} else {
		url->host = g_strdup (url_string);
		camel_url_decode (url->host);
		url->port = 0;
	}

	if (!slash)
		slash = "/";
	url->path = g_strdup (slash);
	camel_url_decode (url->path);

	return url;
}

char *
camel_url_to_string (CamelURL *url, gboolean show_passwd)
{
	char *return_result;
	char *user = NULL, *authmech = NULL, *passwd = NULL;
	char *host = NULL, *path = NULL;
	char port[20];

	if (url->user)
		user = camel_url_encode (url->user, TRUE, ":;@/");
	if (url->authmech)
		authmech = camel_url_encode (url->authmech, TRUE, ":@/");
	if (show_passwd && url->passwd)
		passwd = camel_url_encode (url->passwd, TRUE, "@/");
	if (url->host)
		host = camel_url_encode (url->host, TRUE, ":/");
	if (url->port)
		g_snprintf (port, sizeof (port), "%d", url->port);
	else
		*port = '\0';
	if (url->path)
		path = camel_url_encode (url->path, FALSE, NULL);

	return_result = g_strdup_printf ("%s:%s%s%s%s%s%s%s%s%s%s%s%s",
					 url->protocol,
					 host ? "//" : "",
					 user ? user : "",
					 authmech ? ";auth=" : "",
					 authmech ? authmech : "",
					 passwd ? ":" : "",
					 passwd ? passwd : "",
					 user ? "@" : "",
					 host ? host : "",
					 *port ? ":" : "",
					 port,
					 path && host && *path != '/' ? "/" : "",
					 path ? path : "");
	g_free (user);
	g_free (authmech);
	g_free (passwd);
	g_free (host);
	g_free (path);

	return return_result;
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


/**
 * camel_url_encode:
 * @part: a URL part
 * @escape_unsafe: whether or not to %-escape "unsafe" characters.
 * ("%#<>{}|\^~[]`)
 * @escape_extra: additional characters to escape.
 *
 * This %-encodes the given URL part and returns the escaped version
 * in allocated memory, which the caller must free when it is done.
 **/
char *
camel_url_encode (char *part, gboolean escape_unsafe, char *escape_extra)
{
	char *work, *p;

	/* worst case scenario = 3 times the initial */
	p = work = g_malloc (3 * strlen (part) + 1);

	while (*part) {
		if (((guchar) *part >= 127) || ((guchar) *part <= ' ') ||
		    (escape_unsafe && strchr ("\"%#<>{}|\\^~[]`", *part)) ||
		    (escape_extra && strchr (escape_extra, *part))) {
			sprintf (p, "%%%.02hX", (guchar) *part++);
			p += 3;
		} else
			*p++ = *part++;
	}
	*p = '\0';

	return work;
}

#define HEXVAL(c) (isdigit (c) ? (c) - '0' : tolower (c) - 'a' + 10)

/**
 * camel_url_decode:
 * @part: a URL part
 *
 * %-decodes the passed-in URL *in place*. The decoded version is
 * never longer than the encoded version, so there does not need to
 * be any additional space at the end of the string.
 */
void
camel_url_decode (char *part)
{
	guchar *s, *d;

	s = d = (guchar *)part;
	while (*s) {
		if (*s == '%') {
			if (isxdigit (s[1]) && isxdigit (s[2])) {
				*d++ = HEXVAL (s[1]) * 16 + HEXVAL (s[2]);
				s += 3;
			} else
				*d++ = *s++;
		} else
			*d++ = *s++;
	}
	*d = '\0';
}

static void
add_hash (guint *hash, char *s)
{
	if (s)
		*hash ^= g_str_hash(s);
}

guint camel_url_hash (const void *v)
{
	const CamelURL *u = v;
	guint hash = 0;

	add_hash (&hash, u->protocol);
	add_hash (&hash, u->user);
	add_hash (&hash, u->authmech);
	add_hash (&hash, u->host);
	add_hash (&hash, u->path);
	hash ^= u->port;
	
	return hash;
}

static int
check_equal (char *s1, char *s2)
{
	if (s1 == NULL) {
		if (s2 == NULL)
			return TRUE;
		else
			return FALSE;
	}
	
	if (s2 == NULL)
		return FALSE;

	return strcmp (s1, s2) == 0;
}

int camel_url_equal(const void *v, const void *v2)
{
	const CamelURL *u1 = v, *u2 = v2;
	
	return check_equal(u1->protocol, u2->protocol)
		&& check_equal(u1->user, u2->user)
		&& check_equal(u1->authmech, u2->authmech)
		&& check_equal(u1->host, u2->host)
		&& check_equal(u1->path, u2->path)
		&& u1->port == u2->port;
}
