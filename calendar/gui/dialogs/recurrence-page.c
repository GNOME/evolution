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

#include <gtk/gtksignal.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <glade/glade.h>
#include <gal/widgets/e-unicode.h>
#include "e-util/e-dialog-widgets.h"
#include "recurrence-page.h"



/* Private part of the RecurrencePage structure */
struct _RecurrencePagePrivate {
	/* Glade XML data */
	GladeXML *xml;

	/* Widgets from the Glade file */

	GtkWidget *main;

	GtkWidget *summary;
	GtkWidget *starting_date;
		   
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
};



static void recurrence_page_class_init (RecurrencePageClass *class);
static void recurrence_page_init (RecurrencePage *rpage);
static void recurrence_page_destroy (RecurrencePage *rpage);

static GtkWidget *recurrence_page_get_widget (EditorPage *page);
static void recurrence_page_fill_widgets (EditorPage *page, CalComponent *comp);
static void recurrence_page_fill_component (EditorPage *page, CalComponent *comp);
static void recurrence_page_set_summary (EditorPage *page, const char *summary);
static char *recurrence_page_get_summary (EditorPage *page);
static void recurrence_page_set_dtstart (EditorPage *page, time_t start);

static EditorPageClass *parent_class = NULL;



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

		recurrence_page_type = gtk_type_unique (EDITOR_PAGE_TYPE, &recurrence_page_info);
	}

	return recurrence_page_type;
}

/* Class initialization function for the recurrence page */
static void
recurrence_page_class_init (RecurrencePageClass *class)
{
	EditorPageClass *editor_page_class;
	GtkObjectClass *object_class;

	editor_page_class = (EditorPageClass *) class;
	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (EDITOR_PAGE_TYPE);

	editor_page_class->get_widget = recurrence_page_get_widget;
	editor_page_class->fill_widgets = recurrence_page_fill_widgets;
	editor_page_class->fill_component = recurrence_page_fill_component;
	editor_page_class->set_summary = recurrence_page_set_summary;
	editor_page_class->get_summary = recurrence_page_get_summary;
	editor_page_class->set_dtstart = recurrence_page_set_dtstart;

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
	priv->starting_date = NULL;
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
}

/* Frees the rows and the row data in the exceptions GtkCList */
static void
free_exception_clist_data (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;
	GtkCList *clist;
	int i;

	priv = rpage->priv;

	clist = GTK_CLIST (priv->exception_list);

	for (i = 0; i < clist->rows; i++) {
		gpointer data;

		data = gtk_clist_get_row_data (clist, i);
		g_free (data);
		gtk_clist_set_row_data (clist, i, NULL);
	}

	gtk_clist_clear (clist);
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
recurrence_page_get_widget (EditorPage *page)
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
	e_dialog_option_menu_set (priv->interval_unit, ICAL_DAILY_RECURRENCE, freq_map);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (menu), rpage);

	priv->ending_date = time (NULL);
	priv->ending_count = 1;

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->ending_menu));
	gtk_signal_handler_block_by_data (GTK_OBJECT (menu), rpage);
	e_dialog_option_menu_set (priv->ending_menu, ENDING_FOREVER, ending_types_map);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (menu), rpage);

	/* Exceptions list */
	free_exception_clist_data (GTK_CLIST (priv->exception_list));
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

/* Computes a weekday mask for the start day of a calendar component, for use in
 * a WeekdayPicker widget.
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

		label = gtk_label_new (_("This appointment contains recurrences that Evolution "
					 "cannot edit."));
		gtk_container_add (GTK_CONTAINER (priv->custom_warning_bin), label);
		gtk_widget_show_all (priv->custom_warning_bin);
		break;

	default:
		g_assert_not_reached ();
	}
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
	g_assert (priv->comp != NULL);

	/* Create a scratch component with the start/end and
	 * recurrence/excepttion information from the one we are editing.
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

	recur_to_comp_object (rpage, comp);

	tag_calendar_by_comp (E_CALENDAR (priv->preview_calendar), comp);
	gtk_object_unref (GTK_OBJECT (comp));
}

/* fill_widgets handler for the recurrence page.  This function is particularly
 * tricky because it has to discriminate between recurrences we support for
 * editing and the ones we don't.  We only support at most one recurrence rule;
 * no rdates or exrules (exdates are handled just fine elsewhere).
 */
static void
recurrence_page_fill_widgets (EditorPage *page, CalComponent *comp)
{
	RecurrencePage *rpage;
	RecurrencePagePrivate *priv;
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

	clear_widgets (rpage);

	fill_exception_widgets (rpage, comp);

	/* Set up defaults for the special widgets */
	set_special_defaults (rpage);

	/* No recurrences? */

	if (!cal_component_has_rdates (comp)
	    && !cal_component_has_rrules (comp)
	    && !cal_component_has_exrules (comp)) {
		gtk_signal_handler_block_by_data (GTK_OBJECT (priv->none), rpage);
		gtk_signal_handler_block_by_data (GTK_OBJECT (priv->simple), rpage);
		gtk_signal_handler_block_by_data (GTK_OBJECT (priv->custom), rpage);
		e_dialog_radio_set (priv->none, RECUR_NONE, type_map);
		gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->none), rpage);
		gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->simple), rpage);
		gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->custom), rpage);

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
		e_dialog_option_menu_set (priv->interval_unit, ICAL_DAILY_RECURRENCE, freq_map);
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
		e_dialog_option_menu_set (priv->interval_unit, ICAL_WEEKLY_RECURRENCE, freq_map);
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
		e_dialog_option_menu_set (priv->interval_unit, ICAL_MONTHLY_RECURRENCE, freq_map);
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
		e_dialog_option_menu_set (priv->interval_unit, ICAL_YEARLY_RECURRENCE, freq_map);
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
}



/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (RecurrencePage *rpage)
{
	RecurrencePagePrivate *priv;
	GtkWidget *toplevel;

	priv = rpage->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	toplevel = GW ("recurrence-toplevel");
	priv->main = GW ("recurrence-page");
	if (!(toplevel && priv->main))
		return NULL;

	gtk_widget_ref (priv->main);
	gtk_widget_unparent (priv->main);
	gtk_widget_destroy (toplevel);

	priv->summary = GW ("summary");
	priv->starting_date = GW ("starting-date");
	      
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
		&& priv->starting_date
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
