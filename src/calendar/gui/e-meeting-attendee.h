/*
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
 *		JP Rosevear <jpr@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_MEETING_ATTENDEE_H_
#define _E_MEETING_ATTENDEE_H_

#include <gtk/gtk.h>
#include <libecal/libecal.h>

#include "e-meeting-types.h"

G_BEGIN_DECLS

#define E_TYPE_MEETING_ATTENDEE			(e_meeting_attendee_get_type ())
#define E_MEETING_ATTENDEE(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_MEETING_ATTENDEE, EMeetingAttendee))
#define E_MEETING_ATTENDEE_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_MEETING_ATTENDEE, EMeetingAttendeeClass))
#define E_IS_MEETING_ATTENDEE(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_MEETING_ATTENDEE))
#define E_IS_MEETING_ATTENDEE_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_MEETING_ATTENDEE))

typedef struct _EMeetingAttendee         EMeetingAttendee;
typedef struct _EMeetingAttendeePrivate  EMeetingAttendeePrivate;
typedef struct _EMeetingAttendeeClass    EMeetingAttendeeClass;

/* These specify the type of attendee. Either a person or a resource (e.g. a
 * meeting room). These are used for the Autopick options, where the user can
 * ask for a time when, for example, all people and one resource are free.
 * The default is E_MEETING_ATTENDEE_REQUIRED_PERSON. */
typedef enum {
	E_MEETING_ATTENDEE_REQUIRED_PERSON,
	E_MEETING_ATTENDEE_OPTIONAL_PERSON,
	E_MEETING_ATTENDEE_RESOURCE,
	E_MEETING_ATTENDEE_RESOURCE_UNKNOWN
} EMeetingAttendeeType;

typedef enum
{
	E_MEETING_ATTENDEE_EDIT_FULL,
	E_MEETING_ATTENDEE_EDIT_STATUS,
	E_MEETING_ATTENDEE_EDIT_NONE
} EMeetingAttendeeEditLevel;

struct _EMeetingAttendee {
	GObject parent;

	EMeetingAttendeePrivate *priv;
};

struct _EMeetingAttendeeClass {
	GObjectClass parent_class;

	void (* changed) (EMeetingAttendee *ia);
};

GType		e_meeting_attendee_get_type	(void);
GObject *	e_meeting_attendee_new		(void);
GObject *	e_meeting_attendee_new_from_e_cal_component_attendee
						(const ECalComponentAttendee *ca);

ECalComponentAttendee *
		e_meeting_attendee_as_e_cal_component_attendee
						(const EMeetingAttendee *ia);

const gchar *	e_meeting_attendee_get_address	(const EMeetingAttendee *ia);
void		e_meeting_attendee_set_address	(EMeetingAttendee *ia,
						 const gchar *address);
gboolean	e_meeting_attendee_is_set_address
						(const EMeetingAttendee *ia);
const gchar *	e_meeting_attendee_get_member	(const EMeetingAttendee *ia);
void		e_meeting_attendee_set_member	(EMeetingAttendee *ia,
						 const gchar *member);
gboolean	e_meeting_attendee_is_set_member(const EMeetingAttendee *ia);
ICalParameterCutype
		e_meeting_attendee_get_cutype	(const EMeetingAttendee *ia);
void		e_meeting_attendee_set_cutype	(EMeetingAttendee *ia,
						 ICalParameterCutype cutype);
ICalParameterRole
		e_meeting_attendee_get_role	(const EMeetingAttendee *ia);
void		e_meeting_attendee_set_role	(EMeetingAttendee *ia,
						 ICalParameterRole role);
gboolean	e_meeting_attendee_get_rsvp	(const EMeetingAttendee *ia);
void		e_meeting_attendee_set_rsvp	(EMeetingAttendee *ia,
						 gboolean rsvp);
const gchar *	e_meeting_attendee_get_delto	(const EMeetingAttendee *ia);
void		e_meeting_attendee_set_delto	(EMeetingAttendee *ia,
						 const gchar *delto);
