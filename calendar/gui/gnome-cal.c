/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar - Main calendar view widget
 *
 * Copyright (C) 1998 The Free Software Foundation
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 * Copyright (C) 2003 Novell, Inc
 *
 * Authors: Miguel de Icaza <miguel@ximian.com>
 *          Federico Mena-Quintero <federico@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *          Rodrigo Moya <rodrigo@ximian.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <math.h>
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
#include "shell/e-user-creatable-items-handler.h"
#include <libecal/e-cal-time-util.h>
#include <gal/menus/gal-view-factory-etable.h>
#include <gal/menus/gal-view-etable.h>
#include <gal/menus/gal-define-views-dialog.h>
#include "widgets/menus/gal-view-menus.h"
#include "widgets/misc/e-error.h"
#include "e-comp-editor-registry.h"
#include "dialogs/delete-error.h"
#include "dialogs/event-editor.h"
#include "dialogs/task-editor.h"
#include "comp-util.h"
#include "e-calendar-marshal.h"
#include "e-cal-model-calendar.h"
#include "e-day-view.h"
#include "e-day-view-config.h"
#include "e-day-view-time-item.h"
#include "e-week-view.h"
#include "e-week-view-config.h"
#include "e-cal-list-view.h"
#include "e-cal-list-view-config.h"
#include "e-mini-calendar-config.h"
#include "e-calendar-table-config.h"
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
#include "ea-calendar.h"
#include "common/authentication.h"
#include "e-cal-popup.h"
#include "e-cal-menu.h"

/* FIXME glib 2.4 and above has this */
#ifndef G_MAXINT32
#define G_MAXINT32	((gint32)  0x7fffffff)
#endif



/* Private part of the GnomeCalendar structure */
struct _GnomeCalendarPrivate {
	/* The clients for display */
	
	GHashTable *clients[E_CAL_SOURCE_TYPE_LAST];
	GList *clients_list[E_CAL_SOURCE_TYPE_LAST];
	ECal *default_client[E_CAL_SOURCE_TYPE_LAST];
	
	/* Categories from the calendar clients */
	/* FIXME are we getting all the categories? */
	GPtrArray *categories[E_CAL_SOURCE_TYPE_LAST];

	/*
	 * Fields for the calendar view
	 */

	/* This is the last time explicitly selected by the user */
	time_t base_view_time;
	
	/* Widgets */

	GtkWidget   *search_bar;

	GtkWidget   *hpane;
	GtkWidget   *notebook;
	GtkWidget   *vpane;

	ECalendar   *date_navigator;
	EMiniCalendarConfig *date_navigator_config;	
	GtkWidget   *todo;
	ECalendarTableConfig *todo_config;
	
	GtkWidget   *day_view;
	GtkWidget   *work_week_view;
	GtkWidget   *week_view;
	GtkWidget   *month_view;
	GtkWidget   *list_view;

	/* Activity */
	EActivityHandler *activity_handler;

	/* plugin menu managers */
	ECalMenu    *calendar_menu;
	ECalMenu    *taskpad_menu;

	/* Calendar query for the date navigator */
	GList       *dn_queries; /* list of CalQueries */
	char        *sexp;
	char        *todo_sexp;
	guint        update_timeout;
	
	/* This is the view currently shown. We use it to keep track of the
	   positions of the panes. range_selected is TRUE if a range of dates
	   was selected in the date navigator to show the view. */
	ECalendarView    *views[GNOME_CAL_LAST_VIEW];
	GObject    *configs[GNOME_CAL_LAST_VIEW];
	GnomeCalendarViewType current_view_type;
	GList *notifications;
	
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

	/* Our current week start */
	int week_start;
	
	/* Our current timezone. */
	icaltimezone *zone;

	/* The dates currently shown. If they are -1 then we have no dates
	   shown. We only use these to check if we need to emit a
	   'dates-shown-changed' signal.*/
	time_t visible_start;
	time_t visible_end;
};

/* Signal IDs */

enum {
	DATES_SHOWN_CHANGED,
	CALENDAR_SELECTION_CHANGED,
	TASKPAD_SELECTION_CHANGED,
	CALENDAR_FOCUS_CHANGE,
	TASKPAD_FOCUS_CHANGE,
	GOTO_DATE,
	SOURCE_ADDED,
	SOURCE_REMOVED,
	LAST_SIGNAL
};

/* Used to indicate who has the focus within the calendar view */
typedef enum {
	FOCUS_CALENDAR,
	FOCUS_TASKPAD,
	FOCUS_OTHER
} FocusLocation;

static guint gnome_calendar_signals[LAST_SIGNAL];




static void gnome_calendar_destroy (GtkObject *object);
static void gnome_calendar_goto_date (GnomeCalendar *gcal,
				      GnomeCalendarGotoDateType goto_date);

static void gnome_calendar_set_pane_positions	(GnomeCalendar	*gcal);
static void update_view_times (GnomeCalendar *gcal, time_t start_time);
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

static void update_query (GnomeCalendar *gcal);

static void update_todo_view (GnomeCalendar *gcal);

G_DEFINE_TYPE (GnomeCalendar, gnome_calendar, GTK_TYPE_VBOX);

/* Class initialization function for the gnome calendar */
static void
gnome_calendar_class_init (GnomeCalendarClass *class)
{
	GtkObjectClass *object_class;
	GtkBindingSet *binding_set;

	object_class = (GtkObjectClass *) class;

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

	gnome_calendar_signals[SOURCE_ADDED] =
		g_signal_new ("source_added",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GnomeCalendarClass, source_added),
			      NULL, NULL,
			      e_calendar_marshal_VOID__INT_OBJECT,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_INT, G_TYPE_OBJECT);

	gnome_calendar_signals[SOURCE_REMOVED] =
		g_signal_new ("source_removed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GnomeCalendarClass, source_removed),
			      NULL, NULL,
			      e_calendar_marshal_VOID__INT_OBJECT,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_INT, G_TYPE_OBJECT);

	gnome_calendar_signals[GOTO_DATE] =
		g_signal_new ("goto_date",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (GnomeCalendarClass, goto_date),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_INT);

	object_class->destroy = gnome_calendar_destroy;

	class->dates_shown_changed = NULL;
	class->calendar_selection_changed = NULL;
	class->taskpad_selection_changed = NULL;
	class->calendar_focus_change = NULL;
	class->taskpad_focus_change = NULL;
	class->source_added = NULL;
	class->source_removed = NULL;
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
	/* init the accessibility support for gnome_calendar */
	gnome_calendar_a11y_init ();

}

/* Callback used when the calendar query reports of an updated object */
static void
dn_e_cal_view_objects_added_cb (ECalView *query, GList *objects, gpointer data)
{
	GnomeCalendar *gcal;
	GnomeCalendarPrivate *priv;
	GList *l;
	
	gcal = GNOME_CALENDAR (data);
	priv = gcal->priv;

	for (l = objects; l; l = l->next) {
		ECalComponent *comp = NULL;

		comp = e_cal_component_new ();
		if (!e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (l->data))) {
			g_object_unref (comp);
			
			continue;
		}

		tag_calendar_by_comp (priv->date_navigator, comp, e_cal_view_get_client (query), NULL,
				      FALSE, TRUE);
		g_object_unref (comp);
	}
}

static void
dn_e_cal_view_objects_modified_cb (ECalView *query, GList *objects, gpointer data)
{
	GnomeCalendar *gcal;
	GnomeCalendarPrivate *priv;

	gcal = GNOME_CALENDAR (data);
	priv = gcal->priv;

	/* We have to retag the whole thing: an event may change dates
	 * and the tag_calendar_by_comp() below would not know how to
	 * untag the old dates.
	 */
	update_query (gcal);
}

/* Callback used when the calendar query reports of a removed object */
static void
dn_e_cal_view_objects_removed_cb (ECalView *query, GList *uids, gpointer data)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	/* Just retag the whole thing */
	update_query (gcal);
}

/* Callback used when the calendar query is done */
static void
dn_e_cal_view_done_cb (ECalView *query, ECalendarStatus status, gpointer data)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	/* FIXME Better error reporting */
	if (status != E_CALENDAR_STATUS_OK)
		g_warning (G_STRLOC ": Query did not successfully complete");
}

/* Returns the current view widget, an EDayView, EWeekView or ECalListView. */
GtkWidget*
gnome_calendar_get_current_view_widget (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;

	priv = gcal->priv;

	return GTK_WIDGET (priv->views[priv->current_view_type]);
}

