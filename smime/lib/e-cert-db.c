/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-cert-db.c
 *
 * Copyright (C) 2003 Ximian, Inc.
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
 * Author: Chris Toshok (toshok@ximian.com)
 */

/* The following is the mozilla license blurb, as the bodies of most
   of these functions were derived from the mozilla source. */

/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* XXX toshok why oh *why* god WHY did they do this?  no fucking
   sense */
/* private NSS defines used by PSM */
/* (must be declated before cert.h) */
#define CERT_NewTempCertificate __CERT_NewTempCertificate
#define CERT_AddTempCertToPerm __CERT_AddTempCertToPerm

#include "smime-marshal.h"
#include "e-cert-db.h"
#include "e-cert-trust.h"
#include "e-pkcs12.h"

#include "gmodule.h"

#include "nss.h"
#include "ssl.h"
#include "p12plcy.h"
#include "pk11func.h"
#include "secmod.h"
#include "certdb.h"
#include "plstr.h"
#include "prprf.h"
#include "prmem.h"
#include "e-util/e-passwords.h"
#include "e-util/e-dialog-utils.h"
#include <gtk/gtkmessagedialog.h>
#include <libgnome/gnome-i18n.h>
#include <fcntl.h>
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

struct _ECertDBPrivate {
};

#define PARENT_TYPE G_TYPE_OBJECT
static GObjectClass *parent_class;

static CERTDERCerts* e_cert_db_get_certs_from_package (PRArenaPool *arena, char *data, guint32 length);



static void
e_cert_db_dispose (GObject *object)
{
	ECertDB *ec = E_CERT_DB (object);

	if (!ec->priv)
		return;

	/* XXX free instance specific data */

	g_free (ec->priv);
	ec->priv = NULL;

	if (G_OBJECT_CLASS (parent_class)->dispose)
		G_OBJECT_CLASS (parent_class)->dispose (object);
}

#if notyet
PRBool
ucs2_ascii_conversion_fn (PRBool toUnicode,
			  unsigned char *inBuf,
			  unsigned int inBufLen,
			  unsigned char *outBuf,
			  unsigned int maxOutBufLen,
			  unsigned int *outBufLen,
			  PRBool swapBytes)
{
	printf ("in ucs2_ascii_conversion_fn\n");
}
#endif

