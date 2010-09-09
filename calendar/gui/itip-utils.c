/*
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
 *		JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <libedataserver/e-time-utils.h>
#include <gtk/gtk.h>
#include <libical/ical.h>
#include <e-util/e-dialog-utils.h>
#include <libecal/e-cal-time-util.h>
#include <libecal/e-cal-util.h>
#include <libsoup/soup.h>
#include "calendar-config.h"
#include "itip-utils.h"
#include <time.h>
#include "dialogs/comp-editor-util.h"

#include <composer/e-msg-composer.h>

static const gchar *itip_methods[] = {
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
itip_organizer_is_user_ex (ECalComponent *comp, ECal *client, gboolean skip_cap_test)
{
	ECalComponentOrganizer organizer;
	const gchar *strip;
	gboolean user_org = FALSE;

	if (!e_cal_component_has_organizer (comp) || (!skip_cap_test && e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_NO_ORGANIZER)))
		return FALSE;

	e_cal_component_get_organizer (comp, &organizer);
	if (organizer.value != NULL) {

		strip = itip_strip_mailto (organizer.value);

		if (e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_ORGANIZER_NOT_EMAIL_ADDRESS)) {
			gchar *email = NULL;

			if (e_cal_get_cal_address (client, &email, NULL) && !g_ascii_strcasecmp (email, strip)) {
				g_free (email);

				return TRUE;
			}

			g_free (email);
			return FALSE;
		}

		user_org = e_account_list_find(itip_addresses_get(), E_ACCOUNT_FIND_ID_ADDRESS, strip) != NULL;
	}

	return user_org;
}

gboolean
itip_organizer_is_user (ECalComponent *comp, ECal *client)
{
	return itip_organizer_is_user_ex (comp, client, FALSE);
}

gboolean
itip_sentby_is_user (ECalComponent *comp, ECal *client)
{
	ECalComponentOrganizer organizer;
	const gchar *strip;
	gboolean user_sentby = FALSE;

	if (!e_cal_component_has_organizer (comp) ||e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_NO_ORGANIZER))
		return FALSE;

	e_cal_component_get_organizer (comp, &organizer);
	if (organizer.sentby != NULL) {
		strip = itip_strip_mailto (organizer.sentby);
		user_sentby = e_account_list_find(itip_addresses_get(), E_ACCOUNT_FIND_ID_ADDRESS, strip) != NULL;
	}

	return user_sentby;
}

static ECalComponentAttendee *
get_attendee (GSList *attendees, gchar *address)
{
	GSList *l;

	if (!address)
		return NULL;

	for (l = attendees; l; l = l->next) {
		ECalComponentAttendee *attendee = l->data;

		if (!g_ascii_strcasecmp (itip_strip_mailto (attendee->value), address)) {
			return attendee;
		}
	}

	return NULL;
}

static ECalComponentAttendee *
get_attendee_if_attendee_sentby_is_user (GSList *attendees, gchar *address)
{
	GSList *l;

	for (l = attendees; l; l = l->next) {
		ECalComponentAttendee *attendee = l->data;

		if (attendee->sentby && g_str_equal (itip_strip_mailto (attendee->sentby), address)) {
			return attendee;
		}
	}

	return NULL;
}

static gchar *
html_new_lines_for (const gchar *string)
{
	gchar **lines;
	gchar *joined;

	lines = g_strsplit_set (string, "\n", -1);
	joined = g_strjoinv ("<br>", lines);
	g_strfreev (lines);

	return joined;
}

gchar *
itip_get_comp_attendee (ECalComponent *comp, ECal *client)
{
	GSList *attendees;
	EAccountList *al;
	EAccount *a;
	EIterator *it;
	ECalComponentAttendee *attendee = NULL;
	gchar *address = NULL;

	e_cal_component_get_attendee_list (comp, &attendees);
	al = itip_addresses_get ();

	if (client)
		e_cal_get_cal_address (client, &address, NULL);

	if (address && *address) {
		attendee = get_attendee (attendees, address);

		if (attendee) {
			gchar *user_email = g_strdup (itip_strip_mailto (attendee->value));

			e_cal_component_free_attendee_list (attendees);
			g_free (address);
			return user_email;
		}

		attendee = get_attendee_if_attendee_sentby_is_user (attendees, address);

		if (attendee) {
			gchar *user_email = g_strdup (itip_strip_mailto (attendee->sentby));

			e_cal_component_free_attendee_list (attendees);
			g_free (address);
			return user_email;
		}

		g_free (address);
		address = NULL;
	}

	for (it = e_list_get_iterator((EList *)al);
			e_iterator_is_valid(it);
			e_iterator_next(it)) {
		a = (EAccount *) e_iterator_get(it);

		if (!a->enabled)
			continue;

		attendee = get_attendee (attendees, a->id->address);
		if (attendee) {
			gchar *user_email = g_strdup (itip_strip_mailto (attendee->value));

			e_cal_component_free_attendee_list (attendees);
			return user_email;
		}

		/* If the account was not found in the attendees list, then let's
		check the 'sentby' fields of the attendees if we can find the account */
		attendee = get_attendee_if_attendee_sentby_is_user (attendees, a->id->address);
		if (attendee) {
			gchar *user_email = g_strdup (itip_strip_mailto (attendee->sentby));

			e_cal_component_free_attendee_list (attendees);
			return user_email;
		}
	}

	/* We could not find the attendee in the component, so just give the default
	account address if the email address is not set in the backend */
	/* FIXME do we have a better way ? */
	a = itip_addresses_get_default ();
	address = g_strdup ((a != NULL) ? a->id->address : "");

	e_cal_component_free_attendee_list (attendees);
	return address;
}

