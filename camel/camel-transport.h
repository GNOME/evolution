/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-transport.h : Abstract class for an email transport */

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


#ifndef CAMEL_TRANSPORT_H
#define CAMEL_TRANSPORT_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>
#include "camel-types.h"
#include "camel-service.h"

#define CAMEL_TRANSPORT_TYPE     (camel_transport_get_type ())
#define CAMEL_TRANSPORT(obj)     (GTK_CHECK_CAST((obj), CAMEL_TRANSPORT_TYPE, CamelTransport))
#define CAMEL_TRANSPORT_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_TRANSPORT_TYPE, CamelTransportClass))
#define CAMEL_IS_TRANSPORT(o)    (GTK_CHECK_TYPE((o), CAMEL_TRANSPORT_TYPE))


struct _CamelTransport
{
	CamelService parent_object;

};



typedef struct {
	CamelServiceClass parent_class;

	gboolean (*can_send) (CamelTransport *transport, CamelMedium *message);
	gboolean (*send) (CamelTransport *transport, CamelMedium *message,
			  CamelException *ex);
	gboolean (*send_to) (CamelTransport *transport,
			     CamelMedium *message, GList *recipients,
			     CamelException *ex);
} CamelTransportClass;


/* public methods */
gboolean camel_transport_can_send (CamelTransport *transport,
				   CamelMedium *message);

gboolean camel_transport_send (CamelTransport *transport,
			       CamelMedium *message,
			       CamelException *ex);

gboolean camel_transport_send_to (CamelTransport *transport,
				  CamelMedium *message,
				  GList *recipients,
				  CamelException *ex);

/* Standard Gtk function */
GtkType camel_transport_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_TRANSPORT_H */