static void
get_times_for_views (GnomeCalendar *gcal, GnomeCalendarViewType view_type, time_t *start_time, time_t *end_time)
{
	GnomeCalendarPrivate *priv;
	int shown, display_start;
	GDate date;
	gint weekday, first_day, last_day, days_shown, i;
	gboolean has_working_days = FALSE;
	guint offset;
	struct icaltimetype tt = icaltime_null_time ();
	
	priv = gcal->priv;
	
	switch (view_type) {
	case GNOME_CAL_DAY_VIEW:
		shown  = e_day_view_get_days_shown (E_DAY_VIEW (priv->views[view_type]));
		*start_time = time_day_begin_with_zone (*start_time, priv->zone);
		*end_time = time_add_day_with_zone (*start_time, shown, priv->zone);
		break;
	case GNOME_CAL_WORK_WEEK_VIEW:
		/* FIXME Kind of gross, but it works */
		time_to_gdate_with_zone (&date, *start_time, priv->zone);

		/* The start of the work-week is the first working day after the
		   week start day. */

		/* Get the weekday corresponding to start_time, 0 (Sun) to 6 (Sat). */
		weekday = g_date_weekday (&date) % 7;

		/* Find the first working day in the week, 0 (Sun) to 6 (Sat). */
		first_day = (E_DAY_VIEW (priv->views[view_type])->week_start_day + 1) % 7;
		for (i = 0; i < 7; i++) {
			if (E_DAY_VIEW (priv->views[view_type])->working_days & (1 << first_day)) {
				has_working_days = TRUE;
				break;
			}
			first_day = (first_day + 1) % 7;
		}

		if (has_working_days) {
			/* Now find the last working day of the week, backwards. */
			last_day = E_DAY_VIEW (priv->views[view_type])->week_start_day % 7;
			for (i = 0; i < 7; i++) {
				if (E_DAY_VIEW (priv->views[view_type])->working_days & (1 << last_day))
					break;
				last_day = (last_day + 6) % 7;
			}
			/* Now calculate the days we need to show to include all the
			   working days in the week. Add 1 to make it inclusive. */
			days_shown = (last_day + 7 - first_day) % 7 + 1;
		} else {
			/* If no working days are set, just use 7. */
			days_shown = 7;
		}

		/* Calculate how many days we need to go back to the first workday. */
		if (weekday < first_day) {
			offset = (first_day - weekday) % 7;
			g_date_add_days (&date, offset);
		} else {
			offset = (weekday - first_day) % 7;
			g_date_subtract_days (&date, offset);
		}

		tt.year = g_date_year (&date);
		tt.month = g_date_month (&date);
		tt.day = g_date_day (&date);

		*start_time = icaltime_as_timet_with_zone (tt, priv->zone);
		*end_time = time_add_day_with_zone (*start_time, days_shown, priv->zone);
		break;
	case GNOME_CAL_WEEK_VIEW:
		/* FIXME We should be using the same day of the week enum every where */
		display_start = (E_WEEK_VIEW (priv->views[view_type])->display_start_day + 1) % 7;

		*start_time = time_week_begin_with_zone (*start_time, display_start, priv->zone);
		*end_time = time_add_week_with_zone (*start_time, 1, priv->zone);
		break;
	case GNOME_CAL_MONTH_VIEW:
		shown = e_week_view_get_weeks_shown (E_WEEK_VIEW (priv->views[view_type]));
		/* FIXME We should be using the same day of the week enum every where */
		display_start = (E_WEEK_VIEW (priv->views[view_type])->display_start_day + 1) % 7;
		
		if (!priv->range_selected)
			*start_time = time_month_begin_with_zone (*start_time, priv->zone);
		*start_time = time_week_begin_with_zone (*start_time, display_start, priv->zone);
		*end_time = time_add_week_with_zone (*start_time, shown, priv->zone);
		break;
	case GNOME_CAL_LIST_VIEW:
		/* FIXME What to do here? */
		*start_time = time_month_begin_with_zone (*start_time, priv->zone);
		*end_time = time_add_month_with_zone (*start_time, 1, priv->zone);
		break;		
	default:
		g_assert_not_reached ();
		return;
	}
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
		ECalListView *lv;

		widget = gnome_calendar_get_current_view_widget (gcal);

		switch (priv->current_view_type) {
		case GNOME_CAL_DAY_VIEW:
		case GNOME_CAL_WORK_WEEK_VIEW:
			dv = E_DAY_VIEW (widget);

			if (GTK_WIDGET_HAS_FOCUS (dv->top_canvas)
			    || GNOME_CANVAS (dv->top_canvas)->focused_item != NULL
			    || GTK_WIDGET_HAS_FOCUS (dv->main_canvas)
			    || GNOME_CANVAS (dv->main_canvas)->focused_item != NULL)
				return FOCUS_CALENDAR;
			else
				return FOCUS_OTHER;

		case GNOME_CAL_WEEK_VIEW:
		case GNOME_CAL_MONTH_VIEW:
			wv = E_WEEK_VIEW (widget);

			if (GTK_WIDGET_HAS_FOCUS (wv->main_canvas)
			    || GNOME_CANVAS (wv->main_canvas)->focused_item != NULL)
				return FOCUS_CALENDAR;
			else
				return FOCUS_OTHER;

		case GNOME_CAL_LIST_VIEW:
			lv = E_CAL_LIST_VIEW (widget);

			if (GTK_WIDGET_HAS_FOCUS (e_table_scrolled_get_table (lv->table_scrolled)))
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
adjust_e_cal_view_sexp (GnomeCalendar *gcal, const char *sexp)
{
	time_t start_time, end_time;
	char *start, *end;
	char *new_sexp;

	get_date_navigator_range (gcal, &start_time, &end_time);
	if (start_time == -1 || end_time == -1)
		return NULL;

	start = isodate_from_time_t (start_time);
	end = isodate_from_time_t (end_time);

	new_sexp = g_strdup_printf ("(and (occur-in-time-range? (make-time \"%s\")"
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
	ECalView *old_query;
	char *real_sexp;
	GList *l;

	priv = gcal->priv;

	e_calendar_view_set_status_message (E_CALENDAR_VIEW (priv->week_view), _("Updating query"));
	e_calendar_item_clear_marks (priv->date_navigator->calitem);

	/* free the previous queries */
	for (l = priv->dn_queries; l != NULL; l = l->next) {
		old_query = l->data;

		if (old_query) {
			g_signal_handlers_disconnect_matched (old_query, G_SIGNAL_MATCH_DATA,
							      0, 0, NULL, NULL, gcal);
			g_object_unref (old_query);
		}
	}

	g_list_free (priv->dn_queries);
	priv->dn_queries = NULL;

	g_assert (priv->sexp != NULL);

	real_sexp = adjust_e_cal_view_sexp (gcal, priv->sexp);
	if (!real_sexp) {
		e_calendar_view_set_status_message (E_CALENDAR_VIEW (priv->week_view), NULL);
		return; /* No time range is set, so don't start a query */
	}

	/* create queries for each loaded client */
	for (l = priv->clients_list[E_CAL_SOURCE_TYPE_EVENT]; l != NULL; l = l->next) {
		/* don't create queries for clients not loaded yet */
		if (e_cal_get_load_state ((ECal *) l->data) != E_CAL_LOAD_LOADED)
			continue;

		old_query = NULL;
		if (!e_cal_get_query ((ECal *) l->data, real_sexp, &old_query, NULL)) {
			g_warning (G_STRLOC ": Could not create the query");

			continue;
		}

		g_signal_connect (old_query, "objects_added",
				  G_CALLBACK (dn_e_cal_view_objects_added_cb), gcal);
		g_signal_connect (old_query, "objects_modified",
				  G_CALLBACK (dn_e_cal_view_objects_modified_cb), gcal);
		g_signal_connect (old_query, "objects_removed",
				  G_CALLBACK (dn_e_cal_view_objects_removed_cb), gcal);
		g_signal_connect (old_query, "view_done",
				  G_CALLBACK (dn_e_cal_view_done_cb), gcal);

		priv->dn_queries = g_list_append (priv->dn_queries, old_query);

		e_cal_view_start (old_query);
	}

	g_free (real_sexp);
	e_calendar_view_set_status_message (E_CALENDAR_VIEW (priv->week_view), NULL);
	update_todo_view (gcal);
}

static void
set_search_query (GnomeCalendar *gcal, const char *sexp)
{
	GnomeCalendarPrivate *priv;
	int i;

	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (sexp != NULL);

	priv = gcal->priv;

	/* Set the query on the date navigator */

	if (priv->sexp)
		g_free (priv->sexp);

	priv->sexp = g_strdup (sexp);

	update_query (gcal);

	/* Set the query on the views */
	for (i = 0; i < GNOME_CAL_LAST_VIEW; i++)
		e_cal_model_set_search_query (e_calendar_view_get_model (priv->views[i]), sexp);

	/* Set the query on the task pad */
	update_todo_view (gcal);
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
	set_search_query (gcal, sexp);
}

/* Callback used when the selected category in the search bar changes */
static void
search_bar_category_changed_cb (CalSearchBar *cal_search, const char *category, gpointer data)
{
	GnomeCalendar *gcal;
	GnomeCalendarPrivate *priv;
	ECalModel *model;
	int i;

	gcal = GNOME_CALENDAR (data);
	priv = gcal->priv;

	for (i = 0; i < GNOME_CAL_LAST_VIEW; i++) {
		e_calendar_view_set_default_category (E_CALENDAR_VIEW (priv->views[i]),
						 category);
	}

	model = e_calendar_table_get_model (E_CALENDAR_TABLE (priv->todo));
	e_cal_model_set_default_category (model, category);
}

static void
view_selection_changed_cb (GtkWidget *view, GnomeCalendar *gcal)
{
	gtk_signal_emit (GTK_OBJECT (gcal),
			 gnome_calendar_signals[CALENDAR_SELECTION_CHANGED]);
}

static void
user_created_cb (GtkWidget *view, GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;	
	ECal *ecal;
	ECalModel *model;
	
	priv = gcal->priv;	

	model = e_calendar_view_get_model (priv->views[priv->current_view_type]);
	ecal = e_cal_model_get_default_client (model);

	gnome_calendar_add_source (gcal, E_CAL_SOURCE_TYPE_EVENT, e_cal_get_source (ecal));
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
	g_signal_connect_after (dv->top_canvas, "focus_in_event",
			  G_CALLBACK (calendar_focus_change_cb), gcal);
	g_signal_connect_after (dv->top_canvas, "focus_out_event",
			  G_CALLBACK (calendar_focus_change_cb), gcal);

	g_signal_connect_after (dv->main_canvas, "focus_in_event",
			  G_CALLBACK (calendar_focus_change_cb), gcal);
	g_signal_connect_after (dv->main_canvas, "focus_out_event",
				G_CALLBACK (calendar_focus_change_cb), gcal);
}

/* Connects to the focus change signals of a week view widget */
static void
connect_week_view_focus (GnomeCalendar *gcal, EWeekView *wv)
{
	if (!E_IS_WEEK_VIEW (wv))
		return;

	g_signal_connect (wv->main_canvas, "focus_in_event",
			  G_CALLBACK (calendar_focus_change_cb), gcal);
	g_signal_connect (wv->main_canvas, "focus_out_event",
			  G_CALLBACK (calendar_focus_change_cb), gcal);
}

static void
connect_list_view_focus (GnomeCalendar *gcal, ECalListView *lv)
{
	ETable *etable;
	
	etable = e_table_scrolled_get_table (lv->table_scrolled);
	
	g_signal_connect (etable->table_canvas, "focus_in_event",
			  G_CALLBACK (calendar_focus_change_cb), gcal);
	g_signal_connect (etable->table_canvas, "focus_out_event",
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
set_week_start (GnomeCalendar *calendar)
{
	GnomeCalendarPrivate *priv;

	priv = calendar->priv;
	
	priv->week_start = calendar_config_get_week_start_day ();

	/* Only do this if views exist */
	if (priv->day_view && priv->work_week_view && priv->week_view && priv->month_view && priv->list_view) {
		update_view_times (calendar, priv->base_view_time);
		gnome_calendar_update_date_navigator (calendar);
		gnome_calendar_notify_dates_shown_changed (calendar);
	}
}

static void
week_start_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	GnomeCalendar *calendar = data;
	
	set_week_start (calendar);
}

static void
set_working_days (GnomeCalendar *calendar)
{
	GnomeCalendarPrivate *priv;

	priv = calendar->priv;
	
	/* Only do this if views exist */
	if (priv->day_view && priv->work_week_view && priv->week_view && priv->month_view && priv->list_view) {
		update_view_times (calendar, priv->base_view_time);
		gnome_calendar_update_date_navigator (calendar);
		gnome_calendar_notify_dates_shown_changed (calendar);
	}
}

static void
working_days_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	GnomeCalendar *calendar = data;
	
	set_working_days (calendar);
}

static void
set_timezone (GnomeCalendar *calendar) 
{
	GnomeCalendarPrivate *priv;
	int i;
	
	priv = calendar->priv;
	
	priv->zone = calendar_config_get_icaltimezone ();

	for (i = 0; i < E_CAL_SOURCE_TYPE_LAST; i++) {
		GList *l;

		for (l = priv->clients_list[i]; l != NULL; l = l->next) {
			ECal *client = l->data;
			
			if (e_cal_get_load_state (client) == E_CAL_LOAD_LOADED)
				/* FIXME Error checking */
				e_cal_set_default_timezone (client, priv->zone, NULL);
		}

		if (priv->default_client[i] 
		    && e_cal_get_load_state (priv->default_client[i]) == E_CAL_LOAD_LOADED)
			/* FIXME Error checking */
			e_cal_set_default_timezone (priv->default_client[i], priv->zone, NULL);
	}
}

static void
timezone_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	GnomeCalendar *calendar = data;
	
	set_timezone (calendar);
}

static void
update_todo_view (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	ECalModel *model;
	char *sexp = NULL;
	
	priv = gcal->priv;
	
	/* Set the query on the task pad */
	if (priv->todo_sexp) {
		g_free (priv->todo_sexp);
	}
	
	model = e_calendar_table_get_model (E_CALENDAR_TABLE (priv->todo));
		
	if ((sexp = calendar_config_get_hide_completed_tasks_sexp()) != NULL) {
		priv->todo_sexp = g_strdup_printf ("(and %s %s)", sexp, priv->sexp);
		e_cal_model_set_search_query (model, priv->todo_sexp);
		g_free (sexp);
	} else {
		priv->todo_sexp = g_strdup (priv->sexp);
		e_cal_model_set_search_query (model, priv->todo_sexp);
	}
	
}

static gboolean
update_todo_view_cb (GnomeCalendar *gcal)
{	
	update_todo_view(gcal);

	return TRUE;
}

static void
config_hide_completed_tasks_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	update_todo_view (data);
}

static void
setup_config (GnomeCalendar *calendar)
{
	GnomeCalendarPrivate *priv;
	guint not;

	priv = calendar->priv;

	/* Week Start */
	set_week_start (calendar);
	not = calendar_config_add_notification_week_start_day (week_start_changed_cb, calendar);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* Working Days */
	set_working_days (calendar);
	not = calendar_config_add_notification_working_days (working_days_changed_cb, calendar);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* Timezone */
	set_timezone (calendar);	
	not = calendar_config_add_notification_timezone (timezone_changed_cb, calendar);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* Hide completed tasks */
	not = calendar_config_add_notification_hide_completed_tasks (config_hide_completed_tasks_changed_cb, 
							      calendar);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));
	
	not = calendar_config_add_notification_hide_completed_tasks_units (config_hide_completed_tasks_changed_cb, 
							      calendar);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));
	
	not = calendar_config_add_notification_hide_completed_tasks_value (config_hide_completed_tasks_changed_cb, 
							      calendar);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));
	
	/* Pane positions */
	priv->hpane_pos = calendar_config_get_hpane_pos ();
	priv->vpane_pos = calendar_config_get_vpane_pos ();
	priv->hpane_pos_month_view = calendar_config_get_month_hpane_pos ();
	priv->vpane_pos_month_view = calendar_config_get_month_vpane_pos ();
}

