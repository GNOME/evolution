/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifndef __EM_FOLDER_TREE_MODEL_H__
#define __EM_FOLDER_TREE_MODEL_H__

#include <gtk/gtktreednd.h>
#include <gtk/gtktreestore.h>

#include <camel/camel-store.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define EM_TYPE_FOLDER_TREE_MODEL		(em_folder_tree_model_get_type ())
#define EM_FOLDER_TREE_MODEL(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), EM_TYPE_FOLDER_TREE_MODEL, EMFolderTreeModel))
#define EM_FOLDER_TREE_MODEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), EM_TYPE_FOLDER_TREE_MODEL, EMFolderTreeModelClass))
#define EM_IS_FOLDER_TREE_MODEL(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), EM_TYPE_FOLDER_TREE_MODEL))
#define EM_IS_FOLDER_TREE_MODEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), EM_TYPE_FOLDER_TREE_MODEL))
#define EM_FOLDER_TREE_MODEL_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), EM_TYPE_FOLDER_TREE_MODEL, EMFolderTreeModelClass))

typedef struct _EMFolderTreeModel EMFolderTreeModel;
typedef struct _EMFolderTreeModelClass EMFolderTreeModelClass;
typedef struct _EMFolderTreeModelStoreInfo EMFolderTreeModelStoreInfo;

enum {
	COL_STRING_DISPLAY_NAME,  /* string that appears in the tree */
	COL_POINTER_CAMEL_STORE,  /* CamelStore object */
	COL_STRING_FOLDER_PATH,   /* if node is a folder, the full path of the folder */
	COL_STRING_URI,           /* the uri to get the store or
				   * folder object */
	COL_UINT_UNREAD,          /* unread count */
	
	COL_BOOL_IS_STORE,        /* toplevel store node? */
	COL_BOOL_LOAD_SUBDIRS,    /* %TRUE only if the store/folder
				   * has subfolders which have not yet
				   * been added to the tree */
	NUM_COLUMNS
};


struct _EMFolderTreeModelStoreInfo {
	CamelStore *store;
	GtkTreeRowReference *row;
	GHashTable *path_hash;  /* maps CamelFolderInfo::path's to GtkTreeRowReferences */
	
	char *display_name;
	
	unsigned int created_id;
	unsigned int deleted_id;
	unsigned int renamed_id;
	unsigned int subscribed_id;
	unsigned int unsubscribed_id;
};

struct _EMFolderTreeModel {
	GtkTreeStore parent_object;
	
	char *filename;          /* state filename */
	
	GHashTable *store_hash;  /* maps CamelStore's to store-info's */
	GHashTable *uri_hash;    /* maps URI's to GtkTreeRowReferences */
	GHashTable *expanded;    /* saved expanded state from previous session */
};

struct _EMFolderTreeModelClass {
	GtkTreeStoreClass parent_class;
	
	/* signals */
	void     (* loading_row)        (EMFolderTreeModel *model,
					 GtkTreePath *path,
					 GtkTreeIter *iter);
};


GType em_folder_tree_model_get_type (void);


EMFolderTreeModel *em_folder_tree_model_new (const char *evolution_dir);


void em_folder_tree_model_set_folder_info (EMFolderTreeModel *model, GtkTreeIter *iter,
					   struct _EMFolderTreeModelStoreInfo *si,
					   CamelFolderInfo *fi);

void em_folder_tree_model_add_store (EMFolderTreeModel *model, CamelStore *store, const char *display_name);
void em_folder_tree_model_remove_store (EMFolderTreeModel *model, CamelStore *store);
void em_folder_tree_model_remove_folders (EMFolderTreeModel *model, struct _EMFolderTreeModelStoreInfo *si,
					  GtkTreeIter *toplevel);

gboolean em_folder_tree_model_get_expanded (EMFolderTreeModel *model, const char *key);
void em_folder_tree_model_set_expanded (EMFolderTreeModel *model, const char *key, gboolean expanded);
void em_folder_tree_model_save_expanded (EMFolderTreeModel *model);

void em_folder_tree_model_set_unread_count (EMFolderTreeModel *model, CamelStore *store, const char *path, int unread);

/* Drag & Drop stuff */
gboolean em_folder_tree_model_drag_data_received (EMFolderTreeModel *model, GtkTreePath *path, GtkSelectionData *selection,
						  guint info, gboolean move, gboolean *moved);
GdkDragAction em_folder_tree_model_row_drop_possible (EMFolderTreeModel *model, GtkTreePath *path, GList *targets);
GdkAtom em_folder_tree_model_row_drop_target (EMFolderTreeModel *model, GtkTreePath *path, GList *targets);
gboolean em_folder_tree_model_row_draggable (EMFolderTreeModel *model, GtkTreePath *path);
gboolean em_folder_tree_model_drag_data_get (EMFolderTreeModel *model, GtkTreePath *path, GtkSelectionData *selection, guint info);
gboolean em_folder_tree_model_drag_data_delete (EMFolderTreeModel *model, GtkTreePath *path);
void em_folder_tree_model_set_drag_drop_types (EMFolderTreeModel *model, GtkWidget *widget);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EM_FOLDER_TREE_MODEL_H__ */
