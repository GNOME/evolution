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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gal/util/e-util.h>

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

static CamelRemoteStoreClass *remote_store_class = NULL;

static gboolean imap_connect (CamelService *service, CamelException *ex);
static gboolean imap_disconnect (CamelService *service, CamelException *ex);
static GList *query_auth_types_generic (CamelService *service, CamelException *ex);
static GList *query_auth_types_connected (CamelService *service, CamelException *ex);
static CamelFolder *get_folder (CamelStore *store, const char *folder_name, gboolean create,
				CamelException *ex);
static char *get_folder_name (CamelStore *store, const char *folder_name,
			      CamelException *ex);
static char *get_root_folder_name (CamelStore *store, CamelException *ex);
static CamelFolderInfo *get_folder_info (CamelStore *store, const char *top,
					 gboolean fast, gboolean recursive,
					 CamelException *ex);
static void imap_keepalive (CamelRemoteStore *store);
/*static gboolean stream_is_alive (CamelStream *istream);*/
static int camel_imap_status (char *cmdid, char *respbuf);

static void
camel_imap_store_class_init (CamelImapStoreClass *camel_imap_store_class)
{
	/* virtual method overload */
	CamelServiceClass *camel_service_class =
		CAMEL_SERVICE_CLASS (camel_imap_store_class);
	CamelStoreClass *camel_store_class =
		CAMEL_STORE_CLASS (camel_imap_store_class);
	CamelRemoteStoreClass *camel_remote_store_class =
		CAMEL_REMOTE_STORE_CLASS (camel_imap_store_class);
	
	remote_store_class = CAMEL_REMOTE_STORE_CLASS(camel_type_get_global_classfuncs 
						      (camel_remote_store_get_type ()));
	
	/* virtual method overload */
	camel_service_class->query_auth_types_generic = query_auth_types_generic;
	camel_service_class->query_auth_types_connected = query_auth_types_connected;
	camel_service_class->connect = imap_connect;
	camel_service_class->disconnect = imap_disconnect;
	
	camel_store_class->get_folder = get_folder;
	camel_store_class->get_folder_name = get_folder_name;
	camel_store_class->get_root_folder_name = get_root_folder_name;
	camel_store_class->get_folder_info = get_folder_info;
	camel_store_class->free_folder_info = camel_store_free_folder_info_full;
	
	camel_remote_store_class->keepalive = imap_keepalive;
}

static void
camel_imap_store_init (gpointer object, gpointer klass)
{
	CamelService *service = CAMEL_SERVICE (object);
	CamelRemoteStore *remote_store = CAMEL_REMOTE_STORE (object);
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (object);
	
	service->url_flags |= (CAMEL_SERVICE_URL_NEED_USER |
			       CAMEL_SERVICE_URL_NEED_HOST |
			       CAMEL_SERVICE_URL_ALLOW_PATH |
			       CAMEL_SERVICE_URL_ALLOW_AUTH);

	remote_store->default_port = 143;
	
	imap_store->dir_sep = NULL;
	imap_store->current_folder = NULL;
}

CamelType
camel_imap_store_get_type (void)
{
	static CamelType camel_imap_store_type = CAMEL_INVALID_TYPE;
	
	if (camel_imap_store_type == CAMEL_INVALID_TYPE)	{
		camel_imap_store_type =
			camel_type_register (CAMEL_REMOTE_STORE_TYPE, "CamelImapStore",
					     sizeof (CamelImapStore),
					     sizeof (CamelImapStoreClass),
					     (CamelObjectClassInitFunc) camel_imap_store_class_init,
					     NULL,
					     (CamelObjectInitFunc) camel_imap_store_init,
					     (CamelObjectFinalizeFunc) NULL);
	}
	
	return camel_imap_store_type;
}

static CamelServiceAuthType password_authtype = {
	"Password",
	
	"This option will connect to the IMAP server using a "
	"plaintext password.",
	
	"",
	TRUE
};

static GList *
query_auth_types_connected (CamelService *service, CamelException *ex)
{
#if 0	
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
				      "Could not connect to IMAP server on %s.",
				      service->url->host ? service->url->host : 
				      "(unknown host)");
	}				      
	
	return ret;
#else
	g_warning ("imap::query_auth_types_connected: not implemented. Defaulting.");
	/* FIXME: use the classfunc instead of the local? */
	return query_auth_types_generic (service, ex);
#endif
}

static GList *
query_auth_types_generic (CamelService *service, CamelException *ex)
{
	GList *prev;
	
	prev = CAMEL_SERVICE_CLASS (remote_store_class)->query_auth_types_generic (service, ex);
	return g_list_prepend (prev, &password_authtype);
}

