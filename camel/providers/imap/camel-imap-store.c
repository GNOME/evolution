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
#include "camel-imap-auth.h"
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
static gboolean imap_disconnect (CamelService *service, gboolean clean, CamelException *ex);
static GList *query_auth_types_generic (CamelService *service, CamelException *ex);
static GList *query_auth_types_connected (CamelService *service, CamelException *ex);
static CamelFolder *get_folder (CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex);
static char *get_folder_name (CamelStore *store, const char *folder_name,
			      CamelException *ex);
static char *get_root_folder_name (CamelStore *store, CamelException *ex);
static CamelFolderInfo *get_folder_info (CamelStore *store, const char *top,
					 gboolean fast, gboolean recursive,
					 gboolean subscribed_only,
					 CamelException *ex);
static gboolean folder_subscribed (CamelStore *store, const char *folder_name);
static void subscribe_folder (CamelStore *store, const char *folder_name,
			      CamelException *ex);
static void unsubscribe_folder (CamelStore *store, const char *folder_name,
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

	camel_store_class->folder_subscribed = folder_subscribed;
	camel_store_class->subscribe_folder = subscribe_folder;
	camel_store_class->unsubscribe_folder = unsubscribe_folder;

	camel_remote_store_class->keepalive = imap_keepalive;
}

static gboolean
free_sub (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	return TRUE;
}

static void
camel_imap_store_finalize (CamelObject *object)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (object);

	g_hash_table_foreach_remove (imap_store->subscribed_folders,
				     free_sub, NULL);
	g_hash_table_destroy (imap_store->subscribed_folders);
}

static void
camel_imap_store_init (gpointer object, gpointer klass)
{
	CamelRemoteStore *remote_store = CAMEL_REMOTE_STORE (object);
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (object);
	CamelStore *store = CAMEL_STORE (object);
	
	remote_store->default_port = 143;

	imap_store->dir_sep = '\0';
	imap_store->current_folder = NULL;

	store->flags = CAMEL_STORE_SUBSCRIPTIONS;
	
	imap_store->connected = FALSE;
	imap_store->subscribed_folders = g_hash_table_new (g_str_hash, g_str_equal);
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
					     (CamelObjectFinalizeFunc) camel_imap_store_finalize);
	}
	
	return camel_imap_store_type;
}

static struct {
	const char *name;
	guint32 flag;
} capabilities[] = {
	{ "IMAP4",		IMAP_CAPABILITY_IMAP4 },
	{ "IMAP4REV1",		IMAP_CAPABILITY_IMAP4REV1 },
	{ "STATUS",		IMAP_CAPABILITY_STATUS },
	{ "NAMESPACE",		IMAP_CAPABILITY_NAMESPACE },
	{ "AUTH=KERBEROS_V4",	IMAP_CAPABILITY_AUTH_KERBEROS_V4 },
	{ "AUTH=GSSAPI",	IMAP_CAPABILITY_AUTH_GSSAPI },
	{ "UIDPLUS",		IMAP_CAPABILITY_UIDPLUS },
	{ "LITERAL+",		IMAP_CAPABILITY_LITERALPLUS },
	{ NULL, 0 }
};

