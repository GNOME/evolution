/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-meeting_attendee.h
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

#ifndef _E_MEETING_ATTENDEE_H_
#define _E_MEETING_ATTENDEE_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <cal-util/cal-component.h>
#include "e-meeting-types.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_MEETING_ATTENDEE			(e_meeting_attendee_get_type ())
#define E_MEETING_ATTENDEE(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_MEETING_ATTENDEE, EMeetingAttendee))
#define E_MEETING_ATTENDEE_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_MEETING_ATTENDEE, EMeetingAttendeeClass))
#define E_IS_MEETING_ATTENDEE(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_MEETING_ATTENDEE))
#define E_IS_MEETING_ATTENDEE_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_MEETING_ATTENDEE))


typedef struct _EMeetingAttendee         EMeetingAttendee;
typedef struct _EMeetingAttendeePrivate  EMeetingAttendeePrivate;
typedef struct _EMeetingAttendeeClass    EMeetingAttendeeClass;

/* These specify the type of attendee. Either a person or a resource (e.g. a
   meeting room). These are used for the Autopick options, where the user can
   ask for a time when, for example, all people and one resource are free.
   The default is E_MEETING_ATTENDEE_REQUIRED_PERSON. */
typedef enum
{
	E_MEETING_ATTENDEE_REQUIRED_PERSON,
	E_MEETING_ATTENDEE_OPTIONAL_PERSON,
	E_MEETING_ATTENDEE_RESOURCE
} EMeetingAttendeeType;

typedef enum
{
	E_MEETING_ATTENDEE_EDIT_FULL,
	E_MEETING_ATTENDEE_EDIT_STATUS,
	E_MEETING_ATTENDEE_EDIT_NONE
} EMeetingAttendeeEditLevel;

struct _EMeetingAttendee {
	GtkObject parent;

	EMeetingAttendeePrivate *priv;
};

struct _EMeetingAttendeeClass {
	GtkObjectClass parent_class;

	void (* changed) (EMeetingAttendee *ia);
};


GType      e_meeting_attendee_get_type (void);
GObject   *e_meeting_attendee_new      (void);
GObject   *e_meeting_attendee_new_from_cal_component_attendee (CalComponentAttendee *ca);

CalComponentAttendee *e_meeting_attendee_as_cal_component_attendee (EMeetingAttendee *ia);

const gchar *e_meeting_attendee_get_address (EMeetingAttendee *ia);
void e_meeting_attendee_set_address (EMeetingAttendee *ia, gchar *address);
gboolean e_meeting_attendee_is_set_address (EMeetingAttendee *ia);

const gchar *e_meeting_attendee_get_member (EMeetingAttendee *ia);
void e_meeting_attendee_set_member (EMeetingAttendee *ia, gchar *member);
gboolean e_meeting_attendee_is_set_member (EMeetingAttendee *ia);

icalparameter_cutype e_meeting_attendee_get_cutype (EMeetingAttendee *ia);
void e_meeting_attendee_set_cutype (EMeetingAttendee *ia, icalparameter_cutype cutype);

icalparameter_role e_meeting_attendee_get_role (EMeetingAttendee *ia);
void e_meeting_attendee_set_role (EMeetingAttendee *ia, icalparameter_role role);

gboolean e_meeting_attendee_get_rsvp (EMeetingAttendee *ia);
void e_meeting_attendee_set_rsvp (EMeetingAttendee *ia, gboolean rsvp);

const gchar *e_meeting_attendee_get_delto (EMeetingAttendee *ia);
void e_meeting_attendee_set_delto (EMeetingAttendee *ia, gchar *delto);
gboolean e_meeting_attendee_is_set_delto (EMeetingAttendee *ia);

const gchar *e_meeting_attendee_get_delfrom (EMeetingAttendee *ia);
void e_meeting_attendee_set_delfrom (EMeetingAttendee *ia, gchar *delfrom);
gboolean e_meeting_attendee_is_set_delfrom (EMeetingAttendee *ia);

icalparameter_partstat e_meeting_attendee_get_status (EMeetingAttendee *ia);
void e_meeting_attendee_set_status (EMeetingAttendee *ia, icalparameter_partstat status);

const gchar *e_meeting_attendee_get_sentby (EMeetingAttendee *ia);
void e_meeting_attendee_set_sentby (EMeetingAttendee *ia, gchar *sentby);
gboolean e_meeting_attendee_is_set_sentby (EMeetingAttendee *ia);

const gchar *e_meeting_attendee_get_cn (EMeetingAttendee *ia);
void e_meeting_attendee_set_cn (EMeetingAttendee *ia, gchar *cn);
gboolean e_meeting_attendee_is_set_cn (EMeetingAttendee *ia);

const gchar *e_meeting_attendee_get_language (EMeetingAttendee *ia);
void e_meeting_attendee_set_language (EMeetingAttendee *ia, gchar *language);
gboolean e_meeting_attendee_is_set_language (EMeetingAttendee *ia);

EMeetingAttendeeType e_meeting_attendee_get_atype (EMeetingAttendee *ia);

EMeetingAttendeeEditLevel e_meeting_attendee_get_edit_level (EMeetingAttendee *ia);
void e_meeting_attendee_set_edit_level (EMeetingAttendee *ia, EMeetingAttendeeEditLevel level);

gboolean e_meeting_attendee_get_has_calendar_info (EMeetingAttendee *ia);
void e_meeting_attendee_set_has_calendar_info (EMeetingAttendee *ia, gboolean has_calendar_info);

const GArray *e_meeting_attendee_get_busy_periods (EMeetingAttendee *ia);
gint e_meeting_attendee_find_first_busy_period (EMeetingAttendee *ia, GDate *date);
gboolean e_meeting_attendee_add_busy_period (EMeetingAttendee *ia, 
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
					EMeetingFreeBusyType busy_type);

EMeetingTime e_meeting_attendee_get_start_busy_range (EMeetingAttendee *ia);
EMeetingTime e_meeting_attendee_get_end_busy_range (EMeetingAttendee *ia);

gboolean e_meeting_attendee_set_start_busy_range (EMeetingAttendee *ia,
						  gint start_year,
						  gint start_month,
						  gint start_day,
						  gint start_hour,
						  gint start_minute);
gboolean e_meeting_attendee_set_end_busy_range (EMeetingAttendee *ia,
						gint end_year,
						gint end_month,
						gint end_day,
						gint end_hour,
						gint end_minute);

void e_meeting_attendee_clear_busy_periods (EMeetingAttendee *ia);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_MEETING_ATTENDEE_H_ */
