/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-storage-set-store.h
 *
 * Copyright (C) 2002  Ximian, Inc.
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
 * Author: Mike Kestner
 */

#ifndef __E_STORAGE_SET_STORE_H__
#define __E_STORAGE_SET_STORE_H__

#include <gtk/gtktreemodel.h>
#include "e-storage-set.h"
#include "e-storage-set-store.h"

G_BEGIN_DECLS

#define E_STORAGE_SET_STORE_TYPE	    (e_storage_set_store_get_type ())
#define E_STORAGE_SET_STORE(obj)	    (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_STORAGE_SET_STORE_TYPE, EStorageSetStore))
#define E_STORAGE_SET_STORE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_STORAGE_SET_STORE_TYPE, EStorageSetStoreClass))
#define E_IS_STORAGE_SET_STORE(obj)	    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_STORAGE_SET_STORE_TYPE))
#define E_IS_STORAGE_SET_STORE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), E_STORAGE_SET_STORE_TYPE))
#define E_STORAGE_SET_STORE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), E_STORAGE_SET_STORE_TYPE, EStorageSetStoreClass))

typedef gboolean (* EStorageSetStoreHasCheckBoxFunc)  (EStorageSet *storage_set,
						       const char  *path,
						       void        *data);

typedef enum {
	E_STORAGE_SET_STORE_COLUMN_NAME,
	E_STORAGE_SET_STORE_COLUMN_HIGHLIGHT,
	E_STORAGE_SET_STORE_COLUMN_CHECKED,
	E_STORAGE_SET_STORE_COLUMN_CHECKABLE,
	E_STORAGE_SET_STORE_COLUMN_ICON,
	E_STORAGE_SET_STORE_COLUMN_COUNT
} E_STORAGE_SET_STORE_COLUMN_TYPE;

typedef struct _EStorageSetStore        EStorageSetStore;
typedef struct _EStorageSetStorePrivate EStorageSetStorePrivate;
typedef struct _EStorageSetStoreClass   EStorageSetStoreClass;

struct _EStorageSetStore {
	GObject parent;

	EStorageSetStorePrivate *priv;
};

struct _EStorageSetStoreClass {
	GObjectClass parent_class;
};


GType e_storage_set_store_get_type (void);

EStorageSetStore *e_storage_set_store_new (EStorageSet *storage_set, gboolean show_folders);

EStorageSet *e_storage_set_store_get_storage_set (EStorageSetStore *storage_set_store);

void e_storage_set_store_set_checkboxes_list (EStorageSetStore *storage_set_store,
					      GSList *checkboxes);
GSList *e_storage_set_store_get_checkboxes_list (EStorageSetStore *storage_set_store);

void e_storage_set_store_set_allow_dnd (EStorageSetStore *storage_set_store,
					gboolean allow_dnd);
gboolean e_storage_set_store_get_allow_dnd (EStorageSetStore *storage_set_store);
						    
GtkTreePath *e_storage_set_store_get_tree_path (EStorageSetStore *store, const gchar *folder_path);

const gchar *e_storage_set_store_get_folder_path (EStorageSetStore *store, GtkTreePath *tree_path);

void e_storage_set_store_set_has_checkbox_func (EStorageSetStore *storage_set_store,
					        EStorageSetStoreHasCheckBoxFunc func,
					        gpointer data);

G_END_DECLS

#endif /* __E_STORAGE_SET_STORE_H__ */
