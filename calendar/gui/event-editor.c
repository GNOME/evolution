/* Evolution calendar - Event editor dialog
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Authors: Miguel de Icaza <miguel@helixcode.com>
 *          Federico Mena-Quintero <federico@helixcode.com>
 *          Seth Alves <alves@helixcode.com>
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
#include <cal-util/timeutil.h>
#include "event-editor.h"
#include "e-meeting-edit.h"
#include "weekday-picker.h"



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

	GtkWidget *classification_radio;

	GtkWidget *recurrence_summary;
	GtkWidget *recurrence_starting_date;

	GtkWidget *recurrence_none;
	GtkWidget *recurrence_simple;
	GtkWidget *recurrence_custom;
	GtkWidget *recurrence_custom_warning;

	GtkWidget *recurrence_params;
	GtkWidget *recurrence_interval_value;
	GtkWidget *recurrence_interval_unit;
	GtkWidget *recurrence_special;
	GtkWidget *recurrence_ending_menu;
	GtkWidget *recurrence_ending_special;

	/* For weekly recurrences, created by hand */
	GtkWidget *recurrence_weekday_picker;
	guint8 recurrence_weekday_day_mask;

	/* For ending date, created by hand */
	GtkWidget *recurrence_ending_date_edit;
	time_t recurrence_ending_date;

	/* For ending count of ocurrences, created by hand */
	GtkWidget *recurrence_ending_count_spin;
	int recurrence_ending_count;

	GtkWidget *recurrence_rule_notebook;
	GtkWidget *recurrence_rule_none;
	GtkWidget *recurrence_rule_daily;
	GtkWidget *recurrence_rule_weekly;
	GtkWidget *recurrence_rule_monthly;
	GtkWidget *recurrence_rule_yearly;

	GtkWidget *recurrence_rule_daily_days;

	GtkWidget *recurrence_rule_weekly_weeks;
	GtkWidget *recurrence_rule_weekly_sun;
	GtkWidget *recurrence_rule_weekly_mon;
	GtkWidget *recurrence_rule_weekly_tue;
	GtkWidget *recurrence_rule_weekly_wed;
	GtkWidget *recurrence_rule_weekly_thu;
	GtkWidget *recurrence_rule_weekly_fri;
	GtkWidget *recurrence_rule_weekly_sat;

	GtkWidget *recurrence_rule_monthly_on_day;
	GtkWidget *recurrence_rule_monthly_weekday;
	GtkWidget *recurrence_rule_monthly_day_nth;
	GtkWidget *recurrence_rule_monthly_week;
	GtkWidget *recurrence_rule_monthly_weekpos;
	GtkWidget *recurrence_rule_monthly_every_n_months;
	GtkWidget *recurrence_rule_yearly_every_n_years;

	/* More widgets from the Glade file */

	GtkWidget *recurrence_exception_date;
	GtkWidget *recurrence_exception_list;
	GtkWidget *recurrence_exception_add;
	GtkWidget *recurrence_exception_modify;
	GtkWidget *recurrence_exception_delete;
};



static void event_editor_class_init (EventEditorClass *class);
static void event_editor_init (EventEditor *ee);
static void event_editor_destroy (GtkObject *object);

static GtkObjectClass *parent_class;

extern int day_begin, day_end;
extern char *user_name;
extern int am_pm_flag;
extern int week_starts_on_monday;


static void append_exception (EventEditor *ee, time_t t);
static void check_all_day (EventEditor *ee);
static void set_all_day (GtkWidget *toggle, EventEditor *ee);
static void alarm_toggle (GtkWidget *toggle, EventEditor *ee);
static void check_dates (EDateEdit *dedit, EventEditor *ee);
static void check_times (EDateEdit *dedit, EventEditor *ee);
static void recurrence_exception_add_cb (GtkWidget *widget, EventEditor *ee);
static void recurrence_exception_modify_cb (GtkWidget *widget, EventEditor *ee);
static void recurrence_exception_delete_cb (GtkWidget *widget, EventEditor *ee);



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

	weekday_picker_set_week_starts_on_monday (wp, week_starts_on_monday);
	weekday_picker_set_days (wp, priv->recurrence_weekday_day_mask);
}

/* Creates the special contents for monthly recurrences */
static void
make_recur_monthly_special (EventEditor *ee)
{
	/* FIXME: create the "on the" <nth> [day, Weekday, last Weekday] */
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
 * For monthly recurrences: "on the" <nth> [day, Weekday, last Weekday]
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

	priv->recurrence_ending_date_edit = e_date_edit_new ();
	de = E_DATE_EDIT (priv->recurrence_ending_date_edit);

	e_date_edit_set_show_time (de, FALSE);
	gtk_container_add (GTK_CONTAINER (priv->recurrence_ending_special), GTK_WIDGET (de));

	gtk_widget_show_all (GTK_WIDGET (de));

	/* Set the value */

	e_date_edit_set_time (de, priv->recurrence_ending_date);
}

