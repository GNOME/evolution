/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Evolution calendar - Event editor dialog
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Miguel de Icaza <miguel@ximian.com>
 *          Federico Mena-Quintero <federico@ximian.com>
 *          Seth Alves <alves@hungry.com>
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
#include "schedule-page.h"
#include "cancel-comp.h"
#include "event-editor.h"

struct _EventEditorPrivate {
	EventPage *event_page;
	AlarmPage *alarm_page;
	RecurrencePage *recur_page;
	MeetingPage *meet_page;
	SchedulePage *sched_page;

	EMeetingModel *model;
	
	gboolean meeting_shown;
	gboolean updating;	
};



static void event_editor_class_init (EventEditorClass *class);
static void event_editor_init (EventEditor *ee);
static void event_editor_set_cal_client (CompEditor *editor, CalClient *client);
static void event_editor_edit_comp (CompEditor *editor, CalComponent *comp);
static gboolean event_editor_send_comp (CompEditor *editor, CalComponentItipMethod method);
static void event_editor_destroy (GtkObject *object);

static void schedule_meeting_cmd (GtkWidget *widget, gpointer data);
static void refresh_meeting_cmd (GtkWidget *widget, gpointer data);
static void cancel_meeting_cmd (GtkWidget *widget, gpointer data);
static void forward_cmd (GtkWidget *widget, gpointer data);

static void model_row_changed_cb (ETableModel *etm, int row, gpointer data);
static void row_count_changed_cb (ETableModel *etm, int row, int count, gpointer data);

static EPixmap pixmaps [] = {
	E_PIXMAP ("/Toolbar/Actions/ActionScheduleMeeting", "schedule-meeting-24.png"),
	E_PIXMAP_END
};

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

	editor_class->set_cal_client = event_editor_set_cal_client;
	editor_class->edit_comp = event_editor_edit_comp;
	editor_class->send_comp = event_editor_send_comp;
	
	object_class->destroy = event_editor_destroy;
}

static void
set_menu_sens (EventEditor *ee) 
{
	EventEditorPrivate *priv;
	gboolean sens, existing, user, read_only;
	
	priv = ee->priv;

	existing = comp_editor_get_existing_org (COMP_EDITOR (ee));
	user = comp_editor_get_user_org (COMP_EDITOR (ee));
	read_only = cal_client_is_read_only (comp_editor_get_cal_client (COMP_EDITOR (ee)));

	sens = priv->meeting_shown;
	comp_editor_set_ui_prop (COMP_EDITOR (ee), 
				 "/commands/ActionScheduleMeeting", 
				 "sensitive", sens || read_only ? "0" : "1");

	sens = priv->meeting_shown && existing && !user && !read_only;
	comp_editor_set_ui_prop (COMP_EDITOR (ee), 
				 "/commands/ActionRefreshMeeting", 
				 "sensitive", sens ? "1" : "0");

	sens = priv->meeting_shown && existing && user && !read_only;
	comp_editor_set_ui_prop (COMP_EDITOR (ee), 
				 "/commands/ActionCancelMeeting", 
				 "sensitive", sens? "1" : "0");

	comp_editor_set_ui_prop (COMP_EDITOR (ee),
				 "/commands/FileSave",
				 "sensitive", read_only ? "0" : "1");
	comp_editor_set_ui_prop (COMP_EDITOR (ee),
				 "/commands/FileSaveAndClose",
				 "sensitive", read_only ? "0" : "1");
	comp_editor_set_ui_prop (COMP_EDITOR (ee),
				 "/commands/FileDelete",
				 "sensitive", read_only ? "0" : "1");
}

static void
init_widgets (EventEditor *ee)
{
	EventEditorPrivate *priv;

	priv = ee->priv;

	gtk_signal_connect (GTK_OBJECT (priv->model), "model_row_changed",
			    GTK_SIGNAL_FUNC (model_row_changed_cb), ee);
	gtk_signal_connect (GTK_OBJECT (priv->model), "model_rows_inserted",
			    GTK_SIGNAL_FUNC (row_count_changed_cb), ee);
	gtk_signal_connect (GTK_OBJECT (priv->model), "model_rows_deleted",
			    GTK_SIGNAL_FUNC (row_count_changed_cb), ee);
}

/* Object initialization function for the event editor */
static void
event_editor_init (EventEditor *ee)
{
	EventEditorPrivate *priv;
	
	priv = g_new0 (EventEditorPrivate, 1);
	ee->priv = priv;

	priv->model = E_MEETING_MODEL (e_meeting_model_new ());
	priv->meeting_shown = TRUE;
	priv->updating = FALSE;	
}

