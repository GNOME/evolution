/* Evolution calendar - Main page of the task editor dialog
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
#include <gtk/gtktext.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtkoptionmenu.h>
#include <glade/glade.h>
#include <gal/widgets/e-unicode.h>
#include <gal/widgets/e-categories.h>
#include <widgets/misc/e-dateedit.h>
#include "e-util/e-dialog-widgets.h"
#include "e-util/e-categories-config.h"
#include "../e-timezone-entry.h"
#include "../calendar-config.h"
#include "comp-editor-util.h"
#include "task-page.h"



/* Private part of the TaskPage structure */
struct _TaskPagePrivate {
	/* Glade XML data */
	GladeXML *xml;

	/* Widgets from the Glade file */
	GtkWidget *main;

	GtkWidget *summary;

	GtkWidget *due_date;
	GtkWidget *start_date;
	GtkWidget *due_timezone;
	GtkWidget *start_timezone;

	GtkWidget *description;

	GtkWidget *classification_public;
	GtkWidget *classification_private;
	GtkWidget *classification_confidential;

	GtkWidget *contacts_btn;	
	GtkWidget *contacts_box;

	GtkWidget *categories_btn;
	GtkWidget *categories;

	gboolean updating;

	/* The Corba component for selecting contacts, and the entry field
	   which we place in the dialog. */
	GNOME_Evolution_Addressbook_SelectNames corba_select_names;
	GtkWidget *contacts_entry;
};

static const int classification_map[] = {
	CAL_COMPONENT_CLASS_PUBLIC,
	CAL_COMPONENT_CLASS_PRIVATE,
	CAL_COMPONENT_CLASS_CONFIDENTIAL,
	-1
};



static void task_page_class_init (TaskPageClass *class);
static void task_page_init (TaskPage *tpage);
static void task_page_destroy (GtkObject *object);

static GtkWidget *task_page_get_widget (CompEditorPage *page);
static void task_page_focus_main_widget (CompEditorPage *page);
static void task_page_fill_widgets (CompEditorPage *page, CalComponent *comp);
static gboolean task_page_fill_component (CompEditorPage *page, CalComponent *comp);
static void task_page_set_summary (CompEditorPage *page, const char *summary);
static void task_page_set_dates (CompEditorPage *page, CompEditorPageDates *dates);

static CompEditorPageClass *parent_class = NULL;



/**
 * task_page_get_type:
 * 
 * Registers the #TaskPage class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the #TaskPage class.
 **/
GtkType
task_page_get_type (void)
{
	static GtkType task_page_type;

	if (!task_page_type) {
		static const GtkTypeInfo task_page_info = {
			"TaskPage",
			sizeof (TaskPage),
			sizeof (TaskPageClass),
			(GtkClassInitFunc) task_page_class_init,
			(GtkObjectInitFunc) task_page_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		task_page_type = gtk_type_unique (TYPE_COMP_EDITOR_PAGE,
						  &task_page_info);
	}

	return task_page_type;
}

/* Class initialization function for the task page */
static void
task_page_class_init (TaskPageClass *class)
{
	CompEditorPageClass *editor_page_class;
	GtkObjectClass *object_class;

	editor_page_class = (CompEditorPageClass *) class;
	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (TYPE_COMP_EDITOR_PAGE);

	editor_page_class->get_widget = task_page_get_widget;
	editor_page_class->focus_main_widget = task_page_focus_main_widget;
	editor_page_class->fill_widgets = task_page_fill_widgets;
	editor_page_class->fill_component = task_page_fill_component;
	editor_page_class->set_summary = task_page_set_summary;
	editor_page_class->set_dates = task_page_set_dates;

	object_class->destroy = task_page_destroy;
}

/* Object initialization function for the task page */
static void
task_page_init (TaskPage *tpage)
{
	TaskPagePrivate *priv;

	priv = g_new0 (TaskPagePrivate, 1);
	tpage->priv = priv;

	priv->xml = NULL;

	priv->main = NULL;
	priv->summary = NULL;
	priv->due_date = NULL;
	priv->start_date = NULL;
	priv->due_timezone = NULL;
	priv->start_timezone = NULL;
	priv->description = NULL;
	priv->classification_public = NULL;
	priv->classification_private = NULL;
	priv->classification_confidential = NULL;
	priv->contacts_btn = NULL;
	priv->contacts_box = NULL;
	priv->categories_btn = NULL;
	priv->categories = NULL;

	priv->updating = FALSE;

	priv->corba_select_names = CORBA_OBJECT_NIL;
	priv->contacts_entry = NULL;
}

/* Destroy handler for the task page */
static void
task_page_destroy (GtkObject *object)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_TASK_PAGE (object));

	tpage = TASK_PAGE (object);
	priv = tpage->priv;

	if (priv->corba_select_names != CORBA_OBJECT_NIL) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		bonobo_object_release_unref (priv->corba_select_names, &ev);
		CORBA_exception_free (&ev);
	}

	if (priv->xml) {
		gtk_object_unref (GTK_OBJECT (priv->xml));
		priv->xml = NULL;
	}

	g_free (priv);
	tpage->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* get_widget handler for the task page */
