/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-sendmail-provider.c: sendmail provider registration code */

/* 
 * Authors :
 *   Dan Winship <danw@ximian.com>
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

#include "camel-provider.h"
#include "camel-sendmail-transport.h"
#include "camel-session.h"
#include "camel-url.h"
#include "camel-i18n.h"

static CamelProvider sendmail_provider = {
	"sendmail",
	N_("Sendmail"),

	N_("For delivering mail by passing it to the \"sendmail\" program "
	   "on the local system."),

	"mail",

	0, /* flags */

	0, /* url_flags */

	/* ... */
};

void
camel_provider_module_init(void)
{
	sendmail_provider.object_types[CAMEL_PROVIDER_TRANSPORT] = camel_sendmail_transport_get_type();
	
	sendmail_provider.url_hash = camel_url_hash;
	sendmail_provider.url_equal = camel_url_equal;
	
	camel_provider_register(&sendmail_provider);
}



