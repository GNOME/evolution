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
#include "dialogs/task-editor.h"
#include "e-calendar-table.h"
#include "alarm-notify.h"
#include "component-factory.h"

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

	/* URI of the folder being shown. */
	char *folder_uri;

	/* The ECalendarTable showing the tasks. */
	GtkWidget   *tasks_view;

	/* The option menu showing the categories, and the popup menu. */
	GtkWidget *categories_option_menu;
	GtkWidget *categories_menu;
};


static void e_tasks_class_init (ETasksClass *class);
static void e_tasks_init (ETasks *tasks);
static void setup_widgets (ETasks *tasks);
static void e_tasks_destroy (GtkObject *object);

static void cal_loaded_cb (CalClient *client, CalClientLoadStatus status, gpointer data);
static void obj_updated_cb (CalClient *client, const char *uid, gpointer data);
static void obj_removed_cb (CalClient *client, const char *uid, gpointer data);

static char* e_tasks_get_config_filename (ETasks *tasks);

static void e_tasks_on_filter_selected	(GtkMenuShell	*menu_shell,
					 ETasks		*tasks);
static void e_tasks_on_categories_changed	(CalendarModel	*model,
						 ETasks		*tasks);
static void e_tasks_rebuild_categories_menu	(ETasks		*tasks);
static gint e_tasks_add_menu_item		(gpointer key,
						 gpointer value,
						 gpointer data);


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
	GtkWidget *hbox, *menuitem, *categories_label;

	priv = tasks->priv;

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_table_attach (GTK_TABLE (tasks), hbox, 0, 1, 0, 1,
			  GTK_EXPAND | GTK_FILL, 0, 0, 0);

	priv->categories_option_menu = gtk_option_menu_new ();
	gtk_widget_show (priv->categories_option_menu);
	gtk_box_pack_end (GTK_BOX (hbox), priv->categories_option_menu,
			  FALSE, FALSE, 0);

	priv->categories_menu = gtk_menu_new ();

	menuitem = gtk_menu_item_new_with_label (_("All"));
	gtk_widget_show (menuitem);
	gtk_menu_append (GTK_MENU (priv->categories_menu), menuitem);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (priv->categories_option_menu), priv->categories_menu);

	categories_label = gtk_label_new (_("Category:"));
	gtk_widget_show (categories_label);
	gtk_box_pack_end (GTK_BOX (hbox), categories_label, FALSE, FALSE, 4);


	priv->tasks_view = e_calendar_table_new ();
	etable = e_table_scrolled_get_table (E_TABLE_SCROLLED (E_CALENDAR_TABLE (priv->tasks_view)->etable));
	e_table_set_state (etable, E_TASKS_TABLE_DEFAULT_STATE);
	gtk_table_attach (GTK_TABLE (tasks), priv->tasks_view, 0, 1, 1, 2,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show (priv->tasks_view);

	gtk_signal_connect (GTK_OBJECT (E_CALENDAR_TABLE (priv->tasks_view)->model), "categories-changed", GTK_SIGNAL_FUNC (e_tasks_on_categories_changed), tasks);
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
	char *config_filename;

	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_TASKS (object));

	tasks = E_TASKS (object);
	priv = tasks->priv;

	/* Save the ETable layout. */
	config_filename = e_tasks_get_config_filename (tasks);
	e_calendar_table_save_state (E_CALENDAR_TABLE (priv->tasks_view),
				     config_filename);
	g_free (config_filename);

	priv->load_state = LOAD_STATE_NOT_LOADED;

	if (priv->folder_uri) {
		g_free (priv->folder_uri);
		priv->folder_uri = NULL;
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
	char *config_filename;

	g_return_val_if_fail (tasks != NULL, FALSE);
	g_return_val_if_fail (E_IS_TASKS (tasks), FALSE);
	g_return_val_if_fail (file != NULL, FALSE);

	priv = tasks->priv;
	g_return_val_if_fail (priv->load_state == LOAD_STATE_NOT_LOADED,
			      FALSE);

	g_assert (priv->folder_uri == NULL);

	priv->folder_uri = g_strdup (file);

	if (gcom == E_TASKS_OPEN)
		priv->load_state = LOAD_STATE_WAIT_LOAD;
	else if (gcom == E_TASKS_OPEN_OR_CREATE)
		priv->load_state = LOAD_STATE_WAIT_LOAD_BEFORE_CREATE;
	else {
		g_assert_not_reached ();
		return FALSE;
	}

	config_filename = e_tasks_get_config_filename (tasks);
	e_calendar_table_load_state (E_CALENDAR_TABLE (priv->tasks_view),
				     config_filename);
	g_free (config_filename);


	if (!cal_client_load_calendar (priv->client, file)) {
		priv->load_state = LOAD_STATE_NOT_LOADED;
		g_free (priv->folder_uri);
		priv->folder_uri = NULL;

		g_message ("e_tasks_open(): Could not issue the request");
		return FALSE;
	}

	return TRUE;
}


