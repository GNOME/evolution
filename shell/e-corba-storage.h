/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-corba-storage.h
 *
 * Copyright (C) 2000 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */

#ifndef __E_CORBA_STORAGE_H__
#define __E_CORBA_STORAGE_H__

#include "e-storage.h"

#include "Evolution.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_CORBA_STORAGE		(e_corba_storage_get_type ())
#define E_CORBA_STORAGE(obj)		(GTK_CHECK_CAST ((obj), E_TYPE_CORBA_STORAGE, ECorbaStorage))
#define E_CORBA_STORAGE_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_CORBA_STORAGE, ECorbaStorageClass))
#define E_IS_CORBA_STORAGE(obj)		(GTK_CHECK_TYPE ((obj), E_TYPE_CORBA_STORAGE))
#define E_IS_CORBA_STORAGE_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_CORBA_STORAGE))


typedef struct _ECorbaStorage        ECorbaStorage;
typedef struct _ECorbaStoragePrivate ECorbaStoragePrivate;
typedef struct _ECorbaStorageClass   ECorbaStorageClass;

struct _ECorbaStorage {
	EStorage parent;

	ECorbaStoragePrivate *priv;
};

struct _ECorbaStorageClass {
	EStorageClass parent_class;
};


struct _ECorbaStoragePropertyItem {
	char *label;
	char *tooltip;
	GdkPixbuf *icon;
};
typedef struct _ECorbaStoragePropertyItem ECorbaStoragePropertyItem;


GtkType   e_corba_storage_get_type   (void);
void      e_corba_storage_construct  (ECorbaStorage                 *corba_storage,
				      const GNOME_Evolution_Storage  storage_interface,
				      const char                    *name);
EStorage *e_corba_storage_new        (const GNOME_Evolution_Storage  storage_interface,
				      const char                    *name);

GNOME_Evolution_Storage e_corba_storage_get_corba_objref (ECorbaStorage *corba_storage);

const GNOME_Evolution_StorageListener  e_corba_storage_get_StorageListener  (ECorbaStorage *corba_storage);

GSList *e_corba_storage_get_folder_property_items  (ECorbaStorage *corba_storage);
void    e_corba_storage_free_property_items_list   (GSList        *list);

void e_corba_storage_show_folder_properties (ECorbaStorage *corba_storage,
					     const char *path,
					     int property_item_id,
					     GdkWindow *parent_window);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_CORBA_STORAGE_H__ */
