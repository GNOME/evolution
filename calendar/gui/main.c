/*
 * Main file for the GNOME Calendar program
 * Copyright (C) 1998 the Free Software Foundation
 *
 * Authors:
 *          Miguel de Icaza (miguel@kernel.org)
 *          Federico Mena (federico@nuclecu.unam.mx)
 */

#include <config.h>
#include <gnome.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include "calendar.h"
#include "alarm.h"
#include "eventedit.h"
#include "gnome-cal.h"
#include "main.h"
#include "timeutil.h"


#define COOKIE_USER_HOME_DIR ((char *) -1)


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

/* Whether weeks starts on Sunday or Monday */
int week_starts_on_monday;

/* If true, do not show our top level window */
int startup_hidden = 0;

/* The array of color properties -- keep in sync with the enumeration defined in main.h.  The color
 * values specified here are the defaults for the program.
 */
struct color_prop color_props[] = {
	{ 0x3e72, 0x35ec, 0x8ba2, "Outline:",              "/calendar/Colors/outline" },
	{ 0xffff, 0xffff, 0xffff, "Headings:",             "/calendar/Colors/headings" },
	{ 0xf26c, 0xecec, 0xbbe7, "Empty days:",           "/calendar/Colors/empty_bg" },
	{ 0xfc1e, 0xf87f, 0x5f80, "Appointments:",         "/calendar/Colors/mark_bg" },
	{ 0xd364, 0xc6b7, 0x7969, "Highlighted day:",      "/calendar/Colors/prelight_bg" },
	{ 0x01f0, 0x01f0, 0x01f0, "Day numbers:",          "/calendar/Colors/day_fg" },
	{ 0x0000, 0x0000, 0xffff, "Current day's number:", "/calendar/Colors/current_fg" }
};

/* Number of active calendars */
int active_calendars = 0;

/* A list of all of the calendars started */
GList *all_calendars = NULL;

static void new_calendar (char *full_name, char *calendar_file, char *geometry, char *view, gboolean hidden);

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
		char *comma;

		user_name = g_strdup (p);
		full_name = g_strdup (passwd->pw_gecos);

		/* Keep only the name from the gecos field */
		if ((comma = strchr (full_name, ',')) != NULL)
			*comma = 0;
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
	int i;
	char *cspec, *color;
	char *str;

	init_username ();
	user_calendar_file = g_concat_dir_and_file (gnome_util_user_home (), ".gnome/user-cal.vcf");

	gnome_config_push_prefix (calendar_settings);

	/* Read calendar settings */

	day_begin  = range_check_hour (gnome_config_get_int  ("/calendar/Calendar/Day start=8"));
	day_end    = range_check_hour (gnome_config_get_int  ("/calendar/Calendar/Day end=17"));
	am_pm_flag = gnome_config_get_bool ("/calendar/Calendar/AM PM flag=0");
	week_starts_on_monday = gnome_config_get_bool ("/calendar/Calendar/Week starts on Monday=0");

	if (day_end < day_begin){
		day_begin = 8;
		day_end   = 17;
	}

	/* Read color settings */

	for (i = 0; i < COLOR_PROP_LAST; i++) {
		cspec = build_color_spec (color_props[i].r, color_props[i].g, color_props[i].b);
		str = g_strconcat (color_props[i].key, "=", cspec, NULL);

		color = gnome_config_get_string (str);
		parse_color_spec (color, &color_props[i].r, &color_props[i].g, &color_props[i].b);

		g_free (str);
		g_free (color);
	}

	/* Done */

	gnome_config_pop_prefix ();
}

static void save_calendar_cmd (GtkWidget *widget, void *data);

static void
about_calendar_cmd (GtkWidget *widget, void *data)
{
        GtkWidget *about;
        const gchar *authors[] = {
		"Miguel de Icaza (miguel@kernel.org)",
		"Federico Mena (federico@gimp.org)",
		"Arturo Espinosa (arturo@nuclecu.unam.mx)",
		NULL
	};

        about = gnome_about_new (_("Gnome Calendar"), VERSION,
				 "(C) 1998 the Free Software Foundation",
				 authors,
				 _("The GNOME personal calendar and schedule manager."),
				 NULL);
	gtk_window_set_modal (GTK_WINDOW (about), TRUE);
	gnome_dialog_set_close (GNOME_DIALOG (about), TRUE);
        gtk_widget_show (about);
}

