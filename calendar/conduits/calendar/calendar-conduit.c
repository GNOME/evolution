/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar - Calendar Conduit
 *
 * Copyright (C) 1998 Free Software Foundation
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Eskil Heyn Olsen <deity@eskil.dk> 
 *          JP Rosevear <jpr@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>

#define G_LOG_DOMAIN "ecalconduit"

#include <libecal/e-cal-types.h>
#include <libecal/e-cal.h>
#include <libecal/e-cal-time-util.h>
#include <pi-source.h>
#include <pi-socket.h>
#include <pi-dlp.h>
#include <pi-datebook.h>
#include <gpilotd/gnome-pilot-conduit.h>
#include <gpilotd/gnome-pilot-conduit-sync-abs.h>
#include <libgpilotdCM/gnome-pilot-conduit-management.h>
#include <libgpilotdCM/gnome-pilot-conduit-config.h>
#include <e-pilot-map.h>
#include <e-pilot-settings.h>
#include <e-pilot-util.h>
#include <e-config-listener.h>

GnomePilotConduit * conduit_get_gpilot_conduit (guint32);
void conduit_destroy_gpilot_conduit (GnomePilotConduit*);

#define CONDUIT_VERSION "0.1.6"

#define DEBUG_CALCONDUIT 1
/* #undef DEBUG_CALCONDUIT */

#ifdef DEBUG_CALCONDUIT
#define LOG(x) x
#else
#define LOG(x)
#endif 

#define WARN g_warning
#define INFO g_message

#define PILOT_MAX_ADVANCE 99

typedef struct _ECalLocalRecord ECalLocalRecord;
typedef struct _ECalConduitCfg ECalConduitCfg;
typedef struct _ECalConduitGui ECalConduitGui;
typedef struct _ECalConduitContext ECalConduitContext;

/* Local Record */
struct _ECalLocalRecord {
	/* The stuff from gnome-pilot-conduit-standard-abs.h
	   Must be first in the structure, or instances of this
	   structure cannot be used by gnome-pilot-conduit-standard-abs.
	*/
	GnomePilotDesktopRecord local;

	/* The corresponding Comp object */
	ECalComponent *comp;

        /* pilot-link appointment structure */
	struct Appointment *appt;
};

static void
calconduit_destroy_record (ECalLocalRecord *local)
{
	g_object_unref (local->comp);
	free_Appointment (local->appt);
	g_free (local->appt);	
	g_free (local);
}

/* Configuration */
struct _ECalConduitCfg {
	guint32 pilot_id;
	GnomePilotConduitSyncType  sync_type;

	gboolean secret;
	gboolean multi_day_split;
	
	gchar *last_uri;
};

static ECalConduitCfg *
calconduit_load_configuration (guint32 pilot_id) 
{
	ECalConduitCfg *c;
	GnomePilotConduitManagement *management;
	GnomePilotConduitConfig *config;
 	gchar prefix[256];
	
	c = g_new0 (ECalConduitCfg, 1);
	g_assert (c != NULL);

	/* Pilot ID */
	c->pilot_id = pilot_id;

	/* Sync Type */
	management = gnome_pilot_conduit_management_new ("e_calendar_conduit", GNOME_PILOT_CONDUIT_MGMT_ID);
	gtk_object_ref (GTK_OBJECT (management));
	gtk_object_sink (GTK_OBJECT (management));
	config = gnome_pilot_conduit_config_new (management, pilot_id);
	gtk_object_ref (GTK_OBJECT (config));
	gtk_object_sink (GTK_OBJECT (config));
	if (!gnome_pilot_conduit_config_is_enabled (config, &c->sync_type))
		c->sync_type = GnomePilotConduitSyncTypeNotSet;
	gtk_object_unref (GTK_OBJECT (config));
	gtk_object_unref (GTK_OBJECT (management));

	/* Custom settings */
	g_snprintf (prefix, 255, "/gnome-pilot.d/e-calendar-conduit/Pilot_%u/", pilot_id);
	gnome_config_push_prefix (prefix);

	c->secret = gnome_config_get_bool ("secret=FALSE");
	c->multi_day_split = gnome_config_get_bool ("multi_day_split=TRUE");
	if ((c->last_uri = gnome_config_get_string ("last_uri")) && !strncmp (c->last_uri, "file://", 7)) {
		const char *path = c->last_uri + 7;
		const char *home;
		
		home = g_get_home_dir ();
		
		if (!strncmp (path, home, strlen (home))) {
			path += strlen (home);
			if (*path == '/')
				path++;
			
			if (!strcmp (path, "evolution/local/Calendar/calendar.ics")) {
				/* need to upgrade the last_uri. yay. */
				g_free (c->last_uri);
				c->last_uri = g_strdup_printf ("file://%s/.evolution/calendar/local/system/calendar.ics", home);
			}
		}
	}
	
	gnome_config_pop_prefix (); 

	return c;
}

static void
calconduit_save_configuration (ECalConduitCfg *c) 
{
	gchar prefix[256];

	g_snprintf (prefix, 255, "/gnome-pilot.d/e-calendar-conduit/Pilot_%u/", c->pilot_id);
	gnome_config_push_prefix (prefix);

	gnome_config_set_bool ("secret", c->secret);
	gnome_config_set_bool ("multi_day_split", c->multi_day_split);
	gnome_config_set_string ("last_uri", c->last_uri);

	gnome_config_pop_prefix ();

	gnome_config_sync ();
	gnome_config_drop_all ();
}

static ECalConduitCfg*
calconduit_dupe_configuration (ECalConduitCfg *c) 
{
	ECalConduitCfg *retval;

	g_return_val_if_fail (c != NULL, NULL);

	retval = g_new0 (ECalConduitCfg, 1);
	retval->pilot_id = c->pilot_id;
	retval->sync_type = c->sync_type;
	retval->secret = c->secret;
	retval->multi_day_split = c->multi_day_split;
	retval->last_uri = g_strdup (c->last_uri);
	
	return retval;
}

static void 
calconduit_destroy_configuration (ECalConduitCfg *c) 
{
	g_return_if_fail (c != NULL);

	g_free (c->last_uri);
	g_free (c);
}

/* Gui */
struct _ECalConduitGui {
	GtkWidget *multi_day_split;
};

static ECalConduitGui *
e_cal_gui_new (EPilotSettings *ps) 
{
	ECalConduitGui *gui;
	GtkWidget *lbl;
	gint rows;
	
	g_return_val_if_fail (ps != NULL, NULL);
	g_return_val_if_fail (E_IS_PILOT_SETTINGS (ps), NULL);

	gtk_table_resize (GTK_TABLE (ps), E_PILOT_SETTINGS_TABLE_ROWS + 1, E_PILOT_SETTINGS_TABLE_COLS);

	gui = g_new0 (ECalConduitGui, 1);

	rows = E_PILOT_SETTINGS_TABLE_ROWS;
	lbl = gtk_label_new (_("Split Multi-Day Events:"));
	gui->multi_day_split = gtk_check_button_new ();
	gtk_table_attach_defaults (GTK_TABLE (ps), lbl, 0, 1, rows, rows + 1);
	gtk_table_attach_defaults (GTK_TABLE (ps), gui->multi_day_split, 1, 2, rows, rows + 1);
	gtk_widget_show (lbl);
	gtk_widget_show (gui->multi_day_split);
	
	return gui;
}

static void
e_cal_gui_fill_widgets (ECalConduitGui *gui, ECalConduitCfg *cfg) 
{
	g_return_if_fail (gui != NULL);
	g_return_if_fail (cfg != NULL);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gui->multi_day_split),
				      cfg->multi_day_split);
}

static void
e_cal_gui_fill_config (ECalConduitGui *gui, ECalConduitCfg *cfg) 
{
	g_return_if_fail (gui != NULL);
	g_return_if_fail (cfg != NULL);

	cfg->multi_day_split = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (gui->multi_day_split));
}

