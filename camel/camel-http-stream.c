/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "camel-net-utils.h"
#include "camel-http-stream.h"

#include "camel-mime-utils.h"
#include "camel-stream-buffer.h"
#include "camel-tcp-stream-raw.h"
#ifdef HAVE_SSL
#include "camel-tcp-stream-ssl.h"
#endif
#include "camel-exception.h"
#include "camel-session.h"
#include "camel-service.h" /* for hostname stuff */

#define d(x) 

static CamelStreamClass *parent_class = NULL;

static ssize_t stream_read (CamelStream *stream, char *buffer, size_t n);
static ssize_t stream_write (CamelStream *stream, const char *buffer, size_t n);
static int stream_flush  (CamelStream *stream);
static int stream_close  (CamelStream *stream);
static int stream_reset  (CamelStream *stream);

static void
camel_http_stream_class_init (CamelHttpStreamClass *camel_http_stream_class)
{
	CamelStreamClass *camel_stream_class =
		CAMEL_STREAM_CLASS (camel_http_stream_class);
	
	parent_class = CAMEL_STREAM_CLASS (camel_type_get_global_classfuncs (camel_stream_get_type ()));
	
	/* virtual method overload */
	camel_stream_class->read = stream_read;
	camel_stream_class->write = stream_write;
	camel_stream_class->flush = stream_flush;
	camel_stream_class->close = stream_close;
	camel_stream_class->reset = stream_reset;
}

static void
camel_http_stream_init (gpointer object, gpointer klass)
{
	CamelHttpStream *http = CAMEL_HTTP_STREAM (object);
	
	http->parser = NULL;
	http->content_type = NULL;
	http->headers = NULL;
	http->session = NULL;
	http->url = NULL;
	http->proxy = NULL;
	http->authrealm = NULL;
	http->authpass = NULL;
	http->statuscode = 0;
	http->raw = NULL;
}

static void
camel_http_stream_finalize (CamelObject *object)
{
	CamelHttpStream *http = CAMEL_HTTP_STREAM (object);
	
	if (http->parser)
		camel_object_unref(http->parser);
	
	if (http->content_type)
		camel_content_type_unref (http->content_type);
	
	if (http->headers)
		camel_header_raw_clear (&http->headers);
	
	if (http->session)
		camel_object_unref(http->session);
	
	if (http->url)
		camel_url_free (http->url);
	
	if (http->proxy)
		camel_url_free (http->proxy);
	
	g_free (http->authrealm);
	g_free (http->authpass);
	
	if (http->raw)
		camel_object_unref(http->raw);
	if (http->read)
		camel_object_unref(http->read);
}

CamelType
camel_http_stream_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_stream_get_type (),
					    "CamelHttpStream",
					    sizeof (CamelHttpStream),
					    sizeof (CamelHttpStreamClass),
					    (CamelObjectClassInitFunc) camel_http_stream_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_http_stream_init,
					    (CamelObjectFinalizeFunc) camel_http_stream_finalize);
	}
	
	return type;
}

/**
 * camel_http_stream_new:
 * @method: HTTP method
 * @session: active session
 * @url: URL to act upon
 *
 * Return value: a http stream
 **/
CamelStream *
camel_http_stream_new (CamelHttpMethod method, struct _CamelSession *session, CamelURL *url)
{
	CamelHttpStream *stream;
	char *str;
	
	g_return_val_if_fail(CAMEL_IS_SESSION(session), NULL);
	g_return_val_if_fail(url != NULL, NULL);
	
	stream = CAMEL_HTTP_STREAM (camel_object_new (camel_http_stream_get_type ()));
	
	stream->method = method;
	stream->session = session;
	camel_object_ref(session);
	
	str = camel_url_to_string (url, 0);
	stream->url = camel_url_new (str, NULL);
	g_free (str);
	
	return (CamelStream *)stream;
}

#define SSL_FLAGS (CAMEL_TCP_STREAM_SSL_ENABLE_SSL2 | CAMEL_TCP_STREAM_SSL_ENABLE_SSL3)