const gchar *
itip_strip_mailto (const gchar *address)
{
	if (address == NULL)
		return NULL;

	if (!g_ascii_strncasecmp (address, "mailto:", 7))
		address += 7;

	return address;
}

static gchar *
get_label (struct icaltimetype *tt)
{
        gchar buffer[1000];
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
	const gchar *tzid;
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
	g_hash_table_insert (tz_data->tzids, (gchar *)tzid, (gchar *)tzid);
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
users_has_attendee (GList *users, const gchar *address)
{
	GList *l;

	for (l = users; l != NULL; l = l->next) {
		if (!g_ascii_strcasecmp (address, l->data))
			return TRUE;
	}

	return FALSE;
}

static gchar *
comp_from (ECalComponentItipMethod method, ECalComponent *comp)
{
	ECalComponentOrganizer organizer;
	ECalComponentAttendee *attendee;
	GSList *attendees;
	gchar *from;
	gchar *sender = NULL;

	switch (method) {
	case E_CAL_COMPONENT_METHOD_PUBLISH:
		return NULL;

	case E_CAL_COMPONENT_METHOD_REQUEST:
		return itip_get_comp_attendee (comp, NULL);

	case E_CAL_COMPONENT_METHOD_REPLY:
		sender = itip_get_comp_attendee (comp, NULL);
		if (sender != NULL)
			return sender;
		if (!e_cal_component_has_attendees (comp))
			return NULL;

	case E_CAL_COMPONENT_METHOD_CANCEL:

	case E_CAL_COMPONENT_METHOD_ADD:

		e_cal_component_get_organizer (comp, &organizer);
		if (organizer.value == NULL) {
			e_notice (NULL, GTK_MESSAGE_ERROR,
				  _("An organizer must be set."));
			return NULL;
		}
		return g_strdup (itip_strip_mailto (organizer.value));

	default:
		if (!e_cal_component_has_attendees (comp))
			return NULL;

		e_cal_component_get_attendee_list (comp, &attendees);
		attendee = attendees->data;
		if (attendee->value != NULL)
			from = g_strdup (itip_strip_mailto (attendee->value));
		else
			from = NULL;
		e_cal_component_free_attendee_list (attendees);

		return from;
	}
}

static EDestination **
comp_to_list (ECalComponentItipMethod method, ECalComponent *comp, GList *users, gboolean reply_all, const GSList *only_attendees)
{
	ECalComponentOrganizer organizer;
	GSList *attendees, *l;
	GPtrArray *array = NULL;
	EDestination *destination;
	gint len;
	gchar *sender = NULL;

	union {
		gpointer *pdata;
		EDestination **destinations;
	} convert;

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

		e_cal_component_get_organizer (comp, &organizer);
		if (organizer.value == NULL) {
			e_notice (NULL, GTK_MESSAGE_ERROR,
				  _("An organizer must be set."));
			return NULL;
		}

		array = g_ptr_array_new ();

		sender = itip_get_comp_attendee (comp, NULL);

		for (l = attendees; l != NULL; l = l->next) {
			ECalComponentAttendee *att = l->data;

			if (att->cutype != ICAL_CUTYPE_INDIVIDUAL && att->cutype != ICAL_CUTYPE_GROUP)
				continue;
			else if (users_has_attendee (users, att->value))
				continue;
			else if (att->sentby && users_has_attendee (users, att->sentby))
				continue;
			else if (!g_ascii_strcasecmp (att->value, organizer.value))
				continue;
			else if (att->sentby && !g_ascii_strcasecmp (att->sentby, organizer.sentby))
				continue;
			else if (!g_ascii_strcasecmp (itip_strip_mailto (att->value), sender))
				continue;
			else if (att->status == ICAL_PARTSTAT_DELEGATED && (att->delto && *att->delto)
					&& !(att->rsvp) && method == E_CAL_COMPONENT_METHOD_REQUEST)
				continue;
			else if (only_attendees && !comp_editor_have_in_new_attendees_lst (only_attendees, itip_strip_mailto (att->value)))
				continue;

			destination = e_destination_new ();
			if (att->cn != NULL)
				e_destination_set_name (destination, att->cn);
			e_destination_set_email (
				destination, itip_strip_mailto (att->value));
			g_ptr_array_add (array, destination);
		}
		g_free (sender);
		e_cal_component_free_attendee_list (attendees);
		break;

	case E_CAL_COMPONENT_METHOD_REPLY:

		if (reply_all) {
			e_cal_component_get_attendee_list (comp, &attendees);
			len = g_slist_length (attendees);

			if (len <= 0)
				return NULL;

			array = g_ptr_array_new ();

			e_cal_component_get_organizer (comp, &organizer);
			sender = itip_get_comp_attendee (comp, NULL);

			for (l = attendees; l != NULL; l = l->next) {
				ECalComponentAttendee *att = l->data;

				if (att->cutype != ICAL_CUTYPE_INDIVIDUAL && att->cutype != ICAL_CUTYPE_GROUP)
					continue;
				else if (only_attendees && !comp_editor_have_in_new_attendees_lst (only_attendees, itip_strip_mailto (att->value)))
					continue;

				destination = e_destination_new ();
				if (att->cn != NULL)
					e_destination_set_name (destination, att->cn);
				e_destination_set_email (
					destination, itip_strip_mailto (att->value));
				g_ptr_array_add (array, destination);
			}

			g_free (sender);
			e_cal_component_free_attendee_list (attendees);

		} else {
			array = g_ptr_array_new ();

			destination = e_destination_new ();
			e_cal_component_get_organizer (comp, &organizer);
			if (organizer.value)
				e_destination_set_email (
					destination, itip_strip_mailto (organizer.value));
			g_ptr_array_add (array, destination);
		}
		break;

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

		array = g_ptr_array_new ();

		destination = e_destination_new ();
		if (organizer.cn != NULL)
			e_destination_set_name (destination, organizer.cn);
		e_destination_set_email (
			destination, itip_strip_mailto (organizer.value));
		g_ptr_array_add (array, destination);

		/* send the status to delegatee to the delegate also*/
		e_cal_component_get_attendee_list (comp, &attendees);
		sender = itip_get_comp_attendee (comp, NULL);

		for (l = attendees; l != NULL; l = l->next) {
			ECalComponentAttendee *att = l->data;

			if (att->cutype != ICAL_CUTYPE_INDIVIDUAL && att->cutype != ICAL_CUTYPE_GROUP)
				continue;

			if (!g_ascii_strcasecmp (itip_strip_mailto (att->value), sender) || (att->sentby && !g_ascii_strcasecmp (itip_strip_mailto (att->sentby), sender))) {

				if (!(att->delfrom && *att->delfrom))
					break;

				destination = e_destination_new ();
				e_destination_set_email (
					destination, itip_strip_mailto (att->delfrom));
				g_ptr_array_add (array, destination);
			}

		}
		e_cal_component_free_attendee_list (attendees);

		break;
	case E_CAL_COMPONENT_METHOD_PUBLISH:
		if (users) {
			GList *list;

			array = g_ptr_array_new ();

			for (list = users; list != NULL; list = list->next) {
				destination = e_destination_new ();
				e_destination_set_email (destination, list->data);
				g_ptr_array_add (array, destination);
			}

			break;
		}
	default:
		break;
	}

	if (array == NULL)
		return NULL;

	g_ptr_array_add (array, NULL);
	convert.pdata = g_ptr_array_free (array, FALSE);

	return convert.destinations;
}

