/* Evolution calendar - task details page
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

#include <gtk/gtksignal.h>
#include <gtk/gtktogglebutton.h>
#include <libgnome/gnome-i18n.h>
#include <glade/glade.h>
#include <widgets/misc/e-dateedit.h>
#include <widgets/misc/e-url-entry.h>
#include "e-util/e-dialog-widgets.h"
#include "../calendar-config.h"
#include "../e-timezone-entry.h"
#include "comp-editor-util.h"
#include "task-details-page.h"



/* Private part of the TaskDetailsPage structure */
struct _TaskDetailsPagePrivate {
	/* Glade XML data */
	GladeXML *xml;

	/* Widgets from the Glade file */
	GtkWidget *main;

	GtkWidget *status;
	GtkWidget *priority;
	GtkWidget *percent_complete;

	GtkWidget *date_completed_label;
	GtkWidget *completed_date;

	GtkWidget *url_label;
	GtkWidget *url_entry;
	GtkWidget *url;

	gboolean updating;
};

/* Note that these two arrays must match. */
static const int status_map[] = {
	ICAL_STATUS_NONE,
	ICAL_STATUS_INPROCESS,
	ICAL_STATUS_COMPLETED,
	ICAL_STATUS_CANCELLED,
	-1
};

typedef enum {
	PRIORITY_HIGH,
	PRIORITY_NORMAL,
	PRIORITY_LOW,
	PRIORITY_UNDEFINED,
} TaskEditorPriority;

static const int priority_map[] = {
	PRIORITY_HIGH,
	PRIORITY_NORMAL,
	PRIORITY_LOW,
	PRIORITY_UNDEFINED,
	-1
};



static void task_details_page_class_init (TaskDetailsPageClass *class);
static void task_details_page_init (TaskDetailsPage *tdpage);
static void task_details_page_finalize (GObject *object);

static GtkWidget *task_details_page_get_widget (CompEditorPage *page);
static void task_details_page_focus_main_widget (CompEditorPage *page);
static gboolean task_details_page_fill_widgets (CompEditorPage *page, ECalComponent *comp);
static gboolean task_details_page_fill_component (CompEditorPage *page, ECalComponent *comp);
static gboolean task_details_page_fill_timezones (CompEditorPage *page, GHashTable *timezones);

static CompEditorPageClass *parent_class = NULL;



/**
 * task_details_page_get_type:
 * 
 * Registers the #TaskDetailsPage class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the #TaskDetailsPage class.
 **/

E_MAKE_TYPE (task_details_page, "TaskDetailsPage", TaskDetailsPage, task_details_page_class_init,
	     task_details_page_init, TYPE_COMP_EDITOR_PAGE);

/* Class initialization function for the task page */
static void
task_details_page_class_init (TaskDetailsPageClass *class)
{
	CompEditorPageClass *editor_page_class;
	GObjectClass *object_class;

	editor_page_class = (CompEditorPageClass *) class;
	object_class = (GObjectClass *) class;

	parent_class = g_type_class_ref(TYPE_COMP_EDITOR_PAGE);

	editor_page_class->get_widget = task_details_page_get_widget;
	editor_page_class->focus_main_widget = task_details_page_focus_main_widget;
	editor_page_class->fill_widgets = task_details_page_fill_widgets;
	editor_page_class->fill_component = task_details_page_fill_component;
	editor_page_class->fill_timezones = task_details_page_fill_timezones;

	object_class->finalize = task_details_page_finalize;
}

/* Object initialization function for the task page */
static void
task_details_page_init (TaskDetailsPage *tdpage)
{
	TaskDetailsPagePrivate *priv;

	priv = g_new0 (TaskDetailsPagePrivate, 1);
	tdpage->priv = priv;

	priv->xml = NULL;

	priv->main = NULL;

	priv->status = NULL;
	priv->priority = NULL;
	priv->percent_complete = NULL;
	
	priv->date_completed_label = NULL;
	priv->completed_date = NULL;

	priv->url_label = NULL;
	priv->url_entry = NULL;
	priv->url = NULL;

	priv->updating = FALSE;
}

