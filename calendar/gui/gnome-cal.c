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



/* These must match the page numbers in the GtkNotebook. */
typedef enum {
	VIEW_NOT_SET = -1,
	VIEW_DAY = 0,
	VIEW_WORK_WEEK,
	VIEW_WEEK,
	VIEW_MONTH
} ViewType;

/* States for the calendar loading and creation state machine */
typedef enum {
	LOAD_STATE_NOT_LOADED,
	LOAD_STATE_WAIT_LOAD,
	LOAD_STATE_WAIT_LOAD_BEFORE_CREATE,
	LOAD_STATE_WAIT_CREATE,
	LOAD_STATE_LOADED
} LoadState;

/* Private part of the GnomeCalendar structure */
struct _GnomeCalendarPrivate {
	CalClient   *client;

	/* Loading state; we can be loading or creating a calendar */
	LoadState load_state;

	/* Mapping of component UIDs to event editors */
	GHashTable  *object_editor_hash;

	/* This is the last selection explicitly selected by the user. We try
	   to keep it the same when we switch views, but we may have to alter
	   it depending on the view (e.g. the week views only select days, so
	   any times are lost. */
	time_t      selection_start_time;
	time_t      selection_end_time;

	/* Widgets */

	GtkWidget   *hpane;
	GtkWidget   *notebook;
	GtkWidget   *vpane;
	ECalendar   *date_navigator;
	GtkWidget   *todo;

	GtkWidget   *day_view;
	GtkWidget   *work_week_view;
	GtkWidget   *week_view;
	GtkWidget   *month_view;

	/* These are the toolbar radio buttons for switching views. */
	GtkWidget   *day_button;
	GtkWidget   *work_week_button;
	GtkWidget   *week_button;
	GtkWidget   *month_button;

	/* This is the view currently shown. We use it to keep track of the
	   positions of the panes. range_selected is TRUE if a range of dates
	   was selected in the date navigator to show the view. */
	ViewType current_view_type;
	gboolean range_selected;

	/* These are the saved positions of the panes. They are multiples of
	   calendar month widths & heights in the date navigator, so that they
	   will work OK after theme changes. */
	gfloat	     hpane_pos;
	gfloat	     vpane_pos;
	gfloat	     hpane_pos_month_view;
	gfloat	     vpane_pos_month_view;

	/* The signal handler id for our GtkCalendar "day_selected" handler. */
	guint	     day_selected_id;

	/* Alarm ID for the midnight refresh function */
	gpointer midnight_alarm_refresh_id;

	/* UID->alarms hash */
	GHashTable *alarms;
};



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
						 gboolean	 range_selected,
						 gboolean	 focus);
static void gnome_calendar_set_pane_positions	(GnomeCalendar	*gcal);
static void gnome_calendar_update_view_times (GnomeCalendar *gcal);
static void gnome_calendar_update_date_navigator (GnomeCalendar *gcal);

static void gnome_calendar_on_date_navigator_style_set (GtkWidget *widget,
							GtkStyle  *previous_style,
							gpointer data);
static void gnome_calendar_on_date_navigator_size_allocate (GtkWidget     *widget,
							    GtkAllocation *allocation,
							    gpointer data);
static void gnome_calendar_on_date_navigator_date_range_changed (ECalendarItem *calitem,
								 GnomeCalendar *gcal);
static void gnome_calendar_on_date_navigator_selection_changed (ECalendarItem    *calitem,
								GnomeCalendar    *gcal);
static gboolean gnome_calendar_get_days_shown	(GnomeCalendar	*gcal,
						 GDate		*start_date,
						 gint		*days_shown);


static GtkVBoxClass *parent_class;

static void setup_alarm (GnomeCalendar *cal, CalAlarmInstance *ai);



