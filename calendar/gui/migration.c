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

#include <string.h>
#include <bonobo/bonobo-i18n.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-xfer.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkmain.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkprogressbar.h>
#include <gal/util/e-util.h>
#include <libecal/e-cal.h>
#include <e-util/e-bconf-map.h>
#include <e-util/e-folder-map.h>
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

static GtkWidget *window;
static GtkLabel *label;
static GtkProgressBar *progress;

static void
setup_progress_dialog (gboolean tasks)
{
	GtkWidget *vbox, *hbox, *w;
	
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title ((GtkWindow *) window, _("Migrating..."));
	gtk_window_set_modal ((GtkWindow *) window, TRUE);
	gtk_container_set_border_width ((GtkContainer *) window, 6);
	
	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);
	gtk_container_add ((GtkContainer *) window, vbox);
	
	if (tasks)
		w = gtk_label_new (_("The location and hierarchy of the Evolution task "
				     "folders has changed since Evolution 1.x.\n\nPlease be "
				     "patient while Evolution migrates your folders..."));
	else
		w = gtk_label_new (_("The location and hierarchy of the Evolution calendar "
				     "folders has changed since Evolution 1.x.\n\nPlease be "
				     "patient while Evolution migrates your folders..."));

	gtk_label_set_line_wrap ((GtkLabel *) w, TRUE);
	gtk_widget_show (w);
	gtk_box_pack_start_defaults ((GtkBox *) vbox, w);
	
	hbox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (hbox);
	gtk_box_pack_start_defaults ((GtkBox *) vbox, hbox);
	
	label = (GtkLabel *) gtk_label_new ("");
	gtk_widget_show ((GtkWidget *) label);
	gtk_box_pack_start_defaults ((GtkBox *) hbox, (GtkWidget *) label);
	
	progress = (GtkProgressBar *) gtk_progress_bar_new ();
	gtk_widget_show ((GtkWidget *) progress);
	gtk_box_pack_start_defaults ((GtkBox *) hbox, (GtkWidget *) progress);
	
	gtk_widget_show (window);
}

static void
dialog_close (void)
{
	gtk_widget_destroy ((GtkWidget *) window);
}

static void
dialog_set_folder_name (const char *folder_name)
{
	char *text;
	
	text = g_strdup_printf (_("Migrating `%s':"), folder_name);
	gtk_label_set_text (label, text);
	g_free (text);
	
	gtk_progress_bar_set_fraction (progress, 0.0);
	
	while (gtk_events_pending ())
		gtk_main_iteration ();
}

static void
dialog_set_progress (double percent)
{
	char text[5];
	
	snprintf (text, sizeof (text), "%d%%", (int) (percent * 100.0f));
	
	gtk_progress_bar_set_fraction (progress, percent);
	gtk_progress_bar_set_text (progress, text);
	
	while (gtk_events_pending ())
		gtk_main_iteration ();
}

static gboolean
check_for_conflict (ESourceGroup *group, char *name)
{
	GSList *sources;
	GSList *s;

	sources = e_source_group_peek_sources (group);

	for (s = sources; s; s = s->next) {
		ESource *source = E_SOURCE (s->data);

		if (!strcmp (e_source_peek_name (source), name))
			return TRUE;
	}

	return FALSE;
}

static char *
get_source_name (ESourceGroup *group, const char *path)
{
	char **p = g_strsplit (path, "/", 0);
	int i, j, starting_index;
	int num_elements;
	gboolean conflict;
	GString *s = g_string_new ("");

	for (i = 0; p[i]; i ++) ;

	num_elements = i;
	i--;

	/* p[i] is now the last path element */

	/* check if it conflicts */
	starting_index = i;
	do {
		g_string_assign (s, "");
		for (j = starting_index; j < num_elements; j += 2) {
			if (j != starting_index)
				g_string_append_c (s, '_');
			g_string_append (s, p[j]);
		}

		conflict = check_for_conflict (group, s->str);


		/* if there was a conflict back up 2 levels (skipping the /subfolder/ element) */
		if (conflict)
			starting_index -= 2;

		/* we always break out if we can't go any further,
		   regardless of whether or not we conflict. */
		if (starting_index < 0)
			break;

	} while (conflict);

	return g_string_free (s, FALSE);
}

