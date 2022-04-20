/*
 * Evolution calendar - Utilities for manipulating ECalComponent objects
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
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include <string.h>
#include <time.h>

#include "calendar-config.h"
#include "comp-util.h"
#include "e-calendar-view.h"
#include "itip-utils.h"

#include "shell/e-shell-window.h"
#include "shell/e-shell-view.h"

/**
 * cal_comp_util_add_exdate:
 * @comp: A calendar component object.
 * @t: Time for the exception.
 * @zone: an #ICalTimezone
 *
 * Adds an exception date to the current list of EXDATE properties in a calendar
 * component object.
 **/
void
cal_comp_util_add_exdate (ECalComponent *comp,
			  time_t t,
			  ICalTimezone *zone)
{
	GSList *exdates;
	ECalComponentDateTime *cdt;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	exdates = e_cal_component_get_exdates (comp);

	cdt = e_cal_component_datetime_new_take (i_cal_time_new_from_timet_with_zone (t, FALSE, zone),
		zone ? g_strdup (i_cal_timezone_get_tzid (zone)) : NULL);

	exdates = g_slist_append (exdates, cdt);
	e_cal_component_set_exdates (comp, exdates);

	g_slist_free_full (exdates, e_cal_component_datetime_free);
}

/* Returns TRUE if the TZIDs are equivalent, i.e. both NULL or the same. */
static gboolean
cal_comp_util_tzid_equal (const gchar *tzid1,
			  const gchar *tzid2)
{
	gboolean retval = TRUE;

	if (tzid1) {
		if (!tzid2 || strcmp (tzid1, tzid2))
			retval = FALSE;
	} else {
		if (tzid2)
			retval = FALSE;
	}

	return retval;
}

/**
 * cal_comp_util_compare_event_timezones:
 * @comp: A calendar component object.
 * @client: An #ECalClient.
 *
 * Checks if the component uses the given timezone for both the start and
 * the end time, or if the UTC offsets of the start and end times are the same
 * as in the given zone.
 *
 * Returns: TRUE if the component's start and end time are at the same UTC
 * offset in the given timezone.
 **/
gboolean
cal_comp_util_compare_event_timezones (ECalComponent *comp,
				       ECalClient *client,
				       ICalTimezone *zone)
{
	ECalComponentDateTime *start_datetime, *end_datetime;
	const gchar *tzid;
	gboolean retval = FALSE;
	ICalTimezone *start_zone = NULL;
	ICalTimezone *end_zone = NULL;
	gint offset1, offset2;

	tzid = i_cal_timezone_get_tzid (zone);

	start_datetime = e_cal_component_get_dtstart (comp);
	end_datetime = e_cal_component_get_dtend (comp);

	/* If either the DTSTART or the DTEND is a DATE value, we return TRUE.
	 * Maybe if one was a DATE-TIME we should check that, but that should
	 * not happen often. */
	if ((start_datetime && i_cal_time_is_date (e_cal_component_datetime_get_value (start_datetime))) ||
	    (end_datetime && i_cal_time_is_date (e_cal_component_datetime_get_value (end_datetime)))) {
		retval = TRUE;
		goto out;
	}

	if (!start_datetime || !end_datetime) {
		if (e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_EVENT) {
			retval = FALSE;
		} else if (start_datetime || end_datetime) {
			ECalComponentDateTime *dt = start_datetime ? start_datetime : end_datetime;

			retval = !e_cal_component_datetime_get_tzid (dt) ||
			         cal_comp_util_tzid_equal (tzid, e_cal_component_datetime_get_tzid (dt));

			if (!retval) {
				ICalTimezone *dt_zone;

				if (!e_cal_client_get_timezone_sync (client, e_cal_component_datetime_get_tzid (dt), &dt_zone, NULL, NULL))
					dt_zone = NULL;

				if (dt_zone) {
					gint is_daylight = 0; /* Its value is ignored, but libical-glib 3.0.5 API requires it */

					offset1 = i_cal_timezone_get_utc_offset (dt_zone,
						e_cal_component_datetime_get_value (dt),
						&is_daylight);
					offset2 = i_cal_timezone_get_utc_offset (zone,
						e_cal_component_datetime_get_value (dt),
						&is_daylight);

					retval = offset1 == offset2;
				}
			}
		} else {
			retval = TRUE;
		}

		goto out;
	}

	/* If the event uses UTC for DTSTART & DTEND, return TRUE. Outlook
	 * will send single events as UTC, so we don't want to mark all of
	 * these. */
	if (i_cal_time_is_utc (e_cal_component_datetime_get_value (start_datetime)) &&
	    i_cal_time_is_utc (e_cal_component_datetime_get_value (end_datetime))) {
		retval = TRUE;
		goto out;
	}

	/* If the event uses floating time for DTSTART & DTEND, return TRUE.
	 * Imported vCalendar files will use floating times, so we don't want
	 * to mark all of these. */
	if (!e_cal_component_datetime_get_tzid (start_datetime) &&
	    !e_cal_component_datetime_get_tzid (end_datetime)) {
		retval = TRUE;
		goto out;
	}

	/* FIXME: DURATION may be used instead. */
	if (cal_comp_util_tzid_equal (tzid, e_cal_component_datetime_get_tzid (start_datetime)) &&
	    cal_comp_util_tzid_equal (tzid, e_cal_component_datetime_get_tzid (end_datetime))) {
		/* If both TZIDs are the same as the given zone's TZID, then
		 * we know the timezones are the same so we return TRUE. */
		retval = TRUE;
	} else {
		gint is_daylight = 0; /* Its value is ignored, but libical-glib 3.0.5 API requires it */

		/* If the TZIDs differ, we have to compare the UTC offsets
		 * of the start and end times, using their own timezones and
		 * the given timezone. */
		if (!e_cal_component_datetime_get_tzid (start_datetime) ||
		    !e_cal_client_get_timezone_sync (client, e_cal_component_datetime_get_tzid (start_datetime), &start_zone, NULL, NULL))
			start_zone = NULL;

		if (start_zone == NULL)
			goto out;

		if (e_cal_component_datetime_get_value (start_datetime)) {
			offset1 = i_cal_timezone_get_utc_offset (
				start_zone,
				e_cal_component_datetime_get_value (start_datetime),
				&is_daylight);
			offset2 = i_cal_timezone_get_utc_offset (
				zone,
				e_cal_component_datetime_get_value (start_datetime),
				&is_daylight);
			if (offset1 != offset2)
				goto out;
		}

		if (!e_cal_component_datetime_get_tzid (end_datetime) ||
		    !e_cal_client_get_timezone_sync (client, e_cal_component_datetime_get_tzid (end_datetime), &end_zone, NULL, NULL))
			end_zone = NULL;

		if (end_zone == NULL)
			goto out;

		if (e_cal_component_datetime_get_value (end_datetime)) {
			offset1 = i_cal_timezone_get_utc_offset (
				end_zone,
				e_cal_component_datetime_get_value (end_datetime),
				&is_daylight);
			offset2 = i_cal_timezone_get_utc_offset (
				zone,
				e_cal_component_datetime_get_value (end_datetime),
				&is_daylight);
			if (offset1 != offset2)
				goto out;
		}

		retval = TRUE;
	}

 out:

	e_cal_component_datetime_free (start_datetime);
	e_cal_component_datetime_free (end_datetime);

	return retval;
}

/**
 * cal_comp_is_on_server_sync:
 * @comp: an #ECalComponent
 * @client: an #ECalClient
 * @cancellable: (allow none): a #GCancellable
 * @error: (out): (allow none): a #GError
 *
 * Checks whether @client contains @comp. A "No Such Object" error is not
 * propagated to the caller, any other errors are.
 *
 * Returns: #TRUE, when the @client contains @comp, #FALSE when not or on error.
 *    The @error is not set when the @client doesn't contain the @comp.
 **/
gboolean
cal_comp_is_on_server_sync (ECalComponent *comp,
			    ECalClient *client,
			    GCancellable *cancellable,
			    GError **error)
{
	const gchar *uid;
	gchar *rid = NULL;
	ICalComponent *icomp = NULL;
	GError *local_error = NULL;

	g_return_val_if_fail (comp != NULL, FALSE);
	g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), FALSE);
	g_return_val_if_fail (client != NULL, FALSE);
	g_return_val_if_fail (E_IS_CAL_CLIENT (client), FALSE);

	/* See if the component is on the server.  If it is not, then it likely
	 * means that the appointment is new, only in the day view, and we
	 * haven't added it yet to the server.  In that case, we don't need to
	 * confirm and we can just delete the event.  Otherwise, we ask
	 * the user.
	 */
	uid = e_cal_component_get_uid (comp);

	/* TODO We should not be checking for this here. But since
	 *      e_cal_util_construct_instance does not create the instances
	 *      of all day events, so we default to old behaviour. */
	if (e_cal_client_check_recurrences_no_master (client)) {
		rid = e_cal_component_get_recurid_as_string (comp);
	}

	if (e_cal_client_get_object_sync (client, uid, rid, &icomp, cancellable, &local_error) &&
	    icomp != NULL) {
		g_object_unref (icomp);
		g_free (rid);

		return TRUE;
	}

	if (g_error_matches (local_error, E_CAL_CLIENT_ERROR, E_CAL_CLIENT_ERROR_OBJECT_NOT_FOUND))
		g_clear_error (&local_error);
	else
		g_propagate_error (error, local_error);

	g_free (rid);

	return FALSE;
}

/**
 * cal_comp_is_icalcomp_on_server_sync:
 * The same as cal_comp_is_on_server_sync(), only the component parameter is
 * ICalComponent, not the ECalComponent.
 **/
gboolean
cal_comp_is_icalcomp_on_server_sync (ICalComponent *icomp,
				     ECalClient *client,
				     GCancellable *cancellable,
				     GError **error)
{
	gboolean on_server;
	ECalComponent *comp;

	if (!icomp || !client || !i_cal_component_get_uid (icomp))
		return FALSE;

	comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (icomp));
	if (!comp)
		return FALSE;

	on_server = cal_comp_is_on_server_sync (comp, client, cancellable, error);

	g_object_unref (comp);

	return on_server;
}

