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
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>

#define CAMEL_TCP_STREAM_TYPE     (camel_tcp_stream_get_type ())
#define CAMEL_TCP_STREAM(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_TCP_STREAM_TYPE, CamelTcpStream))
#define CAMEL_TCP_STREAM_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_TCP_STREAM_TYPE, CamelTcpStreamClass))
#define CAMEL_IS_TCP_STREAM(o)    (CAMEL_CHECK_TYPE((o), CAMEL_TCP_STREAM_TYPE))

typedef enum {
	CAMEL_SOCKOPT_NONBLOCKING,     /* nonblocking io */
	CAMEL_SOCKOPT_LINGER,          /* linger on close if data present */
	CAMEL_SOCKOPT_REUSEADDR,       /* allow local address reuse */
	CAMEL_SOCKOPT_KEEPALIVE,       /* keep connections alive */
	CAMEL_SOCKOPT_RECVBUFFERSIZE,  /* receive buffer size */
	CAMEL_SOCKOPT_SENDBUFFERSIZE,  /* send buffer size */
	
	CAMEL_SOCKOPT_IPTIMETOLIVE,    /* time to live */
	CAMEL_SOCKOPT_IPTYPEOFSERVICE, /* type of service and precedence */
	
	CAMEL_SOCKOPT_ADDMEMBER,       /* add an IP group membership */
	CAMEL_SOCKOPT_DROPMEMBER,      /* drop an IP group membership */
	CAMEL_SOCKOPT_MCASTINTERFACE,  /* multicast interface address */
	CAMEL_SOCKOPT_MCASTTIMETOLIVE, /* multicast timetolive */
	CAMEL_SOCKOPT_MCASTLOOPBACK,   /* multicast loopback */
	
	CAMEL_SOCKOPT_NODELAY,         /* don't delay send to coalesce packets */
	CAMEL_SOCKOPT_MAXSEGMENT,      /* maximum segment size */
	CAMEL_SOCKOPT_BROADCAST,       /* enable broadcast */
	CAMEL_SOCKOPT_LAST
} CamelSockOpt;

typedef struct linger CamelLinger;

typedef struct _CamelSockOptData {
	CamelSockOpt option;
	union {
		guint       ip_ttl;              /* IP time to live */
		guint       mcast_ttl;           /* IP multicast time to live */
		guint       tos;                 /* IP type of service and precedence */
		gboolean    non_blocking;        /* Non-blocking (network) I/O */
		gboolean    reuse_addr;          /* Allow local address reuse */
		gboolean    keep_alive;          /* Keep connections alive */
		gboolean    mcast_loopback;      /* IP multicast loopback */
		gboolean    no_delay;            /* Don't delay send to coalesce packets */
		gboolean    broadcast;           /* Enable broadcast */
		size_t      max_segment;         /* Maximum segment size */
		size_t      recv_buffer_size;    /* Receive buffer size */
		size_t      send_buffer_size;    /* Send buffer size */
		CamelLinger linger;              /* Time to linger on close if data present */
	} value;
} CamelSockOptData;

struct _CamelTcpStream
{
	CamelStream parent_object;
	
};

typedef struct {
	CamelStreamClass parent_class;

	/* Virtual methods */
	int (*connect)    (CamelTcpStream *stream, struct hostent *host, int port);
	int (*getsockopt) (CamelTcpStream *stream, CamelSockOptData *data);
	int (*setsockopt) (CamelTcpStream *stream, const CamelSockOptData *data);

} CamelTcpStreamClass;

/* Standard Camel function */
CamelType camel_tcp_stream_get_type (void);

/* public methods */
int         camel_tcp_stream_connect    (CamelTcpStream *stream, struct hostent *host, int port);
int         camel_tcp_stream_getsockopt (CamelTcpStream *stream, CamelSockOptData *data);
int         camel_tcp_stream_setsockopt (CamelTcpStream *stream, const CamelSockOptData *data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_TCP_STREAM_H */
