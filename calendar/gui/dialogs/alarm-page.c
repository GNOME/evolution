
/* Evolution calendar - Alarm page of the calendar component dialogs
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          Miguel de Icaza <miguel@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *          JP Rosevear <jpr@ximian.com>
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

#include <string.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <glade/glade.h>
#include <gal/widgets/e-unicode.h>
#include "e-util/e-dialog-widgets.h"
#include "e-util/e-time-utils.h"
#include "cal-util/cal-util.h"
#include "cal-util/timeutil.h"
#include "../calendar-config.h"
#include "comp-editor-util.h"
#include "alarm-options.h"
#include "alarm-page.h"



/* Private part of the AlarmPage structure */
struct _AlarmPagePrivate {
	/* Glade XML data */
	GladeXML *xml;

	/* Widgets from the Glade file */

	GtkWidget *main;

	GtkWidget *summary;
	GtkWidget *date_time;

	GtkWidget *list;
	GtkWidget *add;
	GtkWidget *delete;

	GtkWidget *action;
	GtkWidget *interval_value;
	GtkWidget *value_units;
	GtkWidget *relative;
	GtkWidget *time;

	GtkWidget *button_options;

	/* Alarm options dialog and the alarm we maintain */
	CalComponentAlarm *alarm;

	gboolean updating;
};

/* "relative" types */
enum {
	BEFORE,
	AFTER
};

/* Time units */
enum {
	MINUTES,
	HOURS,
	DAYS
};

/* Option menu maps */
static const int action_map[] = {
	CAL_ALARM_DISPLAY,
	CAL_ALARM_AUDIO,
	CAL_ALARM_PROCEDURE,
	CAL_ALARM_EMAIL,
	-1
};

static const char *action_map_cap[] = {
	"no-display-alarms",
	"no-audio-alarms",
	"no-procedure-alarms",
	"no-email-alarms"
};

static const int value_map[] = {
	MINUTES,
	HOURS,
	DAYS,
	-1
};

static const int relative_map[] = {
	BEFORE,
	AFTER,
	-1
};

static const int time_map[] = {
	CAL_ALARM_TRIGGER_RELATIVE_START,
	CAL_ALARM_TRIGGER_RELATIVE_END,
	-1
};



static void alarm_page_class_init (AlarmPageClass *class);
static void alarm_page_init (AlarmPage *apage);
static void alarm_page_destroy (GtkObject *object);

static GtkWidget *alarm_page_get_widget (CompEditorPage *page);
static void alarm_page_focus_main_widget (CompEditorPage *page);
static void alarm_page_fill_widgets (CompEditorPage *page, CalComponent *comp);
static gboolean alarm_page_fill_component (CompEditorPage *page, CalComponent *comp);
static void alarm_page_set_summary (CompEditorPage *page, const char *summary);
static void alarm_page_set_dates (CompEditorPage *page, CompEditorPageDates *dates);

static CompEditorPageClass *parent_class = NULL;



/**
 * alarm_page_get_type:
 *
 * Registers the #AlarmPage class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #AlarmPage class.
 **/
