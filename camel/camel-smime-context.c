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

#ifdef HAVE_NSS
#include "camel-smime-context.h"

#include "camel-mime-filter-from.h"
#include "camel-mime-filter-crlf.h"
#include "camel-stream-filter.h"
#include "camel-stream-fs.h"
#include "camel-stream-mem.h"
#include "camel-mime-part.h"
#include "camel-multipart.h"

#include "nss.h"
#include <cms.h>
#include <cert.h>
#include <certdb.h>
#include <pkcs11.h>
#include <smime.h>

#define d(x)

struct _CamelSMimeContextPrivate {
	CERTCertDBHandle *certdb;
};


static CamelMimeMessage *smime_sign      (CamelCMSContext *ctx, CamelMimeMessage *message,
					  const char *userid, gboolean signing_time,
					  gboolean detached, CamelException *ex);

static CamelMimeMessage *smime_certsonly (CamelCMSContext *ctx, CamelMimeMessage *message,
					  const char *userid, GPtrArray *recipients,
					  CamelException *ex);

static CamelMimeMessage *smime_encrypt   (CamelCMSContext *ctx, CamelMimeMessage *message,
					  const char *userid, GPtrArray *recipients, 
					  CamelException *ex);

static CamelMimeMessage *smime_envelope  (CamelCMSContext *ctx, CamelMimeMessage *message,
					  const char *userid, GPtrArray *recipients, 
					  CamelException *ex);

static CamelMimeMessage *smime_decode    (CamelCMSContext *ctx, CamelMimeMessage *message,
					  CamelCMSValidityInfo **info, CamelException *ex);

static CamelCMSContextClass *parent_class;

static void
camel_smime_context_init (CamelSMimeContext *context)
{
	context->priv = g_new0 (struct _CamelSMimeContextPrivate, 1);
}

static void
camel_smime_context_finalise (CamelObject *o)
{
	CamelSMimeContext *context = (CamelSMimeContext *)o;
	
	g_free (context->encryption_key);
	g_free (context->priv);
}

static void
camel_smime_context_class_init (CamelSMimeContextClass *camel_smime_context_class)
{
	CamelCMSContextClass *camel_cms_context_class =
		CAMEL_CMS_CONTEXT_CLASS (camel_smime_context_class);
	
	parent_class = CAMEL_CMS_CONTEXT_CLASS (camel_type_get_global_classfuncs (camel_cms_context_get_type ()));
	
	camel_cms_context_class->sign = smime_sign;
	camel_cms_context_class->certsonly = smime_certsonly;
	camel_cms_context_class->encrypt = smime_encrypt;
	camel_cms_context_class->envelope = smime_envelope;
	camel_cms_context_class->decode = smime_decode;
}

CamelType
camel_smime_context_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_cms_context_get_type (),
					    "CamelSMimeContext",
					    sizeof (CamelSMimeContext),
					    sizeof (CamelSMimeContextClass),
					    (CamelObjectClassInitFunc) camel_smime_context_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_smime_context_init,
					    (CamelObjectFinalizeFunc) camel_smime_context_finalise);
	}
	
	return type;
}


/**
 * camel_smime_context_new:
 * @session: CamelSession
 * @encryption_key: preferred encryption key (used when attaching cert chains to messages)
 *
 * This creates a new CamelSMimeContext object which is used to sign,
 * verify, encrypt and decrypt streams.
 *
 * Return value: the new CamelSMimeContext
 **/
CamelSMimeContext *
camel_smime_context_new (CamelSession *session, const char *encryption_key)
{
	CamelSMimeContext *context;
	CERTCertDBHandle *certdb;
	
	g_return_val_if_fail (session != NULL, NULL);
	g_return_val_if_fail (CAMEL_IS_SESSION (session), NULL);
	
	certdb = CERT_GetDefaultCertDB ();
	if (!certdb)
		return NULL;
	
	context = CAMEL_SMIME_CONTEXT (camel_object_new (CAMEL_SMIME_CONTEXT_TYPE));
	
	camel_cms_context_construct (CAMEL_CMS_CONTEXT (context), session);
	
	context->encryption_key = g_strdup (encryption_key);
	context->priv->certdb = certdb;
	
	return context;
}


struct _GetPasswdData {
	CamelSession *session;
	const char *userid;
	CamelException *ex;
};

