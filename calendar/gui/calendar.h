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

	/* Events that have a recurrence field are also present here */
	GList   *recur;

	/* Time at which the calendar was created */
	time_t  created;

	/* If the calendar was last modified */
	int     modified;
	void    *temp;
} Calendar;

/* This is only used by the calendar_get_events_in_range routine to get
 * a list of objects that recur on a specific date
 */
typedef struct {
	time_t     ev_start;
	time_t     ev_end;
	iCalObject *ico;
} CalendarObject;

Calendar *calendar_new                  (char *title);
char     *calendar_load                 (Calendar *cal, char *fname);
void      calendar_add_object           (Calendar *cal, iCalObject *obj);
void      calendar_remove_object        (Calendar *cal, iCalObject *obj);
void      calendar_destroy              (Calendar *cal);
GList    *calendar_get_objects_in_range (GList *objects, time_t start, time_t end, GCompareFunc sort_func);
GList    *calendar_get_todo_in_range    (Calendar *cal, time_t start, time_t end, GCompareFunc sort_func);
GList    *calendar_get_journal_in_range (Calendar *cal, time_t start, time_t end, GCompareFunc sort_func);
gint      calendar_compare_by_dtstart   (gpointer a, gpointer b);

void      calendar_iterate_on_objects   (GList *objects, time_t start, time_t end, calendarfn cb, void *closure);
void      calendar_iterate              (Calendar *cal, time_t start, time_t end, calendarfn cb, void *closure);

/* Note this routine returns a GList with CalendarObjects */
GList    *calendar_get_events_in_range  (Calendar *cal, time_t start, time_t end);

/* Destroy the above list with this method */
void      calendar_destroy_event_list (GList *l);

END_GNOME_DECLS

#endif
