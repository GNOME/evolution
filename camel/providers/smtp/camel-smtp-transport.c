/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-smtp-transport.c : class for a smtp transport */

/* 
 * Authors:
 *   Jeffrey Stedfast <fejj@helixcode.com>
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
#include <unistd.h>
#undef MIN
#undef MAX
#include "camel-mime-filter-smtp.h"
#include "camel-stream-filter.h"
#include "camel-smtp-transport.h"
#include "camel-mime-message.h"
#include "camel-stream-buffer.h"
#include "camel-stream-fs.h"
#include "camel-session.h"
#include "camel-exception.h"
#include "md5-utils.h"

/* Specified in RFC 821 */
#define SMTP_PORT 25

/* camel smtp transport class prototypes */
static gboolean _can_send (CamelTransport *transport, CamelMedium *message);
static gboolean _send (CamelTransport *transport, CamelMedium *message, CamelException *ex);
static gboolean _send_to (CamelTransport *transport, CamelMedium *message, GList *recipients, CamelException *ex);

/* support prototypes */
static gboolean smtp_connect (CamelService *service, CamelException *ex);
static gboolean smtp_disconnect (CamelService *service, CamelException *ex);
static GList *esmtp_get_authtypes(gchar *buffer);
static GList *query_auth_types (CamelService *service, CamelException *ex);
static void free_auth_types (CamelService *service, GList *authtypes);
static gchar *smtp_get_email_addr_from_text (gchar *text);
static gboolean smtp_helo (CamelSmtpTransport *transport, CamelException *ex);
static gboolean smtp_mail (CamelSmtpTransport *transport, gchar *sender, CamelException *ex);
static gboolean smtp_rcpt (CamelSmtpTransport *transport, gchar *recipient, CamelException *ex);
static gboolean smtp_data (CamelSmtpTransport *transport, CamelMedium *message, CamelException *ex);
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
	
	service_class = gtk_type_class (camel_service_get_type ());

	/* virtual method overload */
	camel_service_class->connect = smtp_connect;
	camel_service_class->disconnect = smtp_disconnect;
	camel_service_class->query_auth_types = query_auth_types;
	camel_service_class->free_auth_types = free_auth_types;

	camel_transport_class->can_send = _can_send;
	camel_transport_class->send = _send;
	camel_transport_class->send_to = _send_to;
}

static void
camel_smtp_transport_init (gpointer object, gpointer klass)
{
	CamelService *service = CAMEL_SERVICE (object);

	service->url_flags = CAMEL_SERVICE_URL_NEED_HOST;
}

