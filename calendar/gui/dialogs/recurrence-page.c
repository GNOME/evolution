/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Evolution calendar - Recurrence page of the calendar component dialogs
 *
 * Copyright (C) 2001-2003 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          Miguel de Icaza <miguel@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *          JP Rosevear <jpr@ximian.com>
 *          Hans Petter Jansson <hpj@ximiman.com>
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

#include <gtk/gtklabel.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkstock.h>
#include <libgnome/gnome-i18n.h>
#include <glade/glade.h>
#include <e-util/e-dialog-widgets.h>
#include <e-util/e-time-utils.h>
#include <widgets/misc/e-dateedit.h>
#include <libecal/e-cal-recur.h>
#include <libecal/e-cal-time-util.h>
#include "../calendar-config.h"
#include "../tag-calendar.h"
#include "../weekday-picker.h"
#include "comp-editor-util.h"
#include "../e-date-time-list.h"
#include "../e-mini-calendar-config.h"
#include "recurrence-page.h"



enum month_num_options {
	MONTH_NUM_FIRST,
	MONTH_NUM_SECOND,
	MONTH_NUM_THIRD,
	MONTH_NUM_FOURTH,
	MONTH_NUM_LAST,
	MONTH_NUM_DAY,
	MONTH_NUM_OTHER
};

static const int month_num_options_map[] = {
	MONTH_NUM_FIRST,
	MONTH_NUM_SECOND,
	MONTH_NUM_THIRD,
	MONTH_NUM_FOURTH,
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

static const int month_day_options_map[] = {
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

static const int type_map[] = {
	RECUR_NONE,
	RECUR_SIMPLE,
	RECUR_CUSTOM,
	-1
};

static const int freq_map[] = {
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

static const int ending_types_map[] = {
	ENDING_FOR,
	ENDING_UNTIL,
	ENDING_FOREVER,
	-1
};

/* Private part of the RecurrencePage structure */
struct _RecurrencePagePrivate {
	/* Component we use to expand the recurrence rules for the preview */
	ECalComponent *comp;

	/* Glade XML data */
	GladeXML *xml;

	/* Widgets from the Glade file */
	GtkWidget *main;

	GtkWidget *recurs;
	gboolean custom;
	
	GtkWidget *params;
	GtkWidget *interval_value;
	GtkWidget *interval_unit;
	GtkWidget *special;
	GtkWidget *ending_menu;
	GtkWidget *ending_special;
	GtkWidget *custom_warning_bin;

	/* For weekly recurrences, created by hand */
	GtkWidget *weekday_picker;
	guint8 weekday_day_mask;
	guint8 weekday_blocked_day_mask;

	/* For monthly recurrences, created by hand */
	int month_index;

	GtkWidget *month_day_menu;
	enum month_day_options month_day;

	GtkWidget *month_num_menu;
	enum month_num_options month_num;
	
	/* For ending date, created by hand */
	GtkWidget *ending_date_edit;
	struct icaltimetype ending_date_tt;

	/* For ending count of occurrences, created by hand */
	GtkWidget *ending_count_spin;
	int ending_count;

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
	EMiniCalendarConfig *preview_calendar_config;
	
	gboolean updating;
};



static void recurrence_page_finalize (GObject *object);

static GtkWidget *recurrence_page_get_widget (CompEditorPage *page);
static void recurrence_page_focus_main_widget (CompEditorPage *page);
static gboolean recurrence_page_fill_widgets (CompEditorPage *page, ECalComponent *comp);
static gboolean recurrence_page_fill_component (CompEditorPage *page, ECalComponent *comp);
static void recurrence_page_set_dates (CompEditorPage *page, CompEditorPageDates *dates);

static void field_changed (RecurrencePage *apage);

G_DEFINE_TYPE (RecurrencePage, recurrence_page, TYPE_COMP_EDITOR_PAGE);

/* Class initialization function for the recurrence page */
static void
recurrence_page_class_init (RecurrencePageClass *class)
{
	CompEditorPageClass *editor_page_class;
	GObjectClass *object_class;

	editor_page_class = (CompEditorPageClass *) class;
	object_class = (GObjectClass *) class;

	editor_page_class->get_widget = recurrence_page_get_widget;
	editor_page_class->focus_main_widget = recurrence_page_focus_main_widget;
	editor_page_class->fill_widgets = recurrence_page_fill_widgets;
	editor_page_class->fill_component = recurrence_page_fill_component;
	editor_page_class->set_dates = recurrence_page_set_dates;

	object_class->finalize = recurrence_page_finalize;
}

/* Object initialization function for the recurrence page */
static void
recurrence_page_init (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;

	priv = g_new0 (RecurrencePagePrivate, 1);
	rpage->priv = priv;

	priv->xml = NULL;

	priv->main = NULL;
	priv->recurs = NULL;
	priv->custom = FALSE;
	priv->params = NULL;
	priv->interval_value = NULL;
	priv->interval_unit = NULL;
	priv->special = NULL;
	priv->ending_menu = NULL;
	priv->ending_special = NULL;
	priv->custom_warning_bin = NULL;
	priv->weekday_picker = NULL;
	priv->month_day_menu = NULL;
	priv->month_num_menu = NULL;
	priv->ending_date_edit = NULL;
	priv->ending_count_spin = NULL;
	priv->exception_list = NULL;
	priv->exception_add = NULL;
	priv->exception_modify = NULL;
	priv->exception_delete = NULL;
	priv->preview_bin = NULL;
	priv->preview_calendar = NULL;

	priv->comp = NULL;
}

/* Destroy handler for the recurrence page */
static void
recurrence_page_finalize (GObject *object)
{
	RecurrencePage *rpage;
	RecurrencePagePrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_RECURRENCE_PAGE (object));

	rpage = RECURRENCE_PAGE (object);
	priv = rpage->priv;

	if (priv->main)
		gtk_widget_unref (priv->main);

	if (priv->xml) {
		g_object_unref (priv->xml);
		priv->xml = NULL;
	}

	if (priv->comp) {
		g_object_unref (priv->comp);
		priv->comp = NULL;
	}

	if (priv->exception_list_store) {
		g_object_unref (priv->exception_list_store);
		priv->exception_list_store = NULL;
	}

	if (priv->preview_calendar_config) {
		g_object_unref (priv->preview_calendar_config);
		priv->preview_calendar_config = NULL;
	}

	g_free (priv);
	rpage->priv = NULL;

	if (G_OBJECT_CLASS (recurrence_page_parent_class)->finalize)
		(* G_OBJECT_CLASS (recurrence_page_parent_class)->finalize) (object);
}



/* get_widget handler for the recurrence page */
static GtkWidget *
recurrence_page_get_widget (CompEditorPage *page)
{
	RecurrencePage *rpage;
	RecurrencePagePrivate *priv;

	rpage = RECURRENCE_PAGE (page);
	priv = rpage->priv;

	return priv->main;
}

/* focus_main_widget handler for the recurrence page */
static void
recurrence_page_focus_main_widget (CompEditorPage *page)
{
	RecurrencePage *rpage;
	RecurrencePagePrivate *priv;

	rpage = RECURRENCE_PAGE (page);
	priv = rpage->priv;

	gtk_widget_grab_focus (priv->recurs);
}

/* Fills the widgets with default values */
static void
clear_widgets (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;
	GtkAdjustment *adj;
	GtkWidget *menu;

	priv = rpage->priv;

	priv->custom = FALSE;
	
	priv->weekday_day_mask = 0;

	priv->month_index = 1;
	priv->month_num = MONTH_NUM_DAY;
	priv->month_day = MONTH_DAY_NTH;

	g_signal_handlers_block_matched (priv->recurs, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
	e_dialog_toggle_set (priv->recurs, FALSE);
	g_signal_handlers_unblock_matched (priv->recurs, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);

	adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->interval_value));
	g_signal_handlers_block_matched (adj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
	e_dialog_spin_set (priv->interval_value, 1);
	g_signal_handlers_unblock_matched (adj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->interval_unit));
	g_signal_handlers_block_matched (menu, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
	e_dialog_option_menu_set (priv->interval_unit,
				  ICAL_DAILY_RECURRENCE,
				  freq_map);
	g_signal_handlers_unblock_matched (menu, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);

	priv->ending_date_tt = icaltime_today ();
	priv->ending_count = 1;

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->ending_menu));
	g_signal_handlers_block_matched (menu, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
	e_dialog_option_menu_set (priv->ending_menu, 
				  ENDING_FOREVER,
				  ending_types_map);
	g_signal_handlers_unblock_matched (menu, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);

	/* Exceptions list */
	e_date_time_list_clear (priv->exception_list_store);
}

/* Appends an exception date to the list */
static void
append_exception (RecurrencePage *rpage, ECalComponentDateTime *datetime)
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
fill_exception_widgets (RecurrencePage *rpage, ECalComponent *comp)
{
	RecurrencePagePrivate *priv;
	GSList *list, *l;
	gboolean added = FALSE;

	priv = rpage->priv;
	e_cal_component_get_exdate_list (comp, &list);

	for (l = list; l; l = l->next) {
		ECalComponentDateTime *cdt;

		added = TRUE;

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
		short weekday;

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
	RecurrencePagePrivate *priv;
	gboolean recurs;
	GtkWidget *label;

	priv = rpage->priv;

	recurs = e_dialog_toggle_get (priv->recurs);
	
	/* We can't preview that well for instances right now */
	if (e_cal_component_is_instance (priv->comp))
		gtk_widget_set_sensitive (priv->preview_calendar, FALSE);
	else
		gtk_widget_set_sensitive (priv->preview_calendar, TRUE);

	if (GTK_BIN (priv->custom_warning_bin)->child)
		gtk_widget_destroy (GTK_BIN (priv->custom_warning_bin)->child);

	if (recurs && priv->custom) {
		gtk_widget_set_sensitive (priv->params, FALSE);
		gtk_widget_hide (priv->params);

		label = gtk_label_new (_("This appointment contains "
					 "recurrences that Evolution "
					 "cannot edit."));
		gtk_container_add (GTK_CONTAINER (priv->custom_warning_bin),
				   label);
		gtk_widget_show_all (priv->custom_warning_bin);
	} else if (recurs) {
		gtk_widget_set_sensitive (priv->params, TRUE);
		gtk_widget_show (priv->params);
		gtk_widget_hide (priv->custom_warning_bin);
	} else {
		gtk_widget_set_sensitive (priv->params, FALSE);
		gtk_widget_show (priv->params);
		gtk_widget_hide (priv->custom_warning_bin);
	}
}

static void
sensitize_buttons (RecurrencePage *rpage)
{
	gboolean read_only;
	gint selected_rows;
	RecurrencePagePrivate *priv;
	icalcomponent *icalcomp;
	const char *uid;

	priv = rpage->priv;

	selected_rows = gtk_tree_selection_count_selected_rows (
		gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->exception_list)));

	if (!e_cal_is_read_only (COMP_EDITOR_PAGE (rpage)->client, &read_only, NULL))
		read_only = TRUE;
	
	if (!read_only) {
		e_cal_component_get_uid (priv->comp, &uid);

		if (e_cal_get_static_capability (COMP_EDITOR_PAGE (rpage)->client, CAL_STATIC_CAPABILITY_NO_CONV_TO_RECUR) && e_cal_get_object(COMP_EDITOR_PAGE (rpage)->client, uid, NULL, &icalcomp, NULL)) {
			read_only = TRUE;
			icalcomponent_free (icalcomp);
		}
	}

	if (!read_only)
		sensitize_recur_widgets (rpage);
	else
		gtk_widget_set_sensitive (priv->params, FALSE);

	gtk_widget_set_sensitive (priv->recurs, !read_only);
	gtk_widget_set_sensitive (priv->exception_add, !read_only && e_cal_component_has_recurrences (priv->comp));
	gtk_widget_set_sensitive (priv->exception_modify, !read_only && selected_rows > 0);
	gtk_widget_set_sensitive (priv->exception_delete, !read_only && selected_rows > 0);
}

