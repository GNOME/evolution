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

#include <e-util/e-util.h>

#include "camel-imap-store.h"
#include "camel-imap-folder.h"
#include "camel-imap-utils.h"
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
static gboolean imap_noop (gpointer data);
/*static gboolean stream_is_alive (CamelStream *istream);*/
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

	service->url_flags = (CAMEL_SERVICE_URL_NEED_USER |
			      CAMEL_SERVICE_URL_NEED_HOST |
			      CAMEL_SERVICE_URL_ALLOW_PATH);

	store->folders = g_hash_table_new (g_str_hash, g_str_equal);
	CAMEL_IMAP_STORE (store)->dir_sep = NULL;
	CAMEL_IMAP_STORE (store)->current_folder = NULL;
	CAMEL_IMAP_STORE (store)->timeout_id = 0;
}

GtkType
camel_imap_store_get_type (void)
{
	static GtkType camel_imap_store_type = 0;
	
	if (!camel_imap_store_type) {
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
	if (fd == -1 || connect (fd, (struct sockaddr *)&sin, sizeof (sin)) == -1) {

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

	/* FIXME: do we really need this here? */
	if (store->timeout_id) {
		gtk_timeout_remove (store->timeout_id);
		store->timeout_id = 0;
	}
	
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
				      "Could not connect to %s (port %d): %s",
				      service->url->host ? service->url->host : "(unknown host)", 
				      service->url->port ? service->url->port : IMAP_PORT,
				      strerror(errno));
		if (fd > -1)
			close (fd);
		
		return FALSE;
	}

	/* parent class conect initialization */
	service_class->connect (service, ex);
	
	store->ostream = camel_stream_fs_new_with_fd (fd);
	store->istream = camel_stream_buffer_new (store->ostream, CAMEL_STREAM_BUFFER_READ);
	store->command = 0;
	g_free (store->dir_sep);
	store->dir_sep = g_strdup ("/");  /* default dir sep */
	
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
		/* Non-fatal error, but we should still warn the user... */
		CamelService *service = CAMEL_SERVICE (store);
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not get capabilities on IMAP server %s: %s.",
				      service->url->host, 
				      status != CAMEL_IMAP_FAIL && result ? result :
				      "Unknown error");
	}

	/* FIXME: parse for capabilities here. */
	d(fprintf (stderr, "%s\n", result));

	if (e_strstrcase (result, "IMAP4REV1"))
		store->server_level = IMAP_LEVEL_IMAP4REV1;
	else if (e_strstrcase (result, "IMAP4"))
		store->server_level = IMAP_LEVEL_IMAP4;
	else
		store->server_level = IMAP_LEVEL_UNKNOWN;
	
	if ((store->server_level >= IMAP_LEVEL_IMAP4REV1) || (e_strstrcase (result, "STATUS")))
		store->has_status_capability = TRUE;
	else
		store->has_status_capability = FALSE;
	
	g_free (result);

	/* We now need to find out which directory separator this daemon uses */
	status = camel_imap_command_extended (store, NULL, &result, "LIST \"\" \"\"");
	
	if (status != CAMEL_IMAP_OK) {
		/* Again, this is non-fatal */
		CamelService *service = CAMEL_SERVICE (store);
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not get directory separator on IMAP server %s: %s.",
				      service->url->host, 
				      status != CAMEL_IMAP_FAIL && result ? result :
				      "Unknown error");
	} else {
		char *flags, *sep, *folder;
		
		if (imap_parse_list_response (result, "", &flags, &sep, &folder)) {
			if (*sep) {
				g_free (store->dir_sep);
				store->dir_sep = g_strdup (sep);
			}
		}
		
		g_free (flags);
		g_free (sep);
		g_free (folder);
	}

	g_free (result);

	/* Lets add a timeout so that we can hopefully prevent getting disconnected */
	store->timeout_id = gtk_timeout_add (600000, imap_noop, store);
	
	return TRUE;
}

