/* Evolution calendar - Main page of the task editor dialog
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
#include <widgets/misc/e-dateedit.h>
#include "e-util/e-dialog-widgets.h"
#include "../e-timezone-entry.h"
#include "comp-editor-util.h"
#include "task-details-page.h"



/* Private part of the TaskDetailsPage structure */
struct _TaskDetailsPagePrivate {
	/* Glade XML data */
	GladeXML *xml;

	/* Widgets from the Glade file */
	GtkWidget *main;

	GtkWidget *summary;
	GtkWidget *date_time;
	
	GtkWidget *completed_date;
	GtkWidget *completed_timezone;

	GtkWidget *url;

	gboolean updating;
};



static void task_details_page_class_init (TaskDetailsPageClass *class);
static void task_details_page_init (TaskDetailsPage *tdpage);
static void task_details_page_destroy (GtkObject *object);

static GtkWidget *task_details_page_get_widget (CompEditorPage *page);
static void task_details_page_fill_widgets (CompEditorPage *page, CalComponent *comp);
static void task_details_page_fill_component (CompEditorPage *page, CalComponent *comp);
static void task_details_page_set_summary (CompEditorPage *page, const char *summary);
static void task_details_page_set_dates (CompEditorPage *page, CompEditorPageDates *dates);
static void task_details_page_set_cal_client (CompEditorPage *page, CalClient *client);

static CompEditorPageClass *parent_class = NULL;



/**
 * task_details_page_get_type:
 * 
 * Registers the #TaskDetailsPage class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the #TaskDetailsPage class.
 **/
