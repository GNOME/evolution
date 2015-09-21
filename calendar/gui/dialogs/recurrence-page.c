/*
 * Evolution calendar - Recurrence page of the calendar component dialogs
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
 *		Federico Mena-Quintero <federico@ximian.com>
 *		Miguel de Icaza <miguel@ximian.com>
 *		Seth Alves <alves@hungry.com>
 *		JP Rosevear <jpr@ximian.com>
 *		Hans Petter Jansson <hpj@ximiman.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "../tag-calendar.h"
#include "../e-weekday-chooser.h"
#include "comp-editor-util.h"
#include "../e-date-time-list.h"
#include "recurrence-page.h"

#define RECURRENCE_PAGE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), TYPE_RECURRENCE_PAGE, RecurrencePagePrivate))

enum month_num_options {
	MONTH_NUM_FIRST,
	MONTH_NUM_SECOND,
	MONTH_NUM_THIRD,
	MONTH_NUM_FOURTH,
	MONTH_NUM_FIFTH,
	MONTH_NUM_LAST,
	MONTH_NUM_DAY,
	MONTH_NUM_OTHER
};

static const gint month_num_options_map[] = {
	MONTH_NUM_FIRST,
	MONTH_NUM_SECOND,
	MONTH_NUM_THIRD,
	MONTH_NUM_FOURTH,
	MONTH_NUM_FIFTH,
	MONTH_NUM_LAST,
	MONTH_NUM_DAY,
	MONTH_NUM_OTHER,
	-1
};

enum month_day_options {
	MONTH_DAY_NTH,
	MONTH_DAY_MON,
	MONTH_DAY_TUE,
	MONTH_DAY_WED,
	MONTH_DAY_THU,
	MONTH_DAY_FRI,
	MONTH_DAY_SAT,
	MONTH_DAY_SUN
};

static const gint month_day_options_map[] = {
	MONTH_DAY_NTH,
	MONTH_DAY_MON,
	MONTH_DAY_TUE,
	MONTH_DAY_WED,
	MONTH_DAY_THU,
	MONTH_DAY_FRI,
	MONTH_DAY_SAT,
	MONTH_DAY_SUN,
	-1
};

enum recur_type {
	RECUR_NONE,
	RECUR_SIMPLE,
	RECUR_CUSTOM
};

static const gint type_map[] = {
	RECUR_NONE,
	RECUR_SIMPLE,
	RECUR_CUSTOM,
	-1
};

static const gint freq_map[] = {
	ICAL_DAILY_RECURRENCE,
	ICAL_WEEKLY_RECURRENCE,
	ICAL_MONTHLY_RECURRENCE,
	ICAL_YEARLY_RECURRENCE,
	-1
};

enum ending_type {
	ENDING_FOR,
	ENDING_UNTIL,
	ENDING_FOREVER
};

static const gint ending_types_map[] = {
	ENDING_FOR,
	ENDING_UNTIL,
	ENDING_FOREVER,
	-1
};

/* Private part of the RecurrencePage structure */
struct _RecurrencePagePrivate {
	/* Component we use to expand the recurrence rules for the preview */
	ECalComponent *comp;

	GtkBuilder *builder;

	/* Widgets from the UI file */
	GtkWidget *main;

	GtkWidget *recurs;
	gboolean custom;

	GtkWidget *params;
	GtkWidget *interval_value;
	GtkWidget *interval_unit_combo;
	GtkWidget *special;
	GtkWidget *ending_combo;
	GtkWidget *ending_special;
	GtkWidget *custom_warning_bin;

	/* For weekly recurrences, created by hand */
	GtkWidget *weekday_chooser;
	guint8 weekday_day_mask;
	guint8 weekday_blocked_day_mask;

	/* For monthly recurrences, created by hand */
	gint month_index;

	GtkWidget *month_day_combo;
	enum month_day_options month_day;

	GtkWidget *month_num_combo;
	enum month_num_options month_num;

	/* For ending date, created by hand */
	GtkWidget *ending_date_edit;
	struct icaltimetype ending_date_tt;

	/* For ending count of occurrences, created by hand */
	GtkWidget *ending_count_spin;
	gint ending_count;

	/* More widgets from the Glade file */
	GtkWidget *exception_list;  /* This is a GtkTreeView now */
	GtkWidget *exception_add;
	GtkWidget *exception_modify;
	GtkWidget *exception_delete;

	GtkWidget *preview_bin;

	/* Store for exception_list */
	EDateTimeList *exception_list_store;

	/* For the recurrence preview, the actual widget */
	GtkWidget *preview_calendar;

	/* This just holds some settings we need */
	EMeetingStore *meeting_store;

	GCancellable *cancellable;
};

static void recurrence_page_finalize (GObject *object);

static gboolean fill_component (RecurrencePage *rpage, ECalComponent *comp);
static GtkWidget *recurrence_page_get_widget (CompEditorPage *page);
static void recurrence_page_focus_main_widget (CompEditorPage *page);
static gboolean recurrence_page_fill_widgets (CompEditorPage *page, ECalComponent *comp);
static gboolean recurrence_page_fill_component (CompEditorPage *page, ECalComponent *comp);
static void recurrence_page_set_dates (CompEditorPage *page, CompEditorPageDates *dates);
static void preview_date_range_changed_cb (ECalendarItem *item, RecurrencePage *rpage);

static void make_ending_count_special (RecurrencePage *rpage);
static void make_ending_special (RecurrencePage *rpage);

G_DEFINE_TYPE (RecurrencePage, recurrence_page, TYPE_COMP_EDITOR_PAGE)

/* Re-tags the recurrence preview calendar based on the current information of
 * the widgets in the recurrence page.
 */
static void
preview_recur (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv = rpage->priv;
	CompEditor *editor;
	ECalClient *client;
	ECalComponent *comp;
	ECalComponentDateTime cdt;
	GSList *l;
	icaltimezone *zone = NULL;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (rpage));
	client = comp_editor_get_client (editor);

	/* If our component has not been set yet through ::fill_widgets(), we
	 * cannot preview the recurrence.
	 */
	if (!priv || !priv->comp || e_cal_component_is_instance (priv->comp))
		return;

	/* Create a scratch component with the start/end and
	 * recurrence/exception information from the one we are editing.
	 */

	comp = e_cal_component_new ();
	e_cal_component_set_new_vtype (comp, E_CAL_COMPONENT_EVENT);

	e_cal_component_get_dtstart (priv->comp, &cdt);
	if (cdt.tzid != NULL) {
		/* FIXME Will e_cal_client_get_timezone_sync really not return builtin zones? */
		e_cal_client_get_timezone_sync (
			client, cdt.tzid, &zone, NULL, NULL);
		if (zone == NULL)
			zone = icaltimezone_get_builtin_timezone_from_tzid (cdt.tzid);
	}
	e_cal_component_set_dtstart (comp, &cdt);
	e_cal_component_free_datetime (&cdt);

	e_cal_component_get_dtend (priv->comp, &cdt);
	e_cal_component_set_dtend (comp, &cdt);
	e_cal_component_free_datetime (&cdt);

	e_cal_component_get_exdate_list (priv->comp, &l);
	e_cal_component_set_exdate_list (comp, l);
	e_cal_component_free_exdate_list (l);

	e_cal_component_get_exrule_list (priv->comp, &l);
	e_cal_component_set_exrule_list (comp, l);
	e_cal_component_free_recur_list (l);

	e_cal_component_get_rdate_list (priv->comp, &l);
	e_cal_component_set_rdate_list (comp, l);
	e_cal_component_free_period_list (l);

	e_cal_component_get_rrule_list (priv->comp, &l);
	e_cal_component_set_rrule_list (comp, l);
	e_cal_component_free_recur_list (l);

	fill_component (rpage, comp);

	tag_calendar_by_comp (
		E_CALENDAR (priv->preview_calendar), comp,
		client, zone, TRUE, FALSE, FALSE, priv->cancellable);
	g_object_unref (comp);
}

static GObject *
recurrence_page_constructor (GType type,
                             guint n_construct_properties,
                             GObjectConstructParam *construct_properties)
{
	GObject *object;
	CompEditor *editor;

	/* Chain up to parent's constructor() method. */
	object = G_OBJECT_CLASS (recurrence_page_parent_class)->constructor (
		type, n_construct_properties, construct_properties);

	/* Keep the calendar updated as the user twizzles widgets. */
	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (object));

	e_signal_connect_notify_swapped (
		editor, "notify::changed",
		G_CALLBACK (preview_recur), object);

	return object;
}

