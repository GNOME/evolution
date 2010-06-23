/*
 * Evolution calendar - Utilities for manipulating ECalComponent objects
 *
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
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <time.h>
#include "calendar-config.h"
#include "comp-util.h"
#include "dialogs/delete-comp.h"
#include <libecal/e-cal-component.h>
#include "e-util/e-categories-config.h"
#include "common/authentication.h"

#include "gnome-cal.h"
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
cal_comp_util_add_exdate (ECalComponent *comp, time_t t, icaltimezone *zone)
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
e_cal_component_compare_tzid (const gchar *tzid1, const gchar *tzid2)
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
 * @client: A #ECal.
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
				       ECal *client,
				       icaltimezone *zone)
{
	ECalComponentDateTime start_datetime, end_datetime;
	const gchar *tzid;
	gboolean retval = FALSE;
	icaltimezone *start_zone, *end_zone;
	gint offset1, offset2;

	tzid = icaltimezone_get_tzid (zone);

	e_cal_component_get_dtstart (comp, &start_datetime);
	e_cal_component_get_dtend (comp, &end_datetime);

	/* If either the DTSTART or the DTEND is a DATE value, we return TRUE.
	   Maybe if one was a DATE-TIME we should check that, but that should
	   not happen often. */
	if ((start_datetime.value && start_datetime.value->is_date)
	    || (end_datetime.value && end_datetime.value->is_date)) {
		retval = TRUE;
		goto out;
	}

	/* If the event uses UTC for DTSTART & DTEND, return TRUE. Outlook
	   will send single events as UTC, so we don't want to mark all of
	   these. */
	if ((!start_datetime.value || start_datetime.value->is_utc)
	    && (!end_datetime.value || end_datetime.value->is_utc)) {
		retval = TRUE;
		goto out;
	}

	/* If the event uses floating time for DTSTART & DTEND, return TRUE.
	   Imported vCalendar files will use floating times, so we don't want
	   to mark all of these. */
	if (!start_datetime.tzid && !end_datetime.tzid) {
		retval = TRUE;
		goto out;
	}

	/* FIXME: DURATION may be used instead. */
	if (e_cal_component_compare_tzid (tzid, start_datetime.tzid)
	    && e_cal_component_compare_tzid (tzid, end_datetime.tzid)) {
		/* If both TZIDs are the same as the given zone's TZID, then
		   we know the timezones are the same so we return TRUE. */
		retval = TRUE;
	} else {
		/* If the TZIDs differ, we have to compare the UTC offsets
		   of the start and end times, using their own timezones and
		   the given timezone. */
		if (!e_cal_get_timezone (client, start_datetime.tzid,
					      &start_zone, NULL))
			goto out;

		if (start_datetime.value) {
			offset1 = icaltimezone_get_utc_offset (start_zone,
							       start_datetime.value,
							       NULL);
			offset2 = icaltimezone_get_utc_offset (zone,
							       start_datetime.value,
							       NULL);
			if (offset1 != offset2)
				goto out;
		}

		if (!e_cal_get_timezone (client, end_datetime.tzid,
					      &end_zone, NULL))
			goto out;

		if (end_datetime.value) {
			offset1 = icaltimezone_get_utc_offset (end_zone,
							       end_datetime.value,
							       NULL);
			offset2 = icaltimezone_get_utc_offset (zone,
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
 * cal_comp_confirm_delete_empty_comp:
 * @comp: A calendar component.
 * @client: Calendar client where the component purportedly lives.
 * @widget: Widget to be used as the basis for UTF8 conversion.
 *
 * Assumming a calendar component with an empty SUMMARY property (as per
 * string_is_empty()), asks whether the user wants to delete it based on
 * whether the appointment is on the calendar server or not.  If the
 * component is on the server, this function will present a confirmation
 * dialog and delete the component if the user tells it to.  If the component
 * is not on the server it will just return TRUE.
 *
 * Return value: A result code indicating whether the component
 * was not on the server and is to be deleted locally, whether it
 * was on the server and the user deleted it, or whether the
 * user cancelled the deletion.
 **/
gboolean
cal_comp_is_on_server (ECalComponent *comp, ECal *client)
{
	const gchar *uid;
	gchar *rid = NULL;
	icalcomponent *icalcomp;
	GError *error = NULL;

	g_return_val_if_fail (comp != NULL, FALSE);
	g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), FALSE);
	g_return_val_if_fail (client != NULL, FALSE);
	g_return_val_if_fail (E_IS_CAL (client), FALSE);

	/* See if the component is on the server.  If it is not, then it likely
	 * means that the appointment is new, only in the day view, and we
	 * haven't added it yet to the server.  In that case, we don't need to
	 * confirm and we can just delete the event.  Otherwise, we ask
	 * the user.
	 */
	e_cal_component_get_uid (comp, &uid);

	/*TODO We should not be checking for this here. But since e_cal_util_construct_instance does not
	  create the instances of all day events, so we dafault to old behaviour */
	if (e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_RECURRENCES_NO_MASTER)) {
		rid = e_cal_component_get_recurid_as_string (comp);
	}

	if (e_cal_get_object (client, uid, rid, &icalcomp, &error)) {
		icalcomponent_free (icalcomp);
		g_free (rid);

		return TRUE;
	}

	if (error->code != E_CALENDAR_STATUS_OBJECT_NOT_FOUND)
		g_warning (G_STRLOC ": %s", error->message);

	g_clear_error (&error);
	g_free (rid);

	return FALSE;
}

