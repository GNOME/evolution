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

#ifndef CAMEL_SASL_H
#define CAMEL_SASL_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <glib.h>
#include <camel/camel-object.h>
#include <camel/camel-exception.h>
#include <camel/camel-service.h>

#define CAMEL_SASL_TYPE     (camel_sasl_get_type ())
#define CAMEL_SASL(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_SASL_TYPE, CamelSasl))
#define CAMEL_SASL_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_SASL_TYPE, CamelSaslClass))
#define CAMEL_IS_SASL(o)    (CAMEL_CHECK_TYPE((o), CAMEL_SASL_TYPE))

typedef struct _CamelSasl {
	CamelObject parent_object;
	
	char *service_name;
	char *mech;		/* mechanism */
	CamelService *service;
	gboolean authenticated;
} CamelSasl;


typedef struct _CamelSaslClass {
	CamelObjectClass parent_class;
	
	GByteArray *    (*challenge)   (CamelSasl *sasl, GByteArray *token, CamelException *ex);

} CamelSaslClass;


/* Standard Camel function */
CamelType  camel_sasl_get_type (void);

/* public methods */
GByteArray *camel_sasl_challenge        (CamelSasl *sasl, GByteArray *token, CamelException *ex);
char       *camel_sasl_challenge_base64 (CamelSasl *sasl, const char *token, CamelException *ex);

gboolean    camel_sasl_authenticated    (CamelSasl *sasl);

/* utility functions */
CamelSasl  *camel_sasl_new              (const char *service_name, const char *mechanism, CamelService *service);

GList                *camel_sasl_authtype_list (gboolean include_plain);
CamelServiceAuthType *camel_sasl_authtype      (const char *mechanism);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_SASL_H */
