/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-pop3-provider.c: pop3 provider registration code */

/* 
 * Authors :
 *   Dan Winship <danw@ximian.com>
 *   Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 2000 Ximian, Inc. (www.ximian.com)
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

#include "camel-pop3-store.h"
#include "camel-provider.h"
#include "camel-session.h"
#include "camel-url.h"
#include "camel-sasl.h"
#include "camel-i18n.h"

CamelProviderConfEntry pop3_conf_entries[] = {
	{ CAMEL_PROVIDER_CONF_SECTION_START, "storage", NULL,
	  N_("Message storage") },
	{ CAMEL_PROVIDER_CONF_CHECKBOX, "keep_on_server", NULL,
	  N_("Leave messages on server"), "0" },
#ifdef NOT_FOR_1_0
	{ CAMEL_PROVIDER_CONF_CHECKSPIN, "delete_after", "UNIMPLEMENTED",
	  N_("Delete after %s day(s)"), "0:1:7:365" },
#endif
	{ CAMEL_PROVIDER_CONF_CHECKBOX, "disable_extensions", NULL,
	  N_("Disable support for all POP3 extensions"), "0" },
	{ CAMEL_PROVIDER_CONF_SECTION_END },
	{ CAMEL_PROVIDER_CONF_END }
};

static CamelProvider pop3_provider = {
	"pop",
	
	N_("POP"),
	
	N_("For connecting to and downloading mail from POP servers."),
	
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

void
camel_provider_module_init(void)
{
	CamelServiceAuthType *auth;

	pop3_provider.object_types[CAMEL_PROVIDER_STORE] = camel_pop3_store_get_type();
	pop3_provider.url_hash = camel_url_hash;
	pop3_provider.url_equal = camel_url_equal;

	pop3_provider.authtypes = camel_sasl_authtype_list (FALSE);
	auth = camel_sasl_authtype("LOGIN");
	if (auth)
		pop3_provider.authtypes = g_list_prepend(pop3_provider.authtypes, auth);
	pop3_provider.authtypes = g_list_prepend(pop3_provider.authtypes, &camel_pop3_apop_authtype);
	pop3_provider.authtypes = g_list_prepend(pop3_provider.authtypes, &camel_pop3_password_authtype);

	camel_provider_register(&pop3_provider);
}
