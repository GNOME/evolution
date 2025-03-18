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

#define LIBICAL_GLIB_UNSTABLE_API 1
#include <libical-glib/libical-glib.h>
#undef LIBICAL_GLIB_UNSTABLE_API

#include <libsoup/soup.h>

#include <libedataserver/libedataserver.h>

#include "composer/e-msg-composer.h"
#include "libemail-engine/libemail-engine.h"

#include "calendar-config.h"
#include "comp-util.h"
#include "e-cal-data-model.h"

#include "itip-utils.h"

#define d(x)

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
 * Returns: a %NULL-terminated array of mail identity strings
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

		if (!e_util_identity_can_send (registry, source))
			continue;

		extension = e_source_get_extension (source, extension_name);

		name = e_source_mail_identity_get_name (extension);
		address = e_source_mail_identity_get_address (extension);

		if (address)
			g_ptr_array_add (identities, camel_internet_address_format_address (name, address));

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

					g_ptr_array_add (identities, camel_internet_address_format_address (alias_name, alias_address));
				}
			}

			g_object_unref (inet_address);
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
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), FALSE);
	g_return_val_if_fail (address != NULL, FALSE);

	return em_utils_address_is_user (registry, address, FALSE);
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
	ECalComponentOrganizer *organizer;
	const gchar *strip;
	gboolean user_org = FALSE;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), FALSE);

	if (!e_cal_component_has_organizer (comp) ||
		(!skip_cap_test && e_client_check_capability (
		E_CLIENT (cal_client), E_CAL_STATIC_CAPABILITY_NO_ORGANIZER)))
		return FALSE;

	organizer = e_cal_component_get_organizer (comp);
	if (organizer) {
		gchar *email = NULL;

		strip = e_cal_util_get_organizer_email (organizer);

		if (e_client_get_backend_property_sync (E_CLIENT (cal_client),
							E_CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS,
							&email, NULL, NULL) &&
				email && e_cal_util_email_addresses_equal (email, strip)) {
			e_cal_component_organizer_free (organizer);
			g_free (email);

			return TRUE;
		}

		g_free (email);

		if (e_client_check_capability (E_CLIENT (cal_client), E_CAL_STATIC_CAPABILITY_ORGANIZER_NOT_EMAIL_ADDRESS)) {
			e_cal_component_organizer_free (organizer);
			return FALSE;
		}

		user_org = itip_address_is_user (registry, strip);
	}

	e_cal_component_organizer_free (organizer);

	return user_org;
}

gboolean
itip_sentby_is_user (ESourceRegistry *registry,
                     ECalComponent *comp,
                     ECalClient *cal_client)
{
	ECalComponentOrganizer *organizer;
	const gchar *strip;
	gboolean user_sentby = FALSE;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), FALSE);

	if (!e_cal_component_has_organizer (comp) ||
		e_client_check_capability (
		E_CLIENT (cal_client), E_CAL_STATIC_CAPABILITY_NO_ORGANIZER))
		return FALSE;

	organizer = e_cal_component_get_organizer (comp);
	if (organizer && e_cal_component_organizer_get_sentby (organizer)) {
		strip = e_cal_util_strip_mailto (e_cal_component_organizer_get_sentby (organizer));
		user_sentby = itip_address_is_user (registry, strip);
	}

	e_cal_component_organizer_free (organizer);

	return user_sentby;
}

gboolean
itip_has_any_attendees (ECalComponent *comp)
{
	ECalComponentOrganizer *organizer;
	ECalComponentAttendee *attendee;
	GSList *attendees = NULL;
	const gchar *organizer_email;
	const gchar *attendee_email;
	gboolean res;

	g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), FALSE);

	if (!e_cal_component_has_attendees (comp))
		return FALSE;

	attendees = e_cal_component_get_attendees (comp);

	/* No attendee */
	if (!attendees)
		return FALSE;

	/* More than one attendee */
	if (attendees->next) {
		g_slist_free_full (attendees, e_cal_component_attendee_free);
		return TRUE;
	}

	/* Exactly one attendee, check if it's not the organizer */
	attendee = attendees->data;

	g_return_val_if_fail (attendee != NULL, FALSE);

	if (!e_cal_component_has_organizer (comp)) {
		g_slist_free_full (attendees, e_cal_component_attendee_free);
		return FALSE;
	}

	organizer = e_cal_component_get_organizer (comp);

	organizer_email = e_cal_util_get_organizer_email (organizer);
	attendee_email = e_cal_util_get_attendee_email (attendee);

	res = attendee_email && (!organizer_email ||
	      !e_cal_util_email_addresses_equal (attendee_email, organizer_email));

	g_slist_free_full (attendees, e_cal_component_attendee_free);
	e_cal_component_organizer_free (organizer);

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

		nomailto = e_cal_util_get_attendee_email (attendee);

		if (!nomailto || !*nomailto)
			continue;

		if ((address && e_cal_util_email_addresses_equal (nomailto, address)) ||
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

		nomailto = e_cal_util_strip_mailto (e_cal_component_attendee_get_sentby (attendee));
		if (!nomailto || !*nomailto)
			continue;

		if ((address && e_cal_util_email_addresses_equal (nomailto, address)) ||
		    (aliases && g_hash_table_contains (aliases, nomailto))) {
			return attendee;
		}
	}

	return NULL;
}

gboolean
itip_attendee_is_user (ESourceRegistry *registry,
		       ECalComponent *comp,
		       ECalClient *cal_client)
{
	ESource *source;
	GSList *attendees;
	ECalComponentAttendee *attendee = NULL;
	GList *list, *link;
	const gchar *extension_name;
	gchar *address = NULL;

	attendees = e_cal_component_get_attendees (comp);

	if (cal_client)
		e_client_get_backend_property_sync (
			E_CLIENT (cal_client),
			E_CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS,
			&address, NULL, NULL);

	if (address && *address) {
		attendee = get_attendee (attendees, address, NULL);

		if (attendee) {
			g_slist_free_full (attendees, e_cal_component_attendee_free);
			g_free (address);

			return TRUE;
		}

		attendee = get_attendee_if_attendee_sentby_is_user (attendees, address, NULL);

		if (attendee) {
			g_slist_free_full (attendees, e_cal_component_attendee_free);
			g_free (address);

			return TRUE;
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
			g_slist_free_full (attendees, e_cal_component_attendee_free);

			if (aliases)
				g_hash_table_destroy (aliases);
			g_free (address);

			g_list_free_full (list, g_object_unref);

			return TRUE;
		}

		/* If the account was not found in the attendees list, then
		 * let's check the 'sentby' fields of the attendees if we can
		 * find the account. */
		attendee = get_attendee_if_attendee_sentby_is_user (attendees, address, aliases);
		if (attendee) {
			g_slist_free_full (attendees, e_cal_component_attendee_free);

			if (aliases)
				g_hash_table_destroy (aliases);
			g_free (address);

			g_list_free_full (list, g_object_unref);

			return TRUE;
		}

		if (aliases)
			g_hash_table_destroy (aliases);
		g_free (address);
	}

	g_slist_free_full (attendees, e_cal_component_attendee_free);
	g_list_free_full (list, g_object_unref);

	return FALSE;
}

ECalComponentAttendee *
itip_dup_comp_attendee (ESourceRegistry *registry,
			ECalComponent *comp,
			ECalClient *cal_client,
			gboolean *out_is_sent_by)
{
	ESource *source;
	GSList *attendees;
	ECalComponentAttendee *attendee = NULL;
	GList *list, *link;
	const gchar *extension_name;
	gchar *address = NULL;

	if (out_is_sent_by)
		*out_is_sent_by = FALSE;

	attendees = e_cal_component_get_attendees (comp);

	if (cal_client)
		e_client_get_backend_property_sync (
			E_CLIENT (cal_client),
			E_CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS,
			&address, NULL, NULL);

	if (address != NULL && *address != '\0') {
		attendee = get_attendee (attendees, address, NULL);

		if (attendee) {
			attendees = g_slist_remove (attendees, attendee);

			g_slist_free_full (attendees, e_cal_component_attendee_free);
			g_free (address);

			return attendee;
		}

		attendee = get_attendee_if_attendee_sentby_is_user (attendees, address, NULL);

		if (attendee != NULL) {
			if (out_is_sent_by)
				*out_is_sent_by = TRUE;

			attendees = g_slist_remove (attendees, attendee);

			g_slist_free_full (attendees, e_cal_component_attendee_free);
			g_free (address);

			return attendee;
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
			attendees = g_slist_remove (attendees, attendee);

			g_slist_free_full (attendees, e_cal_component_attendee_free);

			if (aliases)
				g_hash_table_destroy (aliases);
			g_free (address);

			g_list_free_full (list, g_object_unref);

			return attendee;
		}

		/* If the account was not found in the attendees list, then
		 * let's check the 'sentby' fields of the attendees if we can
		 * find the account. */
		attendee = get_attendee_if_attendee_sentby_is_user (attendees, address, aliases);
		if (attendee) {
			if (out_is_sent_by)
				*out_is_sent_by = TRUE;

			attendees = g_slist_remove (attendees, attendee);

			g_slist_free_full (attendees, e_cal_component_attendee_free);

			if (aliases)
				g_hash_table_destroy (aliases);
			g_free (address);

			g_list_free_full (list, g_object_unref);

			return attendee;
		}

		if (aliases)
			g_hash_table_destroy (aliases);
		g_free (address);
	}

	g_list_free_full (list, g_object_unref);
	g_slist_free_full (attendees, e_cal_component_attendee_free);

	return NULL;
}

