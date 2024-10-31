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

#include "e-cert-trust.h"

void
e_cert_trust_init (CERTCertTrust *trust)
{
	memset (trust, 0, sizeof (CERTCertTrust));
}

void
e_cert_trust_init_with_values (CERTCertTrust *trust,
                               guint ssl,
                               guint email,
                               guint objsign)
{
	memset (trust, 0, sizeof (CERTCertTrust));
	e_cert_trust_add_trust (&trust->sslFlags, ssl);
	e_cert_trust_add_trust (&trust->emailFlags, email);
	e_cert_trust_add_trust (&trust->objectSigningFlags, objsign);
}

void
e_cert_trust_add_ca_trust (CERTCertTrust *trust,
                           PRBool ssl,
                           PRBool email,
                           PRBool objSign)
{
	if (ssl) {
		e_cert_trust_add_trust (
			&trust->sslFlags, CERTDB_TRUSTED_CA);
		e_cert_trust_add_trust (
			&trust->sslFlags, CERTDB_TRUSTED_CLIENT_CA);
	}

	if (email) {
		e_cert_trust_add_trust (
			&trust->emailFlags, CERTDB_TRUSTED_CA);
		e_cert_trust_add_trust (
			&trust->emailFlags, CERTDB_TRUSTED_CLIENT_CA);
	}

	if (objSign) {
		e_cert_trust_add_trust (
			&trust->objectSigningFlags, CERTDB_TRUSTED_CA);
		e_cert_trust_add_trust (
			&trust->objectSigningFlags, CERTDB_TRUSTED_CLIENT_CA);
	}
}

void
e_cert_trust_add_peer_trust (CERTCertTrust *trust,
                             PRBool ssl,
                             PRBool email,
                             PRBool objSign)
{
	if (ssl)
		e_cert_trust_add_trust (&trust->sslFlags, CERTDB_TRUSTED);
	if (email)
		e_cert_trust_add_trust (&trust->emailFlags, CERTDB_TRUSTED);
	if (objSign)
		e_cert_trust_add_trust (&trust->objectSigningFlags, CERTDB_TRUSTED);
}

void
e_cert_trust_set_ssl_trust (CERTCertTrust *trust,
                            PRBool peer,
                            PRBool tPeer,
                            PRBool ca,
                            PRBool tCA,
                            PRBool tClientCA,
                            PRBool user,
                            PRBool warn)
{
	trust->sslFlags = 0;
	if (peer || tPeer)
		e_cert_trust_add_trust (&trust->sslFlags, CERTDB_TERMINAL_RECORD);
	if (tPeer)
		e_cert_trust_add_trust (&trust->sslFlags, CERTDB_TRUSTED);
	if (ca || tCA)
		e_cert_trust_add_trust (&trust->sslFlags, CERTDB_VALID_CA);
	if (tClientCA)
		e_cert_trust_add_trust (&trust->sslFlags, CERTDB_TRUSTED_CLIENT_CA);
	if (tCA)
		e_cert_trust_add_trust (&trust->sslFlags, CERTDB_TRUSTED_CA);
	if (user)
		e_cert_trust_add_trust (&trust->sslFlags, CERTDB_USER);
	if (warn)
		e_cert_trust_add_trust (&trust->sslFlags, CERTDB_SEND_WARN);
}

void
e_cert_trust_set_email_trust (CERTCertTrust *trust,
                              PRBool peer,
                              PRBool tPeer,
                              PRBool ca,
                              PRBool tCA,
                              PRBool tClientCA,
                              PRBool user,
                              PRBool warn)
{
	trust->emailFlags = 0;
	if (peer || tPeer)
		e_cert_trust_add_trust (&trust->emailFlags, CERTDB_TERMINAL_RECORD);
	if (tPeer)
		e_cert_trust_add_trust (&trust->emailFlags, CERTDB_TRUSTED);
	if (ca || tCA)
		e_cert_trust_add_trust (&trust->emailFlags, CERTDB_VALID_CA);
	if (tClientCA)
		e_cert_trust_add_trust (&trust->emailFlags, CERTDB_TRUSTED_CLIENT_CA);
	if (tCA)
		e_cert_trust_add_trust (&trust->emailFlags, CERTDB_TRUSTED_CA);
	if (user)
		e_cert_trust_add_trust (&trust->emailFlags, CERTDB_USER);
	if (warn)
		e_cert_trust_add_trust (&trust->emailFlags, CERTDB_SEND_WARN);
}

void
e_cert_trust_set_objsign_trust (CERTCertTrust *trust,
                                PRBool peer,
                                PRBool tPeer,
                                PRBool ca,
                                PRBool tCA,
                                PRBool tClientCA,
                                PRBool user,
                                PRBool warn)
{
	trust->objectSigningFlags = 0;
	if (peer || tPeer)
		e_cert_trust_add_trust (
			&trust->objectSigningFlags,
			CERTDB_TERMINAL_RECORD);
	if (tPeer)
		e_cert_trust_add_trust (
			&trust->objectSigningFlags,
			CERTDB_TRUSTED);
	if (ca || tCA)
		e_cert_trust_add_trust (
			&trust->objectSigningFlags,
			CERTDB_VALID_CA);
	if (tClientCA)
		e_cert_trust_add_trust (
			&trust->objectSigningFlags,
			CERTDB_TRUSTED_CLIENT_CA);
	if (tCA)
		e_cert_trust_add_trust (
			&trust->objectSigningFlags,
			CERTDB_TRUSTED_CA);
	if (user)
		e_cert_trust_add_trust (
			&trust->objectSigningFlags,
			CERTDB_USER);
	if (warn)
		e_cert_trust_add_trust (
			&trust->objectSigningFlags,
			CERTDB_SEND_WARN);
}

