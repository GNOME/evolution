/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * GnomeCalendar widget
 * Copyright (C) 1998 the Free Software Foundation
 *
 * Author: Miguel de Icaza (miguel@kernel.org)
 */

#include <config.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <gtk/gtkmain.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkframe.h>
#include <libgnomeui/gnome-messagebox.h>
#include <cal-util/timeutil.h>
#include "alarm.h"
#include "gnome-cal.h"
#include "gncal-day-panel.h"
#include "gncal-week-view.h"
#include "month-view.h"
#include "year-view.h"
#include "calendar-commands.h"



GnomeApp *parent_class;

guint
gnome_calendar_get_type (void)
{
	static guint gnome_calendar_type = 0;
	if(!gnome_calendar_type) {
		GtkTypeInfo gnome_calendar_info = {
			"GnomeCalendar",
			sizeof(GnomeCalendar),
			sizeof(GnomeCalendarClass),
			(GtkClassInitFunc) NULL,
			(GtkObjectInitFunc) NULL,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL,
		};
		/*
		gnome_calendar_type = gtk_type_unique(gnome_app_get_type(), &gnome_calendar_info);
		parent_class = gtk_type_class (gnome_app_get_type());
		*/
		gnome_calendar_type = gtk_type_unique (gtk_frame_get_type (), &gnome_calendar_info);
		parent_class = gtk_type_class (gtk_frame_get_type ());
	}
	return gnome_calendar_type;
}

static void
setup_widgets (GnomeCalendar *gcal)
{
	time_t now;

	now = time (NULL);

	gcal->notebook   = gtk_notebook_new ();
	gcal->day_view   = gncal_day_panel_new (gcal, now);
	gcal->week_view  = gncal_week_view_new (gcal, now);
	gcal->month_view = month_view_new (gcal, now);
	gcal->year_view  = year_view_new (gcal, now);

	gcal->year_view_sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (gcal->year_view_sw),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (gcal->year_view_sw), gcal->year_view);
	GTK_LAYOUT (gcal->year_view)->vadjustment->step_increment = 10.0;
	gtk_adjustment_changed (GTK_ADJUSTMENT (GTK_LAYOUT (gcal->year_view)->vadjustment));

	gtk_notebook_append_page (GTK_NOTEBOOK (gcal->notebook), gcal->day_view,   gtk_label_new (_("Day View")));
	gtk_notebook_append_page (GTK_NOTEBOOK (gcal->notebook), gcal->week_view,  gtk_label_new (_("Week View")));
	gtk_notebook_append_page (GTK_NOTEBOOK (gcal->notebook), gcal->month_view, gtk_label_new (_("Month View")));
	gtk_notebook_append_page (GTK_NOTEBOOK (gcal->notebook), gcal->year_view_sw,  gtk_label_new (_("Year View")));

	gtk_widget_show_all (gcal->notebook);

	/*gnome_app_set_contents (GNOME_APP (gcal), gcal->notebook);*/
	gtk_container_add (GTK_CONTAINER (gcal), gcal->notebook);


	gtk_widget_show (GTK_WIDGET (gcal));
}

static GtkWidget *
get_current_page (GnomeCalendar *gcal)
{
	return GTK_NOTEBOOK (gcal->notebook)->cur_page->child;
}

char *
gnome_calendar_get_current_view_name (GnomeCalendar *gcal)
{
	GtkWidget *page;

	g_return_val_if_fail (gcal != NULL, "dayview");
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), "dayview");

	page = get_current_page (gcal);

	if (page == gcal->day_view)
		return "dayview";
	else if (page == gcal->week_view)
		return "weekview";
	else if (page == gcal->month_view)
		return "monthview";
	else if (page == gcal->year_view_sw)
		return "yearview";
	else
		return "dayview";
}

