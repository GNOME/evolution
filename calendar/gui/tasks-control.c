/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* tasks-control.c
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
 * Authors: Damon Chaplin <damon@helixcode.com>
 *	    Ettore Perazzoli
 */

#include <config.h>
#include <gnome.h>
#include <bonobo.h>
#include <bonobo/bonobo-control.h>
#include "e-tasks.h"
#include "tasks-control.h"


#define TASKS_CONTROL_PROPERTY_URI		"folder_uri"
#define TASKS_CONTROL_PROPERTY_URI_IDX		1


static void tasks_control_properties_init	(BonoboControl		*control,
						 ETasks			*tasks);
static void tasks_control_get_property		(BonoboPropertyBag	*bag,
						 BonoboArg		*arg,
						 guint			 arg_id,
						 CORBA_Environment      *ev,
						 gpointer		 user_data);
static void tasks_control_set_property		(BonoboPropertyBag	*bag,
						 const BonoboArg	*arg,
						 guint			 arg_id,
						 CORBA_Environment      *ev,
						 gpointer		 user_data);
static void tasks_control_activate_cb		(BonoboControl		*control,
						 gboolean		 activate,
						 gpointer		 user_data);
static void tasks_control_activate		(BonoboControl		*control,
						 ETasks			*tasks);
static void tasks_control_deactivate		(BonoboControl		*control);

static void tasks_control_new_task_cmd		(BonoboUIComponent	*uic,
						 gpointer		 data,
						 const char		*path);


BonoboControl *
tasks_control_new			(void)
{
	BonoboControl *control;
	GtkWidget *tasks;

	tasks = e_tasks_new ();
	if (!tasks)
		return NULL;

	gtk_widget_show (tasks);

	control = bonobo_control_new (tasks);
	if (!control) {
		g_message ("control_factory_fn(): could not create the control!");
		return NULL;
	}

	tasks_control_properties_init (control, E_TASKS (tasks));

	gtk_signal_connect (GTK_OBJECT (control), "activate",
			    GTK_SIGNAL_FUNC (tasks_control_activate_cb),
			    tasks);

	return control;
}


/* Creates the property bag for our new control. */
static void
tasks_control_properties_init		(BonoboControl		*control,
					 ETasks			*tasks)
					 
{
	BonoboPropertyBag *pbag;

	pbag = bonobo_property_bag_new (tasks_control_get_property,
					tasks_control_set_property, tasks);

	bonobo_property_bag_add (pbag,
				 TASKS_CONTROL_PROPERTY_URI,
				 TASKS_CONTROL_PROPERTY_URI_IDX,
				 BONOBO_ARG_STRING,
				 NULL,
				 _("The URI of the tasks folder to display"),
				 0);

	bonobo_control_set_properties (control, pbag);
}


/* Gets a property of our control. FIXME: Finish. */
static void
tasks_control_get_property		(BonoboPropertyBag	*bag,
					 BonoboArg		*arg,
					 guint			 arg_id,
					 CORBA_Environment      *ev,
					 gpointer		 user_data)
{
	/*GnomeCalendar *gcal = user_data;*/

	switch (arg_id) {

	case TASKS_CONTROL_PROPERTY_URI_IDX:
		/*
		if (fb && fb->uri)
			BONOBO_ARG_SET_STRING (arg, fb->uri);
		else
			BONOBO_ARG_SET_STRING (arg, "");
		*/
		break;

	default:
		g_warning ("Unhandled arg %d\n", arg_id);
	}
}


static void
tasks_control_set_property		(BonoboPropertyBag	*bag,
					 const BonoboArg	*arg,
					 guint			 arg_id,
					 CORBA_Environment      *ev,
					 gpointer		 user_data)
{
	ETasks *tasks = user_data;
	char *filename;

	switch (arg_id) {

	case TASKS_CONTROL_PROPERTY_URI_IDX:
		filename = g_strdup_printf ("%s/tasks.ics",
					    BONOBO_ARG_GET_STRING (arg));
		e_tasks_open (tasks, filename);
		g_free (filename);
		break;

	default:
		g_warning ("Unhandled arg %d\n", arg_id);
		break;
	}
}


static void
tasks_control_activate_cb		(BonoboControl		*control,
					 gboolean		 activate,
					 gpointer		 user_data)
{
	if (activate)
		tasks_control_activate (control, user_data);
	else
		tasks_control_deactivate (control);
}


static BonoboUIVerb verbs [] = {
	BONOBO_UI_VERB ("TasksNewTask", tasks_control_new_task_cmd),

	BONOBO_UI_VERB_END
};

static void
tasks_control_activate			(BonoboControl		*control,
					 ETasks			*tasks)
{
	Bonobo_UIContainer remote_uih;
	BonoboUIComponent *uic;

	uic = bonobo_control_get_ui_component (control);
	g_assert (uic != NULL);

	remote_uih = bonobo_control_get_remote_ui_container (control);
	bonobo_ui_component_set_container (uic, remote_uih);
	bonobo_object_release_unref (remote_uih, NULL);

	bonobo_ui_component_add_verb_list_with_data (uic, verbs, tasks);

	bonobo_ui_component_freeze (uic, NULL);

	bonobo_ui_util_set_ui (uic, EVOLUTION_DATADIR,
			       "evolution-tasks.xml",
			       "evolution-tasks");

	e_tasks_setup_menus(tasks, uic);

	bonobo_ui_component_thaw (uic, NULL);
}


static void
tasks_control_deactivate		(BonoboControl		*control)
{
	BonoboUIComponent *uic = bonobo_control_get_ui_component (control);
	g_assert (uic != NULL);

	bonobo_ui_component_rm (uic, "/", NULL);
 	bonobo_ui_component_unset_container (uic);
}


static void
tasks_control_new_task_cmd		(BonoboUIComponent	*uic,
					 gpointer		 data,
					 const char		*path)
{
	ETasks *tasks = data;

	e_tasks_new_task (tasks);
}

