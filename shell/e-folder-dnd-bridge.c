/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-folder-dnd-bridge.c - Utility functions for handling dnd to Evolution
 * folders using the ShellComponentDnd interface.
 *
 * Copyright (C) 2002 Ximian, Inc.
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
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-folder-dnd-bridge.h"

#include "Evolution.h"
#include "e-storage-set-view.h"

#include <gal/widgets/e-gui-utils.h>

#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>


/* Callbacks for folder operations.  */

static void
folder_xfer_callback (EStorageSet *storage_set,
		      EStorageResult result,
		      void *data)
{
	GtkWindow *parent;

	if (result == E_STORAGE_OK)
		return;

	parent = GTK_WINDOW (data);
	e_notice (parent, GNOME_MESSAGE_BOX_ERROR, _("Cannot transfer folder:\n%s"),
		  e_storage_result_to_string (result));
}


static GNOME_Evolution_ShellComponentDnd_ActionSet
convert_gdk_drag_action_set_to_corba (GdkDragAction action)
{
	GNOME_Evolution_ShellComponentDnd_Action retval;

	retval = GNOME_Evolution_ShellComponentDnd_ACTION_DEFAULT;

	if (action & GDK_ACTION_COPY)
		retval |= GNOME_Evolution_ShellComponentDnd_ACTION_COPY;
	if (action & GDK_ACTION_MOVE)
		retval |= GNOME_Evolution_ShellComponentDnd_ACTION_MOVE;
	if (action & GDK_ACTION_LINK)
		retval |= GNOME_Evolution_ShellComponentDnd_ACTION_LINK;
	if (action & GDK_ACTION_ASK)
		retval |= GNOME_Evolution_ShellComponentDnd_ACTION_ASK;

	return retval;
}

static GNOME_Evolution_ShellComponentDnd_ActionSet
convert_gdk_drag_action_to_corba (GdkDragAction action)
{
	if (action == GDK_ACTION_COPY)
		return GNOME_Evolution_ShellComponentDnd_ACTION_COPY;
	else if (action == GDK_ACTION_MOVE)
		return GNOME_Evolution_ShellComponentDnd_ACTION_MOVE;
	else if (action == GDK_ACTION_LINK)
		return GNOME_Evolution_ShellComponentDnd_ACTION_LINK;
	else if (action == GDK_ACTION_ASK)
		return GNOME_Evolution_ShellComponentDnd_ACTION_ASK;
	else {
		g_warning ("Unknown GdkDragAction %d", action);
		return GNOME_Evolution_ShellComponentDnd_ACTION_DEFAULT;
	}
}

static GdkDragAction
convert_corba_drag_action_to_gdk (GNOME_Evolution_ShellComponentDnd_ActionSet action)
{
	if (action == GNOME_Evolution_ShellComponentDnd_ACTION_COPY)
		return GDK_ACTION_COPY;
	else if (action == GNOME_Evolution_ShellComponentDnd_ACTION_MOVE)
		return GDK_ACTION_MOVE;
	else if (action == GNOME_Evolution_ShellComponentDnd_ACTION_LINK)
		return GDK_ACTION_LINK;
	else if (action == GNOME_Evolution_ShellComponentDnd_ACTION_ASK)
		return GDK_ACTION_ASK;
	else {
		g_warning ("unknown GNOME_Evolution_ShellComponentDnd_ActionSet %d", action);
		return GDK_ACTION_DEFAULT;
	}
}

static EvolutionShellComponentClient *
get_component_at_path (EStorageSet *storage_set,
		       const char *path)
{
	EvolutionShellComponentClient *component_client;
	EFolderTypeRegistry *folder_type_registry;
	EFolder *folder;

	folder = e_storage_set_get_folder (storage_set, path);
	if (folder == NULL)
		return NULL;

	folder_type_registry = e_storage_set_get_folder_type_registry (storage_set);
	g_assert (folder_type_registry != NULL);

	component_client = e_folder_type_registry_get_handler_for_type (folder_type_registry,
									e_folder_get_type_string (folder));

	return component_client;
}

/* This will look for the targets in @drag_context, choose one that matches
   with the allowed types at @path, and return its name.  The EVOLUTION_PATH
   type always matches.  */