/* Creates the special contents for the ocurrence count case */
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

	label = gtk_label_new (_("ocurrences"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	gtk_widget_show_all (hbox);

	/* Set the values */

	e_dialog_spin_set (priv->recurrence_ending_count_spin,
			   priv->recurrence_ending_count);
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
	
/* Callback used when one of the recurrence type radio buttons is toggled.  We
 * enable or the recurrence parameters.
 */
static void
recurrence_type_toggled_cb (GtkWidget *widget, gpointer data)
{
	EventEditor *ee;
	EventEditorPrivate *priv;
	enum recur_type type;

	ee = EVENT_EDITOR (data);
	priv = ee->priv;

	type = e_dialog_radio_get (widget, recur_type_map);

	switch (type) {
	case RECUR_NONE:
		gtk_widget_set_sensitive (priv->recurrence_params, FALSE);
		gtk_widget_hide (priv->recurrence_custom_warning);
		break;

	case RECUR_SIMPLE:
		gtk_widget_set_sensitive (priv->recurrence_params, TRUE);
		gtk_widget_hide (priv->recurrence_custom_warning);
		break;

	case RECUR_CUSTOM:
		gtk_widget_set_sensitive (priv->recurrence_params, FALSE);
		gtk_widget_show (priv->recurrence_custom_warning);
		break;

	default:
		g_assert_not_reached ();
	}
}

/* Callback used when the recurrence interval option menu changes.  We need to
 * change the contents of the recurrence special widget.
 */
static void
recur_interval_selection_done_cb (GtkMenuShell *menu_shell, gpointer data)
{
	EventEditor *ee;

	ee = EVENT_EDITOR (data);
	make_recurrence_special (ee);
}

/* Callback used when the recurrence ending option menu changes.  We need to
 * change the contents of the ending special widget.
 */
static void
recur_ending_selection_done_cb (GtkMenuShell *menu_shell, gpointer data)
{
	EventEditor *ee;

	ee = EVENT_EDITOR (data);
	make_recurrence_ending_special (ee);
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

	priv->classification_radio = GW ("classification-radio");

	priv->recurrence_summary = GW ("recurrence-summary");
	priv->recurrence_starting_date = GW ("recurrence-starting-date");

	priv->recurrence_none = GW ("recurrence-none");
	priv->recurrence_simple = GW ("recurrence-simple");
	priv->recurrence_custom = GW ("recurrence-custom");
	priv->recurrence_custom_warning = GW ("recurrence-custom-warning");
	priv->recurrence_params = GW ("recurrence-params");

	priv->recurrence_interval_value = GW ("recurrence-interval-value");
	priv->recurrence_interval_unit = GW ("recurrence-interval-unit");
	priv->recurrence_special = GW ("recurrence-special");
	priv->recurrence_ending_menu = GW ("recurrence-ending-menu");
	priv->recurrence_ending_special = GW ("recurrence-ending-special");

	priv->recurrence_rule_monthly_on_day = GW ("recurrence-rule-monthly-on-day");
	priv->recurrence_rule_monthly_weekday = GW ("recurrence-rule-monthly-weekday");
	priv->recurrence_rule_monthly_day_nth = GW ("recurrence-rule-monthly-day-nth");
	priv->recurrence_rule_monthly_week = GW ("recurrence-rule-monthly-week");
	priv->recurrence_rule_monthly_weekpos = GW ("recurrence-rule-monthly-weekpos");
	priv->recurrence_rule_monthly_every_n_months = GW ("recurrence-rule-monthly-every-n-months");

	priv->recurrence_exception_date = GW ("recurrence-exception-date");
	priv->recurrence_exception_list = GW ("recurrence-exception-list");
	priv->recurrence_exception_add = GW ("recurrence-exception-add");
	priv->recurrence_exception_modify = GW ("recurrence-exception-modify");
	priv->recurrence_exception_delete = GW ("recurrence-exception-delete");

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
		&& priv->classification_radio
		&& priv->recurrence_summary
		&& priv->recurrence_starting_date
		&& priv->recurrence_none
		&& priv->recurrence_simple
		&& priv->recurrence_custom
		&& priv->recurrence_custom_warning
		&& priv->recurrence_params
		&& priv->recurrence_interval_value
		&& priv->recurrence_interval_unit
		&& priv->recurrence_special
		&& priv->recurrence_ending_menu
		&& priv->recurrence_ending_special

		&& priv->recurrence_rule_monthly_on_day
		&& priv->recurrence_rule_monthly_weekday
		&& priv->recurrence_rule_monthly_day_nth
		&& priv->recurrence_rule_monthly_week
		&& priv->recurrence_rule_monthly_weekpos
		&& priv->recurrence_rule_monthly_every_n_months

		&& priv->recurrence_exception_date
		&& priv->recurrence_exception_list
		&& priv->recurrence_exception_add
		&& priv->recurrence_exception_modify
		&& priv->recurrence_exception_delete);
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

/* Syncs the contents of two date editor widgets, while blocking signals from
 * each other.
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

/* Hooks the widget signals */
static void
init_widgets (EventEditor *ee)
{
	EventEditorPrivate *priv;
	GtkWidget *menu;

	priv = ee->priv;

	/* Summary in the main and recurrence pages */

	gtk_signal_connect (GTK_OBJECT (priv->general_summary), "changed",
			    GTK_SIGNAL_FUNC (summary_changed_cb), priv->recurrence_summary);
	gtk_signal_connect (GTK_OBJECT (priv->recurrence_summary), "changed",
			    GTK_SIGNAL_FUNC (summary_changed_cb), priv->general_summary);

	/* Start dates in the main and recurrence pages */

	gtk_signal_connect (GTK_OBJECT (priv->start_time), "date_changed",
			    GTK_SIGNAL_FUNC (start_date_changed_cb), priv->recurrence_starting_date);
	gtk_signal_connect (GTK_OBJECT (priv->recurrence_starting_date), "date_changed",
			    GTK_SIGNAL_FUNC (start_date_changed_cb), priv->start_time);

	/* Start and end times */

	gtk_signal_connect (GTK_OBJECT (priv->start_time), "date_changed",
			    GTK_SIGNAL_FUNC (check_dates), ee);
	gtk_signal_connect (GTK_OBJECT (priv->start_time), "time_changed",
			    GTK_SIGNAL_FUNC (check_times), ee);

	gtk_signal_connect (GTK_OBJECT (priv->end_time), "date_changed",
			    GTK_SIGNAL_FUNC (check_dates), ee);
	gtk_signal_connect (GTK_OBJECT (priv->end_time), "time_changed",
			    GTK_SIGNAL_FUNC (check_times), ee);

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

	/* Recurrence types */

	gtk_signal_connect (GTK_OBJECT (priv->recurrence_none), "toggled",
			    GTK_SIGNAL_FUNC (recurrence_type_toggled_cb), ee);
	gtk_signal_connect (GTK_OBJECT (priv->recurrence_simple), "toggled",
			    GTK_SIGNAL_FUNC (recurrence_type_toggled_cb), ee);
	gtk_signal_connect (GTK_OBJECT (priv->recurrence_custom), "toggled",
			    GTK_SIGNAL_FUNC (recurrence_type_toggled_cb), ee);

	/* Recurrence units */

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->recurrence_interval_unit));
	g_assert (menu != NULL);

	gtk_signal_connect (GTK_OBJECT (menu), "selection_done",
			    GTK_SIGNAL_FUNC (recur_interval_selection_done_cb), ee);

	/* Recurrence ending */

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->recurrence_ending_menu));
	g_assert (menu != NULL);

	gtk_signal_connect (GTK_OBJECT (menu), "selection_done",
			    GTK_SIGNAL_FUNC (recur_ending_selection_done_cb), ee);

	/* Exception buttons */

	gtk_signal_connect (GTK_OBJECT (priv->recurrence_exception_add), "clicked",
			    GTK_SIGNAL_FUNC (recurrence_exception_add_cb), ee);
	gtk_signal_connect (GTK_OBJECT (priv->recurrence_exception_modify), "clicked",
			    GTK_SIGNAL_FUNC (recurrence_exception_modify_cb), ee);
	gtk_signal_connect (GTK_OBJECT (priv->recurrence_exception_delete), "clicked",
			    GTK_SIGNAL_FUNC (recurrence_exception_delete_cb), ee);
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

	priv = ee->priv;

	now = time (NULL);

	/* Summary, description */

	e_dialog_editable_set (priv->general_summary, NULL); /* will also change recur summary */
	e_dialog_editable_set (priv->description, NULL);

	/* Start and end times */

	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->start_time), ee);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->end_time), ee);

	e_date_edit_set_time (E_DATE_EDIT (priv->start_time), now);
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

	e_dialog_radio_set (priv->classification_radio,
			    CAL_COMPONENT_CLASS_PRIVATE, classification_map);

	/* Recurrences */

	priv->recurrence_weekday_day_mask = 0;

	e_dialog_radio_set (priv->recurrence_none, RECUR_NONE, recur_type_map);

	e_dialog_spin_set (priv->recurrence_interval_value, 1);
	e_dialog_option_menu_set (priv->recurrence_interval_unit, ICAL_DAILY_RECURRENCE,
				  recur_freq_map);

	priv->recurrence_ending_date = time (NULL);
	priv->recurrence_ending_count = 1;

	e_dialog_option_menu_set (priv->recurrence_ending_menu, ENDING_FOREVER,
				  ending_types_map);

	/* Old recurrences */

	e_dialog_toggle_set (priv->recurrence_rule_monthly_on_day, TRUE);
	e_dialog_spin_set (priv->recurrence_rule_monthly_day_nth, 1);
	e_dialog_spin_set (priv->recurrence_rule_monthly_every_n_months, 1);
	e_dialog_option_menu_set (priv->recurrence_rule_monthly_week, 0, month_pos_map);
	e_dialog_option_menu_set (priv->recurrence_rule_monthly_weekpos, 0, weekday_map);
	e_dialog_spin_set (priv->recurrence_rule_monthly_every_n_months, 1);

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

	priv = ee->priv;

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
		/* Count of ocurrences */

		priv->recurrence_ending_count = r->count;
		e_dialog_option_menu_set (priv->recurrence_ending_menu,
					  ENDING_FOR,
					  ending_types_map);
	}
}