static char* PR_CALLBACK
pk11_password (PK11SlotInfo* slot, PRBool retry, void* arg)
{
	char *pwd;
	char *nsspwd;

	gboolean rv = FALSE;

	g_signal_emit (e_cert_db_peek (),
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
	char *evolution_dir_path;
	gboolean success;

	evolution_dir_path = g_build_path ("/", g_get_home_dir (), ".evolution", NULL);

	/* we initialize NSS here to make sure it only happens once */
	success = (SECSuccess == NSS_InitReadWrite (evolution_dir_path));
	if (!success) {
		success = (SECSuccess == NSS_Init (evolution_dir_path));
		if (success)
			g_warning ("opening cert databases read-only");
	}
	if (!success) {
		success = (SECSuccess == NSS_NoDB_Init (evolution_dir_path));
		if (success)
			g_warning ("initializing security library without cert databases.");
	}
	g_free (evolution_dir_path);

	if (!success) {
		g_warning ("Failed all methods for initializing NSS");
		return;
	}

	NSS_SetDomesticPolicy();

	PK11_SetPasswordFunc(pk11_password);

	/* Enable ciphers for PKCS#12 */
	SEC_PKCS12EnableCipher(PKCS12_RC4_40, 1);
	SEC_PKCS12EnableCipher(PKCS12_RC4_128, 1);
	SEC_PKCS12EnableCipher(PKCS12_RC2_CBC_40, 1);
	SEC_PKCS12EnableCipher(PKCS12_RC2_CBC_128, 1);
	SEC_PKCS12EnableCipher(PKCS12_DES_56, 1);
	SEC_PKCS12EnableCipher(PKCS12_DES_EDE3_168, 1);
	SEC_PKCS12SetPreferredCipher(PKCS12_DES_EDE3_168, 1);
#if notyet
	PORT_SetUCS2_ASCIIConversionFunction(ucs2_ascii_conversion_fn);
#endif
}

static void
install_loadable_roots (void)
{
	gboolean has_roots;
	PK11SlotList *list;

	has_roots = FALSE;
	list = PK11_GetAllTokens(CKM_INVALID_MECHANISM, PR_FALSE, PR_FALSE, NULL);
	if (list) {
		PK11SlotListElement *le;

		for (le = list->head; le; le = le->next) {
			if (PK11_HasRootCerts(le->slot)) {
				has_roots = TRUE;
				break;
			}
		}
	}

	if (!has_roots) {
		/* grovel in various places for mozilla's built-in
		   cert module.

		   XXX yes this is gross.  *sigh*
		*/
		char *paths_to_check[] = {
			"/usr/lib",
			"/usr/lib/mozilla",
		};
		int i;

		for (i = 0; i < G_N_ELEMENTS (paths_to_check); i ++) {
			char *dll_path = g_module_build_path (paths_to_check [i],
							      "nssckbi");

			if (g_file_test (dll_path, G_FILE_TEST_EXISTS)) {
				SECMOD_AddNewModule("Mozilla Root Certs",dll_path, 0, 0);
				g_free (dll_path);
				break;
			}

			g_free (dll_path);
		}
	}
}

static void
e_cert_db_class_init (ECertDBClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_ref (PARENT_TYPE);

	object_class->dispose = e_cert_db_dispose;

	initialize_nss();
	/* check to see if you have a rootcert module installed */
	install_loadable_roots();

	e_cert_db_signals[PK11_PASSWD] =
		g_signal_new ("pk11_passwd",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECertDBClass, pk11_passwd),
			      NULL, NULL,
			      smime_marshal_BOOLEAN__POINTER_BOOLEAN_POINTER,
			      G_TYPE_BOOLEAN, 3,
			      G_TYPE_POINTER, G_TYPE_BOOLEAN, G_TYPE_POINTER);

	e_cert_db_signals[PK11_CHANGE_PASSWD] =
		g_signal_new ("pk11_change_passwd",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECertDBClass, pk11_change_passwd),
			      NULL, NULL,
			      smime_marshal_BOOLEAN__POINTER_POINTER,
			      G_TYPE_BOOLEAN, 2,
			      G_TYPE_POINTER, G_TYPE_POINTER);

	e_cert_db_signals[CONFIRM_CA_CERT_IMPORT] =
		g_signal_new ("confirm_ca_cert_import",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECertDBClass, confirm_ca_cert_import),
			      NULL, NULL,
			      smime_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER,
			      G_TYPE_BOOLEAN, 4,
			      G_TYPE_POINTER, G_TYPE_POINTER, G_TYPE_POINTER, G_TYPE_POINTER);
}

static void
e_cert_db_init (ECertDB *ec)
{
	ec->priv = g_new0 (ECertDBPrivate, 1);
}

