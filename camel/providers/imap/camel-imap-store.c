/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-imap-store.c : class for an imap store */

/*
 *  Authors:
 *    Dan Winship <danw@ximian.com>
 *    Jeffrey Stedfast <fejj@danw.com>
 *
 *  Copyright 2000, 2001 Ximian, Inc.
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
#include <unistd.h>
#include <errno.h>

#include "e-util/e-path.h"

#include "camel-imap-store.h"
#include "camel-imap-folder.h"
#include "camel-imap-utils.h"
#include "camel-imap-command.h"
#include "camel-disco-diary.h"
#include "camel-file-utils.h"
#include "camel-folder.h"
#include "camel-exception.h"
#include "camel-session.h"
#include "camel-stream.h"
#include "camel-stream-buffer.h"
#include "camel-stream-fs.h"
#include "camel-url.h"
#include "camel-sasl.h"
#include "string-utils.h"

#include "camel-imap-private.h"
#include "camel-private.h"

/* Specified in RFC 2060 */
#define IMAP_PORT 143

static CamelDiscoStoreClass *disco_store_class = NULL;
static CamelRemoteStoreClass *remote_store_class = NULL;
static char imap_tag_prefix = 'A';

static void construct (CamelService *service, CamelSession *session,
		       CamelProvider *provider, CamelURL *url,
		       CamelException *ex);
static gboolean can_work_offline (CamelDiscoStore *disco_store);
static gboolean imap_connect_online (CamelService *service, CamelException *ex);
static gboolean imap_connect_offline (CamelService *service, CamelException *ex);
static gboolean imap_disconnect_online (CamelService *service, gboolean clean, CamelException *ex);
static gboolean imap_disconnect_offline (CamelService *service, gboolean clean, CamelException *ex);
static GList *query_auth_types (CamelService *service, CamelException *ex);
static guint hash_folder_name (gconstpointer key);
static gint compare_folder_name (gconstpointer a, gconstpointer b);
static CamelFolder *get_folder_online (CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex);
static CamelFolder *get_folder_offline (CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex);
static CamelFolderInfo *create_folder (CamelStore *store, const char *parent_name, const char *folder_name, CamelException *ex);
static CamelFolderInfo *get_folder_info_online (CamelStore *store,
						const char *top,
						guint32 flags,
						CamelException *ex);
static CamelFolderInfo *get_folder_info_offline (CamelStore *store,
						 const char *top,
						 guint32 flags,
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
	CamelServiceClass *camel_service_class =
		CAMEL_SERVICE_CLASS (camel_imap_store_class);
	CamelStoreClass *camel_store_class =
		CAMEL_STORE_CLASS (camel_imap_store_class);
	CamelRemoteStoreClass *camel_remote_store_class =
		CAMEL_REMOTE_STORE_CLASS (camel_imap_store_class);
	CamelDiscoStoreClass *camel_disco_store_class =
		CAMEL_DISCO_STORE_CLASS (camel_imap_store_class);

	disco_store_class = CAMEL_DISCO_STORE_CLASS (camel_type_get_global_classfuncs (camel_disco_store_get_type ()));
	remote_store_class = CAMEL_REMOTE_STORE_CLASS (camel_type_get_global_classfuncs (camel_remote_store_get_type ()));

	/* virtual method overload */
	camel_service_class->construct = construct;
	camel_service_class->query_auth_types = query_auth_types;

	camel_store_class->hash_folder_name = hash_folder_name;
	camel_store_class->compare_folder_name = compare_folder_name;
	camel_store_class->create_folder = create_folder;
	camel_store_class->free_folder_info = camel_store_free_folder_info_full;
	camel_store_class->folder_subscribed = folder_subscribed;
	camel_store_class->subscribe_folder = subscribe_folder;
	camel_store_class->unsubscribe_folder = unsubscribe_folder;

	camel_remote_store_class->keepalive = imap_keepalive;

	camel_disco_store_class->can_work_offline = can_work_offline;
	camel_disco_store_class->connect_online = imap_connect_online;
	camel_disco_store_class->connect_offline = imap_connect_offline;
	camel_disco_store_class->disconnect_online = imap_disconnect_online;
	camel_disco_store_class->disconnect_offline = imap_disconnect_offline;
	camel_disco_store_class->get_folder_online = get_folder_online;
	camel_disco_store_class->get_folder_offline = get_folder_offline;
	camel_disco_store_class->get_folder_resyncing = get_folder_online;
	camel_disco_store_class->get_folder_info_online = get_folder_info_online;
	camel_disco_store_class->get_folder_info_offline = get_folder_info_offline;
	camel_disco_store_class->get_folder_info_resyncing = get_folder_info_online;
}

static gboolean
free_key (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	return TRUE;
}

static void
camel_imap_store_finalize (CamelObject *object)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (object);

	/* This frees current_folder, folders, authtypes, and namespace. */
	imap_disconnect_offline (CAMEL_SERVICE (object), FALSE, NULL);

	if (imap_store->base_url)
		g_free (imap_store->base_url);
	if (imap_store->storage_path)
		g_free (imap_store->storage_path);

#ifdef ENABLE_THREADS
	e_mutex_destroy(imap_store->priv->command_lock);
#endif
	g_free(imap_store->priv);
}

