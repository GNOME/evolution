/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-service.c : Abstract class for an email service */

/*
 *
 * Author :
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999-2003 Ximian, Inc. (www.ximian.com)
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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

#include <sys/poll.h>

#include "libedataserver/e-msgport.h"

#include "camel-service.h"
#include "camel-session.h"
#include "camel-exception.h"
#include "camel-operation.h"
#include "camel-private.h"
#include "camel-i18n.h"

#define d(x)
#define w(x)

static CamelObjectClass *parent_class = NULL;

/* Returns the class for a CamelService */
#define CSERV_CLASS(so) CAMEL_SERVICE_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static void construct (CamelService *service, CamelSession *session,
		       CamelProvider *provider, CamelURL *url,
		       CamelException *ex);
static gboolean service_connect(CamelService *service, CamelException *ex);
static gboolean service_disconnect(CamelService *service, gboolean clean,
				   CamelException *ex);
static void cancel_connect (CamelService *service);
static GList *query_auth_types (CamelService *service, CamelException *ex);
static char *get_name (CamelService *service, gboolean brief);
static char *get_path (CamelService *service);

static int service_setv (CamelObject *object, CamelException *ex, CamelArgV *args);
static int service_getv (CamelObject *object, CamelException *ex, CamelArgGetV *args);


static void
camel_service_class_init (CamelServiceClass *camel_service_class)
{
	CamelObjectClass *object_class = CAMEL_OBJECT_CLASS (camel_service_class);
	
	parent_class = camel_type_get_global_classfuncs (CAMEL_OBJECT_TYPE);
	
	/* virtual method overloading */
	object_class->setv = service_setv;
	object_class->getv = service_getv;
	
	/* virtual method definition */
	camel_service_class->construct = construct;
	camel_service_class->connect = service_connect;
	camel_service_class->disconnect = service_disconnect;
	camel_service_class->cancel_connect = cancel_connect;
	camel_service_class->query_auth_types = query_auth_types;
	camel_service_class->get_name = get_name;
	camel_service_class->get_path = get_path;
}

static void
camel_service_init (void *o, void *k)
{
	CamelService *service = o;
	
	service->priv = g_malloc0(sizeof(*service->priv));
	service->priv->connect_lock = e_mutex_new(E_MUTEX_REC);
	service->priv->connect_op_lock = e_mutex_new(E_MUTEX_SIMPLE);
}

static void
camel_service_finalize (CamelObject *object)
{
	CamelService *service = CAMEL_SERVICE (object);

	if (service->status == CAMEL_SERVICE_CONNECTED) {
		CamelException ex;
		
		camel_exception_init (&ex);
		CSERV_CLASS (service)->disconnect (service, TRUE, &ex);
		if (camel_exception_is_set (&ex)) {
			w(g_warning ("camel_service_finalize: silent disconnect failure: %s",
				     camel_exception_get_description (&ex)));
		}
		camel_exception_clear (&ex);
	}
	
	if (service->url)
		camel_url_free (service->url);
	if (service->session)
		camel_object_unref (service->session);
	
	e_mutex_destroy (service->priv->connect_lock);
	e_mutex_destroy (service->priv->connect_op_lock);
	
	g_free (service->priv);
}



CamelType
camel_service_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type =
			camel_type_register (CAMEL_OBJECT_TYPE,
					     "CamelService",
					     sizeof (CamelService),
					     sizeof (CamelServiceClass),
					     (CamelObjectClassInitFunc) camel_service_class_init,
					     NULL,
					     (CamelObjectInitFunc) camel_service_init,
					     camel_service_finalize );
	}
	
	return type;
}


