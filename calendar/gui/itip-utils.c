/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 *  JP Rosevear <jpr@ximian.com>
 *
 * Copyright 2001, Ximian, Inc.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-moniker-util.h>
#include <libgnome/gnome-i18n.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkwidget.h>
#include <libical/ical.h>
#include <Evolution-Composer.h>
#include <e-util/e-dialog-utils.h>
#include <e-util/e-time-utils.h>
#include <libecal/e-cal-time-util.h>
#include <libecal/e-cal-util.h>
#include <libsoup/soup-session-async.h>
#include <libsoup/soup-message.h>
#include <libsoup/soup-uri.h>
#include "e-util/e-passwords.h"
#include "calendar-config.h"
#include "itip-utils.h"

#define GNOME_EVOLUTION_COMPOSER_OAFIID "OAFIID:GNOME_Evolution_Mail_Composer:" BASE_VERSION

static gchar *itip_methods[] = {
	"PUBLISH",
	"REQUEST",
	"REPLY",
	"ADD",
	"CANCEL",
	"RERESH",
	"COUNTER",
	"DECLINECOUNTER"
};

static icalproperty_method itip_methods_enum[] = {
    ICAL_METHOD_PUBLISH,
    ICAL_METHOD_REQUEST,
    ICAL_METHOD_REPLY,
    ICAL_METHOD_ADD,
    ICAL_METHOD_CANCEL,
    ICAL_METHOD_REFRESH,
    ICAL_METHOD_COUNTER,
    ICAL_METHOD_DECLINECOUNTER,
};

static EAccountList *accounts = NULL;

EAccountList *
itip_addresses_get (void)
{
	if (accounts == NULL) {
		GConfClient *gconf_client = gconf_client_get_default ();
		accounts = e_account_list_new (gconf_client);
		g_object_unref (gconf_client);
	}

	return accounts;
}

EAccount *
itip_addresses_get_default (void)
{
	return (EAccount *)e_account_list_get_default(itip_addresses_get());
}

gboolean
itip_organizer_is_user (ECalComponent *comp, ECal *client)
{
	ECalComponentOrganizer organizer;
	const char *strip;
	gboolean user_org = FALSE;
	
	if (!e_cal_component_has_organizer (comp))
		return FALSE;

	e_cal_component_get_organizer (comp, &organizer);
	if (organizer.value != NULL) {

  		strip = itip_strip_mailto (organizer.value);
  
 		if (e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_ORGANIZER_NOT_EMAIL_ADDRESS)) { 
 			char *email;
 			
  			if (e_cal_get_cal_address (client, &email, NULL) && !g_strcasecmp (email, strip)) {
				g_free (email);
				
 				return TRUE;
			}
			
 			return FALSE;
 		}
 	
		user_org = e_account_list_find(itip_addresses_get(), E_ACCOUNT_FIND_ID_ADDRESS, strip) != NULL;
	}

	return user_org;
}

gboolean
itip_sentby_is_user (ECalComponent *comp)
{
	ECalComponentOrganizer organizer;
	const char *strip;
	gboolean user_sentby = FALSE;
	
	if (!e_cal_component_has_organizer (comp))
		return FALSE;

	e_cal_component_get_organizer (comp, &organizer);
	if (organizer.sentby != NULL) {
		strip = itip_strip_mailto (organizer.sentby);
		user_sentby = e_account_list_find(itip_addresses_get(), E_ACCOUNT_FIND_ID_ADDRESS, strip) != NULL;
	}

	return user_sentby;
}
				 
const gchar *
itip_strip_mailto (const gchar *address) 
{
	if (address == NULL)
		return NULL;
	
	if (!g_strncasecmp (address, "mailto:", 7))
		address += 7;

	return address;
}

static char *
get_label (struct icaltimetype *tt)
{
        char buffer[1000];
        struct tm tmp_tm;

	tmp_tm = icaltimetype_to_tm (tt);
        e_time_format_date_and_time (&tmp_tm,
                                     calendar_config_get_24_hour_format (),
                                     FALSE, FALSE,
                                     buffer, 1000);

        return g_strdup (buffer);
}

typedef struct {
	GHashTable *tzids;
	icalcomponent *icomp;	
	ECal *client;
	icalcomponent *zones;
} ItipUtilTZData;


