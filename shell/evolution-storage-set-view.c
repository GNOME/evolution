/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-storage-set-view.c
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

#include "evolution-storage-set-view.h"
#include <gal/util/e-util.h>


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
storage_set_view_widget_storage_selected_cb (EStorageSetView *storage_set_view_widget,
					     const char *name,
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
		GNOME_Evolution_StorageSetViewListener_notifyStorageSelected (listener, name, &ev);

		/* FIXME: What if we fail?  */

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

static POA_GNOME_Evolution_StorageSetView__vepv StorageSetView_vepv;

static POA_GNOME_Evolution_StorageSetView *
create_servant (void)
{
	POA_GNOME_Evolution_StorageSetView *servant;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	servant = (POA_GNOME_Evolution_StorageSetView *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &StorageSetView_vepv;

	POA_GNOME_Evolution_StorageSetView__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		servant = NULL;
	}

	CORBA_exception_free (&ev);

	return servant;
}

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
impl_StorageSetView__get_show_folders (PortableServer_Servant servant,
				       CORBA_Environment * ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorageSetView *storage_set_view;
	EvolutionStorageSetViewPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	storage_set_view = EVOLUTION_STORAGE_SET_VIEW (bonobo_object);
	priv = storage_set_view->priv;

	return (CORBA_boolean)e_storage_set_view_get_show_folders (
			 E_STORAGE_SET_VIEW(priv->storage_set_view_widget));
}

static void
impl_StorageSetView__set_show_folders (PortableServer_Servant servant,
				       const CORBA_boolean value,
				       CORBA_Environment * ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorageSetView *storage_set_view;
	EvolutionStorageSetViewPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	storage_set_view = EVOLUTION_STORAGE_SET_VIEW (bonobo_object);
	priv = storage_set_view->priv;

	e_storage_set_view_set_show_folders (
		          E_STORAGE_SET_VIEW(priv->storage_set_view_widget),
			  (gboolean)value);
}


/* GtkObject methods.  */

static void
impl_destroy (GtkObject *object)
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

	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
corba_class_init (void)
{
	POA_GNOME_Evolution_StorageSetView__vepv *vepv;
	POA_GNOME_Evolution_StorageSetView__epv *epv;
	PortableServer_ServantBase__epv *base_epv;

	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private    = NULL;
	base_epv->finalize    = NULL;
	base_epv->default_POA = NULL;

	epv = g_new0 (POA_GNOME_Evolution_StorageSetView__epv, 1);
	epv->addListener      = impl_StorageSetView_add_listener;
	epv->removeListener   = impl_StorageSetView_remove_listener;
	epv->_set_showFolders = impl_StorageSetView__set_show_folders;
	epv->_get_showFolders = impl_StorageSetView__get_show_folders;

	vepv = &StorageSetView_vepv;
	vepv->_base_epv                    = base_epv;
	vepv->Bonobo_Unknown_epv           = bonobo_object_get_epv ();
	vepv->GNOME_Evolution_StorageSetView_epv = epv;
}

static void
class_init (EvolutionStorageSetViewClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = impl_destroy;

	parent_class = gtk_type_class (bonobo_object_get_type ());

	corba_class_init ();
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
				      GNOME_Evolution_StorageSetView corba_object,
				      EStorageSetView *storage_set_view_widget)
{
	EvolutionStorageSetViewPrivate *priv;

	g_return_if_fail (storage_set_view != NULL);
	g_return_if_fail (EVOLUTION_IS_STORAGE_SET_VIEW (storage_set_view));
	g_return_if_fail (corba_object != CORBA_OBJECT_NIL);
	g_return_if_fail (storage_set_view_widget != NULL);
	g_return_if_fail (E_IS_STORAGE_SET_VIEW (storage_set_view_widget));

	priv = storage_set_view->priv;

	bonobo_object_construct (BONOBO_OBJECT (storage_set_view), corba_object);

	g_assert (priv->storage_set_view_widget == NULL);
	priv->storage_set_view_widget = GTK_WIDGET (storage_set_view_widget);

	gtk_signal_connect (GTK_OBJECT (priv->storage_set_view_widget), "folder_selected",
			    GTK_SIGNAL_FUNC (storage_set_view_widget_folder_selected_cb), storage_set_view);
	gtk_signal_connect (GTK_OBJECT (priv->storage_set_view_widget), "storage_selected",
			    GTK_SIGNAL_FUNC (storage_set_view_widget_storage_selected_cb), storage_set_view);
}

EvolutionStorageSetView *
evolution_storage_set_view_new (EStorageSetView *storage_set_view_widget)
{
	POA_GNOME_Evolution_StorageSetView *servant;
	GNOME_Evolution_StorageSetView corba_object;
	EvolutionStorageSetView *new;

	g_return_val_if_fail (storage_set_view_widget != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE_SET_VIEW (storage_set_view_widget), NULL);

	servant = create_servant ();
	if (servant == NULL)
		return NULL;

	new = gtk_type_new (evolution_storage_set_view_get_type ());

	corba_object = bonobo_object_activate_servant (BONOBO_OBJECT (new), servant);

	evolution_storage_set_view_construct (new, corba_object, storage_set_view_widget);

	return new;
}


E_MAKE_TYPE (evolution_storage_set_view, "EvolutionStorageSetView", EvolutionStorageSetView, class_init, init, PARENT_TYPE)