GtkType
alarm_page_get_type (void)
{
	static GtkType alarm_page_type;

	if (!alarm_page_type) {
		static const GtkTypeInfo alarm_page_info = {
			"AlarmPage",
			sizeof (AlarmPage),
			sizeof (AlarmPageClass),
			(GtkClassInitFunc) alarm_page_class_init,
			(GtkObjectInitFunc) alarm_page_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		alarm_page_type = gtk_type_unique (TYPE_COMP_EDITOR_PAGE,
						   &alarm_page_info);
	}

	return alarm_page_type;
}

/* Class initialization function for the alarm page */
static void
alarm_page_class_init (AlarmPageClass *class)
{
	CompEditorPageClass *editor_page_class;
	GtkObjectClass *object_class;

	editor_page_class = (CompEditorPageClass *) class;
	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (TYPE_COMP_EDITOR_PAGE);

	editor_page_class->get_widget = alarm_page_get_widget;
	editor_page_class->focus_main_widget = alarm_page_focus_main_widget;
	editor_page_class->fill_widgets = alarm_page_fill_widgets;
	editor_page_class->fill_component = alarm_page_fill_component;
	editor_page_class->set_summary = alarm_page_set_summary;
	editor_page_class->set_dates = alarm_page_set_dates;

	object_class->destroy = alarm_page_destroy;
}

/* Object initialization function for the alarm page */
static void
alarm_page_init (AlarmPage *apage)
{
	AlarmPagePrivate *priv;
	icalcomponent *icalcomp;
	icalproperty *icalprop;

	priv = g_new0 (AlarmPagePrivate, 1);
	apage->priv = priv;

	priv->xml = NULL;

	priv->main = NULL;
	priv->summary = NULL;
	priv->date_time = NULL;
	priv->list = NULL;
	priv->add = NULL;
	priv->delete = NULL;
	priv->action = NULL;
	priv->interval_value = NULL;
	priv->value_units = NULL;
	priv->relative = NULL;
	priv->time = NULL;
	priv->button_options = NULL;

	/* create the default alarm, which will contain the
	 * X-EVOLUTION-NEEDS-DESCRIPTION property, so that we
	 * set a correct description if none is ser */
	priv->alarm = cal_component_alarm_new ();

	icalcomp = cal_component_alarm_get_icalcomponent (priv->alarm);
	icalprop = icalproperty_new_x ("1");
	icalproperty_set_x_name (icalprop, "X-EVOLUTION-NEEDS-DESCRIPTION");
        icalcomponent_add_property (icalcomp, icalprop);

	priv->updating = FALSE;
}

/* Destroy handler for the alarm page */
static void
alarm_page_destroy (GtkObject *object)
{
	AlarmPage *apage;
	AlarmPagePrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_ALARM_PAGE (object));

	apage = ALARM_PAGE (object);
	priv = apage->priv;

	if (priv->xml) {
		gtk_object_unref (GTK_OBJECT (priv->xml));
		priv->xml = NULL;
	}

	if (priv->alarm) {
		cal_component_alarm_free (priv->alarm);
		priv->alarm = NULL;
	}

	g_free (priv);
	apage->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* get_widget handler for the alarm page */
static GtkWidget *
alarm_page_get_widget (CompEditorPage *page)
{
	AlarmPage *apage;
	AlarmPagePrivate *priv;

	apage = ALARM_PAGE (page);
	priv = apage->priv;

	return priv->main;
}

/* focus_main_widget handler for the alarm page */
static void
alarm_page_focus_main_widget (CompEditorPage *page)
{
	AlarmPage *apage;
	AlarmPagePrivate *priv;

	apage = ALARM_PAGE (page);
	priv = apage->priv;

	gtk_widget_grab_focus (priv->action);
}

/* Fills the widgets with default values */
static void
clear_widgets (AlarmPage *apage)
{
	AlarmPagePrivate *priv;

	priv = apage->priv;

	/* Summary */
	gtk_label_set_text (GTK_LABEL (priv->summary), "");

	/* Start date */
	gtk_label_set_text (GTK_LABEL (priv->date_time), "");

	/* Sane defaults */
	e_dialog_option_menu_set (priv->action, CAL_ALARM_DISPLAY, action_map);
	e_dialog_spin_set (priv->interval_value, 15);
	e_dialog_option_menu_set (priv->value_units, MINUTES, value_map);
	e_dialog_option_menu_set (priv->relative, BEFORE, relative_map);
	e_dialog_option_menu_set (priv->time, CAL_ALARM_TRIGGER_RELATIVE_START, time_map);

	/* List data */
	gtk_clist_clear (GTK_CLIST (priv->list));
}

/* Builds a string for the duration of the alarm.  If the duration is zero, returns NULL. */
static char *
get_alarm_duration_string (struct icaldurationtype *duration)
{
	GString *string = g_string_new (NULL);
	char *ret;
	gboolean have_something;

	have_something = FALSE;

	if (duration->days > 1) {
		g_string_sprintf (string, _("%d days"), duration->days);
		have_something = TRUE;
	} else if (duration->days == 1) {
		g_string_append (string, _("1 day"));
		have_something = TRUE;
	}

	if (duration->weeks > 1) {
		g_string_sprintf (string, _("%d weeks"), duration->weeks);
		have_something = TRUE;
	} else if (duration->weeks == 1) {
		g_string_append (string, _("1 week"));
		have_something = TRUE;
	}

	if (duration->hours > 1) {
		g_string_sprintf (string, _("%d hours"), duration->hours);
		have_something = TRUE;
	} else if (duration->hours == 1) {
		g_string_append (string, _("1 hour"));
		have_something = TRUE;
	}

	if (duration->minutes > 1) {
		g_string_sprintf (string, _("%d minutes"), duration->minutes);
		have_something = TRUE;
	} else if (duration->minutes == 1) {
		g_string_append (string, _("1 minute"));
		have_something = TRUE;
	}

	if (duration->seconds > 1) {
		g_string_sprintf (string, _("%d seconds"), duration->seconds);
		have_something = TRUE;
	} else if (duration->seconds == 1) {
		g_string_append (string, _("1 second"));
		have_something = TRUE;
	}

	if (have_something) {
		ret = string->str;
		g_string_free (string, FALSE);
		return ret;
	} else {
		g_string_free (string, TRUE);
		return NULL;
	}
}

static char *
get_alarm_string (CalComponentAlarm *alarm)
{
	CalAlarmAction action;
	CalAlarmTrigger trigger;
	char string[256];
	char *base, *str = NULL, *dur;

	string [0] = '\0';

	cal_component_alarm_get_action (alarm, &action);
	cal_component_alarm_get_trigger (alarm, &trigger);

	switch (action) {
	case CAL_ALARM_AUDIO:
		base = _("Play a sound");
		break;

	case CAL_ALARM_DISPLAY:
		base = _("Display a message");
		break;

	case CAL_ALARM_EMAIL:
		base = _("Send an email");
		break;

	case CAL_ALARM_PROCEDURE:
		base = _("Run a program");
		break;

	case CAL_ALARM_NONE:
	case CAL_ALARM_UNKNOWN:
	default:
		base = _("Unknown action to be performed");
		break;
	}

	/* FIXME: This does not look like it will localize correctly. */

	switch (trigger.type) {
	case CAL_ALARM_TRIGGER_RELATIVE_START:
		dur = get_alarm_duration_string (&trigger.u.rel_duration);

		if (dur) {
			if (trigger.u.rel_duration.is_neg)
				str = g_strdup_printf (_("%s %s before the start of the appointment"),
						       base, dur);
			else
				str = g_strdup_printf (_("%s %s after the start of the appointment"),
						       base, dur);

			g_free (dur);
		} else
			str = g_strdup_printf (_("%s at the start of the appointment"), base);

		break;

	case CAL_ALARM_TRIGGER_RELATIVE_END:
		dur = get_alarm_duration_string (&trigger.u.rel_duration);

		if (dur) {
			if (trigger.u.rel_duration.is_neg)
				str = g_strdup_printf (_("%s %s before the end of the appointment"),
						       base, dur);
			else
				str = g_strdup_printf (_("%s %s after the end of the appointment"),
						       base, dur);

			g_free (dur);
		} else
			str = g_strdup_printf (_("%s at the end of the appointment"), base);

		break;

	case CAL_ALARM_TRIGGER_ABSOLUTE: {
		struct icaltimetype itt;
		icaltimezone *utc_zone, *current_zone;
		char *location;
		struct tm tm;
		char buf[256];

		/* Absolute triggers come in UTC, so convert them to the local timezone */

		itt = trigger.u.abs_time;

		utc_zone = icaltimezone_get_utc_timezone ();
		location = calendar_config_get_timezone ();
		current_zone = icaltimezone_get_builtin_timezone (location);

		tm = icaltimetype_to_tm_with_zone (&itt, utc_zone, current_zone);

		e_time_format_date_and_time (&tm, calendar_config_get_24_hour_format (),
					     FALSE, FALSE, buf, sizeof (buf));

		str = g_strdup_printf (_("%s at %s"), base, buf);

		break; }

	case CAL_ALARM_TRIGGER_NONE:
	default:
		str = g_strdup_printf (_("%s for an unknown trigger type"), base);
		break;
	}

	return str;
}

static void
sensitize_buttons (AlarmPage *apage)
{
	AlarmPagePrivate *priv;
	CalClient *client;
	GtkCList *clist;
	
	priv = apage->priv;
	
	client = COMP_EDITOR_PAGE (apage)->client;
	clist = GTK_CLIST (priv->list);

	gtk_widget_set_sensitive (priv->add, cal_client_get_one_alarm_only (client) && clist->rows > 0 ? FALSE : TRUE);
	gtk_widget_set_sensitive (priv->delete, clist->rows > 0 ? TRUE : FALSE);
}

/* Appends an alarm to the list */
static void
append_reminder (AlarmPage *apage, CalComponentAlarm *alarm)
{
	AlarmPagePrivate *priv;
	GtkCList *clist;
	char *c[1];
	int i;

	priv = apage->priv;

	clist = GTK_CLIST (priv->list);

	c[0] = get_alarm_string (alarm);
	i = gtk_clist_append (clist, c);

	gtk_clist_set_row_data_full (clist, i, alarm, (GtkDestroyNotify) cal_component_alarm_free);
	gtk_clist_select_row (clist, i, 0);
	g_free (c[0]);

	sensitize_buttons (apage);
}

/* fill_widgets handler for the alarm page */
static void
alarm_page_fill_widgets (CompEditorPage *page, CalComponent *comp)
{
	AlarmPage *apage;
	AlarmPagePrivate *priv;
	CalComponentText text;
	GList *alarms, *l;
	GtkCList *clist;
	GtkWidget *menu;
	CompEditorPageDates dates;
	int i;
	
	apage = ALARM_PAGE (page);
	priv = apage->priv;

	/* Don't send off changes during this time */
	priv->updating = TRUE;

	/* Clean the page */
	clear_widgets (apage);

	/* Summary */
	cal_component_get_summary (comp, &text);
	alarm_page_set_summary (page, text.value);

	/* Dates */
	comp_editor_dates (&dates, comp);
	alarm_page_set_dates (page, &dates);
	comp_editor_free_dates (&dates);

	/* List */
	if (!cal_component_has_alarms (comp))
		goto out;

	alarms = cal_component_get_alarm_uids (comp);

	clist = GTK_CLIST (priv->list);
	for (l = alarms; l != NULL; l = l->next) {
		CalComponentAlarm *ca, *ca_copy;
		const char *auid;

		auid = l->data;
		ca = cal_component_get_alarm (comp, auid);
		g_assert (ca != NULL);

		ca_copy = cal_component_alarm_clone (ca);
		cal_component_alarm_free (ca);

		append_reminder (apage, ca_copy);
	}
	cal_obj_uid_list_free (alarms);

 out:

	/* Alarm types */
	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->action));
	for (i = 0, l = GTK_MENU_SHELL (menu)->children; action_map[i] != -1; i++, l = l->next) {
		if (cal_client_get_static_capability (page->client, action_map_cap[i]))
			gtk_widget_set_sensitive (l->data, FALSE);
		else
			gtk_widget_set_sensitive (l->data, TRUE);
	}
	

	priv->updating = FALSE;
}

