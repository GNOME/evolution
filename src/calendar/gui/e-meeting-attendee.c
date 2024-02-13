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
 *		JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <stdlib.h>
#include <gtk/gtk.h>

#include "comp-util.h"
#include "e-meeting-utils.h"
#include "e-meeting-attendee.h"

struct _EMeetingAttendeePrivate {
	gchar *address;
	gchar *member;
	gchar *fburi;

	ICalParameterCutype cutype;
	ICalParameterRole role;

	gboolean rsvp;

	gchar *delto;
	gchar *delfrom;

	ICalParameterPartstat partstat;

	gchar *sentby;
	gchar *cn;
	gchar *language;

	ECalComponentParameterBag *parameter_bag;

	EMeetingAttendeeEditLevel edit_level;

	gboolean show_address;
	gboolean has_calendar_info;

	GArray *busy_periods;
	gboolean busy_periods_sorted;

	EMeetingTime busy_periods_start;
	EMeetingTime busy_periods_end;
	gboolean start_busy_range_set;
	gboolean end_busy_range_set;

	gint longest_period_in_days;
};

enum {
	CHANGED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

static void e_meeting_attendee_finalize	(GObject *obj);

G_DEFINE_TYPE_WITH_PRIVATE (EMeetingAttendee, e_meeting_attendee, G_TYPE_OBJECT)

static gchar *
string_test (const gchar *string)
{
	return g_strdup (string ? string : "");
}

static gboolean
string_is_set (const gchar *string)
{
	if (string != NULL && *string != '\0')
		return TRUE;

	return FALSE;
}

static void
busy_periods_array_clear_func (gpointer data)
{
	EMeetingFreeBusyPeriod *period = (EMeetingFreeBusyPeriod *) data;

	/* We're expected to clear the data segment,
	 * but not deallocate the segment itself. The
	 * XFB data possibly attached to the
	 * EMeetingFreeBusyPeriod requires special
	 * care when removing elements from the GArray
	 */
	e_meeting_xfb_data_clear (&(period->xfb));
}

static void
notify_changed (EMeetingAttendee *ia)
{
	g_signal_emit_by_name (ia, "changed");
}

static void
set_string_value (EMeetingAttendee *ia,
		  gchar **member,
		  const gchar *value)
{
	if (!string_is_set (*member) && !string_is_set (value))
		return;

	if (g_strcmp0 (*member, value) == 0)
		return;

	g_free (*member);
	*member = string_test (value);

	notify_changed (ia);
}

static void
e_meeting_attendee_finalize (GObject *object)
{
	EMeetingAttendee *ia;

	ia = E_MEETING_ATTENDEE (object);

	g_free (ia->priv->address);
	g_free (ia->priv->member);
	g_free (ia->priv->fburi);

	g_free (ia->priv->delto);
	g_free (ia->priv->delfrom);

	g_free (ia->priv->sentby);
	g_free (ia->priv->cn);
	g_free (ia->priv->language);

	e_cal_component_parameter_bag_free (ia->priv->parameter_bag);

	g_array_free (ia->priv->busy_periods, TRUE);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_meeting_attendee_parent_class)->finalize (object);
}

