/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-smtp-transport.c : class for a smtp transport */

/* 
 * Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 * Copyright (C) 2000 Helix Code, Inc. (www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include "config.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#undef MIN
#undef MAX
#include "camel-mime-filter-crlf.h"
#include "camel-mime-filter-linewrap.h"
#include "camel-stream-filter.h"
#include "camel-smtp-transport.h"
#include "camel-mime-message.h"
#include "camel-multipart.h"
#include "camel-mime-part.h"
#include "camel-stream-buffer.h"
#include "camel-stream-fs.h"
#include "camel-session.h"
#include "camel-exception.h"

#define d(x) x

/* Specified in RFC 821 */
#define SMTP_PORT 25

/* camel smtp transport class prototypes */
static gboolean _can_send (CamelTransport *transport, CamelMedium *message);
static gboolean _send (CamelTransport *transport, CamelMedium *message, CamelException *ex);
static gboolean _send_to (CamelTransport *transport, CamelMedium *message, GList *recipients, CamelException *ex);

/* support prototypes */
static gboolean smtp_connect (CamelService *service, CamelException *ex);
static gboolean smtp_disconnect (CamelService *service, gboolean clean, CamelException *ex);
static GList *esmtp_get_authtypes(gchar *buffer);
static GList *query_auth_types_connected (CamelService *service, CamelException *ex);
static GList *query_auth_types_generic (CamelService *service, CamelException *ex);
static void free_auth_types (CamelService *service, GList *authtypes);
static char *get_name (CamelService *service, gboolean brief);

static gboolean smtp_helo (CamelSmtpTransport *transport, CamelException *ex);
static gboolean smtp_mail (CamelSmtpTransport *transport, const char *sender,
			   gboolean has_8bit_parts, CamelException *ex);
static gboolean smtp_rcpt (CamelSmtpTransport *transport, const char *recipient, CamelException *ex);
static gboolean smtp_data (CamelSmtpTransport *transport, CamelMedium *message,
			   gboolean has_8bit_parts, CamelException *ex);
static gboolean smtp_rset (CamelSmtpTransport *transport, CamelException *ex);
static gboolean smtp_quit (CamelSmtpTransport *transport, CamelException *ex);

/* private data members */
static CamelServiceClass *service_class = NULL;

static void
camel_smtp_transport_class_init (CamelSmtpTransportClass *camel_smtp_transport_class)
{
	CamelTransportClass *camel_transport_class =
		CAMEL_TRANSPORT_CLASS (camel_smtp_transport_class);
	CamelServiceClass *camel_service_class =
		CAMEL_SERVICE_CLASS (camel_smtp_transport_class);
	
	service_class = CAMEL_SERVICE_CLASS (camel_type_get_global_classfuncs (camel_service_get_type ()));
	
	/* virtual method overload */
	camel_service_class->connect = smtp_connect;
	camel_service_class->disconnect = smtp_disconnect;
	camel_service_class->query_auth_types_generic = query_auth_types_generic;
	camel_service_class->query_auth_types_connected = query_auth_types_connected;
	camel_service_class->free_auth_types = free_auth_types;
	camel_service_class->get_name = get_name;

	camel_transport_class->can_send = _can_send;
	camel_transport_class->send = _send;
	camel_transport_class->send_to = _send_to;
}

static void
camel_smtp_transport_init (gpointer object)
{
	CamelTransport *transport = CAMEL_TRANSPORT (object);
	
	transport->supports_8bit = FALSE;
}

CamelType
camel_smtp_transport_get_type (void)
{
	static CamelType camel_smtp_transport_type = CAMEL_INVALID_TYPE;
	
	if (camel_smtp_transport_type == CAMEL_INVALID_TYPE) {
		camel_smtp_transport_type =
			camel_type_register (CAMEL_TRANSPORT_TYPE, "CamelSmtpTransport",
					     sizeof (CamelSmtpTransport),
					     sizeof (CamelSmtpTransportClass),
					     (CamelObjectClassInitFunc) camel_smtp_transport_class_init,
					     NULL,
					     (CamelObjectInitFunc) camel_smtp_transport_init,
					     NULL);
	}
	
	return camel_smtp_transport_type;
}

