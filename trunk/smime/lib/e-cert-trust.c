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

/* this code is pretty much cut&pasted and renamed from mozilla.
   here's their copyright/blurb */

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
 * Contributor(s):
 *  Ian McGreer <mcgreer@netscape.com>
 *  Javier Delgadillo <javi@netscape.com>
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

#include "e-cert-trust.h"

void
e_cert_trust_init (CERTCertTrust *trust)
{
  memset(trust, 0, sizeof(CERTCertTrust));
}

void
e_cert_trust_init_with_values (CERTCertTrust *trust,
			       unsigned int ssl, 
			       unsigned int email, 
			       unsigned int objsign)
{
  memset(trust, 0, sizeof(CERTCertTrust));
  e_cert_trust_add_trust(&trust->sslFlags, ssl);
  e_cert_trust_add_trust(&trust->emailFlags, email);
  e_cert_trust_add_trust(&trust->objectSigningFlags, objsign);
}

void
e_cert_trust_copy (CERTCertTrust *trust, CERTCertTrust *t)
{
  if (t)
    memcpy(trust, t, sizeof(CERTCertTrust));
  else
    memset(trust, 0, sizeof(CERTCertTrust)); 
}

void
e_cert_trust_add_ca_trust (CERTCertTrust *trust, PRBool ssl, PRBool email, PRBool objSign)
{
  if (ssl) {
    e_cert_trust_add_trust(&trust->sslFlags, CERTDB_TRUSTED_CA);
    e_cert_trust_add_trust(&trust->sslFlags, CERTDB_TRUSTED_CLIENT_CA);
  }
  if (email) {
    e_cert_trust_add_trust(&trust->emailFlags, CERTDB_TRUSTED_CA);
    e_cert_trust_add_trust(&trust->emailFlags, CERTDB_TRUSTED_CLIENT_CA);
  }
  if (objSign) {
    e_cert_trust_add_trust(&trust->objectSigningFlags, CERTDB_TRUSTED_CA);
    e_cert_trust_add_trust(&trust->objectSigningFlags, CERTDB_TRUSTED_CLIENT_CA);
  }
}

void
e_cert_trust_add_peer_trust (CERTCertTrust *trust, PRBool ssl, PRBool email, PRBool objSign)
{
  if (ssl)
    e_cert_trust_add_trust(&trust->sslFlags, CERTDB_TRUSTED);
  if (email)
    e_cert_trust_add_trust(&trust->emailFlags, CERTDB_TRUSTED);
  if (objSign)
    e_cert_trust_add_trust(&trust->objectSigningFlags, CERTDB_TRUSTED);
}

void
e_cert_trust_set_ssl_trust (CERTCertTrust *trust,
			    PRBool peer, PRBool tPeer,
			    PRBool ca,   PRBool tCA, PRBool tClientCA,
			    PRBool user, PRBool warn)
{
  trust->sslFlags = 0;
  if (peer || tPeer)
    e_cert_trust_add_trust(&trust->sslFlags, CERTDB_VALID_PEER);
  if (tPeer)
    e_cert_trust_add_trust(&trust->sslFlags, CERTDB_TRUSTED);
  if (ca || tCA)
    e_cert_trust_add_trust(&trust->sslFlags, CERTDB_VALID_CA);
  if (tClientCA)
    e_cert_trust_add_trust(&trust->sslFlags, CERTDB_TRUSTED_CLIENT_CA);
  if (tCA)
    e_cert_trust_add_trust(&trust->sslFlags, CERTDB_TRUSTED_CA);
  if (user)
    e_cert_trust_add_trust(&trust->sslFlags, CERTDB_USER);
  if (warn)
    e_cert_trust_add_trust(&trust->sslFlags, CERTDB_SEND_WARN);
}

