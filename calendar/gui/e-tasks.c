/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-tasks.c
 *
 * Copyright (C) 2001  Helix Code, Inc.
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
 * Authors: Federico Mena Quintero <federico@helixcode.com>
 *	    Damon Chaplin <damon@helixcode.com>
 */

#include <config.h>
#include <gnome.h>
#include <gal/util/e-util.h>
#include <gal/e-table/e-table-scrolled.h>
#include "e-calendar-table.h"
#include "alarm-notify.h"

#include "e-tasks.h"


/* States for the calendar loading and creation state machine */
typedef enum {
	LOAD_STATE_NOT_LOADED,
	LOAD_STATE_WAIT_LOAD,
	LOAD_STATE_WAIT_LOAD_BEFORE_CREATE,
	LOAD_STATE_WAIT_CREATE,
	LOAD_STATE_LOADED
} LoadState;

/* Private part of the GnomeCalendar structure */
struct _ETasksPrivate {
	/* The calendar client object we monitor */
	CalClient   *client;

	/* Loading state; we can be loading or creating a calendar */
	LoadState load_state;

	/* URI being loaded, NULL if we are not being loaded */
	char *loading_uri;

	/* The ECalendarTable showing the tasks. */
	GtkWidget   *tasks_view;
};


static void e_tasks_class_init (ETasksClass *class);
static void e_tasks_init (ETasks *tasks);
static void setup_widgets (ETasks *tasks);
static void e_tasks_destroy (GtkObject *object);

static void cal_loaded_cb (CalClient *client, CalClientLoadStatus status, gpointer data);
static void obj_updated_cb (CalClient *client, const char *uid, gpointer data);
static void obj_removed_cb (CalClient *client, const char *uid, gpointer data);

static GtkTableClass *parent_class;

E_MAKE_TYPE (e_tasks, "ETasks", ETasks,
	     e_tasks_class_init, e_tasks_init,
	     GTK_TYPE_TABLE)


/* Class initialization function for the gnome calendar */
static void
e_tasks_class_init (ETasksClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (GTK_TYPE_TABLE);

	object_class->destroy = e_tasks_destroy;
}


/* Object initialization function for the gnome calendar */
static void
e_tasks_init (ETasks *tasks)
{
	ETasksPrivate *priv;

	priv = g_new0 (ETasksPrivate, 1);
	tasks->priv = priv;

	priv->load_state = LOAD_STATE_NOT_LOADED;

	setup_widgets (tasks);
}


#define E_TASKS_TABLE_DEFAULT_STATE					\
	"<?xml version=\"1.0\"?>"					\
	"<ETableState>"							\
	"<column source=\"13\"/>"					\
	"<column source=\"14\"/>"					\
	"<column source=\"9\"/>"					\
	"<column source=\"5\"/>"					\
	"<grouping/>"							\
	"</ETableState>"

