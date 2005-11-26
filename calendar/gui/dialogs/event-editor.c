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
#include <misc/e-dateedit.h>
#include <e-util/e-icon-factory.h>
#include <e-util/e-util-private.h>
#include <evolution-shell-component-utils.h>
#include "event-page.h"
#include "recurrence-page.h"
#include "meeting-page.h"
#include "schedule-page.h"
#include "cancel-comp.h"
#include "event-editor.h"
#include "../calendar-config.h"

struct _EventEditorPrivate {
	EventPage *event_page;
	RecurrencePage *recur_page;
	GtkWidget *recur_window;
	SchedulePage *sched_page;
	GtkWidget *sched_window;

	EMeetingStore *model;
	gboolean is_meeting;
	gboolean meeting_shown;
	gboolean updating;	
};



static void event_editor_set_e_cal (CompEditor *editor, ECal *client);
static void event_editor_edit_comp (CompEditor *editor, ECalComponent *comp);
static gboolean event_editor_send_comp (CompEditor *editor, ECalComponentItipMethod method);
static void event_editor_finalize (GObject *object);

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

static void
menu_view_role_cb (BonoboUIComponent           *component,
		      const char                  *path,
		      Bonobo_UIComponent_EventType type,
		      const char                  *state,
		      gpointer                     user_data)
{
	EventEditor *ee = (EventEditor *) user_data;
	
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	event_page_set_view_role (ee->priv->event_page, atoi(state));	
	calendar_config_set_show_role (atoi(state));	
}

static void
menu_view_status_cb (BonoboUIComponent           *component,
		      const char                  *path,
		      Bonobo_UIComponent_EventType type,
		      const char                  *state,
		      gpointer                     user_data)
{
	EventEditor *ee = (EventEditor *) user_data;
	
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	
	event_page_set_view_status (ee->priv->event_page, atoi(state));
	calendar_config_set_show_status (atoi(state));	
}

static void
menu_view_type_cb (BonoboUIComponent           *component,
		      const char                  *path,
		      Bonobo_UIComponent_EventType type,
		      const char                  *state,
		      gpointer                     user_data)
{
	EventEditor *ee = (EventEditor *) user_data;
	
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	event_page_set_view_type (ee->priv->event_page, atoi(state));	
	calendar_config_set_show_type (atoi(state));	
}

static void
menu_view_rsvp_cb (BonoboUIComponent           *component,
		      const char                  *path,
		      Bonobo_UIComponent_EventType type,
		      const char                  *state,
		      gpointer                     user_data)
{
	EventEditor *ee = (EventEditor *) user_data;
	
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	event_page_set_view_rsvp (ee->priv->event_page, atoi(state));	
	calendar_config_set_show_rsvp (atoi(state));	
}

static void
menu_action_alarm_cb (BonoboUIComponent           *component,
		      const char                  *path,
		      Bonobo_UIComponent_EventType type,
		      const char                  *state,
		      gpointer                     user_data)
{
	EventEditor *ee = (EventEditor *) user_data;

	event_page_show_alarm (ee->priv->event_page);
}

static void
menu_show_time_busy_cb (BonoboUIComponent           *component,
		        const char                  *path,
		        Bonobo_UIComponent_EventType type,
		        const char                  *state,
		        gpointer                     user_data)
{
	EventEditor *ee = (EventEditor *) user_data;
	
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	event_page_set_show_time_busy (ee->priv->event_page, atoi(state));	
}

static void
menu_all_day_event_cb (BonoboUIComponent           *component,
		      const char                  *path,
		      Bonobo_UIComponent_EventType type,
		      const char                  *state,
		      gpointer                     user_data)
{
	EventEditor *ee = (EventEditor *) user_data;
	
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	event_page_set_all_day_event (ee->priv->event_page, atoi(state));	
}

static void
menu_show_time_zone_cb (BonoboUIComponent           *component,
		       const char                  *path,
		       Bonobo_UIComponent_EventType type,
		       const char                  *state,
		       gpointer                     user_data)
{
	EventEditor *ee = (EventEditor *) user_data;
	
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	event_page_set_show_timezone (ee->priv->event_page, atoi(state));	
	calendar_config_set_show_timezone (atoi(state));
}

