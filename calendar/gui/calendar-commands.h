#ifndef CALENDAR_COMMANDS_H
#define CALENDAR_COMMANDS_H

#include <bonobo/bonobo-control.h>

/* This enum and the following array define the color preferences */

typedef enum {
	COLOR_PROP_OUTLINE_COLOR,	/* Color of calendar outline */
	COLOR_PROP_HEADING_COLOR,	/* Color for headings */
	COLOR_PROP_EMPTY_DAY_BG,	/* Background color for empty days */
	COLOR_PROP_MARK_DAY_BG,		/* Background color for days with appointments */
	COLOR_PROP_PRELIGHT_DAY_BG,	/* Background color for prelighted day */
	COLOR_PROP_DAY_FG,		/* Color for day numbers */
	COLOR_PROP_CURRENT_DAY_FG,	/* Color for current day's number */
	COLOR_PROP_TODO_NOT_DUE_YET,    /* Color for Todo items not yet due */
	COLOR_PROP_TODO_DUE_TODAY,      /* Color for Todo items due today */ 
	COLOR_PROP_TODO_OVERDUE,        /* Color for Todo items that are overdue */
	COLOR_PROP_LAST			/* Number of color properties */
} ColorProp;

struct color_prop {
	int r;		/* Values are in [0, 65535] */
	int g;
	int b;
	char *label;	/* Label for properties dialog */
	char *key;	/* Key for gnome_config */
};

extern struct color_prop color_props[];


#define COOKIE_USER_HOME_DIR ((char *) -1)

 
/* Calendar preferences */

extern int day_begin, day_end;
extern char *user_name;
extern int am_pm_flag;
extern int week_starts_on_monday;

/* todo preferences */
extern int todo_show_due_date;

extern int todo_item_dstatus_highlight_overdue;
extern int todo_item_dstatus_highlight_due_today;
extern int todo_item_dstatus_highlight_not_due_yet;

extern int todo_show_time_remaining;
extern int todo_show_priority;
extern char *todo_overdue_font_text;
extern gboolean todo_style_changed;
extern gint todo_current_sort_column;
extern gint todo_current_sort_type;

/* alarm stuff */
extern CalendarAlarm alarm_defaults[4];
extern gboolean beep_on_display;
extern gboolean enable_aalarm_timeout;
extern guint audio_alarm_timeout;
extern const guint MAX_AALARM_TIMEOUT;
extern gboolean enable_snooze;
extern guint snooze_secs;
extern const guint MAX_SNOOZE_SECS;

/* Creates and runs the preferences dialog box */
void properties (GtkWidget *toplevel);

/* Asks for all the time-related displays to be updated when the user changes the time format
 * preferences.
 */
void time_format_changed (void);

/* Asks for all the month items' colors to be reset */
void colors_changed (void);

/* Asks for all todo lists to reflect the accurate properties */
void todo_properties_changed(void);

/* Creates and runs the Go-to date dialog */
void goto_dialog (GnomeCalendar *gcal);

/* Returns a pointer to a statically-allocated string with a representation of the specified color.
 * Values must be in [0, 65535].
 */
char *build_color_spec (int r, int g, int b);

/* Parses a color specification of the form "#%04x%04x%04x" and returns the color components. */
void parse_color_spec (char *spec, int *r, int *g, int *b);

/* Calls build_color_spec() for the color in the specified property number */
char *color_spec_from_prop (ColorProp propnum);

GnomeCalendar *new_calendar (char *full_name, char *calendar_file,
			     char *geometry, char *page, gboolean hidden);


/*----------------------------------------------------------------------*/
/* FIX ME -- where should this stuff go?                                */
/*----------------------------------------------------------------------*/

/* This is only used by the calendar_get_events_in_range routine to get
 * a list of objects that recur on a specific date
 */
typedef struct {
	time_t     ev_start;
	time_t     ev_end;
	iCalObject *ico;
} CalendarObject;

GList *calendar_get_events_in_range (CalClient *calc,
				     time_t start, time_t end);
void
calendar_iterate (GnomeCalendar *cal,
		  time_t start, time_t end,
		  calendarfn cb, void *closure);

void init_calendar (void);

void calendar_control_activate (BonoboControl *control, BonoboUIHandler *uih);
void calendar_control_deactivate (BonoboControl *control, BonoboUIHandler *uih);

void close_cmd (GtkWidget *widget, GnomeCalendar *gcal);
void quit_cmd (void);

extern char *user_calendar_file;
extern char *user_name;
extern char *full_name;
extern int debug_alarms;
extern int active_calendars;
extern GList *all_calendars;

#endif /* CALENDAR_COMMANDS_H */
