/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-imap-auth.c: IMAP AUTHENTICATE implementations */

/*
 *  Authors: Dan Winship <danw@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
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

#include <config.h>

#include <string.h>

#ifdef HAVE_KRB4
#include <krb.h>
/* MIT krb4 des.h #defines _. Sigh. We don't need it. */
#undef _
#endif

#include "camel-exception.h"
#include "camel-mime-utils.h"

#include "camel-imap-auth.h"
#include "camel-imap-command.h"
#include "camel-imap-utils.h"

static char *
base64_encode_simple (const char *data, int len)
{
	unsigned char *out;
	int state = 0, outlen;
	unsigned int save = 0;

	out = g_malloc (len * 4 / 3 + 5);
	outlen = base64_encode_close ((unsigned char *)data, len, FALSE,
				      out, &state, &save);
	out[outlen] = '\0';
	return (char *)out;
}

static int
base64_decode_simple (char *data, int len)
{
	int state = 0;
	unsigned int save = 0;

	return base64_decode_step ((unsigned char *)data, len,
				   (unsigned char *)data, &state, &save);
}

#ifdef HAVE_KRB4
#define IMAP_KERBEROS_V4_PROTECTION_NONE      1
#define IMAP_KERBEROS_V4_PROTECTION_INTEGRITY 2
#define IMAP_KERBEROS_V4_PROTECTION_PRIVACY   4

gboolean
imap_try_kerberos_v4_auth (CamelImapStore *store, CamelException *ex)
{
	CamelImapResponse *response;
	char *resp, *data;
	int status, len;
	char *inst, *realm, *buf, *username;
	guint32 nonce_n, nonce_h, plus1;
	struct hostent *h;
	KTEXT_ST authenticator;
	CREDENTIALS credentials;
	des_cblock session;
	des_key_schedule schedule;

	/* The kickoff. */
	response = camel_imap_command (store, NULL, ex,
				       "AUTHENTICATE KERBEROS_V4");
	if (!response)
		return FALSE;
	resp = camel_imap_response_extract_continuation (response, ex);
	if (!resp)
		return FALSE;
	data = imap_next_word (resp);

	/* First server response is a base64-encoded 32-bit random number
	 * ("nonce") in network byte order.
	 */
	if (strlen (data) != 8 || base64_decode_simple (data, 8) != 4) {
		g_free (resp);
		goto break_and_lose;
	}
	memcpy (&nonce_n, data, 4);
	g_free (resp);
	nonce_h = ntohl (nonce_n);

	/* Our response is an authenticator including that number. */
	h = camel_service_gethost (CAMEL_SERVICE (store), ex);
	if (!h)
		goto break_and_lose;
	inst = g_strndup (h->h_name, strcspn (h->h_name, "."));
	g_strdown (inst);
	realm = g_strdup (krb_realmofhost (h->h_name));
	status = krb_mk_req (&authenticator, "imap", inst, realm, nonce_h);
	if (status == KSUCCESS) {
		status = krb_get_cred ("imap", inst, realm, &credentials);
		memcpy (session, credentials.session, sizeof (session));
		memset (&credentials, 0, sizeof (credentials));
	}
	g_free (inst);
	g_free (realm);

	if (status != KSUCCESS) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
				      _("Could not get Kerberos ticket:\n%s"),
				      krb_err_txt[status]);
		goto break_and_lose;
	}
	des_key_sched (&session, schedule);

	buf = base64_encode_simple (authenticator.dat, authenticator.length);
	response = camel_imap_command_continuation (store, ex, buf);
	g_free (buf);
	if (!response)
		goto lose;
	resp = camel_imap_response_extract_continuation (response, ex);
	if (!resp)
		goto lose;
	data = imap_next_word (resp);

	len = strlen (data);
	base64_decode_simple (data, strlen (data));

	/* This one is encrypted. */
	des_ecb_encrypt ((des_cblock *)data, (des_cblock *)data, schedule, 0);

	/* Check that the returned value is the original nonce plus one. */
	memcpy (&plus1, data, 4);
	if (ntohl (plus1) != nonce_h + 1) {
		g_free (resp);
		goto lose;
	}

	/* "the fifth octet contain[s] a bit-mask specifying the
	 * protection mechanisms supported by the server"
	 */
	if (!(data[4] & IMAP_KERBEROS_V4_PROTECTION_NONE)) {
		g_warning ("Server does not support `no protection' :-(");
		g_free (resp);
		goto break_and_lose;
	}
	g_free (resp);

	username = CAMEL_SERVICE (store)->url->user;
	len = strlen (username) + 9;
	len += 8 - len % 8;
	data = g_malloc0 (len);
	memcpy (data, &nonce_n, 4);
	data[4] = IMAP_KERBEROS_V4_PROTECTION_NONE;
	data[5] = data[6] = data[7] = 0;
	strcpy (data + 8, username);

	des_pcbc_encrypt ((des_cblock *)data, (des_cblock *)data, len,
			  schedule, &session, 1);
	memset (&session, 0, sizeof (session));
	buf = base64_encode_simple (data, len);
	g_free (data);

	response = camel_imap_command_continuation (store, ex, buf);
	if (!response)
		goto lose;
	camel_imap_response_free (response);
	return TRUE;

 break_and_lose:
	/* Get the server out of "waiting for continuation data" mode. */
	response = camel_imap_command_continuation (store, NULL, "*");
	if (response)
		camel_imap_response_free (response);

 lose:
	memset (&session, 0, sizeof (session));

	if (!camel_exception_is_set (ex)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
				     _("Bad authentication response from server."));
	}
	return FALSE;
}
#endif /* HAVE_KRB4 */
