/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-imap-store.c : class for an imap store */

/*
 * Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *          Ross Golder <ross@golder.org>
 *
 * Copyright (C) 2000 Helix Code, Inc.
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


#include <config.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>


#include "camel-imap-store.h"
#include "camel-imap-folder.h"
#include "camel-folder.h"
#include "camel-exception.h"
#include "camel-session.h"
#include "camel-stream.h"
#include "camel-stream-buffer.h"
#include "camel-stream-fs.h"
#include "camel-url.h"

/* Specified in RFC 2060 */
#define IMAP_PORT 143

static CamelServiceClass *service_class = NULL;

static void finalize (GtkObject *object);
static gboolean imap_create (CamelFolder *folder, CamelException *ex);
static gboolean imap_connect (CamelService *service, CamelException *ex);
static gboolean imap_disconnect (CamelService *service, CamelException *ex);
static GList *query_auth_types (CamelService *service, CamelException *ex);
static void free_auth_types (CamelService *service, GList *authtypes);

static CamelFolder *get_folder (CamelStore *store, const char *folder_name, gboolean create,
				CamelException *ex);
static char *get_folder_name (CamelStore *store, const char *folder_name, CamelException *ex);

static void
camel_imap_store_class_init (CamelImapStoreClass *camel_imap_store_class)
{
	/* virtual method overload */
	GtkObjectClass *object_class =
		GTK_OBJECT_CLASS (camel_imap_store_class);
	CamelServiceClass *camel_service_class =
		CAMEL_SERVICE_CLASS (camel_imap_store_class);
	CamelStoreClass *camel_store_class =
		CAMEL_STORE_CLASS (camel_imap_store_class);
	
	service_class = gtk_type_class (camel_service_get_type ());

	/* virtual method overload */
	object_class->finalize = finalize;

	camel_service_class->connect = imap_connect;
	camel_service_class->disconnect = imap_disconnect;
	camel_service_class->query_auth_types = query_auth_types;
	camel_service_class->free_auth_types = free_auth_types;

	camel_store_class->get_folder = get_folder;
	camel_store_class->get_folder_name = get_folder_name;
}


static void
camel_imap_store_init (gpointer object, gpointer klass)
{
	CamelService *service = CAMEL_SERVICE (object);
	CamelStore *store = CAMEL_STORE (object);

	service->url_flags = ( CAMEL_SERVICE_URL_NEED_USER |
			       CAMEL_SERVICE_URL_NEED_HOST );

	store->folders = g_hash_table_new (g_str_hash, g_str_equal);
}


GtkType
camel_imap_store_get_type (void)
{
	static GtkType camel_imap_store_type = 0;
	
	if (!camel_imap_store_type)	{
		GtkTypeInfo camel_imap_store_info =	
		{
			"CamelImapStore",
			sizeof (CamelImapStore),
			sizeof (CamelImapStoreClass),
			(GtkClassInitFunc) camel_imap_store_class_init,
			(GtkObjectInitFunc) camel_imap_store_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_imap_store_type = gtk_type_unique (CAMEL_STORE_TYPE, &camel_imap_store_info);
	}
	
	return camel_imap_store_type;
}

static void
finalize (GtkObject *object)
{
	CamelException ex;

	camel_exception_init (&ex);
	imap_disconnect (CAMEL_SERVICE (object), &ex);
	camel_exception_clear (&ex);
}

static CamelServiceAuthType password_authtype = {
	"Password",

	"This option will connect to the IMAP server using a "
	"plaintext password.",

	"",
	TRUE
};

static gboolean
try_connect (CamelService *service, CamelException *ex)
{
	struct hostent *h;
	struct sockaddr_in sin;
	gint fd;

	h = camel_service_gethost (service, ex);
	if (!h)
		return FALSE;

	sin.sin_family = h->h_addrtype;
	sin.sin_port = htons (service->url->port ? service->url->port : IMAP_PORT);
	memcpy (&sin.sin_addr, h->h_addr, sizeof (sin.sin_addr));

	fd = socket (h->h_addrtype, SOCK_STREAM, 0);
	if (fd == -1 || connect (fd, (struct sockaddr *)&sin, sizeof(sin)) == -1) {

		/* We don't want to set a CamelException here */

		if (fd > -1)
			close (fd);

		return FALSE;
	}

	close (fd);
	return TRUE;
}

static GList *
query_auth_types (CamelService *service, CamelException *ex)
{
	GList *ret = NULL;
	gboolean passwd = TRUE;


	if (service->url) {
		passwd = try_connect (service, ex);
		if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE)
			return NULL;
	}

	if (passwd)
		ret = g_list_append (ret, &password_authtype);

	if (!ret) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not connect to IMAP server on "
				      "%s.", service->url->host);
	}				      

	return ret;
}

