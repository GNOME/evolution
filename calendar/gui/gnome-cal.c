/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar - Main calendar view widget
 *
 * Copyright (C) 1998 The Free Software Foundation
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Miguel de Icaza <miguel@ximian.com>
 *          Federico Mena-Quintero <federico@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkbindings.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <bonobo/bonobo-exception.h>
#include "e-util/e-url.h"
#include <cal-util/timeutil.h>
#include "widgets/menus/gal-view-menus.h"
#include "e-comp-editor-registry.h"
#include "dialogs/delete-error.h"
#include "dialogs/event-editor.h"
#include "dialogs/task-editor.h"
#include "comp-util.h"
#include "e-day-view.h"
#include "e-day-view-time-item.h"
#include "e-week-view.h"
#include "evolution-calendar.h"
#include "gnome-cal.h"
#include "calendar-component.h"
#include "cal-search-bar.h"
#include "calendar-commands.h"
#include "calendar-config.h"
#include "calendar-view.h"
#include "calendar-view-factory.h"
#include "tag-calendar.h"
#include "misc.h"

extern ECompEditorRegistry *comp_editor_registry;



/* Private part of the GnomeCalendar structure */
struct _GnomeCalendarPrivate {
	/*
	 * The Calendar Folder.
	 */

	/* The calendar client object we monitor */
	CalClient *client;

	/* Set of categories from the calendar client */
	GPtrArray *cal_categories;

	/*
	 * The TaskPad Folder.
	 */

	/* The calendar client object we monitor */
	CalClient   *task_pad_client;

	/* Set of categories from the tasks client */
	GPtrArray *tasks_categories;

	/*
	 * Fields for the calendar view
	 */

	/* This is the last selection explicitly selected by the user. We try
	   to keep it the same when we switch views, but we may have to alter
	   it depending on the view (e.g. the week views only select days, so
	   any times are lost. */
	time_t      selection_start_time;
	time_t      selection_end_time;

	/* Widgets */

	GtkWidget   *search_bar;

	GtkWidget   *hpane;
	GtkWidget   *notebook;
	GtkWidget   *vpane;
	ECalendar   *date_navigator;
	GtkWidget   *todo;

	GtkWidget   *day_view;
	GtkWidget   *work_week_view;
	GtkWidget   *week_view;
	GtkWidget   *month_view;

	/* Calendar query for the date navigator */
	CalQuery    *dn_query;
	char        *sexp;
	guint        query_timeout;
	
	/* This is the view currently shown. We use it to keep track of the
	   positions of the panes. range_selected is TRUE if a range of dates
	   was selected in the date navigator to show the view. */
	GnomeCalendarViewType current_view_type;
	gboolean range_selected;

	/* These are the saved positions of the panes. They are multiples of
	   calendar month widths & heights in the date navigator, so that they
	   will work OK after theme changes. */
	gint	     hpane_pos;
	gint	     vpane_pos;
	gint	     hpane_pos_month_view;
	gint	     vpane_pos_month_view;

	/* The signal handler id for our GtkCalendar "day_selected" handler. */
	guint	     day_selected_id;

	/* View instance and menus for the control */
	GalViewInstance *view_instance;
	GalViewMenus *view_menus;

	/* Our current timezone. */
	icaltimezone *zone;

	/* The dates currently shown. If they are -1 then we have no dates
	   shown. We only use these to check if we need to emit a
	   'dates-shown-changed' signal.*/
	time_t visible_start;
	time_t visible_end;

	/* Calendar query for purging old events */
	CalQuery *exp_query;
	time_t exp_older_than;
};

/* Signal IDs */

enum {
	DATES_SHOWN_CHANGED,
	CALENDAR_SELECTION_CHANGED,
	TASKPAD_SELECTION_CHANGED,
	CALENDAR_FOCUS_CHANGE,
	TASKPAD_FOCUS_CHANGE,
	GOTO_DATE,
	LAST_SIGNAL
};

/* Used to indicate who has the focus within the calendar view */
typedef enum {
	FOCUS_CALENDAR,
	FOCUS_TASKPAD,
	FOCUS_OTHER
} FocusLocation;

static guint gnome_calendar_signals[LAST_SIGNAL];




static void gnome_calendar_class_init (GnomeCalendarClass *class);
static void gnome_calendar_init (GnomeCalendar *gcal);
static void gnome_calendar_destroy (GtkObject *object);
static void gnome_calendar_goto_date (GnomeCalendar *gcal,
				      GnomeCalendarGotoDateType goto_date);

static void gnome_calendar_set_pane_positions	(GnomeCalendar	*gcal);
static void gnome_calendar_update_view_times (GnomeCalendar *gcal);
static void gnome_calendar_update_date_navigator (GnomeCalendar *gcal);

static void gnome_calendar_hpane_realized (GtkWidget *w, GnomeCalendar *gcal);
static void gnome_calendar_vpane_realized (GtkWidget *w, GnomeCalendar *gcal);
static gboolean gnome_calendar_vpane_resized (GtkWidget *w, GdkEventButton *e, GnomeCalendar *gcal);
static gboolean gnome_calendar_hpane_resized (GtkWidget *w, GdkEventButton *e, GnomeCalendar *gcal);

static void gnome_calendar_on_date_navigator_date_range_changed (ECalendarItem *calitem,
								 GnomeCalendar *gcal);
static void gnome_calendar_on_date_navigator_selection_changed (ECalendarItem    *calitem,
								GnomeCalendar    *gcal);
static void gnome_calendar_notify_dates_shown_changed (GnomeCalendar *gcal);

static void add_alarms (const char *uri);
static void update_query (GnomeCalendar *gcal);


static GtkVBoxClass *parent_class;




E_MAKE_TYPE (gnome_calendar, "GnomeCalendar", GnomeCalendar, gnome_calendar_class_init,
	     gnome_calendar_init, GTK_TYPE_VBOX);

