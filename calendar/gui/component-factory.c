/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* component-factory.c
 *
 * Copyright (C) 2000  Ximian, Inc.
 * Copyright (C) 2000  Ximian, Inc.
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
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#include <config.h>

#include <errno.h>
#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-directory.h>

#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-context.h>
#include "evolution-shell-component.h"
#include "component-factory.h"
#include "tasks-control-factory.h"
#include "control-factory.h"
#include "calendar-config.h"
#include "tasks-control.h"
#include "tasks-migrate.h"



/* OAFIID for the component.  */
#define COMPONENT_ID "OAFIID:GNOME_Evolution_Calendar_ShellComponent"
 
/* Folder type IDs */
#define FOLDER_CALENDAR "calendar"
#define FOLDER_TASKS "tasks"

/* IDs for user creatable items */
#define CREATE_EVENT_ID "event"
#define CREATE_TASK_ID "task"

char *evolution_dir;

static const EvolutionShellComponentFolderType folder_types[] = {
	{ FOLDER_CALENDAR,
	  "evolution-calendar.png",
	  N_("Calendar"),
	  N_("Folder containing appointments and events"),
	  TRUE, NULL, NULL },
	{ FOLDER_TASKS,
	  "evolution-tasks.png",
	  N_("Tasks"),
	  N_("Folder containing to-do items"),
	  TRUE, NULL, NULL },
	{ NULL, NULL }
};



/* EvolutionShellComponent methods and signals.  */

static EvolutionShellComponentResult
create_view (EvolutionShellComponent *shell_component,
	     const char *physical_uri,
	     const char *type,
	     BonoboControl **control_return,
	     void *closure)
{
	BonoboControl *control;

	if (!g_strcasecmp (type, "calendar")) {
		control = control_factory_new_control ();
		if (!control)
			return EVOLUTION_SHELL_COMPONENT_CORBAERROR;
	} else if (!g_strcasecmp (type, "tasks")) {
		control = tasks_control_new ();
		if (!control)
			return EVOLUTION_SHELL_COMPONENT_CORBAERROR;
	} else {
		return EVOLUTION_SHELL_COMPONENT_UNSUPPORTEDTYPE;
	}

	bonobo_control_set_property (control, "folder_uri", physical_uri, NULL);

	*control_return = control;

	return EVOLUTION_SHELL_COMPONENT_OK;
}

static void
create_folder (EvolutionShellComponent *shell_component,
	       const char *physical_uri,
	       const char *type,
	       const GNOME_Evolution_ShellComponentListener listener,
	       void *closure)
{
	CORBA_Environment ev;
	GnomeVFSURI *uri;

	CORBA_exception_init (&ev);

	if (strcmp (type, FOLDER_CALENDAR) && strcmp (type, FOLDER_TASKS)) {
		GNOME_Evolution_ShellComponentListener_notifyResult (
			listener,
			GNOME_Evolution_ShellComponentListener_UNSUPPORTED_TYPE,
			&ev);
		CORBA_exception_free (&ev);
		return;
	}

	uri = gnome_vfs_uri_new_private (physical_uri, TRUE, TRUE, TRUE);
	if (uri) {
		/* we don't need to do anything */
		GNOME_Evolution_ShellComponentListener_notifyResult (
			listener,
			GNOME_Evolution_ShellComponentListener_OK, &ev);
		gnome_vfs_uri_unref (uri);
	}
	else {
		GNOME_Evolution_ShellComponentListener_notifyResult (
			listener,
			GNOME_Evolution_ShellComponentListener_INVALID_URI,
			&ev);
	}

	CORBA_exception_free (&ev);
}

