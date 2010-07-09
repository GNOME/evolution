/*
 * Evolution calendar importer component
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include <gtk/gtk.h>

#include <libecal/e-cal.h>
#include <libecal/e-cal-time-util.h>
#include <libedataserverui/e-source-selector.h>
#include <libical/icalvcal.h>
#include "evolution-calendar-importer.h"
#include "shell/e-shell.h"
#include "common/authentication.h"
#include "gui/calendar-config-keys.h"

#include "e-util/e-import.h"
#include "e-util/e-util-private.h"
#include "e-util/e-datetime-format.h"
#include "misc/e-web-view-preview.h"

/* We timeout after 2 minutes, when opening the folders. */
#define IMPORTER_TIMEOUT_SECONDS 120

typedef struct {
	EImport *import;
	EImportTarget *target;

	guint idle_id;

	ECal *client;
	ECalSourceType source_type;

	icalcomponent *icalcomp;

	guint cancelled:1;
} ICalImporter;

typedef struct {
	guint cancelled:1;
} ICalIntelligentImporter;

static const gint import_type_map[] = {
	E_CAL_SOURCE_TYPE_EVENT,
	E_CAL_SOURCE_TYPE_TODO,
	-1
};

static const gchar *import_type_strings[] = {
	N_("Appointments and Meetings"),
	N_("Tasks"),
	NULL
};

/*
 * Functions shared by iCalendar & vCalendar importer.
 */

static GtkWidget *ical_get_preview (icalcomponent *icalcomp);

static gboolean
is_icalcomp_usable (icalcomponent *icalcomp)
{
	return icalcomp && icalcomponent_is_valid (icalcomp) && (
		icalcomponent_get_first_component (icalcomp, ICAL_VEVENT_COMPONENT) != NULL ||
		icalcomponent_get_first_component (icalcomp, ICAL_VTODO_COMPONENT) != NULL);
}

static void
ivcal_import_done(ICalImporter *ici)
{
	g_object_unref (ici->client);
	icalcomponent_free (ici->icalcomp);

	e_import_complete(ici->import, ici->target);
	g_object_unref(ici->import);
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
		} else {
			icalcompiter_next (&iter);
		}
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
		} else {
			icalcompiter_next (&iter);
		}
	}

	for (elem = vtodos; elem; elem = elem->next) {
		icalcomponent_add_component (icalcomp, elem->data);
	}
	g_list_free (vtodos);
}

static gboolean
update_objects (ECal *client, icalcomponent *icalcomp)
{
	icalcomponent_kind kind;
	icalcomponent *vcal;
	gboolean success = TRUE;

	kind = icalcomponent_isa (icalcomp);
	if (kind == ICAL_VTODO_COMPONENT || kind == ICAL_VEVENT_COMPONENT) {
		vcal = e_cal_util_new_top_level ();
		if (icalcomponent_get_method (icalcomp) == ICAL_METHOD_CANCEL)
			icalcomponent_set_method (vcal, ICAL_METHOD_CANCEL);
		else
			icalcomponent_set_method (vcal, ICAL_METHOD_PUBLISH);
		icalcomponent_add_component (vcal, icalcomponent_new_clone (icalcomp));
	} else if (kind == ICAL_VCALENDAR_COMPONENT) {
		vcal = icalcomponent_new_clone (icalcomp);
		if (!icalcomponent_get_first_property (vcal, ICAL_METHOD_PROPERTY))
			icalcomponent_set_method (vcal, ICAL_METHOD_PUBLISH);
	} else
		return FALSE;

	if (!e_cal_receive_objects (client, vcal, NULL))
		success = FALSE;

	icalcomponent_free (vcal);

	return success;
}

struct _selector_data {
	EImportTarget *target;
	GtkWidget *selector;
	GtkWidget *notebook;
	gint page;
};

static void
button_toggled_cb (GtkWidget *widget, struct _selector_data *sd)
{
	g_datalist_set_data_full(&sd->target->data, "primary-source",
				 g_object_ref(e_source_selector_peek_primary_selection((ESourceSelector *)sd->selector)),
				 g_object_unref);
	g_datalist_set_data(&sd->target->data, "primary-type", GINT_TO_POINTER(import_type_map[sd->page]));
	gtk_notebook_set_current_page((GtkNotebook *)sd->notebook, sd->page);
}

static void
primary_selection_changed_cb (ESourceSelector *selector, EImportTarget *target)
{
	g_datalist_set_data_full(&target->data, "primary-source",
				 g_object_ref(e_source_selector_peek_primary_selection(selector)),
				 g_object_unref);
}

