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
#include "camel-address.h"
#include "camel-mime-message.h"
#include "camel-private.h"

static CamelServiceClass *parent_class = NULL;

/* Returns the class for a CamelTransport */
#define CT_CLASS(so) CAMEL_TRANSPORT_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static int transport_setv (CamelObject *object, CamelException *ex, CamelArgV *args);
static int transport_getv (CamelObject *object, CamelException *ex, CamelArgGetV *args);


static void
camel_transport_class_init (CamelTransportClass *camel_transport_class)
{
	CamelObjectClass *camel_object_class = CAMEL_OBJECT_CLASS (camel_transport_class);
	
	parent_class = CAMEL_SERVICE_CLASS (camel_type_get_global_classfuncs (camel_service_get_type ()));
	
	/* virtual method overload */
	camel_object_class->setv = transport_setv;
	camel_object_class->getv = transport_getv;
}

static void
camel_transport_init (gpointer object, gpointer klass)
{
	CamelTransport *xport = object;
	
	xport->priv = g_malloc0 (sizeof (struct _CamelTransportPrivate));
	xport->priv->send_lock = g_mutex_new ();
}

static void
camel_transport_finalize (CamelObject *object)
{
	CamelTransport *xport = CAMEL_TRANSPORT (object);
	
	g_mutex_free (xport->priv->send_lock);
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
					    (CamelObjectClassInitFunc) camel_transport_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_transport_init,
					    (CamelObjectFinalizeFunc) camel_transport_finalize);
	}
	
	return type;
}


static int
transport_setv (CamelObject *object, CamelException *ex, CamelArgV *args)
{
	/* CamelTransport doesn't currently have anything to set */
	return CAMEL_OBJECT_CLASS (parent_class)->setv (object, ex, args);
}

static int
transport_getv (CamelObject *object, CamelException *ex, CamelArgGetV *args)
{
	/* CamelTransport doesn't currently have anything to get */
	return CAMEL_OBJECT_CLASS (parent_class)->getv (object, ex, args);
}


/**
 * camel_transport_send_to:
 * @transport: the transport
 * @message: the message
 * @from: from address
 * @recipients: the recipients
 * @ex: a CamelException
 *
 * Sends the message to the given recipients, regardless of the contents
 * of @message. If the message contains a "Bcc" header, the transport
 * is responsible for stripping it.
 *
 * Return value: success or failure.
 **/
gboolean
camel_transport_send_to (CamelTransport *transport, CamelMimeMessage *message,
			 CamelAddress *from, CamelAddress *recipients,
			 CamelException *ex)
{
	gboolean sent;
	
	g_return_val_if_fail (CAMEL_IS_TRANSPORT (transport), FALSE);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), FALSE);
	g_return_val_if_fail (CAMEL_IS_ADDRESS (from), FALSE);
	g_return_val_if_fail (CAMEL_IS_ADDRESS (recipients), FALSE);
	
	CAMEL_TRANSPORT_LOCK (transport, send_lock);
	sent = CT_CLASS (transport)->send_to (transport, message,
					      from, recipients, ex);
	CAMEL_TRANSPORT_UNLOCK (transport, send_lock);
	
	return sent;
}
