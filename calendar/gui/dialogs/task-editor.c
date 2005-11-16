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

#include <e-util/e-icon-factory.h>
#include <evolution-shell-component-utils.h>

#include "task-page.h"
#include "task-details-page.h"
#include "meeting-page.h"
#include "cancel-comp.h"
#include "../calendar-config.h"
#include "task-editor.h"

struct _TaskEditorPrivate {
	TaskPage *task_page;
	TaskDetailsPage *task_details_page;
	
	GtkWidget *task_details_window;
	EMeetingStore *model;
	
	gboolean assignment_shown;
	gboolean is_assigned;
	gboolean updating;	
};



static void task_editor_set_e_cal (CompEditor *editor, ECal *client);
static void task_editor_edit_comp (CompEditor *editor, ECalComponent *comp);
static gboolean task_editor_send_comp (CompEditor *editor, ECalComponentItipMethod method);
static void task_editor_finalize (GObject *object);

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

static void
menu_view_role_cb (BonoboUIComponent           *component,
		      const char                  *path,
		      Bonobo_UIComponent_EventType type,
		      const char                  *state,
		      gpointer                     user_data)
{
	TaskEditor *te = (TaskEditor *) user_data;
	
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	task_page_set_view_role (te->priv->task_page, atoi(state));	
	calendar_config_set_show_role (atoi(state));	
}

static void
menu_view_status_cb (BonoboUIComponent           *component,
		      const char                  *path,
		      Bonobo_UIComponent_EventType type,
		      const char                  *state,
		      gpointer                     user_data)
{
	TaskEditor *te = (TaskEditor *) user_data;
	
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	
	task_page_set_view_status (te->priv->task_page, atoi(state));
	calendar_config_set_show_status (atoi(state));	
}

static void
menu_view_type_cb (BonoboUIComponent           *component,
		      const char                  *path,
		      Bonobo_UIComponent_EventType type,
		      const char                  *state,
		      gpointer                     user_data)
{
	TaskEditor *te = (TaskEditor *) user_data;
	
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	task_page_set_view_type (te->priv->task_page, atoi(state));	
	calendar_config_set_show_type (atoi(state));	
}

static void
menu_view_rsvp_cb (BonoboUIComponent           *component,
		      const char                  *path,
		      Bonobo_UIComponent_EventType type,
		      const char                  *state,
		      gpointer                     user_data)
{
	TaskEditor *te = (TaskEditor *) user_data;
	
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	task_page_set_view_rsvp (te->priv->task_page, atoi(state));	
	calendar_config_set_show_rsvp (atoi(state));	
}

static void
menu_show_time_zone_cb (BonoboUIComponent           *component,
		       const char                  *path,
		       Bonobo_UIComponent_EventType type,
		       const char                  *state,
		       gpointer                     user_data)
{
	TaskEditor *te = (TaskEditor *) user_data;
	
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	task_page_set_show_timezone (te->priv->task_page, atoi(state));	
	calendar_config_set_show_timezone (atoi(state));
}

static void
menu_show_categories_cb (BonoboUIComponent           *component,
		       const char                  *path,
		       Bonobo_UIComponent_EventType type,
		       const char                  *state,
		       gpointer                     user_data)
{
	TaskEditor *te = (TaskEditor *) user_data;
	
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	task_page_set_show_categories (te->priv->task_page, atoi(state));	
	calendar_config_set_show_categories (atoi(state));
}

static void
menu_class_public_cb (BonoboUIComponent           *ui_component,
		     const char                  *path,
		     Bonobo_UIComponent_EventType type,
		     const char                  *state,
		     gpointer			  user_data)
{
	TaskEditor *te = (TaskEditor *) user_data;

	if (state[0] == '0')
		return;
	printf("Setting to public\n");
	task_page_set_classification (te->priv->task_page, E_CAL_COMPONENT_CLASS_PUBLIC);
}

static void
menu_class_private_cb (BonoboUIComponent           *ui_component,
		      const char                  *path,
		      Bonobo_UIComponent_EventType type,
		      const char                  *state,
		      gpointer			  user_data)
{
	TaskEditor *te = (TaskEditor *) user_data;
	if (state[0] == '0')
		return;
	
	printf("Setting to private\n");
	task_page_set_classification (te->priv->task_page, E_CAL_COMPONENT_CLASS_PRIVATE);
}

static void
menu_class_confidential_cb (BonoboUIComponent           *ui_component,
		     	   const char                  *path,
		     	   Bonobo_UIComponent_EventType type,
		     	   const char                  *state,
		     	   gpointer			user_data)
{
	TaskEditor *te = (TaskEditor *) user_data;
	if (state[0] == '0')
		return;
	
	printf("Setting to confidential\n");
	task_page_set_classification (te->priv->task_page, E_CAL_COMPONENT_CLASS_CONFIDENTIAL);
}


static void
menu_option_status_cb (BonoboUIComponent           *ui_component,
		       const char                  *path,
		       Bonobo_UIComponent_EventType type,
		       const char                  *state,
		       gpointer			user_data)
{
	TaskEditor *te = (TaskEditor *) user_data;
	
	gtk_widget_show (te->priv->task_details_window);
}

