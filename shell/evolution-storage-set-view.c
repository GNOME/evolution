/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-storage-set-view.c
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

#include "evolution-storage-set-view.h"
#include "e-shell-constants.h"

#include <gal/util/e-util.h>

#include <gtk/gtksignal.h>

#include <string.h>


#define PARENT_TYPE bonobo_object_get_type ()
static BonoboObjectClass *parent_class = NULL;

struct _EvolutionStorageSetViewPrivate {
	GtkWidget *storage_set_view_widget;
	GList *listeners;
};


/* EStorageSet widget callbacks.  */

static void
storage_set_view_widget_folder_selected_cb (EStorageSetView *storage_set_view_widget,
					    const char *uri,
					    void *data)
{
	EvolutionStorageSetView *storage_set_view;
	EvolutionStorageSetViewPrivate *priv;
	GList *p;

	storage_set_view = EVOLUTION_STORAGE_SET_VIEW (data);
	priv = storage_set_view->priv;

	for (p = priv->listeners; p != NULL; p = p->next) {
		CORBA_Environment ev;
		GNOME_Evolution_StorageSetViewListener listener;

		CORBA_exception_init (&ev);

		listener = (GNOME_Evolution_StorageSetViewListener) p->data;
		GNOME_Evolution_StorageSetViewListener_notifyFolderSelected (listener, uri, &ev);

		/* FIXME: What if we fail?  */

		CORBA_exception_free (&ev);
	}
}

static void
storage_set_view_widget_folder_toggled_cb (EStorageSetView *storage_set_view_widget,
					   void *data)
{
	EvolutionStorageSetView *storage_set_view;
	EvolutionStorageSetViewPrivate *priv;
	GList *p;

	storage_set_view = EVOLUTION_STORAGE_SET_VIEW (data);
	priv = storage_set_view->priv;

	for (p = priv->listeners; p != NULL; p = p->next) {
		CORBA_Environment ev;
		GNOME_Evolution_StorageSetViewListener listener;

		CORBA_exception_init (&ev);

		listener = (GNOME_Evolution_StorageSetViewListener) p->data;
		GNOME_Evolution_StorageSetViewListener_notifyFolderToggled (listener, &ev);

		/* FIXME: What if we fail? */

		CORBA_exception_free (&ev);
	}
}


/* Listener handling.  */

static GList *
find_listener_in_list (GNOME_Evolution_StorageSetViewListener listener,
		       GList *list)
{
	CORBA_Environment ev;
	GList *p;

	CORBA_exception_init (&ev);

	for (p = list; p != NULL; p = p->next) {
		GNOME_Evolution_StorageSetViewListener listener_item;

		listener_item = (GNOME_Evolution_StorageSetViewListener) p->data;
		if (CORBA_Object_is_equivalent (listener, listener_item, &ev))
			break;
	}

	CORBA_exception_free (&ev);

	return p;
}

static gboolean
add_listener (EvolutionStorageSetView *storage_set_view,
	      GNOME_Evolution_StorageSetViewListener listener)
{
	EvolutionStorageSetViewPrivate *priv;
	CORBA_Environment ev;
	const char *current_uri;
	GNOME_Evolution_StorageSetViewListener copy_of_listener;

	priv = storage_set_view->priv;

	if (find_listener_in_list (listener, priv->listeners) != NULL)
		return FALSE;

	CORBA_exception_init (&ev);

	copy_of_listener = CORBA_Object_duplicate (listener, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		return FALSE;
	}

	priv->listeners = g_list_prepend (priv->listeners, copy_of_listener);

	current_uri = e_storage_set_view_get_current_folder (E_STORAGE_SET_VIEW (priv->storage_set_view_widget));
	if (current_uri != NULL)
		GNOME_Evolution_StorageSetViewListener_notifyFolderSelected (listener, current_uri, &ev);

	CORBA_exception_free (&ev);

	return TRUE;
}

static gboolean
remove_listener (EvolutionStorageSetView *storage_set_view,
		 GNOME_Evolution_StorageSetViewListener listener)
{
	EvolutionStorageSetViewPrivate *priv;
	GList *listener_node;
	CORBA_Environment ev;

	priv = storage_set_view->priv;

	listener_node = find_listener_in_list (listener, priv->listeners);
	if (listener_node == NULL)
		return FALSE;

	CORBA_exception_init (&ev);
	CORBA_Object_release ((CORBA_Object) listener_node->data, &ev);
	CORBA_exception_free (&ev);

	priv->listeners = g_list_remove_link (priv->listeners, listener_node);

	return TRUE;
}


/* CORBA interface implementation.  */