static gboolean
imap_connect (CamelService *service, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (service);
	CamelSession *session = camel_service_get_session (CAMEL_SERVICE (store));
	gchar *result, *buf, *errbuf = NULL;
	GPtrArray *response;
	gboolean authenticated = FALSE;
	gint status;
	
	if (CAMEL_SERVICE_CLASS (remote_store_class)->connect (service, ex) == FALSE)
		return FALSE;
	
	store->command = 0;
	g_free (store->dir_sep);
	store->dir_sep = g_strdup ("/");  /* default dir sep */
	
	/* Read the greeting, if any. */
	if (camel_remote_store_recv_line (CAMEL_REMOTE_STORE (service), &buf, ex) < 0) {
		return FALSE;
	}
	g_free (buf);
	
	/* authenticate the user */
	while (!authenticated) {
		if (errbuf) {
			/* We need to un-cache the password before prompting again */
			camel_session_query_authenticator (session,
							   CAMEL_AUTHENTICATOR_TELL, NULL,
							   TRUE, service, "password", ex);
			g_free (service->url->passwd);
			service->url->passwd = NULL;
		}
		
		if (!service->url->authmech && !service->url->passwd) {
			gchar *prompt;
			
			prompt = g_strdup_printf ("%sPlease enter the IMAP password for %s@%s",
						  errbuf ? errbuf : "", service->url->user, service->url->host);
			service->url->passwd =
				camel_session_query_authenticator (session,
								   CAMEL_AUTHENTICATOR_ASK, prompt,
								   TRUE, service, "password", ex);
			g_free (prompt);
			g_free (errbuf);
			errbuf = NULL;
			
			if (!service->url->passwd) {
				camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL, 
						     "You didn\'t enter a password.");
				return FALSE;
			}
		}
		
		status = camel_imap_command (store, NULL, ex, "LOGIN \"%s\" \"%s\"",
					     service->url->user,
					     service->url->passwd);
		
		if (status != CAMEL_IMAP_OK) {
			errbuf = g_strdup_printf ("Unable to authenticate to IMAP server.\n"
						  "%s\n\n",
						  camel_exception_get_description (ex));
			camel_exception_clear (ex);
		} else {
			g_message ("IMAP Service sucessfully authenticated user %s", service->url->user);
			authenticated = TRUE;
		}
	}
	
	/* Now lets find out the IMAP capabilities */
	status = camel_imap_command_extended (store, NULL, &response, ex, "CAPABILITY");
	if (status != CAMEL_IMAP_OK)
		return FALSE;

	result = camel_imap_response_extract (response, "CAPABILITY", ex);
	if (!result)
		return FALSE;

	/* parse for capabilities here. */
	if (e_strstrcase (result, "IMAP4REV1"))
		store->server_level = IMAP_LEVEL_IMAP4REV1;
	else if (e_strstrcase (result, "IMAP4"))
		store->server_level = IMAP_LEVEL_IMAP4;
	else
		store->server_level = IMAP_LEVEL_UNKNOWN;

	if ((store->server_level >= IMAP_LEVEL_IMAP4REV1) ||
	    (e_strstrcase (result, "STATUS")))
		store->has_status_capability = TRUE;
	else
		store->has_status_capability = FALSE;
	g_free (result);

	/* We now need to find out which directory separator this daemon uses */
	status = camel_imap_command_extended (store, NULL, &response, ex, "LIST \"\" \"\"");
	if (status != CAMEL_IMAP_OK)
		return FALSE;
	result = camel_imap_response_extract (response, "LIST", ex);
	if (!result)
		return FALSE;
	else {
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
		g_free (result);
	}

	camel_remote_store_refresh_folders (CAMEL_REMOTE_STORE (store), ex);

	return ! camel_exception_is_set (ex);
}

static gboolean
imap_disconnect (CamelService *service, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (service);
	
	/* send the logout command */
	camel_imap_command_extended (store, NULL, NULL, ex, "LOGOUT");
	
	g_free (store->dir_sep);
	store->dir_sep = NULL;
	
	store->current_folder = NULL;
	
	return CAMEL_SERVICE_CLASS (remote_store_class)->disconnect (service, ex);
}

char *
camel_imap_store_folder_path (CamelImapStore *store, const char *name)
{
	CamelURL *url = CAMEL_SERVICE (store)->url;
	char *namespace;

	if (url->path && *url->path)
		namespace = url->path + 1;
	else
		namespace = "";

	if (!*name)
		return g_strdup (namespace);
	else if (!g_strcasecmp (name, "INBOX") || !*namespace)
		return g_strdup (name);
	else
		return g_strdup_printf ("%s%s%s", namespace, store->dir_sep, name);
}

static gboolean
imap_folder_exists (CamelImapStore *store, const char *folder_path, gboolean *selectable, CamelException *ex)
{
	GPtrArray *response;
	char *result, *flags, *sep, *dirname;
	gint status;
	
	if (!g_strcasecmp (folder_path, "INBOX")) {
		if (selectable)
			*selectable = TRUE;
		return TRUE;
	}
	
	/* it's always gonna be FALSE unless it's true - how's that for a comment? ;-) */
	if (selectable)
		*selectable = FALSE;
	
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (store), NULL,
					      &response, ex, "LIST \"\" \"%s\"", folder_path);
	if (status != CAMEL_IMAP_OK)
		return FALSE;
	result = camel_imap_response_extract (response, "LIST", ex);
	if (!result)
		return FALSE;

	if (imap_parse_list_response (result, "", &flags, &sep, &dirname)) {
		if (selectable)
			*selectable = !e_strstrcase (flags, "NoSelect");
		
		g_free (flags);
		g_free (sep);
		g_free (dirname);
		g_free (result);
		
		return TRUE;
	}
	g_free (result);
	
	g_free (flags);
	g_free (sep);
	g_free (dirname);
	
	return FALSE;
}