static void
e_meeting_attendee_class_init (EMeetingAttendeeClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = e_meeting_attendee_finalize;

	signals[CHANGED] = g_signal_new (
		"changed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EMeetingAttendeeClass, changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_meeting_attendee_init (EMeetingAttendee *ia)
{
	ia->priv = e_meeting_attendee_get_instance_private (ia);

	ia->priv->address = string_test (NULL);
	ia->priv->member = string_test (NULL);

	ia->priv->cutype = I_CAL_CUTYPE_NONE;
	ia->priv->role = I_CAL_ROLE_NONE;

	ia->priv->rsvp = FALSE;

	ia->priv->delto = string_test (NULL);
	ia->priv->delfrom = string_test (NULL);

	ia->priv->partstat = I_CAL_PARTSTAT_NONE;

	ia->priv->sentby = string_test (NULL);
	ia->priv->cn = string_test (NULL);
	ia->priv->language = string_test (NULL);

	ia->priv->parameter_bag = e_cal_component_parameter_bag_new ();

	ia->priv->edit_level = E_MEETING_ATTENDEE_EDIT_FULL;
	ia->priv->show_address = FALSE;
	ia->priv->has_calendar_info = FALSE;

	ia->priv->busy_periods = g_array_new (FALSE, FALSE, sizeof (EMeetingFreeBusyPeriod));
	g_array_set_clear_func (ia->priv->busy_periods, busy_periods_array_clear_func);
	ia->priv->busy_periods_sorted = FALSE;

	g_date_clear (&ia->priv->busy_periods_start.date, 1);
	ia->priv->busy_periods_start.hour = 0;
	ia->priv->busy_periods_start.minute = 0;

	g_date_clear (&ia->priv->busy_periods_end.date, 1);
	ia->priv->busy_periods_end.hour = 0;
	ia->priv->busy_periods_end.minute = 0;

	ia->priv->start_busy_range_set = FALSE;
	ia->priv->end_busy_range_set = FALSE;

	ia->priv->longest_period_in_days = 0;
}

GObject *
e_meeting_attendee_new (void)
{
	return g_object_new (E_TYPE_MEETING_ATTENDEE, NULL);
}

GObject *
e_meeting_attendee_new_from_e_cal_component_attendee (const ECalComponentAttendee *ca)
{
	EMeetingAttendee *ia;

	g_return_val_if_fail (ca != NULL, NULL);

	ia = E_MEETING_ATTENDEE (g_object_new (E_TYPE_MEETING_ATTENDEE, NULL));

	e_meeting_attendee_set_address (ia, e_cal_util_get_attendee_email (ca));
	e_meeting_attendee_set_member (ia, e_cal_component_attendee_get_member (ca));
	e_meeting_attendee_set_cutype (ia, e_cal_component_attendee_get_cutype (ca));
	e_meeting_attendee_set_role (ia, e_cal_component_attendee_get_role (ca));
	e_meeting_attendee_set_partstat (ia, e_cal_component_attendee_get_partstat (ca));
	e_meeting_attendee_set_rsvp (ia, e_cal_component_attendee_get_rsvp (ca));
	e_meeting_attendee_set_delto (ia, e_cal_component_attendee_get_delegatedto (ca));
	e_meeting_attendee_set_delfrom (ia, e_cal_component_attendee_get_delegatedfrom (ca));
	e_meeting_attendee_set_sentby (ia, e_cal_component_attendee_get_sentby (ca));
	e_meeting_attendee_set_cn (ia, e_cal_component_attendee_get_cn (ca));
	e_meeting_attendee_set_language (ia, e_cal_component_attendee_get_language (ca));
	e_cal_component_parameter_bag_assign (ia->priv->parameter_bag,
		e_cal_component_attendee_get_parameter_bag (ca));

	return G_OBJECT (ia);
}

ECalComponentAttendee *
e_meeting_attendee_as_e_cal_component_attendee (const EMeetingAttendee *ia)
{
	ECalComponentAttendee *attendee;

	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), NULL);

	attendee = e_cal_component_attendee_new_full (
		ia->priv->address,
		string_is_set (ia->priv->member) ? ia->priv->member : NULL,
		ia->priv->cutype,
		ia->priv->role,
		ia->priv->partstat,
		ia->priv->rsvp,
		string_is_set (ia->priv->delfrom) ? ia->priv->delfrom : NULL,
		string_is_set (ia->priv->delto) ? ia->priv->delto : NULL,
		string_is_set (ia->priv->sentby) ? ia->priv->sentby : NULL,
		string_is_set (ia->priv->cn) ? ia->priv->cn : NULL,
		string_is_set (ia->priv->language) ? ia->priv->language : NULL);

	e_cal_component_parameter_bag_assign (e_cal_component_attendee_get_parameter_bag (attendee),
		ia->priv->parameter_bag);

	return attendee;
}

const gchar *
e_meeting_attendee_get_fburi (const EMeetingAttendee *ia)
{
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), NULL);

	return ia->priv->fburi;
}

