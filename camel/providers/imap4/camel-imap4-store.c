/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  Camel
 *  Copyright (C) 1999-2004 Jeffrey Stedfast
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
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <camel/camel-sasl.h>
#include <camel/camel-utf8.h>
#include <camel/camel-tcp-stream-raw.h>
#include <camel/camel-tcp-stream-ssl.h>

#include <camel/camel-private.h>

#include "camel-imap4-store.h"
#include "camel-imap4-engine.h"
#include "camel-imap4-folder.h"
#include "camel-imap4-stream.h"
#include "camel-imap4-command.h"
#include "camel-imap4-utils.h"
#include "camel-imap4-summary.h"


static void camel_imap4_store_class_init (CamelIMAP4StoreClass *klass);
static void camel_imap4_store_init (CamelIMAP4Store *store, CamelIMAP4StoreClass *klass);
static void camel_imap4_store_finalize (CamelObject *object);

/* service methods */
static void imap4_construct (CamelService *service, CamelSession *session,
			     CamelProvider *provider, CamelURL *url,
			     CamelException *ex);
static char *imap4_get_name (CamelService *service, gboolean brief);
static gboolean imap4_connect (CamelService *service, CamelException *ex);
static gboolean imap4_disconnect (CamelService *service, gboolean clean, CamelException *ex);
static GList *imap4_query_auth_types (CamelService *service, CamelException *ex);

/* store methods */
static CamelFolder *imap4_get_folder (CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex);
static CamelFolderInfo *imap4_create_folder (CamelStore *store, const char *parent_name, const char *folder_name, CamelException *ex);
static void imap4_delete_folder (CamelStore *store, const char *folder_name, CamelException *ex);
static void imap4_rename_folder (CamelStore *store, const char *old_name, const char *new_name, CamelException *ex);
static CamelFolderInfo *imap4_get_folder_info (CamelStore *store, const char *top, guint32 flags, CamelException *ex);
static void imap4_subscribe_folder (CamelStore *store, const char *folder_name, CamelException *ex);
static void imap4_unsubscribe_folder (CamelStore *store, const char *folder_name, CamelException *ex);
static void imap4_noop (CamelStore *store, CamelException *ex);


static CamelStoreClass *parent_class = NULL;


CamelType
camel_imap4_store_get_type (void)
{
	static CamelType type = 0;
	
	if (!type) {
		type = camel_type_register (CAMEL_STORE_TYPE,
					    "CamelIMAP4Store",
					    sizeof (CamelIMAP4Store),
					    sizeof (CamelIMAP4StoreClass),
					    (CamelObjectClassInitFunc) camel_imap4_store_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_imap4_store_init,
					    (CamelObjectFinalizeFunc) camel_imap4_store_finalize);
	}
	
	return type;
}

static guint
imap4_hash_folder_name (gconstpointer key)
{
	if (g_ascii_strcasecmp (key, "INBOX") == 0)
		return g_str_hash ("INBOX");
	else
		return g_str_hash (key);
}

static int
imap4_compare_folder_name (gconstpointer a, gconstpointer b)
{
	gconstpointer aname = a, bname = b;
	
	if (g_ascii_strcasecmp (a, "INBOX") == 0)
		aname = "INBOX";
	if (g_ascii_strcasecmp (b, "INBOX") == 0)
		bname = "INBOX";
	
	return g_str_equal (aname, bname);
}

static void
camel_imap4_store_class_init (CamelIMAP4StoreClass *klass)
{
	CamelServiceClass *service_class = (CamelServiceClass *) klass;
	CamelStoreClass *store_class = (CamelStoreClass *) klass;
	
	parent_class = (CamelStoreClass *) camel_type_get_global_classfuncs (CAMEL_STORE_TYPE);
	
	service_class->construct = imap4_construct;
	service_class->get_name = imap4_get_name;
	service_class->connect = imap4_connect;
	service_class->disconnect = imap4_disconnect;
	service_class->query_auth_types = imap4_query_auth_types;
	
	store_class->hash_folder_name = imap4_hash_folder_name;
	store_class->compare_folder_name = imap4_compare_folder_name;
	
	store_class->get_folder = imap4_get_folder;
	store_class->create_folder = imap4_create_folder;
	store_class->delete_folder = imap4_delete_folder;
	store_class->rename_folder = imap4_rename_folder;
	store_class->get_folder_info = imap4_get_folder_info;
	store_class->subscribe_folder = imap4_subscribe_folder;
	store_class->unsubscribe_folder = imap4_unsubscribe_folder;
	store_class->noop = imap4_noop;
}

static void
camel_imap4_store_init (CamelIMAP4Store *store, CamelIMAP4StoreClass *klass)
{
	store->engine = NULL;
}

static void
camel_imap4_store_finalize (CamelObject *object)
{
	CamelIMAP4Store *store = (CamelIMAP4Store *) object;
	
	if (store->engine)
		camel_object_unref (store->engine);
	
	g_free (store->storage_path);
}


static void
imap4_construct (CamelService *service, CamelSession *session, CamelProvider *provider, CamelURL *url, CamelException *ex)
{
	CamelIMAP4Store *imap_store = (CamelIMAP4Store *) service;
	
	CAMEL_SERVICE_CLASS (parent_class)->construct (service, session, provider, url, ex);
	if (camel_exception_is_set (ex))
		return;
	
	imap_store->storage_path = camel_session_get_storage_path (session, service, ex);
}

static char *
imap4_get_name (CamelService *service, gboolean brief)
{
	if (brief)
		return g_strdup_printf (_("IMAP server %s"), service->url->host);
	else
		return g_strdup_printf (_("IMAP service for %s on %s"),
					service->url->user, service->url->host);
}