static CamelStream *
http_connect (CamelHttpStream *http, CamelURL *url)
{
	CamelStream *stream = NULL;
	struct addrinfo *ai, hints = { 0 };
	int errsave;
	char *serv;

	d(printf("connecting to http stream @ '%s'\n", url->host));

	if (!strcasecmp (url->protocol, "https")) {
#ifdef HAVE_SSL
		stream = camel_tcp_stream_ssl_new (http->session, url->host, SSL_FLAGS);
#endif
	} else {
		stream = camel_tcp_stream_raw_new ();
	}
	
	if (stream == NULL) {
		errno = EINVAL;
		return NULL;
	}

	if (url->port) {
		serv = g_alloca(16);
		sprintf(serv, "%d", url->port);
	} else {
		serv = url->protocol;
	}
	hints.ai_socktype = SOCK_STREAM;

	ai = camel_getaddrinfo(url->host, serv, &hints, NULL);
	if (ai == NULL) {
		camel_object_unref (stream);
		return NULL;
	}
	
	if (camel_tcp_stream_connect (CAMEL_TCP_STREAM (stream), ai) == -1) {
		errsave = errno;
		camel_object_unref (stream);
		camel_freeaddrinfo(ai);
		errno = errsave;
		return NULL;
	}
	
	camel_freeaddrinfo(ai);

	http->raw = stream;
	http->read = camel_stream_buffer_new (stream, CAMEL_STREAM_BUFFER_READ);
	
	return stream;
}

static void
http_disconnect(CamelHttpStream *http)
{
	if (http->raw) {
		camel_object_unref(http->raw);
		http->raw = NULL;
	}
			
	if (http->read) {
		camel_object_unref(http->read);
		http->read = NULL;
	}

	if (http->parser) {
		camel_object_unref(http->parser);
		http->parser = NULL;
	}
}

static const char *
http_next_token (const unsigned char *in)
{
	const unsigned char *inptr = in;
	
	while (*inptr && !isspace ((int) *inptr))
		inptr++;
	
	while (*inptr && isspace ((int) *inptr))
		inptr++;
	
	return (const char *) inptr;
}

static int
http_get_statuscode (CamelHttpStream *http)
{
	const char *token;
	char buffer[4096];
	
	if (camel_stream_buffer_gets ((CamelStreamBuffer *)http->read, buffer, sizeof (buffer)) <= 0)
		return -1;

	d(printf("HTTP Status: %s\n", buffer));

	/* parse the HTTP status code */
	if (!strncasecmp (buffer, "HTTP/", 5)) {
		token = http_next_token (buffer);
		http->statuscode = camel_header_decode_int (&token);
		return http->statuscode;
	}

	http_disconnect(http);
	
	return -1;
}

static int
http_get_headers (CamelHttpStream *http)
{
	struct _camel_header_raw *headers, *node, *tail;
	const char *type;
	char *buf;
	size_t len;
	int err;
	
	if (http->parser)
		camel_object_unref (http->parser);
	
	http->parser = camel_mime_parser_new ();
	camel_mime_parser_init_with_stream (http->parser, http->read);
	
	switch (camel_mime_parser_step (http->parser, &buf, &len)) {
	case CAMEL_MIME_PARSER_STATE_MESSAGE:
	case CAMEL_MIME_PARSER_STATE_HEADER:
		headers = camel_mime_parser_headers_raw (http->parser);
		if (http->content_type)
			camel_content_type_unref (http->content_type);
		type = camel_header_raw_find (&headers, "Content-Type", NULL);
		if (type)
			http->content_type = camel_content_type_decode (type);
		else
			http->content_type = NULL;
		
		if (http->headers)
			camel_header_raw_clear (&http->headers);
		
		http->headers = NULL;
		tail = (struct _camel_header_raw *) &http->headers;
		
		d(printf("HTTP Headers:\n"));
		while (headers) {
			d(printf(" %s:%s\n", headers->name, headers->value));
			node = g_new (struct _camel_header_raw, 1);
			node->next = NULL;
			node->name = g_strdup (headers->name);
			node->value = g_strdup (headers->value);
			node->offset = headers->offset;
			tail->next = node;
			tail = node;
			headers = headers->next;
		}
		
		break;
	default:
		g_warning ("Invalid state encountered???: %d", camel_mime_parser_state (http->parser));
	}
	
	err = camel_mime_parser_errno (http->parser);
	
	if (err != 0) {
		camel_object_unref (http->parser);
		http->parser = NULL;
		goto exception;
	}
	
	camel_mime_parser_drop_step (http->parser);
	
	return 0;
	
 exception:
	http_disconnect(http);
	
	return -1;
}

