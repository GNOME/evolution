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


#ifndef CAMEL_TCP_STREAM_H
#define CAMEL_TCP_STREAM_H


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <camel/camel-stream.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define CAMEL_TCP_STREAM_TYPE     (camel_tcp_stream_get_type ())
#define CAMEL_TCP_STREAM(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_TCP_STREAM_TYPE, CamelTcpStream))
#define CAMEL_TCP_STREAM_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_TCP_STREAM_TYPE, CamelTcpStreamClass))
#define CAMEL_IS_TCP_STREAM(o)    (CAMEL_CHECK_TYPE((o), CAMEL_TCP_STREAM_TYPE))

struct _CamelTcpStream
{
	CamelStream parent_object;
	
};

typedef struct {
	CamelStreamClass parent_class;

	/* Virtual methods */
	int (*connect)    (CamelTcpStream *stream, struct hostent *host, int port);
	int (*disconnect) (CamelTcpStream *stream);
	
} CamelTcpStreamClass;

/* Standard Camel function */
CamelType camel_tcp_stream_get_type (void);

/* public methods */
int camel_tcp_stream_connect    (CamelTcpStream *stream, struct hostent *host, int port);
int camel_tcp_stream_disconnect (CamelTcpStream *stream);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_TCP_STREAM_H */
