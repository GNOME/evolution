/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-attendee.c
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: JP Rosevear
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <gtk/gtk.h>
#include "e-meeting-attendee.h"

struct _EMeetingAttendeePrivate {
	gchar *address;
	gchar *member;

	icalparameter_cutype cutype;
	icalparameter_role role;

	gboolean rsvp;

	gchar *delto;
	gchar *delfrom;

	icalparameter_partstat status;
	
	gchar *sentby;
	gchar *cn;
	gchar *language;

	EMeetingAttendeeEditLevel edit_level;

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

G_DEFINE_TYPE (EMeetingAttendee, e_meeting_attendee, G_TYPE_OBJECT);

static void
e_meeting_attendee_class_init (EMeetingAttendeeClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	signals[CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EMeetingAttendeeClass, changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	object_class->finalize = e_meeting_attendee_finalize;
}

static gchar *
string_test (gchar *string)
{
	return string != NULL ? string : g_strdup ("");
}

static gboolean
string_is_set (gchar *string) 
{
	if (string != NULL && *string != '\0')
		return TRUE;
	
	return FALSE;
}

static void
notify_changed (EMeetingAttendee *ia) 
{
	g_signal_emit_by_name (G_OBJECT (ia), "changed");
}

static void
e_meeting_attendee_init (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;

	priv = g_new0 (EMeetingAttendeePrivate, 1);

	ia->priv = priv;

	priv->address = string_test (NULL);
	priv->member = string_test (NULL);

	priv->cutype = ICAL_CUTYPE_NONE;
	priv->role = ICAL_ROLE_NONE;

	priv->rsvp = FALSE;

	priv->delto = string_test (NULL);
	priv->delfrom = string_test (NULL);
	
	priv->status = ICAL_PARTSTAT_NONE;
	
	priv->sentby = string_test (NULL);
	priv->cn = string_test (NULL);
	priv->language = string_test (NULL);

	priv->edit_level = E_MEETING_ATTENDEE_EDIT_FULL;
	priv->has_calendar_info = FALSE;
	
	priv->busy_periods = g_array_new (FALSE, FALSE, sizeof (EMeetingFreeBusyPeriod));
	priv->busy_periods_sorted = FALSE;

	g_date_clear (&priv->busy_periods_start.date, 1);
	priv->busy_periods_start.hour = 0;
	priv->busy_periods_start.minute = 0;

	g_date_clear (&priv->busy_periods_end.date, 1);
	priv->busy_periods_end.hour = 0;
	priv->busy_periods_end.minute = 0;

	priv->start_busy_range_set = FALSE;
	priv->end_busy_range_set = FALSE;
	
	priv->longest_period_in_days = 0;
}


static void
e_meeting_attendee_finalize (GObject *obj)
{
	EMeetingAttendee *ia = E_MEETING_ATTENDEE (obj);
	EMeetingAttendeePrivate *priv;

	priv = ia->priv;

	g_free (priv->address);
	g_free (priv->member);

	g_free (priv->delto);
	g_free (priv->delfrom);

	g_free (priv->sentby);
	g_free (priv->cn);
	g_free (priv->language);
	
	g_array_free (priv->busy_periods, TRUE);
	
	g_free (priv);

	if (G_OBJECT_CLASS (e_meeting_attendee_parent_class)->finalize)
		(* G_OBJECT_CLASS (e_meeting_attendee_parent_class)->finalize) (obj);
}

GObject *
e_meeting_attendee_new (void)
{
	return g_object_new (E_TYPE_MEETING_ATTENDEE, NULL);
}

GObject *
e_meeting_attendee_new_from_e_cal_component_attendee (ECalComponentAttendee *ca)
{
	EMeetingAttendee *ia;
	
	ia = E_MEETING_ATTENDEE (g_object_new (E_TYPE_MEETING_ATTENDEE, NULL));

	e_meeting_attendee_set_address (ia, g_strdup (ca->value));
	e_meeting_attendee_set_member (ia, g_strdup (ca->member));
	e_meeting_attendee_set_cutype (ia, ca->cutype);
	e_meeting_attendee_set_role (ia, ca->role);
	e_meeting_attendee_set_status (ia, ca->status);
	e_meeting_attendee_set_rsvp (ia, ca->rsvp);
	e_meeting_attendee_set_delto (ia, g_strdup (ca->delto));
	e_meeting_attendee_set_delfrom (ia, g_strdup (ca->delfrom));
	e_meeting_attendee_set_sentby (ia, g_strdup (ca->sentby));
	e_meeting_attendee_set_cn (ia, g_strdup (ca->cn));
	e_meeting_attendee_set_language (ia, g_strdup (ca->language));
	
	return G_OBJECT (ia);
}

ECalComponentAttendee *
e_meeting_attendee_as_e_cal_component_attendee (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;
	ECalComponentAttendee *ca;

	priv = ia->priv;
	
	ca = g_new0 (ECalComponentAttendee, 1);

	ca->value = priv->address;
	ca->member = string_is_set (priv->member) ? priv->member : NULL;
	ca->cutype= priv->cutype;
	ca->role = priv->role;
	ca->status = priv->status;
	ca->rsvp = priv->rsvp;
	ca->delto = string_is_set (priv->delto) ? priv->delto : NULL;
	ca->delfrom = string_is_set (priv->delfrom) ? priv->delfrom : NULL;
	ca->sentby = string_is_set (priv->sentby) ? priv->sentby : NULL;
	ca->cn = string_is_set (priv->cn) ? priv->cn : NULL;
	ca->language = string_is_set (priv->language) ? priv->language : NULL;

	return ca;
}


const gchar *
e_meeting_attendee_get_address (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;
	
	return priv->address;
}

void
e_meeting_attendee_set_address (EMeetingAttendee *ia, gchar *address)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;
	
	if (priv->address != NULL)
		g_free (priv->address);
	
	priv->address = string_test (address);

	notify_changed (ia);
}

gboolean
e_meeting_attendee_is_set_address (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;
	
	return string_is_set (priv->address);
}

const gchar *
e_meeting_attendee_get_member (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;
	
	return priv->member;
}

void
e_meeting_attendee_set_member (EMeetingAttendee *ia, gchar *member)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;
	
