/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-imap-store.c : class for an imap store */

/*
 *  Authors:
 *    Dan Winship <danw@ximian.com>
 *    Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2000, 2001 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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
#include "camel-imap-summary.h"
#include "camel-imap-message-cache.h"
#include "camel-disco-diary.h"
#include "camel-file-utils.h"
#include "camel-folder.h"
#include "camel-exception.h"
#include "camel-session.h"
#include "camel-stream.h"
#include "camel-stream-buffer.h"
#include "camel-stream-fs.h"
#include "camel-tcp-stream-raw.h"
#include "camel-tcp-stream-ssl.h"
#include "camel-url.h"
#include "camel-sasl.h"
#include "string-utils.h"

#include "camel-imap-private.h"
#include "camel-private.h"

#define d(x) 

/* Specified in RFC 2060 */
#define IMAP_PORT 143
#define SIMAP_PORT 993

extern int camel_verbose_debug;

static CamelDiscoStoreClass *parent_class = NULL;

static char imap_tag_prefix = 'A';

static void construct (CamelService *service, CamelSession *session,
		       CamelProvider *provider, CamelURL *url,
		       CamelException *ex);

static int imap_setv (CamelObject *object, CamelException *ex, CamelArgV *args);
static int imap_getv (CamelObject *object, CamelException *ex, CamelArgGetV *args);

static char *imap_get_name (CamelService *service, gboolean brief);

static gboolean can_work_offline (CamelDiscoStore *disco_store);
static gboolean imap_connect_online (CamelService *service, CamelException *ex);
static gboolean imap_connect_offline (CamelService *service, CamelException *ex);
static gboolean imap_disconnect_online (CamelService *service, gboolean clean, CamelException *ex);
static gboolean imap_disconnect_offline (CamelService *service, gboolean clean, CamelException *ex);
static void imap_noop (CamelStore *store, CamelException *ex);
static GList *query_auth_types (CamelService *service, CamelException *ex);
static guint hash_folder_name (gconstpointer key);
static gint compare_folder_name (gconstpointer a, gconstpointer b);
static CamelFolder *get_folder_online (CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex);
static CamelFolder *get_folder_offline (CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex);
static CamelFolderInfo *create_folder (CamelStore *store, const char *parent_name, const char *folder_name, CamelException *ex);
static void             delete_folder (CamelStore *store, const char *folder_name, CamelException *ex);
static void             rename_folder (CamelStore *store, const char *old_name, const char *new_name, CamelException *ex);
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

static void get_folders_online (CamelImapStore *imap_store, const char *pattern,
				GPtrArray *folders, gboolean lsub, CamelException *ex);


static void imap_folder_effectively_unsubscribed(CamelImapStore *imap_store, 
						 const char *folder_name, CamelException *ex);

static gboolean imap_check_folder_still_extant (CamelImapStore *imap_store, const char *full_name, 
					    CamelException *ex);

static void imap_forget_folder(CamelImapStore *imap_store, const char *folder_name,
			       CamelException *ex);

static void
camel_imap_store_class_init (CamelImapStoreClass *camel_imap_store_class)
{
	CamelObjectClass *camel_object_class =
		CAMEL_OBJECT_CLASS (camel_imap_store_class);
	CamelServiceClass *camel_service_class =
		CAMEL_SERVICE_CLASS (camel_imap_store_class);
	CamelStoreClass *camel_store_class =
		CAMEL_STORE_CLASS (camel_imap_store_class);
	CamelDiscoStoreClass *camel_disco_store_class =
		CAMEL_DISCO_STORE_CLASS (camel_imap_store_class);
	
	parent_class = CAMEL_DISCO_STORE_CLASS (camel_type_get_global_classfuncs (camel_disco_store_get_type ()));
	
	/* virtual method overload */
	camel_object_class->setv = imap_setv;
	camel_object_class->getv = imap_getv;
	
	camel_service_class->construct = construct;
	camel_service_class->query_auth_types = query_auth_types;
	camel_service_class->get_name = imap_get_name;
	
	camel_store_class->hash_folder_name = hash_folder_name;
	camel_store_class->compare_folder_name = compare_folder_name;
	camel_store_class->create_folder = create_folder;
	camel_store_class->delete_folder = delete_folder;
	camel_store_class->rename_folder = rename_folder;
	camel_store_class->free_folder_info = camel_store_free_folder_info_full;
	camel_store_class->folder_subscribed = folder_subscribed;
	camel_store_class->subscribe_folder = subscribe_folder;
	camel_store_class->unsubscribe_folder = unsubscribe_folder;
	camel_store_class->noop = imap_noop;
	
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
	
	if (imap_store->istream)
		camel_object_unref (CAMEL_OBJECT (imap_store->istream));
	
	if (imap_store->ostream)
		camel_object_unref (CAMEL_OBJECT (imap_store->ostream));
	
	/* This frees current_folder, folders, authtypes, and namespace. */
	imap_disconnect_offline (CAMEL_SERVICE (object), FALSE, NULL);
	
	if (imap_store->base_url)
		g_free (imap_store->base_url);
	if (imap_store->storage_path)
		g_free (imap_store->storage_path);
	
#ifdef ENABLE_THREADS
	e_mutex_destroy (imap_store->priv->command_lock);
	e_thread_destroy (imap_store->async_thread);
#endif
	g_free (imap_store->priv);
}

#ifdef ENABLE_THREADS
static void async_destroy(EThread *et, EMsg *em, void *data)
{
	CamelImapStore *imap_store = data;
	CamelImapMsg *msg = (CamelImapMsg *)em;
	
	if (msg->free)
		msg->free (imap_store, msg);
	
	g_free (msg);
}

static void async_received(EThread *et, EMsg *em, void *data)
{
	CamelImapStore *imap_store = data;
	CamelImapMsg *msg = (CamelImapMsg *)em;

	if (msg->receive)
		msg->receive(imap_store, msg);
}

CamelImapMsg *camel_imap_msg_new(void (*receive)(CamelImapStore *store, struct _CamelImapMsg *m),
				 void (*free)(CamelImapStore *store, struct _CamelImapMsg *m),
				 size_t size)
{
	CamelImapMsg *msg;

	g_assert(size >= sizeof(*msg));

	msg = g_malloc0(size);
	msg->receive = receive;
	msg->free = free;

	return msg;
}

void camel_imap_msg_queue(CamelImapStore *store, CamelImapMsg *msg)
{
	e_thread_put(store->async_thread, (EMsg *)msg);
}

#endif

static void
camel_imap_store_init (gpointer object, gpointer klass)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (object);
	
	imap_store->istream = NULL;
	imap_store->ostream = NULL;
	
	imap_store->dir_sep = '\0';
	imap_store->current_folder = NULL;
	imap_store->connected = FALSE;
	imap_store->subscribed_folders = NULL;
	
	imap_store->tag_prefix = imap_tag_prefix++;
	if (imap_tag_prefix > 'Z')
		imap_tag_prefix = 'A';
	
	imap_store->priv = g_malloc0 (sizeof (*imap_store->priv));
#ifdef ENABLE_THREADS
	imap_store->priv->command_lock = e_mutex_new (E_MUTEX_REC);
	imap_store->async_thread = e_thread_new(E_THREAD_QUEUE);
	e_thread_set_msg_destroy(imap_store->async_thread, async_destroy, imap_store);
	e_thread_set_msg_received(imap_store->async_thread, async_received, imap_store);
#endif
}

