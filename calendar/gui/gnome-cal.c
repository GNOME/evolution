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
#include <gtk/gtkframe.h>
#include <gtk/gtkhpaned.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmain.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkvpaned.h>
#include <libgnomeui/gnome-messagebox.h>
#include <cal-util/timeutil.h>
#include "alarm.h"
#include "e-day-view.h"
#include "e-week-view.h"
#include "gncal-todo.h"
#include "gnome-cal.h"
#include "year-view.h"
#include "calendar-commands.h"



static void gnome_calendar_update_view_times (GnomeCalendar *gcal,
					      GtkWidget *page);
static void gnome_calendar_update_gtk_calendar (GnomeCalendar *gcal);
static void gnome_calendar_on_day_selected (GtkCalendar   *calendar,
					    GnomeCalendar *gcal);
static void gnome_calendar_on_month_changed (GtkCalendar   *calendar,
					     GnomeCalendar *gcal);

static GtkVBoxClass *parent_class;

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
		gnome_calendar_type = gtk_type_unique (gtk_vbox_get_type (),
						       &gnome_calendar_info);
		parent_class = gtk_type_class (gtk_vbox_get_type ());
	}
	return gnome_calendar_type;
}

static void
setup_widgets (GnomeCalendar *gcal)
{
	GtkWidget *vpane, *w;

	/* The Main Notebook. */
	gcal->main_notebook = gtk_notebook_new ();
	gtk_notebook_set_show_border (GTK_NOTEBOOK (gcal->main_notebook),
				      FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (gcal->main_notebook), FALSE);
	gtk_widget_show (gcal->main_notebook);
	gtk_box_pack_start (GTK_BOX (gcal), gcal->main_notebook,
			    TRUE, TRUE, 0);

	/* The First Page of the Main Notebook, containing a HPaned with the
	   Sub-Notebook on the left and the GtkCalendar and ToDo list on the
	   right. */
	gcal->hpane = gtk_hpaned_new ();
	gtk_widget_show (gcal->hpane);
	gtk_notebook_append_page (GTK_NOTEBOOK (gcal->main_notebook),
				  gcal->hpane, gtk_label_new (""));

	/* The Sub-Notebook, to contain the Day, Work-Week & Week views. */
	gcal->sub_notebook = gtk_notebook_new ();
	gtk_notebook_set_show_border (GTK_NOTEBOOK (gcal->sub_notebook),
				      FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (gcal->sub_notebook), FALSE);
	gtk_widget_show (gcal->sub_notebook);
	gtk_paned_pack1 (GTK_PANED (gcal->hpane), gcal->sub_notebook,
			 TRUE, TRUE);

	/* The VPaned widget, to contain the GtkCalendar & ToDo list. */
	vpane = gtk_vpaned_new ();
	gtk_widget_show (vpane);
	gtk_paned_pack2 (GTK_PANED (gcal->hpane), vpane, FALSE, TRUE);

	/* The GtkCalendar. */
	w = gtk_calendar_new ();
	gcal->gtk_calendar = GTK_CALENDAR (w);
	gtk_widget_show (w);
	gtk_paned_pack1 (GTK_PANED (vpane), w, FALSE, TRUE);
	gcal->day_selected_id = gtk_signal_connect (GTK_OBJECT (gcal->gtk_calendar),
						    "day_selected",
						    (GtkSignalFunc) gnome_calendar_on_day_selected,
						    gcal);
	gtk_signal_connect (GTK_OBJECT (gcal->gtk_calendar), "month_changed",
			    GTK_SIGNAL_FUNC (gnome_calendar_on_month_changed),
			    gcal);

	/* The ToDo list. */
	gcal->todo = gncal_todo_new (gcal);
	gtk_paned_pack2 (GTK_PANED (vpane), gcal->todo, TRUE, TRUE);
	gtk_widget_show (gcal->todo);


	/* The Day View. */
	gcal->day_view = e_day_view_new ();
	e_day_view_set_calendar (E_DAY_VIEW (gcal->day_view), gcal);
	gtk_widget_show (gcal->day_view);
	gtk_notebook_append_page (GTK_NOTEBOOK (gcal->sub_notebook),
				  gcal->day_view, gtk_label_new (""));

	/* The Work Week View. */
	gcal->work_week_view = e_day_view_new ();
	e_day_view_set_days_shown (E_DAY_VIEW (gcal->work_week_view), 5);
	e_day_view_set_calendar (E_DAY_VIEW (gcal->work_week_view), gcal);
	gtk_widget_show (gcal->work_week_view);
	gtk_notebook_append_page (GTK_NOTEBOOK (gcal->sub_notebook),
				  gcal->work_week_view, gtk_label_new (""));

	/* The Week View. */
	gcal->week_view = e_week_view_new ();
	e_week_view_set_calendar (E_WEEK_VIEW (gcal->week_view), gcal);
	gtk_widget_show (gcal->week_view);
	gtk_notebook_append_page (GTK_NOTEBOOK (gcal->sub_notebook),
				  gcal->week_view, gtk_label_new (""));

	/* The Month View. */
	gcal->month_view = e_week_view_new ();
	e_week_view_set_calendar (E_WEEK_VIEW (gcal->month_view), gcal);
	e_week_view_set_display_month (E_WEEK_VIEW (gcal->month_view), TRUE);
	gtk_widget_show (gcal->month_view);
	gtk_notebook_append_page (GTK_NOTEBOOK (gcal->main_notebook),
				  gcal->month_view, gtk_label_new (""));

	/* The Year View. */
	gcal->year_view  = year_view_new (gcal, gcal->selection_start_time);
#if 0
	gtk_widget_show (gcal->year_view);
#endif
	gcal->year_view_sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (gcal->year_view_sw);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (gcal->year_view_sw),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (gcal->year_view_sw),
			   gcal->year_view);
	GTK_LAYOUT (gcal->year_view)->vadjustment->step_increment = 10.0;
	gtk_adjustment_changed (GTK_LAYOUT (gcal->year_view)->vadjustment);
	gtk_notebook_append_page (GTK_NOTEBOOK (gcal->main_notebook),
				  gcal->year_view_sw, gtk_label_new (""));
}

