/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *           Michael Zucchi <notzed@ximian.com>
 *
 *  The Initial Developer of the Original Code is Netscape
 *  Communications Corporation.  Portions created by Netscape are 
 *  Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 *  Rights Reserved.
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

#ifdef ENABLE_SMIME

#include "nss.h"
#include <cms.h>
#include <cert.h>
#include <certdb.h>
#include <pkcs11.h>
#include <smime.h>
#include <pkcs11t.h>
#include <pk11func.h>

#include <errno.h>

#include <camel/camel-exception.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-data-wrapper.h>

#include <camel/camel-mime-part.h>
#include <camel/camel-multipart-signed.h>
#include <camel/camel-stream-fs.h>
#include <camel/camel-stream-filter.h>
#include <camel/camel-mime-filter-basic.h>
#include <camel/camel-mime-filter-canon.h>

#include "camel-smime-context.h"
#include "camel-operation.h"

#define d(x)

struct _CamelSMIMEContextPrivate {
	CERTCertDBHandle *certdb;

	char *encrypt_key;
	camel_smime_sign_t sign_mode;

	int password_tries;
	unsigned int send_encrypt_key_prefs:1;
};

static CamelCipherContextClass *parent_class = NULL;

/* used for decode content callback, for streaming decode */
static void
sm_write_stream(void *arg, const char *buf, unsigned long len)
{
	camel_stream_write((CamelStream *)arg, buf, len);
}

static PK11SymKey *
sm_decrypt_key(void *arg, SECAlgorithmID *algid)
{
	printf("Decrypt key called\n");
	return (PK11SymKey *)arg;
}

static char *
sm_get_passwd(PK11SlotInfo *info, PRBool retry, void *arg)
{
	CamelSMIMEContext *context = arg;
	char *pass, *nsspass = NULL;
	char *prompt;
	CamelException *ex;

	ex = camel_exception_new();

	/* we got a password, but its asking again, the password we had was wrong */
	if (context->priv->password_tries > 0) {
		camel_session_forget_password(((CamelCipherContext *)context)->session, NULL, PK11_GetTokenName(info), NULL);
		context->priv->password_tries = 0;
	}

	prompt = g_strdup_printf(_("Enter security pass-phrase for `%s'"), PK11_GetTokenName(info));
	pass = camel_session_get_password(((CamelCipherContext *)context)->session, prompt,
					  CAMEL_SESSION_PASSWORD_SECRET|CAMEL_SESSION_PASSWORD_STATIC, NULL, PK11_GetTokenName(info), ex);
	camel_exception_free(ex);
	g_free(prompt);
	if (pass) {
		nsspass = PORT_Strdup(pass);
		memset(pass, 0, strlen(pass));
		g_free(pass);
		context->priv->password_tries++;
	}
	
	return nsspass;
}

/**
 * camel_smime_context_new:
 * @session: session
 *
 * Creates a new sm cipher context object.
 *
 * Returns a new sm cipher context object.
 **/
CamelCipherContext *
camel_smime_context_new(CamelSession *session)
{
	CamelCipherContext *cipher;
	CamelSMIMEContext *ctx;
	
	g_return_val_if_fail(CAMEL_IS_SESSION(session), NULL);
	
	ctx =(CamelSMIMEContext *) camel_object_new(camel_smime_context_get_type());
	
	cipher =(CamelCipherContext *) ctx;
	cipher->session = session;
	camel_object_ref(session);
	
	return cipher;
}

void
camel_smime_context_set_encrypt_key(CamelSMIMEContext *context, gboolean use, const char *key)
{
	context->priv->send_encrypt_key_prefs = use;
	g_free(context->priv->encrypt_key);
	context->priv->encrypt_key = g_strdup(key);
}

/* set signing mode, clearsigned multipart/signed or enveloped */
void
camel_smime_context_set_sign_mode(CamelSMIMEContext *context, camel_smime_sign_t type)
{
	context->priv->sign_mode = type;
}

