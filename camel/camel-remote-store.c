/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-remote-store.c : class for an remote store */

/*
 *  Authors: Peter Williams <peterw@helixcode.com>
 *           based on camel-imap-provider.c
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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <e-util/e-util.h>

#include "camel-remote-store.h"
#include "camel-folder.h"
#include "camel-exception.h"
#include "camel-session.h"
#include "camel-stream.h"
#include "camel-stream-buffer.h"
#include "camel-stream-fs.h"
#include "camel-url.h"
#include "string-utils.h"

#define d(x) x

#define CSRVC(obj) (CAMEL_SERVICE_CLASS      (CAMEL_OBJECT_GET_CLASS (obj)))
#define CSTRC(obj) (CAMEL_STORE_CLASS        (CAMEL_OBJECT_GET_CLASS (obj)))
#define CRSC(obj)  (CAMEL_REMOTE_STORE_CLASS (CAMEL_OBJECT_GET_CLASS (obj)))

static CamelStoreClass *store_class = NULL;

static gboolean remote_connect         (CamelService *service, CamelException *ex);
static gboolean remote_disconnect      (CamelService *service, CamelException *ex);
static GList   *remote_query_auth_types_generic   (CamelService *service, CamelException *ex);
static GList   *remote_query_auth_types_connected (CamelService *service, CamelException *ex);
static void     remote_free_auth_types (CamelService *service, GList *authtypes);
static char    *remote_get_name        (CamelService *service, gboolean brief);
static char    *remote_get_folder_name (CamelStore *store, 
					const char *folder_name, 
					CamelException *ex);
static gint     remote_send_string     (CamelRemoteStore *store, CamelException *ex, 
					char *fmt, va_list ap);
static gint     remote_send_stream     (CamelRemoteStore *store, CamelStream *stream,
					CamelException *ex);
static gint     remote_recv_line       (CamelRemoteStore *store, char **dest, 
					CamelException *ex);

static void
camel_remote_store_class_init (CamelRemoteStoreClass *camel_remote_store_class)
{
	/* virtual method overload */
	CamelServiceClass *camel_service_class =
		CAMEL_SERVICE_CLASS (camel_remote_store_class);
	CamelStoreClass *camel_store_class =
		CAMEL_STORE_CLASS (camel_remote_store_class);
	
	store_class = CAMEL_STORE_CLASS (camel_type_get_global_classfuncs (camel_store_get_type ()));
	
	/* virtual method overload */
	camel_service_class->connect = remote_connect;
	camel_service_class->disconnect = remote_disconnect;
	camel_service_class->query_auth_types_generic = remote_query_auth_types_generic;
	camel_service_class->query_auth_types_connected = remote_query_auth_types_connected;
	camel_service_class->free_auth_types = remote_free_auth_types;
	camel_service_class->get_name = remote_get_name;
	
	camel_store_class->get_folder_name = remote_get_folder_name;
	
	camel_remote_store_class->send_string = remote_send_string;
	camel_remote_store_class->send_stream = remote_send_stream;
	camel_remote_store_class->recv_line = remote_recv_line;
	camel_remote_store_class->keepalive = NULL;
}

static void
camel_remote_store_init (CamelObject *object)
{
	CamelService *service = CAMEL_SERVICE (object);
	CamelStore *store = CAMEL_STORE (object);
	CamelRemoteStore *remote_store = CAMEL_REMOTE_STORE (object);
	
	service->url_flags |= CAMEL_SERVICE_URL_NEED_HOST;
	
	store->folders = g_hash_table_new (g_str_hash, g_str_equal);
	
	remote_store->istream = NULL;
	remote_store->ostream = NULL;
	remote_store->timeout_id = 0;
}

/*
 *static void
 *camel_remote_store_finalize (CamelObject *object)
 *{
 *	CamelRemoteStore *remote_store = CAMEL_REMOTE_STORE (object);
 *
 *	g_free (remote_store->nice_name);
 *}
 */

CamelType
camel_remote_store_get_type (void)
{
	static CamelType camel_remote_store_type = CAMEL_INVALID_TYPE;
	
	if (camel_remote_store_type == CAMEL_INVALID_TYPE) {
		camel_remote_store_type =
			camel_type_register (CAMEL_STORE_TYPE, "CamelRemoteStore",
					     sizeof (CamelRemoteStore),
					     sizeof (CamelRemoteStoreClass),
					     (CamelObjectClassInitFunc) camel_remote_store_class_init,
					     NULL,
					     (CamelObjectInitFunc) camel_remote_store_init,
					     (CamelObjectFinalizeFunc) NULL);
	}
	
	return camel_remote_store_type;
}

/* Auth stuff */

/*
static CamelServiceAuthType password_authtype = {
	"SSH Tunneling",
	
	"This option will connect to the REMOTE server using a "
	"plaintext password.",
	
	"",
	TRUE
};
*/