static void
free_auth_types (CamelService *service, GList *authtypes)
{
	g_list_free (authtypes);
}

static gboolean
imap_connect (CamelService *service, CamelException *ex)
{
	struct hostent *h;
	struct sockaddr_in sin;
	gint fd, status;
	gchar *buf, *msg;
	CamelImapStore *store = CAMEL_IMAP_STORE (service);


	h = camel_service_gethost (service, ex);
	if (!h)
		return FALSE;

	if (!service->url->authmech && !service->url->passwd) {
		gchar *prompt = g_strdup_printf ("Please enter the IMAP password for %s@%s",
						service->url->user, h->h_name);
		service->url->passwd =
			camel_session_query_authenticator (camel_service_get_session (service),
							   prompt, TRUE,
							   service, "password",
							   ex);
		g_free (prompt);
		if (!service->url->passwd)
			return FALSE;
	}

	sin.sin_family = h->h_addrtype;
	if (service->url->port)
		sin.sin_port = htons(service->url->port);
	else
		sin.sin_port = htons(IMAP_PORT);

	memcpy (&sin.sin_addr, h->h_addr, sizeof (sin.sin_addr));

	fd = socket (h->h_addrtype, SOCK_STREAM, 0);
	if (fd == -1 || connect (fd, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not connect to %s (port %s): %s",
				      service->url->host, service->url->port,
				      strerror(errno));
		if (fd > -1)
			close (fd);

		return FALSE;
	}


	store->ostream = camel_stream_fs_new_with_fd (fd);
	store->istream = camel_stream_buffer_new (store->ostream,
						  CAMEL_STREAM_BUFFER_READ);
	store->command = 0;

	/* Read the greeting, if any. */
	buf = camel_stream_buffer_read_line (CAMEL_STREAM_BUFFER (store->istream));
	if (!buf) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not read greeting from IMAP "
				      "server: %s",
				      camel_exception_get_description (ex));
		gtk_object_unref (GTK_OBJECT (store->ostream));
		gtk_object_unref (GTK_OBJECT (store->istream));
		return FALSE;
	}
	g_free (buf);

	status = camel_imap_command(store, NULL, &msg, "LOGIN \"%s\" \"%s\"",
				    service->url->user,
				    service->url->passwd);

	if (status != CAMEL_IMAP_OK) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
				      "Unable to authenticate to IMAP "
				      "server. Error sending password:"
				      " %s", msg ? msg : "(Unknown)");
		g_free (msg);
		gtk_object_unref (GTK_OBJECT (store->ostream));
		gtk_object_unref (GTK_OBJECT (store->istream));
		return FALSE;
	}

	service_class->connect (service, ex);
	return TRUE;
}

static gboolean
imap_disconnect (CamelService *service, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (service);

	if (!service->connected)
		return TRUE;

	if (!service_class->disconnect (service, ex))
		return FALSE;

	gtk_object_unref (GTK_OBJECT (store->ostream));
	gtk_object_unref (GTK_OBJECT (store->istream));
	store->ostream = NULL;
	store->istream = NULL;

	return TRUE;
}

const gchar *
camel_imap_store_get_toplevel_dir (CamelImapStore *store)
{
	CamelURL *url = CAMEL_SERVICE (store)->url;

	g_assert(url != NULL);
	return url->path;
}

static gboolean
imap_create (CamelFolder *folder, CamelException *ex)
{
	gchar *result;
	gint status;

	g_return_val_if_fail (folder != NULL, FALSE);

	if (!(folder->full_name || folder->name)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_FOLDER_INVALID,
				     "invalid folder path. Use set_name ?");
		return FALSE;
	}
	
	if (camel_folder_get_subfolder(folder->parent_folder, folder->name, FALSE, ex))
		return TRUE;

        /* create the directory for the subfolder */
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), NULL,
					      &result, "CREATE %s", folder->full_name);
	
	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not CREATE %s on IMAP server %s: %s.",
				      folder->full_name, service->url->host,
				      status == CAMEL_IMAP_ERR ? result :
				      "Unknown error");
		g_free (result);
		return FALSE;
	}
	
	g_free(result);

	return TRUE;
}

