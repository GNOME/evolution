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
#include "camel-stream-ssl.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

static CamelStreamClass *parent_class = NULL;

/* Returns the class for a CamelStreamSSL */
#define CSSSL_CLASS(so) CAMEL_STREAM_SSL_CLASS (CAMEL_OBJECT_GET_CLASS (so))

static SSL *open_ssl_connection (int sockfd);
static int close_ssl_connection (SSL *ssl);

static ssize_t stream_read (CamelStream *stream, char *buffer, size_t n);
static ssize_t stream_write (CamelStream *stream, const char *buffer, size_t n);
static int stream_flush  (CamelStream *stream);
static int stream_close  (CamelStream *stream);

static void
camel_stream_ssl_class_init (CamelStreamSSLClass *camel_stream_ssl_class)
{
	CamelStreamClass *camel_stream_class =
		CAMEL_STREAM_CLASS (camel_stream_ssl_class);
	
	parent_class = CAMEL_STREAM_CLASS (camel_type_get_global_classfuncs (camel_seekable_stream_get_type ()));
	
	/* virtual method overload */
	camel_stream_class->read = stream_read;
	camel_stream_class->write = stream_write;
	camel_stream_class->flush = stream_flush;
	camel_stream_class->close = stream_close;
}

static void
camel_stream_ssl_init (gpointer object, gpointer klass)
{
	CamelStreamSSL *stream = CAMEL_STREAM_SSL (object);
	
	stream->fd = -1;
	stream->ssl = NULL;
}

static void
camel_stream_ssl_finalize (CamelObject *object)
{
	CamelStreamSSL *stream = CAMEL_STREAM_FS (object);
	
	if (stream->ssl) {
		SSL_shutdown (stream->ssl);
		
		if (stream->ssl->ctx)
			SSL_CTX_free (stream->ssl->ctx);
		
		SSL_free (stream->ssl);
	}
	
	if (stream->fd != -1)
		close (stream->fd);
}


CamelType
camel_stream_ssl_get_type (void)
{
	static CamelType camel_stream_ssl_type = CAMEL_INVALID_TYPE;
	
	if (camel_stream_ssl_type == CAMEL_INVALID_TYPE) {
		camel_stream_ssl_type =
			camel_type_register (camel_stream_get_type (), "CamelStreamSSL",
					     sizeof (CamelStreamSSL),
					     sizeof (CamelStreamSSLClass),
					     (CamelObjectClassInitFunc) camel_stream_ssl_class_init,
					     NULL,
					     (CamelObjectInitFunc) camel_stream_ssl_init,
					     (CamelObjectFinalizeFunc) camel_stream_ssl_finalize);
	}
	
	return camel_stream_ssl_type;
}

static int
verify_callback (int ok, X509_STORE_CTX *ctx) 
{
	char *str, buf[256];
	X509 *cert;
	int err;
	
	cert = X509_STORE_CTX_get_current_cert (ctx);
	err = X509_STORE_CTX_get_error (ctx);
	
	str = X509_NAME_oneline (X509_get_subject_name (cert), buf, 256);
	if (str) {
		if (ok)
			d(fprintf (stderr, "CamelStreamSSL: depth=%d %s\n", ctx->error_depth, buf));
		else
			d(fprintf (stderr, "CamelStreamSSL: depth=%d error=%d %s\n",
				   ctx->error_depth, err, buf));
	}
	
	if (!ok) {
		switch (err) {
		case X509_V_ERR_CERT_NOT_YET_VALID:
		case X509_V_ERR_CERT_HAS_EXPIRED:
		case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
			ok = 1;
		}
	}
	
	return ok;
}

static SSL *
open_ssl_connection (int sockfd)
{
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	int n;
	
	/* SSLv23_client_method will negotiate with SSL v2, v3, or TLS v1 */
	ssl_ctx = SSL_CTX_new (SSLv23_client_method ());
	SSL_CTX_set_verify (ssl_ctx, SSL_VERIFY_PEER, &verify_cb);
	ssl = SSL_new (ssl_ctx);
	SSL_set_fd (ssl, sockfd);
	
	n = SSL_connect (ssl);
	if (n != 1) {
		SSL_shutdown (ssl);
		
		if (ssl->ctx)
			SSL_CTX_free (ssl->ctx);
		
		SSL_free (ssl);
		ssl = NULL;
	}
	
	return ssl;
}

static int
close_ssl_connection (SSL *ssl)
{
	if (ssl) {
		SSL_shutdown (ssl);
		
		if (ssl->ctx)
			SSL_CTX_free (ssl->ctx);
		
		SSL_free (ssl);
	}
	
	return 0;
}


/**
 * camel_stream_ssl_new:
 * @sockfd: a socket file descriptor
 *
 * Returns a stream associated with the given file descriptor.
 * When the stream is destroyed, the file descriptor will be closed.
 *
 * Return value: the stream
 **/
CamelStream *
camel_stream_ssl_new (int sockfd)
{
	CamelStreamSSL *stream_ssl;
	SSL *ssl;
	
	if (sockfd == -1)
		return NULL;
	
	ssl = open_ssl_connection (sockfd);
	if (!ssl)
		return NULL;
	
	stream_ssl = CAMEL_STREAM_SSL (camel_object_new (camel_stream_ssl_get_type ()));
	stream_ssl->sockfd = sockfd;
	stream_ssl->ssl = ssl;
	
	return CAMEL_STREAM (stream_ssl);
}

static ssize_t
stream_read (CamelStream *stream, char *buffer, size_t n)
{
	CamelStreamSSL *stream_ssl = CAMEL_STREAM_SSL (stream);
	ssize_t nread;
	
	do {
		nread = SSL_read (stream_ssl->ssl, buffer, n);
	} while (nread == -1 && errno == EINTR);
	
	return nread;
}

static ssize_t
stream_write (CamelStream *stream, const char *buffer, size_t n)
{
	CamelStreamSSL *stream_ssl = CAMEL_STREAM_SSL (stream);
	ssize_t v, written = 0;
	
	do {
		v = SSL_write (stream_ssl->ssl, buffer, n);
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
	return fsync (((CamelStreamSSL *)stream)->fd);
}

static int
stream_close (CamelStream *stream)
{
	close_ssl_connection (((CamelStreamSSL *)stream)->ssl);
	
	return close (((CamelStreamSSL *)stream)->fd);
}
