/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <time.h>
#include <glib/gi18n-lib.h>
#include <libical/ical.h>
#include <libsoup/soup.h>

#include <composer/e-msg-composer.h>

#include "calendar-config.h"
#include "comp-util.h"

#include "itip-utils.h"

#define d(x)

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

/**
 * itip_get_default_name_and_address:
 * @registry: an #ESourceRegistry
 * @name: return location for the user's real name, or %NULL
 * @address: return location for the user's email address, or %NULL
 *
 * Returns the real name and email address of the default mail identity,
 * if available.  If no default mail identity is available, @name and
 * @address are set to %NULL and the function returns %FALSE.
 *
 * Returns: %TRUE if @name and/or @address were set
 **/
gboolean
itip_get_default_name_and_address (ESourceRegistry *registry,
                                   gchar **name,
                                   gchar **address)
{
	ESource *source;
	ESourceExtension *extension;
	const gchar *extension_name;
	gboolean success;

	source = e_source_registry_ref_default_mail_identity (registry);

	if (source != NULL) {
		extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
		extension = e_source_get_extension (source, extension_name);

		if (name != NULL)
			*name = e_source_mail_identity_dup_name (
				E_SOURCE_MAIL_IDENTITY (extension));

		if (address != NULL)
			*address = e_source_mail_identity_dup_address (
				E_SOURCE_MAIL_IDENTITY (extension));

		g_object_unref (source);

		success = TRUE;

	} else {
		if (name != NULL)
			*name = NULL;

		if (address != NULL)
			*address = NULL;

		success = FALSE;
	}

	return success;
}

static gint
sort_identities_by_email_cb (gconstpointer ptr1,
			     gconstpointer ptr2)
{
	const gchar **pv1 = (const gchar **) ptr1, **pv2 = (const gchar **) ptr2;
	const gchar *addr1, *addr2;
	gint res;

	if (!pv1 || !*pv1 || !pv2 || !*pv2) {
		if (pv1 && *pv1)
			return -1;
		if (pv2 && *pv2)
			return 1;
		return 0;
	}

	addr1 = strchr (*pv1, '<');
	addr2 = strchr (*pv2, '<');

	if (addr1)
		addr1++;
	else
		addr1 = *pv1;
	if (addr2)
		addr2++;
	else
		addr2 = *pv2;

	res = g_ascii_strcasecmp (addr1, addr2);

	if (!res && addr1 != *pv1 && addr2 != *pv2)
		res = g_ascii_strcasecmp (*pv1, *pv2);

	return res;
}

/**
 * itip_get_user_identities:
 * @registry: an #ESourceRegistry
 *
 * Returns a %NULL-terminated array of name + address strings based on
 * registered mail identities.  Free the returned array with g_strfreev().
 *
 * Returns: an %NULL-terminated array of mail identity strings
 **/
gchar **
itip_get_user_identities (ESourceRegistry *registry)
{
	GList *list, *link;
	const gchar *extension_name;
	GPtrArray *identities;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;

	list = e_source_registry_list_enabled (registry, extension_name);

	identities = g_ptr_array_sized_new (g_list_length (list) + 1);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		ESourceMailIdentity *extension;
		const gchar *name, *address;
		gchar *aliases;

		extension = e_source_get_extension (source, extension_name);

		name = e_source_mail_identity_get_name (extension);
		address = e_source_mail_identity_get_address (extension);

		if (address) {
			if (name && *name)
				g_ptr_array_add (identities, g_strdup_printf ("%s <%s>", name, address));
			else
				g_ptr_array_add (identities, g_strdup_printf ("%s", address));
		}

		aliases = e_source_mail_identity_dup_aliases (extension);
		if (aliases && *aliases) {
			CamelInternetAddress *inet_address;
			gint ii, len;

			inet_address = camel_internet_address_new ();
			len = camel_address_decode (CAMEL_ADDRESS (inet_address), aliases);

			for (ii = 0; ii < len; ii++) {
				const gchar *alias_name = NULL, *alias_address = NULL;

				if (camel_internet_address_get (inet_address, ii, &alias_name, &alias_address) &&
				    alias_address && *alias_address) {
					if (!alias_name || !*alias_name)
						alias_name = name;

					if (alias_name && *alias_name)
						g_ptr_array_add (identities, g_strdup_printf ("%s <%s>", alias_name, alias_address));
					else
						g_ptr_array_add (identities, g_strdup_printf ("%s", alias_address));
				}
			}
		}

		g_free (aliases);
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	g_ptr_array_sort (identities, sort_identities_by_email_cb);

	/* NULL-terminated array */
	g_ptr_array_add (identities, NULL);

	return (gchar **) g_ptr_array_free (identities, FALSE);
}

/**
 * itip_get_fallback_identity:
 * @registry: an #ESourceRegistry
 *
 * Returns a name + address string taken from the default mail identity,
 * but only if the corresponding account is enabled.  If the account is
 * disabled, the function returns %NULL.  This is meant to be used as a
 * fallback identity for organizers.  Free the returned string with
 * g_free().
 *
 * Returns: a fallback mail identity, or %NULL
 **/
gchar *
itip_get_fallback_identity (ESourceRegistry *registry)
{
	ESource *source;
	ESourceMailIdentity *mail_identity;
	const gchar *extension_name;
	const gchar *address;
	const gchar *name;
	gchar *identity = NULL;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	source = e_source_registry_ref_default_mail_identity (registry);

	if (source == NULL)
		return NULL;

	if (!e_source_registry_check_enabled (registry, source)) {
		g_object_unref (source);
		return NULL;
	}

	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
	mail_identity = e_source_get_extension (source, extension_name);

	name = e_source_mail_identity_get_name (mail_identity);
	address = e_source_mail_identity_get_address (mail_identity);

	if (address != NULL) {
		if (name && *name)
			identity = g_strdup_printf ("%s <%s>", name, address);
		else
			identity = g_strdup_printf ("%s", address);
	}

	g_object_unref (source);

	return identity;
}

/**
 * itip_address_is_user:
 * @registry: an #ESourceRegistry
 * @address: an email address
 *
 * Looks for a registered mail identity with a matching email address.
 *
 * Returns: %TRUE if a match was found, %FALSE if not
 **/
gboolean
itip_address_is_user (ESourceRegistry *registry,
                      const gchar *address)
{
	GList *list, *iter;
	const gchar *extension_name;
	gboolean match = FALSE;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), FALSE);
	g_return_val_if_fail (address != NULL, FALSE);

	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;

	list = e_source_registry_list_sources (registry, extension_name);

	for (iter = list; iter && !match; iter = g_list_next (iter)) {
		ESource *source = E_SOURCE (iter->data);
		ESourceMailIdentity *extension;
		GHashTable *aliases;
		const gchar *id_address;

		extension = e_source_get_extension (source, extension_name);
		id_address = e_source_mail_identity_get_address (extension);

		if (id_address && g_ascii_strcasecmp (address, id_address) == 0) {
			match = TRUE;
			break;
		}

		aliases = e_source_mail_identity_get_aliases_as_hash_table (extension);
		if (aliases) {
			match = g_hash_table_contains (aliases, address);
			g_hash_table_destroy (aliases);
		}
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	return match;
}