static GList *
remote_query_auth_types_connected (CamelService *service, CamelException *ex)
{
	g_warning ("remote::query_auth_types_connected: not implemented. Defaulting.");
	return CSRVC (service)->query_auth_types_generic (service, ex);
}

static GList *
remote_query_auth_types_generic (CamelService *service, CamelException *ex)
{
	g_warning ("remote::query_auth_types_generic: not implemented. Defaulting.");
	return NULL;
}

static void
remote_free_auth_types (CamelService *service, GList *authtypes)
{
	g_list_free (authtypes);
}

static char *
remote_get_name (CamelService *service, gboolean brief)
{
	if (brief)
		return g_strdup_printf ("%s server %s", 
					service->provider->name,
					service->url->host);
	else {
		return g_strdup_printf ("%s service for %s on %s",
					service->provider->name,
					service->url->user,
					service->url->host);
	}
}

static void
refresh_folder_info (gpointer key, gpointer value, gpointer data)
{
	CamelFolder *folder = CAMEL_FOLDER (value);
	
	camel_folder_refresh_info (folder, (CamelException *) data);
}

static gboolean
timeout_cb (gpointer data)
{
	CRSC (data)->keepalive (CAMEL_REMOTE_STORE (data));
	return TRUE;
}

static gboolean
remote_connect (CamelService *service, CamelException *ex)
{
	CamelRemoteStore *store = CAMEL_REMOTE_STORE (service);
	struct hostent *h;
	struct sockaddr_in sin;
	gint fd;
	gint port;
	
	h = camel_service_gethost (service, ex);
	if (!h)
		return FALSE;
	
	/* connect to the server */
	sin.sin_family = h->h_addrtype;
	
	if (service->url->port)
		port = service->url->port;
	else {
		CamelProvider *prov = camel_service_get_provider (service);
		
		port = prov->default_ports[CAMEL_PROVIDER_STORE];
		g_assert (port); /* a remote service MUST define a valid default port */
	}
	
	sin.sin_port = htons (port);
	
	memcpy (&sin.sin_addr, h->h_addr, sizeof (sin.sin_addr));
	
	fd = socket (h->h_addrtype, SOCK_STREAM, 0);
	if (fd == -1 || connect (fd, (struct sockaddr *)&sin, sizeof (sin)) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not connect to %s (port %d): %s",
				      service->url->host ? service->url->host : "(unknown host)", 
				      port, g_strerror (errno));
		if (fd > -1)
			close (fd);
		
		return FALSE;
	}
	
	/* parent class connect initialization */
	if (CAMEL_SERVICE_CLASS (store_class)->connect (service, ex) == FALSE)
		return FALSE;
	
	store->ostream = camel_stream_fs_new_with_fd (fd);
	store->istream = camel_stream_buffer_new (store->ostream, CAMEL_STREAM_BUFFER_READ);
	
	/* Okay, good enough for us */
	CAMEL_SERVICE (store)->connected = TRUE;
	
	if (camel_exception_is_set (ex)) {
		CamelException dex;
		
		camel_exception_init (&dex);
		camel_service_disconnect (CAMEL_SERVICE (store), &dex);
		camel_exception_clear (&dex);
		return FALSE;
	}

	/* Add a timeout so that we can hopefully prevent getting disconnected */
	/* (Only if the implementation supports it) */
	if (CRSC (store)->keepalive) {
		CamelSession *session = camel_service_get_session (CAMEL_SERVICE (store));
		
		store->timeout_id = camel_session_register_timeout (session, 10 * 60 * 1000, 
								    timeout_cb, 
								    store);
	}
	
	/* Let's make sure that any of our folders are brought up to speed */
	g_hash_table_foreach (CAMEL_STORE (store)->folders, refresh_folder_info, ex);
	
	return TRUE;
}

static gboolean
remote_disconnect (CamelService *service, CamelException *ex)
{
	CamelRemoteStore *store = CAMEL_REMOTE_STORE (service);
	
	if (store->timeout_id) {
		camel_session_remove_timeout (camel_service_get_session (CAMEL_SERVICE (store)),
					      store->timeout_id);
		store->timeout_id = 0;
	}
	
	if (!CAMEL_SERVICE_CLASS (store_class)->disconnect (service, ex))
		return FALSE;
	
	if (store->istream) {
		camel_object_unref (CAMEL_OBJECT (store->istream));
		store->istream = NULL;
	}
	
	if (store->ostream) {
		camel_object_unref (CAMEL_OBJECT (store->ostream));
		store->ostream = NULL;
	}
	
	return TRUE;
}

static gchar *
remote_get_folder_name (CamelStore *store, const char *folder_name, CamelException *ex)
{
	return g_strdup (folder_name);
}

