/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * e-url.c
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Developed by Jon Trowbridge <trow@ximian.com>
 *              Rodrigo Moya   <rodrigo@ximian.com>
 */

/*
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
 * USA.
 */

#include <config.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "e-url.h"

char *
e_url_shroud (const char *url)
{
	const char *first_colon = NULL;
	const char *last_at = NULL;
	const char *p;
	char *shrouded;
	
	if (url == NULL)
		return NULL;

	/* Skip past the moniker */
	for (p = url; *p && *p != ':'; ++p);
	if (*p)
		++p;

	while (*p) {
		if (first_colon == NULL && *p == ':')
			first_colon = p;
		if (*p == '@')
			last_at = p;
		++p;
	}

	if (first_colon && last_at && first_colon < last_at) {
		shrouded = g_strdup_printf ("%.*s%s", first_colon - url, url, last_at);
	} else {
		shrouded = g_strdup (url);
	}

	return shrouded;
}

gboolean
e_url_equal (const char *url1, const char *url2)
{
	char *shroud1 = e_url_shroud (url1);
	char *shroud2 = e_url_shroud (url2);
	gint len1, len2;
	gboolean rv;

	if (shroud1 == NULL || shroud2 == NULL) {
		rv = (shroud1 == shroud2);
	} else {
		len1 = strlen (shroud1);
		len2 = strlen (shroud2);

		rv = !strncmp (shroud1, shroud2, MIN (len1, len2));
	}

	g_free (shroud1);
	g_free (shroud2);

	return rv;
}

#define HEXVAL(c) (isdigit (c) ? (c) - '0' : tolower (c) - 'a' + 10)

static void
uri_decode (char *part)
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

EUri *
e_uri_new (const char *uri_string)
{
	EUri *uri;
	const char *end, *hash, *colon, *semi, *at, *slash, *question;
	const char *p;

	if (!uri_string)
		return NULL;

	uri = g_new0 (EUri, 1);

	/* find fragment */
	end = hash = strchr (uri_string, '#');
	if (hash && hash[1]) {
		uri->fragment = g_strdup (hash + 1);
		uri_decode (uri->fragment);
	}
	else
		end = uri_string + strlen (uri_string);

	/* find protocol: initial [a-z+.-]* substring until ":" */
	p = uri_string;
	while (p < end && (isalnum ((unsigned char) *p) ||
			   *p == '.' || *p == '+' || *p == '-'))
		p++;

	if (p > uri_string && *p == ':') {
		uri->protocol = g_ascii_strdown (uri_string, p - uri_string);
		uri_string = p + 1;
	}
	else
		uri->protocol = g_strdup ("file");

	if (!*uri_string)
		return uri;

	/* check for authority */
	if (strncmp (uri_string, "//", 2) == 0) {
		uri_string += 2;

		slash = uri_string + strcspn (uri_string, "/#");
		at = strchr (uri_string, '@');
		if (at && at < slash) {
			colon = strchr (uri_string, ':');
			if (colon && colon < at) {
				uri->passwd = g_strndup (colon + 1, at - colon - 1);
				uri_decode (uri->passwd);
			}
			else {
				uri->passwd = NULL;
				colon = at;
			}

			semi = strchr (uri_string, ';');
			if (semi && semi < colon &&
			    !strncasecmp (semi, ";auth=", 6)) {
				uri->authmech = g_strndup (semi + 6, colon - semi - 6);
				uri_decode (uri->authmech);
			}
			else {
				uri->authmech = NULL;
				semi = colon;
			}

			uri->user = g_strndup (uri_string, semi - uri_string);
			uri_decode (uri->user);
			uri_string = at + 1;
		}
		else
			uri->user = uri->passwd = uri->authmech = NULL;

		/* find host and port */
		colon = strchr (uri_string, ':');
		if (colon && colon < slash) {
			uri->host = g_strndup (uri_string, colon - uri_string);
			uri->port = strtoul (colon + 1, NULL, 10);
		}
		else {
			uri->host = g_strndup (uri_string, slash - uri_string);
			uri_decode (uri->host);
			uri->port = 0;
		}
	}

	/* find query */
	question = memchr (uri_string, '?', end - uri_string);
	if (question) {
		if (question[1]) {
			uri->query = g_strndup (question + 1, end - (question + 1));
			uri_decode (uri->query);
		}
		end = question;
	}

	/* find parameters */
	semi = memchr (uri_string, ';', end - uri_string);
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
					uri_decode (value);
				} else {
					name = g_strndup (cur, p - cur);
					value = g_strdup ("");
				}

				uri_decode (name);
				g_datalist_set_data_full (&uri->params, name,
							  value, g_free);
				g_free (name);
			}
		}
		end = semi;
	}

	if (end != uri_string) {
		uri->path = g_strndup (uri_string, end - uri_string);
		uri_decode (uri->path);
	}

	return uri;
}