CamelType
camel_imap_store_get_type (void)
{
	static CamelType camel_imap_store_type = CAMEL_INVALID_TYPE;
	
	if (camel_imap_store_type == CAMEL_INVALID_TYPE)	{
		camel_imap_store_type =
			camel_type_register (CAMEL_DISCO_STORE_TYPE,
					     "CamelImapStore",
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

	CAMEL_SERVICE_CLASS (parent_class)->construct (service, session, provider, url, ex);
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
	if (camel_url_get_param (url, "filter")) {
		imap_store->parameters |= IMAP_PARAM_FILTER_INBOX;
		store->flags |= CAMEL_STORE_FILTER_INBOX;
	}
}

static int
imap_setv (CamelObject *object, CamelException *ex, CamelArgV *args)
{
	CamelImapStore *store = (CamelImapStore *) object;
	guint32 tag, flags;
	int i;
	
	for (i = 0; i < args->argc; i++) {
		tag = args->argv[i].tag;
		
		/* make sure this arg wasn't already handled */
		if (tag & CAMEL_ARG_IGNORE)
			continue;
		
		/* make sure this is an arg we're supposed to handle */
		if ((tag & CAMEL_ARG_TAG) <= CAMEL_IMAP_STORE_ARG_FIRST ||
		    (tag & CAMEL_ARG_TAG) >= CAMEL_IMAP_STORE_ARG_FIRST + 100)
			continue;
		
		if (tag == CAMEL_IMAP_STORE_NAMESPACE) {
			if (strcmp (store->namespace, args->argv[i].ca_str) != 0) {
				g_free (store->namespace);
				store->namespace = g_strdup (args->argv[i].ca_str);
				/* the current imap code will need to do a reconnect for this to take effect */
				/*reconnect = TRUE;*/
			}
		} else if (tag == CAMEL_IMAP_STORE_OVERRIDE_NAMESPACE) {
			flags = args->argv[i].ca_int ? IMAP_PARAM_OVERRIDE_NAMESPACE : 0;
			flags |= (store->parameters & ~IMAP_PARAM_OVERRIDE_NAMESPACE);
			
			if (store->parameters != flags) {
				store->parameters = flags;
				/* the current imap code will need to do a reconnect for this to take effect */
				/*reconnect = TRUE;*/
			}
		} else if (tag == CAMEL_IMAP_STORE_CHECK_ALL) {
			flags = args->argv[i].ca_int ? IMAP_PARAM_CHECK_ALL : 0;
			flags |= (store->parameters & ~IMAP_PARAM_CHECK_ALL);
			store->parameters = flags;
			/* no need to reconnect for this option to take effect... */
		} else if (tag == CAMEL_IMAP_STORE_FILTER_INBOX) {
			flags = args->argv[i].ca_int ? IMAP_PARAM_FILTER_INBOX : 0;
			flags |= (store->parameters & ~IMAP_PARAM_FILTER_INBOX);
			store->parameters = flags;
			/* no need to reconnect for this option to take effect... */
		} else {
			/* error?? */
			continue;
		}
		
		/* let our parent know that we've handled this arg */
		camel_argv_ignore (args, i);
	}
	
	/* FIXME: if we need to reconnect for a change to take affect,
           we need to do it here... or, better yet, somehow chain it
           up to CamelService's setv implementation. */
	
	return CAMEL_OBJECT_CLASS (parent_class)->setv (object, ex, args);
}

static int
imap_getv (CamelObject *object, CamelException *ex, CamelArgGetV *args)
{
	CamelImapStore *store = (CamelImapStore *) object;
	guint32 tag;
	int i;
	
	for (i = 0; i < args->argc; i++) {
		tag = args->argv[i].tag;
		
		/* make sure this is an arg we're supposed to handle */
		if ((tag & CAMEL_ARG_TAG) <= CAMEL_IMAP_STORE_ARG_FIRST ||
		    (tag & CAMEL_ARG_TAG) >= CAMEL_IMAP_STORE_ARG_FIRST + 100)
			continue;
		
		switch (tag) {
		case CAMEL_IMAP_STORE_NAMESPACE:
			/* get the username */
			*args->argv[i].ca_str = store->namespace;
			break;
		case CAMEL_IMAP_STORE_OVERRIDE_NAMESPACE:
			/* get the auth mechanism */
			*args->argv[i].ca_int = store->parameters & IMAP_PARAM_OVERRIDE_NAMESPACE ? TRUE : FALSE;
			break;
		case CAMEL_IMAP_STORE_CHECK_ALL:
			/* get the hostname */
			*args->argv[i].ca_int = store->parameters & IMAP_PARAM_CHECK_ALL ? TRUE : FALSE;
			break;
		case CAMEL_IMAP_STORE_FILTER_INBOX:
			/* get the port */
			*args->argv[i].ca_int = store->parameters & IMAP_PARAM_FILTER_INBOX ? TRUE : FALSE;
			break;
		default:
			/* error? */
		}
	}
	
	return CAMEL_OBJECT_CLASS (parent_class)->getv (object, ex, args);
}

static char *
imap_get_name (CamelService *service, gboolean brief)
{
	if (brief)
		return g_strdup_printf (_("IMAP server %s"), service->url->host);
	else
		return g_strdup_printf (_("IMAP service for %s on %s"),
					service->url->user, service->url->host);
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
	{ "STARTTLS",           IMAP_CAPABILITY_STARTTLS },
	{ NULL, 0 }
};


static gboolean
imap_get_capability (CamelService *service, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (service);
	CamelImapResponse *response;
	char *result, *capa, *lasts;
	int i;
	
	CAMEL_IMAP_STORE_ASSERT_LOCKED (store, command_lock);
	
	/* Find out the IMAP capabilities */
	/* We assume we have utf8 capable search until a failed search tells us otherwise */
	store->capabilities = IMAP_CAPABILITY_utf8_search;
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

enum {
	USE_SSL_NEVER,
	USE_SSL_ALWAYS,
	USE_SSL_WHEN_POSSIBLE
};

static gboolean
connect_to_server (CamelService *service, int ssl_mode, int try_starttls, CamelException *ex)
{
	CamelImapStore *store = (CamelImapStore *) service;
	CamelImapResponse *response;
	CamelStream *tcp_stream;
	struct hostent *h;
	int clean_quit;
	int port, ret;
	char *buf;
	
	h = camel_service_gethost (service, ex);
	if (!h)
		return FALSE;
	
	port = service->url->port ? service->url->port : 143;
	
#ifdef HAVE_SSL
	if (ssl_mode != USE_SSL_NEVER) {
		if (try_starttls)
			tcp_stream = camel_tcp_stream_ssl_new_raw (service, service->url->host);
		else {
			port = service->url->port ? service->url->port : 993;
			tcp_stream = camel_tcp_stream_ssl_new (service, service->url->host);
		}
	} else {
		tcp_stream = camel_tcp_stream_raw_new ();
	}
#else
	tcp_stream = camel_tcp_stream_raw_new ();
#endif /* HAVE_SSL */
	
	ret = camel_tcp_stream_connect (CAMEL_TCP_STREAM (tcp_stream), h, port);
	camel_free_host (h);
	if (ret == -1) {
		if (errno == EINTR)
			camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL,
					     _("Connection cancelled"));
		else
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					      _("Could not connect to %s (port %d): %s"),
					      service->url->host, port, g_strerror (errno));
		
		camel_object_unref (CAMEL_OBJECT (tcp_stream));
		
		return FALSE;
	}
	
	store->ostream = tcp_stream;
	store->istream = camel_stream_buffer_new (tcp_stream, CAMEL_STREAM_BUFFER_READ);
	
	store->connected = TRUE;
	store->command = 0;
	
	/* Read the greeting, if any. FIXME: deal with PREAUTH */
	if (camel_imap_store_readline (store, &buf, ex) < 0) {
		if (store->istream) {
			camel_object_unref (CAMEL_OBJECT (store->istream));
			store->istream = NULL;
		}
		
		if (store->ostream) {
			camel_object_unref (CAMEL_OBJECT (store->ostream));
			store->ostream = NULL;
		}
		
		store->connected = FALSE;
		return FALSE;
	}
	g_free (buf);
	
	/* get the imap server capabilities */
	if (!imap_get_capability (service, ex)) {
		if (store->istream) {
			camel_object_unref (CAMEL_OBJECT (store->istream));
			store->istream = NULL;
		}
		
		if (store->ostream) {
			camel_object_unref (CAMEL_OBJECT (store->ostream));
			store->ostream = NULL;
		}
		
		store->connected = FALSE;
		return FALSE;
	}
	
#ifdef HAVE_SSL
	if (ssl_mode == USE_SSL_WHEN_POSSIBLE) {
		if (store->capabilities & IMAP_CAPABILITY_STARTTLS)
			goto starttls;
	} else if (ssl_mode == USE_SSL_ALWAYS) {
		if (try_starttls) {
			if (store->capabilities & IMAP_CAPABILITY_STARTTLS) {
				/* attempt to toggle STARTTLS mode */
				goto starttls;
			} else {
				/* server doesn't support STARTTLS, abort */
				camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
						      _("Failed to connect to IMAP server %s in secure mode: %s"),
						      service->url->host, _("SSL/TLS extension not supported."));
				/* we have the possibility of quitting cleanly here */
				clean_quit = TRUE;
				goto exception;
			}
		}
	}
#endif /* HAVE_SSL */
	
	return TRUE;
	
#ifdef HAVE_SSL
 starttls:
	
	/* as soon as we send a STARTTLS command, all hope is lost of a clean QUIT if problems arise */
	clean_quit = FALSE;
	
	response = camel_imap_command (store, NULL, ex, "STARTTLS");
	if (!response) {
		camel_object_unref (CAMEL_OBJECT (store->istream));
		camel_object_unref (CAMEL_OBJECT (store->ostream));
		store->istream = store->ostream = NULL;
		return FALSE;
	}
	
	camel_imap_response_free_without_processing (store, response);
	
	/* Okay, now toggle SSL/TLS mode */
	if (camel_tcp_stream_ssl_enable_ssl (CAMEL_TCP_STREAM_SSL (tcp_stream)) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to connect to IMAP server %s in secure mode: %s"),
				      service->url->host, _("SSL negotiations failed"));
		goto exception;
	}
	
	/* rfc2595, section 4 states that after a successful STLS
           command, the client MUST discard prior CAPA responses */
	if (!imap_get_capability (service, ex)) {
		camel_object_unref (CAMEL_OBJECT (store->istream));
		camel_object_unref (CAMEL_OBJECT (store->ostream));
		store->istream = NULL;
		store->ostream = NULL;
		return FALSE;
	}
	
	return TRUE;
	
 exception:
	if (clean_quit) {
		/* try to disconnect cleanly */
		response = camel_imap_command (store, NULL, ex, "LOGOUT");
		if (response)
			camel_imap_response_free_without_processing (store, response);
	}
	
	camel_object_unref (CAMEL_OBJECT (store->istream));
	camel_object_unref (CAMEL_OBJECT (store->ostream));
	store->istream = NULL;
	store->ostream = NULL;
	
	return FALSE;
