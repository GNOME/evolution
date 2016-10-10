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

#include <string.h>
#include <time.h>

#include "calendar-config.h"
#include "comp-util.h"
#include "e-calendar-view.h"

#include "shell/e-shell-window.h"
#include "shell/e-shell-view.h"

/**
 * cal_comp_util_add_exdate:
 * @comp: A calendar component object.
 * @itt: Time for the exception.
 *
 * Adds an exception date to the current list of EXDATE properties in a calendar
 * component object.
 **/
void
cal_comp_util_add_exdate (ECalComponent *comp,
                          time_t t,
                          icaltimezone *zone)
{
	GSList *list;
	ECalComponentDateTime *cdt;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	e_cal_component_get_exdate_list (comp, &list);

	cdt = g_new (ECalComponentDateTime, 1);
	cdt->value = g_new (struct icaltimetype, 1);
	*cdt->value = icaltime_from_timet_with_zone (t, FALSE, zone);
	cdt->tzid = g_strdup (icaltimezone_get_tzid (zone));

	list = g_slist_append (list, cdt);
	e_cal_component_set_exdate_list (comp, list);
	e_cal_component_free_exdate_list (list);
}

/* Returns TRUE if the TZIDs are equivalent, i.e. both NULL or the same. */
static gboolean
e_cal_component_compare_tzid (const gchar *tzid1,
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
 * @client: A #ECalClient.
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
                                       icaltimezone *zone)
{
	ECalComponentDateTime start_datetime, end_datetime;
	const gchar *tzid;
	gboolean retval = FALSE;
	icaltimezone *start_zone = NULL;
	icaltimezone *end_zone = NULL;
	gint offset1, offset2;

	tzid = icaltimezone_get_tzid (zone);

	e_cal_component_get_dtstart (comp, &start_datetime);
	e_cal_component_get_dtend (comp, &end_datetime);

	/* If either the DTSTART or the DTEND is a DATE value, we return TRUE.
	 * Maybe if one was a DATE-TIME we should check that, but that should
	 * not happen often. */
	if ((start_datetime.value && start_datetime.value->is_date)
	    || (end_datetime.value && end_datetime.value->is_date)) {
		retval = TRUE;
		goto out;
	}

	/* If the event uses UTC for DTSTART & DTEND, return TRUE. Outlook
	 * will send single events as UTC, so we don't want to mark all of
	 * these. */
	if ((!start_datetime.value || start_datetime.value->is_utc)
	    && (!end_datetime.value || end_datetime.value->is_utc)) {
		retval = TRUE;
		goto out;
	}

	/* If the event uses floating time for DTSTART & DTEND, return TRUE.
	 * Imported vCalendar files will use floating times, so we don't want
	 * to mark all of these. */
	if (!start_datetime.tzid && !end_datetime.tzid) {
		retval = TRUE;
		goto out;
	}

	/* FIXME: DURATION may be used instead. */
	if (e_cal_component_compare_tzid (tzid, start_datetime.tzid)
	    && e_cal_component_compare_tzid (tzid, end_datetime.tzid)) {
		/* If both TZIDs are the same as the given zone's TZID, then
		 * we know the timezones are the same so we return TRUE. */
		retval = TRUE;
	} else {
		/* If the TZIDs differ, we have to compare the UTC offsets
		 * of the start and end times, using their own timezones and
		 * the given timezone. */
		if (start_datetime.tzid)
			e_cal_client_get_timezone_sync (client, start_datetime.tzid, &start_zone, NULL, NULL);
		else
			start_zone = NULL;

		if (start_zone == NULL)
			goto out;

		if (start_datetime.value) {
			offset1 = icaltimezone_get_utc_offset (
				start_zone,
				start_datetime.value,
				NULL);
			offset2 = icaltimezone_get_utc_offset (
				zone,
				start_datetime.value,
				NULL);
			if (offset1 != offset2)
				goto out;
		}

		if (end_datetime.tzid)
			e_cal_client_get_timezone_sync (client, end_datetime.tzid, &end_zone, NULL, NULL);
		else
			end_zone = NULL;

		if (end_zone == NULL)
			goto out;

		if (end_datetime.value) {
			offset1 = icaltimezone_get_utc_offset (
				end_zone,
				end_datetime.value,
				NULL);
			offset2 = icaltimezone_get_utc_offset (
				zone,
				end_datetime.value,
				NULL);
			if (offset1 != offset2)
				goto out;
		}

		retval = TRUE;
	}

 out:

	e_cal_component_free_datetime (&start_datetime);
	e_cal_component_free_datetime (&end_datetime);

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
	icalcomponent *icalcomp = NULL;
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
	e_cal_component_get_uid (comp, &uid);

	/* TODO We should not be checking for this here. But since
	 *      e_cal_util_construct_instance does not create the instances
	 *      of all day events, so we default to old behaviour. */
	if (e_cal_client_check_recurrences_no_master (client)) {
		rid = e_cal_component_get_recurid_as_string (comp);
	}

	if (e_cal_client_get_object_sync (client, uid, rid, &icalcomp, cancellable, &local_error) &&
	    icalcomp != NULL) {
		icalcomponent_free (icalcomp);
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
 * icalcomponent, not the ECalComponent.
 **/
gboolean
cal_comp_is_icalcomp_on_server_sync (icalcomponent *icalcomp,
				     ECalClient *client,
				     GCancellable *cancellable,
				     GError **error)
{
	gboolean on_server;
	ECalComponent *comp;

	if (!icalcomp || !client || !icalcomponent_get_uid (icalcomp))
		return FALSE;

	comp = e_cal_component_new_from_icalcomponent (icalcomponent_new_clone (icalcomp));
	if (!comp)
		return FALSE;

	on_server = cal_comp_is_on_server_sync (comp, client, cancellable, error);

	g_object_unref (comp);

	return on_server;
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
	icalcomponent *icalcomp = NULL;
	ECalComponent *comp;
	ECalComponentAlarm *alarm;
	icalproperty *icalprop;
	ECalComponentAlarmTrigger trigger;

	if (client && !e_cal_client_get_default_object_sync (client, &icalcomp, cancellable, error))
		return NULL;

	if (icalcomp == NULL)
		icalcomp = icalcomponent_new (ICAL_VEVENT_COMPONENT);

	comp = e_cal_component_new ();
	if (!e_cal_component_set_icalcomponent (comp, icalcomp)) {
		icalcomponent_free (icalcomp);

		e_cal_component_set_new_vtype (comp, E_CAL_COMPONENT_EVENT);
	}

	if (all_day || !use_default_reminder)
		return comp;

	alarm = e_cal_component_alarm_new ();

	/* We don't set the description of the alarm; we'll copy it from the
	 * summary when it gets committed to the server. For that, we add a
	 * X-EVOLUTION-NEEDS-DESCRIPTION property to the alarm's component.
	 */
	icalcomp = e_cal_component_alarm_get_icalcomponent (alarm);
	icalprop = icalproperty_new_x ("1");
	icalproperty_set_x_name (icalprop, "X-EVOLUTION-NEEDS-DESCRIPTION");
	icalcomponent_add_property (icalcomp, icalprop);

	e_cal_component_alarm_set_action (alarm, E_CAL_COMPONENT_ALARM_DISPLAY);

	trigger.type = E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START;

	memset (&trigger.u.rel_duration, 0, sizeof (trigger.u.rel_duration));

	trigger.u.rel_duration.is_neg = TRUE;

	switch (default_reminder_units) {
	case E_DURATION_MINUTES:
		trigger.u.rel_duration.minutes = default_reminder_interval;
		break;

	case E_DURATION_HOURS:
		trigger.u.rel_duration.hours = default_reminder_interval;
		break;

	case E_DURATION_DAYS:
		trigger.u.rel_duration.days = default_reminder_interval;
		break;

	default:
		g_warning ("wrong units %d\n", default_reminder_units);
	}

	e_cal_component_alarm_set_trigger (alarm, trigger);

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
	struct icaltimetype itt;
	ECalComponentDateTime dt;
	icaltimezone *zone;

	comp = cal_comp_event_new_with_defaults_sync (
		client, all_day, use_default_reminder,
		default_reminder_interval, default_reminder_units,
		cancellable, error);
	if (!comp)
		return NULL;

	zone = calendar_config_get_icaltimezone ();

	if (all_day) {
		itt = icaltime_from_timet_with_zone (time (NULL), 1, zone);

		dt.value = &itt;
		dt.tzid = icaltimezone_get_tzid (zone);

		e_cal_component_set_dtstart (comp, &dt);
		e_cal_component_set_dtend (comp, &dt);
	} else {
		itt = icaltime_current_time_with_zone (zone);
		icaltime_adjust (&itt, 0, 1, -itt.minute, -itt.second);

		dt.value = &itt;
		dt.tzid = icaltimezone_get_tzid (zone);

		e_cal_component_set_dtstart (comp, &dt);
		icaltime_adjust (&itt, 0, 1, 0, 0);
		e_cal_component_set_dtend (comp, &dt);
	}

	return comp;
}

ECalComponent *
cal_comp_task_new_with_defaults_sync (ECalClient *client,
				      GCancellable *cancellable,
				      GError **error)
{
	ECalComponent *comp;
	icalcomponent *icalcomp = NULL;

	if (client && !e_cal_client_get_default_object_sync (client, &icalcomp, cancellable, error))
		return NULL;

	if (icalcomp == NULL)
		icalcomp = icalcomponent_new (ICAL_VTODO_COMPONENT);

	comp = e_cal_component_new ();
	if (!e_cal_component_set_icalcomponent (comp, icalcomp)) {
		icalcomponent_free (icalcomp);

		e_cal_component_set_new_vtype (comp, E_CAL_COMPONENT_TODO);
	}

	return comp;
}

ECalComponent *
cal_comp_memo_new_with_defaults_sync (ECalClient *client,
				      GCancellable *cancellable,
				      GError **error)
{
	ECalComponent *comp;
	icalcomponent *icalcomp = NULL;

	if (client && !e_cal_client_get_default_object_sync (client, &icalcomp, cancellable, error))
		return NULL;

	if (icalcomp == NULL)
		icalcomp = icalcomponent_new (ICAL_VJOURNAL_COMPONENT);

	comp = e_cal_component_new ();
	if (!e_cal_component_set_icalcomponent (comp, icalcomp)) {
		icalcomponent_free (icalcomp);

		e_cal_component_set_new_vtype (comp, E_CAL_COMPONENT_JOURNAL);
	}

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
			icaltimezone *zone;
			struct icaltimetype itt;
			icalcomponent *icalcomp;
			icalproperty *prop;

			shell_view = e_shell_window_peek_shell_view (shell_window, "calendar");
			g_return_if_fail (shell_view != NULL);

			cal_view = NULL;
			shell_content = e_shell_view_get_shell_content (shell_view);
			g_object_get (shell_content, "current-view", &cal_view, NULL);
			g_return_if_fail (cal_view != NULL);
			g_return_if_fail (e_calendar_view_get_visible_time_range (cal_view, &start, &end));

			zone = e_cal_model_get_timezone (e_calendar_view_get_model (cal_view));
			itt = icaltime_from_timet_with_zone (start, FALSE, zone);

			icalcomp = e_cal_component_get_icalcomponent (comp);
			prop = icalcomponent_get_first_property (icalcomp, ICAL_DTSTART_PROPERTY);
			if (prop) {
				icalproperty_set_dtstart (prop, itt);
			} else {
				prop = icalproperty_new_dtstart (itt);
				icalcomponent_add_property (icalcomp, prop);
			}

			e_cal_component_rescan (comp);

			g_clear_object (&cal_view);
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

	e_cal_component_get_categories_list (comp, &categories_list);
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
	e_cal_component_free_categories_list (categories_list);

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
	icaltimezone *from, *to;

	g_return_if_fail (date != NULL);

	if (date->tzid == NULL || tzid == NULL ||
	    date->tzid == tzid || g_str_equal (date->tzid, tzid))
		return;

	from = icaltimezone_get_builtin_timezone_from_tzid (date->tzid);
	if (!from) {
		GError *error = NULL;

		e_cal_client_get_timezone_sync (
			client, date->tzid, &from, NULL, &error);

		if (error != NULL) {
			g_warning (
				"%s: Could not get timezone '%s' from server: %s",
				G_STRFUNC, date->tzid ? date->tzid : "",
				error->message);
			g_error_free (error);
		}
	}

	to = icaltimezone_get_builtin_timezone_from_tzid (tzid);
	if (!to) {
		/* do not check failure here, maybe the zone is not available there */
		e_cal_client_get_timezone_sync (client, tzid, &to, NULL, NULL);
	}

	icaltimezone_convert_time (date->value, from, to);
	date->tzid = tzid;
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
	ECalComponentDateTime olddate, date;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (pdate != NULL);

	e_cal_component_get_dtstart (comp, &olddate);

	date = *pdate;

	datetime_to_zone (client, &date, olddate.tzid);
	e_cal_component_set_dtstart (comp, &date);

	e_cal_component_free_datetime (&olddate);
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
	ECalComponentDateTime olddate, date;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (pdate != NULL);

	e_cal_component_get_dtend (comp, &olddate);

	date = *pdate;

	datetime_to_zone (client, &date, olddate.tzid);
	e_cal_component_set_dtend (comp, &date);

	e_cal_component_free_datetime (&olddate);
}

gboolean
comp_util_sanitize_recurrence_master_sync (ECalComponent *comp,
					   ECalClient *client,
					   GCancellable *cancellable,
					   GError **error)
{
	ECalComponent *master = NULL;
	icalcomponent *icalcomp = NULL;
	ECalComponentRange rid;
	ECalComponentDateTime sdt;
	const gchar *uid;

	/* Get the master component */
	e_cal_component_get_uid (comp, &uid);

	if (!e_cal_client_get_object_sync (client, uid, NULL, &icalcomp, cancellable, error))
		return FALSE;

	master = e_cal_component_new_from_icalcomponent (icalcomp);
	if (!master) {
		g_warn_if_reached ();
		return FALSE;
	}

	/* Compare recur id and start date */
	e_cal_component_get_recurid (comp, &rid);
	e_cal_component_get_dtstart (comp, &sdt);

	if (rid.datetime.value && sdt.value &&
	    icaltime_compare_date_only (
	    *rid.datetime.value, *sdt.value) == 0) {
		ECalComponentDateTime msdt, medt, edt;
		gint *sequence;

		e_cal_component_get_dtstart (master, &msdt);
		e_cal_component_get_dtend (master, &medt);

		e_cal_component_get_dtend (comp, &edt);

		if (!msdt.value || !medt.value || !edt.value) {
			g_warn_if_reached ();
			e_cal_component_free_datetime (&msdt);
			e_cal_component_free_datetime (&medt);
			e_cal_component_free_datetime (&edt);
			e_cal_component_free_datetime (&sdt);
			e_cal_component_free_range (&rid);
			g_object_unref (master);
			return FALSE;
		}

		sdt.value->year = msdt.value->year;
		sdt.value->month = msdt.value->month;
		sdt.value->day = msdt.value->day;

		edt.value->year = medt.value->year;
		edt.value->month = medt.value->month;
		edt.value->day = medt.value->day;

		e_cal_component_set_dtstart (comp, &sdt);
		e_cal_component_set_dtend (comp, &edt);

		e_cal_component_get_sequence (master, &sequence);
		e_cal_component_set_sequence (comp, sequence);

		e_cal_component_free_datetime (&msdt);
		e_cal_component_free_datetime (&medt);
		e_cal_component_free_datetime (&edt);
	}

	e_cal_component_free_datetime (&sdt);
	e_cal_component_free_range (&rid);
	e_cal_component_set_recurid (comp, NULL);

	g_object_unref (master);

	return TRUE;
}

gchar *
icalcomp_suggest_filename (icalcomponent *icalcomp,
                           const gchar *default_name)
{
	icalproperty *prop;
	const gchar *summary = NULL;

	if (!icalcomp)
		return g_strconcat (default_name, ".ics", NULL);

	prop = icalcomponent_get_first_property (icalcomp, ICAL_SUMMARY_PROPERTY);
	if (prop)
		summary = icalproperty_get_summary (prop);

	if (!summary || !*summary)
		summary = default_name;

	return g_strconcat (summary, ".ics", NULL);
}

void
cal_comp_get_instance_times (ECalClient *client,
			     icalcomponent *icalcomp,
			     const icaltimezone *default_zone,
			     time_t *instance_start,
			     gboolean *start_is_date,
			     time_t *instance_end,
			     gboolean *end_is_date,
			     GCancellable *cancellable)
{
	struct icaltimetype start_time, end_time;
	const icaltimezone *zone = default_zone;

	g_return_if_fail (E_IS_CAL_CLIENT (client));
	g_return_if_fail (icalcomp != NULL);
	g_return_if_fail (instance_start != NULL);
	g_return_if_fail (instance_end != NULL);

	start_time = icalcomponent_get_dtstart (icalcomp);
	end_time = icalcomponent_get_dtend (icalcomp);

	/* Some event can have missing DTEND, then use the start_time for them */
	if (icaltime_is_null_time (end_time))
		end_time = start_time;

	if (start_time.zone) {
		zone = start_time.zone;
	} else {
		icalparameter *param = NULL;
		icalproperty *prop = icalcomponent_get_first_property (icalcomp, ICAL_DTSTART_PROPERTY);

		if (prop) {
			param = icalproperty_get_first_parameter (prop, ICAL_TZID_PARAMETER);

			if (param) {
				const gchar *tzid = NULL;
				icaltimezone *st_zone = NULL;

				tzid = icalparameter_get_tzid (param);
				if (tzid)
					e_cal_client_get_timezone_sync (client, tzid, &st_zone, cancellable, NULL);

				if (st_zone)
					zone = st_zone;
			}
		}
	}

	*instance_start = icaltime_as_timet_with_zone (start_time, zone);
	if (start_is_date)
		*start_is_date = start_time.is_date;

	if (end_time.zone) {
		zone = end_time.zone;
	} else {
		icalparameter *param = NULL;
		icalproperty *prop = icalcomponent_get_first_property (icalcomp, ICAL_DTSTART_PROPERTY);

		if (prop) {
			param = icalproperty_get_first_parameter (prop, ICAL_TZID_PARAMETER);

			if (param) {
				const gchar *tzid = NULL;
				icaltimezone *end_zone = NULL;

				tzid = icalparameter_get_tzid (param);
				if (tzid)
					e_cal_client_get_timezone_sync (client, tzid, &end_zone, cancellable, NULL);

				if (end_zone)
					zone = end_zone;
			}
		}
	}

	*instance_end = icaltime_as_timet_with_zone (end_time, zone);
	if (end_is_date)
		*end_is_date = end_time.is_date;
}

time_t
cal_comp_gdate_to_timet (const GDate *date,
			 const icaltimezone *with_zone)
{
	struct tm tm;
	struct icaltimetype tt;

	g_return_val_if_fail (date != NULL, (time_t) -1);
	g_return_val_if_fail (g_date_valid (date), (time_t) -1);

	g_date_to_struct_tm (date, &tm);

	tt = tm_to_icaltimetype (&tm, TRUE);
	if (with_zone)
		return icaltime_as_timet_with_zone (tt, with_zone);

	return icaltime_as_timet (tt);
}

typedef struct _AsyncContext {
	ECalClient *src_client;
	icalcomponent *icalcomp_clone;
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
	if (async_context->src_client)
		g_object_unref (async_context->src_client);

	if (async_context->icalcomp_clone)
		icalcomponent_free (async_context->icalcomp_clone);

	g_slice_free (AsyncContext, async_context);
}

static void
add_timezone_to_cal_cb (icalparameter *param,
                        gpointer data)
{
	struct ForeachTzidData *ftd = data;
	icaltimezone *tz = NULL;
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

	tzid = icalparameter_get_tzid (param);
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
		async_context->icalcomp_clone,
		async_context->do_copy,
		cancellable, &local_error);

	if (local_error != NULL)
		g_simple_async_result_take_error (simple, local_error);
}

void
cal_comp_transfer_item_to (ECalClient *src_client,
                           ECalClient *dest_client,
                           icalcomponent *icalcomp_vcal,
                           gboolean do_copy,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
	GSimpleAsyncResult *simple;
	AsyncContext *async_context;

	g_return_if_fail (E_IS_CAL_CLIENT (src_client));
	g_return_if_fail (E_IS_CAL_CLIENT (dest_client));
	g_return_if_fail (icalcomp_vcal != NULL);

	async_context = g_slice_new0 (AsyncContext);
	async_context->src_client = g_object_ref (src_client);
	async_context->icalcomp_clone = icalcomponent_new_clone (icalcomp_vcal);
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
                                icalcomponent *icalcomp_vcal,
                                gboolean do_copy,
                                GCancellable *cancellable,
                                GError **error)
{
	icalcomponent *icalcomp;
	icalcomponent *icalcomp_event, *subcomp;
	icalcomponent_kind icalcomp_kind;
	const gchar *uid;
	gchar *new_uid = NULL;
	struct ForeachTzidData ftd;
	ECalClientSourceType source_type;
	GHashTable *processed_uids;
	gboolean success = FALSE;

	g_return_val_if_fail (E_IS_CAL_CLIENT (src_client), FALSE);
	g_return_val_if_fail (E_IS_CAL_CLIENT (dest_client), FALSE);
	g_return_val_if_fail (icalcomp_vcal != NULL, FALSE);

	icalcomp_event = icalcomponent_get_inner (icalcomp_vcal);
	g_return_val_if_fail (icalcomp_event != NULL, FALSE);

	source_type = e_cal_client_get_source_type (src_client);
	switch (source_type) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			icalcomp_kind = ICAL_VEVENT_COMPONENT;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			icalcomp_kind = ICAL_VTODO_COMPONENT;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			icalcomp_kind = ICAL_VJOURNAL_COMPONENT;
			break;
		default:
			g_return_val_if_reached (FALSE);
	}

	processed_uids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	icalcomp_event = icalcomponent_get_first_component (icalcomp_vcal, icalcomp_kind);
	/*
	 * This check should be removed in the near future.
	 * We should be able to work properly with multiselection, which means that we always
	 * will receive a component with subcomponents.
	 */
	if (icalcomp_event == NULL)
		icalcomp_event = icalcomp_vcal;
	for (;
	     icalcomp_event;
	     icalcomp_event = icalcomponent_get_next_component (icalcomp_vcal, icalcomp_kind)) {
		GError *local_error = NULL;

		uid = icalcomponent_get_uid (icalcomp_event);

		if (g_hash_table_lookup (processed_uids, uid))
			continue;

		success = e_cal_client_get_object_sync (dest_client, uid, NULL, &icalcomp, cancellable, &local_error);
		if (success) {
			success = e_cal_client_modify_object_sync (
				dest_client, icalcomp_event, E_CAL_OBJ_MOD_ALL, cancellable, error);

			icalcomponent_free (icalcomp);
			if (!success)
				goto exit;

			if (!do_copy) {
				ECalObjModType mod_type = E_CAL_OBJ_MOD_THIS;

				/* Remove the item from the source calendar. */
				if (e_cal_util_component_is_instance (icalcomp_event) ||
				    e_cal_util_component_has_recurrences (icalcomp_event))
					mod_type = E_CAL_OBJ_MOD_ALL;

				success = e_cal_client_remove_object_sync (
						src_client, uid, NULL, mod_type, cancellable, error);
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

		if (e_cal_util_component_is_instance (icalcomp_event)) {
			GSList *ecalcomps = NULL, *eiter;
			ECalComponent *comp;

			success = e_cal_client_get_objects_for_uid_sync (src_client, uid, &ecalcomps, cancellable, error);
			if (!success)
				goto exit;

			if (ecalcomps && !ecalcomps->next) {
				/* only one component, no need for a vCalendar list */
				comp = ecalcomps->data;
				icalcomp = icalcomponent_new_clone (e_cal_component_get_icalcomponent (comp));
			} else {
				icalcomp = icalcomponent_new (ICAL_VCALENDAR_COMPONENT);
				for (eiter = ecalcomps; eiter; eiter = g_slist_next (eiter)) {
					comp = eiter->data;

					icalcomponent_add_component (
						icalcomp,
						icalcomponent_new_clone (e_cal_component_get_icalcomponent (comp)));
				}
			}

			e_cal_client_free_ecalcomp_slist (ecalcomps);
		} else {
			icalcomp = icalcomponent_new_clone (icalcomp_event);
		}

		if (do_copy) {
			/* Change the UID to avoid problems with duplicated UID */
			new_uid = e_cal_component_gen_uid ();
			if (icalcomponent_isa (icalcomp) == ICAL_VCALENDAR_COMPONENT) {
				/* in case of a vCalendar, the component might have detached instances,
				 * thus change the UID on all of the subcomponents of it */
				for (subcomp = icalcomponent_get_first_component (icalcomp, icalcomp_kind);
				     subcomp;
				     subcomp = icalcomponent_get_next_component (icalcomp, icalcomp_kind)) {
					icalcomponent_set_uid (subcomp, new_uid);
				}
			} else {
				icalcomponent_set_uid (icalcomp, new_uid);
			}
			g_free (new_uid);
			new_uid = NULL;
		}

		ftd.source_client = src_client;
		ftd.destination_client = dest_client;
		ftd.cancellable = cancellable;
		ftd.error = error;
		ftd.success = TRUE;

		if (icalcomponent_isa (icalcomp) == ICAL_VCALENDAR_COMPONENT) {
			/* in case of a vCalendar, the component might have detached instances,
			 * thus check timezones on all of the subcomponents of it */
			for (subcomp = icalcomponent_get_first_component (icalcomp, icalcomp_kind);
			     subcomp && ftd.success;
			     subcomp = icalcomponent_get_next_component (icalcomp, icalcomp_kind)) {
				icalcomponent_foreach_tzid (subcomp, add_timezone_to_cal_cb, &ftd);
			}
		} else {
			icalcomponent_foreach_tzid (icalcomp, add_timezone_to_cal_cb, &ftd);
		}

		if (!ftd.success) {
			success = FALSE;
			goto exit;
		}

		if (icalcomponent_isa (icalcomp) == ICAL_VCALENDAR_COMPONENT) {
			gboolean did_add = FALSE;

			/* in case of a vCalendar, the component might have detached instances,
			 * thus add the master object first, and then all of the subcomponents of it */
			for (subcomp = icalcomponent_get_first_component (icalcomp, icalcomp_kind);
			     subcomp && !did_add;
			     subcomp = icalcomponent_get_next_component (icalcomp, icalcomp_kind)) {
				if (icaltime_is_null_time (icalcomponent_get_recurrenceid (subcomp))) {
					did_add = TRUE;
					success = e_cal_client_create_object_sync (
						dest_client, subcomp,
						&new_uid, cancellable, error);
					g_free (new_uid);
				}
			}

			if (!success) {
				icalcomponent_free (icalcomp);
				goto exit;
			}

			/* deal with detached instances */
			for (subcomp = icalcomponent_get_first_component (icalcomp, icalcomp_kind);
			     subcomp && success;
			     subcomp = icalcomponent_get_next_component (icalcomp, icalcomp_kind)) {
				if (!icaltime_is_null_time (icalcomponent_get_recurrenceid (subcomp))) {
					if (did_add) {
						success = e_cal_client_modify_object_sync (
							dest_client, subcomp,
							E_CAL_OBJ_MOD_THIS, cancellable, error);
					} else {
						/* just in case there are only detached instances and no master object */
						did_add = TRUE;
						success = e_cal_client_create_object_sync (
							dest_client, subcomp,
							&new_uid, cancellable, error);
						g_free (new_uid);
					}
				}
			}
		} else {
			success = e_cal_client_create_object_sync (dest_client, icalcomp, &new_uid, cancellable, error);
			g_free (new_uid);
		}

		icalcomponent_free (icalcomp);
		if (!success)
			goto exit;

		if (!do_copy) {
			ECalObjModType mod_type = E_CAL_OBJ_MOD_THIS;

			/* Remove the item from the source calendar. */
			if (e_cal_util_component_is_instance (icalcomp_event) ||
			    e_cal_util_component_has_recurrences (icalcomp_event))
				mod_type = E_CAL_OBJ_MOD_ALL;

			success = e_cal_client_remove_object_sync (src_client, uid, NULL, mod_type, cancellable, error);
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
cal_comp_util_update_tzid_parameter (icalproperty *prop,
				     const struct icaltimetype tt)
{
	icalparameter *param;
	const gchar *tzid = NULL;

	g_return_if_fail (prop != NULL);

	if (!icaltime_is_valid_time (tt) ||
	    icaltime_is_null_time (tt))
		return;

	param = icalproperty_get_first_parameter (prop, ICAL_TZID_PARAMETER);
	if (tt.zone)
		tzid = icaltimezone_get_tzid ((icaltimezone *) tt.zone);

	if (tt.zone && tzid && *tzid && !tt.is_utc && !tt.is_date) {
		if (param) {
			icalparameter_set_tzid (param, (gchar *) tzid);
		} else {
			param = icalparameter_new_tzid ((gchar *) tzid);
			icalproperty_add_parameter (prop, param);
		}
	} else if (param) {
		icalproperty_remove_parameter (prop, ICAL_TZID_PARAMETER);
	}
}

/* Returns <0 for time before today, 0 for today, >0 for after today (future) */
gint
cal_comp_util_compare_time_with_today (const struct icaltimetype time_tt)
{
	struct icaltimetype now_tt;

	if (icaltime_is_null_time (time_tt))
		return 0;

	if (time_tt.is_date) {
		now_tt = icaltime_today ();
		return icaltime_compare_date_only (time_tt, now_tt);
	} else {
		now_tt = icaltime_current_time_with_zone (time_tt.zone);
		now_tt.zone = time_tt.zone;
	}

	return icaltime_compare (time_tt, now_tt);
}

/* Returns whether removed any */
gboolean
cal_comp_util_remove_all_properties (icalcomponent *component,
				     icalproperty_kind kind)
{
	icalproperty *prop;
	gboolean removed_any = FALSE;

	g_return_val_if_fail (component != NULL, FALSE);

	while (prop = icalcomponent_get_first_property (component, kind), prop) {
		icalcomponent_remove_property (component, prop);
		icalproperty_free (prop);

		removed_any = TRUE;
	}

	return removed_any;
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
