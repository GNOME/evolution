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

#include <config.h>
#include <string.h>
#include <glade/glade.h>
#include <gal/widgets/e-unicode.h>
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

	EMeetingModel *model;
	
	gboolean assignment_shown;
	gboolean updating;	
};



static void task_editor_class_init (TaskEditorClass *class);
static void task_editor_init (TaskEditor *te);
static void task_editor_edit_comp (CompEditor *editor, CalComponent *comp);
static gboolean task_editor_send_comp (CompEditor *editor, CalComponentItipMethod method);
static void task_editor_finalize (GObject *object);

static void assign_task_cmd (GtkWidget *widget, gpointer data);
static void refresh_task_cmd (GtkWidget *widget, gpointer data);
static void cancel_task_cmd (GtkWidget *widget, gpointer data);
static void forward_cmd (GtkWidget *widget, gpointer data);

static void model_row_changed_cb (ETableModel *etm, int row, gpointer data);
static void row_count_changed_cb (ETableModel *etm, int row, int count, gpointer data);

static BonoboUIVerb verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("ActionAssignTask", assign_task_cmd),
	BONOBO_UI_UNSAFE_VERB ("ActionRefreshTask", refresh_task_cmd),
	BONOBO_UI_UNSAFE_VERB ("ActionCancelTask", cancel_task_cmd),
	BONOBO_UI_UNSAFE_VERB ("ActionForward", forward_cmd),

	BONOBO_UI_VERB_END
};

static CompEditorClass *parent_class;



/**
 * task_editor_get_type:
 *
 * Registers the #TaskEditor class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #TaskEditor class.
 **/

E_MAKE_TYPE (task_editor, "TaskEditor", TaskEditor, task_editor_class_init, task_editor_init,
	     TYPE_COMP_EDITOR);

/* Class initialization function for the event editor */
static void
task_editor_class_init (TaskEditorClass *klass)
{
	GObjectClass *object_class;
	CompEditorClass *editor_class;

	object_class = (GObjectClass *) klass;
	editor_class = (CompEditorClass *) klass;

	parent_class = g_type_class_ref(TYPE_COMP_EDITOR);

	editor_class->edit_comp = task_editor_edit_comp;
	editor_class->send_comp = task_editor_send_comp;

	object_class->finalize = task_editor_finalize;
}

static void
set_menu_sens (TaskEditor *te) 
{
	TaskEditorPrivate *priv;
	gboolean sens, existing, user, read_only;
	
	priv = te->priv;

 	existing = comp_editor_get_existing_org (COMP_EDITOR (te));
 	user = comp_editor_get_user_org (COMP_EDITOR (te));
	read_only = cal_client_is_read_only (comp_editor_get_cal_client (COMP_EDITOR (te)));
 
  	sens = cal_client_get_static_capability (comp_editor_get_cal_client (COMP_EDITOR (te)),
						 CAL_STATIC_CAPABILITY_NO_TASK_ASSIGNMENT)
						 || priv->assignment_shown || read_only;
  	comp_editor_set_ui_prop (COMP_EDITOR (te), 
  				 "/commands/ActionAssignTask", 
  				 "sensitive", sens ? "0" : "1");
  
 	sens = priv->assignment_shown && existing && !user && !read_only;
  	comp_editor_set_ui_prop (COMP_EDITOR (te), 
  				 "/commands/ActionRefreshTask", 
  				 "sensitive", sens ? "1" : "0");
 
 	sens = priv->assignment_shown && existing && user && !read_only;
  	comp_editor_set_ui_prop (COMP_EDITOR (te), 
  				 "/commands/ActionCancelTask", 
  				 "sensitive", sens ? "1" : "0");

	comp_editor_set_ui_prop (COMP_EDITOR (te),
				 "/commands/FileSave",
				 "sensitive", read_only ? "0" : "1");
	comp_editor_set_ui_prop (COMP_EDITOR (te),
				 "/commands/FileSaveAndClose",
				 "sensitive", read_only ? "0" : "1");
	comp_editor_set_ui_prop (COMP_EDITOR (te),
				 "/commands/FileDelete",
				 "sensitive", read_only ? "0" : "1");
}

