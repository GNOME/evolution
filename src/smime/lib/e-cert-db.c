/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* The following is the mozilla license blurb, as the bodies some of
 * these functions were derived from the mozilla source. */
/* e-cert-db.c
 *
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

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <camel/camel.h>

/* private NSS defines used by PSM */
/* (must be declated before cert.h) */
#define CERT_NewTempCertificate __CERT_NewTempCertificate
#define CERT_AddTempCertToPerm __CERT_AddTempCertToPerm

#include "e-cert-db.h"
#include "e-cert-trust.h"
#include "e-pkcs12.h"

#include "gmodule.h"

#include "nss.h"
#include "ssl.h"
#include "p12plcy.h"
#include "pk11func.h"
#include "nssckbi.h"
#include <secerr.h>
#include "secmod.h"
#include "certdb.h"
#include "plstr.h"
#include "prprf.h"
#include "prmem.h"
#include "e-util/e-util.h"
#include "e-util/e-util-private.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

enum {
	PK11_PASSWD,
	PK11_CHANGE_PASSWD,
	CONFIRM_CA_CERT_IMPORT,
	LAST_SIGNAL
};

static guint e_cert_db_signals[LAST_SIGNAL];

G_DEFINE_TYPE (ECertDB, e_cert_db, G_TYPE_OBJECT)

GQuark
e_certdb_error_quark (void)
{
	static GQuark q = 0;
	if (q == 0)
		q = g_quark_from_static_string ("e-certdb-error-quark");

	return q;
}

