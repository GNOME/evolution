/* Evolution calendar - Main page of the event editor dialog
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

#include <gtk/gtksignal.h>
#include <gtk/gtktogglebutton.h>
#include <glade/glade.h>
#include <gal/widgets/e-unicode.h>
#include <gal/widgets/e-categories.h>
#include "e-util/e-categories-config.h"
#include "e-util/e-dialog-widgets.h"
#include "widgets/misc/e-dateedit.h"
#include "cal-util/timeutil.h"
#include "../calendar-config.h"
#include "../e-timezone-entry.h"
#include "comp-editor-util.h"
#include "event-page.h"



/* Private part of the EventPage structure */
struct _EventPagePrivate {
	/* Glade XML data */
	GladeXML *xml;

	/* Widgets from the Glade file */

	GtkWidget *main;

	GtkWidget *summary;

	GtkWidget *start_time;
	GtkWidget *end_time;
	GtkWidget *start_timezone;
	GtkWidget *end_timezone;
	GtkWidget *all_day_event;

	GtkWidget *description;

	GtkWidget *classification_public;
	GtkWidget *classification_private;
	GtkWidget *classification_confidential;

	GtkWidget *contacts_btn;
	GtkWidget *contacts;

	GtkWidget *categories_btn;
	GtkWidget *categories;

	gboolean updating;

	/* This is TRUE if both the start & end timezone are the same. If the
	   start timezone is then changed, we updated the end timezone to the
	   same value, since 99% of events start and end in one timezone. */
	gboolean sync_timezones;
};



static void event_page_class_init (EventPageClass *class);
static void event_page_init (EventPage *epage);
static void event_page_destroy (GtkObject *object);

static GtkWidget *event_page_get_widget (CompEditorPage *page);
static void event_page_focus_main_widget (CompEditorPage *page);
static void event_page_fill_widgets (CompEditorPage *page, CalComponent *comp);
static void event_page_fill_component (CompEditorPage *page, CalComponent *comp);
static void event_page_set_summary (CompEditorPage *page, const char *summary);
static void event_page_set_dates (CompEditorPage *page, CompEditorPageDates *dates);

static CompEditorPageClass *parent_class = NULL;



/**
 * event_page_get_type:
 * 
 * Registers the #EventPage class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the #EventPage class.
 **/
