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
#include <errno.h>

#include <gal/util/e-util.h>

#include "camel-imap-store.h"
#include "camel-imap-folder.h"
#include "camel-imap-utils.h"
#include "camel-imap-command.h"
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
					 gboolean subscribed_only,
					 CamelException *ex);
static void imap_keepalive (CamelRemoteStore *store);

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
	
	imap_store->connected = FALSE;
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
	CamelImapResponse *response;
	gboolean authenticated = FALSE;
	
	if (CAMEL_SERVICE_CLASS (remote_store_class)->connect (service, ex) == FALSE)
		return FALSE;
	
	store->command = 0;
	g_free (store->dir_sep);
	store->dir_sep = g_strdup ("/");  /* default dir sep */
	if (!store->storage_path) {
		store->storage_path =
			camel_session_get_storage_path (session, service, ex);
		if (camel_exception_is_set (ex))
			return FALSE;
	}
	
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
		
		response = camel_imap_command (store, NULL, ex,
					       "LOGIN \"%s\" \"%s\"",
					       service->url->user,
					       service->url->passwd);
		if (!response) {
			errbuf = g_strdup_printf ("Unable to authenticate to IMAP server.\n"
						  "%s\n\n",
						  camel_exception_get_description (ex));
			camel_exception_clear (ex);
		} else {
			g_message ("IMAP Service sucessfully authenticated user %s", service->url->user);
			authenticated = TRUE;
			camel_imap_response_free (response);
		}
	}
	
	/* At this point we know we're connected... */
	store->connected = TRUE;
	
	/* Now lets find out the IMAP capabilities */
	response = camel_imap_command (store, NULL, ex, "CAPABILITY");
	if (!response)
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

	/* We now need to find out which directory separator this daemon
	 * uses. In the pre-4rev1 case, we can't do it, so we'll just
	 * hope that it's "/".
	 * FIXME: This code is wrong. The hierarchy separator is per
	 * namespace.
	 */
	if (store->server_level >= IMAP_LEVEL_IMAP4REV1) {
		response = camel_imap_command (store, NULL, ex,
					       "LIST \"\" \"\"");
		if (!response)
			return FALSE;
		result = camel_imap_response_extract (response, "LIST", ex);
		if (!result)
			return FALSE;
		else {
			char *flags, *sep, *folder;

			if (imap_parse_list_response (result, "", &flags,
						      &sep, &folder)) {
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
	}

	camel_remote_store_refresh_folders (CAMEL_REMOTE_STORE (store), ex);

	return ! camel_exception_is_set (ex);
}

static gboolean
imap_disconnect (CamelService *service, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (service);
	CamelImapResponse *response;
	
	if (store->connected) {
		/* send the logout command */
		response = camel_imap_command (store, NULL, ex, "LOGOUT");
		camel_imap_response_free (response);
	}
	
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
	CamelImapResponse *response;
	char *result, *flags, *sep, *dirname;
	
	if (!g_strcasecmp (folder_path, "INBOX")) {
		if (selectable)
			*selectable = TRUE;
		return TRUE;
	}
	
	/* it's always gonna be FALSE unless it's true - how's that for a comment? ;-) */
	if (selectable)
		*selectable = FALSE;
	
	response = camel_imap_command (store, NULL, ex,
				     "LIST \"\" \"%s\"", folder_path);
	if (!response)
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
	CamelImapResponse *response;
	
	response = camel_imap_command (store, NULL, ex,
				       "CREATE \"%s\"", folder_path);
	camel_imap_response_free (response);

	return !camel_exception_is_set (ex);
}

static CamelFolder *
get_folder (CamelStore *store, const char *folder_name, gboolean create, CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	CamelFolder *new_folder = NULL;
	char *folder_path, *summary_file, *p;
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

	summary_file = g_strdup_printf ("%s/%s/#summary",
					imap_store->storage_path,
					folder_name);
	p = strrchr (summary_file, '/');
	*p = '\0';
	if (e_mkdir_hier (summary_file, S_IRWXU) == 0) {
		*p = '/';
		new_folder = camel_imap_folder_new (store, folder_name,
						    summary_file, ex);
	} else {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Could not create directory %s: %s",
				      summary_file, g_strerror (errno));
	}
	g_free (summary_file);
	g_free (folder_path);

	if (camel_exception_is_set (ex))
		return NULL;

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
	if (sep)
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
		 gboolean recursive, gboolean subscribed_only,
		 CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	CamelURL *url = CAMEL_SERVICE (store)->url;
	gboolean found_inbox = FALSE;
	int len, i;
	CamelImapResponse *response;
	GPtrArray *folders;
	char *dir_sep, *namespace, *base_url, *list;
	CamelFolderInfo *topfi = NULL, *fi;

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

	response = camel_imap_command (imap_store, NULL, ex,
				       "LIST \"\" \"%s\"", namespace);
	if (response) {
		list = camel_imap_response_extract (response, "LIST", ex);
		if (list) {
			topfi = parse_list_response_as_folder_info (list,
								    namespace,
								    base_url);
			g_free (list);
		}
	}
	if (!topfi) {
		camel_exception_clear (ex);
		topfi = g_new0 (CamelFolderInfo, 1);
		topfi->full_name = g_strdup (namespace);
		topfi->name = g_strdup (namespace);
	}

	response = camel_imap_command (imap_store, NULL, ex,
				       "LIST \"\" \"%s%s%c\"",
				       namespace, *namespace ? dir_sep : "",
				       recursive ? '*' : '%');
	if (!response) {
		g_free (namespace);
		g_free (base_url);
		return NULL;
	}

	/* Turn responses into CamelFolderInfo and remove any
	 * extraneous responses.
	 */
	folders = g_ptr_array_new ();
	for (i = 0; i < response->untagged->len; i++) {
		list = response->untagged->pdata[i];
		fi = parse_list_response_as_folder_info (list, namespace,
							 base_url);
		if (!fi)
			continue;
		g_ptr_array_add (folders, fi);

		if (!g_strcasecmp (fi->full_name, "INBOX"))
			found_inbox = TRUE;
	}
	camel_imap_response_free (response);

	/* Add INBOX, if necessary */	
	if (!*top && !found_inbox) {
		fi = g_new0 (CamelFolderInfo, 1);
		fi->full_name = g_strdup ("INBOX");
		fi->name = g_strdup ("INBOX");
		fi->url = g_strdup_printf ("%sINBOX", base_url);
		/* FIXME: read/unread msg count */

		g_ptr_array_add (folders, fi);
	}

	/* And assemble */
	camel_folder_info_build (folders, topfi, *dir_sep, TRUE);
	g_ptr_array_free (folders, FALSE);

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
	CamelImapResponse *response;

	response = camel_imap_command (imap_store, NULL, NULL, "NOOP");
	camel_imap_response_free (response);
}
