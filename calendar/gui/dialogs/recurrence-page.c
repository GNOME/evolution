/* Evolution calendar - Recurrence page of the calendar component dialogs
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          Miguel de Icaza <miguel@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *          JP Rosevear <jpr@ximian.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkoptionmenu.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkspinbutton.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <glade/glade.h>
#include <gal/widgets/e-unicode.h>
#include "e-util/e-dialog-widgets.h"
#include "widgets/misc/e-dateedit.h"
#include "../calendar-config.h"
#include "../tag-calendar.h"
#include "../weekday-picker.h"
#include "../widget-util.h"
#include "comp-editor-util.h"
#include "recurrence-page.h"



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
	CalComponent *comp;

	/* Glade XML data */
	GladeXML *xml;

	/* Widgets from the Glade file */

	GtkWidget *main;

	GtkWidget *summary;
	GtkWidget *date_time;
		   
	GtkWidget *none;
	GtkWidget *simple;
	GtkWidget *custom;
		   
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
	GtkWidget *month_index_spin;
	int month_index;

	GtkWidget *month_day_menu;
	enum month_day_options month_day;

	/* For ending date, created by hand */
	GtkWidget *ending_date_edit;
	time_t ending_date;

	/* For ending count of occurrences, created by hand */
	GtkWidget *ending_count_spin;
	int ending_count;

	/* More widgets from the Glade file */

	GtkWidget *exception_date;
	GtkWidget *exception_list;
	GtkWidget *exception_add;
	GtkWidget *exception_modify;
	GtkWidget *exception_delete;

	GtkWidget *preview_bin;

	/* For the recurrence preview, the actual widget */
	GtkWidget *preview_calendar;

	gboolean updating;
};



static void recurrence_page_class_init (RecurrencePageClass *class);
static void recurrence_page_init (RecurrencePage *rpage);
static void recurrence_page_destroy (GtkObject *object);

static GtkWidget *recurrence_page_get_widget (CompEditorPage *page);
static void recurrence_page_fill_widgets (CompEditorPage *page, CalComponent *comp);
static void recurrence_page_fill_component (CompEditorPage *page, CalComponent *comp);
static void recurrence_page_set_summary (CompEditorPage *page, const char *summary);
static void recurrence_page_set_dates (CompEditorPage *page, CompEditorPageDates *dates);

static void field_changed (RecurrencePage *apage);

static CompEditorPageClass *parent_class = NULL;



/**
 * recurrence_page_get_type:
 * 
 * Registers the #RecurrencePage class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the #RecurrencePage class.
 **/
