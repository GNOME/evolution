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



/* An entry in the UID->alarms hash table.  The UID key *is* the uid field in
 * this structure, so don't free it separately.
 */
typedef struct {
	char *uid;
	GList *alarm_ids;
} ObjectAlarms;



static void gnome_calendar_class_init (GnomeCalendarClass *class);
static void gnome_calendar_init (GnomeCalendar *gcal);
static void gnome_calendar_destroy (GtkObject *object);

static void gnome_calendar_update_view_times (GnomeCalendar *gcal,
					      GtkWidget *page);
static void gnome_calendar_update_gtk_calendar (GnomeCalendar *gcal);
static int gnome_calendar_mark_gtk_calendar_day (GnomeCalendar *cal,
						 GtkCalendar *gtk_cal,
						 time_t start,
						 time_t end);
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
			sizeof (GnomeCalendar),
			sizeof (GnomeCalendarClass),
			(GtkClassInitFunc) gnome_calendar_class_init,
			(GtkObjectInitFunc) gnome_calendar_init,
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

/* Class initialization function for the gnome calendar */
static void
gnome_calendar_class_init (GnomeCalendarClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	object_class->destroy = gnome_calendar_destroy;
}

/* Object initialization function for the gnome calendar */
static void
gnome_calendar_init (GnomeCalendar *gcal)
{
	gcal->alarms = g_hash_table_new (g_str_hash, g_str_equal);
}

/* Used from g_hash_table_foreach(); frees an object alarms entry */
static void
free_object_alarms (gpointer key, gpointer value, gpointer data)
{
	ObjectAlarms *oa;

	oa = value;

	g_assert (oa->uid != NULL);
	g_free (oa->uid);
	oa->uid = NULL;

	g_assert (oa->alarm_ids != NULL);
	g_list_free (oa->alarm_ids);
	oa->alarm_ids = NULL;

	g_free (oa);
}

static void
gnome_calendar_destroy (GtkObject *object)
{
	GnomeCalendar *gcal;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (object));

	gcal = GNOME_CALENDAR (object);

	gtk_object_unref (GTK_OBJECT (gcal->client));

	g_hash_table_foreach (gcal->alarms, free_object_alarms, NULL);
	g_hash_table_destroy (gcal->alarms);
	gcal->alarms = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
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

/* Sends a mail notification of an alarm trigger */
static void
mail_notification (char *mail_address, char *text, time_t app_time)
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

/* Executes a program as a notification of an alarm trigger */
static void
program_notification (char *command, int close_standard)
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

/* Present a display notification of an alarm trigger */
static void
display_notification (time_t trigger, time_t occur, iCalObject *ico, GnomeCalendar *gcal)
{
	g_message ("DISPLAY NOTIFICATION!");
	/* FIXME */
}

/* Present an audible notification of an alarm trigger */
static void
audio_notification (time_t trigger, time_t occur, iCalObject *ico, GnomeCalendar *gcal)
{
	g_message ("AUDIO NOTIFICATION!");
	/* FIXME */
}

struct trigger_alarm_closure {
	GnomeCalendar *gcal;
	char *uid;
	enum AlarmType type;
	time_t occur;
};