void
e_meeting_attendee_set_fburi (EMeetingAttendee *ia,
			      const gchar *fburi)
{
	g_return_if_fail (E_IS_MEETING_ATTENDEE (ia));

	set_string_value (ia, &ia->priv->fburi, fburi);
}

const gchar *
e_meeting_attendee_get_address (const EMeetingAttendee *ia)
{
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), NULL);

	return ia->priv->address;
}

void
e_meeting_attendee_set_address (EMeetingAttendee *ia,
				const gchar *address)
{
	g_return_if_fail (E_IS_MEETING_ATTENDEE (ia));

	if (address && *address && g_ascii_strncasecmp (address, "mailto:", 7) != 0) {
		/* Always with mailto: prefix */
		gchar *tmp = g_strconcat ("mailto:", address, NULL);
		set_string_value (ia, &ia->priv->address, tmp);
		g_free (tmp);
	} else {
		set_string_value (ia, &ia->priv->address, address);
	}
}

gboolean
e_meeting_attendee_is_set_address (const EMeetingAttendee *ia)
{
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), FALSE);

	return string_is_set (ia->priv->address);
}

const gchar *
e_meeting_attendee_get_member (const EMeetingAttendee *ia)
{
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), NULL);

	return ia->priv->member;
}

void
e_meeting_attendee_set_member (EMeetingAttendee *ia,
			       const gchar *member)
{
	g_return_if_fail (E_IS_MEETING_ATTENDEE (ia));

	set_string_value (ia, &ia->priv->member, member);
}

gboolean
e_meeting_attendee_is_set_member (const EMeetingAttendee *ia)
{
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), FALSE);

	return string_is_set (ia->priv->member);
}

ICalParameterCutype
e_meeting_attendee_get_cutype (const EMeetingAttendee *ia)
{
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), I_CAL_CUTYPE_NONE);

	return ia->priv->cutype;
}

void
e_meeting_attendee_set_cutype (EMeetingAttendee *ia,
			       ICalParameterCutype cutype)
{
	g_return_if_fail (E_IS_MEETING_ATTENDEE (ia));

	if (ia->priv->cutype != cutype) {
		ia->priv->cutype = cutype;
		notify_changed (ia);
	}
}

ICalParameterRole
e_meeting_attendee_get_role (const EMeetingAttendee *ia)
{
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), I_CAL_ROLE_NONE);

	return ia->priv->role;
}

void
e_meeting_attendee_set_role (EMeetingAttendee *ia,
			     ICalParameterRole role)
{
	g_return_if_fail (E_IS_MEETING_ATTENDEE (ia));

	if (ia->priv->role != role) {
		ia->priv->role = role;
		notify_changed (ia);
	}
}

gboolean
e_meeting_attendee_get_rsvp (const EMeetingAttendee *ia)
{
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), FALSE);

	return ia->priv->rsvp;
}

void
e_meeting_attendee_set_rsvp (EMeetingAttendee *ia,
                             gboolean rsvp)
{
	g_return_if_fail (E_IS_MEETING_ATTENDEE (ia));

	if ((ia->priv->rsvp ? 1 : 0) != (rsvp ? 1 : 0)) {
		ia->priv->rsvp = rsvp;
		notify_changed (ia);
	}
}

const gchar *
e_meeting_attendee_get_delto (const EMeetingAttendee *ia)
{
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), NULL);

	return ia->priv->delto;
}

void
e_meeting_attendee_set_delto (EMeetingAttendee *ia,
			      const gchar *delto)
{
	g_return_if_fail (E_IS_MEETING_ATTENDEE (ia));

	set_string_value (ia, &ia->priv->delto, delto);
}

gboolean
e_meeting_attendee_is_set_delto (const EMeetingAttendee *ia)
{
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), FALSE);

	return string_is_set (ia->priv->delto);
}

const gchar *
e_meeting_attendee_get_delfrom (const EMeetingAttendee *ia)
{
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), NULL);

	return ia->priv->delfrom;
}

void
e_meeting_attendee_set_delfrom (EMeetingAttendee *ia,
				const gchar *delfrom)
{
	g_return_if_fail (E_IS_MEETING_ATTENDEE (ia));

	set_string_value (ia, &ia->priv->delfrom, delfrom);
}

