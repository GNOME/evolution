/*
 * Evolution calendar - Main calendar view widget
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Miguel de Icaza <miguel@ximian.com>
 *      Federico Mena-Quintero <federico@ximian.com>
 *      Seth Alves <alves@hungry.com>
 *      Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gnome-cal.h"

#include <unistd.h>
#include <math.h>
#include <signal.h>
#include <fcntl.h>

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "shell/e-shell.h"

#include "dialogs/delete-error.h"
#include "dialogs/event-editor.h"

#include "calendar-config.h"
#include "calendar-view.h"
#include "comp-util.h"
#include "e-cal-list-view.h"
#include "e-cal-model-calendar.h"
#include "e-day-view-time-item.h"
#include "e-day-view.h"
#include "e-memo-table.h"
#include "e-month-view.h"
#include "e-task-table.h"
#include "e-week-view.h"
#include "ea-calendar.h"
#include "misc.h"
#include "tag-calendar.h"

#define d(x)

#define GNOME_CALENDAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), GNOME_TYPE_CALENDAR, GnomeCalendarPrivate))

typedef struct _ViewData ViewData;

/* Private part of the GnomeCalendar structure */
struct _GnomeCalendarPrivate {
	ESourceRegistry *registry;
	ECalModel *model;

	/*
	 * Fields for the calendar view
	 */

	/* This is the last time explicitly selected by the user */
	time_t base_view_time;

	/* Widgets */

	GtkWidget   *hpane;

	ECalendar   *date_navigator;
	GtkWidget   *memo_table; /* EMemoTable, but can be NULL */
	GtkWidget   *task_table; /* ETaskTable, but can be NULL */

	GHashTable *date_nav_view_data;  /* set of ViewData */
	GMutex date_nav_view_data_lock;

	gchar        *sexp;
	guint        update_timeout;
	guint        update_marcus_bains_line_timeout;

	/* This is the view currently shown. We use it to keep track of the
	 * positions of the panes. range_selected is TRUE if a range of dates
	 * was selected in the date navigator to show the view. */
	ECalendarView    *views[GNOME_CAL_LAST_VIEW];
	GnomeCalendarViewType current_view_type;

	gboolean range_selected;

	/* These are the saved positions of the panes. They are multiples of
	 * calendar month widths & heights in the date navigator, so that they
	 * will work OK after theme changes. */
	gint	     hpane_pos;
	gint	     hpane_pos_month_view;

	/* The signal handler id for our GtkCalendar "day_selected" handler. */
	guint	     day_selected_id;

	/* The dates currently shown. If they are -1 then we have no dates
	 * shown. We only use these to check if we need to emit a
	 * 'dates-shown-changed' signal.*/
	time_t visible_start;
	time_t visible_end;
	gboolean updating;

	/* If this is true, list view uses range of showing the events
	 * as the dates selected in date navigator which is one month,
	 * else it uses the date range set in search bar. */
	gboolean lview_select_daten_range;

	GCancellable *cancellable;

	gulong notify_week_start_day_id;
};

struct _ViewData {
	volatile gint ref_count;
	GWeakRef gcal_weak_ref;
	GCancellable *cancellable;
	ECalClientView *client_view;
	gulong objects_added_handler_id;
	gulong objects_modified_handler_id;
	gulong objects_removed_handler_id;
	gulong complete_handler_id;
};

enum {
	PROP_0,
	PROP_DATE_NAVIGATOR,
	PROP_MEMO_TABLE,
	PROP_REGISTRY,
	PROP_TASK_TABLE,
	PROP_VIEW
};

