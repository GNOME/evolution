/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 *
 * Copyright (C) 2001-2003 Ximian, Inc. <www.ximain.com>
 *
 * Authors: Christopher Toshok <toshok@ximian.com>
 *    	    Michael Zucchi <notzed@ximian.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include <camel/camel-url.h>
#include <camel/string-utils.h>
#include <camel/camel-session.h>
#include <camel/camel-tcp-stream-raw.h>
#include <camel/camel-tcp-stream-ssl.h>

#include "camel-nntp-summary.h"
#include "camel-nntp-store.h"
#include "camel-nntp-folder.h"
#include "camel-nntp-private.h"

#define w(x)
extern int camel_verbose_debug;
#define dd(x) (camel_verbose_debug?(x):0)

#define NNTP_PORT  119
#define NNTPS_PORT 563

#define DUMP_EXTENSIONS

/* define if you want the subscribe ui to show folders in tree form */
/* #define INFO_AS_TREE */

static CamelStoreClass *parent_class = NULL;
static CamelServiceClass *service_class = NULL;

/* Returns the class for a CamelNNTPStore */
#define CNNTPS_CLASS(so) CAMEL_NNTP_STORE_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CNNTPF_CLASS(so) CAMEL_NNTP_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))


enum {
	USE_SSL_NEVER,
	USE_SSL_ALWAYS,
	USE_SSL_WHEN_POSSIBLE
};

static gboolean
connect_to_server (CamelService *service, int ssl_mode, CamelException *ex)
{
	CamelNNTPStore *store = (CamelNNTPStore *) service;
	CamelStream *tcp_stream;
	gboolean retval = FALSE;
	unsigned char *buf;
	unsigned int len;
	struct hostent *h;
	int port, ret;
	
	CAMEL_NNTP_STORE_LOCK(store, command_lock);
		
	/* setup store-wide cache */
	if (store->cache == NULL) {
		char *root;
		
		root = camel_session_get_storage_path (service->session, service, ex);
		if (root == NULL)
			goto fail;
		
		store->cache = camel_data_cache_new (root, 0, ex);
		g_free (root);
		if (store->cache == NULL)
			goto fail;
		
		/* Default cache expiry - 2 weeks old, or not visited in 5 days */
		camel_data_cache_set_expire_age (store->cache, 60*60*24*14);
		camel_data_cache_set_expire_access (store->cache, 60*60*24*5);
	}
	
	h = camel_service_gethost (service, ex);
	if (!h)
		goto fail;
	
	port = service->url->port ? service->url->port : NNTP_PORT;
	
#ifdef HAVE_SSL
	if (ssl_mode != USE_SSL_NEVER) {
		port = service->url->port ? service->url->port : NNTPS_PORT;
		tcp_stream = camel_tcp_stream_ssl_new (service, service->url->host);
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
		
		goto fail;
	}
	
	store->stream = (CamelNNTPStream *) camel_nntp_stream_new (tcp_stream);
	camel_object_unref (CAMEL_OBJECT (tcp_stream));
	
	/* Read the greeting, if any. */
	if (camel_nntp_stream_line (store->stream, &buf, &len) == -1) {
		if (errno == EINTR)
			camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL,
					     _("Connection cancelled"));
		else
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					      _("Could not read greeting from %s: %s"),
					      service->url->host, g_strerror (errno));
		
		camel_object_unref (CAMEL_OBJECT (store->stream));
		store->stream = NULL;
		
		goto fail;
	}
	
	len = strtoul (buf, (char **) &buf, 10);
	if (len != 200 && len != 201) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("NNTP server %s returned error code %d: %s"),
				      service->url->host, len, buf);
		
		camel_object_unref (CAMEL_OBJECT (store->stream));
		store->stream = NULL;
		
		goto fail;
	}
	
	/* set 'reader' mode & ignore return code */
	camel_nntp_command (store, (char **) &buf, "mode reader");
	retval = TRUE;
	
 fail:
	CAMEL_NNTP_STORE_UNLOCK(store, command_lock);
	
	return retval;
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
nntp_connect (CamelService *service, CamelException *ex)
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
		/* Connect via SSL */
		return connect_to_server (service, ssl_mode, ex);
	} else if (ssl_mode == USE_SSL_WHEN_POSSIBLE) {
		/* If the server supports SSL, use it */
		if (!connect_to_server (service, ssl_mode, ex)) {
			if (camel_exception_get_id (ex) == CAMEL_EXCEPTION_SERVICE_UNAVAILABLE) {
				/* The ssl port seems to be unavailable, fall back to plain NNTP */
				camel_exception_clear (ex);
				return connect_to_server (service, USE_SSL_NEVER, ex);
			} else {
				return FALSE;
			}
		}
		
		return TRUE;
	} else {
		/* User doesn't care about SSL */
		return connect_to_server (service, ssl_mode, ex);
	}