static GtkWidget *
task_page_get_widget (CompEditorPage *page)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;

	tpage = TASK_PAGE (page);
	priv = tpage->priv;

	return priv->main;
}

/* focus_main_widget handler for the task page */
static void
task_page_focus_main_widget (CompEditorPage *page)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;

	tpage = TASK_PAGE (page);
	priv = tpage->priv;

	gtk_widget_grab_focus (priv->summary);
}

/* Fills the widgets with default values */
static void
clear_widgets (TaskPage *tpage)
{
	TaskPagePrivate *priv;

	priv = tpage->priv;

	/* Summary, description */
	e_dialog_editable_set (priv->summary, NULL);
	e_dialog_editable_set (priv->description, NULL);

	/* Start, due times */
	e_date_edit_set_time (E_DATE_EDIT (priv->start_date), 0);
	e_date_edit_set_time (E_DATE_EDIT (priv->due_date), 0);

	/* Classification */
	e_dialog_radio_set (priv->classification_public,
			    CAL_COMPONENT_CLASS_PRIVATE, classification_map);

	/* Categories */
	e_dialog_editable_set (priv->categories, NULL);
}

/* Decode the radio button group for classifications */
static CalComponentClassification
classification_get (GtkWidget *widget)
{
	return e_dialog_radio_get (widget, classification_map);
}

static void
contacts_changed_cb (BonoboListener    *listener,
		     char              *event_name,
		     CORBA_any         *arg,
		     CORBA_Environment *ev,
		     gpointer           data)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;
	
	tpage = TASK_PAGE (data);
	priv = tpage->priv;
	
	if (!priv->updating)
		comp_editor_page_notify_changed (COMP_EDITOR_PAGE (tpage));
}