GtkType
gnome_calendar_get_type (void)
{
	static GtkType gnome_calendar_type = 0;

	if (!gnome_calendar_type) {
		static const GtkTypeInfo gnome_calendar_info = {
			"GnomeCalendar",
			sizeof (GnomeCalendar),
			sizeof (GnomeCalendarClass),
			(GtkClassInitFunc) gnome_calendar_class_init,
			(GtkObjectInitFunc) gnome_calendar_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		gnome_calendar_type = gtk_type_unique (GTK_TYPE_VBOX, &gnome_calendar_info);
	}

	return gnome_calendar_type;
}

/* Class initialization function for the gnome calendar */
static void
gnome_calendar_class_init (GnomeCalendarClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (GTK_TYPE_VBOX);

	object_class->destroy = gnome_calendar_destroy;
}

static void
setup_widgets (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	GtkWidget *w;

	priv = gcal->priv;

	/* The main HPaned, with the notebook of calendar views on the left
	   and the ECalendar and ToDo list on the right. */
	priv->hpane = e_hpaned_new ();
	gtk_widget_show (priv->hpane);
	gtk_box_pack_start (GTK_BOX (gcal), priv->hpane, TRUE, TRUE, 0);

	/* The Notebook containing the 4 calendar views. */
	priv->notebook = gtk_notebook_new ();
	gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->notebook), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);
	gtk_widget_show (priv->notebook);
	e_paned_pack1 (E_PANED (priv->hpane), priv->notebook, TRUE, TRUE);

	/* The VPaned widget, to contain the GtkCalendar & ToDo list. */
	priv->vpane = e_vpaned_new ();
	gtk_widget_show (priv->vpane);
	e_paned_pack2 (E_PANED (priv->hpane), priv->vpane, FALSE, TRUE);

	/* The ECalendar. */
	w = e_calendar_new ();
	priv->date_navigator = E_CALENDAR (w);
	gtk_widget_show (w);
	e_paned_pack1 (E_PANED (priv->vpane), w, FALSE, TRUE);
	gtk_signal_connect (GTK_OBJECT (priv->date_navigator),
			    "style_set",
			    GTK_SIGNAL_FUNC (gnome_calendar_on_date_navigator_style_set),
			    gcal);
	gtk_signal_connect_after (GTK_OBJECT (priv->date_navigator),
				  "size_allocate",
				  (GtkSignalFunc) gnome_calendar_on_date_navigator_size_allocate,
				  gcal);
	gtk_signal_connect (GTK_OBJECT (priv->date_navigator->calitem),
			    "selection_changed",
			    (GtkSignalFunc) gnome_calendar_on_date_navigator_selection_changed,
			    gcal);
	gtk_signal_connect (GTK_OBJECT (priv->date_navigator->calitem),
			    "date_range_changed",
			    GTK_SIGNAL_FUNC (gnome_calendar_on_date_navigator_date_range_changed),
			    gcal);

	/* The ToDo list. */
	priv->todo = e_calendar_table_new ();
	e_paned_pack2 (E_PANED (priv->vpane), priv->todo, TRUE, TRUE);
	gtk_widget_show (priv->todo);

	/* The Day View. */
	priv->day_view = e_day_view_new ();
	e_day_view_set_calendar (E_DAY_VIEW (priv->day_view), gcal);
	gtk_widget_show (priv->day_view);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
				  priv->day_view, gtk_label_new (""));

	/* The Work Week View. */
	priv->work_week_view = e_day_view_new ();
	e_day_view_set_work_week_view (E_DAY_VIEW (priv->work_week_view),
				       TRUE);
	e_day_view_set_days_shown (E_DAY_VIEW (priv->work_week_view), 5);
	e_day_view_set_calendar (E_DAY_VIEW (priv->work_week_view), gcal);
	gtk_widget_show (priv->work_week_view);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
				  priv->work_week_view, gtk_label_new (""));

	/* The Week View. */
	priv->week_view = e_week_view_new ();
	e_week_view_set_calendar (E_WEEK_VIEW (priv->week_view), gcal);
	gtk_widget_show (priv->week_view);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
				  priv->week_view, gtk_label_new (""));

	/* The Month View. */
	priv->month_view = e_week_view_new ();
	e_week_view_set_calendar (E_WEEK_VIEW (priv->month_view), gcal);
	e_week_view_set_multi_week_view (E_WEEK_VIEW (priv->month_view), TRUE);
	gtk_widget_show (priv->month_view);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
				  priv->month_view, gtk_label_new (""));
}

/* Object initialization function for the gnome calendar */
static void
gnome_calendar_init (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;

	priv = g_new0 (GnomeCalendarPrivate, 1);
	gcal->priv = priv;

	priv->load_state = LOAD_STATE_NOT_LOADED;

	priv->object_editor_hash = g_hash_table_new (g_str_hash, g_str_equal);
	priv->alarms = g_hash_table_new (g_str_hash, g_str_equal);

	priv->current_view_type = VIEW_NOT_SET;
	priv->range_selected = FALSE;

	priv->selection_start_time = time_day_begin (time (NULL));
	priv->selection_end_time = time_add_day (priv->selection_start_time, 1);

	/* Set the default pane positions. These will eventually come from
	   gconf settings. They are multiples of calendar month widths &
	   heights in the date navigator. */
	priv->hpane_pos = 1.0;
	priv->vpane_pos = 1.0;
	priv->hpane_pos_month_view = 0.0;
	priv->vpane_pos_month_view = 1.0;

	setup_widgets (gcal);
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
	GnomeCalendarPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (object));

	gcal = GNOME_CALENDAR (object);
	priv = gcal->priv;

	priv->load_state = LOAD_STATE_NOT_LOADED;

	if (priv->client) {
		gtk_object_unref (GTK_OBJECT (priv->client));
		priv->client = NULL;
	}

	g_hash_table_foreach (priv->alarms, free_object_alarms, NULL);
	g_hash_table_destroy (priv->alarms);
	priv->alarms = NULL;

	g_hash_table_foreach (priv->object_editor_hash, free_uid, NULL);
	g_hash_table_destroy (priv->object_editor_hash);
	priv->object_editor_hash = NULL;

	g_free (priv);
	gcal->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static GtkWidget *
get_current_page (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;

	priv = gcal->priv;

	return GTK_NOTEBOOK (priv->notebook)->cur_page->child;
}

char *
gnome_calendar_get_current_view_name (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	GtkWidget *page;

	g_return_val_if_fail (gcal != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), NULL);

	priv = gcal->priv;

	page = get_current_page (gcal);

	if (page == priv->day_view)
		return "dayview";
	else if (page == priv->work_week_view)
		return "workweekview";
	else if (page == priv->week_view)
		return "weekview";
	else if (page == priv->month_view)
		return "monthview";
	else {
		g_assert_not_reached ();
		return NULL;
	}
}

void
gnome_calendar_goto (GnomeCalendar *gcal, time_t new_time)
{
	GnomeCalendarPrivate *priv;

	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (new_time != -1);

	priv = gcal->priv;

	priv->selection_start_time = time_day_begin (new_time);
	priv->selection_end_time = time_add_day (priv->selection_start_time, 1);

	gnome_calendar_update_view_times (gcal);
	gnome_calendar_update_date_navigator (gcal);
}