static void
e_cal_gui_destroy (ECalConduitGui *gui) 
{
	g_free (gui);
}

/* Context */
struct _ECalConduitContext {
	GnomePilotDBInfo *dbi;

	ECalConduitCfg *cfg;
	ECalConduitCfg *new_cfg;
	ECalConduitGui *gui;
	GtkWidget *ps;
	
	struct AppointmentAppInfo ai;

	ECal *client;

	icaltimezone *timezone;
	ECalComponent *default_comp;
	GList *comps;
	GList *changed;
	GHashTable *changed_hash;
	GList *locals;
	
	EPilotMap *map;
};

static ECalConduitContext *
e_calendar_context_new (guint32 pilot_id) 
{
	ECalConduitContext *ctxt;

	ctxt = g_new0 (ECalConduitContext, 1);
	g_assert (ctxt != NULL);
	
	ctxt->cfg = calconduit_load_configuration (pilot_id);
	ctxt->new_cfg = calconduit_dupe_configuration (ctxt->cfg);
	ctxt->ps = NULL;
	ctxt->dbi = NULL;
	ctxt->client = NULL;
	ctxt->timezone = NULL;
	ctxt->default_comp = NULL;
	ctxt->comps = NULL;
	ctxt->changed = NULL;
	ctxt->changed_hash = NULL;
	ctxt->locals = NULL;
	ctxt->map = NULL;
	
	return ctxt;
}

static gboolean
e_calendar_context_foreach_change (gpointer key, gpointer value, gpointer data) 
{
	g_free (key);

	return TRUE;
}

static void
e_calendar_context_destroy (ECalConduitContext *ctxt)
{
	GList *l;
	
	g_return_if_fail (ctxt != NULL);

	if (ctxt->cfg != NULL)
		calconduit_destroy_configuration (ctxt->cfg);
	if (ctxt->new_cfg != NULL)
		calconduit_destroy_configuration (ctxt->new_cfg);
	if (ctxt->gui != NULL)
		e_cal_gui_destroy (ctxt->gui);

	if (ctxt->client != NULL)
		g_object_unref (ctxt->client);
 	if (ctxt->default_comp != NULL)
 		g_object_unref (ctxt->default_comp);	
	if (ctxt->comps != NULL) {
		for (l = ctxt->comps; l; l = l->next)
			g_object_unref (l->data);
		g_list_free (ctxt->comps);
	}
	
	if (ctxt->changed != NULL)
		e_cal_free_change_list (ctxt->changed);
	
	if (ctxt->changed_hash != NULL) {
		g_hash_table_foreach_remove (ctxt->changed_hash, e_calendar_context_foreach_change, NULL);
		g_hash_table_destroy (ctxt->changed_hash);
	}
	
	if (ctxt->locals != NULL) {
		for (l = ctxt->locals; l != NULL; l = l->next)
			calconduit_destroy_record (l->data);
		g_list_free (ctxt->locals);
	}
	
	if (ctxt->map != NULL)
		e_pilot_map_destroy (ctxt->map);
}

/* Debug routines */
static char *
print_local (ECalLocalRecord *local)
{
	static char buff[ 4096 ];

	if (local == NULL) {
		sprintf (buff, "[NULL]");
		return buff;
	}

	if (local->appt && local->appt->description) {
		g_snprintf (buff, 4096, "[%ld %ld '%s' '%s']",
			    mktime (&local->appt->begin),
			    mktime (&local->appt->end),
			    local->appt->description ?
			    local->appt->description : "",
			    local->appt->note ?
			    local->appt->note : "");
		return buff;
	}

	return "";
}

static char *print_remote (GnomePilotRecord *remote)
{
	static char buff[ 4096 ];
	struct Appointment appt;

	if (remote == NULL) {
		sprintf (buff, "[NULL]");
		return buff;
	}

	memset (&appt, 0, sizeof (struct Appointment));
	unpack_Appointment (&appt, remote->record, remote->length);

	g_snprintf (buff, 4096, "[%ld %ld '%s' '%s']",
		    mktime (&appt.begin),
		    mktime (&appt.end),
		    appt.description ?
		    appt.description : "",
		    appt.note ?
		    appt.note : "");

	free_Appointment (&appt);

	return buff;
}

static int
start_calendar_server (ECalConduitContext *ctxt)
{
	g_return_val_if_fail (ctxt != NULL, -2);
	
	/* FIXME Need a mechanism for the user to select uri's */
	/* FIXME Can we use the cal model? */
	
	if (!e_cal_open_default (&ctxt->client, E_CAL_SOURCE_TYPE_EVENT, NULL, NULL, NULL))
		return -1;
	
	return 0;
}

/* Utility routines */
static icaltimezone *
get_timezone (ECal *client, const char *tzid) 
{
	icaltimezone *timezone = NULL;

	timezone = icaltimezone_get_builtin_timezone_from_tzid (tzid);
	if (timezone == NULL)		 
		 e_cal_get_timezone (client, tzid, &timezone, NULL);
	
	return timezone;
}

static icaltimezone *
get_default_timezone (void)
{
	EConfigListener *listener;
	icaltimezone *timezone = NULL;
	char *location;

	listener = e_config_listener_new ();

	location = e_config_listener_get_string_with_default (listener,
		"/apps/evolution/calendar/display/timezone", "UTC", NULL);
	if (!location || !location[0]) {
		g_free (location);
		location = g_strdup ("UTC");
	}
	
	timezone = icaltimezone_get_builtin_timezone (location);
	g_free (location);

	g_object_unref (listener);

	return timezone;	
}


static char *
map_name (ECalConduitContext *ctxt) 
{
	char *filename;
	
	filename = g_strdup_printf ("%s/.evolution/calendar/local/system/pilot-map-calendar-%d.xml",
				    g_get_home_dir (), ctxt->cfg->pilot_id);
	
	return filename;
}

static icalrecurrencetype_weekday
get_ical_day (int day) 
{
	switch (day) {
	case 0:
		return ICAL_SUNDAY_WEEKDAY;
	case 1:
		return ICAL_MONDAY_WEEKDAY;
	case 2:
		return ICAL_TUESDAY_WEEKDAY;
	case 3:
		return ICAL_WEDNESDAY_WEEKDAY;
	case 4:
		return ICAL_THURSDAY_WEEKDAY;
	case 5:
		return ICAL_FRIDAY_WEEKDAY;
	case 6:
		return ICAL_SATURDAY_WEEKDAY;
	}

	return ICAL_NO_WEEKDAY;
}

static int
get_pilot_day (icalrecurrencetype_weekday wd) 
{
	switch (wd) {
	case ICAL_SUNDAY_WEEKDAY:
		return 0;
	case ICAL_MONDAY_WEEKDAY:
		return 1;
	case ICAL_TUESDAY_WEEKDAY:
		return 2;
	case ICAL_WEDNESDAY_WEEKDAY:
		return 3;
	case ICAL_THURSDAY_WEEKDAY:
		return 4;
	case ICAL_FRIDAY_WEEKDAY:
		return 5;
	case ICAL_SATURDAY_WEEKDAY:
		return 6;
	default:
		return -1;
	}
}

static gboolean
is_empty_time (struct tm time) 
{
	if (time.tm_sec || time.tm_min || time.tm_hour 
	    || time.tm_mday || time.tm_mon || time.tm_year) 
		return FALSE;
	
	return TRUE;
}

