/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-folder-tree.h
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#ifndef _E_FOLDER_TREE_H_
#define _E_FOLDER_TREE_H_


typedef struct _EFolderTree EFolderTree;

typedef void (* EFolderDestroyNotify)   (EFolderTree *tree, const char *path, void *data, void *closure);
typedef void (* EFolderTreeForeachFunc) (EFolderTree *tree, const char *path, void *data, void *closure);


EFolderTree *e_folder_tree_new             (EFolderDestroyNotify    folder_destroy_notify,
					    void                   *closure);
void         e_folder_tree_destroy         (EFolderTree            *folder_tree);
gboolean     e_folder_tree_add             (EFolderTree            *folder_tree,
					    const char             *path,
					    void                   *data);
gboolean     e_folder_tree_remove          (EFolderTree            *folder_tree,
					    const char             *path);

void        *e_folder_tree_get_folder      (EFolderTree            *folder_tree,
					    const char             *path);
GList       *e_folder_tree_get_subfolders  (EFolderTree            *folder_tree,
					    const char             *path);

void         e_folder_tree_foreach         (EFolderTree            *folder_tree,
					    EFolderTreeForeachFunc  foreach_func,
					    void                   *data);

#endif /* _E_FOLDER_TREE_H_ */