static void
gnome_calendar_update_view_times (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	GtkWidget *page;

	priv = gcal->priv;

	page = get_current_page (gcal);

	if (page == priv->day_view || page == priv->work_week_view) {
		e_day_view_set_selected_time_range (E_DAY_VIEW (page),
						    priv->selection_start_time,
						    priv->selection_end_time);
	} else if (page == priv->week_view || page == priv->month_view) {
		e_week_view_set_selected_time_range (E_WEEK_VIEW (page),
						     priv->selection_start_time,
						     priv->selection_end_time);
	} else {
		g_warning ("My penguin is gone!");
		g_assert_not_reached ();
	}
}

static void
gnome_calendar_direction (GnomeCalendar *gcal, int direction)
{
	GnomeCalendarPrivate *priv;
	GtkWidget *cp;
	time_t start_time, end_time;

	priv = gcal->priv;

	cp = get_current_page (gcal);

	start_time = priv->selection_start_time;
	end_time = priv->selection_end_time;

	if (cp == priv->day_view) {
		start_time = time_add_day (start_time, direction);
		end_time = time_add_day (end_time, direction);
	} else if (cp == priv->work_week_view) {
		start_time = time_add_week (start_time, direction);
		end_time = time_add_week (end_time, direction);
	} else if (cp == priv->week_view) {
		start_time = time_add_week (start_time, direction);
		end_time = time_add_week (end_time, direction);
	} else if (cp == priv->month_view) {
		start_time = time_add_month (start_time, direction);
		end_time = time_add_month (end_time, direction);
	} else {
		g_warning ("Weee!  Where did the penguin go?");
		g_assert_not_reached ();
		return;
	}

	priv->selection_start_time = start_time;
	priv->selection_end_time = end_time;

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
	GnomeCalendarPrivate *priv;

	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	priv = gcal->priv;

	priv->selection_start_time = time_day_begin (time);
	priv->selection_end_time = time_add_day (priv->selection_start_time, 1);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->day_button), TRUE);
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
				 gboolean	 range_selected,
				 gboolean	 focus)
{
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (page_name != NULL);

	gnome_calendar_set_view_internal (gcal, page_name, range_selected, focus);
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
					 gboolean	 range_selected,
					 gboolean	 focus)
{
	GnomeCalendarPrivate *priv;
	int view;
	gboolean round_selection;
	GtkWidget *focus_widget;

	priv = gcal->priv;

	round_selection = FALSE;

	if (!strcmp (page_name, "dayview")) {
		view = VIEW_DAY;
		focus_widget = priv->day_view;

		if (!range_selected)
			e_day_view_set_days_shown (E_DAY_VIEW (priv->day_view), 1);
	} else if (!strcmp (page_name, "workweekview")) {
		view = VIEW_WORK_WEEK;
		focus_widget = priv->work_week_view;
	} else if (!strcmp (page_name, "weekview")) {
		view = VIEW_WEEK;
		focus_widget = priv->week_view;
		round_selection = TRUE;
	} else if (!strcmp (page_name, "monthview")) {
		view = VIEW_MONTH;
		focus_widget = priv->month_view;

		if (!range_selected)
			e_week_view_set_weeks_shown (E_WEEK_VIEW (priv->month_view), 5);
		round_selection = TRUE;
	} else {
		g_warning ("Unknown calendar view: %s", page_name);
		g_assert_not_reached ();
		return;
	}

	priv->current_view_type = view;
	priv->range_selected = range_selected;

	gtk_notebook_set_page (GTK_NOTEBOOK (priv->notebook), view);

	if (focus)
		gtk_widget_grab_focus (focus_widget);

	gnome_calendar_set_pane_positions (gcal);

	/* For the week & month views we want the selection in the date
	   navigator to be rounded to the nearest week when the arrow buttons
	   are pressed to move to the previous/next month. */
	gtk_object_set (GTK_OBJECT (priv->date_navigator->calitem),
			"round_selection_when_moving", round_selection,
			NULL);
}


static void
gnome_calendar_set_pane_positions	(GnomeCalendar	*gcal)
{
	GnomeCalendarPrivate *priv;
	gint top_border, bottom_border, left_border, right_border;
	gint col_width, row_height;
	gfloat right_pane_width, top_pane_height;

	priv = gcal->priv;

	/* Get the size of the calendar month width & height. */
	e_calendar_get_border_size (priv->date_navigator,
				    &top_border, &bottom_border,
				    &left_border, &right_border);
	gtk_object_get (GTK_OBJECT (priv->date_navigator->calitem),
			"row_height", &row_height,
			"column_width", &col_width,
			NULL);

	if (priv->current_view_type == VIEW_MONTH && !priv->range_selected) {
		right_pane_width = priv->hpane_pos_month_view;
		top_pane_height = priv->vpane_pos_month_view;
	} else {
		right_pane_width = priv->hpane_pos;
		top_pane_height = priv->vpane_pos;
	}

	/* We add the borders before multiplying due to the way we are using
	   the EPaned quantum feature. */
	if (right_pane_width < 0.001)
		right_pane_width = 0.0;
	else
		right_pane_width = (right_pane_width * (col_width + left_border + right_border)
				    + 0.5);
	if (top_pane_height < 0.001)
		top_pane_height = 0.0;
	else
		top_pane_height = (top_pane_height * (row_height + top_border + bottom_border)
				   + 0.5);

	e_paned_set_position (E_PANED (priv->hpane), -1);
	e_paned_set_position (E_PANED (priv->vpane), -1);

	/* We add one to each dimension since we can't use 0. */

	gtk_widget_set_usize (priv->vpane, right_pane_width + 1, -2);
	gtk_widget_set_usize (GTK_WIDGET (priv->date_navigator), -2, top_pane_height + 1);
}