/* Class initialization function for the gnome calendar */
static void
gnome_calendar_class_init (GnomeCalendarClass *class)
{
	GtkObjectClass *object_class;
	GtkBindingSet *binding_set;

	object_class = (GtkObjectClass *) class;

	parent_class = g_type_class_peek_parent (class);

	gnome_calendar_signals[DATES_SHOWN_CHANGED] =
		gtk_signal_new ("dates_shown_changed",
				GTK_RUN_LAST,
				G_TYPE_FROM_CLASS (object_class),
				GTK_SIGNAL_OFFSET (GnomeCalendarClass,
						   dates_shown_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gnome_calendar_signals[CALENDAR_SELECTION_CHANGED] =
		gtk_signal_new ("calendar_selection_changed",
				GTK_RUN_LAST,
				G_TYPE_FROM_CLASS (object_class),
				GTK_SIGNAL_OFFSET (GnomeCalendarClass, calendar_selection_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gnome_calendar_signals[TASKPAD_SELECTION_CHANGED] =
		gtk_signal_new ("taskpad_selection_changed",
				GTK_RUN_LAST,
				G_TYPE_FROM_CLASS (object_class),
				GTK_SIGNAL_OFFSET (GnomeCalendarClass, taskpad_selection_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gnome_calendar_signals[CALENDAR_FOCUS_CHANGE] =
		gtk_signal_new ("calendar_focus_change",
				GTK_RUN_FIRST,
				G_TYPE_FROM_CLASS (object_class),
				GTK_SIGNAL_OFFSET (GnomeCalendarClass, calendar_focus_change),
				gtk_marshal_NONE__BOOL,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_BOOL);

	gnome_calendar_signals[TASKPAD_FOCUS_CHANGE] =
		gtk_signal_new ("taskpad_focus_change",
				GTK_RUN_FIRST,
				G_TYPE_FROM_CLASS (object_class),
				GTK_SIGNAL_OFFSET (GnomeCalendarClass, taskpad_focus_change),
				gtk_marshal_NONE__BOOL,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_BOOL);

	gnome_calendar_signals[GOTO_DATE] =
		g_signal_new ("goto_date",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (GnomeCalendarClass, goto_date),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__ENUM,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_INT);


	object_class->destroy = gnome_calendar_destroy;

	class->dates_shown_changed = NULL;
	class->calendar_selection_changed = NULL;
	class->taskpad_selection_changed = NULL;
	class->calendar_focus_change = NULL;
	class->taskpad_focus_change = NULL;
	class->goto_date = gnome_calendar_goto_date;

	/*
	 * Key bindings
	 */

	binding_set = gtk_binding_set_by_class (class);

	/* Alt+PageUp/PageDown, go to the first/last day of the month */
	gtk_binding_entry_add_signal (binding_set, GDK_Page_Up,
				      GDK_MOD1_MASK,
				      "goto_date", 1,
				      G_TYPE_ENUM,
				      GNOME_CAL_GOTO_FIRST_DAY_OF_MONTH);
	gtk_binding_entry_add_signal (binding_set, GDK_KP_Page_Up,
				      GDK_MOD1_MASK,
				      "goto_date", 1,
				      G_TYPE_ENUM,
				      GNOME_CAL_GOTO_FIRST_DAY_OF_MONTH);
	gtk_binding_entry_add_signal (binding_set, GDK_Page_Down,
				      GDK_MOD1_MASK,
				      "goto_date", 1,
				      G_TYPE_ENUM,
				      GNOME_CAL_GOTO_LAST_DAY_OF_MONTH);
	gtk_binding_entry_add_signal (binding_set, GDK_KP_Page_Down,
				      GDK_MOD1_MASK,
				      "goto_date", 1,
				      G_TYPE_ENUM,
				      GNOME_CAL_GOTO_LAST_DAY_OF_MONTH);

	/* Alt+Home/End, go to the first/last day of the week */
	gtk_binding_entry_add_signal (binding_set, GDK_Home,
				      GDK_MOD1_MASK,
				      "goto_date", 1,
				      G_TYPE_ENUM,
				      GNOME_CAL_GOTO_FIRST_DAY_OF_WEEK);
	gtk_binding_entry_add_signal (binding_set, GDK_End,
				      GDK_MOD1_MASK,
				      "goto_date", 1,
				      G_TYPE_ENUM,
				      GNOME_CAL_GOTO_LAST_DAY_OF_WEEK);
	gtk_binding_entry_add_signal (binding_set, GDK_KP_Home,
				      GDK_MOD1_MASK,
				      "goto_date", 1,
				      G_TYPE_ENUM,
				      GNOME_CAL_GOTO_FIRST_DAY_OF_WEEK);
	gtk_binding_entry_add_signal (binding_set, GDK_KP_End,
				      GDK_MOD1_MASK,
				      "goto_date", 1,
				      G_TYPE_ENUM,
				      GNOME_CAL_GOTO_LAST_DAY_OF_WEEK);

	/*Alt+Left/Right, go to the same day of the previous/next week*/
	gtk_binding_entry_add_signal (binding_set,GDK_Left,
				      GDK_MOD1_MASK,
				      "goto_date",1,
				      G_TYPE_ENUM,
				      GNOME_CAL_GOTO_SAME_DAY_OF_PREVIOUS_WEEK);
	gtk_binding_entry_add_signal (binding_set,GDK_KP_Left,
				      GDK_MOD1_MASK,
				      "goto_date",1,
				      G_TYPE_ENUM,
				      GNOME_CAL_GOTO_SAME_DAY_OF_PREVIOUS_WEEK);
	gtk_binding_entry_add_signal (binding_set,GDK_Right,
				      GDK_MOD1_MASK,
				      "goto_date",1,
				      G_TYPE_ENUM,
				      GNOME_CAL_GOTO_SAME_DAY_OF_NEXT_WEEK);
	gtk_binding_entry_add_signal (binding_set,GDK_KP_Right,
				      GDK_MOD1_MASK,
				      "goto_date",1,
				      G_TYPE_ENUM,
				      GNOME_CAL_GOTO_SAME_DAY_OF_NEXT_WEEK);
}

/* Callback used when the calendar query reports of an updated object */
static void
dn_query_obj_updated_cb (CalQuery *query, const char *uid,
			 gboolean query_in_progress, int n_scanned, int total,
			 gpointer data)
{
	GnomeCalendar *gcal;
	GnomeCalendarPrivate *priv;
	CalComponent *comp;
	CalClientGetStatus status;

	gcal = GNOME_CALENDAR (data);
	priv = gcal->priv;

	/* If this is an update that is not part of an ongoing query, we have to
	 * retag the whole thing:  an event may change dates and the
	 * tag_calendar_by_comp() below would not know how to untag the old
	 * dates.
	 */
	if (!query_in_progress) {
		update_query (gcal);
		return;
	}

	status = cal_client_get_object (priv->client, uid, &comp);

	switch (status) {
	case CAL_CLIENT_GET_SUCCESS:
		/* Everything is fine */
		break;

	case CAL_CLIENT_GET_SYNTAX_ERROR:
		g_message ("dn_query_obj_updated_cb(): Syntax error while getting object `%s'", uid);
		break;

	case CAL_CLIENT_GET_NOT_FOUND:
		/* The object is no longer in the server, so do nothing */
		break;

	default:
		g_assert_not_reached ();
		return;
	}

	tag_calendar_by_comp (priv->date_navigator, comp, priv->client, NULL,
			      FALSE, TRUE);
	g_object_unref (comp);
}

/* Callback used when the calendar query reports of a removed object */
static void
dn_query_obj_removed_cb (CalQuery *query, const char *uid, gpointer data)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	/* Just retag the whole thing */
	update_query (gcal);
}

/* Callback used when the calendar query is done */
static void
dn_query_query_done_cb (CalQuery *query, CalQueryDoneStatus status, const char *error_str,
			gpointer data)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	/* FIXME */

	if (status != CAL_QUERY_DONE_SUCCESS)
		fprintf (stderr, "query done: %s\n", error_str);
}

/* Callback used when the calendar query reports an evaluation error */
static void
dn_query_eval_error_cb (CalQuery *query, const char *error_str, gpointer data)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	/* FIXME */

	fprintf (stderr, "eval error: %s\n", error_str);
}

/* Returns the current view widget, a EDayView or EWeekView. */
GtkWidget*
gnome_calendar_get_current_view_widget (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	GtkWidget *retval = NULL;

	priv = gcal->priv;

	switch (priv->current_view_type) {
	case GNOME_CAL_DAY_VIEW:
		retval = priv->day_view;
		break;
	case GNOME_CAL_WORK_WEEK_VIEW:
		retval = priv->work_week_view;
		break;
	case GNOME_CAL_WEEK_VIEW:
		retval = priv->week_view;
		break;
	case GNOME_CAL_MONTH_VIEW:
		retval = priv->month_view;
		break;
	default:
		g_assert_not_reached ();
	}

	return retval;
}

/* Gets the focus location based on who is the focused widget within the
 * calendar view.
 */
static FocusLocation
get_focus_location (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	ETable *etable;

	priv = gcal->priv;

	etable = e_calendar_table_get_table (E_CALENDAR_TABLE (priv->todo));

	if (GTK_WIDGET_HAS_FOCUS (etable->table_canvas))
		return FOCUS_TASKPAD;
	else {
		GtkWidget *widget;
		EDayView *dv;
		EWeekView *wv;

		widget = gnome_calendar_get_current_view_widget (gcal);

		switch (priv->current_view_type) {
		case GNOME_CAL_DAY_VIEW:
		case GNOME_CAL_WORK_WEEK_VIEW:
			dv = E_DAY_VIEW (widget);

			if (GTK_WIDGET_HAS_FOCUS (dv->top_canvas)
			    || GTK_WIDGET_HAS_FOCUS (dv->main_canvas))
				return FOCUS_CALENDAR;
			else
				return FOCUS_OTHER;

		case GNOME_CAL_WEEK_VIEW:
		case GNOME_CAL_MONTH_VIEW:
			wv = E_WEEK_VIEW (widget);

			if (GTK_WIDGET_HAS_FOCUS (wv->main_canvas))
				return FOCUS_CALENDAR;
			else
				return FOCUS_OTHER;

		default:
			g_assert_not_reached ();
			return FOCUS_OTHER;
		}
	}
}

/* Computes the range of time that the date navigator is showing */
static void
get_date_navigator_range (GnomeCalendar *gcal, time_t *start_time, time_t *end_time)
{
	GnomeCalendarPrivate *priv;
	gint start_year, start_month, start_day;
	gint end_year, end_month, end_day;
	struct icaltimetype start_tt;
	struct icaltimetype end_tt;

	priv = gcal->priv;

	start_tt = icaltime_null_time ();
	end_tt = icaltime_null_time ();

	if (!e_calendar_item_get_date_range (priv->date_navigator->calitem,
					     &start_year, &start_month, &start_day,
					     &end_year, &end_month, &end_day)) {
		*start_time = -1;
		*end_time = -1;
		return;
	}

	start_tt.year = start_year;
	start_tt.month = start_month + 1;
	start_tt.day = start_day;

	end_tt.year = end_year;
	end_tt.month = end_month + 1;
	end_tt.day = end_day;

	icaltime_adjust (&end_tt, 1, 0, 0, 0);

	*start_time = icaltime_as_timet_with_zone (start_tt, priv->zone);
	*end_time = icaltime_as_timet_with_zone (end_tt, priv->zone);
}

/* Adjusts a given query sexp with the time range of the date navigator */
static char *
adjust_query_sexp (GnomeCalendar *gcal, const char *sexp)
{
	time_t start_time, end_time;
	char *start, *end;
	char *new_sexp;

	get_date_navigator_range (gcal, &start_time, &end_time);
	if (start_time == -1 || end_time == -1)
		return NULL;

	start = isodate_from_time_t (start_time);
	end = isodate_from_time_t (end_time);

	new_sexp = g_strdup_printf ("(and (= (get-vtype) \"VEVENT\")"
				    "     (occur-in-time-range? (make-time \"%s\")"
				    "                           (make-time \"%s\"))"
				    "     %s)",
				    start, end,
				    sexp);

	g_free (start);
	g_free (end);

	return new_sexp;
}

/* Restarts a query for the date navigator in the calendar */
static void
update_query (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	CalQuery *old_query;
	char *real_sexp;

	priv = gcal->priv;

	e_calendar_item_clear_marks (priv->date_navigator->calitem);

	if (!(priv->client
	      && cal_client_get_load_state (priv->client) == CAL_CLIENT_LOAD_LOADED))
		return;

	old_query = priv->dn_query;
	priv->dn_query = NULL;

	if (old_query) {
		g_signal_handlers_disconnect_matched (old_query, G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, gcal);
		g_object_unref (old_query);
	}

	g_assert (priv->sexp != NULL);

	real_sexp = adjust_query_sexp (gcal, priv->sexp);
	if (!real_sexp)
		return; /* No time range is set, so don't start a query */

	priv->dn_query = cal_client_get_query (priv->client, real_sexp);
	g_free (real_sexp);

	if (!priv->dn_query) {
		g_message ("update_query(): Could not create the query");
		return;
	}

	g_signal_connect (priv->dn_query, "obj_updated",
			  G_CALLBACK (dn_query_obj_updated_cb), gcal);
	g_signal_connect (priv->dn_query, "obj_removed",
			  G_CALLBACK (dn_query_obj_removed_cb), gcal);
	g_signal_connect (priv->dn_query, "query_done",
			  G_CALLBACK (dn_query_query_done_cb), gcal);
	g_signal_connect (priv->dn_query, "eval_error",
			  G_CALLBACK (dn_query_eval_error_cb), gcal);
}

/**
 * gnome_calendar_set_query:
 * @gcal: A calendar.
 * @sexp: Sexp that defines the query.
 * 
 * Sets the query sexp for all the views in a calendar.
 **/
void
gnome_calendar_set_query (GnomeCalendar *gcal, const char *sexp)
{
	GnomeCalendarPrivate *priv;
	CalendarModel *model;

	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (sexp != NULL);

	priv = gcal->priv;

	/* Set the query on the date navigator */

	if (priv->sexp)
		g_free (priv->sexp);

	priv->sexp = g_strdup (sexp);

	update_query (gcal);

	/* Set the query on the main view */

	switch (priv->current_view_type) {
	case GNOME_CAL_DAY_VIEW:
		e_day_view_set_query (E_DAY_VIEW (priv->day_view), sexp);
		break;

	case GNOME_CAL_WORK_WEEK_VIEW:
		e_day_view_set_query (E_DAY_VIEW (priv->work_week_view), sexp);
		break;

	case GNOME_CAL_WEEK_VIEW:
		e_week_view_set_query (E_WEEK_VIEW (priv->week_view), sexp);
		break;

	case GNOME_CAL_MONTH_VIEW:
		e_week_view_set_query (E_WEEK_VIEW (priv->month_view), sexp);
		break;

	default:
		g_warning ("A penguin bit my hand!");
		g_assert_not_reached ();
	}

	/* Set the query on the task pad */

	model = e_calendar_table_get_model (E_CALENDAR_TABLE (priv->todo));
	calendar_model_set_query (model, sexp);
}

/* Returns the current time, for the ECalendarItem. */
static struct tm
get_current_time (ECalendarItem *calitem, gpointer data)
{
	GnomeCalendar *cal = data;
	struct tm tmp_tm = { 0 };
	struct icaltimetype tt;

	g_return_val_if_fail (cal != NULL, tmp_tm);
	g_return_val_if_fail (GNOME_IS_CALENDAR (cal), tmp_tm);

	tt = icaltime_from_timet_with_zone (time (NULL), FALSE,
					    cal->priv->zone);

	/* Now copy it to the struct tm and return it. */
	tmp_tm = icaltimetype_to_tm (&tt);

	return tmp_tm;
}

/* Callback used when the sexp changes in the calendar search bar */
static void
search_bar_sexp_changed_cb (CalSearchBar *cal_search, const char *sexp, gpointer data)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);
	gnome_calendar_set_query (gcal, sexp);
}

/* Callback used when the selected category in the search bar changes */
static void
search_bar_category_changed_cb (CalSearchBar *cal_search, const char *category, gpointer data)
{
	GnomeCalendar *gcal;
	GnomeCalendarPrivate *priv;
	CalendarModel *model;

	gcal = GNOME_CALENDAR (data);
	priv = gcal->priv;

	e_day_view_set_default_category (E_DAY_VIEW (priv->day_view), category);
	e_day_view_set_default_category (E_DAY_VIEW (priv->work_week_view), category);
	e_week_view_set_default_category (E_WEEK_VIEW (priv->week_view), category);
	e_week_view_set_default_category (E_WEEK_VIEW (priv->month_view), category);

	model = e_calendar_table_get_model (E_CALENDAR_TABLE (priv->todo));
	calendar_model_set_default_category (model, category);
}

static void
view_selection_changed_cb (GtkWidget *view, GnomeCalendar *gcal)
{
	gtk_signal_emit (GTK_OBJECT (gcal),
			 gnome_calendar_signals[CALENDAR_SELECTION_CHANGED]);
}


/* Callback used when the taskpad receives a focus event.  We emit the
 * corresponding signal so that parents can change the menus as appropriate.
 */
static gint
table_canvas_focus_change_cb (GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	gtk_signal_emit (GTK_OBJECT (gcal), gnome_calendar_signals [TASKPAD_FOCUS_CHANGE],
			 event->in ? TRUE : FALSE);

	return FALSE;
}

static gint
calendar_focus_change_cb (GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	gtk_signal_emit (GTK_OBJECT (gcal), gnome_calendar_signals [CALENDAR_FOCUS_CHANGE],
			 event->in ? TRUE : FALSE);

	return FALSE;
}

/* Connects to the focus change signals of a day view widget */
static void
connect_day_view_focus (GnomeCalendar *gcal, EDayView *dv)
{
	g_signal_connect (dv->top_canvas, "focus_in_event",
			  G_CALLBACK (calendar_focus_change_cb), gcal);
	g_signal_connect (dv->top_canvas, "focus_out_event",
			  G_CALLBACK (calendar_focus_change_cb), gcal);

	g_signal_connect (dv->main_canvas, "focus_in_event",
			  G_CALLBACK (calendar_focus_change_cb), gcal);
	g_signal_connect (dv->main_canvas, "focus_out_event",
			  G_CALLBACK (calendar_focus_change_cb), gcal);
}

/* Connects to the focus change signals of a week view widget */
static void
connect_week_view_focus (GnomeCalendar *gcal, EWeekView *wv)
{
	g_signal_connect (wv->main_canvas, "focus_in_event",
			  G_CALLBACK (calendar_focus_change_cb), gcal);
	g_signal_connect (wv->main_canvas, "focus_out_event",
			  G_CALLBACK (calendar_focus_change_cb), gcal);
}

/* Callback used when the selection in the taskpad table changes.  We just proxy
 * the signal with our own one.
 */
static void
table_selection_change_cb (ETable *etable, gpointer data)
{
	GnomeCalendar *gcal;
	int n_selected;

	gcal = GNOME_CALENDAR (data);

	n_selected = e_table_selected_count (etable);
	gtk_signal_emit (GTK_OBJECT (gcal), gnome_calendar_signals[TASKPAD_SELECTION_CHANGED]);
}

static void
setup_widgets (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	GtkWidget *w;
	gchar *filename;
	CalendarModel *model;
	ETable *etable;

	priv = gcal->priv;

	priv->search_bar = cal_search_bar_new ();
	g_signal_connect (priv->search_bar, "sexp_changed",
			  G_CALLBACK (search_bar_sexp_changed_cb), gcal);
	g_signal_connect (priv->search_bar, "category_changed",
			  G_CALLBACK (search_bar_category_changed_cb), gcal);

	gtk_widget_show (priv->search_bar);
	gtk_box_pack_start (GTK_BOX (gcal), priv->search_bar, FALSE, FALSE, 6);

	/* The main HPaned, with the notebook of calendar views on the left
	   and the ECalendar and ToDo list on the right. */
	priv->hpane = gtk_hpaned_new ();
	g_signal_connect_after(priv->hpane, "realize", 
			       G_CALLBACK(gnome_calendar_hpane_realized), gcal);
	g_signal_connect (priv->hpane, "button_release_event",
			  G_CALLBACK (gnome_calendar_hpane_resized), gcal);
	gtk_widget_show (priv->hpane);
	gtk_box_pack_start (GTK_BOX (gcal), priv->hpane, TRUE, TRUE, 6);

	/* The Notebook containing the 4 calendar views. */
	priv->notebook = gtk_notebook_new ();
	gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->notebook), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);
	gtk_widget_show (priv->notebook);
	gtk_paned_pack1 (GTK_PANED (priv->hpane), priv->notebook, FALSE, TRUE);

	/* The VPaned widget, to contain the GtkCalendar & ToDo list. */
	priv->vpane = gtk_vpaned_new ();
	g_signal_connect_after (priv->vpane, "realize",
				G_CALLBACK(gnome_calendar_vpane_realized), gcal);
	g_signal_connect (priv->vpane, "button_release_event",
			  G_CALLBACK (gnome_calendar_vpane_resized), gcal);
	gtk_widget_show (priv->vpane);
	gtk_paned_pack2 (GTK_PANED (priv->hpane), priv->vpane, TRUE, TRUE);

	/* The ECalendar. */
	w = e_calendar_new ();
	priv->date_navigator = E_CALENDAR (w);
	e_calendar_item_set_days_start_week_sel (priv->date_navigator->calitem, 9);
	e_calendar_item_set_max_days_sel (priv->date_navigator->calitem, 42);
	gtk_widget_show (w);
	e_calendar_item_set_get_time_callback (priv->date_navigator->calitem,
					       (ECalendarItemGetTimeCallback) get_current_time,
					       gcal, NULL);

	gtk_paned_pack1 (GTK_PANED (priv->vpane), w, FALSE, TRUE);	

	g_signal_connect (priv->date_navigator->calitem, "selection_changed",
			  G_CALLBACK (gnome_calendar_on_date_navigator_selection_changed), gcal);
	g_signal_connect (priv->date_navigator->calitem, "date_range_changed",
			  G_CALLBACK (gnome_calendar_on_date_navigator_date_range_changed), gcal);

	/* The ToDo list. */
	priv->todo = e_calendar_table_new ();
	calendar_config_configure_e_calendar_table (E_CALENDAR_TABLE (priv->todo));
	model = e_calendar_table_get_model (E_CALENDAR_TABLE (priv->todo));
	calendar_model_set_new_comp_vtype (model, CAL_COMPONENT_TODO);
	gtk_paned_pack2 (GTK_PANED (priv->vpane), priv->todo, TRUE, TRUE);
	gtk_widget_show (priv->todo);

	filename = g_strdup_printf ("%s/config/TaskPad", evolution_dir);
	e_calendar_table_load_state (E_CALENDAR_TABLE (priv->todo), filename);
	g_free (filename);

	etable = e_calendar_table_get_table (E_CALENDAR_TABLE (priv->todo));
	g_signal_connect (etable->table_canvas, "focus_in_event",
			  G_CALLBACK (table_canvas_focus_change_cb), gcal);
	g_signal_connect (etable->table_canvas, "focus_out_event",
			  G_CALLBACK (table_canvas_focus_change_cb), gcal);

	g_signal_connect (etable, "selection_change",
			  G_CALLBACK (table_selection_change_cb), gcal);

	/* The Day View. */
	priv->day_view = e_day_view_new ();
	e_cal_view_set_calendar (E_CAL_VIEW (priv->day_view), gcal);
	gtk_widget_show (priv->day_view);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
				  priv->day_view, gtk_label_new (""));
	g_signal_connect (priv->day_view, "selection_changed",
			  G_CALLBACK (view_selection_changed_cb), gcal);

	connect_day_view_focus (gcal, E_DAY_VIEW (priv->day_view));

	/* The Work Week View. */
	priv->work_week_view = e_day_view_new ();
	e_day_view_set_work_week_view (E_DAY_VIEW (priv->work_week_view),
				       TRUE);
	e_day_view_set_days_shown (E_DAY_VIEW (priv->work_week_view), 5);
	e_cal_view_set_calendar (E_CAL_VIEW (priv->work_week_view), gcal);
	gtk_widget_show (priv->work_week_view);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
				  priv->work_week_view, gtk_label_new (""));
	g_signal_connect (priv->work_week_view, "selection_changed",
			  G_CALLBACK (view_selection_changed_cb), gcal);

	connect_day_view_focus (gcal, E_DAY_VIEW (priv->work_week_view));

	/* The Week View. */
	priv->week_view = e_week_view_new ();
	e_cal_view_set_calendar (E_CAL_VIEW (priv->week_view), gcal);
	gtk_widget_show (priv->week_view);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
				  priv->week_view, gtk_label_new (""));
	g_signal_connect (priv->week_view, "selection_changed",
			  G_CALLBACK (view_selection_changed_cb), gcal);

	connect_week_view_focus (gcal, E_WEEK_VIEW (priv->week_view));

	/* The Month View. */
	priv->month_view = e_week_view_new ();
	e_cal_view_set_calendar (E_CAL_VIEW (priv->month_view), gcal);
	e_week_view_set_multi_week_view (E_WEEK_VIEW (priv->month_view), TRUE);
	gtk_widget_show (priv->month_view);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
				  priv->month_view, gtk_label_new (""));
	g_signal_connect (priv->month_view, "selection_changed",
			  G_CALLBACK (view_selection_changed_cb), gcal);

	connect_week_view_focus (gcal, E_WEEK_VIEW (priv->month_view));

	gnome_calendar_update_config_settings (gcal, TRUE);
}