static GtkWidget *
ivcal_getwidget(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	GtkWidget *vbox, *hbox, *first = NULL;
	GSList *group = NULL;
	gint i;
	GtkWidget *nb;

	vbox = gtk_vbox_new (FALSE, FALSE);

	hbox = gtk_hbox_new (FALSE, FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 6);

	nb = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (nb), FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), nb, TRUE, TRUE, 6);

	/* Type of icalendar items */
	for (i = 0; import_type_map[i] != -1; i++) {
		GtkWidget *selector, *rb;
		ESourceList *source_list;
		ESource *primary;
		GtkWidget *scrolled;
		struct _selector_data *sd;

		/* FIXME Better error handling */
		if (!e_cal_get_sources (&source_list, import_type_map[i], NULL))
			continue;

		selector = e_source_selector_new (source_list);
		e_source_selector_show_selection (E_SOURCE_SELECTOR (selector), FALSE);
		scrolled = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy((GtkScrolledWindow *)scrolled, GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
		gtk_container_add((GtkContainer *)scrolled, selector);
		gtk_notebook_append_page (GTK_NOTEBOOK (nb), scrolled, NULL);

		/* FIXME What if no sources? */
		primary = e_source_list_peek_source_any (source_list);
		e_source_selector_set_primary_selection (E_SOURCE_SELECTOR (selector), primary);

		g_signal_connect (selector, "primary_selection_changed", G_CALLBACK (primary_selection_changed_cb), target);

		rb = gtk_radio_button_new_with_label (group, _(import_type_strings[i]));
		gtk_box_pack_start (GTK_BOX (hbox), rb, FALSE, FALSE, 6);

		sd = g_malloc0(sizeof(*sd));
		sd->target = target;
		sd->selector = selector;
		sd->notebook = nb;
		sd->page = i;
		g_object_set_data_full((GObject *)rb, "selector-data", sd, g_free);
		g_signal_connect(G_OBJECT (rb), "toggled", G_CALLBACK (button_toggled_cb), sd);

		if (!group)
			group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (rb));
		if (first == NULL) {
			g_datalist_set_data_full(&target->data, "primary-source", g_object_ref(primary), g_object_unref);
			g_datalist_set_data(&target->data, "primary-type", GINT_TO_POINTER(import_type_map[i]));
			first = rb;
		}
		g_object_unref (source_list);
	}
	if (first)
		gtk_toggle_button_set_active((GtkToggleButton *)first, TRUE);

	gtk_widget_show_all (vbox);

	return vbox;
}

static gboolean
ivcal_import_items(gpointer d)
{
	ICalImporter *ici = d;

	switch (ici->source_type) {
	case E_CAL_SOURCE_TYPE_EVENT:
		prepare_events (ici->icalcomp, NULL);
		if (!update_objects (ici->client, ici->icalcomp)) {
			/* FIXME: e_alert ... */;
		}
		break;
	case E_CAL_SOURCE_TYPE_TODO:
		prepare_tasks (ici->icalcomp, NULL);
		if (!update_objects (ici->client, ici->icalcomp)) {
			/* FIXME: e_alert ... */;
		}
		break;
	default:
		g_return_val_if_reached (FALSE);
	}

	ivcal_import_done(ici);
	ici->idle_id = 0;

	return FALSE;
}

static void
ivcal_opened(ECal *ecal, const GError *error, ICalImporter *ici)
{
	if (!ici->cancelled && !error) {
		e_import_status(ici->import, ici->target, _("Importing..."), 0);
		ici->idle_id = g_idle_add(ivcal_import_items, ici);
	} else
		ivcal_import_done(ici);
}

static void
ivcal_import(EImport *ei, EImportTarget *target, icalcomponent *icalcomp)
{
	ECal *client;
	ECalSourceType type;

	type = GPOINTER_TO_INT(g_datalist_get_data(&target->data, "primary-type"));

	client = e_auth_new_cal_from_source (g_datalist_get_data(&target->data, "primary-source"), type);
	if (client) {
		ICalImporter *ici = g_malloc0(sizeof(*ici));

		ici->import = ei;
		g_datalist_set_data(&target->data, "ivcal-data", ici);
		g_object_ref(ei);
		ici->target = target;
		ici->icalcomp = icalcomp;
		ici->client = client;
		ici->source_type = type;
		e_import_status(ei, target, _("Opening calendar"), 0);
		g_signal_connect(client, "cal-opened-ex", G_CALLBACK(ivcal_opened), ici);
		e_cal_open_async(client, TRUE);
		return;
	} else {
		icalcomponent_free(icalcomp);
		e_import_complete(ei, target);
	}
}

static void
ivcal_cancel(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	ICalImporter *ici = g_datalist_get_data(&target->data, "ivcal-data");

	if (ici)
		ici->cancelled = 1;
}

/* ********************************************************************** */
/*
 * iCalendar importer functions.
 */

