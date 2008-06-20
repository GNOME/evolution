/* Evolution calendar - Base class for calendar component editor pages
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include "comp-editor-page.h"



static void comp_editor_page_class_init (CompEditorPageClass *class);
static void comp_editor_page_init (CompEditorPage *page);
static void comp_editor_page_dispose (GObject *object);

static gpointer parent_class;

/* Signal IDs */

enum {
	CHANGED,
	NEEDS_SEND,
	SUMMARY_CHANGED,
	DATES_CHANGED,
	CLIENT_CHANGED,
	FOCUS_IN,
	FOCUS_OUT,
	LAST_SIGNAL
};

static guint comp_editor_page_signals[LAST_SIGNAL];

#define CLASS(page) (COMP_EDITOR_PAGE_CLASS (G_OBJECT_GET_CLASS (page)))



/**
 * comp_editor_page_get_type:
 *
 * Registers the #CompEditorPage class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #CompEditorPage class.
 **/
GType
comp_editor_page_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (CompEditorPageClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) comp_editor_page_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (CompEditorPage),
			0,     /* n_preallocs */
			(GInstanceInitFunc) comp_editor_page_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			G_TYPE_OBJECT, "CompEditorPage", &type_info, 0);
	}

	return type;
}

/* Class initialization function for the abstract editor page */
static void
comp_editor_page_class_init (CompEditorPageClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = comp_editor_page_dispose;

	class->changed = NULL;
	class->summary_changed = NULL;
	class->dates_changed = NULL;

	class->get_widget = NULL;
	class->focus_main_widget = NULL;
	class->fill_widgets = NULL;
	class->fill_component = NULL;
	class->fill_timezones = NULL;
	class->set_summary = NULL;
	class->set_dates = NULL;

	comp_editor_page_signals[CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CompEditorPageClass, changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	comp_editor_page_signals[NEEDS_SEND] =
		g_signal_new ("needs_send",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CompEditorPageClass, needs_send),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	comp_editor_page_signals[SUMMARY_CHANGED] =
		g_signal_new ("summary_changed",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CompEditorPageClass, summary_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

	comp_editor_page_signals[DATES_CHANGED] =
		g_signal_new ("dates_changed",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CompEditorPageClass, dates_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

	comp_editor_page_signals[CLIENT_CHANGED] =
		g_signal_new ("client_changed",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CompEditorPageClass, client_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, G_TYPE_OBJECT);
	comp_editor_page_signals[FOCUS_IN] =
		g_signal_new ("focus_in",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CompEditorPageClass, focus_in),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);
	comp_editor_page_signals[FOCUS_OUT] =
		g_signal_new ("focus_out",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CompEditorPageClass, focus_out),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);
}



static void
comp_editor_page_init (CompEditorPage *page)
{
	page->client = NULL;
	page->accel_group = NULL;
}


static void
comp_editor_page_dispose (GObject *object)
{
	CompEditorPage *page;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_COMP_EDITOR_PAGE (object));

	page = COMP_EDITOR_PAGE (object);

	if (page->client) {
		g_object_unref (page->client);
		page->client = NULL;
	}

	if (page->accel_group) {
		g_object_unref (page->accel_group);
		page->accel_group = NULL;
	}

	G_OBJECT_CLASS (parent_class)->dispose (object);
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
	g_return_val_if_fail (CLASS (page)->get_widget != NULL, NULL);

	return (* CLASS (page)->get_widget) (page);
}

/**
 * comp_editor_page_focus_main_widget:
 * @page: An editor page.
 *
 * Makes an editor page focus its main widget.  This is used by the component
 * editor when it first pops up so that it can focus the main widget in the
 * first page.
 **/
void
comp_editor_page_focus_main_widget (CompEditorPage *page)
{
	g_return_if_fail (page != NULL);
	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));
	g_return_if_fail (CLASS (page)->focus_main_widget != NULL);

	(* CLASS (page)->focus_main_widget) (page);
}

/**
 * comp_editor_page_fill_widgets:
 * @page: An editor page.
 * @comp: A calendar component.
 *
 * Fills the widgets of an editor page with the data from a calendar component.
 **/
gboolean
comp_editor_page_fill_widgets (CompEditorPage *page, ECalComponent *comp)
{
	g_return_val_if_fail (IS_COMP_EDITOR_PAGE (page), FALSE);
	g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), FALSE);
	g_return_val_if_fail (CLASS (page)->fill_widgets != NULL, FALSE);

	return (* CLASS (page)->fill_widgets) (page, comp);
}

