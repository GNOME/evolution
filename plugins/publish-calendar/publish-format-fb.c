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
#include <gconf/gconf-client.h>
#include <libedataserver/e-source.h>
#include <libedataserver/e-source-list.h>
#include <libecal/e-cal.h>
#include <libecal/e-cal-util.h>
#include <libecal/e-cal-time-util.h>
#include <calendar/common/authentication.h>
#include "publish-format-fb.h"

static gboolean
write_calendar (gchar *uid, ESourceList *source_list, GOutputStream *stream)
{
	ESource *source;
	ECal *client = NULL;
	GError *error = NULL;
	GList *objects;
	icaltimezone *utc;
	time_t start = time(NULL), end;
	icalcomponent *top_level;
	char *email = NULL;
	GList *users = NULL;
	gboolean res = FALSE;

	utc = icaltimezone_get_utc_timezone ();
	start = time_day_begin_with_zone (start, utc);
	end = time_add_week_with_zone (start, 6, utc);

	source = e_source_list_peek_source_by_uid (source_list, uid);
	if (source)
		client = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_EVENT);
	if (!client) {
		g_warning (G_STRLOC ": Could not publish calendar: Calendar backend no longer exists");
		return FALSE;
	}

	if (!e_cal_open (client, TRUE, &error)) {
		if (error) {
			g_warning ("%s", error->message);
			g_error_free (error);
		}
		g_object_unref (client);
		return FALSE;
	}

	if (e_cal_get_cal_address (client, &email, &error)) {
		if (email && *email)
			users = g_list_append (users, email);
	}

	top_level = e_cal_util_new_top_level ();
	error = NULL;

	if (e_cal_get_free_busy (client, users, start, end, &objects, &error)) {
		char *ical_string;

		while (objects) {
			ECalComponent *comp = objects->data;
			icalcomponent *icalcomp = e_cal_component_get_icalcomponent (comp);
			icalcomponent_add_component (top_level, icalcomp);
			objects = g_list_remove (objects, comp);
		}

		ical_string = icalcomponent_as_ical_string (top_level);
		res = g_output_stream_write_all (stream, ical_string, strlen (ical_string), NULL, NULL, &error);

		g_free (ical_string);
	}

	if (users)
		g_list_free (users);

	g_free (email);
	g_object_unref (client);

	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	return res;
}

void
publish_calendar_as_fb (GOutputStream *stream, EPublishUri *uri)
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
		if (!write_calendar (uid, source_list, stream))
			break;
		l = g_slist_next (l);
	}
	g_object_unref (source_list);

	g_object_unref (gconf_client);
}