static CamelFolder *
get_folder (CamelStore *store, const char *folder_name, gboolean create, CamelException *ex)
{
	CamelImapFolder *new_imap_folder;
	CamelFolder *new_folder;

	new_imap_folder = gtk_type_new (CAMEL_IMAP_FOLDER_TYPE);
	new_folder = CAMEL_FOLDER (new_imap_folder);
	CAMEL_FOLDER_CLASS (new_folder)->init (new_folder, store, NULL,
				     folder_name, '/', ex);

	if (imap_create (new_folder, ex))
		return new_folder;

	return NULL;
}

static gchar *
get_folder_name (CamelStore *store, const char *folder_name,
		 CamelException *ex)
{
	return g_strdup (folder_name);
}

/**
 * camel_imap_command: Send a command to a IMAP server.
 * @store: the IMAP store
 * @ret: a pointer to return the full server response in
 * @fmt: a printf-style format string, followed by arguments
 *
 * This command sends the command specified by @fmt and the following
 * arguments to the connected IMAP store specified by @store. It then
 * reads the server's response and parses out the status code. If
 * the caller passed a non-NULL pointer for @ret, camel_imap_command
 * will set it to point to an buffer containing the rest of the
 * response from the IMAP server. (If @ret was passed but there was
 * no extended response, @ret will be set to NULL.) The caller must
 * free this buffer when it is done with it.
 *
 * Return value: one of CAMEL_IMAP_OK (command executed successfully),
 * CAMEL_IMAP_ERR (command encounted an error), or CAMEL_IMAP_FAIL
 * (a protocol-level error occurred, and Camel is uncertain of the
 * result of the command.)
 **/