#if 0

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
		gnome_calendar_edit_object (c->gcal, c->comp);
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
	GnomeCalendarPrivate *priv;
	CalComponent *comp;
	CalClientGetStatus status;
	const char *uid;
	ObjectAlarms *oa;
   	GList *l;

	c = data;
	priv = c->gcal->priv;

	/* Fetch the object */

	status = cal_client_get_object (priv->client, c->uid, &comp);

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
	oa = g_hash_table_lookup (priv->alarms, uid);
	g_assert (oa != NULL);

	l = g_list_find (oa->alarm_ids, alarm_id);
	g_assert (l != NULL);

	oa->alarm_ids = g_list_remove_link (oa->alarm_ids, l);
	g_list_free_1 (l);

	if (!oa->alarm_ids) {
		g_hash_table_remove (priv->alarms, uid);
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
setup_alarm (GnomeCalendar *gcal, CalAlarmInstance *ai)
{
	GnomeCalendarPrivate *priv;
	struct trigger_alarm_closure *c;
	gpointer alarm;
	ObjectAlarms *oa;

	priv = gcal->priv;

	c = g_new (struct trigger_alarm_closure, 1);
	c->gcal = gcal;
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

	oa = g_hash_table_lookup (priv->alarms, ai->uid);
	if (oa)
		oa->alarm_ids = g_list_prepend (oa->alarm_ids, alarm);
	else {
		oa = g_new (ObjectAlarms, 1);
		oa->uid = g_strdup (ai->uid);
		oa->alarm_ids = g_list_prepend (NULL, alarm);

		g_hash_table_insert (priv->alarms, oa->uid, oa);
	}
}

static void load_alarms (GnomeCalendar *cal);

/* Called nightly to refresh the day's alarms */
static void
midnight_refresh_cb (gpointer alarm_id, time_t trigger, gpointer data)
{
	GnomeCalendar *cal;
	GnomeCalendarPrivate *priv;

	cal = GNOME_CALENDAR (data);
	priv = cal->priv;

	priv->midnight_alarm_refresh_id = NULL;

	load_alarms (cal);
}

/* Loads and queues the alarms from the current time up to midnight. */
static void
load_alarms (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	time_t now;
	time_t end_of_day;
	GList *alarms, *l;

	priv = gcal->priv;

	now = time (NULL);
	end_of_day = time_day_end (now);

	/* Queue alarms */

	alarms = cal_client_get_alarms_in_range (priv->client, now, end_of_day);

	for (l = alarms; l; l = l->next)
		setup_alarm (gcal, l->data);

	cal_alarm_instance_list_free (alarms);

	/* Queue the midnight alarm refresh */

	priv->midnight_alarm_refresh_id = alarm_add (end_of_day, midnight_refresh_cb, gcal, NULL);
	if (!priv->midnight_alarm_refresh_id) {
		g_message ("load_alarms(): Could not set up the midnight refresh alarm!");
		/* FIXME: what to do? */
	}
}

/* Loads the initial data into the calendar; this should be called right after
 * the cal_loaded signal from the client is invoked.
 */
static void
initial_load (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;

	priv = gcal->priv;

	load_alarms (gcal);
	gnome_calendar_tag_calendar (gcal, priv->date_navigator);
}

/* Removes any queued alarms for the specified UID */
static void
remove_alarms_for_object (GnomeCalendar *gcal, const char *uid)
{
	GnomeCalendarPrivate *priv;
	ObjectAlarms *oa;
	GList *l;

	priv = gcal->priv;

	oa = g_hash_table_lookup (priv->alarms, uid);
	if (!oa)
		return;

	for (l = oa->alarm_ids; l; l = l->next) {
		gpointer alarm_id;

		alarm_id = l->data;
		alarm_remove (alarm_id);
	}

	g_hash_table_remove (priv->alarms, uid);

	g_free (oa->uid);
	g_list_free (oa->alarm_ids);
	g_free (oa);
}

/* Adds today's alarms for the specified object */
static void
add_alarms_for_object (GnomeCalendar *gcal, const char *uid)
{
	GnomeCalendarPrivate *priv;
	GList *alarms;
	gboolean result;
	time_t now, end_of_day;
	GList *l;

	priv = gcal->priv;

	now = time (NULL);
	end_of_day = time_day_end (now);

	result = cal_client_get_alarms_for_object (priv->client, uid, now, end_of_day, &alarms);
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

struct load_create_closure {
	GnomeCalendar *gcal;
	char *uri;
};

static void cal_loaded_cb (CalClient *client, CalClientLoadStatus status, gpointer data);

/* Connects to the cal_loaded signal of the client while creating the proper
 * closure for the callback.
 */
static struct load_create_closure *
connect_load (GnomeCalendar *gcal, const char *uri)
{
	GnomeCalendarPrivate *priv;
	struct load_create_closure *c;

	priv = gcal->priv;

	c = g_new (struct load_create_closure, 1);
	c->gcal = gcal;
	c->uri = g_strdup (uri);

	gtk_signal_connect (GTK_OBJECT (priv->client), "cal_loaded",
			    GTK_SIGNAL_FUNC (cal_loaded_cb), c);

	return c;
}

/* Disconnects from the cal_loaded signal of the client; also frees the callback
 * closure data.
 */
static void
disconnect_load (GnomeCalendar *gcal, struct load_create_closure *c)
{
	GnomeCalendarPrivate *priv;

	priv = gcal->priv;

	gtk_signal_disconnect_by_func (GTK_OBJECT (priv->client),
				       GTK_SIGNAL_FUNC (cal_loaded_cb),
				       c);

	g_free (c->uri);
	g_free (c);
}

/* Displays an error to indicate that loading a calendar failed */
static void
load_error (GnomeCalendar *gcal, const char *uri)
{
	char *msg;

	msg = g_strdup_printf (_("Could not load the calendar in `%s'"), uri);
	gnome_error_dialog_parented (msg, GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (gcal))));
	g_free (msg);
}