#if 0
/* Encondes a position/weekday pair into the proper format for
 * icalrecurrencetype.by_day. Not needed at present.
 */
static short
nth_weekday (int pos, icalrecurrencetype_weekday weekday)
{
	g_assert (pos > 0 && pos <= 5);

	return (pos << 3) | (int) weekday;
}
#endif

/* Gets the simple recurrence data from the recurrence widgets and stores it in
 * the calendar component.
 */
static void
simple_recur_to_comp (RecurrencePage *rpage, ECalComponent *comp)
{
	RecurrencePagePrivate *priv;
	struct icalrecurrencetype r;
	GSList l;
	enum ending_type ending_type;
	gboolean date_set;

	priv = rpage->priv;

	icalrecurrencetype_clear (&r);

	/* Frequency, interval, week start */

	r.freq = e_dialog_option_menu_get (priv->interval_unit, freq_map);
	r.interval = e_dialog_spin_get_int (priv->interval_value);
	r.week_start = ICAL_SUNDAY_WEEKDAY
		+ calendar_config_get_week_start_day ();

	/* Frequency-specific data */

	switch (r.freq) {
	case ICAL_DAILY_RECURRENCE:
		/* Nothing else is required */
		break;

	case ICAL_WEEKLY_RECURRENCE: {
		guint8 day_mask;
		int i;

		g_assert (GTK_BIN (priv->special)->child != NULL);
		g_assert (priv->weekday_picker != NULL);
		g_assert (IS_WEEKDAY_PICKER (priv->weekday_picker));

		day_mask = weekday_picker_get_days (WEEKDAY_PICKER (priv->weekday_picker));

		i = 0;

		if (day_mask & (1 << 0))
			r.by_day[i++] = ICAL_SUNDAY_WEEKDAY;

		if (day_mask & (1 << 1))
			r.by_day[i++] = ICAL_MONDAY_WEEKDAY;

		if (day_mask & (1 << 2))
			r.by_day[i++] = ICAL_TUESDAY_WEEKDAY;

		if (day_mask & (1 << 3))
			r.by_day[i++] = ICAL_WEDNESDAY_WEEKDAY;

		if (day_mask & (1 << 4))
			r.by_day[i++] = ICAL_THURSDAY_WEEKDAY;

		if (day_mask & (1 << 5))
			r.by_day[i++] = ICAL_FRIDAY_WEEKDAY;

		if (day_mask & (1 << 6))
			r.by_day[i++] = ICAL_SATURDAY_WEEKDAY;

		break;
	}

	case ICAL_MONTHLY_RECURRENCE: {
		enum month_num_options month_num;
		enum month_day_options month_day;
		
		g_assert (GTK_BIN (priv->special)->child != NULL);
		g_assert (priv->month_day_menu != NULL);
		g_assert (GTK_IS_OPTION_MENU (priv->month_day_menu));
		g_assert (priv->month_num_menu != NULL);
		g_assert (GTK_IS_OPTION_MENU (priv->month_num_menu));

		month_num = e_dialog_option_menu_get (priv->month_num_menu,
						      month_num_options_map );
		month_day = e_dialog_option_menu_get (priv->month_day_menu,
						      month_day_options_map);

		if (month_num == MONTH_NUM_LAST)
			month_num = -1;
		else
			month_num++;
		
		switch (month_day) {
		case MONTH_DAY_NTH:
			if (month_num == -1)
				r.by_month_day[0] = -1;
			else
				r.by_month_day[0] = priv->month_index;
			break;

		/* Outlook 2000 uses BYDAY=TU;BYSETPOS=2, and will not
		   accept BYDAY=2TU. So we now use the same as Outlook
		   by default. */
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
			g_assert_not_reached ();
		}

		break;
	}

	case ICAL_YEARLY_RECURRENCE:
		/* Nothing else is required */
		break;

	default:
		g_assert_not_reached ();
	}

	/* Ending date */

	ending_type = e_dialog_option_menu_get (priv->ending_menu,
						ending_types_map);

	switch (ending_type) {
	case ENDING_FOR:
		g_assert (priv->ending_count_spin != NULL);
		g_assert (GTK_IS_SPIN_BUTTON (priv->ending_count_spin));

		r.count = e_dialog_spin_get_int (priv->ending_count_spin);
		break;

	case ENDING_UNTIL:
		g_assert (priv->ending_date_edit != NULL);
		g_assert (E_IS_DATE_EDIT (priv->ending_date_edit));

		/* We only allow a DATE value to be set for the UNTIL property,
		   since we don't support sub-day recurrences. */
		date_set = e_date_edit_get_date (E_DATE_EDIT (priv->ending_date_edit),
						 &r.until.year,
						 &r.until.month,
						 &r.until.day);
		g_assert (date_set);

		r.until.is_date = 1;

		break;

	case ENDING_FOREVER:
		/* Nothing to be done */
		break;

	default:
		g_assert_not_reached ();
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
fill_component (RecurrencePage *rpage, ECalComponent *comp)
{
	RecurrencePagePrivate *priv;
	gboolean recurs;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid_iter;
	GSList *list;

	priv = rpage->priv;
	model = GTK_TREE_MODEL (priv->exception_list_store);

	recurs = e_dialog_toggle_get (priv->recurs);
	
	if (recurs && priv->custom) {
		/* We just keep whatever the component has currently */
	} else if (recurs) {
		e_cal_component_set_rdate_list (comp, NULL);
		e_cal_component_set_exrule_list (comp, NULL);
		simple_recur_to_comp (rpage, comp);
	} else {
		e_cal_component_set_rdate_list (comp, NULL);
		e_cal_component_set_rrule_list (comp, NULL);
		e_cal_component_set_exrule_list (comp, NULL);
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
		g_assert (dt != NULL);

		if (!icaltime_is_valid_time (*dt->value)) {
			comp_editor_page_display_validation_error (COMP_EDITOR_PAGE (rpage),
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

	return TRUE;
}

/* Re-tags the recurrence preview calendar based on the current information of
 * the widgets in the recurrence page.
 */
static void
preview_recur (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;
	ECalComponent *comp;
	ECalComponentDateTime cdt;
	GSList *l;
	icaltimezone *zone = NULL;
	
	priv = rpage->priv;

	/* If our component has not been set yet through ::fill_widgets(), we
	 * cannot preview the recurrence.
	 */
	if (!priv->comp || e_cal_component_is_instance (priv->comp))
		return;

	/* Create a scratch component with the start/end and
	 * recurrence/exception information from the one we are editing.
	 */

	comp = e_cal_component_new ();
	e_cal_component_set_new_vtype (comp, E_CAL_COMPONENT_EVENT);

	e_cal_component_get_dtstart (priv->comp, &cdt);
	if (cdt.tzid != NULL) {
		/* FIXME Will e_cal_get_timezone really not return builtin zones? */
		if (!e_cal_get_timezone (COMP_EDITOR_PAGE (rpage)->client, cdt.tzid, &zone, NULL))
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

	tag_calendar_by_comp (E_CALENDAR (priv->preview_calendar), comp,
			      COMP_EDITOR_PAGE (rpage)->client, zone, TRUE, FALSE);
	g_object_unref(comp);
}

/* Callback used when the recurrence weekday picker changes */
static void
weekday_picker_changed_cb (WeekdayPicker *wp, gpointer data)
{
	RecurrencePage *rpage;

	rpage = RECURRENCE_PAGE (data);

	field_changed (rpage);
	preview_recur (rpage);
}

/* Creates the special contents for weekly recurrences */
static void
make_weekly_special (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;
	GtkWidget *hbox;
	GtkWidget *label;
	WeekdayPicker *wp;

	priv = rpage->priv;

	g_assert (GTK_BIN (priv->special)->child == NULL);
	g_assert (priv->weekday_picker == NULL);

	/* Create the widgets */

	hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (priv->special), hbox);

	label = gtk_label_new (_("on"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 6);

	wp = WEEKDAY_PICKER (weekday_picker_new ());

	priv->weekday_picker = GTK_WIDGET (wp);
	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (wp), FALSE, FALSE, 6);

	gtk_widget_show_all (hbox);

	/* Set the weekdays */

	weekday_picker_set_week_start_day (wp, calendar_config_get_week_start_day ());
	weekday_picker_set_days (wp, priv->weekday_day_mask);
	weekday_picker_set_blocked_days (wp, priv->weekday_blocked_day_mask);

	g_signal_connect((wp), "changed",
			    G_CALLBACK (weekday_picker_changed_cb),
			    rpage);
}


static void
month_num_submenu_selection_done_cb (GtkMenuShell *menu_shell, gpointer data)
{
	GtkWidget *item;
	int month_index;
	
	item = gtk_menu_get_active (GTK_MENU (menu_shell));
	item = gtk_menu_get_active (GTK_MENU (gtk_menu_item_get_submenu (GTK_MENU_ITEM (item))));

	month_index = GPOINTER_TO_INT (gtk_object_get_user_data (GTK_OBJECT (item)));
	gtk_object_set_user_data (GTK_OBJECT (data), GINT_TO_POINTER (month_index));
}

/* Creates the option menu for the monthly recurrence number */
static GtkWidget *
make_recur_month_num_submenu (const char *title, int start, int end)
{
	GtkWidget *submenu, *item;
	int i;
	
	submenu = gtk_menu_new ();
	for (i = start; i < end; i++) {
		item = gtk_menu_item_new_with_label (_(e_cal_recur_nth[i]));
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
		gtk_object_set_user_data (GTK_OBJECT (item), GINT_TO_POINTER (i + 1));
		gtk_widget_show (item);
	}	

	item = gtk_menu_item_new_with_label (_(title));
	gtk_widget_show (item);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);

	return item;
}

static GtkWidget *
make_recur_month_num_menu (int month_index)
{
	static const char *options[] = {
		N_("first"),
		N_("second"),
		N_("third"),
		N_("fourth"),
		N_("last")
	};

	GtkWidget *menu, *submenu, *item, *submenu_item;
	GtkWidget *omenu;
	int i;

	menu = gtk_menu_new ();

	/* Relation */
	for (i = 0; i < sizeof (options) / sizeof (options[0]); i++) {
		item = gtk_menu_item_new_with_label (_(options[i]));
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show (item);
	}

	/* Current date */
	item = gtk_menu_item_new_with_label (_(e_cal_recur_nth[month_index - 1]));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show (item);

	/* Other Submenu */
	submenu = gtk_menu_new ();
	submenu_item = gtk_menu_item_new_with_label (_("Other Date"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), submenu_item);
	gtk_widget_show (submenu_item);

	item = make_recur_month_num_submenu ("1st to 10th", 0, 10);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
	item = make_recur_month_num_submenu ("11th to 20th", 10, 20);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
	item = make_recur_month_num_submenu ("21st to 31st", 20, 31);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);

	gtk_menu_item_set_submenu (GTK_MENU_ITEM (submenu_item), submenu);
	gtk_object_set_user_data (GTK_OBJECT (submenu_item), GINT_TO_POINTER (month_index));
	g_signal_connect((submenu), "selection_done",
			    G_CALLBACK (month_num_submenu_selection_done_cb),
			    submenu_item);

	omenu = gtk_option_menu_new ();
	gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);

	return omenu;
}

/* Creates the option menu for the monthly recurrence days */
static GtkWidget *
make_recur_month_menu (void)
{
	static const char *options[] = {
		N_("day"),
		N_("Monday"),
		N_("Tuesday"),
		N_("Wednesday"),
		N_("Thursday"),
		N_("Friday"),
		N_("Saturday"),
		N_("Sunday")
	};

	GtkWidget *menu;
	GtkWidget *omenu;
	int i;

	menu = gtk_menu_new ();

	for (i = 0; i < sizeof (options) / sizeof (options[0]); i++) {
		GtkWidget *item;

		item = gtk_menu_item_new_with_label (_(options[i]));
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show (item);
	}

	omenu = gtk_option_menu_new ();
	gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);

	return omenu;
}

static void
month_num_menu_selection_done_cb (GtkMenuShell *menu_shell, gpointer data)
{
	RecurrencePage *rpage;
	RecurrencePagePrivate *priv;
	enum month_num_options month_num;
	enum month_day_options month_day;

	rpage = RECURRENCE_PAGE (data);
	priv = rpage->priv;

	month_num = e_dialog_option_menu_get (priv->month_num_menu,
					      month_num_options_map);
	month_day = e_dialog_option_menu_get (priv->month_day_menu,
					      month_day_options_map);

	if (month_num == MONTH_NUM_OTHER) {
		GtkWidget *label, *item;

		item = gtk_menu_get_active (GTK_MENU (menu_shell));
		priv->month_index = GPOINTER_TO_INT (gtk_object_get_user_data (GTK_OBJECT (item)));

		month_num = MONTH_NUM_DAY;
		e_dialog_option_menu_set (priv->month_num_menu, month_num, month_num_options_map);

		label = GTK_BIN (priv->month_num_menu)->child;
		gtk_label_set_text (GTK_LABEL (label), _(e_cal_recur_nth[priv->month_index - 1]));

		e_dialog_option_menu_set (priv->month_num_menu, 0, month_num_options_map);
		e_dialog_option_menu_set (priv->month_num_menu, month_num, month_num_options_map);
	}
	
	if (month_num == MONTH_NUM_DAY && month_day != MONTH_DAY_NTH)
		e_dialog_option_menu_set (priv->month_day_menu,
					  MONTH_DAY_NTH,
					  month_day_options_map);
	else if (month_num != MONTH_NUM_DAY && month_num != MONTH_NUM_LAST && month_day == MONTH_DAY_NTH)
		e_dialog_option_menu_set (priv->month_day_menu,
					  MONTH_DAY_MON,
					  month_num_options_map);
	field_changed (rpage);
	preview_recur (rpage);
}

/* Callback used when the monthly day selection menu changes.  We need
 * to change the valid range of the day index spin button; e.g. days
 * are 1-31 while a Sunday is the 1st through 5th.
 */
static void
month_day_menu_selection_done_cb (GtkMenuShell *menu_shell, gpointer data)
{
	RecurrencePage *rpage;
	RecurrencePagePrivate *priv;
	enum month_num_options month_num;
	enum month_day_options month_day;

	rpage = RECURRENCE_PAGE (data);
	priv = rpage->priv;
	
	month_num = e_dialog_option_menu_get (priv->month_num_menu,
					      month_num_options_map);
	month_day = e_dialog_option_menu_get (priv->month_day_menu,
					      month_day_options_map);
	if (month_day == MONTH_DAY_NTH && month_num != MONTH_NUM_LAST && month_num != MONTH_NUM_DAY)
		e_dialog_option_menu_set (priv->month_num_menu,
					  MONTH_NUM_DAY,
					  month_num_options_map);
	else if (month_day != MONTH_DAY_NTH && month_num == MONTH_NUM_DAY)
		e_dialog_option_menu_set (priv->month_num_menu,
					  MONTH_NUM_FIRST,
					  month_num_options_map);
	field_changed (rpage);
	preview_recur (rpage);
}

/* Callback used when the month index value changes. */
static void
month_index_value_changed_cb (GtkAdjustment *adj, gpointer data)
{
	RecurrencePage *rpage;

	rpage = RECURRENCE_PAGE (data);

	field_changed (rpage);
	preview_recur (rpage);
}

/* Creates the special contents for monthly recurrences */
static void
make_monthly_special (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkAdjustment *adj;
	GtkWidget *menu;

	priv = rpage->priv;

	g_assert (GTK_BIN (priv->special)->child == NULL);
	g_assert (priv->month_day_menu == NULL);

	/* Create the widgets */

	hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (priv->special), hbox);

	label = gtk_label_new (_("on the"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 6);

	adj = GTK_ADJUSTMENT (gtk_adjustment_new (1, 1, 31, 1, 10, 10));

	priv->month_num_menu = make_recur_month_num_menu (priv->month_index);
	gtk_box_pack_start (GTK_BOX (hbox), priv->month_num_menu,
			    FALSE, FALSE, 6);

	priv->month_day_menu = make_recur_month_menu ();
	gtk_box_pack_start (GTK_BOX (hbox), priv->month_day_menu,
			    FALSE, FALSE, 6);

	gtk_widget_show_all (hbox);

	/* Set the options */
	e_dialog_option_menu_set (priv->month_num_menu,
				  priv->month_num,
				  month_num_options_map);
	e_dialog_option_menu_set (priv->month_day_menu,
				  priv->month_day,
				  month_day_options_map);

	g_signal_connect((adj), "value_changed",			    G_CALLBACK (month_index_value_changed_cb),
			    rpage);

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->month_num_menu));
	g_signal_connect((menu), "selection_done",
			    G_CALLBACK (month_num_menu_selection_done_cb),
			    rpage);
	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->month_day_menu));
	g_signal_connect((menu), "selection_done",
			    G_CALLBACK (month_day_menu_selection_done_cb),
			    rpage);
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

	priv = rpage->priv;

	if (priv->month_num_menu != NULL) {
		gtk_widget_destroy (priv->month_num_menu);
		priv->month_num_menu = NULL;
	}
	if (GTK_BIN (priv->special)->child != NULL) {
		gtk_widget_destroy (GTK_BIN (priv->special)->child);

		priv->weekday_picker = NULL;
		priv->month_day_menu = NULL;
	}

	frequency = e_dialog_option_menu_get (priv->interval_unit, freq_map);

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
		g_assert_not_reached ();
	}
}

