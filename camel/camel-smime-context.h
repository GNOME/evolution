/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *           Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2002,2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __CAMEL_SMIME_CONTEXT_H__
#define __CAMEL_SMIME_CONTEXT_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-cipher-context.h>

#define CAMEL_SMIME_CONTEXT_TYPE    (camel_smime_context_get_type())
#define CAMEL_SMIME_CONTEXT(obj)    (CAMEL_CHECK_CAST((obj), CAMEL_SMIME_CONTEXT_TYPE, CamelSMIMEContext))
#define CAMEL_SMIME_CONTEXT_CLASS(k)(CAMEL_CHECK_CLASS_CAST((k), CAMEL_SMIME_CONTEXT_TYPE, CamelSMIMEContextClass))
#define CAMEL_IS_SMIME_CONTEXT(o)   (CAMEL_CHECK_TYPE((o), CAMEL_SMIME_CONTEXT_TYPE))

typedef enum _camel_smime_sign_t {
	CAMEL_SMIME_SIGN_CLEARSIGN,
	CAMEL_SMIME_SIGN_ENVELOPED
} camel_smime_sign_t;

typedef enum _camel_smime_describe_t {
	CAMEL_SMIME_SIGNED = 1,
	CAMEL_SMIME_ENCRYPTED = 2,
	CAMEL_SMIME_CERTS = 4,
} camel_smime_describe_t;

typedef struct _CamelSMIMEContext CamelSMIMEContext;
typedef struct _CamelSMIMEContextClass CamelSMIMEContextClass;

struct _CamelSMIMEContext {
	CamelCipherContext cipher;

	struct _CamelSMIMEContextPrivate *priv;
};

struct _CamelSMIMEContextClass {
	CamelCipherContextClass cipher_class;
};

CamelType camel_smime_context_get_type(void);

CamelCipherContext *camel_smime_context_new(CamelSession *session);

/* nick to use for SMIMEEncKeyPrefs attribute for signed data */
void camel_smime_context_set_encrypt_key(CamelSMIMEContext *context, gboolean use, const char *key);
/* set signing mode, clearsigned multipart/signed or enveloped */
void camel_smime_context_set_sign_mode(CamelSMIMEContext *context, camel_smime_sign_t type);

guint32 camel_smime_context_describe_part(CamelSMIMEContext *, struct _CamelMimePart *);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAMEL_SMIME_CONTEXT_H__ */