static void
init_widgets (TaskEditor *te)
{
	TaskEditorPrivate *priv;

	priv = te->priv;

	g_signal_connect((priv->model), "model_row_changed",
			    G_CALLBACK (model_row_changed_cb), te);
	g_signal_connect((priv->model), "model_rows_inserted",
			    G_CALLBACK (row_count_changed_cb), te);
	g_signal_connect((priv->model), "model_rows_deleted",
			    G_CALLBACK (row_count_changed_cb), te);
}

/* Object initialization function for the task editor */
static void
task_editor_init (TaskEditor *te)
{
	TaskEditorPrivate *priv;
	
	priv = g_new0 (TaskEditorPrivate, 1);
	te->priv = priv;

	priv->model = E_MEETING_MODEL (e_meeting_model_new ());
	priv->assignment_shown = TRUE;
	priv->updating = FALSE;	

}

TaskEditor *
task_editor_construct (TaskEditor *te, CalClient *client)
{
	TaskEditorPrivate *priv;
	
	priv = te->priv;

	priv->task_page = task_page_new ();
	g_object_ref (priv->task_page);
	gtk_object_sink (GTK_OBJECT (priv->task_page));
	comp_editor_append_page (COMP_EDITOR (te), 
				 COMP_EDITOR_PAGE (priv->task_page),
				 _("Basic"));

	priv->task_details_page = task_details_page_new ();
	g_object_ref (priv->task_details_page);
	gtk_object_sink (GTK_OBJECT (priv->task_details_page));
	comp_editor_append_page (COMP_EDITOR (te),
				 COMP_EDITOR_PAGE (priv->task_details_page),
				 _("Details"));

	priv->meet_page = meeting_page_new (priv->model, client);
	g_object_ref (priv->meet_page);
	gtk_object_sink (GTK_OBJECT (priv->meet_page));
	comp_editor_append_page (COMP_EDITOR (te),
				 COMP_EDITOR_PAGE (priv->meet_page),
				 _("Assignment"));

	comp_editor_set_cal_client (COMP_EDITOR (te), client);

	comp_editor_merge_ui (COMP_EDITOR (te), "evolution-task-editor.xml", verbs, NULL);

	init_widgets (te);
	set_menu_sens (te);

	return te;
}

static void
task_editor_edit_comp (CompEditor *editor, CalComponent *comp)
{
	TaskEditor *te;
	TaskEditorPrivate *priv;
	CalComponentOrganizer organizer;
	CalClient *client;
	GSList *attendees = NULL;
	
	te = TASK_EDITOR (editor);
	priv = te->priv;

	priv->updating = TRUE;

	if (parent_class->edit_comp)
		parent_class->edit_comp (editor, comp);

	client = comp_editor_get_cal_client (COMP_EDITOR (editor));

	/* Get meeting related stuff */
	cal_component_get_organizer (comp, &organizer);
	cal_component_get_attendee_list (comp, &attendees);
	
	/* Clear things up */
	e_meeting_model_remove_all_attendees (priv->model);

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
			CalComponentAttendee *ca = l->data;
			EMeetingAttendee *ia;

			ia = E_MEETING_ATTENDEE (e_meeting_attendee_new_from_cal_component_attendee (ca));
 			if (!comp_editor_get_user_org (editor))
 				e_meeting_attendee_set_edit_level (ia,  E_MEETING_ATTENDEE_EDIT_NONE);
  			e_meeting_model_add_attendee (priv->model, ia);			

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

				ia = e_meeting_model_find_attendee (priv->model, account->id->address, &row);
				if (ia != NULL)
					e_meeting_attendee_set_edit_level (ia, E_MEETING_ATTENDEE_EDIT_STATUS);
			}
			g_object_unref(it);
		} else if (cal_client_get_organizer_must_attend (client)) {
			EMeetingAttendee *ia;

			ia = e_meeting_model_find_attendee (priv->model, organizer.value, &row);
			if (ia != NULL)
				e_meeting_attendee_set_edit_level (ia, E_MEETING_ATTENDEE_EDIT_NONE);
		}

		priv->assignment_shown = TRUE;		
	}
	cal_component_free_attendee_list (attendees);

	set_menu_sens (te);
	comp_editor_set_needs_send (COMP_EDITOR (te), priv->assignment_shown && itip_organizer_is_user (comp, client));

	priv->updating = FALSE;
}

