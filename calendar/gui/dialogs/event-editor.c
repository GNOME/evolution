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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glade/glade.h>
#include <libgnome/gnome-i18n.h>
#include <widgets/misc/e-dateedit.h>
#include <e-util/e-icon-factory.h>

#include "event-page.h"
#include "recurrence-page.h"
#include "meeting-page.h"
#include "schedule-page.h"
#include "cancel-comp.h"
#include "event-editor.h"

struct _EventEditorPrivate {
	EventPage *event_page;
	RecurrencePage *recur_page;
	MeetingPage *meet_page;
	SchedulePage *sched_page;

	EMeetingStore *model;
	gboolean is_meeting;
	gboolean meeting_shown;
	gboolean updating;	
};



static void event_editor_set_e_cal (CompEditor *editor, ECal *client);
static void event_editor_edit_comp (CompEditor *editor, ECalComponent *comp);
static gboolean event_editor_send_comp (CompEditor *editor, ECalComponentItipMethod method);
static void event_editor_finalize (GObject *object);

static void schedule_meeting_cmd (GtkWidget *widget, gpointer data);
static void refresh_meeting_cmd (GtkWidget *widget, gpointer data);
static void cancel_meeting_cmd (GtkWidget *widget, gpointer data);
static void forward_cmd (GtkWidget *widget, gpointer data);

static void model_row_change_insert_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data);
static void model_row_delete_cb (GtkTreeModel *model, GtkTreePath *path, gpointer data);


G_DEFINE_TYPE (EventEditor, event_editor, TYPE_COMP_EDITOR);

/* Class initialization function for the event editor */
static void
event_editor_class_init (EventEditorClass *klass)
{
	GObjectClass *gobject_class;
	CompEditorClass *editor_class;
	
	gobject_class = (GObjectClass *) klass;
	editor_class = (CompEditorClass *) klass;

	editor_class->set_e_cal = event_editor_set_e_cal;
	editor_class->edit_comp = event_editor_edit_comp;
	editor_class->send_comp = event_editor_send_comp;
	
	gobject_class->finalize = event_editor_finalize;
}

static void
init_widgets (EventEditor *ee)
{
	EventEditorPrivate *priv;

	priv = ee->priv;

	g_signal_connect((priv->model), "row_changed",
			    G_CALLBACK (model_row_change_insert_cb), ee);
	g_signal_connect((priv->model), "row_inserted",
			    G_CALLBACK (model_row_change_insert_cb), ee);
	g_signal_connect((priv->model), "row_deleted",
			    G_CALLBACK (model_row_delete_cb), ee);
}

static void
client_changed_cb (CompEditorPage *page, ECal *client, gpointer user_data)
{
	//set_menu_sens (EVENT_EDITOR (user_data));
}

/* Object initialization function for the event editor */
static void
event_editor_init (EventEditor *ee)
{
	EventEditorPrivate *priv;
	
	priv = g_new0 (EventEditorPrivate, 1);
	ee->priv = priv;

	priv->model = E_MEETING_STORE (e_meeting_store_new ());
	priv->meeting_shown = TRUE;
	priv->updating = FALSE;	
}

EventEditor *
event_editor_construct (EventEditor *ee, ECal *client)
{
	EventEditorPrivate *priv;

	priv = ee->priv;

	priv->event_page = event_page_new ();
	g_object_ref (priv->event_page);
	gtk_object_sink (GTK_OBJECT (priv->event_page));
	comp_editor_append_page (COMP_EDITOR (ee), 
				 COMP_EDITOR_PAGE (priv->event_page),
				 _("Appointment"));
	g_signal_connect (G_OBJECT (priv->event_page), "client_changed",
			  G_CALLBACK (client_changed_cb), ee);

	priv->recur_page = recurrence_page_new ();
	g_object_ref (priv->recur_page);
	gtk_object_sink (GTK_OBJECT (priv->recur_page));
	comp_editor_append_page (COMP_EDITOR (ee),
				 COMP_EDITOR_PAGE (priv->recur_page),
				 _("Recurrence"));
	
	priv->sched_page = schedule_page_new (priv->model);
	g_object_ref (priv->sched_page);
	gtk_object_sink (GTK_OBJECT (priv->sched_page));
	comp_editor_append_page (COMP_EDITOR (ee),
				 COMP_EDITOR_PAGE (priv->sched_page),
				 _("Scheduling"));

	priv->meet_page = meeting_page_new (priv->model, client);
	g_object_ref (priv->meet_page);
	gtk_object_sink (GTK_OBJECT (priv->meet_page));
	comp_editor_append_page (COMP_EDITOR (ee),
				 COMP_EDITOR_PAGE (priv->meet_page),
				 _("Invitations"));

	comp_editor_set_e_cal (COMP_EDITOR (ee), client);

	init_widgets (ee);
	gtk_window_set_default_size (GTK_WINDOW (ee), 300, 225);

	return ee;
}