/* Counts the elements in the by_xxx fields of an icalrecurrencetype */
static int
count_by_xxx (short *field, int max_elements)
{
	int i;

	for (i = 0; i < max_elements; i++)
		if (field[i] == ICAL_RECURRENCE_ARRAY_MAX)
			break;

	return i;
}

/* Callback used when the ending-until date editor changes */
static void
ending_until_changed_cb (EDateEdit *de, gpointer data)
{
	RecurrencePage *rpage;

	rpage = RECURRENCE_PAGE (data);
	field_changed (rpage);
	preview_recur (rpage);
}

/* Creates the special contents for "ending until" (end date) recurrences */
static void
make_ending_until_special (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;
	EDateEdit *de;

	priv = rpage->priv;

	g_assert (GTK_BIN (priv->ending_special)->child == NULL);
	g_assert (priv->ending_date_edit == NULL);

	/* Create the widget */

	priv->ending_date_edit = comp_editor_new_date_edit (TRUE, FALSE,
							    FALSE);
	de = E_DATE_EDIT (priv->ending_date_edit);

	gtk_container_add (GTK_CONTAINER (priv->ending_special),
			   GTK_WIDGET (de));
	gtk_widget_show_all (GTK_WIDGET (de));

	/* Set the value */

	e_date_edit_set_date (de, priv->ending_date_tt.year,
			      priv->ending_date_tt.month,
			      priv->ending_date_tt.day);

	g_signal_connect((de), "changed",
			    G_CALLBACK (ending_until_changed_cb), rpage);

	/* Make sure the EDateEdit widget uses our timezones to get the
	   current time. */
	e_date_edit_set_get_time_callback (de,
					   (EDateEditGetTimeCallback) comp_editor_get_current_time,
					   rpage, NULL);
}

