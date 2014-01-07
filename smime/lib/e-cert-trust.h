/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_CERT_TRUST_H_
#define _E_CERT_TRUST_H_

#include <glib.h>
#include <cert.h>
#include <certdb.h>

G_BEGIN_DECLS

void e_cert_trust_init (CERTCertTrust *trust);
void e_cert_trust_init_with_values (CERTCertTrust *trust,
				    guint ssl,
				    guint email,
				    guint objsign);
void e_cert_trust_add_ca_trust (CERTCertTrust *trust, PRBool ssl, PRBool email, PRBool objSign);
void e_cert_trust_add_peer_trust (CERTCertTrust *trust, PRBool ssl, PRBool email, PRBool objSign);
void e_cert_trust_set_ssl_trust (CERTCertTrust *trust,
				 PRBool peer, PRBool tPeer,
				 PRBool ca,   PRBool tCA, PRBool tClientCA,
				 PRBool user, PRBool warn);
void e_cert_trust_set_email_trust (CERTCertTrust *trust,
				   PRBool peer, PRBool tPeer,
				   PRBool ca,   PRBool tCA, PRBool tClientCA,
				   PRBool user, PRBool warn);
void e_cert_trust_set_objsign_trust (CERTCertTrust *trust,
				     PRBool peer, PRBool tPeer,
				     PRBool ca,   PRBool tCA, PRBool tClientCA,
				     PRBool user, PRBool warn);
void e_cert_trust_set_valid_ca (CERTCertTrust *trust);
void e_cert_trust_set_valid_peer (CERTCertTrust *trust);
PRBool e_cert_trust_has_any_ca (CERTCertTrust *trust);
PRBool e_cert_trust_has_peer (CERTCertTrust *trust,
				PRBool checkSSL,
				PRBool checkEmail,
				PRBool checkObjSign);
PRBool e_cert_trust_has_any_user (CERTCertTrust *trust);
PRBool e_cert_trust_has_trusted_ca (CERTCertTrust *trust,
				      PRBool checkSSL,
				      PRBool checkEmail,
				      PRBool checkObjSign);
PRBool e_cert_trust_has_trusted_peer (CERTCertTrust *trust,
					PRBool checkSSL,
					PRBool checkEmail,
					PRBool checkObjSign);
void e_cert_trust_add_trust (guint *t, guint v);
PRBool e_cert_trust_has_trust (guint t, guint v);

G_END_DECLS

#endif /* _E_CERT_H_ */