static gchar *
comp_subject (ECalComponentItipMethod method, ECalComponent *comp)
{
	ECalComponentText caltext;
	const gchar *description, *prefix = NULL;
	GSList *alist, *l;
	gchar *subject;
	gchar *sender;
	ECalComponentAttendee *a = NULL;

	e_cal_component_get_summary (comp, &caltext);
	if (caltext.value != NULL)
		description = caltext.value;
	else {
		switch (e_cal_component_get_vtype (comp)) {
		case E_CAL_COMPONENT_EVENT:
			description = _("Event information");
			break;
		case E_CAL_COMPONENT_TODO:
			description = _("Task information");
			break;
		case E_CAL_COMPONENT_JOURNAL:
			description = _("Memo information");
			break;
		case E_CAL_COMPONENT_FREEBUSY:
			description = _("Free/Busy information");
			break;
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
		sender = itip_get_comp_attendee (comp, NULL);
		if (sender) {

			for (l = alist; l != NULL; l = l->next) {
				a = l->data;
				if ((sender && *sender) && (g_ascii_strcasecmp (itip_strip_mailto (a->value), sender) || (a->sentby && g_ascii_strcasecmp (itip_strip_mailto (a->sentby), sender))))
					break;
			}
			g_free (sender);
		}

		if (alist != NULL) {

			switch (a->status) {
			case ICAL_PARTSTAT_ACCEPTED:
				/* Translators: This is part of the subject
				 * line of a meeting request or update email.
				 * The full subject line would be:
				 * "Accepted: Meeting Name". */
				prefix = C_("Meeting", "Accepted");
				break;
			case ICAL_PARTSTAT_TENTATIVE:
				/* Translators: This is part of the subject
				 * line of a meeting request or update email.
				 * The full subject line would be:
				 * "Tentatively Accepted: Meeting Name". */
				prefix = C_("Meeting", "Tentatively Accepted");
				break;
			case ICAL_PARTSTAT_DECLINED:
				/* Translators: This is part of the subject
				 * line of a meeting request or update email.
				 * The full subject line would be:
				 * "Declined: Meeting Name". */
				prefix = C_("Meeting", "Declined");
				break;
			case ICAL_PARTSTAT_DELEGATED:
				/* Translators: This is part of the subject
				 * line of a meeting request or update email.
				 * The full subject line would be:
				 * "Delegated: Meeting Name". */
				prefix = C_("Meeting", "Delegated");
				break;
			default:
				break;
			}
			e_cal_component_free_attendee_list (alist);
		}
		break;

	case E_CAL_COMPONENT_METHOD_ADD:
		/* Translators: This is part of the subject line of a
		 * meeting request or update email.  The full subject
		 * line would be: "Updated: Meeting Name". */
		prefix = C_("Meeting", "Updated");
		break;

	case E_CAL_COMPONENT_METHOD_CANCEL:
		/* Translators: This is part of the subject line of a
		 * meeting request or update email.  The full subject
		 * line would be: "Cancel: Meeting Name". */
		prefix = C_("Meeting", "Cancel");
		break;

	case E_CAL_COMPONENT_METHOD_REFRESH:
		/* Translators: This is part of the subject line of a
		 * meeting request or update email.  The full subject
		 * line would be: "Refresh: Meeting Name". */
		prefix = C_("Meeting", "Refresh");
		break;

	case E_CAL_COMPONENT_METHOD_COUNTER:
		/* Translators: This is part of the subject line of a
		 * meeting request or update email.  The full subject
		 * line would be: "Counter-proposal: Meeting Name". */
		prefix = C_("Meeting", "Counter-proposal");
		break;

	case E_CAL_COMPONENT_METHOD_DECLINECOUNTER:
		/* Translators: This is part of the subject line of a
		 * meeting request or update email.  The full subject
		 * line would be: "Declined: Meeting Name". */
		prefix = C_("Meeting", "Declined");
		break;

	default:
		break;
	}

	if (prefix != NULL)
		subject = g_strdup_printf ("%s: %s", prefix, description);
	else
		subject = g_strdup (description);

	return subject;
}

static gchar *
comp_content_type (ECalComponent *comp, ECalComponentItipMethod method)
{
	return g_strdup_printf (
		"text/calendar; name=\"%s\"; charset=utf-8; METHOD=%s",
		e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_FREEBUSY ?
		"freebusy.ifb" : "calendar.ics", itip_methods[method]);
}

static const gchar *
comp_filename (ECalComponent *comp)
{
	if (e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_FREEBUSY)
		return "freebusy.ifb";
	else
		return "calendar.ics";
}

static gchar *
comp_description (ECalComponent *comp)
{
	gchar *description;
        ECalComponentDateTime dt;
        gchar *start = NULL, *end = NULL;

        switch (e_cal_component_get_vtype (comp)) {
        case E_CAL_COMPONENT_EVENT:
                description = g_strdup (_("Event information"));
		break;
        case E_CAL_COMPONENT_TODO:
                description = g_strdup (_("Task information"));
		break;
        case E_CAL_COMPONENT_JOURNAL:
                description = g_strdup (_("Memo information"));
		break;
        case E_CAL_COMPONENT_FREEBUSY:
                e_cal_component_get_dtstart (comp, &dt);
                if (dt.value)
                        start = get_label (dt.value);
		e_cal_component_free_datetime (&dt);

		e_cal_component_get_dtend (comp, &dt);
		if (dt.value)
			end = get_label (dt.value);
		e_cal_component_free_datetime (&dt);

                if (start != NULL && end != NULL)
			description = g_strdup_printf (
				_("Free/Busy information (%s to %s)"),
				start, end);
                else
			description = g_strdup (_("Free/Busy information"));
                g_free (start);
                g_free (end);
		break;
        default:
                description = g_strdup (_("iCalendar information"));
		break;
        }

	return description;
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
			if (error->code == E_CALENDAR_STATUS_OBJECT_ID_ALREADY_EXISTS) {
				e_notice (NULL, GTK_MESSAGE_ERROR, _("Unable to book a resource, the new event collides with some other."));
			} else {
				gchar *msg = g_strconcat (_("Unable to book a resource, error: "), error->message, NULL);
				e_notice (NULL, GTK_MESSAGE_ERROR, msg);
				g_free (msg);
			}

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
		gchar *attendee;
		gchar *attendee_text;
		icalparameter *param;
		const gchar *attendee_sentby;
		gchar *attendee_sentby_text = NULL;

		/* If we've already found something, just erase the rest */
		if (found) {
			list = g_slist_prepend (list, prop);
			continue;
		}

		attendee = icalproperty_get_value_as_string_r (prop);
		if (!attendee)
			continue;

		attendee_text = g_strdup (itip_strip_mailto (attendee));
		g_free (attendee);
		attendee_text = g_strstrip (attendee_text);
		found = match = e_account_list_find(itip_addresses_get(), E_ACCOUNT_FIND_ID_ADDRESS, attendee_text) != NULL;

		if (!found) {
			param = icalproperty_get_first_parameter (prop, ICAL_SENTBY_PARAMETER);
			if (param) {
				attendee_sentby = icalparameter_get_sentby (param);
				attendee_sentby_text = g_strdup (itip_strip_mailto (attendee_sentby));
				attendee_sentby_text = g_strstrip (attendee_sentby_text);
				found = match = e_account_list_find(itip_addresses_get(), E_ACCOUNT_FIND_ID_ADDRESS, attendee_sentby_text) != NULL;
			}
		}

		g_free(attendee_text);
		g_free (attendee_sentby_text);

		if (!match)
			list = g_slist_prepend (list, prop);
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
	GSList * attendees, *l;
	gchar *user = NULL;

	e_cal_component_get_organizer (comp, &organizer);
	if (!organizer.value) {
		EAccount *a = itip_addresses_get_default ();

		organizer.value = g_strdup_printf ("MAILTO:%s", a->id->address);
		organizer.sentby = NULL;
		organizer.cn = a->id->name;
		organizer.language = NULL;

		e_cal_component_set_organizer (comp, &organizer);
		g_free ((gchar *) organizer.value);

		return;
	}

	e_cal_component_get_attendee_list (comp, &attendees);
	user = itip_get_comp_attendee (comp, client);
	for (l = attendees; l; l = l->next) {
		ECalComponentAttendee *a = l->data;

		if (!g_ascii_strcasecmp (itip_strip_mailto (a->value), user) || (a->sentby && !g_ascii_strcasecmp (itip_strip_mailto (a->sentby), user))) {
			g_free (user);
			return;
		}
	}

	if (!itip_organizer_is_user (comp, client) && !itip_sentby_is_user (comp, client)) {
		EAccount *a = itip_addresses_get_default ();

		organizer.value = g_strdup (organizer.value);
		organizer.sentby = g_strdup_printf ("MAILTO:%s", a->id->address);
		organizer.cn = g_strdup (organizer.cn);
		organizer.language = g_strdup (organizer.language);

		e_cal_component_set_organizer (comp, &organizer);

		g_free ((gchar *)organizer.value);
		g_free ((gchar *)organizer.sentby);
		g_free ((gchar *)organizer.cn);
		g_free ((gchar *)organizer.language);
	}
}
static ECalComponent *
comp_minimal (ECalComponent *comp, gboolean attendee)
{
	ECalComponent *clone;
	icalcomponent *icomp, *icomp_clone;
	icalproperty *prop;
	ECalComponentOrganizer organizer;
	const gchar *uid;
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

static void
strip_x_microsoft_props (ECalComponent *comp)
{
	GSList *lst = NULL, *l;
	icalcomponent *icalcomp;
	icalproperty *icalprop;

	g_return_if_fail (comp != NULL);

	icalcomp = e_cal_component_get_icalcomponent (comp);
	g_return_if_fail (icalcomp != NULL);

	for (icalprop = icalcomponent_get_first_property (icalcomp, ICAL_X_PROPERTY);
	     icalprop;
	     icalprop = icalcomponent_get_next_property (icalcomp, ICAL_X_PROPERTY)) {
		const gchar *x_name = icalproperty_get_x_name (icalprop);

		if (x_name && g_ascii_strncasecmp (x_name, "X-MICROSOFT-", 12) == 0)
			lst = g_slist_prepend (lst, icalprop);
	}

	for (l = lst; l != NULL; l = l->next) {
		icalprop = l->data;
		icalcomponent_remove_property (icalcomp, icalprop);
		icalproperty_free (icalprop);
	}

	g_slist_free (lst);
}

static ECalComponent *
comp_compliant (ECalComponentItipMethod method, ECalComponent *comp, ECal *client, icalcomponent *zones, gboolean strip_alarms)
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

			e_cal_component_free_datetime (&dt);
			e_cal_component_set_rrule_list (clone, rrule_list);
			e_cal_component_abort_sequence (clone);
		}

		e_cal_component_free_recur_list (rrule_list);
	}

	/* We delete incoming alarms if requested, even this helps with outlook */
	if (strip_alarms) {
		e_cal_component_remove_all_alarms (clone);
	} else {
		/* Always strip procedure alarms, because of security */
		GList *uids, *l;

		uids = e_cal_component_get_alarm_uids (clone);

		for (l = uids; l; l = l->next) {
			ECalComponentAlarm *alarm;
			ECalComponentAlarmAction action = E_CAL_COMPONENT_ALARM_UNKNOWN;

			alarm = e_cal_component_get_alarm (clone, (const gchar *)l->data);
			if (alarm) {
				e_cal_component_alarm_get_action (alarm, &action);
				e_cal_component_alarm_free (alarm);

				if (action == E_CAL_COMPONENT_ALARM_PROCEDURE)
					e_cal_component_remove_alarm (clone, (const gchar *)l->data);
			}
		}

		cal_obj_uid_list_free (uids);
	}

	strip_x_microsoft_props (clone);

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

