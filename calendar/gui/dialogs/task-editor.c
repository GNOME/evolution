/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar - Task editor dialog
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Miguel de Icaza <miguel@ximian.com>
 *          Federico Mena-Quintero <federico@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *          JP Rosevear <jpr@ximian.com>
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

#include "task-page.h"
#include "task-details-page.h"
#include "meeting-page.h"
#include "cancel-comp.h"
#include "task-editor.h"

struct _TaskEditorPrivate {
	TaskPage *task_page;
	TaskDetailsPage *task_details_page;
	MeetingPage *meet_page;

	EMeetingStore *model;
	
	gboolean assignment_shown;
	gboolean is_assigned;
	gboolean updating;	
};



static void task_editor_set_e_cal (CompEditor *editor, ECal *client);
static void task_editor_edit_comp (CompEditor *editor, ECalComponent *comp);
static gboolean task_editor_send_comp (CompEditor *editor, ECalComponentItipMethod method);
static void task_editor_finalize (GObject *object);

static void assign_task_cmd (GtkWidget *widget, gpointer data);
static void refresh_task_cmd (GtkWidget *widget, gpointer data);
static void cancel_task_cmd (GtkWidget *widget, gpointer data);
static void forward_cmd (GtkWidget *widget, gpointer data);

static void model_row_change_insert_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data);
static void model_row_delete_cb (GtkTreeModel *model, GtkTreePath *path, gpointer data);

G_DEFINE_TYPE (TaskEditor, task_editor, TYPE_COMP_EDITOR);

/* Class initialization function for the event editor */
static void
task_editor_class_init (TaskEditorClass *klass)
{
	GObjectClass *object_class;
	CompEditorClass *editor_class;

	object_class = (GObjectClass *) klass;
	editor_class = (CompEditorClass *) klass;

	editor_class->set_e_cal = task_editor_set_e_cal;
	editor_class->edit_comp = task_editor_edit_comp;
	editor_class->send_comp = task_editor_send_comp;

	object_class->finalize = task_editor_finalize;
}

static void
init_widgets (TaskEditor *te)
{
	TaskEditorPrivate *priv;

	priv = te->priv;

	g_signal_connect((priv->model), "row_changed",
			    G_CALLBACK (model_row_change_insert_cb), te);
	g_signal_connect((priv->model), "row_inserted",
			    G_CALLBACK (model_row_change_insert_cb), te);
	g_signal_connect((priv->model), "row_deleted",
			    G_CALLBACK (model_row_delete_cb), te);
}

static void
client_changed_cb (CompEditorPage *page, ECal *client, gpointer user_data)
{
//	set_menu_sens (TASK_EDITOR (user_data));
}

/* Object initialization function for the task editor */
static void
task_editor_init (TaskEditor *te)
{
	TaskEditorPrivate *priv;
	
	priv = g_new0 (TaskEditorPrivate, 1);
	te->priv = priv;

	priv->model = E_MEETING_STORE (e_meeting_store_new ());
	priv->assignment_shown = TRUE;
	priv->updating = FALSE;	
	priv->is_assigned = FALSE;

}

TaskEditor *
task_editor_construct (TaskEditor *te, ECal *client, gboolean is_assigned)
{
	TaskEditorPrivate *priv;
	
	priv = te->priv;

	priv->is_assigned = is_assigned;
	priv->task_page = task_page_new ();
	g_object_ref (priv->task_page);
	gtk_object_sink (GTK_OBJECT (priv->task_page));
	comp_editor_append_page (COMP_EDITOR (te), 
				 COMP_EDITOR_PAGE (priv->task_page),
				 _("Task"));
	g_signal_connect (G_OBJECT (priv->task_page), "client_changed",
			  G_CALLBACK (client_changed_cb), te);

	priv->task_details_page = task_details_page_new ();
	g_object_ref (priv->task_details_page);
	gtk_object_sink (GTK_OBJECT (priv->task_details_page));
	comp_editor_append_page (COMP_EDITOR (te),
				 COMP_EDITOR_PAGE (priv->task_details_page),
				 _("Status"));
	if (priv->is_assigned) {
		comp_editor_set_group_item (COMP_EDITOR (te), TRUE);
		priv->meet_page = meeting_page_new (priv->model, client);
		g_object_ref (priv->meet_page);
		gtk_object_sink (GTK_OBJECT (priv->meet_page));
		comp_editor_append_page (COMP_EDITOR (te),
					 COMP_EDITOR_PAGE (priv->meet_page),
					 _("Assignment"));
	}

	comp_editor_set_e_cal (COMP_EDITOR (te), client);

	init_widgets (te);

	return te;
}

static void
task_editor_set_e_cal (CompEditor *editor, ECal *client)
{
	TaskEditor *te;
	TaskEditorPrivate *priv;

	te = TASK_EDITOR (editor);
	priv = te->priv;

	e_meeting_store_set_e_cal (priv->model, client);

	if (COMP_EDITOR_CLASS (task_editor_parent_class)->set_e_cal)
		COMP_EDITOR_CLASS (task_editor_parent_class)->set_e_cal (editor, client);
}