GtkType
camel_smtp_transport_get_type (void)
{
	static GtkType camel_smtp_transport_type = 0;

	if (!camel_smtp_transport_type) {
		GtkTypeInfo camel_smtp_transport_info =	
		{
			"CamelSmtpTransport",
			sizeof (CamelSmtpTransport),
			sizeof (CamelSmtpTransportClass),
			(GtkClassInitFunc) camel_smtp_transport_class_init,
			(GtkObjectInitFunc) camel_smtp_transport_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		camel_smtp_transport_type = gtk_type_unique (CAMEL_TRANSPORT_TYPE, &camel_smtp_transport_info);
	}

	return camel_smtp_transport_type;
}

static gboolean
smtp_connect (CamelService *service, CamelException *ex)
{
	struct hostent *h;
	struct sockaddr_in sin;
	gint fd, num, i;
	guint32 addrlen;
	gchar *pass = NULL, *respbuf = NULL;
	CamelSmtpTransport *transport = CAMEL_SMTP_TRANSPORT (service);

	if (!service_class->connect (service, ex))
		return FALSE;

	h = camel_service_gethost (service, ex);
	if (!h)
		return FALSE;

	/* set some smtp transport defaults */
	transport->smtp_is_esmtp = FALSE;
	transport->esmtp_supported_authtypes = NULL;

	sin.sin_family = h->h_addrtype;
	sin.sin_port = htons (service->url->port ? service->url->port : SMTP_PORT);
	memcpy (&sin.sin_addr, h->h_addr, sizeof (sin.sin_addr));

	fd = socket (h->h_addrtype, SOCK_STREAM, 0);
	if (fd == -1 || connect (fd, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not connect to %s (port %s): %s",
				      service->url->host, service->url->port,
				      strerror(errno));
		if (fd > -1)
			close (fd);
		g_free (pass);
		return FALSE;
	}

	/* get the localaddr - needed later by smtp_helo */
	addrlen = sizeof(transport->localaddr);
	getsockname(fd, (struct sockaddr*)&transport->localaddr, &addrlen);

	transport->ostream = camel_stream_fs_new_with_fd (fd);
	transport->istream = camel_stream_buffer_new (transport->ostream, 
						      CAMEL_STREAM_BUFFER_READ);

	/* Read the greeting, note whether the server is ESMTP and if it requests AUTH. */
	do {
		/* Check for "220" */
		g_free(respbuf);
		respbuf = camel_stream_buffer_read_line (CAMEL_STREAM_BUFFER (transport->istream));
		if ( !respbuf || strncmp(respbuf, "220", 3) ) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "Welcome response error: "
					      "%s: possibly non-fatal",
					      g_strerror (errno));
			return FALSE;
		}
		if (strstr(respbuf, "ESMTP"))
			transport->smtp_is_esmtp = TRUE;
	} while ( *(respbuf+3) == '-' ); /* if we got "220-" then loop again */
	g_free(respbuf);

	/* send HELO (or EHLO, depending on the service type) */
	smtp_helo(transport, ex);

	/* check to see if AUTH is required, if so...then AUTH ourselves */
	if (transport->smtp_is_esmtp && transport->esmtp_supported_authtypes) {
		/* not really supported yet, but we can at least show what auth types are supported */
		fprintf(stderr, "camel-smtp-transport::connect(): %s requires AUTH\n", service->url->host);
		num = g_list_length(transport->esmtp_supported_authtypes);

		for (i = 0; i < num; i++)
			fprintf(stderr, "\nSupported AUTH: %s\n\n", 
				(gchar *) g_list_nth_data(transport->esmtp_supported_authtypes, i));

		g_list_free(transport->esmtp_supported_authtypes);
		transport->esmtp_supported_authtypes = NULL;
	} else {
		fprintf(stderr, "\ncamel-smtp-transport::connect(): provider does not use AUTH\n\n");
	}

	return TRUE;
}

static gboolean
smtp_disconnect (CamelService *service, CamelException *ex)
{
	CamelSmtpTransport *transport = CAMEL_SMTP_TRANSPORT (service);

	if (!service->connected)
		return TRUE;

	/* send the QUIT command to the SMTP server */
	smtp_quit(transport, ex);

	if (!service_class->disconnect (service, ex))
		return FALSE;

	g_free(transport->esmtp_supported_authtypes);
	transport->esmtp_supported_authtypes = NULL;
	gtk_object_unref (GTK_OBJECT (transport->ostream));
	gtk_object_unref (GTK_OBJECT (transport->istream));
	transport->ostream = NULL;
	transport->istream = NULL;

	return TRUE;
}

static GList
*esmtp_get_authtypes(gchar *buffer)
{
	GList *ret = NULL;
	gchar *start, *end;

	if (!(start = strstr(buffer, " AUTH ")))
		return NULL;

	/* advance to the first token */
	for (start += 6; *start && *start != ' '; start++);

	for ( ; *start; ) {
		/* advance to the end of the token */
		for (end = start; *end && *end != ' '; end++);

		ret = g_list_append(ret, g_strndup(start, end - start));

		/* advance to the next token */
		for (start = end; *start && *start != ' '; start++);
	}

	return ret;
}

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

