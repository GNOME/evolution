/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Evolution calendar - Event editor dialog
 *
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
#include <gnome.h>
#include <glade/glade.h>
#include <e-util/e-dialog-widgets.h>
#include <widgets/misc/e-dateedit.h>
#include <gal/widgets/e-unicode.h>
#include <gal/widgets/e-categories.h>
#include <cal-util/timeutil.h>
#include "dialogs/delete-comp.h"
#include "calendar-config.h"
#include "event-editor.h"
#include "e-meeting-edit.h"
#include "calendar-config.h"
#include "tag-calendar.h"
#include "weekday-picker.h"
#include "widget-util.h"



/* Options for monthly recurrences */
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

struct _EventEditorPrivate {
	/* Glade XML data */
	GladeXML *xml;

	/* UI handler */
	BonoboUIComponent *uic;

	/* Client to use */
	CalClient *client;

	/* Calendar object/uid we are editing; this is an internal copy */
	CalComponent *comp;

	/* Widgets from the Glade file */

	GtkWidget *app;

	GtkWidget *general_summary;

	GtkWidget *start_time;
	GtkWidget *end_time;
	GtkWidget *all_day_event;

	GtkWidget *description;

	GtkWidget *alarm_display;
	GtkWidget *alarm_program;
	GtkWidget *alarm_audio;
	GtkWidget *alarm_mail;
	GtkWidget *alarm_display_amount;
	GtkWidget *alarm_display_unit;
	GtkWidget *alarm_audio_amount;
	GtkWidget *alarm_audio_unit;
	GtkWidget *alarm_program_amount;
	GtkWidget *alarm_program_unit;
	GtkWidget *alarm_program_run_program;
	GtkWidget *alarm_program_run_program_entry;
	GtkWidget *alarm_mail_amount;
	GtkWidget *alarm_mail_unit;
	GtkWidget *alarm_mail_mail_to;

	GtkWidget *classification_public;
	GtkWidget *classification_private;
	GtkWidget *classification_confidential;

	GtkWidget *categories;
	GtkWidget *categories_btn;

	GtkWidget *recurrence_summary;
	GtkWidget *recurrence_starting_date;

	GtkWidget *recurrence_none;
	GtkWidget *recurrence_simple;
	GtkWidget *recurrence_custom;

	GtkWidget *recurrence_params;
	GtkWidget *recurrence_interval_value;
	GtkWidget *recurrence_interval_unit;
	GtkWidget *recurrence_special;
	GtkWidget *recurrence_ending_menu;
	GtkWidget *recurrence_ending_special;
	GtkWidget *recurrence_custom_warning_bin;

	/* For weekly recurrences, created by hand */
	GtkWidget *recurrence_weekday_picker;
	guint8 recurrence_weekday_day_mask;
	guint8 recurrence_weekday_blocked_day_mask;

	/* For monthly recurrences, created by hand */
	GtkWidget *recurrence_month_index_spin;
	int recurrence_month_index;

	GtkWidget *recurrence_month_day_menu;
	enum month_day_options recurrence_month_day;

	/* For ending date, created by hand */
	GtkWidget *recurrence_ending_date_edit;
	time_t recurrence_ending_date;

	/* For ending count of occurrences, created by hand */
	GtkWidget *recurrence_ending_count_spin;
	int recurrence_ending_count;

	/* More widgets from the Glade file */

	GtkWidget *recurrence_exception_date;
	GtkWidget *recurrence_exception_list;
	GtkWidget *recurrence_exception_add;
	GtkWidget *recurrence_exception_modify;
	GtkWidget *recurrence_exception_delete;

	GtkWidget *recurrence_preview_bin;

	/* For the recurrence preview, the actual widget */
	GtkWidget *recurrence_preview_calendar;

	/* Call event_editor_set_changed() to set this to TRUE when any field
	   in the dialog is changed. When the user closes the dialog we will
	   prompt to save changes. */
	gboolean changed;
};



static void event_editor_class_init (EventEditorClass *class);
static void event_editor_init (EventEditor *ee);
static void event_editor_destroy (GtkObject *object);

static GtkObjectClass *parent_class;


static void append_exception (EventEditor *ee, time_t t);
static void check_all_day (EventEditor *ee);
static void set_all_day (GtkWidget *toggle, EventEditor *ee);
static void alarm_toggle (GtkWidget *toggle, EventEditor *ee);
static void date_changed_cb (EDateEdit *dedit, gpointer data);
static void preview_recur (EventEditor *ee);
static void recur_to_comp_object (EventEditor *ee, CalComponent *comp);
static void recurrence_exception_add_cb (GtkWidget *widget, EventEditor *ee);
static void recurrence_exception_modify_cb (GtkWidget *widget, EventEditor *ee);
static void recurrence_exception_delete_cb (GtkWidget *widget, EventEditor *ee);
static void field_changed		(GtkWidget	*widget,
					 EventEditor	*ee);
static void event_editor_set_changed	(EventEditor	*ee,
					 gboolean	 changed);
static gboolean prompt_to_save_changes	(EventEditor	*ee);
static void categories_clicked (GtkWidget *button, EventEditor *ee);



/**
 * event_editor_get_type:
 *
 * Registers the #EventEditor class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #EventEditor class.
 **/
