/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-pop3-provider.c: pop3 provider registration code */

/* 
 * Authors :
 *   Dan Winship <danw@helixcode.com>
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

#include "camel-pop3-store.h"
#include "camel-provider.h"
#include "camel-session.h"
#include "camel-url.h"

CamelProviderConfEntry pop3_conf_entries[] = {
	{ CAMEL_PROVIDER_CONF_SECTION_START, NULL, NULL,
	  N_("Message storage") },
	{ CAMEL_PROVIDER_CONF_CHECKBOX, "keep_on_server", NULL,
	  N_("Leave messages on server"), "0" },
	{ CAMEL_PROVIDER_CONF_CHECKSPIN, "delete_after", "UNIMPLEMENTED",
	  N_("Delete after %s day(s)"), "0:1:7:365" },
	{ CAMEL_PROVIDER_CONF_SECTION_END },
	{ CAMEL_PROVIDER_CONF_END }
};

static CamelProvider pop3_provider = {
	"pop",
	N_("POP"),

	N_("For connecting to POP servers. The POP protocol can also "
	   "be used to retrieve mail from certain web mail providers "
	   "and proprietary email systems."),

	"mail",

	CAMEL_PROVIDER_IS_REMOTE | CAMEL_PROVIDER_IS_SOURCE |
	CAMEL_PROVIDER_SUPPORTS_SSL,

	CAMEL_URL_NEED_USER | CAMEL_URL_NEED_HOST | CAMEL_URL_ALLOW_AUTH,

	pop3_conf_entries,

	/* ... */
};

CamelServiceAuthType camel_pop3_password_authtype = {
	N_("Password"),

	N_("This option will connect to the POP server using a plaintext "
	   "password. This is the only option supported by many POP servers."),

	"",
	TRUE
};

CamelServiceAuthType camel_pop3_apop_authtype = {
	"APOP",

	N_("This option will connect to the POP server using an encrypted "
	   "password via the APOP protocol. This may not work for all users "
	   "even on servers that claim to support it."),

	"+APOP",
	TRUE
};

#ifdef HAVE_KRB4
CamelServiceAuthType camel_pop3_kpop_authtype = {
	"Kerberos 4 (KPOP)",

	N_("This will connect to the POP server and use Kerberos 4 "
	   "to authenticate to it."),

	"+KPOP",
	FALSE
};
#endif

void
camel_provider_module_init (CamelSession *session)
{
	pop3_provider.object_types[CAMEL_PROVIDER_STORE] =
		camel_pop3_store_get_type ();
	pop3_provider.service_cache = g_hash_table_new (camel_url_hash, camel_url_equal);

#ifdef HAVE_KRB4
	pop3_provider.authtypes = g_list_prepend (camel_remote_store_authtype_list (), &camel_pop3_kpop_authtype);
#endif
	pop3_provider.authtypes = g_list_prepend (pop3_provider.authtypes, &camel_pop3_apop_authtype);
	pop3_provider.authtypes = g_list_prepend (pop3_provider.authtypes, &camel_pop3_password_authtype);

	camel_session_register_provider (session, &pop3_provider);
}
