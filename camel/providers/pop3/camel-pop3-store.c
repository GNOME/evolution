/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-pop3-store.c : class for a pop3 store */

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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "camel-pop3-store.h"
#include "camel-pop3-folder.h"
#include "camel-stream-buffer.h"
#include "camel-stream-fs.h"
#include "camel-session.h"
#include "camel-exception.h"
#include "md5-utils.h"
#include "url-util.h"

/* Specified in RFC 1939 */
#define POP3_PORT 110

static CamelServiceClass *service_class = NULL;

static void finalize (GtkObject *object);

static gboolean pop3_connect (CamelService *service, CamelException *ex);
static gboolean pop3_disconnect (CamelService *service, CamelException *ex);
static GList *query_auth_types (CamelService *service);
static void free_auth_types (CamelService *service, GList *authtypes);

static CamelFolder *get_folder (CamelStore *store, const gchar *folder_name, 
				CamelException *ex);


static void
camel_pop3_store_class_init (CamelPop3StoreClass *camel_pop3_store_class)
{
	GtkObjectClass *object_class =
		GTK_OBJECT_CLASS (camel_pop3_store_class);
	CamelServiceClass *camel_service_class =
		CAMEL_SERVICE_CLASS (camel_pop3_store_class);
	CamelStoreClass *camel_store_class =
		CAMEL_STORE_CLASS (camel_pop3_store_class);
	
	service_class = gtk_type_class (camel_service_get_type ());

	/* virtual method overload */
	object_class->finalize = finalize;

	camel_service_class->connect = pop3_connect;
	camel_service_class->disconnect = pop3_disconnect;
	camel_service_class->query_auth_types = query_auth_types;
	camel_service_class->free_auth_types = free_auth_types;

	camel_store_class->get_root_folder = camel_pop3_folder_new;
	camel_store_class->get_default_folder = camel_pop3_folder_new;
	camel_store_class->get_folder = get_folder;
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

static void
finalize (GtkObject *object)
{
	CamelException ex;

	camel_exception_init (&ex);
	pop3_disconnect (CAMEL_SERVICE (object), &ex);
	camel_exception_free (&ex);
}


static CamelServiceAuthType password_authtype = {
	"Password/APOP",

	"This option will connect to the POP server using the APOP "
	"protocol if possible, or a plaintext password if not.",

	"",
	TRUE
};

static GList
*query_auth_types (CamelService *service)
{
	GList *ret;

	ret = g_list_append (NULL, &password_authtype);
	return ret;
}

static void
free_auth_types (CamelService *service, GList *authtypes)
{
	g_list_free (authtypes);
}

/**
 * camel_pop3_store_open: Connect to the server if we are currently
 * disconnected.
 * @store: the store
 * @ex: a CamelException
 *
 * The POP protocol does not allow deleted messages to be expunged
 * except by closing the connection. Thus, camel_pop3_folder_{open,close}
 * sometimes need to connect to or disconnect from the server. This
 * routine reconnects to the server if we have disconnected.
 *
 **/
void
camel_pop3_store_open (CamelPop3Store *store, CamelException *ex)
{
	CamelService *service = CAMEL_SERVICE (store);

	if (!camel_service_is_connected (service))
		pop3_connect (service, ex);
}

/**
 * camel_pop3_store_close: Close the connection to the server and
 * possibly expunge deleted messages.
 * @store: the store
 * @expunge: whether or not to expunge deleted messages
 * @ex: a CamelException
 *
 * See camel_pop3_store_open for an explanation of why this is needed.
 *
 **/
void
camel_pop3_store_close (CamelPop3Store *store, gboolean expunge,
			CamelException *ex)
{
	if (expunge)
		camel_pop3_command (store, NULL, "QUIT");
	else
		camel_pop3_command (store, NULL, "RSET");
	pop3_disconnect (CAMEL_SERVICE (store), ex);
}

static gboolean
pop3_connect (CamelService *service, CamelException *ex)
{
	struct hostent *h;
	struct sockaddr_in sin;
	int port, fd, status, apoplen;
	char *buf, *apoptime, *pass;
	CamelPop3Store *store = CAMEL_POP3_STORE (service);

	if (!service_class->connect (service, ex))
		return FALSE;

	h = camel_service_gethost (service, ex);
	if (!h)
		return FALSE;
	port = camel_service_getport (service, "pop3", POP3_PORT, "tcp", ex);
	if (port == -1)
		return FALSE;

	pass = g_strdup (service->url->passwd);
	if (!pass) {
		char *prompt = g_strdup_printf ("Please enter the POP3 password for %s@%s",
						service->url->user, h->h_name);
		pass = camel_session_query_authenticator (camel_service_get_session (service),
							  prompt, TRUE,
							  service, "password",
							  ex);
		g_free (prompt);
		if (!pass)
			return FALSE;
	}

	sin.sin_family = h->h_addrtype;
	sin.sin_port = port;
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
		g_free (pass);
		return FALSE;
	}

	store->ostream = camel_stream_fs_new_with_fd (fd);
	store->istream = camel_stream_buffer_new (store->ostream,
						  CAMEL_STREAM_BUFFER_READ);

	/* Read the greeting, note APOP timestamp, if any. */
	buf = camel_stream_buffer_read_line (CAMEL_STREAM_BUFFER (store->istream));
	if (!buf) {
		g_free (pass);
		return -1;
	}
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

		status = camel_pop3_command(store, NULL, "APOP %s %s",
					    service->url->user, md5asc);
	}

	if (status != CAMEL_POP3_OK ) {
		status = camel_pop3_command(store, NULL, "USER %s",
					    service->url->user);
		if (status == CAMEL_POP3_OK) {
			status = camel_pop3_command(store, NULL,
						    "PASS %s", pass);
		}
	}

	if (status != CAMEL_POP3_OK) {
		camel_exception_set (ex,
				     CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
				     "Unable to authenticate to POP server.");
		return FALSE;
	}

	g_free (pass);
	return TRUE;
}