/**
 * is_icalcomp_on_the_server:
 * same as @cal_comp_is_on_server, only the component parameter is icalcomponent, not the ECalComponent.
 **/
gboolean
is_icalcomp_on_the_server (icalcomponent *icalcomp, ECal *client)
{
	gboolean on_server;
	ECalComponent *comp;

	if (!icalcomp || !client || !icalcomponent_get_uid (icalcomp))
		return FALSE;

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (icalcomp));

	on_server = cal_comp_is_on_server (comp, client);

	g_object_unref (comp);

	return on_server;
}

/**
 * cal_comp_event_new_with_defaults:
 *
 * Creates a new VEVENT component and adds any default alarms to it as set in
 * the program's configuration values, but only if not the all_day event.
 *
 * Return value: A newly-created calendar component.
 **/
ECalComponent *
cal_comp_event_new_with_defaults (ECal *client, gboolean all_day)
{
	icalcomponent *icalcomp;
	ECalComponent *comp;
	gint interval;
	CalUnits units;
	ECalComponentAlarm *alarm;
	icalproperty *icalprop;
	ECalComponentAlarmTrigger trigger;

	if (!e_cal_get_default_object (client, &icalcomp, NULL))
		icalcomp = icalcomponent_new (ICAL_VEVENT_COMPONENT);

	comp = e_cal_component_new ();
	if (!e_cal_component_set_icalcomponent (comp, icalcomp)) {
		icalcomponent_free (icalcomp);

		e_cal_component_set_new_vtype (comp, E_CAL_COMPONENT_EVENT);
	}

	if (all_day || !calendar_config_get_use_default_reminder ())
		return comp;

	interval = calendar_config_get_default_reminder_interval ();
	units = calendar_config_get_default_reminder_units ();

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

	switch (units) {
	case CAL_MINUTES:
		trigger.u.rel_duration.minutes = interval;
		break;

	case CAL_HOURS:
		trigger.u.rel_duration.hours = interval;
		break;

	case CAL_DAYS:
		trigger.u.rel_duration.days = interval;
		break;

	default:
		g_warning ("wrong units %d\n", units);
	}

	e_cal_component_alarm_set_trigger (alarm, trigger);

	e_cal_component_add_alarm (comp, alarm);
	e_cal_component_alarm_free (alarm);

	return comp;
}