/* fill_component handler for the alarm page */
static gboolean
alarm_page_fill_component (CompEditorPage *page, CalComponent *comp)
{
	AlarmPage *apage;
	AlarmPagePrivate *priv;
	GList *list, *l;
	GtkCList *clist;
	int i;

	apage = ALARM_PAGE (page);
	priv = apage->priv;

	/* Remove all the alarms from the component */

	list = cal_component_get_alarm_uids (comp);
	for (l = list; l; l = l->next) {
		const char *auid;

		auid = l->data;
		cal_component_remove_alarm (comp, auid);
	}
	cal_obj_uid_list_free (list);

	/* Add the new alarms */

	clist = GTK_CLIST (priv->list);
	for (i = 0; i < clist->rows; i++) {
		CalComponentAlarm *alarm, *alarm_copy;
		icalcomponent *icalcomp;
		icalproperty *icalprop;

		alarm = gtk_clist_get_row_data (clist, i);
		g_assert (alarm != NULL);

		/* We set the description of the alarm if it's got
		 * the X-EVOLUTION-NEEDS-DESCRIPTION property.
		 */
		icalcomp = cal_component_alarm_get_icalcomponent (alarm);
		icalprop = icalcomponent_get_first_property (icalcomp, ICAL_X_PROPERTY);
		while (icalprop) {
			const char *x_name;
			CalComponentText summary;

			x_name = icalproperty_get_x_name (icalprop);
			if (!strcmp (x_name, "X-EVOLUTION-NEEDS-DESCRIPTION")) {
				cal_component_get_summary (comp, &summary);
				cal_component_alarm_set_description (alarm, &summary);

				icalcomponent_remove_property (icalcomp, icalprop);
				break;
			}

			icalprop = icalcomponent_get_next_property (icalcomp, ICAL_X_PROPERTY);
		}

		/* We clone the alarm to maintain the invariant that the alarm
		 * structures in the list did *not* come from the component.
		 */

		alarm_copy = cal_component_alarm_clone (alarm);
		cal_component_add_alarm (comp, alarm_copy);
		cal_component_alarm_free (alarm_copy);
	}

	return TRUE;
}