void
e_uri_free (EUri *uri)
{
	if (uri) {
		g_free (uri->protocol);
		g_free (uri->user);
		g_free (uri->authmech);
		g_free (uri->passwd);
		g_free (uri->host);
		g_free (uri->path);
		g_datalist_clear (&uri->params);
		g_free (uri->query);
		g_free (uri->fragment);

		g_free (uri);
	}
}

const char *
e_uri_get_param (EUri *uri, const char *name)
{
	return g_datalist_get_data (&uri->params, name);
}

static void
copy_param_cb (GQuark key_id, gpointer data, gpointer user_data)
{
	GData *params = (GData *) user_data;

	g_datalist_id_set_data_full (&params, key_id, g_strdup (data), g_free);
}

EUri *
e_uri_copy (EUri *uri)
{
	EUri *uri_copy;

	g_return_val_if_fail (uri != NULL, NULL);

	uri_copy = g_new0 (EUri, 1);
	uri_copy->protocol = g_strdup (uri->protocol);
	uri_copy->user = g_strdup (uri->user);
	uri_copy->authmech = g_strdup (uri->authmech);
	uri_copy->passwd = g_strdup (uri->passwd);
	uri_copy->host = g_strdup (uri->host);
	uri_copy->port = uri->port;
	uri_copy->path = g_strdup (uri->path);
	uri_copy->query = g_strdup (uri->query);
	uri_copy->fragment = g_strdup (uri->fragment);

	/* copy uri->params */
	g_datalist_foreach (&uri->params,
			    (GDataForeachFunc) copy_param_cb,
			    &uri_copy->params);

	return uri_copy;
}

char *
e_uri_to_string (EUri *uri, gboolean show_password)
{
	char *str_uri = NULL;

	g_return_val_if_fail (uri != NULL, NULL);

	if (uri->port != 0)
		str_uri = g_strdup_printf (
			"%s://%s%s%s%s%s%s%s:%d%s%s%s",
			uri->protocol,
			uri->user ? uri->user : "",
			uri->authmech ? ";auth=" : "",
			uri->authmech ? uri->authmech : "",
			uri->passwd && show_password ? ":" : "",
			uri->passwd && show_password ? uri->passwd : "",
			uri->user ? "@" : "",
			uri->host ? uri->host : "",
			uri->port,
			uri->path ? uri->path : "",
			uri->query ? "?" : "",
			uri->query ? uri->query : "");
	else
		str_uri = g_strdup_printf(
                        "%s://%s%s%s%s%s%s%s%s%s%s",
                        uri->protocol,
                        uri->user ? uri->user : "",
                        uri->authmech ? ";auth=" : "",
                        uri->authmech ? uri->authmech : "",
                        uri->passwd && show_password ? ":" : "",
                        uri->passwd && show_password ? uri->passwd : "",
                        uri->user ? "@" : "",
                        uri->host ? uri->host : "",
                        uri->path ? uri->path : "",
                        uri->query ? "?" : "",
                        uri->query ? uri->query : "");

	return str_uri;
}