static void
append_cal_attachments (EMsgComposer *composer,
                        ECalComponent *comp,
                        GSList *attach_list)
{
	struct CalMimeAttach *mime_attach;
	GSList *l;

	for (l = attach_list; l; l = l->next) {
		CamelMimePart *attachment;

		mime_attach = (struct CalMimeAttach *) l->data;

		attachment = camel_mime_part_new ();
		camel_mime_part_set_content (
			attachment, mime_attach->encoded_data,
			mime_attach->length, mime_attach->content_type);
		if (mime_attach->content_id)
			camel_mime_part_set_content_id (attachment, mime_attach->content_id);
		if (mime_attach->filename != NULL)
			camel_mime_part_set_filename (
				attachment, mime_attach->filename);
		if (mime_attach->description != NULL)
			camel_mime_part_set_description (
				attachment, mime_attach->description);
		if (mime_attach->disposition)
			camel_mime_part_set_disposition (
				attachment, "inline");
		else
			camel_mime_part_set_disposition (
				attachment, "attachment");
		e_msg_composer_attach (composer, attachment);
		g_object_unref (attachment);

		g_free (mime_attach->filename);
		g_free (mime_attach->content_type);
		g_free (mime_attach->content_id);
		g_free (mime_attach->description);
		g_free (mime_attach->encoded_data);
		g_free (mime_attach);
	}

	g_slist_free (attach_list);
}

