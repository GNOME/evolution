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

#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#include "e-util/e-util.h"

#include "e-cert-db.h"
#include "e-pkcs12.h"

#include "prmem.h"
#include "nss.h"
#include "ssl.h"
#include "pkcs12.h"
#include "p12plcy.h"
#include "pk11func.h"
#include "secerr.h"

/* static callback functions for the NSS PKCS#12 library */
static SECItem * PR_CALLBACK nickname_collision (SECItem *, PRBool *, gpointer);

static gboolean handle_error (gint myerr);

#define PKCS12_BUFFER_SIZE         2048
#define PKCS12_RESTORE_OK          1
#define PKCS12_BACKUP_OK           2
#define PKCS12_USER_CANCELED       3
#define PKCS12_NOSMARTCARD_EXPORT  4
#define PKCS12_RESTORE_FAILED      5
#define PKCS12_BACKUP_FAILED       6
#define PKCS12_NSS_ERROR           7

G_DEFINE_TYPE (EPKCS12, e_pkcs12, G_TYPE_OBJECT)

G_DEFINE_QUARK (e-pkcs12-error-quark, e_pkcs12_error)

static void
e_pkcs12_class_init (EPKCS12Class *class)
{
}

static void
e_pkcs12_init (EPKCS12 *ec)
{
}

EPKCS12 *
e_pkcs12_new (void)
{
	return g_object_new (E_TYPE_PKCS12, NULL);
}

static gboolean
input_to_decoder (SEC_PKCS12DecoderContext *dcx,
                  const gchar *path,
                  GError **error)
{
	/*  nsNSSShutDownPreventionLock locker; */
	SECStatus srv;
	gint amount;
	gchar buf[PKCS12_BUFFER_SIZE];
	FILE *fp;

	/* open path */
	fp = g_fopen (path, "rb");
	if (!fp) {
		/* XXX gerror */
		printf ("couldn't open '%s'\n", path);
		return FALSE;
	}

	while (TRUE) {
		amount = fread (buf, 1, sizeof (buf), fp);
		if (amount < 0) {
			fclose (fp);
			return FALSE;
		}

		/* feed the file data into the decoder */
		srv = SEC_PKCS12DecoderUpdate (
			dcx, (guchar *) buf, amount);
		if (srv) {
			/* XXX g_error */
			fclose (fp);
			return FALSE;
		}
		if (amount < PKCS12_BUFFER_SIZE)
			break;
	}
	fclose (fp);
	return TRUE;
}

/* XXX toshok - this needs to be done using a signal as in the
 * e_cert_db_login_to_slot stuff, instead of a direct gui dep here..
 * for now, though, it stays. */
static gboolean
prompt_for_password (gchar *title,
                     gchar *prompt,
                     SECItem *pwd)
{
	gboolean res = TRUE;
	gchar *passwd;

	passwd = e_passwords_ask_password (
		title, "", prompt,
		E_PASSWORDS_REMEMBER_NEVER | E_PASSWORDS_SECRET,
		NULL, NULL);

	if (passwd) {
		gsize len = strlen (passwd);

		pwd->len = len * 3 + 2;
		pwd->data = (unsigned char *) PORT_ZAlloc (pwd->len);

		if (pwd->data) {
			PRBool toUnicode = PR_TRUE;
			#if G_BYTE_ORDER == G_LITTLE_ENDIAN
			PRBool swapUnicode = PR_TRUE;
			#else
			PRBool swapUnicode = PR_FALSE;
			#endif
			if (PORT_UCS2_ASCIIConversion (toUnicode, (unsigned char *) passwd, len, pwd->data, pwd->len, &pwd->len, swapUnicode) == PR_FALSE) {
				res = FALSE;
			} else if ((!pwd->len) || ((pwd->len >= 2) && (pwd->data[pwd->len - 1] || pwd->data[pwd->len - 2]))) {
				if (pwd->len + 2 > 3 * len)
					pwd->data = (unsigned char *) PORT_Realloc (pwd->data, pwd->len + 2);
				if (!pwd->data) {
					res = FALSE;
				} else {
					pwd->len += 2;
					pwd->data[pwd->len - 1] = 0;
					pwd->data[pwd->len - 2] = 0;
				}
			}
		} else {
			res = FALSE;
		}

		memset (passwd, 0, strlen (passwd));
		g_free (passwd);

		if (!res && pwd->data) {
			PORT_Free (pwd->data);
			pwd->data = NULL;
			pwd->len = 0;
		}
	}

	return res;
}