static void
foreach_tzid_callback (icalparameter *param, gpointer data)
{
	ItipUtilTZData *tz_data = data;	
	const char *tzid;
	icaltimezone *zone = NULL;
	icalcomponent *vtimezone_comp;

	/* Get the TZID string from the parameter. */
	tzid = icalparameter_get_tzid (param);
	if (!tzid || g_hash_table_lookup (tz_data->tzids, tzid))
		return;

	/* Look for the timezone */
	if (tz_data->zones != NULL)
		zone = icalcomponent_get_timezone (tz_data->zones, tzid);
	if (zone == NULL)
		zone = icaltimezone_get_builtin_timezone_from_tzid (tzid);
	if (zone == NULL && tz_data->client != NULL)		
		e_cal_get_timezone (tz_data->client, tzid, &zone, NULL);
	if (zone == NULL)
		return;

	/* Convert it to a string and add it to the hash. */
	vtimezone_comp = icaltimezone_get_component (zone);
	if (!vtimezone_comp)
		return;

	icalcomponent_add_component (tz_data->icomp, icalcomponent_new_clone (vtimezone_comp));
	g_hash_table_insert (tz_data->tzids, (char *)tzid, (char *)tzid);	
}

static icalcomponent *
comp_toplevel_with_zones (ECalComponentItipMethod method, ECalComponent *comp, ECal *client, icalcomponent *zones)
{
	icalcomponent *top_level, *icomp;
	icalproperty *prop;
	icalvalue *value;
	ItipUtilTZData tz_data;

	top_level = e_cal_util_new_top_level ();

	prop = icalproperty_new (ICAL_METHOD_PROPERTY);
	value = icalvalue_new_method (itip_methods_enum[method]);
	icalproperty_set_value (prop, value);
	icalcomponent_add_property (top_level, prop);

	icomp = e_cal_component_get_icalcomponent (comp);
	icomp = icalcomponent_new_clone (icomp);
	
	tz_data.tzids = g_hash_table_new (g_str_hash, g_str_equal);
	tz_data.icomp = top_level;
	tz_data.client = client;
	tz_data.zones = zones;
	icalcomponent_foreach_tzid (icomp, foreach_tzid_callback, &tz_data);
	g_hash_table_destroy (tz_data.tzids);

	icalcomponent_add_component (top_level, icomp);

	return top_level;
}

static gboolean
users_has_attendee (GList *users, const char *address)
{
	GList *l;

	for (l = users; l != NULL; l = l->next) {
		if (!g_strcasecmp (address, l->data))
			return TRUE;
	}

	return FALSE;
}

static CORBA_char *
comp_from (ECalComponentItipMethod method, ECalComponent *comp)
{
	ECalComponentOrganizer organizer;
	ECalComponentAttendee *attendee;
	GSList *attendees;
	CORBA_char *str;
	
	switch (method) {
	case E_CAL_COMPONENT_METHOD_PUBLISH:
		return CORBA_string_dup ("");
		
	case E_CAL_COMPONENT_METHOD_REQUEST:
	case E_CAL_COMPONENT_METHOD_CANCEL:
	case E_CAL_COMPONENT_METHOD_ADD:
		e_cal_component_get_organizer (comp, &organizer);
		if (organizer.value == NULL) {
			e_notice (NULL, GTK_MESSAGE_ERROR,
				  _("An organizer must be set."));
			return NULL;
		}

		return CORBA_string_dup (itip_strip_mailto (organizer.value));

	default:
		if (!e_cal_component_has_attendees (comp))
			return CORBA_string_dup ("");

		e_cal_component_get_attendee_list (comp, &attendees);
		attendee = attendees->data;
		str = CORBA_string_dup (attendee->value ? itip_strip_mailto (attendee->value) : "");
		e_cal_component_free_attendee_list (attendees);

		return str;
	}
}