GtkType
event_page_get_type (void)
{
	static GtkType event_page_type;

	if (!event_page_type) {
		static const GtkTypeInfo event_page_info = {
			"EventPage",
			sizeof (EventPage),
			sizeof (EventPageClass),
			(GtkClassInitFunc) event_page_class_init,
			(GtkObjectInitFunc) event_page_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		event_page_type = gtk_type_unique (TYPE_COMP_EDITOR_PAGE, 
						   &event_page_info);
	}

	return event_page_type;
}

/* Class initialization function for the event page */
static void
event_page_class_init (EventPageClass *class)
{
	CompEditorPageClass *editor_page_class;
	GtkObjectClass *object_class;

	editor_page_class = (CompEditorPageClass *) class;
	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (TYPE_COMP_EDITOR_PAGE);

	editor_page_class->get_widget = event_page_get_widget;
	editor_page_class->focus_main_widget = event_page_focus_main_widget;
	editor_page_class->fill_widgets = event_page_fill_widgets;
	editor_page_class->fill_component = event_page_fill_component;
	editor_page_class->set_summary = event_page_set_summary;
	editor_page_class->set_dates = event_page_set_dates;

	object_class->destroy = event_page_destroy;
}

/* Object initialization function for the event page */
static void
event_page_init (EventPage *epage)
{
	EventPagePrivate *priv;

	priv = g_new0 (EventPagePrivate, 1);
	epage->priv = priv;

	priv->xml = NULL;

	priv->main = NULL;
	priv->summary = NULL;
	priv->start_time = NULL;
	priv->end_time = NULL;
	priv->start_timezone = NULL;
	priv->end_timezone = NULL;
	priv->all_day_event = NULL;
	priv->description = NULL;
	priv->classification_public = NULL;
	priv->classification_private = NULL;
	priv->classification_confidential = NULL;
	priv->contacts_btn = NULL;
	priv->contacts = NULL;
	priv->categories_btn = NULL;
	priv->categories = NULL;

	priv->updating = FALSE;
	priv->sync_timezones = FALSE;
}

/* Destroy handler for the event page */
static void
event_page_destroy (GtkObject *object)
{
	EventPage *epage;
	EventPagePrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_EVENT_PAGE (object));

	epage = EVENT_PAGE (object);
	priv = epage->priv;

	if (priv->xml) {
		gtk_object_unref (GTK_OBJECT (priv->xml));
		priv->xml = NULL;
	}

	g_free (priv);
	epage->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



static const int classification_map[] = {
	CAL_COMPONENT_CLASS_PUBLIC,
	CAL_COMPONENT_CLASS_PRIVATE,
	CAL_COMPONENT_CLASS_CONFIDENTIAL,
	-1
};

/* get_widget handler for the event page */
static GtkWidget *
event_page_get_widget (CompEditorPage *page)
{
	EventPage *epage;
	EventPagePrivate *priv;

	epage = EVENT_PAGE (page);
	priv = epage->priv;

	return priv->main;
}

/* focus_main_widget handler for the event page */
static void
event_page_focus_main_widget (CompEditorPage *page)
{
	EventPage *epage;
	EventPagePrivate *priv;

	epage = EVENT_PAGE (page);
	priv = epage->priv;

	gtk_widget_grab_focus (priv->summary);
}

/* Checks if the event's time starts and ends at midnight, and sets the 
 *"all day event" box accordingly.
 */
static void
check_all_day (EventPage *epage)
{
	EventPagePrivate *priv;
	gboolean all_day = FALSE, start_set, end_set;
	gint start_hour, start_minute, end_hour, end_minute;

	priv = epage->priv;

	start_set = e_date_edit_get_time_of_day (E_DATE_EDIT (priv->start_time),
						 &start_hour, &start_minute);

	end_set = e_date_edit_get_time_of_day (E_DATE_EDIT (priv->end_time),
					       &end_hour, &end_minute);

	/* all day event checkbox */
	if ((!start_set || (start_hour == 0 && start_minute == 0))
	    && (!end_set || (end_hour == 0 && end_minute == 0)))
		all_day = TRUE;

	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->all_day_event),
					  epage);
	e_dialog_toggle_set (priv->all_day_event, all_day);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->all_day_event),
					    epage);

	e_date_edit_set_show_time (E_DATE_EDIT (priv->start_time), !all_day);
	e_date_edit_set_show_time (E_DATE_EDIT (priv->end_time), !all_day);
}

/* Fills the widgets with default values */
static void
clear_widgets (EventPage *epage)
{
	EventPagePrivate *priv;

	priv = epage->priv;

	/* Summary, description */
	e_dialog_editable_set (priv->summary, NULL);
	e_dialog_editable_set (priv->description, NULL);

	/* Start and end times */
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->start_time),
					  epage);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->end_time), epage);

	e_date_edit_set_time (E_DATE_EDIT (priv->start_time), 0);
	e_date_edit_set_time (E_DATE_EDIT (priv->end_time), 0);

	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->start_time),
					    epage);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->end_time),
					    epage);

	check_all_day (epage);

	/* Classification */
	e_dialog_radio_set (priv->classification_public,
			    CAL_COMPONENT_CLASS_PRIVATE, classification_map);

	/* Categories */
	e_dialog_editable_set (priv->categories, NULL);
}

