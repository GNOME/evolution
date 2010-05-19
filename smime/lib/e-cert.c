/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* The following is the mozilla license blurb, as the bodies some of
   these functions were derived from the mozilla source. */
/*
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 */

/*
 * Author: Chris Toshok (toshok@ximian.com)
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <time.h>

#include <glib/gi18n.h>
#include <libedataserver/e-data-server-util.h>
#include <e-util/e-util.h>	/* for e_utf8_strftime, what about e_time_format_time? */

#include "e-cert.h"
#include "e-cert-trust.h"
#include "pk11func.h"
#include "certdb.h"
#include "hasht.h"

struct _ECertPrivate {
	CERTCertificate *cert;

	/* pointers we cache since the nss implementation allocs the
	   string */
	gchar *org_name;
	gchar *org_unit_name;
	gchar *cn;

	gchar *issuer_org_name;
	gchar *issuer_org_unit_name;
	gchar *issuer_cn;

	PRTime issued_on;
	PRTime expires_on;

	gchar *issued_on_string;
	gchar *expires_on_string;

	gchar *serial_number;

	gchar *usage_string;

	gchar *sha1_fingerprint;
	gchar *md5_fingerprint;

	EASN1Object *asn1;

	gboolean delete;
};

#define PARENT_TYPE G_TYPE_OBJECT
static GObjectClass *parent_class;

