/* Evolution calendar - iCalendar component object
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef CAL_COMPONENT_H
#define CAL_COMPONENT_H

#include <libgnome/gnome-defs.h>
#include <time.h>
#include <gtk/gtkobject.h>
#include <ical.h>

BEGIN_GNOME_DECLS



#define CAL_COMPONENT_TYPE            (cal_component_get_type ())
#define CAL_COMPONENT(obj)            (GTK_CHECK_CAST ((obj), CAL_COMPONENT_TYPE, CalComponent))
#define CAL_COMPONENT_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), CAL_COMPONENT_TYPE,	\
				       CalComponentClass))
#define IS_CAL_COMPONENT(obj)         (GTK_CHECK_TYPE ((obj), CAL_COMPONENT_TYPE))
#define IS_CAL_COMPONENT_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), CAL_COMPONENT_TYPE))

/* Types of calendar components to be stored by a CalComponent, as per RFC 2445.
 * We don't put the alarm component type here since we store alarms as separate
 * structures inside the other "real" components.
 */
typedef enum {
	CAL_COMPONENT_NO_TYPE,
	CAL_COMPONENT_EVENT,
	CAL_COMPONENT_TODO,
	CAL_COMPONENT_JOURNAL,
	CAL_COMPONENT_FREEBUSY,
	CAL_COMPONENT_TIMEZONE
} CalComponentVType;

/* Field identifiers for a calendar component; these are used by the data model
 * for ETable.
 *
 * NOTE: These are also used in the ETable specification, and the column
 *       numbers are saved in the user settings file. So don't reorder them!
 */
typedef enum {
	CAL_COMPONENT_FIELD_CATEGORIES,		/* concatenation of the categories list */
	CAL_COMPONENT_FIELD_CLASSIFICATION,
	CAL_COMPONENT_FIELD_COMPLETED,
	CAL_COMPONENT_FIELD_DTEND,
	CAL_COMPONENT_FIELD_DTSTART,
	CAL_COMPONENT_FIELD_DUE,
	CAL_COMPONENT_FIELD_GEO,
	CAL_COMPONENT_FIELD_PERCENT,
	CAL_COMPONENT_FIELD_PRIORITY,
	CAL_COMPONENT_FIELD_SUMMARY,
	CAL_COMPONENT_FIELD_TRANSPARENCY,
	CAL_COMPONENT_FIELD_URL,
	CAL_COMPONENT_FIELD_HAS_ALARMS,		/* not a real field */
	CAL_COMPONENT_FIELD_ICON,		/* not a real field */
	CAL_COMPONENT_FIELD_COMPLETE,		/* not a real field */
	CAL_COMPONENT_FIELD_RECURRING,		/* not a real field */
	CAL_COMPONENT_FIELD_OVERDUE,		/* not a real field */
	CAL_COMPONENT_FIELD_COLOR,		/* not a real field */
	CAL_COMPONENT_FIELD_STATUS,
	CAL_COMPONENT_FIELD_COMPONENT,		/* not a real field */
#if 0
	CAL_COMPONENT_FIELD_LOCATION,
#endif
	CAL_COMPONENT_FIELD_NUM_FIELDS
} CalComponentField;

/* Structures and enumerations to return properties and their parameters */

/* CLASSIFICATION property */
typedef enum {
	CAL_COMPONENT_CLASS_NONE,
	CAL_COMPONENT_CLASS_PUBLIC,
	CAL_COMPONENT_CLASS_PRIVATE,
	CAL_COMPONENT_CLASS_CONFIDENTIAL,
	CAL_COMPONENT_CLASS_UNKNOWN
} CalComponentClassification;

/* Properties that have time and timezone information */
typedef struct {
	/* Actual date/time value */
	struct icaltimetype *value;

	/* Timezone ID */
	const char *tzid;
} CalComponentDateTime;

/* Way in which a period of time is specified */
typedef enum {
	CAL_COMPONENT_PERIOD_DATETIME,
	CAL_COMPONENT_PERIOD_DURATION
} CalComponentPeriodType;

/* Period of time, can have explicit start/end times or start/duration instead */
typedef struct {
	CalComponentPeriodType type;

	struct icaltimetype start;

	union {
		struct icaltimetype end;
		struct icaldurationtype duration;
	} u;
} CalComponentPeriod;

