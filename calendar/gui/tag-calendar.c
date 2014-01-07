/*
 *
 * Evolution calendar - Utilities for tagging ECalendar widgets
 *
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
 *		Damon Chaplin <damon@ximian.com>
 *      Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "shell/e-shell.h"
#include "calendar-config.h"
#include "tag-calendar.h"

struct calendar_tag_closure {
	ECalendarItem *calitem;
	icaltimezone *zone;
	time_t start_time;
	time_t end_time;

	gboolean skip_transparent_events;
	gboolean recur_events_italic;
};

/* Clears all the tags in a calendar and fills a closure structure with the
 * necessary information for iterating over occurrences.  Returns FALSE if
 * the calendar has no dates shown.  */
static gboolean
prepare_tag (ECalendar *ecal,
             struct calendar_tag_closure *closure,
             icaltimezone *zone,
             gboolean clear_first)
{
	gint start_year, start_month, start_day;
	gint end_year, end_month, end_day;
	struct icaltimetype start_tt = icaltime_null_time ();
	struct icaltimetype end_tt = icaltime_null_time ();

	if (clear_first)
		e_calendar_item_clear_marks (ecal->calitem);

	if (!e_calendar_item_get_date_range (
		ecal->calitem,
		&start_year, &start_month, &start_day,
		&end_year, &end_month, &end_day))
		return FALSE;

	start_tt.year = start_year;
	start_tt.month = start_month + 1;
	start_tt.day = start_day;

	end_tt.year = end_year;
	end_tt.month = end_month + 1;
	end_tt.day = end_day;

	icaltime_adjust (&end_tt, 1, 0, 0, 0);

	closure->calitem = ecal->calitem;

	if (zone != NULL)
		closure->zone = zone;
	else
		closure->zone = calendar_config_get_icaltimezone ();

	closure->start_time =
		icaltime_as_timet_with_zone (start_tt, closure->zone);
	closure->end_time =
		icaltime_as_timet_with_zone (end_tt, closure->zone);

	return TRUE;
}

/* Marks the specified range in an ECalendar;
 * called from e_cal_generate_instances() */
static gboolean
tag_calendar_cb (ECalComponent *comp,
                 time_t istart,
                 time_t iend,
                 struct calendar_tag_closure *closure)
{
	struct icaltimetype start_tt, end_tt;
	ECalComponentTransparency transparency;
	guint8 style = 0;

	/* If we are skipping TRANSPARENT events, return if the event is
	 * transparent. */
	e_cal_component_get_transparency (comp, &transparency);
	if (transparency == E_CAL_COMPONENT_TRANSP_TRANSPARENT) {
		if (closure->skip_transparent_events)
			return TRUE;

		style = E_CALENDAR_ITEM_MARK_ITALIC;
	} else if (closure->recur_events_italic && e_cal_component_is_instance (comp)) {
		style = E_CALENDAR_ITEM_MARK_ITALIC;
	} else {
		style = E_CALENDAR_ITEM_MARK_BOLD;
	}

	start_tt = icaltime_from_timet_with_zone (istart, FALSE, closure->zone);
	end_tt = icaltime_from_timet_with_zone (iend - 1, FALSE, closure->zone);

	e_calendar_item_mark_days (
		closure->calitem,
		start_tt.year, start_tt.month - 1, start_tt.day,
		end_tt.year, end_tt.month - 1, end_tt.day,
		style, TRUE);

	return TRUE;
}

/**
 * tag_calendar_by_client:
 * @ecal: Calendar widget to tag.
 * @client: A calendar client object.
 * @cancellable: A #GCancellable; can be %NULL
 *
 * Tags an #ECalendar widget with the events that occur in its current time
 * range.  The occurrences are extracted from the specified calendar @client.
 **/
