/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Main file for the GNOME Calendar program
 * Copyright (C) 1998 the Free Software Foundation
 *
 * Authors:
 *          Miguel de Icaza (miguel@kernel.org)
 *          Federico Mena (federico@helixcode.com)
 */

#include <config.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <gnome.h>
#include <libgnorba/gnorba.h>
#include <bonobo.h>
#include <cal-util/timeutil.h>
#include "gnome-cal.h"
#include "calendar-commands.h"
#include "print.h"
#include "dialogs/cal-prefs-dialog.h"

#include "dayview.xpm"
#include "workweekview.xpm"
#include "weekview.xpm"
#include "monthview.xpm"
#if 0
#include "yearview.xpm"
#endif


/* The username, used to set the `owner' field of the event */
char *user_name;

/* The full user name from the Gecos field */
char *full_name;

/* a gnome-config string prefix that can be used to access the calendar config info */
char *calendar_settings;

/* Day begin, day end parameters */
int day_begin, day_end;

/* Whether weeks starts on Sunday or Monday */
int week_starts_on_monday;

/* If AM/PM indicators should be used. This may not be supported by the new
   views. */
int am_pm_flag = 0;

/* The array of color properties -- keep in sync with the enumeration defined in main.h.  The color
 * values specified here are the defaults for the program.
 */
struct color_prop color_props[] = {
	{ 0x3e72, 0x35ec, 0x8ba2, N_("Outline:"),              "/calendar/Colors/outline" },
	{ 0xffff, 0xffff, 0xffff, N_("Headings:"),             "/calendar/Colors/headings" },
	{ 0xf26c, 0xecec, 0xbbe7, N_("Empty days:"),           "/calendar/Colors/empty_bg" },
	{ 0xfc1e, 0xf87f, 0x5f80, N_("Appointments:"),         "/calendar/Colors/mark_bg" },
	{ 0xd364, 0xc6b7, 0x7969, N_("Highlighted day:"),      "/calendar/Colors/prelight_bg" },
	{ 0x01f0, 0x01f0, 0x01f0, N_("Day numbers:"),          "/calendar/Colors/day_fg" },
	{ 0x0000, 0x0000, 0xffff, N_("Current day's number:"), "/calendar/Colors/current_fg" },
	{ 0xbbbb, 0xbbbb, 0x0000, N_("To-Do item that is not yet due:"), "/calendar/Colors/todo_not_yet" },
	{ 0xdddd, 0xbbbb, 0x0000, N_("To-Do item that is due today:"),   "/calendar/Colors/todo_today" },
	{ 0xbbbb, 0xdddd, 0x0000, N_("To-Do item that is overdue:"),     "/calendar/Colors/todo_overdue" }
};

/* A list of all of the calendars started */
static GList *all_calendars = NULL;

/* If set, beep on display alarms */
gboolean beep_on_display = 0;

/* If true, timeout the beeper on audio alarms */

gboolean enable_aalarm_timeout = 0;
guint audio_alarm_timeout = 0;
const guint MAX_AALARM_TIMEOUT = 3600;
const guint MAX_SNOOZE_SECS = 3600;
gboolean enable_snooze = 0;
guint snooze_secs = 60;

#if 0
CalendarAlarm alarm_defaults[4] = {
        { ALARM_MAIL, 0, 15, ALARM_MINUTES },
        { ALARM_PROGRAM, 0, 15, ALARM_MINUTES },
        { ALARM_DISPLAY, 0, 15, ALARM_MINUTES },
        { ALARM_AUDIO, 0, 15, ALARM_MINUTES }
};
#endif

/* We have one global preferences dialog. */
static CalPrefsDialog *preferences_dialog = NULL;


static void update_pixmaps	(BonoboUIComponent *uic);
static void set_pixmap		(BonoboUIComponent *uic,
				 const char        *xml_path,
				 char		  **xpm_data);



