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
#include <cal-util/timeutil.h>
#include "event-editor.h"



typedef struct {
	/* Glade XML data */
	GladeXML *xml;

	/* UI handler */
	BonoboUIHandler *uih;

	/* Calendar object we are editing; this is an internal copy and is not
	 * one of the read-only objects from the parent calendar.
	 */
	iCalObject *ico;

	/* Widgets from the Glade file */

	GtkWidget *app;

	GtkWidget *general_owner;
	GtkWidget *general_summary;

	GtkWidget *start_time;
	GtkWidget *end_time;
	GtkWidget *all_day_event;

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

	GtkWidget *recurrence_ending_date_repeat_forever;
	GtkWidget *recurrence_ending_date_end_on;
	GtkWidget *recurrence_ending_date_end_on_date;
	GtkWidget *recurrence_ending_date_end_after;
	GtkWidget *recurrence_ending_date_end_after_count;

	GtkWidget *recurrence_exceptions_date;
	GtkWidget *recurrence_exceptions_list;
	GtkWidget *recurrence_exception_add;
	GtkWidget *recurrence_exception_delete;
	GtkWidget *recurrence_exception_change;

	GtkWidget *exception_list;
	GtkWidget *exception_date;
} EventEditorPrivate;



/* Signal IDs */
enum {
	ICAL_OBJECT_RELEASED,
	LAST_SIGNAL
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
static void check_dates (GnomeDateEdit *gde, EventEditor *ee);
static void check_times (GnomeDateEdit *gde, EventEditor *ee);
static void recurrence_toggled (GtkWidget *radio, EventEditor *ee);
static void recurrence_exception_added (GtkWidget *widget, EventEditor *ee);
static void recurrence_exception_deleted (GtkWidget *widget, EventEditor *ee);
static void recurrence_exception_changed (GtkWidget *widget, EventEditor *ee);

static guint event_editor_signals[LAST_SIGNAL];



/**
 * event_editor_get_type:
 * @void:
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

	event_editor_signals[ICAL_OBJECT_RELEASED] =
		gtk_signal_new ("ical_object_released",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EventEditorClass, ical_object_released),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, event_editor_signals, LAST_SIGNAL);

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

	if (priv->ico) {
		gtk_signal_emit (GTK_OBJECT (ee), event_editor_signals[ICAL_OBJECT_RELEASED],
				 priv->ico->uid);
		ical_object_unref (priv->ico);
		priv->ico = NULL;
	}

	if (priv->uih) {
		bonobo_object_unref (BONOBO_OBJECT (priv->uih));
		priv->uih = NULL;
	}

	if (priv->app) {
		gtk_widget_destroy (priv->app);
		priv->app = NULL;
	}

	if (priv->xml) {
		gtk_object_unref (GTK_OBJECT (priv->xml));
		priv->xml = NULL;
	}

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/* Creates an appropriate title for the event editor dialog */
static char *
make_title_from_ico (iCalObject *ico)
{
	const char *summary;

	if (!ico)
		return g_strdup (_("Edit Appointment"));

	if (ico->summary)
		summary = ico->summary;
	else
		summary =  _("No summary");

	switch (ico->type) {
	case ICAL_EVENT:
		return g_strdup_printf (_("Appointment - %s"), summary);

	case ICAL_TODO:
		return g_strdup_printf (_("Task - %s"), summary);

	case ICAL_JOURNAL:
		return g_strdup_printf (_("Journal entry - %s"), summary);

	default:
		g_message ("make_title_from_ico(): Cannot handle object of type %d", (int) ico->type);
		return NULL;
	}
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

	priv->general_owner = GW ("general-owner");
	priv->general_summary = GW ("general-summary");

	priv->start_time = GW ("start-time");
	priv->end_time = GW ("end-time");
	priv->all_day_event = GW ("all-day-event");

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

	priv->recurrence_rule_notebook = GW ("recurrence-rule-notebook");
	priv->recurrence_rule_none = GW ("recurrence-rule-none");
	priv->recurrence_rule_daily = GW ("recurrence-rule-daily");
	priv->recurrence_rule_weekly = GW ("recurrence-rule-weekly");
	priv->recurrence_rule_monthly = GW ("recurrence-rule-monthly");
	priv->recurrence_rule_yearly = GW ("recurrence-rule-yearly");

	priv->recurrence_rule_daily_days = GW ("recurrence-rule-daily-days");

	priv->recurrence_rule_weekly_weeks = GW ("recurrence-rule-weekly-weeks");
	priv->recurrence_rule_weekly_sun = GW ("recurrence-rule-weekly-sun");
	priv->recurrence_rule_weekly_mon = GW ("recurrence-rule-weekly-mon");
	priv->recurrence_rule_weekly_tue = GW ("recurrence-rule-weekly-tue");
	priv->recurrence_rule_weekly_wed = GW ("recurrence-rule-weekly-wed");
	priv->recurrence_rule_weekly_thu = GW ("recurrence-rule-weekly-thu");
	priv->recurrence_rule_weekly_fri = GW ("recurrence-rule-weekly-fri");
	priv->recurrence_rule_weekly_sat = GW ("recurrence-rule-weekly-sat");

	priv->recurrence_rule_monthly_on_day = GW ("recurrence-rule-monthly-on-day");
	priv->recurrence_rule_monthly_weekday = GW ("recurrence-rule-monthly-weekday");
	priv->recurrence_rule_monthly_day_nth = GW ("recurrence-rule-monthly-day-nth");
	priv->recurrence_rule_monthly_week = GW ("recurrence-rule-monthly-week");
	priv->recurrence_rule_monthly_weekpos = GW ("recurrence-rule-monthly-weekpos");
	priv->recurrence_rule_monthly_every_n_months = GW ("recurrence-rule-monthly-every-n-months");
	priv->recurrence_rule_yearly_every_n_years = GW ("recurrence-rule-yearly-every-n-years");

	priv->recurrence_ending_date_repeat_forever = GW ("recurrence-ending-date-repeat-forever");
	priv->recurrence_ending_date_end_on = GW ("recurrence-ending-date-end-on");
	priv->recurrence_ending_date_end_on_date = GW ("recurrence-ending-date-end-on-date");
	priv->recurrence_ending_date_end_after = GW ("recurrence-ending-date-end-after");
	priv->recurrence_ending_date_end_after_count = GW ("recurrence-ending-date-end-after-count");

	priv->recurrence_exceptions_date = GW ("recurrence-exceptions-date");
	priv->recurrence_exceptions_list = GW ("recurrence-exceptions-list");
	priv->recurrence_exception_add = GW ("recurrence-exceptions-add");
	priv->recurrence_exception_delete = GW ("recurrence-exceptions-delete");
	priv->recurrence_exception_change = GW ("recurrence-exceptions-change");

	priv->exception_list = GW ("recurrence-exceptions-list");
	priv->exception_date = GW ("recurrence-exceptions-date");

#undef GW

	return (priv->general_owner
		&& priv->general_summary
		&& priv->start_time
		&& priv->end_time
		&& priv->all_day_event
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
		&& priv->recurrence_rule_notebook
		&& priv->recurrence_rule_none
		&& priv->recurrence_rule_daily
		&& priv->recurrence_rule_weekly
		&& priv->recurrence_rule_monthly
		&& priv->recurrence_rule_yearly
		&& priv->recurrence_rule_daily_days
		&& priv->recurrence_rule_weekly_weeks
		&& priv->recurrence_rule_monthly_on_day
		&& priv->recurrence_rule_monthly_weekday
		&& priv->recurrence_rule_monthly_day_nth
		&& priv->recurrence_rule_monthly_week
		&& priv->recurrence_rule_monthly_weekpos
		&& priv->recurrence_rule_monthly_every_n_months
		&& priv->recurrence_rule_yearly_every_n_years
		&& priv->recurrence_ending_date_repeat_forever
		&& priv->recurrence_ending_date_end_on
		&& priv->recurrence_ending_date_end_on_date
		&& priv->recurrence_ending_date_end_after
		&& priv->recurrence_ending_date_end_after_count
		&& priv->recurrence_exceptions_date
		&& priv->recurrence_exceptions_list
		&& priv->recurrence_exception_add
		&& priv->recurrence_exception_delete
		&& priv->recurrence_exception_change
		&& priv->exception_list
		&& priv->exception_date);
}

/* Classification types; just an enum for mapping them to radio buttons
 * (iCalObject uses strings for these).
 */
typedef enum {
	CLASSIFICATION_PUBLIC,
	CLASSIFICATION_PRIVATE,
	CLASSIFICATION_CONFIDENTIAL
} ClassificationType;

static const int classification_map[] = {
	CLASSIFICATION_PUBLIC,
	CLASSIFICATION_PRIVATE,
	CLASSIFICATION_CONFIDENTIAL,
	-1
};

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

/* Recurrence types for mapping them to radio buttons */
typedef enum {
	RECUR_OPTION_NONE,
	RECUR_OPTION_DAILY,
	RECUR_OPTION_WEEKLY,
	RECUR_OPTION_MONTHLY,
	RECUR_OPTION_YEARLY,
} RecurOptions;

static const int recur_options_map[] = {
	RECUR_OPTION_NONE,
	RECUR_OPTION_DAILY,
	RECUR_OPTION_WEEKLY,
	RECUR_OPTION_MONTHLY,
	RECUR_OPTION_YEARLY,
	-1
};

static RecurOptions
recur_options_get (GtkWidget *widget)
{
	return e_dialog_radio_get (widget, recur_options_map);
}

static const int month_pos_map[] = { 0, 1, 2, 3, 4, -1 };
static const int weekday_map[] = { 0, 1, 2, 3, 4, 5, 6, -1 };

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

/* Callback used when the exception date GtkCList is destroyed */
static void
exception_clist_destroyed (GtkObject *object, gpointer data)
{
	EventEditor *ee;
	EventEditorPrivate *priv;

	ee = EVENT_EDITOR (data);
	priv = ee->priv;

	free_exception_clist_data (GTK_CLIST (priv->recurrence_exceptions_list));
}

/* Hooks the widget signals */
static void
init_widgets (EventEditor *ee)
{
	EventEditorPrivate *priv;

	priv = ee->priv;

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

	gtk_signal_connect (GTK_OBJECT (priv->recurrence_rule_none), "toggled",
			    GTK_SIGNAL_FUNC (recurrence_toggled), ee);
	gtk_signal_connect (GTK_OBJECT (priv->recurrence_rule_daily), "toggled",
			    GTK_SIGNAL_FUNC (recurrence_toggled), ee);
	gtk_signal_connect (GTK_OBJECT (priv->recurrence_rule_weekly), "toggled",
			    GTK_SIGNAL_FUNC (recurrence_toggled), ee);
	gtk_signal_connect (GTK_OBJECT (priv->recurrence_rule_monthly), "toggled",
			    GTK_SIGNAL_FUNC (recurrence_toggled), ee);
	gtk_signal_connect (GTK_OBJECT (priv->recurrence_rule_yearly), "toggled",
			    GTK_SIGNAL_FUNC (recurrence_toggled), ee);

	/* Exception buttons */

	gtk_signal_connect (GTK_OBJECT (priv->recurrence_exception_add), "clicked",
			    GTK_SIGNAL_FUNC (recurrence_exception_added), ee);
	gtk_signal_connect (GTK_OBJECT (priv->recurrence_exception_delete), "clicked",
			    GTK_SIGNAL_FUNC (recurrence_exception_deleted), ee);
	gtk_signal_connect (GTK_OBJECT (priv->recurrence_exception_change), "clicked",
			    GTK_SIGNAL_FUNC (recurrence_exception_changed), ee);

	/* Exception list */

	gtk_signal_connect (GTK_OBJECT (priv->recurrence_exceptions_list), "destroy",
			    GTK_SIGNAL_FUNC (exception_clist_destroyed), ee);
}

/* Fills the widgets with default values */
static void
clear_widgets (EventEditor *ee)
{
	EventEditorPrivate *priv;
	time_t now;

	priv = ee->priv;

	now = time (NULL);

	/* Owner, summary */

	e_dialog_editable_set (priv->general_owner, _("?"));
	e_dialog_editable_set (priv->general_summary, NULL);

	/* Start and end times */

	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->start_time), ee);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->end_time), ee);

	e_dialog_dateedit_set (priv->start_time, now);
	e_dialog_dateedit_set (priv->end_time, now);

	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->start_time), ee);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->end_time), ee);

	check_all_day (ee);

	/* Alarms */

	/* FIXMe: these should use configurable defaults */

	e_dialog_toggle_set (priv->alarm_display, FALSE);
	e_dialog_toggle_set (priv->alarm_program, FALSE);
	e_dialog_toggle_set (priv->alarm_audio, FALSE);
	e_dialog_toggle_set (priv->alarm_mail, FALSE);

	e_dialog_spin_set (priv->alarm_display_amount, 15);
	e_dialog_spin_set (priv->alarm_audio_amount, 15);
	e_dialog_spin_set (priv->alarm_program_amount, 15);
	e_dialog_spin_set (priv->alarm_mail_amount, 15);

	alarm_unit_set (priv->alarm_display_unit, ALARM_MINUTES);
	alarm_unit_set (priv->alarm_audio_unit, ALARM_MINUTES);
	alarm_unit_set (priv->alarm_program_unit, ALARM_MINUTES);
	alarm_unit_set (priv->alarm_mail_unit, ALARM_MINUTES);

	e_dialog_editable_set (priv->alarm_program_run_program_entry, NULL);
	e_dialog_editable_set (priv->alarm_mail_mail_to, NULL);

	/* Classification */

	e_dialog_radio_set (priv->classification_radio, CLASSIFICATION_PRIVATE, classification_map);

	/* Recurrences */

	e_dialog_radio_set (priv->recurrence_rule_none, RECUR_OPTION_NONE, recur_options_map);

	e_dialog_spin_set (priv->recurrence_rule_daily_days, 1);

	e_dialog_spin_set (priv->recurrence_rule_weekly_weeks, 1);
	e_dialog_toggle_set (priv->recurrence_rule_weekly_sun, FALSE);
	e_dialog_toggle_set (priv->recurrence_rule_weekly_mon, FALSE);
	e_dialog_toggle_set (priv->recurrence_rule_weekly_tue, FALSE);
	e_dialog_toggle_set (priv->recurrence_rule_weekly_wed, FALSE);
	e_dialog_toggle_set (priv->recurrence_rule_weekly_thu, FALSE);
	e_dialog_toggle_set (priv->recurrence_rule_weekly_fri, FALSE);
	e_dialog_toggle_set (priv->recurrence_rule_weekly_sat, FALSE);

	e_dialog_toggle_set (priv->recurrence_rule_monthly_on_day, TRUE);
	e_dialog_spin_set (priv->recurrence_rule_monthly_day_nth, 1);
	e_dialog_spin_set (priv->recurrence_rule_monthly_every_n_months, 1);
	e_dialog_option_menu_set (priv->recurrence_rule_monthly_week, 0, month_pos_map);
	e_dialog_option_menu_set (priv->recurrence_rule_monthly_weekpos, 0, weekday_map);
	e_dialog_spin_set (priv->recurrence_rule_monthly_every_n_months, 1);

	e_dialog_spin_set (priv->recurrence_rule_yearly_every_n_years, 1);

	e_dialog_toggle_set (priv->recurrence_ending_date_repeat_forever, TRUE);
	e_dialog_spin_set (priv->recurrence_ending_date_end_after_count, 1);
	e_dialog_dateedit_set (priv->recurrence_ending_date_end_on_date,
			       time_add_day (time (NULL), 1));

	/* Exceptions list */

	free_exception_clist_data (GTK_CLIST (priv->recurrence_exceptions_list));
}