gboolean
itip_organizer_is_user (ESourceRegistry *registry,
                        ECalComponent *comp,
                        ECalClient *cal_client)
{
	return itip_organizer_is_user_ex (registry, comp, cal_client, FALSE);
}

gboolean
itip_organizer_is_user_ex (ESourceRegistry *registry,
                           ECalComponent *comp,
                           ECalClient *cal_client,
                           gboolean skip_cap_test)
{
	ECalComponentOrganizer organizer;
	const gchar *strip;
	gboolean user_org = FALSE;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), FALSE);

	if (!e_cal_component_has_organizer (comp) ||
		(!skip_cap_test && e_client_check_capability (
		E_CLIENT (cal_client), CAL_STATIC_CAPABILITY_NO_ORGANIZER)))
		return FALSE;

	e_cal_component_get_organizer (comp, &organizer);
	if (organizer.value != NULL) {
		gchar *email = NULL;

		strip = itip_strip_mailto (organizer.value);

		if (e_client_get_backend_property_sync (E_CLIENT (cal_client),
							CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS,
							&email, NULL, NULL) &&
				email && g_ascii_strcasecmp (email, strip) == 0) {
			g_free (email);

			return TRUE;
		}

		g_free (email);

		if (e_client_check_capability (E_CLIENT (cal_client), CAL_STATIC_CAPABILITY_ORGANIZER_NOT_EMAIL_ADDRESS)) {
			return FALSE;
		}

		user_org = itip_address_is_user (registry, strip);
	}

	return user_org;
}

gboolean
itip_sentby_is_user (ESourceRegistry *registry,
                     ECalComponent *comp,
                     ECalClient *cal_client)
{
	ECalComponentOrganizer organizer;
	const gchar *strip;
	gboolean user_sentby = FALSE;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), FALSE);

	if (!e_cal_component_has_organizer (comp) ||
		e_client_check_capability (
		E_CLIENT (cal_client), CAL_STATIC_CAPABILITY_NO_ORGANIZER))
		return FALSE;

	e_cal_component_get_organizer (comp, &organizer);
	if (organizer.sentby != NULL) {
		strip = itip_strip_mailto (organizer.sentby);
		user_sentby = itip_address_is_user (registry, strip);
	}

	return user_sentby;
}

gboolean
itip_has_any_attendees (ECalComponent *comp)
{
	ECalComponentOrganizer organizer;
	ECalComponentAttendee *attendee;
	GSList *attendees = NULL;
	gboolean res;

	g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), FALSE);

	if (!e_cal_component_has_attendees (comp))
		return FALSE;

	e_cal_component_get_attendee_list (comp, &attendees);

	/* No attendee */
	if (!attendees)
		return FALSE;

	/* More than one attendee */
	if (attendees->next) {
		e_cal_component_free_attendee_list (attendees);
		return TRUE;
	}

	/* Exactly one attendee, check if it's not the organizer */
	attendee = attendees->data;

	g_return_val_if_fail (attendee != NULL, FALSE);

	if (!e_cal_component_has_organizer (comp)) {
		e_cal_component_free_attendee_list (attendees);
		return FALSE;
	}

	e_cal_component_get_organizer (comp, &organizer);

	res = attendee->value && (!organizer.value ||
		g_ascii_strcasecmp (itip_strip_mailto (attendee->value), itip_strip_mailto (organizer.value)) != 0);

	e_cal_component_free_attendee_list (attendees);

	return res;
}

static ECalComponentAttendee *
get_attendee (GSList *attendees,
	      const gchar *address,
	      GHashTable *aliases)
{
	GSList *l;

	if (!address)
		return NULL;

	for (l = attendees; l; l = l->next) {
		ECalComponentAttendee *attendee = l->data;
		const gchar *nomailto;

		nomailto = itip_strip_mailto (attendee->value);
		if (!nomailto || !*nomailto)
			continue;

		if ((address && g_ascii_strcasecmp (nomailto, address) == 0) ||
		    (aliases && g_hash_table_contains (aliases, nomailto))) {
			return attendee;
		}
	}

	return NULL;
}

