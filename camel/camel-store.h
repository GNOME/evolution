/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-store.h : Abstract class for an email store */

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


#ifndef CAMEL_STORE_H
#define CAMEL_STORE_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>

typedef struct _CamelStore CamelStore;

#include "camel-folder.h"
#include "camel-service.h"
#include "camel-session.h"

#define CAMEL_STORE_TYPE     (camel_store_get_type ())
#define CAMEL_STORE(obj)     (GTK_CHECK_CAST((obj), CAMEL_STORE_TYPE, CamelStore))
#define CAMEL_STORE_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_STORE_TYPE, CamelStoreClass))
#define IS_CAMEL_STORE(o)    (GTK_CHECK_TYPE((o), CAMEL_STORE_TYPE))


struct _CamelStore
{
	CamelService parent_object;	
	
	CamelSession *session;
	gchar *url_name;
	gchar separator;
};



typedef struct {
	CamelServiceClass parent_class;
	
	void (*init) (CamelStore *store, CamelSession *session, gchar *url_name);
	void (*set_separator) (CamelStore *store, gchar sep);
	gchar (*get_separator) (CamelStore *store);
	CamelFolder * (*get_folder) (CamelStore *store, gchar *folder_name);
	CamelFolder * (*get_root_folder) (CamelStore *store);
	CamelFolder * (*get_default_folder) (CamelStore *store);

} CamelStoreClass;


/* public methods */

/* Standard Gtk function */
GtkType camel_store_get_type (void);

void camel_store_init(CamelStore *store, CamelSession *session, gchar *url_name);
CamelFolder *camel_store_get_folder(CamelStore *store, gchar *folder_name);
gchar camel_store_get_separator(CamelStore *store);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_STORE_H */