/* Counts the number of elements in the by_xxx fields of an icalrecurrencetype */
static int
count_by_xxx (short *field, int max_elements)
{
	int i;

	for (i = 0; i < max_elements; i++)
		if (field[i] == SHRT_MAX)
			break;

	return i;
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

	priv = ee->priv;
	g_assert (priv->comp != NULL);

	/* No recurrences? */

	if (!cal_component_has_rdates (priv->comp)
	    && !cal_component_has_rrules (priv->comp)
	    && !cal_component_has_exrules (priv->comp)) {
		e_dialog_radio_set (priv->recurrence_none, RECUR_NONE, recur_type_map);
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

		e_dialog_option_menu_set (priv->recurrence_interval_unit, ICAL_DAILY_RECURRENCE,
					  recur_freq_map);
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

		for (i = 0; i < 8 && r->by_day[i] != SHRT_MAX; i++) {
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

		e_dialog_option_menu_set (priv->recurrence_interval_unit, ICAL_WEEKLY_RECURRENCE,
					  recur_freq_map);
		break;
	}

	case ICAL_MONTHLY_RECURRENCE:
		/* FIXME */

	default:
		goto custom;
	}

	/* If we got here it means it is a simple recurrence */

	e_dialog_radio_set (priv->recurrence_simple, RECUR_SIMPLE, recur_type_map);
	e_dialog_spin_set (priv->recurrence_interval_value, r->interval);

	fill_ending_date (ee, r);

	goto out;

 custom:

	e_dialog_radio_set (priv->recurrence_custom, RECUR_CUSTOM, recur_type_map);

 out:

	cal_component_free_recur_list (rrule_list);
}

