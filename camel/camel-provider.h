/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-provider.h :  provider definition  */

/*
 *
 * Authors:
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *  Jeffrey Stedfast <fejj@helixcode.com>
 *
 * Copyright 1999, 2000 Helix Code, Inc. (http://www.helixcode.com)
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

#include <camel/camel-types.h>
#include <camel/camel-object.h>

#define CAMEL_PROVIDER(obj) ((CamelProvider *)(obj))

typedef enum {
	CAMEL_PROVIDER_STORE,
	CAMEL_PROVIDER_TRANSPORT,
	CAMEL_NUM_PROVIDER_TYPES
} CamelProviderType;

extern char *camel_provider_type_name[CAMEL_NUM_PROVIDER_TYPES];

/* _IS_SOURCE means the user can get mail from there.
 * _IS_STORAGE means the user can read mail from there.
 */
#define CAMEL_PROVIDER_IS_REMOTE	(1 << 0)
#define CAMEL_PROVIDER_IS_SOURCE	(1 << 1)
#define CAMEL_PROVIDER_IS_STORAGE	(1 << 2)


/* Flags for url_flags. "ALLOW" means the config dialog will let
 * the user configure it. "NEED" implies "ALLOW" but means the user
 * must configure it. Service code can assume that any url part
 * for which it has set the NEED flag will be set when the service
 * is created.
 */
#define CAMEL_URL_ALLOW_USER	 (1 << 0)
#define CAMEL_URL_ALLOW_AUTH	 (1 << 1)
#define CAMEL_URL_ALLOW_PASSWORD (1 << 2)
#define CAMEL_URL_ALLOW_HOST	 (1 << 3)
#define CAMEL_URL_ALLOW_PORT	 (1 << 4)
#define CAMEL_URL_ALLOW_PATH	 (1 << 5)

#define CAMEL_URL_NEED_USER	 (1 << 6 | 1 << 0)
#define CAMEL_URL_NEED_AUTH	 (1 << 7 | 1 << 1)
#define CAMEL_URL_NEED_PASSWORD	 (1 << 8 | 1 << 2)
#define CAMEL_URL_NEED_HOST	 (1 << 9 | 1 << 3)
#define CAMEL_URL_NEED_PORT	 (1 << 10 | 1 << 4)
#define CAMEL_URL_NEED_PATH	 (1 << 11 | 1 << 5)

#define CAMEL_URL_PATH_IS_ABSOLUTE (1 << 12)

#define CAMEL_URL_ALLOW_SSL      (1 << 13)

typedef struct {
	/* Provider name used in CamelURLs. */
	char *protocol;

	/* Provider name as used by people. (May be the same as protocol) */
	char *name;

	/* Description of the provider. A novice user should be able
	 * to read this description, and the information provided by
	 * an ISP, IS department, etc, and determine whether or not
	 * this provider is relevant to him, and if so, which
	 * information goes with it.
	 */
	char *description;

	/* The category of message that this provider works with.
	 * (evolution-mail will only list a provider in the store/transport
	 * config dialogs if its domain is "mail".)
	 */
	char *domain;

	int flags, url_flags;

	CamelType object_types [CAMEL_NUM_PROVIDER_TYPES];

	GHashTable *service_cache;
	
} CamelProvider;

GHashTable *camel_provider_init (void);
void camel_provider_load (CamelSession *session, const char *path, CamelException *ex);

/* This is defined by each module, not by camel-provider.c. */
void camel_provider_module_init (CamelSession *session);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_PROVIDER_H */