GtkType
task_details_page_get_type (void)
{
	static GtkType task_details_page_type;

	if (!task_details_page_type) {
		static const GtkTypeInfo task_details_page_info = {
			"TaskDetailsPage",
			sizeof (TaskDetailsPage),
			sizeof (TaskDetailsPageClass),
			(GtkClassInitFunc) task_details_page_class_init,
			(GtkObjectInitFunc) task_details_page_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		task_details_page_type = 
			gtk_type_unique (TYPE_COMP_EDITOR_PAGE,
					 &task_details_page_info);
	}

	return task_details_page_type;
}

/* Class initialization function for the task page */
static void
task_details_page_class_init (TaskDetailsPageClass *class)
{
	CompEditorPageClass *editor_page_class;
	GtkObjectClass *object_class;

	editor_page_class = (CompEditorPageClass *) class;
	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (TYPE_COMP_EDITOR_PAGE);

	editor_page_class->get_widget = task_details_page_get_widget;
	editor_page_class->fill_widgets = task_details_page_fill_widgets;
	editor_page_class->fill_component = task_details_page_fill_component;
	editor_page_class->set_summary = task_details_page_set_summary;
	editor_page_class->set_dates = task_details_page_set_dates;
	editor_page_class->set_cal_client = task_details_page_set_cal_client;

	object_class->destroy = task_details_page_destroy;
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
	priv->summary = NULL;
	priv->date_time = NULL;
	priv->completed_date = NULL;
	priv->completed_timezone = NULL;
	priv->url = NULL;

	priv->updating = FALSE;
}

/* Destroy handler for the task page */
static void
task_details_page_destroy (GtkObject *object)
{
	TaskDetailsPage *tdpage;
	TaskDetailsPagePrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_TASK_DETAILS_PAGE (object));

	tdpage = TASK_DETAILS_PAGE (object);
	priv = tdpage->priv;

	if (priv->xml) {
		gtk_object_unref (GTK_OBJECT (priv->xml));
		priv->xml = NULL;
	}

	g_free (priv);
	tdpage->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
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

/* Fills the widgets with default values */
static void
clear_widgets (TaskDetailsPage *tdpage)
{
	TaskDetailsPagePrivate *priv;

	priv = tdpage->priv;

	/* Summary */
	gtk_label_set_text (GTK_LABEL (priv->summary), "");

	/* Start date */
	gtk_label_set_text (GTK_LABEL (priv->date_time), "");

	/* Date completed */
	e_date_edit_set_time (E_DATE_EDIT (priv->completed_date), -1);

	/* URL */
	e_dialog_editable_set (priv->url, NULL);
}

/* fill_widgets handler for the task page */
static void
task_details_page_fill_widgets (CompEditorPage *page, CalComponent *comp)
{
	TaskDetailsPage *tdpage;
	TaskDetailsPagePrivate *priv;
	CalComponentText text;
	const char *url;
	CompEditorPageDates dates;
	
	tdpage = TASK_DETAILS_PAGE (page);
	priv = tdpage->priv;

	priv->updating = TRUE;
	
	/* Clean the screen */
	clear_widgets (tdpage);
	
	/* Summary */
	cal_component_get_summary (comp, &text);
	task_details_page_set_summary (page, text.value);

	/* Dates */
	comp_editor_dates (&dates, comp);
	task_details_page_set_dates (page, &dates);
	
	/* URL */
	cal_component_get_url (comp, &url);
	e_dialog_editable_set (priv->url, url);

	priv->updating = FALSE;
}

/* fill_component handler for the task page */
static void
task_details_page_fill_component (CompEditorPage *page, CalComponent *comp)
{
	TaskDetailsPage *tdpage;
	TaskDetailsPagePrivate *priv;
	CalComponentDateTime date;
	time_t t;
	char *url;
	
	tdpage = TASK_DETAILS_PAGE (page);
	priv = tdpage->priv;

	/* Completed Date. */
	date.value = g_new (struct icaltimetype, 1);
	date.tzid = NULL;

	t = e_date_edit_get_time (E_DATE_EDIT (priv->completed_date));
	if (t != -1) {
		*date.value = icaltime_from_timet (t, FALSE);
		cal_component_set_completed (comp, date.value);
	} else {
		cal_component_set_completed (comp, NULL);
	}

	g_free (date.value);

	/* URL. */
	url = e_dialog_editable_get (priv->url);
	cal_component_set_url (comp, url);
	if (url)
		g_free (url);
}

/* set_summary handler for the task page */
static void
task_details_page_set_summary (CompEditorPage *page, const char *summary)
{
	TaskDetailsPage *tdpage;
	TaskDetailsPagePrivate *priv;
	gchar *s;
	
	tdpage = TASK_DETAILS_PAGE (page);
	priv = tdpage->priv;

	s = e_utf8_to_gtk_string (priv->summary, summary);
	gtk_label_set_text (GTK_LABEL (priv->summary), s);
	g_free (s);
}

static void
task_details_page_set_dates (CompEditorPage *page, CompEditorPageDates *dates)
{
	TaskDetailsPage *tdpage;
	TaskDetailsPagePrivate *priv;

	tdpage = TASK_DETAILS_PAGE (page);
	priv = tdpage->priv;

	comp_editor_date_label (dates, priv->date_time);
	if (dates->complete != 0)
	        e_date_edit_set_time (E_DATE_EDIT (priv->completed_date),
				      dates->complete);
}

static void
task_details_page_set_cal_client (CompEditorPage *page, CalClient *client)
{
	TaskDetailsPage *tdpage;
	TaskDetailsPagePrivate *priv;

	tdpage = TASK_DETAILS_PAGE (page);
	priv = tdpage->priv;

	e_timezone_entry_set_cal_client (E_TIMEZONE_ENTRY (priv->completed_timezone), client);
}



/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (TaskDetailsPage *tdpage)
{
	TaskDetailsPagePrivate *priv;

	priv = tdpage->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	priv->main = GW ("task-details-page");
	g_assert (priv->main);
	gtk_widget_ref (priv->main);
	gtk_widget_unparent (priv->main);

	priv->summary = GW ("summary");
	priv->date_time = GW ("date-time");

	priv->completed_date = GW ("completed-date");
	priv->completed_timezone = GW ("completed-timezone");

	priv->url = GW ("url");

#undef GW

	return (priv->summary
		&& priv->date_time
		&& priv->completed_date
		&& priv->completed_timezone
		&& priv->url);
}

/* Callback used when the start or end date widgets change.  We check that the
 * start date < end date and we set the "all day task" button as appropriate.
 */
static void
date_changed_cb (EDateEdit *dedit, gpointer data)
{
	TaskDetailsPage *tdpage;
	TaskDetailsPagePrivate *priv;
	CompEditorPageDates dates;

	tdpage = TASK_DETAILS_PAGE (data);
	priv = tdpage->priv;

	if (priv->updating)
		return;

	dates.start = 0;
	dates.end = 0;
	dates.due = 0;
	dates.complete = e_date_edit_get_time (E_DATE_EDIT (priv->completed_date));
	
	/* Notify upstream */
	comp_editor_page_notify_dates_changed (COMP_EDITOR_PAGE (tdpage), &dates);
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

	/* Completed Date */
	gtk_signal_connect (GTK_OBJECT (priv->completed_date), "changed",
			    GTK_SIGNAL_FUNC (date_changed_cb), tdpage);

	gtk_signal_connect (GTK_OBJECT (priv->completed_timezone), "changed",
			    GTK_SIGNAL_FUNC (field_changed_cb), tdpage);

	/* URL */
	gtk_signal_connect (GTK_OBJECT (priv->url), "changed",
			    GTK_SIGNAL_FUNC (field_changed_cb), tdpage);
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
				   "/task-details-page.glade", NULL);
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

	tdpage = gtk_type_new (TYPE_TASK_DETAILS_PAGE);
	if (!task_details_page_construct (tdpage)) {
		gtk_object_unref (GTK_OBJECT (tdpage));
		return NULL;
	}

	return tdpage;
}

GtkWidget *task_details_page_create_date_edit (void);

GtkWidget *
task_details_page_create_date_edit (void)
{
	GtkWidget *dedit;

	dedit = comp_editor_new_date_edit (TRUE, TRUE);
	e_date_edit_set_allow_no_date_set (E_DATE_EDIT (dedit), TRUE);

	return dedit;
}
