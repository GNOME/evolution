/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "camel-tcp-stream.h"

#define w(x)

static CamelStreamClass *parent_class = NULL;

/* Returns the class for a CamelTcpStream */
#define CTS_CLASS(so) CAMEL_TCP_STREAM_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static int tcp_connect    (CamelTcpStream *stream, struct addrinfo *host);
static int tcp_getsockopt (CamelTcpStream *stream, CamelSockOptData *data);
static int tcp_setsockopt (CamelTcpStream *stream, const CamelSockOptData *data);
static struct sockaddr *tcp_get_local_address (CamelTcpStream *stream, socklen_t *len);
static struct sockaddr *tcp_get_remote_address (CamelTcpStream *stream, socklen_t *len);

static void
camel_tcp_stream_class_init (CamelTcpStreamClass *camel_tcp_stream_class)
{
	/*CamelStreamClass *camel_stream_class = CAMEL_STREAM_CLASS (camel_tcp_stream_class);*/
	
	parent_class = CAMEL_STREAM_CLASS (camel_type_get_global_classfuncs (CAMEL_STREAM_TYPE));
	
	/* tcp stream methods */
	camel_tcp_stream_class->connect            = tcp_connect;
	camel_tcp_stream_class->getsockopt         = tcp_getsockopt;
	camel_tcp_stream_class->setsockopt         = tcp_setsockopt;
	camel_tcp_stream_class->get_local_address  = tcp_get_local_address;
	camel_tcp_stream_class->get_remote_address = tcp_get_remote_address;
}

static void
camel_tcp_stream_init (void *o)
{
	;
}

CamelType
camel_tcp_stream_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (CAMEL_STREAM_TYPE,
					    "CamelTcpStream",
					    sizeof (CamelTcpStream),
					    sizeof (CamelTcpStreamClass),
					    (CamelObjectClassInitFunc) camel_tcp_stream_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_tcp_stream_init,
					    NULL);
	}

	return type;
}


static int
tcp_connect (CamelTcpStream *stream, struct addrinfo *host)
{
	w(g_warning ("CamelTcpStream::connect called on default implementation"));
	return -1;
}

/**
 * camel_tcp_stream_connect:
 * @stream: a CamelTcpStream object.
 * @host: A linked list of addrinfo structures to try to connect, in
 * the order of most likely to least likely to work.
 *
 * Create a socket and connect based upon the data provided.
 *
 * Return value: zero on success or -1 on fail.
 **/
int
camel_tcp_stream_connect (CamelTcpStream *stream, struct addrinfo *host)
{
	g_return_val_if_fail (CAMEL_IS_TCP_STREAM (stream), -1);
	
	return CTS_CLASS (stream)->connect (stream, host);
}

static int
tcp_getsockopt (CamelTcpStream *stream, CamelSockOptData *data)
{
	w(g_warning ("CamelTcpStream::getsockopt called on default implementation"));
	return -1;
}

/**
 * camel_tcp_stream_getsockopt:
 * @stream: tcp stream object
 * @data: socket option data
 *
 * Get the socket options set on the stream and populate #data.
 *
 * Return value: zero on success or -1 on fail.
 **/
int
camel_tcp_stream_getsockopt (CamelTcpStream *stream, CamelSockOptData *data)
{
	g_return_val_if_fail (CAMEL_IS_TCP_STREAM (stream), -1);
	
	return CTS_CLASS (stream)->getsockopt (stream, data);
}

static int
tcp_setsockopt (CamelTcpStream *stream, const CamelSockOptData *data)
{
	w(g_warning ("CamelTcpStream::setsockopt called on default implementation"));
	return -1;
}

/**
 * camel_tcp_stream_setsockopt:
 * @stream: tcp stream object
 * @data: socket option data
 *
 * Set the socket options contained in #data on the stream.
 *
 * Return value: zero on success or -1 on fail.
 **/
int
camel_tcp_stream_setsockopt (CamelTcpStream *stream, const CamelSockOptData *data)
{
	g_return_val_if_fail (CAMEL_IS_TCP_STREAM (stream), -1);
	
	return CTS_CLASS (stream)->setsockopt (stream, data);
}

static struct sockaddr *
tcp_get_local_address (CamelTcpStream *stream, socklen_t *len)
{
	w(g_warning ("CamelTcpStream::get_local_address called on default implementation"));
	return NULL;
}

/**
 * camel_tcp_stream_get_local_address:
 * @stream: tcp stream object
 * @len: Pointer to address length which must be supplied.
 *
 * Get the local address of @stream.
 *
 * Return value: the stream's local address (which must be freed with
 * g_free()) if the stream is connected, or %NULL if not.
 **/
struct sockaddr *
camel_tcp_stream_get_local_address (CamelTcpStream *stream, socklen_t *len)
{
	g_return_val_if_fail (CAMEL_IS_TCP_STREAM (stream), NULL);
	g_return_val_if_fail(len != NULL, NULL);
	
	return CTS_CLASS (stream)->get_local_address (stream, len);
}

static struct sockaddr *
tcp_get_remote_address (CamelTcpStream *stream, socklen_t *len)
{
	w(g_warning ("CamelTcpStream::get_remote_address called on default implementation"));
	return NULL;
}

/**
 * camel_tcp_stream_get_remote_address:
 * @stream: tcp stream object
 * @len: Pointer to address length, which must be supplied.
 *
 * Get the remote address of @stream.
 *
 * Return value: the stream's remote address (which must be freed with
 * g_free()) if the stream is connected, or %NULL if not.
 **/
struct sockaddr *
camel_tcp_stream_get_remote_address (CamelTcpStream *stream, socklen_t *len)
{
	g_return_val_if_fail (CAMEL_IS_TCP_STREAM (stream), NULL);
	g_return_val_if_fail(len != NULL, NULL);

	return CTS_CLASS (stream)->get_remote_address (stream, len);
}