static gboolean
smtp_connect (CamelService *service, CamelException *ex)
{
	CamelSmtpTransport *transport = CAMEL_SMTP_TRANSPORT (service);
	struct hostent *h;
	struct sockaddr_in sin;
	gint fd, num, i;
	guint32 addrlen;
	gchar *pass = NULL, *respbuf = NULL;
	
	if (!service_class->connect (service, ex))
		return FALSE;
	
	h = camel_service_gethost (service, ex);
	if (!h)
		return FALSE;
	
	/* set some smtp transport defaults */
	transport->is_esmtp = FALSE;
	transport->esmtp_supported_authtypes = NULL;
	CAMEL_TRANSPORT (transport)->supports_8bit = FALSE;
	
	sin.sin_family = h->h_addrtype;
	sin.sin_port = htons (service->url->port ? service->url->port : SMTP_PORT);
	memcpy (&sin.sin_addr, h->h_addr, sizeof (sin.sin_addr));
	
	fd = socket (h->h_addrtype, SOCK_STREAM, 0);
	if (fd == -1 || connect (fd, (struct sockaddr *)&sin, sizeof (sin)) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not connect to %s (port %d): %s",
				      service->url->host,
				      service->url->port ? service->url->port : SMTP_PORT,
				      strerror (errno));
		if (fd > -1)
			close (fd);
		g_free (pass);
		return FALSE;
	}
	
	/* get the localaddr - needed later by smtp_helo */
	addrlen = sizeof (transport->localaddr);
	getsockname (fd, (struct sockaddr*)&transport->localaddr, &addrlen);
	
	transport->ostream = camel_stream_fs_new_with_fd (fd);
	transport->istream = camel_stream_buffer_new (transport->ostream, 
						      CAMEL_STREAM_BUFFER_READ);
	
	/* Read the greeting, note whether the server is ESMTP or not. */
	do {
		/* Check for "220" */
		g_free (respbuf);
		respbuf = camel_stream_buffer_read_line (CAMEL_STREAM_BUFFER (transport->istream));
		if (!respbuf || strncmp (respbuf, "220", 3)) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "Welcome response error: "
					      "%s: possibly non-fatal",
					      g_strerror (errno));
			return FALSE;
		}
		if (strstr (respbuf, "ESMTP"))
			transport->is_esmtp = TRUE;
	} while (*(respbuf+3) == '-'); /* if we got "220-" then loop again */
	g_free (respbuf);
	
	/* send HELO (or EHLO, depending on the service type) */
	if (!transport->is_esmtp) {
		/* If we did not auto-detect ESMTP, we should still send EHLO */
		transport->is_esmtp = TRUE;
		if (!smtp_helo (transport, NULL)) {
			/* Okay, apprently this server doesn't support ESMTP */
			transport->is_esmtp = FALSE;
			smtp_helo (transport, ex);
		}
	} else {
		/* send EHLO */
		smtp_helo (transport, ex);
	}
	
	/* check to see if AUTH is required, if so...then AUTH ourselves */
	if (transport->is_esmtp && transport->esmtp_supported_authtypes) {
		/* not really supported yet, but we can at least show what auth types are supported */
		d(fprintf (stderr, "camel-smtp-transport::connect(): %s requires AUTH\n", service->url->host));
		num = g_list_length (transport->esmtp_supported_authtypes);
		
		for (i = 0; i < num; i++)
			d(fprintf (stderr, "\nSupported AUTH: %s\n\n", 
				(gchar *) g_list_nth_data (transport->esmtp_supported_authtypes, i)));
		
		g_list_free (transport->esmtp_supported_authtypes);
		transport->esmtp_supported_authtypes = NULL;
	} else {
		d(fprintf (stderr, "\ncamel-smtp-transport::connect(): provider does not use AUTH\n\n"));
	}
	
	return TRUE;
}

static gboolean
smtp_disconnect (CamelService *service, gboolean clean, CamelException *ex)
{
	CamelSmtpTransport *transport = CAMEL_SMTP_TRANSPORT (service);
	
	/*if (!service->connected)
	 *	return TRUE;
	 */

	if (clean) {
		/* send the QUIT command to the SMTP server */
		smtp_quit (transport, ex);
	}
	
	if (!service_class->disconnect (service, clean, ex))
		return FALSE;
	
	g_free (transport->esmtp_supported_authtypes);
	transport->esmtp_supported_authtypes = NULL;
	camel_object_unref (CAMEL_OBJECT (transport->ostream));
	camel_object_unref (CAMEL_OBJECT (transport->istream));
	transport->ostream = NULL;
	transport->istream = NULL;
	
	return TRUE;
}

