/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EM_FOLDER_TREE_MODEL_H
#define EM_FOLDER_TREE_MODEL_H

#include <gtk/gtk.h>
#include <camel/camel.h>

#include <libemail-engine/libemail-engine.h>

/* Standard GObject macros */
#define EM_TYPE_FOLDER_TREE_MODEL \
	(em_folder_tree_model_get_type ())
#define EM_FOLDER_TREE_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_FOLDER_TREE_MODEL, EMFolderTreeModel))
#define EM_FOLDER_TREE_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_FOLDER_TREE_MODEL, EMFolderTreeModelClass))
#define EM_IS_FOLDER_TREE_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_FOLDER_TREE_MODEL))
#define EM_IS_FOLDER_TREE_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_FOLDER_TREE_MODEL))
#define EM_FOLDER_TREE_MODEL_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_FOLDER_TREE_MODEL, EMFolderTreeModelClass))

G_BEGIN_DECLS

typedef struct _EMFolderTreeModel EMFolderTreeModel;
typedef struct _EMFolderTreeModelClass EMFolderTreeModelClass;
typedef struct _EMFolderTreeModelPrivate EMFolderTreeModelPrivate;

enum {
	COL_STRING_DISPLAY_NAME,	/* string that appears in the tree */
	COL_OBJECT_CAMEL_STORE,		/* CamelStore object */
	COL_STRING_FULL_NAME,		/* if node is a folder, the full path
					 * name of the folder, no leading / */
	COL_STRING_ICON_NAME,		/* icon name for the folder */
	COL_UINT_UNREAD,		/* unread count */
	COL_UINT_FLAGS,			/* FolderInfo.flags */

	COL_BOOL_IS_STORE,		/* toplevel store node? */
	COL_BOOL_IS_FOLDER,		/* folder (not a store) */
	COL_BOOL_LOAD_SUBDIRS,		/* %TRUE only if the store/folder
					 * has subfolders which have not yet
					 * been added to the tree */
	COL_UINT_UNREAD_LAST_SEL,	/* last known unread count */
	COL_BOOL_IS_DRAFT,		/* %TRUE for a draft folder */

	/* Status icon/spinner, only for top-level store rows. */
	COL_STATUS_ICON,
	COL_STATUS_ICON_VISIBLE,
	COL_STATUS_SPINNER_PULSE,
	COL_STATUS_SPINNER_VISIBLE,

	NUM_COLUMNS
};

struct _EMFolderTreeModel {
	GtkTreeStore parent;
	EMFolderTreeModelPrivate *priv;
};

struct _EMFolderTreeModelClass {
	GtkTreeStoreClass parent_class;

	/* signals */
	void		(*loading_row)		(EMFolderTreeModel *model,
						 GtkTreePath *path,
						 GtkTreeIter *iter);

	void		(*loaded_row)		(EMFolderTreeModel *model,
						 GtkTreePath *path,
						 GtkTreeIter *iter);
};

GType		em_folder_tree_model_get_type	(void);
EMFolderTreeModel *
		em_folder_tree_model_new	(void);
EMFolderTreeModel *
		em_folder_tree_model_get_default (void);
void		em_folder_tree_model_free_default (void);
GtkTreeSelection *
		em_folder_tree_model_get_selection
					(EMFolderTreeModel *model);
void		em_folder_tree_model_set_selection
					(EMFolderTreeModel *model,
					 GtkTreeSelection *selection);
EMailSession *	em_folder_tree_model_get_session
					(EMFolderTreeModel *model);
void		em_folder_tree_model_set_session
					(EMFolderTreeModel *model,
					 EMailSession *session);
void		em_folder_tree_model_set_folder_info
					(EMFolderTreeModel *model,
					 GtkTreeIter *iter,
					 CamelStore *store,
					 CamelFolderInfo *fi,
					 gint fully_loaded);
void		em_folder_tree_model_add_store
					(EMFolderTreeModel *model,
					 CamelStore *store);
void		em_folder_tree_model_remove_store
					(EMFolderTreeModel *model,
					 CamelStore *store);
void		em_folder_tree_model_remove_all_stores
					(EMFolderTreeModel *model);
GList *		em_folder_tree_model_list_stores
					(EMFolderTreeModel *model);
gboolean	em_folder_tree_model_is_type_inbox
					(EMFolderTreeModel *model,
					 CamelStore *store,
					 const gchar *full);
gchar *		em_folder_tree_model_get_folder_name
					(EMFolderTreeModel *model,
					 CamelStore *store,
					 const gchar *full);
GtkTreeRowReference *
		em_folder_tree_model_get_row_reference
					(EMFolderTreeModel *model,
					 CamelStore *store,
					 const gchar *folder_name);
void		em_folder_tree_model_user_marked_unread
					(EMFolderTreeModel *model,
					 CamelFolder *folder,
					 guint n_marked);
gboolean	em_folder_tree_model_has_unread_mismatch
					(GtkTreeModel *model,
					 GtkTreeIter *store_iter);

G_END_DECLS

#endif /* EM_FOLDER_TREE_MODEL_H */