static ECalComponent *
cal_comp_util_ref_default_object (ECalClient *client,
				  ICalComponentKind icomp_kind,
				  ECalComponentVType ecomp_vtype,
				  GCancellable *cancellable,
				  GError **error)
{
	ICalComponent *icomp = NULL;
	ECalComponent *comp;

	/* Simply ignore errors here */
	if (client && !e_cal_client_get_default_object_sync (client, &icomp, cancellable, NULL))
		icomp = NULL;

	if (!icomp)
		icomp = i_cal_component_new (icomp_kind);

	comp = e_cal_component_new ();

	if (!e_cal_component_set_icalcomponent (comp, icomp)) {
		g_clear_object (&icomp);

		e_cal_component_set_new_vtype (comp, ecomp_vtype);
	}

	return comp;
}

/**
 * cal_comp_event_new_with_defaults_sync:
 *
 * Creates a new VEVENT component and adds any default alarms to it as set in
 * the program's configuration values, but only if not the all_day event.
 *
 * Return value: A newly-created calendar component.
 **/
ECalComponent *
cal_comp_event_new_with_defaults_sync (ECalClient *client,
				       gboolean all_day,
				       gboolean use_default_reminder,
				       gint default_reminder_interval,
				       EDurationType default_reminder_units,
				       GCancellable *cancellable,
				       GError **error)
{
	ECalComponent *comp;
	ECalComponentAlarm *alarm;
	ICalProperty *prop;
	ICalDuration *duration;
	ECalComponentAlarmTrigger *trigger;

	comp = cal_comp_util_ref_default_object (client, I_CAL_VEVENT_COMPONENT, E_CAL_COMPONENT_EVENT, cancellable, error);

	if (!comp)
		return NULL;

	if (all_day || !use_default_reminder)
		return comp;

	alarm = e_cal_component_alarm_new ();

	/* We don't set the description of the alarm; we'll copy it from the
	 * summary when it gets committed to the server. For that, we add a
	 * X-EVOLUTION-NEEDS-DESCRIPTION property to the alarm's component.
	 */
	prop = i_cal_property_new_x ("1");
	i_cal_property_set_x_name (prop, "X-EVOLUTION-NEEDS-DESCRIPTION");
	e_cal_component_property_bag_take (e_cal_component_alarm_get_property_bag (alarm), prop);

	e_cal_component_alarm_set_action (alarm, E_CAL_COMPONENT_ALARM_DISPLAY);

	duration = i_cal_duration_new_null_duration ();
	i_cal_duration_set_is_neg (duration, TRUE);

	switch (default_reminder_units) {
	case E_DURATION_MINUTES:
		i_cal_duration_set_minutes (duration, default_reminder_interval);
		break;

	case E_DURATION_HOURS:
		i_cal_duration_set_hours (duration, default_reminder_interval);
		break;

	case E_DURATION_DAYS:
		i_cal_duration_set_days (duration, default_reminder_interval);
		break;

	default:
		g_warning ("wrong units %d\n", default_reminder_units);
	}

	trigger = e_cal_component_alarm_trigger_new_relative (E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START, duration);
	g_clear_object (&duration);

	e_cal_component_alarm_take_trigger (alarm, trigger);

	e_cal_component_add_alarm (comp, alarm);
	e_cal_component_alarm_free (alarm);

	return comp;
}

ECalComponent *
cal_comp_event_new_with_current_time_sync (ECalClient *client,
					   gboolean all_day,
					   gboolean use_default_reminder,
					   gint default_reminder_interval,
					   EDurationType default_reminder_units,
					   GCancellable *cancellable,
					   GError **error)
{
	ECalComponent *comp;
	ICalTime *itt;
	ECalComponentDateTime *dt;
	ICalTimezone *zone;

	comp = cal_comp_event_new_with_defaults_sync (
		client, all_day, use_default_reminder,
		default_reminder_interval, default_reminder_units,
		cancellable, error);
	if (!comp)
		return NULL;

	zone = calendar_config_get_icaltimezone ();

	if (all_day) {
		itt = i_cal_time_new_from_timet_with_zone (time (NULL), 1, zone);

		dt = e_cal_component_datetime_new_take (itt, zone ? g_strdup (i_cal_timezone_get_tzid (zone)) : NULL);

		e_cal_component_set_dtstart (comp, dt);
		e_cal_component_set_dtend (comp, dt);
	} else {
		GSettings *settings;
		gint shorten_by;

		itt = i_cal_time_new_current_with_zone (zone);
		i_cal_time_adjust (itt, 0, 1, -i_cal_time_get_minute (itt), -i_cal_time_get_second (itt));

		dt = e_cal_component_datetime_new_take (itt, zone ? g_strdup (i_cal_timezone_get_tzid (zone)) : NULL);

		e_cal_component_set_dtstart (comp, dt);

		i_cal_time_adjust (e_cal_component_datetime_get_value (dt), 0, 1, 0, 0);

		settings = e_util_ref_settings ("org.gnome.evolution.calendar");
		shorten_by = g_settings_get_int (settings, "shorten-end-time");
		g_clear_object (&settings);

		if (shorten_by > 0 && shorten_by < 60)
			i_cal_time_adjust (e_cal_component_datetime_get_value (dt), 0, 0, -shorten_by, 0);

		e_cal_component_set_dtend (comp, dt);
	}

	e_cal_component_datetime_free (dt);

	return comp;
}

ECalComponent *
cal_comp_task_new_with_defaults_sync (ECalClient *client,
				      GCancellable *cancellable,
				      GError **error)
{
	ECalComponent *comp;

	comp = cal_comp_util_ref_default_object (client, I_CAL_VTODO_COMPONENT, E_CAL_COMPONENT_TODO, cancellable, error);

	return comp;
}

ECalComponent *
cal_comp_memo_new_with_defaults_sync (ECalClient *client,
				      GCancellable *cancellable,
				      GError **error)
{
	ECalComponent *comp;

	comp = cal_comp_util_ref_default_object (client, I_CAL_VJOURNAL_COMPONENT, E_CAL_COMPONENT_JOURNAL, cancellable, error);

	return comp;
}

void
cal_comp_update_time_by_active_window (ECalComponent *comp,
                                       EShell *shell)
{
	GtkWindow *window;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (shell != NULL);

	window = e_shell_get_active_window (shell);

	if (E_IS_SHELL_WINDOW (window)) {
		EShellWindow *shell_window;
		const gchar *active_view;

		shell_window = E_SHELL_WINDOW (window);
		active_view = e_shell_window_get_active_view (shell_window);

		if (g_strcmp0 (active_view, "calendar") == 0) {
			EShellContent *shell_content;
			EShellView *shell_view;
			ECalendarView *cal_view;
			time_t start = 0, end = 0;
			ICalTimezone *zone;
			ICalTime *itt;
			ICalComponent *icomp;
			ICalProperty *prop;

			shell_view = e_shell_window_peek_shell_view (shell_window, "calendar");
			g_return_if_fail (shell_view != NULL);

			cal_view = NULL;
			shell_content = e_shell_view_get_shell_content (shell_view);
			g_object_get (shell_content, "current-view", &cal_view, NULL);
			g_return_if_fail (cal_view != NULL);
			g_return_if_fail (e_calendar_view_get_visible_time_range (cal_view, &start, &end));

			zone = e_cal_model_get_timezone (e_calendar_view_get_model (cal_view));
			itt = i_cal_time_new_from_timet_with_zone (start, FALSE, zone);

			icomp = e_cal_component_get_icalcomponent (comp);
			prop = i_cal_component_get_first_property (icomp, I_CAL_DTSTART_PROPERTY);
			if (prop) {
				i_cal_property_set_dtstart (prop, itt);
				g_object_unref (prop);
			} else {
				prop = i_cal_property_new_dtstart (itt);
				i_cal_component_take_property (icomp, prop);
			}

			g_clear_object (&cal_view);
			g_object_unref (itt);
		}
	}
}

/**
 * cal_comp_util_get_n_icons:
 * @comp: A calendar component object.
 * @pixbufs: List of pixbufs to use. Can be NULL.
 *
 * Get the number of icons owned by the component.
 * Each member of pixmaps should be freed with g_object_unref
 * and the list itself should be freed too.
 *
 * Returns: the number of icons owned by the component.
 **/
gint
cal_comp_util_get_n_icons (ECalComponent *comp,
                           GSList **pixbufs)
{
	GSList *categories_list, *elem;
	gint num_icons = 0;

	g_return_val_if_fail (comp != NULL, 0);
	g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), 0);

	categories_list = e_cal_component_get_categories_list (comp);
	for (elem = categories_list; elem; elem = elem->next) {
		const gchar *category;
		GdkPixbuf *pixbuf = NULL;

		category = elem->data;
		if (e_categories_config_get_icon_for (category, &pixbuf)) {
			if (!pixbuf)
				continue;

			num_icons++;

			if (pixbufs) {
				*pixbufs = g_slist_append (*pixbufs, pixbuf);
			} else {
				g_object_unref (pixbuf);
			}
		}
	}
	g_slist_free_full (categories_list, g_free);

	return num_icons;
}

/**
 * cal_comp_selection_set_string_list
 * @data: Selection data, where to put list of strings.
 * @str_list: List of strings. (Each element is of type const gchar *.)
 *
 * Stores list of strings into selection target data.  Use
 * cal_comp_selection_get_string_list() to get this list from target data.
 **/
void
cal_comp_selection_set_string_list (GtkSelectionData *data,
                                    GSList *str_list)
{
	/* format is "str1\0str2\0...strN\0" */
	GSList *p;
	GByteArray *array;
	GdkAtom target;

	g_return_if_fail (data != NULL);

	if (!str_list)
		return;

	array = g_byte_array_new ();
	for (p = str_list; p; p = p->next) {
		const guint8 *c = p->data;

		if (c)
			g_byte_array_append (array, c, strlen ((const gchar *) c) + 1);
	}

	target = gtk_selection_data_get_target (data);
	gtk_selection_data_set (data, target, 8, array->data, array->len);
	g_byte_array_free (array, TRUE);
}

/**
 * cal_comp_selection_get_string_list
 * @data: Selection data, where to put list of strings.
 *
 * Converts data from selection to list of strings. Data should be assigned
 * to selection data with cal_comp_selection_set_string_list().
 * Each string in newly created list should be freed by g_free().
 * List itself should be freed by g_slist_free().
 *
 * Returns: Newly allocated #GSList of strings.
 **/
