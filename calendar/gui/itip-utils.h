
#ifndef ITIP_UTILS_HEADER
#define ITIP_UTILS_HEADER

#include <config.h>
#include <ical.h>
#include <string.h>
#include <gnome.h>

extern gchar *partstat_values[];
extern gchar *role_values[];

icalparameter * get_icalparam_by_type (icalproperty *prop, icalparameter_kind kind);

#endif