	if (priv->member != NULL)
		g_free (priv->member);
	
	priv->member = string_test (member);

	notify_changed (ia);
}

gboolean
e_meeting_attendee_is_set_member (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;

	return string_is_set (priv->member);
}

icalparameter_cutype
e_meeting_attendee_get_cutype (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;
	
	return priv->cutype;
}

void 
e_meeting_attendee_set_cutype (EMeetingAttendee *ia, icalparameter_cutype cutype)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;
	
	priv->cutype = cutype;

	notify_changed (ia);
}

icalparameter_role
e_meeting_attendee_get_role (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;
	
	return priv->role;
}

void
e_meeting_attendee_set_role (EMeetingAttendee *ia, icalparameter_role role)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;
	
	priv->role = role;

	notify_changed (ia);
}

gboolean 
e_meeting_attendee_get_rsvp (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;
	
	return priv->rsvp;
}

void
e_meeting_attendee_set_rsvp (EMeetingAttendee *ia, gboolean rsvp)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;
	
	priv->rsvp = rsvp;

	notify_changed (ia);
}

const gchar *
e_meeting_attendee_get_delto (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;
	
	return priv->delto;
}

void
e_meeting_attendee_set_delto (EMeetingAttendee *ia, gchar *delto)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;
	
	if (priv->delto != NULL)
		g_free (priv->delto);
	
	priv->delto = string_test (delto);

	notify_changed (ia);
}

gboolean
e_meeting_attendee_is_set_delto (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;

	return string_is_set (priv->delto);
}

const gchar *
e_meeting_attendee_get_delfrom (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;
	
	return priv->delfrom;
}

