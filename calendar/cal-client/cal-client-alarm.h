#ifndef CAL_CLIENT_ALARM_H_INCLUDED
#define CAL_CLIENT_ALARM_H_INCLUDED

#include <sys/time.h>
#include "cal-util/alarm-enums.h"

typedef int AlarmHandle;

/* these need to be replaced with something that does corba stuff */

time_t cal_client_alarm_get_trigger (AlarmHandle alarm);
enum AlarmType cal_client_alarm_get_type (AlarmHandle alarm);
void cal_client_alarm_set_type (AlarmHandle alarm, enum AlarmType type);
enum AlarmUnit cal_client_alarm_get_units (AlarmHandle alarm);
void cal_client_alarm_set_units (AlarmHandle alarm, enum AlarmUnit units);
int cal_client_alarm_get_count (AlarmHandle alarm);
void cal_client_alarm_set_count (AlarmHandle alarm, int count);
int cal_client_alarm_get_enabled (AlarmHandle alarm);
void cal_client_alarm_set_enabled (AlarmHandle alarm, int enabled);
char *cal_client_alarm_get_data (AlarmHandle alarm);
void cal_client_alarm_set_data (AlarmHandle alarm, char *data);

#endif /* CAL_CLIENT_ALARM_H_INCLUDED */