static int
service_setv (CamelObject *object, CamelException *ex, CamelArgV *args)
{
	CamelService *service = (CamelService *) object;
	CamelURL *url = service->url;
	gboolean reconnect = FALSE;
	guint32 tag;
	int i;
	
	for (i = 0; i < args->argc; i++) {
		tag = args->argv[i].tag;
		
		/* make sure this is an arg we're supposed to handle */
		if ((tag & CAMEL_ARG_TAG) <= CAMEL_SERVICE_ARG_FIRST ||
		    (tag & CAMEL_ARG_TAG) >= CAMEL_SERVICE_ARG_FIRST + 100)
			continue;
		
		if (tag == CAMEL_SERVICE_USERNAME) {
			/* set the username */
			if (strcmp (url->user, args->argv[i].ca_str) != 0) {
				camel_url_set_user (url, args->argv[i].ca_str);
				reconnect = TRUE;
			}
		} else if (tag == CAMEL_SERVICE_AUTH) {
			/* set the auth mechanism */
			if (strcmp (url->authmech, args->argv[i].ca_str) != 0) {
				camel_url_set_authmech (url, args->argv[i].ca_str);
				reconnect = TRUE;
			}
		} else if (tag == CAMEL_SERVICE_HOSTNAME) {
			/* set the hostname */
			if (strcmp (url->host, args->argv[i].ca_str) != 0) {
				camel_url_set_host (url, args->argv[i].ca_str);
				reconnect = TRUE;
			}
		} else if (tag == CAMEL_SERVICE_PORT) {
			/* set the port */
			if (url->port != args->argv[i].ca_int) {
				camel_url_set_port (url, args->argv[i].ca_int);
				reconnect = TRUE;
			}
		} else if (tag == CAMEL_SERVICE_PATH) {
			/* set the path */
			if (strcmp (url->path, args->argv[i].ca_str) != 0) {
				camel_url_set_path (url, args->argv[i].ca_str);
				reconnect = TRUE;
			}
		} else {
			/* error? */
			continue;
		}
		
		/* let our parent know that we've handled this arg */
		camel_argv_ignore (args, i);
	}
	
	/* FIXME: what if we are in the process of connecting? */
	if (reconnect && service->status == CAMEL_SERVICE_CONNECTED) {
		/* reconnect the service using the new URL */
		if (camel_service_disconnect (service, TRUE, ex))
			camel_service_connect (service, ex);
	}
	
	return CAMEL_OBJECT_CLASS (parent_class)->setv (object, ex, args);
}

static int
service_getv (CamelObject *object, CamelException *ex, CamelArgGetV *args)
{
	CamelService *service = (CamelService *) object;
	CamelURL *url = service->url;
	guint32 tag;
	int i;
	
	for (i = 0; i < args->argc; i++) {
		tag = args->argv[i].tag;
		
		/* make sure this is an arg we're supposed to handle */
		if ((tag & CAMEL_ARG_TAG) <= CAMEL_SERVICE_ARG_FIRST ||
		    (tag & CAMEL_ARG_TAG) >= CAMEL_SERVICE_ARG_FIRST + 100)
			continue;
		
		switch (tag) {
		case CAMEL_SERVICE_USERNAME:
			/* get the username */
			*args->argv[i].ca_str = url->user;
			break;
		case CAMEL_SERVICE_AUTH:
			/* get the auth mechanism */
			*args->argv[i].ca_str = url->authmech;
			break;
		case CAMEL_SERVICE_HOSTNAME:
			/* get the hostname */
			*args->argv[i].ca_str = url->host;
			break;
		case CAMEL_SERVICE_PORT:
			/* get the port */
			*args->argv[i].ca_int = url->port;
			break;
		case CAMEL_SERVICE_PATH:
			/* get the path */
			*args->argv[i].ca_str = url->path;
			break;
		default:
			/* error? */
			break;
		}
	}
	
	return CAMEL_OBJECT_CLASS (parent_class)->getv (object, ex, args);
}

