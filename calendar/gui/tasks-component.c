/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* tasks-component.c
 *
 * Copyright (C) 2003  Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Rodrigo Moya <rodrigo@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <string.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-i18n.h>
#include <bonobo/bonobo-exception.h>
#include <gconf/gconf-client.h>
#include <libecal/e-cal.h>
#include <shell/e-user-creatable-items-handler.h>
#include "e-cal-model.h"
#include "e-tasks.h"
#include "tasks-component.h"
#include "tasks-control.h"
#include "e-comp-editor-registry.h"
#include "migration.h"
#include "comp-util.h"
#include "calendar-config.h"
#include "common/authentication.h"
#include "dialogs/calendar-setup.h"
#include "dialogs/comp-editor.h"
#include "dialogs/copy-source-dialog.h"
#include "dialogs/task-editor.h"
#include "widgets/misc/e-source-selector.h"
#include "widgets/misc/e-info-label.h"
#include "e-util/e-icon-factory.h"

#define CREATE_TASK_ID      "task"
#define CREATE_TASK_LIST_ID "task-list"


#define PARENT_TYPE bonobo_object_get_type ()

static BonoboObjectClass *parent_class = NULL;

/* Tasks should have their own registry */
extern ECompEditorRegistry *comp_editor_registry;


typedef struct 
{
	ESourceList *source_list;
	
	GSList *source_selection;
	
	ETasks *tasks;
	GtkWidget *source_selector;

	BonoboControl *view_control;
	BonoboControl *sidebar_control;
	BonoboControl *statusbar_control;

	GList *notifications;

	EUserCreatableItemsHandler *creatable_items_handler;

	EActivityHandler *activity_handler;
} TasksComponentView;

struct _TasksComponentPrivate {
	char *base_directory;
	char *config_directory;

	ESourceList *source_list;
	GSList *source_selection;

	GList *views;
	
	ECal *create_ecal;
	
	GList *notifications;
};

/* Utility functions.  */
/* FIXME Some of these are duplicated from calendar-component.c */
static gboolean
is_in_selection (GSList *selection, ESource *source)
{
	GSList *l;
	
	for (l = selection; l; l = l->next) {
		ESource *selected_source = l->data;
		
		if (!strcmp (e_source_peek_uid (selected_source), e_source_peek_uid (source)))
			return TRUE;
	}

	return FALSE;
}

static gboolean
is_in_uids (GSList *uids, ESource *source)
{
	GSList *l;
	
	for (l = uids; l; l = l->next) {
		const char *uid = l->data;
		
		if (!strcmp (uid, e_source_peek_uid (source)))
			return TRUE;
	}

	return FALSE;
}

static void
update_uris_for_selection (TasksComponentView *component_view)
{
	GSList *selection, *l, *uids_selected = NULL;
	
	selection = e_source_selector_get_selection (E_SOURCE_SELECTOR (component_view->source_selector));
	
	for (l = component_view->source_selection; l; l = l->next) {
		ESource *old_selected_source = l->data;

		if (!is_in_selection (selection, old_selected_source))
			e_tasks_remove_todo_source (component_view->tasks, old_selected_source);
	}	
	
	for (l = selection; l; l = l->next) {
		ESource *selected_source = l->data;
		
		e_tasks_add_todo_source (component_view->tasks, selected_source);
		uids_selected = g_slist_append (uids_selected, (char *)e_source_peek_uid (selected_source));
	}

	e_source_selector_free_selection (component_view->source_selection);
	component_view->source_selection = selection;

	/* Save the selection for next time we start up */
	calendar_config_set_tasks_selected (uids_selected);
	g_slist_free (uids_selected);
}

static void
update_uri_for_primary_selection (TasksComponentView *component_view)
{
	ESource *source;
	ECalendarTable *cal_table;
	ETable *etable;

	source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (component_view->source_selector));
	if (!source)
		return;

	/* Set the default */
	e_tasks_set_default_source (component_view->tasks, source);

	cal_table = e_tasks_get_calendar_table (component_view->tasks);
	etable = e_calendar_table_get_table (cal_table);

	tasks_control_sensitize_commands (component_view->view_control, component_view->tasks, e_table_selected_count (etable));
	
	/* Save the selection for next time we start up */
	calendar_config_set_primary_tasks (e_source_peek_uid (source));
}