enum {
	USE_SSL_NEVER,
	USE_SSL_ALWAYS,
	USE_SSL_WHEN_POSSIBLE
};

#define SSL_PORT_FLAGS (CAMEL_TCP_STREAM_SSL_ENABLE_SSL2 | CAMEL_TCP_STREAM_SSL_ENABLE_SSL3)
#define STARTTLS_FLAGS (CAMEL_TCP_STREAM_SSL_ENABLE_TLS)

static gboolean
connect_to_server (CamelService *service, struct hostent *host, int ssl_mode, int try_starttls, CamelException *ex)
{
	CamelIMAP4Store *store = (CamelIMAP4Store *) service;
	CamelIMAP4Engine *engine;
	CamelStream *tcp_stream;
	int port, ret;
	
	if (store->engine) {
		camel_object_unref (store->engine);
		store->engine = NULL;
	}
	
	port = service->url->port ? service->url->port : 143;
	
	if (ssl_mode) {
#ifdef HAVE_SSL
		if (try_starttls) {
			tcp_stream = camel_tcp_stream_ssl_new (service->session, service->url->host, STARTTLS_FLAGS);
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
		
		return FALSE;
#endif /* HAVE_SSL */
	} else {
		tcp_stream = camel_tcp_stream_raw_new ();
	}
	
	fprintf (stderr, "connecting to %s:%d\n", service->url->host, port);
	if ((ret = camel_tcp_stream_connect ((CamelTcpStream *) tcp_stream, host, port)) == -1) {
		if (errno == EINTR)
			camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL,
					     _("Connection cancelled"));
		else
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					      _("Could not connect to %s (port %d): %s"),
					      service->url->host, port,
					      g_strerror (errno));
		
		camel_object_unref (tcp_stream);
		
		return FALSE;
	}
	
	engine = camel_imap4_engine_new (service->session, service->url);
	if (camel_imap4_engine_take_stream (engine, tcp_stream, ex) == -1) {
		camel_object_unref (engine);
		
		return FALSE;
	}
	
	if (camel_imap4_engine_capability (engine, ex) == -1) {
		camel_object_unref (engine);
		
		return FALSE;
	}
	
	store->engine = engine;
	
#ifdef HAVE_SSL
	if (ssl_mode == USE_SSL_WHEN_POSSIBLE) {
		/* try_starttls is always TRUE here */
		if (engine->capa & CAMEL_IMAP4_CAPABILITY_STARTTLS)
			goto starttls;
	} else if (ssl_mode == USE_SSL_ALWAYS) {
		if (try_starttls) {
			if (engine->capa & CAMEL_IMAP4_CAPABILITY_STARTTLS) {
				goto starttls;
			} else {
				/* server doesn't support STARTTLS, abort */
				camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
						      _("Failed to connect to IMAP server %s in secure mode: "
							"Server does not support STARTTLS"),
						      service->url->host);
				goto exception;
			}
		}
	}
#endif /* HAVE_SSL */
	
	return TRUE;
	
#ifdef HAVE_SSL
 starttls:
	
	if (1) {
		CamelIMAP4Command *ic;
		int id;
		
		ic = camel_imap4_engine_queue (engine, NULL, "STARTTLS\r\n");
		while ((id = camel_imap4_engine_iterate (engine)) < ic->id && id != -1)
			;
		
		if (id == -1 || ic->result != CAMEL_IMAP4_RESULT_OK) {
			if (ic->result != CAMEL_IMAP4_RESULT_OK) {
				camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
						      _("Failed to connect to IMAP server %s in secure mode: %s"),
						      service->url->host, _("Unknown error"));
			} else {
				camel_exception_xfer (ex, &ic->ex);
			}
			
			camel_imap4_command_unref (ic);
			
			goto exception;
		}
		
		camel_imap4_command_unref (ic);
	}
	
	return TRUE;
	
 exception:
	
	camel_object_unref (store->engine);
	store->engine = NULL;
	
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
	const char *use_ssl;
	struct hostent *h;
	int ssl_mode;
	int ret, i;
	
	if (!(h = camel_service_gethost (service, ex)))
		return FALSE;
	
	if ((use_ssl = camel_url_get_param (service->url, "use_ssl"))) {
		for (i = 0; ssl_options[i].value; i++)
			if (!strcmp (ssl_options[i].value, use_ssl))
				break;
		ssl_mode = ssl_options[i].mode;
	} else {
		ssl_mode = USE_SSL_NEVER;
	}
	
	if (ssl_mode == USE_SSL_ALWAYS) {
		/* First try the ssl port */
		if (!(ret = connect_to_server (service, h, ssl_mode, FALSE, ex))) {
			if (camel_exception_get_id (ex) == CAMEL_EXCEPTION_SERVICE_UNAVAILABLE) {
				/* The ssl port seems to be unavailable, lets try STARTTLS */
				camel_exception_clear (ex);
				ret = connect_to_server (service, h, ssl_mode, TRUE, ex);
			}
		}
	} else if (ssl_mode == USE_SSL_WHEN_POSSIBLE) {
		/* If the server supports STARTTLS, use it */
		ret = connect_to_server (service, h, ssl_mode, TRUE, ex);
	} else {
		/* User doesn't care about SSL */
		ret = connect_to_server (service, h, USE_SSL_NEVER, FALSE, ex);
	}
	
	camel_free_host (h);
	
	return ret;
}