/* Loads the initial data into the calendar; this should be called right after
 * the cal_loaded signal from the client is invoked.
 */
static void
initial_load				(ETasks		*tasks)
{
	ETasksPrivate *priv;

	priv = tasks->priv;

	/* FIXME: Do we need to do anything? */
}

/* Displays an error to indicate that loading a calendar failed */
static void
load_error				(ETasks		*tasks,
					 const char	*uri)
{
	char *msg;

	msg = g_strdup_printf (_("Could not load the tasks in `%s'"), uri);
	gnome_error_dialog_parented (msg, GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (tasks))));
	g_free (msg);
}

/* Displays an error to indicate that creating a calendar failed */
static void
create_error				(ETasks		*tasks,
					 const char	*uri)
{
	char *msg;

	msg = g_strdup_printf (_("Could not create a tasks file in `%s'"),
			       uri);
	gnome_error_dialog_parented (msg, GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (tasks))));
	g_free (msg);
}

/* Displays an error to indicate that the specified URI method is not supported */
static void
method_error				(ETasks		*tasks,
					 const char	*uri)
{
	char *msg;

	msg = g_strdup_printf (_("The method required to load `%s' is not supported"), uri);
	gnome_error_dialog_parented (msg, GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (tasks))));
	g_free (msg);
}

/* Callback from the calendar client when a calendar is loaded */
static void
cal_loaded_cb				(CalClient	*client,
					 CalClientLoadStatus status,
					 gpointer	 data)
{
	ETasks *tasks;
	ETasksPrivate *priv;
	gboolean free_uri;

	tasks = E_TASKS (data);
	priv = tasks->priv;

	g_assert (priv->load_state != LOAD_STATE_NOT_LOADED
		  && priv->load_state != LOAD_STATE_LOADED);
	g_assert (priv->folder_uri != NULL);

	free_uri = TRUE;

	switch (priv->load_state) {
	case LOAD_STATE_WAIT_LOAD:
		if (status == CAL_CLIENT_LOAD_SUCCESS) {
			priv->load_state = LOAD_STATE_LOADED;
			initial_load (tasks);
		} else if (status == CAL_CLIENT_LOAD_ERROR) {
			priv->load_state = LOAD_STATE_NOT_LOADED;
			load_error (tasks, priv->folder_uri);
		} else if (status == CAL_CLIENT_LOAD_METHOD_NOT_SUPPORTED) {
			priv->load_state = LOAD_STATE_NOT_LOADED;
			method_error (tasks, priv->folder_uri);
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
							 priv->folder_uri)) {
				priv->load_state = LOAD_STATE_NOT_LOADED;
				free_uri = TRUE;
				g_message ("cal_loaded_cb(): Could not issue the create request");
			}
		} else if (status == CAL_CLIENT_LOAD_METHOD_NOT_SUPPORTED) {
			priv->load_state = LOAD_STATE_NOT_LOADED;
			method_error (tasks, priv->folder_uri);
		} else
			g_assert_not_reached ();

		break;

	case LOAD_STATE_WAIT_CREATE:
		if (status == CAL_CLIENT_LOAD_SUCCESS) {
			priv->load_state = LOAD_STATE_LOADED;
			initial_load (tasks);
		} else if (status == CAL_CLIENT_LOAD_ERROR) {
			priv->load_state = LOAD_STATE_NOT_LOADED;
			create_error (tasks, priv->folder_uri);
		} else if (status == CAL_CLIENT_LOAD_IN_USE) {
			/* Someone created the URI while we were issuing the
			 * create request, so we just try to reload.
			 */
			priv->load_state = LOAD_STATE_WAIT_LOAD;
			free_uri = FALSE;

			if (!cal_client_load_calendar (priv->client,
						       priv->folder_uri)) {
				priv->load_state = LOAD_STATE_NOT_LOADED;
				free_uri = TRUE;
				g_message ("cal_loaded_cb(): Could not issue the load request");
			}
		} else if (status == CAL_CLIENT_LOAD_METHOD_NOT_SUPPORTED) {
			priv->load_state = LOAD_STATE_NOT_LOADED;
			method_error (tasks, priv->folder_uri);
		} else
			g_assert_not_reached ();

		break;

	default:
		g_assert_not_reached ();
	}
}