/* Callback used when the ending-count value changes */
static void
ending_count_value_changed_cb (GtkAdjustment *adj, gpointer data)
{
	RecurrencePage *rpage;

	rpage = RECURRENCE_PAGE (data);
	field_changed (rpage);
	preview_recur (rpage);
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

	g_assert (GTK_BIN (priv->ending_special)->child == NULL);
	g_assert (priv->ending_count_spin == NULL);

	/* Create the widgets */

	hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (priv->ending_special), hbox);

	adj = GTK_ADJUSTMENT (gtk_adjustment_new (1, 1, 10000, 1, 10, 10));
	priv->ending_count_spin = gtk_spin_button_new (adj, 1, 0);
	gtk_spin_button_set_numeric ((GtkSpinButton *)priv->ending_count_spin, TRUE);
	gtk_box_pack_start (GTK_BOX (hbox), priv->ending_count_spin,
			    FALSE, FALSE, 6);

	label = gtk_label_new (_("occurrences"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 6);

	gtk_widget_show_all (hbox);

	/* Set the values */

	e_dialog_spin_set (priv->ending_count_spin, priv->ending_count);

	g_signal_connect((adj), "value_changed",
			    G_CALLBACK (ending_count_value_changed_cb),
			    rpage);
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

	priv = rpage->priv;

	if (GTK_BIN (priv->ending_special)->child != NULL) {
		gtk_widget_destroy (GTK_BIN (priv->ending_special)->child);

		priv->ending_date_edit = NULL;
		priv->ending_count_spin = NULL;
	}

	ending_type = e_dialog_option_menu_get (priv->ending_menu,
						ending_types_map);

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
		g_assert_not_reached ();
	}
}