enum {
	DATES_SHOWN_CHANGED,
	CALENDAR_SELECTION_CHANGED,
	GOTO_DATE,
	SOURCE_ADDED,
	SOURCE_REMOVED,
	CHANGE_VIEW,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void gnome_calendar_do_dispose (GObject *object);
static void gnome_calendar_finalize   (GObject *object);
static void gnome_calendar_goto_date (GnomeCalendar *gcal,
				      GnomeCalendarGotoDateType goto_date);

static void gnome_calendar_update_date_navigator (GnomeCalendar *gcal);

static void update_task_and_memo_views (GnomeCalendar *gcal);

G_DEFINE_TYPE (GnomeCalendar, gnome_calendar, G_TYPE_OBJECT)

static ViewData *
view_data_new (GnomeCalendar *gcal)
{
	ViewData *view_data;

	view_data = g_slice_new0 (ViewData);
	view_data->ref_count = 1;
	view_data->cancellable = g_cancellable_new ();

	g_weak_ref_set (&view_data->gcal_weak_ref, gcal);

	return view_data;
}

static ViewData *
view_data_ref (ViewData *view_data)
{
	g_return_val_if_fail (view_data != NULL, NULL);
	g_return_val_if_fail (view_data->ref_count > 0, NULL);

	g_atomic_int_inc (&view_data->ref_count);

	return view_data;
}

static void
view_data_unref (ViewData *view_data)
{
	g_return_if_fail (view_data != NULL);
	g_return_if_fail (view_data->ref_count > 0);

	if (g_atomic_int_dec_and_test (&view_data->ref_count)) {

		if (view_data->objects_added_handler_id > 0)
			g_signal_handler_disconnect (
				view_data->client_view,
				view_data->objects_added_handler_id);

		if (view_data->objects_modified_handler_id > 0)
			g_signal_handler_disconnect (
				view_data->client_view,
				view_data->objects_modified_handler_id);

		if (view_data->objects_removed_handler_id > 0)
			g_signal_handler_disconnect (
				view_data->client_view,
				view_data->objects_removed_handler_id);

		if (view_data->complete_handler_id > 0)
			g_signal_handler_disconnect (
				view_data->client_view,
				view_data->complete_handler_id);

		g_weak_ref_set (&view_data->gcal_weak_ref, NULL);

		g_cancellable_cancel (view_data->cancellable);

		g_clear_object (&view_data->cancellable);
		g_clear_object (&view_data->client_view);

		g_slice_free (ViewData, view_data);
	}
}

static void
date_nav_view_data_insert (GnomeCalendar *gcal,
                           ViewData *view_data)
{
	g_return_if_fail (view_data != NULL);

	g_mutex_lock (&gcal->priv->date_nav_view_data_lock);

	g_hash_table_add (
		gcal->priv->date_nav_view_data,
		view_data_ref (view_data));

	g_mutex_unlock (&gcal->priv->date_nav_view_data_lock);
}

static void
date_nav_view_data_remove_all (GnomeCalendar *gcal)
{
	g_mutex_lock (&gcal->priv->date_nav_view_data_lock);

	g_hash_table_remove_all (gcal->priv->date_nav_view_data);

	g_mutex_unlock (&gcal->priv->date_nav_view_data_lock);
}

static void
gcal_update_status_message (GnomeCalendar *gcal,
                            const gchar *message,
                            gdouble percent)
{
	ECalModel *model;

	g_return_if_fail (gcal != NULL);

	model = gnome_calendar_get_model (gcal);
	g_return_if_fail (model != NULL);

	e_cal_model_update_status_message (model, message, percent);
}

static void
update_adjustment (GnomeCalendar *gcal,
                   GtkAdjustment *adjustment,
                   EWeekView *week_view)
{
	GDate date;
	GDate first_day_shown;
	ECalModel *model;
	gint week_offset;
	struct icaltimetype start_tt = icaltime_null_time ();
	time_t lower;
	guint32 old_first_day_julian, new_first_day_julian;
	icaltimezone *timezone;
	gdouble value;

	e_week_view_get_first_day_shown (week_view, &first_day_shown);

	/* If we don't have a valid date set yet, just return. */
	if (!g_date_valid (&first_day_shown))
		return;

	value = gtk_adjustment_get_value (adjustment);

	/* Determine the first date shown. */
	date = week_view->base_date;
	week_offset = floor (value + 0.5);
	g_date_add_days (&date, week_offset * 7);

	/* Convert the old & new first days shown to julian values. */
	old_first_day_julian = g_date_get_julian (&first_day_shown);
	new_first_day_julian = g_date_get_julian (&date);

	/* If we are already showing the date, just return. */
	if (old_first_day_julian == new_first_day_julian)
		return;

	/* Convert it to a time_t. */
	start_tt.year = g_date_get_year (&date);
	start_tt.month = g_date_get_month (&date);
	start_tt.day = g_date_get_day (&date);

	model = gnome_calendar_get_model (gcal);
	timezone = e_cal_model_get_timezone (model);
	lower = icaltime_as_timet_with_zone (start_tt, timezone);

	e_week_view_set_update_base_date (week_view, FALSE);
	gnome_calendar_set_selected_time_range (gcal, lower);
	e_week_view_set_update_base_date (week_view, TRUE);
}

static void
week_view_adjustment_changed_cb (GtkAdjustment *adjustment,
                                 GnomeCalendar *gcal)
{
	ECalendarView *view;

	view = gnome_calendar_get_calendar_view (gcal, GNOME_CAL_WEEK_VIEW);
	update_adjustment (gcal, adjustment, E_WEEK_VIEW (view));
}

static void
month_view_adjustment_changed_cb (GtkAdjustment *adjustment,
                                  GnomeCalendar *gcal)
{
	ECalendarView *view;

	view = gnome_calendar_get_calendar_view (gcal, GNOME_CAL_MONTH_VIEW);
	update_adjustment (gcal, adjustment, E_WEEK_VIEW (view));
}

static void
view_selection_changed_cb (GnomeCalendar *gcal)
{
	g_signal_emit (gcal, signals[CALENDAR_SELECTION_CHANGED], 0);
}

static void
view_progress_cb (ECalModel *model,
                  const gchar *message,
                  gint percent,
                  ECalClientSourceType type,
                  GnomeCalendar *gcal)
{
	gcal_update_status_message (gcal, message, percent);
}

static void
view_complete_cb (ECalModel *model,
                  const GError *error,
                  ECalClientSourceType type,
                  GnomeCalendar *gcal)
{
	gcal_update_status_message (gcal, NULL, -1);
}

static void
gnome_calendar_notify_week_start_day_cb (GnomeCalendar *gcal)
{
	time_t start_time;

	start_time = gcal->priv->base_view_time;
	gnome_calendar_set_selected_time_range (gcal, start_time);
}

static void
gnome_calendar_update_time_range (GnomeCalendar *gcal)
{
	time_t start_time;

	start_time = gcal->priv->base_view_time;
	gnome_calendar_set_selected_time_range (gcal, start_time);
}

static void
gnome_calendar_set_registry (GnomeCalendar *gcal,
                             ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (gcal->priv->registry == NULL);

	gcal->priv->registry = g_object_ref (registry);
}

static void
gnome_calendar_set_property (GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DATE_NAVIGATOR:
			gnome_calendar_set_date_navigator (
				GNOME_CALENDAR (object),
				g_value_get_object (value));
			return;

		case PROP_MEMO_TABLE:
			gnome_calendar_set_memo_table (
				GNOME_CALENDAR (object),
				g_value_get_object (value));
			return;

		case PROP_REGISTRY:
			gnome_calendar_set_registry (
				GNOME_CALENDAR (object),
				g_value_get_object (value));
			return;

		case PROP_TASK_TABLE:
			gnome_calendar_set_task_table (
				GNOME_CALENDAR (object),
				g_value_get_object (value));
			return;

		case PROP_VIEW:
			gnome_calendar_set_view (
				GNOME_CALENDAR (object),
				g_value_get_int (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
gnome_calendar_get_property (GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DATE_NAVIGATOR:
			g_value_set_object (
				value, gnome_calendar_get_date_navigator (
				GNOME_CALENDAR (object)));
			return;

		case PROP_MEMO_TABLE:
			g_value_set_object (
				value, gnome_calendar_get_memo_table (
				GNOME_CALENDAR (object)));
			return;

		case PROP_REGISTRY:
			g_value_set_object (
				value, gnome_calendar_get_registry (
				GNOME_CALENDAR (object)));
			return;

		case PROP_TASK_TABLE:
			g_value_set_object (
				value, gnome_calendar_get_task_table (
				GNOME_CALENDAR (object)));
			return;

		case PROP_VIEW:
			g_value_set_int (
				value, gnome_calendar_get_view (
				GNOME_CALENDAR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
gnome_calendar_constructed (GObject *object)
{
	GnomeCalendar *gcal = GNOME_CALENDAR (object);
	ECalendarView *calendar_view;
	ESourceRegistry *registry;
	ECalModel *model;
	GtkAdjustment *adjustment;

	registry = gnome_calendar_get_registry (gcal);

	/* Create the model for the views. */
	model = e_cal_model_calendar_new (registry);
	e_cal_model_set_flags (model, E_CAL_MODEL_FLAGS_EXPAND_RECURRENCES);
	gcal->priv->model = model;

	g_signal_connect (
		model, "cal-view-progress",
		G_CALLBACK (view_progress_cb), gcal);

	g_signal_connect (
		model, "cal-view-complete",
		G_CALLBACK (view_complete_cb), gcal);

	/* Day View */
	calendar_view = e_day_view_new (model);
	e_calendar_view_set_calendar (calendar_view, gcal);
	gcal->priv->views[GNOME_CAL_DAY_VIEW] = calendar_view;
	g_object_ref_sink (calendar_view);

	g_signal_connect_swapped (
		calendar_view, "selection-changed",
		G_CALLBACK (view_selection_changed_cb), gcal);

	/* Work Week View */
	calendar_view = e_day_view_new (model);
	e_day_view_set_work_week_view (E_DAY_VIEW (calendar_view), TRUE);
	e_day_view_set_days_shown (E_DAY_VIEW (calendar_view), 5);
	e_calendar_view_set_calendar (calendar_view, gcal);
	gcal->priv->views[GNOME_CAL_WORK_WEEK_VIEW] = calendar_view;
	g_object_ref_sink (calendar_view);

	e_signal_connect_notify_swapped (
		calendar_view, "notify::working-day-monday",
		G_CALLBACK (gnome_calendar_update_time_range), gcal);

	e_signal_connect_notify_swapped (
		calendar_view, "notify::working-day-tuesday",
		G_CALLBACK (gnome_calendar_update_time_range), gcal);

	e_signal_connect_notify_swapped (
		calendar_view, "notify::working-day-wednesday",
		G_CALLBACK (gnome_calendar_update_time_range), gcal);

	e_signal_connect_notify_swapped (
		calendar_view, "notify::working-day-thursday",
		G_CALLBACK (gnome_calendar_update_time_range), gcal);

	e_signal_connect_notify_swapped (
		calendar_view, "notify::working-day-friday",
		G_CALLBACK (gnome_calendar_update_time_range), gcal);

	e_signal_connect_notify_swapped (
		calendar_view, "notify::working-day-saturday",
		G_CALLBACK (gnome_calendar_update_time_range), gcal);

	e_signal_connect_notify_swapped (
		calendar_view, "notify::working-day-sunday",
		G_CALLBACK (gnome_calendar_update_time_range), gcal);

	/* Week View */
	calendar_view = e_week_view_new (model);
	e_calendar_view_set_calendar (calendar_view, gcal);
	gcal->priv->views[GNOME_CAL_WEEK_VIEW] = calendar_view;
	g_object_ref_sink (calendar_view);

	g_signal_connect_swapped (
		calendar_view, "selection-changed",
		G_CALLBACK (view_selection_changed_cb), gcal);

	adjustment = gtk_range_get_adjustment (
		GTK_RANGE (E_WEEK_VIEW (calendar_view)->vscrollbar));
	g_signal_connect (
		adjustment, "value-changed",
		G_CALLBACK (week_view_adjustment_changed_cb), gcal);

	/* Month View */
	calendar_view = e_month_view_new (model);
	e_week_view_set_multi_week_view (E_WEEK_VIEW (calendar_view), TRUE);
	e_week_view_set_weeks_shown (E_WEEK_VIEW (calendar_view), 6);
	e_calendar_view_set_calendar (calendar_view, gcal);
	gcal->priv->views[GNOME_CAL_MONTH_VIEW] = calendar_view;
	g_object_ref_sink (calendar_view);

	g_signal_connect_swapped (
		calendar_view, "selection-changed",
		G_CALLBACK (view_selection_changed_cb), gcal);

	adjustment = gtk_range_get_adjustment (
		GTK_RANGE (E_WEEK_VIEW (calendar_view)->vscrollbar));
	g_signal_connect (
		adjustment, "value-changed",
		G_CALLBACK (month_view_adjustment_changed_cb), gcal);

	/* List View */
	calendar_view = e_cal_list_view_new (model);
	e_calendar_view_set_calendar (calendar_view, gcal);
	gcal->priv->views[GNOME_CAL_LIST_VIEW] = calendar_view;
	g_object_ref_sink (calendar_view);

	g_signal_connect_swapped (
		calendar_view, "selection-changed",
		G_CALLBACK (view_selection_changed_cb), gcal);

	gcal->priv->notify_week_start_day_id = e_signal_connect_notify_swapped (
		model, "notify::week-start-day",
		G_CALLBACK (gnome_calendar_notify_week_start_day_cb), gcal);

	gnome_calendar_goto_today (gcal);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (gnome_calendar_parent_class)->constructed (object);
}

/* Class initialization function for the gnome calendar */
static void
gnome_calendar_class_init (GnomeCalendarClass *class)
{
	GObjectClass *object_class;
	GtkBindingSet *binding_set;

	g_type_class_add_private (class, sizeof (GnomeCalendarPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = gnome_calendar_set_property;
	object_class->get_property = gnome_calendar_get_property;
	object_class->constructed = gnome_calendar_constructed;
	object_class->dispose = gnome_calendar_do_dispose;
	object_class->finalize = gnome_calendar_finalize;

	class->dates_shown_changed = NULL;
	class->calendar_selection_changed = NULL;
	class->source_added = NULL;
	class->source_removed = NULL;
	class->goto_date = gnome_calendar_goto_date;
	class->change_view = gnome_calendar_set_view;

	g_object_class_install_property (
		object_class,
		PROP_DATE_NAVIGATOR,
		g_param_spec_object (
			"date-navigator",
			"Date Navigator",
			NULL,
			E_TYPE_CALENDAR,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_MEMO_TABLE,
		g_param_spec_object (
			"memo-table",
			"Memo table",
			NULL,
			E_TYPE_MEMO_TABLE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_REGISTRY,
		g_param_spec_object (
			"registry",
			"Registry",
			"Data source registry",
			E_TYPE_SOURCE_REGISTRY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_TASK_TABLE,
		g_param_spec_object (
			"task-table",
			"Task table",
			NULL,
			E_TYPE_TASK_TABLE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_VIEW,
		g_param_spec_int (
			"view",
			"View",
			NULL,
			GNOME_CAL_DAY_VIEW,
			GNOME_CAL_LIST_VIEW,
			GNOME_CAL_DAY_VIEW,
			G_PARAM_READWRITE));

	signals[DATES_SHOWN_CHANGED] = g_signal_new (
		"dates_shown_changed",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GnomeCalendarClass, dates_shown_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[CALENDAR_SELECTION_CHANGED] = g_signal_new (
		"calendar_selection_changed",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GnomeCalendarClass, calendar_selection_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[SOURCE_ADDED] = g_signal_new (
		"source_added",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GnomeCalendarClass, source_added),
		NULL, NULL,
		e_marshal_VOID__INT_OBJECT,
		G_TYPE_NONE, 2,
		G_TYPE_INT,
		G_TYPE_OBJECT);

	signals[SOURCE_REMOVED] = g_signal_new (
		"source_removed",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GnomeCalendarClass, source_removed),
		NULL, NULL,
		e_marshal_VOID__INT_OBJECT,
		G_TYPE_NONE, 2,
		G_TYPE_INT,
		G_TYPE_OBJECT);

	signals[GOTO_DATE] = g_signal_new (
		"goto_date",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (GnomeCalendarClass, goto_date),
		NULL, NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1,
		G_TYPE_INT);

	signals[CHANGE_VIEW] = g_signal_new (
		"change_view",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (GnomeCalendarClass, change_view),
		NULL, NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1,
		G_TYPE_INT);

	/*
	 * Key bindings
	 */

	binding_set = gtk_binding_set_new (G_OBJECT_CLASS_NAME (class));

	/* Alt+PageUp/PageDown, go to the first/last day of the month */
	gtk_binding_entry_add_signal (
		binding_set, GDK_KEY_Page_Up,
		GDK_MOD1_MASK,
		"goto_date", 1,
		G_TYPE_ENUM,
		GNOME_CAL_GOTO_FIRST_DAY_OF_MONTH);
	gtk_binding_entry_add_signal (
		binding_set, GDK_KEY_KP_Page_Up,
		GDK_MOD1_MASK,
		"goto_date", 1,
		G_TYPE_ENUM,
		GNOME_CAL_GOTO_FIRST_DAY_OF_MONTH);
	gtk_binding_entry_add_signal (
		binding_set, GDK_KEY_Page_Down,
		GDK_MOD1_MASK,
		"goto_date", 1,
		G_TYPE_ENUM,
		GNOME_CAL_GOTO_LAST_DAY_OF_MONTH);
	gtk_binding_entry_add_signal (
		binding_set, GDK_KEY_KP_Page_Down,
		GDK_MOD1_MASK,
		"goto_date", 1,
		G_TYPE_ENUM,
		GNOME_CAL_GOTO_LAST_DAY_OF_MONTH);

	/* Alt+Home/End, go to the first/last day of the week */
	gtk_binding_entry_add_signal (
		binding_set, GDK_KEY_Home,
		GDK_MOD1_MASK,
		"goto_date", 1,
		G_TYPE_ENUM,
		GNOME_CAL_GOTO_FIRST_DAY_OF_WEEK);
	gtk_binding_entry_add_signal (
		binding_set, GDK_KEY_End,
		GDK_MOD1_MASK,
		"goto_date", 1,
		G_TYPE_ENUM,
		GNOME_CAL_GOTO_LAST_DAY_OF_WEEK);
	gtk_binding_entry_add_signal (
		binding_set, GDK_KEY_KP_Home,
		GDK_MOD1_MASK,
		"goto_date", 1,
		G_TYPE_ENUM,
		GNOME_CAL_GOTO_FIRST_DAY_OF_WEEK);
	gtk_binding_entry_add_signal (
		binding_set, GDK_KEY_KP_End,
		GDK_MOD1_MASK,
		"goto_date", 1,
		G_TYPE_ENUM,
		GNOME_CAL_GOTO_LAST_DAY_OF_WEEK);

	/*Alt+Left/Right, go to the same day of the previous/next week*/
	gtk_binding_entry_add_signal (
		binding_set,GDK_KEY_Left,
		GDK_MOD1_MASK,
		"goto_date",1,
		G_TYPE_ENUM,
		GNOME_CAL_GOTO_SAME_DAY_OF_PREVIOUS_WEEK);
	gtk_binding_entry_add_signal (
		binding_set,GDK_KEY_KP_Left,
		GDK_MOD1_MASK,
		"goto_date",1,
		G_TYPE_ENUM,
		GNOME_CAL_GOTO_SAME_DAY_OF_PREVIOUS_WEEK);
	gtk_binding_entry_add_signal (
		binding_set,GDK_KEY_Right,
		GDK_MOD1_MASK,
		"goto_date",1,
		G_TYPE_ENUM,
		GNOME_CAL_GOTO_SAME_DAY_OF_NEXT_WEEK);
	gtk_binding_entry_add_signal (
		binding_set,GDK_KEY_KP_Right,
		GDK_MOD1_MASK,
		"goto_date",1,
		G_TYPE_ENUM,
		GNOME_CAL_GOTO_SAME_DAY_OF_NEXT_WEEK);

	/* Ctrl+Y/J/K/M/L to switch between
	 * DayView/WorkWeekView/WeekView/MonthView/ListView */
	gtk_binding_entry_add_signal (
		binding_set, GDK_KEY_y,
		GDK_CONTROL_MASK,
		"change_view", 1,
		G_TYPE_ENUM,
		GNOME_CAL_DAY_VIEW);
	gtk_binding_entry_add_signal (
		binding_set, GDK_KEY_j,
		GDK_CONTROL_MASK,
		"change_view", 1,
		G_TYPE_ENUM,
		GNOME_CAL_WORK_WEEK_VIEW);
	gtk_binding_entry_add_signal (
		binding_set, GDK_KEY_k,
		GDK_CONTROL_MASK,
		"change_view", 1,
		G_TYPE_ENUM,
		GNOME_CAL_WEEK_VIEW);
	gtk_binding_entry_add_signal (
		binding_set, GDK_KEY_m,
		GDK_CONTROL_MASK,
		"change_view", 1,
		G_TYPE_ENUM,
		GNOME_CAL_MONTH_VIEW);
	gtk_binding_entry_add_signal (
		binding_set, GDK_KEY_l,
		GDK_CONTROL_MASK,
		"change_view", 1,
		G_TYPE_ENUM,
		GNOME_CAL_LIST_VIEW);

	/* init the accessibility support for gnome_calendar */
	gnome_calendar_a11y_init ();

}

/* We do this check since the calendar items are downloaded from the server
 * in the open_method, since the default timezone might not be set there. */
static void
ensure_dates_are_in_default_zone (GnomeCalendar *gcal,
                                  icalcomponent *icalcomp)
{
	ECalModel *model;
	icaltimezone *timezone;
	icaltimetype dt;

	model = gnome_calendar_get_model (gcal);
	timezone = e_cal_model_get_timezone (model);

	if (timezone == NULL)
		return;

	dt = icalcomponent_get_dtstart (icalcomp);
	if (dt.is_utc) {
		dt = icaltime_convert_to_zone (dt, timezone);
		icalcomponent_set_dtstart (icalcomp, dt);
	}

	dt = icalcomponent_get_dtend (icalcomp);
	if (dt.is_utc) {
		dt = icaltime_convert_to_zone (dt, timezone);
		icalcomponent_set_dtend (icalcomp, dt);
	}
}

/* Callback used when the calendar query reports of an updated object */
static void
gnome_cal_objects_added_cb (ECalClientView *view,
                            const GSList *list,
                            GWeakRef *weak_ref)
{
	GnomeCalendar *gcal;

	gcal = g_weak_ref_get (weak_ref);

	if (gcal != NULL) {
		const GSList *link;

		for (link = list; link != NULL; link = g_slist_next (link)) {
			ECalComponent *comp = NULL;
			icalcomponent *icalcomp = link->data;

			ensure_dates_are_in_default_zone (gcal, icalcomp);
			comp = e_cal_component_new ();
			if (!e_cal_component_set_icalcomponent (
				comp, icalcomponent_new_clone (icalcomp))) {
				g_object_unref (comp);
				continue;
			}

			tag_calendar_by_comp (
				gcal->priv->date_navigator, comp,
				e_cal_client_view_get_client (view),
				NULL, FALSE, TRUE, TRUE,
				gcal->priv->cancellable);
			g_object_unref (comp);
		}

		g_object_unref (gcal);
	}
}

static void
gnome_cal_objects_modified_cb (ECalClientView *view,
                               const GSList *objects,
                               GWeakRef *weak_ref)
{
	GnomeCalendar *gcal;

	gcal = g_weak_ref_get (weak_ref);

	if (gcal != NULL) {
		/* We have to retag the whole thing: an event may change dates
		 * and the tag_calendar_by_comp() below would not know how to
		 * untag the old dates. */
		gnome_calendar_update_query (gcal);
		g_object_unref (gcal);
	}
}

/* Callback used when the calendar query reports of a removed object */
static void
gnome_cal_objects_removed_cb (ECalClientView *view,
                              const GSList *ids,
                              GWeakRef *weak_ref)
{
	GnomeCalendar *gcal;

	gcal = g_weak_ref_get (weak_ref);

	if (gcal != NULL) {
		/* Just retag the whole thing */
		gnome_calendar_update_query (gcal);
		g_object_unref (gcal);
	}
}

/* Callback used when the calendar query is done */
static void
gnome_cal_view_complete_cb (ECalClientView *query,
                            const GError *error,
                            GWeakRef *weak_ref)
{
	/* FIXME Better error reporting */
	if (error != NULL)
		g_warning (
			"%s: Query did not complete successfully: %s",
			G_STRFUNC, error->message);
}

ECalendarView *
gnome_calendar_get_calendar_view (GnomeCalendar *gcal,
                                  GnomeCalendarViewType view_type)
{
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), NULL);
	g_return_val_if_fail (view_type < GNOME_CAL_LAST_VIEW, NULL);

	return gcal->priv->views[view_type];
}

static void
get_times_for_views (GnomeCalendar *gcal,
                     GnomeCalendarViewType view_type,
                     time_t *start_time,
                     time_t *end_time,
                     time_t *select_time)
{
	GnomeCalendarPrivate *priv;
	ECalModel *model;
	EDayView *day_view;
	EWeekView *week_view;
	gint shown;
	GDate date;
	gint days_shown;
	GDateWeekday week_start_day;
	GDateWeekday first_work_day;
	GDateWeekday last_work_day;
	GDateWeekday start_day;
	GDateWeekday weekday;
	guint offset;
	struct icaltimetype tt = icaltime_null_time ();
	icaltimezone *timezone;
	gboolean range_selected;

	model = gnome_calendar_get_model (gcal);
	range_selected = gnome_calendar_get_range_selected (gcal);

	timezone = e_cal_model_get_timezone (model);
	week_start_day = e_cal_model_get_week_start_day (model);

	priv = gcal->priv;

	switch (view_type) {
	case GNOME_CAL_DAY_VIEW:
		day_view = E_DAY_VIEW (priv->views[view_type]);
		shown = e_day_view_get_days_shown (day_view);
		*start_time = time_day_begin_with_zone (*start_time, timezone);
		*end_time = time_add_day_with_zone (*start_time, shown, timezone);
		break;
	case GNOME_CAL_WORK_WEEK_VIEW:
		/* FIXME Kind of gross, but it works */
		day_view = E_DAY_VIEW (priv->views[view_type]);
		time_to_gdate_with_zone (&date, *start_time, timezone);

		/* The start of the work-week is the first working day after the
		 * week start day. */

		/* Get the weekday corresponding to start_time. */
		weekday = g_date_get_weekday (&date);

		/* Find the first working day of the week. */
		first_work_day = e_cal_model_get_work_day_first (model);

		if (first_work_day != G_DATE_BAD_WEEKDAY) {
			last_work_day = e_cal_model_get_work_day_last (model);

			/* Now calculate the days we need to show to include
			 * all the working days in the week. Add 1 to make it
			 * inclusive. */
			days_shown = e_weekday_get_days_between (
				first_work_day, last_work_day) + 1;
		} else {
			/* If no working days are set, just use 7. */
			days_shown = 7;
		}

		if (first_work_day == G_DATE_BAD_WEEKDAY)
			first_work_day = week_start_day;

		/* Calculate how many days we need to go back to the first workday. */
		if (weekday < first_work_day)
			offset = (weekday + 7) - first_work_day;
		else
			offset = weekday - first_work_day;

		if (offset > 0)
			g_date_subtract_days (&date, offset);

		tt.year = g_date_get_year (&date);
		tt.month = g_date_get_month (&date);
		tt.day = g_date_get_day (&date);

		*start_time = icaltime_as_timet_with_zone (tt, timezone);
		*end_time = time_add_day_with_zone (*start_time, days_shown, timezone);

		if (select_time && day_view->selection_start_day == -1)
			time (select_time);
		break;
	case GNOME_CAL_WEEK_VIEW:
		week_view = E_WEEK_VIEW (priv->views[view_type]);
		start_day = e_week_view_get_display_start_day (week_view);

		*start_time = time_week_begin_with_zone (
			*start_time,
			e_weekday_to_tm_wday (start_day),
			timezone);
		*end_time = time_add_week_with_zone (
			*start_time, 1, timezone);

		if (select_time && week_view->selection_start_day == -1)
			time (select_time);
		break;
	case GNOME_CAL_MONTH_VIEW:
		week_view = E_WEEK_VIEW (priv->views[view_type]);
		shown = e_week_view_get_weeks_shown (week_view);
		start_day = e_week_view_get_display_start_day (week_view);

		if (!range_selected && (
			!e_week_view_get_multi_week_view (week_view) ||
			!week_view->month_scroll_by_week))
			*start_time = time_month_begin_with_zone (
				*start_time, timezone);

		*start_time = time_week_begin_with_zone (
			*start_time,
			e_weekday_to_tm_wday (start_day),
			timezone);
		*end_time = time_add_week_with_zone (
			*start_time, shown, timezone);

		if (select_time && week_view->selection_start_day == -1)
			time (select_time);
		break;
	case GNOME_CAL_LIST_VIEW:
		/* FIXME What to do here? */
		*start_time = time_month_begin_with_zone (*start_time, timezone);
		*end_time = time_add_month_with_zone (*start_time, 1, timezone);
		break;
	default:
		g_return_if_reached ();
	}
}

/* Computes the range of time that the date navigator is showing */
static void
get_date_navigator_range (GnomeCalendar *gcal,
                          time_t *start_time,
                          time_t *end_time)
{
	ECalModel *model;
	gint start_year, start_month, start_day;
	gint end_year, end_month, end_day;
	struct icaltimetype start_tt;
	struct icaltimetype end_tt;
	icaltimezone *timezone;

	model = gnome_calendar_get_model (gcal);
	timezone = e_cal_model_get_timezone (model);

	start_tt = icaltime_null_time ();
	end_tt = icaltime_null_time ();

	if (!e_calendar_item_get_date_range (
		gcal->priv->date_navigator->calitem,
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

	*start_time = icaltime_as_timet_with_zone (start_tt, timezone);
	*end_time = icaltime_as_timet_with_zone (end_tt, timezone);
}

static const gchar *
gcal_get_default_tzloc (GnomeCalendar *gcal)
{
	ECalModel *model;
	icaltimezone *timezone;
	const gchar *tzloc = NULL;

	g_return_val_if_fail (gcal != NULL, "");

	model = gnome_calendar_get_model (gcal);
	timezone = e_cal_model_get_timezone (model);

	if (timezone && timezone != icaltimezone_get_utc_timezone ())
		tzloc = icaltimezone_get_location (timezone);

	return tzloc ? tzloc : "";
}

/* Adjusts a given query sexp with the time range of the date navigator */
static gchar *
adjust_client_view_sexp (GnomeCalendar *gcal,
                         const gchar *sexp)
{
	time_t start_time, end_time;
	gchar *start, *end;
	gchar *new_sexp;

	get_date_navigator_range (gcal, &start_time, &end_time);
	if (start_time == -1 || end_time == -1)
		return NULL;

	start = isodate_from_time_t (start_time);
	end = isodate_from_time_t (end_time);

	if (sexp) {
		new_sexp = g_strdup_printf (
			"(and (occur-in-time-range? (make-time \"%s\") (make-time \"%s\") \"%s\") %s)",
			start, end, gcal_get_default_tzloc (gcal), sexp);
	} else {
		new_sexp = g_strdup_printf (
			"(occur-in-time-range? (make-time \"%s\") (make-time \"%s\") \"%s\")",
			start, end, gcal_get_default_tzloc (gcal));
	}

	g_free (start);
	g_free (end);

	return new_sexp;
}

static void
gnome_cal_get_client_view_cb (GObject *source_object,
                              GAsyncResult *result,
                              gpointer user_data)
{
	ECalClient *client;
	ECalClientView *client_view = NULL;
	GnomeCalendar *gcal;
	ViewData *view_data;
	GError *local_error = NULL;

	client = E_CAL_CLIENT (source_object);
	view_data = (ViewData *) user_data;

	e_cal_client_get_view_finish (
		client, result, &client_view, &local_error);

	/* Sanity check. */
	g_return_if_fail (
		((client_view != NULL) && (local_error == NULL)) ||
		((client_view == NULL) && (local_error != NULL)));

	gcal = g_weak_ref_get (&view_data->gcal_weak_ref);

	if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (local_error);

	} else if (local_error != NULL) {
		/* FIXME This should be displayed as an EAlert. */
		g_warning ("%s: %s", G_STRFUNC, local_error->message);
		g_error_free (local_error);

	} else if (gcal != NULL) {
		gulong handler_id;

		/* The ViewData struct is only modified from a single
		 * thread, so no locking is necessary when populating
		 * the struct members. */

		view_data->client_view = g_object_ref (client_view);

		handler_id = g_signal_connect_data (
			client_view, "objects-added",
			G_CALLBACK (gnome_cal_objects_added_cb),
			e_weak_ref_new (gcal),
			(GClosureNotify) e_weak_ref_free, 0);
		view_data->objects_added_handler_id = handler_id;

		handler_id = g_signal_connect_data (
			client_view, "objects-modified",
			G_CALLBACK (gnome_cal_objects_modified_cb),
			e_weak_ref_new (gcal),
			(GClosureNotify) e_weak_ref_free, 0);
		view_data->objects_modified_handler_id = handler_id;

		handler_id = g_signal_connect_data (
			client_view, "objects-removed",
			G_CALLBACK (gnome_cal_objects_removed_cb),
			e_weak_ref_new (gcal),
			(GClosureNotify) e_weak_ref_free, 0);
		view_data->objects_removed_handler_id = handler_id;

		handler_id = g_signal_connect_data (
			client_view, "complete",
			G_CALLBACK (gnome_cal_view_complete_cb),
			e_weak_ref_new (gcal),
			(GClosureNotify) e_weak_ref_free, 0);
		view_data->complete_handler_id = handler_id;

		/* XXX This call blocks with no way to cancel.  But the
		 *     ECalClientView API does not provide a proper way. */
		e_cal_client_view_start (client_view, &local_error);

		if (local_error != NULL) {
			g_warning ("%s: %s", G_STRFUNC, local_error->message);
			g_error_free (local_error);
		}
	}

	g_clear_object (&gcal);
	g_clear_object (&client_view);

	view_data_unref (view_data);
}

/* Restarts a query for the date navigator in the calendar */
void
gnome_calendar_update_query (GnomeCalendar *gcal)
{
	GList *list, *link;
	gchar *real_sexp;

	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	e_calendar_item_clear_marks (gcal->priv->date_navigator->calitem);

	/* This cancels any previous view requests still in progress. */
	date_nav_view_data_remove_all (gcal);

	g_return_if_fail (gcal->priv->sexp != NULL);

	/* Don't start a query unless a time range is set. */
	real_sexp = adjust_client_view_sexp (gcal, gcal->priv->sexp);
	if (real_sexp == NULL)
		return;

	list = e_cal_model_list_clients (gcal->priv->model);

	/* create queries for each loaded client */
	for (link = list; link != NULL; link = g_list_next (link)) {
		ECalClient *client = E_CAL_CLIENT (link->data);
		ViewData *view_data;

		view_data = view_data_new (gcal);
		date_nav_view_data_insert (gcal, view_data);

		e_cal_client_get_view (
			client, real_sexp,
			view_data->cancellable,
			gnome_cal_get_client_view_cb,
			view_data_ref (view_data));

		view_data_unref (view_data);
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	g_free (real_sexp);

	update_task_and_memo_views (gcal);
}

void
gnome_calendar_set_search_query (GnomeCalendar *gcal,
                                 const gchar *sexp,
                                 gboolean range_search,
                                 time_t start_range,
                                 time_t end_range)
{
	GnomeCalendarPrivate *priv;
	ECalModel *model;
	gint i;
	time_t start, end;

	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (sexp != NULL);

	priv = gcal->priv;

	model = gnome_calendar_get_model (gcal);

	/* Set the query on the date navigator */

	g_free (priv->sexp);
	priv->sexp = g_strdup (sexp);

	priv->lview_select_daten_range = !range_search;
	start = start_range;
	end = end_range;

	d (g_print ("Changing the queries %s \n", sexp));

	gnome_calendar_update_query (gcal);

	i = priv->current_view_type;

	/* Set the query on the views */
	if (i == GNOME_CAL_LIST_VIEW && !priv->lview_select_daten_range) {
		start = priv->base_view_time;
		get_times_for_views (gcal, GNOME_CAL_LIST_VIEW, &start, &end, NULL);

		e_cal_model_set_search_query_with_time_range (
			model, sexp, start, end);

		if (priv->current_view_type == GNOME_CAL_LIST_VIEW)
			gnome_calendar_update_date_navigator (gcal);
	} else
		e_cal_model_set_search_query (model, sexp);
}

static void
update_task_and_memo_views (GnomeCalendar *gcal)
{
	if (gcal->priv->task_table != NULL) {
		ECalModel *task_model;
		ETaskTable *task_table;
		gchar *hide_completed_tasks_sexp;

		/* Set the query on the task pad. */

		task_table = E_TASK_TABLE (gcal->priv->task_table);
		task_model = e_task_table_get_model (task_table);

		hide_completed_tasks_sexp =
			calendar_config_get_hide_completed_tasks_sexp (FALSE);

		if (hide_completed_tasks_sexp != NULL) {
			if (gcal->priv->sexp != NULL) {
				gchar *search_query;

				search_query = g_strdup_printf (
					"(and %s %s)",
					hide_completed_tasks_sexp,
					gcal->priv->sexp);
				e_cal_model_set_search_query (
					task_model, search_query);
				g_free (search_query);
			} else {
				e_cal_model_set_search_query (
					task_model, hide_completed_tasks_sexp);
			}
		} else {
			e_cal_model_set_search_query (
				task_model, gcal->priv->sexp);
		}

		g_free (hide_completed_tasks_sexp);
	}

	if (gcal->priv->memo_table != NULL) {
		ECalModel *memo_model;
		ECalModel *view_model;
		EMemoTable *memo_table;
		time_t start = -1, end = -1;

		/* Set the query on the memo pad. */

		memo_table = E_MEMO_TABLE (gcal->priv->memo_table);
		memo_model = e_memo_table_get_model (memo_table);

		view_model = gnome_calendar_get_model (gcal);
		e_cal_model_get_time_range (view_model, &start, &end);

		if (start != -1 && end != -1) {
			gchar *search_query;
			gchar *iso_start;
			gchar *iso_end;

			iso_start = isodate_from_time_t (start);
			iso_end = isodate_from_time_t (end);

			search_query = g_strdup_printf (
				"(and (or (not (has-start?)) "
				"(occur-in-time-range? (make-time \"%s\") "
				"(make-time \"%s\") \"%s\")) %s)",
				iso_start, iso_end,
				gcal_get_default_tzloc (gcal),
				gcal->priv->sexp ? gcal->priv->sexp : "");

			e_cal_model_set_search_query (
				memo_model, search_query);

			g_free (search_query);
			g_free (iso_start);
			g_free (iso_end);
		}
	}
}

static gboolean
update_marcus_bains_line_cb (gpointer user_data)
{
	GnomeCalendar *gcal;
	GnomeCalendarViewType view_type;
	ECalendarView *view;
	time_t now, day_begin;

	gcal = GNOME_CALENDAR (user_data);
	view_type = gnome_calendar_get_view (gcal);
	view = gnome_calendar_get_calendar_view (gcal, view_type);

	if (E_IS_DAY_VIEW (view))
		e_day_view_marcus_bains_update (E_DAY_VIEW (view));

	time (&now);
	day_begin = time_day_begin (now);

	/* check in the first two minutes */
	if (now >= day_begin && now <= day_begin + 120) {
		time_t start_time = 0, end_time = 0;

		g_return_val_if_fail (view != NULL, TRUE);

		e_calendar_view_get_selected_time_range (view, &start_time, &end_time);

		if (end_time >= time_add_day (day_begin, -1) && start_time <= day_begin) {
			gnome_calendar_goto (gcal, now);
		}
	}

	return TRUE;
}

static void
setup_widgets (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;

	priv = gcal->priv;

	/* update_task_and_memo_views (gcal); */

	/* Timeout check to hide completed items */
#if 0 /* KILL-BONOBO */
	priv->update_timeout = g_timeout_add_full (
		G_PRIORITY_LOW, 60000, (GSourceFunc)
		update_task_and_memo_views_cb, gcal, NULL);
#endif

	/* The Marcus Bains line */
	priv->update_marcus_bains_line_timeout =
		e_named_timeout_add_seconds_full (
			G_PRIORITY_LOW, 60,
			update_marcus_bains_line_cb, gcal, NULL);

	/* update_memo_view (gcal); */
}

/* Object initialization function for the gnome calendar */
static void
gnome_calendar_init (GnomeCalendar *gcal)
{
	GHashTable *date_nav_view_data;

	date_nav_view_data = g_hash_table_new_full (
		(GHashFunc) g_direct_hash,
		(GEqualFunc) g_direct_equal,
		(GDestroyNotify) view_data_unref,
		(GDestroyNotify) NULL);

	gcal->priv = GNOME_CALENDAR_GET_PRIVATE (gcal);

	g_mutex_init (&gcal->priv->date_nav_view_data_lock);

	gcal->priv->current_view_type = GNOME_CAL_WORK_WEEK_VIEW;
	gcal->priv->range_selected = FALSE;
	gcal->priv->lview_select_daten_range = TRUE;

	setup_widgets (gcal);

	gcal->priv->date_nav_view_data = date_nav_view_data;

	gcal->priv->sexp = g_strdup ("#t"); /* Match all */

	gcal->priv->visible_start = -1;
	gcal->priv->visible_end = -1;
	gcal->priv->updating = FALSE;

	gcal->priv->cancellable = g_cancellable_new ();
}

static void
gnome_calendar_do_dispose (GObject *object)
{
	GnomeCalendarPrivate *priv;
	gint ii;

	priv = GNOME_CALENDAR_GET_PRIVATE (object);

	if (priv->registry != NULL) {
		g_object_unref (priv->registry);
		priv->registry = NULL;
	}

	if (priv->model != NULL) {
		g_signal_handlers_disconnect_by_data (priv->model, object);
		e_signal_disconnect_notify_handler (priv->model, &priv->notify_week_start_day_id);
		g_object_unref (priv->model);
		priv->model = NULL;
	}

	for (ii = 0; ii < GNOME_CAL_LAST_VIEW; ii++) {
		if (priv->views[ii] != NULL) {
			g_object_unref (priv->views[ii]);
			priv->views[ii] = NULL;
		}
	}

	g_hash_table_remove_all (priv->date_nav_view_data);

	if (priv->sexp) {
		g_free (priv->sexp);
		priv->sexp = NULL;
	}

	if (priv->update_timeout) {
		g_source_remove (priv->update_timeout);
		priv->update_timeout = 0;
	}

	if (priv->update_marcus_bains_line_timeout) {
		g_source_remove (priv->update_marcus_bains_line_timeout);
		priv->update_marcus_bains_line_timeout = 0;
	}

	if (priv->cancellable) {
		g_cancellable_cancel (priv->cancellable);
		g_object_unref (priv->cancellable);
		priv->cancellable = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (gnome_calendar_parent_class)->dispose (object);
}

static void
gnome_calendar_finalize (GObject *object)
{
	GnomeCalendarPrivate *priv;

	priv = GNOME_CALENDAR_GET_PRIVATE (object);

	g_mutex_clear (&priv->date_nav_view_data_lock);

	g_hash_table_destroy (priv->date_nav_view_data);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (gnome_calendar_parent_class)->finalize (object);
}

static void
notify_selected_time_changed (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	gint i;

	priv = gcal->priv;
	for (i = 0; i < GNOME_CAL_LAST_VIEW; i++) {
		g_signal_emit_by_name (priv->views[i], "selected_time_changed");
	}
}

static void
gnome_calendar_goto_date (GnomeCalendar *gcal,
                          GnomeCalendarGotoDateType goto_date)
{
	ECalModel *model;
	time_t new_time = 0;
	GDateWeekday week_start_day;
	gboolean need_updating = FALSE;
	icaltimezone *timezone;

	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	model = gnome_calendar_get_model (gcal);
	week_start_day = e_cal_model_get_week_start_day (model);
	timezone = e_cal_model_get_timezone (model);

	switch (goto_date) {
		/* GNOME_CAL_GOTO_TODAY and GNOME_CAL_GOTO_DATE are
		 * currently not used
		*/
	case GNOME_CAL_GOTO_TODAY:
		break;
	case GNOME_CAL_GOTO_DATE:
		break;
	case GNOME_CAL_GOTO_FIRST_DAY_OF_MONTH:
		new_time = time_month_begin_with_zone (
			gcal->priv->base_view_time, timezone);
		need_updating = TRUE;
		break;
	case GNOME_CAL_GOTO_LAST_DAY_OF_MONTH:
		new_time = time_add_month_with_zone (
			gcal->priv->base_view_time, 1, timezone);
		new_time = time_month_begin_with_zone (new_time, timezone);
		new_time = time_add_day_with_zone (new_time, -1, timezone);
		need_updating = TRUE;
		break;
	case GNOME_CAL_GOTO_FIRST_DAY_OF_WEEK:
		new_time = time_week_begin_with_zone (
			gcal->priv->base_view_time,
			e_weekday_to_tm_wday (week_start_day),
			timezone);
		need_updating = TRUE;
		break;
	case GNOME_CAL_GOTO_LAST_DAY_OF_WEEK:
		new_time = time_week_begin_with_zone (
			gcal->priv->base_view_time,
			e_weekday_to_tm_wday (week_start_day),
			timezone);
		if (gcal->priv->current_view_type == GNOME_CAL_DAY_VIEW ||
		    gcal->priv->current_view_type == GNOME_CAL_WORK_WEEK_VIEW) {
			/* FIXME Shouldn't hard code work week end */
			/* goto Friday of this week */
			new_time = time_add_day_with_zone (new_time, 4, timezone);
		} else {
			/* goto Sunday of this week */
			/* FIXME Shouldn't hard code week end */
			new_time = time_add_day_with_zone (new_time, 6, timezone);
		}
		need_updating = TRUE;
		break;
	case GNOME_CAL_GOTO_SAME_DAY_OF_PREVIOUS_WEEK:
		new_time = time_add_week_with_zone (
			gcal->priv->base_view_time, -1, timezone);
		need_updating = TRUE;
		break;
	case GNOME_CAL_GOTO_SAME_DAY_OF_NEXT_WEEK:
		new_time = time_add_week_with_zone (
			gcal->priv->base_view_time, 1, timezone);
		need_updating = TRUE;
		break;
	default:
		break;
	}

	if (need_updating) {
		gnome_calendar_set_selected_time_range (gcal, new_time);
		notify_selected_time_changed (gcal);
	}
}

void
gnome_calendar_goto (GnomeCalendar *gcal,
                     time_t new_time)
{
	GnomeCalendarPrivate *priv;
	gint i;

	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (new_time != -1);

	priv = gcal->priv;

	gnome_calendar_set_selected_time_range (gcal, new_time);

	for (i = 0; i < GNOME_CAL_LAST_VIEW; i++)
		e_calendar_view_set_selected_time_range (
			priv->views[i], new_time, new_time);
}

void
gnome_calendar_update_view_times (GnomeCalendar *gcal,
                                  time_t start_time)
{
	GnomeCalendarPrivate *priv;
	ECalModel *model;
	time_t real_start_time = start_time;
	time_t end_time, select_time = 0;

	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	priv = gcal->priv;

	priv->base_view_time = start_time;

	model = gnome_calendar_get_model (gcal);

	get_times_for_views (
		gcal, priv->current_view_type,
		&real_start_time, &end_time, &select_time);

	if (priv->current_view_type == GNOME_CAL_LIST_VIEW &&
		!priv->lview_select_daten_range)
		return;

	e_cal_model_set_time_range (model, real_start_time, end_time);

	if (select_time != 0 && select_time >= real_start_time && select_time <= end_time)
		e_calendar_view_set_selected_time_range (
			priv->views[priv->current_view_type],
			select_time, select_time);
}

static void
gnome_calendar_direction (GnomeCalendar *gcal,
                          gint direction)
{
	ECalModel *model;
	icaltimezone *timezone;

	model = gnome_calendar_get_model (gcal);
	timezone = e_cal_model_get_timezone (model);

	switch (gnome_calendar_get_view (gcal)) {
	case GNOME_CAL_DAY_VIEW:
		gcal->priv->base_view_time = time_add_day_with_zone (
			gcal->priv->base_view_time, direction, timezone);
		break;
	case GNOME_CAL_WORK_WEEK_VIEW:
	case GNOME_CAL_WEEK_VIEW:
		gcal->priv->base_view_time = time_add_week_with_zone (
			gcal->priv->base_view_time, direction, timezone);
		break;
	case GNOME_CAL_MONTH_VIEW:
	case GNOME_CAL_LIST_VIEW:
		gcal->priv->base_view_time = time_add_month_with_zone (
			gcal->priv->base_view_time, direction, timezone);
		break;
	default:
		g_return_if_reached ();
	}

	gnome_calendar_set_selected_time_range (
		gcal, gcal->priv->base_view_time);
}

void
gnome_calendar_next (GnomeCalendar *gcal)
{
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	gnome_calendar_direction (gcal, 1);
}

void
gnome_calendar_previous (GnomeCalendar *gcal)
{
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	gnome_calendar_direction (gcal, -1);
}

void
gnome_calendar_dayjump (GnomeCalendar *gcal,
                        time_t time)
{
	ECalModel *model;
	icaltimezone *timezone;

	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	model = gnome_calendar_get_model (gcal);
	timezone = e_cal_model_get_timezone (model);

	gcal->priv->base_view_time =
		time_day_begin_with_zone (time, timezone);

	gnome_calendar_update_view_times (gcal, gcal->priv->base_view_time);
	gnome_calendar_set_view (gcal, GNOME_CAL_DAY_VIEW);
}

void
gnome_calendar_goto_today (GnomeCalendar *gcal)
{
	GnomeCalendarViewType view_type;
	ECalendarView *view;

	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	view_type = gnome_calendar_get_view (gcal);
	view = gnome_calendar_get_calendar_view (gcal, view_type);

	gnome_calendar_goto (gcal, time (NULL));
	gtk_widget_grab_focus (GTK_WIDGET (view));
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
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), GNOME_CAL_DAY_VIEW);

	return gcal->priv->current_view_type;
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
gnome_calendar_set_view (GnomeCalendar *gcal,
                         GnomeCalendarViewType view_type)
{
	ECalendarView *calendar_view;
	gint ii;

	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	if (gcal->priv->current_view_type == view_type &&
	    E_CALENDAR_VIEW (gcal->priv->views[view_type])->in_focus)
		return;

	gcal->priv->current_view_type = view_type;
	gnome_calendar_set_range_selected (gcal, FALSE);

	E_CALENDAR_VIEW (gcal->priv->views[view_type])->in_focus = TRUE;
	for (ii = 0; ii < GNOME_CAL_LAST_VIEW; ii++) {
		if (ii == view_type)
			continue;
		E_CALENDAR_VIEW (gcal->priv->views[ii])->in_focus = FALSE;
	}

	calendar_view = gnome_calendar_get_calendar_view (gcal, view_type);
	gtk_widget_grab_focus (GTK_WIDGET (calendar_view));

	g_object_notify (G_OBJECT (gcal), "view");
}

void
gnome_calendar_display_view (GnomeCalendar *gcal,
                             GnomeCalendarViewType view_type)
{
	ECalendarView *view;
	gboolean preserve_day;
	gboolean range_selected;
	time_t start_time;

	view = gnome_calendar_get_calendar_view (gcal, view_type);

	/* Set the view without changing the selection or updating the date
	 * navigator. If a range of dates isn't selected, also reset the
	 * number of days/weeks shown to the default (i.e. 1 day for the
	 * day view or 6 weeks for the month view). */

	preserve_day = FALSE;

	switch (view_type) {
	case GNOME_CAL_DAY_VIEW:
		if (!gnome_calendar_get_range_selected (gcal))
			e_day_view_set_days_shown (E_DAY_VIEW (view), 1);

		gtk_widget_show (GTK_WIDGET (gcal->priv->date_navigator));
		break;

	case GNOME_CAL_WORK_WEEK_VIEW:
		preserve_day = TRUE;
		gtk_widget_show (GTK_WIDGET (gcal->priv->date_navigator));
		break;

	case GNOME_CAL_WEEK_VIEW:
		preserve_day = TRUE;
		gtk_widget_show (GTK_WIDGET (gcal->priv->date_navigator));
		break;

	case GNOME_CAL_MONTH_VIEW:
		if (!gnome_calendar_get_range_selected (gcal))
			e_week_view_set_weeks_shown (E_WEEK_VIEW (view), 6);

		preserve_day = TRUE;
		gtk_widget_show (GTK_WIDGET (gcal->priv->date_navigator));
		break;

	case GNOME_CAL_LIST_VIEW:
		if (!gcal->priv->lview_select_daten_range)
			gtk_widget_hide (GTK_WIDGET (gcal->priv->date_navigator));
		else
			gtk_widget_show (GTK_WIDGET (gcal->priv->date_navigator));
		break;

	default:
		g_return_if_reached ();
	}

	range_selected = gnome_calendar_get_range_selected (gcal);
	gnome_calendar_set_view (gcal, view_type);
	gnome_calendar_set_range_selected (gcal, range_selected);

	/* For the week & month views we want the selection in the date
	 * navigator to be rounded to the nearest week when the arrow buttons
	 * are pressed to move to the previous/next month. */
	g_object_set (
		gcal->priv->date_navigator->calitem,
		"preserve_day_when_moving", preserve_day, NULL);

	/* keep week days selected as before for a work week view */
	g_object_set (
		gcal->priv->date_navigator->calitem,
		"keep_wdays_on_weeknum_click",
		view_type == GNOME_CAL_WORK_WEEK_VIEW,
		NULL);

	if (!gcal->priv->base_view_time)
		start_time = time (NULL);
	else
		start_time = gcal->priv->base_view_time;

	gnome_calendar_set_selected_time_range (gcal, start_time);

}

GtkWidget *
gnome_calendar_new (ESourceRegistry *registry)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		GNOME_TYPE_CALENDAR,
		"registry", registry, NULL);
}

ESourceRegistry *
gnome_calendar_get_registry (GnomeCalendar *gcal)
{
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), NULL);

	return gcal->priv->registry;
}

ECalendar *
gnome_calendar_get_date_navigator (GnomeCalendar *gcal)
{
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), NULL);

	return gcal->priv->date_navigator;
}

void
gnome_calendar_set_date_navigator (GnomeCalendar *gcal,
                                   ECalendar *date_navigator)
{
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	if (gcal->priv->date_navigator == date_navigator)
		return;

	if (date_navigator != NULL) {
		g_return_if_fail (E_IS_CALENDAR (date_navigator));
		g_object_ref (date_navigator);
	}

	if (gcal->priv->date_navigator != NULL)
		g_object_unref (gcal->priv->date_navigator);

	gcal->priv->date_navigator = date_navigator;

	/* Update the new date navigator */
	gnome_calendar_update_date_navigator (gcal);

	g_object_notify (G_OBJECT (gcal), "date-navigator");
}

GtkWidget *
gnome_calendar_get_memo_table (GnomeCalendar *gcal)
{
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), NULL);

	return gcal->priv->memo_table;
}

void
gnome_calendar_set_memo_table (GnomeCalendar *gcal,
                               GtkWidget *memo_table)
{
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	if (gcal->priv->memo_table == memo_table)
		return;

	if (memo_table != NULL) {
		g_return_if_fail (E_IS_MEMO_TABLE (memo_table));
		g_object_ref (memo_table);
	}

	if (gcal->priv->memo_table != NULL)
		g_object_unref (gcal->priv->memo_table);

	gcal->priv->memo_table = memo_table;

	g_object_notify (G_OBJECT (gcal), "memo-table");
}

GtkWidget *
gnome_calendar_get_task_table (GnomeCalendar *gcal)
{
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), NULL);

	return gcal->priv->task_table;
}

void
gnome_calendar_set_task_table (GnomeCalendar *gcal,
                               GtkWidget *task_table)
{
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	if (gcal->priv->task_table == task_table)
		return;

	if (task_table != NULL) {
		g_return_if_fail (E_IS_TASK_TABLE (task_table));
		g_object_ref (task_table);
	}

	if (gcal->priv->task_table != NULL)
		g_object_unref (gcal->priv->task_table);

	gcal->priv->task_table = task_table;

	g_object_notify (G_OBJECT (gcal), "task-table");
}

/**
 * gnome_calendar_get_model:
 * @gcal: A calendar view.
 *
 * Queries the calendar model object that a calendar view is using.
 *
 * Return value: A calendar client interface object.
 **/
ECalModel *
gnome_calendar_get_model (GnomeCalendar *gcal)
{
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), NULL);

	return gcal->priv->model;
}

gboolean
gnome_calendar_get_range_selected (GnomeCalendar *gcal)
{
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), FALSE);

	return gcal->priv->range_selected;
}