static void
display_objedit (GtkWidget *widget, GnomeCalendar *gcal)
{
	GtkWidget *ee;
	iCalObject *ico;

	/* Default to the day the user is looking at */
	ico = ical_new ("", user_name, "");
	ico->new = 1;
	ico->dtstart = time_add_minutes (gcal->current_display, day_begin * 60);
	ico->dtend   = time_add_minutes (ico->dtstart, day_begin * 60 + 30 );

	ee = event_editor_new (gcal, ico);
	gtk_widget_show (ee);
}

static void
display_objedit_today (GtkWidget *widget, GnomeCalendar *gcal)
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

	gtk_widget_destroy (GTK_WIDGET (gcal));
	active_calendars--;

	if (active_calendars == 0)
		gtk_main_quit ();
}

void
time_format_changed (void)
{
	GList *l;

	for (l = all_calendars; l; l = l->next)
		gnome_calendar_time_format_changed (GNOME_CALENDAR (l->data));
}

void
colors_changed (void)
{
	GList *l;

	for (l = all_calendars; l; l = l->next)
		gnome_calendar_colors_changed (GNOME_CALENDAR (l->data));
}

static void
quit_cmd (void)
{
	while (all_calendars){
		GnomeCalendar *cal = GNOME_CALENDAR (all_calendars->data);

		close_cmd (GTK_WIDGET (cal), cal);
	}
}

/* Sets a clock cursor for the specified calendar window */
static void
set_clock_cursor (GnomeCalendar *gcal)
{
	GdkCursor *cursor;

	cursor = gdk_cursor_new (GDK_WATCH);
	gdk_window_set_cursor (GTK_WIDGET (gcal)->window, cursor);
	gdk_cursor_destroy (cursor);
	gdk_flush ();
}

/* Resets the normal cursor for the specified calendar window */
static void
set_normal_cursor (GnomeCalendar *gcal)
{
	gdk_window_set_cursor (GTK_WIDGET (gcal)->window, NULL);
	gdk_flush ();
}

static void
previous_clicked (GtkWidget *widget, GnomeCalendar *gcal)
{
	set_clock_cursor (gcal);
	gnome_calendar_previous (gcal);
	set_normal_cursor (gcal);
}

static void
next_clicked (GtkWidget *widget, GnomeCalendar *gcal)
{
	set_clock_cursor (gcal);
	gnome_calendar_next (gcal);
	set_normal_cursor (gcal);
}

static void
today_clicked (GtkWidget *widget, GnomeCalendar *gcal)
{
	set_clock_cursor (gcal);
	gnome_calendar_goto_today (gcal);
	set_normal_cursor (gcal);
}

static void
goto_clicked (GtkWidget *widget, GnomeCalendar *gcal)
{
	goto_dialog (gcal);
}

static void
new_calendar_cmd (GtkWidget *widget, void *data)
{
	new_calendar (full_name, NULL, NULL, NULL, FALSE);
}