/* Fills the recurrence ending date widgets with the values from the calendar
 * component.
 */
static void
fill_ending_date (RecurrencePage *rpage, struct icalrecurrencetype *r)
{
	RecurrencePagePrivate *priv;
	GtkWidget *menu;

	priv = rpage->priv;

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->ending_menu));
	g_signal_handlers_block_matched (menu, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);

	if (r->count == 0) {
		if (r->until.year == 0) {
			/* Forever */

			e_dialog_option_menu_set (priv->ending_menu,
						  ENDING_FOREVER,
						  ending_types_map);
		} else {
			/* Ending date */

			if (!r->until.is_date) {
				ECal *client = COMP_EDITOR_PAGE (rpage)->client;
				ECalComponentDateTime dt;
				icaltimezone *from_zone, *to_zone;
			
				e_cal_component_get_dtstart (priv->comp, &dt);

				if (dt.value->is_date)
					to_zone = calendar_config_get_icaltimezone ();
				else if (dt.tzid == NULL)
					to_zone = icaltimezone_get_utc_timezone ();
				else
					/* FIXME Error checking? */
					e_cal_get_timezone (client, dt.tzid, &to_zone, NULL);
				from_zone = icaltimezone_get_utc_timezone ();

				icaltimezone_convert_time (&r->until, from_zone, to_zone);

				r->until.hour = 0;
				r->until.minute = 0;
				r->until.second = 0;
				r->until.is_date = TRUE;
				r->until.is_utc = FALSE;
			}

			priv->ending_date_tt = r->until;
			e_dialog_option_menu_set (priv->ending_menu,
						  ENDING_UNTIL,
						  ending_types_map);
		}
	} else {
		/* Count of occurrences */

		priv->ending_count = r->count;
		e_dialog_option_menu_set (priv->ending_menu,
					  ENDING_FOR,
					  ending_types_map);
	}

	g_signal_handlers_unblock_matched (menu, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);

	make_ending_special (rpage);
}