static void
camel_imap_store_init (gpointer object, gpointer klass)
{
	CamelRemoteStore *remote_store = CAMEL_REMOTE_STORE (object);
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (object);
	
	remote_store->default_port = 143;
	remote_store->default_ssl_port = 993;
	
	imap_store->dir_sep = '\0';
	imap_store->current_folder = NULL;
	imap_store->connected = FALSE;
	imap_store->subscribed_folders = NULL;

	imap_store->tag_prefix = imap_tag_prefix++;
	if (imap_tag_prefix > 'Z')
		imap_tag_prefix = 'A';

	imap_store->priv = g_malloc0 (sizeof (*imap_store->priv));
#ifdef ENABLE_THREADS
	imap_store->priv->command_lock = e_mutex_new(E_MUTEX_REC);
#endif
}

CamelType
camel_imap_store_get_type (void)
{
	static CamelType camel_imap_store_type = CAMEL_INVALID_TYPE;
	
	if (camel_imap_store_type == CAMEL_INVALID_TYPE)	{
		camel_imap_store_type =
			camel_type_register (CAMEL_DISCO_STORE_TYPE, "CamelImapStore",
					     sizeof (CamelImapStore),
					     sizeof (CamelImapStoreClass),
					     (CamelObjectClassInitFunc) camel_imap_store_class_init,
					     NULL,
					     (CamelObjectInitFunc) camel_imap_store_init,
					     (CamelObjectFinalizeFunc) camel_imap_store_finalize);
	}
	
	return camel_imap_store_type;
}

static void
construct (CamelService *service, CamelSession *session,
	   CamelProvider *provider, CamelURL *url,
	   CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (service);
	CamelStore *store = CAMEL_STORE (service);

	CAMEL_SERVICE_CLASS (disco_store_class)->construct (service, session, provider, url, ex);
	if (camel_exception_is_set (ex))
		return;

	imap_store->storage_path = camel_session_get_storage_path (session, service, ex);
	if (!imap_store->storage_path)
		return;

	/* FIXME */
	imap_store->base_url = camel_url_to_string (service->url, (CAMEL_URL_HIDE_PASSWORD |
								   CAMEL_URL_HIDE_PARAMS |
								   CAMEL_URL_HIDE_AUTH));

	imap_store->parameters = 0;
	if (camel_url_get_param (url, "use_lsub"))
		store->flags |= CAMEL_STORE_SUBSCRIPTIONS;
	if (camel_url_get_param (url, "namespace")) {
		imap_store->parameters |= IMAP_PARAM_OVERRIDE_NAMESPACE;
		imap_store->namespace = g_strdup (camel_url_get_param (url, "namespace"));
	}
	if (camel_url_get_param (url, "check_all"))
		imap_store->parameters |= IMAP_PARAM_CHECK_ALL;
	if (camel_url_get_param (url, "filter"))
		imap_store->parameters |= IMAP_PARAM_FILTER_INBOX;
}

static void
imap_set_server_level (CamelImapStore *store)
{
	if (store->capabilities & IMAP_CAPABILITY_IMAP4REV1) {
		store->server_level = IMAP_LEVEL_IMAP4REV1;
		store->capabilities |= IMAP_CAPABILITY_STATUS;
	} else if (store->capabilities & IMAP_CAPABILITY_IMAP4)
		store->server_level = IMAP_LEVEL_IMAP4;
	else
		store->server_level = IMAP_LEVEL_UNKNOWN;
}

static struct {
	const char *name;
	guint32 flag;
} capabilities[] = {
	{ "IMAP4",		IMAP_CAPABILITY_IMAP4 },
	{ "IMAP4REV1",		IMAP_CAPABILITY_IMAP4REV1 },
	{ "STATUS",		IMAP_CAPABILITY_STATUS },
	{ "NAMESPACE",		IMAP_CAPABILITY_NAMESPACE },
	{ "UIDPLUS",		IMAP_CAPABILITY_UIDPLUS },
	{ "LITERAL+",		IMAP_CAPABILITY_LITERALPLUS },
	{ NULL, 0 }
};

/* we have remote-store:connect_lock by now */
static gboolean
connect_to_server (CamelService *service, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (service);
	CamelImapResponse *response;
	char *result, *buf, *capa, *lasts;
	int i;

	CAMEL_IMAP_STORE_ASSERT_LOCKED (store, command_lock);

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
	store->authtypes = g_hash_table_new (g_str_hash, g_str_equal);
	response = camel_imap_command (store, NULL, ex, "CAPABILITY");
	if (!response)
		return FALSE;
	result = camel_imap_response_extract (store, response, "CAPABILITY ", ex);
	if (!result)
		return FALSE;

	/* Skip over "* CAPABILITY ". */
	capa = result + 13;
	for (capa = strtok_r (capa, " ", &lasts); capa;
	     capa = strtok_r (NULL, " ", &lasts)) {
		if (!strncmp (capa, "AUTH=", 5)) {
			g_hash_table_insert (store->authtypes,
					     g_strdup (capa + 5),
					     GINT_TO_POINTER (1));
			continue;
		}
		for (i = 0; capabilities[i].name; i++) {
			if (g_strcasecmp (capa, capabilities[i].name) == 0) {
				store->capabilities |= capabilities[i].flag;
				break;
			}
		}
	}
	g_free (result);

	imap_set_server_level (store);

	return TRUE;
}

extern CamelServiceAuthType camel_imap_password_authtype;

