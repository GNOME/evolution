/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-pop3-store.c : class for a pop3 store */

/* 
 * Authors:
 *   Dan Winship <danw@helixcode.com>
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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "camel-pop3-store.h"
#include "camel-pop3-folder.h"
#include "camel-stream-buffer.h"
#include "camel-stream-fs.h"
#include "camel-session.h"
#include "camel-exception.h"
#include "camel-url.h"
#include "md5-utils.h"

/* Specified in RFC 1939 */
#define POP3_PORT 110

#ifdef HAVE_KRB4
/* Specified nowhere */
#define KPOP_PORT 1109

#include <krb.h>
#endif

static CamelServiceClass *service_class = NULL;

static void finalize (GtkObject *object);

static gboolean pop3_connect (CamelService *service, CamelException *ex);
static gboolean pop3_disconnect (CamelService *service, CamelException *ex);
static GList *query_auth_types (CamelService *service, CamelException *ex);
static void free_auth_types (CamelService *service, GList *authtypes);

static CamelFolder *get_folder (CamelStore *store, const char *folder_name, 
				gboolean create, CamelException *ex);
static char *get_folder_name (CamelStore *store, const char *folder_name, 
			      CamelException *ex);
static char *get_root_folder_name (CamelStore *store, CamelException *ex);


static void
camel_pop3_store_class_init (CamelPop3StoreClass *camel_pop3_store_class)
{
	GtkObjectClass *object_class =
		GTK_OBJECT_CLASS (camel_pop3_store_class);
	CamelServiceClass *camel_service_class =
		CAMEL_SERVICE_CLASS (camel_pop3_store_class);
	CamelStoreClass *camel_store_class =
		CAMEL_STORE_CLASS (camel_pop3_store_class);
	
	service_class = gtk_type_class (camel_service_get_type ());

	/* virtual method overload */
	object_class->finalize = finalize;

	camel_service_class->connect = pop3_connect;
	camel_service_class->disconnect = pop3_disconnect;
	camel_service_class->query_auth_types = query_auth_types;
	camel_service_class->free_auth_types = free_auth_types;

	camel_store_class->get_folder = get_folder;
	camel_store_class->get_folder_name = get_folder_name;
	camel_store_class->get_root_folder_name = get_root_folder_name;
}



static void
camel_pop3_store_init (gpointer object, gpointer klass)
{
	CamelService *service = CAMEL_SERVICE (object);

	service->url_flags = ( CAMEL_SERVICE_URL_NEED_USER |
			       CAMEL_SERVICE_URL_NEED_HOST );
}




GtkType
camel_pop3_store_get_type (void)
{
	static GtkType camel_pop3_store_type = 0;

	if (!camel_pop3_store_type) {
		GtkTypeInfo camel_pop3_store_info =	
		{
			"CamelPop3Store",
			sizeof (CamelPop3Store),
			sizeof (CamelPop3StoreClass),
			(GtkClassInitFunc) camel_pop3_store_class_init,
			(GtkObjectInitFunc) camel_pop3_store_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		camel_pop3_store_type = gtk_type_unique (CAMEL_STORE_TYPE, &camel_pop3_store_info);
	}

	return camel_pop3_store_type;
}

static void
finalize (GtkObject *object)
{
	CamelException ex;

	camel_exception_init (&ex);
	pop3_disconnect (CAMEL_SERVICE (object), &ex);
	camel_exception_clear (&ex);
}


static CamelServiceAuthType password_authtype = {
	"Password",

	"This option will connect to the POP server using a plaintext "
	"password. This is the only option supported by many POP servers.",

	"",
	TRUE
};

static CamelServiceAuthType apop_authtype = {
	"APOP",

	"This option will connect to the POP server using an encrypted "
	"password via the APOP protocol. This may not work for all users "
	"even on servers that claim to support it.",

	"+APOP",
	TRUE
};

#ifdef HAVE_KRB4
static CamelServiceAuthType kpop_authtype = {
	"Kerberos 4 (KPOP)",

	"This will connect to the POP server and use Kerberos 4 "
	"to authenticate to it.",

	"+KPOP",
	FALSE
};
#endif

static gboolean
connect_to_server (CamelService *service, gboolean real, CamelException *ex)
{
	CamelPop3Store *store = CAMEL_POP3_STORE (service);
	struct hostent *h;
	struct sockaddr_in sin;
	int fd;
	char *buf, *apoptime, *apopend;
#ifdef HAVE_KRB4
	gboolean kpop = (service->url->port == KPOP_PORT);
#endif

	h = camel_service_gethost (service, ex);
	if (!h)
		return FALSE;

	sin.sin_family = h->h_addrtype;
	if (service->url->port)
		sin.sin_port = htons (service->url->port);
	else
		sin.sin_port = htons (POP3_PORT);
	memcpy (&sin.sin_addr, h->h_addr, sizeof (sin.sin_addr));

	fd = socket (h->h_addrtype, SOCK_STREAM, 0);
	if (fd == -1 ||
	    connect (fd, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
		if (real) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					      "Could not connect to %s: %s",
					      h->h_name, g_strerror(errno));
		}
		if (fd > -1)
			close (fd);
		return FALSE;
	}

#ifdef HAVE_KRB4
	if (kpop) {
		KTEXT_ST ticket_st;
		MSG_DAT msg_data;
		CREDENTIALS cred;
		Key_schedule schedule;
		char *hostname;

		/* Need to copy hostname, because krb_realmofhost will
		 * call gethostbyname as well, and gethostbyname uses
		 * static storage.
		 */
		hostname = g_strdup (h->h_name);
		status = krb_sendauth (0, fd, &ticket_st, "pop", hostname,
				       krb_realmofhost (hostname), 0,
				       &msg_data, &cred, schedule,
				       NULL, NULL, "KPOPV0.1");
		g_free (hostname);
		if (status != KSUCCESS) {
			if (real) {
				camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
						      "Could not authenticate "
						      "to KPOP server: %s",
						      krb_err_txt[status]);
			}
			close (fd);
			return FALSE;
		}

		if (!service->url->passwd)
			service->url->passwd = g_strdup (service->url->user);
	}