static void
update_selection (TasksComponentView *component_view)
{
	GSList *selection, *uids_selected, *l;

	/* Get the selection in gconf */
	uids_selected = calendar_config_get_tasks_selected ();

	/* Remove any that aren't there any more */
	selection = e_source_selector_get_selection (E_SOURCE_SELECTOR (component_view->source_selector));

	for (l = selection; l; l = l->next) {
		ESource *source = l->data;

		if (!is_in_uids (uids_selected, source)) 
			e_source_selector_unselect_source (E_SOURCE_SELECTOR (component_view->source_selector), source);
	}
	
	e_source_selector_free_selection (selection);

	/* Make sure the whole selection is there */
	for (l = uids_selected; l; l = l->next) {
		char *uid = l->data;
		ESource *source;

		source = e_source_list_peek_source_by_uid (component_view->source_list, uid);
		if (source) 
			e_source_selector_select_source (E_SOURCE_SELECTOR (component_view->source_selector), source);
		
		g_free (uid);
	}
	g_slist_free (uids_selected);
}

static void
update_primary_selection (TasksComponentView *component_view)
{
	ESource *source = NULL;
	char *uid;

	uid = calendar_config_get_primary_tasks ();
	if (uid) {
		source = e_source_list_peek_source_by_uid (component_view->source_list, uid);
		g_free (uid);
	}
	
	if (source) {
		e_source_selector_set_primary_selection (E_SOURCE_SELECTOR (component_view->source_selector), source);
	} else {
		/* Try to create a default if there isn't one */
		source = e_source_list_peek_source_any (component_view->source_list);
		if (source)
			e_source_selector_set_primary_selection (E_SOURCE_SELECTOR (component_view->source_selector), source);
	}
}

/* Callbacks.  */
static void
add_popup_menu_item (GtkMenu *menu, const char *label, const char *icon_name,
		     GCallback callback, gpointer user_data, gboolean sensitive)
{
	GtkWidget *item, *image;
	GdkPixbuf *pixbuf;

	if (icon_name) {
		item = gtk_image_menu_item_new_with_label (label);

		/* load the image */
		pixbuf = e_icon_factory_get_icon (icon_name, 16);
		image = gtk_image_new_from_pixbuf (pixbuf);

		if (image) {
			gtk_widget_show (image);
			gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
		}
	} else {
		item = gtk_menu_item_new_with_label (label);
	}

	if (callback)
		g_signal_connect (G_OBJECT (item), "activate", callback, user_data);

	if (!sensitive)
		gtk_widget_set_sensitive (item, FALSE);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
}

static void
copy_task_list_cb (GtkWidget *widget, TasksComponentView *component_view)
{
	ESource *selected_source;
	
	selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (component_view->source_selector));
	if (!selected_source)
		return;

	copy_source_dialog (GTK_WINDOW (gtk_widget_get_toplevel (widget)), selected_source, E_CAL_SOURCE_TYPE_TODO);
}

static void
delete_task_list_cb (GtkWidget *widget, TasksComponentView *component_view)
{
	ESource *selected_source;
	GtkWidget *dialog;

	selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (component_view->source_selector));
	if (!selected_source)
		return;

	/* create the confirmation dialog */
	dialog = gtk_message_dialog_new (
		GTK_WINDOW (gtk_widget_get_toplevel (widget)),
		GTK_DIALOG_MODAL,
		GTK_MESSAGE_QUESTION,
		GTK_BUTTONS_YES_NO,
		_("Task List '%s' will be removed. Are you sure you want to continue?"),
		e_source_peek_name (selected_source));
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES) {
		ECal *cal;
		char *uri;

		/* first, ask the backend to remove the task list */
		uri = e_source_get_uri (selected_source);
		cal = e_cal_model_get_client_for_uri (
			e_calendar_table_get_model (E_CALENDAR_TABLE (e_tasks_get_calendar_table (component_view->tasks))),
			uri);
		if (!cal)
			cal = e_cal_new_from_uri (uri, E_CAL_SOURCE_TYPE_TODO);
		g_free (uri);
		if (cal) {
			if (e_cal_remove (cal, NULL)) {
				if (e_source_selector_source_is_selected (E_SOURCE_SELECTOR (component_view->source_selector),
									  selected_source)) {
					e_tasks_remove_todo_source (component_view->tasks, selected_source);
					e_source_selector_unselect_source (E_SOURCE_SELECTOR (component_view->source_selector),
									   selected_source);
				}

				e_source_group_remove_source (e_source_peek_group (selected_source), selected_source);
				e_source_list_sync (component_view->source_list, NULL);
			}
		}
	}

	gtk_widget_destroy (dialog);
}