static gboolean
ical_supported(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	gchar *filename;
	gchar *contents;
	gboolean ret = FALSE;
	EImportTargetURI *s;

	if (target->type != E_IMPORT_TARGET_URI)
		return FALSE;

	s = (EImportTargetURI *)target;
	if (s->uri_src == NULL)
		return TRUE;

	if (strncmp(s->uri_src, "file:///", 8) != 0)
		return FALSE;

	filename = g_filename_from_uri (s->uri_src, NULL, NULL);
	if (!filename)
		return FALSE;

	if (g_file_get_contents (filename, &contents, NULL, NULL)) {
		icalcomponent *icalcomp = NULL;

		if (g_ascii_strncasecmp (contents, "BEGIN:", 6) == 0)
			icalcomp = e_cal_util_parse_ics_string (contents);
		g_free (contents);

		if (icalcomp) {
			if (is_icalcomp_usable (icalcomp))
				ret = TRUE;
			else
				ret = FALSE;
			icalcomponent_free (icalcomp);
		}
	}
	g_free (filename);

	return ret;
}

static void
ical_import(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	gchar *filename;
	gchar *contents;
	icalcomponent *icalcomp;
	EImportTargetURI *s = (EImportTargetURI *)target;

	filename = g_filename_from_uri (s->uri_src, NULL, NULL);
	if (!filename) {
		e_import_complete(ei, target);
		return;
	}

	if (!g_file_get_contents (filename, &contents, NULL, NULL)) {
		g_free (filename);
		e_import_complete(ei, target);
		return;
	}
	g_free (filename);

	icalcomp = e_cal_util_parse_ics_string (contents);
	g_free (contents);

	if (icalcomp)
		ivcal_import(ei, target, icalcomp);
	else
		e_import_complete(ei, target);
}

static GtkWidget *
ivcal_get_preview (EImport *ei, EImportTarget *target, EImportImporter *im)
{
	GtkWidget *preview;
	EImportTargetURI *s = (EImportTargetURI *)target;
	gchar *filename;
	icalcomponent *icalcomp;
	gchar *contents;

	filename = g_filename_from_uri (s->uri_src, NULL, NULL);
	if (filename == NULL) {
		g_message (G_STRLOC ": Couldn't get filename from URI '%s'", s->uri_src);
		return NULL;
	}

	if (!g_file_get_contents (filename, &contents, NULL, NULL)) {
		g_free (filename);
		return NULL;
	}
	g_free (filename);

	icalcomp = e_cal_util_parse_ics_string (contents);
	g_free (contents);

	if (!icalcomp)
		return NULL;

	preview = ical_get_preview (icalcomp);

	icalcomponent_free (icalcomp);

	return preview;
}

static EImportImporter ical_importer = {
	E_IMPORT_TARGET_URI,
	0,
	ical_supported,
	ivcal_getwidget,
	ical_import,
	ivcal_cancel,
	ivcal_get_preview,
};

EImportImporter *
ical_importer_peek(void)
{
	ical_importer.name = _("iCalendar files (.ics)");
	ical_importer.description = _("Evolution iCalendar importer");

	return &ical_importer;
}

/* ********************************************************************** */
/*
 * vCalendar importer functions.
 */

static gboolean
vcal_supported(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	gchar *filename;
	gchar *contents;
	gboolean ret = FALSE;
	EImportTargetURI *s;

	if (target->type != E_IMPORT_TARGET_URI)
		return FALSE;

	s = (EImportTargetURI *)target;
	if (s->uri_src == NULL)
		return TRUE;

	if (strncmp(s->uri_src, "file:///", 8) != 0)
		return FALSE;

	filename = g_filename_from_uri (s->uri_src, NULL, NULL);
	if (!filename)
		return FALSE;

	/* Z: Wow, this is *efficient* */

	if (g_file_get_contents(filename, &contents, NULL, NULL)) {
		VObject *vcal;
		icalcomponent *icalcomp;

		icalcomp = e_cal_util_parse_ics_string (contents);

		if (icalcomp && is_icalcomp_usable (icalcomp)) {
			/* If we can create proper iCalendar from the file, then
			   rather use ics importer, because it knows to read more
			   information than older version, the vCalendar. */
			ret = FALSE;
			g_free (contents);
		} else {
			if (icalcomp)
				icalcomponent_free (icalcomp);

			/* parse the file */
			vcal = Parse_MIME (contents, strlen (contents));
			g_free (contents);

			if (vcal) {
				icalcomp = icalvcal_convert (vcal);

				if (icalcomp) {
					icalcomponent_free (icalcomp);
					ret = TRUE;
				}

				cleanVObject (vcal);
			}
		}
	}
	g_free (filename);

	return ret;
}

/* This tries to load in a vCalendar file and convert it to an icalcomponent.
   It returns NULL on failure. */