/* fill_widgets handler for the task page */
static void
task_page_fill_widgets (CompEditorPage *page, CalComponent *comp)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;
	CalComponentText text;
	CalComponentDateTime d;
	CalComponentClassification cl;
	CalClientGetStatus get_tz_status;
	GSList *l;
	const char *categories;
	icaltimezone *zone, *default_zone;
	char *location;

	tpage = TASK_PAGE (page);
	priv = tpage->priv;

	priv->updating = TRUE;
	
	/* Clean the screen */
	clear_widgets (tpage);

        /* Summary, description(s) */
	cal_component_get_summary (comp, &text);
	e_dialog_editable_set (priv->summary, text.value);

	cal_component_get_description_list (comp, &l);
	if (l) {
		text = *(CalComponentText *)l->data;
		e_dialog_editable_set (priv->description, text.value);
	} else {
		e_dialog_editable_set (priv->description, NULL);
	}
	cal_component_free_text_list (l);

	location = calendar_config_get_timezone ();
	default_zone = icaltimezone_get_builtin_timezone (location);

	/* Due Date. */
	cal_component_get_due (comp, &d);
	zone = NULL;
	if (d.value) {
		struct icaltimetype *due_tt = d.value;
		e_date_edit_set_date (E_DATE_EDIT (priv->due_date),
				      due_tt->year, due_tt->month,
				      due_tt->day);
		if (due_tt->is_date) {
			e_date_edit_set_time_of_day (E_DATE_EDIT (priv->due_date),
						     -1, -1);
			zone = default_zone;
		} else {
			e_date_edit_set_time_of_day (E_DATE_EDIT (priv->due_date),
						     due_tt->hour,
						     due_tt->minute);
		}
	} else {
		e_date_edit_set_time (E_DATE_EDIT (priv->due_date), -1);

		/* If no time is set, we use the default timezone, so the
		   user usually doesn't have to set this when they set the
		   date. */
		zone = default_zone;
	}

	/* Note that if we are creating a new task, the timezones may not be
	   on the server, so we try to get the builtin timezone with the TZID
	   first. */
	if (!zone)
		zone = icaltimezone_get_builtin_timezone_from_tzid (d.tzid);
	if (!zone) {
		get_tz_status = cal_client_get_timezone (page->client, d.tzid,
							 &zone);
		/* FIXME: Handle error better. */
		if (get_tz_status != CAL_CLIENT_GET_SUCCESS)
		  g_warning ("Couldn't get timezone from server: %s",
			     d.tzid ? d.tzid : "");
	}
	e_timezone_entry_set_timezone (E_TIMEZONE_ENTRY (priv->due_timezone),
				       zone);

	cal_component_free_datetime (&d);


	/* Start Date. */
	cal_component_get_dtstart (comp, &d);
	zone = NULL;
	if (d.value) {
		struct icaltimetype *start_tt = d.value;
		e_date_edit_set_date (E_DATE_EDIT (priv->start_date),
				      start_tt->year, start_tt->month,
				      start_tt->day);
		if (start_tt->is_date) {
			e_date_edit_set_time_of_day (E_DATE_EDIT (priv->start_date),
						     -1, -1);
			zone = default_zone;
		} else {
			e_date_edit_set_time_of_day (E_DATE_EDIT (priv->start_date),
						     start_tt->hour,
						     start_tt->minute);
		}
	} else {
		e_date_edit_set_time (E_DATE_EDIT (priv->start_date), -1);

		/* If no time is set, we use the default timezone, so the
		   user usually doesn't have to set this when they set the
		   date. */
		zone = default_zone;
	}

	if (!zone)
		zone = icaltimezone_get_builtin_timezone_from_tzid (d.tzid);
	if (!zone) {
		get_tz_status = cal_client_get_timezone (page->client, d.tzid,
							 &zone);
		/* FIXME: Handle error better. */
		if (get_tz_status != CAL_CLIENT_GET_SUCCESS)
			g_warning ("Couldn't get timezone from server: %s",
				   d.tzid ? d.tzid : "");
	}
	e_timezone_entry_set_timezone (E_TIMEZONE_ENTRY (priv->start_timezone),
				       zone);

	cal_component_free_datetime (&d);

	/* Classification. */
	cal_component_get_classification (comp, &cl);

	switch (cl) {
	case CAL_COMPONENT_CLASS_PUBLIC:
	    	e_dialog_radio_set (priv->classification_public,
				    CAL_COMPONENT_CLASS_PUBLIC,
				    classification_map);
		break;

	case CAL_COMPONENT_CLASS_PRIVATE:
	    	e_dialog_radio_set (priv->classification_public,
				    CAL_COMPONENT_CLASS_PRIVATE,
				    classification_map);
		break;

	case CAL_COMPONENT_CLASS_CONFIDENTIAL:
	    	e_dialog_radio_set (priv->classification_public,
				    CAL_COMPONENT_CLASS_CONFIDENTIAL,
				    classification_map);
		break;

	default:
		/* default to PUBLIC */
                e_dialog_radio_set (priv->classification_public,
                                    CAL_COMPONENT_CLASS_PUBLIC,
                                    classification_map);
                break;
	}

	/* Categories */
	cal_component_get_categories (comp, &categories);
	e_dialog_editable_set (priv->categories, categories);


	/* Contacts */
	comp_editor_contacts_to_widget (priv->contacts_entry, comp);

	/* We connect the contacts changed signal here, as we have to be a bit
	   more careful with it due to the use or Corba. The priv->updating
	   flag won't work as we won't get the changed event immediately.
	   FIXME: Unfortunately this doesn't work either. We never get the
	   changed event now. */
	comp_editor_connect_contacts_changed (priv->contacts_entry,
					      contacts_changed_cb, tpage);


	priv->updating = FALSE;
}

