/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-storage.c
 *
 * Copyright (C) 2000, 2001 Ximian, Inc.
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

#include "evolution-storage.h"

#include "Evolution.h"

#include "e-folder.h"
#include "e-folder-tree.h"
#include "e-shell-constants.h"
#include "e-shell-corba-icon-utils.h"

#include "e-shell-marshal.h"

#include <string.h>
#include <gal/util/e-util.h>
#include <gtk/gtksignal.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-exception.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>


#define PARENT_TYPE BONOBO_OBJECT_TYPE
static BonoboObjectClass *parent_class = NULL;

struct _FolderPropertyItem {
	char *label;
	char *tooltip;
	GdkPixbuf *icon;
};
typedef struct _FolderPropertyItem FolderPropertyItem;

struct _EvolutionStoragePrivate {
	/* Name of the storage.  */
	char *name;

	/* Whether there are shared folders in this storage.  */
	gboolean has_shared_folders;

	/* What we will display as the name of the storage. */
	char *display_name;

	/* The set of folders we have in this storage.  */
	EFolderTree *folder_tree;

	/* Mappings from URIs to folder tree paths.  */
	GHashTable *uri_to_path;

	/* The listener registered on this storage.  */
	GList *corba_storage_listeners;

	/* The property items.  */
	GSList *folder_property_items;
};


enum {
	CREATE_FOLDER,
	REMOVE_FOLDER,
	XFER_FOLDER,
	UPDATE_FOLDER,
	OPEN_FOLDER,
	DISCOVER_SHARED_FOLDER,
	CANCEL_DISCOVER_SHARED_FOLDER,
	REMOVE_SHARED_FOLDER,
	SHOW_FOLDER_PROPERTIES,

	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


/* Utility functions.  */

static void
list_through_listener_foreach (EFolderTree *tree,
			       const char *path,
			       void *data,
			       void *closure)
{
	const GNOME_Evolution_Folder *corba_folder;
	GNOME_Evolution_StorageListener corba_listener;
	CORBA_Environment ev;

	corba_folder = (GNOME_Evolution_Folder *) data;
	corba_listener = (GNOME_Evolution_StorageListener) closure;

	/* The root folder has no data.  */
	if (corba_folder == NULL)
		return;
	
	CORBA_exception_init (&ev);
	GNOME_Evolution_StorageListener_notifyFolderCreated (corba_listener, path, corba_folder, &ev);
	CORBA_exception_free (&ev);
}

static void
list_through_listener (EvolutionStorage *storage,
		       GNOME_Evolution_StorageListener listener,
		       CORBA_Environment *ev)
{
	EvolutionStoragePrivate *priv;

	priv = storage->priv;

	e_folder_tree_foreach (priv->folder_tree,
			       list_through_listener_foreach,
			       listener);
}

static GList *
find_listener_in_list (const GNOME_Evolution_StorageListener listener,
		       GList *list)
{
	CORBA_Environment ev;
	GList *p;

	CORBA_exception_init (&ev);

	for (p = list; p != NULL; p = p->next) {
		GNOME_Evolution_StorageListener listener_item;

		listener_item = (GNOME_Evolution_StorageListener) p->data;

		if (CORBA_Object_is_equivalent (listener_item, listener, &ev) && ev._major == CORBA_NO_EXCEPTION)
			return p;
	}

	CORBA_exception_free (&ev);

	return NULL;
}

static gboolean
add_listener (EvolutionStorage *storage,
	      const GNOME_Evolution_StorageListener listener)
{
	EvolutionStoragePrivate *priv;
	GNOME_Evolution_StorageListener listener_copy;
	CORBA_Environment ev;

	priv = storage->priv;

	if (find_listener_in_list (listener, priv->corba_storage_listeners) != NULL)
		return FALSE;

	CORBA_exception_init (&ev);

	listener_copy = CORBA_Object_duplicate (listener, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		/* Panic.  */
		g_warning ("EvolutionStorage -- Cannot duplicate listener.");
		CORBA_exception_free (&ev);

		/* FIXME this will cause the ::add_listener implementation to
                   incorrectly raise `AlreadyListening' */
		return FALSE;
	}

	priv->corba_storage_listeners = g_list_prepend (priv->corba_storage_listeners,
							listener_copy);

	list_through_listener (storage, listener_copy, &ev);

	CORBA_exception_free (&ev);

	return TRUE;
}

static gboolean
remove_listener (EvolutionStorage *storage,
		 const GNOME_Evolution_StorageListener listener)
{
	EvolutionStoragePrivate *priv;
	CORBA_Environment ev;
	GList *p;

	priv = storage->priv;

	p = find_listener_in_list (listener, priv->corba_storage_listeners);
	if (p == NULL)
		return FALSE;

	CORBA_exception_init (&ev);
	CORBA_Object_release ((CORBA_Object) p->data, &ev);
	CORBA_exception_free (&ev);

	priv->corba_storage_listeners = g_list_remove_link (priv->corba_storage_listeners, p);

	return TRUE;
}


/* Functions for the EFolderTree in the storage.  */

static void
folder_destroy_notify (EFolderTree *tree,
		       const char *path,
		       void *data,
		       void *closure)
{
	GNOME_Evolution_Folder *corba_folder;

	corba_folder = (GNOME_Evolution_Folder *) data;
	CORBA_free (data);
}


/* CORBA interface implementation.  */

static CORBA_char *
impl_Storage__get_name (PortableServer_Servant servant,
			CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorage *storage;
	EvolutionStoragePrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	storage = EVOLUTION_STORAGE (bonobo_object);
	priv = storage->priv;

	return CORBA_string_dup (priv->name);
}

static CORBA_boolean
impl_Storage__get_hasSharedFolders (PortableServer_Servant servant,
				    CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorage *storage;
	EvolutionStoragePrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	storage = EVOLUTION_STORAGE (bonobo_object);
	priv = storage->priv;

	return priv->has_shared_folders;
}

static GNOME_Evolution_Folder *
impl_Storage_getFolderAtPath (PortableServer_Servant servant,
			      const CORBA_char *path,
			      CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorage *storage;
	EvolutionStoragePrivate *priv;
	GNOME_Evolution_Folder *corba_folder;
	GNOME_Evolution_Folder *return_value;

	bonobo_object = bonobo_object_from_servant (servant);
	storage = EVOLUTION_STORAGE (bonobo_object);
	priv = storage->priv;

        corba_folder = e_folder_tree_get_folder (priv->folder_tree, path);
	if (corba_folder == NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Storage_NotFound, NULL);
		return NULL;
	}