static void
e_cert_dispose (GObject *object)
{
	ECert *ec = E_CERT (object);

	if (!ec->priv)
		return;

	if (ec->priv->org_name)
		PORT_Free (ec->priv->org_name);
	if (ec->priv->org_unit_name)
		PORT_Free (ec->priv->org_unit_name);
	if (ec->priv->cn)
		PORT_Free (ec->priv->cn);

	if (ec->priv->issuer_org_name)
		PORT_Free (ec->priv->issuer_org_name);
	if (ec->priv->issuer_org_unit_name)
		PORT_Free (ec->priv->issuer_org_unit_name);
	if (ec->priv->issuer_cn)
		PORT_Free (ec->priv->issuer_cn);

	if (ec->priv->issued_on_string)
		PORT_Free (ec->priv->issued_on_string);
	if (ec->priv->expires_on_string)
		PORT_Free (ec->priv->expires_on_string);
	if (ec->priv->serial_number)
		PORT_Free (ec->priv->serial_number);

	g_free(ec->priv->usage_string);

	if (ec->priv->sha1_fingerprint)
		PORT_Free (ec->priv->sha1_fingerprint);
	if (ec->priv->md5_fingerprint)
		PORT_Free (ec->priv->md5_fingerprint);

	if (ec->priv->asn1)
		g_object_unref (ec->priv->asn1);

	if (ec->priv->delete) {
		printf ("attempting to delete cert marked for deletion\n");
		if (e_cert_get_cert_type (ec) == E_CERT_USER) {
			PK11_DeleteTokenCertAndKey(ec->priv->cert, NULL);
		} else if (!PK11_IsReadOnly(ec->priv->cert->slot)) {
			/* If the list of built-ins does contain a non-removable
			   copy of this certificate, our call will not remove
			   the certificate permanently, but rather remove all trust. */
			SEC_DeletePermCertificate(ec->priv->cert);
		}
	}

	if (ec->priv->cert) {
		CERT_DestroyCertificate (ec->priv->cert);
		ec->priv->cert = NULL;
	}

	g_free (ec->priv);
	ec->priv = NULL;

	if (G_OBJECT_CLASS (parent_class)->dispose)
		G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
e_cert_class_init (ECertClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_ref (PARENT_TYPE);

	object_class->dispose = e_cert_dispose;
}

static void
e_cert_init (ECert *ec)
{
	ec->priv = g_new0 (ECertPrivate, 1);
}

GType
e_cert_get_type (void)
{
	static GType cert_type = 0;

	if (!cert_type) {
		static const GTypeInfo cert_info =  {
			sizeof (ECertClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_cert_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (ECert),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_cert_init,
		};

		cert_type = g_type_register_static (PARENT_TYPE, "ECert", &cert_info, 0);
	}

	return cert_type;
}



static void
e_cert_populate (ECert *cert)
{
	CERTCertificate *c = cert->priv->cert;
	guchar fingerprint[20];
	SECItem fpItem;

	cert->priv->org_name = CERT_GetOrgName (&c->subject);
	cert->priv->org_unit_name = CERT_GetOrgUnitName (&c->subject);

	cert->priv->issuer_org_name = CERT_GetOrgName (&c->issuer);
	cert->priv->issuer_org_unit_name = CERT_GetOrgUnitName (&c->issuer);

	cert->priv->cn = CERT_GetCommonName (&c->subject);
	cert->priv->issuer_cn = CERT_GetCommonName (&c->issuer);

	if (SECSuccess == CERT_GetCertTimes (
		c, &cert->priv->issued_on, &cert->priv->expires_on)) {
		PRExplodedTime explodedTime;
		struct tm exploded_tm;
		gchar buf[32];

		PR_ExplodeTime (cert->priv->issued_on, PR_LocalTimeParameters, &explodedTime);
		exploded_tm.tm_sec = explodedTime.tm_sec;
		exploded_tm.tm_min = explodedTime.tm_min;
		exploded_tm.tm_hour = explodedTime.tm_hour;
		exploded_tm.tm_mday = explodedTime.tm_mday;
		exploded_tm.tm_mon = explodedTime.tm_month;
		exploded_tm.tm_year = explodedTime.tm_year - 1900;
		e_utf8_strftime (buf, sizeof(buf), _("%d/%m/%Y"), &exploded_tm);
		cert->priv->issued_on_string = g_strdup (buf);

		PR_ExplodeTime (cert->priv->expires_on, PR_LocalTimeParameters, &explodedTime);
		exploded_tm.tm_sec = explodedTime.tm_sec;
		exploded_tm.tm_min = explodedTime.tm_min;
		exploded_tm.tm_hour = explodedTime.tm_hour;
		exploded_tm.tm_mday = explodedTime.tm_mday;
		exploded_tm.tm_mon = explodedTime.tm_month;
		exploded_tm.tm_year = explodedTime.tm_year - 1900;
		e_utf8_strftime (buf, sizeof(buf), _("%d/%m/%Y"), &exploded_tm);
		cert->priv->expires_on_string = g_strdup (buf);
	}

	cert->priv->serial_number = CERT_Hexify (&cert->priv->cert->serialNumber, TRUE);

	memset(fingerprint, 0, sizeof fingerprint);
	PK11_HashBuf(SEC_OID_SHA1, fingerprint,
		     cert->priv->cert->derCert.data,
		     cert->priv->cert->derCert.len);
	fpItem.data = fingerprint;
	fpItem.len = SHA1_LENGTH;
	cert->priv->sha1_fingerprint = CERT_Hexify (&fpItem, TRUE);

	memset(fingerprint, 0, sizeof fingerprint);
	PK11_HashBuf(SEC_OID_MD5, fingerprint,
		     cert->priv->cert->derCert.data,
		     cert->priv->cert->derCert.len);
	fpItem.data = fingerprint;
	fpItem.len = MD5_LENGTH;
	cert->priv->md5_fingerprint = CERT_Hexify (&fpItem, TRUE);
}

ECert*
e_cert_new (CERTCertificate *cert)
{
	ECert *ecert = E_CERT (g_object_new (E_TYPE_CERT, NULL));

	/* ECert owns a reference to the 'cert', which will be freed on ECert finalize */
	ecert->priv->cert = cert;

	e_cert_populate (ecert);

	return ecert;
}

ECert*
e_cert_new_from_der (gchar *data, guint32 len)
{
	CERTCertificate *cert = CERT_DecodeCertFromPackage (data, len);

	if (!cert)
		return NULL;

	if (cert->dbhandle == NULL)
		cert->dbhandle = CERT_GetDefaultCertDB();

	return e_cert_new (cert);
}



CERTCertificate*
e_cert_get_internal_cert (ECert *cert)
{
	/* XXX should this refcnt it? */
	return cert->priv->cert;
}

gboolean
e_cert_get_raw_der (ECert *cert, gchar **data, guint32 *len)
{
	/* XXX do we really need to check if cert->priv->cert is NULL
	   here?  it should always be non-null if we have the
	   ECert.. */
	if (cert->priv->cert) {
		*data = (gchar *)cert->priv->cert->derCert.data;
		*len = (guint32)cert->priv->cert->derCert.len;
		return TRUE;
	}

	*len = 0;
	return FALSE;

}

const gchar *
e_cert_get_window_title  (ECert *cert)
{
	if (cert->priv->cert->nickname)
		return cert->priv->cert->nickname;
	else if (cert->priv->cn)
		return cert->priv->cn;
	else
		return cert->priv->cert->subjectName;
}

const gchar *
e_cert_get_nickname (ECert *cert)
{
	return cert->priv->cert->nickname;
}

const gchar *
e_cert_get_email    (ECert *cert)
{
	return cert->priv->cert->emailAddr;
}

const gchar *
e_cert_get_org      (ECert *cert)
{
	return cert->priv->org_name;
}

const gchar *
e_cert_get_org_unit (ECert *cert)
{
	return cert->priv->org_unit_name;
}

const gchar *
e_cert_get_cn       (ECert *cert)
{
	return cert->priv->cn;
}

const gchar *
e_cert_get_issuer_name (ECert *cert)
{
	return cert->priv->cert->issuerName;
}

const gchar *
e_cert_get_issuer_cn (ECert *cert)
{
	return cert->priv->issuer_cn;
}

const gchar *
e_cert_get_issuer_org (ECert *cert)
{
	return cert->priv->issuer_org_name;
}

const gchar *
e_cert_get_issuer_org_unit (ECert *cert)
{
	return cert->priv->issuer_org_unit_name;
}

const gchar *
e_cert_get_subject_name (ECert *cert)
{
	return cert->priv->cert->subjectName;
}

PRTime
e_cert_get_issued_on_time  (ECert *cert)
{
	return cert->priv->issued_on;
}

const gchar *
e_cert_get_issued_on (ECert *cert)
{
	return cert->priv->issued_on_string;
}

PRTime
e_cert_get_expires_on_time  (ECert *cert)
{
	return cert->priv->expires_on;
}

const gchar *
e_cert_get_expires_on (ECert *cert)
{
	return cert->priv->expires_on_string;
}

static struct {
	gint bit;
	const gchar *text;
} usageinfo[] = {
	/* x509 certificate usage types */
	{ certificateUsageEmailSigner, N_("Sign") },
	{ certificateUsageEmailRecipient, N_("Encrypt") },
};

const gchar *
e_cert_get_usage(ECert *cert)
{
	if (cert->priv->usage_string == NULL) {
		gint i;
		GString *str = g_string_new("");
		CERTCertificate *icert = e_cert_get_internal_cert (cert);

		for (i = 0; i < G_N_ELEMENTS (usageinfo); i++) {
			if (icert->keyUsage & usageinfo[i].bit) {
				if (str->len != 0)
					g_string_append(str, ", ");
				g_string_append(str, _(usageinfo[i].text));
			}
		}

		cert->priv->usage_string = str->str;
		g_string_free(str, FALSE);
	}

	return cert->priv->usage_string;
}

const gchar *
e_cert_get_serial_number (ECert *cert)
{
	return cert->priv->serial_number;
}

const gchar *
e_cert_get_sha1_fingerprint (ECert *cert)
{
	return cert->priv->sha1_fingerprint;
}

const gchar *
e_cert_get_md5_fingerprint  (ECert *cert)
{
	return cert->priv->md5_fingerprint;
}

GList*
e_cert_get_chain (ECert *ecert)
{
	GList *l = NULL;

	g_object_ref (ecert);

	while (ecert) {
		CERTCertificate *cert = e_cert_get_internal_cert (ecert);
		CERTCertificate *next_cert;

		l = g_list_append (l, ecert);

		if (SECITEM_CompareItem(&cert->derIssuer, &cert->derSubject) == SECEqual)
			break;

		next_cert = CERT_FindCertIssuer (cert, PR_Now(), certUsageSSLClient);
		if (!next_cert)
			break;

		/* next_cert has a reference already */
		ecert = e_cert_new (next_cert);
	}

	return l;
}

ECert *
e_cert_get_ca_cert(ECert *ecert)
{
	CERTCertificate *cert, *next = e_cert_get_internal_cert(ecert), *internal;

	cert = next;
	internal = cert;
	do {
		if (cert != next && cert != internal)
			CERT_DestroyCertificate (cert);

		cert = next;
		next = CERT_FindCertIssuer (cert, PR_Now(), certUsageAnyCA);
	} while (next && next != cert);

	if (cert == internal)
		return g_object_ref(ecert);
	else
		return e_cert_new(cert);
}

static gboolean
get_int_value (SECItem *versionItem,
	       unsigned long *version)
{
	SECStatus srv;
	srv = SEC_ASN1DecodeInteger(versionItem,version);
	if (srv != SECSuccess) {
		g_warning ("could not decode version of cert");
		return FALSE;
	}
	return TRUE;
}

static gboolean
process_version (SECItem     *versionItem,
		 EASN1Object **retItem)
{
	EASN1Object *item = e_asn1_object_new ();
	unsigned long version;

	e_asn1_object_set_display_name (item, _("Version"));

	/* Now to figure out what version this certificate is. */

	if (versionItem->data) {
		if (!get_int_value (versionItem, &version))
			return FALSE;
	} else {
		/* If there is no version present in the cert, then rfc2459
		   says we default to v1 (0) */
		version = 0;
	}

	switch (version) {
	case 0:
		e_asn1_object_set_display_value (item, _("Version 1"));
		break;
	case 1:
		e_asn1_object_set_display_value (item, _("Version 2"));
		break;
	case 2:
		e_asn1_object_set_display_value (item, _("Version 3"));
		break;
	default:
		g_warning ("Bad value for cert version");
		return FALSE;
	}

	*retItem = item;
	return TRUE;
}

static gboolean
process_serial_number_der (SECItem      *serialItem,
			   EASN1Object **retItem)
{
	gchar *serialNumber;
	EASN1Object *item = e_asn1_object_new ();

	e_asn1_object_set_display_name (item, _("Serial Number"));

	serialNumber = CERT_Hexify(serialItem, 1);

	e_asn1_object_set_display_value (item, serialNumber);
	PORT_Free (serialNumber); /* XXX the right free to use? */

	*retItem = item;
	return TRUE;
}

static gboolean
get_default_oid_format (SECItem *oid,
			gchar **text)
{
	gchar buf[300];
	guint len;
	gint written;

	unsigned long val  = oid->data[0];
	guint  i    = val % 40;
	val /= 40;
	written = PR_snprintf(buf, 300, "%lu %u ", val, i);
	if (written < 0)
		return FALSE;
	len = written;

	val = 0;
	for (i = 1; i < oid->len; ++i) {
		/* In this loop, we have to parse a DER formatted
		   If the first bit is a 1, then the integer is
		   represented by more than one byte.  If the
		   first bit is set then we continue on and add
		   the values of the later bytes until we get
		   a byte without the first bit set.
		*/
		unsigned long j;

		j = oid->data[i];
		val = (val << 7) | (j & 0x7f);
		if (j & 0x80)
			continue;
		written = PR_snprintf(&buf[len], sizeof(buf)-len, "%lu ", val);
		if (written < 0)
			return FALSE;

		len += written;
		if (len >= sizeof (buf))
			g_warning ("OID data to big to display in 300 chars.");
		val = 0;
  }

  *text = g_strdup (buf);
  return TRUE;
}

static gboolean
get_oid_text (SECItem *oid, gchar **text)
{
	SECOidTag oidTag = SECOID_FindOIDTag(oid);
	gchar *temp;

	switch (oidTag) {
	case SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION:
		*text = g_strdup (_("PKCS #1 MD2 With RSA Encryption"));
		break;
	case SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION:
		*text = g_strdup (_("PKCS #1 MD5 With RSA Encryption"));
		break;
	case SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION:
		*text = g_strdup (_("PKCS #1 SHA-1 With RSA Encryption"));
		break;
	case SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION:
		*text = g_strdup (_("PKCS #1 SHA-256 With RSA Encryption"));
		break;
	case SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION:
		*text = g_strdup (_("PKCS #1 SHA-384 With RSA Encryption"));
		break;
	case SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION:
		*text = g_strdup (_("PKCS #1 SHA-512 With RSA Encryption"));
		break;
	case SEC_OID_AVA_COUNTRY_NAME:
		*text = g_strdup ("C");
		break;
	case SEC_OID_AVA_COMMON_NAME:
		*text = g_strdup ("CN");
		break;
	case SEC_OID_AVA_ORGANIZATIONAL_UNIT_NAME:
		*text = g_strdup ("OU");
		break;
	case SEC_OID_AVA_ORGANIZATION_NAME:
		*text = g_strdup ("O");
		break;
	case SEC_OID_AVA_LOCALITY:
		*text = g_strdup ("L");
		break;
	case SEC_OID_AVA_DN_QUALIFIER:
		*text = g_strdup ("DN");
		break;
	case SEC_OID_AVA_DC:
		*text = g_strdup ("DC");
		break;
	case SEC_OID_AVA_STATE_OR_PROVINCE:
		*text = g_strdup ("ST");
		break;
	case SEC_OID_PKCS1_RSA_ENCRYPTION:
		*text = g_strdup (_("PKCS #1 RSA Encryption"));
		break;
	case SEC_OID_X509_KEY_USAGE:
		*text = g_strdup (_("Certificate Key Usage"));
		break;
	case SEC_OID_NS_CERT_EXT_CERT_TYPE:
		*text = g_strdup (_("Netscape Certificate Type"));
		break;
	case SEC_OID_X509_AUTH_KEY_ID:
		*text = g_strdup (_("Certificate Authority Key Identifier"));
		break;
	case SEC_OID_RFC1274_UID:
		*text = g_strdup ("UID");
		break;
	case SEC_OID_PKCS9_EMAIL_ADDRESS:
		*text = g_strdup ("E");
		break;
	default:
		if (!get_default_oid_format (oid, &temp))
			return FALSE;

		*text = g_strdup_printf (_("Object Identifier (%s)"), temp);
		g_free (temp);

		break;
	}
	return TRUE;
}

static gboolean
process_raw_bytes (SECItem *data, gchar **text)
{
	/* This function is used to display some DER bytes
	   that we have not added support for decoding.
	   It prints the value of the byte out into a
	   string that can later be displayed as a byte
	   string.  We place a new line after 24 bytes
	   to break up extermaly long sequence of bytes.
	*/
	GString *str = g_string_new ("");
	PRUint32 i;
	gchar buffer[5];
	for (i=0; i<data->len; i++) {
		PR_snprintf(buffer, 5, "%02x ", data->data[i]);
		g_string_append (str, buffer);
		if ((i+1)%16 == 0) {
			g_string_append (str, "\n");
		}
	}
	*text = g_string_free (str, FALSE);
	return TRUE;
}

static gboolean
process_sec_algorithm_id (SECAlgorithmID  *algID,
			  EASN1Object    **retSequence)
{
	EASN1Object *sequence = e_asn1_object_new ();
	gchar *text;

	*retSequence = NULL;

	get_oid_text (&algID->algorithm, &text);

	if (!algID->parameters.len ||
		algID->parameters.data[0] == E_ASN1_OBJECT_TYPE_NULL) {
		e_asn1_object_set_display_value (sequence, text);
		e_asn1_object_set_valid_container (sequence, FALSE);
	} else {
		EASN1Object *subitem;

		subitem = e_asn1_object_new ();
		e_asn1_object_set_display_name (subitem, _("Algorithm Identifier"));
		e_asn1_object_set_display_value (subitem, text);
		e_asn1_object_append_child (sequence, subitem);
		g_object_unref (subitem);

		g_free (text);

		subitem = e_asn1_object_new ();
		e_asn1_object_set_display_name (subitem, _("Algorithm Parameters"));
		process_raw_bytes (&algID->parameters, &text);
		e_asn1_object_set_display_value (subitem, text);
		e_asn1_object_append_child (sequence, subitem);
		g_object_unref (subitem);
	}

	g_free (text);
	*retSequence = sequence;
	return TRUE;
}

static gboolean
process_subject_public_key_info (CERTSubjectPublicKeyInfo *spki,
				 EASN1Object *parentSequence)
{
	EASN1Object *spkiSequence = e_asn1_object_new();
	EASN1Object *sequenceItem;
	EASN1Object *printableItem;
	SECItem data;
	gchar *text;

	e_asn1_object_set_display_name (spkiSequence, _("Subject Public Key Info"));

	if (!process_sec_algorithm_id (&spki->algorithm, &sequenceItem))
		return FALSE;

	e_asn1_object_set_display_name (sequenceItem, _("Subject Public Key Algorithm"));

	e_asn1_object_append_child (spkiSequence, sequenceItem);

	/* The subjectPublicKey field is encoded as a bit string.
	   ProcessRawBytes expects the lenght to be in bytes, so
	   let's convert the lenght into a temporary SECItem.
	*/
	data.data = spki->subjectPublicKey.data;
	data.len  = spki->subjectPublicKey.len / 8;

	process_raw_bytes (&data, &text);
	printableItem = e_asn1_object_new ();

	e_asn1_object_set_display_value (printableItem, text);
	e_asn1_object_set_display_name (printableItem, _("Subject's Public Key"));
	e_asn1_object_append_child (spkiSequence, printableItem);
	g_object_unref (printableItem);

	e_asn1_object_append_child (parentSequence, spkiSequence);
	g_object_unref (spkiSequence);

	return TRUE;
}

static gboolean
process_ns_cert_type_extensions (SECItem  *extData,
				 GString *text)
{
	SECItem decoded;
	guchar nsCertType;

	decoded.data = NULL;
	decoded.len  = 0;
	if (SECSuccess != SEC_ASN1DecodeItem(NULL, &decoded,
					     SEC_ASN1_GET(SEC_BitStringTemplate), extData)) {
		g_string_append (text, _("Error: Unable to process extension"));
		return TRUE;
	}

	nsCertType = decoded.data[0];

	PORT_Free (decoded.data); /* XXX right free? */

	if (nsCertType & NS_CERT_TYPE_SSL_CLIENT) {
		g_string_append (text, _("SSL Client Certificate"));
		g_string_append (text, "\n");
	}
	if (nsCertType & NS_CERT_TYPE_SSL_SERVER) {
		g_string_append (text, _("SSL Server Certificate"));
		g_string_append (text, "\n");
	}
	if (nsCertType & NS_CERT_TYPE_EMAIL) {
		g_string_append (text, _("Email"));
		g_string_append (text, "\n");
	}
	if (nsCertType & NS_CERT_TYPE_OBJECT_SIGNING) {
		g_string_append (text, _("Object Signer"));
		g_string_append (text, "\n");
	}
	if (nsCertType & NS_CERT_TYPE_SSL_CA) {
		g_string_append (text, _("SSL Certificate Authority"));
		g_string_append (text, "\n");
	}
	if (nsCertType & NS_CERT_TYPE_EMAIL_CA) {
		g_string_append (text, _("Email Certificate Authority"));
		g_string_append (text, "\n");
	}
	if (nsCertType & NS_CERT_TYPE_OBJECT_SIGNING_CA) {
		g_string_append (text, _("Object Signer"));
		g_string_append (text, "\n");
	}
	return TRUE;
}

static gboolean
process_key_usage_extensions (SECItem *extData, GString *text)
{
	SECItem decoded;
	guchar keyUsage;

	decoded.data = NULL;
	decoded.len  = 0;
	if (SECSuccess != SEC_ASN1DecodeItem(NULL, &decoded,
					     SEC_ASN1_GET(SEC_BitStringTemplate), extData)) {
		g_string_append (text, _("Error: Unable to process extension"));
		return TRUE;
	}

	keyUsage = decoded.data[0];
	PORT_Free (decoded.data); /* XXX right free? */

	if (keyUsage & KU_DIGITAL_SIGNATURE) {
		g_string_append (text, _("Signing"));
		g_string_append (text, "\n");
	}
	if (keyUsage & KU_NON_REPUDIATION) {
		g_string_append (text, _("Non-repudiation"));
		g_string_append (text, "\n");
	}
	if (keyUsage & KU_KEY_ENCIPHERMENT) {
		g_string_append (text, _("Key Encipherment"));
		g_string_append (text, "\n");
	}
	if (keyUsage & KU_DATA_ENCIPHERMENT) {
		g_string_append (text, _("Data Encipherment"));
		g_string_append (text, "\n");
	}
	if (keyUsage & KU_KEY_AGREEMENT) {
		g_string_append (text, _("Key Agreement"));
		g_string_append (text, "\n");
	}
	if (keyUsage & KU_KEY_CERT_SIGN) {
		g_string_append (text, _("Certificate Signer"));
		g_string_append (text, "\n");
	}
	if (keyUsage & KU_CRL_SIGN) {
		g_string_append (text, _("CRL Signer"));
		g_string_append (text, "\n");
	}

	return TRUE;
}

static gboolean
process_extension_data (SECOidTag oidTag, SECItem *extData,
			GString *str)
{
	gboolean rv;
	switch (oidTag) {
	case SEC_OID_NS_CERT_EXT_CERT_TYPE:
		rv = process_ns_cert_type_extensions (extData, str);
		break;
	case SEC_OID_X509_KEY_USAGE:
		rv = process_key_usage_extensions (extData, str);
		break;
	default: {
		gchar *text;
		rv = process_raw_bytes (extData, &text);
		g_string_append (str, text);
		g_free (text);
		break;
	}
	}
	return rv;
}

static gboolean
process_single_extension (CERTCertExtension *extension,
			  EASN1Object **retExtension)
{
	GString *str = g_string_new ("");
	gchar *text;
	EASN1Object *extensionItem;
	SECOidTag oidTag = SECOID_FindOIDTag(&extension->id);

	get_oid_text (&extension->id, &text);

	extensionItem = e_asn1_object_new ();

	e_asn1_object_set_display_name (extensionItem, text);
	g_free (text);

	if (extension->critical.data != NULL) {
		if (extension->critical.data[0]) {
			g_string_append (str, _("Critical"));
		} else {
			g_string_append (str, _("Not Critical"));
		}
	} else {
		g_string_append (str, _("Not Critical"));
	}
	g_string_append (str, "\n");
	if (!process_extension_data (oidTag, &extension->value, str)) {
		g_string_free (str, TRUE);
		return FALSE;
	}

	e_asn1_object_set_display_value (extensionItem, str->str);
	g_string_free (str, TRUE);
	*retExtension = extensionItem;
	return TRUE;
}

static gboolean
process_extensions (CERTCertExtension **extensions,
		    EASN1Object        *parentSequence)
{
	EASN1Object *extensionSequence = e_asn1_object_new ();
	PRInt32 i;

	e_asn1_object_set_display_name (extensionSequence, _("Extensions"));

	for (i=0; extensions[i] != NULL; i++) {
		EASN1Object *newExtension;

		if (!process_single_extension (extensions[i],
					       &newExtension))
			return FALSE;

		e_asn1_object_append_child (extensionSequence, newExtension);
	}
	e_asn1_object_append_child (parentSequence, extensionSequence);
	return TRUE;
}

static gboolean
process_name (CERTName *name, gchar **value)
{
	CERTRDN** rdns;
	CERTRDN** rdn;
	CERTAVA** avas;
	CERTAVA* ava;
	SECItem *decodeItem = NULL;
	GString *final_string = g_string_new ("");

	gchar *type;
	GString *avavalue;
	gchar *temp;
	CERTRDN **lastRdn;

	rdns = name->rdns;

	/* find last RDN */
	lastRdn = rdns;
	while (*lastRdn) lastRdn++;

	/* The above whille loop will put us at the last member
	 * of the array which is a NULL pointer.  So let's back
	 * up one spot so that we have the last non-NULL entry in
	 * the array in preparation for traversing the
	 * RDN's (Relative Distinguished Name) in reverse order.
	 */
	lastRdn--;

	/*
	 * Loop over name contents in _reverse_ RDN order appending to string
	 * When building the Ascii string, NSS loops over these entries in
	 * reverse order, so I will as well.  The difference is that NSS
	 * will always place them in a one line string separated by commas,
	 * where I want each entry on a single line.  I can't just use a comma
	 * as my delimitter because it is a valid character to have in the
	 * value portion of the AVA and could cause trouble when parsing.
	 */
	for (rdn = lastRdn; rdn >= rdns; rdn--) {
		avas = (*rdn)->avas;
		while ((ava = *avas++) != 0) {
			if (!get_oid_text (&ava->type, &type))
				return FALSE;

			/* This function returns a string in UTF8 format. */
			decodeItem = CERT_DecodeAVAValue(&ava->value);
			if (!decodeItem) {
				return FALSE;
			}

			avavalue = g_string_new_len ((gchar *)decodeItem->data, decodeItem->len);

			SECITEM_FreeItem(decodeItem, PR_TRUE);

			/* Translators: This string is used in Certificate details for fields like Issuer
			   or Subject, which shows the field name on the left and its respective value
			   on the right, both as stored in the certificate itself. You probably do not
			   need to change this string, unless changing the order of name and value.
			   As a result example: "OU = VeriSign Trust Network"
			*/
			temp = g_strdup_printf (_("%s = %s"), type, avavalue->str);

			g_string_append (final_string, temp);
			g_string_append (final_string, "\n");
			g_string_free (avavalue, TRUE);
			g_free (temp);
		}
	}
	*value = g_string_free (final_string, FALSE);
	return TRUE;
}

static gboolean
create_tbs_certificate_asn1_struct (ECert *cert, EASN1Object **seq)
{
	/*
	**   TBSCertificate  ::=  SEQUENCE  {
	**        version         [0]  EXPLICIT Version DEFAULT v1,
	**        serialNumber         CertificateSerialNumber,
	**        signature            AlgorithmIdentifier,
	**        issuer               Name,
	**        validity             Validity,
	**        subject              Name,
	**        subjectPublicKeyInfo SubjectPublicKeyInfo,
	**        issuerUniqueID  [1]  IMPLICIT UniqueIdentifier OPTIONAL,
	**                             -- If present, version shall be v2 or v3
	**        subjectUniqueID [2]  IMPLICIT UniqueIdentifier OPTIONAL,
	**                             -- If present, version shall be v2 or v3
	**        extensions      [3]  EXPLICIT Extensions OPTIONAL
	**                             -- If present, version shall be v3
	**        }
	**
	** This is the ASN1 structure we should be dealing with at this point.
	** The code in this method will assert this is the structure we're dealing
	** and then add more user friendly text for that field.
	*/
	EASN1Object *sequence = e_asn1_object_new ();
	gchar *text;
	EASN1Object *subitem;
	SECItem data;

	e_asn1_object_set_display_name (sequence, _("Certificate"));

	if (!process_version (&cert->priv->cert->version, &subitem))
		return FALSE;
	e_asn1_object_append_child (sequence, subitem);
	g_object_unref (subitem);

	if (!process_serial_number_der (&cert->priv->cert->serialNumber, &subitem))
		return FALSE;
	e_asn1_object_append_child (sequence, subitem);
	g_object_unref (subitem);

	if (!process_sec_algorithm_id (&cert->priv->cert->signature, &subitem))
		return FALSE;
	e_asn1_object_set_display_name (subitem, _("Certificate Signature Algorithm"));
	e_asn1_object_append_child (sequence, subitem);
	g_object_unref (subitem);

	process_name (&cert->priv->cert->issuer, &text);
	subitem = e_asn1_object_new ();
	e_asn1_object_set_display_value (subitem, text);
	g_free (text);

	e_asn1_object_set_display_name (subitem, _("Issuer"));
	e_asn1_object_append_child (sequence, subitem);
	g_object_unref (subitem);

#ifdef notyet
	nsCOMPtr<nsIASN1Sequence> validitySequence = new nsNSSASN1Sequence();
	nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpValidity").get(),
					    text);
	validitySequence->SetDisplayName(text);
	asn1Objects->AppendElement(validitySequence, PR_FALSE);
	nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpNotBefore").get(),
					    text);
	nsCOMPtr<nsIX509CertValidity> validityData;
	GetValidity(getter_AddRefs(validityData));
	PRTime notBefore, notAfter;

	validityData->GetNotBefore(&notBefore);
	validityData->GetNotAfter(&notAfter);
	validityData = 0;
	rv = ProcessTime(notBefore, text.get(), validitySequence);
	if (NS_FAILED(rv))
		return rv;

	nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpNotAfter").get(),
					    text);
	rv = ProcessTime(notAfter, text.get(), validitySequence);
	if (NS_FAILED(rv))
		return rv;
