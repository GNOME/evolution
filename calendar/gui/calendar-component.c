/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* component-factory.c
 *
 * Copyright (C) 2000, 2001, 2002, 2003  Ximian, Inc.
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

#include <config.h>

#include <errno.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-directory.h>
#include <libgnomevfs/gnome-vfs-file-info.h>

#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-exception.h>

#include "evolution-shell-component.h"
#include "calendar-offline-handler.h"
#include "calendar-component.h"
#include "tasks-control.h"
#include "control-factory.h"
#include "calendar-config.h"
#include "tasks-control.h"
#include "e-comp-editor-registry.h"
#include "dialogs/comp-editor.h"


/* OAFIID for the component.  */
#define COMPONENT_ID "OAFIID:GNOME_Evolution_Calendar_ShellComponent"
 
/* Folder type IDs */
#define FOLDER_CALENDAR "calendar"
#define FOLDER_TASKS "tasks"
#define FOLDER_PUBLIC_CALENDAR "calendar/public"
#define FOLDER_PUBLIC_TASKS "tasks/public"

/* IDs for user creatable items */
#define CREATE_EVENT_ID "event"
#define CREATE_ALLDAY_EVENT_ID "allday-event"
#define CREATE_MEETING_ID "meeting"
#define CREATE_TASK_ID "task"

char *evolution_dir = NULL;
EvolutionShellClient *global_shell_client = NULL;
extern ECompEditorRegistry *comp_editor_registry;

static const EvolutionShellComponentFolderType folder_types[] = {
	{ FOLDER_CALENDAR,
	  "evolution-calendar.png",
	  N_("Calendar"),
	  N_("Folder containing appointments and events"),
	  TRUE, NULL, NULL },
	{ FOLDER_PUBLIC_CALENDAR,
	  "evolution-calendar.png",
	  N_("Public Calendar"),
	  N_("Public folder containing appointments and events"),
	  FALSE, NULL, NULL },
	{ FOLDER_TASKS,
	  "evolution-tasks.png",
	  N_("Tasks"),
	  N_("Folder containing to-do items"),
	  TRUE, NULL, NULL },
	{ FOLDER_PUBLIC_TASKS,
	  "evolution-tasks.png",
	  N_("Public Tasks"),
	  N_("Public folder containing to-do items"),
	  FALSE, NULL, NULL },
	{ NULL, NULL }
};



static inline gboolean
type_is_calendar (const char *type)
{
	return !strcmp (type, FOLDER_CALENDAR) ||
		!strcmp (type, FOLDER_PUBLIC_CALENDAR);
}

static inline gboolean
type_is_tasks (const char *type)
{
	return !strcmp (type, FOLDER_TASKS) ||
		!strcmp (type, FOLDER_PUBLIC_TASKS);
}

/* EvolutionShellComponent methods and signals.  */

