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
#include "cal-util/timeutil.h"
#include "e-util/e-dialog-widgets.h"
#include "widgets/misc/e-dateedit.h"
#include "../calendar-config.h"
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
	GtkWidget *all_day_event;

	GtkWidget *description;

	GtkWidget *classification_public;
	GtkWidget *classification_private;
	GtkWidget *classification_confidential;

	GtkWidget *contacts_btn;
	GtkWidget *contacts;

	GtkWidget *categories_btn;
	GtkWidget *categories;
};



static void event_page_class_init (EventPageClass *class);
static void event_page_init (EventPage *epage);
static void event_page_destroy (GtkObject *object);

static GtkWidget *event_page_get_widget (EditorPage *page);
static void event_page_fill_widgets (EditorPage *page, CalComponent *comp);
static void event_page_fill_component (EditorPage *page, CalComponent *comp);
static void event_page_set_summary (EditorPage *page, const char *summary);
static char *event_page_get_summary (EditorPage *page);
static void event_page_set_dates (EditorPage *page, time_t start, time_t end);

/* Signal IDs */
enum {
	DATES_CHANGED,
	LAST_SIGNAL
};

static guint event_page_signals[LAST_SIGNAL] = { 0 };

static EditorPageClass *parent_class = NULL;



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

		event_page_type = gtk_type_unique (TYPE_EDITOR_PAGE, &event_page_info);
	}

	return event_page_type;
}

/* Class initialization function for the event page */
static void
event_page_class_init (EventPageClass *class)
{
	EditorPageClass *editor_page_class;
	GtkObjectClass *object_class;

	editor_page_class = (EditorPageClass *) class;
	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (TYPE_EDITOR_PAGE);

	event_page_signals[DATES_CHANGED] =
		gtk_signal_new ("dates_changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EventPageClass, dates_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, event_page_signals, LAST_SIGNAL);

	class->dates_changed = NULL;

	editor_page_class->get_widget = event_page_get_widget;
	editor_page_class->fill_widgets = event_page_fill_widgets;
	editor_page_class->fill_component = event_page_fill_component;
	editor_page_class->set_summary = event_page_set_summary;
	editor_page_class->get_summary = event_page_get_summary;
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
	priv->all_day_event = NULL;
	priv->description = NULL;
	priv->classification_public = NULL;
	priv->classification_private = NULL;
	priv->classification_confidential = NULL;
	priv->contacts_btn = NULL;
	priv->contacts = NULL;
	priv->categories_btn = NULL;
	priv->categories = NULL;
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
event_page_get_widget (EditorPage *page)
{
	EventPage *epage;
	EventPagePrivate *priv;

	epage = EVENT_PAGE (page);
	priv = epage->priv;

	return priv->main;
}

/* Checks if the event's time starts and ends at midnight, and sets the "all day
 * event" box accordingly.
 */
static void
check_all_day (EventPage *epage)
{
	EventPagePrivate *priv;
	time_t ev_start, ev_end;
	gboolean all_day = FALSE;

	priv = epage->priv;

	/* Currently we just return if the date is not set or not valid.
	   I'm not entirely sure this is the corrent thing to do. */
	ev_start = e_date_edit_get_time (E_DATE_EDIT (priv->start_time));
	g_assert (ev_start != -1);

	ev_end = e_date_edit_get_time (E_DATE_EDIT (priv->end_time));
	g_assert (ev_end != -1);

	/* all day event checkbox */
	if (time_day_begin (ev_start) == ev_start && time_day_begin (ev_end) == ev_end)
		all_day = TRUE;

	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->all_day_event), epage);
	e_dialog_toggle_set (priv->all_day_event, all_day);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->all_day_event), epage);

	e_date_edit_set_show_time (E_DATE_EDIT (priv->start_time), !all_day);
	e_date_edit_set_show_time (E_DATE_EDIT (priv->end_time), !all_day);
}