void
gnome_calendar_set_range_selected (GnomeCalendar *gcal,
                                   gboolean range_selected)
{
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	gcal->priv->range_selected = range_selected;
}

void
gnome_calendar_set_selected_time_range (GnomeCalendar *gcal,
                                        time_t start_time)
{
	gnome_calendar_update_view_times (gcal, start_time);
	gnome_calendar_update_date_navigator (gcal);
	gnome_calendar_notify_dates_shown_changed (gcal);
}

/**
 * gnome_calendar_new_task:
 * @gcal: An Evolution calendar.
 * @dtstart: Start time of the task, in same timezone as model.
 * @dtend: End time of the task, in same timezone as model.
 *
 * Opens a task editor dialog for a new task. dtstart or dtend can be NULL.
 **/
#if 0 /* KILL-BONOBO */
void
gnome_calendar_new_task (GnomeCalendar *gcal,
                         time_t *dtstart,
                         time_t *dtend)
{
	GnomeCalendarPrivate *priv;
	ECal *ecal;
	ECalModel *model;
	CompEditor *editor;
	ECalComponent *comp;
	icalcomponent *icalcomp;
	const gchar *category;
	guint32 flags = 0;
	ECalComponentDateTime dt;
	struct icaltimetype itt;

	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	priv = gcal->priv;
	model = e_calendar_table_get_model (E_CALENDAR_TABLE (priv->todo));
	ecal = e_cal_model_get_default_client (model);
	if (!ecal)
		return;

	flags |= COMP_EDITOR_NEW_ITEM;
	editor = task_editor_new (ecal, flags);

	icalcomp = e_cal_model_create_component_with_defaults (model, FALSE);
	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomp);

	dt.value = &itt;
	dt.tzid = icaltimezone_get_tzid (e_cal_model_get_timezone (model));

	if (dtstart) {
		itt = icaltime_from_timet_with_zone (
			*dtstart, FALSE, e_cal_model_get_timezone (model));
		e_cal_component_set_dtstart (comp, &dt);
	}

	if (dtend) {
		itt = icaltime_from_timet_with_zone (
			*dtend, FALSE, e_cal_model_get_timezone (model));
		e_cal_component_set_due (comp, &dt); /* task uses 'due' not 'dtend' */
	}

	if (dtstart || dtend)
		e_cal_component_commit_sequence (comp);

	comp_editor_edit_comp (editor, comp);
	g_object_unref (comp);

	gtk_window_present (GTK_WINDOW (editor));
}
#endif

