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

#include <config.h>
#include "camel-sasl-plain.h"
#include "camel-service.h"
#include <string.h>

static CamelSaslClass *parent_class = NULL;

/* Returns the class for a CamelSaslPlain */
#define CSP_CLASS(so) CAMEL_SASL_PLAIN_CLASS (CAMEL_OBJECT_GET_CLASS (so))

static GByteArray *plain_challenge (CamelSasl *sasl, GByteArray *token, CamelException *ex);

static void
camel_sasl_plain_class_init (CamelSaslPlainClass *camel_sasl_plain_class)
{
	CamelSaslClass *camel_sasl_class = CAMEL_SASL_CLASS (camel_sasl_plain_class);
	
	parent_class = CAMEL_SASL_CLASS (camel_type_get_global_classfuncs (camel_sasl_get_type ()));
	
	/* virtual method overload */
	camel_sasl_class->challenge = plain_challenge;
}

CamelType
camel_sasl_plain_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_sasl_get_type (),
					    "CamelSaslPlain",
					    sizeof (CamelSaslPlain),
					    sizeof (CamelSaslPlainClass),
					    (CamelObjectClassInitFunc) camel_sasl_plain_class_init,
					    NULL,
					    NULL,
					    NULL);
	}
	
	return type;
}

static GByteArray *
plain_challenge (CamelSasl *sasl, GByteArray *token, CamelException *ex)
{
	GByteArray *buf = NULL;
	CamelURL *url = sasl->service->url;

	if (token) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
				     _("Authentication failed."));
		return NULL;
	}

	g_return_val_if_fail (url->passwd != NULL, NULL);

	/* FIXME: make sure these are "UTF8-SAFE" */
	buf = g_byte_array_new ();
	g_byte_array_append (buf, "", 1);
	g_byte_array_append (buf, url->user, strlen (url->user));
	g_byte_array_append (buf, "", 1);
	g_byte_array_append (buf, url->passwd, strlen (url->passwd));

	sasl->authenticated = TRUE;
	
	return buf;
}
