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
#include "camel-sasl-anonymous.h"
#include "camel-internet-address.h"
#include <string.h>

static CamelSaslClass *parent_class = NULL;

/* Returns the class for a CamelSaslAnonymous */
#define CSA_CLASS(so) CAMEL_SASL_ANONYMOUS_CLASS (CAMEL_OBJECT_GET_CLASS (so))

static GByteArray *anon_challenge (CamelSasl *sasl, GByteArray *token, CamelException *ex);

static void
camel_sasl_anonymous_class_init (CamelSaslAnonymousClass *camel_sasl_anonymous_class)
{
	CamelSaslClass *camel_sasl_class = CAMEL_SASL_CLASS (camel_sasl_anonymous_class);
	
	parent_class = CAMEL_SASL_CLASS (camel_type_get_global_classfuncs (camel_sasl_get_type ()));
	
	/* virtual method overload */
	camel_sasl_class->challenge = anon_challenge;
}

static void
camel_sasl_anonymous_finalize (CamelObject *object)
{
	CamelSaslAnonymous *sasl = CAMEL_SASL_ANONYMOUS (object);
	
	g_free (sasl->trace_info);
}


CamelType
camel_sasl_anonymous_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_sasl_get_type (),
					    "CamelSaslAnonymous",
					    sizeof (CamelSaslAnonymous),
					    sizeof (CamelSaslAnonymousClass),
					    (CamelObjectClassInitFunc) camel_sasl_anonymous_class_init,
					    NULL,
					    NULL,
					    (CamelObjectFinalizeFunc) camel_sasl_anonymous_finalize);
	}
	
	return type;
}

CamelSasl *
camel_sasl_anonymous_new (CamelSaslAnonTraceType type, const char *trace_info)
{
	CamelSaslAnonymous *sasl_anon;
	
	if (!trace_info && type != CAMEL_SASL_ANON_TRACE_EMPTY) return NULL;
	
	sasl_anon = CAMEL_SASL_ANONYMOUS (camel_object_new (camel_sasl_anonymous_get_type ()));
	sasl_anon->trace_info = g_strdup (trace_info);
	sasl_anon->type = type;
	
	return CAMEL_SASL (sasl_anon);
}

static GByteArray *
anon_challenge (CamelSasl *sasl, GByteArray *token, CamelException *ex)
{
	CamelSaslAnonymous *sasl_anon = CAMEL_SASL_ANONYMOUS (sasl);
	CamelInternetAddress *cia;
	GByteArray *ret = NULL;

	if (token) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
				     _("Authentication failed."));
		return NULL;
	}

	switch (sasl_anon->type) {
	case CAMEL_SASL_ANON_TRACE_EMAIL:
		cia = camel_internet_address_new ();
		if (camel_internet_address_add (cia, NULL, sasl_anon->trace_info) != 1) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
					      _("Invalid email address trace information:\n%s"),
					      sasl_anon->trace_info);
			camel_object_unref (CAMEL_OBJECT (cia));
			return NULL;
		}
		camel_object_unref (CAMEL_OBJECT (cia));
		ret = g_byte_array_new ();
		g_byte_array_append (ret, sasl_anon->trace_info, strlen (sasl_anon->trace_info));
		break;
	case CAMEL_SASL_ANON_TRACE_OPAQUE:
		if (strchr (sasl_anon->trace_info, '@')) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
					      _("Invalid opaque trace information:\n%s"),
					      sasl_anon->trace_info);
			return NULL;
		}
		ret = g_byte_array_new ();
		g_byte_array_append (ret, sasl_anon->trace_info, strlen (sasl_anon->trace_info));
		break;
	case CAMEL_SASL_ANON_TRACE_EMPTY:
		ret = g_byte_array_new ();
		break;
	default:
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
				      _("Invalid trace information:\n%s"),
				      sasl_anon->trace_info);
		return NULL;
	}

	sasl->authenticated = TRUE;
	return ret;
}