static char *
smime_get_password (PK11SlotInfo *info, PRBool retry, void *arg)
{
	CamelSession *session = ((struct _GetPasswdData *)arg)->session;
	const char *userid = ((struct _GetPasswdData *)arg)->userid;
	CamelException *ex = ((struct _GetPasswdData *)arg)->ex;
	char *prompt, *passwd, *ret;
	
	prompt = g_strdup_printf (_("Please enter your password for %s"), userid);
	passwd = camel_session_get_password (session, prompt, FALSE, TRUE,
					     NULL, userid, ex);
	g_free (prompt);
	
	ret = PL_strdup (passwd);
	g_free (passwd);
	
	return ret;
}

static PK11SymKey *
decode_key_cb (void *arg, SECAlgorithmID *algid)
{
	return (PK11SymKey *)arg;
}


static NSSCMSMessage *
signed_data (CamelSMimeContext *ctx, const char *userid, gboolean signing_time,
	     gboolean detached, CamelException *ex)
{
	NSSCMSMessage *cmsg = NULL;
	NSSCMSContentInfo *cinfo;
	NSSCMSSignedData *sigd;
	NSSCMSSignerInfo *signerinfo;
	CERTCertificate *cert, *ekpcert;
	
	if (!userid) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Please indicate the nickname of a certificate to sign with."));
		return NULL;
	}
	
	if ((cert = CERT_FindCertByNickname (ctx->priv->certdb, (char *) userid)) == NULL) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("The signature certificate for \"%s\" does not exist."),
				      userid);
		return NULL;
	}
	
	/* create the cms message object */
	cmsg = NSS_CMSMessage_Create (NULL);
	
	/* build chain of objects: message->signedData->data */
	sigd = NSS_CMSSignedData_Create (cmsg);
	
	cinfo = NSS_CMSMessage_GetContentInfo (cmsg);
	NSS_CMSContentInfo_SetContent_SignedData (cmsg, cinfo, sigd); 
	
	cinfo = NSS_CMSSignedData_GetContentInfo (sigd);
	
	/* speciffy whether we want detached signatures or not */
	NSS_CMSContentInfo_SetContent_Data (cmsg, cinfo, NULL, detached);
	
	/* create & attach signer information */
	signerinfo = NSS_CMSSignerInfo_Create (cmsg, cert, SEC_OID_SHA1);
	
	/* include the cert chain */
	NSS_CMSSignerInfo_IncludeCerts (signerinfo, NSSCMSCM_CertChain, 
					certUsageEmailSigner);
	
	if (signing_time) {
		NSS_CMSSignerInfo_AddSigningTime (signerinfo, PR_Now ());
	}
	
	if (TRUE) {
		/* Add S/MIME Capabilities */
		NSS_CMSSignerInfo_AddSMIMECaps (signerinfo);
	}
	
	if (ctx->encryption_key) {
		/* get the cert, add it to the message */
		ekpcert = CERT_FindCertByNickname (ctx->priv->certdb, ctx->encryption_key);
		if (!ekpcert) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("The encryption certificate for \"%s\" does not exist."),
					      ctx->encryption_key);
			goto exception;
		}
		
		NSS_CMSSignerInfo_AddSMIMEEncKeyPrefs (signerinfo, ekpcert, ctx->priv->certdb);
		
		NSS_CMSSignedData_AddCertificate (sigd, ekpcert);
	} else {
		/* check signing cert for fitness as encryption cert */
		/* if yes, add signing cert as EncryptionKeyPreference */
		NSS_CMSSignerInfo_AddSMIMEEncKeyPrefs (signerinfo, cert, ctx->priv->certdb);
	}
	
	NSS_CMSSignedData_AddSignerInfo (sigd, signerinfo);
	
	return cmsg;
	
 exception:
	
	NSS_CMSMessage_Destroy (cmsg);
	
	return NULL;
}

