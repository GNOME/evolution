#ifndef ALARM_H
#define ALARM_H

#include <time.h>
#include "cal-util/calobj.h"

typedef void (*AlarmFunction)(time_t time, CalendarAlarm *which, void *closuse);

void      alarm_init    (void);
gboolean  alarm_add     (CalendarAlarm *alarm, AlarmFunction fn, void *closure);
int       alarm_kill    (void *closure);

#endif
