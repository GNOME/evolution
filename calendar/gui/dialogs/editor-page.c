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
#include "editor-page.h"



static void editor_page_class_init (EditorPageClass *class);

/* Signal IDs */

enum {
	CHANGED,
	SUMMARY_CHANGED,
	LAST_SIGNAL
};

static guint editor_page_signals[LAST_SIGNAL];

#define CLASS(page) (EDITOR_PAGE_CLASS (GTK_OBJECT (page)->klass))



/**
 * editor_page_get_type:
 * 
 * Registers the #EditorPage class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the #EditorPage class.
 **/
GtkType
editor_page_get_type (void)
{
	static GtkType editor_page_type = 0;

	if (!editor_page_type) {
		static const GtkTypeInfo editor_page_info = {
			"EditorPage",
			sizeof (EditorPage),
			sizeof (EditorPageClass),
			(GtkClassInitFunc) editor_page_class_init,
			(GtkObjectInitFunc) NULL,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		editor_page_type = gtk_type_unique (GTK_TYPE_OBJECT, &editor_page_info);
	}

	return editor_page_type;
}

/* Class initialization function for the abstract editor page */
static void
editor_page_class_init (EditorPageClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	editor_page_signals[CHANGED] =
		gtk_signal_new ("changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EditorPageClass, changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	editor_page_signals[SUMMARY_CHANGED] =
		gtk_signal_new ("summary_changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EditorPageClass, summary_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, editor_page_signals, LAST_SIGNAL);

	class->changed = NULL;
	class->summary_changed = NULL;

	class->get_widget = NULL;
	class->fill_widgets = NULL;
	class->fill_component = NULL;
	class->set_summary = NULL;
	class->get_summary = NULL;
	class->set_dtstart = NULL;
}



/**
 * editor_page_get_widget:
 * @page: An editor page.
 * 
 * Queries the main widget of an editor page.
 * 
 * Return value: The widget that is the page's upper container.  It should
 * normally be inserted in a notebook widget.
 **/
GtkWidget *
editor_page_get_widget (EditorPage *page)
{
	g_return_val_if_fail (page != NULL, NULL);
	g_return_val_if_fail (IS_EDITOR_PAGE (page), NULL);

	g_assert (CLASS (page)->get_widget != NULL);
	return (* CLASS (page)->get_widget) (page);
}

/**
 * editor_page_fill_widgets:
 * @page: An editor page.
 * @comp: A calendar component.
 * 
 * Fills the widgets of an editor page with the data from a calendar component.
 **/
void
editor_page_fill_widgets (EditorPage *page, CalComponent *comp)
{
	g_return_if_fail (page != NULL);
	g_return_if_fail (IS_EDITOR_PAGE (page));
	g_return_if_fail (comp != NULL);

	g_assert (CLASS (page)->fill_widgets != NULL);
	(* CLASS (page)->fill_widgets) (page, comp);
}

/**
 * editor_page_fill_component:
 * @page: An editor page.
 * @comp: A calendar component.
 * 
 * Takes the data from the widgets of an editor page and sets it on a calendar
 * component, replacing the contents of the properties that the editor page
 * knows how to manipulate.
 **/
void
editor_page_fill_component (EditorPage *page, CalComponent *comp)
{
	g_return_if_fail (page != NULL);
	g_return_if_fail (IS_EDITOR_PAGE (page));
	g_return_if_fail (comp != NULL);

	g_assert (CLASS (page)->fill_component != NULL);
	(* CLASS (page)->fill_component) (page, comp);
}

/**
 * editor_page_set_summary:
 * @page: An editor page.
 * @summary: Summary string to set in the page's widgets, which must be encoded
 * in UTF8.
 * 
 * Sets the calendar component summary string in an editor page.
 **/
void
editor_page_set_summary (EditorPage *page, const char *summary)
{
	g_return_if_fail (page != NULL);
	g_return_if_fail (IS_EDITOR_PAGE (page));
	g_return_if_fail (summary != NULL);

	g_assert (CLASS (page)->set_summary != NULL);
	(* CLASS (page)->set_summary) (page, summary);
}

/**
 * editor_page_get_summary:
 * @page: An editor page.
 * 
 * Queries the current summary string in an editor page.
 * 
 * Return value: Summary string in UTF8; must be freed by the caller.
 **/
char *
editor_page_get_summary (EditorPage *page)
{
	g_return_val_if_fail (page != NULL, NULL);
	g_return_val_if_fail (IS_EDITOR_PAGE (page), NULL);

	g_assert (CLASS (page)->get_summary != NULL);
	return (* CLASS (page)->get_summary) (page);
}

/**
 * editor_page_set_dtstart:
 * @page: An editor page.
 * @start: Start date for calendar component.
 * 
 * Sets the calendar component DTSTART in an editor page.
 **/
void
editor_page_set_dtstart (EditorPage *page, time_t start)
{
	g_return_if_fail (page != NULL);
	g_return_if_fail (IS_EDITOR_PAGE (page));
	g_return_if_fail (start != -1);

	g_assert (CLASS (page)->set_dtstart != NULL);
	(* CLASS (page)->set_dtstart) (page, start);
}

/**
 * editor_page_notify_changed:
 * @page: An editor page.
 * 
 * Makes an editor page emit the "changed" signal.  This is meant to be
 * used only by page implementations.
 **/
void
editor_page_notify_changed (EditorPage *page)
{
	g_return_if_fail (page != NULL);
	g_return_if_fail (IS_EDITOR_PAGE (page));

	gtk_signal_emit (GTK_OBJECT (page), editor_page_signals[CHANGED]);
}

/**
 * editor_page_notify_summary_changed:
 * @page: An editor page.
 * 
 * Makes an editor page emit the "summary_changed" signal.  This is meant to be
 * used only by page implementations.
 **/
void
editor_page_notify_summary_changed (EditorPage *page)
{
	g_return_if_fail (page != NULL);
	g_return_if_fail (IS_EDITOR_PAGE (page));

	gtk_signal_emit (GTK_OBJECT (page), editor_page_signals[SUMMARY_CHANGED]);
}
