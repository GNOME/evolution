/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-imap-store.c : class for a imap store */

/* 
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 2000-2002 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "camel/camel-operation.h"

#include "camel/camel-stream-buffer.h"
#include "camel/camel-session.h"
#include "camel/camel-exception.h"
#include "camel/camel-url.h"
#include "camel/camel-sasl.h"
#include "camel/camel-data-cache.h"
#include "camel/camel-tcp-stream.h"
#include "camel/camel-tcp-stream-raw.h"
#ifdef HAVE_SSL
#include "camel/camel-tcp-stream-ssl.h"
#endif
#include "camel/camel-i18n.h"

#include "camel-imapp-store-summary.h"
#include "camel-imapp-store.h"
#include "camel-imapp-folder.h"
#include "camel-imapp-engine.h"
#include "camel-imapp-exception.h"
#include "camel-imapp-utils.h"
#include "camel-imapp-driver.h"

/* Specified in RFC 2060 section 2.1 */
#define IMAP_PORT 143

static CamelStoreClass *parent_class = NULL;

static void finalize (CamelObject *object);

static void imap_construct(CamelService *service, CamelSession *session, CamelProvider *provider, CamelURL *url, CamelException *ex);
/* static char *imap_get_name(CamelService *service, gboolean brief);*/
static gboolean imap_connect (CamelService *service, CamelException *ex);
static gboolean imap_disconnect (CamelService *service, gboolean clean, CamelException *ex);
static GList *imap_query_auth_types (CamelService *service, CamelException *ex);

static CamelFolder *imap_get_trash  (CamelStore *store, CamelException *ex);

static CamelFolder *imap_get_folder(CamelStore * store, const char *folder_name, guint32 flags, CamelException * ex);
static CamelFolder *imap_get_inbox (CamelStore *store, CamelException *ex);
static void imap_rename_folder(CamelStore *store, const char *old_name, const char *new_name, CamelException *ex);
static CamelFolderInfo *imap_get_folder_info (CamelStore *store, const char *top, guint32 flags, CamelException *ex);
static void imap_delete_folder(CamelStore *store, const char *folder_name, CamelException *ex);
static void imap_rename_folder(CamelStore *store, const char *old, const char *new, CamelException *ex);
static CamelFolderInfo *imap_create_folder(CamelStore *store, const char *parent_name, const char *folder_name, CamelException *ex);

/* yet to see if this should go global or not */
void camel_imapp_store_folder_selected(CamelIMAPPStore *store, CamelIMAPPFolder *folder, CamelIMAPPSelectResponse *select);

static void
camel_imapp_store_class_init (CamelIMAPPStoreClass *camel_imapp_store_class)
{
	CamelServiceClass *camel_service_class = CAMEL_SERVICE_CLASS(camel_imapp_store_class);
	CamelStoreClass *camel_store_class = CAMEL_STORE_CLASS(camel_imapp_store_class);

	parent_class = CAMEL_STORE_CLASS(camel_type_get_global_classfuncs(camel_store_get_type()));
	
	/* virtual method overload */
	camel_service_class->construct = imap_construct;
	/*camel_service_class->get_name = imap_get_name;*/
	camel_service_class->query_auth_types = imap_query_auth_types;
	camel_service_class->connect = imap_connect;
	camel_service_class->disconnect = imap_disconnect;

	camel_store_class->get_trash = imap_get_trash;
	camel_store_class->get_folder = imap_get_folder;
	camel_store_class->get_inbox = imap_get_inbox;

	camel_store_class->create_folder = imap_create_folder;
	camel_store_class->rename_folder = imap_rename_folder;
	camel_store_class->delete_folder = imap_delete_folder;
	camel_store_class->get_folder_info = imap_get_folder_info;
}

static void
camel_imapp_store_init (gpointer object, gpointer klass)
{
	/*CamelIMAPPStore *istore = object;*/
}

