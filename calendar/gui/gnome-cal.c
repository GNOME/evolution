/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar - Main calendar view widget
 *
 * Copyright (C) 1998 The Free Software Foundation
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Miguel de Icaza <miguel@ximian.com>
 *          Federico Mena-Quintero <federico@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <gal/e-paned/e-hpaned.h>
#include <gal/e-paned/e-vpaned.h>
#include <cal-util/timeutil.h>
#include "widgets/misc/e-search-bar.h"
#include "widgets/misc/e-filter-bar.h"
#include "dialogs/alarm-notify-dialog.h"
#include "dialogs/event-editor.h"
#include "e-calendar-table.h"
#include "e-day-view.h"
#include "e-week-view.h"
#include "gnome-cal.h"
#include "component-factory.h"
#include "calendar-commands.h"
#include "calendar-config.h"
#include "tag-calendar.h"



/* These must match the page numbers in the GtkNotebook. */
typedef enum {
	VIEW_NOT_SET = -1,
	VIEW_DAY = 0,
	VIEW_WORK_WEEK,
	VIEW_WEEK,
	VIEW_MONTH
} ViewType;

/* Private part of the GnomeCalendar structure */
struct _GnomeCalendarPrivate {
	/*
	 * The Calendar Folder.
	 */

	/* The calendar client object we monitor */
	CalClient *client;

	/*
	 * The TaskPad Folder.
	 */

	/* The calendar client object we monitor */
	CalClient   *task_pad_client;

	/*
	 * Fields for the calendar view
	 */

	/* Mapping of component UIDs to event editors */
	GHashTable  *object_editor_hash;

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

	/* Whether we are being destroyed and should not mess with the object
	 * editor hash table.
	 */
	guint in_destroy : 1;
};



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
static void gnome_calendar_update_paned_quanta (GnomeCalendar	*gcal);
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

static GtkWidget *
get_current_page (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;

	priv = gcal->priv;

	return GTK_NOTEBOOK (priv->notebook)->cur_page->child;
}

static ESearchBarItem search_menu_items[] = {
	E_FILTERBAR_RESET,
	{ NULL, -1 }
};

enum {
	SEARCH_ANY_FIELD_CONTAINS,
	SEARCH_SUMMARY_CONTAINS,
	SEARCH_DESCRIPTION_CONTAINS,
	SEARCH_COMMENT_CONTAINS,
	SEARCH_HAS_CATEGORY
};

static ESearchBarItem search_option_items[] = {
	{ N_("Any field contains"), SEARCH_ANY_FIELD_CONTAINS },
	{ N_("Summary contains"), SEARCH_SUMMARY_CONTAINS },
	{ N_("Description contains"), SEARCH_DESCRIPTION_CONTAINS },
	{ N_("Comment contains"), SEARCH_COMMENT_CONTAINS },
	{ N_("Has category"), SEARCH_HAS_CATEGORY },
	{ NULL, -1 }
};

/* Sets the query sexp for the current view in the calendar */
static void
set_query (GnomeCalendar *gcal, char *sexp)
{
	GnomeCalendarPrivate *priv;
	GtkWidget *page;

	g_assert (sexp != NULL);

	priv = gcal->priv;

	page = get_current_page (gcal);

	if (page == priv->day_view || page == priv->work_week_view)
		e_day_view_set_query (E_DAY_VIEW (page), sexp);
	else if (page == priv->week_view || page == priv->month_view)
		e_week_view_set_query (E_WEEK_VIEW (page), sexp);
	else {
		g_warning ("A penguin bit my hand!");
		g_assert_not_reached ();
	}
}

/* Sets the query string to be (contains? "field" "text") */
static void
set_query_contains (GnomeCalendar *gcal, const char *field, const char *text)
{
	char *sexp;

	sexp = g_strdup_printf ("(contains? \"%s\" \"%s\")", field, text);
	set_query (gcal, sexp);
	g_free (sexp);
}

/* Callback used when the query string is changed in the search bar */
static void
search_bar_query_changed_cb (ESearchBar *search_bar, gpointer data)
{
	GnomeCalendar *gcal;
	int item;
	char *text;

	gcal = GNOME_CALENDAR (data);

	item = e_search_bar_get_option_choice (search_bar);
	text = e_search_bar_get_text (search_bar);

	if (!text)
		return; /* This is an error in the UTF8 conversion, not an empty string! */

	switch (item) {
	case SEARCH_ANY_FIELD_CONTAINS:
		set_query_contains (gcal, "any", text);
		break;

	case SEARCH_SUMMARY_CONTAINS:
		set_query_contains (gcal, "summary", text);
		break;

	case SEARCH_DESCRIPTION_CONTAINS:
		set_query_contains (gcal, "description", text);
		break;

	case SEARCH_COMMENT_CONTAINS:
		set_query_contains (gcal, "comment", text);
		break;

	case SEARCH_HAS_CATEGORY: {
		char *sexp;

		sexp = g_strdup_printf ("(has-categories? \"%s\")", text);
		set_query (gcal, sexp);
		g_free (sexp);
		break;
	}

	default:
		g_assert_not_reached ();
	}

	g_free (text);
}

