/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-service.c : Abstract class for an email service */

/*
 *
 * Author :
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999, 2000 Helix Code, Inc. (http://www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
#include <config.h>
#include "camel-service.h"
#include "camel-session.h"
#include "camel-exception.h"

#include <ctype.h>
#include <stdlib.h>

static CamelObjectClass *parent_class = NULL;

/* Returns the class for a CamelService */
#define CSERV_CLASS(so) CAMEL_SERVICE_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static gboolean service_connect(CamelService *service, CamelException *ex);
static gboolean service_disconnect(CamelService *service, CamelException *ex);
/*static gboolean is_connected (CamelService *service);*/
static GList *  query_auth_types_func (CamelService *service, CamelException *ex);
static void     free_auth_types (CamelService *service, GList *authtypes);
static char *   get_name (CamelService *service, gboolean brief);
static gboolean check_url (CamelService *service, CamelException *ex);


static void
camel_service_class_init (CamelServiceClass *camel_service_class)
{
	parent_class = camel_type_get_global_classfuncs (CAMEL_OBJECT_TYPE);

	/* virtual method definition */
	camel_service_class->connect = service_connect;
	camel_service_class->disconnect = service_disconnect;
	/*camel_service_class->is_connected = is_connected;*/
	camel_service_class->query_auth_types_connected = query_auth_types_func;
	camel_service_class->query_auth_types_generic = query_auth_types_func;
	camel_service_class->free_auth_types = free_auth_types;
	camel_service_class->get_name = get_name;
}

static void
camel_service_finalize (CamelObject *object)
{
	CamelService *camel_service = CAMEL_SERVICE (object);

	if (camel_service->connected) {
		CamelException ex;

		/*g_warning ("camel_service_finalize: finalizing while still connected!");*/
		camel_exception_init (&ex);
		CSERV_CLASS (camel_service)->disconnect (camel_service, &ex);
		if (camel_exception_is_set (&ex)) {
			g_warning ("camel_service_finalize: silent disconnect failure: %s",
				   camel_exception_get_description(&ex));
		}
		camel_exception_clear (&ex);
	}

	if (camel_service->url)
		camel_url_free (camel_service->url);
	if (camel_service->session)
		camel_object_unref (CAMEL_OBJECT (camel_service->session));
}



CamelType
camel_service_get_type (void)
{
	static CamelType camel_service_type = CAMEL_INVALID_TYPE;

	if (camel_service_type == CAMEL_INVALID_TYPE) {
		camel_service_type = camel_type_register( CAMEL_OBJECT_TYPE, "CamelService",
							  sizeof (CamelService),
							  sizeof (CamelServiceClass),
							  (CamelObjectClassInitFunc) camel_service_class_init,
							  NULL,
							  NULL,
							  camel_service_finalize );
	}

	return camel_service_type;
}

static gboolean
check_url (CamelService *service, CamelException *ex)
{
	char *url_string;

	if (service->url_flags & CAMEL_SERVICE_URL_NEED_USER &&
	    (service->url->user == NULL || service->url->user[0] == '\0')) {
		url_string = camel_url_to_string (service->url, FALSE);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_URL_INVALID,
				      "URL '%s' needs a username component",
				      url_string);
		g_free (url_string);
		return FALSE;
	} else if (service->url_flags & CAMEL_SERVICE_URL_NEED_HOST &&
		   (service->url->host == NULL || service->url->host[0] == '\0')) {
		url_string = camel_url_to_string (service->url, FALSE);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_URL_INVALID,
				      "URL '%s' needs a host component",
				      url_string);
		g_free (url_string);
		return FALSE;
	} else if (service->url_flags & CAMEL_SERVICE_URL_NEED_PATH &&
		   (service->url->path == NULL || service->url->path[0] == '\0')) {
		url_string = camel_url_to_string (service->url, FALSE);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_URL_INVALID,
				      "URL '%s' needs a path component",
				      url_string);
		g_free (url_string);
		return FALSE;
	}

	return TRUE;
}

/**
 * camel_service_new: create a new CamelService or subtype
 * @type: the CamelType of the class to create
 * @session: the session for the service
 * @url: the default URL for the service (may be NULL)
 * @ex: a CamelException
 *
 * Creates a new CamelService (or one of its subtypes), initialized
 * with the given parameters.
 *
 * Return value: the CamelService, or NULL.
 **/