static void
smime_sign_restore (CamelMimePart *mime_part, GSList **encodings)
{
	CamelDataWrapper *wrapper;
	
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
	if (!wrapper)
		return;
	
	if (CAMEL_IS_MULTIPART (wrapper)) {
		int parts, i;
		
		parts = camel_multipart_get_number (CAMEL_MULTIPART (wrapper));
		for (i = 0; i < parts; i++) {
			CamelMimePart *part = camel_multipart_get_part (CAMEL_MULTIPART (wrapper), i);
			
			smime_sign_restore (part, encodings);
			*encodings = (*encodings)->next;
		}
	} else {
		CamelTransferEncoding encoding;
		
		if (CAMEL_IS_MIME_MESSAGE (wrapper)) {
			/* restore the message parts' subparts */
			smime_sign_restore (CAMEL_MIME_PART (wrapper), encodings);
		} else {
			encoding = GPOINTER_TO_INT ((*encodings)->data);
			
			camel_mime_part_set_encoding (mime_part, encoding);
		}
	}
}

static void
smime_sign_prepare (CamelMimePart *mime_part, GSList **encodings)
{
	CamelDataWrapper *wrapper;
	int parts, i;
	
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
	if (!wrapper)
		return;
	
	if (CAMEL_IS_MULTIPART (wrapper)) {
		parts = camel_multipart_get_number (CAMEL_MULTIPART (wrapper));
		for (i = 0; i < parts; i++) {
			CamelMimePart *part = camel_multipart_get_part (CAMEL_MULTIPART (wrapper), i);
			
			smime_sign_prepare (part, encodings);
		}
	} else {
		CamelTransferEncoding encoding;
		
		if (CAMEL_IS_MIME_MESSAGE (wrapper)) {
			/* prepare the message parts' subparts */
			smime_sign_prepare (CAMEL_MIME_PART (wrapper), encodings);
		} else {
			encoding = camel_mime_part_get_encoding (mime_part);
			
			/* FIXME: find the best encoding for this part and use that instead?? */
			/* the encoding should really be QP or Base64 */
			if (encoding != CAMEL_TRANSFER_ENCODING_BASE64)
				camel_mime_part_set_encoding (mime_part, CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE);
			
			*encodings = g_slist_append (*encodings, GINT_TO_POINTER (encoding));
		}
	}
}


static CamelMimeMessage *
smime_sign (CamelCMSContext *ctx, CamelMimeMessage *message,
	    const char *userid, gboolean signing_time,
	    gboolean detached, CamelException *ex)
{
	CamelMimeMessage *mesg = NULL;
	NSSCMSMessage *cmsg = NULL;
	struct _GetPasswdData *data;
	PLArenaPool *arena;
	NSSCMSEncoderContext *ecx;
	SECItem output = { 0, 0, 0 };
	CamelStream *stream;
	GSList *list, *encodings = NULL;
	GByteArray *buf;
	
	cmsg = signed_data (CAMEL_SMIME_CONTEXT (ctx), userid, signing_time, detached, ex);
	if (!cmsg)
		return NULL;
	
	arena = PORT_NewArena (1024);
	data = g_new (struct _GetPasswdData, 1);
	data->session = ctx->session;
	data->userid = userid;
	data->ex = ex;
	ecx = NSS_CMSEncoder_Start (cmsg, NULL, NULL, &output, arena,
				    smime_get_password, data, NULL, NULL,
				    NULL, NULL);
	
	stream = camel_stream_mem_new ();
	
	smime_sign_prepare (CAMEL_MIME_PART (message), &encodings);
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), stream);
	list = encodings;
	smime_sign_restore (CAMEL_MIME_PART (message), &list);
	g_slist_free (encodings);
	
	buf = CAMEL_STREAM_MEM (stream)->buffer;
	
	NSS_CMSEncoder_Update (ecx, buf->data, buf->len);
	NSS_CMSEncoder_Finish (ecx);
	
	camel_object_unref (CAMEL_OBJECT (stream));
	g_free (data);
	
	/* write the result to a camel stream */
	stream = camel_stream_mem_new ();
	camel_stream_write (stream, output.data, output.len);
	PORT_FreeArena (arena, PR_FALSE);
	
	NSS_CMSMessage_Destroy (cmsg);
	
	/* parse the stream into a new CamelMimeMessage */
	mesg = camel_mime_message_new ();
	camel_stream_reset (stream);
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (mesg), stream);
	camel_object_unref (CAMEL_OBJECT (stream));
	
	return mesg;
}