static gboolean
is_all_day (ECal *client, ECalComponentDateTime *dt_start, ECalComponentDateTime *dt_end) 
{
	time_t dt_start_time, dt_end_time;
	icaltimezone *timezone;

	if (dt_start->value->is_date && dt_end->value->is_date)
		return TRUE;
	
	timezone = get_timezone (client, dt_start->tzid);
	dt_start_time = icaltime_as_timet_with_zone (*dt_start->value, timezone);
	dt_end_time = icaltime_as_timet_with_zone (*dt_end->value, get_timezone (client, dt_end->tzid));

	if (dt_end_time == time_add_day_with_zone (dt_start_time, 1, timezone))
		return TRUE;

	return FALSE;
}

static gboolean
process_multi_day (ECalConduitContext *ctxt, ECalChange *ccc, GList **multi_comp, GList **multi_ccc)
{
	ECalComponentDateTime dt_start, dt_end;
	icaltimezone *tz_start, *tz_end;
	time_t event_start, event_end, day_end;
	struct icaltimetype *old_start_value, *old_end_value;
	const char *uid;
	gboolean is_date = FALSE;
	gboolean last = FALSE;
	gboolean ret = TRUE;

	*multi_ccc = NULL;
	*multi_comp = NULL;
	
	if (ccc->type == E_CAL_CHANGE_DELETED)
		return FALSE;

	/* Start time */
	e_cal_component_get_dtstart (ccc->comp, &dt_start);
	if (dt_start.value->is_date)
		tz_start = ctxt->timezone;
	else
		tz_start = get_timezone (ctxt->client, dt_start.tzid);
	event_start = icaltime_as_timet_with_zone (*dt_start.value, tz_start);

	e_cal_component_get_dtend (ccc->comp, &dt_end);
	if (dt_end.value->is_date)
		tz_end = ctxt->timezone;
	else
		tz_end = get_timezone (ctxt->client, dt_end.tzid);
	event_end = icaltime_as_timet_with_zone (*dt_end.value, tz_end);

	day_end = time_day_end_with_zone (event_start, ctxt->timezone);			
	if (day_end >= event_end) {
		ret = FALSE;
		goto cleanup;
	} else if (e_cal_component_has_recurrences (ccc->comp) || !ctxt->cfg->multi_day_split) {
		ret = TRUE;
		goto cleanup;
	}

	if (dt_start.value->is_date && dt_end.value->is_date)
		is_date = TRUE;
	
	old_start_value = dt_start.value;
 	old_end_value = dt_end.value;
	while (!last) {
		ECalComponent *clone = e_cal_component_clone (ccc->comp);
		char *new_uid = e_cal_component_gen_uid ();
		struct icaltimetype start_value, end_value;
		ECalChange *c = g_new0 (ECalChange, 1);
		
		if (day_end >= event_end) {
			day_end = event_end;
			last = TRUE;
		}

		e_cal_component_set_uid (clone, new_uid);

		start_value = icaltime_from_timet_with_zone (event_start, is_date, tz_start);
		dt_start.value = &start_value;
		e_cal_component_set_dtstart (clone, &dt_start);
		
		end_value = icaltime_from_timet_with_zone (day_end, is_date, tz_end);
		dt_end.value = &end_value;
		e_cal_component_set_dtend (clone, &dt_end);

		/* FIXME Error handling */
		e_cal_create_object (ctxt->client, e_cal_component_get_icalcomponent (clone), NULL, NULL);

		c->comp = clone;
		c->type = E_CAL_CHANGE_ADDED;
		
		*multi_ccc = g_list_prepend (*multi_ccc, c);
		*multi_comp = g_list_prepend (*multi_comp, g_object_ref (c->comp));

		event_start = day_end;
		day_end = time_day_end_with_zone (event_start, ctxt->timezone);
	}
	dt_start.value = old_start_value;
	dt_end.value = old_end_value;
	
	e_cal_component_get_uid (ccc->comp, &uid);
	/* FIXME Error handling */
	e_cal_remove_object (ctxt->client, uid, NULL);
	ccc->type = E_CAL_CHANGE_DELETED;

 cleanup:
	e_cal_component_free_datetime (&dt_start);
	e_cal_component_free_datetime (&dt_end);

	return ret;
}

static short
nth_weekday (int pos, icalrecurrencetype_weekday weekday)
{
	g_assert ((pos > 0 && pos <= 5) || (pos == -1));

	return ((abs (pos) * 8) + weekday) * (pos < 0 ? -1 : 1);
}

static GList *
next_changed_item (ECalConduitContext *ctxt, GList *changes) 
{
	ECalChange *ccc;
	GList *l;
	
	for (l = changes; l != NULL; l = l->next) {
		const char *uid;

		ccc = l->data;
		
		e_cal_component_get_uid (ccc->comp, &uid);
		if (g_hash_table_lookup (ctxt->changed_hash, uid))
			return l;
	}
	
	return NULL;
}

static void
compute_status (ECalConduitContext *ctxt, ECalLocalRecord *local, const char *uid)
{
	ECalChange *ccc;

	local->local.archived = FALSE;
	local->local.secret = FALSE;

	ccc = g_hash_table_lookup (ctxt->changed_hash, uid);
	
	if (ccc == NULL) {
		local->local.attr = GnomePilotRecordNothing;
		return;
	}
	
	switch (ccc->type) {
	case E_CAL_CHANGE_ADDED:
		local->local.attr = GnomePilotRecordNew;
		break;
		
	case E_CAL_CHANGE_MODIFIED:
		local->local.attr = GnomePilotRecordModified;
		break;
		
	case E_CAL_CHANGE_DELETED:
		local->local.attr = GnomePilotRecordDeleted;
		break;
	}
}

static gboolean
rrules_mostly_equal (struct icalrecurrencetype *a, struct icalrecurrencetype *b)
{
	struct icalrecurrencetype acopy, bcopy;
	
	acopy = *a;
	bcopy = *b;
	
	acopy.until = bcopy.until = icaltime_null_time ();
	acopy.count = bcopy.count = 0;
	
	if (!memcmp (&acopy, &bcopy, sizeof (struct icalrecurrencetype)))
		return TRUE;
	
	return FALSE;
}

static gboolean
find_last_cb (ECalComponent *comp, time_t start, time_t end, gpointer data)
{
	time_t *last = data;
	
	*last = start;

	return TRUE;
}

static GnomePilotRecord
local_record_to_pilot_record (ECalLocalRecord *local,
			      ECalConduitContext *ctxt)
{
	GnomePilotRecord p;
	static char record[0xffff];

	g_assert (local->comp != NULL);
	g_assert (local->appt != NULL );
	
	p.ID = local->local.ID;
	p.category = local->local.category;
	p.attr = local->local.attr;
	p.archived = local->local.archived;
	p.secret = local->local.secret;

	/* Generate pilot record structure */
	p.record = record;
	p.length = pack_Appointment (local->appt, p.record, 0xffff);

	return p;	
}

/*
 * converts a ECalComponent object to a ECalLocalRecord
 */