static GList
*query_auth_types (CamelService *service, CamelException *ex)
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

static gboolean
_can_send (CamelTransport *transport, CamelMedium *message)
{
	return CAMEL_IS_MIME_MESSAGE (message);
}

static gboolean
_send_to (CamelTransport *transport, CamelMedium *message,
	  GList *recipients, CamelException *ex)
{
	GList *r;
	gchar *recipient, *s, *sender;
	guint i, len;
	CamelSmtpTransport *smtp_transport = CAMEL_SMTP_TRANSPORT(transport);

	s = g_strdup(camel_mime_message_get_from (CAMEL_MIME_MESSAGE(message)));
	if (!s) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Cannot send message: "
				      "sender address not defined.");
		return FALSE;
	}

	sender = smtp_get_email_addr_from_text(s);
	smtp_mail(smtp_transport, sender, ex);
	g_free(sender);
	g_free(s);

	if (!(len = g_list_length(recipients))) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Cannot send message: "
				      "no recipients defined.");
		return FALSE;
	}
	for (i = 0, r = recipients; i < len; i++, r = r->next) {
		recipient = smtp_get_email_addr_from_text(r->data);
		if (!smtp_rcpt(smtp_transport, recipient, ex)) {
			g_free(recipient);
			return FALSE;
		}
		g_free(recipient);
	}

	if (!smtp_data(smtp_transport, message, ex))
		return FALSE;

	/* reset the service for our next transfer session */
	smtp_rset(smtp_transport, ex);

	return TRUE;
}

static gboolean
_send (CamelTransport *transport, CamelMedium *message,
       CamelException *ex)
{
	const CamelInternetAddress *to, *cc, *bcc;
	GList *recipients = NULL;
	guint index, len;

	to = camel_mime_message_get_recipients ((CamelMimeMessage *) message, CAMEL_RECIPIENT_TYPE_TO);
	cc = camel_mime_message_get_recipients ((CamelMimeMessage *) message, CAMEL_RECIPIENT_TYPE_CC);
	bcc = camel_mime_message_get_recipients ((CamelMimeMessage *) message, CAMEL_RECIPIENT_TYPE_BCC);

	/* get all of the To addresses into our recipient list */
	len = ((CamelAddress *)to)->addresses->len;
	for (index = 0; index < len; index++) {
		const char *addr;

		if (camel_internet_address_get(to, index, NULL, &addr))
			recipients = g_list_append(recipients, g_strdup(addr));
	}

	/* get all of the Cc addresses into our recipient list */
	len = ((CamelAddress *)cc)->addresses->len;
	for (index = 0; index < len; index++) {
		const char *addr;

		if (camel_internet_address_get(cc, index, NULL, &addr))
			recipients = g_list_append(recipients, g_strdup(addr));
	}

	/* get all of the Bcc addresses into our recipient list */
	len = ((CamelAddress *)bcc)->addresses->len;
	for (index = 0; index < len; index++) {
		const char *addr;

		if (camel_internet_address_get(bcc, index, NULL, &addr))
			recipients = g_list_append(recipients, g_strdup(addr));
	}

	return _send_to (transport, message, recipients, ex);
}