static GList *
query_auth_types (CamelService *service, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (service);
	CamelServiceAuthType *authtype;
	GList *types, *sasl_types, *t, *next;
	gboolean connected;

	if (!camel_disco_store_check_online (CAMEL_DISCO_STORE (store), ex))
		return NULL;

	CAMEL_IMAP_STORE_LOCK (store, command_lock);
	connected = CAMEL_SERVICE_CLASS (remote_store_class)->connect (service, ex) && connect_to_server (service, ex);
	CAMEL_IMAP_STORE_UNLOCK (store, command_lock);
	if (!connected)
		return NULL;

	types = CAMEL_SERVICE_CLASS (disco_store_class)->query_auth_types (service, ex);
	if (camel_exception_is_set (ex))
		return types;

	sasl_types = camel_sasl_authtype_list (FALSE);
	for (t = sasl_types; t; t = next) {
		authtype = t->data;
		next = t->next;

		if (!g_hash_table_lookup (store->authtypes, authtype->authproto)) {
			sasl_types = g_list_remove_link (sasl_types, t);
			g_list_free_1 (t);
		}
	}
	types = g_list_concat (types, sasl_types);

	return g_list_prepend (types, &camel_imap_password_authtype);
}

/* call refresh folder directly, bypassing the folder lock */
static void
refresh_folder_info (gpointer key, gpointer value, gpointer data)
{
	CamelFolder *folder = CAMEL_FOLDER (value);

	CAMEL_IMAP_FOLDER (folder)->need_rescan = TRUE;
	CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(folder))->refresh_info(folder, data);
}

/* This is a little 'hack' to avoid the deadlock conditions that would otherwise
   ensue when calling camel_folder_refresh_info from inside a lock */
/* NB: on second thougts this is probably not entirely safe, but it'll do for now */
/* the alternative is to:
   make the camel folder->lock recursive (which should probably be done)
   or remove it from camel_folder_refresh_info, and use another locking mechanism */
static void
imap_store_refresh_folders (CamelRemoteStore *store, CamelException *ex)
{
	CAMEL_STORE_LOCK(store, cache_lock);

	g_hash_table_foreach (CAMEL_STORE (store)->folders, refresh_folder_info, ex);

	CAMEL_STORE_UNLOCK(store, cache_lock);
}	

static gboolean
try_auth (CamelImapStore *store, const char *mech, CamelException *ex)
{
	CamelSasl *sasl;
	CamelImapResponse *response;
	char *resp;
	char *sasl_resp;

	CAMEL_IMAP_STORE_ASSERT_LOCKED (store, command_lock);

	sasl = camel_sasl_new ("imap", mech, CAMEL_SERVICE (store));

	sasl_resp = camel_sasl_challenge_base64 (sasl, NULL, ex);

	response = camel_imap_command (store, NULL, ex, "AUTHENTICATE %s%s%s",
				       mech, sasl_resp ? " " : "",
				       sasl_resp ? sasl_resp : "");
	if (!response)
		goto lose;

	while (!camel_sasl_authenticated (sasl)) {
		resp = camel_imap_response_extract_continuation (store, response, ex);
		if (!resp)
			goto lose;

		sasl_resp = camel_sasl_challenge_base64 (sasl, resp + 2, ex);
		g_free (resp);
		if (camel_exception_is_set (ex))
			goto break_and_lose;

		response = camel_imap_command_continuation (store, ex, sasl_resp);
		g_free (sasl_resp);
		if (!response)
			goto lose;
	}

	resp = camel_imap_response_extract_continuation (store, response, NULL);
	if (resp) {
		/* Oops. SASL claims we're done, but the IMAP server
		 * doesn't think so...
		 */
		g_free (resp);
		goto lose;
	}
	
	camel_object_unref (CAMEL_OBJECT (sasl));
	
	return TRUE;

 break_and_lose:
	/* Get the server out of "waiting for continuation data" mode. */
	response = camel_imap_command_continuation (store, NULL, "*");
	if (response)
		camel_imap_response_free (store, response);

 lose:
	if (!camel_exception_is_set (ex)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
				     _("Bad authentication response from server."));
	}
	
	camel_object_unref (CAMEL_OBJECT (sasl));
	
	return FALSE;
}

static gboolean
imap_auth_loop (CamelService *service, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (service);
	CamelSession *session = camel_service_get_session (service);
	CamelServiceAuthType *authtype = NULL;
	CamelImapResponse *response;
	char *errbuf = NULL;
	gboolean authenticated = FALSE;

	CAMEL_IMAP_STORE_ASSERT_LOCKED (store, command_lock);

	if (service->url->authmech) {
		if (!g_hash_table_lookup (store->authtypes, service->url->authmech)) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
					      _("IMAP server %s does not support requested "
						"authentication type %s"),
					      service->url->host,
					      service->url->authmech);
			return FALSE;
		}

		authtype = camel_sasl_authtype (service->url->authmech);
		if (!authtype) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
					      _("No support for authentication type %s"),
					      service->url->authmech);
			return FALSE;
		}

		if (!authtype->need_password) {
			authenticated = try_auth (store, authtype->authproto, ex);
			if (!authenticated)
				return FALSE;
		}
	}

	while (!authenticated) {
		if (errbuf) {
			/* We need to un-cache the password before prompting again */
			camel_session_forget_password (
				session, service, "password", ex);
			g_free (service->url->passwd);
			service->url->passwd = NULL;
		}

		if (!service->url->passwd) {
			char *prompt;

			prompt = g_strdup_printf (_("%sPlease enter the IMAP "
						    "password for %s@%s"),
						  errbuf ? errbuf : "",
						  service->url->user,
						  service->url->host);
			service->url->passwd =
				camel_session_get_password (
					session, prompt, TRUE,
					service, "password", ex);
			g_free (prompt);
			g_free (errbuf);
			errbuf = NULL;

			if (!service->url->passwd) {
				camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL,
						     _("You didn't enter a password."));
				return FALSE;
			}
		}

		if (!store->connected) {
			/* Some servers (eg, courier) will disconnect on
			 * a bad password. So reconnect here.
			 */
			if (!connect_to_server (service, ex))
				return FALSE;
		}

		if (authtype)
			authenticated = try_auth (store, authtype->authproto, ex);
		else {
			response = camel_imap_command (store, NULL, ex,
						       "LOGIN %S %S",
						       service->url->user,
						       service->url->passwd);
			if (response) {
				camel_imap_response_free (store, response);
				authenticated = TRUE;
			}
		}
		if (!authenticated) {
			errbuf = g_strdup_printf (_("Unable to authenticate "
						    "to IMAP server.\n%s\n\n"),
						  camel_exception_get_description (ex));
			camel_exception_clear (ex);
		}
	}

	return TRUE;
}