void
e_cert_trust_set_valid_ca (CERTCertTrust *trust)
{
	e_cert_trust_set_ssl_trust (
		trust, PR_FALSE, PR_FALSE, PR_TRUE,
		PR_FALSE, PR_FALSE, PR_FALSE, PR_FALSE);

	e_cert_trust_set_email_trust (
		trust, PR_FALSE, PR_FALSE, PR_TRUE,
		PR_FALSE, PR_FALSE, PR_FALSE, PR_FALSE);

	e_cert_trust_set_objsign_trust (
		trust, PR_FALSE, PR_FALSE, PR_TRUE,
		PR_FALSE, PR_FALSE, PR_FALSE, PR_FALSE);
}

void
e_cert_trust_set_valid_peer (CERTCertTrust *trust)
{
	e_cert_trust_set_ssl_trust (
		trust, PR_TRUE, PR_FALSE, PR_FALSE,
		PR_FALSE, PR_FALSE, PR_FALSE, PR_FALSE);

	e_cert_trust_set_email_trust (
		trust, PR_TRUE, PR_FALSE, PR_FALSE,
		PR_FALSE, PR_FALSE, PR_FALSE, PR_FALSE);

	e_cert_trust_set_objsign_trust (
		trust, PR_TRUE, PR_FALSE, PR_FALSE,
		PR_FALSE, PR_FALSE, PR_FALSE, PR_FALSE);
}

PRBool
e_cert_trust_has_any_ca (CERTCertTrust *trust)
{
	if (e_cert_trust_has_trust (trust->sslFlags, CERTDB_VALID_CA) ||
		e_cert_trust_has_trust (trust->emailFlags, CERTDB_VALID_CA) ||
		e_cert_trust_has_trust (trust->objectSigningFlags, CERTDB_VALID_CA))
		return PR_TRUE;

	return PR_FALSE;
}

PRBool
e_cert_trust_has_peer (CERTCertTrust *trust,
                       PRBool checkSSL,
                       PRBool checkEmail,
                       PRBool checkObjSign)
{
	if (checkSSL && !e_cert_trust_has_trust (
		trust->sslFlags, CERTDB_TERMINAL_RECORD))
		return PR_FALSE;

	if (checkEmail && !e_cert_trust_has_trust (
		trust->emailFlags, CERTDB_TERMINAL_RECORD))
		return PR_FALSE;

	if (checkObjSign && !e_cert_trust_has_trust (
		trust->objectSigningFlags, CERTDB_TERMINAL_RECORD))
		return PR_FALSE;

	return PR_TRUE;
}

PRBool
e_cert_trust_has_any_user (CERTCertTrust *trust)
{
	if (e_cert_trust_has_trust (trust->sslFlags, CERTDB_USER) ||
		e_cert_trust_has_trust (trust->emailFlags, CERTDB_USER) ||
		e_cert_trust_has_trust (trust->objectSigningFlags, CERTDB_USER))
		return PR_TRUE;

	return PR_FALSE;
}

PRBool
e_cert_trust_has_trusted_ca (CERTCertTrust *trust,
                             PRBool checkSSL,
                             PRBool checkEmail,
                             PRBool checkObjSign)
{
	if (checkSSL && !(e_cert_trust_has_trust (
		trust->sslFlags, CERTDB_TRUSTED_CA) ||
		e_cert_trust_has_trust (
		trust->sslFlags, CERTDB_TRUSTED_CLIENT_CA)))
		return PR_FALSE;

	if (checkEmail && !(e_cert_trust_has_trust (
		trust->emailFlags, CERTDB_TRUSTED_CA) ||
		e_cert_trust_has_trust (
		trust->emailFlags, CERTDB_TRUSTED_CLIENT_CA)))
		return PR_FALSE;

	if (checkObjSign && !(e_cert_trust_has_trust (
		trust->objectSigningFlags, CERTDB_TRUSTED_CA) ||
		e_cert_trust_has_trust (
		trust->objectSigningFlags, CERTDB_TRUSTED_CLIENT_CA)))
		return PR_FALSE;

	return PR_TRUE;
}

PRBool
e_cert_trust_has_trusted_peer (CERTCertTrust *trust,
                               PRBool checkSSL,
                               PRBool checkEmail,
                               PRBool checkObjSign)
{
	if (checkSSL && !(e_cert_trust_has_trust (
		trust->sslFlags, CERTDB_TRUSTED)))
		return PR_FALSE;

	if (checkEmail && !(e_cert_trust_has_trust (
		trust->emailFlags, CERTDB_TRUSTED)))
		return PR_FALSE;

	if (checkObjSign && !(e_cert_trust_has_trust (
		trust->objectSigningFlags, CERTDB_TRUSTED)))
		return PR_FALSE;

	return PR_TRUE;
}

void
e_cert_trust_add_trust (guint *t,
                        guint v)
{
	*t |= v;
}

PRBool
e_cert_trust_has_trust (guint t,
                        guint v)
{
	return (t & v);
}