static const gchar *
nss_error_to_string (glong errorcode)
{
#define cs(a,b) case a: return b;

	switch (errorcode) {
	cs (SEC_ERROR_IO, "An I/O error occurred during security authorization.")
	cs (SEC_ERROR_LIBRARY_FAILURE, "security library failure.")
	cs (SEC_ERROR_BAD_DATA, "security library: received bad data.")
	cs (SEC_ERROR_OUTPUT_LEN, "security library: output length error.")
	cs (SEC_ERROR_INPUT_LEN, "security library has experienced an input length error.")
	cs (SEC_ERROR_INVALID_ARGS, "security library: invalid arguments.")
	cs (SEC_ERROR_INVALID_ALGORITHM, "security library: invalid algorithm.")
	cs (SEC_ERROR_INVALID_AVA, "security library: invalid AVA.")
	cs (SEC_ERROR_INVALID_TIME, "Improperly formatted time string.")
	cs (SEC_ERROR_BAD_DER, "security library: improperly formatted DER-encoded message.")
	cs (SEC_ERROR_BAD_SIGNATURE, "Peer's certificate has an invalid signature.")
	cs (SEC_ERROR_EXPIRED_CERTIFICATE, "Peer's Certificate has expired.")
	cs (SEC_ERROR_REVOKED_CERTIFICATE, "Peer's Certificate has been revoked.")
	cs (SEC_ERROR_UNKNOWN_ISSUER, "Peer's Certificate issuer is not recognized.")
	cs (SEC_ERROR_BAD_KEY, "Peer's public key is invalid.")
	cs (SEC_ERROR_BAD_PASSWORD, "The security password entered is incorrect.")
	cs (SEC_ERROR_RETRY_PASSWORD, "New password entered incorrectly.  Please try again.")
	cs (SEC_ERROR_NO_NODELOCK, "security library: no nodelock.")
	cs (SEC_ERROR_BAD_DATABASE, "security library: bad database.")
	cs (SEC_ERROR_NO_MEMORY, "security library: memory allocation failure.")
	cs (SEC_ERROR_UNTRUSTED_ISSUER, "Peer's certificate issuer has been marked as not trusted by the user.")
	cs (SEC_ERROR_UNTRUSTED_CERT, "Peer's certificate has been marked as not trusted by the user.")
	cs (SEC_ERROR_DUPLICATE_CERT, "Certificate already exists in your database.")
	cs (SEC_ERROR_DUPLICATE_CERT_NAME, "Downloaded certificate's name duplicates one already in your database.")
	cs (SEC_ERROR_ADDING_CERT, "Error adding certificate to database.")
	cs (SEC_ERROR_FILING_KEY, "Error refiling the key for this certificate.")
	cs (SEC_ERROR_NO_KEY, "The private key for this certificate cannot be found in key database")
	cs (SEC_ERROR_CERT_VALID, "This certificate is valid.")
	cs (SEC_ERROR_CERT_NOT_VALID, "This certificate is not valid.")
	cs (SEC_ERROR_CERT_NO_RESPONSE, "Cert Library: No Response")
	cs (SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE, "The certificate issuer's certificate has expired.  Check your system date and time.")
	cs (SEC_ERROR_CRL_EXPIRED, "The CRL for the certificate's issuer has expired.  Update it or check your system date and time.")
	cs (SEC_ERROR_CRL_BAD_SIGNATURE, "The CRL for the certificate's issuer has an invalid signature.")
	cs (SEC_ERROR_CRL_INVALID, "New CRL has an invalid format.")
	cs (SEC_ERROR_EXTENSION_VALUE_INVALID, "Certificate extension value is invalid.")
	cs (SEC_ERROR_EXTENSION_NOT_FOUND, "Certificate extension not found.")
	cs (SEC_ERROR_CA_CERT_INVALID, "Issuer certificate is invalid.")
	cs (SEC_ERROR_PATH_LEN_CONSTRAINT_INVALID, "Certificate path length constraint is invalid.")
	cs (SEC_ERROR_CERT_USAGES_INVALID, "Certificate usages field is invalid.")
	cs (SEC_INTERNAL_ONLY, "**Internal ONLY module**")
	cs (SEC_ERROR_INVALID_KEY, "The key does not support the requested operation.")
	cs (SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION, "Certificate contains unknown critical extension.")
	cs (SEC_ERROR_OLD_CRL, "New CRL is not later than the current one.")
	cs (SEC_ERROR_NO_EMAIL_CERT, "Not encrypted or signed: you do not yet have an email certificate.")
	cs (SEC_ERROR_NO_RECIPIENT_CERTS_QUERY, "Not encrypted: you do not have certificates for each of the recipients.")
	cs (SEC_ERROR_NOT_A_RECIPIENT, "Cannot decrypt: you are not a recipient, or matching certificate and private key not found.")
	cs (SEC_ERROR_PKCS7_KEYALG_MISMATCH, "Cannot decrypt: key encryption algorithm does not match your certificate.")
	cs (SEC_ERROR_PKCS7_BAD_SIGNATURE, "Signature verification failed: no signer found, too many signers found, or improper or corrupted data.")
	cs (SEC_ERROR_UNSUPPORTED_KEYALG, "Unsupported or unknown key algorithm.")
	cs (SEC_ERROR_DECRYPTION_DISALLOWED, "Cannot decrypt: encrypted using a disallowed algorithm or key size.")
	cs (XP_SEC_FORTEZZA_BAD_CARD, "Fortezza card has not been properly initialized.  Please remove it and return it to your issuer.")
	cs (XP_SEC_FORTEZZA_NO_CARD, "No Fortezza cards Found")
	cs (XP_SEC_FORTEZZA_NONE_SELECTED, "No Fortezza card selected")
	cs (XP_SEC_FORTEZZA_MORE_INFO, "Please select a personality to get more info on")
	cs (XP_SEC_FORTEZZA_PERSON_NOT_FOUND, "Personality not found")
	cs (XP_SEC_FORTEZZA_NO_MORE_INFO, "No more information on that Personality")
	cs (XP_SEC_FORTEZZA_BAD_PIN, "Invalid Pin")
	cs (XP_SEC_FORTEZZA_PERSON_ERROR, "Couldn't initialize Fortezza personalities.")
	cs (SEC_ERROR_NO_KRL, "No KRL for this site's certificate has been found.")
	cs (SEC_ERROR_KRL_EXPIRED, "The KRL for this site's certificate has expired.")
	cs (SEC_ERROR_KRL_BAD_SIGNATURE, "The KRL for this site's certificate has an invalid signature.")
	cs (SEC_ERROR_REVOKED_KEY, "The key for this site's certificate has been revoked.")
	cs (SEC_ERROR_KRL_INVALID, "New KRL has an invalid format.")
	cs (SEC_ERROR_NEED_RANDOM, "security library: need random data.")
	cs (SEC_ERROR_NO_MODULE, "security library: no security module can perform the requested operation.")
	cs (SEC_ERROR_NO_TOKEN, "The security card or token does not exist, needs to be initialized, or has been removed.")
	cs (SEC_ERROR_READ_ONLY, "security library: read-only database.")
	cs (SEC_ERROR_NO_SLOT_SELECTED, "No slot or token was selected.")
	cs (SEC_ERROR_CERT_NICKNAME_COLLISION, "A certificate with the same nickname already exists.")
	cs (SEC_ERROR_KEY_NICKNAME_COLLISION, "A key with the same nickname already exists.")
	cs (SEC_ERROR_SAFE_NOT_CREATED, "error while creating safe object")
	cs (SEC_ERROR_BAGGAGE_NOT_CREATED, "error while creating baggage object")
	cs (XP_JAVA_REMOVE_PRINCIPAL_ERROR, "Couldn't remove the principal")
	cs (XP_JAVA_DELETE_PRIVILEGE_ERROR, "Couldn't delete the privilege")
	cs (XP_JAVA_CERT_NOT_EXISTS_ERROR, "This principal doesn't have a certificate")
	cs (SEC_ERROR_BAD_EXPORT_ALGORITHM, "Required algorithm is not allowed.")
	cs (SEC_ERROR_EXPORTING_CERTIFICATES, "Error attempting to export certificates.")
	cs (SEC_ERROR_IMPORTING_CERTIFICATES, "Error attempting to import certificates.")
	cs (SEC_ERROR_PKCS12_DECODING_PFX, "Unable to import.  Decoding error.  File not valid.")
	cs (SEC_ERROR_PKCS12_INVALID_MAC, "Unable to import.  Invalid MAC.  Incorrect password or corrupt file.")
	cs (SEC_ERROR_PKCS12_UNSUPPORTED_MAC_ALGORITHM, "Unable to import.  MAC algorithm not supported.")
	cs (SEC_ERROR_PKCS12_UNSUPPORTED_TRANSPORT_MODE, "Unable to import.  Only password integrity and privacy modes supported.")
	cs (SEC_ERROR_PKCS12_CORRUPT_PFX_STRUCTURE, "Unable to import.  File structure is corrupt.")
	cs (SEC_ERROR_PKCS12_UNSUPPORTED_PBE_ALGORITHM, "Unable to import.  Encryption algorithm not supported.")
	cs (SEC_ERROR_PKCS12_UNSUPPORTED_VERSION, "Unable to import.  File version not supported.")
	cs (SEC_ERROR_PKCS12_PRIVACY_PASSWORD_INCORRECT, "Unable to import.  Incorrect privacy password.")
	cs (SEC_ERROR_PKCS12_CERT_COLLISION, "Unable to import.  Same nickname already exists in database.")
	cs (SEC_ERROR_USER_CANCELLED, "The user pressed cancel.")
	cs (SEC_ERROR_PKCS12_DUPLICATE_DATA, "Not imported, already in database.")
	cs (SEC_ERROR_MESSAGE_SEND_ABORTED, "Message not sent.")
	cs (SEC_ERROR_INADEQUATE_KEY_USAGE, "Certificate key usage inadequate for attempted operation.")
	cs (SEC_ERROR_INADEQUATE_CERT_TYPE, "Certificate type not approved for application.")
	cs (SEC_ERROR_CERT_ADDR_MISMATCH, "Address in signing certificate does not match address in message headers.")
	cs (SEC_ERROR_PKCS12_UNABLE_TO_IMPORT_KEY, "Unable to import.  Error attempting to import private key.")
	cs (SEC_ERROR_PKCS12_IMPORTING_CERT_CHAIN, "Unable to import.  Error attempting to import certificate chain.")
	cs (SEC_ERROR_PKCS12_UNABLE_TO_LOCATE_OBJECT_BY_NAME, "Unable to export.  Unable to locate certificate or key by nickname.")
	cs (SEC_ERROR_PKCS12_UNABLE_TO_EXPORT_KEY, "Unable to export.  Private Key could not be located and exported.")
	cs (SEC_ERROR_PKCS12_UNABLE_TO_WRITE, "Unable to export.  Unable to write the export file.")
	cs (SEC_ERROR_PKCS12_UNABLE_TO_READ, "Unable to import.  Unable to read the import file.")
	cs (SEC_ERROR_PKCS12_KEY_DATABASE_NOT_INITIALIZED, "Unable to export.  Key database corrupt or deleted.")
	cs (SEC_ERROR_KEYGEN_FAIL, "Unable to generate public/private key pair.")
	cs (SEC_ERROR_INVALID_PASSWORD, "Password entered is invalid.  Please pick a different one.")
	cs (SEC_ERROR_RETRY_OLD_PASSWORD, "Old password entered incorrectly.  Please try again.")
	cs (SEC_ERROR_BAD_NICKNAME, "Certificate nickname already in use.")
	cs (SEC_ERROR_NOT_FORTEZZA_ISSUER, "Peer FORTEZZA chain has a non-FORTEZZA Certificate.")
	cs (SEC_ERROR_CANNOT_MOVE_SENSITIVE_KEY, "A sensitive key cannot be moved to the slot where it is needed.")
	cs (SEC_ERROR_JS_INVALID_MODULE_NAME, "Invalid module name.")
	cs (SEC_ERROR_JS_INVALID_DLL, "Invalid module path/filename")
	cs (SEC_ERROR_JS_ADD_MOD_FAILURE, "Unable to add module")
	cs (SEC_ERROR_JS_DEL_MOD_FAILURE, "Unable to delete module")
	cs (SEC_ERROR_OLD_KRL, "New KRL is not later than the current one.")
	cs (SEC_ERROR_CKL_CONFLICT, "New CKL has different issuer than current CKL.  Delete current CKL.")
	cs (SEC_ERROR_CERT_NOT_IN_NAME_SPACE, "The Certifying Authority for this certificate is not permitted to issue a certificate with this name.")
	cs (SEC_ERROR_KRL_NOT_YET_VALID, "The key revocation list for this certificate is not yet valid.")
	cs (SEC_ERROR_CRL_NOT_YET_VALID, "The certificate revocation list for this certificate is not yet valid.")
	cs (SEC_ERROR_UNKNOWN_CERT, "The requested certificate could not be found.")
	cs (SEC_ERROR_UNKNOWN_SIGNER, "The signer's certificate could not be found.")
	cs (SEC_ERROR_CERT_BAD_ACCESS_LOCATION,	 "The location for the certificate status server has invalid format.")
	cs (SEC_ERROR_OCSP_UNKNOWN_RESPONSE_TYPE, "The OCSP response cannot be fully decoded; it is of an unknown type.")
	cs (SEC_ERROR_OCSP_BAD_HTTP_RESPONSE, "The OCSP server returned unexpected/invalid HTTP data.")
	cs (SEC_ERROR_OCSP_MALFORMED_REQUEST, "The OCSP server found the request to be corrupted or improperly formed.")
	cs (SEC_ERROR_OCSP_SERVER_ERROR, "The OCSP server experienced an internal error.")
	cs (SEC_ERROR_OCSP_TRY_SERVER_LATER, "The OCSP server suggests trying again later.")
	cs (SEC_ERROR_OCSP_REQUEST_NEEDS_SIG, "The OCSP server requires a signature on this request.")
	cs (SEC_ERROR_OCSP_UNAUTHORIZED_REQUEST, "The OCSP server has refused this request as unauthorized.")
	cs (SEC_ERROR_OCSP_UNKNOWN_RESPONSE_STATUS, "The OCSP server returned an unrecognizable status.")
	cs (SEC_ERROR_OCSP_UNKNOWN_CERT, "The OCSP server has no status for the certificate.")
	cs (SEC_ERROR_OCSP_NOT_ENABLED, "You must enable OCSP before performing this operation.")
	cs (SEC_ERROR_OCSP_NO_DEFAULT_RESPONDER, "You must set the OCSP default responder before performing this operation.")
	cs (SEC_ERROR_OCSP_MALFORMED_RESPONSE, "The response from the OCSP server was corrupted or improperly formed.")
	cs (SEC_ERROR_OCSP_UNAUTHORIZED_RESPONSE, "The signer of the OCSP response is not authorized to give status for this certificate.")
	cs (SEC_ERROR_OCSP_FUTURE_RESPONSE, "The OCSP response is not yet valid (contains a date in the future).")
	cs (SEC_ERROR_OCSP_OLD_RESPONSE, "The OCSP response contains out-of-date information.")
	cs (SEC_ERROR_DIGEST_NOT_FOUND, "The CMS or PKCS #7 Digest was not found in signed message.")
	cs (SEC_ERROR_UNSUPPORTED_MESSAGE_TYPE, "The CMS or PKCS #7 Message type is unsupported.")
	cs (SEC_ERROR_MODULE_STUCK, "PKCS #11 module could not be removed because it is still in use.")
	cs (SEC_ERROR_BAD_TEMPLATE, "Could not decode ASN.1 data. Specified template was invalid.")
	cs (SEC_ERROR_CRL_NOT_FOUND, "No matching CRL was found.")
	cs (SEC_ERROR_REUSED_ISSUER_AND_SERIAL, "You are attempting to import a cert with the same issuer/serial as an existing cert, but that is not the same cert.")
	cs (SEC_ERROR_BUSY, "NSS could not shutdown. Objects are still in use.")
	cs (SEC_ERROR_EXTRA_INPUT, "DER-encoded message contained extra unused data.")
	cs (SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE, "Unsupported elliptic curve.")
	cs (SEC_ERROR_UNSUPPORTED_EC_POINT_FORM, "Unsupported elliptic curve point form.")
	cs (SEC_ERROR_UNRECOGNIZED_OID, "Unrecognized Object Identifier.")
	cs (SEC_ERROR_OCSP_INVALID_SIGNING_CERT, "Invalid OCSP signing certificate in OCSP response.")
	cs (SEC_ERROR_REVOKED_CERTIFICATE_CRL, "Certificate is revoked in issuer's certificate revocation list.")
	cs (SEC_ERROR_REVOKED_CERTIFICATE_OCSP, "Issuer's OCSP responder reports certificate is revoked.")
	cs (SEC_ERROR_CRL_INVALID_VERSION, "Issuer's Certificate Revocation List has an unknown version number.")
	cs (SEC_ERROR_CRL_V1_CRITICAL_EXTENSION, "Issuer's V1 Certificate Revocation List has a critical extension.")
	cs (SEC_ERROR_CRL_UNKNOWN_CRITICAL_EXTENSION, "Issuer's V2 Certificate Revocation List has an unknown critical extension.")
	cs (SEC_ERROR_UNKNOWN_OBJECT_TYPE, "Unknown object type specified.")
	cs (SEC_ERROR_INCOMPATIBLE_PKCS11, "PKCS #11 driver violates the spec in an incompatible way.")
	cs (SEC_ERROR_NO_EVENT, "No new slot event is available at this time.")
	cs (SEC_ERROR_CRL_ALREADY_EXISTS, "CRL already exists.")
	cs (SEC_ERROR_NOT_INITIALIZED, "NSS is not initialized.")
	cs (SEC_ERROR_TOKEN_NOT_LOGGED_IN, "The operation failed because the PKCS#11 token is not logged in.")
	cs (SEC_ERROR_OCSP_RESPONDER_CERT_INVALID, "Configured OCSP responder's certificate is invalid.")
	cs (SEC_ERROR_OCSP_BAD_SIGNATURE, "OCSP response has an invalid signature.")

	#if defined (NSS_VMAJOR) && defined (NSS_VMINOR) && defined (NSS_VPATCH) && (NSS_VMAJOR > 3 || (NSS_VMAJOR == 3 && NSS_VMINOR > 12) || (NSS_VMAJOR == 3 && NSS_VMINOR == 12 && NSS_VPATCH >= 2))
	cs (SEC_ERROR_OUT_OF_SEARCH_LIMITS, "Cert validation search is out of search limits")
	cs (SEC_ERROR_INVALID_POLICY_MAPPING, "Policy mapping contains anypolicy")
	cs (SEC_ERROR_POLICY_VALIDATION_FAILED, "Cert chain fails policy validation")
	cs (SEC_ERROR_UNKNOWN_AIA_LOCATION_TYPE, "Unknown location type in cert AIA extension")
	cs (SEC_ERROR_BAD_HTTP_RESPONSE, "Server returned bad HTTP response")
	cs (SEC_ERROR_BAD_LDAP_RESPONSE, "Server returned bad LDAP response")
	cs (SEC_ERROR_FAILED_TO_ENCODE_DATA, "Failed to encode data with ASN1 encoder")
	cs (SEC_ERROR_BAD_INFO_ACCESS_LOCATION, "Bad information access location in cert extension")
	cs (SEC_ERROR_LIBPKIX_INTERNAL, "Libpkix internal error occured during cert validation.")
	cs (SEC_ERROR_PKCS11_GENERAL_ERROR, "A PKCS #11 module returned CKR_GENERAL_ERROR, indicating that an unrecoverable error has occurred.")
	cs (SEC_ERROR_PKCS11_FUNCTION_FAILED, "A PKCS #11 module returned CKR_FUNCTION_FAILED, indicating that the requested function could not be performed.  Trying the same operation again might succeed.")
	cs (SEC_ERROR_PKCS11_DEVICE_ERROR, "A PKCS #11 module returned CKR_DEVICE_ERROR, indicating that a problem has occurred with the token or slot.")
	#endif
	}

	#undef cs

	return NULL;
}