static GList *
esmtp_get_authtypes (gchar *buffer)
{
	GList *ret = NULL;
	gchar *start, *end;
	
	/* advance to the first token */
	for (start = buffer; *start == ' ' || *start == '='; start++);
	
	for ( ; *start; ) {
		/* advance to the end of the token */
		for (end = start; *end && *end != ' '; end++);
		
		ret = g_list_append (ret, g_strndup (start, end - start));
		
		/* advance to the next token */
		for (start = end; *start == ' '; start++);
	}
	
	return ret;
}

/* FIXME: use these? */
#ifdef notyet
static CamelServiceAuthType no_authtype = {
	"No authentication required",
	
	"This option will connect to the SMTP server without using any "
	"kind of authentication. This should be fine for connecting to "
	"most SMTP servers."
	
	"",
	FALSE
};

static CamelServiceAuthType cram_md5_authtype = {
	"CRAM-MD5",
	
	"This option will connect to the SMTP server using CRAM-MD5 "
	"authentication.",
	
	"CRAM-MD5",
	TRUE
};
#endif

static GList *
query_auth_types_connected (CamelService *service, CamelException *ex)
{
	/* FIXME: Re-enable this when auth types are actually
	 * implemented.
	 */
	
	return NULL;
}

static GList *
query_auth_types_generic (CamelService *service, CamelException *ex)
{
	/* FIXME: Re-enable this when auth types are actually
	 * implemented.
	 */
	
	return NULL;
}

static void
free_auth_types (CamelService *service, GList *authtypes)
{
	g_list_free (authtypes);
}

static char *
get_name (CamelService *service, gboolean brief)
{
	if (brief)
		return g_strdup_printf ("SMTP server %s", service->url->host);
	else {
		return g_strdup_printf ("SMTP mail delivery via %s",
					service->url->host);
	}
}

static gboolean
_can_send (CamelTransport *transport, CamelMedium *message)
{
	return CAMEL_IS_MIME_MESSAGE (message);
}

static gboolean
_send_to (CamelTransport *transport, CamelMedium *message,
	  GList *recipients, CamelException *ex)
{
	CamelSmtpTransport *smtp_transport = CAMEL_SMTP_TRANSPORT (transport);
	const CamelInternetAddress *cia;
	char *recipient;
	const char *addr;
	gboolean has_8bit_parts;
	GList *r;
	
	cia = camel_mime_message_get_from(CAMEL_MIME_MESSAGE (message));
	if (!cia) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Cannot send message: "
				      "sender address not defined.");
		return FALSE;
	}
	
	if (!camel_internet_address_get (cia, 0, NULL, &addr)) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Cannot send message: "
				      "sender address not valid.");
		return FALSE;
	}
	
	/* find out if the message has 8bit mime parts */
	has_8bit_parts = camel_mime_message_has_8bit_parts (CAMEL_MIME_MESSAGE (message));
	
	/* rfc1652 (8BITMIME) requires that you notify the ESMTP daemon that
	   you'll be sending an 8bit mime message at "MAIL FROM:" time. */
	smtp_mail (smtp_transport, addr, has_8bit_parts, ex);
	
	if (!recipients) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Cannot send message: "
				      "no recipients defined.");
		return FALSE;
	}
	
	for (r = recipients; r; r = r->next) {
		recipient = (char *) r->data;
		if (!smtp_rcpt (smtp_transport, recipient, ex)) {
			g_free (recipient);
			return FALSE;
		}
		g_free (recipient);
	}
	
	/* passing in has_8bit_parts saves time as we don't have to
           recurse through the message all over again if the user is
           not sending 8bit mime parts */
	if (!smtp_data (smtp_transport, message, has_8bit_parts, ex))
		return FALSE;
	
	/* reset the service for our next transfer session */
	smtp_rset (smtp_transport, ex);
	
	return TRUE;
}

