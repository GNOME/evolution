/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-groupwise-provider.c: Groupwise provider registration code */

/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *           Sivaiah Nallagatla <snallagatla@novell.com>
 *           Rodrigo Moya <rodrigo@ximian.com>
 *
 *  Copyright 2003 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gmodule.h>
#include "camel-imap-store.h"
#include "camel-provider.h"
#include "camel-session.h"
#include "camel-smtp-transport.h"
#include "camel-url.h"
#include "camel-sasl.h"

static void add_hash (guint *hash, char *s);
static guint groupwise_url_hash (gconstpointer key);
static gint check_equal (char *s1, char *s2);
static gint groupwise_url_equal (gconstpointer a, gconstpointer b);

CamelProviderConfEntry groupwise_conf_entries[] = {
	/* override the labels/defaults of the standard settings */

	{ CAMEL_PROVIDER_CONF_SECTION_START, "mailcheck", NULL,
	  N_("Checking for new mail") },
	{ CAMEL_PROVIDER_CONF_CHECKBOX, "check_all", NULL,
	  N_("Check for new messages in all folders"), "1" },
	{ CAMEL_PROVIDER_CONF_SECTION_END },

	/* extra Groupwise  configuration settings */
	{ CAMEL_PROVIDER_CONF_SECTION_START, "ldapserver", NULL,
	  N_("Address Book") },

	{ CAMEL_PROVIDER_CONF_ENTRY, "ldap_server", NULL,
	  N_("LDAP Server Name:") },

	{ CAMEL_PROVIDER_CONF_CHECKSPIN, "ldap_download_limit", NULL,
	  N_("LDAP Download limit: %s"), "y:1:500:10000" },

	{ CAMEL_PROVIDER_CONF_SECTION_END },

	{ CAMEL_PROVIDER_CONF_CHECKBOX, "filter", NULL,
	  N_("Apply filters to new messages in Inbox on this server"), "0" },

	{ CAMEL_PROVIDER_CONF_CHECKBOX, "offline_sync", NULL,
	  N_("Automatically synchronize remote mail locally"), "0" },

	{ CAMEL_PROVIDER_CONF_END }
};


static CamelProvider groupwise_provider = {
	"groupwise",
	N_("Novell GroupWise"),

	N_("For accesing Novell Groupwise servers"),

	"mail",

	CAMEL_PROVIDER_IS_REMOTE | CAMEL_PROVIDER_IS_SOURCE |
	CAMEL_PROVIDER_IS_STORAGE | CAMEL_PROVIDER_SUPPORTS_SSL,

	CAMEL_URL_NEED_USER | CAMEL_URL_NEED_HOST | CAMEL_URL_ALLOW_AUTH,

	groupwise_conf_entries,

	/* ... */
};

CamelServiceAuthType camel_groupwise_password_authtype = {
	N_("Password"),
	
	N_("This option will connect to the IMAP server using a "
	   "plaintext password."),
	
	"",
	TRUE
};

void
camel_provider_module_init (CamelSession *session)
{
	GModule *module;

	/* make sure the IMAP and SMTP providers are loaded */
	module = g_module_open (CAMEL_PROVIDERDIR "/libcamelimap.so", G_MODULE_BIND_LAZY);
	g_module_make_resident (module);

	module = g_module_open (CAMEL_PROVIDERDIR "/libcamelsmtp.so", G_MODULE_BIND_LAZY);
	g_module_make_resident (module);

	groupwise_provider.object_types[CAMEL_PROVIDER_STORE] = camel_imap_store_get_type ();
	groupwise_provider.object_types[CAMEL_PROVIDER_TRANSPORT] = camel_smtp_transport_get_type ();
	groupwise_provider.url_hash = groupwise_url_hash;
	groupwise_provider.url_equal = groupwise_url_equal;
	groupwise_provider.authtypes = g_list_prepend (groupwise_provider.authtypes,
						  &camel_groupwise_password_authtype);

	camel_session_register_provider (session, &groupwise_provider);
}

static void
add_hash (guint *hash, char *s)
{
	if (s)
		*hash ^= g_str_hash(s);
}

static guint
groupwise_url_hash (gconstpointer key)
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
groupwise_url_equal (gconstpointer a, gconstpointer b)
{
	const CamelURL *u1 = a, *u2 = b;
	
	return check_equal (u1->user, u2->user)
		&& check_equal (u1->authmech, u2->authmech)
		&& check_equal (u1->host, u2->host)
		&& u1->port == u2->port;
}
