/* Evolution calendar - Base class for calendar component editor pages
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
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
#include "comp-editor-page.h"



static void comp_editor_page_class_init (CompEditorPageClass *class);

/* Signal IDs */

enum {
	CHANGED,
	NEEDS_SEND,
	SUMMARY_CHANGED,
	DATES_CHANGED,
	LAST_SIGNAL
};

static guint comp_editor_page_signals[LAST_SIGNAL];

#define CLASS(page) (COMP_EDITOR_PAGE_CLASS (GTK_OBJECT (page)->klass))



/**
 * comp_editor_page_get_type:
 * 
 * Registers the #CompEditorPage class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the #CompEditorPage class.
 **/
GtkType
comp_editor_page_get_type (void)
{
	static GtkType comp_editor_page_type = 0;

	if (!comp_editor_page_type) {
		static const GtkTypeInfo comp_editor_page_info = {
			"CompEditorPage",
			sizeof (CompEditorPage),
			sizeof (CompEditorPageClass),
			(GtkClassInitFunc) comp_editor_page_class_init,
			(GtkObjectInitFunc) NULL,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		comp_editor_page_type =
			gtk_type_unique (GTK_TYPE_OBJECT,
					 &comp_editor_page_info);
	}

	return comp_editor_page_type;
}

/* Class initialization function for the abstract editor page */
static void
comp_editor_page_class_init (CompEditorPageClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	comp_editor_page_signals[CHANGED] =
		gtk_signal_new ("changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (CompEditorPageClass,
						   changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	comp_editor_page_signals[NEEDS_SEND] =
		gtk_signal_new ("needs_send",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (CompEditorPageClass,
						   needs_send),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	comp_editor_page_signals[SUMMARY_CHANGED] =
		gtk_signal_new ("summary_changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (CompEditorPageClass,
						   summary_changed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	comp_editor_page_signals[DATES_CHANGED] =
		gtk_signal_new ("dates_changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (CompEditorPageClass,
						   dates_changed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class,
				      comp_editor_page_signals,
				      LAST_SIGNAL);

	class->changed = NULL;
	class->summary_changed = NULL;
	class->dates_changed = NULL;

	class->get_widget = NULL;
	class->fill_widgets = NULL;
	class->fill_component = NULL;
	class->set_summary = NULL;
	class->set_dates = NULL;
	class->set_cal_client = NULL;
}



/**
 * comp_editor_page_get_widget:
 * @page: An editor page.
 * 
 * Queries the main widget of an editor page.
 * 
 * Return value: The widget that is the page's upper container.  It should
 * normally be inserted in a notebook widget.
 **/
GtkWidget *
comp_editor_page_get_widget (CompEditorPage *page)
{
	g_return_val_if_fail (page != NULL, NULL);
	g_return_val_if_fail (IS_COMP_EDITOR_PAGE (page), NULL);

	g_assert (CLASS (page)->get_widget != NULL);
	return (* CLASS (page)->get_widget) (page);
}

/**
 * comp_editor_page_fill_widgets:
 * @page: An editor page.
 * @comp: A calendar component.
 * 
 * Fills the widgets of an editor page with the data from a calendar component.
 **/
void
comp_editor_page_fill_widgets (CompEditorPage *page, CalComponent *comp)
{
	g_return_if_fail (page != NULL);
	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));
	g_return_if_fail (comp != NULL);

	g_assert (CLASS (page)->fill_widgets != NULL);
	(* CLASS (page)->fill_widgets) (page, comp);
}

/**
 * comp_editor_page_fill_component:
 * @page: An editor page.
 * @comp: A calendar component.
 * 
 * Takes the data from the widgets of an editor page and sets it on a calendar
 * component, replacing the contents of the properties that the editor page
 * knows how to manipulate.
 **/
void
comp_editor_page_fill_component (CompEditorPage *page, CalComponent *comp)
{
	g_return_if_fail (page != NULL);
	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));
	g_return_if_fail (comp != NULL);

	if (CLASS (page)->fill_component != NULL)
		(* CLASS (page)->fill_component) (page, comp);
}

/**
 * comp_editor_page_set_cal_client:
 * @page: An editor page
 * @client: A #CalClient object
 * 
 * Sets the #CalClient for the dialog page to use.
 **/
void
comp_editor_page_set_cal_client (CompEditorPage *page, CalClient *client)
{
	g_return_if_fail (page != NULL);
	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));

	if (CLASS (page)->set_cal_client != NULL)
		(* CLASS (page)->set_cal_client) (page, client);
}

/**
 * comp_editor_page_set_summary:
 * @page: An editor page
 * @summary: The text of the new summary value
 * 
 * Sets the summary value for this group of widgets
 **/
void
comp_editor_page_set_summary (CompEditorPage *page, const char *summary)
{
	g_return_if_fail (page != NULL);
	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));

	if (CLASS (page)->set_summary != NULL)
		(* CLASS (page)->set_summary) (page, summary);
}

/**
 * comp_editor_page_set_dates:
 * @page: An editor page
 * @dates: A collection of various dates in time_t format
 * 
 * Sets the date values for this group of widgets
 **/
void
comp_editor_page_set_dates (CompEditorPage *page, CompEditorPageDates *dates)
{
	g_return_if_fail (page != NULL);
	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));

	if (CLASS (page)->set_dates != NULL)
		(* CLASS (page)->set_dates) (page, dates);
}

/**
 * comp_editor_page_notify_changed:
 * @page: An editor page.
 * 
 * Makes an editor page emit the "changed" signal.  This is meant to be
 * used only by page implementations.
 **/
void
comp_editor_page_notify_changed (CompEditorPage *page)
{
	g_return_if_fail (page != NULL);
	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));

	gtk_signal_emit (GTK_OBJECT (page), comp_editor_page_signals[CHANGED]);
}

/**
 * comp_editor_page_notify_needs_send:
 * @page: 
 * 
 * 
 **/
void
comp_editor_page_notify_needs_send (CompEditorPage *page)
{
	g_return_if_fail (page != NULL);
	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));

	gtk_signal_emit (GTK_OBJECT (page), comp_editor_page_signals[NEEDS_SEND]);	
}

/**
 * comp_editor_page_notify_summary_changed:
 * @page: An editor page.
 * 
 * Makes an editor page emit the "summary_changed" signal.  This is meant to be
 * used only by page implementations.
 **/
void
comp_editor_page_notify_summary_changed (CompEditorPage *page,
					 const char *summary)
{
	g_return_if_fail (page != NULL);
	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));

	
	gtk_signal_emit (GTK_OBJECT (page),
			 comp_editor_page_signals[SUMMARY_CHANGED],
			 summary);
}

/**
 * comp_editor_page_notify_dates_changed:
 * @page: An editor page.
 * 
 * Makes an editor page emit the "dates_changed" signal.  This is meant to be
 * used only by page implementations.
 **/
void
comp_editor_page_notify_dates_changed (CompEditorPage *page,
				       CompEditorPageDates *dates)
{
	g_return_if_fail (page != NULL);
	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));

	gtk_signal_emit (GTK_OBJECT (page),
			 comp_editor_page_signals[DATES_CHANGED],
			 dates);
}