/* set_summary handler for the alarm page */
static void
alarm_page_set_summary (CompEditorPage *page, const char *summary)
{
	AlarmPage *apage;
	AlarmPagePrivate *priv;
	gchar *s;

	apage = ALARM_PAGE (page);
	priv = apage->priv;

	s = e_utf8_to_gtk_string (priv->summary, summary);
	gtk_label_set_text (GTK_LABEL (priv->summary), s);
	g_free (s);
}

/* set_dates handler for the alarm page */
static void
alarm_page_set_dates (CompEditorPage *page, CompEditorPageDates *dates)
{
	AlarmPage *apage;
	AlarmPagePrivate *priv;

	apage = ALARM_PAGE (page);
	priv = apage->priv;

	comp_editor_date_label (dates, priv->date_time);
}



/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (AlarmPage *apage)
{
	CompEditorPage *page = COMP_EDITOR_PAGE (apage);
	AlarmPagePrivate *priv;
	GSList *accel_groups;
	GtkWidget *toplevel;

	priv = apage->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	priv->main = GW ("alarm-page");
	if (!priv->main)
		return FALSE;

	/* Get the GtkAccelGroup from the toplevel window, so we can install
	   it when the notebook page is mapped. */
	toplevel = gtk_widget_get_toplevel (priv->main);
	accel_groups = gtk_accel_groups_from_object (GTK_OBJECT (toplevel));
	if (accel_groups) {
		page->accel_group = accel_groups->data;
		gtk_accel_group_ref (page->accel_group);
	}

	gtk_widget_ref (priv->main);
	gtk_widget_unparent (priv->main);

	priv->summary = GW ("summary");
	priv->date_time = GW ("date-time");

	priv->list = GW ("list");
	priv->add = GW ("add");
	priv->delete = GW ("delete");

	priv->action = GW ("action");
	priv->interval_value = GW ("interval-value");
	priv->value_units = GW ("value-units");
	priv->relative = GW ("relative");
	priv->time = GW ("time");

	priv->button_options = GW ("button-options");

#undef GW

	return (priv->summary
		&& priv->date_time
		&& priv->list
		&& priv->add
		&& priv->delete
		&& priv->action
		&& priv->interval_value
		&& priv->value_units
		&& priv->relative
		&& priv->time
		&& priv->button_options);
}

