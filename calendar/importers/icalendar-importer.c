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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkmain.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkradiobutton.h>
#include <gtk/gtknotebook.h>
#include <libgnome/gnome-util.h>
#include <libgnome/gnome-i18n.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-exception.h>
#include <libecal/e-cal.h>
#include <libedataserverui/e-source-selector.h>
#include <importer/evolution-importer.h>
#include <importer/evolution-intelligent-importer.h>
#include <importer/GNOME_Evolution_Importer.h>
#include <libical/icalvcal.h>
#include <e-util/e-dialog-widgets.h>
#include "evolution-calendar-importer.h"
#include "common/authentication.h"

/* We timeout after 2 minutes, when opening the folders. */
#define IMPORTER_TIMEOUT_SECONDS 120


typedef struct {
	EvolutionImporter *importer;

	GtkWidget *nb;
	
	ESource *primary;
	ESourceSelector *selectors[E_CAL_SOURCE_TYPE_LAST];

	ECal *client;
	ECalSourceType source_type;

	icalcomponent *icalcomp;
} ICalImporter;

typedef struct {
	gboolean do_calendar;
	gboolean do_tasks;
} ICalIntelligentImporter;

static const int import_type_map[] = {
	E_CAL_SOURCE_TYPE_EVENT,
	E_CAL_SOURCE_TYPE_TODO,
	-1
};

static const char *import_type_strings[] = {
	N_("Appointments and Meetings"),
	N_("Tasks"),
	NULL
};


/*
 * Functions shared by iCalendar & vCalendar importer.
 */

static void
importer_destroy_cb (gpointer user_data)
{
	ICalImporter *ici = (ICalImporter *) user_data;
	
	g_return_if_fail (ici != NULL);
	
	if (ici->client)
		g_object_unref (ici->client);
	
	if (ici->icalcomp != NULL) {
		icalcomponent_free (ici->icalcomp);
		ici->icalcomp = NULL;
	}

	g_free (ici);
}

/* This removes all components except VEVENTs and VTIMEZONEs from the toplevel */
static void
prepare_events (icalcomponent *icalcomp, GList **vtodos)
{
	icalcomponent *subcomp;
	icalcompiter iter;

	if (vtodos)
		*vtodos = NULL;
	
	iter = icalcomponent_begin_component (icalcomp, ICAL_ANY_COMPONENT);
	while ((subcomp = icalcompiter_deref (&iter)) != NULL) {
		icalcomponent_kind child_kind = icalcomponent_isa (subcomp);
		if (child_kind != ICAL_VEVENT_COMPONENT
		    && child_kind != ICAL_VTIMEZONE_COMPONENT) {

			icalcompiter_next (&iter);

			icalcomponent_remove_component (icalcomp, subcomp);
			if (child_kind == ICAL_VTODO_COMPONENT && vtodos)
				*vtodos = g_list_prepend (*vtodos, subcomp);
			else
                                icalcomponent_free (subcomp);
		}

		icalcompiter_next (&iter);
	}
}


/* This removes all components except VTODOs and VTIMEZONEs from the toplevel
   icalcomponent, and adds the given list of VTODO components. The list is
   freed afterwards. */
static void
prepare_tasks (icalcomponent *icalcomp, GList *vtodos)
{
	icalcomponent *subcomp;
	GList *elem;
	icalcompiter iter;

	iter = icalcomponent_begin_component (icalcomp, ICAL_ANY_COMPONENT);
	while ((subcomp = icalcompiter_deref (&iter)) != NULL) {
		icalcomponent_kind child_kind = icalcomponent_isa (subcomp);
		if (child_kind != ICAL_VTODO_COMPONENT
		    && child_kind != ICAL_VTIMEZONE_COMPONENT) {
			icalcompiter_next (&iter);
			icalcomponent_remove_component (icalcomp, subcomp);
			icalcomponent_free (subcomp);
		}

		icalcompiter_next (&iter);
	}

	for (elem = vtodos; elem; elem = elem->next) {
		icalcomponent_add_component (icalcomp, elem->data);
	}
	g_list_free (vtodos);
}

