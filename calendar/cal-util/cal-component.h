/* Evolution calendar - iCalendar component object
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: Federico Mena-Quintero <federico@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

/* Structures to return properties and their parameters */

typedef enum {
	CAL_COMPONENT_CLASS_NONE,
	CAL_COMPONENT_CLASS_PUBLIC,
	CAL_COMPONENT_CLASS_PRIVATE,
	CAL_COMPONENT_CLASS_CONFIDENTIAL,
	CAL_COMPONENT_CLASS_UNKNOWN
} CalComponentClassification;

typedef struct {
	/* Actual date/time value */
	struct icaltimetype *value;

	/* Timezone ID */
	const char *tzid;
} CalComponentDateTime;

typedef enum {
	CAL_COMPONENT_PERIOD_DATETIME,
	CAL_COMPONENT_PERIOD_DURATION
} CalComponentPeriodType;

typedef struct {
	CalComponentPeriodType type;

	struct icaltimetype start;

	union {
		struct icaltimetype end;
		struct icaldurationtype duration;
	} u;
} CalComponentPeriod;

typedef struct {
	/* Description string */
	const char *value;

	/* Alternate representation URI */
	const char *altrep;
} CalComponentText;

typedef enum {
	CAL_COMPONENT_TRANSP_NONE,
	CAL_COMPONENT_TRANSP_TRANSPARENT,
	CAL_COMPONENT_TRANSP_OPAQUE,
	CAL_COMPONENT_TRANSP_UNKNOWN
} CalComponentTransparency;

typedef struct _CalComponentAlarm CalComponentAlarm;

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

void cal_component_set_new_vtype (CalComponent *comp, CalComponentVType type);

gboolean cal_component_set_icalcomponent (CalComponent *comp, icalcomponent *icalcomp);
icalcomponent *cal_component_get_icalcomponent (CalComponent *comp);

CalComponentVType cal_component_get_vtype (CalComponent *comp);

char *cal_component_get_as_string (CalComponent *comp);

void cal_component_commit_sequence (CalComponent *comp);

void cal_component_get_uid (CalComponent *comp, const char **uid);
void cal_component_set_uid (CalComponent *comp, const char *uid);

void cal_component_get_categories_list (CalComponent *comp, GSList **categ_list);
void cal_component_set_categories_list (CalComponent *comp, GSList *categ_list);

void cal_component_get_classification (CalComponent *comp, CalComponentClassification *classif);
void cal_component_set_classification (CalComponent *comp, CalComponentClassification classif);

void cal_component_get_comment_list (CalComponent *comp, GSList **text_list);
void cal_component_set_comment_list (CalComponent *comp, GSList *text_list);

void cal_component_get_completed (CalComponent *comp, struct icaltimetype **t);
void cal_component_set_completed (CalComponent *comp, struct icaltimetype *t);

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

void cal_component_get_exrule_list (CalComponent *comp, GSList **recur_list);
void cal_component_set_exrule_list (CalComponent *comp, GSList *recur_list);

void cal_component_get_last_modified (CalComponent *comp, struct icaltimetype **t);
void cal_component_set_last_modified (CalComponent *comp, struct icaltimetype *t);

void cal_component_get_rdate_list (CalComponent *comp, GSList **period_list);
void cal_component_set_rdate_list (CalComponent *comp, GSList *period_list);

void cal_component_get_rrule_list (CalComponent *comp, GSList **recur_list);
void cal_component_set_rrule_list (CalComponent *comp, GSList *recur_list);

void cal_component_get_sequence (CalComponent *comp, int **sequence);
void cal_component_set_sequence (CalComponent *comp, int *sequence);

void cal_component_get_summary (CalComponent *comp, CalComponentText *summary);
void cal_component_set_summary (CalComponent *comp, CalComponentText *summary);

void cal_component_get_transparency (CalComponent *comp, CalComponentTransparency *transp);
void cal_component_set_transparency (CalComponent *comp, CalComponentTransparency transp);

void cal_component_get_url (CalComponent *comp, const char **url);
void cal_component_set_url (CalComponent *comp, const char *url);

/* Functions to free returned values */

void cal_component_free_categories_list (GSList *categ_list);
void cal_component_free_datetime (CalComponentDateTime *dt);
void cal_component_free_exdate_list (GSList *exdate_list);
void cal_component_free_icaltimetype (struct icaltimetype *t);
void cal_component_free_period_list (GSList *period_list);
void cal_component_free_recur_list (GSList *recur_list);
void cal_component_free_sequence (int *sequence);
void cal_component_free_text_list (GSList *text_list);

/* Alarms */

typedef enum {
	CAL_COMPONENT_ALARM_NONE,
	CAL_COMPONENT_ALARM_AUDIO,
	CAL_COMPONENT_ALARM_DISPLAY,
	CAL_COMPONENT_ALARM_EMAIL,
	CAL_COMPONENT_ALARM_PROCEDURE,
	CAL_COMPONENT_ALARM_UNKNOWN
} CalComponentAlarmAction;

typedef enum {
	CAL_COMPONENT_ALARM_TRIGGER_RELATIVE,
	CAL_COMPONENT_ALARM_TRIGGER_ABSOLUTE
} CalComponentAlarmTriggerType;

typedef enum {
	CAL_COMPONENT_ALARM_TRIGGER_RELATED_START,
	CAL_COMPONENT_ALARM_TRIGGER_RELATED_END
} CalComponentAlarmTriggerRelated;

typedef struct {
	CalComponentAlarmTriggerType type;

	union {
		struct {
			struct icaldurationtype duration;
			CalComponentAlarmTriggerRelated related;
		} relative;

		struct icaltimetype absolute;
	} u;
} CalComponentAlarmTrigger;

CalComponentAlarm *cal_component_get_first_alarm (CalComponent *comp);
CalComponentAlarm *cal_component_get_next_alarm (CalComponent *comp);

void cal_component_alarm_free (CalComponentAlarm *alarm);

void cal_component_alarm_get_action (CalComponentAlarm *alarm, CalComponentAlarmAction *action);
void cal_component_alarm_set_action (CalComponentAlarm *alarm, CalComponentAlarmAction action);

void cal_component_alarm_get_trigger (CalComponentAlarm *alarm, CalComponentAlarmTrigger **trigger);
void cal_component_alarm_set_trigger (CalComponentAlarm *alarm, CalComponentAlarmTrigger *trigger);
void cal_component_alarm_free_trigger (CalComponentAlarmTrigger *trigger);



END_GNOME_DECLS

#endif