static NSSCMSMessage *
certsonly_data (CamelSMimeContext *ctx, const char *userid, GPtrArray *recipients, CamelException *ex)
{
	NSSCMSMessage *cmsg = NULL;
	NSSCMSContentInfo *cinfo;
	NSSCMSSignedData *sigd;
	CERTCertificate **rcerts;
	int i = 0;
	
	/* find the signer's and the recipients' certs */
	rcerts = g_new (CERTCertificate *, recipients->len + 2);
	rcerts[0] = CERT_FindCertByNicknameOrEmailAddr (ctx->priv->certdb, (char *) userid);
	if (!rcerts[0]) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to find certificate for \"%s\"."),
				      recipients->pdata[i]);
		goto exception;
	}
	
	for (i = 0; i < recipients->len; i++) {
		rcerts[i + 1] = CERT_FindCertByNicknameOrEmailAddr (ctx->priv->certdb,
								    recipients->pdata[i]);
		
		if (!rcerts[i + 1]) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Failed to find certificate for \"%s\"."),
					      recipients->pdata[i]);
			goto exception;
		}
	}
	rcerts[i + 1] = NULL;
	
	/* create the cms message object */
	cmsg = NSS_CMSMessage_Create (NULL);
	
	sigd = NSS_CMSSignedData_CreateCertsOnly (cmsg, rcerts[0], PR_TRUE);
	
	/* add the recipient cert chain */
	for (i = 0; i < recipients->len; i++) {
		NSS_CMSSignedData_AddCertChain (sigd, rcerts[i]);
	}
	
	cinfo = NSS_CMSMessage_GetContentInfo (cmsg);
	NSS_CMSContentInfo_SetContent_SignedData (cmsg, cinfo, sigd);
	
	cinfo = NSS_CMSSignedData_GetContentInfo (sigd);
	NSS_CMSContentInfo_SetContent_Data (cmsg, cinfo, NULL, PR_FALSE);
	
	g_free (rcerts);
	
	return cmsg;
	
 exception:
	
	NSS_CMSMessage_Destroy (cmsg);
	
	g_free (rcerts);
	
	return NULL;
}

static CamelMimeMessage *
smime_certsonly (CamelCMSContext *ctx, CamelMimeMessage *message,
		 const char *userid, GPtrArray *recipients,
		 CamelException *ex)
{
	CamelMimeMessage *mesg = NULL;
	struct _GetPasswdData *data;
	NSSCMSMessage *cmsg = NULL;
	PLArenaPool *arena;
	NSSCMSEncoderContext *ecx;
	SECItem output = { 0, 0, 0 };
	CamelStream *stream;
	GByteArray *buf;
	
	cmsg = certsonly_data (CAMEL_SMIME_CONTEXT (ctx), userid, recipients, ex);
	if (!cmsg)
		return NULL;
	
	arena = PORT_NewArena (1024);
	data = g_new (struct _GetPasswdData, 1);
	data->session = ctx->session;
	data->userid = userid;
	data->ex = ex;
	ecx = NSS_CMSEncoder_Start (cmsg, NULL, NULL, &output, arena,
				    smime_get_password, data, NULL, NULL,
				    NULL, NULL);
	
	stream = camel_stream_mem_new ();
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), stream);
	buf = CAMEL_STREAM_MEM (stream)->buffer;
	
	NSS_CMSEncoder_Update (ecx, buf->data, buf->len);
	NSS_CMSEncoder_Finish (ecx);
	
	camel_object_unref (CAMEL_OBJECT (stream));
	g_free (data);
	
	/* write the result to a camel stream */
	stream = camel_stream_mem_new ();
	camel_stream_write (stream, output.data, output.len);
	PORT_FreeArena (arena, PR_FALSE);
	
	NSS_CMSMessage_Destroy (cmsg);
	
	/* parse the stream into a new CamelMimeMessage */
	mesg = camel_mime_message_new ();
	camel_stream_reset (stream);
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (mesg), stream);
	camel_object_unref (CAMEL_OBJECT (stream));
	
	return mesg;
}