static void
update_adjustment (GnomeCalendar *gcal, GtkAdjustment *adjustment, EWeekView *week_view) 
{
	GDate date;
	gint week_offset;
	struct icaltimetype start_tt = icaltime_null_time ();
	time_t lower;
	guint32 old_first_day_julian, new_first_day_julian;

	/* If we don't have a valid date set yet, just return. */
	if (!g_date_valid (&week_view->first_day_shown))
		return;

	/* Determine the first date shown. */
	date = week_view->base_date;
	week_offset = floor (adjustment->value + 0.5);
	g_date_add_days (&date, week_offset * 7);

	/* Convert the old & new first days shown to julian values. */
	old_first_day_julian = g_date_julian (&week_view->first_day_shown);
	new_first_day_julian = g_date_julian (&date);

	/* If we are already showing the date, just return. */
	if (old_first_day_julian == new_first_day_julian)
		return;

	/* Convert it to a time_t. */
	start_tt.year = g_date_year (&date);
	start_tt.month = g_date_month (&date);
	start_tt.day = g_date_day (&date);

	lower = icaltime_as_timet_with_zone (start_tt, gcal->priv->zone);

	e_week_view_set_update_base_date (week_view, FALSE);
	update_view_times (gcal, lower);
	gnome_calendar_update_date_navigator (gcal);
	gnome_calendar_notify_dates_shown_changed (gcal);
	e_week_view_set_update_base_date (week_view, TRUE);
}

static void
week_view_adjustment_changed_cb (GtkAdjustment *adjustment, GnomeCalendar *gcal) 
{
	update_adjustment (gcal, adjustment, E_WEEK_VIEW (gcal->priv->week_view));	
}

static void
month_view_adjustment_changed_cb (GtkAdjustment *adjustment, GnomeCalendar *gcal) 
{
	update_adjustment (gcal, adjustment, E_WEEK_VIEW (gcal->priv->month_view));
}

static void
setup_widgets (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	GtkWidget *w;
	gchar *filename;
	ETable *etable;
	GtkAdjustment *adjustment;
	int i;

	priv = gcal->priv;

	priv->search_bar = cal_search_bar_new (CAL_SEARCH_CALENDAR_DEFAULT);
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
	priv->date_navigator_config = e_mini_calendar_config_new (priv->date_navigator);
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
	priv->todo_config = e_calendar_table_config_new (E_CALENDAR_TABLE (priv->todo));
	gtk_paned_pack2 (GTK_PANED (priv->vpane), priv->todo, TRUE, TRUE);
	gtk_widget_show (priv->todo);

	filename = g_build_filename (calendar_component_peek_config_directory (calendar_component_peek ()),
				     "TaskPad", NULL);
	e_calendar_table_load_state (E_CALENDAR_TABLE (priv->todo), filename);
	
	update_todo_view (gcal);
	g_free (filename);

	etable = e_calendar_table_get_table (E_CALENDAR_TABLE (priv->todo));
	g_signal_connect (etable->table_canvas, "focus_in_event",
			  G_CALLBACK (table_canvas_focus_change_cb), gcal);
	g_signal_connect (etable->table_canvas, "focus_out_event",
			  G_CALLBACK (table_canvas_focus_change_cb), gcal);

	g_signal_connect (etable, "selection_change",
			  G_CALLBACK (table_selection_change_cb), gcal);

	/* Timeout check to hide completed items */
	priv->update_timeout = g_timeout_add_full (G_PRIORITY_LOW, 60000, (GSourceFunc) update_todo_view_cb, gcal, NULL);	

	/* The Day View. */
	priv->day_view = e_day_view_new ();
	e_calendar_view_set_calendar (E_CALENDAR_VIEW (priv->day_view), gcal);
	e_calendar_view_set_timezone (E_CALENDAR_VIEW (priv->day_view), priv->zone);
	g_signal_connect (priv->day_view, "selection_changed",
			  G_CALLBACK (view_selection_changed_cb), gcal);
	connect_day_view_focus (gcal, E_DAY_VIEW (priv->day_view));

	/* The Work Week View. */
	priv->work_week_view = e_day_view_new ();
	e_day_view_set_work_week_view (E_DAY_VIEW (priv->work_week_view),
				       TRUE);
	e_day_view_set_days_shown (E_DAY_VIEW (priv->work_week_view), 5);
	e_calendar_view_set_calendar (E_CALENDAR_VIEW (priv->work_week_view), gcal);
	e_calendar_view_set_timezone (E_CALENDAR_VIEW (priv->work_week_view), priv->zone);
	connect_day_view_focus (gcal, E_DAY_VIEW (priv->work_week_view));

	/* The Week View. */
	priv->week_view = e_week_view_new ();
	e_calendar_view_set_calendar (E_CALENDAR_VIEW (priv->week_view), gcal);
	e_calendar_view_set_timezone (E_CALENDAR_VIEW (priv->week_view), priv->zone);
	g_signal_connect (priv->week_view, "selection_changed",
			  G_CALLBACK (view_selection_changed_cb), gcal);

	connect_week_view_focus (gcal, E_WEEK_VIEW (priv->week_view));

	adjustment = gtk_range_get_adjustment (GTK_RANGE (E_WEEK_VIEW (priv->week_view)->vscrollbar));
	g_signal_connect (adjustment, "value_changed",
			  G_CALLBACK (week_view_adjustment_changed_cb),
			  gcal);
	
	/* The Month View. */
	priv->month_view = e_week_view_new ();
	e_calendar_view_set_calendar (E_CALENDAR_VIEW (priv->month_view), gcal);
	e_calendar_view_set_timezone (E_CALENDAR_VIEW (priv->month_view), priv->zone);
	e_week_view_set_multi_week_view (E_WEEK_VIEW (priv->month_view), TRUE);
	e_week_view_set_weeks_shown (E_WEEK_VIEW (priv->month_view), 6);
	g_signal_connect (priv->month_view, "selection_changed",
			  G_CALLBACK (view_selection_changed_cb), gcal);

	connect_week_view_focus (gcal, E_WEEK_VIEW (priv->month_view));

	adjustment = gtk_range_get_adjustment (GTK_RANGE (E_WEEK_VIEW (priv->month_view)->vscrollbar));
	g_signal_connect (adjustment, "value_changed",
			  G_CALLBACK (month_view_adjustment_changed_cb),
			  gcal);

	/* The List View. */
	priv->list_view = e_cal_list_view_new ();

	e_calendar_view_set_calendar (E_CALENDAR_VIEW (priv->list_view), gcal);
	e_calendar_view_set_timezone (E_CALENDAR_VIEW (priv->list_view), priv->zone);
	g_signal_connect (priv->list_view, "selection_changed",
			  G_CALLBACK (view_selection_changed_cb), gcal);

	connect_list_view_focus (gcal, E_CAL_LIST_VIEW (priv->list_view));

	priv->views[GNOME_CAL_DAY_VIEW] = E_CALENDAR_VIEW (priv->day_view);
	priv->configs[GNOME_CAL_DAY_VIEW] = G_OBJECT (e_day_view_config_new (E_DAY_VIEW (priv->views[GNOME_CAL_DAY_VIEW])));
	priv->views[GNOME_CAL_WORK_WEEK_VIEW] = E_CALENDAR_VIEW (priv->work_week_view);
	priv->configs[GNOME_CAL_WORK_WEEK_VIEW] = G_OBJECT (e_day_view_config_new (E_DAY_VIEW (priv->views[GNOME_CAL_WORK_WEEK_VIEW])));
	priv->views[GNOME_CAL_WEEK_VIEW] = E_CALENDAR_VIEW (priv->week_view);
	priv->configs[GNOME_CAL_WEEK_VIEW] = G_OBJECT (e_week_view_config_new (E_WEEK_VIEW (priv->views[GNOME_CAL_WEEK_VIEW])));
	priv->views[GNOME_CAL_MONTH_VIEW] = E_CALENDAR_VIEW (priv->month_view);
	priv->configs[GNOME_CAL_MONTH_VIEW] = G_OBJECT (e_week_view_config_new (E_WEEK_VIEW (priv->views[GNOME_CAL_MONTH_VIEW])));
	priv->views[GNOME_CAL_LIST_VIEW] = E_CALENDAR_VIEW (priv->list_view);
	priv->configs[GNOME_CAL_LIST_VIEW] = G_OBJECT (e_cal_list_view_config_new (E_CAL_LIST_VIEW (priv->views[GNOME_CAL_LIST_VIEW])));

	for (i = 0; i < GNOME_CAL_LAST_VIEW; i++) {
		gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
					  GTK_WIDGET (priv->views[i]), gtk_label_new (""));

		g_signal_connect (priv->views[i], "user_created",
				  G_CALLBACK (user_created_cb), gcal);

		gtk_widget_show (GTK_WIDGET (priv->views[i]));
	}

	/* make sure we set the initial time ranges for the views */
	update_view_times (gcal, time (NULL));
	gnome_calendar_update_date_navigator (gcal);	
}

