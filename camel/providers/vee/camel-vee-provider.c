/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *
 *  This program is free software; you can redistribute it and/or 
 *  modify it under the terms of the GNU General Public License as 
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 *  USA
 */

#include "config.h"
#include "camel-vee-store.h"
#include "camel-provider.h"
#include "camel-session.h"
#include "camel-url.h"

static CamelProvider vee_provider = {
	"vfolder",
	"Virtual folder email provider",

	"For reading mail as a query of another set of folders",

	"vfolder",

	0,

	{ 0, 0 },
	{ 0, 0 },

	NULL
};

void
camel_provider_module_init (CamelSession *session)
{
	vee_provider.object_types[CAMEL_PROVIDER_STORE] =
		camel_vee_store_get_type();

	vee_provider.service_cache = g_hash_table_new (camel_url_hash, camel_url_equal);
	
	camel_session_register_provider (session, &vee_provider);
}
