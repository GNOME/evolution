/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  Camel
 *  Copyright (C) 1999-2004 Jeffrey Stedfast
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
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <camel/camel-sasl.h>
#include <camel/camel-provider.h>

#include "camel-imap4-store.h"


CamelProviderConfEntry imap4_conf_entries[] = {
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
	{ CAMEL_PROVIDER_CONF_END }
};

static CamelProvider imap4_provider = {
	"imap4",
	N_("IMAPv4rev1"),
	
	N_("For reading and storing mail on IMAPv4rev1 servers. EXPERIMENTAL !!"),
	
	"mail",
	
	CAMEL_PROVIDER_IS_REMOTE | CAMEL_PROVIDER_IS_SOURCE |
	CAMEL_PROVIDER_IS_STORAGE | CAMEL_PROVIDER_SUPPORTS_SSL,
	
	CAMEL_URL_NEED_USER | CAMEL_URL_NEED_HOST | CAMEL_URL_ALLOW_AUTH | CAMEL_URL_FRAGMENT_IS_PATH,
	
	imap4_conf_entries,
	
	/* ... */
};

CamelServiceAuthType camel_imap4_password_authtype = {
	N_("Password"),
	
	N_("This option will connect to the IMAPv4rev1 server using a "
	   "plaintext password."),
	
	"",
	TRUE
};


static void
add_hash (guint *hash, char *s)
{
	if (s)
		*hash ^= g_str_hash(s);
}

static guint
imap4_url_hash (gconstpointer key)
{
	const CamelURL *u = (CamelURL *)key;
	guint hash = 0;
	
	add_hash (&hash, u->user);
	add_hash (&hash, u->authmech);
	add_hash (&hash, u->host);
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

static int
imap4_url_equal (gconstpointer a, gconstpointer b)
{
	const CamelURL *u1 = a, *u2 = b;
	
	return check_equal (u1->protocol, u2->protocol)
		&& check_equal (u1->user, u2->user)
		&& check_equal (u1->authmech, u2->authmech)
		&& check_equal (u1->host, u2->host)
		&& u1->port == u2->port;
}


void
camel_provider_module_init (void)
{
	imap4_provider.object_types[CAMEL_PROVIDER_STORE] = camel_imap4_store_get_type ();
	imap4_provider.url_hash = imap4_url_hash;
	imap4_provider.url_equal = imap4_url_equal;
	imap4_provider.authtypes = camel_sasl_authtype_list (FALSE);
	imap4_provider.authtypes = g_list_prepend (imap4_provider.authtypes, &camel_imap4_password_authtype);
	
	camel_provider_register (&imap4_provider);
}