GtkType
recurrence_page_get_type (void)
{
	static GtkType recurrence_page_type;

	if (!recurrence_page_type) {
		static const GtkTypeInfo recurrence_page_info = {
			"RecurrencePage",
			sizeof (RecurrencePage),
			sizeof (RecurrencePageClass),
			(GtkClassInitFunc) recurrence_page_class_init,
			(GtkObjectInitFunc) recurrence_page_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		recurrence_page_type = gtk_type_unique (TYPE_COMP_EDITOR_PAGE,
							&recurrence_page_info);
	}

	return recurrence_page_type;
}

/* Class initialization function for the recurrence page */
static void
recurrence_page_class_init (RecurrencePageClass *class)
{
	CompEditorPageClass *editor_page_class;
	GtkObjectClass *object_class;

	editor_page_class = (CompEditorPageClass *) class;
	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (TYPE_COMP_EDITOR_PAGE);

	editor_page_class->get_widget = recurrence_page_get_widget;
	editor_page_class->fill_widgets = recurrence_page_fill_widgets;
	editor_page_class->fill_component = recurrence_page_fill_component;
	editor_page_class->set_summary = recurrence_page_set_summary;
	editor_page_class->set_dates = recurrence_page_set_dates;

	object_class->destroy = recurrence_page_destroy;
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
	priv->summary = NULL;
	priv->date_time = NULL;
	priv->none = NULL;
	priv->simple = NULL;
	priv->custom = NULL;
	priv->params = NULL;
	priv->interval_value = NULL;
	priv->interval_unit = NULL;
	priv->special = NULL;
	priv->ending_menu = NULL;
	priv->ending_special = NULL;
	priv->custom_warning_bin = NULL;
	priv->weekday_picker = NULL;
	priv->month_index_spin = NULL;
	priv->month_day_menu = NULL;
	priv->ending_date_edit = NULL;
	priv->ending_count_spin = NULL;
	priv->exception_date = NULL;
	priv->exception_list = NULL;
	priv->exception_add = NULL;
	priv->exception_modify = NULL;
	priv->exception_delete = NULL;
	priv->preview_bin = NULL;
	priv->preview_calendar = NULL;

	priv->comp = NULL;
}

/* Frees the rows and the row data in the exceptions GtkCList */
static void
free_exception_clist_data (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;
	int i;

	priv = rpage->priv;

	if (priv->exception_list) {
		GtkCList *clist = GTK_CLIST (priv->exception_list);

		for (i = 0; i < clist->rows; i++) {
			gpointer data;
			
			data = gtk_clist_get_row_data (clist, i);
			g_free (data);
			gtk_clist_set_row_data (clist, i, NULL);
		}
	
		gtk_clist_clear (clist);
	}
}

/* Destroy handler for the recurrence page */
static void
recurrence_page_destroy (GtkObject *object)
{
	RecurrencePage *rpage;
	RecurrencePagePrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_RECURRENCE_PAGE (object));

	rpage = RECURRENCE_PAGE (object);
	priv = rpage->priv;

	if (priv->xml) {
		gtk_object_unref (GTK_OBJECT (priv->xml));
		priv->xml = NULL;
	}

	free_exception_clist_data (rpage);

	g_free (priv);
	rpage->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
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

/* Fills the widgets with default values */
static void
clear_widgets (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;
	GtkAdjustment *adj;
	GtkWidget *menu;

	priv = rpage->priv;

	priv->weekday_day_mask = 0;

	priv->month_index = 1;
	priv->month_day = MONTH_DAY_NTH;

	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->none), rpage);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->simple), rpage);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->custom), rpage);
	e_dialog_radio_set (priv->none, RECUR_NONE, type_map);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->none), rpage);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->simple), rpage);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->custom), rpage);

	adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->interval_value));
	gtk_signal_handler_block_by_data (GTK_OBJECT (adj), rpage);
	e_dialog_spin_set (priv->interval_value, 1);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (adj), rpage);

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->interval_unit));
	gtk_signal_handler_block_by_data (GTK_OBJECT (menu), rpage);
	e_dialog_option_menu_set (priv->interval_unit,
				  ICAL_DAILY_RECURRENCE,
				  freq_map);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (menu), rpage);

	priv->ending_date = time (NULL);
	priv->ending_count = 1;

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->ending_menu));
	gtk_signal_handler_block_by_data (GTK_OBJECT (menu), rpage);
	e_dialog_option_menu_set (priv->ending_menu, 
				  ENDING_FOREVER,
				  ending_types_map);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (menu), rpage);

	/* Exceptions list */
	free_exception_clist_data (rpage);
}

/* Builds a static string out of an exception date */
static char *
get_exception_string (time_t t)
{
	static char buf[256];

	strftime (buf, sizeof (buf), _("%a %b %d %Y"), localtime (&t));
	return buf;
}

/* Appends an exception date to the list */
static void
append_exception (RecurrencePage *rpage, time_t t)
{
	RecurrencePagePrivate *priv;
	time_t *tt;
	char *c[1];
	int i;
	GtkCList *clist;

	priv = rpage->priv;

	tt = g_new (time_t, 1);
	*tt = t;

	clist = GTK_CLIST (priv->exception_list);

	gtk_signal_handler_block_by_data (GTK_OBJECT (clist), rpage);

	c[0] = get_exception_string (t);
	i = gtk_clist_append (clist, c);

	gtk_clist_set_row_data (clist, i, tt);

	gtk_clist_select_row (clist, i, 0);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (clist), rpage);

	e_date_edit_set_time (E_DATE_EDIT (priv->exception_date), t);

	gtk_widget_set_sensitive (priv->exception_modify, TRUE);
	gtk_widget_set_sensitive (priv->exception_delete, TRUE);
}

/* Fills in the exception widgets with the data from the calendar component */
static void
fill_exception_widgets (RecurrencePage *rpage, CalComponent *comp)
{
	RecurrencePagePrivate *priv;
	GSList *list, *l;
	gboolean added;

	priv = rpage->priv;

	cal_component_get_exdate_list (comp, &list);

	added = FALSE;

	for (l = list; l; l = l->next) {
		CalComponentDateTime *cdt;
		time_t ext;

		added = TRUE;

		cdt = l->data;
		ext = icaltime_as_timet (*cdt->value);
		append_exception (rpage, ext);
	}

	cal_component_free_exdate_list (list);

	if (added)
		gtk_clist_select_row (GTK_CLIST (priv->exception_list), 0, 0);
}

/* Computes a weekday mask for the start day of a calendar component,
 * for use in a WeekdayPicker widget.
 */