/* Returns the selected time range for the current view. Note that this may be
 * different from the fields in the GnomeCalendar, since the view may clip
 * this or choose a more appropriate time. */
void
gnome_calendar_get_current_time_range (GnomeCalendar *gcal,
                                       time_t *start_time,
                                       time_t *end_time)
{
	GnomeCalendarViewType view_type;
	ECalendarView *view;

	view_type = gnome_calendar_get_view (gcal);
	view = gnome_calendar_get_calendar_view (gcal, view_type);

	e_calendar_view_get_selected_time_range (view, start_time, end_time);
}

/* This updates the month shown and the days selected in the calendar, if
 * necessary. */
static void
gnome_calendar_update_date_navigator (GnomeCalendar *gcal)
{
	GnomeCalendarPrivate *priv;
	ECalModel *model;
	time_t start, end;
	GDateWeekday week_start_day;
	GDate start_date, end_date;
	icaltimezone *timezone;

	priv = gcal->priv;

	/* If the ECalendar is not yet set, we just return. */
	if (priv->date_navigator == NULL)
		return;

	/* If the ECalendar isn't visible, we just return. */
	if (!gtk_widget_get_visible (GTK_WIDGET (priv->date_navigator)))
		return;

	if (priv->current_view_type == GNOME_CAL_LIST_VIEW &&
		!priv->lview_select_daten_range)
		return;

	model = gnome_calendar_get_model (gcal);
	timezone = e_cal_model_get_timezone (model);
	week_start_day = e_cal_model_get_week_start_day (model);
	e_cal_model_get_time_range (model, &start, &end);

	time_to_gdate_with_zone (&start_date, start, timezone);
	if (priv->current_view_type == GNOME_CAL_MONTH_VIEW) {
		EWeekView *week_view = E_WEEK_VIEW (priv->views[priv->current_view_type]);

		if (week_start_day == G_DATE_MONDAY &&
		   (!e_week_view_get_multi_week_view (week_view) ||
		    e_week_view_get_compress_weekend (week_view)))
			g_date_add_days (&start_date, 1);
	}
	time_to_gdate_with_zone (&end_date, end, timezone);
	g_date_subtract_days (&end_date, 1);

	e_calendar_item_set_selection (
		priv->date_navigator->calitem,
		&start_date, &end_date);
}