#endif /* HAVE_SSL */
}

static struct {
	char *value;
	int mode;
} ssl_options[] = {
	{ "",              USE_SSL_ALWAYS        },
	{ "always",        USE_SSL_ALWAYS        },
	{ "when-possible", USE_SSL_WHEN_POSSIBLE },
	{ "never",         USE_SSL_NEVER         },
	{ NULL,            USE_SSL_NEVER         },
};

static gboolean
connect_to_server_wrapper (CamelService *service, CamelException *ex)
{
#ifdef HAVE_SSL
	const char *use_ssl;
	int i, ssl_mode;
	
	use_ssl = camel_url_get_param (service->url, "use_ssl");
	if (use_ssl) {
		for (i = 0; ssl_options[i].value; i++)
			if (!strcmp (ssl_options[i].value, use_ssl))
				break;
		ssl_mode = ssl_options[i].mode;
	} else
		ssl_mode = USE_SSL_NEVER;
	
	if (ssl_mode == USE_SSL_ALWAYS) {
		/* First try the ssl port */
		if (!connect_to_server (service, ssl_mode, FALSE, ex)) {
			if (camel_exception_get_id (ex) == CAMEL_EXCEPTION_SERVICE_UNAVAILABLE) {
				/* The ssl port seems to be unavailable, lets try STARTTLS */
				camel_exception_clear (ex);
				return connect_to_server (service, ssl_mode, TRUE, ex);
			} else {
				return FALSE;
			}
		}
		
		return TRUE;
	} else if (ssl_mode == USE_SSL_WHEN_POSSIBLE) {
		/* If the server supports STARTTLS, use it */
		return connect_to_server (service, ssl_mode, TRUE, ex);
	} else {
		/* User doesn't care about SSL */
		return connect_to_server (service, ssl_mode, FALSE, ex);
	}
#else
	return connect_to_server (service, USE_SSL_NEVER, FALSE, ex);
#endif
}

extern CamelServiceAuthType camel_imap_password_authtype;

static GList *
query_auth_types (CamelService *service, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (service);
	CamelServiceAuthType *authtype;
	GList *sasl_types, *t, *next;
	gboolean connected;
	
	if (!camel_disco_store_check_online (CAMEL_DISCO_STORE (store), ex))
		return NULL;
	
	CAMEL_IMAP_STORE_LOCK (store, command_lock);
	connected = connect_to_server_wrapper (service, ex);
	CAMEL_IMAP_STORE_UNLOCK (store, command_lock);
	if (!connected)
		return NULL;
	
	sasl_types = camel_sasl_authtype_list (FALSE);
	for (t = sasl_types; t; t = next) {
		authtype = t->data;
		next = t->next;
		
		if (!g_hash_table_lookup (store->authtypes, authtype->authproto)) {
			sasl_types = g_list_remove_link (sasl_types, t);
			g_list_free_1 (t);
		}
	}
	
	return g_list_prepend (sasl_types, &camel_imap_password_authtype);
}

static void
imap_folder_effectively_unsubscribed(CamelImapStore *imap_store, 
				     const char *folder_name, CamelException *ex)
{
	gpointer key, value;
	CamelFolderInfo *fi;
	const char *name;

	if (g_hash_table_lookup_extended (imap_store->subscribed_folders,
					  folder_name, &key, &value)) {
		g_hash_table_remove (imap_store->subscribed_folders, key);
		g_free (key);
	}
	
	if (imap_store->renaming) {
		/* we don't need to emit a "folder_unsubscribed" signal
                   if we are in the process of renaming folders, so we
                   are done here... */
		return;
	}
	
	name = strrchr (folder_name, imap_store->dir_sep);
	if (name)
		name++;
	else
		name = folder_name;
	
	fi = g_new0 (CamelFolderInfo, 1);
	fi->full_name = g_strdup (folder_name);
	fi->name = g_strdup (name);
	fi->url = g_strdup_printf ("%s/%s", imap_store->base_url, folder_name);
	fi->unread_message_count = -1;
	camel_folder_info_build_path (fi, imap_store->dir_sep);
	
	camel_object_trigger_event (CAMEL_OBJECT (imap_store), "folder_unsubscribed", fi);
	camel_folder_info_free (fi);
}

static void
imap_forget_folder (CamelImapStore *imap_store, const char *folder_name, CamelException *ex)
{
	CamelFolderSummary *summary;
	CamelImapMessageCache *cache;
	char *summary_file;
	char *journal_file;
	char *folder_dir, *storage_path;
	CamelFolderInfo *fi;
	const char *name;
	
	name = strrchr (folder_name, imap_store->dir_sep);
	if (name)
		name++;
	else
		name = folder_name;
	
	storage_path = g_strdup_printf ("%s/folders", imap_store->storage_path);
	folder_dir = e_path_to_physical (storage_path, folder_name);
	g_free (storage_path);
	if (access (folder_dir, F_OK) != 0) {
		g_free (folder_dir);
		goto event;
	}
	
	summary_file = g_strdup_printf ("%s/summary", folder_dir);
	summary = camel_imap_summary_new (summary_file);
	if (!summary) {
		g_free (summary_file);
		g_free (folder_dir);
		goto event;
	}
	
	cache = camel_imap_message_cache_new (folder_dir, summary, ex);
	if (cache)
		camel_imap_message_cache_clear (cache);
	
	camel_object_unref (CAMEL_OBJECT (cache));
	camel_object_unref (CAMEL_OBJECT (summary));
	
	unlink (summary_file);
	g_free (summary_file);
		
	journal_file = g_strdup_printf ("%s/summary", folder_dir);
	unlink (journal_file);
	g_free (journal_file);
	
	rmdir (folder_dir);
	g_free (folder_dir);
	
 event:
	
	fi = g_new0 (CamelFolderInfo, 1);
	fi->full_name = g_strdup (folder_name);
	fi->name = g_strdup (name);
	fi->url = g_strdup_printf ("%s/%s", imap_store->base_url, folder_name);
	fi->unread_message_count = -1;
	camel_folder_info_build_path (fi, imap_store->dir_sep);
	camel_object_trigger_event (CAMEL_OBJECT (imap_store), "folder_deleted", fi);
	camel_folder_info_free (fi);
}

