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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <camel-nntp-auth.h>
#include <camel-nntp-store.h>
#include <camel-nntp-resp-codes.h>
#include <camel-exception.h>
#include <camel-session.h>

int
camel_nntp_auth_authenticate (CamelNNTPStore *store, CamelException *ex)
{
	CamelService *service = CAMEL_SERVICE (store);
	CamelSession *session = camel_service_get_session (service);
	int resp;

	if (!service->url->authmech && !service->url->passwd) {
		gchar *prompt;
			
		prompt = g_strdup_printf (_("Please enter the NNTP password for %s@%s"),
					  service->url->user, service->url->host);
		service->url->passwd =
			camel_session_query_authenticator (session,
							   CAMEL_AUTHENTICATOR_ASK, prompt,
							   TRUE, service, "password", ex);
		g_free (prompt);
			
		if (!service->url->passwd) {
			camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL, 
					     "You didn\'t enter a password.");
			resp = 666;
			goto done;
		}
	}

	/* first send username */
	resp = camel_nntp_command (store, ex, NULL, "AUTHINFO USER %s", service->url->user);

	if (resp == NNTP_AUTH_REJECTED) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
				      _("Server rejected username"));
		goto done;

	}
	else if (resp != NNTP_AUTH_CONTINUE) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
				      _("Failed to send username to server"));
		goto done;
	}

	/* then send the username if the server asks for it */
	resp = camel_nntp_command (store, ex, NULL, "AUTHINFO PASS %s", service->url->passwd);

	if (resp == NNTP_AUTH_REJECTED) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
				      _("Server rejected username/password"));
		goto done;
	}

 done:

	if (service->url->passwd) {
		/* let's be paranoid */
		memset (service->url->passwd, 0, strlen (service->url->passwd));
		g_free (service->url->passwd);
		service->url->passwd = NULL;
	}
	return resp;
}
