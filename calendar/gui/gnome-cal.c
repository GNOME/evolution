/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * GnomeCalendar widget
 * Copyright (C) 1998 the Free Software Foundation
 *
 * Authors: Miguel de Icaza (miguel@kernel.org)
 *          Federico Mena-Quintero <federico@helixcode.com>
 */

#include <config.h>
#include <gnome.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <gnome.h>
#include <widgets/e-paned/e-hpaned.h>
#include <widgets/e-paned/e-vpaned.h>
#include <cal-util/timeutil.h>
#include "dialogs/alarm-notify-dialog.h"
#include "alarm.h"
#include "e-calendar-table.h"
#include "e-day-view.h"
#include "e-week-view.h"
#include "event-editor.h"
#include "gnome-cal.h"
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

static void gnome_calendar_set_view_internal	(GnomeCalendar	*gcal,
						 char		*page_name,
						 gboolean	 range_selected);
static void gnome_calendar_set_pane_positions	(GnomeCalendar	*gcal);
static void gnome_calendar_update_view_times (GnomeCalendar *gcal);
static void gnome_calendar_update_date_navigator (GnomeCalendar *gcal);

static void gnome_calendar_on_date_navigator_style_set (GtkWidget *widget,
							GtkStyle  *previous_style,
							GnomeCalendar *gcal);
static void gnome_calendar_on_date_navigator_size_allocate (GtkWidget     *widget,
							    GtkAllocation *allocation,
							    GnomeCalendar *gcal);
static void gnome_calendar_on_date_navigator_date_range_changed (ECalendarItem *calitem,
								 GnomeCalendar *gcal);
static void gnome_calendar_on_date_navigator_selection_changed (ECalendarItem    *calitem,
								GnomeCalendar    *gcal);
static gboolean gnome_calendar_get_days_shown	(GnomeCalendar	*gcal,
						 GDate		*start_date,
						 gint		*days_shown);


static GtkVBoxClass *parent_class;

static void setup_alarm (GnomeCalendar *cal, CalAlarmInstance *ai);



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
	gcal->object_editor_hash = g_hash_table_new (g_str_hash, g_str_equal);
	gcal->alarms = g_hash_table_new (g_str_hash, g_str_equal);

	gcal->current_view_type = GNOME_CALENDAR_VIEW_NOT_SET;
	gcal->range_selected = FALSE;

	/* Set the default pane positions. These will eventually come from
	   gconf settings. They are multiples of calendar month widths &
	   heights in the date navigator. */
	gcal->hpane_pos = 1.0;
	gcal->vpane_pos = 1.0;
	gcal->hpane_pos_month_view = 0.0;
	gcal->vpane_pos_month_view = 1.0;

	gcal->ignore_view_button_clicks = FALSE;
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

