/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-provider.h :  provider definition  */

/* 
 *
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@inria.fr> .
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


#ifndef CAMEL_PROVIDER_H
#define CAMEL_PROVIDER_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>
#include <gmodule.h>

#define CAMEL_PROVIDER(obj) (CamelProvider *)(obj)

typedef enum {
	PROVIDER_STORE,
	PROVIDER_TRANSPORT
} ProviderType;


typedef struct {
	GtkType object_type;        /* used to create instance of the provider */
	ProviderType provider_type; /* is a store or a transport */ 
	gchar *protocol;            /* name of the protocol ("imap"/"smtp"/"mh" ...) */
	gchar *name;       /* name of the provider ("Raymond the imap provider") */
	gchar *description;         /* Useful when multiple providers are available for a same protocol */

	GModule *gmodule;
} CamelProvider;

void camel_provider_register (CamelProvider *provider);
const CamelProvider *camel_provider_register_as_module (const gchar *module_path);
const CamelProvider *camel_provider_get_for_protocol (const gchar *protocol, ProviderType type);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_PROVIDER_H */
