/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-pop3-store.c : class for a pop3 store */

/* 
 * Authors:
 *   Dan Winship <danw@ximian.com>
 *   Michael Zucchi <notzed@ximian.com>
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

#include "camel-operation.h"

#include "camel-pop3-store.h"
#include "camel-pop3-folder.h"
#include "camel-stream-buffer.h"
#include "camel-session.h"
#include "camel-exception.h"
#include "camel-url.h"
#include "e-util/md5-utils.h"
#include "camel-pop3-engine.h"
#include "camel-sasl.h"
#include "camel-data-cache.h"
#include "camel-tcp-stream.h"
#include "camel-tcp-stream-raw.h"
#ifdef HAVE_SSL
#include "camel-tcp-stream-ssl.h"
#endif
#include "camel-i18n.h"

/* Specified in RFC 1939 */
#define POP3_PORT "110"
#define POP3S_PORT "995"

static CamelStoreClass *parent_class = NULL;

static void finalize (CamelObject *object);

static gboolean pop3_connect (CamelService *service, CamelException *ex);
static gboolean pop3_disconnect (CamelService *service, gboolean clean, CamelException *ex);
static GList *query_auth_types (CamelService *service, CamelException *ex);

static CamelFolder *get_folder (CamelStore *store, const char *folder_name, 
				guint32 flags, CamelException *ex);

static CamelFolder *get_trash  (CamelStore *store, CamelException *ex);

static void
camel_pop3_store_class_init (CamelPOP3StoreClass *camel_pop3_store_class)
{
	CamelServiceClass *camel_service_class =
		CAMEL_SERVICE_CLASS (camel_pop3_store_class);
	CamelStoreClass *camel_store_class =
		CAMEL_STORE_CLASS (camel_pop3_store_class);

	parent_class = CAMEL_STORE_CLASS (camel_type_get_global_classfuncs (camel_store_get_type ()));
	
	/* virtual method overload */
	camel_service_class->query_auth_types = query_auth_types;
	camel_service_class->connect = pop3_connect;
	camel_service_class->disconnect = pop3_disconnect;

	camel_store_class->get_folder = get_folder;
	camel_store_class->get_trash = get_trash;
}



static void
camel_pop3_store_init (gpointer object, gpointer klass)
{
	;
}

CamelType
camel_pop3_store_get_type (void)
{
	static CamelType camel_pop3_store_type = CAMEL_INVALID_TYPE;

	if (!camel_pop3_store_type) {
		camel_pop3_store_type = camel_type_register (CAMEL_STORE_TYPE,
							     "CamelPOP3Store",
							     sizeof (CamelPOP3Store),
							     sizeof (CamelPOP3StoreClass),
							     (CamelObjectClassInitFunc) camel_pop3_store_class_init,
							     NULL,
							     (CamelObjectInitFunc) camel_pop3_store_init,
							     finalize);
	}

	return camel_pop3_store_type;
}

static void
finalize (CamelObject *object)
{
	CamelPOP3Store *pop3_store = CAMEL_POP3_STORE (object);

	/* force disconnect so we dont have it run later, after we've cleaned up some stuff */
	/* SIGH */

	camel_service_disconnect((CamelService *)pop3_store, TRUE, NULL);

	if (pop3_store->engine)
		camel_object_unref((CamelObject *)pop3_store->engine);
	if (pop3_store->cache)
		camel_object_unref((CamelObject *)pop3_store->cache);
}

enum {
	MODE_CLEAR,
	MODE_SSL,
	MODE_TLS,
};

#define SSL_PORT_FLAGS (CAMEL_TCP_STREAM_SSL_ENABLE_SSL2 | CAMEL_TCP_STREAM_SSL_ENABLE_SSL3)
#define STARTTLS_FLAGS (CAMEL_TCP_STREAM_SSL_ENABLE_TLS)