static void
recurrence_page_dispose (GObject *object)
{
	RecurrencePagePrivate *priv;

	priv = RECURRENCE_PAGE_GET_PRIVATE (object);

	if (priv->main != NULL) {
		g_object_unref (priv->main);
		priv->main = NULL;
	}

	if (priv->builder != NULL) {
		g_object_unref (priv->builder);
		priv->builder = NULL;
	}

	if (priv->comp != NULL) {
		g_object_unref (priv->comp);
		priv->comp = NULL;
	}

	if (priv->exception_list_store != NULL) {
		g_object_unref (priv->exception_list_store);
		priv->exception_list_store = NULL;
	}

	if (priv->meeting_store != NULL) {
		g_object_unref (priv->meeting_store);
		priv->meeting_store = NULL;
	}

	if (priv->cancellable) {
		g_cancellable_cancel (priv->cancellable);
		g_object_unref (priv->cancellable);
		priv->cancellable = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (recurrence_page_parent_class)->dispose (object);
}

static void
recurrence_page_finalize (GObject *object)
{
	RecurrencePagePrivate *priv;

	priv = RECURRENCE_PAGE_GET_PRIVATE (object);

	g_signal_handlers_disconnect_matched (
		E_CALENDAR (priv->preview_calendar)->calitem,
		G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
		preview_date_range_changed_cb, NULL);

	g_signal_handlers_disconnect_matched (
		priv->interval_unit_combo, G_SIGNAL_MATCH_DATA,
		0, 0, NULL, NULL, object);

	g_signal_handlers_disconnect_matched (
		priv->ending_combo, G_SIGNAL_MATCH_DATA,
		0, 0, NULL, NULL, object);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (recurrence_page_parent_class)->finalize (object);
}

static void
recurrence_page_class_init (RecurrencePageClass *class)
{
	GObjectClass *object_class;
	CompEditorPageClass *editor_page_class;

	g_type_class_add_private (class, sizeof (RecurrencePagePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructor = recurrence_page_constructor;
	object_class->dispose = recurrence_page_dispose;
	object_class->finalize = recurrence_page_finalize;

	editor_page_class = COMP_EDITOR_PAGE_CLASS (class);
	editor_page_class->get_widget = recurrence_page_get_widget;
	editor_page_class->focus_main_widget = recurrence_page_focus_main_widget;
	editor_page_class->fill_widgets = recurrence_page_fill_widgets;
	editor_page_class->fill_component = recurrence_page_fill_component;
	editor_page_class->set_dates = recurrence_page_set_dates;

}

static void
recurrence_page_init (RecurrencePage *rpage)
{
	rpage->priv = RECURRENCE_PAGE_GET_PRIVATE (rpage);

	rpage->priv->cancellable = g_cancellable_new ();
}

/* get_widget handler for the recurrence page */
static GtkWidget *
recurrence_page_get_widget (CompEditorPage *page)
{
	RecurrencePagePrivate *priv;

	priv = RECURRENCE_PAGE_GET_PRIVATE (page);

	return priv->main;
}

/* focus_main_widget handler for the recurrence page */
static void
recurrence_page_focus_main_widget (CompEditorPage *page)
{
	RecurrencePagePrivate *priv;

	priv = RECURRENCE_PAGE_GET_PRIVATE (page);

	gtk_widget_grab_focus (priv->recurs);
}

/* Fills the widgets with default values */
static void
clear_widgets (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;
	GtkAdjustment *adj;

	priv = rpage->priv;

	priv->custom = FALSE;

	priv->weekday_day_mask = 0;

	priv->month_index = 1;
	priv->month_num = MONTH_NUM_DAY;
	priv->month_day = MONTH_DAY_NTH;

	g_signal_handlers_block_matched (priv->recurs, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->recurs), FALSE);
	g_signal_handlers_unblock_matched (priv->recurs, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);

	adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->interval_value));
	g_signal_handlers_block_matched (adj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (priv->interval_value), 1);
	g_signal_handlers_unblock_matched (adj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);

	g_signal_handlers_block_matched (priv->interval_unit_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
	e_dialog_combo_box_set (
		priv->interval_unit_combo,
		ICAL_DAILY_RECURRENCE,
		freq_map);
	g_signal_handlers_unblock_matched (priv->interval_unit_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);

	priv->ending_date_tt = icaltime_today ();
	priv->ending_count = 2;

	g_signal_handlers_block_matched (priv->ending_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
	e_dialog_combo_box_set (
		priv->ending_combo,
		priv->ending_count == -1 ? ENDING_FOREVER : ENDING_FOR,
		ending_types_map);
	g_signal_handlers_unblock_matched (priv->ending_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
	if (priv->ending_count == -1)
		priv->ending_count = 2;
	make_ending_special (rpage);
	/* Exceptions list */
	e_date_time_list_clear (priv->exception_list_store);
}

/* Appends an exception date to the list */
static void
append_exception (RecurrencePage *rpage,
                  ECalComponentDateTime *datetime)
{
	RecurrencePagePrivate *priv;
	GtkTreeView *view;
	GtkTreeIter  iter;

	priv = rpage->priv;
	view = GTK_TREE_VIEW (priv->exception_list);

	e_date_time_list_append (priv->exception_list_store, &iter, datetime);
	gtk_tree_selection_select_iter (gtk_tree_view_get_selection (view), &iter);
}

/* Fills in the exception widgets with the data from the calendar component */
static void
fill_exception_widgets (RecurrencePage *rpage,
                        ECalComponent *comp)
{
	GSList *list, *l;

	e_cal_component_get_exdate_list (comp, &list);

	for (l = list; l; l = l->next) {
		ECalComponentDateTime *cdt;

		cdt = l->data;
		append_exception (rpage, cdt);
	}

	e_cal_component_free_exdate_list (list);
}

/* Computes a weekday mask for the start day of a calendar component,
 * for use in a WeekdayPicker widget.
 */
static guint8
get_start_weekday_mask (ECalComponent *comp)
{
	ECalComponentDateTime dt;
	guint8 retval;

	e_cal_component_get_dtstart (comp, &dt);

	if (dt.value) {
		gshort weekday;

		weekday = icaltime_day_of_week (*dt.value);
		retval = 0x1 << (weekday - 1);
	} else
		retval = 0;

	e_cal_component_free_datetime (&dt);

	return retval;
}

/* Sets some sane defaults for the data sources for the recurrence special
 * widgets, even if they will not be used immediately.
 */
static void
set_special_defaults (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;
	guint8 mask;

	priv = rpage->priv;

	mask = get_start_weekday_mask (priv->comp);

	priv->weekday_day_mask = mask;
	priv->weekday_blocked_day_mask = mask;
}

/* Sensitizes the recurrence widgets based on the state of the recurrence type
 * radio group.
 */
static void
sensitize_recur_widgets (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv = rpage->priv;
	CompEditor *editor;
	CompEditorFlags flags;
	gboolean recurs, sens = TRUE;
	GtkWidget *child;
	GtkWidget *label;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (rpage));
	flags = comp_editor_get_flags (editor);

	if (flags & COMP_EDITOR_MEETING)
		sens = flags & COMP_EDITOR_USER_ORG;

	recurs = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->recurs));

	/* We can't preview that well for instances right now */
	if (e_cal_component_is_instance (priv->comp))
		gtk_widget_set_sensitive (priv->preview_calendar, FALSE);
	else
		gtk_widget_set_sensitive (priv->preview_calendar, TRUE && sens);

	child = gtk_bin_get_child (GTK_BIN (priv->custom_warning_bin));
	if (child != NULL)
		gtk_widget_destroy (child);

	if (recurs && priv->custom) {
		gtk_widget_set_sensitive (priv->params, FALSE);
		gtk_widget_hide (priv->params);

		label = gtk_label_new (
			_("This appointment contains "
			"recurrences that Evolution "
			"cannot edit."));
		gtk_container_add (
			GTK_CONTAINER (priv->custom_warning_bin),
			label);
		gtk_widget_show_all (priv->custom_warning_bin);
	} else if (recurs) {
		gtk_widget_set_sensitive (priv->params, sens);
		gtk_widget_show (priv->params);
		gtk_widget_hide (priv->custom_warning_bin);
	} else {
		gtk_widget_set_sensitive (priv->params, FALSE);
		gtk_widget_show (priv->params);
		gtk_widget_hide (priv->custom_warning_bin);
	}
}

static void
update_with_readonly (RecurrencePage *rpage,
                      gboolean read_only)
{
	RecurrencePagePrivate *priv = rpage->priv;
	CompEditor *editor;
	CompEditorFlags flags;
	gint selected_rows;
	gboolean sensitize = TRUE;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (rpage));
	flags = comp_editor_get_flags (editor);

	if (flags & COMP_EDITOR_MEETING)
		sensitize = flags & COMP_EDITOR_USER_ORG;

	selected_rows = gtk_tree_selection_count_selected_rows (
		gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->exception_list)));

	if (!read_only)
		sensitize_recur_widgets (rpage);
	else
		gtk_widget_set_sensitive (priv->params, FALSE);

	gtk_widget_set_sensitive (priv->recurs, !read_only && sensitize);
	gtk_widget_set_sensitive (priv->exception_add, !read_only && sensitize && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->recurs)));
	gtk_widget_set_sensitive (priv->exception_modify, !read_only && selected_rows > 0 && sensitize);
	gtk_widget_set_sensitive (priv->exception_delete, !read_only && selected_rows > 0 && sensitize);
}

static void
rpage_get_objects_for_uid_cb (GObject *source_object,
                              GAsyncResult *result,
                              gpointer user_data)
{
	ECalClient *client = E_CAL_CLIENT (source_object);
	RecurrencePage *rpage = user_data;
	GSList *ecalcomps = NULL;
	GError *error = NULL;

	if (result && !e_cal_client_get_objects_for_uid_finish (client, result, &ecalcomps, &error)) {
		ecalcomps = NULL;
		if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			g_clear_error (&error);
			return;
		}

		g_clear_error (&error);
	}

	if (g_slist_length (ecalcomps) > 1 && rpage->priv->comp) {
		icalcomponent *icalcomp;
		gboolean has_rrule;

		icalcomp = e_cal_component_get_icalcomponent (rpage->priv->comp);
		has_rrule = icalcomponent_get_first_property (icalcomp, ICAL_RRULE_PROPERTY) != NULL;

		if (has_rrule) {
			/* Not a detached instance, can edit recurrences */
			g_slist_free_full (ecalcomps, g_object_unref);
			ecalcomps = NULL;
		}
	}

	update_with_readonly (rpage, g_slist_length (ecalcomps) > 1);

	g_slist_free_full (ecalcomps, g_object_unref);
}

static void
rpage_get_object_cb (GObject *source_object,
                     GAsyncResult *result,
                     gpointer user_data)
{
	ECalClient *client = E_CAL_CLIENT (source_object);
	RecurrencePage *rpage = user_data;
	icalcomponent *icalcomp = NULL;
	const gchar *uid = NULL;
	GError *error = NULL;

	if (result && !e_cal_client_get_object_finish (client, result, &icalcomp, &error)) {
		icalcomp = NULL;
		if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			g_clear_error (&error);
			return;
		}

		g_clear_error (&error);
	}

	if (icalcomp) {
		icalcomponent_free (icalcomp);
		update_with_readonly (rpage, TRUE);
		return;
	}

	if (rpage->priv->comp)
		e_cal_component_get_uid (rpage->priv->comp, &uid);

	if (!uid || !*uid) {
		update_with_readonly (rpage, FALSE);
		return;
	}

	/* see if we have detached instances */
	e_cal_client_get_objects_for_uid (client, uid, rpage->priv->cancellable, rpage_get_objects_for_uid_cb, rpage);
}