static ECalComponentAttendee *
get_attendee_if_attendee_sentby_is_user (GSList *attendees,
					 const gchar *address,
					 GHashTable *aliases)
{
	GSList *l;

	for (l = attendees; l; l = l->next) {
		ECalComponentAttendee *attendee = l->data;
		const gchar *nomailto;

		nomailto = itip_strip_mailto (attendee->sentby);
		if (!nomailto || !*nomailto)
			continue;

		if ((address && g_ascii_strcasecmp (nomailto, address) == 0) ||
		    (aliases && g_hash_table_contains (aliases, nomailto))) {
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
itip_get_comp_attendee (ESourceRegistry *registry,
                        ECalComponent *comp,
                        ECalClient *cal_client)
{
	ESource *source;
	GSList *attendees;
	ECalComponentAttendee *attendee = NULL;
	GList *list, *link;
	const gchar *extension_name;
	gchar *address = NULL;

	e_cal_component_get_attendee_list (comp, &attendees);

	if (cal_client)
		e_client_get_backend_property_sync (
			E_CLIENT (cal_client),
			CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS,
			&address, NULL, NULL);

	if (address != NULL && *address != '\0') {
		attendee = get_attendee (attendees, address, NULL);

		if (attendee) {
			gchar *user_email;

			user_email = g_strdup (
				itip_strip_mailto (attendee->value));
			e_cal_component_free_attendee_list (attendees);
			g_free (address);

			return user_email;
		}

		attendee = get_attendee_if_attendee_sentby_is_user (attendees, address, NULL);

		if (attendee != NULL) {
			gchar *user_email;

			user_email = g_strdup (
				itip_strip_mailto (attendee->sentby));
			e_cal_component_free_attendee_list (attendees);
			g_free (address);

			return user_email;
		}
	}

	g_free (address);
	address = NULL;

	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
	list = e_source_registry_list_enabled (registry, extension_name);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESourceMailIdentity *extension;
		GHashTable *aliases;

		source = E_SOURCE (link->data);

		extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
		extension = e_source_get_extension (source, extension_name);

		address = e_source_mail_identity_dup_address (extension);

		aliases = e_source_mail_identity_get_aliases_as_hash_table (extension);

		attendee = get_attendee (attendees, address, aliases);
		if (attendee != NULL) {
			gchar *user_email;

			user_email = g_strdup (itip_strip_mailto (attendee->value));
			e_cal_component_free_attendee_list (attendees);

			if (aliases)
				g_hash_table_destroy (aliases);
			g_free (address);

			return user_email;
		}

		/* If the account was not found in the attendees list, then
		 * let's check the 'sentby' fields of the attendees if we can
		 * find the account. */
		attendee = get_attendee_if_attendee_sentby_is_user (attendees, address, aliases);
		if (attendee) {
			gchar *user_email;

			user_email = g_strdup (itip_strip_mailto (attendee->sentby));
			e_cal_component_free_attendee_list (attendees);

			if (aliases)
				g_hash_table_destroy (aliases);
			g_free (address);

			return user_email;
		}

		if (aliases)
			g_hash_table_destroy (aliases);
		g_free (address);
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	/* We could not find the attendee in the component, so just give
	 * the default account address if the email address is not set in
	 * the backend. */
	/* FIXME do we have a better way ? */
	itip_get_default_name_and_address (registry, NULL, &address);

	e_cal_component_free_attendee_list (attendees);

	if (address == NULL)
		address = g_strdup ("");

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
get_label (struct icaltimetype *tt,
           gboolean use_24_hour_format)
{
	gchar buffer[1000];
	struct tm tmp_tm;

	tmp_tm = icaltimetype_to_tm (tt);

	e_time_format_date_and_time (
		&tmp_tm, use_24_hour_format, FALSE, FALSE, buffer, 1000);

	return g_strdup (buffer);
}

typedef struct {
	GHashTable *tzids;
	icalcomponent *icomp;
	ECalClient *client;
	icalcomponent *zones;
} ItipUtilTZData;

static void
foreach_tzid_callback (icalparameter *param,
                       gpointer data)
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
		e_cal_client_get_timezone_sync (tz_data->client, tzid, &zone, NULL, NULL);
	if (zone == NULL)
		return;

	/* Convert it to a string and add it to the hash. */
	vtimezone_comp = icaltimezone_get_component (zone);
	if (!vtimezone_comp)
		return;

	icalcomponent_add_component (
		tz_data->icomp, icalcomponent_new_clone (vtimezone_comp));
	g_hash_table_insert (tz_data->tzids, (gchar *) tzid, (gchar *) tzid);
}

static icalcomponent *
comp_toplevel_with_zones (ECalComponentItipMethod method,
                          ECalComponent *comp,
                          ECalClient *cal_client,
                          icalcomponent *zones)
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
	tz_data.client = cal_client;
	tz_data.zones = zones;
	icalcomponent_foreach_tzid (icomp, foreach_tzid_callback, &tz_data);
	g_hash_table_destroy (tz_data.tzids);

	icalcomponent_add_component (top_level, icomp);

	return top_level;
}

static gboolean
users_has_attendee (const GSList *users,
                    const gchar *address)
{
	const GSList *l;

	for (l = users; l != NULL; l = l->next) {
		if (!g_ascii_strcasecmp (address, l->data))
			return TRUE;
	}

	return FALSE;
}

static gchar *
comp_from (ECalComponentItipMethod method,
           ECalComponent *comp,
           ESourceRegistry *registry,
	   gchar **from_name)
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
		return itip_get_comp_attendee (registry, comp, NULL);

	case E_CAL_COMPONENT_METHOD_REPLY:
		sender = itip_get_comp_attendee (registry, comp, NULL);
		if (sender != NULL)
			return sender;
		if (!e_cal_component_has_attendees (comp))
			return NULL;
		/* coverity[fallthrough] */

	case E_CAL_COMPONENT_METHOD_CANCEL:

	case E_CAL_COMPONENT_METHOD_ADD:

		e_cal_component_get_organizer (comp, &organizer);
		if (organizer.value == NULL) {
			e_notice (
				NULL, GTK_MESSAGE_ERROR,
				_("An organizer must be set."));
			return NULL;
		}
		if (from_name)
			*from_name = g_strdup (organizer.cn);
		return g_strdup (itip_strip_mailto (organizer.value));

	default:
		if (!e_cal_component_has_attendees (comp))
			return NULL;

		e_cal_component_get_attendee_list (comp, &attendees);
		attendee = attendees->data;
		if (attendee->value != NULL) {
			from = g_strdup (itip_strip_mailto (attendee->value));
			if (from_name)
				*from_name = g_strdup (attendee->cn);
		} else
			from = NULL;
		e_cal_component_free_attendee_list (attendees);

		return from;
	}
}

