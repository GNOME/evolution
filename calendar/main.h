#ifndef MAIN_H
#define MAIN_H

/* Calendar preferences */

extern int day_begin, day_end;
extern char *user_name;
extern int am_pm_flag;
extern int week_starts_on_monday;


/* This enum and the following array define the color preferences */

typedef enum {
	COLOR_PROP_OUTLINE_COLOR,	/* Color of calendar outline */
	COLOR_PROP_HEADING_COLOR,	/* Color for headings */
	COLOR_PROP_EMPTY_DAY_BG,	/* Background color for empty days */
	COLOR_PROP_MARK_DAY_BG,		/* Background color for days with appointments */
	COLOR_PROP_PRELIGHT_DAY_BG,	/* Background color for prelighted day */
	COLOR_PROP_DAY_FG,		/* Color for day numbers */
	COLOR_PROP_CURRENT_DAY_FG,	/* Color for current day's number */
	COLOR_PROP_OVERDUE_TODO,
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

 
/* todo preferences */
extern int todo_show_due_date;
extern int todo_due_date_overdue_highlight;
extern int todo_show_priority;
extern char *todo_overdue_font_text;
extern struct color_prop todo_overdue_highlight_color;
extern gboolean todo_style_changed;
extern gint todo_current_sort_column;
extern gint todo_current_sort_type;

/* alarm stuff */
extern CalendarAlarm alarm_defaults[4];
extern gboolean beep_on_display;
extern gboolean enable_aalarm_timeout;
extern guint audio_alarm_timeout;
extern const guint MAX_AALARM_TIMEOUT;

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

void save_default_calendar (GnomeCalendar *gcal);

GnomeCalendar *new_calendar (char *full_name, char *calendar_file,
			     char *geometry, char *page, gboolean hidden);

#endif