/* Callback function used when an alarm is triggered */
static void
trigger_alarm_cb (gpointer alarm_id, time_t trigger, gpointer data)
{
	struct trigger_alarm_closure *c;
	char *str_ico;
	iCalObject *ico;
	CalObjFindStatus status;
	ObjectAlarms *oa;
	GList *l;

	c = data;

	/* Fetch the object */

	str_ico = cal_client_get_object (c->gcal->client, c->uid);
	status = ical_object_find_in_string (c->uid, str_ico, &ico);

	switch (status) {
	case CAL_OBJ_FIND_SUCCESS:
		/* Go on */
		break;

	case CAL_OBJ_FIND_SYNTAX_ERROR:
		g_message ("trigger_alarm_cb(): syntax error in fetched object");
		return;

	case CAL_OBJ_FIND_NOT_FOUND:
		g_message ("trigger_alarm_cb(): could not find fetched object");
		return;
	}

	g_assert (ico != NULL);

	/* Present notification */

	switch (c->type) {
	case ALARM_MAIL:
		g_assert (ico->malarm.enabled);
		mail_notification (ico->malarm.data, ico->summary, c->occur);
		break;

	case ALARM_PROGRAM:
		g_assert (ico->palarm.enabled);
		program_notification (ico->palarm.data, FALSE);
		break;

	case ALARM_DISPLAY:
		g_assert (ico->dalarm.enabled);
		display_notification (trigger, c->occur, ico, c->gcal);
		break;

	case ALARM_AUDIO:
		g_assert (ico->aalarm.enabled);
		audio_notification (trigger, c->occur, ico, c->gcal);
		break;
	}

	/* Remove the alarm from the hash table */

	oa = g_hash_table_lookup (c->gcal->alarms, ico->uid);
	g_assert (oa != NULL);

	l = g_list_find (oa->alarm_ids, alarm_id);
	g_assert (l != NULL);

	oa->alarm_ids = g_list_remove_link (oa->alarm_ids, l);
	g_list_free_1 (l);

	if (!oa->alarm_ids) {
		g_hash_table_remove (c->gcal->alarms, ico->uid);
		g_free (oa->uid);
		g_free (oa);
	}
}

/* Frees a struct trigger_alarm_closure */
static void
free_trigger_alarm_closure (gpointer data)
{
	struct trigger_alarm_closure *c;

	c = data;
	g_free (c->uid);
	g_free (c);
}

/* Queues the specified alarm */
static void
setup_alarm (GnomeCalendar *cal, CalAlarmInstance *ai)
{
	struct trigger_alarm_closure *c;
	gpointer alarm;
	ObjectAlarms *oa;

	c = g_new (struct trigger_alarm_closure, 1);
	c->gcal = cal;
	c->uid = g_strdup (ai->uid);
	c->type = ai->type;
	c->occur = ai->occur;

	alarm = alarm_add (ai->trigger, trigger_alarm_cb, c, free_trigger_alarm_closure);
	if (!alarm) {
		g_message ("setup_alarm(): Could not set up alarm");
		g_free (c->uid);
		g_free (c);
		return;
	}

	oa = g_hash_table_lookup (cal->alarms, ai->uid);
	if (oa)
		oa->alarm_ids = g_list_prepend (oa->alarm_ids, alarm);
	else {
		oa = g_new (ObjectAlarms, 1);
		oa->uid = g_strdup (ai->uid);
		oa->alarm_ids = g_list_prepend (NULL, alarm);

		g_hash_table_insert (cal->alarms, oa->uid, oa);
	}
}

static void load_alarms (GnomeCalendar *cal);

/* Called nightly to refresh the day's alarms */
static void
midnight_refresh_cb (gpointer alarm_id, time_t trigger, gpointer data)
{
	GnomeCalendar *cal;

	cal = GNOME_CALENDAR (data);
	cal->midnight_alarm_refresh_id = NULL;

	load_alarms (cal);
}

/* Loads and queues the alarms from the current time up to midnight. */
static void
load_alarms (GnomeCalendar *cal)
{
	time_t now;
	time_t end_of_day;
	GList *alarms, *l;

	now = time (NULL);
	end_of_day = time_day_end (now);

	/* Queue alarms */

	alarms = cal_client_get_alarms_in_range (cal->client, now, end_of_day);

	for (l = alarms; l; l = l->next)
		setup_alarm (cal, l->data);

	cal_alarm_instance_list_free (alarms);

	/* Queue the midnight alarm refresh */

	cal->midnight_alarm_refresh_id = alarm_add (end_of_day, midnight_refresh_cb, cal, NULL);
	if (!cal->midnight_alarm_refresh_id) {
		g_message ("load_alarms(): Could not set up the midnight refresh alarm!");
		/* FIXME: what to do? */
	}
}