#endif /* HAVE_KRB4 */

	store->ostream = camel_stream_fs_new_with_fd (fd);
	store->istream = camel_stream_buffer_new (store->ostream,
						  CAMEL_STREAM_BUFFER_READ);

	/* Read the greeting, note APOP timestamp, if any. */
	buf = camel_stream_buffer_read_line (CAMEL_STREAM_BUFFER (store->istream));
	if (!buf) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not read greeting from POP "
				      "server: %s",
				      camel_exception_get_description (ex));
		pop3_disconnect (service, ex);
		return FALSE;
	}
	apoptime = strchr (buf, '<');
	apopend = apoptime ? strchr (apoptime, '>') : NULL;
	if (apoptime && apopend) {
		store->apop_timestamp = g_strndup (apoptime,
						   apopend - apoptime + 1);
	}
	g_free (buf);

	return TRUE;
}

static GList *
query_auth_types (CamelService *service, CamelException *ex)
{
	CamelPop3Store *store = CAMEL_POP3_STORE (service);
	GList *ret = NULL;
	gboolean passwd = TRUE, apop = TRUE;
#ifdef HAVE_KRB4
	gboolean kpop = TRUE;
	int saved_port;
#endif

	if (service->url) {
		passwd = connect_to_server (service, FALSE, ex);
		if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE)
			return NULL;
		apop = store->apop_timestamp != NULL;
		if (passwd)
			pop3_disconnect (service, ex);
#ifdef HAVE_KRB4
		saved_port = service->url->port;
		service->url->port = KPOP_PORT;
		kpop = connect_to_server (service, FALSE, ex);
		service->url->port = saved_port;
		if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE)
			return NULL;
		if (kpop)
			pop3_disconnect (service, ex);
#endif
	}

	if (passwd)
		ret = g_list_append (ret, &password_authtype);
	if (apop)
		ret = g_list_append (ret, &apop_authtype);
#ifdef HAVE_KRB4
	if (kpop)
		ret = g_list_append (ret, &kpop_authtype);
#endif

	if (!ret) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not connect to POP server on "
				      "%s.", service->url->host);
	}				      

	return ret;
}

static void
free_auth_types (CamelService *service, GList *authtypes)
{
	g_list_free (authtypes);
}

/**
 * camel_pop3_store_open: Connect to the server if we are currently
 * disconnected.
 * @store: the store
 * @ex: a CamelException
 *
 * The POP protocol does not allow deleted messages to be expunged
 * except by closing the connection. Thus, camel_pop3_folder_{open,close}
 * sometimes need to connect to or disconnect from the server. This
 * routine reconnects to the server if we have disconnected.
 *
 **/
void
camel_pop3_store_open (CamelPop3Store *store, CamelException *ex)
{
	CamelService *service = CAMEL_SERVICE (store);

	if (!camel_service_is_connected (service))
		pop3_connect (service, ex);
}