static GNOME_Evolution_Composer_RecipientList *
comp_to_list (ECalComponentItipMethod method, ECalComponent *comp, GList *users)
{
	GNOME_Evolution_Composer_RecipientList *to_list;
	GNOME_Evolution_Composer_Recipient *recipient;
	ECalComponentOrganizer organizer;
	GSList *attendees, *l;
	gint len;

	switch (method) {
	case E_CAL_COMPONENT_METHOD_REQUEST:
	case E_CAL_COMPONENT_METHOD_CANCEL:
		e_cal_component_get_attendee_list (comp, &attendees);
		len = g_slist_length (attendees);
		if (len <= 0) {
			e_notice (NULL, GTK_MESSAGE_ERROR,
				  _("At least one attendee is necessary"));
			e_cal_component_free_attendee_list (attendees);
			return NULL;
		}
		
		to_list = GNOME_Evolution_Composer_RecipientList__alloc ();
		to_list->_maximum = len;
		to_list->_length = 0;
		to_list->_buffer = CORBA_sequence_GNOME_Evolution_Composer_Recipient_allocbuf (len);

		e_cal_component_get_organizer (comp, &organizer);
		if (organizer.value == NULL) {
			e_notice (NULL, GTK_MESSAGE_ERROR,
				  _("An organizer must be set."));
			return NULL;
		}

		for (l = attendees; l != NULL; l = l->next) {
			ECalComponentAttendee *att = l->data;

			if (users_has_attendee (users, att->value))
				continue;
			else if (!g_strcasecmp (att->value, organizer.value))
				continue;
			
			recipient = &(to_list->_buffer[to_list->_length]);
			if (att->cn)
				recipient->name = CORBA_string_dup (att->cn);
			else
				recipient->name = CORBA_string_dup ("");
			recipient->address = CORBA_string_dup (itip_strip_mailto (att->value));
			
			to_list->_length++;
		}
		e_cal_component_free_attendee_list (attendees);
		break;

	case E_CAL_COMPONENT_METHOD_REPLY:
	case E_CAL_COMPONENT_METHOD_ADD:
	case E_CAL_COMPONENT_METHOD_REFRESH:
	case E_CAL_COMPONENT_METHOD_COUNTER:
	case E_CAL_COMPONENT_METHOD_DECLINECOUNTER:
		e_cal_component_get_organizer (comp, &organizer);
		if (organizer.value == NULL) {
			e_notice (NULL, GTK_MESSAGE_ERROR,
				  _("An organizer must be set."));
			return NULL;
		}
		
		len = 1;

		to_list = GNOME_Evolution_Composer_RecipientList__alloc ();
		to_list->_maximum = len;
		to_list->_length = len;
		to_list->_buffer = CORBA_sequence_GNOME_Evolution_Composer_Recipient_allocbuf (len);
		recipient = &(to_list->_buffer[0]);

		if (organizer.cn != NULL)
			recipient->name = CORBA_string_dup (organizer.cn);
		else
			recipient->name = CORBA_string_dup ("");
		recipient->address = CORBA_string_dup (itip_strip_mailto (organizer.value));
		break;

	default:
		to_list = GNOME_Evolution_Composer_RecipientList__alloc ();
		to_list->_maximum = to_list->_length = 0;
		break;
	}
	CORBA_sequence_set_release (to_list, TRUE);

	return to_list;	
}
	
static CORBA_char *
comp_subject (ECalComponentItipMethod method, ECalComponent *comp)
{
	ECalComponentText caltext;
	const char *description, *prefix = NULL;
	GSList *alist;
	CORBA_char *subject;

	e_cal_component_get_summary (comp, &caltext);
	if (caltext.value != NULL)	
		description = caltext.value;
	else {
		switch (e_cal_component_get_vtype (comp)) {
		case E_CAL_COMPONENT_EVENT:
			description = _("Event information");
		case E_CAL_COMPONENT_TODO:
			description = _("Task information");
		case E_CAL_COMPONENT_JOURNAL:
			description = _("Journal information");
		case E_CAL_COMPONENT_FREEBUSY:
			description = _("Free/Busy information");
		default:
			description = _("Calendar information");
		}
	}

	switch (method) {
	case E_CAL_COMPONENT_METHOD_PUBLISH:
	case E_CAL_COMPONENT_METHOD_REQUEST:
		/* FIXME: If this is an update to a previous
		 * PUBLISH or REQUEST, then
			prefix = U_("Updated");
		 */
		break;

	case E_CAL_COMPONENT_METHOD_REPLY:
		e_cal_component_get_attendee_list (comp, &alist);
		if (alist != NULL) {
			ECalComponentAttendee *a = alist->data;

			switch (a->status) {
			case ICAL_PARTSTAT_ACCEPTED:
				prefix = _("Accepted");
				break;
			case ICAL_PARTSTAT_TENTATIVE:
				prefix = _("Tentatively Accepted");
				break;
			case ICAL_PARTSTAT_DECLINED:
				prefix = _("Declined");
				break;
			default:
				break;
			}
			e_cal_component_free_attendee_list (alist);
		}
		break;

	case E_CAL_COMPONENT_METHOD_ADD:
		prefix = _("Updated");
		break;

	case E_CAL_COMPONENT_METHOD_CANCEL:
		prefix = _("Cancel");
		break;

	case E_CAL_COMPONENT_METHOD_REFRESH:
		prefix = _("Refresh");
		break;

	case E_CAL_COMPONENT_METHOD_COUNTER:
		prefix = _("Counter-proposal");
		break;

	case E_CAL_COMPONENT_METHOD_DECLINECOUNTER:
		prefix = _("Declined");
		break;

	default:
		break;
	}

	if (prefix) {
		subject = CORBA_string_alloc (strlen (description) +
					      strlen (prefix) + 3);
		sprintf (subject, "%s: %s", prefix, description);
	} else
		subject = CORBA_string_dup (description);

	return subject;
}

static CORBA_char *
comp_content_type (ECalComponent *comp, ECalComponentItipMethod method)
{
	char tmp[256];	

	sprintf (tmp, "text/calendar; name=\"%s\"; charset=utf-8; METHOD=%s",
		 e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_FREEBUSY ?
		 "freebusy.ifb" : "calendar.ics", itip_methods[method]);
	return CORBA_string_dup (tmp);

}