gchar *
itip_get_comp_attendee (ESourceRegistry *registry,
                        ECalComponent *comp,
                        ECalClient *cal_client)
{
	ECalComponentAttendee *attendee;
	gboolean is_sent_by = FALSE;
	gchar *address = NULL;

	attendee = itip_dup_comp_attendee (registry, comp, cal_client, &is_sent_by);
	if (attendee) {
		if (is_sent_by)
			address = g_strdup (e_cal_util_strip_mailto (e_cal_component_attendee_get_sentby (attendee)));
		else
			address = g_strdup (e_cal_util_get_attendee_email (attendee));

		e_cal_component_attendee_free (attendee);
	}

	if (!address) {
		/* We could not find the attendee in the component, so just give
		 * the default account address if the email address is not set in
		 * the backend. */
		/* FIXME do we have a better way ? */
		e_cal_util_get_default_name_and_address (registry, NULL, &address);
	}

	if (!address)
		address = g_strdup ("");

	return address;
}

static gchar *
get_label (ICalTime *tt,
           gboolean use_24_hour_format)
{
	gchar buffer[1000];
	struct tm tmp_tm;

	tmp_tm = e_cal_util_icaltime_to_tm (tt);

	e_time_format_date_and_time (
		&tmp_tm, use_24_hour_format, FALSE, FALSE, buffer, 1000);

	return g_strdup (buffer);
}

typedef struct {
	GHashTable *tzids;
	ICalComponent *icomp;
	ECalClient *client;
	ICalComponent *zones;
	ICalTime *clamp_vtimezone_from;
	ICalTime *clamp_vtimezone_to;
} ItipUtilTZData;

static void
foreach_tzid_callback (ICalParameter *param,
                       gpointer data)
{
	ItipUtilTZData *tz_data = data;
	ICalTimezone *zone = NULL;
	ICalComponent *vtimezone_comp, *tzcomp = NULL;
	const gchar *tzid, *location;
	gchar *tzid_dup = NULL;

	/* Get the TZID string from the parameter. */
	tzid = i_cal_parameter_get_tzid (param);
	if (!tzid)
		return;

	if (g_hash_table_contains (tz_data->tzids, tzid)) {
		location = g_hash_table_lookup (tz_data->tzids, tzid);
		if (location)
			i_cal_parameter_set_tzid (param, location);
		return;
	}

	/* Look for the timezone */
	if (tz_data->zones != NULL)
		zone = i_cal_component_get_timezone (tz_data->zones, tzid);
	if (zone == NULL)
		zone = i_cal_timezone_get_builtin_timezone_from_tzid (tzid);
	if (zone == NULL && tz_data->client != NULL &&
	    !e_cal_client_get_timezone_sync (tz_data->client, tzid, &zone, NULL, NULL))
		zone = NULL;
	if (zone == NULL)
		return;

	/* Convert it to a string and add it to the hash. */
	vtimezone_comp = i_cal_timezone_get_component (zone);
	if (!vtimezone_comp)
		return;

	location = i_cal_timezone_get_location (zone);
	if (location && *location) {
		ICalProperty *prop;

		tzid_dup = g_strdup (tzid);
		tzid = tzid_dup;

		/* This frees the original 'tzid' */
		i_cal_parameter_set_tzid (param, location);

		if (g_hash_table_contains (tz_data->tzids, location)) {
			g_object_unref (vtimezone_comp);
			g_free (tzid_dup);
			return;
		}

		tzcomp = i_cal_component_clone (vtimezone_comp);
		prop = i_cal_component_get_first_property (tzcomp, I_CAL_TZID_PROPERTY);
		if (prop) {
			i_cal_property_set_tzid (prop, location);
			g_object_unref (prop);
		}

		g_hash_table_insert (tz_data->tzids, g_strdup (location), NULL);
	} else {
		location = NULL;
	}

	if (!tzcomp)
		tzcomp = i_cal_component_clone (vtimezone_comp);

	if (tz_data->clamp_vtimezone_from)
		e_cal_util_clamp_vtimezone (tzcomp, tz_data->clamp_vtimezone_from, tz_data->clamp_vtimezone_to);

	i_cal_component_take_component (tz_data->icomp, tzcomp);
	g_hash_table_insert (tz_data->tzids, tzid_dup ? tzid_dup : g_strdup (tzid), g_strdup (location));
	g_object_unref (vtimezone_comp);
}

static ICalComponent *
comp_toplevel_with_zones (ICalPropertyMethod method,
			  const GSList *ecomps,
			  ECalClient *cal_client,
			  ICalComponent *zones)
{
	ICalComponent *top_level, *icomp;
	ICalProperty *prop;
	ItipUtilTZData tz_data;
	ECalComponentDateTime *dt;
	GSList *link;

	g_return_val_if_fail (ecomps != NULL, NULL);

	top_level = e_cal_util_new_top_level ();

	prop = i_cal_property_new_method (method);
	i_cal_component_take_property (top_level, prop);

	tz_data.tzids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	tz_data.icomp = top_level;
	tz_data.client = cal_client;
	tz_data.zones = zones;
	tz_data.clamp_vtimezone_from = NULL;
	tz_data.clamp_vtimezone_to = NULL;

	dt = e_cal_component_get_dtstart (ecomps->data);
	if (dt && e_cal_component_datetime_get_value (dt))
		tz_data.clamp_vtimezone_from = i_cal_time_clone (e_cal_component_datetime_get_value (dt));
	e_cal_component_datetime_free (dt);

	if (tz_data.clamp_vtimezone_from && !ecomps->next &&
	    !e_cal_component_has_rrules (ecomps->data)) {
		dt = e_cal_component_get_dtend (ecomps->data);

		if (dt && e_cal_component_datetime_get_value (dt))
			tz_data.clamp_vtimezone_to = i_cal_time_clone (e_cal_component_datetime_get_value (dt));

		if (!tz_data.clamp_vtimezone_to)
			tz_data.clamp_vtimezone_to = g_object_ref (tz_data.clamp_vtimezone_from);

		e_cal_component_datetime_free (dt);
	}

	for (link = (GSList *) ecomps; link; link = g_slist_next (link)) {
		icomp = e_cal_component_get_icalcomponent (link->data);
		icomp = i_cal_component_clone (icomp);

		i_cal_component_foreach_tzid (icomp, foreach_tzid_callback, &tz_data);

		i_cal_component_take_component (top_level, icomp);
	}

	g_hash_table_destroy (tz_data.tzids);
	g_clear_object (&tz_data.clamp_vtimezone_from);
	g_clear_object (&tz_data.clamp_vtimezone_to);

	return top_level;
}

static gboolean
users_has_attendee (const GSList *users,
                    const gchar *address)
{
	const GSList *l;

	for (l = users; l != NULL; l = l->next) {
		if (e_cal_util_email_addresses_equal (address, l->data))
			return TRUE;
	}

	return FALSE;
}

static gchar *
comp_from (ICalPropertyMethod method,
           ECalComponent *comp,
           ESourceRegistry *registry,
	   gchar **from_name)
{
	ECalComponentOrganizer *organizer;
	ECalComponentAttendee *attendee;
	GSList *attendees;
	const gchar *email_address;
	gchar *from;
	gchar *sender = NULL;

	switch (method) {
	case I_CAL_METHOD_PUBLISH:
	case I_CAL_METHOD_REQUEST:
		return itip_get_comp_attendee (registry, comp, NULL);

	case I_CAL_METHOD_REPLY:
		sender = itip_get_comp_attendee (registry, comp, NULL);
		if (sender != NULL)
			return sender;
		if (!e_cal_component_has_attendees (comp))
			return NULL;
		/* coverity[fallthrough] */
		/* falls through */

	case I_CAL_METHOD_CANCEL:

	case I_CAL_METHOD_ADD:

		organizer = e_cal_component_get_organizer (comp);
		email_address = e_cal_util_get_organizer_email (organizer);

		if (!email_address) {
			e_cal_component_organizer_free (organizer);
			e_notice (
				NULL, GTK_MESSAGE_ERROR,
				_("An organizer must be set."));
			return NULL;
		}
		if (from_name)
			*from_name = g_strdup (e_cal_component_organizer_get_cn (organizer));
		from = g_strdup (email_address);
		e_cal_component_organizer_free (organizer);
		return from;

	default:
		attendees = e_cal_component_get_attendees (comp);
		if (!attendees)
			return NULL;

		attendee = attendees->data;
		email_address = e_cal_util_get_attendee_email (attendee);

		if (email_address) {
			from = g_strdup (email_address);
			if (from_name)
				*from_name = g_strdup (e_cal_component_attendee_get_cn (attendee));
		} else
			from = NULL;
		g_slist_free_full (attendees, e_cal_component_attendee_free);

		return from;
	}
}