static gboolean
imap_check_folder_still_extant (CamelImapStore *imap_store, const char *full_name, 
				CamelException *ex)
{
	CamelImapResponse *response;

	response = camel_imap_command (imap_store, NULL, ex, "LIST \"\" %S",
				       full_name);

	if (response) {
		gboolean stillthere = FALSE;

		if (response->untagged->len)
			stillthere = TRUE;

		camel_imap_response_free_without_processing (imap_store, response);

		if (stillthere)
			return TRUE;
	}

	/* either LIST command was rejected or it gave no results,
	 * we can be sure that the folder is gone. */

	camel_exception_setv (ex, CAMEL_EXCEPTION_FOLDER_INVALID,
			     _("The folder %s no longer exists"),
			     full_name);
	return FALSE;
}

static void
copy_folder(char *key, CamelFolder *folder, GPtrArray *out)
{
	g_ptr_array_add(out, folder);
	camel_object_ref((CamelObject *)folder);
}

/* This is a little 'hack' to avoid the deadlock conditions that would otherwise
   ensue when calling camel_folder_refresh_info from inside a lock */
/* NB: on second thougts this is probably not entirely safe, but it'll do for now */
/* No, its definetly not safe.  So its been changed to copy the folders first */
/* the alternative is to:
   make the camel folder->lock recursive (which should probably be done)
   or remove it from camel_folder_refresh_info, and use another locking mechanism */
/* also see get_folder_info_online() for the same hack repeated */
static void
imap_store_refresh_folders (CamelImapStore *store, CamelException *ex)
{
	GPtrArray *folders;
	int i;
	
	folders = g_ptr_array_new();
	CAMEL_STORE_LOCK(store, cache_lock);
	g_hash_table_foreach (CAMEL_STORE (store)->folders, (GHFunc)copy_folder, folders);
	CAMEL_STORE_UNLOCK(store, cache_lock);
	
	for (i = 0; i <folders->len; i++) {
		CamelFolder *folder = folders->pdata[i];

		CAMEL_IMAP_FOLDER (folder)->need_rescan = TRUE;
		if (!camel_exception_is_set(ex))
			CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(folder))->refresh_info(folder, ex);

		if (camel_exception_is_set (ex) &&
		    imap_check_folder_still_extant (store, folder->full_name, ex) == FALSE) {
			gchar *namedup;
			
			/* the folder was deleted (may happen when we come back online
			 * after being offline */
			
			namedup = g_strdup (folder->full_name);
			camel_object_unref((CamelObject *)folder);
			imap_folder_effectively_unsubscribed (store, namedup, ex);
			imap_forget_folder (store, namedup, ex);
			camel_exception_clear (ex);
			g_free (namedup);
		} else
			camel_object_unref((CamelObject *)folder);
	}
	
	g_ptr_array_free (folders, TRUE);
}	

static gboolean
try_auth (CamelImapStore *store, const char *mech, CamelException *ex)
{
	CamelSasl *sasl;
	CamelImapResponse *response;
	char *resp;
	char *sasl_resp;
	
	CAMEL_IMAP_STORE_ASSERT_LOCKED (store, command_lock);
	
	response = camel_imap_command (store, NULL, ex, "AUTHENTICATE %s", mech);
	if (!response)
		return FALSE;
	
	sasl = camel_sasl_new ("imap", mech, CAMEL_SERVICE (store));
	while (!camel_sasl_authenticated (sasl)) {
		resp = camel_imap_response_extract_continuation (store, response, ex);
		if (!resp)
			goto lose;
		
		sasl_resp = camel_sasl_challenge_base64 (sasl, imap_next_word (resp), ex);
		g_free (resp);
		if (camel_exception_is_set (ex))
			goto break_and_lose;
		
		response = camel_imap_command_continuation (store, sasl_resp, strlen (sasl_resp), ex);
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
	response = camel_imap_command_continuation (store, "*", 1, NULL);
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
			if (!connect_to_server_wrapper (service, ex))
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
			if (camel_exception_get_id(ex) == CAMEL_EXCEPTION_USER_CANCEL)
				return FALSE;
			
			errbuf = g_strdup_printf (_("Unable to authenticate "
						    "to IMAP server.\n%s\n\n"),
						  camel_exception_get_description (ex));
			camel_exception_clear (ex);
		}
	}
	
	return TRUE;
}