static int
sasl_auth (CamelIMAP4Engine *engine, CamelIMAP4Command *ic, const unsigned char *linebuf, size_t linelen, CamelException *ex)
{
	/* Perform a single challenge iteration */
	CamelSasl *sasl = ic->user_data;
	char *challenge;
	
	if (camel_sasl_authenticated (sasl)) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
				      _("Cannot authenticate to IMAP server %s using the %s authentication mechanism"),
				      engine->url->host, engine->url->authmech);
		return -1;
	}
	
	while (isspace (*linebuf))
		linebuf++;
	
	if (*linebuf == '\0')
		linebuf = NULL;
	
	if (!(challenge = camel_sasl_challenge_base64 (sasl, (const char *) linebuf, ex)))
		return -1;
	
	fprintf (stderr, "sending : %s\r\n", challenge);
	
	if (camel_stream_printf (engine->ostream, "%s\r\n", challenge) == -1) {
		g_free (challenge);
		return -1;
	}
	
	g_free (challenge);
	
	if (camel_stream_flush (engine->ostream) == -1)
		return -1;
	
	return 0;
}

static int
imap4_try_authenticate (CamelService *service, gboolean reprompt, const char *errmsg, CamelException *ex)
{
	CamelIMAP4Store *store = (CamelIMAP4Store *) service;
	CamelSession *session = service->session;
	CamelSasl *sasl = NULL;
	CamelIMAP4Command *ic;
	int id;
	
	if (!service->url->passwd) {
		guint32 flags = CAMEL_SESSION_PASSWORD_SECRET;
		char *prompt;
		
		if (reprompt)
			flags |= CAMEL_SESSION_PASSWORD_REPROMPT;
		
		prompt = g_strdup_printf (_("%sPlease enter the IMAP password for %s on host %s"),
					  errmsg ? errmsg : "",
					  service->url->user,
					  service->url->host);
		
		service->url->passwd = camel_session_get_password (session, service, NULL, prompt, "password", flags, ex);
		
		g_free (prompt);
		
		if (!service->url->passwd)
			return FALSE;
	}
	
	if (service->url->authmech) {
		CamelServiceAuthType *mech;
		
		mech = g_hash_table_lookup (store->engine->authtypes, service->url->authmech);
		sasl = camel_sasl_new ("imap4", mech->authproto, service);
		
		ic = camel_imap4_engine_queue (store->engine, NULL, "AUTHENTICATE %s\r\n", service->url->authmech);
		ic->plus = sasl_auth;
		ic->user_data = sasl;
	} else {
		ic = camel_imap4_engine_queue (store->engine, NULL, "LOGIN %S %S\r\n",
					       service->url->user, service->url->passwd);
	}
	
	while ((id = camel_imap4_engine_iterate (store->engine)) < ic->id && id != -1)
		;
	
	if (sasl != NULL)
		camel_object_unref (sasl);
	
	if (id == -1 || ic->status == CAMEL_IMAP4_COMMAND_ERROR) {
		/* unrecoverable error */
		camel_exception_xfer (ex, &ic->ex);
		camel_imap4_command_unref (ic);
		
		return FALSE;
	}
	
	if (ic->result != CAMEL_IMAP4_RESULT_OK) {
		camel_imap4_command_unref (ic);
		
		/* try again */
		
		return TRUE;
	}
	
	camel_imap4_command_unref (ic);
	
	return FALSE;
}

static gboolean
imap4_connect (CamelService *service, CamelException *ex)
{
	CamelIMAP4Store *store = (CamelIMAP4Store *) service;
	CamelServiceAuthType *mech;
	gboolean reprompt = FALSE;
	char *errmsg = NULL;
	CamelException lex;
	
	CAMEL_SERVICE_LOCK (store, connect_lock);
	
	if (!connect_to_server_wrapper (service, ex)) {
		CAMEL_SERVICE_UNLOCK (store, connect_lock);
		return FALSE;
	}
	
#define CANT_USE_AUTHMECH (!(mech = g_hash_table_lookup (store->engine->authtypes, service->url->authmech)))
	if (service->url->authmech && CANT_USE_AUTHMECH) {
		/* Oops. We can't AUTH using the requested mechanism */
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
				      _("Cannot authenticate to IMAP server %s using %s"),
				      service->url->host, service->url->authmech);
		
		camel_object_unref (store->engine);
		store->engine = NULL;
		
		CAMEL_SERVICE_UNLOCK (store, connect_lock);
		
		return FALSE;
	}
	
	camel_exception_init (&lex);
	while (imap4_try_authenticate (service, reprompt, errmsg, &lex)) {
		g_free (errmsg);
		errmsg = g_strdup (lex.desc);
		camel_exception_clear (&lex);
		reprompt = TRUE;
	}
	g_free (errmsg);
	
	if (camel_exception_is_set (&lex)) {
		camel_exception_xfer (ex, &lex);
		camel_object_unref (store->engine);
		store->engine = NULL;
		
		CAMEL_SERVICE_UNLOCK (store, connect_lock);
		
		return FALSE;
	}
	
	if (camel_imap4_engine_namespace (store->engine, ex) == -1) {
		camel_object_unref (store->engine);
		store->engine = NULL;
		
		CAMEL_SERVICE_UNLOCK (store, connect_lock);
		
		return FALSE;
	}
	
	CAMEL_SERVICE_UNLOCK (store, connect_lock);
	
	return TRUE;
}

static gboolean
imap4_disconnect (CamelService *service, gboolean clean, CamelException *ex)
{
	CamelIMAP4Store *store = (CamelIMAP4Store *) service;
	CamelIMAP4Command *ic;
	int id;
	
	if (clean && !store->engine->istream->disconnected) {
		ic = camel_imap4_engine_queue (store->engine, NULL, "LOGOUT\r\n");
		while ((id = camel_imap4_engine_iterate (store->engine)) < ic->id && id != -1)
			;
		
		camel_imap4_command_unref (ic);
	}
	
	camel_object_unref (store->engine);
	
	return 0;
}