static void
sensitize_buttons (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv = rpage->priv;
	CompEditor *editor;
	ECalClient *client;
	const gchar *uid;

	if (priv->comp == NULL)
		return;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (rpage));
	client = comp_editor_get_client (editor);

	if (e_client_is_readonly (E_CLIENT (client))) {
		update_with_readonly (rpage, TRUE);
		return;
	}

	if (priv->cancellable) {
		g_cancellable_cancel (priv->cancellable);
		g_object_unref (priv->cancellable);
	}
	priv->cancellable = g_cancellable_new ();

	e_cal_component_get_uid (priv->comp, &uid);
	if (!uid || !*uid) {
		update_with_readonly (rpage, FALSE);
		return;
	}

	if (e_client_check_capability (E_CLIENT (client), CAL_STATIC_CAPABILITY_NO_CONV_TO_RECUR)) {
		e_cal_client_get_object (client, uid, NULL, priv->cancellable, rpage_get_object_cb, rpage);
	} else {
		rpage_get_object_cb (G_OBJECT (client), NULL, rpage);
	}
}

/* Gets the simple recurrence data from the recurrence widgets and stores it in
 * the calendar component.
 */
static void
simple_recur_to_comp (RecurrencePage *rpage,
                      ECalComponent *comp)
{
	CompEditor *editor;
	RecurrencePagePrivate *priv;
	struct icalrecurrencetype r;
	GSList l;
	enum ending_type ending_type;
	gboolean date_set;

	priv = rpage->priv;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (rpage));

	icalrecurrencetype_clear (&r);

	/* Frequency, interval, week start */

	r.freq = e_dialog_combo_box_get (priv->interval_unit_combo, freq_map);
	r.interval = gtk_spin_button_get_value_as_int (
		GTK_SPIN_BUTTON (priv->interval_value));

	switch (comp_editor_get_week_start_day (editor)) {
		case G_DATE_MONDAY:
			r.week_start = ICAL_MONDAY_WEEKDAY;
			break;
		case G_DATE_TUESDAY:
			r.week_start = ICAL_TUESDAY_WEEKDAY;
			break;
		case G_DATE_WEDNESDAY:
			r.week_start = ICAL_WEDNESDAY_WEEKDAY;
			break;
		case G_DATE_THURSDAY:
			r.week_start = ICAL_THURSDAY_WEEKDAY;
			break;
		case G_DATE_FRIDAY:
			r.week_start = ICAL_FRIDAY_WEEKDAY;
			break;
		case G_DATE_SATURDAY:
			r.week_start = ICAL_SATURDAY_WEEKDAY;
			break;
		case G_DATE_SUNDAY:
			r.week_start = ICAL_SUNDAY_WEEKDAY;
			break;
		default:
			g_warn_if_reached ();
			break;
	}

	/* Frequency-specific data */

	switch (r.freq) {
	case ICAL_DAILY_RECURRENCE:
		/* Nothing else is required */
		break;

	case ICAL_WEEKLY_RECURRENCE: {
		EWeekdayChooser *chooser;
		gint i;

		g_return_if_fail (gtk_bin_get_child (GTK_BIN (priv->special)) != NULL);
		g_return_if_fail (E_IS_WEEKDAY_CHOOSER (priv->weekday_chooser));

		chooser = E_WEEKDAY_CHOOSER (priv->weekday_chooser);

		i = 0;

		if (e_weekday_chooser_get_selected (chooser, E_DATE_SUNDAY))
			r.by_day[i++] = ICAL_SUNDAY_WEEKDAY;

		if (e_weekday_chooser_get_selected (chooser, E_DATE_MONDAY))
			r.by_day[i++] = ICAL_MONDAY_WEEKDAY;

		if (e_weekday_chooser_get_selected (chooser, E_DATE_TUESDAY))
			r.by_day[i++] = ICAL_TUESDAY_WEEKDAY;

		if (e_weekday_chooser_get_selected (chooser, E_DATE_WEDNESDAY))
			r.by_day[i++] = ICAL_WEDNESDAY_WEEKDAY;

		if (e_weekday_chooser_get_selected (chooser, E_DATE_THURSDAY))
			r.by_day[i++] = ICAL_THURSDAY_WEEKDAY;

		if (e_weekday_chooser_get_selected (chooser, E_DATE_FRIDAY))
			r.by_day[i++] = ICAL_FRIDAY_WEEKDAY;

		if (e_weekday_chooser_get_selected (chooser, E_DATE_SATURDAY))
			r.by_day[i] = ICAL_SATURDAY_WEEKDAY;

		break;
	}

	case ICAL_MONTHLY_RECURRENCE: {
		enum month_num_options month_num;
		enum month_day_options month_day;

		g_return_if_fail (gtk_bin_get_child (GTK_BIN (priv->special)) != NULL);
		g_return_if_fail (priv->month_day_combo != NULL);
		g_return_if_fail (GTK_IS_COMBO_BOX (priv->month_day_combo));
		g_return_if_fail (priv->month_num_combo != NULL);
		g_return_if_fail (GTK_IS_COMBO_BOX (priv->month_num_combo));

		month_num = e_dialog_combo_box_get (
			priv->month_num_combo,
			month_num_options_map);
		month_day = e_dialog_combo_box_get (
			priv->month_day_combo,
			month_day_options_map);

		if (month_num == MONTH_NUM_LAST)
			month_num = -1;
		else if (month_num != -1)
			month_num++;
		else
			g_warn_if_reached ();

		switch (month_day) {
		case MONTH_DAY_NTH:
			if (month_num == -1)
				r.by_month_day[0] = -1;
			else
				r.by_month_day[0] = priv->month_index;
			break;

		/* Outlook 2000 uses BYDAY=TU;BYSETPOS=2, and will not
		 * accept BYDAY=2TU. So we now use the same as Outlook
		 * by default. */
		case MONTH_DAY_MON:
			r.by_day[0] = ICAL_MONDAY_WEEKDAY;
			r.by_set_pos[0] = month_num;
			break;

		case MONTH_DAY_TUE:
			r.by_day[0] = ICAL_TUESDAY_WEEKDAY;
			r.by_set_pos[0] = month_num;
			break;

		case MONTH_DAY_WED:
			r.by_day[0] = ICAL_WEDNESDAY_WEEKDAY;
			r.by_set_pos[0] = month_num;
			break;

		case MONTH_DAY_THU:
			r.by_day[0] = ICAL_THURSDAY_WEEKDAY;
			r.by_set_pos[0] = month_num;
			break;

		case MONTH_DAY_FRI:
			r.by_day[0] = ICAL_FRIDAY_WEEKDAY;
			r.by_set_pos[0] = month_num;
			break;

		case MONTH_DAY_SAT:
			r.by_day[0] = ICAL_SATURDAY_WEEKDAY;
			r.by_set_pos[0] = month_num;
			break;

		case MONTH_DAY_SUN:
			r.by_day[0] = ICAL_SUNDAY_WEEKDAY;
			r.by_set_pos[0] = month_num;
			break;

		default:
			g_return_if_reached ();
		}

		break;
	}

	case ICAL_YEARLY_RECURRENCE:
		/* Nothing else is required */
		break;

	default:
		g_return_if_reached ();
	}

	/* Ending date */

	ending_type = e_dialog_combo_box_get (priv->ending_combo, ending_types_map);

	switch (ending_type) {
	case ENDING_FOR:
		g_return_if_fail (priv->ending_count_spin != NULL);
		g_return_if_fail (GTK_IS_SPIN_BUTTON (priv->ending_count_spin));

		r.count = gtk_spin_button_get_value_as_int (
			GTK_SPIN_BUTTON (priv->ending_count_spin));
		break;

	case ENDING_UNTIL:
		g_return_if_fail (priv->ending_date_edit != NULL);
		g_return_if_fail (E_IS_DATE_EDIT (priv->ending_date_edit));

		/* We only allow a DATE value to be set for the UNTIL property,
		 * since we don't support sub-day recurrences. */
		date_set = e_date_edit_get_date (
			E_DATE_EDIT (priv->ending_date_edit),
			&r.until.year,
			&r.until.month,
			&r.until.day);
		g_return_if_fail (date_set);

		r.until.is_date = 1;

		break;

	case ENDING_FOREVER:
		/* Nothing to be done */
		break;

	default:
		g_return_if_reached ();
	}

	/* Set the recurrence */

	l.data = &r;
	l.next = NULL;

	e_cal_component_set_rrule_list (comp, &l);
}

/* Fills a component with the data from the recurrence page; in the case of a
 * custom recurrence, it leaves it intact.
 */
static gboolean
fill_component (RecurrencePage *rpage,
                ECalComponent *comp)
{
	RecurrencePagePrivate *priv;
	gboolean recurs;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid_iter;
	GSList *list;

	priv = rpage->priv;
	model = GTK_TREE_MODEL (priv->exception_list_store);

	recurs = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->recurs));

	if (recurs && priv->custom) {
		/* We just keep whatever the component has currently */
	} else if (recurs) {
		e_cal_component_set_rdate_list (comp, NULL);
		e_cal_component_set_exrule_list (comp, NULL);
		simple_recur_to_comp (rpage, comp);
	} else {
		gboolean had_recurrences = e_cal_component_has_recurrences (comp);

		e_cal_component_set_rdate_list (comp, NULL);
		e_cal_component_set_rrule_list (comp, NULL);
		e_cal_component_set_exrule_list (comp, NULL);

		if (had_recurrences)
			e_cal_component_set_recurid (comp, NULL);
	}

	/* Set exceptions */

	list = NULL;

	for (valid_iter = gtk_tree_model_get_iter_first (model, &iter); valid_iter;
	     valid_iter = gtk_tree_model_iter_next (model, &iter)) {
		const ECalComponentDateTime *dt;
		ECalComponentDateTime *cdt;

		cdt = g_new (ECalComponentDateTime, 1);
		cdt->value = g_new (struct icaltimetype, 1);

		dt = e_date_time_list_get_date_time (E_DATE_TIME_LIST (model), &iter);
		g_return_val_if_fail (dt != NULL, FALSE);

		if (!icaltime_is_valid_time (*dt->value)) {
			comp_editor_page_display_validation_error (
				COMP_EDITOR_PAGE (rpage),
				_("Recurrence date is invalid"),
				priv->exception_list);
			return FALSE;
		}

		*cdt->value = *dt->value;
		cdt->tzid = g_strdup (dt->tzid);

		list = g_slist_prepend (list, cdt);
	}

	e_cal_component_set_exdate_list (comp, list);
	e_cal_component_free_exdate_list (list);

	if (gtk_widget_get_visible (priv->ending_combo) && gtk_widget_get_sensitive (priv->ending_combo) &&
	    e_dialog_combo_box_get (priv->ending_combo, ending_types_map) == ENDING_UNTIL) {
		/* check whether the "until" date is in the future */
		struct icaltimetype tt = icaltime_null_time ();
		gboolean ok = TRUE;

		if (e_date_edit_get_date (E_DATE_EDIT (priv->ending_date_edit), &tt.year, &tt.month, &tt.day)) {
			ECalComponentDateTime dtstart;

			/* the dtstart should be set already */
			e_cal_component_get_dtstart (comp, &dtstart);

			tt.is_date = 1;
			tt.zone = NULL;

			if (dtstart.value && icaltime_is_valid_time (*dtstart.value)) {
				ok = icaltime_compare_date_only (*dtstart.value, tt) <= 0;

				if (!ok)
					e_date_edit_set_date (E_DATE_EDIT (priv->ending_date_edit), dtstart.value->year, dtstart.value->month, dtstart.value->day);
				else {
					/* to have the date shown in "normalized" format */
					e_date_edit_set_date (E_DATE_EDIT (priv->ending_date_edit), tt.year, tt.month, tt.day);
				}
			}

			e_cal_component_free_datetime (&dtstart);
		}

		if (!ok) {
			comp_editor_page_display_validation_error (COMP_EDITOR_PAGE (rpage), _("End time of the recurrence was before event's start"), priv->ending_date_edit);
			return FALSE;
		}
	}

	return TRUE;
}