/* Callback used when a menu item is activated in the search bar */
static void
search_bar_menu_activated_cb (ESearchBar *search_bar, int item, gpointer data)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	switch (item) {
	case E_FILTERBAR_RESET_ID:
		set_query (gcal, "#t"); /* match all */
		/* FIXME: should we change the rest of the search bar so that
		 * the user sees that he selected "show all" instead of some
		 * type/text search combination?
		 */
		break;

	default:
		g_assert_not_reached ();
	}
}

static void
setup_widgets (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	GtkWidget *w;
	gchar *filename;
	CalendarModel *model;

	priv = gcal->priv;

	priv->search_bar = e_search_bar_new (search_menu_items, search_option_items);
	gtk_signal_connect (GTK_OBJECT (priv->search_bar), "query_changed",
			    GTK_SIGNAL_FUNC (search_bar_query_changed_cb), gcal);
	gtk_signal_connect (GTK_OBJECT (priv->search_bar), "menu_activated",
			    GTK_SIGNAL_FUNC (search_bar_menu_activated_cb), gcal);

	gtk_widget_show (priv->search_bar);
	gtk_box_pack_start (GTK_BOX (gcal), priv->search_bar, FALSE, FALSE, 0);

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
	e_calendar_item_set_days_start_week_sel (priv->date_navigator->calitem, 9);
	e_calendar_item_set_max_days_sel (priv->date_navigator->calitem, 42);
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
	calendar_config_configure_e_calendar_table (E_CALENDAR_TABLE (priv->todo));
	model = e_calendar_table_get_model (E_CALENDAR_TABLE (priv->todo));
	calendar_model_set_new_comp_vtype (model, CAL_COMPONENT_TODO);
	e_paned_pack2 (E_PANED (priv->vpane), priv->todo, TRUE, TRUE);
	gtk_widget_show (priv->todo);

	filename = g_strdup_printf ("%s/config/TaskPad", evolution_dir);
	e_calendar_table_load_state (E_CALENDAR_TABLE (priv->todo), filename);
	g_free (filename);

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

	gnome_calendar_update_config_settings (gcal, TRUE);
}

/* Object initialization function for the gnome calendar */
static void
gnome_calendar_init (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;

	priv = g_new0 (GnomeCalendarPrivate, 1);
	gcal->priv = priv;

	priv->object_editor_hash = g_hash_table_new (g_str_hash, g_str_equal);

	priv->current_view_type = VIEW_NOT_SET;
	priv->range_selected = FALSE;

	setup_widgets (gcal);

	priv->selection_start_time = time_day_begin (time (NULL));
	priv->selection_end_time = time_add_day (priv->selection_start_time, 1);
}

/* Used from g_hash_table_foreach(); frees an UID string */
static void
destroy_editor_cb (gpointer key, gpointer value, gpointer data)
{
	EventEditor *ee;

	ee = EVENT_EDITOR (value);
	gtk_object_unref (GTK_OBJECT (ee));
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

	/* Save the TaskPad layout. */
	filename = g_strdup_printf ("%s/config/TaskPad", evolution_dir);
	e_calendar_table_save_state (E_CALENDAR_TABLE (priv->todo), filename);
	g_free (filename);

	if (priv->client) {
#if 0
		alarm_notify_remove_client (priv->client);
#endif
		gtk_object_unref (GTK_OBJECT (priv->client));
		priv->client = NULL;
	}

	if (priv->task_pad_client) {
#if 0
		alarm_notify_remove_client (priv->task_pad_client);
#endif
		gtk_object_unref (GTK_OBJECT (priv->task_pad_client));
		priv->task_pad_client = NULL;
	}

	priv->in_destroy = TRUE;
	g_hash_table_foreach (priv->object_editor_hash, destroy_editor_cb, NULL);
	g_hash_table_destroy (priv->object_editor_hash);
	priv->object_editor_hash = NULL;

	g_free (priv);
	gcal->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
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
	if (priv->day_button)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->day_button), TRUE);
	else
		gnome_calendar_set_view (gcal, "dayview", FALSE, TRUE);
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

	calendar_config_set_default_view (view);

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