extern CamelServiceAuthType camel_imap4_password_authtype;

static GList *
imap4_query_auth_types (CamelService *service, CamelException *ex)
{
	CamelIMAP4Store *store = (CamelIMAP4Store *) service;
	CamelServiceAuthType *authtype;
	GList *sasl_types, *t, *next;
	gboolean connected;
	
	CAMEL_SERVICE_LOCK (store, connect_lock);
	connected = connect_to_server_wrapper (service, ex);
	CAMEL_SERVICE_UNLOCK (store, connect_lock);
	if (!connected)
		return NULL;
	
	sasl_types = camel_sasl_authtype_list (FALSE);
	for (t = sasl_types; t; t = next) {
		authtype = t->data;
		next = t->next;
		
		if (!g_hash_table_lookup (store->engine->authtypes, authtype->authproto)) {
			sasl_types = g_list_remove_link (sasl_types, t);
			g_list_free_1 (t);
		}
	}
	
	return g_list_prepend (sasl_types, &camel_imap4_password_authtype);
}


static char
imap4_get_path_delim (CamelIMAP4Engine *engine, const char *full_name)
{
	/* FIXME: move this to utils so imap4-folder.c can share */
	CamelIMAP4Namespace *namespace;
	const char *slash;
	size_t len;
	char *top;
	
	if ((slash = strchr (full_name, '/')))
		len = (slash - full_name);
	else
		len = strlen (full_name);
	
	top = g_alloca (len + 1);
	memcpy (top, full_name, len);
	top[len] = '\0';
	
	if (!g_ascii_strcasecmp (top, "INBOX"))
		top = "INBOX";
	
 retry:
	namespace = engine->namespaces.personal;
	while (namespace != NULL) {
		if (!strcmp (namespace->path, top))
			return namespace->sep;
		namespace = namespace->next;
	}
	
	namespace = engine->namespaces.other;
	while (namespace != NULL) {
		if (!strcmp (namespace->path, top))
			return namespace->sep;
		namespace = namespace->next;
	}
	
	namespace = engine->namespaces.shared;
	while (namespace != NULL) {
		if (!strcmp (namespace->path, top))
			return namespace->sep;
		namespace = namespace->next;
	}
	
	if (top[0] != '\0') {
		/* look for a default namespace? */
		top[0] = '\0';
		goto retry;
	}
	
	return '/';
}

static char *
imap4_folder_utf7_name (CamelStore *store, const char *folder_name, char wildcard)
{
	char *real_name, *p;
	char sep;
	int len;
	
	sep = imap4_get_path_delim (((CamelIMAP4Store *) store)->engine, folder_name);
	
	if (sep != '/') {
		p = real_name = g_alloca (strlen (folder_name) + 1);
		strcpy (real_name, folder_name);
		while (*p != '\0') {
			if (*p == '/')
				*p = sep;
			p++;
		}
		
		folder_name = real_name;
	}
	
	if (*folder_name)
		real_name = camel_utf8_utf7 (folder_name);
	else
		real_name = g_strdup ("");
	
	if (wildcard) {
		len = strlen (real_name);
		real_name = g_realloc (real_name, len + 3);
		
		if (len > 0)
			real_name[len++] = sep;
		
		real_name[len++] = wildcard;
		real_name[len] = '\0';
	}
	
	return real_name;
}

