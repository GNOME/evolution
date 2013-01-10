/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "e-asn1-object.h"

#include "pk11func.h"
#include "certdb.h"
#include "hasht.h"

#define E_ASN1_OBJECT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ASN1_OBJECT, EASN1ObjectPrivate))

struct _EASN1ObjectPrivate {
	PRUint32 tag;
	PRUint32 type;
	gboolean valid_container;

	GList *children;

	gchar *display_name;
	gchar *value;

	gchar *data;
	guint data_len;
};

G_DEFINE_TYPE (EASN1Object, e_asn1_object, G_TYPE_OBJECT)

static gboolean
get_int_value (SECItem *versionItem,
               gulong *version)
{
	SECStatus srv;
	srv = SEC_ASN1DecodeInteger (versionItem,version);
	if (srv != SECSuccess) {
		g_warning ("could not decode version of cert");
		return FALSE;
	}
	return TRUE;
}

static gboolean
process_version (SECItem *versionItem,
                 EASN1Object **retItem)
{
	EASN1Object *item = e_asn1_object_new ();
	gulong version;

	e_asn1_object_set_display_name (item, _("Version"));

	/* Now to figure out what version this certificate is. */

	if (versionItem->data) {
		if (!get_int_value (versionItem, &version))
			return FALSE;
	} else {
		/* If there is no version present in the cert, then rfc2459
		 * says we default to v1 (0) */
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
process_serial_number_der (SECItem *serialItem,
                           EASN1Object **retItem)
{
	gchar *serialNumber;
	EASN1Object *item = e_asn1_object_new ();

	e_asn1_object_set_display_name (item, _("Serial Number"));

	serialNumber = CERT_Hexify (serialItem, 1);

	e_asn1_object_set_display_value (item, serialNumber);
	PORT_Free (serialNumber); /* XXX the right free to use? */

	*retItem = item;
	return TRUE;
}

static gboolean
get_default_oid_format (SECItem *oid,
                        gchar **text)
{
	GString *str;
	gulong val = oid->data[0];
	guint ii = val % 40;

	val /= 40;

	str = g_string_new ("");
	g_string_append_printf (str, "%lu %u ", val, ii);

	val = 0;
	for (ii = 1; ii < oid->len; ii++) {
		/* In this loop, we have to parse a DER formatted
		 * If the first bit is a 1, then the integer is
		 * represented by more than one byte.  If the
		 * first bit is set then we continue on and add
		 * the values of the later bytes until we get
		 * a byte without the first bit set.
		*/
		gulong jj;

		jj = oid->data[ii];
		val = (val << 7) | (jj & 0x7f);
		if (jj & 0x80)
			continue;
		g_string_append_printf (str, "%lu ", val);

		val = 0;
  }

  *text = g_string_free (str, FALSE);

  return TRUE;
}

static gboolean
get_oid_text (SECItem *oid,
              gchar **text)
{
	SECOidTag oidTag = SECOID_FindOIDTag (oid);
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
process_raw_bytes (SECItem *data,
                   gchar **text)
{
	/* This function is used to display some DER bytes
	 * that we have not added support for decoding.
	 * It prints the value of the byte out into a
	 * string that can later be displayed as a byte
	 * string.  We place a new line after 24 bytes
	 * to break up extermaly long sequence of bytes.
	*/
	GString *str = g_string_new ("");
	PRUint32 i;

	for (i = 0; i < data->len; i++) {
		g_string_append_printf (str, "%02x ", data->data[i]);
		if ((i + 1) % 16 == 0) {
			g_string_append (str, "\n");
		}
	}
	*text = g_string_free (str, FALSE);
	return TRUE;
}

static gboolean
process_sec_algorithm_id (SECAlgorithmID *algID,
                          EASN1Object **retSequence)
{
	EASN1Object *sequence = e_asn1_object_new ();
	gchar *text = NULL;

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
	EASN1Object *spkiSequence = e_asn1_object_new ();
	EASN1Object *sequenceItem;
	EASN1Object *printableItem;
	SECItem data;
	gchar *text = NULL;

	e_asn1_object_set_display_name (spkiSequence, _("Subject Public Key Info"));

	if (!process_sec_algorithm_id (&spki->algorithm, &sequenceItem))
		return FALSE;

	e_asn1_object_set_display_name (sequenceItem, _("Subject Public Key Algorithm"));

	e_asn1_object_append_child (spkiSequence, sequenceItem);

	/* The subjectPublicKey field is encoded as a bit string.
	 * ProcessRawBytes expects the lenght to be in bytes, so
	 * let's convert the lenght into a temporary SECItem.
	*/
	data.data = spki->subjectPublicKey.data;
	data.len  = spki->subjectPublicKey.len / 8;

	process_raw_bytes (&data, &text);
	printableItem = e_asn1_object_new ();

	e_asn1_object_set_display_value (printableItem, text);
	e_asn1_object_set_display_name (printableItem, _("Subject's Public Key"));
	e_asn1_object_append_child (spkiSequence, printableItem);
	g_object_unref (printableItem);
	g_free (text);

	e_asn1_object_append_child (parentSequence, spkiSequence);
	g_object_unref (spkiSequence);

	return TRUE;
}

static gboolean
process_ns_cert_type_extensions (SECItem *extData,
                                 GString *text)
{
	SECItem decoded;
	guchar nsCertType;

	decoded.data = NULL;
	decoded.len  = 0;
	if (SECSuccess != SEC_ASN1DecodeItem (NULL, &decoded,
					     SEC_ASN1_GET (SEC_BitStringTemplate), extData)) {
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
process_key_usage_extensions (SECItem *extData,
                              GString *text)
{
	SECItem decoded;
	guchar keyUsage;

	decoded.data = NULL;
	decoded.len  = 0;
	if (SECSuccess != SEC_ASN1DecodeItem (NULL, &decoded,
					     SEC_ASN1_GET (SEC_BitStringTemplate), extData)) {
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
process_extension_data (SECOidTag oidTag,
                        SECItem *extData,
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
	SECOidTag oidTag = SECOID_FindOIDTag (&extension->id);

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
                    EASN1Object *parentSequence)
{
	EASN1Object *extensionSequence = e_asn1_object_new ();
	PRInt32 i;

	e_asn1_object_set_display_name (extensionSequence, _("Extensions"));

	for (i = 0; extensions[i] != NULL; i++) {
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
process_name (CERTName *name,
              gchar **value)
{
	CERTRDN ** rdns;
	CERTRDN ** rdn;
	CERTAVA ** avas;
	CERTAVA * ava;
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
			decodeItem = CERT_DecodeAVAValue (&ava->value);
			if (!decodeItem) {
				g_free (type);
				return FALSE;
			}

			avavalue = g_string_new_len (
				(gchar *) decodeItem->data, decodeItem->len);

			SECITEM_FreeItem (decodeItem, PR_TRUE);

			/* Translators: This string is used in Certificate
			 * details for fields like Issuer or Subject, which
			 * shows the field name on the left and its respective
			 * value on the right, both as stored in the
			 * certificate itself.  You probably do not need to
			 * change this string, unless changing the order of
			 * name and value.  As a result example:
			 * "OU = VeriSign Trust Network" */
			temp = g_strdup_printf (_("%s = %s"), type, avavalue->str);

			g_string_append (final_string, temp);
			g_string_append (final_string, "\n");
			g_string_free (avavalue, TRUE);
			g_free (temp);
			g_free (type);
		}
	}
	*value = g_string_free (final_string, FALSE);
	return TRUE;
}

static gboolean
create_tbs_certificate_asn1_struct (CERTCertificate *cert,
                                    EASN1Object **seq)
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

	if (!process_version (&cert->version, &subitem))
		return FALSE;
	e_asn1_object_append_child (sequence, subitem);
	g_object_unref (subitem);

	if (!process_serial_number_der (&cert->serialNumber, &subitem))
		return FALSE;
	e_asn1_object_append_child (sequence, subitem);
	g_object_unref (subitem);

	if (!process_sec_algorithm_id (&cert->signature, &subitem))
		return FALSE;
	e_asn1_object_set_display_name (subitem, _("Certificate Signature Algorithm"));
	e_asn1_object_append_child (sequence, subitem);
	g_object_unref (subitem);

	process_name (&cert->issuer, &text);
	subitem = e_asn1_object_new ();
	e_asn1_object_set_display_value (subitem, text);
	g_free (text);

	e_asn1_object_set_display_name (subitem, _("Issuer"));
	e_asn1_object_append_child (sequence, subitem);
	g_object_unref (subitem);

#ifdef notyet
	nsCOMPtr < nsIASN1Sequence> validitySequence = new nsNSSASN1Sequence ();
	nssComponent->GetPIPNSSBundleString (NS_LITERAL_STRING ("CertDumpValidity").get (),
					    text);
	validitySequence->SetDisplayName (text);
	asn1Objects->AppendElement (validitySequence, PR_FALSE);
	nssComponent->GetPIPNSSBundleString (NS_LITERAL_STRING ("CertDumpNotBefore").get (),
					    text);
	nsCOMPtr < nsIX509CertValidity> validityData;
	GetValidity (getter_AddRefs (validityData));
	PRTime notBefore, notAfter;

	validityData->GetNotBefore (&notBefore);
	validityData->GetNotAfter (&notAfter);
	validityData = 0;
	rv = ProcessTime (notBefore, text.get (), validitySequence);
	if (NS_FAILED (rv))
		return rv;

	nssComponent->GetPIPNSSBundleString (NS_LITERAL_STRING ("CertDumpNotAfter").get (),
					    text);
	rv = ProcessTime (notAfter, text.get (), validitySequence);
	if (NS_FAILED (rv))
		return rv;
#endif

	subitem = e_asn1_object_new ();
	e_asn1_object_set_display_name (subitem, _("Subject"));

	process_name (&cert->subject, &text);
	e_asn1_object_set_display_value (subitem, text);
	g_free (text);
	e_asn1_object_append_child (sequence, subitem);
	g_object_unref (subitem);

	if (!process_subject_public_key_info (&cert->subjectPublicKeyInfo, sequence))
		return FALSE;

	/* Is there an issuerUniqueID? */
	if (cert->issuerID.data) {
		/* The issuerID is encoded as a bit string.
		 * The function ProcessRawBytes expects the
		 * length to be in bytes, so let's convert the
		 * length in a temporary SECItem
		*/
		data.data = cert->issuerID.data;
		data.len  = cert->issuerID.len / 8;

		subitem = e_asn1_object_new ();

		e_asn1_object_set_display_name (subitem, _("Issuer Unique ID"));
		process_raw_bytes (&data, &text);
		e_asn1_object_set_display_value (subitem, text);
		g_free (text);

		e_asn1_object_append_child (sequence, subitem);
	}

	if (cert->subjectID.data) {
		/* The subjectID is encoded as a bit string.
		 * The function ProcessRawBytes expects the
		 * length to be in bytes, so let's convert the
		 * length in a temporary SECItem
		*/
		data.data = cert->issuerID.data;
		data.len  = cert->issuerID.len / 8;

		subitem = e_asn1_object_new ();

		e_asn1_object_set_display_name (subitem, _("Subject Unique ID"));
		process_raw_bytes (&data, &text);
		e_asn1_object_set_display_value (subitem, text);
		g_free (text);

		e_asn1_object_append_child (sequence, subitem);
	}
	if (cert->extensions) {
		if (!process_extensions (cert->extensions, sequence))
			return FALSE;
	}

	*seq = sequence;

	return TRUE;
}

static gboolean
fill_asn1_from_cert (EASN1Object *asn1,
                     CERTCertificate *cert)
{
	EASN1Object *sequence;
	SECItem temp;
	gchar *text;

	g_return_val_if_fail (asn1 != NULL, FALSE);
	g_return_val_if_fail (cert != NULL, FALSE);

	if (cert->nickname) {
		e_asn1_object_set_display_name (asn1, cert->nickname);
	} else {
		gchar *str;

		str = CERT_GetCommonName (&cert->subject);
		if (str) {
			e_asn1_object_set_display_name (asn1, str);
			PORT_Free (str);
		} else {
			e_asn1_object_set_display_name (asn1, cert->subjectName);
		}
	}

	/* This sequence will be contain the tbsCertificate, signatureAlgorithm,
	 * and signatureValue. */

	if (!create_tbs_certificate_asn1_struct (cert, &sequence))
		return FALSE;
	e_asn1_object_append_child (asn1, sequence);
	g_object_unref (sequence);

	if (!process_sec_algorithm_id (&cert->signatureWrap.signatureAlgorithm, &sequence))
		return FALSE;
	e_asn1_object_set_display_name (
		sequence, _("Certificate Signature Algorithm"));
	e_asn1_object_append_child (asn1, sequence);
	g_object_unref (sequence);

	sequence = e_asn1_object_new ();
	e_asn1_object_set_display_name (
		sequence, _("Certificate Signature Value"));

	/* The signatureWrap is encoded as a bit string.
	 * The function ProcessRawBytes expects the
	 * length to be in bytes, so let's convert the
	 * length in a temporary SECItem */
	temp.data = cert->signatureWrap.signature.data;
	temp.len  = cert->signatureWrap.signature.len / 8;
	process_raw_bytes (&temp, &text);
	e_asn1_object_set_display_value (sequence, text);
	e_asn1_object_append_child (asn1, sequence);
	g_free (text);

	return TRUE;
}

static void
e_asn1_object_finalize (GObject *object)
{
	EASN1ObjectPrivate *priv;

	priv = E_ASN1_OBJECT_GET_PRIVATE (object);

	g_free (priv->display_name);
	g_free (priv->value);

	g_list_free_full (priv->children, (GDestroyNotify) g_object_unref);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_asn1_object_parent_class)->finalize (object);
}

static void
e_asn1_object_class_init (EASN1ObjectClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EASN1ObjectPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = e_asn1_object_finalize;
}

static void
e_asn1_object_init (EASN1Object *asn1)
{
	asn1->priv = E_ASN1_OBJECT_GET_PRIVATE (asn1);

	asn1->priv->valid_container = TRUE;
}

EASN1Object *
e_asn1_object_new (void)
{
	return E_ASN1_OBJECT (g_object_new (E_TYPE_ASN1_OBJECT, NULL));
}

EASN1Object *
e_asn1_object_new_from_cert (CERTCertificate *cert)
{
	EASN1Object *asn1;

	g_return_val_if_fail (cert != NULL, NULL);

	asn1 = e_asn1_object_new ();
	if (!fill_asn1_from_cert (asn1, cert)) {
		g_object_unref (asn1);
		return NULL;
	}

	return asn1;
}

void
e_asn1_object_set_valid_container (EASN1Object *obj,
                                   gboolean flag)
{
	obj->priv->valid_container = flag;
}

gboolean
e_asn1_object_is_valid_container (EASN1Object *obj)
{
	return obj->priv->valid_container;
}

PRUint32
e_asn1_object_get_asn1_type (EASN1Object *obj)
{
	return obj->priv->type;
}

PRUint32
e_asn1_object_get_asn1_tag (EASN1Object *obj)
{
	return obj->priv->tag;
}

GList *
e_asn1_object_get_children (EASN1Object *obj)
{
	GList *children = g_list_copy (obj->priv->children);

	g_list_foreach (children, (GFunc) g_object_ref, NULL);

	return children;
}

void
e_asn1_object_append_child (EASN1Object *parent,
                            EASN1Object *child)
{
	parent->priv->children = g_list_append (
		parent->priv->children, g_object_ref (child));
}

void
e_asn1_object_set_display_name (EASN1Object *obj,
                                const gchar *name)
{
	g_free (obj->priv->display_name);
	obj->priv->display_name = g_strdup (name);
}

const gchar *
e_asn1_object_get_display_name (EASN1Object *obj)
{
	return obj->priv->display_name;
}

void
e_asn1_object_set_display_value (EASN1Object *obj,
                                 const gchar *value)
{
	g_free (obj->priv->value);
	obj->priv->value = g_strdup (value);
}

const gchar *
e_asn1_object_get_display_value (EASN1Object *obj)
{
	return obj->priv->value;
}

void
e_asn1_object_get_data (EASN1Object *obj,
                        gchar **data,
                        guint32 *len)
{
	*data = obj->priv->data;
	*len = obj->priv->data_len;
}