#define IMAP_STOREINFO_VERSION 2

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
	struct _namespaces *namespaces;
	char *result, *name, *path;
	FILE *storeinfo;
	int i, flags;
	size_t len;
	
	CAMEL_IMAP_STORE_LOCK (store, command_lock);
	if (!connect_to_server_wrapper (service, ex) ||
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
		
		/* new code... */
		namespaces = imap_parse_namespace_response (result);
		imap_namespaces_destroy (namespaces);
		/* end new code */
		
		name = strstrcase (result, "NAMESPACE ((");
		if (name) {
			char *sep;
			
			name += 12;
			store->namespace = imap_parse_string ((const char **) &name, &len);
			if (name && *name++ == ' ') {
				sep = imap_parse_string ((const char **) &name, &len);
				if (sep) {
					store->dir_sep = *sep;
					((CamelStore *)store)->dir_sep = store->dir_sep;
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
			imap_parse_list_response (store, result, NULL, &store->dir_sep, NULL);
			g_free (result);
		}
		if (!store->dir_sep) {
			store->dir_sep = '/';	/* Guess */
			((CamelStore *)store)->dir_sep = store->dir_sep;
		}
	}
	
	/* canonicalize the namespace to end with dir_sep */
	len = strlen (store->namespace);
	if (len && store->namespace[len - 1] != store->dir_sep) {
		gchar *tmp;
		
		tmp = g_strdup_printf ("%s%c", store->namespace, store->dir_sep);
		g_free (store->namespace);
		store->namespace = tmp;
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
			if (!imap_parse_list_response (store, result, &flags, NULL, &name))
				continue;
			if (flags & (CAMEL_IMAP_FOLDER_MARKED | CAMEL_IMAP_FOLDER_UNMARKED))
				store->capabilities |= IMAP_CAPABILITY_useful_lsub;
			if (flags & CAMEL_FOLDER_NOSELECT) {
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
	else if (camel_disco_diary_empty (disco_store->diary))
		imap_store_refresh_folders (store, ex);
	
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
		
		/* We know we're offline, so this will have to set @ex
		 * and return FALSE.
		 */
		return camel_disco_store_check_online (CAMEL_DISCO_STORE (store), ex);
	}
	
	store->subscribed_folders = g_hash_table_new (g_str_hash, g_str_equal);

	camel_file_util_decode_uint32 (storeinfo, &store->capabilities);
	imap_set_server_level (store);
	camel_file_util_decode_string (storeinfo, &name);
	/* if the namespace has changed, the subscribed folder list in this file is bogus */
	if (store->namespace == NULL || (name != NULL && strcmp(name, store->namespace) == 0)) {
		g_free(store->namespace);
		store->namespace = name;
		camel_file_util_decode_uint32 (storeinfo, &tmp);
		store->dir_sep = tmp;
		((CamelStore *)store)->dir_sep = tmp;
		while (camel_file_util_decode_string (storeinfo, &buf) == 0) {
			if (!imap_parse_list_response (store, buf, NULL, NULL, &name)) {
				g_free (buf);
				continue;
			}
			g_hash_table_insert (store->subscribed_folders, name,
					     GINT_TO_POINTER (1));
			g_free (buf);
		}
	} else {
		g_free(name);
	}

	fclose (storeinfo);
	imap_store_refresh_folders (store, ex);
	
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
		response = camel_imap_command (store, NULL, NULL, "LOGOUT");
		camel_imap_response_free (store, response);
	}
	
	if (store->istream) {
		camel_object_unref (CAMEL_OBJECT (store->istream));
		store->istream = NULL;
	}
	
	if (store->ostream) {
		camel_object_unref (CAMEL_OBJECT (store->ostream));
		store->ostream = NULL;
	}
	
	imap_disconnect_offline (service, clean, ex);
	
	return TRUE;
}

static void
imap_noop (CamelStore *store, CamelException *ex)
{
	CamelImapStore *imap_store = (CamelImapStore *) store;
	CamelDiscoStore *disco = (CamelDiscoStore *) store;
	CamelImapResponse *response;
	
	switch (camel_disco_store_status (disco)) {
	case CAMEL_DISCO_STORE_ONLINE:
	case CAMEL_DISCO_STORE_RESYNCING:
		response = camel_imap_command (imap_store, NULL, NULL, "NOOP");
		if (response)
			camel_imap_response_free (imap_store, response);
		break;
	case CAMEL_DISCO_STORE_OFFLINE:
		break;
	}
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

static int
get_folder_status (CamelImapStore *imap_store, const char *folder_name, const char *type)
{
	CamelImapResponse *response;
	char *status, *p;
	int out;

	/* FIXME: we assume the server is STATUS-capable */

	response = camel_imap_command (imap_store, NULL, NULL,
				       "STATUS %F (%s)",
				       folder_name,
				       type);

	if (!response) {
		CamelException ex;

		camel_exception_init (&ex);
		if (imap_check_folder_still_extant (imap_store, folder_name, &ex) == FALSE) {
			imap_folder_effectively_unsubscribed (imap_store, folder_name, &ex);
			imap_forget_folder (imap_store, folder_name, &ex);
		}
		camel_exception_clear (&ex);
		return -1;
	}

	status = camel_imap_response_extract (imap_store, response,
					      "STATUS", NULL);
	if (!status)
		return -1;

	p = strstrcase (status, type);
	if (p)
		out = strtoul (p + strlen (type), NULL, 10);
	else
		out = -1;

	g_free (status);
	return out;
}

static CamelFolder *
get_folder_online (CamelStore *store, const char *folder_name,
		   guint32 flags, CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	CamelImapResponse *response;
	CamelFolder *new_folder;
	char *folder_dir, *storage_path;
	
	if (!camel_imap_store_connected (imap_store, ex))
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
				       "SELECT %F", folder_name);
	if (!response) {
		if (!flags & CAMEL_STORE_FOLDER_CREATE) {
			CAMEL_IMAP_STORE_UNLOCK (imap_store, command_lock);
			return no_such_folder (folder_name, ex);
		}
		
		response = camel_imap_command (imap_store, NULL, ex,
					       "CREATE %F", folder_name);
		if (response) {
			camel_imap_response_free (imap_store, response);
			
			response = camel_imap_command (imap_store, NULL, NULL,
						       "SELECT %F", folder_name);
		}
		if (!response) {
			CAMEL_IMAP_STORE_UNLOCK (imap_store, command_lock);
			return NULL;
		}
	}

	storage_path = g_strdup_printf("%s/folders", imap_store->storage_path);
	folder_dir = e_path_to_physical (storage_path, folder_name);
	g_free(storage_path);
	new_folder = camel_imap_folder_new (store, folder_name, folder_dir, ex);
	g_free (folder_dir);
	if (new_folder) {
		CamelException local_ex;

		imap_store->current_folder = new_folder;
		camel_object_ref (CAMEL_OBJECT (new_folder));
		camel_exception_init (&local_ex);
		camel_imap_folder_selected (new_folder, response, &local_ex);

		if (camel_exception_is_set (&local_ex)) {
			camel_exception_xfer (ex, &local_ex);
			camel_object_unref (CAMEL_OBJECT (imap_store->current_folder));
			imap_store->current_folder = NULL;
			camel_object_unref (CAMEL_OBJECT (new_folder));
			new_folder = NULL;
		}
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
	char *folder_dir, *storage_path;
	
	if (!imap_store->connected &&
	    !camel_service_connect (CAMEL_SERVICE (store), ex))
		return NULL;
	
	if (!g_strcasecmp (folder_name, "INBOX"))
		folder_name = "INBOX";
	
	storage_path = g_strdup_printf("%s/folders", imap_store->storage_path);
	folder_dir = e_path_to_physical (storage_path, folder_name);
	g_free(storage_path);
	if (!folder_dir || access (folder_dir, F_OK) != 0) {
		g_free (folder_dir);
		camel_exception_setv (ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				      _("No such folder %s"), folder_name);
		return NULL;
	}
	
	new_folder = camel_imap_folder_new (store, folder_name, folder_dir, ex);
	g_free (folder_dir);
	
	return new_folder;
}

static void
delete_folder (CamelStore *store, const char *folder_name, CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	CamelImapResponse *response;
	
	if (!camel_disco_store_check_online (CAMEL_DISCO_STORE (store), ex))
		return;
	
	/* make sure this folder isn't currently SELECTed */
	response = camel_imap_command (imap_store, NULL, ex, "SELECT INBOX");
	if (response) {
		camel_imap_response_free_without_processing (imap_store, response);
		
		CAMEL_IMAP_STORE_LOCK (imap_store, command_lock);
		
		if (imap_store->current_folder)
			camel_object_unref (CAMEL_OBJECT (imap_store->current_folder));
		/* no need to actually create a CamelFolder for INBOX */
		imap_store->current_folder = NULL;
		
		CAMEL_IMAP_STORE_UNLOCK (imap_store, command_lock);
	} else
		return;
	
	response = camel_imap_command (imap_store, NULL, ex, "DELETE %F",
				       folder_name);
	
	if (response) {
		camel_imap_response_free (imap_store, response);
		imap_forget_folder (imap_store, folder_name, ex);
	}
}

static void
manage_subscriptions (CamelStore *store, CamelFolderInfo *fi, gboolean subscribe)
{
	while (fi) {
		if (fi->child)
			manage_subscriptions (store, fi->child, subscribe);
		
		if (subscribe)
			subscribe_folder (store, fi->full_name, NULL);
		else
			unsubscribe_folder (store, fi->full_name, NULL);
		
		fi = fi->sibling;
	}
}

#define subscribe_folders(store, fi) manage_subscriptions (store, fi, TRUE)
#define unsubscribe_folders(store, fi) manage_subscriptions (store, fi, FALSE)

static void
rename_folder_info (CamelImapStore *imap_store, CamelFolderInfo *fi, const char *old_name, const char *new_name)
{
	CamelImapResponse *response;
	char *name;
	
	while (fi) {
		if (fi->child)
			rename_folder_info (imap_store, fi->child, old_name, new_name);
		
		name = g_strdup_printf ("%s%s", new_name, fi->full_name + strlen (old_name));
		
		if (imap_store->dir_sep == '.') {
			/* kludge around imap servers like Courier that don't rename
			   subfolders when you rename the parent folder - like
			   the spec says to do!!! */
			response = camel_imap_command (imap_store, NULL, NULL, "RENAME %F %F", fi->full_name, name);
			if (response)
				camel_imap_response_free (imap_store, response);
		}
		
		g_free (fi->full_name);
		fi->full_name = name;
		
		fi = fi->sibling;
	}
}

static void
rename_folder (CamelStore *store, const char *old_name, const char *new_name, CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	CamelImapResponse *response;
	char *oldpath, *newpath, *storage_path;
	CamelFolderInfo *fi;
	guint32 flags;
	
	if (!camel_disco_store_check_online (CAMEL_DISCO_STORE (store), ex))
		return;
	
	/* make sure this folder isn't currently SELECTed - it's
           actually possible to rename INBOX but if you do another
           INBOX will immediately be created by the server */
	response = camel_imap_command (imap_store, NULL, ex, "SELECT INBOX");
	if (response) {
		camel_imap_response_free_without_processing (imap_store, response);
		
		CAMEL_IMAP_STORE_LOCK (imap_store, command_lock);
		
		if (imap_store->current_folder)
			camel_object_unref (CAMEL_OBJECT (imap_store->current_folder));
		/* no need to actually create a CamelFolder for INBOX */
		imap_store->current_folder = NULL;
		
		CAMEL_IMAP_STORE_UNLOCK (imap_store, command_lock);
	} else
		return;
	
	imap_store->renaming = TRUE;
	
	flags = CAMEL_STORE_FOLDER_INFO_FAST | CAMEL_STORE_FOLDER_INFO_RECURSIVE |
		(store->flags & CAMEL_STORE_SUBSCRIPTIONS ? CAMEL_STORE_FOLDER_INFO_SUBSCRIBED : 0);
	
	fi = ((CamelStoreClass *)((CamelObject *)store)->klass)->get_folder_info (store, old_name, flags, ex);
	if (fi && store->flags & CAMEL_STORE_SUBSCRIPTIONS)
		unsubscribe_folders (store, fi);
	
	response = camel_imap_command (imap_store, NULL, ex, "RENAME %F %F", old_name, new_name);
	
	if (!response) {
		if (fi && store->flags & CAMEL_STORE_SUBSCRIPTIONS)
			subscribe_folders (store, fi);
		
		camel_store_free_folder_info (store, fi);
		imap_store->renaming = FALSE;
		return;
	}
	
	camel_imap_response_free (imap_store, response);
	
	rename_folder_info (imap_store, fi, old_name, new_name);
	if (fi && store->flags & CAMEL_STORE_SUBSCRIPTIONS)
		subscribe_folders (store, fi);
	
	camel_store_free_folder_info (store, fi);
	
	storage_path = g_strdup_printf("%s/folders", imap_store->storage_path);
	oldpath = e_path_to_physical (storage_path, old_name);
	newpath = e_path_to_physical (storage_path, new_name);
	g_free(storage_path);

	/* So do we care if this didn't work?  Its just a cache? */
	if (rename (oldpath, newpath) == -1) {
		g_warning ("Could not rename message cache '%s' to '%s': %s: cache reset",
			   oldpath, newpath, strerror (errno));
	}
	
	g_free (oldpath);
	g_free (newpath);
	
	imap_store->renaming = FALSE;
}

static CamelFolderInfo *
create_folder (CamelStore *store, const char *parent_name,
	       const char *folder_name, CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	char *full_name, *resp, *thisone;
	CamelImapResponse *response;
	CamelException internal_ex;
	CamelFolderInfo *root = NULL;
	gboolean need_convert;
	char **pathnames = NULL;
	GPtrArray *folders = NULL;
	int i = 0, flags;
	
	if (!camel_disco_store_check_online (CAMEL_DISCO_STORE (store), ex))
		return NULL;
	if (!parent_name)
		parent_name = "";
	
	if (strchr (folder_name, imap_store->dir_sep)) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_FOLDER_INVALID_PATH,
				      _("The folder name \"%s\" is invalid because "
					"it containes the character \"%c\""),
				      folder_name, imap_store->dir_sep);
		return NULL;
	}
	
	/* check if the parent allows inferiors */
	
	need_convert = FALSE;
	response = camel_imap_command (imap_store, NULL, ex, "LIST \"\" %F",
				       parent_name);
	if (!response) /* whoa, this is bad */
		return NULL;
	
	/* FIXME: does not handle unexpected circumstances very well */
	for (i = 0; i < response->untagged->len; i++) {
		resp = response->untagged->pdata[i];
		
		if (!imap_parse_list_response (imap_store, resp, &flags, NULL, &thisone))
			continue;
		
		if (strcmp (thisone, parent_name) == 0) {
			if (flags & CAMEL_FOLDER_NOINFERIORS)
				need_convert = TRUE;
			break;
		}
	}
	
	camel_imap_response_free (imap_store, response);
	
	camel_exception_init (&internal_ex);
	
	/* if not, check if we can delete it and recreate it */
	if (need_convert) {
		char *name;
		
		if (get_folder_status (imap_store, parent_name, "MESSAGES")) {
			camel_exception_set (ex, CAMEL_EXCEPTION_FOLDER_INVALID_STATE,
					     _("The parent folder is not allowed to contain subfolders"));
			return NULL;
		}
		
		/* delete the old parent and recreate it */
		delete_folder (store, parent_name, &internal_ex);
		if (camel_exception_is_set (&internal_ex)) {
			camel_exception_xfer (ex, &internal_ex);
			return NULL;
		}
		
		/* add the dirsep to the end of parent_name */
		name = g_strdup_printf ("%s%c", parent_name, imap_store->dir_sep);
		response = camel_imap_command (imap_store, NULL, ex, "CREATE %F",
					       name);
		g_free (name);
		
		if (!response)
			return NULL;
		else
			camel_imap_response_free (imap_store, response);
	}
	
	/* ok now we can create the folder */
	
	full_name = imap_concat (imap_store, parent_name, folder_name);
	response = camel_imap_command (imap_store, NULL, ex, "CREATE %F",
				       full_name);
	g_free (full_name);
	
	if (response) {
		CamelFolderInfo *parent, *fi;
		
		camel_imap_response_free (imap_store, response);
		
		/* We have to do this in case we are creating a
                   recursive directory structure */
		i = 0;
		pathnames = imap_parse_folder_name (imap_store, folder_name);
		full_name = imap_concat (imap_store, parent_name, pathnames[i]);
		g_free (pathnames[i]);
		
		folders = g_ptr_array_new ();
		
		get_folders_online (imap_store, full_name, folders, FALSE, ex);
		g_free (full_name);
		if (camel_exception_is_set (&internal_ex)) {
			camel_exception_xfer (&internal_ex, ex);
			goto exception;
		}
		
		root = parent = folders->pdata[i];
		
		for (i = 1; parent && pathnames[i]; i++) {
			full_name = imap_concat (imap_store, parent_name, pathnames[i]);
			g_free (pathnames[i]);
			
			get_folders_online (imap_store, full_name, folders, FALSE, &internal_ex);
			if (camel_exception_is_set (&internal_ex)) {
				camel_exception_xfer (&internal_ex, ex);
				goto exception;
			}
			g_free (full_name);
			
			if (folders->len != i + 1)
				break;
			
			fi = folders->pdata[i];
			camel_folder_info_build_path (fi, imap_store->dir_sep);
			parent->child = fi;
			fi->parent = parent;
			parent = fi;
		}
		
		camel_folder_info_build_path(root, imap_store->dir_sep);
		camel_object_trigger_event (CAMEL_OBJECT (store), "folder_created", root);
		
		g_free (pathnames);
		
		g_ptr_array_free (folders, TRUE);
	}
	
	return root;
	
 exception:
	
	for (/* i is already set */; pathnames && pathnames[i]; i++)
		g_free (pathnames[i]);
	g_free (pathnames);
	
	if (folders) {
		for (i = 0; i < folders->len; i++)
			camel_folder_info_free (folders->pdata[i]);
		g_ptr_array_free (folders, TRUE);
	}
	
	return NULL;
}