/* Object initialization function for the gnome calendar */
static void
gnome_calendar_init (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	int i;
	
	priv = g_new0 (GnomeCalendarPrivate, 1);
	gcal->priv = priv;

	for (i = 0; i < E_CAL_SOURCE_TYPE_LAST; i++)
		priv->clients[i] = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	
	priv->current_view_type = GNOME_CAL_DAY_VIEW;
	priv->range_selected = FALSE;

	setup_config (gcal);
	setup_widgets (gcal);

	priv->calendar_menu = e_cal_menu_new("org.gnome.evolution.calendar.view");
	priv->taskpad_menu = e_cal_menu_new("org.gnome.evolution.calendar.taskpad");

	priv->dn_queries = NULL;	
	priv->sexp = g_strdup ("#t"); /* Match all */
	priv->todo_sexp = g_strdup ("#t");

	priv->view_instance = NULL;
	priv->view_menus = NULL;

	priv->visible_start = -1;
	priv->visible_end = -1;
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
		GList *l;
		int i;
		
		/* Clean up the clients */
		for (i = 0; i < E_CAL_SOURCE_TYPE_LAST; i++) {
			for (l = priv->clients_list[i]; l != NULL; l = l->next) {
				g_signal_handlers_disconnect_matched (l->data, G_SIGNAL_MATCH_DATA,
								      0, 0, NULL, NULL, gcal);
			}

			g_hash_table_destroy (priv->clients[i]);
			g_list_free (priv->clients_list[i]);

			priv->clients[i] = NULL;
			priv->clients_list[i] = NULL;

			if (priv->default_client[i]) {
				g_signal_handlers_disconnect_matched (priv->default_client[i], 
								      G_SIGNAL_MATCH_DATA,
								      0, 0, NULL, NULL, gcal);
				g_object_unref (priv->default_client[i]);
			}		
			priv->default_client[i] = NULL;
		}
		
		for (i = 0; i < E_CAL_SOURCE_TYPE_LAST; i++) {
			free_categories (priv->categories[i]);
			priv->categories[i] = NULL;
		}

		for (i = 0; i < GNOME_CAL_LAST_VIEW; i++) {
			if (priv->configs[i])
				g_object_unref (priv->configs[i]);
			priv->configs[i] = NULL;
		}
		g_object_unref (priv->date_navigator_config);
		g_object_unref (priv->todo_config);
		
		for (l = priv->notifications; l; l = l->next)
			calendar_config_remove_notification (GPOINTER_TO_UINT (l->data));
		g_list_free (priv->notifications);
		priv->notifications = NULL;
		
		/* Save the TaskPad layout. */
		filename = g_build_filename (calendar_component_peek_config_directory (calendar_component_peek ()),
					     "TaskPad", NULL);
		e_calendar_table_save_state (E_CALENDAR_TABLE (priv->todo), filename);
		g_free (filename);

		if (priv->dn_queries) {
			for (l = priv->dn_queries; l != NULL; l = l->next) {
				g_signal_handlers_disconnect_matched ((ECalView *) l->data, G_SIGNAL_MATCH_DATA,
								      0, 0, NULL, NULL, gcal);
				g_object_unref (l->data);
			}

			g_list_free (priv->dn_queries);
			priv->dn_queries = NULL;
		}

		if (priv->sexp) {
			g_free (priv->sexp);
			priv->sexp = NULL;
		}
		
		if (priv->todo_sexp) {
			g_free (priv->todo_sexp);
			priv->todo_sexp = NULL;
		}

		if (priv->update_timeout) {
			g_source_remove (priv->update_timeout);
			priv->update_timeout = 0;
		}

		if (priv->view_instance) {
			g_object_unref (priv->view_instance);
			priv->view_instance = NULL;
		}

		if (priv->view_menus) {
			g_object_unref (priv->view_menus);
			priv->view_menus = NULL;
		}

		if (priv->calendar_menu) {
			g_object_unref (priv->calendar_menu);
			priv->calendar_menu = NULL;
		}

		if (priv->taskpad_menu) {
			g_object_unref (priv->taskpad_menu);
			priv->taskpad_menu = NULL;
		}

		g_free (priv);
		gcal->priv = NULL;
	}
	
	if (GTK_OBJECT_CLASS (gnome_calendar_parent_class)->destroy)
		(* GTK_OBJECT_CLASS (gnome_calendar_parent_class)->destroy) (object);
}

static void
gnome_calendar_goto_date (GnomeCalendar *gcal,
			  GnomeCalendarGotoDateType goto_date)
{
	GnomeCalendarPrivate *priv;
	time_t	 new_time = 0; 
	gboolean need_updating = FALSE;

	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR(gcal));

	priv = gcal->priv;

	switch (goto_date) {
		/* GNOME_CAL_GOTO_TODAY and GNOME_CAL_GOTO_DATE are
		   currently not used
		*/
	case GNOME_CAL_GOTO_TODAY:
		break;
	case GNOME_CAL_GOTO_DATE:
		break;
	case GNOME_CAL_GOTO_FIRST_DAY_OF_MONTH:
		new_time = time_month_begin_with_zone (priv->base_view_time, priv->zone);
		need_updating = TRUE;
		break;
	case GNOME_CAL_GOTO_LAST_DAY_OF_MONTH:
		new_time = time_add_month_with_zone (priv->base_view_time, 1, priv->zone);
		new_time = time_month_begin_with_zone (new_time, priv->zone);
		new_time = time_add_day_with_zone (new_time, -1, priv->zone);
		need_updating = TRUE;
		break;
	case GNOME_CAL_GOTO_FIRST_DAY_OF_WEEK:
		new_time = time_week_begin_with_zone (priv->base_view_time, priv->week_start, priv->zone);
		need_updating = TRUE;
		break;
	case GNOME_CAL_GOTO_LAST_DAY_OF_WEEK:
		new_time = time_week_begin_with_zone (priv->base_view_time, priv->week_start, priv->zone);
		if (priv->current_view_type == GNOME_CAL_DAY_VIEW ||
		    priv->current_view_type == GNOME_CAL_WORK_WEEK_VIEW) {
			/* FIXME Shouldn't hard code work week end */
			/* goto Friday of this week */
			new_time = time_add_day_with_zone (new_time, 4, priv->zone);
		} else {
			/* goto Sunday of this week */
			/* FIXME Shouldn't hard code week end */
			new_time = time_add_day_with_zone (new_time, 6, priv->zone);
		}
		need_updating = TRUE;
		break;
	case GNOME_CAL_GOTO_SAME_DAY_OF_PREVIOUS_WEEK:
		new_time = time_add_week_with_zone (priv->base_view_time, -1, priv->zone);
		need_updating = TRUE;
		break;
	case GNOME_CAL_GOTO_SAME_DAY_OF_NEXT_WEEK:
		new_time = time_add_week_with_zone (priv->base_view_time, 1, priv->zone);
		need_updating = TRUE;
		break;
	default:
		break;
	}

	if (need_updating) {
		update_view_times (gcal, new_time);
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

	update_view_times (gcal, new_time);
	gnome_calendar_update_date_navigator (gcal);
	gnome_calendar_notify_dates_shown_changed (gcal);
}