void
e_cert_trust_set_email_trust (CERTCertTrust *trust,
			      PRBool peer, PRBool tPeer,
			      PRBool ca,   PRBool tCA, PRBool tClientCA,
			      PRBool user, PRBool warn)
{
  trust->emailFlags = 0;
  if (peer || tPeer)
    e_cert_trust_add_trust(&trust->emailFlags, CERTDB_VALID_PEER);
  if (tPeer)
    e_cert_trust_add_trust(&trust->emailFlags, CERTDB_TRUSTED);
  if (ca || tCA)
    e_cert_trust_add_trust(&trust->emailFlags, CERTDB_VALID_CA);
  if (tClientCA)
    e_cert_trust_add_trust(&trust->emailFlags, CERTDB_TRUSTED_CLIENT_CA);
  if (tCA)
    e_cert_trust_add_trust(&trust->emailFlags, CERTDB_TRUSTED_CA);
  if (user)
    e_cert_trust_add_trust(&trust->emailFlags, CERTDB_USER);
  if (warn)
    e_cert_trust_add_trust(&trust->emailFlags, CERTDB_SEND_WARN);
}

void
e_cert_trust_set_objsign_trust (CERTCertTrust *trust,
				PRBool peer, PRBool tPeer,
				PRBool ca,   PRBool tCA, PRBool tClientCA,
				PRBool user, PRBool warn)
{
  trust->objectSigningFlags = 0;
  if (peer || tPeer)
    e_cert_trust_add_trust(&trust->objectSigningFlags, CERTDB_VALID_PEER);
  if (tPeer)
    e_cert_trust_add_trust(&trust->objectSigningFlags, CERTDB_TRUSTED);
  if (ca || tCA)
    e_cert_trust_add_trust(&trust->objectSigningFlags, CERTDB_VALID_CA);
  if (tClientCA)
    e_cert_trust_add_trust(&trust->objectSigningFlags, CERTDB_TRUSTED_CLIENT_CA);
  if (tCA)
    e_cert_trust_add_trust(&trust->objectSigningFlags, CERTDB_TRUSTED_CA);
  if (user)
    e_cert_trust_add_trust(&trust->objectSigningFlags, CERTDB_USER);
  if (warn)
    e_cert_trust_add_trust(&trust->objectSigningFlags, CERTDB_SEND_WARN);
}

void
e_cert_trust_set_valid_ca (CERTCertTrust *trust)
{
  e_cert_trust_set_ssl_trust (trust,
			      PR_FALSE, PR_FALSE,
			      PR_TRUE, PR_FALSE, PR_FALSE,
			      PR_FALSE, PR_FALSE);
  e_cert_trust_set_email_trust (trust,
				PR_FALSE, PR_FALSE,
				PR_TRUE, PR_FALSE, PR_FALSE,
				PR_FALSE, PR_FALSE);
  e_cert_trust_set_objsign_trust (trust,
				  PR_FALSE, PR_FALSE,
				  PR_TRUE, PR_FALSE, PR_FALSE,
				  PR_FALSE, PR_FALSE);
}

void
e_cert_trust_set_trusted_server_ca (CERTCertTrust *trust)
{
  e_cert_trust_set_ssl_trust (trust,
			      PR_FALSE, PR_FALSE,
			      PR_TRUE, PR_TRUE, PR_FALSE,
			      PR_FALSE, PR_FALSE);
  e_cert_trust_set_email_trust (trust,
				PR_FALSE, PR_FALSE,
				PR_TRUE, PR_TRUE, PR_FALSE,
				PR_FALSE, PR_FALSE);
  e_cert_trust_set_objsign_trust (trust,
				  PR_FALSE, PR_FALSE,
				  PR_TRUE, PR_TRUE, PR_FALSE,
				  PR_FALSE, PR_FALSE);
}

void
e_cert_trust_set_trusted_ca (CERTCertTrust *trust)
{
  e_cert_trust_set_ssl_trust (trust,
			      PR_FALSE, PR_FALSE,
			      PR_TRUE, PR_TRUE, PR_TRUE,
			      PR_FALSE, PR_FALSE);
  e_cert_trust_set_email_trust (trust,
				PR_FALSE, PR_FALSE,
				PR_TRUE, PR_TRUE, PR_TRUE,
				PR_FALSE, PR_FALSE);
  e_cert_trust_set_objsign_trust (trust,
				  PR_FALSE, PR_FALSE,
				  PR_TRUE, PR_TRUE, PR_TRUE,
				  PR_FALSE, PR_FALSE);
}