static gboolean
imap_disconnect (CamelService *service, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (service);
	char *result;
	int status;

	if (!service->connected)
		return TRUE;

	/* send the logout command */
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (service), NULL, &result, "LOGOUT");
	if (status != CAMEL_IMAP_OK) {
		/* Oh fuck it, we're disconnecting anyway... */
	}
	g_free (result);
	
	if (!service_class->disconnect (service, ex))
		return FALSE;

	if (store->istream) {
		gtk_object_unref (GTK_OBJECT (store->istream));
		store->istream = NULL;
	}

	if (store->ostream) {
		gtk_object_unref (GTK_OBJECT (store->ostream));
		store->ostream = NULL;
	}

	g_free (store->dir_sep);
	store->dir_sep = NULL;

	store->current_folder = NULL;

	if (store->timeout_id) {
		gtk_timeout_remove (store->timeout_id);
		store->timeout_id = 0;
	}
	
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
	gchar *result, *folder_path, *dir_sep;
	gint status;

	dir_sep = CAMEL_IMAP_STORE (folder->parent_store)->dir_sep;
	
	if (url && url->path && *(url->path + 1) && strcmp (folder->full_name, "INBOX"))
		folder_path = g_strdup_printf ("%s%s%s", url->path + 1, dir_sep, folder->full_name);
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
	gchar *result, *folder_path, *dir_sep;
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
	dir_sep = CAMEL_IMAP_STORE (folder->parent_store)->dir_sep;
	
	if (url && url->path && *(url->path + 1) && strcmp (folder->full_name, "INBOX"))
		folder_path = g_strdup_printf ("%s%s%s", url->path + 1, dir_sep, folder->full_name);
	else
		folder_path = g_strdup (folder->full_name);
	
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), NULL,
					      &result, "CREATE %s", folder_path);

	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not CREATE %s on IMAP server %s: %s.",
				      folder_path, service->url->host,
				      status != CAMEL_IMAP_FAIL && result ? result :
				      "Unknown error");
		g_free (result);
		g_free (folder_path);
		return FALSE;
	}
	g_free (folder_path);
	g_free (result);

	return TRUE;
}

static gboolean
folder_is_selectable (CamelStore *store, const char *folder_path)
{
	char *result, *flags, *sep, *folder;
	int status;
	
	if (!strcmp (folder_path, "INBOX"))
		return TRUE;
	
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (store), NULL,
					      &result, "LIST \"\" %s", folder_path);
	if (status != CAMEL_IMAP_OK) {
		g_free (result);
		return FALSE;
	}
	
	if (imap_parse_list_response (result, "", &flags, &sep, &folder)) {
		gboolean retval;
		
		retval = !e_strstrcase (flags, "NoSelect");
		g_free (flags);
		g_free (sep);
		g_free (folder);
		
		return retval;
	}
	g_free (flags);
	g_free (sep);
	g_free (folder);
	
	return FALSE;
}

static CamelFolder *
get_folder (CamelStore *store, const char *folder_name, gboolean create, CamelException *ex)
{
	CamelURL *url = CAMEL_SERVICE (store)->url;
	CamelFolder *new_folder;
	char *folder_path, *dir_sep;
	
	g_return_val_if_fail (store != NULL, NULL);
	g_return_val_if_fail (folder_name != NULL, NULL);
	
	dir_sep = CAMEL_IMAP_STORE (store)->dir_sep;

	if (!strcmp (folder_name, dir_sep))
		folder_path = g_strdup (url->path + 1);
	else
		folder_path = g_strdup (folder_name);

	new_folder = camel_imap_folder_new (store, folder_path, ex);

	if (!strcmp (folder_name, dir_sep))
		return new_folder;

	if (create && !imap_create (new_folder, ex)) {
		if (!folder_is_selectable (store, folder_path)) {
			camel_exception_clear (ex);
			new_folder->can_hold_messages = FALSE;
			return new_folder;
		} else {
			g_free (folder_path);
			gtk_object_unref (GTK_OBJECT (new_folder));		
			return NULL;
		}
	}
	
	return new_folder;
}

static gchar *
get_folder_name (CamelStore *store, const char *folder_name, CamelException *ex)
{
	return g_strdup (folder_name);
}

static gboolean
imap_noop (gpointer data)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (data);
	char *result;
	int status;

	status = camel_imap_command_extended (store, store->current_folder, &result, "NOOP");

	g_free (result);

	return TRUE;
}

