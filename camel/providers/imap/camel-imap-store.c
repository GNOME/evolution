/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-imap-store.c : class for an imap store */

/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
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
#include "string-utils.h"

#define d(x) x

/* Specified in RFC 2060 */
#define IMAP_PORT 143

static CamelServiceClass *service_class = NULL;

static void finalize (GtkObject *object);
static gboolean imap_create (CamelFolder *folder, CamelException *ex);
static gboolean imap_connect (CamelService *service, CamelException *ex);
static gboolean imap_disconnect (CamelService *service, CamelException *ex);
static GList *query_auth_types (CamelService *service, CamelException *ex);
static void free_auth_types (CamelService *service, GList *authtypes);
static char *get_name (CamelService *service, gboolean brief);
static CamelFolder *get_folder (CamelStore *store, const char *folder_name, gboolean create,
				CamelException *ex);
static char *get_folder_name (CamelStore *store, const char *folder_name, CamelException *ex);
static int camel_imap_status (char *cmdid, char *respbuf);

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
	camel_service_class->get_name = get_name;

	camel_store_class->get_folder = get_folder;
	camel_store_class->get_folder_name = get_folder_name;
}

static void
camel_imap_store_init (gpointer object, gpointer klass)
{
	CamelService *service = CAMEL_SERVICE (object);
	CamelStore *store = CAMEL_STORE (object);

	service->url_flags = (CAMEL_SERVICE_URL_NEED_USER | CAMEL_SERVICE_URL_NEED_HOST);

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
				      "%s.", service->url->host ? service->url->host : 
				      "(unknown host)");
	}				      

	return ret;
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
		return g_strdup_printf ("IMAP server %s", service->url->host);
	else {
		return g_strdup_printf ("IMAP service for %s on %s",
					service->url->user,
					service->url->host);
	}
}

static gboolean
imap_connect (CamelService *service, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (service);
	struct hostent *h;
	struct sockaddr_in sin;
	gint fd, status;
	gchar *buf, *msg, *result, *errbuf = NULL;
	gboolean authenticated = FALSE;


	h = camel_service_gethost (service, ex);
	if (!h)
		return FALSE;

	/* connect to the IMAP server */
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
				      service->url->host ? service->url->host : "(unknown host)", 
				      service->url->port ? service->url->port : "(unknown port)",
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
		
		imap_disconnect (service, ex);
		return FALSE;
	}
	g_free (buf);

	/* authenticate the user */
	while (!authenticated) {
		if (errbuf) {
			/* We need to un-cache the password before prompting again */
			camel_session_query_authenticator (camel_service_get_session (service),
							   CAMEL_AUTHENTICATOR_TELL, NULL,
							   TRUE, service, "password", ex);
			g_free (service->url->passwd);
			service->url->passwd = NULL;
		}

		if (!service->url->authmech && !service->url->passwd) {
			gchar *prompt;
			
			prompt = g_strdup_printf ("%sPlease enter the IMAP password for %s@%s",
						  errbuf ? errbuf : "", service->url->user, h->h_name);
			service->url->passwd =
				camel_session_query_authenticator (camel_service_get_session (service),
								   CAMEL_AUTHENTICATOR_ASK, prompt,
								   TRUE, service, "password", ex);
			g_free (prompt);
			g_free (errbuf);
			errbuf = NULL;
			
			if (!service->url->passwd) {
				imap_disconnect (service, ex);
				return FALSE;
			}
		}

		status = camel_imap_command (store, NULL, &msg, "LOGIN \"%s\" \"%s\"",
					     service->url->user,
					     service->url->passwd);

		if (status != CAMEL_IMAP_OK) {
			errbuf = g_strdup_printf ("Unable to authenticate to IMAP server.\n"
						  "Error sending password: %s\n\n",
						  msg ? msg : "(Unknown)");
		} else {
			g_message ("IMAP Service sucessfully authenticated user %s", service->url->user);
			authenticated = TRUE;
		}
	}
	
	/* Now lets find out the IMAP capabilities */
	status = camel_imap_command_extended (store, NULL, &result, "CAPABILITY");
	
	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (store);
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not get capabilities on IMAP server %s: %s.",
				      service->url->host, 
				      status == CAMEL_IMAP_ERR ? result :
				      "Unknown error");
	}
	
	if (strstrcase (result, "SEARCH"))
		store->has_search_capability = TRUE;
	else
		store->has_search_capability = FALSE;
	
	g_free (result);

	d(fprintf (stderr, "IMAP provider does%shave SEARCH support\n", store->has_search_capability ? " " : "n't "));

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

	g_assert (url != NULL);
	return url->path;
}

