/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-mh-provider.c: mh provider registration code */

/* 
 *
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@aful.org> .
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
#include "camel-mh-store.h"
#include "camel-provider.h"
#include "camel-log.h"


static CamelProvider _mh_provider = {
	(GtkType) 0,
	PROVIDER_STORE,
	"mh",
	"Camel default mh provider",
	"This is a very simple provider, mh is a bad protocol anyway",
	(GModule *) NULL
};



CamelProvider *
camel_provider_module_init ()
{
	_mh_provider.object_type = camel_mh_store_get_type();
	return &_mh_provider;
}
