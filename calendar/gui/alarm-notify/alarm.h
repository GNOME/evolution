#ifndef ALARM_H
#define ALARM_H

#include <time.h>
#include "cal-client/cal-client-alarm.h"

typedef struct {
  	/* Widgets */
	void   *w_count;      /* A GtkEntry */
	void   *w_enabled;    /* A GtkChecButton */
	void   *w_timesel;    /* A GtkMenu */
	void   *w_entry;      /* A GnomeEntryFile/GtkEntry for PROGRAM/MAIL */
	void   *w_label;

        AlarmHandle alarm_handle; /* something that hooks to the server */
} CalendarAlarmUI;

typedef void (*AlarmFunction) (time_t time,
			       CalendarAlarmUI *which,
			       void *closuse);

void     alarm_init  (void);
gboolean alarm_add   (CalendarAlarmUI *alarm, AlarmFunction fn, void *closure);
int      alarm_kill  (void *closure);

#endif