#define IMAP_STOREINFO_VERSION 1

static gboolean
can_work_offline (CamelDiscoStore *disco_store)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (disco_store);
	char *path;
	gboolean can;

	path = g_strdup_printf ("%s/storeinfo", store->storage_path);
	can = access (path, F_OK) == 0;
	g_free (path);
	return can;
}

static gboolean
imap_connect_online (CamelService *service, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (service);
	CamelDiscoStore *disco_store = CAMEL_DISCO_STORE (service);
	CamelImapResponse *response;
	int i, flags, len;
	char *result, *name, *path;
	FILE *storeinfo;

	CAMEL_IMAP_STORE_LOCK (store, command_lock);
	if (!connect_to_server (service, ex) ||
	    !imap_auth_loop (service, ex)) {
		CAMEL_IMAP_STORE_UNLOCK (store, command_lock);
		camel_service_disconnect (service, TRUE, NULL);
		return FALSE;
	}

	path = g_strdup_printf ("%s/storeinfo", store->storage_path);
	storeinfo = fopen (path, "w");
	if (!storeinfo)
		g_warning ("Could not open storeinfo %s", path);
	g_free (path);

	/* Write header and capabilities */
	camel_file_util_encode_uint32 (storeinfo, IMAP_STOREINFO_VERSION);
	camel_file_util_encode_uint32 (storeinfo, store->capabilities);

	/* Get namespace and hierarchy separator */
	if ((store->capabilities & IMAP_CAPABILITY_NAMESPACE) &&
	    !(store->parameters & IMAP_PARAM_OVERRIDE_NAMESPACE)) {
		response = camel_imap_command (store, NULL, ex, "NAMESPACE");
		if (!response)
			goto done;

		result = camel_imap_response_extract (store, response, "NAMESPACE", ex);
		if (!result)
			goto done;

		name = strstrcase (result, "NAMESPACE ((");
		if (name) {
			char *sep;

			name += 12;
			store->namespace = imap_parse_string (&name, &len);
			if (name && *name++ == ' ') {
				sep = imap_parse_string (&name, &len);
				if (sep) {
					store->dir_sep = *sep;
					g_free (sep);
				}
			}
		}
		g_free (result);
	}
	if (!store->namespace)
		store->namespace = g_strdup ("");

	if (!store->dir_sep) {
		if (store->server_level >= IMAP_LEVEL_IMAP4REV1) {
			/* This idiom means "tell me the hierarchy separator
			 * for the given path, even if that path doesn't exist.
			 */
			response = camel_imap_command (store, NULL, ex,
						       "LIST %S \"\"",
						       store->namespace);
		} else {
			/* Plain IMAP4 doesn't have that idiom, so we fall back
			 * to "tell me about this folder", which will fail if
			 * the folder doesn't exist (eg, if namespace is "").
			 */
			response = camel_imap_command (store, NULL, ex,
						       "LIST \"\" %S",
						       store->namespace);
		}
		if (!response)
			goto done;

		result = camel_imap_response_extract (store, response, "LIST", NULL);
		if (result) {
			imap_parse_list_response (result, NULL, &store->dir_sep, NULL);
			g_free (result);
		}
		if (!store->dir_sep)
			store->dir_sep = '/';	/* Guess */
	}

	/* Write namespace/separator out */
	camel_file_util_encode_string (storeinfo, store->namespace);
	camel_file_util_encode_uint32 (storeinfo, store->dir_sep);

	if (CAMEL_STORE (store)->flags & CAMEL_STORE_SUBSCRIPTIONS) {
		/* Get subscribed folders */
		response = camel_imap_command (store, NULL, ex, "LSUB \"\" \"*\"");
		if (!response)
			goto done;
		store->subscribed_folders = g_hash_table_new (g_str_hash, g_str_equal);
		for (i = 0; i < response->untagged->len; i++) {
			result = response->untagged->pdata[i];
			if (!imap_parse_list_response (result, &flags, NULL, &name))
				continue;
			if (flags & (IMAP_LIST_FLAG_MARKED | IMAP_LIST_FLAG_UNMARKED))
				store->capabilities |= IMAP_CAPABILITY_useful_lsub;
			if (flags & IMAP_LIST_FLAG_NOSELECT) {
				g_free (name);
				continue;
			}
			g_hash_table_insert (store->subscribed_folders, name,
					     GINT_TO_POINTER (1));
			camel_file_util_encode_string (storeinfo, result);
		}
		camel_imap_response_free (store, response);
	}

	path = g_strdup_printf ("%s/journal", store->storage_path);
	disco_store->diary = camel_disco_diary_new (disco_store, path, ex);
	g_free (path);

 done:
	fclose (storeinfo);
	CAMEL_IMAP_STORE_UNLOCK (store, command_lock);

	if (camel_exception_is_set (ex))
		camel_service_disconnect (service, TRUE, NULL);
	else
		imap_store_refresh_folders (CAMEL_REMOTE_STORE (store), ex);
	return !camel_exception_is_set (ex);
}