#endif

	subitem = e_asn1_object_new ();
	e_asn1_object_set_display_name (subitem, _("Subject"));

	process_name (&cert->priv->cert->subject, &text);
	e_asn1_object_set_display_value (subitem, text);
	g_free (text);
	e_asn1_object_append_child (sequence, subitem);
	g_object_unref (subitem);

	if (!process_subject_public_key_info (
		&cert->priv->cert->subjectPublicKeyInfo, sequence))
		return FALSE;

	/* Is there an issuerUniqueID? */
	if (cert->priv->cert->issuerID.data) {
		/* The issuerID is encoded as a bit string.
		   The function ProcessRawBytes expects the
		   length to be in bytes, so let's convert the
		   length in a temporary SECItem
		*/
		data.data = cert->priv->cert->issuerID.data;
		data.len  = cert->priv->cert->issuerID.len / 8;

		subitem = e_asn1_object_new ();

		e_asn1_object_set_display_name (subitem, _("Issuer Unique ID"));
		process_raw_bytes (&data, &text);
		e_asn1_object_set_display_value (subitem, text);
		g_free (text);

		e_asn1_object_append_child (sequence, subitem);
	}

	if (cert->priv->cert->subjectID.data) {
		/* The subjectID is encoded as a bit string.
		   The function ProcessRawBytes expects the
		   length to be in bytes, so let's convert the
		   length in a temporary SECItem
		*/
		data.data = cert->priv->cert->issuerID.data;
		data.len  = cert->priv->cert->issuerID.len / 8;

		subitem = e_asn1_object_new ();

		e_asn1_object_set_display_name (subitem, _("Subject Unique ID"));
		process_raw_bytes (&data, &text);
		e_asn1_object_set_display_value (subitem, text);
		g_free (text);

		e_asn1_object_append_child (sequence, subitem);
	}
	if (cert->priv->cert->extensions) {
		if (!process_extensions (cert->priv->cert->extensions, sequence))
			return FALSE;
	}

	*seq = sequence;

	return TRUE;
}

