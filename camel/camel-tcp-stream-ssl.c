/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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
#include "camel-tcp-stream-ssl.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

static CamelTcpStreamClass *parent_class = NULL;

/* Returns the class for a CamelTcpStreamSSL */
#define CTSS_CLASS(so) CAMEL_TCP_STREAM_SSL_CLASS (CAMEL_OBJECT_GET_CLASS (so))

static ssize_t stream_read (CamelStream *stream, char *buffer, size_t n);
static ssize_t stream_write (CamelStream *stream, const char *buffer, size_t n);
static int stream_flush  (CamelStream *stream);
static int stream_close  (CamelStream *stream);

static int stream_connect    (CamelTcpStream *stream, struct hostent *host, int port);
static int stream_getsockopt (CamelTcpStream *stream, CamelSockOptData *data);
static int stream_setsockopt (CamelTcpStream *stream, const CamelSockOptData *data);

/* callbacks */

static SECStatus ssl_bad_cert  (void *data, PRFileDesc *fd);
static SECStatus ssl_auth_cert (void *data, PRFileDesc *fd, PRBool checksig, PRBool is_server);


static void
camel_tcp_stream_ssl_class_init (CamelTcpStreamSSLClass *camel_tcp_stream_ssl_class)
{
	CamelTcpStreamClass *camel_tcp_stream_class =
		CAMEL_TCP_STREAM_CLASS (camel_tcp_stream_ssl_class);
	CamelStreamClass *camel_stream_class =
		CAMEL_STREAM_CLASS (camel_tcp_stream_ssl_class);
	
	parent_class = CAMEL_STREAM_CLASS (camel_type_get_global_classfuncs (camel_tcp_stream_get_type ()));
	
	/* virtual method overload */
	camel_stream_class->read = stream_read;
	camel_stream_class->write = stream_write;
	camel_stream_class->flush = stream_flush;
	camel_stream_class->close = stream_close;
	
	camel_tcp_stream_class->connect = stream_connect;
	camel_tcp_stream_class->getsockopt = stream_getsockopt;
	camel_tcp_stream_class->setsockopt = stream_setsockopt;
}

static void
camel_tcp_stream_ssl_init (gpointer object, gpointer klass)
{
	CamelTcpStreamSSL *stream = CAMEL_TCP_STREAM_SSL (object);
	
	stream->sockfd = NULL;
	stream->session = NULL;
	stream->expected_host = NULL;
}

static void
camel_tcp_stream_ssl_finalize (CamelObject *object)
{
	CamelTcpStreamSSL *stream = CAMEL_TCP_STREAM_SSL (object);
	
	if (stream->sockfd != NULL)
		PR_Close (stream->sockfd);
	
	camel_object_unref (CAMEL_OBJECT (stream->session));
	g_free (stream->expected_host);
}


CamelType
camel_tcp_stream_ssl_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_stream_get_type (),
					    "CamelTcpStreamSSL",
					    sizeof (CamelTcpStreamSSL),
					    sizeof (CamelTcpStreamSSLClass),
					    (CamelObjectClassInitFunc) camel_tcp_stream_ssl_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_tcp_stream_ssl_init,
					    (CamelObjectFinalizeFunc) camel_tcp_stream_ssl_finalize);
	}
	
	return type;
}


/**
 * camel_tcp_stream_ssl_new:
 * @session: camel session
 * @expected_host: host that the stream is expected to connect with.
 *
 * Since the SSL certificate authenticator may need to prompt the
 * user, a CamelSession is needed. #expected_host is needed as a
 * protection against an MITM attack.
 *
 * Return value: a tcp stream
 **/
CamelStream *
camel_tcp_stream_ssl_new (CamelSession *session, const char *expected_host)
{
	CamelTcpStreamSSL *stream;
	
	stream = CAMEL_TCP_STREAM_SSL (camel_object_new (camel_tcp_stream_ssl_get_type ()));
	
	camel_object_ref (CAMEL_OBJECT (session));
	stream->session = session;
	stream->expected_host = g_strdup (expected_host);
	
	return CAMEL_STREAM (stream);
}

static ssize_t
stream_read (CamelStream *stream, char *buffer, size_t n)
{
	CamelTcpStreamSSL *tcp_stream_ssl = CAMEL_TCP_STREAM_SSL (stream);
	ssize_t nread;
	
	do {
		nread = PR_Read (tcp_stream_ssl->sockfd, buffer, n);
	} while (nread == -1 && errno == EINTR);
	
	return nread;
}