CamelType
camel_imapp_store_get_type (void)
{
	static CamelType camel_imapp_store_type = CAMEL_INVALID_TYPE;

	if (!camel_imapp_store_type) {
		camel_imapp_store_type = camel_type_register(CAMEL_STORE_TYPE,
							    "CamelIMAPPStore",
							    sizeof (CamelIMAPPStore),
							    sizeof (CamelIMAPPStoreClass),
							    (CamelObjectClassInitFunc) camel_imapp_store_class_init,
							    NULL,
							    (CamelObjectInitFunc) camel_imapp_store_init,
							    finalize);
	}

	return camel_imapp_store_type;
}

static void
finalize (CamelObject *object)
{
	CamelIMAPPStore *imap_store = CAMEL_IMAPP_STORE (object);

	/* force disconnect so we dont have it run later, after we've cleaned up some stuff */
	/* SIGH */

	camel_service_disconnect((CamelService *)imap_store, TRUE, NULL);

	if (imap_store->driver)
		camel_object_unref(imap_store->driver);
	if (imap_store->cache)
		camel_object_unref(imap_store->cache);
}

static void imap_construct(CamelService *service, CamelSession *session, CamelProvider *provider, CamelURL *url, CamelException *ex)
{
	char *root, *summary;
	CamelIMAPPStore *store = (CamelIMAPPStore *)service;

	CAMEL_SERVICE_CLASS (parent_class)->construct (service, session, provider, url, ex);
	if (camel_exception_is_set(ex))
		return;
	
	CAMEL_TRY {
		store->summary = camel_imapp_store_summary_new();
		root = camel_session_get_storage_path(service->session, service, ex);
		if (root) {
			summary = g_build_filename(root, ".ev-store-summary", NULL);
			camel_store_summary_set_filename((CamelStoreSummary *)store->summary, summary);
			/* FIXME: need to remove params, passwords, etc */
			camel_store_summary_set_uri_base((CamelStoreSummary *)store->summary, service->url);
			camel_store_summary_load((CamelStoreSummary *)store->summary);
		}
	} CAMEL_CATCH(e) {
		camel_exception_xfer(ex, e);
	} CAMEL_DONE;
}

enum {
	USE_SSL_NEVER,
	USE_SSL_ALWAYS,
	USE_SSL_WHEN_POSSIBLE
};

#define SSL_PORT_FLAGS (CAMEL_TCP_STREAM_SSL_ENABLE_SSL2 | CAMEL_TCP_STREAM_SSL_ENABLE_SSL3)
#define STARTTLS_FLAGS (CAMEL_TCP_STREAM_SSL_ENABLE_TLS)

static void
connect_to_server (CamelService *service, int ssl_mode, int try_starttls)
/* throws IO exception */
{
	CamelIMAPPStore *store = CAMEL_IMAPP_STORE (service);
	CamelStream * volatile tcp_stream = NULL;
	CamelIMAPPStream * volatile imap_stream = NULL;
	int ret;
	CamelException *ex;

	ex = camel_exception_new();
	CAMEL_TRY {
		char *serv;
		const char *port = NULL;
		struct addrinfo *ai, hints = { 0 };

		/* parent class connect initialization */
		CAMEL_SERVICE_CLASS (parent_class)->connect (service, ex);
		if (ex->id)
			camel_exception_throw_ex(ex);

		if (service->url->port) {
			serv = g_alloca(16);
			sprintf(serv, "%d", service->url->port);
		} else {
			serv = "imap";
			port = "143";
		}

#ifdef HAVE_SSL	
		if (camel_url_get_param (service->url, "use_ssl")) {
			if (try_starttls)
				tcp_stream = camel_tcp_stream_ssl_new_raw (service->session, service->url->host, STARTTLS_FLAGS);
			else {
				if (service->url->port == 0) {
					serv = "imaps";
					port = "993";
				}
				tcp_stream = camel_tcp_stream_ssl_new (service->session, service->url->host, SSL_PORT_FLAGS);
			}
		} else {
			tcp_stream = camel_tcp_stream_raw_new ();
		}
#else	
		tcp_stream = camel_tcp_stream_raw_new ();
#endif /* HAVE_SSL */

		hints.ai_socktype = SOCK_STREAM;
		ai = camel_getaddrinfo(service->url->host, serv, &hints, ex);
		if (ex->id && ex->id != CAMEL_EXCEPTION_USER_CANCEL && port != NULL) {
			camel_exception_clear(ex);
			ai = camel_getaddrinfo(service->url->host, port, &hints, ex);
		}

		if (ex->id)
			camel_exception_throw_ex(ex);
	
		ret = camel_tcp_stream_connect(CAMEL_TCP_STREAM(tcp_stream), ai);
		camel_freeaddrinfo(ai);
		if (ret == -1) {
			if (errno == EINTR)
				camel_exception_throw(CAMEL_EXCEPTION_USER_CANCEL, _("Connection cancelled"));
			else
				camel_exception_throw(CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
						      _("Could not connect to %s (port %s): %s"),
						      service->url->host, serv, strerror(errno));
		}

		imap_stream = (CamelIMAPPStream *)camel_imapp_stream_new(tcp_stream);
		store->driver = camel_imapp_driver_new(imap_stream);

		camel_object_unref(imap_stream);
		camel_object_unref(tcp_stream);
	} CAMEL_CATCH(e) {
		if (tcp_stream)
			camel_object_unref(tcp_stream);
		if (imap_stream)
			camel_object_unref((CamelObject *)imap_stream);
		camel_exception_throw_ex(e);
	} CAMEL_DONE;

	camel_exception_free(ex);
}