GType
e_cert_db_get_type (void)
{
	static GType cert_type = 0;

	if (!cert_type) {
		static const GTypeInfo cert_info =  {
			sizeof (ECertDBClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_cert_db_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (ECertDB),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_cert_db_init,
		};

		cert_type = g_type_register_static (PARENT_TYPE, "ECertDB", &cert_info, 0);
	}

	return cert_type;
}



GStaticMutex init_mutex = G_STATIC_MUTEX_INIT;
static ECertDB *cert_db = NULL;

ECertDB*
e_cert_db_peek (void)
{
	g_static_mutex_lock (&init_mutex);
	if (!cert_db)
		cert_db = g_object_new (E_TYPE_CERT_DB, NULL);
	g_static_mutex_unlock (&init_mutex);

	return cert_db;
}

void
e_cert_db_shutdown (void)
{
	/* XXX */
}

/* searching for certificates */
ECert*
e_cert_db_find_cert_by_nickname (ECertDB *certdb,
				 const char *nickname,
				 GError **error)
{
	/*  nsNSSShutDownPreventionLock locker;*/
	CERTCertificate *cert = NULL;

	/*PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Getting \"%s\"\n", asciiname));*/
	cert = PK11_FindCertFromNickname((char*)nickname, NULL);
	if (!cert) {
		cert = CERT_FindCertByNickname(CERT_GetDefaultCertDB(), (char*)nickname);
	}


	if (cert) {
		/*    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("got it\n"));*/
		ECert *ecert = e_cert_new (cert);
		return ecert;
	}
	else {
		/* XXX gerror */
		return NULL;
	}
}

#if notyet
ECert*
e_cert_db_find_cert_by_key (ECertDB *certdb,
			    const char *db_key,
			    GError **error)
{
	/*  nsNSSShutDownPreventionLock locker;*/
	SECItem keyItem = {siBuffer, NULL, 0};
	SECItem *dummy;
	CERTIssuerAndSN issuerSN;
	unsigned long moduleID,slotID;
	CERTCertificate *cert;

	if (!db_key) {
		/* XXX gerror */
		return NULL;
	}

	dummy = NSSBase64_DecodeBuffer(NULL, &keyItem, db_key,
				       (PRUint32)PL_strlen(db_key)); 

	/* someday maybe we can speed up the search using the moduleID and slotID*/
	moduleID = NS_NSS_GET_LONG(keyItem.data);
	slotID = NS_NSS_GET_LONG(&keyItem.data[NS_NSS_LONG]);

	/* build the issuer/SN structure*/
	issuerSN.serialNumber.len = NS_NSS_GET_LONG(&keyItem.data[NS_NSS_LONG*2]);
	issuerSN.derIssuer.len = NS_NSS_GET_LONG(&keyItem.data[NS_NSS_LONG*3]);
	issuerSN.serialNumber.data= &keyItem.data[NS_NSS_LONG*4];
	issuerSN.derIssuer.data= &keyItem.data[NS_NSS_LONG*4+
					       issuerSN.serialNumber.len];

	cert = CERT_FindCertByIssuerAndSN(CERT_GetDefaultCertDB(), &issuerSN);
	PR_FREEIF(keyItem.data);
	if (cert) {
		ECert *ecert = e_cert_new (cert);
		return e_cert;
	}

	/* XXX gerror */
	return NULL;
}

GList*
e_cert_db_get_cert_nicknames    (ECertDB *certdb,
				 ECertType cert_type,
				 GError **error)
{
}

ECert*
e_cert_db_find_email_encryption_cert (ECertDB *certdb,
				      const char *nickname,
				      GError **error)
{
}

ECert*
e_cert_db_find_email_signing_cert (ECertDB *certdb,
				   const char *nickname,
				   GError **error)
{
}
#endif

ECert*
e_cert_db_find_cert_by_email_address (ECertDB *certdb,
				      const char *email,
				      GError **error)
{
	/*  nsNSSShutDownPreventionLock locker; */
	ECert *cert;
	CERTCertificate *any_cert = CERT_FindCertByNicknameOrEmailAddr(CERT_GetDefaultCertDB(),
								       (char*)email);
	CERTCertList *certlist;

	if (!any_cert) {
		/* XXX gerror */
		return NULL;
	}

	/* any_cert now contains a cert with the right subject, but it might not have the correct usage */
	certlist = CERT_CreateSubjectCertList(NULL,
					      CERT_GetDefaultCertDB(),
					      &any_cert->derSubject,
					      PR_Now(), PR_TRUE);
	if (!certlist) {
		/* XXX gerror */
		CERT_DestroyCertificate(any_cert);
		return NULL;
	}

	if (SECSuccess != CERT_FilterCertListByUsage(certlist, certUsageEmailRecipient, PR_FALSE)) {
		/* XXX gerror */
		CERT_DestroyCertificate(any_cert);
		/* XXX free certlist? */
		return NULL;
	}
  
	if (CERT_LIST_END(CERT_LIST_HEAD(certlist), certlist)) {
		/* XXX gerror */
		CERT_DestroyCertificate(any_cert);
		/* XXX free certlist? */
		return NULL;
	}

	cert = e_cert_new (CERT_LIST_HEAD(certlist)->cert);

	return cert;
}

static gboolean
confirm_download_ca_cert (ECertDB *cert_db, ECert *cert, gboolean *trust_ssl, gboolean *trust_email, gboolean *trust_objsign)
{
	gboolean rv = FALSE;

	*trust_ssl =
		*trust_email =
		*trust_objsign = FALSE;

	g_signal_emit (e_cert_db_peek (),
		       e_cert_db_signals[CONFIRM_CA_CERT_IMPORT], 0,
		       cert,
		       trust_ssl,
		       trust_email,
		       trust_objsign,
		       &rv);

	return rv;
}

static gboolean
handle_ca_cert_download(ECertDB *cert_db, GList *certs, GError **error)
{
	ECert *certToShow;
	SECItem der;
	CERTCertificate *tmpCert;

	/* First thing we have to do is figure out which certificate
	   we're gonna present to the user.  The CA may have sent down
	   a list of certs which may or may not be a chained list of
	   certs.  Until the day we can design some solid UI for the
	   general case, we'll code to the > 90% case.  That case is
	   where a CA sends down a list that is a chain up to its root
	   in either ascending or descending order.  What we're gonna
	   do is compare the first 2 entries, if the first was signed
	   by the second, we assume the leaf cert is the first cert
	   and display it.  If the second cert was signed by the first
	   cert, then we assume the first cert is the root and the
	   last cert in the array is the leaf.  In this case we
	   display the last cert.
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
		const char* cert0SubjectName;
		const char* cert0IssuerName;
		const char* cert1SubjectName;
		const char* cert1IssuerName;
		
		cert0 = E_CERT (certs->data);
		cert1 = E_CERT (certs->next->data);

		cert0IssuerName = e_cert_get_issuer_name (cert0);
		cert0SubjectName = e_cert_get_subject_name (cert0);

		cert1IssuerName = e_cert_get_issuer_name (cert1);
		cert1SubjectName = e_cert_get_subject_name (cert1);

		if (!strcmp(cert1IssuerName, cert0SubjectName)) {
			/* In this case, the first cert in the list signed the second,
			   so the first cert is the root.  Let's display the last cert 
			   in the list. */
			certToShow = E_CERT (g_list_last (certs)->data);
		}
		else if (!strcmp(cert0IssuerName, cert1SubjectName)) {
			/* In this case the second cert has signed the first cert.  The 
			   first cert is the leaf, so let's display it. */
			certToShow = cert0;
		} else {
			/* It's not a chain, so let's just show the first one in the 
			   downloaded list. */
			certToShow = cert0;
		}
	}

	if (!certToShow) {
		/* XXX gerror */
		return FALSE;
	}

	if (!e_cert_get_raw_der (certToShow, (char**)&der.data, &der.len)) {
		/* XXX gerror */
		return FALSE;
	}

	{
		/*PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Creating temp cert\n"));*/
		CERTCertDBHandle *certdb = CERT_GetDefaultCertDB();
		tmpCert = CERT_FindCertByDERCert(certdb, &der);
		if (!tmpCert) {
			tmpCert = CERT_NewTempCertificate(certdb, &der,
							  NULL, PR_FALSE, PR_TRUE);
		}
		if (!tmpCert) {
			g_warning ("Couldn't create cert from DER blob");
			return FALSE;
		}
	}