/* fill_widgets handler for the recurrence page.  This function is particularly
 * tricky because it has to discriminate between recurrences we support for
 * editing and the ones we don't.  We only support at most one recurrence rule;
 * no rdates or exrules (exdates are handled just fine elsewhere).
 */
static gboolean
recurrence_page_fill_widgets (CompEditorPage *page, ECalComponent *comp)
{
	RecurrencePage *rpage;
	RecurrencePagePrivate *priv;
	ECalComponentText text;
	CompEditorPageDates dates;
	GSList *rrule_list;
	int len;
	struct icalrecurrencetype *r;
	int n_by_second, n_by_minute, n_by_hour;
	int n_by_day, n_by_month_day, n_by_year_day;
	int n_by_week_no, n_by_month, n_by_set_pos;
	GtkWidget *menu;
	GtkAdjustment *adj;

	rpage = RECURRENCE_PAGE (page);
	priv = rpage->priv;

	/* Keep a copy of the component so that we can expand the recurrence 
	 * set for the preview.
	 */

	if (priv->comp)
		g_object_unref (priv->comp);

	priv->comp = e_cal_component_clone (comp);

	/* Don't send off changes during this time */
	priv->updating = TRUE;

	/* Clean the page */
	clear_widgets (rpage);

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
		gtk_signal_handler_block_by_data (GTK_OBJECT (priv->recurs), rpage);
		e_dialog_toggle_set (priv->recurs, FALSE);
		gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->recurs), rpage);

		sensitize_buttons (rpage);
		preview_recur (rpage);

		priv->updating = FALSE;
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

	g_assert (len == 1);
	r = rrule_list->data;

	/* Any funky frequency? */

	if (r->freq == ICAL_SECONDLY_RECURRENCE
	    || r->freq == ICAL_MINUTELY_RECURRENCE
	    || r->freq == ICAL_HOURLY_RECURRENCE)
		goto custom;

	/* Any funky shit? */

