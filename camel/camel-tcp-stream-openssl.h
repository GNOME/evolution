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


#ifndef CAMEL_TCP_STREAM_OPENSSL_H
#define CAMEL_TCP_STREAM_OPENSSL_H


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-tcp-stream.h>

#define CAMEL_TCP_STREAM_OPENSSL_TYPE     (camel_tcp_stream_openssl_get_type ())
#define CAMEL_TCP_STREAM_OPENSSL(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_TCP_STREAM_OPENSSL_TYPE, CamelTcpStreamOpenSSL))
#define CAMEL_TCP_STREAM_OPENSSL_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_TCP_STREAM_OPENSSL_TYPE, CamelTcpStreamOpenSSLClass))
#define CAMEL_IS_TCP_STREAM_OPENSSL(o)    (CAMEL_CHECK_TYPE((o), CAMEL_TCP_STREAM_OPENSSL_TYPE))

struct _CamelTcpStreamOpenSSL
{
	CamelTcpStream parent_object;
	
	struct _CamelTcpStreamOpenSSLPrivate *priv;
};

typedef struct {
	CamelTcpStreamClass parent_class;
	
	/* virtual functions */
	
} CamelTcpStreamOpenSSLClass;

/* Standard Camel function */
CamelType camel_tcp_stream_openssl_get_type (void);

/* public methods */
CamelStream *camel_tcp_stream_openssl_new (CamelService *service, const char *expected_host);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_TCP_STREAM_OPENSSL_H */
