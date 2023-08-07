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
 *		David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>
#include <time.h>
#include <glib/gi18n.h>

#include <shell/e-shell.h>

#include "publish-format-fb.h"

static gboolean
write_calendar (const gchar *uid,
                GOutputStream *stream,
		gboolean with_details,
                gint dur_type,
                gint dur_value,
                GError **error)
{
	EShell *shell;
	ESource *source;
	ESourceRegistry *registry;
	EClient *client = NULL;
	GSList *objects = NULL;
	ICalTimezone *utc;
	time_t start = time (NULL), end;
	ICalComponent *top_level;
	gchar *email = NULL;
	GSList *users = NULL;
	gboolean success = FALSE;

	utc = i_cal_timezone_get_utc_timezone ();
	start = time_day_begin_with_zone (start, utc);

	switch (dur_type) {
	case FB_DURATION_DAYS:
		end = time_add_day_with_zone (start, dur_value, utc);
		break;
	default:
	case FB_DURATION_WEEKS:
		end = time_add_week_with_zone (start, dur_value, utc);
		break;
	case FB_DURATION_MONTHS:
		end = time_add_month_with_zone (start, dur_value, utc);
		break;
	}

	shell = e_shell_get_default ();
	registry = e_shell_get_registry (shell);
	source = e_source_registry_ref_source (registry, uid);

	if (source != NULL) {
		EClientCache *client_cache;

		client_cache = e_shell_get_client_cache (shell);
		client = e_client_cache_get_client_sync (client_cache, source, E_SOURCE_EXTENSION_CALENDAR, E_DEFAULT_WAIT_FOR_CONNECTED_SECONDS, NULL, error);

		g_object_unref (source);
	} else {
		g_set_error (
			error, E_CAL_CLIENT_ERROR,
			E_CAL_CLIENT_ERROR_NO_SUCH_CALENDAR,
			_("Invalid source UID “%s”"), uid);
	}

	if (client == NULL)
		return FALSE;

	if (e_client_get_backend_property_sync (client, E_CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS, &email, NULL, NULL)) {
		if (email && *email)
			users = g_slist_append (users, email);
	}

	top_level = e_cal_util_new_top_level ();

	success = e_cal_client_get_free_busy_sync (
		E_CAL_CLIENT (client), start, end, users, &objects, NULL, error);

	if (success) {
		gchar *ical_string;
		GSList *iter;

		for (iter = objects; iter; iter = iter->next) {
			ECalComponent *comp = iter->data;
			ICalComponent *icomp = i_cal_component_clone (e_cal_component_get_icalcomponent (comp));

			if (!icomp)
				continue;

			if (!with_details) {
				ICalProperty *prop;

				for (prop = i_cal_component_get_first_property (icomp, I_CAL_FREEBUSY_PROPERTY);
				     prop;
				     g_object_unref (prop), prop = i_cal_component_get_next_property (icomp, I_CAL_FREEBUSY_PROPERTY)) {
					i_cal_property_remove_parameter_by_name (prop, "X-SUMMARY");
					i_cal_property_remove_parameter_by_name (prop, "X-LOCATION");
				}
			}

			i_cal_component_take_component (top_level, icomp);
		}

		ical_string = i_cal_component_as_ical_string (top_level);

		success = g_output_stream_write_all (
			stream, ical_string,
			strlen (ical_string),
			NULL, NULL, error);

		e_util_free_nullable_object_slist (objects);
		g_free (ical_string);
	}

	if (users)
		g_slist_free (users);

	g_free (email);
	g_object_unref (client);
	g_object_unref (top_level);

	return success;
}

void
publish_calendar_as_fb (GOutputStream *stream,
                        EPublishUri *uri,
                        GError **error)
{
	GSList *l;
	gboolean with_details = uri->publish_format == URI_PUBLISH_AS_FB_WITH_DETAILS;

	/* events */
	l = uri->events;
	while (l) {
		gchar *uid = l->data;
		if (!write_calendar (uid, stream, with_details, uri->fb_duration_type, uri->fb_duration_value, error))
			break;
		l = g_slist_next (l);
	}
}