static void
event_editor_set_e_cal (CompEditor *editor, ECal *client)
{
	EventEditor *ee;
	EventEditorPrivate *priv;
	
	ee = EVENT_EDITOR (editor);
	priv = ee->priv;

	e_meeting_store_set_e_cal (priv->model, client);
	
	if (COMP_EDITOR_CLASS (event_editor_parent_class)->set_e_cal)
		COMP_EDITOR_CLASS (event_editor_parent_class)->set_e_cal (editor, client);	
}

static void
event_editor_edit_comp (CompEditor *editor, ECalComponent *comp)
{
	EventEditor *ee;
	EventEditorPrivate *priv;
	ECalComponentOrganizer organizer;
	ECal *client;
	GSList *attendees = NULL;
	
	ee = EVENT_EDITOR (editor);
	priv = ee->priv;
	
	priv->updating = TRUE;
	
	if (COMP_EDITOR_CLASS (event_editor_parent_class)->edit_comp)
		COMP_EDITOR_CLASS (event_editor_parent_class)->edit_comp (editor, comp);

	client = comp_editor_get_e_cal (COMP_EDITOR (editor));

	/* Get meeting related stuff */
	e_cal_component_get_organizer (comp, &organizer);
	e_cal_component_get_attendee_list (comp, &attendees);

	/* Set up the attendees */
	if (attendees == NULL && !priv->is_meeting) {
		comp_editor_remove_page (editor, COMP_EDITOR_PAGE (priv->meet_page));
		comp_editor_remove_page (editor, COMP_EDITOR_PAGE (priv->sched_page));
		priv->meeting_shown = FALSE;
	} else {
		GSList *l;
		int row;

		if (!priv->meeting_shown) {
			comp_editor_append_page (COMP_EDITOR (ee),
						 COMP_EDITOR_PAGE (priv->sched_page),
						 _("Scheduling"));
			comp_editor_append_page (COMP_EDITOR (ee),
						 COMP_EDITOR_PAGE (priv->meet_page),
						 _("Invitations"));
		}
	
		for (l = attendees; l != NULL; l = l->next) {
			ECalComponentAttendee *ca = l->data;
			EMeetingAttendee *ia;

			ia = E_MEETING_ATTENDEE (e_meeting_attendee_new_from_e_cal_component_attendee (ca));

			/* If we aren't the organizer or the attendee is just delegating, don't allow editing */
			if (!comp_editor_get_user_org (editor) || e_meeting_attendee_is_set_delto (ia))
				e_meeting_attendee_set_edit_level (ia,  E_MEETING_ATTENDEE_EDIT_NONE);
			e_meeting_store_add_attendee (priv->model, ia);

			g_object_unref(ia);
		}

		/* If we aren't the organizer we can still change our own status */
		if (!comp_editor_get_user_org (editor)) {
			EAccountList *accounts;
			EAccount *account;
			EIterator *it;

			accounts = itip_addresses_get ();
			for (it = e_list_get_iterator((EList *)accounts);e_iterator_is_valid(it);e_iterator_next(it)) {
				EMeetingAttendee *ia;

				account = (EAccount*)e_iterator_get(it);

				ia = e_meeting_store_find_attendee (priv->model, account->id->address, &row);
				if (ia != NULL)
					e_meeting_attendee_set_edit_level (ia, E_MEETING_ATTENDEE_EDIT_STATUS);
			}
			g_object_unref(it);
		} else if (e_cal_get_organizer_must_attend (client)) {
			EMeetingAttendee *ia;

			ia = e_meeting_store_find_attendee (priv->model, organizer.value, &row);
			if (ia != NULL)
				e_meeting_attendee_set_edit_level (ia, E_MEETING_ATTENDEE_EDIT_NONE);
		}
		
		priv->meeting_shown = TRUE;
	}	
	e_cal_component_free_attendee_list (attendees);

	comp_editor_set_needs_send (COMP_EDITOR (ee), priv->meeting_shown && itip_organizer_is_user (comp, client));
	
	priv->updating = FALSE;
}

