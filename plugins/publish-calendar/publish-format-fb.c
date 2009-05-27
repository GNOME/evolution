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
 *		David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <string.h>
#include <time.h>
#include <glib/gi18n.h>
#include <gconf/gconf-client.h>
#include <libedataserver/e-source.h>
#include <libedataserver/e-source-list.h>
#include <libecal/e-cal.h>
#include <libecal/e-cal-util.h>
#include <libecal/e-cal-time-util.h>
#include <calendar/common/authentication.h>
#include "publish-format-fb.h"

static gboolean
write_calendar (gchar *uid, ESourceList *source_list, GOutputStream *stream, gint dur_type, gint dur_value, GError **error)
{
	ESource *source;
	ECal *client = NULL;
	GList *objects;
	icaltimezone *utc;
	time_t start = time(NULL), end;
	icalcomponent *top_level;
	gchar *email = NULL;
	GList *users = NULL;
	gboolean res = FALSE;

	utc = icaltimezone_get_utc_timezone ();
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

	source = e_source_list_peek_source_by_uid (source_list, uid);
	if (source)
		client = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_EVENT);
	if (!client) {
		if (error)
			*error = g_error_new (e_calendar_error_quark (), E_CALENDAR_STATUS_NO_SUCH_CALENDAR, _("Could not publish calendar: Calendar backend no longer exists"));
		return FALSE;
	}

	if (!e_cal_open (client, TRUE, error)) {
		g_object_unref (client);
		return FALSE;
	}

	if (e_cal_get_cal_address (client, &email, NULL)) {
		if (email && *email)
			users = g_list_append (users, email);
	}

	top_level = e_cal_util_new_top_level ();

	if (e_cal_get_free_busy (client, users, start, end, &objects, error)) {
		gchar *ical_string;

		while (objects) {
			ECalComponent *comp = objects->data;
			icalcomponent *icalcomp = e_cal_component_get_icalcomponent (comp);
			icalcomponent_add_component (top_level, icalcomp);
			objects = g_list_remove (objects, comp);
		}

		ical_string = icalcomponent_as_ical_string_r (top_level);
		res = g_output_stream_write_all (stream, ical_string, strlen (ical_string), NULL, NULL, error);

		g_free (ical_string);
	}

	if (users)
		g_list_free (users);

	g_free (email);
	g_object_unref (client);

	return res;
}

void
publish_calendar_as_fb (GOutputStream *stream, EPublishUri *uri, GError **error)
{
	GSList *l;
	ESourceList *source_list;
	GConfClient *gconf_client;

	gconf_client = gconf_client_get_default ();

	/* events */
	source_list = e_source_list_new_for_gconf (gconf_client, "/apps/evolution/calendar/sources");
	l = uri->events;
	while (l) {
		gchar *uid = l->data;
		if (!write_calendar (uid, source_list, stream, uri->fb_duration_type, uri->fb_duration_value, error))
			break;
		l = g_slist_next (l);
	}
	g_object_unref (source_list);

	g_object_unref (gconf_client);
}