static void
set_nss_error (GError **error)
{
	glong err_code;
	const gchar *err_str;

	if (!error)
		return;

	g_return_if_fail (*error == NULL);

	err_code = PORT_GetError ();

	if (!err_code)
		return;

	err_str = nss_error_to_string (err_code);
	if (!err_str)
		return;

	*error = g_error_new_literal (E_CERTDB_ERROR, err_code, err_str);
}

static SECStatus PR_CALLBACK
collect_certs (gpointer arg,
               SECItem **certs,
               gint numcerts)
{
	CERTDERCerts *collectArgs;
	SECItem *cert;
	SECStatus rv;

	collectArgs = (CERTDERCerts *) arg;

	collectArgs->numcerts = numcerts;
	collectArgs->rawCerts = (SECItem *) PORT_ArenaZAlloc (
		collectArgs->arena, sizeof (SECItem) * numcerts);
	if (collectArgs->rawCerts == NULL)
		return (SECFailure);

	cert = collectArgs->rawCerts;

	while (numcerts--) {
		rv = SECITEM_CopyItem (collectArgs->arena, cert, *certs);
		if (rv == SECFailure)
			return (SECFailure);
		cert++;
		certs++;
	}

	return (SECSuccess);
}

static CERTDERCerts *
e_cert_db_get_certs_from_package (PRArenaPool *arena,
                                  gchar *data,
                                  guint32 length)
{
	/*nsNSSShutDownPreventionLock locker;*/
	CERTDERCerts *collectArgs =
		(CERTDERCerts *) PORT_ArenaZAlloc (arena, sizeof (CERTDERCerts));
	SECStatus sec_rv;

	if (!collectArgs)
		return NULL;

	collectArgs->arena = arena;
	sec_rv = CERT_DecodeCertPackage (
		data,
					length, collect_certs,
					(gpointer) collectArgs);

	if (sec_rv != SECSuccess)
		return NULL;

	return collectArgs;
}