EventEditor *
event_editor_construct (EventEditor *ee, CalClient *client)
{
	EventEditorPrivate *priv;

	priv = ee->priv;

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
	
	priv->sched_page = schedule_page_new (priv->model);
	comp_editor_append_page (COMP_EDITOR (ee),
				 COMP_EDITOR_PAGE (priv->sched_page),
				 _("Scheduling"));

	priv->meet_page = meeting_page_new (priv->model, client);
	comp_editor_append_page (COMP_EDITOR (ee),
				 COMP_EDITOR_PAGE (priv->meet_page),
				 _("Meeting"));

	comp_editor_set_cal_client (COMP_EDITOR (ee), client);

 	comp_editor_merge_ui (COMP_EDITOR (ee), "evolution-event-editor.xml", verbs, pixmaps);

	init_widgets (ee);
	set_menu_sens (ee);	

	return ee;
}

static void
event_editor_set_cal_client (CompEditor *editor, CalClient *client)
{
	EventEditor *ee;
	EventEditorPrivate *priv;
	
	ee = EVENT_EDITOR (editor);
	priv = ee->priv;

	e_meeting_model_set_cal_client (priv->model, client);
	
	if (parent_class->set_cal_client)
		parent_class->set_cal_client (editor, client);	
}

static void
event_editor_edit_comp (CompEditor *editor, CalComponent *comp)
{
	EventEditor *ee;
	EventEditorPrivate *priv;
	CalComponentOrganizer organizer;
	CalClient *client;
	GSList *attendees = NULL;
	
	ee = EVENT_EDITOR (editor);
	priv = ee->priv;
	
	priv->updating = TRUE;
	
	if (parent_class->edit_comp)
		parent_class->edit_comp (editor, comp);

	client = comp_editor_get_cal_client (COMP_EDITOR (editor));

	/* Get meeting related stuff */
	cal_component_get_organizer (comp, &organizer);
	cal_component_get_attendee_list (comp, &attendees);

	/* Clear things up */
	e_meeting_model_remove_all_attendees (priv->model);

	/* Set up the attendees */
	if (attendees == NULL) {
		comp_editor_remove_page (editor, COMP_EDITOR_PAGE (priv->meet_page));
		comp_editor_remove_page (editor, COMP_EDITOR_PAGE (priv->sched_page));
		priv->meeting_shown = FALSE;
	} else {
		GSList *l;
		GList *addresses, *ll;
		int row;

		if (!priv->meeting_shown) {
			comp_editor_append_page (COMP_EDITOR (ee),
						 COMP_EDITOR_PAGE (priv->sched_page),
						 _("Scheduling"));
			comp_editor_append_page (COMP_EDITOR (ee),
						 COMP_EDITOR_PAGE (priv->meet_page),
						 _("Meeting"));
		}
	
		for (l = attendees; l != NULL; l = l->next) {
			CalComponentAttendee *ca = l->data;
			EMeetingAttendee *ia;

			ia = E_MEETING_ATTENDEE (e_meeting_attendee_new_from_cal_component_attendee (ca));
			if (!comp_editor_get_user_org (editor))
				e_meeting_attendee_set_edit_level (ia,  E_MEETING_ATTENDEE_EDIT_NONE);
			e_meeting_model_add_attendee (priv->model, ia);
			
			gtk_object_unref (GTK_OBJECT (ia));
		}

		/* If we aren't the organizer we can still change our own status */
		if (!comp_editor_get_user_org (editor)) {
			addresses = itip_addresses_get ();
			for (ll = addresses; ll != NULL; ll = ll->next) {
				EMeetingAttendee *ia;
				ItipAddress *a = ll->data;
				
				ia = e_meeting_model_find_attendee (priv->model, a->address, &row);
				if (ia != NULL)
					e_meeting_attendee_set_edit_level (ia, E_MEETING_ATTENDEE_EDIT_STATUS);
			}
			itip_addresses_free (addresses);
		} else if (cal_client_get_organizer_must_attend (client)) {
			EMeetingAttendee *ia;

			ia = e_meeting_model_find_attendee (priv->model, organizer.value, &row);
			if (ia != NULL)
				e_meeting_attendee_set_edit_level (ia, E_MEETING_ATTENDEE_EDIT_NONE);
		}
		
		priv->meeting_shown = TRUE;
	}	
	cal_component_free_attendee_list (attendees);

	set_menu_sens (ee);
	comp_editor_set_needs_send (COMP_EDITOR (ee), priv->meeting_shown && itip_organizer_is_user (comp, client));
	
	priv->updating = FALSE;
}