#if 0

/* leave this stuff out for now */


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
#endif

extern CamelServiceAuthType camel_imapp_password_authtype;
extern CamelServiceAuthType camel_imapp_apop_authtype;

static GList *
imap_query_auth_types (CamelService *service, CamelException *ex)
{
	/*CamelIMAPPStore *store = CAMEL_IMAPP_STORE (service);*/
	GList *types = NULL;

        types = CAMEL_SERVICE_CLASS (parent_class)->query_auth_types (service, ex);
	if (types == NULL)
		return NULL;

#if 0
	if (connect_to_server_wrapper (service, NULL)) {
		types = g_list_concat(types, g_list_copy(store->engine->auth));
		imap_disconnect (service, TRUE, NULL);
	} else {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      _("Could not connect to POP server on %s"),
				      service->url->host);
	}
#endif
	return types;
}

static void
store_get_pass(CamelIMAPPStore *store)
{
	if (((CamelService *)store)->url->passwd == NULL) {
		char *prompt;
		CamelException ex;

		camel_exception_init(&ex);
		
		prompt = g_strdup_printf (_("%sPlease enter the IMAP password for %s@%s"),
					  store->login_error?store->login_error:"",
					  ((CamelService *)store)->url->user,
					  ((CamelService *)store)->url->host);
		((CamelService *)store)->url->passwd = camel_session_get_password(camel_service_get_session((CamelService *)store),
										  (CamelService *)store, NULL,
										  prompt, "password", CAMEL_SESSION_PASSWORD_SECRET, &ex);
		g_free (prompt);
		if (camel_exception_is_set(&ex))
			camel_exception_throw_ex(&ex);
	}
}

static struct _CamelSasl *
store_get_sasl(struct _CamelIMAPPDriver *driver, CamelIMAPPStore *store)
{
	store_get_pass(store);

	if (((CamelService *)store)->url->authmech)
		return camel_sasl_new("imap", ((CamelService *)store)->url->authmech, (CamelService *)store);

	return NULL;
}

static void
store_get_login(struct _CamelIMAPPDriver *driver, char **login, char **pass, CamelIMAPPStore *store)
{
	store_get_pass(store);
	
	*login = g_strdup(((CamelService *)store)->url->user);
	*pass = g_strdup(((CamelService *)store)->url->passwd);
}