static guint8
get_start_weekday_mask (CalComponent *comp)
{
	CalComponentDateTime dt;
	guint8 retval;

	cal_component_get_dtstart (comp, &dt);

	if (dt.value) {
		time_t t;
		struct tm tm;

		t = icaltime_as_timet (*dt.value);
		tm = *localtime (&t);

		retval = 0x1 << tm.tm_wday;
	} else
		retval = 0;

	cal_component_free_datetime (&dt);

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
	enum recur_type type;
	GtkWidget *label;

	priv = rpage->priv;

	type = e_dialog_radio_get (priv->none, type_map);

	if (GTK_BIN (priv->custom_warning_bin)->child)
		gtk_widget_destroy (GTK_BIN (priv->custom_warning_bin)->child);

	switch (type) {
	case RECUR_NONE:
		gtk_widget_set_sensitive (priv->params, FALSE);
		gtk_widget_show (priv->params);
		gtk_widget_hide (priv->custom_warning_bin);
		break;

	case RECUR_SIMPLE:
		gtk_widget_set_sensitive (priv->params, TRUE);
		gtk_widget_show (priv->params);
		gtk_widget_hide (priv->custom_warning_bin);
		break;

	case RECUR_CUSTOM:
		gtk_widget_set_sensitive (priv->params, FALSE);
		gtk_widget_hide (priv->params);

		label = gtk_label_new (_("This appointment contains "
					 "recurrences that Evolution "
					 "cannot edit."));
		gtk_container_add (GTK_CONTAINER (priv->custom_warning_bin),
				   label);
		gtk_widget_show_all (priv->custom_warning_bin);
		break;

	default:
		g_assert_not_reached ();
	}
}

/* Encondes a position/weekday pair into the proper format for
 * icalrecurrencetype.by_day.
 */
static short
nth_weekday (int pos, icalrecurrencetype_weekday weekday)
{
	g_assert (pos > 0 && pos <= 5);

	return (pos << 3) | (int) weekday;
}

/* Gets the simple recurrence data from the recurrence widgets and stores it in
 * the calendar component.
 */