static EAccount *
find_enabled_account (EAccountList *accounts, const gchar *id_address)
{
	EIterator *it;
	EAccount *account = NULL;

	g_return_val_if_fail (accounts != NULL, NULL);

	if (!id_address)
		return NULL;

	for (it = e_list_get_iterator ((EList *)accounts);
	     e_iterator_is_valid (it);
	     e_iterator_next (it)) {
		account = (EAccount *)e_iterator_get (it);

		if (account
		    && account->enabled
		    && account->id
		    && account->id->address
		    && g_ascii_strcasecmp (account->id->address, id_address) == 0)
			break;

		account = NULL;
	}

	return account;
}

static void
setup_from (ECalComponentItipMethod method, ECalComponent *comp, ECal *client, EComposerHeaderTable *table)
{
	EAccountList *accounts;

	accounts = e_composer_header_table_get_account_list (table);
	if (accounts) {
		EAccount *account = NULL;

		/* always use organizer's email when user is an organizer */
		if (itip_organizer_is_user (comp, client)) {
			ECalComponentOrganizer organizer = {0};

			e_cal_component_get_organizer (comp, &organizer);
			if (organizer.value != NULL) {
				account = find_enabled_account (accounts, itip_strip_mailto (organizer.value));
			}
		}

		if (!account) {
			gchar *from = comp_from (method, comp);

			if (from)
				account = find_enabled_account (accounts, from);

			g_free (from);
		}

		if (account)
			e_composer_header_table_set_account (table, account);
	}
}

