/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* calendar-component.c
 *
 * Copyright (C) 2003  Ximian, Inc
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
 * Author: Rodrigo Moya <rodrigo@ximian.com>
 */

#include <bonobo/bonobo-i18n.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-xfer.h>
#include <gal/util/e-util.h>
#include <e-util/e-bconf-map.h>
#include "migration.h"

static e_gconf_map_t calendar_display_map[] = {
	/* /Calendar/Display */
	{ "Timezone", "calendar/display/timezone", E_GCONF_MAP_STRING },
	{ "Use24HourFormat", "calendar/display/use_24hour_format", E_GCONF_MAP_BOOL },
	{ "WeekStartDay", "calendar/display/week_start_day", E_GCONF_MAP_INT },
	{ "DayStartHour", "calendar/display/day_start_hour", E_GCONF_MAP_INT },
	{ "DayStartMinute", "calendar/display/day_start_minute", E_GCONF_MAP_INT },
	{ "DayEndHour", "calendar/display/day_end_hour", E_GCONF_MAP_INT },
	{ "DayEndMinute", "calendar/display/day_end_minute", E_GCONF_MAP_INT },
	{ "TimeDivisions", "calendar/display/time_divisions", E_GCONF_MAP_INT },
	{ "View", "calendar/display/default_view", E_GCONF_MAP_INT },
	{ "HPanePosition", "calendar/display/hpane_position", E_GCONF_MAP_FLOAT },
	{ "VPanePosition", "calendar/display/vpane_position", E_GCONF_MAP_FLOAT },
	{ "MonthHPanePosition", "calendar/display/month_hpane_position", E_GCONF_MAP_FLOAT },
	{ "MonthVPanePosition", "calendar/display/month_vpane_position", E_GCONF_MAP_FLOAT },
	{ "CompressWeekend", "calendar/display/compress_weekend", E_GCONF_MAP_BOOL },
	{ "ShowEventEndTime", "calendar/display/show_event_end", E_GCONF_MAP_BOOL },
	{ "WorkingDays", "calendar/display/working_days", E_GCONF_MAP_INT },
	{ 0 },
};

static e_gconf_map_t calendar_tasks_map[] = {
	/* /Calendar/Tasks */
	{ "HideCompletedTasks", "calendar/tasks/hide_completed", E_GCONF_MAP_BOOL },
	{ "HideCompletedTasksUnits", "calendar/tasks/hide_completed_units", E_GCONF_MAP_STRING },
	{ "HideCompletedTasksValue", "calendar/tasks/hide_completed_value", E_GCONF_MAP_INT },
	{ 0 },
};

static e_gconf_map_t calendar_tasks_colours_map[] = {
	/* /Calendar/Tasks/Colors */
	{ "TasksDueToday", "calendar/tasks/colors/due_today", E_GCONF_MAP_STRING },
	{ "TasksOverDue", "calendar/tasks/colors/overdue", E_GCONF_MAP_STRING },
	{ "TasksDueToday", "calendar/tasks/colors/due_today", E_GCONF_MAP_STRING },
	{ 0 },
};

static e_gconf_map_t calendar_other_map[] = {
	/* /Calendar/Other */
	{ "ConfirmDelete", "calendar/prompts/confirm_delete", E_GCONF_MAP_BOOL },
	{ "ConfirmExpunge", "calendar/prompts/confirm_expunge", E_GCONF_MAP_BOOL },
	{ "UseDefaultReminder", "calendar/other/use_default_reminder", E_GCONF_MAP_BOOL },
	{ "DefaultReminderInterval", "calendar/other/default_reminder_interval", E_GCONF_MAP_INT },
	{ "DefaultReminderUnits", "calendar/other/default_reminder_units", E_GCONF_MAP_STRING },
	{ 0 },
};

static e_gconf_map_t calendar_datenavigator_map[] = {
	/* /Calendar/DateNavigator */
	{ "ShowWeekNumbers", "calendar/date_navigator/show_week_numbers", E_GCONF_MAP_BOOL },
	{ 0 },
};

static e_gconf_map_t calendar_alarmnotify_map[] = {
	/* /Calendar/AlarmNotify */
	{ "LastNotificationTime", "calendar/notify/last_notification_time", E_GCONF_MAP_INT },
	{ "CalendarToLoad%i", "calendar/notify/calendars", E_GCONF_MAP_STRING|E_GCONF_MAP_LIST },
	{ "BlessedProgram%i", "calendar/notify/programs", E_GCONF_MAP_STRING|E_GCONF_MAP_LIST },
	{ 0 },
};

e_gconf_map_list_t calendar_remap_list[] = {

	{ "/Calendar/Display", calendar_display_map },
	{ "/Calendar/Other/Map", calendar_other_map },
	{ "/Calendar/DateNavigator", calendar_datenavigator_map },
	{ "/Calendar/AlarmNotify", calendar_alarmnotify_map },

	{ 0 },
};

e_gconf_map_list_t task_remap_list[] = {

	{ "/Calendar/Tasks", calendar_tasks_map },
	{ "/Calendar/Tasks/Colors", calendar_tasks_colours_map },

	{ 0 },
};

