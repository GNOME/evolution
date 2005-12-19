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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

		while (objects) {
			icalcomponent *icalcomp = objects->data;
			icalcomponent_add_component (top_level, icalcomp);
			objects = g_list_remove (objects, icalcomp);
		}

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