static CORBA_char *
comp_filename (ECalComponent *comp)
{
        switch (e_cal_component_get_vtype (comp)) {
        case E_CAL_COMPONENT_FREEBUSY:
                return CORBA_string_dup ("freebusy.ifb");
        default:
                return CORBA_string_dup ("calendar.ics");
        }
}

static CORBA_char *
comp_description (ECalComponent *comp)
{
        CORBA_char *description;
        ECalComponentDateTime dt;
        char *start = NULL, *end = NULL;

        switch (e_cal_component_get_vtype (comp)) {
        case E_CAL_COMPONENT_EVENT:
                return CORBA_string_dup (_("Event information"));
        case E_CAL_COMPONENT_TODO:
                return CORBA_string_dup (_("Task information"));
        case E_CAL_COMPONENT_JOURNAL:
                return CORBA_string_dup (_("Journal information"));
        case E_CAL_COMPONENT_FREEBUSY:
                e_cal_component_get_dtstart (comp, &dt);
                if (dt.value)
                        start = get_label (dt.value);
		e_cal_component_free_datetime (&dt);

		e_cal_component_get_dtend (comp, &dt);
		if (dt.value)
			end = get_label (dt.value);
		e_cal_component_free_datetime (&dt);

                if (start != NULL && end != NULL) {
                        char *tmp;
                        tmp = g_strdup_printf (_("Free/Busy information (%s to %s)"), start, end);
                        description = CORBA_string_dup (tmp);
                        g_free (tmp);
                } else {
                        description = CORBA_string_dup (_("Free/Busy information"));
                }
                g_free (start);
                g_free (end);
                return description;
        default:
                return CORBA_string_dup (_("iCalendar information"));
        }
}

static gboolean
comp_server_send (ECalComponentItipMethod method, ECalComponent *comp, ECal *client, 
		  icalcomponent *zones, GList **users)
{
	icalcomponent *top_level, *returned_icalcomp = NULL;
	gboolean retval = TRUE;
	GError *error = NULL;
	
	top_level = comp_toplevel_with_zones (method, comp, client, zones);
	if (!e_cal_send_objects (client, top_level, users, &returned_icalcomp, &error)) {
		/* FIXME Really need a book problem status code */
		if (error->code != E_CALENDAR_STATUS_OK) {
			/* FIXME Better error message */
			e_notice (NULL, GTK_MESSAGE_ERROR, "Unable to book");
			
			retval = FALSE;
		}
	}

	g_clear_error (&error);

	if (returned_icalcomp)
		icalcomponent_free (returned_icalcomp);
	icalcomponent_free (top_level);

	return retval;
}

static gboolean
comp_limit_attendees (ECalComponent *comp) 
{
	icalcomponent *icomp;
	icalproperty *prop;
	gboolean found = FALSE, match = FALSE;
	GSList *l, *list = NULL;

	icomp = e_cal_component_get_icalcomponent (comp);

	for (prop = icalcomponent_get_first_property (icomp, ICAL_ATTENDEE_PROPERTY);
	     prop != NULL;
	     prop = icalcomponent_get_next_property (icomp, ICAL_ATTENDEE_PROPERTY))
	{
		icalvalue *value;
		const char *attendee;
		char *text;

		/* If we've already found something, just erase the rest */
		if (found) {
			list = g_slist_prepend (list, prop);
			continue;
		}
		
		value = icalproperty_get_value (prop);
		if (!value)
			continue;

		attendee = icalvalue_get_string (value);

		text = g_strdup (itip_strip_mailto (attendee));
		text = g_strstrip (text);
		found = match = e_account_list_find(itip_addresses_get(), E_ACCOUNT_FIND_ID_ADDRESS, text) != NULL;
		g_free (text);
		
		if (!match)
			list = g_slist_prepend (list, prop);
		match = FALSE;
	}

	for (l = list; l != NULL; l = l->next) {
		prop = l->data;

		icalcomponent_remove_property (icomp, prop);
		icalproperty_free (prop);
	}
	g_slist_free (list);

	return found;
}

