/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-nntp-provider.c: nntp provider registration code */

/* 
 * Authors :
 *   Chris Toshok <toshok@helixcode.com>
 *
 * Copyright (C) 2000 Helix Code, Inc. (www.helixcode.com)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include "camel-nntp-store.h"
#include "camel-provider.h"
#include "camel-session.h"

static void add_hash (guint *hash, char *s);
static guint nntp_url_hash (gconstpointer key);
static gint check_equal (char *s1, char *s2);
static gint nntp_url_equal (gconstpointer a, gconstpointer b);

static CamelProvider news_provider = {
	"nntp",
	N_("USENET news"),

	N_("This is a provider for reading from and posting to"
	   "USENET newsgroups."),

	"news",

	CAMEL_PROVIDER_IS_REMOTE | CAMEL_PROVIDER_IS_STORAGE,

	CAMEL_URL_NEED_HOST | CAMEL_URL_ALLOW_USER |
	CAMEL_URL_ALLOW_PASSWORD | CAMEL_URL_ALLOW_AUTH,

	/* ... */
};

void
camel_provider_module_init (CamelSession *session)
{
	news_provider.object_types[CAMEL_PROVIDER_STORE] =
		camel_nntp_store_get_type();

	news_provider.service_cache = g_hash_table_new (nntp_url_hash, nntp_url_equal);

	camel_session_register_provider (session, &news_provider);
}

static void
add_hash (guint *hash, char *s)
{
	if (s)
		*hash ^= g_str_hash(s);
}

static guint
nntp_url_hash (gconstpointer key)
{
	const CamelURL *u = (CamelURL *)key;
	guint hash = 0;

	add_hash (&hash, u->user);
	add_hash (&hash, u->host);
	hash ^= u->port;
	
	return hash;
}

static gint
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

static gint
nntp_url_equal (gconstpointer a, gconstpointer b)
{
	const CamelURL *u1 = a, *u2 = b;
	
	return check_equal (u1->user, u2->user)
		&& check_equal (u1->host, u2->host)
		&& u1->port == u2->port;
}