static void
update_view_times (GnomeCalendar *gcal, time_t start_time)
{
	GnomeCalendarPrivate *priv;
	int i;
	
	priv = gcal->priv;
	
	priv->base_view_time = start_time;

	for (i = 0; i < GNOME_CAL_LAST_VIEW; i++) {
		ECalModel *model;
		time_t real_start_time = start_time;
		time_t end_time;

		model = e_calendar_view_get_model (priv->views[i]);
		get_times_for_views (gcal, i, &real_start_time, &end_time);

 		e_cal_model_set_time_range (model, real_start_time, end_time);
	}
}

static void
gnome_calendar_direction (GnomeCalendar *gcal, int direction)
{
	GnomeCalendarPrivate *priv;

	priv = gcal->priv;

	switch (priv->current_view_type) {
	case GNOME_CAL_DAY_VIEW:
		priv->base_view_time = time_add_day_with_zone (priv->base_view_time, direction, priv->zone);
		break;
	case GNOME_CAL_WORK_WEEK_VIEW:
	case GNOME_CAL_WEEK_VIEW:
		priv->base_view_time = time_add_week_with_zone (priv->base_view_time, direction, priv->zone);
		break;
	case GNOME_CAL_LIST_VIEW:
		g_warning ("Using month view time interval for list view.");
	case GNOME_CAL_MONTH_VIEW:
		priv->base_view_time = time_add_month_with_zone (priv->base_view_time, direction, priv->zone);
		break;
	default:
		g_assert_not_reached ();
		return;
	}

	update_view_times (gcal, priv->base_view_time);
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

	priv->base_view_time = time_day_begin_with_zone (time, priv->zone);

	update_view_times (gcal, priv->base_view_time);
	gnome_calendar_set_view (gcal, GNOME_CAL_DAY_VIEW);
}

static void
focus_current_view (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;

	priv = gcal->priv;

	gtk_widget_grab_focus (gnome_calendar_get_current_view_widget (gcal));
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

static void
set_view (GnomeCalendar *gcal, GnomeCalendarViewType view_type, gboolean range_selected)
{
	GnomeCalendarPrivate *priv;
	const char *view_id;

	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	priv = gcal->priv;

	switch (view_type) {
	case GNOME_CAL_DAY_VIEW:
		view_id = "Day_View";
		break;

	case GNOME_CAL_WORK_WEEK_VIEW:
		view_id = "Work_Week_View";
		break;

	case GNOME_CAL_WEEK_VIEW:
		view_id = "Week_View";
		break;

	case GNOME_CAL_MONTH_VIEW:
		view_id = "Month_View";
		break;

	case GNOME_CAL_LIST_VIEW:
		view_id = "List_View";
		break;

	default:
		g_assert_not_reached ();
		return;
	}

	priv->range_selected = range_selected;
	priv->current_view_type = view_type;

	gal_view_instance_set_current_view_id (priv->view_instance, view_id);
	focus_current_view (gcal);
}


/**
 * gnome_calendar_set_view:
 * @gcal: A calendar.
 * @view_type: Type of view to show.
 *
 * Sets the view that should be shown in a calendar.  If @reset_range is true,
 * this function will automatically set the number of days or weeks shown in
 * the view; otherwise the last configuration will be kept.
 **/
void
gnome_calendar_set_view (GnomeCalendar *gcal, GnomeCalendarViewType view_type)
{
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	set_view (gcal, view_type, FALSE);
}

/* Sets the view without changing the selection or updating the date
 * navigator. If a range of dates isn't selected it will also reset the number
 * of days/weeks shown to the default (i.e. 1 day for the day view or 5 weeks
 * for the month view).
 */
static void
display_view (GnomeCalendar *gcal, GnomeCalendarViewType view_type, gboolean grab_focus)
{
	GnomeCalendarPrivate *priv;
	gboolean preserve_day;

	priv = gcal->priv;

	preserve_day = FALSE;

	switch (view_type) {
	case GNOME_CAL_DAY_VIEW:
		if (!priv->range_selected)
			e_day_view_set_days_shown (E_DAY_VIEW (priv->day_view), 1);

		break;

	case GNOME_CAL_WORK_WEEK_VIEW:
		preserve_day = TRUE;
		break;

	case GNOME_CAL_WEEK_VIEW:
		preserve_day = TRUE;
		break;

	case GNOME_CAL_MONTH_VIEW:
		if (!priv->range_selected)
			e_week_view_set_weeks_shown (E_WEEK_VIEW (priv->month_view), 6);

		preserve_day = TRUE;
		break;

	case GNOME_CAL_LIST_VIEW:
		break;

	default:
		g_assert_not_reached ();
		return;
	}

	priv->current_view_type = view_type;

	gtk_notebook_set_page (GTK_NOTEBOOK (priv->notebook), (int) view_type);

	if (grab_focus)
		focus_current_view (gcal);

	gnome_calendar_set_pane_positions (gcal);

	/* For the week & month views we want the selection in the date
	   navigator to be rounded to the nearest week when the arrow buttons
	   are pressed to move to the previous/next month. */
	g_object_set (G_OBJECT (priv->date_navigator->calitem),
		      "preserve_day_when_moving", preserve_day,
		      NULL);
}

/* Callback used when the view collection asks us to display a particular view */
static void
display_view_cb (GalViewInstance *view_instance, GalView *view, gpointer data)
{
	GnomeCalendar *gcal;
	GnomeCalendarPrivate *priv;
	CalendarView *cal_view;
	GnomeCalendarViewType view_type;
	
	gcal = GNOME_CALENDAR (data);
	priv = gcal->priv;
	
	if (GAL_IS_VIEW_ETABLE(view)) {
		ETable *table;
		
		view_type = GNOME_CAL_LIST_VIEW;
		
		table = e_table_scrolled_get_table (E_CAL_LIST_VIEW (priv->list_view)->table_scrolled);
		gal_view_etable_attach_table (GAL_VIEW_ETABLE (view), table);
	} else if (IS_CALENDAR_VIEW (view)) {
		cal_view = CALENDAR_VIEW (view);
		
		view_type = calendar_view_get_view_type (cal_view);
	} else {
		g_error (G_STRLOC ": Unknown type of view for GnomeCalendar");
		return;
	}

	
	display_view (gcal, view_type, TRUE);
	gnome_calendar_update_date_navigator (gcal);
	gnome_calendar_notify_dates_shown_changed (gcal);
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
	GalViewFactory *gal_factory;
	static GalViewCollection *collection = NULL;

	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (uic != NULL);
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (uic));

	priv = gcal->priv;

	g_assert (priv->view_instance == NULL);
	g_assert (priv->view_menus == NULL);

	/* Create the view instance */
	if (collection == NULL) {
		ETableSpecification *spec;

		collection = gal_view_collection_new ();

		gal_view_collection_set_title (collection, _("Calendar"));

		path = g_build_filename (calendar_component_peek_base_directory (calendar_component_peek ()), 
					 "calendar", "views", NULL);
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

		spec = e_table_specification_new ();
		e_table_specification_load_from_file (spec, EVOLUTION_ETSPECDIR "/e-cal-list-view.etspec");
		gal_factory = gal_view_factory_etable_new (spec);
		g_object_unref (spec);
		gal_view_collection_add_factory (collection, GAL_VIEW_FACTORY (gal_factory));
		g_object_unref (gal_factory);

		/* Load the collection and create the menus */

		gal_view_collection_load (collection);

	}

	priv->view_instance = gal_view_instance_new (collection, NULL);
	priv->view_menus = gal_view_menus_new (priv->view_instance);
	gal_view_menus_apply (priv->view_menus, uic, NULL);

	g_signal_connect (priv->view_instance, "display_view", G_CALLBACK (display_view_cb), gcal);
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

	g_assert (priv->view_instance != NULL);
	g_assert (priv->view_menus != NULL);

	g_object_unref (priv->view_instance);
	priv->view_instance = NULL;

	g_object_unref (priv->view_menus);
	priv->view_menus = NULL;
}

/* This is copied/moved from gal-view-instance, only the calendar uses this for a popup menu */
static void
gc_set_view(EPopup *ep, EPopupItem *pitem, void *data)
{
	GnomeCalendar *gcal = data;

	if (pitem->type & E_POPUP_ACTIVE)
		gal_view_instance_set_current_view_id(gcal->priv->view_instance, (char *)pitem->user_data);
}

static void
gc_save_custom_view(EPopup *ep, EPopupItem *pitem, void *data)
{
	GnomeCalendar *gcal = data;

	gal_view_instance_save_as(gcal->priv->view_instance);
}

static void
gc_define_views_response(GtkWidget *d, int id, GnomeCalendar *gcal)
{
	if (id == GTK_RESPONSE_OK)
		gal_view_collection_save(gcal->priv->view_instance->collection);

	gtk_widget_destroy(d);
}

static void
gc_define_views(EPopup *ep, EPopupItem *pitem, void *data)
{
	GnomeCalendar *gcal = data;
	GtkWidget *dialog = gal_define_views_dialog_new(gcal->priv->view_instance->collection);

	g_signal_connect(dialog, "response", G_CALLBACK(gc_define_views_response), data);
	gtk_widget_show(dialog);
}

static EPopupItem gc_popups[] = {
	/* Code generates the path to fit */
	{ E_POPUP_BAR, NULL },
	{ E_POPUP_RADIO|E_POPUP_ACTIVE, NULL, N_("Custom View"), },
	{ E_POPUP_ITEM, NULL, N_("Save Custom View"), gc_save_custom_view },

	/* index == 3, when we have non-custom view */

	{ E_POPUP_BAR, NULL },
	{ E_POPUP_ITEM, NULL, N_("Define Views..."), gc_define_views },
};