static void
task_editor_edit_comp (CompEditor *editor, ECalComponent *comp)
{
	TaskEditor *te;
	TaskEditorPrivate *priv;
	ECalComponentOrganizer organizer;
	ECal *client;
	GSList *attendees = NULL;
	
	te = TASK_EDITOR (editor);
	priv = te->priv;

	priv->updating = TRUE;

	if (COMP_EDITOR_CLASS (task_editor_parent_class)->edit_comp)
		COMP_EDITOR_CLASS (task_editor_parent_class)->edit_comp (editor, comp);

	client = comp_editor_get_e_cal (COMP_EDITOR (editor));

	/* Get meeting related stuff */
	e_cal_component_get_organizer (comp, &organizer);
	e_cal_component_get_attendee_list (comp, &attendees);
	
	/* Clear things up */
	e_meeting_store_remove_all_attendees (priv->model);

	if (attendees == NULL) {
		comp_editor_remove_page (editor, COMP_EDITOR_PAGE (priv->meet_page));
		priv->assignment_shown = FALSE;
	} else {
		GSList *l;
		int row;
		
		if (!priv->assignment_shown)
			comp_editor_append_page (COMP_EDITOR (te),
						 COMP_EDITOR_PAGE (priv->meet_page),
						 _("Assignment"));

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
		
		
		comp_editor_set_group_item (COMP_EDITOR (te), TRUE);
		priv->assignment_shown = TRUE;		
	}
	e_cal_component_free_attendee_list (attendees);

	comp_editor_set_needs_send (COMP_EDITOR (te), priv->assignment_shown && itip_organizer_is_user (comp, client));

	priv->updating = FALSE;
}

static gboolean
task_editor_send_comp (CompEditor *editor, ECalComponentItipMethod method)
{
	TaskEditor *te = TASK_EDITOR (editor);
	TaskEditorPrivate *priv;
	ECalComponent *comp = NULL;

	priv = te->priv;

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
	if (COMP_EDITOR_CLASS (task_editor_parent_class)->send_comp)
		return COMP_EDITOR_CLASS (task_editor_parent_class)->send_comp (editor, method);

	return FALSE;
}

/* Destroy handler for the event editor */
static void
task_editor_finalize (GObject *object)
{
	TaskEditor *te;
	TaskEditorPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_TASK_EDITOR (object));

	te = TASK_EDITOR (object);
	priv = te->priv;

	g_object_unref (priv->task_page);
	g_object_unref (priv->task_details_page);
	g_object_unref (priv->meet_page);
	
	g_object_unref (priv->model);
	
	g_free (priv);

	if (G_OBJECT_CLASS (task_editor_parent_class)->finalize)
		(* G_OBJECT_CLASS (task_editor_parent_class)->finalize) (object);
}

/**
 * task_editor_new:
 * @client: a ECal
 *
 * Creates a new event editor dialog.
 *
 * Return value: A newly-created event editor dialog, or NULL if the event
 * editor could not be created.
 **/
TaskEditor *
task_editor_new (ECal *client, gboolean is_assigned)
{
	TaskEditor *te;

	te = g_object_new (TYPE_TASK_EDITOR, NULL);
	return task_editor_construct (te, client, is_assigned);
}

static void
show_assignment (TaskEditor *te)
{
	TaskEditorPrivate *priv;

	priv = te->priv;

	if (!priv->assignment_shown) {
		comp_editor_append_page (COMP_EDITOR (te),
					 COMP_EDITOR_PAGE (priv->meet_page),
					 _("Assignment"));
		priv->assignment_shown = TRUE;

		comp_editor_set_needs_send (COMP_EDITOR (te), priv->assignment_shown);
		comp_editor_set_changed (COMP_EDITOR (te), TRUE);
	}

}

void
task_editor_show_assignment (TaskEditor *te)
{
	g_return_if_fail (te != NULL);
	g_return_if_fail (IS_TASK_EDITOR (te));

	show_assignment (te);
}

static void
assign_task_cmd (GtkWidget *widget, gpointer data)
{
	TaskEditor *te = TASK_EDITOR (data);

	show_assignment (te);
}

static void
refresh_task_cmd (GtkWidget *widget, gpointer data)
{
	TaskEditor *te = TASK_EDITOR (data);

	comp_editor_send_comp (COMP_EDITOR (te), E_CAL_COMPONENT_METHOD_REFRESH);
}

static void
cancel_task_cmd (GtkWidget *widget, gpointer data)
{
	TaskEditor *te = TASK_EDITOR (data);
	ECalComponent *comp;
	
	comp = comp_editor_get_current_comp (COMP_EDITOR (te));
	if (cancel_component_dialog ((GtkWindow *) te,
				     comp_editor_get_e_cal (COMP_EDITOR (te)), comp, FALSE)) {
		comp_editor_send_comp (COMP_EDITOR (te), E_CAL_COMPONENT_METHOD_CANCEL);
		comp_editor_delete_comp (COMP_EDITOR (te));
	}
}

static void
forward_cmd (GtkWidget *widget, gpointer data)
{
	TaskEditor *te = TASK_EDITOR (data);
	
	if (comp_editor_save_comp (COMP_EDITOR (te), TRUE))
		comp_editor_send_comp (COMP_EDITOR (te), E_CAL_COMPONENT_METHOD_PUBLISH);
}

static void
model_changed (TaskEditor *te)
{
	if (!te->priv->updating) {
		comp_editor_set_changed (COMP_EDITOR (te), TRUE);
		comp_editor_set_needs_send (COMP_EDITOR (te), TRUE);
	}	
}

static void
model_row_change_insert_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	model_changed (TASK_EDITOR (data));
}

static void
model_row_delete_cb (GtkTreeModel *model, GtkTreePath *path, gpointer data)
{
	model_changed (TASK_EDITOR (data));
}
