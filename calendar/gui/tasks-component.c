/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* tasks-component.c
 *
 * Copyright (C) 2003  Rodrigo Moya
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
#include "e-cal-model.h"
#include "e-tasks.h"
#include "tasks-component.h"
#include "tasks-control.h"
#include "e-comp-editor-registry.h"
#include "migration.h"
#include "comp-util.h"
#include "dialogs/comp-editor.h"
#include "dialogs/task-editor.h"
#include "widgets/misc/e-source-selector.h"


#define CREATE_TASK_ID "task"


#define PARENT_TYPE bonobo_object_get_type ()

static BonoboObjectClass *parent_class = NULL;

/* Tasks should have their own registry */
extern ECompEditorRegistry *comp_editor_registry;

struct _TasksComponentPrivate {
	char *config_directory;
	GConfClient *gconf_client;

	ESourceList *source_list;
	GSList *source_selection;

	ETasks *tasks;
};

/* Utility functions.  */

static void
add_uri_for_source (ESource *source, ETasks *tasks)
{
	char *uri = e_source_get_uri (source);

	e_tasks_add_todo_uri (tasks, uri);
	g_free (uri);
}

static void
remove_uri_for_source (ESource *source, ETasks *tasks)
{
	char *uri = e_source_get_uri (source);

	e_tasks_remove_todo_uri (tasks, uri);
	g_free (uri);
}

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

static void
update_uris_for_selection (ESourceSelector *selector, TasksComponent *component)
{
	TasksComponentPrivate *priv;
	GSList *selection, *l;
	
	selection = e_source_selector_get_selection (selector);

	priv = component->priv;
	
	for (l = priv->source_selection; l; l = l->next) {
		ESource *old_selected_source = l->data;

		if (!is_in_selection (selection, old_selected_source))
			remove_uri_for_source (old_selected_source, E_TASKS (priv->tasks));
	}	
	
	for (l = selection; l; l = l->next) {
		ESource *selected_source = l->data;
		
		add_uri_for_source (selected_source, E_TASKS (priv->tasks));
	}

	e_source_selector_free_selection (priv->source_selection);
	priv->source_selection = selection;
}

/* FIXME This is duplicated from comp-editor-factory.c, should it go in comp-util? */
static ECalComponent *
get_default_task (ECal *ecal)
{
	ECalComponent *comp;
	
	comp = cal_comp_task_new_with_defaults (ecal);

	return comp;
}

/* Callbacks.  */
static void
source_selection_changed_cb (ESourceSelector *selector, TasksComponent *component)
{
	update_uris_for_selection (selector, component);
}

static void
primary_source_selection_changed_cb (ESourceSelector *selector, TasksComponent *component)
{
	TasksComponentPrivate *priv;
	ESource *source;
	ECal *client;
	char *uri;
	ECalModel *model;

	priv = component->priv;
	
	source = e_source_selector_peek_primary_selection (selector);
	if (!source)
		return;

	/* Set the default */
	uri = e_source_get_uri (source);
	model = e_calendar_table_get_model (e_tasks_get_calendar_table (E_TASKS (priv->tasks)));
	client = e_cal_model_get_client_for_uri (model, uri);
	if (client)
		e_cal_model_set_default_client (model, client);

	g_free (uri);

}

/* GObject methods */

static void
impl_dispose (GObject *object)
{
	TasksComponentPrivate *priv = TASKS_COMPONENT (object)->priv;

	if (priv->source_list != NULL) {
		g_object_unref (priv->source_list);
		priv->source_list = NULL;
	}

	if (priv->source_selection != NULL) {
		e_source_selector_free_selection (priv->source_selection);
		priv->source_selection = NULL;
	}

	if (priv->gconf_client != NULL) {
		g_object_unref (priv->gconf_client);
		priv->gconf_client = NULL;
	}

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	TasksComponentPrivate *priv = TASKS_COMPONENT (object)->priv;

	g_free (priv->config_directory);
	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* Evolution::Component CORBA methods */

static void
control_activate_cb (BonoboControl *control, gboolean activate, gpointer data)
{
	ETasks *tasks = data;

	if (activate)
		tasks_control_activate (control, tasks);
	else
		tasks_control_deactivate (control, tasks);
}

static void
impl_createControls (PortableServer_Servant servant,
		     Bonobo_Control *corba_sidebar_control,
		     Bonobo_Control *corba_view_control,
		     CORBA_Environment *ev)
{
	TasksComponent *component = TASKS_COMPONENT (bonobo_object_from_servant (servant));
	TasksComponentPrivate *priv;
	GtkWidget *selector;
	GtkWidget *selector_scrolled_window;
	BonoboControl *sidebar_control, *view_control;

	priv = component->priv;

	/* create sidebar selector */
	selector = e_source_selector_new (priv->source_list);
	gtk_widget_show (selector);

	selector_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (selector_scrolled_window), selector);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (selector_scrolled_window),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (selector_scrolled_window),
					     GTK_SHADOW_IN);
	gtk_widget_show (selector_scrolled_window);

	sidebar_control = bonobo_control_new (selector_scrolled_window);

	/* create the tasks view */
	priv->tasks = E_TASKS (e_tasks_new ());
	if (!priv->tasks) {
		g_warning (G_STRLOC ": could not create the control!");
		bonobo_exception_set (ev, ex_GNOME_Evolution_Component_Failed);
		return;
	}
	
	gtk_widget_show (GTK_WIDGET (priv->tasks));

 	view_control = bonobo_control_new (GTK_WIDGET (priv->tasks));
	if (!view_control) {
		g_warning (G_STRLOC ": could not create the control!");
		bonobo_exception_set (ev, ex_GNOME_Evolution_Component_Failed);
		return;
	}

	g_signal_connect (view_control, "activate", G_CALLBACK (control_activate_cb), priv->tasks);

	g_signal_connect_object (selector, "selection_changed",
				 G_CALLBACK (source_selection_changed_cb),
				 G_OBJECT (component), 0);
	g_signal_connect_object (selector, "primary_selection_changed",
				 G_CALLBACK (primary_source_selection_changed_cb),
				 G_OBJECT (component), 0);

	update_uris_for_selection (E_SOURCE_SELECTOR (selector), component);

	*corba_sidebar_control = CORBA_Object_duplicate (BONOBO_OBJREF (sidebar_control), ev);
	*corba_view_control = CORBA_Object_duplicate (BONOBO_OBJREF (view_control), ev);
}