#if 0
	CERTCertificateCleaner tmpCertCleaner(tmpCert);
#endif

	if (tmpCert->isperm) {
		/* XXX we shouldn't be popping up dialogs in this code. */
		e_notice (NULL, GTK_MESSAGE_WARNING, _("Certificate already exists"));
		/* XXX gerror */
		return FALSE;
	}
	else {
		gboolean trust_ssl, trust_email, trust_objsign;
		char *nickname;
		SECStatus srv;
		CERTCertTrust trust;

		if (!confirm_download_ca_cert (cert_db, certToShow, &trust_ssl, &trust_email, &trust_objsign)) {
			/* XXX gerror */
			return FALSE;
		}

		/*PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("trust is %d\n", trustBits));*/
		
		nickname = CERT_MakeCANickname(tmpCert);

		/*PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Created nick \"%s\"\n", nickname.get()));*/

		e_cert_trust_init (&trust);
		e_cert_trust_set_valid_ca (&trust);
		e_cert_trust_add_ca_trust (&trust,
					   trust_ssl,
					   trust_email,
					   trust_objsign);

		srv = CERT_AddTempCertToPerm(tmpCert,
					     nickname,
					     &trust); 

		if (srv != SECSuccess) {
			/* XXX gerror */
			return FALSE;
		}

#if 0
		/* Now it's time to add the rest of the certs we just downloaded.
		   Since we didn't prompt the user about any of these certs, we
		   won't set any trust bits for them. */
		e_cert_trust_init (&trust);
		e_cert_trust_set_valid_ca (&trust);
		e_cert_trusts_add_ca_trust (&trust, 0, 0, 0);
		for (PRUint32 i=0; i<numCerts; i++) {
			if (i == selCertIndex)
				continue;

			certToShow = do_QueryElementAt(x509Certs, i);
			certToShow->GetRawDER(&der.len, (PRUint8 **)&der.data);

			CERTCertificate *tmpCert2 = 
				CERT_NewTempCertificate(certdb, &der, nsnull, PR_FALSE, PR_TRUE);

			if (!tmpCert2) {
				NS_ASSERTION(0, "Couldn't create temp cert from DER blob\n");
				continue;  /* Let's try to import the rest of 'em */
			}
			nickname.Adopt(CERT_MakeCANickname(tmpCert2));
			CERT_AddTempCertToPerm(tmpCert2, NS_CONST_CAST(char*,nickname.get()), 
					       defaultTrust.GetTrust());
			CERT_DestroyCertificate(tmpCert2);
		}
#endif
		return TRUE;
	}
}