/* This tells all components to reload all calendar objects. */
static void
gnome_calendar_update_all (GnomeCalendar *cal)
{
	load_alarms (cal);

	e_day_view_update_all_events (E_DAY_VIEW (cal->day_view));
	e_day_view_update_all_events (E_DAY_VIEW (cal->work_week_view));
	e_week_view_update_all_events (E_WEEK_VIEW (cal->week_view));
	e_week_view_update_all_events (E_WEEK_VIEW (cal->month_view));

#if 0
	year_view_update (YEAR_VIEW (cal->year_view), NULL, TRUE);
#endif

	gncal_todo_update (GNCAL_TODO (cal->todo), NULL, TRUE);
	gnome_calendar_tag_calendar (cal, cal->gtk_calendar);
}

/* Removes any queued alarms for the specified UID */
static void
remove_alarms_for_object (GnomeCalendar *gcal, const char *uid)
{
	ObjectAlarms *oa;
	GList *l;

	oa = g_hash_table_lookup (gcal->alarms, uid);
	if (!oa)
		return;

	for (l = oa->alarm_ids; l; l = l->next) {
		gpointer alarm_id;

		alarm_id = l->data;
		alarm_remove (alarm_id);
	}

	g_hash_table_remove (gcal->alarms, uid);

	g_free (oa->uid);
	g_list_free (oa->alarm_ids);
	g_free (oa);
}

static void
gnome_calendar_object_updated_cb (GtkWidget *cal_client,
				  const char *uid,
				  GnomeCalendar  *gcal)
{
	g_message ("gnome-cal: got object changed_cb, uid='%s'",
		   uid?uid:"<NULL>");

	remove_alarms_for_object (gcal, uid);
	/* FIXME: put in new alarms */

	/* FIXME: do we really want each view to reload the event itself?
	   Maybe we should keep track of events globally, maybe with ref
	   counts. We also need to sort out where they get freed. */
	e_day_view_update_event (E_DAY_VIEW (gcal->day_view), uid);
	e_day_view_update_event (E_DAY_VIEW (gcal->work_week_view), uid);
	e_week_view_update_event (E_WEEK_VIEW (gcal->week_view), uid);
	e_week_view_update_event (E_WEEK_VIEW (gcal->month_view), uid);

	/* FIXME: optimize these? */
#if 0
	year_view_update (YEAR_VIEW (gcal->year_view), NULL, TRUE);
#endif

	gncal_todo_update (GNCAL_TODO (gcal->todo), NULL, TRUE);
	gnome_calendar_tag_calendar (gcal, gcal->gtk_calendar);
}