static gboolean
connect_to_server (CamelService *service, struct addrinfo *ai, int ssl_mode, CamelException *ex)
{
	CamelPOP3Store *store = CAMEL_POP3_STORE (service);
	CamelStream *tcp_stream;
	CamelPOP3Command *pc;
	guint32 flags = 0;
	int clean_quit;
	int ret;
	
	if (ssl_mode != MODE_CLEAR) {
#ifdef HAVE_SSL
		if (ssl_mode == MODE_TLS) {
			tcp_stream = camel_tcp_stream_ssl_new (service->session, service->url->host, STARTTLS_FLAGS);
		} else {
			tcp_stream = camel_tcp_stream_ssl_new (service->session, service->url->host, SSL_PORT_FLAGS);
		}
#else
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      _("Could not connect to %s: %s"),
				      service->url->host, _("SSL unavailable"));
		
		return FALSE;
#endif /* HAVE_SSL */
	} else {
		tcp_stream = camel_tcp_stream_raw_new ();
	}
	
	if ((ret = camel_tcp_stream_connect ((CamelTcpStream *) tcp_stream, ai)) == -1) {
		if (errno == EINTR)
			camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL,
					     _("Connection cancelled"));
		else
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					      _("Could not connect to %s: %s"),
					      service->url->host,
					      g_strerror (errno));
		
		camel_object_unref (tcp_stream);
		
		return FALSE;
	}
	
	/* parent class connect initialization */
	if (CAMEL_SERVICE_CLASS (parent_class)->connect (service, ex) == FALSE) {
		camel_object_unref (tcp_stream);
		return FALSE;
	}
	
	if (camel_url_get_param (service->url, "disable_extensions"))
		flags |= CAMEL_POP3_ENGINE_DISABLE_EXTENSIONS;
	
	if (!(store->engine = camel_pop3_engine_new (tcp_stream, flags))) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to read a valid greeting from POP server %s"),
				      service->url->host);
		
		return FALSE;
	}
	
	if (ssl_mode != MODE_TLS) {
		camel_object_unref (tcp_stream);
		return TRUE;
	}
	
	if (!(store->engine->capa & CAMEL_POP3_CAP_STLS)) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to connect to POP server %s in secure mode: %s"),
				      service->url->host, _("STLS not supported"));
		goto stls_exception;
	}
	
	/* as soon as we send a STLS command, all hope is lost of a clean QUIT if problems arise */
	clean_quit = FALSE;
	
	pc = camel_pop3_engine_command_new (store->engine, 0, NULL, NULL, "STLS\r\n");
	while (camel_pop3_engine_iterate (store->engine, NULL) > 0)
		;
	
	ret = pc->state == CAMEL_POP3_COMMAND_OK;
	camel_pop3_engine_command_free (store->engine, pc);
	
	if (ret == FALSE) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to connect to POP server %s in secure mode: %s"),
				      service->url->host, store->engine->line);
		goto stls_exception;
	}
	
	/* Okay, now toggle SSL/TLS mode */
	ret = camel_tcp_stream_ssl_enable_ssl (CAMEL_TCP_STREAM_SSL (tcp_stream));
	
	camel_object_unref (CAMEL_OBJECT (tcp_stream));
	
	if (ret == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to connect to POP server %s in secure mode: %s"),
				      service->url->host, _("SSL negotiations failed"));
		goto stls_exception;
	}
	
	/* rfc2595, section 4 states that after a successful STLS
           command, the client MUST discard prior CAPA responses */
	camel_pop3_engine_reget_capabilities (store->engine);
	
	return TRUE;
	
 stls_exception:
	if (clean_quit) {
		/* try to disconnect cleanly */
		pc = camel_pop3_engine_command_new (store->engine, 0, NULL, NULL, "QUIT\r\n");
		while (camel_pop3_engine_iterate (store->engine, NULL) > 0)
			;
		camel_pop3_engine_command_free (store->engine, pc);
	}
	
	camel_object_unref (CAMEL_OBJECT (store->engine));
	camel_object_unref (CAMEL_OBJECT (tcp_stream));
	store->engine = NULL;
	
	return FALSE;
}

static struct {
	char *value;
	char *serv;
	char *port;
	int mode;
} ssl_options[] = {
	{ "",              "pop3s", POP3S_PORT, MODE_SSL   },  /* really old (1.x) */
	{ "always",        "pop3s", POP3S_PORT, MODE_SSL   },
	{ "when-possible", "pop3",  POP3S_PORT, MODE_TLS   },
	{ "never",         "pop3",  POP3S_PORT, MODE_CLEAR },
	{ NULL,            "pop3",  POP3S_PORT, MODE_CLEAR },
};

