/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-imap-store.c : class for an imap store */

/*
 *  Authors:
 *    Dan Winship <danw@ximian.com>
 *    Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2000, 2003 Ximian, Inc.
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
#include "camel-imap-store-summary.h"
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
#include "camel-stream-process.h"
#include "camel-tcp-stream-raw.h"
#include "camel-tcp-stream-ssl.h"
#include "camel-url.h"
#include "camel-sasl.h"
#include "camel-utf8.h"
#include "camel-string-utils.h"

#include "camel-imap-private.h"
#include "camel-private.h"

#include "camel-debug.h"

#define d(x) 

/* Specified in RFC 2060 */
#define IMAP_PORT 143
#define SIMAP_PORT 993

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
static CamelFolder *imap_get_junk(CamelStore *store, CamelException *ex);
static CamelFolder *imap_get_trash(CamelStore *store, CamelException *ex);
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


static void imap_folder_effectively_unsubscribed(CamelImapStore *imap_store, const char *folder_name, CamelException *ex);
static gboolean imap_check_folder_still_extant (CamelImapStore *imap_store, const char *full_name,  CamelException *ex);
static void imap_forget_folder(CamelImapStore *imap_store, const char *folder_name, CamelException *ex);
static void imap_set_server_level (CamelImapStore *store);

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
	camel_store_class->get_trash = imap_get_trash;
	camel_store_class->get_junk = imap_get_junk;
	
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

	/* This frees current_folder, folders, authtypes, streams, and namespace. */
	camel_service_disconnect((CamelService *)imap_store, TRUE, NULL);

	if (imap_store->summary) {
		camel_store_summary_save((CamelStoreSummary *)imap_store->summary);
		camel_object_unref(imap_store->summary);
	}
	
	if (imap_store->base_url)
		g_free (imap_store->base_url);
	if (imap_store->storage_path)
		g_free (imap_store->storage_path);
}

static void
camel_imap_store_init (gpointer object, gpointer klass)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (object);
	
	imap_store->istream = NULL;
	imap_store->ostream = NULL;
	
	imap_store->dir_sep = '\0';
	imap_store->current_folder = NULL;
	imap_store->connected = FALSE;
	imap_store->preauthed = FALSE;
	
	imap_store->tag_prefix = imap_tag_prefix++;
	if (imap_tag_prefix > 'Z')
		imap_tag_prefix = 'A';
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
	char *tmp;
	CamelURL *summary_url;

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
		g_free(imap_store->namespace);
		imap_store->namespace = g_strdup (camel_url_get_param (url, "namespace"));
	}
	if (camel_url_get_param (url, "check_all"))
		imap_store->parameters |= IMAP_PARAM_CHECK_ALL;
	if (camel_url_get_param (url, "filter")) {
		imap_store->parameters |= IMAP_PARAM_FILTER_INBOX;
		store->flags |= CAMEL_STORE_FILTER_INBOX;
	}
	if (camel_url_get_param (url, "filter_junk"))
		imap_store->parameters |= IMAP_PARAM_FILTER_JUNK;
	if (camel_url_get_param (url, "filter_junk_inbox"))
		imap_store->parameters |= IMAP_PARAM_FILTER_JUNK_INBOX;

	/* setup/load the store summary */
	tmp = alloca(strlen(imap_store->storage_path)+32);
	sprintf(tmp, "%s/.ev-store-summary", imap_store->storage_path);
	imap_store->summary = camel_imap_store_summary_new();
	camel_store_summary_set_filename((CamelStoreSummary *)imap_store->summary, tmp);
	summary_url = camel_url_new(imap_store->base_url, NULL);
	camel_store_summary_set_uri_base((CamelStoreSummary *)imap_store->summary, summary_url);
	camel_url_free(summary_url);
	if (camel_store_summary_load((CamelStoreSummary *)imap_store->summary) == 0) {
		CamelImapStoreSummary *is = imap_store->summary;

		if (is->namespace) {
			/* if namespace has changed, clear folder list */
			if (imap_store->namespace && strcmp(imap_store->namespace, is->namespace->full_name) != 0) {
				camel_store_summary_clear((CamelStoreSummary *)is);
			} else {
				imap_store->namespace = g_strdup(is->namespace->full_name);
				imap_store->dir_sep = is->namespace->sep;
			}
		}
 
		imap_store->capabilities = is->capabilities;
		imap_set_server_level(imap_store);
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
		
		/* make sure this is an arg we're supposed to handle */
		if ((tag & CAMEL_ARG_TAG) <= CAMEL_IMAP_STORE_ARG_FIRST ||
		    (tag & CAMEL_ARG_TAG) >= CAMEL_IMAP_STORE_ARG_FIRST + 100)
			continue;

		switch (tag) {
		case CAMEL_IMAP_STORE_NAMESPACE:
			if (strcmp (store->namespace, args->argv[i].ca_str) != 0) {
				g_free (store->namespace);
				store->namespace = g_strdup (args->argv[i].ca_str);
				/* the current imap code will need to do a reconnect for this to take effect */
				/*reconnect = TRUE;*/
			}
			break;
		case CAMEL_IMAP_STORE_OVERRIDE_NAMESPACE:
			flags = args->argv[i].ca_int ? IMAP_PARAM_OVERRIDE_NAMESPACE : 0;
			flags |= (store->parameters & ~IMAP_PARAM_OVERRIDE_NAMESPACE);
			
			if (store->parameters != flags) {
				store->parameters = flags;
				/* the current imap code will need to do a reconnect for this to take effect */
				/*reconnect = TRUE;*/
			}
			break;
		case CAMEL_IMAP_STORE_CHECK_ALL:
			flags = args->argv[i].ca_int ? IMAP_PARAM_CHECK_ALL : 0;
			flags |= (store->parameters & ~IMAP_PARAM_CHECK_ALL);
			store->parameters = flags;
			/* no need to reconnect for this option to take effect... */
			break;
		case CAMEL_IMAP_STORE_FILTER_INBOX:
			flags = args->argv[i].ca_int ? IMAP_PARAM_FILTER_INBOX : 0;
			flags |= (store->parameters & ~IMAP_PARAM_FILTER_INBOX);
			store->parameters = flags;
			/* no need to reconnect for this option to take effect... */
			break;
		case CAMEL_IMAP_STORE_FILTER_JUNK:
			flags = args->argv[i].ca_int ? IMAP_PARAM_FILTER_JUNK : 0;
			store->parameters = flags | (store->parameters & ~IMAP_PARAM_FILTER_JUNK);
			break;
		case CAMEL_IMAP_STORE_FILTER_JUNK_INBOX:
			flags = args->argv[i].ca_int ? IMAP_PARAM_FILTER_JUNK_INBOX : 0;
			store->parameters = flags | (store->parameters & ~IMAP_PARAM_FILTER_JUNK_INBOX);
			break;
		default:
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
			*args->argv[i].ca_str = store->namespace;
			break;
		case CAMEL_IMAP_STORE_OVERRIDE_NAMESPACE:
			*args->argv[i].ca_int = store->parameters & IMAP_PARAM_OVERRIDE_NAMESPACE ? TRUE : FALSE;
			break;
		case CAMEL_IMAP_STORE_CHECK_ALL:
			*args->argv[i].ca_int = store->parameters & IMAP_PARAM_CHECK_ALL ? TRUE : FALSE;
			break;
		case CAMEL_IMAP_STORE_FILTER_INBOX:
			*args->argv[i].ca_int = store->parameters & IMAP_PARAM_FILTER_INBOX ? TRUE : FALSE;
			break;
		case CAMEL_IMAP_STORE_FILTER_JUNK:
			*args->argv[i].ca_int = store->parameters & IMAP_PARAM_FILTER_JUNK ? TRUE : FALSE;
			break;
		case CAMEL_IMAP_STORE_FILTER_JUNK_INBOX:
			*args->argv[i].ca_int = store->parameters & IMAP_PARAM_FILTER_JUNK_INBOX ? TRUE : FALSE;
			break;
		default:
			/* error? */
			break;
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
	
	CAMEL_SERVICE_ASSERT_LOCKED (store, connect_lock);
	
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
			if (strcasecmp (capa, capabilities[i].name) == 0) {
				store->capabilities |= capabilities[i].flag;
				break;
			}
		}
	}
	g_free (result);
	
	imap_set_server_level (store);
	
	if (store->summary->capabilities != store->capabilities) {
		store->summary->capabilities = store->capabilities;
		camel_store_summary_touch((CamelStoreSummary *)store->summary);
		camel_store_summary_save((CamelStoreSummary *)store->summary);
	}
	
	return TRUE;
}

enum {
	USE_SSL_NEVER,
	USE_SSL_ALWAYS,
	USE_SSL_WHEN_POSSIBLE
};

#define SSL_PORT_FLAGS (CAMEL_TCP_STREAM_SSL_ENABLE_SSL2 | CAMEL_TCP_STREAM_SSL_ENABLE_SSL3)
#define STARTTLS_FLAGS (CAMEL_TCP_STREAM_SSL_ENABLE_TLS)