/*
 * copy from pk12util.c
 */
static SECStatus
p12u_SwapUnicodeBytes(SECItem *uniItem)
{
	unsigned int i;
	unsigned char a;
	if ((uniItem == NULL) || (uniItem->len % 2)) {
		return SECFailure;
	}
	for (i = 0; i < uniItem->len; i += 2) {
		a = uniItem->data[i];
		uniItem->data[i] = uniItem->data[i+1];
		uniItem->data[i+1] = a;
	}
	return SECSuccess;
}

/*
 * copy from pk12util.c
 */
static PRBool
p12u_ucs2_ascii_conversion_function(PRBool	   toUnicode,
				    unsigned char *inBuf,
				    unsigned int   inBufLen,
				    unsigned char *outBuf,
				    unsigned int   maxOutBufLen,
				    unsigned int  *outBufLen,
				    PRBool	   swapBytes)
{
	SECItem it = { 0 };
	SECItem *dup = NULL;
	PRBool ret;

#ifdef DEBUG_CONVERSION
	if (pk12_debugging) {
		unsigned int i;
		printf ("Converted from:\n");
		for (i = 0; i < inBufLen; i++) {
			printf("%2x ", inBuf[i]);
			/*if ((i % 60) == 0) printf ("\n");*/
		}
		printf ("\n");
	}
#endif

	it.data = inBuf;
	it.len = inBufLen;
	dup = SECITEM_DupItem(&it);
	/* If converting Unicode to ASCII, swap bytes before conversion
	 * as neccessary.
	 */
	if (!toUnicode && swapBytes) {
		if (p12u_SwapUnicodeBytes(dup) != SECSuccess) {
			SECITEM_ZfreeItem(dup, PR_TRUE);
			return PR_FALSE;
		}
	}
	/* Perform the conversion. */
	ret = PORT_UCS2_UTF8Conversion (toUnicode, dup->data, dup->len,
					outBuf, maxOutBufLen, outBufLen);
	if (dup)
		SECITEM_ZfreeItem(dup, PR_TRUE);

#ifdef DEBUG_CONVERSION
	if (pk12_debugging) {
		unsigned int ii;
		printf ("Converted to:\n");
		for (ii = 0; ii < *outBufLen; ii++) {
			printf ("%2x ", outBuf[ii]);
			/*if ((i % 60) == 0) printf ("\n");*/
		}
		printf ("\n");
	}
#endif

	return ret;
}