static void
new_task_list_cb (GtkWidget *widget, TasksComponentView *component_view)
{
	calendar_setup_new_task_list (GTK_WINDOW (gtk_widget_get_toplevel (widget)));
}

static void
edit_task_list_cb (GtkWidget *widget, TasksComponentView *component_view)
{
	ESource *selected_source;

	selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (component_view->source_selector));
	if (!selected_source)
		return;

	calendar_setup_edit_task_list (GTK_WINDOW (gtk_widget_get_toplevel (widget)), selected_source);
}

static void
fill_popup_menu_cb (ESourceSelector *selector, GtkMenu *menu, TasksComponentView *component_view)
{
	gboolean sensitive;

	sensitive = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (component_view->source_selector)) ?
		TRUE : FALSE;

	add_popup_menu_item (menu, _("New Task List"), "stock_todo",
			     G_CALLBACK (new_task_list_cb), component_view, TRUE);
	add_popup_menu_item (menu, _("Copy"), "stock_folder-copy",
			     G_CALLBACK (copy_task_list_cb), component_view, sensitive);
	add_popup_menu_item (menu, _("Delete"), "stock_delete", G_CALLBACK (delete_task_list_cb),
			     component_view, sensitive);
	add_popup_menu_item (menu, _("Properties..."), NULL, G_CALLBACK (edit_task_list_cb),
			     component_view, sensitive);
}

static void
source_selection_changed_cb (ESourceSelector *selector, TasksComponentView *component_view)
{
	update_uris_for_selection (component_view);
}

static void
primary_source_selection_changed_cb (ESourceSelector *selector, TasksComponentView *component_view)
{
	update_uri_for_primary_selection (component_view);
}

static void
source_removed_cb (ETasks *tasks, ESource *source, TasksComponentView *component_view)
{
	e_source_selector_unselect_source (E_SOURCE_SELECTOR (component_view->source_selector), source);
}

/* Evolution::Component CORBA methods */

static CORBA_boolean
impl_upgradeFromVersion (PortableServer_Servant servant,
			 CORBA_short major,
			 CORBA_short minor,
			 CORBA_short revision,
			 CORBA_Environment *ev)
{
	TasksComponent *component = TASKS_COMPONENT (bonobo_object_from_servant (servant));

	return migrate_tasks (component, major, minor, revision);
}

static void
control_activate_cb (BonoboControl *control, gboolean activate, gpointer data)
{
	TasksComponentView *component_view = data;

	if (activate) {
		BonoboUIComponent *uic;
		uic = bonobo_control_get_ui_component (component_view->view_control);
		
		e_user_creatable_items_handler_activate (component_view->creatable_items_handler, uic);
	}	
}

static void
config_create_ecal_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{	
	TasksComponent *component = data;
	TasksComponentPrivate *priv;
	
	priv = component->priv;

	g_object_unref (priv->create_ecal);
	priv->create_ecal = NULL;
	
	priv->notifications = g_list_remove (priv->notifications, GUINT_TO_POINTER (id));
}

static ECal *
setup_create_ecal (TasksComponent *component, TasksComponentView *component_view) 
{
	TasksComponentPrivate *priv;
	ESource *source = NULL;
	char *uid;
	guint not;
	
	priv = component->priv;

	if (component_view) {
		ECal *default_ecal;

		default_ecal = e_tasks_get_default_client (component_view->tasks);
		if (default_ecal)
			return default_ecal;
	}
	
	if (priv->create_ecal)
		return priv->create_ecal; 
	
	/* Get the current primary calendar, or try to set one if it doesn't already exist */		
	uid = calendar_config_get_primary_tasks ();
	if (uid) {
		source = e_source_list_peek_source_by_uid (priv->source_list, uid);
		g_free (uid);

		priv->create_ecal = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_TODO);
	} 

	if (!priv->create_ecal) {
		/* Try to create a default if there isn't one */
		source = e_source_list_peek_source_any (priv->source_list);
		if (source)
			priv->create_ecal = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_TODO);
	}
		
	if (priv->create_ecal) {
		if (!e_cal_open (priv->create_ecal, FALSE, NULL)) {
			GtkWidget *dialog;
			
			dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
							 GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
							 _("Unable to open the task list '%s' for creating events and meetings"), 
							   e_source_peek_name (source));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);

			return NULL;
		}
	} else {
		GtkWidget *dialog;
			
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
						 GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
						 _("There is no calendar available for creating tasks"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		return NULL;
	}		

	/* Handle the fact it may change on us */
	not = calendar_config_add_notification_primary_tasks (config_create_ecal_changed_cb, 
							      component);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* Save the primary source for use elsewhere */
	calendar_config_set_primary_tasks (e_source_peek_uid (source));

	return priv->create_ecal ;
}

