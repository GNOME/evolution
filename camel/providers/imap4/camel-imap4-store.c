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
static void imap4_sync (CamelStore *store, gboolean expunge, CamelException *ex);
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
		type = camel_type_register (CAMEL_TYPE_IMAP4_STORE,
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

static gint
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
	store_class->sync = imap4_sync;
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
}


static void
imap4_construct (CamelService *service, CamelSession *session, CamelProvider *provider, CamelURL *url, CamelException *ex)
{
	CAMEL_SERVICE_CLASS (parent_class)->construct (service, session, provider, url, ex);
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
		ret = connect_to_server (service, h, USE_SSL_ALWAYS, FALSE, ex);
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
		
		service->url->passwd = camel_session_get_password (session, prompt, flags, service, "password", ex);
		
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


static char *
imap4_folder_utf7_name (CamelStore *store, const char *folder_name)
{
	char *real_name, *p;
	
	if (store->dir_sep != '/') {
		p = real_name = g_alloca (strlen (folder_name) + 1);
		strcpy (real_name, folder_name);
		while (*p != '\0') {
			if (*p == '/')
				*p = store->dir_sep;
			p++;
		}
		
		folder_name = real_name;
	}
	
	return camel_utf8_utf7 (folder_name);
}

static CamelFolder *
imap4_get_folder (CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex)
{
	/* FIXME: implement me */
	
	return NULL;
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
	const char *c;
	char *name;
	int id;
	
	c = folder_name;
	while (*c != '\0') {
		if (*c == store->dir_sep || strchr ("#%*", *c)) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_FOLDER_INVALID_PATH,
					      _("The folder name \"%s\" is invalid because "
						"it containes the character \"%c\""),
					      folder_name, *c);
			return NULL;
		}
		
		c++;
	}
	
	if (parent_name != NULL && *parent_name)
		name = g_strdup_printf ("%s/%s", parent_name, folder_name);
	else
		name = g_strdup (folder_name);
	
	utf7_name = imap4_folder_utf7_name (store, name);
	g_free (name);
	
	ic = camel_imap4_engine_queue (engine, NULL, "CREATE %S\r\n", utf7_name);
	g_free (utf7_name);
	
	while ((id = camel_imap4_engine_iterate (engine)) < ic->id && id != -1)
		;
	
	if (id == -1 || ic->status != CAMEL_IMAP4_COMMAND_COMPLETE) {
		camel_exception_xfer (ex, &ic->ex);
		camel_imap4_command_unref (ic);
		return NULL;
	}
	
	switch (ic->result) {
	case CAMEL_IMAP4_RESULT_OK:
		/* FIXME: allocate fi */
		break;
	case CAMEL_IMAP4_RESULT_NO:
		/* FIXME: would be good to save the NO reason into the err message */
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot create folder `%s': Invalid mailbox name"),
				      folder_name);
		break;
	case CAMEL_IMAP4_RESULT_BAD:
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot create folder `%s': Bad command"),
				      folder_name);
		break;
	default:
		g_assert_not_reached ();
	}
	
	camel_imap4_command_unref (ic);
	
	return fi;
}

