/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar importer component
 *
 * Authors: Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 2001  Ximian, Inc.
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
 */

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkmain.h>
#include <libgnome/gnome-util.h>
#include <libgnome/gnome-i18n.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-exception.h>
#include <cal-client.h>
#include <importer/evolution-importer.h>
#include <importer/evolution-intelligent-importer.h>
#include <importer/GNOME_Evolution_Importer.h>
#include <shell/e-shell.h>
#include <shell/evolution-shell-client.h>
#include "icalvcal.h"
#include "evolution-calendar-importer.h"

/* We timeout after 2 minutes, when opening the folders. */
#define IMPORTER_TIMEOUT_SECONDS 120


typedef struct {
	CalClient *client;
	CalClient *tasks_client;
	EvolutionImporter *importer;
	icalcomponent *icalcomp;
	gboolean folder_contains_events;
	gboolean folder_contains_tasks;
} ICalImporter;

typedef struct {
	gboolean do_calendar;
	gboolean do_tasks;
} ICalIntelligentImporter;

/*
 * Functions shared by iCalendar & vCalendar importer.
 */

static void
importer_destroy_cb (gpointer user_data)
{
	ICalImporter *ici = (ICalImporter *) user_data;

	g_return_if_fail (ici != NULL);

	g_object_unref (ici->client);
	g_object_unref (ici->tasks_client);

	if (ici->icalcomp != NULL) {
		icalcomponent_free (ici->icalcomp);
		ici->icalcomp = NULL;
	}

	g_free (ici);
}

/* This reads in an entire file and returns it. It returns NULL on error.
   The returned string should be freed. */
static char*
read_file (const char *filename)
{
	int fd, n;
	GString *str;
	char buffer[2049];
	gboolean error = FALSE;

	/* read file contents */
	fd = open (filename, O_RDONLY);
	if (fd == -1)
		return NULL;

	str = g_string_new ("");
	while (1) {
		memset (buffer, 0, sizeof(buffer));
		n = read (fd, buffer, sizeof (buffer) - 1);
		if (n > 0) {
			str = g_string_append (str, buffer);
		} else if (n == 0) {
			break;
		} else {
			error = TRUE;
			break;
		}
	}

	close (fd);

	if (error) {
		g_string_free (str, FALSE);
		return NULL;
	} else {
		gchar *retval = str->str;
		g_string_free (str, FALSE);
		return retval;
	}
}

/* This removes all components except VEVENTs and VTIMEZONEs from the toplevel
   icalcomponent, and returns a GList of the VTODO components. */
static GList*
prepare_events (icalcomponent *icalcomp)
{
	icalcomponent *subcomp, *next_subcomp;
	GList *vtodos = NULL;

	subcomp = icalcomponent_get_first_component (icalcomp,
						     ICAL_ANY_COMPONENT);
	while (subcomp) {
		icalcomponent_kind child_kind = icalcomponent_isa (subcomp);
		next_subcomp = icalcomponent_get_next_component (icalcomp, ICAL_ANY_COMPONENT);
		if (child_kind != ICAL_VEVENT_COMPONENT
		    && child_kind != ICAL_VTIMEZONE_COMPONENT) {

			icalcomponent_remove_component (icalcomp,
							subcomp);
			if (child_kind == ICAL_VTODO_COMPONENT)
				vtodos = g_list_prepend (vtodos, subcomp);
			else
				icalcomponent_free (subcomp);
		}
		subcomp = next_subcomp;
	}

	return vtodos;
}


/* This removes all components except VTODOs and VTIMEZONEs from the toplevel
   icalcomponent, and adds the given list of VTODO components. The list is
   freed afterwards. */
