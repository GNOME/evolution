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

static GtkObjectClass *parent_class=NULL;

/* Returns the class for a CamelService */
#define CSERV_CLASS(so) CAMEL_SERVICE_CLASS (GTK_OBJECT(so)->klass)

static gboolean _connect(CamelService *service, CamelException *ex);
static gboolean _connect_with_url (CamelService *service, Gurl *url,
				   CamelException *ex);
static gboolean _disconnect(CamelService *service, CamelException *ex);
static gboolean _is_connected (CamelService *service);
static void _finalize (GtkObject *object);

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

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelService::finalize\n");
}



/**
 * _connect : connect to a service 
 *
 * connect to the service using the parameters 
 * stored in the session it is initialized with
 * WARNING: session not implemented for the moment
 *
 * @service: object to connect
 **/
static gboolean
_connect (CamelService *service, CamelException *ex)
{
#warning need to get default URL from somewhere
	return CSERV_CLASS(service)->connect_with_url(service, NULL, ex);
}



/**
 * camel_service_connect:connect to a service 
 * @service: CamelService object
 * 
 * connect to the service using the parameters 
 * stored in the session it is initialized with
 * WARNING: session not implemented for the moment
 * 
 **/
gboolean
camel_service_connect (CamelService *service, CamelException *ex)
{
	return CSERV_CLASS(service)->connect(service, ex);
}



/**
 * _connect_with_url: connect to the specified address
 * 
 * Connect to the service, but do not use the session
 * default parameters to retrieve server's address
 *
 * @service: object to connect
 * @url: URL describing service to connect to
 **/
static gboolean
_connect_with_url (CamelService *service, Gurl *url, CamelException *ex)
{
	service->connected = TRUE;
	service->url = url;

	return TRUE;
}

/**
 * camel_service_connect_with_url: connect a service 
 * @service:  the service to connect
 * @url:  URL describing the service to connect to
 * 
 * Connect to a service, but do not use the session
 * default parameters to retrieve server's address
 * 
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
 *
 * disconnect from the service
 *
 * @service: object to disconnect
 **/
static gboolean
_disconnect (CamelService *service, CamelException *ex)
{
	service->connected = FALSE;
	if (service->url) {
		g_url_free(service->url);
		service->url = NULL;
	}

	return TRUE;
}



/**
 * camel_service_disconnect: disconnect from a service 
 * @service: CamelService object
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
 * Return value: 
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
 * Return value: 
 **/
gboolean
camel_service_is_connected (CamelService *service)
{
	return CSERV_CLASS(service)->is_connected(service);
}


/**
 * camel_service_get_url: get the url representing a service
 * @service: the service
 * 
 * returns the URL representing a service. For security reasons 
 * This routine does not return the password. 
 * 
 * Return value: the url name
 **/
gchar *
camel_service_get_url (CamelService *service)
{
	return g_url_to_string(service->url, FALSE);
}