static void
gnome_calendar_object_removed_cb (GtkWidget *cal_client,
				  const char *uid,
				  GnomeCalendar  *gcal)
{
	g_message ("gnome-cal: got object removed _cb, uid='%s'",
		   uid?uid:"<NULL>");

	remove_alarms_for_object (gcal, uid);

	e_day_view_remove_event (E_DAY_VIEW (gcal->day_view), uid);
	e_day_view_remove_event (E_DAY_VIEW (gcal->work_week_view), uid);
	e_week_view_remove_event (E_WEEK_VIEW (gcal->week_view), uid);
	e_week_view_remove_event (E_WEEK_VIEW (gcal->month_view), uid);

	/* FIXME: optimize these? */
#if 0
	year_view_update (YEAR_VIEW (gcal->year_view), NULL, CHANGE_ALL);
#endif
	gncal_todo_update (GNCAL_TODO (gcal->todo), NULL, CHANGE_ALL);
	gnome_calendar_tag_calendar (gcal, gcal->gtk_calendar);
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
			CalClientLoadStatus status,
			load_or_create_data *locd)
{
	g_return_if_fail (locd);
	g_return_if_fail (GNOME_IS_CALENDAR (locd->gcal));

	switch (status) {
	case CAL_CLIENT_LOAD_SUCCESS:
		gnome_calendar_update_all (locd->gcal);
		g_message ("gnome_calendar_load_cb: success");
		break;

	case CAL_CLIENT_LOAD_ERROR:
		g_message ("gnome_calendar_load_cb: load error");
		if (locd->gcom == CALENDAR_OPEN_OR_CREATE) {
			g_message ("gnome_calendar_load_cb: trying create...");
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
			gnome_calendar_update_all (locd->gcal);
		}
		break;

	case CAL_CLIENT_LOAD_IN_USE:
		/* FIXME: what to do? */
		g_message ("gnome_calendar_load_cb: in use");
		break;

	case CAL_CLIENT_LOAD_METHOD_NOT_SUPPORTED:
		/* FIXME: what to do? */
		g_message ("gnome_calendar_load_cb(): method not supported");
		break;

	default:
		g_message ("gnome_calendar_load_cb(): unhandled result code %d!", (int) status);
		g_assert_not_reached ();
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
		g_message ("Error loading calendar: %s", file);
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

#if 0

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

#endif

/*
 * Tags the dates with appointments in a GtkCalendar based on the
 * GnomeCalendar contents
 */
void
gnome_calendar_tag_calendar (GnomeCalendar *cal, GtkCalendar *gtk_cal)
{
	time_t month_begin, month_end;
	struct tm tm;
	GList *cois, *l;

	g_return_if_fail (cal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (cal));
	g_return_if_fail (gtk_cal != NULL);
	g_return_if_fail (GTK_IS_CALENDAR (gtk_cal));

	/* If the GtkCalendar isn't visible, we just return. */
	if (!GTK_WIDGET_VISIBLE (cal->gtk_calendar))
		return;

	/* compute month_begin */
	tm.tm_hour = 0;
	tm.tm_min  = 0;
	tm.tm_sec  = 0;
	/* setting tm_day to zero is a no-no; it will set mktime back to the
	   end of the previous month, which may be 28,29,30; this may chop
	   some days from the calendar */
	tm.tm_mday = 1;
	tm.tm_mon  = gtk_cal->month;
	tm.tm_year = gtk_cal->year - 1900;
	tm.tm_isdst= -1;

	month_begin = mktime (&tm);
	tm.tm_mon++;
	month_end   = mktime (&tm);

	gtk_calendar_freeze (gtk_cal);
	gtk_calendar_clear_marks (gtk_cal);

	cois = cal_client_get_events_in_range (cal->client, month_begin,
					       month_end);

	for (l = cois; l; l = l->next) {
		CalObjInstance *coi = l->data;

		gnome_calendar_mark_gtk_calendar_day (cal, gtk_cal,
						      coi->start, coi->end);

		g_free (coi->uid);
		g_free (coi);
	}

	g_list_free (cois);

	gtk_calendar_thaw (gtk_cal);
}


/*
 * This is called from gnome_calendar_tag_calendar to mark the days of a
 * GtkCalendar on which the user has appointments.
 */
static int
gnome_calendar_mark_gtk_calendar_day (GnomeCalendar *cal,
				      GtkCalendar *gtk_cal,
				      time_t start,
				      time_t end)
{
	struct tm tm_s, tm_e;
	gint start_day, end_day, day;

	tm_s = *localtime (&start);
	tm_e = *localtime (&end);

	start_day = tm_s.tm_mday;
	end_day = tm_e.tm_mday;

	/* If the event ends at midnight then really it ends on the previous
	   day (unless it started at the same time). */
	if (start != end && tm_e.tm_hour == 0 && tm_e.tm_min == 0)
		end_day--;

	for (day = start_day; day <= end_day; day++)
		gtk_calendar_mark_day (gtk_cal, day);

	return TRUE;
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


/* Returns the selected time range for the current view. Note that this may be
   different from the fields in the GnomeCalendar, since the view may clip
   this or choose a more appropriate time. */
void
gnome_calendar_get_current_time_range (GnomeCalendar *gcal,
				       time_t	 *start_time,
				       time_t	 *end_time)
{
	GtkWidget *page;

	page = get_current_page (gcal);

	if (page == gcal->day_view
	    || page == gcal->work_week_view)
		e_day_view_get_selected_time_range (E_DAY_VIEW (page),
						    start_time, end_time);
	else if (page == gcal->week_view
		 || page == gcal->month_view)
		e_week_view_get_selected_time_range (E_WEEK_VIEW (page),
						     start_time, end_time);
#if 0
	else if (page == gcal->year_view_sw)
		year_view_set (YEAR_VIEW (gcal->year_view),
			       gcal->selection_start_time);
#endif
	else {
		g_warning ("My penguin is gone!");
		g_assert_not_reached ();
	}
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
