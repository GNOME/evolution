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
#include <gtk/gtktextview.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtkoptionmenu.h>
#include <libgnome/gnome-i18n.h>
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

	GtkWidget *categories_btn;
	GtkWidget *categories;

	gboolean updating;
};

static const int classification_map[] = {
	E_CAL_COMPONENT_CLASS_PUBLIC,
	E_CAL_COMPONENT_CLASS_PRIVATE,
	E_CAL_COMPONENT_CLASS_CONFIDENTIAL,
	-1
};



static void task_page_class_init (TaskPageClass *class);
static void task_page_init (TaskPage *tpage);
static void task_page_finalize (GObject *object);

static GtkWidget *task_page_get_widget (CompEditorPage *page);
static void task_page_focus_main_widget (CompEditorPage *page);
static void task_page_fill_widgets (CompEditorPage *page, ECalComponent *comp);
static gboolean task_page_fill_component (CompEditorPage *page, ECalComponent *comp);
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

E_MAKE_TYPE (task_page, "TaskPage", TaskPage, task_page_class_init, task_page_init,
	     TYPE_COMP_EDITOR_PAGE);

/* Class initialization function for the task page */
static void
task_page_class_init (TaskPageClass *class)
{
	CompEditorPageClass *editor_page_class;
	GObjectClass *object_class;

	editor_page_class = (CompEditorPageClass *) class;
	object_class = (GObjectClass *) class;

	parent_class = g_type_class_ref(TYPE_COMP_EDITOR_PAGE);

	editor_page_class->get_widget = task_page_get_widget;
	editor_page_class->focus_main_widget = task_page_focus_main_widget;
	editor_page_class->fill_widgets = task_page_fill_widgets;
	editor_page_class->fill_component = task_page_fill_component;
	editor_page_class->set_summary = task_page_set_summary;
	editor_page_class->set_dates = task_page_set_dates;

	object_class->finalize = task_page_finalize;
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
	priv->categories_btn = NULL;
	priv->categories = NULL;

	priv->updating = FALSE;
}

/* Destroy handler for the task page */
static void
task_page_finalize (GObject *object)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_TASK_PAGE (object));

	tpage = TASK_PAGE (object);
	priv = tpage->priv;

	if (priv->main)
		gtk_widget_unref (priv->main);

	if (priv->xml) {
		g_object_unref (priv->xml);
		priv->xml = NULL;
	}

	g_free (priv);
	tpage->priv = NULL;

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
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
	gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->description)), "", 0);

	/* Start, due times */
	e_date_edit_set_time (E_DATE_EDIT (priv->start_date), 0);
	e_date_edit_set_time (E_DATE_EDIT (priv->due_date), 0);

	/* Classification */
	e_dialog_radio_set (priv->classification_public,
			    E_CAL_COMPONENT_CLASS_PRIVATE, classification_map);

	/* Categories */
	e_dialog_editable_set (priv->categories, NULL);
}

/* Decode the radio button group for classifications */
static ECalComponentClassification
classification_get (GtkWidget *widget)
{
	return e_dialog_radio_get (widget, classification_map);
}

/* fill_widgets handler for the task page */
static void
task_page_fill_widgets (CompEditorPage *page, ECalComponent *comp)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;
	ECalComponentText text;
	ECalComponentDateTime d;
	ECalComponentClassification cl;
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
	e_cal_component_get_summary (comp, &text);
	e_dialog_editable_set (priv->summary, text.value);

	e_cal_component_get_description_list (comp, &l);
	if (l) {
		text = *(ECalComponentText *)l->data;
		gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->description)),
					  text.value, -1);
	} else {
		gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->description)),
					  "", 0);
	}
	e_cal_component_free_text_list (l);

	location = calendar_config_get_timezone ();
	default_zone = icaltimezone_get_builtin_timezone (location);

	/* Due Date. */
	e_cal_component_get_due (comp, &d);
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
		if (!e_cal_get_timezone (page->client, d.tzid, &zone, NULL))
			/* FIXME: Handle error better. */
			g_warning ("Couldn't get timezone from server: %s",
				   d.tzid ? d.tzid : "");
	}
	e_timezone_entry_set_timezone (E_TIMEZONE_ENTRY (priv->due_timezone),
				       zone);

	e_cal_component_free_datetime (&d);


	/* Start Date. */
	e_cal_component_get_dtstart (comp, &d);
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
		if (!e_cal_get_timezone (page->client, d.tzid, &zone, NULL))
			/* FIXME: Handle error better. */
			g_warning ("Couldn't get timezone from server: %s",
				   d.tzid ? d.tzid : "");
	}
	e_timezone_entry_set_timezone (E_TIMEZONE_ENTRY (priv->start_timezone),
				       zone);

	e_cal_component_free_datetime (&d);

	/* Classification. */
	e_cal_component_get_classification (comp, &cl);

	switch (cl) {
	case E_CAL_COMPONENT_CLASS_PUBLIC:
	    	e_dialog_radio_set (priv->classification_public,
				    E_CAL_COMPONENT_CLASS_PUBLIC,
				    classification_map);
		break;

	case E_CAL_COMPONENT_CLASS_PRIVATE:
	    	e_dialog_radio_set (priv->classification_public,
				    E_CAL_COMPONENT_CLASS_PRIVATE,
				    classification_map);
		break;

	case E_CAL_COMPONENT_CLASS_CONFIDENTIAL:
	    	e_dialog_radio_set (priv->classification_public,
				    E_CAL_COMPONENT_CLASS_CONFIDENTIAL,
				    classification_map);
		break;

	default:
		/* default to PUBLIC */
                e_dialog_radio_set (priv->classification_public,
                                    E_CAL_COMPONENT_CLASS_PUBLIC,
                                    classification_map);
                break;
	}

	/* Categories */
	e_cal_component_get_categories (comp, &categories);
	e_dialog_editable_set (priv->categories, categories);


	priv->updating = FALSE;
}

