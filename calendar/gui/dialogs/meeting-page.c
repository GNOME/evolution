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

#include <liboaf/liboaf.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkradiobutton.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktable.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkvbox.h>
#include <glade/glade.h>
#include <gal/widgets/e-unicode.h>
#include <widgets/misc/e-dateedit.h>
#include <widgets/meeting-time-sel/e-meeting-time-sel.h>
#include <e-util/e-dialog-widgets.h>
#include "../Evolution-Addressbook-SelectNames.h"
#include "comp-editor-util.h"
#include "meeting-page.h"


#define SELECT_NAMES_OAFID "OAFIID:GNOME_Evolution_Addressbook_SelectNames"

/* Private part of the MeetingPage structure */
struct _MeetingPagePrivate {
	/* Glade XML data */
	GladeXML *xml;

	/* Widgets from the Glade file */
	GtkWidget *main;

	EMeetingTimeSelector *selector;
	GtkWidget *table;
	
	/* Other widgets */
	GtkWidget *rb1[2];
	GtkWidget *rb2[2];
	
	/* For handling the invite button */
	GNOME_Evolution_Addressbook_SelectNames corba_select_names;

	gboolean updating;
};



static void meeting_page_class_init (MeetingPageClass *class);
static void meeting_page_init (MeetingPage *mpage);
static void meeting_page_destroy (GtkObject *object);

static GtkWidget *meeting_page_get_widget (CompEditorPage *page);
static void meeting_page_fill_widgets (CompEditorPage *page, CalComponent *comp);
static void meeting_page_fill_component (CompEditorPage *page, CalComponent *comp);
static void meeting_page_set_summary (CompEditorPage *page, const char *summary);
static void meeting_page_set_dates (CompEditorPage *page, CompEditorPageDates *dates);

static CompEditorPageClass *parent_class = NULL;



/**
 * meeting_page_get_type:
 * 
 * Registers the #MeetingPage class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the #MeetingPage class.
 **/