static icalcomponent*
load_vcalendar_file (const gchar *filename)
{
	icalvcal_defaults defaults = { NULL };
	icalcomponent *icalcomp = NULL;
	gchar *contents;
	gchar *default_alarm_filename;

	default_alarm_filename = g_build_filename (EVOLUTION_SOUNDDIR,
						   "default_alarm.wav",
						   NULL);
	defaults.alarm_audio_url = g_filename_to_uri (default_alarm_filename,
						      NULL, NULL);
	g_free (default_alarm_filename);
	defaults.alarm_audio_fmttype = (gchar *) "audio/x-wav";
	defaults.alarm_description = (gchar *) _("Reminder!");

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

static void
vcal_import(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	gchar *filename;
	icalcomponent *icalcomp;
	EImportTargetURI *s = (EImportTargetURI *)target;

	filename = g_filename_from_uri(s->uri_src, NULL, NULL);
	if (!filename) {
		e_import_complete(ei, target);
		return;
	}

	icalcomp = load_vcalendar_file(filename);
	g_free (filename);
	if (icalcomp)
		ivcal_import(ei, target, icalcomp);
	else
		e_import_complete(ei, target);
}

static GtkWidget *
vcal_get_preview (EImport *ei, EImportTarget *target, EImportImporter *im)
{
	GtkWidget *preview;
	EImportTargetURI *s = (EImportTargetURI *)target;
	gchar *filename;
	icalcomponent *icalcomp;

	filename = g_filename_from_uri (s->uri_src, NULL, NULL);
	if (filename == NULL) {
		g_message (G_STRLOC ": Couldn't get filename from URI '%s'", s->uri_src);
		return NULL;
	}

	icalcomp = load_vcalendar_file (filename);
	g_free (filename);

	if (!icalcomp)
		return NULL;

	preview = ical_get_preview (icalcomp);

	icalcomponent_free (icalcomp);

	return preview;
}

static EImportImporter vcal_importer = {
	E_IMPORT_TARGET_URI,
	0,
	vcal_supported,
	ivcal_getwidget,
	vcal_import,
	ivcal_cancel,
	vcal_get_preview,
};

EImportImporter *
vcal_importer_peek(void)
{
	vcal_importer.name = _("vCalendar files (.vcs)");
	vcal_importer.description = _("Evolution vCalendar importer");

	return &vcal_importer;
}

/* ********************************************************************** */

static gboolean
gnome_calendar_supported(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	gchar *filename;
	gboolean res;

	if (target->type != E_IMPORT_TARGET_HOME)
		return FALSE;

	filename = g_build_filename(g_get_home_dir (), "user-cal.vcf", NULL);
	res = g_file_test(filename, G_FILE_TEST_IS_REGULAR);
	g_free (filename);

	return res;
}

static void
gnome_calendar_import(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	icalcomponent *icalcomp = NULL;
	gchar *filename;
	GList *vtodos;
	ECal *calendar_client = NULL, *tasks_client = NULL;
	gint t;
	gint do_calendar, do_tasks;
	ICalIntelligentImporter *ici;

	/* This is pretty shitty, everything runs in the gui thread and can block
	   for quite some time */

	do_calendar = GPOINTER_TO_INT(g_datalist_get_data(&target->data, "gnomecal-do-cal"));
	do_tasks = GPOINTER_TO_INT(g_datalist_get_data(&target->data, "gnomecal-do-tasks"));

	/* If neither is selected, just return. */
	if (!do_calendar && !do_tasks)
		return;

	e_import_status(ei, target, _("Opening calendar"), 0);

	/* Try to open the default calendar & tasks folders. */
	if (do_calendar) {
		calendar_client = e_auth_new_cal_from_default (E_CAL_SOURCE_TYPE_EVENT);
		if (!calendar_client)
			goto out;
	}

	if (do_tasks) {
		tasks_client = e_auth_new_cal_from_default (E_CAL_SOURCE_TYPE_TODO);
		if (!tasks_client)
			goto out;
	}

	/* Load the Gnome Calendar file and convert to iCalendar. */
	filename = g_build_filename(g_get_home_dir (), "user-cal.vcf", NULL);
	icalcomp = load_vcalendar_file (filename);
	g_free (filename);

	/* If we couldn't load the file, just return. FIXME: Error message? */
	if (!icalcomp)
		goto out;

	ici = g_malloc0(sizeof(*ici));
	g_datalist_set_data_full(&target->data, "gnomecal-data", ici, g_free);

	/* Wait for client to finish opening the calendar & tasks folders. */
	for (t = 0; t < IMPORTER_TIMEOUT_SECONDS; t++) {
		ECalLoadState calendar_state, tasks_state;

		calendar_state = tasks_state = E_CAL_LOAD_LOADED;

		/* We need this so the ECal gets notified that the
		   folder is opened, via Corba. */
		while (gtk_events_pending ())
			gtk_main_iteration ();

		if (do_calendar)
			calendar_state = e_cal_get_load_state (calendar_client);

		if (do_tasks)
			tasks_state = e_cal_get_load_state (tasks_client);

		if (calendar_state == E_CAL_LOAD_LOADED
		    && tasks_state == E_CAL_LOAD_LOADED)
			break;

		g_usleep(1000000);
		if (ici->cancelled)
			goto out;
	}

	/* If we timed out, just return. */
	if (t == IMPORTER_TIMEOUT_SECONDS)
		goto out;

	e_import_status(ei, target, _("Importing..."), 0);

	/*
	 * Import the calendar events into the default calendar folder.
	 */
	prepare_events (icalcomp, &vtodos);
	if (do_calendar)
		update_objects (calendar_client, icalcomp);

	if (ici->cancelled)
		goto out;

	/*
	 * Import the tasks into the default tasks folder.
	 */
	prepare_tasks (icalcomp, vtodos);
	if (do_tasks)
		update_objects (tasks_client, icalcomp);

 out:
	if (icalcomp)
		icalcomponent_free (icalcomp);
	if (calendar_client)
		g_object_unref (calendar_client);
	if (tasks_client)
		g_object_unref (tasks_client);

	e_import_complete(ei, target);
}

static void
calendar_toggle_cb(GtkToggleButton *tb, EImportTarget *target)
{
	g_datalist_set_data(&target->data, "gnomecal-do-cal", GINT_TO_POINTER(gtk_toggle_button_get_active(tb)));
}

static void
tasks_toggle_cb(GtkToggleButton *tb, EImportTarget *target)
{
	g_datalist_set_data(&target->data, "gnomecal-do-tasks", GINT_TO_POINTER(gtk_toggle_button_get_active(tb)));
}

static GtkWidget *
gnome_calendar_getwidget(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	GtkWidget *hbox, *w;
	GConfClient *gconf;
	gboolean done_cal, done_tasks;

	gconf = gconf_client_get_default ();
	done_cal = gconf_client_get_bool (gconf, "/apps/evolution/importer/gnome-calendar/calendar", NULL);
	done_tasks = gconf_client_get_bool (gconf, "/apps/evolution/importer/gnome-calendar/tasks", NULL);
	g_object_unref(gconf);

	g_datalist_set_data(&target->data, "gnomecal-do-cal", GINT_TO_POINTER(!done_cal));
	g_datalist_set_data(&target->data, "gnomecal-do-tasks", GINT_TO_POINTER(!done_tasks));

	hbox = gtk_hbox_new (FALSE, 2);

	w = gtk_check_button_new_with_label (_("Calendar Events"));
	gtk_toggle_button_set_active((GtkToggleButton *)w, !done_cal);
	g_signal_connect (w, "toggled", G_CALLBACK (calendar_toggle_cb), target);
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);

	w = gtk_check_button_new_with_label (_("Tasks"));
	gtk_toggle_button_set_active((GtkToggleButton *)w, !done_tasks);
	g_signal_connect (w, "toggled", G_CALLBACK (tasks_toggle_cb), target);
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);

	gtk_widget_show_all (hbox);

	return hbox;
}

