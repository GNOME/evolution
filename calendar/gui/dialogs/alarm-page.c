/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Evolution calendar - Alarm page of the calendar component dialogs
 *
 * Copyright (C) 2001-2003 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          Miguel de Icaza <miguel@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *          JP Rosevear <jpr@ximian.com>
 *          Hans Petter Jansson <hpj@ximian.com>
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
#include <gtk/gtklabel.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkoptionmenu.h>
#include <libgnome/gnome-i18n.h>
#include <glade/glade.h>
#include "e-util/e-dialog-widgets.h"
#include "e-util/e-time-utils.h"
#include <libecal/e-cal-util.h>
#include <libecal/e-cal-time-util.h>
#include "../calendar-config.h"
#include "comp-editor-util.h"
#include "alarm-options.h"
#include "../e-alarm-list.h"
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
	ECalComponentAlarm *alarm;

	/* Alarm store for the GtkTreeView list widget */
	EAlarmList *list_store;

	gboolean updating;

	/* Old summary, to detect changes */
	gchar *old_summary;
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
	E_CAL_COMPONENT_ALARM_DISPLAY,
	E_CAL_COMPONENT_ALARM_AUDIO,
	E_CAL_COMPONENT_ALARM_PROCEDURE,
	E_CAL_COMPONENT_ALARM_EMAIL,
	-1
};

static const char *action_map_cap[] = {
	CAL_STATIC_CAPABILITY_NO_DISPLAY_ALARMS,
	CAL_STATIC_CAPABILITY_NO_AUDIO_ALARMS,
        CAL_STATIC_CAPABILITY_NO_PROCEDURE_ALARMS,
	CAL_STATIC_CAPABILITY_NO_EMAIL_ALARMS
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
	E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START,
	E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_END,
	-1
};



static void alarm_page_class_init (AlarmPageClass *class);
static void alarm_page_init (AlarmPage *apage);
static void alarm_page_finalize (GObject *object);

static GtkWidget *alarm_page_get_widget (CompEditorPage *page);
static void alarm_page_focus_main_widget (CompEditorPage *page);
static gboolean alarm_page_fill_widgets (CompEditorPage *page, ECalComponent *comp);
static gboolean alarm_page_fill_component (CompEditorPage *page, ECalComponent *comp);
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

E_MAKE_TYPE (alarm_page, "AlarmPage", AlarmPage, alarm_page_class_init,
	     alarm_page_init, TYPE_COMP_EDITOR_PAGE);

/* Class initialization function for the alarm page */
static void
alarm_page_class_init (AlarmPageClass *class)
{
	CompEditorPageClass *editor_page_class;
	GObjectClass *gobject_class;

	editor_page_class = (CompEditorPageClass *) class;
	gobject_class = (GObjectClass *) class;

	parent_class = g_type_class_ref (TYPE_COMP_EDITOR_PAGE);

	editor_page_class->get_widget = alarm_page_get_widget;
	editor_page_class->focus_main_widget = alarm_page_focus_main_widget;
	editor_page_class->fill_widgets = alarm_page_fill_widgets;
	editor_page_class->fill_component = alarm_page_fill_component;
	editor_page_class->set_summary = alarm_page_set_summary;
	editor_page_class->set_dates = alarm_page_set_dates;

	gobject_class->finalize = alarm_page_finalize;
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
	 * set a correct description if none is set */
	priv->alarm = e_cal_component_alarm_new ();

	icalcomp = e_cal_component_alarm_get_icalcomponent (priv->alarm);
	icalprop = icalproperty_new_x ("1");
	icalproperty_set_x_name (icalprop, "X-EVOLUTION-NEEDS-DESCRIPTION");
        icalcomponent_add_property (icalcomp, icalprop);

	priv->updating = FALSE;
	priv->old_summary = NULL;
}

/* Destroy handler for the alarm page */
static void
alarm_page_finalize (GObject *object)
{
	AlarmPage *apage;
	AlarmPagePrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_ALARM_PAGE (object));

	apage = ALARM_PAGE (object);
	priv = apage->priv;

	if (priv->main)
		gtk_widget_unref (priv->main);

	if (priv->xml) {
		g_object_unref (priv->xml);
		priv->xml = NULL;
	}

	if (priv->alarm) {
		e_cal_component_alarm_free (priv->alarm);
		priv->alarm = NULL;
	}

	if (priv->list_store) {
		g_object_unref (priv->list_store);
		priv->list_store = NULL;
	}

	if (priv->old_summary) {
		g_free (priv->old_summary);
		priv->old_summary = NULL;
	}

	g_free (priv);
	apage->priv = NULL;

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
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
	e_dialog_option_menu_set (priv->action, E_CAL_COMPONENT_ALARM_DISPLAY, action_map);
	e_dialog_spin_set (priv->interval_value, 15);
	e_dialog_option_menu_set (priv->value_units, MINUTES, value_map);
	e_dialog_option_menu_set (priv->relative, BEFORE, relative_map);
	e_dialog_option_menu_set (priv->time, E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START, time_map);

	/* List data */
	e_alarm_list_clear (priv->list_store);
}

