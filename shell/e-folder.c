/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-folder.c
 *
 * Copyright (C) 2000, 2001, 2002  Ximian, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-folder.h"
#include "e-shell-marshal.h"

#include "e-util/e-corba-utils.h"

#include <string.h>
#include <glib.h>
#include <gtk/gtksignal.h>
#include <gal/util/e-util.h>


#define PARENT_TYPE GTK_TYPE_OBJECT
static GtkObjectClass *parent_class = NULL;

struct _EFolderPrivate {
	char *name;
	char *type;
	char *description;
	char *physical_uri;

	int child_highlight;
	int unread_count;

	/* Folders have a default sorting priority of zero; when deciding the
	   sort order in the Evolution folder tree, folders with the same
	   priority value are compared by name, while folders with a higher
	   priority number always come after the folders with a lower priority
	   number.  */
	int sorting_priority;

	unsigned int self_highlight : 1;
	unsigned int is_stock : 1;
	unsigned int can_sync_offline : 1;
	unsigned int has_subfolders : 1;

	/* Custom icon for this folder; if NULL the folder will just use the
	   icon for its type.  */
	char *custom_icon_name;
};

#define EF_CLASS(obj) \
	E_FOLDER_CLASS (GTK_OBJECT (obj)->klass)


enum {
	CHANGED,
	NAME_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


/* EFolder methods.  */

static gboolean
impl_save_info (EFolder *folder)
{
	g_warning ("`%s' does not implement `EFolder::save_info()'",
		   G_OBJECT_TYPE_NAME (folder));
	return FALSE;
}

static gboolean
impl_load_info (EFolder *folder)
{
	g_warning ("`%s' does not implement `EFolder::load_info()'",
		   G_OBJECT_TYPE_NAME (folder));
	return FALSE;
}

static gboolean
impl_remove (EFolder *folder)
{
	g_warning ("`%s' does not implement `EFolder::remove()'",
		   G_OBJECT_TYPE_NAME (folder));
	return FALSE;
}

static const char *
impl_get_physical_uri (EFolder *folder)
{
	return folder->priv->physical_uri;
}


/* GObject methods.  */

static void
impl_finalize (GObject *object)
{
	EFolder *folder;
	EFolderPrivate *priv;

	folder = E_FOLDER (object);
	priv = folder->priv;

	g_free (priv->name);
	g_free (priv->type);
	g_free (priv->description);
	g_free (priv->physical_uri);

	g_free (priv->custom_icon_name);

	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


static void
class_init (EFolderClass *klass)
{
	GObjectClass *object_class;

	parent_class = g_type_class_ref(gtk_object_get_type ());

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = impl_finalize;

	signals[CHANGED] = g_signal_new ("changed",
					 G_OBJECT_CLASS_TYPE (object_class),
					 G_SIGNAL_RUN_FIRST,
					 G_STRUCT_OFFSET (EFolderClass, changed),
					 NULL, NULL,
					 e_shell_marshal_NONE__NONE,
					 G_TYPE_NONE, 0);

	signals[NAME_CHANGED] = g_signal_new ("name_changed",
					      G_OBJECT_CLASS_TYPE (object_class),
					      G_SIGNAL_RUN_FIRST,
					      G_STRUCT_OFFSET (EFolderClass, name_changed),
					      NULL, NULL,
					      e_shell_marshal_NONE__NONE,
					      G_TYPE_NONE, 0);

	klass->save_info 	= impl_save_info;
	klass->load_info 	= impl_load_info;
	klass->remove    	= impl_remove;
	klass->get_physical_uri = impl_get_physical_uri;
}

static void
init (EFolder *folder)
{
	EFolderPrivate *priv;

	priv = g_new (EFolderPrivate, 1);
	priv->type             = NULL;
	priv->name             = NULL;
	priv->description      = NULL;
	priv->physical_uri     = NULL;
	priv->child_highlight  = 0;
	priv->unread_count     = 0;
	priv->sorting_priority = 0;
	priv->self_highlight   = FALSE;
	priv->is_stock         = FALSE;
	priv->can_sync_offline = FALSE;
	priv->has_subfolders   = FALSE;
	priv->custom_icon_name = NULL;

	folder->priv = priv;

	GTK_OBJECT_UNSET_FLAGS (GTK_OBJECT (folder), GTK_FLOATING);
}


void
e_folder_construct (EFolder *folder,
		    const char *name,
		    const char *type,
		    const char *description)
{
	EFolderPrivate *priv;

	g_return_if_fail (folder != NULL);
	g_return_if_fail (E_IS_FOLDER (folder));
	g_return_if_fail (name != NULL);
	g_return_if_fail (type != NULL);

	priv = folder->priv;

	priv->name        = g_strdup (name);
	priv->type        = g_strdup (type);
	priv->description = g_strdup (description);
}

EFolder *
e_folder_new (const char *name,
	      const char *type,
	      const char *description)
{
	EFolder *folder;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (type != NULL, NULL);
	g_return_val_if_fail (description != NULL, NULL);

	folder = g_object_new (E_TYPE_FOLDER, NULL);

	e_folder_construct (folder, name, type, description);

	return folder;
}


const char *
e_folder_get_name (EFolder *folder)
{
	g_return_val_if_fail (folder != NULL, NULL);
	g_return_val_if_fail (E_IS_FOLDER (folder), NULL);

	return folder->priv->name;
}

const char *
e_folder_get_type_string (EFolder *folder)
{
	g_return_val_if_fail (folder != NULL, NULL);
	g_return_val_if_fail (E_IS_FOLDER (folder), NULL);

	return folder->priv->type;
}

const char *
e_folder_get_description (EFolder *folder)
{
	g_return_val_if_fail (folder != NULL, NULL);
	g_return_val_if_fail (E_IS_FOLDER (folder), NULL);

	return folder->priv->description;
}

const char *
e_folder_get_physical_uri (EFolder *folder)
{
	g_return_val_if_fail (folder != NULL, NULL);
	g_return_val_if_fail (E_IS_FOLDER (folder), NULL);

	return folder->priv->physical_uri;
}

int
e_folder_get_unread_count (EFolder *folder)
{
	g_return_val_if_fail (folder != NULL, FALSE);
	g_return_val_if_fail (E_IS_FOLDER (folder), FALSE);

	return folder->priv->unread_count;
}

gboolean
e_folder_get_highlighted (EFolder *folder)
{
	g_return_val_if_fail (folder != NULL, FALSE);
	g_return_val_if_fail (E_IS_FOLDER (folder), FALSE);

	return folder->priv->child_highlight || folder->priv->unread_count;
}

gboolean
e_folder_get_is_stock (EFolder *folder)
{
	g_return_val_if_fail (folder != NULL, FALSE);
	g_return_val_if_fail (E_IS_FOLDER (folder), FALSE);

	return folder->priv->is_stock;
}

gboolean
e_folder_get_can_sync_offline (EFolder *folder)
{
	g_return_val_if_fail (E_IS_FOLDER (folder), FALSE);

	return folder->priv->can_sync_offline;
}

gboolean
e_folder_get_has_subfolders (EFolder *folder)
{
	g_return_val_if_fail (E_IS_FOLDER (folder), FALSE);

	return folder->priv->has_subfolders;
}

/**
 * e_folder_get_custom_icon:
 * @folder: An EFolder
 * 
 * Get the name of the custom icon for @folder, or NULL if no custom icon is
 * associated with it.
 **/
const char *
e_folder_get_custom_icon_name (EFolder *folder)
{
	g_return_val_if_fail (E_IS_FOLDER (folder), NULL);

	return folder->priv->custom_icon_name;
}

/**
 * e_folder_get_sorting_priority:
 * @folder: An EFolder
 * 
 * Get the sorting priority for @folder.
 * 
 * Return value: Sorting priority value for @folder.
 **/
int
e_folder_get_sorting_priority (EFolder *folder)
{
	g_return_val_if_fail (E_IS_FOLDER (folder), 0);

	return folder->priv->sorting_priority;
}


void
e_folder_set_name (EFolder *folder,
		   const char *name)
{
	g_return_if_fail (folder != NULL);
	g_return_if_fail (E_IS_FOLDER (folder));
	g_return_if_fail (name != NULL);

	if (folder->priv->name == name)
		return;

	g_free (folder->priv->name);
	folder->priv->name = g_strdup (name);

	g_signal_emit (folder, signals[NAME_CHANGED], 0);
	g_signal_emit (folder, signals[CHANGED], 0);
}

void
e_folder_set_type_string (EFolder *folder,
			  const char *type)
{
	g_return_if_fail (folder != NULL);
	g_return_if_fail (E_IS_FOLDER (folder));
	g_return_if_fail (type != NULL);

	g_free (folder->priv->type);
	folder->priv->type = g_strdup (type);

	g_signal_emit (folder, signals[CHANGED], 0);
}

void
e_folder_set_description (EFolder *folder,
			  const char *description)
{
	g_return_if_fail (folder != NULL);
	g_return_if_fail (E_IS_FOLDER (folder));
	g_return_if_fail (description != NULL);

	g_free (folder->priv->description);
	folder->priv->description = g_strdup (description);

	g_signal_emit (folder, signals[CHANGED], 0);
}

void
e_folder_set_physical_uri (EFolder *folder,
			   const char *physical_uri)
{
	g_return_if_fail (folder != NULL);
	g_return_if_fail (E_IS_FOLDER (folder));
	g_return_if_fail (physical_uri != NULL);

	if (folder->priv->physical_uri == physical_uri)
		return;

	g_free (folder->priv->physical_uri);
	folder->priv->physical_uri = g_strdup (physical_uri);

	g_signal_emit (folder, signals[CHANGED], 0);
}

void
e_folder_set_unread_count (EFolder *folder,
			   gint unread_count)
{
	g_return_if_fail (folder != NULL);
	g_return_if_fail (E_IS_FOLDER (folder));

	folder->priv->unread_count = unread_count;

	g_signal_emit (folder, signals[CHANGED], 0);
}

void
e_folder_set_child_highlight (EFolder *folder,
			      gboolean highlighted)
{
	g_return_if_fail (folder != NULL);
	g_return_if_fail (E_IS_FOLDER (folder));

	if (highlighted)
		folder->priv->child_highlight++;
	else
		folder->priv->child_highlight--;

	g_signal_emit (folder, signals[CHANGED], 0);
}

void
e_folder_set_is_stock (EFolder *folder,
		       gboolean is_stock)
{
	g_return_if_fail (folder != NULL);
	g_return_if_fail (E_IS_FOLDER (folder));

	folder->priv->is_stock = !! is_stock;

	g_signal_emit (folder, signals[CHANGED], 0);
}

void
e_folder_set_can_sync_offline (EFolder *folder,
			       gboolean can_sync_offline)
{
	g_return_if_fail (E_IS_FOLDER (folder));

	folder->priv->can_sync_offline = !! can_sync_offline;

	g_signal_emit (folder, signals[CHANGED], 0);
}

void
e_folder_set_has_subfolders (EFolder *folder,
			     gboolean has_subfolders)
{
	g_return_if_fail (E_IS_FOLDER (folder));

	folder->priv->has_subfolders = !! has_subfolders;

	g_signal_emit (folder, signals[CHANGED], 0);
}

/**
 * e_folder_set_custom_icon_name:
 * @folder: An EFolder
 * @icon_name: Name of the icon to be set (to be found in the standard
 * Evolution icon dir)
 * 
 * Set a custom icon for @folder (thus overriding the default icon, which is
 * the one associated to the type of the folder).
 **/
void
e_folder_set_custom_icon (EFolder *folder,
			  const char *icon_name)
{
	g_return_if_fail (E_IS_FOLDER (folder));

	if (icon_name == folder->priv->custom_icon_name)
		return;

	if (folder->priv->custom_icon_name == NULL
	    || (icon_name != NULL && strcmp (icon_name, folder->priv->custom_icon_name) != 0)) {
		g_free (folder->priv->custom_icon_name);
		folder->priv->custom_icon_name = g_strdup (icon_name);

		g_signal_emit (folder, signals[CHANGED], 0);
	}
}

/**
 * e_folder_set_sorting_priority:
 * @folder: An EFolder
 * @sorting_priority: A sorting priority number
 * 
 * Set the sorting priority for @folder.  Folders have a default sorting
 * priority of zero; when deciding the sort order in the Evolution folder tree,
 * folders with the same priority value are compared by name, while folders
 * with a higher priority number always come after the folders with a lower
 * priority number.
 **/
void
e_folder_set_sorting_priority (EFolder *folder,
			       int sorting_priority)
{
	g_return_if_fail (E_IS_FOLDER (folder));

	if (folder->priv->sorting_priority == sorting_priority)
		return;

	folder->priv->sorting_priority = sorting_priority;

	g_signal_emit (folder, signals[CHANGED], 0);
}


/* Gotta love CORBA.  */

static CORBA_char *
safe_corba_string_dup (const char *s)
{
	if (s == NULL)
		return CORBA_string_dup ("");

	return CORBA_string_dup (s);
}

void
e_folder_to_corba (EFolder *folder,
		   const char *evolution_uri,
		   GNOME_Evolution_Folder *folder_return)
{
	g_return_if_fail (E_IS_FOLDER (folder));
	g_return_if_fail (folder_return != NULL);

	folder_return->type            = safe_corba_string_dup (e_folder_get_type_string (folder));
	folder_return->description     = safe_corba_string_dup (e_folder_get_description (folder));
	folder_return->displayName     = safe_corba_string_dup (e_folder_get_name (folder));
	folder_return->physicalUri     = safe_corba_string_dup (e_folder_get_physical_uri (folder));
	folder_return->evolutionUri    = safe_corba_string_dup (evolution_uri);
	folder_return->customIconName  = safe_corba_string_dup (e_folder_get_custom_icon_name (folder));
	folder_return->unreadCount     = e_folder_get_unread_count (folder);
	folder_return->canSyncOffline  = e_folder_get_can_sync_offline (folder);
	folder_return->sortingPriority = e_folder_get_sorting_priority (folder);
}


E_MAKE_TYPE (e_folder, "EFolder", EFolder, class_init, init, PARENT_TYPE)