static NSSCMSMessage *
enveloped_data (CamelSMimeContext *ctx, const char *userid, GPtrArray *recipients, CamelException *ex)
{
	NSSCMSMessage *cmsg = NULL;
	NSSCMSContentInfo *cinfo;
	NSSCMSEnvelopedData *envd;
	NSSCMSRecipientInfo *rinfo;
	CERTCertificate **rcerts;
	SECOidTag bulkalgtag;
	int keysize, i;
	
	/* find the recipient certs by email address or nickname */
	rcerts = g_new (CERTCertificate *, recipients->len + 2);
	rcerts[0] = CERT_FindCertByNicknameOrEmailAddr (ctx->priv->certdb, (char *) userid);
	if (!rcerts[0]) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to find certificate for \"%s\"."),
				      userid);
		goto exception;
	}
	
	for (i = 0; i < recipients->len; i++) {
		rcerts[i + 1] = CERT_FindCertByNicknameOrEmailAddr (ctx->priv->certdb,  
								    recipients->pdata[i]);
		if (!rcerts[i + 1]) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Failed to find certificate for \"%s\"."),
					      recipients->pdata[i]);
			goto exception;
		}
	}
	rcerts[i + 1] = NULL;
	
	/* find a nice bulk algorithm */
	if (!NSS_SMIMEUtil_FindBulkAlgForRecipients (rcerts, &bulkalgtag, &keysize)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Failed to find a common bulk algorithm."));
		goto exception;
	}
	
	/* create a cms message object */
	cmsg = NSS_CMSMessage_Create (NULL);
	
	envd = NSS_CMSEnvelopedData_Create (cmsg, bulkalgtag, keysize);
	cinfo = NSS_CMSMessage_GetContentInfo (cmsg);
	NSS_CMSContentInfo_SetContent_EnvelopedData (cmsg, cinfo, envd);
	
	cinfo = NSS_CMSEnvelopedData_GetContentInfo (envd);
	NSS_CMSContentInfo_SetContent_Data (cmsg, cinfo, NULL, PR_FALSE);
	
	/* create & attach recipient information */
	for (i = 0; rcerts[i] != NULL; i++) {
		rinfo = NSS_CMSRecipientInfo_Create (cmsg, rcerts[i]);
		NSS_CMSEnvelopedData_AddRecipient (envd, rinfo);
	}
	
	g_free (rcerts);
	
	return cmsg;
	
 exception:
	
	NSS_CMSMessage_Destroy (cmsg);
	
	g_free (rcerts);
	
	return NULL;
}

static CamelMimeMessage *
smime_envelope (CamelCMSContext *ctx, CamelMimeMessage *message,
		const char *userid, GPtrArray *recipients, 
		CamelException *ex)
{
	CamelMimeMessage *mesg = NULL;
	struct _GetPasswdData *data;
	NSSCMSMessage *cmsg = NULL;
	PLArenaPool *arena;
	NSSCMSEncoderContext *ecx;
	SECItem output = { 0, 0, 0 };
	CamelStream *stream;
	GByteArray *buf;
	
	cmsg = enveloped_data (CAMEL_SMIME_CONTEXT (ctx), userid, recipients, ex);
	if (!cmsg)
		return NULL;
	
	arena = PORT_NewArena (1024);
	data = g_new (struct _GetPasswdData, 1);
	data->session = ctx->session;
	data->userid = userid;
	data->ex = ex;
	ecx = NSS_CMSEncoder_Start (cmsg, NULL, NULL, &output, arena,
				    smime_get_password, data, NULL, NULL,
				    NULL, NULL);
	
	stream = camel_stream_mem_new ();
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), stream);
	buf = CAMEL_STREAM_MEM (stream)->buffer;
	
	NSS_CMSEncoder_Update (ecx, buf->data, buf->len);
	NSS_CMSEncoder_Finish (ecx);
	
	camel_object_unref (CAMEL_OBJECT (stream));
	g_free (data);
	
	/* write the result to a camel stream */
	stream = camel_stream_mem_new ();
	camel_stream_write (stream, output.data, output.len);
	PORT_FreeArena (arena, PR_FALSE);
	
	NSS_CMSMessage_Destroy (cmsg);
	
	/* parse the stream into a new CamelMimeMessage */
	mesg = camel_mime_message_new ();
	camel_stream_reset (stream);
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (mesg), stream);
	camel_object_unref (CAMEL_OBJECT (stream));
	
	return mesg;
}


struct _BulkKey {
	PK11SymKey *bulkkey;
	SECOidTag bulkalgtag;
	int keysize;
};

