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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_KRB5

#include <string.h>
#ifdef HAVE_ET_COM_ERR_H
#include <et/com_err.h>
#else
#include <com_err.h>
#endif
#ifdef HAVE_MIT_KRB5
#include <gssapi/gssapi.h>
#include <gssapi/gssapi_generic.h>
#else /* HAVE_HEIMDAL_KRB5 */
#include <gssapi.h>
#define gss_nt_service_name GSS_C_NT_HOSTBASED_SERVICE
#endif
#include <errno.h>

#ifndef GSS_C_OID_KRBV5_DES
#define GSS_C_OID_KRBV5_DES GSS_C_NO_OID
#endif

#include "camel-i18n.h"
#include "camel-sasl-gssapi.h"

CamelServiceAuthType camel_sasl_gssapi_authtype = {
	N_("GSSAPI"),
	
	N_("This option will connect to the server using "
	   "Kerberos 5 authentication."),
	
	"GSSAPI",
	FALSE
};

enum {
	GSSAPI_STATE_INIT,
	GSSAPI_STATE_CONTINUE_NEEDED,
	GSSAPI_STATE_COMPLETE,
	GSSAPI_STATE_AUTHENTICATED
};

#define GSSAPI_SECURITY_LAYER_NONE       (1 << 0)
#define GSSAPI_SECURITY_LAYER_INTEGRITY  (1 << 1)
#define GSSAPI_SECURITY_LAYER_PRIVACY    (1 << 2)

#define DESIRED_SECURITY_LAYER  GSSAPI_SECURITY_LAYER_NONE

struct _CamelSaslGssapiPrivate {
	int state;
	gss_ctx_id_t ctx;
	gss_name_t target;
};


static GByteArray *gssapi_challenge (CamelSasl *sasl, GByteArray *token, CamelException *ex);


static CamelSaslClass *parent_class = NULL;


static void
camel_sasl_gssapi_class_init (CamelSaslGssapiClass *klass)
{
	CamelSaslClass *camel_sasl_class = CAMEL_SASL_CLASS (klass);
	
	parent_class = CAMEL_SASL_CLASS (camel_type_get_global_classfuncs (camel_sasl_get_type ()));
	
	/* virtual method overload */
	camel_sasl_class->challenge = gssapi_challenge;
}

static void
camel_sasl_gssapi_init (gpointer object, gpointer klass)
{
	CamelSaslGssapi *gssapi = CAMEL_SASL_GSSAPI (object);
	
	gssapi->priv = g_new (struct _CamelSaslGssapiPrivate, 1);
	gssapi->priv->state = GSSAPI_STATE_INIT;
	gssapi->priv->ctx = GSS_C_NO_CONTEXT;
	gssapi->priv->target = GSS_C_NO_NAME;
}

static void
camel_sasl_gssapi_finalize (CamelObject *object)
{
	CamelSaslGssapi *gssapi = CAMEL_SASL_GSSAPI (object);
	guint32 status;
	
	if (gssapi->priv->ctx != GSS_C_NO_CONTEXT)
		gss_delete_sec_context (&status, &gssapi->priv->ctx, GSS_C_NO_BUFFER);
	
	if (gssapi->priv->target != GSS_C_NO_NAME)
		gss_release_name (&status, &gssapi->priv->target);
	
	g_free (gssapi->priv);
}


CamelType
camel_sasl_gssapi_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (
			camel_sasl_get_type (),
			"CamelSaslGssapi",
			sizeof (CamelSaslGssapi),
			sizeof (CamelSaslGssapiClass),
			(CamelObjectClassInitFunc) camel_sasl_gssapi_class_init,
			NULL,
			(CamelObjectInitFunc) camel_sasl_gssapi_init,
			(CamelObjectFinalizeFunc) camel_sasl_gssapi_finalize);
	}
	
	return type;
}

static void
gssapi_set_exception (OM_uint32 major, OM_uint32 minor, CamelException *ex)
{
	const char *str;
	
	switch (major) {
	case GSS_S_BAD_MECH:
		str = _("The specified mechanism is not supported by the "
			"provided credential, or is unrecognized by the "
			"implementation.");
		break;
	case GSS_S_BAD_NAME:
		str = _("The provided target_name parameter was ill-formed.");
		break;
	case GSS_S_BAD_NAMETYPE:
		str = _("The provided target_name parameter contained an "
			"invalid or unsupported type of name.");
		break;
	case GSS_S_BAD_BINDINGS:
		str = _("The input_token contains different channel "
			"bindings to those specified via the "
			"input_chan_bindings parameter.");
		break;
	case GSS_S_BAD_SIG:
		str = _("The input_token contains an invalid signature, or a "
			"signature that could not be verified.");
		break;
	case GSS_S_NO_CRED:
		str = _("The supplied credentials were not valid for context "
			"initiation, or the credential handle did not "
			"reference any credentials.");
		break;
	case GSS_S_NO_CONTEXT:
		str = _("The supplied context handle did not refer to a valid context.");
		break;
	case GSS_S_DEFECTIVE_TOKEN:
		str = _("The consistency checks performed on the input_token failed.");
		break;
	case GSS_S_DEFECTIVE_CREDENTIAL:
		str = _("The consistency checks performed on the credential failed.");
		break;
	case GSS_S_CREDENTIALS_EXPIRED:
		str = _("The referenced credentials have expired.");
		break;
	case GSS_S_FAILURE:
		str = error_message (minor);
		break;
	default:
		str = _("Bad authentication response from server.");
	}
	
	camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE, str);
}