static void
simple_recur_to_comp (RecurrencePage *rpage, CalComponent *comp)
{
	RecurrencePagePrivate *priv;
	struct icalrecurrencetype r;
	GSList l;
	enum ending_type ending_type;

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
		int day_index;
		enum month_day_options month_day;

		g_assert (GTK_BIN (priv->special)->child != NULL);
		g_assert (priv->month_index_spin != NULL);
		g_assert (GTK_IS_SPIN_BUTTON (priv->month_index_spin));
		g_assert (priv->month_day_menu != NULL);
		g_assert (GTK_IS_OPTION_MENU (priv->month_day_menu));

		day_index = e_dialog_spin_get_int (priv->month_index_spin);
		month_day = e_dialog_option_menu_get (priv->month_day_menu,
						      month_day_options_map);

		switch (month_day) {
		case MONTH_DAY_NTH:
			r.by_month_day[0] = day_index;
			break;

		case MONTH_DAY_MON:
			r.by_day[0] = nth_weekday (day_index, 
						   ICAL_MONDAY_WEEKDAY);
			break;

		case MONTH_DAY_TUE:
			r.by_day[0] = nth_weekday (day_index,
						   ICAL_TUESDAY_WEEKDAY);
			break;

		case MONTH_DAY_WED:
			r.by_day[0] = nth_weekday (day_index,
						   ICAL_WEDNESDAY_WEEKDAY);
			break;

		case MONTH_DAY_THU:
			r.by_day[0] = nth_weekday (day_index,
						   ICAL_THURSDAY_WEEKDAY);
			break;

		case MONTH_DAY_FRI:
			r.by_day[0] = nth_weekday (day_index,
						   ICAL_FRIDAY_WEEKDAY);
			break;

		case MONTH_DAY_SAT:
			r.by_day[0] = nth_weekday (day_index,
						   ICAL_SATURDAY_WEEKDAY);
			break;

		case MONTH_DAY_SUN:
			r.by_day[0] = nth_weekday (day_index,
						   ICAL_SUNDAY_WEEKDAY);
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

		r.until = icaltime_from_timet (
			e_date_edit_get_time (E_DATE_EDIT (priv->ending_date_edit)),
			TRUE);
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

	cal_component_set_rrule_list (comp, &l);
}

/* Fills a component with the data from the recurrence page; in the case of a
 * custom recurrence, it leaves it intact.
 */
static void
fill_component (RecurrencePage *rpage, CalComponent *comp)
{
	RecurrencePagePrivate *priv;
	enum recur_type recur_type;
	GtkCList *exception_list;
	GSList *list;
	int i;

	priv = rpage->priv;

	recur_type = e_dialog_radio_get (priv->none, type_map);

	switch (recur_type) {
	case RECUR_NONE:
		cal_component_set_rdate_list (comp, NULL);
		cal_component_set_rrule_list (comp, NULL);
		cal_component_set_exrule_list (comp, NULL);
		break;

	case RECUR_SIMPLE:
		cal_component_set_rdate_list (comp, NULL);
		cal_component_set_exrule_list (comp, NULL);
		simple_recur_to_comp (rpage, comp);
		break;

	case RECUR_CUSTOM:
		/* We just keep whatever the component has currently */
		break;

	default:
		g_assert_not_reached ();
	}

	/* Set exceptions */

	list = NULL;
	exception_list = GTK_CLIST (priv->exception_list);
	for (i = 0; i < exception_list->rows; i++) {
		CalComponentDateTime *cdt;
		time_t *tim;

		cdt = g_new (CalComponentDateTime, 1);
		cdt->value = g_new (struct icaltimetype, 1);
		cdt->tzid = NULL;

		tim = gtk_clist_get_row_data (exception_list, i);
		g_assert (tim != NULL);
		*cdt->value = icaltime_from_timet (*tim, FALSE);

		list = g_slist_prepend (list, cdt);
	}

	cal_component_set_exdate_list (comp, list);
	cal_component_free_exdate_list (list);
}

/* Re-tags the recurrence preview calendar based on the current information of
 * the widgets in the recurrence page.
 */
static void
preview_recur (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;
	CalComponent *comp;
	CalComponentDateTime cdt;
	GSList *l;

	priv = rpage->priv;

	/* If our component has not been set yet through ::fill_widgets(), we
	 * cannot preview the recurrence.
	 */
	if (!priv->comp)
		return;

	/* Create a scratch component with the start/end and
	 * recurrence/exception information from the one we are editing.
	 */

	comp = cal_component_new ();
	cal_component_set_new_vtype (comp, CAL_COMPONENT_EVENT);

	cal_component_get_dtstart (priv->comp, &cdt);
	cal_component_set_dtstart (comp, &cdt);
	cal_component_free_datetime (&cdt);

	cal_component_get_dtend (priv->comp, &cdt);
	cal_component_set_dtend (comp, &cdt);
	cal_component_free_datetime (&cdt);

	cal_component_get_exdate_list (priv->comp, &l);
	cal_component_set_exdate_list (comp, l);
	cal_component_free_exdate_list (l);

	cal_component_get_exrule_list (priv->comp, &l);
	cal_component_set_exrule_list (comp, l);
	cal_component_free_recur_list (l);

	cal_component_get_rdate_list (priv->comp, &l);
	cal_component_set_rdate_list (comp, l);
	cal_component_free_period_list (l);

	cal_component_get_rrule_list (priv->comp, &l);
	cal_component_set_rrule_list (comp, l);
	cal_component_free_recur_list (l);

	fill_component (rpage, comp);

	tag_calendar_by_comp (E_CALENDAR (priv->preview_calendar), comp);
	gtk_object_unref (GTK_OBJECT (comp));
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
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	wp = WEEKDAY_PICKER (weekday_picker_new ());

	priv->weekday_picker = GTK_WIDGET (wp);
	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (wp), FALSE, FALSE, 0);

	gtk_widget_show_all (hbox);

	/* Set the weekdays */

	weekday_picker_set_week_start_day (wp, calendar_config_get_week_start_day ());
	weekday_picker_set_days (wp, priv->weekday_day_mask);
	weekday_picker_set_blocked_days (wp, priv->weekday_blocked_day_mask);

	gtk_signal_connect (GTK_OBJECT (wp), "changed",
			    GTK_SIGNAL_FUNC (weekday_picker_changed_cb),
			    rpage);
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
		gtk_menu_append (GTK_MENU (menu), item);
		gtk_widget_show (item);
	}

	omenu = gtk_option_menu_new ();
	gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);

	return omenu;
}

/* For monthly recurrences, changes the valid range of the recurrence day index
 * spin button; e.g. month days are 1-31 while the valid range for a Sunday is
 * the 1st through 5th of the month.
 */
static void
adjust_day_index_spin (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;
	GtkAdjustment *adj;
	enum month_day_options month_day;

	priv = rpage->priv;

	g_assert (priv->month_day_menu != NULL);
	g_assert (GTK_IS_OPTION_MENU (priv->month_day_menu));
	g_assert (priv->month_index_spin != NULL);
	g_assert (GTK_IS_SPIN_BUTTON (priv->month_index_spin));

	month_day = e_dialog_option_menu_get (priv->month_day_menu,
					      month_day_options_map);

	adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->month_index_spin));

	switch (month_day) {
	case MONTH_DAY_NTH:
		adj->upper = 31;
		gtk_adjustment_changed (adj);
		break;

	case MONTH_DAY_MON:
	case MONTH_DAY_TUE:
	case MONTH_DAY_WED:
	case MONTH_DAY_THU:
	case MONTH_DAY_FRI:
	case MONTH_DAY_SAT:
	case MONTH_DAY_SUN:
		adj->upper = 5;
		gtk_adjustment_changed (adj);

		if (adj->value > 5) {
			adj->value = 5;
			gtk_adjustment_value_changed (adj);
		}

		break;

	default:
		g_assert_not_reached ();
	}
}