GtkType
event_editor_get_type (void)
{
	static GtkType event_editor_type = 0;

	if (!event_editor_type) {
		static const GtkTypeInfo event_editor_info = {
			"EventEditor",
			sizeof (EventEditor),
			sizeof (EventEditorClass),
			(GtkClassInitFunc) event_editor_class_init,
			(GtkObjectInitFunc) event_editor_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		event_editor_type = gtk_type_unique (GTK_TYPE_OBJECT, &event_editor_info);
	}

	return event_editor_type;
}

/* Class initialization function for the event editor */
static void
event_editor_class_init (EventEditorClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (GTK_TYPE_OBJECT);

	object_class->destroy = event_editor_destroy;
}

/* Object initialization function for the event editor */
static void
event_editor_init (EventEditor *ee)
{
	EventEditorPrivate *priv;

	priv = g_new0 (EventEditorPrivate, 1);
	ee->priv = priv;

	event_editor_set_changed (ee, FALSE);
}

/* Frees the rows and the row data in the recurrence exceptions GtkCList */
static void
free_exception_clist_data (GtkCList *clist)
{
	int i;

	for (i = 0; i < clist->rows; i++) {
		gpointer data;

		data = gtk_clist_get_row_data (clist, i);
		g_free (data);
		gtk_clist_set_row_data (clist, i, NULL);
	}

	gtk_clist_clear (clist);
}

/* Destroy handler for the event editor */
static void
event_editor_destroy (GtkObject *object)
{
	EventEditor *ee;
	EventEditorPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_EVENT_EDITOR (object));

	ee = EVENT_EDITOR (object);
	priv = ee->priv;

	if (priv->uic) {
		bonobo_object_unref (BONOBO_OBJECT (priv->uic));
		priv->uic = NULL;
	}

	free_exception_clist_data (GTK_CLIST (priv->recurrence_exception_list));

	if (priv->app) {
		gtk_signal_disconnect_by_data (GTK_OBJECT (priv->app), ee);
		gtk_widget_destroy (priv->app);
		priv->app = NULL;
	}

	if (priv->comp) {
		gtk_object_unref (GTK_OBJECT (priv->comp));
		priv->comp = NULL;
	}

	if (priv->client) {
		gtk_signal_disconnect_by_data (GTK_OBJECT (priv->client), ee);
		gtk_object_unref (GTK_OBJECT (priv->client));
		priv->client = NULL;
	}

	if (priv->xml) {
		gtk_object_unref (GTK_OBJECT (priv->xml));
		priv->xml = NULL;
	}

	g_free (priv);
	ee->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/* Creates an appropriate title for the event editor dialog */
static char *
make_title_from_comp (CalComponent *comp)
{
	const char *summary;
	CalComponentVType type;
	CalComponentText text;

	if (!comp)
		return g_strdup (_("Edit Appointment"));

	cal_component_get_summary (comp, &text);
	if (text.value)
		summary = text.value;
	else
		summary =  _("No summary");


	type = cal_component_get_vtype (comp);
	switch (type) {
	case CAL_COMPONENT_EVENT:
		return g_strdup_printf (_("Appointment - %s"), summary);

	case CAL_COMPONENT_TODO:
		return g_strdup_printf (_("Task - %s"), summary);

	case CAL_COMPONENT_JOURNAL:
		return g_strdup_printf (_("Journal entry - %s"), summary);

	default:
		g_message ("make_title_from_comp(): Cannot handle object of type %d", type);
		return NULL;
	}
}

/* Sets the event editor's window title from a calendar component */
static void
set_title_from_comp (EventEditor *ee, CalComponent *comp)
{
	EventEditorPrivate *priv;
	char *title, *tmp;

	priv = ee->priv;

	title = make_title_from_comp (comp);
	tmp = e_utf8_to_gtk_string (priv->app, title);
	g_free (title);

	if (tmp) {
		gtk_window_set_title (GTK_WINDOW (priv->app), tmp);
		g_free (tmp);
	} else {
		g_message ("set_title_from_comp(): Could not convert the title from UTF8");
		gtk_window_set_title (GTK_WINDOW (priv->app), "");
	}
}

/* Callback used when the recurrence weekday picker changes */
static void
recur_weekday_picker_changed_cb (WeekdayPicker *wp, gpointer data)
{
	EventEditor *ee;

	ee = EVENT_EDITOR (data);
	event_editor_set_changed (ee, TRUE);
	preview_recur (ee);
}

/* Creates the special contents for weekly recurrences */
static void
make_recur_weekly_special (EventEditor *ee)
{
	EventEditorPrivate *priv;
	GtkWidget *hbox;
	GtkWidget *label;
	WeekdayPicker *wp;

	priv = ee->priv;

	g_assert (GTK_BIN (priv->recurrence_special)->child == NULL);
	g_assert (priv->recurrence_weekday_picker == NULL);

	/* Create the widgets */

	hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (priv->recurrence_special), hbox);

	label = gtk_label_new (_("on"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	wp = WEEKDAY_PICKER (weekday_picker_new ());

	priv->recurrence_weekday_picker = GTK_WIDGET (wp);
	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (wp), FALSE, FALSE, 0);

	gtk_widget_show_all (hbox);

	/* Set the weekdays */

	weekday_picker_set_week_start_day (wp, calendar_config_get_week_start_day ());
	weekday_picker_set_days (wp, priv->recurrence_weekday_day_mask);
	weekday_picker_set_blocked_days (wp, priv->recurrence_weekday_blocked_day_mask);

	gtk_signal_connect (GTK_OBJECT (wp), "changed",
			    GTK_SIGNAL_FUNC (recur_weekday_picker_changed_cb), ee);
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
adjust_day_index_spin (EventEditor *ee)
{
	EventEditorPrivate *priv;
	GtkAdjustment *adj;
	enum month_day_options month_day;

	priv = ee->priv;

	g_assert (priv->recurrence_month_day_menu != NULL);
	g_assert (GTK_IS_OPTION_MENU (priv->recurrence_month_day_menu));
	g_assert (priv->recurrence_month_index_spin != NULL);
	g_assert (GTK_IS_SPIN_BUTTON (priv->recurrence_month_index_spin));

	month_day = e_dialog_option_menu_get (priv->recurrence_month_day_menu, month_day_options_map);

	adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->recurrence_month_index_spin));

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

/* Callback used when the monthly day selection menu changes.  We need to change
 * the valid range of the day index spin button; e.g. days are 1-31 while a
 * Sunday is the 1st through 5th.
 */
static void
month_day_menu_selection_done_cb (GtkMenuShell *menu_shell, gpointer data)
{
	EventEditor *ee;

	ee = EVENT_EDITOR (data);
	adjust_day_index_spin (ee);
	event_editor_set_changed (ee, TRUE);
	preview_recur (ee);
}

/* Callback used when the month index value changes. */
static void
recur_month_index_value_changed_cb (GtkAdjustment *adj, gpointer data)
{
	EventEditor *ee;

	ee = EVENT_EDITOR (data);
	event_editor_set_changed (ee, TRUE);
	preview_recur (ee);
}

/* Creates the special contents for monthly recurrences */
static void
make_recur_monthly_special (EventEditor *ee)
{
	EventEditorPrivate *priv;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkAdjustment *adj;
	GtkWidget *menu;

	priv = ee->priv;

	g_assert (GTK_BIN (priv->recurrence_special)->child == NULL);
	g_assert (priv->recurrence_month_index_spin == NULL);
	g_assert (priv->recurrence_month_day_menu == NULL);

	/* Create the widgets */

	hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (priv->recurrence_special), hbox);

	label = gtk_label_new (_("on the"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	adj = GTK_ADJUSTMENT (gtk_adjustment_new (1, 1, 31, 1, 10, 10));
	priv->recurrence_month_index_spin = gtk_spin_button_new (adj, 1, 0);
	gtk_box_pack_start (GTK_BOX (hbox), priv->recurrence_month_index_spin, FALSE, FALSE, 0);

	label = gtk_label_new (_("th"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	priv->recurrence_month_day_menu = make_recur_month_menu ();
	gtk_box_pack_start (GTK_BOX (hbox), priv->recurrence_month_day_menu, FALSE, FALSE, 0);

	gtk_widget_show_all (hbox);

	/* Set the options */

	e_dialog_spin_set (priv->recurrence_month_index_spin, priv->recurrence_month_index);
	e_dialog_option_menu_set (priv->recurrence_month_day_menu,
				  priv->recurrence_month_day,
				  month_day_options_map);
	adjust_day_index_spin (ee);

	gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
			    GTK_SIGNAL_FUNC (recur_month_index_value_changed_cb), ee);

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->recurrence_month_day_menu));
	gtk_signal_connect (GTK_OBJECT (menu), "selection_done",
			    GTK_SIGNAL_FUNC (month_day_menu_selection_done_cb), ee);
}

static const int recur_freq_map[] = {
	ICAL_DAILY_RECURRENCE,
	ICAL_WEEKLY_RECURRENCE,
	ICAL_MONTHLY_RECURRENCE,
	ICAL_YEARLY_RECURRENCE,
	-1
};

/* Changes the recurrence-special widget to match the interval units.
 *
 * For daily recurrences: nothing.
 * For weekly recurrences: weekday selector.
 * For monthly recurrences: "on the" <nth> [day, Weekday]
 * For yearly recurrences: nothing.
 */
static void
make_recurrence_special (EventEditor *ee)
{
	EventEditorPrivate *priv;
	icalrecurrencetype_frequency frequency;

	priv = ee->priv;

	if (GTK_BIN (priv->recurrence_special)->child != NULL) {
		gtk_widget_destroy (GTK_BIN (priv->recurrence_special)->child);

		priv->recurrence_weekday_picker = NULL;
		priv->recurrence_month_index_spin = NULL;
		priv->recurrence_month_day_menu = NULL;
	}

	frequency = e_dialog_option_menu_get (priv->recurrence_interval_unit, recur_freq_map);

	switch (frequency) {
	case ICAL_DAILY_RECURRENCE:
		gtk_widget_hide (priv->recurrence_special);
		break;

	case ICAL_WEEKLY_RECURRENCE:
		make_recur_weekly_special (ee);
		gtk_widget_show (priv->recurrence_special);
		break;

	case ICAL_MONTHLY_RECURRENCE:
		make_recur_monthly_special (ee);
		gtk_widget_show (priv->recurrence_special);
		break;

	case ICAL_YEARLY_RECURRENCE:
		gtk_widget_hide (priv->recurrence_special);
		break;

	default:
		g_assert_not_reached ();
	}
}

/* Callback used when the ending-until date editor changes */
static void
recur_ending_until_changed_cb (EDateEdit *de, gpointer data)
{
	EventEditor *ee;

	ee = EVENT_EDITOR (data);
	event_editor_set_changed (ee, TRUE);
	preview_recur (ee);
}

/* Creates the special contents for "ending until" (end date) recurrences */
static void
make_recur_ending_until_special (EventEditor *ee)
{
	EventEditorPrivate *priv;
	EDateEdit *de;

	priv = ee->priv;

	g_assert (GTK_BIN (priv->recurrence_ending_special)->child == NULL);
	g_assert (priv->recurrence_ending_date_edit == NULL);

	/* Create the widget */

	priv->recurrence_ending_date_edit = date_edit_new (TRUE, FALSE);
	de = E_DATE_EDIT (priv->recurrence_ending_date_edit);

	gtk_container_add (GTK_CONTAINER (priv->recurrence_ending_special), GTK_WIDGET (de));
	gtk_widget_show_all (GTK_WIDGET (de));

	/* Set the value */

	e_date_edit_set_time (de, priv->recurrence_ending_date);

	gtk_signal_connect (GTK_OBJECT (de), "changed",
			    GTK_SIGNAL_FUNC (recur_ending_until_changed_cb), ee);
}

/* Callback used when the ending-count value changes */
static void
recur_ending_count_value_changed_cb (GtkAdjustment *adj, gpointer data)
{
	EventEditor *ee;

	ee = EVENT_EDITOR (data);
	event_editor_set_changed (ee, TRUE);
	preview_recur (ee);
}

/* Creates the special contents for the occurrence count case */
static void
make_recur_ending_count_special (EventEditor *ee)
{
	EventEditorPrivate *priv;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkAdjustment *adj;

	priv = ee->priv;

	g_assert (GTK_BIN (priv->recurrence_ending_special)->child == NULL);
	g_assert (priv->recurrence_ending_count_spin == NULL);

	/* Create the widgets */

	hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (priv->recurrence_ending_special), hbox);

	adj = GTK_ADJUSTMENT (gtk_adjustment_new (1, 1, 10000, 1, 10, 10));
	priv->recurrence_ending_count_spin = gtk_spin_button_new (adj, 1, 0);
	gtk_box_pack_start (GTK_BOX (hbox), priv->recurrence_ending_count_spin, FALSE, FALSE, 0);

	label = gtk_label_new (_("occurrences"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	gtk_widget_show_all (hbox);

	/* Set the values */

	e_dialog_spin_set (priv->recurrence_ending_count_spin,
			   priv->recurrence_ending_count);

	gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
			    GTK_SIGNAL_FUNC (recur_ending_count_value_changed_cb), ee);
}

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

/* Changes the recurrence-ending-special widget to match the ending date option.
 *
 * For: <n> [days, weeks, months, years, occurrences]
 * Until: <date selector>
 * Forever: nothing.
 */
static void
make_recurrence_ending_special (EventEditor *ee)
{
	EventEditorPrivate *priv;
	enum ending_type ending_type;

	priv = ee->priv;

	if (GTK_BIN (priv->recurrence_ending_special)->child != NULL) {
		gtk_widget_destroy (GTK_BIN (priv->recurrence_ending_special)->child);

		priv->recurrence_ending_date_edit = NULL;
		priv->recurrence_ending_count_spin = NULL;
	}

	ending_type = e_dialog_option_menu_get (priv->recurrence_ending_menu, ending_types_map);

	switch (ending_type) {
	case ENDING_FOR:
		make_recur_ending_count_special (ee);
		gtk_widget_show (priv->recurrence_ending_special);
		break;

	case ENDING_UNTIL:
		make_recur_ending_until_special (ee);
		gtk_widget_show (priv->recurrence_ending_special);
		break;

	case ENDING_FOREVER:
		gtk_widget_hide (priv->recurrence_ending_special);
		break;

	default:
		g_assert_not_reached ();
	}
}

enum recur_type {
	RECUR_NONE,
	RECUR_SIMPLE,
	RECUR_CUSTOM
};

static const int recur_type_map[] = {
	RECUR_NONE,
	RECUR_SIMPLE,
	RECUR_CUSTOM,
	-1
};

/* Sensitizes the recurrence widgets based on the state of the recurrence type
 * radio group.
 */
static void
sensitize_recur_widgets (EventEditor *ee)
{
	EventEditorPrivate *priv;
	enum recur_type type;
	GtkWidget *label;

	priv = ee->priv;

	type = e_dialog_radio_get (priv->recurrence_none, recur_type_map);

	if (GTK_BIN (priv->recurrence_custom_warning_bin)->child)
		gtk_widget_destroy (GTK_BIN (priv->recurrence_custom_warning_bin)->child);

	switch (type) {
	case RECUR_NONE:
		gtk_widget_set_sensitive (priv->recurrence_params, FALSE);
		gtk_widget_show (priv->recurrence_params);
		gtk_widget_hide (priv->recurrence_custom_warning_bin);
		break;

	case RECUR_SIMPLE:
		gtk_widget_set_sensitive (priv->recurrence_params, TRUE);
		gtk_widget_show (priv->recurrence_params);
		gtk_widget_hide (priv->recurrence_custom_warning_bin);
		break;

	case RECUR_CUSTOM:
		gtk_widget_set_sensitive (priv->recurrence_params, FALSE);
		gtk_widget_hide (priv->recurrence_params);

		label = gtk_label_new (_("This appointment contains recurrences that Evolution "
					 "cannot edit."));
		gtk_container_add (GTK_CONTAINER (priv->recurrence_custom_warning_bin), label);
		gtk_widget_show_all (priv->recurrence_custom_warning_bin);
		break;

	default:
		g_assert_not_reached ();
	}
}

/* Callback used when one of the recurrence type radio buttons is toggled.  We
 * enable or the recurrence parameters.
 */
static void
recurrence_type_toggled_cb (GtkToggleButton *toggle, gpointer data)
{
	EventEditor *ee;

	ee = EVENT_EDITOR (data);

	event_editor_set_changed (ee, TRUE);

	if (toggle->active) {
		sensitize_recur_widgets (ee);
		preview_recur (ee);
	}
}

/* Callback used when the recurrence interval value spin button changes. */
static void
recur_interval_value_changed_cb (GtkAdjustment *adj, gpointer data)
{
	EventEditor *ee;

	ee = EVENT_EDITOR (data);
	event_editor_set_changed (ee, TRUE);
	preview_recur (ee);
}

/* Callback used when the recurrence interval option menu changes.  We need to
 * change the contents of the recurrence special widget.
 */
static void
recur_interval_selection_done_cb (GtkMenuShell *menu_shell, gpointer data)
{
	EventEditor *ee;

	ee = EVENT_EDITOR (data);
	event_editor_set_changed (ee, TRUE);
	make_recurrence_special (ee);
	preview_recur (ee);
}

/* Callback used when the recurrence ending option menu changes.  We need to
 * change the contents of the ending special widget.
 */
static void
recur_ending_selection_done_cb (GtkMenuShell *menu_shell, gpointer data)
{
	EventEditor *ee;

	ee = EVENT_EDITOR (data);
	event_editor_set_changed (ee, TRUE);
	make_recurrence_ending_special (ee);
	preview_recur (ee);
}

/* Gets the widgets from the XML file and returns if they are all available.
 * For the widgets whose values can be simply set with e-dialog-utils, it does
 * that as well.
 */
static gboolean
get_widgets (EventEditor *ee)
{
	EventEditorPrivate *priv;

	priv = ee->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	priv->app = GW ("event-editor-dialog");

	priv->general_summary = GW ("general-summary");

	priv->start_time = GW ("start-time");
	priv->end_time = GW ("end-time");
	priv->all_day_event = GW ("all-day-event");

	priv->description = GW ("description");

	priv->alarm_display = GW ("alarm-display");
	priv->alarm_program = GW ("alarm-program");
	priv->alarm_audio = GW ("alarm-audio");
	priv->alarm_mail = GW ("alarm-mail");
	priv->alarm_display_amount = GW ("alarm-display-amount");
	priv->alarm_display_unit = GW ("alarm-display-unit");
	priv->alarm_audio_amount = GW ("alarm-audio-amount");
	priv->alarm_audio_unit = GW ("alarm-audio-unit");
	priv->alarm_program_amount = GW ("alarm-program-amount");
	priv->alarm_program_unit = GW ("alarm-program-unit");
	priv->alarm_program_run_program = GW ("alarm-program-run-program");
	priv->alarm_program_run_program_entry = GW ("alarm-program-run-program-entry");
	priv->alarm_mail_amount = GW ("alarm-mail-amount");
	priv->alarm_mail_unit = GW ("alarm-mail-unit");
	priv->alarm_mail_mail_to = GW ("alarm-mail-mail-to");

	priv->classification_public = GW ("classification-public");
	priv->classification_private = GW ("classification-private");
	priv->classification_confidential = GW ("classification-confidential");

	priv->categories = GW ("categories");
	priv->categories_btn = GW ("categories-button");

	priv->recurrence_summary = GW ("recurrence-summary");
	priv->recurrence_starting_date = GW ("recurrence-starting-date");

	priv->recurrence_none = GW ("recurrence-none");
	priv->recurrence_simple = GW ("recurrence-simple");
	priv->recurrence_custom = GW ("recurrence-custom");
	priv->recurrence_params = GW ("recurrence-params");

	priv->recurrence_interval_value = GW ("recurrence-interval-value");
	priv->recurrence_interval_unit = GW ("recurrence-interval-unit");
	priv->recurrence_special = GW ("recurrence-special");
	priv->recurrence_ending_menu = GW ("recurrence-ending-menu");
	priv->recurrence_ending_special = GW ("recurrence-ending-special");
	priv->recurrence_custom_warning_bin = GW ("recurrence-custom-warning-bin");

	priv->recurrence_exception_date = GW ("recurrence-exception-date");
	priv->recurrence_exception_list = GW ("recurrence-exception-list");
	priv->recurrence_exception_add = GW ("recurrence-exception-add");
	priv->recurrence_exception_modify = GW ("recurrence-exception-modify");
	priv->recurrence_exception_delete = GW ("recurrence-exception-delete");

	priv->recurrence_preview_bin = GW ("recurrence-preview-bin");

#undef GW

	return (priv->general_summary
		&& priv->start_time
		&& priv->end_time
		&& priv->all_day_event
		&& priv->description
		&& priv->alarm_display
		&& priv->alarm_program
		&& priv->alarm_audio
		&& priv->alarm_mail
		&& priv->alarm_display_amount
		&& priv->alarm_display_unit
		&& priv->alarm_audio_amount
		&& priv->alarm_audio_unit
		&& priv->alarm_program_amount
		&& priv->alarm_program_unit
		&& priv->alarm_program_run_program
		&& priv->alarm_program_run_program_entry
		&& priv->alarm_mail_amount
		&& priv->alarm_mail_unit
		&& priv->alarm_mail_mail_to
		&& priv->classification_public
		&& priv->classification_private
		&& priv->classification_confidential
		&& priv->recurrence_summary
		&& priv->recurrence_starting_date
		&& priv->recurrence_none
		&& priv->recurrence_simple
		&& priv->recurrence_custom
		&& priv->recurrence_params
		&& priv->recurrence_interval_value
		&& priv->recurrence_interval_unit
		&& priv->recurrence_special
		&& priv->recurrence_ending_menu
		&& priv->recurrence_ending_special
		&& priv->recurrence_custom_warning_bin
		&& priv->recurrence_exception_date
		&& priv->recurrence_exception_list
		&& priv->recurrence_exception_add
		&& priv->recurrence_exception_modify
		&& priv->recurrence_exception_delete
		&& priv->recurrence_preview_bin);
}

/* Syncs the contents of two entry widgets, while blocking signals from each
 * other.
 */
static void
sync_entries (GtkEditable *source, GtkEditable *dest)
{
	char *str;

	gtk_signal_handler_block_by_data (GTK_OBJECT (dest), source);

	str = gtk_editable_get_chars (source, 0, -1);
	gtk_entry_set_text (GTK_ENTRY (dest), str);
	g_free (str);

	gtk_signal_handler_unblock_by_data (GTK_OBJECT (dest), source);
}

/* Syncs the contents of two date editor widgets, while blocking signals on the
 * specified data.
 */
static void
sync_date_edits (EDateEdit *source, EDateEdit *dest)
{
	time_t t;

	gtk_signal_handler_block_by_data (GTK_OBJECT (dest), source);

	t = e_date_edit_get_time (source);
	e_date_edit_set_time (dest, t);

	gtk_signal_handler_unblock_by_data (GTK_OBJECT (dest), source);
}

/* Callback used when one of the general or recurrence summary entries change;
 * we sync the other entry to it.
 */
static void
summary_changed_cb (GtkEditable *editable, gpointer data)
{
	sync_entries (editable, GTK_EDITABLE (data));
}

/* Callback used when one of the general or recurrence starting date widgets
 * change; we sync the other date editor to it.
 */
static void
start_date_changed_cb (EDateEdit *de, gpointer data)
{
	sync_date_edits (de, E_DATE_EDIT (data));
}

/* Callback used when the displayed date range in the recurrence preview
 * calendar changes.
 */
static void
recur_preview_date_range_changed_cb (ECalendarItem *item, gpointer data)
{
	EventEditor *ee;

	ee = EVENT_EDITOR (data);
	preview_recur (ee);
}

/* Hooks the widget signals */
static void
init_widgets (EventEditor *ee)
{
	EventEditorPrivate *priv;
	GtkWidget *menu;
	GtkAdjustment *adj;
	ECalendar *ecal;

	priv = ee->priv;

	/* Summary in the main and recurrence pages */

	gtk_signal_connect (GTK_OBJECT (priv->general_summary), "changed",
			    GTK_SIGNAL_FUNC (summary_changed_cb), priv->recurrence_summary);
	gtk_signal_connect (GTK_OBJECT (priv->recurrence_summary), "changed",
			    GTK_SIGNAL_FUNC (summary_changed_cb), priv->general_summary);

	/* Categories button */
	gtk_signal_connect (GTK_OBJECT (priv->categories_btn), "clicked",
			    GTK_SIGNAL_FUNC (categories_clicked), ee);

	/* Start dates in the main and recurrence pages */

	gtk_signal_connect (GTK_OBJECT (priv->start_time), "changed",
			    GTK_SIGNAL_FUNC (start_date_changed_cb), priv->recurrence_starting_date);
	gtk_signal_connect (GTK_OBJECT (priv->recurrence_starting_date), "changed",
			    GTK_SIGNAL_FUNC (start_date_changed_cb), priv->start_time);

	/* Start and end times */

	gtk_signal_connect (GTK_OBJECT (priv->start_time), "changed",
			    GTK_SIGNAL_FUNC (date_changed_cb), ee);
	gtk_signal_connect (GTK_OBJECT (priv->end_time), "changed",
			    GTK_SIGNAL_FUNC (date_changed_cb), ee);

	gtk_signal_connect (GTK_OBJECT (priv->all_day_event), "toggled",
			    GTK_SIGNAL_FUNC (set_all_day), ee);

	/* Alarms */

	gtk_signal_connect (GTK_OBJECT (priv->alarm_display), "toggled",
			    GTK_SIGNAL_FUNC (alarm_toggle), ee);
	gtk_signal_connect (GTK_OBJECT (priv->alarm_program), "toggled",
			    GTK_SIGNAL_FUNC (alarm_toggle), ee);
	gtk_signal_connect (GTK_OBJECT (priv->alarm_audio), "toggled",
			    GTK_SIGNAL_FUNC (alarm_toggle), ee);
	gtk_signal_connect (GTK_OBJECT (priv->alarm_mail), "toggled",
			    GTK_SIGNAL_FUNC (alarm_toggle), ee);

	/* Recurrence preview */

	priv->recurrence_preview_calendar = e_calendar_new ();
	ecal = E_CALENDAR (priv->recurrence_preview_calendar);
	gtk_signal_connect (GTK_OBJECT (ecal->calitem), "date_range_changed",
			    GTK_SIGNAL_FUNC (recur_preview_date_range_changed_cb), ee);
	calendar_config_configure_e_calendar (ecal);
	gnome_canvas_item_set (GNOME_CANVAS_ITEM (ecal->calitem),
			       "maximum_days_selected", 0,
			       NULL);
	gtk_container_add (GTK_CONTAINER (priv->recurrence_preview_bin),
			   priv->recurrence_preview_calendar);
	gtk_widget_show (priv->recurrence_preview_calendar);

	/* Recurrence types */

	gtk_signal_connect (GTK_OBJECT (priv->recurrence_none), "toggled",
			    GTK_SIGNAL_FUNC (recurrence_type_toggled_cb), ee);
	gtk_signal_connect (GTK_OBJECT (priv->recurrence_simple), "toggled",
			    GTK_SIGNAL_FUNC (recurrence_type_toggled_cb), ee);
	gtk_signal_connect (GTK_OBJECT (priv->recurrence_custom), "toggled",
			    GTK_SIGNAL_FUNC (recurrence_type_toggled_cb), ee);

	/* Recurrence interval */

	adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->recurrence_interval_value));
	gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
			    GTK_SIGNAL_FUNC (recur_interval_value_changed_cb), ee);

	/* Recurrence units */

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->recurrence_interval_unit));
	gtk_signal_connect (GTK_OBJECT (menu), "selection_done",
			    GTK_SIGNAL_FUNC (recur_interval_selection_done_cb), ee);

	/* Recurrence ending */

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->recurrence_ending_menu));
	gtk_signal_connect (GTK_OBJECT (menu), "selection_done",
			    GTK_SIGNAL_FUNC (recur_ending_selection_done_cb), ee);

	/* Exception buttons */

	gtk_signal_connect (GTK_OBJECT (priv->recurrence_exception_add), "clicked",
			    GTK_SIGNAL_FUNC (recurrence_exception_add_cb), ee);
	gtk_signal_connect (GTK_OBJECT (priv->recurrence_exception_modify), "clicked",
			    GTK_SIGNAL_FUNC (recurrence_exception_modify_cb), ee);
	gtk_signal_connect (GTK_OBJECT (priv->recurrence_exception_delete), "clicked",
			    GTK_SIGNAL_FUNC (recurrence_exception_delete_cb), ee);


	/*
	 * Connect the default signal handler to use to make sure the "changed"
	 * field gets set whenever a field is changed.
	 */

	/* General Page. */
	gtk_signal_connect (GTK_OBJECT (priv->general_summary), "changed",
			    GTK_SIGNAL_FUNC (field_changed), ee);
	gtk_signal_connect (GTK_OBJECT (priv->description), "changed",
			    GTK_SIGNAL_FUNC (field_changed), ee);
	gtk_signal_connect (GTK_OBJECT (priv->classification_public),
			    "toggled",
			    GTK_SIGNAL_FUNC (field_changed), ee);
	gtk_signal_connect (GTK_OBJECT (priv->classification_private),
			    "toggled",
			    GTK_SIGNAL_FUNC (field_changed), ee);
	gtk_signal_connect (GTK_OBJECT (priv->classification_confidential),
			    "toggled",
			    GTK_SIGNAL_FUNC (field_changed), ee);

	/* Reminder Page. */
	gtk_signal_connect (GTK_OBJECT (GTK_SPIN_BUTTON (priv->alarm_display_amount)->adjustment),
			    "value_changed",
			    GTK_SIGNAL_FUNC (field_changed), ee);
	gtk_signal_connect (GTK_OBJECT (GTK_SPIN_BUTTON (priv->alarm_audio_amount)->adjustment),
			    "value_changed",
			    GTK_SIGNAL_FUNC (field_changed), ee);
	gtk_signal_connect (GTK_OBJECT (GTK_SPIN_BUTTON (priv->alarm_program_amount)->adjustment),
			    "value_changed",
			    GTK_SIGNAL_FUNC (field_changed), ee);
	gtk_signal_connect (GTK_OBJECT (GTK_SPIN_BUTTON (priv->alarm_mail_amount)->adjustment),
			    "value_changed",
			    GTK_SIGNAL_FUNC (field_changed), ee);

	gtk_signal_connect (GTK_OBJECT (GTK_OPTION_MENU (priv->alarm_display_unit)->menu),
			    "deactivate",
			    GTK_SIGNAL_FUNC (field_changed), ee);
	gtk_signal_connect (GTK_OBJECT (GTK_OPTION_MENU (priv->alarm_audio_unit)->menu),
			    "deactivate",
			    GTK_SIGNAL_FUNC (field_changed), ee);
	gtk_signal_connect (GTK_OBJECT (GTK_OPTION_MENU (priv->alarm_program_unit)->menu),
			    "deactivate",
			    GTK_SIGNAL_FUNC (field_changed), ee);
	gtk_signal_connect (GTK_OBJECT (GTK_OPTION_MENU (priv->alarm_mail_unit)->menu),
			    "deactivate",
			    GTK_SIGNAL_FUNC (field_changed), ee);

	gtk_signal_connect (GTK_OBJECT (priv->alarm_program_run_program_entry),
			    "changed",
			    GTK_SIGNAL_FUNC (field_changed), ee);
	gtk_signal_connect (GTK_OBJECT (priv->alarm_mail_mail_to),
			    "changed",
			    GTK_SIGNAL_FUNC (field_changed), ee);


	/* Recurrence Page. */
}