static void
construct (CamelService *service, CamelSession *session, CamelProvider *provider, CamelURL *url, CamelException *ex)
{
	char *err, *url_string;
	
	if (CAMEL_PROVIDER_NEEDS (provider, CAMEL_URL_PART_USER) &&
	    (url->user == NULL || url->user[0] == '\0')) {
		err = _("URL '%s' needs a username component");
		goto fail;
	} else if (CAMEL_PROVIDER_NEEDS (provider, CAMEL_URL_PART_HOST) &&
		   (url->host == NULL || url->host[0] == '\0')) {
		err = _("URL '%s' needs a host component");
		goto fail;
	} else if (CAMEL_PROVIDER_NEEDS (provider, CAMEL_URL_PART_PATH) &&
		   (url->path == NULL || url->path[0] == '\0')) {
		err = _("URL '%s' needs a path component");
		goto fail;
	}
	
	service->provider = provider;
	service->url = camel_url_copy(url);
	service->session = session;
	camel_object_ref (session);
	
	service->status = CAMEL_SERVICE_DISCONNECTED;

	return;

fail:
	url_string = camel_url_to_string(url, CAMEL_URL_HIDE_PASSWORD);
	camel_exception_setv(ex, CAMEL_EXCEPTION_SERVICE_URL_INVALID, err, url_string);
	g_free(url_string);
}

/**
 * camel_service_construct:
 * @service: the CamelService
 * @session: the session for the service
 * @provider: the service's provider
 * @url: the default URL for the service (may be NULL)
 * @ex: a CamelException
 *
 * Constructs a CamelService initialized with the given parameters.
 **/
void
camel_service_construct (CamelService *service, CamelSession *session,
			 CamelProvider *provider, CamelURL *url,
			 CamelException *ex)
{
	g_return_if_fail (CAMEL_IS_SERVICE (service));
	g_return_if_fail (CAMEL_IS_SESSION (session));
	
	CSERV_CLASS (service)->construct (service, session, provider, url, ex);
}


static gboolean
service_connect (CamelService *service, CamelException *ex)
{
	/* Things like the CamelMboxStore can validly
	 * not define a connect function.
	 */
	 return TRUE;
}

/**
 * camel_service_connect:
 * @service: CamelService object
 * @ex: a CamelException
 *
 * Connect to the service using the parameters it was initialized
 * with.
 *
 * Return value: whether or not the connection succeeded
 **/

gboolean
camel_service_connect (CamelService *service, CamelException *ex)
{
	gboolean ret = FALSE;
	gboolean unreg = FALSE;
	
	g_return_val_if_fail (CAMEL_IS_SERVICE (service), FALSE);
	g_return_val_if_fail (service->session != NULL, FALSE);
	g_return_val_if_fail (service->url != NULL, FALSE);
	
	CAMEL_SERVICE_LOCK (service, connect_lock);
	
	if (service->status == CAMEL_SERVICE_CONNECTED) {
		CAMEL_SERVICE_UNLOCK (service, connect_lock);
		return TRUE;
	}

	/* Register a separate operation for connecting, so that
	 * the offline code can cancel it.
	 */
	CAMEL_SERVICE_LOCK (service, connect_op_lock);
	service->connect_op = camel_operation_registered ();
	if (!service->connect_op) {
		service->connect_op = camel_operation_new (NULL, NULL);
		camel_operation_register (service->connect_op);
		unreg = TRUE;
	}
	CAMEL_SERVICE_UNLOCK (service, connect_op_lock);

	service->status = CAMEL_SERVICE_CONNECTING;
	ret = CSERV_CLASS (service)->connect (service, ex);
	service->status = ret ? CAMEL_SERVICE_CONNECTED : CAMEL_SERVICE_DISCONNECTED;

	CAMEL_SERVICE_LOCK (service, connect_op_lock);
	if (service->connect_op) {
		if (unreg)
			camel_operation_unregister (service->connect_op);
		
		camel_operation_unref (service->connect_op);
		service->connect_op = NULL;
	}
	CAMEL_SERVICE_UNLOCK (service, connect_op_lock);

	CAMEL_SERVICE_UNLOCK (service, connect_lock);
	
	return ret;
}