static CamelFolder *
imap4_get_folder (CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex)
{
	CamelIMAP4Engine *engine = ((CamelIMAP4Store *) store)->engine;
	CamelFolder *folder = NULL;
	camel_imap4_list_t *list;
	CamelIMAP4Command *ic;
	CamelFolderInfo *fi;
	GPtrArray *array;
	char *utf7_name;
	int create;
	int id, i;
	
	CAMEL_SERVICE_LOCK (store, connect_lock);
	
	/* make sure the folder exists - try LISTing it? */
	utf7_name = imap4_folder_utf7_name (store, folder_name, '\0');
	ic = camel_imap4_engine_queue (engine, NULL, "LIST \"\" %S\r\n", utf7_name);
	camel_imap4_command_register_untagged (ic, "LIST", camel_imap4_untagged_list);
	ic->user_data = array = g_ptr_array_new ();
	g_free (utf7_name);
	
	while ((id = camel_imap4_engine_iterate (engine)) < ic->id && id != -1)
		;
	
	if (id == -1 || ic->status != CAMEL_IMAP4_COMMAND_COMPLETE) {
		camel_exception_xfer (ex, &ic->ex);
		camel_imap4_command_unref (ic);
		g_ptr_array_free (array, TRUE);
		goto done;
	}
	
	create = array->len == 0;
	
	for (i = 0; i < array->len; i++) {
		list = array->pdata[i];
		g_free (list->name);
		g_free (list);
	}
	
	g_ptr_array_free (array, TRUE);
	
	if (ic->result != CAMEL_IMAP4_RESULT_OK) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot get folder `%s' on IMAP server %s: Unknown"),
				      folder_name, ((CamelService *) store)->url->host);
		camel_imap4_command_unref (ic);
		goto done;
	}
	
	camel_imap4_command_unref (ic);
	
	if (create) {
		const char *basename;
		char *parent;
		int len;
		
		if (!(flags & CAMEL_STORE_FOLDER_CREATE))
			goto done;
		
		if (!(basename = strrchr (folder_name, '/')))
			basename = folder_name;
		else
			basename++;
		
		len = basename > folder_name ? (basename - folder_name) - 1 : 0;
		parent = g_alloca (len + 1);
		memcpy (parent, folder_name, len);
		parent[len] = '\0';
		
		if (!(fi = imap4_create_folder (store, parent, basename, ex)))
			goto done;
		
		camel_store_free_folder_info (store, fi);
	}
	
	folder = camel_imap4_folder_new (store, folder_name, ex);
	
 done:
	
	CAMEL_SERVICE_UNLOCK (store, connect_lock);
	
	return folder;
}

static CamelFolderInfo *
imap4_create_folder (CamelStore *store, const char *parent_name, const char *folder_name, CamelException *ex)
{
	/* FIXME: also need to deal with parent folders that can't
	 * contain subfolders - delete them and re-create with the
	 * proper hint */
	CamelIMAP4Engine *engine = ((CamelIMAP4Store *) store)->engine;
	CamelFolderInfo *fi = NULL;
	CamelIMAP4Command *ic;
	char *utf7_name;
	CamelURL *url;
	const char *c;
	char *name;
	char sep;
	int id;
	
	sep = imap4_get_path_delim (engine, parent_name);
	
	c = folder_name;
	while (*c != '\0') {
		if (*c == sep || strchr ("/#%*", *c)) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_FOLDER_INVALID_PATH,
					      _("The folder name \"%s\" is invalid because "
						"it contains the character \"%c\""),
					      folder_name, *c);
			return NULL;
		}
		
		c++;
	}
	
	if (parent_name != NULL && *parent_name)
		name = g_strdup_printf ("%s/%s", parent_name, folder_name);
	else
		name = g_strdup (folder_name);
	
	CAMEL_SERVICE_LOCK (store, connect_lock);
	
	utf7_name = imap4_folder_utf7_name (store, name, '\0');
	ic = camel_imap4_engine_queue (engine, NULL, "CREATE %S\r\n", utf7_name);
	g_free (utf7_name);
	
	while ((id = camel_imap4_engine_iterate (engine)) < ic->id && id != -1)
		;
	
	if (id == -1 || ic->status != CAMEL_IMAP4_COMMAND_COMPLETE) {
		camel_exception_xfer (ex, &ic->ex);
		camel_imap4_command_unref (ic);
		g_free (name);
		goto done;
	}
	
	switch (ic->result) {
	case CAMEL_IMAP4_RESULT_OK:
		url = camel_url_copy (engine->url);
		camel_url_set_fragment (url, name);
		
		c = strrchr (name, '/');
		
		fi = g_malloc0 (sizeof (CamelFolderInfo));
		fi->full_name = name;
		fi->name = g_strdup (c ? c + 1: name);
		fi->uri = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);
		camel_url_free (url);
		fi->flags = 0;
		fi->unread = -1;
		fi->total = -1;
		
		camel_object_trigger_event (store, "folder_created", fi);
		break;
	case CAMEL_IMAP4_RESULT_NO:
		/* FIXME: would be good to save the NO reason into the err message */
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot create folder `%s': Invalid mailbox name"),
				      name);
		g_free (name);
		break;
	case CAMEL_IMAP4_RESULT_BAD:
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot create folder `%s': Bad command"),
				      name);
		g_free (name);
		break;
	default:
		g_assert_not_reached ();
	}
	
	camel_imap4_command_unref (ic);
	
 done:
	
	CAMEL_SERVICE_UNLOCK (store, connect_lock);
	
	return fi;
}

static void
imap4_delete_folder (CamelStore *store, const char *folder_name, CamelException *ex)
{
	CamelIMAP4Engine *engine = ((CamelIMAP4Store *) store)->engine;
	CamelFolder *selected = (CamelFolder *) engine->folder;
	CamelIMAP4Command *ic, *ic0 = NULL;
	CamelFolderInfo *fi;
	char *utf7_name;
	CamelURL *url;
	const char *p;
	int id;
	
	if (!g_ascii_strcasecmp (folder_name, "INBOX")) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot delete folder `%s': Special folder"),
				      folder_name);
		
		return;
	}
	
	CAMEL_SERVICE_LOCK (store, connect_lock);
	
	if (selected && !strcmp (folder_name, selected->full_name))
		ic0 = camel_imap4_engine_queue (engine, NULL, "CLOSE\r\n");
	
	utf7_name = imap4_folder_utf7_name (store, folder_name, '\0');
	ic = camel_imap4_engine_queue (engine, NULL, "DELETE %S\r\n", utf7_name);
	g_free (utf7_name);
	
	while ((id = camel_imap4_engine_iterate (engine)) < ic->id && id != -1)
		;
	
	if (id == -1 || ic->status != CAMEL_IMAP4_COMMAND_COMPLETE) {
		if (ic0 && ic0->status != CAMEL_IMAP4_COMMAND_COMPLETE)
			camel_exception_xfer (ex, &ic0->ex);
		else
			camel_exception_xfer (ex, &ic->ex);
		
		if (ic0 != NULL)
			camel_imap4_command_unref (ic0);
		
		camel_imap4_command_unref (ic);
		CAMEL_SERVICE_UNLOCK (store, connect_lock);
		return;
	}
	
	if (ic0 != NULL)
		camel_imap4_command_unref (ic0);
	
	switch (ic->result) {
	case CAMEL_IMAP4_RESULT_OK:
		/* deleted */
		url = camel_url_copy (engine->url);
		camel_url_set_fragment (url, folder_name);
		
		p = strrchr (folder_name, '/');
		
		fi = g_malloc0 (sizeof (CamelFolderInfo));
		fi->full_name = g_strdup (folder_name);
		fi->name = g_strdup (p ? p + 1: folder_name);
		fi->uri = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);
		camel_url_free (url);
		fi->flags = 0;
		fi->unread = -1;
		fi->total = -1;
		
		camel_object_trigger_event (store, "folder_deleted", fi);
		
		camel_folder_info_free (fi);
		break;
	case CAMEL_IMAP4_RESULT_NO:
		/* FIXME: would be good to save the NO reason into the err message */
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot delete folder `%s': Invalid mailbox name"),
				      folder_name);
		break;
	case CAMEL_IMAP4_RESULT_BAD:
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot delete folder `%s': Bad command"),
				      folder_name);
		break;
	}
	
	camel_imap4_command_unref (ic);
	
	CAMEL_SERVICE_UNLOCK (store, connect_lock);
}