static gboolean
connect_to_server_wrapper (CamelService *service, CamelException *ex)
{
	struct addrinfo hints, *ai;
	const char *ssl_mode;
	int mode, ret, i;
	char *serv;
	const char *port;

	if ((ssl_mode = camel_url_get_param (service->url, "use_ssl"))) {
		for (i = 0; ssl_options[i].value; i++)
			if (!strcmp (ssl_options[i].value, ssl_mode))
				break;
		mode = ssl_options[i].mode;
		serv = ssl_options[i].serv;
		port = ssl_options[i].port;
	} else {
		mode = MODE_CLEAR;
		serv = "pop3";
		port = POP3S_PORT;
	}
	
	if (service->url->port) {
		serv = g_alloca (16);
		sprintf (serv, "%d", service->url->port);
		port = NULL;
	}
	
	memset (&hints, 0, sizeof (hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = PF_UNSPEC;
	ai = camel_getaddrinfo(service->url->host, serv, &hints, ex);
	if (ai == NULL && port != NULL && camel_exception_get_id(ex) != CAMEL_EXCEPTION_USER_CANCEL) {
		camel_exception_clear (ex);
		ai = camel_getaddrinfo(service->url->host, port, &hints, ex);
	}
	if (ai == NULL)
		return FALSE;
	
	ret = connect_to_server (service, ai, mode, ex);
	
	camel_freeaddrinfo (ai);
	
	return ret;
}

extern CamelServiceAuthType camel_pop3_password_authtype;
extern CamelServiceAuthType camel_pop3_apop_authtype;

static GList *
query_auth_types (CamelService *service, CamelException *ex)
{
	CamelPOP3Store *store = CAMEL_POP3_STORE (service);
	GList *types = NULL;

        types = CAMEL_SERVICE_CLASS (parent_class)->query_auth_types (service, ex);
	if (camel_exception_is_set (ex))
		return NULL;

	if (connect_to_server_wrapper (service, NULL)) {
		types = g_list_concat(types, g_list_copy(store->engine->auth));
		pop3_disconnect (service, TRUE, NULL);
	} else {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      _("Could not connect to POP server %s"),
				      service->url->host);
	}

	return types;
}

/**
 * camel_pop3_store_expunge:
 * @store: the store
 * @ex: a CamelException
 *
 * Expunge messages from the store. This will result in the connection
 * being closed, which may cause later commands to fail if they can't
 * reconnect.
 **/
void
camel_pop3_store_expunge (CamelPOP3Store *store, CamelException *ex)
{
	CamelPOP3Command *pc;

	pc = camel_pop3_engine_command_new(store->engine, 0, NULL, NULL, "QUIT\r\n");
	while (camel_pop3_engine_iterate(store->engine, NULL) > 0)
		;
	camel_pop3_engine_command_free(store->engine, pc);

	camel_service_disconnect (CAMEL_SERVICE (store), FALSE, ex);
}

static int
try_sasl(CamelPOP3Store *store, const char *mech, CamelException *ex)
{
	CamelPOP3Stream *stream = store->engine->stream;
	unsigned char *line, *resp;
	CamelSasl *sasl;
	unsigned int len;
	int ret;

	sasl = camel_sasl_new("pop3", mech, (CamelService *)store);
	if (sasl == NULL) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_URL_INVALID,
				      _("Unable to connect to POP server %s: "
					"No support for requested authentication mechanism."),
				      CAMEL_SERVICE (store)->url->host);
		return -1;
	}

	if (camel_stream_printf((CamelStream *)stream, "AUTH %s\r\n", mech) == -1)
		goto ioerror;

	while (1) {
		if (camel_pop3_stream_line(stream, &line, &len) == -1)
			goto ioerror;
		if (strncmp(line, "+OK", 3) == 0)
			break;
		if (strncmp(line, "-ERR", 4) == 0) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
					      _("SASL `%s' Login failed for POP server %s: %s"),
					      mech, CAMEL_SERVICE (store)->url->host, line);
			goto done;
		}
		/* If we dont get continuation, or the sasl object's run out of work, or we dont get a challenge,
		   its a protocol error, so fail, and try reset the server */
		if (strncmp(line, "+ ", 2) != 0
		    || camel_sasl_authenticated(sasl)
		    || (resp = camel_sasl_challenge_base64(sasl, line+2, ex)) == NULL) {
			camel_stream_printf((CamelStream *)stream, "*\r\n");
			camel_pop3_stream_line(stream, &line, &len);
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
					      _("Cannot login to POP server %s: SASL Protocol error"),
					      CAMEL_SERVICE (store)->url->host);
			goto done;
		}

		ret = camel_stream_printf((CamelStream *)stream, "%s\r\n", resp);
		g_free(resp);
		if (ret == -1)
			goto ioerror;

	}
	camel_object_unref((CamelObject *)sasl);
	return 0;
	
 ioerror:
	if (errno == EINTR) {
		camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL, _("Cancelled"));
	} else {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to authenticate on POP server %s: %s"),
				      CAMEL_SERVICE (store)->url->host, g_strerror (errno));
	}
 done:
	camel_object_unref((CamelObject *)sasl);
	return -1;
}