/* Callback used when the monthly day selection menu changes.  We need
 * to change the valid range of the day index spin button; e.g. days
 * are 1-31 while a Sunday is the 1st through 5th.
 */
static void
month_day_menu_selection_done_cb (GtkMenuShell *menu_shell, gpointer data)
{
	RecurrencePage *rpage;

	rpage = RECURRENCE_PAGE (data);

	adjust_day_index_spin (rpage);
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
	g_assert (priv->month_index_spin == NULL);
	g_assert (priv->month_day_menu == NULL);

	/* Create the widgets */

	hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (priv->special), hbox);

	label = gtk_label_new (_("on the"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	adj = GTK_ADJUSTMENT (gtk_adjustment_new (1, 1, 31, 1, 10, 10));
	priv->month_index_spin = gtk_spin_button_new (adj, 1, 0);
	gtk_box_pack_start (GTK_BOX (hbox), priv->month_index_spin,
			    FALSE, FALSE, 0);

	label = gtk_label_new (_("th"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	priv->month_day_menu = make_recur_month_menu ();
	gtk_box_pack_start (GTK_BOX (hbox), priv->month_day_menu,
			    FALSE, FALSE, 0);

	gtk_widget_show_all (hbox);

	/* Set the options */

	e_dialog_spin_set (priv->month_index_spin, priv->month_index);
	e_dialog_option_menu_set (priv->month_day_menu,
				  priv->month_day,
				  month_day_options_map);
	adjust_day_index_spin (rpage);

	gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
			    GTK_SIGNAL_FUNC (month_index_value_changed_cb),
			    rpage);

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->month_day_menu));
	gtk_signal_connect (GTK_OBJECT (menu), "selection_done",
			    GTK_SIGNAL_FUNC (month_day_menu_selection_done_cb),
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

	if (GTK_BIN (priv->special)->child != NULL) {
		gtk_widget_destroy (GTK_BIN (priv->special)->child);

		priv->weekday_picker = NULL;
		priv->month_index_spin = NULL;
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

	priv->ending_date_edit = date_edit_new (TRUE, FALSE);
	de = E_DATE_EDIT (priv->ending_date_edit);

	gtk_container_add (GTK_CONTAINER (priv->ending_special),
			   GTK_WIDGET (de));
	gtk_widget_show_all (GTK_WIDGET (de));

	/* Set the value */

	e_date_edit_set_time (de, priv->ending_date);

	gtk_signal_connect (GTK_OBJECT (de), "changed",
			    GTK_SIGNAL_FUNC (ending_until_changed_cb), rpage);
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
	gtk_box_pack_start (GTK_BOX (hbox), priv->ending_count_spin,
			    FALSE, FALSE, 0);

	label = gtk_label_new (_("occurrences"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	gtk_widget_show_all (hbox);

	/* Set the values */

	e_dialog_spin_set (priv->ending_count_spin, priv->ending_count);

	gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
			    GTK_SIGNAL_FUNC (ending_count_value_changed_cb),
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
	gtk_signal_handler_block_by_data (GTK_OBJECT (menu), rpage);

	if (r->count == 0) {
		if (r->until.year == 0) {
			/* Forever */

			e_dialog_option_menu_set (priv->ending_menu,
						  ENDING_FOREVER,
						  ending_types_map);
		} else {
			/* Ending date */

			priv->ending_date = icaltime_as_timet (r->until);
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

	gtk_signal_handler_unblock_by_data (GTK_OBJECT (menu), rpage);

	make_ending_special (rpage);
}

/* fill_widgets handler for the recurrence page.  This function is particularly
 * tricky because it has to discriminate between recurrences we support for
 * editing and the ones we don't.  We only support at most one recurrence rule;
 * no rdates or exrules (exdates are handled just fine elsewhere).
 */
static void
recurrence_page_fill_widgets (CompEditorPage *page, CalComponent *comp)
{
	RecurrencePage *rpage;
	RecurrencePagePrivate *priv;
	CalComponentText text;
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
		gtk_object_unref (GTK_OBJECT (priv->comp));

	priv->comp = cal_component_clone (comp);

	/* Don't send off changes during this time */
	priv->updating = TRUE;

	/* Clean the page */
	clear_widgets (rpage);

	/* Summary */
	cal_component_get_summary (comp, &text);
	recurrence_page_set_summary (page, text.value);

	/* Dates */
	comp_editor_dates (&dates, comp);
	recurrence_page_set_dates (page, &dates);

	/* Exceptions */
	fill_exception_widgets (rpage, comp);

	/* Set up defaults for the special widgets */
	set_special_defaults (rpage);

	/* No recurrences? */

	if (!cal_component_has_rdates (comp)
	    && !cal_component_has_rrules (comp)
	    && !cal_component_has_exrules (comp)) {
		gtk_signal_handler_block_by_data (GTK_OBJECT (priv->none),
						  rpage);
		gtk_signal_handler_block_by_data (GTK_OBJECT (priv->simple),
						  rpage);
		gtk_signal_handler_block_by_data (GTK_OBJECT (priv->custom),
						  rpage);
		e_dialog_radio_set (priv->none, RECUR_NONE, type_map);
		gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->none),
						    rpage);
		gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->simple),
						    rpage);
		gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->custom),
						    rpage);

		gtk_widget_set_sensitive (priv->custom, FALSE);

		sensitize_recur_widgets (rpage);
		preview_recur (rpage);
		return;
	}

	/* See if it is a custom set we don't support */

	cal_component_get_rrule_list (comp, &rrule_list);
	len = g_slist_length (rrule_list);
	if (len > 1
	    || cal_component_has_rdates (comp)
	    || cal_component_has_exrules (comp))
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
		gtk_signal_handler_block_by_data (GTK_OBJECT (menu), rpage);
		e_dialog_option_menu_set (priv->interval_unit,
					  ICAL_DAILY_RECURRENCE,
					  freq_map);
		gtk_signal_handler_unblock_by_data (GTK_OBJECT (menu), rpage);
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
		gtk_signal_handler_block_by_data (GTK_OBJECT (menu), rpage);
		e_dialog_option_menu_set (priv->interval_unit,
					  ICAL_WEEKLY_RECURRENCE,
					  freq_map);
		gtk_signal_handler_unblock_by_data (GTK_OBJECT (menu), rpage);
		break;
	}

	case ICAL_MONTHLY_RECURRENCE:
		if (n_by_year_day != 0
		    || n_by_week_no != 0
		    || n_by_month != 0
		    || n_by_set_pos != 0)
			goto custom;

		if (n_by_month_day == 1) {
			int nth;

			nth = r->by_month_day[0];
			if (nth < 1)
				goto custom;

			priv->month_index = nth;
			priv->month_day = MONTH_DAY_NTH;
		} else if (n_by_day == 1) {
			enum icalrecurrencetype_weekday weekday;
			int pos;
			enum month_day_options month_day;

			weekday = icalrecurrencetype_day_day_of_week (r->by_day[0]);
			pos = icalrecurrencetype_day_position (r->by_day[0]);

			if (pos < 1)
				goto custom;

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

			priv->month_index = pos;
			priv->month_day = month_day;
		} else
			goto custom;

		menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->interval_unit));
		gtk_signal_handler_block_by_data (GTK_OBJECT (menu), rpage);
		e_dialog_option_menu_set (priv->interval_unit,
					  ICAL_MONTHLY_RECURRENCE,
					  freq_map);
		gtk_signal_handler_unblock_by_data (GTK_OBJECT (menu), rpage);
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
		gtk_signal_handler_block_by_data (GTK_OBJECT (menu), rpage);
		e_dialog_option_menu_set (priv->interval_unit,
					  ICAL_YEARLY_RECURRENCE,
					  freq_map);
		gtk_signal_handler_unblock_by_data (GTK_OBJECT (menu), rpage);
		break;

	default:
		goto custom;
	}

	/* If we got here it means it is a simple recurrence */

	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->none), rpage);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->simple), rpage);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->custom), rpage);
	e_dialog_radio_set (priv->simple, RECUR_SIMPLE, type_map);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->none), rpage);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->simple), rpage);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->custom), rpage);

	gtk_widget_set_sensitive (priv->custom, FALSE);

	sensitize_recur_widgets (rpage);
	make_recurrence_special (rpage);

	adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->interval_value));
	gtk_signal_handler_block_by_data (GTK_OBJECT (adj), rpage);
	e_dialog_spin_set (priv->interval_value, r->interval);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (adj), rpage);

	fill_ending_date (rpage, r);

	goto out;

 custom:

	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->none), rpage);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->simple), rpage);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->custom), rpage);
	e_dialog_radio_set (priv->custom, RECUR_CUSTOM, type_map);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->none), rpage);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->simple), rpage);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->custom), rpage);

	gtk_widget_set_sensitive (priv->custom, TRUE);
	sensitize_recur_widgets (rpage);

 out:

	cal_component_free_recur_list (rrule_list);
	preview_recur (rpage);

	priv->updating = FALSE;
}

