/* Evolution calendar - Scheduling page
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

#include <glib.h>
#include <liboaf/liboaf.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-widget.h>
#include <bonobo/bonobo-exception.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkwindow.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <glade/glade.h>
#include <gal/e-table/e-cell-combo.h>
#include <gal/e-table/e-cell-text.h>
#include <gal/e-table/e-table-simple.h>
#include <gal/e-table/e-table-scrolled.h>
#include <gal/widgets/e-unicode.h>
#include <gal/widgets/e-popup-menu.h>
#include <gal/widgets/e-gui-utils.h>
#include <widgets/misc/e-dateedit.h>
#include <e-util/e-dialog-widgets.h>
#include <e-destination.h>
#include "Evolution-Addressbook-SelectNames.h"
#include "../e-meeting-time-sel.h"
#include "../itip-utils.h"
#include "comp-editor-util.h"
#include "e-delegate-dialog.h"
#include "schedule-page.h"



/* Private part of the SchedulePage structure */
struct _SchedulePagePrivate {	
	/* Glade XML data */
	GladeXML *xml;

	/* Widgets from the Glade file */
	GtkWidget *main;

	/* Model */
	EMeetingModel *model;
	
	/* Selector */
	EMeetingTimeSelector *sel;
	
	gboolean updating;
};



static void schedule_page_class_init (SchedulePageClass *class);
static void schedule_page_init (SchedulePage *spage);
static void schedule_page_destroy (GtkObject *object);

static GtkWidget *schedule_page_get_widget (CompEditorPage *page);
static void schedule_page_focus_main_widget (CompEditorPage *page);
static void schedule_page_fill_widgets (CompEditorPage *page, CalComponent *comp);
static void schedule_page_fill_component (CompEditorPage *page, CalComponent *comp);

static void model_row_changed_cb (ETableModel *etm, int row, gpointer data);
static void row_count_changed_cb (ETableModel *etm, int row, int count, gpointer data);

static CompEditorPageClass *parent_class = NULL;



/**
 * schedule_page_get_type:
 * 
 * Registers the #SchedulePage class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the #SchedulePage class.
 **/