static gboolean
imap_connect_offline (CamelService *service, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (service);
	CamelDiscoStore *disco_store = CAMEL_DISCO_STORE (service);
	char *buf, *name, *path;
	FILE *storeinfo;
	guint32 tmp;

	path = g_strdup_printf ("%s/journal", store->storage_path);
	disco_store->diary = camel_disco_diary_new (disco_store, path, ex);
	g_free (path);
	if (!disco_store->diary)
		return FALSE;

	path = g_strdup_printf ("%s/storeinfo", store->storage_path);
	storeinfo = fopen (path, "r");
	g_free (path);
	tmp = 0;
	if (storeinfo)
		camel_file_util_decode_uint32 (storeinfo, &tmp);
	if (tmp != IMAP_STOREINFO_VERSION) {
		if (storeinfo)
			fclose (storeinfo);

		/* We know we're offline, so this will have to set ex
		 * and return FALSE.
		 */
		return camel_disco_store_check_online (CAMEL_DISCO_STORE (store), ex);
	}

	camel_file_util_decode_uint32 (storeinfo, &store->capabilities);
	imap_set_server_level (store);
	camel_file_util_decode_string (storeinfo, &store->namespace);
	camel_file_util_decode_uint32 (storeinfo, &tmp);
	store->dir_sep = tmp;

	/* Get subscribed folders */
	store->subscribed_folders = g_hash_table_new (g_str_hash, g_str_equal);
	while (camel_file_util_decode_string (storeinfo, &buf) == 0) {
		if (!imap_parse_list_response (buf, NULL, NULL, &name)) {
			g_free (buf);
			continue;
		}
		g_hash_table_insert (store->subscribed_folders, name,
				     GINT_TO_POINTER (1));
		g_free (buf);
	}

	fclose (storeinfo);
	imap_store_refresh_folders (CAMEL_REMOTE_STORE (store), ex);

	store->connected = !camel_exception_is_set (ex);
	return store->connected;
}

static gboolean
imap_disconnect_offline (CamelService *service, gboolean clean, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (service);
	CamelDiscoStore *disco = CAMEL_DISCO_STORE (service);

	store->connected = FALSE;
	if (store->current_folder) {
		camel_object_unref (CAMEL_OBJECT (store->current_folder));
		store->current_folder = NULL;
	}

	if (store->subscribed_folders) {
		g_hash_table_foreach_remove (store->subscribed_folders,
					     free_key, NULL);
		g_hash_table_destroy (store->subscribed_folders);
		store->subscribed_folders = NULL;
	}

	if (store->authtypes) {
		g_hash_table_foreach_remove (store->authtypes,
					     free_key, NULL);
		g_hash_table_destroy (store->authtypes);
		store->authtypes = NULL;
	}

	if (store->namespace && !(store->parameters & IMAP_PARAM_OVERRIDE_NAMESPACE)) {
		g_free (store->namespace);
		store->namespace = NULL;
	}

	if (disco->diary) {
		camel_object_unref (CAMEL_OBJECT (disco->diary));
		disco->diary = NULL;
	}

	return TRUE;
}

static gboolean
imap_disconnect_online (CamelService *service, gboolean clean, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (service);
	CamelImapResponse *response;
	
	if (store->connected && clean) {
		response = camel_imap_command (store, NULL, ex, "LOGOUT");
		camel_imap_response_free (store, response);
	}
	imap_disconnect_offline (service, clean, ex);

	return TRUE;
}

static guint
hash_folder_name (gconstpointer key)
{
	if (g_strcasecmp (key, "INBOX") == 0)
		return g_str_hash ("INBOX");
	else
		return g_str_hash (key);
}

static gint
compare_folder_name (gconstpointer a, gconstpointer b)
{
	gconstpointer aname = a, bname = b;

	if (g_strcasecmp (a, "INBOX") == 0)
		aname = "INBOX";
	if (g_strcasecmp (b, "INBOX") == 0)
		bname = "INBOX";
	return g_str_equal (aname, bname);
}

static CamelFolder *
no_such_folder (const char *name, CamelException *ex)
{
	camel_exception_setv (ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
			      _("No such folder %s"), name);
	return NULL;
}