static gboolean
imap_folder_exists (CamelFolder *folder)
{
	CamelStore *store = CAMEL_STORE (folder->parent_store);
	CamelURL *url = CAMEL_SERVICE (store)->url;
	gchar *result, *folder_path;
	gint status;

	if (url && url->path && *(url->path + 1) && strcmp (folder->full_name, "INBOX"))
		folder_path = g_strdup_printf ("%s/%s", url->path + 1, folder->full_name);
	else
		folder_path = g_strdup (folder->full_name);

	d(fprintf (stderr, "doing an EXAMINE...\n"));
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), NULL,
					      &result, "EXAMINE %s", folder_path);

	if (status != CAMEL_IMAP_OK) {
		g_free (result);
		g_free (folder_path);
		return FALSE;
	}
	g_free (folder_path);
	g_free (result);

	return TRUE;
}

static gboolean
imap_create (CamelFolder *folder, CamelException *ex)
{
	CamelStore *store = CAMEL_STORE (folder->parent_store);
	CamelURL *url = CAMEL_SERVICE (store)->url;
	gchar *result, *folder_path;
	gint status;

	g_return_val_if_fail (folder != NULL, FALSE);
	
	if (!(folder->full_name || folder->name)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_FOLDER_INVALID,
				     "invalid folder path. Use set_name ?");
		return FALSE;
	}

	if (!strcmp (folder->full_name, "INBOX"))
		return TRUE;

	if (imap_folder_exists (folder))
		return TRUE;
	
        /* create the directory for the subfolder */
	if (url && url->path && *(url->path + 1) && strcmp (folder->full_name, "INBOX"))
		folder_path = g_strdup_printf ("%s/%s", url->path + 1, folder->full_name);
	else
		folder_path = g_strdup (folder->full_name);
	
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), NULL,
					      &result, "CREATE %s", folder_path);

	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not CREATE %s on IMAP server %s: %s.",
				      folder_path, service->url->host,
				      status == CAMEL_IMAP_ERR ? result :
				      "Unknown error");
		g_free (result);
		g_free (folder_path);
		return FALSE;
	}
	g_free (folder_path);
	g_free (result);

	return TRUE;
}

static CamelFolder *
get_folder (CamelStore *store, const char *folder_name, gboolean create, CamelException *ex)
{
	CamelFolder *new_folder;
	char *folder_path;

	g_return_val_if_fail (store != NULL, NULL);
	g_return_val_if_fail (folder_name != NULL, NULL);

	if (!strcmp (folder_name, "/"))
		folder_path = g_strdup ("INBOX");
	else
		folder_path = g_strdup (folder_name);
	
	new_folder = camel_imap_folder_new (store, folder_path, ex);

	if (create && !imap_create (new_folder, ex)) {
		return NULL;
	}
	
	return new_folder;
}

static gchar *
get_folder_name (CamelStore *store, const char *folder_name, CamelException *ex)
{
	return g_strdup (folder_name);
}