static void
menu_show_categories_cb (BonoboUIComponent           *component,
		       const char                  *path,
		       Bonobo_UIComponent_EventType type,
		       const char                  *state,
		       gpointer                     user_data)
{
	EventEditor *ee = (EventEditor *) user_data;
	
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	event_page_set_show_categories (ee->priv->event_page, atoi(state));	
	calendar_config_set_show_categories (atoi(state));
}

static void
menu_class_public_cb (BonoboUIComponent           *ui_component,
		     const char                  *path,
		     Bonobo_UIComponent_EventType type,
		     const char                  *state,
		     gpointer			  user_data)
{
	EventEditor *ee = (EventEditor *) user_data;

	if (state[0] == '0')
		return;
	event_page_set_classification (ee->priv->event_page, E_CAL_COMPONENT_CLASS_PUBLIC);
}

static void
menu_class_private_cb (BonoboUIComponent           *ui_component,
		      const char                  *path,
		      Bonobo_UIComponent_EventType type,
		      const char                  *state,
		      gpointer			  user_data)
{
	EventEditor *ee = (EventEditor *) user_data;
	if (state[0] == '0')
		return;
	
	event_page_set_classification (ee->priv->event_page, E_CAL_COMPONENT_CLASS_PRIVATE);
}

static void
menu_class_confidential_cb (BonoboUIComponent           *ui_component,
		     	   const char                  *path,
		     	   Bonobo_UIComponent_EventType type,
		     	   const char                  *state,
		     	   gpointer			user_data)
{
	EventEditor *ee = (EventEditor *) user_data;
	if (state[0] == '0')
		return;
	
	event_page_set_classification (ee->priv->event_page, E_CAL_COMPONENT_CLASS_CONFIDENTIAL);
}

static void
menu_action_recurrence_cb (BonoboUIComponent           *ui_component,
		     	   const char                  *path,
		     	   Bonobo_UIComponent_EventType type,
		     	   const char                  *state,
		     	   gpointer			user_data)
{
	EventEditor *ee = (EventEditor *) user_data;
	
	gtk_widget_show (ee->priv->recur_window);
}

static void
menu_action_freebusy_cb	(BonoboUIComponent           *ui_component,
		     	 const char                  *path,
		     	 Bonobo_UIComponent_EventType type,
		     	 const char                  *state,
		     	 gpointer			user_data) 
{
	EventEditor *ee = (EventEditor *) user_data;
	
	gtk_widget_show (ee->priv->sched_window);
}

static void
menu_action_alarm_cmd (BonoboUIComponent *uic,
		    void *data,
		    const char *path)
{
	EventEditor *ee = (EventEditor *) data;

	event_page_show_alarm (ee->priv->event_page);
}

static void
menu_all_day_event_cmd (BonoboUIComponent *uic,
	                void *data,
		        const char *path)
{
	/* TODO 
	EventEditor *ee = (EventEditor *) data;

	event_page_set_all_day_event (ee->priv->event_page, atoi(state));*/
}

static void
menu_show_time_zone_cmd (BonoboUIComponent *uic,
	                 void *data,
		         const char *path)
{
	/* TODO 
	EventEditor *ee = (EventEditor *) data;
	
	event_page_set_show_timezone (ee->priv->event_page, atoi(state));	
	calendar_config_set_show_timezone (atoi(state)); */
}

static void
menu_action_recurrence_cmd (BonoboUIComponent *uic,
		   	   void *data,
		   	   const char *path)
{
	EventEditor *ee = (EventEditor *) data;
	
	gtk_widget_show (ee->priv->recur_window);
}

static void
menu_action_freebusy_cmd (BonoboUIComponent *uic,
		   	 void *data,
		   	 const char *path)
{
	EventEditor *ee = (EventEditor *) data;
	
	gtk_widget_show (ee->priv->sched_window);
}