#else
	return connect_to_server (service, USE_SSL_NEVER, ex);
#endif
}

static gboolean
nntp_disconnect (CamelService *service, gboolean clean, CamelException *ex)
{
	CamelNNTPStore *store = CAMEL_NNTP_STORE (service);
	char *line;

	CAMEL_NNTP_STORE_LOCK(store, command_lock);

	if (clean)
		camel_nntp_command (store, &line, "quit");

	if (!service_class->disconnect (service, clean, ex))
		return FALSE;

	camel_object_unref((CamelObject *)store->stream);
	store->stream = NULL;

	CAMEL_NNTP_STORE_UNLOCK(store, command_lock);

	return TRUE;
}

static char *
nntp_store_get_name (CamelService *service, gboolean brief)
{
	if (brief)
		return g_strdup_printf ("%s", service->url->host);
	else
		return g_strdup_printf (_("USENET News via %s"), service->url->host);
	
}

static CamelServiceAuthType password_authtype = {
	N_("Password"),
	
	N_("This option will authenticate with the NNTP server using a "
	   "plaintext password."),
	
	"",
	TRUE
};

static GList *
nntp_store_query_auth_types (CamelService *service, CamelException *ex)
{
	g_warning ("nntp::query_auth_types: not implemented. Defaulting.");
	
	return g_list_append (NULL, &password_authtype);
}

static CamelFolder *
nntp_store_get_folder(CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex)
{
	CamelNNTPStore *nntp_store = CAMEL_NNTP_STORE (store);
	CamelFolder *folder;

	CAMEL_NNTP_STORE_LOCK(nntp_store, command_lock);

	folder = camel_nntp_folder_new(store, folder_name, ex);

	CAMEL_NNTP_STORE_UNLOCK(nntp_store, command_lock);

	return folder;
}

static CamelFolderInfo *
nntp_store_get_folder_info(CamelStore *store, const char *top, guint32 flags, CamelException *ex)
{
	CamelURL *url = CAMEL_SERVICE (store)->url;
	CamelNNTPStore *nntp_store = (CamelNNTPStore *)store;
	CamelFolderInfo *groups = NULL, *last = NULL, *fi;
	unsigned int len;
	unsigned char *line, *space;
	int ret = -1;

	CAMEL_NNTP_STORE_LOCK(nntp_store, command_lock);

	ret = camel_nntp_command(nntp_store, (char **)&line, "list");
	if (ret != 215) {
		ret = -1;
		goto error;
	}

	while ( (ret = camel_nntp_stream_line(nntp_store->stream, &line, &len)) > 0) {
		space = strchr(line, ' ');
		if (space)
			*space = 0;

		if (top == NULL || top[0] == 0 || strcmp(top, line) == 0) {
			fi = g_malloc0(sizeof(*fi));
			fi->name = g_strdup(line);
			fi->full_name = g_strdup(line);
			if (url->user)
				fi->url = g_strdup_printf ("nntp://%s@%s/%s", url->user, url->host, line);
			else
				fi->url = g_strdup_printf ("nntp://%s/%s", url->host, line);
			fi->unread_message_count = -1;
			camel_folder_info_build_path(fi, '/');

			if (last)
				last->sibling = fi;
			else
				groups = fi;
			last = fi;
		}
	}

	if (ret < 0)
		goto error;

	CAMEL_NNTP_STORE_UNLOCK(nntp_store, command_lock);

	return groups;

error:
	CAMEL_NNTP_STORE_UNLOCK(nntp_store, command_lock);

	if (groups)
		camel_store_free_folder_info(store, groups);

	return NULL;
}