static CamelFolder *
get_folder_online (CamelStore *store, const char *folder_name,
		   guint32 flags, CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	CamelImapResponse *response;
	CamelFolder *new_folder;
	char *folder_dir;

	if (!camel_remote_store_connected (CAMEL_REMOTE_STORE (store), ex))
		return NULL;

	if (!g_strcasecmp (folder_name, "INBOX"))
		folder_name = "INBOX";

	/* Lock around the whole lot to check/create atomically */
	CAMEL_IMAP_STORE_LOCK (imap_store, command_lock);
	if (imap_store->current_folder) {
		camel_object_unref (CAMEL_OBJECT (imap_store->current_folder));
		imap_store->current_folder = NULL;
	}
	response = camel_imap_command (imap_store, NULL, NULL,
				       "SELECT %S", folder_name);
	if (!response) {
		if (!flags & CAMEL_STORE_FOLDER_CREATE)
			return no_such_folder (folder_name, ex);

		response = camel_imap_command (imap_store, NULL, ex,
					       "CREATE %S", folder_name);
		if (response) {
			camel_imap_response_free (imap_store, response);

			response = camel_imap_command (imap_store, NULL, NULL,
						       "SELECT %S", folder_name);
		}
		if (!response) {
			CAMEL_IMAP_STORE_UNLOCK (imap_store, command_lock);
			return NULL;
		}
	}

	folder_dir = e_path_to_physical (imap_store->storage_path, folder_name);
	new_folder = camel_imap_folder_new (store, folder_name, folder_dir, ex);
	g_free (folder_dir);
	if (new_folder) {
		imap_store->current_folder = new_folder;
		camel_imap_folder_selected (new_folder, response, ex);
		if (camel_exception_is_set (ex)) {
			camel_object_unref (CAMEL_OBJECT (new_folder));
			new_folder = imap_store->current_folder = NULL;
		} else
			camel_object_ref (CAMEL_OBJECT (imap_store->current_folder));
	}
	camel_imap_response_free_without_processing (imap_store, response);

	CAMEL_IMAP_STORE_UNLOCK (imap_store, command_lock);

	return new_folder;
}

static CamelFolder *
get_folder_offline (CamelStore *store, const char *folder_name,
		    guint32 flags, CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	CamelFolder *new_folder;
	char *folder_dir;

	if (!imap_store->connected &&
	    !camel_service_connect (CAMEL_SERVICE (store), ex))
		return NULL;

	if (!g_strcasecmp (folder_name, "INBOX"))
		folder_name = "INBOX";

	folder_dir = e_path_to_physical (imap_store->storage_path, folder_name);
	if (access (folder_dir, F_OK) != 0)
		return no_such_folder (folder_name, ex);

	new_folder = camel_imap_folder_new (store, folder_name, folder_dir, ex);
	g_free (folder_dir);

	return new_folder;
}

static char *
imap_concat (CamelImapStore *imap_store, const char *prefix, const char *suffix)
{
	int len;
	
	len = strlen (prefix);
	if (len == 0 || prefix[len - 1] == imap_store->dir_sep)
		return g_strdup_printf ("%s%s", prefix, suffix);
	else
		return g_strdup_printf ("%s%c%s", prefix, imap_store->dir_sep, suffix);
}

static CamelFolderInfo *
create_folder (CamelStore *store, const char *parent_name,
	       const char *folder_name, CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	CamelImapResponse *response;
	CamelFolderInfo *fi;
	char *full_name;

	if (!camel_disco_store_check_online (CAMEL_DISCO_STORE (store), ex))
		return NULL;
	if (!parent_name)
		parent_name = imap_store->namespace;
	full_name = imap_concat (imap_store, parent_name, folder_name);

	response = camel_imap_command (imap_store, NULL, ex, "CREATE %S",
				       full_name);
	if (response) {
		camel_imap_response_free (imap_store, response);
		fi = get_folder_info_online (store, full_name, 0, ex);
	} else
		fi = NULL;

	g_free (full_name);
	return fi;
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
	if (!(flags & IMAP_LIST_FLAG_NOSELECT)) {
		CamelURL *url;
		
		url = camel_url_new (imap_store->base_url, NULL);
		g_free (url->path);
		url->path = g_strdup_printf ("/%s", dir);
		fi->url = camel_url_to_string (url, 0);
		camel_url_free (url);
	}
	if (!(flags & IMAP_LIST_FLAG_UNMARKED))
		fi->unread_message_count = -1;

	return fi;
}

static void
copy_folder_name (gpointer name, gpointer key, gpointer array)
{
	g_ptr_array_add (array, name);
}

static void
get_subscribed_folders_by_hand (CamelImapStore *imap_store, const char *top,
				GPtrArray *folders, CamelException *ex)
{
	GPtrArray *names;
	CamelImapResponse *response;
	CamelFolderInfo *fi;
	char *result;
	int i, toplen = strlen (top);

	names = g_ptr_array_new ();
	g_hash_table_foreach (imap_store->subscribed_folders,
			      copy_folder_name, names);

	for (i = 0; i < names->len; i++) {
		response = camel_imap_command (imap_store, NULL, ex,
					       "LIST \"\" %S",
					       names->pdata[i]);
		if (!response) {
			g_ptr_array_free (names, TRUE);
			return;
		}
		result = camel_imap_response_extract (imap_store, response, "LIST", NULL);
		if (!result) {
			g_hash_table_remove (imap_store->subscribed_folders,
					     names->pdata[i]);
			g_free (names->pdata[i]);
			g_ptr_array_remove_index_fast (names, i--);
			continue;
		}

		fi = parse_list_response_as_folder_info (imap_store, result);
		if (!fi)
			continue;

		if (strncmp (top, fi->full_name, toplen) != 0) {
			camel_folder_info_free (fi);
			continue;
		}

		g_ptr_array_add (folders, fi);
	}
	g_ptr_array_free (names, TRUE);
}