static void
local_record_from_comp (ECalLocalRecord *local, ECalComponent *comp, ECalConduitContext *ctxt) 
{
	const char *uid;
	ECalComponentText summary;
	GSList *d_list = NULL, *edl = NULL, *l;
	ECalComponentText *description;
	ECalComponentDateTime dt_start, dt_end;
	ECalComponentClassification classif;
	icaltimezone *default_tz = ctxt->timezone;
	int i;
	
	g_return_if_fail (local != NULL);
	g_return_if_fail (comp != NULL);

	local->comp = comp;
	g_object_ref (comp);
	
	e_cal_component_get_uid (local->comp, &uid);
	local->local.ID = e_pilot_map_lookup_pid (ctxt->map, uid, TRUE);
	compute_status (ctxt, local, uid);

	local->appt = g_new0 (struct Appointment, 1);

	/* Handle the fields and category we don't sync by making sure
         * we don't overwrite them 
	 */
	if (local->local.ID != 0) {
		struct Appointment appt;		
		char record[0xffff];
		int cat = 0;
		
		if (dlp_ReadRecordById (ctxt->dbi->pilot_socket, 
					ctxt->dbi->db_handle,
					local->local.ID, &record, 
					NULL, NULL, NULL, &cat) > 0) {
			local->local.category = cat;
			memset (&appt, 0, sizeof (struct Appointment));
			unpack_Appointment (&appt, record, 0xffff);
			local->appt->alarm = appt.alarm;
			local->appt->advance = appt.advance;
			local->appt->advanceUnits = appt.advanceUnits;
			free_Appointment (&appt);
		}
	}

	/* STOP: don't replace these with g_strdup, since free_Appointment
	   uses free to deallocate */
	e_cal_component_get_summary (comp, &summary);
	if (summary.value) 
		local->appt->description = e_pilot_utf8_to_pchar (summary.value);

	e_cal_component_get_description_list (comp, &d_list);
	if (d_list) {
		description = (ECalComponentText *) d_list->data;
		if (description && description->value)
			local->appt->note = e_pilot_utf8_to_pchar (description->value);
		else
			local->appt->note = NULL;
	} else {
		local->appt->note = NULL;
	}

	/* Start/End */
	e_cal_component_get_dtstart (comp, &dt_start);
	e_cal_component_get_dtend (comp, &dt_end);
	if (dt_start.value) {
		icaltimezone_convert_time (dt_start.value, 
					   get_timezone (ctxt->client, dt_start.tzid),
					   default_tz);
		local->appt->begin = icaltimetype_to_tm (dt_start.value);
	}
	
	if (dt_start.value && dt_end.value) {
		if (is_all_day (ctxt->client, &dt_start, &dt_end)) {
			local->appt->event = 1;
		} else {
			icaltimezone_convert_time (dt_end.value, 
						   get_timezone (ctxt->client, dt_end.tzid),
						   default_tz);
			local->appt->end = icaltimetype_to_tm (dt_end.value);
			local->appt->event = 0;
		}
	} else {
		local->appt->event = 1;
	}
	e_cal_component_free_datetime (&dt_start);
	e_cal_component_free_datetime (&dt_end);	

	/* Recurrence Rules */
	local->appt->repeatType = repeatNone;

	if (!e_cal_component_is_instance (comp)) {
		if (e_cal_component_has_rrules (comp)) {
			GSList *list;
			struct icalrecurrencetype *recur;
		
			e_cal_component_get_rrule_list (comp, &list);
			recur = list->data;
		
			switch (recur->freq) {
			case ICAL_DAILY_RECURRENCE:
				local->appt->repeatType = repeatDaily;
				break;
			case ICAL_WEEKLY_RECURRENCE:
				local->appt->repeatType = repeatWeekly;
				for (i = 0; i <= 7 && recur->by_day[i] != ICAL_RECURRENCE_ARRAY_MAX; i++) {
					icalrecurrencetype_weekday wd;

					wd = icalrecurrencetype_day_day_of_week (recur->by_day[i]);
					local->appt->repeatDays[get_pilot_day (wd)] = 1;
				}
			
				break;
			case ICAL_MONTHLY_RECURRENCE:
				if (recur->by_month_day[0] != ICAL_RECURRENCE_ARRAY_MAX) {
					local->appt->repeatType = repeatMonthlyByDate;
					break;
				}
			
				/* FIX ME Not going to work with -ve by_day */
				local->appt->repeatType = repeatMonthlyByDay;
				switch (icalrecurrencetype_day_position (recur->by_day[0])) {
				case 1:
					local->appt->repeatDay = dom1stSun;
					break;
				case 2:
					local->appt->repeatDay = dom2ndSun;
					break;
				case 3:
					local->appt->repeatDay = dom3rdSun;
					break;
				case 4:
					local->appt->repeatDay = dom4thSun;
					break;
				case 5:
					local->appt->repeatDay = domLastSun;
					break;
				}
				local->appt->repeatDay += get_pilot_day (icalrecurrencetype_day_day_of_week (recur->by_day[0]));
				break;
			case ICAL_YEARLY_RECURRENCE:
				local->appt->repeatType = repeatYearly;
				break;
			default:
				break;
			}

			if (local->appt->repeatType != repeatNone) {
				local->appt->repeatFrequency = recur->interval;
			}
		
			if (!icaltime_is_null_time (recur->until)) {
				local->appt->repeatForever = 0;
				local->appt->repeatEnd = icaltimetype_to_tm_with_zone (&recur->until, 
										       icaltimezone_get_utc_timezone (), 
										       default_tz);
			} else if (recur->count > 0) {
				time_t last = -1;
				struct icaltimetype itt;

				/* The palm does not support count recurrences */
				local->appt->repeatForever = 0;
				e_cal_recur_generate_instances (comp, -1, -1, find_last_cb, &last, 
							      e_cal_resolve_tzid_cb, ctxt->client, 
							      default_tz);
				itt = icaltime_from_timet_with_zone (last, TRUE, default_tz);
				local->appt->repeatEnd = icaltimetype_to_tm (&itt);
			} else {
				local->appt->repeatForever = 1;
			}
		
			e_cal_component_free_recur_list (list);
		}

		/* Exceptions */
		e_cal_component_get_exdate_list (comp, &edl);
		local->appt->exceptions = g_slist_length (edl);
		local->appt->exception = g_new0 (struct tm, local->appt->exceptions);	
		for (l = edl, i = 0; l != NULL; l = l->next, i++) {
			ECalComponentDateTime *dt = l->data;

			icaltimezone_convert_time (dt->value, 
						   icaltimezone_get_utc_timezone (),
						   default_tz);
			*local->appt->exception = icaltimetype_to_tm (dt->value);
		}
		e_cal_component_free_exdate_list (edl);
	}
	
	/* Alarm */
	local->appt->alarm = 0;
	if (e_cal_component_has_alarms (comp)) {
		GList *uids, *l;		
		ECalComponentAlarm *alarm;
		ECalComponentAlarmTrigger trigger;

		uids = e_cal_component_get_alarm_uids (comp);
		for (l = uids; l != NULL; l = l->next) {
			alarm = e_cal_component_get_alarm (comp, l->data);
			e_cal_component_alarm_get_trigger (alarm, &trigger);
			
			if ((trigger.type == E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START
			     && trigger.u.rel_duration.is_neg)) {
				local->appt->advanceUnits = advMinutes;
				local->appt->advance = 
					trigger.u.rel_duration.minutes
					+ trigger.u.rel_duration.hours * 60
					+ trigger.u.rel_duration.days * 60 * 24
					+ trigger.u.rel_duration.weeks * 7 * 60 * 24;

				if (local->appt->advance > PILOT_MAX_ADVANCE) {
					local->appt->advanceUnits = advHours;
					local->appt->advance = 
						trigger.u.rel_duration.minutes / 60
						+ trigger.u.rel_duration.hours
						+ trigger.u.rel_duration.days * 24
						+ trigger.u.rel_duration.weeks * 7 * 24;
				}
				if (local->appt->advance > PILOT_MAX_ADVANCE) {
					local->appt->advanceUnits = advDays;
					local->appt->advance =
						trigger.u.rel_duration.minutes / (60 * 24)
						+ trigger.u.rel_duration.hours / 24
						+ trigger.u.rel_duration.days
						+ trigger.u.rel_duration.weeks * 7;
				}
				if (local->appt->advance > PILOT_MAX_ADVANCE)
					local->appt->advance = PILOT_MAX_ADVANCE;

				local->appt->alarm = 1;
				break;
			}
			e_cal_component_alarm_free (alarm);
		}
		cal_obj_uid_list_free (uids);
	}
	
	e_cal_component_get_classification (comp, &classif);

	if (classif == E_CAL_COMPONENT_CLASS_PRIVATE)
		local->local.secret = 1;
	else
		local->local.secret = 0;

	local->local.archived = 0;  
}