static const int classification_map[] = {
	CAL_COMPONENT_CLASS_PUBLIC,
	CAL_COMPONENT_CLASS_PRIVATE,
	CAL_COMPONENT_CLASS_CONFIDENTIAL,
	-1
};

#if 0
static const int alarm_unit_map[] = {
	ALARM_MINUTES,
	ALARM_HOURS,
	ALARM_DAYS,
	-1
};

static void
alarm_unit_set (GtkWidget *widget, enum AlarmUnit unit)
{
	e_dialog_option_menu_set (widget, unit, alarm_unit_map);
}

static enum AlarmUnit
alarm_unit_get (GtkWidget *widget)
{
	return e_dialog_option_menu_get (widget, alarm_unit_map);
}
#endif

static const int month_pos_map[] = { 0, 1, 2, 3, 4, -1 };
static const int weekday_map[] = { 0, 1, 2, 3, 4, 5, 6, -1 };

/* Fills the widgets with default values */
static void
clear_widgets (EventEditor *ee)
{
	EventEditorPrivate *priv;
	time_t now;
	GtkAdjustment *adj;
	GtkWidget *menu;

	priv = ee->priv;

	now = time (NULL);

	/* Summary, description */

	e_dialog_editable_set (priv->general_summary, NULL); /* will also change recur summary */
	e_dialog_editable_set (priv->description, NULL);

	/* Start and end times */

	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->start_time), ee);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->end_time), ee);

	e_date_edit_set_time (E_DATE_EDIT (priv->start_time), now); /* will set recur start too */
	e_date_edit_set_time (E_DATE_EDIT (priv->end_time), now);

	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->start_time), ee);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->end_time), ee);

	check_all_day (ee);

	/* Alarms */

	/* FIXME: these should use configurable defaults */

	e_dialog_toggle_set (priv->alarm_display, FALSE);
	e_dialog_toggle_set (priv->alarm_program, FALSE);
	e_dialog_toggle_set (priv->alarm_audio, FALSE);
	e_dialog_toggle_set (priv->alarm_mail, FALSE);

	e_dialog_spin_set (priv->alarm_display_amount, 15);
	e_dialog_spin_set (priv->alarm_audio_amount, 15);
	e_dialog_spin_set (priv->alarm_program_amount, 15);
	e_dialog_spin_set (priv->alarm_mail_amount, 15);