/* Creates the special contents for weekly recurrences */
static void
make_weekly_special (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;
	GtkWidget *hbox;
	GtkWidget *label;
	EWeekdayChooser *chooser;

	priv = rpage->priv;

	g_return_if_fail (gtk_bin_get_child (GTK_BIN (priv->special)) == NULL);
	g_return_if_fail (priv->weekday_chooser == NULL);

	/* Create the widgets */

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_container_add (GTK_CONTAINER (priv->special), hbox);

	/* TRANSLATORS: Entire string is for example: This appointment recurs/Every [x] week(s) on [Wednesday] [forever]'
	 * (dropdown menu options are in [square brackets]). This means that after the 'on', name of a week day always follows. */
	label = gtk_label_new (_("on"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 6);

	priv->weekday_chooser = e_weekday_chooser_new ();
	chooser = E_WEEKDAY_CHOOSER (priv->weekday_chooser);

	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (chooser), FALSE, FALSE, 6);

	gtk_widget_show_all (hbox);

	/* Set the weekdays */

	e_weekday_chooser_set_selected (
		chooser, G_DATE_SUNDAY,
		(priv->weekday_day_mask & (1 << 0)) != 0);
	e_weekday_chooser_set_selected (
		chooser, G_DATE_MONDAY,
		(priv->weekday_day_mask & (1 << 1)) != 0);
	e_weekday_chooser_set_selected (
		chooser, G_DATE_TUESDAY,
		(priv->weekday_day_mask & (1 << 2)) != 0);
	e_weekday_chooser_set_selected (
		chooser, G_DATE_WEDNESDAY,
		(priv->weekday_day_mask & (1 << 3)) != 0);
	e_weekday_chooser_set_selected (
		chooser, G_DATE_THURSDAY,
		(priv->weekday_day_mask & (1 << 4)) != 0);
	e_weekday_chooser_set_selected (
		chooser, G_DATE_FRIDAY,
		(priv->weekday_day_mask & (1 << 5)) != 0);
	e_weekday_chooser_set_selected (
		chooser, G_DATE_SATURDAY,
		(priv->weekday_day_mask & (1 << 6)) != 0);

	g_signal_connect_swapped (
		chooser, "changed",
		G_CALLBACK (comp_editor_page_changed), rpage);
}

/* Creates the subtree for the monthly recurrence number */
static void
make_recur_month_num_subtree (GtkTreeStore *store,
                              GtkTreeIter *par,
                              const gchar *title,
                              gint start,
                              gint end)
{
	GtkTreeIter iter, parent;
	gint i;

	gtk_tree_store_append (store, &parent, par);
	gtk_tree_store_set (store, &parent, 0, _(title), 1, -1, -1);

	for (i = start; i < end; i++) {
		gtk_tree_store_append (store, &iter, &parent);
		gtk_tree_store_set (store, &iter, 0, _(e_cal_recur_nth[i]), 1, i + 1, -1);
	}
}

static void
only_leaf_sensitive (GtkCellLayout *cell_layout,
                     GtkCellRenderer *cell,
                     GtkTreeModel *tree_model,
                     GtkTreeIter *iter,
                     gpointer data)
{
  gboolean sensitive;

  sensitive = !gtk_tree_model_iter_has_child (tree_model, iter);

  g_object_set (cell, "sensitive", sensitive, NULL);
}

static GtkWidget *
make_recur_month_num_combo (gint month_index)
{
	static const gchar *options[] = {
		/* TRANSLATORS: Entire string is for example: This appointment recurs/Every [x] month(s) on the [first] [Monday] [forever]'
		 * (dropdown menu options are in [square brackets]). This means that after 'first', either the string 'day' or
		 * the name of a week day (like 'Monday' or 'Friday') always follow.
		 */
		N_("first"),
		/* TRANSLATORS: here, "second" is the ordinal number (like "third"), not the time division (like "minute")
		 * Entire string is for example: This appointment recurs/Every [x] month(s) on the [second] [Monday] [forever]'
		 * (dropdown menu options are in [square brackets]). This means that after 'second', either the string 'day' or
		 * the name of a week day (like 'Monday' or 'Friday') always follow.
		 */
		N_("second"),
		/* TRANSLATORS: Entire string is for example: This appointment recurs/Every [x] month(s) on the [third] [Monday] [forever]'
		 * (dropdown menu options are in [square brackets]). This means that after 'third', either the string 'day' or
		 * the name of a week day (like 'Monday' or 'Friday') always follow.
		 */
		N_("third"),
		/* TRANSLATORS: Entire string is for example: This appointment recurs/Every [x] month(s) on the [fourth] [Monday] [forever]'
		 * (dropdown menu options are in [square brackets]). This means that after 'fourth', either the string 'day' or
		 * the name of a week day (like 'Monday' or 'Friday') always follow.
		 */
		N_("fourth"),
		/* TRANSLATORS: Entire string is for example: This appointment recurs/Every [x] month(s) on the [fifth] [Monday] [forever]'
		 * (dropdown menu options are in [square brackets]). This means that after 'fifth', either the string 'day' or
		 * the name of a week day (like 'Monday' or 'Friday') always follow.
		 */
		N_("fifth"),
		/* TRANSLATORS: Entire string is for example: This appointment recurs/Every [x] month(s) on the [last] [Monday] [forever]'
		 * (dropdown menu options are in [square brackets]). This means that after 'last', either the string 'day' or
		 * the name of a week day (like 'Monday' or 'Friday') always follow.
		 */
		N_("last")
	};

	gint i;
	GtkTreeStore *store;
	GtkTreeIter iter;
	GtkWidget *combo;
	GtkCellRenderer *cell;

	store = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_INT);

	/* Relation */
	for (i = 0; i < G_N_ELEMENTS (options); i++) {
		gtk_tree_store_append (store, &iter, NULL);
		gtk_tree_store_set (store, &iter, 0, _(options[i]), 1, month_num_options_map[i], -1);
	}

	/* Current date */
	gtk_tree_store_append (store, &iter, NULL);
	gtk_tree_store_set (store, &iter, 0, _(e_cal_recur_nth[month_index - 1]), 1, MONTH_NUM_DAY, -1);

	gtk_tree_store_append (store, &iter, NULL);
	/* TRANSLATORS: Entire string is for example: This appointment recurs/Every [x] month(s) on the [Other date] [11th to 20th] [17th] [forever]'
	 * (dropdown menu options are in [square brackets]). */
	gtk_tree_store_set (store, &iter, 0, _("Other Date"), 1, MONTH_NUM_OTHER, -1);

	/* TRANSLATORS: This is a submenu option string to split the date range into three submenus to choose the exact day of
	 * the month to setup an appointment recurrence. The entire string is for example: This appointment recurs/Every [x] month(s)
	 * on the [Other date] [1st to 10th] [7th] [forever]' (dropdown menu options are in [square brackets]).
	 */
	make_recur_month_num_subtree (store, &iter, _("1st to 10th"), 0, 10);

	/* TRANSLATORS: This is a submenu option string to split the date range into three submenus to choose the exact day of
	 * the month to setup an appointment recurrence. The entire string is for example: This appointment recurs/Every [x] month(s)
	 * on the [Other date] [11th to 20th] [17th] [forever]' (dropdown menu options are in [square brackets]).
	 */
	make_recur_month_num_subtree (store, &iter, _("11th to 20th"), 10, 20);

	/* TRANSLATORS: This is a submenu option string to split the date range into three submenus to choose the exact day of
	 * the month to setup an appointment recurrence. The entire string is for example: This appointment recurs/Every [x] month(s)
	 * on the [Other date] [21th to 31th] [27th] [forever]' (dropdown menu options are in [square brackets]).
	 */
	make_recur_month_num_subtree (store, &iter, _("21st to 31st"), 20, 31);

	combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
	g_object_unref (store);

	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), cell, "text", 0, NULL);

	gtk_cell_layout_set_cell_data_func (
		GTK_CELL_LAYOUT (combo),
		cell,
		only_leaf_sensitive,
		NULL, NULL);

	return combo;
}

/* Creates the combo box for the monthly recurrence days */
static GtkWidget *
make_recur_month_combobox (void)
{
	static const gchar *options[] = {
		/* For Translator : 'day' is part of the sentence of the form 'appointment recurs/Every [x] month(s) on the [first] [day] [forever]'
		 * (dropdown menu options are in[square brackets]). This means that after 'first', either the string 'day' or
		 * the name of a week day (like 'Monday' or 'Friday') always follow. */
		N_("day"),
		N_("Monday"),
		N_("Tuesday"),
		N_("Wednesday"),
		N_("Thursday"),
		N_("Friday"),
		N_("Saturday"),
		N_("Sunday")
	};

	GtkWidget *combo;
	gint i;

	combo = gtk_combo_box_text_new ();

	for (i = 0; i < G_N_ELEMENTS (options); i++) {
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _(options[i]));
	}

	return combo;
}

