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
#include "camel-tcp-stream.h"
#include "camel-session.h"
#include "camel-exception.h"
#include "camel-url.h"
#include "e-util/md5-utils.h"
#include "camel-pop3-engine.h"
#include "camel-sasl.h"
#include "camel-data-cache.h"

/* Specified in RFC 1939 */
#define POP3_PORT 110

static CamelRemoteStoreClass *parent_class = NULL;

static void finalize (CamelObject *object);

static gboolean pop3_connect (CamelService *service, CamelException *ex);
static gboolean pop3_disconnect (CamelService *service, gboolean clean, CamelException *ex);
static GList *query_auth_types (CamelService *service, CamelException *ex);

static CamelFolder *get_folder (CamelStore *store, const char *folder_name, 
				guint32 flags, CamelException *ex);

static void init_trash (CamelStore *store);
static CamelFolder *get_trash  (CamelStore *store, CamelException *ex);

static void
camel_pop3_store_class_init (CamelPOP3StoreClass *camel_pop3_store_class)
{
	CamelServiceClass *camel_service_class =
		CAMEL_SERVICE_CLASS (camel_pop3_store_class);
	CamelStoreClass *camel_store_class =
		CAMEL_STORE_CLASS (camel_pop3_store_class);

	parent_class = CAMEL_REMOTE_STORE_CLASS(camel_type_get_global_classfuncs 
						(camel_remote_store_get_type ()));

	/* virtual method overload */
	camel_service_class->query_auth_types = query_auth_types;
	camel_service_class->connect = pop3_connect;
	camel_service_class->disconnect = pop3_disconnect;

	camel_store_class->get_folder = get_folder;
	camel_store_class->init_trash = init_trash;
	camel_store_class->get_trash = get_trash;
}



static void
camel_pop3_store_init (gpointer object, gpointer klass)
{
	CamelRemoteStore *remote_store = CAMEL_REMOTE_STORE (object);

	remote_store->default_port = POP3_PORT;
	/* FIXME: what should this port be?? */
	remote_store->default_ssl_port = 995;
}