static SEC_PKCS12DecoderContext *
read_with_password (PK11SlotInfo *slot,
		    const gchar *path,
		    SECItem *passwd,
		    SECStatus *out_status,
		    gboolean *out_rv,
		    GError **error)
{
	SEC_PKCS12DecoderContext *dcx = NULL;

	*out_status = SECFailure;
	*out_rv = FALSE;

	/* initialize the decoder */
	dcx = SEC_PKCS12DecoderStart (
		passwd,
		slot,
		/* we specify NULL for all the
		 * funcs + data so it'll use the
		 * default pk11wrap functions */
		NULL, NULL, NULL,
		NULL, NULL, NULL);
	if (!dcx) {
		*out_status = SECFailure;
		return NULL;
	}
	/* read input file and feed it to the decoder */
	*out_rv = input_to_decoder (dcx, path, error);
	if (!*out_rv) {
#ifdef notyet
		/* XXX we need this to check the gerror */
		if (NS_ERROR_ABORT == rv) {
			/* inputToDecoder indicated a NSS error */
			*out_status = SECFailure;
		}
#else
		*out_status = SECFailure;
#endif
		SEC_PKCS12DecoderFinish (dcx);

		return NULL;
	}

	/* verify the blob */
	*out_status = SEC_PKCS12DecoderVerify (dcx);
	if (*out_status) {
		SEC_PKCS12DecoderFinish (dcx);
		dcx = NULL;
	}

	return dcx;
}

static gboolean
import_from_file_helper (EPKCS12 *pkcs12,
                         PK11SlotInfo *slot,
                         const gchar *path,
                         gboolean *aWantRetry,
                         GError **error)
{
	/*nsNSSShutDownPreventionLock locker; */
	gboolean rv;
	SECStatus srv = SECSuccess;
	SEC_PKCS12DecoderContext *dcx = NULL;
	SECItem passwd;

	*aWantRetry = FALSE;

	memset (&passwd, 0, sizeof (SECItem));

	/* First try without password */
	dcx = read_with_password (slot, path, &passwd, &srv, &rv, NULL);

	if (!dcx) {
		/* Second with an empty password */
		passwd.data = (unsigned char *) "\0\0";
		passwd.len = 2;

		dcx = read_with_password (slot, path, &passwd, &srv, &rv, NULL);

		passwd.data = NULL;
		passwd.len = 0;
	}

	/* if failed, ask for password */
	if (!dcx) {
		passwd.data = NULL;
		rv = prompt_for_password (
			_("PKCS12 File Password"),
			_("Enter password for PKCS12 file:"), &passwd);
		if (!rv)
			goto finish;
		if (passwd.data == NULL) {
			handle_error (PKCS12_USER_CANCELED);
			return TRUE;
		}

		dcx = read_with_password (slot, path, &passwd, &srv, &rv, error);
	}

	/* validate bags */
	srv = SEC_PKCS12DecoderValidateBags (dcx, nickname_collision);
	if (srv) goto finish;
	/* import cert and key */
	srv = SEC_PKCS12DecoderImportBags (dcx);
	if (srv) goto finish;
	/* Later - check to see if this should become default email cert */
	handle_error (PKCS12_RESTORE_OK);
 finish:
	/* If srv != SECSuccess, NSS probably set a specific error code.
	 * We should use that error code instead of inventing a new one
	 * for every error possible. */
	if (srv != SECSuccess) {
		if (SEC_ERROR_BAD_PASSWORD == PORT_GetError () ||
		    SEC_ERROR_INVALID_ARGS == PORT_GetError ()) {
			*aWantRetry = TRUE;
		}
		handle_error (PKCS12_NSS_ERROR);
	} else if (!rv) {
		handle_error (PKCS12_RESTORE_FAILED);
	}
	/* finish the decoder */
	if (dcx)
		SEC_PKCS12DecoderFinish (dcx);
	if (passwd.data)
		PORT_Free (passwd.data);
	return TRUE;
}

gboolean
e_pkcs12_import_from_file (EPKCS12 *pkcs12,
                           const gchar *path,
                           GError **error)
{
	/*nsNSSShutDownPreventionLock locker;*/
	gboolean rv = TRUE;
	gboolean wantRetry;
	PK11SlotInfo *slot;

	printf ("importing pkcs12 from '%s'\n", path);

	slot = PK11_GetInternalKeySlot ();

	if (!e_cert_db_login_to_slot (e_cert_db_peek (), slot))
		return FALSE;

	do {
		rv = import_from_file_helper (pkcs12, slot, path, &wantRetry, error);
	} while (rv && wantRetry);

	return rv;
}

