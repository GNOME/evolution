/*
 * GnomeCalendar widget
 * Copyright (C) 1998 the Free Software Foundation
 *
 * Authors:
 *          Miguel de Icaza (miguel@kernel.org)
 *          Federico Mena (quartic@gimp.org)
 */

#include <config.h>
#include <gnome.h>
#include <pwd.h>
#include <sys/types.h>

#include "alarm.h"
#include "calendar.h"
#include "eventedit.h"
#include "gnome-cal.h"
#include "main.h"
#include "timeutil.h"

/* The username, used to set the `owner' field of the event */
char *user_name;

/* The full user name from the Gecos field */
char *full_name;

/* The user's default calendar file */
char *user_calendar_file;

/* a gnome-config string prefix that can be used to access the calendar config info */
char *calendar_settings;

/* Day begin, day end parameters */
int day_begin, day_end;

/* Number of calendars active */
int active_calendars = 0;

/* A list of all of the calendars started */
GList *all_calendars = NULL;

static void new_calendar (char *full_name, char *calendar_file);

/* For dumping part of a calendar */
static time_t from_t, to_t;

/* File to load instead of the user default's file */
static char *load_file;

/* If set, show events for the specified date and quit */
static int show_events;

static void
init_username (void)
{
	char *p;
	struct passwd *passwd;

	passwd = getpwuid (getuid ());
	if ((p = passwd->pw_name)) {
		user_name = g_strdup (p);
		full_name = g_strdup (passwd->pw_gecos);
	} else {
		if ((p = getenv ("USER"))) {
			user_name = g_strdup (p);
			full_name = g_strdup (p);
			return;
		} else {
			user_name = g_strdup ("unknown");
			full_name = g_strdup ("unknown");
		}
	}
	endpwent ();
}

static int
range_check_hour (int hour)
{
	if (hour < 0)
		hour = 0;
	else if (hour >= 24)
		hour = 23;

	return hour;
}

/*
 * Initializes the calendar internal variables, loads defaults
 */
static void
init_calendar (void)
{
	init_username ();
	user_calendar_file = g_concat_dir_and_file (gnome_util_user_home (), ".gnome/user-cal.vcf");
	calendar_settings  = g_copy_strings ("=", gnome_util_user_home (), ".gnome/calendar=", NULL);

	gnome_config_push_prefix (calendar_settings);
	day_begin = range_check_hour (gnome_config_get_int ("/Calendar/Day start=8"));
	day_end   = range_check_hour (gnome_config_get_int ("/Calendar/Day end=17"));

	if (day_end < day_begin){
		day_begin = 8;
		day_end   = 17;
	}

	gnome_config_pop_prefix ();
}

static void save_calendar_cmd (GtkWidget *widget, void *data);

static void
about_calendar_cmd (GtkWidget *widget, void *data)
{
        GtkWidget *about;
        gchar *authors[] = {
		"Miguel de Icaza (miguel@kernel.org)",
		"Federico Mena (quartic@gimp.org)",
		"Arturo Espinosa (arturo@nuclecu.unam.mx)",
		NULL
	};

        about = gnome_about_new (_("Gnome Calendar"), VERSION,
				 "(C) 1998 the Free Software Fundation",
				 authors,
				 _("The GNOME personal calendar and schedule manager."),
				 NULL);
        gtk_widget_show (about);
}

static void
display_objedit (GtkWidget *widget, GnomeCalendar *gcal)
{
	GtkWidget *ee;

	ee = event_editor_new (gcal, NULL);
	gtk_widget_show (ee);
}

static void
close_cmd (GtkWidget *widget, GnomeCalendar *gcal)
{
	all_calendars = g_list_remove (all_calendars, gcal);
	if (gcal->cal->modified){
		if (!gcal->cal->filename)
			save_calendar_cmd (widget, gcal);
		else
			calendar_save (gcal->cal, gcal->cal->filename);
	}

/*	gtk_widget_destroy (GTK_WIDGET (gcal)); */
	active_calendars--;
	
	if (active_calendars == 0)
		gtk_main_quit ();
}

static void
quit_cmd (GtkWidget *widget, GnomeCalendar *gcal)
{
	while (all_calendars){
		GnomeCalendar *cal = GNOME_CALENDAR (all_calendars->data);

		close_cmd (GTK_WIDGET (cal), cal);
	}
}

static void
previous_clicked (GtkWidget *widget, GnomeCalendar *gcal)
{
	gnome_calendar_previous (gcal);
}

static void
next_clicked (GtkWidget *widget, GnomeCalendar *gcal)
{
	gnome_calendar_next (gcal);
}

static void
today_clicked (GtkWidget *widget, GnomeCalendar *gcal)
{
	gnome_calendar_goto (gcal, time (NULL));
}

static void
new_calendar_cmd (GtkWidget *widget, void *data)
{
	new_calendar (full_name, NULL);
}