gboolean
e_meeting_attendee_is_set_delfrom (const EMeetingAttendee *ia)
{
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), FALSE);

	return string_is_set (ia->priv->delfrom);
}

ICalParameterPartstat
e_meeting_attendee_get_partstat (const EMeetingAttendee *ia)
{
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), I_CAL_PARTSTAT_NONE);

	return ia->priv->partstat;
}

void
e_meeting_attendee_set_partstat (EMeetingAttendee *ia,
				 ICalParameterPartstat partstat)
{
	g_return_if_fail (E_IS_MEETING_ATTENDEE (ia));

	if (ia->priv->partstat != partstat) {
		ia->priv->partstat = partstat;
		notify_changed (ia);
	}
}

const gchar *
e_meeting_attendee_get_sentby (const EMeetingAttendee *ia)
{
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), NULL);

	return ia->priv->sentby;
}

void
e_meeting_attendee_set_sentby (EMeetingAttendee *ia,
			       const gchar *sentby)
{
	g_return_if_fail (E_IS_MEETING_ATTENDEE (ia));

	set_string_value (ia, &ia->priv->sentby, sentby);
}

gboolean
e_meeting_attendee_is_set_sentby (const EMeetingAttendee *ia)
{
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), FALSE);

	return string_is_set (ia->priv->sentby);
}

const gchar *
e_meeting_attendee_get_cn (const EMeetingAttendee *ia)
{
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), NULL);

	return ia->priv->cn;
}

void
e_meeting_attendee_set_cn (EMeetingAttendee *ia,
			   const gchar *cn)
{
	g_return_if_fail (E_IS_MEETING_ATTENDEE (ia));

	set_string_value (ia, &ia->priv->cn, cn);
}

gboolean
e_meeting_attendee_is_set_cn (const EMeetingAttendee *ia)
{
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), FALSE);

	return string_is_set (ia->priv->cn);
}

const gchar *
e_meeting_attendee_get_language (const EMeetingAttendee *ia)
{
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), NULL);

	return ia->priv->language;
}

void
e_meeting_attendee_set_language (EMeetingAttendee *ia,
				 const gchar *language)
{
	g_return_if_fail (E_IS_MEETING_ATTENDEE (ia));

	set_string_value (ia, &ia->priv->language, language);
}

gboolean
e_meeting_attendee_is_set_language (const EMeetingAttendee *ia)
{
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), FALSE);

	return string_is_set (ia->priv->language);
}

ECalComponentParameterBag *
e_meeting_attendee_get_parameter_bag (const EMeetingAttendee *ia)
{
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), NULL);

	return ia->priv->parameter_bag;
}

EMeetingAttendeeType
e_meeting_attendee_get_atype (const EMeetingAttendee *ia)
{
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), E_MEETING_ATTENDEE_RESOURCE_UNKNOWN);

	if (ia->priv->cutype == I_CAL_CUTYPE_ROOM ||
	    ia->priv->cutype == I_CAL_CUTYPE_RESOURCE)
		return E_MEETING_ATTENDEE_RESOURCE;

	if (ia->priv->role == I_CAL_ROLE_CHAIR ||
	    ia->priv->role == I_CAL_ROLE_REQPARTICIPANT)
		return E_MEETING_ATTENDEE_REQUIRED_PERSON;

	return E_MEETING_ATTENDEE_OPTIONAL_PERSON;
}

EMeetingAttendeeEditLevel
e_meeting_attendee_get_edit_level (const EMeetingAttendee *ia)
{
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), E_MEETING_ATTENDEE_EDIT_NONE);

	return ia->priv->edit_level;
}

void
e_meeting_attendee_set_edit_level (EMeetingAttendee *ia,
                                   EMeetingAttendeeEditLevel level)
{
	g_return_if_fail (E_IS_MEETING_ATTENDEE (ia));

	ia->priv->edit_level = level;
}