void
gnome_calendar_notify_dates_shown_changed (GnomeCalendar *gcal)
{
	GnomeCalendarViewType view_type;
	ECalendarView *calendar_view;
	GnomeCalendarPrivate *priv;
	time_t start_time, end_time;
	gboolean has_time_range;

	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	priv = gcal->priv;

	view_type = gnome_calendar_get_view (gcal);
	calendar_view = gnome_calendar_get_calendar_view (gcal, view_type);

	/* If no time range is set yet, just return. */
	has_time_range = e_calendar_view_get_visible_time_range (
		calendar_view, &start_time, &end_time);
	if (!has_time_range)
		return;

	/* We check if the visible date range has changed, and only emit the
	 * signal if it has. (This makes sure we only change the folder title
	 * bar label in the shell when we need to.) */
	if (priv->visible_start != start_time
	    || priv->visible_end != end_time) {
		priv->visible_start = start_time;
		priv->visible_end = end_time;

		gtk_widget_queue_draw (GTK_WIDGET (calendar_view));
		g_signal_emit (gcal, signals[DATES_SHOWN_CHANGED], 0);
	}
	update_task_and_memo_views (gcal);
}

/* Returns the number of selected events (0 or 1 at present). */
gint
gnome_calendar_get_num_events_selected (GnomeCalendar *gcal)
{
	GnomeCalendarViewType view_type;
	ECalendarView *view;
	gint retval = 0;

	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), 0);

	view_type = gnome_calendar_get_view (gcal);
	view = gnome_calendar_get_calendar_view (gcal, view_type);

	if (E_IS_DAY_VIEW (view))
		retval = e_day_view_get_num_events_selected (E_DAY_VIEW (view));
	else
		retval = e_week_view_get_num_events_selected (E_WEEK_VIEW (view));

	return retval;
}