static void
sensitize_buttons (AlarmPage *apage)
{
	AlarmPagePrivate *priv;
	ECal *client;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	gboolean have_selected, read_only, sensitivity;

	priv = apage->priv;

	client = COMP_EDITOR_PAGE (apage)->client;
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->list));
	have_selected = gtk_tree_selection_get_selected (selection, NULL, &iter);

	if (e_cal_is_read_only (client, &read_only, NULL) && read_only)
		sensitivity = FALSE;
	else
		sensitivity = TRUE;

	if (e_cal_get_one_alarm_only (COMP_EDITOR_PAGE (apage)->client) && have_selected)
		gtk_widget_set_sensitive (apage->priv->add, FALSE);
	else
		gtk_widget_set_sensitive (apage->priv->add, sensitivity);
	gtk_widget_set_sensitive (priv->delete, have_selected && !read_only ? TRUE : FALSE);
}

/* Appends an alarm to the list */
static void
append_reminder (AlarmPage *apage, ECalComponentAlarm *alarm)
{
	AlarmPagePrivate *priv;
	GtkTreeView *view;
	GtkTreeIter  iter;

	priv = apage->priv;
	view = GTK_TREE_VIEW (priv->list);

	e_alarm_list_append (priv->list_store, &iter, alarm);
	gtk_tree_selection_select_iter (gtk_tree_view_get_selection (view), &iter);

	sensitize_buttons (apage);
}

/* fill_widgets handler for the alarm page */
static gboolean
alarm_page_fill_widgets (CompEditorPage *page, ECalComponent *comp)
{
	AlarmPage *apage;
	AlarmPagePrivate *priv;
	GtkWidget *menu;
	ECalComponentText text;
	GList *alarms, *l;
	CompEditorPageDates dates;
	int i;
	
	apage = ALARM_PAGE (page);
	priv = apage->priv;

	/* Don't send off changes during this time */
	priv->updating = TRUE;

	/* Clean the page */
	clear_widgets (apage);

	/* Summary */
	e_cal_component_get_summary (comp, &text);
	alarm_page_set_summary (page, text.value);

	/* Dates */
	comp_editor_dates (&dates, comp);
	alarm_page_set_dates (page, &dates);
	comp_editor_free_dates (&dates);

	/* List */
	if (!e_cal_component_has_alarms (comp))
		goto out;

	alarms = e_cal_component_get_alarm_uids (comp);

	for (l = alarms; l != NULL; l = l->next) {
		ECalComponentAlarm *ca, *ca_copy;
		const char *auid;

		auid = l->data;
		ca = e_cal_component_get_alarm (comp, auid);
		g_assert (ca != NULL);

		ca_copy = e_cal_component_alarm_clone (ca);
		e_cal_component_alarm_free (ca);

		append_reminder (apage, ca_copy);
	}
	cal_obj_uid_list_free (alarms);

 out:

	/* Alarm types */
	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->action));
	for (i = 0, l = GTK_MENU_SHELL (menu)->children; action_map[i] != -1; i++, l = l->next) {
		if (e_cal_get_static_capability (page->client, action_map_cap[i]))
			gtk_widget_set_sensitive (l->data, FALSE);
		else
			gtk_widget_set_sensitive (l->data, TRUE);
	}
	
	sensitize_buttons (apage);

	priv->updating = FALSE;

	return TRUE;
}