static void
comp_sentby (ECalComponent *comp, ECal *client)
{
	ECalComponentOrganizer organizer;
	
	e_cal_component_get_organizer (comp, &organizer);
	if (!organizer.value) {
		EAccount *a = itip_addresses_get_default ();

		organizer.value = g_strdup_printf ("MAILTO:%s", a->id->address);
		organizer.sentby = NULL;
		organizer.cn = a->id->name;
		organizer.language = NULL;
		
		e_cal_component_set_organizer (comp, &organizer);
		g_free ((char *) organizer.value);
		
		return;
	}

	if (!itip_organizer_is_user (comp, client) && !itip_sentby_is_user (comp)) {
		EAccount *a = itip_addresses_get_default ();
		
		organizer.value = g_strdup (organizer.value);
		organizer.sentby = g_strdup_printf ("MAILTO:%s", a->id->address);
		organizer.cn = g_strdup (organizer.cn);
		organizer.language = g_strdup (organizer.language);
		
		e_cal_component_set_organizer (comp, &organizer);

		g_free ((char *)organizer.value);
		g_free ((char *)organizer.sentby);
		g_free ((char *)organizer.cn);
		g_free ((char *)organizer.language);
	}
}
static ECalComponent *
comp_minimal (ECalComponent *comp, gboolean attendee)
{
	ECalComponent *clone;
	icalcomponent *icomp, *icomp_clone;
	icalproperty *prop;
	ECalComponentOrganizer organizer;
	const char *uid;
	GSList *comments;
	struct icaltimetype itt;
	ECalComponentRange recur_id;
	
	clone = e_cal_component_new ();
	e_cal_component_set_new_vtype (clone, e_cal_component_get_vtype (comp));

	if (attendee) {
		GSList *attendees;
		
		e_cal_component_get_attendee_list (comp, &attendees);
		e_cal_component_set_attendee_list (clone, attendees);

		if (!comp_limit_attendees (clone)) {
			e_notice (NULL, GTK_MESSAGE_ERROR,
				  _("You must be an attendee of the event."));
			goto error;
		}
	}
	
	itt = icaltime_from_timet_with_zone (time (NULL), FALSE,
					     icaltimezone_get_utc_timezone ());
	e_cal_component_set_dtstamp (clone, &itt);

	e_cal_component_get_organizer (comp, &organizer);
	if (organizer.value == NULL)
		goto error;
	e_cal_component_set_organizer (clone, &organizer);

	e_cal_component_get_uid (comp, &uid);
	e_cal_component_set_uid (clone, uid);

	e_cal_component_get_comment_list (comp, &comments);
	if (g_slist_length (comments) <= 1) {
		e_cal_component_set_comment_list (clone, comments);
	} else {
		GSList *l = comments;
		
		comments = g_slist_remove_link (comments, l);
		e_cal_component_set_comment_list (clone, l);
		e_cal_component_free_text_list (l);
	}
	e_cal_component_free_text_list (comments);
	
	e_cal_component_get_recurid (comp, &recur_id);
	if (recur_id.datetime.value != NULL)
		e_cal_component_set_recurid (clone, &recur_id);
	
	icomp = e_cal_component_get_icalcomponent (comp);
	icomp_clone = e_cal_component_get_icalcomponent (clone);
	for (prop = icalcomponent_get_first_property (icomp, ICAL_X_PROPERTY);
	     prop != NULL;
	     prop = icalcomponent_get_next_property (icomp, ICAL_X_PROPERTY))
	{
		icalproperty *p;
		
		p = icalproperty_new_clone (prop);
		icalcomponent_add_property (icomp_clone, p);
	}

	e_cal_component_rescan (clone);
	
	return clone;

 error:
	g_object_unref (clone);
	return NULL;
}

