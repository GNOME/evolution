/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Chris Toshok <toshok@ximian.com>
 *
 *  Copyright (C) 2003 Novell, Inc. (www.novell.com)
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

#ifndef _E_CERT_TRUST_H_
#define _E_CERT_TRUST_H_

#include <glib.h>
#include <cert.h>
#include <certdb.h>

G_BEGIN_DECLS

void e_cert_trust_init (CERTCertTrust *trust);
void e_cert_trust_init_with_values (CERTCertTrust *trust,
				    unsigned int ssl, 
				    unsigned int email, 
				    unsigned int objsign);
void e_cert_trust_copy (CERTCertTrust *dst_trust, CERTCertTrust *src_trust);
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
void e_cert_trust_set_trusted_server_ca (CERTCertTrust *trust);
void e_cert_trust_set_trusted_ca (CERTCertTrust *trust);
void e_cert_trust_set_valid_peer (CERTCertTrust *trust);
void e_cert_trust_set_valid_server_peer (CERTCertTrust *trust);
void e_cert_trust_set_trusted_peer (CERTCertTrust *trust);
void e_cert_trust_set_user (CERTCertTrust *trust);
PRBool e_cert_trust_has_any_ca (CERTCertTrust *trust);
PRBool e_cert_trust_has_ca (CERTCertTrust *trust,
			      PRBool checkSSL, 
			      PRBool checkEmail,  
			      PRBool checkObjSign);
PRBool e_cert_trust_has_peer (CERTCertTrust *trust,
				PRBool checkSSL, 
				PRBool checkEmail,  
				PRBool checkObjSign);
PRBool e_cert_trust_has_any_user (CERTCertTrust *trust);
PRBool e_cert_trust_has_user (CERTCertTrust *trust,
				PRBool checkSSL, 
				PRBool checkEmail,  
				PRBool checkObjSign);
PRBool e_cert_trust_has_trusted_ca (CERTCertTrust *trust,
				      PRBool checkSSL, 
				      PRBool checkEmail,  
				      PRBool checkObjSign);
PRBool e_cert_trust_has_trusted_peer (CERTCertTrust *trust,
					PRBool checkSSL, 
					PRBool checkEmail,  
					PRBool checkObjSign);
void e_cert_trust_add_trust (unsigned int *t, unsigned int v);
PRBool e_cert_trust_has_trust (unsigned int t, unsigned int v);

G_END_DECLS

#endif /* _E_CERT_H_ */