/* Destroy handler for the task page */
static void
task_details_page_finalize (GObject *object)
{
	TaskDetailsPage *tdpage;
	TaskDetailsPagePrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_TASK_DETAILS_PAGE (object));

	tdpage = TASK_DETAILS_PAGE (object);
	priv = tdpage->priv;

	if (priv->main)
		gtk_widget_unref (priv->main);

	if (priv->xml) {
		g_object_unref (priv->xml);
		priv->xml = NULL;
	}

	g_free (priv);
	tdpage->priv = NULL;

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}



/* get_widget handler for the task page */
static GtkWidget *
task_details_page_get_widget (CompEditorPage *page)
{
	TaskDetailsPage *tdpage;
	TaskDetailsPagePrivate *priv;

	tdpage = TASK_DETAILS_PAGE (page);
	priv = tdpage->priv;

	return priv->main;
}

/* focus_main_widget handler for the task page */
static void
task_details_page_focus_main_widget (CompEditorPage *page)
{
	TaskDetailsPage *tdpage;
	TaskDetailsPagePrivate *priv;

	tdpage = TASK_DETAILS_PAGE (page);
	priv = tdpage->priv;

	gtk_widget_grab_focus (priv->status);
}


static TaskEditorPriority
priority_value_to_index (int priority_value)
{
	TaskEditorPriority retval;

	if (priority_value == 0)
		retval = PRIORITY_UNDEFINED;
	else if (priority_value <= 4)
		retval = PRIORITY_HIGH;
	else if (priority_value == 5)
		retval = PRIORITY_NORMAL;
	else
		retval = PRIORITY_LOW;

	return retval;
}

static int
priority_index_to_value (TaskEditorPriority priority)
{
	int retval;

	switch (priority) {
	case PRIORITY_UNDEFINED:
		retval = 0;
		break;
	case PRIORITY_HIGH:
		retval = 3;
		break;
	case PRIORITY_NORMAL:
		retval = 5;
		break;
	case PRIORITY_LOW:
		retval = 7;
		break;
	default:
		retval = -1;
		g_assert_not_reached ();
		break;
	}

	return retval;
}

/* Fills the widgets with default values */
static void
clear_widgets (TaskDetailsPage *tdpage)
{
	TaskDetailsPagePrivate *priv;

	priv = tdpage->priv;

	/* Date completed */
	e_date_edit_set_time (E_DATE_EDIT (priv->completed_date), -1);

	/* URL */
	e_dialog_editable_set (priv->url, NULL);
}

static void
sensitize_widgets (TaskDetailsPage *tdpage)
{
	gboolean read_only;
	TaskDetailsPagePrivate *priv;
	
	priv = tdpage->priv;

	if (!e_cal_is_read_only (COMP_EDITOR_PAGE (tdpage)->client, &read_only, NULL))
		read_only = TRUE;
	
	gtk_widget_set_sensitive (priv->status, !read_only);
	gtk_widget_set_sensitive (priv->priority, !read_only);
	gtk_widget_set_sensitive (priv->percent_complete, !read_only);
	gtk_widget_set_sensitive (priv->completed_date, !read_only);
	gtk_widget_set_sensitive (priv->url_label, !read_only);
	gtk_entry_set_editable (GTK_ENTRY (e_url_entry_get_entry (priv->url_entry)), !read_only);
}