static void
gnome_calendar_cancel(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	ICalIntelligentImporter *ici = g_datalist_get_data(&target->data, "gnomecal-data");

	if (ici)
		ici->cancelled = 1;
}

static EImportImporter gnome_calendar_importer = {
	E_IMPORT_TARGET_HOME,
	0,
	gnome_calendar_supported,
	gnome_calendar_getwidget,
	gnome_calendar_import,
	gnome_calendar_cancel,
	NULL, /* get_preview */
};

EImportImporter *
gnome_calendar_importer_peek(void)
{
	gnome_calendar_importer.name = _("Gnome Calendar");
	gnome_calendar_importer.description = _("Evolution Calendar intelligent importer");

	return &gnome_calendar_importer;
}

/* ********************************************************************** */

static gchar *
format_dt (const ECalComponentDateTime *dt, GHashTable *timezones, icaltimezone *users_zone)
{
	struct tm tm;

	g_return_val_if_fail (dt != NULL, NULL);
	g_return_val_if_fail (timezones != NULL, NULL);

	if (!dt->value)
		return NULL;

	dt->value->zone = NULL;
	if (dt->tzid) {
		dt->value->zone = g_hash_table_lookup (timezones, dt->tzid);
		if (!dt->value->zone)
			dt->value->zone = icaltimezone_get_builtin_timezone_from_tzid (dt->tzid);
	}

	if (dt->value->zone)
		tm = icaltimetype_to_tm_with_zone (dt->value, (icaltimezone *) dt->value->zone, users_zone);
	else
		tm = icaltimetype_to_tm (dt->value);

	return e_datetime_format_format_tm ("calendar", "table", dt->value->is_date ? DTFormatKindDate : DTFormatKindDateTime, &tm);
}