static void
menu_insert_send_options_cmd (BonoboUIComponent *uic,
		   	 void *data,
		   	 const char *path)
{
	TaskEditor *te = (TaskEditor *) data;
	
	task_page_sendoptions_clicked_cb (te->priv->task_page);
}

static void
menu_show_time_zone_cmd (BonoboUIComponent *uic,
		   	 void *data,
		   	 const char *path)
{	/* TODO implement it
	TaskEditor *te = (TaskEditor *) data;
	
	task_page_set_show_timezone (te->priv->task_page, atoi(state));	
	calendar_config_set_show_timezone (atoi(state)); */
}

static void
menu_option_status_cmd (BonoboUIComponent *uic,
		     void *data,
		     const char *path)
{
	TaskEditor *te = (TaskEditor *) data;
	
	gtk_widget_show (te->priv->task_details_window);
}

static BonoboUIVerb verbs [] = {

	BONOBO_UI_VERB ("ViewTimeZone", menu_show_time_zone_cmd),	
	BONOBO_UI_VERB ("OptionStatus", menu_option_status_cmd),
	BONOBO_UI_VERB ("InsertSendOptions", menu_insert_send_options_cmd),
	BONOBO_UI_VERB_END
};

static EPixmap pixmaps[] = {
	E_PIXMAP ("/Toolbar/ViewTimeZone", "stock_timezone", E_ICON_SIZE_LARGE_TOOLBAR),	
	E_PIXMAP ("/Toolbar/OptionStatus", "stock_view-details", E_ICON_SIZE_LARGE_TOOLBAR),	
	E_PIXMAP ("/commands/OptionStatus", "stock_view-details", E_ICON_SIZE_LARGE_TOOLBAR),	
	
	E_PIXMAP_END
};


/* Object initialization function for the task editor */
static void
task_editor_init (TaskEditor *te)
{
	TaskEditorPrivate *priv;
	CompEditor *editor = COMP_EDITOR(te);
	gboolean status;
	
	priv = g_new0 (TaskEditorPrivate, 1);
	te->priv = priv;

	priv->model = E_MEETING_STORE (e_meeting_store_new ());
	priv->assignment_shown = TRUE;
	priv->updating = FALSE;	
	priv->is_assigned = FALSE;

	bonobo_ui_component_freeze (editor->uic, NULL);

	bonobo_ui_component_add_verb_list_with_data (editor->uic, verbs, te);

	bonobo_ui_util_set_ui (editor->uic, PREFIX,
			       EVOLUTION_UIDIR "/evolution-task-editor.xml",
			       "evolution-task-editor", NULL);

	/* Show hide the status fields */
	status = calendar_config_get_show_status ();
	bonobo_ui_component_set_prop (
		editor->uic, "/commands/ViewStatus",
		"state", status ? "1" : "0", NULL);
	bonobo_ui_component_add_listener (
		editor->uic, "ViewStatus",
		menu_view_status_cb, editor);
	
	/* Show hide the type fields */
	status = calendar_config_get_show_type ();
	bonobo_ui_component_set_prop (
		editor->uic, "/commands/ViewType",
		"state", status ? "1" : "0", NULL);
	bonobo_ui_component_add_listener (
		editor->uic, "ViewType",
		menu_view_type_cb, editor);

	/* Show hide the role fields */
	status = calendar_config_get_show_role ();
	bonobo_ui_component_set_prop (
		editor->uic, "/commands/ViewRole",
		"state", status ? "1" : "0", NULL);
	bonobo_ui_component_add_listener (
		editor->uic, "ViewRole",
		menu_view_role_cb, editor);

	/* Show hide the rsvp fields */
	status = calendar_config_get_show_rsvp ();
	bonobo_ui_component_set_prop (
		editor->uic, "/commands/ViewRSVP",
		"state", status ? "1" : "0", NULL);
	bonobo_ui_component_add_listener (
		editor->uic, "ViewRSVP",
		menu_view_rsvp_cb, editor);

	status = calendar_config_get_show_timezone ();
	bonobo_ui_component_set_prop (
		editor->uic, "/commands/ViewTimeZone",
		"state", status ? "1" : "0", NULL);
	bonobo_ui_component_add_listener (
		editor->uic, "ViewTimeZone",
		menu_show_time_zone_cb, editor);

	status = calendar_config_get_show_categories ();
	bonobo_ui_component_set_prop (
		editor->uic, "/commands/ViewCategories",
		"state", status ? "1" : "0", NULL);
	bonobo_ui_component_add_listener (
		editor->uic, "ViewCategories",
		menu_show_categories_cb, editor);

	bonobo_ui_component_set_prop (
		editor->uic, "/commands/ActionClassPublic",
		"state", "1", NULL);
	bonobo_ui_component_add_listener (
		editor->uic, "ActionClassPublic",
		menu_class_public_cb, editor);
	bonobo_ui_component_add_listener (
		editor->uic, "ActionClassPrivate",
		menu_class_private_cb, editor);
	bonobo_ui_component_add_listener (
		editor->uic, "ActionClassConfidential",
		menu_class_confidential_cb, editor);

	bonobo_ui_component_add_listener (
		editor->uic, "OptionStatus", 
		menu_option_status_cb, editor);
	
	e_pixmaps_update (editor->uic, pixmaps);

	bonobo_ui_component_thaw (editor->uic, NULL);	

	
	comp_editor_set_help_section (COMP_EDITOR (te), "usage-calendar-todo");
}