/* deleting certificates */
gboolean
e_cert_db_delete_cert (ECertDB *certdb,
		       ECert   *ecert)
{
	/*  nsNSSShutDownPreventionLock locker;
	    nsNSSCertificate *nssCert = NS_STATIC_CAST(nsNSSCertificate*, aCert); */

	CERTCertificate *cert;
	SECStatus srv = SECSuccess;
	if (!e_cert_mark_for_deletion (ecert)) {
		return FALSE;
	}

	cert = e_cert_get_internal_cert (ecert);
	if (cert->slot && e_cert_get_cert_type (ecert) != E_CERT_USER) {
		/* To delete a cert of a slot (builtin, most likely), mark it as
		   completely untrusted.  This way we keep a copy cached in the
		   local database, and next time we try to load it off of the 
		   external token/slot, we'll know not to trust it.  We don't 
		   want to do that with user certs, because a user may  re-store
		   the cert onto the card again at which point we *will* want to 
		   trust that cert if it chains up properly. */
		CERTCertTrust trust;

		e_cert_trust_init_with_values (&trust, 0, 0, 0);
		srv = CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), 
					   cert, &trust);
	}

	/*PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("cert deleted: %d", srv));*/
	return (srv) ? FALSE : TRUE;
}

/* importing certificates */
gboolean
e_cert_db_import_certs (ECertDB *certdb,
			char *data, guint32 length,
			ECertType cert_type,
			GError **error)
{
	/*nsNSSShutDownPreventionLock locker;*/
	PRArenaPool *arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
	GList *certs = NULL;
	CERTDERCerts *certCollection = e_cert_db_get_certs_from_package (arena, data, length);
	int i;
	gboolean rv;

	if (!certCollection) {
		/* XXX gerror */
		PORT_FreeArena(arena, PR_FALSE);
		return FALSE;
	}

	/* Now let's create some certs to work with */
	for (i=0; i<certCollection->numcerts; i++) {
		SECItem *currItem = &certCollection->rawCerts[i];
		ECert *cert;
		
		cert = e_cert_new_from_der ((char*)currItem->data, currItem->len);
		if (!cert) {
			/* XXX gerror */
			g_list_foreach (certs, (GFunc)g_object_unref, NULL);
			g_list_free (certs);
			PORT_FreeArena(arena, PR_FALSE);
			return FALSE;
		}
		certs = g_list_append (certs, cert);
	}
	switch (cert_type) {
	case E_CERT_CA:
		rv = handle_ca_cert_download(certdb, certs, error);
		break;
	default:
		/* We only deal with import CA certs in this method currently.*/
		/* XXX gerror */
		PORT_FreeArena(arena, PR_FALSE);
		rv = FALSE;
	}  

	g_list_foreach (certs, (GFunc)g_object_unref, NULL);
	g_list_free (certs);
	PORT_FreeArena(arena, PR_FALSE);
	return rv;
}