static void
prepare_tasks (icalcomponent *icalcomp, GList *vtodos)
{
	icalcomponent *subcomp, *next_subcomp;
	GList *elem;

	subcomp = icalcomponent_get_first_component (icalcomp,
						     ICAL_ANY_COMPONENT);
	while (subcomp) {
		icalcomponent_kind child_kind = icalcomponent_isa (subcomp);
		next_subcomp = icalcomponent_get_next_component (icalcomp, ICAL_ANY_COMPONENT);
		if (child_kind != ICAL_VTODO_COMPONENT
		    && child_kind != ICAL_VTIMEZONE_COMPONENT) {
			icalcomponent_remove_component (icalcomp, subcomp);
			icalcomponent_free (subcomp);
		}
		subcomp = next_subcomp;
	}

	for (elem = vtodos; elem; elem = elem->next) {
		icalcomponent_add_component (icalcomp, elem->data);
	}
	g_list_free (vtodos);
}


static void
process_item_fn (EvolutionImporter *importer,
		 CORBA_Object listener,
		 void *closure,
		 CORBA_Environment *ev)
{
	CalClientLoadState state, tasks_state;
	ICalImporter *ici = (ICalImporter *) closure;
	GNOME_Evolution_ImporterListener_ImporterResult result;

	result = GNOME_Evolution_ImporterListener_OK;

	g_return_if_fail (ici != NULL);
	g_return_if_fail (IS_CAL_CLIENT (ici->client));
	g_return_if_fail (ici->icalcomp != NULL);

	state = cal_client_get_load_state (ici->client);
	tasks_state = cal_client_get_load_state (ici->tasks_client);
	if (state == CAL_CLIENT_LOAD_LOADING
	    || tasks_state == CAL_CLIENT_LOAD_LOADING) {
		GNOME_Evolution_ImporterListener_notifyResult (
			listener,
			GNOME_Evolution_ImporterListener_BUSY,
			TRUE, ev);
		return;
	} else if (state != CAL_CLIENT_LOAD_LOADED
		   || tasks_state != CAL_CLIENT_LOAD_LOADED) {
		GNOME_Evolution_ImporterListener_notifyResult (
			listener,
			GNOME_Evolution_ImporterListener_UNSUPPORTED_OPERATION,
			FALSE, ev);
		return;
	}

	/* If the folder contains events & tasks we can just import everything
	   into it. If it contains just events, we have to strip out the
	   VTODOs and import them into the default tasks folder. If the folder
	   contains just tasks, we strip out the VEVENTs, which do not get
	   imported at all. */
	if (ici->folder_contains_events && ici->folder_contains_tasks) {
		if (cal_client_update_objects (ici->client, ici->icalcomp) != CAL_CLIENT_RESULT_SUCCESS)
			result = GNOME_Evolution_ImporterListener_BAD_DATA;
	} else if (ici->folder_contains_events) {
		GList *vtodos = prepare_events (ici->icalcomp);
		if (cal_client_update_objects (ici->client, ici->icalcomp) != CAL_CLIENT_RESULT_SUCCESS)
			result = GNOME_Evolution_ImporterListener_BAD_DATA;

		prepare_tasks (ici->icalcomp, vtodos);
		if (cal_client_update_objects (ici->tasks_client,
					       ici->icalcomp) != CAL_CLIENT_RESULT_SUCCESS)
			result = GNOME_Evolution_ImporterListener_BAD_DATA;
	} else {
		prepare_tasks (ici->icalcomp, NULL);
		if (cal_client_update_objects (ici->client, ici->icalcomp) != CAL_CLIENT_RESULT_SUCCESS)
			result = GNOME_Evolution_ImporterListener_BAD_DATA;
	}

	GNOME_Evolution_ImporterListener_notifyResult (listener, result, FALSE,
						       ev);
}


/*
 * iCalendar importer functions.
 */

static gboolean
support_format_fn (EvolutionImporter *importer,
		   const char *filename,
		   void *closure)
{
	char *contents;
	icalcomponent *icalcomp;
	gboolean ret = FALSE;

	contents = read_file (filename);

	/* parse the file */
	if (contents) {
		icalcomp = icalparser_parse_string (contents);
		if (icalcomp) {
			icalcomponent_free (icalcomp);
			ret = TRUE;
		}
	}

	g_free (contents);

	return ret;
}