static ECalComponent *
comp_compliant (ECalComponentItipMethod method, ECalComponent *comp, ECal *client, icalcomponent *zones)
{
	ECalComponent *clone, *temp_clone;
	struct icaltimetype itt;
	
	clone = e_cal_component_clone (comp);
	itt = icaltime_from_timet_with_zone (time (NULL), FALSE,
					     icaltimezone_get_utc_timezone ());
	e_cal_component_set_dtstamp (clone, &itt);

	/* Make UNTIL date a datetime in a simple recurrence */
	if (e_cal_component_has_recurrences (clone)
	    && e_cal_component_has_simple_recurrence (clone)) {
		GSList *rrule_list;
		struct icalrecurrencetype *r;
		
		e_cal_component_get_rrule_list (clone, &rrule_list);
		r = rrule_list->data;

		if (!icaltime_is_null_time (r->until) && r->until.is_date) {
			ECalComponentDateTime dt;
			icaltimezone *from_zone = NULL, *to_zone;
			
			e_cal_component_get_dtstart (clone, &dt);

			if (dt.value->is_date) {
				from_zone = calendar_config_get_icaltimezone ();
			} else if (dt.tzid == NULL) {
				from_zone = icaltimezone_get_utc_timezone ();
			} else {
				if (zones != NULL)
					from_zone = icalcomponent_get_timezone (zones, dt.tzid);
				if (from_zone == NULL)
					from_zone = icaltimezone_get_builtin_timezone_from_tzid (dt.tzid);
				if (from_zone == NULL && client != NULL)
					/* FIXME Error checking */
					e_cal_get_timezone (client, dt.tzid, &from_zone, NULL);
			}
			
			to_zone = icaltimezone_get_utc_timezone ();

			r->until.hour = dt.value->hour;
			r->until.minute = dt.value->minute;
			r->until.second = dt.value->second;
			r->until.is_date = FALSE;
			
			icaltimezone_convert_time (&r->until, from_zone, to_zone);
			r->until.is_utc = TRUE;

			e_cal_component_set_rrule_list (clone, rrule_list);
			e_cal_component_abort_sequence (clone);
		}

		e_cal_component_free_recur_list (rrule_list);
	}
	
	/* We delete incoming alarms anyhow, and this helps with outlook */
	e_cal_component_remove_all_alarms (clone);

	/* Strip X-LIC-ERROR stuff */
	e_cal_component_strip_errors (clone);
	
	/* Comply with itip spec */
	switch (method) {
	case E_CAL_COMPONENT_METHOD_PUBLISH:
		comp_sentby (clone, client);
		e_cal_component_set_attendee_list (clone, NULL);
		break;
	case E_CAL_COMPONENT_METHOD_REQUEST:
		comp_sentby (clone, client);
		break;
	case E_CAL_COMPONENT_METHOD_CANCEL:
		comp_sentby (clone, client);
		break;	
	case E_CAL_COMPONENT_METHOD_REPLY:
		break;
	case E_CAL_COMPONENT_METHOD_ADD:
		break;
	case E_CAL_COMPONENT_METHOD_REFRESH:
		/* Need to remove almost everything */
		temp_clone = comp_minimal (clone, TRUE);
		g_object_unref (clone);
		clone = temp_clone;
		break;
	case E_CAL_COMPONENT_METHOD_COUNTER:
		break;
	case E_CAL_COMPONENT_METHOD_DECLINECOUNTER:
		/* Need to remove almost everything */
		temp_clone = comp_minimal (clone, FALSE);
		g_object_unref (clone);
		clone = temp_clone;
		break;
	default:
		break;
	}

	return clone;
}

gboolean
itip_send_comp (ECalComponentItipMethod method, ECalComponent *send_comp,
		ECal *client, icalcomponent *zones)
{
	GNOME_Evolution_Composer composer_server;
	ECalComponent *comp = NULL;
	icalcomponent *top_level = NULL;
	GList *users = NULL;
	GNOME_Evolution_Composer_RecipientList *to_list = NULL;
	GNOME_Evolution_Composer_RecipientList *cc_list = NULL;
	GNOME_Evolution_Composer_RecipientList *bcc_list = NULL;
	CORBA_char *subject = NULL, *body = NULL, *content_type = NULL;
	CORBA_char *from = NULL, *filename = NULL, *description = NULL;
	GNOME_Evolution_Composer_AttachmentData *attach_data = NULL;
	char *ical_string;
	CORBA_Environment ev;
	gboolean retval = FALSE;
	
	CORBA_exception_init (&ev);

	/* Give the server a chance to manipulate the comp */
	if (method != E_CAL_COMPONENT_METHOD_PUBLISH && !e_cal_get_save_schedules (client)) {
		if (!comp_server_send (method, send_comp, client, zones, &users))
			goto cleanup;
	}
	
	/* Tidy up the comp */
	comp = comp_compliant (method, send_comp, client, zones);
	if (comp == NULL)
		goto cleanup;

	/* Recipients */
	to_list = comp_to_list (method, comp, users);
	if (method != E_CAL_COMPONENT_METHOD_PUBLISH) {
		if (to_list == NULL || to_list->_length == 0) {
			/* We sent them all via the server */
			retval = TRUE;
			goto cleanup;
		}
	}

	cc_list = GNOME_Evolution_Composer_RecipientList__alloc ();
	cc_list->_maximum = cc_list->_length = 0;
	bcc_list = GNOME_Evolution_Composer_RecipientList__alloc ();
	bcc_list->_maximum = bcc_list->_length = 0;
	
	/* Subject information */
	subject = comp_subject (method, comp);
	
	/* From address */
	from = comp_from (method, comp);

	/* Obtain an object reference for the Composer. */
	composer_server = bonobo_activation_activate_from_id (GNOME_EVOLUTION_COMPOSER_OAFIID, 0, NULL, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Could not activate composer: %s", bonobo_exception_get_text (&ev));
		CORBA_exception_free (&ev);
		return FALSE;
	}

	/* Set recipients, subject */
	GNOME_Evolution_Composer_setHeaders (composer_server, from, to_list, cc_list, bcc_list, subject, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Unable to set composer headers while sending iTip message: %s",
			   bonobo_exception_get_text (&ev));
		goto cleanup;
	}


	/* Content type */
	content_type = comp_content_type (comp, method);

	top_level = comp_toplevel_with_zones (method, comp, client, zones);
	ical_string = icalcomponent_as_ical_string (top_level);

	if (e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_EVENT) {
		GNOME_Evolution_Composer_setBody (composer_server, ical_string, content_type, &ev);
	} else {
		GNOME_Evolution_Composer_setMultipartType (composer_server, GNOME_Evolution_Composer_MIXED, &ev);
		if (BONOBO_EX (&ev)) {
			g_warning ("Unable to set multipart type while sending iTip message");
			goto cleanup;
		}

		filename = comp_filename (comp);
		description = comp_description (comp);

		GNOME_Evolution_Composer_setBody (composer_server, description, "text/plain", &ev);
		if (BONOBO_EX (&ev)) {
			g_warning ("Unable to set body text while sending iTip message");
			goto cleanup;
		}

		attach_data = GNOME_Evolution_Composer_AttachmentData__alloc ();
		attach_data->_length = strlen (ical_string);
		attach_data->_maximum = attach_data->_length;	
		attach_data->_buffer = CORBA_sequence_CORBA_char_allocbuf (attach_data->_length);
		memcpy (attach_data->_buffer, ical_string, attach_data->_length);

		GNOME_Evolution_Composer_attachData (composer_server,
						     content_type, filename, description,
						     TRUE, attach_data,
						     &ev);
	}
		
	if (BONOBO_EX (&ev)) {
		g_warning ("Unable to place iTip message in composer");
		goto cleanup;
	}
	
	if (method == E_CAL_COMPONENT_METHOD_PUBLISH) {
		GNOME_Evolution_Composer_show (composer_server, &ev);
		if (BONOBO_EX (&ev))
			g_warning ("Unable to show the composer while sending iTip message");
		else
			retval = TRUE;
	} else {		
		GNOME_Evolution_Composer_send (composer_server, &ev);
		if (BONOBO_EX (&ev))
			g_warning ("Unable to send iTip message");
		else
			retval = TRUE;
	}
	
 cleanup:
	CORBA_exception_free (&ev);

	if (comp != NULL)
		g_object_unref (comp);
	if (top_level != NULL)
		icalcomponent_free (top_level);

	if (to_list != NULL)
		CORBA_free (to_list);
	if (cc_list != NULL)
		CORBA_free (cc_list);
	if (bcc_list != NULL)
		CORBA_free (bcc_list);

	if (from != NULL)
		CORBA_free (from);
	if (subject != NULL)
		CORBA_free (subject);
	if (body != NULL)
		CORBA_free (body);
	if (content_type != NULL)
		CORBA_free (content_type);
	if (filename != NULL)
		CORBA_free (filename);
	if (description != NULL)
		CORBA_free (description);
	if (attach_data != NULL) {
		CORBA_free (attach_data->_buffer);
		CORBA_free (attach_data);
	}

	return retval;
}