/* fill_widgets handler for the task page */
static gboolean
task_details_page_fill_widgets (CompEditorPage *page, ECalComponent *comp)
{
	TaskDetailsPage *tdpage;
	TaskDetailsPagePrivate *priv;
	int *priority_value, *percent = NULL;
	TaskEditorPriority priority;
	icalproperty_status status;
	const char *url;
	struct icaltimetype *completed = NULL;

	tdpage = TASK_DETAILS_PAGE (page);
	priv = tdpage->priv;

	priv->updating = TRUE;
	
	/* Clean the screen */
	clear_widgets (tdpage);
	
	/* Percent Complete. */
	e_cal_component_get_percent (comp, &percent);
	if (percent) {
		e_dialog_spin_set (priv->percent_complete, *percent);
	} else {
		/* FIXME: Could check if task is completed and set 100%. */
		e_dialog_spin_set (priv->percent_complete, 0);
	}

	/* Status. */
	e_cal_component_get_status (comp, &status);
	if (status == ICAL_STATUS_NONE || status == ICAL_STATUS_NEEDSACTION) {
		/* Try to use the percent value. */
		if (percent) {
			if (*percent == 100)
				status = ICAL_STATUS_COMPLETED;
			else if (*percent > 0)
				status = ICAL_STATUS_INPROCESS;
			else
				status = ICAL_STATUS_NONE;
		} else
			status = ICAL_STATUS_NONE;
	}
	e_dialog_option_menu_set (priv->status, status, status_map);

	if (percent)
		e_cal_component_free_percent (percent);

	/* Completed Date. */
	e_cal_component_get_completed (comp, &completed);
	if (completed) {
		icaltimezone *utc_zone, *zone;

		/* Completed is in UTC, but that would confuse the user, so
		   we convert it to local time. */
		utc_zone = icaltimezone_get_utc_timezone ();
		zone = calendar_config_get_icaltimezone ();

		icaltimezone_convert_time (completed, utc_zone, zone);

		e_date_edit_set_date (E_DATE_EDIT (priv->completed_date),
				      completed->year, completed->month,
				      completed->day);
		e_date_edit_set_time_of_day (E_DATE_EDIT (priv->completed_date),
					     completed->hour,
					     completed->minute);

		e_cal_component_free_icaltimetype (completed);
	}

	/* Priority. */
	e_cal_component_get_priority (comp, &priority_value);
	if (priority_value) {
		priority = priority_value_to_index (*priority_value);
		e_cal_component_free_priority (priority_value);
	} else {
		priority = PRIORITY_UNDEFINED;
	}
	e_dialog_option_menu_set (priv->priority, priority, priority_map);

	/* URL */
	e_cal_component_get_url (comp, &url);
	e_dialog_editable_set (priv->url, url);
	
	priv->updating = FALSE;

	sensitize_widgets (tdpage);

	return TRUE;
}

/* fill_component handler for the task page */
static gboolean
task_details_page_fill_component (CompEditorPage *page, ECalComponent *comp)
{
	TaskDetailsPage *tdpage;
	TaskDetailsPagePrivate *priv;
	struct icaltimetype icaltime;
	icalproperty_status status;
	TaskEditorPriority priority;
	int priority_value, percent;
	char *url;
	gboolean date_set;
	
	tdpage = TASK_DETAILS_PAGE (page);
	priv = tdpage->priv;

	/* Percent Complete. */
	percent = e_dialog_spin_get_int (priv->percent_complete);
	e_cal_component_set_percent (comp, &percent);

	/* Status. */
	status = e_dialog_option_menu_get (priv->status, status_map);
	e_cal_component_set_status (comp, status);

	/* Priority. */
	priority = e_dialog_option_menu_get (priv->priority, priority_map);
	priority_value = priority_index_to_value (priority);
	e_cal_component_set_priority (comp, &priority_value);

	icaltime = icaltime_null_time ();

	/* COMPLETED must be in UTC. */
	icaltime.is_utc = 1;

	/* Completed Date. */
	if (!e_date_edit_date_is_valid (E_DATE_EDIT (priv->completed_date)) ||
	    !e_date_edit_time_is_valid (E_DATE_EDIT (priv->completed_date))) {
		comp_editor_page_display_validation_error (page, _("Completed date is wrong"), priv->completed_date);
		return FALSE;
	}

	date_set = e_date_edit_get_date (E_DATE_EDIT (priv->completed_date),
					 &icaltime.year,
					 &icaltime.month,
					 &icaltime.day);
	e_date_edit_get_time_of_day (E_DATE_EDIT (priv->completed_date),
				     &icaltime.hour,
				     &icaltime.minute);
	if (date_set) {
		/* COMPLETED must be in UTC, so we assume that the date in the
		   dialog is in the current timezone, and we now convert it
		   to UTC. FIXME: We should really use one timezone for the
		   entire time the dialog is shown. Otherwise if the user
		   changes the timezone, the COMPLETED date may get changed
		   as well. */
		icaltimezone *zone = calendar_config_get_icaltimezone ();
		icaltimezone_convert_time (&icaltime, zone,
					   icaltimezone_get_utc_timezone ());
		e_cal_component_set_completed (comp, &icaltime);
	} else {
		e_cal_component_set_completed (comp, NULL);
	}

	/* URL. */
	url = e_dialog_editable_get (priv->url);
	e_cal_component_set_url (comp, url);
	if (url)
		g_free (url);

	return TRUE;
}