static GByteArray *
gssapi_challenge (CamelSasl *sasl, GByteArray *token, CamelException *ex)
{
	struct _CamelSaslGssapiPrivate *priv = CAMEL_SASL_GSSAPI (sasl)->priv;
	OM_uint32 major, minor, flags, time;
	gss_buffer_desc inbuf, outbuf;
	GByteArray *challenge = NULL;
	gss_buffer_t input_token;
	int conf_state;
	gss_qop_t qop;
	gss_OID mech;
	char *str;
	struct addrinfo *ai, hints;
	
	switch (priv->state) {
	case GSSAPI_STATE_INIT:
		memset(&hints, 0, sizeof(hints));
		hints.ai_flags = AI_CANONNAME;
		ai = camel_getaddrinfo(sasl->service->url->host?sasl->service->url->host:"localhost", NULL, &hints, ex);
		if (ai == NULL)
			return NULL;
		
		str = g_strdup_printf("%s@%s", sasl->service_name, ai->ai_canonname);
		camel_freeaddrinfo(ai);
		
		inbuf.value = str;
		inbuf.length = strlen (str);
		major = gss_import_name (&minor, &inbuf, gss_nt_service_name, &priv->target);
		g_free (str);
		
		if (major != GSS_S_COMPLETE) {
			gssapi_set_exception (major, minor, ex);
			return NULL;
		}
		
		input_token = GSS_C_NO_BUFFER;
		
		goto challenge;
		break;
	case GSSAPI_STATE_CONTINUE_NEEDED:
		if (token == NULL) {
			camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
					     _("Bad authentication response from server."));
			return NULL;
		}
		
		inbuf.value = token->data;
		inbuf.length = token->len;
		input_token = &inbuf;
		
	challenge:
		major = gss_init_sec_context (&minor, GSS_C_NO_CREDENTIAL, &priv->ctx, priv->target,
					      GSS_C_OID_KRBV5_DES, GSS_C_MUTUAL_FLAG |
					      GSS_C_REPLAY_FLAG | GSS_C_SEQUENCE_FLAG,
					      0, GSS_C_NO_CHANNEL_BINDINGS,
					      input_token, &mech, &outbuf, &flags, &time);
		
		switch (major) {
		case GSS_S_COMPLETE:
			priv->state = GSSAPI_STATE_COMPLETE;
			break;
		case GSS_S_CONTINUE_NEEDED:
			priv->state = GSSAPI_STATE_CONTINUE_NEEDED;
			break;
		default:
			gssapi_set_exception (major, minor, ex);
			return NULL;
		}
		
		challenge = g_byte_array_new ();
		g_byte_array_append (challenge, outbuf.value, outbuf.length);
#ifndef HAVE_HEIMDAL_KRB5
		gss_release_buffer (&minor, &outbuf);
#endif
		break;
	case GSSAPI_STATE_COMPLETE:
		if (token == NULL) {
			camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
					     _("Bad authentication response from server."));
			return NULL;
		}
		
		inbuf.value = token->data;
		inbuf.length = token->len;
		
		major = gss_unwrap (&minor, priv->ctx, &inbuf, &outbuf, &conf_state, &qop);
		if (major != GSS_S_COMPLETE) {
			gssapi_set_exception (major, minor, ex);
			return NULL;
		}
		
		if (outbuf.length < 4) {
			camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
					     _("Bad authentication response from server."));
#ifndef HAVE_HEIMDAL_KRB5
			gss_release_buffer (&minor, &outbuf);
#endif
			return NULL;
		}
		
		/* check that our desired security layer is supported */
		if ((((unsigned char *) outbuf.value)[0] & DESIRED_SECURITY_LAYER) != DESIRED_SECURITY_LAYER) {
			camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
					     _("Unsupported security layer."));
#ifndef HAVE_HEIMDAL_KRB5
			gss_release_buffer (&minor, &outbuf);
#endif
			return NULL;
		}
		
		inbuf.length = 4 + strlen (sasl->service->url->user);
		inbuf.value = str = g_malloc (inbuf.length);
		memcpy (inbuf.value, outbuf.value, 4);
		str[0] = DESIRED_SECURITY_LAYER;
		memcpy (str + 4, sasl->service->url->user, inbuf.length - 4);
		
#ifndef HAVE_HEIMDAL_KRB5
		gss_release_buffer (&minor, &outbuf);
#endif
		
		major = gss_wrap (&minor, priv->ctx, FALSE, qop, &inbuf, &conf_state, &outbuf);
		if (major != GSS_S_COMPLETE) {
			gssapi_set_exception (major, minor, ex);
			g_free (str);
			return NULL;
		}
		
		g_free (str);
		challenge = g_byte_array_new ();
		g_byte_array_append (challenge, outbuf.value, outbuf.length);
		
#ifndef HAVE_HEIMDAL_KRB5
		gss_release_buffer (&minor, &outbuf);
#endif
		
		priv->state = GSSAPI_STATE_AUTHENTICATED;
		
		sasl->authenticated = TRUE;
		break;
	default:
		return NULL;
	}
	
	return challenge;
}

#endif /* HAVE_KRB5 */