static gboolean
pop3_disconnect (CamelService *service, CamelException *ex)
{
	CamelPop3Store *store = CAMEL_POP3_STORE (service);

	if (!service->connected)
		return TRUE;

	if (!service_class->disconnect (service, ex))
		return FALSE;

	/* Closing the buffered write stream will close the
	 * unbuffered read stream wrapped inside it as well.
	 */
	camel_stream_close (store->ostream);
	gtk_object_unref (GTK_OBJECT (store->ostream));
	store->ostream = NULL;
	store->istream = NULL;
	return TRUE;
}

static CamelFolder *get_folder (CamelStore *store, const gchar *folder_name, 
				CamelException *ex)
{
	if (!strcasecmp (folder_name, "inbox"))
		return camel_pop3_folder_new (store, ex);
	else {
		camel_exception_setv (ex, CAMEL_EXCEPTION_FOLDER_INVALID,
				      "No such folder `%s'.", folder_name);
		return NULL;
	}
}

/**
 * camel_pop3_command: Send a command to a POP3 server.
 * @store: the POP3 store
 * @ret: a pointer to return the full server response in
 * @fmt: a printf-style format string, followed by arguments
 *
 * This command sends the command specified by @fmt and the following
 * arguments to the connected POP3 store specified by @store. It then
 * reads the server's response and parses out the status code. If
 * the caller passed a non-NULL pointer for @ret, camel_pop3_command
 * will set it to point to an buffer containing the rest of the
 * response from the POP3 server. (If @ret was passed but there was
 * no extended response, @ret will be set to NULL.) The caller must
 * free this buffer when it is done with it.
 *
 * Return value: one of CAMEL_POP3_OK (command executed successfully),
 * CAMEL_POP3_ERR (command encounted an error), or CAMEL_POP3_FAIL
 * (a protocol-level error occurred, and Camel is uncertain of the
 * result of the command.)
 **/
int
camel_pop3_command (CamelPop3Store *store, char **ret, char *fmt, ...)
{
	char *cmdbuf, *respbuf;
	va_list ap;
	int status;

	va_start (ap, fmt);
	cmdbuf = g_strdup_vprintf (fmt, ap);
	va_end (ap);

	/* Send the command */
	camel_stream_write (store->ostream, cmdbuf, strlen (cmdbuf));
	g_free (cmdbuf);
	camel_stream_write (store->ostream, "\r\n", 2);

	/* Read the response */
	respbuf = camel_stream_buffer_read_line (CAMEL_STREAM_BUFFER (store->istream));
	if (!strncmp (respbuf, "+OK", 3))
		status = CAMEL_POP3_OK;
	else if (!strncmp (respbuf, "-ERR", 4))
		status = CAMEL_POP3_ERR;
	else
		status = CAMEL_POP3_FAIL;

	if (ret) {
		if (status != CAMEL_POP3_FAIL) {
			*ret = strchr (respbuf, ' ');
			if (*ret)
				*ret = g_strdup (*ret + 1);
		} else
			*ret = NULL;
	}
	g_free (respbuf);

	return status;
}

/**
 * camel_pop3_command_get_additional_data: get "additional data" from
 * a POP3 command.
 * @store: the POP3 store
 *
 * This command gets the additional data returned by "multi-line" POP
 * commands, such as LIST, RETR, TOP, and UIDL. This command _must_
 * be called after a successful (CAMEL_POP3_OK) call to
 * camel_pop3_command for a command that has a multi-line response.
 * The returned data is un-byte-stuffed, and has lines termined by
 * newlines rather than CR/LF pairs.
 *
 * Return value: the data, which the caller must free.
 **/
char *
camel_pop3_command_get_additional_data (CamelPop3Store *store)
{
	CamelStreamBuffer *stream = CAMEL_STREAM_BUFFER (store->istream);
	GPtrArray *data;
	char *buf;
	int i, status = CAMEL_POP3_OK;

	data = g_ptr_array_new ();
	while (1) {
		buf = camel_stream_buffer_read_line (stream);
		if (!buf) {
			status = CAMEL_POP3_FAIL;
			break;
		}

		if (!strcmp (buf, "."))
			break;
		if (*buf == '.')
			memmove (buf, buf + 1, strlen (buf));
		g_ptr_array_add (data, buf);
	}

	if (status == CAMEL_POP3_OK) {
		/* Append an empty string to the end of the array
		 * so when we g_strjoinv it, we get a "\n" after
		 * the last real line.
		 */
		g_ptr_array_add (data, "");
		g_ptr_array_add (data, NULL);
		buf = g_strjoinv ("\n", (char **)data->pdata);
	} else
		buf = NULL;

	for (i = 0; i < data->len - 2; i++)
		g_free (data->pdata[i]);
	g_ptr_array_free (data, TRUE);

	return buf;
}