static void
imap4_rename_folder (CamelStore *store, const char *old_name, const char *new_name, CamelException *ex)
{
	CamelIMAP4Engine *engine = ((CamelIMAP4Store *) store)->engine;
	char *old_uname, *new_uname;
	CamelIMAP4Command *ic;
	int id;
	
	if (!g_ascii_strcasecmp (old_name, "INBOX")) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot rename folder `%s' to `%s': Special folder"),
				      old_name, new_name);
		
		return;
	}
	
	CAMEL_SERVICE_LOCK (store, connect_lock);
	
	old_uname = imap4_folder_utf7_name (store, old_name, '\0');
	new_uname = imap4_folder_utf7_name (store, new_name, '\0');
	
	ic = camel_imap4_engine_queue (engine, NULL, "RENAME %S %S\r\n", old_uname, new_uname);
	g_free (old_uname);
	g_free (new_uname);
	
	while ((id = camel_imap4_engine_iterate (engine)) < ic->id && id != -1)
		;
	
	if (id == -1 || ic->status != CAMEL_IMAP4_COMMAND_COMPLETE) {
		camel_exception_xfer (ex, &ic->ex);
		camel_imap4_command_unref (ic);
		CAMEL_SERVICE_UNLOCK (store, connect_lock);
		return;
	}
	
	switch (ic->result) {
	case CAMEL_IMAP4_RESULT_OK:
		/* FIXME: need to update state on the renamed folder object */
		break;
	case CAMEL_IMAP4_RESULT_NO:
		/* FIXME: would be good to save the NO reason into the err message */
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot rename folder `%s' to `%s': Invalid mailbox name"),
				      old_name, new_name);
		break;
	case CAMEL_IMAP4_RESULT_BAD:
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot rename folder `%s' to `%s': Bad command"),
				      old_name, new_name);
		break;
	}
	
	camel_imap4_command_unref (ic);
	
	CAMEL_SERVICE_UNLOCK (store, connect_lock);
}

static int
list_sort (const camel_imap4_list_t **list0, const camel_imap4_list_t **list1)
{
	return strcmp ((*list0)->name, (*list1)->name);
}

static void
list_remove_duplicates (GPtrArray *array)
{
	camel_imap4_list_t *list, *last;
	int i;
	
	last = array->pdata[0];
	for (i = 1; i < array->len; i++) {
		list = array->pdata[i];
		if (!strcmp (list->name, last->name)) {
			g_ptr_array_remove_index (array, i--);
			last->flags |= list->flags;
			g_free (list->name);
			g_free (list);
		}
	}
}

static void
imap4_status (CamelStore *store, CamelFolderInfo *fi)
{
	CamelIMAP4Engine *engine = ((CamelIMAP4Store *) store)->engine;
	camel_imap4_status_attr_t *attr, *next;
	camel_imap4_status_t *status;
	CamelIMAP4Command *ic;
	GPtrArray *array;
	char *mailbox;
	int id, i;
	
	mailbox = imap4_folder_utf7_name (store, fi->full_name, '\0');
	ic = camel_imap4_engine_queue (engine, NULL, "STATUS %S (MESSAGES UNSEEN)\r\n", mailbox);
	g_free (mailbox);
	
	camel_imap4_command_register_untagged (ic, "STATUS", camel_imap4_untagged_status);
	ic->user_data = array = g_ptr_array_new ();
	
	while ((id = camel_imap4_engine_iterate (engine)) < ic->id && id != -1)
		;
	
	if (id == -1 || ic->status != CAMEL_IMAP4_COMMAND_COMPLETE) {
		camel_imap4_command_unref (ic);
		g_ptr_array_free (array, TRUE);
		return;
	}
	
	for (i = 0; i < array->len; i++) {
		status = array->pdata[i];
		attr = status->attr_list;
		while (attr != NULL) {
			next = attr->next;
			if (attr->type == CAMEL_IMAP4_STATUS_MESSAGES)
				fi->total = attr->value;
			else if (attr->type == CAMEL_IMAP4_STATUS_UNSEEN)
				fi->unread = attr->value;
			g_free (attr);
			attr = next;
		}
		
		g_free (status->mailbox);
		g_free (status);
	}
	
	camel_imap4_command_unref (ic);
	g_ptr_array_free (array, TRUE);
}

