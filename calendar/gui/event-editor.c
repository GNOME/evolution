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
#include <cal-util/timeutil.h>
#include "event-editor-utils.h"
#include "event-editor.h"



typedef struct {
	/* Glade XML data */
	GladeXML *xml;

	/* Calendar this editor is associated to */
	GnomeCalendar *gcal;

	/* UI handler */
	BonoboUIHandler *uih;

	/* Widgets from the Glade file */

	GtkWidget *general_owner;

	GtkWidget *start_time;
	GtkWidget *end_time;
	GtkWidget *all_day_checkbox;

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
	GtkWidget *alarm_mail_amount;
	GtkWidget *alarm_mail_unit;
	GtkWidget *alarm_mail_mail_to;

	GtkWidget *recurrence_rule_notebook;
	GtkWidget *recurrence_rule_none;
	GtkWidget *recurrence_rule_daily;
	GtkWidget *recurrence_rule_weekly;
	GtkWidget *recurrence_rule_monthly;
	GtkWidget *recurrence_rule_yearly;

	GtkWidget *recurrence_exception_add;
	GtkWidget *recurrence_exception_delete;
	GtkWidget *recurrence_exception_change;

	GtkWidget *exception_list;
	GtkWidget *exception_date;
} EventEditorPrivate;

typedef struct {
	GladeXML *gui;
	GtkWidget *dialog;
	GnomeCalendar *gnome_cal;
	iCalObject *ical;
} EventEditorDialog;



static void event_editor_class_init (EventEditorClass *class);
static void event_editor_init (EventEditor *ee);
static void event_editor_destroy (GtkObject *object);

static GnomeAppClass *parent_class;

extern int day_begin, day_end;
extern char *user_name;
extern int am_pm_flag;
extern int week_starts_on_monday;


static void append_exception (EventEditorDialog *dialog, time_t t);
static void check_all_day (EventEditorDialog *dialog);
static void alarm_toggle (GtkToggleButton *toggle, EventEditorDialog *dialog);



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

		event_editor_type = gtk_type_unique (gnome_app_get_type (), &event_editor_info);
	}

	return event_editor_type;
}