/**
 * camel_pop3_store_close: Close the connection to the server and
 * possibly expunge deleted messages.
 * @store: the store
 * @expunge: whether or not to expunge deleted messages
 * @ex: a CamelException
 *
 * See camel_pop3_store_open for an explanation of why this is needed.
 *
 **/
void
camel_pop3_store_close (CamelPop3Store *store, gboolean expunge,
			CamelException *ex)
{
	if (expunge)
		camel_pop3_command (store, NULL, "QUIT");
	else
		camel_pop3_command (store, NULL, "RSET");
	pop3_disconnect (CAMEL_SERVICE (store), ex);
}

static gboolean
pop3_connect (CamelService *service, CamelException *ex)
{
	CamelPop3Store *store = CAMEL_POP3_STORE (service);
	int status;
	char *msg;
#ifdef HAVE_KRB4
	gboolean kpop = (service->url->authmech &&
			 !strcmp (service->url->authmech, "+KPOP"));

	if (kpop && service->url->port == 0)
		service->url->port = KPOP_PORT;
#endif

	if (!connect_to_server (service, TRUE, ex))
		return FALSE;

	/* The KPOP code will have set the password to be the username
	 * in connect_to_server. Password and APOP are the only other
	 * cases, and they both need a password.
	 */
	if (!service->url->passwd) {
		char *prompt = g_strdup_printf ("Please enter the POP3 password for %s@%s",
						service->url->user,
						service->url->host);
		service->url->passwd = camel_session_query_authenticator (
			camel_service_get_session (service),
			CAMEL_AUTHENTICATOR_ASK, prompt, TRUE,
			service, "password", ex);
		g_free (prompt);
		if (!service->url->passwd) {
			pop3_disconnect (service, ex);
			return FALSE;
		}
	}

	if (!service->url->authmech ||
	    !strcmp (service->url->authmech, "+KPOP")) {
		status = camel_pop3_command (store, &msg, "USER %s",
					     service->url->user);
		if (status != CAMEL_POP3_OK) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
					      "Unable to connect to POP "
					      "server. Error sending username:"
					      " %s", msg ? msg : "(Unknown)");
			g_free (msg);
			goto lose;
		}
		g_free (msg);

		status = camel_pop3_command (store, &msg, "PASS %s",
					     service->url->passwd);
	} else if (!strcmp (service->url->authmech, "+APOP")
		   && store->apop_timestamp) {
		char *secret, md5asc[33], *d;
		unsigned char md5sum[16], *s;

		secret = g_strdup_printf ("%s%s", store->apop_timestamp,
					  service->url->passwd);
		md5_get_digest (secret, strlen (secret), md5sum);
		g_free (secret);

		for (s = md5sum, d = md5asc; d < md5asc + 32; s++, d += 2)
			sprintf (d, "%.2x", *s);

		status = camel_pop3_command (store, &msg, "APOP %s %s",
					     service->url->user, md5asc);
	} else {
		camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
				     "No support for requested "
				     "authentication mechanism.");
		goto lose;
	}

	if (status != CAMEL_POP3_OK) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
				      "Unable to authenticate to POP "
				      "server. Error sending password:"
				      " %s", msg ? msg : "(Unknown)");
		g_free (msg);
		goto lose;
	}
	g_free (msg);

	service_class->connect (service, ex);
	return TRUE;

 lose:
	/* Uncache the password. */
	camel_session_query_authenticator (camel_service_get_session (service),
					   CAMEL_AUTHENTICATOR_TELL, NULL,
					   TRUE, service, "password", ex);

	pop3_disconnect (service, ex);
	return FALSE;
}

static gboolean
pop3_disconnect (CamelService *service, CamelException *ex)
{
	CamelPop3Store *store = CAMEL_POP3_STORE (service);

	if (!service_class->disconnect (service, ex))
		return FALSE;

	if (store->ostream) {
		gtk_object_unref (GTK_OBJECT (store->ostream));
		store->ostream = NULL;
	}
	if (store->istream) {
		gtk_object_unref (GTK_OBJECT (store->istream));
		store->istream = NULL;
	}

	if (store->apop_timestamp) {
		g_free (store->apop_timestamp);
		store->apop_timestamp = NULL;
	}

	return TRUE;
}

static CamelFolder *
get_folder (CamelStore *store, const char *folder_name,
	    gboolean create, CamelException *ex)
{
	return camel_pop3_folder_new (store, ex);
}