static gchar * PR_CALLBACK
pk11_password (PK11SlotInfo *slot,
               PRBool retry,
               gpointer arg)
{
	gchar *pwd;
	gchar *nsspwd;

	gboolean rv = FALSE;

	/* For tokens with CKF_PROTECTED_AUTHENTICATION_PATH we
	 * need to return a non-empty but unused password */
	if (PK11_ProtectedAuthenticationPath(slot))
		return PORT_Strdup("");

	g_signal_emit (
		e_cert_db_peek (),
		e_cert_db_signals[PK11_PASSWD], 0,
		slot,
		retry,
		&pwd,
		&rv);

	if (pwd == NULL)
		return NULL;

	nsspwd = PORT_Strdup (pwd);
	memset (pwd, 0, strlen (pwd));
	g_free (pwd);
	return nsspwd;
}

static void
initialize_nss (void)
{
	/* Use camel_init() to initialise NSS consistently... */
	camel_init (e_get_user_data_dir (), TRUE);

	/* ... except for the bits we only seem to do here. FIXME */
	PK11_SetPasswordFunc (pk11_password);

	/* Enable ciphers for PKCS#12 */
	SEC_PKCS12EnableCipher (PKCS12_RC4_40, 1);
	SEC_PKCS12EnableCipher (PKCS12_RC4_128, 1);
	SEC_PKCS12EnableCipher (PKCS12_RC2_CBC_40, 1);
	SEC_PKCS12EnableCipher (PKCS12_RC2_CBC_128, 1);
	SEC_PKCS12EnableCipher (PKCS12_DES_56, 1);
	SEC_PKCS12EnableCipher (PKCS12_DES_EDE3_168, 1);
	SEC_PKCS12SetPreferredCipher (PKCS12_DES_EDE3_168, 1);
	PORT_SetUCS2_ASCIIConversionFunction (p12u_ucs2_ascii_conversion_function);
}

static void
install_loadable_roots (void)
{
	SECMODModuleList *list = SECMOD_GetDefaultModuleList ();
	SECMODListLock *lock = SECMOD_GetDefaultModuleListLock ();
	SECMODModule *RootsModule = NULL;
	gint i;

	SECMOD_GetReadLock (lock);
	while (!RootsModule && list) {
		SECMODModule *module = list->module;

		for (i = 0; i < module->slotCount; i++) {
			PK11SlotInfo *slot = module->slots[i];
			if (PK11_IsPresent (slot)) {
				if (PK11_HasRootCerts (slot)) {
					RootsModule = module;
					break;
				}
			}
		}

		list = list->next;
	}
	SECMOD_ReleaseReadLock (lock);

	if (RootsModule) {
		/* Check version, and unload module if it is too old */
		CK_INFO info;

		if (PK11_GetModInfo (RootsModule, &info) != SECSuccess) {
			/* Do not use this module */
			RootsModule = NULL;
		} else {
			/* NSS_BUILTINS_LIBRARY_VERSION_MAJOR and NSS_BUILTINS_LIBRARY_VERSION_MINOR
			 * define the version we expect to have.
			 * Later version are fine.
			 * Older versions are not ok, and we will replace with our own version.
			 */
			if ((info.libraryVersion.major < NSS_BUILTINS_LIBRARY_VERSION_MAJOR)
			    || (info.libraryVersion.major == NSS_BUILTINS_LIBRARY_VERSION_MAJOR
				&& info.libraryVersion.minor < NSS_BUILTINS_LIBRARY_VERSION_MINOR)) {
				PRInt32 modType;

				SECMOD_DeleteModule (RootsModule->commonName, &modType);

				RootsModule = NULL;
			}
		}
	}

	if (!RootsModule) {
#ifndef G_OS_WIN32
		/* grovel in various places for mozilla's built-in
		 * cert module.
		 *
		 * XXX yes this is gross.  *sigh *
		*/
		const gchar *paths_to_check[] = {
#ifdef MOZILLA_NSS_LIB_DIR
			MOZILLA_NSS_LIB_DIR,
#endif
			"/usr/lib",
			"/usr/lib/mozilla",
			"/opt/mozilla/lib",
			"/opt/mozilla/lib/mozilla"
		};

		for (i = 0; i < G_N_ELEMENTS (paths_to_check); i++) {
			gchar *dll_path = g_module_build_path (paths_to_check[i], "nssckbi");

			if (g_file_test (dll_path, G_FILE_TEST_EXISTS)) {
				PRInt32 modType;

				/* Delete the existing module */
				SECMOD_DeleteModule ("Mozilla Root Certs", &modType);

				SECMOD_AddNewModule ("Mozilla Root Certs",dll_path, 0, 0);
				g_free (dll_path);
				break;
			}

			g_free (dll_path);
		}
#else
		/* FIXME: Might be useful to look up if there is a
		 * Mozilla installation on the machine and use the
		 * nssckbi.dll from there.
		 */
#endif
	}
}

static void
e_cert_db_class_init (ECertDBClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	initialize_nss ();
	/* check to see if you have a rootcert module installed */
	install_loadable_roots ();

	e_cert_db_signals[PK11_PASSWD] = g_signal_new (
		"pk11_passwd",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECertDBClass, pk11_passwd),
		NULL, NULL,
		e_marshal_BOOLEAN__POINTER_BOOLEAN_POINTER,
		G_TYPE_BOOLEAN, 3,
		G_TYPE_POINTER, G_TYPE_BOOLEAN, G_TYPE_POINTER);

	e_cert_db_signals[PK11_CHANGE_PASSWD] = g_signal_new (
		"pk11_change_passwd",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECertDBClass, pk11_change_passwd),
		NULL, NULL,
		e_marshal_BOOLEAN__POINTER_POINTER,
		G_TYPE_BOOLEAN, 2,
		G_TYPE_POINTER, G_TYPE_POINTER);

	e_cert_db_signals[CONFIRM_CA_CERT_IMPORT] = g_signal_new (
		"confirm_ca_cert_import",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECertDBClass, confirm_ca_cert_import),
		NULL, NULL,
		e_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER,
		G_TYPE_BOOLEAN, 4,
		G_TYPE_POINTER, G_TYPE_POINTER, G_TYPE_POINTER, G_TYPE_POINTER);
}