static gboolean
create_asn1_struct (ECert *cert)
{
	EASN1Object *sequence;
	SECItem temp;
	gchar *text;

	cert->priv->asn1 = e_asn1_object_new ();

	e_asn1_object_set_display_name (cert->priv->asn1, e_cert_get_window_title (cert));

	/* This sequence will be contain the tbsCertificate, signatureAlgorithm,
	   and signatureValue. */

	if (!create_tbs_certificate_asn1_struct (cert, &sequence))
		return FALSE;
	e_asn1_object_append_child (cert->priv->asn1, sequence);
	g_object_unref (sequence);

	if (!process_sec_algorithm_id (
		&cert->priv->cert->signatureWrap.signatureAlgorithm, &sequence))
		return FALSE;
	e_asn1_object_set_display_name (
		sequence, _("Certificate Signature Algorithm"));
	e_asn1_object_append_child (cert->priv->asn1, sequence);
	g_object_unref (sequence);

	sequence = e_asn1_object_new ();
	e_asn1_object_set_display_name (
		sequence, _("Certificate Signature Value"));

	/* The signatureWrap is encoded as a bit string.
	   The function ProcessRawBytes expects the
	   length to be in bytes, so let's convert the
	   length in a temporary SECItem */
	temp.data = cert->priv->cert->signatureWrap.signature.data;
	temp.len  = cert->priv->cert->signatureWrap.signature.len / 8;
	process_raw_bytes (&temp, &text);
	e_asn1_object_set_display_value (sequence, text);
	e_asn1_object_append_child (cert->priv->asn1, sequence);
	g_free (text);

	return TRUE;
}