static gboolean
service_disconnect (CamelService *service, gboolean clean, CamelException *ex)
{
	/*service->connect_level--;*/

	/* We let people get away with not having a disconnect
	 * function -- CamelMboxStore, for example. 
	 */
	
	return TRUE;
}

/**
 * camel_service_disconnect:
 * @service: CamelService object
 * @clean: whether or not to try to disconnect cleanly.
 * @ex: a CamelException
 *
 * Disconnect from the service. If @clean is %FALSE, it should not
 * try to do any synchronizing or other cleanup of the connection.
 *
 * Return value: whether or not the disconnection succeeded without
 * errors. (Consult @ex if %FALSE.)
 **/
gboolean
camel_service_disconnect (CamelService *service, gboolean clean,
			  CamelException *ex)
{
	gboolean res = TRUE;
	int unreg = FALSE;

	CAMEL_SERVICE_LOCK (service, connect_lock);
	
	if (service->status != CAMEL_SERVICE_DISCONNECTED
	    && service->status != CAMEL_SERVICE_DISCONNECTING) {
		CAMEL_SERVICE_LOCK (service, connect_op_lock);
		service->connect_op = camel_operation_registered ();
		if (!service->connect_op) {
			service->connect_op = camel_operation_new (NULL, NULL);
			camel_operation_register (service->connect_op);
			unreg = TRUE;
		}
		CAMEL_SERVICE_UNLOCK (service, connect_op_lock);

		service->status = CAMEL_SERVICE_DISCONNECTING;
		res = CSERV_CLASS (service)->disconnect (service, clean, ex);
		service->status = CAMEL_SERVICE_DISCONNECTED;

		CAMEL_SERVICE_LOCK (service, connect_op_lock);
		if (unreg)
			camel_operation_unregister (service->connect_op);

		camel_operation_unref (service->connect_op);
		service->connect_op = NULL;
		CAMEL_SERVICE_UNLOCK (service, connect_op_lock);
	}
	
	CAMEL_SERVICE_UNLOCK (service, connect_lock);
	
	return res;
}

static void
cancel_connect (CamelService *service)
{
	camel_operation_cancel (service->connect_op);
}

/**
 * camel_service_cancel_connect:
 * @service: a service
 *
 * If @service is currently attempting to connect to or disconnect
 * from a server, this causes it to stop and fail. Otherwise it is a
 * no-op.
 **/
void
camel_service_cancel_connect (CamelService *service)
{
	CAMEL_SERVICE_LOCK (service, connect_op_lock);
	if (service->connect_op)
		CSERV_CLASS (service)->cancel_connect (service);
	CAMEL_SERVICE_UNLOCK (service, connect_op_lock);
}

/**
 * camel_service_get_url:
 * @service: a service
 *
 * Returns the URL representing a service. The returned URL must be
 * freed when it is no longer needed. For security reasons, this
 * routine does not return the password.
 *
 * Return value: the url name
 **/
char *
camel_service_get_url (CamelService *service)
{
	return camel_url_to_string (service->url, CAMEL_URL_HIDE_PASSWORD);
}


static char *
get_name (CamelService *service, gboolean brief)
{
	w(g_warning ("CamelService::get_name not implemented for `%s'",
		     camel_type_to_name (CAMEL_OBJECT_GET_TYPE (service))));
	return g_strdup ("???");
}		

/**
 * camel_service_get_name:
 * @service: the service
 * @brief: whether or not to use a briefer form
 *
 * This gets the name of the service in a "friendly" (suitable for
 * humans) form. If @brief is %TRUE, this should be a brief description
 * such as for use in the folder tree. If @brief is %FALSE, it should
 * be a more complete and mostly unambiguous description.
 *
 * Return value: the description, which the caller must free.
 **/