static int
pop3_try_authenticate (CamelService *service, gboolean reprompt, const char *errmsg, CamelException *ex)
{
	CamelPOP3Store *store = (CamelPOP3Store *)service;
	CamelPOP3Command *pcu = NULL, *pcp = NULL;
	int status;
	
	/* override, testing only */
	/*printf("Forcing authmech to 'login'\n");
	service->url->authmech = g_strdup("LOGIN");*/
	
	if (!service->url->passwd) {
		char *prompt;
		guint32 flags = CAMEL_SESSION_PASSWORD_SECRET;
		
		if (reprompt)
			flags |= CAMEL_SESSION_PASSWORD_REPROMPT;
		
		prompt = g_strdup_printf (_("%sPlease enter the POP password for %s on host %s"),
					  errmsg ? errmsg : "",
					  service->url->user,
					  service->url->host);
		service->url->passwd = camel_session_get_password (camel_service_get_session (service), service, NULL,
								   prompt, "password", flags, ex);
		g_free (prompt);
		if (!service->url->passwd)
			return FALSE;
	}

	if (!service->url->authmech) {
		/* pop engine will take care of pipelining ability */
		pcu = camel_pop3_engine_command_new(store->engine, 0, NULL, NULL, "USER %s\r\n", service->url->user);
		pcp = camel_pop3_engine_command_new(store->engine, 0, NULL, NULL, "PASS %s\r\n", service->url->passwd);
	} else if (strcmp(service->url->authmech, "+APOP") == 0 && store->engine->apop) {
		char *secret, md5asc[33], *d;
		unsigned char md5sum[16], *s;
		
		secret = g_alloca(strlen(store->engine->apop)+strlen(service->url->passwd)+1);
		sprintf(secret, "%s%s",  store->engine->apop, service->url->passwd);
		md5_get_digest(secret, strlen (secret), md5sum);

		for (s = md5sum, d = md5asc; d < md5asc + 32; s++, d += 2)
			sprintf (d, "%.2x", *s);
		
		pcp = camel_pop3_engine_command_new(store->engine, 0, NULL, NULL, "APOP %s %s\r\n",
						    service->url->user, md5asc);
	} else {
		CamelServiceAuthType *auth;
		GList *l;

		l = store->engine->auth;
		while (l) {
			auth = l->data;
			if (strcmp(auth->authproto, service->url->authmech) == 0)
				return try_sasl(store, service->url->authmech, ex) == -1;
			l = l->next;
		}
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_URL_INVALID,
				      _("Unable to connect to POP server %s: "
					"No support for requested authentication mechanism."),
				      CAMEL_SERVICE (store)->url->host);
		return FALSE;
	}
	
	while ((status = camel_pop3_engine_iterate(store->engine, pcp)) > 0)
		;
	
	if (status == -1) {
		if (errno == EINTR) {
			camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL, _("Cancelled"));
		} else {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Unable to connect to POP server %s.\n"
						"Error sending password: %s"),
					      CAMEL_SERVICE (store)->url->host,
					      errno ? g_strerror (errno) : _("Unknown error"));
		}
	} else if (pcu && pcu->state != CAMEL_POP3_COMMAND_OK) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
				      _("Unable to connect to POP server %s.\n"
					"Error sending username: %s"),
				      CAMEL_SERVICE (store)->url->host,
				      store->engine->line ? (char *)store->engine->line : _("Unknown error"));
	} else if (pcp->state != CAMEL_POP3_COMMAND_OK)
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
				      _("Unable to connect to POP server %s.\n"
					"Error sending password: %s"),
				      CAMEL_SERVICE (store)->url->host,
				      store->engine->line ? (char *)store->engine->line : _("Unknown error"));
	
	camel_pop3_engine_command_free(store->engine, pcp);
	
	if (pcu)
		camel_pop3_engine_command_free(store->engine, pcu);
	
	return status;
}

