/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-shell-component-dnd.c
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
 * Author: Chris Toshok
 */

#include "Evolution.h"
#include "evolution-shell-component-dnd.h"

#include <gal/util/e-util.h>

#include <gtk/gtkobject.h>

#define PARENT_TYPE (bonobo_x_object_get_type ())

static BonoboXObjectClass *parent_class;

/* Source Folder stuff */

struct _DndSourceFolderPrivate {
	DndSourceFolderBeginDragFn begin_drag;
	DndSourceFolderGetDataFn get_data;
	DndSourceFolderDeleteDataFn delete_data;
	DndSourceFolderEndDragFn end_drag;
	gpointer user_data;
};

/* GObject methods */
static void
dnd_source_finalize (GObject *object)
{
	EvolutionShellComponentDndSourceFolder *folder;
	DndSourceFolderPrivate *priv;

	folder = EVOLUTION_SHELL_COMPONENT_DND_SOURCE_FOLDER (object);
	priv = folder->priv;

	g_return_if_fail (priv != NULL);

	g_free (priv);
	
	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
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

static void
evolution_shell_component_dnd_source_folder_class_init (EvolutionShellComponentDndSourceFolderClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = dnd_source_finalize;

	klass->epv.beginDrag  = impl_GNOME_Evolution_ShellComponentDnd_SourceFolder_beginDrag;
	klass->epv.getData    = impl_GNOME_Evolution_ShellComponentDnd_SourceFolder_getData;
	klass->epv.deleteData = impl_GNOME_Evolution_ShellComponentDnd_SourceFolder_deleteData;
	klass->epv.endDrag    = impl_GNOME_Evolution_ShellComponentDnd_SourceFolder_endDrag;

	parent_class = g_type_class_ref(PARENT_TYPE);
}

static void
evolution_shell_component_dnd_source_folder_init (EvolutionShellComponentDndSourceFolder *folder)
{
	DndSourceFolderPrivate *priv;

	priv = g_new (DndSourceFolderPrivate, 1);

	folder->priv = priv;
}

E_MAKE_X_TYPE (evolution_shell_component_dnd_source_folder,
	       "EvolutionShellComponentDndSourceFolder",
	       EvolutionShellComponentDndSourceFolder,
	       evolution_shell_component_dnd_source_folder_class_init,
	       evolution_shell_component_dnd_source_folder_init,
	       PARENT_TYPE,
	       POA_GNOME_Evolution_ShellComponentDnd_SourceFolder__init,
	       GTK_STRUCT_OFFSET (EvolutionShellComponentDndSourceFolderClass, epv))

EvolutionShellComponentDndSourceFolder*
evolution_shell_component_dnd_source_folder_new (DndSourceFolderBeginDragFn begin_drag,
						 DndSourceFolderGetDataFn get_data,
						 DndSourceFolderDeleteDataFn delete_data,
						 DndSourceFolderEndDragFn end_drag,
						 gpointer user_data)
{
	EvolutionShellComponentDndSourceFolder *dnd_source;

	g_return_val_if_fail (begin_drag != NULL, NULL);
	g_return_val_if_fail (get_data != NULL, NULL);
	g_return_val_if_fail (delete_data != NULL, NULL);
	g_return_val_if_fail (end_drag != NULL, NULL);

	dnd_source = g_object_new (evolution_shell_component_dnd_source_folder_get_type (), NULL);

	dnd_source->priv->begin_drag  = begin_drag;
	dnd_source->priv->get_data    = get_data;
	dnd_source->priv->delete_data = delete_data;
	dnd_source->priv->end_drag    = end_drag;
	dnd_source->priv->user_data   = user_data;

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
dnd_destination_finalize (GObject *object)
{
	EvolutionShellComponentDndDestinationFolder *folder;
	DndDestinationFolderPrivate *priv;

	folder = EVOLUTION_SHELL_COMPONENT_DND_DESTINATION_FOLDER (object);
	priv = folder->priv;

	g_return_if_fail (priv != NULL);

	g_free (priv);
	
	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* CORBA interface */
static CORBA_boolean
impl_GNOME_Evolution_ShellComponentDnd_DestinationFolder_handleMotion (PortableServer_Servant servant,
								       const CORBA_char* physical_uri,
								       const CORBA_char *folder_type,
								       const GNOME_Evolution_ShellComponentDnd_DestinationFolder_Context * destination_context,
								       GNOME_Evolution_ShellComponentDnd_Action * suggested_action, CORBA_Environment * ev)
{
	BonoboObject *bonobo_object;
	EvolutionShellComponentDndDestinationFolder *folder;
	DndDestinationFolderPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	folder = EVOLUTION_SHELL_COMPONENT_DND_DESTINATION_FOLDER (bonobo_object);
	priv = folder->priv;

	return priv->handle_motion (folder, physical_uri, folder_type, destination_context, suggested_action, priv->user_data);
}

static CORBA_boolean 
impl_GNOME_Evolution_ShellComponentDnd_DestinationFolder_handleDrop (PortableServer_Servant servant,
								     const CORBA_char *physical_uri,
								     const CORBA_char *folder_type,
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

	return priv->handle_drop (folder, physical_uri, folder_type, destination_context, action, data, priv->user_data);
}

static void
evolution_shell_component_dnd_destination_folder_class_init (EvolutionShellComponentDndDestinationFolderClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = dnd_destination_finalize;

	klass->epv.handleMotion = impl_GNOME_Evolution_ShellComponentDnd_DestinationFolder_handleMotion;
	klass->epv.handleDrop = impl_GNOME_Evolution_ShellComponentDnd_DestinationFolder_handleDrop;

	parent_class = g_type_class_ref(PARENT_TYPE);
}

static void
evolution_shell_component_dnd_destination_folder_init (EvolutionShellComponentDndDestinationFolder *folder)
{
	DndDestinationFolderPrivate *priv;

	priv = g_new (DndDestinationFolderPrivate, 1);

	folder->priv = priv;
}


E_MAKE_X_TYPE (evolution_shell_component_dnd_destination_folder,
	       "EvolutionShellComponentDndDestinationFolder",
	       EvolutionShellComponentDndDestinationFolder,
	       evolution_shell_component_dnd_destination_folder_class_init,
	       evolution_shell_component_dnd_destination_folder_init,
	       PARENT_TYPE,
	       POA_GNOME_Evolution_ShellComponentDnd_DestinationFolder__init,
	       GTK_STRUCT_OFFSET (EvolutionShellComponentDndDestinationFolderClass, epv))

EvolutionShellComponentDndDestinationFolder *
evolution_shell_component_dnd_destination_folder_new (DndDestinationFolderHandleMotionFn handle_motion,
						      DndDestinationFolderHandleDropFn handle_drop,
						      gpointer user_data)
{
	EvolutionShellComponentDndDestinationFolder *dnd_destination;

	g_return_val_if_fail (handle_motion != NULL, NULL);
	g_return_val_if_fail (handle_drop != NULL, NULL);

	dnd_destination = g_object_new (evolution_shell_component_dnd_destination_folder_get_type (), NULL);

	dnd_destination->priv->handle_motion = handle_motion;
	dnd_destination->priv->handle_drop   = handle_drop;
	dnd_destination->priv->user_data     = user_data;

	return dnd_destination;
}

