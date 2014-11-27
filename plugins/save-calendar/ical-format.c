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
 *		Rodrigo Moya <rodrigo@novell.com>
 *      Philip Van Hoof <pvanhoof@gnome.org>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n.h>

#include "format-handler.h"

static void
display_error_message (GtkWidget *parent,
                       const gchar *message)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (
		GTK_WINDOW (parent), 0,
		GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
		"%s", message);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

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

static void
do_save_calendar_ical (FormatHandler *handler,
                       ESourceSelector *selector,
		       EClientCache *client_cache,
                       gchar *dest_uri)
{
	ESource *primary_source;
	EClient *source_client;
	GError *error = NULL;
	GSList *objects = NULL;
	icalcomponent *top_level = NULL;

	if (!dest_uri)
		return;

	/* open source client */
	primary_source = e_source_selector_ref_primary_selection (selector);
	source_client = e_client_cache_get_client_sync (client_cache,
		primary_source, e_source_selector_get_extension_name (selector), NULL, &error);
	g_object_unref (primary_source);

	/* Sanity check. */
	g_return_if_fail (
		((source_client != NULL) && (error == NULL)) ||
		((source_client == NULL) && (error != NULL)));

	if (error != NULL) {
		display_error_message (
			gtk_widget_get_toplevel (GTK_WIDGET (selector)),
			error->message);
		g_error_free (error);
		return;
	}

	/* create destination file */
	top_level = e_cal_util_new_top_level ();

	e_cal_client_get_object_list_sync (
		E_CAL_CLIENT (source_client), "#t", &objects, NULL, &error);

	if (objects != NULL) {
		CompTzData tdata;
		GOutputStream *stream;
		GSList *iter;

		tdata.zones = g_hash_table_new (g_str_hash, g_str_equal);
		tdata.client = E_CAL_CLIENT (source_client);

		for (iter = objects; iter; iter = iter->next) {
			icalcomponent *icalcomp = icalcomponent_new_clone (iter->data);

			icalcomponent_foreach_tzid (icalcomp, insert_tz_comps, &tdata);
			icalcomponent_add_component (top_level, icalcomp);
		}

		g_hash_table_foreach (tdata.zones, (GHFunc) append_tz_to_comp, top_level);

		g_hash_table_destroy (tdata.zones);
		tdata.zones = NULL;

		/* save the file */
		stream = open_for_writing (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (selector))), dest_uri, &error);

		if (stream) {
			gchar *ical_str = icalcomponent_as_ical_string_r (top_level);

			g_output_stream_write_all (stream, ical_str, strlen (ical_str), NULL, NULL, &error);
			g_output_stream_close (stream, NULL, NULL);

			g_object_unref (stream);
			g_free (ical_str);
		}

		e_cal_client_free_icalcomp_slist (objects);
	}

	if (error != NULL) {
		display_error_message (
			gtk_widget_get_toplevel (GTK_WIDGET (selector)),
			error->message);
		g_error_free (error);
	}

	/* terminate */
	g_object_unref (source_client);
	icalcomponent_free (top_level);
}

FormatHandler *
ical_format_handler_new (void)
{
	FormatHandler *handler = g_new0 (FormatHandler, 1);

	handler->isdefault = TRUE;
	handler->combo_label = _("iCalendar (.ics)");
	handler->filename_ext = ".ics";
	handler->options_widget = NULL;
	handler->save = do_save_calendar_ical;
	handler->data = NULL;

	return handler;
}