/* The type of range */
typedef enum {
	CAL_COMPONENT_RANGE_SINGLE,
	CAL_COMPONENT_RANGE_THISPRIOR,
	CAL_COMPONENT_RANGE_THISFUTURE,
} CalComponentRangeType;

typedef struct {
	CalComponentRangeType type;
	
	CalComponentDateTime datetime;
} CalComponentRange;

/* Text properties */
typedef struct {
	/* Description string */
	const char *value;

	/* Alternate representation URI */
	const char *altrep;
} CalComponentText;

/* Time transparency */
typedef enum {
	CAL_COMPONENT_TRANSP_NONE,
	CAL_COMPONENT_TRANSP_TRANSPARENT,
	CAL_COMPONENT_TRANSP_OPAQUE,
	CAL_COMPONENT_TRANSP_UNKNOWN
} CalComponentTransparency;

/* Organizer & Attendee */	
typedef struct {
	const char *value;
	
	const char *member;
	icalparameter_cutype cutype;
	icalparameter_role role;
	icalparameter_partstat status;
	gboolean rsvp;
	
	const char *delto;
	const char *delfrom;
	const char *sentby;
	const char *cn;
	const char *language;
} CalComponentAttendee;
	
typedef struct {
	const char *value;
	const char *sentby;
	const char *cn;
	const char *language;
} CalComponentOrganizer;

/* Main calendar component object */

typedef struct _CalComponent CalComponent;
typedef struct _CalComponentClass CalComponentClass;

typedef struct _CalComponentPrivate CalComponentPrivate;

struct _CalComponent {
	GtkObject object;

	/* Private data */
	CalComponentPrivate *priv;
};

struct _CalComponentClass {
	GtkObjectClass parent_class;
};

/* Calendar component */

GtkType cal_component_get_type (void);

char *cal_component_gen_uid (void);

CalComponent *cal_component_new (void);

CalComponent *cal_component_clone (CalComponent *comp);

void cal_component_set_new_vtype (CalComponent *comp, CalComponentVType type);

gboolean cal_component_set_icalcomponent (CalComponent *comp, icalcomponent *icalcomp);
icalcomponent *cal_component_get_icalcomponent (CalComponent *comp);
void cal_component_rescan (CalComponent *comp);
void cal_component_strip_errors (CalComponent *comp);

CalComponentVType cal_component_get_vtype (CalComponent *comp);

char *cal_component_get_as_string (CalComponent *comp);

void cal_component_commit_sequence (CalComponent *comp);
void cal_component_abort_sequence (CalComponent *comp);

void cal_component_get_uid (CalComponent *comp, const char **uid);
void cal_component_set_uid (CalComponent *comp, const char *uid);

void cal_component_get_categories (CalComponent *comp, const char **categories);
void cal_component_set_categories (CalComponent *comp, const char *categories);
void cal_component_get_categories_list (CalComponent *comp, GSList **categ_list);
void cal_component_set_categories_list (CalComponent *comp, GSList *categ_list);

void cal_component_get_classification (CalComponent *comp, CalComponentClassification *classif);
void cal_component_set_classification (CalComponent *comp, CalComponentClassification classif);

void cal_component_get_comment_list (CalComponent *comp, GSList **text_list);
void cal_component_set_comment_list (CalComponent *comp, GSList *text_list);

void cal_component_get_completed (CalComponent *comp, struct icaltimetype **t);
void cal_component_set_completed (CalComponent *comp, struct icaltimetype *t);

void cal_component_get_contact_list (CalComponent *comp, GSList **text_list);
void cal_component_set_contact_list (CalComponent *comp, GSList *text_list);

void cal_component_get_created (CalComponent *comp, struct icaltimetype **t);
void cal_component_set_created (CalComponent *comp, struct icaltimetype *t);

void cal_component_get_description_list (CalComponent *comp, GSList **text_list);
void cal_component_set_description_list (CalComponent *comp, GSList *text_list);

void cal_component_get_dtend (CalComponent *comp, CalComponentDateTime *dt);
void cal_component_set_dtend (CalComponent *comp, CalComponentDateTime *dt);

void cal_component_get_dtstamp (CalComponent *comp, struct icaltimetype *t);
void cal_component_set_dtstamp (CalComponent *comp, struct icaltimetype *t);

void cal_component_get_dtstart (CalComponent *comp, CalComponentDateTime *dt);
void cal_component_set_dtstart (CalComponent *comp, CalComponentDateTime *dt);