#if 0
	alarm_unit_set (priv->alarm_display_unit, ALARM_MINUTES);
	alarm_unit_set (priv->alarm_audio_unit, ALARM_MINUTES);
	alarm_unit_set (priv->alarm_program_unit, ALARM_MINUTES);
	alarm_unit_set (priv->alarm_mail_unit, ALARM_MINUTES);
#endif

	e_dialog_editable_set (priv->alarm_program_run_program_entry, NULL);
	e_dialog_editable_set (priv->alarm_mail_mail_to, NULL);

	/* Classification */

	e_dialog_radio_set (priv->classification_public,
			    CAL_COMPONENT_CLASS_PRIVATE, classification_map);

	/* Recurrences */

	priv->recurrence_weekday_day_mask = 0;

	priv->recurrence_month_index = 1;
	priv->recurrence_month_day = MONTH_DAY_NTH;

	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->recurrence_none), ee);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->recurrence_simple), ee);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->recurrence_custom), ee);
	e_dialog_radio_set (priv->recurrence_none, RECUR_NONE, recur_type_map);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->recurrence_none), ee);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->recurrence_simple), ee);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->recurrence_custom), ee);

	adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->recurrence_interval_value));
	gtk_signal_handler_block_by_data (GTK_OBJECT (adj), ee);
	e_dialog_spin_set (priv->recurrence_interval_value, 1);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (adj), ee);

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->recurrence_interval_unit));
	gtk_signal_handler_block_by_data (GTK_OBJECT (menu), ee);
	e_dialog_option_menu_set (priv->recurrence_interval_unit, ICAL_DAILY_RECURRENCE,
				  recur_freq_map);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (menu), ee);

	priv->recurrence_ending_date = time (NULL);
	priv->recurrence_ending_count = 1;

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->recurrence_ending_menu));
	gtk_signal_handler_block_by_data (GTK_OBJECT (menu), ee);
	e_dialog_option_menu_set (priv->recurrence_ending_menu, ENDING_FOREVER,
				  ending_types_map);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (menu), ee);

	/* Exceptions list */

	free_exception_clist_data (GTK_CLIST (priv->recurrence_exception_list));
}

/* Fills the recurrence ending date widgets with the values from the calendar
 * component.
 */
static void
fill_ending_date (EventEditor *ee, struct icalrecurrencetype *r)
{
	EventEditorPrivate *priv;
	GtkWidget *menu;

	priv = ee->priv;

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->recurrence_ending_menu));
	gtk_signal_handler_block_by_data (GTK_OBJECT (menu), ee);

	if (r->count == 0) {
		if (r->until.year == 0) {
			/* Forever */

			e_dialog_option_menu_set (priv->recurrence_ending_menu,
						  ENDING_FOREVER,
						  ending_types_map);
		} else {
			/* Ending date */

			priv->recurrence_ending_date = icaltime_as_timet (r->until);
			e_dialog_option_menu_set (priv->recurrence_ending_menu,
						  ENDING_UNTIL,
						  ending_types_map);
		}
	} else {
		/* Count of occurrences */

		priv->recurrence_ending_count = r->count;
		e_dialog_option_menu_set (priv->recurrence_ending_menu,
					  ENDING_FOR,
					  ending_types_map);
	}

	gtk_signal_handler_unblock_by_data (GTK_OBJECT (menu), ee);

	make_recurrence_ending_special (ee);
}

/* Counts the number of elements in the by_xxx fields of an icalrecurrencetype */
static int
count_by_xxx (short *field, int max_elements)
{
	int i;

	for (i = 0; i < max_elements; i++)
		if (field[i] == ICAL_RECURRENCE_ARRAY_MAX)
			break;

	return i;
}

/* Re-tags the recurrence preview calendar based on the current information of
 * the event editor.
 */
static void
preview_recur (EventEditor *ee)
{
	EventEditorPrivate *priv;
	CalComponent *comp;
	CalComponentDateTime cdt;
	GSList *l;

	priv = ee->priv;
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

	recur_to_comp_object (ee, comp);

	tag_calendar_by_comp (E_CALENDAR (priv->recurrence_preview_calendar), comp);
	gtk_object_unref (GTK_OBJECT (comp));
}