/* fill_timezones handler for the event page */
static gboolean
task_details_page_fill_timezones (CompEditorPage *page, GHashTable *timezones)
{
	TaskDetailsPage *tdpage;
	TaskDetailsPagePrivate *priv;
	icaltimezone *zone;

	tdpage = TASK_DETAILS_PAGE (page);
	priv = tdpage->priv;

	/* add UTC timezone, which is the one used for the DATE-COMPLETED property */
	zone = icaltimezone_get_utc_timezone ();
	if (zone) {
		if (!g_hash_table_lookup (timezones, icaltimezone_get_tzid (zone)))
			g_hash_table_insert (timezones, icaltimezone_get_tzid (zone), zone);
	}

	return TRUE;
}



/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (TaskDetailsPage *tdpage)
{
	CompEditorPage *page = COMP_EDITOR_PAGE (tdpage);
	TaskDetailsPagePrivate *priv;
	GSList *accel_groups;
	GtkWidget *toplevel;

	priv = tdpage->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	priv->main = GW ("task-details-page");
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

	priv->status = GW ("status");
	priv->priority = GW ("priority");
	priv->percent_complete = GW ("percent-complete");

	priv->date_completed_label = GW ("date_completed_label");

	priv->completed_date = GW ("completed-date");
	gtk_widget_show (priv->completed_date);

	priv->url_label = GW ("url_label");

	priv->url_entry = GW ("url_entry");
	gtk_widget_show (priv->url_entry);
	priv->url = e_url_entry_get_entry (E_URL_ENTRY (priv->url_entry));
	
#undef GW

	return (priv->status
		&& priv->priority
		&& priv->percent_complete
		&& priv->date_completed_label
		&& priv->completed_date
		&& priv->url_label
		&& priv->url);
}


static void
complete_date_changed (TaskDetailsPage *tdpage, time_t ctime, gboolean complete)
{
	TaskDetailsPagePrivate *priv;
	CompEditorPageDates dates = {NULL, NULL, NULL, NULL};
	icaltimezone *zone;
	struct icaltimetype completed_tt = icaltime_null_time();

	priv = tdpage->priv;

	/* Get the current time in UTC. */
	zone = icaltimezone_get_utc_timezone ();
	completed_tt = icaltime_from_timet_with_zone (ctime, FALSE, zone);
	completed_tt.is_utc = TRUE;

	dates.start = NULL;
	dates.end = NULL;
	dates.due = NULL;	
	if (complete)
		dates.complete = &completed_tt;
	
	/* Notify upstream */
	comp_editor_page_notify_dates_changed (COMP_EDITOR_PAGE (tdpage),
					       &dates);
}