/* Displays an error to indicate that creating a calendar failed */
static void
create_error (GnomeCalendar *gcal, const char *uri)
{
	char *msg;

	msg = g_strdup_printf (_("Could not create a calendar in `%s'"), uri);
	gnome_error_dialog_parented (msg, GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (gcal))));
	g_free (msg);
}

/* Displays an error to indicate that the specified URI method is not supported */
static void
method_error (GnomeCalendar *gcal, const char *uri)
{
	char *msg;

	msg = g_strdup_printf (_("The method required to load `%s' is not supported"), uri);
	gnome_error_dialog_parented (msg, GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (gcal))));
	g_free (msg);
}

/* Callback from the calendar client when a calendar is loaded */
static void
cal_loaded_cb (CalClient *client, CalClientLoadStatus status, gpointer data)
{
	struct load_create_closure *c;
	GnomeCalendar *gcal;
	GnomeCalendarPrivate *priv;
	char *uri;

	c = data;

	gcal = c->gcal;
	uri = g_strdup (c->uri);
	priv = gcal->priv;

	disconnect_load (gcal, c);
	c = NULL;

	switch (priv->load_state) {
	case LOAD_STATE_WAIT_LOAD:
		if (status == CAL_CLIENT_LOAD_SUCCESS) {
			priv->load_state = LOAD_STATE_LOADED;
			initial_load (gcal);
		} else if (status == CAL_CLIENT_LOAD_ERROR) {
			priv->load_state = LOAD_STATE_NOT_LOADED;
			load_error (gcal, uri);
		} else if (status == CAL_CLIENT_LOAD_METHOD_NOT_SUPPORTED) {
			priv->load_state = LOAD_STATE_NOT_LOADED;
			method_error (gcal, uri);
		} else
			g_assert_not_reached ();

		break;

	case LOAD_STATE_WAIT_LOAD_BEFORE_CREATE:
		if (status == CAL_CLIENT_LOAD_SUCCESS) {
			priv->load_state = LOAD_STATE_LOADED;
			initial_load (gcal);
		} else if (status == CAL_CLIENT_LOAD_ERROR) {
			priv->load_state = LOAD_STATE_WAIT_CREATE;

			c = connect_load (gcal, uri);
			if (!cal_client_create_calendar (priv->client, uri)) {
				priv->load_state = LOAD_STATE_NOT_LOADED;
				disconnect_load (gcal, c);
				c = NULL;

				g_message ("cal_loaded_cb(): Could not issue the create request");
			}
		} else if (status == CAL_CLIENT_LOAD_METHOD_NOT_SUPPORTED) {
			priv->load_state = LOAD_STATE_NOT_LOADED;
			method_error (gcal, uri);
		} else
			g_assert_not_reached ();

		break;

	case LOAD_STATE_WAIT_CREATE:
		if (status == CAL_CLIENT_LOAD_SUCCESS) {
			priv->load_state = LOAD_STATE_LOADED;
			initial_load (gcal);
		} else if (status == CAL_CLIENT_LOAD_ERROR) {
			priv->load_state = LOAD_STATE_NOT_LOADED;
			create_error (gcal, uri);
		} else if (status == CAL_CLIENT_LOAD_IN_USE) {
			/* Someone created the URI while we were issuing the
			 * create request, so we just try to reload.
			 */
			priv->load_state = LOAD_STATE_WAIT_LOAD;

			c = connect_load (gcal, uri);
			if (!cal_client_load_calendar (priv->client, uri)) {
				priv->load_state = LOAD_STATE_NOT_LOADED;
				disconnect_load (gcal, c);
				c = NULL;

				g_message ("cal_loaded_cb(): Could not issue the load request");
			}
		} else if (status == CAL_CLIENT_LOAD_METHOD_NOT_SUPPORTED) {
			priv->load_state = LOAD_STATE_NOT_LOADED;
			method_error (gcal, uri);
		} else
			g_assert_not_reached ();

		break;

	default:
		g_assert_not_reached ();
	}

	g_free (uri);
}

/* Callback from the calendar client when an object is updated */
static void
obj_updated_cb (CalClient *client, const char *uid, gpointer data)
{
	GnomeCalendar *gcal;
	GnomeCalendarPrivate *priv;

	gcal = GNOME_CALENDAR (data);
	priv = gcal->priv;

	remove_alarms_for_object (gcal, uid);
	add_alarms_for_object (gcal, uid);

	gnome_calendar_tag_calendar (gcal, priv->date_navigator);
}