static gboolean
connect_to_server (CamelService *service, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (service);
	CamelImapResponse *response;
	char *result, *buf, *capa, *lasts;
	int i;

	if (!CAMEL_SERVICE_CLASS (remote_store_class)->connect (service, ex))
		return FALSE;

	store->command = 0;

	/* Read the greeting, if any. FIXME: deal with PREAUTH */
	if (camel_remote_store_recv_line (CAMEL_REMOTE_STORE (service),
					  &buf, ex) < 0) {
		return FALSE;
	}
	g_free (buf);
	store->connected = TRUE;
	
	/* Find out the IMAP capabilities */
	store->capabilities = 0;
	response = camel_imap_command (store, NULL, ex, "CAPABILITY");
	if (!response)
		return FALSE;
	result = camel_imap_response_extract (response, "CAPABILITY", ex);
	if (!result)
		return FALSE;

	/* Skip over "* CAPABILITY". */
	capa = imap_next_word (result + 2);

	for (capa = strtok_r (capa, " ", &lasts); capa;
	     capa = strtok_r (NULL, " ", &lasts)) {
		for (i = 0; capabilities[i].name; i++) {
			if (g_strcasecmp (capa, capabilities[i].name) == 0) {
				store->capabilities |= capabilities[i].flag;
				break;
			}
		}
	}
	g_free (result);

	if (store->capabilities & IMAP_CAPABILITY_IMAP4REV1) {
		store->server_level = IMAP_LEVEL_IMAP4REV1;
		store->capabilities |= IMAP_CAPABILITY_STATUS;
	} else if (store->capabilities & IMAP_CAPABILITY_IMAP4)
		store->server_level = IMAP_LEVEL_IMAP4;
	else
		store->server_level = IMAP_LEVEL_UNKNOWN;

	return TRUE;
}

static CamelServiceAuthType password_authtype = {
	N_("Password"),
	
	N_("This option will connect to the IMAP server using a "
	   "plaintext password."),
	
	"",
	TRUE
};

#ifdef HAVE_KRB4
static CamelServiceAuthType kerberos_v4_authtype = {
	N_("Kerberos 4"),

	N_("This option will connect to the IMAP server using "
	   "Kerberos 4 authentication."),

	"KERBEROS_V4",
	FALSE
};
#endif

static GList *
query_auth_types_connected (CamelService *service, CamelException *ex)
{
	GList *types;

	if (!connect_to_server (service, ex))
		return NULL;

	types = CAMEL_SERVICE_CLASS (remote_store_class)->query_auth_types_connected (service, ex);
#ifdef HAVE_KRB4
	if (CAMEL_IMAP_STORE (service)->capabilities &
	    IMAP_CAPABILITY_AUTH_KERBEROS_V4)
		types = g_list_prepend (types, &kerberos_v4_authtype);
#endif
	return g_list_prepend (types, &password_authtype);
}

static GList *
query_auth_types_generic (CamelService *service, CamelException *ex)
{
	GList *types;
	
	types = CAMEL_SERVICE_CLASS (remote_store_class)->query_auth_types_generic (service, ex);
#ifdef HAVE_KRB4
	types = g_list_prepend (types, &kerberos_v4_authtype);
#endif
	return g_list_prepend (types, &password_authtype);
}

static gboolean
imap_connect (CamelService *service, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (service);
	CamelSession *session = camel_service_get_session (CAMEL_SERVICE (store));
	gchar *result, *errbuf = NULL, *namespace;
	CamelImapResponse *response;
	gboolean authenticated = FALSE;
	int len;

	if (connect_to_server (service, ex) == 0)
		return FALSE;
	
	/* authenticate the user */
#ifdef HAVE_KRB4
	if (service->url->authmech &&
	    !g_strcasecmp (service->url->authmech, "KERBEROS_V4")) {
		if (!(store->capabilities & IMAP_CAPABILITY_AUTH_KERBEROS_V4)) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
					      "IMAP server %s does not "
					      "support requested "
					      "authentication type %s",
					      service->url->host,
					      service->url->authmech);
			camel_service_disconnect (service, TRUE, NULL);
			return FALSE;
		}

		authenticated = imap_try_kerberos_v4_auth (store, ex);
		if (camel_exception_is_set (ex)) {
			camel_service_disconnect (service, TRUE, NULL);
			return FALSE;
		}
	}