static gint
compare_times (EMeetingTime *time1,
               EMeetingTime *time2)
{
	gint day_comparison;

	day_comparison = g_date_compare (
		&time1->date,
		&time2->date);
	if (day_comparison != 0)
		return day_comparison;

	if (time1->hour < time2->hour)
		return -1;
	if (time1->hour > time2->hour)
		return 1;

	if (time1->minute < time2->minute)
		return -1;
	if (time1->minute > time2->minute)
		return 1;

	/* The start times are exactly the same. */
	return 0;
}

static gint
compare_period_starts (gconstpointer arg1,
                       gconstpointer arg2)
{
	EMeetingFreeBusyPeriod *period1, *period2;

	period1 = (EMeetingFreeBusyPeriod *) arg1;
	period2 = (EMeetingFreeBusyPeriod *) arg2;

	return compare_times (&period1->start, &period2->start);
}

static void
ensure_periods_sorted (EMeetingAttendee *ia)
{
	if (ia->priv->busy_periods_sorted)
		return;

	qsort (
		ia->priv->busy_periods->data, ia->priv->busy_periods->len,
		sizeof (EMeetingFreeBusyPeriod),
		compare_period_starts);

	ia->priv->busy_periods_sorted = TRUE;
}

gboolean
e_meeting_attendee_get_show_address (const EMeetingAttendee *ia)
{
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), FALSE);

	return ia->priv->show_address;
}

void
e_meeting_attendee_set_show_address (EMeetingAttendee *ia,
				     gboolean show_address)
{
	g_return_if_fail (E_IS_MEETING_ATTENDEE (ia));

	if ((ia->priv->show_address ? 1 : 0) == (show_address ? 1 : 0))
		return;

	ia->priv->show_address = show_address;

	notify_changed (ia);
}

gboolean
e_meeting_attendee_get_has_calendar_info (const EMeetingAttendee *ia)
{
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), FALSE);

	return ia->priv->has_calendar_info;
}

void
e_meeting_attendee_set_has_calendar_info (EMeetingAttendee *ia,
                                          gboolean has_calendar_info)
{
	g_return_if_fail (E_IS_MEETING_ATTENDEE (ia));

	ia->priv->has_calendar_info = has_calendar_info;
}

const GArray *
e_meeting_attendee_get_busy_periods (EMeetingAttendee *ia)
{
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), NULL);

	ensure_periods_sorted (ia);

	return ia->priv->busy_periods;
}

gint
e_meeting_attendee_find_first_busy_period (EMeetingAttendee *ia,
					   const GDate *date)
{
	EMeetingFreeBusyPeriod *period;
	gint lower, upper, middle = 0, cmp = 0;
	GDate tmp_date;

	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), -1);

	/* Make sure the busy periods have been sorted. */
	ensure_periods_sorted (ia);

	/* Calculate the first day which could have a busy period which
	 * continues onto our given date. */
	tmp_date = *date;
	g_date_subtract_days (&tmp_date, ia->priv->longest_period_in_days);

	/* We want the first busy period which starts on tmp_date. */
	lower = 0;
	upper = ia->priv->busy_periods->len;

	if (upper == 0)
		return -1;

	while (lower < upper) {
		middle = (lower + upper) >> 1;

		period = &g_array_index (ia->priv->busy_periods,
					 EMeetingFreeBusyPeriod, middle);

		cmp = g_date_compare (&tmp_date, &period->start.date);

		if (cmp == 0)
			break;
		else if (cmp < 0)
			upper = middle;
		else
			lower = middle + 1;
	}

	/* There may be several busy periods on the same day so we step
	 * backwards to the first one. */
	if (cmp == 0) {
		while (middle > 0) {
			period = &g_array_index (ia->priv->busy_periods,
						 EMeetingFreeBusyPeriod, middle - 1);
			if (g_date_compare (&tmp_date, &period->start.date) != 0)
				break;
			middle--;
		}
	} else if (cmp > 0) {
		/* This means we couldn't find a period on the given day, and
		 * the last one we looked at was before it, so if there are
		 * any more periods after this one we return it. */
		middle++;
		if (ia->priv->busy_periods->len <= middle)
			return -1;
	}

	return middle;
}