GSList *
cal_comp_selection_get_string_list (GtkSelectionData *selection_data)
{
	/* format is "str1\0str2\0...strN\0" */
	gchar *inptr, *inend;
	GSList *list;
	const guchar *data;
	gint length;

	g_return_val_if_fail (selection_data != NULL, NULL);

	data = gtk_selection_data_get_data (selection_data);
	length = gtk_selection_data_get_length (selection_data);

	list = NULL;
	inptr = (gchar *) data;
	inend = (gchar *) (data + length);

	while (inptr < inend) {
		gchar *start = inptr;

		while (inptr < inend && *inptr)
			inptr++;

		list = g_slist_prepend (list, g_strndup (start, inptr - start));

		inptr++;
	}

	return list;
}

static void
datetime_to_zone (ECalClient *client,
                  ECalComponentDateTime *date,
                  const gchar *tzid)
{
	ICalTimezone *from, *to;

	g_return_if_fail (date != NULL);

	if (!e_cal_component_datetime_get_tzid (date) || !tzid ||
	    e_cal_component_datetime_get_tzid (date) == tzid ||
	    g_str_equal (e_cal_component_datetime_get_tzid (date), tzid))
		return;

	from = i_cal_timezone_get_builtin_timezone_from_tzid (e_cal_component_datetime_get_tzid (date));
	if (!from) {
		GError *error = NULL;

		if (!e_cal_client_get_timezone_sync (client, e_cal_component_datetime_get_tzid (date), &from, NULL, &error))
			from = NULL;

		if (error != NULL) {
			g_warning (
				"%s: Could not get timezone '%s' from server: %s",
				G_STRFUNC, e_cal_component_datetime_get_tzid (date) ? e_cal_component_datetime_get_tzid (date) : "",
				error->message);
			g_error_free (error);
		}
	}

	to = i_cal_timezone_get_builtin_timezone_from_tzid (tzid);
	if (!to) {
		/* do not check failure here, maybe the zone is not available there */
		if (!e_cal_client_get_timezone_sync (client, tzid, &to, NULL, NULL))
			to = NULL;
	}

	i_cal_time_convert_timezone (e_cal_component_datetime_get_value (date), from, to);
	e_cal_component_datetime_set_tzid (date, tzid);
}

/**
 * cal_comp_set_dtstart_with_oldzone:
 * @client: ECalClient structure, to retrieve timezone from, when required.
 * @comp: Component, where make the change.
 * @pdate: Value, to change to.
 *
 * Changes 'dtstart' of the component, but converts time to the old timezone.
 **/
void
cal_comp_set_dtstart_with_oldzone (ECalClient *client,
                                   ECalComponent *comp,
                                   const ECalComponentDateTime *pdate)
{
	ECalComponentDateTime *olddate, *date;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (pdate != NULL);

	olddate = e_cal_component_get_dtstart (comp);
	date = e_cal_component_datetime_copy (pdate);

	datetime_to_zone (client, date, e_cal_component_datetime_get_tzid (olddate));
	e_cal_component_set_dtstart (comp, date);

	e_cal_component_datetime_free (olddate);
	e_cal_component_datetime_free (date);
}

/**
 * cal_comp_set_dtend_with_oldzone:
 * @client: ECalClient structure, to retrieve timezone from, when required.
 * @comp: Component, where make the change.
 * @pdate: Value, to change to.
 *
 * Changes 'dtend' of the component, but converts time to the old timezone.
 **/
void
cal_comp_set_dtend_with_oldzone (ECalClient *client,
                                 ECalComponent *comp,
                                 const ECalComponentDateTime *pdate)
{
	ECalComponentDateTime *olddate, *date;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (pdate != NULL);

	olddate = e_cal_component_get_dtend (comp);
	date = e_cal_component_datetime_copy (pdate);

	datetime_to_zone (client, date, e_cal_component_datetime_get_tzid (olddate));
	e_cal_component_set_dtend (comp, date);

	e_cal_component_datetime_free (olddate);
	e_cal_component_datetime_free (date);
}

gboolean
comp_util_sanitize_recurrence_master_sync (ECalComponent *comp,
					   ECalClient *client,
					   GCancellable *cancellable,
					   GError **error)
{
	ECalComponent *master = NULL;
	ICalComponent *icomp = NULL;
	ECalComponentRange *rid;
	ECalComponentDateTime *sdt, *rdt;
	const gchar *uid;

	/* Get the master component */
	uid = e_cal_component_get_uid (comp);

	if (!e_cal_client_get_object_sync (client, uid, NULL, &icomp, cancellable, error))
		return FALSE;

	master = e_cal_component_new_from_icalcomponent (icomp);
	if (!master) {
		g_warn_if_reached ();
		return FALSE;
	}

	/* Compare recur id and start date */
	rid = e_cal_component_get_recurid (comp);
	sdt = e_cal_component_get_dtstart (comp);
	rdt = rid ? e_cal_component_range_get_datetime (rid) : NULL;

	if (rdt && sdt &&
	    i_cal_time_compare_date_only (e_cal_component_datetime_get_value (rdt), e_cal_component_datetime_get_value (sdt)) == 0) {
		ECalComponentDateTime *msdt, *medt, *edt;
		gint yy = 0, mm = 0, dd = 0;
		gint64 diff;

		msdt = e_cal_component_get_dtstart (master);
		medt = e_cal_component_get_dtend (master);

		edt = e_cal_component_get_dtend (comp);

		if (!msdt || !medt || !edt) {
			g_warn_if_reached ();
			e_cal_component_datetime_free (msdt);
			e_cal_component_datetime_free (medt);
			e_cal_component_datetime_free (edt);
			e_cal_component_datetime_free (sdt);
			e_cal_component_range_free (rid);
			g_object_unref (master);
			return FALSE;
		}

		/* Consider the day difference only, because the time is preserved */
		diff = (i_cal_time_as_timet (e_cal_component_datetime_get_value (edt)) -
			i_cal_time_as_timet (e_cal_component_datetime_get_value (sdt))) / (24 * 60 * 60);

		i_cal_time_get_date (e_cal_component_datetime_get_value (msdt), &yy, &mm, &dd);
		i_cal_time_set_date (e_cal_component_datetime_get_value (sdt), yy, mm, dd);
		i_cal_time_set_date (e_cal_component_datetime_get_value (edt), yy, mm, dd);

		if (diff)
			i_cal_time_adjust (e_cal_component_datetime_get_value (edt), diff, 0, 0, 0);

		/* Make sure the DATE value of the DTSTART and DTEND do not match */
		if (i_cal_time_is_date (e_cal_component_datetime_get_value (sdt)) &&
		    i_cal_time_is_date (e_cal_component_datetime_get_value (edt)) &&
		    i_cal_time_compare_date_only (e_cal_component_datetime_get_value (sdt), e_cal_component_datetime_get_value (edt)) == 0) {
			i_cal_time_adjust (e_cal_component_datetime_get_value (edt), 1, 0, 0, 0);
		}

		e_cal_component_set_dtstart (comp, sdt);
		e_cal_component_set_dtend (comp, edt);

		e_cal_component_abort_sequence (comp);

		e_cal_component_datetime_free (msdt);
		e_cal_component_datetime_free (medt);
		e_cal_component_datetime_free (edt);
	}

	e_cal_component_set_recurid (comp, NULL);

	e_cal_component_datetime_free (sdt);
	e_cal_component_range_free (rid);
	g_object_unref (master);

	return TRUE;
}

gchar *
comp_util_suggest_filename (ICalComponent *icomp,
			    const gchar *default_name)
{
	ICalProperty *prop;
	const gchar *summary = NULL;
	gchar *filename;

	if (!icomp)
		return g_strconcat (default_name, ".ics", NULL);

	prop = i_cal_component_get_first_property (icomp, I_CAL_SUMMARY_PROPERTY);
	if (prop)
		summary = i_cal_property_get_summary (prop);

	if (!summary || !*summary)
		summary = default_name;

	filename = g_strconcat (summary, ".ics", NULL);

	g_clear_object (&prop);

	return filename;
}

void
cal_comp_get_instance_times (ECalClient *client,
			     ICalComponent *icomp,
			     const ICalTimezone *default_zone,
			     ICalTime **out_instance_start,
			     ICalTime **out_instance_end,
			     GCancellable *cancellable)
{
	ICalTime *start_time, *end_time;
	const ICalTimezone *zone;

	g_return_if_fail (E_IS_CAL_CLIENT (client));
	g_return_if_fail (icomp != NULL);
	g_return_if_fail (out_instance_start != NULL);
	g_return_if_fail (out_instance_end != NULL);

	start_time = i_cal_component_get_dtstart (icomp);
	end_time = i_cal_component_get_dtend (icomp);

	/* Some event can have missing DTEND, then use the start_time for them */
	if (!end_time || i_cal_time_is_null_time (end_time)) {
		g_clear_object (&end_time);

		end_time = i_cal_time_clone (start_time);
	}

	zone = NULL;

	if (i_cal_time_get_timezone (start_time)) {
		zone = i_cal_time_get_timezone (start_time);
	} else {
		ICalParameter *param = NULL;
		ICalProperty *prop = i_cal_component_get_first_property (icomp, I_CAL_DTSTART_PROPERTY);

		if (prop) {
			param = i_cal_property_get_first_parameter (prop, I_CAL_TZID_PARAMETER);

			if (param) {
				const gchar *tzid = NULL;
				ICalTimezone *st_zone = NULL;

				tzid = i_cal_parameter_get_tzid (param);
				if (tzid && !e_cal_client_get_timezone_sync (client, tzid, &st_zone, cancellable, NULL))
					st_zone = NULL;

				if (st_zone)
					zone = st_zone;

				g_object_unref (param);
			}

			g_object_unref (prop);
		}
	}

	if (!zone)
		zone = default_zone;

	*out_instance_start = i_cal_time_clone (start_time);
	if (i_cal_time_is_date (*out_instance_start)) {
		i_cal_time_set_is_date (*out_instance_start, FALSE);
		i_cal_time_set_timezone (*out_instance_start, zone);
		i_cal_time_set_is_date (*out_instance_start, TRUE);
	} else {
		i_cal_time_set_timezone (*out_instance_start, zone);
	}

	zone = NULL;

	if (i_cal_time_get_timezone (end_time)) {
		zone = i_cal_time_get_timezone (end_time);
	} else {
		ICalParameter *param = NULL;
		ICalProperty *prop = i_cal_component_get_first_property (icomp, I_CAL_DTEND_PROPERTY);

		if (!prop)
			prop = i_cal_component_get_first_property (icomp, I_CAL_DTSTART_PROPERTY);

		if (prop) {
			param = i_cal_property_get_first_parameter (prop, I_CAL_TZID_PARAMETER);

			if (param) {
				const gchar *tzid = NULL;
				ICalTimezone *end_zone = NULL;

				tzid = i_cal_parameter_get_tzid (param);
				if (tzid && !e_cal_client_get_timezone_sync (client, tzid, &end_zone, cancellable, NULL))
					end_zone = NULL;

				if (end_zone)
					zone = end_zone;

				g_object_unref (param);
			}

			g_object_unref (prop);
		}
	}

	if (!zone)
		zone = default_zone;

	*out_instance_end = i_cal_time_clone (end_time);
	if (i_cal_time_is_date (*out_instance_end)) {
		i_cal_time_set_is_date (*out_instance_end, FALSE);
		i_cal_time_set_timezone (*out_instance_end, zone);
		i_cal_time_set_is_date (*out_instance_end, TRUE);
	} else {
		i_cal_time_set_timezone (*out_instance_end, zone);
	}

	g_clear_object (&start_time);
	g_clear_object (&end_time);
}

