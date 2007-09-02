/*
 * Authors: David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 2005 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <string.h>
#include <gconf/gconf-client.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libedataserver/e-source.h>
#include <libedataserver/e-source-list.h>
#include <libecal/e-cal.h>
#include <libecal/e-cal-util.h>
#include <calendar/common/authentication.h>
#include "publish-format-ical.h"

typedef struct {
	GHashTable *zones;
	ECal *ecal;
} CompTzData;
 
 static void
insert_tz_comps (icalparameter *param, void *cb_data)
{
	const char *tzid;
	CompTzData *tdata = cb_data;
	icaltimezone *zone = NULL;
	icalcomponent *tzcomp;
	GError *error = NULL;

	tzid = icalparameter_get_tzid (param);
	
	if (g_hash_table_lookup (tdata->zones, tzid))
		return;
	
	if (!e_cal_get_timezone (tdata->ecal, tzid, &zone, &error)) {
		g_warning ("Could not get the timezone information for %s :  %s \n", tzid, error->message);
		g_error_free (error);
		return;
	}
	
	tzcomp = icalcomponent_new_clone (icaltimezone_get_component (zone));
	g_hash_table_insert (tdata->zones, (gpointer) tzid, (gpointer) tzcomp);
}

static void
append_tz_to_comp (gpointer key, gpointer value, icalcomponent *toplevel)
{
	icalcomponent_add_component (toplevel, (icalcomponent *) value);
}

static gboolean
write_calendar (gchar *uid, ESourceList *source_list, GnomeVFSHandle *handle)
{
	ESource *source;
	ECal *client = NULL;
	GError *error = NULL;
	GList *objects;
	icalcomponent *top_level;

	source = e_source_list_peek_source_by_uid (source_list, uid);
	if (source)
		client = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_EVENT);
	if (!client) {
		g_warning (G_STRLOC ": Could not publish calendar: Calendar backend no longer exists");
		return FALSE;
	}

	if (!e_cal_open (client, TRUE, &error)) {
		/* FIXME: EError */
		g_object_unref (client);
		g_error_free (error);
		return FALSE;
	}

	top_level = e_cal_util_new_top_level ();
	error = NULL;

	if (e_cal_get_object_list (client, "#t", &objects, &error)) {
		char *ical_string;
		GnomeVFSFileSize bytes_written = 0;
		GnomeVFSResult result;
		CompTzData tdata;
 
		tdata.zones = g_hash_table_new (g_str_hash, g_str_equal);
		tdata.ecal = client;

		while (objects) {
			icalcomponent *icalcomp = objects->data;
			icalcomponent_foreach_tzid (icalcomp, insert_tz_comps, &tdata);
			icalcomponent_add_component (top_level, icalcomp);
			objects = g_list_remove (objects, icalcomp);
		}

		g_hash_table_foreach (tdata.zones, (GHFunc) append_tz_to_comp, top_level);

		g_hash_table_destroy (tdata.zones);
		tdata.zones = NULL;

		ical_string = icalcomponent_as_ical_string (top_level);
		if ((result = gnome_vfs_write (handle, (gconstpointer) ical_string, strlen (ical_string), &bytes_written)) != GNOME_VFS_OK) {
			/* FIXME: EError */
			gnome_vfs_close (handle);
			return FALSE;
		}
	} else {
		/* FIXME: EError */
		g_object_unref (client);
		g_error_free (error);
		return FALSE;
	}

	g_object_unref (client);
	return TRUE;
}

void
publish_calendar_as_ical (GnomeVFSHandle *handle, EPublishUri *uri)
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
		write_calendar (uid, source_list, handle);
		l = g_slist_next (l);
	}
	g_object_unref (source_list);

	g_object_unref (gconf_client);
}