gboolean	e_meeting_attendee_is_set_delto	(const EMeetingAttendee *ia);
const gchar *	e_meeting_attendee_get_delfrom	(const EMeetingAttendee *ia);
void		e_meeting_attendee_set_delfrom	(EMeetingAttendee *ia,
						 const gchar *delfrom);
gboolean	e_meeting_attendee_is_set_delfrom
						(const EMeetingAttendee *ia);
ICalParameterPartstat
		e_meeting_attendee_get_partstat	(const EMeetingAttendee *ia);
void		e_meeting_attendee_set_partstat	(EMeetingAttendee *ia,
						 ICalParameterPartstat partstat);
const gchar *	e_meeting_attendee_get_sentby	(const EMeetingAttendee *ia);
void		e_meeting_attendee_set_sentby	(EMeetingAttendee *ia,
						 const gchar *sentby);
gboolean	e_meeting_attendee_is_set_sentby(const EMeetingAttendee *ia);
const gchar *	e_meeting_attendee_get_cn	(const EMeetingAttendee *ia);
void		e_meeting_attendee_set_cn	(EMeetingAttendee *ia,
						 const gchar *cn);
gboolean	e_meeting_attendee_is_set_cn	(const EMeetingAttendee *ia);
const gchar *	e_meeting_attendee_get_language	(const EMeetingAttendee *ia);
void		e_meeting_attendee_set_language	(EMeetingAttendee *ia,
						 const gchar *language);
gboolean	e_meeting_attendee_is_set_language
						(const EMeetingAttendee *ia);
ECalComponentParameterBag *
		e_meeting_attendee_get_parameter_bag
						(const EMeetingAttendee *ia);
EMeetingAttendeeType
		e_meeting_attendee_get_atype	(const EMeetingAttendee *ia);
EMeetingAttendeeEditLevel
		e_meeting_attendee_get_edit_level
						(const EMeetingAttendee *ia);
void		e_meeting_attendee_set_edit_level
						(EMeetingAttendee *ia,
						 EMeetingAttendeeEditLevel level);
gboolean	e_meeting_attendee_get_show_address
						(const EMeetingAttendee *ia);
void		e_meeting_attendee_set_show_address
						(EMeetingAttendee *ia,
						 gboolean show_address);
gboolean	e_meeting_attendee_get_has_calendar_info
						(const EMeetingAttendee *ia);
void		e_meeting_attendee_set_has_calendar_info
						(EMeetingAttendee *ia,
						 gboolean has_calendar_info);
const gchar *	e_meeting_attendee_get_fburi	(const EMeetingAttendee *ia);
void		e_meeting_attendee_set_fburi	(EMeetingAttendee *ia,
						 const gchar *fburi);
const GArray *	e_meeting_attendee_get_busy_periods
						(EMeetingAttendee *ia);
gint		e_meeting_attendee_find_first_busy_period
						(EMeetingAttendee *ia,
						 const GDate *date);
gboolean	e_meeting_attendee_add_busy_period
						(EMeetingAttendee *ia,
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
						 const gchar *location);
EMeetingTime	e_meeting_attendee_get_start_busy_range
						(const EMeetingAttendee *ia);
EMeetingTime	e_meeting_attendee_get_end_busy_range
						(const EMeetingAttendee *ia);
gboolean	e_meeting_attendee_set_start_busy_range
						(EMeetingAttendee *ia,
						 gint start_year,
						 gint start_month,
						 gint start_day,
						 gint start_hour,
						 gint start_minute);
gboolean	e_meeting_attendee_set_end_busy_range
						(EMeetingAttendee *ia,
						 gint end_year,
						 gint end_month,
						 gint end_day,
						 gint end_hour,
						 gint end_minute);
void		e_meeting_attendee_clear_busy_periods
						(EMeetingAttendee *ia);

G_END_DECLS

#endif /* _E_MEETING_ATTENDEE_H_ */
