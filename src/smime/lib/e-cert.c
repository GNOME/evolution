/* The following is the mozilla license blurb, as the bodies some of
 * these functions were derived from the mozilla source. */
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

#include "evolution-config.h"

#include <time.h>

#include <glib/gi18n.h>

/* for e_utf8_strftime, what about e_time_format_time? */
#include <e-util/e-util.h>

#include "e-cert.h"
#include "e-cert-trust.h"
#include "pk11func.h"
#include "certdb.h"
#include "hasht.h"

struct _ECertPrivate {
	CERTCertificate *cert;

	/* pointers we cache since the nss implementation allocs the
	 * string */
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

	gchar *sha256_fingerprint;
	gchar *sha1_fingerprint;
	gchar *md5_fingerprint;

	gboolean delete;
};

G_DEFINE_TYPE_WITH_PRIVATE (ECert, e_cert, G_TYPE_OBJECT)

static void
e_cert_finalize (GObject *object)
{
	ECert *self = E_CERT (object);

	if (self->priv->org_name)
		PORT_Free (self->priv->org_name);
	if (self->priv->org_unit_name)
		PORT_Free (self->priv->org_unit_name);
	if (self->priv->cn)
		PORT_Free (self->priv->cn);

	if (self->priv->issuer_org_name)
		PORT_Free (self->priv->issuer_org_name);
	if (self->priv->issuer_org_unit_name)
		PORT_Free (self->priv->issuer_org_unit_name);
	if (self->priv->issuer_cn)
		PORT_Free (self->priv->issuer_cn);

	g_free (self->priv->issued_on_string);
	g_free (self->priv->expires_on_string);

	if (self->priv->serial_number)
		PORT_Free (self->priv->serial_number);

	g_free (self->priv->usage_string);

	if (self->priv->sha256_fingerprint)
		PORT_Free (self->priv->sha256_fingerprint);
	if (self->priv->sha1_fingerprint)
		PORT_Free (self->priv->sha1_fingerprint);
	if (self->priv->md5_fingerprint)
		PORT_Free (self->priv->md5_fingerprint);

	if (self->priv->delete) {
		printf ("attempting to delete cert marked for deletion\n");
		if (e_cert_get_cert_type (E_CERT (object)) == E_CERT_USER) {
			PK11_DeleteTokenCertAndKey (self->priv->cert, NULL);
		} else if (!PK11_IsReadOnly (self->priv->cert->slot)) {
			/* If the list of built-ins does contain a non-removable
			 * copy of this certificate, our call will not remove
			 * the certificate permanently, but rather remove all trust. */
			SEC_DeletePermCertificate (self->priv->cert);
		}
	}

	if (self->priv->cert)
		CERT_DestroyCertificate (self->priv->cert);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_cert_parent_class)->finalize (object);
}

static void
e_cert_class_init (ECertClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = e_cert_finalize;
}

static void
e_cert_init (ECert *ec)
{
	ec->priv = e_cert_get_instance_private (ec);
}

static void
e_cert_populate (ECert *cert)
{
	CERTCertificate *c = cert->priv->cert;
	guchar fingerprint[MAX (SHA256_LENGTH, MAX (SHA1_LENGTH, MD5_LENGTH)) + 1];
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

		memset (&exploded_tm, 0, sizeof (struct tm));

		PR_ExplodeTime (
			cert->priv->issued_on,
			PR_LocalTimeParameters, &explodedTime);
		exploded_tm.tm_sec = explodedTime.tm_sec;
		exploded_tm.tm_min = explodedTime.tm_min;
		exploded_tm.tm_hour = explodedTime.tm_hour;
		exploded_tm.tm_mday = explodedTime.tm_mday;
		exploded_tm.tm_mon = explodedTime.tm_month;
		exploded_tm.tm_year = explodedTime.tm_year - 1900;
		cert->priv->issued_on_string = e_datetime_format_format_tm ("mail", "table", DTFormatKindDate, &exploded_tm);

		PR_ExplodeTime (
			cert->priv->expires_on,
			PR_LocalTimeParameters, &explodedTime);
		exploded_tm.tm_sec = explodedTime.tm_sec;
		exploded_tm.tm_min = explodedTime.tm_min;
		exploded_tm.tm_hour = explodedTime.tm_hour;
		exploded_tm.tm_mday = explodedTime.tm_mday;
		exploded_tm.tm_mon = explodedTime.tm_month;
		exploded_tm.tm_year = explodedTime.tm_year - 1900;
		cert->priv->expires_on_string = e_datetime_format_format_tm ("mail", "table", DTFormatKindDate, &exploded_tm);
	}

	cert->priv->serial_number = CERT_Hexify (&cert->priv->cert->serialNumber, TRUE);

	memset (fingerprint, 0, sizeof fingerprint);
	PK11_HashBuf (
		SEC_OID_SHA256, fingerprint,
		cert->priv->cert->derCert.data,
		cert->priv->cert->derCert.len);
	fpItem.data = fingerprint;
	fpItem.len = SHA256_LENGTH;
	cert->priv->sha256_fingerprint = CERT_Hexify (&fpItem, TRUE);

	memset (fingerprint, 0, sizeof fingerprint);
	PK11_HashBuf (
		SEC_OID_SHA1, fingerprint,
		cert->priv->cert->derCert.data,
		cert->priv->cert->derCert.len);
	fpItem.data = fingerprint;
	fpItem.len = SHA1_LENGTH;
	cert->priv->sha1_fingerprint = CERT_Hexify (&fpItem, TRUE);

	memset (fingerprint, 0, sizeof fingerprint);
	PK11_HashBuf (
		SEC_OID_MD5, fingerprint,
		cert->priv->cert->derCert.data,
		cert->priv->cert->derCert.len);
	fpItem.data = fingerprint;
	fpItem.len = MD5_LENGTH;
	cert->priv->md5_fingerprint = CERT_Hexify (&fpItem, TRUE);
}

