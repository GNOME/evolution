#ifndef CALENDAR_H
#define CALENDAR_H

#include "calobj.h"

BEGIN_GNOME_DECLS

typedef struct {
	char    *title;
	char    *filename;
	GList  	*events;
	GList  	*todo;
	GList  	*journal;

	time_t  created;
	int     modified;
	void    *temp;
} Calendar;

Calendar *calendar_new                  (char *title);
void      calendar_load                 (Calendar *cal, char *fname);
void      calendar_add_object           (Calendar *cal, iCalObject *obj);
void      calendar_remove_object        (Calendar *cal, iCalObject *obj);
void      calendar_destroy              (Calendar *cal);
GList    *calendar_get_events_in_range  (Calendar *cal, time_t start, time_t end, GCompareFunc sort_func);
GList    *calendar_get_todo_in_range    (Calendar *cal, time_t start, time_t end, GCompareFunc sort_func);
GList    *calendar_get_journal_in_range (Calendar *cal, time_t start, time_t end, GCompareFunc sort_func);
gint      calendar_compare_by_dtstart   (gpointer a, gpointer b);

END_GNOME_DECLS

#endif
