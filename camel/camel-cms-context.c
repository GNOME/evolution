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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "camel-cms-context.h"

#include <glib.h>

#ifdef ENABLE_THREADS
#include <pthread.h>
#define CMS_LOCK(ctx)   g_mutex_lock (((CamelCMSContext *) ctx)->priv->lock)
#define CMS_UNLOCK(ctx) g_mutex_unlock (((CamelCMSContext *) ctx)->priv->lock);
#else
#define CMS_LOCK(ctx)
#define CMS_UNLOCK(ctx)
#endif

#define d(x)

#define CCC_CLASS(o) CAMEL_CMS_CONTEXT_CLASS(CAMEL_OBJECT_GET_CLASS(o))

struct _CamelCMSContextPrivate {
#ifdef ENABLE_THREADS
	GMutex *lock;
#endif
};

static CamelMimeMessage *cms_sign      (CamelCMSContext *ctx, CamelMimeMessage *message,
					const char *userid, gboolean signing_time,
					gboolean detached, CamelException *ex);

static CamelMimeMessage *cms_certsonly (CamelCMSContext *ctx, CamelMimeMessage *message,
					const char *userid, GPtrArray *recipients,
					CamelException *ex);

static CamelMimeMessage *cms_encrypt   (CamelCMSContext *ctx, CamelMimeMessage *message,
					const char *userid, GPtrArray *recipients, 
					CamelException *ex);

static CamelMimeMessage *cms_envelope  (CamelCMSContext *ctx, CamelMimeMessage *message,
					const char *userid, GPtrArray *recipients, 
					CamelException *ex);

static CamelMimeMessage *cms_decode    (CamelCMSContext *ctx, CamelMimeMessage *message,
					CamelCMSValidityInfo **info, CamelException *ex);

static CamelObjectClass *parent_class;

static void
camel_cms_context_init (CamelCMSContext *context)
{
	context->priv = g_new0 (struct _CamelCMSContextPrivate, 1);
#ifdef ENABLE_THREADS
	context->priv->lock = g_mutex_new ();
#endif
}

static void
camel_cms_context_finalise (CamelObject *o)
{
	CamelCMSContext *context = (CamelCMSContext *)o;
	
	camel_object_unref (context->session);
	
#ifdef ENABLE_THREADS
	g_mutex_free (context->priv->lock);
#endif
	
	g_free (context->priv);
}

static void
camel_cms_context_class_init (CamelCMSContextClass *camel_cms_context_class)
{
	parent_class = camel_type_get_global_classfuncs (camel_object_get_type ());
	
	camel_cms_context_class->sign = cms_sign;
	camel_cms_context_class->certsonly = cms_certsonly;
	camel_cms_context_class->encrypt = cms_encrypt;
	camel_cms_context_class->envelope = cms_envelope;
	camel_cms_context_class->decode = cms_decode;
}

CamelType
camel_cms_context_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_object_get_type (),
					    "CamelCMSContext",
					    sizeof (CamelCMSContext),
					    sizeof (CamelCMSContextClass),
					    (CamelObjectClassInitFunc) camel_cms_context_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_cms_context_init,
					    (CamelObjectFinalizeFunc) camel_cms_context_finalise);
	}
	
	return type;
}


/**
 * camel_cms_context_new:
 * @session: CamelSession
 * @encryption_key: preferred encryption key nickname
 *
 * This creates a new CamelCMSContext object which is used to sign,
 * encrypt, envelope and decode CMS messages.
 *
 * Return value: the new CamelCMSContext
 **/
CamelCMSContext *
camel_cms_context_new (CamelSession *session)
{
	CamelCMSContext *context;
	
	g_return_val_if_fail (session != NULL, NULL);
	g_return_val_if_fail (CAMEL_IS_SESSION (session), NULL);
	
	context = CAMEL_CMS_CONTEXT (camel_object_new (CAMEL_CMS_CONTEXT_TYPE));
	
	camel_object_ref (session);
	context->session = session;
	
	return context;
}


/**
 * camel_cms_context_construct:
 * @context: CMS Context
 * @session: CamelSession
 *
 * Construct the CMS Context.
 **/
void
camel_cms_context_construct (CamelCMSContext *context, CamelSession *session)
{
	g_return_if_fail (CAMEL_IS_CMS_CONTEXT (context));
	g_return_if_fail (CAMEL_IS_SESSION (session));
	
	camel_object_ref (session);
	context->session = session;
}