/* Class initialization function for the event editor */
static void
event_editor_class_init (EventEditorClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (gnome_app_get_type ());

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

/* Gets the widgets from the XML file and returns if they are all available */
static gboolean
get_widgets (EventEditor *ee)
{
	EventEditorPrivate *priv;

	priv = ee->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	priv->general_owner = GW ("general-owner");

	priv->start_time = GW ("start-time");
	priv->end_time = GW ("end-time");
	priv->all_day_checkbox = GW ("all-day-event");

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
	priv->alarm_program_run_program = GW ("run-program-file-entry");
	priv->alarm_mail_amount = GW ("alarm-mail-amount");
	priv->alarm_mail_unit = GW ("alarm-mail-unit");
	priv->alarm_mail_mail_to = GW ("mail-to");

	priv->recurrence_rule_notebook = GW ("recurrence-rule-notebook");
	priv->recurrence_rule_none = GW ("recurrence-rule-none");
	priv->recurrence_rule_daily = GW ("recurrence-rule-daily");
	priv->recurrence_rule_weekly = GW ("recurrence-rule-weekly");
	priv->recurrence_rule_monthly = GW ("recurrence-rule-monthly");
	priv->recurrence_rule_yearly = GW ("recurrence-rule-yearly");

	priv->recurrence_exception_add = GW ("recurrence-exceptions-add");
	priv->recurrence_exception_delete = GW ("recurrence-exceptions-delete");
	priv->recurrence_exception_change = GW ("recurrence-exceptions-change");

	priv->exception_list = GW ("recurrence-exceptions-list");
	priv->exception_date = GW ("recurrence-exceptions-date");

#undef GW

	return (priv->general_owner
		&& priv->start_time
		&& priv->end_time
		&& priv->all_day_checkbox
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
		&& priv->alarm_mail_amount
		&& priv->alarm_mail_unit
		&& priv->alarm_mail_mail_to
		&& priv->recurrence_rule_notebook
		&& priv->recurrence_rule_none
		&& priv->recurrence_rule_daily
		&& priv->recurrence_rule_weekly
		&& priv->recurrence_rule_monthly
		&& priv->recurrence_rule_yearly
		&& priv->recurrence_exception_add
		&& priv->recurrence_exception_delete
		&& priv->recurrence_exception_change
		&& priv->exception_list
		&& priv->exception_date);
}

static GnomeUIInfo main_menu[] = {
	/* FIXME */
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

static GnomeUIInfo toolbar[] = {
	/* FIXME */
	GNOMEUIINFO_END
};

/* Creates the toolbar for the event editor */
static void
create_toolbar (EventEditor *ee)
{
	EventEditorPrivate *priv;
	BonoboUIHandlerToolbarItem *list;

	priv = ee->priv;

	bonobo_ui_handler_create_toolbar (priv->uih, "Toolbar");

	list = bonobo_ui_handler_toolbar_parse_uiinfo_list_with_data (toolbar, ee);
	bonobo_ui_handler_toolbar_add_list (priv->uih, "/Toolbar", list);
}

GtkWidget *
event_editor_construct (EventEditor *ee, GnomeCalendar *gcal, iCalObject *ico)
{
	EventEditorPrivate *priv;
	char *title;
	GtkWidget *toplevel;
	GtkWidget *contents;

	g_return_val_if_fail (ee != NULL, NULL);
	g_return_val_if_fail (IS_EVENT_EDITOR (ee), NULL);
	g_return_val_if_fail (gcal != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), NULL);
	g_return_val_if_fail (ico != NULL, NULL);
	g_return_val_if_fail (ico->uid != NULL, NULL);

	priv = ee->priv;

	/* Create the UI handler */

	priv->uih = bonobo_ui_handler_new ();
	if (!priv->uih) {
		g_message ("event_editor_construct(): Could not create the UI handler");
		goto error;
	}

	bonobo_ui_handler_set_app (priv->uih, GNOME_APP (ee));

	/* Load the content widgets */

	priv->xml = glade_xml_new (EVOLUTION_GLADEDIR "/event-editor-dialog.glade", NULL);
	if (!priv->xml) {
		g_message ("event_editor_construct(): Could not load the Glade XML file!");
		goto error;
	}

	toplevel = glade_xml_get_widget (priv->xml, "event-editor-dialog");
	contents = glade_xml_get_widget (priv->xml, "dialog-contents");
	if (!(toplevel && contents)) {
		g_message ("event_editor_construct(): Could not find the contents in the XML file!");
		goto error;
	}

	if (!get_widgets (ee)) {
		g_message ("event_editor_construct(): Could not find all widgets in the XML file!");
		goto error;
	}

	gtk_object_ref (GTK_OBJECT (contents));
	gtk_container_remove (GTK_CONTAINER (toplevel), GTK_WIDGET (contents));
	gtk_widget_destroy (GTK_WIDGET (toplevel));

	/* Construct the app */

	priv->gcal = gcal;

	title = make_title_from_ico (ico);
	gnome_app_construct (GNOME_APP (ee), "event-editor", title);
	g_free (title);

	create_menu (ee);
	create_toolbar (ee);

	gnome_app_set_contents (GNOME_APP (ee), contents);
	gtk_widget_show (contents);
	gtk_object_unref (GTK_OBJECT (contents));

	return GTK_WIDGET (ee);

 error:

	gtk_object_unref (GTK_OBJECT (ee));
	return NULL;
}

GtkWidget *
event_editor_new (GnomeCalendar *gcal, iCalObject *ico)
{
	EventEditor *ee;

	g_return_val_if_fail (gcal != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CALENDAR (gcal), NULL);
	g_return_val_if_fail (ico != NULL, NULL);
	g_return_val_if_fail (ico->uid != NULL, NULL);

	ee = EVENT_EDITOR (gtk_type_new (TYPE_EVENT_EDITOR));

	ee = event_editor_construct (ee, gcal, ico);

	if (ee)
		gtk_widget_show (GTK_WIDGET (ee));

	return GTK_WIDGET (ee);
}

static void
fill_in_dialog_from_ical (EventEditorDialog *dialog)
{
	iCalObject *ical = dialog->ical;
	GladeXML *gui = dialog->gui;
	GList *list;
	GtkWidget *alarm_display, *alarm_program, *alarm_audio, *alarm_mail;

	store_to_editable (gui, "general-owner",
			   dialog->ical->organizer->addr ?
			   dialog->ical->organizer->addr : _("?"));

	store_to_editable (gui, "general-summary", ical->summary);

	/* start and end time */
	store_to_gnome_dateedit (gui, "start-time", ical->dtstart);
	store_to_gnome_dateedit (gui, "end-time", ical->dtend);

	check_all_day (dialog);

	/* alarms */
	alarm_display = glade_xml_get_widget (dialog->gui, "alarm-display");
	alarm_program = glade_xml_get_widget (dialog->gui, "alarm-program");
	alarm_audio = glade_xml_get_widget (dialog->gui, "alarm-audio");
	alarm_mail = glade_xml_get_widget (dialog->gui, "alarm-mail");

	store_to_toggle (gui, "alarm-display", ical->dalarm.enabled);
	store_to_toggle (gui, "alarm-program", ical->palarm.enabled);
	store_to_toggle (gui, "alarm-audio", ical->aalarm.enabled);
	store_to_toggle (gui, "alarm-mail", ical->malarm.enabled);
	alarm_toggle (GTK_TOGGLE_BUTTON (alarm_display), dialog);
	alarm_toggle (GTK_TOGGLE_BUTTON (alarm_program), dialog);
	alarm_toggle (GTK_TOGGLE_BUTTON (alarm_audio), dialog);
	alarm_toggle (GTK_TOGGLE_BUTTON (alarm_mail), dialog);
	gtk_signal_connect (GTK_OBJECT (alarm_display), "toggled", GTK_SIGNAL_FUNC (alarm_toggle), dialog);
	gtk_signal_connect (GTK_OBJECT (alarm_program), "toggled", GTK_SIGNAL_FUNC (alarm_toggle), dialog);
	gtk_signal_connect (GTK_OBJECT (alarm_audio), "toggled", GTK_SIGNAL_FUNC (alarm_toggle), dialog);
	gtk_signal_connect (GTK_OBJECT (alarm_mail), "toggled", GTK_SIGNAL_FUNC (alarm_toggle), dialog);

	/* alarm counts */
	store_to_spin (gui, "alarm-display-amount", ical->dalarm.count);
	store_to_spin (gui, "alarm-audio-amount", ical->aalarm.count);
	store_to_spin (gui, "alarm-program-amount", ical->palarm.count);
	store_to_spin (gui, "alarm-mail-amount", ical->malarm.count);

	store_to_alarm_unit (gui, "alarm-display-unit", ical->dalarm.units);
	store_to_alarm_unit (gui, "alarm-audio-unit", ical->aalarm.units);
	store_to_alarm_unit (gui, "alarm-program-unit", ical->palarm.units);
	store_to_alarm_unit (gui, "alarm-mail-unit", ical->malarm.units);

	store_to_editable (gui, "run-program", ical->palarm.data);
	store_to_editable (gui, "mail-to", ical->malarm.data);

	/* classification */
	if (strcmp (ical->class, "PUBLIC") == 0)
	    	store_to_toggle (gui, "classification-public", TRUE);
	if (strcmp (ical->class, "PRIVATE") == 0)
	    	store_to_toggle (gui, "classification-private", TRUE);
	if (strcmp (ical->class, "CONFIDENTIAL") == 0)
		store_to_toggle (gui, "classification-confidential", TRUE);

	/* recurrence rules */

	if (! ical->recur)
		return;

	switch (ical->recur->type) {
	case RECUR_DAILY:
		store_to_toggle (gui, "recurrence-rule-daily", TRUE);
		store_to_spin (gui, "recurrence-rule-daily-days", ical->recur->interval);
		break;
	case RECUR_WEEKLY:
		store_to_toggle (gui, "recurrence-rule-weekly", TRUE);
		store_to_spin (gui, "recurrence-rule-weekly-weeks", ical->recur->interval);
		if (ical->recur->weekday & (1 << 0))
			store_to_toggle (gui, "recurrence-rule-weekly-sun", TRUE);
		if (ical->recur->weekday & (1 << 1))
			store_to_toggle (gui, "recurrence-rule-weekly-mon", TRUE);
		if (ical->recur->weekday & (1 << 2))
			store_to_toggle (gui, "recurrence-rule-weekly-tue", TRUE);
		if (ical->recur->weekday & (1 << 3))
			store_to_toggle (gui, "recurrence-rule-weekly-wed", TRUE);
		if (ical->recur->weekday & (1 << 4))
			store_to_toggle (gui, "recurrence-rule-weekly-thu", TRUE);
		if (ical->recur->weekday & (1 << 5))
			store_to_toggle (gui, "recurrence-rule-weekly-fri", TRUE);
		if (ical->recur->weekday & (1 << 6))
			store_to_toggle (gui, "recurrence-rule-weekly-sat", TRUE);
		break;
	case RECUR_MONTHLY_BY_DAY:
		store_to_toggle (gui, "recurrence-rule-monthly", TRUE);
		store_to_toggle (gui, "recurrence-rule-monthly-on-day", TRUE);
		store_to_spin (gui, "recurrence-rule-monthly-day", ical->recur->u.month_day);
		store_to_spin (gui, "recurrence-rule-monthly-every-n-months", ical->recur->interval);
		break;
	case RECUR_MONTHLY_BY_POS:
		store_to_toggle (gui, "recurrence-rule-monthly", TRUE);
		store_to_toggle (gui, "recurrence-rule-monthly-weekday", TRUE);
		store_to_option (gui, "recurrence-rule-monthly-week", ical->recur->u.month_pos);
		store_to_option (gui, "recurrence-rule-monthly-day", ical->recur->weekday);
		store_to_spin (gui, "recurrence-rule-monthly-every-n-months", ical->recur->interval);
		break;
	case RECUR_YEARLY_BY_DAY:
	case RECUR_YEARLY_BY_MONTH:
		store_to_toggle (gui, "recurrence-rule-yearly", TRUE);
		store_to_spin (gui, "recurrence-rule-yearly-every-n-years", ical->recur->interval);
		break;
	}


	if (ical->recur->_enddate == 0) {
		if (ical->recur->duration == 0)
			store_to_toggle (gui, "recurrence-ending-date-repeat-forever", TRUE);
		else {
			store_to_toggle (gui, "recurrence-ending-date-end-after", TRUE);
			store_to_spin (gui, "recurrence-ending-date-end-after-count", ical->recur->duration);
		}
	} else {
		store_to_toggle (gui, "recurrence-ending-date-end-on", TRUE);
		/* Shorten by one day, as we store end-on date a day ahead */
		/* FIX ME is this correct? */
		store_to_gnome_dateedit (gui, "recurrence-ending-date-end-on-date", ical->recur->enddate - 86400);
	}

	/* fill the exceptions list */
	for (list = ical->exdate; list; list = list->next)
		append_exception (dialog, *((time_t *) list->data));
}


static void
fill_in_dialog_from_defaults (EventEditorDialog *dialog)
{
	time_t now = time (NULL);
	time_t soon = time_add_minutes (now, 30);

	store_to_editable (dialog->gui, "general-owner", "?");

	/* start and end time */
	store_to_gnome_dateedit (dialog->gui, "start-time", now);
	store_to_gnome_dateedit (dialog->gui, "end-time", soon);
}


static void
free_exdate (iCalObject *ical)
{
	GList *list;

	if (!ical->exdate)
		return;

	for (list = ical->exdate; list; list = list->next)
		g_free (list->data);

	g_list_free (ical->exdate);
	ical->exdate = NULL;
}


static void
dialog_to_ical (EventEditorDialog *dialog)
{
	iCalObject *ical = dialog->ical;
	gboolean all_day_event;
	GladeXML *gui = dialog->gui;

	/* general event information */

	if (ical->summary)
		g_free (ical->summary);
	ical->summary  = extract_from_editable (gui, "general-summary");

	ical->dtstart = extract_from_gnome_dateedit (gui, "start-time");
	ical->dtend = extract_from_gnome_dateedit (gui, "end-time");

	all_day_event = extract_from_toggle (gui, "all-day-event");

	ical->dalarm.enabled = extract_from_toggle (gui, "alarm-display");
	ical->aalarm.enabled = extract_from_toggle (gui, "alarm-program");
	ical->palarm.enabled = extract_from_toggle (gui, "alarm-audio");
	ical->malarm.enabled = extract_from_toggle (gui, "alarm-mail");

	ical->dalarm.count = extract_from_spin (gui, "alarm-display-amount");
	ical->aalarm.count = extract_from_spin (gui, "alarm-audio-amount");
	ical->palarm.count = extract_from_spin (gui, "alarm-program-amount");
	ical->malarm.count = extract_from_spin (gui, "alarm-mail-amount");

	ical->dalarm.units = extract_from_alarm_unit (gui, "alarm-display-unit");
	ical->aalarm.units = extract_from_alarm_unit (gui, "alarm-audio-unit");
	ical->palarm.units = extract_from_alarm_unit (gui, "alarm-program-unit");
	ical->malarm.units = extract_from_alarm_unit (gui, "alarm-mail-unit");

	ical->palarm.data = g_strdup (extract_from_editable (gui, "run-program"));
	ical->malarm.data = g_strdup (extract_from_editable (gui, "mail-to"));

	if (extract_from_toggle (gui, "classification-public"))
		ical->class = g_strdup ("PUBLIC");
	else if (extract_from_toggle (gui, "classification-private"))
		ical->class = g_strdup ("PRIVATE");
	else /* "classification-confidential" */
		ical->class = g_strdup ("CONFIDENTIAL");


	/* recurrence information */

	if (extract_from_toggle (gui, "recurrence-rule-none"))
		return; /* done */
	if (!ical->recur)
		ical->recur = g_new0 (Recurrence, 1);

	if (extract_from_toggle (gui, "recurrence-rule-daily")) {
		ical->recur->type = RECUR_DAILY;
		ical->recur->interval = extract_from_spin (gui, "recurrence-rule-daily-days");
	}
	if (extract_from_toggle (gui, "recurrence-rule-weekly")) {
		ical->recur->type = RECUR_WEEKLY;
		ical->recur->interval = extract_from_spin (gui, "recurrence-rule-weekly-weeks");
		ical->recur->weekday = 0;

		if (extract_from_toggle (gui, "recurrence-rule-weekly-sun"))
			ical->recur->weekday |= 1 << 0;
		if (extract_from_toggle (gui, "recurrence-rule-weekly-mon"))
			ical->recur->weekday |= 1 << 1;
		if (extract_from_toggle (gui, "recurrence-rule-weekly-tue"))
			ical->recur->weekday |= 1 << 2;
		if (extract_from_toggle (gui, "recurrence-rule-weekly-wed"))
			ical->recur->weekday |= 1 << 3;
		if (extract_from_toggle (gui, "recurrence-rule-weekly-thu"))
			ical->recur->weekday |= 1 << 4;
		if (extract_from_toggle (gui, "recurrence-rule-weekly-fri"))
			ical->recur->weekday |= 1 << 5;
		if (extract_from_toggle (gui, "recurrence-rule-weekly-sat"))
			ical->recur->weekday |= 1 << 6;
	}
	if (extract_from_toggle (gui, "recurrence-rule-monthly")) {
		if (extract_from_toggle (gui, "recurrence-rule-monthly-on-day")) {
			/* by day of in the month (ex: the 5th) */
			ical->recur->type = RECUR_MONTHLY_BY_DAY;
			ical->recur->u.month_day = extract_from_spin (gui, "recurrence-rule-monthly-day");

		}
		else {  /* "recurrence-rule-monthly-weekday" is TRUE */ 
			/* by position on the calender (ex: 2nd monday) */
			ical->recur->type = RECUR_MONTHLY_BY_POS;
			ical->recur->u.month_pos = extract_from_option (gui, "recurrence-rule-monthly-week");
			ical->recur->weekday = extract_from_option (gui, "recurrence-rule-monthly-day");
		}
		ical->recur->interval = extract_from_spin (gui, "recurrence-rule-monthly-every-n-months");
	}
	if (extract_from_toggle (gui, "recurrence-rule-yearly")) {
		ical->recur->type = RECUR_YEARLY_BY_DAY;
		ical->recur->interval = extract_from_spin (gui, "recurrence-rule-yearly-every-n-years");
		/* FIXME: need to specify anything else?  I am assuming the code will look at the dtstart
		 * to figure out when to recur. - Federico
		 */
	}

	/* recurrence ending date */
	if (extract_from_toggle (gui, "recurrence-ending-date-repeat-forever")) {
		ical->recur->_enddate = 0;
		ical->recur->enddate = 0;
		ical->recur->duration = 0;
	}
	if (extract_from_toggle (gui, "recurrence-ending-date-end-on")) {
		/* Also here, to ensure that the event is used, we add 86400 secs to get 
		   get next day, in accordance to the RFC */
		ical->recur->_enddate = extract_from_gnome_dateedit (gui, "recurrence-ending-date-end-on-date") + 86400;
		ical->recur->enddate = ical->recur->_enddate;
		ical->recur->duration = 0;
	}
	if (extract_from_toggle (gui, "recurrence-ending-date-end-after")) {
		ical->recur->duration = extract_from_spin (gui, "recurrence-ending-date-end-after-count");
		ical_object_compute_end (ical);
	}


	/* get exceptions from clist into ical->exdate */
	{
		int i;
		time_t *t;
		GtkCList *exception_list = GTK_CLIST (glade_xml_get_widget (dialog->gui, "recurrence-exceptions-list"));

		free_exdate (ical);

		for (i = 0; i < exception_list->rows; i++) {
			t = gtk_clist_get_row_data (exception_list, i);
			ical->exdate = g_list_prepend (ical->exdate, t);
		}
	}
}


static void
ee_ok (GtkWidget *widget, EventEditorDialog *dialog)
{
	dialog_to_ical (dialog);

	if (dialog->ical->new)
		gnome_calendar_add_object (dialog->gnome_cal, dialog->ical);
	else
		gnome_calendar_object_changed (dialog->gnome_cal,
					       dialog->ical,
					       CHANGE_ALL);
	dialog->ical->new = 0;
}


static void
ee_cancel (GtkWidget *widget, EventEditorDialog *dialog)
{
	if (dialog->ical) {
		ical_object_unref (dialog->ical);
		dialog->ical = NULL;
	}
}


static void
alarm_toggle (GtkToggleButton *toggle, EventEditorDialog *dialog)
{
	GtkWidget *alarm_display = glade_xml_get_widget (dialog->gui, "alarm-display");
	GtkWidget *alarm_program = glade_xml_get_widget (dialog->gui, "alarm-program");
	GtkWidget *alarm_audio = glade_xml_get_widget (dialog->gui, "alarm-audio");
	GtkWidget *alarm_mail = glade_xml_get_widget (dialog->gui, "alarm-mail");
	GtkWidget *alarm_amount, *alarm_unit;

	if (GTK_WIDGET (toggle) == alarm_display) {
		alarm_amount = glade_xml_get_widget (dialog->gui, "alarm-display-amount");
		alarm_unit = glade_xml_get_widget (dialog->gui, "alarm-display-unit");
	}
	if (GTK_WIDGET (toggle) == alarm_audio) {
		alarm_amount = glade_xml_get_widget (dialog->gui, "alarm-audio-amount");
		alarm_unit = glade_xml_get_widget (dialog->gui, "alarm-audio-unit");
	}
	if (GTK_WIDGET (toggle) == alarm_program) {
		GtkWidget *run_program;
		alarm_amount = glade_xml_get_widget (dialog->gui, "alarm-program-amount");
		alarm_unit = glade_xml_get_widget (dialog->gui, "alarm-program-unit");
		run_program = glade_xml_get_widget (dialog->gui, "run-program-file-entry");
		gtk_widget_set_sensitive (run_program, toggle->active);
	}
	if (GTK_WIDGET (toggle) == alarm_mail) {
		GtkWidget *mail_to;
		alarm_amount = glade_xml_get_widget (dialog->gui, "alarm-mail-amount");
		alarm_unit = glade_xml_get_widget (dialog->gui, "alarm-mail-unit");
		mail_to = glade_xml_get_widget (dialog->gui, "mail-to");
		gtk_widget_set_sensitive (mail_to, toggle->active);
	}

	gtk_widget_set_sensitive (alarm_amount, toggle->active);
	gtk_widget_set_sensitive (alarm_unit, toggle->active);
}



/*
 * Checks if the day range occupies all the day, and if so, check the
 * box accordingly
 */
static void
check_all_day (EventEditorDialog *dialog)
{
	time_t ev_start = extract_from_gnome_dateedit (dialog->gui, "start-time");
	time_t ev_end = extract_from_gnome_dateedit (dialog->gui, "end-time");

	/* all day event checkbox */
	if (get_time_t_hour (ev_start) <= day_begin &&
	    get_time_t_hour (ev_end) >= day_end)
		store_to_toggle (dialog->gui, "all-day-event", TRUE);
	else
		store_to_toggle (dialog->gui, "all-day-event", FALSE);
}


/*
 * Callback: all day event box clicked
 */
static void
set_all_day (GtkToggleButton *toggle, EventEditorDialog *dialog)
{
	struct tm tm;
	time_t start_t;

	start_t = extract_from_gnome_dateedit (dialog->gui, "start-time");
	tm = *localtime (&start_t);
	tm.tm_hour = day_begin;
	tm.tm_min  = 0;
	tm.tm_sec  = 0;
	store_to_gnome_dateedit (dialog->gui, "start-time", mktime (&tm));
	
	if (toggle->active)
		tm.tm_hour = day_end;
	else
		tm.tm_hour++;
	
	store_to_gnome_dateedit (dialog->gui, "end-time", mktime (&tm));
}


/*
 * Callback: checks that the dates are start < end
 */
static void
check_dates (GnomeDateEdit *gde, EventEditorDialog *dialog)
{
	time_t start, end;
	struct tm tm_start, tm_end;
	GtkWidget *start_time = glade_xml_get_widget (dialog->gui, "start-time");
	GtkWidget *end_time = glade_xml_get_widget (dialog->gui, "end-time");


	//start = gnome_date_edit_get_date (GNOME_DATE_EDIT (ee->start_time));
	start = extract_from_gnome_dateedit (dialog->gui, "start-time");
	//end = gnome_date_edit_get_date (GNOME_DATE_EDIT (ee->end_time));
	end = extract_from_gnome_dateedit (dialog->gui, "end-time");

	if (start > end) {
		tm_start = *localtime (&start);
		tm_end = *localtime (&end);

		if (GTK_WIDGET (gde) == start_time) {
			tm_end.tm_year = tm_start.tm_year;
			tm_end.tm_mon  = tm_start.tm_mon;
			tm_end.tm_mday = tm_start.tm_mday;

			gnome_date_edit_set_time (GNOME_DATE_EDIT (end_time), mktime (&tm_end));
		} else if (GTK_WIDGET (gde) == end_time) {
			tm_start.tm_year = tm_end.tm_year;
			tm_start.tm_mon  = tm_end.tm_mon;
			tm_start.tm_mday = tm_end.tm_mday;

			//gnome_date_edit_set_time (GNOME_DATE_EDIT (ee->start_time), mktime (&tm_start));
		}
	}
}


/*
 * Callback: checks that start_time < end_time and whether the
 * selected hour range spans all of the day
 */
static void
check_times (GnomeDateEdit *gde, EventEditorDialog *dialog)
{
	time_t start, end;
	struct tm tm_start, tm_end;
	GtkWidget *start_time = glade_xml_get_widget (dialog->gui, "start-time");
	GtkWidget *end_time = glade_xml_get_widget (dialog->gui, "end-time");


	gdk_pointer_ungrab (GDK_CURRENT_TIME);
	gdk_flush ();

	start = gnome_date_edit_get_date (GNOME_DATE_EDIT (start_time));
	end = gnome_date_edit_get_date (GNOME_DATE_EDIT (end_time));

	if (start >= end) {
		tm_start = *localtime (&start);
		tm_end = *localtime (&end);

		if (GTK_WIDGET (gde) == start_time) {
			tm_end.tm_min  = tm_start.tm_min;
			tm_end.tm_sec  = tm_start.tm_sec;

			tm_end.tm_hour = tm_start.tm_hour + 1;

			if (tm_end.tm_hour >= 24) {
				tm_end.tm_hour = 24; /* mktime() will bump the day */
				tm_end.tm_min = 0;
				tm_end.tm_sec = 0;
			}

			gnome_date_edit_set_time (GNOME_DATE_EDIT (end_time), mktime (&tm_end));
		} else if (GTK_WIDGET (gde) == end_time) {
			tm_start.tm_min  = tm_end.tm_min;
			tm_start.tm_sec  = tm_end.tm_sec;

			tm_start.tm_hour = tm_end.tm_hour - 1;

			if (tm_start.tm_hour < 0) {
				tm_start.tm_hour = 0;
				tm_start.tm_min = 0;
				tm_start.tm_min = 0;
			}

			gnome_date_edit_set_time (GNOME_DATE_EDIT (start_time), mktime (&tm_start));
		}
	}

	/* Check whether the event spans the whole day */

	check_all_day (dialog);
}


static void
recurrence_toggled (GtkWidget *radio, EventEditorDialog *dialog)
{
	GtkWidget *recurrence_rule_notebook = glade_xml_get_widget (dialog->gui, "recurrence-rule-notebook");

	GtkWidget *recurrence_rule_none = glade_xml_get_widget (dialog->gui, "recurrence-rule-none");
	GtkWidget *recurrence_rule_daily = glade_xml_get_widget (dialog->gui, "recurrence-rule-daily");
	GtkWidget *recurrence_rule_weekly = glade_xml_get_widget (dialog->gui, "recurrence-rule-weekly");
	GtkWidget *recurrence_rule_monthly = glade_xml_get_widget (dialog->gui, "recurrence-rule-monthly");
	GtkWidget *recurrence_rule_yearly = glade_xml_get_widget (dialog->gui, "recurrence-rule-yearly");

	if (radio == recurrence_rule_none)
		gtk_notebook_set_page (GTK_NOTEBOOK (recurrence_rule_notebook), 0);
	if (radio == recurrence_rule_daily)
		gtk_notebook_set_page (GTK_NOTEBOOK (recurrence_rule_notebook), 1);
	if (radio == recurrence_rule_weekly)
		gtk_notebook_set_page (GTK_NOTEBOOK (recurrence_rule_notebook), 2);
	if (radio == recurrence_rule_monthly)
		gtk_notebook_set_page (GTK_NOTEBOOK (recurrence_rule_notebook), 3);
	if (radio == recurrence_rule_yearly)
		gtk_notebook_set_page (GTK_NOTEBOOK (recurrence_rule_notebook), 4);
}


static char *
get_exception_string (time_t t)
{
	static char buf[256];

	strftime (buf, sizeof(buf), _("%a %b %d %Y"), localtime (&t));
	return buf;
}


static void
append_exception (EventEditorDialog *dialog, time_t t)
{
	time_t *tt;
	char   *c[1];
	int     i;
	GtkCList *exception_list = GTK_CLIST (glade_xml_get_widget (dialog->gui, "recurrence-exceptions-list"));

	c[0] = get_exception_string (t);

	tt = g_new (time_t, 1);
	*tt = t;

	i = gtk_clist_append (exception_list, c);
	gtk_clist_set_row_data (exception_list, i, tt);
	gtk_clist_select_row (exception_list, i, 0);

	//gtk_widget_set_sensitive (ee->recur_ex_vbox, TRUE);
}


static void
recurrence_exception_added (GtkWidget *widget, EventEditorDialog *dialog)
{
	//GtkWidget *exception_date = glade_xml_get_widget (dialog->gui, "recurrence-exceptions-date");
	time_t t = extract_from_gnome_dateedit (dialog->gui, "recurrence-exceptions-date");
	append_exception (dialog, t);
}


static void
recurrence_exception_deleted (GtkWidget *widget, EventEditorDialog *dialog)
{
	GtkCList *clist;
	int sel, length;

	clist = GTK_CLIST (glade_xml_get_widget (dialog->gui, "recurrence-exceptions-list"));
	if (! clist->selection)
		return;
	sel = GPOINTER_TO_INT(clist->selection->data);

	g_free (gtk_clist_get_row_data (clist, sel)); /* free the time_t stored there */

	gtk_clist_remove (clist, sel);
	length = g_list_length(clist->row_list);
	if (sel >= length)
		sel--;
	gtk_clist_select_row (clist, sel, 0);

	//if (clist->rows == 0)
	//	gtk_widget_set_sensitive (ee->recur_ex_vbox, FALSE);
}


static void
recurrence_exception_changed (GtkWidget *widget, EventEditorDialog *dialog)
{
	GtkCList *clist;
	time_t *t;
	int sel;

	clist = GTK_CLIST (glade_xml_get_widget (dialog->gui, "recurrence-exceptions-list"));
	if (! clist->selection)
		return;

	sel = GPOINTER_TO_INT(clist->selection->data);

	t = gtk_clist_get_row_data (clist, sel);
	*t = extract_from_gnome_dateedit (dialog->gui, "recurrence-exceptions-date");

	gtk_clist_set_text (clist, sel, 0, get_exception_string (*t));
}


#if 0
GtkWidget *event_editor_new (GnomeCalendar *gcal, iCalObject *ical)
{
	EventEditorDialog *dialog = g_new0 (EventEditorDialog, 1);

	dialog->ical = ical;
	dialog->gnome_cal = gcal;

	printf ("glade_xml_new (%s, NULL)\n",
		EVOLUTION_GLADEDIR "/event-editor-dialog.glade");

	dialog->gui = glade_xml_new (EVOLUTION_GLADEDIR
				     "/event-editor-dialog.glade",
				     NULL);

	dialog->dialog = glade_xml_get_widget (dialog->gui,
					       "event-editor-dialog");

	gnome_dialog_button_connect (GNOME_DIALOG (dialog->dialog),
				     0, GTK_SIGNAL_FUNC(ee_ok), dialog);
	gnome_dialog_button_connect (GNOME_DIALOG (dialog->dialog),
				     1, GTK_SIGNAL_FUNC(ee_cancel), dialog);


	{
		GtkWidget *start_time = glade_xml_get_widget (dialog->gui, "start-time");
		GtkWidget *end_time = glade_xml_get_widget (dialog->gui, "end-time");

		gtk_signal_connect (GTK_OBJECT (start_time), "date_changed",
				    GTK_SIGNAL_FUNC (check_dates), dialog);
		gtk_signal_connect (GTK_OBJECT (start_time), "time_changed",
				    GTK_SIGNAL_FUNC (check_times), dialog);

		gtk_signal_connect (GTK_OBJECT (end_time), "date_changed",
				    GTK_SIGNAL_FUNC (check_dates), dialog);
		gtk_signal_connect (GTK_OBJECT (end_time), "time_changed",
				    GTK_SIGNAL_FUNC (check_times), dialog);
	}


	{
		GtkWidget *all_day_checkbox = glade_xml_get_widget (dialog->gui, "all-day-event");
		gtk_signal_connect (GTK_OBJECT (all_day_checkbox), "toggled", GTK_SIGNAL_FUNC (set_all_day), dialog);
	}


	{
		GtkWidget *recurrence_rule_none = glade_xml_get_widget (dialog->gui, "recurrence-rule-none");
		GtkWidget *recurrence_rule_daily = glade_xml_get_widget (dialog->gui, "recurrence-rule-daily");
		GtkWidget *recurrence_rule_weekly = glade_xml_get_widget (dialog->gui, "recurrence-rule-weekly");
		GtkWidget *recurrence_rule_monthly = glade_xml_get_widget (dialog->gui, "recurrence-rule-monthly");
		GtkWidget *recurrence_rule_yearly = glade_xml_get_widget (dialog->gui, "recurrence-rule-yearly");

		gtk_signal_connect (GTK_OBJECT (recurrence_rule_none), "toggled",
				    GTK_SIGNAL_FUNC (recurrence_toggled), dialog);
		gtk_signal_connect (GTK_OBJECT (recurrence_rule_daily), "toggled",
				    GTK_SIGNAL_FUNC (recurrence_toggled), dialog);
		gtk_signal_connect (GTK_OBJECT (recurrence_rule_weekly), "toggled",
				    GTK_SIGNAL_FUNC (recurrence_toggled), dialog);
		gtk_signal_connect (GTK_OBJECT (recurrence_rule_monthly), "toggled",
				    GTK_SIGNAL_FUNC (recurrence_toggled), dialog);
		gtk_signal_connect (GTK_OBJECT (recurrence_rule_yearly), "toggled",
				    GTK_SIGNAL_FUNC (recurrence_toggled), dialog);
		
	}


	{
		GtkWidget *recurrence_exception_add = glade_xml_get_widget (dialog->gui, "recurrence-exceptions-add");
		GtkWidget *recurrence_exception_delete = glade_xml_get_widget (dialog->gui, "recurrence-exceptions-delete");
		GtkWidget *recurrence_exception_change = glade_xml_get_widget (dialog->gui, "recurrence-exceptions-change");

		gtk_signal_connect (GTK_OBJECT (recurrence_exception_add), "clicked",
				    GTK_SIGNAL_FUNC (recurrence_exception_added), dialog);
		gtk_signal_connect (GTK_OBJECT (recurrence_exception_delete), "clicked",
				    GTK_SIGNAL_FUNC (recurrence_exception_deleted), dialog);
		gtk_signal_connect (GTK_OBJECT (recurrence_exception_change), "clicked",
				    GTK_SIGNAL_FUNC (recurrence_exception_changed), dialog);
	}



	if (ical->new)
		fill_in_dialog_from_defaults (dialog);
	else
		fill_in_dialog_from_ical (dialog);
	

	gnome_dialog_run (GNOME_DIALOG(dialog->dialog));

	gnome_dialog_close (GNOME_DIALOG(dialog->dialog));

	return dialog->dialog;
}
#endif

void event_editor_new_whole_day (GnomeCalendar *owner, time_t day)
{
	struct tm tm;
	iCalObject *ico;
	GtkWidget *ee;

	g_return_if_fail (owner != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (owner));

	ico = ical_new ("", user_name, "");
	ico->new = TRUE;

	tm = *localtime (&day);

	/* Set the start time of the event to the beginning of the day */

	tm.tm_hour = day_begin;
	tm.tm_min  = 0;
	tm.tm_sec  = 0;
	ico->dtstart = mktime (&tm);

	/* Set the end time of the event to the end of the day */

	tm.tm_hour = day_end;
	ico->dtend = mktime (&tm);

	/* Launch the event editor */

	ee = event_editor_new (owner, ico);
}



GtkWidget *make_date_edit (void)
{
	return date_edit_new (time (NULL), FALSE);
}


GtkWidget *make_date_edit_with_time (void)
{
	return date_edit_new (time (NULL), TRUE);
}

 
GtkWidget *date_edit_new (time_t the_time, int show_time)
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