static gboolean
nntp_store_folder_subscribed (CamelStore *store, const char *folder_name)
{
	CamelNNTPStore *nntp_store = CAMEL_NNTP_STORE (store);

	nntp_store = nntp_store;

	/* FIXME: implement */

	return TRUE;
}

static void
nntp_store_subscribe_folder (CamelStore *store, const char *folder_name,
			     CamelException *ex)
{
	CamelNNTPStore *nntp_store = CAMEL_NNTP_STORE (store);

	nntp_store = nntp_store;

	/* FIXME: implement */
}

static void
nntp_store_unsubscribe_folder (CamelStore *store, const char *folder_name,
			       CamelException *ex)
{
	CamelNNTPStore *nntp_store = CAMEL_NNTP_STORE (store);

	nntp_store = nntp_store;

	/* FIXME: implement */
}

static void
nntp_store_finalise (CamelObject *object)
{
	CamelNNTPStore *nntp_store = CAMEL_NNTP_STORE (object);
	struct _CamelNNTPStorePrivate *p = nntp_store->priv;

	camel_service_disconnect((CamelService *)object, TRUE, NULL);

	camel_object_unref((CamelObject *)nntp_store->mem);
	nntp_store->mem = NULL;
	if (nntp_store->stream)
		camel_object_unref((CamelObject *)nntp_store->stream);
	
	e_mutex_destroy(p->command_lock);
	
	g_free(p);
}

static void
nntp_store_class_init (CamelNNTPStoreClass *camel_nntp_store_class)
{
	CamelStoreClass *camel_store_class = CAMEL_STORE_CLASS (camel_nntp_store_class);
	CamelServiceClass *camel_service_class = CAMEL_SERVICE_CLASS (camel_nntp_store_class);

	parent_class = CAMEL_STORE_CLASS (camel_type_get_global_classfuncs (camel_store_get_type ()));

	service_class = CAMEL_SERVICE_CLASS (camel_type_get_global_classfuncs (camel_service_get_type ()));
	
	/* virtual method overload */
	camel_service_class->connect = nntp_connect;
	camel_service_class->disconnect = nntp_disconnect;
	camel_service_class->query_auth_types = nntp_store_query_auth_types;
	camel_service_class->get_name = nntp_store_get_name;
	
	camel_store_class->get_folder = nntp_store_get_folder;
	camel_store_class->get_folder_info = nntp_store_get_folder_info;
	camel_store_class->free_folder_info = camel_store_free_folder_info_full;
	
	camel_store_class->folder_subscribed = nntp_store_folder_subscribed;
	camel_store_class->subscribe_folder = nntp_store_subscribe_folder;
	camel_store_class->unsubscribe_folder = nntp_store_unsubscribe_folder;
}

static void
nntp_store_init (gpointer object, gpointer klass)
{
	CamelNNTPStore *nntp_store = CAMEL_NNTP_STORE(object);
	CamelStore *store = CAMEL_STORE (object);
	struct _CamelNNTPStorePrivate *p;

	store->flags = CAMEL_STORE_SUBSCRIPTIONS;

	nntp_store->mem = (CamelStreamMem *)camel_stream_mem_new();

	p = nntp_store->priv = g_malloc0(sizeof(*p));
	p->command_lock = e_mutex_new(E_MUTEX_REC);
}

CamelType
camel_nntp_store_get_type (void)
{
	static CamelType camel_nntp_store_type = CAMEL_INVALID_TYPE;
	
	if (camel_nntp_store_type == CAMEL_INVALID_TYPE) {
		camel_nntp_store_type =
			camel_type_register (CAMEL_STORE_TYPE,
					     "CamelNNTPStore",
					     sizeof (CamelNNTPStore),
					     sizeof (CamelNNTPStoreClass),
					     (CamelObjectClassInitFunc) nntp_store_class_init,
					     NULL,
					     (CamelObjectInitFunc) nntp_store_init,
					     (CamelObjectFinalizeFunc) nntp_store_finalise);
	}
	
	return camel_nntp_store_type;
}