static void
get_folders_online (CamelImapStore *imap_store, const char *pattern,
		    GPtrArray *folders, gboolean lsub, CamelException *ex)
{
	CamelImapResponse *response;
	CamelFolderInfo *fi;
	char *list;
	int i;

	response = camel_imap_command (imap_store, NULL, ex,
				       "%s \"\" %S", lsub ? "LSUB" : "LIST",
				       pattern);
	if (!response)
		return;

	for (i = 0; i < response->untagged->len; i++) {
		list = response->untagged->pdata[i];
		fi = parse_list_response_as_folder_info (imap_store, list);
		if (fi)
			g_ptr_array_add (folders, fi);
	}
	camel_imap_response_free (imap_store, response);
}

static CamelFolderInfo *
get_folder_info_online (CamelStore *store, const char *top,
			guint32 flags, CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	CamelImapResponse *response;
	gboolean need_inbox = FALSE;
	GPtrArray *folders;
	const char *name;
	char *pattern, *status, *p;
	CamelFolderInfo *fi, *tree;
	int i;

	if (!camel_remote_store_connected (CAMEL_REMOTE_STORE (store), ex))
		return NULL;

	name = top;
	if (!name) {
		need_inbox = TRUE;
		name = imap_store->namespace;
	}

	folders = g_ptr_array_new ();

	/* Get top-level */
	get_folders_online (imap_store, name, folders, FALSE, ex);
	if (camel_exception_is_set (ex))
		goto lose;
	if (folders->len) {
		fi = folders->pdata[0];
		if (!fi->url) {
			camel_folder_info_free (fi);
			g_ptr_array_remove_index (folders, 0);
		}
	}

	/* If we want to look at only subscribed folders AND check if
	 * any of them have new mail, AND the server doesn't return
	 * Marked/UnMarked with LSUB, then use
	 * get_subscribed_folders_by_hand. In all other cases, use a
	 * single LIST or LSUB command.
	 */
	if ((flags & CAMEL_STORE_FOLDER_INFO_SUBSCRIBED) &&
	    !(imap_store->capabilities & IMAP_CAPABILITY_useful_lsub) &&
	    (imap_store->parameters & IMAP_PARAM_CHECK_ALL)) {
		get_subscribed_folders_by_hand (imap_store, name, folders, ex);
	} else {
		pattern = imap_concat (imap_store, name, (flags & CAMEL_STORE_FOLDER_INFO_RECURSIVE) ? "*" : "%");
		get_folders_online (imap_store, pattern, folders, (flags & CAMEL_STORE_FOLDER_INFO_SUBSCRIBED), ex);
		g_free (pattern);
	}
	if (camel_exception_is_set (ex)) {
	lose:
		for (i = 0; i < folders->len; i++)
			camel_folder_info_free (folders->pdata[i]);
		g_ptr_array_free (folders, TRUE);
		return NULL;
	}

	/* Add INBOX, if necessary */
	if (need_inbox) {
		for (i = 0; i < folders->len; i++) {
			fi = folders->pdata[i];
			if (!g_strcasecmp (fi->full_name, "INBOX")) {
				need_inbox = FALSE;
				break;
			}
		}

		if (need_inbox) {
			CamelURL *url;
			char *uri;

			url = camel_url_new (imap_store->base_url, NULL);
			g_free (url->path);
			url->path = g_strdup ("/INBOX");
			uri = camel_url_to_string (url, 0);
			camel_url_free (url);

			fi = g_new0 (CamelFolderInfo, 1);
			fi->full_name = g_strdup ("INBOX");
			fi->name = g_strdup ("INBOX");
			fi->url = uri;
			fi->unread_message_count = -1;

			g_ptr_array_add (folders, fi);
		}
	}

	/* Assemble. */
	tree = camel_folder_info_build (folders, name, imap_store->dir_sep, TRUE);
	if (flags & CAMEL_STORE_FOLDER_INFO_FAST) {
		g_ptr_array_free (folders, TRUE);
		return tree;
	}

	/* Get unread counts. Sync flag changes to the server first so
	 * it has the same ideas about read/unread as we do.
	 */
	camel_store_sync (store, NULL);
	for (i = 0; i < folders->len; i++) {
		fi = folders->pdata[i];

		/* Don't check if it doesn't contain messages or if it
		 * was \UnMarked.
		 */
		if (!fi->url || fi->unread_message_count != -1)
			continue;
		/* Don't check if it's not INBOX and we're only
		 * checking INBOX.
		 */
		if ((!(imap_store->parameters & IMAP_PARAM_CHECK_ALL))
		    && (g_strcasecmp (fi->name, "INBOX") != 0))
			continue;

		/* UW will give cached data for the currently selected
		 * folder. Grr. Well, I guess this also potentially
		 * saves us one IMAP command.
		 */
		if (imap_store->current_folder &&
		    !strcmp (imap_store->current_folder->full_name, fi->full_name)) {
			fi->unread_message_count = camel_folder_get_unread_message_count (imap_store->current_folder);
			continue;
		}

		response = camel_imap_command (imap_store, NULL, NULL,
					       "STATUS %S (UNSEEN)",
					       fi->full_name);
		if (!response)
			continue;
		status = camel_imap_response_extract (imap_store, response,
						      "STATUS", NULL);
		if (!status)
			continue;

		p = strstrcase (status, "UNSEEN");
		if (p)
			fi->unread_message_count = strtoul (p + 6, NULL, 10);
		g_free (status);
	}

	g_ptr_array_free (folders, TRUE);
	return tree;
}