static CamelFolderInfo *
imap4_build_folder_info (CamelStore *store, const char *top, guint32 flags, GPtrArray *array)
{
	CamelIMAP4Engine *engine = ((CamelIMAP4Store *) store)->engine;
	CamelFolder *folder = (CamelFolder *) engine->folder;
	camel_imap4_list_t *list;
	CamelFolderInfo *fi;
	char *name, *p;
	CamelURL *url;
	int i;
	
	if (array->len == 0) {
		g_ptr_array_free (array, TRUE);
		return NULL;
	}
	
	g_ptr_array_sort (array, (GCompareFunc) list_sort);
	
	list_remove_duplicates (array);
	
	url = camel_url_copy (engine->url);
	
	for (i = 0; i < array->len; i++) {
		list = array->pdata[i];
		fi = g_malloc0 (sizeof (CamelFolderInfo));
		
		p = name = camel_utf7_utf8 (list->name);
		while (*p != '\0') {
			if (*p == list->delim)
				*p = '/';
			p++;
		}
		
		p = strrchr (name, '/');
		camel_url_set_fragment (url, name);
		
		fi->full_name = name;
		fi->name = g_strdup (p ? p + 1: name);
		fi->uri = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);
		fi->flags = list->flags;
		fi->unread = -1;
		fi->total = -1;
		
		if (!(flags & CAMEL_STORE_FOLDER_INFO_FAST)) {
			if (folder && !strcmp (folder->full_name, fi->full_name)) {
				/* can't STATUS this folder since it is SELECTED, besides - it would be wasteful */
				CamelMessageInfo *info;
				int index;
				
				fi->total = camel_folder_summary_count (folder->summary);
				
				fi->unread = 0;
				for (index = 0; index < fi->total; index++) {
					if (!(info = camel_folder_summary_index (folder->summary, index)))
						continue;
					
					if ((info->flags & CAMEL_MESSAGE_SEEN) == 0)
						fi->unread++;
					
					camel_folder_summary_info_free (folder->summary, info);
				}
			} else {
				imap4_status (store, fi);
			}
		}
		
		g_free (list->name);
		g_free (list);
		
		array->pdata[i] = fi;
	}
	
	fi = camel_folder_info_build (array, top, '/', TRUE);
	
	camel_url_free (url);
	
	g_ptr_array_free (array, TRUE);
	
	return fi;
}