static gboolean
_send (CamelTransport *transport, CamelMedium *message, CamelException *ex)
{
	const CamelInternetAddress *to, *cc, *bcc;
	GList *recipients = NULL;
	guint index, len;
	
	to = camel_mime_message_get_recipients (CAMEL_MIME_MESSAGE (message), CAMEL_RECIPIENT_TYPE_TO);
	cc = camel_mime_message_get_recipients (CAMEL_MIME_MESSAGE (message), CAMEL_RECIPIENT_TYPE_CC);
	bcc = camel_mime_message_get_recipients (CAMEL_MIME_MESSAGE (message), CAMEL_RECIPIENT_TYPE_BCC);
	
	/* get all of the To addresses into our recipient list */
	len = CAMEL_ADDRESS (to)->addresses->len;
	for (index = 0; index < len; index++) {
		const char *addr;
		
		if (camel_internet_address_get (to, index, NULL, &addr))
			recipients = g_list_append (recipients, g_strdup (addr));
	}
	
	/* get all of the Cc addresses into our recipient list */
	len = CAMEL_ADDRESS (cc)->addresses->len;
	for (index = 0; index < len; index++) {
		const char *addr;
		
		if (camel_internet_address_get (cc, index, NULL, &addr))
			recipients = g_list_append (recipients, g_strdup (addr));
	}
	
	/* get all of the Bcc addresses into our recipient list */
	len = CAMEL_ADDRESS (bcc)->addresses->len;
	for (index = 0; index < len; index++) {
		const char *addr;
		
		if (camel_internet_address_get (bcc, index, NULL, &addr))
			recipients = g_list_append (recipients, g_strdup (addr));
	}
	
	return _send_to (transport, message, recipients, ex);
}

static gboolean
smtp_helo (CamelSmtpTransport *transport, CamelException *ex)
{
	/* say hello to the server */
	gchar *cmdbuf, *respbuf = NULL;
	struct hostent *host;
	
	/* get the local host name */
	host = gethostbyaddr ((gchar *)&transport->localaddr.sin_addr, sizeof (transport->localaddr.sin_addr), AF_INET);
	
	/* hiya server! how are you today? */
	if (transport->is_esmtp) {
		if (host && host->h_name)
			cmdbuf = g_strdup_printf ("EHLO %s\r\n", host->h_name);
		else
			cmdbuf = g_strdup_printf ("EHLO [%s]\r\n", inet_ntoa (transport->localaddr.sin_addr));
	} else {
		if (host && host->h_name)
			cmdbuf = g_strdup_printf ("HELO %s\r\n", host->h_name);
		else
			cmdbuf = g_strdup_printf ("HELO [%s]\r\n", inet_ntoa (transport->localaddr.sin_addr));
	}
	
	d(fprintf (stderr, "sending : %s", cmdbuf));
	if (camel_stream_write (transport->ostream, cmdbuf, strlen (cmdbuf)) == -1) {
		g_free (cmdbuf);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "HELO request timed out: "
				      "%s: non-fatal",
				      g_strerror (errno));
		return FALSE;
	}
	g_free (cmdbuf);
	
	do {
		/* Check for "250" */
		g_free (respbuf);
		respbuf = camel_stream_buffer_read_line (CAMEL_STREAM_BUFFER (transport->istream));
		
		d(fprintf (stderr, "received: %s\n", respbuf ? respbuf : "(null)"));
		
		if (!respbuf || strncmp (respbuf, "250", 3)) {
			g_free (respbuf);
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "HELO response error: "
					      "%s: non-fatal",
					      g_strerror (errno));
			return FALSE;
		}
		
		if (strstr (respbuf, "8BITMIME")) {
			d(fprintf (stderr, "This server supports 8bit\n"));
			CAMEL_TRANSPORT (transport)->supports_8bit = TRUE;
		}
		
		if (transport->is_esmtp && strstr (respbuf, "AUTH")) {
			/* parse for supported AUTH types */
			char *auths = strstr (respbuf, "AUTH") + 4;
			
			transport->esmtp_supported_authtypes = esmtp_get_authtypes (auths);
		}
	} while (*(respbuf+3) == '-'); /* if we got "250-" then loop again */
	g_free (respbuf);
	
	return TRUE;
}

