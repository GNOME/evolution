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

#include <camel/camel-object.h>
#include <camel/camel-url.h>
#include <netdb.h>

#define CAMEL_SERVICE_TYPE     (camel_service_get_type ())
#define CAMEL_SERVICE(obj)     (GTK_CHECK_CAST((obj), CAMEL_SERVICE_TYPE, CamelService))
#define CAMEL_SERVICE_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_SERVICE_TYPE, CamelServiceClass))
#define CAMEL_IS_SERVICE(o)    (GTK_CHECK_TYPE((o), CAMEL_SERVICE_TYPE))


struct _CamelService {
	CamelObject parent_object;

	CamelSession *session;
	gboolean connected;
	CamelURL *url;
	int url_flags;
};


typedef struct {
	CamelObjectClass parent_class;

	gboolean  (*connect)           (CamelService *service, 
					CamelException *ex);
	gboolean  (*disconnect)        (CamelService *service, 
					CamelException *ex);

	gboolean  (*is_connected)      (CamelService *service);

	GList *   (*query_auth_types)  (CamelService *service,
					CamelException *ex);
	void      (*free_auth_types)   (CamelService *service,
					GList *authtypes);

	char *    (*get_name)          (CamelService *service,
					gboolean brief);

} CamelServiceClass;



/* flags for url_flags. (others can be added if needed) */
#define CAMEL_SERVICE_URL_NEED_USER	(1 << 1)
#define CAMEL_SERVICE_URL_NEED_AUTH	(1 << 2)
#define CAMEL_SERVICE_URL_NEED_HOST	(1 << 4)
#define CAMEL_SERVICE_URL_NEED_PATH	(1 << 6)


/* query_auth_types returns a GList of these */
typedef struct {
	char *name, *description, *authproto;
	gboolean need_password;
} CamelServiceAuthType;


/* public methods */
CamelService *      camel_service_new                (GtkType type, 
						      CamelSession *session,
						      CamelURL *url, 
						      CamelException *ex);

gboolean            camel_service_connect            (CamelService *service, 
						      CamelException *ex);
gboolean            camel_service_disconnect         (CamelService *service, 
                                                      CamelException *ex);
gboolean            camel_service_is_connected       (CamelService *service);

char *              camel_service_get_url            (CamelService *service);
char *              camel_service_get_name           (CamelService *service,
						      gboolean brief);
CamelSession *      camel_service_get_session        (CamelService *service);

GList *             camel_service_query_auth_types   (CamelService *service,
						      CamelException *ex);
void                camel_service_free_auth_types    (CamelService *service,
						      GList *authtypes);

/* convenience functions */
struct hostent *    camel_service_gethost            (CamelService *service,
						      CamelException *ex);


/* Standard Gtk function */
GtkType camel_service_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_SERVICE_H */