static void
init_username (void)
{
        user_name = g_strdup(g_get_user_name());
        full_name = g_strdup(g_get_real_name());
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

#if 0
static void
init_default_alarms (void)
{
	int i;
	gboolean def;

	alarm_defaults [ALARM_DISPLAY].type = ALARM_DISPLAY;
	alarm_defaults [ALARM_AUDIO].type   = ALARM_AUDIO;
	alarm_defaults [ALARM_PROGRAM].type = ALARM_PROGRAM;
	alarm_defaults [ALARM_MAIL].type    = ALARM_MAIL;

	for (i = 0; i < 4; i++) {
		switch (alarm_defaults [i].type) {
		case ALARM_DISPLAY:
			gnome_config_push_prefix ("/calendar/alarms/def_disp_");
			break;
		case ALARM_AUDIO:
			gnome_config_push_prefix ("/calendar/alarms/def_audio_");
			break;
		case ALARM_PROGRAM:
			gnome_config_push_prefix ("/calendar/alarms/def_prog_");
			break;
		case ALARM_MAIL:
			gnome_config_push_prefix ("/calendar/alarms/def_mail_");
			break;
		}

		alarm_defaults[i].enabled = gnome_config_get_int ("enabled=0");
		if (alarm_defaults[i].type != ALARM_MAIL) {
			alarm_defaults[i].count   = gnome_config_get_int ("count=15");
			alarm_defaults[i].units   = gnome_config_get_int ("units=0");
		} else {
			alarm_defaults[i].count   = gnome_config_get_int ("count=1");
			alarm_defaults[i].count   = gnome_config_get_int ("count=2");
		}

		alarm_defaults[i].data = gnome_config_get_string_with_default ("data=",
									       &def);
		if (def)
			alarm_defaults[i].data = NULL;

		gnome_config_pop_prefix ();
	}
}
#endif

/* Callback for the new appointment command */
static void
new_appointment_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (user_data);
	gnome_calendar_new_appointment (gcal);
}

/* Prints the calendar at its current view and time range */
static void
print (GnomeCalendar *gcal, gboolean preview)
{
	time_t start;
	const char *view;
	PrintView print_view;

	gnome_calendar_get_current_time_range (gcal, &start, NULL);
	view = gnome_calendar_get_current_view_name (gcal);

	if (strcmp (view, "dayview") == 0)
		print_view = PRINT_VIEW_DAY;
	else if (strcmp (view, "workweekview") == 0 || strcmp (view, "weekview") == 0)
		print_view = PRINT_VIEW_WEEK;
	else if (strcmp (view, "monthview") == 0)
		print_view = PRINT_VIEW_MONTH;
	else {
		g_assert_not_reached ();
		print_view = PRINT_VIEW_DAY;
	}

	print_calendar (gcal, preview, start, print_view);
}

/* File/Print callback */
static void
file_print_cb (BonoboUIComponent *uih, void *data, const char *path)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);
	print (gcal, FALSE);
}


/* This iterates over each calendar telling them to update their config
   settings. */
void
update_all_config_settings (void)
{
	GList *l;

	for (l = all_calendars; l; l = l->next)
		gnome_calendar_update_config_settings (GNOME_CALENDAR (l->data), FALSE);
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
previous_clicked (BonoboUIComponent *uih, void *user_data, const char *path)
{
	GnomeCalendar *gcal = GNOME_CALENDAR (user_data);
	set_clock_cursor (gcal);
	gnome_calendar_previous (gcal);
	set_normal_cursor (gcal);
}

static void
next_clicked (BonoboUIComponent *uih, void *user_data, const char *path)
{
	GnomeCalendar *gcal = GNOME_CALENDAR (user_data);
	set_clock_cursor (gcal);
	gnome_calendar_next (gcal);
	set_normal_cursor (gcal);
}

static void
today_clicked (BonoboUIComponent *uih, void *user_data, const char *path)
{
	GnomeCalendar *gcal = GNOME_CALENDAR (user_data);
	set_clock_cursor (gcal);
	gnome_calendar_goto_today (gcal);
	set_normal_cursor (gcal);
}

static void
goto_clicked (BonoboUIComponent *uih, void *user_data, const char *path)
{
	GnomeCalendar *gcal = GNOME_CALENDAR (user_data);
	goto_dialog (gcal);
}

static void
show_day_view_clicked (BonoboUIComponent *uih, void *user_data, const char *path)
{
	GnomeCalendar *gcal = GNOME_CALENDAR (user_data);

	gnome_calendar_set_view (gcal, "dayview", FALSE, TRUE);
}

static void
show_work_week_view_clicked (BonoboUIComponent *uih, void *user_data, const char *path)
{
	GnomeCalendar *gcal = GNOME_CALENDAR (user_data);

	gnome_calendar_set_view (gcal, "workweekview", FALSE, TRUE);
}

static void
show_week_view_clicked (BonoboUIComponent *uih, void *user_data, const char *path)
{
	GnomeCalendar *gcal = GNOME_CALENDAR (user_data);

	gnome_calendar_set_view (gcal, "weekview", FALSE, TRUE);
}

static void
show_month_view_clicked (BonoboUIComponent *uih, void *user_data, const char *path)
{
	GnomeCalendar *gcal = GNOME_CALENDAR (user_data);

	gnome_calendar_set_view (gcal, "monthview", FALSE, TRUE);
}


static void
new_calendar_cmd (BonoboUIComponent *uih, void *user_data, const char *path)
{
	new_calendar (full_name);
}

static void
close_cmd (BonoboUIComponent *uih, void *user_data, const char *path)
{
	GnomeCalendar *gcal = GNOME_CALENDAR (user_data);
	all_calendars = g_list_remove (all_calendars, gcal);

	gtk_widget_destroy (GTK_WIDGET (gcal));

	if (all_calendars == NULL)
		gtk_main_quit ();
}


void
quit_cmd (BonoboUIComponent *uih, void *user_data, const char *path)
{
	while (all_calendars){
		GnomeCalendar *cal = GNOME_CALENDAR (all_calendars->data);

		close_cmd (uih, cal, path);
	}
}


static void
open_ok (GtkWidget *widget, GtkFileSelection *fs)
{
	GtkWidget *error_dialog;
	int ret;
	if(!g_file_exists (gtk_file_selection_get_filename (fs))) {
		error_dialog = gnome_message_box_new (
			_("File not found"),
			GNOME_MESSAGE_BOX_ERROR,
			GNOME_STOCK_BUTTON_OK,
			NULL);

		gnome_dialog_set_parent (GNOME_DIALOG (error_dialog), GTK_WINDOW (fs));
		ret = gnome_dialog_run (GNOME_DIALOG (error_dialog));
	} else {
		/* FIXME: find out who owns this calendar and use that name */
#ifndef NO_WARNINGS
#warning "FIXME: find out who owns this calendar and use that name"
#endif
		/*
		new_calendar ("Somebody", gtk_file_selection_get_filename (fs));
		*/
		gtk_widget_destroy (GTK_WIDGET (fs));
	}
}

static void
open_calendar_cmd (BonoboUIComponent *uih, void *user_data, const char *path)
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
	gchar *fname;

	gcal = GNOME_CALENDAR (gtk_object_get_user_data (GTK_OBJECT (fs)));
	gtk_window_set_wmclass (GTK_WINDOW (gcal), "gnomecal", "gnomecal");

	fname = g_strdup (gtk_file_selection_get_filename (fs));
	g_free(fname);
	gtk_main_quit ();
}

