/*
 * GnomeCalendar widget
 * Copyright (C) 1998 the Free Software Foundation
 *
 * Authors: 
 *          Miguel de Icaza (miguel@kernel.org)
 */
#include <config.h>
#include <gnome.h>
#include <pwd.h>
#include <sys/types.h>
#include "calendar.h"
#include "gnome-cal.h"

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

void
init_username (void)
{
	char *p;
	struct passwd *passwd;
	
	passwd = getpwuid (getuid ());
	if ((p = passwd->pw_name)){
		user_name = g_strdup (p);
		full_name = g_strdup (passwd->pw_gecos);
	} else {
		if ((p = getenv ("USER"))){
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

int
range_check_hour (int hour)
{
	if (hour < 0)
		hour = 0;
	if (hour > 24)
		hour = 23;
	return hour;
}

/*
 * Initializes the calendar internal variables, loads defaults
 */
void
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

void
new_calendar_cmd (GtkWidget *widget, void *data)
{
}

void
open_calendar_cmd (GtkWidget *widget, void *data)
{
}

void
save_calendar_cmd (GtkWidget *widget, void *data)
{
}

void
about_calendar_cmd (GtkWidget *widget, void *data)
{

        GtkWidget *about;
        gchar *authors[] = {
		"Miguel de Icaza (miguel@kernel.org)",
		"Federico Mena (federico@gimp.org)",
		NULL
	};

        about = gnome_about_new (_("Gnome Calendar"), VERSION,
				 "(C) 1998 the Free Software Fundation",
				 authors,
				 _("The GNOME personal calendar and schedule manager."),
				 NULL);
        gtk_widget_show (about);
}

void
quit_cmd (GtkWidget *widget, GnomeCalendar *gcal)
{
	/* FIXME: check all of the calendars for their state (modified) */
	
	gtk_main_quit ();
}

void
close_cmd (GtkWidget *widget, GnomeCalendar *gcal)
{
	if (gcal->cal->modified){
		gnome_message_box_new (_("The calendar has unsaved changes, Save them?"),
				       GNOME_MESSAGE_BOX_WARNING,
				       "Yes", "No");
	}
	gtk_widget_destroy (widget);
	active_calendars--;

	if (active_calendars == 0)
		gtk_main_quit ();
}

GnomeUIInfo gnome_cal_file_menu [] = {
	{ GNOME_APP_UI_ITEM, N_("New calendar"),  NULL, new_calendar_cmd },

	{ GNOME_APP_UI_ITEM, N_("Open calendar"), NULL, open_calendar_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_OPEN },

	{ GNOME_APP_UI_ITEM, N_("Save calendar"), NULL, save_calendar_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_SAVE },
	
	{ GNOME_APP_UI_SEPARATOR },	
	{ GNOME_APP_UI_ITEM, N_("Close"), NULL, close_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_EXIT },
	
	{ GNOME_APP_UI_ITEM, N_("Exit"), NULL, quit_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_EXIT },
	
	GNOMEUIINFO_END
};

GnomeUIInfo gnome_cal_about_menu [] = {
	{ GNOME_APP_UI_ITEM, N_("About"), NULL, about_calendar_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_ABOUT },
	GNOMEUIINFO_HELP ("cal"),
	GNOMEUIINFO_END
};

GnomeUIInfo gnome_cal_menu [] = {
	{ GNOME_APP_UI_SUBTREE, N_("File"),     NULL, &gnome_cal_file_menu },
	{ GNOME_APP_UI_SUBTREE, N_("Help"),     NULL, &gnome_cal_about_menu },
	GNOMEUIINFO_END
};

GnomeUIInfo gnome_toolbar [] = {
	{ GNOME_APP_UI_ITEM, N_("Prev"), N_("Previous"), /*previous_clicked*/0, 0, 0,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_BACK },
	
	{ GNOME_APP_UI_ITEM, N_("Today"), N_("Today"), /*previous_clicked*/0, 0, 0,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_BACK },
	
	{ GNOME_APP_UI_ITEM, N_("Next"), N_("Next"), /*previous_clicked*/0, 0, 0,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_FORWARD },

	GNOMEUIINFO_END
};

static void
setup_menu (GtkWidget *gcal)
{
	gnome_app_create_menus_with_data (GNOME_APP (gcal), gnome_cal_menu, gcal);
	gnome_app_create_toolbar_with_data (GNOME_APP (gcal), gnome_toolbar, gcal);
	
}

static void
new_calendar (char *full_name, char *calendar_file)
{
	GtkWidget   *toplevel;
	char        *title;
	
	title = g_copy_strings (full_name, "'s calendar", NULL);
	
	toplevel = gnome_calendar_new (title);
	setup_menu (toplevel);
	gtk_widget_show (toplevel);

	if (g_file_exists (calendar_file)){
		printf ("Trying to load %s\n", calendar_file);
		gnome_calendar_load (GNOME_CALENDAR (toplevel), calendar_file);
	} else {
		printf ("tring: ./test.vcf\n");
		gnome_calendar_load (GNOME_CALENDAR (toplevel), "./test.vcf");
	}
	active_calendars++;
}
	
int 
main(int argc, char *argv[])
{
	GnomeClient *client;
	
	argp_program_version = VERSION;

	/* Initialise the i18n stuff */
	bindtextdomain(PACKAGE, GNOMELOCALEDIR);
	textdomain(PACKAGE);

	gnome_init ("gncal", NULL, argc, argv, 0, NULL);

	init_calendar ();

	new_calendar (full_name, user_calendar_file);
	gtk_main ();
}