GtkType
meeting_page_get_type (void)
{
	static GtkType meeting_page_type;

	if (!meeting_page_type) {
		static const GtkTypeInfo meeting_page_info = {
			"MeetingPage",
			sizeof (MeetingPage),
			sizeof (MeetingPageClass),
			(GtkClassInitFunc) meeting_page_class_init,
			(GtkObjectInitFunc) meeting_page_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		meeting_page_type = 
			gtk_type_unique (TYPE_COMP_EDITOR_PAGE,
					 &meeting_page_info);
	}

	return meeting_page_type;
}

/* Class initialization function for the task page */
static void
meeting_page_class_init (MeetingPageClass *class)
{
	CompEditorPageClass *editor_page_class;
	GtkObjectClass *object_class;

	editor_page_class = (CompEditorPageClass *) class;
	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (TYPE_COMP_EDITOR_PAGE);

	editor_page_class->get_widget = meeting_page_get_widget;
	editor_page_class->fill_widgets = meeting_page_fill_widgets;
	editor_page_class->fill_component = meeting_page_fill_component;
	editor_page_class->set_summary = meeting_page_set_summary;
	editor_page_class->set_dates = meeting_page_set_dates;

	object_class->destroy = meeting_page_destroy;
}

/* Object initialization function for the task page */
static void
meeting_page_init (MeetingPage *mpage)
{
	MeetingPagePrivate *priv;

	priv = g_new0 (MeetingPagePrivate, 1);
	mpage->priv = priv;

	priv->xml = NULL;

	priv->main = NULL;
	priv->selector = NULL;

	priv->updating = FALSE;
}

/* Destroy handler for the task page */
static void
meeting_page_destroy (GtkObject *object)
{
	MeetingPage *mpage;
	MeetingPagePrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_MEETING_PAGE (object));

	mpage = MEETING_PAGE (object);
	priv = mpage->priv;

	if (priv->xml) {
		gtk_object_unref (GTK_OBJECT (priv->xml));
		priv->xml = NULL;
	}

	g_free (priv);
	mpage->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* get_widget handler for the task page */
static GtkWidget *
meeting_page_get_widget (CompEditorPage *page)
{
	MeetingPage *mpage;
	MeetingPagePrivate *priv;

	mpage = MEETING_PAGE (page);
	priv = mpage->priv;

	return priv->main;
}

/* Fills the widgets with default values */
static void
clear_widgets (MeetingPage *mpage)
{
	MeetingPagePrivate *priv;

	priv = mpage->priv;
}

/* fill_widgets handler for the task page */
static void
meeting_page_fill_widgets (CompEditorPage *page, CalComponent *comp)
{
	MeetingPage *mpage;
	MeetingPagePrivate *priv;
	CalComponentDateTime dtstart, dtend;
	struct icaltimetype start, end;
	mpage = MEETING_PAGE (page);
	priv = mpage->priv;

	priv->updating = TRUE;
	
	/* Clean the screen */
	clear_widgets (mpage);
	
	/* Selector */
	cal_component_get_dtstart (comp, &dtstart);
	cal_component_get_dtend (comp, &dtend);
	start = icaltime_as_zone (*dtstart.value, dtstart.tzid);
	end = icaltime_as_zone (*dtend.value, dtend.tzid);
	e_meeting_time_selector_set_meeting_time (priv->selector,
						  start.year,
						  start.month,
						  start.day,
						  start.hour,
						  start.minute,
						  end.year,
						  end.month,
						  end.day,
						  end.hour,
						  end.minute);
	cal_component_free_datetime (&dtstart);
	cal_component_free_datetime (&dtend);

	priv->updating = FALSE;
}

/* fill_component handler for the task page */
static void
meeting_page_fill_component (CompEditorPage *page, CalComponent *comp)
{
#if 0
	MeetingPage *mpage;
	MeetingPagePrivate *priv;
	CalComponentDateTime date;
	time_t t;
	char *url;
	
	mpage = MEETING_PAGE (page);
	priv = mpage->priv;

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
#endif
}

/* set_summary handler for the task page */
static void
meeting_page_set_summary (CompEditorPage *page, const char *summary)
{
#if 0
	MeetingPage *mpage;
	MeetingPagePrivate *priv;
	gchar *s;
	
	mpage = MEETING_PAGE (page);
	priv = mpage->priv;

	s = e_utf8_to_gtk_string (priv->summary, summary);
	gtk_label_set_text (GTK_LABEL (priv->summary), s);
	g_free (s);
#endif
}

static void
meeting_page_set_dates (CompEditorPage *page, CompEditorPageDates *dates)
{
	MeetingPage *mpage;
	MeetingPagePrivate *priv;

	mpage = MEETING_PAGE (page);
	priv = mpage->priv;

//	comp_editor_date_label (dates, priv->date_time);
}



/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (MeetingPage *mpage)
{
	MeetingPagePrivate *priv;

	priv = mpage->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	priv->main = GW ("meeting-page");
	g_assert (priv->main);
	gtk_widget_ref (priv->main);
	gtk_widget_unparent (priv->main);

	priv->selector = E_MEETING_TIME_SELECTOR (GW ("selector"));
	priv->table = GW ("status-table");
	
#undef GW

	return (priv->selector 
		&& priv->table);
}

/* Creates the radio buttons in the given container */
static void
create_radio_buttons (GtkWidget *box, GtkWidget **button1, GtkWidget **button2)
{
	GtkWidget *rb1, *rb2;
	GSList *grp;

	/* The radio buttions to change views */
	rb1 = gtk_radio_button_new_with_label (NULL,
					       "Show attendee scheduling");
	gtk_widget_show (rb1);

	grp = gtk_radio_button_group (GTK_RADIO_BUTTON (rb1));
	rb2 = gtk_radio_button_new_with_label (grp, "Show attendee status");
	gtk_widget_show (rb2);
	
	gtk_box_pack_start (GTK_BOX (box), rb1, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), rb2, FALSE, FALSE, 0);

	*button1 = rb1;
	*button2 = rb2;
}

/* Callback used when the start or end date widgets change.  We check that the
 * start date < end date and we set the "all day task" button as appropriate.
 */
static void
date_changed_cb (EDateEdit *dedit, gpointer data)
{
#if 0
	MeetingPage *mpage;
	MeetingPagePrivate *priv;
	CompEditorPageDates dates;

	mpage = MEETING_PAGE (data);
	priv = mpage->priv;

	if (priv->updating)
		return;

	dates.start = 0;
	dates.end = 0;
	dates.due = 0;
	dates.complete = e_date_edit_get_time (E_DATE_EDIT (priv->completed_date));
	
	/* Notify upstream */
	comp_editor_page_notify_dates_changed (COMP_EDITOR_PAGE (mpage), &dates);
#endif
}

/* This is called when any field is changed; it notifies upstream. */
static void
field_changed_cb (GtkWidget *widget, gpointer data)
{
	MeetingPage *mpage;
	MeetingPagePrivate *priv;
	
	mpage = MEETING_PAGE (data);
	priv = mpage->priv;
	
	if (!priv->updating)
		comp_editor_page_notify_changed (COMP_EDITOR_PAGE (mpage));
}

static gboolean
get_select_name_dialog (MeetingPage *mpage) 
{
	MeetingPagePrivate *priv;
	CORBA_Environment ev;
	
	priv = mpage->priv;

	if (priv->corba_select_names != CORBA_OBJECT_NIL)
		return TRUE;
	
	CORBA_exception_init (&ev);

	priv->corba_select_names = oaf_activate_from_id (SELECT_NAMES_OAFID, 0, NULL, &ev);

	/* OAF seems to be broken -- it can return a CORBA_OBJECT_NIL without
           raising an exception in `ev'.  */
	if (ev._major != CORBA_NO_EXCEPTION || priv->corba_select_names == CORBA_OBJECT_NIL) {
		g_warning ("Cannot activate -- %s", SELECT_NAMES_OAFID);
		CORBA_exception_free (&ev);
		return FALSE;
	}

	CORBA_exception_free (&ev);

	return TRUE;
}