static void
month_num_combo_changed_cb (GtkComboBox *combo,
                            RecurrencePage *rpage)
{
	GtkTreeIter iter;
	RecurrencePagePrivate *priv;
	enum month_num_options month_num;
	enum month_day_options month_day;

	priv = rpage->priv;

	month_day = e_dialog_combo_box_get (
		priv->month_day_combo,
		month_day_options_map);

	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (priv->month_num_combo), &iter)) {
		gint value;
		GtkTreeIter parent;
		GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->month_num_combo));

		gtk_tree_model_get (model, &iter, 1, &value, -1);

		if (value == -1) {
			return;
		}

		if (gtk_tree_model_iter_parent (model, &parent, &iter)) {
			/* it's a leaf, thus the day number */
			month_num = MONTH_NUM_DAY;
			priv->month_index = value;

			g_return_if_fail (gtk_tree_model_iter_nth_child (model, &iter, NULL, month_num));

			gtk_tree_store_set (GTK_TREE_STORE (model), &iter, 0, _(e_cal_recur_nth[priv->month_index - 1]), -1);
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (priv->month_num_combo), &iter);
		} else {
			/* top level node */
			month_num = value;

			if (month_num == MONTH_NUM_OTHER)
				month_num = MONTH_NUM_DAY;
		}
	} else {
		month_num = 0;
	}

	if (month_num == MONTH_NUM_DAY && month_day != MONTH_DAY_NTH)
		e_dialog_combo_box_set (
			priv->month_day_combo,
			MONTH_DAY_NTH,
			month_day_options_map);
	else if (month_num != MONTH_NUM_DAY && month_num != MONTH_NUM_LAST && month_day == MONTH_DAY_NTH)
		e_dialog_combo_box_set (
			priv->month_day_combo,
			MONTH_DAY_MON,
			month_num_options_map);

	comp_editor_page_changed (COMP_EDITOR_PAGE (rpage));
}

/* Callback used when the monthly day selection changes.  We need
 * to change the valid range of the day index spin button; e.g. days
 * are 1-31 while a Sunday is the 1st through 5th.
 */
static void
month_day_combo_changed_cb (GtkComboBox *combo,
                            RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;
	enum month_num_options month_num;
	enum month_day_options month_day;

	priv = rpage->priv;

	month_num = e_dialog_combo_box_get (
		priv->month_num_combo,
		month_num_options_map);
	month_day = e_dialog_combo_box_get (
		priv->month_day_combo,
		month_day_options_map);
	if (month_day == MONTH_DAY_NTH && month_num != MONTH_NUM_LAST && month_num != MONTH_NUM_DAY)
		e_dialog_combo_box_set (
			priv->month_num_combo,
			MONTH_NUM_DAY,
			month_num_options_map);
	else if (month_day != MONTH_DAY_NTH && month_num == MONTH_NUM_DAY)
		e_dialog_combo_box_set (
			priv->month_num_combo,
			MONTH_NUM_FIRST,
			month_num_options_map);

	comp_editor_page_changed (COMP_EDITOR_PAGE (rpage));
}

/* Creates the special contents for monthly recurrences */
static void
make_monthly_special (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkAdjustment *adj;

	priv = rpage->priv;

	g_return_if_fail (gtk_bin_get_child (GTK_BIN (priv->special)) == NULL);
	g_return_if_fail (priv->month_day_combo == NULL);

	/* Create the widgets */

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_container_add (GTK_CONTAINER (priv->special), hbox);

	/* TRANSLATORS: Entire string is for example: 'This appointment recurs/Every [x] month(s) on the [second] [Tuesday] [forever]'
	 * (dropdown menu options are in [square brackets])."
	 */
	label = gtk_label_new (_("on the"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 6);

	adj = GTK_ADJUSTMENT (gtk_adjustment_new (1, 1, 31, 1, 10, 10));

	priv->month_num_combo = make_recur_month_num_combo (priv->month_index);
	gtk_box_pack_start (
		GTK_BOX (hbox), priv->month_num_combo,
		FALSE, FALSE, 6);

	priv->month_day_combo = make_recur_month_combobox ();
	gtk_box_pack_start (
		GTK_BOX (hbox), priv->month_day_combo,
		FALSE, FALSE, 6);

	gtk_widget_show_all (hbox);

	/* Set the options */
	e_dialog_combo_box_set (
		priv->month_num_combo,
		priv->month_num,
		month_num_options_map);
	e_dialog_combo_box_set (
		priv->month_day_combo,
		priv->month_day,
		month_day_options_map);

	g_signal_connect_swapped (
		adj, "value-changed",
		G_CALLBACK (comp_editor_page_changed), rpage);

	g_signal_connect (
		priv->month_num_combo, "changed",
		G_CALLBACK (month_num_combo_changed_cb), rpage);
	g_signal_connect (
		priv->month_day_combo, "changed",
		G_CALLBACK (month_day_combo_changed_cb), rpage);
}

/* Changes the recurrence-special widget to match the interval units.
 *
 * For daily recurrences: nothing.
 * For weekly recurrences: weekday selector.
 * For monthly recurrences: "on the" <nth> [day, Weekday]
 * For yearly recurrences: nothing.
 */
static void
make_recurrence_special (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;
	icalrecurrencetype_frequency frequency;
	GtkWidget *child;

	priv = rpage->priv;

	if (priv->month_num_combo != NULL) {
		gtk_widget_destroy (priv->month_num_combo);
		priv->month_num_combo = NULL;
	}

	child = gtk_bin_get_child (GTK_BIN (priv->special));
	if (child != NULL) {
		gtk_widget_destroy (child);

		priv->weekday_chooser = NULL;
		priv->month_day_combo = NULL;
	}

	frequency = e_dialog_combo_box_get (priv->interval_unit_combo, freq_map);

	switch (frequency) {
	case ICAL_DAILY_RECURRENCE:
		gtk_widget_hide (priv->special);
		break;

	case ICAL_WEEKLY_RECURRENCE:
		make_weekly_special (rpage);
		gtk_widget_show (priv->special);
		break;

	case ICAL_MONTHLY_RECURRENCE:
		make_monthly_special (rpage);
		gtk_widget_show (priv->special);
		break;

	case ICAL_YEARLY_RECURRENCE:
		gtk_widget_hide (priv->special);
		break;

	default:
		g_return_if_reached ();
	}
}

/* Counts the elements in the by_xxx fields of an icalrecurrencetype */
static gint
count_by_xxx (gshort *field,
              gint max_elements)
{
	gint i;

	for (i = 0; i < max_elements; i++)
		if (field[i] == ICAL_RECURRENCE_ARRAY_MAX)
			break;

	return i;
}

/* Creates the special contents for "ending until" (end date) recurrences */
static void
make_ending_until_special (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv = rpage->priv;
	CompEditor *editor;
	CompEditorFlags flags;
	EDateEdit *de;
	ECalComponentDateTime dt_start;

	g_return_if_fail (gtk_bin_get_child (GTK_BIN (priv->ending_special)) == NULL);
	g_return_if_fail (priv->ending_date_edit == NULL);

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (rpage));
	flags = comp_editor_get_flags (editor);

	/* Create the widget */

	priv->ending_date_edit = comp_editor_new_date_edit (TRUE, FALSE, FALSE);
	de = E_DATE_EDIT (priv->ending_date_edit);

	gtk_container_add (
		GTK_CONTAINER (priv->ending_special),
		GTK_WIDGET (de));
	gtk_widget_show_all (GTK_WIDGET (de));

	/* Set the value */

	if (flags & COMP_EDITOR_NEW_ITEM) {
		e_cal_component_get_dtstart (priv->comp, &dt_start);
		/* Setting the default until time to 2 weeks */
		icaltime_adjust (dt_start.value, 14, 0, 0, 0);
		e_date_edit_set_date (de, dt_start.value->year, dt_start.value->month, dt_start.value->day);
		e_cal_component_free_datetime (&dt_start);
	} else {
		e_date_edit_set_date (de, priv->ending_date_tt.year, priv->ending_date_tt.month, priv->ending_date_tt.day);
	}

	g_signal_connect_swapped (
		e_date_edit_get_entry (de), "focus-out-event",
		G_CALLBACK (comp_editor_page_changed), rpage);

	/* Make sure the EDateEdit widget uses our timezones to get the
	 * current time. */
	e_date_edit_set_get_time_callback (
		de,
		(EDateEditGetTimeCallback) comp_editor_get_current_time,
		g_object_ref (editor),
		(GDestroyNotify) g_object_unref);
}

/* Creates the special contents for the occurrence count case */
static void
make_ending_count_special (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkAdjustment *adj;

	priv = rpage->priv;

	g_return_if_fail (gtk_bin_get_child (GTK_BIN (priv->ending_special)) == NULL);
	g_return_if_fail (priv->ending_count_spin == NULL);

	/* Create the widgets */

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_container_add (GTK_CONTAINER (priv->ending_special), hbox);

	adj = GTK_ADJUSTMENT (gtk_adjustment_new (1, 1, 10000, 1, 10, 0));
	priv->ending_count_spin = gtk_spin_button_new (adj, 1, 0);
	gtk_spin_button_set_numeric ((GtkSpinButton *) priv->ending_count_spin, TRUE);
	gtk_box_pack_start (
		GTK_BOX (hbox), priv->ending_count_spin,
		FALSE, FALSE, 6);

	label = gtk_label_new (_("occurrences"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 6);

	gtk_widget_show_all (hbox);

	/* Set the values */

	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (priv->ending_count_spin),
		priv->ending_count);

	g_signal_connect_swapped (
		adj, "value-changed",
		G_CALLBACK (comp_editor_page_changed), rpage);
}

/* Changes the recurrence-ending-special widget to match the ending date option
 *
 * For: <n> [days, weeks, months, years, occurrences]
 * Until: <date selector>
 * Forever: nothing.
 */