	/* duplicate CORBA structure */
	return_value = GNOME_Evolution_Folder__alloc ();
	return_value->type = CORBA_string_dup (corba_folder->type);
	return_value->description = CORBA_string_dup (corba_folder->description);
	return_value->displayName = CORBA_string_dup (corba_folder->displayName);
	return_value->physicalUri = CORBA_string_dup (corba_folder->physicalUri);
	return_value->evolutionUri = CORBA_string_dup (corba_folder->evolutionUri);
	return_value->customIconName = CORBA_string_dup (corba_folder->customIconName);
	return_value->unreadCount = corba_folder->unreadCount;
	return_value->canSyncOffline = corba_folder->canSyncOffline;
	return_value->sortingPriority = corba_folder->sortingPriority;

	return return_value;
}

static void
get_folder_list_foreach (EFolderTree *tree,
			 const char *path,
			 void *data,
			 void *closure)
{
	const GNOME_Evolution_Folder *corba_folder;
	GNOME_Evolution_Folder *new_corba_folder;
	GNOME_Evolution_FolderList *folder_list;
	
	corba_folder = (GNOME_Evolution_Folder *) data;
	folder_list = (GNOME_Evolution_FolderList *) closure;

	/* The root folder has no data.  */
	if (corba_folder == NULL)
		return;

	new_corba_folder = folder_list->_buffer + folder_list->_length;
	new_corba_folder->displayName  	  = CORBA_string_dup (corba_folder->displayName);
	new_corba_folder->description  	  = CORBA_string_dup (corba_folder->description);
	new_corba_folder->type         	  = CORBA_string_dup (corba_folder->type);
	new_corba_folder->physicalUri  	  = CORBA_string_dup (corba_folder->physicalUri);
	new_corba_folder->evolutionUri 	  = CORBA_string_dup (corba_folder->evolutionUri);
	new_corba_folder->unreadCount  	  = corba_folder->unreadCount;
	new_corba_folder->sortingPriority = corba_folder->sortingPriority;
	new_corba_folder->customIconName  = CORBA_string_dup (corba_folder->customIconName);
	
	folder_list->_length++;
}

static GNOME_Evolution_FolderList *
impl_Storage__get_folderList (PortableServer_Servant servant,
			      CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorage *storage;
	EvolutionStoragePrivate *priv;
	GNOME_Evolution_FolderList *folder_list;
	
	bonobo_object = bonobo_object_from_servant (servant);
	storage = EVOLUTION_STORAGE (bonobo_object);
	priv = storage->priv;
	
	folder_list = GNOME_Evolution_FolderList__alloc ();
	folder_list->_maximum = e_folder_tree_get_count (priv->folder_tree) - 1;
	folder_list->_length = 0;
	folder_list->_buffer = CORBA_sequence_GNOME_Evolution_Folder_allocbuf (folder_list->_maximum);

	e_folder_tree_foreach (priv->folder_tree, get_folder_list_foreach, folder_list);

	CORBA_sequence_set_release (folder_list, TRUE);
	return folder_list;
}

static void
impl_Storage_asyncCreateFolder (PortableServer_Servant servant,
				const CORBA_char *path,
				const CORBA_char *type,
				const CORBA_char *description,
				const CORBA_char *parent_physical_uri,
				const Bonobo_Listener listener,
				CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	CORBA_Object obj_dup;
	EvolutionStorage *storage;

	bonobo_object = bonobo_object_from_servant (servant);
	storage = EVOLUTION_STORAGE (bonobo_object);

	obj_dup = CORBA_Object_duplicate (listener, ev);
	g_signal_emit (storage, signals[CREATE_FOLDER], 0,
		       obj_dup, path, type, description, parent_physical_uri);
}