static CamelFolderInfo *
parse_list_response_as_folder_info (CamelImapStore *imap_store,
				    const char *response)
{
	CamelFolderInfo *fi;
	int flags;
	char sep, *dir, *name = NULL;
	CamelURL *url;
	
	if (!imap_parse_list_response (imap_store, response, &flags, &sep, &dir))
		return NULL;
	
	if (sep) {
		name = strrchr (dir, sep);
		if (name && !*++name) {
			g_free (dir);
			return NULL;
		}
	}

	fi = g_new0 (CamelFolderInfo, 1);
	fi->flags = flags;
	fi->full_name = dir;
	if (sep && name)
		fi->name = g_strdup (name);
	else
		fi->name = g_strdup (dir);
	
	url = camel_url_new (imap_store->base_url, NULL);
	g_free (url->path);
	url->path = g_strdup_printf ("/%s", dir);
	if (flags & CAMEL_FOLDER_NOSELECT || fi->name[0] == 0)
		camel_url_set_param (url, "noselect", "yes");
	fi->url = camel_url_to_string (url, 0);
	camel_url_free (url);

	/* FIXME: redundant */
	if (flags & CAMEL_IMAP_FOLDER_UNMARKED)
		fi->unread_message_count = -1;
	
	return fi;
}

static void
copy_folder_name (gpointer name, gpointer key, gpointer array)
{
	g_ptr_array_add (array, name);
}