/* Fills in the widgets with the proper values */
static void
fill_widgets (EventEditor *ee)
{
	EventEditorPrivate *priv;
	GList *list;

	priv = ee->priv;

	clear_widgets (ee);

	if (!priv->ico)
		return;

	/* Owner, summary */

	e_dialog_editable_set (priv->general_owner,
			       priv->ico->organizer->addr ?
			       priv->ico->organizer->addr : _("?"));

	e_dialog_editable_set (priv->general_summary, priv->ico->summary);

	/* Start and end times */

	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->start_time), ee);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->end_time), ee);

	e_dialog_dateedit_set (priv->start_time, priv->ico->dtstart);
	e_dialog_dateedit_set (priv->end_time, priv->ico->dtend);

	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->start_time), ee);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->end_time), ee);

	check_all_day (ee);

	/* Alarms */

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

	/* Classification */

	if (strcmp (priv->ico->class, "PUBLIC") == 0)
	    	e_dialog_radio_set (priv->classification_radio, CLASSIFICATION_PUBLIC,
				    classification_map);
	else if (strcmp (priv->ico->class, "PRIVATE") == 0)
	    	e_dialog_radio_set (priv->classification_radio, CLASSIFICATION_PRIVATE,
				    classification_map);
	else if (strcmp (priv->ico->class, "CONFIDENTIAL") == 0)
	    	e_dialog_radio_set (priv->classification_radio, CLASSIFICATION_CONFIDENTIAL,
				    classification_map);
	else {
		/* What do do?  We can't g_assert_not_reached() since it is a
		 * value from an external file.
		 */
	}

	/* Recurrences */

	if (priv->ico->recur) {
		switch (priv->ico->recur->type) {
		case RECUR_DAILY:
			e_dialog_radio_set (priv->recurrence_rule_daily, RECUR_OPTION_DAILY,
					    recur_options_map);
			e_dialog_spin_set (priv->recurrence_rule_daily_days,
					   priv->ico->recur->interval);
			break;

		case RECUR_WEEKLY:
			e_dialog_radio_set (priv->recurrence_rule_weekly, RECUR_OPTION_WEEKLY,
					    recur_options_map);
			e_dialog_spin_set (priv->recurrence_rule_weekly_weeks,
					   priv->ico->recur->interval);

			e_dialog_toggle_set (priv->recurrence_rule_weekly_sun,
					     (priv->ico->recur->weekday & (1 << 0)) != 0);
			e_dialog_toggle_set (priv->recurrence_rule_weekly_mon,
					     (priv->ico->recur->weekday & (1 << 1)) != 0);
			e_dialog_toggle_set (priv->recurrence_rule_weekly_tue,
					     (priv->ico->recur->weekday & (1 << 2)) != 0);
			e_dialog_toggle_set (priv->recurrence_rule_weekly_wed,
					     (priv->ico->recur->weekday & (1 << 3)) != 0);
			e_dialog_toggle_set (priv->recurrence_rule_weekly_thu,
					     (priv->ico->recur->weekday & (1 << 4)) != 0);
			e_dialog_toggle_set (priv->recurrence_rule_weekly_fri,
					     (priv->ico->recur->weekday & (1 << 5)) != 0);
			e_dialog_toggle_set (priv->recurrence_rule_weekly_sat,
					     (priv->ico->recur->weekday & (1 << 6)) != 0);
			break;

		case RECUR_MONTHLY_BY_DAY:
			e_dialog_radio_set (priv->recurrence_rule_monthly, RECUR_OPTION_MONTHLY,
					    recur_options_map);
			e_dialog_toggle_set (priv->recurrence_rule_monthly_on_day, TRUE);
			e_dialog_spin_set (priv->recurrence_rule_monthly_day_nth,
					   priv->ico->recur->u.month_day);
			e_dialog_spin_set (priv->recurrence_rule_monthly_every_n_months,
					   priv->ico->recur->interval);
			break;

		case RECUR_MONTHLY_BY_POS:
			e_dialog_radio_set (priv->recurrence_rule_monthly, RECUR_OPTION_MONTHLY,
					    recur_options_map);
			e_dialog_toggle_set (priv->recurrence_rule_monthly_weekday, TRUE);
			e_dialog_option_menu_set (priv->recurrence_rule_monthly_week,
						  priv->ico->recur->u.month_pos,
						  month_pos_map);
			e_dialog_option_menu_set (priv->recurrence_rule_monthly_weekpos,
						  priv->ico->recur->weekday,
						  weekday_map);
			e_dialog_spin_set (priv->recurrence_rule_monthly_every_n_months,
					   priv->ico->recur->interval);
			break;

		case RECUR_YEARLY_BY_DAY:
		case RECUR_YEARLY_BY_MONTH:
			e_dialog_radio_set (priv->recurrence_rule_yearly, RECUR_OPTION_YEARLY,
					    recur_options_map);
			e_dialog_spin_set (priv->recurrence_rule_yearly_every_n_years,
					   priv->ico->recur->interval);
			break;

		default:
			g_assert_not_reached ();
		}

		if (priv->ico->recur->_enddate == 0) {
			if (priv->ico->recur->duration == 0)
				e_dialog_toggle_set (priv->recurrence_ending_date_repeat_forever,
						     TRUE);
			else {
				e_dialog_toggle_set (priv->recurrence_ending_date_end_after, TRUE);
				e_dialog_spin_set (priv->recurrence_ending_date_end_after_count,
						   priv->ico->recur->duration);
			}
		} else {
			e_dialog_toggle_set (priv->recurrence_ending_date_end_on, TRUE);
			/* Shorten by one day, as we store end-on date a day ahead */
			/* FIXME is this correct? */
			e_dialog_dateedit_set (priv->recurrence_ending_date_end_on_date,
					       time_add_day (priv->ico->recur->enddate, -1));
		}
	}

	/* Exceptions list */

	for (list = priv->ico->exdate; list; list = list->next)
		append_exception (ee, *((time_t *) list->data));
}