/* fill_component handler for the alarm page */
static gboolean
alarm_page_fill_component (CompEditorPage *page, ECalComponent *comp)
{
	AlarmPage *apage;
	AlarmPagePrivate *priv;
	GtkTreeView *view;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid_iter;
	GList *list, *l;

	apage = ALARM_PAGE (page);
	priv = apage->priv;

	/* Remove all the alarms from the component */

	list = e_cal_component_get_alarm_uids (comp);
	for (l = list; l; l = l->next) {
		const char *auid;

		auid = l->data;
		e_cal_component_remove_alarm (comp, auid);
	}
	cal_obj_uid_list_free (list);

	/* Add the new alarms */

	view  = GTK_TREE_VIEW  (priv->list);
	model = GTK_TREE_MODEL (priv->list_store);

	for (valid_iter = gtk_tree_model_get_iter_first (model, &iter); valid_iter;
	     valid_iter = gtk_tree_model_iter_next (model, &iter)) {
		ECalComponentAlarm *alarm, *alarm_copy;
		icalcomponent *icalcomp;
		icalproperty *icalprop;

		alarm = (ECalComponentAlarm *) e_alarm_list_get_alarm (priv->list_store, &iter);
		g_assert (alarm != NULL);

		/* We set the description of the alarm if it's got
		 * the X-EVOLUTION-NEEDS-DESCRIPTION property.
		 */
		icalcomp = e_cal_component_alarm_get_icalcomponent (alarm);
		icalprop = icalcomponent_get_first_property (icalcomp, ICAL_X_PROPERTY);
		while (icalprop) {
			const char *x_name;
			ECalComponentText summary;

			x_name = icalproperty_get_x_name (icalprop);
			if (!strcmp (x_name, "X-EVOLUTION-NEEDS-DESCRIPTION")) {
				e_cal_component_get_summary (comp, &summary);
				e_cal_component_alarm_set_description (alarm, &summary);

				icalcomponent_remove_property (icalcomp, icalprop);
				break;
			}

			icalprop = icalcomponent_get_next_property (icalcomp, ICAL_X_PROPERTY);
		}

		/* We clone the alarm to maintain the invariant that the alarm
		 * structures in the list did *not* come from the component.
		 */

		alarm_copy = e_cal_component_alarm_clone (alarm);
		e_cal_component_add_alarm (comp, alarm_copy);
		e_cal_component_alarm_free (alarm_copy);
	}

	return TRUE;
}

/* set_summary handler for the alarm page */
static void
alarm_page_set_summary (CompEditorPage *page, const char *summary)
{
	AlarmPage *apage;
	AlarmPagePrivate *priv;

	apage = ALARM_PAGE (page);
	priv = apage->priv;

	gtk_label_set_text (GTK_LABEL (priv->summary), summary);

	/* iterate over all alarms */
	if (priv->old_summary) {
		GtkTreeView *view;
		GtkTreeModel *model;
		GtkTreeIter iter;
		gboolean valid_iter;

		view = GTK_TREE_VIEW (priv->list);
		model = GTK_TREE_MODEL (priv->list_store);

		for (valid_iter = gtk_tree_model_get_iter_first (model, &iter); valid_iter;
		     valid_iter = gtk_tree_model_iter_next (model, &iter)) {
			ECalComponentAlarm *alarm;
			ECalComponentText desc;

			alarm = (ECalComponentAlarm *) e_alarm_list_get_alarm (priv->list_store, &iter);
			g_assert (alarm != NULL);

			e_cal_component_alarm_get_description (alarm, &desc);
			if (desc.value && *desc.value) {
				if (!strcmp (desc.value, priv->old_summary)) {
					desc.value = summary;
					e_cal_component_alarm_set_description (alarm, &desc);
				}
			}
		}

		g_free (priv->old_summary);
	}
	
	/* update old summary */
	priv->old_summary = g_strdup (summary);
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



/* Gets the widgets from the XML file and returns TRUE if they are all available. */
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
	accel_groups = gtk_accel_groups_from_object (G_OBJECT (toplevel));
	if (accel_groups) {
		page->accel_group = accel_groups->data;
		gtk_accel_group_ref (page->accel_group);
	}

	gtk_widget_ref (priv->main);
	gtk_container_remove (GTK_CONTAINER (priv->main->parent), priv->main);

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
	ECalComponentAlarm *alarm;
	ECalComponentAlarmTrigger trigger;
	ECalComponentAlarmAction action;
	
	apage = ALARM_PAGE (data);
	priv = apage->priv;

	alarm = e_cal_component_alarm_clone (priv->alarm);

	memset (&trigger, 0, sizeof (ECalComponentAlarmTrigger));
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
	e_cal_component_alarm_set_trigger (alarm, trigger);

	action = e_dialog_option_menu_get (priv->action, action_map);
	e_cal_component_alarm_set_action (alarm, action);
	if (action == E_CAL_COMPONENT_ALARM_EMAIL && !e_cal_component_alarm_has_attendees (alarm)) {
		char *email;

		if (!e_cal_get_static_capability (COMP_EDITOR_PAGE (apage)->client, CAL_STATIC_CAPABILITY_NO_EMAIL_ALARMS)
		    && e_cal_get_alarm_email_address (COMP_EDITOR_PAGE (apage)->client, &email, NULL)) {
			ECalComponentAttendee *a;
			GSList attendee_list;

			a = g_new0 (ECalComponentAttendee, 1);
			a->value = email;
			attendee_list.data = a;
			attendee_list.next = NULL;
			e_cal_component_alarm_set_attendee_list (alarm, &attendee_list);
			g_free (email);
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
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreePath *path;
	gboolean valid_iter;

	apage = ALARM_PAGE (data);
	priv = apage->priv;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->list));
	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		g_warning ("Could not get a selection to delete.");
		return;
	}

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->list_store), &iter);
	e_alarm_list_remove (priv->list_store, &iter);

	/* Select closest item after removal */
	valid_iter = gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->list_store), &iter, path);
	if (!valid_iter) {
		gtk_tree_path_prev (path);
		valid_iter = gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->list_store), &iter, path);
	}

	if (valid_iter)
		gtk_tree_selection_select_iter (selection, &iter);

	sensitize_buttons (apage);

	gtk_tree_path_free (path);
}