void
gnome_calendar_goto (GnomeCalendar *gcal, time_t new_time)
{
	GtkWidget *current;

	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (new_time != -1);

	current = get_current_page (gcal);
	new_time = time_day_begin (new_time);

	if (current == gcal->day_view)
		gncal_day_panel_set (GNCAL_DAY_PANEL (gcal->day_view), new_time);
	else if (current == gcal->week_view)
		gncal_week_view_set (GNCAL_WEEK_VIEW (gcal->week_view), new_time);
	else if (current == gcal->month_view)
		month_view_set (MONTH_VIEW (gcal->month_view), new_time);
	else if (current == gcal->year_view_sw)
		year_view_set (YEAR_VIEW (gcal->year_view), new_time);
	else {
		g_warning ("My penguin is gone!");
		g_assert_not_reached ();
	}

	gcal->current_display = new_time;
}

static void
gnome_calendar_direction (GnomeCalendar *gcal, int direction)
{
	GtkWidget *cp = get_current_page (gcal);
	time_t new_time;

	if (cp == gcal->day_view)
		new_time = time_add_day (time_day_begin (gcal->current_display), 1 * direction);
	else if (cp == gcal->week_view)
		new_time = time_add_week (time_week_begin (gcal->current_display), 1 * direction);
	else if (cp == gcal->month_view)
		new_time = time_add_month (time_month_begin (gcal->current_display), 1 * direction);
	else if (cp == gcal->year_view_sw)
		new_time = time_add_year (time_year_begin (gcal->current_display), 1 * direction);
	else {
		g_warning ("Weee!  Where did the penguin go?");
		g_assert_not_reached ();
		new_time = 0;
	}

	gnome_calendar_goto (gcal, new_time);
}

void
gnome_calendar_next (GnomeCalendar *gcal)
{
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	gnome_calendar_direction (gcal, 1);
}

void
gnome_calendar_previous (GnomeCalendar *gcal)
{
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	gnome_calendar_direction (gcal, -1);
}

void
gnome_calendar_dayjump (GnomeCalendar *gcal, time_t time)
{
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	gtk_notebook_set_page (GTK_NOTEBOOK (gcal->notebook), 0);
	gnome_calendar_goto (gcal, time);
}

void
gnome_calendar_goto_today (GnomeCalendar *gcal)
{
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	gnome_calendar_goto (gcal, time (NULL));
}

void
gnome_calendar_set_view (GnomeCalendar *gcal, char *page_name)
{
	int page = 0;

	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (page_name != NULL);


	if (strcmp (page_name, "dayview") == 0)
		page = 0;
	else if (strcmp (page_name, "weekview") == 0)
		page = 1;
	else if (strcmp (page_name, "monthview") == 0)
		page = 2;
	else if (strcmp (page_name, "yearview") == 0)
		page = 3;
	gtk_notebook_set_page (GTK_NOTEBOOK (gcal->notebook), page);
}


GtkWidget *
gnome_calendar_new (char *title)
{
	GtkWidget      *retval;
	GnomeCalendar  *gcal;
	/*GnomeApp       *app;*/

	retval = gtk_type_new (gnome_calendar_get_type ());
	/*app = GNOME_APP (retval);*/
	gcal = GNOME_CALENDAR (retval);

	/*
	app->name = g_strdup ("calendar");
	app->prefix = g_strconcat ("/", app->name, "/", NULL);
	*/

	/* gtk_window_set_title(GTK_WINDOW(retval), title); */

	gcal->current_display = time_day_begin (time (NULL));
	gcal->client = cal_client_new ();
	setup_widgets (gcal);

	return retval;
}

static void
gnome_calendar_update_all (GnomeCalendar *cal, iCalObject *object, int flags)
{
	gncal_day_panel_update (GNCAL_DAY_PANEL (cal->day_view), object, flags);
	gncal_week_view_update (GNCAL_WEEK_VIEW (cal->week_view), object, flags);
	month_view_update (MONTH_VIEW (cal->month_view), object, flags);
	year_view_update (YEAR_VIEW (cal->year_view), object, flags);
}