/* Object initialization function for the gnome calendar */
static void
gnome_calendar_init (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;

	priv = g_new0 (GnomeCalendarPrivate, 1);
	gcal->priv = priv;

	priv->cal_categories = NULL;
	priv->tasks_categories = NULL;

	priv->current_view_type = GNOME_CAL_DAY_VIEW;
	priv->range_selected = FALSE;

	setup_widgets (gcal);
	priv->dn_query = NULL;
	priv->sexp = g_strdup ("#t"); /* Match all */

	priv->selection_start_time = time_day_begin_with_zone (time (NULL),
							       priv->zone);
	priv->selection_end_time = time_add_day_with_zone (priv->selection_start_time, 1, priv->zone);

	priv->view_instance = NULL;
	priv->view_menus = NULL;

	priv->visible_start = -1;
	priv->visible_end = -1;

	priv->exp_query = NULL;
}

/* Frees a set of categories */
static void
free_categories (GPtrArray *categories)
{
	int i;

	if (!categories)
		return;

	for (i = 0; i < categories->len; i++)
		g_free (categories->pdata[i]);

	g_ptr_array_free (categories, TRUE);
}

static void
gnome_calendar_destroy (GtkObject *object)
{
	GnomeCalendar *gcal;
	GnomeCalendarPrivate *priv;
	gchar *filename;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (object));

	gcal = GNOME_CALENDAR (object);
	priv = gcal->priv;

	if (priv) {
		free_categories (priv->cal_categories);
		priv->cal_categories = NULL;

		free_categories (priv->tasks_categories);
		priv->tasks_categories = NULL;

		/* Save the TaskPad layout. */
		filename = g_strdup_printf ("%s/config/TaskPad", evolution_dir);
		e_calendar_table_save_state (E_CALENDAR_TABLE (priv->todo), filename);
		g_free (filename);

		if (priv->dn_query) {
			g_signal_handlers_disconnect_matched (priv->dn_query, G_SIGNAL_MATCH_DATA,
							      0, 0, NULL, NULL, gcal);
			g_object_unref (priv->dn_query);
			priv->dn_query = NULL;
		}

		if (priv->sexp) {
			g_free (priv->sexp);
			priv->sexp = NULL;
		}

		if (priv->query_timeout) {
			g_source_remove (priv->query_timeout);
			priv->query_timeout = 0;
		}
	
		if (priv->client) {
			g_signal_handlers_disconnect_matched (priv->client, G_SIGNAL_MATCH_DATA,
							      0, 0, NULL, NULL, gcal);
			g_object_unref (priv->client);
			priv->client = NULL;
		}

		if (priv->task_pad_client) {
			g_signal_handlers_disconnect_matched (priv->task_pad_client, G_SIGNAL_MATCH_DATA,
							      0, 0, NULL, NULL, gcal);
			g_object_unref (priv->task_pad_client);
			priv->task_pad_client = NULL;
		}

		if (priv->view_instance) {
			g_object_unref (priv->view_instance);
			priv->view_instance = NULL;
		}

		if (priv->view_menus) {
			g_object_unref (priv->view_menus);
			priv->view_menus = NULL;
		}

		if (priv->exp_query) {
			g_signal_handlers_disconnect_matched (priv->exp_query, G_SIGNAL_MATCH_DATA,
							      0, 0, NULL, NULL, gcal);
			g_object_unref (priv->exp_query);
			priv->exp_query = NULL;
		}

		g_free (priv);
		gcal->priv = NULL;
	}
	
	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gnome_calendar_goto_date (GnomeCalendar *gcal,
			  GnomeCalendarGotoDateType goto_date)
{
	GnomeCalendarPrivate *priv;
	time_t	 start_time;
	time_t	 end_time;
	gboolean need_updating = FALSE;

	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR(gcal));

	priv = gcal->priv;

	gnome_calendar_get_current_time_range (gcal, &start_time, &end_time);

	switch (goto_date) {
		/* GNOME_CAL_GOTO_TODAY and GNOME_CAL_GOTO_DATE are
		   currently not used
		*/
	case GNOME_CAL_GOTO_TODAY:
		break;
	case GNOME_CAL_GOTO_DATE:
		break;
	case GNOME_CAL_GOTO_FIRST_DAY_OF_MONTH:
		priv->selection_start_time =
			time_month_begin_with_zone (start_time, priv->zone);
		priv->selection_end_time =
			time_add_day_with_zone (priv->selection_start_time,
						1, priv->zone);
		need_updating = TRUE;
		break;
	case GNOME_CAL_GOTO_LAST_DAY_OF_MONTH:
		start_time = time_add_month_with_zone (start_time, 1,
						       priv->zone);
		priv->selection_end_time =
			time_month_begin_with_zone (start_time, priv->zone);
		priv->selection_start_time =
			time_add_day_with_zone (priv->selection_end_time,
						-1, priv->zone);
		need_updating = TRUE;
		break;
	case GNOME_CAL_GOTO_FIRST_DAY_OF_WEEK:
		/* 1 for Monday */
		priv->selection_start_time =
			time_week_begin_with_zone (start_time, 1, priv->zone);
		priv->selection_end_time =
			time_add_day_with_zone (priv->selection_start_time,
						1, priv->zone);
		need_updating = TRUE;
		break;
	case GNOME_CAL_GOTO_LAST_DAY_OF_WEEK:
		/* 1 for Monday */
		start_time = time_week_begin_with_zone (start_time, 1,
							priv->zone);
		if (priv->current_view_type == GNOME_CAL_DAY_VIEW ||
		    priv->current_view_type == GNOME_CAL_WORK_WEEK_VIEW) {
			/* goto Friday of this week */
			priv->selection_start_time =
				time_add_day_with_zone (start_time,
							4, priv->zone);
		}
		else {
			/* goto Sunday of this week */
			priv->selection_start_time =
				time_add_day_with_zone (start_time,
							6, priv->zone);
		}
		priv->selection_end_time =
			time_add_day_with_zone (priv->selection_start_time,
						1, priv->zone);
		need_updating = TRUE;
		break;
	case GNOME_CAL_GOTO_SAME_DAY_OF_PREVIOUS_WEEK:
		priv->selection_start_time = time_add_day_with_zone (start_time,
								     -7, priv->zone);
		priv->selection_end_time = time_add_day_with_zone (end_time,
								   -7,priv->zone);
		need_updating = TRUE;
		break;
	case GNOME_CAL_GOTO_SAME_DAY_OF_NEXT_WEEK:
		priv->selection_start_time = time_add_day_with_zone (start_time,
								     7, priv->zone);
		priv->selection_end_time = time_add_day_with_zone (end_time,
								   7,priv->zone);
		need_updating = TRUE;
		break;

	default:
		break;
	}

	if (need_updating) {
		gnome_calendar_update_view_times (gcal);
		gnome_calendar_update_date_navigator (gcal);
		gnome_calendar_notify_dates_shown_changed (gcal);
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

	priv->selection_start_time = time_day_begin_with_zone (new_time,
							       priv->zone);
	priv->selection_end_time = time_add_day_with_zone (priv->selection_start_time, 1, priv->zone);

	gnome_calendar_update_view_times (gcal);
	gnome_calendar_update_date_navigator (gcal);
	gnome_calendar_notify_dates_shown_changed (gcal);
}