static void
e_cert_db_init (ECertDB *ec)
{
}

GMutex init_mutex;
static ECertDB *cert_db = NULL;

ECertDB *
e_cert_db_peek (void)
{
	g_mutex_lock (&init_mutex);
	if (!cert_db)
		cert_db = g_object_new (E_TYPE_CERT_DB, NULL);
	g_mutex_unlock (&init_mutex);

	return cert_db;
}

void
e_cert_db_shutdown (void)
{
	/* XXX */
}

static gboolean
confirm_download_ca_cert (ECertDB *cert_db,
                          ECert *cert,
                          gboolean *trust_ssl,
                          gboolean *trust_email,
                          gboolean *trust_objsign)
{
	gboolean rv = FALSE;

	*trust_ssl =
		*trust_email =
		*trust_objsign = FALSE;

	g_signal_emit (
		e_cert_db_peek (),
		e_cert_db_signals[CONFIRM_CA_CERT_IMPORT], 0,
		cert,
		trust_ssl,
		trust_email,
		trust_objsign,
		&rv);

	return rv;
}

static gboolean
handle_ca_cert_download (ECertDB *cert_db,
                         GList *certs,
                         GError **error)
{
	ECert *certToShow;
	SECItem der;
	gchar *raw_der = NULL;
	CERTCertificate *tmpCert;

	/* First thing we have to do is figure out which certificate
	 * we're gonna present to the user.  The CA may have sent down
	 * a list of certs which may or may not be a chained list of
	 * certs.  Until the day we can design some solid UI for the
	 * general case, we'll code to the > 90% case.  That case is
	 * where a CA sends down a list that is a chain up to its root
	 * in either ascending or descending order.  What we're gonna
	 * do is compare the first 2 entries, if the first was signed
	 * by the second, we assume the leaf cert is the first cert
	 * and display it.  If the second cert was signed by the first
	 * cert, then we assume the first cert is the root and the
	 * last cert in the array is the leaf.  In this case we
	 * display the last cert.
	*/

	/*  nsNSSShutDownPreventionLock locker;*/

	if (certs == NULL) {
		g_warning ("Didn't get any certs to import.");
		return TRUE;
	}
	else if (certs->next == NULL) {
		/* there's 1 cert */
		certToShow = E_CERT (certs->data);
	}
	else {
		/* there are multiple certs */
		ECert *cert0;
		ECert *cert1;
		const gchar * cert0SubjectName;
		const gchar * cert0IssuerName;
		const gchar * cert1SubjectName;
		const gchar * cert1IssuerName;

		cert0 = E_CERT (certs->data);
		cert1 = E_CERT (certs->next->data);

		cert0IssuerName = e_cert_get_issuer_name (cert0);
		cert0SubjectName = e_cert_get_subject_name (cert0);

		cert1IssuerName = e_cert_get_issuer_name (cert1);
		cert1SubjectName = e_cert_get_subject_name (cert1);

		if (!strcmp (cert1IssuerName, cert0SubjectName)) {
			/* In this case, the first cert in the list signed the second,
			 * so the first cert is the root.  Let's display the last cert
			 * in the list. */
			certToShow = E_CERT (g_list_last (certs)->data);
		}
		else if (!strcmp (cert0IssuerName, cert1SubjectName)) {
			/* In this case the second cert has signed the first cert.  The
			 * first cert is the leaf, so let's display it. */
			certToShow = cert0;
		} else {
			/* It's not a chain, so let's just show the first one in the
			 * downloaded list. */
			certToShow = cert0;
		}
	}

	if (!certToShow) {
		set_nss_error (error);
		return FALSE;
	}

	if (!e_cert_get_raw_der (certToShow, &raw_der, &der.len)) {
		set_nss_error (error);
		return FALSE;
	}

	der.data = (guchar *) raw_der;

	{
		/*PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Creating temp cert\n"));*/
		CERTCertDBHandle *certdb = CERT_GetDefaultCertDB ();
		tmpCert = CERT_FindCertByDERCert (certdb, &der);
		if (!tmpCert) {
			tmpCert = CERT_NewTempCertificate (
				certdb, &der,
				NULL, PR_FALSE, PR_TRUE);
		}
		if (!tmpCert) {
			g_warning ("Couldn't create cert from DER blob");
			set_nss_error (error);
			return FALSE;
		}
	}

#if 0
	CERTCertificateCleaner tmpCertCleaner (tmpCert);
#endif

	if (tmpCert->isperm) {
		if (error && !*error)
			*error = g_error_new_literal (E_CERTDB_ERROR, 0, _("Certificate already exists"));
		return FALSE;
	}
	else {
		gboolean trust_ssl, trust_email, trust_objsign;
		gchar *nickname;
		SECStatus srv;
		CERTCertTrust trust;

		if (!confirm_download_ca_cert (
			cert_db, certToShow, &trust_ssl,
			&trust_email, &trust_objsign)) {
			set_nss_error (error);
			return FALSE;
		}

		/*PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("trust is %d\n", trustBits));*/

		nickname = CERT_MakeCANickname (tmpCert);

		/*PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Created nick \"%s\"\n", nickname.get()));*/

		e_cert_trust_init (&trust);
		e_cert_trust_set_valid_ca (&trust);
		e_cert_trust_add_ca_trust (
			&trust,
			trust_ssl,
			trust_email,
			trust_objsign);

		srv = CERT_AddTempCertToPerm (
			tmpCert,
			nickname,
			&trust);

		/* If we aren't logged into the token, then what *should*
		 * happen is the above call should fail, and we should
		 * authenticate and then try again. But see NSS bug #595861.
		 * With NSS 3.12.6 at least, the above call will fail, but
		 * it *will* have added the cert to the database, with
		 * random trust bits. We have to authenticate and then set
		 * the trust bits correctly. And calling
		 * CERT_AddTempCertToPerm() again doesn't work either -- it'll
		 * fail even though it arguably ought to succeed (which is
		 * probably another NSS bug).
		 * So if we get SEC_ERROR_TOKEN_NOT_LOGGED_IN, we first try
		 * CERT_ChangeCertTrust(), and if that doesn't work we hope
		 * we're on a fixed version of NSS and we try calling
		 * CERT_AddTempCertToPerm() again instead. */
		if (srv != SECSuccess &&
		    PORT_GetError () == SEC_ERROR_TOKEN_NOT_LOGGED_IN &&
		    e_cert_db_login_to_slot (NULL, PK11_GetInternalKeySlot ())) {
			srv = CERT_ChangeCertTrust (
				CERT_GetDefaultCertDB (),
				tmpCert, &trust);
			if (srv != SECSuccess)
				srv = CERT_AddTempCertToPerm (
					tmpCert,
					nickname,
					&trust);
		}
		if (srv != SECSuccess) {
			set_nss_error (error);
			return FALSE;
		}

#if 0
		/* Now it's time to add the rest of the certs we just downloaded.
		 * Since we didn't prompt the user about any of these certs, we
		 * won't set any trust bits for them. */
		e_cert_trust_init (&trust);
		e_cert_trust_set_valid_ca (&trust);
		e_cert_trusts_add_ca_trust (&trust, 0, 0, 0);
		for (PRUint32 i = 0; i < numCerts; i++) {
			if (i == selCertIndex)
				continue;

			certToShow = do_QueryElementAt (x509Certs, i);
			certToShow->GetRawDER (&der.len, (PRUint8 **) &der.data);

			CERTCertificate *tmpCert2 =
				CERT_NewTempCertificate (certdb, &der, nsnull, PR_FALSE, PR_TRUE);

			if (!tmpCert2) {
				NS_ASSERTION (0, "Couldn't create temp cert from DER blob\n");
				continue;  /* Let's try to import the rest of 'em */
			}
			nickname.Adopt (CERT_MakeCANickname (tmpCert2));
			CERT_AddTempCertToPerm (
				tmpCert2, NS_CONST_CAST (gchar *,nickname.get ()),
				defaultTrust.GetTrust ());
			CERT_DestroyCertificate (tmpCert2);
		}
#endif
		return TRUE;
	}
}
gboolean e_cert_db_change_cert_trust (CERTCertificate *cert, CERTCertTrust *trust)
{
	SECStatus srv;

	srv = CERT_ChangeCertTrust (
		CERT_GetDefaultCertDB (),
		cert, trust);
	if (srv != SECSuccess &&
	    PORT_GetError () == SEC_ERROR_TOKEN_NOT_LOGGED_IN &&
	    e_cert_db_login_to_slot (NULL, PK11_GetInternalKeySlot ()))
		srv = CERT_ChangeCertTrust (
			CERT_GetDefaultCertDB (),
			cert, trust);

	if (srv != SECSuccess) {
		glong err = PORT_GetError ();
		g_warning (
			"CERT_ChangeCertTrust() failed: %s\n",
			nss_error_to_string (err));
		return FALSE;
	}
	return TRUE;
}