/* TODO: This is suboptimal, but the only other solution is to pass around NSSCMSMessages */
guint32
camel_smime_context_describe_part(CamelSMIMEContext *context, CamelMimePart *part)
{
	guint32 flags = 0;
	CamelContentType *ct;
	const char *tmp;

	ct = camel_mime_part_get_content_type(part);

	if (camel_content_type_is(ct, "multipart", "signed")) {
		tmp = camel_content_type_param(ct, "protocol");
		if (tmp &&
		    (g_ascii_strcasecmp(tmp, ((CamelCipherContext *)context)->sign_protocol) == 0
		     || g_ascii_strcasecmp(tmp, "application/pkcs7-signature") == 0))
			flags = CAMEL_SMIME_SIGNED;
	} else if (camel_content_type_is(ct, "application", "x-pkcs7-mime")) {
		CamelStreamMem *istream;
		NSSCMSMessage *cmsg;
		NSSCMSDecoderContext *dec;

		/* FIXME: stream this to the decoder incrementally */
		istream = (CamelStreamMem *)camel_stream_mem_new();
		camel_data_wrapper_decode_to_stream(camel_medium_get_content_object((CamelMedium *)part), (CamelStream *)istream);
		camel_stream_reset((CamelStream *)istream);

		dec = NSS_CMSDecoder_Start(NULL, 
					   NULL, NULL,
					   sm_get_passwd, context,	/* password callback    */
					   NULL, NULL); /* decrypt key callback */
		
		NSS_CMSDecoder_Update(dec, istream->buffer->data, istream->buffer->len);
		camel_object_unref(istream);
		
		cmsg = NSS_CMSDecoder_Finish(dec);
		if (cmsg) {
			if (NSS_CMSMessage_IsSigned(cmsg)) {
				printf("message is signed\n");
				flags |= CAMEL_SMIME_SIGNED;
			}
			
			if (NSS_CMSMessage_IsEncrypted(cmsg)) {
				printf("message is encrypted\n");
				flags |= CAMEL_SMIME_ENCRYPTED;
			}
#if 0
			if (NSS_CMSMessage_ContainsCertsOrCrls(cmsg)) {
				printf("message contains certs or crls\n");
				flags |= CAMEL_SMIME_CERTS;
			}
#endif
			NSS_CMSMessage_Destroy(cmsg);
		} else {
			printf("Message could not be parsed\n");
		}
	}

	return flags;
}

static const char *
sm_hash_to_id(CamelCipherContext *context, CamelCipherHash hash)
{
	switch(hash) {
	case CAMEL_CIPHER_HASH_MD5:
		return "md5";
	case CAMEL_CIPHER_HASH_SHA1:
	case CAMEL_CIPHER_HASH_DEFAULT:
		return "sha1";
	default:
		return NULL;
	}
}

static CamelCipherHash
sm_id_to_hash(CamelCipherContext *context, const char *id)
{
	if (id) {
		if (!strcmp(id, "md5"))
			return CAMEL_CIPHER_HASH_MD5;
		else if (!strcmp(id, "sha1"))
			return CAMEL_CIPHER_HASH_SHA1;
	}
	
	return CAMEL_CIPHER_HASH_DEFAULT;
}

static NSSCMSMessage *
sm_signing_cmsmessage(CamelSMIMEContext *context, const char *nick, SECOidTag hash, int detached, CamelException *ex)
{
	struct _CamelSMIMEContextPrivate *p = context->priv;
	NSSCMSMessage *cmsg = NULL;
	NSSCMSContentInfo *cinfo;
	NSSCMSSignedData *sigd;
	NSSCMSSignerInfo *signerinfo;
	CERTCertificate *cert= NULL, *ekpcert = NULL;

	if ((cert = CERT_FindUserCertByUsage(p->certdb,
					     (char *)nick,
					     certUsageEmailSigner,
					     PR_FALSE,
					     NULL)) == NULL) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM, _("Cannot find certificate for '%s'"), nick);
		return NULL;
	}

	cmsg = NSS_CMSMessage_Create(NULL); /* create a message on its own pool */
	if (cmsg == NULL) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Cannot create CMS message"));
		goto fail;
	}

	if ((sigd = NSS_CMSSignedData_Create(cmsg)) == NULL) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Cannot create CMS signedData"));
		goto fail;
	}

	cinfo = NSS_CMSMessage_GetContentInfo(cmsg);
	if (NSS_CMSContentInfo_SetContent_SignedData(cmsg, cinfo, sigd) != SECSuccess) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Cannot attach CMS signedData"));
		goto fail;
	}

	/* if !detatched, the contentinfo will alloc a data item for us */
	cinfo = NSS_CMSSignedData_GetContentInfo(sigd);
	if (NSS_CMSContentInfo_SetContent_Data(cmsg, cinfo, NULL, detached) != SECSuccess) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Cannot attach CMS data"));
		goto fail;
	}

	signerinfo = NSS_CMSSignerInfo_Create(cmsg, cert, hash);
	if (signerinfo == NULL) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Cannot create CMS SignerInfo"));
		goto fail;
	}

	/* we want the cert chain included for this one */
	if (NSS_CMSSignerInfo_IncludeCerts(signerinfo, NSSCMSCM_CertChain, certUsageEmailSigner) != SECSuccess) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Cannot find cert chain"));
		goto fail;
	}

	/* SMIME RFC says signing time should always be added */
	if (NSS_CMSSignerInfo_AddSigningTime(signerinfo, PR_Now()) != SECSuccess) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Cannot add CMS SigningTime"));
		goto fail;
	}

#if 0
	/* this can but needn't be added.  not sure what general usage is */
	if (NSS_CMSSignerInfo_AddSMIMECaps(signerinfo) != SECSuccess) {
		fprintf(stderr, "ERROR: cannot add SMIMECaps attribute.\n");
		goto loser;
	}
