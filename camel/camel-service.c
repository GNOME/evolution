/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelService.c : Abstract class for an email service */

/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999, 2000 HelixCode (http://www.helixcode.com) .
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
#include "camel-log.h"
#include "camel-exception.h"

static GtkObjectClass *parent_class=NULL;

/* Returns the class for a CamelService */
#define CSERV_CLASS(so) CAMEL_SERVICE_CLASS (GTK_OBJECT(so)->klass)

static gboolean _connect(CamelService *service, CamelException *ex);
static gboolean _connect_with_url (CamelService *service, Gurl *url,
				   CamelException *ex);
static gboolean _disconnect(CamelService *service, CamelException *ex);
static gboolean _is_connected (CamelService *service);
static void _finalize (GtkObject *object);
static gboolean _set_url (CamelService *service, Gurl *url,
			  CamelException *ex);

static void
camel_service_class_init (CamelServiceClass *camel_service_class)
{
	GtkObjectClass *gtk_object_class =
		GTK_OBJECT_CLASS (camel_service_class);

	parent_class = gtk_type_class (gtk_object_get_type ());
	
	/* virtual method definition */
	camel_service_class->connect = _connect;
	camel_service_class->connect_with_url = _connect_with_url;
	camel_service_class->disconnect = _disconnect;
	camel_service_class->is_connected = _is_connected;

	/* virtual method overload */
	gtk_object_class->finalize = _finalize;
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
			(GtkObjectInitFunc) NULL,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_service_type = gtk_type_unique (gtk_object_get_type (),
						      &camel_service_info);
	}
	
	return camel_service_type;
}


static void           
_finalize (GtkObject *object)
{
	CamelService *camel_service = CAMEL_SERVICE (object);

	CAMEL_LOG_FULL_DEBUG ("Entering CamelService::finalize\n");

	if (camel_service->url)
		g_url_free (camel_service->url);
	if (camel_service->session)
		gtk_object_unref (GTK_OBJECT (camel_service->session));

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelService::finalize\n");
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
camel_service_new (GtkType type, CamelSession *session, Gurl *url,
		   CamelException *ex)
{
	CamelService *service;

	g_assert(session);

	service = CAMEL_SERVICE (gtk_object_new (type, NULL));
	service->session = session;
	gtk_object_ref (GTK_OBJECT (session));
	if (url) {
		if (!_set_url (service, url, ex))
			return NULL;
	}

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
	g_assert (service->session);
	/* XXX it's possible that this should be an exception
	 * rather than an assertion... I'm not sure how the code
	 * is supposed to be used.
	 */
	g_assert (service->url);

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
_connect_with_url (CamelService *service, Gurl *url, CamelException *ex)
{
	g_assert (service->session);

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
camel_service_connect_with_url (CamelService *service, char *url,
				CamelException *ex)
{
	return CSERV_CLASS(service)->connect_with_url (service, g_url_new(url),
						       ex);
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
 * This converts the URL to a Gurl, validates it for the service,
 * and sets it as the default URL for the service.
 *
 * Return value: success or failure
 **/
static gboolean
_set_url (CamelService *service, Gurl *url, CamelException *ex)
{
	char *url_string;

	if (service->url_flags & CAMEL_SERVICE_URL_NEED_USER &&
	    (url->user == NULL || url->user[0] == '\0')) {
		url_string = g_url_to_string (url, FALSE);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_URL_INVALID,
				      "URL '%s' needs a username component",
				      url_string);
		g_free (url_string);
		return FALSE;
	} else if (service->url_flags & CAMEL_SERVICE_URL_NEED_HOST &&
		   (url->host == NULL || url->host[0] == '\0')) {
		url_string = g_url_to_string (url, FALSE);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_URL_INVALID,
				      "URL '%s' needs a host component",
				      url_string);
		g_free (url_string);
		return FALSE;
	} else if (service->url_flags & CAMEL_SERVICE_URL_NEED_PATH &&
		   (url->path == NULL || url->path[0] == '\0')) {
		url_string = g_url_to_string (url, FALSE);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_URL_INVALID,
				      "URL '%s' needs a path component",
				      url_string);
		g_free (url_string);
		return FALSE;
	}

	if (service->url)
		g_url_free (service->url);
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
	return g_url_to_string(service->url, FALSE);
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