static void 
local_record_from_uid (ECalLocalRecord *local,
		       const char *uid,
		       ECalConduitContext *ctxt)
{
	ECalComponent *comp;
	icalcomponent *icalcomp;
	GError *error = NULL;

	g_assert(local!=NULL);

	if (e_cal_get_object (ctxt->client, uid, NULL, &icalcomp, &error)) {
		comp = e_cal_component_new ();
		if (!e_cal_component_set_icalcomponent (comp, icalcomp)) {
			g_object_unref (comp);
			icalcomponent_free (icalcomp);
			return;
		}

		local_record_from_comp (local, comp, ctxt);
		g_object_unref (comp);
	} else if (error->code == E_CALENDAR_STATUS_OBJECT_NOT_FOUND) {
		comp = e_cal_component_new ();
		e_cal_component_set_new_vtype (comp, E_CAL_COMPONENT_EVENT);
		e_cal_component_set_uid (comp, uid);
		local_record_from_comp (local, comp, ctxt);
		g_object_unref (comp);
	} else {
		INFO ("Object did not exist");
	}

	g_clear_error (&error);
}

static ECalComponent *
comp_from_remote_record (GnomePilotConduitSyncAbs *conduit,
			 GnomePilotRecord *remote,
			 ECalComponent *in_comp,
			 ECal *client,
			 icaltimezone *timezone)
{
	ECalComponent *comp;
	struct Appointment appt;
	struct icaltimetype now = icaltime_from_timet_with_zone (time (NULL), FALSE, timezone), it;
	struct icalrecurrencetype recur;
	ECalComponentText summary = {NULL, NULL};
	ECalComponentDateTime dt = {NULL, NULL};
	GSList *edl = NULL;	
	char *txt;
	int pos, i;
	
	g_return_val_if_fail (remote != NULL, NULL);

	memset (&appt, 0, sizeof (struct Appointment));
	unpack_Appointment (&appt, remote->record, remote->length);

	if (in_comp == NULL) {
		comp = e_cal_component_new ();
		e_cal_component_set_new_vtype (comp, E_CAL_COMPONENT_EVENT);
		e_cal_component_set_created (comp, &now);
	} else {
		comp = e_cal_component_clone (in_comp);
	}

	e_cal_component_set_last_modified (comp, &now);

	summary.value = txt = e_pilot_utf8_from_pchar (appt.description);
	e_cal_component_set_summary (comp, &summary);
	free (txt);

	/* The iCal description field */
	if (!appt.note) {
		e_cal_component_set_description_list (comp, NULL);
	} else {
		GSList l;
		ECalComponentText text;

		text.value = txt = e_pilot_utf8_from_pchar (appt.note);
		text.altrep = NULL;
		l.data = &text;
		l.next = NULL;

		e_cal_component_set_description_list (comp, &l);
		free (txt);
	} 

	if (appt.event && !is_empty_time (appt.begin)) {
		it = tm_to_icaltimetype (&appt.begin, TRUE);
		dt.value = &it;
		dt.tzid = NULL;
		e_cal_component_set_dtstart (comp, &dt);
		e_cal_component_set_dtend (comp, &dt);
	} else {
		dt.tzid = icaltimezone_get_tzid (timezone);

		if (!is_empty_time (appt.begin)) {
			it = tm_to_icaltimetype (&appt.begin, FALSE);
			dt.value = &it;
			e_cal_component_set_dtstart (comp, &dt);
		}

		if (!is_empty_time (appt.end)) {		
			it = tm_to_icaltimetype (&appt.end, FALSE);
			dt.value = &it;
			e_cal_component_set_dtend (comp, &dt);
		}
	}

	/* Recurrence information */
  	icalrecurrencetype_clear (&recur);

	switch (appt.repeatType) {
	case repeatNone:
		recur.freq = ICAL_NO_RECURRENCE;
		break;

	case repeatDaily:
		recur.freq = ICAL_DAILY_RECURRENCE;
		recur.interval = appt.repeatFrequency;
		break;

	case repeatWeekly:
		recur.freq = ICAL_WEEKLY_RECURRENCE;
		recur.interval = appt.repeatFrequency;

		pos = 0;
		for (i = 0; i < 7; i++) {
			if (appt.repeatDays[i])
				recur.by_day[pos++] = get_ical_day (i);
		}
		
		break;

	case repeatMonthlyByDay:
		recur.freq = ICAL_MONTHLY_RECURRENCE;
		recur.interval = appt.repeatFrequency;
		if (appt.repeatDay < domLastSun)
			recur.by_day[0] = nth_weekday ((appt.repeatDay / 7) + 1, 
						       get_ical_day (appt.repeatDay % 7));
		else
			recur.by_day[0] = nth_weekday (-1, get_ical_day (appt.repeatDay % 7));		
		break;
		
	case repeatMonthlyByDate:
		recur.freq = ICAL_MONTHLY_RECURRENCE;
		recur.interval = appt.repeatFrequency;
		recur.by_month_day[0] = appt.begin.tm_mday;
		break;

	case repeatYearly:
		recur.freq = ICAL_YEARLY_RECURRENCE;
		recur.interval = appt.repeatFrequency;
		break;
		
	default:
		g_assert_not_reached ();
	}

	if (recur.freq != ICAL_NO_RECURRENCE) {
		GSList *list = NULL, *existing;
		struct icalrecurrencetype *erecur;
		
		/* recurrence start of week */
		recur.week_start = get_ical_day (appt.repeatWeekstart);

		if (!appt.repeatForever) {
			recur.until = tm_to_icaltimetype (&appt.repeatEnd, TRUE);	
		}

		list = g_slist_append (list, &recur);
		e_cal_component_set_rrule_list (comp, list);

		/* If the desktop uses count and rrules are
		 * equivalent, use count still on the desktop */
		if (!appt.repeatForever && e_cal_component_has_rrules (in_comp)) {
			e_cal_component_get_rrule_list (in_comp, &existing);
			erecur = existing->data;
			
			/* If the rules are otherwise the same and the existing uses count, 
			   see if they end at the same point */
			if (rrules_mostly_equal (&recur, erecur) && 
			    icaltime_is_null_time (erecur->until) && erecur->count > 0) {
				time_t last, elast;
				
				e_cal_recur_generate_instances (comp, -1, -1, find_last_cb, &last, 
							      e_cal_resolve_tzid_cb, client,
							      timezone);
				e_cal_recur_generate_instances (in_comp, -1, -1, find_last_cb, &elast, 
							      e_cal_resolve_tzid_cb, client, 
							      timezone);

				
				if (last == elast) {
					recur.until = icaltime_null_time ();
					recur.count = erecur->count;
					e_cal_component_set_rrule_list (comp, list);
				}
			}
		}

		g_slist_free (list);
	} else {
		e_cal_component_set_rrule_list (comp, NULL);		
	}

	/* Exceptions */
	for (i = 0; i < appt.exceptions; i++) {
		struct tm ex;
		ECalComponentDateTime *dt = g_new0 (ECalComponentDateTime, 1);

		dt->value = g_new0 (struct icaltimetype, 1);
		dt->tzid = NULL;		
		
		ex = appt.exception[i];
		*dt->value = tm_to_icaltimetype (&ex, TRUE);
		
		edl = g_slist_prepend (edl, dt);
	}
	e_cal_component_set_exdate_list (comp, edl);
	e_cal_component_free_exdate_list (edl);

	/* Alarm */
	if (appt.alarm) {
		ECalComponentAlarm *alarm = NULL;
		ECalComponentAlarmTrigger trigger;
		gboolean found = FALSE;
		
		if (e_cal_component_has_alarms (comp)) {
			GList *uids, *l;
			
			uids = e_cal_component_get_alarm_uids (comp);
			for (l = uids; l != NULL; l = l->next) {
				alarm = e_cal_component_get_alarm (comp, l->data);
				e_cal_component_alarm_get_trigger (alarm, &trigger);
				if ((trigger.type == E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START
				     && trigger.u.rel_duration.is_neg)) {
					found = TRUE;
					break;
				}
				e_cal_component_alarm_free (alarm);
			}
			cal_obj_uid_list_free (uids);
		}
		if (!found)
			alarm = e_cal_component_alarm_new ();

		memset (&trigger, 0, sizeof (ECalComponentAlarmTrigger));
		trigger.type = E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START;
		trigger.u.rel_duration.is_neg = 1;
		switch (appt.advanceUnits) {
		case advMinutes:
			trigger.u.rel_duration.minutes = appt.advance;
			break;
		case advHours:
			trigger.u.rel_duration.hours = appt.advance;
			break;
		case advDays:
			trigger.u.rel_duration.days = appt.advance;
			break;
		}
		e_cal_component_alarm_set_trigger (alarm, trigger);
		e_cal_component_alarm_set_action (alarm, E_CAL_COMPONENT_ALARM_DISPLAY);

		if (!found)
			e_cal_component_add_alarm (comp, alarm);
		e_cal_component_alarm_free (alarm);
	}
	
	e_cal_component_set_transparency (comp, E_CAL_COMPONENT_TRANSP_NONE);

	if (remote->secret)
		e_cal_component_set_classification (comp, E_CAL_COMPONENT_CLASS_PRIVATE);
	else
		e_cal_component_set_classification (comp, E_CAL_COMPONENT_CLASS_PUBLIC);

	e_cal_component_commit_sequence (comp);
	
	free_Appointment (&appt);

	return comp;
}