static gboolean
event_editor_send_comp (CompEditor *editor, ECalComponentItipMethod method)
{
	EventEditor *ee = EVENT_EDITOR (editor);
	EventEditorPrivate *priv;
	ECalComponent *comp = NULL;

	priv = ee->priv;

	/* Don't cancel more than once or when just publishing */
	if (method == E_CAL_COMPONENT_METHOD_PUBLISH ||
	    method == E_CAL_COMPONENT_METHOD_CANCEL)
		goto parent;
	
	comp = meeting_page_get_cancel_comp (priv->meet_page);
	if (comp != NULL) {
		ECal *client;
		gboolean result;
		
		client = e_meeting_store_get_e_cal (priv->model);
		result = itip_send_comp (E_CAL_COMPONENT_METHOD_CANCEL, comp, client, NULL);
		g_object_unref (comp);

		if (!result)
			return FALSE;
	}

 parent:
	if (COMP_EDITOR_CLASS (event_editor_parent_class)->send_comp)
		return COMP_EDITOR_CLASS (event_editor_parent_class)->send_comp (editor, method);

	return FALSE;
}

/* Destroy handler for the event editor */
static void
event_editor_finalize (GObject *object)
{
	EventEditor *ee;
	EventEditorPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_EVENT_EDITOR (object));

	ee = EVENT_EDITOR (object);
	priv = ee->priv;

	g_object_unref (priv->event_page);
	g_object_unref (priv->recur_page);
	g_object_unref (priv->meet_page);
	g_object_unref (priv->sched_page);

	g_object_unref (priv->model);

	g_free (priv);

	if (G_OBJECT_CLASS (event_editor_parent_class)->finalize)
		(* G_OBJECT_CLASS (event_editor_parent_class)->finalize) (object);
}

/**
 * event_editor_new:
 * @client: a ECal
 *
 * Creates a new event editor dialog.
 *
 * Return value: A newly-created event editor dialog, or NULL if the event
 * editor could not be created.
 **/
EventEditor *
event_editor_new (ECal *client, gboolean is_meeting)
{
	EventEditor *ee;

	ee = EVENT_EDITOR (g_object_new (TYPE_EVENT_EDITOR, NULL));
	ee->priv->is_meeting = is_meeting;
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
					 _("Invitations"));
		priv->meeting_shown = TRUE;

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
	
	comp_editor_send_comp (COMP_EDITOR (ee), E_CAL_COMPONENT_METHOD_REFRESH);
}

static void
cancel_meeting_cmd (GtkWidget *widget, gpointer data)
{
	EventEditor *ee = EVENT_EDITOR (data);
	ECalComponent *comp;
	
	comp = comp_editor_get_current_comp (COMP_EDITOR (ee));
	if (cancel_component_dialog ((GtkWindow *) ee,
				     comp_editor_get_e_cal (COMP_EDITOR (ee)), comp, FALSE)) {
		comp_editor_send_comp (COMP_EDITOR (ee), E_CAL_COMPONENT_METHOD_CANCEL);
		comp_editor_delete_comp (COMP_EDITOR (ee));
	}
}

static void
forward_cmd (GtkWidget *widget, gpointer data)
{
	EventEditor *ee = EVENT_EDITOR (data);

	if (comp_editor_save_comp (COMP_EDITOR (ee), TRUE))
		comp_editor_send_comp (COMP_EDITOR (ee), E_CAL_COMPONENT_METHOD_PUBLISH);
}

static void
model_changed (EventEditor *ee)
{
	if (!ee->priv->updating) {
		comp_editor_set_changed (COMP_EDITOR (ee), TRUE);
		comp_editor_set_needs_send (COMP_EDITOR (ee), TRUE);
	}
}

static void
model_row_change_insert_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	model_changed (EVENT_EDITOR (data));
}

static void
model_row_delete_cb (GtkTreeModel *model, GtkTreePath *path, gpointer data)
{
	model_changed (EVENT_EDITOR (data));
}