/* Callback from the calendar client when an object is updated */
static void
obj_updated_cb				(CalClient	*client,
					 const char	*uid,
					 gpointer	 data)
{
	ETasks *tasks;
	ETasksPrivate *priv;

	tasks = E_TASKS (data);
	priv = tasks->priv;

	/* FIXME: Do we need to do anything? */
}


/* Callback from the calendar client when an object is removed */
static void
obj_removed_cb				(CalClient	*client,
					 const char	*uid,
					 gpointer	 data)
{
	ETasks *tasks;
	ETasksPrivate *priv;

	tasks = E_TASKS (data);
	priv = tasks->priv;

	/* FIXME: Do we need to do anything? */
}


static char*
e_tasks_get_config_filename		(ETasks		*tasks)
{
	ETasksPrivate *priv;
	char *url, *filename;

	priv = tasks->priv;

	url = g_strdup (priv->folder_uri);

	/* This turns all funny characters into '_', in the string itself. */
	e_filename_make_safe (url);
	
	filename = g_strdup_printf ("%s/config/et-header-%s", evolution_dir,
				    url);
	g_free (url);
	
	return filename;
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
e_tasks_get_cal_client			(ETasks		*tasks)
{
	ETasksPrivate *priv;

	g_return_val_if_fail (E_IS_TASKS (tasks), NULL);

	priv = tasks->priv;

	return priv->client;
}


void
e_tasks_new_task			(ETasks		*tasks)
{
	ETasksPrivate *priv;
	TaskEditor *tedit;
	CalComponent *comp;

	g_return_if_fail (E_IS_TASKS (tasks));

	priv = tasks->priv;

	tedit = task_editor_new ();
	task_editor_set_cal_client (tedit, priv->client);

	comp = cal_component_new ();
	cal_component_set_new_vtype (comp, CAL_COMPONENT_TODO);

	task_editor_set_todo_object (tedit, comp);

	gtk_object_unref (GTK_OBJECT (comp));
}


static void
e_tasks_on_filter_selected		(GtkMenuShell	*menu_shell,
					 ETasks		*tasks)
{
	ETasksPrivate *priv;
	ECalendarTable *cal_table;
	CalendarModel *model;
	GtkWidget *label;
	char *category;

	g_return_if_fail (E_IS_TASKS (tasks));

	priv = tasks->priv;

	label = GTK_BIN (priv->categories_option_menu)->child;
	gtk_label_get (GTK_LABEL (label), &category);

	cal_table = E_CALENDAR_TABLE (priv->tasks_view);
	model = cal_table->model;

	g_print ("!#!#!#!#!# filter selected: %s\n", category);

	if (!strcmp (category, _("All"))) {
		calendar_model_set_default_category (model, NULL);
		e_calendar_table_set_filter_func (cal_table, NULL, NULL,
						  NULL);
	} else {
		calendar_model_set_default_category (model, category);
		e_calendar_table_set_filter_func (cal_table,
						  e_calendar_table_filter_by_category,
						  g_strdup (category), g_free);
	}
}


static void
e_tasks_on_categories_changed	(CalendarModel	*model,
				 ETasks		*tasks)
{
	g_print ("In e_tasks_on_categories_changed\n");

	e_tasks_rebuild_categories_menu (tasks);
}


static void
e_tasks_rebuild_categories_menu	(ETasks		*tasks)
{
	ETasksPrivate *priv;
	CalendarModel *model;
	GTree *categories;
	GtkWidget *menuitem;

	priv = tasks->priv;

	priv->categories_menu = gtk_menu_new ();

	menuitem = gtk_menu_item_new_with_label (_("All"));
	gtk_widget_show (menuitem);
	gtk_menu_append (GTK_MENU (priv->categories_menu), menuitem);

	model = E_CALENDAR_TABLE (priv->tasks_view)->model;
	categories = calendar_model_get_categories (model);
	g_return_if_fail (categories != NULL);

	g_tree_traverse (categories, e_tasks_add_menu_item, G_IN_ORDER,
			 priv->categories_menu);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (priv->categories_option_menu), priv->categories_menu);

	gtk_signal_connect (GTK_OBJECT (priv->categories_menu), "deactivate",
			    GTK_SIGNAL_FUNC (e_tasks_on_filter_selected),
			    tasks);
}


static gint
e_tasks_add_menu_item		(gpointer key,
				 gpointer value,
				 gpointer data)
{
	GtkWidget *menuitem;

	menuitem = gtk_menu_item_new_with_label ((char*) key);
	gtk_widget_show (menuitem);
	gtk_menu_append (GTK_MENU (data), menuitem);

	return FALSE;
}