/* Fills in the exception widgets with the data from the calendar component */
static void
fill_exception_widgets (EventEditor *ee)
{
	EventEditorPrivate *priv;
	GSList *list, *l;

	priv = ee->priv;
	g_assert (priv->comp != NULL);

	/* Exceptions list */

	cal_component_get_exdate_list (priv->comp, &list);

	for (l = list; l; l = l->next) {
		CalComponentDateTime *cdt;
		time_t ext;

		cdt = l->data;
		ext = icaltime_as_timet (*cdt->value);
		append_exception (ee, ext);
	}

	cal_component_free_exdate_list (list);
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
set_recur_special_defaults (EventEditor *ee)
{
	EventEditorPrivate *priv;
	guint8 mask;

	priv = ee->priv;

	mask = get_start_weekday_mask (priv->comp);

	priv->recurrence_weekday_day_mask = mask;
	priv->recurrence_weekday_blocked_day_mask = mask;
}

/* Fills in the recurrence widgets with the values from the calendar component.
 * This function is particularly tricky because it has to discriminate between
 * recurrences we support for editing and the ones we don't.  We only support at
 * most one recurrence rule; no rdates or exrules (exdates are handled just fine
 * elsewhere).
 */
static void
fill_recurrence_widgets (EventEditor *ee)
{
	EventEditorPrivate *priv;
	GSList *rrule_list;
	int len;
	struct icalrecurrencetype *r;
	int n_by_second, n_by_minute, n_by_hour;
	int n_by_day, n_by_month_day, n_by_year_day;
	int n_by_week_no, n_by_month, n_by_set_pos;
	GtkWidget *menu;
	GtkAdjustment *adj;

	priv = ee->priv;
	g_assert (priv->comp != NULL);

	fill_exception_widgets (ee);

	/* Set up defaults for the special widgets */

	set_recur_special_defaults (ee);

	/* No recurrences? */

	if (!cal_component_has_rdates (priv->comp)
	    && !cal_component_has_rrules (priv->comp)
	    && !cal_component_has_exrules (priv->comp)) {
		gtk_signal_handler_block_by_data (GTK_OBJECT (priv->recurrence_none), ee);
		gtk_signal_handler_block_by_data (GTK_OBJECT (priv->recurrence_simple), ee);
		gtk_signal_handler_block_by_data (GTK_OBJECT (priv->recurrence_custom), ee);
		e_dialog_radio_set (priv->recurrence_none, RECUR_NONE, recur_type_map);
		gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->recurrence_none), ee);
		gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->recurrence_simple), ee);
		gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->recurrence_custom), ee);

		gtk_widget_set_sensitive (priv->recurrence_custom, FALSE);

		sensitize_recur_widgets (ee);
		preview_recur (ee);
		return;
	}

	/* See if it is a custom set we don't support */

	cal_component_get_rrule_list (priv->comp, &rrule_list);
	len = g_slist_length (rrule_list);
	if (len > 1
	    || cal_component_has_rdates (priv->comp)
	    || cal_component_has_exrules (priv->comp))
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

		menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->recurrence_interval_unit));
		gtk_signal_handler_block_by_data (GTK_OBJECT (menu), ee);
		e_dialog_option_menu_set (priv->recurrence_interval_unit, ICAL_DAILY_RECURRENCE,
					  recur_freq_map);
		gtk_signal_handler_unblock_by_data (GTK_OBJECT (menu), ee);
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

		priv->recurrence_weekday_day_mask = day_mask;

		menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->recurrence_interval_unit));
		gtk_signal_handler_block_by_data (GTK_OBJECT (menu), ee);
		e_dialog_option_menu_set (priv->recurrence_interval_unit, ICAL_WEEKLY_RECURRENCE,
					  recur_freq_map);
		gtk_signal_handler_unblock_by_data (GTK_OBJECT (menu), ee);
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

			priv->recurrence_month_index = nth;
			priv->recurrence_month_day = MONTH_DAY_NTH;
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

			priv->recurrence_month_index = pos;
			priv->recurrence_month_day = month_day;
		} else
			goto custom;

		menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->recurrence_interval_unit));
		gtk_signal_handler_block_by_data (GTK_OBJECT (menu), ee);
		e_dialog_option_menu_set (priv->recurrence_interval_unit, ICAL_MONTHLY_RECURRENCE,
					  recur_freq_map);
		gtk_signal_handler_unblock_by_data (GTK_OBJECT (menu), ee);
		break;

	case ICAL_YEARLY_RECURRENCE:
		if (n_by_day != 0
		    || n_by_month_day != 0
		    || n_by_year_day != 0
		    || n_by_week_no != 0
		    || n_by_month != 0
		    || n_by_set_pos != 0)
			goto custom;

		menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->recurrence_interval_unit));
		gtk_signal_handler_block_by_data (GTK_OBJECT (menu), ee);
		e_dialog_option_menu_set (priv->recurrence_interval_unit, ICAL_YEARLY_RECURRENCE,
					  recur_freq_map);
		gtk_signal_handler_unblock_by_data (GTK_OBJECT (menu), ee);
		break;

	default:
		goto custom;
	}

	/* If we got here it means it is a simple recurrence */

	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->recurrence_none), ee);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->recurrence_simple), ee);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->recurrence_custom), ee);
	e_dialog_radio_set (priv->recurrence_simple, RECUR_SIMPLE, recur_type_map);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->recurrence_none), ee);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->recurrence_simple), ee);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->recurrence_custom), ee);

	gtk_widget_set_sensitive (priv->recurrence_custom, FALSE);

	sensitize_recur_widgets (ee);
	make_recurrence_special (ee);

	adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->recurrence_interval_value));
	gtk_signal_handler_block_by_data (GTK_OBJECT (adj), ee);
	e_dialog_spin_set (priv->recurrence_interval_value, r->interval);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (adj), ee);

	fill_ending_date (ee, r);

	goto out;

 custom:

	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->recurrence_none), ee);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->recurrence_simple), ee);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->recurrence_custom), ee);
	e_dialog_radio_set (priv->recurrence_custom, RECUR_CUSTOM, recur_type_map);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->recurrence_none), ee);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->recurrence_simple), ee);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->recurrence_custom), ee);

	gtk_widget_set_sensitive (priv->recurrence_custom, TRUE);
	sensitize_recur_widgets (ee);

 out:

	cal_component_free_recur_list (rrule_list);
	preview_recur (ee);
}

/* Fills in the widgets with the value from the calendar component */
static void
fill_widgets (EventEditor *ee)
{
	EventEditorPrivate *priv;
	CalComponentText text;
	CalComponentClassification cl;
	CalComponentDateTime d;
	GSList *l;
	time_t dtstart, dtend;
	const char *categories;

	priv = ee->priv;

	clear_widgets (ee);

	if (!priv->comp)
		return;

	/* Summary, description(s) */

	cal_component_get_summary (priv->comp, &text);
	e_dialog_editable_set (priv->general_summary, text.value); /* will also set recur summary */

	cal_component_get_description_list (priv->comp, &l);
	if (l) {
		text = *(CalComponentText *)l->data;
		e_dialog_editable_set (priv->description, text.value);
	}
	cal_component_free_text_list (l);

	/* Start and end times */

	/* All-day events are inclusive, i.e. if the end date shown is 2nd Feb
	   then the event includes all of the 2nd Feb. We would normally show
	   3rd Feb as the end date, since it really ends at midnight on 3rd,
	   so we have to subtract a day so we only show the 2nd. */
	cal_component_get_dtstart (priv->comp, &d);
	dtstart = icaltime_as_timet (*d.value);
	cal_component_free_datetime (&d);

	cal_component_get_dtend (priv->comp, &d);
	dtend = icaltime_as_timet (*d.value);
	cal_component_free_datetime (&d);

	if (time_day_begin (dtstart) == dtstart
	    && time_day_begin (dtend) == dtend) {
		dtend = time_add_day (dtend, -1);
	}

	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->start_time), ee);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->end_time), ee);

	e_date_edit_set_time (E_DATE_EDIT (priv->start_time), dtstart); /* will set recur start too */
	e_date_edit_set_time (E_DATE_EDIT (priv->end_time), dtend);

	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->start_time), ee);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->end_time), ee);

	check_all_day (ee);

	/* Alarms */
#if 0
	e_dialog_toggle_set (priv->alarm_display, priv->ico->dalarm.enabled);
	e_dialog_toggle_set (priv->alarm_program, priv->ico->palarm.enabled);
	e_dialog_toggle_set (priv->alarm_audio, priv->ico->aalarm.enabled);
	e_dialog_toggle_set (priv->alarm_mail, priv->ico->malarm.enabled);

	/* Alarm data */

	e_dialog_spin_set (priv->alarm_display_amount, priv->ico->dalarm.count);
	e_dialog_spin_set (priv->alarm_audio_amount, priv->ico->aalarm.count);
	e_dialog_spin_set (priv->alarm_program_amount, priv->ico->palarm.count);
	e_dialog_spin_set (priv->alarm_mail_amount, priv->ico->malarm.count);

	alarm_unit_set (priv->alarm_display_unit, priv->ico->dalarm.units);
	alarm_unit_set (priv->alarm_audio_unit, priv->ico->aalarm.units);
	alarm_unit_set (priv->alarm_program_unit, priv->ico->palarm.units);
	alarm_unit_set (priv->alarm_mail_unit, priv->ico->malarm.units);

	e_dialog_editable_set (priv->alarm_program_run_program_entry, priv->ico->palarm.data);
	e_dialog_editable_set (priv->alarm_mail_mail_to, priv->ico->malarm.data);
#endif

	/* Call alarm_toggle to set the sensitivity of the widgets. */
	alarm_toggle (priv->alarm_display, ee);
	alarm_toggle (priv->alarm_audio, ee);
	alarm_toggle (priv->alarm_program, ee);
	alarm_toggle (priv->alarm_mail, ee);


	/* Classification */

	cal_component_get_classification (priv->comp, &cl);

	switch (cl) {
	case CAL_COMPONENT_CLASS_PUBLIC:
	    	e_dialog_radio_set (priv->classification_public, CAL_COMPONENT_CLASS_PUBLIC,
				    classification_map);
	case CAL_COMPONENT_CLASS_PRIVATE:
	    	e_dialog_radio_set (priv->classification_public, CAL_COMPONENT_CLASS_PRIVATE,
				    classification_map);
	case CAL_COMPONENT_CLASS_CONFIDENTIAL:
	    	e_dialog_radio_set (priv->classification_public, CAL_COMPONENT_CLASS_CONFIDENTIAL,
				    classification_map);
	default:
		/* What do do?  We can't g_assert_not_reached() since it is a
		 * value from an external file.
		 */
	}

	/* Categories */
	cal_component_get_categories (priv->comp, &categories);
	e_dialog_editable_set (priv->categories, categories);

	/* Recurrences */
	fill_recurrence_widgets (ee);

	/* Do this last, since the callbacks will set it to TRUE. */
	event_editor_set_changed (ee, FALSE);
}


/**
 * event_editor_update_widgets:
 * @ee: An event editor.
 *
 * Causes an event editor dialog to re-read the values of its calendar component
 * object.  This function should be used if the #CalComponent is changed by
 * external means while it is open in the editor.
 **/
void
event_editor_update_widgets (EventEditor *ee)
{
	g_return_if_fail (ee != NULL);
	g_return_if_fail (IS_EVENT_EDITOR (ee));

	fill_widgets (ee);
}



