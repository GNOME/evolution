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
#include "meeting-page.h"
#include "cancel-comp.h"
#include "event-editor.h"

struct _EventEditorPrivate {
	EventPage *event_page;
	AlarmPage *alarm_page;
	RecurrencePage *recur_page;
	MeetingPage *meet_page;
	
	gboolean meeting_shown;
};



static void event_editor_class_init (EventEditorClass *class);
static void event_editor_init (EventEditor *ee);
static void event_editor_edit_comp (CompEditor *editor, CalComponent *comp);
static void event_editor_destroy (GtkObject *object);

static void schedule_meeting_cmd (GtkWidget *widget, gpointer data);
static void refresh_meeting_cmd (GtkWidget *widget, gpointer data);
static void cancel_meeting_cmd (GtkWidget *widget, gpointer data);
static void forward_cmd (GtkWidget *widget, gpointer data);

static BonoboUIVerb verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("ActionScheduleMeeting", schedule_meeting_cmd),
	BONOBO_UI_UNSAFE_VERB ("ActionRefreshMeeting", refresh_meeting_cmd),
	BONOBO_UI_UNSAFE_VERB ("ActionCancelMeeting", cancel_meeting_cmd),
	BONOBO_UI_UNSAFE_VERB ("ActionForward", forward_cmd),

	BONOBO_UI_VERB_END
};

static CompEditorClass *parent_class;



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
event_editor_class_init (EventEditorClass *klass)
{
	GtkObjectClass *object_class;
	CompEditorClass *editor_class;
	
	object_class = (GtkObjectClass *) klass;
	editor_class = (CompEditorClass *) klass;

	parent_class = gtk_type_class (TYPE_COMP_EDITOR);

	editor_class->edit_comp = event_editor_edit_comp;
	
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

	priv->meet_page = meeting_page_new ();
	comp_editor_append_page (COMP_EDITOR (ee),
				 COMP_EDITOR_PAGE (priv->meet_page),
				 _("Meeting"));

	priv->meeting_shown = TRUE;
	
	comp_editor_merge_ui (COMP_EDITOR (ee), EVOLUTION_DATADIR 
			      "/gnome/ui/evolution-event-editor.xml",
			      verbs);
}

static void
event_editor_edit_comp (CompEditor *editor, CalComponent *comp)
{
	EventEditor *ee;
	EventEditorPrivate *priv;
	GSList *attendees = NULL;
	
	ee = EVENT_EDITOR (editor);
	priv = ee->priv;
	
	cal_component_get_attendee_list (comp, &attendees);
	if (attendees == NULL) {
		comp_editor_remove_page (editor, COMP_EDITOR_PAGE (priv->meet_page));
		priv->meeting_shown = FALSE;
	}
		
	if (parent_class->edit_comp)
		parent_class->edit_comp (editor, comp);
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

static void
schedule_meeting_cmd (GtkWidget *widget, gpointer data)
{
	EventEditor *ee = EVENT_EDITOR (data);
	EventEditorPrivate *priv;

	priv = ee->priv;

	if (!priv->meeting_shown) {
		comp_editor_append_page (COMP_EDITOR (ee),
					 COMP_EDITOR_PAGE (priv->meet_page),
					 _("Meeting"));
		priv->meeting_shown = FALSE;
	}
	
	comp_editor_show_page (COMP_EDITOR (ee),
			       COMP_EDITOR_PAGE (priv->meet_page));
}

static void
refresh_meeting_cmd (GtkWidget *widget, gpointer data)
{
	EventEditor *ee = EVENT_EDITOR (data);
	
	comp_editor_send_comp (COMP_EDITOR (ee), CAL_COMPONENT_METHOD_REFRESH);
}

static void
cancel_meeting_cmd (GtkWidget *widget, gpointer data)
{
	EventEditor *ee = EVENT_EDITOR (data);
	CalComponent *comp;
	
	comp = comp_editor_get_current_comp (COMP_EDITOR (ee));
	if (cancel_component_dialog (comp)) {
		comp_editor_send_comp (COMP_EDITOR (ee), CAL_COMPONENT_METHOD_CANCEL);
		comp_editor_delete_comp (COMP_EDITOR (ee));
	}
}

static void
forward_cmd (GtkWidget *widget, gpointer data)
{
	EventEditor *ee = EVENT_EDITOR (data);
	
	comp_editor_send_comp (COMP_EDITOR (ee), CAL_COMPONENT_METHOD_PUBLISH);
}

