/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <camel/camel.h>

#include "camel-rss-store.h"

static CamelProvider rss_provider = {
	"rss",
	N_("News and Blogs"),

	N_("This is a provider for reading RSS news and blogs."),

	"rss",

	CAMEL_PROVIDER_IS_LOCAL | CAMEL_PROVIDER_IS_SOURCE | CAMEL_PROVIDER_IS_STORAGE,

	CAMEL_URL_NEED_HOST,

	NULL, /* conf_entries */

	NULL, /* port_entries */

	/* ... */
};

static void
add_hash (guint *hash,
          gchar *s)
{
	if (s)
		*hash ^= g_str_hash(s);
}

static guint
rss_url_hash (gconstpointer key)
{
	const CamelURL *u = (CamelURL *) key;
	guint hash = 0;

	add_hash (&hash, u->user);
	add_hash (&hash, u->host);
	hash ^= u->port;

	return hash;
}

static gint
check_equal (gchar *s1,
	     gchar *s2)
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
rss_url_equal (gconstpointer a,
	       gconstpointer b)
{
	const CamelURL *u1 = a, *u2 = b;

	return check_equal (u1->protocol, u2->protocol)
		&& check_equal (u1->user, u2->user)
		&& check_equal (u1->host, u2->host)
		&& u1->port == u2->port;
}

void
camel_provider_module_init (void)
{
	rss_provider.object_types[CAMEL_PROVIDER_STORE] = camel_rss_store_get_type ();
	rss_provider.url_hash = rss_url_hash;
	rss_provider.url_equal = rss_url_equal;
	rss_provider.authtypes = NULL;
	rss_provider.translation_domain = GETTEXT_PACKAGE;

	camel_provider_register (&rss_provider);
}