static gboolean
smtp_mail (CamelSmtpTransport *transport, const char *sender, gboolean has_8bit_parts, CamelException *ex)
{
	/* we gotta tell the smtp server who we are. (our email addy) */
	gchar *cmdbuf, *respbuf = NULL;
	
	/* enclose address in <>'s since some SMTP daemons *require* that */
	if (CAMEL_TRANSPORT (transport)->supports_8bit && has_8bit_parts)
		cmdbuf = g_strdup_printf ("MAIL FROM: <%s> BODY=8BITMIME\r\n", sender);
	else
		cmdbuf = g_strdup_printf ("MAIL FROM: <%s>\r\n", sender);
	
	d(fprintf (stderr, "sending : %s", cmdbuf));
	
	if (camel_stream_write (transport->ostream, cmdbuf, strlen (cmdbuf)) == -1) {
		g_free (cmdbuf);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "MAIL FROM request timed out: "
				      "%s: mail not sent",
				      g_strerror (errno));
		return FALSE;
	}
	g_free (cmdbuf);
	
	do {
		/* Check for "250 Sender OK..." */
		g_free (respbuf);
		respbuf = camel_stream_buffer_read_line (CAMEL_STREAM_BUFFER (transport->istream));
		
		d(fprintf (stderr, "received: %s\n", respbuf ? respbuf : "(null)"));
		
		if (!respbuf || strncmp (respbuf, "250", 3)) {
			g_free (respbuf);
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "MAIL FROM response error: "
					      "%s: mail not sent",
					      g_strerror (errno));
			return FALSE;
		}
	} while (*(respbuf+3) == '-'); /* if we got "250-" then loop again */
	g_free (respbuf);
	
	return TRUE;
}

static gboolean
smtp_rcpt (CamelSmtpTransport *transport, const char *recipient, CamelException *ex)
{
	/* we gotta tell the smtp server who we are going to be sending
	 * our email to */
	gchar *cmdbuf, *respbuf = NULL;
	
	/* enclose address in <>'s since some SMTP daemons *require* that */
	cmdbuf = g_strdup_printf ("RCPT TO: <%s>\r\n", recipient);
	
	d(fprintf (stderr, "sending : %s", cmdbuf));
	
	if (camel_stream_write (transport->ostream, cmdbuf, strlen (cmdbuf)) == -1) {
		g_free (cmdbuf);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "RCPT TO request timed out: "
				      "%s: mail not sent",
				      g_strerror (errno));
		return FALSE;
	}
	g_free (cmdbuf);
	
	do {
		/* Check for "250 Sender OK..." */
		g_free (respbuf);
		respbuf = camel_stream_buffer_read_line (CAMEL_STREAM_BUFFER (transport->istream));
		
		d(fprintf (stderr, "received: %s\n", respbuf ? respbuf : "(null)"));
		
		if (!respbuf || strncmp (respbuf, "250", 3)) {
			g_free (respbuf);
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "RCPT TO response error: "
					      "%s: mail not sent",
					      g_strerror (errno));
			return FALSE;
		}
	} while (*(respbuf+3) == '-'); /* if we got "250-" then loop again */
	g_free (respbuf);
	
	return TRUE;
}

static gboolean
smtp_data (CamelSmtpTransport *transport, CamelMedium *message, gboolean has_8bit_parts, CamelException *ex)
{
	/* now we can actually send what's important :p */
	gchar *cmdbuf, *respbuf = NULL;
	CamelStreamFilter *filtered_stream;
	CamelMimeFilter *crlffilter, *lwfilter;
	gint crlfid, lwid;
	
	
	/* if the message contains 8bit mime parts and the server
           doesn't support it, encode 8bit parts to the best
           encoding. */
	if (has_8bit_parts && !CAMEL_TRANSPORT (transport)->supports_8bit)
		camel_mime_message_encode_8bit_parts (CAMEL_MIME_MESSAGE (message));
	
	cmdbuf = g_strdup ("DATA\r\n");
	
	d(fprintf (stderr, "sending : %s", cmdbuf));
	
	if (camel_stream_write (transport->ostream, cmdbuf, strlen (cmdbuf)) == -1) {
		g_free (cmdbuf);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "DATA request timed out: "
				      "%s: mail not sent",
				      g_strerror (errno));
		return FALSE;
	}
	g_free (cmdbuf);
	
	respbuf = camel_stream_buffer_read_line (CAMEL_STREAM_BUFFER (transport->istream));
	
	d(fprintf (stderr, "received: %s\n", respbuf ? respbuf : "(null)"));
	
	if (!respbuf || strncmp (respbuf, "354", 3)) {
		/* we should have gotten instructions on how to use the DATA command:
		 * 354 Enter mail, end with "." on a line by itself
		 */
		g_free (respbuf);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "DATA response error: "
				      "%s: mail not sent",
				      g_strerror (errno));
		return FALSE;
	}

	g_free (respbuf);
	respbuf = NULL;
	
	/* setup stream filtering */
	lwfilter = camel_mime_filter_linewrap_new (998, 998, '\t');
	crlffilter = camel_mime_filter_crlf_new (CAMEL_MIME_FILTER_CRLF_ENCODE, CAMEL_MIME_FILTER_CRLF_MODE_CRLF_DOTS);
	
	filtered_stream = camel_stream_filter_new_with_stream (transport->ostream);
	lwid = camel_stream_filter_add (filtered_stream, CAMEL_MIME_FILTER (lwfilter));
	crlfid = camel_stream_filter_add (filtered_stream, CAMEL_MIME_FILTER (crlffilter));
	
	if (camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), CAMEL_STREAM (filtered_stream)) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "DATA send timed out: message termination: "
				      "%s: mail not sent",
				      g_strerror (errno));
		return FALSE;
	}
	
	camel_stream_filter_remove (filtered_stream, lwid);
	camel_stream_filter_remove (filtered_stream, crlfid);
	camel_stream_flush (CAMEL_STREAM (filtered_stream));
	camel_object_unref (CAMEL_OBJECT (filtered_stream));
	
	/* terminate the message body */
	
	d(fprintf (stderr, "sending : \\r\\n.\\r\\n\n"));
	
	if (camel_stream_write (transport->ostream, "\r\n.\r\n", 5) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "DATA send timed out: message termination: "
				      "%s: mail not sent",
				      g_strerror (errno));
		return FALSE;
	}
	
	do {
		/* Check for "250 Sender OK..." */
		g_free (respbuf);
		respbuf = camel_stream_buffer_read_line (CAMEL_STREAM_BUFFER (transport->istream));
		
		d(fprintf (stderr, "received: %s\n", respbuf ? respbuf : "(null)"));
		
		if (!respbuf || strncmp (respbuf, "250", 3)) {
			g_free (respbuf);
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "DATA response error: message termination: "
					      "%s: mail not sent",
					      g_strerror (errno));
			return FALSE;
		}
	} while (*(respbuf+3) == '-'); /* if we got "250-" then loop again */
	g_free (respbuf);
	
	return TRUE;
}