/* Fills in the widgets with the value from the calendar component */
static void
fill_widgets (EventEditor *ee)
{
	EventEditorPrivate *priv;
	CalComponentText text;
	CalComponentClassification cl;
	CalComponentDateTime d;
	GSList *list, *l;
	time_t dtstart, dtend;

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

	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->start_time), ee);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->end_time), ee);

	/* All-day events are inclusive, i.e. if the end date shown is 2nd Feb
	   then the event includes all of the 2nd Feb. We would normally show
	   3rd Feb as the end date, since it really ends at midnight on 3rd,
	   so we have to subtract a day so we only show the 2nd. */
	cal_component_get_dtstart (priv->comp, &d);
	dtstart = icaltime_as_timet (*d.value);
	cal_component_get_dtend (priv->comp, &d);
	dtend = icaltime_as_timet (*d.value);

	if (time_day_begin (dtstart) == dtstart
	    && time_day_begin (dtend) == dtend) {
		dtend = time_add_day (dtend, -1);
	}

	e_date_edit_set_time (E_DATE_EDIT (priv->start_time), dtstart);
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
#endif
	/* Alarm data */
#if 0
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
	/* Classification */

	cal_component_get_classification (priv->comp, &cl);

	switch (cl) {
	case CAL_COMPONENT_CLASS_PUBLIC:
	    	e_dialog_radio_set (priv->classification_radio, CAL_COMPONENT_CLASS_PUBLIC,
				    classification_map);
	case CAL_COMPONENT_CLASS_PRIVATE:
	    	e_dialog_radio_set (priv->classification_radio, CAL_COMPONENT_CLASS_PRIVATE,
				    classification_map);
	case CAL_COMPONENT_CLASS_CONFIDENTIAL:
	    	e_dialog_radio_set (priv->classification_radio, CAL_COMPONENT_CLASS_CONFIDENTIAL,
				    classification_map);
	default:
		/* What do do?  We can't g_assert_not_reached() since it is a
		 * value from an external file.
		 */
	}

	/* Recurrences */

	fill_recurrence_widgets (ee);