gboolean
itip_send_comp (ECalComponentItipMethod method, ECalComponent *send_comp,
		ECal *client, icalcomponent *zones, GSList *attachments_list, GList *users,
		gboolean strip_alarms, gboolean only_new_attendees)
{
	EShell *shell;
	EMsgComposer *composer;
	EComposerHeaderTable *table;
	EDestination **destinations;
	ECalComponent *comp = NULL;
	icalcomponent *top_level = NULL;
	gchar *ical_string = NULL;
	gchar *content_type = NULL;
	gchar *subject = NULL;
	gboolean retval = FALSE;

	/* check whether backend could handle auto-saving requests/updates */
	if (method != E_CAL_COMPONENT_METHOD_PUBLISH && e_cal_get_save_schedules (client))
		return TRUE;

	/* Give the server a chance to manipulate the comp */
	if (method != E_CAL_COMPONENT_METHOD_PUBLISH) {
		if (!comp_server_send (method, send_comp, client, zones, &users))
			goto cleanup;
	}

	/* check whether backend could handle sending requests/updates */
	if (method != E_CAL_COMPONENT_METHOD_PUBLISH && e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_CREATE_MESSAGES)) {
		if (users) {
			g_list_foreach (users, (GFunc) g_free, NULL);
			g_list_free (users);
		}
		return TRUE;
	}

	/* Tidy up the comp */
	comp = comp_compliant (method, send_comp, client, zones, strip_alarms);

	if (comp == NULL)
		goto cleanup;

	/* Recipients */
	destinations = comp_to_list (method, comp, users, FALSE, only_new_attendees ? g_object_get_data (G_OBJECT (send_comp), "new-attendees") : NULL);
	if (method != E_CAL_COMPONENT_METHOD_PUBLISH) {
		if (destinations == NULL) {
			/* We sent them all via the server */
			retval = TRUE;
			goto cleanup;
		}
	}

	/* Subject information */
	subject = comp_subject (method, comp);

	/* FIXME Pass this in. */
	shell = e_shell_get_default ();

	composer = e_msg_composer_new (shell);
	table = e_msg_composer_get_header_table (composer);

	setup_from (method, send_comp, client, table);
	e_composer_header_table_set_subject (table, subject);
	e_composer_header_table_set_destinations_to (table, destinations);

	e_destination_freev (destinations);

	/* Content type */
	content_type = comp_content_type (comp, method);

	top_level = comp_toplevel_with_zones (method, comp, client, zones);
	ical_string = icalcomponent_as_ical_string_r (top_level);

	if (e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_EVENT) {
		e_msg_composer_set_body (composer, ical_string, content_type);
	} else {
		CamelMimePart *attachment;
		const gchar *filename;
		gchar *description;
		gchar *body;

		filename = comp_filename (comp);
		description = comp_description (comp);

		body = camel_text_to_html (
			description, CAMEL_MIME_FILTER_TOHTML_PRE, 0);
		e_msg_composer_set_body_text (composer, body, -1);
		g_free (body);

		attachment = camel_mime_part_new ();
		camel_mime_part_set_content (
			attachment, ical_string,
			strlen (ical_string), content_type);
		if (filename != NULL && *filename != '\0')
			camel_mime_part_set_filename (attachment, filename);
		if (description != NULL && *description != '\0')
			camel_mime_part_set_description (attachment, description);
		camel_mime_part_set_disposition (attachment, "inline");
		e_msg_composer_attach (composer, attachment);
		g_object_unref (attachment);

		g_free (description);
	}

	append_cal_attachments (composer, comp, attachments_list);

	if ((method == E_CAL_COMPONENT_METHOD_PUBLISH) && !users)
		gtk_widget_show (GTK_WIDGET (composer));
	else
		e_msg_composer_send (composer);

	retval = TRUE;

 cleanup:
	if (comp != NULL)
		g_object_unref (comp);
	if (top_level != NULL)
		icalcomponent_free (top_level);

	if (users) {
		g_list_foreach (users, (GFunc) g_free, NULL);
		g_list_free (users);
	}

	g_free (content_type);
	g_free (subject);
	g_free (ical_string);

	return retval;
}