time_t
cal_comp_gdate_to_timet (const GDate *date,
			 const ICalTimezone *with_zone)
{
	struct tm tm;
	ICalTime *tt;
	time_t res;

	g_return_val_if_fail (date != NULL, (time_t) -1);
	g_return_val_if_fail (g_date_valid (date), (time_t) -1);

	g_date_to_struct_tm (date, &tm);

	tt = e_cal_util_tm_to_icaltime (&tm, TRUE);
	if (with_zone)
		res = i_cal_time_as_timet_with_zone (tt, (ICalTimezone *) with_zone);
	else
		res = i_cal_time_as_timet (tt);

	g_clear_object (&tt);

	return res;
}

typedef struct _AsyncContext {
	ECalClient *src_client;
	ICalComponent *icomp_clone;
	gboolean do_copy;
} AsyncContext;

struct ForeachTzidData
{
	ECalClient *source_client;
	ECalClient *destination_client;
	GCancellable *cancellable;
	GError **error;
	gboolean success;
};

static void
async_context_free (AsyncContext *async_context)
{
	g_clear_object (&async_context->src_client);
	g_clear_object (&async_context->icomp_clone);
	g_slice_free (AsyncContext, async_context);
}

static void
add_timezone_to_cal_cb (ICalParameter *param,
                        gpointer data)
{
	struct ForeachTzidData *ftd = data;
	ICalTimezone *tz = NULL;
	const gchar *tzid;

	g_return_if_fail (ftd != NULL);
	g_return_if_fail (ftd->source_client != NULL);
	g_return_if_fail (ftd->destination_client != NULL);

	if (!ftd->success)
		return;

	if (ftd->cancellable && g_cancellable_is_cancelled (ftd->cancellable)) {
		ftd->success = FALSE;
		return;
	}

	tzid = i_cal_parameter_get_tzid (param);
	if (!tzid || !*tzid)
		return;

	if (e_cal_client_get_timezone_sync (ftd->source_client, tzid, &tz, ftd->cancellable, NULL) && tz)
		ftd->success = e_cal_client_add_timezone_sync (
				ftd->destination_client, tz, ftd->cancellable, ftd->error);
}

/* Helper for cal_comp_transfer_item_to() */
static void
cal_comp_transfer_item_to_thread (GSimpleAsyncResult *simple,
                                  GObject *source_object,
                                  GCancellable *cancellable)
{
	AsyncContext *async_context;
	GError *local_error = NULL;

	async_context = g_simple_async_result_get_op_res_gpointer (simple);

	cal_comp_transfer_item_to_sync (
		async_context->src_client,
		E_CAL_CLIENT (source_object),
		async_context->icomp_clone,
		async_context->do_copy,
		cancellable, &local_error);

	if (local_error != NULL)
		g_simple_async_result_take_error (simple, local_error);
}

void
cal_comp_transfer_item_to (ECalClient *src_client,
			   ECalClient *dest_client,
			   ICalComponent *icomp_vcal,
			   gboolean do_copy,
			   GCancellable *cancellable,
			   GAsyncReadyCallback callback,
			   gpointer user_data)
{
	GSimpleAsyncResult *simple;
	AsyncContext *async_context;

	g_return_if_fail (E_IS_CAL_CLIENT (src_client));
	g_return_if_fail (E_IS_CAL_CLIENT (dest_client));
	g_return_if_fail (icomp_vcal != NULL);

	async_context = g_slice_new0 (AsyncContext);
	async_context->src_client = g_object_ref (src_client);
	async_context->icomp_clone = i_cal_component_clone (icomp_vcal);
	async_context->do_copy = do_copy;

	simple = g_simple_async_result_new (
		G_OBJECT (dest_client), callback, user_data,
		cal_comp_transfer_item_to);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	g_simple_async_result_set_op_res_gpointer (
		simple, async_context, (GDestroyNotify) async_context_free);

	g_simple_async_result_run_in_thread (
		simple, cal_comp_transfer_item_to_thread,
		G_PRIORITY_DEFAULT, cancellable);

	g_object_unref (simple);
}

gboolean
cal_comp_transfer_item_to_finish (ECalClient *client,
                                  GAsyncResult *result,
                                  GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (result, G_OBJECT (client), cal_comp_transfer_item_to),
		FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);

	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	return TRUE;
}