gboolean
e_cert_db_import_email_cert (ECertDB *certdb,
			     char *data, guint32 length,
			     GError **error)
{
	/*nsNSSShutDownPreventionLock locker;*/
	SECStatus srv = SECFailure;
	gboolean rv = TRUE;
	CERTCertificate * cert;
	SECItem **rawCerts;
	int numcerts;
	int i;
	PRArenaPool *arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
	CERTDERCerts *certCollection = e_cert_db_get_certs_from_package (arena, data, length);

	if (!certCollection) {
		/* XXX g_error */

		PORT_FreeArena(arena, PR_FALSE);
		return FALSE;
	}

	cert = CERT_NewTempCertificate(CERT_GetDefaultCertDB(), certCollection->rawCerts,
				       (char *)NULL, PR_FALSE, PR_TRUE);
	if (!cert) {
		/* XXX g_error */
		rv = FALSE;
		goto loser;
	}
	numcerts = certCollection->numcerts;
	rawCerts = (SECItem **) PORT_Alloc(sizeof(SECItem *) * numcerts);
	if ( !rawCerts ) {
		/* XXX g_error */
		rv = FALSE;
		goto loser;
	}

	for ( i = 0; i < numcerts; i++ ) {
		rawCerts[i] = &certCollection->rawCerts[i];
	}
 
	srv = CERT_ImportCerts(CERT_GetDefaultCertDB(), certUsageEmailSigner,
			       numcerts, rawCerts, NULL, PR_TRUE, PR_FALSE,
			       NULL);
	if ( srv != SECSuccess ) {
		/* XXX g_error */
		rv = FALSE;
		goto loser;
	}
	srv = CERT_SaveSMimeProfile(cert, NULL, NULL);
	PORT_Free(rawCerts);
 loser:
	if (cert)
		CERT_DestroyCertificate(cert);
	if (arena) 
		PORT_FreeArena(arena, PR_TRUE);
	return rv;
}

static char *
default_nickname (CERTCertificate *cert)
{   
	/*  nsNSSShutDownPreventionLock locker; */
	char *username = NULL;
	char *caname = NULL;
	char *nickname = NULL;
	char *tmp = NULL;
	int count;
	char *nickFmt=NULL, *nickFmtWithNum = NULL;
	CERTCertificate *dummycert;
	PK11SlotInfo *slot=NULL;
	CK_OBJECT_HANDLE keyHandle;

	CERTCertDBHandle *defaultcertdb = CERT_GetDefaultCertDB();

	username = CERT_GetCommonName(&cert->subject);
	if ( username == NULL ) 
		username = PL_strdup("");

	if ( username == NULL ) 
		goto loser;
    
	caname = CERT_GetOrgName(&cert->issuer);
	if ( caname == NULL ) 
		caname = PL_strdup("");
  
	if ( caname == NULL ) 
		goto loser;
  
	count = 1;

	nickFmt = "%1$s's %2$s ID";
	nickFmtWithNum = "%1$s's %2$s ID #%3$d";

	nickname = PR_smprintf(nickFmt, username, caname);
	/*
	 * We need to see if the private key exists on a token, if it does
	 * then we need to check for nicknames that already exist on the smart
	 * card.
	 */
	slot = PK11_KeyForCertExists(cert, &keyHandle, NULL);
	if (slot == NULL) {
		goto loser;
	}
	if (!PK11_IsInternal(slot)) {
		tmp = PR_smprintf("%s:%s", PK11_GetTokenName(slot), nickname);
		PR_Free(nickname);
		nickname = tmp;
		tmp = NULL;
	}
	tmp = nickname;
	while ( 1 ) {	
		if ( count > 1 ) {
			nickname = PR_smprintf("%s #%d", tmp, count);
		}
  
		if ( nickname == NULL ) 
			goto loser;
 
		if (PK11_IsInternal(slot)) {
			/* look up the nickname to make sure it isn't in use already */
			dummycert = CERT_FindCertByNickname(defaultcertdb, nickname);
      
		} else {
			/*
			 * Check the cert against others that already live on the smart 
			 * card.
			 */
			dummycert = PK11_FindCertFromNickname(nickname, NULL);
			if (dummycert != NULL) {
				/*
				 * Make sure the subject names are different.
				 */ 
				if (CERT_CompareName(&cert->subject, &dummycert->subject) == SECEqual) {
					/*
					 * There is another certificate with the same nickname and
					 * the same subject name on the smart card, so let's use this
					 * nickname.
					 */
					CERT_DestroyCertificate(dummycert);
					dummycert = NULL;
				}
			}
		}
		if ( dummycert == NULL ) 
			goto done;
    
		/* found a cert, destroy it and loop */
		CERT_DestroyCertificate(dummycert);
		if (tmp != nickname) PR_Free(nickname);
		count++;
	} /* end of while(1) */

 loser:
	if ( nickname ) {
		PR_Free(nickname);
	}
	nickname = NULL;
 done:
	if ( caname ) {
		PR_Free(caname);
	}
	if ( username )  {
		PR_Free(username);
	}
	if (slot != NULL) {
		PK11_FreeSlot(slot);
		if (nickname != NULL) {
			tmp = nickname;
			nickname = strchr(tmp, ':');
			if (nickname != NULL) {
				nickname++;
				nickname = PL_strdup(nickname);
				PR_Free(tmp);
				tmp = NULL;
			} else {
				nickname = tmp;
				tmp = NULL;
			}
		}
	}
	PR_FREEIF(tmp);
	return(nickname);
}