/* Decode the radio button group for classifications */
static CalComponentClassification
classification_get (GtkWidget *widget)
{
	return e_dialog_radio_get (widget, classification_map);
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
 * the calendar component object.
 */
static void
simple_recur_to_comp_object (EventEditor *ee, CalComponent *comp)
{
	EventEditorPrivate *priv;
	struct icalrecurrencetype r;
	GSList l;
	enum ending_type ending_type;

	priv = ee->priv;

	icalrecurrencetype_clear (&r);

	/* Frequency, interval, week start */

	r.freq = e_dialog_option_menu_get (priv->recurrence_interval_unit, recur_freq_map);
	r.interval = e_dialog_spin_get_int (priv->recurrence_interval_value);
	r.week_start = ICAL_SUNDAY_WEEKDAY + calendar_config_get_week_start_day ();

	/* Frequency-specific data */

	switch (r.freq) {
	case ICAL_DAILY_RECURRENCE:
		/* Nothing else is required */
		break;

	case ICAL_WEEKLY_RECURRENCE: {
		guint8 day_mask;
		int i;

		g_assert (GTK_BIN (priv->recurrence_special)->child != NULL);
		g_assert (priv->recurrence_weekday_picker != NULL);
		g_assert (IS_WEEKDAY_PICKER (priv->recurrence_weekday_picker));

		day_mask = weekday_picker_get_days (WEEKDAY_PICKER (priv->recurrence_weekday_picker));

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

		g_assert (GTK_BIN (priv->recurrence_special)->child != NULL);
		g_assert (priv->recurrence_month_index_spin != NULL);
		g_assert (GTK_IS_SPIN_BUTTON (priv->recurrence_month_index_spin));
		g_assert (priv->recurrence_month_day_menu != NULL);
		g_assert (GTK_IS_OPTION_MENU (priv->recurrence_month_day_menu));

		day_index = e_dialog_spin_get_int (priv->recurrence_month_index_spin);
		month_day = e_dialog_option_menu_get (priv->recurrence_month_day_menu,
						      month_day_options_map);

		switch (month_day) {
		case MONTH_DAY_NTH:
			r.by_month_day[0] = day_index;
			break;

		case MONTH_DAY_MON:
			r.by_day[0] = nth_weekday (day_index, ICAL_MONDAY_WEEKDAY);
			break;

		case MONTH_DAY_TUE:
			r.by_day[0] = nth_weekday (day_index, ICAL_TUESDAY_WEEKDAY);
			break;

		case MONTH_DAY_WED:
			r.by_day[0] = nth_weekday (day_index, ICAL_WEDNESDAY_WEEKDAY);
			break;

		case MONTH_DAY_THU:
			r.by_day[0] = nth_weekday (day_index, ICAL_THURSDAY_WEEKDAY);
			break;

		case MONTH_DAY_FRI:
			r.by_day[0] = nth_weekday (day_index, ICAL_FRIDAY_WEEKDAY);
			break;

		case MONTH_DAY_SAT:
			r.by_day[0] = nth_weekday (day_index, ICAL_SATURDAY_WEEKDAY);
			break;

		case MONTH_DAY_SUN:
			r.by_day[0] = nth_weekday (day_index, ICAL_SUNDAY_WEEKDAY);
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

	ending_type = e_dialog_option_menu_get (priv->recurrence_ending_menu, ending_types_map);

	switch (ending_type) {
	case ENDING_FOR:
		g_assert (priv->recurrence_ending_count_spin != NULL);
		g_assert (GTK_IS_SPIN_BUTTON (priv->recurrence_ending_count_spin));

		r.count = e_dialog_spin_get_int (priv->recurrence_ending_count_spin);
		break;

	case ENDING_UNTIL:
		g_assert (priv->recurrence_ending_date_edit != NULL);
		g_assert (E_IS_DATE_EDIT (priv->recurrence_ending_date_edit));

		r.until = icaltime_from_timet (
			e_date_edit_get_time (E_DATE_EDIT (priv->recurrence_ending_date_edit)),
			TRUE, TRUE);
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

/* Gets the data from the recurrence widgets and stores it in the calendar
 * component object.
 */
static void
recur_to_comp_object (EventEditor *ee, CalComponent *comp)
{
	EventEditorPrivate *priv;
	enum recur_type recur_type;
	GtkCList *exception_list;
	GSList *list;
	int i;

	priv = ee->priv;

	recur_type = e_dialog_radio_get (priv->recurrence_none, recur_type_map);

	switch (recur_type) {
	case RECUR_NONE:
		cal_component_set_rdate_list (comp, NULL);
		cal_component_set_rrule_list (comp, NULL);
		cal_component_set_exrule_list (comp, NULL);
		break;

	case RECUR_SIMPLE:
		cal_component_set_rdate_list (comp, NULL);
		cal_component_set_exrule_list (comp, NULL);
		simple_recur_to_comp_object (ee, comp);
		break;

	case RECUR_CUSTOM:
		/* We just keep whatever the component has currently */
		break;

	default:
		g_assert_not_reached ();
	}

	/* Set exceptions */

	list = NULL;
	exception_list = GTK_CLIST (priv->recurrence_exception_list);
	for (i = 0; i < exception_list->rows; i++) {
		CalComponentDateTime *cdt;
		time_t *tim;

		cdt = g_new (CalComponentDateTime, 1);
		cdt->value = g_new (struct icaltimetype, 1);
		cdt->tzid = NULL;

		tim = gtk_clist_get_row_data (exception_list, i);
		*cdt->value = icaltime_from_timet (*tim, FALSE, TRUE);

		list = g_slist_prepend (list, cdt);
	}

	cal_component_set_exdate_list (comp, list);
	cal_component_free_exdate_list (list);
}

/* Gets the data from the widgets and stores it in the calendar component object */
static void
dialog_to_comp_object (EventEditor *ee, CalComponent *comp)
{
	EventEditorPrivate *priv;
	CalComponentDateTime date;
	time_t t;
	gboolean all_day_event;
	char *cat, *str;

	priv = ee->priv;

	/* Summary */

	str = e_dialog_editable_get (priv->general_summary);
	if (!str || strlen (str) == 0)
		cal_component_set_summary (comp, NULL);
	else {
		CalComponentText text;

		text.value = str;
		text.altrep = NULL;

		cal_component_set_summary (comp, &text);
	}

	if (str)
		g_free (str);

	/* Description */

	str = e_dialog_editable_get (priv->description);
	if (!str || strlen (str) == 0)
		cal_component_set_description_list (comp, NULL);
	else {
		GSList l;
		CalComponentText text;

		text.value = str;
		text.altrep = NULL;
		l.data = &text;
		l.next = NULL;

		cal_component_set_description_list (comp, &l);
	}

	if (!str)
		g_free (str);

	/* Dates */

	date.value = g_new (struct icaltimetype, 1);
	date.tzid = NULL;

	t = e_date_edit_get_time (E_DATE_EDIT (priv->start_time));
	if (t != -1) {
		*date.value = icaltime_from_timet (t, FALSE, TRUE);
		cal_component_set_dtstart (comp, &date);
	} else {
		/* FIXME: What do we do here? */
	}

	/* If the all_day toggle is set, the end date is inclusive of the
	   entire day on which it points to. */
	all_day_event = e_dialog_toggle_get (priv->all_day_event);
	t = e_date_edit_get_time (E_DATE_EDIT (priv->end_time));
	if (t != -1) {
		if (all_day_event)
			t = time_day_end (t);

		*date.value = icaltime_from_timet (t, FALSE, TRUE);
		cal_component_set_dtend (comp, &date);
	} else {
		/* FIXME: What do we do here? */
	}
	g_free (date.value);

	/* Categories */
	cat = e_dialog_editable_get (priv->categories);
	cal_component_set_categories (comp, cat);

	if (cat)
		g_free (cat);

#if 0
	ico->dalarm.enabled = e_dialog_toggle_get (priv->alarm_display);
	ico->aalarm.enabled = e_dialog_toggle_get (priv->alarm_program);
	ico->palarm.enabled = e_dialog_toggle_get (priv->alarm_audio);
	ico->malarm.enabled = e_dialog_toggle_get (priv->alarm_mail);

	ico->dalarm.count = e_dialog_spin_get_int (priv->alarm_display_amount);
	ico->aalarm.count = e_dialog_spin_get_int (priv->alarm_audio_amount);
	ico->palarm.count = e_dialog_spin_get_int (priv->alarm_program_amount);
	ico->malarm.count = e_dialog_spin_get_int (priv->alarm_mail_amount);

	ico->dalarm.units = alarm_unit_get (priv->alarm_display_unit);
	ico->aalarm.units = alarm_unit_get (priv->alarm_audio_unit);
	ico->palarm.units = alarm_unit_get (priv->alarm_program_unit);
	ico->malarm.units = alarm_unit_get (priv->alarm_mail_unit);

	if (ico->palarm.data)
		g_free (ico->palarm.data);

	if (ico->malarm.data)
		g_free (ico->malarm.data);

	ico->palarm.data = e_dialog_editable_get (priv->alarm_program_run_program_entry);
	ico->malarm.data = e_dialog_editable_get (priv->alarm_mail_mail_to);
#endif

	cal_component_set_classification (comp, classification_get (priv->classification_public));

	/* Recurrence information */
	recur_to_comp_object (ee, comp);

	cal_component_commit_sequence (comp);
}

/* Fills the calendar component object from the data in the widgets and commits
 * the component to the storage.
 */
static void
save_event_object (EventEditor *ee)
{
	EventEditorPrivate *priv;

	priv = ee->priv;

	if (!priv->comp)
		return;

	dialog_to_comp_object (ee, priv->comp);
	set_title_from_comp (ee, priv->comp);

	if (!cal_client_update_object (priv->client, priv->comp))
		g_message ("save_event_object(): Could not update the object!");
	else
		event_editor_set_changed (ee, FALSE);
}

/* Closes the dialog box and emits the appropriate signals */
static void
close_dialog (EventEditor *ee)
{
	EventEditorPrivate *priv;

	priv = ee->priv;

	g_assert (priv->app != NULL);

	gtk_object_destroy (GTK_OBJECT (ee));
}



static void
debug_xml_cb (BonoboUIComponent *uic, gpointer data, const char *path)
{
	EventEditor *ee = EVENT_EDITOR (data);
	EventEditorPrivate *priv = ee->priv;

	bonobo_window_dump (BONOBO_WINDOW (priv->app), "on demand");
}

/* File/Save callback */
static void
file_save_cb (BonoboUIComponent *uic, gpointer data, const char *path)
{
	EventEditor *ee;

	ee = EVENT_EDITOR (data);
	save_event_object (ee);
}

/* File/Save and Close callback */
static void
file_save_and_close_cb (BonoboUIComponent *uic, gpointer data, const char *path)
{
	EventEditor *ee;

	ee = EVENT_EDITOR (data);
	save_event_object (ee);
	close_dialog (ee);
}

/* File/Delete callback */
static void
file_delete_cb (BonoboUIComponent *uic, gpointer data, const char *path)
{
	EventEditor *ee;
	EventEditorPrivate *priv;

	ee = EVENT_EDITOR (data);

	g_return_if_fail (IS_EVENT_EDITOR (ee));

	priv = ee->priv;

	g_return_if_fail (priv->comp);

	if (delete_component_dialog (priv->comp, priv->app)) {
		const char *uid;

		cal_component_get_uid (priv->comp, &uid);

		/* We don't check the return value; FALSE can mean the object
		 * was not in the server anyways.
		 */
		cal_client_remove_object (priv->client, uid);

		close_dialog (ee);
	}
}

/* File/Close callback */
static void
file_close_cb (BonoboUIComponent *uic, gpointer data, const char *path)
{
	EventEditor *ee;

	g_return_if_fail (IS_EVENT_EDITOR (data));

	ee = EVENT_EDITOR (data);

	if (prompt_to_save_changes (ee))
		close_dialog (ee);
}

static void
schedule_meeting_cb (BonoboUIComponent *uic, gpointer data, const char *path)
{
	EventEditor *ee;
	EventEditorPrivate *priv;
	EMeetingEditor *editor;

	ee = EVENT_EDITOR (data);

	g_return_if_fail (IS_EVENT_EDITOR (ee));

	priv = (EventEditorPrivate *)ee->priv;

	editor = e_meeting_editor_new (priv->comp, priv->client, ee);
	e_meeting_edit (editor);
	e_meeting_editor_free (editor);
}


/*
 * NB. there is an insane amount of replication here between
 * this and the task-editor.
 */
static BonoboUIVerb verbs [] = {
	BONOBO_UI_VERB ("FileSave", file_save_cb),
	BONOBO_UI_VERB ("FileDelete", file_delete_cb),
	BONOBO_UI_VERB ("FileClose", file_close_cb),
	BONOBO_UI_VERB ("FileSaveAndClose", file_save_and_close_cb),

	BONOBO_UI_VERB ("ActionScheduleMeeting", schedule_meeting_cb),

	BONOBO_UI_VERB ("DebugDumpXml", debug_xml_cb),

	BONOBO_UI_VERB_END
};



/* Callback used when the dialog box is destroyed */
static gint
app_delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	EventEditor *ee;

	g_return_val_if_fail (IS_EVENT_EDITOR (data), TRUE);

	ee = EVENT_EDITOR (data);

	if (prompt_to_save_changes (ee))
		close_dialog (ee);

	return TRUE;
}

/**
 * event_editor_construct:
 * @ee: An event editor.
 *
 * Constructs an event editor by loading its Glade data.
 *
 * Return value: The same object as @ee, or NULL if the widgets could not be
 * created.  In the latter case, the event editor will automatically be
 * destroyed.
 **/
EventEditor *
event_editor_construct (EventEditor *ee)
{
	EventEditorPrivate *priv;
	GtkWidget *bonobo_win;

	g_return_val_if_fail (ee != NULL, NULL);
	g_return_val_if_fail (IS_EVENT_EDITOR (ee), NULL);

	priv = ee->priv;

	/* Load the content widgets */

	priv->xml = glade_xml_new (EVOLUTION_GLADEDIR "/event-editor-dialog.glade", NULL);
	if (!priv->xml) {
		g_message ("event_editor_construct(): Could not load the Glade XML file!");
		goto error;
	}

	if (!get_widgets (ee)) {
		g_message ("event_editor_construct(): Could not find all widgets in the XML file!");
		goto error;
	}

	init_widgets (ee);

	priv->uic = bonobo_ui_component_new ("event-editor-dialog");
	if (!priv->uic) {
		g_message ("task_editor_construct(): Could not create the UI component");
		goto error;
	}

	/* Construct the app */
	bonobo_win = bonobo_window_new ("event-editor-dialog", "Event Editor");

	/* FIXME: The sucking bit */
	{
		GtkWidget *contents;

		contents = gnome_dock_get_client_area (
			GNOME_DOCK (GNOME_APP (priv->app)->dock));
		if (!contents) {
			g_message ("event_editor_construct(): Could not get contents");
			goto error;
		}
		gtk_widget_ref (contents);
		gtk_container_remove (GTK_CONTAINER (contents->parent), contents);
		bonobo_window_set_contents (BONOBO_WINDOW (bonobo_win), contents);
		gtk_widget_destroy (priv->app);
		priv->app = bonobo_win;
	}

	{
		BonoboUIContainer *container = bonobo_ui_container_new ();
		bonobo_ui_container_set_win (container, BONOBO_WINDOW (priv->app));
		bonobo_ui_component_set_container (
			priv->uic, bonobo_object_corba_objref (BONOBO_OBJECT (container)));
	}

	bonobo_ui_component_add_verb_list_with_data (priv->uic, verbs, ee);

	bonobo_ui_util_set_ui (priv->uic, EVOLUTION_DATADIR,
			       "evolution-event-editor.xml",
			       "evolution-event-editor");

	/* Hook to destruction of the dialog */

	gtk_signal_connect (GTK_OBJECT (priv->app), "delete_event",
			    GTK_SIGNAL_FUNC (app_delete_event_cb), ee);


	/* Add focus to the summary entry */

	gtk_widget_grab_focus (GTK_WIDGET (priv->general_summary));

	/* Show the dialog */

	gtk_widget_show (priv->app);

	return ee;

 error:

	gtk_object_unref (GTK_OBJECT (ee));
	return NULL;
}

/**
 * event_editor_new:
 *
 * Creates a new event editor dialog.
 *
 * Return value: A newly-created event editor dialog, or NULL if the event
 * editor could not be created.
 **/
EventEditor *
event_editor_new (void)
{
	EventEditor *ee;

	ee = EVENT_EDITOR (gtk_type_new (TYPE_EVENT_EDITOR));
	return event_editor_construct (EVENT_EDITOR (ee));
}

/* Brings attention to a window by raising it and giving it focus */
static void
raise_and_focus (GtkWidget *widget)
{
	g_assert (GTK_WIDGET_REALIZED (widget));
	gdk_window_show (widget->window);
	gtk_widget_grab_focus (widget);
}

/* Callback used when the calendar client tells us that an object changed */
static void
obj_updated_cb (CalClient *client, const char *uid, gpointer data)
{
	EventEditor *ee;
	EventEditorPrivate *priv;
	CalComponent *comp;
	CalClientGetStatus status;
	const gchar *editing_uid;

	ee = EVENT_EDITOR (data);

	g_return_if_fail (IS_EVENT_EDITOR (ee));

	priv = ee->priv;

	/* If we aren't showing the object which has been updated, return. */
	if (!priv->comp)
	  return;
	cal_component_get_uid (priv->comp, &editing_uid);
	if (strcmp (uid, editing_uid))
	  return;


	/* Get the event from the server. */
	status = cal_client_get_object (priv->client, uid, &comp);

	switch (status) {
	case CAL_CLIENT_GET_SUCCESS:
		/* Everything is fine */
		break;

	case CAL_CLIENT_GET_SYNTAX_ERROR:
		g_message ("obj_updated_cb(): Syntax error when getting object `%s'", uid);
		return;

	case CAL_CLIENT_GET_NOT_FOUND:
		/* The object is no longer in the server, so do nothing */
		return;

	default:
		g_assert_not_reached ();
		return;
	}

	raise_and_focus (priv->app);
}

/* Callback used when the calendar client tells us that an object was removed */
static void
obj_removed_cb (CalClient *client, const char *uid, gpointer data)
{
	EventEditor *ee;
	EventEditorPrivate *priv;
	const gchar *editing_uid;

	ee = EVENT_EDITOR (data);

	g_return_if_fail (ee != NULL);
	g_return_if_fail (IS_EVENT_EDITOR (ee));

	priv = ee->priv;

	/* If we aren't showing the object which has been updated, return. */
	if (!priv->comp)
	  return;
	cal_component_get_uid (priv->comp, &editing_uid);
	if (strcmp (uid, editing_uid))
	  return;


	raise_and_focus (priv->app);
}

/**
 * event_editor_set_cal_client:
 * @ee: An event editor.
 * @client: Calendar client.
 *
 * Sets the calendar client than an event editor will use for updating its
 * calendar components.
 **/
void
event_editor_set_cal_client (EventEditor *ee, CalClient *client)
{
	EventEditorPrivate *priv;

	g_return_if_fail (ee != NULL);
	g_return_if_fail (IS_EVENT_EDITOR (ee));

	priv = ee->priv;

	if (client == priv->client)
		return;

	if (client)
		g_return_if_fail (IS_CAL_CLIENT (client));

	if (client)
		g_return_if_fail (cal_client_get_load_state (client) == CAL_CLIENT_LOAD_LOADED);

	if (client)
		gtk_object_ref (GTK_OBJECT (client));

	if (priv->client) {
		gtk_signal_disconnect_by_data (GTK_OBJECT (priv->client), ee);
		gtk_object_unref (GTK_OBJECT (priv->client));
	}

	priv->client = client;

	if (priv->client) {
		gtk_signal_connect (GTK_OBJECT (priv->client), "obj_updated",
				    GTK_SIGNAL_FUNC (obj_updated_cb), ee);
		gtk_signal_connect (GTK_OBJECT (priv->client), "obj_removed",
				    GTK_SIGNAL_FUNC (obj_removed_cb), ee);
	}
}

/**
 * event_editor_get_cal_client:
 * @ee: An event editor.
 *
 * Queries the calendar client that an event editor is using to update its
 * calendar components.
 *
 * Return value: A calendar client object.
 **/
CalClient *
event_editor_get_cal_client (EventEditor *ee)
{
	EventEditorPrivate *priv;

	g_return_val_if_fail (ee != NULL, NULL);
	g_return_val_if_fail (IS_EVENT_EDITOR (ee), NULL);

	priv = ee->priv;
	return priv->client;
}

/**
 * event_editor_set_event_object:
 * @ee: An event editor.
 * @comp: A calendar object.
 *
 * Sets the calendar object that an event editor dialog will manipulate.
 **/
void
event_editor_set_event_object (EventEditor *ee, CalComponent *comp)
{
	EventEditorPrivate *priv;

	g_return_if_fail (ee != NULL);
	g_return_if_fail (IS_EVENT_EDITOR (ee));

	priv = ee->priv;

	if (priv->comp) {
		gtk_object_unref (GTK_OBJECT (priv->comp));
		priv->comp = NULL;
	}

	if (comp) {
		priv->comp = cal_component_clone (comp);
	}

	set_title_from_comp (ee, priv->comp);
	fill_widgets (ee);
}

/**
 * event_editor_focus:
 * @ee: An event editor.
 *
 * Makes sure an event editor is shown, on top of other windows, and focused.
 **/
void
event_editor_focus (EventEditor *ee)
{
	EventEditorPrivate *priv;

	g_return_if_fail (ee != NULL);
	g_return_if_fail (IS_EVENT_EDITOR (ee));

	priv = ee->priv;
	gtk_widget_show_now (priv->app);
	raise_and_focus (priv->app);
}

/* Sets the sensitivity of the relevant alarm widgets. Called when filling
   the widgets initially and when the alarm button is toggled. */
static void
alarm_toggle (GtkWidget *toggle, EventEditor *ee)
{
	EventEditorPrivate *priv;
	GtkWidget *alarm_amount = NULL;
	GtkWidget *alarm_unit = NULL;
	gboolean active;

	priv = ee->priv;

	event_editor_set_changed (ee, TRUE);

	active = GTK_TOGGLE_BUTTON (toggle)->active;

	if (toggle == priv->alarm_display) {
		alarm_amount = priv->alarm_display_amount;
		alarm_unit = priv->alarm_display_unit;
	} else if (toggle == priv->alarm_audio) {
		alarm_amount = priv->alarm_audio_amount;
		alarm_unit = priv->alarm_audio_unit;
	} else if (toggle == priv->alarm_program) {
		alarm_amount = priv->alarm_program_amount;
		alarm_unit = priv->alarm_program_unit;
		gtk_widget_set_sensitive (priv->alarm_program_run_program, active);
	} else if (toggle == priv->alarm_mail) {
		alarm_amount = priv->alarm_mail_amount;
		alarm_unit = priv->alarm_mail_unit;
		gtk_widget_set_sensitive (priv->alarm_mail_mail_to, active);
	} else
		g_assert_not_reached ();

	gtk_widget_set_sensitive (alarm_amount, active);
	gtk_widget_set_sensitive (alarm_unit, active);
}

/* Checks if the event's time starts and ends at midnight, and sets the
 * "all day event" box accordingly.
 */
static void
check_all_day (EventEditor *ee)
{
	EventEditorPrivate *priv;
	time_t ev_start, ev_end;
	gboolean all_day = FALSE;

	priv = ee->priv;

	/* Currently we just return if the date is not set or not valid.
	   I'm not entirely sure this is the corrent thing to do. */
	ev_start = e_date_edit_get_time (E_DATE_EDIT (priv->start_time));
	g_return_if_fail (ev_start != -1);

	ev_end = e_date_edit_get_time (E_DATE_EDIT (priv->end_time));
	g_return_if_fail (ev_end != -1);

	/* all day event checkbox */
	if (time_day_begin (ev_start) == ev_start
	    && time_day_begin (ev_end) == ev_end)
		all_day = TRUE;

	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->all_day_event), ee);

	e_dialog_toggle_set (priv->all_day_event, all_day);

	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->all_day_event), ee);

	e_date_edit_set_show_time (E_DATE_EDIT (priv->start_time), !all_day);
	e_date_edit_set_show_time (E_DATE_EDIT (priv->end_time), !all_day);
}

/*
 * Callback: all day event button toggled.
 * Note that this should only be called when the user explicitly toggles the
 * button. Be sure to block this handler when the toggle button's state is set
 * within the code.
 */
static void
set_all_day (GtkWidget *toggle, EventEditor *ee)
{
	EventEditorPrivate *priv;
	struct tm start_tm, end_tm;
	time_t start_t, end_t;
	gboolean all_day;

	priv = ee->priv;

	event_editor_set_changed (ee, TRUE);

	/* When the all_day toggle is turned on, the start date is rounded down
	   to the start of the day, and end date is rounded down to the start
	   of the day on which the event ends. The event is then taken to be
	   inclusive of the days between the start and end days.
	   Note that if the event end is at midnight, we do not round it down
	   to the previous day, since if we do that and the user repeatedly
	   turns the all_day toggle on and off, the event keeps shrinking.
	   (We'd also need to make sure we didn't adjust the time when the
	   radio button is initially set.)

	   When the all_day_toggle is turned off, we set the event start to the
	   start of the working day, and if the event end is on or before the
	   day of the event start we set it to one hour after the event start.
	*/
	all_day = GTK_TOGGLE_BUTTON (toggle)->active;

	/*
	 * Start time.
	 */
	start_t = e_date_edit_get_time (E_DATE_EDIT (priv->start_time));
	g_return_if_fail (start_t != -1);

	start_tm = *localtime (&start_t);

	if (all_day) {
		/* Round down to the start of the day. */
		start_tm.tm_hour = 0;
		start_tm.tm_min  = 0;
		start_tm.tm_sec  = 0;
	} else {
		/* Set to the start of the working day. */
		start_tm.tm_hour = calendar_config_get_day_start_hour ();
		start_tm.tm_min  = calendar_config_get_day_start_minute ();
		start_tm.tm_sec  = 0;
	}

	/*
	 * End time.
	 */
	end_t = e_date_edit_get_time (E_DATE_EDIT (priv->end_time));
	g_return_if_fail (end_t != -1);

	end_tm = *localtime (&end_t);

	if (all_day) {
		/* Round down to the start of the day. */
		end_tm.tm_hour = 0;
		end_tm.tm_min  = 0;
		end_tm.tm_sec  = 0;
	} else {
		/* If the event end is now on or before the event start day,
		   make it end one hour after the start. mktime() will fix any
		   overflows. */
		if (end_tm.tm_year < start_tm.tm_year
		    || (end_tm.tm_year == start_tm.tm_year
			&& end_tm.tm_mon < start_tm.tm_mon)
		    || (end_tm.tm_year == start_tm.tm_year
			&& end_tm.tm_mon == start_tm.tm_mon
			&& end_tm.tm_mday <= start_tm.tm_mday)) {
			end_tm.tm_year = start_tm.tm_year;
			end_tm.tm_mon = start_tm.tm_mon;
			end_tm.tm_mday = start_tm.tm_mday;
			end_tm.tm_hour = start_tm.tm_hour + 1;
		}
	}

	/* Block date_changed_cb, or dates_changed() will be called after the
	   start time is set (but before the end time is set) and will call
	   check_all_day() and mess us up. */
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->start_time), ee);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->end_time), ee);

	/* will set recur start too */
	e_date_edit_set_time (E_DATE_EDIT (priv->start_time), mktime (&start_tm));

	e_date_edit_set_time (E_DATE_EDIT (priv->end_time), mktime (&end_tm));

	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->start_time), ee);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->end_time), ee);

	e_date_edit_set_show_time (E_DATE_EDIT (priv->start_time), !all_day);
	e_date_edit_set_show_time (E_DATE_EDIT (priv->end_time), !all_day);

	preview_recur (ee);
}