static void
date_changed_cb (EDateEdit *dedit, gpointer data)
{
	TaskDetailsPage *tdpage;
	TaskDetailsPagePrivate *priv;
	CompEditorPageDates dates = {NULL, NULL, NULL, NULL};
	struct icaltimetype completed_tt = icaltime_null_time ();
	icalproperty_status status;
	gboolean date_set;

	tdpage = TASK_DETAILS_PAGE (data);
	priv = tdpage->priv;

	if (priv->updating)
		return;

	priv->updating = TRUE;

	date_set = e_date_edit_get_date (E_DATE_EDIT (priv->completed_date),
					 &completed_tt.year,
					 &completed_tt.month,
					 &completed_tt.day);
	e_date_edit_get_time_of_day (E_DATE_EDIT (priv->completed_date),
				     &completed_tt.hour,
				     &completed_tt.minute);

	status = e_dialog_option_menu_get (priv->status, status_map);

	if (!date_set) {
		completed_tt = icaltime_null_time ();
		if (status == ICAL_STATUS_COMPLETED) {
			e_dialog_option_menu_set (priv->status,
						  ICAL_STATUS_NONE,
						  status_map);
			e_dialog_spin_set (priv->percent_complete, 0);
		}
	} else {
		if (status != ICAL_STATUS_COMPLETED) {
			e_dialog_option_menu_set (priv->status,
						  ICAL_STATUS_COMPLETED,
						  status_map);
		}
		e_dialog_spin_set (priv->percent_complete, 100);
	}
	
	priv->updating = FALSE;

	/* Notify upstream */
	dates.complete = &completed_tt;
	comp_editor_page_notify_dates_changed (COMP_EDITOR_PAGE (tdpage), &dates);
}

static void
status_changed (GtkMenu	*menu, TaskDetailsPage *tdpage)
{
	TaskDetailsPagePrivate *priv;
	icalproperty_status status;
	time_t ctime = -1;
	
	priv = tdpage->priv;

	if (priv->updating)
		return;

	priv->updating = TRUE;

	status = e_dialog_option_menu_get (priv->status, status_map);
	if (status == ICAL_STATUS_NONE) {
		e_dialog_spin_set (priv->percent_complete, 0);
		e_date_edit_set_time (E_DATE_EDIT (priv->completed_date), ctime);
		complete_date_changed (tdpage, 0, FALSE);
	} else if (status == ICAL_STATUS_INPROCESS) {
		gint percent_complete = e_dialog_spin_get_int (priv->percent_complete);
		if (percent_complete <= 0 || percent_complete >= 100)
			e_dialog_spin_set (priv->percent_complete, 50);

		e_date_edit_set_time (E_DATE_EDIT (priv->completed_date), ctime);
		complete_date_changed (tdpage, 0, FALSE);
	} else if (status == ICAL_STATUS_COMPLETED) {
		e_dialog_spin_set (priv->percent_complete, 100);
		ctime = time (NULL);
		e_date_edit_set_time (E_DATE_EDIT (priv->completed_date), ctime);
		complete_date_changed (tdpage, ctime, TRUE);
	}

	priv->updating = FALSE;

	comp_editor_page_notify_changed (COMP_EDITOR_PAGE (tdpage));
}

static void
percent_complete_changed (GtkAdjustment	*adj, TaskDetailsPage *tdpage)
{
	TaskDetailsPagePrivate *priv;
	gint percent;
	icalproperty_status status;
	gboolean complete;
	time_t ctime = -1;
	
	priv = tdpage->priv;

	if (priv->updating)
		return;
	
	priv->updating = TRUE;

	percent = e_dialog_spin_get_int (priv->percent_complete);
	if (percent == 100) {
		complete = TRUE;
		ctime = time (NULL);
		status = ICAL_STATUS_COMPLETED;
	} else {
		complete = FALSE;

		if (percent == 0)
			status = ICAL_STATUS_NONE;
		else
			status = ICAL_STATUS_INPROCESS;
	}

	e_dialog_option_menu_set (priv->status, status, status_map);
	e_date_edit_set_time (E_DATE_EDIT (priv->completed_date), ctime);
	complete_date_changed (tdpage, ctime, complete);

	priv->updating = FALSE;

	comp_editor_page_notify_changed (COMP_EDITOR_PAGE (tdpage));
}

/* This is called when any field is changed; it notifies upstream. */
static void
field_changed_cb (GtkWidget *widget, gpointer data)
{
	TaskDetailsPage *tdpage;
	TaskDetailsPagePrivate *priv;
	
	tdpage = TASK_DETAILS_PAGE (data);
	priv = tdpage->priv;

	if (!priv->updating)
		comp_editor_page_notify_changed (COMP_EDITOR_PAGE (tdpage));
}