gboolean
e_cert_db_import_user_cert (ECertDB *certdb,
			    char *data, guint32 length,
			    GError **error)
{
	/*  nsNSSShutDownPreventionLock locker;*/
	PK11SlotInfo *slot;
	char * nickname = NULL;
	gboolean rv = FALSE;
	int numCACerts;
	SECItem *CACerts;
	CERTDERCerts * collectArgs;
	PRArenaPool *arena;
	CERTCertificate * cert=NULL;

	arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
	if ( arena == NULL ) {
		/* XXX g_error */
		goto loser;
	}

	collectArgs = e_cert_db_get_certs_from_package (arena, data, length);
	if (!collectArgs) {
		/* XXX g_error */
		goto loser;
	}

	cert = CERT_NewTempCertificate(CERT_GetDefaultCertDB(), collectArgs->rawCerts,
				       (char *)NULL, PR_FALSE, PR_TRUE);
	if (!cert) {
		/* XXX g_error */
		goto loser;
	}

	slot = PK11_KeyForCertExists(cert, NULL, NULL);
	if ( slot == NULL ) {
		/* XXX g_error */
		goto loser;
	}
	PK11_FreeSlot(slot);

	/* pick a nickname for the cert */
	if (cert->nickname) {
		/* sigh, we need a call to look up other certs with this subject and
		 * identify nicknames from them. We can no longer walk down internal
		 * database structures  rjr */
		nickname = cert->nickname;
	}
	else {
		nickname = default_nickname(cert);
	}

	/* user wants to import the cert */
	slot = PK11_ImportCertForKey(cert, nickname, NULL);
	if (!slot) {
		/* XXX g_error */
		goto loser;
	}
	PK11_FreeSlot(slot);
	numCACerts = collectArgs->numcerts - 1;

	if (numCACerts) {
		CACerts = collectArgs->rawCerts+1;
		if ( ! CERT_ImportCAChain(CACerts, numCACerts, certUsageUserCertImport) ) {
			rv = TRUE;
		}
	}
  
 loser:
	if (arena) {
		PORT_FreeArena(arena, PR_FALSE);
	}
	if ( cert ) {
		CERT_DestroyCertificate(cert);
	}
	return rv;
}

gboolean
e_cert_db_import_server_cert (ECertDB *certdb,
			      char *data, guint32 length,
			      GError **error)
{
	/* not c&p'ing this over at the moment, as we don't have a UI
	   for server certs anyway */
	return FALSE;
}