static gboolean
update_single_object (ECal *client, icalcomponent *icalcomp)
{
	char *uid;
	icalcomponent *tmp_icalcomp;

	uid = (char *) icalcomponent_get_uid (icalcomp);

	/* FIXME Shouldn't we check for RIDs here? */
	/* FIXME Should we always create a new UID? */
	if (uid && e_cal_get_object (client, uid, NULL, &tmp_icalcomp, NULL))
		return e_cal_modify_object (client, icalcomp, CALOBJ_MOD_ALL, NULL);

	return e_cal_create_object (client, icalcomp, &uid, NULL);	
}

static gboolean
update_objects (ECal *client, icalcomponent *icalcomp)
{
	icalcomponent *subcomp;
	icalcomponent_kind kind;

	kind = icalcomponent_isa (icalcomp);
	if (kind == ICAL_VTODO_COMPONENT || kind == ICAL_VEVENT_COMPONENT)
		return update_single_object (client, icalcomp);
	else if (kind != ICAL_VCALENDAR_COMPONENT)
		return FALSE;

	subcomp = icalcomponent_get_first_component (icalcomp, ICAL_ANY_COMPONENT);
	while (subcomp) {
		gboolean success;
		
		kind = icalcomponent_isa (subcomp);
		if (kind == ICAL_VTIMEZONE_COMPONENT) {
			icaltimezone *zone;

			zone = icaltimezone_new ();
			icaltimezone_set_component (zone, subcomp);

			success = e_cal_add_timezone (client, zone, NULL);
			icaltimezone_free (zone, 1);
			if (!success)
				return success;
		} else if (kind == ICAL_VTODO_COMPONENT ||
			   kind == ICAL_VEVENT_COMPONENT) {
			success = update_single_object (client, subcomp);
			if (!success)
				return success;
		}

		subcomp = icalcomponent_get_next_component (icalcomp, ICAL_ANY_COMPONENT);
	}

	return TRUE;
}

static void
button_toggled_cb (GtkWidget *widget, gpointer data)
{
	ICalImporter *ici = data;

	ici->source_type = e_dialog_radio_get (widget, import_type_map);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (ici->nb), ici->source_type);

	/* If we switched pages we have a new primary source */
	if (ici->primary)
		g_object_unref (ici->primary);
	ici->primary = g_object_ref (e_source_selector_peek_primary_selection (ici->selectors[ici->source_type]));
}

static void
primary_selection_changed_cb (ESourceSelector *selector, gpointer data)
{
	ICalImporter *ici = data;

	if (ici->primary)
		g_object_unref (ici->primary);
	ici->primary = g_object_ref (e_source_selector_peek_primary_selection (selector));
}

static void
create_control_fn (EvolutionImporter *importer, Bonobo_Control *control, void *closure)
{
	ICalImporter *ici = (ICalImporter *) closure;
	GtkWidget *vbox, *hbox, *rb = NULL;
	GSList *group = NULL;
	ESourceList *source_list;	
	int i;
	
	vbox = gtk_vbox_new (FALSE, FALSE);
	
	hbox = gtk_hbox_new (FALSE, FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 6);

	/* Type of icalendar items */
	for (i = 0; import_type_map[i] != -1; i++) {
		rb = gtk_radio_button_new_with_label (group, import_type_strings[i]);
		gtk_box_pack_start (GTK_BOX (hbox), rb, FALSE, FALSE, 6);
		g_signal_connect (G_OBJECT (rb), "toggled", G_CALLBACK (button_toggled_cb), ici);
		if (!group)
			group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (rb));
	}
	e_dialog_radio_set (rb, import_type_map[0], import_type_map);
	
	/* The source selector notebook */
	ici->nb = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (ici->nb), FALSE);
	gtk_container_add (GTK_CONTAINER (vbox), ici->nb);
	
	/* The source selectors */
	for (i = 0; import_type_map[i] != -1; i++) {
		GtkWidget *selector;
		ESource *primary;

		/* FIXME Better error handling */
		if (!e_cal_get_sources (&source_list, import_type_map[i], NULL))
			return;		

		selector = e_source_selector_new (source_list);
		e_source_selector_show_selection (E_SOURCE_SELECTOR (selector), FALSE);
		gtk_notebook_append_page (GTK_NOTEBOOK (ici->nb), selector, NULL);

		/* FIXME What if no sources? */
		primary = e_source_list_peek_source_any (source_list);
		e_source_selector_set_primary_selection (E_SOURCE_SELECTOR (selector), primary);
		if (!ici->primary)
			ici->primary = g_object_ref (primary);
		g_object_unref (source_list);
		
		g_signal_connect (G_OBJECT (selector), "primary_selection_changed",
				  G_CALLBACK (primary_selection_changed_cb), ici);
		
		ici->selectors[import_type_map[i]] = E_SOURCE_SELECTOR (selector);
	}

	gtk_widget_show_all (vbox);
	
	*control = BONOBO_OBJREF (bonobo_control_new (vbox));
}