/* Used from g_hash_table_foreach(); frees an UID string */
static void
free_uid (gpointer key, gpointer value, gpointer data)
{
	char *uid;

	uid = key;
	g_free (uid);
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

	g_hash_table_foreach (gcal->object_editor_hash, free_uid, NULL);
	g_hash_table_destroy (gcal->object_editor_hash);
	gcal->object_editor_hash = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
setup_widgets (GnomeCalendar *gcal)
{
	GtkWidget *w;

	/* The main HPaned, with the notebook of calendar views on the left
	   and the ECalendar and ToDo list on the right. */
	gcal->hpane = e_hpaned_new ();
	gtk_widget_show (gcal->hpane);
	gtk_box_pack_start (GTK_BOX (gcal), gcal->hpane, TRUE, TRUE, 0);

	/* The Notebook containing the 4 calendar views. */
	gcal->notebook = gtk_notebook_new ();
	gtk_notebook_set_show_border (GTK_NOTEBOOK (gcal->notebook), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (gcal->notebook), FALSE);
	gtk_widget_show (gcal->notebook);
	e_paned_pack1 (E_PANED (gcal->hpane), gcal->notebook, TRUE, TRUE);

	/* The VPaned widget, to contain the GtkCalendar & ToDo list. */
	gcal->vpane = e_vpaned_new ();
	gtk_widget_show (gcal->vpane);
	e_paned_pack2 (E_PANED (gcal->hpane), gcal->vpane, FALSE, TRUE);

	/* The ECalendar. */
	w = e_calendar_new ();
	gcal->date_navigator = E_CALENDAR (w);
	gtk_widget_show (w);
	e_paned_pack1 (E_PANED (gcal->vpane), w, FALSE, TRUE);
	gtk_signal_connect (GTK_OBJECT (gcal->date_navigator),
			    "style_set",
			    GTK_SIGNAL_FUNC (gnome_calendar_on_date_navigator_style_set),
			    gcal);
	gtk_signal_connect_after (GTK_OBJECT (gcal->date_navigator),
				  "size_allocate",
				  (GtkSignalFunc) gnome_calendar_on_date_navigator_size_allocate,
				  gcal);
	gtk_signal_connect (GTK_OBJECT (gcal->date_navigator->calitem),
			    "selection_changed",
			    (GtkSignalFunc) gnome_calendar_on_date_navigator_selection_changed,
			    gcal);
	gtk_signal_connect (GTK_OBJECT (gcal->date_navigator->calitem),
			    "date_range_changed",
			    GTK_SIGNAL_FUNC (gnome_calendar_on_date_navigator_date_range_changed),
			    gcal);

	/* The ToDo list. */
	gcal->todo = e_calendar_table_new ();
	e_paned_pack2 (E_PANED (gcal->vpane), gcal->todo, TRUE, TRUE);
	gtk_widget_show (gcal->todo);
	e_calendar_table_set_cal_client (E_CALENDAR_TABLE (gcal->todo),
					 gcal->client);


	/* The Day View. */
	gcal->day_view = e_day_view_new ();
	e_day_view_set_calendar (E_DAY_VIEW (gcal->day_view), gcal);
	e_day_view_set_cal_client (E_DAY_VIEW (gcal->day_view), gcal->client);
	gtk_widget_show (gcal->day_view);
	gtk_notebook_append_page (GTK_NOTEBOOK (gcal->notebook),
				  gcal->day_view, gtk_label_new (""));

	/* The Work Week View. */
	gcal->work_week_view = e_day_view_new ();
	e_day_view_set_work_week_view (E_DAY_VIEW (gcal->work_week_view),
				       TRUE);
	e_day_view_set_days_shown (E_DAY_VIEW (gcal->work_week_view), 5);
	e_day_view_set_calendar (E_DAY_VIEW (gcal->work_week_view), gcal);
	e_day_view_set_cal_client (E_DAY_VIEW (gcal->work_week_view), gcal->client);
	gtk_widget_show (gcal->work_week_view);
	gtk_notebook_append_page (GTK_NOTEBOOK (gcal->notebook),
				  gcal->work_week_view, gtk_label_new (""));

	/* The Week View. */
	gcal->week_view = e_week_view_new ();
	e_week_view_set_calendar (E_WEEK_VIEW (gcal->week_view), gcal);
	e_week_view_set_cal_client (E_WEEK_VIEW (gcal->week_view), gcal->client);
	gtk_widget_show (gcal->week_view);
	gtk_notebook_append_page (GTK_NOTEBOOK (gcal->notebook),
				  gcal->week_view, gtk_label_new (""));

	/* The Month View. */
	gcal->month_view = e_week_view_new ();
	e_week_view_set_calendar (E_WEEK_VIEW (gcal->month_view), gcal);
	e_week_view_set_multi_week_view (E_WEEK_VIEW (gcal->month_view), TRUE);
	e_week_view_set_cal_client (E_WEEK_VIEW (gcal->month_view), gcal->client);
	gtk_widget_show (gcal->month_view);
	gtk_notebook_append_page (GTK_NOTEBOOK (gcal->notebook),
				  gcal->month_view, gtk_label_new (""));
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
	else if (page == gcal->work_week_view)
		return "workweekview";
	else if (page == gcal->week_view)
		return "weekview";
	else if (page == gcal->month_view)
		return "monthview";
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
	gnome_calendar_update_view_times (gcal);
	gnome_calendar_update_date_navigator (gcal);
}


static void
gnome_calendar_update_view_times (GnomeCalendar *gcal)
{
	GtkWidget *page;

	page = get_current_page (gcal);

	if (page == gcal->day_view || page == gcal->work_week_view) {
		e_day_view_set_selected_time_range
			(E_DAY_VIEW (page),
			 gcal->selection_start_time,
			 gcal->selection_end_time);
	} else if (page == gcal->week_view || page == gcal->month_view) {
		e_week_view_set_selected_time_range
			(E_WEEK_VIEW (page),
			 gcal->selection_start_time,
			 gcal->selection_end_time);
	} else {
		g_warning ("My penguin is gone!");
		g_assert_not_reached ();
	}
}

static void
gnome_calendar_direction (GnomeCalendar *gcal, int direction)
{
	GtkWidget *cp = get_current_page (gcal);
	time_t start_time, end_time;

	start_time = gcal->selection_start_time;
	end_time = gcal->selection_end_time;

	if (cp == gcal->day_view) {
		start_time = time_add_day (start_time, direction);
		end_time = time_add_day (end_time, direction);
	} else if (cp == gcal->work_week_view) {
		start_time = time_add_week (start_time, direction);
		end_time = time_add_week (end_time, direction);
	} else if (cp == gcal->week_view) {
		start_time = time_add_week (start_time, direction);
		end_time = time_add_week (end_time, direction);
	} else if (cp == gcal->month_view) {
		start_time = time_add_month (start_time, direction);
		end_time = time_add_month (end_time, direction);
	} else {
		g_warning ("Weee!  Where did the penguin go?");
		g_assert_not_reached ();
		start_time = 0;
		end_time = 0;
	}

	gcal->selection_start_time = start_time;
	gcal->selection_end_time = end_time;

	gnome_calendar_update_view_times (gcal);
	gnome_calendar_update_date_navigator (gcal);
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

	gcal->selection_start_time = time_day_begin (time);
	gcal->selection_end_time = time_add_day (gcal->selection_start_time,
						 1);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gcal->day_button),
				      TRUE);
}