/* Loads the initial data into the calendar; this should be called right after
 * the cal_opened signal from the client is invoked.
 */
static void
initial_load (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;

	priv = gcal->priv;

	tag_calendar_by_client (priv->date_navigator, priv->client);
}

/* Displays an error to indicate that opening a calendar failed */
static void
open_error (GnomeCalendar *gcal, const char *uri)
{
	char *msg;

	msg = g_strdup_printf (_("Could not open the folder in `%s'"), uri);
	gnome_error_dialog_parented (msg, GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (gcal))));
	g_free (msg);
}

/* Displays an error to indicate that the specified URI method is not supported */
static void
method_error (GnomeCalendar *gcal, const char *uri)
{
	char *msg;

	msg = g_strdup_printf (_("The method required to open `%s' is not supported"), uri);
	gnome_error_dialog_parented (msg, GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (gcal))));
	g_free (msg);
}

/* Callback from the calendar client when a calendar is loaded */
static void
cal_opened_cb (CalClient *client, CalClientOpenStatus status, gpointer data)
{
	GnomeCalendar *gcal;
	GnomeCalendarPrivate *priv;

	gcal = GNOME_CALENDAR (data);
	priv = gcal->priv;

	switch (status) {
	case CAL_CLIENT_OPEN_SUCCESS:
		if (client == priv->client)
			initial_load (gcal);

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

	default:
		g_assert_not_reached ();
		return;
	}
}

/* Callback from the calendar client when an object is updated */
static void
obj_updated_cb (CalClient *client, const char *uid, gpointer data)
{
	GnomeCalendar *gcal;
	GnomeCalendarPrivate *priv;

	gcal = GNOME_CALENDAR (data);
	priv = gcal->priv;

	tag_calendar_by_client (priv->date_navigator, priv->client);
}

/* Callback from the calendar client when an object is removed */
static void
obj_removed_cb (CalClient *client, const char *uid, gpointer data)
{
	GnomeCalendar *gcal;
	GnomeCalendarPrivate *priv;

	gcal = GNOME_CALENDAR (data);
	priv = gcal->priv;

	tag_calendar_by_client (priv->date_navigator, priv->client);
}

GtkWidget *
gnome_calendar_construct (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	gint view;
	gchar *page;

	g_return_val_if_fail (gcal != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), NULL);

	priv = gcal->priv;

	/*
	 * Calendar Folder Client.
	 */
	priv->client = cal_client_new ();
	if (!priv->client)
		return NULL;

	gtk_signal_connect (GTK_OBJECT (priv->client), "cal_opened",
			    GTK_SIGNAL_FUNC (cal_opened_cb), gcal);
	gtk_signal_connect (GTK_OBJECT (priv->client), "obj_updated",
			    GTK_SIGNAL_FUNC (obj_updated_cb), gcal);
	gtk_signal_connect (GTK_OBJECT (priv->client), "obj_removed",
			    GTK_SIGNAL_FUNC (obj_removed_cb), gcal);

#if 0
	alarm_notify_add_client (priv->client);
#endif

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

	gtk_signal_connect (GTK_OBJECT (priv->task_pad_client), "cal_opened",
			    GTK_SIGNAL_FUNC (cal_opened_cb), gcal);

#if 0
	alarm_notify_add_client (priv->task_pad_client);
#endif

	e_calendar_table_set_cal_client (E_CALENDAR_TABLE (priv->todo),
					 priv->task_pad_client);


	/* Get the default view to show. */
	view = calendar_config_get_default_view ();
	switch (view) {
	case 1:
		page = "workweekview";
		break;
	case 2:
		page = "weekview";
		break;
	case 3:
		page = "monthview";
		break;
	default:
		page = "dayview";
		break;
	}

	gnome_calendar_set_view (gcal, page, FALSE, FALSE);

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