#endif

	while (!authenticated) {
		if (errbuf) {
			/* We need to un-cache the password before prompting again */
			camel_session_query_authenticator (
				session, CAMEL_AUTHENTICATOR_TELL, NULL,
				TRUE, service, "password", ex);
			g_free (service->url->passwd);
			service->url->passwd = NULL;
		}

		if (!service->url->authmech && !service->url->passwd) {
			char *prompt;

			prompt = g_strdup_printf (_("%sPlease enter the IMAP "
						    "password for %s@%s"),
						  errbuf ? errbuf : "",
						  service->url->user,
						  service->url->host);
			service->url->passwd =
				camel_session_query_authenticator (
					session, CAMEL_AUTHENTICATOR_ASK,
					prompt, TRUE, service, "password", ex);
			g_free (prompt);
			g_free (errbuf);
			errbuf = NULL;

			if (!service->url->passwd) {
				camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL,
						     "You didn\'t enter a password.");
				camel_service_disconnect (service, TRUE, NULL);
				return FALSE;
			}
		}

		response = camel_imap_command (store, NULL, ex,
					       "LOGIN \"%s\" \"%s\"",
					       service->url->user,
					       service->url->passwd);
		if (!response) {
			errbuf = g_strdup_printf (_("Unable to authenticate "
						    "to IMAP server.\n%s\n\n"),
						  camel_exception_get_description (ex));
			camel_exception_clear (ex);
		} else {
			authenticated = TRUE;
			camel_imap_response_free (response);
		}
	}

	/* Find our storage path. */
	if (!store->storage_path) {
		store->storage_path =
			camel_session_get_storage_path (session, service, ex);
		if (camel_exception_is_set (ex)) 
			return FALSE;
	}

	/* Find the hierarchy separator for our namespace. */
	namespace = service->url->path;
	if (namespace)
		namespace++;
	else
		namespace = "";
	if (store->server_level >= IMAP_LEVEL_IMAP4REV1) {
		/* This idiom means "tell me the hierarchy separator
		 * for the given path, even if that path doesn't exist.
		 */
		response = camel_imap_command (store, NULL, ex,
					       "LIST \"%s\" \"\"",
					       namespace);
	} else {
		/* Plain IMAP4 doesn't have that idiom, so we fall back
		 * to "tell me about this folder", which will fail if
		 * the folder doesn't exist (eg, if namespace is "").
		 */
		response = camel_imap_command (store, NULL, ex,
					       "LIST \"\" \"%s\"",
					       namespace);
	}
        if (!response)
		return FALSE;

	result = camel_imap_response_extract (response, "LIST", NULL);
	if (result) {
		imap_parse_list_response (result, NULL, &store->dir_sep, NULL);
		g_free (result);
	}
	if (!store->dir_sep)
		store->dir_sep = '/';	/* Guess */

	/* Generate base URL */
	store->base_url = camel_url_to_string (service->url, FALSE);
	len = strlen (store->base_url);
	if (service->url->path)
		store->base_url[len - strlen (service->url->path) + 1] = '\0';
	else {
		store->base_url = g_realloc (store->base_url, len + 2);
		store->base_url[len] = '/';
		store->base_url[len + 1] = '\0';
	}

	camel_remote_store_refresh_folders (CAMEL_REMOTE_STORE (store), ex);

	return !camel_exception_is_set (ex);
}

static gboolean
imap_disconnect (CamelService *service, gboolean clean, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (service);
	CamelImapResponse *response;
	
	if (store->connected && clean) {
		/* send the logout command */
		response = camel_imap_command (store, NULL, ex, "LOGOUT");
		camel_imap_response_free (response);
	}
	
	store->current_folder = NULL;
	
	return CAMEL_SERVICE_CLASS (remote_store_class)->disconnect (service, clean, ex);
}

static gboolean
imap_folder_exists (CamelImapStore *store, const char *folder_name,
		    gboolean *selectable, char **short_name,
		    CamelException *ex)
{
	CamelImapResponse *response;
	char *result, sep;
	int flags;

	if (!g_strcasecmp (folder_name, "INBOX")) {
		if (selectable)
			*selectable = TRUE;
		if (short_name)
			*short_name = g_strdup ("INBOX");
		return TRUE;
	}

	response = camel_imap_command (store, NULL, ex, "LIST \"\" \"%s\"",
				       folder_name);
	if (!response)
		return FALSE;
	result = camel_imap_response_extract (response, "LIST", ex);
	if (!result)
		return FALSE;

	if (!imap_parse_list_response (result, &flags, &sep, NULL))
		return FALSE;

	if (selectable)
		*selectable = !(flags & IMAP_LIST_FLAG_NOSELECT);
	if (short_name) {
		*short_name = strrchr (folder_name, sep);
		if (*short_name)
			*short_name = g_strdup (*short_name + 1);
		else
			*short_name = g_strdup (folder_name);
	}

	return TRUE;
}