static gboolean
process_old_dir (ESourceGroup *source_group, const char *path,
		 const char *filename, const char *name, const char *base_uri)
{
	char *s;
	GnomeVFSURI *from, *to;
	GnomeVFSResult vres;
	ESource *source;
	GDir *dir;
	gboolean retval = TRUE;

	s = g_build_filename (path, filename, NULL);
	if (!g_file_test (s, G_FILE_TEST_EXISTS)) {
		g_free (s);
		return FALSE;
	}

	/* transfer the old file to its new location */
	from = gnome_vfs_uri_new (s);
	g_free (s);
	if (!from)
		return FALSE;

	s = g_build_filename (e_source_group_peek_base_uri (source_group), base_uri,
			      filename, NULL);
	if (e_mkdir_hier (s, 0700) != 0) {
		gnome_vfs_uri_unref (from);
		g_free (s);
		return FALSE;
	}
	to = gnome_vfs_uri_new (s);
	g_free (s);
	if (!to) {
		gnome_vfs_uri_unref (from);
		return FALSE;
	}

	vres = gnome_vfs_xfer_uri ((const GnomeVFSURI *) from,
				   (const GnomeVFSURI *) to,
				   GNOME_VFS_XFER_DEFAULT,
				   GNOME_VFS_XFER_ERROR_MODE_ABORT,
				   GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,
				   NULL, NULL);
	gnome_vfs_uri_unref (from);
	gnome_vfs_uri_unref (to);

	if (vres != GNOME_VFS_OK)
		return FALSE;

	/* Find the default source we create or create a new source */
	source = e_source_group_peek_source_by_name (source_group, name);
	if (!source)
		source = e_source_new (name, base_uri);
	e_source_group_add_source (source_group, source, -1);

	/* process subfolders */
	s = g_build_filename (path, "subfolders", NULL);
	dir = g_dir_open (s, 0, NULL);
	if (dir) {
		const char *name;
		char *tmp_s;

		while ((name = g_dir_read_name (dir))) {
			tmp_s = g_build_filename (s, name, NULL);
			if (g_file_test (tmp_s, G_FILE_TEST_IS_DIR)) {
				retval = process_old_dir (source_group, tmp_s, filename, name, name);
			}

			g_free (tmp_s);
		}

		g_dir_close (dir);
	}

	g_free (s);

	return retval;
}

static ESourceGroup *
create_calendar_contact_source (ESourceList *source_list)
{
	ESourceGroup *group;
	ESource *source;
	
	/* Create the contacts group */
	group = e_source_group_new (_("Contacts"), "contacts://");
	e_source_group_set_readonly (group, TRUE);
	e_source_list_add_group (source_list, group, -1);
	
	source = e_source_new (_("Birthdays & Anniversaries"), "/");
	e_source_group_add_source (group, source, -1);
	g_object_unref (source);

	return group;
}

static gboolean
create_calendar_sources (CalendarComponent *component,
			 ESourceList   *source_list,
			 ESourceGroup **on_this_computer,
			 ESourceGroup **on_the_web,
			 ESourceGroup **contacts)
{
	GSList *groups;

	groups = e_source_list_peek_groups (source_list);
	if (groups) {
		/* groups are already there, we need to search for things... */
		g_warning ("can't migrate when existing groups are present");
		return FALSE;
	} else {
		ESourceGroup *group;
		ESource *source;
		char *base_uri, *base_uri_proto, *new_dir;

		/* Create the local source group */
		base_uri = g_build_filename (calendar_component_peek_base_directory (component),
					     "/calendar/local/OnThisComputer/",
					     NULL);

		base_uri_proto = g_strconcat ("file://", base_uri, NULL);

		group = e_source_group_new (_("On This Computer"), base_uri_proto);
		e_source_list_add_group (source_list, group, -1);

		if (on_this_computer)
			*on_this_computer = group;
		else
			g_object_unref (group);
		
		g_free (base_uri_proto);

		/* Create default calendar */
		new_dir = g_build_filename (base_uri, "Personal/", NULL);
		if (!e_mkdir_hier (new_dir, 0700)) {
			source = e_source_new (_("Personal"), "Personal");
			e_source_group_add_source (group, source, -1);
			g_object_unref (source);
		}
		g_free (new_dir);

		g_free (base_uri);

		/* Create the web group */
		group = e_source_group_new (_("On The Web"), "webcal://");
		e_source_list_add_group (source_list, group, -1);

		if (on_the_web)
			*on_the_web = group;
		else
			g_object_unref (group);
		
		/* Create the contact group */
		group = create_calendar_contact_source (source_list);
		if (contacts)
			*contacts = group;
		else
			g_object_unref (group);
	}

	return TRUE;
}

