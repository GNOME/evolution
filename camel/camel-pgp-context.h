/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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

#ifndef CAMEL_PGP_CONTEXT_H
#define CAMEL_PGP_CONTEXT_H

#include <camel/camel-session.h>
#include <camel/camel-stream.h>
#include <camel/camel-exception.h>
#include <camel/camel-cipher-context.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define CAMEL_PGP_CONTEXT_TYPE     (camel_pgp_context_get_type ())
#define CAMEL_PGP_CONTEXT(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_PGP_CONTEXT_TYPE, CamelPgpContext))
#define CAMEL_PGP_CONTEXT_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_PGP_CONTEXT_TYPE, CamelPgpContextClass))
#define CAMEL_IS_PGP_CONTEXT(o)    (CAMEL_CHECK_TYPE((o), CAMEL_PGP_CONTEXT_TYPE))

typedef enum {
	CAMEL_PGP_TYPE_NONE,
	CAMEL_PGP_TYPE_PGP2,   /* no longer supported */
	CAMEL_PGP_TYPE_PGP5,
	CAMEL_PGP_TYPE_PGP6,
	CAMEL_PGP_TYPE_GPG
} CamelPgpType;

typedef struct _CamelPgpContext {
	CamelCipherContext parent_object;
	
	struct _CamelPgpContextPrivate *priv;
	
} CamelPgpContext;

typedef struct _CamelPgpContextClass {
	CamelCipherContextClass parent_class;
	
} CamelPgpContextClass;

CamelType         camel_pgp_context_get_type (void);

CamelCipherContext  *camel_pgp_context_new (CamelSession *session, CamelPgpType type,
					    const char *path);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_PGP_CONTEXT_H */