static gboolean
connect_to_server (CamelService *service, int ssl_mode, int try_starttls, CamelException *ex)
{
	CamelImapStore *store = (CamelImapStore *) service;
	CamelImapResponse *response;
	CamelStream *tcp_stream;
	CamelSockOptData sockopt;
	gboolean force_imap4 = FALSE;
	struct hostent *h;
	int clean_quit;
	int port, ret;
	char *buf;
	
	if (!(h = camel_service_gethost (service, ex)))
		return FALSE;
	
	port = service->url->port ? service->url->port : 143;
	
	if (ssl_mode != USE_SSL_NEVER) {
#ifdef HAVE_SSL
		if (try_starttls) {
			tcp_stream = camel_tcp_stream_ssl_new_raw (service->session, service->url->host, STARTTLS_FLAGS);
		} else {
			port = service->url->port ? service->url->port : 993;
			tcp_stream = camel_tcp_stream_ssl_new (service->session, service->url->host, SSL_PORT_FLAGS);
		}
#else
		if (!try_starttls)
			port = service->url->port ? service->url->port : 993;
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      _("Could not connect to %s (port %d): %s"),
				      service->url->host, port,
				      _("SSL unavailable"));
		
		camel_free_host (h);
		
		return FALSE;
#endif /* HAVE_SSL */
	} else {
		tcp_stream = camel_tcp_stream_raw_new ();
	}
	
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
		
		camel_object_unref (tcp_stream);
		
		return FALSE;
	}
	
	store->ostream = tcp_stream;
	store->istream = camel_stream_buffer_new (tcp_stream, CAMEL_STREAM_BUFFER_READ);
	
	store->connected = TRUE;
	store->preauthed = FALSE;
	store->command = 0;

	/* Disable Nagle - we send a lot of small requests which nagle slows down */
	sockopt.option = CAMEL_SOCKOPT_NODELAY;
	sockopt.value.no_delay = TRUE;
	camel_tcp_stream_setsockopt((CamelTcpStream *)tcp_stream, &sockopt);

	/* Set keepalive - needed for some hosts/router configurations, we're idle a lot */
	sockopt.option = CAMEL_SOCKOPT_KEEPALIVE;
	sockopt.value.keep_alive = TRUE;
	camel_tcp_stream_setsockopt((CamelTcpStream *)tcp_stream, &sockopt);

	/* Read the greeting, if any, and deal with PREAUTH */
	if (camel_imap_store_readline (store, &buf, ex) < 0) {
		if (store->istream) {
			camel_object_unref (store->istream);
			store->istream = NULL;
		}
		
		if (store->ostream) {
			camel_object_unref (store->ostream);
			store->ostream = NULL;
		}
		
		store->connected = FALSE;
		
		return FALSE;
	}
	
	if (!strncmp(buf, "* PREAUTH", 9))
		store->preauthed = TRUE;
	
	if (strstr (buf, "Courier-IMAP")) {
		/* Courier-IMAP is braindamaged. So far this flag only
		 * works around the fact that Courier-IMAP is known to
		 * give invalid BODY responses seemingly because its
		 * MIME parser sucks. In any event, we can't rely on
		 * them so we always have to request the full messages
		 * rather than getting individual parts. */
		store->braindamaged = TRUE;
	} else if (strstr (buf, "WEB.DE") || strstr (buf, "Mail2World")) {
		/* This is a workaround for servers which advertise
		 * IMAP4rev1 but which can sometimes subtly break in
		 * various ways if we try to use IMAP4rev1 queries.
		 *
		 * WEB.DE: when querying for HEADER.FIELDS.NOT, it
		 * returns an empty literal for the headers. Many
		 * complaints about empty message-list fields on the
		 * mailing lists and probably a few bugzilla bugs as
		 * well.
		 *
		 * Mail2World (aka NamePlanet): When requesting
		 * message info's, it ignores the fact that we
		 * requested BODY.PEEK[HEADER.FIELDS.NOT (RECEIVED)]
		 * and so the responses are incomplete. See bug #58766
		 * for details.
		 **/
		force_imap4 = TRUE;
	}
	
	g_free (buf);
	
	/* get the imap server capabilities */
	if (!imap_get_capability (service, ex)) {
		if (store->istream) {
			camel_object_unref (store->istream);
			store->istream = NULL;
		}
		
		if (store->ostream) {
			camel_object_unref (store->ostream);
			store->ostream = NULL;
		}
		
		store->connected = FALSE;
		return FALSE;
	}
	
	if (force_imap4) {
		store->capabilities &= ~IMAP_CAPABILITY_IMAP4REV1;
		store->server_level = IMAP_LEVEL_IMAP4;
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
		camel_object_unref (store->istream);
		camel_object_unref (store->ostream);
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
		if (store->istream) {
			camel_object_unref (store->istream);
			store->istream = NULL;
		}
		
		if (store->ostream) {
			camel_object_unref (store->ostream);
			store->ostream = NULL;
		}
		
		store->connected = FALSE;
		
		return FALSE;
	}
	
	return TRUE;
	
 exception:
	
	if (clean_quit && store->connected) {
		/* try to disconnect cleanly */
		response = camel_imap_command (store, NULL, ex, "LOGOUT");
		if (response)
			camel_imap_response_free_without_processing (store, response);
	}
	
	if (store->istream) {
		camel_object_unref (store->istream);
		store->istream = NULL;
	}
	
	if (store->ostream) {
		camel_object_unref (store->ostream);
		store->ostream = NULL;
	}
	
	store->connected = FALSE;
	
	return FALSE;
#endif /* HAVE_SSL */
}

static gboolean
connect_to_server_process (CamelService *service, const char *cmd, CamelException *ex)
{
	CamelImapStore *store = (CamelImapStore *) service;
	CamelStream *cmd_stream;
	int ret, i = 0;
	char *buf;
	char *cmd_copy;
	char *full_cmd;
	char *child_env[7];

	/* Put full details in the environment, in case the connection 
	   program needs them */
	buf = camel_url_to_string(service->url, 0);
	child_env[i++] = g_strdup_printf("URL=%s", buf);
	g_free(buf);

	child_env[i++] = g_strdup_printf("URLHOST=%s", service->url->host);
	if (service->url->port)
		child_env[i++] = g_strdup_printf("URLPORT=%d", service->url->port);
	if (service->url->user)
		child_env[i++] = g_strdup_printf("URLUSER=%s", service->url->user);
	if (service->url->passwd)
		child_env[i++] = g_strdup_printf("URLPASSWD=%s", service->url->passwd);
	if (service->url->path)
		child_env[i++] = g_strdup_printf("URLPATH=%s", service->url->path);
	child_env[i] = NULL;

	/* Now do %h, %u, etc. substitution in cmd */
	buf = cmd_copy = g_strdup(cmd);

	full_cmd = g_strdup("");

	for(;;) {
		char *pc;
		char *tmp;
		char *var;
		int len;

		pc = strchr(buf, '%');
	ignore:
		if (!pc) {
			tmp = g_strdup_printf("%s%s", full_cmd, buf);
			g_free(full_cmd);
			full_cmd = tmp;
			break;
		}
		
		len = pc - buf;

		var = NULL;

		switch(pc[1]) {
		case 'h':
			var = service->url->host;
			break;
		case 'u':
			var = service->url->user;
			break;
		}
		if (!var) {
			/* If there wasn't a valid %-code, with an actual
			   variable to insert, pretend we didn't see the % */
			pc = strchr(pc + 1, '%');
			goto ignore;
		}
		tmp = g_strdup_printf("%s%.*s%s", full_cmd, len, buf, var);
		g_free(full_cmd);
		full_cmd = tmp;
		buf = pc + 2;
	}
			
	g_free(cmd_copy);

	cmd_stream = camel_stream_process_new ();
	
	ret = camel_stream_process_connect (CAMEL_STREAM_PROCESS(cmd_stream),
					    full_cmd, (const char **)child_env);

	while (i)
		g_free(child_env[--i]);
		
	if (ret == -1) {
		if (errno == EINTR)
			camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL,
					     _("Connection cancelled"));
		else
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					      _("Could not connect with command \"%s\": %s"),
					      full_cmd, g_strerror (errno));
		
		camel_object_unref (cmd_stream);
		g_free (full_cmd);
		return FALSE;
	}
	g_free (full_cmd);
	
	store->ostream = cmd_stream;
	store->istream = camel_stream_buffer_new (cmd_stream, CAMEL_STREAM_BUFFER_READ);
	
	store->connected = TRUE;
	store->preauthed = FALSE;
	store->command = 0;
	
	/* Read the greeting, if any, and deal with PREAUTH */
	if (camel_imap_store_readline (store, &buf, ex) < 0) {
		if (store->istream) {
			camel_object_unref (store->istream);
			store->istream = NULL;
		}
		
		if (store->ostream) {
			camel_object_unref (store->ostream);
			store->ostream = NULL;
		}
		
		store->connected = FALSE;
		return FALSE;
	}
	if (!strncmp(buf, "* PREAUTH", 9))
		store->preauthed = TRUE;
	g_free (buf);
	
	/* get the imap server capabilities */
	if (!imap_get_capability (service, ex)) {
		if (store->istream) {
			camel_object_unref (store->istream);
			store->istream = NULL;
		}
		
		if (store->ostream) {
			camel_object_unref (store->ostream);
			store->ostream = NULL;
		}
		
		store->connected = FALSE;
		return FALSE;
	}
	
	return TRUE;
	
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
	const char *command;
