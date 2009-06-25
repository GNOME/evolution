/*
 *
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

#ifndef __EM_FOLDER_TREE_H__
#define __EM_FOLDER_TREE_H__

#include <gtk/gtk.h>
#include <camel/camel-store.h>

#include "mail/em-folder-tree-model.h"

G_BEGIN_DECLS

#define EM_TYPE_FOLDER_TREE            (em_folder_tree_get_type ())
#define EM_FOLDER_TREE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EM_TYPE_FOLDER_TREE, EMFolderTree))
#define EM_FOLDER_TREE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EM_TYPE_FOLDER_TREE, EMFolderTreeClass))
#define EM_IS_FOLDER_TREE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EM_TYPE_FOLDER_TREE))
#define EM_IS_FOLDER_TREE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EM_TYPE_FOLDER_TREE))
#define EM_FOLDER_TREE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EM_TYPE_FOLDER_TREE, EMFolderTreeClass))

typedef struct _EMFolderTree EMFolderTree;
typedef struct _EMFolderTreeClass EMFolderTreeClass;

/* not sure this api is the best, but its the easiest to implement and will cover what we need */
#define EMFT_EXCLUDE_NOSELECT CAMEL_FOLDER_NOSELECT
#define EMFT_EXCLUDE_NOINFERIORS CAMEL_FOLDER_NOINFERIORS
#define EMFT_EXCLUDE_VIRTUAL CAMEL_FOLDER_VIRTUAL
#define EMFT_EXCLUDE_SYSTEM CAMEL_FOLDER_SYSTEM
#define EMFT_EXCLUDE_VTRASH CAMEL_FOLDER_VTRASH

typedef gboolean (*EMFTExcludeFunc)(EMFolderTree *emft, GtkTreeModel *model, GtkTreeIter *iter, gpointer data);

struct _EMFolderTree {
	GtkVBox parent_object;

	struct _EMFolderTreePrivate *priv;
};

struct _EMFolderTreeClass {
	GtkVBoxClass parent_class;

	/* signals */
	void (* folder_activated) (EMFolderTree *emft, const gchar *full_name, const gchar *uri);
	void (* folder_selected) (EMFolderTree *emft, const gchar *full_name, const gchar *uri, guint32 flags);
	void (* hidden_key_event) (EMFolderTree *emft, GdkEvent *event);
};

GType em_folder_tree_get_type (void);

GtkWidget *em_folder_tree_new (void);
GtkWidget *em_folder_tree_new_with_model (EMFolderTreeModel *model);

void em_folder_tree_enable_drag_and_drop (EMFolderTree *emft);

void em_folder_tree_set_multiselect (EMFolderTree *emft, gboolean mode);
void em_folder_tree_set_excluded(EMFolderTree *emft, guint32 flags);
void em_folder_tree_set_excluded_func(EMFolderTree *emft, EMFTExcludeFunc exclude, gpointer data);

void em_folder_tree_set_selected_list (EMFolderTree *emft, GList *list, gboolean expand_only);
GList *em_folder_tree_get_selected_uris (EMFolderTree *emft);
GList *em_folder_tree_get_selected_paths (EMFolderTree *emft);

void em_folder_tree_set_selected (EMFolderTree *emft, const gchar *uri, gboolean expand_only);
void em_folder_tree_select_next_path (EMFolderTree *emft, gboolean skip_read_folders);
void em_folder_tree_select_prev_path (EMFolderTree *emft, gboolean skip_read_folders);
gchar *em_folder_tree_get_selected_uri (EMFolderTree *emft);
gchar *em_folder_tree_get_selected_path (EMFolderTree *emft);
CamelFolder *em_folder_tree_get_selected_folder (EMFolderTree *emft);
CamelFolderInfo *em_folder_tree_get_selected_folder_info (EMFolderTree *emft);

EMFolderTreeModel *em_folder_tree_get_model (EMFolderTree *emft);
EMFolderTreeModelStoreInfo *em_folder_tree_get_model_storeinfo (EMFolderTree *emft, CamelStore *store);

gboolean em_folder_tree_create_folder (EMFolderTree *emft, const gchar *full_name, const gchar *uri);
GtkWidget * em_folder_tree_get_tree_view (EMFolderTree *emft);
void em_folder_tree_set_skip_double_click (EMFolderTree *emft, gboolean skip);

G_END_DECLS

#endif /* __EM_FOLDER_TREE_H__ */
