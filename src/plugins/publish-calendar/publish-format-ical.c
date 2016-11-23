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
#include <glib/gi18n.h>

#include <shell/e-shell.h>

#include "publish-format-ical.h"

typedef struct {
	GHashTable *zones;
	ECalClient *client;
} CompTzData;

 static void
insert_tz_comps (icalparameter *param,
                 gpointer cb_data)
{
	const gchar *tzid;
	CompTzData *tdata = cb_data;
	icaltimezone *zone = NULL;
	icalcomponent *tzcomp;
	GError *error = NULL;

	tzid = icalparameter_get_tzid (param);

	if (g_hash_table_lookup (tdata->zones, tzid))
		return;

	e_cal_client_get_timezone_sync (
		tdata->client, tzid, &zone, NULL, &error);

	if (error != NULL) {
		g_warning (
			"Could not get the timezone information for %s: %s",
			tzid, error->message);
		g_error_free (error);
		return;
	}

	tzcomp = icalcomponent_new_clone (icaltimezone_get_component (zone));
	g_hash_table_insert (tdata->zones, (gpointer) tzid, (gpointer) tzcomp);
}

static void
append_tz_to_comp (gpointer key,
                   gpointer value,
                   icalcomponent *toplevel)
{
	icalcomponent_add_component (toplevel, (icalcomponent *) value);
}

static gboolean
write_calendar (const gchar *uid,
                GOutputStream *stream,
                GError **error)
{
	EShell *shell;
	ESource *source;
	ESourceRegistry *registry;
	EClient *client = NULL;
	GSList *objects = NULL;
	icalcomponent *top_level;
	gboolean res = FALSE;

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
			_("Invalid source UID “%s”"), uid);
	}

	if (client == NULL)
		return FALSE;

	top_level = e_cal_util_new_top_level ();

	e_cal_client_get_object_list_sync (
		E_CAL_CLIENT (client), "#t", &objects, NULL, error);

	if (objects != NULL) {
		GSList *iter;
		gchar *ical_string;
		CompTzData tdata;

		tdata.zones = g_hash_table_new (g_str_hash, g_str_equal);
		tdata.client = E_CAL_CLIENT (client);

		for (iter = objects; iter; iter = iter->next) {
			icalcomponent *icalcomp = icalcomponent_new_clone (iter->data);
			icalcomponent_foreach_tzid (icalcomp, insert_tz_comps, &tdata);
			icalcomponent_add_component (top_level, icalcomp);
		}

		g_hash_table_foreach (tdata.zones, (GHFunc) append_tz_to_comp, top_level);

		g_hash_table_destroy (tdata.zones);
		tdata.zones = NULL;

		ical_string = icalcomponent_as_ical_string_r (top_level);
		res = g_output_stream_write_all (stream, ical_string, strlen (ical_string), NULL, NULL, error);
		g_free (ical_string);
		e_cal_client_free_icalcomp_slist (objects);
	}

	g_object_unref (client);
	icalcomponent_free (top_level);

	return res;
}

void
publish_calendar_as_ical (GOutputStream *stream,
                          EPublishUri *uri,
                          GError **error)
{
	GSList *l;

	/* events */
	l = uri->events;
	while (l) {
		gchar *uid = l->data;
		if (!write_calendar (uid, stream, error))
			break;
		l = g_slist_next (l);
	}
}