static gboolean
migrate_ical (ECal *old_ecal, ECal *new_ecal)
{
	GList *l, *objects;
	int num_added = 0;
	int num_objects;
	gboolean retval = TRUE;
	
	/* both ecals are loaded, start the actual migration */
	if (!e_cal_get_object_list (old_ecal, "#t", &objects, NULL))
		return FALSE;

	num_objects = g_list_length (objects);
	for (l = objects; l; l = l->next) {
		icalcomponent *ical_comp = l->data;
		GError *error = NULL;

		if (!e_cal_create_object (new_ecal, ical_comp, NULL, &error)) {
			g_warning ("Migration of object failed: %s", error->message);
			retval = FALSE;
		}
		
		g_clear_error (&error);

		num_added ++;
		dialog_set_progress ((double)num_added / num_objects);
	}

	return retval;
}

static gboolean
migrate_ical_folder (char *old_path, ESourceGroup *dest_group, char *source_name, ECalSourceType type)
{
	ECal *old_ecal = NULL, *new_ecal = NULL;
	ESource *old_source;
	ESource *new_source;
	ESourceGroup *group;
	char *old_uri = g_strdup_printf ("file://%s", old_path);
	GError *error = NULL;
	gboolean retval = FALSE;
	
	group = e_source_group_new ("", old_uri);
	old_source = e_source_new ("", "");
	e_source_set_group (old_source, group);
	g_object_unref (group);

	new_source = e_source_new (source_name, source_name);
	e_source_set_group (new_source, dest_group);

	dialog_set_folder_name (source_name);

	old_ecal = e_cal_new (old_source, type);
	if (!e_cal_open (old_ecal, TRUE, &error)) {
		g_warning ("failed to load source ecal for migration: `%s'", error->message);
		goto finish;
	}

	new_ecal = e_cal_new (new_source, type);
	if (!e_cal_open (new_ecal, FALSE, &error)) {
		g_warning ("failed to load destination ecal for migration: `%s'", error->message);
		goto finish;
	}

	retval = migrate_ical (old_ecal, new_ecal);

 finish:
	g_clear_error (&error);
	g_object_unref (old_ecal);
	g_object_unref (new_ecal);
	g_free (old_uri);

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
			GSList *migration_dirs, *l;
			char *path, *local_cal_folder;

			setup_progress_dialog (FALSE);

			if (!create_calendar_sources (component, calendar_component_peek_source_list (component), &on_this_computer, NULL, NULL))
				return FALSE;

			path = g_build_filename (g_get_home_dir (), "evolution", "local", NULL);
			migration_dirs = e_folder_map_local_folders (path, "calendar");
			local_cal_folder = g_build_filename (path, "Calendar", NULL);
			g_free (path);

			for (l = migration_dirs; l; l = l->next) {
				char *source_name;
			
				if (!strcmp (l->data, local_cal_folder))
					source_name = g_strdup (_("Personal"));
				else
					source_name = get_source_name (on_this_computer, (char*)l->data + strlen (path) + 1);

				if (!migrate_ical_folder (l->data, on_this_computer, source_name, E_CAL_SOURCE_TYPE_EVENT))
					retval = FALSE;
				
				g_free (source_name);
			}
			
			g_free (local_cal_folder);
			
			e_source_list_sync (calendar_component_peek_source_list (component), NULL);

			dialog_close ();
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
			GSList *migration_dirs, *l;
			char *path, *local_task_folder;

			setup_progress_dialog (TRUE);
			
			if (!create_task_sources (component, tasks_component_peek_source_list (component), &on_this_computer))
				return FALSE;
			
			path = g_build_filename (g_get_home_dir (), "evolution", "local", NULL);
			migration_dirs = e_folder_map_local_folders (path, "tasks");
			local_task_folder = g_build_filename (path, "Tasks", NULL);
			g_free (path);
			
			for (l = migration_dirs; l; l = l->next) {
				char *source_name;
			
				if (!strcmp (l->data, local_task_folder))
					source_name = g_strdup (_("Personal"));
				else
					source_name = get_source_name (on_this_computer, (char*)l->data + strlen (path) + 1);

				if (!migrate_ical_folder (l->data, on_this_computer, source_name, E_CAL_SOURCE_TYPE_TODO))
					retval = FALSE;
				
				g_free (source_name);
			}
			
			g_free (local_task_folder);
			
			e_source_list_sync (tasks_component_peek_source_list (component), NULL);

			dialog_close ();
		}		
	}
	
        return retval;
}