/* fill_component handler for the task page */
static gboolean
task_page_fill_component (CompEditorPage *page, CalComponent *comp)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;
	CalComponentDateTime date;
	struct icaltimetype icaltime;
	char *cat, *str;
	gboolean date_set, time_set;
	icaltimezone *zone;

	tpage = TASK_PAGE (page);
	priv = tpage->priv;

	/* Summary. */

	str = e_dialog_editable_get (priv->summary);
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

	icaltime = icaltime_null_time ();

	date.value = &icaltime;
	date.tzid = NULL;

	/* Due Date. */
	date_set = e_date_edit_get_date (E_DATE_EDIT (priv->due_date),
					 &icaltime.year,
					 &icaltime.month,
					 &icaltime.day);
	time_set = e_date_edit_get_time_of_day (E_DATE_EDIT (priv->due_date),
						&icaltime.hour,
						&icaltime.minute);
	if (date_set) {
		if (time_set) {
			zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->due_timezone));
			date.tzid = icaltimezone_get_tzid (zone);
		} else {
			icaltime.is_date = TRUE;
			date.tzid = NULL;
		}
		cal_component_set_due (comp, &date);
	} else {
		cal_component_set_due (comp, NULL);
	}

	/* Start Date. */
	icaltime = icaltime_null_time ();
	date_set = e_date_edit_get_date (E_DATE_EDIT (priv->start_date),
					 &icaltime.year,
					 &icaltime.month,
					 &icaltime.day);
	time_set = e_date_edit_get_time_of_day (E_DATE_EDIT (priv->start_date),
						&icaltime.hour,
						&icaltime.minute);
	if (date_set) {
		if (time_set) {
			zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->start_timezone));
			date.tzid = icaltimezone_get_tzid (zone);
		} else {
			icaltime.is_date = TRUE;
			date.tzid = NULL;
		}
		cal_component_set_dtstart (comp, &date);
	} else {
		cal_component_set_dtstart (comp, NULL);
	}

	/* Classification. */
	cal_component_set_classification (comp, classification_get (priv->classification_public));

	/* Categories */
	cat = e_dialog_editable_get (priv->categories);
	str = comp_editor_strip_categories (cat);
	if (cat)
		g_free (cat);

	cal_component_set_categories (comp, str);

	if (str)
		g_free (str);

	/* Contacts */
	comp_editor_contacts_to_component (priv->contacts_entry, comp);

	return TRUE;
}

/* set_summary handler for the task page */
static void
task_page_set_summary (CompEditorPage *page, const char *summary)
{
	/* nothing */
}

static void
task_page_set_dates (CompEditorPage *page, CompEditorPageDates *dates)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;

	tpage = TASK_PAGE (page);
	priv = tpage->priv;

	if (priv->updating)
	        return;

	priv->updating = TRUE;

	priv->updating = FALSE;
}



/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (TaskPage *tpage)
{
	CompEditorPage *page = COMP_EDITOR_PAGE (tpage);
	TaskPagePrivate *priv;
	GSList *accel_groups;
	GtkWidget *toplevel;

	priv = tpage->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	priv->main = GW ("task-page");
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

	priv->due_date = GW ("due-date");
	priv->start_date = GW ("start-date");
	priv->due_timezone = GW ("due-timezone");
	priv->start_timezone = GW ("start-timezone");

	priv->description = GW ("description");

	priv->classification_public = GW ("classification-public");
	priv->classification_private = GW ("classification-private");
	priv->classification_confidential = GW ("classification-confidential");

	priv->contacts_btn = GW ("contacts-button");
	priv->contacts_box = GW ("contacts-box");

	priv->categories_btn = GW ("categories-button");
	priv->categories = GW ("categories");

#undef GW

	return (priv->summary
		&& priv->due_date
		&& priv->start_date
		&& priv->due_timezone
		&& priv->start_timezone
		&& priv->classification_public
		&& priv->classification_private
		&& priv->classification_confidential
		&& priv->description
		&& priv->contacts_btn
		&& priv->contacts_box
		&& priv->categories_btn
		&& priv->categories);
}

/* Callback used when the summary changes; we emit the notification signal. */
static void
summary_changed_cb (GtkEditable *editable, gpointer data)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;
	gchar *summary;
	
	tpage = TASK_PAGE (data);
	priv = tpage->priv;
	
	if (priv->updating)
		return;
	
	summary = e_dialog_editable_get (GTK_WIDGET (editable));
	comp_editor_page_notify_summary_changed (COMP_EDITOR_PAGE (tpage), 
						 summary);
	g_free (summary);
}

/* Callback used when the start or due date widgets change.  We notify the
 * other pages in the task editor, so they can update any labels. 
 */