static GtkWidget *
get_current_page (GnomeCalendar *gcal)
{
	GtkWidget *page;

	page = GTK_NOTEBOOK (gcal->main_notebook)->cur_page->child;
	if (page == gcal->hpane)
		return GTK_NOTEBOOK (gcal->sub_notebook)->cur_page->child;
	else
		return page;
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
	else if (page == gcal->work_week_view)
		return "workweekview";
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
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (new_time != -1);

	gcal->selection_start_time = time_day_begin (new_time);
	gcal->selection_end_time = time_add_day (gcal->selection_start_time,
						 1);
	gnome_calendar_update_view_times (gcal, NULL);
	gnome_calendar_update_gtk_calendar (gcal);
}


static void
gnome_calendar_update_view_times (GnomeCalendar *gcal,
				  GtkWidget *page)
{
	if (page == NULL)
		page = get_current_page (gcal);

	if (page == gcal->day_view
	    || page == gcal->work_week_view)
		e_day_view_set_selected_time_range (E_DAY_VIEW (page),
						    gcal->selection_start_time,
						    gcal->selection_end_time);
	else if (page == gcal->week_view
		 || page == gcal->month_view)
		e_week_view_set_selected_time_range (E_WEEK_VIEW (page),
						     gcal->selection_start_time,
						     gcal->selection_end_time);
	else if (page == gcal->year_view_sw)
		year_view_set (YEAR_VIEW (gcal->year_view),
			       gcal->selection_start_time);
	else {
		g_warning ("My penguin is gone!");
		g_assert_not_reached ();
	}
}