static void
impl_Storage_asyncRemoveFolder (PortableServer_Servant servant,
				const CORBA_char *path,
				const CORBA_char *physical_uri,
				const Bonobo_Listener listener,
				CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorage *storage;
	CORBA_Object obj_dup;

	bonobo_object = bonobo_object_from_servant (servant);
	storage = EVOLUTION_STORAGE (bonobo_object);

	obj_dup = CORBA_Object_duplicate (listener, ev);
	g_signal_emit (storage, signals[REMOVE_FOLDER], 0,
		       obj_dup, path, physical_uri);
}

static void
impl_Storage_asyncXferFolder (PortableServer_Servant servant,
			      const CORBA_char *source_path,
			      const CORBA_char *destination_path,
			      const CORBA_boolean remove_source,
			      const Bonobo_Listener listener,
			      CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorage *storage;
	CORBA_Object obj_dup;

	bonobo_object = bonobo_object_from_servant (servant);
	storage = EVOLUTION_STORAGE (bonobo_object);

	obj_dup = CORBA_Object_duplicate (listener, ev);
	g_signal_emit (storage, signals[XFER_FOLDER], 0,
		       obj_dup, source_path, destination_path, remove_source);
}

static void
impl_Storage_updateFolder (PortableServer_Servant servant,
			   const CORBA_char *path,
			   CORBA_long unread_count,
			   CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionStoragePrivate *priv;
	EvolutionStorage *storage;
	GList *p;

	bonobo_object = bonobo_object_from_servant (servant);
	storage = EVOLUTION_STORAGE (bonobo_object);

	g_signal_emit (storage, signals[UPDATE_FOLDER], 0,
		       path, unread_count);

	priv = storage->priv;

	if (priv->corba_storage_listeners == NULL)
		return;

	for (p = priv->corba_storage_listeners; p != NULL; p = p->next) {
		GNOME_Evolution_StorageListener listener;
		CORBA_Environment my_ev;

		CORBA_exception_init (&my_ev);

		listener = p->data;
		GNOME_Evolution_StorageListener_notifyFolderUpdated (listener, path,
								     unread_count, &my_ev);

		CORBA_exception_free (&my_ev);
	}
}

static void
impl_Storage_asyncOpenFolder (PortableServer_Servant servant,
			      const CORBA_char *path,
			      Bonobo_Listener listener,
			      CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorage *storage;
	CORBA_Object obj_dup;

	bonobo_object = bonobo_object_from_servant (servant);
	storage = EVOLUTION_STORAGE (bonobo_object);

	obj_dup = CORBA_Object_duplicate (listener, ev);
	g_signal_emit (storage, signals[OPEN_FOLDER], 0,
		       obj_dup, path);
}

static void
impl_Storage_asyncDiscoverSharedFolder (PortableServer_Servant servant,
					const CORBA_char *user,
					const CORBA_char *folder_name,
					Bonobo_Listener listener,
					CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorage *storage;
	CORBA_Object obj_dup;

	bonobo_object = bonobo_object_from_servant (servant);
	storage = EVOLUTION_STORAGE (bonobo_object);

	obj_dup = CORBA_Object_duplicate (listener, ev);
	g_signal_emit (storage, signals[DISCOVER_SHARED_FOLDER], 0,
		       obj_dup, user, folder_name);
}

static void
impl_Storage_cancelDiscoverSharedFolder (PortableServer_Servant servant,
					const CORBA_char *user,
					const CORBA_char *folder_name,
					CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorage *storage;

	bonobo_object = bonobo_object_from_servant (servant);
	storage = EVOLUTION_STORAGE (bonobo_object);

	g_signal_emit (storage, signals[CANCEL_DISCOVER_SHARED_FOLDER], 0,
		       user, folder_name);
}

static void
impl_Storage_asyncRemoveSharedFolder (PortableServer_Servant servant,
				      const CORBA_char *path,
				      const Bonobo_Listener listener,
				      CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorage *storage;
	CORBA_Object obj_dup;

	bonobo_object = bonobo_object_from_servant (servant);
	storage = EVOLUTION_STORAGE (bonobo_object);

	obj_dup = CORBA_Object_duplicate (listener, ev);
	g_signal_emit (storage, signals[REMOVE_SHARED_FOLDER], 0,
		       obj_dup, path);
}

static void
impl_Storage_addListener (PortableServer_Servant servant,
			  const GNOME_Evolution_StorageListener listener,
			  CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorage *storage;

	bonobo_object = bonobo_object_from_servant (servant);
	storage = EVOLUTION_STORAGE (bonobo_object);

	if (! add_listener (storage, listener))
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Storage_AlreadyListening, NULL);
}

static void
impl_Storage_removeListener (PortableServer_Servant servant,
			     const GNOME_Evolution_StorageListener listener,
			     CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorage *storage;

	bonobo_object = bonobo_object_from_servant (servant);
	storage = EVOLUTION_STORAGE (bonobo_object);

	if (! remove_listener (storage, listener))
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Storage_NotFound, NULL);
}

