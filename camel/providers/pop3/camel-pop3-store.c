/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-pop3-store.c : class for an pop3 store */

/* 
 * Authors:
 *   Dan Winship <danw@helixcode.com>
 *
 * Copyright (C) 2000 Helix Code, Inc. (www.helixcode.com)
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

#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "camel-pop3-store.h"
#include "camel-pop3-folder.h"
#include "camel-stream-buffer.h"
#include "camel-stream-fs.h"
#include "camel-exception.h"
#include "md5-utils.h"
#include "url-util.h"



static gboolean _connect (CamelService *service, CamelException *ex);


static void
camel_pop3_store_class_init (CamelPop3StoreClass *camel_pop3_store_class)
{
	CamelServiceClass *camel_service_class =
		CAMEL_SERVICE_CLASS (camel_pop3_store_class);
	
	/* virtual method overload */
	camel_service_class->connect = _connect;
}



static void
camel_pop3_store_init (gpointer object, gpointer klass)
{
	CamelService *service = CAMEL_SERVICE (object);

	service->url_flags = ( CAMEL_SERVICE_URL_NEED_USER |
			       CAMEL_SERVICE_URL_NEED_HOST );
}




GtkType
camel_pop3_store_get_type (void)
{
	static GtkType camel_pop3_store_type = 0;

	if (!camel_pop3_store_type) {
		GtkTypeInfo camel_pop3_store_info =	
		{
			"CamelPop3Store",
			sizeof (CamelPop3Store),
			sizeof (CamelPop3StoreClass),
			(GtkClassInitFunc) camel_pop3_store_class_init,
			(GtkObjectInitFunc) camel_pop3_store_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		camel_pop3_store_type = gtk_type_unique (CAMEL_STORE_TYPE, &camel_pop3_store_info);
	}

	return camel_pop3_store_type;
}



static gboolean
_connect (CamelService *service, CamelException *ex)
{
	struct hostent *h;
	struct sockaddr_in sin;
	int fd, status, apoplen;
	char *buf, *apoptime, *pass;
	CamelPop3Store *store = CAMEL_POP3_STORE (service);

	if (!CAMEL_SERVICE_CLASS (service)->connect (service, ex))
		return FALSE;

	h = gethostbyname (service->url->host);
	if (!h) {
		extern int h_errno;
		if (h_errno == HOST_NOT_FOUND || h_errno == NO_DATA) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_URL_INVALID,
					      "No such host %s.",
					      service->url->host);
		} else {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					      "Temporarily unable to look up "
					      "hostname %s.",
					      service->url->host);
		}
		return FALSE;
	}

	sin.sin_family = h->h_addrtype;
	sin.sin_port = htons (atoi (service->url->port)); /* XXX */
	memcpy (&sin.sin_addr, h->h_addr, sizeof (sin.sin_addr));

	fd = socket (h->h_addrtype, SOCK_STREAM, 0);
	if (fd == -1 ||
	    connect (fd, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not connect to %s (port %s): %s",
				      service->url->host, service->url->port,
				      strerror(errno));
		if (fd > -1)
			close (fd);
		return FALSE;
	}

	store->stream = CAMEL_STREAM_BUFFER (camel_stream_buffer_new (camel_stream_fs_new_with_fd (fd), CAMEL_STREAM_BUFFER_READ));

	/* Read the greeting, note APOP timestamp, if any. */
	buf = camel_stream_buffer_read_line (CAMEL_STREAM_BUFFER (store->stream));
	if (!buf)
		return -1;
	apoptime = strchr (buf, '<');
	if (apoptime) {
		int len = strcspn (apoptime, ">");

		apoptime = g_strndup (apoptime, len);
	} else
		apoptime = NULL;
	g_free (buf);

	/* Authenticate via APOP if we can, USER/PASS if we can't. */
	status = CAMEL_POP3_FAIL;
	if (apoptime && apoptime[apoplen] == '>') {
		char *secret, md5asc[32], *d;
		unsigned char md5sum[16], *s;

		secret = g_strdup_printf("%.*s%s", apoplen + 1, apoptime,
					 pass);
		md5_get_digest(secret, strlen(secret), md5sum);
		g_free(secret);

		for (s = md5sum, d = md5asc; d < md5asc + 32; s++, d += 2)
			sprintf(d, "%.2x", *s);

		status = camel_pop3_command(store->stream, NULL, "APOP %s %s",
					    service->url->user, md5asc);
	}
	g_free(buf);

	if (status != CAMEL_POP3_OK ) {
		status = camel_pop3_command(store->stream, NULL, "USER %s",
					    service->url->user);
		if (status == CAMEL_POP3_OK) {
			status = camel_pop3_command(store->stream, NULL,
						    "PASS %s", pass);
		}
	}

	if (status != CAMEL_POP3_OK) {
		camel_exception_set (ex,
				     CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
				     "Unable to authenticate to POP server.");
		return FALSE;
	}

	return TRUE;
}

int
camel_pop3_command (CamelStreamBuffer *stream, char **ret, char *fmt, ...)
{
	char *cmdbuf, *respbuf;
	va_list ap;
	int status, i;
	GPtrArray *data;

	va_start (ap, fmt);
	cmdbuf = g_strdup_vprintf (fmt, ap);
	va_end (ap);

	/* Send the command */
	camel_stream_write (CAMEL_STREAM (stream), cmdbuf, strlen (cmdbuf));
	g_free (cmdbuf);
	camel_stream_write (CAMEL_STREAM (stream), "\r\n", 2);

	/* Read the response */
	respbuf = camel_stream_buffer_read_line (stream);
	if (!strncmp (respbuf, "+OK", 3))
		status = CAMEL_POP3_OK;
	else if (!strncmp (respbuf, "-ERR", 4))
		status = CAMEL_POP3_ERR;
	else
		status = CAMEL_POP3_FAIL;
	g_free (respbuf);

	if (status != CAMEL_POP3_OK || !ret)
		return status;

	/* Read the additional data. */
	data = g_ptr_array_new ();
	while (1) {
		respbuf = camel_stream_buffer_read_line (stream);
		if (!respbuf) {
			status = CAMEL_POP3_FAIL;
			break;
		}

		if (!strcmp (respbuf, "."))
			break;
		if (*respbuf == '.')
			memmove (respbuf, respbuf + 1, strlen (respbuf));
		g_ptr_array_add (data, respbuf);
	}

	if (status == CAMEL_POP3_OK) {
		g_ptr_array_add (data, NULL);
		*ret = g_strjoinv ("\n", (char **)data->pdata);
	}

	for (i = 0; i < data->len; i++)
		g_free (data->pdata[i]);
	g_ptr_array_free (data, TRUE);

	return status;
}