static void
impl_StorageSetView_add_listener (PortableServer_Servant servant,
				  const GNOME_Evolution_StorageSetViewListener listener,
				  CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorageSetView *storage_set_view;

	bonobo_object = bonobo_object_from_servant (servant);
	storage_set_view = EVOLUTION_STORAGE_SET_VIEW (bonobo_object);

	if (! add_listener (storage_set_view, listener))
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_StorageSetView_AlreadyListening, NULL);
}

static void
impl_StorageSetView_remove_listener (PortableServer_Servant servant,
				     const GNOME_Evolution_StorageSetViewListener listener,
				     CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorageSetView *storage_set_view;

	bonobo_object = bonobo_object_from_servant (servant);
	storage_set_view = EVOLUTION_STORAGE_SET_VIEW (bonobo_object);

	if (! remove_listener (storage_set_view, listener))
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_StorageSetView_NotFound, NULL);
}

static CORBA_boolean
impl_StorageSetView__get_showFolders (PortableServer_Servant servant,
				      CORBA_Environment * ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorageSetView *storage_set_view;
	EvolutionStorageSetViewPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	storage_set_view = EVOLUTION_STORAGE_SET_VIEW (bonobo_object);
	priv = storage_set_view->priv;

	return e_storage_set_view_get_show_folders (E_STORAGE_SET_VIEW (priv->storage_set_view_widget));
}

static void
impl_StorageSetView__set_showFolders (PortableServer_Servant servant,
				      const CORBA_boolean value,
				      CORBA_Environment * ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorageSetView *storage_set_view;
	EvolutionStorageSetViewPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	storage_set_view = EVOLUTION_STORAGE_SET_VIEW (bonobo_object);
	priv = storage_set_view->priv;

	e_storage_set_view_set_show_folders (E_STORAGE_SET_VIEW (priv->storage_set_view_widget), value);
}

static void
impl_StorageSetView__set_showCheckboxes (PortableServer_Servant servant,
					 const CORBA_boolean value,
					 CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorageSetView *storage_set_view;
	EvolutionStorageSetViewPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	storage_set_view = EVOLUTION_STORAGE_SET_VIEW (bonobo_object);
	priv = storage_set_view->priv;

	e_storage_set_view_set_show_checkboxes (E_STORAGE_SET_VIEW (priv->storage_set_view_widget),
						value, NULL, NULL);
}

static CORBA_boolean
impl_StorageSetView__get_showCheckboxes (PortableServer_Servant servant,
					 CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorageSetView *storage_set_view;
	EvolutionStorageSetViewPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	storage_set_view = EVOLUTION_STORAGE_SET_VIEW (bonobo_object);
	priv = storage_set_view->priv;

	return e_storage_set_view_get_show_checkboxes (E_STORAGE_SET_VIEW (priv->storage_set_view_widget));
}

static void
impl_StorageSetView__set_checkedFolders (PortableServer_Servant servant,
					 const GNOME_Evolution_FolderList *list,
					 CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorageSetView *storage_set_view;
	EvolutionStorageSetViewPrivate *priv;
	GList *path_list = NULL;
	int i;
	
	bonobo_object = bonobo_object_from_servant (servant);
	storage_set_view = EVOLUTION_STORAGE_SET_VIEW (bonobo_object);
	priv = storage_set_view->priv;

	for (i = 0; i < list->_length; i++) {
		if (strncmp (list->_buffer[i].evolutionUri, "evolution:", 10) != 0)
			continue;

		path_list = g_list_append (path_list, g_strdup (list->_buffer[i].evolutionUri + 10));
	}

	e_storage_set_view_set_checkboxes_list (E_STORAGE_SET_VIEW (priv->storage_set_view_widget),
						path_list);

	e_free_string_list (path_list);
}