#if 0
static gboolean
stream_is_alive (CamelStream *istream)
{
	CamelStreamFs *fs_stream;
	char buf;
	
	g_return_val_if_fail (istream != NULL, FALSE);
	
	fs_stream = CAMEL_STREAM_FS (CAMEL_STREAM_BUFFER (istream)->stream);
	g_return_val_if_fail (fs_stream->fd != -1, FALSE);
	
	if (read (fs_stream->fd, (void *) &buf, 0) == 0)
		return TRUE;
	
	return FALSE;
}
#endif

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
				return CAMEL_IMAP_NO;
			else if (!strncmp (retcode, "BAD", 3))
				return CAMEL_IMAP_BAD;
		}
	}
	
	return CAMEL_IMAP_FAIL;
}

/**
 * camel_imap_command: Send a command to a IMAP server.
 * @store: the IMAP store
 * @folder: The folder to perform the operation in
 * @ret: a pointer to return the full server response in
 * @fmt: a printf-style format string, followed by arguments
 * 
 * This camel method sends the command specified by @fmt and the following
 * arguments to the connected IMAP store specified by @store. It then
 * reads the server's response and parses out the status code. If
 * the caller passed a non-NULL pointer for @ret, camel_imap_command
 * will set it to point to a buffer containing the rest of the
 * response from the IMAP server. (If @ret was passed but there was
 * no extended response, @ret will be set to NULL.) The caller function is
 * responsible for freeing @ret.
 * 
 * Return value: one of CAMEL_IMAP_OK (command executed successfully),
 * CAMEL_IMAP_NO (operational error message), CAMEL_IMAP_BAD (error
 * message from the server), or CAMEL_IMAP_FAIL (a protocol-level error
 * occurred, and Camel is uncertain of the result of the command.)
 **/
