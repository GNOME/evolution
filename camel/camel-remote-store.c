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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "camel-remote-store.h"
#include "camel-folder.h"
#include "camel-exception.h"
#include "camel-session.h"
#include "camel-stream.h"
#include "camel-stream-buffer.h"
#include "camel-tcp-stream.h"
#include "camel-tcp-stream-raw.h"
#if HAVE_NSS
#include "camel-tcp-stream-ssl.h"
#endif

#include "camel-url.h"
#include "string-utils.h"

#include "camel-private.h"
#include "camel-operation.h"

#define d(x) x
#if d(!)0
extern gboolean camel_verbose_debug;
#endif

#define CSRVC(obj) (CAMEL_SERVICE_CLASS      (CAMEL_OBJECT_GET_CLASS (obj)))
#define CSTRC(obj) (CAMEL_STORE_CLASS        (CAMEL_OBJECT_GET_CLASS (obj)))
#define CRSC(obj)  (CAMEL_REMOTE_STORE_CLASS (CAMEL_OBJECT_GET_CLASS (obj)))

static CamelStoreClass *store_class = NULL;

static void     remote_construct       (CamelService *service, CamelSession *session,
					CamelProvider *provider, CamelURL *url,
					CamelException *ex);
static gboolean remote_connect         (CamelService *service, CamelException *ex);
static gboolean remote_disconnect      (CamelService *service, gboolean clean, CamelException *ex);
static GList   *remote_query_auth_types(CamelService *service, CamelException *ex);
static char    *remote_get_name        (CamelService *service, gboolean brief);
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
	
	store_class = CAMEL_STORE_CLASS (camel_type_get_global_classfuncs (camel_store_get_type ()));
	
	/* virtual method overload */
	camel_service_class->construct = remote_construct;
	camel_service_class->connect = remote_connect;
	camel_service_class->disconnect = remote_disconnect;
	camel_service_class->query_auth_types = remote_query_auth_types;
	camel_service_class->get_name = remote_get_name;
	
	camel_remote_store_class->send_string = remote_send_string;
	camel_remote_store_class->send_stream = remote_send_stream;
	camel_remote_store_class->recv_line = remote_recv_line;
	camel_remote_store_class->keepalive = NULL;
}

static void
camel_remote_store_init (CamelObject *object)
{
	CamelRemoteStore *remote_store = CAMEL_REMOTE_STORE (object);
	
	remote_store->istream = NULL;
	remote_store->ostream = NULL;
	remote_store->timeout_id = 0;

	remote_store->priv = g_malloc0(sizeof(*remote_store->priv));
#ifdef ENABLE_THREADS
	remote_store->priv->stream_lock = e_mutex_new(E_MUTEX_REC);
#endif
}

static void
camel_remote_store_finalise(CamelObject *object)
{
	CamelRemoteStore *remote_store = CAMEL_REMOTE_STORE (object);

#ifdef ENABLE_THREADS
	e_mutex_destroy(remote_store->priv->stream_lock);
#endif
	g_free(remote_store->priv);
}


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
					     (CamelObjectFinalizeFunc) camel_remote_store_finalise);
	}
	
	return camel_remote_store_type;
}

static void
remote_construct (CamelService *service, CamelSession *session,
		  CamelProvider *provider, CamelURL *url,
		  CamelException *ex)
{
	CamelRemoteStore *remote_store = CAMEL_REMOTE_STORE (service);

	CAMEL_SERVICE_CLASS (store_class)->construct (service, session, provider, url, ex);

	if (camel_url_get_param (url, "use_ssl"))
		remote_store->use_ssl = TRUE;
}


/* Auth stuff... for now, nothing, but might eventually add SSH tunneling
 */

static GList *
remote_query_auth_types (CamelService *service, CamelException *ex)
{
	return NULL;
}

GList *
camel_remote_store_authtype_list (void)
{
	return NULL;
}


static char *
remote_get_name (CamelService *service, gboolean brief)
{
	if (brief)
		return g_strdup_printf (_("%s server %s"),
					service->provider->name,
					service->url->host);
	else {
		return g_strdup_printf (_("%s service for %s on %s"),
					service->provider->name,
					service->url->user,
					service->url->host);
	}
}

static gboolean
timeout_cb (gpointer data)
{
	CamelRemoteStore *store = CAMEL_REMOTE_STORE(data);

	CRSC (data)->keepalive(store);

	return TRUE;
}