gboolean
e_meeting_attendee_add_busy_period (EMeetingAttendee *ia,
                                    gint start_year,
                                    gint start_month,
                                    gint start_day,
                                    gint start_hour,
                                    gint start_minute,
                                    gint end_year,
                                    gint end_month,
                                    gint end_day,
                                    gint end_hour,
                                    gint end_minute,
                                    EMeetingFreeBusyType busy_type,
                                    const gchar *summary,
                                    const gchar *location)
{
	EMeetingFreeBusyPeriod period;
	gint period_in_days;

	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), FALSE);
	g_return_val_if_fail (busy_type < E_MEETING_FREE_BUSY_LAST, FALSE);
	/* summary may be NULL (optional XFB data)  */
	/* location may be NULL (optional XFB data) */

	/* Check the dates are valid. */
	if (!g_date_valid_dmy (start_day, start_month, start_year))
		return FALSE;
	if (!g_date_valid_dmy (end_day, end_month, end_year))
		return FALSE;
	if (start_hour < 0 || start_hour > 23)
		return FALSE;
	if (end_hour < 0 || end_hour > 23)
		return FALSE;
	if (start_minute < 0 || start_minute > 59)
		return FALSE;
	if (end_minute < 0 || end_minute > 59)
		return FALSE;

	g_date_clear (&period.start.date, 1);
	g_date_clear (&period.end.date, 1);
	g_date_set_dmy (&period.start.date, start_day, start_month, start_year);
	g_date_set_dmy (&period.end.date, end_day, end_month, end_year);
	period.start.hour = start_hour;
	period.start.minute = start_minute;
	period.end.hour = end_hour;
	period.end.minute = end_minute;
	period.busy_type = busy_type;

	/* Check that the start time is before or equal to the end time. */
	if (compare_times (&period.start, &period.end) > 0)
		return FALSE;

	/* If the busy_type is FREE, then there is no need to render it in UI */
	if (busy_type == E_MEETING_FREE_BUSY_FREE)
		goto done;

	/* If the busy range is not set elsewhere, track it as best we can */
	if (!ia->priv->start_busy_range_set) {
		if (!g_date_valid (&ia->priv->busy_periods_start.date)) {
			ia->priv->busy_periods_start.date = period.start.date;
			ia->priv->busy_periods_start.hour = period.start.hour;
			ia->priv->busy_periods_start.minute = period.start.minute;
		} else {
			gint compare;

			compare = g_date_compare (
				&period.start.date,
				&ia->priv->busy_periods_start.date);

			switch (compare) {
			case -1:
				ia->priv->busy_periods_start.date = period.start.date;
				ia->priv->busy_periods_start.hour = period.start.hour;
				ia->priv->busy_periods_start.minute = period.start.minute;
				break;
			case 0:
				if (period.start.hour < ia->priv->busy_periods_start.hour
				    || (period.start.hour == ia->priv->busy_periods_start.hour
					&& period.start.minute < ia->priv->busy_periods_start.minute)) {
					ia->priv->busy_periods_start.date = period.start.date;
					ia->priv->busy_periods_start.hour = period.start.hour;
					ia->priv->busy_periods_start.minute = period.start.minute;
					break;
				}
				break;
			}
		}
	}

	if (!ia->priv->end_busy_range_set) {
		if (!g_date_valid (&ia->priv->busy_periods_end.date)) {
			ia->priv->busy_periods_end.date = period.end.date;
			ia->priv->busy_periods_end.hour = period.end.hour;
			ia->priv->busy_periods_end.minute = period.end.minute;
		} else {
			gint compare;

			compare = g_date_compare (
				&period.end.date,
				&ia->priv->busy_periods_end.date);

			switch (compare) {
			case 0:
				if (period.end.hour > ia->priv->busy_periods_end.hour
				    || (period.end.hour == ia->priv->busy_periods_end.hour
					&& period.end.minute > ia->priv->busy_periods_end.minute)) {
					ia->priv->busy_periods_end.date = period.end.date;
					ia->priv->busy_periods_end.hour = period.end.hour;
					ia->priv->busy_periods_end.minute = period.end.minute;
					break;
				}
				break;
			case 1:
				ia->priv->busy_periods_end.date = period.end.date;
				ia->priv->busy_periods_end.hour = period.end.hour;
				ia->priv->busy_periods_end.minute = period.end.minute;
				break;
			}
		}
	}

	/* Setting of extended free/busy (XFB) data, if we have any. */
	e_meeting_xfb_data_init (&(period.xfb));
	e_meeting_xfb_data_set (&(period.xfb), summary, location);

	g_array_append_val (ia->priv->busy_periods, period);

	period_in_days =
		g_date_get_julian (&period.end.date) -
		g_date_get_julian (&period.start.date) + 1;
	ia->priv->longest_period_in_days =
		MAX (ia->priv->longest_period_in_days, period_in_days);