void
gnome_calendar_goto_today (GnomeCalendar *gcal)
{
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	gnome_calendar_goto (gcal, time (NULL));

	gtk_widget_grab_focus (get_current_page (gcal));
}


/* This sets which view is currently shown. It also updates the selection time
   of the view so it shows the appropriate days. */
void
gnome_calendar_set_view		(GnomeCalendar	*gcal,
				 char		*page_name,
				 gboolean	 range_selected)
{
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (page_name != NULL);

	gnome_calendar_set_view_internal (gcal, page_name, range_selected);
	gnome_calendar_update_view_times (gcal);
	gnome_calendar_update_date_navigator (gcal);
}


/* This sets the view without changing the selection or updating the date
   navigator. If a range of dates isn't selected it will also reset the number
   of days/weeks shown to the default (i.e. 1 day for the day view or 5 weeks
   for the month view). */
static void
gnome_calendar_set_view_internal	(GnomeCalendar	*gcal,
					 char		*page_name,
					 gboolean	 range_selected)
{
	int view;
	gboolean round_selection = FALSE;

	g_print ("In gnome_calendar_set_view_internal: %s\n", page_name);

	if (!strcmp (page_name, "dayview")) {
		view = GNOME_CALENDAR_VIEW_DAY;
		if (!range_selected)
			e_day_view_set_days_shown
				(E_DAY_VIEW (gcal->day_view), 1);
	} else if (!strcmp (page_name, "workweekview")) {
		view = GNOME_CALENDAR_VIEW_WORK_WEEK;
	} else if (!strcmp (page_name, "weekview")) {
		view = GNOME_CALENDAR_VIEW_WEEK;
		round_selection = TRUE;
	} else if (!strcmp (page_name, "monthview")) {
		view = GNOME_CALENDAR_VIEW_MONTH;
		if (!range_selected)
			e_week_view_set_weeks_shown
				(E_WEEK_VIEW (gcal->month_view), 5);
		round_selection = TRUE;
	} else {
		g_warning ("Unknown calendar view: %s", page_name);
		return;
	}

	gcal->current_view_type = view;
	gcal->range_selected = range_selected;

	gtk_notebook_set_page (GTK_NOTEBOOK (gcal->notebook), view);

	gnome_calendar_set_pane_positions (gcal);

	/* For the week & month views we want the selection in the date
	   navigator to be rounded to the nearest week when the arrow buttons
	   are pressed to move to the previous/next month. */
	gtk_object_set (GTK_OBJECT (gcal->date_navigator->calitem),
			"round_selection_when_moving", round_selection,
			NULL);
}


static void
gnome_calendar_set_pane_positions	(GnomeCalendar	*gcal)
{
	gint top_border, bottom_border, left_border, right_border;
	gint col_width, row_height;
	gfloat right_pane_width, top_pane_height;

	/* Get the size of the calendar month width & height. */
	e_calendar_get_border_size (gcal->date_navigator,
				    &top_border, &bottom_border,
				    &left_border, &right_border);
	gtk_object_get (GTK_OBJECT (gcal->date_navigator->calitem),
			"row_height", &row_height,
			"column_width", &col_width,
			NULL);

	if (gcal->current_view_type == GNOME_CALENDAR_VIEW_MONTH
	    && !gcal->range_selected) {
		right_pane_width = gcal->hpane_pos_month_view;
		top_pane_height = gcal->vpane_pos_month_view;
	} else {
		right_pane_width = gcal->hpane_pos;
		top_pane_height = gcal->vpane_pos;
	}

	/* We add the borders before multiplying due to the way we are using
	   the EPaned quantum feature. */
	if (right_pane_width < 0.001)
		right_pane_width = 0.0;
	else
		right_pane_width = right_pane_width * (col_width
			+ left_border + right_border) + 0.5;
	if (top_pane_height < 0.001)
		top_pane_height = 0.0;
	else
		top_pane_height = top_pane_height * (row_height
			+ top_border + bottom_border) + 0.5;

	g_print ("right width:%g top height:%g\n", right_pane_width,
		 top_pane_height);

	e_paned_set_position (E_PANED (gcal->hpane), -1);
	e_paned_set_position (E_PANED (gcal->vpane), -1);
	/* We add one to each dimension since we can't use 0. */
	gtk_widget_set_usize (gcal->vpane, right_pane_width + 1, -2);
	gtk_widget_set_usize (GTK_WIDGET (gcal->date_navigator),
			      -2, top_pane_height + 1);
}


#ifndef NO_WARNINGS
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
#endif

