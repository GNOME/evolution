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
#include "camel-sasl.h"

static CamelObjectClass *parent_class = NULL;

/* Returns the class for a CamelSasl */
#define CS_CLASS(so) CAMEL_SASL_CLASS (CAMEL_OBJECT_GET_CLASS (so))

static GByteArray *sasl_challenge (CamelSasl *sasl, const char *token, CamelException *ex);

static void
camel_sasl_class_init (CamelSaslClass *camel_sasl_class)
{
	parent_class = camel_type_get_global_classfuncs (CAMEL_OBJECT_TYPE);
	
	/* virtual method definition */
	camel_sasl_class->challenge = sasl_challenge;
}

CamelType
camel_sasl_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (CAMEL_OBJECT_TYPE,
					    "CamelSasl",
					    sizeof (CamelSasl),
					    sizeof (CamelSaslClass),
					    (CamelObjectClassInitFunc) camel_sasl_class_init,
					    NULL,
					    NULL,
					    NULL );
	}
	
	return type;
}


static GByteArray *
sasl_challenge (CamelSasl *sasl, const char *token, CamelException *ex)
{
	g_warning ("sasl_challenge: Using default implementation!");
	return NULL;
}

/**
 * camel_sasl_challenge:
 * @sasl: a sasl object
 * @token: a token
 * @ex: exception
 *
 * Generate the next sasl challenge to send to the server.
 *
 * Return value: a string containing the base64 encoded sasl challenge
 * or NULL on either an error or if the negotiation is complete. If an
 * error has occured, @ex will also be set.
 **/
GByteArray *
camel_sasl_challenge (CamelSasl *sasl, const char *token, CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_SASL (sasl), NULL);
	
	return CS_CLASS (sasl)->challenge (sasl, token, ex);
}