static gboolean
load_file_fn (EvolutionImporter *importer,
	      const char *filename,
	      const char *physical_uri,
	      const char *folder_type,
	      void *closure)
{
	char *contents, *f;
	gboolean ret = FALSE;
	ICalImporter *ici = (ICalImporter *) closure;

	g_return_val_if_fail (ici != NULL, FALSE);

	contents = read_file (filename);

	if (!strcmp (folder_type, "calendar")) {
		ici->folder_contains_events = TRUE;
		ici->folder_contains_tasks = FALSE;

		f = g_strdup ("calendar.ics");
	} else {
		ici->folder_contains_events = FALSE;
		ici->folder_contains_tasks = TRUE;

		f = g_strdup ("tasks.ics");
	}

	/* parse the file */
	if (contents) {
		icalcomponent *icalcomp;

		icalcomp = icalparser_parse_string (contents);
		if (icalcomp) {
			char *real_uri;

			if (!g_strncasecmp (physical_uri, "file", 4) &&
			    g_strcasecmp (physical_uri + (strlen (physical_uri) - strlen (f)), f)) {
				real_uri = g_concat_dir_and_file (physical_uri, f);
			} else
				real_uri = g_strdup (physical_uri);

			if (cal_client_open_calendar (ici->client, real_uri, TRUE)
			    && cal_client_open_default_tasks (ici->tasks_client, FALSE)) {
				ici->icalcomp = icalcomp;
				ret = TRUE;
			}

			g_free (real_uri);
		}
	}

	g_free (contents);
	g_free (f);

	return ret;
}

BonoboObject *
ical_importer_new (void)
{
	ICalImporter *ici;

	ici = g_new0 (ICalImporter, 1);
	ici->client = cal_client_new ();
	ici->tasks_client = cal_client_new ();
	ici->icalcomp = NULL;
	ici->importer = evolution_importer_new (support_format_fn,
						load_file_fn,
						process_item_fn,
						NULL,
						ici);

	g_object_weak_ref (G_OBJECT (ici->importer), (GWeakNotify) importer_destroy_cb, ici);

	return BONOBO_OBJECT (ici->importer);
}



/*
 * vCalendar importer functions.
 */

static gboolean
vcal_support_format_fn (EvolutionImporter *importer,
			const char *filename,
			void *closure)
{
	char *contents;
	gboolean ret = FALSE;

	contents = read_file (filename);

	/* parse the file */
	if (contents) {
		VObject *vcal;

		vcal = Parse_MIME (contents, strlen (contents));

		if (vcal) {
			icalcomponent *icalcomp;

			icalcomp = icalvcal_convert (vcal);
		
			if (icalcomp) {
				icalcomponent_free (icalcomp);
				ret = TRUE;
			}

			cleanVObject (vcal);
		}
	}

	g_free (contents);

	return ret;
}

/* This tries to load in a vCalendar file and convert it to an icalcomponent.
   It returns NULL on failure. */
static icalcomponent*
load_vcalendar_file (const char *filename)
{
	icalvcal_defaults defaults = { 0 };
	icalcomponent *icalcomp = NULL;
	char *contents;

	defaults.alarm_audio_url = "file://" EVOLUTION_SOUNDDIR "/default_alarm.wav";
	defaults.alarm_audio_fmttype = "audio/x-wav";
	defaults.alarm_description = (char*) _("Reminder!!");

	contents = read_file (filename);

	/* parse the file */
	if (contents) {
		VObject *vcal;

		vcal = Parse_MIME (contents, strlen (contents));

		if (vcal) {
			icalcomp = icalvcal_convert_with_defaults (vcal,
								   &defaults);
			cleanVObject (vcal);
		}
	}

	g_free (contents);

	return icalcomp;
}

