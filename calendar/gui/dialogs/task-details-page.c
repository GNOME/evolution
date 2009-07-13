/*
 * Evolution calendar - task details page
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Federico Mena-Quintero <federico@ximian.com>
 *      Miguel de Icaza <miguel@ximian.com>
 *      Seth Alves <alves@hungry.com>
 *      JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glade/glade.h>
#include <misc/e-dateedit.h>
#include <misc/e-url-entry.h>
#include "e-util/e-dialog-widgets.h"
#include "e-util/e-util-private.h"
#include "../calendar-config.h"
#include "../e-timezone-entry.h"
#include "comp-editor-util.h"
#include "task-details-page.h"

#define TASK_DETAILS_PAGE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), TYPE_TASK_DETAILS_PAGE, TaskDetailsPagePrivate))

struct _TaskDetailsPagePrivate {
	/* Glade XML data */
	GladeXML *xml;

	/* Widgets from the Glade file */
	GtkWidget *main;

	GtkWidget *status_combo;
	GtkWidget *priority_combo;
	GtkWidget *percent_complete;

	GtkWidget *date_completed_label;
	GtkWidget *completed_date;

	GtkWidget *url_label;
	GtkWidget *url_entry;
	GtkWidget *url;
};

/* Note that these two arrays must match. */
static const gint status_map[] = {
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
	PRIORITY_UNDEFINED
} TaskEditorPriority;

static const gint priority_map[] = {
	PRIORITY_HIGH,
	PRIORITY_NORMAL,
	PRIORITY_LOW,
	PRIORITY_UNDEFINED,
	-1
};

static GtkWidget *task_details_page_get_widget (CompEditorPage *page);
static void task_details_page_focus_main_widget (CompEditorPage *page);
static gboolean task_details_page_fill_widgets (CompEditorPage *page, ECalComponent *comp);
static gboolean task_details_page_fill_component (CompEditorPage *page, ECalComponent *comp);
static gboolean task_details_page_fill_timezones (CompEditorPage *page, GHashTable *timezones);

G_DEFINE_TYPE (TaskDetailsPage, task_details_page, TYPE_COMP_EDITOR_PAGE)