/* Fills the widgets with default values */
static void
clear_widgets (EventPage *epage)
{
	EventPagePrivate *priv;
	time_t now;

	priv = epage->priv;

	now = time (NULL);

	/* Summary, description */
	e_dialog_editable_set (priv->summary, NULL);
	e_dialog_editable_set (priv->description, NULL);

	/* Start and end times */
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->start_time), epage);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->end_time), epage);

	e_date_edit_set_time (E_DATE_EDIT (priv->start_time), now);
	e_date_edit_set_time (E_DATE_EDIT (priv->end_time), now);

	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->start_time), epage);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->end_time), epage);

	check_all_day (epage);

	/* Classification */
	e_dialog_radio_set (priv->classification_public,
			    CAL_COMPONENT_CLASS_PRIVATE, classification_map);

	/* Categories */
	e_dialog_editable_set (priv->categories, NULL);
}

/* fill_widgets handler for the event page */
static void
event_page_fill_widgets (EditorPage *page, CalComponent *comp)
{
	EventPage *epage;
	EventPagePrivate *priv;
	CalComponentText text;
	CalComponentClassification cl;
	CalComponentDateTime d;
	GSList *l;
	time_t dtstart, dtend;
	const char *categories;

	epage = EVENT_PAGE (page);
	priv = epage->priv;

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

	/* All-day events are inclusive, i.e. if the end date shown is 2nd Feb
	   then the event includes all of the 2nd Feb. We would normally show
	   3rd Feb as the end date, since it really ends at midnight on 3rd,
	   so we have to subtract a day so we only show the 2nd. */
	cal_component_get_dtstart (comp, &d);
	dtstart = icaltime_as_timet (*d.value);
	cal_component_free_datetime (&d);

	cal_component_get_dtend (comp, &d);
	dtend = icaltime_as_timet (*d.value);
	cal_component_free_datetime (&d);

	if (time_day_begin (dtstart) == dtstart && time_day_begin (dtend) == dtend)
		dtend = time_add_day (dtend, -1);

	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->start_time), epage);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->end_time), epage);

	e_date_edit_set_time (E_DATE_EDIT (priv->start_time), dtstart);
	e_date_edit_set_time (E_DATE_EDIT (priv->end_time), dtend);

	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->start_time), epage);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->end_time), epage);

	check_all_day (epage);

	/* Classification */

	cal_component_get_classification (comp, &cl);

	switch (cl) {
	case CAL_COMPONENT_CLASS_PUBLIC:
	    	e_dialog_radio_set (priv->classification_public, CAL_COMPONENT_CLASS_PUBLIC,
				    classification_map);
		break;

	case CAL_COMPONENT_CLASS_PRIVATE:
	    	e_dialog_radio_set (priv->classification_public, CAL_COMPONENT_CLASS_PRIVATE,
				    classification_map);
		break;

	case CAL_COMPONENT_CLASS_CONFIDENTIAL:
	    	e_dialog_radio_set (priv->classification_public, CAL_COMPONENT_CLASS_CONFIDENTIAL,
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
}

/* fill_component handler for the event page */
static void
event_page_fill_component (EditorPage *page, CalComponent *comp)
{
	EventPage *epage;
	EventPagePrivate *priv;
	CalComponentDateTime date;
	struct icaltimetype icaltime;
	time_t t;
	gboolean all_day_event;
	char *cat, *str;
	CalComponentClassification classif;

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

	t = e_date_edit_get_time (E_DATE_EDIT (priv->start_time));
	if (t != -1) {
		*date.value = icaltime_from_timet (t, FALSE);
		cal_component_set_dtstart (comp, &date);
	} else {
		/* FIXME: What do we do here? */
	}

	/* If the all_day toggle is set, the end date is inclusive of the
	   entire day on which it points to. */
	all_day_event = e_dialog_toggle_get (priv->all_day_event);
	t = e_date_edit_get_time (E_DATE_EDIT (priv->end_time));
	if (t != -1) {
		if (all_day_event)
			t = time_day_end (t);

		*date.value = icaltime_from_timet (t, FALSE);
		cal_component_set_dtend (comp, &date);
	} else {
		/* FIXME: What do we do here? */
	}

	/* Categories */

	cat = e_dialog_editable_get (priv->categories);
	cal_component_set_categories (comp, cat);

	if (cat)
		g_free (cat);

	/* Classification */

	classif = e_dialog_radio_get (priv->classification_public, classification_map);
	cal_component_set_classification (comp, classif);
}

/* set_summary handler for the event page */
static void
event_page_set_summary (EditorPage *page, const char *summary)
{
	EventPage *epage;
	EventPagePrivate *priv;

	epage = EVENT_PAGE (page);
	priv = epage->priv;

	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->summary), epage);
	e_utf8_gtk_entry_set_text (GTK_ENTRY (priv->summary), summary);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->summary), epage);
}