void
tag_calendar_by_client (ECalendar *ecal,
                        ECalClient *client,
                        GCancellable *cancellable)
{
	GSettings *settings;
	struct calendar_tag_closure *closure;

	g_return_if_fail (E_IS_CALENDAR (ecal));
	g_return_if_fail (E_IS_CAL_CLIENT (client));

	/* If the ECalendar isn't visible, we just return. */
	if (!gtk_widget_get_visible (GTK_WIDGET (ecal)))
		return;

	closure = g_new0 (struct calendar_tag_closure, 1);

	if (!prepare_tag (ecal, closure, NULL, TRUE)) {
		g_free (closure);
		return;
	}

	settings = g_settings_new ("org.gnome.evolution.calendar");

	closure->skip_transparent_events = TRUE;
	closure->recur_events_italic =
		g_settings_get_boolean (settings, "recur-events-italic");

	g_object_unref (settings);

	e_cal_client_generate_instances (
		client, closure->start_time, closure->end_time, cancellable,
		(ECalRecurInstanceFn) tag_calendar_cb,
		closure, (GDestroyNotify) g_free);
}

/* Resolves TZIDs for the recurrence generator, for when the comp is not on
 * the server. We need to try to use builtin timezones first, as they may not
 * be added to the server yet. */
static icaltimezone *
resolve_tzid_cb (const gchar *tzid,
                 ECalClient *client)
{
	icaltimezone *zone = NULL;

	/* Try to find the builtin timezone first. */
	zone = icaltimezone_get_builtin_timezone_from_tzid (tzid);

	if (!zone && tzid) {
		/* FIXME: Handle errors. */
		GError *error = NULL;

		e_cal_client_get_timezone_sync (
			client, tzid, &zone, NULL, &error);

		if (error != NULL) {
			g_warning (
				"%s: Failed to get timezone '%s': %s",
				G_STRFUNC, tzid, error->message);
			g_error_free (error);
		}
	}

	return zone;
}

/**
 * tag_calendar_by_comp:
 * @ecal: Calendar widget to tag.
 * @comp: A calendar component object.
 * @clear_first: Whether the #ECalendar should be cleared of any marks first.
 *
 * Tags an #ECalendar widget with any occurrences of a specific calendar
 * component that occur within the calendar's current time range.
 * Note that TRANSPARENT events are also tagged here.
 *
 * If comp_is_on_server is FALSE, it will try to resolve TZIDs using builtin
 * timezones first, before querying the server, since the timezones may not
 * have been added to the calendar on the server yet.
 **/
void
tag_calendar_by_comp (ECalendar *ecal,
                      ECalComponent *comp,
                      ECalClient *client,
                      icaltimezone *display_zone,
                      gboolean clear_first,
                      gboolean comp_is_on_server,
                      gboolean can_recur_events_italic,
                      GCancellable *cancellable)
{
	GSettings *settings;
	struct calendar_tag_closure closure;

	g_return_if_fail (E_IS_CALENDAR (ecal));
	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	/* If the ECalendar isn't visible, we just return. */
	if (!gtk_widget_get_visible (GTK_WIDGET (ecal)))
		return;

	if (!prepare_tag (ecal, &closure, display_zone, clear_first))
		return;

	settings = g_settings_new ("org.gnome.evolution.calendar");

	closure.skip_transparent_events = FALSE;
	closure.recur_events_italic =
		can_recur_events_italic &&
		g_settings_get_boolean (settings, "recur-events-italic");

	g_object_unref (settings);

	if (comp_is_on_server) {
		struct calendar_tag_closure *alloced_closure;

		alloced_closure = g_new0 (struct calendar_tag_closure, 1);

		*alloced_closure = closure;

		e_cal_client_generate_instances_for_object (
			client, e_cal_component_get_icalcomponent (comp),
			closure.start_time, closure.end_time, cancellable,
			(ECalRecurInstanceFn) tag_calendar_cb,
			alloced_closure, (GDestroyNotify) g_free);
	} else
		e_cal_recur_generate_instances (
			comp, closure.start_time, closure.end_time,
			(ECalRecurInstanceFn) tag_calendar_cb,
			&closure,
			(ECalRecurResolveTimezoneFn) resolve_tzid_cb,
			client, closure.zone);
}