static void
gnome_calendar_update_view_times (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;

	priv = gcal->priv;

	switch (priv->current_view_type) {
	case GNOME_CAL_DAY_VIEW:
		e_day_view_set_selected_time_range (E_DAY_VIEW (priv->day_view),
						    priv->selection_start_time,
						    priv->selection_end_time);
		break;

	case GNOME_CAL_WORK_WEEK_VIEW:
		e_day_view_set_selected_time_range (E_DAY_VIEW (priv->work_week_view),
						    priv->selection_start_time,
						    priv->selection_end_time);
		break;

	case GNOME_CAL_WEEK_VIEW:
		e_week_view_set_selected_time_range (E_WEEK_VIEW (priv->week_view),
						     priv->selection_start_time,
						     priv->selection_end_time);
		break;

	case GNOME_CAL_MONTH_VIEW:
		e_week_view_set_selected_time_range (E_WEEK_VIEW (priv->month_view),
						     priv->selection_start_time,
						     priv->selection_end_time);
		break;

	default:
		g_warning ("My penguin is gone!");
		g_assert_not_reached ();
	}
}

static void
gnome_calendar_direction (GnomeCalendar *gcal, int direction)
{
	GnomeCalendarPrivate *priv;
	time_t start_time, end_time;

	priv = gcal->priv;

	start_time = priv->selection_start_time;
	end_time = priv->selection_end_time;

	switch (priv->current_view_type) {
	case GNOME_CAL_DAY_VIEW:
		start_time = time_add_day_with_zone (start_time, direction,
						     priv->zone);
		end_time = time_add_day_with_zone (end_time, direction,
						   priv->zone);
		break;

	case GNOME_CAL_WORK_WEEK_VIEW:
		start_time = time_add_week_with_zone (start_time, direction,
						      priv->zone);
		end_time = time_add_week_with_zone (end_time, direction,
						    priv->zone);
		break;

	case GNOME_CAL_WEEK_VIEW:
		start_time = time_add_week_with_zone (start_time, direction,
						      priv->zone);
		end_time = time_add_week_with_zone (end_time, direction,
						    priv->zone);
		break;

	case GNOME_CAL_MONTH_VIEW:
		start_time = time_add_month_with_zone (start_time, direction,
						       priv->zone);
		end_time = time_add_month_with_zone (end_time, direction,
						     priv->zone);
		break;

	default:
		g_warning ("Weee!  Where did the penguin go?");
		g_assert_not_reached ();
		return;
	}

	priv->selection_start_time = start_time;
	priv->selection_end_time = end_time;

	gnome_calendar_update_view_times (gcal);
	gnome_calendar_update_date_navigator (gcal);
	gnome_calendar_notify_dates_shown_changed (gcal);
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

	priv->selection_start_time = time_day_begin_with_zone (time,
							       priv->zone);
	priv->selection_end_time = time_add_day_with_zone (priv->selection_start_time, 1, priv->zone);

	gnome_calendar_set_view (gcal, GNOME_CAL_DAY_VIEW, FALSE, TRUE);
}

static void
focus_current_view (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;

	priv = gcal->priv;

	switch (priv->current_view_type) {
	case GNOME_CAL_DAY_VIEW:
		gtk_widget_grab_focus (priv->day_view);
		break;

	case GNOME_CAL_WORK_WEEK_VIEW:
		gtk_widget_grab_focus (priv->work_week_view);
		break;

	case GNOME_CAL_WEEK_VIEW:
		gtk_widget_grab_focus (priv->week_view);
		break;

	case GNOME_CAL_MONTH_VIEW:
		gtk_widget_grab_focus (priv->month_view);
		break;

	default:
		g_warning ("A penguin fell on its face!");
		g_assert_not_reached ();
	}
}

void
gnome_calendar_goto_today (GnomeCalendar *gcal)
{
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	gnome_calendar_goto (gcal, time (NULL));
	focus_current_view (gcal);
}

/**
 * gnome_calendar_get_view:
 * @gcal: A calendar.
 *
 * Queries the type of the view that is being shown in a calendar.
 *
 * Return value: Type of the view that is currently shown.
 **/
GnomeCalendarViewType
gnome_calendar_get_view (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;

	g_return_val_if_fail (gcal != NULL, GNOME_CAL_DAY_VIEW);
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), GNOME_CAL_DAY_VIEW);

	priv = gcal->priv;
	return priv->current_view_type;
}

/* Sets the view without changing the selection or updating the date
 * navigator. If a range of dates isn't selected it will also reset the number
 * of days/weeks shown to the default (i.e. 1 day for the day view or 5 weeks
 * for the month view).
 */
static void
set_view (GnomeCalendar	*gcal, GnomeCalendarViewType view_type,
	  gboolean range_selected, gboolean grab_focus)
{
	GnomeCalendarPrivate *priv;
	gboolean round_selection;
	GtkWidget *focus_widget;
	const char *view_id;
	static gboolean updating = FALSE;

	if (updating)
		return;

	priv = gcal->priv;

	round_selection = FALSE;
	focus_widget = NULL;

	switch (view_type) {
	case GNOME_CAL_DAY_VIEW:
		view_id = "Day_View";
		focus_widget = priv->day_view;
		
		if (!range_selected)
			e_day_view_set_days_shown (E_DAY_VIEW (priv->day_view), 1);

		break;

	case GNOME_CAL_WORK_WEEK_VIEW:
		view_id = "Work_Week_View";
		focus_widget = priv->work_week_view;
		break;

	case GNOME_CAL_WEEK_VIEW:
		view_id = "Week_View";
		focus_widget = priv->week_view;
		round_selection = TRUE;
		break;

	case GNOME_CAL_MONTH_VIEW:
		view_id = "Month_View";
		focus_widget = priv->month_view;

		if (!range_selected)
			e_week_view_set_weeks_shown (E_WEEK_VIEW (priv->month_view), 5);

		round_selection = TRUE;
		break;

	default:
		g_warning ("A penguin is loose!");
		g_assert_not_reached ();
		return;
	}

	priv->current_view_type = view_type;
	priv->range_selected = range_selected;

	g_assert (focus_widget != NULL);

	calendar_config_set_default_view (view_type);

	updating = TRUE;
	gtk_notebook_set_page (GTK_NOTEBOOK (priv->notebook), (int) view_type);
	if (priv->view_instance)
		gal_view_instance_set_current_view_id (priv->view_instance, view_id);
	updating = FALSE;

	if (grab_focus)
		gtk_widget_grab_focus (focus_widget);

	gnome_calendar_set_pane_positions (gcal);

	/* For the week & month views we want the selection in the date
	   navigator to be rounded to the nearest week when the arrow buttons
	   are pressed to move to the previous/next month. */
	g_object_set (G_OBJECT (priv->date_navigator->calitem),
		      "round_selection_when_moving", round_selection,
		      NULL);
}

/**
 * gnome_calendar_set_view:
 * @gcal: A calendar.
 * @view_type: Type of view to show.
 * @range_selected: If false, the range of days/weeks shown will be reset to the
 * default value (1 for day view, 5 for week view, respectively).  If true, the
 * currently displayed range will be kept.
 * @grab_focus: Whether the view widget should grab the focus.
 *
 * Sets the view that should be shown in a calendar.  If @reset_range is true,
 * this function will automatically set the number of days or weeks shown in
 * the view; otherwise the last configuration will be kept.
 **/
void
gnome_calendar_set_view (GnomeCalendar *gcal, GnomeCalendarViewType view_type,
			 gboolean range_selected, gboolean grab_focus)
{
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	set_view (gcal, view_type, range_selected, grab_focus);
	gnome_calendar_update_view_times (gcal);
	gnome_calendar_update_date_navigator (gcal);
	gnome_calendar_notify_dates_shown_changed (gcal);
}

/* Callback used when the view collection asks us to display a particular view */
static void
display_view_cb (GalViewInstance *view_instance, GalView *view, gpointer data)
{
	GnomeCalendar *gcal;
	CalendarView *cal_view;

	gcal = GNOME_CALENDAR (data);

	if (!IS_CALENDAR_VIEW (view))
		g_error ("display_view_cb(): Unknown type of view for GnomeCalendar");

	cal_view = CALENDAR_VIEW (view);

	gnome_calendar_set_view (gcal, calendar_view_get_view_type (cal_view), FALSE, TRUE);
}

/**
 * gnome_calendar_setup_view_menus:
 * @gcal: A calendar.
 * @uic: UI controller to use for the menus.
 * 
 * Sets up the #GalView menus for a calendar.  This function should be called
 * from the Bonobo control activation callback for this calendar.  Also, the
 * menus should be discarded using gnome_calendar_discard_view_menus().
 **/
void
gnome_calendar_setup_view_menus (GnomeCalendar *gcal, BonoboUIComponent *uic)
{
	GnomeCalendarPrivate *priv;
	char *path;
	CalendarViewFactory *factory;
	static GalViewCollection *collection = NULL;

	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (uic != NULL);
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (uic));

	priv = gcal->priv;

	g_return_if_fail (priv->view_instance == NULL);

	g_assert (priv->view_instance == NULL);
	g_assert (priv->view_menus == NULL);

	/* Create the view instance */

	if (collection == NULL) {
		collection = gal_view_collection_new ();

		gal_view_collection_set_title (collection, _("Calendar"));

 		path = gnome_util_prepend_user_home ("/evolution/views/calendar/");
		gal_view_collection_set_storage_directories (collection,
							     EVOLUTION_GALVIEWSDIR "/calendar/",
							     path);
		g_free (path);

		/* Create the views */

		factory = calendar_view_factory_new (GNOME_CAL_DAY_VIEW);
		gal_view_collection_add_factory (collection, GAL_VIEW_FACTORY (factory));
		g_object_unref (factory);

		factory = calendar_view_factory_new (GNOME_CAL_WORK_WEEK_VIEW);
		gal_view_collection_add_factory (collection, GAL_VIEW_FACTORY (factory));
		g_object_unref (factory);

		factory = calendar_view_factory_new (GNOME_CAL_WEEK_VIEW);
		gal_view_collection_add_factory (collection, GAL_VIEW_FACTORY (factory));
		g_object_unref (factory);

		factory = calendar_view_factory_new (GNOME_CAL_MONTH_VIEW);
		gal_view_collection_add_factory (collection, GAL_VIEW_FACTORY (factory));
		g_object_unref (factory);

		/* Load the collection and create the menus */

		gal_view_collection_load (collection);
	}

	priv->view_instance = gal_view_instance_new (collection, cal_client_get_uri (priv->client));

	priv->view_menus = gal_view_menus_new (priv->view_instance);
	gal_view_menus_set_show_define_views (priv->view_menus, FALSE);
	gal_view_menus_apply (priv->view_menus, uic, NULL);
	gnome_calendar_set_view (gcal, priv->current_view_type, TRUE, FALSE);

	g_signal_connect (priv->view_instance, "display_view",
			  G_CALLBACK (display_view_cb), gcal);
	display_view_cb (priv->view_instance, gal_view_instance_get_current_view (priv->view_instance), gcal);
}

/**
 * gnome_calendar_discard_view_menus:
 * @gcal: A calendar.
 * 
 * Discards the #GalView menus used by a calendar.  This function should be
 * called from the Bonobo control deactivation callback for this calendar.  The
 * menus should have been set up with gnome_calendar_setup_view_menus().
 **/