TaskEditor *
task_editor_construct (TaskEditor *te, ECal *client)
{
	TaskEditorPrivate *priv;
	gboolean read_only = FALSE;
	CompEditor *editor = COMP_EDITOR (te);
	
	priv = te->priv;

	priv->task_page = task_page_new (priv->model, client, editor->uic);
	g_object_ref (priv->task_page);
	gtk_object_sink (GTK_OBJECT (priv->task_page));
	comp_editor_append_page (COMP_EDITOR (te), 
				 COMP_EDITOR_PAGE (priv->task_page),
				 _("_Task"), TRUE);
	g_signal_connect (G_OBJECT (priv->task_page), "client_changed",
			  G_CALLBACK (client_changed_cb), te);

	priv->task_details_window = gtk_dialog_new_with_buttons (_("Task Details"),
								(GtkWindow *) te, GTK_DIALOG_MODAL,
							  	"gtk-close", GTK_RESPONSE_CLOSE,
							  	NULL);
	g_signal_connect (priv->task_details_window, "response", G_CALLBACK(gtk_widget_hide), NULL);
	
	priv->task_details_page = task_details_page_new ();
	g_object_ref (priv->task_details_page);
	gtk_object_sink (GTK_OBJECT (priv->task_details_page));
	gtk_container_add ((GtkContainer *) GTK_DIALOG(priv->task_details_window)->vbox, 
		           comp_editor_page_get_widget ((CompEditorPage *)priv->task_details_page));
	gtk_widget_show_all (priv->task_details_window);
	gtk_widget_hide (priv->task_details_window);
	comp_editor_append_page (editor, COMP_EDITOR_PAGE (priv->task_details_page), NULL, FALSE);
	
	if (!e_cal_is_read_only (client, &read_only, NULL))
			read_only = TRUE;

	if (priv->is_assigned) {
		if (e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_REQ_SEND_OPTIONS))
			task_page_show_options (priv->task_page);
	
		task_page_set_assignment (priv->task_page, TRUE);
		comp_editor_set_group_item (COMP_EDITOR (te), TRUE);
	} else {
		task_page_set_assignment (priv->task_page, FALSE);
	
		bonobo_ui_component_set_prop (editor->uic, "/commands/InsertSendOptions", "hidden", "1", NULL);
		bonobo_ui_component_set_prop (editor->uic, "/commands/ViewRole", "hidden", "1", NULL);
		bonobo_ui_component_set_prop (editor->uic, "/commands/ViewRSVP", "hidden", "1", NULL);
		bonobo_ui_component_set_prop (editor->uic, "/commands/ViewType", "hidden", "1", NULL);
		bonobo_ui_component_set_prop (editor->uic, "/commands/ViewStatus", "hidden", "1", NULL);
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
	
	if (attendees != NULL) {
		GSList *l;
		int row;
		
		task_page_hide_options (priv->task_page);	
		task_page_set_assignment (priv->task_page, TRUE);

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
	
	comp = task_page_get_cancel_comp (priv->task_page);
	if (comp != NULL) {
		ECal *client;
		gboolean result;
		
		client = e_meeting_store_get_e_cal (priv->model);
		result = itip_send_comp (E_CAL_COMPONENT_METHOD_CANCEL, comp,
				client, NULL, NULL);
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

	if (priv->task_page) {
		g_object_unref (priv->task_page);
		priv->task_page = NULL;
	}

	if (priv->task_details_page) {
		g_object_unref (priv->task_details_page);
		priv->task_details_page = NULL;
	}
	
	if (priv->model) {
		g_object_unref (priv->model);
		priv->model = NULL;
	}
	
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
task_editor_new (ECal *client, CompEditorFlags flags)
{
	TaskEditor *te;

	te = g_object_new (TYPE_TASK_EDITOR, NULL);
	te->priv->is_assigned = flags & COMP_EDITOR_IS_ASSIGNED;
	comp_editor_set_flags (COMP_EDITOR (te), flags);

	return task_editor_construct (te, client);
}

static void
show_assignment (TaskEditor *te)
{
	TaskEditorPrivate *priv;

	priv = te->priv;

	task_page_set_assignment (priv->task_page, TRUE);
	if (!priv->assignment_shown) {
		priv->assignment_shown = TRUE;

		comp_editor_set_needs_send (COMP_EDITOR (te), priv->assignment_shown);
		comp_editor_set_changed (COMP_EDITOR (te), FALSE);
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