gboolean
reply_to_calendar_comp (ECalComponentItipMethod method,
                        ECalComponent *send_comp,
                        ECal *client,
                        gboolean reply_all,
                        icalcomponent *zones,
                        GSList *attachments_list)
{
	EShell *shell;
	EMsgComposer *composer;
	EComposerHeaderTable *table;
	EDestination **destinations;
	ECalComponent *comp = NULL;
	icalcomponent *top_level = NULL;
	GList *users = NULL;
	gchar *subject = NULL;
	gchar *ical_string = NULL;
	gboolean retval = FALSE;

	/* Tidy up the comp */
	comp = comp_compliant (method, send_comp, client, zones, TRUE);
	if (comp == NULL)
		goto cleanup;

	/* Recipients */
	destinations = comp_to_list (method, comp, users, reply_all, NULL);

	/* Subject information */
	subject = comp_subject (method, comp);

	/* FIXME Pass this in. */
	shell = e_shell_get_default ();

	composer = e_msg_composer_new (shell);
	table = e_msg_composer_get_header_table (composer);

	setup_from (method, send_comp, client, table);
	e_composer_header_table_set_subject (table, subject);
	e_composer_header_table_set_destinations_to (table, destinations);

	e_destination_freev (destinations);

	top_level = comp_toplevel_with_zones (method, comp, client, zones);
	ical_string = icalcomponent_as_ical_string_r (top_level);

	if (e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_EVENT) {

		GString *body;
		gchar *orig_from = NULL;
		const gchar *description = NULL;
		gchar *subject = NULL;
		const gchar *location = NULL;
		gchar *time = NULL;
		gchar *html_description = NULL;
		GSList *text_list = NULL;
		ECalComponentOrganizer organizer;
		ECalComponentText text;
		ECalComponentDateTime dtstart;
		icaltimezone *start_zone = NULL;
		time_t start;

		e_cal_component_get_description_list (comp, &text_list);

		if (text_list) {
			ECalComponentText text = *((ECalComponentText *)text_list->data);
			if (text.value)
				description = text.value;
			else
				description = "";
		} else {
			description = "";
		}

		e_cal_component_free_text_list (text_list);

		e_cal_component_get_summary (comp, &text);
		if (text.value)
			subject = g_strdup (text.value);

		e_cal_component_get_organizer (comp, &organizer);
		if (organizer.value)
			orig_from = g_strdup (itip_strip_mailto (organizer.value));

		e_cal_component_get_location (comp, &location);
		if (!location)
			location = "Unspecified";

		e_cal_component_get_dtstart (comp, &dtstart);
		if (dtstart.value) {
			start_zone = icaltimezone_get_builtin_timezone_from_tzid (dtstart.tzid);
			if (!start_zone) {
				if (!e_cal_get_timezone (client, dtstart.tzid, &start_zone, NULL))
					g_warning ("Couldn't get timezone from server: %s", dtstart.tzid ? dtstart.tzid : "");
			}

			if (!start_zone || dtstart.value->is_date)
				start_zone = calendar_config_get_icaltimezone ();

			start = icaltime_as_timet_with_zone (*dtstart.value, start_zone);
			time = g_strdup (ctime (&start));
		}

		body = g_string_new ("<br><br><hr><br><b>______ Original Appointment ______ </b><br><br><table>");

		if (orig_from && *orig_from)
			g_string_append_printf (body,
				"<tr><td><b>From</b></td>"
				"<td>:</td><td>%s</td></tr>", orig_from);
		g_free (orig_from);

		if (subject)
			g_string_append_printf (body,
				"<tr><td><b>Subject</b></td>"
				"<td>:</td><td>%s</td></tr>", subject);
		g_free (subject);

		g_string_append_printf (body,
			"<tr><td><b>Location</b></td>"
			"<td>:</td><td>%s</td></tr>", location);

		if (time)
			g_string_append_printf (body,
				"<tr><td><b>Time</b></td>"
				"<td>:</td><td>%s</td></tr>", time);
		g_free (time);

		g_string_append_printf (body, "</table><br>");

		html_description = html_new_lines_for (description);
		g_string_append (body, html_description);
		g_free (html_description);

		e_msg_composer_set_body_text (composer, body->str, -1);
		g_string_free (body, TRUE);
	}

	gtk_widget_show (GTK_WIDGET (composer));

	retval = TRUE;

 cleanup:

	if (comp != NULL)
		g_object_unref (comp);
	if (top_level != NULL)
		icalcomponent_free (top_level);

	if (users) {
		g_list_foreach (users, (GFunc) g_free, NULL);
		g_list_free (users);
	}

	g_free (subject);
	g_free (ical_string);
	return retval;
}

