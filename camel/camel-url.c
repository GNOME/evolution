/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-url.c : utility functions to parse URLs */

/* 
 * Authors:
 *  Dan Winship <danw@ximian.com>
 *  Tiago Antào <tiagoantao@bigfoot.com>
 *  Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright 1999-2001 Ximian, Inc. (http://www.ximian.com)
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
#include "camel-mime-utils.h"
#include "camel-object.h"

static void copy_param (GQuark key_id, gpointer data, gpointer user_data);
static void output_param (GQuark key_id, gpointer data, gpointer user_data);

/**
 * camel_url_new_with_base:
 * @base: a base URL
 * @url_string: the URL
 *
 * Parses @url_string relative to @base.
 *
 * Return value: a parsed CamelURL.
 **/
CamelURL *
camel_url_new_with_base (CamelURL *base, const char *url_string)
{
	CamelURL *url;
	const char *end, *hash, *colon, *semi, *at, *slash, *question;
	const char *p;

	url = g_new0 (CamelURL, 1);

	/* See RFC1808 for details. IF YOU CHANGE ANYTHING IN THIS
	 * FUNCTION, RUN tests/misc/url AFTERWARDS.
	 */

	/* Find fragment. */
	end = hash = strchr (url_string, '#');
	if (hash && hash[1]) {
		url->fragment = g_strdup (hash + 1);
		camel_url_decode (url->fragment);
	} else
		end = url_string + strlen (url_string);

	/* Find protocol: initial [a-z+.-]* substring until ":" */
	p = url_string;
	while (p < end && (isalnum ((unsigned char)*p) ||
			   *p == '.' || *p == '+' || *p == '-'))
		p++;

	if (p > url_string && *p == ':') {
		url->protocol = g_strndup (url_string, p - url_string);
		g_strdown (url->protocol);
		url_string = p + 1;
	}

	if (!*url_string && !base)
		return url;

	/* Check for authority */
	if (strncmp (url_string, "//", 2) == 0) {
		url_string += 2;

		slash = url_string + strcspn (url_string, "/#");
		at = strchr (url_string, '@');
		if (at && at < slash) {
			colon = strchr (url_string, ':');
			if (colon && colon < at) {
				url->passwd = g_strndup (colon + 1,
							 at - colon - 1);
				camel_url_decode (url->passwd);
			} else {
				url->passwd = NULL;
				colon = at;
			}

			semi = strchr(url_string, ';');
			if (semi && semi < colon &&
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
		colon = strchr (url_string, ':');
		if (colon && colon < slash) {
			url->host = g_strndup (url_string, colon - url_string);
			url->port = strtoul (colon + 1, NULL, 10);
		} else {
			url->host = g_strndup (url_string, slash - url_string);
			camel_url_decode (url->host);
			url->port = 0;
		}

		url_string = slash;
	}

	/* Find query */
	question = memchr (url_string, '?', end - url_string);
	if (question) {
		if (question[1]) {
			url->query = g_strndup (question + 1,
						end - (question + 1));
			camel_url_decode (url->query);
		}
		end = question;
	}

	/* Find parameters */
	semi = memchr (url_string, ';', end - url_string);
	if (semi) {
		if (semi[1]) {
			const char *cur, *p, *eq;
			char *name, *value;

			for (cur = semi + 1; cur < end; cur = p + 1) {
				p = memchr (cur, ';', end - cur);
				if (!p)
					p = end;
				eq = memchr (cur, '=', p - cur);
				if (eq) {
					name = g_strndup (cur, eq - cur);
					value = g_strndup (eq + 1, end - (eq + 1));
					camel_url_decode (value);
				} else {
					name = g_strndup (cur, end - cur);
					value = g_strdup ("");
				}
				camel_url_decode (name);
				g_datalist_set_data_full (&url->params, name,
							  value, g_free);
			}
		}
		end = semi;
	}

	if (end != url_string) {
		url->path = g_strndup (url_string, end - url_string);
		camel_url_decode (url->path);
	}

	/* Apply base URL. Again, this is spelled out in RFC 1808. */
	if (base && !url->protocol && url->host)
		url->protocol = g_strdup (base->protocol);
	else if (base && !url->protocol) {
		if (!url->user && !url->authmech && !url->passwd &&
		    !url->host && !url->port && !url->path &&
		    !url->params && !url->query && !url->fragment)
			url->fragment = g_strdup (base->fragment);

		url->protocol = g_strdup (base->protocol);
		url->user = g_strdup (base->user);
		url->authmech = g_strdup (base->authmech);
		url->passwd = g_strdup (base->passwd);
		url->host = g_strdup (base->host);
		url->port = base->port;

		if (!url->path) {
			url->path = g_strdup (base->path);
			if (!url->params) {
				g_datalist_foreach (&base->params, copy_param,
						    &url->params);
				if (!url->query)
					url->query = g_strdup (base->query);
			}
		} else if (*url->path != '/') {
			char *newpath, *last, *p, *q;

			last = strrchr (base->path, '/');
			if (last) {
				newpath = g_strdup_printf ("%.*s/%s",
							   last - base->path,
							   base->path,
							   url->path);
			} else
				newpath = g_strdup_printf ("/%s", url->path);

			/* Remove "./" where "." is a complete segment. */
			for (p = newpath + 1; *p; ) {
				if (*(p - 1) == '/' &&
				    *p == '.' && *(p + 1) == '/')
					memmove (p, p + 2, strlen (p + 2) + 1);
				else
					p++;
			}
			/* Remove "." at end. */
			if (p > newpath + 2 &&
			    *(p - 1) == '.' && *(p - 2) == '/')
				*(p - 1) = '\0';
			/* Remove "<segment>/../" where <segment> != ".." */
			for (p = newpath + 1; *p; ) {
				if (!strncmp (p, "../", 3)) {
					p += 3;
					continue;
				}
				q = strchr (p + 1, '/');
				if (!q)
					break;
				if (strncmp (q, "/../", 4) != 0) {
					p = q + 1;
					continue;
				}
				memmove (p, q + 4, strlen (q + 4) + 1);
				p = newpath + 1;
			}
			/* Remove "<segment>/.." at end */
			q = strrchr (newpath, '/');
			if (q && !strcmp (q, "/..")) {
				p = q - 1;
				while (p > newpath && *p != '/')
					p--;
				if (strncmp (p, "/../", 4) != 0)
					*(p + 1) = 0;
			}
			g_free (url->path);
			url->path = newpath;
		}
	}

	return url;
}

static void
copy_param (GQuark key_id, gpointer data, gpointer user_data)
{
	GData **copy = user_data;

	g_datalist_id_set_data_full (copy, key_id, g_strdup (data), g_free);
}

/**
 * camel_url_new:
 * @url_string: a URL
 *
 * Parses an absolute URL.
 *
 * Return value: a CamelURL, or %NULL.
 **/
CamelURL *
camel_url_new (const char *url_string)
{
	CamelURL *url = camel_url_new_with_base (NULL, url_string);

	if (!url->protocol) {
		camel_url_free (url);
		return NULL;
	}
	return url;
}

/**
 * camel_url_to_string:
 * @url: a CamelURL
 * @show_password: whether or not to include the password in the output
 *
 * Return value: a string representing @url, which the caller must free.
 **/
char *
camel_url_to_string (CamelURL *url, gboolean show_passwd)
{
	GString *str;
	char *enc, *return_result;

	/* IF YOU CHANGE ANYTHING IN THIS FUNCTION, RUN
	 * tests/misc/url AFTERWARD.
	 */

	str = g_string_sized_new (20);

	if (url->protocol)
		g_string_sprintfa (str, "%s:", url->protocol);
	if (url->host) {
		g_string_append (str, "//");
		if (url->user) {
			enc = camel_url_encode (url->user, TRUE, ":;@/");
			g_string_append (str, enc);
			g_free (enc);
		}
		if (url->authmech && *url->authmech) {
			enc = camel_url_encode (url->authmech, TRUE, ":@/");
			g_string_sprintfa (str, ";auth=%s", enc);
			g_free (enc);
		}
		if (show_passwd && url->passwd) {
			enc = camel_url_encode (url->passwd, TRUE, "@/");
			g_string_sprintfa (str, ":%s", enc);
			g_free (enc);
		}
		if (url->host) {
			enc = camel_url_encode (url->host, TRUE, ":/");
			g_string_sprintfa (str, "%s%s", url->user ? "@" : "", enc);
		}
		if (url->port)
			g_string_sprintfa (str, ":%d", url->port);
		if (!url->path && (url->params || url->query || url->fragment))
			g_string_append_c (str, '/');
	}

	if (url->path) {
		enc = camel_url_encode (url->path, FALSE, ";?#");
		g_string_sprintfa (str, "%s", enc);
		g_free (enc);
	}
	if (url->params)
		g_datalist_foreach (&url->params, output_param, str);
	if (url->query) {
		enc = camel_url_encode (url->query, FALSE, "#");
		g_string_sprintfa (str, "?%s", enc);
		g_free (enc);
	}
	if (url->fragment) {
		enc = camel_url_encode (url->fragment, FALSE, NULL);
		g_string_sprintfa (str, "#%s", enc);
		g_free (enc);
	}

	return_result = str->str;
	g_string_free (str, FALSE);
	return return_result;
}

static void
output_param (GQuark key_id, gpointer data, gpointer user_data)
{
	GString *str = user_data;
	char *enc;

	enc = camel_url_encode (g_quark_to_string (key_id), FALSE, "?#");
	g_string_sprintfa (str, ";%s", enc);
	g_free (enc);
}

/**
 * camel_url_free:
 * @url: a CamelURL
 *
 * Frees @url
 **/
void
camel_url_free (CamelURL *url)
{
	g_return_if_fail (url);

	g_free (url->protocol);
	g_free (url->user);
	g_free (url->authmech);
	g_free (url->passwd);
	g_free (url->host);
	g_free (url->path);
	g_datalist_clear (&url->params);
	g_free (url->query);
	g_free (url->fragment);

	g_free (url);
}


#define DEFINE_CAMEL_URL_SET(part)			\
void							\
camel_url_set_##part (CamelURL *url, const char *part)	\
{							\
	g_free (url->part);				\
	url->part = g_strdup (part);			\
}

DEFINE_CAMEL_URL_SET (protocol)
DEFINE_CAMEL_URL_SET (user)
DEFINE_CAMEL_URL_SET (authmech)
DEFINE_CAMEL_URL_SET (passwd)
DEFINE_CAMEL_URL_SET (host)
DEFINE_CAMEL_URL_SET (path)
DEFINE_CAMEL_URL_SET (query)
DEFINE_CAMEL_URL_SET (fragment)

void
camel_url_set_port (CamelURL *url, int port)
{
	url->port = port;
}

void
camel_url_set_param (CamelURL *url, const char *name, const char *value)
{
	g_datalist_set_data (&url->params, name, value ? g_strdup (value) : NULL);
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

guint
camel_url_hash (const void *v)
{
	const CamelURL *u = v;
	guint hash = 0;

#define ADD_HASH(s) if (s) hash ^= g_str_hash (s);

	ADD_HASH (u->protocol);
	ADD_HASH (u->user);
	ADD_HASH (u->authmech);
	ADD_HASH (u->host);
	ADD_HASH (u->path);
	ADD_HASH (u->query);
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

int
camel_url_equal(const void *v, const void *v2)
{
	const CamelURL *u1 = v, *u2 = v2;
	
	return check_equal(u1->protocol, u2->protocol)
		&& check_equal(u1->user, u2->user)
		&& check_equal(u1->authmech, u2->authmech)
		&& check_equal(u1->host, u2->host)
		&& check_equal(u1->path, u2->path)
		&& check_equal(u1->query, u2->query)
		&& u1->port == u2->port;
}
