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


#ifndef __EM_FOLDER_TREE_H__
#define __EM_FOLDER_TREE_H__

#include <gtk/gtkvbox.h>
#include <camel/camel-store.h>

#include "em-folder-tree-model.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

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

struct _EMFolderTree {
	GtkVBox parent_object;

	struct _EMFolderTreePrivate *priv;
};

struct _EMFolderTreeClass {
	GtkVBoxClass parent_class;
	
	/* signals */
	void (* folder_activated) (EMFolderTree *emft, const char *path, const char *uri);
	void (* folder_selected) (EMFolderTree *emft, const char *path, const char *uri, guint32 flags);
};

GType em_folder_tree_get_type (void);

GtkWidget *em_folder_tree_new (void);
GtkWidget *em_folder_tree_new_with_model (EMFolderTreeModel *model);

void em_folder_tree_enable_drag_and_drop (EMFolderTree *emft);

void em_folder_tree_set_multiselect (EMFolderTree *emft, gboolean mode);
void em_folder_tree_set_excluded(EMFolderTree *emft, guint32 flags);

void em_folder_tree_set_selected_list (EMFolderTree *emft, GList *list);
GList *em_folder_tree_get_selected_uris (EMFolderTree *emft);
GList *em_folder_tree_get_selected_paths (EMFolderTree *emft);

void em_folder_tree_set_selected (EMFolderTree *emft, const char *uri);
const char *em_folder_tree_get_selected_uri (EMFolderTree *emft);
const char *em_folder_tree_get_selected_path (EMFolderTree *emft);

EMFolderTreeModel *em_folder_tree_get_model (EMFolderTree *emft);

gboolean em_folder_tree_create_folder (EMFolderTree *emft, const char *path, const char *uri);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EM_FOLDER_TREE_H__ */
