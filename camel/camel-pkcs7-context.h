/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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

#ifndef CAMEL_PKCS7_CONTEXT_H
#define CAMEL_PKCS7_CONTEXT_H

#include <camel/camel-session.h>
#include <camel/camel-stream.h>
#include <camel/camel-exception.h>
#include <camel/camel-cipher-context.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define CAMEL_PKCS7_CONTEXT_TYPE     (camel_pkcs7_context_get_type ())
#define CAMEL_PKCS7_CONTEXT(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_PKCS7_CONTEXT_TYPE, CamelPkcs7Context))
#define CAMEL_PKCS7_CONTEXT_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_PKCS7_CONTEXT_TYPE, CamelPkcs7ContextClass))
#define CAMEL_IS_PKCS7_CONTEXT(o)    (CAMEL_CHECK_TYPE((o), CAMEL_PKCS7_CONTEXT_TYPE))

typedef struct _CamelPkcs7Context {
	CamelCipherContext parent_object;
	
	struct _CamelPkcs7ContextPrivate *priv;
	
} CamelPkcs7Context;

typedef struct _CamelPkcs7ContextClass {
	CamelCipherContextClass parent_class;
	
} CamelPkcs7ContextClass;


CamelType            camel_pkcs7_context_get_type (void);

CamelPkcs7Context   *camel_pkcs7_context_new (CamelSession *session, const char *certdb_path);

/* PKCS7 routines */
#define camel_pkcs7_sign(c, u, h, i, o, e) camel_cipher_sign (CAMEL_CIPHER_CONTEXT (c), u, h, i, o, e)

#define camel_pkcs7_clearsign(c, u, h, i, o, e) camel_cipher_clearsign (CAMEL_CIPHER_CONTEXT (c), u, h, i, o, e)

#define camel_pkcs7_verify(c, i, s, e) camel_cipher_verify (CAMEL_CIPHER_CONTEXT (c), i, s, e)

#define camel_pkcs7_encrypt(c, s, u, r, i, o, e) camel_cipher_encrypt (CAMEL_CIPHER_CONTEXT (c), s, u, r, i, o, e)

#define camel_pkcs7_decrypt(c, i, o, e) camel_cipher_decrypt (CAMEL_CIPHER_CONTEXT (c), i, o, e)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_PKCS7_CONTEXT_H */