static void
menu_insert_send_options_cmd (BonoboUIComponent *uic,
		   	 void *data,
		   	 const char *path)
{
	EventEditor *ee = (EventEditor *) data;
	
	event_page_sendoptions_clicked_cb (ee->priv->event_page);
}

static BonoboUIVerb verbs [] = {
	BONOBO_UI_VERB ("ActionAlarm", menu_action_alarm_cmd),
	BONOBO_UI_VERB ("ActionAllDayEvent", menu_all_day_event_cmd),
	BONOBO_UI_VERB ("ViewTimeZone", menu_show_time_zone_cmd),	
	BONOBO_UI_VERB ("ActionRecurrence", menu_action_recurrence_cmd),
	BONOBO_UI_VERB ("ActionFreeBusy", menu_action_freebusy_cmd),
	BONOBO_UI_VERB ("InsertSendOptions", menu_insert_send_options_cmd),
	
	BONOBO_UI_VERB_END
};

static EPixmap pixmaps[] = {
	E_PIXMAP ("/Toolbar/ActionAlarm", "stock_alarm", E_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/ActionAllDayEvent", "stock_new-24h-appointment", E_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/ViewTimeZone", "stock_timezone", E_ICON_SIZE_LARGE_TOOLBAR),	
	E_PIXMAP ("/Toolbar/ActionRecurrence", "stock_task-recurring", E_ICON_SIZE_LARGE_TOOLBAR),	
	E_PIXMAP ("/commands/ActionRecurrence", "stock_task-recurring", E_ICON_SIZE_LARGE_TOOLBAR),		
	E_PIXMAP ("/Toolbar/ActionFreeBusy", "stock_task-recurring", E_ICON_SIZE_LARGE_TOOLBAR),			
	E_PIXMAP_END
};

/* Object initialization function for the event editor */
static void
event_editor_init (EventEditor *ee)
{
	EventEditorPrivate *priv;
	CompEditor *editor = COMP_EDITOR(ee);
	gboolean status;
	char *xmlfile;

	priv = g_new0 (EventEditorPrivate, 1);
	ee->priv = priv;

	priv->model = E_MEETING_STORE (e_meeting_store_new ());
	priv->meeting_shown = TRUE;
	priv->updating = FALSE;	
	priv->is_meeting = FALSE;

	bonobo_ui_component_freeze (editor->uic, NULL);

	bonobo_ui_component_add_verb_list_with_data (editor->uic, verbs, ee);

	xmlfile = g_build_filename (EVOLUTION_UIDIR,
				    "evolution-event-editor.xml",
				    NULL);
	bonobo_ui_util_set_ui (editor->uic, PREFIX,
			       xmlfile,
			       "evolution-event-editor", NULL);
	g_free (xmlfile);

	/* Hide send options */
	bonobo_ui_component_set_prop (
		editor->uic, "/commands/InsertSendOptions",
		"hidden", "1", NULL);

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

	bonobo_ui_component_add_listener (
		editor->uic, "ActionAlarm",
		menu_action_alarm_cb, editor);
	
	bonobo_ui_component_add_listener (
		editor->uic, "ActionAllDayEvent", 
		menu_all_day_event_cb, editor);
	
	bonobo_ui_component_add_listener (
		editor->uic, "ActionShowTimeBusy", 
		menu_show_time_busy_cb, editor);
	
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
		editor->uic, "ActionRecurrence", 
		menu_action_recurrence_cb, editor);
	bonobo_ui_component_add_listener (
		editor->uic, "ActionFreeBusy", 
		menu_action_freebusy_cb, editor);

	e_pixmaps_update (editor->uic, pixmaps);

	bonobo_ui_component_thaw (editor->uic, NULL);	

	comp_editor_set_help_section (COMP_EDITOR (ee), "usage-calendar-apts");
}