static void
impl_Storage_showFolderProperties (PortableServer_Servant servant,
				   const CORBA_char *path,
				   const CORBA_short item_number,
				   const CORBA_long parent_window_id,
				   CORBA_Environment *ev)
{
	EvolutionStorage *storage;

	storage = EVOLUTION_STORAGE (bonobo_object_from_servant (servant));
	g_signal_emit (storage, signals[SHOW_FOLDER_PROPERTIES], 0,
		       path, item_number, parent_window_id);
}

static GNOME_Evolution_Storage_FolderPropertyItemList *
impl_Storage__get_folderPropertyItems (PortableServer_Servant servant,
				       CORBA_Environment *ev)
{
	EvolutionStorage *storage;
	EvolutionStoragePrivate *priv;
	GNOME_Evolution_Storage_FolderPropertyItemList *list;
	GSList *p;
	int count;
	int i;

	storage = EVOLUTION_STORAGE (bonobo_object_from_servant (servant));
	priv = storage->priv;

	count = g_slist_length (priv->folder_property_items);

	list = GNOME_Evolution_Storage_FolderPropertyItemList__alloc ();
	list->_length = list->_maximum = count;
	list->_buffer = CORBA_sequence_GNOME_Evolution_Storage_FolderPropertyItem_allocbuf (list->_maximum);

	for (i = 0, p = priv->folder_property_items; p != NULL; i ++, p = p->next) {
		const FolderPropertyItem *item;

		item = (const FolderPropertyItem *) p->data;

		list->_buffer[i].label   = CORBA_string_dup (item->label);
		list->_buffer[i].tooltip = CORBA_string_dup (item->tooltip);
		e_store_corba_icon_from_pixbuf (item->icon, & list->_buffer[i].icon);
	}

	CORBA_sequence_set_release (list, TRUE);

	return list;
}


/* GObject methods.  */

static void
free_mapping (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	g_free (value);
}

static void
impl_dispose (GObject *object)
{
	EvolutionStorage *storage;
	EvolutionStoragePrivate *priv;
	CORBA_Environment ev;
	GList *p;

	storage = EVOLUTION_STORAGE (object);
	priv = storage->priv;

	CORBA_exception_init (&ev);

	for (p = priv->corba_storage_listeners; p != NULL; p = p->next) {
		GNOME_Evolution_StorageListener listener;

		listener = p->data;

		GNOME_Evolution_StorageListener_notifyDestroyed (listener, &ev);

		/* (This is not a Bonobo object, so no unref.)  */
		CORBA_Object_release (listener, &ev);
	}

	g_list_free (priv->corba_storage_listeners);
	priv->corba_storage_listeners = NULL;

	CORBA_exception_free (&ev);

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	EvolutionStorage *storage;
	EvolutionStoragePrivate *priv;
	GSList *sp;

	storage = EVOLUTION_STORAGE (object);
	priv = storage->priv;

	g_free (priv->name);
	if (priv->folder_tree != NULL)
		e_folder_tree_destroy (priv->folder_tree);
	if (priv->uri_to_path != NULL) {
		g_hash_table_foreach (priv->uri_to_path, free_mapping, NULL);
		g_hash_table_destroy (priv->uri_to_path);
	}

	for (sp = priv->folder_property_items; sp != NULL; sp = sp->next) {
		FolderPropertyItem *item;

		item = (FolderPropertyItem *) sp->data;

		g_free (item->label);
		g_free (item->tooltip);
		if (item->icon != NULL)
			g_object_unref (item->icon);
		g_free (item);
	}
	g_slist_free (priv->folder_property_items);

	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


static void
evolution_storage_class_init (EvolutionStorageClass *klass)
{
	POA_GNOME_Evolution_Storage__epv *epv;
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	epv = & klass->epv;
	epv->_get_name                  = impl_Storage__get_name;
	epv->_get_hasSharedFolders      = impl_Storage__get_hasSharedFolders;
	epv->getFolderAtPath            = impl_Storage_getFolderAtPath;
	epv->_get_folderList            = impl_Storage__get_folderList;
	epv->asyncCreateFolder          = impl_Storage_asyncCreateFolder;
	epv->asyncRemoveFolder          = impl_Storage_asyncRemoveFolder;
	epv->asyncXferFolder            = impl_Storage_asyncXferFolder;
	epv->asyncOpenFolder            = impl_Storage_asyncOpenFolder;
	epv->updateFolder               = impl_Storage_updateFolder;
	epv->asyncDiscoverSharedFolder  = impl_Storage_asyncDiscoverSharedFolder;
	epv->cancelDiscoverSharedFolder = impl_Storage_cancelDiscoverSharedFolder;
	epv->asyncRemoveSharedFolder    = impl_Storage_asyncRemoveSharedFolder;
	epv->addListener                = impl_Storage_addListener;
	epv->removeListener             = impl_Storage_removeListener;
	epv->showFolderProperties       = impl_Storage_showFolderProperties;
	epv->_get_folderPropertyItems   = impl_Storage__get_folderPropertyItems;

	parent_class = g_type_class_ref(PARENT_TYPE);

	signals[CREATE_FOLDER] 
		= g_signal_new ("create_folder",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (EvolutionStorageClass, create_folder),
				NULL, NULL,
				e_shell_marshal_NONE__POINTER_STRING_STRING_STRING_STRING,
				G_TYPE_NONE, 5,
				G_TYPE_POINTER,
				G_TYPE_STRING,
				G_TYPE_STRING,
				G_TYPE_STRING,
				G_TYPE_STRING);

	signals[REMOVE_FOLDER] 
		= g_signal_new ("remove_folder",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (EvolutionStorageClass, remove_folder),
				NULL, NULL,
				e_shell_marshal_NONE__POINTER_STRING_STRING,
				G_TYPE_NONE, 3,
				G_TYPE_POINTER,
				G_TYPE_STRING,
				G_TYPE_STRING);

	signals[XFER_FOLDER] 
		= g_signal_new ("xfer_folder",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (EvolutionStorageClass, xfer_folder),
				NULL, NULL,
				e_shell_marshal_NONE__POINTER_STRING_STRING_BOOL,
				G_TYPE_NONE, 4,
				G_TYPE_POINTER,
				G_TYPE_STRING,
				G_TYPE_STRING,
				G_TYPE_BOOLEAN);
	
	signals[UPDATE_FOLDER] 
		= g_signal_new ("update_folder",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EvolutionStorageClass, update_folder),
				NULL, NULL,
				e_shell_marshal_NONE__STRING_INT,
				G_TYPE_NONE, 2,
				G_TYPE_STRING,
				G_TYPE_INT);

	signals[OPEN_FOLDER] 
		= g_signal_new ("open_folder",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (EvolutionStorageClass, open_folder),
				NULL, NULL,
				e_shell_marshal_NONE__POINTER_STRING,
				G_TYPE_NONE, 2,
				G_TYPE_POINTER,
				G_TYPE_STRING);

	signals[DISCOVER_SHARED_FOLDER] 
		= g_signal_new ("discover_shared_folder",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (EvolutionStorageClass, discover_shared_folder),
				NULL, NULL,
				e_shell_marshal_NONE__POINTER_STRING_STRING,
				G_TYPE_NONE, 3,
				G_TYPE_POINTER,
				G_TYPE_STRING,
				G_TYPE_STRING);

	signals[CANCEL_DISCOVER_SHARED_FOLDER] 
		= g_signal_new ("cancel_discover_shared_folder",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (EvolutionStorageClass, cancel_discover_shared_folder),
				NULL, NULL,
				e_shell_marshal_NONE__STRING_STRING,
				G_TYPE_NONE, 2,
				G_TYPE_STRING,
				G_TYPE_STRING);

	signals[REMOVE_SHARED_FOLDER] 
		= g_signal_new ("remove_shared_folder",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (EvolutionStorageClass, remove_shared_folder),
				NULL, NULL,
				e_shell_marshal_NONE__POINTER_STRING,
				G_TYPE_NONE, 2,
				G_TYPE_POINTER,
				G_TYPE_STRING);

	signals[SHOW_FOLDER_PROPERTIES] 
		= g_signal_new ("show_folder_properties",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (EvolutionStorageClass, show_folder_properties),
				NULL, NULL,
				e_shell_marshal_NONE__STRING_INT_INT,
				G_TYPE_NONE, 3,
				G_TYPE_STRING,
				G_TYPE_INT,
				G_TYPE_INT);
}