static void
open_ok (GtkWidget *widget, GtkFileSelection *fs)
{
	/* FIXME: find out who owns this calendar and use that name */
	new_calendar ("Somebody", gtk_file_selection_get_filename (fs));

	gtk_widget_destroy (GTK_WIDGET (fs));
}

static void
open_calendar_cmd (GtkWidget *widget, void *data)
{
	GtkFileSelection *fs;

	fs = GTK_FILE_SELECTION (gtk_file_selection_new (_("Open calendar")));

	gtk_signal_connect (GTK_OBJECT (fs->ok_button), "clicked",
			    (GtkSignalFunc) open_ok,
			    fs);
	gtk_signal_connect_object (GTK_OBJECT (fs->cancel_button), "clicked",
				   (GtkSignalFunc) gtk_widget_destroy,
				   GTK_OBJECT (fs));

	gtk_widget_show (GTK_WIDGET (fs));
	gtk_grab_add (GTK_WIDGET (fs)); /* Yes, it is modal, so sue me */
}

static void
save_ok (GtkWidget *widget, GtkFileSelection *fs)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (gtk_object_get_user_data (GTK_OBJECT (fs)));
	gtk_window_set_wmclass (GTK_WINDOW (gcal), "gnomecal", "gnomecal");
	
	if (gcal->cal->filename)
		g_free (gcal->cal->filename);

	gcal->cal->filename = g_strdup (gtk_file_selection_get_filename (fs));
	calendar_save (gcal->cal, gcal->cal->filename);
	gtk_main_quit ();
}

static gint
close_save (GtkWidget *w)
{
	gtk_main_quit ();
	return TRUE;
}

static void
save_as_calendar_cmd (GtkWidget *widget, void *data)
{
	GtkFileSelection *fs;

	fs = GTK_FILE_SELECTION (gtk_file_selection_new (_("Save calendar")));
	gtk_object_set_user_data (GTK_OBJECT (fs), data);

	gtk_signal_connect (GTK_OBJECT (fs->ok_button), "clicked",
			    (GtkSignalFunc) save_ok,
			    fs);
	gtk_signal_connect_object (GTK_OBJECT (fs->cancel_button), "clicked",
				   (GtkSignalFunc) close_save,
				   GTK_OBJECT (fs));
	gtk_signal_connect_object (GTK_OBJECT (fs), "delete_event",
				   GTK_SIGNAL_FUNC (close_save),
				   GTK_OBJECT (fs));
	gtk_widget_show (GTK_WIDGET (fs));
	gtk_grab_add (GTK_WIDGET (fs)); /* Yes, it is modal, so sue me even more */
	gtk_main ();
	gtk_widget_destroy (GTK_WIDGET (fs));
}

static void
save_calendar_cmd (GtkWidget *widget, void *data)
{
	GnomeCalendar *gcal = data;

	if (gcal->cal->filename)
		calendar_save (gcal->cal, gcal->cal->filename);
	else
		save_as_calendar_cmd (widget, data);
}

static GnomeUIInfo gnome_cal_file_menu [] = {
	{ GNOME_APP_UI_ITEM, N_("New calendar"),  NULL, new_calendar_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_NEW },

	{ GNOME_APP_UI_ITEM, N_("Open calendar..."), NULL, open_calendar_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_OPEN },

	{ GNOME_APP_UI_ITEM, N_("Save calendar..."), NULL, save_calendar_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_SAVE },

	{ GNOME_APP_UI_ITEM, N_("Save calendar as..."), NULL, save_as_calendar_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_SAVE },

	{ GNOME_APP_UI_SEPARATOR },
	{ GNOME_APP_UI_ITEM, N_("Close this calendar"), NULL, close_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_EXIT },

	{ GNOME_APP_UI_ITEM, N_("Exit"), NULL, quit_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_EXIT },

	GNOMEUIINFO_END
};

static GnomeUIInfo gnome_cal_about_menu [] = {
	{ GNOME_APP_UI_ITEM, N_("About"), NULL, about_calendar_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_ABOUT },
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_HELP ("cal"),
	GNOMEUIINFO_END
};

static GnomeUIInfo gnome_cal_edit_menu [] = {
	{ GNOME_APP_UI_ITEM, N_("New appointment"), NULL, display_objedit },
	GNOMEUIINFO_END
};

static GnomeUIInfo gnome_cal_menu [] = {
	{ GNOME_APP_UI_SUBTREE, N_("File"), NULL, &gnome_cal_file_menu },
	{ GNOME_APP_UI_SUBTREE, N_("Calendar"), NULL, &gnome_cal_edit_menu },
	{ GNOME_APP_UI_SUBTREE, N_("Help"), NULL, &gnome_cal_about_menu },
	GNOMEUIINFO_END
};