#if 0
	
	if (cal_component_has_rrules (priv->comp)) {
		struct icalrecurrencetype *r;
		int i;
		
		cal_component_get_rrule_list (priv->comp, &list);
		r = list->data;
		
		switch (r->freq) {
		case ICAL_MONTHLY_RECURRENCE:
			e_dialog_radio_set (priv->recurrence_rule_monthly, ICAL_MONTHLY_RECURRENCE,
					    recur_options_map);
			
			if (r->by_month_day[0] != SHRT_MAX) {
				e_dialog_toggle_set (priv->recurrence_rule_monthly_on_day, TRUE);
				e_dialog_spin_set (priv->recurrence_rule_monthly_day_nth, 
						   r->by_month_day[0]);
			} else if (r->by_day[0] != SHRT_MAX) {
				e_dialog_toggle_set (priv->recurrence_rule_monthly_weekday, TRUE);
				/* libical does not handle ints in by day */
/*  				e_dialog_option_menu_set (priv->recurrence_rule_monthly_week, */
/*  							  priv->ico->recur->u.month_pos, */
/*  							  month_pos_map); */
/*  				e_dialog_option_menu_set (priv->recurrence_rule_monthly_weekpos, */
/*  							  priv->ico->recur->weekday, */
/*  							  weekday_map); */
			}
			
			e_dialog_spin_set (priv->recurrence_rule_monthly_every_n_months,
					   r->interval);
			break;

		default:
			break;
/*  			g_assert_not_reached (); */
		}

		cal_component_free_recur_list (list);
	}

#endif

	/* Exceptions list */

	cal_component_get_exdate_list (priv->comp, &list);

	for (l = list; l; l = l->next) {
		struct icaltimetype *t;
		time_t ext;
		
		t = l->data;
		ext = icaltime_as_timet (*t);
		append_exception (ee, ext);
	}

	cal_component_free_exdate_list (list);
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

/* Gets the simple recurrence data from the recurrence widgets and stores it in
 * the calendar component object.
 */
static void
simple_recur_to_comp_object (EventEditor *ee)
{
	EventEditorPrivate *priv;
	struct icalrecurrencetype r;
	guint8 day_mask;
	int i;
	GSList l;
	enum ending_type ending_type;

	priv = ee->priv;

	icalrecurrencetype_clear (&r);

	/* Frequency and interval */

	r.freq = e_dialog_option_menu_get (priv->recurrence_interval_unit, recur_freq_map);
	r.interval = e_dialog_spin_get_int (priv->recurrence_interval_value);

	/* Frequency-specific data */

	switch (r.freq) {
	case ICAL_DAILY_RECURRENCE:
		/* Nothing else is required */
		break;

	case ICAL_WEEKLY_RECURRENCE:
		g_assert (GTK_BIN (priv->recurrence_special)->child != NULL);
		g_assert (GTK_BIN (priv->recurrence_special)->child
			  == priv->recurrence_weekday_picker);
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

	case ICAL_MONTHLY_RECURRENCE:
		/* FIXME */
		break;

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
			TRUE, FALSE);
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

	cal_component_set_rrule_list (priv->comp, &l);
}

/* Gets the data from the recurrence widgets and stores it in the calendar
 * component object.
 */
static void
recur_to_comp_object (EventEditor *ee)
{
	EventEditorPrivate *priv;
	enum recur_type recur_type;

	priv = ee->priv;
	g_assert (priv->comp != NULL);

	recur_type = e_dialog_radio_get (priv->recurrence_none, recur_type_map);

	switch (recur_type) {
	case RECUR_NONE:
		cal_component_set_rdate_list (priv->comp, NULL);
		cal_component_set_rrule_list (priv->comp, NULL);
		cal_component_set_exrule_list (priv->comp, NULL);
		break;

	case RECUR_SIMPLE:
		simple_recur_to_comp_object (ee);
		break;

	case RECUR_CUSTOM:
		/* We just keep whatever the component has currently */
		break;

	default:
		g_assert_not_reached ();
	}
}