static gboolean
create_task_sources (TasksComponent *component,
		    ESourceList   *source_list,
		    ESourceGroup **on_this_computer)
{
	GSList *groups;

	groups = e_source_list_peek_groups (source_list);
	if (groups) {
		/* groups are already there, we need to search for things... */
		g_warning ("can't migrate when existing groups are present");
		return FALSE;
	} else {
		ESourceGroup *group;
		ESource *source;
		char *base_uri, *base_uri_proto, *new_dir;

		/* create the local source group */
		base_uri = g_build_filename (tasks_component_peek_base_directory (component),
					     "/tasks/local/OnThisComputer/",
					     NULL);

		base_uri_proto = g_strconcat ("file://", base_uri, NULL);

		group = e_source_group_new (_("On This Computer"), base_uri_proto);
		e_source_list_add_group (source_list, group, -1);

		if (on_this_computer)
			*on_this_computer = group;
		else
			g_object_unref (group);
		
		g_free (base_uri_proto);

		/* Create default task list */
		new_dir = g_build_filename (base_uri, "Personal/", NULL);
		if (!e_mkdir_hier (new_dir, 0700)) {
			source = e_source_new (_("Personal"), "Personal");
			e_source_group_add_source (group, source, -1);
			g_object_unref (source);
		}
		g_free (new_dir);

		g_free (base_uri);
	}

	return TRUE;
}

gboolean
migrate_calendars (CalendarComponent *component, int major, int minor, int revision)
{
	gboolean retval = TRUE;

	if (major == 0 && minor == 0 && revision == 0)
		return create_calendar_sources (component, calendar_component_peek_source_list (component), NULL, NULL, NULL);

	if (major == 1) {
		xmlDocPtr config_doc = NULL;
		char *conf_file;
		struct stat st;

		conf_file = g_build_filename (g_get_home_dir (), "evolution", "config.xmldb", NULL);
		if (lstat (conf_file, &st) == 0 && S_ISREG (st.st_mode)) 
			config_doc = xmlParseFile (conf_file);
		g_free (conf_file);
		
		if (config_doc && minor <= 2) {
			GConfClient *gconf;	
			int res = 0;
			
			/* move bonobo config to gconf */
			gconf = gconf_client_get_default ();
			
			res = e_bconf_import (gconf, config_doc, calendar_remap_list);
			
			g_object_unref (gconf);
			
			xmlFreeDoc(config_doc);

			if (res != 0) {
				g_warning("Could not move config from bonobo-conf to gconf");
				return FALSE;
			}
		}

		if (minor <= 4) {
			ESourceGroup *on_this_computer;
			char *path;

			if (!create_calendar_sources (component, calendar_component_peek_source_list (component), &on_this_computer, NULL, NULL))
				return FALSE;

			/* FIXME Look for all top level calendars */
			path = g_build_filename (g_get_home_dir (), "evolution/local/Calendar", NULL);
			if (!g_file_test (path, G_FILE_TEST_IS_DIR)) {
				g_free (path);
				return FALSE;
			}
			retval = process_old_dir (on_this_computer, path, "calendar.ics", _("Personal"), "Personal");
			g_free (path);
			
			e_source_list_sync (calendar_component_peek_source_list (component), NULL);
		}

		if (minor == 5 && revision < 2) {
			ESourceGroup *group;
			
			group = create_calendar_contact_source (calendar_component_peek_source_list (component));
			g_object_unref (group);
			
			e_source_list_sync (calendar_component_peek_source_list (component), NULL);
		}

	}

	return retval;
}

gboolean
migrate_tasks (TasksComponent *component, int major, int minor, int revision)
{
	gboolean retval = TRUE;

	if (major == 0 && minor == 0 && revision == 0)
		return create_task_sources (component, tasks_component_peek_source_list (component), NULL);

	if (major == 1) {
		xmlDocPtr config_doc = NULL;
		char *conf_file;
		struct stat st;

		conf_file = g_build_filename (g_get_home_dir (), "evolution", "config.xmldb", NULL);
		if (lstat (conf_file, &st) == 0 && S_ISREG (st.st_mode)) 
			config_doc = xmlParseFile (conf_file);
		g_free (conf_file);
		
		if (config_doc && minor <= 2) {
			GConfClient *gconf;	
			int res = 0;
			
			/* move bonobo config to gconf */
			gconf = gconf_client_get_default ();
			
			res = e_bconf_import (gconf, config_doc, task_remap_list);
			
			g_object_unref (gconf);
			
			xmlFreeDoc(config_doc);

			if (res != 0) {
				g_warning("Could not move config from bonobo-conf to gconf");
				return FALSE;
			}
		}

		if (minor <= 4) {
			ESourceGroup *on_this_computer;
			char *path;

			if (!create_task_sources (component, tasks_component_peek_source_list (component), &on_this_computer))
				return FALSE;
			
			/* FIXME Look for all top level tasks */
			path = g_build_filename (g_get_home_dir (), "evolution/local/Tasks", NULL);
			if (!g_file_test (path, G_FILE_TEST_IS_DIR)) {
				g_free (path);
				return FALSE;
			}
			retval = process_old_dir (on_this_computer, path, "tasks.ics", _("Personal"), "Personal");
			g_free (path);
			
			e_source_list_sync (tasks_component_peek_source_list (component), NULL);
		}		
	}
	
        return retval;
}