static gboolean
imap_create (CamelImapStore *store, const char *folder_path, CamelException *ex)
{
	gint status;
	
	status = camel_imap_command_extended (store, NULL, NULL, ex,
					      "CREATE \"%s\"", folder_path);
	
	return status == CAMEL_IMAP_OK;
}

static CamelFolder *
get_folder (CamelStore *store, const char *folder_name, gboolean create, CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	CamelFolder *new_folder;
	char *folder_path;
	gboolean selectable;
	
	folder_path = camel_imap_store_folder_path (imap_store, folder_name);
	if (!imap_folder_exists (imap_store, folder_path, &selectable, ex)) {
		if (!create) {
			g_free (folder_path);
			return NULL;
		}

		if (!imap_create (imap_store, folder_path, ex)) {
			g_free (folder_path);
			return NULL;
		}
	} else if (!selectable) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				      "%s is not a selectable folder",
				      folder_name);
		g_free (folder_path);
		return NULL;
	}
	g_free (folder_path);

	new_folder = camel_imap_folder_new (store, folder_name);
	camel_folder_refresh_info (new_folder, ex);
	if (camel_exception_is_set (ex)) {
		camel_object_unref (CAMEL_OBJECT (new_folder));
		return NULL;
	}

	return new_folder;
}

static char *
get_folder_name (CamelStore *store, const char *folder_name,
		 CamelException *ex)
{
	/* INBOX is case-insensitive */
	if (g_strcasecmp (folder_name, "INBOX") == 0)
		return g_strdup ("INBOX");
	else
		return g_strdup (folder_name);
}

static char *
get_root_folder_name (CamelStore *store, CamelException *ex)
{
	return g_strdup ("");
}

static CamelFolderInfo *
parse_list_response_as_folder_info (const char *response,
				    const char *namespace,
				    const char *base_url)
{
	CamelFolderInfo *fi;
	char *flags, *sep, *dir;

	if (!imap_parse_list_response (response, namespace,
				       &flags, &sep, &dir))
		return NULL;

	fi = g_new0 (CamelFolderInfo, 1);
	fi->full_name = dir;
	fi->name = strrchr (dir, *sep);
	if (fi->name)
		fi->name = g_strdup (fi->name + 1);
	else
		fi->name = g_strdup (dir);
	g_free (sep);
	if (!e_strstrcase (flags, "\\NoSelect"))
		fi->url = g_strdup_printf ("%s%s", base_url, dir);
	g_free (flags);
	/* FIXME: read/unread msg count */

	return fi;
}

static CamelFolderInfo *
get_folder_info (CamelStore *store, const char *top, gboolean fast,
		 gboolean recursive, CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	CamelURL *url = CAMEL_SERVICE (store)->url;
	gboolean found_inbox = FALSE;
	int status, len, i;
	GPtrArray *response;
	char *dir_sep, *namespace, *base_url, *list;
	CamelFolderInfo *topfi, *fi;

	if (!top)
		top = "";
	dir_sep = imap_store->dir_sep;
	namespace = camel_imap_store_folder_path (imap_store, top);

	/* Yah! I am complicated! */
	base_url = camel_url_to_string (url, FALSE);
	len = strlen (base_url);
	if (url->path && base_url[len - 1] != *dir_sep) {
		base_url = g_realloc (base_url, len + 2);
		base_url[len] = *dir_sep;
		base_url[len + 1] = '\0';
	} else if (!url->path) {
		base_url = g_realloc (base_url, len + 2);
		base_url[len] = '/';
		base_url[len + 1] = '\0';
	}

	status = camel_imap_command_extended (imap_store, NULL, &response, ex,
					      "LIST \"\" \"%s\"", namespace);
	if (status != CAMEL_IMAP_OK) {
		g_free (namespace);
		g_free (base_url);
		return NULL;
	}
	list = camel_imap_response_extract (response, "LIST", ex);
	if (!list) {
		g_free (namespace);
		g_free (base_url);
		return NULL;
	}
	topfi = parse_list_response_as_folder_info (list, namespace, base_url);
	g_free (list);

	status = camel_imap_command_extended (imap_store, NULL, &response, ex,
					      "LIST \"\" \"%s%s%c\"",
					      namespace,
					      *namespace ? dir_sep : "",
					      recursive ? '*' : '%');
	if (status != CAMEL_IMAP_OK) {
		g_free (namespace);
		g_free (base_url);
		return NULL;
	}

	/* Turn responses into CamelFolderInfo and remove any
	 * extraneous responses.
	 */
	for (i = 0; i < response->len; i++) {
		list = response->pdata[i];
		response->pdata[i] = fi =
			parse_list_response_as_folder_info (list, namespace,
							    base_url);
		g_free (list);

		if (!response->pdata[i]) {
			g_ptr_array_remove_index_fast (response, i--);
			continue;
		}

		if (!g_strcasecmp (fi->full_name, "INBOX"))
			found_inbox = TRUE;
	}

	/* Add INBOX, if necessary */	
	if (!*top && !found_inbox) {
		fi = g_new0 (CamelFolderInfo, 1);
		fi->full_name = g_strdup ("INBOX");
		fi->name = g_strdup ("INBOX");
		fi->url = g_strdup_printf ("%sINBOX", base_url);
		/* FIXME: read/unread msg count */

		g_ptr_array_add (response, fi);
	}

	/* And assemble */
	camel_folder_info_build (response, topfi, *dir_sep, TRUE);
	g_ptr_array_free (response, FALSE);

	/* Remove the top if it's the root of the store. */
	if (!*top && !topfi->sibling) {
		fi = topfi;
		topfi = topfi->child;
		fi->child = NULL;
		camel_folder_info_free (fi);
	}

	g_free (namespace);
	g_free (base_url);
	return topfi;
}

