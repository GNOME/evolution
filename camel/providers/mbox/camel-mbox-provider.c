/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-mbox-provider.c: mbox provider registration code */

/* 
 * Authors :
 *   Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright (C) 2000 HelixCode (www.helixcode.com).
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
#include "camel-mbox-store.h"
#include "camel-provider.h"
#include "camel-log.h"


static CamelProvider _mbox_provider = {
	(GtkType) 0,
	PROVIDER_STORE,
	"mbox",
	"Camel default mbox provider",
	"This the first full fledged local mail provider",
	(GModule *) NULL
};

CamelProvider *
camel_provider_module_init (void);


CamelProvider *
camel_provider_module_init (void)
{
	_mbox_provider.object_type = camel_mbox_store_get_type();
	return &_mbox_provider;
}



