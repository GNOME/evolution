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

#include <stdlib.h>
#include <string.h>

#include "mail-crypto.h"
#include "mail-session.h"
#include "mail-config.h"


/**
 * mail_crypto_get_pgp_cipher_context:
 * @account: Account that will be using this context
 *
 * Constructs a new GnuPG cipher context with the appropriate
 * options set based on the account provided.
 **/
CamelCipherContext *
mail_crypto_get_pgp_cipher_context (const MailConfigAccount *account)
{
	CamelCipherContext *cipher;
	
	cipher = camel_gpg_context_new (session);
	if (account)
		camel_gpg_context_set_always_trust ((CamelGpgContext *) cipher, account->pgp_always_trust);
	
	return cipher;
}


/**
 * mail_crypto_smime_sign:
 * @message: MIME message to sign
 * @userid: userid to sign with
 * @signing_time: Include signing time
 * @detached: create detached signature
 * @ex: exception which will be set if there are any errors.
 *
 * Returns a S/MIME message in compliance with rfc2633. Returns %NULL
 * on failure and @ex will be set.
 **/
CamelMimeMessage *
mail_crypto_smime_sign (CamelMimeMessage *message, const char *userid,
			gboolean signing_time, gboolean detached,
			CamelException *ex)
{
	CamelSMimeContext *context = NULL;
	CamelMimeMessage *mesg = NULL;
	
#ifdef HAVE_NSS
	context = camel_smime_context_new (session, NULL);
#endif
	
	if (context) {
		mesg = camel_cms_sign (CAMEL_CMS_CONTEXT (context), message,
				       userid, signing_time, detached, ex);
		camel_object_unref (context);
	} else
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not create a S/MIME signature context."));
	
	return mesg;
}


/**
 * mail_crypto_smime_certsonly:
 * @message: MIME message
 * @userid: userid
 * @recipients: recipients
 * @ex: exception
 *
 * Returns a S/MIME message.
 **/
CamelMimeMessage *
mail_crypto_smime_certsonly (CamelMimeMessage *message, const char *userid,
			     GPtrArray *recipients, CamelException *ex)
{
	CamelSMimeContext *context = NULL;
	CamelMimeMessage *mesg = NULL;
	
#ifdef HAVE_NSS
	context = camel_smime_context_new (session, NULL);
#endif
	
	if (context) {
		mesg = camel_cms_certsonly (CAMEL_CMS_CONTEXT (context), message,
					    userid, recipients, ex);
		camel_object_unref (context);
	} else
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not create a S/MIME certsonly context."));
	
	return mesg;
}


/**
 * mail_crypto_smime_encrypt:
 * @message: MIME message
 * @userid: userid
 * @recipients: recipients
 * @ex: exception
 *
 * Returns a S/MIME message.
 **/
CamelMimeMessage *
mail_crypto_smime_encrypt (CamelMimeMessage *message, const char *userid,
			   GPtrArray *recipients, CamelException *ex)
{
	CamelSMimeContext *context = NULL;
	CamelMimeMessage *mesg = NULL;
	
#ifdef HAVE_NSS
	context = camel_smime_context_new (session, NULL);
#endif
	
	if (context) {
		mesg = camel_cms_encrypt (CAMEL_CMS_CONTEXT (context), message,
					  userid, recipients, ex);
		camel_object_unref (context);
	} else
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not create a S/MIME encryption context."));
	
	return mesg;
}


/**
 * mail_crypto_smime_envelope:
 * @message: MIME message
 * @userid: userid
 * @recipients: recipients
 * @ex: exception
 *
 * Returns a S/MIME message.
 **/
CamelMimeMessage *
mail_crypto_smime_envelope (CamelMimeMessage *message, const char *userid,
			    GPtrArray *recipients, CamelException *ex)
{
	CamelSMimeContext *context = NULL;
	CamelMimeMessage *mesg = NULL;
	
#ifdef HAVE_NSS
	context = camel_smime_context_new (session, NULL);
#endif
	
	if (context) {
		mesg = camel_cms_envelope (CAMEL_CMS_CONTEXT (context), message,
					   userid, recipients, ex);
		camel_object_unref (context);
	} else
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not create a S/MIME envelope context."));
	
	return mesg;
}


/**
 * mail_crypto_smime_decode:
 * @message: MIME message
 * @info: pointer to a CamelCMSValidityInfo structure (or %NULL)
 * @ex: exception
 *
 * Returns a decoded S/MIME message.
 **/
CamelMimeMessage *
mail_crypto_smime_decode (CamelMimeMessage *message, CamelCMSValidityInfo **info,
			  CamelException *ex)
{
	CamelSMimeContext *context = NULL;
	CamelMimeMessage *mesg = NULL;
	
#ifdef HAVE_NSS
	context = camel_smime_context_new (session, NULL);
#endif
	
	if (context) {
		mesg = camel_cms_decode (CAMEL_CMS_CONTEXT (context),
					 message, info, ex);
		camel_object_unref (context);
	} else
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not create a S/MIME decode context."));
	
	return mesg;
}
