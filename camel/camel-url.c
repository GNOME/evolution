/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-url.c : utility functions to parse URLs */

/* 
 * Authors:
 *  Dan Winship <danw@ximian.com>
 *  Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright 1999-2001 Ximian, Inc. (www.ximian.com)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camel-url.h"
#include "camel-exception.h"
#include "camel-mime-utils.h"
#include "camel-object.h"
#include "camel-string-utils.h"

static void copy_param (GQuark key_id, gpointer data, gpointer user_data);
static void output_param (GQuark key_id, gpointer data, gpointer user_data);

static void append_url_encoded (GString *str, const char *in, const char *extra_enc_chars);

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

	/* Find fragment.  RFC 1808 2.4.1 */
	end = hash = strchr (url_string, '#');
	if (hash) {
		if (hash[1]) {
			url->fragment = g_strdup (hash + 1);
			camel_url_decode (url->fragment);
		}
	} else
		end = url_string + strlen (url_string);

	/* Find protocol: initial [a-z+.-]* substring until ":" */
	p = url_string;
	while (p < end && (isalnum ((unsigned char)*p) ||
			   *p == '.' || *p == '+' || *p == '-'))
		p++;

	if (p > url_string && *p == ':') {
		url->protocol = g_strndup (url_string, p - url_string);
		camel_strdown (url->protocol);
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
					value = g_strndup (eq + 1, p - (eq + 1));
					camel_url_decode (value);
				} else {
					name = g_strndup (cur, p - cur);
					value = g_strdup ("");
				}
				camel_url_decode (name);
				g_datalist_set_data_full (&url->params, name,
							  value, g_free);
				g_free (name);
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
 * @ex: a CamelException
 *
 * Parses an absolute URL.
 *
 * Return value: a CamelURL, or %NULL.
 **/
CamelURL *
camel_url_new (const char *url_string, CamelException *ex)
{
	CamelURL *url = camel_url_new_with_base (NULL, url_string);

	if (!url->protocol) {
		camel_url_free (url);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_URL_INVALID,
				      _("Could not parse URL `%s'"),
				      url_string);
		return NULL;
	}
	return url;
}

/**
 * camel_url_to_string:
 * @url: a CamelURL
 * @flags: additional translation options.
 *
 * Return value: a string representing @url, which the caller must free.
 **/
char *
camel_url_to_string (CamelURL *url, guint32 flags)
{
	GString *str;
	char *return_result;
	
	/* IF YOU CHANGE ANYTHING IN THIS FUNCTION, RUN
	 * tests/misc/url AFTERWARD.
	 */
	
	str = g_string_sized_new (20);
	
	if (url->protocol)
		g_string_append_printf (str, "%s:", url->protocol);
	
	if (url->host) {
		g_string_append (str, "//");
		if (url->user) {
			append_url_encoded (str, url->user, ":;@/");
			if (url->authmech && *url->authmech) {
				g_string_append (str, ";auth=");
				append_url_encoded (str, url->authmech, ":@/");
			}
			if (url->passwd && !(flags & CAMEL_URL_HIDE_PASSWORD)) {
				g_string_append_c (str, ':');
				append_url_encoded (str, url->passwd, "@/");
			}
			g_string_append_c (str, '@');
		}
		append_url_encoded (str, url->host, ":/");
		if (url->port)
			g_string_append_printf (str, ":%d", url->port);
		if (!url->path && (url->params || url->query || url->fragment))
			g_string_append_c (str, '/');
	}
	
	if (url->path)
		append_url_encoded (str, url->path, ";?");
	if (url->params && !(flags & CAMEL_URL_HIDE_PARAMS))
		g_datalist_foreach (&url->params, output_param, str);
	if (url->query) {
		g_string_append_c (str, '?');
		append_url_encoded (str, url->query, NULL);
	}
	if (url->fragment) {
		g_string_append_c (str, '#');
		append_url_encoded (str, url->fragment, NULL);
	}
	
	return_result = str->str;
	g_string_free (str, FALSE);
	
	return return_result;
}

static void
output_param (GQuark key_id, gpointer data, gpointer user_data)
{
	GString *str = user_data;

	g_string_append_c (str, ';');
	append_url_encoded (str, g_quark_to_string (key_id), "?=");
	if (*(char *)data) {
		g_string_append_c (str, '=');
		append_url_encoded (str, data, "?");
	}
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
	if (url) {
		if (url->passwd)
			memset(url->passwd, 0, strlen(url->passwd));
		if (url->user)
			memset(url->user, 0, strlen(url->user));
		if (url->host)
			memset(url->host, 0, strlen(url->host));
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
	g_datalist_set_data_full (&url->params, name, value ? g_strdup (value) : NULL, g_free);
}

const char *
camel_url_get_param (CamelURL *url, const char *name)
{
	return g_datalist_get_data (&url->params, name);
}

/* From RFC 2396 2.4.3, the characters that should always be encoded */
static const char url_encoded_char[] = {
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0x00 - 0x0f */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0x10 - 0x1f */
	1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  ' ' - '/'  */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0,  /*  '0' - '?'  */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  '@' - 'O'  */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0,  /*  'P' - '_'  */
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  '`' - 'o'  */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1,  /*  'p' - 0x7f */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

static void
append_url_encoded (GString *str, const char *in, const char *extra_enc_chars)
{
	const unsigned char *s = (const unsigned char *)in;

	while (*s) {
		if (url_encoded_char[*s] ||
		    (extra_enc_chars && strchr (extra_enc_chars, *s)))
			g_string_append_printf (str, "%%%02x", (int)*s++);
		else
			g_string_append_c (str, *s++);
	}
}

/**
 * camel_url_encode:
 * @part: a URL part
 * @escape_extra: additional characters beyond " \"%#<>{}|\^[]`"
 * to escape (or %NULL)
 *
 * This %-encodes the given URL part and returns the escaped version
 * in allocated memory, which the caller must free when it is done.
 **/
char *
camel_url_encode (const char *part, const char *escape_extra)
{
	GString *str;
	char *encoded;

	str = g_string_new (NULL);
	append_url_encoded (str, part, escape_extra);
	encoded = str->str;
	g_string_free (str, FALSE);

	return encoded;
}

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
	unsigned char *s, *d;

#define XDIGIT(c) ((c) <= '9' ? (c) - '0' : ((c) & 0x4F) - 'A' + 10)

	s = d = (unsigned char *)part;
	do {
		if (*s == '%' && isxdigit(s[1]) && isxdigit(s[2])) {
			*d++ = (XDIGIT (s[1]) << 4) + XDIGIT (s[2]);
			s += 2;
		} else
			*d++ = *s;
	} while (*s++);
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

CamelURL *
camel_url_copy(const CamelURL *in)
{
	CamelURL *out;

	out = g_malloc(sizeof(*out));
	out->protocol = g_strdup(in->protocol);
	out->user = g_strdup(in->user);
	out->authmech = g_strdup(in->authmech);
	out->passwd = g_strdup(in->passwd);
	out->host = g_strdup(in->host);
	out->port = in->port;
	out->path = g_strdup(in->path);
	out->params = NULL;
	if (in->params)
		g_datalist_foreach(&((CamelURL *)in)->params, copy_param, &out->params);
	out->query = g_strdup(in->query);
	out->fragment = g_strdup(in->fragment);

	return out;
}