static gboolean
remote_connect (CamelService *service, CamelException *ex)
{
	CamelRemoteStore *store = CAMEL_REMOTE_STORE (service);
	CamelStream *tcp_stream;
	struct hostent *h;
	gint ret, port;
	
	h = camel_service_gethost (service, ex);
	if (!h)
		return FALSE;
	
	if (service->url->port)
		port = service->url->port;
	else if (store->use_ssl)
		port = store->default_ssl_port;
	else
		port = store->default_port;	
	
#ifdef HAVE_NSS
	if (store->use_ssl)
		tcp_stream = camel_tcp_stream_ssl_new (service, service->url->host);
	else
		tcp_stream = camel_tcp_stream_raw_new ();
#else
	tcp_stream = camel_tcp_stream_raw_new ();
#endif /* HAVE_NSS */
	
	ret = camel_tcp_stream_connect (CAMEL_TCP_STREAM (tcp_stream), h, port);
	camel_free_host(h);
	if (ret == -1) {
		if (errno == EINTR)
			camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL,
					     _("Connection cancelled"));
		else
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					      _("Could not connect to %s (port %d): %s"),
					      service->url->host ? service->url->host : _("(unknown host)"),
					      port, g_strerror (errno));
		return FALSE;
	}
	
	/* parent class connect initialization */
	if (CAMEL_SERVICE_CLASS (store_class)->connect (service, ex) == FALSE)
		return FALSE;
	
	store->ostream = tcp_stream;
	store->istream = camel_stream_buffer_new (tcp_stream, CAMEL_STREAM_BUFFER_READ);
	
	/* Okay, good enough for us */
	CAMEL_SERVICE (store)->connected = TRUE;
	
	/* Add a timeout so that we can hopefully prevent getting disconnected */
	/* (Only if the implementation supports it) */
	if (CRSC (store)->keepalive) {
		CamelSession *session = camel_service_get_session (CAMEL_SERVICE (store));
		
		store->timeout_id = camel_session_register_timeout (session, 10 * 60 * 1000, 
								    timeout_cb, 
								    store);
	}
	
	return TRUE;
}

static gboolean
remote_disconnect (CamelService *service, gboolean clean, CamelException *ex)
{
	CamelRemoteStore *store = CAMEL_REMOTE_STORE (service);
	
	if (store->timeout_id) {
		camel_session_remove_timeout (camel_service_get_session (CAMEL_SERVICE (store)),
					      store->timeout_id);
		store->timeout_id = 0;
	}
	
	if (!CAMEL_SERVICE_CLASS (store_class)->disconnect (service, clean, ex))
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

static gint
remote_send_string (CamelRemoteStore *store, CamelException *ex, char *fmt, va_list ap)
{
	gchar *cmdbuf;
	
	/* Check for connectedness. Failed (or cancelled) operations will
	 * close the connection. */
	if (!camel_remote_store_connected (store, ex))
		return -1;
	
	/* create the command */
	cmdbuf = g_strdup_vprintf (fmt, ap);

#if d(!)0
	if (camel_verbose_debug) {
		if (strncmp (cmdbuf, "PASS ", 5) == 0)
			fprintf (stderr, "sending : PASS xxxx\n");
		else if (strstr (cmdbuf, "LOGIN \""))
			fprintf (stderr, "sending : ---- LOGIN \"xxxx\" \"xxxx\"\n");
		else
			fprintf (stderr, "sending : %s", cmdbuf);
	}
#endif
	
	if (camel_stream_printf (store->ostream, "%s", cmdbuf) == -1) {
		if (errno == EINTR)
			camel_exception_set(ex, CAMEL_EXCEPTION_USER_CANCEL, _("Operation cancelled"));
		else
			camel_exception_set(ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE, strerror(errno));
		g_free (cmdbuf);
		
		camel_service_disconnect (CAMEL_SERVICE (store), FALSE, NULL);
		return -1;
	}
	g_free (cmdbuf);
	
	return 0;
}

/* FIXME: All of these functions need an api overhaul, they're not like
   any other functions, anywhere in the world ... */

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
	CAMEL_REMOTE_STORE_LOCK(store, stream_lock);
	ret = CRSC (store)->send_string (store, ex, fmt, ap);
	CAMEL_REMOTE_STORE_UNLOCK(store, stream_lock);
	va_end (ap);
	
	return ret;
}

static gint
remote_send_stream (CamelRemoteStore *store, CamelStream *stream, CamelException *ex)
{
	int ret;

	/* Check for connectedness. Failed (or cancelled) operations will
	 * close the connection. */

	if (!camel_remote_store_connected (store, ex))
		return -1;
	
	d(fprintf (stderr, "(sending stream)\n"));
	
	ret = camel_stream_write_to_stream (stream, store->ostream);
	if (ret == -1) {
		if (errno == EINTR)
			camel_exception_set(ex, CAMEL_EXCEPTION_USER_CANCEL, _("Operation cancelled"));
		else
			camel_exception_set(ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE, strerror(errno));
		
		camel_service_disconnect (CAMEL_SERVICE (store), FALSE, NULL);
	}

	return ret;
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
	int ret;

	g_return_val_if_fail (CAMEL_IS_REMOTE_STORE (store), -1);
	g_return_val_if_fail (CAMEL_IS_STREAM (stream), -1);

	CAMEL_REMOTE_STORE_LOCK(store, stream_lock);
	
	ret = CRSC (store)->send_stream (store, stream, ex);

	CAMEL_REMOTE_STORE_UNLOCK(store, stream_lock);

	return ret;
}