static NSSCMSMessage *
encrypted_data (CamelSMimeContext *ctx, GByteArray *input, struct _BulkKey *key,
		CamelStream *ostream, CamelException *ex)
{
	NSSCMSMessage *cmsg = NULL;
	NSSCMSContentInfo *cinfo;
	NSSCMSEncryptedData *encd;
	NSSCMSEncoderContext *ecx = NULL;
	PLArenaPool *arena = NULL;
	SECItem output = { 0, 0, 0 };
	
	/* arena for output */
	arena = PORT_NewArena (1024);
	
	/* create cms message object */
	cmsg = NSS_CMSMessage_Create (NULL);
	
	encd = NSS_CMSEncryptedData_Create (cmsg, key->bulkalgtag, key->keysize);
	
	cinfo = NSS_CMSMessage_GetContentInfo (cmsg);
	NSS_CMSContentInfo_SetContent_EncryptedData (cmsg, cinfo, encd);
	
	cinfo = NSS_CMSEncryptedData_GetContentInfo (encd);
	NSS_CMSContentInfo_SetContent_Data (cmsg, cinfo, NULL, PR_FALSE);
	
	ecx = NSS_CMSEncoder_Start (cmsg, NULL, NULL, &output, arena, NULL, NULL,
				    decode_key_cb, key->bulkkey, NULL, NULL);
	
	NSS_CMSEncoder_Update (ecx, input->data, input->len);
	
	NSS_CMSEncoder_Finish (ecx);
	
	camel_stream_write (ostream, output.data, output.len);
	
	if (arena)
		PORT_FreeArena (arena, PR_FALSE);
	
	return cmsg;
}

static struct _BulkKey *
get_bulkkey (CamelSMimeContext *ctx, const char *userid, GPtrArray *recipients, CamelException *ex)
{
	struct _BulkKey *bulkkey = NULL;
	NSSCMSMessage *env_cmsg;
	NSSCMSContentInfo *cinfo;
	SECItem dummyOut = { 0, 0, 0 };
	SECItem dummyIn  = { 0, 0, 0 };
	char str[] = "You are not a beautiful and unique snowflake.";
	PLArenaPool *arena;
	int i, nlevels;
	
	/* construct an enveloped data message to obtain bulk keys */
	arena = PORT_NewArena (1024);
	dummyIn.data = (unsigned char *)str;
	dummyIn.len = strlen (str);
	
	env_cmsg = enveloped_data (ctx, userid, recipients, ex);
	NSS_CMSDEREncode (env_cmsg, &dummyIn, &dummyOut, arena);
	/*camel_stream_write (envstream, dummyOut.data, dummyOut.len);*/
	PORT_FreeArena (arena, PR_FALSE);
	
	/* get the content info for the enveloped data */
	nlevels = NSS_CMSMessage_ContentLevelCount (env_cmsg);
	for (i = 0; i < nlevels; i++) {
		SECOidTag typetag;
		
		cinfo = NSS_CMSMessage_ContentLevel (env_cmsg, i);
		typetag = NSS_CMSContentInfo_GetContentTypeTag (cinfo);
		if (typetag == SEC_OID_PKCS7_DATA) {
			bulkkey = g_new (struct _BulkKey, 1);
			
			/* get the symmertic key */
			bulkkey->bulkalgtag = NSS_CMSContentInfo_GetContentEncAlgTag (cinfo);
			bulkkey->keysize = NSS_CMSContentInfo_GetBulkKeySize (cinfo);
			bulkkey->bulkkey = NSS_CMSContentInfo_GetBulkKey (cinfo);
			
			return bulkkey;
		}
	}
	
	return NULL;
}