/* fill_component handler for the recurrence page */
static void
recurrence_page_fill_component (CompEditorPage *page, CalComponent *comp)
{
	RecurrencePage *rpage;

	rpage = RECURRENCE_PAGE (page);
	fill_component (rpage, comp);
}

/* set_summary handler for the recurrence page */
static void
recurrence_page_set_summary (CompEditorPage *page, const char *summary)
{
	RecurrencePage *rpage;
	RecurrencePagePrivate *priv;
	gchar *s;
	
	rpage = RECURRENCE_PAGE (page);
	priv = rpage->priv;

	s = e_utf8_to_gtk_string (priv->summary, summary);
	gtk_label_set_text (GTK_LABEL (priv->summary), s);
	g_free (s);
}

/* set_dates handler for the recurrence page */
static void
recurrence_page_set_dates (CompEditorPage *page, CompEditorPageDates *dates)
{
	RecurrencePage *rpage;
	RecurrencePagePrivate *priv;
	CalComponentDateTime dt;
	struct icaltimetype icaltime;

	rpage = RECURRENCE_PAGE (page);
	priv = rpage->priv;

	comp_editor_date_label (dates, priv->date_time);

	/* Copy the dates to our component */

	if (!priv->comp)
		return;

	dt.value = &icaltime;
	dt.tzid = NULL;

	*dt.value = icaltime_from_timet (dates->start, FALSE);
	cal_component_set_dtstart (priv->comp, &dt);

	if (dates->end != 0) {
		*dt.value = icaltime_from_timet (dates->end, FALSE);
		cal_component_set_dtend (priv->comp, &dt);
	}
}