static gboolean
event_editor_send_comp (CompEditor *editor, CalComponentItipMethod method)
{
	EventEditor *ee = EVENT_EDITOR (editor);
	EventEditorPrivate *priv;
	CalComponent *comp = NULL;

	priv = ee->priv;

	/* Don't cancel more than once or when just publishing */
	if (method == CAL_COMPONENT_METHOD_PUBLISH ||
	    method == CAL_COMPONENT_METHOD_CANCEL)
		goto parent;
	
	comp = meeting_page_get_cancel_comp (priv->meet_page);
	if (comp != NULL) {
		CalClient *client;
		gboolean result;
		
		client = e_meeting_model_get_cal_client (priv->model);
		result = itip_send_comp (CAL_COMPONENT_METHOD_CANCEL, comp, client, NULL);
		gtk_object_unref (GTK_OBJECT (comp));

		if (!result)
			return FALSE;
	}

 parent:
	if (parent_class->send_comp)
		return parent_class->send_comp (editor, method);

	return FALSE;
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

	gtk_object_unref (GTK_OBJECT (priv->event_page));
	gtk_object_unref (GTK_OBJECT (priv->alarm_page));
	gtk_object_unref (GTK_OBJECT (priv->recur_page));
	gtk_object_unref (GTK_OBJECT (priv->meet_page));
	gtk_object_unref (GTK_OBJECT (priv->sched_page));

	gtk_object_destroy (GTK_OBJECT (priv->model));
	gtk_object_unref (GTK_OBJECT (priv->model));

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/**
 * event_editor_new:
 * @client: a CalClient
 *
 * Creates a new event editor dialog.
 *
 * Return value: A newly-created event editor dialog, or NULL if the event
 * editor could not be created.
 **/
EventEditor *
event_editor_new (CalClient *client)
{
	EventEditor *ee;

	ee = EVENT_EDITOR (gtk_type_new (TYPE_EVENT_EDITOR));
	return event_editor_construct (ee, client);
}

static void
show_meeting (EventEditor *ee)
{
	EventEditorPrivate *priv;

	priv = ee->priv;

	if (!priv->meeting_shown) {
		comp_editor_append_page (COMP_EDITOR (ee),
					 COMP_EDITOR_PAGE (priv->sched_page),
					 _("Scheduling"));
		comp_editor_append_page (COMP_EDITOR (ee),
					 COMP_EDITOR_PAGE (priv->meet_page),
					 _("Meeting"));
		priv->meeting_shown = TRUE;

		set_menu_sens (ee);
 		comp_editor_set_changed (COMP_EDITOR (ee), priv->meeting_shown);
		comp_editor_set_needs_send (COMP_EDITOR (ee), priv->meeting_shown);
	}
	
	comp_editor_show_page (COMP_EDITOR (ee),
			       COMP_EDITOR_PAGE (priv->meet_page));
}

void
event_editor_show_meeting (EventEditor *ee)
{
	g_return_if_fail (ee != NULL);
	g_return_if_fail (IS_EVENT_EDITOR (ee));


	show_meeting (ee);
}

static void
schedule_meeting_cmd (GtkWidget *widget, gpointer data)
{
	EventEditor *ee = EVENT_EDITOR (data);

	show_meeting (ee);
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
	if (cancel_component_dialog (comp_editor_get_cal_client (COMP_EDITOR (ee)), comp, FALSE)) {
		comp_editor_send_comp (COMP_EDITOR (ee), CAL_COMPONENT_METHOD_CANCEL);
		comp_editor_delete_comp (COMP_EDITOR (ee));
	}
}

static void
forward_cmd (GtkWidget *widget, gpointer data)
{
	EventEditor *ee = EVENT_EDITOR (data);

	if (comp_editor_save_comp (COMP_EDITOR (ee), TRUE))
		comp_editor_send_comp (COMP_EDITOR (ee), CAL_COMPONENT_METHOD_PUBLISH);
}

static void
model_row_changed_cb (ETableModel *etm, int row, gpointer data)
{
	EventEditor *ee = EVENT_EDITOR (data);
	EventEditorPrivate *priv;
	
	priv = ee->priv;
	
	if (!priv->updating) {
		comp_editor_set_changed (COMP_EDITOR (ee), TRUE);
		comp_editor_set_needs_send (COMP_EDITOR (ee), TRUE);
	}
}

static void
row_count_changed_cb (ETableModel *etm, int row, int count, gpointer data)
{
	EventEditor *ee = EVENT_EDITOR (data);
	EventEditorPrivate *priv;
	
	priv = ee->priv;
	
	if (!priv->updating) {
		comp_editor_set_changed (COMP_EDITOR (ee), TRUE);
		comp_editor_set_needs_send (COMP_EDITOR (ee), TRUE);
	}
}
