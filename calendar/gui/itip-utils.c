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
#include <bonobo/bonobo-object-client.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo-conf/bonobo-config-database.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <gtk/gtkwidget.h>
#include <gal/widgets/e-gui-utils.h>
#include <gal/widgets/e-unicode.h>
#include <gal/util/e-unicode-i18n.h>
#include <gal/util/e-util.h>
#include <ical.h>
#include <Evolution-Composer.h>
#include <e-util/e-time-utils.h>
#include <e-util/e-config-listener.h>
#include <cal-util/timeutil.h>
#include <cal-util/cal-util.h>
#include "calendar-config.h"
#include "itip-utils.h"

#define GNOME_EVOLUTION_COMPOSER_OAFIID "OAFIID:GNOME_Evolution_Mail_Composer"

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

static EConfigListener *config = NULL;

static ItipAddress *
get_address (long num) 
{
	ItipAddress *a;
	gchar *path;
		
	a = g_new0 (ItipAddress, 1);

	/* get the identity info */
	path = g_strdup_printf ("/Mail/Accounts/identity_name_%ld", num);
	a->name = e_config_listener_get_string_with_default (config, path, NULL, NULL);
	g_free (path);

	path = g_strdup_printf ("/Mail/Accounts/identity_address_%ld", num);
	a->address = e_config_listener_get_string_with_default (config, path, NULL, NULL);
	a->address = g_strstrip (a->address);
	g_free (path);

	a->full = g_strdup_printf ("%s <%s>", a->name, a->address);

	return a;
}

GList *
itip_addresses_get (void)
{
	GList *addresses = NULL;
	glong len, def, i;

	if (config == NULL)
		config = e_config_listener_new ();
	
	len = e_config_listener_get_long_with_default (config, "/Mail/Accounts/num", 0, NULL);
	def = e_config_listener_get_long_with_default (config, "/Mail/Accounts/default_account", 0, NULL);

	for (i = 0; i < len; i++) {
		ItipAddress *a;

		a = get_address (i);
		if (i == def)
			a->default_address = TRUE;

		addresses = g_list_append (addresses, a);
	}

	return addresses;
}

ItipAddress *
itip_addresses_get_default (void)
{
	ItipAddress *a;
	glong def;

	if (config == NULL)
		config = e_config_listener_new ();
	
	def = e_config_listener_get_long_with_default (config, "/Mail/Accounts/default_account", 0, NULL);

	a = get_address (def);
	a->default_address = TRUE;

	return a;
}

void
itip_address_free (ItipAddress *address) 
{
	g_free (address->name);
	g_free (address->address);
	g_free (address->full);
	g_free (address);
}

void
itip_addresses_free (GList *addresses)
{
	GList *l;
	
	for (l = addresses; l != NULL; l = l->next) {
		ItipAddress *a = l->data;
		itip_address_free (a);
	}
	g_list_free (addresses);
}

gboolean
itip_organizer_is_user (CalComponent *comp, CalClient *client)
{
	CalComponentOrganizer organizer;
	GList *addresses, *l;
	const char *strip;
	gboolean user_org = FALSE;

	if (!cal_component_has_organizer (comp))
		return FALSE;

	cal_component_get_organizer (comp, &organizer);
	if (organizer.value != NULL) {

		strip = itip_strip_mailto (organizer.value);

		if (cal_client_get_static_capability (client, "organizer-not-email-address")) { 
			const char *email;
			
			email = cal_client_get_cal_address (client);
			if (email && !g_strcasecmp (email, strip))
				return TRUE;

			return FALSE;
		}
	
		addresses = itip_addresses_get ();
		for (l = addresses; l != NULL; l = l->next) {
			ItipAddress *a = l->data;
			
			if (!g_strcasecmp (a->address, strip)) {
				user_org = TRUE;
				break;
			}
		}
		itip_addresses_free (addresses);
	}

	return user_org;
}