static const char *
find_matching_target_for_drag_context (EStorageSet *storage_set,
				       const char *path,
				       GdkDragContext *drag_context)
{
	EFolderTypeRegistry *folder_type_registry;
	EFolder *folder;
	GList *accepted_types;
	GList *p, *q;

	folder_type_registry = e_storage_set_get_folder_type_registry (storage_set);

	folder = e_storage_set_get_folder (storage_set, path);
	if (folder == NULL)
		return E_FOLDER_DND_PATH_TARGET_TYPE;

	accepted_types = e_folder_type_registry_get_accepted_dnd_types_for_type (folder_type_registry,
										 e_folder_get_type_string (folder));

	/* FIXME?  We might make this more efficient.  Currently it takes `n *
	   m' string compares, where `n' is the number of targets in the
	   @drag_context, and `m' is the number of supported types in
	   @folder.  */

	for (p = drag_context->targets; p != NULL; p = p->next) {
		char *possible_type;

		possible_type = gdk_atom_name ((GdkAtom) p->data);
		if (strcmp (possible_type, E_FOLDER_DND_PATH_TARGET_TYPE) == 0) {
			g_free (possible_type);
			return E_FOLDER_DND_PATH_TARGET_TYPE;
		}

		for (q = accepted_types; q != NULL; q = q->next) {
			const char *accepted_type;

			accepted_type = (const char *) q->data;
			if (strcmp (possible_type, accepted_type) == 0) {
				g_free (possible_type);
				return accepted_type;
			}
		}

		g_free (possible_type);
	}

	return NULL;
}

static gboolean
handle_evolution_path_drag_motion (EStorageSet *storage_set,
				   const char *path,
				   GdkDragContext *context,
				   unsigned int time)
{
	GdkModifierType modifiers;
	GdkDragAction action;

	gdk_window_get_pointer (NULL, NULL, NULL, &modifiers);

	if ((modifiers & GDK_CONTROL_MASK) != 0) {
		action = GDK_ACTION_COPY;
	} else {
		GtkWidget *source_widget;

		action = GDK_ACTION_MOVE;

		source_widget = gtk_drag_get_source_widget (context);
		if (source_widget != NULL
		    && E_IS_STORAGE_SET_VIEW (source_widget)) {
			const char *source_path;

			source_path = e_storage_set_view_get_current_folder (E_STORAGE_SET_VIEW (source_widget));
			if (source_path != NULL) {
				EFolder *folder;
				int source_path_len;
				char *destination_path;

				folder = e_storage_set_get_folder (storage_set, source_path);
				if (folder != NULL && e_folder_get_is_stock (folder))
					return FALSE;

				source_path_len = strlen (path);
				if (strcmp (path, source_path) == 0)
					return FALSE;

				destination_path = g_strconcat (path, "/", g_basename (source_path), NULL);
				if (strncmp (destination_path, source_path, source_path_len) == 0) {
					g_free (destination_path);
					return FALSE;
				}

				g_free (destination_path);
			}
		}
	}

	gdk_drag_status (context, action, time);
	return TRUE;
}


gboolean
e_folder_dnd_bridge_motion  (GtkWidget *widget,
			     GdkDragContext *context,
			     unsigned int time,
			     EStorageSet *storage_set,
			     const char *path)
{
	EvolutionShellComponentClient *component_client;
	GNOME_Evolution_ShellComponentDnd_DestinationFolder destination_folder_interface;
	GNOME_Evolution_ShellComponentDnd_DestinationFolder_Context corba_context;
	GNOME_Evolution_ShellComponentDnd_Action suggested_action;
	CORBA_Environment ev;
	CORBA_boolean can_handle;
	EFolder *folder;
	const char *dnd_type;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (context != NULL, FALSE);
	g_return_val_if_fail (E_IS_STORAGE_SET (storage_set), FALSE);
	g_return_val_if_fail (path != NULL, FALSE);

	dnd_type = find_matching_target_for_drag_context (storage_set, path, context);
	if (dnd_type == NULL)
		return FALSE;

	if (strcmp (dnd_type, E_FOLDER_DND_PATH_TARGET_TYPE) == 0)
		return handle_evolution_path_drag_motion (storage_set, path, context, time);

	component_client = get_component_at_path (storage_set, path);
	if (component_client == NULL)
		return FALSE;

	destination_folder_interface = evolution_shell_component_client_get_dnd_destination_interface (component_client);
	if (destination_folder_interface == NULL)
		return FALSE;

	CORBA_exception_init (&ev);

	corba_context.dndType = (char *) dnd_type; /* (Safe cast, as we don't actually free the corba_context.)  */
	corba_context.possibleActions = convert_gdk_drag_action_set_to_corba (context->actions);
	corba_context.suggestedAction = convert_gdk_drag_action_to_corba (context->suggested_action);

	folder = e_storage_set_get_folder (storage_set, path);
	
	can_handle = GNOME_Evolution_ShellComponentDnd_DestinationFolder_handleMotion (destination_folder_interface,
										       e_folder_get_physical_uri (folder),
										       e_folder_get_type_string (folder),
										       &corba_context,
										       &suggested_action,
										       &ev);
	if (ev._major != CORBA_NO_EXCEPTION || ! can_handle) {
		CORBA_exception_free (&ev);
		return FALSE;
	}

	CORBA_exception_free (&ev);

	gdk_drag_status (context, convert_corba_drag_action_to_gdk (suggested_action), time);
	return TRUE;
}