static void
imap_keepalive (CamelRemoteStore *store)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	
	camel_imap_command_extended (imap_store, NULL, NULL, NULL, "NOOP");
}

static int
camel_imap_status (char *cmdid, char *respbuf)
{
	char *retcode;
	
	if (respbuf) {
		if (!strncmp (respbuf, cmdid, strlen (cmdid))) {
			retcode = imap_next_word (respbuf);
			
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

static gint
check_current_folder (CamelImapStore *store, CamelFolder *folder, char *fmt, CamelException *ex)
{
	char *folder_path;
	int status;
	
	/* return OK if we meet one of the following criteria:
	 * 1. the command doesn't care about which folder we're in (folder == NULL)
	 * 2. if we're already in the right folder (store->current_folder == folder)
	 * 3. we're going to create a new folder */
	if (!folder || store->current_folder == folder || !strncmp (fmt, "CREATE ", 7))
		return CAMEL_IMAP_OK;
	
	folder_path = camel_imap_store_folder_path (store, folder->full_name);
	status = camel_imap_command_extended (store, NULL, NULL, ex, "SELECT \"%s\"", folder_path);
	g_free (folder_path);
	
	if (status != CAMEL_IMAP_OK) {
		store->current_folder = NULL;
		return status;
	}
	
	/* remember our currently selected folder */
	store->current_folder = folder;
	
	return CAMEL_IMAP_OK;
}

static gboolean
send_command (CamelImapStore *store, char **cmdid, char *fmt, va_list ap, CamelException *ex)
{
	gchar *cmdbuf;
	
	*cmdid = g_strdup_printf ("A%.5d", store->command++);
	
	cmdbuf = g_strdup_vprintf (fmt, ap);
	
	if (camel_remote_store_send_string (CAMEL_REMOTE_STORE (store), ex, "%s %s\r\n", *cmdid, cmdbuf) < 0) {
		g_free (cmdbuf);
		g_free (*cmdid);
		*cmdid = NULL;
		return FALSE;
	}
	
	g_free (cmdbuf);
	return TRUE;
}


/**
 * camel_imap_command: Send a command to a IMAP server.
 * @store: the IMAP store
 * @folder: The folder to perform the operation in
 * @ret: a pointer to return the full server response in
 * @ex: a CamelException.
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
camel_imap_command (CamelImapStore *store, CamelFolder *folder, CamelException *ex, char *fmt, ...)
{
	char *cmdid, *respbuf, *word;
	gint status = CAMEL_IMAP_OK;
	va_list ap;
	
	/* check for current folder */
	status = check_current_folder (store, folder, fmt, ex);
	if (status != CAMEL_IMAP_OK)
		return status;
	
	/* send the command */
	va_start (ap, fmt);
        if (!send_command (store, &cmdid, fmt, ap, ex)) {
		va_end (ap);
		g_free (cmdid);
		return CAMEL_IMAP_FAIL;
	}
	va_end (ap);
	
	/* read single line response */
	if (camel_remote_store_recv_line (CAMEL_REMOTE_STORE (store), &respbuf, ex) < 0) {
		g_free (cmdid);
		return CAMEL_IMAP_FAIL;
	}
	
	status = camel_imap_status (cmdid, respbuf);
	g_free (cmdid);
	
	if (status == CAMEL_IMAP_OK)
		return status;
	
	if (respbuf) {
		/* get error response and set exception accordingly */
		word = imap_next_word (respbuf); /* points to status */
		word = imap_next_word (word);    /* points to fail message, if there is one */
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "IMAP command failed: %s", word);
	} else {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "IMAP command failed: %s", "Unknown");
	}
	
	return status;
}

/**
 * camel_imap_command_extended: Send a command to a IMAP server and get
 * a multi-line response.
 * @store: the IMAP store
 * @folder: The folder to perform the operation in
 * @ret: a pointer to return the full server response in, or %NULL
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
 * commands, such as SELECT, LIST, and various other commands.
 * The returned data is un-byte-stuffed, and has lines termined by
 * newlines rather than CR/LF pairs.
 * 
 * Return value: one of CAMEL_IMAP_OK (command executed successfully),
 * CAMEL_IMAP_NO (operational error message), CAMEL_IMAP_BAD (error
 * message from the server), or CAMEL_IMAP_FAIL (a protocol-level error
 * occurred, and Camel is uncertain of the result of the command.)
 **/