gboolean
e_cert_db_import_certs_from_file (ECertDB *cert_db,
				  const char *file_path,
				  ECertType cert_type,
				  GError **error)
{
	gboolean rv;
	int fd;
	struct stat sb;
	char *buf;
	int bytes_read;

	switch (cert_type) {
	case E_CERT_CA:
	case E_CERT_CONTACT:
	case E_CERT_SITE:
		/* good */
		break;
    
	default:
		/* not supported (yet) */
		/* XXX gerror */
		return FALSE;
	}

	fd = open (file_path, O_RDONLY);
	if (fd == -1) {
		/* XXX gerror */
		return FALSE;
	}

	if (-1 == fstat (fd, &sb)) {
		/* XXX gerror */
		close (fd);
		return FALSE;
	}
  
	buf = g_malloc (sb.st_size);
	if (!buf) {
		/* XXX gerror */
		close (fd);
		return FALSE;
	}

	bytes_read = read (fd, buf, sb.st_size);

	close (fd);
  
	if (bytes_read != sb.st_size) {
		/* XXX gerror */
		rv = FALSE;
	}
	else {
		printf ("importing %d bytes from `%s'\n", bytes_read, file_path);

		switch (cert_type) {
		case E_CERT_CA:
			rv = e_cert_db_import_certs (cert_db, buf, bytes_read, cert_type, error);
			break;

		case E_CERT_SITE:
			rv = e_cert_db_import_server_cert (cert_db, buf, bytes_read, error);
			break;

		case E_CERT_CONTACT:
			rv = e_cert_db_import_email_cert (cert_db, buf, bytes_read, error);
			break;
      
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
			      const char *file_path,
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

#if notyet
gboolean
e_cert_db_export_pkcs12_file (ECertDB *cert_db,
			      const char *file_path,
			      GList *certs,
			      GError **error)
{
}
#endif

gboolean
e_cert_db_login_to_slot (ECertDB *cert_db,
			 PK11SlotInfo *slot)
{
	if (PK11_NeedLogin (slot)) {
		PK11_Logout (slot);

		if (PK11_NeedUserInit (slot)) {
			char *pwd;
			gboolean rv = FALSE;

			printf ("initializing slot password\n");

			g_signal_emit (e_cert_db_peek (),
				       e_cert_db_signals[PK11_CHANGE_PASSWD], 0,
				       NULL,
				       &pwd,
				       &rv);

			if (!rv)
				return FALSE;

			/* the user needs to specify the initial password */
			PK11_InitPin (slot, "", pwd);
		}

		PK11_SetPasswordFunc(pk11_password);
		if (PK11_Authenticate (slot, PR_TRUE, NULL) != SECSuccess) {
			printf ("PK11_Authenticate failed (err = %d/%d)\n", PORT_GetError(), PORT_GetError() + 0x2000);
			return FALSE;
		}
	}

	return TRUE;
}



static SECStatus PR_CALLBACK
collect_certs(void *arg, SECItem **certs, int numcerts)
{
	CERTDERCerts *collectArgs;
	SECItem *cert;
	SECStatus rv;

	collectArgs = (CERTDERCerts *)arg;

	collectArgs->numcerts = numcerts;
	collectArgs->rawCerts = (SECItem *) PORT_ArenaZAlloc(collectArgs->arena, sizeof(SECItem) * numcerts);
	if ( collectArgs->rawCerts == NULL )
		return(SECFailure);

	cert = collectArgs->rawCerts;

	while ( numcerts-- ) {
		rv = SECITEM_CopyItem(collectArgs->arena, cert, *certs);
		if ( rv == SECFailure )
			return(SECFailure);
		cert++;
		certs++;
	}

	return (SECSuccess);
}

static CERTDERCerts*
e_cert_db_get_certs_from_package (PRArenaPool *arena,
				  char *data,
				  guint32 length)
{
	/*nsNSSShutDownPreventionLock locker;*/
	CERTDERCerts *collectArgs = 
		(CERTDERCerts *)PORT_ArenaZAlloc(arena, sizeof(CERTDERCerts));
	SECStatus sec_rv;

	if (!collectArgs)
		return NULL;

	collectArgs->arena = arena;
	sec_rv = CERT_DecodeCertPackage(data,
					length, collect_certs, 
					(void *)collectArgs);

	if (sec_rv != SECSuccess)
		return NULL;

	return collectArgs;
}