static const gchar *
strip_mailto (const gchar *str)
{
	if (!str || g_ascii_strncasecmp (str, "mailto:", 7) != 0)
		return str;

	return str + 7;
}

static void
preview_comp (EWebViewPreview *preview, ECalComponent *comp)
{
	ECalComponentText text = { 0 };
	ECalComponentDateTime dt;
	ECalComponentClassification classif;
	const gchar *str;
	gchar *tmp;
	gint percent;
	gboolean have;
	GHashTable *timezones;
	icaltimezone *users_zone;
	GSList *slist, *l;

	g_return_if_fail (preview != NULL);
	g_return_if_fail (comp != NULL);

	timezones = g_object_get_data (G_OBJECT (preview), "iCalImp-timezones");
	users_zone = g_object_get_data (G_OBJECT (preview), "iCalImp-userszone");

	str = NULL;
	switch (e_cal_component_get_vtype (comp)) {
	case E_CAL_COMPONENT_EVENT:
		str = e_cal_component_has_attendees (comp) ? C_("iCalImp", "Meeting") : C_("iCalImp", "Event");
		break;
	case E_CAL_COMPONENT_TODO:
		str = C_("iCalImp", "Task");
		break;
	case E_CAL_COMPONENT_JOURNAL:
		str = C_("iCalImp", "Memo");
		break;
	default:
		str = "??? Other ???";
		break;
	}

	have = FALSE;
	if (e_cal_component_has_recurrences (comp)) {
		e_web_view_preview_add_section (preview, have ? NULL : str, C_("iCalImp", "has recurrences"));
		have = TRUE;
	}

	if (e_cal_component_is_instance (comp)) {
		e_web_view_preview_add_section (preview, have ? NULL : str, C_("iCalImp", "is an instance"));
		have = TRUE;
	}

	if (e_cal_component_has_alarms (comp)) {
		e_web_view_preview_add_section (preview, have ? NULL : str, C_("iCalImp", "has alarms"));
		have = TRUE;
	}

	if (e_cal_component_has_attachments (comp)) {
		e_web_view_preview_add_section (preview, have ? NULL : str, C_("iCalImp", "has attachments"));
		have = TRUE;
	}

	if (!have) {
		e_web_view_preview_add_section (preview, str, "");
	}

	str = NULL;
	classif = E_CAL_COMPONENT_CLASS_NONE;
	e_cal_component_get_classification (comp, &classif);
	if (classif == E_CAL_COMPONENT_CLASS_PUBLIC) {
		/* Translators: Appointment's classification */
		str = C_("iCalImp", "Public");
	} else if (classif == E_CAL_COMPONENT_CLASS_PRIVATE) {
		/* Translators: Appointment's classification */
		str = C_("iCalImp", "Private");
	} else if (classif == E_CAL_COMPONENT_CLASS_CONFIDENTIAL) {
		/* Translators: Appointment's classification */
		str = C_("iCalImp", "Confidential");
	}
	if (str)
		/* Translators: Appointment's classification section name */
		e_web_view_preview_add_section (preview, C_("iCalImp", "Classification"), str);

	e_cal_component_get_summary (comp, &text);
	if ((text.value && *text.value) || (text.altrep && *text.altrep))
		/* Translators: Appointment's summary */
		e_web_view_preview_add_section (preview, C_("iCalImp", "Summary"), (text.value && *text.value) ? text.value : text.altrep);

	str = NULL;
	e_cal_component_get_location (comp, &str);
	if (str && *str)
		/* Translators: Appointment's location */
		e_web_view_preview_add_section (preview, C_("iCalImp", "Location"), str);

	dt.value = NULL;
	e_cal_component_get_dtstart (comp, &dt);
	if (dt.value) {
		tmp = format_dt (&dt, timezones, users_zone);
		if (tmp)
			/* Translators: Appointment's start time */
			e_web_view_preview_add_section (preview, C_("iCalImp", "Start"), tmp);
		g_free (tmp);
	}
	e_cal_component_free_datetime (&dt);

	dt.value = NULL;
	e_cal_component_get_due (comp, &dt);
	if (dt.value) {
		tmp = format_dt (&dt, timezones, users_zone);
		if (tmp)
			/* Translators: 'Due' like the time due a task should be finished */
			e_web_view_preview_add_section (preview, C_("iCalImp", "Due"), tmp);
		g_free (tmp);
	} else {
		e_cal_component_free_datetime (&dt);

		dt.value = NULL;
		e_cal_component_get_dtend (comp, &dt);
		if (dt.value) {
			tmp = format_dt (&dt, timezones, users_zone);

			if (tmp)
				/* Translators: Appointment's end time */
				e_web_view_preview_add_section (preview, C_("iCalImp", "End"), tmp);
			g_free (tmp);
		}
	}
	e_cal_component_free_datetime (&dt);

	str = NULL;
	e_cal_component_get_categories (comp, &str);
	if (str && *str)
		/* Translators: Appointment's categories */
		e_web_view_preview_add_section (preview, C_("iCalImp", "Categories"), str);

	percent = e_cal_component_get_percent_as_int (comp);
	if (percent >= 0) {
		tmp = NULL;
		if (percent == 100) {
			icaltimetype *completed = NULL;

			e_cal_component_get_completed (comp, &completed);

			if (completed) {
				dt.tzid = "UTC";
				dt.value = completed;

				tmp = format_dt (&dt, timezones, users_zone);

				e_cal_component_free_icaltimetype (completed);
			}
		}

		if (!tmp)
			tmp = g_strdup_printf ("%d%%", percent);

		/* Translators: Appointment's complete value (either percentage, or a date/time of a completion) */
		e_web_view_preview_add_section (preview, C_("iCalImp", "Completed"), tmp);
		g_free (tmp);
	}

	str = NULL;
	e_cal_component_get_url (comp, &str);
	if (str && *str)
		/* Translators: Appointment's URL */
		e_web_view_preview_add_section (preview, C_("iCalImp", "URL"), str);

	if (e_cal_component_has_organizer (comp)) {
		ECalComponentOrganizer organizer = { 0 };

		e_cal_component_get_organizer (comp, &organizer);

		if (organizer.value && *organizer.value) {
			if (organizer.cn && *organizer.cn) {
				tmp = g_strconcat (organizer.cn, " <", strip_mailto (organizer.value), ">", NULL);
				/* Translators: Appointment's organizer */
				e_web_view_preview_add_section (preview, C_("iCalImp", "Organizer"), tmp);
				g_free (tmp);
			} else {
				e_web_view_preview_add_section (preview, C_("iCalImp", "Organizer"), strip_mailto (organizer.value));
			}
		}
	}

	if (e_cal_component_has_attendees (comp)) {
		GSList *attendees = NULL, *a;
		have = FALSE;

		e_cal_component_get_attendee_list (comp, &attendees);

		for (a = attendees; a; a = a->next) {
			ECalComponentAttendee *attnd = a->data;

			if (!attnd || !attnd->value || !*attnd->value)
				continue;

			if (attnd->cn && *attnd->cn) {
				tmp = g_strconcat (attnd->cn, " <", strip_mailto (attnd->value), ">", NULL);
				/* Translators: Appointment's attendees */
				e_web_view_preview_add_section (preview, have ? NULL : C_("iCalImp", "Attendees"), tmp);
				g_free (tmp);
			} else {
				e_web_view_preview_add_section (preview, have ? NULL : C_("iCalImp", "Attendees"), strip_mailto (attnd->value));
			}

			have = TRUE;
		}

		e_cal_component_free_attendee_list (attendees);
	}

	slist = NULL;
	e_cal_component_get_description_list (comp, &slist);
	for (l = slist; l; l = l->next) {
		ECalComponentText *txt = l->data;

		e_web_view_preview_add_section (preview, l != slist ? NULL : C_("iCalImp", "Description"), (txt && txt->value) ? txt->value : "");
	}

	e_cal_component_free_text_list (slist);
}

