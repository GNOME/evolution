/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-service.h : Abstract class for an email service */

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


#ifndef CAMEL_SERVICE_H
#define CAMEL_SERVICE_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>
#include "camel-types.h"
#include "url-util.h"

#define CAMEL_SERVICE_TYPE     (camel_service_get_type ())
#define CAMEL_SERVICE(obj)     (GTK_CHECK_CAST((obj), CAMEL_SERVICE_TYPE, CamelService))
#define CAMEL_SERVICE_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_SERVICE_TYPE, CamelServiceClass))
#define CAMEL_IS_SERVICE(o)    (GTK_CHECK_TYPE((o), CAMEL_SERVICE_TYPE))



struct _CamelService {
	GtkObject parent_object;

	CamelSession *session;
	gboolean connected;
	Gurl *url;
	int url_flags;

};



typedef struct {
	GtkObjectClass parent_class;

	gboolean  (*connect) (CamelService *service, CamelException *ex);
	gboolean  (*connect_with_url) (CamelService *service, Gurl *url,
				       CamelException *ex);
	gboolean  (*disconnect) (CamelService *service, CamelException *ex);

	gboolean  (*is_connected) (CamelService *service);

} CamelServiceClass;



/* flags for url_flags. (others can be added if needed) */
#define CAMEL_SERVICE_URL_NEED_USER	(1 << 1)
#define CAMEL_SERVICE_URL_NEED_HOST	(1 << 4)
#define CAMEL_SERVICE_URL_NEED_PATH	(1 << 6)



/* public methods */
CamelService *camel_service_new (GtkType type, CamelSession *session,
				 Gurl *url, CamelException *ex);

gboolean camel_service_connect (CamelService *service, CamelException *ex);
gboolean camel_service_connect_with_url (CamelService *service, char *url,
					 CamelException *ex);
gboolean camel_service_disconnect (CamelService *service, CamelException *ex);
gboolean camel_service_is_connected (CamelService *service);
char *camel_service_get_url (CamelService *service);
CamelSession *camel_service_get_session (CamelService *service);

/* Standard Gtk function */
GtkType camel_service_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_SERVICE_H */