static void
process_item_fn (EvolutionImporter *importer,
		 CORBA_Object listener,
		 void *closure,
		 CORBA_Environment *ev)
{
	ECalLoadState state;
	ICalImporter *ici = (ICalImporter *) closure;
	GNOME_Evolution_ImporterListener_ImporterResult result;
	
	result = GNOME_Evolution_ImporterListener_OK;

	g_return_if_fail (ici != NULL);
	g_return_if_fail (ici->icalcomp != NULL);

	state = e_cal_get_load_state (ici->client);
	if (state == E_CAL_LOAD_LOADING) {
		GNOME_Evolution_ImporterListener_notifyResult (
			listener,
			GNOME_Evolution_ImporterListener_BUSY,
			TRUE, ev);
		return;
	} else if (state != E_CAL_LOAD_LOADED) {
		GNOME_Evolution_ImporterListener_notifyResult (
			listener,
			GNOME_Evolution_ImporterListener_UNSUPPORTED_OPERATION,
			FALSE, ev);
		return;
	}

	switch (ici->source_type) {
	case E_CAL_SOURCE_TYPE_EVENT:
		prepare_events (ici->icalcomp, NULL);
		if (!update_objects (ici->client, ici->icalcomp))
			result = GNOME_Evolution_ImporterListener_BAD_DATA;
		break;
	case E_CAL_SOURCE_TYPE_TODO:
		prepare_tasks (ici->icalcomp, NULL);
		if (!update_objects (ici->client, ici->icalcomp))
			result = GNOME_Evolution_ImporterListener_BAD_DATA;
		break;
	default:
		g_assert_not_reached ();
	}

	GNOME_Evolution_ImporterListener_notifyResult (listener, result, FALSE, ev);
}


/*
 * iCalendar importer functions.
 */

static gboolean
support_format_fn (EvolutionImporter *importer,
		   const char *filename,
		   void *closure)
{
	char *contents ;
	icalcomponent *icalcomp;
	gboolean ret = FALSE;

	if (g_file_get_contents (filename, &contents, NULL, NULL)) {
		/* parse the file */
		icalcomp = icalparser_parse_string (contents);
		g_free (contents);

		if (icalcomp) {
			if (icalcomponent_is_valid (icalcomp))
				ret = TRUE;
			else 
				ret = FALSE;
			icalcomponent_free (icalcomp);
		}
	}

	return ret;
}

static gboolean
load_file_fn (EvolutionImporter *importer,
	      const char *filename,
	      void *closure)
{
	char *contents;
	gboolean ret = FALSE;
	ICalImporter *ici = (ICalImporter *) closure;

	g_return_val_if_fail (ici != NULL, FALSE);

	if (g_file_get_contents (filename, &contents, NULL, NULL)) {
		icalcomponent *icalcomp;

		/* parse the file */
		icalcomp = icalparser_parse_string (contents);
		g_free (contents);
		
		if (icalcomp) {
			/* create the neccessary ECal */
			if (ici->client)
				g_object_unref (ici->client);
			ici->client = auth_new_cal_from_source (ici->primary, ici->source_type);

			if (ici->client) {
				if (e_cal_open (ici->client, TRUE, NULL)) {
					ici->icalcomp = icalcomp;
					ret = TRUE;
				}
			}
		}
	}

	return ret;
}