static gint
close_save (GtkWidget *w)
{
	gtk_main_quit ();
	return TRUE;
}

static void
save_as_calendar_cmd (BonoboUIComponent *uih, void *user_data, const char *path)
{
	GtkFileSelection *fs;

	fs = GTK_FILE_SELECTION (gtk_file_selection_new (_("Save calendar")));
	gtk_object_set_user_data (GTK_OBJECT (fs), user_data);

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
properties_cmd (BonoboUIComponent *uih, void *user_data, const char *path)
{
	if (!preferences_dialog)
		preferences_dialog = cal_prefs_dialog_new ();
	else
		cal_prefs_dialog_show (preferences_dialog);
}


static BonoboUIVerb verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("CalendarNew", new_calendar_cmd),
	BONOBO_UI_UNSAFE_VERB ("CalendarOpen", open_calendar_cmd),
	BONOBO_UI_UNSAFE_VERB ("CalendarSaveAs", save_as_calendar_cmd),
	BONOBO_UI_UNSAFE_VERB ("CalendarPrint", file_print_cb),
	BONOBO_UI_UNSAFE_VERB ("EditNewAppointment", new_appointment_cb),
	BONOBO_UI_UNSAFE_VERB ("CalendarPreferences", properties_cmd),

	BONOBO_UI_UNSAFE_VERB ("CalendarPrev", previous_clicked),
	BONOBO_UI_UNSAFE_VERB ("CalendarToday", today_clicked),
	BONOBO_UI_UNSAFE_VERB ("CalendarNext", next_clicked),
	BONOBO_UI_UNSAFE_VERB ("CalendarGoto", goto_clicked),

	BONOBO_UI_UNSAFE_VERB ("ShowDayView", show_day_view_clicked),
	BONOBO_UI_UNSAFE_VERB ("ShowWorkWeekView", show_work_week_view_clicked),
	BONOBO_UI_UNSAFE_VERB ("ShowWeekView", show_week_view_clicked),
	BONOBO_UI_UNSAFE_VERB ("ShowMonthView", show_month_view_clicked),

	BONOBO_UI_VERB_END
};