/* enter owning lock */
int
camel_nntp_store_set_folder (CamelNNTPStore *store, CamelFolder *folder, CamelFolderChangeInfo *changes, CamelException *ex)
{
	int ret;

	if (store->current_folder && strcmp(folder->full_name, store->current_folder) == 0)
		return 0;

	/* FIXME: Do something with changeinfo */
	ret = camel_nntp_summary_check((CamelNNTPSummary *)folder->summary, changes, ex);

	g_free(store->current_folder);
	store->current_folder = g_strdup(folder->full_name);

	return ret;
}

static gboolean
nntp_connected (CamelNNTPStore *store, CamelException *ex)
{
	if (store->stream == NULL)
		return camel_service_connect (CAMEL_SERVICE (store), ex);
	return TRUE;
}

/* Enter owning lock */
int
camel_nntp_command(CamelNNTPStore *store, char **line, const char *fmt, ...)
{
	const unsigned char *p, *ps;
	unsigned char c;
	va_list ap;
	char *s;
	int d;
	unsigned int u, u2;

	e_mutex_assert_locked(store->priv->command_lock);

	if (!nntp_connected (store, NULL))
		return -1;
	
	/* Check for unprocessed data, ! */
	if (store->stream->mode == CAMEL_NNTP_STREAM_DATA) {
		g_warning("Unprocessed data left in stream, flushing");
		while (camel_nntp_stream_getd(store->stream, (unsigned char **)&p, &u) > 0)
			;
	}
	camel_nntp_stream_set_mode(store->stream, CAMEL_NNTP_STREAM_LINE);

	va_start(ap, fmt);
	ps = p = fmt;
	while ( (c = *p++) ) {
		switch (c) {
		case '%':
			c = *p++;
			camel_stream_write((CamelStream *)store->mem, ps, p-ps-(c=='%'?1:2));
			ps = p;
			switch (c) {
			case 's':
				s = va_arg(ap, char *);
				camel_stream_write((CamelStream *)store->mem, s, strlen(s));
				break;
			case 'd':
				d = va_arg(ap, int);
				camel_stream_printf((CamelStream *)store->mem, "%d", d);
				break;
			case 'u':
				u = va_arg(ap, unsigned int);
				camel_stream_printf((CamelStream *)store->mem, "%u", u);
				break;
			case 'm':
				s = va_arg(ap, char *);
				camel_stream_printf((CamelStream *)store->mem, "<%s>", s);
				break;
			case 'r':
				u = va_arg(ap, unsigned int);
				u2 = va_arg(ap, unsigned int);
				if (u == u2)
					camel_stream_printf((CamelStream *)store->mem, "%u", u);
				else
					camel_stream_printf((CamelStream *)store->mem, "%u-%u", u, u2);
				break;
			default:
				g_warning("Passing unknown format to nntp_command: %c\n", c);
				g_assert(0);
			}
		}
	}

	camel_stream_write((CamelStream *)store->mem, ps, p-ps-1);
	dd(printf("NNTP_COMMAND: '%.*s'\n", (int)store->mem->buffer->len, store->mem->buffer->data));
	camel_stream_write((CamelStream *)store->mem, "\r\n", 2);
	camel_stream_write((CamelStream *)store->stream, store->mem->buffer->data, store->mem->buffer->len);
	camel_stream_reset((CamelStream *)store->mem);
	/* FIXME: hack */
	g_byte_array_set_size(store->mem->buffer, 0);

	if (camel_nntp_stream_line(store->stream, (unsigned char **)line, &u) == -1)
		return -1;

	u = strtoul(*line, NULL, 10);

	/* Handle all switching to data mode here, to make callers job easier */
	if (u == 215 || (u >= 220 && u <=224) || (u >= 230 && u <= 231))
		camel_nntp_stream_set_mode(store->stream, CAMEL_NNTP_STREAM_DATA);

	return u;
}