static gboolean
pop3_connect (CamelService *service, CamelException *ex)
{
	CamelPOP3Store *store = (CamelPOP3Store *)service;
	gboolean reprompt = FALSE;
	CamelSession *session;
	char *errbuf = NULL;
	int status;
	
	session = camel_service_get_session (service);
	
	if (store->cache == NULL) {
		char *root;

		root = camel_session_get_storage_path (session, service, ex);
		if (root) {
			store->cache = camel_data_cache_new(root, 0, ex);
			g_free(root);
			if (store->cache) {
				/* Default cache expiry - 1 week or not visited in a day */
				camel_data_cache_set_expire_age(store->cache, 60*60*24*7);
				camel_data_cache_set_expire_access(store->cache, 60*60*24);
			}
		}
	}
	
	if (!connect_to_server_wrapper (service, ex))
		return FALSE;
	
	do {
		status = pop3_try_authenticate (service, reprompt, errbuf, ex);
		g_free (errbuf);
		errbuf = NULL;
		
		/* we only re-prompt if we failed to authenticate, any other error and we just abort */
		if (camel_exception_get_id (ex) == CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE) {
			errbuf = g_strdup_printf ("%s\n\n", camel_exception_get_description (ex));
			g_free (service->url->passwd);
			service->url->passwd = NULL;
			reprompt = TRUE;
			camel_exception_clear (ex);
		}
	} while (status != -1 && ex->id == CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE);
	
	g_free (errbuf);
	
	if (status == -1 || camel_exception_is_set(ex)) {
		camel_service_disconnect(service, TRUE, ex);
		return FALSE;
	}
	
	/* Now that we are in the TRANSACTION state, try regetting the capabilities */
	store->engine->state = CAMEL_POP3_ENGINE_TRANSACTION;
	camel_pop3_engine_reget_capabilities (store->engine);
	
	return TRUE;
}

static gboolean
pop3_disconnect (CamelService *service, gboolean clean, CamelException *ex)
{
	CamelPOP3Store *store = CAMEL_POP3_STORE (service);
	
	if (clean) {
		CamelPOP3Command *pc;
		
		pc = camel_pop3_engine_command_new(store->engine, 0, NULL, NULL, "QUIT\r\n");
		while (camel_pop3_engine_iterate(store->engine, NULL) > 0)
			;
		camel_pop3_engine_command_free(store->engine, pc);
	}
	
	if (!CAMEL_SERVICE_CLASS (parent_class)->disconnect (service, clean, ex))
		return FALSE;
	
	camel_object_unref((CamelObject *)store->engine);
	store->engine = NULL;
	
	return TRUE;
}

static CamelFolder *
get_folder (CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex)
{
	if (strcasecmp (folder_name, "inbox") != 0) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_FOLDER_INVALID,
				      _("No such folder `%s'."), folder_name);
		return NULL;
	}
	return camel_pop3_folder_new (store, ex);
}

static CamelFolder *
get_trash (CamelStore *store, CamelException *ex)
{
	/* no-op */
	return NULL;
}
