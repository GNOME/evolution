/*
 *
 * Evolution calendar - Base class for calendar component editor pages
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include "comp-editor.h"
#include "comp-editor-page.h"

#define COMP_EDITOR_PAGE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), TYPE_COMP_EDITOR_PAGE, CompEditorPagePrivate))

struct _CompEditorPagePrivate {
	CompEditor *editor;  /* not referenced */
	gboolean updating;
};

enum {
	PROP_0,
	PROP_EDITOR,
	PROP_UPDATING
};

enum {
	DATES_CHANGED,
	LAST_SIGNAL
};

static gpointer parent_class;
static guint comp_editor_page_signals[LAST_SIGNAL];

static void
comp_editor_page_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
	CompEditorPagePrivate *priv;

	priv = COMP_EDITOR_PAGE_GET_PRIVATE (object);

	switch (property_id) {
		case PROP_EDITOR:
			priv->editor = g_value_get_object (value);
			return;

		case PROP_UPDATING:
			comp_editor_page_set_updating (
				COMP_EDITOR_PAGE (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
comp_editor_page_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EDITOR:
			g_value_set_object (
				value, comp_editor_page_get_editor (
				COMP_EDITOR_PAGE (object)));
			return;

		case PROP_UPDATING:
			g_value_set_boolean (
				value, comp_editor_page_get_updating (
				COMP_EDITOR_PAGE (object)));
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
comp_editor_page_dispose (GObject *object)
{
	CompEditorPage *page;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_COMP_EDITOR_PAGE (object));

	page = COMP_EDITOR_PAGE (object);

	if (page->accel_group) {
		g_object_unref (page->accel_group);
		page->accel_group = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
comp_editor_page_class_init (CompEditorPageClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (CompEditorPagePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = comp_editor_page_set_property;
	object_class->get_property = comp_editor_page_get_property;
	object_class->dispose = comp_editor_page_dispose;

	class->dates_changed = NULL;

	class->get_widget = NULL;
	class->focus_main_widget = NULL;
	class->fill_widgets = NULL;
	class->fill_component = NULL;
	class->fill_timezones = NULL;
	class->set_dates = NULL;

	g_object_class_install_property (
		object_class,
		PROP_EDITOR,
		g_param_spec_object (
			"editor",
			NULL,
			NULL,
			TYPE_COMP_EDITOR,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_UPDATING,
		g_param_spec_boolean (
			"updating",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	comp_editor_page_signals[DATES_CHANGED] =
		g_signal_new ("dates_changed",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CompEditorPageClass, dates_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void
comp_editor_page_init (CompEditorPage *page)
{
	page->priv = COMP_EDITOR_PAGE_GET_PRIVATE (page);

	page->accel_group = NULL;
}

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

/**
 * comp_editor_page_get_editor:
 * @page: a #CompEditorPage
 *
 * Returns the #CompEditor to which @page belongs.
 *
 * Returns: the parent #CompEditor
 **/
CompEditor *
comp_editor_page_get_editor (CompEditorPage *page)
{
	g_return_val_if_fail (IS_COMP_EDITOR_PAGE (page), NULL);

	return page->priv->editor;
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
	CompEditorPageClass *class;

	g_return_val_if_fail (IS_COMP_EDITOR_PAGE (page), NULL);

	class = COMP_EDITOR_PAGE_GET_CLASS (page);
	g_return_val_if_fail (class->get_widget != NULL, NULL);

	return class->get_widget (page);
}

gboolean
comp_editor_page_get_updating (CompEditorPage *page)
{
	g_return_val_if_fail (IS_COMP_EDITOR_PAGE (page), FALSE);

	return page->priv->updating;
}

void
comp_editor_page_set_updating (CompEditorPage *page,
                               gboolean updating)
{
	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));

	page->priv->updating = updating;

	g_object_notify (G_OBJECT (page), "updating");
}

void
comp_editor_page_changed (CompEditorPage *page)
{
	CompEditor *editor;

	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));

	/* Block change notifications if the page is updating.  This right
	 * here is why we have an 'updating' flag.  It's up to subclasses
	 * to set and clear it at appropriate times. */
	if (page->priv->updating)
		return;

	editor = comp_editor_page_get_editor (page);
	comp_editor_set_changed (editor, TRUE);
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
	CompEditorPageClass *class;

	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));

	class = COMP_EDITOR_PAGE_GET_CLASS (page);
	g_return_if_fail (class->focus_main_widget != NULL);

	class->focus_main_widget (page);
}

/**
 * comp_editor_page_fill_widgets:
 * @page: An editor page.
 * @comp: A calendar component.
 *
 * Fills the widgets of an editor page with the data from a calendar component.
 **/
gboolean
comp_editor_page_fill_widgets (CompEditorPage *page,
                               ECalComponent *comp)
{
	CompEditorPageClass *class;
	gboolean success;

	g_return_val_if_fail (IS_COMP_EDITOR_PAGE (page), FALSE);
	g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), FALSE);

	class = COMP_EDITOR_PAGE_GET_CLASS (page);
	g_return_val_if_fail (class->fill_widgets != NULL, FALSE);

	comp_editor_page_set_updating (page, TRUE);
	success = class->fill_widgets (page, comp);
	comp_editor_page_set_updating (page, FALSE);

	return success;
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
	CompEditorPageClass *class;

	g_return_val_if_fail (IS_COMP_EDITOR_PAGE (page), FALSE);
	g_return_val_if_fail (comp != NULL, FALSE);

	class = COMP_EDITOR_PAGE_GET_CLASS (page);

	if (class->fill_component != NULL)
		return class->fill_component (page, comp);

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
	CompEditorPageClass *class;

	g_return_val_if_fail (IS_COMP_EDITOR_PAGE (page), FALSE);
	g_return_val_if_fail (timezones != NULL, FALSE);

	class = COMP_EDITOR_PAGE_GET_CLASS (page);

	if (class->fill_timezones != NULL)
		return class->fill_timezones (page, timezones);

	return TRUE;
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
	CompEditorPageClass *class;

	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));

	class = COMP_EDITOR_PAGE_GET_CLASS (page);

	if (class->set_dates != NULL)
		class->set_dates (page, dates);
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
	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));

	g_signal_emit (page,
		       comp_editor_page_signals[DATES_CHANGED], 0,
		       dates);
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
					   const gchar *msg,
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