/* fill_widgets handler for the event page */
static void
event_page_fill_widgets (CompEditorPage *page, CalComponent *comp)
{
	EventPage *epage;
	EventPagePrivate *priv;
	CalComponentText text;
	CalComponentClassification cl;
	CalComponentDateTime start_date, end_date;
	GSList *l;
	const char *categories;
	CalClientGetStatus status;
	struct icaltimetype *start_tt, *end_tt;
	icaltimezone *start_zone = NULL, *end_zone = NULL;

	g_return_if_fail (page->client != NULL);

	epage = EVENT_PAGE (page);
	priv = epage->priv;

	/* Don't send off changes during this time */
	priv->updating = TRUE;

	/* Clean the page */
	clear_widgets (epage);

	/* Summary, description(s) */

	cal_component_get_summary (comp, &text);
	e_dialog_editable_set (priv->summary, text.value);

	cal_component_get_description_list (comp, &l);
	if (l) {
		text = *(CalComponentText *)l->data;
		e_dialog_editable_set (priv->description, text.value);
	}
	cal_component_free_text_list (l);

	/* Start and end times */

	cal_component_get_dtstart (comp, &start_date);
	status = cal_client_get_timezone (page->client, start_date.tzid,
					  &start_zone);
	/* FIXME: Handle error better. */
	if (status != CAL_CLIENT_GET_SUCCESS)
		g_warning ("Couldn't get timezone from server: %s",
			   start_date.tzid ? start_date.tzid : "");

	cal_component_get_dtend (comp, &end_date);
	status = cal_client_get_timezone (page->client, end_date.tzid, &end_zone);
	/* FIXME: Handle error better. */
	if (status != CAL_CLIENT_GET_SUCCESS)
	  g_warning ("Couldn't get timezone from server: %s",
		     end_date.tzid ? end_date.tzid : "");

	/* All-day events are inclusive, i.e. if the end date shown is 2nd Feb
	   then the event includes all of the 2nd Feb. We would normally show
	   3rd Feb as the end date, since it really ends at midnight on 3rd,
	   so we have to subtract a day so we only show the 2nd. */
	start_tt = start_date.value;
	end_tt = end_date.value;
	if (start_tt->hour == 0 && start_tt->minute == 0 && start_tt->second == 0
	    && end_tt->hour == 0 && end_tt->minute == 0 && end_tt->second == 0)
		icaltime_adjust (end_tt, 1, 0, 0, 0);

	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->start_time),
					  epage);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->end_time), epage);

	e_date_edit_set_date (E_DATE_EDIT (priv->start_time), start_tt->year,
			      start_tt->month, start_tt->day);
	e_date_edit_set_time_of_day (E_DATE_EDIT (priv->start_time),
				     start_tt->hour, start_tt->minute);

	e_date_edit_set_date (E_DATE_EDIT (priv->end_time), end_tt->year,
			      end_tt->month, end_tt->day);
	e_date_edit_set_time_of_day (E_DATE_EDIT (priv->end_time),
				     end_tt->hour, end_tt->minute);

	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->start_time),
					    epage);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->end_time),
					    epage);

	cal_component_free_datetime (&start_date);
	cal_component_free_datetime (&end_date);

	check_all_day (epage);

	/* Set the timezones, and set sync_timezones to TRUE if both timezones
	   are the same. */
	e_timezone_entry_set_timezone (E_TIMEZONE_ENTRY (priv->start_timezone),
				       start_zone);
	e_timezone_entry_set_timezone (E_TIMEZONE_ENTRY (priv->end_timezone),
				       end_zone);
	priv->sync_timezones = (start_zone == end_zone) ? TRUE : FALSE;


	/* Classification */

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
		/* What do do?  We can't g_assert_not_reached() since it is a
		 * value from an external file.
		 */
	}

	/* Categories */
	cal_component_get_categories (comp, &categories);
	e_dialog_editable_set (priv->categories, categories);

	priv->updating = FALSE;
}