gint
camel_imap_command (CamelImapStore *store, CamelFolder *folder, char **ret, char *fmt, ...)
{
	CamelURL *url = CAMEL_SERVICE (store)->url;
	gchar *cmdbuf, *respbuf;
	gchar *cmdid;
	va_list ap;
	gint status = CAMEL_IMAP_OK;
	
	if (folder && store->current_folder != folder && strncmp (fmt, "CREATE", 5)) {
		/* We need to select the correct mailbox first */
		char *r, *folder_path, *dir_sep;
		int s;
		
		dir_sep = store->dir_sep;
		if (url && url->path && *(url->path + 1) && strcmp (folder->full_name, "INBOX"))
			folder_path = g_strdup_printf ("%s%s%s", url->path + 1, dir_sep, folder->full_name);
		else
			folder_path = g_strdup (folder->full_name);
		
		s = camel_imap_command_extended (store, NULL, &r, "SELECT %s", folder_path);
		g_free (folder_path);
		if (!r || s != CAMEL_IMAP_OK) {
			*ret = r;
			store->current_folder = NULL;
			
			return s;
		}
		
		g_free (r);
		
		store->current_folder = folder;
	}
	
	/* create the command */
	cmdid = g_strdup_printf ("A%.5d", store->command++);
	va_start (ap, fmt);
	cmdbuf = g_strdup_vprintf (fmt, ap);
	va_end (ap);
	
	d(fprintf (stderr, "sending : %s %s\r\n", cmdid, cmdbuf));
	
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
 * @folder: The folder to perform the operation in
 * @ret: a pointer to return the full server response in
 * @fmt: a printf-style format string, followed by arguments
 *
 * This camel method sends the IMAP command specified by @fmt and the
 * following arguments to the IMAP store specified by @store. If the
 * store is in a disconnected state, camel_imap_command_extended will first
 * re-connect the store before sending the specified IMAP command. It then
 * reads the server's response and parses out the status code. If the caller
 * passed a non-NULL pointer for @ret, camel_imap_command_extended will set
 * it to point to a buffer containing the rest of the response from the IMAP
 * server. (If @ret was passed but there was no extended response, @ret will
 * be set to NULL.) The caller function is responsible for freeing @ret.
 * 
 * This camel method gets the additional data returned by "multi-line" IMAP
 * commands, such as SELECT, LIST, FETCH, and various other commands.
 * The returned data is un-byte-stuffed, and has lines termined by
 * newlines rather than CR/LF pairs.
 * 
 * Return value: one of CAMEL_IMAP_OK (command executed successfully),
 * CAMEL_IMAP_NO (operational error message), CAMEL_IMAP_BAD (error
 * message from the server), or CAMEL_IMAP_FAIL (a protocol-level error
 * occurred, and Camel is uncertain of the result of the command.)
 **/
gint
camel_imap_command_extended (CamelImapStore *store, CamelFolder *folder, char **ret, char *fmt, ...)
{
	CamelService *service = CAMEL_SERVICE (store);
	CamelURL *url = service->url;
	gint len = 0, recent = 0, status = CAMEL_IMAP_OK;
	gchar *cmdid, *cmdbuf, *respbuf;
	GPtrArray *data;
	va_list app;
	int i;

#if 0
	/* First make sure we're connected... */
	if (!service->connected || !stream_is_alive (store->istream)) {
		CamelException *ex;
		
		ex = camel_exception_new ();
		
		if (!imap_disconnect (service, ex) || !imap_connect (service, ex)) {
			camel_exception_free (ex);
			
			*ret = NULL;
			
			return CAMEL_IMAP_FAIL;
		}
		service->connected = TRUE;
		
		camel_exception_free (ex);
	}
#endif
	
	if (folder && store->current_folder != folder && strncmp (fmt, "CREATE", 6)) {
		/* We need to select the correct mailbox first */
		char *r, *folder_path, *dir_sep;
		int s;
		
		dir_sep = store->dir_sep;
		
		if (url && url->path && *(url->path + 1) && strcmp (folder->full_name, "INBOX"))
			folder_path = g_strdup_printf ("%s%s%s", url->path + 1, dir_sep, folder->full_name);
		else
			folder_path = g_strdup (folder->full_name);
		
		s = camel_imap_command_extended (store, NULL, &r, "SELECT %s", folder_path);
		g_free (folder_path);
		if (!r || s != CAMEL_IMAP_OK) {
			*ret = r;
			store->current_folder = NULL;
			
			return s;
		}
		
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
		CamelStreamBuffer *stream = CAMEL_STREAM_BUFFER (store->istream);
		char *ptr;
		
		respbuf = camel_stream_buffer_read_line (stream);
		if (!respbuf || !strncmp (respbuf, cmdid, strlen (cmdid))) {
			/* IMAP's last response starts with our command id */
			d(fprintf (stderr, "received: %s\n", respbuf ? respbuf : "(null)"));
#if 0			
			if (!respbuf && strcmp (fmt, "LOGOUT")) {
				/* we need to force a disconnect here? */
				CamelException *ex;
				
				ex = camel_exception_new ();
				imap_disconnect (service, ex);
				camel_exception_free (ex);
		        }
#endif
			break;
		}
		
		d(fprintf (stderr, "received: %s\n", respbuf));
		
		g_ptr_array_add (data, respbuf);
		len += strlen (respbuf) + 1;
		
		/* If recent was somehow set and this response doesn't begin with a '*'
		   then recent must have been misdetected */
		if (recent && *respbuf != '*')
			recent = 0;
		
		if (*respbuf == '*' && (ptr = strstr (respbuf, "RECENT"))) {
			char *rcnt, *ercnt;
			
			d(fprintf (stderr, "*** We may have found a 'RECENT' flag: %s\n", respbuf));
			/* Make sure it's in the form: "* %d RECENT" */
			rcnt = respbuf + 2;
			if (*rcnt > '0' || *rcnt < '9') {
				for (ercnt = rcnt; ercnt < ptr && *ercnt != ' '; ercnt++);
				if (ercnt + 1 == ptr)
					recent = atoi (rcnt);
			}
		}
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
	
	for (i = 0; i < data->len; i++)
		g_free (data->pdata[i]);
	g_ptr_array_free (data, TRUE);
	
	if (folder && recent > 0) {
		CamelException *ex;
		
		ex = camel_exception_new ();
		camel_imap_folder_changed (folder, recent, ex);
		camel_exception_free (ex);
	}
	
	return status;
}