void
gnome_calendar_discard_view_menus (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;

	g_return_if_fail (gcal != NULL);

	priv = gcal->priv;

	g_return_if_fail (priv->view_instance != NULL);

	g_assert (priv->view_instance != NULL);
	g_assert (priv->view_menus != NULL);

	g_object_unref (priv->view_instance);
	priv->view_instance = NULL;

	g_object_unref (priv->view_menus);
	priv->view_menus = NULL;
}

EPopupMenu *
gnome_calendar_setup_view_popup (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;

	g_return_val_if_fail (gcal != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), NULL);

	priv = gcal->priv;

	g_return_val_if_fail (priv->view_instance != NULL, NULL);

	return gal_view_instance_get_popup_menu (priv->view_instance);
}

void
gnome_calendar_discard_view_popup (GnomeCalendar *gcal, EPopupMenu *popup)
{


	GnomeCalendarPrivate *priv;

	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	priv = gcal->priv;

	g_return_if_fail (priv->view_instance != NULL);

	gal_view_instance_free_popup_menu (priv->view_instance, popup);
}

static void
gnome_calendar_set_pane_positions	(GnomeCalendar	*gcal)
{
	GnomeCalendarPrivate *priv;

	priv = gcal->priv;

	if (priv->current_view_type == GNOME_CAL_MONTH_VIEW && !priv->range_selected) {
		gtk_paned_set_position (GTK_PANED (priv->hpane), priv->hpane_pos_month_view);
		gtk_paned_set_position (GTK_PANED (priv->vpane), priv->vpane_pos_month_view);
	} else {
		gtk_paned_set_position (GTK_PANED (priv->hpane), priv->hpane_pos);
		gtk_paned_set_position (GTK_PANED (priv->vpane), priv->vpane_pos);
	}
}

/* Displays an error to indicate that opening a calendar failed */
static void
open_error (GnomeCalendar *gcal, const char *uri)
{
	char *msg;
	char *urinopwd;

	urinopwd = get_uri_without_password (uri);
	msg = g_strdup_printf (_("Could not open the folder in `%s'"), urinopwd);
	gnome_error_dialog_parented (msg, GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (gcal))));
	g_free (msg);
	g_free (urinopwd);
}

/* Displays an error to indicate that the specified URI method is not supported */
static void
method_error (GnomeCalendar *gcal, const char *uri)
{
	char *msg;
	char *urinopwd;

	urinopwd = get_uri_without_password (uri);
	msg = g_strdup_printf (_("The method required to open `%s' is not supported"), urinopwd);
	gnome_error_dialog_parented (msg, GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (gcal))));
	g_free (msg);
	g_free (urinopwd);
}

/* Displays an error to indicate permission problems */
static void
permission_error (GnomeCalendar *gcal, const char *uri)
{
	char *msg;
	char *urinopwd;

	urinopwd = get_uri_without_password (uri);
	msg = g_strdup_printf (_("You don't have permission to open the folder in `%s'"), urinopwd);
	gnome_error_dialog_parented (msg, GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (gcal))));
	g_free (msg);
	g_free (urinopwd);
}

/* Callback from the calendar client when a calendar is loaded */
static gboolean
update_query_timeout (gpointer data)
{
	GnomeCalendar *gcal = data;
	GnomeCalendarPrivate *priv;

	gcal = GNOME_CALENDAR (data);
	priv = gcal->priv;

	update_query (gcal);
	priv->query_timeout = 0;

	return FALSE;
}

static void
client_cal_opened_cb (CalClient *client, CalClientOpenStatus status, gpointer data)
{
	GnomeCalendar *gcal;
	GnomeCalendarPrivate *priv;
	char *msg;
	char *uristr;

	gcal = GNOME_CALENDAR (data);
	priv = gcal->priv;

	switch (status) {
	case CAL_CLIENT_OPEN_SUCCESS:
		/* If this is the main CalClient, update the Date Navigator. */
		if (client == priv->client) {
			priv->query_timeout = g_timeout_add (100, update_query_timeout, gcal);
		}

		/* Set the client's default timezone, if we have one. */
		if (priv->zone) {
			cal_client_set_default_timezone (client, priv->zone);
		}

		/* add the alarms for this client */
		uristr = get_uri_without_password (cal_client_get_uri (client));
		msg = g_strdup_printf (_("Adding alarms for %s"), uristr);
		g_free (uristr);
		if (client == priv->client) {
			e_week_view_set_status_message (E_WEEK_VIEW (priv->week_view), msg);
		}
		else if (client == priv->task_pad_client) {
			calendar_model_set_status_message (
				e_calendar_table_get_model (E_CALENDAR_TABLE (priv->todo)), msg);
		}
		g_free (msg);

		add_alarms (cal_client_get_uri (client));
		break;

	case CAL_CLIENT_OPEN_ERROR:
		open_error (gcal, cal_client_get_uri (client));
		break;

	case CAL_CLIENT_OPEN_NOT_FOUND:
		/* bullshit; we did not specify only_if_exists */
		g_assert_not_reached ();
		return;

	case CAL_CLIENT_OPEN_METHOD_NOT_SUPPORTED:
		method_error (gcal, cal_client_get_uri (client));
		break;

	case CAL_CLIENT_OPEN_PERMISSION_DENIED :
		permission_error (gcal, cal_client_get_uri (client));
		break;

	default:
		g_assert_not_reached ();
		return;
	}

	if (client == priv->client) {
		e_week_view_set_status_message (E_WEEK_VIEW (priv->week_view), NULL);
	}
	else if (client == priv->task_pad_client) {
		calendar_model_set_status_message (
			e_calendar_table_get_model (E_CALENDAR_TABLE (priv->todo)), NULL);
	}
}

/* Duplicates an array of categories */
static GPtrArray *
copy_categories (GPtrArray *categories)
{
	GPtrArray *c;
	int i;

	c = g_ptr_array_new ();
	g_ptr_array_set_size (c, categories->len);

	for (i = 0; i < categories->len; i++)
		c->pdata[i] = g_strdup (categories->pdata[i]);

	return c;
}

/* Adds the categories from an array to a hash table if they don't exist there
 * already.
 */
static void
add_categories (GHashTable *categories, GPtrArray *c)
{
	int i;

	if (!c)
		return;

	for (i = 0; i < c->len; i++) {
		const char *cat;
		const char *str;

		cat = c->pdata[i];
		str = g_hash_table_lookup (categories, cat);

		if (!str)
			g_hash_table_insert (categories, (char *) cat, NULL);
	}
}

/* Used to append categories from a hash table to an array */
struct append_category_closure {
	GPtrArray *c;

	int i;
};

/* Appends a category from the hash table to the array */
static void
append_category_cb (gpointer key, gpointer value, gpointer data)
{
	struct append_category_closure *closure;
	const char *category;

	category = key;
	closure = data;

	closure->c->pdata[closure->i] = g_strdup (category);
	closure->i++;
}

/* Creates the union of two sets of categories */
static GPtrArray *
merge_categories (GPtrArray *a, GPtrArray *b)
{
	GHashTable *categories;
	int n;
	GPtrArray *c;
	struct append_category_closure closure;

	categories = g_hash_table_new (g_str_hash, g_str_equal);

	add_categories (categories, a);
	add_categories (categories, b);

	n = g_hash_table_size (categories);

	c = g_ptr_array_new ();
	g_ptr_array_set_size (c, n);

	closure.c = c;
	closure.i = 0;
	g_hash_table_foreach (categories, append_category_cb, &closure);
	g_hash_table_destroy (categories);

	return c;
}

/* Callback from the calendar client when the set of categories changes.  We
 * have to merge the categories of the calendar and tasks clients.
 */
static void
client_categories_changed_cb (CalClient *client, GPtrArray *categories, gpointer data)
{
	GnomeCalendar *gcal;
	GnomeCalendarPrivate *priv;
	GPtrArray *merged;

	gcal = GNOME_CALENDAR (data);
	priv = gcal->priv;

	if (client == priv->client) {
		free_categories (priv->cal_categories);
		priv->cal_categories = copy_categories (categories);
	} else if (client == priv->task_pad_client) {
		free_categories (priv->tasks_categories);
		priv->tasks_categories = copy_categories (categories);
	} else
		g_assert_not_reached ();

	merged = merge_categories (priv->cal_categories, priv->tasks_categories);
	cal_search_bar_set_categories (CAL_SEARCH_BAR (priv->search_bar), merged);
	free_categories (merged);
}

/* Callback when we get an error message from the backend */
static void
backend_error_cb (CalClient *client, const char *message, gpointer data)
{
	GnomeCalendar *gcal;
	GnomeCalendarPrivate *priv;
	char *errmsg;
	char *uristr;

	gcal = GNOME_CALENDAR (data);
	priv = gcal->priv;

	uristr = get_uri_without_password (cal_client_get_uri (client));
	errmsg = g_strdup_printf (_("Error on %s:\n %s"), uristr, message);
	gnome_error_dialog_parented (errmsg, GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (gcal))));
	g_free (errmsg);
	g_free (uristr);
}

/* Callback when the backend dies */
static void
backend_died_cb (CalClient *client, gpointer data)
{
	GnomeCalendar *gcal;
	GnomeCalendarPrivate *priv;
	char *message;
	char *uristr;

	gcal = GNOME_CALENDAR (data);
	priv = gcal->priv;

	uristr = get_uri_without_password (cal_client_get_uri (priv->client));
	if (client == priv->client) {
		message = g_strdup_printf (_("The calendar backend for\n%s\n has crashed. "
					     "You will have to restart Evolution in order "
					     "to use it again"),
					   uristr);
		e_day_view_set_status_message (E_DAY_VIEW (priv->day_view), NULL);
		e_day_view_set_status_message (E_DAY_VIEW (priv->work_week_view), NULL);
		e_week_view_set_status_message (E_WEEK_VIEW (priv->week_view), NULL);
		e_week_view_set_status_message (E_WEEK_VIEW (priv->month_view), NULL);
	} else if (client == priv->task_pad_client) {
		message = g_strdup_printf (_("The task backend for\n%s\n has crashed. "
					     "You will have to restart Evolution in order "
					     "to use it again"),
					   uristr);
		calendar_model_set_status_message (
			e_calendar_table_get_model (E_CALENDAR_TABLE (priv->todo)), NULL);
	} else {
		message = NULL;
		g_assert_not_reached ();
	}

	gnome_error_dialog_parented (message, GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (gcal))));
	g_free (message);
	g_free (uristr);
}

GtkWidget *
gnome_calendar_construct (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	GnomeCalendarViewType view_type;
	CalendarModel *model;

	g_return_val_if_fail (gcal != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), NULL);

	priv = gcal->priv;

	/*
	 * Calendar Folder Client.
	 */
	priv->client = cal_client_new ();
	if (!priv->client)
		return NULL;

	g_signal_connect (priv->client, "cal_opened",
			  G_CALLBACK (client_cal_opened_cb), gcal);
	g_signal_connect (priv->client, "backend_error",
			  G_CALLBACK (backend_error_cb), gcal);
	g_signal_connect (priv->client, "categories_changed",
			  G_CALLBACK (client_categories_changed_cb), gcal);
	g_signal_connect (priv->client, "backend_died",
			  G_CALLBACK (backend_died_cb), gcal);

	e_day_view_set_cal_client (E_DAY_VIEW (priv->day_view),
				   priv->client);
	e_day_view_set_cal_client (E_DAY_VIEW (priv->work_week_view),
				   priv->client);
	e_week_view_set_cal_client (E_WEEK_VIEW (priv->week_view),
				    priv->client);
	e_week_view_set_cal_client (E_WEEK_VIEW (priv->month_view),
				    priv->client);

	/*
	 * TaskPad Folder Client.
	 */
	priv->task_pad_client = cal_client_new ();
	if (!priv->task_pad_client)
		return NULL;

	g_signal_connect (priv->task_pad_client, "cal_opened",
			  G_CALLBACK (client_cal_opened_cb), gcal);	
	g_signal_connect (priv->task_pad_client, "backend_error",
			  G_CALLBACK (backend_error_cb), gcal);
	g_signal_connect (priv->task_pad_client, "categories_changed",
			  G_CALLBACK (client_categories_changed_cb), gcal);
	g_signal_connect (priv->task_pad_client, "backend_died",
			  G_CALLBACK (backend_died_cb), gcal);

	model = e_calendar_table_get_model (E_CALENDAR_TABLE (priv->todo));
	g_assert (model != NULL);

	calendar_model_set_cal_client (model, priv->task_pad_client, CALOBJ_TYPE_TODO);

	/* Get the default view to show. */
	view_type = calendar_config_get_default_view ();
	if (view_type < GNOME_CAL_DAY_VIEW || view_type > GNOME_CAL_MONTH_VIEW)
		view_type = GNOME_CAL_DAY_VIEW;

	gnome_calendar_set_view (gcal, view_type, FALSE, FALSE);

	return GTK_WIDGET (gcal);
}