static gboolean
vcal_load_file_fn (EvolutionImporter *importer,
		   const char *filename,
		   const char *physical_uri,
		   const char *folder_type,
		   void *closure)
{
	gboolean ret = FALSE;
	char *f;
	ICalImporter *ici = (ICalImporter *) closure;
	icalcomponent *icalcomp;

	g_return_val_if_fail (ici != NULL, FALSE);

	if (!strcmp (folder_type, "calendar")) {
		ici->folder_contains_events = TRUE;
		ici->folder_contains_tasks = FALSE;

		f = g_strdup ("calendar.ics");
	} else {
		ici->folder_contains_events = FALSE;
		ici->folder_contains_tasks = TRUE;

		f = g_strdup ("tasks.ics");
	}

	icalcomp = load_vcalendar_file (filename);
	if (icalcomp) {
		char *real_uri;

		if (!g_strncasecmp (physical_uri, "file", 4) &&
		    g_strcasecmp (physical_uri + (strlen (physical_uri) - strlen (f)), f)) {
			real_uri = g_concat_dir_and_file (physical_uri, f);
		} else
			real_uri = g_strdup (physical_uri);

		if (cal_client_open_calendar (ici->client, real_uri, TRUE)
		    && cal_client_open_default_tasks (ici->tasks_client, FALSE)) {
			ici->icalcomp = icalcomp;
			ret = TRUE;
		}

		g_free (real_uri);
	}

	g_free (f);

	return ret;
}

BonoboObject *
vcal_importer_new (void)
{
	ICalImporter *ici;

	ici = g_new0 (ICalImporter, 1);
	ici->client = cal_client_new ();
	ici->tasks_client = cal_client_new ();
	ici->icalcomp = NULL;
	ici->importer = evolution_importer_new (vcal_support_format_fn,
						vcal_load_file_fn,
						process_item_fn,
						NULL,
						ici);

	g_object_weak_ref (G_OBJECT (ici->importer), (GWeakNotify) importer_destroy_cb, ici);

	return BONOBO_OBJECT (ici->importer);
}






static void
gnome_calendar_importer_destroy_cb (gpointer user_data)
{
	ICalIntelligentImporter *ici = (ICalIntelligentImporter *) user_data;

	g_return_if_fail (ici != NULL);

	g_free (ici);
}



static gboolean
gnome_calendar_can_import_fn (EvolutionIntelligentImporter *ii,
			      void *closure)
{
	char *filename;
	gboolean gnome_calendar_exists;

	filename = gnome_util_home_file ("user-cal.vcf");
	gnome_calendar_exists = g_file_exists (filename);
	g_free (filename);

	return gnome_calendar_exists;
}