static CamelFolderInfo *
imap4_get_folder_info (CamelStore *store, const char *top, guint32 flags, CamelException *ex)
{
	CamelIMAP4Engine *engine = ((CamelIMAP4Store *) store)->engine;
	CamelIMAP4Command *ic, *ic0 = NULL;
	CamelFolderInfo *fi = NULL;
	camel_imap4_list_t *list;
	GPtrArray *array;
	const char *cmd;
	char *pattern;
	char wildcard;
	int id, i;
	
	CAMEL_SERVICE_LOCK (store, connect_lock);
	
	if (engine == NULL) {
		if (!camel_service_connect ((CamelService *) store, ex))
			return NULL;
		
		engine = ((CamelIMAP4Store *) store)->engine;
	}
	
	if (flags & CAMEL_STORE_FOLDER_INFO_SUBSCRIBED)
		cmd = "LSUB";
	else
		cmd = "LIST";
	
	if (top == NULL)
		top = "";
	
	wildcard = (flags & CAMEL_STORE_FOLDER_INFO_RECURSIVE) ? '*' : '%';
	pattern = imap4_folder_utf7_name (store, top, wildcard);
	array = g_ptr_array_new ();
	
	if (*top != '\0') {
		size_t len;
		char sep;
		
		len = strlen (pattern);
		sep = pattern[len - 2];
		pattern[len - 2] = '\0';
		
		ic0 = camel_imap4_engine_queue (engine, NULL, "%s \"\" %S\r\n", cmd, pattern);
		camel_imap4_command_register_untagged (ic0, cmd, camel_imap4_untagged_list);
		ic0->user_data = array;
		
		pattern[len - 2] = sep;
	}
	
	ic = camel_imap4_engine_queue (engine, NULL, "%s \"\" %S\r\n", cmd, pattern);
	camel_imap4_command_register_untagged (ic, cmd, camel_imap4_untagged_list);
	ic->user_data = array;
	
	while ((id = camel_imap4_engine_iterate (engine)) < ic->id && id != -1)
		;
	
	if (id == -1 || ic->status != CAMEL_IMAP4_COMMAND_COMPLETE) {
		if (ic0 && ic0->status != CAMEL_IMAP4_COMMAND_COMPLETE)
			camel_exception_xfer (ex, &ic0->ex);
		else
			camel_exception_xfer (ex, &ic->ex);
		
		if (ic0 != NULL)
			camel_imap4_command_unref (ic0);
		camel_imap4_command_unref (ic);
		
		for (i = 0; i < array->len; i++) {
			list = array->pdata[i];
			g_free (list->name);
			g_free (list);
		}
		
		g_ptr_array_free (array, TRUE);
		g_free (pattern);
		
		goto done;
	}
	
	if (ic0 != NULL)
		camel_imap4_command_unref (ic0);
	
	if (ic->result != CAMEL_IMAP4_RESULT_OK) {
		camel_imap4_command_unref (ic);
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot get %s information for pattern `%s' on IMAP server %s: %s"),
				      cmd, pattern, engine->url->host, ic->result == CAMEL_IMAP4_RESULT_BAD ?
				      _("Bad command") : _("Unknown"));
		
		for (i = 0; i < array->len; i++) {
			list = array->pdata[i];
			g_free (list->name);
			g_free (list);
		}
		
		g_ptr_array_free (array, TRUE);
		
		g_free (pattern);
		
		goto done;
	}
	
	g_free (pattern);
	
	fi = imap4_build_folder_info (store, top, flags, array);
	
 done:
	
	CAMEL_SERVICE_UNLOCK (store, connect_lock);
	
	return fi;
}

static void
imap4_subscribe_folder (CamelStore *store, const char *folder_name, CamelException *ex)
{
	CamelIMAP4Engine *engine = ((CamelIMAP4Store *) store)->engine;
	CamelIMAP4Command *ic;
	CamelFolderInfo *fi;
	char *utf7_name;
	CamelURL *url;
	const char *p;
	int id;
	
	CAMEL_SERVICE_LOCK (store, connect_lock);
	
	utf7_name = imap4_folder_utf7_name (store, folder_name, '\0');
	ic = camel_imap4_engine_queue (engine, NULL, "SUBSCRIBE %S\r\n", utf7_name);
	g_free (utf7_name);
	
	while ((id = camel_imap4_engine_iterate (engine)) < ic->id && id != -1)
		;
	
	if (id == -1 || ic->status != CAMEL_IMAP4_COMMAND_COMPLETE) {
		camel_exception_xfer (ex, &ic->ex);
		camel_imap4_command_unref (ic);
		CAMEL_SERVICE_UNLOCK (store, connect_lock);
		return;
	}
	
	switch (ic->result) {
	case CAMEL_IMAP4_RESULT_OK:
		/* subscribed */
		url = camel_url_copy (engine->url);
		camel_url_set_fragment (url, folder_name);
		
		p = strrchr (folder_name, '/');
		
		fi = g_malloc0 (sizeof (CamelFolderInfo));
		fi->full_name = g_strdup (folder_name);
		fi->name = g_strdup (p ? p + 1: folder_name);
		fi->uri = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);
		camel_url_free (url);
		fi->flags = CAMEL_FOLDER_NOCHILDREN;
		fi->unread = -1;
		fi->total = -1;
		
		camel_object_trigger_event (store, "folder_subscribed", fi);
		camel_folder_info_free (fi);
		break;
	case CAMEL_IMAP4_RESULT_NO:
		/* FIXME: would be good to save the NO reason into the err message */
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot subscribe to folder `%s': Invalid mailbox name"),
				      folder_name);
		break;
	case CAMEL_IMAP4_RESULT_BAD:
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot subscribe to folder `%s': Bad command"),
				      folder_name);
		break;
	}
	
	camel_imap4_command_unref (ic);
	
	CAMEL_SERVICE_UNLOCK (store, connect_lock);
}

static void
imap4_unsubscribe_folder (CamelStore *store, const char *folder_name, CamelException *ex)
{
	CamelIMAP4Engine *engine = ((CamelIMAP4Store *) store)->engine;
	CamelIMAP4Command *ic;
	CamelFolderInfo *fi;
	char *utf7_name;
	CamelURL *url;
	const char *p;
	int id;
	
	CAMEL_SERVICE_LOCK (store, connect_lock);
	
	utf7_name = imap4_folder_utf7_name (store, folder_name, '\0');
	ic = camel_imap4_engine_queue (engine, NULL, "UNSUBSCRIBE %S\r\n", utf7_name);
	g_free (utf7_name);
	
	while ((id = camel_imap4_engine_iterate (engine)) < ic->id && id != -1)
		;
	
	if (id == -1 || ic->status != CAMEL_IMAP4_COMMAND_COMPLETE) {
		camel_exception_xfer (ex, &ic->ex);
		camel_imap4_command_unref (ic);
		CAMEL_SERVICE_UNLOCK (store, connect_lock);
		return;
	}
	
	switch (ic->result) {
	case CAMEL_IMAP4_RESULT_OK:
		/* unsubscribed */
		url = camel_url_copy (engine->url);
		camel_url_set_fragment (url, folder_name);
		
		p = strrchr (folder_name, '/');
		
		fi = g_malloc0 (sizeof (CamelFolderInfo));
		fi->full_name = g_strdup (folder_name);
		fi->name = g_strdup (p ? p + 1: folder_name);
		fi->uri = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);
		camel_url_free (url);
		fi->flags = 0;
		fi->unread = -1;
		fi->total = -1;
		
		camel_object_trigger_event (store, "folder_unsubscribed", fi);
		camel_folder_info_free (fi);
		break;
	case CAMEL_IMAP4_RESULT_NO:
		/* FIXME: would be good to save the NO reason into the err message */
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot unsubscribe from folder `%s': Invalid mailbox name"),
				      folder_name);
		break;
	case CAMEL_IMAP4_RESULT_BAD:
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot unsubscribe from folder `%s': Bad command"),
				      folder_name);
		break;
	}
	
	camel_imap4_command_unref (ic);
	
	CAMEL_SERVICE_UNLOCK (store, connect_lock);
}

static void
imap4_noop (CamelStore *store, CamelException *ex)
{
	CamelIMAP4Engine *engine = ((CamelIMAP4Store *) store)->engine;
	CamelFolder *folder = (CamelFolder *) engine->folder;
	CamelIMAP4Command *ic;
	int id;
	
	CAMEL_SERVICE_LOCK (store, connect_lock);
	
	if (folder) {
		camel_folder_sync (folder, FALSE, ex);
		if (camel_exception_is_set (ex)) {
			CAMEL_SERVICE_UNLOCK (store, connect_lock);
			return;
		}
	}
	
	ic = camel_imap4_engine_queue (engine, NULL, "NOOP\r\n");
	while ((id = camel_imap4_engine_iterate (engine)) < ic->id && id != -1)
		;
	
	if (id == -1 || ic->status != CAMEL_IMAP4_COMMAND_COMPLETE)
		camel_exception_xfer (ex, &ic->ex);
	
	camel_imap4_command_unref (ic);
	
	if (folder && !camel_exception_is_set (ex))
		camel_imap4_summary_flush_updates (folder->summary, ex);
	
	CAMEL_SERVICE_UNLOCK (store, connect_lock);
}