static void
date_changed_cb (EDateEdit *dedit, gpointer data)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;
	CompEditorPageDates dates;
	gboolean date_set, time_set;
	CalComponentDateTime start_dt, due_dt;
	struct icaltimetype start_tt = icaltime_null_time();
	struct icaltimetype due_tt = icaltime_null_time();

	tpage = TASK_PAGE (data);
	priv = tpage->priv;

	if (priv->updating)
		return;

	date_set = e_date_edit_get_date (E_DATE_EDIT (priv->start_date),
					 &start_tt.year,
					 &start_tt.month,
					 &start_tt.day);
	time_set = e_date_edit_get_time_of_day (E_DATE_EDIT (priv->start_date),
						&start_tt.hour,
						&start_tt.minute);
	if (date_set) {
		if (time_set) {
			icaltimezone *zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->start_timezone));
			start_dt.tzid = icaltimezone_get_tzid (zone);
		} else {
			start_tt.is_date = TRUE;
			start_dt.tzid = NULL;
		}
	} else {
		start_tt = icaltime_null_time ();
		start_dt.tzid = NULL;
	}

	date_set = e_date_edit_get_date (E_DATE_EDIT (priv->due_date),
					 &due_tt.year,
					 &due_tt.month,
					 &due_tt.day);
	time_set = e_date_edit_get_time_of_day (E_DATE_EDIT (priv->due_date),
						&due_tt.hour,
						&due_tt.minute);
	if (date_set) {
		if (time_set) {
			icaltimezone *zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->due_timezone));
			due_dt.tzid = icaltimezone_get_tzid (zone);
		} else {
			due_tt.is_date = TRUE;
			due_dt.tzid = NULL;
		}
	} else {
		due_tt = icaltime_null_time ();
		due_dt.tzid = NULL;
	}

	start_dt.value = &start_tt;
	dates.start = &start_dt;
	dates.end = NULL;
	due_dt.value = &due_tt;
	dates.due = &due_dt;
	dates.complete = NULL;
	
	/* Notify upstream */
	comp_editor_page_notify_dates_changed (COMP_EDITOR_PAGE (tpage),
					       &dates);
}

/* Callback used when the contacts button is clicked; we must bring up the
 * contact list dialog.
 */
static void
contacts_clicked_cb (GtkWidget *button, gpointer data)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;

	tpage = TASK_PAGE (data);
	priv = tpage->priv;

	comp_editor_show_contacts_dialog (priv->corba_select_names);

	/* FIXME: Currently we aren't getting the changed event from the
	   SelectNames component correctly, so we aren't saving the event
	   if just the contacts are changed. To work around that, we assume
	   that if the contacts button is clicked it is changed. */
	comp_editor_page_notify_changed (COMP_EDITOR_PAGE (tpage));
}

/* Callback used when the categories button is clicked; we must bring up the
 * category list dialog.
 */
static void
categories_clicked_cb (GtkWidget *button, gpointer data)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;
	GtkWidget *entry;

	tpage = TASK_PAGE (data);
	priv = tpage->priv;

	entry = priv->categories;
	e_categories_config_open_dialog_for_entry (GTK_ENTRY (entry));
}

/* This is called when any field is changed; it notifies upstream. */
static void
field_changed_cb (GtkWidget *widget, gpointer data)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;
	
	tpage = TASK_PAGE (data);
	priv = tpage->priv;
	
	if (!priv->updating)
		comp_editor_page_notify_changed (COMP_EDITOR_PAGE (tpage));
}