static EDestination **
comp_to_list (ESourceRegistry *registry,
              ECalComponentItipMethod method,
              ECalComponent *comp,
              const GSList *users,
              gboolean reply_all,
              const GSList *only_attendees)
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
			e_notice (
				NULL, GTK_MESSAGE_ERROR,
				_("At least one attendee is necessary"));
			e_cal_component_free_attendee_list (attendees);
			return NULL;
		}

		e_cal_component_get_organizer (comp, &organizer);
		if (organizer.value == NULL) {
			e_notice (
				NULL, GTK_MESSAGE_ERROR,
				_("An organizer must be set."));
			return NULL;
		}

		array = g_ptr_array_new ();

		sender = itip_get_comp_attendee (registry, comp, NULL);

		for (l = attendees; l != NULL; l = l->next) {
			ECalComponentAttendee *att = l->data;

			/* Bugfix: 688711 - Varadhan
			 * Resource is also considered as a "attendee". If the respective backend
			 * is able to successfully book resources automagically, it will appear
			 * in the users list and thereby won't get added to the list of destinations
			 * to send the meeting invite, otherwise, as a safety measure, a meeting
			 * invite will be sent to the resources as well. */
			if (att->cutype != ICAL_CUTYPE_INDIVIDUAL &&
			    att->cutype != ICAL_CUTYPE_GROUP &&
			    att->cutype != ICAL_CUTYPE_RESOURCE &&
			    att->cutype != ICAL_CUTYPE_UNKNOWN)
				continue;
			else if (users_has_attendee (users, att->value))
				continue;
			else if (att->sentby &&
				users_has_attendee (users, att->sentby))
				continue;
			else if (!g_ascii_strcasecmp (
				att->value, organizer.value))
				continue;
			else if (att->sentby && !g_ascii_strcasecmp (
				att->sentby, organizer.sentby))
				continue;
			else if (!g_ascii_strcasecmp (
				itip_strip_mailto (att->value), sender))
				continue;
			else if (att->status == ICAL_PARTSTAT_DELEGATED &&
				(att->delto && *att->delto) && !(att->rsvp) &&
				method == E_CAL_COMPONENT_METHOD_REQUEST)
				continue;
			else if (only_attendees &&
				!cal_comp_util_have_in_new_attendees (
				only_attendees, itip_strip_mailto (att->value)))
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

			sender = itip_get_comp_attendee (registry, comp, NULL);

			e_cal_component_get_organizer (comp, &organizer);
			if (organizer.value && (!sender || g_ascii_strcasecmp (
			    itip_strip_mailto (organizer.value), sender) != 0)) {
				destination = e_destination_new ();
				e_destination_set_email (
					destination,
					itip_strip_mailto (organizer.value));
				if (organizer.cn)
					e_destination_set_name (destination, organizer.cn);
				g_ptr_array_add (array, destination);
			}

			for (l = attendees; l != NULL; l = l->next) {
				ECalComponentAttendee *att = l->data;

				if (!att->value)
					continue;
				else if (att->cutype != ICAL_CUTYPE_INDIVIDUAL &&
					 att->cutype != ICAL_CUTYPE_GROUP &&
					 att->cutype != ICAL_CUTYPE_UNKNOWN)
					continue;
				else if (only_attendees &&
					!cal_comp_util_have_in_new_attendees (
					only_attendees, itip_strip_mailto (att->value)))
					continue;
				else if (organizer.value &&
					 g_ascii_strcasecmp (att->value, organizer.value) == 0)
					continue;
				else if (sender && g_ascii_strcasecmp (
					itip_strip_mailto (att->value), sender) == 0)
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
			if (organizer.cn)
				e_destination_set_name (destination, organizer.cn);
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
			e_notice (
				NULL, GTK_MESSAGE_ERROR,
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
		sender = itip_get_comp_attendee (registry, comp, NULL);

		for (l = attendees; l != NULL; l = l->next) {
			ECalComponentAttendee *att = l->data;

			if (att->cutype != ICAL_CUTYPE_INDIVIDUAL &&
			    att->cutype != ICAL_CUTYPE_GROUP &&
			    att->cutype != ICAL_CUTYPE_UNKNOWN)
				continue;

			if (!g_ascii_strcasecmp (
				itip_strip_mailto (att->value), sender) ||
				(att->sentby && !g_ascii_strcasecmp (
				itip_strip_mailto (att->sentby), sender))) {

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
			const GSList *list;

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
comp_subject (ESourceRegistry *registry,
              ECalComponentItipMethod method,
              ECalComponent *comp)
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
		sender = itip_get_comp_attendee (registry, comp, NULL);
		if (sender) {

			for (l = alist; l != NULL; l = l->next) {
				a = l->data;
				if ((sender && *sender) && (g_ascii_strcasecmp (
					itip_strip_mailto (a->value), sender) ||
					(a->sentby && g_ascii_strcasecmp (
					itip_strip_mailto (a->sentby), sender))))
					break;
			}
			g_free (sender);
		}

		if (a != NULL) {

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
comp_content_type (ECalComponent *comp,
                   ECalComponentItipMethod method)
{
	const gchar *name;

	if (e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_FREEBUSY)
		name = "freebusy.ifb";
	else
		name = "calendar.ics";

	return g_strdup_printf (
		"text/calendar; name=\"%s\"; charset=utf-8; METHOD=%s",
		name, itip_methods[method]);
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
comp_description (ECalComponent *comp,
                  gboolean use_24_hour_format)
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
			start = get_label (dt.value, use_24_hour_format);
		e_cal_component_free_datetime (&dt);

		e_cal_component_get_dtend (comp, &dt);
		if (dt.value)
			end = get_label (dt.value, use_24_hour_format);
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
comp_server_send_sync (ECalComponentItipMethod method,
		       ECalComponent *comp,
		       ECalClient *cal_client,
		       icalcomponent *zones,
		       GSList **users,
		       GCancellable *cancellable,
		       GError **error)
{
	icalcomponent *top_level, *returned_icalcomp = NULL;
	gboolean retval = TRUE;
	GError *local_error = NULL;

	top_level = comp_toplevel_with_zones (method, comp, cal_client, zones);
	d (printf ("itip-utils.c: comp_server_send_sync: calling e_cal_send_objects... \n"));

	e_cal_client_send_objects_sync (
		cal_client, top_level, users,
		&returned_icalcomp, cancellable, &local_error);

	if (g_error_matches (local_error, E_CAL_CLIENT_ERROR, E_CAL_CLIENT_ERROR_OBJECT_ID_ALREADY_EXISTS)) {
		g_propagate_error (error, g_error_new (local_error->domain, local_error->code,
			_("Unable to book a resource, the new event collides with some other.")));
		g_clear_error (&local_error);
		retval = FALSE;

	} else if (local_error != NULL) {
		g_prefix_error (&local_error, "%s", _("Unable to book a resource, error: "));
		g_propagate_error (error, local_error);
		retval = FALSE;
	}

	if (returned_icalcomp != NULL)
		icalcomponent_free (returned_icalcomp);
	icalcomponent_free (top_level);

	return retval;
}

static gboolean
comp_limit_attendees (ESourceRegistry *registry,
                      ECalComponent *comp)
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
		found = match = itip_address_is_user (registry, attendee_text);

		if (!found) {
			param = icalproperty_get_first_parameter (prop, ICAL_SENTBY_PARAMETER);
			if (param) {
				attendee_sentby =
					icalparameter_get_sentby (param);
				attendee_sentby =
					itip_strip_mailto (attendee_sentby);
				attendee_sentby_text =
					g_strstrip (g_strdup (attendee_sentby));
				found = match = itip_address_is_user (
					registry, attendee_sentby_text);
			}
		}

		g_free (attendee_text);
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
comp_sentby (ECalComponent *comp,
             ECalClient *cal_client,
             ESourceRegistry *registry)
{
	ECalComponentOrganizer organizer;
	GSList * attendees, *l;
	gchar *name;
	gchar *address;
	gchar *user = NULL;

	itip_get_default_name_and_address (registry, &name, &address);

	e_cal_component_get_organizer (comp, &organizer);
	if (!organizer.value && name != NULL && address != NULL) {
		organizer.value = g_strdup_printf ("MAILTO:%s", address);
		organizer.sentby = NULL;
		organizer.cn = name;
		organizer.language = NULL;

		e_cal_component_set_organizer (comp, &organizer);
		g_free ((gchar *) organizer.value);

		g_free (name);
		g_free (address);
		return;
	}

	e_cal_component_get_attendee_list (comp, &attendees);
	user = itip_get_comp_attendee (registry, comp, cal_client);
	for (l = attendees; l; l = l->next) {
		ECalComponentAttendee *a = l->data;

		if (!g_ascii_strcasecmp (
			itip_strip_mailto (a->value), user) ||
			(a->sentby && !g_ascii_strcasecmp (
			itip_strip_mailto (a->sentby), user))) {
			g_free (user);

			g_free (name);
			g_free (address);
			return;
		}
	}

	if (!itip_organizer_is_user (registry, comp, cal_client) &&
	    !itip_sentby_is_user (registry, comp, cal_client) &&
	    address != NULL) {
		organizer.value = g_strdup (organizer.value);
		organizer.sentby = g_strdup_printf ("MAILTO:%s", address);
		organizer.cn = g_strdup (organizer.cn);
		organizer.language = g_strdup (organizer.language);

		e_cal_component_set_organizer (comp, &organizer);

		g_free ((gchar *) organizer.value);
		g_free ((gchar *) organizer.sentby);
		g_free ((gchar *) organizer.cn);
		g_free ((gchar *) organizer.language);
	}

	g_free (name);
	g_free (address);
}

static ECalComponent *
comp_minimal (ESourceRegistry *registry,
              ECalComponent *comp,
              gboolean attendee)
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

		if (!comp_limit_attendees (registry, clone)) {
			e_notice (
				NULL, GTK_MESSAGE_ERROR,
				_("You must be an attendee of the event."));
			goto error;
		}
	}

	itt = icaltime_from_timet_with_zone (
		time (NULL), FALSE,
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
comp_compliant (ESourceRegistry *registry,
                ECalComponentItipMethod method,
                ECalComponent *comp,
                ECalClient *client,
                icalcomponent *zones,
                icaltimezone *default_zone,
                gboolean strip_alarms)
{
	ECalComponent *clone, *temp_clone;
	struct icaltimetype itt;

	clone = e_cal_component_clone (comp);
	itt = icaltime_from_timet_with_zone (
		time (NULL), FALSE,
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
				from_zone = default_zone;
			} else if (dt.tzid == NULL) {
				from_zone = icaltimezone_get_utc_timezone ();
			} else {
				if (zones != NULL)
					from_zone = icalcomponent_get_timezone (zones, dt.tzid);
				if (from_zone == NULL)
					from_zone = icaltimezone_get_builtin_timezone_from_tzid (dt.tzid);
				if (from_zone == NULL && client != NULL)
					/* FIXME Error checking */
					e_cal_client_get_timezone_sync (
						client, dt.tzid,
						&from_zone, NULL, NULL);
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

			alarm = e_cal_component_get_alarm (clone, (const gchar *) l->data);
			if (alarm) {
				e_cal_component_alarm_get_action (alarm, &action);
				e_cal_component_alarm_free (alarm);

				if (action == E_CAL_COMPONENT_ALARM_PROCEDURE)
					e_cal_component_remove_alarm (clone, (const gchar *) l->data);
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
		comp_sentby (clone, client, registry);
		e_cal_component_set_attendee_list (clone, NULL);
		break;
	case E_CAL_COMPONENT_METHOD_REQUEST:
		comp_sentby (clone, client, registry);
		break;
	case E_CAL_COMPONENT_METHOD_CANCEL:
		comp_sentby (clone, client, registry);
		break;
	case E_CAL_COMPONENT_METHOD_REPLY:
		break;
	case E_CAL_COMPONENT_METHOD_ADD:
		break;
	case E_CAL_COMPONENT_METHOD_REFRESH:
		/* Need to remove almost everything */
		temp_clone = comp_minimal (registry, clone, TRUE);
		g_object_unref (clone);
		clone = temp_clone;
		break;
	case E_CAL_COMPONENT_METHOD_COUNTER:
		break;
	case E_CAL_COMPONENT_METHOD_DECLINECOUNTER:
		/* Need to remove almost everything */
		temp_clone = comp_minimal (registry, clone, FALSE);
		g_object_unref (clone);
		clone = temp_clone;
		break;
	default:
		break;
	}

	return clone;
}

void
itip_cal_mime_attach_free (gpointer ptr)
{
	struct CalMimeAttach *mime_attach = ptr;

	if (mime_attach) {
		g_free (mime_attach->filename);
		g_free (mime_attach->content_type);
		g_free (mime_attach->content_id);
		g_free (mime_attach->description);
		g_free (mime_attach->encoded_data);
		g_free (mime_attach);
	}
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
			camel_mime_part_set_content_id (
				attachment, mime_attach->content_id);
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
	}

	g_slist_free_full (attach_list, itip_cal_mime_attach_free);
}

static ESource *
find_enabled_identity (ESourceRegistry *registry,
                       const gchar *id_address)
{
	GList *list, *link;
	ESource *mail_identity = NULL;
	const gchar *extension_name;

	if (id_address == NULL)
		return NULL;

	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
	list = e_source_registry_list_enabled (registry, extension_name);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		ESourceMailIdentity *extension;
		GHashTable *aliases;
		const gchar *address;

		extension = e_source_get_extension (source, extension_name);
		address = e_source_mail_identity_get_address (extension);

		if (address && g_ascii_strcasecmp (address, id_address) == 0) {
			mail_identity = g_object_ref (source);
			break;
		}

		aliases = e_source_mail_identity_get_aliases_as_hash_table (extension);
		if (aliases) {
			if (g_hash_table_contains (aliases, id_address))
				mail_identity = g_object_ref (source);
			g_hash_table_destroy (aliases);

			if (mail_identity)
				break;
		}
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	return mail_identity;
}

static gchar *
get_identity_uid_for_from (EShell *shell,
			   ECalComponentItipMethod method,
			   ECalComponent *comp,
			   ECalClient *cal_client,
			   gchar **identity_name,
			   gchar **identity_address)
{
	EClientCache *client_cache;
	ESourceRegistry *registry;
	ESource *source = NULL;
	gchar *identity_uid = NULL;

	client_cache = e_shell_get_client_cache (shell);
	registry = e_client_cache_ref_registry (client_cache);

	/* always use organizer's email when user is an organizer */
	if (itip_organizer_is_user (registry, comp, cal_client)) {
		ECalComponentOrganizer organizer = {0};

		e_cal_component_get_organizer (comp, &organizer);
		if (organizer.value != NULL) {
			source = find_enabled_identity (
				registry,
				itip_strip_mailto (organizer.value));

			if (source) {
				if (identity_name)
					*identity_name = g_strdup (organizer.cn);
				if (identity_address)
					*identity_address = g_strdup (itip_strip_mailto (organizer.value));
			}
		}
	}

	if (source == NULL) {
		gchar *from = comp_from (method, comp, registry, identity_name);

		if (from != NULL) {
			source = find_enabled_identity (registry, from);
			if (source) {
				if (identity_address)
					*identity_address = g_strdup (from);
			}
		}

		g_free (from);
	}

	if (source != NULL) {
		identity_uid = g_strdup (e_source_get_uid (source));

		g_object_unref (source);
	}

	g_object_unref (registry);

	return identity_uid;
}

typedef struct {
	ESourceRegistry *registry;
	ECalComponentItipMethod method;
	ECalComponent *send_comp;
	ECalClient *cal_client;
	icalcomponent *zones;
	GSList *attachments_list;
	GSList *users;
	gboolean strip_alarms;
	gboolean only_new_attendees;
	gboolean ensure_master_object;

	gboolean completed;
	gboolean success;

	GError *async_error;
} ItipSendComponentData;

static void
itip_send_component_data_free (gpointer ptr)
{
	ItipSendComponentData *isc = ptr;

	if (isc) {
		g_clear_object (&isc->registry);
		g_clear_object (&isc->send_comp);
		g_clear_object (&isc->cal_client);
		g_clear_error (&isc->async_error);
		if (isc->zones)
			icalcomponent_free (isc->zones);
		g_slist_free_full (isc->attachments_list, itip_cal_mime_attach_free); /* CamelMimePart */
		g_slist_free_full (isc->users, g_free);
		g_free (isc);
	}
}

static void
itip_send_component_begin (ItipSendComponentData *isc,
			   GCancellable *cancellable,
			   GError **error)
{
	g_return_if_fail (isc != NULL);

	isc->completed = FALSE;

	if (isc->method != E_CAL_COMPONENT_METHOD_PUBLISH && e_cal_client_check_save_schedules (isc->cal_client)) {
		isc->success = TRUE;
		isc->completed = TRUE;
		return;
	}

	if (isc->ensure_master_object && e_cal_component_is_instance (isc->send_comp)) {
		/* Ensure we send the master object, not the instance only */
		icalcomponent *icalcomp = NULL;
		const gchar *uid = NULL;

		e_cal_component_get_uid (isc->send_comp, &uid);
		if (e_cal_client_get_object_sync (isc->cal_client, uid, NULL, &icalcomp, cancellable, NULL) && icalcomp) {
			ECalComponent *send_comp;

			send_comp = e_cal_component_new_from_icalcomponent (icalcomp);
			if (send_comp) {
				g_object_unref (isc->send_comp);
				isc->send_comp = send_comp;
			}
		}
	}

	/* Give the server a chance to manipulate the comp */
	if (isc->method != E_CAL_COMPONENT_METHOD_PUBLISH) {
		d (printf ("itip-utils.c: itip_send_component_begin: calling comp_server_send_sync... \n"));
		if (!comp_server_send_sync (isc->method, isc->send_comp, isc->cal_client, isc->zones, &isc->users, cancellable, error)) {
			isc->success = FALSE;
			isc->completed = TRUE;
			return;
		}
	}

	/* check whether backend could handle sending requests/updates */
	if (isc->method != E_CAL_COMPONENT_METHOD_PUBLISH &&
	    e_client_check_capability (E_CLIENT (isc->cal_client), CAL_STATIC_CAPABILITY_CREATE_MESSAGES)) {
		isc->success = TRUE;
		isc->completed = TRUE;
	}
}

typedef struct _CreateComposerData {
	gchar *identity_uid;
	gchar *identity_name;
	gchar *identity_address;
	EDestination **destinations;
	gchar *subject;
	gchar *ical_string;
	gchar *content_type;
	gchar *event_body_text;
	GSList *attachments_list;
	ECalComponent *comp;
	gboolean show_only;
} CreateComposerData;

static void
itip_send_component_composer_created_cb (GObject *source_object,
					 GAsyncResult *result,
					 gpointer user_data)
{
	CreateComposerData *ccd = user_data;
	EComposerHeaderTable *table;
	EMsgComposer *composer;
	GSettings *settings;
	gboolean use_24hour_format;
	GError *error = NULL;

	g_return_if_fail (ccd != NULL);

	composer = e_msg_composer_new_finish (result, &error);
	if (error) {
		g_warning ("%s: Failed to create msg composer: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
		return;
	}

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");
	use_24hour_format = g_settings_get_boolean (settings, "use-24hour-format");
	g_object_unref (settings);

	table = e_msg_composer_get_header_table (composer);

	if (ccd->identity_uid)
		e_composer_header_table_set_identity_uid (table, ccd->identity_uid, ccd->identity_name, ccd->identity_address);

	e_composer_header_table_set_subject (table, ccd->subject);
	e_composer_header_table_set_destinations_to (table, ccd->destinations);

	if (e_cal_component_get_vtype (ccd->comp) == E_CAL_COMPONENT_EVENT) {
		if (ccd->event_body_text)
			e_msg_composer_set_body_text (composer, ccd->event_body_text, TRUE);
		else
			e_msg_composer_set_body (composer, ccd->ical_string, ccd->content_type);
	} else {
		CamelMimePart *attachment;
		const gchar *filename;
		gchar *description;
		gchar *body;

		filename = comp_filename (ccd->comp);
		description = comp_description (ccd->comp, use_24hour_format);

		body = camel_text_to_html (description, CAMEL_MIME_FILTER_TOHTML_PRE, 0);
		e_msg_composer_set_body_text (composer, body, TRUE);
		g_free (body);

		attachment = camel_mime_part_new ();
		camel_mime_part_set_content (
			attachment, ccd->ical_string,
			strlen (ccd->ical_string), ccd->content_type);
		if (filename != NULL && *filename != '\0')
			camel_mime_part_set_filename (attachment, filename);
		if (description != NULL && *description != '\0')
			camel_mime_part_set_description (attachment, description);
		camel_mime_part_set_disposition (attachment, "inline");
		e_msg_composer_attach (composer, attachment);
		g_object_unref (attachment);

		g_free (description);
	}

	append_cal_attachments (composer, ccd->comp, ccd->attachments_list);
	ccd->attachments_list = NULL;

	if (ccd->show_only)
		gtk_widget_show (GTK_WIDGET (composer));
	else
		e_msg_composer_send (composer);

	e_destination_freev (ccd->destinations);
	g_clear_object (&ccd->comp);
	g_free (ccd->identity_uid);
	g_free (ccd->identity_name);
	g_free (ccd->identity_address);
	g_free (ccd->subject);
	g_free (ccd->ical_string);
	g_free (ccd->content_type);
	g_free (ccd->event_body_text);
	g_free (ccd);
}

static void
itip_send_component_complete (ItipSendComponentData *isc)
{
	CreateComposerData *ccd;
	EDestination **destinations;
	ECalComponent *comp = NULL;
	EShell *shell;
	icalcomponent *top_level = NULL;
	icaltimezone *default_zone;

	g_return_if_fail (isc != NULL);

	if (isc->completed)
		return;

	isc->success = FALSE;

	default_zone = calendar_config_get_icaltimezone ();

	/* Tidy up the comp */
	comp = comp_compliant (
		isc->registry, isc->method, isc->send_comp, isc->cal_client,
		isc->zones, default_zone, isc->strip_alarms);

	if (comp == NULL)
		goto cleanup;

	/* Recipients */
	destinations = comp_to_list (
		isc->registry, isc->method, comp, isc->users, FALSE,
		isc->only_new_attendees ? g_object_get_data (
		G_OBJECT (isc->send_comp), "new-attendees") : NULL);
	if (isc->method != E_CAL_COMPONENT_METHOD_PUBLISH) {
		if (destinations == NULL) {
			/* We sent them all via the server */
			isc->success = TRUE;
			goto cleanup;
		}
	}

	shell = e_shell_get_default ();
	top_level = comp_toplevel_with_zones (isc->method, comp, isc->cal_client, isc->zones);

	ccd = g_new0 (CreateComposerData, 1);
	ccd->identity_uid = get_identity_uid_for_from (shell, isc->method, isc->send_comp, isc->cal_client, &ccd->identity_name, &ccd->identity_address);
	ccd->destinations = destinations;
	ccd->subject = comp_subject (isc->registry, isc->method, comp);
	ccd->ical_string = icalcomponent_as_ical_string_r (top_level);
	ccd->content_type = comp_content_type (comp, isc->method);
	ccd->event_body_text = NULL;
	ccd->attachments_list = isc->attachments_list;
	ccd->comp = comp;
	ccd->show_only = isc->method == E_CAL_COMPONENT_METHOD_PUBLISH && !isc->users;

	isc->attachments_list = NULL;
	comp = NULL;

	e_msg_composer_new (shell, itip_send_component_composer_created_cb, ccd);

	isc->success = TRUE;

 cleanup:
	g_clear_object (&comp);
	if (top_level != NULL)
		icalcomponent_free (top_level);
}

static void
itip_send_component_complete_and_free (gpointer ptr)
{
	ItipSendComponentData *isc = ptr;

	if (isc) {
		itip_send_component_complete (isc);
		itip_send_component_data_free (isc);
	}
}

static void
itip_send_component_thread (EAlertSinkThreadJobData *job_data,
			    gpointer user_data,
			    GCancellable *cancellable,
			    GError **error)
{
	ItipSendComponentData *isc = user_data;

	g_return_if_fail (isc != NULL);

	itip_send_component_begin (isc, cancellable, error);
}

void
itip_send_component_with_model (ECalModel *model,
				ECalComponentItipMethod method,
				ECalComponent *send_comp,
				ECalClient *cal_client,
				icalcomponent *zones,
				GSList *attachments_list,
				GSList *users,
				gboolean strip_alarms,
				gboolean only_new_attendees,
				gboolean ensure_master_object)
{
	ESourceRegistry *registry;
	ECalDataModel *data_model;
	ESource *source;
	const gchar *alert_ident = NULL;
	const gchar *description = NULL;
	gchar *display_name;
	GCancellable *cancellable;
	ItipSendComponentData *isc;

	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_CAL_CLIENT (cal_client));

	switch (e_cal_client_get_source_type (cal_client)) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			description = _("Sending an event");
			alert_ident = "calendar:failed-send-event";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			description = _("Sending a memo");
			alert_ident = "calendar:failed-send-memo";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			description = _("Sending a task");
			alert_ident = "calendar:failed-send-task";
			break;
		default:
			g_warn_if_reached ();
			break;
	}

	registry = e_cal_model_get_registry (model);
	data_model = e_cal_model_get_data_model (model);
	source = e_client_get_source (E_CLIENT (cal_client));

	isc = g_new0 (ItipSendComponentData, 1);
	isc->registry = g_object_ref (registry);
	isc->method = method;
	isc->send_comp = g_object_ref (send_comp);
	isc->cal_client = g_object_ref (cal_client);
	if (zones) {
		isc->zones = icalcomponent_new_clone (zones);
	}
	isc->attachments_list = attachments_list;
	if (users) {
		GSList *link;

		isc->users = g_slist_copy (users);
		for (link = isc->users; link; link = g_slist_next (link)) {
			link->data = g_strdup (link->data);
		}
	}
	isc->strip_alarms = strip_alarms;
	isc->only_new_attendees = only_new_attendees;
	isc->ensure_master_object = ensure_master_object;
	isc->success = FALSE;
	isc->completed = FALSE;

	display_name = e_util_get_source_full_name (registry, source);
	cancellable = e_cal_data_model_submit_thread_job (data_model, description, alert_ident,
		display_name, itip_send_component_thread,
		isc, itip_send_component_complete_and_free);

	g_clear_object (&cancellable);
	g_free (display_name);
}

gboolean
itip_send_comp_sync (ESourceRegistry *registry,
		     ECalComponentItipMethod method,
		     ECalComponent *send_comp,
		     ECalClient *cal_client,
		     icalcomponent *zones,
		     GSList *attachments_list,
		     GSList *users,
		     gboolean strip_alarms,
		     gboolean only_new_attendees,
		     GCancellable *cancellable,
		     GError **error)
{
	ItipSendComponentData isc;

	memset (&isc, 0, sizeof (ItipSendComponentData));

	isc.registry = registry;
	isc.method = method;
	isc.send_comp = send_comp;
	isc.cal_client = cal_client;
	isc.zones = zones;
	isc.attachments_list = attachments_list;
	isc.users = users;
	isc.strip_alarms = strip_alarms;
	isc.only_new_attendees = only_new_attendees;

	isc.completed = FALSE;
	isc.success = FALSE;

	itip_send_component_begin (&isc, cancellable, error);
	itip_send_component_complete (&isc);

	g_slist_free_full (isc.users, g_free);

	return isc.success;
}

static void
itip_send_component_task_thread (GTask *task,
				 gpointer source_object,
				 gpointer task_data,
				 GCancellable *cancellable)
{
	ItipSendComponentData *isc = task_data;

	g_return_if_fail (isc != NULL);

	itip_send_component_begin (isc, cancellable, &isc->async_error);
}

void
itip_send_component (ESourceRegistry *registry,
		     ECalComponentItipMethod method,
		     ECalComponent *send_comp,
		     ECalClient *cal_client,
		     icalcomponent *zones,
		     GSList *attachments_list,
		     GSList *users,
		     gboolean strip_alarms,
		     gboolean only_new_attendees,
		     gboolean ensure_master_object,
		     GCancellable *cancellable,
		     GAsyncReadyCallback callback,
		     gpointer user_data)
{
	GTask *task;
	ItipSendComponentData *isc;

	isc = g_new0 (ItipSendComponentData, 1);
	isc->registry = g_object_ref (registry);
	isc->method = method;
	isc->send_comp = g_object_ref (send_comp);
	isc->cal_client = g_object_ref (cal_client);
	if (zones)
		isc->zones = icalcomponent_new_clone (zones);
	isc->attachments_list = attachments_list;
	if (users) {
		GSList *link;

		isc->users = g_slist_copy (users);
		for (link = isc->users; link; link = g_slist_next (link)) {
			link->data = g_strdup (link->data);
		}
	}
	isc->strip_alarms = strip_alarms;
	isc->only_new_attendees = only_new_attendees;
	isc->ensure_master_object = ensure_master_object;

	isc->completed = FALSE;
	isc->success = FALSE;

	task = g_task_new (NULL, cancellable, callback, user_data);
	g_task_set_task_data (task, isc, itip_send_component_data_free);
	g_task_set_source_tag (task, itip_send_component);
	g_task_run_in_thread (task, itip_send_component_task_thread);
	g_object_unref (task);
}

gboolean
itip_send_component_finish (GAsyncResult *result,
			    GError **error)
{
	ItipSendComponentData *isc;

	isc = g_task_get_task_data (G_TASK (result));

	g_return_val_if_fail (isc != NULL, FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, itip_send_component), FALSE);

	itip_send_component_complete (isc);

	if (isc->async_error) {
		g_propagate_error (error, isc->async_error);
		isc->async_error = NULL;
	}

	return isc->success;
}

gboolean
reply_to_calendar_comp (ESourceRegistry *registry,
                        ECalComponentItipMethod method,
                        ECalComponent *send_comp,
                        ECalClient *cal_client,
                        gboolean reply_all,
                        icalcomponent *zones,
                        GSList *attachments_list)
{
	EShell *shell;
	ECalComponent *comp = NULL;
	icalcomponent *top_level = NULL;
	icaltimezone *default_zone;
	gboolean retval = FALSE;
	CreateComposerData *ccd;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), FALSE);

	/* FIXME Pass this in. */
	shell = e_shell_get_default ();

	default_zone = calendar_config_get_icaltimezone ();

	/* Tidy up the comp */
	comp = comp_compliant (
		registry, method, send_comp, cal_client,
		zones, default_zone, TRUE);
	if (comp == NULL)
		goto cleanup;

	top_level = comp_toplevel_with_zones (method, comp, cal_client, zones);

	ccd = g_new0 (CreateComposerData, 1);
	ccd->identity_uid = get_identity_uid_for_from (shell, method, send_comp, cal_client, &ccd->identity_name, &ccd->identity_address);
	ccd->destinations = comp_to_list (registry, method, comp, NULL, reply_all, NULL);
	ccd->subject = comp_subject (registry, method, comp);
	ccd->ical_string = icalcomponent_as_ical_string_r (top_level);
	ccd->comp = comp;
	ccd->show_only = TRUE;

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
			ECalComponentText text = *((ECalComponentText *) text_list->data);
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
			if (!start_zone && dtstart.tzid) {
				GError *error = NULL;

				e_cal_client_get_timezone_sync (
					cal_client, dtstart.tzid,
					&start_zone, NULL, &error);

				if (error != NULL) {
					g_warning (
						"%s: Couldn't get timezone '%s' from server: %s",
						G_STRFUNC,
						dtstart.tzid ? dtstart.tzid : "",
						error->message);
					g_error_free (error);
				}
			}

			if (!start_zone || dtstart.value->is_date)
				start_zone = default_zone;

			start = icaltime_as_timet_with_zone (*dtstart.value, start_zone);
			time = g_strdup (ctime (&start));
		}

		body = g_string_new (
			"<br><br><hr><br><b>"
			"______ Original Appointment ______ "
			"</b><br><br><table>");

		if (orig_from && *orig_from)
			g_string_append_printf (
				body,
				"<tr><td><b>From</b></td>"
				"<td>:</td><td>%s</td></tr>", orig_from);
		g_free (orig_from);

		if (subject)
			g_string_append_printf (
				body,
				"<tr><td><b>Subject</b></td>"
				"<td>:</td><td>%s</td></tr>", subject);
		g_free (subject);

		g_string_append_printf (
			body,
			"<tr><td><b>Location</b></td>"
			"<td>:</td><td>%s</td></tr>", location);

		if (time)
			g_string_append_printf (
				body,
				"<tr><td><b>Time</b></td>"
				"<td>:</td><td>%s</td></tr>", time);
		g_free (time);

		g_string_append_printf (body, "</table><br>");

		html_description = html_new_lines_for (description);
		g_string_append (body, html_description);
		g_free (html_description);

		ccd->event_body_text = g_string_free (body, FALSE);
	}

	comp = NULL;

	e_msg_composer_new (shell, itip_send_component_composer_created_cb, ccd);

	retval = TRUE;

 cleanup:

	if (comp != NULL)
		g_object_unref (comp);
	if (top_level != NULL)
		icalcomponent_free (top_level);

	return retval;
}

gboolean
itip_publish_begin (ECalComponent *pub_comp,
                    ECalClient *cal_client,
                    gboolean cloned,
                    ECalComponent **clone)
{
	icalcomponent *icomp = NULL, *icomp_clone = NULL;
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
				prop = icalcomponent_get_next_property (
					icomp,
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

static gboolean
check_time (const struct icaltimetype tmval,
            gboolean can_null_time)
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

/* Returns whether the passed-in icalcomponent is valid or not.
 * It does some sanity checks on values too. */
gboolean
is_icalcomp_valid (icalcomponent *icalcomp)
{
	if (!icalcomp || !icalcomponent_is_valid (icalcomp))
		return FALSE;

	switch (icalcomponent_isa (icalcomp)) {
	case ICAL_VEVENT_COMPONENT:
		return	check_time (icalcomponent_get_dtstart (icalcomp), FALSE) &&
			check_time (icalcomponent_get_dtend (icalcomp), TRUE);
	case ICAL_VTODO_COMPONENT:
		return	check_time (icalcomponent_get_dtstart (icalcomp), TRUE) &&
			check_time (icalcomponent_get_due (icalcomp), TRUE);
	case ICAL_VJOURNAL_COMPONENT:
		return	check_time (icalcomponent_get_dtstart (icalcomp), TRUE) &&
			check_time (icalcomponent_get_dtend (icalcomp), TRUE);
	default:
		break;
	}

	return TRUE;
}

gboolean
itip_component_has_recipients (ECalComponent *comp)
{
	GSList *attendees = NULL;
	ECalComponentAttendee *attendee;
	ECalComponentOrganizer organizer;
	gboolean res = FALSE;

	g_return_val_if_fail (comp != NULL, FALSE);

	e_cal_component_get_organizer (comp, &organizer);
	e_cal_component_get_attendee_list (comp, &attendees);

	if (!attendees) {
		if (organizer.value && e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_JOURNAL) {
			/* memos store recipients in an extra property */
			icalcomponent *icalcomp;
			icalproperty *icalprop;

			icalcomp = e_cal_component_get_icalcomponent (comp);

			for (icalprop = icalcomponent_get_first_property (icalcomp, ICAL_X_PROPERTY);
			     icalprop != NULL;
			     icalprop = icalcomponent_get_next_property (icalcomp, ICAL_X_PROPERTY)) {
				const gchar *x_name;

				x_name = icalproperty_get_x_name (icalprop);

				if (g_str_equal (x_name, "X-EVOLUTION-RECIPIENTS")) {
					const gchar *str_recipients = icalproperty_get_x (icalprop);

					res = str_recipients && g_ascii_strcasecmp (organizer.value, str_recipients) != 0;
					break;
				}
			}
		}

		return res;
	}

	if (g_slist_length (attendees) > 1 || !e_cal_component_has_organizer (comp)) {
		e_cal_component_free_attendee_list (attendees);
		return TRUE;
	}

	attendee = attendees->data;

	res = organizer.value && attendee && attendee->value && g_ascii_strcasecmp (organizer.value, attendee->value) != 0;

	e_cal_component_free_attendee_list (attendees);

	return res;
}
