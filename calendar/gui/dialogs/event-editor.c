/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Evolution calendar - Event editor dialog
 *
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Miguel de Icaza <miguel@ximian.com>
 *          Federico Mena-Quintero <federico@ximian.com>
 *          Seth Alves <alves@hungry.com>
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

#include <config.h>
#include <string.h>
#include <glade/glade.h>
#include <gal/widgets/e-unicode.h>
#include <libgnome/gnome-i18n.h>
#include <widgets/misc/e-dateedit.h>

#include "event-page.h"
#include "alarm-page.h"
#include "recurrence-page.h"
#include "event-editor.h"

struct _EventEditorPrivate {
	EventPage *event_page;
	AlarmPage *alarm_page;
	RecurrencePage *recur_page;
};



static void event_editor_class_init (EventEditorClass *class);
static void event_editor_init (EventEditor *ee);
static void event_editor_destroy (GtkObject *object);

static CompEditor *parent_class;



/**
 * event_editor_get_type:
 *
 * Registers the #EventEditor class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #EventEditor class.
 **/
GtkType
event_editor_get_type (void)
{
	static GtkType event_editor_type = 0;

	if (!event_editor_type) {
		static const GtkTypeInfo event_editor_info = {
			"EventEditor",
			sizeof (EventEditor),
			sizeof (EventEditorClass),
			(GtkClassInitFunc) event_editor_class_init,
			(GtkObjectInitFunc) event_editor_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		event_editor_type = gtk_type_unique (TYPE_COMP_EDITOR,
						     &event_editor_info);
	}

	return event_editor_type;
}

/* Class initialization function for the event editor */
static void
event_editor_class_init (EventEditorClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (TYPE_COMP_EDITOR);

	object_class->destroy = event_editor_destroy;
}

/* Object initialization function for the event editor */
static void
event_editor_init (EventEditor *ee)
{
	EventEditorPrivate *priv;
	
	priv = g_new0 (EventEditorPrivate, 1);
	ee->priv = priv;

	priv->event_page = event_page_new ();
	comp_editor_append_page (COMP_EDITOR (ee), 
				 COMP_EDITOR_PAGE (priv->event_page),
				 _("Appointment"));

	priv->alarm_page = alarm_page_new ();
	comp_editor_append_page (COMP_EDITOR (ee),
				 COMP_EDITOR_PAGE (priv->alarm_page),
				 _("Reminder"));

	priv->recur_page = recurrence_page_new ();
	comp_editor_append_page (COMP_EDITOR (ee),
				 COMP_EDITOR_PAGE (priv->recur_page),
				 _("Recurrence"));
	
}

/* Destroy handler for the event editor */
static void
event_editor_destroy (GtkObject *object)
{
	EventEditor *ee;
	EventEditorPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_EVENT_EDITOR (object));

	ee = EVENT_EDITOR (object);
	priv = ee->priv;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/**
 * event_editor_new:
 *
 * Creates a new event editor dialog.
 *
 * Return value: A newly-created event editor dialog, or NULL if the event
 * editor could not be created.
 **/
EventEditor *
event_editor_new (void)
{
	return EVENT_EDITOR (gtk_type_new (TYPE_EVENT_EDITOR));
}