void
e_meeting_attendee_set_delfrom (EMeetingAttendee *ia, gchar *delfrom)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;
	
	if (priv->delfrom != NULL)
		g_free (priv->delfrom);
	
	priv->delfrom = string_test (delfrom);

	notify_changed (ia);
}

gboolean
e_meeting_attendee_is_set_delfrom (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;

	return string_is_set (priv->delfrom);
}

icalparameter_partstat
e_meeting_attendee_get_status (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;
	
	return priv->status;
}

void
e_meeting_attendee_set_status (EMeetingAttendee *ia, icalparameter_partstat status)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;
	
	priv->status = status;

	notify_changed (ia);
}

const gchar *
e_meeting_attendee_get_sentby (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;
	
	return priv->sentby;
}

void
e_meeting_attendee_set_sentby (EMeetingAttendee *ia, gchar *sentby)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;
	
	if (priv->sentby != NULL)
		g_free (priv->sentby);
	
	priv->sentby = string_test (sentby);

	notify_changed (ia);
}

gboolean
e_meeting_attendee_is_set_sentby (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;

	return string_is_set (priv->sentby);
}

const gchar *
e_meeting_attendee_get_cn (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;
	
	return priv->cn;
}

void
e_meeting_attendee_set_cn (EMeetingAttendee *ia, gchar *cn)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;
	
	if (priv->cn != NULL)
		g_free (priv->cn);
	
	priv->cn = string_test (cn);

	notify_changed (ia);
}

gboolean
e_meeting_attendee_is_set_cn (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;

	return string_is_set (priv->cn);
}

const gchar *
e_meeting_attendee_get_language (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;
	
	return priv->language;
}

void
e_meeting_attendee_set_language (EMeetingAttendee *ia, gchar *language)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;
	
	if (priv->language != NULL)
		g_free (priv->language);
	
	priv->language = string_test (language);

	notify_changed (ia);
}

gboolean
e_meeting_attendee_is_set_language (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;

	return string_is_set (priv->language);
}

EMeetingAttendeeType
e_meeting_attendee_get_atype (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;

	if (priv->cutype == ICAL_CUTYPE_ROOM
	    || priv->cutype == ICAL_CUTYPE_RESOURCE)
		return E_MEETING_ATTENDEE_RESOURCE;

	if (priv->role == ICAL_ROLE_CHAIR
	    || priv->role == ICAL_ROLE_REQPARTICIPANT)
		return E_MEETING_ATTENDEE_REQUIRED_PERSON;
	
	return E_MEETING_ATTENDEE_OPTIONAL_PERSON;
}


EMeetingAttendeeEditLevel
e_meeting_attendee_get_edit_level (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;
	
	g_return_val_if_fail (ia != NULL, E_MEETING_ATTENDEE_EDIT_NONE);
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), E_MEETING_ATTENDEE_EDIT_NONE);

	priv = ia->priv;

	return priv->edit_level;
}

void 
e_meeting_attendee_set_edit_level (EMeetingAttendee *ia, EMeetingAttendeeEditLevel level)
{
	EMeetingAttendeePrivate *priv;
	
	g_return_if_fail (ia != NULL);
	g_return_if_fail (E_IS_MEETING_ATTENDEE (ia));

	priv = ia->priv;

	priv->edit_level = level;
}


