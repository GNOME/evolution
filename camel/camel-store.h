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

#include <camel/camel-object.h>
#include <camel/camel-service.h>


typedef struct _CamelFolderInfo {
	struct _CamelFolderInfo *sibling, *child;
	char *url, *full_name, *name;
	int message_count, unread_message_count;
} CamelFolderInfo;


#define CAMEL_STORE_TYPE     (camel_store_get_type ())
#define CAMEL_STORE(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_STORE_TYPE, CamelStore))
#define CAMEL_STORE_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_STORE_TYPE, CamelStoreClass))
#define CAMEL_IS_STORE(o)    (CAMEL_CHECK_TYPE((o), CAMEL_STORE_TYPE))


/* Flags for store flags */
#define CAMEL_STORE_SUBSCRIPTIONS (1 << 0)

struct _CamelStore
{
	CamelService parent_object;

	GHashTable *folders;

	int flags;
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
	void		(*rename_folder)	    (CamelStore *store,
						     const char *old_name,
						     const char *new_name,
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

	CamelFolderInfo *(*get_folder_info)         (CamelStore *store,
						     const char *top,
						     gboolean fast,
						     gboolean recursive,
						     gboolean subscribed_only,
						     CamelException *ex);
	void            (*free_folder_info)         (CamelStore *store,
						     CamelFolderInfo *fi);

	gboolean        (*folder_subscribed)        (CamelStore *store,
						     const char *folder_name);
	void            (*subscribe_folder)         (CamelStore *store,
						     const char *folder_name,
						     CamelException *ex);
	void            (*unsubscribe_folder)       (CamelStore *store,
						     const char *folder_name,
						     CamelException *ex);
} CamelStoreClass;


/* Standard Camel function */
CamelType camel_store_get_type (void);

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
void             camel_store_rename_folder      (CamelStore *store,
						 const char *old_name,
						 const char *new_name,
						 CamelException *ex);

CamelFolderInfo *camel_store_get_folder_info    (CamelStore *store,
						 const char *top,
						 gboolean fast,
						 gboolean recursive,
						 gboolean subscribed_only,
						 CamelException *ex);
void             camel_store_free_folder_info   (CamelStore *store,
						 CamelFolderInfo *fi);

void             camel_store_free_folder_info_full (CamelStore *store,
						    CamelFolderInfo *fi);
void             camel_store_free_folder_info_nop  (CamelStore *store,
						    CamelFolderInfo *fi);

void             camel_folder_info_free            (CamelFolderInfo *fi);
void             camel_folder_info_build           (GPtrArray *folders,
						    CamelFolderInfo *top,
						    char separator,
						    gboolean short_names);

gboolean         camel_store_supports_subscriptions   (CamelStore *store);

gboolean         camel_store_folder_subscribed        (CamelStore *store,
						       const char *folder_name);
void             camel_store_subscribe_folder         (CamelStore *store,
						       const char *folder_name,
						       CamelException *ex);
void             camel_store_unsubscribe_folder       (CamelStore *store,
						       const char *folder_name,
						       CamelException *ex);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_STORE_H */