gint
camel_imap_command_extended (CamelImapStore *store, CamelFolder *folder, GPtrArray **ret, CamelException *ex, char *fmt, ...)
{
	gint status = CAMEL_IMAP_OK;
	GPtrArray *data = NULL;
	GArray *expunged;
	gchar *respbuf, *cmdid;
	gint recent = 0;
	va_list ap;
	gint i;
	
	/* check for current folder */
	status = check_current_folder (store, folder, fmt, ex);
	if (status != CAMEL_IMAP_OK)
		return status;
	
	/* send the command */
	va_start (ap, fmt);
        if (!send_command (store, &cmdid, fmt, ap, ex)) {
		va_end (ap);
		return CAMEL_IMAP_FAIL;
	}
	va_end (ap);
	
	expunged = g_array_new (FALSE, FALSE, sizeof (int));
	if (ret)
		data = g_ptr_array_new ();
	
	/* read multi-line response */
	while (1) {
		if (camel_remote_store_recv_line (CAMEL_REMOTE_STORE (store), &respbuf, ex) < 0) {
			/* cleanup */
			if (ret) {
				for (i = 0; i < data->len; i++)
					g_free (data->pdata[i]);
				g_ptr_array_free (data, TRUE);
			}
			g_array_free (expunged, TRUE);
			
			return CAMEL_IMAP_FAIL;
		}
		
		/* IMAPs multi-line response ends with the cmdid string at the beginning of the line */
		if (!strncmp (respbuf, cmdid, strlen (cmdid))) {
			status = camel_imap_status (cmdid, respbuf);
			break;
		}
		
		/* Check for a RECENT in the untagged response */
		if (*respbuf == '*') {
			if (strstr (respbuf, "RECENT")) {
				char *rcnt;
				
				d(fprintf (stderr, "*** We may have found a 'RECENT' flag: %s\n", respbuf));
				/* Make sure it's in the form: "* %d RECENT" */
				rcnt = imap_next_word (respbuf);
				if (*rcnt >= '0' && *rcnt <= '9' && !strncmp ("RECENT", imap_next_word (rcnt), 6))
					recent = atoi (rcnt);
				g_free (respbuf);
				continue;
			} else if (strstr (respbuf, "EXPUNGE")) {
				char *id_str;
				int id;
				
				d(fprintf (stderr, "*** We may have found an 'EXPUNGE' flag: %s\n", respbuf));
				/* Make sure it's in the form: "* %d EXPUNGE" */
				id_str = imap_next_word (respbuf);
				if (*id_str >= '0' && *id_str <= '9' && !strncmp ("EXPUNGE", imap_next_word (id_str), 7)) {
					id = atoi (id_str);
					g_array_append_val (expunged, id);
				}
				g_free (respbuf);
				continue;
			}
		}
		if (ret)
			g_ptr_array_add (data, respbuf);
		else
			g_free (respbuf);
	}
	
	if (status == CAMEL_IMAP_OK) {
		if (ret)
			*ret = data;
	} else {
		/* command failed */
		if (respbuf) {
			char *word;
			
			word = imap_next_word (respbuf); /* should now point to status */
			
			word = imap_next_word (word);    /* points to fail message, if there is one */
			
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					      "IMAP command failed: %s", word);
		} else {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					      "IMAP command failed: Unknown");
		}
		
		if (ret) {
			for (i = 0; i < data->len; i++)
				g_free (data->pdata[i]);
			g_ptr_array_free (data, TRUE);
		}
	}
	
	if (respbuf)
		g_free (respbuf);

	/* Update the summary */
	if (folder && (recent > 0 || expunged->len > 0)) {
		CamelException dex;
		
		camel_exception_init (&dex);
		camel_imap_folder_changed (folder, recent, expunged, &dex);
		camel_exception_clear (&dex);
	}
	g_array_free (expunged, TRUE);
	
	return status;
}

/**
 * camel_imap_response_free:
 * @response: the result data returned from camel_imap_command_extended
 *
 * Frees the data.
 **/
void
camel_imap_response_free (GPtrArray *response)
{
	int i;

	for (i = 0; i < response->len; i++)
		g_free (response->pdata[i]);
	g_ptr_array_free (response, TRUE);
}

/**
 * camel_imap_response_extract:
 * @response: the result data returned from camel_imap_command_extended
 * @type: the response type to extract
 * @ex: a CamelException
 *
 * This checks that @response contains a single untagged response of
 * type @type and returns just that response data. If @response
 * doesn't contain the right information, the function will set @ex and
 * return %NULL. Either way, @response will be freed.
 *
 * Return value: the desired response string, which the caller must free.
 **/
char *
camel_imap_response_extract (GPtrArray *response, const char *type,
			     CamelException *ex)
{
	int len = strlen (type), i;
	char *resp;

	for (i = 0; i < response->len; i++) {
		resp = response->pdata[i];
		if (strncmp (resp, "* ", 2) != 0) {
			g_free (resp);
			continue;
		}

		/* Skip inititial sequence number, if present */
		strtoul (resp + 2, &resp, 10);
		if (*resp == ' ')
			resp++;

		if (!g_strncasecmp (resp, type, len))
			break;

		g_free (resp);
	}

	if (i < response->len) {
		resp = response->pdata[i];
		for (i++; i < response->len; i++)
			g_free (response->pdata[i]);
	} else {
		resp = NULL;
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "IMAP server response did not contain "
				      "%s information", type);
	}

	g_ptr_array_free (response, TRUE);
	return resp;
}	