static int
http_method_invoke (CamelHttpStream *http)
{
	const char *method = NULL;
	char *url;
	
	switch (http->method) {
	case CAMEL_HTTP_METHOD_GET:
		method = "GET";
		break;
	case CAMEL_HTTP_METHOD_HEAD:
		method = "HEAD";
		break;
	default:
		g_assert_not_reached ();
	}
	
	url = camel_url_to_string (http->url, 0);
	d(printf("HTTP Stream Sending: %s %s HTTP/1.0\r\nUser-Agent: %s\r\nHost: %s\r\n",
		 method,
		 http->proxy ? url : http->url->path,
		 http->user_agent ? http->user_agent : "CamelHttpStream/1.0",
		 http->url->host));
	if (camel_stream_printf (http->raw, "%s %s HTTP/1.0\r\nUser-Agent: %s\r\nHost: %s\r\n",
				 method,
				 http->proxy ? url : http->url->path,
				 http->user_agent ? http->user_agent : "CamelHttpStream/1.0",
				 http->url->host) == -1) {
		http_disconnect(http);
		g_free (url);
		return -1;
	}
	g_free (url);

	if (http->authrealm)
		d(printf("HTTP Stream Sending: WWW-Authenticate: %s\n", http->authrealm));
	
	if (http->authrealm && camel_stream_printf (http->raw, "WWW-Authenticate: %s\r\n", http->authrealm) == -1) {
		http_disconnect(http);
		return -1;
	}

	if (http->authpass && http->proxy)
		d(printf("HTTP Stream Sending: Proxy-Aurhorization: Basic %s\n", http->authpass));
	
	if (http->authpass && http->proxy && camel_stream_printf (http->raw, "Proxy-Authorization: Basic %s\r\n",
								  http->authpass) == -1) {
		http_disconnect(http);
		return -1;
	}
	
	/* end the headers */
	if (camel_stream_write (http->raw, "\r\n", 2) == -1 || camel_stream_flush (http->raw) == -1) {
		http_disconnect(http);
		return -1;
	}
	
	return 0;
}

static ssize_t
stream_read (CamelStream *stream, char *buffer, size_t n)
{
	CamelHttpStream *http = CAMEL_HTTP_STREAM (stream);
	const char *parser_buf;
	ssize_t nread;
	
	if (http->method != CAMEL_HTTP_METHOD_GET && http->method != CAMEL_HTTP_METHOD_HEAD) {
		errno = EIO;
		return -1;
	}
	
 redirect:
	
	if (!http->raw) {
		if (http_connect (http, http->proxy ? http->proxy : http->url) == NULL)
			return -1;
		
		if (http_method_invoke (http) == -1)
			return -1;
		
		if (http_get_statuscode (http) == -1)
			return -1;
		
		if (http_get_headers (http) == -1)
			return -1;
		
		switch (http->statuscode) {
		case 200:
		case 206:
			/* we are OK to go... */
			break;
		case 301:
		case 302: {
			char *loc;
			CamelURL *url;

			camel_content_type_unref (http->content_type);
			http->content_type = NULL;
			http_disconnect(http);

			loc = g_strdup(camel_header_raw_find(&http->headers, "Location", NULL));
			if (loc == NULL) {
				camel_header_raw_clear(&http->headers);
				return -1;
			}

			/* redirect... */
			g_strstrip(loc);
			d(printf("HTTP redirect, location = %s\n", loc));
			url = camel_url_new_with_base(http->url, loc);
			camel_url_free (http->url);
			http->url = url;
			if (url == NULL)
				http->url = camel_url_new(loc, NULL);
			g_free(loc);
			if (http->url == NULL) {
				camel_header_raw_clear (&http->headers);
				return -1;
			}
			d(printf(" redirect url = %p\n", http->url));
			camel_header_raw_clear (&http->headers);

			goto redirect;
			break; }
		case 407:
			/* failed proxy authentication? */
		default:
			/* unknown error */
			http_disconnect(http);
			return -1;
		}
	}
	
	nread = camel_mime_parser_read (http->parser, &parser_buf, n);
	
	if (nread > 0)
		memcpy (buffer, parser_buf, nread);
	else if (nread == 0)
		stream->eos = TRUE;
	
	return nread;
}