CamelService *
camel_service_new (CamelType type, CamelSession *session, CamelProvider *provider,
		   CamelURL *url, CamelException *ex)
{
	CamelService *service;

	g_return_val_if_fail (CAMEL_IS_SESSION (session), NULL);

	service = CAMEL_SERVICE (camel_object_new (type));

	/*service->connect_level = 0;*/

	service->url = url;
	if (!url->empty && !check_url (service, ex)) {
		camel_object_unref (CAMEL_OBJECT (service));
		return NULL;
	}

	service->session = session;
	camel_object_ref (CAMEL_OBJECT (session));

	service->provider = provider;
	/* don't ref -- providers are not CamelObjects */

	service->connected = FALSE;
	
	if (!url->empty) {
		if (CSERV_CLASS (service)->connect (service, ex) == FALSE) {
			camel_object_unref (CAMEL_OBJECT (service));
			return NULL;
		}

		service->connected = TRUE;
	}

	return service;
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
	g_return_val_if_fail (CAMEL_IS_SERVICE (service), FALSE);
	g_return_val_if_fail (service->session != NULL, FALSE);
	g_return_val_if_fail (service->url != NULL, FALSE);

	if (service->connected) {
		/* But we're still connected, so no exception
		 * and return true.
		 */
		g_warning ("camel_service_connect: trying to connect to an already connected service");
		return TRUE;
	}

	return CSERV_CLASS (service)->connect (service, ex);
}

static gboolean
service_disconnect (CamelService *service, CamelException *ex)
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
 * @ex: a CamelException
 *
 * Disconnect from the service.
 *
 * Return value: whether or not the disconnection succeeded without
 * errors. (Consult @ex if %FALSE.)
 **/

gboolean
camel_service_disconnect (CamelService *service, CamelException *ex)
{
	if (!service->connected) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_NOT_CONNECTED,
				     "Trying to disconnect from a service that isn't connected");
		return FALSE;
	}
	
	return CSERV_CLASS (service)->disconnect (service, ex);
}

/**
 *static gboolean
 *is_connected (CamelService *service)
 *{
 *	return (service->connect_level > 0);
 *}
 **/

/**
 * camel_service_is_connected:
 * @service: object to test
 *
 * Return value: whether or not the service is connected
 **/
/**
 *gboolean
 *camel_service_is_connected (CamelService *service)
 *{
 *	return CSERV_CLASS (service)->is_connected (service);
 *}
 **/

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
	return camel_url_to_string(service->url, FALSE);
}


static char *
get_name (CamelService *service, gboolean brief)
{
	g_warning ("CamelService::get_name not implemented for `%s'",
		   camel_type_to_name (CAMEL_OBJECT_GET_TYPE (service)));
	return "???";
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

GList *
query_auth_types_func (CamelService *service, CamelException *ex)
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
 * This may be called on a service with or without an associated URL.
 * If there is no URL, the routine must return a generic answer. If
 * the service does have a URL, the routine SHOULD connect to the
 * server and query what authentication mechanisms it supports. If
 * it cannot do that for any reason, it should set @ex accordingly.
 *
 * Return value: a list of CamelServiceAuthType records. The caller
 * must free the list by calling camel_service_free_auth_types when
 * it is done.
 **/
GList *
camel_service_query_auth_types (CamelService *service, CamelException *ex)
{
	if (service->connected)
		return CSERV_CLASS (service)->query_auth_types_connected (service, ex);
	else
		return CSERV_CLASS (service)->query_auth_types_generic (service, ex);
}


static void
free_auth_types (CamelService *service, GList *authtypes)
{
	;
}

/**
 * camel_service_free_auth_types:
 * @service: the service
 * @authtypes: the list of authtypes
 *
 * This frees the data allocated by camel_service_query_auth_types().
 **/
void
camel_service_free_auth_types (CamelService *service, GList *authtypes)
{
	CSERV_CLASS (service)->free_auth_types (service, authtypes);
}


/* URL utility routines */

/**
 * camel_service_gethost:
 * @service: a CamelService
 * @ex: a CamelException
 *
 * This is a convenience function to do a gethostbyname on the host
 * for the service's URL.
 *
 * Return value: a (statically-allocated) hostent.
 **/
struct hostent *
camel_service_gethost (CamelService *service, CamelException *ex)
{
	struct hostent *h;
	char *hostname;

	if (service->url->host)
		hostname = service->url->host;
	else
		hostname = "localhost";
	h = gethostbyname (hostname);
	if (!h) {
		extern int h_errno;

		if (h_errno == HOST_NOT_FOUND || h_errno == NO_DATA) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_URL_INVALID,
					      "No such host %s.", hostname);
		} else {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					      "Temporarily unable to look up "
					      "hostname %s.", hostname);
		}
		return NULL;
	}

	return h;
}
