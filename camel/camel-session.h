/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-session.h : Abstract class for an email session */

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


#ifndef CAMEL_SESSION_H
#define CAMEL_SESSION_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>

typedef struct _CamelSession CamelSession;

#include "camel-provider.h"
#include "camel-store.h"

#define CAMEL_SESSION_TYPE     (camel_session_get_type ())
#define CAMEL_SESSION(obj)     (GTK_CHECK_CAST((obj), CAMEL_SESSION_TYPE, CamelSession))
#define CAMEL_SESSION_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_SESSION_TYPE, CamelSessionClass))
#define IS_CAMEL_SESSION(o)    (GTK_CHECK_TYPE((o), CAMEL_SESSION_TYPE))




struct _CamelSession
{
	GtkObject parent_object;
	GHashTable *store_provider_list; /* providers are identified by their protocol */
	GHashTable *transport_provider_list; 
	
	
};



typedef struct {
	GtkObjectClass parent_class;
	
	/* Virtual methods */	

} CamelSessionClass;


/* public methods */

/* Standard Gtk function */
GtkType camel_session_get_type (void);


CamelSession *camel_session_new ();
void camel_session_set_provider (CamelSession *session, CamelProvider *provider);
CamelStore *camel_session_get_store_for_protocol (CamelSession *session, const gchar *protocol);
CamelStore *camel_session_get_store (CamelSession *session, const gchar *url_string);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_SESSION_H */