static gboolean
create_new_todo (TasksComponent *task_component, TasksComponentView *component_view)
{
	ECal *ecal;
	TasksComponentPrivate *priv;
	ECalComponent *comp;
	TaskEditor *editor;
	gboolean read_only;
	
	priv = task_component->priv;
	
	ecal = setup_create_ecal (task_component, component_view);
	if (!ecal)
		return FALSE;

	if (!e_cal_is_read_only (ecal, &read_only, NULL) || read_only) {
		GtkWidget *dialog;
			
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
						 GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
						 _("Selected task list is read-only, events cannot be created. Please select a read-write calendar."));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return TRUE;
	}

	editor = task_editor_new (ecal);
	comp = cal_comp_task_new_with_defaults (ecal);

	comp_editor_edit_comp (COMP_EDITOR (editor), comp);
	comp_editor_focus (COMP_EDITOR (editor));

	e_comp_editor_registry_add (comp_editor_registry, COMP_EDITOR (editor), TRUE);

	return TRUE;
}

static void
create_local_item_cb (EUserCreatableItemsHandler *handler, const char *item_type_name, void *data)
{
	TasksComponent *tasks_component = data;
	TasksComponentPrivate *priv;
	TasksComponentView *component_view = NULL;
	GList *l;
	
	priv = tasks_component->priv;
	
	for (l = priv->views; l; l = l->next) {
		component_view = l->data;

		if (component_view->creatable_items_handler == handler)
			break;
		
		component_view = NULL;
	}
	
	if (strcmp (item_type_name, CREATE_TASK_ID) == 0) {
		create_new_todo (tasks_component, component_view);
	} else if (strcmp (item_type_name, CREATE_TASK_LIST_ID) == 0) {
		calendar_setup_new_task_list (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (component_view->tasks))));
	}
}

static TasksComponentView *
create_component_view (TasksComponent *tasks_component)
{
	TasksComponentPrivate *priv;
	TasksComponentView *component_view;
	GtkWidget *selector_scrolled_window, *vbox, *info;
	GtkWidget *statusbar_widget;
	
	priv = tasks_component->priv;

	/* Create the calendar component view */
	component_view = g_new0 (TasksComponentView, 1);
	
	/* Add the source lists */
	component_view->source_list = g_object_ref (priv->source_list);
	
	/* Create sidebar selector */
	component_view->source_selector = e_source_selector_new (tasks_component->priv->source_list);
	gtk_widget_show (component_view->source_selector);

	selector_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (selector_scrolled_window), component_view->source_selector);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (selector_scrolled_window),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (selector_scrolled_window),
					     GTK_SHADOW_IN);
	gtk_widget_show (selector_scrolled_window);

	info = e_info_label_new("stock_task");
	e_info_label_set_info((EInfoLabel *)info, _("Tasks"), "");
	gtk_widget_show (info);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX (vbox), info, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX (vbox), selector_scrolled_window, TRUE, TRUE, 0);
	gtk_widget_show (vbox);

	component_view->sidebar_control = bonobo_control_new (vbox);

	/* Create main view */
	component_view->view_control = tasks_control_new ();
	if (!component_view->view_control) {
		/* FIXME free memory */

		return NULL;
	}

	component_view->tasks = (ETasks *) bonobo_control_get_widget (component_view->view_control);
	g_signal_connect (component_view->tasks, "source_removed", 
			  G_CALLBACK (source_removed_cb), component_view);

	/* Create status bar */
	statusbar_widget = e_task_bar_new ();
	component_view->activity_handler = e_activity_handler_new ();
	e_activity_handler_attach_task_bar (component_view->activity_handler, E_TASK_BAR (statusbar_widget));
	gtk_widget_show (statusbar_widget);

	component_view->statusbar_control = bonobo_control_new (statusbar_widget);
	
	e_calendar_table_set_activity_handler (e_tasks_get_calendar_table (component_view->tasks), component_view->activity_handler);
	
	/* connect after setting the initial selections, or we'll get unwanted calls
	   to calendar_control_sensitize_calendar_commands */
	g_signal_connect (component_view->source_selector, "selection_changed",
			  G_CALLBACK (source_selection_changed_cb), component_view);
	g_signal_connect (component_view->source_selector, "primary_selection_changed",
			  G_CALLBACK (primary_source_selection_changed_cb), component_view);
	g_signal_connect (component_view->source_selector, "fill_popup_menu",
			  G_CALLBACK (fill_popup_menu_cb), component_view);

	/* Set up the "new" item handler */
	component_view->creatable_items_handler = e_user_creatable_items_handler_new ("tasks", create_local_item_cb, tasks_component);
	g_signal_connect (component_view->view_control, "activate", G_CALLBACK (control_activate_cb), component_view);

	/* Load the selection from the last run */
	update_selection (component_view);	
	update_primary_selection (component_view);

	return component_view;
}