static gchar
*smtp_get_email_addr_from_text (gchar *text)
{
	/* get the actual email address from the string passed and place it in addr
	 * we can assume the address will be in one of the following forms:
	 * 1) The Name <person@host.com>
	 * 2) <person@host.com>
	 * 3) person@host.com
	 * 4) person@host.com (The Name)
	 */

	gchar *tmp, *addr = NULL;
	gchar *addr_strt;         /* points to start of addr */
	gchar *addr_end;          /* points to end of addr */
	gchar *ptr1;
   
   
	/* check the incoming args */
	if (!text || !*text)
		return NULL;

	/* scan the string for an open brace */
	for (addr_strt = text; *addr_strt; addr_strt++) 
		if (*addr_strt == '<')
			break;

	if (*addr_strt) {
		/* we found an open brace, let's look for it's counterpart */
		for (addr_end = addr_strt; *addr_end; addr_end++)
			if (*addr_end == '>') 
				break;

		/* if we didn't find it, or braces are empty... */
		if (!(*addr_end) || (addr_strt == addr_end - 1))
			return NULL;
			
		/* addr_strt points to '<' and addr_end points to '>'.
		 * Now let's adjust 'em slightly to point to the beginning
		 * and ending of the email addy
		 */
		addr_strt++;
		addr_end--;
	} else {
		/* no open brace...assume type 3 or 4? */
		addr_strt = text;
			
		/* find the end of the email addr/string */
		for (addr_end = addr_strt; *addr_end || *addr_end == ' '; addr_end++);
 
		addr_end--;       /* points to NULL, move it back one char */
	}

	/* now addr_strt & addr_end point to the beginning & ending of the email addy */

	/* copy the string into addr */
	addr = g_strndup(addr_strt, (gint)(addr_end - addr_strt + 1));

	for (ptr1 = addr_strt; ptr1 <= addr_end; ptr1++)    /* look for an '@' sign */
		if (*ptr1 == '@')
			break;

	if (*ptr1 != '@') {
		/* here we found out the name doesn't have an '@' part
		 * let's figure out what machine we're on & stick it on the end
		 */
		gchar hostname[MAXHOSTNAMELEN];

		if (gethostname(hostname, MAXHOSTNAMELEN)) {
			g_free(addr);
			return NULL;
		}
		tmp = addr;
		addr = g_strconcat(tmp, "@", hostname, NULL);
		g_free(tmp);
	}

	return addr;
}

static gboolean
smtp_helo (CamelSmtpTransport *transport, CamelException *ex)
{
	/* say hello to the server */
	gchar *cmdbuf, *respbuf = NULL;
	struct hostent *host;

	/* get the local host name */
	host = gethostbyaddr((gchar *)&transport->localaddr.sin_addr, sizeof(transport->localaddr.sin_addr), AF_INET);

	/* hiya server! how are you today? */
	if (transport->smtp_is_esmtp)
		cmdbuf = g_strdup_printf ("EHLO %s\r\n", host && host->h_name ? host->h_name : 
					  inet_ntoa(transport->localaddr.sin_addr));
	else
		cmdbuf = g_strdup_printf ("HELO %s\r\n", host && host->h_name ? host->h_name : 
					  inet_ntoa(transport->localaddr.sin_addr));

	fprintf(stderr, "sending : %s", cmdbuf);
	if ( camel_stream_write (transport->ostream, cmdbuf, strlen(cmdbuf)) == -1) {
		g_free(cmdbuf);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "HELO request timed out: "
				      "%s: non-fatal",
				      g_strerror (errno));
		return FALSE;
	}
	g_free(cmdbuf);

	do {
		/* Check for "250" */
		g_free(respbuf);
		respbuf = camel_stream_buffer_read_line (CAMEL_STREAM_BUFFER (transport->istream));
		if ( !respbuf || strncmp(respbuf, "250", 3) ) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "HELO response error: "
					      "%s: non-fatal",
					      g_strerror (errno));
			return FALSE;
		}

		fprintf(stderr, "received: %s\n", respbuf);

		if (transport->smtp_is_esmtp && strstr(respbuf, "AUTH")) {
			/* parse for supported AUTH types */
			g_strchomp(respbuf);
			transport->esmtp_supported_authtypes = esmtp_get_authtypes(respbuf);
		}
	} while ( *(respbuf+3) == '-' ); /* if we got "250-" then loop again */
	g_free(respbuf);

	return TRUE;
}