void cal_component_get_due (CalComponent *comp, CalComponentDateTime *dt);
void cal_component_set_due (CalComponent *comp, CalComponentDateTime *dt);

void cal_component_get_exdate_list (CalComponent *comp, GSList **exdate_list);
void cal_component_set_exdate_list (CalComponent *comp, GSList *exdate_list);
gboolean cal_component_has_exdates (CalComponent *comp);

void cal_component_get_exrule_list (CalComponent *comp, GSList **recur_list);
void cal_component_get_exrule_property_list (CalComponent *comp, GSList **recur_list);
void cal_component_set_exrule_list (CalComponent *comp, GSList *recur_list);
gboolean cal_component_has_exrules (CalComponent *comp);

gboolean cal_component_has_exceptions (CalComponent *comp);

void cal_component_get_geo (CalComponent *comp, struct icalgeotype **geo);
void cal_component_set_geo (CalComponent *comp, struct icalgeotype *geo);

void cal_component_get_last_modified (CalComponent *comp, struct icaltimetype **t);
void cal_component_set_last_modified (CalComponent *comp, struct icaltimetype *t);

void cal_component_get_organizer (CalComponent *comp, CalComponentOrganizer *organizer);
void cal_component_set_organizer (CalComponent *comp, CalComponentOrganizer *organizer);
gboolean cal_component_has_organizer (CalComponent *comp);

void cal_component_get_percent (CalComponent *comp, int **percent);
void cal_component_set_percent (CalComponent *comp, int *percent);

void cal_component_get_priority (CalComponent *comp, int **priority);
void cal_component_set_priority (CalComponent *comp, int *priority);

void cal_component_get_recurid (CalComponent *comp, CalComponentRange *recur_id);
void cal_component_set_recurid (CalComponent *comp, CalComponentRange *recur_id);

void cal_component_get_rdate_list (CalComponent *comp, GSList **period_list);
void cal_component_set_rdate_list (CalComponent *comp, GSList *period_list);
gboolean cal_component_has_rdates (CalComponent *comp);

void cal_component_get_rrule_list (CalComponent *comp, GSList **recur_list);
void cal_component_get_rrule_property_list (CalComponent *comp, GSList **recur_list);
void cal_component_set_rrule_list (CalComponent *comp, GSList *recur_list);
gboolean cal_component_has_rrules (CalComponent *comp);

gboolean cal_component_has_recurrences (CalComponent *comp);
gboolean cal_component_has_simple_recurrence (CalComponent *comp);
gboolean cal_component_is_instance (CalComponent *comp);

void cal_component_get_sequence (CalComponent *comp, int **sequence);
void cal_component_set_sequence (CalComponent *comp, int *sequence);

void cal_component_get_status (CalComponent *comp, icalproperty_status *status);
void cal_component_set_status (CalComponent *comp, icalproperty_status status);

void cal_component_get_summary (CalComponent *comp, CalComponentText *summary);
void cal_component_set_summary (CalComponent *comp, CalComponentText *summary);

void cal_component_get_transparency (CalComponent *comp, CalComponentTransparency *transp);
void cal_component_set_transparency (CalComponent *comp, CalComponentTransparency transp);

void cal_component_get_url (CalComponent *comp, const char **url);
void cal_component_set_url (CalComponent *comp, const char *url);

void cal_component_get_attendee_list (CalComponent *comp, GSList **attendee_list);
void cal_component_set_attendee_list (CalComponent *comp, GSList *attendee_list);
gboolean cal_component_has_attendees (CalComponent *comp);

void cal_component_get_location (CalComponent *comp, const char **location);
void cal_component_set_location (CalComponent *comp, const char *location);

gboolean cal_component_event_dates_match (CalComponent *comp1, CalComponent *comp2);


/* Functions to free returned values */

void cal_component_free_categories_list (GSList *categ_list);
void cal_component_free_datetime (CalComponentDateTime *dt);
void cal_component_free_range (CalComponentRange *range);
void cal_component_free_exdate_list (GSList *exdate_list);
void cal_component_free_geo (struct icalgeotype *geo);
void cal_component_free_icaltimetype (struct icaltimetype *t);
void cal_component_free_percent (int *percent);
void cal_component_free_priority (int *priority);
void cal_component_free_period_list (GSList *period_list);
void cal_component_free_recur_list (GSList *recur_list);
void cal_component_free_sequence (int *sequence);
void cal_component_free_text_list (GSList *text_list);
void cal_component_free_attendee_list (GSList *attendee_list);