static void
destroy_component_view (TasksComponentView *component_view)
{	
	GList *l;
	
	if (component_view->source_list)
		g_object_unref (component_view->source_list);

	if (component_view->source_selection)
		e_source_selector_free_selection (component_view->source_selection);
	
	for (l = component_view->notifications; l; l = l->next)
		calendar_config_remove_notification (GPOINTER_TO_UINT (l->data));
	g_list_free (component_view->notifications);

	if (component_view->creatable_items_handler)
		g_object_unref (component_view->creatable_items_handler);

	if (component_view->activity_handler)
		g_object_unref (component_view->activity_handler);

	g_free (component_view);
}

static void
view_destroyed_cb (gpointer data, GObject *where_the_object_was)
{
	TasksComponent *tasks_component = data;
	TasksComponentPrivate *priv;
	GList *l;
	
	priv = tasks_component->priv;

	for (l = priv->views; l; l = l->next) {
		TasksComponentView *component_view = l->data;
		
		if (G_OBJECT (component_view->view_control) == where_the_object_was) {
			priv->views = g_list_remove (priv->views, component_view);
			destroy_component_view (component_view);

			break;
		}
	}
}

static void
impl_createControls (PortableServer_Servant servant,
		     Bonobo_Control *corba_sidebar_control,
		     Bonobo_Control *corba_view_control,
		     Bonobo_Control *corba_statusbar_control,
		     CORBA_Environment *ev)
{
	TasksComponent *component = TASKS_COMPONENT (bonobo_object_from_servant (servant));
	TasksComponentPrivate *priv;
	TasksComponentView *component_view;
	
	priv = component->priv;

	/* Create the calendar component view */
	component_view = create_component_view (component);
	if (!component_view) {
		/* FIXME Should we describe the problem in a control? */
		bonobo_exception_set (ev, ex_GNOME_Evolution_Component_Failed);

		return;
	}

	g_object_weak_ref (G_OBJECT (component_view->view_control), view_destroyed_cb, component);
	priv->views = g_list_append (priv->views, component_view);
	
	/* Return the controls */
	*corba_sidebar_control = CORBA_Object_duplicate (BONOBO_OBJREF (component_view->sidebar_control), ev);
	*corba_view_control = CORBA_Object_duplicate (BONOBO_OBJREF (component_view->view_control), ev);
	*corba_statusbar_control = CORBA_Object_duplicate (BONOBO_OBJREF (component_view->statusbar_control), ev);
}

static GNOME_Evolution_CreatableItemTypeList *
impl__get_userCreatableItems (PortableServer_Servant servant,
			      CORBA_Environment *ev)
{
	GNOME_Evolution_CreatableItemTypeList *list = GNOME_Evolution_CreatableItemTypeList__alloc ();

	list->_length  = 2;
	list->_maximum = list->_length;
	list->_buffer  = GNOME_Evolution_CreatableItemTypeList_allocbuf (list->_length);

	CORBA_sequence_set_release (list, FALSE);

	list->_buffer[0].id = CREATE_TASK_ID;
	list->_buffer[0].description = _("New task");
	list->_buffer[0].menuDescription = _("_Task");
	list->_buffer[0].tooltip = _("Create a new task");
	list->_buffer[0].menuShortcut = 't';
	list->_buffer[0].iconName = "stock_task";
	list->_buffer[0].type = GNOME_Evolution_CREATABLE_OBJECT;

	list->_buffer[1].id = CREATE_TASK_LIST_ID;
	list->_buffer[1].description = _("New tasks group");
	list->_buffer[1].menuDescription = _("_Tasks Group");
	list->_buffer[1].tooltip = _("Create a new tasks group");
	list->_buffer[1].menuShortcut = 'n';
	list->_buffer[1].iconName = "stock_todo";
	list->_buffer[1].type = GNOME_Evolution_CREATABLE_FOLDER;

	return list;
}