int
gnome_calendar_load (GnomeCalendar *gcal, char *file)
{
	g_return_val_if_fail (gcal != NULL, 0);
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), 0);
	g_return_val_if_fail (file != NULL, 0);

	/* FIXME: connect to the cal_loaded signal fo the CalClient and get the
	 * asynchronous notification properly!
	 */

	/* if ((r = calendar_load (gcal->cal, file)) != NULL){ DELETE */
	if (cal_client_load_calendar (gcal->client, file) == FALSE){
		printf ("Error loading calendar: %s\n", file);
		return 0;
	}
	gnome_calendar_update_all (gcal, NULL, 0);
	return 1;
}

void
gnome_calendar_add_object (GnomeCalendar *gcal, iCalObject *obj)
{
	char *obj_string;
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (obj != NULL);

	/*calendar_add_object (gcal->cal, obj); DELETE */
	obj_string = ical_object_to_string (obj);
	cal_client_update_object (gcal->client, obj->uid, obj_string);
	g_free (obj_string);
	gnome_calendar_update_all (gcal, obj, CHANGE_NEW);
}

void
gnome_calendar_remove_object (GnomeCalendar *gcal, iCalObject *obj)
{
	gboolean r;

	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (obj != NULL);

	/* calendar_remove_object (gcal->cal, obj); DELETE */
	r = cal_client_remove_object (gcal->client, obj->uid);
	gnome_calendar_update_all (gcal, obj, CHANGE_ALL);
}

void
gnome_calendar_object_changed (GnomeCalendar *gcal, iCalObject *obj, int flags)
{
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (obj != NULL);

	/* FIXME -- i don't know what to do here */
	/* gcal->cal->modified = TRUE; */

	gnome_calendar_update_all (gcal, obj, flags);
	/* calendar_object_changed (gcal->cal, obj, flags); */
}

static int
max_open_files (void)
{
        static int files;

        if (files)
                return files;

        files = sysconf (_SC_OPEN_MAX);
        if (files != -1)
                return files;
#ifdef OPEN_MAX
        return files = OPEN_MAX;
#else
        return files = 256;
#endif
}

static void
execute (char *command, int close_standard)
{
	struct sigaction ignore, save_intr, save_quit;
	int status = 0, i;
	pid_t pid;

	ignore.sa_handler = SIG_IGN;
	sigemptyset (&ignore.sa_mask);
	ignore.sa_flags = 0;

	sigaction (SIGINT, &ignore, &save_intr);
	sigaction (SIGQUIT, &ignore, &save_quit);

	if ((pid = fork ()) < 0){
		fprintf (stderr, "\n\nfork () = -1\n");
		return;
	}
	if (pid == 0){
		pid = fork ();
		if (pid == 0){
			const int top = max_open_files ();
			sigaction (SIGINT,  &save_intr, NULL);
			sigaction (SIGQUIT, &save_quit, NULL);

			for (i = (close_standard ? 0 : 3); i < top; i++)
				close (i);

			/* FIXME: As an excercise to the reader, copy the
			 * code from mc to setup shell properly instead of
			 * /bin/sh.  Yes, this comment is larger than a cut and paste.
			 */
			execl ("/bin/sh", "/bin/sh", "-c", command, (char *) 0);

			_exit (127);
		} else {
			_exit (127);
		}
	}
	wait (&status);
	sigaction (SIGINT,  &save_intr, NULL);
	sigaction (SIGQUIT, &save_quit, NULL);
}

static void
mail_notify (char *mail_address, char *text, time_t app_time)
{
	pid_t pid;
	int   p [2];
	char *command;

	pipe (p);
	pid = fork ();
	if (pid == 0){
		int dev_null;

		dev_null = open ("/dev/null", O_RDWR);
		dup2 (p [0], 0);
		dup2 (dev_null, 1);
		dup2 (dev_null, 2);
		execl ("/usr/lib/sendmail", "/usr/lib/sendmail",
		       mail_address, NULL);
		_exit (127);
	}
	command = g_strconcat ("To: ", mail_address, "\n",
				  "Subject: ", _("Reminder of your appointment at "),
				  ctime (&app_time), "\n\n", text, "\n", NULL);
	write (p [1], command, strlen (command));
 	close (p [1]);
	close (p [0]);
	g_free (command);
}

