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
#include "alarm.h"
#include "eventedit.h"
#include "gnome-cal.h"
#include "calendar-commands.h"


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

/* If true, enable debug output for alarms */
int debug_alarms = 0;


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

/* Number of active calendars */
int active_calendars = 0;

/* A list of all of the calendars started */
GList *all_calendars = NULL;

/* If set, beep on display alarms */
gboolean beep_on_display = 0;

/* If true, timeout the beeper on audio alarms */

gboolean enable_aalarm_timeout = 0;
guint audio_alarm_timeout = 0;
const guint MAX_AALARM_TIMEOUT = 3600;
const guint MAX_SNOOZE_SECS = 3600;
gboolean enable_snooze = 0;
guint snooze_secs = 60;

/*extern CalendarAlarm alarm_defaults[4];*/
CalendarAlarm alarm_defaults[4] = {
        { ALARM_MAIL, 0, 15, ALARM_MINUTES },
        { ALARM_PROGRAM, 0, 15, ALARM_MINUTES },
        { ALARM_DISPLAY, 0, 15, ALARM_MINUTES },
        { ALARM_AUDIO, 0, 15, ALARM_MINUTES }
};


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

	
static void
about_calendar_cmd (BonoboUIHandler *uih, void *user_data, const char *path)
{
        GtkWidget *about;
        const gchar *authors[] = {
		"Miguel de Icaza (miguel@kernel.org)",
		"Federico Mena (federico@gimp.org)",
		"Arturo Espinosa (arturo@nuclecu.unam.mx)",
		"Russell Steinthal (rms39@columbia.edu)",
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
display_objedit (BonoboUIHandler *uih, void *user_data, const char *path)
{
	GtkWidget *ee;
	iCalObject *ico;
	GnomeCalendar *gcal = GNOME_CALENDAR (user_data);

	/* Default to the day the user is looking at */
	ico = ical_new ("", user_name, "");
	ico->new = 1;
	ico->dtstart = time_add_minutes (gcal->current_display, day_begin * 60);
	ico->dtend   = time_add_minutes (ico->dtstart, day_begin * 60 + 30 );

	ee = event_editor_new (gcal, ico);
	gtk_widget_show (ee);
}

static void
display_objedit_today (BonoboUIHandler *uih, void *user_data, const char *path)
{
	GtkWidget *ee;
	GnomeCalendar *gcal = GNOME_CALENDAR (user_data);

	ee = event_editor_new (gcal, NULL);
	gtk_widget_show (ee);
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

void 
todo_properties_changed(void) 
{
        GList *l;
	
	for (l = all_calendars; l; l = l->next)
		gnome_calendar_todo_properties_changed (GNOME_CALENDAR (l->data));
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
previous_clicked (BonoboUIHandler *uih, void *user_data, const char *path)
{
	GnomeCalendar *gcal = GNOME_CALENDAR (user_data);
	set_clock_cursor (gcal);
	gnome_calendar_previous (gcal);
	set_normal_cursor (gcal);
}

static void
next_clicked (BonoboUIHandler *uih, void *user_data, const char *path)
{
	GnomeCalendar *gcal = GNOME_CALENDAR (user_data);
	set_clock_cursor (gcal);
	gnome_calendar_next (gcal);
	set_normal_cursor (gcal);
}

static void
today_clicked (BonoboUIHandler *uih, void *user_data, const char *path)
{
	GnomeCalendar *gcal = GNOME_CALENDAR (user_data);
	set_clock_cursor (gcal);
	gnome_calendar_goto_today (gcal);
	set_normal_cursor (gcal);
}

static void
goto_clicked (BonoboUIHandler *uih, void *user_data, const char *path)
{
	GnomeCalendar *gcal = GNOME_CALENDAR (user_data);
	goto_dialog (gcal);
}

static void
new_calendar_cmd (BonoboUIHandler *uih, void *user_data, const char *path)
{
	new_calendar (full_name, NULL, NULL, NULL, FALSE);
}

static void
close_cmd (BonoboUIHandler *uih, void *user_data, const char *path)
{
	GnomeCalendar *gcal = GNOME_CALENDAR (user_data);
	all_calendars = g_list_remove (all_calendars, gcal);

	/* DELETE
	   FIXME -- what do i do here?
	if (gcal->cal->modified){
		if (!gcal->cal->filename)
			save_calendar_cmd (widget, gcal);
		else
			calendar_save (gcal->cal, gcal->cal->filename);
	}
	*/

	gtk_widget_destroy (GTK_WIDGET (gcal));
	active_calendars--;

	if (active_calendars == 0)
		gtk_main_quit ();
}


void
quit_cmd (BonoboUIHandler *uih, void *user_data, const char *path)
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
		new_calendar ("Somebody", gtk_file_selection_get_filename (fs), NULL, NULL, FALSE);
		gtk_widget_destroy (GTK_WIDGET (fs));
	}
}

static void
open_calendar_cmd (BonoboUIHandler *uih, void *user_data, const char *path)
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
save_as_calendar_cmd (BonoboUIHandler *uih, void *user_data, const char *path)
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
properties_cmd (BonoboUIHandler *uih, void *user_data, const char *path)
{
	GnomeCalendar *gcal = GNOME_CALENDAR (user_data);
	properties (GTK_WIDGET (gcal));
}


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



/* Performs signal connection as appropriate for interpreters or native bindings */
static void
do_ui_signal_connect (GnomeUIInfo *uiinfo, gchar *signal_name, 
		      GnomeUIBuilderData *uibdata)
{
	if (uibdata->is_interp)
		gtk_signal_connect_full (GTK_OBJECT (uiinfo->widget),
				signal_name, NULL, uibdata->relay_func,
				uibdata->data ?
				uibdata->data : uiinfo->user_data,
				uibdata->destroy_func, FALSE, FALSE);

	else if (uiinfo->moreinfo)
		gtk_signal_connect (GTK_OBJECT (uiinfo->widget),
				signal_name, uiinfo->moreinfo, uibdata->data ?
				uibdata->data : uiinfo->user_data);
}


void
calendar_control_activate (BonoboControl *control,
			   GnomeCalendar *cal)
{
	Bonobo_UIHandler  remote_uih;
	GtkWidget *toolbar;
	GnomeUIBuilderData uibdata;
	BonoboUIHandler *uih = bonobo_control_get_ui_handler (control);
	g_assert (uih);

	uibdata.connect_func = do_ui_signal_connect;
	uibdata.data = cal;
	uibdata.is_interp = FALSE;
	uibdata.relay_func = NULL;
	uibdata.destroy_func = NULL;

	remote_uih = bonobo_control_get_remote_ui_handler (control);
	bonobo_ui_handler_set_container (uih, remote_uih);

	toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL,
				   GTK_TOOLBAR_BOTH);
	gnome_app_fill_toolbar_custom (GTK_TOOLBAR (toolbar),
				       gnome_toolbar, &uibdata, 
				       /*app->accel_group*/ NULL);

	gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

	gtk_widget_show_all (toolbar);

	bonobo_ui_handler_dock_add (uih, "/Toolbar",
				    bonobo_object_corba_objref (BONOBO_OBJECT (bonobo_control_new (toolbar))),
				    GNOME_DOCK_ITEM_BEH_LOCKED |
				    GNOME_DOCK_ITEM_BEH_EXCLUSIVE,
				    GNOME_DOCK_TOP,
				    1, 1, 0);


	/* file menu */
	bonobo_ui_handler_menu_new_item (uih, "/File/New/Calendar", N_("New Ca_lendar"),
					 N_("Create a new calendar"),
					 -1, BONOBO_UI_HANDLER_PIXMAP_NONE,
					 NULL, 0, 0, new_calendar_cmd, cal);
	bonobo_ui_handler_menu_new_item (uih, "/File/Open/Calendar", N_("Open Ca_lendar"),
					 N_("Open a calendar"), -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 0, 0, open_calendar_cmd, cal);
	bonobo_ui_handler_menu_new_item (uih, "/File/Save Calendar As",
					 N_("Save Calendar As"),
					 N_("Save Calendar As"),
					 -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 0, 0, save_as_calendar_cmd, cal);
	bonobo_ui_handler_menu_new_item (uih, "/File/Close", N_("_Close Calendar"),
					 N_("Close current calendar"),
					 -1, BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 0, 0, close_cmd, cal);
	/*bonobo_ui_handler_menu_new_item (uih, "/File/Exit",
					 N_("_Exit"), N_("Exit"),
					 -1, BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 0, 0, quit_cmd, cal);	*/

	/* edit menu */
	bonobo_ui_handler_menu_new_item (uih, "/Edit/New Appointment",
					 N_("_New appointment..."), N_("Create a new appointment"),
					 -1, BONOBO_UI_HANDLER_PIXMAP_STOCK,
					 GNOME_STOCK_MENU_NEW, 0, 0,
					 display_objedit, cal);
	bonobo_ui_handler_menu_new_item (uih, "/Edit/New Appointment for today",
					 N_("New appointment for _today..."),
					 N_("Create a new appointment for today"),
					 -1, BONOBO_UI_HANDLER_PIXMAP_STOCK,
					 GNOME_STOCK_MENU_NEW, 0, 0,
					 display_objedit_today, cal);

	//bonobo_ui_handler_menu_new_separator (uih, "/Edit", -1);

	bonobo_ui_handler_menu_new_item (uih, "/Edit/Preferences",
					 N_("Preferences"), N_("Preferences"),
					 -1, BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 0, 0, properties_cmd, cal);
	/* help menu */

	bonobo_ui_handler_menu_new_item (uih,
					 "/Help/About Calendar",
					 N_("About Calendar"),
					 N_("About Calendar"),
					 -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 0, 0, about_calendar_cmd, cal);

}


void
calendar_control_deactivate (BonoboControl *control)
{
	BonoboUIHandler *uih = bonobo_control_get_ui_handler (control);
	g_assert (uih);
	bonobo_ui_handler_dock_remove (uih, "/Toolbar");
 	bonobo_ui_handler_unset_container (uih);
}




static gint
calendar_close_event (GtkWidget *widget, GdkEvent *event, GnomeCalendar *gcal)
{
	close_cmd (NULL, gcal, NULL);
	return TRUE;
}

GnomeCalendar *
new_calendar (char *full_name, char *calendar_file, char *geometry, char *page, gboolean hidden)
{
	GtkWidget   *toplevel;
	char        title[128];
	int         xpos, ypos, width, height;
	gboolean    success;

	/* i18n: This "%s%s" indicates possession. Languages where the order is
	 * the inverse should translate it to "%2$s%1$s".
	 */
	g_snprintf(title, 128, _("%s%s"), full_name, _("'s calendar"));

	toplevel = gnome_calendar_new (title);
	    
	if (gnome_parse_geometry (geometry, &xpos, &ypos, &width, &height)){
		if (xpos != -1)
			gtk_widget_set_uposition (toplevel, xpos, ypos);
		/*if (width != -1)
		  gtk_widget_set_usize (toplevel, width, 600);*/
	}
 	/*gtk_widget_set_usize (toplevel, width, 600); */

	/*
	setup_appbar (toplevel);
	setup_menu (toplevel);
	*/


	if (page)
		gnome_calendar_set_view (GNOME_CALENDAR (toplevel), page);

	printf ("calendar_file is '%s'\n", calendar_file?calendar_file:"NULL");
	if (calendar_file && g_file_exists (calendar_file)) {
		printf ("loading calendar\n");
		success = gnome_calendar_load (GNOME_CALENDAR (toplevel), calendar_file);
	}
	else {
		printf ("creating calendar\n");
		success = gnome_calendar_create (GNOME_CALENDAR (toplevel), calendar_file);
		/*GNOME_CALENDAR (toplevel)->client->filename = g_strdup (calendar_file);*/
	}

	printf ("load or create returned %d\n", success);


	gtk_signal_connect (GTK_OBJECT (toplevel), "delete_event",
			    GTK_SIGNAL_FUNC(calendar_close_event), toplevel);

	active_calendars++;
	all_calendars = g_list_prepend (all_calendars, toplevel);

	if (hidden){
		GnomeWinState state;
		
		/* Realize the toplevel window to prevent a segfault */
		gtk_widget_realize (toplevel);
		state = gnome_win_hints_get_state (toplevel);

		state |= WIN_STATE_MINIMIZED;
		gnome_win_hints_set_state (toplevel, state);
	}
	
	gtk_widget_show (toplevel);

	return GNOME_CALENDAR (toplevel);
}




/*
 * Initializes the calendar internal variables, loads defaults
 */
void init_calendar (void)
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

	/* read todolist settings */

	todo_show_time_remaining = gnome_config_get_bool("/calendar/Todo/show_time_remain");
	todo_show_due_date = gnome_config_get_bool("/calendar/Todo/show_due_date");

	todo_item_dstatus_highlight_overdue = gnome_config_get_bool("/calendar/Todo/highlight_overdue");
	
	todo_item_dstatus_highlight_due_today = gnome_config_get_bool("/calendar/Todo/highlight_due_today");
	
	todo_item_dstatus_highlight_not_due_yet = gnome_config_get_bool("/calendar/Todo/highlight_not_due_yet");

        todo_current_sort_column = gnome_config_get_int("/calendar/Todo/sort_column");
	
	todo_current_sort_type = gnome_config_get_int("/calendar/Todo/sort_type");
	
	todo_show_priority = gnome_config_get_bool("/calendar/Todo/show_priority");

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

	init_default_alarms ();
	

	/* Done */

	gnome_config_pop_prefix ();
}



