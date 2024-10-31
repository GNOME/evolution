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

const gchar *
e_cert_db_nss_error_to_string (gint errorcode)
{
	const gchar *str = camel_smime_context_util_nss_error_to_string (errorcode);

	if (str)
		return str;

	return _("Unknown error");
}

static void
set_nss_error (GError **error)
{
	gint err_code;
	const gchar *err_str;

	if (!error)
		return;

	g_return_if_fail (*error == NULL);

	err_code = PORT_GetError ();

	if (!err_code)
		return;

	err_str = e_cert_db_nss_error_to_string (err_code);
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
	if (!dup)
		return PR_FALSE;

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
static ECertDB *glob_cert_db = NULL;

ECertDB *
e_cert_db_peek (void)
{
	g_mutex_lock (&init_mutex);
	if (!glob_cert_db)
		glob_cert_db = g_object_new (E_TYPE_CERT_DB, NULL);
	g_mutex_unlock (&init_mutex);

	return glob_cert_db;
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
		gint err = PORT_GetError ();
		g_warning (
			"CERT_ChangeCertTrust() failed: %s\n",
			e_cert_db_nss_error_to_string (err));
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
			ECert *ecert;

			ecert = e_cert_new_from_der ((gchar *) currItem->data, currItem->len);
			if (ecert)
				*imported_certs = g_slist_prepend (*imported_certs, ecert);
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