static EDestination **
comp_to_list (ESourceRegistry *registry,
              ICalPropertyMethod method,
              ECalComponent *comp,
              const GSList *users,
              gboolean reply_all,
              const GSList *only_attendees)
{
	ECalComponentOrganizer *organizer;
	GSList *attendees, *l;
	GPtrArray *array = NULL;
	EDestination *destination;
	gint len;
	gchar *sender = NULL;
	const gchar *organizer_email;
	const gchar *attendee_email;

	union {
		gpointer *pdata;
		EDestination **destinations;
	} convert;

	switch (method) {
	case I_CAL_METHOD_REQUEST:
	case I_CAL_METHOD_CANCEL:
		attendees = e_cal_component_get_attendees (comp);
		len = g_slist_length (attendees);
		if (len <= 0) {
			g_slist_free_full (attendees, e_cal_component_attendee_free);
			e_notice (
				NULL, GTK_MESSAGE_ERROR,
				_("At least one attendee is necessary"));
			return NULL;
		}

		organizer = e_cal_component_get_organizer (comp);
		organizer_email = e_cal_util_get_organizer_email (organizer);

		if (!organizer_email) {
			g_slist_free_full (attendees, e_cal_component_attendee_free);
			e_cal_component_organizer_free (organizer);
			e_notice (
				NULL, GTK_MESSAGE_ERROR,
				_("An organizer must be set."));
			return NULL;
		}

		array = g_ptr_array_new ();

		sender = itip_get_comp_attendee (registry, comp, NULL);

		for (l = attendees; l != NULL; l = l->next) {
			ECalComponentAttendee *att = l->data;

			attendee_email = e_cal_util_get_attendee_email (att);

			if (!attendee_email)
				continue;

			if (users_has_attendee (users, attendee_email))
				continue;
			else if (e_cal_component_attendee_get_sentby (att) &&
				users_has_attendee (users, e_cal_component_attendee_get_sentby (att)))
				continue;
			else if (e_cal_util_email_addresses_equal (attendee_email, organizer_email))
				continue;
			else if (e_cal_component_attendee_get_sentby (att) &&
				 e_cal_component_organizer_get_sentby (organizer) &&
				 e_cal_util_email_addresses_equal (e_cal_component_attendee_get_sentby (att), e_cal_component_organizer_get_sentby (organizer)))
				continue;
			else if (e_cal_util_email_addresses_equal (attendee_email, sender))
				continue;
			else if (e_cal_component_attendee_get_partstat (att) == I_CAL_PARTSTAT_DELEGATED &&
				 !e_cal_component_attendee_get_rsvp (att) &&
				 method == I_CAL_METHOD_REQUEST) {
					const gchar *delegatedto;

					delegatedto = e_cal_component_attendee_get_delegatedto (att);
					if (delegatedto && *delegatedto)
						continue;
			} else if (only_attendees &&
				!cal_comp_util_have_in_new_attendees (only_attendees, attendee_email))
				continue;

			destination = e_destination_new ();
			if (e_cal_component_attendee_get_cn (att))
				e_destination_set_name (destination, e_cal_component_attendee_get_cn (att));
			e_destination_set_email (destination, attendee_email);
			g_ptr_array_add (array, destination);
		}
		g_free (sender);
		g_slist_free_full (attendees, e_cal_component_attendee_free);
		e_cal_component_organizer_free (organizer);
		break;

	case I_CAL_METHOD_REPLY:

		if (reply_all) {
			attendees = e_cal_component_get_attendees (comp);
			len = g_slist_length (attendees);

			if (len <= 0)
				return NULL;

			array = g_ptr_array_new ();

			sender = itip_get_comp_attendee (registry, comp, NULL);

			organizer = e_cal_component_get_organizer (comp);
			organizer_email = e_cal_util_get_organizer_email (organizer);

			if (organizer_email &&
			    (!sender || !e_cal_util_email_addresses_equal (organizer_email, sender))) {
				destination = e_destination_new ();
				e_destination_set_email (destination, organizer_email);
				if (e_cal_component_organizer_get_cn (organizer))
					e_destination_set_name (destination, e_cal_component_organizer_get_cn (organizer));
				g_ptr_array_add (array, destination);
			}

			for (l = attendees; l != NULL; l = l->next) {
				ECalComponentAttendee *att = l->data;
				ICalParameterCutype cutype;

				attendee_email = e_cal_util_get_attendee_email (att);

				if (!attendee_email)
					continue;

				cutype = e_cal_component_attendee_get_cutype (att);

				if (cutype != I_CAL_CUTYPE_INDIVIDUAL &&
				    cutype != I_CAL_CUTYPE_GROUP &&
				    cutype != I_CAL_CUTYPE_UNKNOWN)
					continue;
				else if (only_attendees &&
					!cal_comp_util_have_in_new_attendees (only_attendees, attendee_email))
					continue;
				else if (organizer_email &&
					 e_cal_util_email_addresses_equal (attendee_email, organizer_email))
					continue;
				else if (sender && e_cal_util_email_addresses_equal (attendee_email, sender))
					continue;

				destination = e_destination_new ();
				if (e_cal_component_attendee_get_cn (att))
					e_destination_set_name (destination, e_cal_component_attendee_get_cn (att));
				e_destination_set_email (destination, attendee_email);
				g_ptr_array_add (array, destination);
			}

			g_free (sender);
			g_slist_free_full (attendees, e_cal_component_attendee_free);
			e_cal_component_organizer_free (organizer);

		} else {
			array = g_ptr_array_new ();

			destination = e_destination_new ();
			organizer = e_cal_component_get_organizer (comp);
			organizer_email = e_cal_util_get_organizer_email (organizer);
			if (organizer && e_cal_component_organizer_get_cn (organizer))
				e_destination_set_name (destination, e_cal_component_organizer_get_cn (organizer));
			if (organizer_email)
				e_destination_set_email (destination, organizer_email);
			g_ptr_array_add (array, destination);

			e_cal_component_organizer_free (organizer);
		}
		break;

	case I_CAL_METHOD_ADD:
	case I_CAL_METHOD_REFRESH:
	case I_CAL_METHOD_COUNTER:
	case I_CAL_METHOD_DECLINECOUNTER:
		organizer = e_cal_component_get_organizer (comp);
		organizer_email = e_cal_util_get_organizer_email (organizer);

		if (!organizer_email) {
			e_cal_component_organizer_free (organizer);
			e_notice (
				NULL, GTK_MESSAGE_ERROR,
				_("An organizer must be set."));
			return NULL;
		}

		array = g_ptr_array_new ();

		destination = e_destination_new ();
		if (e_cal_component_organizer_get_cn (organizer))
			e_destination_set_name (destination, e_cal_component_organizer_get_cn (organizer));
		e_destination_set_email (destination, organizer_email);
		g_ptr_array_add (array, destination);

		/* send the status to delegatee to the delegate also*/
		attendees = e_cal_component_get_attendees (comp);
		sender = itip_get_comp_attendee (registry, comp, NULL);

		for (l = attendees; l != NULL; l = l->next) {
			ECalComponentAttendee *att = l->data;
			ICalParameterCutype cutype;

			attendee_email = e_cal_util_get_attendee_email (att);

			if (!attendee_email)
				continue;

			cutype = e_cal_component_attendee_get_cutype (att);

			if (cutype != I_CAL_CUTYPE_INDIVIDUAL &&
			    cutype != I_CAL_CUTYPE_GROUP &&
			    cutype != I_CAL_CUTYPE_UNKNOWN)
				continue;

			if (sender && (
			    e_cal_util_email_addresses_equal (attendee_email, sender) ||
			    (e_cal_component_attendee_get_sentby (att) &&
			    e_cal_util_email_addresses_equal (e_cal_util_strip_mailto (e_cal_component_attendee_get_sentby (att)), sender)))) {
				const gchar *delegatedfrom;

				delegatedfrom = e_cal_component_attendee_get_delegatedfrom (att);

				if (!delegatedfrom || !*delegatedfrom)
					break;

				destination = e_destination_new ();
				e_destination_set_email (
					destination, e_cal_util_strip_mailto (delegatedfrom));
				g_ptr_array_add (array, destination);
			}

		}
		g_slist_free_full (attendees, e_cal_component_attendee_free);
		e_cal_component_organizer_free (organizer);

		break;
	case I_CAL_METHOD_PUBLISH:
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
              ICalPropertyMethod method,
              ECalComponent *comp,
	      gboolean is_reply)
{
	ECalComponentText *caltext;
	const gchar *description, *prefix = NULL;
	GSList *alist, *l;
	gchar *subject;
	gchar *sender;
	ECalComponentAttendee *a = NULL;

	caltext = e_cal_component_dup_summary_for_locale (comp, NULL);
	if (caltext && e_cal_component_text_get_value (caltext)) {
		description = e_cal_component_text_get_value (caltext);
	} else {
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
	case I_CAL_METHOD_PUBLISH:
	case I_CAL_METHOD_REQUEST:
		/* FIXME: If this is an update to a previous
		 * PUBLISH or REQUEST, then
			prefix = U_("Updated");
		 */
		break;

	case I_CAL_METHOD_REPLY:
		alist = e_cal_component_get_attendees (comp);
		sender = itip_get_comp_attendee (registry, comp, NULL);
		if (sender) {
			for (l = alist; l != NULL; l = l->next) {
				const gchar *value, *sentby;

				a = l->data;
				value = e_cal_util_get_attendee_email (a);
				sentby = e_cal_component_attendee_get_sentby (a);

				if ((sender && *sender) && (
				    (value && e_cal_util_email_addresses_equal (value, sender)) ||
				    (sentby && e_cal_util_email_addresses_equal (sentby, sender))))
					break;
			}
			g_free (sender);
		}

		if (a != NULL) {

			switch (e_cal_component_attendee_get_partstat (a)) {
			case I_CAL_PARTSTAT_ACCEPTED:
				/* Translators: This is part of the subject
				 * line of a meeting request or update email.
				 * The full subject line would be:
				 * "Accepted: Meeting Name". */
				prefix = C_("Meeting", "Accepted");
				break;
			case I_CAL_PARTSTAT_TENTATIVE:
				/* Translators: This is part of the subject
				 * line of a meeting request or update email.
				 * The full subject line would be:
				 * "Tentatively Accepted: Meeting Name". */
				prefix = C_("Meeting", "Tentatively Accepted");
				break;
			case I_CAL_PARTSTAT_DECLINED:
				/* Translators: This is part of the subject
				 * line of a meeting request or update email.
				 * The full subject line would be:
				 * "Declined: Meeting Name". */
				prefix = C_("Meeting", "Declined");
				break;
			case I_CAL_PARTSTAT_DELEGATED:
				/* Translators: This is part of the subject
				 * line of a meeting request or update email.
				 * The full subject line would be:
				 * "Delegated: Meeting Name". */
				prefix = C_("Meeting", "Delegated");
				break;
			default:
				break;
			}
			g_slist_free_full (alist, e_cal_component_attendee_free);
		}
		break;

	case I_CAL_METHOD_ADD:
		/* Translators: This is part of the subject line of a
		 * meeting request or update email.  The full subject
		 * line would be: "Updated: Meeting Name". */
		prefix = C_("Meeting", "Updated");
		break;

	case I_CAL_METHOD_CANCEL:
		/* Translators: This is part of the subject line of a
		 * meeting request or update email.  The full subject
		 * line would be: "Cancel: Meeting Name". */
		prefix = C_("Meeting", "Cancel");
		break;

	case I_CAL_METHOD_REFRESH:
		/* Translators: This is part of the subject line of a
		 * meeting request or update email.  The full subject
		 * line would be: "Refresh: Meeting Name". */
		prefix = C_("Meeting", "Refresh");
		break;

	case I_CAL_METHOD_COUNTER:
		/* Translators: This is part of the subject line of a
		 * meeting request or update email.  The full subject
		 * line would be: "Counter-proposal: Meeting Name". */
		prefix = C_("Meeting", "Counter-proposal");
		break;

	case I_CAL_METHOD_DECLINECOUNTER:
		/* Translators: This is part of the subject line of a
		 * meeting request or update email.  The full subject
		 * line would be: "Declined: Meeting Name". */
		prefix = C_("Meeting", "Declined");
		break;

	default:
		/* Do not localize this string; it also ignores "composer-use-localized-fwd-re" setting */
		if (is_reply)
			prefix = "Re";
		break;
	}

	if (prefix != NULL) {
		/* Translators: This constructs a meeting subject,
		   where the first "%s" is prefix, like "Accepted" or "Re",
		   and the second "%s" is the meeting summary. The full
		   subject line would be: "Accepted: Meeting Name". */
		subject = g_strdup_printf (C_("Meeting", "%s: %s"), prefix, description);
	} else {
		subject = g_strdup (description);
	}

	e_cal_component_text_free (caltext);

	return subject;
}

static gchar *
comp_content_type (ECalComponent *comp,
                   ICalPropertyMethod method,
		   gboolean include_name)
{
	const gchar *name;

	if (!include_name) {
		return g_strdup_printf (
			"text/calendar; charset=utf-8; method=%s",
			i_cal_property_method_to_string (method));
	}

	if (e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_FREEBUSY)
		name = "freebusy.ifb";
	else
		name = "calendar.ics";

	return g_strdup_printf (
		"text/calendar; name=\"%s\"; charset=utf-8; method=%s",
		name, i_cal_property_method_to_string (method));
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
	ECalComponentDateTime *dt;
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
		dt = e_cal_component_get_dtstart (comp);
		if (dt && e_cal_component_datetime_get_value (dt))
			start = get_label (e_cal_component_datetime_get_value (dt), use_24_hour_format);
		e_cal_component_datetime_free (dt);

		dt = e_cal_component_get_dtend (comp);
		if (dt && e_cal_component_datetime_get_value (dt))
			end = get_label (e_cal_component_datetime_get_value (dt), use_24_hour_format);
		e_cal_component_datetime_free (dt);

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
comp_server_send_sync (ICalPropertyMethod method,
		       const GSList *ecomps,
		       ECalClient *cal_client,
		       ICalComponent *zones,
		       GSList **users,
		       GCancellable *cancellable,
		       GError **error)
{
	ICalComponent *top_level, *returned_icomp = NULL;
	gboolean retval = TRUE;
	GError *local_error = NULL;

	top_level = comp_toplevel_with_zones (method, ecomps, cal_client, zones);
	d (printf ("itip-utils.c: comp_server_send_sync: calling e_cal_send_objects... \n"));

	e_cal_client_send_objects_sync (
		cal_client, top_level, E_CAL_OPERATION_FLAG_NONE, users,
		&returned_icomp, cancellable, &local_error);

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

	g_clear_object (&returned_icomp);
	g_clear_object (&top_level);

	return retval;
}

static gboolean
comp_limit_attendees (ESourceRegistry *registry,
                      ECalComponent *comp)
{
	ICalComponent *icomp;
	ICalProperty *prop;
	gboolean found = FALSE, match = FALSE;
	GSList *l, *list = NULL;

	icomp = e_cal_component_get_icalcomponent (comp);

	for (prop = i_cal_component_get_first_property (icomp, I_CAL_ATTENDEE_PROPERTY);
	     prop != NULL;
	     g_object_unref (prop), prop = i_cal_component_get_next_property (icomp, I_CAL_ATTENDEE_PROPERTY)) {
		gchar *attendee;
		gchar *attendee_text;

		/* If we've already found something, just erase the rest */
		if (found) {
			list = g_slist_prepend (list, g_object_ref (prop));
			continue;
		}

		attendee = i_cal_property_get_value_as_string (prop);
		if (!attendee)
			continue;

		attendee_text = g_strdup (e_cal_util_strip_mailto (attendee));
		g_free (attendee);
		attendee_text = g_strstrip (attendee_text);
		found = match = itip_address_is_user (registry, attendee_text);

		if (!found) {
			ICalParameter *param;

			param = i_cal_property_get_first_parameter (prop, I_CAL_SENTBY_PARAMETER);
			if (param) {
				const gchar *attendee_sentby;
				gchar *attendee_sentby_text;

				attendee_sentby = i_cal_parameter_get_sentby (param);
				attendee_sentby = e_cal_util_strip_mailto (attendee_sentby);
				attendee_sentby_text = g_strstrip (g_strdup (attendee_sentby));
				found = match = itip_address_is_user (
					registry, attendee_sentby_text);

				g_free (attendee_sentby_text);
				g_object_unref (param);
			}
		}

		g_free (attendee_text);

		if (!match)
			list = g_slist_prepend (list, g_object_ref (prop));
	}

	for (l = list; l != NULL; l = l->next) {
		prop = l->data;

		i_cal_component_remove_property (icomp, prop);
		g_object_unref (prop);
	}
	g_slist_free (list);

	return found;
}

static void
comp_sentby (ECalComponent *comp,
             ECalClient *cal_client,
             ESourceRegistry *registry)
{
	ECalComponentOrganizer *organizer;
	GSList * attendees, *l;
	const gchar *organizer_email;
	gchar *name = NULL;
	gchar *address = NULL;
	gchar *user;

	e_cal_util_get_default_name_and_address (registry, &name, &address);

	organizer = e_cal_component_get_organizer (comp);
	organizer_email = e_cal_util_get_organizer_email (organizer);

	if (!organizer_email && name != NULL && address != NULL) {
		gchar *tmp;

		e_cal_component_organizer_free (organizer);

		tmp = g_strdup_printf ("mailto:%s", address);
		organizer = e_cal_component_organizer_new_full (tmp, NULL, name, NULL);

		e_cal_component_set_organizer (comp, organizer);

		e_cal_component_organizer_free (organizer);

		g_free (tmp);
		g_free (name);
		g_free (address);
		return;
	}

	attendees = e_cal_component_get_attendees (comp);
	user = itip_get_comp_attendee (registry, comp, cal_client);
	for (l = attendees; l && user; l = l->next) {
		ECalComponentAttendee *a = l->data;
		const gchar *value, *sentby;

		if (!a)
			continue;

		value = e_cal_util_get_attendee_email (a);

		sentby = e_cal_component_attendee_get_sentby (a);
		if (sentby)
			sentby = e_cal_util_strip_mailto (sentby);

		if ((value && e_cal_util_email_addresses_equal (value, user)) ||
		    (sentby && e_cal_util_email_addresses_equal (sentby, user))) {
			g_slist_free_full (attendees, e_cal_component_attendee_free);
			e_cal_component_organizer_free (organizer);
			g_free (user);
			g_free (name);
			g_free (address);
			return;
		}
	}

	g_slist_free_full (attendees, e_cal_component_attendee_free);
	g_free (user);

	if (!itip_organizer_is_user (registry, comp, cal_client) &&
	    !itip_sentby_is_user (registry, comp, cal_client) &&
	    address && organizer) {
		ECalComponentOrganizer *sentbyorg;
		gchar *sentby;

		sentby = g_strdup_printf ("mailto:%s", address);
		sentbyorg = e_cal_component_organizer_copy (organizer);

		e_cal_component_organizer_set_sentby (sentbyorg, sentby);

		e_cal_component_set_organizer (comp, sentbyorg);

		e_cal_component_organizer_free (sentbyorg);
		g_free (sentby);
	}

	g_free (name);
	g_free (address);
	e_cal_component_organizer_free (organizer);
}

static ECalComponent *
comp_minimal (ESourceRegistry *registry,
              ECalComponent *comp,
              gboolean attendee)
{
	ECalComponent *clone;
	ECalComponentOrganizer *organizer;
	ECalComponentRange *recur_id;
	ICalComponent *icomp, *icomp_clone;
	ICalProperty *prop;
	ICalTime *itt;
	const gchar *uid;
	const gchar *organizer_email;
	GSList *comments;

	clone = e_cal_component_new ();
	e_cal_component_set_new_vtype (clone, e_cal_component_get_vtype (comp));

	if (attendee) {
		GSList *attendees;

		attendees = e_cal_component_get_attendees (comp);
		e_cal_component_set_attendees (clone, attendees);

		g_slist_free_full (attendees, e_cal_component_attendee_free);

		if (!comp_limit_attendees (registry, clone)) {
			e_notice (
				NULL, GTK_MESSAGE_ERROR,
				_("You must be an attendee of the event."));
			goto error;
		}
	}

	itt = i_cal_time_new_from_timet_with_zone (time (NULL), FALSE, i_cal_timezone_get_utc_timezone ());
	e_cal_component_set_dtstamp (clone, itt);
	g_clear_object (&itt);

	organizer = e_cal_component_get_organizer (comp);
	organizer_email = e_cal_util_get_organizer_email (organizer);
	if (!organizer_email) {
		e_cal_component_organizer_free (organizer);
		goto error;
	}
	e_cal_component_set_organizer (clone, organizer);
	e_cal_component_organizer_free (organizer);

	uid = e_cal_component_get_uid (comp);
	e_cal_component_set_uid (clone, uid);

	comments = e_cal_component_get_comments (comp);
	if (g_slist_length (comments) <= 1) {
		e_cal_component_set_comments (clone, comments);
	} else {
		GSList *l = comments;

		comments = g_slist_remove_link (comments, l);
		e_cal_component_set_comments (clone, l);
		g_slist_free_full (l, e_cal_component_text_free);
	}
	g_slist_free_full (comments, e_cal_component_text_free);

	recur_id = e_cal_component_get_recurid (comp);
	if (recur_id)
		e_cal_component_set_recurid (clone, recur_id);
	e_cal_component_range_free (recur_id);

	icomp = e_cal_component_get_icalcomponent (comp);
	icomp_clone = e_cal_component_get_icalcomponent (clone);
	for (prop = i_cal_component_get_first_property (icomp, I_CAL_X_PROPERTY);
	     prop != NULL;
	     g_object_unref (prop), prop = i_cal_component_get_next_property (icomp, I_CAL_X_PROPERTY))
	{
		ICalProperty *p;

		p = i_cal_property_clone (prop);
		i_cal_component_take_property (icomp_clone, p);
	}

	return clone;

 error:
	g_object_unref (clone);
	return NULL;
}

static void
strip_x_microsoft_props (ECalComponent *comp)
{
	GSList *lst = NULL, *l;
	ICalComponent *icomp;
	ICalProperty *prop;

	g_return_if_fail (comp != NULL);

	icomp = e_cal_component_get_icalcomponent (comp);
	g_return_if_fail (icomp != NULL);

	for (prop = i_cal_component_get_first_property (icomp, I_CAL_X_PROPERTY);
	     prop;
	     g_object_unref (prop), prop = i_cal_component_get_next_property (icomp, I_CAL_X_PROPERTY)) {
		const gchar *x_name = i_cal_property_get_x_name (prop);

		if (x_name && g_ascii_strncasecmp (x_name, "X-MICROSOFT-", 12) == 0)
			lst = g_slist_prepend (lst, g_object_ref (prop));
	}

	for (l = lst; l != NULL; l = l->next) {
		prop = l->data;

		i_cal_component_remove_property (icomp, prop);
	}

	g_slist_free_full (lst, g_object_unref);
}

/* https://tools.ietf.org/html/rfc6638#section-7.3 */
static void
remove_schedule_status_parameters (ECalComponent *comp)
{
	ICalComponent *icomp;
	ICalProperty *prop;

	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	icomp = e_cal_component_get_icalcomponent (comp);

	if (!icomp)
		return;

	for (prop = i_cal_component_get_first_property (icomp, I_CAL_ORGANIZER_PROPERTY);
	     prop;
	     g_object_unref (prop), prop = i_cal_component_get_next_property (icomp, I_CAL_ORGANIZER_PROPERTY)) {
		i_cal_property_remove_parameter_by_kind (prop, I_CAL_SCHEDULESTATUS_PARAMETER);
	}

	for (prop = i_cal_component_get_first_property (icomp, I_CAL_ATTENDEE_PROPERTY);
	     prop;
	     g_object_unref (prop), prop = i_cal_component_get_next_property (icomp, I_CAL_ATTENDEE_PROPERTY)) {
		i_cal_property_remove_parameter_by_kind (prop, I_CAL_SCHEDULESTATUS_PARAMETER);
	}
}

static ECalComponent *
comp_compliant_one (ESourceRegistry *registry,
		    ICalPropertyMethod method,
		    ECalComponent *comp,
		    ECalClient *client,
		    ICalComponent *zones,
		    ICalTimezone *default_zone,
		    gboolean strip_alarms)
{
	ECalComponent *clone, *temp_clone;
	ICalTime *itt;

	clone = e_cal_component_clone (comp);
	itt = i_cal_time_new_from_timet_with_zone (time (NULL), FALSE, i_cal_timezone_get_utc_timezone ());
	e_cal_component_set_dtstamp (clone, itt);
	g_clear_object (&itt);

	/* Make UNTIL date a datetime in a simple recurrence */
	if (e_cal_component_has_recurrences (clone) &&
	    e_cal_component_has_simple_recurrence (clone)) {
		GSList *rrule_list;
		ICalRecurrence *rt;

		rrule_list = e_cal_component_get_rrules (clone);
		rt = rrule_list->data;

		itt = i_cal_recurrence_get_until (rt);
		if (itt && !i_cal_time_is_null_time (itt) && i_cal_time_is_date (itt)) {
			ECalComponentDateTime *dt;
			ICalTime *dtvalue;
			ICalTimezone *from_zone = NULL, *to_zone;

			dt = e_cal_component_get_dtstart (clone);
			dtvalue = dt ? e_cal_component_datetime_get_value (dt) : NULL;

			if (!dtvalue || i_cal_time_is_date (dtvalue)) {
				from_zone = default_zone;
			} else if (!e_cal_component_datetime_get_tzid (dt)) {
				from_zone = i_cal_timezone_get_utc_timezone ();
			} else {
				if (zones != NULL)
					from_zone = i_cal_component_get_timezone (zones, e_cal_component_datetime_get_tzid (dt));
				if (from_zone == NULL)
					from_zone = i_cal_timezone_get_builtin_timezone_from_tzid (e_cal_component_datetime_get_tzid (dt));
				if (from_zone == NULL && client != NULL)
					/* FIXME Error checking */
					if (!e_cal_client_get_timezone_sync (client, e_cal_component_datetime_get_tzid (dt), &from_zone, NULL, NULL))
						from_zone = NULL;
			}

			to_zone = i_cal_timezone_get_utc_timezone ();

			i_cal_time_set_time (itt,
				i_cal_time_get_hour (dtvalue),
				i_cal_time_get_minute (dtvalue),
				i_cal_time_get_second (dtvalue));
			i_cal_time_set_is_date (itt, FALSE);

			i_cal_time_convert_timezone (itt, from_zone, to_zone);
			i_cal_time_set_timezone (itt, to_zone);

			i_cal_recurrence_set_until (rt, itt);

			e_cal_component_datetime_free (dt);
			e_cal_component_set_rrules (clone, rrule_list);
			e_cal_component_abort_sequence (clone);
		}

		g_slist_free_full (rrule_list, g_object_unref);
		g_clear_object (&itt);
	}

	/* We delete incoming alarms if requested, even this helps with outlook */
	if (strip_alarms) {
		e_cal_component_remove_all_alarms (clone);
	} else {
		/* Always strip procedure alarms, because of security */
		GSList *uids, *link;

		uids = e_cal_component_get_alarm_uids (clone);

		for (link = uids; link; link = g_slist_next (link)) {
			ECalComponentAlarm *alarm;

			alarm = e_cal_component_get_alarm (clone, link->data);
			if (alarm) {
				ECalComponentAlarmAction action;

				action = e_cal_component_alarm_get_action (alarm);
				e_cal_component_alarm_free (alarm);

				if (action == E_CAL_COMPONENT_ALARM_PROCEDURE)
					e_cal_component_remove_alarm (clone, link->data);
			}
		}

		g_slist_free_full (uids, g_free);
	}

	strip_x_microsoft_props (clone);

	/* Strip X-LIC-ERROR stuff */
	e_cal_component_strip_errors (clone);

	/* Comply with itip spec */
	switch (method) {
	case I_CAL_METHOD_PUBLISH:
		comp_sentby (clone, client, registry);
		e_cal_component_set_attendees (clone, NULL);
		break;
	case I_CAL_METHOD_REQUEST:
		comp_sentby (clone, client, registry);
		break;
	case I_CAL_METHOD_CANCEL:
		comp_sentby (clone, client, registry);
		break;
	case I_CAL_METHOD_REPLY:
		break;
	case I_CAL_METHOD_ADD:
		break;
	case I_CAL_METHOD_REFRESH:
		/* Need to remove almost everything */
		temp_clone = comp_minimal (registry, clone, TRUE);
		g_object_unref (clone);
		clone = temp_clone;
		break;
	case I_CAL_METHOD_COUNTER:
		break;
	case I_CAL_METHOD_DECLINECOUNTER:
		/* Need to remove almost everything */
		temp_clone = comp_minimal (registry, clone, FALSE);
		g_object_unref (clone);
		clone = temp_clone;
		break;
	default:
		break;
	}

	remove_schedule_status_parameters (clone);

	return clone;
}

static gboolean
comp_compliant (ESourceRegistry *registry,
		ICalPropertyMethod method,
		GSList *ecomps,
		gboolean unref_orig_ecomp,
		ECalClient *client,
		ICalComponent *zones,
		ICalTimezone *default_zone,
		gboolean strip_alarms)
{
	GSList *link;

	if (!ecomps)
		return FALSE;

	for (link = ecomps; link; link = g_slist_next (link)) {
		ECalComponent *original_comp = link->data, *new_comp;

		new_comp = comp_compliant_one (registry, method, original_comp, client, zones, default_zone, strip_alarms);
		if (new_comp) {
			cal_comp_util_copy_new_attendees (new_comp, original_comp);

			if (unref_orig_ecomp)
				g_object_unref (original_comp);

			link->data = new_comp;
		} else {
			return FALSE;
		}
	}

	return TRUE;
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

		if (address && e_cal_util_email_addresses_equal (address, id_address)) {
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
			   ICalPropertyMethod method,
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
		ECalComponentOrganizer *organizer;
		const gchar *organizer_email;

		organizer = e_cal_component_get_organizer (comp);
		organizer_email = e_cal_util_get_organizer_email (organizer);
		if (organizer_email) {
			source = find_enabled_identity (registry, organizer_email);

			if (source) {
				if (identity_name)
					*identity_name = g_strdup (e_cal_component_organizer_get_cn (organizer));
				if (identity_address)
					*identity_address = g_strdup (organizer_email);
			}
		}

		e_cal_component_organizer_free (organizer);
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

static gint
master_first_cmp (gconstpointer ptr1,
		  gconstpointer ptr2)
{
	ECalComponent *comp1 = (ECalComponent *) ptr1;
	ECalComponent *comp2 = (ECalComponent *) ptr2;
	ICalComponent *icomp1 = comp1 ? e_cal_component_get_icalcomponent (comp1) : NULL;
	ICalComponent *icomp2 = comp2 ? e_cal_component_get_icalcomponent (comp2) : NULL;
	gboolean has_rid1, has_rid2;

	has_rid1 = (icomp1 && e_cal_util_component_has_property (icomp1, I_CAL_RECURRENCEID_PROPERTY)) ? 1 : 0;
	has_rid2 = (icomp2 && e_cal_util_component_has_property (icomp2, I_CAL_RECURRENCEID_PROPERTY)) ? 1 : 0;

	if (has_rid1 == has_rid2)
		return g_strcmp0 (icomp1 ? i_cal_component_get_uid (icomp1) : NULL,
				  icomp2 ? i_cal_component_get_uid (icomp2) : NULL);

	if (has_rid1)
		return 1;

	return -1;
}

typedef struct {
	ESourceRegistry *registry;
	ICalPropertyMethod method;
	GSList *send_comps; /* ECalComponent * */
	ECalClient *cal_client;
	ICalComponent *zones;
	GSList *attachments_list;
	GSList *users;
	EItipSendComponentFlags flags;

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
		g_slist_free_full (isc->send_comps, g_object_unref);
		g_clear_object (&isc->cal_client);
		g_clear_object (&isc->zones);
		g_clear_error (&isc->async_error);
		g_slist_free_full (isc->attachments_list, itip_cal_mime_attach_free); /* CamelMimePart */
		g_slist_free_full (isc->users, g_free);
		g_slice_free (ItipSendComponentData, isc);
	}
}

static void
itip_send_component_begin (ItipSendComponentData *isc,
			   GCancellable *cancellable,
			   GError **error)
{
	GSList *link;

	g_return_if_fail (isc != NULL);

	isc->completed = FALSE;

	if ((isc->flags & (E_ITIP_SEND_COMPONENT_FLAG_SAVE_RESPONSE_ACCEPTED |
			   E_ITIP_SEND_COMPONENT_FLAG_SAVE_RESPONSE_DECLINED |
			   E_ITIP_SEND_COMPONENT_FLAG_SAVE_RESPONSE_TENTATIVE)) != 0 &&
	    isc->send_comps && !isc->send_comps->next) {
		ICalComponent *toplevel, *icomp;
		ICalParameterPartstat partstat;
		gchar *attendee;

		if ((isc->flags & E_ITIP_SEND_COMPONENT_FLAG_SAVE_RESPONSE_ACCEPTED) != 0)
			partstat = I_CAL_PARTSTAT_ACCEPTED;
		else if ((isc->flags & E_ITIP_SEND_COMPONENT_FLAG_SAVE_RESPONSE_DECLINED) != 0)
			partstat = I_CAL_PARTSTAT_DECLINED;
		else
			partstat = I_CAL_PARTSTAT_TENTATIVE;

		if ((isc->flags & E_ITIP_SEND_COMPONENT_FLAG_ENSURE_MASTER_OBJECT) != 0 && isc->send_comps) {
			/* Ensure we send the master object with its detached instances, not the instance only */
			GSList *ecalcomps = NULL;
			const gchar *uid;

			uid = e_cal_component_get_uid (isc->send_comps->data);

			if (e_cal_client_get_objects_for_uid_sync (isc->cal_client, uid, &ecalcomps, cancellable, NULL) && ecalcomps) {
				GSList *old_send_comps = isc->send_comps;

				isc->send_comps = g_slist_sort (ecalcomps, master_first_cmp);

				cal_comp_util_copy_new_attendees (isc->send_comps->data, old_send_comps->data);

				g_slist_free_full (old_send_comps, g_object_unref);
			}

			isc->flags &= ~E_ITIP_SEND_COMPONENT_FLAG_ENSURE_MASTER_OBJECT;
		}

		attendee = itip_get_comp_attendee (isc->registry, isc->send_comps->data, isc->cal_client);

		icomp = e_cal_component_get_icalcomponent (isc->send_comps->data);
		itip_utils_prepare_attendee_response (isc->registry, icomp, attendee, partstat);
		itip_utils_update_cdo_replytime (icomp);

		toplevel = i_cal_component_new_vcalendar ();
		i_cal_component_set_method (toplevel, isc->method);

		for (link = isc->send_comps; link; link = g_slist_next (link)) {
			ECalComponent *comp = link->data;

			icomp = e_cal_component_get_icalcomponent (comp);

			if (icomp)
				i_cal_component_take_component (toplevel, i_cal_component_clone (icomp));
		}

		isc->success = e_cal_client_receive_objects_sync (isc->cal_client, toplevel, E_CAL_OPERATION_FLAG_NONE, cancellable, error);

		g_object_unref (toplevel);

		if (!isc->success) {
			isc->completed = TRUE;
			g_free (attendee);
			return;
		}

		/* Include only the 'attendee' and use the first component as well in the reply */
		if (isc->method == I_CAL_METHOD_REPLY) {
			while (g_slist_next (isc->send_comps)) {
				ECalComponent *comp = isc->send_comps->next->data;

				isc->send_comps = g_slist_remove (isc->send_comps, comp);

				g_clear_object (&comp);
			}

			itip_utils_remove_all_but_attendee (e_cal_component_get_icalcomponent (isc->send_comps->data), attendee);
		}

		g_free (attendee);
	}

	if (isc->method != I_CAL_METHOD_PUBLISH && e_cal_client_check_save_schedules (isc->cal_client) &&
	    (isc->method != I_CAL_METHOD_CANCEL || !e_client_check_capability (E_CLIENT (isc->cal_client), E_CAL_STATIC_CAPABILITY_RETRACT_SUPPORTED))) {
		isc->success = TRUE;
		isc->completed = TRUE;
		return;
	}

	if ((isc->flags & E_ITIP_SEND_COMPONENT_FLAG_ENSURE_MASTER_OBJECT) != 0 && isc->send_comps) {
		/* Ensure we send the master object with its detached instances, not the instance only */
		GSList *ecalcomps = NULL;
		const gchar *uid;

		uid = e_cal_component_get_uid (isc->send_comps->data);

		if (e_cal_client_get_objects_for_uid_sync (isc->cal_client, uid, &ecalcomps, cancellable, NULL) && ecalcomps) {
			GSList *old_send_comps = isc->send_comps;

			isc->send_comps = g_slist_sort (ecalcomps, master_first_cmp);

			cal_comp_util_copy_new_attendees (isc->send_comps->data, old_send_comps->data);

			g_slist_free_full (old_send_comps, g_object_unref);
		}
	}

	for (link = isc->send_comps; link; link = g_slist_next (link)) {
		ECalComponent *comp = link->data;

		if (comp && e_cal_component_has_attachments (comp)) {
			ECalComponent *clone;

			clone = e_cal_component_clone (comp);
			g_object_unref (comp);

			link->data = clone;

			if (!e_cal_util_inline_local_attachments_sync (e_cal_component_get_icalcomponent (clone), cancellable, error)) {
				isc->success = FALSE;
				return;
			}
		}
	}

	/* Give the server a chance to manipulate the comp */
	if (isc->method != I_CAL_METHOD_PUBLISH) {
		d (printf ("itip-utils.c: itip_send_component_begin: calling comp_server_send_sync... \n"));
		if (!comp_server_send_sync (isc->method, isc->send_comps, isc->cal_client, isc->zones, &isc->users, cancellable, error)) {
			isc->success = FALSE;
			isc->completed = TRUE;
			return;
		}
	}

	/* check whether backend could handle sending requests/updates */
	if (isc->method != I_CAL_METHOD_PUBLISH &&
	    e_client_check_capability (E_CLIENT (isc->cal_client), E_CAL_STATIC_CAPABILITY_CREATE_MESSAGES)) {
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
	gchar *html_body;
	GSList *attachments_list;
	GSList *send_comps; /* ECalComponent * */
	gboolean show_only;
	gboolean without_ical_data;
	EItipSendComponentFlags flags;
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
		goto free_ccd;
	}

	e_msg_composer_set_is_imip (composer, TRUE);

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");
	use_24hour_format = g_settings_get_boolean (settings, "use-24hour-format");
	g_object_unref (settings);

	table = e_msg_composer_get_header_table (composer);

	if (ccd->identity_uid)
		e_composer_header_table_set_identity_uid (table, ccd->identity_uid, ccd->identity_name, ccd->identity_address);

	e_composer_header_table_set_subject (table, ccd->subject);
	e_composer_header_table_set_destinations_to (table, ccd->destinations);
	e_msg_composer_set_body_text (composer, ccd->html_body, TRUE);

	if (!ccd->without_ical_data) {
		CamelMimePart *attachment;

		attachment = camel_mime_part_new ();
		camel_mime_part_set_content (
			attachment, ccd->ical_string,
			strlen (ccd->ical_string), ccd->content_type);
		if ((ccd->flags & E_ITIP_SEND_COMPONENT_FLAG_AS_ATTACHMENT) != 0) {
			const gchar *filename;
			gchar *description;

			filename = comp_filename (ccd->send_comps->data);
			description = comp_description (ccd->send_comps->data, use_24hour_format);

			if (filename != NULL && *filename != '\0')
				camel_mime_part_set_filename (attachment, filename);
			if (description != NULL && *description != '\0')
				camel_mime_part_set_description (attachment, description);

			e_msg_composer_attach (composer, attachment);

			g_free (description);
		} else {
			e_msg_composer_set_alternative_body (composer, attachment);
		}

		g_object_unref (attachment);
	}

	append_cal_attachments (composer, ccd->attachments_list);
	ccd->attachments_list = NULL;

	if (ccd->show_only)
		gtk_widget_show (GTK_WIDGET (composer));
	else
		e_msg_composer_send (composer);

 free_ccd:
	e_destination_freev (ccd->destinations);
	g_slist_free_full (ccd->send_comps, g_object_unref);
	g_free (ccd->identity_uid);
	g_free (ccd->identity_name);
	g_free (ccd->identity_address);
	g_free (ccd->subject);
	g_free (ccd->ical_string);
	g_free (ccd->content_type);
	g_free (ccd->html_body);
	g_slice_free (CreateComposerData, ccd);
}

static void
itip_send_component_complete (ItipSendComponentData *isc)
{
	CreateComposerData *ccd;
	EDestination **destinations;
	EShell *shell;
	ICalComponent *top_level = NULL;
	ICalTimezone *default_zone;
	GString *html;
	gchar *identity_uid, *identity_name = NULL, *identity_address = NULL;
	gboolean as_attachment;

	g_return_if_fail (isc != NULL);

	if (isc->completed)
		return;

	isc->success = FALSE;

	default_zone = calendar_config_get_icaltimezone ();

	shell = e_shell_get_default ();

	identity_uid = get_identity_uid_for_from (shell, isc->method, isc->send_comps->data, isc->cal_client, &identity_name, &identity_address);

	/* Tidy up the comp */
	if (!comp_compliant (isc->registry, isc->method, isc->send_comps, TRUE, isc->cal_client, isc->zones, default_zone,
			     (isc->flags & E_ITIP_SEND_COMPONENT_FLAG_STRIP_ALARMS) != 0)) {
		g_free (identity_uid);
		g_free (identity_name);
		g_free (identity_address);
		goto cleanup;
	}

	/* Recipients */
	destinations = comp_to_list (
		isc->registry, isc->method, isc->send_comps->data, isc->users, FALSE,
		(isc->flags & E_ITIP_SEND_COMPONENT_FLAG_ONLY_NEW_ATTENDEES) != 0 ? g_object_get_data (
		G_OBJECT (isc->send_comps->data), "new-attendees") : NULL);
	if (isc->method != I_CAL_METHOD_PUBLISH) {
		if (destinations == NULL) {
			/* We sent them all via the server */
			isc->success = TRUE;
			g_free (identity_uid);
			g_free (identity_name);
			g_free (identity_address);
			goto cleanup;
		}
	}

	html = g_string_new ("");

	cal_comp_util_write_to_html (html, isc->cal_client, isc->send_comps->data, default_zone, E_COMP_TO_HTML_FLAG_NONE);

	as_attachment = calendar_config_get_itip_attach_components ();
	top_level = comp_toplevel_with_zones (isc->method, isc->send_comps, isc->cal_client, isc->zones);

	ccd = g_slice_new0 (CreateComposerData);
	ccd->identity_uid = identity_uid;
	ccd->identity_name = identity_name;
	ccd->identity_address = identity_address;
	ccd->destinations = destinations;
	ccd->subject = comp_subject (isc->registry, isc->method, isc->send_comps->data, FALSE);
	ccd->html_body = g_string_free (html, FALSE);
	ccd->ical_string = i_cal_component_as_ical_string (top_level);
	ccd->content_type = comp_content_type (isc->send_comps->data, isc->method, as_attachment);
	ccd->attachments_list = isc->attachments_list;
	ccd->send_comps = isc->send_comps;
	ccd->show_only = isc->method == I_CAL_METHOD_PUBLISH && !isc->users;
	ccd->flags = isc->flags | (as_attachment ? E_ITIP_SEND_COMPONENT_FLAG_AS_ATTACHMENT : 0);

	isc->attachments_list = NULL;
	isc->send_comps = NULL;

	e_msg_composer_new (shell, itip_send_component_composer_created_cb, ccd);

	isc->success = TRUE;

 cleanup:
	g_clear_object (&top_level);
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
itip_send_component_with_model (ECalDataModel *data_model,
				ICalPropertyMethod method,
				ECalComponent *send_comp,
				ECalClient *cal_client,
				ICalComponent *zones,
				GSList *attachments_list,
				GSList *users,
				EItipSendComponentFlags flags)
{
	ESourceRegistry *registry;
	ESource *source;
	const gchar *alert_ident = NULL;
	const gchar *description = NULL;
	gchar *display_name;
	GCancellable *cancellable;
	ItipSendComponentData *isc;

	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));
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

	registry = e_cal_data_model_get_registry (data_model);
	source = e_client_get_source (E_CLIENT (cal_client));

	isc = g_slice_new0 (ItipSendComponentData);
	isc->registry = g_object_ref (registry);
	isc->method = method;
	isc->send_comps = g_slist_prepend (NULL, g_object_ref (send_comp));
	isc->cal_client = g_object_ref (cal_client);
	if (zones) {
		isc->zones = i_cal_component_clone (zones);
	}
	isc->attachments_list = attachments_list;
	if (users) {
		GSList *link;

		isc->users = g_slist_copy (users);
		for (link = isc->users; link; link = g_slist_next (link)) {
			link->data = g_strdup (link->data);
		}
	}
	isc->flags = flags;
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
		     ICalPropertyMethod method,
		     ECalComponent *send_comp,
		     ECalClient *cal_client,
		     ICalComponent *zones,
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
	isc.send_comps = g_slist_prepend (NULL, g_object_ref (send_comp));
	isc.cal_client = cal_client;
	isc.zones = zones;
	isc.attachments_list = attachments_list;
	isc.users = users;
	isc.flags = (strip_alarms ? E_ITIP_SEND_COMPONENT_FLAG_STRIP_ALARMS : 0) |
		    (only_new_attendees ? E_ITIP_SEND_COMPONENT_FLAG_ONLY_NEW_ATTENDEES : 0);

	isc.completed = FALSE;
	isc.success = FALSE;

	itip_send_component_begin (&isc, cancellable, error);
	itip_send_component_complete (&isc);

	g_slist_free_full (isc.send_comps, g_object_unref);
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
		     ICalPropertyMethod method,
		     ECalComponent *send_comp,
		     ECalClient *cal_client,
		     ICalComponent *zones,
		     GSList *attachments_list,
		     GSList *users,
		     EItipSendComponentFlags flags,
		     GCancellable *cancellable,
		     GAsyncReadyCallback callback,
		     gpointer user_data)
{
	GTask *task;
	ItipSendComponentData *isc;

	isc = g_slice_new0 (ItipSendComponentData);
	isc->registry = g_object_ref (registry);
	isc->method = method;
	isc->send_comps = g_slist_prepend (NULL, g_object_ref (send_comp));
	isc->cal_client = g_object_ref (cal_client);
	if (zones)
		isc->zones = i_cal_component_clone (zones);
	isc->attachments_list = attachments_list;
	if (users) {
		GSList *link;

		isc->users = g_slist_copy (users);
		for (link = isc->users; link; link = g_slist_next (link)) {
			link->data = g_strdup (link->data);
		}
	}
	isc->flags = flags;
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
                        ICalPropertyMethod method,
                        ECalComponent *send_comp,
                        ECalClient *cal_client,
                        gboolean reply_all,
                        ICalComponent *zones,
                        GSList *attachments_list)
{
	EShell *shell;
	ICalComponent *top_level = NULL;
	ICalTimezone *default_zone;
	gboolean retval = FALSE;
	gchar *identity_uid, *identity_name = NULL, *identity_address = NULL;
	GSList *ecomps;
	GString *html;
	CreateComposerData *ccd;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), FALSE);

	/* FIXME Pass this in. */
	shell = e_shell_get_default ();

	default_zone = calendar_config_get_icaltimezone ();

	ecomps = g_slist_prepend (NULL, send_comp);

	identity_uid = get_identity_uid_for_from (shell, method, send_comp, cal_client, &identity_name, &identity_address);

	/* Tidy up the comp */
	if (!comp_compliant (registry, method, ecomps, FALSE, cal_client, zones, default_zone, TRUE)) {
		g_free (identity_uid);
		g_free (identity_name);
		g_free (identity_address);
		goto cleanup;
	}

	top_level = comp_toplevel_with_zones (method, ecomps, cal_client, zones);

	html = g_string_new ("");

	if (e_cal_component_get_vtype (ecomps->data) == E_CAL_COMPONENT_EVENT) {
		gchar *str;

		str = g_markup_escape_text (_("Original Appointment"), -1);

		g_string_append_printf (html,
			"<div><br></div>"
			"<div><br></div>"
			"<hr>"
			"<div><br></div>"
			"<div><b>______ %s ______ </b><br></div>"
			"<div><br></div>", str);

		g_free (str);
	}

	cal_comp_util_write_to_html (html, cal_client, send_comp, default_zone, E_COMP_TO_HTML_FLAG_NONE);

	ccd = g_slice_new0 (CreateComposerData);
	ccd->identity_uid = identity_uid;
	ccd->identity_name = identity_name;
	ccd->identity_address = identity_address;
	ccd->destinations = comp_to_list (registry, method, ecomps->data, NULL, reply_all, NULL);
	/* Using I_CAL_METHOD_NONE to not get attendee's response as a prefix */
	ccd->subject = comp_subject (registry, I_CAL_METHOD_NONE, ecomps->data, TRUE);
	ccd->html_body = g_string_free (html, FALSE);
	ccd->ical_string = i_cal_component_as_ical_string (top_level);
	ccd->send_comps = ecomps;
	ccd->show_only = TRUE;
	ccd->without_ical_data = e_cal_component_get_vtype (ecomps->data) == E_CAL_COMPONENT_EVENT;
	ccd->flags = 0;

	if (calendar_config_get_itip_attach_components ())
		ccd->flags |= E_ITIP_SEND_COMPONENT_FLAG_AS_ATTACHMENT;

	e_msg_composer_new (shell, itip_send_component_composer_created_cb, ccd);

	retval = TRUE;

 cleanup:

	g_clear_object (&top_level);

	return retval;
}