static int
remote_recv_line (CamelRemoteStore *store, char **dest, CamelException *ex)
{
	CamelStreamBuffer *stream;
	GByteArray *bytes;
	gchar buf[1024], *ret;
	gboolean exception = FALSE;
	gint nread;
	
	*dest = NULL;
	
	/* Check for connectedness. Failed (or cancelled) operations will
	 * close the connection. We can't expect a read to have any
	 * meaning if we reconnect, so always set an exception.
	 */
	
	if (!camel_remote_store_connected (store, ex)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_NOT_CONNECTED,
				     g_strerror (errno));
		return -1;
	}
	stream = CAMEL_STREAM_BUFFER (store->istream);

	bytes = g_byte_array_new ();
	
	do {
		nread = camel_stream_buffer_gets (stream, buf, sizeof (buf));
		if (nread > 0)
			g_byte_array_append (bytes, buf, nread);
	} while (nread == sizeof (buf) - 1);

	if (nread == -1) {
		exception = TRUE;
		if (errno == EINTR)
			camel_exception_set(ex, CAMEL_EXCEPTION_USER_CANCEL, _("Operation cancelled"));
		else
			camel_exception_set(ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE, strerror(errno));
	} else if (bytes->len == 0) {
		exception = TRUE;
		camel_exception_set(ex, CAMEL_EXCEPTION_SERVICE_NOT_CONNECTED,
				    _("Server unexpectedly disconnected"));
	}
	if (camel_exception_is_set (ex) || exception) {
		g_byte_array_free(bytes, TRUE);
		camel_service_disconnect (CAMEL_SERVICE (store), FALSE, NULL);
		return -1;
	}

	g_byte_array_append (bytes, "", 1);
	ret = bytes->data;
	nread = bytes->len - 1;
	g_byte_array_free (bytes, FALSE);
	
	/* strip off the CRLF sequence */
	while (nread > 0 && ret[nread] != '\r')
		ret[nread--] = '\0';
	ret[nread] = '\0';
	
	*dest = ret;
	
#if d(!)0
	if (camel_verbose_debug)
		fprintf (stderr, "received: %s\n", *dest);
#endif
	
	return nread;
}

/**
 * camel_remote_store_recv_line: Reads a line from the server
 * @store: a CamelRemoteStore
 * @dest: a pointer that will be set to the location of a buffer
 *        holding the server's response
 * @ex: a CamelException
 * Return value: -1 on error, otherwise the length read.
 *
 * Reads a line from the server (terminated by \n or \r\n).
 **/

gint 
camel_remote_store_recv_line (CamelRemoteStore *store, char **dest,
			      CamelException *ex)
{
	int ret;

	g_return_val_if_fail (CAMEL_IS_REMOTE_STORE (store), -1);
	g_return_val_if_fail (dest, -1);

	CAMEL_REMOTE_STORE_LOCK(store, stream_lock);
	
	ret = CRSC (store)->recv_line (store, dest, ex);

	CAMEL_REMOTE_STORE_UNLOCK(store, stream_lock);

	return ret;
}

static void
refresh_folder_info (gpointer key, gpointer value, gpointer data)
{
	CamelFolder *folder = CAMEL_FOLDER (value);
	
	camel_folder_refresh_info (folder, (CamelException *) data);
}

/**
 * camel_remote_store_refresh_folders: Refresh the folders that I
 * contain
 * @store: a CamelRemoteStore
 * @ex: a CamelException
 *
 * Refreshes the folders listed in the folders hashtable.
 **/
void
camel_remote_store_refresh_folders (CamelRemoteStore *store, CamelException *ex)
{
	CAMEL_STORE_LOCK(store, cache_lock);

	g_hash_table_foreach (CAMEL_STORE (store)->folders, refresh_folder_info, ex);

	CAMEL_STORE_UNLOCK(store, cache_lock);
}	

/**
 * camel_remote_store_connected:
 * @store: a CamelRemoteStore
 * @ex: a CamelException
 *
 * Ensure that the remote store is connected.
 *
 * Return value: Whether or not it is connected
 **/
gboolean
camel_remote_store_connected (CamelRemoteStore *store, CamelException *ex)
{
	if (store->istream == NULL)
		return camel_service_connect (CAMEL_SERVICE (store), ex);
	return TRUE;
}