static void
setup_widgets (ETasks *tasks)
{
	ETasksPrivate *priv;
	ETable *etable;

	priv = tasks->priv;

	priv->tasks_view = e_calendar_table_new ();
	etable = e_table_scrolled_get_table (E_TABLE_SCROLLED (E_CALENDAR_TABLE (priv->tasks_view)->etable));
	e_table_set_state (etable, E_TASKS_TABLE_DEFAULT_STATE);
	gtk_table_attach (GTK_TABLE (tasks), priv->tasks_view, 0, 1, 0, 1,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show (priv->tasks_view);

}


GtkWidget *
e_tasks_construct (ETasks *tasks)
{
	ETasksPrivate *priv;

	g_return_val_if_fail (tasks != NULL, NULL);
	g_return_val_if_fail (E_IS_TASKS (tasks), NULL);

	priv = tasks->priv;

	priv->client = cal_client_new ();
	if (!priv->client)
		return NULL;

	gtk_signal_connect (GTK_OBJECT (priv->client), "cal_loaded",
			    GTK_SIGNAL_FUNC (cal_loaded_cb), tasks);
	gtk_signal_connect (GTK_OBJECT (priv->client), "obj_updated",
			    GTK_SIGNAL_FUNC (obj_updated_cb), tasks);
	gtk_signal_connect (GTK_OBJECT (priv->client), "obj_removed",
			    GTK_SIGNAL_FUNC (obj_removed_cb), tasks);

	alarm_notify_add_client (priv->client);

	e_calendar_table_set_cal_client (E_CALENDAR_TABLE (priv->tasks_view),
					 priv->client);

	return GTK_WIDGET (tasks);
}


GtkWidget *
e_tasks_new (void)
{
	ETasks *tasks;

	tasks = gtk_type_new (e_tasks_get_type ());

	if (!e_tasks_construct (tasks)) {
		g_message ("e_tasks_new(): Could not construct the tasks GUI");
		gtk_object_unref (GTK_OBJECT (tasks));
		return NULL;
	}

	return GTK_WIDGET (tasks);
}


static void
e_tasks_destroy (GtkObject *object)
{
	ETasks *tasks;
	ETasksPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_TASKS (object));

	tasks = E_TASKS (object);
	priv = tasks->priv;

	/* Save the ETable layout. FIXME: Need to save in a per-folder config
	   file like the mail folders use. */
#if 0
	filename = g_strdup_printf ("%s/config/TaskPad", evolution_dir);
	e_calendar_table_save_state (E_CALENDAR_TABLE (priv->todo), filename);
	g_free (filename);
#endif

	priv->load_state = LOAD_STATE_NOT_LOADED;

	if (priv->loading_uri) {
		g_free (priv->loading_uri);
		priv->loading_uri = NULL;
	}

	if (priv->client) {
		alarm_notify_remove_client (priv->client);
		gtk_object_unref (GTK_OBJECT (priv->client));
		priv->client = NULL;
	}

	g_free (priv);
	tasks->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


gboolean
e_tasks_open			(ETasks		*tasks,
				 char		*file,
				 ETasksOpenMode	 gcom)
{
	ETasksPrivate *priv;

	g_return_val_if_fail (tasks != NULL, FALSE);
	g_return_val_if_fail (E_IS_TASKS (tasks), FALSE);
	g_return_val_if_fail (file != NULL, FALSE);

	priv = tasks->priv;
	g_return_val_if_fail (priv->load_state == LOAD_STATE_NOT_LOADED,
			      FALSE);

	g_assert (priv->loading_uri == NULL);

	priv->loading_uri = g_strdup (file);

	if (gcom == E_TASKS_OPEN)
		priv->load_state = LOAD_STATE_WAIT_LOAD;
	else if (gcom == E_TASKS_OPEN_OR_CREATE)
		priv->load_state = LOAD_STATE_WAIT_LOAD_BEFORE_CREATE;
	else {
		g_assert_not_reached ();
		return FALSE;
	}

	if (!cal_client_load_calendar (priv->client, file)) {
		priv->load_state = LOAD_STATE_NOT_LOADED;
		g_free (priv->loading_uri);
		priv->loading_uri = NULL;

		g_message ("e_tasks_open(): Could not issue the request");
		return FALSE;
	}

	return TRUE;
}


/* Loads the initial data into the calendar; this should be called right after
 * the cal_loaded signal from the client is invoked.
 */
static void
initial_load (ETasks *tasks)
{
	ETasksPrivate *priv;

	priv = tasks->priv;

	/* FIXME: Do we need to do anything? */
}

/* Displays an error to indicate that loading a calendar failed */
static void
load_error (ETasks *tasks, const char *uri)
{
	char *msg;

	msg = g_strdup_printf (_("Could not load the tasks in `%s'"), uri);
	gnome_error_dialog_parented (msg, GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (tasks))));
	g_free (msg);
}

/* Displays an error to indicate that creating a calendar failed */
static void
create_error (ETasks *tasks, const char *uri)
{
	char *msg;

	msg = g_strdup_printf (_("Could not create a tasks file in `%s'"),
			       uri);
	gnome_error_dialog_parented (msg, GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (tasks))));
	g_free (msg);
}

/* Displays an error to indicate that the specified URI method is not supported */
static void
method_error (ETasks *tasks, const char *uri)
{
	char *msg;

	msg = g_strdup_printf (_("The method required to load `%s' is not supported"), uri);
	gnome_error_dialog_parented (msg, GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (tasks))));
	g_free (msg);
}