#endif

	/* Check if we need to send along our return encrypt cert, rfc2633 2.5.3 */
	if (p->send_encrypt_key_prefs) {
		CERTCertificate *enccert = NULL;

		if (p->encrypt_key) {
			/* encrypt key has its own nick */
			if ((ekpcert = CERT_FindUserCertByUsage(
				     p->certdb,
				     p->encrypt_key,
				     certUsageEmailRecipient, PR_FALSE, NULL)) == NULL) {
				camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM, _("Encryption cert for '%s' does not exist"), p->encrypt_key);
				goto fail;
			}
			enccert = ekpcert;
		} else if (CERT_CheckCertUsage(cert, certUsageEmailRecipient) == SECSuccess) {
			/* encrypt key is signing key */
			enccert = cert;
		} else {
			/* encrypt key uses same nick */
			if ((ekpcert = CERT_FindUserCertByUsage(
				     p->certdb, (char *)nick,
				     certUsageEmailRecipient, PR_FALSE, NULL)) == NULL) {
				camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM, _("Encryption cert for '%s' does not exist"), nick);
				goto fail;
			}
			enccert = ekpcert;
		}

		if (NSS_CMSSignerInfo_AddSMIMEEncKeyPrefs(signerinfo, enccert, p->certdb) != SECSuccess) {
			camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Cannot add SMIMEEncKeyPrefs attribute"));
			goto fail;
		}

		if (NSS_CMSSignerInfo_AddMSSMIMEEncKeyPrefs(signerinfo, enccert, p->certdb) != SECSuccess) {
			camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Cannot add MS SMIMEEncKeyPrefs attribute"));
			goto fail;
		}

		if (ekpcert != NULL && NSS_CMSSignedData_AddCertificate(sigd, ekpcert) != SECSuccess) {
			camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Cannot add add encryption certificate"));
			goto fail;
		}
	}

	if (NSS_CMSSignedData_AddSignerInfo(sigd, signerinfo) != SECSuccess) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Cannot add CMS SignerInfo"));
		goto fail;
	}

	if (ekpcert)
		CERT_DestroyCertificate(ekpcert);

	if (cert)
		CERT_DestroyCertificate(cert);

	return cmsg;
fail:
	if (ekpcert)
		CERT_DestroyCertificate(ekpcert);

	if (cert)
		CERT_DestroyCertificate(cert);

	NSS_CMSMessage_Destroy(cmsg);

	return NULL;
}

