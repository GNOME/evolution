#ifndef CALENDAR_H
#define CALENDAR_H

BEGIN_GNOME_DECLS

typedef struct {
	char    *title;
	char    *filename;
	GList  	*events;
	GList  	*todo;
	GList  	*journal;
} Calendar;

Calendar *calendar_new      (char *title);
void calendar_add_object    (Calendar *cal, iCalObject *obj);
void calendar_remove_object (Calendar *cal, iCalObject *obj);
void calendar_destroy       (Calendar *cal);

GList *calendar_get_events_in_range  (Calendar *cal, time_t start, time_t end);
GList *calendar_get_todo_in_range    (Calendar *cal, time_t start, time_t end);
GList *calendar_get_journal_in_range (Calendar *cal, time_t start, time_t end);

END_GNOME_DECLS

#endif