/* fill_component handler for the event page */
static void
event_page_fill_component (CompEditorPage *page, CalComponent *comp)
{
	EventPage *epage;
	EventPagePrivate *priv;
	CalComponentDateTime date;
	struct icaltimetype icaltime;
	gboolean all_day_event, date_set;
	char *cat, *str;
	CalComponentClassification classif;
	icaltimezone *zone;

	epage = EVENT_PAGE (page);
	priv = epage->priv;

	/* Summary */

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

	if (str)
		g_free (str);

	/* Dates */

	date.value = &icaltime;
	date.tzid = NULL;

	icaltime.is_utc = 0;
	/* FIXME: We should use is_date at some point. */
	icaltime.is_date = 0;
	icaltime.is_daylight = 0;
	icaltime.second = 0;

	date_set = e_date_edit_get_date (E_DATE_EDIT (priv->start_time),
					 &icaltime.year,
					 &icaltime.month,
					 &icaltime.day);
	e_date_edit_get_time_of_day (E_DATE_EDIT (priv->start_time),
				     &icaltime.hour,
				     &icaltime.minute);
	g_assert (date_set);
	zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->start_timezone));
	if (zone)
		date.tzid = icaltimezone_get_tzid (zone);
	cal_component_set_dtstart (comp, &date);

	/* If the all_day toggle is set, the end date is inclusive of the
	   entire day on which it points to. */
	all_day_event = e_dialog_toggle_get (priv->all_day_event);

	date_set = e_date_edit_get_date (E_DATE_EDIT (priv->end_time),
					 &icaltime.year,
					 &icaltime.month,
					 &icaltime.day);
	e_date_edit_get_time_of_day (E_DATE_EDIT (priv->end_time),
				     &icaltime.hour,
				     &icaltime.minute);
	g_assert (date_set);

	if (all_day_event) {
		icaltime.hour = 0;
		icaltime.minute = 0;
		icaltime.second = 0;
		icaltime_adjust (&icaltime, 1, 0, 0, 0);
	}

	zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->end_timezone));
	if (zone)
	  date.tzid = icaltimezone_get_tzid (zone);
	cal_component_set_dtend (comp, &date);


	/* Categories */

	cat = e_dialog_editable_get (priv->categories);
	cal_component_set_categories (comp, cat);

	if (cat)
		g_free (cat);

	/* Classification */

	classif = e_dialog_radio_get (priv->classification_public,
				      classification_map);
	cal_component_set_classification (comp, classif);
}

/* set_summary handler for the event page */
static void
event_page_set_summary (CompEditorPage *page, const char *summary)
{
	EventPage *epage;
	EventPagePrivate *priv;

	epage = EVENT_PAGE (page);
	priv = epage->priv;

	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->summary), epage);
	e_utf8_gtk_entry_set_text (GTK_ENTRY (priv->summary), summary);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->summary), epage);
}

static void
event_page_set_dates (CompEditorPage *page, CompEditorPageDates *dates)
{
	/* nothing */
}



/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (EventPage *epage)
{
	EventPagePrivate *priv;

	priv = epage->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	priv->main = GW ("event-page");
	if (!priv->main)
		return FALSE;

	gtk_widget_ref (priv->main);
	gtk_widget_unparent (priv->main);

	priv->summary = GW ("general-summary");

	priv->start_time = GW ("start-time");
	priv->end_time = GW ("end-time");
	priv->start_timezone = GW ("start-timezone");
	priv->end_timezone = GW ("end-timezone");
	priv->all_day_event = GW ("all-day-event");

	priv->description = GW ("description");

	priv->classification_public = GW ("classification-public");
	priv->classification_private = GW ("classification-private");
	priv->classification_confidential = GW ("classification-confidential");

	priv->contacts_btn = GW ("contacts-button");
	priv->contacts = GW ("contacts");

	priv->categories_btn = GW ("categories-button");
	priv->categories = GW ("categories");

#undef GW

	return (priv->summary
		&& priv->start_time
		&& priv->end_time
		&& priv->start_timezone
		&& priv->end_timezone
		&& priv->all_day_event
		&& priv->description
		&& priv->classification_public
		&& priv->classification_private
		&& priv->classification_confidential
		&& priv->contacts_btn
		&& priv->contacts
		&& priv->categories_btn
		&& priv->categories);
}