GtkWidget *
gnome_calendar_new (void)
{
	GnomeCalendar *gcal;

	gcal = g_object_new (gnome_calendar_get_type (), NULL);

	if (!gnome_calendar_construct (gcal)) {
		g_message ("gnome_calendar_new(): Could not construct the calendar GUI");
		g_object_unref (gcal);
		return NULL;
	}

	return GTK_WIDGET (gcal);
}

void
gnome_calendar_set_ui_component (GnomeCalendar *gcal,
				 BonoboUIComponent *ui_component)
{
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (ui_component == NULL || BONOBO_IS_UI_COMPONENT (ui_component));

	e_search_bar_set_ui_component (E_SEARCH_BAR (gcal->priv->search_bar), ui_component);
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

/**
 * gnome_calendar_get_task_pad_cal_client:
 * @gcal: A calendar view.
 *
 * Queries the calendar client interface object that a calendar view is using
 * for the Task Pad.
 *
 * Return value: A calendar client interface object.
 **/
CalClient *
gnome_calendar_get_task_pad_cal_client (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;

	g_return_val_if_fail (gcal != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), NULL);

	priv = gcal->priv;

	return priv->task_pad_client;
}

/* Adds the specified URI to the alarm notification service */
static void
add_alarms (const char *uri)
{
	CORBA_Environment ev;
	GNOME_Evolution_Calendar_AlarmNotify an;

	/* Activate the alarm notification service */

	CORBA_exception_init (&ev);
	an = bonobo_activation_activate_from_id ("OAFIID:GNOME_Evolution_Calendar_AlarmNotify", 0, NULL, &ev);

	if (BONOBO_EX (&ev)) {
		g_warning ("add_alarms(): Could not activate the alarm notification service: %s",
			   CORBA_exception_id (&ev));
		CORBA_exception_free (&ev);
		return;
	}
	CORBA_exception_free (&ev);

	/* Ask the service to load the URI */

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_AlarmNotify_addCalendar (an, uri, &ev);

	if (BONOBO_USER_EX (&ev, ex_GNOME_Evolution_Calendar_AlarmNotify_InvalidURI))
		g_message ("add_calendar(): Invalid URI reported from the "
			   "alarm notification service");
	else if (BONOBO_USER_EX (&ev, ex_GNOME_Evolution_Calendar_AlarmNotify_BackendContactError))
		g_message ("add_calendar(): The alarm notification service could "
			   "not contact the backend");		
	else if (BONOBO_EX (&ev))
		g_message ("add_calendar(): Could not issue the addCalendar request");

	CORBA_exception_free (&ev);

	/* Get rid of the service */

	CORBA_exception_init (&ev);
	bonobo_object_release_unref (an, &ev);
	if (BONOBO_EX (&ev))
		g_message ("add_alarms(): Could not unref the alarm notification service");

	CORBA_exception_free (&ev);
}

gboolean
gnome_calendar_open (GnomeCalendar *gcal, const char *str_uri)
{
	GnomeCalendarPrivate *priv;
	char *tasks_uri;
	gboolean success;
	EUri *uri;
	char *message;
	char *real_uri;
	char *urinopwd;

	g_return_val_if_fail (gcal != NULL, FALSE);
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), FALSE);
	g_return_val_if_fail (str_uri != NULL, FALSE);

	priv = gcal->priv;

	g_return_val_if_fail (
		cal_client_get_load_state (priv->client) == CAL_CLIENT_LOAD_NOT_LOADED,
		FALSE);

	g_return_val_if_fail (
		cal_client_get_load_state (priv->task_pad_client) == CAL_CLIENT_LOAD_NOT_LOADED,
		FALSE);

	uri = e_uri_new (str_uri);
	if (!uri || !g_strncasecmp (uri->protocol, "file", 4))
		real_uri = g_concat_dir_and_file (str_uri, "calendar.ics");
	else
		real_uri = g_strdup (str_uri);

	urinopwd = get_uri_without_password (real_uri);
	message = g_strdup_printf (_("Opening calendar at %s"), urinopwd);
	g_free (urinopwd);
	e_week_view_set_status_message (E_WEEK_VIEW (priv->week_view), message);
	g_free (message);

	if (!cal_client_open_calendar (priv->client, real_uri, FALSE)) {
		g_message ("gnome_calendar_open(): Could not issue the request to open the calendar folder");
		g_free (real_uri);
		e_uri_free (uri);
		e_week_view_set_status_message (E_WEEK_VIEW (priv->week_view), NULL);

		return FALSE;
	}

	/* Open the appropriate Tasks folder to show in the TaskPad */

	if (!uri) {
		tasks_uri = g_strdup_printf ("%s/local/Tasks/tasks.ics", evolution_dir);
		message = g_strdup_printf (_("Opening tasks at %s"), tasks_uri);
		calendar_model_set_status_message (
			e_calendar_table_get_model (E_CALENDAR_TABLE (priv->todo)), message);
		g_free (message);

		success = cal_client_open_calendar (priv->task_pad_client, tasks_uri, FALSE);
		g_free (tasks_uri);
	}
	else {
		if (!g_strncasecmp (uri->protocol, "file", 4)) {
			tasks_uri = g_strdup_printf ("%s/local/Tasks/tasks.ics", evolution_dir);
			message = g_strdup_printf (_("Opening tasks at %s"), tasks_uri);
			calendar_model_set_status_message (
				e_calendar_table_get_model (E_CALENDAR_TABLE (priv->todo)), message);
			g_free (message);

			success = cal_client_open_calendar (priv->task_pad_client, tasks_uri, FALSE);
			g_free (tasks_uri);
		}
		else {
			calendar_model_set_status_message (
				e_calendar_table_get_model (E_CALENDAR_TABLE (priv->todo)),
				_("Opening default tasks folder"));
			success = cal_client_open_default_tasks (priv->task_pad_client, FALSE);
		}
	}

	g_free (real_uri);
	e_uri_free (uri);

	if (!success) {
		g_message ("gnome_calendar_open(): Could not issue the request to open the tasks folder");
		calendar_model_set_status_message (
			e_calendar_table_get_model (E_CALENDAR_TABLE (priv->todo)), NULL);
		return FALSE;
	}

	return TRUE;
}

/* Tells the calendar to reload all config settings.
   If initializing is TRUE it sets the pane positions as well. (We don't
   want to reset the pane positions after the user clicks 'Apply' in the
   preferences dialog.) */