/* Callback used when the start or end date widgets change.  We check that the
 * start date < end date and we set the "all day event" button as appropriate.
 */
static void
date_changed_cb (EDateEdit *dedit, gpointer data)
{
	EventEditor *ee;
	EventEditorPrivate *priv;
	time_t start, end;
	struct tm tm_start, tm_end;

	ee = EVENT_EDITOR (data);
	priv = ee->priv;

	event_editor_set_changed (ee, TRUE);

	/* Ensure that start < end */

	start = e_date_edit_get_time (E_DATE_EDIT (priv->start_time));
	g_return_if_fail (start != -1);
	end = e_date_edit_get_time (E_DATE_EDIT (priv->end_time));
	g_return_if_fail (end != -1);

	if (start >= end) {
		tm_start = *localtime (&start);
		tm_end = *localtime (&end);

		if (start == end && tm_start.tm_hour == 0
		    && tm_start.tm_min == 0 && tm_start.tm_sec == 0) {
			/* If the start and end times are the same, but both
			   are on day boundaries, then that is OK since it
			   means we have an all-day event lasting 1 day.
			   So we do nothing here. */

		} else if (GTK_WIDGET (dedit) == priv->start_time) {
			/* Modify the end time */

			tm_end.tm_year = tm_start.tm_year;
			tm_end.tm_mon  = tm_start.tm_mon;
			tm_end.tm_mday = tm_start.tm_mday;
			tm_end.tm_hour = tm_start.tm_hour + 1;
			tm_end.tm_min  = tm_start.tm_min;
			tm_end.tm_sec  = tm_start.tm_sec;

			gtk_signal_handler_block_by_data (GTK_OBJECT (priv->end_time), ee);
			e_date_edit_set_time (E_DATE_EDIT (priv->end_time), mktime (&tm_end));
			gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->end_time), ee);
		} else if (GTK_WIDGET (dedit) == priv->end_time) {
			/* Modify the start time */

			tm_start.tm_year = tm_end.tm_year;
			tm_start.tm_mon  = tm_end.tm_mon;
			tm_start.tm_mday = tm_end.tm_mday;
			tm_start.tm_hour = tm_end.tm_hour - 1;
			tm_start.tm_min  = tm_end.tm_min;
			tm_start.tm_sec  = tm_end.tm_sec;

			gtk_signal_handler_block_by_data (GTK_OBJECT (priv->start_time), ee);
			e_date_edit_set_time (E_DATE_EDIT (priv->start_time), mktime (&tm_start));
			gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->start_time), ee);
		} else
			g_assert_not_reached ();
	}

	/* Set the "all day event" button as appropriate */

	check_all_day (ee);

	/* Retag the recurrence preview calendar */

	preview_recur (ee);
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
append_exception (EventEditor *ee, time_t t)
{
	EventEditorPrivate *priv;
	time_t *tt;
	char *c[1];
	int i;
	GtkCList *clist;

	priv = ee->priv;

	tt = g_new (time_t, 1);
	*tt = t;

	clist = GTK_CLIST (priv->recurrence_exception_list);

	c[0] = get_exception_string (t);
	i = e_utf8_gtk_clist_append (clist, c);

	gtk_clist_set_row_data (clist, i, tt);
	gtk_clist_select_row (clist, i, 0);

	gtk_widget_set_sensitive (priv->recurrence_exception_modify, TRUE);
	gtk_widget_set_sensitive (priv->recurrence_exception_delete, TRUE);
}


