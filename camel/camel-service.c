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
#define CSERV_CLASS(so) CAMEL_SERVICE_CLASS (GTK_OBJECT(so)->klass)

static gboolean _connect(CamelService *service, CamelException *ex);
static gboolean _connect_with_url (CamelService *service, CamelURL *url,
				   CamelException *ex);
static gboolean _disconnect(CamelService *service, CamelException *ex);
static gboolean _is_connected (CamelService *service);
static GList *  _query_auth_types (CamelService *service, CamelException *ex);
static void     _free_auth_types (CamelService *service, GList *authtypes);
static void     _finalize (GtkObject *object);
static gboolean _set_url (CamelService *service, CamelURL *url,
			  CamelException *ex);

static void
camel_service_class_init (CamelServiceClass *camel_service_class)
{
	GtkObjectClass *gtk_object_class =
		GTK_OBJECT_CLASS (camel_service_class);

	parent_class = gtk_type_class (camel_object_get_type ());
	
	/* virtual method definition */
	camel_service_class->connect = _connect;
	camel_service_class->connect_with_url = _connect_with_url;
	camel_service_class->disconnect = _disconnect;
	camel_service_class->is_connected = _is_connected;
	camel_service_class->query_auth_types = _query_auth_types;
	camel_service_class->free_auth_types = _free_auth_types;

	/* virtual method overload */
	gtk_object_class->finalize = _finalize;
}

static void
camel_service_init (void *o, void *k)
{
/*	CamelService *service = o;*/
	return;
}

GtkType
camel_service_get_type (void)
{
	static GtkType camel_service_type = 0;
	
	if (!camel_service_type)	{
		GtkTypeInfo camel_service_info =	
		{
			"CamelService",
			sizeof (CamelService),
			sizeof (CamelServiceClass),
			(GtkClassInitFunc) camel_service_class_init,
			(GtkObjectInitFunc) camel_service_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_service_type = gtk_type_unique (camel_object_get_type (),
						      &camel_service_info);
	}
	
	return camel_service_type;
}

static void           
_finalize (GtkObject *object)
{
	CamelService *camel_service = CAMEL_SERVICE (object);

	if (camel_service->url)
		camel_url_free (camel_service->url);
	if (camel_service->session)
		gtk_object_unref (GTK_OBJECT (camel_service->session));

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}


/**
 * camel_service_new: create a new CamelService or subtype
 * @type: the GtkType of the class to create
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
camel_service_new (GtkType type, CamelSession *session, CamelURL *url,
		   CamelException *ex)
{
	CamelService *service;

	g_return_val_if_fail (CAMEL_IS_SESSION (session), NULL);

	service = CAMEL_SERVICE (gtk_object_new (type, NULL));
	service->session = session;
	gtk_object_ref (GTK_OBJECT (session));
	if (!_set_url (service, url, ex) && !url->empty)
		return NULL;

	return service;
}

/**
 * _connect : connect to a service 
 * @service: object to connect
 * @ex: a CamelException
 *
 * connect to the service using the parameters 
 * stored in the session it is initialized with
 *
 * Return value: whether or not the connection succeeded
 **/
static gboolean
_connect (CamelService *service, CamelException *ex)
{
	service->connected = TRUE;
	return TRUE;
}



/**
 * camel_service_connect:connect to a service 
 * @service: CamelService object
 * @ex: a CamelException
 * 
 * connect to the service using the parameters 
 * stored in the session it is initialized with
 *
 * Return value: whether or not the connection succeeded
 **/
gboolean
camel_service_connect (CamelService *service, CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_SERVICE (service), FALSE);
	g_return_val_if_fail (service->session != NULL, FALSE);
	g_return_val_if_fail (service->url != NULL, FALSE);

	return CSERV_CLASS(service)->connect(service, ex);
}



/**
 * _connect_with_url: connect to the specified address
 * @service: object to connect
 * @url: URL describing service to connect to
 * @ex: a CamelException
 *
 * Connect to the service, but do not use the session
 * default parameters to retrieve server's address
 *
 * Return value: whether or not the connection succeeded
 **/
static gboolean
_connect_with_url (CamelService *service, CamelURL *url, CamelException *ex)
{
	if (!_set_url (service, url, ex))
		return FALSE;

	return CSERV_CLASS(service)->connect (service, ex);
}

/**
 * camel_service_connect_with_url: connect a service 
 * @service:  the service to connect
 * @url:  URL describing the service to connect to
 * @ex: a CamelException
 * 
 * Connect to a service, but do not use the session
 * default parameters to retrieve server's address
 *
 * Return value: whether or not the connection succeeded
 **/