static void
encoder_output_cb (void *arg,
                   const char *buf,
                   unsigned long len)
{
	gsize n_bytes_written = 0;
	GError *error = NULL;

	if (!g_output_stream_write_all (G_OUTPUT_STREAM (arg), buf, len, &n_bytes_written, NULL, &error)) {
		g_warning ("I/O error during certificate backup, error message: %s", error ? error->message : "Unknown error");
		g_clear_error (&error);
	}
}

gboolean
e_pkcs12_export_to_file (GList *certs,
                         GFile *file,
                         const gchar *pwd,
                         gboolean save_chain,
                         GError **error)
{
	GList *link;
	SECStatus srv = SECSuccess;
	GFileOutputStream *output_stream;
	SEC_PKCS12ExportContext *p12exp = NULL;
	SEC_PKCS12SafeInfo *keySafe = NULL, *certSafe = NULL;
	SECItem password;

	password.data = (guchar *) strdup (pwd);
	password.len = strlen (pwd);

	p12exp = SEC_PKCS12CreateExportContext (
			NULL /* SECKEYGetPasswordKey pwfn*/,
			NULL /* void *pwfnarg */,
			NULL /* slot */,
			NULL /* void *wincx*/);
	if (!p12exp) {
		gint err_code = PORT_GetError ();
		*error = g_error_new (E_PKCS12_ERROR, E_PKCS12_ERROR_NSS_FAILED, _("Unable to create export context, err_code: %i"), err_code);
		goto error;
	}

	srv = SEC_PKCS12AddPasswordIntegrity (p12exp, &password, SEC_OID_SHA512);
	if (srv != SECSuccess)  {
		gint err_code = PORT_GetError();
		*error = g_error_new (E_PKCS12_ERROR, E_PKCS12_ERROR_NSS_FAILED, _("Unable to setup password integrity, err_code: %i"), err_code);
		goto error;
	}

	for (link = certs; link; link = g_list_next (link)) {
		keySafe = NULL,	certSafe = NULL;
		keySafe = SEC_PKCS12CreateUnencryptedSafe (p12exp);
		certSafe = SEC_PKCS12CreatePasswordPrivSafe (p12exp, &password, SEC_OID_AES_256_CBC);
		if (!keySafe || !certSafe) {
			gint err_code = PORT_GetError();
			*error = g_error_new (E_PKCS12_ERROR, E_PKCS12_ERROR_NSS_FAILED, _("Unable to create safe bag, err_code: %i"), err_code);
			goto error;
		}

		srv = SEC_PKCS12AddCertOrChainAndKey (
				p12exp /* SEC_PKCS12ExportContext *p12ctxt */,
				certSafe,
				NULL   /* void *certNestedDest */,
				e_cert_get_internal_cert (E_CERT (link->data)),
				CERT_GetDefaultCertDB () /* CERTCertDBHandle *certDb */,
				keySafe,
				NULL    /* void *keyNestedDest*/,
				PR_TRUE /* PRBool shroudKey */,
				&password /* SECItem *pwItem */,
				SEC_OID_AES_256_CBC /* SECOidTag algorithm */,
				save_chain /* includeCertChain */);
		if (srv != SECSuccess) {
			gint err_code = PORT_GetError ();
			*error = g_error_new (E_PKCS12_ERROR, E_PKCS12_ERROR_NSS_FAILED, _("Unable to add key/cert to the store, err_code: %i"), err_code);
			goto error;
		}
	}

	output_stream = g_file_replace (file, NULL, TRUE, G_FILE_CREATE_PRIVATE, NULL, error);
	if (!output_stream) {
		goto error;
	}
	srv = SEC_PKCS12Encode (
		p12exp,
		encoder_output_cb /* SEC_PKCS12EncoderOutputCallback output */,
		output_stream     /* void *outputarg */);
	if (!g_output_stream_close (G_OUTPUT_STREAM (output_stream), NULL, error))
		goto error;

	if (srv != SECSuccess) {
		gint err_code = PORT_GetError ();
		*error = g_error_new (E_PKCS12_ERROR, E_PKCS12_ERROR_NSS_FAILED, _("Unable to write store to disk, err_code: %i"), err_code);
		goto error;
	}

	SEC_PKCS12DestroyExportContext (p12exp);
	SECITEM_ZfreeItem (&password, PR_FALSE); /* free password.data */

	return TRUE;

 error:
	SECITEM_ZfreeItem (&password, PR_FALSE); /* free password.data */
	if (p12exp)
		SEC_PKCS12DestroyExportContext (p12exp);

	return FALSE;
}

