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


#ifndef CAMEL_TCP_STREAM_SSL_H
#define CAMEL_TCP_STREAM_SSL_H


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-tcp-stream.h>

#define CAMEL_TCP_STREAM_SSL_TYPE     (camel_tcp_stream_ssl_get_type ())
#define CAMEL_TCP_STREAM_SSL(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_TCP_STREAM_SSL_TYPE, CamelTcpStreamSSL))
#define CAMEL_TCP_STREAM_SSL_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_TCP_STREAM_SSL_TYPE, CamelTcpStreamSSLClass))
#define CAMEL_IS_TCP_STREAM_SSL(o)    (CAMEL_CHECK_TYPE((o), CAMEL_TCP_STREAM_SSL_TYPE))

struct _CamelSession;

enum {
	CAMEL_TCP_STREAM_SSL_ENABLE_SSL2 = (1 << 0),
	CAMEL_TCP_STREAM_SSL_ENABLE_SSL3 = (1 << 1),
	CAMEL_TCP_STREAM_SSL_ENABLE_TLS  = (1 << 2),
};

struct _CamelTcpStreamSSL {
	CamelTcpStream parent_object;
	
	struct _CamelTcpStreamSSLPrivate *priv;
};

typedef struct {
	CamelTcpStreamClass parent_class;
	
	/* virtual functions */
	
} CamelTcpStreamSSLClass;

/* Standard Camel function */
CamelType camel_tcp_stream_ssl_get_type (void);

/* public methods */
CamelStream *camel_tcp_stream_ssl_new (struct _CamelSession *session, const char *expected_host, guint32 flags);

CamelStream *camel_tcp_stream_ssl_new_raw (struct _CamelSession *session, const char *expected_host, guint32 flags);

int camel_tcp_stream_ssl_enable_ssl (CamelTcpStreamSSL *ssl);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_TCP_STREAM_SSL_H */
