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

#ifndef CAMEL_SASL_DIGEST_MD5_H
#define CAMEL_SASL_DIGEST_MD5_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <sys/types.h>
#include <netdb.h>
#include <camel/camel-sasl.h>

#define CAMEL_SASL_DIGEST_MD5_TYPE     (camel_sasl_digest_md5_get_type ())
#define CAMEL_SASL_DIGEST_MD5(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_SASL_DIGEST_MD5_TYPE, CamelSaslDigestMd5))
#define CAMEL_SASL_DIGEST_MD5_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_SASL_DIGEST_MD5_TYPE, CamelSaslDigestMd5Class))
#define CAMEL_IS_SASL_DIGEST_MD5(o)    (CAMEL_CHECK_TYPE((o), CAMEL_SASL_DIGEST_MD5_TYPE))

typedef struct _CamelSaslDigestMd5 {
	CamelSasl parent_object;
	struct _CamelSaslDigestMd5Private *priv;
	
} CamelSaslDigestMd5;


typedef struct _CamelSaslDigestMd5Class {
	CamelSaslClass parent_class;
	
} CamelSaslDigestMd5Class;


/* Standard Camel function */
CamelType camel_sasl_digest_md5_get_type (void);

extern CamelServiceAuthType camel_sasl_digest_md5_authtype;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_SASL_DIGEST_MD5_H */