static gboolean
smtp_mail (CamelSmtpTransport *transport, gchar *sender, CamelException *ex)
{
	/* we gotta tell the smtp server who we are. (our email addy) */
	gchar *cmdbuf, *respbuf = NULL;

	/* enclose address in <>'s since some SMTP daemons *require* that */
	cmdbuf = g_strdup_printf("MAIL FROM: <%s>\r\n", sender);

	fprintf(stderr, "sending : %s", cmdbuf);

	if ( camel_stream_write (transport->ostream, cmdbuf, strlen(cmdbuf)) == -1) {
		g_free(cmdbuf);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "MAIL FROM request timed out: "
				      "%s: mail not sent",
				      g_strerror (errno));
		return FALSE;
	}
	g_free(cmdbuf);

	do {
		/* Check for "250 Sender OK..." */
		g_free(respbuf);
		respbuf = camel_stream_buffer_read_line (CAMEL_STREAM_BUFFER (transport->istream));
		if ( !respbuf || strncmp(respbuf, "250", 3) ) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "MAIL FROM response error: "
					      "%s: mail not sent",
					      g_strerror (errno));
			return FALSE;
		}

		fprintf(stderr, "received: %s\n", respbuf);

	} while ( *(respbuf+3) == '-' ); /* if we got "250-" then loop again */
	g_free(respbuf);

	return TRUE;
}

static gboolean
smtp_rcpt (CamelSmtpTransport *transport, gchar *recipient, CamelException *ex)
{
	/* we gotta tell the smtp server who we are going to be sending
	 * our email to */
	gchar *cmdbuf, *respbuf = NULL;

	/* enclose address in <>'s since some SMTP daemons *require* that */
	cmdbuf = g_strdup_printf("RCPT TO: <%s>\r\n", recipient);

	fprintf(stderr, "sending : %s", cmdbuf);

	if ( camel_stream_write (transport->ostream, cmdbuf, strlen(cmdbuf)) == -1) {
		g_free(cmdbuf);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "RCPT TO request timed out: "
				      "%s: mail not sent",
				      g_strerror (errno));
		return FALSE;
	}
	g_free(cmdbuf);

	do {
		/* Check for "250 Sender OK..." */
		g_free(respbuf);
		respbuf = camel_stream_buffer_read_line (CAMEL_STREAM_BUFFER (transport->istream));
		if ( !respbuf || strncmp(respbuf, "250", 3) ) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "RCPT TO response error: "
					      "%s: mail not sent",
					      g_strerror (errno));
			return FALSE;
		}

		fprintf(stderr, "received: %s\n", respbuf);

	} while ( *(respbuf+3) == '-' ); /* if we got "250-" then loop again */
	g_free(respbuf);
  
	return TRUE;
}