/* Alarms */

/* Opaque structure used to represent alarm subcomponents */
typedef struct _CalComponentAlarm CalComponentAlarm;

/* An alarm occurrence, i.e. a trigger instance */
typedef struct {
	/* UID of the alarm that triggered */
	const char *auid;

	/* Trigger time, i.e. "5 minutes before the appointment" */
	time_t trigger;

	/* Actual event occurrence to which this trigger corresponds */
	time_t occur_start;
	time_t occur_end;
} CalAlarmInstance;

/* Alarm trigger instances for a particular component */
typedef struct {
	/* The actual component */
	CalComponent *comp;

	/* List of CalAlarmInstance structures */
	GSList *alarms;
} CalComponentAlarms;

/* Alarm types */
typedef enum {
	CAL_ALARM_NONE,
	CAL_ALARM_AUDIO,
	CAL_ALARM_DISPLAY,
	CAL_ALARM_EMAIL,
	CAL_ALARM_PROCEDURE,
	CAL_ALARM_UNKNOWN
} CalAlarmAction;

/* Whether a trigger is relative to the start or end of an event occurrence, or
 * whether it is specified to occur at an absolute time.
 */
typedef enum {
	CAL_ALARM_TRIGGER_NONE,
	CAL_ALARM_TRIGGER_RELATIVE_START,
	CAL_ALARM_TRIGGER_RELATIVE_END,
	CAL_ALARM_TRIGGER_ABSOLUTE
} CalAlarmTriggerType;

typedef struct {
	CalAlarmTriggerType type;

	union {
		struct icaldurationtype rel_duration;
		struct icaltimetype abs_time;
	} u;
} CalAlarmTrigger;

typedef struct {
	/* Number of extra repetitions, zero for none */
	int repetitions;

	/* Interval between repetitions */
	struct icaldurationtype duration;
} CalAlarmRepeat;

gboolean cal_component_has_alarms (CalComponent *comp);
void cal_component_add_alarm (CalComponent *comp, CalComponentAlarm *alarm);
void cal_component_remove_alarm (CalComponent *comp, const char *auid);
void cal_component_remove_all_alarms (CalComponent *comp);

GList *cal_component_get_alarm_uids (CalComponent *comp);
CalComponentAlarm *cal_component_get_alarm (CalComponent *comp, const char *auid);

void cal_component_alarms_free (CalComponentAlarms *alarms);

/* CalComponentAlarms */
CalComponentAlarm *cal_component_alarm_new (void);
CalComponentAlarm *cal_component_alarm_clone (CalComponentAlarm *alarm);
void cal_component_alarm_free (CalComponentAlarm *alarm);

const char *cal_component_alarm_get_uid (CalComponentAlarm *alarm);

void cal_component_alarm_get_action (CalComponentAlarm *alarm, CalAlarmAction *action);
void cal_component_alarm_set_action (CalComponentAlarm *alarm, CalAlarmAction action);

void cal_component_alarm_get_attach (CalComponentAlarm *alarm, icalattach **attach);
void cal_component_alarm_set_attach (CalComponentAlarm *alarm, icalattach *attach);

void cal_component_alarm_get_description (CalComponentAlarm *alarm, CalComponentText *description);
void cal_component_alarm_set_description (CalComponentAlarm *alarm, CalComponentText *description);

void cal_component_alarm_get_repeat (CalComponentAlarm *alarm, CalAlarmRepeat *repeat);
void cal_component_alarm_set_repeat (CalComponentAlarm *alarm, CalAlarmRepeat repeat);

void cal_component_alarm_get_trigger (CalComponentAlarm *alarm, CalAlarmTrigger *trigger);
void cal_component_alarm_set_trigger (CalComponentAlarm *alarm, CalAlarmTrigger trigger);

void cal_component_alarm_get_attendee_list (CalComponentAlarm *alarm, GSList **attendee_list);
void cal_component_alarm_set_attendee_list (CalComponentAlarm *alarm, GSList *attendee_list);
gboolean cal_component_alarm_has_attendees (CalComponentAlarm *alarm);

icalcomponent *cal_component_alarm_get_icalcomponent (CalComponentAlarm *alarm);



END_GNOME_DECLS

#endif