static void
gnome_calendar_import_data_fn (EvolutionIntelligentImporter *ii,
			       void *closure)
{
	ICalIntelligentImporter *ici = closure;
	icalcomponent *icalcomp = NULL;
	char *filename;
	GList *vtodos;
	CalClient *calendar_client = NULL, *tasks_client = NULL;
	int t;

	/* If neither is selected, just return. */
	if (!ici->do_calendar && !ici->do_tasks) {
		return;
	}

	/* Try to open the default calendar & tasks folders. */
	if (ici->do_calendar) {
		calendar_client = cal_client_new ();
		if (!cal_client_open_default_calendar (calendar_client, FALSE))
			goto out;
	}

	if (ici->do_tasks) {
		tasks_client = cal_client_new ();
		if (!cal_client_open_default_tasks (tasks_client, FALSE))
			goto out;
	}

	/* Load the Gnome Calendar file and convert to iCalendar. */
	filename = gnome_util_home_file ("user-cal.vcf");
	icalcomp = load_vcalendar_file (filename);
	g_free (filename);

	/* If we couldn't load the file, just return. FIXME: Error message? */
	if (!icalcomp)
		goto out;

	/*
	 * Import the calendar events into the default calendar folder.
	 */
	vtodos = prepare_events (icalcomp);

	/* Wait for client to finish opening the calendar & tasks folders. */
	for (t = 0; t < IMPORTER_TIMEOUT_SECONDS; t++) {
		CalClientLoadState calendar_state, tasks_state;

		calendar_state = tasks_state = CAL_CLIENT_LOAD_LOADED;

		/* We need this so the CalClient gets notified that the
		   folder is opened, via Corba. */
		while (gtk_events_pending ())
			gtk_main_iteration ();

		if (ici->do_calendar)
			calendar_state = cal_client_get_load_state (calendar_client);

		if (ici->do_tasks)
			tasks_state = cal_client_get_load_state (tasks_client);

		if (calendar_state == CAL_CLIENT_LOAD_LOADED
		    && tasks_state == CAL_CLIENT_LOAD_LOADED)
			break;

		sleep (1);
	}

	/* If we timed out, just return. */
	if (t == IMPORTER_TIMEOUT_SECONDS)
		goto out;

	/* Import the calendar events. */
	/* FIXME: What do intelligent importers do about errors? */
	if (ici->do_calendar)
		cal_client_update_objects (calendar_client, icalcomp);


	/*
	 * Import the tasks into the default tasks folder.
	 */
	prepare_tasks (icalcomp, vtodos);
	if (ici->do_tasks)
		cal_client_update_objects (tasks_client, icalcomp);

 out:
	if (icalcomp)
		icalcomponent_free (icalcomp);
	if (calendar_client)
		g_object_unref (calendar_client);
	if (tasks_client)
		g_object_unref (tasks_client);
}


/* Fun with aggregation */
static void
checkbox_toggle_cb (GtkToggleButton *tb,
		    gboolean *do_item)
{
	*do_item = gtk_toggle_button_get_active (tb);
}

static BonoboControl *
create_checkboxes_control (ICalIntelligentImporter *ici)
{
	GtkWidget *hbox, *calendar_checkbox, *tasks_checkbox;
	BonoboControl *control;

	hbox = gtk_hbox_new (FALSE, 2);

	calendar_checkbox = gtk_check_button_new_with_label (_("Calendar Events"));
	g_signal_connect (G_OBJECT (calendar_checkbox), "toggled",
			  G_CALLBACK (checkbox_toggle_cb),
			  &ici->do_calendar);
	gtk_box_pack_start (GTK_BOX (hbox), calendar_checkbox,
			    FALSE, FALSE, 0);

	tasks_checkbox = gtk_check_button_new_with_label (_("Tasks"));
	g_signal_connect (G_OBJECT (tasks_checkbox), "toggled",
			  G_CALLBACK (checkbox_toggle_cb),
			  &ici->do_tasks);
	gtk_box_pack_start (GTK_BOX (hbox), tasks_checkbox,
			    FALSE, FALSE, 0);

	gtk_widget_show_all (hbox);
	control = bonobo_control_new (hbox);
	return control;
}

BonoboObject *
gnome_calendar_importer_new (void)
{
	EvolutionIntelligentImporter *importer;
	ICalIntelligentImporter *ici;
	BonoboControl *control;
	char *message = N_("Evolution has found Gnome Calendar files.\n"
			   "Would you like to import them into Evolution?");

	ici = g_new0 (ICalIntelligentImporter, 1);

	importer = evolution_intelligent_importer_new (gnome_calendar_can_import_fn,
						       gnome_calendar_import_data_fn,
						       _("Gnome Calendar"),
						       _(message),
						       ici);


	g_object_weak_ref (G_OBJECT (importer), (GWeakNotify) gnome_calendar_importer_destroy_cb, ici);

	control = create_checkboxes_control (ici);
	bonobo_object_add_interface (BONOBO_OBJECT (importer),
				     BONOBO_OBJECT (control));

	return BONOBO_OBJECT (importer);
}
