/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-transport.c : Abstract class for an email transport */

/* 
 *
 * Author : 
 *  Dan Winship <danw@helixcode.com>
 *
 * Copyright 2000 Helix Code, Inc. (http://www.helixcode.com)
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
#include "camel-transport.h"
#include "camel-exception.h"
#include "camel-log.h"

/* Returns the class for a CamelTransport */
#define CT_CLASS(so) CAMEL_TRANSPORT_CLASS (GTK_OBJECT(so)->klass)

GtkType
camel_transport_get_type (void)
{
	static GtkType camel_transport_type = 0;
	
	if (!camel_transport_type)	{
		GtkTypeInfo camel_transport_info =	
		{
			"CamelTransport",
			sizeof (CamelTransport),
			sizeof (CamelTransportClass),
			(GtkClassInitFunc) NULL,
			(GtkObjectInitFunc) NULL,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_transport_type = gtk_type_unique (CAMEL_SERVICE_TYPE, &camel_transport_info);
	}
	
	return camel_transport_type;
}


/**
 * camel_transport_can_send: Determine if a message is send-able on a transport
 * @transport: the transport
 * @message: the message
 *
 * Determines if a CamelMedium is of an appropriate subclass to send
 * via the given @transport. (Mail transports are not able to send
 * netnews articles, and vice versa.)
 *
 * Return value: TRUE or FALSE
 **/
gboolean
camel_transport_can_send (CamelTransport *transport, CamelMedium *message)
{
	return CT_CLASS (transport)->can_send (transport, message);
}

/**
 * camel_transport_send: Send a message via a transport
 * @transport: the transport
 * @message: the message
 * @ex: a CamelException
 *
 * Sends the message to the recipients indicated in the message.
 *
 * Return value: success or failure.
 **/
gboolean
camel_transport_send (CamelTransport *transport, CamelMedium *message,
		      CamelException *ex)
{
	return CT_CLASS (transport)->send (transport, message, ex);
}

/**
 * camel_transport_send_to: Send a message non-standard recipients
 * @transport: the transport
 * @message: the message
 * @recipients: the recipients
 * @ex: a CamelException
 *
 * Sends the message to the given recipients, rather than to the
 * recipients indicated in the message.
 *
 * Return value: success or failure.
 **/
gboolean
camel_transport_send_to (CamelTransport *transport, CamelMedium *message,
			 GList *recipients, CamelException *ex)
{
	return CT_CLASS (transport)->send_to (transport, message,
					      recipients, ex);
}