static CamelMimeMessage *
smime_encrypt (CamelCMSContext *ctx, CamelMimeMessage *message,
	       const char *userid, GPtrArray *recipients, 
	       CamelException *ex)
{
	struct _BulkKey *bulkkey = NULL;
	CamelMimeMessage *mesg = NULL;
	NSSCMSMessage *cmsg = NULL;
	CamelStream *stream;
	GByteArray *buf;
	
	bulkkey = get_bulkkey (CAMEL_SMIME_CONTEXT (ctx), userid, recipients, ex);
	if (!bulkkey)
		return NULL;
	
	buf = g_byte_array_new ();
	stream = camel_stream_mem_new ();
	camel_stream_mem_set_byte_array (CAMEL_STREAM_MEM (stream), buf);
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), stream);
	camel_object_unref (CAMEL_OBJECT (stream));
	
	stream = camel_stream_mem_new ();
	cmsg = encrypted_data (CAMEL_SMIME_CONTEXT (ctx), buf, bulkkey, stream, ex);
	g_byte_array_free (buf, TRUE);
	g_free (bulkkey);
	if (!cmsg) {
		camel_object_unref (CAMEL_OBJECT (stream));
		return NULL;
	}
	
	NSS_CMSMessage_Destroy (cmsg);
	
	/* parse the stream into a new CamelMimeMessage */
	mesg = camel_mime_message_new ();
	camel_stream_reset (stream);
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (mesg), stream);
	camel_object_unref (CAMEL_OBJECT (stream));
	
	return mesg;
}


static NSSCMSMessage *
decode_data (CamelSMimeContext *ctx, GByteArray *input, CamelStream *ostream,
	     CamelCMSValidityInfo **info, CamelException *ex)
{
	NSSCMSDecoderContext *dcx;
	struct _GetPasswdData *data;
	CamelCMSValidityInfo *vinfo = NULL;
	NSSCMSMessage *cmsg = NULL;
	NSSCMSContentInfo *cinfo;
	NSSCMSSignedData *sigd = NULL;
	NSSCMSEnvelopedData *envd;
	NSSCMSEncryptedData *encd;
	int nlevels, i, nsigners, j;
	char *signercn;
	NSSCMSSignerInfo *si;
	SECOidTag typetag;
	SECItem *item;
	
	data = g_new (struct _GetPasswdData, 1);
	data->session = CAMEL_CMS_CONTEXT (ctx)->session;
	data->userid = NULL;
	data->ex = ex;
	
	dcx = NSS_CMSDecoder_Start (NULL,
				    NULL, NULL,
				    smime_get_password, data,
				    decode_key_cb,
				    NULL);
	
	NSS_CMSDecoder_Update (dcx, input->data, input->len);
	
	cmsg = NSS_CMSDecoder_Finish (dcx);
	g_free (data);
	if (cmsg == NULL) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Failed to decode message."));
		return NULL;
	}
	
	nlevels = NSS_CMSMessage_ContentLevelCount (cmsg);
	for (i = 0; i < nlevels; i++) {
		CamelCMSSigner *signers = NULL;
		
		cinfo = NSS_CMSMessage_ContentLevel (cmsg, i);
		typetag = NSS_CMSContentInfo_GetContentTypeTag (cinfo);
		
		if (info && !vinfo) {
			vinfo = g_new0 (CamelCMSValidityInfo, 1);
			*info = vinfo;
		} else if (vinfo) {
			vinfo->next = g_new0 (CamelCMSValidityInfo, 1);
			vinfo = vinfo->next;
		}
		
		switch (typetag) {
		case SEC_OID_PKCS7_SIGNED_DATA:
			if (vinfo)
				vinfo->type = CAMEL_CMS_TYPE_SIGNED;
			
			sigd = (NSSCMSSignedData *)NSS_CMSContentInfo_GetContent (cinfo);
			
			/* import the certificates */
			NSS_CMSSignedData_ImportCerts (sigd, ctx->priv->certdb,
						       certUsageEmailSigner, PR_FALSE);
			
			/* find out about signers */
			nsigners = NSS_CMSSignedData_SignerInfoCount (sigd);
			
			if (nsigners == 0) {
				/* must be a cert transport message */
				SECStatus retval;
				
				/* XXX workaround for bug #54014 */
				NSS_CMSSignedData_ImportCerts (sigd, ctx->priv->certdb, 
							       certUsageEmailSigner, PR_TRUE);
				
				retval = NSS_CMSSignedData_VerifyCertsOnly (sigd, ctx->priv->certdb, 
									    certUsageEmailSigner);
				if (retval != SECSuccess) {
					camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
							     _("Failed to verify certificates."));
					goto exception;
				}
				
				return cmsg;
			}
			
			for (j = 0; vinfo && j < nsigners; j++) {
				if (!signers) {
					signers = g_new0 (CamelCMSSigner, 1);
					vinfo->signers = signers;
				} else {
					signers->next = g_new0 (CamelCMSSigner, 1);
					signers = signers->next;
				}
				
				si = NSS_CMSSignedData_GetSignerInfo (sigd, j);
				signercn = NSS_CMSSignerInfo_GetSignerCommonName (si);
				if (signercn == NULL)
					signercn = "";
				
				NSS_CMSSignedData_VerifySignerInfo (sigd, j, ctx->priv->certdb, 
								    certUsageEmailSigner);
				
				if (signers) {
					signers->signercn = g_strdup (signercn);
					signers->status = g_strdup (
						NSS_CMSUtil_VerificationStatusToString (
							NSS_CMSSignerInfo_GetVerificationStatus (si)));
				}
			}
			break;
		case SEC_OID_PKCS7_ENVELOPED_DATA:
			if (vinfo)
				vinfo->type = CAMEL_CMS_TYPE_ENVELOPED;
			
			envd = (NSSCMSEnvelopedData *)NSS_CMSContentInfo_GetContent (cinfo);
			break;
		case SEC_OID_PKCS7_ENCRYPTED_DATA:
			if (vinfo)
				vinfo->type = CAMEL_CMS_TYPE_ENCRYPTED;
			
			encd = (NSSCMSEncryptedData *)NSS_CMSContentInfo_GetContent (cinfo);
			break;
		case SEC_OID_PKCS7_DATA:
			break;
		default:
			break;
		}
	}
	
	item = NSS_CMSMessage_GetContent (cmsg);
	camel_stream_write (ostream, item->data, item->len);
	
	return cmsg;
	
 exception:
	
	if (info)
		camel_cms_validity_info_free (*info);
	
	if (cmsg)
		NSS_CMSMessage_Destroy (cmsg);
	
	return NULL;
}