gboolean
camel_service_connect_with_url (CamelService *service, char *url_string,
				CamelException *ex)
{
	CamelURL *url;

	g_return_val_if_fail (CAMEL_IS_SERVICE (service), FALSE);
	g_return_val_if_fail (service->session != NULL, FALSE);

	url = camel_url_new (url_string, ex);
	if (!url)
		return FALSE;
	return CSERV_CLASS(service)->connect_with_url (service, url, ex);
}



/**
 * _disconnect : disconnect from a service 
 * @service: object to disconnect
 * @ex: a CamelException
 *
 * disconnect from the service
 *
 * Return value: whether or not the disconnection succeeded without
 * errors. (Consult @ex if FALSE.)
 **/
static gboolean
_disconnect (CamelService *service, CamelException *ex)
{
	service->connected = FALSE;

	return TRUE;
}



/**
 * camel_service_disconnect: disconnect from a service 
 * @service: CamelService object
 * @ex: a CamelException
 *
 * disconnect from the service
 *
 * Return value: whether or not the disconnection succeeded without
 * errors. (Consult @ex if FALSE.)
 **/
gboolean
camel_service_disconnect (CamelService *service, CamelException *ex)
{
	return CSERV_CLASS(service)->disconnect(service, ex);
}



/**
 * _is_connected: test if the service object is connected
 * @service: object to test
 * 
 * Return value: whether or not the service is connected
 **/
static gboolean
_is_connected (CamelService *service)
{
	return service->connected;
}


/**
 * camel_service_is_connected: test if the service object is connected
 * @service: object to test
 * 
 * Return value: whether or not the service is connected
 **/
gboolean
camel_service_is_connected (CamelService *service)
{
	return CSERV_CLASS(service)->is_connected(service);
}


/**
 * _set_url: Validate a URL and set it as the default for a service
 * @service: the CamelService
 * @url_string: the URL
 * @ex: a CamelException
 *
 * This converts the URL to a CamelURL, validates it for the service,
 * and sets it as the default URL for the service.
 *
 * Return value: success or failure
 **/
static gboolean
_set_url (CamelService *service, CamelURL *url, CamelException *ex)
{
	char *url_string;

	if (service->url_flags & CAMEL_SERVICE_URL_NEED_USER &&
	    (url->user == NULL || url->user[0] == '\0')) {
		url_string = camel_url_to_string (url, FALSE);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_URL_INVALID,
				      "URL '%s' needs a username component",
				      url_string);
		g_free (url_string);
		return FALSE;
	} else if (service->url_flags & CAMEL_SERVICE_URL_NEED_HOST &&
		   (url->host == NULL || url->host[0] == '\0')) {
		url_string = camel_url_to_string (url, FALSE);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_URL_INVALID,
				      "URL '%s' needs a host component",
				      url_string);
		g_free (url_string);
		return FALSE;
	} else if (service->url_flags & CAMEL_SERVICE_URL_NEED_PATH &&
		   (url->path == NULL || url->path[0] == '\0')) {
		url_string = camel_url_to_string (url, FALSE);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_URL_INVALID,
				      "URL '%s' needs a path component",
				      url_string);
		g_free (url_string);
		return FALSE;
	}

	if (service->url)
		camel_url_free (service->url);
	service->url = url;
	return TRUE;
}

/**
 * camel_service_get_url: get the url representing a service
 * @service: the service
 * 
 * returns the URL representing a service. The returned URL must be
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


/**
 * camel_service_get_session: return the session associated with a service
 * @service: the service
 *
 * returns the CamelSession associated with the service.
 *
 * Return value: the session
 **/
CamelSession *
camel_service_get_session (CamelService *service)
{
	return service->session;
}


GList *
_query_auth_types (CamelService *service, CamelException *ex)
{
	return NULL;
}

/**
 * camel_service_query_auth_types: return a list of supported
 * authentication types.
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
	return CSERV_CLASS (service)->query_auth_types (service, ex);
}

static void
_free_auth_types (CamelService *service, GList *authtypes)
{
	;
}

/**
 * camel_service_free_auth_types: free a type list returned by
 * camel_service_query_auth_types.
 * @service: the service
 * @authtypes: the list of authtypes
 *
 * This frees the data allocated by camel_service_query_auth_types.
 **/
void
camel_service_free_auth_types (CamelService *service, GList *authtypes)
{
	CSERV_CLASS (service)->free_auth_types (service, authtypes);
}



/* URL utility routines */

/**
 * camel_service_gethost: get a hostent for a CamelService's host
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
