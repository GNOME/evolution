#ifndef MAIN_H
#define MAIN_H

/* Global preferences */

extern int day_begin, day_end;
extern char *user_name;
extern int am_pm_flag;
extern int week_starts_on_monday;

/* Creates and runs the preferences dialog box */
void properties (void);

/* Asks for all the time-related displays to be updated when the user changes the time format
 * preferences.
 */
void time_format_changed (void);

/* Creates and runs the Go-to date dialog */
void goto_dialog (GnomeCalendar *gcal);

#endif
