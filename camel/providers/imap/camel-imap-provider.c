/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-imap-provider.c: imap provider registration code */

/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#include "config.h"
#include "camel-imap-store.h"
#include "camel-provider.h"
#include "camel-session.h"
#include "camel-url.h"
#include "camel-sasl.h"

static void add_hash (guint *hash, char *s);
static guint imap_url_hash (gconstpointer key);
static gint check_equal (char *s1, char *s2);
static gint imap_url_equal (gconstpointer a, gconstpointer b);

CamelProviderConfEntry imap_conf_entries[] = {
	{ CAMEL_PROVIDER_CONF_SECTION_START, "mailcheck", NULL,
	  N_("Checking for new mail") },
	{ CAMEL_PROVIDER_CONF_CHECKBOX, "check_all", NULL,
	  N_("Check for new messages in all folders"), "1" },
	{ CAMEL_PROVIDER_CONF_SECTION_END },
	{ CAMEL_PROVIDER_CONF_SECTION_START, "folders", NULL,
	  N_("Folders") },
	{ CAMEL_PROVIDER_CONF_CHECKBOX, "use_lsub", NULL,
	  N_("Show only subscribed folders"), "1" },
	{ CAMEL_PROVIDER_CONF_CHECKBOX, "override_namespace", NULL,
	  N_("Override server-supplied folder namespace"), "0" },
	{ CAMEL_PROVIDER_CONF_ENTRY, "namespace", "override_namespace",
	  N_("Namespace") },
	{ CAMEL_PROVIDER_CONF_SECTION_END },
	{ CAMEL_PROVIDER_CONF_CHECKBOX, "filter", "UNIMPLEMENTED",
	  N_("Apply filters to new messages in INBOX on this server"), "0" },
	{ CAMEL_PROVIDER_CONF_END }
};

static CamelProvider imap_provider = {
	"imap",
	N_("IMAPv4"),

	N_("For reading and storing mail on IMAP servers."),

	"mail",

	CAMEL_PROVIDER_IS_REMOTE | CAMEL_PROVIDER_IS_SOURCE |
	CAMEL_PROVIDER_IS_STORAGE | CAMEL_PROVIDER_SUPPORTS_SSL,

	CAMEL_URL_NEED_USER | CAMEL_URL_NEED_HOST | CAMEL_URL_ALLOW_AUTH,

	imap_conf_entries,

	/* ... */
};

CamelServiceAuthType camel_imap_password_authtype = {
	N_("Password"),
	
	N_("This option will connect to the IMAP server using a "
	   "plaintext password."),
	
	"",
	TRUE
};

void
camel_provider_module_init (CamelSession *session)
{
	imap_provider.object_types[CAMEL_PROVIDER_STORE] =
		camel_imap_store_get_type ();
	imap_provider.service_cache = g_hash_table_new (imap_url_hash, imap_url_equal);
	imap_provider.authtypes = g_list_concat (camel_remote_store_authtype_list (),
						 camel_sasl_authtype_list ());
	imap_provider.authtypes = g_list_prepend (imap_provider.authtypes,
						  &camel_imap_password_authtype);

	camel_session_register_provider (session, &imap_provider);
}

static void
add_hash (guint *hash, char *s)
{
	if (s)
		*hash ^= g_str_hash(s);
}

static guint
imap_url_hash (gconstpointer key)
{
	const CamelURL *u = (CamelURL *)key;
	guint hash = 0;

	add_hash (&hash, u->user);
	add_hash (&hash, u->authmech);
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
imap_url_equal (gconstpointer a, gconstpointer b)
{
	const CamelURL *u1 = a, *u2 = b;
	
	return check_equal (u1->user, u2->user)
		&& check_equal (u1->authmech, u2->authmech)
		&& check_equal (u1->host, u2->host)
		&& u1->port == u2->port;
}