static gint
compare_times (EMeetingTime *time1,
	       EMeetingTime *time2)
{
	gint day_comparison;

	day_comparison = g_date_compare (&time1->date,
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
compare_period_starts (const void *arg1,
		       const void *arg2)
{
	EMeetingFreeBusyPeriod *period1, *period2;

	period1 = (EMeetingFreeBusyPeriod *) arg1;
	period2 = (EMeetingFreeBusyPeriod *) arg2;

	return compare_times (&period1->start, &period2->start);
}

static void
ensure_periods_sorted (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;

	if (priv->busy_periods_sorted)
		return;

	qsort (priv->busy_periods->data, priv->busy_periods->len,
	       sizeof (EMeetingFreeBusyPeriod),
	       compare_period_starts);

	priv->busy_periods_sorted = TRUE;
}

gboolean
e_meeting_attendee_get_has_calendar_info (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;

	return priv->has_calendar_info;
}

void
e_meeting_attendee_set_has_calendar_info (EMeetingAttendee *ia, gboolean has_calendar_info)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;

	priv->has_calendar_info = has_calendar_info;
}

const GArray *
e_meeting_attendee_get_busy_periods (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;

	ensure_periods_sorted (ia);
	
	return priv->busy_periods;	
}

gint
e_meeting_attendee_find_first_busy_period (EMeetingAttendee *ia, GDate *date)
{
	EMeetingAttendeePrivate *priv;
	EMeetingFreeBusyPeriod *period;
	gint lower, upper, middle = 0, cmp = 0;
	GDate tmp_date;

	priv = ia->priv;
	
	/* Make sure the busy periods have been sorted. */
	ensure_periods_sorted (ia);

	/* Calculate the first day which could have a busy period which
	   continues onto our given date. */
	tmp_date = *date;
	g_date_subtract_days (&tmp_date, priv->longest_period_in_days);

	/* We want the first busy period which starts on tmp_date. */
	lower = 0;
	upper = priv->busy_periods->len;

	if (upper == 0)
		return -1;

	while (lower < upper) {
		middle = (lower + upper) >> 1;
	  
		period = &g_array_index (priv->busy_periods,
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
	   backwards to the first one. */
	if (cmp == 0) {
		while (middle > 0) {
			period = &g_array_index (priv->busy_periods,
						 EMeetingFreeBusyPeriod, middle - 1);
			if (g_date_compare (&tmp_date, &period->start.date) != 0)
				break;
			middle--;
		}
	} else if (cmp > 0) {
		/* This means we couldn't find a period on the given day, and
		   the last one we looked at was before it, so if there are
		   any more periods after this one we return it. */
		middle++;
		if (priv->busy_periods->len <= middle)
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
				    EMeetingFreeBusyType busy_type)
{
	EMeetingAttendeePrivate *priv;
	EMeetingFreeBusyPeriod period;
	gint period_in_days;

	g_return_val_if_fail (ia != NULL, FALSE);
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), FALSE);
	g_return_val_if_fail (busy_type >= 0, FALSE);
	g_return_val_if_fail (busy_type < E_MEETING_FREE_BUSY_LAST, FALSE);

	priv = ia->priv;
	
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

	/* If the busy range is not set elsewhere, track it as best we can */
	if (!priv->start_busy_range_set) {
		if (!g_date_valid (&priv->busy_periods_start.date)) {
			priv->busy_periods_start.date = period.start.date;
			priv->busy_periods_start.hour = period.start.hour;
			priv->busy_periods_start.minute = period.start.minute;			
		} else {
			switch (g_date_compare (&period.start.date, &priv->busy_periods_start.date)) {
			case -1:
				priv->busy_periods_start.date = period.start.date;
				priv->busy_periods_start.hour = period.start.hour;
				priv->busy_periods_start.minute = period.start.minute;			
				break;
			case 0:
				if (period.start.hour < priv->busy_periods_start.hour
				    || (period.start.hour == priv->busy_periods_start.hour
					&& period.start.minute < priv->busy_periods_start.minute)) {
					priv->busy_periods_start.date = period.start.date;
					priv->busy_periods_start.hour = period.start.hour;
					priv->busy_periods_start.minute = period.start.minute;			
					break;
				}
				break;
			}
		}
	}
	if (!priv->end_busy_range_set) {
		if (!g_date_valid (&priv->busy_periods_end.date)) {
			priv->busy_periods_end.date = period.end.date;
			priv->busy_periods_end.hour = period.end.hour;
			priv->busy_periods_end.minute = period.end.minute;			
		} else {
			switch (g_date_compare (&period.end.date, &priv->busy_periods_end.date)) {
			case 0:
				if (period.end.hour > priv->busy_periods_end.hour
				    || (period.end.hour == priv->busy_periods_end.hour
					&& period.end.minute > priv->busy_periods_end.minute)) {
					priv->busy_periods_end.date = period.end.date;
					priv->busy_periods_end.hour = period.end.hour;
					priv->busy_periods_end.minute = period.end.minute;			
					break;
				}
				break;
			case 1:
				priv->busy_periods_end.date = period.end.date;
				priv->busy_periods_end.hour = period.end.hour;
				priv->busy_periods_end.minute = period.end.minute;			
				break;
			}
		}
	}
	
	g_array_append_val (priv->busy_periods, period);
	priv->has_calendar_info = TRUE;
	priv->busy_periods_sorted = FALSE;

	period_in_days = g_date_julian (&period.end.date) - g_date_julian (&period.start.date) + 1;
	priv->longest_period_in_days = MAX (priv->longest_period_in_days, period_in_days);

	return TRUE;
}

EMeetingTime
e_meeting_attendee_get_start_busy_range (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;

	return priv->busy_periods_start;
}

EMeetingTime
e_meeting_attendee_get_end_busy_range (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;
	
	priv = ia->priv;
	
	return priv->busy_periods_end;
}

gboolean
e_meeting_attendee_set_start_busy_range (EMeetingAttendee *ia,
					 gint start_year,
					 gint start_month,
					 gint start_day,
					 gint start_hour,
					 gint start_minute)
{
	EMeetingAttendeePrivate *priv;
	
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), FALSE);

	priv = ia->priv;
	
	/* Check the dates are valid. */
	if (!g_date_valid_dmy (start_day, start_month, start_year))
		return FALSE;
	if (start_hour < 0 || start_hour > 23)
		return FALSE;
	if (start_minute < 0 || start_minute > 59)
		return FALSE;

	g_date_clear (&priv->busy_periods_start.date, 1);
	g_date_set_dmy (&priv->busy_periods_start.date,
			start_day, start_month, start_year);
	priv->busy_periods_start.hour = start_hour;
	priv->busy_periods_start.minute = start_minute;

	priv->start_busy_range_set = TRUE;
	
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
	EMeetingAttendeePrivate *priv;
	
	g_return_val_if_fail (E_IS_MEETING_ATTENDEE (ia), FALSE);

	priv = ia->priv;
	
	/* Check the dates are valid. */
	if (!g_date_valid_dmy (end_day, end_month, end_year))
		return FALSE;
	if (end_hour < 0 || end_hour > 23)
		return FALSE;
	if (end_minute < 0 || end_minute > 59)
		return FALSE;

	g_date_clear (&priv->busy_periods_end.date, 1);
	g_date_set_dmy (&priv->busy_periods_end.date,
			end_day, end_month, end_year);
	priv->busy_periods_end.hour = end_hour;
	priv->busy_periods_end.minute = end_minute;

	priv->end_busy_range_set = TRUE;
	
	return TRUE;
}

/* Clears all busy times for the given attendee. */
void
e_meeting_attendee_clear_busy_periods (EMeetingAttendee *ia)
{
	EMeetingAttendeePrivate *priv;

	g_return_if_fail (E_IS_MEETING_ATTENDEE (ia));

	priv = ia->priv;
	
	g_array_set_size (priv->busy_periods, 0);
	priv->busy_periods_sorted = TRUE;

	g_date_clear (&priv->busy_periods_start.date, 1);
	priv->busy_periods_start.hour = 0;
	priv->busy_periods_start.minute = 0;

	g_date_clear (&priv->busy_periods_end.date, 1);
	priv->busy_periods_end.hour = 0;
	priv->busy_periods_end.minute = 0;

	priv->longest_period_in_days = 0;
}