static void
impl_requestCreateItem (PortableServer_Servant servant,
			const CORBA_char *item_type_name,
			CORBA_Environment *ev)
{
	TasksComponent *tasks_component = TASKS_COMPONENT (bonobo_object_from_servant (servant));
	TasksComponentPrivate *priv;
	
	priv = tasks_component->priv;	
	
	if (strcmp (item_type_name, CREATE_TASK_ID) == 0) {
		if (!create_new_todo (tasks_component, NULL))
			bonobo_exception_set (ev, ex_GNOME_Evolution_Component_Failed);
	} else if (strcmp (item_type_name, CREATE_TASK_LIST_ID) == 0) {
		/* FIXME Should we use the last opened window? */
		calendar_setup_new_task_list (NULL);
	} else {
		bonobo_exception_set (ev, ex_GNOME_Evolution_Component_UnknownType);
	}
}

/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	TasksComponent *tasks_component = TASKS_COMPONENT (object);
	TasksComponentPrivate *priv = tasks_component->priv;
	GList *l;
	
	if (priv->source_list != NULL) {
		g_object_unref (priv->source_list);
		priv->source_list = NULL;
	}
	if (priv->source_selection != NULL) {
		e_source_selector_free_selection (priv->source_selection);
		priv->source_selection = NULL;
	}

	if (priv->create_ecal) {
		g_object_unref (priv->create_ecal);
		priv->create_ecal = NULL;
	}

	for (l = priv->views; l; l = l->next) {
		TasksComponentView *component_view = l->data;
	
		g_object_weak_unref (G_OBJECT (component_view->view_control), view_destroyed_cb, tasks_component);
	}
	g_list_free (priv->views);
	priv->views = NULL;

	for (l = priv->notifications; l; l = l->next)
		calendar_config_remove_notification (GPOINTER_TO_UINT (l->data));
	g_list_free (priv->notifications);
	priv->notifications = NULL;

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	TasksComponentPrivate *priv = TASKS_COMPONENT (object)->priv;
	GList *l;
	
	for (l = priv->views; l; l = l->next) {
		TasksComponentView *component_view = l->data;
		
		destroy_component_view (component_view);
	}
	g_list_free (priv->views);

	g_free (priv->base_directory);
	g_free (priv->config_directory);
	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
tasks_component_class_init (TasksComponentClass *klass)
{
	POA_GNOME_Evolution_Component__epv *epv = &klass->epv;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	epv->upgradeFromVersion      = impl_upgradeFromVersion;
	epv->createControls          = impl_createControls;
	epv->_get_userCreatableItems = impl__get_userCreatableItems;
	epv->requestCreateItem       = impl_requestCreateItem;

	object_class->dispose = impl_dispose;
	object_class->finalize = impl_finalize;
}

static void
tasks_component_init (TasksComponent *component, TasksComponentClass *klass)
{
	TasksComponentPrivate *priv;

	priv = g_new0 (TasksComponentPrivate, 1);

	priv->base_directory = g_build_filename (g_get_home_dir (), ".evolution", NULL);
	priv->config_directory = g_build_filename (g_get_home_dir (),
						   ".evolution", "tasks", "config",
						   NULL);

	if (!e_cal_get_sources (&priv->source_list, E_CAL_SOURCE_TYPE_TODO, NULL))
		;

	component->priv = priv;
}

/* Public API */

TasksComponent *
tasks_component_peek (void)
{
	static TasksComponent *component = NULL;

	if (component == NULL) {
		component = g_object_new (tasks_component_get_type (), NULL);

		if (e_mkdir_hier (component->priv->config_directory, 0777) != 0) {
			g_warning (G_STRLOC ": Cannot create directory %s: %s",
				   component->priv->config_directory, g_strerror (errno));
			g_object_unref (component);
			component = NULL;
		}
	}

	return component;
}

const char *
tasks_component_peek_base_directory (TasksComponent *component)
{
	return component->priv->base_directory;
}

const char *
tasks_component_peek_config_directory (TasksComponent *component)
{
	return component->priv->config_directory;
}

ESourceList *
tasks_component_peek_source_list (TasksComponent *component)
{
	return component->priv->source_list;	
}

BONOBO_TYPE_FUNC_FULL (TasksComponent, GNOME_Evolution_Component, PARENT_TYPE, tasks_component)
