/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>  
 *
 *
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __EM_FOLDER_TREE_MODEL_H__
#define __EM_FOLDER_TREE_MODEL_H__

#include <gtk/gtk.h>

#include <libxml/tree.h>

#include <camel/camel-store.h>

#include <libedataserver/e-account-list.h>

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
	COL_STRING_FULL_NAME,   /* if node is a folder, the full path name of the folder, no leading / */
	COL_STRING_URI,           /* the uri to get the store or folder object */
	COL_UINT_UNREAD,          /* unread count */
	COL_UINT_FLAGS,		/* FolderInfo.flags */

	COL_BOOL_IS_STORE,        /* toplevel store node? */
	COL_BOOL_LOAD_SUBDIRS,    /* %TRUE only if the store/folder
				   * has subfolders which have not yet
				   * been added to the tree */
	NUM_COLUMNS
};


struct _EMFolderTreeModelStoreInfo {
	CamelStore *store;
	GtkTreeRowReference *row;
	GHashTable *full_hash;  /* maps CamelFolderInfo::full_name's to GtkTreeRowReferences */
	EAccount *account;

	char *display_name;

	unsigned int created_id;
	unsigned int deleted_id;
	unsigned int renamed_id;
	unsigned int subscribed_id;
	unsigned int unsubscribed_id;
};

struct _EMFolderTreeModel {
	GtkTreeStore parent_object;

	char *filename;            /* state filename */
	xmlDocPtr state;           /* saved expanded state from previous session */

	GHashTable *store_hash;    /* maps CamelStore's to store-info's */
	GHashTable *uri_hash;      /* maps URI's to GtkTreeRowReferences */

	EAccountList *accounts;
	GHashTable *account_hash;  /* maps accounts to store-info's */
	gulong account_changed_id;
	gulong account_removed_id;
};

struct _EMFolderTreeModelClass {
	GtkTreeStoreClass parent_class;

	/* signals */
	void     (* loading_row)        (EMFolderTreeModel *model,
					 GtkTreePath *path,
					 GtkTreeIter *iter);

	void     (* loaded_row)         (EMFolderTreeModel *model,
					 GtkTreePath *path,
					 GtkTreeIter *iter);

	void     (* folder_added)       (EMFolderTreeModel *model,
					 const char *path,
					 const char *uri);

	void     (* store_added)        (EMFolderTreeModel *model,
					 const char *uri);
};


GType em_folder_tree_model_get_type (void);


EMFolderTreeModel *em_folder_tree_model_new (const char *evolution_dir);


void em_folder_tree_model_set_folder_info (EMFolderTreeModel *model, GtkTreeIter *iter,
					   struct _EMFolderTreeModelStoreInfo *si,
					   CamelFolderInfo *fi, int fully_loaded);

void em_folder_tree_model_add_store (EMFolderTreeModel *model, CamelStore *store, const char *display_name);
void em_folder_tree_model_remove_store (EMFolderTreeModel *model, CamelStore *store);
void em_folder_tree_model_remove_folders (EMFolderTreeModel *model, struct _EMFolderTreeModelStoreInfo *si,
					  GtkTreeIter *toplevel);

char *em_folder_tree_model_get_selected (EMFolderTreeModel *model);
void em_folder_tree_model_set_selected (EMFolderTreeModel *model, const char *uri);

gboolean em_folder_tree_model_get_expanded (EMFolderTreeModel *model, const char *key);
void em_folder_tree_model_set_expanded (EMFolderTreeModel *model, const char *key, gboolean expanded);

gboolean em_folder_tree_model_get_expanded_uri (EMFolderTreeModel *model, const char *uri);
void em_folder_tree_model_set_expanded_uri (EMFolderTreeModel *model, const char *uri, gboolean expanded);

void em_folder_tree_model_save_state (EMFolderTreeModel *model);

typedef void (* EMFTModelExpandFunc) (EMFolderTreeModel *model, const char *path, void *user_data);
void em_folder_tree_model_expand_foreach (EMFolderTreeModel *model, EMFTModelExpandFunc func, void *user_data);

void em_folder_tree_model_set_unread_count (EMFolderTreeModel *model, CamelStore *store, const char *path, int unread);
gboolean em_folder_tree_model_is_type_inbox (EMFolderTreeModel *model, CamelStore *store, const char *full);
char * em_folder_tree_model_get_folder_name (EMFolderTreeModel *model, CamelStore *store, const char *full);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EM_FOLDER_TREE_MODEL_H__ */
