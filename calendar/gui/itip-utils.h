
#ifndef ITIP_UTILS_HEADER
#define ITIP_UTILS_HEADER

#include <config.h>
#include <ical.h>
#include <string.h>
#include <glib.h>
#include <cal-util/cal-component.h>

extern gchar *partstat_values[];
extern gchar *role_values[];

icalparameter * get_icalparam_by_type (icalproperty *prop, icalparameter_kind kind);

typedef enum {
	CAL_COMPONENT_METHOD_PUBLISH,
	CAL_COMPONENT_METHOD_REQUEST,
	CAL_COMPONENT_METHOD_REPLY,
	CAL_COMPONENT_METHOD_ADD,
	CAL_COMPONENT_METHOD_CANCEL,
	CAL_COMPONENT_METHOD_REFRESH,
	CAL_COMPONENT_METHOD_COUNTER,
	CAL_COMPONENT_METHOD_DECLINECOUNTER
} CalComponentItipMethod;

void itip_send_comp (CalComponentItipMethod method, CalComponent *comp);

#endif