static gboolean
smtp_rset (CamelSmtpTransport *transport, CamelException *ex)
{
	/* we are going to reset the smtp server (just to be nice) */
	gchar *cmdbuf, *respbuf = NULL;
	
	cmdbuf = g_strdup ("RSET\r\n");
	
	d(fprintf (stderr, "sending : %s", cmdbuf));
	
	if (camel_stream_write (transport->ostream, cmdbuf, strlen (cmdbuf)) == -1) {
		g_free (cmdbuf);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "RSET request timed out: "
				      "%s",
				      g_strerror (errno));
		return FALSE;
	}
	g_free (cmdbuf);
	
	do {
		/* Check for "250" */
		g_free (respbuf);
		respbuf = camel_stream_buffer_read_line (CAMEL_STREAM_BUFFER (transport->istream));
		
		d(fprintf (stderr, "received: %s\n", respbuf ? respbuf : "(null)"));
		
		if (!respbuf || strncmp (respbuf, "250", 3)) {
			g_free (respbuf);
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "RSET response error: %s",
					      g_strerror (errno));
			return FALSE;
		}
	} while (*(respbuf+3) == '-'); /* if we got "250-" then loop again */
	g_free (respbuf);
	
	return TRUE;
}

static gboolean
smtp_quit (CamelSmtpTransport *transport, CamelException *ex)
{
	/* we are going to reset the smtp server (just to be nice) */
	gchar *cmdbuf, *respbuf = NULL;
	
	cmdbuf = g_strdup ("QUIT\r\n");
	
	d(fprintf (stderr, "sending : %s", cmdbuf));
	
	if (camel_stream_write (transport->ostream, cmdbuf, strlen (cmdbuf)) == -1) {
		g_free (cmdbuf);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "QUIT request timed out: "
				      "%s: non-fatal",
				      g_strerror (errno));
		return FALSE;
	}
	g_free (cmdbuf);
	
	do {
		/* Check for "221" */
		g_free (respbuf);
		respbuf = camel_stream_buffer_read_line (CAMEL_STREAM_BUFFER (transport->istream));
		
		d(fprintf (stderr, "received: %s\n", respbuf ? respbuf : "(null)"));
		
		if (!respbuf || strncmp (respbuf, "221", 3)) {
			g_free (respbuf);
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "QUIT response error: "
					      "%s: non-fatal",
					      g_strerror (errno));
			return FALSE;
		}
	} while (*(respbuf+3) == '-'); /* if we got "221-" then loop again */
	g_free (respbuf);
	
	return TRUE;
}