static char *
get_folder_name (CamelStore *store, const char *folder_name,
		 CamelException *ex)
{
	if (!g_strcasecmp (folder_name, "inbox"))
		return g_strdup ("inbox");
	else {
		camel_exception_setv (ex, CAMEL_EXCEPTION_FOLDER_INVALID,
				      "No such folder `%s'.", folder_name);
		return NULL;
	}
}

static char *
get_root_folder_name (CamelStore *store, CamelException *ex)
{
	return g_strdup ("inbox");
}


/**
 * camel_pop3_command: Send a command to a POP3 server.
 * @store: the POP3 store
 * @ret: a pointer to return the full server response in
 * @fmt: a printf-style format string, followed by arguments
 *
 * This command sends the command specified by @fmt and the following
 * arguments to the connected POP3 store specified by @store. It then
 * reads the server's response and parses out the status code. If
 * the caller passed a non-NULL pointer for @ret, camel_pop3_command
 * will set it to point to an buffer containing the rest of the
 * response from the POP3 server. (If @ret was passed but there was
 * no extended response, @ret will be set to NULL.) The caller must
 * free this buffer when it is done with it.
 *
 * Return value: one of CAMEL_POP3_OK (command executed successfully),
 * CAMEL_POP3_ERR (command encounted an error), or CAMEL_POP3_FAIL
 * (a protocol-level error occurred, and Camel is uncertain of the
 * result of the command.)
 **/
int
camel_pop3_command (CamelPop3Store *store, char **ret, char *fmt, ...)
{
	char *cmdbuf, *respbuf;
	va_list ap;
	int status;

	va_start (ap, fmt);
	cmdbuf = g_strdup_vprintf (fmt, ap);
	va_end (ap);

	/* Send the command */
	if (camel_stream_printf (store->ostream, "%s\r\n", cmdbuf) == -1) {
		g_free (cmdbuf);
		if (*ret)
			*ret = g_strdup(strerror(errno));
		return CAMEL_POP3_FAIL;
	}
	g_free (cmdbuf);

	/* Read the response */
	respbuf = camel_stream_buffer_read_line (CAMEL_STREAM_BUFFER (store->istream));
	if (respbuf == NULL) {
		if (*ret)
			*ret = g_strdup(strerror(errno));
		return CAMEL_POP3_FAIL;
	}
	if (!strncmp (respbuf, "+OK", 3))
		status = CAMEL_POP3_OK;
	else if (!strncmp (respbuf, "-ERR", 4))
		status = CAMEL_POP3_ERR;
	else
		status = CAMEL_POP3_FAIL;

	if (ret) {
		if (status != CAMEL_POP3_FAIL) {
			*ret = strchr (respbuf, ' ');
			if (*ret)
				*ret = g_strdup (*ret + 1);
		} else
			*ret = NULL;
	}
	g_free (respbuf);

	return status;
}

/**
 * camel_pop3_command_get_additional_data: get "additional data" from
 * a POP3 command.
 * @store: the POP3 store
 *
 * This command gets the additional data returned by "multi-line" POP
 * commands, such as LIST, RETR, TOP, and UIDL. This command _must_
 * be called after a successful (CAMEL_POP3_OK) call to
 * camel_pop3_command for a command that has a multi-line response.
 * The returned data is un-byte-stuffed, and has lines termined by
 * newlines rather than CR/LF pairs.
 *
 * Return value: the data, which the caller must free.
 **/
char *
camel_pop3_command_get_additional_data (CamelPop3Store *store,
					CamelException *ex)
{
	CamelStreamBuffer *stream = CAMEL_STREAM_BUFFER (store->istream);
	GPtrArray *data;
	char *buf;
	int i, status = CAMEL_POP3_OK;

	data = g_ptr_array_new ();
	while (1) {
		buf = camel_stream_buffer_read_line (stream);
		if (!buf) {
			status = CAMEL_POP3_FAIL;
			break;
		}

		if (!strcmp (buf, "."))
			break;
		if (*buf == '.')
			memmove (buf, buf + 1, strlen (buf));
		g_ptr_array_add (data, buf);
	}
	g_free (buf);

	if (status == CAMEL_POP3_OK) {
		/* Append an empty string to the end of the array
		 * so when we g_strjoinv it, we get a "\n" after
		 * the last real line.
		 */
		g_ptr_array_add (data, "");
		g_ptr_array_add (data, NULL);
		buf = g_strjoinv ("\n", (char **)data->pdata);
	} else
		buf = NULL;

	for (i = 0; i < data->len - 2; i++)
		g_free (data->pdata[i]);
	g_ptr_array_free (data, TRUE);

	return buf;
}