ECert *
e_cert_new (CERTCertificate *cert)
{
	ECert *ecert = E_CERT (g_object_new (E_TYPE_CERT, NULL));

	/* ECert owns a reference to the 'cert', which will be freed on ECert finalize */
	ecert->priv->cert = cert;

	e_cert_populate (ecert);

	return ecert;
}

ECert *
e_cert_new_from_der (gchar *data,
                     guint32 len)
{
	CERTCertificate *cert = CERT_DecodeCertFromPackage (data, len);

	if (!cert)
		return NULL;

	if (cert->dbhandle == NULL)
		cert->dbhandle = CERT_GetDefaultCertDB ();

	return e_cert_new (cert);
}

CERTCertificate *
e_cert_get_internal_cert (ECert *cert)
{
	/* XXX should this refcnt it? */
	return cert->priv->cert;
}

gboolean
e_cert_get_raw_der (ECert *cert,
                    gchar **data,
                    guint32 *len)
{
	/* XXX do we really need to check if cert->priv->cert is NULL
	 * here?  it should always be non - null if we have the
	 * ECert.. */
	if (cert->priv->cert) {
		*data = (gchar *)cert->priv->cert->derCert.data;
		*len = (guint32)cert->priv->cert->derCert.len;
		return TRUE;
	}

	*len = 0;
	return FALSE;

}

const gchar *
e_cert_get_nickname (ECert *cert)
{
	return cert->priv->cert->nickname;
}

const gchar *
e_cert_get_email (ECert *cert)
{
	return cert->priv->cert->emailAddr;
}

const gchar *
e_cert_get_org (ECert *cert)
{
	return cert->priv->org_name;
}

const gchar *
e_cert_get_org_unit (ECert *cert)
{
	return cert->priv->org_unit_name;
}

const gchar *
e_cert_get_cn (ECert *cert)
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

const gchar *
e_cert_get_issued_on (ECert *cert)
{
	return cert->priv->issued_on_string;
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
	{ KU_DIGITAL_SIGNATURE, N_("Sign") },
	{ KU_KEY_ENCIPHERMENT | KU_DATA_ENCIPHERMENT, N_("Encrypt") },
};

const gchar *
e_cert_get_usage (ECert *cert)
{
	if (cert->priv->usage_string == NULL) {
		gint i;
		GString *str = g_string_new ("");
		CERTCertificate *icert = e_cert_get_internal_cert (cert);

		for (i = 0; i < G_N_ELEMENTS (usageinfo); i++) {
			if (icert->keyUsage & usageinfo[i].bit) {
				if (str->len != 0)
					g_string_append (str, ", ");
				g_string_append (str, _(usageinfo[i].text));
			}
		}

		cert->priv->usage_string = g_string_free (str, FALSE);
	}

	return cert->priv->usage_string;
}

const gchar *
e_cert_get_serial_number (ECert *cert)
{
	return cert->priv->serial_number;
}

const gchar *
e_cert_get_sha256_fingerprint (ECert *cert)
{
	return cert->priv->sha256_fingerprint;
}

const gchar *
e_cert_get_sha1_fingerprint (ECert *cert)
{
	return cert->priv->sha1_fingerprint;
}

const gchar *
e_cert_get_md5_fingerprint (ECert *cert)
{
	return cert->priv->md5_fingerprint;
}

ECert *
e_cert_get_ca_cert (ECert *ecert)
{
	CERTCertificate *cert, *next = e_cert_get_internal_cert (ecert), *internal;

	cert = next;
	internal = cert;
	do {
		if (cert != next && cert != internal)
			CERT_DestroyCertificate (cert);

		cert = next;
		next = CERT_FindCertIssuer (cert, PR_Now (), certUsageAnyCA);
	} while (next && next != cert);

	if (cert == internal)
		return g_object_ref (ecert);
	else
		return e_cert_new (cert);
}

gboolean
e_cert_mark_for_deletion (ECert *cert)
{
	if (PK11_NeedLogin (cert->priv->cert->slot)
	    && !PK11_NeedUserInit (cert->priv->cert->slot)
	    && !PK11_IsInternal (cert->priv->cert->slot)) {
		if (PK11_Authenticate (cert->priv->cert->slot, PR_TRUE, NULL) != SECSuccess) {
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
		    || CERT_IsCACert (cert,NULL))
			return E_CERT_CA;
		if (e_cert_trust_has_peer (cert->trust, PR_TRUE, PR_FALSE, PR_FALSE))
			return E_CERT_SITE;
	}
	if (email && e_cert_trust_has_peer (cert->trust, PR_FALSE, PR_TRUE, PR_FALSE))
		return E_CERT_CONTACT;

	return E_CERT_UNKNOWN;
}