GtkType
schedule_page_get_type (void)
{
	static GtkType schedule_page_type;

	if (!schedule_page_type) {
		static const GtkTypeInfo schedule_page_info = {
			"SchedulePage",
			sizeof (SchedulePage),
			sizeof (SchedulePageClass),
			(GtkClassInitFunc) schedule_page_class_init,
			(GtkObjectInitFunc) schedule_page_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		schedule_page_type = 
			gtk_type_unique (TYPE_COMP_EDITOR_PAGE,
					 &schedule_page_info);
	}

	return schedule_page_type;
}

/* Class initialization function for the schedule page */
static void
schedule_page_class_init (SchedulePageClass *class)
{
	CompEditorPageClass *editor_page_class;
	GtkObjectClass *object_class;

	editor_page_class = (CompEditorPageClass *) class;
	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (TYPE_COMP_EDITOR_PAGE);

	editor_page_class->get_widget = schedule_page_get_widget;
	editor_page_class->focus_main_widget = schedule_page_focus_main_widget;
	editor_page_class->fill_widgets = schedule_page_fill_widgets;
	editor_page_class->fill_component = schedule_page_fill_component;
	editor_page_class->set_summary = NULL;
	editor_page_class->set_dates = NULL;

	object_class->destroy = schedule_page_destroy;
}

/* Object initialization function for the schedule page */
static void
schedule_page_init (SchedulePage *spage)
{
	SchedulePagePrivate *priv;

	priv = g_new0 (SchedulePagePrivate, 1);
	spage->priv = priv;

	priv->xml = NULL;

	priv->main = NULL;

	priv->updating = FALSE;
}

/* Destroy handler for the schedule page */
static void
schedule_page_destroy (GtkObject *object)
{
	SchedulePage *spage;
	SchedulePagePrivate *priv;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_SCHEDULE_PAGE (object));

	spage = SCHEDULE_PAGE (object);
	priv = spage->priv;

	if (priv->xml) {
		gtk_object_unref (GTK_OBJECT (priv->xml));
		priv->xml = NULL;
	}

	gtk_object_unref (GTK_OBJECT (priv->model));

	g_free (priv);
	spage->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* get_widget handler for the schedule page */
static GtkWidget *
schedule_page_get_widget (CompEditorPage *page)
{
	SchedulePage *spage;
	SchedulePagePrivate *priv;

	spage = SCHEDULE_PAGE (page);
	priv = spage->priv;

	return priv->main;
}

/* focus_main_widget handler for the schedule page */
static void
schedule_page_focus_main_widget (CompEditorPage *page)
{
	SchedulePage *spage;
	SchedulePagePrivate *priv;

	spage = SCHEDULE_PAGE (page);
	priv = spage->priv;

	gtk_widget_grab_focus (GTK_WIDGET (priv->sel));
}

/* Fills the widgets with default values */
static void
clear_widgets (SchedulePage *spage)
{
	SchedulePagePrivate *priv;
	
	priv = spage->priv;
}

/* fill_widgets handler for the schedule page */
static void
schedule_page_fill_widgets (CompEditorPage *page, CalComponent *comp)
{
	SchedulePage *spage;
	SchedulePagePrivate *priv;
	GSList *attendees;
	
	spage = SCHEDULE_PAGE (page);
	priv = spage->priv;

	priv->updating = TRUE;

	/* Clean the screen */
	clear_widgets (spage);

	/* Attendees */
	cal_component_get_attendee_list (comp, &attendees);

	/* So the comp editor knows we need to send if anything changes */
	if (attendees != NULL)
		comp_editor_page_notify_needs_send (COMP_EDITOR_PAGE (spage));

	cal_component_free_attendee_list (attendees);

	priv->updating = FALSE;
}

/* fill_component handler for the schedule page */
static void
schedule_page_fill_component (CompEditorPage *page, CalComponent *comp)
{
	SchedulePage *spage;
	SchedulePagePrivate *priv;
	
	spage = SCHEDULE_PAGE (page);
	priv = spage->priv;
}



/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (SchedulePage *spage)
{
	SchedulePagePrivate *priv;

	priv = spage->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	priv->main = GW ("schedule-page");
	if (!priv->main)
		return FALSE;

	gtk_widget_ref (priv->main);
	gtk_widget_unparent (priv->main);

#undef GW

	return TRUE;
}

/* Hooks the widget signals */
static void
init_widgets (SchedulePage *spage)
{
	SchedulePagePrivate *priv;

	priv = spage->priv;

	gtk_signal_connect (GTK_OBJECT (priv->model), "model_row_changed",
			    GTK_SIGNAL_FUNC (model_row_changed_cb), spage);
	gtk_signal_connect (GTK_OBJECT (priv->model), "model_rows_inserted",
			    GTK_SIGNAL_FUNC (row_count_changed_cb), spage);
	gtk_signal_connect (GTK_OBJECT (priv->model), "model_rows_deleted",
			    GTK_SIGNAL_FUNC (row_count_changed_cb), spage);
}



/**
 * schedule_page_construct:
 * @spage: An schedule page.
 * 
 * Constructs an schedule page by loading its Glade data.
 * 
 * Return value: The same object as @spage, or NULL if the widgets could not 
 * be created.
 **/
SchedulePage *
schedule_page_construct (SchedulePage *spage, EMeetingModel *emm)
{
	SchedulePagePrivate *priv;
	
	priv = spage->priv;

	priv->xml = glade_xml_new (EVOLUTION_GLADEDIR 
				   "/schedule-page.glade", NULL);
	if (!priv->xml) {
		g_message ("schedule_page_construct(): "
			   "Could not load the Glade XML file!");
		return NULL;
	}

	if (!get_widgets (spage)) {
		g_message ("schedule_page_construct(): "
			   "Could not find all widgets in the XML file!");
		return NULL;
	}

	/* Model */
	gtk_object_ref (GTK_OBJECT (emm));
	priv->model = emm;
	
	/* Selector */
	priv->sel = E_MEETING_TIME_SELECTOR (e_meeting_time_selector_new (emm));
	gtk_widget_show (GTK_WIDGET (priv->sel));
	gtk_box_pack_start (GTK_BOX (priv->main), GTK_WIDGET (priv->sel), TRUE, TRUE, 2);

	/* Init the widget signals */
	init_widgets (spage);

	return spage;
}

/**
 * schedule_page_new:
 * 
 * Creates a new schedule page.
 * 
 * Return value: A newly-created schedule page, or NULL if the page could
 * not be created.
 **/
SchedulePage *
schedule_page_new (EMeetingModel *emm)
{
	SchedulePage *spage;

	spage = gtk_type_new (TYPE_SCHEDULE_PAGE);
	if (!schedule_page_construct (spage, emm)) {
		gtk_object_unref (GTK_OBJECT (spage));
		return NULL;
	}

	return spage;
}

static void
model_row_changed_cb (ETableModel *etm, int row, gpointer data)
{
	SchedulePage *spage = SCHEDULE_PAGE (data);
	SchedulePagePrivate *priv;
	
	priv = spage->priv;
	
	if (!priv->updating)
		comp_editor_page_notify_changed (COMP_EDITOR_PAGE (spage));
}

static void
row_count_changed_cb (ETableModel *etm, int row, int count, gpointer data)
{
	SchedulePage *spage = SCHEDULE_PAGE (data);
	SchedulePagePrivate *priv;
	
	priv = spage->priv;
	
	if (!priv->updating)
		comp_editor_page_notify_changed (COMP_EDITOR_PAGE (spage));
}