/**
 * camel_imap_fetch_command: Send a FETCH request to an IMAP server and get
 * a multi-line response.
 * @store: the IMAP store
 * @folder: The folder to perform the operation in
 * @ret: a pointer to return the full server response in, or %NULL
 * @fmt: a printf-style format string, followed by arguments
 * 
 * This camel method sends the IMAP FETCH command specified by @fmt and the
 * following arguments to the IMAP store specified by @store. If the
 * store is in a disconnected state, camel_imap_fetch_command will first
 * re-connect the store before sending the specified IMAP command. It then
 * reads the server's response and parses out the status code. If the caller
 * passed a non-NULL pointer for @ret, camel_imap_fetch_command will set
 * it to point to a buffer containing the rest of the response from the IMAP
 * server. (If @ret was passed but there was no extended response, @ret will
 * be set to NULL.) The caller function is responsible for freeing @ret.
 * 
 * Return value: one of CAMEL_IMAP_OK (command executed successfully),
 * CAMEL_IMAP_NO (operational error message), CAMEL_IMAP_BAD (error
 * message from the server), or CAMEL_IMAP_FAIL (a protocol-level error
 * occurred, and Camel is uncertain of the result of the command.)
 **/

gint
camel_imap_fetch_command (CamelImapStore *store, CamelFolder *folder, char **ret, CamelException *ex, char *fmt, ...)
{
	/* Security Note: We have to be careful about assuming
	 * that a server response is valid as the command we are
	 * calling may require a literal string response which could
	 * possibly contain strings that appear to be valid server
	 * responses but aren't. We should, therefor, find a way to
	 * determine whether we are actually reading server responses.
	 */
	gint status = CAMEL_IMAP_OK;
	GPtrArray *data;
	GArray *expunged;
	gboolean is_notification;
	gchar *respbuf, *cmdid;
	guint32 len = 0;
	gint partlen = 0;
	gint recent = 0;
	va_list ap;
	gint i;
	
	status = check_current_folder (store, folder, fmt, ex);
	if (status != CAMEL_IMAP_OK)
		return status;
	
	/* send the command */
	va_start (ap, fmt);
        if (!send_command (store, &cmdid, fmt, ap, ex)) {
		va_end (ap);
		return CAMEL_IMAP_FAIL;
	}
	va_end (ap);
	
	data = g_ptr_array_new ();
	
	/* get first response line */
	if (camel_remote_store_recv_line (CAMEL_REMOTE_STORE (store), &respbuf, ex) != -1) {
		char *p, *q;
		
		g_ptr_array_add (data, respbuf);
		len += strlen (respbuf) + 1;
		
		for (p = respbuf; *p && *p != '{' && *p != '"' && *p != '\n'; p++);
		switch (*p) {
		case '"':
			/* a quoted string - section 4.3 */
			p++;
			for (q = p; *q && *q != '"'; q++);
			partlen = (guint32) (q - p);
			
			is_notification = TRUE;
			
			break;
		case '{':
			/* a literal string - section 4.3 */
			partlen = atoi (p + 1);
			
			/* add len to partlen because the partlen
			   doesn't count the first response buffer */
			partlen += len;
			
			is_notification = FALSE;
			
			break;
		default:
			/* bad input */
			g_ptr_array_free (data, TRUE);
			return CAMEL_IMAP_FAIL;
		}
	} else {
		g_ptr_array_free (data, TRUE);
		return CAMEL_IMAP_FAIL;
	}
	
	expunged = g_array_new (FALSE, FALSE, sizeof (int));
	
	/* read multi-line response */
	while (1) {
		if (camel_remote_store_recv_line (CAMEL_REMOTE_STORE (store), &respbuf, ex) < 0) {
			/* cleanup */
			for (i = 0; i < data->len; i++)
				g_free (data->pdata[i]);
			g_ptr_array_free (data, TRUE);
			g_array_free (expunged, TRUE);
			
			return CAMEL_IMAP_FAIL;
		}
		
		g_ptr_array_add (data, respbuf);
		len += strlen (respbuf) + 1;
		
		/* IMAPs multi-line response ends with the cmdid string at the beginning of the line */
		if (is_notification && !strncmp (respbuf, cmdid, strlen (cmdid))) {
			status = camel_imap_status (cmdid, respbuf);
			break;
		}
		
		/* FIXME: this is redundant */
		/* If recent or expunge flags were somehow set and this
		   response doesn't begin with a '*' then
		   recent/expunged must have been misdetected */
		if ((recent || expunged->len > 0) && *respbuf != '*') {
			d(fprintf (stderr, "hmmm, someone tried to pull a fast one on us.\n"));
			
			recent = 0;
			
			for (i = 0; i < expunged->len; i++)
				g_array_remove_index (expunged, i);
		}
		
		/* Check for a RECENT in the untagged response */
		if (*respbuf == '*' && is_notification) {
			if (strstr (respbuf, "RECENT")) {
				char *rcnt;
				
				d(fprintf (stderr, "*** We may have found a 'RECENT' flag: %s\n", respbuf));
				/* Make sure it's in the form: "* %d RECENT" */
				rcnt = imap_next_word (respbuf);
				if (*rcnt >= '0' && *rcnt <= '9' && !strncmp ("RECENT", imap_next_word (rcnt), 6))
					recent = atoi (rcnt);
			} else if (strstr (respbuf, "EXPUNGE")) {
				char *id_str;
				int id;
				
				d(fprintf (stderr, "*** We may have found an 'EXPUNGE' flag: %s\n", respbuf));
				/* Make sure it's in the form: "* %d EXPUNGE" */
				id_str = imap_next_word (respbuf);
				if (*id_str >= '0' && *id_str <= '9' && !strncmp ("EXPUNGE", imap_next_word (id_str), 7)) {
					id = atoi (id_str);
					g_array_append_val (expunged, id);
				}
			}
		}
		
	        if (!is_notification) {
			partlen--;
			if (len >= partlen)
				is_notification = TRUE;
		}
	}
	
	if (status == CAMEL_IMAP_OK && ret) {
		gchar *p;
		
		/* populate the return buffer with the server response */
		*ret = g_new (char, len + 1);
		p = *ret;
		
		for (i = 0; i < data->len; i++) {
			char *datap;
			
			datap = (char *) data->pdata[i];
			if (*datap == '.')
				datap++;
			len = strlen (datap);
			memcpy (p, datap, len);
			p += len;
			*p++ = '\n';
		}
		
		*p = '\0';
	} else if (status != CAMEL_IMAP_OK) {
		/* command failed */
		if (respbuf) {
			char *word;
			
			word = imap_next_word (respbuf); /* should now point to status */
			
			word = imap_next_word (word);    /* points to fail message, if there is one */
			
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					      "IMAP command failed: %s", word);
		} else {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					      "IMAP command failed: Unknown");
		}
		
		if (ret)
			*ret = NULL;
	}
	
	/* Update the summary */
	if (folder && (recent > 0 || expunged->len > 0)) {
		CamelException dex;
		
		camel_exception_init (&dex);
		camel_imap_folder_changed (folder, recent, expunged, &dex);
		camel_exception_clear (&dex);
	}
	
	for (i = 0; i < data->len; i++)
		g_free (data->pdata[i]);
	g_ptr_array_free (data, TRUE);
	g_array_free (expunged, TRUE);
	
	return status;
}