/* Callback used when the summary changes; we emit the notification signal. */
static void
summary_changed_cb (GtkEditable *editable, gpointer data)
{
	EventPage *epage;
	EventPagePrivate *priv;
	gchar *summary;
	
	epage = EVENT_PAGE (data);
	priv = epage->priv;
	
	if (priv->updating)
		return;
	
	summary = gtk_editable_get_chars (editable, 0, -1);
	comp_editor_page_notify_summary_changed (COMP_EDITOR_PAGE (epage),
						 summary);
	g_free (summary);
}

/* Callback used when the start or end date widgets change.  We check that the
 * start date < end date and we set the "all day event" button as appropriate.
 */
static void
date_changed_cb (EDateEdit *dedit, gpointer data)
{
	EventPage *epage;
	EventPagePrivate *priv;
	CompEditorPageDates dates;
	struct icaltimetype start_tt = icaltime_null_time();
	struct icaltimetype end_tt = icaltime_null_time();
	int cmp;
	gboolean date_set;
	
	epage = EVENT_PAGE (data);
	priv = epage->priv;

	if (priv->updating)
		return;

	/* Ensure that start < end */
	date_set = e_date_edit_get_date (E_DATE_EDIT (priv->start_time),
					 &start_tt.year,
					 &start_tt.month,
					 &start_tt.day);
	e_date_edit_get_time_of_day (E_DATE_EDIT (priv->start_time),
				     &start_tt.hour,
				     &start_tt.minute);
	g_assert (date_set);

	date_set = e_date_edit_get_date (E_DATE_EDIT (priv->end_time),
					 &end_tt.year,
					 &end_tt.month,
					 &end_tt.day);
	e_date_edit_get_time_of_day (E_DATE_EDIT (priv->end_time),
				     &end_tt.hour,
				     &end_tt.minute);
	g_assert (date_set);

	/* FIXME: TIMEZONES. */
	cmp = icaltime_compare (start_tt, end_tt);
	if (cmp >= 0) {
		if (cmp == 0 && start_tt.hour == 0
		    && start_tt.minute == 0
		    && start_tt.second == 0) {
			/* If the start and end times are the same, but both
			 * are on day boundaries, then that is OK since it 
			 * means we have an all-day event lasting 1 day.  So
			 * we do nothing here.
			 */
		} else if (GTK_WIDGET (dedit) == priv->start_time) {
			/* Modify the end time, to be the start + 1 hour. */

			/* FIXME: TIMEZONES - Probably want to leave the
			   timezone as it is, so we need to convert the time.*/

			end_tt = start_tt;
			icaltime_adjust (&end_tt, 0, 1, 0, 0);

			gtk_signal_handler_block_by_data (GTK_OBJECT (priv->end_time), epage);

			e_date_edit_set_date (E_DATE_EDIT (priv->end_time),
					      end_tt.year,
					      end_tt.month,
					      end_tt.day);
			e_date_edit_set_time_of_day (E_DATE_EDIT (priv->end_time),
						     end_tt.hour,
						     end_tt.minute);

			gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->end_time), epage);
		} else if (GTK_WIDGET (dedit) == priv->end_time) {
			/* Modify the start time, to be the end - 1 hour. */

			/* FIXME: TIMEZONES - Probably want to leave the
			   timezone as it is, so we need to convert the time.*/

			start_tt = end_tt;
			icaltime_adjust (&start_tt, 0, -1, 0, 0);

			gtk_signal_handler_block_by_data (GTK_OBJECT (priv->start_time), epage);

			e_date_edit_set_date (E_DATE_EDIT (priv->start_time),
					      start_tt.year,
					      start_tt.month,
					      start_tt.day);
			e_date_edit_set_time_of_day (E_DATE_EDIT (priv->start_time),
						     start_tt.hour,
						     start_tt.minute);

			gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->start_time), epage);
		} else
			g_assert_not_reached ();
	}

	/* Set the "all day event" button as appropriate */
	check_all_day (epage);

	/* Notify upstream */
	dates.start = &start_tt;
	dates.end = &end_tt;
	dates.due = NULL;
	dates.complete = NULL;
	comp_editor_page_notify_dates_changed (COMP_EDITOR_PAGE (epage),
					       &dates);
}