/* Function called to invite more people */
static void
invite_cb (GtkWidget *widget, gpointer data) 
{
	MeetingPage *mpage;
	MeetingPagePrivate *priv;
	CORBA_Environment ev;
	
	mpage = MEETING_PAGE (data);
	priv = mpage->priv;
	
	if (!get_select_name_dialog (mpage))
		return;
	
	CORBA_exception_init (&ev);

	GNOME_Evolution_Addressbook_SelectNames_addSection (
		priv->corba_select_names, "Required", "Required", &ev);

	GNOME_Evolution_Addressbook_SelectNames_activateDialog (
		priv->corba_select_names, "Test", &ev);

	CORBA_exception_free (&ev);

	if (!priv->updating)
		comp_editor_page_notify_changed (COMP_EDITOR_PAGE (mpage));
}

static void
page_toggle_cb (GtkWidget *widget, gpointer data)
{
	MeetingPage *mpage;
	MeetingPagePrivate *priv;
	gboolean active;
	
	mpage = MEETING_PAGE (data);
	priv = mpage->priv;

	active = GTK_TOGGLE_BUTTON (widget)->active;
	if (active)
		gtk_notebook_set_page (GTK_NOTEBOOK (priv->main), 0);
	else
		gtk_notebook_set_page (GTK_NOTEBOOK (priv->main), 1);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->rb1[0]), active);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->rb1[1]), !active);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->rb2[0]), active);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->rb2[1]), !active);
}
	
/* Hooks the widget signals */
static void
init_widgets (MeetingPage *mpage)
{
	MeetingPagePrivate *priv;

	priv = mpage->priv;

	/* Selector */
	gtk_signal_connect (GTK_OBJECT (priv->selector->invite_button), 
			    "clicked", GTK_SIGNAL_FUNC (invite_cb), mpage);

	/* Radio buttons */
	gtk_signal_connect (GTK_OBJECT (priv->rb1[0]), "toggled", 
			    GTK_SIGNAL_FUNC (page_toggle_cb), mpage);
	gtk_signal_connect (GTK_OBJECT (priv->rb2[0]), "toggled", 
			    GTK_SIGNAL_FUNC (page_toggle_cb), mpage);
	
#if 0
	/* Completed Date */
	gtk_signal_connect (GTK_OBJECT (priv->completed_date), "changed",
			    GTK_SIGNAL_FUNC (date_changed_cb), mpage);

#endif
}



/**
 * meeting_page_construct:
 * @mpage: An task details page.
 * 
 * Constructs an task page by loading its Glade data.
 * 
 * Return value: The same object as @mpage, or NULL if the widgets could not 
 * be created.
 **/
MeetingPage *
meeting_page_construct (MeetingPage *mpage)
{
	MeetingPagePrivate *priv;
	GtkWidget *vbox;
	
	priv = mpage->priv;

	priv->xml = glade_xml_new (EVOLUTION_GLADEDIR 
				   "/meeting-page.glade", NULL);
	if (!priv->xml) {
		g_message ("meeting_page_construct(): "
			   "Could not load the Glade XML file!");
		return NULL;
	}

	if (!get_widgets (mpage)) {
		g_message ("meeting_page_construct(): "
			   "Could not find all widgets in the XML file!");
		return NULL;
	}

	/* Stuff for the second page */
	vbox = gtk_vbox_new (FALSE, 2);
	gtk_widget_show (vbox);
	gtk_table_attach (GTK_TABLE (priv->table), vbox, 0, 1, 0, 1,
			  GTK_EXPAND | GTK_FILL, GTK_FILL, 2, 0);

	/* The radio buttions to change views on first page */
	create_radio_buttons (priv->selector->attendees_title_bar_vbox, 
			      &priv->rb1[0], &priv->rb1[1]);

	/* The radio buttions to change views on second page */
	create_radio_buttons (vbox, &priv->rb2[0], &priv->rb2[1]);

	/* Init the widget signals */
	init_widgets (mpage);

	return mpage;
}

/**
 * meeting_page_new:
 * 
 * Creates a new task details page.
 * 
 * Return value: A newly-created task details page, or NULL if the page could
 * not be created.
 **/
MeetingPage *
meeting_page_new (void)
{
	MeetingPage *mpage;

	mpage = gtk_type_new (TYPE_MEETING_PAGE);
	if (!meeting_page_construct (mpage)) {
		gtk_object_unref (GTK_OBJECT (mpage));
		return NULL;
	}

	return mpage;
}

GtkWidget *meeting_page_create_selector (void);

GtkWidget *
meeting_page_create_selector (void)
{
	GtkWidget *sel;

	sel = e_meeting_time_selector_new ();

	return sel;
}
