/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelService.c : Abstract class for an email service */

/* 
 *
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@inria.fr> .
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

#include "camel-service.h"

static GtkObjectClass *camel_service_parent_class=NULL;

/* Returns the class for a CamelService */
#define CSERV_CLASS(so) CAMEL_SERVICE_CLASS (GTK_OBJECT(so)->klass)

static void camel_service_connect(CamelService *service);
static void camel_service_connect_to_with_login_passwd(CamelService *service, GString *host, GString *login, GString *passwd);
static void camel_service_connect_to_with_login_passwd_port(CamelService *service, GString *host, GString *login, GString *passwd, guint port);
static gboolean camel_service_is_connected(CamelService *service);
static void camel_service_set_connected(CamelService *service, gboolean state);

static void
camel_service_class_init (CamelServiceClass *camel_service_class)
{
	camel_service_parent_class = gtk_type_class (gtk_object_get_type ());
	
	/* virtual method definition */
	camel_service_class->connect = camel_service_connect;
	camel_service_class->connect_to_with_login_passwd = camel_service_connect_to_with_login_passwd;
	camel_service_class->connect_to_with_login_passwd_port = camel_service_connect_to_with_login_passwd_port;
	camel_service_class->is_connected = camel_service_is_connected;
	camel_service_class->set_connected = camel_service_set_connected;
 

	/* virtual method overload */
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





/**
 * camel_service_connect : connect to a service 
 *
 * connect to the service using the parameters 
 * stored in the session it is initialized with
 * WARNING: session not implemented for the moment
 *
 * @service: object to connect
 **/
static void
camel_service_connect(CamelService *service)
{

}



/**
 * camel_service_connect_to:login:password : connect to the specified address
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
camel_service_connect_to_with_login_passwd(CamelService *service, GString *host, GString *login, GString *passwd)
{
  camel_service_set_connected(service, TRUE);
}


/**
 * camel_service_connect_to:login:password : connect to the specified address
 * 
 * Connect to the service, but do not use the session
 * default parameters to retrieve server's address
 *
 * @service: object to connect
 * @host: host to connect to
 * @login: user name used to log in
 * @passwd: password used to log in
 * @port: port to connect to
 *
 **/
static void
camel_service_connect_to_with_login_passwd_port(CamelService *service, GString *host, GString *login, GString *passwd, guint port)
{
  camel_service_set_connected(service, TRUE);
}




/**
 * camel_service_is_connected: test if the service object is connected
 *
 *
 * @service: object to test
 *  
 **/
static gboolean
camel_service_is_connected(CamelService *service)
{
  return service->connected;
}


/**
 * camel_service_set_connected: set the connected state
 * 
 * This routine has to be called by providers to set the 
 * connection state, mainly when the service is disconnected
 * wheras the close() method has not been called.
 *
 * @service: object to set the state of
 * @state: connected/disconnected
 *  
 **/
static void
camel_service_set_connected(CamelService *service, gboolean state)
{
  service->connected = state;
}