/* FIXME -- where should this go? */
void
calendar_iterate (GnomeCalendar *cal,
		  time_t start, time_t end,
		  calendarfn cb, void *closure)
{
	GList *l, *uids = 0;

	uids = cal_client_get_uids (cal->client, CALOBJ_TYPE_EVENT);

	for (l = uids; l; l = l->next){
		CalObjFindStatus status;
		iCalObject *ico;
		char *uid = l->data;
		char *obj_string = cal_client_get_object (cal->client, uid);

		/*iCalObject *obj = string_to_ical_object (obj_string);*/
		status = ical_object_find_in_string (uid, obj_string, &ico);
		switch (status){
		case CAL_OBJ_FIND_SUCCESS:
			ical_object_generate_events (ico, start, end,
						     cb, closure);
			break;
		case CAL_OBJ_FIND_SYNTAX_ERROR:
			printf("calendar_iterate: syntax error uid=%s\n",uid);
			break;
		case CAL_OBJ_FIND_NOT_FOUND:
			printf("calendar_iterate: obj not found uid=%s\n",uid);
			break;
		}

		g_free (l->data);
	}
	g_list_free (uids);
}



static gint
calendar_object_compare_by_start (gconstpointer a, gconstpointer b)
{
	const CalendarObject *ca = a;
	const CalendarObject *cb = b;
	time_t diff;

	diff = ca->ev_start - cb->ev_start;
	return (diff < 0) ? -1 : (diff > 0) ? 1 : 0;
}