static void
evolution_storage_init (EvolutionStorage *storage)
{
	EvolutionStoragePrivate *priv;

	priv = g_new (EvolutionStoragePrivate, 1);
	priv->name                    = NULL;
	priv->has_shared_folders      = FALSE;
	priv->folder_tree             = e_folder_tree_new (folder_destroy_notify, storage);
	priv->uri_to_path             = g_hash_table_new (g_str_hash, g_str_equal);
	priv->corba_storage_listeners = NULL;
	priv->folder_property_items   = NULL;

	storage->priv = priv;
}


void
evolution_storage_construct (EvolutionStorage *storage,
			     const char *name,
			     gboolean has_shared_folders)
{
	EvolutionStoragePrivate *priv;

	g_return_if_fail (EVOLUTION_IS_STORAGE (storage));
	g_return_if_fail (name != NULL);
	g_return_if_fail (name[0] != '\0');

	priv = storage->priv;
	priv->name               = g_strdup (name);
	priv->has_shared_folders = !! has_shared_folders;
}

EvolutionStorage *
evolution_storage_new (const char *name,
		       gboolean has_shared_folders)
{
	EvolutionStorage *new;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (name[0] != '\0', NULL);

	new = g_object_new (evolution_storage_get_type (), 0);

	evolution_storage_construct (new, name, has_shared_folders);

	return new;
}

void
evolution_storage_rename (EvolutionStorage *evolution_storage,
			  const char *new_name)
{
	/* FIXME: Implement me */
}