/* deleting certificates */
gboolean
e_cert_db_delete_cert (ECertDB *certdb,
                       ECert *ecert)
{
	/*  nsNSSShutDownPreventionLock locker;
	 *  nsNSSCertificate *nssCert = NS_STATIC_CAST (nsNSSCertificate *, aCert); */

	CERTCertificate *cert;

	cert = e_cert_get_internal_cert (ecert);
	if (!cert)
		return FALSE;

	if (cert->slot && !e_cert_db_login_to_slot (certdb, cert->slot))
		return FALSE;

	if (!e_cert_mark_for_deletion (ecert)) {
		return FALSE;
	}

	if (cert->slot && e_cert_get_cert_type (ecert) != E_CERT_USER) {
		/* To delete a cert of a slot (builtin, most likely), mark it as
		 * completely untrusted.  This way we keep a copy cached in the
		 * local database, and next time we try to load it off of the
		 * external token/slot, we'll know not to trust it.  We don't
		 * want to do that with user certs, because a user may  re-store
		 * the cert onto the card again at which point we *will* want to
		 * trust that cert if it chains up properly. */
		CERTCertTrust trust;

		e_cert_trust_init_with_values (&trust, 0, 0, 0);
		return e_cert_db_change_cert_trust (cert, &trust);
	}

	return TRUE;
}

/* importing certificates */
gboolean
e_cert_db_import_certs (ECertDB *certdb,
                        gchar *data,
                        guint32 length,
                        ECertType cert_type,
                        GSList **imported_certs,
                        GError **error)
{
	/*nsNSSShutDownPreventionLock locker;*/
	PRArenaPool *arena = PORT_NewArena (DER_DEFAULT_CHUNKSIZE);
	GList *certs = NULL;
	CERTDERCerts *certCollection = e_cert_db_get_certs_from_package (arena, data, length);
	gint i;
	gboolean rv;

	if (!certCollection) {
		set_nss_error (error);
		PORT_FreeArena (arena, PR_FALSE);
		return FALSE;
	}

	/* Now let's create some certs to work with */
	for (i = 0; i < certCollection->numcerts; i++) {
		SECItem *currItem = &certCollection->rawCerts[i];
		ECert *cert;

		cert = e_cert_new_from_der ((gchar *) currItem->data, currItem->len);
		if (!cert) {
			set_nss_error (error);
			g_list_foreach (certs, (GFunc) g_object_unref, NULL);
			g_list_free (certs);
			PORT_FreeArena (arena, PR_FALSE);
			return FALSE;
		}
		certs = g_list_append (certs, cert);
	}
	switch (cert_type) {
	case E_CERT_CA:
		rv = handle_ca_cert_download (certdb, certs, error);
		if (rv && imported_certs) {
			GList *l;

			/* copy certificates to the caller */
			*imported_certs = NULL;
			for (l = certs; l; l = l->next) {
				ECert *cert = l->data;

				if (cert)
					*imported_certs = g_slist_prepend (*imported_certs, g_object_ref (cert));
			}

			*imported_certs = g_slist_reverse (*imported_certs);
		}
		break;
	default:
		/* We only deal with import CA certs in this method currently.*/
		set_nss_error (error);
		PORT_FreeArena (arena, PR_FALSE);
		rv = FALSE;
	}

	g_list_foreach (certs, (GFunc) g_object_unref, NULL);
	g_list_free (certs);
	PORT_FreeArena (arena, PR_FALSE);
	return rv;
}