/**
 * camel_imap_command_preliminary: Send a preliminary command to the
 * IMAP server.
 * @store: the IMAP store
 * @cmdid: a pointer to return the command identifier (for use in
 * camel_imap_command_continuation)
 * @fmt: a printf-style format string, followed by arguments
 * 
 * This camel method sends a preliminary IMAP command specified by
 * @fmt and the following arguments to the IMAP store specified by
 * @store. This function is meant for use with multi-transactional
 * IMAP communications like Kerberos authentication and APPEND.
 * 
 * Return value: one of CAMEL_IMAP_PLUS, CAMEL_IMAP_NO, CAMEL_IMAP_BAD
 * or CAMEL_IMAP_FAIL
 * 
 * Note: on success (CAMEL_IMAP_PLUS), you will need to follow up with
 * a camel_imap_command_continuation call.
 **/
gint
camel_imap_command_preliminary (CamelImapStore *store, char **cmdid, CamelException *ex, char *fmt, ...)
{
	char *respbuf, *word;
	gint status = CAMEL_IMAP_OK;
	va_list ap;
	
	/* send the command */
	va_start (ap, fmt);
        if (!send_command (store, cmdid, fmt, ap, ex)) {
		va_end (ap);
		return CAMEL_IMAP_FAIL;
	}
	va_end (ap);
	
	/* read single line response */
	if (camel_remote_store_recv_line (CAMEL_REMOTE_STORE (store), &respbuf, ex) < 0)
		return CAMEL_IMAP_FAIL;
	
	/* Check for '+' which indicates server is ready for command continuation */
	if (*respbuf == '+')
		return CAMEL_IMAP_PLUS;
	
	status = camel_imap_status (*cmdid, respbuf);
	
	if (respbuf) {
		/* get error response and set exception accordingly */
		word = imap_next_word (respbuf); /* points to status */
		word = imap_next_word (word);    /* points to fail message, if there is one */
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "IMAP command failed: %s", word);
	} else {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "IMAP command failed: %s", "Unknown");
	}
	
	return status;
}

/**
 * camel_imap_command_continuation: Handle another transaction with the IMAP
 * server and possibly get a multi-line response.
 * @store: the IMAP store
 * @cmdid: The command identifier returned from camel_imap_command_preliminary
 * @ret: a pointer to return the full server response in, or %NULL
 * @cmdbuf: buffer containing the response/request data
 *
 * This method is for sending continuing responses to the IMAP server.
 * Meant to be used as a followup to camel_imap_command_preliminary.
 * If @ret is non-%NULL camel_imap_command_continuation will set it to
 * point to a buffer containing the rest of the response from the IMAP
 * server. The caller function is responsible for freeing @ret.
 * 
 * Return value: one of CAMEL_IMAP_PLUS (command requires additional data),
 * CAMEL_IMAP_OK (command executed successfully),
 * CAMEL_IMAP_NO (operational error message),
 * CAMEL_IMAP_BAD (error message from the server), or
 * CAMEL_IMAP_FAIL (a protocol-level error occurred, and Camel is uncertain
 * of the result of the command.)
 **/