/* Gets the data from the widgets and stores it in the calendar component object */
static void
dialog_to_comp_object (EventEditor *ee)
{
	EventEditorPrivate *priv;
	CalComponent *comp;
	CalComponentText *text;
	CalComponentDateTime date;
	time_t t;
	gboolean all_day_event;
	GtkCList *exception_list;
	GSList *list;
	int i;
	
	priv = ee->priv;
	g_assert (priv->comp != NULL);

	comp = priv->comp;

	text = g_new0 (CalComponentText, 1);
	text->value = e_dialog_editable_get (priv->general_summary);
	cal_component_set_summary (comp, text);

	list = NULL;
	text->value  = e_dialog_editable_get (priv->description);
	list = g_slist_prepend (list, text);
	cal_component_set_description_list (comp, list);
	cal_component_free_text_list (list);
	
	date.value = g_new (struct icaltimetype, 1);
	t = e_date_edit_get_time (E_DATE_EDIT (priv->start_time));
	*date.value = icaltime_from_timet (t, FALSE, FALSE);
	date.tzid = NULL;
	cal_component_set_dtstart (comp, &date);

	/* If the all_day toggle is set, the end date is inclusive of the
	   entire day on which it points to. */
	all_day_event = e_dialog_toggle_get (priv->all_day_event);
	t = e_date_edit_get_time (E_DATE_EDIT (priv->end_time));
	if (all_day_event)
		t = time_day_end (t);

	*date.value = icaltime_from_timet (t, FALSE, FALSE);
	cal_component_set_dtend (comp, &date);
	g_free (date.value);

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

	cal_component_set_classification (comp, classification_get (priv->classification_radio));

	/* Recurrence information */

	recur_to_comp_object (ee);

#if 0
	switch (recur.freq) {
	case ICAL_MONTHLY_RECURRENCE:

		if (e_dialog_toggle_get (priv->recurrence_rule_monthly_on_day)) {
				/* by day of in the month (ex: the 5th) */
			recur.by_month_day[0] = 
				e_dialog_spin_get_int (priv->recurrence_rule_monthly_day_nth);
		} else if (e_dialog_toggle_get (priv->recurrence_rule_monthly_weekday)) {

/* "recurrence-rule-monthly-weekday" is TRUE */
				/* by position on the calendar (ex: 2nd monday) */
			/* libical does not handle this yet */
/*  			ico->recur->u.month_pos = e_dialog_option_menu_get ( */
/*  				priv->recurrence_rule_monthly_week, */
/*  				month_pos_map); */
/*  			ico->recur->weekday = e_dialog_option_menu_get ( */
/*  				priv->recurrence_rule_monthly_weekpos, */
/*  				weekday_map); */

		} else
			g_assert_not_reached ();

		recur.interval = e_dialog_spin_get_int (priv->recurrence_rule_monthly_every_n_months);

		break;

	default:
		break;
/*  		g_assert_not_reached (); */
	}

	if (recur.freq != ICAL_NO_RECURRENCE) {
		/* recurrence start of week */
		if (week_starts_on_monday)
			recur.week_start = ICAL_MONDAY_WEEKDAY;
		else
			recur.week_start = ICAL_SUNDAY_WEEKDAY;

		list = NULL;
		list = g_slist_append (list, &recur);
		cal_component_set_rrule_list (comp, list);
		g_slist_free (list);
	} else {
		list = NULL;
		cal_component_set_rrule_list (comp, list);		
	}
#endif	
	/* Set exceptions */

	list = NULL;
	exception_list = GTK_CLIST (priv->recurrence_exception_list);
	for (i = 0; i < exception_list->rows; i++) {
		struct icaltimetype *tt;
		time_t *t;
		
		t = gtk_clist_get_row_data (exception_list, i);
		tt = g_new0 (struct icaltimetype, 1);
		*tt = icaltime_from_timet (*t, FALSE, FALSE);
		
		list = g_slist_prepend (list, tt);
	}
	cal_component_set_exdate_list (comp, list);
	if (list)
		cal_component_free_exdate_list (list);

	cal_component_commit_sequence (comp);
}

/* Fills the calendar component object from the data in the widgets and commits
 * the component to the storage.
 */
static void
save_event_object (EventEditor *ee)
{
	EventEditorPrivate *priv;
	char *title;

	priv = ee->priv;

	if (!priv->comp)
		return;

	dialog_to_comp_object (ee);

	title = make_title_from_comp (priv->comp);
	gtk_window_set_title (GTK_WINDOW (priv->app), title);
	g_free (title);

	if (!cal_client_update_object (priv->client, priv->comp))
		g_message ("save_event_object(): Could not update the object!");
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
debug_xml_cb (GtkWidget *widget, gpointer data)
{
	EventEditor *ee = EVENT_EDITOR (data);
	EventEditorPrivate *priv = ee->priv;
	
	bonobo_win_dump (BONOBO_WIN (priv->app), "on demand");
}

/* File/Save callback */
static void
file_save_cb (GtkWidget *widget, gpointer data)
{
	EventEditor *ee;

	ee = EVENT_EDITOR (data);
	save_event_object (ee);
}

/* File/Save and Close callback */
static void
file_save_and_close_cb (GtkWidget *widget, gpointer data)
{
	EventEditor *ee;

	ee = EVENT_EDITOR (data);
	save_event_object (ee);
	close_dialog (ee);
}

/* File/Delete callback */
static void
file_delete_cb (GtkWidget *widget, gpointer data)
{
	EventEditor *ee;
	EventEditorPrivate *priv;
	const char *uid;
	
	ee = EVENT_EDITOR (data);

	g_return_if_fail (IS_EVENT_EDITOR (ee));

	priv = ee->priv;
	
	g_return_if_fail (priv->comp);

	cal_component_get_uid (priv->comp, &uid);

	/* We don't check the return value; FALSE can mean the object was not in
	 * the server anyways.
	 */
	cal_client_remove_object (priv->client, uid);

	close_dialog (ee);
}

/* File/Close callback */
static void
file_close_cb (GtkWidget *widget, gpointer data)
{
	EventEditor *ee;

	ee = EVENT_EDITOR (data);

	g_return_if_fail (IS_EVENT_EDITOR (ee));

	close_dialog (ee);
}

static void
schedule_meeting_cb (GtkWidget *widget, gpointer data)
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
	BONOBO_UI_UNSAFE_VERB ("FileSave", file_save_cb),
	BONOBO_UI_UNSAFE_VERB ("FileDelete", file_delete_cb),
	BONOBO_UI_UNSAFE_VERB ("FileClose", file_close_cb),
	BONOBO_UI_UNSAFE_VERB ("FileSaveAndClose", file_save_and_close_cb),

	BONOBO_UI_UNSAFE_VERB ("ActionScheduleMeeting", schedule_meeting_cb),

	BONOBO_UI_UNSAFE_VERB ("DebugDumpXml", debug_xml_cb),

	BONOBO_UI_VERB_END
};