/* Callback from the calendar client when an object is removed */
static void
obj_removed_cb (CalClient *client, const char *uid, gpointer data)
{
	GnomeCalendar *gcal;
	GnomeCalendarPrivate *priv;

	gcal = GNOME_CALENDAR (data);
	priv = gcal->priv;

	remove_alarms_for_object (gcal, uid);

	gnome_calendar_tag_calendar (gcal, priv->date_navigator);
}


GtkWidget *
gnome_calendar_construct (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;

	g_return_val_if_fail (gcal != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), NULL);

	priv = gcal->priv;

	priv->client = cal_client_new ();
	if (!priv->client)
		return NULL;

	gtk_signal_connect (GTK_OBJECT (priv->client), "obj_updated",
			    GTK_SIGNAL_FUNC (obj_updated_cb), gcal);
	gtk_signal_connect (GTK_OBJECT (priv->client), "obj_removed",
			    GTK_SIGNAL_FUNC (obj_removed_cb), gcal);

	e_calendar_table_set_cal_client (E_CALENDAR_TABLE (priv->todo), priv->client);

	e_day_view_set_cal_client (E_DAY_VIEW (priv->day_view), priv->client);
	e_day_view_set_cal_client (E_DAY_VIEW (priv->work_week_view), priv->client);
	e_week_view_set_cal_client (E_WEEK_VIEW (priv->week_view), priv->client);
	e_week_view_set_cal_client (E_WEEK_VIEW (priv->month_view), priv->client);

	gnome_calendar_set_view (gcal, "dayview", FALSE, FALSE);

	return GTK_WIDGET (gcal);
}

GtkWidget *
gnome_calendar_new (void)
{
	GnomeCalendar *gcal;

	gcal = gtk_type_new (gnome_calendar_get_type ());

	if (!gnome_calendar_construct (gcal)) {
		g_message ("gnome_calendar_new(): Could not construct the calendar GUI");
		gtk_object_unref (GTK_OBJECT (gcal));
		return NULL;
	}

	return GTK_WIDGET (gcal);
}

/**
 * gnome_calendar_get_cal_client:
 * @gcal: A calendar view.
 * 
 * Queries the calendar client interface object that a calendar view is using.
 * 
 * Return value: A calendar client interface object.
 **/
CalClient *
gnome_calendar_get_cal_client (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;

	g_return_val_if_fail (gcal != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), NULL);

	priv = gcal->priv;

	return priv->client;
}

gboolean
gnome_calendar_open (GnomeCalendar *gcal, char *file, GnomeCalendarOpenMode gcom)
{
	GnomeCalendarPrivate *priv;
	struct load_create_closure *c;

	g_return_val_if_fail (gcal != NULL, FALSE);
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), FALSE);
	g_return_val_if_fail (file != NULL, FALSE);

	priv = gcal->priv;
	g_return_val_if_fail (priv->load_state == LOAD_STATE_NOT_LOADED, FALSE);

	c = connect_load (gcal, file);

	if (gcom == CALENDAR_OPEN)
		priv->load_state = LOAD_STATE_WAIT_LOAD;
	else if (gcom == CALENDAR_OPEN_OR_CREATE)
		priv->load_state = LOAD_STATE_WAIT_LOAD_BEFORE_CREATE;
	else {
		g_assert_not_reached ();
		return FALSE;
	}

	if (!cal_client_load_calendar (priv->client, file)) {
		priv->load_state = LOAD_STATE_NOT_LOADED;
		disconnect_load (gcal, c);
		g_message ("gnome_calendar_open(): Could not issue the request");
		return FALSE;
	}

	return TRUE;
}

#if 0

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
	GnomeCalendarPrivate *priv;
	struct calendar_tag_closure c;
	gint start_year, start_month, start_day;
	gint end_year, end_month, end_day;
	struct tm start_tm = { 0 }, end_tm = { 0 };

	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (ecal != NULL);
	g_return_if_fail (E_IS_CALENDAR (ecal));

	priv = gcal->priv;

	/* If the ECalendar isn't visible, we just return. */
	if (!GTK_WIDGET_VISIBLE (ecal))
		return;

	e_calendar_item_clear_marks (ecal->calitem);

	if (!cal_client_is_loaded (priv->client))
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

	c.calitem = ecal->calitem;
	c.start_time = mktime (&start_tm);
	c.end_time = mktime (&end_tm);

	cal_client_generate_instances (priv->client, CALOBJ_TYPE_EVENT,
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
	GnomeCalendarPrivate *priv;

	priv = gcal->priv;

	priv->selection_start_time = start_time;
	priv->selection_end_time = end_time;

	gnome_calendar_update_date_navigator (gcal);
}

/**
 * gnome_calendar_get_selected_time_range:
 * @gcal: A calendar view.
 * @start_time: Return value for the start of the time selection.
 * @end_time: Return value for the end of the time selection.
 * 
 * Queries the time selection range on the calendar view.
 **/
void
gnome_calendar_get_selected_time_range (GnomeCalendar *gcal,
					time_t	 *start_time,
					time_t	 *end_time)
{
	GnomeCalendarPrivate *priv;

	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	priv = gcal->priv;

	if (start_time)
		*start_time = priv->selection_start_time;

	if (end_time)
		*end_time = priv->selection_end_time;
}