static void
gnome_calendar_direction (GnomeCalendar *gcal, int direction)
{
	GtkWidget *cp = get_current_page (gcal);
	time_t current_time, new_time;

	current_time = gcal->selection_start_time;

	if (cp == gcal->day_view)
		new_time = time_add_day (current_time, direction);
	else if (cp == gcal->work_week_view)
		new_time = time_add_week (current_time, direction);
	else if (cp == gcal->week_view)
		new_time = time_add_week (current_time, direction);
	else if (cp == gcal->month_view)
		new_time = time_add_month (current_time, direction);
	else if (cp == gcal->year_view_sw)
		new_time = time_add_year (current_time, direction);
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

	gnome_calendar_set_view (gcal, "dayview");
	gnome_calendar_goto (gcal, time);
}

void
gnome_calendar_goto_today (GnomeCalendar *gcal)
{
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	gnome_calendar_goto (gcal, time (NULL));
}


/* This sets which view is currently shown. It also updates the selection time
   of the view so it shows the appropriate days. */
void
gnome_calendar_set_view (GnomeCalendar *gcal, char *page_name)
{
	GtkWidget *page;
	int main_page = 0, sub_page = -1;

	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (page_name != NULL);

	if (strcmp (page_name, "dayview") == 0) {
		page = gcal->day_view;
		sub_page = 0;
	} else if (strcmp (page_name, "workweekview") == 0) {
		page = gcal->work_week_view;
		sub_page = 1;
	} else if (strcmp (page_name, "weekview") == 0) {
		page = gcal->week_view;
		sub_page = 2;
	} else if (strcmp (page_name, "monthview") == 0) {
		page = gcal->month_view;
		main_page = 1;
	} else if (strcmp (page_name, "yearview") == 0) {
		page = gcal->year_view_sw;
		main_page = 2;
	} else {
		g_warning ("Unknown calendar view: %s", page_name);
		return;
	}

	gnome_calendar_update_view_times (gcal, page);

	if (sub_page != -1)
		gtk_notebook_set_page (GTK_NOTEBOOK (gcal->sub_notebook),
				       sub_page);
	gtk_notebook_set_page (GTK_NOTEBOOK (gcal->main_notebook), main_page);

	gnome_calendar_update_gtk_calendar (gcal);
}


static void
gnome_calendar_update_all (GnomeCalendar *cal, iCalObject *object, int flags)
{
	e_day_view_update_event (E_DAY_VIEW (cal->day_view),
				 object, flags);
	e_day_view_update_event (E_DAY_VIEW (cal->work_week_view),
				 object, flags);
	e_week_view_update_event (E_WEEK_VIEW (cal->week_view),
				  object, flags);
	e_week_view_update_event (E_WEEK_VIEW (cal->month_view),
				  object, flags);
	year_view_update (YEAR_VIEW (cal->year_view), object, flags);

	gncal_todo_update (GNCAL_TODO (cal->todo), object, flags);
	gnome_calendar_tag_calendar (cal, cal->gtk_calendar);
}


static void
gnome_calendar_object_updated_cb (GtkWidget *cal_client,
				  const char *uid,
				  GnomeCalendar  *gcal)
{
	printf ("gnome-cal: got object changed_cb, uid='%s'\n",
		uid?uid:"<NULL>");
	gnome_calendar_update_all (gcal, NULL, CHANGE_NEW);
}


static void
gnome_calendar_object_removed_cb (GtkWidget *cal_client,
				  const char *uid,
				  GnomeCalendar  *gcal)
{
	printf ("gnome-cal: got object removed _cb, uid='%s'\n",
		uid?uid:"<NULL>");
	gnome_calendar_update_all (gcal, NULL, CHANGE_ALL);
}