static void
check_for_slow_setting (GnomePilotConduit *c, ECalConduitContext *ctxt)
{
	GnomePilotConduitStandard *conduit = GNOME_PILOT_CONDUIT_STANDARD (c);
	int map_count;
	const char *uri;
	
	/* If there are objects but no log */
	map_count = g_hash_table_size (ctxt->map->pid_map);
	if (map_count == 0)
		gnome_pilot_conduit_standard_set_slow (conduit, TRUE);

	/* Or if the URI's don't match */
	uri = e_cal_get_uri (ctxt->client);
	LOG (g_message ( "  Current URI %s (%s)\n", uri, ctxt->cfg->last_uri ? ctxt->cfg->last_uri : "<NONE>" ));
	if (ctxt->cfg->last_uri != NULL && strcmp (ctxt->cfg->last_uri, uri)) {
		gnome_pilot_conduit_standard_set_slow (conduit, TRUE);
		e_pilot_map_clear (ctxt->map);
	}
	
	if (gnome_pilot_conduit_standard_get_slow (conduit)) {
		ctxt->map->write_touched_only = TRUE;
		LOG (g_message ( "    doing slow sync\n" ));
	} else {
		LOG (g_message ( "    doing fast sync\n" ));
	}
}

/* Pilot syncing callbacks */
static gint
pre_sync (GnomePilotConduit *conduit,
	  GnomePilotDBInfo *dbi,
	  ECalConduitContext *ctxt)
{
	GnomePilotConduitSyncAbs *abs_conduit;
	GList *removed = NULL, *added = NULL, *l;
	int len;
	unsigned char *buf;
	char *filename, *change_id;
	icalcomponent *icalcomp;
	gint num_records, add_records = 0, mod_records = 0, del_records = 0;

	abs_conduit = GNOME_PILOT_CONDUIT_SYNC_ABS (conduit);

	LOG (g_message ( "---------------------------------------------------------\n" ));
	LOG (g_message ( "pre_sync: Calendar Conduit v.%s", CONDUIT_VERSION ));

	ctxt->dbi = dbi;	
	ctxt->client = NULL;

	if (start_calendar_server (ctxt) != 0) {
		WARN(_("Could not start evolution-data-server"));
		gnome_pilot_conduit_error (conduit, _("Could not start evolution-data-server"));
		return -1;
	}

	/* Get the timezone */
	ctxt->timezone = get_default_timezone ();
	if (ctxt->timezone == NULL)
		return -1;
	LOG (g_message ( "  Using timezone: %s", icaltimezone_get_tzid (ctxt->timezone) ));
	
	/* Set the default timezone on the backend. */
	if (ctxt->timezone) {
		if (!e_cal_set_default_timezone (ctxt->client, ctxt->timezone, NULL))
			return -1;
	}

	/* Get the default component */
	if (!e_cal_get_default_object (ctxt->client, &icalcomp, NULL))
		return -1;

	ctxt->default_comp = e_cal_component_new ();
	if (!e_cal_component_set_icalcomponent (ctxt->default_comp, icalcomp)) {
		g_object_unref (ctxt->default_comp);
		icalcomponent_free (icalcomp);
		return -1;
	}

	/* Load the uid <--> pilot id mapping */
	filename = map_name (ctxt);
	e_pilot_map_read (filename, &ctxt->map);
	g_free (filename);

	/* Get the local database */
	if (!e_cal_get_object_list_as_comp (ctxt->client, "#t", &ctxt->comps, NULL))
		return -1;

	/* Find the added, modified and deleted items */
	change_id = g_strdup_printf ("pilot-sync-evolution-calendar-%d", ctxt->cfg->pilot_id);
	if (!e_cal_get_changes (ctxt->client, change_id, &ctxt->changed, NULL))
		return -1;
	ctxt->changed_hash = g_hash_table_new (g_str_hash, g_str_equal);
	g_free (change_id);
	
	/* See if we need to split up any events */
	for (l = ctxt->changed; l != NULL; l = l->next) {
		ECalChange *ccc = l->data;
		GList *multi_comp = NULL, *multi_ccc = NULL;
		
		if (process_multi_day (ctxt, ccc, &multi_comp, &multi_ccc)) {
			ctxt->comps = g_list_concat (ctxt->comps, multi_comp);
			
			added = g_list_concat (added, multi_ccc);
			removed = g_list_prepend (removed, ccc);
		}
	}

	/* Remove the events that were split up */
	g_list_concat (ctxt->changed, added);
	for (l = removed; l != NULL; l = l->next) {
		ECalChange *ccc = l->data;
		const char *uid;

		e_cal_component_get_uid (ccc->comp, &uid);
		if (e_pilot_map_lookup_pid (ctxt->map, uid, FALSE) == 0) {
			ctxt->changed = g_list_remove (ctxt->changed, ccc);
			g_object_unref (ccc->comp);
			g_free (ccc);
		}
	}
	g_list_free (removed);
	
	for (l = ctxt->changed; l != NULL; l = l->next) {
		ECalChange *ccc = l->data;
		const char *uid;
		
		e_cal_component_get_uid (ccc->comp, &uid);
		if (!e_pilot_map_uid_is_archived (ctxt->map, uid)) {

			g_hash_table_insert (ctxt->changed_hash, g_strdup (uid), ccc);

			switch (ccc->type) {
			case E_CAL_CHANGE_ADDED:
				add_records++;
				break;
			case E_CAL_CHANGE_MODIFIED:
				mod_records++;
				break;
			case E_CAL_CHANGE_DELETED:
				del_records++;
				break;
			}
		} else if (ccc->type == E_CAL_CHANGE_DELETED) {
			e_pilot_map_remove_by_uid (ctxt->map, uid);
		}
	}
	
	/* Set the count information */
	num_records = g_list_length (ctxt->comps);
	gnome_pilot_conduit_sync_abs_set_num_local_records(abs_conduit, num_records);
	gnome_pilot_conduit_sync_abs_set_num_new_local_records (abs_conduit, add_records);
	gnome_pilot_conduit_sync_abs_set_num_updated_local_records (abs_conduit, mod_records);
	gnome_pilot_conduit_sync_abs_set_num_deleted_local_records(abs_conduit, del_records);

	buf = (unsigned char*)g_malloc (0xffff);
	len = dlp_ReadAppBlock (dbi->pilot_socket, dbi->db_handle, 0,
			      (unsigned char *)buf, 0xffff);
	
	if (len < 0) {
		WARN (_("Could not read pilot's Calendar application block"));
		WARN ("dlp_ReadAppBlock(...) = %d", len);
		gnome_pilot_conduit_error (conduit,
					   _("Could not read pilot's Calendar application block"));
		return -1;
	}
	unpack_AppointmentAppInfo (&(ctxt->ai), buf, len);
	g_free (buf);

	check_for_slow_setting (conduit, ctxt);
	if (ctxt->cfg->sync_type == GnomePilotConduitSyncTypeCopyToPilot
	    || ctxt->cfg->sync_type == GnomePilotConduitSyncTypeCopyFromPilot)
		ctxt->map->write_touched_only = TRUE;
	
	return 0;
}

