/* Evolution calendar - Alarm page of the calendar component dialogs
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

#include <string.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <glade/glade.h>
#include <gal/widgets/e-unicode.h>
#include "e-util/e-dialog-widgets.h"
#include "cal-util/cal-util.h"
#include "comp-editor-util.h"
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

	gboolean updating;
};



static void alarm_page_class_init (AlarmPageClass *class);
static void alarm_page_init (AlarmPage *apage);
static void alarm_page_destroy (GtkObject *object);

static GtkWidget *alarm_page_get_widget (CompEditorPage *page);
static void alarm_page_fill_widgets (CompEditorPage *page, CalComponent *comp);
static void alarm_page_fill_component (CompEditorPage *page, CalComponent *comp);
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

	priv->updating = FALSE;
}

/* Frees all the alarm data and empties the list */
static void
free_alarms (AlarmPage *apage)
{
	AlarmPagePrivate *priv;
	int i;

	priv = apage->priv;

	if (priv->list != NULL) {
		GtkCList *clist = GTK_CLIST (priv->list);

		for (i = 0; i < clist->rows; i++) {
			CalComponentAlarm *alarm;
			
			alarm = gtk_clist_get_row_data (clist, i);
			g_assert (alarm != NULL);
			cal_component_alarm_free (alarm);
			
			gtk_clist_set_row_data (clist, i, NULL);
		}
	
		gtk_clist_clear (clist);
	}
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

	free_alarms (apage);

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

	/* List data */
	free_alarms (apage);
}

static char *
get_alarm_duration_string (struct icaldurationtype *duration)
{
	GString *string = g_string_new (NULL);
	char *ret;

	if (duration->days > 1)
		g_string_sprintf (string, _("%d days"), duration->days);
	else if (duration->days == 1)
		g_string_append (string, _("1 day"));

	if (duration->weeks > 1)
		g_string_sprintf (string, _("%d weeks"), duration->weeks);
	else if (duration->weeks == 1)
		g_string_append (string, _("1 week"));

	if (duration->hours > 1)
		g_string_sprintf (string, _("%d hours"), duration->hours);
	else if (duration->hours == 1)
		g_string_append (string, _("1 hour"));

	if (duration->minutes > 1)
		g_string_sprintf (string, _("%d minutes"), duration->minutes);
	else if (duration->minutes == 1)
		g_string_append (string, _("1 minute"));

	if (duration->seconds > 1)
		g_string_sprintf (string, _("%d seconds"), duration->seconds);
	else if (duration->seconds == 1)
		g_string_append (string, _("1 second"));

	ret = string->str;
	g_string_free (string, FALSE);

	return ret;
}

static char *
get_alarm_string (CalComponentAlarm *alarm)
{
	CalAlarmAction action;
	CalAlarmTrigger trigger;
	char string[256];
	char *base;
	char *str;
	char *dur;

	string [0] = '\0';

	cal_component_alarm_get_action (alarm, &action);
	cal_component_alarm_get_trigger (alarm, &trigger);

	switch (action) {
	case CAL_ALARM_AUDIO:
		base = _("Play a sound");
		break;

	case CAL_ALARM_DISPLAY:
		base = _("Show a dialog");
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
		base = _("Unknown");
		break;
	}

	/* FIXME: This does not look like it will localize correctly. */

	switch (trigger.type) {
	case CAL_ALARM_TRIGGER_RELATIVE_START:
		dur = get_alarm_duration_string (&trigger.u.rel_duration);

		if (trigger.u.rel_duration.is_neg)
			str = g_strdup_printf ("%s %s %s", base, dur,
					       _(" before start of appointment"));
		else
			str = g_strdup_printf ("%s %s %s", base, dur,
					       _(" after start of appointment"));

		g_free (dur);
		break;

	case CAL_ALARM_TRIGGER_RELATIVE_END:
		dur = get_alarm_duration_string (&trigger.u.rel_duration);

		if (trigger.u.rel_duration.is_neg)
			str = g_strdup_printf ("%s %s %s", base, dur,
					       _(" before end of appointment"));
		else
			str = g_strdup_printf ("%s %s %s", base, dur,
					       _(" after end of appointment"));

		g_free (dur);
		break;
	case CAL_ALARM_TRIGGER_NONE:
	case CAL_ALARM_TRIGGER_ABSOLUTE:
	default:
		str = g_strdup_printf ("%s %s", base,
				       _("Unknown"));
		break;
	}

	return str;
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

	gtk_clist_set_row_data (clist, i, alarm);
	gtk_clist_select_row (clist, i, 0);
	g_free (c[0]);

	gtk_widget_set_sensitive (priv->delete, TRUE);
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
	CompEditorPageDates dates;
	
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

	/* List */
	if (!cal_component_has_alarms (comp))
		return;

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

	priv->updating = FALSE;
}

/* fill_component handler for the alarm page */
static void
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

		alarm = gtk_clist_get_row_data (clist, i);
		g_assert (alarm != NULL);

		/* We clone the alarm to maintain the invariant that the alarm
		 * structures in the list did *not* come from the component.
		 */

		alarm_copy = cal_component_alarm_clone (alarm);
		cal_component_add_alarm (comp, alarm);
		cal_component_alarm_free (alarm_copy);
	}
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
	CAL_ALARM_EMAIL,
	CAL_ALARM_PROCEDURE,
	-1
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

/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (AlarmPage *apage)
{
	AlarmPagePrivate *priv;

	priv = apage->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	priv->main = GW ("alarm-page");
	g_assert (priv->main);
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
		&& priv->time);
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

	apage = ALARM_PAGE (data);
	priv = apage->priv;

	alarm = cal_component_alarm_new ();

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

	cal_component_alarm_set_action (alarm, e_dialog_option_menu_get (priv->action, action_map));

	append_reminder (apage, alarm);
}

/* Callback used for the "delete reminder" button */
static void
delete_clicked_cb (GtkButton *button, gpointer data)
{
	AlarmPage *apage;
	AlarmPagePrivate *priv;
	GtkCList *clist;
	CalComponentAlarm *alarm;
	int sel;

	apage = ALARM_PAGE (data);
	priv = apage->priv;

	clist = GTK_CLIST (priv->list);
	if (!clist->selection)
		return;

	sel = GPOINTER_TO_INT (clist->selection->data);

	alarm = gtk_clist_get_row_data (clist, sel);
	g_assert (alarm != NULL);
	cal_component_alarm_free (alarm);
	gtk_clist_set_row_data (clist, sel, NULL);

	gtk_clist_remove (clist, sel);
	if (sel >= clist->rows)
		sel--;

	if (clist->rows > 0)
		gtk_clist_select_row (clist, sel, 0);
	else
		gtk_widget_set_sensitive (priv->delete, FALSE);
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