static void
task_details_page_dispose (GObject *object)
{
	TaskDetailsPagePrivate *priv;

	priv = TASK_DETAILS_PAGE_GET_PRIVATE (object);

	if (priv->main != NULL) {
		g_object_unref (priv->main);
		priv->main = NULL;
	}

	if (priv->xml != NULL) {
		g_object_unref (priv->xml);
		priv->xml = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (task_details_page_parent_class)->dispose (object);
}

static void
task_details_page_class_init (TaskDetailsPageClass *class)
{
	GObjectClass *object_class;
	CompEditorPageClass *editor_page_class;

	g_type_class_add_private (class, sizeof (TaskDetailsPagePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = task_details_page_dispose;

	editor_page_class = COMP_EDITOR_PAGE_CLASS (class);
	editor_page_class->get_widget = task_details_page_get_widget;
	editor_page_class->focus_main_widget = task_details_page_focus_main_widget;
	editor_page_class->fill_widgets = task_details_page_fill_widgets;
	editor_page_class->fill_component = task_details_page_fill_component;
	editor_page_class->fill_timezones = task_details_page_fill_timezones;
}

static void
task_details_page_init (TaskDetailsPage *tdpage)
{
	tdpage->priv = TASK_DETAILS_PAGE_GET_PRIVATE (tdpage);
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

	gtk_widget_grab_focus (priv->status_combo);
}

static TaskEditorPriority
priority_value_to_index (gint priority_value)
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

static gint
priority_index_to_value (TaskEditorPriority priority)
{
	gint retval;

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
		retval = 0;
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
	TaskDetailsPagePrivate *priv = tdpage->priv;
	CompEditor *editor;
	ECal *client;
	gboolean read_only;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (tdpage));
	client = comp_editor_get_client (editor);

	if (!e_cal_is_read_only (client, &read_only, NULL))
		read_only = TRUE;

	gtk_widget_set_sensitive (priv->status_combo, !read_only);
	gtk_widget_set_sensitive (priv->priority_combo, !read_only);
	gtk_widget_set_sensitive (priv->percent_complete, !read_only);
	gtk_widget_set_sensitive (priv->completed_date, !read_only);
	gtk_widget_set_sensitive (priv->url_label, !read_only);
	gtk_editable_set_editable (GTK_EDITABLE (e_url_entry_get_entry (E_URL_ENTRY (priv->url_entry))), !read_only);
}

/* fill_widgets handler for the task page */
static gboolean
task_details_page_fill_widgets (CompEditorPage *page, ECalComponent *comp)
{
	TaskDetailsPage *tdpage;
	TaskDetailsPagePrivate *priv;
	gint *priority_value, *percent = NULL;
	TaskEditorPriority priority;
	icalproperty_status status;
	const gchar *url;
	struct icaltimetype *completed = NULL;

	tdpage = TASK_DETAILS_PAGE (page);
	priv = tdpage->priv;

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
	e_dialog_combo_box_set (priv->status_combo, status, status_map);

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
	e_dialog_combo_box_set (priv->priority_combo, priority, priority_map);

	/* URL */
	e_cal_component_get_url (comp, &url);
	e_dialog_editable_set (priv->url, url);

	sensitize_widgets (tdpage);

	return TRUE;
}

/* fill_component handler for the task page */
static gboolean
task_details_page_fill_component (CompEditorPage *page, ECalComponent *comp)
{
	TaskDetailsPage *tdpage;
	TaskDetailsPagePrivate *priv;
	struct icaltimetype icalcomplete, icaltoday;
	icalproperty_status status;
	TaskEditorPriority priority;
	gint priority_value, percent;
	gchar *url;
	gboolean date_set;
	icaltimezone *zone = calendar_config_get_icaltimezone ();

	tdpage = TASK_DETAILS_PAGE (page);
	priv = tdpage->priv;

	/* Percent Complete. */
	percent = e_dialog_spin_get_int (priv->percent_complete);
	e_cal_component_set_percent (comp, &percent);

	/* Status. */
	status = e_dialog_combo_box_get (priv->status_combo, status_map);
	e_cal_component_set_status (comp, status);

	/* Priority. */
	priority = e_dialog_combo_box_get (priv->priority_combo, priority_map);
	priority_value = priority_index_to_value (priority);
	e_cal_component_set_priority (comp, &priority_value);

	icalcomplete = icaltime_null_time ();

	/* COMPLETED must be in UTC. */
	icalcomplete.is_utc = 1;

	/* Completed Date. */
	if (!e_date_edit_date_is_valid (E_DATE_EDIT (priv->completed_date)) ||
	    !e_date_edit_time_is_valid (E_DATE_EDIT (priv->completed_date))) {
		comp_editor_page_display_validation_error (page, _("Completed date is wrong"), priv->completed_date);
		return FALSE;
	}

	date_set = e_date_edit_get_date (E_DATE_EDIT (priv->completed_date),
					 &icalcomplete.year,
					 &icalcomplete.month,
					 &icalcomplete.day);

	if (date_set) {
		e_date_edit_get_time_of_day (E_DATE_EDIT (priv->completed_date),
				&icalcomplete.hour,
				&icalcomplete.minute);

		/* COMPLETED today or before */
		icaltoday = icaltime_current_time_with_zone (zone);
		icaltimezone_convert_time (&icaltoday, zone,
				icaltimezone_get_utc_timezone());

		if (icaltime_compare_date_only (icalcomplete, icaltoday) > 0) {
			comp_editor_page_display_validation_error (page, _("Completed date is wrong"), priv->completed_date);
			return FALSE;
		}

		/* COMPLETED must be in UTC, so we assume that the date in the
		   dialog is in the current timezone, and we now convert it
		   to UTC. FIXME: We should really use one timezone for the
		   entire time the dialog is shown. Otherwise if the user
		   changes the timezone, the COMPLETED date may get changed
		   as well. */
		icaltimezone_convert_time (&icalcomplete, zone,
				icaltimezone_get_utc_timezone ());
		e_cal_component_set_completed (comp, &icalcomplete);
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
	icaltimezone *zone;

	/* add UTC timezone, which is the one used for the DATE-COMPLETED property */
	zone = icaltimezone_get_utc_timezone ();
	if (zone) {
		if (!g_hash_table_lookup (timezones, icaltimezone_get_tzid (zone)))
			g_hash_table_insert (timezones, (gpointer) icaltimezone_get_tzid (zone), zone);
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
	if (accel_groups)
		page->accel_group = g_object_ref (accel_groups->data);

	g_object_ref (priv->main);
	gtk_container_remove (GTK_CONTAINER (priv->main->parent), priv->main);

	priv->status_combo = GW ("status-combobox");
	priv->priority_combo = GW ("priority-combobox");
	priv->percent_complete = GW ("percent-complete");

	priv->date_completed_label = GW ("date_completed_label");

	priv->completed_date = GW ("completed-date");
	gtk_widget_show (priv->completed_date);

	priv->url_label = GW ("url_label");

	priv->url_entry = GW ("url_entry");
	gtk_widget_show (priv->url_entry);
	priv->url = e_url_entry_get_entry (E_URL_ENTRY (priv->url_entry));
	atk_object_set_name (gtk_widget_get_accessible (priv->url), _("Web Page"));

#undef GW

	return (priv->status_combo
		&& priv->priority_combo
		&& priv->percent_complete
		&& priv->date_completed_label
		&& priv->completed_date
		&& priv->url_label
		&& priv->url);
}

static void
complete_date_changed (TaskDetailsPage *tdpage, time_t ctime, gboolean complete)
{
	CompEditorPageDates dates = {NULL, NULL, NULL, NULL};
	icaltimezone *zone;
	struct icaltimetype completed_tt = icaltime_null_time();

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

	if (comp_editor_page_get_updating (COMP_EDITOR_PAGE (tdpage)))
		return;

	comp_editor_page_set_updating (COMP_EDITOR_PAGE (tdpage), TRUE);

	date_set = e_date_edit_get_date (E_DATE_EDIT (priv->completed_date),
					 &completed_tt.year,
					 &completed_tt.month,
					 &completed_tt.day);
	e_date_edit_get_time_of_day (E_DATE_EDIT (priv->completed_date),
				     &completed_tt.hour,
				     &completed_tt.minute);

	status = e_dialog_combo_box_get (priv->status_combo, status_map);

	if (!date_set) {
		completed_tt = icaltime_null_time ();
		if (status == ICAL_STATUS_COMPLETED) {
			e_dialog_combo_box_set (priv->status_combo,
						  ICAL_STATUS_NONE,
						  status_map);
			e_dialog_spin_set (priv->percent_complete, 0);
		}
	} else {
		if (status != ICAL_STATUS_COMPLETED) {
			e_dialog_combo_box_set (priv->status_combo,
						  ICAL_STATUS_COMPLETED,
						  status_map);
		}
		e_dialog_spin_set (priv->percent_complete, 100);
	}

	comp_editor_page_set_updating (COMP_EDITOR_PAGE (tdpage), FALSE);

	/* Notify upstream */
	dates.complete = &completed_tt;
	comp_editor_page_notify_dates_changed (COMP_EDITOR_PAGE (tdpage), &dates);
}

static void
status_changed (GtkWidget *combo, TaskDetailsPage *tdpage)
{
	TaskDetailsPagePrivate *priv;
	icalproperty_status status;
	CompEditor *editor;
	time_t ctime = -1;

	priv = tdpage->priv;

	if (comp_editor_page_get_updating (COMP_EDITOR_PAGE (tdpage)))
		return;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (tdpage));

	comp_editor_page_set_updating (COMP_EDITOR_PAGE (tdpage), TRUE);

	status = e_dialog_combo_box_get (priv->status_combo, status_map);
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

	comp_editor_page_set_updating (COMP_EDITOR_PAGE (tdpage), FALSE);

	comp_editor_set_changed (editor, TRUE);
}

static void
percent_complete_changed (GtkAdjustment	*adj, TaskDetailsPage *tdpage)
{
	TaskDetailsPagePrivate *priv;
	gint percent;
	icalproperty_status status;
	CompEditor *editor;
	gboolean complete;
	time_t ctime = -1;

	priv = tdpage->priv;

	if (comp_editor_page_get_updating (COMP_EDITOR_PAGE (tdpage)))
		return;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (tdpage));

	comp_editor_page_set_updating (COMP_EDITOR_PAGE (tdpage), TRUE);

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

	e_dialog_combo_box_set (priv->status_combo, status, status_map);
	e_date_edit_set_time (E_DATE_EDIT (priv->completed_date), ctime);
	complete_date_changed (tdpage, ctime, complete);

	comp_editor_page_set_updating (COMP_EDITOR_PAGE (tdpage), FALSE);

	comp_editor_set_changed (editor, TRUE);
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
	g_signal_connect (GTK_COMBO_BOX (priv->status_combo),
			    "changed",
			    G_CALLBACK (status_changed), tdpage);

	g_signal_connect((GTK_SPIN_BUTTON (priv->percent_complete)->adjustment),
			    "value_changed",
			    G_CALLBACK (percent_complete_changed), tdpage);

	/* Priority */
	g_signal_connect_swapped (
		GTK_COMBO_BOX (priv->priority_combo), "changed",
		G_CALLBACK (comp_editor_page_changed), tdpage);

	/* Completed Date */
	g_signal_connect (
		priv->completed_date, "changed",
		G_CALLBACK (date_changed_cb), tdpage);
	g_signal_connect_swapped (
		priv->completed_date, "changed",
		G_CALLBACK (comp_editor_page_changed), tdpage);

	/* URL */
	g_signal_connect_swapped (
		priv->url, "changed",
		G_CALLBACK (comp_editor_page_changed), tdpage);
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
	TaskDetailsPagePrivate *priv = tdpage->priv;
	CompEditor *editor;
	gchar *gladefile;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (tdpage));

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      "task-details-page.glade",
				      NULL);
	priv->xml = glade_xml_new (gladefile, NULL, NULL);
	g_free (gladefile);

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

	g_signal_connect_swapped (
		editor, "notify::client",
		G_CALLBACK (sensitize_widgets), tdpage);

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
task_details_page_new (CompEditor *editor)
{
	TaskDetailsPage *tdpage;

	tdpage = g_object_new (TYPE_TASK_DETAILS_PAGE, "editor", editor, NULL);
	if (!task_details_page_construct (tdpage)) {
		g_object_unref (tdpage);
		g_return_val_if_reached (NULL);
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