/* this is used when lsub doesn't provide very useful information */
static GPtrArray *
get_subscribed_folders (CamelImapStore *imap_store, const char *top, CamelException *ex)
{
	GPtrArray *names, *folders;
	CamelImapResponse *response;
	CamelFolderInfo *fi;
	char *result;
	int i, toplen = strlen (top);
	
	folders = g_ptr_array_new ();
	names = g_ptr_array_new ();
	g_hash_table_foreach (imap_store->subscribed_folders,
			      copy_folder_name, names);
	
	if (names->len == 0)
		g_ptr_array_add (names, "INBOX");
	
	for (i = 0; i < names->len; i++) {
		response = camel_imap_command (imap_store, NULL, ex,
					       "LIST \"\" %F",
					       names->pdata[i]);
		if (!response)
			break;
		
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
	
	return folders;
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
				       "%s \"\" %F", lsub ? "LSUB" : "LIST",
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

#if 1
static void
dumpfi(CamelFolderInfo *fi)
{
	int depth;
	CamelFolderInfo *n = fi;

	if (fi == NULL)
		return;

	depth = 0;
	while (n->parent) {
		depth++;
		n = n->parent;
	}

	while (fi) {
		printf("%-40s %-30s %*s\n", fi->path, fi->full_name, depth*2+strlen(fi->name), fi->name);
		if (fi->child)
			dumpfi(fi->child);
		fi = fi->sibling;
	}
}
#endif

static void
get_folder_counts(CamelImapStore *imap_store, CamelFolderInfo *fi, CamelException *ex)
{
	GSList *q;

	/* non-recursive breath first search */

	q = g_slist_append(NULL, fi);

	while (q) {
		fi = q->data;
		q = g_slist_remove_link(q, q);

		while (fi) {
			/* ignore noselect folders, and check only inbox if we only check inbox */
			if ((fi->flags & CAMEL_FOLDER_NOSELECT) == 0
			    && ( (imap_store->parameters & IMAP_PARAM_CHECK_ALL)
				 || strcasecmp(fi->full_name, "inbox") == 0) ) {

				CAMEL_IMAP_STORE_LOCK (imap_store, command_lock);
				/* For the current folder, poke it to check for new	
				 * messages and then report that number, rather than
				 * doing a STATUS command.
				 */
				if (imap_store->current_folder && strcmp(imap_store->current_folder->full_name, fi->full_name) == 0) {
					/* we bypass the folder locking otherwise we can deadlock.  we use the command lock for
					   any operations anyway so this is 'safe'.  See comment above imap_store_refresh_folders() for info */
					CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(imap_store->current_folder))->refresh_info(imap_store->current_folder, ex);
					fi->unread_message_count = camel_folder_get_unread_message_count (imap_store->current_folder);
				} else
					fi->unread_message_count = get_folder_status (imap_store, fi->full_name, "UNSEEN");
		
				CAMEL_IMAP_STORE_UNLOCK (imap_store, command_lock);
			} else {
				fi->unread_message_count = -1;
			}

			if (fi->child)
				q = g_slist_append(q, fi->child);
			fi = fi->sibling;
		}
	}
}

/* imap needs to treat inbox case insensitive */
/* we'll assume the names are normalised already */
static guint folder_hash(const void *ap)
{
	const char *a = ap;

	if (strcasecmp(a, "INBOX") == 0)
		a = "INBOX";

	return g_str_hash(a);
}

static int folder_eq(const void *ap, const void *bp)
{
	const char *a = ap;
	const char *b = bp;

	if (strcasecmp(a, "INBOX") == 0)
		a = "INBOX";
	if (strcasecmp(b, "INBOX") == 0)
		b = "INBOX";

	return g_str_equal(a, b);
}

static GPtrArray *
get_folders(CamelStore *store, const char *top, guint32 flags, CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	GSList *p = NULL;
	GHashTable *infos;
	int i;
	GPtrArray *folders, *folders_out;
	CamelFolderInfo *fi;
	char *name;
	int depth = 0;
	int haveinbox = 0;
	static int imap_max_depth = 0;

	if (!camel_imap_store_connected (imap_store, ex))
		return NULL;

	/* allow megalomaniacs to override the max of 10 */
	if (imap_max_depth == 0) {
		name = getenv("CAMEL_IMAP_MAX_DEPTH");
		if (name) {
			imap_max_depth = atoi (name);
			imap_max_depth = MIN (MAX (imap_max_depth, 0), 2);
		} else
			imap_max_depth = 10;
	}

	infos = g_hash_table_new(folder_hash, folder_eq);

	/* get starting point & strip trailing '/' */
	if (top[0] == 0 && imap_store->namespace)
		top = imap_store->namespace;
	i = strlen(top)-1;
	name = alloca(i+2);
	strcpy(name, top);
	while (i>0 && name[i] == store->dir_sep)
		name[i--] = 0;

	d(printf("\n\nList '%s' %s\n", name, flags&CAMEL_STORE_FOLDER_INFO_RECURSIVE?"RECURSIVE":"NON-RECURSIVE"));

	folders_out = g_ptr_array_new();
	folders = g_ptr_array_new();
	
	/* first get working list of names */
	get_folders_online (imap_store, name[0]?name:"%", folders, flags & CAMEL_STORE_FOLDER_INFO_SUBSCRIBED, ex);
	for (i=0; i<folders->len && !haveinbox; i++) {
		fi = folders->pdata[i];
		haveinbox = (strcasecmp(fi->full_name, "INBOX")) == 0;
	}

	if (!haveinbox && top == imap_store->namespace)
		get_folders_online(imap_store, "INBOX", folders, flags & CAMEL_STORE_FOLDER_INFO_SUBSCRIBED, ex);

	for (i=0; i<folders->len; i++)
		p = g_slist_prepend(p, folders->pdata[i]);

	g_ptr_array_set_size(folders, 0);

	/* p is a reversed list of pending folders for the next level, q is the list of folders for this */
	while (p) {
		GSList *q = g_slist_reverse(p);

		p = NULL;
		while (q) {
			fi = q->data;

			q = g_slist_remove_link(q, q);
			g_ptr_array_add(folders_out, fi);

			d(printf("Checking folder '%s'\n", fi->full_name));

			/* First if we're not recursive mode on the top level, and we know it has or doesn't
                            or can't have children, no need to go further - a bit ugly */
			if ( top == imap_store->namespace
			     && (flags & CAMEL_STORE_FOLDER_INFO_RECURSIVE) == 0
			     && (fi->flags & (CAMEL_FOLDER_CHILDREN|CAMEL_IMAP_FOLDER_NOCHILDREN|CAMEL_FOLDER_NOINFERIORS)) != 0) {
				/* do nothing */
			}
				/* Otherwise, if this has (or might have) children, scan it */
			else if ( (fi->flags & (CAMEL_IMAP_FOLDER_NOCHILDREN|CAMEL_FOLDER_NOINFERIORS)) == 0
				  || (fi->flags & CAMEL_FOLDER_CHILDREN) != 0) {
				char *n;
				
				n = imap_concat(imap_store, fi->full_name, "%");
				get_folders_online(imap_store, n, folders, flags & CAMEL_STORE_FOLDER_INFO_SUBSCRIBED, ex);
				g_free(n);

				if (folders->len > 0)
					fi->flags |= CAMEL_FOLDER_CHILDREN;

				for (i=0;i<folders->len;i++) {
					fi = folders->pdata[i];
					if (g_hash_table_lookup(infos, fi->full_name) == NULL) {
						g_hash_table_insert(infos, fi->full_name, fi);
						if ((flags & CAMEL_STORE_FOLDER_INFO_RECURSIVE) && depth<imap_max_depth)
							p = g_slist_prepend(p, fi);
						else
							g_ptr_array_add(folders_out, fi);
					} else {
						camel_folder_info_free(fi);
					}
				}
				g_ptr_array_set_size(folders, 0);
			}
		}
		depth++;
	}

	g_ptr_array_free(folders, TRUE);
	g_hash_table_destroy(infos);

	return folders_out;
}

static CamelFolderInfo *
get_folder_info_online (CamelStore *store, const char *top, guint32 flags, CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	GPtrArray *folders;
	CamelFolderInfo *tree;

	if (top == NULL)
		top = "";

	if ((flags & CAMEL_STORE_FOLDER_INFO_SUBSCRIBED)
	    && !(imap_store->capabilities & IMAP_CAPABILITY_useful_lsub)
	    && (imap_store->parameters & IMAP_PARAM_CHECK_ALL))
		folders = get_subscribed_folders(imap_store, top, ex);
	else
		folders = get_folders(store, top, flags, ex);

	/* note the weird top stuff, it is so a namespace based list "" is properly tree-ised */
	tree = camel_folder_info_build(folders, top[0] == 0 && imap_store->namespace?"":top, imap_store->dir_sep, TRUE);
	g_ptr_array_free(folders, TRUE);

	if (!(flags & CAMEL_STORE_FOLDER_INFO_FAST))
		get_folder_counts(imap_store, tree, ex);

	dumpfi(tree);

	return tree;
}

static gboolean
get_one_folder_offline (const char *physical_path, const char *path, gpointer data)
{
	GPtrArray *folders = data;
	CamelImapStore *imap_store = folders->pdata[0];
	CamelFolderInfo *fi;
	CamelURL *url;

	if (*path != '/')
		return TRUE;

	/* folder_info_build will insert parent nodes as necessary and mark
	 * them as noselect, which is information we actually don't have at
	 * the moment. So let it do the right thing by bailing out if it's
	 * not a folder we're explicitly interested in.
	 */

	if (g_hash_table_lookup (imap_store->subscribed_folders, path + 1) == 0)
		return TRUE;

	fi = g_new0 (CamelFolderInfo, 1);
	fi->full_name = g_strdup (path+1);
	fi->name = strrchr (fi->full_name, imap_store->dir_sep);
	if (fi->name)
		fi->name = g_strdup (fi->name + 1);
	else
		fi->name = g_strdup (fi->full_name);

	url = camel_url_new(imap_store->base_url, NULL);
	camel_url_set_path(url, path);
	fi->url = camel_url_to_string(url, 0);
	camel_url_free(url);

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
	char *storage_path;

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
	storage_path = g_strdup_printf("%s/folders", imap_store->storage_path);
	if (!e_path_find_folders (storage_path, get_one_folder_offline, folders)) {
		camel_disco_store_check_online (CAMEL_DISCO_STORE (imap_store), ex);
		fi = NULL;
	} else {
		g_ptr_array_remove_index_fast (folders, 0);
		fi = camel_folder_info_build (folders, "",
					      imap_store->dir_sep, TRUE);
	}
	g_free(storage_path);

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
	const char *name;
	CamelURL *url;
	
	if (!camel_disco_store_check_online (CAMEL_DISCO_STORE (store), ex))
		return;
	if (!camel_imap_store_connected (imap_store, ex))
		return;
	
	response = camel_imap_command (imap_store, NULL, ex,
				       "SUBSCRIBE %F", folder_name);
	if (!response)
		return;
	camel_imap_response_free (imap_store, response);
	
	g_hash_table_insert (imap_store->subscribed_folders,
			     g_strdup (folder_name), GUINT_TO_POINTER (1));
	
	if (imap_store->renaming) {
		/* we don't need to emit a "folder_subscribed" signal
                   if we are in the process of renaming folders, so we
                   are done here... */
		return;
	}
	
	name = strrchr (folder_name, imap_store->dir_sep);
	if (name)
		name++;
	else
		name = folder_name;
	
	url = camel_url_new (imap_store->base_url, NULL);
	g_free (url->path);
	url->path = g_strdup_printf ("/%s", folder_name);
	
	fi = g_new0 (CamelFolderInfo, 1);
	fi->full_name = g_strdup (folder_name);
	fi->name = g_strdup (name);
	fi->url = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);
	fi->unread_message_count = -1;
	camel_folder_info_build_path (fi, imap_store->dir_sep);
	
	camel_url_free (url);
	
	camel_object_trigger_event (CAMEL_OBJECT (store), "folder_subscribed", fi);
	camel_folder_info_free (fi);
}