static gboolean
check_time (ICalTime *tmval,
            gboolean can_null_time)
{
	gboolean valid;

	if (!tmval || i_cal_time_is_null_time (tmval)) {
		g_clear_object (&tmval);
		return can_null_time;
	}

	valid = i_cal_time_is_valid_time (tmval) &&
		i_cal_time_get_month (tmval) >= 1 && i_cal_time_get_month (tmval) <= 12 &&
		i_cal_time_get_day (tmval) >= 1 && i_cal_time_get_day (tmval) <= 31 &&
		i_cal_time_get_hour (tmval) >= 0 && i_cal_time_get_hour (tmval) < 24 &&
		i_cal_time_get_minute (tmval) >= 0 && i_cal_time_get_minute (tmval) < 60 &&
		i_cal_time_get_second (tmval) >= 0 && i_cal_time_get_second (tmval) < 60;

	g_clear_object (&tmval);

	return valid;
}

/* Returns whether the passed-in ICalComponent is valid or not.
 * It does some sanity checks on values too. */
gboolean
itip_is_component_valid (ICalComponent *icomp)
{
	if (!icomp || !i_cal_component_is_valid (icomp))
		return FALSE;

	switch (i_cal_component_isa (icomp)) {
	case I_CAL_VEVENT_COMPONENT:
		return	check_time (i_cal_component_get_dtstart (icomp), FALSE) &&
			check_time (i_cal_component_get_dtend (icomp), TRUE);
	case I_CAL_VTODO_COMPONENT:
		return	check_time (i_cal_component_get_dtstart (icomp), TRUE) &&
			check_time (i_cal_component_get_due (icomp), TRUE);
	case I_CAL_VJOURNAL_COMPONENT:
		return	check_time (i_cal_component_get_dtstart (icomp), TRUE) &&
			check_time (i_cal_component_get_dtend (icomp), TRUE);
	default:
		break;
	}

	return TRUE;
}