static gboolean
handle_data_received_path (GdkDragContext *context,
			   GtkSelectionData *selection_data,
			   EStorageSet *storage_set,
			   const char *path,
			   GtkWindow *toplevel_window)
{
	const char *source_path;
	char *destination_path;
	gboolean handled;

	source_path = (const char *) selection_data->data;

	/* (Basic sanity checks.)  */
	if (source_path == NULL || source_path[0] != G_DIR_SEPARATOR || source_path[1] == '\0')
		return FALSE;

	destination_path = g_concat_dir_and_file (path, g_basename (source_path));

	switch (context->action) {
	case GDK_ACTION_MOVE:
		e_storage_set_async_xfer_folder (storage_set,
						 source_path,
						 destination_path,
						 TRUE,
						 folder_xfer_callback,
						 toplevel_window);
		handled = TRUE;
		break;
	case GDK_ACTION_COPY:
		e_storage_set_async_xfer_folder (storage_set,
						 source_path,
						 destination_path,
						 FALSE,
						 folder_xfer_callback,
						 toplevel_window);
		handled = TRUE;
		break;
	default:
		handled = FALSE;
		g_warning ("EStorageSetView: Unknown action %d", context->action);
	}

	g_free (destination_path);

	return handled;
}

static gboolean
handle_data_received_non_path (GdkDragContext *context,
			       GtkSelectionData *selection_data,
			       EStorageSet *storage_set,
			       const char *path,
			       const char *target_type)
{
	GNOME_Evolution_ShellComponentDnd_DestinationFolder destination_folder_interface;
	GNOME_Evolution_ShellComponentDnd_DestinationFolder_Context corba_context;
	GNOME_Evolution_ShellComponentDnd_Data corba_data;
	EvolutionShellComponentClient *component_client;
	EFolder *folder;
	CORBA_Environment ev;

	component_client = get_component_at_path (storage_set, path);
	if (component_client == NULL)
		return FALSE;

	destination_folder_interface = evolution_shell_component_client_get_dnd_destination_interface (component_client);
	if (destination_folder_interface == NULL)
		return FALSE;
		
	CORBA_exception_init (&ev);

	folder = e_storage_set_get_folder (storage_set, path);
	
	corba_context.dndType = (char *) target_type;
	corba_context.possibleActions = convert_gdk_drag_action_set_to_corba (context->actions);
	corba_context.suggestedAction = convert_gdk_drag_action_to_corba (context->suggested_action);

	corba_data.format = selection_data->format;
	corba_data.target = selection_data->target;

	corba_data.bytes._release = FALSE;

	if (selection_data->data == NULL) {
		/* If data is NULL the length is -1 and this would mess things
		   up so we handle it separately.  */
		corba_data.bytes._length = 0;
		corba_data.bytes._buffer = NULL;
	} else {
		corba_data.bytes._length = selection_data->length;
		corba_data.bytes._buffer = selection_data->data;
	}

	/* pass off the data to the component's DestinationFolderInterface */
	return GNOME_Evolution_ShellComponentDnd_DestinationFolder_handleDrop (destination_folder_interface,
									       e_folder_get_physical_uri (folder),
									       e_folder_get_type_string (folder),
									       &corba_context,
									       convert_gdk_drag_action_to_corba (context->action),
									       &corba_data,
									       &ev);
}

void
e_folder_dnd_bridge_data_received (GtkWidget *widget,
				   GdkDragContext *context,
				   GtkSelectionData *selection_data,
				   unsigned int time,
				   EStorageSet *storage_set,
				   const char *path)
{
	char *target_type;
	gboolean handled;

	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (context != NULL);
	g_return_if_fail (E_IS_STORAGE_SET (storage_set));
	g_return_if_fail (path != NULL);

	if (selection_data->data == NULL && selection_data->length == -1)
		return;

	target_type = gdk_atom_name (selection_data->target);

	if (strcmp (target_type, E_FOLDER_DND_PATH_TARGET_TYPE) != 0) {
		handled = handle_data_received_non_path (context, selection_data, storage_set,
							 path, target_type);
	} else {
		GtkWindow *toplevel_window;

		toplevel_window = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (widget)));
		handled = handle_data_received_path (context, selection_data, storage_set, path,
						     toplevel_window);
	}

	g_free (target_type);
	gtk_drag_finish (context, handled, FALSE, time);
}