ECalComponent *
cal_comp_event_new_with_current_time (ECal *client, gboolean all_day)
{
	ECalComponent *comp;
	struct icaltimetype itt;
	ECalComponentDateTime dt;
	icaltimezone *zone;

	comp = cal_comp_event_new_with_defaults (client, all_day);

	g_return_val_if_fail (comp, NULL);

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
cal_comp_task_new_with_defaults (ECal *client)
{
	ECalComponent *comp;
	icalcomponent *icalcomp;

	if (!e_cal_get_default_object (client, &icalcomp, NULL))
		icalcomp = icalcomponent_new (ICAL_VTODO_COMPONENT);

	comp = e_cal_component_new ();
	if (!e_cal_component_set_icalcomponent (comp, icalcomp)) {
		icalcomponent_free (icalcomp);

		e_cal_component_set_new_vtype (comp, E_CAL_COMPONENT_TODO);
	}

	return comp;
}

ECalComponent *
cal_comp_memo_new_with_defaults (ECal *client)
{
	ECalComponent *comp;
	icalcomponent *icalcomp;

	if (!e_cal_get_default_object (client, &icalcomp, NULL))
		icalcomp = icalcomponent_new (ICAL_VJOURNAL_COMPONENT);

	comp = e_cal_component_new ();
	if (!e_cal_component_set_icalcomponent (comp, icalcomp)) {
		icalcomponent_free (icalcomp);

		e_cal_component_set_new_vtype (comp, E_CAL_COMPONENT_JOURNAL);
	}

	return comp;
}

void
cal_comp_update_time_by_active_window (ECalComponent *comp, EShell *shell)
{
	GtkWindow *window;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (shell != NULL);

	window = e_shell_get_active_window (shell);
	if (window && E_IS_SHELL_WINDOW (window)) {
		EShellWindow *shell_window = E_SHELL_WINDOW (window);

		if (e_shell_window_get_active_view (shell_window)
		    && g_str_equal (e_shell_window_get_active_view (shell_window), "calendar")) {
			EShellView *view;
			GnomeCalendar *gnome_cal;
			time_t start = 0, end = 0;
			icaltimezone *zone;
			struct icaltimetype itt;
			icalcomponent *icalcomp;
			icalproperty *prop;

			view = e_shell_window_peek_shell_view (shell_window, "calendar");
			g_return_if_fail (view != NULL);

			gnome_cal = NULL;
			g_object_get (G_OBJECT (e_shell_view_get_shell_content (view)), "calendar", &gnome_cal, NULL);
			g_return_if_fail (gnome_cal != NULL);

			gnome_calendar_get_current_time_range (gnome_cal, &start, &end);
			g_return_if_fail (start != 0);

			zone = e_cal_model_get_timezone (gnome_calendar_get_model (gnome_cal));
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
cal_comp_util_get_n_icons (ECalComponent *comp, GSList **pixbufs)
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
 * Stores list of strings into selection target data.
 * Use @ref cal_comp_selection_get_string_list to get this list from target data.
 *
 * @param data Selection data, where to put list of strings.
 * @param str_list List of strings. (Each element is of type const gchar *.)
 **/
void
cal_comp_selection_set_string_list (GtkSelectionData *data, GSList *str_list)
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
 * Converts data from selection to list of strings. Data should be assigned
 * to selection data with @ref cal_comp_selection_set_string_list.
 * Each string in newly created list should be freed by g_free.
 * List itself should be freed by g_slist_free.
 *
 * @param data Selection data, where to put list of strings.
 * @return Newly allocated GSList of strings.
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
datetime_to_zone (ECal *client, ECalComponentDateTime *date, const gchar *tzid)
{
	icaltimezone *from, *to;

	g_return_if_fail (date != NULL);

	if (date->tzid == NULL || tzid == NULL ||
	    date->tzid == tzid || g_str_equal (date->tzid, tzid))
		return;

	from = icaltimezone_get_builtin_timezone_from_tzid (date->tzid);
	if (!from) {
		if (!e_cal_get_timezone (client, date->tzid, &from, NULL))
			g_warning ("%s: Could not get timezone from server: %s", G_STRFUNC, date->tzid ? date->tzid : "");
	}

	to = icaltimezone_get_builtin_timezone_from_tzid (tzid);
	if (!to) {
		/* do not check failure here, maybe the zone is not available there */
		e_cal_get_timezone (client, tzid, &to, NULL);
	}

	icaltimezone_convert_time (date->value, from, to);
	date->tzid = tzid;
}

/**
 * cal_comp_set_dtstart_with_oldzone:
 * Changes 'dtstart' of the component, but converts time to the old timezone.
 * @param client ECal structure, to retrieve timezone from, when required.
 * @param comp Component, where make the change.
 * @param pdate Value, to change to.
 **/
void
cal_comp_set_dtstart_with_oldzone (ECal *client, ECalComponent *comp, const ECalComponentDateTime *pdate)
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
 * Changes 'dtend' of the component, but converts time to the old timezone.
 * @param client ECal structure, to retrieve timezone from, when required.
 * @param comp Component, where make the change.
 * @param pdate Value, to change to.
 **/
void
cal_comp_set_dtend_with_oldzone (ECal *client, ECalComponent *comp, const ECalComponentDateTime *pdate)
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

static gboolean
update_single_object (ECal *client, icalcomponent *icalcomp, gboolean fail_on_modify)
{
	const gchar *uid;
	gchar *tmp;
	icalcomponent *tmp_icalcomp;
	gboolean res;

	uid = icalcomponent_get_uid (icalcomp);

	if (e_cal_get_object (client, uid, NULL, &tmp_icalcomp, NULL)) {
		if (fail_on_modify)
			return FALSE;

		return e_cal_modify_object (client, icalcomp, CALOBJ_MOD_ALL, NULL);
	}

	tmp = NULL;
	res = e_cal_create_object (client, icalcomp, &tmp, NULL);

	g_free (tmp);

	return res;
}

static gboolean
update_objects (ECal *client, icalcomponent *icalcomp)
{
	icalcomponent *subcomp;
	icalcomponent_kind kind;

	kind = icalcomponent_isa (icalcomp);
	if (kind == ICAL_VTODO_COMPONENT ||
	    kind == ICAL_VEVENT_COMPONENT ||
	    kind == ICAL_VJOURNAL_COMPONENT)
		return update_single_object (client, icalcomp, kind == ICAL_VJOURNAL_COMPONENT);
	else if (kind != ICAL_VCALENDAR_COMPONENT)
		return FALSE;

	subcomp = icalcomponent_get_first_component (icalcomp, ICAL_ANY_COMPONENT);
	while (subcomp) {
		gboolean success;

		kind = icalcomponent_isa (subcomp);
		if (kind == ICAL_VTIMEZONE_COMPONENT) {
			icaltimezone *zone;

			zone = icaltimezone_new ();
			icaltimezone_set_component (zone, subcomp);

			success = e_cal_add_timezone (client, zone, NULL);
			icaltimezone_free (zone, 1);
			if (!success)
				return success;
		} else if (kind == ICAL_VTODO_COMPONENT ||
			   kind == ICAL_VEVENT_COMPONENT ||
			   kind == ICAL_VJOURNAL_COMPONENT) {
			success = update_single_object (client, subcomp, kind == ICAL_VJOURNAL_COMPONENT);
			if (!success)
				return success;
		}

		subcomp = icalcomponent_get_next_component (icalcomp, ICAL_ANY_COMPONENT);
	}

	return TRUE;
}

/**
 * cal_comp_process_source_list_drop:
 * Processes the drop signal over the ESourceList.
 * @param destination Where to put the component.
 * @param comp Component to move/copy.
 * @param action What to do.
 * @param source_uid Where the component comes from; used when moving.
 * @param source_list The ESourceList over which the event was called.
 * @return Whether was the operation successful.
 **/
gboolean
cal_comp_process_source_list_drop (ECal *destination, icalcomponent *comp, GdkDragAction action, const gchar *source_uid, ESourceList *source_list)
{
	const gchar * uid;
	gchar *old_uid = NULL;
	icalcomponent *tmp_icalcomp = NULL;
	GError *error = NULL;
	gboolean success = FALSE;

	g_return_val_if_fail (destination != NULL, FALSE);
	g_return_val_if_fail (comp != NULL, FALSE);
	g_return_val_if_fail (source_uid != NULL, FALSE);
	g_return_val_if_fail (source_list != NULL, FALSE);

	/* FIXME deal with GDK_ACTION_ASK */
	if (action == GDK_ACTION_COPY) {
		gchar *tmp;

		old_uid = g_strdup (icalcomponent_get_uid (comp));
		tmp = e_cal_component_gen_uid ();

		icalcomponent_set_uid (comp, tmp);
		g_free (tmp);
	}

	uid = icalcomponent_get_uid (comp);
	if (!old_uid)
		old_uid = g_strdup (uid);

	if (!e_cal_get_object (destination, uid, NULL, &tmp_icalcomp, &error)) {
		if ((error != NULL) && (error->code != E_CALENDAR_STATUS_OBJECT_NOT_FOUND)) {
			switch (e_cal_get_source_type (destination)) {
			case E_CAL_SOURCE_TYPE_EVENT:
				g_message ("Failed to search the object in destination event list: %s", error->message);
				break;
			case E_CAL_SOURCE_TYPE_TODO:
				g_message ("Failed to search the object in destination task list: %s", error->message);
				break;
			case E_CAL_SOURCE_TYPE_JOURNAL:
				g_message ("Failed to search the object in destination memo list: %s", error->message);
				break;
			default:
				break;
			}
		} else {
			/* this will report success by last item, but we don't care */
			success = update_objects (destination, comp);

			if (success && action == GDK_ACTION_MOVE) {
				/* remove components rather here, because we know which has been moved */
				ESource *source_source;
				ECal *source_client;

				source_source = e_source_list_peek_source_by_uid (source_list, source_uid);

				if (source_source && !E_IS_SOURCE_GROUP (source_source) && !e_source_get_readonly (source_source)) {
					source_client = e_auth_new_cal_from_source (source_source, e_cal_get_source_type (destination));

					if (source_client) {
						gboolean read_only = TRUE;

						e_cal_is_read_only (source_client, &read_only, NULL);

						if (!read_only && e_cal_open (source_client, TRUE, NULL))
							e_cal_remove_object (source_client, old_uid, NULL);
						else if (!read_only) {
							switch (e_cal_get_source_type (destination)) {
							case E_CAL_SOURCE_TYPE_EVENT:
								g_message ("Cannot open source client to remove old event");
								break;
							case E_CAL_SOURCE_TYPE_TODO:
								g_message ("Cannot open source client to remove old task");
								break;
							case E_CAL_SOURCE_TYPE_JOURNAL:
								g_message ("Cannot open source client to remove old memo");
								break;
							default:
								break;
							}
						}

						g_object_unref (source_client);
					} else {
						switch (e_cal_get_source_type (destination)) {
						case E_CAL_SOURCE_TYPE_EVENT:
							g_message ("Cannot create source client to remove old event");
							break;
						case E_CAL_SOURCE_TYPE_TODO:
							g_message ("Cannot create source client to remove old task");
							break;
						case E_CAL_SOURCE_TYPE_JOURNAL:
							g_message ("Cannot create source client to remove old memo");
							break;
						default:
							break;
						}
					}
				}
			}
		}

		g_clear_error (&error);
	} else
		icalcomponent_free (tmp_icalcomp);

	g_free (old_uid);

	return success;
}

void
comp_util_sanitize_recurrence_master (ECalComponent *comp, ECal *client)
{
	ECalComponent *master = NULL;
	icalcomponent *icalcomp = NULL;
	ECalComponentRange rid;
	ECalComponentDateTime sdt;
	const gchar *uid;

	/* Get the master component */
	e_cal_component_get_uid (comp, &uid);
	if (!e_cal_get_object (client, uid, NULL, &icalcomp, NULL)) {
		g_warning ("Unable to get the master component \n");
		return;
	}

	master = e_cal_component_new ();
	e_cal_component_set_icalcomponent (master, icalcomp);

	/* Compare recur id and start date */
	e_cal_component_get_recurid (comp, &rid);
	e_cal_component_get_dtstart (comp, &sdt);

	if (rid.datetime.value && sdt.value && icaltime_compare_date_only (*rid.datetime.value, *sdt.value) == 0) {
		ECalComponentDateTime msdt, medt, edt;
		gint *sequence;

		e_cal_component_get_dtstart (master, &msdt);
		e_cal_component_get_dtend (master, &medt);

		e_cal_component_get_dtend (comp, &edt);

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
}

gchar *
icalcomp_suggest_filename (icalcomponent *icalcomp, const gchar *default_name)
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