static gboolean
imap_connect (CamelService *service, CamelException *ex)
{
	CamelIMAPPStore *store = (CamelIMAPPStore *)service;
	volatile int ret = FALSE;

	CAMEL_TRY {
		volatile int retry = TRUE;

		if (store->cache == NULL) {
			char *root;
			
			root = camel_session_get_storage_path(service->session, service, ex);
			if (root) {
				store->cache = camel_data_cache_new(root, 0, ex);
				g_free(root);
				if (store->cache) {
					/* Default cache expiry - 1 week or not visited in a day */
					camel_data_cache_set_expire_age(store->cache, 60*60*24*7);
					camel_data_cache_set_expire_access(store->cache, 60*60*24);
				}
			}
			if (camel_exception_is_set(ex))
				camel_exception_throw_ex(ex);
		}

		connect_to_server(service, USE_SSL_NEVER, FALSE);

		camel_imapp_driver_set_sasl_factory(store->driver, (CamelIMAPPSASLFunc)store_get_sasl, store);
		camel_imapp_driver_set_login_query(store->driver, (CamelIMAPPLoginFunc)store_get_login, store);
		store->login_error = NULL;

		do {
			CAMEL_TRY {
				if (store->driver->engine->state != IMAP_ENGINE_AUTH)
					camel_imapp_driver_login(store->driver);
				ret = TRUE;
				retry = FALSE;
			} CAMEL_CATCH(e) {
				g_free(store->login_error);
				store->login_error = NULL;
				switch (e->id) {
				case CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE:
					store->login_error = g_strdup_printf("%s\n\n", e->desc);
					camel_session_forget_password(service->session, service, NULL, "password", ex);
					camel_url_set_passwd(service->url, NULL);
					break;
				default:
					camel_exception_throw_ex(e);
					break;
				}
			} CAMEL_DONE;
		} while (retry);
	} CAMEL_CATCH(e) {
		camel_exception_xfer(ex, e);
		camel_service_disconnect(service, TRUE, NULL);
		ret = FALSE;
	} CAMEL_DONE;

	g_free(store->login_error);
	store->login_error = NULL;

	return ret;
}

static gboolean
imap_disconnect (CamelService *service, gboolean clean, CamelException *ex)
{
	CamelIMAPPStore *store = CAMEL_IMAPP_STORE (service);

	/* FIXME: logout */
	
	if (!CAMEL_SERVICE_CLASS (parent_class)->disconnect (service, clean, ex))
		return FALSE;

	/* logout/disconnect */
	if (store->driver) {
		camel_object_unref(store->driver);
		store->driver = NULL;
	}
	
	return TRUE;
}

static CamelFolder *
imap_get_trash (CamelStore *store, CamelException *ex)
{
	/* no-op */
	return NULL;
}

static CamelFolder *
imap_get_folder (CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex)
{
	CamelIMAPPStore *istore = (CamelIMAPPStore *)store;
	CamelIMAPPFolder * volatile folder = NULL;

	/* ??? */

	/* 1. create the folder */
	/* 2. run select? */
	/* 3. update the folder */

	CAMEL_TRY {
		folder = (CamelIMAPPFolder *)camel_imapp_folder_new(store, folder_name);
		camel_imapp_driver_select(istore->driver, folder);
	} CAMEL_CATCH (e) {
		if (folder) {
			camel_object_unref(folder);
			folder = NULL;
		}
		camel_exception_xfer(ex, e);
	} CAMEL_DONE;

	return (CamelFolder *)folder;
}

static CamelFolder *
imap_get_inbox(CamelStore *store, CamelException *ex)
{
	camel_exception_setv(ex, 1, "get_inbox::unimplemented");

	return NULL;
}

/* 8 bit, string compare */
static int folders_build_cmp(const void *app, const void *bpp)
{
	struct _list_info *a = *((struct _list_info **)app);
	struct _list_info *b = *((struct _list_info **)bpp);
	unsigned char *ap = (unsigned char *)(a->name);
	unsigned char *bp = (unsigned char *)(b->name);

	printf("qsort, cmp '%s' <> '%s'\n", ap, bp);

	while (*ap && *ap == *bp) {
		ap++;
		bp++;
	}

	if (*ap < *bp)
		return -1;
	else if (*ap > *bp)
		return 1;
	return 0;
}

