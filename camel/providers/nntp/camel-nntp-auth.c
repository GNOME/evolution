/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-nntp-auth.c : authentication for nntp */

/* 
 *
 * Copyright (C) 2000 Helix Code, Inc. <toshok@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


#include <camel-nntp-auth.h>
#include <camel-nntp-store.h>
#include <camel-nntp-resp-codes.h>
#include <camel-exception.h>

int
camel_nntp_auth_authenticate (CamelNNTPStore *store, CamelException *ex)
{
	int resp;

	/* first send username */
	resp = camel_nntp_command (store, ex, NULL, "AUTHINFO USER %s", "username"); /* XXX */

	if (resp == NNTP_AUTH_REJECTED) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
				      "Server rejected username");
		return resp;
	}
	else if (resp != NNTP_AUTH_CONTINUE) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
				      "Failed to send username to server");
		return resp;
	}

	/* then send the username if the server asks for it */
	resp = camel_nntp_command (store, ex, NULL, "AUTHINFO PASS %s", "password"); /* XXX */
	if (resp == NNTP_AUTH_REJECTED) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
				      "Server rejected username/password");
		return resp;
	}

	return resp;
}