struct purge_data {
	gboolean remove;
	time_t older_than;
};

static gboolean
check_instance_cb (ECalComponent *comp,
                   time_t instance_start,
                   time_t instance_end,
                   gpointer data)
{
	struct purge_data *pd = data;

	if (instance_end >= pd->older_than)
		pd->remove = FALSE;

	return pd->remove;
}

void
gnome_calendar_purge (GnomeCalendar *gcal,
                      time_t older_than)
{
	ECalModel *model;
	gchar *sexp, *start, *end;
	GList *list, *link;

	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	model = gnome_calendar_get_model (gcal);

	start = isodate_from_time_t (0);
	end = isodate_from_time_t (older_than);
	sexp = g_strdup_printf (
		"(occur-in-time-range? (make-time \"%s\") (make-time \"%s\") \"%s\")",
		start, end, gcal_get_default_tzloc (gcal));

	gcal_update_status_message (gcal, _("Purging"), -1);

	/* FIXME Confirm expunge */
	list = e_cal_model_list_clients (model);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ECalClient *client = E_CAL_CLIENT (link->data);
		GSList *objects, *m;
		GError *error = NULL;

		if (e_client_is_readonly (E_CLIENT (client)))
			continue;

		e_cal_client_get_object_list_sync (
			client, sexp, &objects, NULL, &error);

		if (error != NULL) {
			g_warning (
				"%s: Could not get the objects: %s",
				G_STRFUNC, error->message);
			g_error_free (error);
			continue;
		}

		for (m = objects; m; m = m->next) {
			gboolean remove = TRUE;

			/* FIXME write occur-before and occur-after
			 * sexp funcs so we don't have to use the max
			 * gint */
			if (!e_cal_client_check_recurrences_no_master (client)) {
				struct purge_data pd;

				pd.remove = TRUE;
				pd.older_than = older_than;

				e_cal_client_generate_instances_for_object_sync (client, m->data,
							     older_than, G_MAXINT32,
							     check_instance_cb,
							     &pd);

				remove = pd.remove;
			}

			/* FIXME Better error handling */
			if (remove) {
				const gchar *uid = icalcomponent_get_uid (m->data);
				GError *error = NULL;

				if (e_cal_util_component_is_instance (m->data) ||
					e_cal_util_component_has_recurrences (m->data)) {
					gchar *rid = NULL;
					struct icaltimetype recur_id;

					recur_id = icalcomponent_get_recurrenceid (m->data);

					if (!icaltime_is_null_time (recur_id))
						rid = icaltime_as_ical_string_r (recur_id);

					e_cal_client_remove_object_sync (
						client, uid, rid,
						CALOBJ_MOD_ALL, NULL, &error);
					g_free (rid);
				} else {
					e_cal_client_remove_object_sync (
						client, uid, NULL,
						CALOBJ_MOD_THIS, NULL, &error);
				}

				if (error != NULL) {
					g_warning (
						"%s: Unable to purge events: %s",
						G_STRFUNC, error->message);
					g_error_free (error);
				}
			}
		}

		g_slist_foreach (objects, (GFunc) icalcomponent_free, NULL);
		g_slist_free (objects);
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	gcal_update_status_message (gcal, NULL, -1);

	g_free (sexp);
	g_free (start);
	g_free (end);

}
