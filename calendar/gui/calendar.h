#ifndef CALENDAR_H
#define CALENDAR_H

#include "calobj.h"

BEGIN_GNOME_DECLS

typedef struct {
	/* This calendar's title */
	char    *title;

	/* backing store for this calendar object */
	char    *filename;

	/* The list of events;  todo's and journal entries */
	GList  	*events;
	GList  	*todo;
	GList  	*journal;

	GHashTable *event_hash;
	
	/* Time at which the calendar was created */
	time_t  created;

	/* Timestamp in the filename */
	time_t  file_time;
	
	/* If the calendar was last modified */
	int     modified;
	void    *temp;

	void    *corba_server;
} Calendar;

/* This is only used by the calendar_get_events_in_range routine to get
 * a list of objects that recur on a specific date
 */
typedef struct {
	time_t     ev_start;
	time_t     ev_end;
	iCalObject *ico;
} CalendarObject;

typedef enum {
	CALENDAR_INIT_NIL    = 0,
	CALENDAR_INIT_ALARMS = 1 << 0
} CalendarNewOptions;

Calendar *calendar_new                  (char *title,CalendarNewOptions options);
char     *calendar_get_as_vcal_string   (Calendar *cal);
char     *calendar_string_from_object   (iCalObject *object);

char     *calendar_load                 (Calendar *cal, char *fname);
char     *calendar_load_from_memory     (Calendar *cal, const char *buffer);
void      calendar_load_from_vobject    (Calendar *cal, VObject *vcal);
void      calendar_save                 (Calendar *cal, char *fname);
void      calendar_add_object           (Calendar *cal, iCalObject *obj);
void      calendar_remove_object        (Calendar *cal, iCalObject *obj);
void      calendar_destroy              (Calendar *cal);
GList    *calendar_get_objects_in_range (GList *objects, time_t start, time_t end, GCompareFunc sort_func);
GList    *calendar_get_todo_in_range    (Calendar *cal, time_t start, time_t end, GCompareFunc sort_func);
GList    *calendar_get_journal_in_range (Calendar *cal, time_t start, time_t end, GCompareFunc sort_func);
gint      calendar_compare_by_dtstart   (gpointer a, gpointer b);

void      calendar_iterate_on_objects   (GList *objects, time_t start, time_t end, calendarfn cb, void *closure);
void      calendar_iterate              (Calendar *cal, time_t start, time_t end, calendarfn cb, void *closure);

/* Returns a list of CalendarObject structures.  These represent the events in the calendar that are
 * in the specified range.
 */
GList *calendar_get_events_in_range (Calendar *cal, time_t start, time_t end);

/* Destroy list returned by calendar_get_events_in_range() with this function */
void calendar_destroy_event_list (GList *l);

/* Informs the calendar that obj information has changed */
void      calendar_object_changed       (Calendar *cal, iCalObject *obj, int flags);

void      calendar_notify (time_t time, CalendarAlarm *which, void *data);

iCalObject *calendar_object_find_event    (Calendar *cal, const char *uid);
iCalObject *calendar_object_find_todo     (Calendar *cal, const char *uid);
iCalObject *calendar_object_find          (Calendar *cal, const char *uid);
iCalObject *calendar_object_find_by_pilot (Calendar *cal, int pilot_id);

END_GNOME_DECLS

#endif
