/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
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


#ifndef __CAMEL_SASL_GSSAPI_H__
#define __CAMEL_SASL_GSSAPI_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <sys/types.h>
#include <camel/camel-sasl.h>

#define CAMEL_SASL_GSSAPI_TYPE     (camel_sasl_gssapi_get_type ())
#define CAMEL_SASL_GSSAPI(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_SASL_GSSAPI_TYPE, CamelSaslGssapi))
#define CAMEL_SASL_GSSAPI_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_SASL_GSSAPI_TYPE, CamelSaslGssapiClass))
#define CAMEL_IS_SASL_GSSAPI(o)    (CAMEL_CHECK_TYPE((o), CAMEL_SASL_GSSAPI_TYPE))

typedef struct _CamelSaslGssapi CamelSaslGssapi;
typedef struct _CamelSaslGssapiClass CamelSaslGssapiClass;

struct _CamelSaslGssapi {
	CamelSasl parent_object;
	
	struct _CamelSaslGssapiPrivate *priv;
	
};

struct _CamelSaslGssapiClass {
	CamelSaslClass parent_class;
	
};

/* Standard Camel function */
CamelType camel_sasl_gssapi_get_type (void);

extern CamelServiceAuthType camel_sasl_gssapi_authtype;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAMEL_SASL_GSSAPI_H__ */