gboolean
cal_comp_transfer_item_to_sync (ECalClient *src_client,
				ECalClient *dest_client,
				ICalComponent *icomp_vcal,
				gboolean do_copy,
				GCancellable *cancellable,
				GError **error)
{
	ICalComponent *icomp;
	ICalComponent *icomp_event, *subcomp;
	ICalComponentKind icomp_kind;
	const gchar *uid;
	gchar *new_uid = NULL;
	struct ForeachTzidData ftd;
	ECalClientSourceType source_type;
	GHashTable *processed_uids;
	gboolean same_client;
	gboolean success = FALSE;

	g_return_val_if_fail (E_IS_CAL_CLIENT (src_client), FALSE);
	g_return_val_if_fail (E_IS_CAL_CLIENT (dest_client), FALSE);
	g_return_val_if_fail (icomp_vcal != NULL, FALSE);

	icomp_event = i_cal_component_get_inner (icomp_vcal);
	g_return_val_if_fail (icomp_event != NULL, FALSE);

	source_type = e_cal_client_get_source_type (src_client);
	switch (source_type) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			icomp_kind = I_CAL_VEVENT_COMPONENT;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			icomp_kind = I_CAL_VTODO_COMPONENT;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			icomp_kind = I_CAL_VJOURNAL_COMPONENT;
			break;
		default:
			g_return_val_if_reached (FALSE);
	}

	same_client = src_client == dest_client || e_source_equal (
		e_client_get_source (E_CLIENT (src_client)), e_client_get_source (E_CLIENT (dest_client)));
	processed_uids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	icomp_event = i_cal_component_get_first_component (icomp_vcal, icomp_kind);
	/*
	 * This check should be removed in the near future.
	 * We should be able to work properly with multiselection, which means that we always
	 * will receive a component with subcomponents.
	 */
	if (icomp_event == NULL)
		icomp_event = icomp_vcal;
	for (;
	     icomp_event;
	     g_object_unref (icomp_event), icomp_event = i_cal_component_get_next_component (icomp_vcal, icomp_kind)) {
		GError *local_error = NULL;

		uid = i_cal_component_get_uid (icomp_event);

		if (g_hash_table_lookup (processed_uids, uid))
			continue;

		if (do_copy && same_client)
			success = FALSE;
		else
			success = e_cal_client_get_object_sync (dest_client, uid, NULL, &icomp, cancellable, &local_error);
		if (success) {
			success = e_cal_client_modify_object_sync (
				dest_client, icomp_event, E_CAL_OBJ_MOD_ALL, E_CAL_OPERATION_FLAG_DISABLE_ITIP_MESSAGE, cancellable, error);

			g_clear_object (&icomp);
			if (!success)
				goto exit;

			if (!do_copy) {
				ECalObjModType mod_type = E_CAL_OBJ_MOD_THIS;

				/* Remove the item from the source calendar. */
				if (e_cal_util_component_is_instance (icomp_event) ||
				    e_cal_util_component_has_recurrences (icomp_event))
					mod_type = E_CAL_OBJ_MOD_ALL;

				success = e_cal_client_remove_object_sync (
						src_client, uid, NULL, mod_type, E_CAL_OPERATION_FLAG_DISABLE_ITIP_MESSAGE, cancellable, error);
				if (!success)
					goto exit;
			}

			continue;
		} else if (local_error != NULL && !g_error_matches (
					local_error, E_CAL_CLIENT_ERROR, E_CAL_CLIENT_ERROR_OBJECT_NOT_FOUND)) {
			g_propagate_error (error, local_error);
			goto exit;
		} else {
			g_clear_error (&local_error);
		}

		if (e_cal_util_component_is_instance (icomp_event)) {
			GSList *ecalcomps = NULL, *eiter;
			ECalComponent *comp;

			success = e_cal_client_get_objects_for_uid_sync (src_client, uid, &ecalcomps, cancellable, error);
			if (!success)
				goto exit;

			if (ecalcomps && !ecalcomps->next) {
				/* only one component, no need for a vCalendar list */
				comp = ecalcomps->data;
				icomp = i_cal_component_clone (e_cal_component_get_icalcomponent (comp));
			} else {
				icomp = i_cal_component_new (I_CAL_VCALENDAR_COMPONENT);
				for (eiter = ecalcomps; eiter; eiter = g_slist_next (eiter)) {
					comp = eiter->data;

					i_cal_component_take_component (
						icomp,
						i_cal_component_clone (e_cal_component_get_icalcomponent (comp)));
				}
			}

			e_util_free_nullable_object_slist (ecalcomps);
		} else {
			icomp = i_cal_component_clone (icomp_event);
		}

		if (do_copy) {
			/* Change the UID to avoid problems with duplicated UID */
			new_uid = e_util_generate_uid ();
			if (i_cal_component_isa (icomp) == I_CAL_VCALENDAR_COMPONENT) {
				/* in case of a vCalendar, the component might have detached instances,
				 * thus change the UID on all of the subcomponents of it */
				for (subcomp = i_cal_component_get_first_component (icomp, icomp_kind);
				     subcomp;
				     g_object_unref (subcomp), subcomp = i_cal_component_get_next_component (icomp, icomp_kind)) {
					i_cal_component_set_uid (subcomp, new_uid);
				}
			} else {
				i_cal_component_set_uid (icomp, new_uid);
			}
			g_free (new_uid);
			new_uid = NULL;
		}

		ftd.source_client = src_client;
		ftd.destination_client = dest_client;
		ftd.cancellable = cancellable;
		ftd.error = error;
		ftd.success = TRUE;

		if (i_cal_component_isa (icomp) == I_CAL_VCALENDAR_COMPONENT) {
			/* in case of a vCalendar, the component might have detached instances,
			 * thus check timezones on all of the subcomponents of it */
			for (subcomp = i_cal_component_get_first_component (icomp, icomp_kind);
			     subcomp && ftd.success;
			     g_object_unref (subcomp), subcomp = i_cal_component_get_next_component (icomp, icomp_kind)) {
				i_cal_component_foreach_tzid (subcomp, add_timezone_to_cal_cb, &ftd);
			}

			g_clear_object (&subcomp);
		} else {
			i_cal_component_foreach_tzid (icomp, add_timezone_to_cal_cb, &ftd);
		}

		if (!ftd.success) {
			success = FALSE;
			goto exit;
		}

		if (i_cal_component_isa (icomp) == I_CAL_VCALENDAR_COMPONENT) {
			gboolean did_add = FALSE;

			/* in case of a vCalendar, the component might have detached instances,
			 * thus add the master object first, and then all of the subcomponents of it */
			for (subcomp = i_cal_component_get_first_component (icomp, icomp_kind);
			     subcomp && !did_add;
			     g_object_unref (subcomp), subcomp = i_cal_component_get_next_component (icomp, icomp_kind)) {
				if (!e_cal_util_component_has_property (subcomp, I_CAL_RECURRENCEID_PROPERTY)) {
					did_add = TRUE;
					success = e_cal_client_create_object_sync (
						dest_client, subcomp, E_CAL_OPERATION_FLAG_DISABLE_ITIP_MESSAGE,
						&new_uid, cancellable, error);
					g_free (new_uid);
				}
			}

			g_clear_object (&subcomp);

			if (!success) {
				g_clear_object (&icomp);
				goto exit;
			}

			/* deal with detached instances */
			for (subcomp = i_cal_component_get_first_component (icomp, icomp_kind);
			     subcomp && success;
			     g_object_unref (subcomp), subcomp = i_cal_component_get_next_component (icomp, icomp_kind)) {
				if (e_cal_util_component_has_property (subcomp, I_CAL_RECURRENCEID_PROPERTY)) {
					if (did_add) {
						success = e_cal_client_modify_object_sync (
							dest_client, subcomp,
							E_CAL_OBJ_MOD_THIS, E_CAL_OPERATION_FLAG_DISABLE_ITIP_MESSAGE, cancellable, error);
					} else {
						/* just in case there are only detached instances and no master object */
						did_add = TRUE;
						success = e_cal_client_create_object_sync (
							dest_client, subcomp, E_CAL_OPERATION_FLAG_DISABLE_ITIP_MESSAGE,
							&new_uid, cancellable, error);
						g_free (new_uid);
					}
				}
			}

			g_clear_object (&subcomp);
		} else {
			success = e_cal_client_create_object_sync (dest_client, icomp, E_CAL_OPERATION_FLAG_DISABLE_ITIP_MESSAGE, &new_uid, cancellable, error);
			g_free (new_uid);
		}

		g_clear_object (&icomp);
		if (!success)
			goto exit;

		if (!do_copy) {
			ECalObjModType mod_type = E_CAL_OBJ_MOD_THIS;

			/* Remove the item from the source calendar. */
			if (e_cal_util_component_is_instance (icomp_event) ||
			    e_cal_util_component_has_recurrences (icomp_event))
				mod_type = E_CAL_OBJ_MOD_ALL;

			success = e_cal_client_remove_object_sync (src_client, uid, NULL, mod_type, E_CAL_OPERATION_FLAG_DISABLE_ITIP_MESSAGE, cancellable, error);
			if (!success)
				goto exit;
		}

		g_hash_table_insert (processed_uids, g_strdup (uid), GINT_TO_POINTER (1));
	}

 exit:
	g_hash_table_destroy (processed_uids);

	return success;
}

void
cal_comp_util_update_tzid_parameter (ICalProperty *prop,
				     const ICalTime *tt)
{
	ICalParameter *param;
	const gchar *tzid = NULL;

	g_return_if_fail (prop != NULL);

	if (!tt || !i_cal_time_is_valid_time ((ICalTime *) tt) ||
	    i_cal_time_is_null_time ((ICalTime *) tt))
		return;

	param = i_cal_property_get_first_parameter (prop, I_CAL_TZID_PARAMETER);
	if (i_cal_time_get_timezone ((ICalTime *) tt))
		tzid = i_cal_timezone_get_tzid (i_cal_time_get_timezone ((ICalTime *) tt));

	if (i_cal_time_get_timezone ((ICalTime *) tt) && tzid && *tzid &&
	    !i_cal_time_is_utc ((ICalTime *) tt) &&
	    !i_cal_time_is_date ((ICalTime *) tt)) {
		if (param) {
			i_cal_parameter_set_tzid (param, (gchar *) tzid);
			g_object_unref (param);
		} else {
			param = i_cal_parameter_new_tzid ((gchar *) tzid);
			i_cal_property_take_parameter (prop, param);
		}
	} else if (param) {
		i_cal_property_remove_parameter_by_kind (prop, I_CAL_TZID_PARAMETER);
		g_object_unref (param);
	}
}

/* Returns <0 for time before today, 0 for today, >0 for after today (future) */
gint
cal_comp_util_compare_time_with_today (const ICalTime *time_tt)
{
	ICalTime *now_tt, *tt = (ICalTime *) time_tt;
	gint res;

	if (!tt || i_cal_time_is_null_time (tt))
		return 0;

	if (i_cal_time_is_date (tt)) {
		time_t now;

		/* Compare with localtime, not with UTC */
		now = time (NULL);
		now_tt = e_cal_util_tm_to_icaltime (localtime (&now), TRUE);

		res = i_cal_time_compare_date_only (tt, now_tt);
	} else {
		now_tt = i_cal_time_new_current_with_zone (i_cal_time_get_timezone (tt));
		i_cal_time_set_timezone (now_tt, i_cal_time_get_timezone (tt));
		if (!i_cal_time_get_second (time_tt))
			i_cal_time_set_second (now_tt, 0);
		res = i_cal_time_compare (tt, now_tt);
	}

	g_clear_object (&now_tt);

	return res;
}

gboolean
cal_comp_util_have_in_new_attendees (const GSList *new_attendees_mails,
				     const gchar *eml)
{
	const GSList *link;

	if (!eml)
		return FALSE;

	for (link = new_attendees_mails; link; link = g_slist_next (link)) {
		if (link->data && g_ascii_strcasecmp (eml, link->data) == 0)
			return TRUE;
	}

	return FALSE;
}

static void
free_slist_strs (gpointer data)
{
	GSList *lst = data;

	if (lst) {
		g_slist_foreach (lst, (GFunc) g_free, NULL);
		g_slist_free (lst);
	}
}

/**
 * cal_comp_util_copy_new_attendees:
 * @des: Component, to copy to.
 * @src: Component, to copy from.
 *
 * Copies "new-attendees" information from @src to @des component.
 **/
void
cal_comp_util_copy_new_attendees (ECalComponent *des,
				  ECalComponent *src)
{
	GSList *copy = NULL, *l;

	g_return_if_fail (src != NULL);
	g_return_if_fail (des != NULL);

	for (l = g_object_get_data (G_OBJECT (src), "new-attendees"); l; l = l->next) {
		copy = g_slist_append (copy, g_strdup (l->data));
	}

	g_object_set_data_full (G_OBJECT (des), "new-attendees", copy, free_slist_strs);
}

/* Takes ownership of the 'emails' */
void
cal_comp_util_set_added_attendees_mails (ECalComponent *comp,
					 GSList *emails)
{
	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	g_object_set_data_full (G_OBJECT (comp), "new-attendees", emails, free_slist_strs);
}

gchar *
cal_comp_util_dup_parameter_xvalue (ICalProperty *prop,
				    const gchar *name)
{
	ICalParameter *param;

	if (!prop || !name || !*name)
		return NULL;

	for (param = i_cal_property_get_first_parameter (prop, I_CAL_X_PARAMETER);
	     param;
	     g_object_unref (param), param = i_cal_property_get_next_parameter (prop, I_CAL_X_PARAMETER)) {
		const gchar *xname = i_cal_parameter_get_xname (param);

		if (xname && g_ascii_strcasecmp (xname, name) == 0) {
			gchar *value;

			value = g_strdup (i_cal_parameter_get_xvalue (param));
			g_object_unref (param);

			return value;
		}
	}

	return NULL;
}