gboolean
itip_component_has_recipients (ECalComponent *comp)
{
	GSList *attendees, *link;
	ECalComponentAttendee *attendee;
	ECalComponentOrganizer *organizer;
	const gchar *organizer_email;
	gboolean res = FALSE;

	g_return_val_if_fail (comp != NULL, FALSE);

	organizer = e_cal_component_get_organizer (comp);
	organizer_email = e_cal_util_get_organizer_email (organizer);

	attendees = e_cal_component_get_attendees (comp);

	if (!attendees) {
		if (organizer_email &&
		    e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_JOURNAL) {
			/* memos store recipients in an extra property */
			ICalComponent *icomp;
			ICalProperty *prop;

			icomp = e_cal_component_get_icalcomponent (comp);

			for (prop = i_cal_component_get_first_property (icomp, I_CAL_X_PROPERTY);
			     prop;
			     g_object_unref (prop), prop = i_cal_component_get_next_property (icomp, I_CAL_X_PROPERTY)) {
				const gchar *x_name;

				x_name = i_cal_property_get_x_name (prop);

				if (g_str_equal (x_name, "X-EVOLUTION-RECIPIENTS")) {
					const gchar *str_recipients = i_cal_property_get_x (prop);

					res = str_recipients && !e_cal_util_email_addresses_equal (organizer_email, str_recipients);
					g_object_unref (prop);
					break;
				}
			}
		}

		e_cal_component_organizer_free (organizer);

		return res;
	}

	if (g_slist_length (attendees) > 1 || !e_cal_component_has_organizer (comp)) {
		g_slist_free_full (attendees, e_cal_component_attendee_free);
		e_cal_component_organizer_free (organizer);
		return TRUE;
	}

	for (link = attendees; link && !res; link = g_slist_next (link)) {
		const gchar *attendee_email;

		attendee = link->data;
		attendee_email = e_cal_util_get_attendee_email (attendee);

		res = !e_cal_util_email_addresses_equal (organizer_email, attendee_email);
	}

	g_slist_free_full (attendees, e_cal_component_attendee_free);
	e_cal_component_organizer_free (organizer);

	return res;
}