static int
sm_sign(CamelCipherContext *context, const char *userid, CamelCipherHash hash, CamelMimePart *ipart, CamelMimePart *opart, CamelException *ex)
{
	int res = -1;
	NSSCMSMessage *cmsg;
	CamelStream *ostream, *istream;
	SECOidTag sechash;
	NSSCMSEncoderContext *enc;
	CamelDataWrapper *dw;
	CamelContentType *ct;

	switch (hash) {
	case CAMEL_CIPHER_HASH_SHA1:
	case CAMEL_CIPHER_HASH_DEFAULT:
	default:
		sechash = SEC_OID_SHA1;
		break;
	case CAMEL_CIPHER_HASH_MD5:
		sechash = SEC_OID_MD5;
		break;
	}

	cmsg = sm_signing_cmsmessage((CamelSMIMEContext *)context, userid, sechash,
				     ((CamelSMIMEContext *)context)->priv->sign_mode == CAMEL_SMIME_SIGN_CLEARSIGN, ex);
	if (cmsg == NULL)
		return -1;

	ostream = camel_stream_mem_new();

	/* FIXME: stream this, we stream output at least */
	istream = camel_stream_mem_new();
	if (camel_cipher_canonical_to_stream(ipart,
					     CAMEL_MIME_FILTER_CANON_STRIP
					     |CAMEL_MIME_FILTER_CANON_CRLF
					     |CAMEL_MIME_FILTER_CANON_FROM, istream) == -1) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not generate signing data: %s"), g_strerror(errno));
		goto fail;
	}

	enc = NSS_CMSEncoder_Start(cmsg, 
				   sm_write_stream, ostream, /* DER output callback  */
				   NULL, NULL, /* destination storage  */
				   sm_get_passwd, context, /* password callback    */
				   NULL, NULL,     /* decrypt key callback */
				   NULL, NULL );   /* detached digests    */
	if (!enc) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Cannot create encoder context"));
		goto fail;
	}

	if (NSS_CMSEncoder_Update(enc, ((CamelStreamMem *)istream)->buffer->data, ((CamelStreamMem *)istream)->buffer->len) != SECSuccess) {
		NSS_CMSEncoder_Cancel(enc);
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Failed to add data to CMS encoder"));
		goto fail;
	}

	if (NSS_CMSEncoder_Finish(enc) != SECSuccess) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Failed to encode data"));
		goto fail;
	}

	res = 0;

	dw = camel_data_wrapper_new();
	camel_stream_reset(ostream);
	camel_data_wrapper_construct_from_stream(dw, ostream);
	dw->encoding = CAMEL_TRANSFER_ENCODING_BINARY;

	if (((CamelSMIMEContext *)context)->priv->sign_mode == CAMEL_SMIME_SIGN_CLEARSIGN) {
		CamelMultipartSigned *mps;
		CamelMimePart *sigpart;

		sigpart = camel_mime_part_new();
		ct = camel_content_type_new("application", "x-pkcs7-signature");
		camel_content_type_set_param(ct, "name", "smime.p7s");
		camel_data_wrapper_set_mime_type_field(dw, ct);
		camel_content_type_unref(ct);

		camel_medium_set_content_object((CamelMedium *)sigpart, dw);

		camel_mime_part_set_filename(sigpart, "smime.p7s");
		camel_mime_part_set_disposition(sigpart, "attachment");
		camel_mime_part_set_encoding(sigpart, CAMEL_TRANSFER_ENCODING_BASE64);

		mps = camel_multipart_signed_new();
		ct = camel_content_type_new("multipart", "signed");
		camel_content_type_set_param(ct, "micalg", camel_cipher_hash_to_id(context, hash));
		camel_content_type_set_param(ct, "protocol", context->sign_protocol);
		camel_data_wrapper_set_mime_type_field((CamelDataWrapper *)mps, ct);
		camel_content_type_unref(ct);
		camel_multipart_set_boundary((CamelMultipart *)mps, NULL);

		mps->signature = sigpart;
		mps->contentraw = istream;
		camel_stream_reset(istream);
		camel_object_ref(istream);
		
		camel_medium_set_content_object((CamelMedium *)opart, (CamelDataWrapper *)mps);
	} else {
		ct = camel_content_type_new("application", "x-pkcs7-mime");
		camel_content_type_set_param(ct, "name", "smime.p7m");
		camel_content_type_set_param(ct, "smime-type", "signed-data");
		camel_data_wrapper_set_mime_type_field(dw, ct);
		camel_content_type_unref(ct);
		
		camel_medium_set_content_object((CamelMedium *)opart, dw);

		camel_mime_part_set_filename(opart, "smime.p7m");
		camel_mime_part_set_description(opart, "S/MIME Signed Message");
		camel_mime_part_set_disposition(opart, "attachment");
		camel_mime_part_set_encoding(opart, CAMEL_TRANSFER_ENCODING_BASE64);
	}
	
	camel_object_unref(dw);
fail:	
	camel_object_unref(ostream);
	camel_object_unref(istream);

	return res;
}

static const char *
sm_status_description(NSSCMSVerificationStatus status)
{
	/* could use this but then we can't control i18n? */
	/*NSS_CMSUtil_VerificationStatusToString(status));*/

	switch(status) {
	case NSSCMSVS_Unverified:
	default:
		return _("Unverified");
	case NSSCMSVS_GoodSignature:
		return _("Good signature");
	case NSSCMSVS_BadSignature:
		return _("Bad signature");
	case NSSCMSVS_DigestMismatch:
		return _("Content tampered with or altered in transit");
	case NSSCMSVS_SigningCertNotFound:
		return _("Signing certificate not found");
	case NSSCMSVS_SigningCertNotTrusted:
		return _("Signing certificate not trusted");
	case NSSCMSVS_SignatureAlgorithmUnknown:
		return _("Signature algorithm unknown");
	case NSSCMSVS_SignatureAlgorithmUnsupported:
		return _("Signature algorithm unsupported");
	case NSSCMSVS_MalformedSignature:
		return _("Malformed signature");
	case NSSCMSVS_ProcessingError:
		return _("Processing error");
	}
}