/* Callback used when the dialog box is destroyed */
static gint
app_delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	EventEditor *ee;

	/* FIXME: need to check for a dirty object */

	ee = EVENT_EDITOR (data);
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
	bonobo_win = bonobo_win_new ("event-editor-dialog", "Event Editor");

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
		bonobo_win_set_contents (BONOBO_WIN (bonobo_win), contents);
		gtk_widget_destroy (priv->app);
		priv->app = bonobo_win;
	}

	{
		BonoboUIContainer *container = bonobo_ui_container_new ();
		bonobo_ui_container_set_win (container, BONOBO_WIN (priv->app));
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
		g_return_if_fail (cal_client_is_loaded (client));	
	
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
	char *title;

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

	title = make_title_from_comp (priv->comp);
	gtk_window_set_title (GTK_WINDOW (priv->app), title);
	g_free (title);

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

static void
alarm_toggle (GtkWidget *toggle, EventEditor *ee)
{
	EventEditorPrivate *priv;
	GtkWidget *alarm_amount = NULL;
	GtkWidget *alarm_unit = NULL;
	gboolean active;

	priv = ee->priv;

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

/*
 * Checks if the day range occupies all the day, and if so, check the
 * box accordingly
 */
static void
check_all_day (EventEditor *ee)
{
	EventEditorPrivate *priv;
	time_t ev_start, ev_end;

	priv = ee->priv;

	ev_start = e_date_edit_get_time (E_DATE_EDIT (priv->start_time));
	ev_end = e_date_edit_get_time (E_DATE_EDIT (priv->end_time));

	/* all day event checkbox */
	if (time_day_begin (ev_start) == ev_start
	    && time_day_begin (ev_end) == ev_end)
		e_dialog_toggle_set (priv->all_day_event, TRUE);
	else
		e_dialog_toggle_set (priv->all_day_event, FALSE);
}

/*
 * Callback: all day event box clicked
 */
static void
set_all_day (GtkWidget *toggle, EventEditor *ee)
{
	EventEditorPrivate *priv;
	struct tm start_tm, end_tm;
	time_t start_t, end_t;
	gboolean all_day;

	priv = ee->priv;

	/* If the all_day toggle is set, the end date is inclusive of the
	   entire day on which it points to. */
	all_day = GTK_TOGGLE_BUTTON (toggle)->active;

	start_t = e_date_edit_get_time (E_DATE_EDIT (priv->start_time));
	start_tm = *localtime (&start_t);
	start_tm.tm_min  = 0;
	start_tm.tm_sec  = 0;

	if (all_day)
		start_tm.tm_hour = 0;
	else
		start_tm.tm_hour = day_begin;

	e_date_edit_set_time (E_DATE_EDIT (priv->start_time),
			      mktime (&start_tm));

	end_t = e_date_edit_get_time (E_DATE_EDIT (priv->end_time));
	end_tm = *localtime (&end_t);
	end_tm.tm_min  = 0;
	end_tm.tm_sec  = 0;

	if (all_day) {
		/* mktime() will fix this if we go past the end of the month.*/
		end_tm.tm_hour = 0;
	} else {
		if (end_tm.tm_year == start_tm.tm_year
		    && end_tm.tm_mon == start_tm.tm_mon
		    && end_tm.tm_mday == start_tm.tm_mday
		    && end_tm.tm_hour <= start_tm.tm_hour)
			end_tm.tm_hour = start_tm.tm_hour + 1;
	}

	e_date_edit_set_show_time (E_DATE_EDIT (priv->start_time), !all_day);
	e_date_edit_set_show_time (E_DATE_EDIT (priv->end_time), !all_day);
	e_date_edit_set_time (E_DATE_EDIT (priv->end_time), mktime (&end_tm));
}

/*
 * Callback: checks that the dates are start < end
 */
static void
check_dates (EDateEdit *dedit, EventEditor *ee)
{
	EventEditorPrivate *priv;
	time_t start, end;
	struct tm tm_start, tm_end;

	priv = ee->priv;

	start = e_date_edit_get_time (E_DATE_EDIT (priv->start_time));
	end = e_date_edit_get_time (E_DATE_EDIT (priv->end_time));

	if (start > end) {
		tm_start = *localtime (&start);
		tm_end = *localtime (&end);

		if (GTK_WIDGET (dedit) == priv->start_time) {
			tm_end.tm_year = tm_start.tm_year;
			tm_end.tm_mon  = tm_start.tm_mon;
			tm_end.tm_mday = tm_start.tm_mday;

			gtk_signal_handler_block_by_data (GTK_OBJECT (priv->end_time), ee);
			e_date_edit_set_time (E_DATE_EDIT (priv->end_time),
					      mktime (&tm_end));
			gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->end_time), ee);
		} else if (GTK_WIDGET (dedit) == priv->end_time) {
			tm_start.tm_year = tm_end.tm_year;
			tm_start.tm_mon  = tm_end.tm_mon;
			tm_start.tm_mday = tm_end.tm_mday;

#if 0
			gtk_signal_handler_block_by_data (GTK_OBJECT (priv->start_time), ee);
			e_date_edit_set_time (E_DATE_EDIT (priv->start_time),
					      mktime (&tm_start));
			gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->start_time), ee);
#endif
		}
	}
}

