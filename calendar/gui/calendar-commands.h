#ifndef CALENDAR_COMMANDS_H
#define CALENDAR_COMMANDS_H

#include <bonobo/bonobo-control.h>
#include "gnome-cal.h"

/* This tells all the calendars to reload the config settings. */
void update_all_config_settings (void);

GnomeCalendar *new_calendar (void);

void calendar_set_uri (GnomeCalendar *gcal, char *calendar_file);

void calendar_control_activate (BonoboControl *control,
				GnomeCalendar *cal);
void calendar_control_deactivate (BonoboControl *control);

#endif /* CALENDAR_COMMANDS_H */