static gboolean
smtp_data (CamelSmtpTransport *transport, CamelMedium *message, CamelException *ex)
{
	/* now we can actually send what's important :p */
	gchar *cmdbuf, *respbuf = NULL;
	CamelStreamFilter *filtered_stream;
	CamelMimeFilterSmtp *mimefilter;
	gint id;

	/* enclose address in <>'s since some SMTP daemons *require* that */
	cmdbuf = g_strdup("DATA\r\n");

	fprintf(stderr, "sending : %s", cmdbuf);

	if ( camel_stream_write (transport->ostream, cmdbuf, strlen(cmdbuf)) == -1) {
		g_free(cmdbuf);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "DATA request timed out: "
				      "%s: mail not sent",
				      g_strerror (errno));
		return FALSE;
	}
	g_free(cmdbuf);

	respbuf = camel_stream_buffer_read_line (CAMEL_STREAM_BUFFER (transport->istream));
	if ( !respbuf || strncmp(respbuf, "354", 3) ) {
		/* we should have gotten instructions on how to use the DATA command:
		 * 354 Enter mail, end with "." on a line by itself
		 */
		g_free(respbuf);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "DATA response error: "
				      "%s: mail not sent",
				      g_strerror (errno));
		return FALSE;
	}

	fprintf(stderr, "received: %s\n", respbuf);
	
	/* setup stream filtering */
	mimefilter = camel_mime_filter_smtp_new();
        filtered_stream = camel_stream_filter_new_with_stream(transport->ostream);
	id = camel_stream_filter_add(filtered_stream, CAMEL_MIME_FILTER(mimefilter));

	if (camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER(message), CAMEL_STREAM(filtered_stream)) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "DATA send timed out: message termination: "
				      "%s: mail not sent",
				      g_strerror (errno));
		return FALSE;
	}
        
	camel_stream_filter_remove(filtered_stream, id);
	camel_stream_close(CAMEL_STREAM(filtered_stream));
	gtk_object_unref(GTK_OBJECT(filtered_stream));

	/* terminate the message body */

	fprintf(stderr, "sending : \\r\\n.\\r\\n\n");

	if ( camel_stream_write (transport->ostream, "\r\n.\r\n", 5) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "DATA send timed out: message termination: "
				      "%s: mail not sent",
				      g_strerror (errno));
		return FALSE;
	}

	do {
		/* Check for "250 Sender OK..." */
		g_free(respbuf);
		respbuf = camel_stream_buffer_read_line (CAMEL_STREAM_BUFFER (transport->istream));
		if ( !respbuf || strncmp(respbuf, "250", 3) ) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "DATA response error: message termination: "
					      "%s: mail not sent",
					      g_strerror (errno));
			return FALSE;
		}

		fprintf(stderr, "received: %s\n", respbuf);

	} while ( *(respbuf+3) == '-' ); /* if we got "250-" then loop again */
	g_free(respbuf);

	return TRUE;
}

static gboolean
smtp_rset (CamelSmtpTransport *transport, CamelException *ex)
{
	/* we are going to reset the smtp server (just to be nice) */
	gchar *cmdbuf, *respbuf = NULL;

	cmdbuf = g_strdup ("RSET\r\n");

	fprintf(stderr, "sending : %s", cmdbuf);

	if ( camel_stream_write (transport->ostream, cmdbuf, strlen(cmdbuf)) == -1) {
		g_free(cmdbuf);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "RSET request timed out: "
				      "%s",
				      g_strerror (errno));
		return FALSE;
	}
	g_free(cmdbuf);

	do {
		/* Check for "250" */
		g_free(respbuf);
		respbuf = camel_stream_buffer_read_line (CAMEL_STREAM_BUFFER (transport->istream));
		if ( !respbuf || strncmp(respbuf, "250", 3) ) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "RSET response error: "
					      "%s",
					      g_strerror (errno));
			return FALSE;
		}

		fprintf(stderr, "received: %s\n", respbuf);

	} while ( *(respbuf+3) == '-' ); /* if we got "250-" then loop again */
	g_free(respbuf);

	return TRUE;
}

static gboolean
smtp_quit (CamelSmtpTransport *transport, CamelException *ex)
{
	/* we are going to reset the smtp server (just to be nice) */
	gchar *cmdbuf, *respbuf = NULL;

	cmdbuf = g_strdup ("QUIT\r\n");

	fprintf(stderr, "sending : %s", cmdbuf);

	if ( camel_stream_write (transport->ostream, cmdbuf, strlen(cmdbuf)) == -1) {
		g_free(cmdbuf);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "QUIT request timed out: "
				      "%s: non-fatal",
				      g_strerror (errno));
		return FALSE;
	}
	g_free(cmdbuf);

	do {
		/* Check for "221" */
		g_free(respbuf);
		respbuf = camel_stream_buffer_read_line (CAMEL_STREAM_BUFFER (transport->istream));
		if ( !respbuf || strncmp(respbuf, "221", 3) ) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "QUIT response error: "
					      "%s: non-fatal",
					      g_strerror (errno));
			return FALSE;
		}

		fprintf(stderr, "received: %s\n", respbuf);

	} while ( *(respbuf+3) == '-' ); /* if we got "221-" then loop again */
	g_free(respbuf);

	return TRUE;
}
