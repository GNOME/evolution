#ifndef ICALENDAR_H
#define ICALENDAR_H

#include <ical.h>
#include <cal-util/calobj.h>



iCalObject *ical_object_create_from_icalcomponent (icalcomponent* comp);



#endif