gint
camel_imap_command (CamelImapStore *store, CamelFolder *folder, char **ret, char *fmt, ...)
{
	gchar *cmdbuf, *respbuf;
	gchar *cmdid, *code;
	va_list ap;
	gint status;

	if (folder && store->current_folder != folder && strncmp(fmt, "SELECT", 6) &&
	    strncmp(fmt, "STATUS", 6) && strncmp(fmt, "CREATE", 5)) {
		/* We need to select the correct mailbox first */
		char *r;
		int s;

		s = camel_imap_command(store, folder, &r, "SELECT %s", folder->full_name);
		if (s != CAMEL_IMAP_OK) {
			*ret = r;
			return s;
		}

		store->current_folder = folder;
	}
	
	/* create the command */
	cmdid = g_strdup_printf("A%.5d", store->command++);
	va_start (ap, fmt);
	cmdbuf = g_strdup_vprintf (fmt, ap);
	va_end (ap);

	fprintf(stderr, "sending : %s %s\r\n", cmdid, cmdbuf);

	if (camel_stream_printf (store->ostream, "%s %s\r\n", cmdid, cmdbuf) == -1) {
		g_free(cmdbuf);
		g_free(cmdid);
		if (*ret)
			*ret = g_strdup(strerror(errno));
		return CAMEL_IMAP_FAIL;
	}
	g_free(cmdbuf);
	g_free(cmdid);

	/* Read the response */
	respbuf = camel_stream_buffer_read_line (CAMEL_STREAM_BUFFER (store->istream));
	if (respbuf == NULL) {
		if (*ret)
			*ret = g_strdup(strerror(errno));
		return CAMEL_IMAP_FAIL;
	}

	fprintf(stderr, "received: %s\n", respbuf ? respbuf : "(null)");

        if (respbuf) {
		code = strstr(respbuf, cmdid) + strlen(cmdid) + 1;
		if (!strncmp(code, "OK", 2))
			status = CAMEL_IMAP_OK;
		else if (!strncmp(code, "NO", 2))
			status = CAMEL_IMAP_ERR;
		else
			status = CAMEL_IMAP_FAIL;
	} else {
		status = CAMEL_IMAP_FAIL;
	}

	if (ret) {
		if (status != CAMEL_IMAP_FAIL) {
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
 * camel_imap_command_extended: Send a command to a IMAP server and get
 * a multi-line response.
 * @store: the IMAP store
 * @ret: a pointer to return the full server response in
 * @fmt: a printf-style format string, followed by arguments
 *
 * This command sends the command specified by @fmt and the following
 * arguments to the connected IMAP store specified by @store. It then
 * reads the server's response and parses out the status code.
 * Camel_imap_command_extended will set it to point to a buffer containing the
 * response from the IMAP server. (If @ret was passed but there was The caller
 * must free this buffer when it is done with it.
 *
 * This command gets the additional data returned by "multi-line" IMAP
 * commands, such as SELECT, LIST, LSUB, and various other commands.
 * The returned data is un-byte-stuffed, and has lines termined by
 * newlines rather than CR/LF pairs.
 *
 * Return value: one of CAMEL_IMAP_OK (command executed successfully),
 * CAMEL_IMAP_ERR (command encounted an error), or CAMEL_IMAP_FAIL
 * (a protocol-level error occurred, and Camel is uncertain of the
 * result of the command.)
 **/

gint
camel_imap_command_extended (CamelImapStore *store, CamelFolder *folder, char **ret, char *fmt, ...)
{
	CamelStreamBuffer *stream = CAMEL_STREAM_BUFFER (store->istream);
	GPtrArray *data;
	gchar *cmdid, *cmdbuf, *respbuf, *code;
	va_list app;
	gint i, status = CAMEL_IMAP_OK;

	if (folder && store->current_folder != folder && strncmp(fmt, "SELECT", 6) &&
	    strncmp(fmt, "STATUS", 6) && strncmp(fmt, "CREATE", 5)) {
		/* We need to select the correct mailbox first */
		char *r;
		int s;

		s = camel_imap_command(store, folder, &r, "SELECT %s", folder->full_name);
		if (s != CAMEL_IMAP_OK) {
			*ret = r;
			return s;
		}

		store->current_folder = folder;
	}
	
	/* Create the command */
	cmdid = g_strdup_printf("A%.5d", store->command++);
	va_start (app, fmt);
	cmdbuf = g_strdup_vprintf (fmt, app);
	va_end (app);

	fprintf(stderr, "sending : %s %s\r\n", cmdid, cmdbuf);

	if (camel_stream_printf (store->ostream, "%s %s\r\n", cmdid, cmdbuf) == -1) {
		g_free(cmdbuf);
		g_free(cmdid);

		*ret = g_strdup(strerror(errno));

		return CAMEL_IMAP_FAIL;
	}
	g_free(cmdbuf);
	g_free(cmdid);

	data = g_ptr_array_new ();
	while (1) {
		respbuf = camel_stream_buffer_read_line (stream);
		if (!respbuf || !strncmp(respbuf, cmdid, strlen(cmdid)) ) {
			/* IMAP's last response starts with our command id */
			break;
		}

		fprintf(stderr, "received: %s\n", respbuf);

		g_ptr_array_add (data, respbuf);
	}

	if (respbuf) {
		code = strstr(respbuf, cmdid) + strlen(cmdid) + 1;
		if (!strncmp(code, "OK", 2))
			status = CAMEL_IMAP_OK;
		else if (!strncmp(code, "NO", 2))
			status = CAMEL_IMAP_ERR;
		else
			status = CAMEL_IMAP_FAIL;

		g_free(respbuf);
	} else {
		status = CAMEL_IMAP_FAIL;
	}

	if (status == CAMEL_IMAP_OK) {
		/* Append an empty string to the end of the array
		 * so when we g_strjoinv it, we get a "\n" after
		 * the last real line.
		 */
		g_ptr_array_add (data, "");
		g_ptr_array_add (data, NULL);
		*ret = g_strjoinv ("\n", (gchar **)data->pdata);
	} else {
		if (status != CAMEL_IMAP_FAIL)
		        *ret = g_strdup (strchr (respbuf, ' ' + 1));
		else
			*ret = NULL;
	}

	for (i = 0; i < data->len - 2; i++)
		g_free (data->pdata[i]);
	g_ptr_array_free (data, TRUE);

	return status;
}


