static void
preview_selection_changed_cb (GtkTreeSelection *selection, EWebViewPreview *preview)
{
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;

	g_return_if_fail (selection != NULL);
	g_return_if_fail (preview != NULL);

	e_web_view_preview_begin_update (preview);

	if (gtk_tree_selection_get_selected (selection, &model, &iter) && model) {
		ECalComponent *comp = NULL;

		gtk_tree_model_get (model, &iter, 3, &comp, -1);

		if (comp) {
			preview_comp (preview, comp);
			g_object_unref (comp);
		}
	}

	e_web_view_preview_end_update (preview);
}

static icaltimezone *
get_users_timezone (void)
{
	/* more or less copy&paste of calendar_config_get_icaltimezone */
	EShell *shell;
	EShellSettings *shell_settings;
	icaltimezone *zone = NULL;
	gchar *location;

	/* FIXME Pass this in. */
	shell = e_shell_get_default ();
	shell_settings = e_shell_get_shell_settings (shell);

	if (e_shell_settings_get_boolean (shell_settings, "cal-use-system-timezone")) {
		location = e_cal_util_get_system_timezone_location ();
	} else {
		GConfClient *client = gconf_client_get_default ();

		location = gconf_client_get_string (client, CALENDAR_CONFIG_TIMEZONE, NULL);

		g_object_unref (client);
	}

	if (location) {
		zone = icaltimezone_get_builtin_timezone (location);

		g_free (location);
	}

	return zone;
}

static void
free_zone_cb (gpointer ptr)
{
	icaltimezone *zone = ptr;

	if (zone)
		icaltimezone_free (zone, 1);
}