static ssize_t
stream_write (CamelStream *stream, const char *buffer, size_t n)
{
	CamelTcpStreamSSL *tcp_stream_ssl = CAMEL_TCP_STREAM_SSL (stream);
	ssize_t v, written = 0;
	
	do {
		v = PR_Write (tcp_stream_ssl->sockfd, buffer, n);
		if (v > 0)
			written += v;
	} while (v == -1 && errno == EINTR);
	
	if (v == -1)
		return -1;
	else
		return written;
}

static int
stream_flush (CamelStream *stream)
{
	return PR_Fsync (((CamelTcpStreamSSL *)stream)->sockfd);
}

static int
stream_close (CamelStream *stream)
{
	if (PR_Close (((CamelTcpStreamSSL *)stream)->sockfd) == PR_Failure)
		return -1;
	
	((CamelTcpStreamSSL *)stream)->sockfd = NULL;
	
	return 0;
}


static SECStatus
ssl_auth_cert (void *data, PRFileDesc *fd, PRBool checksig, PRBool is_server)
{
	return SSL_AuthCertificate (NULL, fd, TRUE, FALSE);
}

static SECStatus
ssl_bad_cert (void *data, PRFileDesc *fd)
{
	CamelSession *session;
	char *string;
	PRInt32 len;
	
	g_return_val_if_fail (data != NULL, SECFailure);
	g_return_val_if_fail (CAMEL_IS_SESSION (data), SECFailure);
	
	/* FIXME: International issues here?? */
	len = PR_GetErrorTextLen (PR_GetError ());
	string = g_malloc0 (len);
	PR_GetErrorText (string);
	
	session = CAMEL_SESSION (data);
	if (camel_session_query_cert_authenticator (session, string))
		return SECSuccess;
	
	return SECFailure;
}

static int
stream_connect (CamelTcpStream *stream, struct hostent *host, int port)
{
	CamelTcpStreamSSL *ssl = CAMEL_TCP_STREAM_SSL (stream);
	PRIntervalTime timeout;
	PRNetAddr netaddr;
	PRFileDesc *fd, *ssl_fd;
	
	g_return_val_if_fail (host != NULL, -1);
	
	memset ((void *) &netaddr, 0, sizeof (PRNetAddr));
	memcpy (&netaddr.inet.ip, host->h_addr, sizeof (netaddr.inet.ip));
	
	if (PR_InitializeNetAddr (PR_IpAddrNull, port, &netaddr) == PR_FAILUE)
		return -1;
	
	fd = PR_OpenTCPSocket (host->h_addrtype);
	ssl_fd = SSL_ImportFD (NULL, fd);
	
	SSL_SetUrl (ssl_fd, ssl->expected_host);
	
	if (ssl_fd == NULL || PR_Connect (ssl_fd, &netaddr, timeout) == PR_FAILURE) {
		if (ssl_fd != NULL)
			PR_Close (ssl_fd);
		
		return -1;
	}
	
	SSL_AuthCertificateHook (ssl_fd, ssl_auth_cert, NULL);
	SSL_BadCertHook (ssl_fd, ssl_bad_cert, ssl->session);
	
	ssl->sockfd = ssl_fd;
	
	return 0;
}


static int
stream_getsockopt (CamelTcpStream *stream, CamelSockOptData *data)
{
	PRSocketOptionData sodata;
	
	memset ((void *) &sodata, 0, sizeof (sodata));
	memcpy ((void *) &sodata, (void *) data, sizeof (CamelSockOptData));
	
	if (PR_GetSocketOption (((CamelTcpStreamSSL *)stream)->sockfd, &sodata) == PR_FAILURE)
		return -1;
	
	memcpy ((void *) data, (void *) &sodata, sizeof (CamelSockOptData));
	
	return 0;
}

static int
stream_setsockopt (CamelTcpStream *stream, const CamelSockOptData *data)
{
	PRSocketOptionData sodata;
	
	memset ((void *) &sodata, 0, sizeof (sodata));
	memcpy ((void *) &sodata, (void *) data, sizeof (CamelSockOptData));
	
	if (PR_SetSocketOption (((CamelTcpStreamRaw *)stream)->sockfd, &sodata) == PR_FAILURE)
		return -1;
	
	return 0;
}