/* get_summary handler for the event page */
static char *
event_page_get_summary (EditorPage *page)
{
	EventPage *epage;
	EventPagePrivate *priv;

	epage = EVENT_PAGE (page);
	priv = epage->priv;

	return e_utf8_gtk_entry_get_text (GTK_ENTRY (priv->summary));
}

/* set_dates handler for the event page.  We do nothing since we are *the*
 * only provider of the date values.
 */
static void
event_page_set_dates (EditorPage *page, time_t start, time_t end)
{
	/* nothing */
}



/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (EventPage *epage)
{
	EventPagePrivate *priv;
	GtkWidget *toplevel;

	priv = epage->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	toplevel = GW ("event-toplevel");
	priv->main = GW ("event-page");
	if (!(toplevel && priv->main))
		return FALSE;

	gtk_widget_ref (priv->main);
	gtk_widget_unparent (priv->main);
	gtk_widget_destroy (toplevel);

	priv->summary = GW ("summary");

	priv->start_time = GW ("start-time");
	priv->end_time = GW ("end-time");
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
	
	epage = EVENT_PAGE (data);
	editor_page_notify_summary_changed (EDITOR_PAGE (epage));
}

/* Callback used when the start or end date widgets change.  We check that the
 * start date < end date and we set the "all day event" button as appropriate.
 */
