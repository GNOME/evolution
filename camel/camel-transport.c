/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-transport.c : Abstract class for an email transport */

/* 
 *
 * Author : 
 *  Dan Winship <danw@ximian.com>
 *
 * Copyright 2000 Ximian, Inc. (www.ximian.com)
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

#include "camel-transport.h"
#include "camel-exception.h"
#include "camel-private.h"

/* Returns the class for a CamelTransport */
#define CT_CLASS(so) CAMEL_TRANSPORT_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static void
camel_transport_init (gpointer object, gpointer klass)
{
	CamelTransport *xport = object;
	
	xport->priv = g_malloc0 (sizeof (struct _CamelTransportPrivate));
#ifdef ENABLE_THREADS
	xport->priv->send_lock = g_mutex_new ();
#endif
}

static void
camel_transport_finalize (CamelObject *object)
{
	CamelTransport *xport = CAMEL_TRANSPORT (object);
	
#ifdef ENABLE_THREADS
	g_mutex_free (xport->priv->send_lock);
#endif
	g_free (xport->priv);
}

CamelType
camel_transport_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (CAMEL_SERVICE_TYPE,
					    "CamelTransport",
					    sizeof (CamelTransport),
					    sizeof (CamelTransportClass),
					    NULL,
					    NULL,
					    (CamelObjectInitFunc) camel_transport_init,
					    (CamelObjectFinalizeFunc) camel_transport_finalize);
	}
	
	return type;
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
	gboolean sent;
	
	g_return_val_if_fail (CAMEL_IS_TRANSPORT (transport), FALSE);
	
	CAMEL_TRANSPORT_LOCK (transport, send_lock);
	sent = CT_CLASS (transport)->send (transport, message, ex);
	CAMEL_TRANSPORT_UNLOCK (transport, send_lock);
	
	return sent;
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
			 CamelAddress *recipients, CamelException *ex)
{
	gboolean sent;
	
	g_return_val_if_fail (CAMEL_IS_TRANSPORT (transport), FALSE);
	
	CAMEL_TRANSPORT_LOCK (transport, send_lock);
	sent = CT_CLASS (transport)->send_to (transport, message,
					      recipients, ex);
	CAMEL_TRANSPORT_UNLOCK (transport, send_lock);
	
	return sent;
}