gint
camel_imap_command_continuation (CamelImapStore *store, char **ret, char *cmdid, char *cmdbuf, CamelException *ex)
{
	gint status = CAMEL_IMAP_OK;
	GPtrArray *data;
	gchar *respbuf;
	guint32 len = 0;
	gint i;
	
	if (camel_remote_store_send_string (CAMEL_REMOTE_STORE (store), ex, "%s\r\n", cmdbuf) < 0) {
		if (ret)
			*ret = NULL;
		return CAMEL_IMAP_FAIL;
	}
	
	data = g_ptr_array_new ();
	
	/* read multi-line response */
	while (1) {
		if (camel_remote_store_recv_line (CAMEL_REMOTE_STORE (store), &respbuf, ex) < 0) {
			/* cleanup */
			for (i = 0; i < data->len; i++)
				g_free (data->pdata[i]);
			g_ptr_array_free (data, TRUE);
			
			return CAMEL_IMAP_FAIL;
		}
		
		g_ptr_array_add (data, respbuf);
		len += strlen (respbuf) + 1;
		
		/* IMAPs multi-line response ends with the cmdid string at the beginning of the line */
		if (!strncmp (respbuf, cmdid, strlen (cmdid))) {
			status = camel_imap_status (cmdid, respbuf);
			break;
		}
	}
	
	if (status == CAMEL_IMAP_OK && ret) {
		gchar *p;
		
		/* populate the return buffer with the server response */
		*ret = g_new (char, len + 1);
		p = *ret;
		
		for (i = 0; i < data->len; i++) {
			char *datap;
			
			datap = (char *) data->pdata[i];
			if (*datap == '.')
				datap++;
			len = strlen (datap);
			memcpy (p, datap, len);
			p += len;
			*p++ = '\n';
		}
		
		*p = '\0';
	} else if (status != CAMEL_IMAP_OK) {
		/* command failed */
		if (respbuf) {
			char *word;
			
			word = imap_next_word (respbuf); /* should now point to status */
			
			word = imap_next_word (word);    /* points to fail message, if there is one */
			
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					      "IMAP command failed: %s", word);
		} else {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					      "IMAP command failed: Unknown");
		}
		
		if (ret)
			*ret = NULL;
	}
	
	/* cleanup */
	for (i = 0; i < data->len; i++)
		g_free (data->pdata[i]);
	g_ptr_array_free (data, TRUE);
	
	return status;
}

/**
 * camel_imap_command_continuation_with_stream: Handle another transaction with the IMAP
 * server and possibly get a multi-line response.
 * @store: the IMAP store
 * @cmdid: The command identifier returned from camel_imap_command_preliminary
 * @ret: a pointer to return the full server response in, or %NULL
 * @cstream: a CamelStream containing a continuation response.
 * 
 * This method is for sending continuing responses to the IMAP server.
 * Meant to be used as a followup to camel_imap_command_preliminary.
 * If @ret is not %NULL, camel_imap_command_continuation will set it
 * to point to a buffer containing the rest of the response from the
 * IMAP server. The caller function is responsible for freeing @ret.
 * 
 * Return value: one of CAMEL_IMAP_PLUS (command requires additional data),
 * CAMEL_IMAP_OK (command executed successfully),
 * CAMEL_IMAP_NO (operational error message),
 * CAMEL_IMAP_BAD (error message from the server), or
 * CAMEL_IMAP_FAIL (a protocol-level error occurred, and Camel is uncertain
 * of the result of the command.)
 **/
gint
camel_imap_command_continuation_with_stream (CamelImapStore *store, char **ret, char *cmdid,
					     CamelStream *cstream, CamelException *ex)
{
	gint status = CAMEL_IMAP_OK;
	GPtrArray *data;
	gchar *respbuf;
	guint32 len = 0;
	gint i;
	
	/* send stream */
	if (camel_remote_store_send_stream (CAMEL_REMOTE_STORE (store), cstream, ex) < 0) {
		if (ret)
			*ret = NULL;
		return CAMEL_IMAP_FAIL;
	}
	
	data = g_ptr_array_new ();
	
	/* read the servers multi-line response */
	while (1) {
		if (camel_remote_store_recv_line (CAMEL_REMOTE_STORE (store), &respbuf, ex) < 0) {
			/* cleanup */
			for (i = 0; i < data->len; i++)
				g_free (data->pdata[i]);
			g_ptr_array_free (data, TRUE);
			
			return CAMEL_IMAP_FAIL;
		}
		
		g_ptr_array_add (data, respbuf);
		len += strlen (respbuf) + 1;
		
		/* IMAPs multi-line response ends with the cmdid string at the beginning of the line */
		if (!strncmp (respbuf, cmdid, strlen (cmdid))) {
			status = camel_imap_status (cmdid, respbuf);
			break;
		}
	}
	
	if (status == CAMEL_IMAP_OK && ret) {
		gchar *p;
		
		/* populate the return buffer with the server response */
		*ret = g_new (char, len + 1);
		p = *ret;
		
		for (i = 0; i < data->len; i++) {
			char *datap;
			
			datap = (char *) data->pdata[i];
			if (*datap == '.')
				datap++;
			len = strlen (datap);
			memcpy (p, datap, len);
			p += len;
			*p++ = '\n';
		}
		
		*p = '\0';
	} else if (status != CAMEL_IMAP_OK) {
		/* command failed */
		if (respbuf) {
			char *word;
			
			word = imap_next_word (respbuf); /* should now point to status */
			
			word = imap_next_word (word);    /* points to fail message, if there is one */
			
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					      "IMAP command failed: %s", word);
		} else {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					      "IMAP command failed: Unknown error");
		}
		
		if (ret)
			*ret = NULL;
	}
	
	/* cleanup */
	for (i = 0; i < data->len; i++)
		g_free (data->pdata[i]);
	g_ptr_array_free (data, TRUE);
	
	return status;
}