char *
camel_service_get_name (CamelService *service, gboolean brief)
{
	g_return_val_if_fail (CAMEL_IS_SERVICE (service), NULL);
	g_return_val_if_fail (service->url, NULL);
	
	return CSERV_CLASS (service)->get_name (service, brief);
}


static char *
get_path (CamelService *service)
{
	CamelProvider *prov = service->provider;
	CamelURL *url = service->url;
	GString *gpath;
	char *path;
	
	/* A sort of ad-hoc default implementation that works for our
	 * current set of services.
	 */
	
	gpath = g_string_new (service->provider->protocol);
	if (CAMEL_PROVIDER_ALLOWS (prov, CAMEL_URL_PART_USER)) {
		if (CAMEL_PROVIDER_ALLOWS (prov, CAMEL_URL_PART_HOST)) {
			g_string_append_printf (gpath, "/%s@%s",
						url->user ? url->user : "",
						url->host ? url->host : "");
			
			if (url->port)
				g_string_append_printf (gpath, ":%d", url->port);
		} else {
			g_string_append_printf (gpath, "/%s%s", url->user ? url->user : "",
						CAMEL_PROVIDER_NEEDS (prov, CAMEL_URL_PART_USER) ? "" : "@");
		}
	} else if (CAMEL_PROVIDER_ALLOWS (prov, CAMEL_URL_PART_HOST)) {
		g_string_append_printf (gpath, "/%s%s",
					CAMEL_PROVIDER_NEEDS (prov, CAMEL_URL_PART_HOST) ? "" : "@",
					url->host ? url->host : "");
		
		if (url->port)
			g_string_append_printf (gpath, ":%d", url->port);
	}
	
	if (CAMEL_PROVIDER_NEEDS (prov, CAMEL_URL_PART_PATH))
		g_string_append_printf (gpath, "%s%s", *url->path == '/' ? "" : "/", url->path);
	
	path = gpath->str;
	g_string_free (gpath, FALSE);
	
	return path;
}		

/**
 * camel_service_get_path:
 * @service: the service
 *
 * This gets a valid UNIX relative path describing the service, which
 * is guaranteed to be different from the path returned for any
 * different service. This path MUST start with the name of the
 * provider, followed by a "/", but after that, it is up to the
 * provider.
 *
 * Return value: the path, which the caller must free.
 **/
char *
camel_service_get_path (CamelService *service)
{
	g_return_val_if_fail (CAMEL_IS_SERVICE (service), NULL);
	g_return_val_if_fail (service->url, NULL);
	
	return CSERV_CLASS (service)->get_path (service);
}


/**
 * camel_service_get_session:
 * @service: a service
 *
 * Returns the CamelSession associated with the service.
 *
 * Return value: the session
 **/
CamelSession *
camel_service_get_session (CamelService *service)
{
	return service->session;
}

/**
 * camel_service_get_provider:
 * @service: a service
 *
 * Returns the CamelProvider associated with the service.
 *
 * Return value: the provider
 **/
CamelProvider *
camel_service_get_provider (CamelService *service)
{
	return service->provider;
}

static GList *
query_auth_types (CamelService *service, CamelException *ex)
{
	return NULL;
}

/**
 * camel_service_query_auth_types:
 * @service: a CamelService
 * @ex: a CamelException
 *
 * This is used by the mail source wizard to get the list of
 * authentication types supported by the protocol, and information
 * about them.
 *
 * Return value: a list of CamelServiceAuthType records. The caller
 * must free the list with g_list_free() when it is done with it.
 **/
GList *
camel_service_query_auth_types (CamelService *service, CamelException *ex)
{
	GList *ret;
	
	/* note that we get the connect lock here, which means the callee
	   must not call the connect functions itself */
	CAMEL_SERVICE_LOCK (service, connect_lock);
	ret = CSERV_CLASS (service)->query_auth_types (service, ex);
	CAMEL_SERVICE_UNLOCK (service, connect_lock);
	
	return ret;
}
