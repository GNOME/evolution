/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-shell-component-dnd.h
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

#ifndef EVOLUTION_SHELL_COMPONENT_DND_H
#define EVOLUTION_SHELL_COMPONENT_DND_H

#include <bonobo/bonobo-object.h>
#include <gtk/gtktypeutils.h>

#include "Evolution.h"

#ifdef cplusplus
extern "C" {
#pragma }
#endif /* cplusplus */

/* Source Folder stuff */
#define EVOLUTION_SHELL_COMPONENT_DND_SOURCE_FOLDER_TYPE (evolution_shell_component_dnd_source_folder_get_type ())
#define EVOLUTION_SHELL_COMPONENT_DND_SOURCE_FOLDER(obj) (GTK_CHECK_CAST ((obj), EVOLUTION_SHELL_COMPONENT_DND_SOURCE_FOLDER_TYPE, EvolutionShellComponentDndSourceFolder))
#define EVOLUTION_SHELL_COMPONENT_DND_SOURCE_FOLDER_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), EVOLUTION_SHELL_COMPONENT_DND_SOURCE_FOLDER_TYPE, EvolutionShellComponentDndSourceFolderClass))
#define IS_EVOLUTION_SHELL_COMPONENT_DND_SOURCE_FOLDER(obj) (GTK_CHECK_TYPE ((obj), EVOLUTION_SHELL_COMPONENT_DND_SOURCE_FOLDER_TYPE))
#define IS_EVOLUTION_SHELL_COMPONENT_DND_SOURCE_FOLDER_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), EVOLUTION_SHELL_COMPONENT_DND_SOURCE_FOLDER_TYPE))

typedef struct _DndSourceFolderPrivate DndSourceFolderPrivate;
typedef struct _EvolutionShellComponentDndSourceFolder EvolutionShellComponentDndSourceFolder;
typedef struct _EvolutionShellComponentDndSourceFolderClass EvolutionShellComponentDndSourceFolderClass;

typedef void (*DndSourceFolderBeginDragFn)(EvolutionShellComponentDndSourceFolder *folder,
					   const char *physical_uri,
					   const char *folder_type,
					   GNOME_Evolution_ShellComponentDnd_ActionSet *possible_actions_return,
					   GNOME_Evolution_ShellComponentDnd_Action *suggested_action_return,
					   gpointer closure);
typedef void (*DndSourceFolderGetDataFn)(EvolutionShellComponentDndSourceFolder *folder,
					 const GNOME_Evolution_ShellComponentDnd_SourceFolder_Context * source_context,
					 const GNOME_Evolution_ShellComponentDnd_Action action,
					 const char * dnd_type,
					 GNOME_Evolution_ShellComponentDnd_Data ** data_return,
					 CORBA_Environment *ev,
					 gpointer closure);
typedef void (*DndSourceFolderDeleteDataFn)(EvolutionShellComponentDndSourceFolder *folder,
					    const GNOME_Evolution_ShellComponentDnd_SourceFolder_Context *source_context,
					    gpointer closure);
typedef void (*DndSourceFolderEndDragFn)(EvolutionShellComponentDndSourceFolder *folder,
					 const GNOME_Evolution_ShellComponentDnd_SourceFolder_Context *source_context,
					 gpointer closure);

struct _EvolutionShellComponentDndSourceFolder {
	BonoboObject object;
	DndSourceFolderPrivate *priv;
};

struct _EvolutionShellComponentDndSourceFolderClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_ShellComponentDnd_SourceFolder__epv epv;
};

GtkType evolution_shell_component_dnd_source_folder_get_type (void);

EvolutionShellComponentDndSourceFolder*
evolution_shell_component_dnd_source_folder_new (DndSourceFolderBeginDragFn begin_drag,
						 DndSourceFolderGetDataFn get_data,
						 DndSourceFolderDeleteDataFn delete_data,
						 DndSourceFolderEndDragFn end_drag,
						 gpointer user_data);



/* Destination Folder stuff */
#define EVOLUTION_SHELL_COMPONENT_DND_DESTINATION_FOLDER_TYPE (evolution_shell_component_dnd_destination_folder_get_type ())
#define EVOLUTION_SHELL_COMPONENT_DND_DESTINATION_FOLDER(obj) (GTK_CHECK_CAST ((obj), EVOLUTION_SHELL_COMPONENT_DND_DESTINATION_FOLDER_TYPE, EvolutionShellComponentDndDestinationFolder))
#define EVOLUTION_SHELL_COMPONENT_DND_DESTINATION_FOLDER_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), EVOLUTION_SHELL_COMPONENT_DND_DESTINATION_FOLDER_TYPE, EvolutionShellComponentDndDestinationFolderClass))
#define IS_EVOLUTION_SHELL_COMPONENT_DND_DESTINATION_FOLDER(obj) (GTK_CHECK_TYPE ((obj), EVOLUTION_SHELL_COMPONENT_DND_DESTINATION_FOLDER_TYPE))
#define IS_EVOLUTION_SHELL_COMPONENT_DND_DESTINATION_FOLDER_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), EVOLUTION_SHELL_COMPONENT_DND_DESTINATION_FOLDER_TYPE))

typedef struct _DndDestinationFolderPrivate DndDestinationFolderPrivate;
typedef struct _EvolutionShellComponentDndDestinationFolder EvolutionShellComponentDndDestinationFolder;
typedef struct _EvolutionShellComponentDndDestinationFolderClass EvolutionShellComponentDndDestinationFolderClass;

typedef CORBA_boolean (*DndDestinationFolderHandleMotionFn)(EvolutionShellComponentDndDestinationFolder *folder,
							    const char *physical_uri,
							    const char *folder_type,
							    const GNOME_Evolution_ShellComponentDnd_DestinationFolder_Context * destination_context,
							    GNOME_Evolution_ShellComponentDnd_Action * suggested_action_return,
							    gpointer closure);
typedef CORBA_boolean (*DndDestinationFolderHandleDropFn)(EvolutionShellComponentDndDestinationFolder *folder,
							  const char *physical_uri,
							  const char *folder_type,
							  const GNOME_Evolution_ShellComponentDnd_DestinationFolder_Context * destination_context,
							  const GNOME_Evolution_ShellComponentDnd_Action action,
							  const GNOME_Evolution_ShellComponentDnd_Data * data,
							  gpointer closure);

struct _EvolutionShellComponentDndDestinationFolder {
	BonoboObject object;
	DndDestinationFolderPrivate *priv;
};

struct _EvolutionShellComponentDndDestinationFolderClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_ShellComponentDnd_DestinationFolder__epv epv;
};

GtkType evolution_shell_component_dnd_destination_folder_get_type (void);

EvolutionShellComponentDndDestinationFolder*
evolution_shell_component_dnd_destination_folder_new (DndDestinationFolderHandleMotionFn handle_motion,
						      DndDestinationFolderHandleDropFn handle_drop,
						      gpointer user_data);

#ifdef cplusplus
}
#endif /* cplusplus */

#endif /* EVOLUTION_SHELL_COMPONENT_DND_H */