/* Hooks the widget signals */
static void
init_widgets (TaskDetailsPage *tdpage)
{
	TaskDetailsPagePrivate *priv;

	priv = tdpage->priv;

	/* Make sure the EDateEdit widgets use our timezones to get the
	   current time. */
	e_date_edit_set_get_time_callback (E_DATE_EDIT (priv->completed_date),
					   (EDateEditGetTimeCallback) comp_editor_get_current_time,
					   tdpage, NULL);

	/* These are created by hand, so hook the mnemonics manually */
	gtk_label_set_mnemonic_widget (GTK_LABEL (priv->date_completed_label), priv->completed_date);
	gtk_label_set_mnemonic_widget (GTK_LABEL (priv->url_label), priv->url_entry);

	/* Connect signals. The Status, Percent Complete & Date Completed
	   properties are closely related so whenever one changes we may need
	   to update the other 2. */
	g_signal_connect((GTK_OPTION_MENU (priv->status)->menu),
			    "deactivate",
			    G_CALLBACK (status_changed), tdpage);

	g_signal_connect((GTK_SPIN_BUTTON (priv->percent_complete)->adjustment),
			    "value_changed",
			    G_CALLBACK (percent_complete_changed), tdpage);

	/* Priority */
	g_signal_connect((GTK_OPTION_MENU (priv->priority)->menu),
			    "deactivate",
			    G_CALLBACK (field_changed_cb), tdpage);

	/* Completed Date */
	g_signal_connect((priv->completed_date), "changed",
			    G_CALLBACK (date_changed_cb), tdpage);

	/* URL */
	g_signal_connect((priv->url), "changed",
			    G_CALLBACK (field_changed_cb), tdpage);
}

static void
client_changed_cb (CompEditorPage *page, ECal *client, gpointer user_data)
{
	TaskDetailsPage *tdpage = TASK_DETAILS_PAGE (page);

	sensitize_widgets (tdpage);
}

/**
 * task_details_page_construct:
 * @tdpage: An task details page.
 * 
 * Constructs an task page by loading its Glade data.
 * 
 * Return value: The same object as @tdpage, or NULL if the widgets could not 
 * be created.
 **/
TaskDetailsPage *
task_details_page_construct (TaskDetailsPage *tdpage)
{
	TaskDetailsPagePrivate *priv;

	priv = tdpage->priv;

	priv->xml = glade_xml_new (EVOLUTION_GLADEDIR 
				   "/task-details-page.glade", NULL, NULL);
	if (!priv->xml) {
		g_message ("task_details_page_construct(): "
			   "Could not load the Glade XML file!");
		return NULL;
	}

	if (!get_widgets (tdpage)) {
		g_message ("task_details_page_construct(): "
			   "Could not find all widgets in the XML file!");
		return NULL;
	}

	init_widgets (tdpage);

	g_signal_connect_after (G_OBJECT (tdpage), "client_changed",
				G_CALLBACK (client_changed_cb), NULL);

	return tdpage;
}

/**
 * task_details_page_new:
 * 
 * Creates a new task details page.
 * 
 * Return value: A newly-created task details page, or NULL if the page could
 * not be created.
 **/
TaskDetailsPage *
task_details_page_new (void)
{
	TaskDetailsPage *tdpage;

	tdpage = g_object_new (TYPE_TASK_DETAILS_PAGE, NULL);
	if (!task_details_page_construct (tdpage)) {
		g_object_unref (tdpage);
		return NULL;
	}

	return tdpage;
}

GtkWidget *task_details_page_create_date_edit (void);

GtkWidget *
task_details_page_create_date_edit (void)
{
	GtkWidget *dedit;

	dedit = comp_editor_new_date_edit (TRUE, TRUE, FALSE);
	e_date_edit_set_allow_no_date_set (E_DATE_EDIT (dedit), TRUE);

	return dedit;
}