static GtkWidget *
ical_get_preview (icalcomponent *icalcomp)
{
	GtkWidget *preview;
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkListStore *store;
	GtkTreeIter iter;
	GHashTable *timezones;
	icalcomponent *subcomp;
	icaltimezone *users_zone;

	if (!icalcomp || !is_icalcomp_usable (icalcomp))
		return NULL;

	store = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, E_TYPE_CAL_COMPONENT);

	timezones = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, free_zone_cb);
	users_zone = get_users_timezone ();

	/* get timezones first */
	for (subcomp = icalcomponent_get_first_component (icalcomp, ICAL_VTIMEZONE_COMPONENT);
	     subcomp;
	     subcomp = icalcomponent_get_next_component (icalcomp,  ICAL_VTIMEZONE_COMPONENT)) {
		icaltimezone *zone = icaltimezone_new ();
		if (!icaltimezone_set_component (zone, icalcomponent_new_clone (subcomp)) || !icaltimezone_get_tzid (zone)) {
			icaltimezone_free (zone, 1);
		} else {
			g_hash_table_insert (timezones, (gchar *) icaltimezone_get_tzid (zone), zone);
		}
	}

	/* then each component */
	for (subcomp = icalcomponent_get_first_component (icalcomp, ICAL_ANY_COMPONENT);
	     subcomp;
	     subcomp = icalcomponent_get_next_component (icalcomp,  ICAL_ANY_COMPONENT)) {
		icalcomponent_kind kind = icalcomponent_isa (subcomp);

		if (kind == ICAL_VEVENT_COMPONENT ||
		    kind == ICAL_VTODO_COMPONENT ||
		    kind == ICAL_VJOURNAL_COMPONENT) {
			ECalComponent *comp = e_cal_component_new ();
			ECalComponentText summary = { 0 };
			ECalComponentDateTime dt = { 0 };
			gchar *formatted_dt;

			if (!e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (subcomp))) {
				g_object_unref (comp);
				continue;
			}

			e_cal_component_get_summary (comp, &summary);
			e_cal_component_get_dtstart (comp, &dt);
			formatted_dt = format_dt (&dt, timezones, users_zone);

			gtk_list_store_append (store, &iter);
			gtk_list_store_set (store, &iter,
				0, kind == ICAL_VEVENT_COMPONENT ? (e_cal_component_has_attendees (comp) ? C_("iCalImp", "Meeting") : C_("iCalImp", "Event")) :
				   kind == ICAL_VTODO_COMPONENT ? C_("iCalImp", "Task") :
				   kind == ICAL_VJOURNAL_COMPONENT ? C_("iCalImp", "Memo") : "??? Other ???",
				1, formatted_dt ? formatted_dt : "",
				2, summary.value && *summary.value ? summary.value : summary.altrep && *summary.altrep ? summary.altrep : "",
				3, comp,
				-1);

			g_free (formatted_dt);
			e_cal_component_free_datetime (&dt);
			g_object_unref (comp);
		}
	}

	if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter)) {
		g_object_unref (store);
		g_hash_table_destroy (timezones);
		return NULL;
	}

	preview = e_web_view_preview_new ();
	gtk_widget_show (preview);

	g_object_set_data_full (G_OBJECT (preview), "iCalImp-timezones", timezones, (GDestroyNotify) g_hash_table_destroy);
	g_object_set_data (G_OBJECT (preview), "iCalImp-userszone", users_zone);

	tree_view = e_web_view_preview_get_tree_view (E_WEB_VIEW_PREVIEW (preview));
	g_return_val_if_fail (tree_view != NULL, NULL);

	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (store));
	g_object_unref (store);

	/* Translators: Column header for a component type; it can be Event, Task or Memo */
	gtk_tree_view_insert_column_with_attributes (tree_view, -1, C_("iCalImp", "Type"),
		gtk_cell_renderer_text_new (), "text", 0, NULL);

	/* Translators: Column header for a component start date/time */
	gtk_tree_view_insert_column_with_attributes (tree_view, -1, C_("iCalImp", "Start"),
		gtk_cell_renderer_text_new (), "text", 1, NULL);

	/* Translators: Column header for a component summary */
	gtk_tree_view_insert_column_with_attributes (tree_view, -1, C_("iCalImp", "Summary"),
		gtk_cell_renderer_text_new (), "text", 2, NULL);

	if (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL) > 1)
		e_web_view_preview_show_tree_view (E_WEB_VIEW_PREVIEW (preview));

	selection = gtk_tree_view_get_selection (tree_view);
	gtk_tree_selection_select_iter (selection, &iter);
	g_signal_connect (selection, "changed", G_CALLBACK (preview_selection_changed_cb), preview);

	preview_selection_changed_cb (selection, E_WEB_VIEW_PREVIEW (preview));

	return preview;
}