static void
date_changed_cb (EDateEdit *dedit, gpointer data)
{
	EventPage *epage;
	EventPagePrivate *priv;
	time_t start, end;
	struct tm tm_start, tm_end;

	epage = EVENT_PAGE (data);
	priv = epage->priv;

	/* Ensure that start < end */

	start = e_date_edit_get_time (E_DATE_EDIT (priv->start_time));
	g_assert (start != -1);
	end = e_date_edit_get_time (E_DATE_EDIT (priv->end_time));
	g_assert (end != -1);

	if (start >= end) {
		tm_start = *localtime (&start);
		tm_end = *localtime (&end);

		if (start == end && tm_start.tm_hour == 0
		    && tm_start.tm_min == 0 && tm_start.tm_sec == 0) {
			/* If the start and end times are the same, but both are
			 * on day boundaries, then that is OK since it means we
			 * have an all-day event lasting 1 day.  So we do
			 * nothing here.
			 */
		} else if (GTK_WIDGET (dedit) == priv->start_time) {
			/* Modify the end time */

			tm_end.tm_year = tm_start.tm_year;
			tm_end.tm_mon  = tm_start.tm_mon;
			tm_end.tm_mday = tm_start.tm_mday;
			tm_end.tm_hour = tm_start.tm_hour + 1;
			tm_end.tm_min  = tm_start.tm_min;
			tm_end.tm_sec  = tm_start.tm_sec;

			gtk_signal_handler_block_by_data (GTK_OBJECT (priv->end_time), epage);
			e_date_edit_set_time (E_DATE_EDIT (priv->end_time), mktime (&tm_end));
			gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->end_time), epage);
		} else if (GTK_WIDGET (dedit) == priv->end_time) {
			/* Modify the start time */

			tm_start.tm_year = tm_end.tm_year;
			tm_start.tm_mon  = tm_end.tm_mon;
			tm_start.tm_mday = tm_end.tm_mday;
			tm_start.tm_hour = tm_end.tm_hour - 1;
			tm_start.tm_min  = tm_end.tm_min;
			tm_start.tm_sec  = tm_end.tm_sec;

			gtk_signal_handler_block_by_data (GTK_OBJECT (priv->start_time), epage);
			e_date_edit_set_time (E_DATE_EDIT (priv->start_time), mktime (&tm_start));
			gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->start_time), epage);
		} else
			g_assert_not_reached ();
	}

	/* Set the "all day event" button as appropriate */
	check_all_day (epage);

	/* Notify upstream */
	gtk_signal_emit (GTK_OBJECT (epage), event_page_signals[DATES_CHANGED]);
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
	struct tm start_tm, end_tm;
	time_t start_t, end_t;
	gboolean all_day;

	epage = EVENT_PAGE (data);
	priv = epage->priv;

	/* When the all_day toggle is turned on, the start date is rounded down
	 * to the start of the day, and end date is rounded down to the start of
	 * the day on which the event ends. The event is then taken to be
	 * inclusive of the days between the start and end days.  Note that if
	 * the event end is at midnight, we do not round it down to the previous
	 * day, since if we do that and the user repeatedly turns the all_day
	 * toggle on and off, the event keeps shrinking.  (We'd also need to
	 * make sure we didn't adjust the time when the radio button is
	 * initially set.)
	 *
	 * When the all_day_toggle is turned off, we set the event start to the
	 * start of the working day, and if the event end is on or before the
	 * day of the event start we set it to one hour after the event start.
	 */
	all_day = GTK_TOGGLE_BUTTON (toggle)->active;

	/*
	 * Start time.
	 */
	start_t = e_date_edit_get_time (E_DATE_EDIT (priv->start_time));
	g_assert (start_t != -1);

	start_tm = *localtime (&start_t);

	if (all_day) {
		/* Round down to the start of the day. */
		start_tm.tm_hour = 0;
		start_tm.tm_min  = 0;
		start_tm.tm_sec  = 0;
	} else {
		/* Set to the start of the working day. */
		start_tm.tm_hour = calendar_config_get_day_start_hour ();
		start_tm.tm_min  = calendar_config_get_day_start_minute ();
		start_tm.tm_sec  = 0;
	}

	/*
	 * End time.
	 */
	end_t = e_date_edit_get_time (E_DATE_EDIT (priv->end_time));
	g_assert (end_t != -1);

	end_tm = *localtime (&end_t);

	if (all_day) {
		/* Round down to the start of the day. */
		end_tm.tm_hour = 0;
		end_tm.tm_min  = 0;
		end_tm.tm_sec  = 0;
	} else {
		/* If the event end is now on or before the event start day,
		 * make it end one hour after the start. mktime() will fix any
		 * overflows.
		 */
		if (end_tm.tm_year < start_tm.tm_year
		    || (end_tm.tm_year == start_tm.tm_year
			&& end_tm.tm_mon < start_tm.tm_mon)
		    || (end_tm.tm_year == start_tm.tm_year
			&& end_tm.tm_mon == start_tm.tm_mon
			&& end_tm.tm_mday <= start_tm.tm_mday)) {
			end_tm.tm_year = start_tm.tm_year;
			end_tm.tm_mon = start_tm.tm_mon;
			end_tm.tm_mday = start_tm.tm_mday;
			end_tm.tm_hour = start_tm.tm_hour + 1;
		}
	}

	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->start_time), epage);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->end_time), epage);

	e_date_edit_set_time (E_DATE_EDIT (priv->start_time), mktime (&start_tm));
	e_date_edit_set_time (E_DATE_EDIT (priv->end_time), mktime (&end_tm));

	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->start_time), epage);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->end_time), epage);

	e_date_edit_set_show_time (E_DATE_EDIT (priv->start_time), !all_day);
	e_date_edit_set_show_time (E_DATE_EDIT (priv->end_time), !all_day);

	/* Notify upstream */
	gtk_signal_emit (GTK_OBJECT (epage), event_page_signals[DATES_CHANGED]);
}