static void
unsubscribe_folder (CamelStore *store, const char *folder_name,
		    CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	CamelImapResponse *response;
	
	if (!camel_disco_store_check_online (CAMEL_DISCO_STORE (store), ex))
		return;
	if (!camel_imap_store_connected (imap_store, ex))
		return;
	
	response = camel_imap_command (imap_store, NULL, ex,
				       "UNSUBSCRIBE %F", folder_name);
	if (!response)
		return;
	camel_imap_response_free (imap_store, response);

	imap_folder_effectively_unsubscribed (imap_store, folder_name, ex);
}

#if 0
static gboolean
folder_flags_have_changed (CamelFolder *folder)
{
	CamelMessageInfo *info;
	int i, max;
	
	max = camel_folder_summary_count (folder->summary);
	for (i = 0; i < max; i++) {
		info = camel_folder_summary_index (folder->summary, i);
		if (!info)
			continue;
		if (info->flags & CAMEL_MESSAGE_FOLDER_FLAGGED) {
			return TRUE;
		}
	}
	
	return FALSE;
}
#endif


gboolean
camel_imap_store_connected (CamelImapStore *store, CamelException *ex)
{
	if (store->istream == NULL || !store->connected)
		return camel_service_connect (CAMEL_SERVICE (store), ex);
	return TRUE;
}


/* FIXME: please god, when will the hurting stop? Thus function is so
   fucking broken it's not even funny. */
ssize_t
camel_imap_store_readline (CamelImapStore *store, char **dest, CamelException *ex)
{
	CamelStreamBuffer *stream;
	char linebuf[1024];
	GByteArray *ba;
	ssize_t nread;
	
	g_return_val_if_fail (CAMEL_IS_IMAP_STORE (store), -1);
	g_return_val_if_fail (dest, -1);
	
	*dest = NULL;
	
	/* Check for connectedness. Failed (or cancelled) operations will
	 * close the connection. We can't expect a read to have any
	 * meaning if we reconnect, so always set an exception.
	 */
	
	if (!camel_imap_store_connected (store, ex)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_NOT_CONNECTED,
				     g_strerror (errno));
		return -1;
	}
	
	stream = CAMEL_STREAM_BUFFER (store->istream);
	
	ba = g_byte_array_new ();
	while ((nread = camel_stream_buffer_gets (stream, linebuf, sizeof (linebuf))) > 0) {
		g_byte_array_append (ba, linebuf, nread);
		if (linebuf[nread - 1] == '\n')
			break;
	}
	
	if (nread <= 0) {
		if (errno == EINTR)
			camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL, _("Operation cancelled"));
		else
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					      _("Server unexpectedly disconnected: %s"),
					      g_strerror (errno));
		
		camel_service_disconnect (CAMEL_SERVICE (store), FALSE, NULL);
		g_byte_array_free (ba, TRUE);
		return -1;
	}
	
	if (camel_verbose_debug) {
		fprintf (stderr, "received: ");
		fwrite (ba->data, 1, ba->len, stderr);
	}
	
	/* camel-imap-command.c:imap_read_untagged expects the CRLFs
           to be stripped off and be nul-terminated *sigh* */
	nread = ba->len - 1;
	ba->data[nread] = '\0';
	if (ba->data[nread - 1] == '\r') {
		ba->data[nread - 1] = '\0';
		nread--;
	}
	
	*dest = ba->data;
	g_byte_array_free (ba, FALSE);
	
	return nread;
}