/*
 * Callback: checks that start_time < end_time and whether the
 * selected hour range spans all of the day
 */
static void
check_times (EDateEdit *dedit, EventEditor *ee)
{
	EventEditorPrivate *priv;
	time_t start, end;
	struct tm tm_start, tm_end;

	priv = ee->priv;
#if 0
	gdk_pointer_ungrab (GDK_CURRENT_TIME);
	gdk_flush ();
#endif
	start = e_date_edit_get_time (E_DATE_EDIT (priv->start_time));
	end = e_date_edit_get_time (E_DATE_EDIT (priv->end_time));

	if (start > end) {
		tm_start = *localtime (&start);
		tm_end = *localtime (&end);

		if (GTK_WIDGET (dedit) == priv->start_time) {
			tm_end.tm_min  = tm_start.tm_min;
			tm_end.tm_sec  = tm_start.tm_sec;
			tm_end.tm_hour = tm_start.tm_hour + 1;

			if (tm_end.tm_hour >= 24) {
				tm_end.tm_hour = 24; /* mktime() will bump the day */
				tm_end.tm_min = 0;
				tm_end.tm_sec = 0;
			}

			gtk_signal_handler_block_by_data (GTK_OBJECT (priv->end_time), ee);
			e_date_edit_set_time (E_DATE_EDIT (priv->end_time),
					      mktime (&tm_end));
			gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->end_time), ee);
		} else if (GTK_WIDGET (dedit) == priv->end_time) {
			tm_start.tm_min  = tm_end.tm_min;
			tm_start.tm_sec  = tm_end.tm_sec;
			tm_start.tm_hour = tm_end.tm_hour - 1;

			if (tm_start.tm_hour < 0) {
				tm_start.tm_hour = 0;
				tm_start.tm_min = 0;
				tm_start.tm_min = 0;
			}

			gtk_signal_handler_block_by_data (GTK_OBJECT (priv->start_time), ee);
			e_date_edit_set_time (E_DATE_EDIT (priv->start_time),
					      mktime (&tm_start));
			gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->start_time), ee);
		}
	}

	/* Check whether the event spans the whole day */

	check_all_day (ee);
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

	t = e_date_edit_get_time (E_DATE_EDIT (priv->recurrence_exception_date));
	append_exception (ee, t);
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

	sel = GPOINTER_TO_INT (clist->selection->data);

	t = gtk_clist_get_row_data (clist, sel);
	*t = e_date_edit_get_time (E_DATE_EDIT (priv->recurrence_exception_date));

	e_utf8_gtk_clist_set_text (clist, sel, 0, get_exception_string (*t));
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
}


GtkWidget *
make_date_edit (void)
{
	return date_edit_new (time (NULL), FALSE);
}


GtkWidget *
make_date_edit_with_time (void)
{
	return date_edit_new (time (NULL), TRUE);
}


GtkWidget *
date_edit_new (time_t the_time, int show_time)
{
	GtkWidget *dedit;

	dedit = e_date_edit_new ();
	/* FIXME: Set other options. */
	e_date_edit_set_show_time (E_DATE_EDIT (dedit), show_time);
	e_date_edit_set_time_popup_range (E_DATE_EDIT (dedit), 8, 18);
	return dedit;
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


/* todo

   get the apply button to work right

   make the properties stuff unglobal

   figure out why alarm units aren't sticking between edits

   closing the dialog window with the wm caused a crash
   Gtk-WARNING **: invalid cast from `(unknown)' to `GnomeDialog'
   on line 669:  gnome_dialog_close (GNOME_DIALOG(dialog->dialog));
 */