static GNOME_Evolution_FolderList *
impl_StorageSetView__get_checkedFolders (PortableServer_Servant servant,
					 CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorageSetView *storage_set_view;
	EvolutionStorageSetViewPrivate *priv;
	EStorageSet *storage_set;
	GNOME_Evolution_FolderList *return_list;
	GList *path_list;
	GList *p;
	int num_folders;
	int i;

	bonobo_object = bonobo_object_from_servant (servant);
	storage_set_view = EVOLUTION_STORAGE_SET_VIEW (bonobo_object);
	priv = storage_set_view->priv;

	path_list = e_storage_set_view_get_checkboxes_list (E_STORAGE_SET_VIEW (priv->storage_set_view_widget));
	num_folders = g_list_length (path_list);

	return_list = GNOME_Evolution_FolderList__alloc ();
	return_list->_maximum = num_folders;
	return_list->_length  = num_folders;
	return_list->_buffer  = CORBA_sequence_GNOME_Evolution_Folder_allocbuf (return_list->_maximum);

	storage_set = e_storage_set_view_get_storage_set (E_STORAGE_SET_VIEW (priv->storage_set_view_widget));

	for (p = path_list, i = 0; p != NULL; p = p->next, i ++) {
		EFolder *folder;
		const char *path;
		char *evolution_uri;

		path = (const char *) p->data;

		folder = e_storage_set_get_folder (storage_set, path);
		if (folder == NULL) {
			g_warning ("Cannot find folder -- %s", path);

			/* Subtract here so that we don't start putting
			   ininitialised blanks into the CORBA list */
			return_list->_length--;
			i--;
			continue;
		}

		evolution_uri = g_strconcat (E_SHELL_URI_PREFIX, path, NULL);
		e_folder_to_corba (folder, evolution_uri, return_list->_buffer + i);
		g_free (evolution_uri);
	}

	e_free_string_list (path_list);

	CORBA_sequence_set_release (return_list, TRUE);
	return return_list;
}


/* GObject methods.  */

static void
impl_finalize (GObject *object)
{
	EvolutionStorageSetView *storage_set_view;
	EvolutionStorageSetViewPrivate *priv;
	CORBA_Environment ev;
	GList *p;

	storage_set_view = EVOLUTION_STORAGE_SET_VIEW (object);
	priv = storage_set_view->priv;

	CORBA_exception_init (&ev);

	for (p = priv->listeners; p != NULL; p = p->next) {
		GNOME_Evolution_StorageSetViewListener listener;

		listener = (GNOME_Evolution_StorageSetViewListener) p->data;
		CORBA_Object_release (listener, &ev);
	}

	CORBA_exception_free (&ev);

	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


static void
class_init (EvolutionStorageSetViewClass *klass)
{
	POA_GNOME_Evolution_StorageSetView__epv *epv;
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = impl_finalize;

	epv = & (klass->epv);
	epv->addListener         = impl_StorageSetView_add_listener;
	epv->removeListener      = impl_StorageSetView_remove_listener;
	epv->_set_showFolders    = impl_StorageSetView__set_showFolders;
	epv->_get_showFolders    = impl_StorageSetView__get_showFolders;
	epv->_set_showCheckboxes = impl_StorageSetView__set_showCheckboxes;
	epv->_get_showCheckboxes = impl_StorageSetView__get_showCheckboxes;
	epv->_set_checkedFolders = impl_StorageSetView__set_checkedFolders;
	epv->_get_checkedFolders = impl_StorageSetView__get_checkedFolders;

	parent_class = gtk_type_class (PARENT_TYPE);
}

static void
init (EvolutionStorageSetView *storage_set_view)
{
	EvolutionStorageSetViewPrivate *priv;

	priv = g_new (EvolutionStorageSetViewPrivate, 1);
	priv->storage_set_view_widget = NULL;
	priv->listeners               = NULL;

	storage_set_view->priv = priv;
}


void
evolution_storage_set_view_construct (EvolutionStorageSetView *storage_set_view,
				      EStorageSetView *storage_set_view_widget)
{
	EvolutionStorageSetViewPrivate *priv;

	g_return_if_fail (EVOLUTION_IS_STORAGE_SET_VIEW (storage_set_view));
	g_return_if_fail (E_IS_STORAGE_SET_VIEW (storage_set_view_widget));

	priv = storage_set_view->priv;

	g_assert (priv->storage_set_view_widget == NULL);
	priv->storage_set_view_widget = GTK_WIDGET (storage_set_view_widget);

	g_signal_connect (priv->storage_set_view_widget, "folder_selected",
			  G_CALLBACK (storage_set_view_widget_folder_selected_cb), storage_set_view);
	g_signal_connect (priv->storage_set_view_widget, "checkboxes_changed",
			  G_CALLBACK (storage_set_view_widget_folder_toggled_cb), storage_set_view);
}

EvolutionStorageSetView *
evolution_storage_set_view_new (EStorageSetView *storage_set_view_widget)
{
	EvolutionStorageSetView *new;

	g_return_val_if_fail (storage_set_view_widget != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE_SET_VIEW (storage_set_view_widget), NULL);

	new = g_object_new (evolution_storage_set_view_get_type (), NULL);

	evolution_storage_set_view_construct (new, storage_set_view_widget);

	return new;
}


E_MAKE_X_TYPE (evolution_storage_set_view, "EvolutionStorageSetView", EvolutionStorageSetView,
	       class_init, init, PARENT_TYPE,
	       POA_GNOME_Evolution_StorageSetView__init,
	       GTK_STRUCT_OFFSET (EvolutionStorageSetViewClass, epv))