static gboolean
imap_create (CamelImapStore *store, const char *folder_name,
	     CamelException *ex)
{
	CamelImapResponse *response;

	response = camel_imap_command (store, NULL, ex, "CREATE \"%s\"",
				       folder_name);
	camel_imap_response_free (response);

	return !camel_exception_is_set (ex);
}

static CamelFolder *
get_folder (CamelStore *store, const char *folder_name, guint32 flags,
	    CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	CamelFolder *new_folder = NULL;
	char *short_name, *summary_file, *p;
	gboolean selectable;

	if (!imap_folder_exists (imap_store, folder_name,
				 &selectable, &short_name, ex)) {
		if ((flags & CAMEL_STORE_FOLDER_CREATE) == 0)
			return NULL;

		if (!imap_create (imap_store, folder_name, ex))
			return NULL;

		if (!imap_folder_exists (imap_store, folder_name,
					 &selectable, &short_name, ex))
			return NULL;
	}

	if (!selectable) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				      "%s is not a selectable folder",
				      folder_name);
		g_free (short_name);
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
						    short_name, summary_file,
						    ex);
	} else {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not create directory %s: %s"),
				      summary_file, g_strerror (errno));
	}
	g_free (summary_file);
	g_free (short_name);

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
parse_list_response_as_folder_info (CamelImapStore *imap_store,
				    const char *response)
{
	CamelFolderInfo *fi;
	int flags;
	char sep, *dir, *name = NULL;

	if (!imap_parse_list_response (response, &flags, &sep, &dir))
		return NULL;

	if (sep) {
		name = strrchr (dir, sep);
		if (name && !*++name) {
			g_free (dir);
			return NULL;
		}
	}

	fi = g_new0 (CamelFolderInfo, 1);
	fi->full_name = dir;
	if (sep && name)
		fi->name = g_strdup (name);
	else
		fi->name = g_strdup (dir);
	if (!(flags & IMAP_LIST_FLAG_NOSELECT))
		fi->url = g_strdup_printf ("%s%s", imap_store->base_url, dir);

	return fi;
}