done:
	ia->priv->has_calendar_info = TRUE;
	ia->priv->busy_periods_sorted = FALSE;

	return TRUE;
}

EMeetingTime
e_meeting_attendee_get_start_busy_range (const EMeetingAttendee *ia)
{
	EMeetingTime mt;

	g_date_clear (&mt.date, 1);
	mt.hour = 0;
	mt.minute = 0;

	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), mt);

	return ia->priv->busy_periods_start;
}

EMeetingTime
e_meeting_attendee_get_end_busy_range (const EMeetingAttendee *ia)
{
	EMeetingTime mt;

	g_date_clear (&mt.date, 1);
	mt.hour = 0;
	mt.minute = 0;

	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), mt);

	return ia->priv->busy_periods_end;
}

gboolean
e_meeting_attendee_set_start_busy_range (EMeetingAttendee *ia,
                                         gint start_year,
                                         gint start_month,
                                         gint start_day,
                                         gint start_hour,
                                         gint start_minute)
{
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), FALSE);

	/* Check the dates are valid. */
	if (!g_date_valid_dmy (start_day, start_month, start_year))
		return FALSE;
	if (start_hour < 0 || start_hour > 23)
		return FALSE;
	if (start_minute < 0 || start_minute > 59)
		return FALSE;

	g_date_clear (&ia->priv->busy_periods_start.date, 1);
	g_date_set_dmy (
		&ia->priv->busy_periods_start.date,
		start_day, start_month, start_year);
	ia->priv->busy_periods_start.hour = start_hour;
	ia->priv->busy_periods_start.minute = start_minute;

	ia->priv->start_busy_range_set = TRUE;

	return TRUE;
}

gboolean
e_meeting_attendee_set_end_busy_range (EMeetingAttendee *ia,
                                       gint end_year,
                                       gint end_month,
                                       gint end_day,
                                       gint end_hour,
                                       gint end_minute)
{
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), FALSE);

	/* Check the dates are valid. */
	if (!g_date_valid_dmy (end_day, end_month, end_year))
		return FALSE;
	if (end_hour < 0 || end_hour > 23)
		return FALSE;
	if (end_minute < 0 || end_minute > 59)
		return FALSE;

	g_date_clear (&ia->priv->busy_periods_end.date, 1);
	g_date_set_dmy (
		&ia->priv->busy_periods_end.date,
		end_day, end_month, end_year);
	ia->priv->busy_periods_end.hour = end_hour;
	ia->priv->busy_periods_end.minute = end_minute;

	ia->priv->end_busy_range_set = TRUE;

	return TRUE;
}

/* Clears all busy times for the given attendee. */
void
e_meeting_attendee_clear_busy_periods (EMeetingAttendee *ia)
{
	g_return_if_fail (E_IS_MEETING_ATTENDEE (ia));

	g_array_set_size (ia->priv->busy_periods, 0);
	ia->priv->busy_periods_sorted = TRUE;

	g_date_clear (&ia->priv->busy_periods_start.date, 1);
	ia->priv->busy_periods_start.hour = 0;
	ia->priv->busy_periods_start.minute = 0;

	g_date_clear (&ia->priv->busy_periods_end.date, 1);
	ia->priv->busy_periods_end.hour = 0;
	ia->priv->busy_periods_end.minute = 0;

	ia->priv->longest_period_in_days = 0;
}