static void
stop_beeping (GtkObject* object, gpointer data)
{
	guint timer_tag, beep_tag;
	timer_tag = GPOINTER_TO_INT (gtk_object_get_data (object, "timer_tag"));
	beep_tag  = GPOINTER_TO_INT (gtk_object_get_data (object, "beep_tag"));
	
	if (beep_tag > 0) {
		gtk_timeout_remove (beep_tag);
		gtk_object_set_data (object, "beep_tag", GINT_TO_POINTER (0));
	}
	if (timer_tag > 0) {
		gtk_timeout_remove (timer_tag);
		gtk_object_set_data (object, "timer_tag", GINT_TO_POINTER (0));
	}
}

static gint
start_beeping (gpointer data)
{
	gdk_beep ();

	return TRUE;
}

static gint
timeout_beep (gpointer data)
{
	stop_beeping (data, NULL);
	return FALSE;
}

void
calendar_notify (time_t activation_time, CalendarAlarm *which, void *data)
{
	iCalObject *ico = data;
	guint beep_tag, timer_tag;
	int ret;
	gchar* snooze_button = (enable_snooze ? _("Snooze") : NULL);
	time_t now, diff;

	if (&ico->aalarm == which){
		time_t app = ico->aalarm.trigger + ico->aalarm.offset;
		GtkWidget *w;
		char *msg;

		msg = g_strconcat (_("Reminder of your appointment at "),
					ctime (&app), "`",
					ico->summary, "'", NULL);

		/* Idea: we need Snooze option :-) */
		w = gnome_message_box_new (msg, GNOME_MESSAGE_BOX_INFO, _("Ok"), snooze_button, NULL);
		beep_tag = gtk_timeout_add (1000, start_beeping, NULL);
		if (enable_aalarm_timeout)
			timer_tag = gtk_timeout_add (audio_alarm_timeout*1000, 
						     timeout_beep, w);
		else
			timer_tag = 0;
		gtk_object_set_data (GTK_OBJECT (w), "timer_tag",
				     GINT_TO_POINTER (timer_tag));
		gtk_object_set_data (GTK_OBJECT (w), "beep_tag",
				     GINT_TO_POINTER (beep_tag));
		gtk_widget_ref (w);
		gtk_window_set_modal (GTK_WINDOW (w), FALSE);
		ret = gnome_dialog_run (GNOME_DIALOG (w));
		switch (ret) {
		case 1:
			stop_beeping (GTK_OBJECT (w), NULL);
			now = time (NULL);
			diff = now - which->trigger;
			which->trigger = which->trigger + diff + snooze_secs;
			which->offset  = which->offset - diff - snooze_secs;
			alarm_add (which, &calendar_notify, data);
			break;
		default:
			stop_beeping (GTK_OBJECT (w), NULL);
			break;
		}
		
		gtk_widget_unref (w);
		return;
	}

        if (&ico->palarm == which){
		execute (ico->palarm.data, 0);
		return;
	}

	if (&ico->malarm == which){
		time_t app = ico->malarm.trigger + ico->malarm.offset;

		mail_notify (ico->malarm.data, ico->summary, app);
		return;
	}

	if (&ico->dalarm == which){
		time_t app = ico->dalarm.trigger + ico->dalarm.offset;
		GtkWidget *w;
		char *msg;

		if (beep_on_display)
			gdk_beep ();
		msg = g_strconcat (_("Reminder of your appointment at "),
					ctime (&app), "`",
					ico->summary, "'", NULL);
		w = gnome_message_box_new (msg, GNOME_MESSAGE_BOX_INFO, 
					   _("Ok"), snooze_button, NULL);
		gtk_window_set_modal (GTK_WINDOW (w), FALSE);
		ret = gnome_dialog_run (GNOME_DIALOG (w));
		switch (ret) {
		case 1:
			now = time (NULL);
			diff = now - which->trigger;
			which->trigger = which->trigger + diff + snooze_secs;
			which->offset  = which->offset - diff - snooze_secs;
			alarm_add (which, &calendar_notify, data);
			break;
		default:
			break;
		}
		
		return;
	}
}