/* Menu bar */

static GnomeUIInfo file_new_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Appointment"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Meeting Re_quest"), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Mail Message"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Contact"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Task"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Task _Request"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Journal Entry"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Note"), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Ch_oose Form..."), NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo file_page_setup_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Memo Style"), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Define Print _Styles..."), NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo file_menu[] = {
	GNOMEUIINFO_MENU_NEW_SUBTREE (file_new_menu),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: S_end"), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_SAVE_ITEM (NULL, NULL),
	GNOMEUIINFO_MENU_SAVE_AS_ITEM (NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Save Attac_hments..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Delete"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Move to Folder..."), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Cop_y to Folder..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_SUBTREE (N_("Page Set_up"), file_page_setup_menu),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Print Pre_view"), NULL, NULL),
	GNOMEUIINFO_MENU_PRINT_ITEM (NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_PROPERTIES_ITEM (NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_CLOSE_ITEM (NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo edit_object_menu[] = {
	GNOMEUIINFO_ITEM_NONE ("FIXME: what goes here?", NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo edit_menu[] = {
	GNOMEUIINFO_MENU_UNDO_ITEM (NULL, NULL),
	GNOMEUIINFO_MENU_REDO_ITEM (NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_CUT_ITEM (NULL, NULL),
	GNOMEUIINFO_MENU_COPY_ITEM (NULL, NULL),
	GNOMEUIINFO_MENU_PASTE_ITEM (NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Paste _Special..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_CLEAR_ITEM (NULL, NULL),
	GNOMEUIINFO_MENU_SELECT_ALL_ITEM (NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Mark as U_nread"), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_FIND_ITEM (NULL, NULL),
	GNOMEUIINFO_MENU_FIND_AGAIN_ITEM (NULL, NULL),
	GNOMEUIINFO_SUBTREE (N_("_Object"), edit_object_menu),
	GNOMEUIINFO_END
};

static GnomeUIInfo view_previous_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Item"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Unread Item"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Fi_rst Item in Folder"), NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo view_next_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Item"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Unread Item"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Last Item in Folder"), NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo view_toolbars_menu[] = {
	{ GNOME_APP_UI_TOGGLEITEM, N_("FIXME: _Standard"), NULL, NULL, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
	{ GNOME_APP_UI_TOGGLEITEM, N_("FIXME: __Formatting"), NULL, NULL, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Customize..."), NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo view_menu[] = {
	GNOMEUIINFO_SUBTREE (N_("Pre_vious"), view_previous_menu),
	GNOMEUIINFO_SUBTREE (N_("Ne_xt"), view_next_menu),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Ca_lendar..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_SUBTREE (N_("_Toolbars"), view_toolbars_menu),
	GNOMEUIINFO_END
};

static GnomeUIInfo insert_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _File..."), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: It_em..."), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Object..."), NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo format_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Font..."), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Paragraph..."), NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo tools_forms_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Ch_oose Form..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Desi_gn This Form"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: D_esign a Form..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Publish _Form..."), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Pu_blish Form As..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Script _Debugger"), NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo tools_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Spelling..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Chec_k Names"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Address _Book..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_SUBTREE (N_("_Forms"), tools_forms_menu),
	GNOMEUIINFO_END
};

static GnomeUIInfo actions_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _New Appointment"), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Rec_urrence..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Intive _Attendees..."), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: C_ancel Invitation..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Forward as v_Calendar"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: For_ward"), NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo help_menu[] = {
	GNOMEUIINFO_ITEM_NONE ("FIXME: fix Bonobo so it supports help items!", NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo main_menu[] = {
	GNOMEUIINFO_MENU_FILE_TREE (file_menu),
	GNOMEUIINFO_MENU_EDIT_TREE (edit_menu),
	GNOMEUIINFO_MENU_VIEW_TREE (view_menu),
	GNOMEUIINFO_SUBTREE (N_("_Insert"), insert_menu),
	GNOMEUIINFO_SUBTREE (N_("F_ormat"), format_menu),
	GNOMEUIINFO_SUBTREE (N_("_Tools"), tools_menu),
	GNOMEUIINFO_SUBTREE (N_("Actio_ns"), actions_menu),
	GNOMEUIINFO_MENU_HELP_TREE (help_menu),
	GNOMEUIINFO_END
};

/* Creates the menu bar for the event editor */
static void
create_menu (EventEditor *ee)
{
	EventEditorPrivate *priv;
	BonoboUIHandlerMenuItem *list;

	priv = ee->priv;

	bonobo_ui_handler_create_menubar (priv->uih);

	list = bonobo_ui_handler_menu_parse_uiinfo_list_with_data (main_menu, ee);
	bonobo_ui_handler_menu_add_list (priv->uih, "/", list);
}

/* Toolbar */

static GnomeUIInfo toolbar[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Save and Close"), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Print..."), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Insert File..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Recurrence..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Invite Attendees..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Delete"), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Previous"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Next"), NULL, NULL),
	GNOMEUIINFO_ITEM_STOCK (N_("FIXME: Help"), NULL, NULL, GNOME_STOCK_PIXMAP_HELP),
	GNOMEUIINFO_END
};

/* Creates the toolbar for the event editor */
static void
create_toolbar (EventEditor *ee)
{
	EventEditorPrivate *priv;
	BonoboUIHandlerToolbarItem *list;
	GnomeDockItem *dock_item;
	GtkWidget *toolbar_child;

	priv = ee->priv;

	bonobo_ui_handler_create_toolbar (priv->uih, "Toolbar");

	/* Fetch the toolbar.  What a pain in the ass. */

	dock_item = gnome_app_get_dock_item_by_name (GNOME_APP (priv->app), GNOME_APP_TOOLBAR_NAME);
	g_assert (dock_item != NULL);

	toolbar_child = gnome_dock_item_get_child (dock_item);
	g_assert (toolbar_child != NULL && GTK_IS_TOOLBAR (toolbar_child));

	/* Turn off labels as GtkToolbar sucks */
	gtk_toolbar_set_style (GTK_TOOLBAR (toolbar_child), GTK_TOOLBAR_ICONS);

	list = bonobo_ui_handler_toolbar_parse_uiinfo_list_with_data (toolbar, ee);
	bonobo_ui_handler_toolbar_add_list (priv->uih, "/Toolbar", list);
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

	/* Construct the app */

	priv->uih = bonobo_ui_handler_new ();
	if (!priv->uih) {
		g_message ("event_editor_construct(): Could not create the UI handler");
		goto error;
	}

	bonobo_ui_handler_set_app (priv->uih, GNOME_APP (priv->app));

	create_menu (ee);
	create_toolbar (ee);

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

/**
 * event_editor_set_ical_object:
 * @ee: An event editor.
 * @ico: A calendar object.
 * 
 * Sets the calendar object that an event editor dialog will manipulate.
 **/
void
event_editor_set_ical_object (EventEditor *ee, iCalObject *ico)
{
	EventEditorPrivate *priv;
	char *title;

	g_return_if_fail (ee != NULL);
	g_return_if_fail (IS_EVENT_EDITOR (ee));

	if (ico)
		g_return_if_fail (ico->uid != NULL);

	priv = ee->priv;

	if (priv->ico) {
		gtk_signal_emit (GTK_OBJECT (ee), event_editor_signals[ICAL_OBJECT_RELEASED],
				 priv->ico->uid);
		ical_object_unref (priv->ico);
		priv->ico = NULL;
	}

	if (ico)
		priv->ico = ical_object_duplicate (ico);

	title = make_title_from_ico (priv->ico);
	gtk_window_set_title (GTK_WINDOW (priv->app), title);
	g_free (title);

	fill_widgets (ee);
}

/* Brings attention to a window by raising it and giving it focus */
static void
raise_and_focus (GtkWidget *widget)
{
	g_assert (GTK_WIDGET_REALIZED (widget));
	gdk_window_show (widget->window);
	gtk_widget_grab_focus (widget);
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
free_exdate (iCalObject *ico)
{
	GList *list;

	if (!ico->exdate)
		return;

	for (list = ico->exdate; list; list = list->next)
		g_free (list->data);

	g_list_free (ico->exdate);
	ico->exdate = NULL;
}

/* Decode the radio button group for classifications */
static ClassificationType
classification_get (GtkWidget *widget)
{
	return e_dialog_radio_get (widget, classification_map);
}

/* Get the values of the widgets in the event editor and put them in the iCalObject */
static void
dialog_to_ical_object (EventEditor *ee)
{
	EventEditorPrivate *priv;
	iCalObject *ico;
	gboolean all_day_event;
	int i;
	time_t *t;
	GtkCList *exception_list;

	priv = ee->priv;
	ico = priv->ico;

	if (ico->summary)
		g_free (ico->summary);

	ico->summary  = e_dialog_editable_get (priv->general_summary);

	ico->dtstart = e_dialog_dateedit_get (priv->start_time);
	ico->dtend = e_dialog_dateedit_get (priv->end_time);

	all_day_event = e_dialog_toggle_get (priv->all_day_event);

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

	ico->palarm.data = e_dialog_editable_get (priv->alarm_program_run_program);
	ico->malarm.data = e_dialog_editable_get (priv->alarm_mail_mail_to);

	if (ico->class)
		g_free (ico->class);

	switch (classification_get (priv->classification_radio)) {
	case CLASSIFICATION_PUBLIC:
		ico->class = g_strdup ("PUBLIC");
		break;

	case CLASSIFICATION_PRIVATE:
		ico->class = g_strdup ("PRIVATE");
		break;

	case CLASSIFICATION_CONFIDENTIAL:
		ico->class = g_strdup ("CONFIDENTIAL");
		break;

	default:
		g_assert_not_reached ();
	}

	/* Recurrence information */

	if (ico->recur) {
		g_free (ico->recur);
		ico->recur = NULL;
	}

	switch (recur_options_get (priv->recurrence_rule_none)) {
	case RECUR_OPTION_NONE:
		/* nothing */
		break;

	case RECUR_OPTION_DAILY:
		ico->recur = g_new0 (Recurrence, 1);
		ico->recur->type = RECUR_DAILY;
		ico->recur->interval = e_dialog_spin_get_int (priv->recurrence_rule_daily_days);
		break;

	case RECUR_OPTION_WEEKLY:
		ico->recur = g_new0 (Recurrence, 1);
		ico->recur->type = RECUR_WEEKLY;
		ico->recur->interval = e_dialog_spin_get_int (priv->recurrence_rule_weekly_weeks);
		ico->recur->weekday = 0;

		if (e_dialog_toggle_get (priv->recurrence_rule_weekly_sun))
			ico->recur->weekday |= 1 << 0;
		if (e_dialog_toggle_get (priv->recurrence_rule_weekly_mon))
			ico->recur->weekday |= 1 << 1;
		if (e_dialog_toggle_get (priv->recurrence_rule_weekly_tue))
			ico->recur->weekday |= 1 << 2;
		if (e_dialog_toggle_get (priv->recurrence_rule_weekly_wed))
			ico->recur->weekday |= 1 << 3;
		if (e_dialog_toggle_get (priv->recurrence_rule_weekly_thu))
			ico->recur->weekday |= 1 << 4;
		if (e_dialog_toggle_get (priv->recurrence_rule_weekly_fri))
			ico->recur->weekday |= 1 << 5;
		if (e_dialog_toggle_get (priv->recurrence_rule_weekly_sat))
			ico->recur->weekday |= 1 << 6;

		break;

	case RECUR_OPTION_MONTHLY:
		ico->recur = g_new0 (Recurrence, 1);

		if (e_dialog_toggle_get (priv->recurrence_rule_monthly_on_day)) {
			/* by day of in the month (ex: the 5th) */
			ico->recur->type = RECUR_MONTHLY_BY_DAY;
			ico->recur->u.month_day = e_dialog_spin_get_int (
				priv->recurrence_rule_monthly_day_nth);
		} else if (e_dialog_toggle_get (priv->recurrence_rule_monthly_weekday)) {
			/* "recurrence-rule-monthly-weekday" is TRUE */
			/* by position on the calender (ex: 2nd monday) */
			ico->recur->type = RECUR_MONTHLY_BY_POS;
			ico->recur->u.month_pos = e_dialog_option_menu_get (
				priv->recurrence_rule_monthly_week,
				month_pos_map);
			ico->recur->weekday = e_dialog_option_menu_get (
				priv->recurrence_rule_monthly_weekpos,
				weekday_map);

		} else
			g_assert_not_reached ();

		ico->recur->interval = e_dialog_spin_get_int (
			priv->recurrence_rule_monthly_every_n_months);

		break;

	case RECUR_OPTION_YEARLY:
		ico->recur = g_new0 (Recurrence, 1);
		ico->recur->type = RECUR_YEARLY_BY_DAY;
		ico->recur->interval = e_dialog_spin_get_int (
			priv->recurrence_rule_yearly_every_n_years);

	default:
		g_assert_not_reached ();
	}

	/* recurrence ending date */

	if (e_dialog_toggle_get (priv->recurrence_ending_date_repeat_forever)) {
		ico->recur->_enddate = 0;
		ico->recur->enddate = 0;
		ico->recur->duration = 0;
	} else if (e_dialog_toggle_get (priv->recurrence_ending_date_end_on)) {
		/* Also here, to ensure that the event is used, we add 86400
		 * secs to get get next day, in accordance to the RFC
		 */
		ico->recur->_enddate = e_dialog_dateedit_get (
			priv->recurrence_ending_date_end_on_date) + 86400;
		ico->recur->enddate = ico->recur->_enddate;
		ico->recur->duration = 0;
	} else if (e_dialog_toggle_get (priv->recurrence_ending_date_end_after)) {
		ico->recur->duration = e_dialog_spin_get_int (
			priv->recurrence_ending_date_end_after_count);
		ical_object_compute_end (ico);
	} else
		g_assert_not_reached ();

	/* Get exceptions from clist into ico->exdate */

	free_exdate (ico);
	exception_list = GTK_CLIST (priv->recurrence_exceptions_list);

	for (i = 0; i < exception_list->rows; i++) {
		t = gtk_clist_get_row_data (exception_list, i);
		ico->exdate = g_list_prepend (ico->exdate, t);
	}
}

#if 0
static void
ee_ok (GtkWidget *widget, EventEditor *ee)
{
	EventEditorPrivate *priv;

	priv = ee->priv;

	dialog_to_ical_object (ee);

	if (priv->ico->new)
		gnome_calendar_add_object (priv->gcal, priv->ico);
	else
		gnome_calendar_object_changed (priv->gcal, priv->ico);

	priv->ico->new = FALSE;
}

static void
ee_cancel (GtkWidget *widget, EventEditor *ee)
{
	EventEditorPrivate *priv;

	priv = ee->priv;

	if (priv->ico) {
		ical_object_unref (priv->ico);
		priv->ico = NULL;
	}
}
#endif

static void
alarm_toggle (GtkWidget *toggle, EventEditor *ee)
{
	EventEditorPrivate *priv;
	GtkWidget *alarm_amount;
	GtkWidget *alarm_unit;
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

	ev_start = e_dialog_dateedit_get (priv->start_time);
	ev_end = e_dialog_dateedit_get (priv->end_time);

	/* all day event checkbox */
	if (get_time_t_hour (ev_start) <= day_begin &&
	    get_time_t_hour (ev_end) >= day_end)
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
	struct tm tm;
	time_t start_t;

	priv = ee->priv;

	start_t = e_dialog_dateedit_get (priv->start_time);
	tm = *localtime (&start_t);
	tm.tm_hour = day_begin;
	tm.tm_min  = 0;
	tm.tm_sec  = 0;
	e_dialog_dateedit_set (priv->start_time, mktime (&tm));

	if (GTK_TOGGLE_BUTTON (toggle)->active)
		tm.tm_hour = day_end;
	else
		tm.tm_hour++;

	e_dialog_dateedit_set (priv->end_time, mktime (&tm));
}

/*
 * Callback: checks that the dates are start < end
 */
static void
check_dates (GnomeDateEdit *gde, EventEditor *ee)
{
	EventEditorPrivate *priv;
	time_t start, end;
	struct tm tm_start, tm_end;

	priv = ee->priv;

	start = e_dialog_dateedit_get (priv->start_time);
	end = e_dialog_dateedit_get (priv->end_time);

	if (start > end) {
		tm_start = *localtime (&start);
		tm_end = *localtime (&end);

		if (GTK_WIDGET (gde) == priv->start_time) {
			tm_end.tm_year = tm_start.tm_year;
			tm_end.tm_mon  = tm_start.tm_mon;
			tm_end.tm_mday = tm_start.tm_mday;

			gtk_signal_handler_block_by_data (GTK_OBJECT (priv->end_time), ee);
			e_dialog_dateedit_set (priv->end_time, mktime (&tm_end));
			gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->end_time), ee);
		} else if (GTK_WIDGET (gde) == priv->end_time) {
			tm_start.tm_year = tm_end.tm_year;
			tm_start.tm_mon  = tm_end.tm_mon;
			tm_start.tm_mday = tm_end.tm_mday;

#if 0
			gtk_signal_handler_block_by_data (GTK_OBJECT (priv->start_time), ee);
			e_dialog_dateedit_set (priv->start_time, mktime (&tm_start));
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
check_times (GnomeDateEdit *gde, EventEditor *ee)
{
	EventEditorPrivate *priv;
	time_t start, end;
	struct tm tm_start, tm_end;

	priv = ee->priv;
#if 0
	gdk_pointer_ungrab (GDK_CURRENT_TIME);
	gdk_flush ();
#endif
	start = e_dialog_dateedit_get (priv->start_time);
	end = e_dialog_dateedit_get (priv->end_time);

	if (start >= end) {
		tm_start = *localtime (&start);
		tm_end = *localtime (&end);

		if (GTK_WIDGET (gde) == priv->start_time) {
			tm_end.tm_min  = tm_start.tm_min;
			tm_end.tm_sec  = tm_start.tm_sec;
			tm_end.tm_hour = tm_start.tm_hour + 1;

			if (tm_end.tm_hour >= 24) {
				tm_end.tm_hour = 24; /* mktime() will bump the day */
				tm_end.tm_min = 0;
				tm_end.tm_sec = 0;
			}

			gtk_signal_handler_block_by_data (GTK_OBJECT (priv->end_time), ee);
			e_dialog_dateedit_set (priv->end_time, mktime (&tm_end));
			gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->end_time), ee);
		} else if (GTK_WIDGET (gde) == priv->end_time) {
			tm_start.tm_min  = tm_end.tm_min;
			tm_start.tm_sec  = tm_end.tm_sec;
			tm_start.tm_hour = tm_end.tm_hour - 1;

			if (tm_start.tm_hour < 0) {
				tm_start.tm_hour = 0;
				tm_start.tm_min = 0;
				tm_start.tm_min = 0;
			}

			gtk_signal_handler_block_by_data (GTK_OBJECT (priv->start_time), ee);
			e_dialog_dateedit_set (priv->start_time, mktime (&tm_start));
			gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->start_time), ee);
		}
	}

	/* Check whether the event spans the whole day */

	check_all_day (ee);
}

static void
recurrence_toggled (GtkWidget *radio, EventEditor *ee)
{
	EventEditorPrivate *priv;
	RecurOptions ro;

	priv = ee->priv;

	if (!GTK_TOGGLE_BUTTON (radio)->active)
		return;

	ro = e_dialog_radio_get (radio, recur_options_map);

	gtk_notebook_set_page (GTK_NOTEBOOK (priv->recurrence_rule_notebook), (int) ro);
}


static char *
get_exception_string (time_t t)
{
	static char buf[256];

	strftime (buf, sizeof (buf), _("%a %b %d %Y"), localtime (&t));
	return buf;
}


static void
append_exception (EventEditor *ee, time_t t)
{
	EventEditorPrivate *priv;
	time_t *tt;
	char *c[1];
	int i;
	GtkCList *clist;

	priv = ee->priv;

	c[0] = get_exception_string (t);

	tt = g_new (time_t, 1);
	*tt = t;

	clist = GTK_CLIST (priv->recurrence_exceptions_list);

	i = gtk_clist_append (clist, c);
	gtk_clist_set_row_data (clist, i, tt);
	gtk_clist_select_row (clist, i, 0);

/*  	gtk_widget_set_sensitive (ee->recur_ex_vbox, TRUE); */
}


static void
recurrence_exception_added (GtkWidget *widget, EventEditor *ee)
{
	EventEditorPrivate *priv;
	time_t t;

	priv = ee->priv;

	t = e_dialog_dateedit_get (priv->recurrence_exceptions_date);
	append_exception (ee, t);
}


static void
recurrence_exception_deleted (GtkWidget *widget, EventEditor *ee)
{
	EventEditorPrivate *priv;
	GtkCList *clist;
	int sel;

	priv = ee->priv;

	clist = GTK_CLIST (priv->recurrence_exceptions_list);
	if (!clist->selection)
		return;

	sel = GPOINTER_TO_INT (clist->selection->data);

	g_free (gtk_clist_get_row_data (clist, sel)); /* free the time_t stored there */

	gtk_clist_remove (clist, sel);
	if (sel >= clist->rows)
		sel--;

	gtk_clist_select_row (clist, sel, 0);
}


static void
recurrence_exception_changed (GtkWidget *widget, EventEditor *ee)
{
	EventEditorPrivate *priv;
	GtkCList *clist;
	time_t *t;
	int sel;

	priv = ee->priv;

	clist = GTK_CLIST (priv->recurrence_exceptions_list);
	if (!clist->selection)
		return;

	sel = GPOINTER_TO_INT (clist->selection->data);

	t = gtk_clist_get_row_data (clist, sel);
	*t = e_dialog_dateedit_get (priv->recurrence_exceptions_date);

	gtk_clist_set_text (clist, sel, 0, get_exception_string (*t));
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
	return gnome_date_edit_new_flags (the_time,
					  ((show_time ? GNOME_DATE_EDIT_SHOW_TIME : 0)
					   | (am_pm_flag ? 0 : GNOME_DATE_EDIT_24_HR)
					   | (week_starts_on_monday
					      ? GNOME_DATE_EDIT_WEEK_STARTS_ON_MONDAY
					      : 0)));
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

   build some of the recur stuff by hand to take into account
   the start-on-monday preference?

   get the apply button to work right

   make the properties stuff unglobal

   figure out why alarm units aren't sticking between edits

   closing the dialog window with the wm caused a crash
   Gtk-WARNING **: invalid cast from `(unknown)' to `GnomeDialog'
   on line 669:  gnome_dialog_close (GNOME_DIALOG(dialog->dialog));
 */