static EvolutionShellComponentResult
create_view (EvolutionShellComponent *shell_component,
	     const char *physical_uri,
	     const char *type,
	     const char *view_info,
	     BonoboControl **control_return,
	     void *closure)
{
	BonoboControl *control;

	if (type_is_calendar (type)) {
		control = control_factory_new_control ();
		if (!control)
			return EVOLUTION_SHELL_COMPONENT_CORBAERROR;
	} else if (type_is_tasks (type)) {
		control = tasks_control_new ();
		if (!control)
			return EVOLUTION_SHELL_COMPONENT_CORBAERROR;
	} else {
		return EVOLUTION_SHELL_COMPONENT_UNSUPPORTEDTYPE;
	}

	bonobo_control_set_property (control, NULL, "folder_uri", TC_CORBA_string, physical_uri, NULL);
	if (type_is_calendar (type) && *view_info)
		bonobo_control_set_property (control, NULL, "view", TC_CORBA_string, view_info, NULL);

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

	if (!type_is_calendar (type) && !type_is_tasks (type)) {
		GNOME_Evolution_ShellComponentListener_notifyResult (
			listener,
			GNOME_Evolution_ShellComponentListener_UNSUPPORTED_TYPE,
			&ev);
		CORBA_exception_free (&ev);
		return;
	}

	uri = gnome_vfs_uri_new (physical_uri);
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

/* Asks the alarm daemon to stop monitoring the specified URI */
static void
stop_alarms (GnomeVFSURI *uri)
{
	char *str_uri;
	CORBA_Environment ev;
	GNOME_Evolution_Calendar_AlarmNotify an;

	/* Activate the alarm notification service */

	CORBA_exception_init (&ev);
	an = bonobo_activation_activate_from_id ("OAFIID:GNOME_Evolution_Calendar_AlarmNotify", 0, NULL, &ev);

	if (BONOBO_EX (&ev)) {
		g_message ("stop_alarms(): Could not activate the alarm notification service");
		CORBA_exception_free (&ev);
		return;
	}
	CORBA_exception_free (&ev);

	/* Ask the service to remove the URI from its list of calendars */

	str_uri = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
	g_assert (str_uri != NULL);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_AlarmNotify_removeCalendar (an, str_uri, &ev);
	g_free (str_uri);

	if (BONOBO_USER_EX (&ev, ex_GNOME_Evolution_Calendar_AlarmNotify_InvalidURI)) {
		g_message ("stop_alarms(): Invalid URI reported from the alarm notification service");
	} else if (BONOBO_USER_EX (&ev, ex_GNOME_Evolution_Calendar_AlarmNotify_NotFound)) {
		/* This is OK; the service may not have loaded that calendar */
	} else if (BONOBO_EX (&ev)) {
		g_message ("stop_alarms(): Could not issue the removeCalendar request");
	}
	
	CORBA_exception_free (&ev);

	/* Get rid of the service */

	CORBA_exception_init (&ev);
	bonobo_object_release_unref (an, &ev);
	if (BONOBO_EX (&ev))
		g_message ("stop_alarms(): Could not unref the alarm notification service");
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
	if (!type_is_calendar (type) && !type_is_tasks (type)) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		GNOME_Evolution_ShellComponentListener_notifyResult (
			listener,
			GNOME_Evolution_ShellComponentListener_UNSUPPORTED_TYPE,
			&ev);

		if (BONOBO_EX (&ev))
			g_message ("remove_folder(): Could not notify the listener of "
				   "an unsupported folder type");

		CORBA_exception_free (&ev);
		return;
	}

	/* check URI */
	dir_uri = gnome_vfs_uri_new (physical_uri);
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

	if (type_is_calendar (type)) {
		data_uri = gnome_vfs_uri_append_file_name (dir_uri, "calendar.ics");
		backup_uri = gnome_vfs_uri_append_file_name (dir_uri, "calendar.ics~");
	} else if (type_is_tasks (type)) {
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

		if (BONOBO_EX (&ev))
			g_message ("remove_folder(): Could not notify the listener "
				   "of an invalid URI");

		CORBA_exception_free (&ev);

		goto out;
	}

	/* Ask the alarm daemon to stop monitoring this URI */

	stop_alarms (data_uri);

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

		if (BONOBO_EX (&ev))
			g_message ("remove_folder(): Could not notify the listener about success");

		CORBA_exception_free (&ev);
	} else {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		GNOME_Evolution_ShellComponentListener_notifyResult (
			listener,
			GNOME_Evolution_ShellComponentListener_PERMISSION_DENIED,
			&ev);

		if (BONOBO_EX (&ev))
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

static GNOME_Evolution_ShellComponentListener_Result
xfer_file (GnomeVFSURI *base_src_uri,
	   GnomeVFSURI *base_dest_uri,
	   const char *file_name,
	   int remove_source)
{
	GnomeVFSURI *src_uri, *dest_uri;
	GnomeVFSHandle *hin, *hout;
	GnomeVFSResult result;
	GnomeVFSFileInfo file_info;
	GnomeVFSFileSize size;
	char *buffer;

	src_uri = gnome_vfs_uri_append_file_name (base_src_uri, file_name);

	result = gnome_vfs_open_uri (&hin, src_uri, GNOME_VFS_OPEN_READ);
	if (result == GNOME_VFS_ERROR_NOT_FOUND) {
		gnome_vfs_uri_unref (src_uri);
		return GNOME_Evolution_ShellComponentListener_OK; /* No need to xfer anything.  */
	}
	if (result != GNOME_VFS_OK) {
		gnome_vfs_uri_unref (src_uri);
		return GNOME_Evolution_ShellComponentListener_PERMISSION_DENIED;
	}

	result = gnome_vfs_get_file_info_uri (src_uri, &file_info, GNOME_VFS_FILE_INFO_DEFAULT);
	if (result != GNOME_VFS_OK) {
		gnome_vfs_uri_unref (src_uri);
		return GNOME_Evolution_ShellComponentListener_PERMISSION_DENIED;
	}

	dest_uri = gnome_vfs_uri_append_file_name (base_dest_uri, file_name);

	result = gnome_vfs_create_uri (&hout, dest_uri, GNOME_VFS_OPEN_WRITE, FALSE, 0600);
	if (result != GNOME_VFS_OK) {
		gnome_vfs_close (hin);
		gnome_vfs_uri_unref (src_uri);
		gnome_vfs_uri_unref (dest_uri);
		return GNOME_Evolution_ShellComponentListener_PERMISSION_DENIED;
	}

	/* write source file to destination file */
	buffer = g_malloc (file_info.size);
	result = gnome_vfs_read (hin, buffer, file_info.size, &size);
	if (result != GNOME_VFS_OK) {
		gnome_vfs_close (hin);
		gnome_vfs_close (hout);
		gnome_vfs_uri_unref (src_uri);
		gnome_vfs_uri_unref (dest_uri);
		g_free (buffer);
		return GNOME_Evolution_ShellComponentListener_PERMISSION_DENIED;
	}

	result = gnome_vfs_write (hout, buffer, file_info.size, &size);
	if (result != GNOME_VFS_OK) {
		gnome_vfs_close (hin);
		gnome_vfs_close (hout);
		gnome_vfs_uri_unref (src_uri);
		gnome_vfs_uri_unref (dest_uri);
		g_free (buffer);
		return GNOME_Evolution_ShellComponentListener_PERMISSION_DENIED;
	}

	if (remove_source) {
		char *text_uri;

		/* Sigh, we have to do this as there is no gnome_vfs_unlink_uri(). :-(  */

		text_uri = gnome_vfs_uri_to_string (src_uri, GNOME_VFS_URI_HIDE_NONE);
		result = gnome_vfs_unlink (text_uri);
		g_free (text_uri);
	}

	gnome_vfs_close (hin);
	gnome_vfs_close (hout);
	gnome_vfs_uri_unref (src_uri);
	gnome_vfs_uri_unref (dest_uri);
	g_free (buffer);

	return GNOME_Evolution_ShellComponentListener_OK;
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
	char *filename, *backup_filename;

	CORBA_exception_init (&ev);

	/* check type */
	if (!type_is_calendar (type) && !type_is_tasks (type)) {
		GNOME_Evolution_ShellComponentListener_notifyResult (
			listener,
			GNOME_Evolution_ShellComponentListener_UNSUPPORTED_TYPE,
			&ev);
		CORBA_exception_free (&ev);
		return;
	}

	/* check URIs */
	src_uri = gnome_vfs_uri_new (source_physical_uri);
	dest_uri = gnome_vfs_uri_new (destination_physical_uri);
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

	if (type_is_calendar (type)) {
		filename = "calendar.ics";
		backup_filename = "calendar.ics~";
	} else if (type_is_tasks (type)) {
		filename = "tasks.ics";
		backup_filename = "tasks.ics~";
	} else {
		g_assert_not_reached ();
		return;
	}

	result = xfer_file (src_uri, dest_uri, filename, remove_source);
	if (result == GNOME_Evolution_ShellComponentListener_OK)
		result = xfer_file (src_uri, dest_uri, backup_filename, remove_source);
	
	GNOME_Evolution_ShellComponentListener_notifyResult (listener, result, &ev);

	gnome_vfs_uri_unref (src_uri);
	gnome_vfs_uri_unref (dest_uri);

        CORBA_exception_free (&ev);	
}

static gboolean
request_quit (EvolutionShellComponent *shell_component, void *closure)
{
	return e_comp_editor_registry_close_all (comp_editor_registry);
}

static void
owner_set_cb (EvolutionShellComponent *shell_component,
	      EvolutionShellClient *shell_client,
	      const char *evolution_homedir,
	      gpointer user_data)
{
	if (evolution_dir)
		g_free (evolution_dir);
	evolution_dir = g_strdup (evolution_homedir);
	global_shell_client = shell_client;
}

static void
owner_unset_cb (EvolutionShellComponent *shell_component,
		gpointer user_data)
{
	global_shell_client = NULL;
}

/* Computes the final URI for a calendar component */
static char *
get_data_uri (const char *uri, CalComponentVType vtype)
{
	if (uri) {
		if (*uri != '/' && strncmp (uri, "file:", 5) != 0)
			return g_strdup (uri);

		if (vtype == CAL_COMPONENT_EVENT)
			return cal_util_expand_uri ((char *) uri, FALSE);
		else if (vtype == CAL_COMPONENT_TODO)
			return cal_util_expand_uri ((char *) uri, TRUE);
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
create_component (const char *uri, GNOME_Evolution_Calendar_CompEditorFactory_CompEditorMode type)
{
	char *real_uri;
	CORBA_Environment ev;
	GNOME_Evolution_Calendar_CompEditorFactory factory;
	CalComponentVType vtype;

	switch (type) {
	case GNOME_Evolution_Calendar_CompEditorFactory_EDITOR_MODE_EVENT:
	case GNOME_Evolution_Calendar_CompEditorFactory_EDITOR_MODE_ALLDAY_EVENT:
	case GNOME_Evolution_Calendar_CompEditorFactory_EDITOR_MODE_MEETING:
		vtype = CAL_COMPONENT_EVENT;
		break;
	case GNOME_Evolution_Calendar_CompEditorFactory_EDITOR_MODE_TODO:
		vtype = CAL_COMPONENT_TODO;
		break;
	default:
		g_assert_not_reached ();
		return;
	}

	real_uri = get_data_uri (uri, vtype);

	/* Get the factory */

	CORBA_exception_init (&ev);
	factory = bonobo_activation_activate_from_id ("OAFIID:GNOME_Evolution_Calendar_CompEditorFactory",
						      0, NULL, &ev);

	if (BONOBO_EX (&ev)) {
		g_message ("create_component(): Could not activate the component editor factory");
		CORBA_exception_free (&ev);
		g_free (real_uri);
		return;
	}
	CORBA_exception_free (&ev);

	/* Create the item */

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_CompEditorFactory_editNew (factory, real_uri, type, &ev);

	if (BONOBO_EX (&ev))
		g_message ("create_component(): Exception while creating the component");

	CORBA_exception_free (&ev);
	g_free (real_uri);

	/* Get rid of the factory */

	CORBA_exception_init (&ev);
	bonobo_object_release_unref (factory, &ev);
	if (BONOBO_EX (&ev))
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
	char *tmp_uri;

	if (strcmp (id, CREATE_EVENT_ID) == 0) {
		if (type_is_calendar (parent_folder_type))
			create_component (parent_folder_physical_uri, 
					  GNOME_Evolution_Calendar_CompEditorFactory_EDITOR_MODE_EVENT);
		else {
			tmp_uri = calendar_config_default_calendar_folder ();
			create_component (tmp_uri,
					  GNOME_Evolution_Calendar_CompEditorFactory_EDITOR_MODE_EVENT);
			g_free (tmp_uri);
		}
 	} else if (strcmp (id, CREATE_ALLDAY_EVENT_ID) == 0) {
		if (type_is_calendar (parent_folder_type))
			create_component (parent_folder_physical_uri, 
					  GNOME_Evolution_Calendar_CompEditorFactory_EDITOR_MODE_ALLDAY_EVENT);
		else {
			tmp_uri = calendar_config_default_calendar_folder ();
			create_component (tmp_uri,
					  GNOME_Evolution_Calendar_CompEditorFactory_EDITOR_MODE_ALLDAY_EVENT);
			g_free (tmp_uri);
		}
	} else if (strcmp (id, CREATE_MEETING_ID) == 0) {
		if (type_is_calendar (parent_folder_type))
			create_component (parent_folder_physical_uri,
					  GNOME_Evolution_Calendar_CompEditorFactory_EDITOR_MODE_MEETING);
		else {
			tmp_uri = calendar_config_default_calendar_folder ();
			create_component (tmp_uri,
					  GNOME_Evolution_Calendar_CompEditorFactory_EDITOR_MODE_MEETING);
			g_free (tmp_uri);
		}
	} else if (strcmp (id, CREATE_TASK_ID) == 0) {
		if (type_is_tasks (parent_folder_type))
			create_component (parent_folder_physical_uri,
					  GNOME_Evolution_Calendar_CompEditorFactory_EDITOR_MODE_TODO);
		else {
			tmp_uri = calendar_config_default_tasks_folder ();
			create_component (tmp_uri, 
					  GNOME_Evolution_Calendar_CompEditorFactory_EDITOR_MODE_TODO);
			g_free (tmp_uri);
		}
	} else
		g_assert_not_reached ();
}


/* The factory function.  */

static void
add_creatable_item (EvolutionShellComponent *shell_component,
		    const char *id,
		    const char *description,
		    const char *menu_description,
		    const char *tooltip,
		    const char *folder_type,
		    char menu_shortcut,
		    const char *icon_name)
{
	char *icon_path;
	GdkPixbuf *icon;

	if (icon_name == NULL) {
		icon_path = NULL;
		icon = NULL;
	} else {
		icon_path = g_concat_dir_and_file (EVOLUTION_IMAGESDIR, icon_name);
		icon = gdk_pixbuf_new_from_file (icon_path, NULL);
	}

	evolution_shell_component_add_user_creatable_item (shell_component,
							   id,
							   description,
							   menu_description,
							   tooltip,
							   folder_type,
							   menu_shortcut,
							   icon);

	if (icon != NULL)
		gdk_pixbuf_unref (icon);
	g_free (icon_path);
}

static BonoboObject *
create_object (void)
{
	EvolutionShellComponent *shell_component;
	CalendarOfflineHandler *offline_handler;	

	shell_component = evolution_shell_component_new (folder_types,
							 NULL,
							 create_view,
							 create_folder,
							 remove_folder,
							 xfer_folder,
							 NULL, /* populate_folder_context_menu_fn */
							 NULL, /* unpopulate_folder_context_menu_fn */
							 NULL, /* get_dnd_selection_fn */
							 request_quit,
							 NULL  /* closure */);

	/* Offline handler */
	offline_handler = calendar_offline_handler_new ();
	bonobo_object_add_interface (BONOBO_OBJECT (shell_component), 
				     BONOBO_OBJECT (offline_handler));
	
	g_signal_connect (shell_component, "owner_set", G_CALLBACK (owner_set_cb), NULL);
	g_signal_connect (shell_component, "owner_unset", G_CALLBACK (owner_unset_cb), NULL);

	/* User creatable items */

	add_creatable_item (shell_component, CREATE_EVENT_ID,
			    _("New appointment"), _("_Appointment"),
			    _("Create a new appointment"),
			    FOLDER_CALENDAR, 'a', "new_appointment.xpm");

	add_creatable_item (shell_component, CREATE_MEETING_ID,
			    _("New meeting"), _("M_eeting"),
			    _("Create a new meeting request"),
			    FOLDER_CALENDAR, 'e', "meeting-request-16.png");

	add_creatable_item (shell_component, CREATE_TASK_ID,
			    _("New task"), _("_Task"),
			    _("Create a new task"),
			    FOLDER_TASKS, 't', "new_task-16.png");

	add_creatable_item (shell_component, CREATE_ALLDAY_EVENT_ID,
			    _("New All Day Appointment"), _("All _Day Appointment"),
			    _("Create a new all-day appointment"),
			    FOLDER_CALENDAR, 'd', "new_all_day_event.png");

	g_signal_connect (shell_component, "user_create_new_item",
			  G_CALLBACK (sc_user_create_new_item_cb), NULL);

	return BONOBO_OBJECT (shell_component);
}


BonoboObject *
calendar_component_get_object (void)
{
	static BonoboObject *object = NULL;

	if (object != NULL) {
		bonobo_object_ref (BONOBO_OBJECT (object));
	} else {
		object = create_object ();
		g_object_add_weak_pointer (G_OBJECT (object), (void *) &object);
	}

	return object;
}