/* Callback used when the alarm options button is clicked */
static void
button_options_clicked_cb (GtkWidget *widget, gpointer data)
{
	AlarmPage *apage;
	AlarmPagePrivate *priv;
	gboolean repeat;
	char *email;
	
	apage = ALARM_PAGE (data);
	priv = apage->priv;

	e_cal_component_alarm_set_action (priv->alarm,
					e_dialog_option_menu_get (priv->action, action_map));

	repeat = !e_cal_get_static_capability (COMP_EDITOR_PAGE (apage)->client,
						    CAL_STATIC_CAPABILITY_NO_ALARM_REPEAT);

	if (e_cal_get_static_capability (COMP_EDITOR_PAGE (apage)->client, CAL_STATIC_CAPABILITY_NO_EMAIL_ALARMS)
                   || e_cal_get_alarm_email_address (COMP_EDITOR_PAGE (apage)->client, &email, NULL)) {
		if (!alarm_options_dialog_run (priv->alarm, email, repeat))
			g_message ("button_options_clicked_cb(): Could not create the alarm options dialog");
	}
}

/* Hooks the widget signals */
static void
init_widgets (AlarmPage *apage)
{
	AlarmPagePrivate *priv;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell_renderer;

	priv = apage->priv;

	/* Reminder buttons */
	g_signal_connect ((priv->add), "clicked",
			  G_CALLBACK (add_clicked_cb), apage);
	g_signal_connect ((priv->delete), "clicked",
			  G_CALLBACK (delete_clicked_cb), apage);

	/* Connect the default signal handler to use to make sure we notify
	 * upstream of changes to the widget values.
	 */
	g_signal_connect ((priv->add), "clicked",
			  G_CALLBACK (field_changed_cb), apage);
	g_signal_connect ((priv->delete), "clicked",
			  G_CALLBACK (field_changed_cb), apage);

	/* Options button */
	g_signal_connect ((priv->button_options), "clicked",
			  G_CALLBACK (button_options_clicked_cb), apage);

	/* Alarm list */

	/* Model */
	priv->list_store = e_alarm_list_new ();
	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->list),
				 GTK_TREE_MODEL (priv->list_store));

	/* View */
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Action/Trigger"));
	cell_renderer = GTK_CELL_RENDERER (gtk_cell_renderer_text_new ());
	gtk_tree_view_column_pack_start (column, cell_renderer, TRUE);
	gtk_tree_view_column_add_attribute (column, cell_renderer, "text", E_ALARM_LIST_COLUMN_DESCRIPTION);
	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->list), column);

#if 0
	/* If we want the alarm setup widgets to reflect the currently selected alarm, we
	 * need to do something like this */
	g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->list)), "changed",
			  G_CALLBACK (alarm_selection_changed_cb), apage);
#endif
}



static void
client_changed_cb (CompEditorPage *page, ECal *client, gpointer user_data)
{
	AlarmPage *apage = ALARM_PAGE (page);

	sensitize_buttons (apage);
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
				   NULL, NULL);
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

	g_signal_connect (G_OBJECT (apage), "client_changed",
			  G_CALLBACK (client_changed_cb), NULL);

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

	apage = g_object_new (TYPE_ALARM_PAGE, NULL);
	if (!alarm_page_construct (apage)) {
		g_object_unref (apage);
		return NULL;
	}

	return apage;
}