void
calendar_control_activate (BonoboControl *control,
			   GnomeCalendar *cal)
{
	Bonobo_UIContainer remote_uih;
	BonoboUIComponent *uic;

	uic = bonobo_control_get_ui_component (control);
	g_assert (uic != NULL);

	remote_uih = bonobo_control_get_remote_ui_container (control);
	bonobo_ui_component_set_container (uic, remote_uih);
	bonobo_object_release_unref (remote_uih, NULL);

#if 0
	/* FIXME: Need to update this to use new Bonobo ui stuff somehow.
	   Also need radio buttons really. */

	/* Note that these indices should correspond with the button indices
	   in the gnome_toolbar_view_buttons UIINFO struct. */
	gnome_calendar_set_view_buttons (cal,
					 gnome_toolbar_view_buttons[0].widget,
					 gnome_toolbar_view_buttons[1].widget,
					 gnome_toolbar_view_buttons[2].widget,
					 gnome_toolbar_view_buttons[3].widget);

	/* This makes the appropriate radio button in the toolbar active. */
	gnome_calendar_update_view_buttons (cal);
#endif
	
	bonobo_ui_component_add_verb_list_with_data (
		uic, verbs, cal);

	bonobo_ui_component_freeze (uic, NULL);

	bonobo_ui_util_set_ui (uic, EVOLUTION_DATADIR,
			       "evolution-calendar.xml",
			       "evolution-calendar");

	update_pixmaps (uic);

	bonobo_ui_component_thaw (uic, NULL);
}


static void
update_pixmaps (BonoboUIComponent *uic)
{
	set_pixmap (uic, "/Toolbar/DayView",	  dayview_xpm);
	set_pixmap (uic, "/Toolbar/WorkWeekView", workweekview_xpm);
	set_pixmap (uic, "/Toolbar/WeekView",	  weekview_xpm);
	set_pixmap (uic, "/Toolbar/MonthView",	  monthview_xpm);
}


static void
set_pixmap (BonoboUIComponent *uic,
	    const char        *xml_path,
	    char	     **xpm_data)
{
	GdkPixbuf *pixbuf;

	pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **) xpm_data);
	g_return_if_fail (pixbuf != NULL);

	bonobo_ui_util_set_pixbuf (uic, xml_path, pixbuf);

	gdk_pixbuf_unref (pixbuf);
}


void
calendar_control_deactivate (BonoboControl *control)
{
	BonoboUIComponent *uic = bonobo_control_get_ui_component (control);
	g_assert (uic != NULL);

	bonobo_ui_component_rm (uic, "/", NULL);
 	bonobo_ui_component_unset_container (uic);
}

GnomeCalendar *
new_calendar (char *full_name)
{
	GtkWidget *gcal;

	gcal = gnome_calendar_new ();

	all_calendars = g_list_prepend (all_calendars, gcal);

	gtk_widget_show (gcal);
	return GNOME_CALENDAR (gcal);
}


void calendar_set_uri (GnomeCalendar *gcal, char *calendar_file)
{
	g_return_if_fail (gcal);
	g_return_if_fail (calendar_file);

	gnome_calendar_open (gcal,
			     calendar_file,
			     CALENDAR_OPEN_OR_CREATE);
}





/*
 * Initializes the calendar internal variables, loads defaults
 */
void
init_calendar (void)
{
	int i;
	char *cspec, *color;
	char *str;

	init_username ();
	/*user_calendar_file = g_concat_dir_and_file (gnome_util_user_home (), ".gnome/user-cal.vcf");*/

	gnome_config_push_prefix (calendar_settings);

	/* Read calendar settings */

	day_begin  = range_check_hour (gnome_config_get_int  ("/calendar/Calendar/Day start=8"));
	day_end    = range_check_hour (gnome_config_get_int  ("/calendar/Calendar/Day end=17"));
	am_pm_flag = gnome_config_get_bool ("/calendar/Calendar/AM PM flag=0");
	week_starts_on_monday = gnome_config_get_bool ("/calendar/Calendar/Week starts on Monday=0");

	if (day_end < day_begin) {
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

	/* read alarm settings */
	beep_on_display = gnome_config_get_bool ("/calendar/alarms/beep_on_display=FALSE");
	enable_aalarm_timeout = gnome_config_get_bool ("/calendar/alarms/enable_audio_timeout=FALSE");
	audio_alarm_timeout = gnome_config_get_int ("/calendar/alarms/audio_alarm_timeout=60");
	if (audio_alarm_timeout < 1)
		audio_alarm_timeout = 1;
	if (audio_alarm_timeout > MAX_AALARM_TIMEOUT)
		audio_alarm_timeout = MAX_AALARM_TIMEOUT;
	enable_snooze = gnome_config_get_bool ("/calendar/alarms/enable_snooze=FALSE");
	snooze_secs = gnome_config_get_int ("/calendar/alarms/snooze_secs=300");
	if (snooze_secs < 1)
		snooze_secs = 1;
	if (snooze_secs > MAX_SNOOZE_SECS)
		snooze_secs = MAX_SNOOZE_SECS;

#if 0
	init_default_alarms ();
#endif

	/* Done */

	gnome_config_pop_prefix ();
}