#ifndef NO_WARNINGS
/* Callback used when an event editor finishes editing an object */
static void
released_event_object_cb (EventEditor *ee, const char *uid, gpointer data)
{
	GnomeCalendar *gcal;
	GnomeCalendarPrivate *priv;
	gboolean result;
	gpointer orig_key;
	char *orig_uid;

	gcal = GNOME_CALENDAR (data);
	priv = gcal->priv;

	result = g_hash_table_lookup_extended (priv->object_editor_hash, uid, &orig_key, NULL);
	g_assert (result != FALSE);

	orig_uid = orig_key;

	g_hash_table_remove (priv->object_editor_hash, orig_uid);
	g_free (orig_uid);
}
#endif

/* Callback used when an event editor dialog is closed */
struct editor_closure
{
	GnomeCalendar *gcal;
	char *uid;
};

static void
editor_closed_cb (GtkWidget *widget, gpointer data)
{
	GnomeCalendar *gcal;
	GnomeCalendarPrivate *priv;
	struct editor_closure *ec;
	gboolean result;
	gpointer orig_key;
	char *orig_uid;

	g_print ("editor_closed_cb ()\n");

	ec = (struct editor_closure *)data;
	gcal = ec->gcal;
	priv = gcal->priv;

	result = g_hash_table_lookup_extended (priv->object_editor_hash, ec->uid, &orig_key, NULL);
	g_assert (result != FALSE);

	orig_uid = orig_key;

	g_hash_table_remove (priv->object_editor_hash, orig_uid);
	g_free (orig_uid);
}