/* This is called when any field is changed; it notifies upstream. */
static void
field_changed_cb (GtkWidget *widget, gpointer data)
{
	AlarmPage *apage;
	AlarmPagePrivate *priv;

	apage = ALARM_PAGE (data);
	priv = apage->priv;

	if (!priv->updating)
		comp_editor_page_notify_changed (COMP_EDITOR_PAGE (apage));
}

/* Callback used for the "add reminder" button */
static void
add_clicked_cb (GtkButton *button, gpointer data)
{
	AlarmPage *apage;
	AlarmPagePrivate *priv;
	CalComponentAlarm *alarm;
	CalAlarmTrigger trigger;
	CalAlarmAction action;
	
	apage = ALARM_PAGE (data);
	priv = apage->priv;

	alarm = cal_component_alarm_clone (priv->alarm);

	memset (&trigger, 0, sizeof (CalAlarmTrigger));
	trigger.type = e_dialog_option_menu_get (priv->time, time_map);
	if (e_dialog_option_menu_get (priv->relative, relative_map) == BEFORE)
		trigger.u.rel_duration.is_neg = 1;
	else
		trigger.u.rel_duration.is_neg = 0;

	switch (e_dialog_option_menu_get (priv->value_units, value_map)) {
	case MINUTES:
		trigger.u.rel_duration.minutes =
			e_dialog_spin_get_int (priv->interval_value);
		break;

	case HOURS:
		trigger.u.rel_duration.hours =
			e_dialog_spin_get_int (priv->interval_value);
		break;

	case DAYS:
		trigger.u.rel_duration.days =
			e_dialog_spin_get_int (priv->interval_value);
		break;

	default:
		g_assert_not_reached ();
	}
	cal_component_alarm_set_trigger (alarm, trigger);

	action = e_dialog_option_menu_get (priv->action, action_map);
	cal_component_alarm_set_action (alarm, action);
	if (action == CAL_ALARM_EMAIL && !cal_component_alarm_has_attendees (alarm)) {
		const char *email;
		
		email = cal_client_get_alarm_email_address (COMP_EDITOR_PAGE (apage)->client);
		if (email != NULL) {
			CalComponentAttendee *a;
			GSList attendee_list;

			a = g_new0 (CalComponentAttendee, 1);
			a->value = email;
			attendee_list.data = a;
			attendee_list.next = NULL;
			cal_component_alarm_set_attendee_list (alarm, &attendee_list);
			g_free (a);
		}
	}

	append_reminder (apage, alarm);
}