static gint
post_sync (GnomePilotConduit *conduit,
	   GnomePilotDBInfo *dbi,
	   ECalConduitContext *ctxt)
{
	GList *changed;
	gchar *filename, *change_id;
	
	LOG (g_message ( "post_sync: Calendar Conduit v.%s", CONDUIT_VERSION ));

	g_free (ctxt->cfg->last_uri);
	ctxt->cfg->last_uri = g_strdup (e_cal_get_uri (ctxt->client));
	calconduit_save_configuration (ctxt->cfg);
	
	filename = map_name (ctxt);
	e_pilot_map_write (filename, ctxt->map);
	g_free (filename);
	
	/* FIX ME ugly hack - our changes musn't count, this does introduce
	 * a race condition if anyone changes a record elsewhere during sycnc
         */
	change_id = g_strdup_printf ("pilot-sync-evolution-calendar-%d", ctxt->cfg->pilot_id);
	if (e_cal_get_changes (ctxt->client, change_id, &changed, NULL))
		e_cal_free_change_list (changed);
	g_free (change_id);
	
	LOG (g_message ( "---------------------------------------------------------\n" ));

	return 0;
}

static gint
set_pilot_id (GnomePilotConduitSyncAbs *conduit,
	      ECalLocalRecord *local,
	      guint32 ID,
	      ECalConduitContext *ctxt)
{
	const char *uid;

	LOG (g_message ( "set_pilot_id: setting to %d\n", ID ));
	
	e_cal_component_get_uid (local->comp, &uid);
	e_pilot_map_insert (ctxt->map, ID, uid, FALSE);

        return 0;
}

static gint
set_status_cleared (GnomePilotConduitSyncAbs *conduit,
		    ECalLocalRecord *local,
		    ECalConduitContext *ctxt)
{
	const char *uid;
	
	LOG (g_message ( "set_status_cleared: clearing status\n" ));
	
	e_cal_component_get_uid (local->comp, &uid);
	g_hash_table_remove (ctxt->changed_hash, uid);
	
        return 0;
}

static gint
for_each (GnomePilotConduitSyncAbs *conduit,
	  ECalLocalRecord **local,
	  ECalConduitContext *ctxt)
{
	static GList *comps, *iterator;
	static int count;

	g_return_val_if_fail (local != NULL, -1);

	if (*local == NULL) {
		LOG (g_message ( "beginning for_each" ));

		comps = ctxt->comps;
		count = 0;
		
		if (comps != NULL) {
			LOG (g_message ( "iterating over %d records", g_list_length (comps)));

			*local = g_new0 (ECalLocalRecord, 1);
			local_record_from_comp (*local, comps->data, ctxt);
			g_list_prepend (ctxt->locals, *local);

			iterator = comps;
		} else {
			LOG (g_message ( "no events" ));
			(*local) = NULL;
			return 0;
		}
	} else {
		count++;
		if (g_list_next (iterator)) {
			iterator = g_list_next (iterator);

			*local = g_new0 (ECalLocalRecord, 1);
			local_record_from_uid (*local, iterator->data, ctxt);
			g_list_prepend (ctxt->locals, *local);
		} else {
			LOG (g_message ( "for_each ending" ));

			/* Tell the pilot the iteration is over */
			*local = NULL;

			return 0;
		}
	}

	return 0;
}

static gint
for_each_modified (GnomePilotConduitSyncAbs *conduit,
		   ECalLocalRecord **local,
		   ECalConduitContext *ctxt)
{
	static GList *iterator;
	static int count;

	g_return_val_if_fail (local != NULL, -1);

	if (*local == NULL) {
		LOG (g_message ( "for_each_modified beginning\n" ));
		
		iterator = ctxt->changed;
		
		count = 0;
	
		LOG (g_message ( "iterating over %d records", g_hash_table_size (ctxt->changed_hash) ));
		
		iterator = next_changed_item (ctxt, iterator);
		if (iterator != NULL) {
			ECalChange *ccc = iterator->data;
		
			*local = g_new0 (ECalLocalRecord, 1);
			local_record_from_comp (*local, ccc->comp, ctxt);
			g_list_prepend (ctxt->locals, *local);
		} else {
			LOG (g_message ( "no events" ));

			*local = NULL;
		}
	} else {
		count++;
		iterator = g_list_next (iterator);
		if (iterator && (iterator = next_changed_item (ctxt, iterator))) {
			ECalChange *ccc = iterator->data;
			
			*local = g_new0 (ECalLocalRecord, 1);
			local_record_from_comp (*local, ccc->comp, ctxt);
			g_list_prepend (ctxt->locals, *local);
		} else {
			LOG (g_message ( "for_each_modified ending" ));

			/* Signal the iteration is over */
			*local = NULL;
		}
	}

	return 0;
}

static gint
compare (GnomePilotConduitSyncAbs *conduit,
	 ECalLocalRecord *local,
	 GnomePilotRecord *remote,
	 ECalConduitContext *ctxt)
{
	/* used by the quick compare */
	GnomePilotRecord local_pilot;
	int retval = 0;

	LOG (g_message ("compare: local=%s remote=%s...\n",
			print_local (local), print_remote (remote)));

	g_return_val_if_fail (local!=NULL,-1);
	g_return_val_if_fail (remote!=NULL,-1);

	local_pilot = local_record_to_pilot_record (local, ctxt);

	if (remote->length != local_pilot.length
	    || memcmp (local_pilot.record, remote->record, remote->length))
		retval = 1;

	if (retval == 0)
		LOG (g_message ( "    equal" ));
	else
		LOG (g_message ( "    not equal" ));
	
	return retval;
}

static gint
add_record (GnomePilotConduitSyncAbs *conduit,
	    GnomePilotRecord *remote,
	    ECalConduitContext *ctxt)
{
	ECalComponent *comp;
	char *uid;
	int retval = 0;
	
	g_return_val_if_fail (remote != NULL, -1);

	LOG (g_message ( "add_record: adding %s to desktop\n", print_remote (remote) ));

	comp = comp_from_remote_record (conduit, remote, ctxt->default_comp, ctxt->client, ctxt->timezone);

	/* Give it a new UID otherwise it will be the uid of the default comp */
	uid = e_cal_component_gen_uid ();
	e_cal_component_set_uid (comp, uid);
	
	if (!e_cal_create_object (ctxt->client, e_cal_component_get_icalcomponent (comp), NULL, NULL))
		return -1;

	e_pilot_map_insert (ctxt->map, remote->ID, uid, FALSE);

	g_free (uid);	

	g_object_unref (comp);
	
	return retval;
}