static ssize_t
stream_write (CamelStream *stream, const char *buffer, size_t n)
{
	return -1;
}

static int
stream_flush (CamelStream *stream)
{
	CamelHttpStream *http = (CamelHttpStream *) stream;
	
	if (http->raw)
		return camel_stream_flush (http->raw);
	else
		return 0;
}

static int
stream_close (CamelStream *stream)
{
	CamelHttpStream *http = (CamelHttpStream *) stream;
	
	if (http->raw) {
		if (camel_stream_close (http->raw) == -1)
			return -1;

		http_disconnect(http);
	}
	
	return 0;
}

static int
stream_reset (CamelStream *stream)
{
	CamelHttpStream *http = CAMEL_HTTP_STREAM (stream);
	
	if (http->raw)
		http_disconnect(http);

	return 0;
};

CamelContentType *
camel_http_stream_get_content_type (CamelHttpStream *http_stream)
{
	g_return_val_if_fail (CAMEL_IS_HTTP_STREAM (http_stream), NULL);
	
	if (!http_stream->content_type && !http_stream->raw) {
		if (http_connect (http_stream, http_stream->url) == NULL)
			return NULL;
		
		if (http_method_invoke (http_stream) == -1)
			return NULL;
		
		if (http_get_headers (http_stream) == -1)
			return NULL;
	}
	
	if (http_stream->content_type)
		camel_content_type_ref (http_stream->content_type);
	
	return http_stream->content_type;
}

void
camel_http_stream_set_user_agent (CamelHttpStream *http_stream, const char *user_agent)
{
	g_return_if_fail (CAMEL_IS_HTTP_STREAM (http_stream));
	
	g_free (http_stream->user_agent);
	http_stream->user_agent = g_strdup (user_agent);
}

void
camel_http_stream_set_proxy (CamelHttpStream *http_stream, const char *proxy_url)
{
	g_return_if_fail (CAMEL_IS_HTTP_STREAM (http_stream));
	
	if (http_stream->proxy)
		camel_url_free (http_stream->proxy);

	if (proxy_url == NULL)
		http_stream->proxy = NULL;
	else
		http_stream->proxy = camel_url_new (proxy_url, NULL);

	if (http_stream->proxy) {
		char *basic, *basic64;

		basic = g_strdup_printf("%s:%s", http_stream->proxy->user?http_stream->proxy->user:"",
					http_stream->proxy->passwd?http_stream->proxy->passwd:"");
		basic64 = camel_base64_encode_simple(basic, strlen(basic));
		memset(basic, 0, strlen(basic));
		g_free(basic);
		camel_http_stream_set_proxy_authpass(http_stream, basic64);
		memset(basic64, 0, strlen(basic64));
		g_free(basic64);
	} else {
		camel_http_stream_set_proxy_authpass(http_stream, NULL);
	}
}

void
camel_http_stream_set_proxy_authrealm (CamelHttpStream *http_stream, const char *proxy_authrealm)
{
	g_return_if_fail (CAMEL_IS_HTTP_STREAM (http_stream));
	
	g_free (http_stream->authrealm);
	http_stream->authrealm = g_strdup (proxy_authrealm);
}

void
camel_http_stream_set_proxy_authpass (CamelHttpStream *http_stream, const char *proxy_authpass)
{
	g_return_if_fail (CAMEL_IS_HTTP_STREAM (http_stream));
	
	g_free (http_stream->authpass);
	http_stream->authpass = g_strdup (proxy_authpass);
}