CamelType
camel_pop3_store_get_type (void)
{
	static CamelType camel_pop3_store_type = CAMEL_INVALID_TYPE;

	if (!camel_pop3_store_type) {
		camel_pop3_store_type = camel_type_register (CAMEL_REMOTE_STORE_TYPE, "CamelPOP3Store",
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

static gboolean
connect_to_server (CamelService *service, CamelException *ex)
{
	CamelPOP3Store *store = CAMEL_POP3_STORE (service);
	gboolean result;

  	result = CAMEL_SERVICE_CLASS (parent_class)->connect (service, ex);

	if (result == FALSE)
		return FALSE;

	store->engine = camel_pop3_engine_new(CAMEL_REMOTE_STORE(store)->ostream);

	return store->engine != NULL;
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
		return types;

	if (connect_to_server (service, NULL)) {
		types = g_list_concat(types, g_list_copy(store->engine->auth));
		pop3_disconnect (service, TRUE, NULL);
	} else {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      _("Could not connect to POP server on "
					"%s."), service->url->host);
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
		camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
				     _("Unable to connect to POP server.\n"
				       "No support for requested "
				       "authentication mechanism."));
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
			camel_exception_setv(ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
					     _("SASL `%s' Login failed: %s"), mech, line);
			goto done;
		}
		/* If we dont get continuation, or the sasl object's run out of work, or we dont get a challenge,
		   its a protocol error, so fail, and try reset the server */
		if (strncmp(line, "+ ", 2) != 0
		    || camel_sasl_authenticated(sasl)
		    || (resp = camel_sasl_challenge_base64(sasl, line+2, ex)) == NULL) {
			camel_stream_printf((CamelStream *)stream, "*\r\n");
			camel_pop3_stream_line(stream, &line, &len);
			camel_exception_setv(ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
					     _("SASL Protocol error"));
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
	camel_exception_setv(ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
			     _("I/O Error: %s"), strerror(errno));
done:
	camel_object_unref((CamelObject *)sasl);
	return -1;
}

static gboolean
pop3_try_authenticate (CamelService *service, const char *errmsg,
		       CamelException *ex)
{
	CamelPOP3Store *store = (CamelPOP3Store *)service;
	int status;
	CamelPOP3Command *pcu = NULL, *pcp = NULL;

	/* override, testing only */
	/*printf("Forcing authmech to 'login'\n");
	service->url->authmech = g_strdup("LOGIN");*/

	if (!service->url->passwd) {
		char *prompt;
		
		prompt = g_strdup_printf (_("%sPlease enter the POP password for %s@%s"),
					  errmsg ? errmsg : "",
					  service->url->user,
					  service->url->host);
		service->url->passwd = camel_session_get_password (camel_service_get_session (service),
								   prompt, TRUE, service, "password", ex);
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
		
		secret = alloca(strlen(store->engine->apop)+strlen(service->url->passwd)+1);
		sprintf(secret, "%s%s",  store->engine->apop, service->url->passwd);
		md5_get_digest(secret, strlen (secret), md5sum);

		for (s = md5sum, d = md5asc; d < md5asc + 32; s++, d += 2)
			sprintf (d, "%.2x", *s);
		
		pcp = camel_pop3_engine_command_new(store->engine, 0, NULL, NULL, "APOP %s %s\r\n", service->url->user, md5asc);
	} else {
		CamelServiceAuthType *auth;
		GList *l;

		l = store->engine->auth;
		while (l) {
			auth = l->data;
			if (strcmp(auth->authproto, service->url->authmech) == 0) {
				return try_sasl(store, service->url->authmech, ex) == -1;
			}
		}

		camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
				     _("Unable to connect to POP server.\n"
				       "No support for requested "
				       "authentication mechanism."));
		return FALSE;
	}

	while (camel_pop3_engine_iterate(store->engine, pcp) > 0)
		;
	status = pcp->state != CAMEL_POP3_COMMAND_OK;
	if (status) {
		camel_exception_setv(ex,  CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
				     _("Unable to connect to POP server.\nError sending password: %s"),
				     store->engine->line);
	}
	camel_pop3_engine_command_free(store->engine, pcp);

	if (pcu)
		camel_pop3_engine_command_free(store->engine, pcu);

	return status;
}

static gboolean
pop3_connect (CamelService *service, CamelException *ex)
{
	char *errbuf = NULL;
	gboolean tryagain;
	CamelPOP3Store *store = (CamelPOP3Store *)service;

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
	}
	
	if (!connect_to_server (service, ex))
		return FALSE;
	
	camel_exception_clear (ex);
	do {
		if (camel_exception_is_set (ex)) {
			errbuf = g_strdup_printf ("%s\n\n", camel_exception_get_description (ex));
			camel_exception_clear (ex);
			
			/* Uncache the password before prompting again. */
			camel_session_forget_password (camel_service_get_session (service),
						       service, "password", ex);
			g_free (service->url->passwd);
			service->url->passwd = NULL;
		}
		
		tryagain = pop3_try_authenticate (service, errbuf, ex);
		g_free (errbuf);
		errbuf = NULL;
	} while (tryagain);
	
	if (camel_exception_is_set (ex)) {
		camel_service_disconnect (service, TRUE, ex);
		return FALSE;
	}
	
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

	camel_object_unref((CamelObject *)store->engine);
	store->engine = NULL;
	
	if (!CAMEL_SERVICE_CLASS (parent_class)->disconnect (service, clean, ex))
		return FALSE;
	
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

static void
init_trash (CamelStore *store)
{
	/* no-op */
	;
}

static CamelFolder *
get_trash (CamelStore *store, CamelException *ex)
{
	/* no-op */
	return NULL;
}
