/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-shell-component-dnd.c
 *
 * Copyright (C) 2000, 2001 Ximian, Inc.
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
 * Author: Chris Toshok
 */

#include <gal/util/e-util.h>

#include "Evolution.h"
#include "evolution-shell-component-dnd.h"


#define PARENT_TYPE (bonobo_object_get_type ())

static BonoboObjectClass *parent_class;

/* Source Folder stuff */

struct _DndSourceFolderPrivate {
	DndSourceFolderBeginDragFn begin_drag;
	DndSourceFolderGetDataFn get_data;
	DndSourceFolderDeleteDataFn delete_data;
	DndSourceFolderEndDragFn end_drag;
	gpointer user_data;
};

/* GtkObject methods */
static void
dnd_source_destroy (GtkObject *object)
{
	EvolutionShellComponentDndSourceFolder *folder;
	DndSourceFolderPrivate *priv;

	folder = EVOLUTION_SHELL_COMPONENT_DND_SOURCE_FOLDER (object);
	priv = folder->priv;

	g_return_if_fail (priv != NULL);

	g_free (priv);
	
	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
impl_GNOME_Evolution_ShellComponentDnd_SourceFolder_beginDrag (PortableServer_Servant servant, const CORBA_char * physical_uri,
							       const CORBA_char * folder_type, GNOME_Evolution_ShellComponentDnd_ActionSet * possible_actions,
							       GNOME_Evolution_ShellComponentDnd_Action * suggested_action, CORBA_Environment * ev)
{
	BonoboObject *bonobo_object;
	EvolutionShellComponentDndSourceFolder *folder;
	DndSourceFolderPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	folder = EVOLUTION_SHELL_COMPONENT_DND_SOURCE_FOLDER (bonobo_object);
	priv = folder->priv;

	priv->begin_drag (folder, physical_uri, folder_type, possible_actions, suggested_action, priv->user_data);
}

static void 
impl_GNOME_Evolution_ShellComponentDnd_SourceFolder_getData (PortableServer_Servant servant,
							     const GNOME_Evolution_ShellComponentDnd_SourceFolder_Context * source_context,
							     const GNOME_Evolution_ShellComponentDnd_Action action, const CORBA_char * dnd_type,
							     GNOME_Evolution_ShellComponentDnd_Data ** data, CORBA_Environment * ev)
{
	BonoboObject *bonobo_object;
	EvolutionShellComponentDndSourceFolder *folder;
	DndSourceFolderPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	folder = EVOLUTION_SHELL_COMPONENT_DND_SOURCE_FOLDER (bonobo_object);
	priv = folder->priv;

	priv->get_data (folder, source_context, action, dnd_type, data, ev, priv->user_data);
}

static void
impl_GNOME_Evolution_ShellComponentDnd_SourceFolder_deleteData (PortableServer_Servant servant,
								const GNOME_Evolution_ShellComponentDnd_SourceFolder_Context * source_context,
								CORBA_Environment * ev)
{
	BonoboObject *bonobo_object;
	EvolutionShellComponentDndSourceFolder *folder;
	DndSourceFolderPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	folder = EVOLUTION_SHELL_COMPONENT_DND_SOURCE_FOLDER (bonobo_object);
	priv = folder->priv;

	priv->delete_data (folder, source_context, priv->user_data);
}

static void
impl_GNOME_Evolution_ShellComponentDnd_SourceFolder_endDrag (PortableServer_Servant servant,
							     const GNOME_Evolution_ShellComponentDnd_SourceFolder_Context * source_context,
							     CORBA_Environment * ev)
{
	BonoboObject *bonobo_object;
	EvolutionShellComponentDndSourceFolder *folder;
	DndSourceFolderPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	folder = EVOLUTION_SHELL_COMPONENT_DND_SOURCE_FOLDER (bonobo_object);
	priv = folder->priv;

	priv->end_drag (folder, source_context, priv->user_data);
}

static POA_GNOME_Evolution_ShellComponentDnd_SourceFolder__vepv SourceFolder_vepv;

static POA_GNOME_Evolution_ShellComponentDnd_SourceFolder *
create_dnd_source_servant (void)
{
	POA_GNOME_Evolution_ShellComponentDnd_SourceFolder *servant;
	CORBA_Environment ev;

	servant = (POA_GNOME_Evolution_ShellComponentDnd_SourceFolder *)g_new0 (BonoboObjectServant, 1);
	servant->vepv = &SourceFolder_vepv;

	CORBA_exception_init (&ev);
	POA_GNOME_Evolution_ShellComponentDnd_SourceFolder__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	return servant;
}

static void
source_corba_class_init (void)
{
	POA_GNOME_Evolution_ShellComponentDnd_SourceFolder__vepv *vepv;
	POA_GNOME_Evolution_ShellComponentDnd_SourceFolder__epv *epv;
	PortableServer_ServantBase__epv *base_epv;

	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private = NULL;
	base_epv->finalize = NULL;
	base_epv->default_POA = NULL;

	epv = g_new0 (POA_GNOME_Evolution_ShellComponentDnd_SourceFolder__epv, 1);
	epv->beginDrag = impl_GNOME_Evolution_ShellComponentDnd_SourceFolder_beginDrag;
	epv->getData = impl_GNOME_Evolution_ShellComponentDnd_SourceFolder_getData;
	epv->deleteData = impl_GNOME_Evolution_ShellComponentDnd_SourceFolder_deleteData;
	epv->endDrag = impl_GNOME_Evolution_ShellComponentDnd_SourceFolder_endDrag;

	vepv = &SourceFolder_vepv;
	vepv->_base_epv = base_epv;
	vepv->Bonobo_Unknown_epv = bonobo_object_get_epv ();
	vepv->GNOME_Evolution_ShellComponentDnd_SourceFolder_epv = epv;
}

static void
evolution_shell_component_dnd_source_folder_class_init (EvolutionShellComponentDndSourceFolderClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = dnd_source_destroy;

	parent_class = gtk_type_class (PARENT_TYPE);

	source_corba_class_init ();
}

static void
evolution_shell_component_dnd_source_folder_init (EvolutionShellComponentDndSourceFolder *folder)
{
	DndSourceFolderPrivate *priv;

	priv = g_new (DndSourceFolderPrivate, 1);

	folder->priv = priv;
}


E_MAKE_TYPE (evolution_shell_component_dnd_source_folder, "EvolutionShellComponentDndSourceFolder",
	     EvolutionShellComponentDndSourceFolder, evolution_shell_component_dnd_source_folder_class_init,
	     evolution_shell_component_dnd_source_folder_init, PARENT_TYPE);

static void
evolution_shell_component_dnd_source_folder_construct (EvolutionShellComponentDndSourceFolder *dnd_source,
						       DndSourceFolderBeginDragFn begin_drag,
						       DndSourceFolderGetDataFn get_data,
						       DndSourceFolderDeleteDataFn delete_data,
						       DndSourceFolderEndDragFn end_drag,
						       gpointer user_data,
						       GNOME_Evolution_ShellComponentDnd_SourceFolder corba_object)
{
	DndSourceFolderPrivate *priv;

	g_return_if_fail (dnd_source != NULL);
	g_return_if_fail (IS_EVOLUTION_SHELL_COMPONENT_DND_SOURCE_FOLDER (dnd_source));
	g_return_if_fail (corba_object != CORBA_OBJECT_NIL);

	priv = dnd_source->priv;

	priv->begin_drag = begin_drag;
	priv->get_data = get_data;
	priv->delete_data = delete_data;
	priv->end_drag = end_drag;
	priv->user_data = user_data;

	bonobo_object_construct (BONOBO_OBJECT (dnd_source), corba_object);
}

EvolutionShellComponentDndSourceFolder*
evolution_shell_component_dnd_source_folder_new (DndSourceFolderBeginDragFn begin_drag,
						 DndSourceFolderGetDataFn get_data,
						 DndSourceFolderDeleteDataFn delete_data,
						 DndSourceFolderEndDragFn end_drag,
						 gpointer user_data)
{
	EvolutionShellComponentDndSourceFolder *dnd_source;
	POA_GNOME_Evolution_ShellComponentDnd_SourceFolder *servant;
	GNOME_Evolution_ShellComponentDnd_SourceFolder corba_object;

	g_return_val_if_fail (begin_drag != NULL, NULL);
	g_return_val_if_fail (get_data != NULL, NULL);
	g_return_val_if_fail (delete_data != NULL, NULL);
	g_return_val_if_fail (end_drag != NULL, NULL);

	servant = create_dnd_source_servant();
	if (servant == NULL)
		return NULL;

	dnd_source = gtk_type_new (evolution_shell_component_dnd_source_folder_get_type ());
	corba_object = bonobo_object_activate_servant (BONOBO_OBJECT (dnd_source),
						       servant);

	evolution_shell_component_dnd_source_folder_construct (dnd_source,
							       begin_drag, get_data,
							       delete_data, end_drag,
							       user_data,
							       corba_object);
	return dnd_source;
}



/* Destination Folder stuff */

struct _DndDestinationFolderPrivate {
	DndDestinationFolderHandleMotionFn handle_motion;
	DndDestinationFolderHandleDropFn handle_drop;
	gpointer user_data;
};

/* GtkObject methods */
static void
dnd_destination_destroy (GtkObject *object)
{
	EvolutionShellComponentDndDestinationFolder *folder;
	DndDestinationFolderPrivate *priv;

	folder = EVOLUTION_SHELL_COMPONENT_DND_DESTINATION_FOLDER (object);
	priv = folder->priv;

	g_return_if_fail (priv != NULL);

	g_free (priv);
	
	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

/* CORBA interface */
static CORBA_boolean
impl_GNOME_Evolution_ShellComponentDnd_DestinationFolder_handleMotion (PortableServer_Servant servant,
								       const CORBA_char* physical_uri,
								       const GNOME_Evolution_ShellComponentDnd_DestinationFolder_Context * destination_context,
								       GNOME_Evolution_ShellComponentDnd_Action * suggested_action, CORBA_Environment * ev)
{
	BonoboObject *bonobo_object;
	EvolutionShellComponentDndDestinationFolder *folder;
	DndDestinationFolderPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	folder = EVOLUTION_SHELL_COMPONENT_DND_DESTINATION_FOLDER (bonobo_object);
	priv = folder->priv;

	return priv->handle_motion (folder, physical_uri, destination_context, suggested_action, priv->user_data);
}

static CORBA_boolean 
impl_GNOME_Evolution_ShellComponentDnd_DestinationFolder_handleDrop (PortableServer_Servant servant,
								     const CORBA_char* physical_uri,
								     const GNOME_Evolution_ShellComponentDnd_DestinationFolder_Context * destination_context,
								     const GNOME_Evolution_ShellComponentDnd_Action action,
								     const GNOME_Evolution_ShellComponentDnd_Data * data, CORBA_Environment * ev)
{
	BonoboObject *bonobo_object;
	EvolutionShellComponentDndDestinationFolder *folder;
	DndDestinationFolderPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	folder = EVOLUTION_SHELL_COMPONENT_DND_DESTINATION_FOLDER (bonobo_object);
	priv = folder->priv;

	return priv->handle_drop (folder, physical_uri, destination_context, action, data, priv->user_data);
}

static POA_GNOME_Evolution_ShellComponentDnd_DestinationFolder__vepv DestinationFolder_vepv;

static POA_GNOME_Evolution_ShellComponentDnd_DestinationFolder *
create_dnd_destination_servant (void)
{
	POA_GNOME_Evolution_ShellComponentDnd_DestinationFolder *servant;
	CORBA_Environment ev;

	servant = (POA_GNOME_Evolution_ShellComponentDnd_DestinationFolder *)g_new0 (BonoboObjectServant, 1);
	servant->vepv = &DestinationFolder_vepv;

	CORBA_exception_init (&ev);
	POA_GNOME_Evolution_ShellComponentDnd_DestinationFolder__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	return servant;
}

static void
destination_corba_class_init (void)
{
	POA_GNOME_Evolution_ShellComponentDnd_DestinationFolder__vepv *vepv;
	POA_GNOME_Evolution_ShellComponentDnd_DestinationFolder__epv *epv;
	PortableServer_ServantBase__epv *base_epv;

	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private = NULL;
	base_epv->finalize = NULL;
	base_epv->default_POA = NULL;

	epv = g_new0 (POA_GNOME_Evolution_ShellComponentDnd_DestinationFolder__epv, 1);
	epv->handleMotion = impl_GNOME_Evolution_ShellComponentDnd_DestinationFolder_handleMotion;
	epv->handleDrop = impl_GNOME_Evolution_ShellComponentDnd_DestinationFolder_handleDrop;

	vepv = &DestinationFolder_vepv;
	vepv->_base_epv = base_epv;
	vepv->Bonobo_Unknown_epv = bonobo_object_get_epv ();
	vepv->GNOME_Evolution_ShellComponentDnd_DestinationFolder_epv = epv;
}

static void
evolution_shell_component_dnd_destination_folder_class_init (EvolutionShellComponentDndDestinationFolderClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = dnd_destination_destroy;

	parent_class = gtk_type_class (PARENT_TYPE);

	destination_corba_class_init ();
}

static void
evolution_shell_component_dnd_destination_folder_init (EvolutionShellComponentDndDestinationFolder *folder)
{
	DndDestinationFolderPrivate *priv;

	priv = g_new (DndDestinationFolderPrivate, 1);

	folder->priv = priv;
}


E_MAKE_TYPE (evolution_shell_component_dnd_destination_folder, "EvolutionShellComponentDndDestinationFolder",
	     EvolutionShellComponentDndDestinationFolder, evolution_shell_component_dnd_destination_folder_class_init,
	     evolution_shell_component_dnd_destination_folder_init, PARENT_TYPE);

static void
evolution_shell_component_dnd_destination_folder_construct (EvolutionShellComponentDndDestinationFolder *dnd_destination,
							    DndDestinationFolderHandleMotionFn handle_motion,
							    DndDestinationFolderHandleDropFn handle_drop,
							    gpointer user_data,
							    GNOME_Evolution_ShellComponentDnd_DestinationFolder corba_object)
{
	DndDestinationFolderPrivate *priv;

	g_return_if_fail (dnd_destination != NULL);
	g_return_if_fail (IS_EVOLUTION_SHELL_COMPONENT_DND_DESTINATION_FOLDER (dnd_destination));
	g_return_if_fail (corba_object != CORBA_OBJECT_NIL);

	priv = dnd_destination->priv;

	priv->handle_motion = handle_motion;
	priv->handle_drop = handle_drop;
	priv->user_data = user_data;

	bonobo_object_construct (BONOBO_OBJECT (dnd_destination), corba_object);
}

EvolutionShellComponentDndDestinationFolder*
evolution_shell_component_dnd_destination_folder_new (DndDestinationFolderHandleMotionFn handle_motion,
						      DndDestinationFolderHandleDropFn handle_drop,
						      gpointer user_data)
{
	EvolutionShellComponentDndDestinationFolder *dnd_destination;
	POA_GNOME_Evolution_ShellComponentDnd_DestinationFolder *servant;
	GNOME_Evolution_ShellComponentDnd_DestinationFolder corba_object;

	g_return_val_if_fail (handle_motion != NULL, NULL);
	g_return_val_if_fail (handle_drop != NULL, NULL);

	servant = create_dnd_destination_servant();
	if (servant == NULL)
		return NULL;

	dnd_destination = gtk_type_new (evolution_shell_component_dnd_destination_folder_get_type ());
	corba_object = bonobo_object_activate_servant (BONOBO_OBJECT (dnd_destination),
						       servant);

	evolution_shell_component_dnd_destination_folder_construct (dnd_destination,
								    handle_motion, handle_drop,
								    user_data,
								    corba_object);
	return dnd_destination;
}