/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;

	priv = rpage->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	priv->main = GW ("recurrence-page");
	g_assert (priv->main);
	gtk_widget_ref (priv->main);
	gtk_widget_unparent (priv->main);

	priv->summary = GW ("summary");
	priv->date_time = GW ("date-time");
	      
	priv->none = GW ("none");
	priv->simple = GW ("simple");
	priv->custom = GW ("custom");
	priv->params = GW ("params");
	      
	priv->interval_value = GW ("interval-value");
	priv->interval_unit = GW ("interval-unit");
	priv->special = GW ("special");
	priv->ending_menu = GW ("ending-menu");
	priv->ending_special = GW ("ending-special");
	priv->custom_warning_bin = GW ("custom-warning-bin");
	      
	priv->exception_date = GW ("exception-date");
	priv->exception_list = GW ("exception-list");
	priv->exception_add = GW ("exception-add");
	priv->exception_modify = GW ("exception-modify");
	priv->exception_delete = GW ("exception-delete");
	      
	priv->preview_bin = GW ("preview-bin");

#undef GW
	
	return (priv->summary
		&& priv->date_time
		&& priv->none
		&& priv->simple
		&& priv->custom
		&& priv->params
		&& priv->interval_value
		&& priv->interval_unit
		&& priv->special
		&& priv->ending_menu
		&& priv->ending_special
		&& priv->custom_warning_bin
		&& priv->exception_date
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

	rpage = RECURRENCE_PAGE (data);

	field_changed (rpage);

	if (toggle->active) {
		sensitize_recur_widgets (rpage);
		preview_recur (rpage);
	}
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

/* Callback for the "add exception" button */
static void
exception_add_cb (GtkWidget *widget, gpointer data)
{
	RecurrencePage *rpage;
	RecurrencePagePrivate *priv;
	time_t t;

	rpage = RECURRENCE_PAGE (data);
	priv = rpage->priv;

	field_changed (rpage);

	t = e_date_edit_get_time (E_DATE_EDIT (priv->exception_date));
	append_exception (rpage, t);
	preview_recur (rpage);
}

/* Callback for the "modify exception" button */
static void
exception_modify_cb (GtkWidget *widget, gpointer data)
{
	RecurrencePage *rpage;
	RecurrencePagePrivate *priv;
	GtkCList *clist;
	time_t *t;
	int sel;

	rpage = RECURRENCE_PAGE (data);
	priv = rpage->priv;

	clist = GTK_CLIST (priv->exception_list);
	if (!clist->selection)
		return;

	field_changed (rpage);

	sel = GPOINTER_TO_INT (clist->selection->data);

	t = gtk_clist_get_row_data (clist, sel);
	*t = e_date_edit_get_time (E_DATE_EDIT (priv->exception_date));

	gtk_clist_set_text (clist, sel, 0, get_exception_string (*t));

	preview_recur (rpage);
}

/* Callback for the "delete exception" button */
static void
exception_delete_cb (GtkWidget *widget, gpointer data)
{
	RecurrencePage *rpage;
	RecurrencePagePrivate *priv;
	GtkCList *clist;
	int sel;
	time_t *t;

	rpage = RECURRENCE_PAGE (data);
	priv = rpage->priv;

	clist = GTK_CLIST (priv->exception_list);
	if (!clist->selection)
		return;

	field_changed (rpage);

	sel = GPOINTER_TO_INT (clist->selection->data);

	t = gtk_clist_get_row_data (clist, sel);
	g_assert (t != NULL);
	g_free (t);

	gtk_clist_remove (clist, sel);
	if (sel >= clist->rows)
		sel--;

	if (clist->rows > 0)
		gtk_clist_select_row (clist, sel, 0);
	else {
		gtk_widget_set_sensitive (priv->exception_modify, FALSE);
		gtk_widget_set_sensitive (priv->exception_delete, FALSE);
	}

	preview_recur (rpage);
}

/* Callback used when a row is selected in the list of exception
 * dates.  We must update the date/time widgets to reflect the
 * exception's value.
 */
static void
exception_select_row_cb (GtkCList *clist, gint row, gint col,
			 GdkEvent *event, gpointer data)
{
	RecurrencePage *rpage;
	RecurrencePagePrivate *priv;
	time_t *t;

	rpage = RECURRENCE_PAGE (data);
	priv = rpage->priv;

	t = gtk_clist_get_row_data (clist, row);
	g_assert (t != NULL);

	e_date_edit_set_time (E_DATE_EDIT (priv->exception_date), *t);
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

	priv = rpage->priv;

	/* Recurrence preview */

	priv->preview_calendar = e_calendar_new ();
	ecal = E_CALENDAR (priv->preview_calendar);
	gtk_signal_connect (GTK_OBJECT (ecal->calitem), "date_range_changed",
			    GTK_SIGNAL_FUNC (preview_date_range_changed_cb),
			    rpage);
	calendar_config_configure_e_calendar (ecal);
	e_calendar_item_set_max_days_sel (ecal->calitem, 0);
	gtk_container_add (GTK_CONTAINER (priv->preview_bin),
			   priv->preview_calendar);
	gtk_widget_show (priv->preview_calendar);

	/* Recurrence types */

	gtk_signal_connect (GTK_OBJECT (priv->none), "toggled",
			    GTK_SIGNAL_FUNC (type_toggled_cb), rpage);
	gtk_signal_connect (GTK_OBJECT (priv->simple), "toggled",
			    GTK_SIGNAL_FUNC (type_toggled_cb), rpage);
	gtk_signal_connect (GTK_OBJECT (priv->custom), "toggled",
			    GTK_SIGNAL_FUNC (type_toggled_cb), rpage);

	/* Recurrence interval */

	adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->interval_value));
	gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
			    GTK_SIGNAL_FUNC (interval_value_changed_cb),
			    rpage);

	/* Recurrence units */

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->interval_unit));
	gtk_signal_connect (GTK_OBJECT (menu), "selection_done",
			    GTK_SIGNAL_FUNC (interval_selection_done_cb),
			    rpage);

	/* Recurrence ending */

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->ending_menu));
	gtk_signal_connect (GTK_OBJECT (menu), "selection_done",
			    GTK_SIGNAL_FUNC (ending_selection_done_cb), rpage);

	/* Exception buttons */

	gtk_signal_connect (GTK_OBJECT (priv->exception_add), "clicked",
			    GTK_SIGNAL_FUNC (exception_add_cb), rpage);
	gtk_signal_connect (GTK_OBJECT (priv->exception_modify), "clicked",
			    GTK_SIGNAL_FUNC (exception_modify_cb), rpage);
	gtk_signal_connect (GTK_OBJECT (priv->exception_delete), "clicked",
			    GTK_SIGNAL_FUNC (exception_delete_cb), rpage);

	/* Selections in the exceptions list */

	gtk_signal_connect (GTK_OBJECT (priv->exception_list), "select_row",
			    GTK_SIGNAL_FUNC (exception_select_row_cb), rpage);
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
				   "/recurrence-page.glade", NULL);
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

	rpage = gtk_type_new (TYPE_RECURRENCE_PAGE);
	if (!recurrence_page_construct (rpage)) {
		gtk_object_unref (GTK_OBJECT (rpage));
		return NULL;
	}

	return rpage;
}