GtkWidget *
gnome_calendar_new (char *title)
{
	GtkWidget      *retval;
	GnomeCalendar  *gcal;

	retval = gtk_type_new (gnome_calendar_get_type ());

	gcal = GNOME_CALENDAR (retval);

	gcal->selection_start_time = time_day_begin (time (NULL));
	gcal->selection_end_time = time_add_day (gcal->selection_start_time,
						 1);
	gcal->client = cal_client_new ();

	setup_widgets (gcal);

	gnome_calendar_set_view (gcal, "dayview");

	gtk_signal_connect (GTK_OBJECT (gcal->client), "obj_updated",
			    gnome_calendar_object_updated_cb, gcal);
	gtk_signal_connect (GTK_OBJECT (gcal->client), "obj_removed",
			    gnome_calendar_object_removed_cb, gcal);

	return retval;
}

typedef struct
{
	GnomeCalendar *gcal;
	char *uri;
	GnomeCalendarOpenMode gcom;
	guint signal_handle;
} load_or_create_data;


static void
gnome_calendar_load_cb (GtkWidget *cal_client,
			CalClientLoadStatus success,
			load_or_create_data *locd)
{
	g_return_if_fail (locd);
	g_return_if_fail (GNOME_IS_CALENDAR (locd->gcal));

	switch (success) {
	case CAL_CLIENT_LOAD_SUCCESS:
		gnome_calendar_update_all (locd->gcal, NULL, 0);
		printf ("gnome_calendar_load_cb: success\n");
		break;
	case CAL_CLIENT_LOAD_ERROR:
		printf ("gnome_calendar_load_cb: load error.\n");
		if (locd->gcom == CALENDAR_OPEN_OR_CREATE) {
			printf ("gnome_calendar_load_cb: trying create...\n");
			/* FIXME: connect to the cal_loaded signal of the
			 * CalClient and get theasynchronous notification
			 * properly! */
			/*gtk_signal_connect (GTK_OBJECT (gcal->client),
					    "cal_loaded",
					    gnome_calendar_create_cb, gcal);*/

			gtk_signal_disconnect (GTK_OBJECT (locd->gcal->client),
					       locd->signal_handle);

			cal_client_create_calendar (locd->gcal->client,
						    locd->uri);
			gnome_calendar_update_all (locd->gcal, NULL, 0);
		}
		break;
	case CAL_CLIENT_LOAD_IN_USE:
		printf ("gnome_calendar_load_cb: in use\n");
		break;
	}

	g_free (locd->uri);
	g_free (locd);
}


int
gnome_calendar_open (GnomeCalendar *gcal,
		     char *file,
		     GnomeCalendarOpenMode gcom)
{
	load_or_create_data *locd;

	g_return_val_if_fail (gcal != NULL, 0);
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), 0);
	g_return_val_if_fail (file != NULL, 0);

	locd = g_new0 (load_or_create_data, 1);
	locd->gcal = gcal;
	locd->uri = g_strdup (file);
	locd->gcom = gcom;

	locd->signal_handle = gtk_signal_connect (GTK_OBJECT (gcal->client),
						  "cal_loaded",
						  gnome_calendar_load_cb,
						  locd);

	if (cal_client_load_calendar (gcal->client, file) == FALSE){
		printf ("Error loading calendar: %s\n", file);
		return 0;
	}

	return 1;
}


void
gnome_calendar_add_object (GnomeCalendar *gcal, iCalObject *obj)
{
	char *obj_string;
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (obj != NULL);

	obj_string = ical_object_to_string (obj);
	cal_client_update_object (gcal->client, obj->uid, obj_string);
	g_free (obj_string);
}

void
gnome_calendar_remove_object (GnomeCalendar *gcal, iCalObject *obj)
{
	gboolean r;

	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (obj != NULL);

	r = cal_client_remove_object (gcal->client, obj->uid);
}

