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

#ifndef CAMEL_CMS_CONTEXT_H
#define CAMEL_CMS_CONTEXT_H

#include <camel/camel-session.h>
#include <camel/camel-stream.h>
#include <camel/camel-exception.h>
#include <camel/camel-mime-message.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define CAMEL_CMS_CONTEXT_TYPE     (camel_cms_context_get_type ())
#define CAMEL_CMS_CONTEXT(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_CMS_CONTEXT_TYPE, CamelCMSContext))
#define CAMEL_CMS_CONTEXT_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_CMS_CONTEXT_TYPE, CamelCMSContextClass))
#define CAMEL_IS_CMS_CONTEXT(o)    (CAMEL_CHECK_TYPE((o), CAMEL_CMS_CONTEXT_TYPE))

typedef enum {
	CAMEL_CMS_TYPE_DATA,
	CAMEL_CMS_TYPE_SIGNED,
	CAMEL_CMS_TYPE_ENVELOPED,
	CAMEL_CMS_TYPE_ENCRYPTED
} CamelCMSType;

typedef struct _CamelCMSSigner {
	struct _CamelCMSSigner *next;
	char *signercn;
	char *status;
} CamelCMSSigner;

typedef struct _CamelCMSValidityInfo {
	struct _CamelCMSValidityInfo *next;
	CamelCMSType type;
	CamelCMSSigner *signers;
} CamelCMSValidityInfo;


typedef struct _CamelCMSContext {
	CamelObject parent_object;
	
	struct _CamelCMSContextPrivate *priv;
	
	CamelSession *session;
} CamelCMSContext;

typedef struct _CamelCMSContextClass {
	CamelObjectClass parent_class;
	
	CamelMimeMessage *(*sign)       (CamelCMSContext *ctx, CamelMimeMessage *message,
					 const char *userid, gboolean signing_time,
					 gboolean detached, CamelException *ex);
	
	CamelMimeMessage *(*certsonly)  (CamelCMSContext *ctx, CamelMimeMessage *message,
					 const char *userid, GPtrArray *recipients,
					 CamelException *ex);
	
	CamelMimeMessage *(*encrypt)    (CamelCMSContext *ctx, CamelMimeMessage *message,
					 const char *userid, GPtrArray *recipients, 
					 CamelException *ex);
	
	CamelMimeMessage *(*envelope)   (CamelCMSContext *ctx, CamelMimeMessage *message,
					 const char *userid, GPtrArray *recipients, 
					 CamelException *ex);
	
	CamelMimeMessage *(*decode)     (CamelCMSContext *ctx, CamelMimeMessage *message,
					 CamelCMSValidityInfo **info, CamelException *ex);
	
} CamelCMSContextClass;

CamelType         camel_cms_context_get_type (void);

CamelCMSContext  *camel_cms_context_new (CamelSession *session);

void              camel_cms_context_construct (CamelCMSContext *context, CamelSession *session);

/* cms routines */
CamelMimeMessage *camel_cms_sign      (CamelCMSContext *ctx, CamelMimeMessage *message,
				       const char *userid, gboolean signing_time,
				       gboolean detached, CamelException *ex);

CamelMimeMessage *camel_cms_certsonly (CamelCMSContext *ctx, CamelMimeMessage *message,
				       const char *userid, GPtrArray *recipients,
				       CamelException *ex);

CamelMimeMessage *camel_cms_encrypt   (CamelCMSContext *ctx, CamelMimeMessage *message,
				       const char *userid, GPtrArray *recipients, 
				       CamelException *ex);

CamelMimeMessage *camel_cms_envelope  (CamelCMSContext *ctx, CamelMimeMessage *message,
				       const char *userid, GPtrArray *recipients, 
				       CamelException *ex);

CamelMimeMessage *camel_cms_decode    (CamelCMSContext *ctx, CamelMimeMessage *message,
				       CamelCMSValidityInfo **info, CamelException *ex);


void              camel_cms_signer_free        (CamelCMSSigner *signer);

void              camel_cms_validity_info_free (CamelCMSValidityInfo *info);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_CMS_CONTEXT_H */