static void
gc_popup_free (EPopup *ep, GSList *list, void *data)
{
	while (list) {
		GSList *n = list->next;
		EPopupItem *pitem = list->data;

		g_free(pitem->path);
		g_free(pitem->label);
		g_free(pitem->user_data);
		g_free(pitem);
		g_slist_free_1(list);
		list = n;
	}
}

static void
gc_popup_free_static (EPopup *ep, GSList *list, void *data)
{
	while (list) {
		GSList *n = list->next;
		EPopupItem *pitem = list->data;

		g_free(pitem->path);
		g_free(pitem);
		g_slist_free_1(list);
		list = n;
	}
}

void
gnome_calendar_view_popup_factory (GnomeCalendar *gcal, EPopup *ep, const char *prefix)
{
	GnomeCalendarPrivate *priv;
	int length;
	int i;
	gboolean found = FALSE;
	char *id;
	GSList *menus = NULL;
	EPopupItem *pitem;

	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (prefix != NULL);

	priv = gcal->priv;

	g_return_if_fail (priv->view_instance != NULL);

	length = gal_view_collection_get_count(priv->view_instance->collection);
	id = gal_view_instance_get_current_view_id (priv->view_instance);

	for (i = 0; i < length; i++) {
		GalViewCollectionItem *item = gal_view_collection_get_view_item(priv->view_instance->collection, i);

		pitem = g_malloc0(sizeof(*pitem));
		pitem->type = E_POPUP_RADIO;
		pitem->path = g_strdup_printf("%s/%02d.item", prefix, i);
		pitem->label = g_strdup(item->title);
		pitem->activate = gc_set_view;
		pitem->user_data = g_strdup(item->id);

		if (!found && id && !strcmp (id, item->id)) {
			found = TRUE;
			pitem->type |= E_POPUP_ACTIVE;
		}

		menus = g_slist_prepend(menus, pitem);
	}

	if (menus)
		e_popup_add_items(ep, menus, gc_popup_free, gcal);

	menus = NULL;
	for (i = found?3:0; i<sizeof(gc_popups)/sizeof(gc_popups[0]);i++) {
		pitem = g_malloc0(sizeof(*pitem));
		memcpy(pitem, &gc_popups[i], sizeof(*pitem));
		pitem->path = g_strdup_printf("%s/%02d.item", prefix, i+length);
		menus = g_slist_prepend(menus, pitem);
	}

	e_popup_add_items(ep, menus, gc_popup_free_static, gcal);
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

static void
client_cal_opened_cb (ECal *ecal, ECalendarStatus status, GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	ECalSourceType source_type;
	ESource *source;
	char *msg;
	int i;

	priv = gcal->priv;

	source_type = e_cal_get_source_type (ecal);
	source = e_cal_get_source (ecal);

	if (source_type == E_CAL_SOURCE_TYPE_EVENT)
		e_calendar_view_set_status_message (E_CALENDAR_VIEW (priv->week_view), NULL);
	else
		e_calendar_table_set_status_message (E_CALENDAR_TABLE (priv->todo), NULL);

	if (status == E_CALENDAR_STATUS_BUSY)
		return;
	if (status != E_CALENDAR_STATUS_OK) {
		/* Make sure the source doesn't disappear on us */
		g_object_ref (source);
		
		priv->clients_list[source_type] = g_list_remove (priv->clients_list[source_type], ecal);
		g_hash_table_remove (priv->clients[source_type], e_source_peek_uid (source));

		gtk_signal_emit (GTK_OBJECT (gcal), gnome_calendar_signals[SOURCE_REMOVED], source_type, source);
		
		g_object_unref (source);
		
		return;
	}

	g_signal_handlers_disconnect_matched (ecal, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, client_cal_opened_cb, NULL);

	e_cal_set_default_timezone (ecal, priv->zone, NULL);

	switch (source_type) {
	case E_CAL_SOURCE_TYPE_EVENT :
		msg = g_strdup_printf (_("Loading appointments at %s"), e_cal_get_uri (ecal));
		e_calendar_view_set_status_message (E_CALENDAR_VIEW (priv->week_view), msg);
		g_free (msg);

		/* add client to the views */
		for (i = 0; i < GNOME_CAL_LAST_VIEW; i++) {
			ECalModel *model;
	
			model = e_calendar_view_get_model (priv->views[i]);
			e_cal_model_add_client (model, ecal);
		}

		/* update date navigator query */
		update_query (gcal);

		e_calendar_view_set_status_message (E_CALENDAR_VIEW (priv->week_view), NULL);
		break;
		
	case E_CAL_SOURCE_TYPE_TODO :
		msg = g_strdup_printf (_("Loading tasks at %s"), e_cal_get_uri (ecal));
		e_calendar_table_set_status_message (E_CALENDAR_TABLE (priv->todo), msg);
		g_free (msg);

		e_cal_model_add_client (e_calendar_table_get_model (E_CALENDAR_TABLE (priv->todo)), ecal);

		e_calendar_table_set_status_message (E_CALENDAR_TABLE (priv->todo), NULL);
		break;
		
	default:
		g_assert_not_reached ();
		return;
	}
}

static void
default_client_cal_opened_cb (ECal *ecal, ECalendarStatus status, GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	ECalSourceType source_type;
	ESource *source;
	int i;

	priv = gcal->priv;

	source_type = e_cal_get_source_type (ecal);
	source = e_cal_get_source (ecal);

	switch (source_type) {
	case E_CAL_SOURCE_TYPE_EVENT:
		e_calendar_view_set_status_message (E_CALENDAR_VIEW (priv->week_view), NULL);
		break;		
	case E_CAL_SOURCE_TYPE_TODO:
		e_calendar_table_set_status_message (E_CALENDAR_TABLE (priv->todo), NULL);
		break;
	default:
		break;
	}
	
	if (status == E_CALENDAR_STATUS_BUSY)
		return;

	if (status != E_CALENDAR_STATUS_OK) {
		/* Make sure the source doesn't disappear on us */
		g_object_ref (source);
		
		/* FIXME should we do this to prevent multiple error dialogs? */
		priv->clients_list[source_type] = g_list_remove (priv->clients_list[source_type], ecal);
		g_hash_table_remove (priv->clients[source_type], e_source_peek_uid (source));

		/* FIXME Is there a better way to handle this? */
		g_object_unref (priv->default_client[source_type]);
		priv->default_client[source_type] = NULL;
		
		gtk_signal_emit (GTK_OBJECT (gcal), gnome_calendar_signals[SOURCE_REMOVED], source_type, source);
		
		g_object_unref (source);
		
		return;
	}

	g_signal_handlers_disconnect_matched (ecal, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, default_client_cal_opened_cb, NULL);

	e_cal_set_default_timezone (ecal, priv->zone, NULL);
	
	switch (source_type) {
	case E_CAL_SOURCE_TYPE_EVENT:
		for (i = 0; i < GNOME_CAL_LAST_VIEW; i++) {
			e_cal_model_set_default_client (
				e_calendar_view_get_model (E_CALENDAR_VIEW (priv->views[i])),
				ecal);
		}
		break;

	case E_CAL_SOURCE_TYPE_TODO:
		e_cal_model_set_default_client (e_calendar_table_get_model (E_CALENDAR_TABLE (priv->todo)), ecal);
		break;

	default:
		break;
        }
}

typedef void (*open_func) (ECal *, ECalendarStatus, GnomeCalendar *);

static gboolean
open_ecal (GnomeCalendar *gcal, ECal *cal, gboolean only_if_exists, open_func of)
{
	GnomeCalendarPrivate *priv;
	char *msg;

	priv = gcal->priv;

	msg = g_strdup_printf (_("Opening %s"), e_cal_get_uri (cal));
	switch (e_cal_get_source_type (cal)) {
	case E_CAL_SOURCE_TYPE_EVENT :
		e_calendar_view_set_status_message (E_CALENDAR_VIEW (priv->week_view), msg);
		break;
	case E_CAL_SOURCE_TYPE_TODO :
		e_calendar_table_set_status_message (E_CALENDAR_TABLE (priv->todo), msg);
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	g_free (msg);

	g_signal_connect (G_OBJECT (cal), "cal_opened", G_CALLBACK (of), gcal);
	e_cal_open_async (cal, only_if_exists);

	return TRUE;
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

/* Appends a category from the hash table to the array */
static void
append_category_cb (gpointer key, gpointer value, gpointer data)
{
	GPtrArray *c;
	const char *category;

	category = key;
	c = data;

	g_ptr_array_set_size (c, c->len + 1);	
	c->pdata[c->len - 1] = g_strdup (category);
	
}

/* Callback from the calendar client when the set of categories changes.  We
 * have to merge the categories of the calendar and tasks clients.
 */
static void
client_categories_changed_cb (ECal *ecal, GPtrArray *categories, gpointer data)
{
	GnomeCalendar *gcal;
	GnomeCalendarPrivate *priv;
	ECalSourceType source_type;
	GHashTable *cat_hash;
	GPtrArray *merged;
	int i;
	
	gcal = GNOME_CALENDAR (data);
	priv = gcal->priv;

	source_type = e_cal_get_source_type (ecal);

	free_categories (priv->categories[source_type]);
	priv->categories[source_type] = copy_categories (categories);

	/* Build a non-duplicate list of the categories */
	cat_hash = g_hash_table_new (g_str_hash, g_str_equal);
	for (i = 0; i < E_CAL_SOURCE_TYPE_LAST; i++) {
		add_categories (cat_hash, priv->categories[i]);
	}	

	/* Build the pointer array */
	/* We size it maximally and then to 0 to pre-allocate memory */
	merged = g_ptr_array_sized_new (g_hash_table_size (cat_hash));
	g_ptr_array_set_size (merged, 0);

	g_hash_table_foreach (cat_hash, append_category_cb, merged);
	g_hash_table_destroy (cat_hash);

	cal_search_bar_set_categories (CAL_SEARCH_BAR (priv->search_bar), merged);
	free_categories (merged);
}

/* Callback when we get an error message from the backend */
static void
backend_error_cb (ECal *client, const char *message, gpointer data)
{
	GnomeCalendar *gcal;
	GnomeCalendarPrivate *priv;
	char *errmsg;
	char *uristr;

	gcal = GNOME_CALENDAR (data);
	priv = gcal->priv;

	uristr = get_uri_without_password (e_cal_get_uri (client));
	errmsg = g_strdup_printf (_("Error on %s:\n %s"), uristr, message);
	gnome_error_dialog_parented (errmsg, GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (gcal))));
	g_free (errmsg);
	g_free (uristr);
}

/* Callback when the backend dies */
static void
backend_died_cb (ECal *ecal, gpointer data)
{
	GnomeCalendar *gcal;
	GnomeCalendarPrivate *priv;
	ECalSourceType source_type;
	ESource *source;
	const char *id;

	gcal = GNOME_CALENDAR (data);
	priv = gcal->priv;

	/* FIXME What about default sources? */

	/* Make sure the source doesn't go away on us since we use it below */
	source_type = e_cal_get_source_type (ecal);
	source = g_object_ref (e_cal_get_source (ecal));	
		
	priv->clients_list[source_type] = g_list_remove (priv->clients_list[source_type], ecal);
	g_hash_table_remove (priv->clients[source_type], e_source_peek_uid (source));

	switch (source_type) {
	case E_CAL_SOURCE_TYPE_EVENT:		
		id = "calendar:calendar-crashed";
		
		e_calendar_view_set_status_message (E_CALENDAR_VIEW (priv->week_view), NULL);

		gtk_signal_emit (GTK_OBJECT (gcal), gnome_calendar_signals[SOURCE_REMOVED], source_type, source);
		break;

	case E_CAL_SOURCE_TYPE_TODO:
		id = "calendar:tasks-crashed";
		
		e_calendar_table_set_status_message (E_CALENDAR_TABLE (priv->todo), NULL);

		gtk_signal_emit (GTK_OBJECT (gcal), gnome_calendar_signals[SOURCE_REMOVED], source_type, source);
		break;

	default:
		g_assert_not_reached ();
		return;
	}

	g_object_unref (source);
	
	e_error_run (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (gcal))), id, NULL);
}