gboolean
itip_sentby_is_user (CalComponent *comp)
{
	CalComponentOrganizer organizer;
	GList *addresses, *l;
	const char *strip;
	gboolean user_sentby = FALSE;
	
	if (!cal_component_has_organizer (comp))
		return FALSE;

	cal_component_get_organizer (comp, &organizer);
	if (organizer.sentby != NULL) {
		strip = itip_strip_mailto (organizer.sentby);

		addresses = itip_addresses_get ();
		for (l = addresses; l != NULL; l = l->next) {
			ItipAddress *a = l->data;
			
			if (!g_strcasecmp (a->address, strip)) {
				user_sentby = TRUE;
				break;
			}
		}
		itip_addresses_free (addresses);
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
	CalClient *client;
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
		cal_client_get_timezone (tz_data->client, tzid, &zone);
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
comp_toplevel_with_zones (CalComponentItipMethod method, CalComponent *comp, CalClient *client, icalcomponent *zones)
{
	icalcomponent *top_level, *icomp;
	icalproperty *prop;
	icalvalue *value;
	ItipUtilTZData tz_data;

	top_level = cal_util_new_top_level ();

	prop = icalproperty_new (ICAL_METHOD_PROPERTY);
	value = icalvalue_new_method (itip_methods_enum[method]);
	icalproperty_set_value (prop, value);
	icalcomponent_add_property (top_level, prop);

	icomp = cal_component_get_icalcomponent (comp);
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
comp_from (CalComponentItipMethod method, CalComponent *comp)
{
	CalComponentOrganizer organizer;
	CalComponentAttendee *attendee;
	GSList *attendees;
	CORBA_char *str;
	
	switch (method) {
	case CAL_COMPONENT_METHOD_PUBLISH:
		return CORBA_string_dup ("");
		
	case CAL_COMPONENT_METHOD_REQUEST:
	case CAL_COMPONENT_METHOD_CANCEL:
	case CAL_COMPONENT_METHOD_ADD:
		cal_component_get_organizer (comp, &organizer);
		if (organizer.value == NULL) {
			e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
				  _("An organizer must be set."));
			return NULL;
		}

		return CORBA_string_dup (itip_strip_mailto (organizer.value));

	default:
		if (!cal_component_has_attendees (comp))
			return CORBA_string_dup ("");

		cal_component_get_attendee_list (comp, &attendees);
		attendee = attendees->data;
		str = CORBA_string_dup (attendee->value ? itip_strip_mailto (attendee->value) : "");
		cal_component_free_attendee_list (attendees);

		return str;
	}
}

static GNOME_Evolution_Composer_RecipientList *
comp_to_list (CalComponentItipMethod method, CalComponent *comp, GList *users)
{
	GNOME_Evolution_Composer_RecipientList *to_list;
	GNOME_Evolution_Composer_Recipient *recipient;
	CalComponentOrganizer organizer;
	GSList *attendees, *l;
	gint len;

	switch (method) {
	case CAL_COMPONENT_METHOD_REQUEST:
	case CAL_COMPONENT_METHOD_CANCEL:
		cal_component_get_attendee_list (comp, &attendees);
		len = g_slist_length (attendees);
		if (len <= 0) {
			e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
				  _("At least one attendee is necessary"));
			cal_component_free_attendee_list (attendees);
			return NULL;
		}
		
		to_list = GNOME_Evolution_Composer_RecipientList__alloc ();
		to_list->_maximum = len;
		to_list->_length = 0;
		to_list->_buffer = CORBA_sequence_GNOME_Evolution_Composer_Recipient_allocbuf (len);
		
		for (l = attendees; l != NULL; l = l->next) {
			CalComponentAttendee *att = l->data;

			if (users_has_attendee (users, att->value))
				continue;

			recipient = &(to_list->_buffer[to_list->_length]);
			if (att->cn)
				recipient->name = CORBA_string_dup (att->cn);
			else
				recipient->name = CORBA_string_dup ("");
			recipient->address = CORBA_string_dup (itip_strip_mailto (att->value));
			
			to_list->_length++;
		}
		cal_component_free_attendee_list (attendees);
		break;

	case CAL_COMPONENT_METHOD_REPLY:
	case CAL_COMPONENT_METHOD_ADD:
	case CAL_COMPONENT_METHOD_REFRESH:
	case CAL_COMPONENT_METHOD_COUNTER:
	case CAL_COMPONENT_METHOD_DECLINECOUNTER:
		cal_component_get_organizer (comp, &organizer);
		if (organizer.value == NULL) {
			e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
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
comp_subject (CalComponentItipMethod method, CalComponent *comp)
{
	CalComponentText caltext;
	const char *description, *prefix = NULL;
	GSList *alist;
	CORBA_char *subject;

	cal_component_get_summary (comp, &caltext);
	if (caltext.value != NULL)	
		description = caltext.value;
	else {
		switch (cal_component_get_vtype (comp)) {
		case CAL_COMPONENT_EVENT:
			description = U_("Event information");
		case CAL_COMPONENT_TODO:
			description = U_("Task information");
		case CAL_COMPONENT_JOURNAL:
			description = U_("Journal information");
		case CAL_COMPONENT_FREEBUSY:
			description = U_("Free/Busy information");
		default:
			description = U_("Calendar information");
		}
	}

	switch (method) {
	case CAL_COMPONENT_METHOD_PUBLISH:
	case CAL_COMPONENT_METHOD_REQUEST:
		/* FIXME: If this is an update to a previous
		 * PUBLISH or REQUEST, then
			prefix = U_("Updated");
		 */
		break;

	case CAL_COMPONENT_METHOD_REPLY:
		cal_component_get_attendee_list (comp, &alist);
		if (alist != NULL) {
			CalComponentAttendee *a = alist->data;

			switch (a->status) {
			case ICAL_PARTSTAT_ACCEPTED:
				prefix = U_("Accepted");
				break;
			case ICAL_PARTSTAT_TENTATIVE:
				prefix = U_("Tentatively Accepted");
				break;
			case ICAL_PARTSTAT_DECLINED:
				prefix = U_("Declined");
				break;
			default:
				break;
			}
			cal_component_free_attendee_list (alist);
		}
		break;

	case CAL_COMPONENT_METHOD_ADD:
		prefix = U_("Updated");
		break;

	case CAL_COMPONENT_METHOD_CANCEL:
		prefix = U_("Cancel");
		break;

	case CAL_COMPONENT_METHOD_REFRESH:
		prefix = U_("Refresh");
		break;

	case CAL_COMPONENT_METHOD_COUNTER:
		prefix = U_("Counter-proposal");
		break;

	case CAL_COMPONENT_METHOD_DECLINECOUNTER:
		prefix = U_("Declined");
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
comp_content_type (CalComponent *comp, CalComponentItipMethod method)
{
	char tmp[256];	

	sprintf (tmp, "text/calendar; name=\"%s\"; charset=utf-8; METHOD=%s",
		 cal_component_get_vtype (comp) == CAL_COMPONENT_FREEBUSY ?
		 "freebusy.ifb" : "calendar.ics", itip_methods[method]);
	return CORBA_string_dup (tmp);

}

static CORBA_char *
comp_filename (CalComponent *comp)
{
        switch (cal_component_get_vtype (comp)) {
        case CAL_COMPONENT_FREEBUSY:
                return CORBA_string_dup ("freebusy.ifb");
        default:
                return CORBA_string_dup ("calendar.ics");
        }
}

static CORBA_char *
comp_description (CalComponent *comp)
{
        CORBA_char *description;
        CalComponentDateTime dt;
        char *start = NULL, *end = NULL;

        switch (cal_component_get_vtype (comp)) {
        case CAL_COMPONENT_EVENT:
                return CORBA_string_dup (U_("Event information"));
        case CAL_COMPONENT_TODO:
                return CORBA_string_dup (U_("Task information"));
        case CAL_COMPONENT_JOURNAL:
                return CORBA_string_dup (U_("Journal information"));
        case CAL_COMPONENT_FREEBUSY:
                cal_component_get_dtstart (comp, &dt);
                if (dt.value)
                        start = get_label (dt.value);
		cal_component_free_datetime (&dt);

		cal_component_get_dtend (comp, &dt);
		if (dt.value)
			end = get_label (dt.value);
		cal_component_free_datetime (&dt);

                if (start != NULL && end != NULL) {
                        char *tmp, *tmp_utf;
                        tmp = g_strdup_printf (_("Free/Busy information (%s to %s)"), start, end);
                        tmp_utf = e_utf8_from_locale_string (tmp);
                        description = CORBA_string_dup (tmp_utf);
                        g_free (tmp_utf);
                        g_free (tmp);
                } else {
                        description = CORBA_string_dup (U_("Free/Busy information"));
                }
                g_free (start);
                g_free (end);
                return description;
        default:
                return CORBA_string_dup (U_("iCalendar information"));
        }
}

static gboolean
comp_server_send (CalComponentItipMethod method, CalComponent *comp, CalClient *client, 
		  icalcomponent *zones, GList **users)
{
	CalClientSendResult result;
	icalcomponent *top_level, *new_top_level = NULL;
	char error_msg[256];
	gboolean retval = FALSE;
	
	top_level = comp_toplevel_with_zones (method, comp, client, zones);
	result = cal_client_send_object (client, top_level, &new_top_level, users, error_msg);

	if (result == CAL_CLIENT_SEND_SUCCESS) {
		icalcomponent *ical_comp;
		
		ical_comp = icalcomponent_get_inner (new_top_level);
		icalcomponent_remove_component (new_top_level, ical_comp);
		cal_component_set_icalcomponent (comp, ical_comp);
		icalcomponent_free (new_top_level);
		
		retval = TRUE;
	} else if (result == CAL_CLIENT_SEND_BUSY) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR, error_msg);

		retval = FALSE;
	}

	icalcomponent_free (top_level);

	return retval;
}

static gboolean
comp_limit_attendees (CalComponent *comp) 
{
	icalcomponent *icomp;
	GList *addresses;
	icalproperty *prop;
	gboolean found = FALSE, match = FALSE;
	GSList *l, *list = NULL;

	icomp = cal_component_get_icalcomponent (comp);
	addresses = itip_addresses_get ();	

	for (prop = icalcomponent_get_first_property (icomp, ICAL_ATTENDEE_PROPERTY);
	     prop != NULL;
	     prop = icalcomponent_get_next_property (icomp, ICAL_ATTENDEE_PROPERTY))
	{
		icalvalue *value;
		const char *attendee;
		char *text;
		GList *l;

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
		for (l = addresses; l != NULL; l = l->next) {
			ItipAddress *a = l->data;

			if (!g_strcasecmp (a->address, text))
				found = match = TRUE;
		}
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

	itip_addresses_free (addresses);

	return found;
}

static void
comp_sentby (CalComponent *comp, CalClient *client)
{
	CalComponentOrganizer organizer;
	
	cal_component_get_organizer (comp, &organizer);
	if (!organizer.value) {
		ItipAddress *a = itip_addresses_get_default ();

		organizer.value = g_strdup_printf ("MAILTO:%s", a->address);
		organizer.sentby = NULL;
		organizer.cn = a->name;
		organizer.language = NULL;
		
		cal_component_set_organizer (comp, &organizer);
		g_free ((char *) organizer.value);
		itip_address_free (a);
		
		return;
	}

	if (!itip_organizer_is_user (comp, client) && !itip_sentby_is_user (comp)) {
		ItipAddress *a = itip_addresses_get_default ();
		
		organizer.value = g_strdup (organizer.value);
		organizer.sentby = g_strdup_printf ("MAILTO:%s", a->address);
		organizer.cn = g_strdup (organizer.cn);
		organizer.language = g_strdup (organizer.language);
		
		cal_component_set_organizer (comp, &organizer);

		g_free ((char *)organizer.value);
		g_free ((char *)organizer.sentby);
		g_free ((char *)organizer.cn);
		g_free ((char *)organizer.language);
		itip_address_free (a);
	}
}
static CalComponent *
comp_minimal (CalComponent *comp, gboolean attendee)
{
	CalComponent *clone;
	icalcomponent *icomp, *icomp_clone;
	icalproperty *prop;
	CalComponentOrganizer organizer;
	const char *uid;
	GSList *comments;
	struct icaltimetype itt;
	CalComponentRange recur_id;
	
	clone = cal_component_new ();
	cal_component_set_new_vtype (clone, cal_component_get_vtype (comp));

	if (attendee) {
		GSList *attendees;
		
		cal_component_get_attendee_list (comp, &attendees);
		cal_component_set_attendee_list (clone, attendees);

		if (!comp_limit_attendees (clone)) {
			e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
				  _("You must be an attendee of the event."));
			goto error;
		}
	}
	
	itt = icaltime_from_timet_with_zone (time (NULL), FALSE,
					     icaltimezone_get_utc_timezone ());
	cal_component_set_dtstamp (clone, &itt);

	cal_component_get_organizer (comp, &organizer);
	if (organizer.value == NULL)
		goto error;
	cal_component_set_organizer (clone, &organizer);

	cal_component_get_uid (comp, &uid);
	cal_component_set_uid (clone, uid);

	cal_component_get_comment_list (comp, &comments);
	if (g_slist_length (comments) <= 1) {
		cal_component_set_comment_list (clone, comments);
	} else {
		GSList *l = comments;
		
		comments = g_slist_remove_link (comments, l);
		cal_component_set_comment_list (clone, l);
		cal_component_free_text_list (l);
	}
	cal_component_free_text_list (comments);
	
	cal_component_get_recurid (comp, &recur_id);
	if (recur_id.datetime.value != NULL)
		cal_component_set_recurid (clone, &recur_id);
	
	icomp = cal_component_get_icalcomponent (comp);
	icomp_clone = cal_component_get_icalcomponent (clone);
	for (prop = icalcomponent_get_first_property (icomp, ICAL_X_PROPERTY);
	     prop != NULL;
	     prop = icalcomponent_get_next_property (icomp, ICAL_X_PROPERTY))
	{
		icalproperty *p;
		
		p = icalproperty_new_clone (prop);
		icalcomponent_add_property (icomp_clone, p);
	}

	cal_component_rescan (clone);
	
	return clone;

 error:
	gtk_object_unref (GTK_OBJECT (clone));
	return NULL;
}

static CalComponent *
comp_compliant (CalComponentItipMethod method, CalComponent *comp, CalClient *client, icalcomponent *zones)
{
	CalComponent *clone, *temp_clone;
	struct icaltimetype itt;
	
	clone = cal_component_clone (comp);
	itt = icaltime_from_timet_with_zone (time (NULL), FALSE,
					     icaltimezone_get_utc_timezone ());
	cal_component_set_dtstamp (clone, &itt);

	/* Make UNTIL date a datetime in a simple recurrence */
	if (cal_component_has_recurrences (clone)
	    && cal_component_has_simple_recurrence (clone)) {
		GSList *rrule_list;
		struct icalrecurrencetype *r;
		
		cal_component_get_rrule_list (clone, &rrule_list);
		r = rrule_list->data;

		if (!icaltime_is_null_time (r->until) && r->until.is_date) {
			CalComponentDateTime dt;
			icaltimezone *from_zone = NULL, *to_zone;
			
			cal_component_get_dtstart (clone, &dt);

			if (dt.value->is_date) {
				from_zone = icaltimezone_get_builtin_timezone (calendar_config_get_timezone ());
			} else if (dt.tzid == NULL) {
				from_zone = icaltimezone_get_utc_timezone ();
			} else {
				if (zones != NULL)
					from_zone = icalcomponent_get_timezone (zones, dt.tzid);
				if (from_zone == NULL)
					from_zone = icaltimezone_get_builtin_timezone_from_tzid (dt.tzid);
				if (from_zone == NULL && client != NULL)
					cal_client_get_timezone (client, dt.tzid, &from_zone);
			}
			
			to_zone = icaltimezone_get_utc_timezone ();

			r->until.hour = dt.value->hour;
			r->until.minute = dt.value->minute;
			r->until.second = dt.value->second;
			r->until.is_date = FALSE;
			
			icaltimezone_convert_time (&r->until, from_zone, to_zone);
			r->until.is_utc = TRUE;

			cal_component_set_rrule_list (clone, rrule_list);
			cal_component_abort_sequence (clone);
		}

		cal_component_free_recur_list (rrule_list);
	}
	
	/* We delete incoming alarms anyhow, and this helps with outlook */
	cal_component_remove_all_alarms (clone);

	/* Strip X-LIC-ERROR stuff */
	cal_component_strip_errors (clone);
	
	/* Comply with itip spec */
	switch (method) {
	case CAL_COMPONENT_METHOD_PUBLISH:
		comp_sentby (clone, client);
		cal_component_set_attendee_list (clone, NULL);
		break;
	case CAL_COMPONENT_METHOD_REQUEST:
		comp_sentby (clone, client);
		break;
	case CAL_COMPONENT_METHOD_CANCEL:
		comp_sentby (clone, client);
		break;	
	case CAL_COMPONENT_METHOD_REPLY:
		break;
	case CAL_COMPONENT_METHOD_ADD:
		break;
	case CAL_COMPONENT_METHOD_REFRESH:
		/* Need to remove almost everything */
		temp_clone = comp_minimal (clone, TRUE);
		gtk_object_unref (GTK_OBJECT (clone));
		clone = temp_clone;
		break;
	case CAL_COMPONENT_METHOD_COUNTER:
		break;
	case CAL_COMPONENT_METHOD_DECLINECOUNTER:
		/* Need to remove almost everything */
		temp_clone = comp_minimal (clone, FALSE);
		gtk_object_unref (GTK_OBJECT (clone));
		clone = temp_clone;
		break;
	default:
		break;
	}

	return clone;
}

gboolean
itip_send_comp (CalComponentItipMethod method, CalComponent *send_comp,
		CalClient *client, icalcomponent *zones)
{
	BonoboObjectClient *bonobo_server;
	GNOME_Evolution_Composer composer_server;
	CalComponent *comp = NULL;
	icalcomponent *top_level = NULL;
	GList *users;
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

	/* Obtain an object reference for the Composer. */
	bonobo_server = bonobo_object_activate (GNOME_EVOLUTION_COMPOSER_OAFIID, 0);
	g_return_val_if_fail (bonobo_server != NULL, FALSE);
	composer_server = BONOBO_OBJREF (bonobo_server);
	
	/* Give the server a chance to manipulate the comp */
	if (method != CAL_COMPONENT_METHOD_PUBLISH) {
		if (!comp_server_send (method, send_comp, client, zones, &users))
			goto cleanup;
	}
	
	/* Tidy up the comp */
	comp = comp_compliant (method, send_comp, client, zones);
	if (comp == NULL)
		goto cleanup;

	/* Recipients */
	to_list = comp_to_list (method, comp, users);
	if (method != CAL_COMPONENT_METHOD_PUBLISH) {
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

	/* Set recipients, subject */
	GNOME_Evolution_Composer_setHeaders (composer_server, from, to_list, cc_list, bcc_list, subject, &ev);
	if (BONOBO_EX (&ev)) {		
		g_warning ("Unable to set composer headers while sending iTip message");
		goto cleanup;
	}


	/* Content type */
	content_type = comp_content_type (comp, method);

	top_level = comp_toplevel_with_zones (method, comp, client, zones);
	ical_string = icalcomponent_as_ical_string (top_level);

	if (cal_component_get_vtype (comp) == CAL_COMPONENT_EVENT) {
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
		attach_data->_length = strlen (ical_string) + 1;
		attach_data->_maximum = attach_data->_length;	
		attach_data->_buffer = CORBA_sequence_CORBA_char_allocbuf (attach_data->_length);
		strcpy (attach_data->_buffer, ical_string);

		GNOME_Evolution_Composer_attachData (composer_server,
						     content_type, filename, description,
						     TRUE, attach_data,
						     &ev);
	}
		
	if (BONOBO_EX (&ev)) {
		g_warning ("Unable to place iTip message in composer");
		goto cleanup;
	}
	
	if (method == CAL_COMPONENT_METHOD_PUBLISH) {
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
		gtk_object_unref (GTK_OBJECT (comp));
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