#ifdef HAVE_SSL
	const char *use_ssl;
	int i, ssl_mode;
#endif
	command = camel_url_get_param (service->url, "command");
	if (command)
		return connect_to_server_process (service, command, ex);

#ifdef HAVE_SSL
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
	
	CAMEL_SERVICE_LOCK (store, connect_lock);
	connected = connect_to_server_wrapper (service, ex);
	CAMEL_SERVICE_UNLOCK (store, connect_lock);
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

/* folder_name is path name */
static CamelFolderInfo *
imap_build_folder_info(CamelImapStore *imap_store, const char *folder_name)
{
	CamelURL *url;
	const char *name;
	CamelFolderInfo *fi;

	fi = g_malloc0(sizeof(*fi));

	fi->full_name = g_strdup(folder_name);
	fi->unread = 0;
	fi->total = 0;

	url = camel_url_new (imap_store->base_url, NULL);
	g_free (url->path);
	url->path = g_strdup_printf ("/%s", folder_name);
	fi->uri = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);
	camel_url_free(url);
	name = strrchr (fi->full_name, '/');
	if (name == NULL)
		name = fi->full_name;
	else
		name++;
	fi->name = g_strdup (name);
	
	return fi;
}

static void
imap_folder_effectively_unsubscribed(CamelImapStore *imap_store, 
				     const char *folder_name, CamelException *ex)
{
	CamelFolderInfo *fi;
	CamelStoreInfo *si;

	si = camel_store_summary_path((CamelStoreSummary *)imap_store->summary, folder_name);
	if (si) {
		if (si->flags & CAMEL_STORE_INFO_FOLDER_SUBSCRIBED) {
			si->flags &= ~CAMEL_STORE_INFO_FOLDER_SUBSCRIBED;
			camel_store_summary_touch((CamelStoreSummary *)imap_store->summary);
			camel_store_summary_save((CamelStoreSummary *)imap_store->summary);
		}
		camel_store_summary_info_free((CamelStoreSummary *)imap_store->summary, si);
	}

	if (imap_store->renaming) {
		/* we don't need to emit a "folder_unsubscribed" signal
                   if we are in the process of renaming folders, so we
                   are done here... */
		return;

	}

	fi = imap_build_folder_info(imap_store, folder_name);
	camel_object_trigger_event (CAMEL_OBJECT (imap_store), "folder_unsubscribed", fi);
	camel_folder_info_free (fi);
}

static void
imap_forget_folder (CamelImapStore *imap_store, const char *folder_name, CamelException *ex)
{
	CamelFolderSummary *summary;
	CamelImapMessageCache *cache;
	char *summary_file, *state_file;
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
	
	camel_object_unref (cache);
	camel_object_unref (summary);
	
	unlink (summary_file);
	g_free (summary_file);
	
	journal_file = g_strdup_printf ("%s/journal", folder_dir);
	unlink (journal_file);
	g_free (journal_file);

	state_file = g_strdup_printf ("%s/cmeta", folder_dir);
	unlink (state_file);
	g_free (state_file);
	
	rmdir (folder_dir);
	g_free (folder_dir);
	
 event:

	camel_store_summary_remove_path((CamelStoreSummary *)imap_store->summary, folder_name);
	camel_store_summary_save((CamelStoreSummary *)imap_store->summary);

	fi = imap_build_folder_info(imap_store, folder_name);
	camel_object_trigger_event (CAMEL_OBJECT (imap_store), "folder_deleted", fi);
	camel_folder_info_free (fi);
}

static gboolean
imap_check_folder_still_extant (CamelImapStore *imap_store, const char *full_name, 
				CamelException *ex)
{
	CamelImapResponse *response;

	response = camel_imap_command (imap_store, NULL, ex, "LIST \"\" %F",
				       full_name);

	if (response) {
		gboolean stillthere = response->untagged->len != 0;

		camel_imap_response_free_without_processing (imap_store, response);

		return stillthere;
	}

	/* if the command was rejected, there must be some other error,
	   assume it worked so we dont blow away the folder unecessarily */
	return TRUE;
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
	
	folders = camel_object_bag_list(CAMEL_STORE (store)->folders);
	
	for (i = 0; i <folders->len; i++) {
		CamelFolder *folder = folders->pdata[i];

		/* NB: we can have vtrash folders also in our store ... bit hacky */
		if (!CAMEL_IS_IMAP_FOLDER(folder)) {
			camel_object_unref(folder);
			continue;
		}

		CAMEL_IMAP_FOLDER (folder)->need_rescan = TRUE;
		if (!camel_exception_is_set(ex))
			CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(folder))->refresh_info(folder, ex);

		if (camel_exception_is_set (ex) &&
		    imap_check_folder_still_extant (store, folder->full_name, ex) == FALSE) {
			gchar *namedup;
			
			/* the folder was deleted (may happen when we come back online
			 * after being offline */
			
			namedup = g_strdup (folder->full_name);
			camel_object_unref(folder);
			imap_folder_effectively_unsubscribed (store, namedup, ex);
			imap_forget_folder (store, namedup, ex);
			g_free (namedup);
		} else
			camel_object_unref(folder);
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
	
	CAMEL_SERVICE_ASSERT_LOCKED (store, connect_lock);
	
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
	
	camel_object_unref (sasl);
	
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
	
	camel_object_unref (sasl);
	
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
	const char *auth_domain;
	
	CAMEL_SERVICE_ASSERT_LOCKED (store, connect_lock);
	auth_domain = camel_url_get_param (service->url, "auth-domain");
	
	if (store->preauthed) {
		if (camel_verbose_debug)
			fprintf(stderr, "Server %s has preauthenticated us.\n",
				service->url->host);
		return TRUE;
	}

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
			camel_session_forget_password (session, service, auth_domain, "password", ex);
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
				camel_session_get_password (session, service, auth_domain,
							    prompt, "password", CAMEL_SESSION_PASSWORD_SECRET, ex);
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

static gboolean
can_work_offline (CamelDiscoStore *disco_store)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (disco_store);
	
	return camel_store_summary_count((CamelStoreSummary *)store->summary) != 0;
}