static void
open_ok (GtkWidget *widget, GtkFileSelection *fs)
{
	/* FIXME: find out who owns this calendar and use that name */
	new_calendar ("Somebody", gtk_file_selection_get_filename (fs), NULL, NULL, FALSE);

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
properties_cmd (GtkWidget *widget, GtkWidget *gcal)
{
	properties (gcal);
}
		
static void
save_calendar_cmd (GtkWidget *widget, void *data)
{
	GnomeCalendar *gcal = data;

	if (gcal->cal->filename){
		struct stat s;
		
		stat (gcal->cal->filename, &s);

		if (s.st_mtime != gcal->cal->file_time){
			GtkWidget *box;
			char *str;
			int b;
			
			str = g_strdup_printf (
				_("File %s has changed since it was loaded\nContinue?"),
				gcal->cal->filename);
			box = gnome_message_box_new (str, GNOME_MESSAGE_BOX_INFO,
						     GNOME_STOCK_BUTTON_YES,
						     GNOME_STOCK_BUTTON_NO,
						     NULL);
			g_free (str);
			gnome_dialog_set_default (GNOME_DIALOG (box), 1);
			b = gnome_dialog_run (GNOME_DIALOG (box));
			gtk_object_destroy (GTK_OBJECT (box));

			if (b != 0)
				return;
		}
		
		calendar_save (gcal->cal, gcal->cal->filename);
	} else
		save_as_calendar_cmd (widget, data);
}

/*
 * Saves @gcal if it is the default calendar
 */
void
save_default_calendar (GnomeCalendar *gcal)
{
	if (!gcal->cal->filename)
		return;
	
	save_calendar_cmd (NULL, gcal);
}

static GnomeUIInfo gnome_cal_file_menu [] = {
        GNOMEUIINFO_MENU_NEW_ITEM(N_("_New calendar"),
				  N_("Create a new calendar"),
				  new_calendar_cmd, NULL),

	GNOMEUIINFO_MENU_OPEN_ITEM(open_calendar_cmd, NULL),

	GNOMEUIINFO_MENU_SAVE_ITEM(save_calendar_cmd, NULL),

	GNOMEUIINFO_MENU_SAVE_AS_ITEM(save_as_calendar_cmd, NULL),

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_MENU_CLOSE_ITEM(close_cmd, NULL),

	GNOMEUIINFO_MENU_EXIT_ITEM(quit_cmd, NULL),

	GNOMEUIINFO_END
};

static GnomeUIInfo gnome_cal_edit_menu [] = {
	{ GNOME_APP_UI_ITEM, N_("_New appointment..."),
	  N_("Create a new appointment"), display_objedit, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_NEW, 0, 0, NULL },
	{ GNOME_APP_UI_ITEM, N_("New appointment for _today..."),
	  N_("Create a new appointment for today"),
	  display_objedit_today, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_NEW, 0, 0, NULL },
	GNOMEUIINFO_END
};

static GnomeUIInfo gnome_cal_help_menu [] = {
	GNOMEUIINFO_HELP ("cal"),

	GNOMEUIINFO_MENU_ABOUT_ITEM(about_calendar_cmd, NULL),

	GNOMEUIINFO_END
};

static GnomeUIInfo gnome_cal_settings_menu [] = {
        GNOMEUIINFO_MENU_PREFERENCES_ITEM(properties_cmd, NULL),

	GNOMEUIINFO_END
};

static GnomeUIInfo gnome_cal_menu [] = {
        GNOMEUIINFO_MENU_FILE_TREE(gnome_cal_file_menu),
	GNOMEUIINFO_MENU_EDIT_TREE(gnome_cal_edit_menu),
	GNOMEUIINFO_MENU_SETTINGS_TREE(gnome_cal_settings_menu),
	GNOMEUIINFO_MENU_HELP_TREE(gnome_cal_help_menu),
	GNOMEUIINFO_END
};

static GnomeUIInfo gnome_toolbar [] = {
	GNOMEUIINFO_ITEM_STOCK (N_("New"), N_("Create a new appointment"), display_objedit, GNOME_STOCK_PIXMAP_NEW),

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_ITEM_STOCK (N_("Prev"), N_("Go back in time"), previous_clicked, GNOME_STOCK_PIXMAP_BACK),
	GNOMEUIINFO_ITEM_STOCK (N_("Today"), N_("Go to present time"), today_clicked, GNOME_STOCK_PIXMAP_HOME),
	GNOMEUIINFO_ITEM_STOCK (N_("Next"), N_("Go forward in time"), next_clicked, GNOME_STOCK_PIXMAP_FORWARD),

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_ITEM_STOCK (N_("Go to"), N_("Go to a specific date"), goto_clicked, GNOME_STOCK_PIXMAP_JUMP_TO),

	GNOMEUIINFO_END
};

static void
setup_menu (GtkWidget *gcal)
{
	gnome_app_create_menus_with_data (GNOME_APP (gcal), gnome_cal_menu, gcal);
	gnome_app_create_toolbar_with_data (GNOME_APP (gcal), gnome_toolbar, gcal);
	gnome_app_install_menu_hints(GNOME_APP(gcal), gnome_cal_menu);
}

static void
setup_appbar (GtkWidget *gcal)
{
        GnomeAppBar *appbar;
	
        appbar = gnome_appbar_new (FALSE, TRUE, GNOME_PREFERENCES_USER);
	gnome_app_set_statusbar (GNOME_APP (gcal), GTK_WIDGET(appbar));
}

static gint
calendar_close_event (GtkWidget *widget, GdkEvent *event, GnomeCalendar *gcal)
{
	close_cmd (widget, gcal);
	return TRUE;
}

static void
new_calendar (char *full_name, char *calendar_file, char *geometry, char *page, gboolean hidden)
{
	GtkWidget   *toplevel;
	char        title[128];
	int         xpos, ypos, width, height;

	/* i18n: This "%s%s" indicates possession. Languages where the order is
       the inverse should translate it to "%2$s%1$s". */
	g_snprintf(title, 128, _("%s%s"), full_name, _("'s calendar"));

	toplevel = gnome_calendar_new (title);
	    
	if (gnome_parse_geometry (geometry, &xpos, &ypos, &width, &height)){
		if (xpos != -1)
			gtk_widget_set_uposition (toplevel, xpos, ypos);
		if (width != -1)
			gtk_widget_set_usize (toplevel, width, height);
	}
	setup_appbar (toplevel);
	setup_menu (toplevel);


	if (page)
		gnome_calendar_set_view (GNOME_CALENDAR (toplevel), page);

	if (calendar_file && g_file_exists (calendar_file))
		gnome_calendar_load (GNOME_CALENDAR (toplevel), calendar_file);
	else
		GNOME_CALENDAR (toplevel)->cal->filename = g_strdup (calendar_file);

	gtk_signal_connect (GTK_OBJECT (toplevel), "delete_event",
			    GTK_SIGNAL_FUNC(calendar_close_event), toplevel);

	active_calendars++;
	all_calendars = g_list_prepend (all_calendars, toplevel);

	if (hidden){
		GnomeWinState state;
		
		state = gnome_win_hints_get_state (toplevel);

		state |= WIN_STATE_MINIMIZED;
		gnome_win_hints_set_state (toplevel, state);
	}
	
	gtk_widget_show (toplevel);
}

static void
process_dates (void)
{
	if (!from_t)
		from_t = time_day_begin (time (NULL));

	if (!to_t || to_t < from_t)
		to_t = time_add_day (from_t, 1);
}

enum {
	GEOMETRY_KEY = -1,
	USERFILE_KEY = -2,
	VIEW_KEY     = -3,
	HIDDEN_KEY   = -4,
};

/* Lists used to startup various GnomeCalendars */
static GList *start_calendars;
static GList *start_geometries;
static GList *start_views;

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

static void
parse_an_arg (poptContext ctx,
	      enum poptCallbackReason reason,
	      const struct poptOption *opt,
	      char *arg, void *data)
{
	switch (opt->val){
	case 'f':
		from_t = get_date (arg, NULL);
		break;

	case 't':
		to_t = get_date (arg, NULL);
		break;

	case GEOMETRY_KEY:
		start_geometries = g_list_append (start_geometries, arg);
		break;

	case USERFILE_KEY:
		/* This is a special key that tells the program to load
		 * the user's calendar file.  This allows session management
		 * to work even if the User's home directory changes location
		 * (ie, on a networked setup).
		 */
		arg = COOKIE_USER_HOME_DIR;
		/* fall through */

	case VIEW_KEY:
		start_views = g_list_append (start_views, arg);
		break;

	case 'F':
		start_calendars = g_list_append (start_calendars, arg);
		break;

	case 'e':
		show_events = 1;
		break;

	case HIDDEN_KEY:
		startup_hidden = 1;
		break;
		
	default:
	}
}

static const struct poptOption options [] = {
	{ NULL, '\0', POPT_ARG_CALLBACK, parse_an_arg, 0, NULL, NULL },
	{ "events", 'e', POPT_ARG_NONE, NULL, 'e', N_("Show events and quit"),
	  NULL },
	{ "from", 'f', POPT_ARG_STRING, NULL, 'f', N_("Specifies start date [for --events]"), N_("DATE") },
	{ "file", 'F', POPT_ARG_STRING, NULL, 'F', N_("File to load calendar from"), N_("FILE") },
	{ "userfile", '\0', POPT_ARG_NONE, NULL, USERFILE_KEY, N_("Load the user calendar"), NULL },
	{ "geometry", '\0', POPT_ARG_STRING, NULL, GEOMETRY_KEY, N_("Geometry for starting up"), N_("GEOMETRY") },
	{ "view", '\0', POPT_ARG_STRING, NULL, VIEW_KEY, N_("The startup view mode"), N_("VIEW") },
	{ "to", 't', POPT_ARG_STRING, NULL, 't', N_("Specifies ending date [for --events]"), N_("DATE") },
	{ "hidden", 0, POPT_ARG_NONE, NULL, HIDDEN_KEY, N_("If used, starts in iconic mode"), NULL },
	{ NULL, '\0', 0, NULL, 0}
};

static void
session_die (void)
{
	quit_cmd ();
}

/*
 * Save the session callback
 */
static int
session_save_state (GnomeClient *client, gint phase, GnomeRestartStyle save_style, gint shutdown,
		    GnomeInteractStyle  interact_style, gint fast, gpointer client_data)
{
	char *sess_id;
	char **argv = (char **) g_malloc (sizeof (char *) * ((active_calendars * 6) + 3));
	GList *l, *free_list = 0;
	int   i;

	sess_id = gnome_client_get_id (client);

	argv [0] = client_data;
	for (i = 1, l = all_calendars; l; l = l->next){
		GnomeCalendar *gcal = GNOME_CALENDAR (l->data);
		char *geometry;

		geometry = gnome_geometry_string (GTK_WIDGET (gcal)->window);

		if (strcmp (gcal->cal->filename, user_calendar_file) == 0)
			argv [i++] = "--userfile";
		else {
			argv [i++] = "--file";
			argv [i++] = gcal->cal->filename;
		}
		argv [i++] = "--geometry";
		argv [i++] = geometry;
		argv [i++] = "--view";
		argv [i++] = gnome_calendar_get_current_view_name (gcal);
		free_list = g_list_append (free_list, geometry);
		calendar_save (gcal->cal, gcal->cal->filename);
	}
	argv [i] = NULL;
	gnome_client_set_clone_command (client, i, argv);
	gnome_client_set_restart_command (client, i, argv);

	for (l = free_list; l; l = l->next)
		g_free (l->data);
	g_list_free (free_list);

	return 1;
}

int
main(int argc, char *argv[])
{
	GnomeClient *client;

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);

	gnome_init_with_popt_table ("calendar", VERSION, argc, argv,
				    options, 0, NULL);

	if (show_events)
		dump_events ();

	client = gnome_master_client ();
	if (client){
		gtk_signal_connect (GTK_OBJECT (client), "save_yourself",
				    GTK_SIGNAL_FUNC (session_save_state), argv [0]);
		gtk_signal_connect (GTK_OBJECT (client), "die",
				    GTK_SIGNAL_FUNC (session_die), NULL);
	}

	process_dates ();
	alarm_init ();
	init_calendar ();

	/*
	 * Load all of the calendars specifies in the command line with
	 * the geometry specificied -if any-
	 */
	if (start_calendars){
		GList *p, *g, *v;
		char *title;

		p = start_calendars;
		g = start_geometries;
		v = start_views;
		while (p){
			char *file = p->data;
			char *geometry = g ? g->data : NULL;
			char *page_name = v ? v->data : NULL;

			if (file == COOKIE_USER_HOME_DIR)
				file = user_calendar_file;

			if (strcmp (file, user_calendar_file) == 0)
				title = full_name;
			else
				title = file;
			new_calendar (title, file, geometry, page_name, startup_hidden);

			p = p->next;
			if (g)
				g = g->next;
			if (v)
				v = v->next;
		}
		g_list_free (p);
	} else {
		char *geometry = start_geometries ? start_geometries->data : NULL;
		char *page_name = start_views ? start_views->data : NULL;

		new_calendar (full_name, user_calendar_file, geometry, page_name, startup_hidden);
	}
	gtk_main ();
	return 0;
}