void
itip_utils_update_cdo_replytime (ICalComponent *icomp)
{
	ICalTime *stamp;
	gchar *str;

	g_return_if_fail (I_CAL_IS_COMPONENT (icomp));

	while (e_cal_util_component_remove_x_property (icomp, "X-MICROSOFT-CDO-REPLYTIME")) {
		/* Delete all existing X-MICROSOFT-CDO-REPLYTIME properties first */
	}

	/* Set X-MICROSOFT-CDO-REPLYTIME to record the time at which
	 * the user accepted/declined the request. (Outlook ignores
	 * SEQUENCE in REPLY reponses and instead requires that each
	 * updated response have a later REPLYTIME than the previous
	 * one.) This also ends up getting saved in our own copy of
	 * the meeting, though there's currently no way to see that
	 * information (unless it's being saved to an Exchange folder
	 * and you then look at it in Outlook).
	 */
	stamp = i_cal_time_new_current_with_zone (i_cal_timezone_get_utc_timezone ());
	str = i_cal_time_as_ical_string (stamp);
	e_cal_util_component_set_x_property (icomp, "X-MICROSOFT-CDO-REPLYTIME", str);
	g_clear_object (&stamp);
	g_free (str);
}

ICalProperty *
itip_utils_find_attendee_property (ICalComponent *icomp,
				   const gchar *address)
{
	ICalProperty *prop;

	if (!address || !*address)
		return NULL;

	for (prop = i_cal_component_get_first_property (icomp, I_CAL_ATTENDEE_PROPERTY);
	     prop;
	     g_object_unref (prop), prop = i_cal_component_get_next_property (icomp, I_CAL_ATTENDEE_PROPERTY)) {
		gchar *attendee;
		gchar *text;

		attendee = i_cal_property_get_value_as_string (prop);

		 if (!attendee)
			continue;

		text = g_strdup (e_cal_util_strip_mailto (attendee));
		text = g_strstrip (text);
		if (text && e_cal_util_email_addresses_equal (address, text)) {
			g_free (text);
			g_free (attendee);
			break;
		}
		g_free (text);
		g_free (attendee);
	}

	return prop;
}