gboolean
itip_publish_begin (ECalComponent *pub_comp, ECal *client,
		    gboolean cloned, ECalComponent **clone)
{
	icalcomponent *icomp =NULL, *icomp_clone = NULL;
	icalproperty *prop;

	if (e_cal_component_get_vtype (pub_comp) == E_CAL_COMPONENT_FREEBUSY) {

		if (!cloned)
			*clone = e_cal_component_clone (pub_comp);
		else {

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
fb_sort (struct icalperiodtype *ipt, gint fb_count)
{
	gint i,j;

	if (ipt == NULL || fb_count == 0)
		return;

	for (i = 0; i < fb_count-1; i++) {
		for (j = i+1; j < fb_count; j++) {
			struct icalperiodtype temp;

			if (icaltime_compare (ipt[i].start, ipt[j].start) < 0)
				continue;

			if (icaltime_compare (ipt[i].start, ipt[j].start) == 0) {
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
	const gchar *uid,  *comment;
	struct icaltimetype itt;
	gint fb_count, i = 0, j;
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
	/* this will never be reached */
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
	SoupURI *real_uri;
	gchar *ical_string = NULL;

	toplevel = e_cal_util_new_top_level ();
	icalcomponent_set_method (toplevel, ICAL_METHOD_PUBLISH);

	e_cal_component_set_url (*pub_comp, uri);

	icalcomp = e_cal_component_get_icalcomponent (*pub_comp);

	icomp = comp_fb_normalize (icalcomp);

	icalcomponent_add_component (toplevel, icomp);

	/* Publish the component */
	session = soup_session_async_new ();

	real_uri = soup_uri_new (uri);
	if (!real_uri || !real_uri->host) {
		g_warning (G_STRLOC ": Invalid URL: %s", uri);
		g_object_unref (session);
		return FALSE;
	}

	soup_uri_set_user (real_uri, username);
	soup_uri_set_password (real_uri, password);

	/* build the message */
	msg = soup_message_new_from_uri (SOUP_METHOD_PUT, real_uri);
	soup_uri_free (real_uri);
	if (!msg) {
		g_warning (G_STRLOC ": Could not build SOAP message");
		g_object_unref (session);
		return FALSE;
	}

	soup_message_set_flags (msg, SOUP_MESSAGE_NO_REDIRECT);
	ical_string = icalcomponent_as_ical_string_r (toplevel);
	soup_message_set_request (msg, "text/calendar", SOUP_MEMORY_TEMPORARY,
				  ical_string, strlen (ical_string));

	/* send message to server */
	soup_session_send_message (session, msg);
	if (!SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
		g_warning(G_STRLOC ": Could not publish Free/Busy: %d: %s",
			  msg->status_code,
			  msg->reason_phrase);
		g_object_unref (msg);
		g_object_unref (session);
		g_free (ical_string);
		return FALSE;
	}

	g_object_unref (msg);
	g_object_unref (session);
	g_free (ical_string);

	return TRUE;
}

static gboolean
check_time (const struct icaltimetype tmval, gboolean can_null_time)
{
	if (icaltime_is_null_time (tmval))
		return can_null_time;

	return  icaltime_is_valid_time (tmval) &&
		tmval.month >= 1 && tmval.month <= 12 &&
		tmval.day >= 1 && tmval.day <= 31 &&
		tmval.hour >= 0 && tmval.hour < 24 &&
		tmval.minute >= 0 && tmval.minute < 60 &&
		tmval.second >= 0 && tmval.second < 60;
}

/* returns whether the passed-in icalcomponent is valid or not. It does some sanity checks on values too. */
gboolean
is_icalcomp_valid (icalcomponent *icalcomp)
{
	return  icalcomp &&
		icalcomponent_is_valid (icalcomp) &&
		check_time (icalcomponent_get_dtstart (icalcomp), FALSE) &&
		check_time (icalcomponent_get_dtend (icalcomp), TRUE);
}
