/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-store.h : Abstract class for an email store */

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


#ifndef CAMEL_STORE_H
#define CAMEL_STORE_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <camel/camel-service.h>

#define CAMEL_STORE_TYPE     (camel_store_get_type ())
#define CAMEL_STORE(obj)     (GTK_CHECK_CAST((obj), CAMEL_STORE_TYPE, CamelStore))
#define CAMEL_STORE_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_STORE_TYPE, CamelStoreClass))
#define CAMEL_IS_STORE(o)    (GTK_CHECK_TYPE((o), CAMEL_STORE_TYPE))


struct _CamelStore
{
	CamelService parent_object;

	GHashTable *folders;

};



typedef struct {
	CamelServiceClass parent_class;

	CamelFolder *   (*get_folder)               (CamelStore *store,
						     const char *folder_name,
						     gboolean create,
						     CamelException *ex);

	void            (*delete_folder)            (CamelStore *store,
						     const char *folder_name,
						     CamelException *ex);

	char *          (*get_folder_name)          (CamelStore *store,
						     const char *folder_name,
						     CamelException *ex);
	char *          (*get_root_folder_name)     (CamelStore *store,
						     CamelException *ex);
	char *          (*get_default_folder_name)  (CamelStore *store,
						     CamelException *ex);

        CamelFolder *   (*lookup_folder)            (CamelStore *store,
						     const char *folder_name);
	void            (*cache_folder)             (CamelStore *store,
						     const char *folder_name,
						     CamelFolder *folder);
        void            (*uncache_folder)           (CamelStore *store,
						     CamelFolder *folder);

} CamelStoreClass;


/* Standard Gtk function */
GtkType camel_store_get_type (void);

/* public methods */
CamelFolder *    camel_store_get_folder         (CamelStore *store,
					         const char *folder_name,
						 gboolean create,
					         CamelException *ex);
CamelFolder *    camel_store_get_root_folder    (CamelStore *store,
					         CamelException *ex);
CamelFolder *    camel_store_get_default_folder (CamelStore *store,
						 CamelException *ex);

void             camel_store_delete_folder      (CamelStore *store,
						 const char *folder_name,
						 CamelException *ex);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_STORE_H */