/* Queues a snooze alarm */
static void
snooze (GnomeCalendar *gcal, CalComponent *comp, time_t occur, int snooze_mins, gboolean audio)
{
	time_t now, trigger;
	struct tm tm;
	CalAlarmInstance ai;
	
	now = time (NULL);
	tm = *localtime (&now);
	tm.tm_min += snooze_mins;

	trigger = mktime (&tm);
	if (trigger == -1) {
		g_message ("snooze(): produced invalid time_t; not queueing alarm!");
		return;
	}

#if 0
	cal_component_get_uid (comp, &ai.uid);
	ai.type = audio ? ALARM_AUDIO : ALARM_DISPLAY;
#endif
	ai.trigger = trigger;
	ai.occur = occur;

	setup_alarm (gcal, &ai);
}

/* Edits an appointment from the alarm notification dialog */
static void
edit (GnomeCalendar *gcal, CalComponent *comp)
{
	gnome_calendar_edit_object (gcal, comp);
}

struct alarm_notify_closure {
	GnomeCalendar *gcal;
	CalComponent *comp;
	time_t occur;
};

/* Callback used for the result of the alarm notification dialog */
static void
display_notification_cb (AlarmNotifyResult result, int snooze_mins, gpointer data)
{
	struct alarm_notify_closure *c;

	c = data;

	switch (result) {
	case ALARM_NOTIFY_CLOSE:
		break;

	case ALARM_NOTIFY_SNOOZE:
		snooze (c->gcal, c->comp, c->occur, snooze_mins, FALSE);
		break;

	case ALARM_NOTIFY_EDIT:
		edit (c->gcal, c->comp);
		break;

	default:
		g_assert_not_reached ();
	}

	gtk_object_unref (GTK_OBJECT (c->comp));
	g_free (c);
}

/* Present a display notification of an alarm trigger */
static void
display_notification (time_t trigger, time_t occur, CalComponent *comp, GnomeCalendar *gcal)
{
	gboolean result;
	struct alarm_notify_closure *c;

	gtk_object_ref (GTK_OBJECT (comp));

	c = g_new (struct alarm_notify_closure, 1);
	c->gcal = gcal;
	c->comp = comp;
	c->occur = occur;

	result = alarm_notify_dialog (trigger, occur, comp, display_notification_cb, c);
	if (!result) {
		g_message ("display_notification(): could not display the alarm notification dialog");
		g_free (c);
		gtk_object_unref (GTK_OBJECT (comp));
	}
}

/* Present an audible notification of an alarm trigger */
static void
audio_notification (time_t trigger, time_t occur, CalComponent *comp, GnomeCalendar *gcal)
{
	g_message ("AUDIO NOTIFICATION!");
	/* FIXME */
}

struct trigger_alarm_closure {
	GnomeCalendar *gcal;
	char *uid;
	CalComponentAlarmAction type;
	time_t occur;
};