static void
imap4_delete_folder (CamelStore *store, const char *folder_name, CamelException *ex)
{
	CamelIMAP4Engine *engine = ((CamelIMAP4Store *) store)->engine;
	CamelIMAP4Command *ic;
	char *utf7_name;
	int id;
	
	if (!g_ascii_strcasecmp (folder_name, "INBOX")) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot delete folder `%s': Special folder"),
				      folder_name);
		
		return;
	}
	
	utf7_name = imap4_folder_utf7_name (store, folder_name);
	ic = camel_imap4_engine_queue (engine, NULL, "DELETE %S\r\n", utf7_name);
	g_free (utf7_name);
	
	while ((id = camel_imap4_engine_iterate (engine)) < ic->id && id != -1)
		;
	
	if (id == -1 || ic->status != CAMEL_IMAP4_COMMAND_COMPLETE) {
		camel_exception_xfer (ex, &ic->ex);
		camel_imap4_command_unref (ic);
		return;
	}
	
	switch (ic->result) {
	case CAMEL_IMAP4_RESULT_OK:
		/* deleted */
		/*fi = imap4_build_folder_info (store, folder_name);
		camel_object_trigger_event (store, "folder_deleted", fi);
		camel_folder_info_free (fi);*/
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
}

static void
imap4_rename_folder (CamelStore *store, const char *old_name, const char *new_name, CamelException *ex)
{

}

static void
imap4_sync (CamelStore *store, gboolean expunge, CamelException *ex)
{

}

static CamelFolderInfo *
imap4_get_folder_info (CamelStore *store, const char *top, guint32 flags, CamelException *ex)
{
	return NULL;
}

static void
imap4_subscribe_folder (CamelStore *store, const char *folder_name, CamelException *ex)
{
	CamelIMAP4Engine *engine = ((CamelIMAP4Store *) store)->engine;
	CamelIMAP4Command *ic;
	char *utf7_name;
	int id;
	
	utf7_name = imap4_folder_utf7_name (store, folder_name);
	ic = camel_imap4_engine_queue (engine, NULL, "SUBSCRIBE %S\r\n", utf7_name);
	g_free (utf7_name);
	
	while ((id = camel_imap4_engine_iterate (engine)) < ic->id && id != -1)
		;
	
	if (id == -1 || ic->status != CAMEL_IMAP4_COMMAND_COMPLETE) {
		camel_exception_xfer (ex, &ic->ex);
		camel_imap4_command_unref (ic);
		return;
	}
	
	switch (ic->result) {
	case CAMEL_IMAP4_RESULT_OK:
		/* subscribed */
		/*fi = imap4_build_folder_info (store, folder_name);
		  fi->flags |= CAMEL_FOLDER_NOCHILDREN;
		  camel_object_trigger_event (store, "folder_subscribed", fi);
		  camel_folder_info_free (fi);*/
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
}

static void
imap4_unsubscribe_folder (CamelStore *store, const char *folder_name, CamelException *ex)
{
	CamelIMAP4Engine *engine = ((CamelIMAP4Store *) store)->engine;
	CamelIMAP4Command *ic;
	char *utf7_name;
	int id;
	
	utf7_name = imap4_folder_utf7_name (store, folder_name);
	ic = camel_imap4_engine_queue (engine, NULL, "UNSUBSCRIBE %S\r\n", utf7_name);
	g_free (utf7_name);
	
	while ((id = camel_imap4_engine_iterate (engine)) < ic->id && id != -1)
		;
	
	if (id == -1 || ic->status != CAMEL_IMAP4_COMMAND_COMPLETE) {
		camel_exception_xfer (ex, &ic->ex);
		camel_imap4_command_unref (ic);
		return;
	}
	
	switch (ic->result) {
	case CAMEL_IMAP4_RESULT_OK:
		/* unsubscribed */
		/*fi = imap4_build_folder_info (store, folder_name);
		  camel_object_trigger_event (store, "folder_unsubscribed", fi);
		  camel_folder_info_free (fi);*/
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
}

static void
imap4_noop (CamelStore *store, CamelException *ex)
{
	CamelIMAP4Engine *engine = ((CamelIMAP4Store *) store)->engine;
	CamelIMAP4Command *ic;
	int id;
	
	ic = camel_imap4_engine_queue (engine, NULL, "NOOP\r\n");
	while ((id = camel_imap4_engine_iterate (engine)) < ic->id && id != -1)
		;
	
	if (id == -1 || ic->status != CAMEL_IMAP4_COMMAND_COMPLETE)
		camel_exception_xfer (ex, &ic->ex);
	
	camel_imap4_command_unref (ic);
}
