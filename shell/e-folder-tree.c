/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-folder-set.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <string.h>

#include "e-folder-tree.h"


struct _Folder {
	struct _Folder *parent;
	char *path;
	void *data;
	GList *subfolders;
};
typedef struct _Folder Folder;

struct _EFolderTree {
	GHashTable *path_to_folder;

	EFolderDestroyNotify folder_destroy_notify;
	void *folder_destroy_notify_closure;
};


/* Utility functions.  */

static char *
get_parent_path (const char *path)
{
	const char *last_separator;

	g_assert (g_path_is_absolute (path));

	last_separator = strrchr (path, G_DIR_SEPARATOR);

	if (last_separator == path)
		return g_strdup (G_DIR_SEPARATOR_S);

	return g_strndup (path, last_separator - path);
}

static void
traverse_subtree (EFolderTree *tree,
		  Folder *root_folder,
		  EFolderTreeForeachFunc foreach_func,
		  void *data)
{
	GList *p;

	g_assert (foreach_func != NULL);

	(* foreach_func) (tree, root_folder->path, root_folder->data, data);

	for (p = root_folder->subfolders; p != NULL; p = p->next) {
		Folder *folder;

		folder = (Folder *) p->data;
		traverse_subtree (tree, folder, foreach_func, data);
	}
}


/* Folder handling.  */

static Folder *
folder_new (const char *path,
	    void *data)
{
	Folder *folder;

	folder = g_new (Folder, 1);
	folder->parent     = NULL;
	folder->path       = g_strdup (path);
	folder->data       = data;
	folder->subfolders = NULL;

	return folder;
}

static void
folder_remove_subfolder (Folder *folder,
			 Folder *subfolder)
{
	g_list_remove (folder->subfolders, folder);
}

static void
folder_add_subfolder (Folder *folder,
		      Folder *subfolder)
{
	folder->subfolders = g_list_prepend (folder->subfolders, subfolder);
	subfolder->parent = folder;
}

static void
folder_destroy (Folder *folder)
{
	g_assert (folder->subfolders == NULL);

	if (folder->parent != NULL)
		folder_remove_subfolder (folder->parent, folder);

	g_free (folder->path);

	g_free (folder);
}

static void
remove_folder (EFolderTree *folder_tree,
	       Folder *folder)
{
	if (folder->subfolders != NULL) {
		GList *p;

		for (p = folder->subfolders; p != NULL; p = p->next) {
			Folder *subfolder;

			subfolder = (Folder *) p->data;
			remove_folder (folder_tree, subfolder);
		}

		g_list_free (folder->subfolders);
		folder->subfolders = NULL;
	}

	g_hash_table_remove (folder_tree->path_to_folder, folder->path);

	if (folder_tree->folder_destroy_notify != NULL)
		(* folder_tree->folder_destroy_notify) (folder_tree,
							folder->path,
							folder->data,
							folder_tree->folder_destroy_notify_closure);

	folder_destroy (folder);
}


/**
 * e_folder_tree_new:
 * @folder_destroy_notify: Function to be called when a folder gets removed from the tree
 * @closure: Additional data to pass to @folder_destroy_notify
 * 
 * Create a new EFolderTree.
 * 
 * Return value: A pointer to the newly created EFolderTree.
 **/
EFolderTree *
e_folder_tree_new (EFolderDestroyNotify folder_destroy_notify,
		   void *closure)
{
	EFolderTree *new;
	Folder *root_folder;

	new = g_new (EFolderTree, 1);

	new->folder_destroy_notify         = folder_destroy_notify;
	new->folder_destroy_notify_closure = closure;

	new->path_to_folder = g_hash_table_new (g_str_hash, g_str_equal);

	root_folder = folder_new (G_DIR_SEPARATOR_S, NULL);
	g_hash_table_insert (new->path_to_folder, root_folder->path, root_folder);

	return new;
}

/**
 * e_folder_tree_destroy:
 * @folder_tree: A pointer to an EFolderTree
 * 
 * Destroy @folder_tree.
 **/
void
e_folder_tree_destroy (EFolderTree *folder_tree)
{
	Folder *root_folder;

	g_return_if_fail (folder_tree != NULL);

	root_folder = g_hash_table_lookup (folder_tree->path_to_folder, G_DIR_SEPARATOR_S);
	remove_folder (folder_tree, root_folder);

	g_hash_table_destroy (folder_tree->path_to_folder);

	g_free (folder_tree);
}

/**
 * e_folder_tree_add:
 * @folder_tree: A pointer to an EFolderTree
 * @path: Path at which the new folder must be added
 * @data: Data associated with the new folder
 * 
 * Insert a new folder at @path, with the specified @data.
 *
 * Return value: %TRUE if successful, %FALSE if failed.
 **/