static void
make_ending_special (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;
	enum ending_type ending_type;
	GtkWidget *child;

	priv = rpage->priv;

	child = gtk_bin_get_child (GTK_BIN (priv->ending_special));
	if (child != NULL) {
		gtk_widget_destroy (child);

		priv->ending_date_edit = NULL;
		priv->ending_count_spin = NULL;
	}

	ending_type = e_dialog_combo_box_get (priv->ending_combo, ending_types_map);

	switch (ending_type) {
	case ENDING_FOR:
		make_ending_count_special (rpage);
		gtk_widget_show (priv->ending_special);
		break;

	case ENDING_UNTIL:
		make_ending_until_special (rpage);
		gtk_widget_show (priv->ending_special);
		break;

	case ENDING_FOREVER:
		gtk_widget_hide (priv->ending_special);
		break;

	default:
		g_return_if_reached ();
	}
}

/* Fills the recurrence ending date widgets with the values from the calendar
 * component.
 */
static void
fill_ending_date (RecurrencePage *rpage,
                  struct icalrecurrencetype *r)
{
	RecurrencePagePrivate *priv = rpage->priv;
	CompEditor *editor;
	ECalClient *client;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (rpage));
	client = comp_editor_get_client (editor);

	g_signal_handlers_block_matched (priv->ending_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);

	if (r->count == 0) {
		if (r->until.year == 0) {
			/* Forever */

			e_dialog_combo_box_set (
				priv->ending_combo,
				ENDING_FOREVER,
				ending_types_map);
		} else {
			/* Ending date */

			if (!r->until.is_date) {
				ECalComponentDateTime dt;
				icaltimezone *from_zone, *to_zone;

				e_cal_component_get_dtstart (priv->comp, &dt);

				if (dt.value->is_date)
					to_zone = e_meeting_store_get_timezone (priv->meeting_store);
				else if (dt.tzid == NULL)
					to_zone = icaltimezone_get_utc_timezone ();
				else {
					GError *error = NULL;
					/* FIXME Error checking? */
					e_cal_client_get_timezone_sync (client, dt.tzid, &to_zone, NULL, &error);

					if (error != NULL) {
						g_warning (
							"%s: Failed to get timezone: %s",
							G_STRFUNC, error->message);
						g_error_free (error);
					}
				}
				from_zone = icaltimezone_get_utc_timezone ();

				icaltimezone_convert_time (&r->until, from_zone, to_zone);

				r->until.hour = 0;
				r->until.minute = 0;
				r->until.second = 0;
				r->until.is_date = TRUE;
				r->until.is_utc = FALSE;

				e_cal_component_free_datetime (&dt);
			}

			priv->ending_date_tt = r->until;
			e_dialog_combo_box_set (
				priv->ending_combo,
				ENDING_UNTIL,
				ending_types_map);
		}
	} else {
		/* Count of occurrences */

		priv->ending_count = r->count;
		e_dialog_combo_box_set (
			priv->ending_combo,
			ENDING_FOR,
			ending_types_map);
	}

	g_signal_handlers_unblock_matched (priv->ending_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);

	make_ending_special (rpage);
}

/* fill_widgets handler for the recurrence page.  This function is particularly
 * tricky because it has to discriminate between recurrences we support for
 * editing and the ones we don't.  We only support at most one recurrence rule;
 * no rdates or exrules (exdates are handled just fine elsewhere).
 */
static gboolean
recurrence_page_fill_widgets (CompEditorPage *page,
                              ECalComponent *comp)
{
	RecurrencePage *rpage;
	RecurrencePagePrivate *priv;
	ECalComponentText text;
	CompEditor *editor;
	CompEditorFlags flags;
	CompEditorPageDates dates;
	GSList *rrule_list;
	gint len;
	struct icalrecurrencetype *r;
	gint n_by_second, n_by_minute, n_by_hour;
	gint n_by_day, n_by_month_day, n_by_year_day;
	gint n_by_week_no, n_by_month, n_by_set_pos;
	GtkAdjustment *adj;

	rpage = RECURRENCE_PAGE (page);
	priv = rpage->priv;

	editor = comp_editor_page_get_editor (page);
	flags = comp_editor_get_flags (editor);

	/* Keep a copy of the component so that we can expand the recurrence
	 * set for the preview.
	 */

	if (priv->comp)
		g_object_unref (priv->comp);

	priv->comp = e_cal_component_clone (comp);

	if (!e_cal_component_has_organizer (comp)) {
		flags |= COMP_EDITOR_USER_ORG;
		comp_editor_set_flags (editor, flags);
	}

	/* Clean the page */
	clear_widgets (rpage);
	priv->custom = FALSE;

	/* Summary */
	e_cal_component_get_summary (comp, &text);

	/* Dates */
	comp_editor_dates (&dates, comp);
	recurrence_page_set_dates (page, &dates);
	comp_editor_free_dates (&dates);

	/* Exceptions */
	fill_exception_widgets (rpage, comp);

	/* Set up defaults for the special widgets */
	set_special_defaults (rpage);

	/* No recurrences? */

	if (!e_cal_component_has_rdates (comp)
	    && !e_cal_component_has_rrules (comp)
	    && !e_cal_component_has_exrules (comp)) {
		g_signal_handlers_block_matched (priv->recurs, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->recurs), FALSE);
		g_signal_handlers_unblock_matched (priv->recurs, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);

		sensitize_buttons (rpage);
		preview_recur (rpage);

		return TRUE;
	}

	/* See if it is a custom set we don't support */

	e_cal_component_get_rrule_list (comp, &rrule_list);
	len = g_slist_length (rrule_list);
	if (len > 1
	    || e_cal_component_has_rdates (comp)
	    || e_cal_component_has_exrules (comp))
		goto custom;

	/* Down to one rule, so test that one */

	g_return_val_if_fail (len == 1, TRUE);
	r = rrule_list->data;

	/* Any funky frequency? */

	if (r->freq == ICAL_SECONDLY_RECURRENCE
	    || r->freq == ICAL_MINUTELY_RECURRENCE
	    || r->freq == ICAL_HOURLY_RECURRENCE)
		goto custom;

	/* Any funky shit? */