gchar *
cal_comp_util_get_attendee_comments (ICalComponent *icomp)
{
	GString *comments = NULL;
	ICalProperty *prop;

	g_return_val_if_fail (icomp != NULL, NULL);

	for (prop = i_cal_component_get_first_property (icomp, I_CAL_ATTENDEE_PROPERTY);
	     prop;
	     g_object_unref (prop), prop = i_cal_component_get_next_property (icomp, I_CAL_ATTENDEE_PROPERTY)) {
		gchar *guests_str = NULL;
		guint32 num_guests = 0;
		gchar *value;

		value = cal_comp_util_dup_parameter_xvalue (prop, "X-NUM-GUESTS");
		if (value && *value)
			num_guests = atoi (value);
		g_free (value);

		value = cal_comp_util_dup_parameter_xvalue (prop, "X-RESPONSE-COMMENT");

		if (num_guests)
			guests_str = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "with one guest", "with %d guests", num_guests), num_guests);

		if (guests_str || (value && *value)) {
			const gchar *email = i_cal_property_get_attendee (prop);
			const gchar *cn = NULL;
			ICalParameter *cnparam;

			cnparam = i_cal_property_get_first_parameter (prop, I_CAL_CN_PARAMETER);
			if (cnparam) {
				cn = i_cal_parameter_get_cn (cnparam);
				if (!cn || !*cn)
					cn = NULL;
			}

			email = itip_strip_mailto (email);

			if ((email && *email) || (cn && *cn)) {
				if (!comments)
					comments = g_string_new ("");
				else
					g_string_append (comments, "\n    ");

				if (cn && *cn) {
					g_string_append (comments, cn);

					if (g_strcmp0 (email, cn) == 0)
						email = NULL;
				}

				if (email && *email) {
					if (cn && *cn)
						g_string_append_printf (comments, " <%s>", email);
					else
						g_string_append (comments, email);
				}

				g_string_append (comments, ": ");

				if (guests_str) {
					g_string_append (comments, guests_str);

					if (value && *value)
						g_string_append (comments, "; ");
				}

				if (value && *value)
					g_string_append (comments, value);
			}

			g_clear_object (&cnparam);
		}

		g_free (guests_str);
		g_free (value);
	}

	if (comments) {
		gchar *str;

		str = g_strdup_printf (_("Comments: %s"), comments->str);
		g_string_free (comments, TRUE);

		return str;
	}

	return NULL;
}

static struct _status_values {
	ICalComponentKind kind;
	ICalPropertyStatus status;
	const gchar *text;
} status_values[] = {
	{ I_CAL_VEVENT_COMPONENT,   I_CAL_STATUS_NONE,        NC_("iCalendarStatus", "None") },
	{ I_CAL_VEVENT_COMPONENT,   I_CAL_STATUS_TENTATIVE,   NC_("iCalendarStatus", "Tentative") },
	{ I_CAL_VEVENT_COMPONENT,   I_CAL_STATUS_CONFIRMED,   NC_("iCalendarStatus", "Confirmed") },
	{ I_CAL_VJOURNAL_COMPONENT, I_CAL_STATUS_NONE,        NC_("iCalendarStatus", "None") },
	{ I_CAL_VJOURNAL_COMPONENT, I_CAL_STATUS_DRAFT,       NC_("iCalendarStatus", "Draft") },
	{ I_CAL_VJOURNAL_COMPONENT, I_CAL_STATUS_FINAL,       NC_("iCalendarStatus", "Final") },
	{ I_CAL_VTODO_COMPONENT,    I_CAL_STATUS_NONE,        NC_("iCalendarStatus", "Not Started") },
	{ I_CAL_VTODO_COMPONENT,    I_CAL_STATUS_NEEDSACTION, NC_("iCalendarStatus", "Needs Action") },
	{ I_CAL_VTODO_COMPONENT,    I_CAL_STATUS_INPROCESS,   NC_("iCalendarStatus", "In Progress") },
	{ I_CAL_VTODO_COMPONENT,    I_CAL_STATUS_COMPLETED,   NC_("iCalendarStatus", "Completed") },
	{ I_CAL_ANY_COMPONENT,      I_CAL_STATUS_CANCELLED,   NC_("iCalendarStatus", "Cancelled") }
};

/**
 * cal_comp_util_status_to_localized_string:
 * @kind: an #ICalComponentKind of a component the @status belongs to
 * @status: an #ICalPropertyStatus
 *
 * Returns localized text, suitable for user-visible strings,
 * corresponding to the @status value. The @kind is used to
 * distinguish how to localize certain values.
 *
 * To transform the returned string back to the enum value
 * use cal_comp_util_localized_string_to_status().
 *
 * Returns: (nullable): the @status as a localized string, or %NULL,
 *    when such @status could not be found for the given @kind
 *
 * Since: 3.34
 **/
const gchar *
cal_comp_util_status_to_localized_string (ICalComponentKind kind,
					  ICalPropertyStatus status)
{
	gint ii;

	for (ii = 0; ii < G_N_ELEMENTS (status_values); ii++) {
		if ((status_values[ii].kind == kind ||
		     status_values[ii].kind == I_CAL_ANY_COMPONENT ||
		     kind == I_CAL_ANY_COMPONENT) &&
		     status_values[ii].status == status)
			return g_dpgettext2 (GETTEXT_PACKAGE, "iCalendarStatus", status_values[ii].text);
	}

	return NULL;
}

/**
 * cal_comp_util_localized_string_to_status:
 * @kind: an #ICalComponentKind of a component the status belongs to
 * @localized_string: (nullable): localized text for the status, or %NULL
 * @cmp_func: (scope call) (closure user_data) (nullable): optional compare function, can be %NULL
 * @user_data: user data for the @cmp_func
 *
 * Converts @localized_string returned from cal_comp_util_status_to_localized_string()
 * back to an #ICalPropertyStatus enum. Returns %I_CAL_STATUS_NONE, when
 * the @localized_string cannot be found for the given @kind.
 *
 * Returns: an #ICalPropertyStatus corresponding to given @kind and @localized_string,
 *    or %I_CAL_STATUS_NONE, when the value cannot be found.
 *
 * Since: 3.34
 **/
ICalPropertyStatus
cal_comp_util_localized_string_to_status (ICalComponentKind kind,
					  const gchar *localized_string,
					  GCompareDataFunc cmp_func,
					  gpointer user_data)
{
	gint ii;

	if (!localized_string || !*localized_string)
		return I_CAL_STATUS_NONE;

	if (!cmp_func) {
		cmp_func = (GCompareDataFunc) e_util_utf8_strcasecmp;
		user_data = NULL;
	}

	for (ii = 0; ii < G_N_ELEMENTS (status_values); ii++) {
		if ((status_values[ii].kind == kind ||
		     status_values[ii].kind == I_CAL_ANY_COMPONENT ||
		     kind == I_CAL_ANY_COMPONENT) &&
		     cmp_func (localized_string, g_dpgettext2 (GETTEXT_PACKAGE, "iCalendarStatus", status_values[ii].text), user_data) == 0)
			return status_values[ii].status;
	}

	return I_CAL_STATUS_NONE;
}

/**
 * cal_comp_util_get_status_list_for_kind:
 * @kind: an #ICalComponentKind
 *
 * Returns: (element-type utf8) (transfer container): a #GList of localized strings
 *    corresponding to #ICalPropertyStatus usable for the @kind. The caller owns
 *    the returned #GList, but not the items of it. In other words, free the returned
 *    #GList with g_list_free(), when no longer needed.
 *
 * Since: 3.34
 **/
GList *
cal_comp_util_get_status_list_for_kind (ICalComponentKind kind)
{
	GList *items = NULL;
	gint ii;

	for (ii = 0; ii < G_N_ELEMENTS (status_values); ii++) {
		if ((status_values[ii].kind == kind ||
		     status_values[ii].kind == I_CAL_ANY_COMPONENT ||
		     kind == I_CAL_ANY_COMPONENT))
			items = g_list_prepend (items, (gpointer) g_dpgettext2 (GETTEXT_PACKAGE, "iCalendarStatus", status_values[ii].text));
	}

	return g_list_reverse (items);
}

/**
 * cal_comp_util_ensure_allday_timezone:
 * @itime: an #ICalTime to update
 * @zone: (nullable): an #ICalTimezone to set, or %NULL for UTC
 *
 * Changes DATE value of @itime to DATETIME using the @zone.
 * The @itime is not converted to the @zone, the @zone is
 * assigned to it.
 *
 * Returns: whether made any change
 *
 * Since: 3.38
 **/
gboolean
cal_comp_util_ensure_allday_timezone (ICalTime *itime,
				      ICalTimezone *zone)
{
	g_return_val_if_fail (I_CAL_IS_TIME (itime), FALSE);

	if (i_cal_time_is_date (itime)) {
		if (!zone)
			zone = i_cal_timezone_get_utc_timezone ();

		i_cal_time_set_is_date (itime, FALSE);
		i_cal_time_set_time (itime, 0, 0, 0);
		i_cal_time_set_timezone (itime, zone);

		return TRUE;
	}

	return FALSE;
}

static void
ensure_allday_timezone_property (ICalComponent *icomp,
				 ICalTimezone *zone,
				 ICalPropertyKind prop_kind,
				 ICalTime *(* get_func) (ICalComponent *icomp),
				 void (* set_func) (ICalComponent *icomp,
						    ICalTime *dtvalue))
{
	ICalProperty *prop;

	g_return_if_fail (I_CAL_IS_COMPONENT (icomp));
	g_return_if_fail (get_func != NULL);
	g_return_if_fail (set_func != NULL);

	prop = i_cal_component_get_first_property (icomp, prop_kind);

	if (prop) {
		ICalTime *dtvalue;

		dtvalue = get_func (icomp);

		if (dtvalue && cal_comp_util_ensure_allday_timezone (dtvalue, zone)) {
			/* Remove the VALUE parameter, to correspond to the actual value being set */
			i_cal_property_remove_parameter_by_kind (prop, I_CAL_VALUE_PARAMETER);
		}

		set_func (icomp, dtvalue);
		cal_comp_util_update_tzid_parameter (prop, dtvalue);

		g_clear_object (&dtvalue);
		g_clear_object (&prop);
	}
}

/**
 * cal_comp_util_maybe_ensure_allday_timezone_properties:
 * @client: (nullable): an #ECalClient, or NULL, to change it always
 * @icomp: an #ICalComponent to update
 * @zone: (nullable): an #ICalTimezone to eventually set, or %NULL for floating time
 *
 * When the @client is not specified, or when it has set %E_CAL_STATIC_CAPABILITY_ALL_DAY_EVENT_AS_TIME,
 * calls cal_comp_util_ensure_allday_timezone() for DTSTART
 * and DTEND properties, if such exist in the @icomp, and updates
 * those accordingly.
 *
 * Since: 3.38
 **/