static CamelCipherValidity *
sm_verify_cmsg(CamelCipherContext *context, NSSCMSMessage *cmsg, CamelStream *extstream, CamelException *ex)
{
	struct _CamelSMIMEContextPrivate *p = ((CamelSMIMEContext *)context)->priv;
	NSSCMSSignedData *sigd = NULL;
	NSSCMSEnvelopedData *envd;
	NSSCMSEncryptedData *encd;
	SECAlgorithmID **digestalgs;
	NSSCMSDigestContext *digcx;
	int count, i, nsigners, j;
	SECItem **digests;
	PLArenaPool *poolp = NULL;
	CamelStreamMem *mem;
	NSSCMSVerificationStatus status;
	CamelCipherValidity *valid;
	GString *description;

	description = g_string_new("");
	valid = camel_cipher_validity_new();
	camel_cipher_validity_set_valid(valid, TRUE);
	status = NSSCMSVS_Unverified;

	/* NB: this probably needs to go into a decoding routine that can be used for processing
	   enveloped data too */
	count = NSS_CMSMessage_ContentLevelCount(cmsg);
	for (i = 0; i < count; i++) {
		NSSCMSContentInfo *cinfo = NSS_CMSMessage_ContentLevel(cmsg, i);
		SECOidTag typetag = NSS_CMSContentInfo_GetContentTypeTag(cinfo);

		switch (typetag) {
		case SEC_OID_PKCS7_SIGNED_DATA:
			sigd = (NSSCMSSignedData *)NSS_CMSContentInfo_GetContent(cinfo);
			if (sigd == NULL) {
				camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("No signedData in signature"));
				goto fail;
			}

			/* need to build digests of the content */
			if (!NSS_CMSSignedData_HasDigests(sigd)) {
				if (extstream == NULL) {
					camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Digests missing from enveloped data"));
					goto fail;
				}

				if ((poolp = PORT_NewArena(1024)) == NULL) {
					camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, g_strerror (ENOMEM));
					goto fail;
				}

				digestalgs = NSS_CMSSignedData_GetDigestAlgs(sigd);
				
				digcx = NSS_CMSDigestContext_StartMultiple(digestalgs);
				if (digcx == NULL) {
					camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Cannot calculate digests"));
					goto fail;
				}

				mem = (CamelStreamMem *)camel_stream_mem_new();
				camel_stream_write_to_stream(extstream, (CamelStream *)mem);
				NSS_CMSDigestContext_Update(digcx, mem->buffer->data, mem->buffer->len);
				camel_object_unref(mem);

				if (NSS_CMSDigestContext_FinishMultiple(digcx, poolp, &digests) != SECSuccess) {
					camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Cannot calculate digests"));
					goto fail;
				}

				if (NSS_CMSSignedData_SetDigests(sigd, digestalgs, digests) != SECSuccess) {
					camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Cannot set message digests"));
					goto fail;
				}

				PORT_FreeArena(poolp, PR_FALSE);
				poolp = NULL;
			}

			/* import the certificates */
			if (NSS_CMSSignedData_ImportCerts(sigd, p->certdb, certUsageEmailSigner, PR_FALSE) != SECSuccess) {
				camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Certificate import failed"));
				goto fail;
			}

			/* check for certs-only message */
			nsigners = NSS_CMSSignedData_SignerInfoCount(sigd);
			if (nsigners == 0) {
				/* ?? Should we check other usages? */
				NSS_CMSSignedData_ImportCerts(sigd, p->certdb, certUsageEmailSigner, PR_TRUE);
				if (NSS_CMSSignedData_VerifyCertsOnly(sigd, p->certdb, certUsageEmailSigner) != SECSuccess) {
					g_string_printf(description, _("Certficate only message, cannot verify certificates"));
				} else {
					status = NSSCMSVS_GoodSignature;
					g_string_printf(description, _("Certficate only message, certificates imported and verified"));
				}
			} else {
				if (!NSS_CMSSignedData_HasDigests(sigd)) {
					camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Can't find signature digests"));
					goto fail;
				}

				for (j = 0; j < nsigners; j++) {
					NSSCMSSignerInfo *si;
					char *cn, *em;
					
					si = NSS_CMSSignedData_GetSignerInfo(sigd, j);
					NSS_CMSSignedData_VerifySignerInfo(sigd, j, p->certdb, certUsageEmailSigner);
					
					status = NSS_CMSSignerInfo_GetVerificationStatus(si);
					
					cn = NSS_CMSSignerInfo_GetSignerCommonName(si);
					em = NSS_CMSSignerInfo_GetSignerEmailAddress(si);
					
					g_string_append_printf(description, _("Signer: %s <%s>: %s\n"),
							       cn?cn:"<unknown>", em?em:"<unknown>",
							       sm_status_description(status));
					
					camel_cipher_validity_add_certinfo(valid, CAMEL_CIPHER_VALIDITY_SIGN, cn, em);

					if (cn)
						PORT_Free(cn);
					if (em)
						PORT_Free(em);
					
					if (status != NSSCMSVS_GoodSignature)
						camel_cipher_validity_set_valid(valid, FALSE);
				}
			}
			break;
		case SEC_OID_PKCS7_ENVELOPED_DATA:
			envd = (NSSCMSEnvelopedData *)NSS_CMSContentInfo_GetContent(cinfo);
			break;
		case SEC_OID_PKCS7_ENCRYPTED_DATA:
			encd = (NSSCMSEncryptedData *)NSS_CMSContentInfo_GetContent(cinfo);
			break;
		case SEC_OID_PKCS7_DATA:
			break;
		default:
			break;
		}
	}

	camel_cipher_validity_set_valid(valid, status == NSSCMSVS_GoodSignature);
	camel_cipher_validity_set_description(valid, description->str);
	g_string_free(description, TRUE);

	return valid;