#define N_HAS_BY(field) (count_by_xxx (field, sizeof (field) / sizeof (field[0])))

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

		menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->interval_unit));
		g_signal_handlers_block_matched (menu, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
		e_dialog_option_menu_set (priv->interval_unit,
					  ICAL_DAILY_RECURRENCE,
					  freq_map);
		g_signal_handlers_unblock_matched (menu, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
		break;

	case ICAL_WEEKLY_RECURRENCE: {
		int i;
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
			int pos;

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

		menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->interval_unit));
		g_signal_handlers_block_matched (menu, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
		e_dialog_option_menu_set (priv->interval_unit,
					  ICAL_WEEKLY_RECURRENCE,
					  freq_map);
		g_signal_handlers_unblock_matched (menu, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
		break;
	}

	case ICAL_MONTHLY_RECURRENCE:
		if (n_by_year_day != 0
		    || n_by_week_no != 0
		    || n_by_month != 0
		    || n_by_set_pos > 1)
			goto custom;

		if (n_by_month_day == 1) {
			int nth;

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
			} else {
				priv->month_index = nth;
				priv->month_num = MONTH_NUM_DAY;
			}
			priv->month_day = MONTH_DAY_NTH;
			
		} else if (n_by_day == 1) {
			enum icalrecurrencetype_weekday weekday;
			int pos;
			enum month_day_options month_day;

			/* Outlook 2000 uses BYDAY=TU;BYSETPOS=2, and will not
			   accept BYDAY=2TU. So we now use the same as Outlook
			   by default. */

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

		menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->interval_unit));
		g_signal_handlers_block_matched (menu, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
		e_dialog_option_menu_set (priv->interval_unit,
					  ICAL_MONTHLY_RECURRENCE,
					  freq_map);
		g_signal_handlers_unblock_matched (menu, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
		break;

	case ICAL_YEARLY_RECURRENCE:
		if (n_by_day != 0
		    || n_by_month_day != 0
		    || n_by_year_day != 0
		    || n_by_week_no != 0
		    || n_by_month != 0
		    || n_by_set_pos != 0)
			goto custom;

		menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->interval_unit));
		g_signal_handlers_block_matched (menu, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
		e_dialog_option_menu_set (priv->interval_unit,
					  ICAL_YEARLY_RECURRENCE,
					  freq_map);
		g_signal_handlers_unblock_matched (menu, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
		break;

	default:
		goto custom;
	}

	/* If we got here it means it is a simple recurrence */

	g_signal_handlers_block_matched (priv->recurs, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
	e_dialog_toggle_set (priv->recurs, TRUE);
	g_signal_handlers_unblock_matched (priv->recurs, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);

	sensitize_buttons (rpage);
	make_recurrence_special (rpage);

	adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->interval_value));
	g_signal_handlers_block_matched (adj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
	e_dialog_spin_set (priv->interval_value, r->interval);
	g_signal_handlers_unblock_matched (adj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);

	fill_ending_date (rpage, r);

	goto out;

 custom:

	g_signal_handlers_block_matched (priv->recurs, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
	priv->custom = TRUE;
	e_dialog_toggle_set (priv->recurs, TRUE);
	g_signal_handlers_unblock_matched (priv->recurs, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, rpage);
	/* FIXME Desensitize recurrence page */

	sensitize_buttons (rpage);

 out:
	priv->custom = FALSE;
	e_cal_component_free_recur_list (rrule_list);
	preview_recur (rpage);

	priv->updating = FALSE;

	return TRUE;
}

/* fill_component handler for the recurrence page */
static gboolean
recurrence_page_fill_component (CompEditorPage *page, ECalComponent *comp)
{
	RecurrencePage *rpage;

	rpage = RECURRENCE_PAGE (page);
	return fill_component (rpage, comp);
}

/* set_dates handler for the recurrence page */
static void
recurrence_page_set_dates (CompEditorPage *page, CompEditorPageDates *dates)
{
	RecurrencePage *rpage;
	RecurrencePagePrivate *priv;
	ECalComponentDateTime dt;
	struct icaltimetype icaltime;
	guint8 mask;

	rpage = RECURRENCE_PAGE (page);
	priv = rpage->priv;

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
		
		if (priv->weekday_picker != NULL) {
			weekday_picker_set_days (WEEKDAY_PICKER (priv->weekday_picker),
						 priv->weekday_day_mask);
			weekday_picker_set_blocked_days (WEEKDAY_PICKER (priv->weekday_picker),
							 priv->weekday_blocked_day_mask);
		}
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

	priv = rpage->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	priv->main = GW ("recurrence-page");
	if (!priv->main)
		return FALSE;

	/* Get the GtkAccelGroup from the toplevel window, so we can install
	   it when the notebook page is mapped. */
	toplevel = gtk_widget_get_toplevel (priv->main);
	accel_groups = gtk_accel_groups_from_object (G_OBJECT (toplevel));
	if (accel_groups) {
		page->accel_group = accel_groups->data;
		gtk_accel_group_ref (page->accel_group);
	}

	gtk_widget_ref (priv->main);
	gtk_container_remove (GTK_CONTAINER (priv->main->parent), priv->main);
	      
	priv->recurs = GW ("recurs");
	priv->params = GW ("params");
	      
	priv->interval_value = GW ("interval-value");
	priv->interval_unit = GW ("interval-unit");
	priv->special = GW ("special");
	priv->ending_menu = GW ("ending-menu");
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
		&& priv->interval_unit
		&& priv->special
		&& priv->ending_menu
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
preview_date_range_changed_cb (ECalendarItem *item, gpointer data)
{
	RecurrencePage *rpage;

	rpage = RECURRENCE_PAGE (data);
	preview_recur (rpage);
}

/* Callback used when one of the recurrence type radio buttons is toggled.  We
 * enable or disable the recurrence parameters.
 */
static void
type_toggled_cb (GtkToggleButton *toggle, gpointer data)
{
	RecurrencePage *rpage;
	RecurrencePagePrivate *priv;
	gboolean read_only;

	rpage = RECURRENCE_PAGE (data);

	priv = rpage->priv;

	field_changed (rpage);

	sensitize_buttons (rpage);
	preview_recur (rpage);

	/* enable/disable the 'Add' button */
	if (!e_cal_is_read_only (COMP_EDITOR_PAGE (rpage)->client, &read_only, NULL))
		read_only = TRUE;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->recurs)) || read_only)
		gtk_widget_set_sensitive (priv->exception_add, FALSE);
	else 
		gtk_widget_set_sensitive (priv->exception_add, TRUE);
}

/* Callback used when the recurrence interval value spin button changes. */
static void
interval_value_changed_cb (GtkAdjustment *adj, gpointer data)
{
	RecurrencePage *rpage;

	rpage = RECURRENCE_PAGE (data);

	field_changed (rpage);
	preview_recur (rpage);
}

/* Callback used when the recurrence interval option menu changes.  We need to
 * change the contents of the recurrence special widget.
 */
static void
interval_selection_done_cb (GtkMenuShell *menu_shell, gpointer data)
{
	RecurrencePage *rpage;

	rpage = RECURRENCE_PAGE (data);

	field_changed (rpage);
	make_recurrence_special (rpage);
	preview_recur (rpage);
}

/* Callback used when the recurrence ending option menu changes.  We need to
 * change the contents of the ending special widget.
 */
static void
ending_selection_done_cb (GtkMenuShell *menu_shell, gpointer data)
{
	RecurrencePage *rpage;

	rpage = RECURRENCE_PAGE (data);

	field_changed (rpage);
	make_ending_special (rpage);
	preview_recur (rpage);
}

static GtkWidget *
create_exception_dialog (RecurrencePage *rpage, const char *title, GtkWidget **date_edit)
{
	RecurrencePagePrivate *priv;
	GtkWidget *dialog, *toplevel;
	
	priv = rpage->priv;

	toplevel = gtk_widget_get_toplevel (priv->main);
	dialog = gtk_dialog_new_with_buttons (title, GTK_WINDOW (toplevel),
					      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					      GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					      NULL);

	*date_edit = comp_editor_new_date_edit (TRUE, FALSE, TRUE);
	gtk_widget_show (*date_edit);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), *date_edit, FALSE, TRUE, 6);
	
	return dialog;
}

/* Callback for the "add exception" button */
static void
exception_add_cb (GtkWidget *widget, gpointer data)
{
	RecurrencePage *rpage;
	RecurrencePagePrivate *priv;
	GtkWidget *dialog, *date_edit;
	gboolean date_set;
	
	rpage = RECURRENCE_PAGE (data);
	priv = rpage->priv;

	dialog = create_exception_dialog (rpage, "Add exception", &date_edit);
	
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		ECalComponentDateTime dt;
		struct icaltimetype icaltime = icaltime_null_time ();

		field_changed (rpage);
		
		dt.value = &icaltime;
		
		/* We use DATE values for exceptions, so we don't need a TZID. */
		dt.tzid = NULL;
		icaltime.is_date = 1;
		
		date_set = e_date_edit_get_date (E_DATE_EDIT (date_edit),
						 &icaltime.year,
						 &icaltime.month,
						 &icaltime.day);
		g_assert (date_set);
		
		append_exception (rpage, &dt);
		preview_recur (rpage);
	}
	
	gtk_widget_destroy (dialog);
}

