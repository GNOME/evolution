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
#include "camel-tcp-stream-raw.h"
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

static int stream_connect (CamelTcpStream *stream, struct hostent *host, int port);
static int stream_disconnect (CamelTcpStream *stream);

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
	camel_tcp_stream_class->disconnect = stream_disconnect;
}

static void
camel_tcp_stream_ssl_init (gpointer object, gpointer klass)
{
	CamelTcpStreamSSL *stream = CAMEL_TCP_STREAM_SSL (object);
	
	stream->sockfd = NULL;
}

static void
camel_tcp_stream_ssl_finalize (CamelObject *object)
{
	CamelTcpStreamSSL *stream = CAMEL_TCP_STREAM_SSL (object);
	
	if (stream->sockfd != NULL)
		PR_Close (stream->sockfd);
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
 *
 * Return value: a tcp stream
 **/
CamelStream *
camel_tcp_stream_ssl_new ()
{
	CamelTcpStreamSSL *stream;
	
	stream = CAMEL_TCP_STREAM_SSL (camel_object_new (camel_tcp_stream_ssl_get_type ()));
	
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
	g_warning ("CamelTcpStreamSSL::close: Better to call ::disconnect.\n");
	return PR_Close (((CamelTcpStreamSSL *)stream)->sockfd);
}


static int
stream_connect (CamelTcpStream *stream, struct hostent *host, int port)
{
	CamelTcpStreamSSL *ssl = CAMEL_TCP_STREAM_SSL (stream);
	PRIntervalTime timeout;
	PRNetAddr netaddr;
	PRFileDesc *fd;
	
	g_return_val_if_fail (host != NULL, -1);
	
	memset ((void *) &netaddr, 0, sizeof (PRNetAddr));
	memcpy (&netaddr.inet.ip, host->h_addr, sizeof (netaddr.inet.ip));
	
	if (PR_InitializeNetAddr (PR_IpAddrNull, port, &netaddr) == PR_FAILUE)
		return -1;
	
	fd = PR_OpenTCPSocket (host->h_addrtype);
	
	if (fd == NULL || PR_Connect (fd, &netaddr, timeout) == PR_FAILURE) {
		if (fd != NULL)
			PR_Close (fd);
		
		return -1;
	}
	
	ssl->sockfd = fd;
	
	return 0;
}

static int
stream_disconnect (CamelTcpStream *stream)
{
	PRStatus status;
	
	status = PR_Shutdown (((CamelTcpStreamSSL *)stream)->sockfd, PR_SHUTDOWN_BOTH);
	
	if (status == PR_FAILURE)
		return -1;
	
	return PR_Close (((CamelTcpStreamSSL *)stream)->sockfd);
}