static gboolean
imap_connect_online (CamelService *service, CamelException *ex)
{
	CamelImapStore *store = CAMEL_IMAP_STORE (service);
	CamelDiscoStore *disco_store = CAMEL_DISCO_STORE (service);
	CamelImapResponse *response;
	/*struct _namespaces *namespaces;*/
	char *result, *name, *path;
	int i;
	size_t len;
	CamelImapStoreNamespace *ns;

	CAMEL_SERVICE_LOCK (store, connect_lock);
	if (!connect_to_server_wrapper (service, ex) ||
	    !imap_auth_loop (service, ex)) {
		CAMEL_SERVICE_UNLOCK (store, connect_lock);
		camel_service_disconnect (service, TRUE, NULL);
		return FALSE;
	}
	
	/* Get namespace and hierarchy separator */
	if ((store->capabilities & IMAP_CAPABILITY_NAMESPACE) &&
	    !(store->parameters & IMAP_PARAM_OVERRIDE_NAMESPACE)) {
		response = camel_imap_command (store, NULL, ex, "NAMESPACE");
		if (!response)
			goto done;
		
		result = camel_imap_response_extract (store, response, "NAMESPACE", ex);
		if (!result)
			goto done;
		
#if 0
		/* new code... */
		namespaces = imap_parse_namespace_response (result);
		imap_namespaces_destroy (namespaces);
		/* end new code */
#endif
		
		name = camel_strstrcase (result, "NAMESPACE ((");
		if (name) {
			char *sep;
			
			name += 12;
			store->namespace = imap_parse_string ((const char **) &name, &len);
			if (name && *name++ == ' ') {
				sep = imap_parse_string ((const char **) &name, &len);
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
			imap_parse_list_response (store, result, NULL, &store->dir_sep, NULL);
			g_free (result);
		}
		if (!store->dir_sep) {
			store->dir_sep = '/';	/* Guess */
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
	
	ns = camel_imap_store_summary_namespace_new(store->summary, store->namespace, store->dir_sep);
	camel_imap_store_summary_namespace_set(store->summary, ns);
	
	if (CAMEL_STORE (store)->flags & CAMEL_STORE_SUBSCRIPTIONS) {
		gboolean haveinbox = FALSE;
		GPtrArray *folders;
		char *pattern;
		
		/* this pre-fills the summary, and checks that lsub is useful */
		folders = g_ptr_array_new ();
		pattern = g_strdup_printf ("%s*", store->namespace);
		get_folders_online (store, pattern, folders, TRUE, ex);
		g_free (pattern);
		
		for (i = 0; i < folders->len; i++) {
			CamelFolderInfo *fi = folders->pdata[i];
			
			haveinbox = haveinbox || !strcasecmp (fi->full_name, "INBOX");
			
			if (fi->flags & (CAMEL_IMAP_FOLDER_MARKED | CAMEL_IMAP_FOLDER_UNMARKED))
				store->capabilities |= IMAP_CAPABILITY_useful_lsub;
			camel_folder_info_free (fi);
		}
		
		/* if the namespace is under INBOX, check INBOX explicitly */
		if (!g_ascii_strncasecmp (store->namespace, "INBOX", 5) && !camel_exception_is_set (ex)) {
			gboolean just_subscribed = FALSE;
			gboolean need_subscribe = FALSE;
			
		recheck:
			g_ptr_array_set_size (folders, 0);
			get_folders_online (store, "INBOX", folders, TRUE, ex);
			
			for (i = 0; i < folders->len; i++) {
				CamelFolderInfo *fi = folders->pdata[i];
				
				/* this should always be TRUE if folders->len > 0 */
				if (!strcasecmp (fi->full_name, "INBOX")) {
					haveinbox = TRUE;
					
					/* if INBOX is marked as \NoSelect then it is probably
					   because it has not been subscribed to */
					if (!need_subscribe)
						need_subscribe = fi->flags & CAMEL_FOLDER_NOSELECT;
				}
				
				camel_folder_info_free (fi);
			}
			
			need_subscribe = !haveinbox || need_subscribe;
			if (need_subscribe && !just_subscribed && !camel_exception_is_set (ex)) {
				/* in order to avoid user complaints, force a subscription to INBOX */
				response = camel_imap_command (store, NULL, ex, "SUBSCRIBE INBOX");
				if (response != NULL) {
					/* force a re-check which will pre-fill the summary and
					   also get any folder flags present on the INBOX */
					camel_imap_response_free (store, response);
					just_subscribed = TRUE;
					goto recheck;
				}
			}
		}
		
		g_ptr_array_free (folders, TRUE);
	}
	
	path = g_strdup_printf ("%s/journal", store->storage_path);
	disco_store->diary = camel_disco_diary_new (disco_store, path, ex);
	g_free (path);
	
 done:
	/* save any changes we had */
	camel_store_summary_save((CamelStoreSummary *)store->summary);

	CAMEL_SERVICE_UNLOCK (store, connect_lock);
	
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
	char *path;

	path = g_strdup_printf ("%s/journal", store->storage_path);
	disco_store->diary = camel_disco_diary_new (disco_store, path, ex);
	g_free (path);
	if (!disco_store->diary)
		return FALSE;
	
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
		camel_object_unref (store->current_folder);
		store->current_folder = NULL;
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
		camel_object_unref (disco->diary);
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
		camel_object_unref (store->istream);
		store->istream = NULL;
	}
	
	if (store->ostream) {
		camel_object_unref (store->ostream);
		store->ostream = NULL;
	}
	
	imap_disconnect_offline (service, clean, ex);
	
	return TRUE;
}


static gboolean
imap_summary_is_dirty (CamelFolderSummary *summary)
{
	CamelMessageInfo *info;
	int max, i;
	
	max = camel_folder_summary_count (summary);
	for (i = 0; i < max; i++) {
		info = camel_folder_summary_index (summary, i);
		if (info && (info->flags & CAMEL_MESSAGE_FOLDER_FLAGGED))
			return TRUE;
	}
	
	return FALSE;
}

static void
imap_noop (CamelStore *store, CamelException *ex)
{
	CamelImapStore *imap_store = (CamelImapStore *) store;
	CamelDiscoStore *disco = (CamelDiscoStore *) store;
	CamelImapResponse *response;
	CamelFolder *current_folder;
	
	if (camel_disco_store_status (disco) != CAMEL_DISCO_STORE_ONLINE)
		return;
	
	CAMEL_SERVICE_LOCK (imap_store, connect_lock);
	
	current_folder = imap_store->current_folder;
	if (current_folder && imap_summary_is_dirty (current_folder->summary)) {
		/* let's sync the flags instead.  NB: must avoid folder lock */
		((CamelFolderClass *)((CamelObject *)current_folder)->klass)->sync(current_folder, FALSE, ex);
	} else {
		response = camel_imap_command (imap_store, NULL, ex, "NOOP");
		if (response)
			camel_imap_response_free (imap_store, response);
	}
	
	CAMEL_SERVICE_UNLOCK (imap_store, connect_lock);
}

static CamelFolder *
imap_get_trash(CamelStore *store, CamelException *ex)
{
	CamelFolder *folder = CAMEL_STORE_CLASS(parent_class)->get_trash(store, ex);

	if (folder) {
		char *state = g_build_filename(((CamelImapStore *)store)->storage_path, "system", "Trash.cmeta", NULL);

		camel_object_set(folder, NULL, CAMEL_OBJECT_STATE_FILE, state, NULL);
		g_free(state);
		/* no defaults? */
		camel_object_state_read(folder);
	}

	return folder;
}

static CamelFolder *
imap_get_junk(CamelStore *store, CamelException *ex)
{
	CamelFolder *folder = CAMEL_STORE_CLASS(parent_class)->get_junk(store, ex);

	if (folder) {
		char *state = g_build_filename(((CamelImapStore *)store)->storage_path, "system", "Junk.cmeta", NULL);

		camel_object_set(folder, NULL, CAMEL_OBJECT_STATE_FILE, state, NULL);
		g_free(state);
		/* no defaults? */
		camel_object_state_read(folder);
	}

	return folder;
}

static guint
hash_folder_name (gconstpointer key)
{
	if (g_ascii_strcasecmp (key, "INBOX") == 0)
		return g_str_hash ("INBOX");
	else
		return g_str_hash (key);
}

static gint
compare_folder_name (gconstpointer a, gconstpointer b)
{
	gconstpointer aname = a, bname = b;

	if (g_ascii_strcasecmp (a, "INBOX") == 0)
		aname = "INBOX";
	if (g_ascii_strcasecmp (b, "INBOX") == 0)
		bname = "INBOX";
	return g_str_equal (aname, bname);
}

struct imap_status_item {
	struct imap_status_item *next;
	char *name;
	guint32 value;
};

static void
imap_status_item_free (struct imap_status_item *items)
{
	struct imap_status_item *next;
	
	while (items != NULL) {
		next = items->next;
		g_free (items->name);
		g_free (items);
		items = next;
	}
}

static struct imap_status_item *
get_folder_status (CamelImapStore *imap_store, const char *folder_name, const char *type)
{
	struct imap_status_item *items, *item, *tail;
	CamelImapResponse *response;
	char *status, *name, *p;
	
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
		return NULL;
	}
	
	if (!(status = camel_imap_response_extract (imap_store, response, "STATUS", NULL)))
		return NULL;
	
	p = status + strlen ("* STATUS ");
	while (*p == ' ')
		p++;
	
	/* skip past the mailbox string */
	if (*p == '"') {
		p++;
		while (*p != '\0') {
			if (*p == '"' && p[-1] != '\\') {
				p++;
				break;
			}
			
			p++;
		}
	} else {
		while (*p != ' ')
			p++;
	}
	
	while (*p == ' ')
		p++;
	
	if (*p++ != '(') {
		g_free (status);
		return NULL;
	}
	
	while (*p == ' ')
		p++;
	
	if (*p == ')') {
		g_free (status);
		return NULL;
	}
	
	items = NULL;
	tail = (struct imap_status_item *) &items;
	
	do {
		name = p;
		while (*p != ' ')
			p++;
		
		item = g_malloc (sizeof (struct imap_status_item));
		item->next = NULL;
		item->name = g_strndup (name, p - name);
		item->value = strtoul (p, &p, 10);
		
		tail->next = item;
		tail = item;
		
		while (*p == ' ')
			p++;
	} while (*p != ')');
	
	g_free (status);
	
	return items;
}

static CamelFolder *
get_folder_online (CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	CamelImapResponse *response;
	CamelFolder *new_folder;
	char *folder_dir, *storage_path;

	if (!camel_imap_store_connected (imap_store, ex))
		return NULL;
	
	if (!strcasecmp (folder_name, "INBOX"))
		folder_name = "INBOX";

	/* Lock around the whole lot to check/create atomically */
	CAMEL_SERVICE_LOCK (imap_store, connect_lock);
	if (imap_store->current_folder) {
		camel_object_unref (imap_store->current_folder);
		imap_store->current_folder = NULL;
	}
	response = camel_imap_command (imap_store, NULL, ex, "SELECT %F", folder_name);
	if (!response) {
		char *folder_real, *parent_name, *parent_real;
		const char *c;
		
		if (camel_exception_get_id(ex) == CAMEL_EXCEPTION_USER_CANCEL) {
			CAMEL_SERVICE_UNLOCK (imap_store, connect_lock);
			return NULL;
		}
		
		camel_exception_clear (ex);
		
		if (!(flags & CAMEL_STORE_FOLDER_CREATE)) {
			CAMEL_SERVICE_UNLOCK (imap_store, connect_lock);
			camel_exception_setv (ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
					      _("No such folder %s"), folder_name);
			return NULL;
		}
		
		if ((parent_name = strrchr (folder_name, '/'))) {
			parent_name = g_strndup (folder_name, parent_name - folder_name);
			parent_real = camel_imap_store_summary_path_to_full (imap_store->summary, parent_name, imap_store->dir_sep);
		} else {
			parent_real = NULL;
		}
		
		c = parent_name ? parent_name : folder_name;
		while (*c && *c != imap_store->dir_sep && !strchr ("#%*", *c))
			c++;
		
		if (*c != '\0') {
			CAMEL_SERVICE_UNLOCK (imap_store, connect_lock);
			camel_exception_setv (ex, CAMEL_EXCEPTION_FOLDER_INVALID_PATH,
					      _("The folder name \"%s\" is invalid because it contains the character \"%c\""),
					      folder_name, *c);
			g_free (parent_name);
			g_free (parent_real);
			return NULL;
		}
		
		if (parent_real != NULL) {
			gboolean need_convert = FALSE;
			char *resp, *thisone;
			guint32 flags;
			int i;
			
			if (!(response = camel_imap_command (imap_store, NULL, ex, "LIST \"\" %S", parent_real))) {
				CAMEL_SERVICE_UNLOCK (imap_store, connect_lock);
				g_free (parent_name);
				g_free (parent_real);
				return NULL;
			}
			
			/* FIXME: does not handle unexpected circumstances very well */
			for (i = 0; i < response->untagged->len; i++) {
				resp = response->untagged->pdata[i];
				
				if (!imap_parse_list_response (imap_store, resp, &flags, NULL, &thisone))
					continue;
				
				if (!strcmp (parent_name, thisone)) {
					if (flags & CAMEL_FOLDER_NOINFERIORS)
						need_convert = TRUE;
				}
				
				g_free (thisone);
			}
			
			camel_imap_response_free (imap_store, response);
			
			/* if not, check if we can delete it and recreate it */
			if (need_convert) {
				struct imap_status_item *items, *item;
				guint32 messages = 0;
				CamelException lex;
				char *name;
				
				item = items = get_folder_status (imap_store, parent_name, "MESSAGES");
				while (item != NULL) {
					if (!g_ascii_strcasecmp (item->name, "MESSAGES")) {
						messages = item->value;
						break;
					}
					
					item = item->next;
				}
				
				imap_status_item_free (items);
				
				if (messages > 0) {
					camel_exception_set (ex, CAMEL_EXCEPTION_FOLDER_INVALID_STATE,
							     _("The parent folder is not allowed to contain subfolders"));
					CAMEL_SERVICE_UNLOCK (imap_store, connect_lock);
					g_free (parent_name);
					g_free (parent_real);
					return NULL;
				}
				
				/* delete the old parent and recreate it */
				camel_exception_init (&lex);
				delete_folder (store, parent_name, &lex);
				if (camel_exception_is_set (&lex)) {
					CAMEL_SERVICE_UNLOCK (imap_store, connect_lock);
					camel_exception_xfer (ex, &lex);
					g_free (parent_name);
					g_free (parent_real);
					return NULL;
				}
				
				/* add the dirsep to the end of parent_name */
				name = g_strdup_printf ("%s%c", parent_real, imap_store->dir_sep);
				response = camel_imap_command (imap_store, NULL, ex, "CREATE %S",
							       name);
				g_free (name);
				
				if (!response) {
					CAMEL_SERVICE_UNLOCK (imap_store, connect_lock);
					g_free (parent_name);
					g_free (parent_real);
					return NULL;
				} else
					camel_imap_response_free (imap_store, response);
			}
			
			g_free (parent_real);
		}
		
		g_free (parent_name);
		
		folder_real = camel_imap_store_summary_path_to_full(imap_store->summary, folder_name, imap_store->dir_sep);
		response = camel_imap_command (imap_store, NULL, ex, "CREATE %S", folder_real);
		
		if (response) {
			camel_imap_store_summary_add_from_full(imap_store->summary, folder_real, imap_store->dir_sep);

			camel_imap_response_free (imap_store, response);
			
			response = camel_imap_command (imap_store, NULL, NULL, "SELECT %F", folder_name);
		}
		g_free(folder_real);
		if (!response) {
			CAMEL_SERVICE_UNLOCK (imap_store, connect_lock);
			return NULL;
		}
	} else if (flags & CAMEL_STORE_FOLDER_EXCL) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot create folder `%s': folder exists."),
				      folder_name);
		
		camel_imap_response_free_without_processing (imap_store, response);
		
		CAMEL_SERVICE_UNLOCK (imap_store, connect_lock);
		
		return NULL;
	}

	storage_path = g_strdup_printf("%s/folders", imap_store->storage_path);
	folder_dir = e_path_to_physical (storage_path, folder_name);
	g_free(storage_path);
	new_folder = camel_imap_folder_new (store, folder_name, folder_dir, ex);
	g_free (folder_dir);
	if (new_folder) {
		CamelException local_ex;

		imap_store->current_folder = new_folder;
		camel_object_ref (new_folder);
		camel_exception_init (&local_ex);
		camel_imap_folder_selected (new_folder, response, &local_ex);

		if (camel_exception_is_set (&local_ex)) {
			camel_exception_xfer (ex, &local_ex);
			camel_object_unref (imap_store->current_folder);
			imap_store->current_folder = NULL;
			camel_object_unref (new_folder);
			new_folder = NULL;
		}
	}
	camel_imap_response_free_without_processing (imap_store, response);
	
	CAMEL_SERVICE_UNLOCK (imap_store, connect_lock);
	
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
	
	if (!strcasecmp (folder_name, "INBOX"))
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
		
		CAMEL_SERVICE_LOCK (imap_store, connect_lock);
		
		if (imap_store->current_folder)
			camel_object_unref (imap_store->current_folder);
		/* no need to actually create a CamelFolder for INBOX */
		imap_store->current_folder = NULL;
		
		CAMEL_SERVICE_UNLOCK (imap_store, connect_lock);
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
manage_subscriptions (CamelStore *store, const char *old_name, gboolean subscribe)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	CamelStoreInfo *si;
	int olen = strlen(old_name);
	const char *path;
	int i, count;

	count = camel_store_summary_count((CamelStoreSummary *)imap_store->summary);
	for (i=0;i<count;i++) {
		si = camel_store_summary_index((CamelStoreSummary *)imap_store->summary, i);
		if (si) {
			path = camel_store_info_path(imap_store->summary, si);
			if (strncmp(path, old_name, olen) == 0) {
				if (subscribe)
					subscribe_folder(store, path, NULL);
				else
					unsubscribe_folder(store, path, NULL);
			}
			camel_store_summary_info_free((CamelStoreSummary *)imap_store->summary, si);
		}
	}
}

static void
rename_folder_info (CamelImapStore *imap_store, const char *old_name, const char *new_name)
{
	int i, count;
	CamelStoreInfo *si;
	int olen = strlen(old_name);
	const char *path;
	char *npath, *nfull;

	count = camel_store_summary_count((CamelStoreSummary *)imap_store->summary);
	for (i=0;i<count;i++) {
		si = camel_store_summary_index((CamelStoreSummary *)imap_store->summary, i);
		if (si == NULL)
			continue;
		path = camel_store_info_path(imap_store->summary, si);
		if (strncmp(path, old_name, olen) == 0) {
			if (strlen(path) > olen)
				npath = g_strdup_printf("%s/%s", new_name, path+olen+1);
			else
				npath = g_strdup(new_name);
			nfull = camel_imap_store_summary_path_to_full(imap_store->summary, npath, imap_store->dir_sep);
			
			/* workaround for broken server (courier uses '.') that doesn't rename
			   subordinate folders as required by rfc 2060 */
			if (imap_store->dir_sep == '.') {
				CamelImapResponse *response;

				response = camel_imap_command (imap_store, NULL, NULL, "RENAME %F %S", path, nfull);
				if (response)
					camel_imap_response_free (imap_store, response);
			}

			camel_store_info_set_string((CamelStoreSummary *)imap_store->summary, si, CAMEL_STORE_INFO_PATH, npath);
			camel_store_info_set_string((CamelStoreSummary *)imap_store->summary, si, CAMEL_IMAP_STORE_INFO_FULL_NAME, nfull);

			camel_store_summary_touch((CamelStoreSummary *)imap_store->summary);
			g_free(nfull);
			g_free(npath);
		}
		camel_store_summary_info_free((CamelStoreSummary *)imap_store->summary, si);
	}
}

static void
rename_folder (CamelStore *store, const char *old_name, const char *new_name_in, CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	CamelImapResponse *response;
	char *oldpath, *newpath, *storage_path, *new_name;
	
	if (!camel_disco_store_check_online (CAMEL_DISCO_STORE (store), ex))
		return;
	
	/* make sure this folder isn't currently SELECTed - it's
           actually possible to rename INBOX but if you do another
           INBOX will immediately be created by the server */
	response = camel_imap_command (imap_store, NULL, ex, "SELECT INBOX");
	if (response) {
		camel_imap_response_free_without_processing (imap_store, response);
		
		CAMEL_SERVICE_LOCK (imap_store, connect_lock);
		
		if (imap_store->current_folder)
			camel_object_unref (imap_store->current_folder);
		/* no need to actually create a CamelFolder for INBOX */
		imap_store->current_folder = NULL;
		
		CAMEL_SERVICE_UNLOCK (imap_store, connect_lock);
	} else
		return;
	
	imap_store->renaming = TRUE;
	
	if (store->flags & CAMEL_STORE_SUBSCRIPTIONS)
		manage_subscriptions(store, old_name, FALSE);

	new_name = camel_imap_store_summary_path_to_full(imap_store->summary, new_name_in, imap_store->dir_sep);
	response = camel_imap_command (imap_store, NULL, ex, "RENAME %F %S", old_name, new_name);
	
	if (!response) {
		if (store->flags & CAMEL_STORE_SUBSCRIPTIONS)
			manage_subscriptions(store, old_name, TRUE);
		g_free(new_name);
		imap_store->renaming = FALSE;
		return;
	}
	
	camel_imap_response_free (imap_store, response);

	/* rename summary, and handle broken server */
	rename_folder_info(imap_store, old_name, new_name_in);

	if (store->flags & CAMEL_STORE_SUBSCRIPTIONS)
		manage_subscriptions(store, new_name_in, TRUE);

	storage_path = g_strdup_printf("%s/folders", imap_store->storage_path);
	oldpath = e_path_to_physical (storage_path, old_name);
	newpath = e_path_to_physical (storage_path, new_name_in);
	g_free(storage_path);

	/* So do we care if this didn't work?  Its just a cache? */
	if (rename (oldpath, newpath) == -1) {
		g_warning ("Could not rename message cache '%s' to '%s': %s: cache reset",
			   oldpath, newpath, strerror (errno));
	}
	
	g_free (oldpath);
	g_free (newpath);
	g_free(new_name);

	imap_store->renaming = FALSE;
}

static CamelFolderInfo *
create_folder (CamelStore *store, const char *parent_name,
	       const char *folder_name, CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	char *full_name, *resp, *thisone, *parent_real, *real_name;
	CamelImapResponse *response;
	CamelException internal_ex;
	CamelFolderInfo *root = NULL;
	gboolean need_convert;
	int i = 0, flags;
	const char *c;
	
	if (!camel_disco_store_check_online (CAMEL_DISCO_STORE (store), ex))
		return NULL;
	if (!parent_name)
		parent_name = "";
	
	c = folder_name;
	while (*c && *c != imap_store->dir_sep && !strchr ("#%*", *c))
		c++;
	
	if (*c != '\0') {
		camel_exception_setv (ex, CAMEL_EXCEPTION_FOLDER_INVALID_PATH,
				      _("The folder name \"%s\" is invalid because it contains the character \"%c\""),
				      folder_name, *c);
		return NULL;
	}
	
	/* check if the parent allows inferiors */

	/* FIXME: use storesummary directly */
	parent_real = camel_imap_store_summary_full_from_path(imap_store->summary, parent_name);
	if (parent_real == NULL) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_FOLDER_INVALID_STATE,
				     _("Unknown parent folder: %s"), parent_name);
		return NULL;
	}

	need_convert = FALSE;
	response = camel_imap_command (imap_store, NULL, ex, "LIST \"\" %S",
				       parent_real);
	if (!response) /* whoa, this is bad */ {
		g_free(parent_real);
		return NULL;
	}
	
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
		struct imap_status_item *items, *item;
		guint32 messages = 0;
		char *name;
		
		item = items = get_folder_status (imap_store, parent_name, "MESSAGES");
		while (item != NULL) {
			if (!g_ascii_strcasecmp (item->name, "MESSAGES")) {
				messages = item->value;
				break;
			}
			
			item = item->next;
		}
		
		imap_status_item_free (items);
		
		if (messages > 0) {
			camel_exception_set (ex, CAMEL_EXCEPTION_FOLDER_INVALID_STATE,
					     _("The parent folder is not allowed to contain subfolders"));
			g_free(parent_real);
			return NULL;
		}
		
		/* delete the old parent and recreate it */
		delete_folder (store, parent_name, &internal_ex);
		if (camel_exception_is_set (&internal_ex)) {
			camel_exception_xfer (ex, &internal_ex);
			return NULL;
		}
		
		/* add the dirsep to the end of parent_name */
		name = g_strdup_printf ("%s%c", parent_real, imap_store->dir_sep);
		response = camel_imap_command (imap_store, NULL, ex, "CREATE %S",
					       name);
		g_free (name);
		
		if (!response) {
			g_free(parent_real);
			return NULL;
		} else
			camel_imap_response_free (imap_store, response);

		root = imap_build_folder_info(imap_store, parent_name);
	}
	
	/* ok now we can create the folder */
	real_name = camel_imap_store_summary_path_to_full(imap_store->summary, folder_name, imap_store->dir_sep);
	full_name = imap_concat (imap_store, parent_real, real_name);
	g_free(real_name);
	response = camel_imap_command (imap_store, NULL, ex, "CREATE %S", full_name);
	
	if (response) {
		CamelImapStoreInfo *si;
		CamelFolderInfo *fi;

		camel_imap_response_free (imap_store, response);

		si = camel_imap_store_summary_add_from_full(imap_store->summary, full_name, imap_store->dir_sep);
		camel_store_summary_save((CamelStoreSummary *)imap_store->summary);
		fi = imap_build_folder_info(imap_store, camel_store_info_path(imap_store->summary, si));
		fi->flags |= CAMEL_FOLDER_NOCHILDREN;
		if (root) {
			root->child = fi;
			fi->parent = root;
		} else {
			root = fi;
		}
		camel_object_trigger_event (CAMEL_OBJECT (store), "folder_created", root);
	} else if (root) {
		/* need to re-recreate the folder we just deleted */
		camel_object_trigger_event (CAMEL_OBJECT (store), "folder_created", root);
		camel_folder_info_free(root);
		root = NULL;
	}

	g_free (full_name);
	g_free(parent_real);
	
	return root;
}

static CamelFolderInfo *
parse_list_response_as_folder_info (CamelImapStore *imap_store,
				    const char *response)
{
	CamelFolderInfo *fi;
	int flags;
	char sep, *dir, *path;
	CamelURL *url;
	CamelImapStoreInfo *si;
	guint32 newflags;

	if (!imap_parse_list_response (imap_store, response, &flags, &sep, &dir))
		return NULL;

	/* FIXME: should use imap_build_folder_info, note the differences with param setting tho */

	si = camel_imap_store_summary_add_from_full(imap_store->summary, dir, sep?sep:'/');
	if (si == NULL)
		return NULL;

	newflags = (si->info.flags & CAMEL_STORE_INFO_FOLDER_SUBSCRIBED) | (flags & ~CAMEL_STORE_INFO_FOLDER_SUBSCRIBED);
	if (si->info.flags != newflags) {
		si->info.flags = newflags;
		camel_store_summary_touch((CamelStoreSummary *)imap_store->summary);
	}
	
	fi = g_new0 (CamelFolderInfo, 1);
	fi->name = g_strdup(camel_store_info_name(imap_store->summary, si));
	fi->full_name = g_strdup(camel_store_info_path(imap_store->summary, si));
	if (!g_ascii_strcasecmp(fi->full_name, "inbox"))
		flags |= CAMEL_FOLDER_SYSTEM;
	/* HACK: some servers report noinferiors for all folders (uw-imapd)
	   We just translate this into nochildren, and let the imap layer enforce
	   it.  See create folder */
	if (flags & CAMEL_FOLDER_NOINFERIORS)
		flags = (fi->flags & ~CAMEL_FOLDER_NOINFERIORS) | CAMEL_FOLDER_NOCHILDREN;
	fi->flags = flags;
	
	url = camel_url_new (imap_store->base_url, NULL);
	path = alloca(strlen(fi->full_name)+2);
	sprintf(path, "/%s", fi->full_name);
	camel_url_set_path(url, path);

	if (flags & CAMEL_FOLDER_NOSELECT || fi->name[0] == 0)
		camel_url_set_param (url, "noselect", "yes");
	fi->uri = camel_url_to_string (url, 0);
	camel_url_free (url);

	/* FIXME: redundant */
	if (flags & CAMEL_IMAP_FOLDER_UNMARKED)
		fi->unread = -1;

	return fi;
}

/* this is used when lsub doesn't provide very useful information */
static GPtrArray *
get_subscribed_folders (CamelImapStore *imap_store, const char *top, CamelException *ex)
{
	GPtrArray *names, *folders;
	int i, toplen = strlen (top);
	CamelStoreInfo *si;
	CamelImapResponse *response;
	CamelFolderInfo *fi;
	char *result;
	int haveinbox = FALSE;

	if (camel_debug("imap:folder_info"))
		printf("  get_subscribed folders\n");

	folders = g_ptr_array_new ();
	names = g_ptr_array_new ();
	for (i=0;(si = camel_store_summary_index((CamelStoreSummary *)imap_store->summary, i));i++) {
		if (si->flags & CAMEL_STORE_INFO_FOLDER_SUBSCRIBED) {
			g_ptr_array_add(names, (char *)camel_imap_store_info_full_name(imap_store->summary, si));
			haveinbox = haveinbox || strcasecmp(camel_imap_store_info_full_name(imap_store->summary, si), "INBOX") == 0;
		}
		camel_store_summary_info_free((CamelStoreSummary *)imap_store->summary, si);
	}
	
	if (!haveinbox)
		g_ptr_array_add (names, "INBOX");
	
	for (i = 0; i < names->len; i++) {
		response = camel_imap_command (imap_store, NULL, ex,
					       "LIST \"\" %S",
					       names->pdata[i]);
		if (!response)
			break;
		
		result = camel_imap_response_extract (imap_store, response, "LIST", NULL);
		if (!result) {
			camel_store_summary_remove_path((CamelStoreSummary *)imap_store->summary, names->pdata[i]);
			g_ptr_array_remove_index_fast (names, i);
			i--;
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

static int imap_match_pattern(char dir_sep, const char *pattern, const char *name)
{
	char p, n;

	p = *pattern++;
	n = *name++;
	while (n && p) {
		if (n == p) {
			p = *pattern++;
			n = *name++;
		} else if (p == '%') {
			if (n != dir_sep) {
				n = *name++;
			} else {
				p = *pattern++;
			}
		} else if (p == '*') {
			return TRUE;
		} else
			return FALSE;
	}

	return n == 0 && (p == '%' || p == 0);
}

static void
get_folders_online (CamelImapStore *imap_store, const char *pattern,
		    GPtrArray *folders, gboolean lsub, CamelException *ex)
{
	CamelImapResponse *response;
	CamelFolderInfo *fi;
	char *list;
	int i, count;
	GHashTable *present;
	CamelStoreInfo *si;

	response = camel_imap_command (imap_store, NULL, ex,
				       "%s \"\" %S", lsub ? "LSUB" : "LIST",
				       pattern);
	if (!response)
		return;

	present = g_hash_table_new(g_str_hash, g_str_equal);
	for (i = 0; i < response->untagged->len; i++) {
		list = response->untagged->pdata[i];
		fi = parse_list_response_as_folder_info (imap_store, list);
		if (fi) {
			g_ptr_array_add(folders, fi);
			g_hash_table_insert(present, fi->full_name, fi);
		}
	}
	camel_imap_response_free (imap_store, response);

	/* update our summary to match the server */
	count = camel_store_summary_count((CamelStoreSummary *)imap_store->summary);
	for (i=0;i<count;i++) {
		si = camel_store_summary_index((CamelStoreSummary *)imap_store->summary, i);
		if (si == NULL)
			continue;

		if (imap_match_pattern(imap_store->dir_sep, pattern, camel_imap_store_info_full_name(imap_store->summary, si))) {
			if (g_hash_table_lookup(present, camel_store_info_path(imap_store->summary, si)) != NULL) {
				if (lsub && (si->flags & CAMEL_STORE_INFO_FOLDER_SUBSCRIBED) == 0) {
					si->flags |= CAMEL_STORE_INFO_FOLDER_SUBSCRIBED;
					camel_store_summary_touch((CamelStoreSummary *)imap_store->summary);
				}
			} else {
				if (lsub) {
					if (si->flags & CAMEL_STORE_INFO_FOLDER_SUBSCRIBED) {
						si->flags &= ~CAMEL_STORE_INFO_FOLDER_SUBSCRIBED;
						camel_store_summary_touch((CamelStoreSummary *)imap_store->summary);
					}
				} else {
					camel_store_summary_remove((CamelStoreSummary *)imap_store->summary, si);
					count--;
					i--;
				}
			}
		}
		camel_store_summary_info_free((CamelStoreSummary *)imap_store->summary, si);
	}
	g_hash_table_destroy(present);
}

#if 0
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
		printf("%-40s %-30s %*s\n", fi->path, fi->full_name, depth*2+strlen(fi->url), fi->url);
		if (fi->child)
			dumpfi(fi->child);
		fi = fi->sibling;
	}
}
#endif

static void
fill_fi(CamelStore *store, CamelFolderInfo *fi, guint32 flags)
{
	CamelFolder *folder;

	fi->unread = -1;
	fi->total = -1;
	folder = camel_object_bag_peek(store->folders, fi->full_name);
	if (folder) {
		if ((flags & CAMEL_STORE_FOLDER_INFO_FAST) == 0)
			/* we use connect lock for everything, so this should be safe */
			CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(folder))->refresh_info(folder, NULL);
		fi->unread = camel_folder_get_unread_message_count(folder);
		fi->total = camel_folder_get_message_count(folder);
		camel_object_unref(folder);
	} else {
		char *storage_path, *folder_dir, *path;
		CamelFolderSummary *s;

		/* This is a lot of work for one path! */
		storage_path = g_strdup_printf("%s/folders", ((CamelImapStore *)store)->storage_path);
		folder_dir = e_path_to_physical(storage_path, fi->full_name);
		path = g_strdup_printf("%s/summary", folder_dir);
		s = (CamelFolderSummary *)camel_object_new(camel_imap_summary_get_type());
		camel_folder_summary_set_build_content(s, TRUE);
		camel_folder_summary_set_filename(s, path);
		if (camel_folder_summary_header_load(s) != -1) {
			fi->unread = s->unread_count;
			fi->total = s->saved_count;
		}
		g_free(storage_path);
		g_free(folder_dir);
		g_free(path);
		
		camel_object_unref(s);
	}
}

/* NB: We should have connect_lock at this point */
static void
get_folder_counts(CamelImapStore *imap_store, CamelFolderInfo *fi, CamelException *ex)
{
	GSList *q;
	CamelFolder *folder;

	/* non-recursive breath first search */

	q = g_slist_append(NULL, fi);

	while (q) {
		fi = q->data;
		q = g_slist_remove_link(q, q);

		while (fi) {
			/* ignore noselect folders, and check only inbox if we only check inbox */
			if ((fi->flags & CAMEL_FOLDER_NOSELECT) == 0
			    && ( (imap_store->parameters & IMAP_PARAM_CHECK_ALL)
				 || g_ascii_strcasecmp(fi->full_name, "inbox") == 0) ) {

				/* For the current folder, poke it to check for new	
				 * messages and then report that number, rather than
				 * doing a STATUS command.
				 */
				if (imap_store->current_folder && strcmp(imap_store->current_folder->full_name, fi->full_name) == 0) {
					/* we bypass the folder locking otherwise we can deadlock.  we use the command lock for
					   any operations anyway so this is 'safe'.  See comment above imap_store_refresh_folders() for info */
					CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(imap_store->current_folder))->refresh_info(imap_store->current_folder, ex);
					fi->unread = camel_folder_get_unread_message_count (imap_store->current_folder);
					fi->total = camel_folder_get_message_count(imap_store->current_folder);
				} else {
					struct imap_status_item *items, *item;
					
					fi->unread = -1;
					fi->total = -1;
					
					item = items = get_folder_status (imap_store, fi->full_name, "MESSAGES UNSEEN");
					while (item != NULL) {
						if (!g_ascii_strcasecmp (item->name, "MESSAGES")) {
							fi->total = item->value;
						} else if (!g_ascii_strcasecmp (item->name, "UNSEEN")) {
							fi->unread = item->value;
						}
						
						item = item->next;
					}
					
					imap_status_item_free (items);
					
					/* if we have this folder open, and the unread count has changed, update */
					folder = camel_object_bag_peek(CAMEL_STORE(imap_store)->folders, fi->full_name);
					if (folder) {
						if (fi->unread != camel_folder_get_unread_message_count(folder)) {
							CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(folder))->refresh_info(folder, ex);
							fi->unread = camel_folder_get_unread_message_count(folder);
							fi->total = camel_folder_get_message_count(folder);
						}
						camel_object_unref(folder);
					}
				}
			} else {
				/* since its cheap, get it if they're open/consult summary file */
				fill_fi((CamelStore *)imap_store, fi, 0);
			}

			if (fi->child)
				q = g_slist_append(q, fi->child);
			fi = fi->next;
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

static GSList *
get_folders_add_folders(GSList *p, int recurse, GHashTable *infos, GPtrArray *folders, GPtrArray *folders_out)
{
	CamelFolderInfo *oldfi, *fi;
	int i;

	/* This is a nasty mess, because some servers will return
	   broken results from LIST or LSUB if you use '%'.  e.g. you
	   may get (many) duplicate names, and worse, names may have
	   conflicting flags. */
	for (i=0; i<folders->len; i++) {
		fi = folders->pdata[i];
		oldfi = g_hash_table_lookup(infos, fi->full_name);
		if (oldfi == NULL) {
			d(printf(" new folder '%s'\n", fi->full_name));
			g_hash_table_insert(infos, fi->full_name, fi);
			if (recurse)
				p = g_slist_prepend(p, fi);
			g_ptr_array_add(folders_out, fi);
		} else {
			d(printf(" old folder '%s', old flags %08x  new flags %08x\n", fi->full_name, oldfi->flags, fi->flags));

			/* need to special-case noselect, since it also affects the uri */
			if ((oldfi->flags & CAMEL_FOLDER_NOSELECT) != 0
			    && (fi->flags & CAMEL_FOLDER_NOSELECT) == 0) {
				g_free(oldfi->uri);
				oldfi->uri = fi->uri;
				fi->uri = NULL;
			}

			/* some flags are anded together, some are or'd */

			oldfi->flags = (oldfi->flags & fi->flags & (CAMEL_FOLDER_NOSELECT|CAMEL_FOLDER_NOINFERIORS))
				| ((oldfi->flags | fi->flags) & ~(CAMEL_FOLDER_NOSELECT|CAMEL_FOLDER_NOINFERIORS));

			camel_folder_info_free(fi);
		}
	}

	g_ptr_array_set_size(folders, 0);

	return p;
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

	if (camel_debug("imap:folder_info"))
		printf("  get_folders\n");

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
	if (top[0] == 0) {
		if (imap_store->namespace) {
			top = imap_store->namespace;
			i = strlen(top)-1;
			name = g_malloc(i+2);
			strcpy(name, top);
			while (i>0 && name[i] == imap_store->dir_sep)
				name[i--] = 0;
		} else
			name = g_strdup("");
	} else {
		name = camel_imap_store_summary_full_from_path(imap_store->summary, top);
		if (name == NULL)
			name = camel_imap_store_summary_path_to_full(imap_store->summary, top, imap_store->dir_sep);
	}

	d(printf("\n\nList '%s' %s\n", name, flags&CAMEL_STORE_FOLDER_INFO_RECURSIVE?"RECURSIVE":"NON-RECURSIVE"));

	folders_out = g_ptr_array_new();
	folders = g_ptr_array_new();
	
	/* first get working list of names */
	get_folders_online (imap_store, name[0]?name:"%", folders, flags & CAMEL_STORE_FOLDER_INFO_SUBSCRIBED, ex);
	if (camel_exception_is_set(ex))
		goto fail;
	for (i=0; i<folders->len && !haveinbox; i++) {
		fi = folders->pdata[i];
		haveinbox = (strcasecmp(fi->full_name, "INBOX")) == 0;
	}

	if (!haveinbox && top == imap_store->namespace) {
		get_folders_online (imap_store, "INBOX", folders,
				    flags & CAMEL_STORE_FOLDER_INFO_SUBSCRIBED, ex);
		
		if (camel_exception_is_set (ex))
			goto fail;
	}

	p = get_folders_add_folders(p, TRUE, infos, folders, folders_out);

	/* p is a reversed list of pending folders for the next level, q is the list of folders for this */
	while (p) {
		GSList *q = g_slist_reverse(p);

		p = NULL;
		while (q) {
			fi = q->data;

			q = g_slist_remove_link(q, q);

			d(printf("Checking parent folder '%s'\n", fi->full_name));

			/* First if we're not recursive mode on the top level, and we know it has or doesn't
                            or can't have children, no need to go further - a bit ugly */
			if ( top == imap_store->namespace
			     && (flags & CAMEL_STORE_FOLDER_INFO_RECURSIVE) == 0
			     && (fi->flags & (CAMEL_FOLDER_CHILDREN|CAMEL_FOLDER_NOCHILDREN|CAMEL_FOLDER_NOINFERIORS)) != 0) {
				/* do nothing */
				d(printf(" not interested in folder right now ...\n"));
			}
				/* Otherwise, if this has (or might have) children, scan it */
			else if ( (fi->flags & (CAMEL_FOLDER_NOCHILDREN|CAMEL_FOLDER_NOINFERIORS)) == 0
				  || (fi->flags & CAMEL_FOLDER_CHILDREN) != 0) {
				char *n, *real;

				real = camel_imap_store_summary_full_from_path(imap_store->summary, fi->full_name);
				n = imap_concat(imap_store, real?real:fi->full_name, "%");
				get_folders_online(imap_store, n, folders, flags & CAMEL_STORE_FOLDER_INFO_SUBSCRIBED, ex);
				g_free(n);
				g_free(real);

				if (folders->len > 0)
					fi->flags |= CAMEL_FOLDER_CHILDREN;

				p = get_folders_add_folders(p, (flags & CAMEL_STORE_FOLDER_INFO_RECURSIVE) && depth<imap_max_depth,
							    infos, folders, folders_out);
			}
		}
		depth++;
	}

	g_ptr_array_free(folders, TRUE);
	g_hash_table_destroy(infos);
	g_free(name);

	return folders_out;
fail:
	g_ptr_array_free(folders, TRUE);
	g_ptr_array_free(folders_out, TRUE);
	g_hash_table_destroy(infos);
	g_free(name);

	return NULL;
}

static CamelFolderInfo *
get_folder_info_online (CamelStore *store, const char *top, guint32 flags, CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	CamelFolderInfo *tree = NULL;
	GPtrArray *folders;
	
	if (top == NULL)
		top = "";

	if (camel_debug("imap:folder_info"))
		printf("get folder info online\n");

	CAMEL_SERVICE_LOCK(store, connect_lock);

	if ((flags & CAMEL_STORE_FOLDER_INFO_SUBSCRIBED)
	    && !(imap_store->capabilities & IMAP_CAPABILITY_useful_lsub)
	    && (imap_store->parameters & IMAP_PARAM_CHECK_ALL))
		folders = get_subscribed_folders(imap_store, top, ex);
	else
		folders = get_folders(store, top, flags, ex);

	if (folders == NULL)
		goto done;

	tree = camel_folder_info_build(folders, top, '/', TRUE);
	g_ptr_array_free(folders, TRUE);

	if (!(flags & CAMEL_STORE_FOLDER_INFO_FAST))
		get_folder_counts(imap_store, tree, ex);

	d(dumpfi(tree));
	camel_store_summary_save((CamelStoreSummary *)imap_store->summary);
done:
	CAMEL_SERVICE_UNLOCK(store, connect_lock);

	return tree;
}

static gboolean
get_one_folder_offline (const char *physical_path, const char *path, gpointer data)
{
	GPtrArray *folders = data;
	CamelImapStore *imap_store = folders->pdata[0];
	CamelFolderInfo *fi;
	CamelStoreInfo *si;

	if (*path != '/')
		return TRUE;

	/* folder_info_build will insert parent nodes as necessary and mark
	 * them as noselect, which is information we actually don't have at
	 * the moment. So let it do the right thing by bailing out if it's
	 * not a folder we're explicitly interested in.
	 */

	si = camel_store_summary_path((CamelStoreSummary *)imap_store->summary, path+1);
	if (si) {
		if ((((CamelStore *)imap_store)->flags & CAMEL_STORE_SUBSCRIPTIONS) == 0
		    || (si->flags & CAMEL_STORE_INFO_FOLDER_SUBSCRIBED)) {
			fi = imap_build_folder_info(imap_store, path+1);
			fi->flags = si->flags;
			/* HACK: some servers report noinferiors for all folders (uw-imapd)
			   We just translate this into nochildren, and let the imap layer enforce
			   it.  See create folder */
			if (fi->flags & CAMEL_FOLDER_NOINFERIORS)
				fi->flags = (fi->flags & ~CAMEL_FOLDER_NOINFERIORS) | CAMEL_FOLDER_NOCHILDREN;

			if (si->flags & CAMEL_FOLDER_NOSELECT) {
				CamelURL *url = camel_url_new(fi->uri, NULL);
				
				camel_url_set_param (url, "noselect", "yes");
				g_free(fi->uri);
				fi->uri = camel_url_to_string (url, 0);
				camel_url_free (url);
			} else {
				fill_fi((CamelStore *)imap_store, fi, 0);
			}
			g_ptr_array_add (folders, fi);
		}
		camel_store_summary_info_free((CamelStoreSummary *)imap_store->summary, si);
	}

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

	if (camel_debug("imap:folder_info"))
		printf("get folder info offline\n");

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
		fi = camel_folder_info_build (folders, "", '/', TRUE);
	}
	g_free(storage_path);

	g_ptr_array_free (folders, TRUE);
	return fi;
}

static gboolean
folder_subscribed (CamelStore *store, const char *folder_name)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	CamelStoreInfo *si;
	int truth = FALSE;

	si = camel_store_summary_path((CamelStoreSummary *)imap_store->summary, folder_name);
	if (si) {
		truth = (si->flags & CAMEL_STORE_INFO_FOLDER_SUBSCRIBED) != 0;
		camel_store_summary_info_free((CamelStoreSummary *)imap_store->summary, si);
	}

	return truth;
}

/* Note: folder_name must match a folder as listed with get_folder_info() -> full_name */
static void
subscribe_folder (CamelStore *store, const char *folder_name,
		  CamelException *ex)
{
	CamelImapStore *imap_store = CAMEL_IMAP_STORE (store);
	CamelImapResponse *response;
	CamelFolderInfo *fi;
	CamelStoreInfo *si;

	if (!camel_disco_store_check_online (CAMEL_DISCO_STORE (store), ex))
		return;
	if (!camel_imap_store_connected (imap_store, ex))
		return;
	
	response = camel_imap_command (imap_store, NULL, ex,
				       "SUBSCRIBE %F", folder_name);
	if (!response)
		return;
	camel_imap_response_free (imap_store, response);
	
	si = camel_store_summary_path((CamelStoreSummary *)imap_store->summary, folder_name);
	if (si) {
		if ((si->flags & CAMEL_STORE_INFO_FOLDER_SUBSCRIBED) == 0) {
			si->flags |= CAMEL_STORE_INFO_FOLDER_SUBSCRIBED;
			camel_store_summary_touch((CamelStoreSummary *)imap_store->summary);
			camel_store_summary_save((CamelStoreSummary *)imap_store->summary);
		}
		camel_store_summary_info_free((CamelStoreSummary *)imap_store->summary, si);
	}
	
	if (imap_store->renaming) {
		/* we don't need to emit a "folder_subscribed" signal
                   if we are in the process of renaming folders, so we
                   are done here... */
		return;
	}

	fi = imap_build_folder_info(imap_store, folder_name);
	fi->flags |= CAMEL_FOLDER_NOCHILDREN;
	
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
	
	if (!camel_imap_store_connected (store, ex))
		return -1;
	
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