static CamelMimeMessage *
smime_decode (CamelCMSContext *ctx, CamelMimeMessage *message,
	      CamelCMSValidityInfo **info, CamelException *ex)
{
	CamelMimeMessage *mesg = NULL;
	NSSCMSMessage *cmsg = NULL;
	CamelStream *stream, *ostream;
	GByteArray *buf;
	
	stream = camel_stream_mem_new ();
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), stream);
	buf = CAMEL_STREAM_MEM (stream)->buffer;
	
	ostream = camel_stream_mem_new ();
	cmsg = decode_data (CAMEL_SMIME_CONTEXT (ctx), buf, ostream, info, ex);
	camel_object_unref (CAMEL_OBJECT (stream));
	if (!cmsg) {
		camel_object_unref (CAMEL_OBJECT (ostream));
		return NULL;
	}
	
	/* construct a new mime message from the stream */
	mesg = camel_mime_message_new ();
	camel_stream_reset (ostream);
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (mesg), ostream);
	camel_object_unref (CAMEL_OBJECT (ostream));
	
	return mesg;
}

#if 0

/* Ugh, so smime context inherets from cms context, not cipher context
   this needs to be fixed ... */

/* this has a 1:1 relationship to CamelCipherHash */
static char **name_table[] = {
	"sha1",		/* we use sha1 as the 'default' */
	NULL,
	"md5",
	"sha1",
	NULL,
};

static const char *smime_hash_to_id(CamelCipherContext *context, CamelCipherHash hash)
{
	/* if we dont know, just use default? */
	if (hash > sizeof(name_table)/sizeof(name_table[0])
	    || name_table[hash] == NULL;
		hash = CAMEL_CIPHER_HASH_DEFAULT;

	return name_table[hash];
}

static CamelCipherHash smime_id_to_hash(CamelCipherContext *context, const char *id)
{
	int i;
	unsigned char *tmpid, *o, *in;
	unsigned char c;

	if (id == NULL)
		return CAMEL_CIPHER_HASH_DEFAULT;

	tmpid = alloca(strlen(id)+1);
	in = id;
	o = tmpid;
	while ((c = *in++))
		*o++ = tolower(c);

	for (i=1;i<sizeof(name_table)/sizeof(name_table[0]);i++) {
		if (!strcmp(name_table[i], tmpid))
			return i;
	}

	return CAMEL_CIPHER_HASH_DEFAULT;
}
#endif

#endif /* HAVE_NSS */