EASN1Object*
e_cert_get_asn1_struct (ECert *cert)
{
	if (!cert->priv->asn1)
		create_asn1_struct (cert);

	return g_object_ref (cert->priv->asn1);
}

gboolean
e_cert_mark_for_deletion (ECert *cert)
{
	/* nsNSSShutDownPreventionLock locker; */

#if 0
	/* make sure user is logged in to the token */
	nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();
#endif

	if (PK11_NeedLogin(cert->priv->cert->slot)
	    && !PK11_NeedUserInit(cert->priv->cert->slot)
	    && !PK11_IsInternal(cert->priv->cert->slot)) {
		if (SECSuccess != PK11_Authenticate(cert->priv->cert->slot, PR_TRUE, NULL)) {
			return FALSE;
		}
	}

	cert->priv->delete = TRUE;

	return TRUE;
}

ECertType
e_cert_get_cert_type (ECert *ecert)
{
	const gchar *nick = e_cert_get_nickname (ecert);
	const gchar *email = e_cert_get_email (ecert);
	CERTCertificate *cert = ecert->priv->cert;

	if (nick) {
		if (e_cert_trust_has_any_user (cert->trust))
			return E_CERT_USER;
		if (e_cert_trust_has_any_ca (cert->trust)
		    || CERT_IsCACert(cert,NULL))
			return E_CERT_CA;
		if (e_cert_trust_has_peer (cert->trust, PR_TRUE, PR_FALSE, PR_FALSE))
			return E_CERT_SITE;
	}
	if (email && e_cert_trust_has_peer (cert->trust, PR_FALSE, PR_TRUE, PR_FALSE))
		return E_CERT_CONTACT;

	return E_CERT_UNKNOWN;
}