static gint
replace_record (GnomePilotConduitSyncAbs *conduit,
		ECalLocalRecord *local,
		GnomePilotRecord *remote,
		ECalConduitContext *ctxt)
{
	ECalComponent *new_comp;
	int retval = 0;
	
	g_return_val_if_fail (remote != NULL, -1);

	LOG (g_message ("replace_record: replace %s with %s\n",
			print_local (local), print_remote (remote)));

	new_comp = comp_from_remote_record (conduit, remote, local->comp, ctxt->client, ctxt->timezone);
	g_object_unref (local->comp);
	local->comp = new_comp;

	if (!e_cal_modify_object (ctxt->client, e_cal_component_get_icalcomponent (new_comp), 
				       CALOBJ_MOD_ALL, NULL))
		return -1;

	return retval;
}

static gint
delete_record (GnomePilotConduitSyncAbs *conduit,
	       ECalLocalRecord *local,
	       ECalConduitContext *ctxt)
{
	const char *uid;

	g_return_val_if_fail (local != NULL, -1);
	g_assert (local->comp != NULL);

	e_cal_component_get_uid (local->comp, &uid);

	LOG (g_message ( "delete_record: deleting %s\n", uid ));

	e_pilot_map_remove_by_uid (ctxt->map, uid);
	/* FIXME Error handling */
	e_cal_remove_object (ctxt->client, uid, NULL);
	
        return 0;
}

static gint
archive_record (GnomePilotConduitSyncAbs *conduit,
		ECalLocalRecord *local,
		gboolean archive,
		ECalConduitContext *ctxt)
{
	const char *uid;
	int retval = 0;
	
	g_return_val_if_fail (local != NULL, -1);

	LOG (g_message ( "archive_record: %s\n", archive ? "yes" : "no" ));

	e_cal_component_get_uid (local->comp, &uid);
	e_pilot_map_insert (ctxt->map, local->local.ID, uid, archive);
	
        return retval;
}

static gint
match (GnomePilotConduitSyncAbs *conduit,
       GnomePilotRecord *remote,
       ECalLocalRecord **local,
       ECalConduitContext *ctxt)
{
	const char *uid;
	
	LOG (g_message ("match: looking for local copy of %s\n",
			print_remote (remote)));	
	
	g_return_val_if_fail (local != NULL, -1);
	g_return_val_if_fail (remote != NULL, -1);

	*local = NULL;
	uid = e_pilot_map_lookup_uid (ctxt->map, remote->ID, TRUE);
	
	if (!uid)
		return 0;

	LOG (g_message ( "  matched\n" ));
	
	*local = g_new0 (ECalLocalRecord, 1);
	local_record_from_uid (*local, uid, ctxt);
	
	return 0;
}

static gint
free_match (GnomePilotConduitSyncAbs *conduit,
	    ECalLocalRecord *local,
	    ECalConduitContext *ctxt)
{
	LOG (g_message ( "free_match: freeing\n" ));

	g_return_val_if_fail (local != NULL, -1);

	calconduit_destroy_record (local);

	return 0;
}

static gint
prepare (GnomePilotConduitSyncAbs *conduit,
	 ECalLocalRecord *local,
	 GnomePilotRecord *remote,
	 ECalConduitContext *ctxt)
{
	LOG (g_message ( "prepare: encoding local %s\n", print_local (local) ));

	*remote = local_record_to_pilot_record (local, ctxt);

	return 0;
}

/* Pilot Settings Callbacks */
static void
fill_widgets (ECalConduitContext *ctxt)
{
	e_pilot_settings_set_secret (E_PILOT_SETTINGS (ctxt->ps),
				     ctxt->cfg->secret);

	e_cal_gui_fill_widgets (ctxt->gui, ctxt->cfg);
}

static gint
create_settings_window (GnomePilotConduit *conduit,
			GtkWidget *parent,
			ECalConduitContext *ctxt)
{
	LOG (g_message ( "create_settings_window" ));

	ctxt->ps = e_pilot_settings_new ();
	ctxt->gui = e_cal_gui_new (E_PILOT_SETTINGS (ctxt->ps));

	gtk_container_add (GTK_CONTAINER (parent), ctxt->ps);
	gtk_widget_show (ctxt->ps);

	fill_widgets (ctxt);
	
	return 0;
}
static void
display_settings (GnomePilotConduit *conduit, ECalConduitContext *ctxt)
{
	LOG (g_message ( "display_settings" ));
	
	fill_widgets (ctxt);
}

static void
save_settings    (GnomePilotConduit *conduit, ECalConduitContext *ctxt)
{
	LOG (g_message ( "save_settings" ));

	ctxt->new_cfg->secret =
		e_pilot_settings_get_secret (E_PILOT_SETTINGS (ctxt->ps));
	e_cal_gui_fill_config (ctxt->gui, ctxt->new_cfg);

	calconduit_save_configuration (ctxt->new_cfg);
}

static void
revert_settings  (GnomePilotConduit *conduit, ECalConduitContext *ctxt)
{
	LOG (g_message ( "revert_settings" ));

	calconduit_save_configuration (ctxt->cfg);
	calconduit_destroy_configuration (ctxt->new_cfg);
	ctxt->new_cfg = calconduit_dupe_configuration (ctxt->cfg);
}

GnomePilotConduit *
conduit_get_gpilot_conduit (guint32 pilot_id)
{
	GtkObject *retval;
	ECalConduitContext *ctxt;

	LOG (g_message ( "in calendar's conduit_get_gpilot_conduit\n" ));

	retval = gnome_pilot_conduit_sync_abs_new ("DatebookDB", 0x64617465);
	g_assert (retval != NULL);

	ctxt = e_calendar_context_new (pilot_id);
	gtk_object_set_data (GTK_OBJECT (retval), "calconduit_context", ctxt);

	/* Sync signals */
	gtk_signal_connect (retval, "pre_sync", (GtkSignalFunc) pre_sync, ctxt);
	gtk_signal_connect (retval, "post_sync", (GtkSignalFunc) post_sync, ctxt);

  	gtk_signal_connect (retval, "set_pilot_id", (GtkSignalFunc) set_pilot_id, ctxt);
  	gtk_signal_connect (retval, "set_status_cleared", (GtkSignalFunc) set_status_cleared, ctxt);

  	gtk_signal_connect (retval, "for_each", (GtkSignalFunc) for_each, ctxt);
  	gtk_signal_connect (retval, "for_each_modified", (GtkSignalFunc) for_each_modified, ctxt);
  	gtk_signal_connect (retval, "compare", (GtkSignalFunc) compare, ctxt);

  	gtk_signal_connect (retval, "add_record", (GtkSignalFunc) add_record, ctxt);
  	gtk_signal_connect (retval, "replace_record", (GtkSignalFunc) replace_record, ctxt);
  	gtk_signal_connect (retval, "delete_record", (GtkSignalFunc) delete_record, ctxt);
  	gtk_signal_connect (retval, "archive_record", (GtkSignalFunc) archive_record, ctxt);

  	gtk_signal_connect (retval, "match", (GtkSignalFunc) match, ctxt);
  	gtk_signal_connect (retval, "free_match", (GtkSignalFunc) free_match, ctxt);

  	gtk_signal_connect (retval, "prepare", (GtkSignalFunc) prepare, ctxt);

	/* Gui Settings */
	gtk_signal_connect (retval, "create_settings_window", (GtkSignalFunc) create_settings_window, ctxt);
	gtk_signal_connect (retval, "display_settings", (GtkSignalFunc) display_settings, ctxt);
	gtk_signal_connect (retval, "save_settings", (GtkSignalFunc) save_settings, ctxt);
	gtk_signal_connect (retval, "revert_settings", (GtkSignalFunc) revert_settings, ctxt);

	return GNOME_PILOT_CONDUIT (retval);
}

void
conduit_destroy_gpilot_conduit (GnomePilotConduit *conduit)
{ 
	GtkObject *obj = GTK_OBJECT (conduit);
	ECalConduitContext *ctxt;

	ctxt = gtk_object_get_data (obj, "calconduit_context");
	e_calendar_context_destroy (ctxt);

	gtk_object_destroy (obj);
}