void
gnome_calendar_update_config_settings (GnomeCalendar *gcal,
				       gboolean	      initializing)
{
	GnomeCalendarPrivate *priv;
	CalWeekdays working_days;
	gint week_start_day, time_divisions;
	gint start_hour, start_minute, end_hour, end_minute;
	gboolean use_24_hour, show_event_end, compress_weekend;
	char *location;

	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	priv = gcal->priv;

	working_days = calendar_config_get_working_days ();
	/* CalWeekdays and EDayViewDays use the same bit-masks, so we can
	   use the same value. */
	e_day_view_set_working_days (E_DAY_VIEW (priv->day_view),
				     (EDayViewDays) working_days);
	e_day_view_set_working_days (E_DAY_VIEW (priv->work_week_view),
				     (EDayViewDays) working_days);

	/* Note that this is 0 (Sun) to 6 (Sat). */
	week_start_day = calendar_config_get_week_start_day ();

	/* Convert it to 0 (Mon) to 6 (Sun), which is what we use. */
	week_start_day = (week_start_day + 6) % 7;

	e_day_view_set_week_start_day (E_DAY_VIEW (priv->day_view),
				       week_start_day);
	e_day_view_set_week_start_day (E_DAY_VIEW (priv->work_week_view),
				       week_start_day);
	e_week_view_set_week_start_day (E_WEEK_VIEW (priv->week_view),
					week_start_day);
	e_week_view_set_week_start_day (E_WEEK_VIEW (priv->month_view),
					week_start_day);

	start_hour = calendar_config_get_day_start_hour ();
	start_minute = calendar_config_get_day_start_minute ();
	end_hour = calendar_config_get_day_end_hour ();
	end_minute = calendar_config_get_day_end_minute ();
	e_day_view_set_working_day (E_DAY_VIEW (priv->day_view),
				    start_hour, start_minute,
				    end_hour, end_minute);
	e_day_view_set_working_day (E_DAY_VIEW (priv->work_week_view),
				    start_hour, start_minute,
				    end_hour, end_minute);

	use_24_hour = calendar_config_get_24_hour_format ();
	e_day_view_set_24_hour_format (E_DAY_VIEW (priv->day_view),
				       use_24_hour);
	e_day_view_set_24_hour_format (E_DAY_VIEW (priv->work_week_view),
				       use_24_hour);
	e_week_view_set_24_hour_format (E_WEEK_VIEW (priv->week_view),
					use_24_hour);
	e_week_view_set_24_hour_format (E_WEEK_VIEW (priv->month_view),
					use_24_hour);

	time_divisions = calendar_config_get_time_divisions ();
	e_day_view_set_mins_per_row (E_DAY_VIEW (priv->day_view),
				     time_divisions);
	e_day_view_set_mins_per_row (E_DAY_VIEW (priv->work_week_view),
				     time_divisions);

	show_event_end = calendar_config_get_show_event_end ();
	e_day_view_set_show_event_end_times (E_DAY_VIEW (priv->day_view),
					     show_event_end);
	e_day_view_set_show_event_end_times (E_DAY_VIEW (priv->work_week_view),
					     show_event_end);
	e_week_view_set_show_event_end_times (E_WEEK_VIEW (priv->week_view),
					      show_event_end);
	e_week_view_set_show_event_end_times (E_WEEK_VIEW (priv->month_view),
					      show_event_end);

	compress_weekend = calendar_config_get_compress_weekend ();
	e_week_view_set_compress_weekend (E_WEEK_VIEW (priv->month_view),
					  compress_weekend);

	calendar_config_configure_e_calendar (E_CALENDAR (priv->date_navigator));

	calendar_config_configure_e_calendar_table (E_CALENDAR_TABLE (priv->todo));

	location = calendar_config_get_timezone ();
	priv->zone = icaltimezone_get_builtin_timezone (location);

	if (priv->client
	    && cal_client_get_load_state (priv->client) == CAL_CLIENT_LOAD_LOADED) {
		cal_client_set_default_timezone (priv->client, priv->zone);
	}
	if (priv->task_pad_client
	    && cal_client_get_load_state (priv->task_pad_client) == CAL_CLIENT_LOAD_LOADED) {
		cal_client_set_default_timezone (priv->task_pad_client,
						 priv->zone);
	}

	e_day_view_set_timezone (E_DAY_VIEW (priv->day_view), priv->zone);
	e_day_view_set_timezone (E_DAY_VIEW (priv->work_week_view), priv->zone);
	e_week_view_set_timezone (E_WEEK_VIEW (priv->week_view), priv->zone);
	e_week_view_set_timezone (E_WEEK_VIEW (priv->month_view), priv->zone);

	if (initializing) {
		priv->hpane_pos = calendar_config_get_hpane_pos ();
		priv->vpane_pos = calendar_config_get_vpane_pos ();
		priv->hpane_pos_month_view = calendar_config_get_month_hpane_pos ();
		priv->vpane_pos_month_view = calendar_config_get_month_vpane_pos ();
	}

	/* The range of days shown may have changed, so we update the date
	   navigator if needed. */
	gnome_calendar_update_date_navigator (gcal);
	gnome_calendar_notify_dates_shown_changed (gcal);
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
	gnome_calendar_notify_dates_shown_changed (gcal);
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

void
gnome_calendar_edit_object (GnomeCalendar *gcal, CalComponent *comp, 
			    gboolean meeting)
{
	GnomeCalendarPrivate *priv;
	CompEditor *ce;
	const char *uid;

	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (comp != NULL);

	priv = gcal->priv;

	cal_component_get_uid (comp, &uid);

	ce = e_comp_editor_registry_find (comp_editor_registry, uid);
	if (!ce) {
		EventEditor *ee;

		ee = event_editor_new (priv->client);
		if (!ee) {
			g_message ("gnome_calendar_edit_object(): Could not create the event editor");
			return;
		}
		ce = COMP_EDITOR (ee);
		
		comp_editor_edit_comp (ce, comp);
		if (meeting)
			event_editor_show_meeting (ee);

		e_comp_editor_registry_add (comp_editor_registry, ce, FALSE);
	}

	comp_editor_focus (ce);
}

/**
 * gnome_calendar_new_appointment_for:
 * @gcal: An Evolution calendar.
 * @dtstart: a Unix time_t that marks the beginning of the appointment.
 * @dtend: a Unix time_t that marks the end of the appointment.
 * @all_day: if true, the dtstart and dtend are expanded to cover the entire
 * day, and the event is set to TRANSPARENT.
 *
 * Opens an event editor dialog for a new appointment.
 *
 **/
void
gnome_calendar_new_appointment_for (GnomeCalendar *cal,
				    time_t dtstart, time_t dtend,
				    gboolean all_day,
				    gboolean meeting)
{
	GnomeCalendarPrivate *priv;
	struct icaltimetype itt;
	CalComponentDateTime dt;
	CalComponent *comp;
	CalComponentTransparency transparency;
	const char *category;

	g_return_if_fail (cal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (cal));

	priv = cal->priv;

	dt.value = &itt;
	if (all_day)
		dt.tzid = NULL;
	else
		dt.tzid = icaltimezone_get_tzid (priv->zone);

	comp = cal_comp_event_new_with_defaults (priv->client);

	/* DTSTART, DTEND */

	itt = icaltime_from_timet_with_zone (dtstart, FALSE, priv->zone);
	if (all_day) {
		itt.hour = itt.minute = itt.second = 0;
		itt.is_date = TRUE;
	}
	cal_component_set_dtstart (comp, &dt);

	itt = icaltime_from_timet_with_zone (dtend, FALSE, priv->zone);
	if (all_day) {
		/* We round it up to the end of the day, unless it is already
		   set to midnight. */
		if (itt.hour != 0 || itt.minute != 0 || itt.second != 0) {
			icaltime_adjust (&itt, 1, 0, 0, 0);
		}
		itt.hour = itt.minute = itt.second = 0;
		itt.is_date = TRUE;
	}
	cal_component_set_dtend (comp, &dt);

	transparency = all_day ? CAL_COMPONENT_TRANSP_TRANSPARENT
		: CAL_COMPONENT_TRANSP_OPAQUE;
	cal_component_set_transparency (comp, transparency);


	/* Category */

	category = cal_search_bar_get_category (CAL_SEARCH_BAR (priv->search_bar));
	cal_component_set_categories (comp, category);

	/* Edit! */

	cal_component_commit_sequence (comp);

	gnome_calendar_edit_object (cal, comp, meeting);
	g_object_unref (comp);
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
	time_t dtstart, dtend;

	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	gnome_calendar_get_current_time_range (gcal, &dtstart, &dtend);
	gnome_calendar_new_appointment_for (gcal, dtstart, dtend, FALSE, FALSE);
}

/**
 * gnome_calendar_new_task:
 * @gcal: An Evolution calendar.
 *
 * Opens a task editor dialog for a new task.
 **/
void
gnome_calendar_new_task		(GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	TaskEditor *tedit;
	CalComponent *comp;
	const char *category;

	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	priv = gcal->priv;

	tedit = task_editor_new (priv->task_pad_client);

	comp = cal_comp_task_new_with_defaults (priv->client);

	category = cal_search_bar_get_category (CAL_SEARCH_BAR (priv->search_bar));
	cal_component_set_categories (comp, category);

	comp_editor_edit_comp (COMP_EDITOR (tedit), comp);
	g_object_unref (comp);

	comp_editor_focus (COMP_EDITOR (tedit));
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

	priv = gcal->priv;

	switch (priv->current_view_type) {
	case GNOME_CAL_DAY_VIEW:
		e_day_view_get_selected_time_range (E_DAY_VIEW (priv->day_view),
						    start_time, end_time);
		break;

	case GNOME_CAL_WORK_WEEK_VIEW:
		e_day_view_get_selected_time_range (E_DAY_VIEW (priv->work_week_view),
						    start_time, end_time);
		break;

	case GNOME_CAL_WEEK_VIEW:
		e_week_view_get_selected_time_range (E_WEEK_VIEW (priv->week_view),
						     start_time, end_time);
		break;

	case GNOME_CAL_MONTH_VIEW:
		e_week_view_get_selected_time_range (E_WEEK_VIEW (priv->month_view),
						     start_time, end_time);
		break;

	default:
		g_message ("My penguin is gone!");
		g_assert_not_reached ();
	}
}


/* Gets the visible time range for the current view. Returns FALSE if no
   time range has been set yet. */
gboolean
gnome_calendar_get_visible_time_range (GnomeCalendar *gcal,
				       time_t	 *start_time,
				       time_t	 *end_time)
{
	GnomeCalendarPrivate *priv;
	gboolean retval = FALSE;

	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), FALSE);

	priv = gcal->priv;

	switch (priv->current_view_type) {
	case GNOME_CAL_DAY_VIEW:
		retval = e_day_view_get_visible_time_range (E_DAY_VIEW (priv->day_view), start_time, end_time);
		break;

	case GNOME_CAL_WORK_WEEK_VIEW:
		retval = e_day_view_get_visible_time_range (E_DAY_VIEW (priv->work_week_view), start_time, end_time);
		break;

	case GNOME_CAL_WEEK_VIEW:
		retval = e_week_view_get_visible_time_range (E_WEEK_VIEW (priv->week_view), start_time, end_time);
		break;

	case GNOME_CAL_MONTH_VIEW:
		retval = e_week_view_get_visible_time_range (E_WEEK_VIEW (priv->month_view), start_time, end_time);
		break;

	default:
		g_assert_not_reached ();
	}

	return retval;
}



static void
get_days_shown (GnomeCalendar *gcal, GDate *start_date, gint *days_shown)
{
	GnomeCalendarPrivate *priv;

	priv = gcal->priv;

	switch (priv->current_view_type) {
	case GNOME_CAL_DAY_VIEW:
		time_to_gdate_with_zone (start_date,
					 E_DAY_VIEW (priv->day_view)->lower,
					 priv->zone);
		*days_shown = e_day_view_get_days_shown (E_DAY_VIEW (priv->day_view));
		break;

	case GNOME_CAL_WORK_WEEK_VIEW:
		time_to_gdate_with_zone (start_date,
					 E_DAY_VIEW (priv->work_week_view)->lower,
					 priv->zone);
		*days_shown = e_day_view_get_days_shown (E_DAY_VIEW (priv->work_week_view));
		break;

	case GNOME_CAL_WEEK_VIEW:
		*start_date = E_WEEK_VIEW (priv->week_view)->first_day_shown;
		if (e_week_view_get_multi_week_view (E_WEEK_VIEW (priv->week_view)))
			*days_shown = e_week_view_get_weeks_shown (
				E_WEEK_VIEW (priv->week_view)) * 7;
		else
			*days_shown = 7;

		break;

	case GNOME_CAL_MONTH_VIEW:
		*start_date = E_WEEK_VIEW (priv->month_view)->first_day_shown;
		if (e_week_view_get_multi_week_view (E_WEEK_VIEW (priv->month_view)))
			*days_shown = e_week_view_get_weeks_shown (
				E_WEEK_VIEW (priv->month_view)) * 7;
		else
			*days_shown = 7;

		break;

	default:
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

	get_days_shown (gcal, &start_date, &days_shown);

	end_date = start_date;
	g_date_add_days (&end_date, days_shown - 1);

	e_calendar_item_set_selection (priv->date_navigator->calitem,
				       &start_date, &end_date);
}


static void
gnome_calendar_on_date_navigator_selection_changed (ECalendarItem    *calitem,
						    GnomeCalendar    *gcal)
{
	GnomeCalendarPrivate *priv;
	GDate start_date, end_date, new_start_date, new_end_date;
	gint days_shown, new_days_shown;
	gboolean starts_on_week_start_day;
	struct icaltimetype tt;

	priv = gcal->priv;

	starts_on_week_start_day = FALSE;

	get_days_shown (gcal, &start_date, &days_shown);

	end_date = start_date;
	g_date_add_days (&end_date, days_shown - 1);

	e_calendar_item_get_selection (calitem, &new_start_date, &new_end_date);

	/* If the selection hasn't changed just return. */
	if (!g_date_compare (&start_date, &new_start_date)
	    && !g_date_compare (&end_date, &new_end_date))
		return;

	new_days_shown = g_date_julian (&new_end_date) - g_date_julian (&new_start_date) + 1;

	/* If a complete week is selected we show the Week view.
	   Note that if weekends are compressed and the week start day is set
	   to Sunday we don't actually show complete weeks in the Week view,
	   so this may need tweaking. */
	if (g_date_weekday (&new_start_date) % 7 == calendar_config_get_week_start_day ())
		starts_on_week_start_day = TRUE;

	/* Update selection to be in the new time range */
	tt = icaltime_null_time ();
	tt.year = g_date_year (&new_start_date);
	tt.month  = g_date_month (&new_start_date);
	tt.day = g_date_day (&new_start_date);
	priv->selection_start_time = icaltime_as_timet_with_zone (tt, priv->zone);
	icaltime_adjust (&tt, 1, 0, 0, 0);
	priv->selection_end_time = icaltime_as_timet_with_zone (tt, priv->zone);

	/* Switch views as appropriate, and change the number of days or weeks
	   shown. */
	if (new_days_shown > 9) {
		e_week_view_set_weeks_shown (E_WEEK_VIEW (priv->month_view),
					     (new_days_shown + 6) / 7);
		e_week_view_set_first_day_shown (E_WEEK_VIEW (priv->month_view), &new_start_date);

		set_view (gcal, GNOME_CAL_MONTH_VIEW, TRUE, FALSE);
		gnome_calendar_update_date_navigator (gcal);
		gnome_calendar_notify_dates_shown_changed (gcal);
	} else if (new_days_shown == 7 && starts_on_week_start_day) {
		e_week_view_set_first_day_shown (E_WEEK_VIEW (priv->week_view), &new_start_date);

		set_view (gcal, GNOME_CAL_WEEK_VIEW, TRUE, FALSE);
		gnome_calendar_update_date_navigator (gcal);
		gnome_calendar_notify_dates_shown_changed (gcal);
	} else {		
		e_day_view_set_days_shown (E_DAY_VIEW (priv->day_view), new_days_shown);
		
		if (new_days_shown == 5 && priv->current_view_type == GNOME_CAL_WORK_WEEK_VIEW)
			gnome_calendar_set_view (gcal, GNOME_CAL_WORK_WEEK_VIEW, TRUE, FALSE);
		else
			gnome_calendar_set_view (gcal, GNOME_CAL_DAY_VIEW, TRUE, FALSE);

	}

	focus_current_view (gcal);
}


static void
gnome_calendar_on_date_navigator_date_range_changed (ECalendarItem *calitem,
						     GnomeCalendar *gcal)
{
	update_query (gcal);
}

static void
gnome_calendar_hpane_realized (GtkWidget *w, GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;

	priv = gcal->priv;

	if (priv->current_view_type == GNOME_CAL_MONTH_VIEW && !priv->range_selected) {
		gtk_paned_set_position (GTK_PANED (priv->hpane), priv->hpane_pos_month_view);
	} else {
		gtk_paned_set_position (GTK_PANED (priv->hpane), priv->hpane_pos);
	}
}

static void
gnome_calendar_vpane_realized (GtkWidget *w, GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;

	priv = gcal->priv;

	if (priv->current_view_type == GNOME_CAL_MONTH_VIEW && !priv->range_selected) {
		gtk_paned_set_position (GTK_PANED (priv->vpane), priv->vpane_pos_month_view);
	} else {
		gtk_paned_set_position (GTK_PANED (priv->vpane), priv->vpane_pos);
	}
}

static gboolean
gnome_calendar_vpane_resized (GtkWidget *w, GdkEventButton *e, GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;

	priv = gcal->priv;

	if (priv->current_view_type == GNOME_CAL_MONTH_VIEW && !priv->range_selected) {
		priv->vpane_pos_month_view = gtk_paned_get_position (GTK_PANED (priv->vpane));
		calendar_config_set_month_vpane_pos (priv->vpane_pos_month_view);
	} else {
		priv->vpane_pos = gtk_paned_get_position (GTK_PANED (priv->vpane));
		calendar_config_set_vpane_pos (priv->vpane_pos);
	}

	return FALSE;
}

static gboolean
gnome_calendar_hpane_resized (GtkWidget *w, GdkEventButton *e, GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	gint times_width;

	priv = gcal->priv;

	if (priv->current_view_type == GNOME_CAL_MONTH_VIEW && !priv->range_selected) {
		priv->hpane_pos_month_view = gtk_paned_get_position (GTK_PANED (priv->hpane));
		calendar_config_set_month_hpane_pos (priv->hpane_pos_month_view);
	} else {
		priv->hpane_pos = gtk_paned_get_position (GTK_PANED (priv->hpane));
		calendar_config_set_hpane_pos (priv->hpane_pos);
	}

	/* adjust the size of the EDayView's time column */
	times_width = e_day_view_time_item_get_column_width (
		E_DAY_VIEW_TIME_ITEM (E_DAY_VIEW (priv->day_view)->time_canvas_item));
	if (times_width < priv->hpane_pos - 20)
		gtk_widget_set_usize (E_DAY_VIEW (priv->day_view)->time_canvas, times_width, -1);
	else
		gtk_widget_set_usize (E_DAY_VIEW (priv->day_view)->time_canvas, priv->hpane_pos - 20, -1);
	
	
	return FALSE;
}

void
gnome_calendar_cut_clipboard (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	FocusLocation location;

	priv = gcal->priv;

	location = get_focus_location (gcal);

	if (location == FOCUS_CALENDAR) {
		switch (priv->current_view_type) {
		case GNOME_CAL_DAY_VIEW :
			e_day_view_cut_clipboard (E_DAY_VIEW (priv->day_view));
			break;
		case GNOME_CAL_WORK_WEEK_VIEW :
			e_day_view_cut_clipboard (E_DAY_VIEW (priv->work_week_view));
			break;
		case GNOME_CAL_WEEK_VIEW :
			e_week_view_cut_clipboard (E_WEEK_VIEW (priv->week_view));
			break;
		case GNOME_CAL_MONTH_VIEW :
			e_week_view_cut_clipboard (E_WEEK_VIEW (priv->month_view));
			break;
		default:
			g_assert_not_reached ();
		}
	} else if (location == FOCUS_TASKPAD)
		e_calendar_table_cut_clipboard (E_CALENDAR_TABLE (priv->todo));
	else
		g_assert_not_reached ();
}

void
gnome_calendar_copy_clipboard (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	FocusLocation location;

	priv = gcal->priv;

	location = get_focus_location (gcal);

	if (location == FOCUS_CALENDAR) {
		switch (priv->current_view_type) {
		case GNOME_CAL_DAY_VIEW :
			e_day_view_copy_clipboard (E_DAY_VIEW (priv->day_view));
			break;
		case GNOME_CAL_WORK_WEEK_VIEW :
			e_day_view_copy_clipboard (E_DAY_VIEW (priv->work_week_view));
			break;
		case GNOME_CAL_WEEK_VIEW :
			e_week_view_copy_clipboard (E_WEEK_VIEW (priv->week_view));
			break;
		case GNOME_CAL_MONTH_VIEW :
			e_week_view_copy_clipboard (E_WEEK_VIEW (priv->month_view));
			break;
		default:
			g_assert_not_reached ();
		}
	} else if (location == FOCUS_TASKPAD)
		e_calendar_table_copy_clipboard (E_CALENDAR_TABLE (priv->todo));
	else
		g_assert_not_reached ();
}

void
gnome_calendar_paste_clipboard (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	FocusLocation location;

	priv = gcal->priv;

	location = get_focus_location (gcal);

	if (location == FOCUS_CALENDAR) {
		switch (priv->current_view_type) {
		case GNOME_CAL_DAY_VIEW :
			e_day_view_paste_clipboard (E_DAY_VIEW (priv->day_view));
			break;
		case GNOME_CAL_WORK_WEEK_VIEW :
			e_day_view_paste_clipboard (E_DAY_VIEW (priv->work_week_view));
			break;
		case GNOME_CAL_WEEK_VIEW :
			e_week_view_paste_clipboard (E_WEEK_VIEW (priv->week_view));
			break;
		case GNOME_CAL_MONTH_VIEW :
			e_week_view_paste_clipboard (E_WEEK_VIEW (priv->month_view));
			break;
		}
	} else if (location == FOCUS_TASKPAD)
		e_calendar_table_paste_clipboard (E_CALENDAR_TABLE (priv->todo));
	else
		g_assert_not_reached ();
}


/* Get the current timezone. */
icaltimezone*
gnome_calendar_get_timezone	(GnomeCalendar	*gcal)
{
	g_return_val_if_fail (gcal != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), NULL);

	return gcal->priv->zone;
}