gboolean
itip_publish_begin (ECalComponent *pub_comp, ECal *client, 
		    gboolean cloned, ECalComponent **clone)
{
	icalcomponent *icomp =NULL, *icomp_clone = NULL;
	icalproperty *prop;
		
	if (e_cal_component_get_vtype (pub_comp) == E_CAL_COMPONENT_FREEBUSY) {
				
		if (!cloned) {
			*clone = e_cal_component_clone (pub_comp);
			cloned = TRUE;
		} else {
			
			icomp = e_cal_component_get_icalcomponent (pub_comp);
			icomp_clone = e_cal_component_get_icalcomponent (*clone);
			for (prop = icalcomponent_get_first_property (icomp,
						      ICAL_FREEBUSY_PROPERTY);
	     			prop != NULL;
	     			prop = icalcomponent_get_next_property (icomp, 
						       ICAL_FREEBUSY_PROPERTY))
			{
				icalproperty *p;
		
				p = icalproperty_new_clone (prop);
				icalcomponent_add_property (icomp_clone, p);
			}
		}		
	}

	return TRUE;
}

static void
fb_sort (struct icalperiodtype *ipt, int fb_count)
{
	int i,j;
	
	if (ipt == NULL || fb_count == 0)
		return;
	
	for (i = 0; i < fb_count-1; i++) {
		for (j = i+1; j < fb_count; j++) {
			struct icalperiodtype temp;
				
			if (icaltime_compare (ipt[i].start, ipt[j].start) < 0)
				continue;
			
			if (icaltime_compare (ipt[i].start, ipt[j].start) == 0){
				if (icaltime_compare (ipt[i].end, 
						     ipt[j].start) < 0)
					continue;
			}
			temp = ipt[i];
			ipt[i] = ipt[j];
			ipt[j] = temp;
		}
	}
}