/* Callback used when the categories button is clicked; we must bring up the
 * category list dialog.
 */
static void
categories_clicked_cb (GtkWidget *button, gpointer data)
{
	EventPage *epage;
	EventPagePrivate *priv;
	char *categories;
	GnomeDialog *dialog;
	int result;
	GtkWidget *entry;

	epage = EVENT_PAGE (data);
	priv = epage->priv;

	entry = priv->categories;
	categories = e_utf8_gtk_entry_get_text (GTK_ENTRY (entry));

	dialog = GNOME_DIALOG (e_categories_new (categories));
	result = gnome_dialog_run (dialog);
	g_free (categories);

	if (result == 0) {
		gtk_object_get (GTK_OBJECT (dialog),
				"categories", &categories,
				NULL);
		e_utf8_gtk_entry_set_text (GTK_ENTRY (entry), categories);
		g_free (categories);
	}

	gtk_object_destroy (GTK_OBJECT (dialog));
}

/* This is called when any field is changed; it notifies upstream. */
static void
field_changed_cb (GtkWidget *widget, gpointer data)
{
	EventPage *epage;

	epage = EVENT_PAGE (data);
	editor_page_notify_changed (EDITOR_PAGE (epage));
}

/* Hooks the widget signals */
static void
init_widgets (EventPage *epage)
{
	EventPagePrivate *priv;

	priv = epage->priv;

	/* Summary */
	gtk_signal_connect (GTK_OBJECT (priv->summary), "changed",
			    GTK_SIGNAL_FUNC (summary_changed_cb), epage);

	/* Start and end times */
	gtk_signal_connect (GTK_OBJECT (priv->start_time), "changed",
			    GTK_SIGNAL_FUNC (date_changed_cb), epage);
	gtk_signal_connect (GTK_OBJECT (priv->end_time), "changed",
			    GTK_SIGNAL_FUNC (date_changed_cb), epage);

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
	gtk_signal_connect (GTK_OBJECT (priv->all_day_event), "toggled",
			    GTK_SIGNAL_FUNC (field_changed_cb), epage);
	gtk_signal_connect (GTK_OBJECT (priv->description), "changed",
			    GTK_SIGNAL_FUNC (field_changed_cb), epage);
	gtk_signal_connect (GTK_OBJECT (priv->classification_public), "toggled",
			    GTK_SIGNAL_FUNC (field_changed_cb), epage);
	gtk_signal_connect (GTK_OBJECT (priv->classification_private), "toggled",
			    GTK_SIGNAL_FUNC (field_changed_cb), epage);
	gtk_signal_connect (GTK_OBJECT (priv->classification_confidential), "toggled",
			    GTK_SIGNAL_FUNC (field_changed_cb), epage);
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

	priv->xml = glade_xml_new (EVOLUTION_GLADEDIR "/event-page.glade", NULL);
	if (!priv->xml) {
		g_message ("event_page_construct(): Could not load the Glade XML file!");
		return NULL;
	}

	if (!get_widgets (epage)) {
		g_message ("event_page_construct(): Could not find all widgets in the XML file!");
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

/**
 * event_page_get_dates:
 * @page: An event page.
 * @start: Return value for the start date, can be NULL.
 * @end: Return value for the end date, can be NULL.
 * 
 * Queries the start and end dates for the calendar component in an event page.
 **/
void
event_page_get_dates (EventPage *page, time_t *start, time_t *end)
{
	EventPagePrivate *priv;

	g_return_if_fail (page != NULL);
	g_return_if_fail (IS_EVENT_PAGE (page));

	priv = page->priv;

	if (start)
		*start = e_date_edit_get_time (E_DATE_EDIT (priv->start_time));

	if (end)
		*end = e_date_edit_get_time (E_DATE_EDIT (priv->end_time));
}