static gboolean
get_one_folder_offline (const char *physical_path, const char *path, gpointer data)
{
	GPtrArray *folders = data;
	CamelImapStore *imap_store = folders->pdata[0];
	CamelFolderInfo *fi;

	if (*path++ != '/')
		return TRUE;

	fi = g_new0 (CamelFolderInfo, 1);
	fi->full_name = g_strdup (path);
	fi->name = strrchr (fi->full_name, imap_store->dir_sep);
	if (fi->name)
		fi->name = g_strdup (fi->name + 1);
	else
		fi->name = g_strdup (fi->full_name);

	fi->url = g_strdup_printf ("%s/%s", imap_store->base_url, path);

	/* FIXME: check summary */
	fi->unread_message_count = -1;

	g_ptr_array_add (folders, fi);
	return TRUE;
}

static CamelFolderInfo *
get_folder_info_offline (CamelStore *store, const char *top,
			 guint32 flags, CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	CamelFolderInfo *fi;
	GPtrArray *folders;

	if (!imap_store->connected &&
	    !camel_service_connect (CAMEL_SERVICE (store), ex))
		return NULL;

	if ((store->flags & CAMEL_STORE_SUBSCRIPTIONS) &&
	    !(flags & CAMEL_STORE_FOLDER_INFO_SUBSCRIBED)) {
		camel_disco_store_check_online (CAMEL_DISCO_STORE (store), ex);
		return NULL;
	}

	/* FIXME: obey other flags */

	folders = g_ptr_array_new ();

	/* A kludge to avoid having to pass a struct to the callback */
	g_ptr_array_add (folders, imap_store);
	if (!e_path_find_folders (imap_store->storage_path, get_one_folder_offline, folders)) {
		camel_disco_store_check_online (CAMEL_DISCO_STORE (imap_store), ex);
		fi = NULL;
	} else {
		g_ptr_array_remove_index_fast (folders, 0);
		fi = camel_folder_info_build (folders, imap_store->namespace,
					      imap_store->dir_sep, TRUE);
	}

	g_ptr_array_free (folders, TRUE);
	return fi;
}

static gboolean
folder_subscribed (CamelStore *store, const char *folder_name)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);

	g_return_val_if_fail (imap_store->subscribed_folders != NULL, FALSE);

	return g_hash_table_lookup (imap_store->subscribed_folders,
				    folder_name) != NULL;
}

static void
subscribe_folder (CamelStore *store, const char *folder_name,
		  CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	CamelImapResponse *response;
	CamelFolderInfo *fi;
	char *name;

	if (!camel_disco_store_check_online (CAMEL_DISCO_STORE (store), ex))
		return;
	if (!camel_remote_store_connected (CAMEL_REMOTE_STORE (store), ex))
		return;

	response = camel_imap_command (imap_store, NULL, ex,
				       "SUBSCRIBE %S", folder_name);
	if (!response)
		return;
	camel_imap_response_free (imap_store, response);

	g_hash_table_insert (imap_store->subscribed_folders,
			     g_strdup (folder_name), GUINT_TO_POINTER (1));

	name = strrchr (folder_name, imap_store->dir_sep);
	if (name)
		name++;

	fi = g_new0 (CamelFolderInfo, 1);
	fi->full_name = g_strdup (folder_name);
	fi->name = g_strdup (name);
	fi->url = g_strdup_printf ("%s/%s", imap_store->base_url, folder_name);
	fi->unread_message_count = -1;

	camel_object_trigger_event (CAMEL_OBJECT (store), "folder_created", fi);
	camel_folder_info_free (fi);
}

static void
unsubscribe_folder (CamelStore *store, const char *folder_name,
		    CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	CamelImapResponse *response;
	gpointer key, value;
	CamelFolderInfo *fi;
	char *name;

	if (!camel_disco_store_check_online (CAMEL_DISCO_STORE (store), ex))
		return;
	if (!camel_remote_store_connected (CAMEL_REMOTE_STORE (store), ex))
		return;

	response = camel_imap_command (imap_store, NULL, ex,
				       "UNSUBSCRIBE %S", folder_name);
	if (!response)
		return;
	camel_imap_response_free (imap_store, response);

	g_hash_table_lookup_extended (imap_store->subscribed_folders,
				      folder_name, &key, &value);
	g_hash_table_remove (imap_store->subscribed_folders, folder_name);
	g_free (key);

	name = strrchr (folder_name, imap_store->dir_sep);
	if (name)
		name++;

	fi = g_new0 (CamelFolderInfo, 1);
	fi->full_name = g_strdup (folder_name);
	fi->name = g_strdup (name);
	fi->url = g_strdup_printf ("%s/%s", imap_store->base_url, folder_name);
	fi->unread_message_count = -1;

	camel_object_trigger_event (CAMEL_OBJECT (store), "folder_deleted", fi);
	camel_folder_info_free (fi);
}

static void
imap_keepalive (CamelRemoteStore *store)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	CamelImapResponse *response;

	response = camel_imap_command (imap_store, NULL, NULL, "NOOP");
	camel_imap_response_free (imap_store, response);
}