/* Hooks the widget signals */
static gboolean
init_widgets (TaskPage *tpage)
{
	TaskPagePrivate *priv;
	char *location;
	icaltimezone *zone;

	priv = tpage->priv;

	/* Make sure the EDateEdit widgets use our timezones to get the
	   current time. */
	e_date_edit_set_get_time_callback (E_DATE_EDIT (priv->start_date),
					   (EDateEditGetTimeCallback) comp_editor_get_current_time,
					   tpage, NULL);
	e_date_edit_set_get_time_callback (E_DATE_EDIT (priv->due_date),
					   (EDateEditGetTimeCallback) comp_editor_get_current_time,
					   tpage, NULL);
	
	/* Summary */
	gtk_signal_connect (GTK_OBJECT (priv->summary), "changed",
			    GTK_SIGNAL_FUNC (summary_changed_cb), tpage);

	/* Description - turn on word wrap. */
	gtk_text_set_word_wrap (GTK_TEXT (priv->description), TRUE);

	/* Dates */
	gtk_signal_connect (GTK_OBJECT (priv->start_date), "changed",
			    GTK_SIGNAL_FUNC (date_changed_cb), tpage);
	gtk_signal_connect (GTK_OBJECT (priv->due_date), "changed",
			    GTK_SIGNAL_FUNC (date_changed_cb), tpage);

	gtk_signal_connect (GTK_OBJECT (priv->due_timezone), "changed",
			    GTK_SIGNAL_FUNC (field_changed_cb), tpage);
	gtk_signal_connect (GTK_OBJECT (priv->start_timezone), "changed",
			    GTK_SIGNAL_FUNC (field_changed_cb), tpage);

	/* Classification */
	gtk_signal_connect (GTK_OBJECT (priv->classification_public),
			    "toggled",
			    GTK_SIGNAL_FUNC (field_changed_cb), tpage);
	gtk_signal_connect (GTK_OBJECT (priv->classification_private),
			    "toggled",
			    GTK_SIGNAL_FUNC (field_changed_cb), tpage);
	gtk_signal_connect (GTK_OBJECT (priv->classification_confidential),
			    "toggled",
			    GTK_SIGNAL_FUNC (field_changed_cb), tpage);

	/* Connect the default signal handler to use to make sure the "changed"
	   field gets set whenever a field is changed. */
	gtk_signal_connect (GTK_OBJECT (priv->description), "changed",
			    GTK_SIGNAL_FUNC (field_changed_cb), tpage);
	gtk_signal_connect (GTK_OBJECT (priv->categories), "changed",
			    GTK_SIGNAL_FUNC (field_changed_cb), tpage);

	/* Contacts button */
	gtk_signal_connect (GTK_OBJECT (priv->contacts_btn), "clicked",
			    GTK_SIGNAL_FUNC (contacts_clicked_cb), tpage);

	/* Categories button */
	gtk_signal_connect (GTK_OBJECT (priv->categories_btn), "clicked",
			    GTK_SIGNAL_FUNC (categories_clicked_cb), tpage);


	/* Create the contacts entry, a corba control from the address book. */
	priv->corba_select_names = comp_editor_create_contacts_component ();
	if (priv->corba_select_names == CORBA_OBJECT_NIL)
		return FALSE;

	priv->contacts_entry = comp_editor_create_contacts_control (priv->corba_select_names);
	if (priv->contacts_entry == NULL)
		return FALSE;

	gtk_container_add (GTK_CONTAINER (priv->contacts_box),
			   priv->contacts_entry);

	/* Set the default timezone, so the timezone entry may be hidden. */
	location = calendar_config_get_timezone ();
	zone = icaltimezone_get_builtin_timezone (location);
	e_timezone_entry_set_default_timezone (E_TIMEZONE_ENTRY (priv->start_timezone), zone);
	e_timezone_entry_set_default_timezone (E_TIMEZONE_ENTRY (priv->due_timezone), zone);

	return TRUE;
}



/**
 * task_page_construct:
 * @tpage: An task page.
 * 
 * Constructs an task page by loading its Glade data.
 * 
 * Return value: The same object as @tpage, or NULL if the widgets could not be
 * created.
 **/
TaskPage *
task_page_construct (TaskPage *tpage)
{
	TaskPagePrivate *priv;

	priv = tpage->priv;

	priv->xml = glade_xml_new (EVOLUTION_GLADEDIR "/task-page.glade",
				   NULL);
	if (!priv->xml) {
		g_message ("task_page_construct(): "
			   "Could not load the Glade XML file!");
		return NULL;
	}

	if (!get_widgets (tpage)) {
		g_message ("task_page_construct(): "
			   "Could not find all widgets in the XML file!");
		return NULL;
	}

	if (!init_widgets (tpage)) {
		g_message ("event_page_construct(): " 
			   "Could not initialize the widgets!");
		return NULL;
	}

	return tpage;
}

/**
 * task_page_new:
 * 
 * Creates a new task page.
 * 
 * Return value: A newly-created task page, or NULL if the page could
 * not be created.
 **/
TaskPage *
task_page_new (void)
{
	TaskPage *tpage;

	tpage = gtk_type_new (TYPE_TASK_PAGE);
	if (!task_page_construct (tpage)) {
		gtk_object_unref (GTK_OBJECT (tpage));
		return NULL;
	}

	return tpage;
}

GtkWidget *task_page_create_date_edit (void);

GtkWidget *
task_page_create_date_edit (void)
{
	GtkWidget *dedit;

	dedit = comp_editor_new_date_edit (TRUE, TRUE, TRUE);
	e_date_edit_set_allow_no_date_set (E_DATE_EDIT (dedit), TRUE);

	return dedit;
}
