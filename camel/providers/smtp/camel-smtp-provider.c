/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-smtp-provider.c: smtp provider registration code */

/* 
 * Authors :
 *   Jeffrey Stedfast <fejj@stampede.org>
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

#include "config.h"
#include "camel-smtp-transport.h"
#include "camel-provider.h"
#include "camel-session.h"
#include "camel-url.h"

static CamelProvider smtp_provider = {
	"smtp",
	"SMTP",

	"For delivering mail by connecting to a remote mailhub using SMTP.",

	"mail",

	0,

	{ 0, 0 },
	{ 0, 25 },

	NULL
};

void
camel_provider_module_init (CamelSession *session)
{
	smtp_provider.object_types[CAMEL_PROVIDER_TRANSPORT] =
		camel_smtp_transport_get_type();

	smtp_provider.service_cache = g_hash_table_new (camel_url_hash, camel_url_equal);
	
	camel_session_register_provider (session, &smtp_provider);
}