static icalcomponent *
comp_fb_normalize (icalcomponent *icomp)
{
	icalcomponent *iclone;
	icalproperty *prop, *p;
	const char *uid,  *comment;
	struct icaltimetype itt;
	int fb_count, i = 0, j;
	struct icalperiodtype *ipt;
	
	iclone = icalcomponent_new (ICAL_VFREEBUSY_COMPONENT);
	
	prop = icalcomponent_get_first_property (icomp, 
						 ICAL_ORGANIZER_PROPERTY);
	if (prop) {
		p = icalproperty_new_clone (prop);
		icalcomponent_add_property (iclone, p);
	}
	
	itt = icalcomponent_get_dtstart (icomp);
	icalcomponent_set_dtstart (iclone, itt);
	
	itt = icalcomponent_get_dtend (icomp);
	icalcomponent_set_dtend (iclone, itt);
	
	fb_count =  icalcomponent_count_properties (icomp, 
						    ICAL_FREEBUSY_PROPERTY);
	ipt = g_new0 (struct icalperiodtype, fb_count+1);
	
	for (prop = icalcomponent_get_first_property (icomp, 
						      ICAL_FREEBUSY_PROPERTY);
		prop != NULL;
		prop = icalcomponent_get_next_property (icomp, 
							ICAL_FREEBUSY_PROPERTY))
	{
		ipt[i] = icalproperty_get_freebusy (prop);
		i++;
	}
	
	fb_sort (ipt, fb_count);
	
	for (j = 0; j <= fb_count-1; j++) {
		icalparameter *param;
		
		prop = icalproperty_new_freebusy (ipt[j]);
		param = icalparameter_new_fbtype (ICAL_FBTYPE_BUSY);
		icalproperty_add_parameter (prop, param);
		icalcomponent_add_property (iclone, prop);
	}
	g_free (ipt);
	
	/* Should I strip this RFC 2446 says there must not be a UID
		if the METHOD is PUBLISH?? */
	uid = icalcomponent_get_uid (icomp);
	if (uid)
		icalcomponent_set_uid (iclone, uid);

	itt = icaltime_from_timet_with_zone (time (NULL), FALSE,
					     icaltimezone_get_utc_timezone ());
	icalcomponent_set_dtstamp (iclone, itt);	
	
	prop = icalcomponent_get_first_property (icomp, ICAL_URL_PROPERTY);
	if (prop) {
		p = icalproperty_new_clone (prop);
		icalcomponent_add_property (iclone, p);
	}
	
	comment =  icalcomponent_get_comment (icomp);
	if (comment)
		icalcomponent_set_comment (iclone, comment);

	for (prop = icalcomponent_get_first_property (icomp, ICAL_X_PROPERTY);
	     prop != NULL;
	     prop = icalcomponent_get_next_property (icomp, ICAL_X_PROPERTY))
	{		
		p = icalproperty_new_clone (prop);
		icalcomponent_add_property (iclone, p);
	}
	
	return iclone;

	g_object_unref (iclone);
	return NULL;
}

gboolean
itip_publish_comp (ECal *client, gchar *uri, gchar *username, 
		   gchar *password, ECalComponent **pub_comp)
{
	icalcomponent *toplevel = NULL, *icalcomp = NULL;
	icalcomponent *icomp = NULL;
	SoupSession *session;
	SoupMessage *msg;
	SoupUri *real_uri;
	char *ical_string;
	
	toplevel = e_cal_util_new_top_level ();
	icalcomponent_set_method (toplevel, ICAL_METHOD_PUBLISH);
	
	e_cal_component_set_url (*pub_comp, uri);
	
	icalcomp = e_cal_component_get_icalcomponent (*pub_comp);
	
	icomp = comp_fb_normalize (icalcomp);	

	icalcomponent_add_component (toplevel, icomp);
	ical_string = icalcomponent_as_ical_string (toplevel);

	/* Publish the component */
	session = soup_session_async_new ();

	real_uri = soup_uri_new (uri);
	if (!real_uri || !real_uri->host) {
		g_warning (G_STRLOC ": Invalid URL: %s", uri);
		g_object_unref (session);
		return FALSE;
	}
	
	real_uri->user = g_strdup (username);
	real_uri->passwd = g_strdup (password);
		
	/* build the SOAP message */
	msg = soup_message_new_from_uri (SOUP_METHOD_PUT, real_uri);
	if (!msg) {
		g_warning (G_STRLOC ": Could not build SOAP message");
		g_object_unref (session);
		return FALSE;
	}
	soup_message_set_flags (msg, SOUP_MESSAGE_NO_REDIRECT);	
	soup_message_set_request (msg, "text/calendar", SOUP_BUFFER_USER_OWNED,
				  ical_string, strlen (ical_string));
	
	/* send message to server */
	soup_session_send_message (session, msg);
	if (!SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
		g_warning(G_STRLOC ": Could not publish Free/Busy: %d: %s", 
			  msg->status_code, 
			  soup_status_get_phrase (msg->status_code));
		g_object_unref (session);
		return FALSE;
	}
	
	soup_uri_free (real_uri);
	g_object_unref (session);
	
	return TRUE;
}