void
cal_comp_util_maybe_ensure_allday_timezone_properties (ECalClient *client,
						       ICalComponent *icomp,
						       ICalTimezone *zone)
{
	if (client)
		g_return_if_fail (E_IS_CAL_CLIENT (client));

	g_return_if_fail (I_CAL_IS_COMPONENT (icomp));

	if (client && !e_client_check_capability (E_CLIENT (client), E_CAL_STATIC_CAPABILITY_ALL_DAY_EVENT_AS_TIME))
		return;

	ensure_allday_timezone_property (icomp, zone, I_CAL_DTSTART_PROPERTY, i_cal_component_get_dtstart, i_cal_component_set_dtstart);
	ensure_allday_timezone_property (icomp, zone, I_CAL_DTEND_PROPERTY, i_cal_component_get_dtend, i_cal_component_set_dtend);
}

void
cal_comp_util_format_itt (ICalTime *itt,
			  gchar *buffer,
			  gint buffer_size)
{
	struct tm tm;

	g_return_if_fail (itt != NULL);
	g_return_if_fail (buffer != NULL);
	g_return_if_fail (buffer_size > 0);

	buffer[0] = '\0';

	tm = e_cal_util_icaltime_to_tm (itt);
	e_datetime_format_format_tm_inline ("calendar", "table", i_cal_time_is_date (itt) ? DTFormatKindDate : DTFormatKindDateTime, &tm, buffer, buffer_size);
}

ICalTime *
cal_comp_util_date_time_to_zone (ECalComponentDateTime *dt,
				 ECalClient *client,
				 ICalTimezone *default_zone)
{
	ICalTime *itt;
	ICalTimezone *zone = NULL;
	const gchar *tzid;

	if (!dt)
		return NULL;

	itt = i_cal_time_clone (e_cal_component_datetime_get_value (dt));
	tzid = e_cal_component_datetime_get_tzid (dt);

	if (tzid && *tzid) {
		if (!e_cal_client_get_timezone_sync (client, tzid, &zone, NULL, NULL))
			zone = NULL;
	} else if (i_cal_time_is_utc (itt)) {
		zone = i_cal_timezone_get_utc_timezone ();
	}

	if (zone) {
		i_cal_time_convert_timezone (itt, zone, default_zone);
		i_cal_time_set_timezone (itt, default_zone);
	}

	return itt;
}

gchar *
cal_comp_util_dup_attendees_status_info (ECalComponent *comp,
					 ECalClient *cal_client,
					 ESourceRegistry *registry)
{
	struct _values {
		ICalParameterPartstat status;
		const gchar *caption;
		gint count;
	} values[] = {
		{ I_CAL_PARTSTAT_ACCEPTED,    N_("Accepted"),     0 },
		{ I_CAL_PARTSTAT_DECLINED,    N_("Declined"),     0 },
		{ I_CAL_PARTSTAT_TENTATIVE,   N_("Tentative"),    0 },
		{ I_CAL_PARTSTAT_DELEGATED,   N_("Delegated"),    0 },
		{ I_CAL_PARTSTAT_NEEDSACTION, N_("Needs action"), 0 },
		{ I_CAL_PARTSTAT_NONE,        N_("Other"),        0 },
		{ I_CAL_PARTSTAT_X,           NULL,              -1 }
	};
	GSList *attendees = NULL, *link;
	gboolean have = FALSE;
	gchar *res = NULL;
	gint ii;

	g_return_val_if_fail (E_IS_CAL_CLIENT (cal_client), NULL);

	if (registry) {
		g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);
		g_object_ref (registry);
	} else {
		GError *error = NULL;

		registry = e_source_registry_new_sync (NULL, &error);
		if (!registry)
			g_warning ("%s: Failed to create source registry: %s", G_STRFUNC, error ? error->message : "Unknown error");
		g_clear_error (&error);
	}

	if (!comp || !e_cal_component_has_attendees (comp) ||
	    !itip_organizer_is_user_ex (registry, comp, cal_client, TRUE)) {
		g_clear_object (&registry);
		return NULL;
	}

	attendees = e_cal_component_get_attendees (comp);

	for (link = attendees; link; link = g_slist_next (link)) {
		ECalComponentAttendee *att = link->data;

		if (att && e_cal_component_attendee_get_cutype (att) == I_CAL_CUTYPE_INDIVIDUAL &&
		    (e_cal_component_attendee_get_role (att) == I_CAL_ROLE_CHAIR ||
		     e_cal_component_attendee_get_role (att) == I_CAL_ROLE_REQPARTICIPANT ||
		     e_cal_component_attendee_get_role (att) == I_CAL_ROLE_OPTPARTICIPANT)) {
			have = TRUE;

			for (ii = 0; values[ii].count != -1; ii++) {
				if (e_cal_component_attendee_get_partstat (att) == values[ii].status || values[ii].status == I_CAL_PARTSTAT_NONE) {
					values[ii].count++;
					break;
				}
			}
		}
	}

	if (have) {
		GString *str = g_string_new ("");

		for (ii = 0; values[ii].count != -1; ii++) {
			if (values[ii].count > 0) {
				if (str->str && *str->str)
					g_string_append (str, "   ");

				g_string_append_printf (str, "%s: %d", _(values[ii].caption), values[ii].count);
			}
		}

		g_string_prepend (str, ": ");

		/* Translators: 'Status' here means the state of the attendees, the resulting string will be in a form:
		 * Status: Accepted: X   Declined: Y   ... */
		g_string_prepend (str, _("Status"));

		res = g_string_free (str, FALSE);
	}

	g_slist_free_full (attendees, e_cal_component_attendee_free);

	g_clear_object (&registry);

	return res;
}

gchar *
cal_comp_util_describe (ECalComponent *comp,
			ECalClient *client,
			ICalTimezone *default_zone,
			ECalCompUtilDescribeFlags flags)
{
	gboolean use_markup = (flags & E_CAL_COMP_UTIL_DESCRIBE_FLAG_USE_MARKUP) != 0;
	ECalComponentDateTime *dtstart = NULL, *dtend = NULL;
	ICalTime *itt_start, *itt_end;
	ICalComponent *icalcomp;
	gchar *summary;
	const gchar *location;
	gchar *timediff = NULL, *tmp;
	gchar timestr[255];
	GString *markup;

	g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), NULL);
	g_return_val_if_fail (E_IS_CAL_CLIENT (client), NULL);

	timestr[0] = 0;
	markup = g_string_sized_new (256);
	icalcomp = e_cal_component_get_icalcomponent (comp);
	summary = e_calendar_view_dup_component_summary (icalcomp);
	location = i_cal_component_get_location (icalcomp);

	if (location && !*location)
		location = NULL;

	if (e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_EVENT) {
		dtstart = e_cal_component_get_dtstart (comp);
		dtend = e_cal_component_get_dtend (comp);
	} else if (e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_TODO) {
		dtstart = e_cal_component_get_dtstart (comp);
	} else if (e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_JOURNAL) {
		dtstart = e_cal_component_get_dtstart (comp);
	}

	itt_start = cal_comp_util_date_time_to_zone (dtstart, client, default_zone);
	itt_end = cal_comp_util_date_time_to_zone (dtend, client, default_zone);

	if ((flags & E_CAL_COMP_UTIL_DESCRIBE_FLAG_ONLY_TIME) != 0 &&
	    (itt_start && (!itt_end || i_cal_time_compare_date_only (itt_start, itt_end) == 0))) {
		if ((flags & E_CAL_COMP_UTIL_DESCRIBE_FLAG_24HOUR_FORMAT) != 0) {
			g_snprintf (timestr, sizeof (timestr), "%d:%02d", i_cal_time_get_hour (itt_start), i_cal_time_get_minute (itt_start));
		} else {
			gint hour = i_cal_time_get_hour (itt_start);
			const gchar *suffix;

			if (hour < 12) {
				/* String to use in 12-hour time format for times in the morning. */
				suffix = _("am");
			} else {
				hour -= 12;
				/* String to use in 12-hour time format for times in the afternoon. */
				suffix = _("pm");
			}

			if (hour == 0)
				hour = 12;

			if (!i_cal_time_get_minute (itt_start))
				g_snprintf (timestr, sizeof (timestr), "%d %s", hour, suffix);
			else
				g_snprintf (timestr, sizeof (timestr), "%d:%02d %s", hour, i_cal_time_get_minute (itt_start), suffix);
		}
	} else if (itt_start) {
		cal_comp_util_format_itt (itt_start, timestr, sizeof (timestr) - 1);
	}

	if (itt_start && itt_end) {
		gint64 start, end;

		start = i_cal_time_as_timet (itt_start);
		end = i_cal_time_as_timet (itt_end);

		if (start < end)
			timediff = e_cal_util_seconds_to_string (end - start);
	}

	if (!summary || !*summary)
		g_clear_pointer (&summary, g_free);

	if (use_markup) {
		tmp = g_markup_printf_escaped ("<b>%s</b>", summary ? summary : _( "No Summary"));
		g_string_append (markup, tmp);
		g_free (tmp);
	} else {
		g_string_append (markup, summary ? summary : _( "No Summary"));
	}

	if (*timestr) {
		GSList *parts = NULL, *link;
		const gchar *use_timestr = timestr;
		const gchar *use_timediff = timediff;
		const gchar *use_location = location;
		gchar *escaped_timestr = NULL;
		gchar *escaped_timediff = NULL;
		gchar *escaped_location = NULL;

		g_string_append_c (markup, '\n');

		if (use_markup) {
			escaped_timestr = g_markup_escape_text (timestr, -1);
			use_timestr = escaped_timestr;

			if (timediff && *timediff) {
				escaped_timediff = g_markup_escape_text (timediff, -1);
				use_timediff = escaped_timediff;
			}

			if (location) {
				escaped_location = g_markup_escape_text (location, -1);
				use_location = escaped_location;
			}
		}

		if (timediff && *timediff) {
			if (use_location) {
				parts = g_slist_prepend (parts, (gpointer) use_timestr);
				parts = g_slist_prepend (parts, (gpointer) " (");
				parts = g_slist_prepend (parts, (gpointer) use_timediff);
				parts = g_slist_prepend (parts, (gpointer) ") ");
				parts = g_slist_prepend (parts, (gpointer) use_location);
			} else {
				parts = g_slist_prepend (parts, (gpointer) use_timestr);
				parts = g_slist_prepend (parts, (gpointer) " (");
				parts = g_slist_prepend (parts, (gpointer) use_timediff);
				parts = g_slist_prepend (parts, (gpointer) ")");
			}
		} else if (use_location) {
				parts = g_slist_prepend (parts, (gpointer) use_timestr);
				parts = g_slist_prepend (parts, (gpointer) " ");
				parts = g_slist_prepend (parts, (gpointer) use_location);
		} else {
			parts = g_slist_prepend (parts, (gpointer) use_timestr);
		}

		if (!(flags & E_CAL_COMP_UTIL_DESCRIBE_FLAG_RTL))
			parts = g_slist_reverse (parts);

		if (use_markup)
			g_string_append (markup, "<small>");
		for (link = parts; link; link = g_slist_next (link)) {
			g_string_append (markup, (const gchar *) link->data);
		}
		if (use_markup)
			g_string_append (markup, "</small>");

		g_slist_free (parts);
		g_free (escaped_timestr);
		g_free (escaped_timediff);
		g_free (escaped_location);
	} else if (location) {
		g_string_append_c (markup, '\n');

		if (use_markup) {
			tmp = g_markup_printf_escaped ("%s", location);

			g_string_append (markup, "<small>");
			g_string_append (markup, tmp);
			g_string_append (markup, "</small>");

			g_free (tmp);
		} else {
			g_string_append (markup, location);
		}
	}

	g_free (timediff);
	g_free (summary);

	e_cal_component_datetime_free (dtstart);
	e_cal_component_datetime_free (dtend);
	g_clear_object (&itt_start);
	g_clear_object (&itt_end);

	return g_string_free (markup, FALSE);
}

