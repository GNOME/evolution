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
#include "camel-folder.h"

#define CAMEL_STORE_TYPE     (camel_store_get_type ())
#define CAMEL_STORE(obj)     (GTK_CHECK_CAST((obj), CAMEL_STORE_TYPE, CamelStore))
#define CAMEL_STORE_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_STORE_TYPE, CamelStoreClass))
#define IS_CAMEL_STORE(o)    (GTK_CHECK_TYPE((o), CAMEL_STORE_TYPE))

#ifndef CAMEL_FOLDER_DEF
#define CAMEL_FOLDER_DEF 1
typedef struct _CamelFolder CamelFolder;
#endif /* CAMEL_FOLDER_DEF */

#ifndef CAMEL_STORE_DEF
#define CAMEL_STORE_DEF 1
typedef struct _CamelStore CamelStore;
#endif /* CAMEL_STORE_DEF */

struct _CamelStore
{
	GtkObject parent_object;	
	
	gchar separator;
};



typedef struct {
	GtkObjectClass parent_class;
	void (*set_separator) (CamelStore *store, gchar sep);
	gchar (*get_separator) (CamelStore *store);
	CamelFolder * (*get_folder) (GString *folder_name);
	CamelFolder * (*get_root_folder) (CamelStore *store);
	CamelFolder * (*get_default_folder) (CamelStore *store);

} CamelStoreClass;


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_STORE_H */