static gint
remote_send_string (CamelRemoteStore *store, CamelException *ex, char *fmt, va_list ap)
{
	gchar *cmdbuf;
	
	/* Check for connectedness. Failed (or cancelled) operations will
	 * close the connection. */
	
	if (store->ostream == NULL) {
		d(g_message ("remote: (send) disconnected, reconnecting."));
		
		if (!camel_service_connect (CAMEL_SERVICE (store), ex))
			return -1;
	}
	
	/* create the command */
	cmdbuf = g_strdup_vprintf (fmt, ap);
	
	d(fprintf (stderr, "sending : %s", cmdbuf));
	
	if (camel_stream_printf (store->ostream, "%s", cmdbuf) == -1) {
		CamelException dex;
		
		g_free (cmdbuf);
		camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				     g_strerror (errno));
		
		camel_exception_init (&dex);
		camel_service_disconnect (CAMEL_SERVICE (store), &dex);
		camel_exception_clear (&dex);
		return -1;
	}
	g_free (cmdbuf);
	
	return 0;
}

/**
 * camel_remote_store_send_string: Writes a string to the server
 * @store: a CamelRemoteStore
 * @ex: a CamelException
 * @fmt: the printf-style format to use for creating the string to send
 * @...: the arguments to the printf string @fmt
 * Return value: 0 on success, nonzero on error
 *
 * Formats the string and sends it to the server.
 **/

gint 
camel_remote_store_send_string (CamelRemoteStore *store, CamelException *ex,
				char *fmt, ...)
{
	va_list ap;
	gint ret;
	
	g_return_val_if_fail (CAMEL_IS_REMOTE_STORE (store), -1);
	g_return_val_if_fail (fmt, -1);
	
	va_start (ap, fmt);
	ret = CRSC (store)->send_string (store, ex, fmt, ap);
	va_end (ap);
	
	return ret;
}

static gint
remote_send_stream (CamelRemoteStore *store, CamelStream *stream, CamelException *ex)
{
	/* Check for connectedness. Failed (or cancelled) operations will
	 * close the connection. */
	
	if (store->ostream == NULL) {
		d(g_message ("remote: (sendstream) disconnected, reconnecting."));
		
		if (!camel_service_connect (CAMEL_SERVICE (store), ex))
			return -1;
	}
	
	d(fprintf (stderr, "(sending stream)\n"));
	
	if (camel_stream_write_to_stream (stream, store->ostream) < 0) {
		CamelException dex;
		
		camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				     g_strerror (errno));
		
		camel_exception_init (&dex);
		camel_service_disconnect (CAMEL_SERVICE (store), &dex);
		camel_exception_clear (&dex);
		return -1;
	}
	
	return 0;
}

/**
 * camel_remote_store_send_stream: Writes a CamelStream to the server
 * @store: a CamelRemoteStore
 * @stream: the stream to write
 * @ex: a CamelException
 * Return value: 0 on success, nonzero on error
 *
 * Sends the stream to the server.
 **/

gint 
camel_remote_store_send_stream (CamelRemoteStore *store, CamelStream *stream, CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_REMOTE_STORE (store), -1);
	g_return_val_if_fail (CAMEL_IS_STREAM (stream), -1);
	
	return CRSC (store)->send_stream (store, stream, ex);
}

static gint
remote_recv_line (CamelRemoteStore *store, char **dest, CamelException *ex)
{
	CamelStreamBuffer *stream = CAMEL_STREAM_BUFFER (store->istream);
	
	*dest = NULL;
	
	/* Check for connectedness. Failed (or cancelled) operations will
	 * close the connection. We can't expect a read to have any
	 * meaning if we reconnect, so always set an exception.
	 */
	
	if (store->istream == NULL) {
		g_message ("remote: (recv) disconnected, reconnecting.");
		
		camel_service_connect (CAMEL_SERVICE (store), ex);
		
		if (!camel_exception_is_set (ex))
			camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_NOT_CONNECTED,
					     g_strerror (errno));
		
		return -1;
	}
	
	*dest = camel_stream_buffer_read_line (stream);
	
	if (!*dest) {
		CamelException dex;
		
		camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				     g_strerror (errno));
		
		camel_exception_init (&dex);
		camel_service_disconnect (CAMEL_SERVICE (store), &dex);
		camel_exception_clear (&dex);
		return -1;
	}
	
	d(fprintf (stderr, "received: %s\n", *dest));
	
	return 0;
}

/**
 * camel_remote_store_recv_line: Reads a line from the server
 * @store: a CamelRemoteStore
 * @dest: a pointer that will be set to the location of a buffer
 *        holding the server's response
 * @ex: a CamelException
 * Return value: 0 on success, -1 on error
 *
 * Reads a line from the server (terminated by \n or \r\n).
 **/

gint 
camel_remote_store_recv_line (CamelRemoteStore *store, char **dest,
			      CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_REMOTE_STORE (store), -1);
	g_return_val_if_fail (dest, -1);
	
	return CRSC (store)->recv_line (store, dest, ex);
}