/* FIXME -- where should this (and calendar_object_compare_by_start) go? */
/* returns a list of events in the form of CalendarObject* */
GList *calendar_get_events_in_range (CalClient *calc,
				     time_t start, time_t end)
{
	GList *l, *uids, *res = 0;
	uids = cal_client_get_events_in_range (calc, start, end);

	for (l = uids; l; l = l->next){
		CalObjFindStatus status;
		CalObjInstance *coi = l->data;
		char *uid = coi->uid;
		char *obj_string = cal_client_get_object (calc, uid);
		iCalObject *ico;


		status = ical_object_find_in_string (uid, obj_string, &ico);
		switch (status){
		case CAL_OBJ_FIND_SUCCESS:
			{
				CalendarObject *co = g_new (CalendarObject, 1);
				co->ev_start = start;
				co->ev_end   = end;
				co->ico      = ico;

				res = g_list_insert_sorted (res, co,
					calendar_object_compare_by_start);
				break;
			}
		case CAL_OBJ_FIND_SYNTAX_ERROR:
			printf ("calendar_get_events_in_range: "
				"syntax error uid=%s\n", uid);
			break;
		case CAL_OBJ_FIND_NOT_FOUND:
			printf ("calendar_get_events_in_range: "
				"obj not found uid=%s\n", uid);
			break;
		}

	}

	return res;
}