gboolean
e_cert_db_import_email_cert (ECertDB *certdb,
                             gchar *data,
                             guint32 length,
                             GSList **imported_certs,
                             GError **error)
{
	/*nsNSSShutDownPreventionLock locker;*/
	SECStatus srv = SECFailure;
	gboolean rv = TRUE;
	CERTCertificate * cert;
	SECItem **rawCerts;
	gint numcerts;
	gint i;
	PRArenaPool *arena = PORT_NewArena (DER_DEFAULT_CHUNKSIZE);
	CERTDERCerts *certCollection = e_cert_db_get_certs_from_package (arena, data, length);

	if (!certCollection) {
		set_nss_error (error);
		PORT_FreeArena (arena, PR_FALSE);
		return FALSE;
	}

	cert = CERT_NewTempCertificate (
		CERT_GetDefaultCertDB (), certCollection->rawCerts,
		(gchar *) NULL, PR_FALSE, PR_TRUE);
	if (!cert) {
		set_nss_error (error);
		rv = FALSE;
		goto loser;
	}
	numcerts = certCollection->numcerts;
	rawCerts = (SECItem **) PORT_Alloc (sizeof (SECItem *) * numcerts);
	if (!rawCerts) {
		set_nss_error (error);
		rv = FALSE;
		goto loser;
	}

	for (i = 0; i < numcerts; i++) {
		rawCerts[i] = &certCollection->rawCerts[i];
	}

	srv = CERT_ImportCerts (
		CERT_GetDefaultCertDB (), certUsageEmailSigner,
		numcerts, rawCerts, NULL, PR_TRUE, PR_FALSE,
		NULL);
	if (srv != SECSuccess) {
		set_nss_error (error);
		rv = FALSE;
		goto loser;
	}
	CERT_SaveSMimeProfile (cert, NULL, NULL);

	if (imported_certs) {
		*imported_certs = NULL;
		for (i = 0; i < certCollection->numcerts; i++) {
			SECItem *currItem = &certCollection->rawCerts[i];
			ECert *cert;

			cert = e_cert_new_from_der ((gchar *) currItem->data, currItem->len);
			if (cert)
				*imported_certs = g_slist_prepend (*imported_certs, cert);
		}

		*imported_certs = g_slist_reverse (*imported_certs);
	}

	PORT_Free (rawCerts);
 loser:
	if (cert)
		CERT_DestroyCertificate (cert);
	if (arena)
		PORT_FreeArena (arena, PR_TRUE);
	return rv;
}

gboolean
e_cert_db_import_server_cert (ECertDB *certdb,
                              gchar *data,
                              guint32 length,
                              GSList **imported_certs,
                              GError **error)
{
	/* not c&p'ing this over at the moment, as we don't have a UI
	 * for server certs anyway */
	return FALSE;
}

gboolean
e_cert_db_import_certs_from_file (ECertDB *cert_db,
                                  const gchar *file_path,
                                  ECertType cert_type,
                                  GSList **imported_certs,
                                  GError **error)
{
	gboolean rv;
	gint fd;
	struct stat sb;
	gchar *buf;
	gint bytes_read;

	switch (cert_type) {
	case E_CERT_CA:
	case E_CERT_CONTACT:
	case E_CERT_SITE:
		/* good */
		break;

	default:
		/* not supported (yet) */
		set_nss_error (error);
		return FALSE;
	}

	fd = g_open (file_path, O_RDONLY | O_BINARY, 0);
	if (fd == -1) {
		set_nss_error (error);
		return FALSE;
	}

	if (-1 == fstat (fd, &sb)) {
		set_nss_error (error);
		close (fd);
		return FALSE;
	}

	buf = g_malloc (sb.st_size);
	if (!buf) {
		set_nss_error (error);
		close (fd);
		return FALSE;
	}

	bytes_read = read (fd, buf, sb.st_size);

	close (fd);

	if (bytes_read != sb.st_size) {
		set_nss_error (error);
		rv = FALSE;
	}
	else {
		printf ("importing %d bytes from '%s'\n", bytes_read, file_path);

		switch (cert_type) {
		case E_CERT_CA:
			rv = e_cert_db_import_certs (cert_db, buf, bytes_read, cert_type, imported_certs, error);
			break;

		case E_CERT_SITE:
			rv = e_cert_db_import_server_cert (cert_db, buf, bytes_read, imported_certs, error);
			break;

		case E_CERT_CONTACT:
			rv = e_cert_db_import_email_cert (cert_db, buf, bytes_read, imported_certs, error);
			break;

		/* coverity[dead_error_begin] */
		default:
			rv = FALSE;
			break;
		}
	}

	g_free (buf);
	return rv;
}

gboolean
e_cert_db_import_pkcs12_file (ECertDB *cert_db,
                              const gchar *file_path,
                              GError **error)
{
	EPKCS12 *pkcs12 = e_pkcs12_new ();
	GError *e = NULL;

	if (!e_pkcs12_import_from_file (pkcs12, file_path, &e)) {
		g_propagate_error (error, e);
		return FALSE;
	}

	return TRUE;
}

gboolean
e_cert_db_export_pkcs12_file (ECert *cert,
                              GFile *file,
                              const gchar *password,
                              gboolean save_chain,
                              GError **error)
{
	GError *e = NULL;
	GList *list = NULL;

	g_return_val_if_fail (cert != NULL, FALSE);
	list = g_list_append (list, cert);

	if (!e_pkcs12_export_to_file (list, file, password, save_chain, &e)) {
		g_list_free (list);
		g_propagate_error (error, e);
		return FALSE;
	}

	g_list_free (list);

	return TRUE;
}

gboolean
e_cert_db_login_to_slot (ECertDB *cert_db,
                         PK11SlotInfo *slot)
{
	if (PK11_NeedLogin (slot)) {
		PK11_Logout (slot);

		if (PK11_NeedUserInit (slot)) {
			gchar *pwd;
			gboolean rv = FALSE;

			printf ("initializing slot password\n");

			g_signal_emit (
				e_cert_db_peek (),
				e_cert_db_signals[PK11_CHANGE_PASSWD], 0,
				NULL,
				&pwd,
				&rv);

			if (!rv)
				return FALSE;

			/* the user needs to specify the initial password */
			PK11_InitPin (slot, "", pwd);
		}

		PK11_SetPasswordFunc (pk11_password);
		if (PK11_Authenticate (slot, PR_TRUE, NULL) != SECSuccess) {
			printf (
				"PK11_Authenticate failed (err = %d/%d)\n",
				PORT_GetError (), PORT_GetError () + 0x2000);
			return FALSE;
		}
	}

	return TRUE;
}