static void
gnome_calendar_notify_dates_shown_changed (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	time_t start_time, end_time;

	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	priv = gcal->priv;

	/* If no time range is set yet, just return. */
	if (!gnome_calendar_get_visible_time_range (gcal, &start_time,
						    &end_time))
		return;

	/* We check if the visible date range has changed, and only emit the
	   signal if it has. (This makes sure we only change the folder title
	   bar label in the shell when we need to.) */
	if (priv->visible_start != start_time
	    || priv->visible_end != end_time) {
		priv->visible_start = start_time;
		priv->visible_end = end_time;

		gtk_signal_emit (GTK_OBJECT (gcal),
				 gnome_calendar_signals[DATES_SHOWN_CHANGED]);
	}
}


/* Returns the number of selected events (0 or 1 at present). */
gint
gnome_calendar_get_num_events_selected (GnomeCalendar *gcal)
{
	GtkWidget *view;
	gint retval = 0;

	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), 0);

	view = gnome_calendar_get_current_view_widget (gcal);
	if (E_IS_DAY_VIEW (view))
		retval = e_day_view_get_num_events_selected (E_DAY_VIEW (view));
	else
		retval = e_week_view_get_num_events_selected (E_WEEK_VIEW (view));

	return retval;
}

/**
 * gnome_calendar_get_num_tasks_selected:
 * @gcal: A calendar view.
 * 
 * Queries the number of tasks that are currently selected in the task pad of a
 * calendar view.
 * 
 * Return value: Number of selected tasks.
 **/
gint
gnome_calendar_get_num_tasks_selected (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	ETable *etable;

	g_return_val_if_fail (gcal != NULL, -1);
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), -1);

	priv = gcal->priv;

	etable = e_calendar_table_get_table (E_CALENDAR_TABLE (priv->todo));
	return e_table_selected_count (etable);
}


void
gnome_calendar_delete_selection		(GnomeCalendar  *gcal)
{
	GnomeCalendarPrivate *priv;
	FocusLocation location;
	GtkWidget *view;

	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	priv = gcal->priv;

	location = get_focus_location (gcal);

	if (location == FOCUS_CALENDAR) {
		view = gnome_calendar_get_current_view_widget (gcal);

		if (E_IS_DAY_VIEW (view))
			e_day_view_delete_event (E_DAY_VIEW (view));
		else
			e_week_view_delete_event (E_WEEK_VIEW (view));
	} else if (location == FOCUS_TASKPAD)
		e_calendar_table_delete_selected (E_CALENDAR_TABLE (priv->todo));
	else
		g_assert_not_reached ();
}

void
gnome_calendar_delete_selected_occurrence (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	FocusLocation location;
	GtkWidget *view;

	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	priv = gcal->priv;

	location = get_focus_location (gcal);

	if (location == FOCUS_CALENDAR) {

		view = gnome_calendar_get_current_view_widget (gcal);

		if (E_IS_DAY_VIEW (view))
			e_day_view_delete_occurrence (E_DAY_VIEW (view));
		else
			e_week_view_delete_occurrence (E_WEEK_VIEW (view));
	}
}

typedef struct {
	gboolean remove;
	GnomeCalendar *gcal;
} obj_updated_closure;

static gboolean
check_instance_cb (CalComponent *comp,
		   time_t instance_start,
		   time_t instance_end,
		   gpointer data)
{
	obj_updated_closure *closure = data;

	if (instance_start >= closure->gcal->priv->exp_older_than ||
	    instance_end >= closure->gcal->priv->exp_older_than) {
		closure->remove = FALSE;
		return FALSE;
	}

	closure->remove = TRUE;
	return TRUE;
}

static void
purging_obj_updated_cb (CalQuery *query, const char *uid,
			gboolean query_in_progress, int n_scanned, int total,
			gpointer data)
{
	GnomeCalendarPrivate *priv;
	GnomeCalendar *gcal = data;
	CalComponent *comp;
	obj_updated_closure closure;
	gchar *msg;

	priv = gcal->priv;

	if (cal_client_get_object (priv->client, uid, &comp) != CAL_CLIENT_GET_SUCCESS)
		return;

	msg = g_strdup_printf (_("Purging event %s"), uid);

	/* further filter the event, to check the last recurrence end date */
	if (cal_component_has_recurrences (comp)) {
		closure.remove = TRUE;
		closure.gcal = gcal;

		cal_recur_generate_instances (comp, priv->exp_older_than, -1,
					      (CalRecurInstanceFn) check_instance_cb,
					      &closure,
					      (CalRecurResolveTimezoneFn) cal_client_resolve_tzid_cb,
					      priv->client, priv->zone);

		if (closure.remove) {
			e_week_view_set_status_message (E_WEEK_VIEW (priv->week_view), msg);
			delete_error_dialog (cal_client_remove_object (priv->client, uid),
					     CAL_COMPONENT_EVENT);
		}
	} else {
		e_week_view_set_status_message (E_WEEK_VIEW (priv->week_view), msg);
		delete_error_dialog (cal_client_remove_object (priv->client, uid), CAL_COMPONENT_EVENT);
	}

	g_object_unref (comp);
	g_free (msg);
}

static void
purging_eval_error_cb (CalQuery *query, const char *error_str, gpointer data)
{
	GnomeCalendarPrivate *priv;
	GnomeCalendar *gcal = data;

	priv = gcal->priv;

	e_week_view_set_status_message (E_WEEK_VIEW (priv->week_view), NULL);

	g_signal_handlers_disconnect_matched (priv->exp_query, G_SIGNAL_MATCH_DATA,
					      0, 0, NULL, NULL, gcal);
	g_object_unref (priv->exp_query);
	priv->exp_query = NULL;
}

static void
purging_query_done_cb (CalQuery *query, CalQueryDoneStatus status, const char *error_str, gpointer data)
{
	GnomeCalendarPrivate *priv;
	GnomeCalendar *gcal = data;

	priv = gcal->priv;

	e_week_view_set_status_message (E_WEEK_VIEW (priv->week_view), NULL);

	g_signal_handlers_disconnect_matched (priv->exp_query, G_SIGNAL_MATCH_DATA,
					      0, 0, NULL, NULL, gcal);
	g_object_unref (priv->exp_query);
	priv->exp_query = NULL;
}

void
gnome_calendar_purge (GnomeCalendar *gcal, time_t older_than)
{
	GnomeCalendarPrivate *priv;
	char *sexp, *start, *end;

	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	priv = gcal->priv;

	/* if we have a query, we are already purging */
	if (priv->exp_query)
		return;

	priv->exp_older_than = older_than;
	start = isodate_from_time_t (0);
	end = isodate_from_time_t (older_than);
	sexp = g_strdup_printf ("(and (= (get-vtype) \"VEVENT\")"
				"     (occur-in-time-range? (make-time \"%s\")"
				"                           (make-time \"%s\")))",
				start, end);

	e_week_view_set_status_message (E_WEEK_VIEW (priv->week_view), _("Purging"));
	priv->exp_query = cal_client_get_query (priv->client, sexp);

	g_free (sexp);
	g_free (start);
	g_free (end);

	if (!priv->exp_query) {
		e_week_view_set_status_message (E_WEEK_VIEW (priv->week_view), NULL);
		g_message ("gnome_calendar_purge(): Could not create the query");
		return;
	}
	
	g_signal_connect (priv->exp_query, "obj_updated", G_CALLBACK (purging_obj_updated_cb), gcal);
	g_signal_connect (priv->exp_query, "query_done", G_CALLBACK (purging_query_done_cb), gcal);
	g_signal_connect (priv->exp_query, "eval_error", G_CALLBACK (purging_eval_error_cb), gcal);
}

ECalendarTable*
gnome_calendar_get_task_pad	(GnomeCalendar *gcal)
{
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), NULL);

	return E_CALENDAR_TABLE (gcal->priv->todo);
}