/* Callback for the "modify exception" button */
static void
exception_modify_cb (GtkWidget *widget, gpointer data)
{
	RecurrencePage *rpage;
	RecurrencePagePrivate *priv;
	GtkWidget *dialog, *date_edit;
	const ECalComponentDateTime *current_dt;
	GtkTreeSelection *selection;
	GtkTreeIter iter;

	rpage = RECURRENCE_PAGE (data);
	priv = rpage->priv;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->exception_list));
	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		g_warning ("Could not get a selection to modify.");
		return;
	}

	current_dt = e_date_time_list_get_date_time (priv->exception_list_store, &iter);
	
	dialog = create_exception_dialog (rpage, "Modify exception", &date_edit);
	e_date_edit_set_date (E_DATE_EDIT (date_edit), 
			      current_dt->value->year, current_dt->value->month, current_dt->value->day);
	
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		ECalComponentDateTime dt;
		struct icaltimetype icaltime = icaltime_null_time ();
		struct icaltimetype *tt;

		field_changed (rpage);
		
		dt.value = &icaltime;
		tt = dt.value;
		e_date_edit_get_date (E_DATE_EDIT (date_edit), 
				      &tt->year, &tt->month, &tt->day);
		tt->hour = 0;
		tt->minute = 0;
		tt->second = 0;
		tt->is_date = 1;
		
		/* No TZID, since we are using a DATE value now. */
		dt.tzid = NULL;
		
		e_date_time_list_set_date_time (priv->exception_list_store, &iter, &dt);
		preview_recur (rpage);
	}
	
	gtk_widget_destroy (dialog);
}

/* Callback for the "delete exception" button */
static void
exception_delete_cb (GtkWidget *widget, gpointer data)
{
	RecurrencePage *rpage;
	RecurrencePagePrivate *priv;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreePath *path;
	gboolean valid_iter;

	rpage = RECURRENCE_PAGE (data);
	priv = rpage->priv;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->exception_list));
	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		g_warning ("Could not get a selection to delete.");
		return;
	}

	field_changed (rpage);

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
	preview_recur (rpage);
}

/* Callback used when a row is selected in the list of exception
 * dates.  We must update the date/time widgets to reflect the
 * exception's value.
 */
static void
exception_selection_changed_cb (GtkTreeSelection *selection, gpointer data)
{
	RecurrencePage *rpage;
	RecurrencePagePrivate *priv;
	GtkTreeIter iter;

	rpage = RECURRENCE_PAGE (data);
	priv = rpage->priv;

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		gtk_widget_set_sensitive (priv->exception_modify, FALSE);
		gtk_widget_set_sensitive (priv->exception_delete, FALSE);
		return;
	}

	gtk_widget_set_sensitive (priv->exception_modify, TRUE);
	gtk_widget_set_sensitive (priv->exception_delete, TRUE);
}

/* This is called when any field is changed; it notifies upstream. */
static void
field_changed (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;

	priv = rpage->priv;
	
	if (!priv->updating)
		comp_editor_page_notify_changed (COMP_EDITOR_PAGE (rpage));
}

/* Hooks the widget signals */
static void
init_widgets (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;
	ECalendar *ecal;
	GtkAdjustment *adj;
	GtkWidget *menu;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell_renderer;

	priv = rpage->priv;

	/* Recurrence preview */

	priv->preview_calendar = e_calendar_new ();
	ecal = E_CALENDAR (priv->preview_calendar);
	priv->preview_calendar_config = e_mini_calendar_config_new (ecal);
	g_signal_connect((ecal->calitem), "date_range_changed",
			    G_CALLBACK (preview_date_range_changed_cb),
			    rpage);
	e_calendar_item_set_max_days_sel (ecal->calitem, 0);
	gtk_container_add (GTK_CONTAINER (priv->preview_bin),
			   priv->preview_calendar);
	gtk_widget_show (priv->preview_calendar);

	e_calendar_item_set_get_time_callback (ecal->calitem,
					       (ECalendarItemGetTimeCallback) comp_editor_get_current_time,
					       rpage, NULL);

	/* Recurrence types */

	g_signal_connect(priv->recurs, "toggled", G_CALLBACK (type_toggled_cb), rpage);

	/* Recurrence interval */

	adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->interval_value));
	g_signal_connect((adj), "value_changed",
			    G_CALLBACK (interval_value_changed_cb),
			    rpage);

	/* Recurrence units */

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->interval_unit));
	g_signal_connect((menu), "selection_done",
			    G_CALLBACK (interval_selection_done_cb),
			    rpage);

	/* Recurrence ending */

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->ending_menu));
	g_signal_connect((menu), "selection_done",
			    G_CALLBACK (ending_selection_done_cb), rpage);

	/* Exception buttons */

	g_signal_connect((priv->exception_add), "clicked",
			    G_CALLBACK (exception_add_cb), rpage);
	g_signal_connect((priv->exception_modify), "clicked",
			    G_CALLBACK (exception_modify_cb), rpage);
	g_signal_connect((priv->exception_delete), "clicked",
			    G_CALLBACK (exception_delete_cb), rpage);

	gtk_widget_set_sensitive (priv->exception_modify, FALSE);
	gtk_widget_set_sensitive (priv->exception_delete, FALSE);

	/* Exception list */

	/* Model */
	priv->exception_list_store = e_date_time_list_new ();
	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->exception_list),
				 GTK_TREE_MODEL (priv->exception_list_store));

	/* View */
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Date/Time"));
	cell_renderer = GTK_CELL_RENDERER (gtk_cell_renderer_text_new ());
	gtk_tree_view_column_pack_start (column, cell_renderer, TRUE);
	gtk_tree_view_column_add_attribute (column, cell_renderer, "text", E_DATE_TIME_LIST_COLUMN_DESCRIPTION);
	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->exception_list), column);

	g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->exception_list)), "changed",
			  G_CALLBACK (exception_selection_changed_cb), rpage);
}



static void
client_changed_cb (CompEditorPage *page, ECal *client, gpointer user_data)
{
	RecurrencePage *rpage = RECURRENCE_PAGE (page);

	sensitize_buttons (rpage);
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
recurrence_page_construct (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;

	priv = rpage->priv;

	priv->xml = glade_xml_new (EVOLUTION_GLADEDIR 
				   "/recurrence-page.glade", NULL, NULL);
	if (!priv->xml) {
		g_message ("recurrence_page_construct(): "
			   "Could not load the Glade XML file!");
		return NULL;
	}

	if (!get_widgets (rpage)) {
		g_message ("recurrence_page_construct(): "
			   "Could not find all widgets in the XML file!");
		return NULL;
	}

	init_widgets (rpage);

	g_signal_connect_after (G_OBJECT (rpage), "client_changed",
				G_CALLBACK (client_changed_cb), NULL);

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
recurrence_page_new (void)
{
	RecurrencePage *rpage;

	rpage = g_object_new (TYPE_RECURRENCE_PAGE, NULL);
	if (!recurrence_page_construct (rpage)) {
		g_object_unref (rpage);
		return NULL;
	}

	return rpage;
}


GtkWidget *make_exdate_date_edit (void);

GtkWidget *
make_exdate_date_edit (void)
{
	return comp_editor_new_date_edit (TRUE, TRUE, FALSE);
}