BonoboObject *
ical_importer_new (void)
{
	ICalImporter *ici;
	
	ici = g_new0 (ICalImporter, 1);

	ici->client = NULL;
	ici->icalcomp = NULL;
	ici->importer = evolution_importer_new (create_control_fn,
						support_format_fn,
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

	if (g_file_get_contents (filename, &contents, NULL, NULL)) {
		VObject *vcal;

		/* parse the file */
		vcal = Parse_MIME (contents, strlen (contents));
		g_free (contents);

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

	if (g_file_get_contents (filename, &contents, NULL, NULL)) {
		VObject *vcal;

		/* parse the file */
		vcal = Parse_MIME (contents, strlen (contents));
		g_free (contents);

		if (vcal) {
			icalcomp = icalvcal_convert_with_defaults (vcal,
								   &defaults);
			cleanVObject (vcal);
		}
	}

	return icalcomp;
}

static gboolean
vcal_load_file_fn (EvolutionImporter *importer,
		   const char *filename,
		   void *closure)
{
	gboolean ret = FALSE;
	ICalImporter *ici = (ICalImporter *) closure;
	icalcomponent *icalcomp;

	g_return_val_if_fail (ici != NULL, FALSE);

	icalcomp = load_vcalendar_file (filename);
	if (icalcomp) {
		/* create the neccessary ECal */
		if (ici->client)
			g_object_unref (ici->client);
		ici->client = auth_new_cal_from_source (ici->primary, ici->source_type);
		
		if (ici->client) {
			if (e_cal_open (ici->client, TRUE, NULL)) {
				ici->icalcomp = icalcomp;
				ret = TRUE;
			}
		}
	}

	return ret;
}

BonoboObject *
vcal_importer_new (void)
{
	ICalImporter *ici;

	ici = g_new0 (ICalImporter, 1);
	ici->icalcomp = NULL;
	ici->importer = evolution_importer_new (create_control_fn,
						vcal_support_format_fn,
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
	ECal *calendar_client = NULL, *tasks_client = NULL;
	int t;

	/* If neither is selected, just return. */
	if (!ici->do_calendar && !ici->do_tasks) {
		return;
	}

	/* Try to open the default calendar & tasks folders. */
	if (ici->do_calendar) {
		calendar_client = auth_new_cal_from_default (E_CAL_SOURCE_TYPE_EVENT);
			goto out;
	}

	if (ici->do_tasks) {
		tasks_client = auth_new_cal_from_default (E_CAL_SOURCE_TYPE_TODO);
		if (!tasks_client)
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
	prepare_events (icalcomp, &vtodos);

	/* Wait for client to finish opening the calendar & tasks folders. */
	for (t = 0; t < IMPORTER_TIMEOUT_SECONDS; t++) {
		ECalLoadState calendar_state, tasks_state;

		calendar_state = tasks_state = E_CAL_LOAD_LOADED;

		/* We need this so the ECal gets notified that the
		   folder is opened, via Corba. */
		while (gtk_events_pending ())
			gtk_main_iteration ();

		if (ici->do_calendar)
			calendar_state = e_cal_get_load_state (calendar_client);

		if (ici->do_tasks)
			tasks_state = e_cal_get_load_state (tasks_client);

		if (calendar_state == E_CAL_LOAD_LOADED
		    && tasks_state == E_CAL_LOAD_LOADED)
			break;

		sleep (1);
	}

	/* If we timed out, just return. */
	if (t == IMPORTER_TIMEOUT_SECONDS)
		goto out;

	/* Import the calendar events. */
	/* FIXME: What do intelligent importers do about errors? */
	if (ici->do_calendar)
		update_objects (calendar_client, icalcomp);


	/*
	 * Import the tasks into the default tasks folder.
	 */
	prepare_tasks (icalcomp, vtodos);
	if (ici->do_tasks)
		update_objects (tasks_client, icalcomp);

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