/* Callback used when the start timezone is changed. If sync_timezones is set,
 * we set the end timezone to the same value. It also updates the start time
 * labels on the other notebook pages.
 */
static void
start_timezone_changed_cb (GtkWidget *widget, gpointer data)
{
	EventPage *epage;
	EventPagePrivate *priv;
	icaltimezone *zone;

	epage = EVENT_PAGE (data);
	priv = epage->priv;

	if (priv->sync_timezones) {
		zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->start_timezone));
		e_timezone_entry_set_timezone (E_TIMEZONE_ENTRY (priv->end_timezone), zone);
	}
}


/* Callback used when the end timezone is changed. It checks if the end
 * timezone is the same as the start timezone and sets sync_timezones if so.
 */
static void
end_timezone_changed_cb (GtkWidget *widget, gpointer data)
{
	EventPage *epage;
	EventPagePrivate *priv;
	icaltimezone *start_zone, *end_zone;
	
	epage = EVENT_PAGE (data);
	priv = epage->priv;

	start_zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->start_timezone));
	end_zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->end_timezone));

	priv->sync_timezones = (start_zone == end_zone) ? TRUE : FALSE;
}

/* Callback: all day event button toggled.
 * Note that this should only be called when the user explicitly toggles the
 * button. Be sure to block this handler when the toggle button's state is set
 * within the code.
 */
static void
all_day_event_toggled_cb (GtkWidget *toggle, gpointer data)
{
	EventPage *epage;
	EventPagePrivate *priv;
	gboolean all_day;
	CompEditorPageDates dates;
	struct icaltimetype start_tt = icaltime_null_time();
	struct icaltimetype end_tt = icaltime_null_time();
	gboolean date_set;

	epage = EVENT_PAGE (data);
	priv = epage->priv;

	/* When the all_day toggle is turned on, the start date is
	 * rounded down to the start of the day, and end date is
	 * rounded down to the start of the day on which the event
	 * ends. The event is then taken to be inclusive of the days
	 * between the start and end days.  Note that if the event end
	 * is at midnight, we do not round it down to the previous
	 * day, since if we do that and the user repeatedly turns the
	 * all_day toggle on and off, the event keeps shrinking.
	 * (We'd also need to make sure we didn't adjust the time when
	 * the radio button is initially set.)
	 *
	 * When the all_day_toggle is turned off, we set the event start to the
	 * start of the working day, and if the event end is on or before the
	 * day of the event start we set it to one hour after the event start.
	 */
	all_day = GTK_TOGGLE_BUTTON (toggle)->active;

	/*
	 * Start time.
	 */
	date_set = e_date_edit_get_date (E_DATE_EDIT (priv->start_time),
					 &start_tt.year,
					 &start_tt.month,
					 &start_tt.day);
	e_date_edit_get_time_of_day (E_DATE_EDIT (priv->start_time),
				     &start_tt.hour,
				     &start_tt.minute);
	g_assert (date_set);

	if (all_day) {
		/* Round down to the start of the day. */
		start_tt.hour = 0;
		start_tt.minute  = 0;
		start_tt.second  = 0;
	} else {
		/* Set to the start of the working day. */
		start_tt.hour = calendar_config_get_day_start_hour ();
		start_tt.minute  = calendar_config_get_day_start_minute ();
		start_tt.second  = 0;
	}

	/*
	 * End time.
	 */
	date_set = e_date_edit_get_date (E_DATE_EDIT (priv->end_time),
					 &end_tt.year,
					 &end_tt.month,
					 &end_tt.day);
	e_date_edit_get_time_of_day (E_DATE_EDIT (priv->end_time),
				     &end_tt.hour,
				     &end_tt.minute);
	g_assert (date_set);

	if (all_day) {
		/* Round down to the start of the day. */
		end_tt.hour = 0;
		end_tt.minute  = 0;
		end_tt.second  = 0;
	} else {
		/* If the event end is now on or before the event start day,
		 * make it end one hour after the start. */
		if (end_tt.year < start_tt.year
		    || (end_tt.year == start_tt.year
			&& end_tt.month < start_tt.month)
		    || (end_tt.year == start_tt.year
			&& end_tt.month == start_tt.month
			&& end_tt.day <= start_tt.day)) {
			end_tt.year = start_tt.year;
			end_tt.month = start_tt.month;
			end_tt.day = start_tt.day;
			end_tt.hour = start_tt.hour;
			icaltime_adjust (&end_tt, 0, 1, 0, 0);
		}
	}

	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->start_time),
					  epage);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->end_time),
					  epage);

	e_date_edit_set_date (E_DATE_EDIT (priv->start_time), start_tt.year,
			      start_tt.month, start_tt.day);
	e_date_edit_set_time_of_day (E_DATE_EDIT (priv->start_time),
				     start_tt.hour, start_tt.minute);

	e_date_edit_set_date (E_DATE_EDIT (priv->end_time), end_tt.year,
			      end_tt.month, end_tt.day);
	e_date_edit_set_time_of_day (E_DATE_EDIT (priv->end_time),
				     end_tt.hour, end_tt.minute);

	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->start_time),
					    epage);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->end_time),
					    epage);

	e_date_edit_set_show_time (E_DATE_EDIT (priv->start_time), !all_day);
	e_date_edit_set_show_time (E_DATE_EDIT (priv->end_time), !all_day);

	/* Notify upstream */
	dates.start = &start_tt;
	dates.end = &end_tt;
	dates.due = NULL;
	dates.complete = NULL;
	comp_editor_page_notify_dates_changed (COMP_EDITOR_PAGE (epage),
					       &dates);
}