fail:
	camel_cipher_validity_free(valid);
	g_string_free(description, TRUE);

	return NULL;
}

static CamelCipherValidity *
sm_verify(CamelCipherContext *context, CamelMimePart *ipart, CamelException *ex)
{
	NSSCMSDecoderContext *dec;
	NSSCMSMessage *cmsg;
	CamelStreamMem *mem;
	CamelStream *constream;
	CamelCipherValidity *valid = NULL;
	CamelContentType *ct;
	const char *tmp;
	CamelMimePart *sigpart;
	CamelDataWrapper *dw;

	dw = camel_medium_get_content_object((CamelMedium *)ipart);
	ct = dw->mime_type;

	/* FIXME: we should stream this to the decoder */
	mem = (CamelStreamMem *)camel_stream_mem_new();
	
	if (camel_content_type_is(ct, "multipart", "signed")) {
		CamelMultipart *mps = (CamelMultipart *)dw;

		tmp = camel_content_type_param(ct, "protocol");
		if (!CAMEL_IS_MULTIPART_SIGNED(mps)
		    || tmp == NULL
		    || (g_ascii_strcasecmp(tmp, context->sign_protocol) != 0
			&& g_ascii_strcasecmp(tmp, "application/pkcs7-signature") != 0)) {
			camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Cannot verify message signature: Incorrect message format"));
			goto fail;
		}

		constream = camel_multipart_signed_get_content_stream((CamelMultipartSigned *)mps, ex);
		if (constream == NULL)
			goto fail;

		sigpart = camel_multipart_get_part(mps, CAMEL_MULTIPART_SIGNED_SIGNATURE);
		if (sigpart == NULL) {
			camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Cannot verify message signature: Incorrect message format"));
			goto fail;
		}
	} else if (camel_content_type_is(ct, "application", "x-pkcs7-mime")) {
		sigpart = ipart;
	} else {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Cannot verify message signature: Incorrect message format"));
		goto fail;
	}

	dec = NSS_CMSDecoder_Start(NULL, 
				   NULL, NULL, /* content callback     */
				   sm_get_passwd, context,	/* password callback    */
				   NULL, NULL); /* decrypt key callback */

	camel_data_wrapper_decode_to_stream(camel_medium_get_content_object((CamelMedium *)sigpart), (CamelStream *)mem);
	(void)NSS_CMSDecoder_Update(dec, mem->buffer->data, mem->buffer->len);
	cmsg = NSS_CMSDecoder_Finish(dec);
	if (cmsg == NULL) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Decoder failed"));
		goto fail;
	}
	
	valid = sm_verify_cmsg(context, cmsg, constream, ex);
	
	NSS_CMSMessage_Destroy(cmsg);
fail:
	camel_object_unref(mem);
	if (constream)
		camel_object_unref(constream);

	return valid;
}