/* Callback for the "add exception" button */
static void
recurrence_exception_add_cb (GtkWidget *widget, EventEditor *ee)
{
	EventEditorPrivate *priv;
	time_t t;

	priv = ee->priv;

	event_editor_set_changed (ee, TRUE);
	t = e_date_edit_get_time (E_DATE_EDIT (priv->recurrence_exception_date));
	append_exception (ee, t);
	preview_recur (ee);
}

/* Callback for the "modify exception" button */
static void
recurrence_exception_modify_cb (GtkWidget *widget, EventEditor *ee)
{
	EventEditorPrivate *priv;
	GtkCList *clist;
	time_t *t;
	int sel;

	priv = ee->priv;

	clist = GTK_CLIST (priv->recurrence_exception_list);
	if (!clist->selection)
		return;

	event_editor_set_changed (ee, TRUE);

	sel = GPOINTER_TO_INT (clist->selection->data);

	t = gtk_clist_get_row_data (clist, sel);
	*t = e_date_edit_get_time (E_DATE_EDIT (priv->recurrence_exception_date));

	e_utf8_gtk_clist_set_text (clist, sel, 0, get_exception_string (*t));

	preview_recur (ee);
}

/* Callback for the "delete exception" button */
static void
recurrence_exception_delete_cb (GtkWidget *widget, EventEditor *ee)
{
	EventEditorPrivate *priv;
	GtkCList *clist;
	int sel;

	priv = ee->priv;

	clist = GTK_CLIST (priv->recurrence_exception_list);
	if (!clist->selection)
		return;

	event_editor_set_changed (ee, TRUE);

	sel = GPOINTER_TO_INT (clist->selection->data);

	g_free (gtk_clist_get_row_data (clist, sel)); /* free the time_t stored there */

	gtk_clist_remove (clist, sel);
	if (sel >= clist->rows)
		sel--;

	if (clist->rows > 0)
		gtk_clist_select_row (clist, sel, 0);
	else {
		gtk_widget_set_sensitive (priv->recurrence_exception_modify, FALSE);
		gtk_widget_set_sensitive (priv->recurrence_exception_delete, FALSE);
	}

	preview_recur (ee);
}


GtkWidget *
make_date_edit (void)
{
	return date_edit_new (TRUE, TRUE);
}


GtkWidget *
make_spin_button (int val, int low, int high)
{
	GtkAdjustment *adj;
	GtkWidget *spin;

	adj = GTK_ADJUSTMENT (gtk_adjustment_new (val, low, high, 1, 10, 10));
	spin = gtk_spin_button_new (adj, 0.5, 0);
	gtk_widget_set_usize (spin, 60, 0);

	return spin;
}


/* This is called when most fields are changed (except those which already
   have signal handlers). It just sets the "changed" flag. */
static void
field_changed			(GtkWidget	*widget,
				 EventEditor	*ee)
{
	EventEditorPrivate *priv;

	g_return_if_fail (IS_EVENT_EDITOR (ee));

	priv = ee->priv;

	event_editor_set_changed (ee, TRUE);
}


static void
event_editor_set_changed	(EventEditor	*ee,
				 gboolean	 changed)
{
	EventEditorPrivate *priv;

	priv = ee->priv;

#if 0
	g_print ("In event_editor_set_changed: %s\n",
		 changed ? "TRUE" : "FALSE");
#endif

	priv->changed = changed;
}


/* This checks if the "changed" field is set, and if so it prompts to save
   the changes using a "Save/Discard/Cancel" modal dialog. It then saves the
   changes if requested. It returns TRUE if the dialog should now be closed. */
static gboolean
prompt_to_save_changes		(EventEditor	*ee)
{
	EventEditorPrivate *priv;
	GtkWidget *dialog;

	priv = ee->priv;

	if (!priv->changed)
		return TRUE;

	dialog = gnome_message_box_new (_("Do you want to save changes?"),
					GNOME_MESSAGE_BOX_QUESTION,
					GNOME_STOCK_BUTTON_YES,
					GNOME_STOCK_BUTTON_NO,
					GNOME_STOCK_BUTTON_CANCEL,
					NULL);

	gnome_dialog_set_parent (GNOME_DIALOG (dialog),
				 GTK_WINDOW (priv->app));

	switch (gnome_dialog_run_and_close (GNOME_DIALOG (dialog))) {
	case 0: /* Save */
		/* FIXME: If an error occurs here, we should popup a dialog
		   and then return FALSE. */
		save_event_object (ee);
		return TRUE;
	case 1: /* Discard */
		return TRUE;
	case 2: /* Cancel */
	default:
		return FALSE;
		break;
	}
}

static void
categories_clicked (GtkWidget *button, EventEditor *ee)
{
	char *categories;
	GnomeDialog *dialog;
	int result;
	GtkWidget *entry;

	entry = ee->priv->categories;
	categories = e_utf8_gtk_entry_get_text (GTK_ENTRY (entry));

	dialog = GNOME_DIALOG (e_categories_new (categories));
	result = gnome_dialog_run (dialog);
	g_free (categories);

	if (result == 0) {
		gtk_object_get (GTK_OBJECT (dialog),
				"categories", &categories,
				NULL);
		e_utf8_gtk_entry_set_text (GTK_ENTRY (entry), categories);
		g_free (categories);
	}
	gtk_object_destroy (GTK_OBJECT (dialog));
}