/*
 * called from the calendar_iterate routine to mark the days of a GtkCalendar
 */
static int
mark_gtk_calendar_day (iCalObject *obj, time_t start, time_t end, void *c)
{
	GtkCalendar *gtk_cal = c;
	struct tm tm_s;
	time_t t, day_end;

	tm_s = *localtime (&start);
	day_end = time_day_end (end);

	for (t = start; t <= day_end; t += 60*60*24){
		time_t new = mktime (&tm_s);
		struct tm tm_day;

		tm_day = *localtime (&new);
		gtk_calendar_mark_day (gtk_cal, tm_day.tm_mday);
		tm_s.tm_mday++;
	}
	return TRUE;
}

/*
 * Tags the dates with appointments in a GtkCalendar based on the
 * GnomeCalendar contents
 */
void
gnome_calendar_tag_calendar (GnomeCalendar *cal, GtkCalendar *gtk_cal)
{
	time_t month_begin, month_end;
	struct tm tm;

	g_return_if_fail (cal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (cal));
	g_return_if_fail (gtk_cal != NULL);
	g_return_if_fail (GTK_IS_CALENDAR (gtk_cal));

	/* compute month_begin */
	tm.tm_hour = 0;
	tm.tm_min  = 0;
	tm.tm_sec  = 0;
	tm.tm_mday = 1; /* setting this to zero is a no-no; it will set mktime back to the end of the
					   previous month, which may be 28,29,30; this may chop some days from the calendar */
	tm.tm_mon  = gtk_cal->month;
	tm.tm_year = gtk_cal->year - 1900;
	tm.tm_isdst= -1;

	month_begin = mktime (&tm);
	tm.tm_mon++;
	month_end   = mktime (&tm);

	gtk_calendar_freeze (gtk_cal);
	gtk_calendar_clear_marks (gtk_cal);
	calendar_iterate (cal, month_begin, month_end,
			  mark_gtk_calendar_day, gtk_cal);
	gtk_calendar_thaw (gtk_cal);
}

void
gnome_calendar_time_format_changed (GnomeCalendar *gcal)
{
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	/* FIXME: the queue resizes will do until we rewrite those views... */

	gncal_day_panel_time_format_changed (GNCAL_DAY_PANEL (gcal->day_view));
	gtk_widget_queue_resize (gcal->day_view);
	gncal_week_view_time_format_changed (GNCAL_WEEK_VIEW (gcal->week_view));
	gtk_widget_queue_resize (gcal->week_view);
	month_view_time_format_changed (MONTH_VIEW (gcal->month_view));
	year_view_time_format_changed (YEAR_VIEW (gcal->year_view));
}

void
gnome_calendar_colors_changed (GnomeCalendar *gcal)
{
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	/* FIXME: add day and week view when they are done */

	month_view_colors_changed (MONTH_VIEW (gcal->month_view));
	year_view_colors_changed (YEAR_VIEW (gcal->year_view));
	todo_style_changed = 1;
	todo_list_properties_changed (GNCAL_DAY_PANEL (gcal->day_view));
}

void
gnome_calendar_todo_properties_changed (GnomeCalendar *gcal)
{
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	/* FIXME: add day and week view when they are done */

	todo_style_changed = 1;
	todo_list_properties_changed (GNCAL_DAY_PANEL (gcal->day_view));
}
