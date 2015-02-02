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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <time.h>
#include <glib/gi18n.h>

#include <shell/e-shell.h>

#include "publish-format-fb.h"

static void
free_busy_data_cb (ECalClient *client,
                   const GSList *free_busy_ecalcomps,
                   GSList **pobjects)
{
	const GSList *iter;

	g_return_if_fail (pobjects != NULL);

	for (iter = free_busy_ecalcomps; iter != NULL; iter = iter->next) {
		ECalComponent *comp = iter->data;

		if (comp)
			*pobjects = g_slist_prepend (*pobjects, g_object_ref (comp));
	}
}

static gboolean
write_calendar (const gchar *uid,
                GOutputStream *stream,
                gint dur_type,
                gint dur_value,
                GError **error)
{
	EShell *shell;
	ESource *source;
	ESourceRegistry *registry;
	EClient *client = NULL;
	GSList *objects = NULL;
	icaltimezone *utc;
	time_t start = time (NULL), end;
	icalcomponent *top_level;
	gchar *email = NULL;
	GSList *users = NULL;
	gboolean success = FALSE;

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

	shell = e_shell_get_default ();
	registry = e_shell_get_registry (shell);
	source = e_source_registry_ref_source (registry, uid);

	if (source != NULL) {
		EClientCache *client_cache;

		client_cache = e_shell_get_client_cache (shell);
		client = e_client_cache_get_client_sync (client_cache, source, E_SOURCE_EXTENSION_CALENDAR, 30, NULL, error);

		g_object_unref (source);
	} else {
		g_set_error (
			error, E_CAL_CLIENT_ERROR,
			E_CAL_CLIENT_ERROR_NO_SUCH_CALENDAR,
			_("Invalid source UID '%s'"), uid);
	}

	if (client == NULL)
		return FALSE;

	if (e_client_get_backend_property_sync (client, CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS, &email, NULL, NULL)) {
		if (email && *email)
			users = g_slist_append (users, email);
	}

	top_level = e_cal_util_new_top_level ();

	g_signal_connect (
		client, "free-busy-data",
		G_CALLBACK (free_busy_data_cb), &objects);

	success = e_cal_client_get_free_busy_sync (
		E_CAL_CLIENT (client), start, end, users, NULL, error);
	if (success) {
		gchar *ical_string;
		GSList *iter;
		gboolean done = FALSE;

		/* This is to workaround broken dispatch of "free-busy-data" signal,
		 * introduced in 3.8.0. This code can be removed once the below bug is
		 * properly fixed: https://bugzilla.gnome.org/show_bug.cgi?id=692361
		*/
		while (!done) {
			g_usleep (G_USEC_PER_SEC / 10);
			done = !g_main_context_iteration (NULL, FALSE);
		}

		for (iter = objects; iter; iter = iter->next) {
			ECalComponent *comp = iter->data;
			icalcomponent *icalcomp = icalcomponent_new_clone (e_cal_component_get_icalcomponent (comp));
			icalcomponent_add_component (top_level, icalcomp);
		}

		ical_string = icalcomponent_as_ical_string_r (top_level);

		success = g_output_stream_write_all (
			stream, ical_string,
			strlen (ical_string),
			NULL, NULL, error);

		e_cal_client_free_ecalcomp_slist (objects);
		g_free (ical_string);
	}

	if (users)
		g_slist_free (users);

	g_free (email);
	g_object_unref (client);
	icalcomponent_free (top_level);

	return success;
}

void
publish_calendar_as_fb (GOutputStream *stream,
                        EPublishUri *uri,
                        GError **error)
{
	GSList *l;

	/* events */
	l = uri->events;
	while (l) {
		gchar *uid = l->data;
		if (!write_calendar (uid, stream, uri->fb_duration_type, uri->fb_duration_value, error))
			break;
		l = g_slist_next (l);
	}
}