static CamelFolderInfo *
get_folder_info (CamelStore *store, const char *top, gboolean fast,
		 gboolean recursive, gboolean subscribed_only,
		 CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	CamelURL *url = CAMEL_SERVICE (store)->url;
	gboolean need_inbox = FALSE;
	int i;
	CamelImapResponse *response;
	GPtrArray *folders;
	const char *name;
	char *pattern, *list;
	char *status, *p;
	CamelFolderInfo *topfi, *fi;

	name = top;
	if (!name) {
		need_inbox = !subscribed_only;
		if (url->path)
			name = url->path + 1;
		else
			name = "";
	}
	response = camel_imap_command (imap_store, NULL, ex,
				       "LIST \"\" \"%s\"", name);
	if (!response)
		return FALSE;
	list = camel_imap_response_extract (response, "LIST", ex);
	if (!list)
		return FALSE;
	topfi = parse_list_response_as_folder_info (imap_store, list);
	g_free (list);
	if (!topfi) {
		topfi = g_new0 (CamelFolderInfo, 1);
		topfi->full_name = g_strdup (name);
		topfi->name = g_strdup (name);
	}

	if (!top && subscribed_only)
		pattern = g_strdup ("");
 	else if (*name)
		pattern = g_strdup_printf ("%s%c", name, imap_store->dir_sep);
	else
		pattern = g_strdup (name);
	response = camel_imap_command (imap_store, NULL, ex,
				       "%s \"\" \"%s%c\"",
				       subscribed_only ? "LSUB" : "LIST",
				       pattern, recursive ? '*' : '%');
	g_free (pattern);
	if (!response)
		return NULL;

	if (subscribed_only) {
		g_hash_table_foreach_remove (imap_store->subscribed_folders,
					     free_sub, NULL);
	}

	/* Turn responses into CamelFolderInfo and remove any
	 * extraneous responses.
	 */
	folders = g_ptr_array_new ();
	for (i = 0; i < response->untagged->len; i++) {
		list = response->untagged->pdata[i];
		fi = parse_list_response_as_folder_info (imap_store, list);
		if (!fi)
			continue;
		g_ptr_array_add (folders, fi);

		if (subscribed_only) {
			g_hash_table_insert (imap_store->subscribed_folders,
					     g_strdup (fi->full_name),
					     GUINT_TO_POINTER (1));
		}

		if (!g_strcasecmp (fi->full_name, "INBOX"))
			need_inbox = FALSE;
	}
	camel_imap_response_free (response);

	/* Add INBOX, if necessary */
	if (need_inbox) {
		fi = g_new0 (CamelFolderInfo, 1);
		fi->full_name = g_strdup ("INBOX");
		fi->name = g_strdup ("INBOX");
		fi->url = g_strdup_printf ("%sINBOX", imap_store->base_url);

		g_ptr_array_add (folders, fi);
	}

	if (!fast) {
		/* Get read/unread counts */
		for (i = 0; i < folders->len; i++) {
			fi = folders->pdata[i];
			if (!fi->url)
				continue;

			response = camel_imap_command (
				imap_store, NULL, NULL,
				"STATUS \"%s\" (MESSAGES UNSEEN)",
				fi->full_name);
			if (!response)
				continue;
			status = camel_imap_response_extract (
				response, "STATUS", NULL);
			if (!status)
				continue;

			p = e_strstrcase (status, "MESSAGES");
			if (p)
				fi->message_count = strtoul (p + 8, NULL, 10);
			p = e_strstrcase (status, "UNSEEN");
			if (p)
				fi->unread_message_count = strtoul (p + 6, NULL, 10);
			g_free (status);
		}
	}

	/* And assemble */
	camel_folder_info_build (folders, topfi, imap_store->dir_sep, TRUE);
	g_ptr_array_free (folders, TRUE);

	/* Remove the top if it's the root of the store. */
	if (!top && !topfi->sibling && !topfi->url) {
		fi = topfi;
		topfi = topfi->child;
		fi->child = NULL;
		camel_folder_info_free (fi);
		for (fi = topfi; fi; fi = fi->sibling)
			fi->parent = NULL;
	}

	return topfi;
}

static gboolean
folder_subscribed (CamelStore *store, const char *folder_name)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);

	return g_hash_table_lookup (imap_store->subscribed_folders,
				    folder_name) != NULL;
}

static void
subscribe_folder (CamelStore *store, const char *folder_name,
		  CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	CamelImapResponse *response;

	response = camel_imap_command (imap_store, NULL, ex,
				       "SUBSCRIBE \"%s\"", folder_name);
	if (response) {
		g_hash_table_insert (imap_store->subscribed_folders,
				     g_strdup (folder_name),
				     GUINT_TO_POINTER (1));
	}
	camel_imap_response_free (response);
}

static void
unsubscribe_folder (CamelStore *store, const char *folder_name,
		    CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	CamelImapResponse *response;
	gpointer key, value;

	response = camel_imap_command (imap_store, NULL, ex,
				       "UNSUBSCRIBE \"%s\"", folder_name);
	if (response) {
		g_hash_table_lookup_extended (imap_store->subscribed_folders,
					      folder_name, &key, &value);
		g_hash_table_remove (imap_store->subscribed_folders,
				     folder_name);
		g_free (key);
	}
	camel_imap_response_free (response);
}

static void
imap_keepalive (CamelRemoteStore *store)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	CamelImapResponse *response;

	response = camel_imap_command (imap_store, NULL, NULL, "NOOP");
	camel_imap_response_free (response);
}