GtkWidget *
gnome_calendar_construct (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;

	g_return_val_if_fail (gcal != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), NULL);

	priv = gcal->priv;

	return GTK_WIDGET (gcal);
}

GtkWidget *
gnome_calendar_new (void)
{
	GnomeCalendar *gcal;

	gcal = g_object_new (gnome_calendar_get_type (), NULL);

	if (!gnome_calendar_construct (gcal)) {
		g_message (G_STRLOC ": Could not construct the calendar GUI");
		g_object_unref (gcal);
		return NULL;
	}

	return GTK_WIDGET (gcal);
}

void 
gnome_calendar_set_activity_handler (GnomeCalendar *cal, EActivityHandler *activity_handler)
{
	GnomeCalendarPrivate *priv;
	int i;
	
	g_return_if_fail (cal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (cal));

	priv = cal->priv;

	priv->activity_handler = activity_handler;
	
	for (i = 0; i < GNOME_CAL_LAST_VIEW; i++)
		e_calendar_view_set_activity_handler (priv->views[i], activity_handler);
	
	e_calendar_table_set_activity_handler (E_CALENDAR_TABLE (priv->todo), activity_handler);
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
 * gnome_calendar_get_calendar_model:
 * @gcal: A calendar view.
 *
 * Queries the calendar model object that a calendar view is using.
 *
 * Return value: A calendar client interface object.
 **/
ECalModel *
gnome_calendar_get_calendar_model (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;

	g_return_val_if_fail (gcal != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), NULL);

	priv = gcal->priv;

	return e_calendar_view_get_model (priv->views[priv->current_view_type]);
}

/**
 * gnome_calendar_get_default_client
 */
ECal *
gnome_calendar_get_default_client (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;

	g_return_val_if_fail (gcal != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), NULL);

	priv = gcal->priv;

	return e_cal_model_get_default_client (e_calendar_view_get_model (gcal->priv->views[priv->current_view_type]));
}

/**
 * gnome_calendar_add_source:
 * @gcal: A GnomeCalendar.
 * @source: #ESource to add to the calendar views.
 *
 * Adds the given calendar source to the calendar views.
 *
 * Returns: TRUE if successful, FALSE if error.
 */
gboolean
gnome_calendar_add_source (GnomeCalendar *gcal, ECalSourceType source_type, ESource *source)
{
	GnomeCalendarPrivate *priv;
	ECal *client;
	
	g_return_val_if_fail (gcal != NULL, FALSE);
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);

	priv = gcal->priv;

	client = g_hash_table_lookup (priv->clients[source_type], e_source_peek_uid (source));
	if (client) {
		/* We already have it */

		return TRUE;
	} else {
		ESource *default_source;
		
		if (priv->default_client[source_type]) {
			default_source = e_cal_get_source (priv->default_client[source_type]);

			g_message ("Check if default client matches (%s %s)", e_source_peek_uid (default_source), e_source_peek_uid (source));
			/* We don't have it but the default client is it */
			if (!strcmp (e_source_peek_uid (default_source), e_source_peek_uid (source)))
				client = g_object_ref (priv->default_client[source_type]);
		}

		/* Create a new one */		
		if (!client) {
			client = auth_new_cal_from_source (source, source_type);
			if (!client)
				return FALSE;
		}
	}
	
	g_signal_connect (G_OBJECT (client), "backend_error", G_CALLBACK (backend_error_cb), gcal);
	g_signal_connect (G_OBJECT (client), "categories_changed", G_CALLBACK (client_categories_changed_cb), gcal);
	g_signal_connect (G_OBJECT (client), "backend_died", G_CALLBACK (backend_died_cb), gcal);

	/* add the client to internal structure */
	g_hash_table_insert (priv->clients[source_type], g_strdup (e_source_peek_uid (source)), client);
	priv->clients_list[source_type] = g_list_prepend (priv->clients_list[source_type], client);

	gtk_signal_emit (GTK_OBJECT (gcal), gnome_calendar_signals[SOURCE_ADDED], source_type, source);

	open_ecal (gcal, client, FALSE, client_cal_opened_cb);

	return TRUE;
}

/**
 * gnome_calendar_remove_source
 * @gcal: A #GnomeCalendar.
 * @source: #ESource to be removed from the clients.
 *
 * Removes the given source from the list of clients being shown by the
 * calendar views.
 *
 * Returns: TRUE if successful, FALSE otherwise.
 */
gboolean
gnome_calendar_remove_source (GnomeCalendar *gcal, ECalSourceType source_type, ESource *source)
{
	gboolean result;
	
	g_return_val_if_fail (gcal != NULL, FALSE);
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);

	result = gnome_calendar_remove_source_by_uid (gcal, source_type, e_source_peek_uid (source));
	if (result)
		gtk_signal_emit (GTK_OBJECT (gcal), gnome_calendar_signals[SOURCE_REMOVED], source_type, source);

	return result;
}

gboolean
gnome_calendar_remove_source_by_uid (GnomeCalendar *gcal, ECalSourceType source_type, const char *uid)
{
	GnomeCalendarPrivate *priv;
	ECal *client;
	ECalModel *model;
	int i;
	GList *l;

	g_return_val_if_fail (gcal != NULL, FALSE);
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), FALSE);
	g_return_val_if_fail (uid != NULL, FALSE);

	priv = gcal->priv;

	client = g_hash_table_lookup (priv->clients[source_type], uid);
	if (!client)
		return TRUE;

	priv->clients_list[source_type] = g_list_remove (priv->clients_list[source_type], client);
	g_signal_handlers_disconnect_matched (client, G_SIGNAL_MATCH_DATA,
					      0, 0, NULL, NULL, gcal);	

	switch (source_type) {
	case E_CAL_SOURCE_TYPE_EVENT:
		/* remove the query for this client */
		for (l = priv->dn_queries; l != NULL; l = l->next) {
			ECalView *query = l->data;

			if (query && (client == e_cal_view_get_client (query))) {
				g_signal_handlers_disconnect_matched (query, G_SIGNAL_MATCH_DATA,
								      0, 0, NULL, NULL, gcal);
				priv->dn_queries = g_list_remove (priv->dn_queries, query);
				g_object_unref (query);
				break;
			}
		}

		for (i = 0; i < GNOME_CAL_LAST_VIEW; i++) {
			model = e_calendar_view_get_model (priv->views[i]);
			e_cal_model_remove_client (model, client);
		}

		/* update date navigator query */
		update_query (gcal);
		break;
		
	case E_CAL_SOURCE_TYPE_TODO:
		model = e_calendar_table_get_model (E_CALENDAR_TABLE (priv->todo));
		e_cal_model_remove_client (model, client);
		break;
		
	default:
		g_assert_not_reached ();
		break;
	}
	
	g_hash_table_remove (priv->clients[source_type], uid);

	return TRUE;
}

/**
 * gnome_calendar_set_default_source:
 * @gcal: A calendar view
 * @source: The #ESource to use as default
 * 
 * Set the default uri on the given calendar view, the default uri
 * will be used as the default when creating events in the view.

 * 
 * Return value: TRUE if the uri was already added and is set, FALSE
 * otherwise
 **/
gboolean
gnome_calendar_set_default_source (GnomeCalendar *gcal, ECalSourceType source_type, ESource *source)
{
	GnomeCalendarPrivate *priv;
	ECal *client;
	
	g_return_val_if_fail (gcal != NULL, FALSE);
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);

	priv = gcal->priv;

	client = g_hash_table_lookup (priv->clients[source_type], e_source_peek_uid (source));

	if (priv->default_client[source_type])
		g_object_unref (priv->default_client[source_type]);

	if (client) {
		priv->default_client[source_type] = g_object_ref (client);
	} else {
		priv->default_client[source_type] = auth_new_cal_from_source (source, source_type);
		if (!priv->default_client[source_type])
			return FALSE;
	}

	open_ecal (gcal, priv->default_client[source_type], FALSE, default_client_cal_opened_cb);

	return TRUE;
}