gchar *
cal_comp_util_dup_tooltip (ECalComponent *comp,
			   ECalClient *client,
			   ESourceRegistry *registry,
			   ICalTimezone *default_zone)
{
	ECalComponentOrganizer *organizer;
	ECalComponentDateTime *dtstart, *dtend;
	ICalComponent *icalcomp;
	ICalTimezone *zone;
	GString *tooltip;
	const gchar *description;
	gchar *tmp;

	g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), NULL);
	g_return_val_if_fail (E_IS_CAL_CLIENT (client), NULL);

	icalcomp = e_cal_component_get_icalcomponent (comp);
	tooltip = g_string_sized_new (256);

	tmp = e_calendar_view_dup_component_summary (icalcomp);
	e_util_markup_append_escaped (tooltip, "<b>%s</b>", tmp && *tmp ? tmp : _("No Summary"));
	g_clear_pointer (&tmp, g_free);

	organizer = e_cal_component_get_organizer (comp);
	if (organizer && e_cal_component_organizer_get_cn (organizer)) {
		const gchar *email;

		email = itip_strip_mailto (e_cal_component_organizer_get_value (organizer));

		if (email) {
			/* Translators: It will display "Organizer: NameOfTheUser <email@ofuser.com>" */
			tmp = g_strdup_printf (_("Organizer: %s <%s>"), e_cal_component_organizer_get_cn (organizer), email);
		} else {
			/* Translators: It will display "Organizer: NameOfTheUser" */
			tmp = g_strdup_printf (_("Organizer: %s"), e_cal_component_organizer_get_cn (organizer));
		}

		g_string_append_c (tooltip, '\n');
		e_util_markup_append_escaped_text (tooltip, tmp);
		g_clear_pointer (&tmp, g_free);
	}

	e_cal_component_organizer_free (organizer);

	tmp = e_cal_component_get_location (comp);

	if (tmp && *tmp) {
		g_string_append_c (tooltip, '\n');
		/* Translators: It will display "Location: PlaceOfTheMeeting" */
		e_util_markup_append_escaped (tooltip, _("Location: %s"), tmp);
	}

	g_clear_pointer (&tmp, g_free);

	dtstart = e_cal_component_get_dtstart (comp);
	dtend = e_cal_component_get_dtend (comp);

	if (dtstart && e_cal_component_datetime_get_tzid (dtstart)) {
		zone = i_cal_component_get_timezone (icalcomp, e_cal_component_datetime_get_tzid (dtstart));
		if (!zone &&
		    !e_cal_client_get_timezone_sync (client, e_cal_component_datetime_get_tzid (dtstart), &zone, NULL, NULL))
			zone = NULL;

		if (!zone)
			zone = default_zone;

	} else {
		zone = default_zone;
	}

	if (dtstart && e_cal_component_datetime_get_value (dtstart)) {
		struct tm tmp_tm;
		time_t t_start, t_end;
		gchar *tmp1;

		t_start = i_cal_time_as_timet_with_zone (e_cal_component_datetime_get_value (dtstart), zone);

		if (dtend && e_cal_component_datetime_get_value (dtend)) {
			ICalTimezone *end_zone = default_zone;

			if (e_cal_component_datetime_get_tzid (dtend)) {
				end_zone = i_cal_component_get_timezone (e_cal_component_get_icalcomponent (comp), e_cal_component_datetime_get_tzid (dtend));
				if (!end_zone &&
				    !e_cal_client_get_timezone_sync (client, e_cal_component_datetime_get_tzid (dtend), &end_zone, NULL, NULL))
					end_zone = NULL;

				if (!end_zone)
					end_zone = default_zone;
			}

			t_end = i_cal_time_as_timet_with_zone (e_cal_component_datetime_get_value (dtend), end_zone);
		} else {
			t_end = t_start;
		}

		tmp_tm = e_cal_util_icaltime_to_tm_with_zone (e_cal_component_datetime_get_value (dtstart), zone, default_zone);
		tmp1 = e_datetime_format_format_tm ("calendar", "table", i_cal_time_is_date (e_cal_component_datetime_get_value (dtstart)) ?
			DTFormatKindDate : DTFormatKindDateTime, &tmp_tm);

		g_string_append_c (tooltip, '\n');

		if (t_end > t_start) {
			tmp = e_cal_util_seconds_to_string (t_end - t_start);
			/* Translators: It will display "Start: ActualStartDateAndTime (DurationOfTheMeeting)" */
			e_util_markup_append_escaped (tooltip, _("Start: %s (%s)"), tmp1, tmp);
			g_clear_pointer (&tmp, g_free);
		} else {
			/* Translators: It will display "Start: ActualStartDateAndTime" */
			e_util_markup_append_escaped (tooltip, _("Start: %s"), tmp1);
		}

		g_clear_pointer (&tmp1, g_free);

		if (zone && !cal_comp_util_compare_event_timezones (comp, client, default_zone)) {
			tmp_tm = e_cal_util_icaltime_to_tm_with_zone (e_cal_component_datetime_get_value (dtstart), zone, zone);
			tmp1 = e_datetime_format_format_tm ("calendar", "table", i_cal_time_is_date (e_cal_component_datetime_get_value (dtstart)) ?
				DTFormatKindDate : DTFormatKindDateTime, &tmp_tm);
			e_util_markup_append_escaped (tooltip, "\n\t[ %s %s ]", tmp1, i_cal_timezone_get_display_name (zone));
			g_clear_pointer (&tmp1, g_free);
		}
	}

	e_cal_component_datetime_free (dtstart);
	e_cal_component_datetime_free (dtend);

	if (e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_TODO) {
		ECalComponentDateTime *due;
		ICalTime *completed;

		due = e_cal_component_get_due (comp);

		if (due) {
			ICalTime *itt;

			itt = cal_comp_util_date_time_to_zone (due, client, default_zone);
			if (itt) {
				gchar timestr[255] = { 0, };

				cal_comp_util_format_itt (itt, timestr, sizeof (timestr) - 1);

				if (*timestr) {
					g_string_append_c (tooltip, '\n');
					/* Translators: It's for a task due date, it will display "Due: DateAndTime" */
					e_util_markup_append_escaped (tooltip, _("Due: %s"), timestr);
				}

				g_clear_object (&itt);
			}
		}

		e_cal_component_datetime_free (due);

		completed = e_cal_component_get_completed (comp);

		if (completed) {
			gchar timestr[255] = { 0, };

			if (i_cal_time_is_utc (completed)) {
				zone = i_cal_timezone_get_utc_timezone ();
				i_cal_time_convert_timezone (completed, i_cal_timezone_get_utc_timezone (), default_zone);
				i_cal_time_set_timezone (completed, default_zone);
			}

			cal_comp_util_format_itt (completed, timestr, sizeof (timestr) - 1);

			if (*timestr) {
				g_string_append_c (tooltip, '\n');
				/* Translators: It's for a task completed date, it will display "Completed: DateAndTime" */
				e_util_markup_append_escaped (tooltip, _("Completed: %s"), timestr);
			}

			g_clear_object (&completed);
		}
	}

	tmp = cal_comp_util_dup_attendees_status_info (comp, client, registry);
	if (tmp) {
		g_string_append_c (tooltip, '\n');
		e_util_markup_append_escaped_text (tooltip, tmp);
		g_clear_pointer (&tmp, g_free);
	}

	tmp = cal_comp_util_get_attendee_comments (icalcomp);
	if (tmp) {
		g_string_append_c (tooltip, '\n');
		e_util_markup_append_escaped_text (tooltip, tmp);
		g_clear_pointer (&tmp, g_free);
	}

	description = i_cal_component_get_description (icalcomp);
	if (description && *description && g_utf8_validate (description, -1, NULL) &&
	    !g_str_equal (description, "\r") &&
	    !g_str_equal (description, "\n") &&
	    !g_str_equal (description, "\r\n")) {
		#define MAX_TOOLTIP_DESCRIPTION_LEN 1024
		glong len;

		len = g_utf8_strlen (description, -1);
		if (len > MAX_TOOLTIP_DESCRIPTION_LEN) {
			GString *str;
			const gchar *end;

			end = g_utf8_offset_to_pointer (description, MAX_TOOLTIP_DESCRIPTION_LEN);
			str = g_string_new_len (description, end - description);
			g_string_append (str, _("…"));

			tmp = g_string_free (str, FALSE);
		}

		g_string_append_c (tooltip, '\n');
		g_string_append_c (tooltip, '\n');
		e_util_markup_append_escaped_text (tooltip, tmp ? tmp : description);
		g_clear_pointer (&tmp, g_free);
	}

	return g_string_free (tooltip, FALSE);
}
