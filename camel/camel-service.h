/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-service.h : Abstract class for an email service */

/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <Bertrand.Guiheneuf@aful.org>
 *
 * Copyright 1999 International GNOME Support (http://www.gnome-support.com) .
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

#define CAMEL_SERVICE_TYPE     (camel_service_get_type ())
#define CAMEL_SERVICE(obj)     (GTK_CHECK_CAST((obj), CAMEL_SERVICE_TYPE, CamelService))
#define CAMEL_SERVICE_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_SERVICE_TYPE, CamelServiceClass))
#define IS_CAMEL_SERVICE(o)    (GTK_CHECK_TYPE((o), CAMEL_SERVICE_TYPE))



typedef struct {
	GtkObject parent_object;

	gboolean connected;
	gchar *url; /* This may be a full object ? */

} CamelService;



typedef struct {
	GtkObjectClass parent_class;

	void  (*connect) (CamelService *service);
	void  (*connect_to_with_login_passwd) (CamelService *service, gchar *host, gchar *login, gchar *passwd);
	void  (*connect_to_with_login_passwd_port) (CamelService *service, gchar *host, gchar *login, gchar *passwd, guint port);
	gboolean  (*is_connected) (CamelService *service);
	void  (*set_connected) (CamelService *service, gboolean state);
	const gchar *  (*get_url) (CamelService *service);
	
} CamelServiceClass;




/* public methods */
void camel_service_connect (CamelService *service);
gboolean camel_service_is_connected (CamelService *service);
void camel_service_connect_to_with_login_passwd (CamelService *service, gchar *host, gchar *login, gchar *passwd);
void camel_service_connect_to_with_login_passwd_port (CamelService *service, gchar *host, gchar *login, gchar *passwd, guint port);
const gchar *camel_service_get_url (CamelService *service);
/* Standard Gtk function */
GtkType camel_service_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_SERVICE_H */