#define N_HAS_BY(field) (count_by_xxx (field, G_N_ELEMENTS (field)))

	n_by_second = N_HAS_BY (r->by_second);
	n_by_minute = N_HAS_BY (r->by_minute);
	n_by_hour = N_HAS_BY (r->by_hour);
	n_by_day = N_HAS_BY (r->by_day);
	n_by_month_day = N_HAS_BY (r->by_month_day);
	n_by_year_day = N_HAS_BY (r->by_year_day);
	n_by_week_no = N_HAS_BY (r->by_week_no);
	n_by_month = N_HAS_BY (r->by_month);
	n_by_set_pos = N_HAS_BY (r->by_set_pos);

	if (n_by_second != 0
	    || n_by_minute != 0
	    || n_by_hour != 0)
		goto custom;

	/* Filter the funky shit based on the frequency; if there is nothing
	 * weird we can actually set the widgets.
	 */

	switch (r->freq) {
	case ICAL_DAILY_RECURRENCE:
		if (n_by_day != 0
		    || n_by_month_day != 0
		    || n_by_year_day != 0
		    || n_by_week_no != 0
		    || n_by_month != 0
		    || n_by_set_pos != 0)
			goto custom;

		g_signal_handlers_block_matched (priv->interval_unit_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
		e_dialog_combo_box_set (
			priv->interval_unit_combo,
			ICAL_DAILY_RECURRENCE,
			freq_map);
		g_signal_handlers_unblock_matched (priv->interval_unit_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
		break;

	case ICAL_WEEKLY_RECURRENCE: {
		gint i;
		guint8 day_mask;

		if (n_by_month_day != 0
		    || n_by_year_day != 0
		    || n_by_week_no != 0
		    || n_by_month != 0
		    || n_by_set_pos != 0)
			goto custom;

		day_mask = 0;

		for (i = 0; i < 8 && r->by_day[i] != ICAL_RECURRENCE_ARRAY_MAX; i++) {
			enum icalrecurrencetype_weekday weekday;
			gint pos;

			weekday = icalrecurrencetype_day_day_of_week (r->by_day[i]);
			pos = icalrecurrencetype_day_position (r->by_day[i]);

			if (pos != 0)
				goto custom;

			switch (weekday) {
			case ICAL_SUNDAY_WEEKDAY:
				day_mask |= 1 << 0;
				break;

			case ICAL_MONDAY_WEEKDAY:
				day_mask |= 1 << 1;
				break;

			case ICAL_TUESDAY_WEEKDAY:
				day_mask |= 1 << 2;
				break;

			case ICAL_WEDNESDAY_WEEKDAY:
				day_mask |= 1 << 3;
				break;

			case ICAL_THURSDAY_WEEKDAY:
				day_mask |= 1 << 4;
				break;

			case ICAL_FRIDAY_WEEKDAY:
				day_mask |= 1 << 5;
				break;

			case ICAL_SATURDAY_WEEKDAY:
				day_mask |= 1 << 6;
				break;

			default:
				break;
			}
		}

		priv->weekday_day_mask = day_mask;

		g_signal_handlers_block_matched (priv->interval_unit_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
		e_dialog_combo_box_set (
			priv->interval_unit_combo,
			ICAL_WEEKLY_RECURRENCE,
			freq_map);
		g_signal_handlers_unblock_matched (priv->interval_unit_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
		break;
	}

	case ICAL_MONTHLY_RECURRENCE:
		if (n_by_year_day != 0
		    || n_by_week_no != 0
		    || n_by_month != 0
		    || n_by_set_pos > 1)
			goto custom;

		if (n_by_month_day == 1) {
			gint nth;

			if (n_by_set_pos != 0)
				goto custom;

			nth = r->by_month_day[0];
			if (nth < 1 && nth != -1)
				goto custom;

			if (nth == -1) {
				ECalComponentDateTime dt;

				e_cal_component_get_dtstart (comp, &dt);
				priv->month_index = dt.value->day;
				priv->month_num = MONTH_NUM_LAST;
				e_cal_component_free_datetime (&dt);
			} else {
				priv->month_index = nth;
				priv->month_num = MONTH_NUM_DAY;
			}
			priv->month_day = MONTH_DAY_NTH;

		} else if (n_by_day == 1) {
			enum icalrecurrencetype_weekday weekday;
			gint pos;
			enum month_day_options month_day;

			/* Outlook 2000 uses BYDAY=TU;BYSETPOS=2, and will not
			 * accept BYDAY=2TU. So we now use the same as Outlook
			 * by default. */

			weekday = icalrecurrencetype_day_day_of_week (r->by_day[0]);
			pos = icalrecurrencetype_day_position (r->by_day[0]);

			if (pos == 0) {
				if (n_by_set_pos != 1)
					goto custom;
				pos = r->by_set_pos[0];
			} else if (pos < 0) {
				goto custom;
			}

			switch (weekday) {
			case ICAL_MONDAY_WEEKDAY:
				month_day = MONTH_DAY_MON;
				break;

			case ICAL_TUESDAY_WEEKDAY:
				month_day = MONTH_DAY_TUE;
				break;

			case ICAL_WEDNESDAY_WEEKDAY:
				month_day = MONTH_DAY_WED;
				break;

			case ICAL_THURSDAY_WEEKDAY:
				month_day = MONTH_DAY_THU;
				break;

			case ICAL_FRIDAY_WEEKDAY:
				month_day = MONTH_DAY_FRI;
				break;

			case ICAL_SATURDAY_WEEKDAY:
				month_day = MONTH_DAY_SAT;
				break;

			case ICAL_SUNDAY_WEEKDAY:
				month_day = MONTH_DAY_SUN;
				break;

			default:
				goto custom;
			}

			if (pos == -1)
				priv->month_num = MONTH_NUM_LAST;
			else
				priv->month_num = pos - 1;
			priv->month_day = month_day;
		} else
			goto custom;

		g_signal_handlers_block_matched (priv->interval_unit_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
		e_dialog_combo_box_set (
			priv->interval_unit_combo,
			ICAL_MONTHLY_RECURRENCE,
			freq_map);
		g_signal_handlers_unblock_matched (priv->interval_unit_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
		break;

	case ICAL_YEARLY_RECURRENCE:
		if (n_by_day != 0
		    || n_by_month_day != 0
		    || n_by_year_day != 0
		    || n_by_week_no != 0
		    || n_by_month != 0
		    || n_by_set_pos != 0)
			goto custom;

		g_signal_handlers_block_matched (priv->interval_unit_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
		e_dialog_combo_box_set (
			priv->interval_unit_combo,
			ICAL_YEARLY_RECURRENCE,
			freq_map);
		g_signal_handlers_unblock_matched (priv->interval_unit_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
		break;

	default:
		goto custom;
	}

	/* If we got here it means it is a simple recurrence */

	g_signal_handlers_block_matched (priv->recurs, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->recurs), TRUE);
	g_signal_handlers_unblock_matched (priv->recurs, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);

	sensitize_buttons (rpage);
	make_recurrence_special (rpage);

	adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->interval_value));
	g_signal_handlers_block_matched (adj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (priv->interval_value), r->interval);
	g_signal_handlers_unblock_matched (adj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);

	fill_ending_date (rpage, r);

	goto out;

 custom:

	g_signal_handlers_block_matched (priv->recurs, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
	priv->custom = TRUE;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->recurs), TRUE);
	g_signal_handlers_unblock_matched (priv->recurs, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
	/* FIXME Desensitize recurrence page */

	sensitize_buttons (rpage);

 out:
	e_cal_component_free_recur_list (rrule_list);
	preview_recur (rpage);

	return TRUE;
}

/* fill_component handler for the recurrence page */
static gboolean
recurrence_page_fill_component (CompEditorPage *page,
                                ECalComponent *comp)
{
	RecurrencePage *rpage;

	rpage = RECURRENCE_PAGE (page);
	return fill_component (rpage, comp);
}

/* set_dates handler for the recurrence page */
static void
recurrence_page_set_dates (CompEditorPage *page,
                           CompEditorPageDates *dates)
{
	RecurrencePage *rpage;
	RecurrencePagePrivate *priv;
	ECalComponentDateTime dt;
	CompEditor *editor;
	CompEditorFlags flags;
	struct icaltimetype icaltime;
	guint8 mask;

	rpage = RECURRENCE_PAGE (page);
	priv = rpage->priv;

	editor = comp_editor_page_get_editor (page);
	flags = comp_editor_get_flags (editor);

	/* Copy the dates to our component */

	if (!priv->comp)
		return;

	dt.value = &icaltime;

	if (dates->start) {
		icaltime = *dates->start->value;
		dt.tzid = dates->start->tzid;
		e_cal_component_set_dtstart (priv->comp, &dt);
	}

	if (dates->end) {
		icaltime = *dates->end->value;
		dt.tzid = dates->end->tzid;
		e_cal_component_set_dtend (priv->comp, &dt);
	}

	/* Update the weekday picker if necessary */
	mask = get_start_weekday_mask (priv->comp);
	if (mask != priv->weekday_blocked_day_mask) {
		priv->weekday_day_mask = priv->weekday_day_mask | mask;
		priv->weekday_blocked_day_mask = mask;

		if (priv->weekday_chooser != NULL) {
			EWeekdayChooser *chooser;
			guint8 mask;

			chooser = E_WEEKDAY_CHOOSER (priv->weekday_chooser);

			mask = priv->weekday_day_mask;
			e_weekday_chooser_set_selected (
				chooser, G_DATE_SUNDAY,
				(mask & (1 << 0)) != 0);
			e_weekday_chooser_set_selected (
				chooser, G_DATE_MONDAY,
				(mask & (1 << 1)) != 0);
			e_weekday_chooser_set_selected (
				chooser, G_DATE_TUESDAY,
				(mask & (1 << 2)) != 0);
			e_weekday_chooser_set_selected (
				chooser, G_DATE_WEDNESDAY,
				(mask & (1 << 3)) != 0);
			e_weekday_chooser_set_selected (
				chooser, G_DATE_THURSDAY,
				(mask & (1 << 4)) != 0);
			e_weekday_chooser_set_selected (
				chooser, G_DATE_FRIDAY,
				(mask & (1 << 5)) != 0);
			e_weekday_chooser_set_selected (
				chooser, G_DATE_SATURDAY,
				(mask & (1 << 6)) != 0);

			mask = priv->weekday_blocked_day_mask;
			e_weekday_chooser_set_blocked (
				chooser, G_DATE_SUNDAY,
				(mask & (1 << 0)) != 0);
			e_weekday_chooser_set_blocked (
				chooser, G_DATE_MONDAY,
				(mask & (1 << 1)) != 0);
			e_weekday_chooser_set_blocked (
				chooser, G_DATE_TUESDAY,
				(mask & (1 << 2)) != 0);
			e_weekday_chooser_set_blocked (
				chooser, G_DATE_WEDNESDAY,
				(mask & (1 << 3)) != 0);
			e_weekday_chooser_set_blocked (
				chooser, G_DATE_THURSDAY,
				(mask & (1 << 4)) != 0);
			e_weekday_chooser_set_blocked (
				chooser, G_DATE_FRIDAY,
				(mask & (1 << 5)) != 0);
			e_weekday_chooser_set_blocked (
				chooser, G_DATE_SATURDAY,
				(mask & (1 << 6)) != 0);
		}
	}

	if (flags & COMP_EDITOR_NEW_ITEM) {
		ECalendar *ecal;
		GDate *start, *end;

		ecal = E_CALENDAR (priv->preview_calendar);
		start = g_date_new ();
		end = g_date_new ();

		g_date_set_dmy (start, dates->start->value->day, dates->start->value->month, dates->start->value->year);
		g_date_set_dmy (end, dates->end->value->day, dates->end->value->month, dates->end->value->year);
		e_calendar_item_set_selection (ecal->calitem, start, end);

		g_date_free (start);
		g_date_free (end);
	}

	/* Make sure the preview gets updated. */
	preview_recur (rpage);
}

/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (RecurrencePage *rpage)
{
	CompEditorPage *page = COMP_EDITOR_PAGE (rpage);
	RecurrencePagePrivate *priv;
	GSList *accel_groups;
	GtkWidget *toplevel;
	GtkWidget *parent;

	priv = rpage->priv;

#define GW(name) e_builder_get_widget (priv->builder, name)

	priv->main = GW ("recurrence-page");
	if (!priv->main)
		return FALSE;

	/* Get the GtkAccelGroup from the toplevel window, so we can install
	 * it when the notebook page is mapped. */
	toplevel = gtk_widget_get_toplevel (priv->main);
	accel_groups = gtk_accel_groups_from_object (G_OBJECT (toplevel));
	if (accel_groups)
		page->accel_group = g_object_ref (accel_groups->data);

	g_object_ref (priv->main);
	parent = gtk_widget_get_parent (priv->main);
	gtk_container_remove (GTK_CONTAINER (parent), priv->main);

	priv->recurs = GW ("recurs");
	priv->params = GW ("params");

	priv->interval_value = GW ("interval-value");
	priv->interval_unit_combo = GW ("interval-unit-combobox");
	priv->special = GW ("special");
	priv->ending_combo = GW ("ending-combobox");
	priv->ending_special = GW ("ending-special");
	priv->custom_warning_bin = GW ("custom-warning-bin");

	priv->exception_list = GW ("exception-list");
	priv->exception_add = GW ("exception-add");
	priv->exception_modify = GW ("exception-modify");
	priv->exception_delete = GW ("exception-delete");

	priv->preview_bin = GW ("preview-bin");

#undef GW

	return (priv->recurs
		&& priv->params
		&& priv->interval_value
		&& priv->interval_unit_combo
		&& priv->special
		&& priv->ending_combo
		&& priv->ending_special
		&& priv->custom_warning_bin
		&& priv->exception_list
		&& priv->exception_add
		&& priv->exception_modify
		&& priv->exception_delete
		&& priv->preview_bin);
}

/* Callback used when the displayed date range in the recurrence preview
 * calendar changes.
 */
static void
preview_date_range_changed_cb (ECalendarItem *item,
                               RecurrencePage *rpage)
{
	preview_recur (rpage);
}

/* Callback used when one of the recurrence type radio buttons is toggled.  We
 * enable or disable the recurrence parameters.
 */
static void
type_toggled_cb (GtkToggleButton *toggle,
                 RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv = rpage->priv;
	CompEditor *editor;
	ECalClient *client;
	gboolean read_only;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (rpage));
	client = comp_editor_get_client (editor);

	comp_editor_page_changed (COMP_EDITOR_PAGE (rpage));
	sensitize_buttons (rpage);

	/* enable/disable the 'Add' button */
	read_only = e_client_is_readonly (E_CLIENT (client));

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->recurs)) || read_only)
		gtk_widget_set_sensitive (priv->exception_add, FALSE);
	else
		gtk_widget_set_sensitive (priv->exception_add, TRUE);
}