static CamelMimeMessage *
cms_sign (CamelCMSContext *ctx, CamelMimeMessage *message,
	  const char *userid, gboolean signing_time,
	  gboolean detached, CamelException *ex)
{
	g_warning ("Using default CamelCMSContext::sign() method.");
	
	return NULL;
}


CamelMimeMessage *
camel_cms_sign (CamelCMSContext *ctx, CamelMimeMessage *message,
		const char *userid, gboolean signing_time,
		gboolean detached, CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_CMS_CONTEXT (ctx), NULL);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);
	g_return_val_if_fail (userid != NULL, NULL);
	
	return CCC_CLASS (ctx)->sign (ctx, message, userid, signing_time, detached, ex);
}


static CamelMimeMessage *
cms_certsonly (CamelCMSContext *ctx, CamelMimeMessage *message,
	       const char *userid, GPtrArray *recipients,
	       CamelException *ex)
{
	g_warning ("Using default CamelCMSContext::certsonly() method.");
	
	return NULL;
}


CamelMimeMessage *
camel_cms_certsonly (CamelCMSContext *ctx, CamelMimeMessage *message,
		     const char *userid, GPtrArray *recipients,
		     CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_CMS_CONTEXT (ctx), NULL);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);
	g_return_val_if_fail (userid != NULL, NULL);
	g_return_val_if_fail (recipients != NULL, NULL);
	
	return CCC_CLASS (ctx)->certsonly (ctx, message, userid, recipients, ex);
}


static CamelMimeMessage *
cms_envelope (CamelCMSContext *ctx, CamelMimeMessage *message,
	      const char *userid, GPtrArray *recipients, 
	      CamelException *ex)
{
	g_warning ("Using default CamelCMSContext::envelope() method.");
	
	return NULL;
}


CamelMimeMessage *
camel_cms_envelope (CamelCMSContext *ctx, CamelMimeMessage *message,
		    const char *userid, GPtrArray *recipients, 
		    CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_CMS_CONTEXT (ctx), NULL);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);
	g_return_val_if_fail (userid != NULL, NULL);
	g_return_val_if_fail (recipients != NULL, NULL);
	
	return CCC_CLASS (ctx)->envelope (ctx, message, userid, recipients, ex);
}


static CamelMimeMessage *
cms_encrypt (CamelCMSContext *ctx, CamelMimeMessage *message,
	     const char *userid, GPtrArray *recipients, 
	     CamelException *ex)
{
	g_warning ("Using default CamelCMSContext::encrypt() method.");
	
	return NULL;
}


CamelMimeMessage *
camel_cms_encrypt (CamelCMSContext *ctx, CamelMimeMessage *message,
		   const char *userid, GPtrArray *recipients, 
		   CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_CMS_CONTEXT (ctx), NULL);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);
	g_return_val_if_fail (userid != NULL, NULL);
	g_return_val_if_fail (recipients != NULL, NULL);
	
	return CCC_CLASS (ctx)->encrypt (ctx, message, userid, recipients, ex);
}


static CamelMimeMessage *
cms_decode (CamelCMSContext *ctx, CamelMimeMessage *message,
	    CamelCMSValidityInfo **info, CamelException *ex)
{
	g_warning ("Using default CamelCMSContext::decode() method.");
	
	return NULL;
}


CamelMimeMessage *
camel_cms_decode (CamelCMSContext *ctx, CamelMimeMessage *message,
		  CamelCMSValidityInfo **info, CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_CMS_CONTEXT (ctx), NULL);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);
	
	return CCC_CLASS (ctx)->decode (ctx, message, info, ex);
}


void
camel_cms_signer_free (CamelCMSSigner *signer)
{
	CamelCMSSigner *next;
	
	if (!signer)
		return;
	
	while (signer) {
		next = signer->next;
		g_free (signer->signercn);
		g_free (signer->status);
		g_free (signer);
		signer = next;
	}
}


void
camel_cms_validity_info_free (CamelCMSValidityInfo *info)
{
	CamelCMSValidityInfo *next;
	
	if (!info)
		return;
	
	while (info) {
		next = info->next;
		if (info->type == CAMEL_CMS_TYPE_SIGNED)
			camel_cms_signer_free (info->signers);
		g_free (info);
		info = next;
	}
}