/* FIXME: this should go via storesummary? */
static CamelFolderInfo *
folders_build_info(CamelURL *base, struct _list_info *li)
{
	char *path, *full_name, *name;
	CamelFolderInfo *fi;

	full_name = imapp_list_get_path(li);
	name = strrchr(full_name, '/');
	if (name)
		name++;
	else
		name = full_name;

	path = alloca(strlen(full_name)+2);
	sprintf(path, "/%s", full_name);
	camel_url_set_path(base, path);

	fi = g_malloc0(sizeof(*fi));
	fi->uri = camel_url_to_string(base, CAMEL_URL_HIDE_ALL);
	fi->name = g_strdup(name);
	fi->full_name = full_name;
	fi->unread = -1;
	fi->total = -1;
	fi->flags = li->flags;

	if (!g_ascii_strcasecmp(fi->full_name, "inbox"))
		fi->flags |= CAMEL_FOLDER_SYSTEM;

	/* TODO: could look up count here ... */
	/* ?? */
	/*folder = camel_object_bag_get(store->folders, "INBOX");*/

	return fi;
}

/*
  a
  a/b
  a/b/c
  a/d
  b
  c/d

*/

/* note, pname is the raw name, not the folderinfo name */
/* note also this free's as we go, since we never go 'backwards' */
static CamelFolderInfo *
folders_build_rec(CamelURL *base, GPtrArray *folders, int *ip, CamelFolderInfo *pfi, char *pname)
{
	int plen = 0;
	CamelFolderInfo *last = NULL, *first = NULL;

	if (pfi)
		plen = strlen(pname);

	for(;(*ip)<(int)folders->len;) {
		CamelFolderInfo *fi;
		struct _list_info *li;

		li = folders->pdata[*ip];
		printf("checking '%s' is child of '%s'\n", li->name, pname);

		/* is this a child of the parent? */
		if (pfi != NULL
		    && (strncmp(pname, li->name, strlen(pname)) != 0
			|| li->name[plen] != li->separator)) {
			printf("  nope\n");
			break;
		}
		printf("  yep\n");

		/* is this not an immediate child of the parent? */
#if 0
		char *p;
		if (pfi != NULL
		    && li->separator != 0
		    && (p = strchr(li->name + plen + 1, li->separator)) != NULL) {
			if (last == NULL) {
				struct _list_info tli;

				tli.flags = CAMEL_FOLDER_NOSELECT|CAMEL_FOLDER_CHILDREN;
				tli.separator = li->separator;
				tli.name = g_strndup(li->name, p-li->name+1);
				fi = folders_build_info(base, &tli);
				fi->parent = pfi;
				if (pfi && pfi->child == NULL)
					pfi->child = fi;
				i = folders_build_rec(folders, i, fi, tli.name);
				break;
			}
		}
#endif

		fi = folders_build_info(base, li);
		fi->parent = pfi;
		if (last != NULL)
			last->next = fi;
		last = fi;
		if (first == NULL)
			first = fi;

		(*ip)++;
		fi->child = folders_build_rec(base, folders, ip, fi, li->name);
		imap_free_list(li);
	}

	return first;
}

static void
folder_info_dump(CamelFolderInfo *fi, int depth)
{
	char *s;

	s = alloca(depth+1);
	memset(s, ' ', depth);
	s[depth] = 0;
	while (fi) {
		printf("%s%s (%s)\n", s, fi->name, fi->uri);
		if (fi->child)
			folder_info_dump(fi->child, depth+2);
		fi = fi->next;
	}
	
}

