/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-disco-store.h: abstruct class for a disconnectable store */

/* 
 * Authors: Dan Winship <danw@ximian.com>
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
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


#ifndef CAMEL_DISCO_STORE_H
#define CAMEL_DISCO_STORE_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-store.h>

#define CAMEL_DISCO_STORE_TYPE     (camel_disco_store_get_type ())
#define CAMEL_DISCO_STORE(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_DISCO_STORE_TYPE, CamelDiscoStore))
#define CAMEL_DISCO_STORE_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_DISCO_STORE_TYPE, CamelDiscoStoreClass))
#define CAMEL_IS_DISCO_STORE(o)    (CAMEL_CHECK_TYPE((o), CAMEL_DISCO_STORE_TYPE))

enum {
	CAMEL_DISCO_STORE_ARG_FIRST  = CAMEL_STORE_ARG_FIRST + 100,
};

typedef enum {
	CAMEL_DISCO_STORE_ONLINE,
	CAMEL_DISCO_STORE_OFFLINE,
	CAMEL_DISCO_STORE_RESYNCING
} CamelDiscoStoreStatus;

struct _CamelDiscoStore {
	CamelStore parent_object;	

	CamelDiscoStoreStatus status;
	CamelDiscoDiary *diary;
};


typedef struct {
	CamelStoreClass parent_class;

	void              (*set_status)              (CamelDiscoStore *,
						      CamelDiscoStoreStatus,
						      CamelException *);
	gboolean          (*can_work_offline)        (CamelDiscoStore *);


	gboolean          (*connect_online)          (CamelService *,
						      CamelException *);
	gboolean          (*connect_offline)         (CamelService *,
						      CamelException *);

	gboolean          (*disconnect_online)       (CamelService *, gboolean,
						      CamelException *);
	gboolean          (*disconnect_offline)      (CamelService *, gboolean,
						      CamelException *);

	CamelFolder *     (*get_folder_online)       (CamelStore *store,
						      const char *name,
						      guint32 flags,
						      CamelException *ex);
	CamelFolder *     (*get_folder_offline)      (CamelStore *store,
						      const char *name,
						      guint32 flags,
						      CamelException *ex);
	CamelFolder *     (*get_folder_resyncing)    (CamelStore *store,
						      const char *name,
						      guint32 flags,
						      CamelException *ex);

	CamelFolderInfo * (*get_folder_info_online)    (CamelStore *store,
							const char *top,
							guint32 flags,
							CamelException *ex);
	CamelFolderInfo * (*get_folder_info_offline)   (CamelStore *store,
							const char *top,
							guint32 flags,
							CamelException *ex);
	CamelFolderInfo * (*get_folder_info_resyncing) (CamelStore *store,
							const char *top,
							guint32 flags,
							CamelException *ex);

} CamelDiscoStoreClass;


/* Standard Camel function */
CamelType camel_disco_store_get_type (void);

/* Public methods */
CamelDiscoStoreStatus camel_disco_store_status           (CamelDiscoStore *);
void                  camel_disco_store_set_status       (CamelDiscoStore *,
							  CamelDiscoStoreStatus,
							  CamelException *);
gboolean              camel_disco_store_can_work_offline (CamelDiscoStore *);


/* Convenience functions */
gboolean camel_disco_store_check_online (CamelDiscoStore *store, CamelException *ex);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_DISCO_STORE_H */