/* fill_component handler for the task page */
static gboolean
task_page_fill_component (CompEditorPage *page, ECalComponent *comp)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;
	ECalComponentDateTime date;
	struct icaltimetype start_tt, due_tt;
	char *cat, *str;
	gboolean start_date_set, due_date_set, time_set;
	GtkTextBuffer *text_buffer;
	GtkTextIter text_iter_start, text_iter_end;
	struct icaltimetype *due_tt2;
	icaltimezone *start_zone = NULL;
	icaltimezone *due_zone = NULL;

	tpage = TASK_PAGE (page);
	priv = tpage->priv;
	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->description));

	/* Summary. */

	str = e_dialog_editable_get (priv->summary);
	if (!str || strlen (str) == 0)
		e_cal_component_set_summary (comp, NULL);
	else {
		ECalComponentText text;

		text.value = str;
		text.altrep = NULL;

		e_cal_component_set_summary (comp, &text);
	}

	if (str)
		g_free (str);

	/* Description */

	gtk_text_buffer_get_start_iter (text_buffer, &text_iter_start);
	gtk_text_buffer_get_end_iter   (text_buffer, &text_iter_end);
	str = gtk_text_buffer_get_text (text_buffer, &text_iter_start, &text_iter_end, FALSE);

	if (!str || strlen (str) == 0)
		e_cal_component_set_description_list (comp, NULL);
	else {
		GSList l;
		ECalComponentText text;

		text.value = str;
		text.altrep = NULL;
		l.data = &text;
		l.next = NULL;

		e_cal_component_set_description_list (comp, &l);
	}

	if (str)
		g_free (str);

	/* Dates */

	due_tt = icaltime_null_time ();

	date.value = &due_tt;
	date.tzid = NULL;

	/* Due Date. */
	if (!e_date_edit_date_is_valid (E_DATE_EDIT (priv->due_date)) ||
	    !e_date_edit_time_is_valid (E_DATE_EDIT (priv->due_date))) {
		comp_editor_page_display_validation_error (page, _("Due date is wrong"), priv->due_date);
		return FALSE;
	}

	due_date_set = e_date_edit_get_date (E_DATE_EDIT (priv->due_date),
					 &due_tt.year,
					 &due_tt.month,
					 &due_tt.day);
	time_set = e_date_edit_get_time_of_day (E_DATE_EDIT (priv->due_date),
						&due_tt.hour,
						&due_tt.minute);
	if (due_date_set) {
		if (time_set) {
			due_zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->due_timezone));
			date.tzid = icaltimezone_get_tzid (due_zone);
		} else {
			due_tt.is_date = TRUE;
			date.tzid = NULL;
		}
		e_cal_component_set_due (comp, &date);
	} else {
		e_cal_component_set_due (comp, NULL);
	}

	/* Start Date. */
	if (!e_date_edit_date_is_valid (E_DATE_EDIT (priv->start_date)) ||
	    !e_date_edit_time_is_valid (E_DATE_EDIT (priv->start_date))) {
		comp_editor_page_display_validation_error (page, _("Start date is wrong"), priv->start_date);
		return FALSE;
	}

	start_tt = icaltime_null_time ();
	date.value = &start_tt;
	start_date_set = e_date_edit_get_date (E_DATE_EDIT (priv->start_date),
					 &start_tt.year,
					 &start_tt.month,
					 &start_tt.day);
	time_set = e_date_edit_get_time_of_day (E_DATE_EDIT (priv->start_date),
						&start_tt.hour,
						&start_tt.minute);
	if (start_date_set) {
		if (time_set) {
			start_zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->start_timezone));
			date.tzid = icaltimezone_get_tzid (start_zone);
		} else {
			start_tt.is_date = TRUE;
			date.tzid = NULL;
		}
		e_cal_component_set_dtstart (comp, &date);
	} else {
		e_cal_component_set_dtstart (comp, NULL);
	}

	/* Check whether due datetime is before start datetime */
	if (start_date_set && due_date_set) {
		due_tt2 = g_new (struct icaltimetype, 1);
		memcpy (due_tt2,  &due_tt, sizeof(due_tt));
		icaltimezone_convert_time (due_tt2, due_zone, start_zone);
		if (icaltime_compare (start_tt, *due_tt2) > 0) {
			comp_editor_page_display_validation_error (page,
					_("Due date is before start date!"),
					priv->due_date);
			g_free (due_tt2);
			return FALSE;
		}
		g_free (due_tt2);
	}


	/* Classification. */
	e_cal_component_set_classification (comp, classification_get (priv->classification_public));

	/* Categories */
	cat = e_dialog_editable_get (priv->categories);
	str = comp_editor_strip_categories (cat);
	if (cat)
		g_free (cat);

	e_cal_component_set_categories (comp, str);

	if (str)
		g_free (str);

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
	accel_groups = gtk_accel_groups_from_object (G_OBJECT (toplevel));
	if (accel_groups) {
		page->accel_group = accel_groups->data;
		gtk_accel_group_ref (page->accel_group);
	}

	gtk_widget_ref (priv->main);
	gtk_container_remove (GTK_CONTAINER (priv->main->parent), priv->main);

	priv->summary = GW ("summary");

	/* Glade's visibility flag doesn't seem to work for custom widgets */
	priv->due_date = GW ("due-date");
	gtk_widget_show (priv->due_date);
	priv->start_date = GW ("start-date");
	gtk_widget_show (priv->start_date);

	priv->due_timezone = GW ("due-timezone");
	priv->start_timezone = GW ("start-timezone");

	priv->description = GW ("description");

	priv->classification_public = GW ("classification-public");
	priv->classification_private = GW ("classification-private");
	priv->classification_confidential = GW ("classification-confidential");

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
	ECalComponentDateTime start_dt, due_dt;
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
	GtkTextBuffer *text_buffer;
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
	g_signal_connect((priv->summary), "changed",
			    G_CALLBACK (summary_changed_cb), tpage);

	/* Description */
	text_buffer = gtk_text_buffer_new (NULL);
	gtk_text_view_set_buffer (GTK_TEXT_VIEW (priv->description), text_buffer);
	g_object_unref (text_buffer);

	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (priv->description), GTK_WRAP_WORD);

	/* Dates */
	g_signal_connect((priv->start_date), "changed",
			    G_CALLBACK (date_changed_cb), tpage);
	g_signal_connect((priv->due_date), "changed",
			    G_CALLBACK (date_changed_cb), tpage);

	g_signal_connect((priv->due_timezone), "changed",
			    G_CALLBACK (field_changed_cb), tpage);
	g_signal_connect((priv->start_timezone), "changed",
			    G_CALLBACK (field_changed_cb), tpage);

	/* Classification */
	g_signal_connect((priv->classification_public),
			    "toggled",
			    G_CALLBACK (field_changed_cb), tpage);
	g_signal_connect((priv->classification_private),
			    "toggled",
			    G_CALLBACK (field_changed_cb), tpage);
	g_signal_connect((priv->classification_confidential),
			    "toggled",
			    G_CALLBACK (field_changed_cb), tpage);

	/* Connect the default signal handler to use to make sure the "changed"
	   field gets set whenever a field is changed. */

	/* Belongs to priv->description */
	g_signal_connect ((text_buffer), "changed",
			  G_CALLBACK (field_changed_cb), tpage);

	g_signal_connect((priv->categories), "changed",
			    G_CALLBACK (field_changed_cb), tpage);

	/* Categories button */
	g_signal_connect((priv->categories_btn), "clicked",
			    G_CALLBACK (categories_clicked_cb), tpage);


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
				   NULL, NULL);
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
		g_object_unref (tpage);
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