/* what to do when the nickname collides with one already in the db.
 * TODO: not handled, throw a dialog allowing the nick to be changed? */
static SECItem * PR_CALLBACK
nickname_collision (SECItem *oldNick,
                    PRBool *cancel,
                    gpointer wincx)
{
	/* nsNSSShutDownPreventionLock locker; */
	gint count = 1;
	gchar *nickname = NULL;
	gchar *default_nickname = _("Imported Certificate");
	SECItem *new_nick;

	*cancel = PR_FALSE;
	printf ("nickname_collision\n");

	/* The user is trying to import a PKCS#12 file that doesn't have the
	 * attribute we use to set the nickname.  So in order to reduce the
	 * number of interactions we require with the user, we'll build a nickname
	 * for the user.  The nickname isn't prominently displayed in the UI,
	 * so it's OK if we generate one on our own here.
	 * XXX If the NSS API were smarter and actually passed a pointer to
	 *     the CERTCertificate * we're importing we could actually just
	 *     call default_nickname (which is what the issuance code path
	 *     does) and come up with a reasonable nickname.  Alas, the NSS
	 *     API limits our ability to produce a useful nickname without
	 *     bugging the user.  :(
	*/
	while (1) {
		CERTCertificate *cert;

		/* If we've gotten this far, that means there isn't a certificate
		 * in the database that has the same subject name as the cert we're
		 * trying to import.  So we need to come up with a "nickname" to
		 * satisfy the NSS requirement or fail in trying to import.
		 * Basically we use a default nickname from a properties file and
		 * see if a certificate exists with that nickname.  If there isn't, then
		 * create update the count by one and append the string '#1' Or
		 * whatever the count currently is, and look for a cert with
		 * that nickname.  Keep updating the count until we find a nickname
		 * without a corresponding cert.
		 * XXX If a user imports *many * certs without the 'friendly name'
		 * attribute, then this may take a long time.  :(
		*/
		if (count > 1) {
			g_free (nickname);
			nickname = g_strdup_printf ("%s #%d", default_nickname, count);
		} else {
			g_free (nickname);
			nickname = g_strdup (default_nickname);
		}
		cert = CERT_FindCertByNickname (
			CERT_GetDefaultCertDB (),
			nickname);
		if (!cert) {
			break;
		}
		CERT_DestroyCertificate (cert);
		count++;
	}

	new_nick = PR_Malloc (sizeof (SECItem));
	new_nick->type = siAsciiString;
	new_nick->data = (guchar *) nickname;
	new_nick->len = strlen ((gchar *) new_nick->data);
	return new_nick;
}

static gboolean
handle_error (gint myerr)
{
	switch (myerr) {
	case PKCS12_RESTORE_OK:
		printf ("PKCS12: Restore succeeded\n");
		break;
	case PKCS12_BACKUP_OK:
		printf ("PKCS12: Backup succeeded\n");
		break;
	case PKCS12_USER_CANCELED:
		printf ("PKCS12: User cancelled operation\n");
		break;
	case PKCS12_NOSMARTCARD_EXPORT:
		printf ("PKCS12: No smart card export\n");
		break;
	case PKCS12_RESTORE_FAILED:
		printf ("PKCS12: Restore failed\n");
		break;
	case PKCS12_BACKUP_FAILED:
		printf ("PKCS12: Backup failed\n");
		break;
	case PKCS12_NSS_ERROR: {
		gint nss_code = PORT_GetError ();
		const gchar *nss_errstr = e_cert_db_nss_error_to_string (nss_code);

		if (nss_code && nss_errstr)
			printf ("PKCS12: NSS error: %d (%s)\n", nss_code, nss_errstr);
		else if (nss_code)
			printf ("PKCS12: NSS error: %d\n", nss_code);
		else
			printf ("PKCS12: Unknown NSS error\n");
		} break;
	default:
		printf ("PKCS12: handle_error (%d)\n", myerr);
		break;
	}

	return FALSE;
}
