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

#include "camel-tcp-stream.h"

static CamelStreamClass *parent_class = NULL;

/* Returns the class for a CamelTcpStream */
#define CTS_CLASS(so) CAMEL_TCP_STREAM_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static int tcp_connect    (CamelTcpStream *stream, struct hostent *host, int port);
static int tcp_getsockopt (CamelTcpStream *stream, CamelSockOptData *data);
static int tcp_setsockopt (CamelTcpStream *stream, const CamelSockOptData *data);

static void
camel_tcp_stream_class_init (CamelTcpStreamClass *camel_tcp_stream_class)
{
	CamelStreamClass *camel_stream_class =
		CAMEL_STREAM_CLASS (camel_tcp_stream_class);
	
	parent_class = CAMEL_STREAM_CLASS( camel_type_get_global_classfuncs( CAMEL_STREAM_TYPE ) );
	
	/* tcp stream methods */
	camel_tcp_stream_class->connect    = tcp_connect;
	camel_tcp_stream_class->getsockopt = tcp_getsockopt;
	camel_tcp_stream_class->setsockopt = tcp_setsockopt;
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
tcp_connect (CamelTcpStream *stream, struct hostent *host, int port)
{
	g_warning ("CamelTcpStream::connect called on default implementation\n");
	return -1;
}

/**
 * camel_tcp_stream_connect:
 * @stream: a CamelTcpStream object.
 * @host: a hostent value
 * @port: port
 *
 * Create a socket and connect based upon the data provided.
 *
 * Return value: zero on success or -1 on fail.
 **/
int
camel_tcp_stream_connect (CamelTcpStream *stream, struct hostent *host, int port)
{
	g_return_val_if_fail (CAMEL_IS_TCP_STREAM (stream), -1);
	
	return CTS_CLASS (stream)->connect (stream, host, port);
}


static int
tcp_getsockopt (CamelTcpStream *stream, CamelSockOptData *data)
{
	g_warning ("CamelTcpStream::getsockopt called on default implementation\n");
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
	g_warning ("CamelTcpStream::setsockopt called on default implementation\n");
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