static CamelFolderInfo *
imap_get_folder_info(CamelStore *store, const char *top, guint32 flags, CamelException *ex)
{
	CamelIMAPPStore *istore = (CamelIMAPPStore *)store;
	CamelFolderInfo * fi= NULL;
	char *name;

	/* FIXME: temporary, since this is not a disco store */
	if (istore->driver == NULL
	    && !camel_service_connect((CamelService *)store, ex))
		return NULL;

	name = (char *)top;
	if (name == NULL || name[0] == 0) {
		/* namespace? */
		name = "";
	}

	name = "";

	CAMEL_TRY {
		CamelURL *base;
		int i;
		GPtrArray *folders;

		/* FIXME: subscriptions? lsub? */
		folders = camel_imapp_driver_list(istore->driver, name, flags);

		/* this greatly simplifies the tree algorithm ... but it might
		   be faster just to use a hashtable to find parents? */
		qsort(folders->pdata, folders->len, sizeof(folders->pdata[0]), folders_build_cmp);

		i = 0;
		base = camel_url_copy(((CamelService *)store)->url);
		fi = folders_build_rec(base, folders, &i, NULL, NULL);
		camel_url_free(base);
		g_ptr_array_free(folders, TRUE);
	} CAMEL_CATCH(e) {
		camel_exception_xfer(ex, e);
	} CAMEL_DONE;

	printf("built folder info:\n");
	folder_info_dump(fi, 2);

	return fi;

#if 0
	if (top == NULL || !g_ascii_strcasecmp(top, "inbox")) {
		CamelURL *uri = camel_url_copy(((CamelService *)store)->url);

		camel_url_set_path(uri, "/INBOX");
		fi = g_malloc0(sizeof(*fi));
		fi->url = camel_url_to_string(uri, CAMEL_URL_HIDE_ALL);
		camel_url_free(uri);
		fi->name = g_strdup("INBOX");
		fi->full_name = g_strdup("INBOX");
		fi->path = g_strdup("/INBOX");
		fi->unread_message_count = -1;
		fi->flags = 0;

		folder = camel_object_bag_get(store->folders, "INBOX");
		if (folder) {
			/*if (!cflags & FAST)*/
			camel_imapp_driver_update(istore->driver, (CamelIMAPPFolder *)folder);
			fi->unread_message_count = camel_folder_get_unread_message_count(folder);
			camel_object_unref(folder);
		}
	} else {
		camel_exception_setv(ex, 1, "not implemented");
	}
#endif
	return fi;

#if 0
	istore->pending_list = g_ptr_array_new();

	CAMEL_TRY {
		ic = camel_imapp_engine_command_new(istore->driver->engine, "LIST", NULL, "LIST \"\" %f", top);
		camel_imapp_engine_command_queue(istore->driver->engine, ic);
		while (camel_imapp_engine_iterate(istore->driver->engine, ic) > 0)
			;

		if (ic->status->result != IMAP_OK)
			camel_exception_throw(1, "list failed: %s", ic->status->text);
	} CAMEL_CATCH (e) {
		camel_exception_xfer(ex, e);
	} CAMEL_DONE;
	
	camel_imapp_engine_command_free(istore->driver->engine, ic);

	printf("got folder list:\n");
	for (i=0;i<(int)istore->pending_list->len;i++) {
		struct _list_info *linfo = istore->pending_list->pdata[i];

		printf("%s (%c)\n", linfo->name, linfo->separator);
		imap_free_list(linfo);
	}
	istore->pending_list = NULL;

	return NULL;
#endif
}

static void
imap_delete_folder(CamelStore *store, const char *folder_name, CamelException *ex)
{
	camel_exception_setv(ex, 1, "delete_folder::unimplemented");
}

static void
imap_rename_folder(CamelStore *store, const char *old, const char *new, CamelException *ex)
{
	camel_exception_setv(ex, 1, "rename_folder::unimplemented");
}

static CamelFolderInfo *
imap_create_folder(CamelStore *store, const char *parent_name, const char *folder_name, CamelException *ex)
{
	camel_exception_setv(ex, 1, "create_folder::unimplemented");
	return NULL;
}