EvolutionStorageResult
evolution_storage_register (EvolutionStorage *evolution_storage,
			    GNOME_Evolution_StorageRegistry corba_storage_registry)
{
	EvolutionStorageResult result;
	GNOME_Evolution_StorageListener corba_storage_listener;
	GNOME_Evolution_Storage corba_storage;
	EvolutionStoragePrivate *priv;
	CORBA_Environment ev;

	g_return_val_if_fail (evolution_storage != NULL,
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (EVOLUTION_IS_STORAGE (evolution_storage),
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (corba_storage_registry != CORBA_OBJECT_NIL,
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);

	priv = evolution_storage->priv;

	if (priv->corba_storage_listeners != NULL)
		return EVOLUTION_STORAGE_ERROR_ALREADYREGISTERED;

	CORBA_exception_init (&ev);

	corba_storage = bonobo_object_corba_objref (BONOBO_OBJECT (evolution_storage));
	corba_storage_listener = GNOME_Evolution_StorageRegistry_addStorage (corba_storage_registry,
									     corba_storage,
									     priv->name,
									     &ev);

	if (ev._major == CORBA_NO_EXCEPTION) {
		add_listener (evolution_storage, corba_storage_listener);
		result = EVOLUTION_STORAGE_OK;
	} else {
		if (ev._major != CORBA_USER_EXCEPTION)
			result = EVOLUTION_STORAGE_ERROR_CORBA;
		else if (strcmp (CORBA_exception_id (&ev), ex_GNOME_Evolution_StorageRegistry_Exists) == 0)
			result = EVOLUTION_STORAGE_ERROR_EXISTS;
		else
			result = EVOLUTION_STORAGE_ERROR_GENERIC;
	}

	CORBA_exception_free (&ev);

	return result;
}

EvolutionStorageResult
evolution_storage_register_on_shell (EvolutionStorage *evolution_storage,
				     GNOME_Evolution_Shell corba_shell)
{
	GNOME_Evolution_StorageRegistry corba_storage_registry;
	EvolutionStorageResult result;
	CORBA_Environment ev;

	g_return_val_if_fail (evolution_storage != NULL,
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (EVOLUTION_IS_STORAGE (evolution_storage),
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (corba_shell != CORBA_OBJECT_NIL,
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);

	CORBA_exception_init (&ev);

	corba_storage_registry = Bonobo_Unknown_queryInterface (corba_shell,
								"IDL:GNOME/Evolution/StorageRegistry:1.0",
								&ev);
	if (corba_storage_registry == CORBA_OBJECT_NIL || ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		return EVOLUTION_STORAGE_ERROR_NOREGISTRY;
	}

	result = evolution_storage_register (evolution_storage, corba_storage_registry);

	Bonobo_Unknown_unref (corba_storage_registry, &ev);
	CORBA_Object_release (corba_storage_registry, &ev);

	CORBA_exception_free (&ev);

	return result;
}

EvolutionStorageResult
evolution_storage_deregister_on_shell (EvolutionStorage *evolution_storage,
				       GNOME_Evolution_Shell corba_shell)
{
	GNOME_Evolution_StorageRegistry corba_storage_registry;
	EvolutionStorageResult result;
	EvolutionStoragePrivate *priv;
	CORBA_Environment ev;

	g_return_val_if_fail (evolution_storage != NULL,
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (EVOLUTION_IS_STORAGE (evolution_storage),
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (corba_shell != CORBA_OBJECT_NIL,
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);

	priv = evolution_storage->priv;

	CORBA_exception_init (&ev);

	corba_storage_registry = Bonobo_Unknown_queryInterface (corba_shell,
								"IDL:GNOME/Evolution/StorageRegistry:1.0",
								&ev);
	if (corba_storage_registry == CORBA_OBJECT_NIL || ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		return EVOLUTION_STORAGE_ERROR_NOREGISTRY;
	}

	GNOME_Evolution_StorageRegistry_removeStorageByName (corba_storage_registry,
							     priv->name,
							     &ev);

	if (ev._major == CORBA_NO_EXCEPTION)
		result = EVOLUTION_STORAGE_OK;
	else {
		if (ev._major != CORBA_USER_EXCEPTION)
			result = EVOLUTION_STORAGE_ERROR_CORBA;
		else if (strcmp (CORBA_exception_id (&ev), ex_GNOME_Evolution_StorageRegistry_NotFound) == 0)
			result = EVOLUTION_STORAGE_ERROR_EXISTS;
		else
			result = EVOLUTION_STORAGE_ERROR_GENERIC;
	}

	/* Now unref the EvolutionStorage */
	bonobo_object_unref (BONOBO_OBJECT (evolution_storage));

	Bonobo_Unknown_unref (corba_storage_registry, &ev);
	CORBA_Object_release (corba_storage_registry, &ev);

	CORBA_exception_free (&ev);

	return result;
}

static char *
make_full_uri (EvolutionStorage *storage,
	       const char *path)
{
	const char *storage_name;
	char *full_path;

	storage_name = storage->priv->name;

	if (strcmp (path, E_PATH_SEPARATOR_S) == 0)
		full_path = g_strconcat (E_SHELL_URI_PREFIX,
					 E_PATH_SEPARATOR_S, storage_name,
					 NULL);
	else if (! g_path_is_absolute (path))
		full_path = g_strconcat (E_SHELL_URI_PREFIX,
					 E_PATH_SEPARATOR_S, storage_name,
					 E_PATH_SEPARATOR_S, path, NULL);
	else
		full_path = g_strconcat (E_SHELL_URI_PREFIX,
					 E_PATH_SEPARATOR_S, storage_name,
					 path, NULL);

	return full_path;
}

EvolutionStorageResult
evolution_storage_new_folder (EvolutionStorage *evolution_storage,
			      const char *path,
			      const char *display_name,
			      const char *type,
			      const char *physical_uri,
			      const char *description,
			      const char *custom_icon_name,
			      int         unread_count,
			      gboolean    can_sync_offline,
			      int         sorting_priority)
{
	EvolutionStorageResult   result;
	EvolutionStoragePrivate *priv;
	GNOME_Evolution_Folder  *corba_folder;
	CORBA_Environment ev;
	GList *p;
	char *evolutionUri;

	g_return_val_if_fail (evolution_storage != NULL,
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (EVOLUTION_IS_STORAGE (evolution_storage),
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (path != NULL, EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (g_path_is_absolute (path), EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (display_name != NULL, EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (type != NULL, EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (physical_uri != NULL, EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);

	if (description == NULL)
		description = "";

	priv = evolution_storage->priv;

	CORBA_exception_init (&ev);

	corba_folder = GNOME_Evolution_Folder__alloc ();
	corba_folder->displayName     = CORBA_string_dup (display_name);
	corba_folder->description     = CORBA_string_dup (description);
	corba_folder->type            = CORBA_string_dup (type);
	corba_folder->physicalUri     = CORBA_string_dup (physical_uri);
	corba_folder->canSyncOffline  = (CORBA_boolean) can_sync_offline;
	corba_folder->sortingPriority = sorting_priority;

	if (custom_icon_name != NULL)
		corba_folder->customIconName = CORBA_string_dup (custom_icon_name);
	else
		corba_folder->customIconName = CORBA_string_dup ("");

	evolutionUri = make_full_uri (evolution_storage, path);
	corba_folder->evolutionUri = CORBA_string_dup (evolutionUri);
	g_free (evolutionUri);

	corba_folder->unreadCount  = unread_count;

	if (! e_folder_tree_add (priv->folder_tree, path, corba_folder)) {
		CORBA_free (corba_folder);
		return EVOLUTION_STORAGE_ERROR_EXISTS;
	}
	g_hash_table_insert (priv->uri_to_path, g_strdup (physical_uri), g_strdup (path));

	result = EVOLUTION_STORAGE_OK;

	for (p = priv->corba_storage_listeners; p != NULL; p = p->next) {
		GNOME_Evolution_StorageListener listener;

		listener = p->data;
		GNOME_Evolution_StorageListener_notifyFolderCreated (listener, path, corba_folder, &ev);

		if (ev._major == CORBA_NO_EXCEPTION)
			continue;

		if (ev._major != CORBA_USER_EXCEPTION)
			result = EVOLUTION_STORAGE_ERROR_CORBA;
		else if (strcmp (CORBA_exception_id (&ev), ex_GNOME_Evolution_StorageListener_Exists) == 0)
			result = EVOLUTION_STORAGE_ERROR_EXISTS;
		else
			result = EVOLUTION_STORAGE_ERROR_GENERIC;

		break;
	}

	CORBA_exception_free (&ev);

	return result;
}

EvolutionStorageResult
evolution_storage_update_folder (EvolutionStorage *evolution_storage,
				 const char       *path,
				 int               unread_count)
{
	EvolutionStorageResult result;
	EvolutionStoragePrivate *priv;
	GNOME_Evolution_Folder *corba_folder;
	GList *p;

	g_return_val_if_fail (evolution_storage != NULL,
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (EVOLUTION_IS_STORAGE (evolution_storage),
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (path != NULL, EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (g_path_is_absolute (path), EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);

	priv = evolution_storage->priv;

	g_signal_emit (evolution_storage, signals[UPDATE_FOLDER], 0,
		       path, unread_count);

	if (priv->corba_storage_listeners == NULL)
		return EVOLUTION_STORAGE_ERROR_NOTREGISTERED;

	result = EVOLUTION_STORAGE_OK;

	for (p = priv->corba_storage_listeners; p != NULL; p = p->next) {
		GNOME_Evolution_StorageListener listener;
		CORBA_Environment ev;

		CORBA_exception_init (&ev);

		listener = p->data;
		GNOME_Evolution_StorageListener_notifyFolderUpdated (listener, path, unread_count, &ev);

		CORBA_exception_free (&ev);
	}

	if (result == EVOLUTION_STORAGE_OK) {
		corba_folder = e_folder_tree_get_folder (priv->folder_tree, path);
		if (corba_folder != NULL)
			corba_folder->unreadCount = unread_count;
		else
			result = EVOLUTION_STORAGE_ERROR_NOTFOUND;
	}

	return result;
}

EvolutionStorageResult
evolution_storage_update_folder_by_uri (EvolutionStorage *evolution_storage,
					const char       *physical_uri,
					int               unread_count)
{
	EvolutionStoragePrivate *priv;
	char *path;

	g_return_val_if_fail (evolution_storage != NULL,
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (EVOLUTION_IS_STORAGE (evolution_storage),
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (physical_uri != NULL, EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);

	priv = evolution_storage->priv;

	path = g_hash_table_lookup (priv->uri_to_path, physical_uri);
	return evolution_storage_update_folder (evolution_storage, path, unread_count);
}

EvolutionStorageResult
evolution_storage_removed_folder (EvolutionStorage *evolution_storage,
				  const char *path)
{
	EvolutionStorageResult result;
	EvolutionStoragePrivate *priv;
	CORBA_Environment ev;
	GNOME_Evolution_Folder *corba_folder;
	gpointer key, value;
	GList *p;

	g_return_val_if_fail (evolution_storage != NULL,
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (EVOLUTION_IS_STORAGE (evolution_storage),
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (path != NULL, EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (g_path_is_absolute (path), EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);

	priv = evolution_storage->priv;

	if (priv->corba_storage_listeners == NULL)
		return EVOLUTION_STORAGE_ERROR_NOTREGISTERED;

	corba_folder = e_folder_tree_get_folder (priv->folder_tree, path);
	if (corba_folder == NULL)
		return EVOLUTION_STORAGE_ERROR_NOTFOUND;
	if (g_hash_table_lookup_extended (priv->uri_to_path, corba_folder->physicalUri, &key, &value)) {
		g_hash_table_remove (priv->uri_to_path, key);
		g_free (key);
		g_free (value);
	}
	e_folder_tree_remove (priv->folder_tree, path);

	CORBA_exception_init (&ev);

	result = EVOLUTION_STORAGE_OK;

	for (p = priv->corba_storage_listeners; p != NULL; p = p->next) {
		GNOME_Evolution_StorageListener listener;

		listener = p->data;
		GNOME_Evolution_StorageListener_notifyFolderRemoved (listener, path, &ev);
		
		if (ev._major == CORBA_NO_EXCEPTION)
			continue;

		if (ev._major != CORBA_USER_EXCEPTION)
			result = EVOLUTION_STORAGE_ERROR_CORBA;
		else if (strcmp (CORBA_exception_id (&ev), ex_GNOME_Evolution_StorageListener_NotFound) == 0)
			result = EVOLUTION_STORAGE_ERROR_NOTFOUND;
		else
			result = EVOLUTION_STORAGE_ERROR_GENERIC;

		break;
	}

	CORBA_exception_free (&ev);

	return result;
}

gboolean
evolution_storage_folder_exists (EvolutionStorage *evolution_storage,
				 const char *path)
{
	EvolutionStoragePrivate *priv;

	g_return_val_if_fail (EVOLUTION_IS_STORAGE (evolution_storage),
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (path != NULL, EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (g_path_is_absolute (path), EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);

	priv = evolution_storage->priv;

	return e_folder_tree_get_folder (priv->folder_tree, path) != NULL;
}

EvolutionStorageResult
evolution_storage_has_subfolders (EvolutionStorage *evolution_storage,
				  const char       *path,
				  const char       *message)
{
	EvolutionStorageResult result;
	EvolutionStoragePrivate *priv;
	CORBA_Environment ev;
	GList *p;

	g_return_val_if_fail (evolution_storage != NULL,
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (EVOLUTION_IS_STORAGE (evolution_storage),
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (path != NULL, EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (g_path_is_absolute (path), EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (message != NULL, EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);

	priv = evolution_storage->priv;

	if (priv->corba_storage_listeners == NULL)
		return EVOLUTION_STORAGE_ERROR_NOTREGISTERED;

	CORBA_exception_init (&ev);

	result = EVOLUTION_STORAGE_OK;

	for (p = priv->corba_storage_listeners; p != NULL; p = p->next) {
		GNOME_Evolution_StorageListener listener;

		listener = p->data;
		GNOME_Evolution_StorageListener_notifyHasSubfolders (listener, path, message, &ev);

		if (ev._major == CORBA_NO_EXCEPTION)
			continue;
		if (ev._major != CORBA_USER_EXCEPTION)
			result = EVOLUTION_STORAGE_ERROR_CORBA;
		else if (strcmp (CORBA_exception_id (&ev), ex_GNOME_Evolution_StorageListener_NotFound) == 0)
			result = EVOLUTION_STORAGE_ERROR_NOTFOUND;
		else
			result = EVOLUTION_STORAGE_ERROR_GENERIC;

		break;
	}

	CORBA_exception_free (&ev);

	return result;
}


/* Setting up property items.  */

void
evolution_storage_add_property_item  (EvolutionStorage *evolution_storage,
				      const char *label,
				      const char *tooltip,
				      GdkPixbuf *icon)
{
	FolderPropertyItem *item;

	g_return_if_fail (EVOLUTION_IS_STORAGE (evolution_storage));
	g_return_if_fail (label != NULL);

	item = g_new (FolderPropertyItem, 1);
	item->label   = g_strdup (label);
	item->tooltip = g_strdup (tooltip);
	item->icon    = icon;
	if (icon != NULL)
		g_object_ref (icon);

	evolution_storage->priv->folder_property_items = g_slist_append (evolution_storage->priv->folder_property_items,
									 item);
}


BONOBO_TYPE_FUNC_FULL (EvolutionStorage,
		       GNOME_Evolution_Storage,
		       PARENT_TYPE,
		       evolution_storage)