/* Callback used for the "delete reminder" button */
static void
delete_clicked_cb (GtkButton *button, gpointer data)
{
	AlarmPage *apage;
	AlarmPagePrivate *priv;
	GtkCList *clist;
	int sel;

	apage = ALARM_PAGE (data);
	priv = apage->priv;

	clist = GTK_CLIST (priv->list);
	if (!clist->selection)
		return;

	sel = GPOINTER_TO_INT (clist->selection->data);

	gtk_clist_remove (clist, sel);
	if (sel >= clist->rows)
		sel--;

	sensitize_buttons (apage);
}

/* Callback used when the alarm options button is clicked */
static void
button_options_clicked_cb (GtkWidget *widget, gpointer data)
{
	AlarmPage *apage;
	AlarmPagePrivate *priv;
	gboolean repeat;
	const char *email;
	
	apage = ALARM_PAGE (data);
	priv = apage->priv;

	cal_component_alarm_set_action (priv->alarm,
					e_dialog_option_menu_get (priv->action, action_map));

	repeat = !cal_client_get_static_capability (COMP_EDITOR_PAGE (apage)->client, "no-alarm-repeat");
	email = cal_client_get_alarm_email_address (COMP_EDITOR_PAGE (apage)->client);
	if (!alarm_options_dialog_run (priv->alarm, email, repeat))
		g_message ("button_options_clicked_cb(): Could not create the alarm options dialog");
}

/* Hooks the widget signals */
static void
init_widgets (AlarmPage *apage)
{
	AlarmPagePrivate *priv;

	priv = apage->priv;

	/* Reminder buttons */
	gtk_signal_connect (GTK_OBJECT (priv->add), "clicked",
			    GTK_SIGNAL_FUNC (add_clicked_cb), apage);
	gtk_signal_connect (GTK_OBJECT (priv->delete), "clicked",
			    GTK_SIGNAL_FUNC (delete_clicked_cb), apage);

	/* Connect the default signal handler to use to make sure we notify
	 * upstream of changes to the widget values.
	 */
	gtk_signal_connect (GTK_OBJECT (priv->add), "clicked",
			    GTK_SIGNAL_FUNC (field_changed_cb), apage);
	gtk_signal_connect (GTK_OBJECT (priv->delete), "clicked",
			    GTK_SIGNAL_FUNC (field_changed_cb), apage);

	/* Options button */
	gtk_signal_connect (GTK_OBJECT (priv->button_options), "clicked",
			    GTK_SIGNAL_FUNC (button_options_clicked_cb), apage);
}



/**
 * alarm_page_construct:
 * @apage: An alarm page.
 *
 * Constructs an alarm page by loading its Glade data.
 *
 * Return value: The same object as @apage, or NULL if the widgets could not be
 * created.
 **/
AlarmPage *
alarm_page_construct (AlarmPage *apage)
{
	AlarmPagePrivate *priv;

	priv = apage->priv;

	priv->xml = glade_xml_new (EVOLUTION_GLADEDIR "/alarm-page.glade",
				   NULL);
	if (!priv->xml) {
		g_message ("alarm_page_construct(): "
			   "Could not load the Glade XML file!");
		return NULL;
	}

	if (!get_widgets (apage)) {
		g_message ("alarm_page_construct(): "
			   "Could not find all widgets in the XML file!");
		return NULL;
	}

	init_widgets (apage);

	return apage;
}

/**
 * alarm_page_new:
 *
 * Creates a new alarm page.
 *
 * Return value: A newly-created alarm page, or NULL if the page could not be
 * created.
 **/
AlarmPage *
alarm_page_new (void)
{
	AlarmPage *apage;

	apage = gtk_type_new (TYPE_ALARM_PAGE);
	if (!alarm_page_construct (apage)) {
		gtk_object_unref (GTK_OBJECT (apage));
		return NULL;
	}

	return apage;
}