static void
remove_folder (EvolutionShellComponent *shell_component,
	       const char *physical_uri,
	       const char *type,
	       const GNOME_Evolution_ShellComponentListener listener,
	       void *closure)
{
	GnomeVFSURI *dir_uri, *data_uri, *backup_uri;
	GnomeVFSResult data_result, backup_result;

	/* check type */
	if (strcmp (type, FOLDER_CALENDAR) && strcmp (type, FOLDER_TASKS)) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		GNOME_Evolution_ShellComponentListener_notifyResult (
			listener,
			GNOME_Evolution_ShellComponentListener_UNSUPPORTED_TYPE,
			&ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			g_message ("remove_folder(): Could not notify the listener of "
				   "an unsupported folder type");

		CORBA_exception_free (&ev);
		return;
	}

	/* check URI */
	dir_uri = gnome_vfs_uri_new_private (physical_uri, TRUE, TRUE, TRUE);
	if (!dir_uri) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		GNOME_Evolution_ShellComponentListener_notifyResult (
			listener,
			GNOME_Evolution_ShellComponentListener_INVALID_URI,
			&ev);
		CORBA_exception_free (&ev);
		return;
	}

	/* Compute the URIs of the appropriate files */

	if (strcmp (type, FOLDER_CALENDAR) == 0) {
		data_uri = gnome_vfs_uri_append_file_name (dir_uri, "calendar.ics");
		backup_uri = gnome_vfs_uri_append_file_name (dir_uri, "calendar.ics~");
	} else if (strcmp (type, FOLDER_TASKS) == 0) {
		data_uri = gnome_vfs_uri_append_file_name (dir_uri, "tasks.ics");
		backup_uri = gnome_vfs_uri_append_file_name (dir_uri, "tasks.ics~");
	} else {
		g_assert_not_reached ();
		return;
	}

	if (!data_uri || !backup_uri) {
		CORBA_Environment ev;

		g_message ("remove_folder(): Could not generate the data/backup URIs");

		CORBA_exception_init (&ev);
		GNOME_Evolution_ShellComponentListener_notifyResult (
			listener,
			GNOME_Evolution_ShellComponentListener_INVALID_URI,
			&ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			g_message ("remove_folder(): Could not notify the listener "
				   "of an invalid URI");

		CORBA_exception_free (&ev);

		goto out;
	}

	/* Delete the data and backup files; the shell will take care of the rest */

	data_result = gnome_vfs_unlink_from_uri (data_uri);
	backup_result = gnome_vfs_unlink_from_uri (backup_uri);

	if ((data_result == GNOME_VFS_OK || data_result == GNOME_VFS_ERROR_NOT_FOUND)
	    && (backup_result == GNOME_VFS_OK || backup_result == GNOME_VFS_ERROR_NOT_FOUND)) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		GNOME_Evolution_ShellComponentListener_notifyResult (
			listener,
			GNOME_Evolution_ShellComponentListener_OK,
			&ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			g_message ("remove_folder(): Could not notify the listener about success");

		CORBA_exception_free (&ev);
	} else {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		GNOME_Evolution_ShellComponentListener_notifyResult (
			listener,
			GNOME_Evolution_ShellComponentListener_PERMISSION_DENIED,
			&ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			g_message ("remove_folder(): Could not notify the listener about failure");

		CORBA_exception_free (&ev);
	}

 out:

	gnome_vfs_uri_unref (dir_uri);

	if (data_uri)
		gnome_vfs_uri_unref (data_uri);

	if (backup_uri)
		gnome_vfs_uri_unref (backup_uri);
}

static void
xfer_folder (EvolutionShellComponent *shell_component,
	     const char *source_physical_uri,
	     const char *destination_physical_uri,
	     const char *type,
	     gboolean remove_source,
	     const GNOME_Evolution_ShellComponentListener listener,
	     void *closure)
{
	CORBA_Environment ev;
	GnomeVFSURI *src_uri;
	GnomeVFSURI *dest_uri;
	GnomeVFSResult result;
	GList *file_list;
	GList *l;
	gboolean success = TRUE;

	CORBA_exception_init (&ev);

	/* check type */
	if (strcmp (type, FOLDER_CALENDAR) && strcmp (type, FOLDER_TASKS)) {
		GNOME_Evolution_ShellComponentListener_notifyResult (
			listener,
			GNOME_Evolution_ShellComponentListener_UNSUPPORTED_TYPE,
			&ev);
		CORBA_exception_free (&ev);
		return;
	}

	/* check URIs */
	src_uri = gnome_vfs_uri_new_private (source_physical_uri, TRUE, TRUE, TRUE);
	dest_uri = gnome_vfs_uri_new_private (destination_physical_uri, TRUE, TRUE, TRUE);
	if (!src_uri || ! dest_uri) {
		GNOME_Evolution_ShellComponentListener_notifyResult (
			listener,
			GNOME_Evolution_ShellComponentListener_INVALID_URI,
			&ev);
		gnome_vfs_uri_unref (src_uri);
		gnome_vfs_uri_unref (dest_uri);
		CORBA_exception_free (&ev);
		return;
	}

	gnome_vfs_uri_unref (src_uri);
	gnome_vfs_uri_unref (dest_uri);

	/* remove all files in that directory */
	result = gnome_vfs_directory_list_load (&file_list, source_physical_uri, 0, NULL);
	if (result != GNOME_VFS_OK) {
		GNOME_Evolution_ShellComponentListener_notifyResult (
			listener,
			GNOME_Evolution_ShellComponentListener_INVALID_URI,
                        &ev);
		CORBA_exception_free (&ev);
		return;
	}

	for (l = file_list; l; l = l->next) {
		GnomeVFSFileInfo *file_info;
                GnomeVFSHandle *hin;
                GnomeVFSHandle *hout;
                gpointer buffer;
                GnomeVFSFileSize size;

                file_info = (GnomeVFSFileInfo *) l->data;
                if (!file_info || file_info->name[0] == '.')
                        continue;

                /* open source and destination files */
                src_uri = gnome_vfs_uri_new_private (source_physical_uri, TRUE, TRUE, TRUE);
                src_uri = gnome_vfs_uri_append_file_name (src_uri, file_info->name);

                result = gnome_vfs_open_uri (&hin, src_uri, GNOME_VFS_OPEN_READ);
                gnome_vfs_uri_unref (src_uri);
                if (result != GNOME_VFS_OK) {
                        GNOME_Evolution_ShellComponentListener_notifyResult (
                                listener,
                                GNOME_Evolution_ShellComponentListener_PERMISSION_DENIED,
                                &ev);
                        success = FALSE;
			break;
                }

                dest_uri = gnome_vfs_uri_new_private (destination_physical_uri, TRUE, TRUE, TRUE);
                dest_uri = gnome_vfs_uri_append_file_name (dest_uri, file_info->name);

                result = gnome_vfs_create_uri (&hout, dest_uri, GNOME_VFS_OPEN_WRITE, FALSE, 0);
                gnome_vfs_uri_unref (dest_uri);
                if (result != GNOME_VFS_OK) {
                        gnome_vfs_close (hin);
                        GNOME_Evolution_ShellComponentListener_notifyResult (
                                listener,
                                GNOME_Evolution_ShellComponentListener_PERMISSION_DENIED,
                                &ev);
                        success = FALSE;
                        break;
                }

                /* write source file to destination file */
                buffer = g_malloc (file_info->size);
                result = gnome_vfs_read (hin, buffer, file_info->size, &size);
                if (result != GNOME_VFS_OK) {
			gnome_vfs_close (hin);
                        gnome_vfs_close (hout);
                        g_free (buffer);

                        GNOME_Evolution_ShellComponentListener_notifyResult (
                                listener,
                                GNOME_Evolution_ShellComponentListener_PERMISSION_DENIED,
                                &ev);
                        success = FALSE;
                        break;
                }
                result = gnome_vfs_write (hout, buffer, file_info->size, &size);
                if (result != GNOME_VFS_OK) {
                        gnome_vfs_close (hin);
                        gnome_vfs_close (hout);
                        g_free (buffer);

                        GNOME_Evolution_ShellComponentListener_notifyResult (
                                listener,
                                GNOME_Evolution_ShellComponentListener_PERMISSION_DENIED,
                                &ev);
                        success = FALSE;
			break;
                }

                /* free memory */
                gnome_vfs_close (hin);
                gnome_vfs_close (hout);
                g_free (buffer);
        }

        if (success) {
                GNOME_Evolution_ShellComponentListener_notifyResult (
                        listener,
                        GNOME_Evolution_ShellComponentListener_OK,
                        &ev);
        }

        /* free memory */
        gnome_vfs_file_info_list_free (file_list);
        CORBA_exception_free (&ev);	
}

static GList *shells = NULL;

static void
owner_set_cb (EvolutionShellComponent *shell_component,
	      EvolutionShellClient *shell_client,
	      const char *evolution_homedir,
	      gpointer user_data)
{
	static gboolean migrated = FALSE;
	
	evolution_dir = g_strdup (evolution_homedir);

	if (!migrated) {
		tasks_migrate ();
		migrated = TRUE;
	}

	shells = g_list_append (shells, shell_component);
}

static void
owner_unset_cb (EvolutionShellComponent *shell_component,
	      gpointer user_data)
{
	shells = g_list_remove (shells, shell_component);
	
	if (g_list_length (shells) == 0)
		gtk_main_quit ();
}

/* Computes the final URI for a calendar component */
static char *
get_data_uri (const char *uri, CalComponentVType vtype)
{
	if (uri) {
		if (vtype == CAL_COMPONENT_EVENT)
			return g_concat_dir_and_file (uri, "calendar.ics");
		else if (vtype == CAL_COMPONENT_TODO)
			return g_concat_dir_and_file (uri, "tasks.ics");
		else
			g_assert_not_reached ();
	} else {
		if (vtype == CAL_COMPONENT_EVENT)
			return g_concat_dir_and_file (g_get_home_dir (),
						      "evolution/local/Calendar/calendar.ics");
		else if (vtype == CAL_COMPONENT_TODO)
			return g_concat_dir_and_file (g_get_home_dir (),
						      "evolution/local/Tasks/tasks.ics");
		else
			g_assert_not_reached ();
	}

	return NULL;
}

/* Creates a calendar component at a specified URI.  If the URI is NULL then it
 * uses the default folder for that type of component.
 */
static void
create_component (const char *uri, CalComponentVType vtype)
{
	char *real_uri;
	CORBA_Environment ev;
	GNOME_Evolution_Calendar_CalObjType corba_type;
	GNOME_Evolution_Calendar_CompEditorFactory factory;

	real_uri = get_data_uri (uri, vtype);

	switch (vtype) {
	case CAL_COMPONENT_EVENT:
		corba_type = GNOME_Evolution_Calendar_TYPE_EVENT;
		break;

	case CAL_COMPONENT_TODO:
		corba_type = GNOME_Evolution_Calendar_TYPE_TODO;
		break;

	default:
		g_assert_not_reached ();
		return;
	}

	/* Get the factory */

	CORBA_exception_init (&ev);
	factory = oaf_activate_from_id ("OAFIID:GNOME_Evolution_Calendar_CompEditorFactory",
					0, NULL, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("create_component(): Could not activate the component editor factory");
		CORBA_exception_free (&ev);
		g_free (real_uri);
		return;
	}
	CORBA_exception_free (&ev);

	/* Create the item */

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_CompEditorFactory_editNew (factory, real_uri, corba_type, &ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("create_component(): Exception while creating the component");

	CORBA_exception_free (&ev);
	g_free (real_uri);

	/* Get rid of the factory */

	CORBA_exception_init (&ev);
	bonobo_object_release_unref (factory, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("create_component(): Could not unref the calendar component factory");

	CORBA_exception_free (&ev);
}

/* Callback used when we must create a user-creatable item */
static void
sc_user_create_new_item_cb (EvolutionShellComponent *shell_component,
			    const char *id,
			    const char *parent_folder_physical_uri,
			    const char *parent_folder_type)
{
	if (strcmp (id, CREATE_EVENT_ID) == 0) {
		if (strcmp (parent_folder_type, FOLDER_CALENDAR) == 0)
			create_component (parent_folder_physical_uri, CAL_COMPONENT_EVENT);
		else
			create_component (NULL, CAL_COMPONENT_EVENT);
	} else if (strcmp (id, CREATE_TASK_ID) == 0) {
		if (strcmp (parent_folder_type, FOLDER_TASKS) == 0)
			create_component (parent_folder_physical_uri, CAL_COMPONENT_TODO);
		else
			create_component (NULL, CAL_COMPONENT_TODO);
	} else
		g_assert_not_reached ();
}

#if 0
static void
destroy_cb (EvolutionShellComponent *shell_component,
	    gpointer user_data)
{
	shells = g_list_remove (shells, shell_component);
	
	if (g_list_length (shells) == 0)
		gtk_main_quit ();
}
#endif


/* The factory function.  */

static BonoboObject *
create_object (void)
{
	EvolutionShellComponent *shell_component;
	
	shell_component = evolution_shell_component_new (folder_types,
							 NULL,
							 create_view,
							 create_folder,
							 remove_folder,
							 xfer_folder,
							 NULL, /* populate_folder_context_menu_fn */
							 NULL, /* get_dnd_selection_fn */
							 NULL  /* closure */);

	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_set",
			    GTK_SIGNAL_FUNC (owner_set_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_unset",
			    GTK_SIGNAL_FUNC (owner_unset_cb), NULL);

#if 0
	gtk_signal_connect (GTK_OBJECT (shell_component), "destroy",
			    GTK_SIGNAL_FUNC (destroy_cb), NULL);

	shells = g_list_append (shells, shell_component);
#endif

	/* User creatable items */

	evolution_shell_component_add_user_creatable_item (shell_component,
							   CREATE_EVENT_ID,
							   _("Create a new appointment"),
							   _("New _Appointment"),
							   'a');

	evolution_shell_component_add_user_creatable_item (shell_component,
							   CREATE_TASK_ID,
							   _("Create a new task"),
							   _("New _Task"),
							   't');

	gtk_signal_connect (GTK_OBJECT (shell_component), "user_create_new_item",
			    GTK_SIGNAL_FUNC (sc_user_create_new_item_cb), NULL);

	return BONOBO_OBJECT (shell_component);
}


void
component_factory_init (void)
{
	BonoboObject *object;
	int result;

	object = create_object ();

	result = oaf_active_server_register (COMPONENT_ID, bonobo_object_corba_objref (object));

	if (result == OAF_REG_ERROR)
		g_error ("Cannot initialize Evolution's calendar component.");
}