void 
e_cert_trust_set_valid_peer (CERTCertTrust *trust)
{
  e_cert_trust_set_ssl_trust (trust,
			      PR_TRUE, PR_FALSE,
			      PR_FALSE, PR_FALSE, PR_FALSE,
			      PR_FALSE, PR_FALSE);
  e_cert_trust_set_email_trust (trust,
				PR_TRUE, PR_FALSE,
				PR_FALSE, PR_FALSE, PR_FALSE,
				PR_FALSE, PR_FALSE);
  e_cert_trust_set_objsign_trust (trust,
				  PR_TRUE, PR_FALSE,
				  PR_FALSE, PR_FALSE, PR_FALSE,
				  PR_FALSE, PR_FALSE);
}

void 
e_cert_trust_set_valid_server_peer (CERTCertTrust *trust)
{
  e_cert_trust_set_ssl_trust (trust,
			      PR_TRUE, PR_FALSE,
			      PR_FALSE, PR_FALSE, PR_FALSE,
			      PR_FALSE, PR_FALSE);
  e_cert_trust_set_email_trust (trust,
				PR_FALSE, PR_FALSE,
				PR_FALSE, PR_FALSE, PR_FALSE,
				PR_FALSE, PR_FALSE);
  e_cert_trust_set_objsign_trust (trust,
				  PR_FALSE, PR_FALSE,
				  PR_FALSE, PR_FALSE, PR_FALSE,
				  PR_FALSE, PR_FALSE);
}

void 
e_cert_trust_set_trusted_peer (CERTCertTrust *trust)
{
  e_cert_trust_set_ssl_trust (trust,
			      PR_TRUE, PR_TRUE,
			      PR_FALSE, PR_FALSE, PR_FALSE,
			      PR_FALSE, PR_FALSE);
  e_cert_trust_set_email_trust (trust,
				PR_TRUE, PR_TRUE,
				PR_FALSE, PR_FALSE, PR_FALSE,
				PR_FALSE, PR_FALSE);
  e_cert_trust_set_objsign_trust (trust,
				  PR_TRUE, PR_TRUE,
				  PR_FALSE, PR_FALSE, PR_FALSE,
				  PR_FALSE, PR_FALSE);
}

void
e_cert_trust_set_user (CERTCertTrust *trust)
{
  e_cert_trust_set_ssl_trust (trust,
			      PR_FALSE, PR_FALSE,
			      PR_FALSE, PR_FALSE, PR_FALSE,
			      PR_TRUE, PR_FALSE);
  e_cert_trust_set_email_trust (trust,
				PR_FALSE, PR_FALSE,
				PR_FALSE, PR_FALSE, PR_FALSE,
				PR_TRUE, PR_FALSE);
  e_cert_trust_set_objsign_trust (trust,
				  PR_FALSE, PR_FALSE,
				  PR_FALSE, PR_FALSE, PR_FALSE,
				  PR_TRUE, PR_FALSE);
}

PRBool
e_cert_trust_has_any_ca (CERTCertTrust *trust)
{
  if (e_cert_trust_has_trust(trust->sslFlags, CERTDB_VALID_CA) ||
      e_cert_trust_has_trust(trust->emailFlags, CERTDB_VALID_CA) ||
      e_cert_trust_has_trust(trust->objectSigningFlags, CERTDB_VALID_CA))
    return PR_TRUE;
  return PR_FALSE;
}

PRBool
e_cert_trust_has_ca (CERTCertTrust *trust,
		     PRBool checkSSL, 
		     PRBool checkEmail,  
		     PRBool checkObjSign)
{
  if (checkSSL && !e_cert_trust_has_trust(trust->sslFlags, CERTDB_VALID_CA))
    return PR_FALSE;
  if (checkEmail && !e_cert_trust_has_trust(trust->emailFlags, CERTDB_VALID_CA))
    return PR_FALSE;
  if (checkObjSign && !e_cert_trust_has_trust(trust->objectSigningFlags, CERTDB_VALID_CA))
    return PR_FALSE;
  return PR_TRUE;
}