static GnomeUIInfo gnome_toolbar [] = {
	{ GNOME_APP_UI_ITEM, N_("New"), N_("Create a new appointment"), display_objedit, 0, 0,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_NEW },

	GNOMEUIINFO_SEPARATOR,

	{ GNOME_APP_UI_ITEM, N_("Prev"), N_("Go back in time"), previous_clicked, 0, 0,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_BACK },

	{ GNOME_APP_UI_ITEM, N_("Today"), N_("Go to present time"), today_clicked, 0, 0,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_HOME },

	{ GNOME_APP_UI_ITEM, N_("Next"), N_("Go forward in time"), next_clicked, 0, 0,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_FORWARD },

	GNOMEUIINFO_END
};

static void
setup_menu (GtkWidget *gcal)
{
	gnome_app_create_menus_with_data (GNOME_APP (gcal), gnome_cal_menu, gcal);
	gnome_app_create_toolbar_with_data (GNOME_APP (gcal), gnome_toolbar, gcal);
}

static gint
calendar_close_event (GtkWidget *widget, GdkEvent *event, GnomeCalendar *gcal)
{
	close_cmd (widget, gcal);
	return TRUE;
}

static void
new_calendar (char *full_name, char *calendar_file)
{
	GtkWidget   *toplevel;
	char        *title;

	title = g_copy_strings (full_name, "'s calendar", NULL);

	toplevel = gnome_calendar_new (title);
	g_free (title);
	setup_menu (toplevel);

	if (calendar_file && g_file_exists (calendar_file)) 
		gnome_calendar_load (GNOME_CALENDAR (toplevel), calendar_file);
	else
		GNOME_CALENDAR (toplevel)->cal->filename = g_strdup (calendar_file);

	gtk_signal_connect (GTK_OBJECT (toplevel), "delete_event",
			    GTK_SIGNAL_FUNC(calendar_close_event), toplevel);
	
	active_calendars++;
	all_calendars = g_list_prepend (all_calendars, toplevel);
	gtk_widget_show (toplevel);
}

static void
process_dates (void)
{
	if (!from_t)
		from_t = time_start_of_day (time (NULL));
	
	if (!to_t || to_t < from_t)
		to_t = time_add_day (from_t, 1);
}

static struct argp_option argp_options [] = {
	{ "events", 'e', NULL, 0, N_("Show events and quit"),                 0 },
	{ "from",   'f', N_("DATE"), 0, N_("Specifies start date [for --events]"),  1 },
	{ "file",   'F', N_("FILE"), 0, N_("File to load calendar from"),           1 },
	{ "to",     't', N_("DATE"), 0, N_("Specifies ending date [for --events]"), 1 },
	{ NULL, 0, NULL, 0, NULL, 0 },
};

static int
same_day (struct tm *a, struct tm *b)
{
	return (a->tm_mday == b->tm_mday &&
		a->tm_mon  == b->tm_mon &&
		a->tm_year == b->tm_year);
}

static void
dump_events (void)
{
	Calendar *cal;
	GList *l;
	char *s;
	time_t now = time (NULL);
	struct tm today = *localtime (&now);
	
	process_dates ();
	init_calendar ();
	
	cal = calendar_new (full_name);
	s = calendar_load (cal, load_file ? load_file : user_calendar_file);
	if (s){
		printf ("error: %s\n", s);
		exit (1);
	}
	l = calendar_get_events_in_range (cal, from_t, to_t);
	for (; l; l = l->next){
		char start [80], end [80];
		CalendarObject *co = l->data;
		struct tm ts, te;
		char *format;
		
		ts = *localtime (&co->ev_start);
		te = *localtime (&co->ev_end);

		if (same_day (&today, &ts))
			format = "%H:%M";
		else
			format = "%A %d, %H:%M";
		strftime (start, sizeof (start), format, &ts);

		if (!same_day (&ts, &te))
			format = "%A %d, %H:%M";
		strftime (end,   sizeof (start), format, &te);
		
		printf ("%s -- %s\n", start, end);
		printf ("  %s\n", co->ico->summary);
	}
	calendar_destroy_event_list (l);
	calendar_destroy (cal);
	exit (0);
}

extern time_t get_date ();

static error_t
parse_an_arg (int key, char *arg, struct argp_state *state)
{
	switch (key){
	case 'f':
		from_t = get_date (arg, NULL);
		break;

	case 't':
		to_t = get_date (arg, NULL);
		break;

	case 'F':
		load_file = arg;
		break;

	case 'e':
		show_events = 1;
		break;

	case ARGP_KEY_END:
		if (show_events)
			dump_events ();

	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp parser =
{
	argp_options, parse_an_arg, NULL, NULL, NULL, NULL, NULL
};

int 
main(int argc, char *argv[])
{
	argp_program_version = VERSION;

	bindtextdomain(PACKAGE, GNOMELOCALEDIR);
	textdomain(PACKAGE);

	gnome_init ("gncal", &parser, argc, argv, 0, NULL);

	process_dates ();
	alarm_init ();
	init_calendar ();

	new_calendar (full_name, load_file ? load_file : user_calendar_file);
	gtk_main ();
	return 0;
}
