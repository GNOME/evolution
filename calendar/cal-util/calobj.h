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
#include "libversit/vcc.h"

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
	char           *data;	/* not used for iCalendar alarms */

	/* the following pointers are used for iCalendar alarms */

	char           *attach;	           /* AUDIO, EMAIL, PROC */
	char           *desc;	           /* DISPLAY, EMAIL, PROC */
	char           *summary;           /* EMAIL */
	char           *attendee;          /* EMAIL */

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

typedef enum {
	ICAL_PILOT_SYNC_NONE = 0,
	ICAL_PILOT_SYNC_MOD  = 1,
	ICAL_PILOT_SYNC_DEL  = 3
} iCalPilotState;

typedef struct {
	int     valid;		/* true if the Geography was specified */
	double  latitude;
	double  longitude;
} iCalGeo;

typedef enum {
	ICAL_OPAQUE,
	ICAL_TRANSPARENT
} iCalTransp;

typedef struct {
	char   *uid;
	char   *reltype;
} iCalRelation;

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

/* 
   NOTE: iCalPerson is used for various property values which specify
   people (e.g. ATTENDEE, ORGANIZER, etc.  Not all fields are valid
   under RFC 2445 for all property values, but iCalPerson can store
   them anyway.  Enforcing the RFC is a job for the parser.
*/

typedef struct {
	char          *addr;
	char          *name;
	char          *role;
	char          *partstat;
	gboolean      rsvp;
	char          *cutype;	/* calendar user type */
	GList         *member;	/* group memberships */
	GList         *deleg_to;
	GList         *deleg_from;
	char          *sent_by;
	char          *directory;
	GList         *altrep;	/* list of char* URI's */
} iCalPerson;

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
        GList         *attendee; 	/* type: CAL-ADDRESS (list of iCalPerson) */
	GList         *categories; 	/* type: one or more TEXT */
	char          *class;

	char          *comment;		/* we collapse one or more TEXTs into one */
	time_t        completed;
	time_t        created;
	GList         *contact;		/* type: one or more TEXT */
	char          *desc;
	time_t        dtstamp;
	time_t        dtstart;
	time_t        dtend;            /* also duedate for todo's */
        gboolean      date_only;        /* set if the start/end times were
   specified using dates, not times (internal use, not stored to disk) */
	GList         *exdate;		/* type: one or more time_t's */
	GList         *exrule;		/* type: one or more RECUR */
	iCalGeo       geo;
	time_t        last_mod;
	char          *location;
	iCalPerson    *organizer;
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

	GList         *alarms;

	Recurrence    *recur;
	
	int new;
	void *user_data;		/* Generic data pointer */

	/* Pilot */
	iCalPilotState pilot_status;    /* Status information */
	guint32            pilot_id;        /* Pilot ID */

	guint	       ref_count;
} iCalObject;

/* The callback for the recurrence generator */
typedef int (*calendarfn) (iCalObject *, time_t, time_t, void *);

iCalObject *ical_new                        (char *comment, char *organizer, char *summary);
iCalObject *ical_object_new                 (void);

/* iCalObjects are created with a refcount of 1. When it drops to 0 it is
   destroyed with ical_object_destroy(). To maintain backwards compatability
   ical_object_destroy() can still be used, though code which uses it should
   not be mixed with code that uses refcounts. */
void        ical_object_ref                 (iCalObject *ico);
void        ical_object_unref               (iCalObject *ico);

void        ical_object_destroy             (iCalObject *ico);

iCalObject *ical_object_create_from_vobject (VObject *obj, const char *object_name);
VObject    *ical_object_to_vobject          (iCalObject *ical);
iCalObject *ical_object_duplicate           (iCalObject *o);
void        ical_foreach                    (GList *events, calendarfn fn, void *closure);
void        ical_object_generate_events     (iCalObject *ico, time_t start, time_t end, calendarfn cb, void *closure);
void        ical_object_add_exdate          (iCalObject *o, time_t t);

/* Computes the enddate field of the recurrence based on the duration */
void        ical_object_compute_end         (iCalObject *ico);

typedef enum {
	CAL_OBJ_FIND_SUCCESS,
	CAL_OBJ_FIND_SYNTAX_ERROR,
	CAL_OBJ_FIND_NOT_FOUND
} CalObjFindStatus;

CalObjFindStatus ical_object_find_in_string (const char *uid, const char *vcalobj, iCalObject **ico);

char       *ical_object_to_string (iCalObject *ico);


/* Returns the first toggled day in a weekday mask -- we do this because we do not support multiple
 * days on a monthly-by-pos recurrence.  If no days are toggled, it returns -1.
 */
int	    ical_object_get_first_weekday (int weekday_mask);

/* Returns the number of seconds configured to trigger the alarm in advance to an event */
int         alarm_compute_offset (CalendarAlarm *a);


/* Returns TRUE if the dates of both objects match, including any recurrence
   rules. */
gboolean    ical_object_compare_dates (iCalObject *ico1, iCalObject *ico2);

/* Generates a new uid for a calendar object. Should be g_free'd eventually. */
char	   *ical_gen_uid (void);

END_GNOME_DECLS

#endif