/* Callback used when the categories button is clicked; we must bring up the
 * category list dialog.
 */
static void
categories_clicked_cb (GtkWidget *button, gpointer data)
{
	EventPage *epage;
	EventPagePrivate *priv;
	GtkWidget *entry;

	epage = EVENT_PAGE (data);
	priv = epage->priv;

	entry = priv->categories;
	e_categories_config_open_dialog_for_entry (GTK_ENTRY (entry));
}

/* This is called when any field is changed; it notifies upstream. */
static void
field_changed_cb (GtkWidget *widget, gpointer data)
{
	EventPage *epage;
	EventPagePrivate *priv;
	
	epage = EVENT_PAGE (data);
	priv = epage->priv;
	
	if (!priv->updating)
		comp_editor_page_notify_changed (COMP_EDITOR_PAGE (epage));
}

/* Hooks the widget signals */
static void
init_widgets (EventPage *epage)
{
	EventPagePrivate *priv;

	priv = epage->priv;

	/* Make sure the EDateEdit widgets use our timezones to get the
	   current time. */
	e_date_edit_set_get_time_callback (E_DATE_EDIT (priv->start_time),
					   (EDateEditGetTimeCallback) comp_editor_get_current_time,
					   epage, NULL);
	e_date_edit_set_get_time_callback (E_DATE_EDIT (priv->end_time),
					   (EDateEditGetTimeCallback) comp_editor_get_current_time,
					   epage, NULL);

	/* Summary */
	gtk_signal_connect (GTK_OBJECT (priv->summary), "changed",
			    GTK_SIGNAL_FUNC (summary_changed_cb), epage);

	/* Start and end times */
	gtk_signal_connect (GTK_OBJECT (priv->start_time), "changed",
			    GTK_SIGNAL_FUNC (date_changed_cb), epage);
	gtk_signal_connect (GTK_OBJECT (priv->end_time), "changed",
			    GTK_SIGNAL_FUNC (date_changed_cb), epage);

	gtk_signal_connect (GTK_OBJECT (priv->start_timezone), "changed",
			    GTK_SIGNAL_FUNC (start_timezone_changed_cb), epage);
	gtk_signal_connect (GTK_OBJECT (priv->end_timezone), "changed",
			    GTK_SIGNAL_FUNC (end_timezone_changed_cb), epage);

	gtk_signal_connect (GTK_OBJECT (priv->all_day_event), "toggled",
			    GTK_SIGNAL_FUNC (all_day_event_toggled_cb), epage);

	/* Categories button */
	gtk_signal_connect (GTK_OBJECT (priv->categories_btn), "clicked",
			    GTK_SIGNAL_FUNC (categories_clicked_cb), epage);

	/* Connect the default signal handler to use to make sure we notify
	 * upstream of changes to the widget values.
	 */

	gtk_signal_connect (GTK_OBJECT (priv->summary), "changed",
			    GTK_SIGNAL_FUNC (field_changed_cb), epage);
	gtk_signal_connect (GTK_OBJECT (priv->start_time), "changed",
			    GTK_SIGNAL_FUNC (field_changed_cb), epage);
	gtk_signal_connect (GTK_OBJECT (priv->end_time), "changed",
			    GTK_SIGNAL_FUNC (field_changed_cb), epage);
	gtk_signal_connect (GTK_OBJECT (priv->start_timezone), "changed",
			    GTK_SIGNAL_FUNC (field_changed_cb), epage);
	gtk_signal_connect (GTK_OBJECT (priv->end_timezone), "changed",
			    GTK_SIGNAL_FUNC (field_changed_cb), epage);
	gtk_signal_connect (GTK_OBJECT (priv->all_day_event), "toggled",
			    GTK_SIGNAL_FUNC (field_changed_cb), epage);
	gtk_signal_connect (GTK_OBJECT (priv->description), "changed",
			    GTK_SIGNAL_FUNC (field_changed_cb), epage);
	gtk_signal_connect (GTK_OBJECT (priv->classification_public),
			    "toggled", GTK_SIGNAL_FUNC (field_changed_cb),
			    epage);
	gtk_signal_connect (GTK_OBJECT (priv->classification_private),
			    "toggled", GTK_SIGNAL_FUNC (field_changed_cb),
			    epage);
	gtk_signal_connect (GTK_OBJECT (priv->classification_confidential),
			    "toggled", GTK_SIGNAL_FUNC (field_changed_cb),
			    epage);
	gtk_signal_connect (GTK_OBJECT (priv->categories), "changed",
			    GTK_SIGNAL_FUNC (field_changed_cb), epage);

	/* FIXME: we do not support these fields yet, so we disable them */

	gtk_widget_set_sensitive (priv->contacts_btn, FALSE);
	gtk_widget_set_sensitive (priv->contacts, FALSE);
}