/* Callback from the calendar client when a calendar is loaded */
static void
cal_loaded_cb (CalClient *client, CalClientLoadStatus status, gpointer data)
{
	ETasks *tasks;
	ETasksPrivate *priv;
	gboolean free_uri;

	tasks = E_TASKS (data);
	priv = tasks->priv;

	g_assert (priv->load_state != LOAD_STATE_NOT_LOADED
		  && priv->load_state != LOAD_STATE_LOADED);
	g_assert (priv->loading_uri != NULL);

	free_uri = TRUE;

	switch (priv->load_state) {
	case LOAD_STATE_WAIT_LOAD:
		if (status == CAL_CLIENT_LOAD_SUCCESS) {
			priv->load_state = LOAD_STATE_LOADED;
			initial_load (tasks);
		} else if (status == CAL_CLIENT_LOAD_ERROR) {
			priv->load_state = LOAD_STATE_NOT_LOADED;
			load_error (tasks, priv->loading_uri);
		} else if (status == CAL_CLIENT_LOAD_METHOD_NOT_SUPPORTED) {
			priv->load_state = LOAD_STATE_NOT_LOADED;
			method_error (tasks, priv->loading_uri);
		} else
			g_assert_not_reached ();

		break;

	case LOAD_STATE_WAIT_LOAD_BEFORE_CREATE:
		if (status == CAL_CLIENT_LOAD_SUCCESS) {
			priv->load_state = LOAD_STATE_LOADED;
			initial_load (tasks);
		} else if (status == CAL_CLIENT_LOAD_ERROR) {
			priv->load_state = LOAD_STATE_WAIT_CREATE;
			free_uri = FALSE;

			if (!cal_client_create_calendar (priv->client,
							 priv->loading_uri)) {
				priv->load_state = LOAD_STATE_NOT_LOADED;
				free_uri = TRUE;
				g_message ("cal_loaded_cb(): Could not issue the create request");
			}
		} else if (status == CAL_CLIENT_LOAD_METHOD_NOT_SUPPORTED) {
			priv->load_state = LOAD_STATE_NOT_LOADED;
			method_error (tasks, priv->loading_uri);
		} else
			g_assert_not_reached ();

		break;

	case LOAD_STATE_WAIT_CREATE:
		if (status == CAL_CLIENT_LOAD_SUCCESS) {
			priv->load_state = LOAD_STATE_LOADED;
			initial_load (tasks);
		} else if (status == CAL_CLIENT_LOAD_ERROR) {
			priv->load_state = LOAD_STATE_NOT_LOADED;
			create_error (tasks, priv->loading_uri);
		} else if (status == CAL_CLIENT_LOAD_IN_USE) {
			/* Someone created the URI while we were issuing the
			 * create request, so we just try to reload.
			 */
			priv->load_state = LOAD_STATE_WAIT_LOAD;
			free_uri = FALSE;

			if (!cal_client_load_calendar (priv->client,
						       priv->loading_uri)) {
				priv->load_state = LOAD_STATE_NOT_LOADED;
				free_uri = TRUE;
				g_message ("cal_loaded_cb(): Could not issue the load request");
			}
		} else if (status == CAL_CLIENT_LOAD_METHOD_NOT_SUPPORTED) {
			priv->load_state = LOAD_STATE_NOT_LOADED;
			method_error (tasks, priv->loading_uri);
		} else
			g_assert_not_reached ();

		break;

	default:
		g_assert_not_reached ();
	}

	if (free_uri) {
		g_free (priv->loading_uri);
		priv->loading_uri = NULL;
	}
}


/* Callback from the calendar client when an object is updated */
static void
obj_updated_cb (CalClient *client, const char *uid, gpointer data)
{
	ETasks *tasks;
	ETasksPrivate *priv;

	tasks = E_TASKS (data);
	priv = tasks->priv;

	/* FIXME: Do we need to do anything? */
}


/* Callback from the calendar client when an object is removed */
static void
obj_removed_cb (CalClient *client, const char *uid, gpointer data)
{
	ETasks *tasks;
	ETasksPrivate *priv;

	tasks = E_TASKS (data);
	priv = tasks->priv;

	/* FIXME: Do we need to do anything? */
}


/**
 * e_tasks_get_cal_client:
 * @tasks: An #ETasks.
 *
 * Queries the calendar client interface object that a tasks view is using.
 *
 * Return value: A calendar client interface object.
 **/
CalClient *
e_tasks_get_cal_client (ETasks *tasks)
{
	ETasksPrivate *priv;

	g_return_val_if_fail (tasks != NULL, NULL);
	g_return_val_if_fail (E_IS_TASKS (tasks), NULL);

	priv = tasks->priv;

	return priv->client;
}