/* Callback function used when an alarm is triggered */
static void
trigger_alarm_cb (gpointer alarm_id, time_t trigger, gpointer data)
{
	struct trigger_alarm_closure *c;
	CalComponent *comp;
	CalClientGetStatus status;
	const char *uid;
	ObjectAlarms *oa;
   	GList *l;

	c = data;

	/* Fetch the object */

	status = cal_client_get_object (c->gcal->client, c->uid, &comp);

	switch (status) {
	case CAL_CLIENT_GET_SUCCESS:
		/* Go on */
		break;
	case CAL_CLIENT_GET_SYNTAX_ERROR:
	case CAL_CLIENT_GET_NOT_FOUND:
		g_message ("trigger_alarm_cb(): syntax error in fetched object");
		return;
	}

	g_assert (comp != NULL);

	/* Present notification */

	switch (c->type) {
	case CAL_COMPONENT_ALARM_EMAIL:
#if 0
		g_assert (ico->malarm.enabled);
		mail_notification (ico->malarm.data, ico->summary, c->occur);
#endif
		break;

	case CAL_COMPONENT_ALARM_PROCEDURE:
#if 0
		g_assert (ico->palarm.enabled);
		program_notification (ico->palarm.data, FALSE);
#endif
		break;

	case CAL_COMPONENT_ALARM_DISPLAY:
#if 0
		g_assert (ico->dalarm.enabled);
#endif
		display_notification (trigger, c->occur, comp, c->gcal);
		break;

	case CAL_COMPONENT_ALARM_AUDIO:
#if 0
		g_assert (ico->aalarm.enabled);
#endif
		audio_notification (trigger, c->occur, comp, c->gcal);
		break;

	default:
		break;
	}

	/* Remove the alarm from the hash table */
	cal_component_get_uid (comp, &uid);
	oa = g_hash_table_lookup (c->gcal->alarms, uid);
	g_assert (oa != NULL);

	l = g_list_find (oa->alarm_ids, alarm_id);
	g_assert (l != NULL);

	oa->alarm_ids = g_list_remove_link (oa->alarm_ids, l);
	g_list_free_1 (l);

	if (!oa->alarm_ids) {
		g_hash_table_remove (c->gcal->alarms, uid);
		g_free (oa->uid);
		g_free (oa);
	}
	
	gtk_object_unref (GTK_OBJECT (comp));
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
#if 0
	c->type = ai->type;
#endif
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
load_alarms (GnomeCalendar *gcal)
{
	time_t now;
	time_t end_of_day;
	GList *alarms, *l;

	now = time (NULL);
	end_of_day = time_day_end (now);

	/* Queue alarms */

	alarms = cal_client_get_alarms_in_range (gcal->client, now, end_of_day);

	for (l = alarms; l; l = l->next)
		setup_alarm (gcal, l->data);

	cal_alarm_instance_list_free (alarms);

	/* Queue the midnight alarm refresh */

	gcal->midnight_alarm_refresh_id = alarm_add (end_of_day, midnight_refresh_cb, gcal, NULL);
	if (!gcal->midnight_alarm_refresh_id) {
		g_message ("load_alarms(): Could not set up the midnight refresh alarm!");
		/* FIXME: what to do? */
	}
}

/* This tells all components to reload all calendar objects. */
static void
gnome_calendar_update_all (GnomeCalendar *cal)
{
	load_alarms (cal);
	gnome_calendar_tag_calendar (cal, cal->date_navigator);
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

/* Adds today's alarms for the specified object */
static void
add_alarms_for_object (GnomeCalendar *gcal, const char *uid)
{
	GList *alarms;
	gboolean result;
	time_t now, end_of_day;
	GList *l;

	now = time (NULL);
	end_of_day = time_day_end (now);

	result = cal_client_get_alarms_for_object (gcal->client, uid, now, end_of_day, &alarms);
	if (!result) {
		/* FIXME: should we warn here, or is it OK if the object
		 * disappeared in the meantime?
		 */
		return;
	}

	for (l = alarms; l; l = l->next)
		setup_alarm (gcal, l->data);

	cal_alarm_instance_list_free (alarms);
}

static void
gnome_calendar_object_updated_cb (GtkWidget *cal_client,
				  const char *uid,
				  GnomeCalendar  *gcal)
{
	remove_alarms_for_object (gcal, uid);
	add_alarms_for_object (gcal, uid);

	gnome_calendar_tag_calendar (gcal, gcal->date_navigator);
}


static void
gnome_calendar_object_removed_cb (GtkWidget *cal_client,
				  const char *uid,
				  GnomeCalendar  *gcal)
{
	remove_alarms_for_object (gcal, uid);

	gnome_calendar_tag_calendar (gcal, gcal->date_navigator);
}


GtkWidget *
gnome_calendar_new (char *title)
{
	GtkWidget      *retval;
	GnomeCalendar  *gcal;

	retval = gtk_type_new (gnome_calendar_get_type ());

	gcal = GNOME_CALENDAR (retval);

	gcal->selection_start_time = time_day_begin (time (NULL));
	gcal->selection_end_time = time_add_day (gcal->selection_start_time, 1);
	gcal->client = cal_client_new ();

	setup_widgets (gcal);

	gnome_calendar_set_view (gcal, "dayview", FALSE);

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
gnome_calendar_load_cb (CalClient *cal_client,
			CalClientLoadStatus status,
			load_or_create_data *locd)
{
	g_return_if_fail (locd);
	g_return_if_fail (GNOME_IS_CALENDAR (locd->gcal));

	switch (status) {
	case CAL_CLIENT_LOAD_SUCCESS:
		gnome_calendar_update_all (locd->gcal);
		break;

	case CAL_CLIENT_LOAD_ERROR:
		if (locd->gcom == CALENDAR_OPEN_OR_CREATE) {
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

	if (cal_client_load_calendar (gcal->client, file) == FALSE) {
		g_message ("Error loading calendar: %s", file);
		return 0;
	}

	return 1;
}

#ifndef NO_WARNINGS
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
#endif

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

struct calendar_tag_closure
{
	ECalendarItem *calitem;
	time_t start_time;
	time_t end_time;
};

/* Marks the specified range in a GtkCalendar */
static gboolean
gnome_calendar_tag_calendar_cb (CalComponent *comp,
				time_t istart,
				time_t iend,
				gpointer data)
{
	struct calendar_tag_closure *c = data;
	time_t t;

	t = time_day_begin (istart);

	do {
		struct tm tm;

		tm = *localtime (&t);

		e_calendar_item_mark_day (c->calitem, tm.tm_year + 1900,
					  tm.tm_mon, tm.tm_mday,
					  E_CALENDAR_ITEM_MARK_BOLD);

		t = time_day_end (t);
	} while (t < iend);

	return TRUE;
}

/*
 * Tags the dates with appointments in a GtkCalendar based on the
 * GnomeCalendar contents
 */
void
gnome_calendar_tag_calendar (GnomeCalendar *gcal, ECalendar *ecal)
{
	struct calendar_tag_closure c;
	gint start_year, start_month, start_day;
	gint end_year, end_month, end_day;
	struct tm start_tm = { 0 }, end_tm = { 0 };

	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (ecal != NULL);
	g_return_if_fail (E_IS_CALENDAR (ecal));

	/* If the ECalendar isn't visible, we just return. */
	if (!GTK_WIDGET_VISIBLE (ecal))
		return;

	e_calendar_item_get_date_range	(ecal->calitem,
					 &start_year, &start_month, &start_day,
					 &end_year, &end_month, &end_day);

	start_tm.tm_year = start_year - 1900;
	start_tm.tm_mon = start_month;
	start_tm.tm_mday = start_day;
	start_tm.tm_hour = 0;
	start_tm.tm_min = 0;
	start_tm.tm_sec = 0;
	start_tm.tm_isdst = -1;

	end_tm.tm_year = end_year - 1900;
	end_tm.tm_mon = end_month;
	end_tm.tm_mday = end_day;
	end_tm.tm_hour = 0;
	end_tm.tm_min = 0;
	end_tm.tm_sec = 0;
	end_tm.tm_isdst = -1;

	e_calendar_item_clear_marks (ecal->calitem);

	c.calitem = ecal->calitem;
	c.start_time = mktime (&start_tm);
	c.end_time = mktime (&end_tm);

	cal_client_generate_instances (gcal->client, CALOBJ_TYPE_EVENT, 
				       c.start_time, c.end_time,
				       gnome_calendar_tag_calendar_cb, &c);
}


/* This is called when the day begin & end times, the AM/PM flag, or the
   week_starts_on_monday flags are changed.
   FIXME: Which of these options do we want the new views to support? */
void
gnome_calendar_time_format_changed (GnomeCalendar *gcal)
{
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
#if 0
	gtk_calendar_display_options (gcal->gtk_calendar,
				      (week_starts_on_monday
				       ? (gcal->gtk_calendar->display_flags
					  | GTK_CALENDAR_WEEK_START_MONDAY)
				       : (gcal->gtk_calendar->display_flags
					  & ~GTK_CALENDAR_WEEK_START_MONDAY)));
#endif
}

/* This is called when any of the color settings are changed.
   FIXME: Need to update for the new views. */
void
gnome_calendar_colors_changed (GnomeCalendar *gcal)
{
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
}

void
gnome_calendar_todo_properties_changed (GnomeCalendar *gcal)
{
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
}


void
gnome_calendar_set_selected_time_range (GnomeCalendar *gcal,
					time_t	       start_time,
					time_t	       end_time)
{
	gcal->selection_start_time = start_time;
	gcal->selection_end_time = end_time;

	gnome_calendar_update_date_navigator (gcal);
}


/* Callback used when an event editor requests that an object be saved */
static void
save_event_object_cb (EventEditor *ee, CalComponent *comp, gpointer data)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);
	if (!cal_client_update_object (gcal->client, comp))
		g_message ("save_event_object_cb(): Could not update the object!");
}


/* Callback used when an event editor finishes editing an object */
static void
released_event_object_cb (EventEditor *ee, const char *uid, gpointer data)
{
	GnomeCalendar *gcal;
	gboolean result;
	gpointer orig_key;
	char *orig_uid;

	gcal = GNOME_CALENDAR (data);

	result = g_hash_table_lookup_extended (gcal->object_editor_hash, uid, &orig_key, NULL);
	g_assert (result != FALSE);

	orig_uid = orig_key;

	g_hash_table_remove (gcal->object_editor_hash, orig_uid);
	g_free (orig_uid);
}

/* Callback used when an event editor dialog is closed */
static void
editor_closed_cb (EventEditor *ee, gpointer data)
{
	gtk_object_unref (GTK_OBJECT (ee));
}

void
gnome_calendar_edit_object (GnomeCalendar *gcal, CalComponent *comp)
{
	EventEditor *ee;
	const char *uid;
	
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (comp != NULL);

	cal_component_get_uid (comp, &uid);

	ee = g_hash_table_lookup (gcal->object_editor_hash, uid);
	if (!ee) {
		ee = event_editor_new ();
		if (!ee) {
			g_message ("gnome_calendar_edit_object(): Could not create the event editor");
			return;
		}

		/* FIXME: what to do when an event editor wants to switch
		 * objects?  We would need to know about it as well.
		 */

		g_hash_table_insert (gcal->object_editor_hash, g_strdup (uid), ee);

		gtk_signal_connect (GTK_OBJECT (ee), "save_event_object",
				    GTK_SIGNAL_FUNC (save_event_object_cb),
				    gcal);

		gtk_signal_connect (GTK_OBJECT (ee), "released_event_object",
				    GTK_SIGNAL_FUNC (released_event_object_cb),
				    gcal);

		gtk_signal_connect (GTK_OBJECT (ee), "editor_closed",
				    GTK_SIGNAL_FUNC (editor_closed_cb), gcal);

		event_editor_set_event_object (EVENT_EDITOR (ee), comp);
	}

	event_editor_focus (ee);
}

/**
 * gnome_calendar_new_appointment:
 * @gcal: An Evolution calendar.
 * 
 * Opens an event editor dialog for a new appointment.  The appointment's start
 * and end times are set to the currently selected time range in the calendar
 * views.
 **/
void
gnome_calendar_new_appointment (GnomeCalendar *gcal)
{
	CalComponent *comp;
	time_t dtstart, dtend;
	CalComponentDateTime dt;
	struct icaltimetype itt;

	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	gnome_calendar_get_current_time_range (gcal, &dtstart, &dtend);
	dt.value = &itt;
	dt.tzid = NULL;

	comp = cal_component_new ();
	cal_component_set_new_vtype (comp, CAL_COMPONENT_EVENT);

	itt = icaltime_from_timet (dtstart, 0, TRUE);
	cal_component_set_dtstart (comp, &dt);

	itt = icaltime_from_timet (dtend, 0, TRUE);
	cal_component_set_dtend (comp, &dt);

	gnome_calendar_edit_object (gcal, comp);
	gtk_object_unref (GTK_OBJECT (comp));
	
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
	else {
		g_message ("My penguin is gone!");
		g_assert_not_reached ();
	}
}



/* This updates the month shown and the days selected in the calendar, if
   necessary. */
static void
gnome_calendar_update_date_navigator (GnomeCalendar *gcal)
{
	GDate start_date, end_date;
	gint days_shown;

	/* If the ECalendar isn't visible, we just return. */
	if (!GTK_WIDGET_VISIBLE (gcal->date_navigator))
		return;

	if (gnome_calendar_get_days_shown (gcal, &start_date, &days_shown)) {
		end_date = start_date;
		g_date_add_days (&end_date, days_shown - 1);

		e_calendar_item_set_selection (gcal->date_navigator->calitem,
					       &start_date, &end_date);
	}
}


static gboolean
gnome_calendar_get_days_shown	(GnomeCalendar	*gcal,
				 GDate		*start_date,
				 gint		*days_shown)
{
	GtkWidget *page;

	page = get_current_page (gcal);

	if (page == gcal->day_view || page == gcal->work_week_view) {
		g_date_clear (start_date, 1);
		g_date_set_time (start_date, E_DAY_VIEW (page)->lower);
		*days_shown = e_day_view_get_days_shown (E_DAY_VIEW (page));
		return TRUE;
	} else if (page == gcal->week_view || page == gcal->month_view) {
		*start_date = E_WEEK_VIEW (page)->first_day_shown;
		if (e_week_view_get_multi_week_view (E_WEEK_VIEW (page)))
			*days_shown = e_week_view_get_weeks_shown (E_WEEK_VIEW (page)) * 7;
		else
			*days_shown = 7;
		return TRUE;
	} else {
		g_warning ("gnome_calendar_get_days_shown - Invalid page");
		return FALSE;
	}
}


static void
gnome_calendar_on_date_navigator_selection_changed (ECalendarItem    *calitem,
						    GnomeCalendar    *gcal)
{
	GDate start_date, end_date, new_start_date, new_end_date;
	gint days_shown, new_days_shown;
	gint start_year, start_month, start_day;
	gint end_year, end_month, end_day;
	gboolean starts_on_week_start_day = FALSE;
	struct tm tm;

	g_print ("In gnome_calendar_on_date_navigator_selection_changed\n");

	if (!gnome_calendar_get_days_shown (gcal, &start_date, &days_shown))
		return;

	end_date = start_date;
	g_date_add_days (&end_date, days_shown - 1);

	e_calendar_item_get_selection	(calitem, &new_start_date,
					 &new_end_date);

	/* If the selection hasn't changed just return. */
	if (!g_date_compare (&start_date, &new_start_date)
	    && !g_date_compare (&end_date, &new_end_date))
		return;

	new_days_shown = g_date_julian (&new_end_date)
		- g_date_julian (&new_start_date) + 1;

	/* FIXME: This assumes weeks start on Monday for now. */
	if (g_date_weekday (&new_start_date) - 1 == 0)
		starts_on_week_start_day = TRUE;

	/* Switch views as appropriate, and change the number of days or weeks
	   shown. */
	if (new_days_shown > 9) {
		e_week_view_set_weeks_shown (E_WEEK_VIEW (gcal->month_view),
					     (new_days_shown + 6) / 7);
		e_week_view_set_first_day_shown (E_WEEK_VIEW (gcal->month_view),
						 &new_start_date);
		gnome_calendar_set_view_internal (gcal, "monthview", TRUE);
		gnome_calendar_update_date_navigator (gcal);
	} else if (new_days_shown == 7 && starts_on_week_start_day) {
		e_week_view_set_first_day_shown (E_WEEK_VIEW (gcal->week_view),
						 &new_start_date);
		gnome_calendar_set_view_internal (gcal, "weekview", TRUE);
		gnome_calendar_update_date_navigator (gcal);
	} else {
		start_year = g_date_year (&new_start_date);
		start_month = g_date_month (&new_start_date) - 1;
		start_day = g_date_day (&new_start_date);
		end_year = g_date_year (&new_end_date);
		end_month = g_date_month (&new_end_date) - 1;
		end_day = g_date_day (&new_end_date);

		tm.tm_year = start_year - 1900;
		tm.tm_mon  = start_month;
		tm.tm_mday = start_day;
		tm.tm_hour = 0;
		tm.tm_min  = 0;
		tm.tm_sec  = 0;
		tm.tm_isdst = -1;
		gcal->selection_start_time = mktime (&tm);

		tm.tm_year = end_year - 1900;
		tm.tm_mon  = end_month;
		tm.tm_mday = end_day + 1; /* mktime() will normalize this. */
		tm.tm_hour = 0;
		tm.tm_min  = 0;
		tm.tm_sec  = 0;
		tm.tm_isdst = -1;
		gcal->selection_end_time = mktime (&tm);

		e_day_view_set_days_shown (E_DAY_VIEW (gcal->day_view),
					   new_days_shown);
		gnome_calendar_set_view (gcal, "dayview", TRUE);
	}

	gnome_calendar_update_view_buttons (gcal);
	gtk_widget_grab_focus (get_current_page (gcal));
}


static void
gnome_calendar_on_date_navigator_date_range_changed (ECalendarItem *calitem,
						     GnomeCalendar *gcal)
{
	gnome_calendar_tag_calendar (gcal, gcal->date_navigator);
}


static void
gnome_calendar_on_date_navigator_style_set (GtkWidget     *widget,
					    GtkStyle      *previous_style,
					    GnomeCalendar *gcal)
{
	ECalendar *ecal;
	gint row_height, col_width;
	gint top_border, bottom_border, left_border, right_border;

	g_return_if_fail (E_IS_CALENDAR (widget));
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	ecal = E_CALENDAR (widget);

	e_calendar_get_border_size (gcal->date_navigator,
				    &top_border, &bottom_border,
				    &left_border, &right_border);
	gtk_object_get (GTK_OBJECT (ecal->calitem),
			"row_height", &row_height,
			"column_width", &col_width,
			NULL);

	/* The EPaned quantum feature works better if we add on the calendar
	   borders to the quantum size. Otherwise if you shrink the date
	   navigator you get left with the border widths/heights which looks
	   bad. EPaned should be more flexible really. */
	col_width += left_border + right_border;
	row_height += top_border + bottom_border;

	/* We don't have to use the EPaned quantum feature. We could just let
	   the calendar expand to fill the allocated space, showing as many
	   months as will fit. But for that to work nicely the EPaned should
	   resize the widgets as the bar is dragged. Otherwise the user has
	   to mess around to get the number of months that they want. */
#if 1
	gtk_object_set (GTK_OBJECT (gcal->hpane),
			"quantum", (guint) col_width,
			NULL);
	gtk_object_set (GTK_OBJECT (gcal->vpane),
			"quantum", (guint) row_height,
			NULL);
#endif

	gnome_calendar_set_pane_positions (gcal);
}


static void
gnome_calendar_on_date_navigator_size_allocate (GtkWidget     *widget,
						GtkAllocation *allocation,
						GnomeCalendar *gcal)
{
	gint width, height, row_height, col_width;
	gint top_border, bottom_border, left_border, right_border;
	gfloat hpane_pos, vpane_pos;

	g_print ("In gnome_calendar_on_date_navigator_size_allocate %ix%i\n",
		 allocation->width, allocation->height);

	if (gcal->current_view_type != GNOME_CALENDAR_VIEW_NOT_SET) {
		e_calendar_get_border_size (gcal->date_navigator,
					    &top_border, &bottom_border,
					    &left_border, &right_border);
		gtk_object_get (GTK_OBJECT (gcal->date_navigator->calitem),
				"row_height", &row_height,
				"column_width", &col_width,
				NULL);

		/* We subtract one from each dimension since we added 1 in
		   gnome_calendar_set_view_internal(). */
		width = allocation->width - 1;
		height = allocation->height - 1;

		/* We add the border sizes to work around the EPaned
		   quantized feature. */
		col_width += left_border + right_border;
		row_height += top_border + bottom_border;

		hpane_pos = (gfloat) width / col_width;
		vpane_pos = (gfloat) height / row_height;

		if (gcal->current_view_type == GNOME_CALENDAR_VIEW_MONTH
		    && !gcal->range_selected) {
			gcal->hpane_pos_month_view = hpane_pos;
			gcal->vpane_pos_month_view = vpane_pos;
		} else {
			gcal->hpane_pos = hpane_pos;
			gcal->vpane_pos = vpane_pos;
		}

		g_print ("  hpane_pos:%g vpane_pos:%g\n", hpane_pos, vpane_pos);

	}
}


/* This makes the appropriate radio button in the toolbar active.
   It sets the ignore_view_button_clicks flag so the "clicked" signal handlers
   just return without doing anything. */
void
gnome_calendar_update_view_buttons	(GnomeCalendar	*gcal)
{
	GtkWidget *page, *button;

	page = get_current_page (gcal);

	if (page == gcal->day_view)
		button = gcal->day_button;
	else if (page == gcal->work_week_view)
		button = gcal->work_week_button;
	else if (page == gcal->week_view)
		button = gcal->week_button;
	else if (page == gcal->month_view)
		button = gcal->month_button;
	else {
		g_warning ("Unknown calendar view");
		button = gcal->day_button;
	}

	gcal->ignore_view_button_clicks = TRUE;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gcal->ignore_view_button_clicks = FALSE;
}
