/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * e-url.c
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Developed by Jon Trowbridge <trow@ximian.com>
 */

/*
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
 * USA.
 */

#include <config.h>
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