static int
camel_imap_status (char *cmdid, char *respbuf)
{
	char *retcode;

	if (respbuf) {
		retcode = strstr (respbuf, cmdid);
		if (retcode) {
			retcode += strlen (cmdid) + 1;
			
			if (!strncmp (retcode, "OK", 2))
				return CAMEL_IMAP_OK;
			else if (!strncmp (retcode, "NO", 2))
				return CAMEL_IMAP_ERR;
		}
	}
	
	return CAMEL_IMAP_FAIL;
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
	CamelURL *url = CAMEL_SERVICE (store)->url;
	gchar *cmdbuf, *respbuf;
	gchar *cmdid;
	va_list ap;
	gint status = CAMEL_IMAP_OK;

	if (folder && store->current_folder != folder && strncmp (fmt, "STATUS", 6) &&
	    strncmp (fmt, "CREATE", 5) && strcmp (fmt, "CAPABILITY")) {
		/* We need to select the correct mailbox first */
		char *r, *folder_path, *recent;
		int s;

		if (url && url->path && strcmp (folder->full_name, "INBOX"))
			folder_path = g_strdup_printf ("%s/%s", url->path + 1, folder->full_name);
		else
			folder_path = g_strdup (folder->full_name);
		
		s = camel_imap_command_extended (store, folder, &r, "SELECT %s", folder_path);
		g_free (folder_path);
		if (!r || s != CAMEL_IMAP_OK) {
			*ret = r;
			return s;
		} else {
			/* parse the read-write mode */
#if 0
			char *p;

			p = strstr (r, "\n");
			while (p) {
				if (*(p + 1) == '*')
					p = strstr (p, "\n");
				else
					break;
			}

			if (p) {
				if (strstrcase (p, "READ-WRITE"))
					mode = 
			}
#endif
		}
#if 0
		if ((recent = strstrcase (r, "RECENT"))) {
			char *p;
			
			for (p = recent; p > r && *p != '*'; p--);
			for ( ; *p && (*p < '0' || *p > '9') && p < recent; p++);

			if (atoi (p) > 0)
				gtk_signal_emit_by_name (GTK_OBJECT (folder), "folder_changed");
		}
#endif
		g_free (r);

		store->current_folder = folder;
	}
	
	/* create the command */
	cmdid = g_strdup_printf ("A%.5d", store->command++);
	va_start (ap, fmt);
	cmdbuf = g_strdup_vprintf (fmt, ap);
	va_end (ap);

	d(fprintf (stderr, "sending : %s %s\r\n", cmdid, cmdbuf));
	fflush (stderr);

	if (camel_stream_printf (store->ostream, "%s %s\r\n", cmdid, cmdbuf) == -1) {
		g_free (cmdbuf);
		g_free (cmdid);
		if (*ret)
			*ret = g_strdup (strerror (errno));
		return CAMEL_IMAP_FAIL;
	}
	g_free (cmdbuf);

	/* Read the response */
	respbuf = camel_stream_buffer_read_line (CAMEL_STREAM_BUFFER (store->istream));
	if (respbuf == NULL) {
		if (*ret)
			*ret = g_strdup (strerror (errno));
		return CAMEL_IMAP_FAIL;
	}

	d(fprintf (stderr, "received: %s\n", respbuf ? respbuf : "(null)"));

	status = camel_imap_status (cmdid, respbuf);
	g_free (cmdid);

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
	CamelURL *url = CAMEL_SERVICE (CAMEL_STORE (store))->url;
	CamelStreamBuffer *stream = CAMEL_STREAM_BUFFER (store->istream);
	GPtrArray *data;
	gchar *cmdid, *cmdbuf, *respbuf;
	va_list app;
	gint len = 0, status = CAMEL_IMAP_OK;

	if (folder && store->current_folder != folder && strncmp (fmt, "SELECT", 6) &&
	    strncmp (fmt, "EXAMINE", 7) && strncmp (fmt, "STATUS", 6) &&
	    strncmp (fmt, "CREATE", 6) && strcmp (fmt, "CAPABILITY")) {
		/* We need to select the correct mailbox first */
		char *r, *folder_path, *recent;
		int s;

		if (url && url->path && strcmp (folder->full_name, "INBOX"))
			folder_path = g_strdup_printf ("%s/%s", url->path + 1, folder->full_name);
		else
			folder_path = g_strdup (folder->full_name);
		
		s = camel_imap_command_extended (store, folder, &r, "SELECT %s", folder_path);
		g_free (folder_path);
		if (!r || s != CAMEL_IMAP_OK) {
			*ret = r;
			return s;
		}
#if 0
		if ((recent = strstrcase (r, "RECENT"))) {
			char *p;
			
			for (p = recent; p > r && *p != '*'; p--);
			for ( ; *p && (*p < '0' || *p > '9') && p < recent; p++);

			if (atoi (p) > 0)
				gtk_signal_emit_by_name (GTK_OBJECT (folder), "folder_changed");
		}
#endif	
		g_free (r);

		store->current_folder = folder;
	}
	
	/* Create the command */
	cmdid = g_strdup_printf ("A%.5d", store->command++);
	va_start (app, fmt);
	cmdbuf = g_strdup_vprintf (fmt, app);
	va_end (app);

	d(fprintf (stderr, "sending : %s %s\r\n", cmdid, cmdbuf));

	if (camel_stream_printf (store->ostream, "%s %s\r\n", cmdid, cmdbuf) == -1) {
		g_free (cmdbuf);
		g_free (cmdid);

		*ret = g_strdup (strerror (errno));

		return CAMEL_IMAP_FAIL;
	}
	g_free (cmdbuf);

	data = g_ptr_array_new ();

	while (1) {
		respbuf = camel_stream_buffer_read_line (stream);
		if (!respbuf || !strncmp(respbuf, cmdid, strlen(cmdid)) ) {	
			/* IMAP's last response starts with our command id */
			d(fprintf (stderr, "received: %s\n", respbuf ? respbuf : "(null)"));
			break;
		}

		d(fprintf (stderr, "received: %s\n", respbuf));

		g_ptr_array_add (data, respbuf);
		len += strlen (respbuf) + 1;
	}

	if (respbuf) {
		g_ptr_array_add (data, respbuf);
		len += strlen (respbuf) + 1;
		
		status = camel_imap_status (cmdid, respbuf);
	} else {
		status = CAMEL_IMAP_FAIL;
	}
	g_free (cmdid);

	if (status == CAMEL_IMAP_OK) {
		char *p;
		int i;
		
		*ret = g_malloc0 (len + 1);

		for (i = 0, p = *ret; i < data->len; i++) {
			char *ptr, *datap;

			datap = (char *) data->pdata[i];
			ptr = (*datap == '.') ? datap + 1 : datap;
			len = strlen (ptr);
			memcpy (p, ptr, len);
			p += len;
			*p++ = '\n';
		}
		*p = '\0';
	} else {
		if (status != CAMEL_IMAP_FAIL && respbuf)
		        *ret = g_strdup (strchr (respbuf, ' ' + 1));
		else
			*ret = NULL;
	}

	g_ptr_array_free (data, TRUE);
	
	return status;
}