void
itip_utils_prepare_attendee_response (ESourceRegistry *registry,
				      ICalComponent *icomp,
				      const gchar *address,
				      ICalParameterPartstat partstat)
{
	ICalProperty *prop;

	prop = itip_utils_find_attendee_property (icomp, address);
	if (prop) {
		ICalParameter *param;

		i_cal_property_remove_parameter_by_kind (prop, I_CAL_PARTSTAT_PARAMETER);
		param = i_cal_parameter_new_partstat (partstat);
		if (param)
			i_cal_property_take_parameter (prop, param);

		g_object_unref (prop);
	} else {
		ICalParameter *param;

		if (address && *address) {
			gchar *mailto_uri;

			mailto_uri = g_strconcat ("mailto:", e_cal_util_strip_mailto (address), NULL);
			prop = i_cal_property_new_attendee (mailto_uri);
			g_free (mailto_uri);

			param = i_cal_parameter_new_role (I_CAL_ROLE_OPTPARTICIPANT);
			i_cal_property_take_parameter (prop, param);

			param = i_cal_parameter_new_partstat (partstat);
			if (param)
				i_cal_property_take_parameter (prop, param);

			i_cal_component_take_property (icomp, prop);
		} else {
			gchar *default_name = NULL;
			gchar *default_address = NULL;
			gchar *mailto_uri;

			e_cal_util_get_default_name_and_address (registry, &default_name, &default_address);

			mailto_uri = g_strconcat ("mailto:", e_cal_util_strip_mailto (default_address), NULL);
			prop = i_cal_property_new_attendee (mailto_uri);
			g_free (mailto_uri);

			if (default_name && *default_name && g_strcmp0 (default_name, default_address) != 0) {
				param = i_cal_parameter_new_cn (default_name);
				i_cal_property_take_parameter (prop, param);
			}

			param = i_cal_parameter_new_role (I_CAL_ROLE_REQPARTICIPANT);
			i_cal_property_take_parameter (prop, param);

			param = i_cal_parameter_new_partstat (partstat);
			if (param)
				i_cal_property_take_parameter (prop, param);

			i_cal_component_take_property (icomp, prop);

			g_free (default_name);
			g_free (default_address);
		}
	}
}

gboolean
itip_utils_remove_all_but_attendee (ICalComponent *icomp,
				    const gchar *attendee)
{
	ICalProperty *prop;
	GSList *remove = NULL, *link;
	gboolean found = FALSE;

	g_return_val_if_fail (I_CAL_IS_COMPONENT (icomp), FALSE);
	g_return_val_if_fail (attendee != NULL, FALSE);

	for (prop = i_cal_component_get_first_property (icomp, I_CAL_ATTENDEE_PROPERTY);
	     prop;
	     prop = i_cal_component_get_next_property (icomp, I_CAL_ATTENDEE_PROPERTY)) {
		const gchar *address = e_cal_util_get_property_email (prop);

		if (found || !e_cal_util_email_addresses_equal (address, attendee)) {
			remove = g_slist_prepend (remove, prop);
		} else {
			found = TRUE;
			g_object_unref (prop);
		}
	}

	for (link = remove; link; link = g_slist_next (link)) {
		prop = link->data;

		i_cal_component_remove_property (icomp, prop);
	}

	g_slist_free_full (remove, g_object_unref);

	return found;
}