void
gnome_calendar_set_selected_time_range (GnomeCalendar *gcal,
					time_t	       start_time,
					time_t	       end_time)
{
	GnomeCalendarPrivate *priv;

	priv = gcal->priv;

	update_view_times (gcal, start_time);
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
	ECalModel *model;
	
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	priv = gcal->priv;

	model = e_calendar_view_get_model (priv->views[priv->current_view_type]);
	e_cal_model_get_time_range (model, start_time, end_time);
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
	ECal *ecal;
	ECalModel *model;
	TaskEditor *tedit;
	ECalComponent *comp;
	icalcomponent *icalcomp;
	const char *category;
	
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	priv = gcal->priv;

	model = e_calendar_table_get_model (E_CALENDAR_TABLE (priv->todo));
	ecal = e_cal_model_get_default_client (model);
	if (!ecal)
		return;
	
	tedit = task_editor_new (ecal);

	icalcomp = e_cal_model_create_component_with_defaults (model);
	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomp);

	category = cal_search_bar_get_category (CAL_SEARCH_BAR (priv->search_bar));
	e_cal_component_set_categories (comp, category);

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

	e_calendar_view_get_selected_time_range (E_CALENDAR_VIEW (gnome_calendar_get_current_view_widget (gcal)),
					    start_time, end_time);
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

	retval = e_calendar_view_get_visible_time_range (E_CALENDAR_VIEW (gnome_calendar_get_current_view_widget (gcal)),
						    start_time, end_time);

	return retval;
}

/* This updates the month shown and the days selected in the calendar, if
   necessary. */
static void
gnome_calendar_update_date_navigator (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	ECalModel *model;
	time_t start, end;
	GDate start_date, end_date;

	priv = gcal->priv;

	/* If the ECalendar isn't visible, we just return. */
	if (!GTK_WIDGET_VISIBLE (priv->date_navigator))
		return;

	model = e_calendar_view_get_model (priv->views[priv->current_view_type]);
	e_cal_model_get_time_range (model, &start, &end);
	
	time_to_gdate_with_zone (&start_date, start, priv->zone);
	if (priv->current_view_type == GNOME_CAL_MONTH_VIEW) {
		EWeekView *week_view = E_WEEK_VIEW (priv->views[priv->current_view_type]);

		if (priv->week_start == 0 
		    && (!week_view->multi_week_view || week_view->compress_weekend))
			g_date_add_days (&start_date, 1);
	}
	time_to_gdate_with_zone (&end_date, end, priv->zone);
	g_date_subtract_days (&end_date, 1);
	
	e_calendar_item_set_selection (priv->date_navigator->calitem,
				       &start_date, &end_date);
}

static void
gnome_calendar_on_date_navigator_selection_changed (ECalendarItem *calitem, GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	GnomeCalendarViewType view_type;
	ECalModel *model;
	GDate start_date, end_date, new_start_date, new_end_date;
	gint new_days_shown;
	gboolean starts_on_week_start_day;
	time_t new_time, start, end;
	struct icaltimetype tt;

	priv = gcal->priv;
	
	starts_on_week_start_day = FALSE;

	model = e_calendar_view_get_model (priv->views[priv->current_view_type]);
	e_cal_model_get_time_range (model, &start, &end);
	
	time_to_gdate_with_zone (&start_date, start, priv->zone);
	if (priv->current_view_type == GNOME_CAL_MONTH_VIEW) {
		EWeekView *week_view = E_WEEK_VIEW (priv->views[priv->current_view_type]);

		if (priv->week_start == 0 && (!week_view->multi_week_view || week_view->compress_weekend))
			g_date_add_days (&start_date, 1);
	}
	time_to_gdate_with_zone (&end_date, end, priv->zone);
	g_date_subtract_days (&end_date, 1);

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
	if (g_date_weekday (&new_start_date) % 7 == priv->week_start)
		starts_on_week_start_day = TRUE;

	/* Update selection to be in the new time range */
	tt = icaltime_null_time ();
	tt.year = g_date_year (&new_start_date);
	tt.month  = g_date_month (&new_start_date);
	tt.day = g_date_day (&new_start_date);
	new_time = icaltime_as_timet_with_zone (tt, priv->zone);
	
	/* Switch views as appropriate, and change the number of days or weeks
	   shown. */
	if (new_days_shown > 9) {
		/* FIXME Gross hack so that the view times are updated properly */
		priv->range_selected = TRUE;

		e_week_view_set_weeks_shown (E_WEEK_VIEW (priv->month_view),
					     (new_days_shown + 6) / 7);
		view_type = GNOME_CAL_MONTH_VIEW;
	} else if (new_days_shown == 7 && starts_on_week_start_day) {
		view_type = GNOME_CAL_WEEK_VIEW;
	} else {		
		e_day_view_set_days_shown (E_DAY_VIEW (priv->day_view), new_days_shown);
		
		if (new_days_shown == 5 && starts_on_week_start_day 
		    && priv->current_view_type == GNOME_CAL_WORK_WEEK_VIEW)
			view_type = GNOME_CAL_WORK_WEEK_VIEW;
		else
			view_type = GNOME_CAL_DAY_VIEW;
	}

	/* Make the views display things properly */
	update_view_times (gcal, new_time);

	set_view (gcal, view_type, TRUE);
	gnome_calendar_notify_dates_shown_changed (gcal);
}

static void
gnome_calendar_on_date_navigator_date_range_changed (ECalendarItem *calitem, GnomeCalendar *gcal)
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
		e_calendar_view_cut_clipboard (E_CALENDAR_VIEW (gnome_calendar_get_current_view_widget (gcal)));
	} else if (location == FOCUS_TASKPAD)
		e_calendar_table_cut_clipboard (E_CALENDAR_TABLE (priv->todo));
}

void
gnome_calendar_copy_clipboard (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	FocusLocation location;

	priv = gcal->priv;

	location = get_focus_location (gcal);

	if (location == FOCUS_CALENDAR) {
		e_calendar_view_copy_clipboard (E_CALENDAR_VIEW (gnome_calendar_get_current_view_widget (gcal)));
	} else if (location == FOCUS_TASKPAD)
		e_calendar_table_copy_clipboard (E_CALENDAR_TABLE (priv->todo));
}

void
gnome_calendar_paste_clipboard (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	FocusLocation location;

	priv = gcal->priv;

	location = get_focus_location (gcal);

	if (location == FOCUS_CALENDAR) {
		e_calendar_view_paste_clipboard (E_CALENDAR_VIEW (gnome_calendar_get_current_view_widget (gcal)));
	} else if (location == FOCUS_TASKPAD)
		e_calendar_table_paste_clipboard (E_CALENDAR_TABLE (priv->todo));
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

		e_calendar_view_delete_selected_events (E_CALENDAR_VIEW (view));
	} else if (location == FOCUS_TASKPAD)
		e_calendar_table_delete_selected (E_CALENDAR_TABLE (priv->todo));
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
		e_calendar_view_delete_selected_occurrence (E_CALENDAR_VIEW (view));
	}
}

static gboolean
check_instance_cb (ECalComponent *comp,
		   time_t instance_start,
		   time_t instance_end,
		   gpointer data)
{
	gboolean *remove = data;

	*remove = FALSE;

	return FALSE;
}

void
gnome_calendar_purge (GnomeCalendar *gcal, time_t older_than)
{
	GnomeCalendarPrivate *priv;
	char *sexp, *start, *end;
	GList *l;

	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	priv = gcal->priv;

	start = isodate_from_time_t (0);
	end = isodate_from_time_t (older_than);
	sexp = g_strdup_printf ("(occur-in-time-range? (make-time \"%s\")"
				"                      (make-time \"%s\"))",
				start, end);

	e_calendar_view_set_status_message (E_CALENDAR_VIEW (priv->week_view), _("Purging"));

	/* FIXME Confirm expunge */
	for (l = priv->clients_list[E_CAL_SOURCE_TYPE_EVENT]; l != NULL; l = l->next) {
		ECal *client = l->data;
		GList *objects, *m;
		gboolean read_only;
		
		if (!e_cal_is_read_only (client, &read_only, NULL) || read_only)
			continue;
		
		if (!e_cal_get_object_list (client, sexp, &objects, NULL)) {
			g_warning (G_STRLOC ": Could not get the objects");
			
			continue;
		}
		
		for (m = objects; m; m = m->next) {
			gboolean remove = TRUE;

			/* FIXME write occur-before and occur-after
			 * sexp funcs so we don't have to use the max
			 * int */
			e_cal_generate_instances_for_object (client, m->data,
							     older_than, G_MAXINT32,
							     (ECalRecurInstanceFn) check_instance_cb,
							     &remove);

			/* FIXME Better error handling */
			if (remove)
				e_cal_remove_object (client, icalcomponent_get_uid (m->data), NULL);
		}
	}

	e_calendar_view_set_status_message (E_CALENDAR_VIEW (priv->week_view), NULL);

	g_free (sexp);
	g_free (start);
	g_free (end);

}

ECalendarTable*
gnome_calendar_get_task_pad	(GnomeCalendar *gcal)
{
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), NULL);

	return E_CALENDAR_TABLE (gcal->priv->todo);
}

GtkWidget *
gnome_calendar_get_e_calendar_widget (GnomeCalendar *gcal)
{
 	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), NULL);
 
 	return GTK_WIDGET(gcal->priv->date_navigator);
}
 
GtkWidget *
gnome_calendar_get_search_bar_widget (GnomeCalendar *gcal)
{
 	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), NULL);
 
 	return GTK_WIDGET(gcal->priv->search_bar);
}

GtkWidget *
gnome_calendar_get_view_notebook_widget (GnomeCalendar *gcal)
{
 	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), NULL);
 
 	return GTK_WIDGET(gcal->priv->notebook);
}

ECalMenu *gnome_calendar_get_taskpad_menu (GnomeCalendar *gcal)
{
 	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), NULL);
 
 	return gcal->priv->taskpad_menu;
}

ECalMenu *gnome_calendar_get_calendar_menu (GnomeCalendar *gcal)
{
 	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), NULL);
 
 	return gcal->priv->calendar_menu;
}