void
gnome_calendar_edit_object (GnomeCalendar *gcal, CalComponent *comp)
{
	GnomeCalendarPrivate *priv;
	EventEditor *ee;
	struct editor_closure *ec;
	const char *uid;

	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (comp != NULL);

	priv = gcal->priv;

	cal_component_get_uid (comp, &uid);

	ee = g_hash_table_lookup (priv->object_editor_hash, uid);
	if (!ee) {
		ec = g_new0 (struct editor_closure, 1);

		ee = event_editor_new ();
		if (!ee) {
			g_message ("gnome_calendar_edit_object(): Could not create the event editor");
			return;
		}

		ec->gcal = gcal;
		ec->uid = g_strdup (uid);

		g_hash_table_insert (priv->object_editor_hash, ec->uid, ee);

		gtk_signal_connect (GTK_OBJECT (ee), "destroy",
				    GTK_SIGNAL_FUNC (editor_closed_cb),
				    ec);

		event_editor_set_cal_client (EVENT_EDITOR (ee), priv->client);
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

	itt = icaltime_from_timet (dtstart, FALSE, FALSE);
	cal_component_set_dtstart (comp, &dt);

	itt = icaltime_from_timet (dtend, FALSE, FALSE);
	cal_component_set_dtend (comp, &dt);

	cal_component_commit_sequence (comp);

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
	GnomeCalendarPrivate *priv;
	GtkWidget *page;

	priv = gcal->priv;

	page = get_current_page (gcal);

	if (page == priv->day_view || page == priv->work_week_view)
		e_day_view_get_selected_time_range (E_DAY_VIEW (page), start_time, end_time);
	else if (page == priv->week_view || page == priv->month_view)
		e_week_view_get_selected_time_range (E_WEEK_VIEW (page), start_time, end_time);
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
	GnomeCalendarPrivate *priv;
	GDate start_date, end_date;
	gint days_shown;

	priv = gcal->priv;

	/* If the ECalendar isn't visible, we just return. */
	if (!GTK_WIDGET_VISIBLE (priv->date_navigator))
		return;

	if (gnome_calendar_get_days_shown (gcal, &start_date, &days_shown)) {
		end_date = start_date;
		g_date_add_days (&end_date, days_shown - 1);

		e_calendar_item_set_selection (priv->date_navigator->calitem,
					       &start_date, &end_date);
	}
}


static gboolean
gnome_calendar_get_days_shown	(GnomeCalendar	*gcal,
				 GDate		*start_date,
				 gint		*days_shown)
{
	GnomeCalendarPrivate *priv;
	GtkWidget *page;

	priv = gcal->priv;

	page = get_current_page (gcal);

	if (page == priv->day_view || page == priv->work_week_view) {
		g_date_clear (start_date, 1);
		g_date_set_time (start_date, E_DAY_VIEW (page)->lower);
		*days_shown = e_day_view_get_days_shown (E_DAY_VIEW (page));
		return TRUE;
	} else if (page == priv->week_view || page == priv->month_view) {
		*start_date = E_WEEK_VIEW (page)->first_day_shown;
		if (e_week_view_get_multi_week_view (E_WEEK_VIEW (page)))
			*days_shown = e_week_view_get_weeks_shown (E_WEEK_VIEW (page)) * 7;
		else
			*days_shown = 7;

		return TRUE;
	} else {
		g_assert_not_reached ();
		return FALSE;
	}
}


static void
gnome_calendar_on_date_navigator_selection_changed (ECalendarItem    *calitem,
						    GnomeCalendar    *gcal)
{
	GnomeCalendarPrivate *priv;
	GDate start_date, end_date, new_start_date, new_end_date;
	gint days_shown, new_days_shown;
	gint start_year, start_month, start_day;
	gint end_year, end_month, end_day;
	gboolean starts_on_week_start_day;
	struct tm tm;

	priv = gcal->priv;

	starts_on_week_start_day = FALSE;

	if (!gnome_calendar_get_days_shown (gcal, &start_date, &days_shown))
		return;

	end_date = start_date;
	g_date_add_days (&end_date, days_shown - 1);

	e_calendar_item_get_selection (calitem, &new_start_date, &new_end_date);

	/* If the selection hasn't changed just return. */
	if (!g_date_compare (&start_date, &new_start_date)
	    && !g_date_compare (&end_date, &new_end_date))
		return;

	new_days_shown = g_date_julian (&new_end_date) - g_date_julian (&new_start_date) + 1;

	/* FIXME: This assumes weeks start on Monday for now. */
	if (g_date_weekday (&new_start_date) - 1 == 0)
		starts_on_week_start_day = TRUE;

	/* Switch views as appropriate, and change the number of days or weeks
	   shown. */
	if (new_days_shown > 9) {
		e_week_view_set_weeks_shown (E_WEEK_VIEW (priv->month_view),
					     (new_days_shown + 6) / 7);
		e_week_view_set_first_day_shown (E_WEEK_VIEW (priv->month_view), &new_start_date);

		gnome_calendar_set_view_internal (gcal, "monthview", TRUE, FALSE);
		gnome_calendar_update_date_navigator (gcal);
	} else if (new_days_shown == 7 && starts_on_week_start_day) {
		e_week_view_set_first_day_shown (E_WEEK_VIEW (priv->week_view), &new_start_date);

		gnome_calendar_set_view_internal (gcal, "weekview", TRUE, FALSE);
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
		priv->selection_start_time = mktime (&tm);

		tm.tm_year = end_year - 1900;
		tm.tm_mon  = end_month;
		tm.tm_mday = end_day + 1; /* mktime() will normalize this. */
		tm.tm_hour = 0;
		tm.tm_min  = 0;
		tm.tm_sec  = 0;
		tm.tm_isdst = -1;
		priv->selection_end_time = mktime (&tm);

		e_day_view_set_days_shown (E_DAY_VIEW (priv->day_view), new_days_shown);
		gnome_calendar_set_view (gcal, "dayview", TRUE, FALSE);
	}

	gnome_calendar_update_view_buttons (gcal);
	gtk_widget_grab_focus (get_current_page (gcal));
}


static void
gnome_calendar_on_date_navigator_date_range_changed (ECalendarItem *calitem,
						     GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;

	priv = gcal->priv;

	gnome_calendar_tag_calendar (gcal, priv->date_navigator);
}


static void
gnome_calendar_on_date_navigator_style_set (GtkWidget     *widget,
					    GtkStyle      *previous_style,
					    gpointer       data)
{
	GnomeCalendar *gcal;
	GnomeCalendarPrivate *priv;
	ECalendar *ecal;
	gint row_height, col_width;
	gint top_border, bottom_border, left_border, right_border;

	ecal = E_CALENDAR (widget);

	gcal = GNOME_CALENDAR (data);
	priv = gcal->priv;

	e_calendar_get_border_size (priv->date_navigator,
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
	gtk_object_set (GTK_OBJECT (priv->hpane),
			"quantum", (guint) col_width,
			NULL);
	gtk_object_set (GTK_OBJECT (priv->vpane),
			"quantum", (guint) row_height,
			NULL);
#endif

	gnome_calendar_set_pane_positions (gcal);
}


static void
gnome_calendar_on_date_navigator_size_allocate (GtkWidget     *widget,
						GtkAllocation *allocation,
						gpointer data)
{
	GnomeCalendar *gcal;
	GnomeCalendarPrivate *priv;
	gint width, height, row_height, col_width;
	gint top_border, bottom_border, left_border, right_border;
	gfloat hpane_pos, vpane_pos;

	gcal = GNOME_CALENDAR (data);
	priv = gcal->priv;

	if (priv->current_view_type != VIEW_NOT_SET) {
		e_calendar_get_border_size (priv->date_navigator,
					    &top_border, &bottom_border,
					    &left_border, &right_border);
		gtk_object_get (GTK_OBJECT (priv->date_navigator->calitem),
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

		if (priv->current_view_type == VIEW_MONTH && !priv->range_selected) {
			priv->hpane_pos_month_view = hpane_pos;
			priv->vpane_pos_month_view = vpane_pos;
		} else {
			priv->hpane_pos = hpane_pos;
			priv->vpane_pos = vpane_pos;
		}
	}
}

void
gnome_calendar_set_view_buttons	(GnomeCalendar	*gcal,
				 GtkWidget	*day_button,
				 GtkWidget	*work_week_button,
				 GtkWidget	*week_button,
				 GtkWidget	*month_button)
{
	GnomeCalendarPrivate *priv;

	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (day_button != NULL);
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (day_button));
	g_return_if_fail (work_week_button != NULL);
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (work_week_button));
	g_return_if_fail (week_button != NULL);
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (week_button));
	g_return_if_fail (month_button != NULL);
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (month_button));

	priv = gcal->priv;

	priv->day_button = day_button;
	priv->work_week_button = work_week_button;
	priv->week_button = week_button;
	priv->month_button = month_button;
}

/* This makes the appropriate radio button in the toolbar active.  It blocks the
 * signals so that we can do a clean setup without affecting the views.
 */
void
gnome_calendar_update_view_buttons (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	GtkWidget *page, *button;

	priv = gcal->priv;

	page = get_current_page (gcal);

	if (page == priv->day_view)
		button = priv->day_button;
	else if (page == priv->work_week_view)
		button = priv->work_week_button;
	else if (page == priv->week_view)
		button = priv->week_button;
	else if (page == priv->month_view)
		button = priv->month_button;
	else {
		g_assert_not_reached ();
		return;
	}

	gtk_signal_handler_block_by_data (GTK_OBJECT (button), gcal);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (button), gcal);
}