gboolean
gnome_calendar_open (GnomeCalendar *gcal, const char *str_uri)
{
	GnomeCalendarPrivate *priv;
	char *tasks_uri;
	gboolean success;

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

	if (!cal_client_open_calendar (priv->client, str_uri, FALSE)) {
		g_message ("gnome_calendar_open(): Could not issue the request");
		return FALSE;
	}

	/* Open the appropriate Tasks folder to show in the TaskPad.
	   Currently we just show the folder named "Tasks", but it will be
	   a per-calendar option in future. */

	tasks_uri = g_strdup_printf ("%s/local/Tasks/tasks.ics", evolution_dir);
	success = cal_client_open_calendar (priv->task_pad_client, tasks_uri, FALSE);
	g_free (tasks_uri);

	if (!success) {
		g_message ("gnome_calendar_open(): Could not issue the request");
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

	if (initializing) {
		priv->hpane_pos = calendar_config_get_hpane_pos ();
		priv->vpane_pos = calendar_config_get_vpane_pos ();
		priv->hpane_pos_month_view = calendar_config_get_month_hpane_pos ();
		priv->vpane_pos_month_view = calendar_config_get_month_vpane_pos ();
	} else {
		gnome_calendar_update_paned_quanta (gcal);
	}

	/* The range of days shown may have changed, so we update the date
	   navigator if needed. */
	gnome_calendar_update_date_navigator (gcal);
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

	ec = (struct editor_closure *) data;
	gcal = ec->gcal;
	priv = gcal->priv;

	result = g_hash_table_lookup_extended (priv->object_editor_hash, ec->uid, &orig_key, NULL);
	g_assert (result != FALSE);

	orig_uid = orig_key;

	if (!priv->in_destroy)
		g_hash_table_remove (priv->object_editor_hash, orig_uid);

	g_free (orig_uid);

	g_free (ec);
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

		comp_editor_set_cal_client (COMP_EDITOR (ee), priv->client);
		comp_editor_edit_comp (COMP_EDITOR (ee), comp);
	}

	comp_editor_focus (COMP_EDITOR (ee));
}

/**
 * gnome_calendar_new_appointment:
 * @gcal: An Evolution calendar.
 * @dtstart: a Unix time_t that marks the beginning of the appointment.
 * @dtend: a Unix time_t that marks the end of the appointment.
 * @all_day: if true, the dtstart and dtend are expanded to cover the entire day.
 *
 * Opens an event editor dialog for a new appointment.
 *
 **/
void
gnome_calendar_new_appointment_for (GnomeCalendar *cal,
				    time_t dtstart, time_t dtend,
				    gboolean all_day)
{
	struct icaltimetype itt;
	CalComponentDateTime dt;
	CalComponent *comp;

	g_return_if_fail (cal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (cal));

	if (all_day){
		dtstart = time_day_begin (dtstart);
		dtend = time_day_end (dtend);
	}
	
	dt.value = &itt;
	dt.tzid = NULL;

	comp = cal_component_new ();
	cal_component_set_new_vtype (comp, CAL_COMPONENT_EVENT);

	itt = icaltime_from_timet (dtstart, FALSE);
	cal_component_set_dtstart (comp, &dt);

	itt = icaltime_from_timet (dtend, FALSE);
	cal_component_set_dtend (comp, &dt);

	cal_component_commit_sequence (comp);

	gnome_calendar_edit_object (cal, comp);
	gtk_object_unref (GTK_OBJECT (comp));
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
	gnome_calendar_new_appointment_for (gcal, dtstart, dtend, FALSE);
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

	/* If a complete week is selected we show the Week view.
	   Note that if weekends are compressed and the week start day is set
	   to Sunday we don't actually show complete weeks in the Week view,
	   so this may need tweaking. */
	if (g_date_weekday (&new_start_date) % 7 == calendar_config_get_week_start_day ())
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

	tag_calendar_by_client (priv->date_navigator, priv->client);
}


static void
gnome_calendar_on_date_navigator_style_set (GtkWidget     *widget,
					    GtkStyle      *previous_style,
					    gpointer       data)
{
	gnome_calendar_update_paned_quanta (GNOME_CALENDAR (data));
}


static void
gnome_calendar_update_paned_quanta (GnomeCalendar	*gcal)
{
	GnomeCalendarPrivate *priv;
	gint row_height, col_width;
	gint top_border, bottom_border, left_border, right_border;

	priv = gcal->priv;

	e_calendar_get_border_size (priv->date_navigator,
				    &top_border, &bottom_border,
				    &left_border, &right_border);
	gtk_object_get (GTK_OBJECT (priv->date_navigator->calitem),
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

		if (priv->current_view_type == VIEW_MONTH
		    && !priv->range_selected) {
			priv->hpane_pos_month_view = hpane_pos;
			priv->vpane_pos_month_view = vpane_pos;
			calendar_config_set_month_hpane_pos (hpane_pos);
			calendar_config_set_month_vpane_pos (vpane_pos);
		} else {
			priv->hpane_pos = hpane_pos;
			priv->vpane_pos = vpane_pos;
			calendar_config_set_hpane_pos (hpane_pos);
			calendar_config_set_vpane_pos (vpane_pos);
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

	if (button) {
		gtk_signal_handler_block_by_data (GTK_OBJECT (button), gcal);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
		gtk_signal_handler_unblock_by_data (GTK_OBJECT (button), gcal);
	}
}