gboolean
e_folder_tree_add (EFolderTree *folder_tree,
		   const char *path,
		   void *data)
{
	Folder *parent_folder;
	Folder *folder;
	char *parent_path;

	g_return_val_if_fail (folder_tree != NULL, FALSE);
	g_return_val_if_fail (path != NULL, FALSE);
	g_return_val_if_fail (g_path_is_absolute (path), FALSE);

	parent_path = get_parent_path (path);

	parent_folder = g_hash_table_lookup (folder_tree->path_to_folder, parent_path);
	if (parent_folder == NULL) {
		g_warning ("%s: Trying to add a subfolder to a path that does not exist yet -- %s",
			   __FUNCTION__, parent_path);
		return FALSE;
	}

	folder = g_hash_table_lookup (folder_tree->path_to_folder, path);
	if (folder != NULL) {
		g_warning ("%s: Trying to add a subfolder for a path that already exists -- %s",
			   __FUNCTION__, path);
		return FALSE;
	}

	folder = folder_new (path, data);
	folder_add_subfolder (parent_folder, folder);

	g_hash_table_insert (folder_tree->path_to_folder, folder->path, folder);

	g_free (parent_path);

	return TRUE;
}

/**
 * e_folder_tree_remove:
 * @folder_tree: A pointer to an EFolderTree
 * @path: Path of the folder to remove
 * 
 * Remove the folder at @path from @folder_tree.
 *
 * Return value: %TRUE if successful, %FALSE if failed.
 **/
gboolean
e_folder_tree_remove (EFolderTree *folder_tree,
		      const char *path)
{
	Folder *folder;

	g_return_val_if_fail (folder_tree != NULL, FALSE);
	g_return_val_if_fail (path != NULL, FALSE);
	g_return_val_if_fail (g_path_is_absolute (path), FALSE);

	folder = g_hash_table_lookup (folder_tree->path_to_folder, path);
	if (folder == NULL)
		return FALSE;

	remove_folder (folder_tree, folder);
	return TRUE;
}

/**
 * e_folder_tree_get_folder:
 * @folder_tree: A pointer to an EFolderTree
 * @path: Path of the folder for which we want to get the data
 * 
 * Get the data for the folder at @path.
 * 
 * Return value: The pointer to the data for the folder at @path.
 **/
void *
e_folder_tree_get_folder (EFolderTree *folder_tree,
			  const char *path)
{
	Folder *folder;

	g_return_val_if_fail (folder_tree != NULL, NULL);
	g_return_val_if_fail (path != NULL, NULL);
	g_return_val_if_fail (g_path_is_absolute (path), NULL);

	folder = g_hash_table_lookup (folder_tree->path_to_folder, path);
	if (folder == NULL)
		return NULL;

	return folder->data;
}

/**
 * e_folder_tree_get_subfolders:
 * @folder_tree: A pointer to an EFolderTree
 * @path: A path in @folder_tree
 * 
 * Get a list of the paths of the subfolders of @path.
 * 
 * Return value: A list of pointers to the paths of the subfolders.  The list
 * and the strings must be freed by the caller.
 **/
GList *
e_folder_tree_get_subfolders (EFolderTree *folder_tree,
			      const char *path)
{
	Folder *folder;
	GList *list;
	GList *p;

	g_return_val_if_fail (folder_tree != NULL, NULL);
	g_return_val_if_fail (path != NULL, NULL);
	g_return_val_if_fail (g_path_is_absolute (path), NULL);

	folder = g_hash_table_lookup (folder_tree->path_to_folder, path);
	if (folder == NULL)
		return NULL;

	list = NULL;
	for (p = folder->subfolders; p != NULL; p = p->next) {
		const Folder *folder;

		folder = (const Folder *) p->data;
		list = g_list_prepend (list, g_strdup (folder->path));
	}

	return list;
}


/**
 * e_folder_tree_foreach:
 * @folder_tree: 
 * @foreach_func: 
 * @data: 
 * 
 * Call @foreach_func with the specified @data for all the folders
 * in @folder_tree, starting at the root node.
 **/
void
e_folder_tree_foreach (EFolderTree *folder_tree,
		       EFolderTreeForeachFunc foreach_func,
		       void *data)
{
	Folder *root_node;

	g_return_if_fail (folder_tree != NULL);
	g_return_if_fail (foreach_func != NULL);

	root_node = g_hash_table_lookup (folder_tree->path_to_folder,
					 G_DIR_SEPARATOR_S);
	if (root_node == NULL) {
		g_warning ("e_folder_tree_foreach -- What?!  No root node!?");
		return;
	}

	traverse_subtree (folder_tree, root_node, foreach_func, data);
}