static gboolean
task_editor_send_comp (CompEditor *editor, CalComponentItipMethod method)
{
	TaskEditor *te = TASK_EDITOR (editor);
	TaskEditorPrivate *priv;
	CalComponent *comp = NULL;

	priv = te->priv;

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
		g_object_unref((comp));

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
task_editor_finalize (GObject *object)
{
	TaskEditor *te;
	TaskEditorPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_TASK_EDITOR (object));

	te = TASK_EDITOR (object);
	priv = te->priv;

	g_object_unref((priv->task_page));
	g_object_unref((priv->task_details_page));
	g_object_unref((priv->meet_page));
	
#if 0
	/* FIXME we don't unref here because we "sink" in 
	   e-meeting-model.c:init */
	g_object_unref (priv->model);
#endif
	
	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/**
 * task_editor_new:
 * @client: a CalClient
 *
 * Creates a new event editor dialog.
 *
 * Return value: A newly-created event editor dialog, or NULL if the event
 * editor could not be created.
 **/
TaskEditor *
task_editor_new (CalClient *client)
{
	TaskEditor *te;

	te = g_object_new (TYPE_TASK_EDITOR, NULL);
	return task_editor_construct (te, client);
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

		set_menu_sens (te);
		comp_editor_set_needs_send (COMP_EDITOR (te), priv->assignment_shown);
		comp_editor_set_changed (COMP_EDITOR (te), TRUE);
	}

	comp_editor_show_page (COMP_EDITOR (te),
			       COMP_EDITOR_PAGE (priv->meet_page));
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

	comp_editor_send_comp (COMP_EDITOR (te), CAL_COMPONENT_METHOD_REFRESH);
}

static void
cancel_task_cmd (GtkWidget *widget, gpointer data)
{
	TaskEditor *te = TASK_EDITOR (data);
	CalComponent *comp;
	
	comp = comp_editor_get_current_comp (COMP_EDITOR (te));
	if (cancel_component_dialog (comp_editor_get_cal_client (COMP_EDITOR (te)), comp, FALSE)) {
		comp_editor_send_comp (COMP_EDITOR (te), CAL_COMPONENT_METHOD_CANCEL);
		comp_editor_delete_comp (COMP_EDITOR (te));
	}
}

static void
forward_cmd (GtkWidget *widget, gpointer data)
{
	TaskEditor *te = TASK_EDITOR (data);
	
	if (comp_editor_save_comp (COMP_EDITOR (te), TRUE))
		comp_editor_send_comp (COMP_EDITOR (te), CAL_COMPONENT_METHOD_PUBLISH);
}

static void
model_row_changed_cb (ETableModel *etm, int row, gpointer data)
{
	TaskEditor *te = TASK_EDITOR (data);
	TaskEditorPrivate *priv;
	
	priv = te->priv;

	if (!priv->updating) {
		comp_editor_set_changed (COMP_EDITOR (te), TRUE);
		comp_editor_set_needs_send (COMP_EDITOR (te), TRUE);
	}	
}

static void
row_count_changed_cb (ETableModel *etm, int row, int count, gpointer data)
{
	TaskEditor *te = TASK_EDITOR (data);
	TaskEditorPrivate *priv;
	
	priv = te->priv;

	if (!priv->updating) {
		comp_editor_set_changed (COMP_EDITOR (te), TRUE);
		comp_editor_set_needs_send (COMP_EDITOR (te), TRUE);
	}	
}