PRBool
e_cert_trust_has_peer (CERTCertTrust *trust,
		       PRBool checkSSL, 
		       PRBool checkEmail,  
		       PRBool checkObjSign)
{
  if (checkSSL && !e_cert_trust_has_trust(trust->sslFlags, CERTDB_VALID_PEER))
    return PR_FALSE;
  if (checkEmail && !e_cert_trust_has_trust(trust->emailFlags, CERTDB_VALID_PEER))
    return PR_FALSE;
  if (checkObjSign && !e_cert_trust_has_trust(trust->objectSigningFlags, CERTDB_VALID_PEER))
    return PR_FALSE;
  return PR_TRUE;
}

PRBool
e_cert_trust_has_any_user (CERTCertTrust *trust)
{
  if (e_cert_trust_has_trust(trust->sslFlags, CERTDB_USER) ||
      e_cert_trust_has_trust(trust->emailFlags, CERTDB_USER) ||
      e_cert_trust_has_trust(trust->objectSigningFlags, CERTDB_USER))
    return PR_TRUE;
  return PR_FALSE;
}

PRBool
e_cert_trust_has_user (CERTCertTrust *trust,
		       PRBool checkSSL, 
		       PRBool checkEmail,  
		       PRBool checkObjSign)
{
  if (checkSSL && !e_cert_trust_has_trust(trust->sslFlags, CERTDB_USER))
    return PR_FALSE;
  if (checkEmail && !e_cert_trust_has_trust(trust->emailFlags, CERTDB_USER))
    return PR_FALSE;
  if (checkObjSign && !e_cert_trust_has_trust(trust->objectSigningFlags, CERTDB_USER))
    return PR_FALSE;
  return PR_TRUE;
}

PRBool
e_cert_trust_has_trusted_ca (CERTCertTrust *trust,
			     PRBool checkSSL, 
			     PRBool checkEmail,  
			     PRBool checkObjSign)
{
  if (checkSSL && !(e_cert_trust_has_trust(trust->sslFlags, CERTDB_TRUSTED_CA) ||
                    e_cert_trust_has_trust(trust->sslFlags, CERTDB_TRUSTED_CLIENT_CA)))
    return PR_FALSE;
  if (checkEmail && !(e_cert_trust_has_trust(trust->emailFlags, CERTDB_TRUSTED_CA) ||
                      e_cert_trust_has_trust(trust->emailFlags, CERTDB_TRUSTED_CLIENT_CA)))
    return PR_FALSE;
  if (checkObjSign && 
       !(e_cert_trust_has_trust(trust->objectSigningFlags, CERTDB_TRUSTED_CA) ||
         e_cert_trust_has_trust(trust->objectSigningFlags, CERTDB_TRUSTED_CLIENT_CA)))
    return PR_FALSE;
  return PR_TRUE;
}

PRBool
e_cert_trust_has_trusted_peer (CERTCertTrust *trust,
			       PRBool checkSSL, 
			       PRBool checkEmail,  
			       PRBool checkObjSign)
{
  if (checkSSL && !(e_cert_trust_has_trust(trust->sslFlags, CERTDB_TRUSTED)))
    return PR_FALSE;
  if (checkEmail && !(e_cert_trust_has_trust(trust->emailFlags, CERTDB_TRUSTED)))
    return PR_FALSE;
  if (checkObjSign && 
       !(e_cert_trust_has_trust(trust->objectSigningFlags, CERTDB_TRUSTED)))
    return PR_FALSE;
  return PR_TRUE;
}

void
e_cert_trust_add_trust (unsigned int *t, unsigned int v)
{
  *t |= v;
}

PRBool
e_cert_trust_has_trust (unsigned int t, unsigned int v)
{
  return (t & v);
}

