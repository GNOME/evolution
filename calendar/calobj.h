/*
 * Internal representation of a Calendar object.  This is modeled after the
 * iCalendar/vCalendar specificiation
 *
 * Authors: Miguel de Icaza (miguel@gnu.org), Federico Mena (federico@gimp.org).
 */
#ifndef CALOBJ_H
#define CALOBJ_H

#include <libgnome/libgnome.h>

BEGIN_GNOME_DECLS

typedef struct {
	char      *alarm_audio_file;
	char      *alarm_script;
	char      *alarm_email;
	char      *alarm_text;	/* Text to be displayed */
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
	char          *description;
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

	/* VALARM objects are always inside another object, never alone */
	CalendarAlarm *alarm;
} iCalObject;

iCalObject *ical_new (char *comment, char *organizer, char *summary);
iCalObject *ical_object_new (void);
void        ical_object_destroy (iCalObject *ico);

END_GNOME_DECLS

#endif