/**
 * event_page_construct:
 * @epage: An event page.
 * 
 * Constructs an event page by loading its Glade data.
 * 
 * Return value: The same object as @epage, or NULL if the widgets could not be
 * created.
 **/
EventPage *
event_page_construct (EventPage *epage)
{
	EventPagePrivate *priv;

	priv = epage->priv;

	priv->xml = glade_xml_new (EVOLUTION_GLADEDIR "/event-page.glade", 
				   NULL);
	if (!priv->xml) {
		g_message ("event_page_construct(): " 
			   "Could not load the Glade XML file!");
		return NULL;
	}

	if (!get_widgets (epage)) {
		g_message ("event_page_construct(): " 
			   "Could not find all widgets in the XML file!");
		return NULL;
	}

	init_widgets (epage);

	return epage;
}

/**
 * event_page_new:
 * 
 * Creates a new event page.
 * 
 * Return value: A newly-created event page, or NULL if the page could
 * not be created.
 **/
EventPage *
event_page_new (void)
{
	EventPage *epage;

	epage = gtk_type_new (TYPE_EVENT_PAGE);
	if (!event_page_construct (epage)) {
		gtk_object_unref (GTK_OBJECT (epage));
		return NULL;
	}

	return epage;
}

GtkWidget *make_date_edit (void);

GtkWidget *
make_date_edit (void)
{
	return comp_editor_new_date_edit (TRUE, TRUE);
}

GtkWidget *make_timezone_entry (void);

GtkWidget *
make_timezone_entry (void)
{
	return e_timezone_entry_new ();
}