/* ********************************************************************** */
#if 0
static int store_resp_fetch(CamelIMAPPEngine *ie, guint32 id, void *data)
{
	struct _fetch_info *finfo;
	CamelIMAPPStore *istore = data;
	CamelMessageInfo *info;
	struct _pending_fetch *pending;

	finfo = imap_parse_fetch(ie->stream);
	if (istore->selected) {
		if ((finfo->got & FETCH_UID) == 0) {
			printf("didn't get uid in fetch response?\n");
		} else {
			info = camel_folder_summary_index(((CamelFolder *)istore->selected)->summary, id-1);
			/* exists, check/update */
			if (info) {
				if (strcmp(finfo->uid, camel_message_info_uid(info)) != 0) {
					printf("summary at index %d has uid %s expected %s\n", id, camel_message_info_uid(info), finfo->uid);
					/* uid mismatch???  try do it based on uid instead? try to reorder?  i dont know? */
					camel_message_info_free(info);
					info = camel_folder_summary_uid(((CamelFolder *)istore->selected)->summary, finfo->uid);
				}
			}

			if (info) {
				if (finfo->got & (FETCH_FLAGS)) {
					printf("updating flags for uid '%s'\n", finfo->uid);
					info->flags = finfo->flags;
					camel_folder_change_info_change_uid(istore->selected->changes, finfo->uid);
				}
				if (finfo->got & FETCH_MINFO) {
					printf("got envelope unexpectedly?\n");
				}
				/* other things go here, like body fetches */
			} else {
				pending = g_hash_table_lookup(istore->pending_fetch_table, finfo->uid);

				/* we need to create a new info, we only care about flags and minfo */

				if (pending)
					info = pending->info;
				else {
					info = camel_folder_summary_info_new(((CamelFolder *)istore->selected)->summary);
					camel_message_info_set_uid(info, g_strdup(finfo->uid));
				}

				if (finfo->got & FETCH_FLAGS)
					info->flags = finfo->flags;

				if (finfo->got & FETCH_MINFO) {
					/* if we only use ENVELOPE? */
					camel_message_info_set_subject(info, g_strdup(camel_message_info_subject(finfo->minfo)));
					camel_message_info_set_from(info, g_strdup(camel_message_info_from(finfo->minfo)));
					camel_message_info_set_to(info, g_strdup(camel_message_info_to(finfo->minfo)));
					camel_message_info_set_cc(info, g_strdup(camel_message_info_cc(finfo->minfo)));
					info->date_sent = finfo->minfo->date_sent;
					camel_folder_summary_add(((CamelFolder *)istore->selected)->summary, info);
					camel_folder_change_info_add_uid(istore->selected->changes, finfo->uid);
					if (pending) {
						e_dlist_remove((EDListNode *)pending);
						g_hash_table_remove(istore->pending_fetch_table, finfo->uid);
						/*e_memchunk_free(istore->pending_fetch_chunks, pending);*/
					}
				} else if (finfo->got & FETCH_HEADER) {
					/* if we only use HEADER? */
					CamelMimeParser *mp;

					if (pending == NULL)
						camel_message_info_free(info);
					mp = camel_mime_parser_new();
					camel_mime_parser_init_with_stream(mp, finfo->header);
					info = camel_folder_summary_info_new_from_parser(((CamelFolder *)istore->selected)->summary, mp);
					camel_object_unref(mp);
					camel_message_info_set_uid(info, g_strdup(finfo->uid));

					camel_folder_summary_add(((CamelFolder *)istore->selected)->summary, info);
					camel_folder_change_info_add_uid(istore->selected->changes, finfo->uid);
					if (pending) {
						/* FIXME: use a dlist */
						e_dlist_remove((EDListNode *)pending);
						g_hash_table_remove(istore->pending_fetch_table, camel_message_info_uid(pending->info));
						camel_message_info_free(pending->info);
						/*e_memchunk_free(istore->pending_fetch_chunks, pending);*/
					}
				} else if (finfo->got & FETCH_FLAGS) {
					if (pending == NULL) {
						pending = e_memchunk_alloc(istore->pending_fetch_chunks);
						pending->info = info;
						g_hash_table_insert(istore->pending_fetch_table, (char *)camel_message_info_uid(info), pending);
						e_dlist_addtail(&istore->pending_fetch_list, (EDListNode *)pending);
					}
				} else {
					if (pending == NULL)
						camel_message_info_free(info);
					printf("got unexpected fetch response?\n");
					imap_dump_fetch(finfo);
				}
			}
		}
	} else {
		printf("unexpected fetch response, no folder selected?\n");
	}
	/*imap_dump_fetch(finfo);*/
	imap_free_fetch(finfo);

	return camel_imapp_engine_skip(ie);
}
#endif

/* ********************************************************************** */

/* should be moved to imapp-utils?
   stuff in imapp-utils should be moved to imapp-parse? */

/* ********************************************************************** */