static GtkWidget *
create_exception_dialog (RecurrencePage *rpage,
                         const gchar *title,
                         GtkWidget **date_edit)
{
	RecurrencePagePrivate *priv;
	GtkWidget *dialog, *toplevel;
	GtkWidget *container;

	priv = rpage->priv;

	toplevel = gtk_widget_get_toplevel (priv->main);
	dialog = gtk_dialog_new_with_buttons (
		title, GTK_WINDOW (toplevel),
		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		_("_Cancel"), GTK_RESPONSE_REJECT,
		_("_OK"), GTK_RESPONSE_ACCEPT,
		NULL);

	*date_edit = comp_editor_new_date_edit (TRUE, FALSE, TRUE);
	gtk_widget_show (*date_edit);
	container = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_pack_start (GTK_BOX (container), *date_edit, FALSE, TRUE, 6);

	return dialog;
}

/* Callback for the "add exception" button */
static void
exception_add_cb (GtkWidget *widget,
                  RecurrencePage *rpage)
{
	GtkWidget *dialog, *date_edit;
	gboolean date_set;

	dialog = create_exception_dialog (rpage, _("Add exception"), &date_edit);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		ECalComponentDateTime dt;
		struct icaltimetype icaltime = icaltime_null_time ();

		dt.value = &icaltime;

		/* We use DATE values for exceptions, so we don't need a TZID. */
		dt.tzid = NULL;
		icaltime.is_date = 1;

		date_set = e_date_edit_get_date (
			E_DATE_EDIT (date_edit),
			&icaltime.year,
			&icaltime.month,
			&icaltime.day);
		g_return_if_fail (date_set);

		append_exception (rpage, &dt);

		comp_editor_page_changed (COMP_EDITOR_PAGE (rpage));
	}

	gtk_widget_destroy (dialog);
}

/* Callback for the "modify exception" button */
static void
exception_modify_cb (GtkWidget *widget,
                     RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;
	GtkWidget *dialog, *date_edit;
	const ECalComponentDateTime *current_dt;
	GtkTreeSelection *selection;
	GtkTreeIter iter;

	priv = rpage->priv;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->exception_list));
	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		g_warning (_("Could not get a selection to modify."));
		return;
	}

	current_dt = e_date_time_list_get_date_time (priv->exception_list_store, &iter);

	dialog = create_exception_dialog (rpage, _("Modify exception"), &date_edit);
	e_date_edit_set_date (
		E_DATE_EDIT (date_edit),
		current_dt->value->year, current_dt->value->month, current_dt->value->day);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		ECalComponentDateTime dt;
		struct icaltimetype icaltime = icaltime_null_time ();
		struct icaltimetype *tt;

		dt.value = &icaltime;
		tt = dt.value;
		e_date_edit_get_date (
			E_DATE_EDIT (date_edit),
			&tt->year, &tt->month, &tt->day);
		tt->hour = 0;
		tt->minute = 0;
		tt->second = 0;
		tt->is_date = 1;

		/* No TZID, since we are using a DATE value now. */
		dt.tzid = NULL;

		e_date_time_list_set_date_time (priv->exception_list_store, &iter, &dt);

		comp_editor_page_changed (COMP_EDITOR_PAGE (rpage));
	}

	gtk_widget_destroy (dialog);
}

/* Callback for the "delete exception" button */
static void
exception_delete_cb (GtkWidget *widget,
                     RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreePath *path;
	gboolean valid_iter;

	priv = rpage->priv;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->exception_list));
	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		g_warning (_("Could not get a selection to delete."));
		return;
	}

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->exception_list_store), &iter);
	e_date_time_list_remove (priv->exception_list_store, &iter);

	/* Select closest item after removal */
	valid_iter = gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->exception_list_store), &iter, path);
	if (!valid_iter) {
		gtk_tree_path_prev (path);
		valid_iter = gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->exception_list_store), &iter, path);
	}

	if (valid_iter)
		gtk_tree_selection_select_iter (selection, &iter);

	gtk_tree_path_free (path);

	comp_editor_page_changed (COMP_EDITOR_PAGE (rpage));
}

/* Callback used when a row is selected in the list of exception
 * dates.  We must update the date/time widgets to reflect the
 * exception's value.
 */
static void
exception_selection_changed_cb (GtkTreeSelection *selection,
                                RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;
	GtkTreeIter iter;

	priv = rpage->priv;

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		gtk_widget_set_sensitive (priv->exception_modify, FALSE);
		gtk_widget_set_sensitive (priv->exception_delete, FALSE);
		return;
	}

	gtk_widget_set_sensitive (priv->exception_modify, TRUE);
	gtk_widget_set_sensitive (priv->exception_delete, TRUE);
}

/* Hooks the widget signals */
static void
init_widgets (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;
	CompEditor *editor;
	ECalendar *ecal;
	GtkAdjustment *adj;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell_renderer;

	priv = rpage->priv;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (rpage));

	/* Recurrence preview */

	priv->preview_calendar = e_calendar_new ();
	ecal = E_CALENDAR (priv->preview_calendar);

	g_signal_connect (
		ecal->calitem, "date_range_changed",
		G_CALLBACK (preview_date_range_changed_cb), rpage);
	e_calendar_item_set_max_days_sel (ecal->calitem, 0);
	gtk_container_add (
		GTK_CONTAINER (priv->preview_bin),
		priv->preview_calendar);
	gtk_widget_show (priv->preview_calendar);

	e_calendar_item_set_get_time_callback (
		ecal->calitem,
		(ECalendarItemGetTimeCallback) comp_editor_get_current_time,
		g_object_ref (editor),
		(GDestroyNotify) g_object_unref);

	/* Recurrence types */

	g_signal_connect (
		priv->recurs, "toggled",
		G_CALLBACK (type_toggled_cb), rpage);

	/* Recurrence interval */

	adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->interval_value));
	g_signal_connect_swapped (
		adj, "value-changed",
		G_CALLBACK (comp_editor_page_changed), rpage);

	/* Recurrence units */

	g_signal_connect_swapped (
		priv->interval_unit_combo, "changed",
		G_CALLBACK (make_recurrence_special), rpage);
	g_signal_connect_swapped (
		priv->interval_unit_combo, "changed",
		G_CALLBACK (comp_editor_page_changed), rpage);

	/* Recurrence ending */

	g_signal_connect_swapped (
		priv->ending_combo, "changed",
		G_CALLBACK (make_ending_special), rpage);
	g_signal_connect_swapped (
		priv->ending_combo, "changed",
		G_CALLBACK (comp_editor_page_changed), rpage);

	/* Exception buttons */

	g_signal_connect (
		priv->exception_add, "clicked",
		G_CALLBACK (exception_add_cb), rpage);
	g_signal_connect (
		priv->exception_modify, "clicked",
		G_CALLBACK (exception_modify_cb), rpage);
	g_signal_connect (
		priv->exception_delete, "clicked",
		G_CALLBACK (exception_delete_cb), rpage);

	gtk_widget_set_sensitive (priv->exception_modify, FALSE);
	gtk_widget_set_sensitive (priv->exception_delete, FALSE);

	/* Exception list */

	/* Model */
	priv->exception_list_store = e_date_time_list_new ();
	gtk_tree_view_set_model (
		GTK_TREE_VIEW (priv->exception_list),
		GTK_TREE_MODEL (priv->exception_list_store));

	e_binding_bind_property (
		editor, "use-24-hour-format",
		priv->exception_list_store, "use-24-hour-format",
		G_BINDING_SYNC_CREATE);

	/* View */
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Date/Time"));
	cell_renderer = GTK_CELL_RENDERER (gtk_cell_renderer_text_new ());
	gtk_tree_view_column_pack_start (column, cell_renderer, TRUE);
	gtk_tree_view_column_add_attribute (column, cell_renderer, "text", E_DATE_TIME_LIST_COLUMN_DESCRIPTION);
	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->exception_list), column);

	g_signal_connect (
		gtk_tree_view_get_selection (
		GTK_TREE_VIEW (priv->exception_list)), "changed",
		G_CALLBACK (exception_selection_changed_cb), rpage);
}

/**
 * recurrence_page_construct:
 * @rpage: A recurrence page.
 *
 * Constructs a recurrence page by loading its Glade data.
 *
 * Return value: The same object as @rpage, or NULL if the widgets could not be
 * created.
 **/
RecurrencePage *
recurrence_page_construct (RecurrencePage *rpage,
                           EMeetingStore *meeting_store)
{
	RecurrencePagePrivate *priv;
	CompEditor *editor;

	priv = rpage->priv;
	priv->meeting_store = g_object_ref (meeting_store);

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (rpage));

	priv->builder = gtk_builder_new ();
	e_load_ui_builder_definition (priv->builder, "recurrence-page.ui");

	if (!get_widgets (rpage)) {
		g_message (
			"recurrence_page_construct(): "
			"Could not find all widgets in the XML file!");
		return NULL;
	}

	init_widgets (rpage);

	e_signal_connect_notify_swapped (
		editor, "notify::client",
		G_CALLBACK (sensitize_buttons), rpage);

	return rpage;
}

/**
 * recurrence_page_new:
 *
 * Creates a new recurrence page.
 *
 * Return value: A newly-created recurrence page, or NULL if the page could not
 * be created.
 **/
RecurrencePage *
recurrence_page_new (EMeetingStore *meeting_store,
                     CompEditor *editor)
{
	RecurrencePage *rpage;

	g_return_val_if_fail (E_IS_MEETING_STORE (meeting_store), NULL);
	g_return_val_if_fail (IS_COMP_EDITOR (editor), NULL);

	rpage = g_object_new (TYPE_RECURRENCE_PAGE, "editor", editor, NULL);
	if (!recurrence_page_construct (rpage, meeting_store)) {
		g_object_unref (rpage);
		g_return_val_if_reached (NULL);
	}

	return rpage;
}

GtkWidget *make_exdate_date_edit (void);

GtkWidget *
make_exdate_date_edit (void)
{
	return comp_editor_new_date_edit (TRUE, TRUE, FALSE);
}

