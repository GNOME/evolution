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
#include "camel-gw-listener.h"
#include "camel-i18n.h"

static void add_hash (guint *hash, char *s);
static guint groupwise_url_hash (gconstpointer key);
static gint check_equal (char *s1, char *s2);
static gint groupwise_url_equal (gconstpointer a, gconstpointer b);
static void free_groupwise_listener ( void );

static CamelGwListener *config_listener = NULL;

CamelProviderConfEntry groupwise_conf_entries[] = {
	/* override the labels/defaults of the standard settings */

	{ CAMEL_PROVIDER_CONF_SECTION_START, "mailcheck", NULL,
	  N_("Checking for new mail") },
	{ CAMEL_PROVIDER_CONF_CHECKBOX, "check_all", NULL,
	  N_("Check for new messages in all folders"), "1" },
	{ CAMEL_PROVIDER_CONF_SECTION_END },

	{ CAMEL_PROVIDER_CONF_SECTION_START, "general", NULL, N_("Options") },
	{ CAMEL_PROVIDER_CONF_CHECKBOX, "filter", NULL,
	  N_("Apply filters to new messages in Inbox on this server"), "0" },
	{ CAMEL_PROVIDER_CONF_CHECKBOX, "filter_junk", NULL,
	  N_("Check new messages for Junk contents"), "0" },
	{ CAMEL_PROVIDER_CONF_CHECKBOX, "filter_junk_inbox", "filter_junk",
	  N_("Only check for Junk messages in the INBOX folder"), "0" },
	{ CAMEL_PROVIDER_CONF_CHECKBOX, "offline_sync", NULL,
	  N_("Automatically synchronize account locally"), "0" },
	{ CAMEL_PROVIDER_CONF_SECTION_END },

	/* extra Groupwise  configuration settings */
	{CAMEL_PROVIDER_CONF_SECTION_START, "soapport", NULL,
	  N_("Address Book and Calendar") },

	{ CAMEL_PROVIDER_CONF_ENTRY , "poa", NULL,
	 N_("Post Office Agent:"), NULL }, 
	 
	{ CAMEL_PROVIDER_CONF_ENTRY, "soap_port", NULL,
	  N_("Post Office Agent SOAP Port:"), "7181" },
	
	{ CAMEL_PROVIDER_CONF_CHECKBOX, "soap_ssl", NULL,
	  N_("Use Secure Connection (SSL)"), "0"},

	{ CAMEL_PROVIDER_CONF_HIDDEN, "auth-domain", NULL,
	  NULL, "Groupwise" },
	 	
	{ CAMEL_PROVIDER_CONF_SECTION_END }, 

	{ CAMEL_PROVIDER_CONF_END }
};


static CamelProvider groupwise_provider = {
	"groupwise",
	N_("Novell GroupWise"),

	N_("For accessing Novell Groupwise servers"),

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

static int
groupwise_auto_detect_cb (CamelURL *url, GHashTable **auto_detected,
			 CamelException *ex)
{
	*auto_detected = g_hash_table_new (g_str_hash, g_str_equal);

	g_hash_table_insert (*auto_detected, g_strdup ("poa"),
			     g_strdup (url->host));

	return 0;
}

void
camel_provider_module_init(void)
{
	CamelProvider *imap_provider;
	CamelException ex = CAMEL_EXCEPTION_INITIALISER;

	imap_provider =  camel_provider_get("imap://", &ex);
	groupwise_provider.url_hash = groupwise_url_hash;
	groupwise_provider.url_equal = groupwise_url_equal;
	groupwise_provider.auto_detect = groupwise_auto_detect_cb;
	groupwise_provider.authtypes = g_list_prepend (groupwise_provider.authtypes, &camel_groupwise_password_authtype);
	if (imap_provider != NULL) {
		groupwise_provider.object_types[CAMEL_PROVIDER_STORE] =  imap_provider->object_types [CAMEL_PROVIDER_STORE];
		camel_provider_register(&groupwise_provider);
	} else {
		camel_exception_clear(&ex);
	}

	if (!config_listener) {
		config_listener = camel_gw_listener_new ();	
		g_atexit ( free_groupwise_listener );
	}
}

void free_groupwise_listener ( void )
{
	g_object_unref (config_listener);
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