static GNOME_Evolution_CreatableItemTypeList *
impl__get_userCreatableItems (PortableServer_Servant servant,
			      CORBA_Environment *ev)
{
	GNOME_Evolution_CreatableItemTypeList *list = GNOME_Evolution_CreatableItemTypeList__alloc ();

	list->_length  = 1;
	list->_maximum = list->_length;
	list->_buffer  = GNOME_Evolution_CreatableItemTypeList_allocbuf (list->_length);

	CORBA_sequence_set_release (list, FALSE);

	list->_buffer[0].id = CREATE_TASK_ID;
	list->_buffer[0].description = _("New task");
	list->_buffer[0].menuDescription = _("_Task");
	list->_buffer[0].tooltip = _("Create a new task");
	list->_buffer[0].menuShortcut = 't';
	list->_buffer[0].iconName = "new_task-16.png";

	return list;
}

static void
impl_requestCreateItem (PortableServer_Servant servant,
			const CORBA_char *item_type_name,
			CORBA_Environment *ev)
{
	TasksComponent *tasks_component = TASKS_COMPONENT (bonobo_object_from_servant (servant));
	TasksComponentPrivate *priv;
	ECal *ecal;
	ECalComponent *comp;
	TaskEditor *editor;
	
	priv = tasks_component->priv;
	
	ecal = e_tasks_get_default_client (E_TASKS (priv->tasks));
	if (!ecal) {
		/* FIXME We should display a gui dialog or something */
		bonobo_exception_set (ev, ex_GNOME_Evolution_Component_UnknownType);
		g_warning (G_STRLOC ": No default client");
	}
		
	editor = task_editor_new (ecal);
	
	if (strcmp (item_type_name, CREATE_TASK_ID) == 0) {
		comp = get_default_task (ecal);
	} else {
		bonobo_exception_set (ev, ex_GNOME_Evolution_Component_UnknownType);
		return;
	}	

	comp_editor_edit_comp (COMP_EDITOR (editor), comp);
	comp_editor_focus (COMP_EDITOR (editor));

	e_comp_editor_registry_add (comp_editor_registry, COMP_EDITOR (editor), TRUE);
}

/* Initialization */

static void
tasks_component_class_init (TasksComponentClass *klass)
{
	POA_GNOME_Evolution_Component__epv *epv = &klass->epv;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

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
	GSList *groups;

	priv = g_new0 (TasksComponentPrivate, 1);
	priv->config_directory = g_build_filename (g_get_home_dir (),
						   ".evolution", "tasks", "config",
						   NULL);

	/* load the groups from the configuration */
	priv->gconf_client = gconf_client_get_default ();
	priv->source_list = e_source_list_new_for_gconf (priv->gconf_client,
							 "/apps/evolution/tasks/sources");

	/* create default tasks folders if there are no groups */
	groups = e_source_list_peek_groups (priv->source_list);
	if (!groups) {
		ESourceGroup *group;
		ESource *source;
		char *base_uri, *new_dir;

		/* create the source group */
		base_uri = g_build_filename (g_get_home_dir (),
					     ".evolution/tasks/local/OnThisComputer/",
					     NULL);
		group = e_source_group_new (_("On This Computer"), base_uri);
		e_source_list_add_group (priv->source_list, group, -1);

		/* migrate tasks from older setup */
		if (!migrate_old_tasks (group)) {
			/* create default tasks folders */
			new_dir = g_build_filename (base_uri, "Personal/", NULL);
			if (!e_mkdir_hier (new_dir, 0700)) {
				source = e_source_new (_("Personal"), "Personal");
				e_source_group_add_source (group, source, -1);
			}

			g_free (new_dir);
		}

		g_free (base_uri);
	}

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
tasks_component_peek_config_directory (TasksComponent *component)
{
	return (const char *) component->priv->config_directory;
}

BONOBO_TYPE_FUNC_FULL (TasksComponent, GNOME_Evolution_Component, PARENT_TYPE, tasks_component)
