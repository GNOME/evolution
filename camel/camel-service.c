/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelService.c : Abstract class for an email service */

/* 
 *
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@aful.org> .
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

static void _connect(CamelService *service);
static void _connect_to_with_login_passwd (CamelService *service, gchar *host, gchar *login, gchar *passwd);
static void _connect_to_with_login_passwd_port (CamelService *service, gchar *host, gchar *login, gchar *passwd, guint port);
static gboolean _is_connected (CamelService *service);
static void _set_connected (CamelService *service, gboolean state);
static const gchar *_get_url (CamelService *service);
static void _finalize (GtkObject *object);

static void
camel_service_class_init (CamelServiceClass *camel_service_class)
{
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (camel_service_class);

	parent_class = gtk_type_class (gtk_object_get_type ());
	
	/* virtual method definition */
	camel_service_class->connect = _connect;
	camel_service_class->connect_to_with_login_passwd = _connect_to_with_login_passwd;
	camel_service_class->connect_to_with_login_passwd_port = _connect_to_with_login_passwd_port;
	camel_service_class->is_connected = _is_connected;
	camel_service_class->set_connected = _set_connected;
	camel_service_class->get_url = _get_url;

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
		
		camel_service_type = gtk_type_unique (gtk_object_get_type (), &camel_service_info);
	}
	
	return camel_service_type;
}


static void           
_finalize (GtkObject *object)
{
	CamelService *camel_service = CAMEL_SERVICE (object);

	CAMEL_LOG_FULL_DEBUG ("Entering CamelService::finalize\n");

	if (camel_service->url) g_free (camel_service->url);

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
static void
_connect (CamelService *service)
{
	CSERV_CLASS(service)->set_connected(service, TRUE);
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
void
camel_service_connect (CamelService *service)
{
	CSERV_CLASS(service)->connect(service);
}



/**
 * _connect_to: connect to the specified address
 * 
 * Connect to the service, but do not use the session
 * default parameters to retrieve server's address
 *
 * @service: object to connect
 * @host: host to connect to
 * @login: user name used to log in
 * @passwd: password used to log in
 **/
static void
_connect_to_with_login_passwd (CamelService *service, gchar *host, gchar *login, gchar *passwd)
{
  CSERV_CLASS(service)->set_connected(service, TRUE);
}

/**
 * camel_service_connect_to_with_login_passwd: connect a service 
 * @service:  the service to connect
 * @host: host to connect to
 * @login: login to connect with
 * @passwd:  password to connect with
 * 
 * Connect to a service, but do not use the session
 * default parameters to retrieve server's address
 * 
 **/
void
camel_service_connect_to_with_login_passwd (CamelService *service, gchar *host, gchar *login, gchar *passwd)
{
    CSERV_CLASS(service)->connect_to_with_login_passwd (service, host, login, passwd);
}




/**
 * _connect_to_with_login_passwd_port: connect to the specified address
 * @service: service to connect
 * @host:  host to connect to
 * @login:  user name used to log in
 * @passwd: password used to log in
 * @port: port to connect to
 * 
 * 
 **/
static void
_connect_to_with_login_passwd_port (CamelService *service, gchar *host, gchar *login, gchar *passwd, guint port)
{
    CSERV_CLASS(service)->set_connected(service, TRUE);
}


/**
 * camel_service_connect_to_with_login_passwd_port: connect a service 
 * @service: service to connect
 * @host:  host to connect to
 * @login:  user name used to log in
 * @passwd: password used to log in
 * @port: port to connect to
 * 
 * Connect to a service, but do not use the session
 * default parameters to retrieve server's address
 * 
 **/
void
camel_service_connect_to_with_login_passwd_port (CamelService *service, gchar *host, gchar *login, gchar *passwd, guint port)
{
    CSERV_CLASS(service)->connect_to_with_login_passwd_port (service, host, login, passwd, port);
}




/**
 * _is_connected: test if the service object is connected
 * @service: object to test
 * 
 * 
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
 * 
 * 
 * Return value: 
 **/
gboolean
camel_service_is_connected (CamelService *service)
{
  return CSERV_CLASS(service)->is_connected(service);
}



/**
 * _set_connected:set the connected state 
 * @service: object to set the state of
 * @state: connected/disconnected
 * 
 * This routine has to be called by providers to set the 
 * connection state, mainly when the service is disconnected
 * wheras the close() method has not been called.
 * 
 **/
static void
_set_connected (CamelService *service, gboolean state)
{
  service->connected = state;
}



/**
 * _get_url: get url representing a service
 * @service: the service
 * 
 * This method merely returns the "url" field. Subclasses
 * may provide more active implementations.
 * 
 * 
 * Return value: 
 **/
static const gchar *
_get_url (CamelService *service)
{
	return service->url;
}

/**
 * camel_service_get_url: get the url representing a service
 * @service: the service
 * 
 * returns the URL representing a service. For security reasons 
 * This routine may not always return the password. 
 * 
 * Return value: the url name
 **/
const gchar *
camel_service_get_url (CamelService *service)
{
	return CSERV_CLASS(service)->get_url(service);
}