EventEditor *
event_editor_construct (EventEditor *ee, ECal *client)
{
	EventEditorPrivate *priv;
	CompEditor *editor = COMP_EDITOR (ee);
	guint32 flags = comp_editor_get_flags (editor);

	priv = ee->priv;

	priv->event_page = event_page_new (priv->model, client, COMP_EDITOR(ee)->uic);
	g_object_ref (priv->event_page);
	gtk_object_sink (GTK_OBJECT (priv->event_page));
	comp_editor_append_page (COMP_EDITOR (ee), 
				 COMP_EDITOR_PAGE (priv->event_page),
				 _("Appoint_ment"), TRUE);
	g_signal_connect (G_OBJECT (priv->event_page), "client_changed",
			  G_CALLBACK (client_changed_cb), ee);

	priv->recur_window = gtk_dialog_new_with_buttons (_("Recurrence"),
							  (GtkWindow *) ee, GTK_DIALOG_MODAL,
							  "gtk-close", GTK_RESPONSE_CLOSE,
							  NULL);
	g_signal_connect (priv->recur_window, "response", G_CALLBACK (gtk_widget_hide), NULL);
	priv->recur_page = recurrence_page_new ();
	g_object_ref (priv->recur_page);
	gtk_object_sink (GTK_OBJECT (priv->recur_page));
	gtk_container_add ((GtkContainer *) (GTK_DIALOG (priv->recur_window)->vbox), 
			comp_editor_page_get_widget (COMP_EDITOR_PAGE (priv->recur_page)));
	gtk_widget_show_all (priv->recur_window);
	gtk_widget_hide (priv->recur_window);
	comp_editor_append_page (COMP_EDITOR (ee), COMP_EDITOR_PAGE (priv->recur_page), NULL, FALSE);
	if (priv->is_meeting) {

		if (e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_REQ_SEND_OPTIONS))
			event_page_show_options (priv->event_page);

		comp_editor_set_group_item (COMP_EDITOR (ee), TRUE);
		if ((flags & COMP_EDITOR_USER_ORG) || (flags & COMP_EDITOR_DELEGATE)|| (flags & COMP_EDITOR_NEW_ITEM)) {
			priv->sched_window = gtk_dialog_new_with_buttons (_("Free/Busy"),
									  (GtkWindow *) ee, GTK_DIALOG_MODAL,
									  "gtk-close", GTK_RESPONSE_CLOSE,
							  		  NULL);
			priv->sched_page = schedule_page_new (priv->model);
			g_object_ref (priv->sched_page);
			gtk_object_sink (GTK_OBJECT (priv->sched_page));
			gtk_container_add (GTK_CONTAINER (GTK_DIALOG(priv->sched_window)->vbox), comp_editor_page_get_widget (COMP_EDITOR_PAGE (priv->sched_page)));
			gtk_widget_show_all (priv->sched_window);
			gtk_widget_hide (priv->sched_window);

			g_signal_connect (priv->sched_window, "response", G_CALLBACK(gtk_widget_hide), NULL);
			comp_editor_append_page (COMP_EDITOR (ee), COMP_EDITOR_PAGE (priv->sched_page), NULL, FALSE);
	} else
			bonobo_ui_component_set_prop (editor->uic, "/commands/ActionFreeBusy", "hidden", "1", NULL);
		
		event_page_set_meeting (priv->event_page, TRUE);
		priv->meeting_shown=TRUE;
	} else {
		bonobo_ui_component_set_prop (editor->uic, "/commands/ActionFreeBusy", "hidden", "1", NULL);
		bonobo_ui_component_set_prop (editor->uic, "/commands/ViewAttendee", "hidden", "1", NULL);
		bonobo_ui_component_set_prop (editor->uic, "/commands/ViewRole", "hidden", "1", NULL);
		bonobo_ui_component_set_prop (editor->uic, "/commands/ViewRSVP", "hidden", "1", NULL);
		bonobo_ui_component_set_prop (editor->uic, "/commands/ViewType", "hidden", "1", NULL);
		bonobo_ui_component_set_prop (editor->uic, "/commands/ViewStatus", "hidden", "1", NULL);
	}

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
	gboolean delegate;
	ECal *client;
	GSList *attendees = NULL;
	
	ee = EVENT_EDITOR (editor);
	priv = ee->priv;
	
	priv->updating = TRUE;
	delegate = (comp_editor_get_flags (COMP_EDITOR (editor)) & COMP_EDITOR_DELEGATE);
	
	if (COMP_EDITOR_CLASS (event_editor_parent_class)->edit_comp)
		COMP_EDITOR_CLASS (event_editor_parent_class)->edit_comp (editor, comp);

	client = comp_editor_get_e_cal (COMP_EDITOR (editor));

	/* Get meeting related stuff */
	e_cal_component_get_organizer (comp, &organizer);
	e_cal_component_get_attendee_list (comp, &attendees);

	/* Set up the attendees */
	if (attendees != NULL) {
		GSList *l;
		int row;
		char *user_email;
		user_email = itip_get_comp_attendee (comp, client);	
		
		if (!priv->meeting_shown) {
			bonobo_ui_component_set_prop (editor->uic, "/commands/ActionFreeBusy", "hidden", "0", NULL);
		}
		
		if (!(delegate && e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_DELEGATE_TO_MANY))) {
			for (l = attendees; l != NULL; l = l->next) {
				ECalComponentAttendee *ca = l->data;
				EMeetingAttendee *ia;
					
				if (delegate &&	!g_str_equal (itip_strip_mailto (ca->value), user_email))
					continue;
				
				ia = E_MEETING_ATTENDEE (e_meeting_attendee_new_from_e_cal_component_attendee (ca));

				/* If we aren't the organizer or the attendee is just delegated, don't allow editing */
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
		}
	
		event_page_set_meeting (priv->event_page, TRUE);	
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
	
	comp = event_page_get_cancel_comp (priv->event_page);
	if (comp != NULL) {
		ECal *client;
		gboolean result;
		
		client = e_meeting_store_get_e_cal (priv->model);
		result = itip_send_comp (E_CAL_COMPONENT_METHOD_CANCEL, comp,
				client, NULL, NULL);
		g_object_unref (comp);

		if (!result)
			return FALSE;
		else 
			return TRUE;
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

	if (priv->event_page) {
 		g_object_unref (priv->event_page);
		priv->event_page = NULL;
	}
	
	if (priv->recur_page) {
		g_object_unref (priv->recur_page);
		priv->recur_page = NULL;
	}

	if (priv->sched_page) {
		g_object_unref (priv->sched_page);
		priv->sched_page = NULL;
	}

	if (priv->model) {
		g_object_unref (priv->model);
		priv->model = NULL;
	}

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
event_editor_new (ECal *client, CompEditorFlags flags)
{
	EventEditor *ee;

	ee = EVENT_EDITOR (g_object_new (TYPE_EVENT_EDITOR, NULL));
	ee->priv->is_meeting = flags & COMP_EDITOR_MEETING;
	comp_editor_set_flags (COMP_EDITOR (ee), flags);
	return event_editor_construct (ee, client);
}

static void
show_meeting (EventEditor *ee)
{
	EventEditorPrivate *priv;
	CompEditor *editor = COMP_EDITOR (ee);
	CompEditorFlags flags = comp_editor_get_flags (editor);
	
	priv = ee->priv;

	event_page_set_meeting (priv->event_page, TRUE);
	if (!priv->meeting_shown) {
		bonobo_ui_component_set_prop (editor->uic, "/commands/ActionFreeBusy", "hidden", "0", NULL);
	
		priv->meeting_shown = TRUE;

 		comp_editor_set_changed (COMP_EDITOR (ee), FALSE);
		comp_editor_set_needs_send (COMP_EDITOR (ee), priv->meeting_shown);
	}

	if (!(flags & COMP_EDITOR_NEW_ITEM) && !(flags & COMP_EDITOR_USER_ORG))
		gtk_drag_dest_unset (GTK_WIDGET (editor));
}

void
event_editor_show_meeting (EventEditor *ee)
{
	g_return_if_fail (ee != NULL);
	g_return_if_fail (IS_EVENT_EDITOR (ee));

	show_meeting (ee);
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

