/*
 * Internal representation of a Calendar object.  This is modeled after the
 * iCalendar/vCalendar specificiation
 *
 * Authors: Miguel de Icaza (miguel@gnu.org)
 *          Federico Mena (quartic@gimp.org).
 */
#ifndef CALOBJ_H
#define CALOBJ_H

#include <libgnome/libgnome.h>
#include "versit/vcc.h"

BEGIN_GNOME_DECLS

enum AlarmType {
	ALARM_MAIL,
	ALARM_PROGRAM,
	ALARM_DISPLAY,
	ALARM_AUDIO
};

enum AlarmUnit {
	ALARM_MINUTES,
	ALARM_HOURS,
	ALARM_DAYS
};

typedef struct {
	enum AlarmType type;
	int            enabled;
	int            count;
	enum AlarmUnit units;
	char           *data;

	/* Does not get saved, internally used */
	time_t         offset;
	time_t         trigger;

	int            snooze_secs;
	int	       snooze_repeat;
	
	/* Widgets */
	void           *w_count;      /* A GtkEntry */
	void           *w_enabled;    /* A GtkChecButton */
	void           *w_timesel;    /* A GtkMenu */
	void           *w_entry;      /* A GnomeEntryFile/GtkEntry for PROGRAM/MAIL */
	void           *w_label;
} CalendarAlarm;

/* Calendar object type */
typedef enum {
	ICAL_EVENT,
	ICAL_TODO,
	ICAL_JOURNAL,
	ICAL_FBREQUEST,
	ICAL_FBREPLY,
	ICAL_BUSYTIME,
	ICAL_TIMEZONE
} iCalType;

/* For keys that might contain binary or text/binary */
typedef struct {
	char *data;
	int  len;
} iCalValue;

typedef struct {
	int     valid;		/* true if the Geography was specified */
	double  latitude;
	double  longitude;
} iCalGeo;

typedef enum {
	ICAL_OPAQUE,
	ICAL_TRANSPARENT
} iCalTransp;

typedef char NotYet;

enum RecurType {
	RECUR_DAILY,
	RECUR_WEEKLY,
	RECUR_MONTHLY_BY_POS,
	RECUR_MONTHLY_BY_DAY,
	RECUR_YEARLY_BY_MONTH,
	RECUR_YEARLY_BY_DAY,
};

#define DAY_LASTDAY 10000

typedef struct {
	enum RecurType type;

	int            interval;

	/* Used for recur computation */
	time_t         enddate;	/* If the value is zero, it is an infinite event
				 * otherwise, it is either the _enddate value (if
				 * this is what got specified)  or it is our computed
				 * ending date (computed from the duration item).
				 */
	
	int            weekday;

	union {
		int    month_pos;
		int    month_day;
	} u;

	int            duration;
	time_t         _enddate; /* As found on the vCalendar file */
	int            __count;
} Recurrence;

#define IS_INFINITE(r) (r->duration == 0)

/* Flags to indicate what has changed in an object */
typedef enum {
	CHANGE_NEW     = 1 << 0,	/* new object */
	CHANGE_SUMMARY = 1 << 1,	/* summary */
	CHANGE_DATES   = 1 << 2,	/* dtstart / dtend */
	CHANGE_ALL     = CHANGE_SUMMARY | CHANGE_DATES
} CalObjectChange;

/*
 * This describes an iCalendar object, note that we never store durations, instead we
 * always compute the end time computed from the start + duration.
 */
typedef struct {
	iCalType      type;

	GList         *attach;		/* type: one or more URIs or binary data */
	GList         *attendee; 	/* type: CAL-ADDRESS */
	GList         *categories; 	/* type: one or more TEXT */
	char          *class;

	char          *comment;		/* we collapse one or more TEXTs into one */
	time_t        completed;
	time_t        created;
	GList         *contact;		/* type: one or more TEXT */
	time_t        dtstamp;
	time_t        dtstart;
	time_t        dtend;
	GList         *exdate;		/* type: one or more time_t's */
	GList         *exrule;		/* type: one or more RECUR */
	iCalGeo       geo;
	time_t        last_mod;
	char          *location;
	char          *organizer;
	int           percent;
	int           priority;
	char          *rstatus;	        /* request status for freebusy */
	GList         *related;		/* type: one or more TEXT */
	GList         *resources;	/* type: one or more TEXT */
	GList         *rdate;		/* type: one or more recurrence date */
	GList         *rrule;		/* type: one or more recurrence rules */
	int           seq;
	char          *status;
	char          *summary;
	iCalTransp    transp;
	char          *uid;
	char          *url;
	time_t        recurid;

	CalendarAlarm dalarm;
	CalendarAlarm aalarm;
	CalendarAlarm palarm;
	CalendarAlarm malarm;

	Recurrence    *recur;
	
	int new;
	void *user_data;		/* Generic data pointer */
} iCalObject;

/* The callback for the recurrence generator */
typedef int (*calendarfn)(iCalObject *, time_t, time_t, void *);

iCalObject *ical_new                        (char *comment, char *organizer, char *summary);
iCalObject *ical_object_new                 (void);
void        ical_object_destroy             (iCalObject *ico);
iCalObject *ical_object_create_from_vobject (VObject *obj, const char *object_name);
VObject    *ical_object_to_vobject          (iCalObject *ical);
void        ical_foreach                    (GList *events, calendarfn fn, void *closure);
void        ical_object_generate_events     (iCalObject *ico, time_t start, time_t end, calendarfn cb, void *closure);

/* Computes the enddate field of the recurrence based on the duration */
void        ical_object_compute_end         (iCalObject *ico);

/* Returns the number of seconds configured to trigger the alarm in advance to an event */
int         alarm_compute_offset (CalendarAlarm *a);

END_GNOME_DECLS

#endif

