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


#ifndef CAMEL_STREAM_SSL_H
#define CAMEL_STREAM_SSL_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-stream.h>
#include <openssl/ssl.h>

#define CAMEL_STREAM_SSL_TYPE     (camel_stream_ssl_get_type ())
#define CAMEL_STREAM_SSL(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_STREAM_SSL_TYPE, CamelStreamSSL))
#define CAMEL_STREAM_SSL_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_STREAM_SSL_TYPE, CamelStreamSSLClass))
#define CAMEL_IS_STREAM_SSL(o)    (CAMEL_CHECK_TYPE((o), CAMEL_STREAM_SSL_TYPE))

struct _CamelStreamSSL {
	CamelStream parent_object;
	
	int sockfd;             /* file descriptor on the underlying socket */
	SSL *ssl;               /* SSL structure */
};

typedef struct {
	CamelStreamClass parent_class;
} CamelStreamSSLClass;

/* Standard Camel function */
CamelType camel_stream_ssl_get_type (void);

/* public methods */
CamelStream *camel_stream_ssl_new (int sockfd);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_STREAM_SSL_H */