#if 0
void
camel_imapp_store_folder_selected(CamelIMAPPStore *store, CamelIMAPPFolder *folder, CamelIMAPPSelectResponse *select)
{
	CamelIMAPPCommand * volatile ic = NULL;
	CamelIMAPPStore *istore = (CamelIMAPPStore *)store;
	int i;
	struct _uidset_state ss;
	GPtrArray *fetch;
	CamelMessageInfo *info;
	struct _pending_fetch *fw, *fn;

	printf("imap folder selected\n");

	if (select->uidvalidity == folder->uidvalidity
	    && select->exists == folder->exists
	    && select->recent == folder->recent
	    && select->unseen == folder->unseen) {
		/* no work to do? */
		return;
	}

	istore->pending_fetch_table = g_hash_table_new(g_str_hash, g_str_equal);
	istore->pending_fetch_chunks = e_memchunk_new(256, sizeof(struct _pending_fetch));

	/* perform an update - flags first (and see what we have) */
	CAMEL_TRY {
		ic = camel_imapp_engine_command_new(istore->engine, "FETCH", NULL, "FETCH 1:%d (UID FLAGS)", select->exists);
		camel_imapp_engine_command_queue(istore->engine, ic);
		while (camel_imapp_engine_iterate(istore->engine, ic) > 0)
			;

		if (ic->status->result != IMAP_OK)
			camel_exception_throw(1, "fetch failed: %s", ic->status->text);

		/* pending_fetch_list now contains any new messages */
		/* FIXME: how do we work out no-longer present messages? */
		printf("now fetching info for messages?\n");
		uidset_init(&ss, store->engine);
		ic = camel_imapp_engine_command_new(istore->engine, "FETCH", NULL, "UID FETCH ");
		fw = (struct _pending_fetch *)istore->pending_fetch_list.head;
		fn = fw->next;
		while (fn) {
			info = fw->info;
			/* if the uid set fills, then flush the command out */
			if (uidset_add(&ss, ic, camel_message_info_uid(info))
			    || (fn->next == NULL && uidset_done(&ss, ic))) {
				camel_imapp_engine_command_add(istore->engine, ic, " (FLAGS RFC822.HEADER)");
				camel_imapp_engine_command_queue(istore->engine, ic);
				while (camel_imapp_engine_iterate(istore->engine, ic) > 0)
					;
				if (ic->status->result != IMAP_OK)
					camel_exception_throw(1, "fetch failed: %s", ic->status->text);
				/* if not end ... */
				camel_imapp_engine_command_free(istore->engine, ic);
				ic = camel_imapp_engine_command_new(istore->engine, "FETCH", NULL, "UID FETCH ");
			}
			fw = fn;
			fn = fn->next;
		}

		printf("The pending list should now be empty: %s\n", e_dlist_empty(&istore->pending_fetch_list)?"TRUE":"FALSE");
		for (i=0;i<10;i++) {
			info = camel_folder_summary_index(((CamelFolder *)istore->selected)->summary, i);
			if (info) {
				printf("message info [%d] =\n", i);
				camel_message_info_dump(info);
				camel_message_info_free(info);
			}
		}
	} CAMEL_CATCH (e) {
		/* FIXME: cleanup */
		camel_exception_throw_ex(e);
	} CAMEL_DONE;

	g_hash_table_destroy(istore->pending_fetch_table);
	istore->pending_fetch_table = NULL;
	e_memchunk_destroy(istore->pending_fetch_chunks);

	camel_imapp_engine_command_free(istore->engine, ic);
}
#endif

#if 0
/*char *uids[] = {"1", "2", "4", "5", "6", "7", "9", "11", "12", 0};*/
/*char *uids[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", 0};*/
char *uids[] = {"1", "3", "5", "7", "9", "11", "12", "13", "14", "15", "20", "21", "24", "25", "26", 0};

void
uidset_test(CamelIMAPPEngine *ie)
{
	struct _uidset_state ss;
	CamelIMAPPCommand *ic;
	int i;

	/*ic = camel_imapp_engine_command_new(ie, 0, "FETCH", NULL, "FETCH ");*/
	uidset_init(&ss, 0, 0);
	for (i=0;uids[i];i++) {
		if (uidset_add(&ss, uids[i])) {
			printf("\n[%d] flushing uids\n", i);
		}
	}

	if (uidset_done(&ss)) {
		printf("\nflushing uids\n");
	}
}
#endif