static int
sm_encrypt(CamelCipherContext *context, const char *userid, GPtrArray *recipients, CamelMimePart *ipart, CamelMimePart *opart, CamelException *ex)
{
	struct _CamelSMIMEContextPrivate *p = ((CamelSMIMEContext *)context)->priv;
	/*NSSCMSRecipientInfo **recipient_infos;*/
	CERTCertificate **recipient_certs = NULL;
	NSSCMSContentInfo *cinfo;
	PK11SymKey *bulkkey = NULL;
	SECOidTag bulkalgtag;
	int bulkkeysize, i;
	CK_MECHANISM_TYPE type;
	PK11SlotInfo *slot;
	PLArenaPool *poolp;
	NSSCMSMessage *cmsg = NULL;
	NSSCMSEnvelopedData *envd;
 	NSSCMSEncoderContext *enc = NULL;
	CamelStreamMem *mem;
	CamelStream *ostream = NULL;
	CamelDataWrapper *dw;
	CamelContentType *ct;

	poolp = PORT_NewArena(1024);
	if (poolp == NULL) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, g_strerror (ENOMEM));
		return -1;
	}

	/* Lookup all recipients certs, for later working */
	recipient_certs = (CERTCertificate **)PORT_ArenaZAlloc(poolp, sizeof(*recipient_certs[0])*(recipients->len + 1));
	if (recipient_certs == NULL) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, g_strerror (ENOMEM));
		goto fail;
	}

	for (i=0;i<recipients->len;i++) {
		recipient_certs[i] = CERT_FindCertByNicknameOrEmailAddr(p->certdb, recipients->pdata[i]);
		if (recipient_certs[i] == NULL) {
			camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM, _("Can't find certificate for `%s'"), recipients->pdata[i]);
			goto fail;
		}
	}

	/* Find a common algorithm, probably 3DES anyway ... */
	if (NSS_SMIMEUtil_FindBulkAlgForRecipients(recipient_certs, &bulkalgtag, &bulkkeysize) != SECSuccess) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Can't find common bulk encryption algorithm"));
		goto fail;
	}

	/* Generate a new bulk key based on the common algorithm - expensive */
	type = PK11_AlgtagToMechanism(bulkalgtag);
	slot = PK11_GetBestSlot(type, context);
	if (slot == NULL) {
		/* PORT_GetError(); ?? */
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Can't allocate slot for encryption bulk key"));
		goto fail;
	}

	bulkkey = PK11_KeyGen(slot, type, NULL, bulkkeysize/8, context);
	PK11_FreeSlot(slot);

	/* Now we can start building the message */
	/* msg->envelopedData->data */
	cmsg = NSS_CMSMessage_Create(NULL);
	if (cmsg == NULL) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Can't create CMS Message"));
		goto fail;
	}

	envd = NSS_CMSEnvelopedData_Create(cmsg, bulkalgtag, bulkkeysize);
	if (envd == NULL) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Can't create CMS EnvelopedData"));
		goto fail;
	}

	cinfo = NSS_CMSMessage_GetContentInfo(cmsg);
	if (NSS_CMSContentInfo_SetContent_EnvelopedData(cmsg, cinfo, envd) != SECSuccess) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Can't attach CMS EnvelopedData"));
		goto fail;
	}

	cinfo = NSS_CMSEnvelopedData_GetContentInfo(envd);
	if (NSS_CMSContentInfo_SetContent_Data(cmsg, cinfo, NULL, PR_FALSE) != SECSuccess) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Can't attach CMS data object"));
		goto fail;
	}

	/* add recipient certs */
	for (i=0;recipient_certs[i];i++) {
		NSSCMSRecipientInfo *ri = NSS_CMSRecipientInfo_Create(cmsg, recipient_certs[i]);

		if (ri == NULL) {
			camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Can't create CMS RecipientInfo"));
			goto fail;
		}

		if (NSS_CMSEnvelopedData_AddRecipient(envd, ri) != SECSuccess) {
			camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Can't add CMS RecipientInfo"));
			goto fail;
		}
	}

	/* dump it out */
	ostream = camel_stream_mem_new();
	enc = NSS_CMSEncoder_Start(cmsg,
				   sm_write_stream, ostream,
				   NULL, NULL,
				   sm_get_passwd, context,
				   sm_decrypt_key, bulkkey,
				   NULL, NULL);
	if (enc == NULL) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Can't create encoder context"));
		goto fail;
	}

	/* FIXME: Stream the input */
	/* FIXME: Canonicalise the input? */
	mem = (CamelStreamMem *)camel_stream_mem_new();
	camel_data_wrapper_write_to_stream((CamelDataWrapper *)ipart, (CamelStream *)mem);
	if (NSS_CMSEncoder_Update(enc, mem->buffer->data, mem->buffer->len) != SECSuccess) {
		NSS_CMSEncoder_Cancel(enc);
		camel_object_unref(mem);
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Failed to add data to encoder"));
		goto fail;
	}
	camel_object_unref(mem);

	if (NSS_CMSEncoder_Finish(enc) != SECSuccess) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Failed to encode data"));
		goto fail;
	}

	PK11_FreeSymKey(bulkkey);
	NSS_CMSMessage_Destroy(cmsg);
	for (i=0;recipient_certs[i];i++)
		CERT_DestroyCertificate(recipient_certs[i]);
	PORT_FreeArena(poolp, PR_FALSE);
	
	dw = camel_data_wrapper_new();
	camel_data_wrapper_construct_from_stream(dw, ostream);
	camel_object_unref(ostream);
	dw->encoding = CAMEL_TRANSFER_ENCODING_BINARY;

	ct = camel_content_type_new("application", "x-pkcs7-mime");
	camel_content_type_set_param(ct, "name", "smime.p7m");
	camel_content_type_set_param(ct, "smime-type", "enveloped-data");
	camel_data_wrapper_set_mime_type_field(dw, ct);
	camel_content_type_unref(ct);

	camel_medium_set_content_object((CamelMedium *)opart, dw);
	camel_object_unref(dw);

	camel_mime_part_set_disposition(opart, "attachment");
	camel_mime_part_set_filename(opart, "smime.p7m");
	camel_mime_part_set_description(opart, "S/MIME Encrypted Message");
	camel_mime_part_set_encoding(opart, CAMEL_TRANSFER_ENCODING_BASE64);

	return 0;

fail:
	if (ostream)
		camel_object_unref(ostream);
	if (cmsg)
		NSS_CMSMessage_Destroy(cmsg);
	if (bulkkey)
		PK11_FreeSymKey(bulkkey);

	if (recipient_certs) {
		for (i=0;recipient_certs[i];i++)
			CERT_DestroyCertificate(recipient_certs[i]);
	}

	PORT_FreeArena(poolp, PR_FALSE);

	return -1;
}