/**
 * comp_editor_page_fill_component:
 * @page: An editor page.
 * @comp: A calendar component.
 *
 * Takes the data from the widgets of an editor page and sets it on a calendar
 * component, replacing the contents of the properties that the editor page
 * knows how to manipulate.
 *
 * Returns: TRUE if the component could be filled, FALSE otherwise
 **/
gboolean
comp_editor_page_fill_component (CompEditorPage *page, ECalComponent *comp)
{
	g_return_val_if_fail (page != NULL, FALSE);
	g_return_val_if_fail (IS_COMP_EDITOR_PAGE (page), FALSE);
	g_return_val_if_fail (comp != NULL, FALSE);

	if (CLASS (page)->fill_component != NULL)
		return (* CLASS (page)->fill_component) (page, comp);

	return TRUE;
}

/**
 * comp_editor_page_fill_timezones:
 * @page: An editor page.
 * @timezones: Hash table to which timezones will be added.
 *
 * Fills the given hash table with all the timezones used by the dates in the
 * specific editor page.
 *
 * Returns: TRUE if the timezones were added, FALSE otherwise.
 */
gboolean
comp_editor_page_fill_timezones (CompEditorPage *page, GHashTable *timezones)
{
	g_return_val_if_fail (IS_COMP_EDITOR_PAGE (page), FALSE);
	g_return_val_if_fail (timezones != NULL, FALSE);

	if (CLASS (page)->fill_timezones != NULL)
		return (* CLASS (page)->fill_timezones) (page, timezones);

	return TRUE;
}

/**
 * comp_editor_page_set_e_cal:
 * @page: An editor page
 * @client: A #ECal object
 *
 * Sets the #ECal for the dialog page to use.
 **/
void
comp_editor_page_set_e_cal (CompEditorPage *page, ECal *client)
{
	g_return_if_fail (page != NULL);
        g_return_if_fail (IS_COMP_EDITOR_PAGE (page));

	if (client == page->client)
		return;

	if (page->client)
		g_object_unref (page->client);

	page->client = client;
	if (page->client)
		g_object_ref (client);
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
 * comp_editor_page_unset_focused_widget
 * @page: An editor page
 * @widget: The widget that has the current focus
**/
void
comp_editor_page_unset_focused_widget (CompEditorPage *page, GtkWidget *widget)
{
	g_return_if_fail (page!= NULL);
	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));

	g_signal_emit (page,
		       comp_editor_page_signals[FOCUS_OUT], 0,
		       widget);

}

/**
 * comp_editor_page_set_focussed_widget:
 * @page: An editor page
 * @widget: The widget that has the current focus
**/
void
comp_editor_page_set_focused_widget (CompEditorPage *page, GtkWidget *widget)
{
	g_return_if_fail (page!= NULL);
	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));

	g_signal_emit (page,
		       comp_editor_page_signals[FOCUS_IN], 0,
		       widget);

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

	g_signal_emit (page, comp_editor_page_signals[CHANGED], 0);
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

	g_signal_emit (page, comp_editor_page_signals[NEEDS_SEND], 0);
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


	g_signal_emit (page,
		       comp_editor_page_signals[SUMMARY_CHANGED], 0,
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

	g_signal_emit (page,
		       comp_editor_page_signals[DATES_CHANGED], 0,
		       dates);
}

/**
 * comp_editor_page_notify_client_changed:
 * @page: An editor page.
 *
 * Makes an editor page emit the "client_changed" signal.  This is meant to be
 * used only by page implementations.
 **/
void
comp_editor_page_notify_client_changed (CompEditorPage *page,
					ECal *client)
{
	g_return_if_fail (page != NULL);
	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));

	comp_editor_page_set_e_cal (page, client);
	g_signal_emit (page,
		       comp_editor_page_signals[CLIENT_CHANGED], 0,
		       client);
}

/**
 * comp_editor_page_display_validation_error:
 * @page: An editor page.
 * @msg: Error message to display.
 * @field: Widget that caused the validation error.
 *
 * Displays an error message about a validation problem in the
 * given field. Once the error message has been displayed, the
 * focus is set to the widget that caused the validation error.
 */
void
comp_editor_page_display_validation_error (CompEditorPage *page,
					   const char *msg,
					   GtkWidget *field)
{
	GtkWidget *dialog;

	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));
	g_return_if_fail (msg != NULL);
	g_return_if_fail (GTK_IS_WIDGET (field));

	dialog = gtk_message_dialog_new (
		NULL, 0,
		GTK_MESSAGE_ERROR,
		GTK_BUTTONS_CLOSE,
		_("Validation error: %s"), msg);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	gtk_widget_grab_focus (field);
}
