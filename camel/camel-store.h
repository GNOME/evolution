/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-store.h : Abstract class for an email store */

/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999, 2000 HelixCode (http://www.helixcode.com) .
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


#ifndef CAMEL_STORE_H
#define CAMEL_STORE_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>
#include "camel-types.h"
#include "camel-service.h"

#define CAMEL_STORE_TYPE     (camel_store_get_type ())
#define CAMEL_STORE(obj)     (GTK_CHECK_CAST((obj), CAMEL_STORE_TYPE, CamelStore))
#define CAMEL_STORE_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_STORE_TYPE, CamelStoreClass))
#define CAMEL_IS_STORE(o)    (GTK_CHECK_TYPE((o), CAMEL_STORE_TYPE))


struct _CamelStore
{
	CamelService parent_object;	
	
	CamelSession *session;
	/*  gchar *url_name; */
	gchar separator;
};



typedef struct {
	CamelServiceClass parent_class;
	
	void                (*init)                 (CamelStore *store, 
						     CamelSession *session, 
						     const gchar *url_name, 
						     CamelException *ex);
	void                (*set_separator)        (CamelStore *store, gchar sep, 
						     CamelException *ex);
	gchar               (*get_separator)        (CamelStore *store, 
						     CamelException *ex);
	CamelFolder *       (*get_folder)           (CamelStore *store, 
						     const gchar *folder_name, 
						     CamelException *ex);
	CamelFolder *       (*get_root_folder)      (CamelStore *store, 
						     CamelException *ex);
	CamelFolder *       (*get_default_folder)   (CamelStore *store, 
						     CamelException *ex);

} CamelStoreClass;


/* public methods */

/* Standard Gtk function */
GtkType camel_store_get_type (void);

void             camel_store_init             (CamelStore *store, CamelSession *session, const gchar *url_name, CamelException *ex);
CamelFolder *    camel_store_get_folder       (CamelStore *store, const gchar *folder_name, CamelException *ex);
gchar            camel_store_get_separator    (CamelStore *store, CamelException *ex);
CamelSession *   camel_store_get_session      (CamelStore *store, CamelException *ex);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_STORE_H */