static CamelCipherValidity *
sm_decrypt(CamelCipherContext *context, CamelMimePart *ipart, CamelMimePart *opart, CamelException *ex)
{
	NSSCMSDecoderContext *dec;
	NSSCMSMessage *cmsg;
	CamelStreamMem *istream;
	CamelStream *ostream;
	CamelCipherValidity *valid = NULL;

	/* FIXME: This assumes the content is only encrypted.  Perhaps its ok for
	   this api to do this ... */

	ostream = camel_stream_mem_new();

	/* FIXME: stream this to the decoder incrementally */
	istream = (CamelStreamMem *)camel_stream_mem_new();
	camel_data_wrapper_decode_to_stream(camel_medium_get_content_object((CamelMedium *)ipart), (CamelStream *)istream);
	camel_stream_reset((CamelStream *)istream);

	dec = NSS_CMSDecoder_Start(NULL, 
				   sm_write_stream, ostream, /* content callback     */
				   sm_get_passwd, context,	/* password callback    */
				   NULL, NULL); /* decrypt key callback */

	if (NSS_CMSDecoder_Update(dec, istream->buffer->data, istream->buffer->len) != SECSuccess) {
		printf("decoder update failed\n");
	}
	camel_object_unref(istream);

	cmsg = NSS_CMSDecoder_Finish(dec);
	if (cmsg == NULL) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM, _("Decoder failed, error %d"), PORT_GetError());
		goto fail;
	}

#if 0
	/* not sure if we really care about this? */
	if (!NSS_CMSMessage_IsEncrypted(cmsg)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("S/MIME Decrypt: No encrypted content found"));
		NSS_CMSMessage_Destroy(cmsg);
		goto fail;
	}
#endif

	camel_stream_reset(ostream);
	camel_data_wrapper_construct_from_stream((CamelDataWrapper *)opart, ostream);

	if (NSS_CMSMessage_IsSigned(cmsg)) {
		valid = sm_verify_cmsg(context, cmsg, NULL, ex);
	} else {
		valid = camel_cipher_validity_new();
		valid->encrypt.description = g_strdup(_("Encrypted content"));
		valid->encrypt.status = CAMEL_CIPHER_VALIDITY_ENCRYPT_ENCRYPTED;
	}

	NSS_CMSMessage_Destroy(cmsg);
fail:
	camel_object_unref(ostream);

	return valid;
}

static int
sm_import_keys(CamelCipherContext *context, CamelStream *istream, CamelException *ex)
{
	camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("import keys: unimplemented"));
	
	return -1;
}

static int
sm_export_keys(CamelCipherContext *context, GPtrArray *keys, CamelStream *ostream, CamelException *ex)
{
	camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("export keys: unimplemented"));
	
	return -1;
}

/* ********************************************************************** */

static void
camel_smime_context_class_init(CamelSMIMEContextClass *klass)
{
	CamelCipherContextClass *cipher_class = CAMEL_CIPHER_CONTEXT_CLASS(klass);
	
	parent_class = CAMEL_CIPHER_CONTEXT_CLASS(camel_type_get_global_classfuncs(camel_cipher_context_get_type()));
	
	cipher_class->hash_to_id = sm_hash_to_id;
	cipher_class->id_to_hash = sm_id_to_hash;
	cipher_class->sign = sm_sign;
	cipher_class->verify = sm_verify;
	cipher_class->encrypt = sm_encrypt;
	cipher_class->decrypt = sm_decrypt;
	cipher_class->import_keys = sm_import_keys;
	cipher_class->export_keys = sm_export_keys;
}

static void
camel_smime_context_init(CamelSMIMEContext *context)
{
	CamelCipherContext *cipher =(CamelCipherContext *) context;
	
	cipher->sign_protocol = "application/x-pkcs7-signature";
	cipher->encrypt_protocol = "application/x-pkcs7-mime";
	cipher->key_protocol = "application/x-pkcs7-signature";

	context->priv = g_malloc0(sizeof(*context->priv));
	context->priv->certdb = CERT_GetDefaultCertDB();
	context->priv->sign_mode = CAMEL_SMIME_SIGN_CLEARSIGN;
	context->priv->password_tries = 0;
}

static void
camel_smime_context_finalise(CamelObject *object)
{
	CamelSMIMEContext *context = (CamelSMIMEContext *)object;

	/* FIXME: do we have to free the certdb? */

	g_free(context->priv);
}

CamelType
camel_smime_context_get_type(void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register(camel_cipher_context_get_type(),
					   "CamelSMIMEContext",
					   sizeof(CamelSMIMEContext),
					   sizeof(CamelSMIMEContextClass),
					   (CamelObjectClassInitFunc) camel_smime_context_class_init,
					   NULL,
					   (CamelObjectInitFunc) camel_smime_context_init,
					   (CamelObjectFinalizeFunc) camel_smime_context_finalise);
	}
	
	return type;
}

#endif /* ENABLE_SMIME */