void
gnome_calendar_object_changed (GnomeCalendar *gcal, iCalObject *obj, int flags)
{
	char *obj_string;
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (obj != NULL);

	obj_string = ical_object_to_string (obj);
	cal_client_update_object (gcal->client, obj->uid, obj_string);
	g_free (obj_string);
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

/* This is called when the day begin & end times, the AM/PM flag, or the
   week_starts_on_monday flags are changed.
   FIXME: Which of these options do we want the new views to support? */
void
gnome_calendar_time_format_changed (GnomeCalendar *gcal)
{
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	year_view_time_format_changed (YEAR_VIEW (gcal->year_view));

	gtk_calendar_display_options (gcal->gtk_calendar,
				      (week_starts_on_monday
				       ? (gcal->gtk_calendar->display_flags
					  | GTK_CALENDAR_WEEK_START_MONDAY)
				       : (gcal->gtk_calendar->display_flags
					  & ~GTK_CALENDAR_WEEK_START_MONDAY)));
}

/* This is called when any of the color settings are changed.
   FIXME: Need to update for the new views. */
void
gnome_calendar_colors_changed (GnomeCalendar *gcal)
{
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	todo_style_changed = 1;
	gncal_todo_update (GNCAL_TODO (gcal->todo), NULL, 0);
}

void
gnome_calendar_todo_properties_changed (GnomeCalendar *gcal)
{
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	todo_style_changed = 1;
	gncal_todo_update (GNCAL_TODO (gcal->todo), NULL, 0);
}


void
gnome_calendar_set_selected_time_range (GnomeCalendar *gcal,
					time_t	       start_time,
					time_t	       end_time)
{
	gcal->selection_start_time = start_time;
	gcal->selection_end_time = end_time;

	gnome_calendar_update_gtk_calendar (gcal);
}


/* This updates the month shown and the day selected in the calendar, if
   necessary. */
static void
gnome_calendar_update_gtk_calendar (GnomeCalendar *gcal)
{
	GDate date;
	guint current_year, current_month, current_day;
	guint new_year, new_month, new_day;
	gboolean set_day = FALSE;

	/* If the GtkCalendar isn't visible, we just return. */
	if (!GTK_WIDGET_VISIBLE (gcal->gtk_calendar))
		return;

	gtk_calendar_get_date (gcal->gtk_calendar, &current_year,
			       &current_month, &current_day);

	g_date_clear (&date, 1);
	g_date_set_time (&date, gcal->selection_start_time);
	new_year = g_date_year (&date);
	new_month = g_date_month (&date) - 1;
	new_day = g_date_day (&date);

	/* Block the "day_selected" signal while we update the calendar. */
	gtk_signal_handler_block (GTK_OBJECT (gcal->gtk_calendar),
				  gcal->day_selected_id);

	/* If the year & month don't match, update it. */
	if (new_year != current_year || new_month != current_month) {
		/* FIXME: GtkCalendar bug workaround. If we select a month
		   which has less days than the currently selected day, it
		   causes a problem next time we set the day. */
		if (current_day > 28) {
			gtk_calendar_select_day (gcal->gtk_calendar, 28);
			set_day = TRUE;
		}
		gtk_calendar_select_month (gcal->gtk_calendar, new_month,
					   new_year);
	}

	/* If the day doesn't match, update it. */
	if (new_day != current_day || set_day)
		gtk_calendar_select_day (gcal->gtk_calendar, new_day);

	gtk_signal_handler_unblock (GTK_OBJECT (gcal->gtk_calendar),
				    gcal->day_selected_id);
}

static void
gnome_calendar_on_day_selected (GtkCalendar   *calendar,
				GnomeCalendar *gcal)
{
	gint y, m, d;
	struct tm tm;

	gtk_calendar_get_date (calendar, &y, &m, &d);

	tm.tm_year = y - 1900;
	tm.tm_mon  = m;
	tm.tm_mday = d;
	tm.tm_hour = 5; /* for daylight savings time fix */
	tm.tm_min  = 0;
	tm.tm_sec  = 0;

	gnome_calendar_goto (gcal, mktime (&tm));
}


static void
gnome_calendar_on_month_changed (GtkCalendar   *calendar,
				 GnomeCalendar *gcal)
{
	gnome_calendar_tag_calendar (gcal, gcal->gtk_calendar);
}

