/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-tasks.c
 *
 * Copyright (C) 2001-2003  Ximian, Inc.
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
 * Authors: Federico Mena Quintero <federico@ximian.com>
 *	    Damon Chaplin <damon@ximian.com>
 *          Rodrigo Moya <rodrigo@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <gal/e-table/e-table-scrolled.h>
#include <gal/menus/gal-view-instance.h>
#include <gal/menus/gal-view-factory-etable.h>
#include <gal/menus/gal-view-etable.h>

#include "widgets/misc/e-error.h"
#include "e-util/e-categories-config.h"
#include "e-util/e-time-utils.h"
#include "e-util/e-url.h"
#include "shell/e-user-creatable-items-handler.h"
#include <libecal/e-cal-time-util.h>
#include "widgets/menus/gal-view-menus.h"
#include "dialogs/delete-error.h"
#include "dialogs/task-editor.h"
#include "e-calendar-marshal.h"
#include "cal-search-bar.h"
#include "calendar-config.h"
#include "calendar-component.h"
#include "comp-util.h"
#include "e-calendar-table-config.h"
#include "misc.h"
#include "tasks-component.h"
#include "e-cal-component-preview.h"
#include "e-tasks.h"
#include "common/authentication.h"


/* Private part of the GnomeCalendar structure */
struct _ETasksPrivate {
	/* The task lists for display */
	GHashTable *clients;
	GList *clients_list;
	ECal *default_client;

	ECalView *query;
	
	/* The ECalendarTable showing the tasks. */
	GtkWidget   *tasks_view;
	ECalendarTableConfig *tasks_view_config;
	
	/* Calendar search bar for tasks */
	GtkWidget *search_bar;

	/* Paned widget */
	GtkWidget *paned;

	/* The preview */
	GtkWidget *preview;
	
	gchar *current_uid;
	char *sexp;
	guint update_timeout;
	
	/* View instance and the view menus handler */
	GalViewInstance *view_instance;
	GalViewMenus *view_menus;

	GList *notifications;
};

static void setup_widgets (ETasks *tasks);
static void e_tasks_destroy (GtkObject *object);
static void update_view (ETasks *tasks);

static void backend_error_cb (ECal *client, const char *message, gpointer data);

/* Signal IDs */
enum {
	SELECTION_CHANGED,
	SOURCE_ADDED,
	SOURCE_REMOVED,
	LAST_SIGNAL
};

enum DndTargetType {
	TARGET_VCALENDAR
};

static GtkTargetEntry list_drag_types[] = {
	{ "text/calendar",                0, TARGET_VCALENDAR },
	{ "text/x-calendar",              0, TARGET_VCALENDAR }
};
static const int num_list_drag_types = sizeof (list_drag_types) / sizeof (list_drag_types[0]);

static guint e_tasks_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (ETasks, e_tasks, GTK_TYPE_TABLE)

/* Callback used when the cursor changes in the table */
static void
table_cursor_change_cb (ETable *etable, int row, gpointer data)
{
	ETasks *tasks;
	ETasksPrivate *priv;
	ECalModel *model;
	ECalModelComponent *comp_data;
	ECalComponent *comp;
	const char *uid;

	int n_selected;

	tasks = E_TASKS (data);
	priv = tasks->priv;

	n_selected = e_table_selected_count (etable);
	
	/* update the HTML widget */
	if (n_selected != 1) {
		e_cal_component_preview_clear (E_CAL_COMPONENT_PREVIEW (priv->preview));

		return;
	}
	
	model = e_calendar_table_get_model (E_CALENDAR_TABLE (priv->tasks_view));
	
	comp_data = e_cal_model_get_component_at (model, e_table_get_cursor_row (etable));
	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (comp_data->icalcomp));

	e_cal_component_preview_display (E_CAL_COMPONENT_PREVIEW (priv->preview), comp_data->client, comp);
	
	e_cal_component_get_uid (comp, &uid);
	if (priv->current_uid)
		g_free (priv->current_uid);
	priv->current_uid = g_strdup (uid);
	
	g_object_unref (comp);
}

/* Callback used when the selection changes in the table. */
static void
table_selection_change_cb (ETable *etable, gpointer data)
{
	ETasks *tasks;
	int n_selected;

	tasks = E_TASKS (data);

	n_selected = e_table_selected_count (etable);
	gtk_signal_emit (GTK_OBJECT (tasks), e_tasks_signals[SELECTION_CHANGED],
			 n_selected);
}

static void
user_created_cb (GtkWidget *view, ETasks *tasks)
{
	ETasksPrivate *priv;	
	ECal *ecal;
	ECalModel *model;
	
	priv = tasks->priv;

	model = e_calendar_table_get_model (E_CALENDAR_TABLE (priv->tasks_view));
	ecal = e_cal_model_get_default_client (model);

	e_tasks_add_todo_source (tasks, e_cal_get_source (ecal));
}

/* Callback used when the sexp in the search bar changes */
static void
search_bar_sexp_changed_cb (CalSearchBar *cal_search, const char *sexp, gpointer data)
{
	ETasks *tasks;
	ETasksPrivate *priv;

	tasks = E_TASKS (data);
	priv = tasks->priv;

	if (priv->sexp)
		g_free (priv->sexp);
	
	priv->sexp = g_strdup (sexp);
	
	update_view (tasks);
}

/* Callback used when the selected category in the search bar changes */
static void
search_bar_category_changed_cb (CalSearchBar *cal_search, const char *category, gpointer data)
{
	ETasks *tasks;
	ETasksPrivate *priv;
	ECalModel *model;

	tasks = E_TASKS (data);
	priv = tasks->priv;

	model = e_calendar_table_get_model (E_CALENDAR_TABLE (priv->tasks_view));
	e_cal_model_set_default_category (model, category);
}

static gboolean
vpaned_resized_cb (GtkWidget *widget, GdkEventButton *event, ETasks *tasks)
{
	calendar_config_set_task_vpane_pos (gtk_paned_get_position (GTK_PANED (widget)));

	return FALSE;
}

static void
set_timezone (ETasks *tasks) 
{
	ETasksPrivate *priv;
	icaltimezone *zone;
	GList *l;
	
	priv = tasks->priv;
	
	zone = calendar_config_get_icaltimezone ();
	for (l = priv->clients_list; l != NULL; l = l->next) {
		ECal *client = l->data;

		if (e_cal_get_load_state (client) == E_CAL_LOAD_LOADED)
			/* FIXME Error checking */
			e_cal_set_default_timezone (client, zone, NULL);
	}

	if (priv->default_client && e_cal_get_load_state (priv->default_client) == E_CAL_LOAD_LOADED)
		/* FIXME Error checking */
		e_cal_set_default_timezone (priv->default_client, zone, NULL);

	if (priv->preview)
		e_cal_component_preview_set_default_timezone (E_CAL_COMPONENT_PREVIEW (priv->preview), zone);
}

static void
timezone_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	ETasks *tasks = data;
	
	set_timezone (tasks);
}

static void
update_view (ETasks *tasks)
{
	ETasksPrivate *priv;
	ECalModel *model;
	char *real_sexp = NULL;
	char *new_sexp = NULL;
	
	priv = tasks->priv;

	model = e_calendar_table_get_model (E_CALENDAR_TABLE (priv->tasks_view));
		
	if ((new_sexp = calendar_config_get_hide_completed_tasks_sexp()) != NULL) {
		real_sexp = g_strdup_printf ("(and %s %s)", new_sexp, priv->sexp);
		e_cal_model_set_search_query (model, real_sexp);
		g_free (new_sexp);
		g_free (real_sexp);
	} else
		e_cal_model_set_search_query (model, priv->sexp);
}

static gboolean
update_view_cb (ETasks *tasks)
{	
	update_view (tasks);

	return TRUE;
}

static void
config_hide_completed_tasks_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	update_view (data);
}

static void
model_row_changed_cb (ETableModel *etm, int row, gpointer data)
{
	ETasks *tasks;
	ETasksPrivate *priv;
	ECalModelComponent *comp_data;

	tasks = E_TASKS (data);
	priv = tasks->priv;

	if (priv->current_uid) {
		const char *uid;

		comp_data = e_cal_model_get_component_at (E_CAL_MODEL (etm), row);
		if (comp_data) {
			uid = icalcomponent_get_uid (comp_data->icalcomp);
			if (!strcmp (uid ? uid : "", priv->current_uid)) {
				ETable *etable;

				etable = e_table_scrolled_get_table (
					E_TABLE_SCROLLED (E_CALENDAR_TABLE (priv->tasks_view)->etable));
				table_cursor_change_cb (etable, 0, tasks);
			}
		}
	}
}

static void
setup_config (ETasks *tasks)
{
	ETasksPrivate *priv;
	guint not;

	priv = tasks->priv;
	
	/* Timezone */
	set_timezone (tasks);
	
	not = calendar_config_add_notification_timezone (timezone_changed_cb, tasks);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));
	
	not = calendar_config_add_notification_hide_completed_tasks (config_hide_completed_tasks_changed_cb, 
							      tasks);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));
	
	not = calendar_config_add_notification_hide_completed_tasks_units (config_hide_completed_tasks_changed_cb, 
							      tasks);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));
	
	not = calendar_config_add_notification_hide_completed_tasks_value (config_hide_completed_tasks_changed_cb, 
							      tasks);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));	
}

static void
table_drag_data_get (ETable             *table,
		     int                 row,
		     int                 col,
		     GdkDragContext     *context,
		     GtkSelectionData   *selection_data,
		     guint               info,
		     guint               time,
		     ETasks             *tasks)
{
	ETasksPrivate *priv;
	ECalModelComponent *comp_data;

	priv = tasks->priv;

	if (priv->current_uid) {
		ECalModel *model;

		model = e_calendar_table_get_model (E_CALENDAR_TABLE (priv->tasks_view));
		comp_data = e_cal_model_get_component_at (model, row);

		if (info == TARGET_VCALENDAR) {
			/* we will pass an icalcalendar component for both types */
			char *comp_str;
			icalcomponent *vcal;
		
			vcal = e_cal_util_new_top_level ();
			e_cal_util_add_timezones_from_component (vcal, comp_data->icalcomp);
			icalcomponent_add_component (
			        vcal,
				icalcomponent_new_clone (comp_data->icalcomp));

			comp_str = icalcomponent_as_ical_string (vcal);
			if (comp_str) {
				gtk_selection_data_set (selection_data, selection_data->target,
							8, comp_str, strlen (comp_str));
			}
			icalcomponent_free (vcal);
		}
	}
}

/*
static void
table_drag_begin (ETable         *table,
		  int             row,
		  int             col,
		  GdkDragContext *context,
		  ETasks         *tasks)
{

}


static void
table_drag_end (ETable         *table,
		int             row,
		int             col,
		GdkDragContext *context,
		ETasks         *tasks)
{

}
*/

static void 
table_drag_data_delete (ETable         *table,
			int             row,
			int             col,
			GdkDragContext *context,
			ETasks         *tasks)
{
	ETasksPrivate *priv;
	ECalModelComponent *comp_data;
	ECalModel *model;
	gboolean read_only = TRUE;
	
	priv = tasks->priv;
	
	model = e_calendar_table_get_model (E_CALENDAR_TABLE (priv->tasks_view));
	comp_data = e_cal_model_get_component_at (model, row);

	e_cal_is_read_only (comp_data->client, &read_only, NULL);
	if (read_only)
		return;

	e_cal_remove_object (comp_data->client, icalcomponent_get_uid (comp_data->icalcomp), NULL);
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
pane_realized (GtkWidget *widget, ETasks *tasks)
{
	gtk_paned_set_position ((GtkPaned *)widget, calendar_config_get_task_vpane_pos ());
}

static void
setup_widgets (ETasks *tasks)
{
	ETasksPrivate *priv;
	ETable *etable;
	ECalModel *model;

	priv = tasks->priv;

	priv->search_bar = cal_search_bar_new (CAL_SEARCH_TASKS_DEFAULT);
	g_signal_connect (priv->search_bar, "sexp_changed",
			  G_CALLBACK (search_bar_sexp_changed_cb), tasks);
	g_signal_connect (priv->search_bar, "category_changed",
			  G_CALLBACK (search_bar_category_changed_cb), tasks);

	gtk_table_attach (GTK_TABLE (tasks), priv->search_bar, 0, 1, 0, 1,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);
	gtk_widget_show (priv->search_bar);

	/* add the paned widget for the task list and task detail areas */
	priv->paned = gtk_vpaned_new ();
	g_signal_connect (priv->paned, "realize", G_CALLBACK (pane_realized), tasks);

	g_signal_connect (G_OBJECT (priv->paned), "button_release_event",
			  G_CALLBACK (vpaned_resized_cb), tasks);
	gtk_table_attach (GTK_TABLE (tasks), priv->paned, 0, 1, 1, 2,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show (priv->paned);

	/* create the task list */
	priv->tasks_view = e_calendar_table_new ();
	priv->tasks_view_config = e_calendar_table_config_new (E_CALENDAR_TABLE (priv->tasks_view));
	
	g_signal_connect (priv->tasks_view, "user_created", G_CALLBACK (user_created_cb), tasks);

	etable = e_table_scrolled_get_table (
		E_TABLE_SCROLLED (E_CALENDAR_TABLE (priv->tasks_view)->etable));
	e_table_set_state (etable, E_TASKS_TABLE_DEFAULT_STATE);
	gtk_paned_add1 (GTK_PANED (priv->paned), priv->tasks_view);
	gtk_widget_show (priv->tasks_view);


	e_table_drag_source_set (etable, GDK_BUTTON1_MASK,
				 list_drag_types, num_list_drag_types,
				 GDK_ACTION_MOVE|GDK_ACTION_COPY|GDK_ACTION_ASK);
	
	g_signal_connect (etable, "table_drag_data_get",
			  G_CALLBACK(table_drag_data_get), tasks);
	g_signal_connect (etable, "table_drag_data_delete",
			  G_CALLBACK(table_drag_data_delete), tasks);

	/*
	e_table_drag_dest_set (e_table_scrolled_get_table (E_TABLE_SCROLLED (editor->table)),
			       0, list_drag_types, num_list_drag_types, GDK_ACTION_LINK);

	g_signal_connect (e_table_scrolled_get_table (E_TABLE_SCROLLED (editor->table)),
			  "table_drag_motion", G_CALLBACK(table_drag_motion_cb), editor);
	g_signal_connect (e_table_scrolled_get_table (E_TABLE_SCROLLED (editor->table)),
			  "table_drag_drop", G_CALLBACK (table_drag_drop_cb), editor);
	g_signal_connect (e_table_scrolled_get_table (E_TABLE_SCROLLED (editor->table)),
			  "table_drag_data_received", G_CALLBACK(table_drag_data_received_cb), editor);
	*/

	g_signal_connect (etable, "cursor_change", G_CALLBACK (table_cursor_change_cb), tasks);
	g_signal_connect (etable, "selection_change", G_CALLBACK (table_selection_change_cb), tasks);

	/* Timeout check to hide completed items */
	priv->update_timeout = g_timeout_add_full (G_PRIORITY_LOW, 60000, (GSourceFunc) update_view_cb, tasks, NULL);	

	/* create the task detail */
	priv->preview = e_cal_component_preview_new ();
	e_cal_component_preview_set_default_timezone (E_CAL_COMPONENT_PREVIEW (priv->preview), calendar_config_get_icaltimezone ());	
	gtk_paned_add2 (GTK_PANED (priv->paned), priv->preview);
	gtk_widget_show (priv->preview);

	model = e_calendar_table_get_model (E_CALENDAR_TABLE (priv->tasks_view));
	g_signal_connect (G_OBJECT (model), "model_row_changed",
			  G_CALLBACK (model_row_changed_cb), tasks);
}

/* Class initialization function for the gnome calendar */
static void
e_tasks_class_init (ETasksClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	e_tasks_signals[SELECTION_CHANGED] =
		gtk_signal_new ("selection_changed",
				GTK_RUN_LAST,
				G_TYPE_FROM_CLASS (object_class), 
				GTK_SIGNAL_OFFSET (ETasksClass, selection_changed),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_INT);

	e_tasks_signals[SOURCE_ADDED] =
		g_signal_new ("source_added",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (ETasksClass, source_added),
			      NULL, NULL,
			      e_calendar_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_OBJECT);

	e_tasks_signals[SOURCE_REMOVED] =
		g_signal_new ("source_removed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (ETasksClass, source_removed),
			      NULL, NULL,
			      e_calendar_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_OBJECT);

	object_class->destroy = e_tasks_destroy;

	class->selection_changed = NULL;
	class->source_added = NULL;
	class->source_removed = NULL;
}


/* Object initialization function for the gnome calendar */
static void
e_tasks_init (ETasks *tasks)
{
	ETasksPrivate *priv;
	
	priv = g_new0 (ETasksPrivate, 1);
	tasks->priv = priv;

	setup_config (tasks);
	setup_widgets (tasks);

	priv->clients = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);	
	priv->query = NULL;
	priv->view_instance = NULL;
	priv->view_menus = NULL;
	priv->current_uid = NULL;
	priv->sexp = g_strdup ("#t");
	priv->default_client = NULL;
	update_view (tasks);
}

/* Callback used when the set of categories changes in the calendar client */
static void
client_categories_changed_cb (ECal *client, GPtrArray *categories, gpointer data)
{
	ETasks *tasks;
	ETasksPrivate *priv;

	tasks = E_TASKS (data);
	priv = tasks->priv;

	cal_search_bar_set_categories (CAL_SEARCH_BAR (priv->search_bar), categories);
}

GtkWidget *
e_tasks_new (void)
{
	ETasks *tasks;

	tasks = g_object_new (e_tasks_get_type (), NULL);

	return GTK_WIDGET (tasks);
}


void
e_tasks_set_ui_component (ETasks *tasks,
			  BonoboUIComponent *ui_component)
{
	g_return_if_fail (E_IS_TASKS (tasks));
	g_return_if_fail (ui_component == NULL || BONOBO_IS_UI_COMPONENT (ui_component));

	e_search_bar_set_ui_component (E_SEARCH_BAR (tasks->priv->search_bar), ui_component);
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

	if (priv) {
		GList *l;

		/* disconnect from signals on all the clients */
		for (l = priv->clients_list; l != NULL; l = l->next) {
			g_signal_handlers_disconnect_matched (l->data, G_SIGNAL_MATCH_DATA,
							      0, 0, NULL, NULL, tasks);
		}

		g_hash_table_destroy (priv->clients);
		g_list_free (priv->clients_list);

		if (priv->default_client)
			g_object_unref (priv->default_client);
		priv->default_client = NULL;

		if (priv->current_uid) {
			g_free (priv->current_uid);
			priv->current_uid = NULL;
		}
	
		if (priv->sexp) {
			g_free (priv->sexp);
			priv->sexp = NULL;
		}

		if (priv->update_timeout) {
			g_source_remove (priv->update_timeout);
			priv->update_timeout = 0;
		}
	
		if (priv->tasks_view_config) {
			g_object_unref (priv->tasks_view_config);
			priv->tasks_view_config = NULL;
		}
		
		for (l = priv->notifications; l; l = l->next)
			calendar_config_remove_notification (GPOINTER_TO_UINT (l->data));
		priv->notifications = NULL;

		g_free (priv);
		tasks->priv = NULL;
	}

	if (GTK_OBJECT_CLASS (e_tasks_parent_class)->destroy)
		(* GTK_OBJECT_CLASS (e_tasks_parent_class)->destroy) (object);
}

static void
set_status_message (ETasks *tasks, const char *message, ...)
{
	ETasksPrivate *priv;
	va_list args;
	char sz[2048], *msg_string = NULL;

	if (message) {
		va_start (args, message);
		vsnprintf (sz, sizeof sz, message, args);
		va_end (args);
		msg_string = sz;
	}

	priv = tasks->priv;
	
	e_calendar_table_set_status_message (E_CALENDAR_TABLE (priv->tasks_view), msg_string);
}

/* Callback from the calendar client when an error occurs in the backend */
static void
backend_error_cb (ECal *client, const char *message, gpointer data)
{
	ETasks *tasks;
	ETasksPrivate *priv;
	char *errmsg;
	char *urinopwd;

	tasks = E_TASKS (data);
	priv = tasks->priv;

	urinopwd = get_uri_without_password (e_cal_get_uri (client));
	errmsg = g_strdup_printf (_("Error on %s:\n %s"), urinopwd, message);
	gnome_error_dialog_parented (errmsg, GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (tasks))));
	g_free (errmsg);
	g_free (urinopwd);
}

/* Callback from the calendar client when the backend dies */
static void
backend_died_cb (ECal *client, gpointer data)
{
	ETasks *tasks;
	ETasksPrivate *priv;
	ESource *source;
	
	tasks = E_TASKS (data);
	priv = tasks->priv;

	source = g_object_ref (e_cal_get_source (client));

	priv->clients_list = g_list_remove (priv->clients_list, client);
	g_hash_table_remove (priv->clients,  e_source_peek_uid (source));

	gtk_signal_emit (GTK_OBJECT (tasks), e_tasks_signals[SOURCE_REMOVED], source);

	e_calendar_table_set_status_message (E_CALENDAR_TABLE (e_tasks_get_calendar_table (tasks)), NULL);
	
	e_error_run (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (tasks))),
		     "calendar:tasks-crashed", NULL);
	
	g_object_unref (source);
}

/* Callback from the calendar client when the calendar is opened */
static void
client_cal_opened_cb (ECal *ecal, ECalendarStatus status, ETasks *tasks)
{
	ECalModel *model;
	ESource *source;
	ETasksPrivate *priv;

	priv = tasks->priv;

	source = e_cal_get_source (ecal);

	switch (status) {
	case E_CALENDAR_STATUS_OK :
		g_signal_handlers_disconnect_matched (ecal, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, client_cal_opened_cb, NULL);

		set_status_message (tasks, _("Loading tasks"));
		model = e_calendar_table_get_model (E_CALENDAR_TABLE (priv->tasks_view));
		e_cal_model_add_client (model, ecal);

		set_timezone (tasks);
		set_status_message (tasks, NULL);
		break;
	case E_CALENDAR_STATUS_BUSY :
		break;
	default :
		/* Make sure the source doesn't disappear on us */
		g_object_ref (source);

		priv->clients_list = g_list_remove (priv->clients_list, ecal);
		g_signal_handlers_disconnect_matched (ecal, G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, tasks);

		/* Do this last because it unrefs the client */
		g_hash_table_remove (priv->clients,  e_source_peek_uid (source));

		gtk_signal_emit (GTK_OBJECT (tasks), e_tasks_signals[SOURCE_REMOVED], source);

		set_status_message (tasks, NULL);
		g_object_unref (source);

		break;
	}
}

static void
default_client_cal_opened_cb (ECal *ecal, ECalendarStatus status, ETasks *tasks)
{
	ECalModel *model;
	ESource *source;
	ETasksPrivate *priv;

	priv = tasks->priv;

	source = e_cal_get_source (ecal);

	switch (status) {
	case E_CALENDAR_STATUS_OK :
		g_signal_handlers_disconnect_matched (ecal, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, default_client_cal_opened_cb, NULL);
		model = e_calendar_table_get_model (E_CALENDAR_TABLE (priv->tasks_view));
		
		set_timezone (tasks);
		e_cal_model_set_default_client (model, ecal);
		set_status_message (tasks, NULL);
		break;
	case E_CALENDAR_STATUS_BUSY:
		break;
	default :
		/* Make sure the source doesn't disappear on us */
		g_object_ref (source);

		priv->clients_list = g_list_remove (priv->clients_list, ecal);
		g_signal_handlers_disconnect_matched (ecal, G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, tasks);

		/* Do this last because it unrefs the client */
		g_hash_table_remove (priv->clients,  e_source_peek_uid (source));

		gtk_signal_emit (GTK_OBJECT (tasks), e_tasks_signals[SOURCE_REMOVED], source);

		set_status_message (tasks, NULL);
		g_object_unref (priv->default_client);
		priv->default_client = NULL;
		g_object_unref (source);

		break;
	}
}

typedef void (*open_func) (ECal *, ECalendarStatus, ETasks *);

static gboolean
open_ecal (ETasks *tasks, ECal *cal, gboolean only_if_exists, open_func of)
{
	ETasksPrivate *priv;

	priv = tasks->priv;
	
	set_status_message (tasks, _("Opening tasks at %s"), e_cal_get_uri (cal));

	g_signal_connect (G_OBJECT (cal), "cal_opened", G_CALLBACK (of), tasks);
	e_cal_open_async (cal, only_if_exists);

	return TRUE;
}

void
e_tasks_open_task			(ETasks		*tasks)
{
	ECalendarTable *cal_table;
                                                                                
	cal_table = e_tasks_get_calendar_table (tasks);
	e_calendar_table_open_selected (cal_table);
}

void
e_tasks_new_task			(ETasks		*tasks)
{
	ETasksPrivate *priv;
	TaskEditor *tedit;
	ECalComponent *comp;
	const char *category;
	ECal *ecal;
	
	g_return_if_fail (E_IS_TASKS (tasks));

	priv = tasks->priv;

	/* FIXME What to do about no default client */
	ecal = e_tasks_get_default_client (tasks);
	if (!ecal)
		return;
	
	comp = cal_comp_task_new_with_defaults (ecal);

	category = cal_search_bar_get_category (CAL_SEARCH_BAR (priv->search_bar));
	e_cal_component_set_categories (comp, category);

	tedit = task_editor_new (ecal);
	comp_editor_edit_comp (COMP_EDITOR (tedit), comp);
	g_object_unref (comp);

	comp_editor_focus (COMP_EDITOR (tedit));
}

gboolean
e_tasks_add_todo_source (ETasks *tasks, ESource *source)
{
	ETasksPrivate *priv;
	ECal *client;
	const char *uid;

	g_return_val_if_fail (tasks != NULL, FALSE);
	g_return_val_if_fail (E_IS_TASKS (tasks), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);

	priv = tasks->priv;

	uid = e_source_peek_uid (source);
	client = g_hash_table_lookup (priv->clients, uid);
	if (client) {
		/* We already have it */

		return TRUE;
	} else {
		ESource *default_source;
		
		if (priv->default_client) {
			default_source = e_cal_get_source (priv->default_client);
		
			/* We don't have it but the default client is it */
			if (!strcmp (e_source_peek_uid (default_source), uid))
				client = g_object_ref (priv->default_client);
		}

		/* Create a new one */
		if (!client) {
			client = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_TODO);
			if (!client)
				return FALSE;
		}
	}

	g_signal_connect (G_OBJECT (client), "backend_error", G_CALLBACK (backend_error_cb), tasks);
	g_signal_connect (G_OBJECT (client), "categories_changed", G_CALLBACK (client_categories_changed_cb), tasks);
	g_signal_connect (G_OBJECT (client), "backend_died", G_CALLBACK (backend_died_cb), tasks);

	/* add the client to internal structure */	
	g_hash_table_insert (priv->clients, g_strdup (uid) , client);
	priv->clients_list = g_list_prepend (priv->clients_list, client);

	gtk_signal_emit (GTK_OBJECT (tasks), e_tasks_signals[SOURCE_ADDED], source);

	open_ecal (tasks, client, FALSE, client_cal_opened_cb);

	return TRUE;
}

gboolean
e_tasks_remove_todo_source (ETasks *tasks, ESource *source)
{
	ETasksPrivate *priv;
	ECal *client;
	ECalModel *model;
	const char *uid;

	g_return_val_if_fail (tasks != NULL, FALSE);
	g_return_val_if_fail (E_IS_TASKS (tasks), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);

	priv = tasks->priv;

	uid = e_source_peek_uid (source);
	client = g_hash_table_lookup (priv->clients, uid);
	if (!client) 
		return TRUE;
	

	priv->clients_list = g_list_remove (priv->clients_list, client);
	g_signal_handlers_disconnect_matched (client, G_SIGNAL_MATCH_DATA,
					      0, 0, NULL, NULL, tasks);	

	model = e_calendar_table_get_model (E_CALENDAR_TABLE (priv->tasks_view));
	e_cal_model_remove_client (model, client);

	g_hash_table_remove (priv->clients, uid);
       

	gtk_signal_emit (GTK_OBJECT (tasks), e_tasks_signals[SOURCE_REMOVED], source);

	return TRUE;
}

gboolean
e_tasks_set_default_source (ETasks *tasks, ESource *source)
{
	ETasksPrivate *priv;
	ECal *ecal;
	
	g_return_val_if_fail (tasks != NULL, FALSE);
	g_return_val_if_fail (E_IS_TASKS (tasks), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);

	priv = tasks->priv;

	ecal = g_hash_table_lookup (priv->clients, e_source_peek_uid (source));

	if (priv->default_client)
		g_object_unref (priv->default_client);

	if (ecal) {
		priv->default_client = g_object_ref (ecal);
	} else {
		priv->default_client = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_TODO);
		if (!priv->default_client)
			return FALSE;
	}

	open_ecal (tasks, priv->default_client, FALSE, default_client_cal_opened_cb);

	return TRUE;
}

ECal *
e_tasks_get_default_client (ETasks *tasks)
{
	ETasksPrivate *priv;
	
	g_return_val_if_fail (tasks != NULL, NULL);
	g_return_val_if_fail (E_IS_TASKS (tasks), NULL);

	priv = tasks->priv;

	return e_cal_model_get_default_client (e_calendar_table_get_model (E_CALENDAR_TABLE (priv->tasks_view)));	
}

/**
 * e_tasks_complete_selected:
 * @tasks: A tasks control widget
 * 
 * Marks the selected tasks complete
 **/
void
e_tasks_complete_selected (ETasks *tasks)
{
	ETasksPrivate *priv;
	ECalendarTable *cal_table;

	g_return_if_fail (tasks != NULL);
	g_return_if_fail (E_IS_TASKS (tasks));

	priv = tasks->priv;

	cal_table = E_CALENDAR_TABLE (priv->tasks_view);

	set_status_message (tasks, _("Completing tasks..."));
	e_calendar_table_complete_selected (cal_table);
	set_status_message (tasks, NULL);
}

/**
 * e_tasks_delete_selected:
 * @tasks: A tasks control widget.
 * 
 * Deletes the selected tasks in the task list.
 **/
void
e_tasks_delete_selected (ETasks *tasks)
{
	ETasksPrivate *priv;
	ECalendarTable *cal_table;

	g_return_if_fail (tasks != NULL);
	g_return_if_fail (E_IS_TASKS (tasks));

	priv = tasks->priv;

	cal_table = E_CALENDAR_TABLE (priv->tasks_view);
	set_status_message (tasks, _("Deleting selected objects..."));
	e_calendar_table_delete_selected (cal_table);
	set_status_message (tasks, NULL);

	e_cal_component_preview_clear (E_CAL_COMPONENT_PREVIEW (priv->preview));
}

/**
 * e_tasks_expunge:
 * @tasks: A tasks control widget
 * 
 * Removes all tasks marked as completed
 **/
void
e_tasks_delete_completed (ETasks *tasks)
{
	ETasksPrivate *priv;
	char *sexp;
	GList *l;
	
	g_return_if_fail (tasks != NULL);
	g_return_if_fail (E_IS_TASKS (tasks));

	priv = tasks->priv;

	sexp = g_strdup ("(is-completed?)");

	set_status_message (tasks, _("Expunging"));	

	for (l = priv->clients_list; l != NULL; l = l->next) {
		ECal *client = l->data;
		GList *objects, *m;
		gboolean read_only = TRUE;
		
		e_cal_is_read_only (client, &read_only, NULL);
		if (read_only)
			continue;
		
		if (!e_cal_get_object_list (client, sexp, &objects, NULL)) {
			g_warning (G_STRLOC ": Could not get the objects");
			
			continue;
		}
		
		for (m = objects; m; m = m->next) {
			/* FIXME Better error handling */
			e_cal_remove_object (client, icalcomponent_get_uid (m->data), NULL);
		}
	}

	set_status_message (tasks, NULL);

	g_free (sexp);
}

/* Callback used from the view collection when we need to display a new view */
static void
display_view_cb (GalViewInstance *instance, GalView *view, gpointer data)
{
	ETasks *tasks;

	tasks = E_TASKS (data);

	if (GAL_IS_VIEW_ETABLE (view)) {
		gal_view_etable_attach_table (GAL_VIEW_ETABLE (view), e_table_scrolled_get_table (E_TABLE_SCROLLED (E_CALENDAR_TABLE (tasks->priv->tasks_view)->etable)));
	}

	gtk_paned_set_position ((GtkPaned *)tasks->priv->paned, calendar_config_get_task_vpane_pos ());
}

/**
 * e_tasks_setup_view_menus:
 * @tasks: A tasks widget.
 * @uic: UI controller to use for the menus.
 * 
 * Sets up the #GalView menus for a tasks control.  This function should be
 * called from the Bonobo control activation callback for this tasks control.
 * Also, the menus should be discarded using e_tasks_discard_view_menus().
 **/
void
e_tasks_setup_view_menus (ETasks *tasks, BonoboUIComponent *uic)
{
	ETasksPrivate *priv;
	GalViewFactory *factory;
	ETableSpecification *spec;
	char *dir;
	static GalViewCollection *collection = NULL;

	g_return_if_fail (tasks != NULL);
	g_return_if_fail (E_IS_TASKS (tasks));
	g_return_if_fail (uic != NULL);
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (uic));

	priv = tasks->priv;

	g_return_if_fail (priv->view_instance == NULL);

	g_assert (priv->view_instance == NULL);
	g_assert (priv->view_menus == NULL);

	/* Create the view instance */

	if (collection == NULL) {
		collection = gal_view_collection_new ();

		gal_view_collection_set_title (collection, _("Tasks"));

		dir = g_build_filename (tasks_component_peek_base_directory (tasks_component_peek ()), 
					"tasks", "views", NULL);		
		gal_view_collection_set_storage_directories (collection,
							     EVOLUTION_GALVIEWSDIR "/tasks/",
							     dir);
		g_free (dir);

		/* Create the views */

		spec = e_table_specification_new ();
		e_table_specification_load_from_file (spec, 
						      EVOLUTION_ETSPECDIR "/e-calendar-table.etspec");

		factory = gal_view_factory_etable_new (spec);
		g_object_unref (spec);
		gal_view_collection_add_factory (collection, factory);
		g_object_unref (factory);

		/* Load the collection and create the menus */

		gal_view_collection_load (collection);
	}

	priv->view_instance = gal_view_instance_new (collection, NULL);

	priv->view_menus = gal_view_menus_new (priv->view_instance);
	gal_view_menus_apply (priv->view_menus, uic, NULL);
	g_signal_connect (priv->view_instance, "display_view", G_CALLBACK (display_view_cb), tasks);
	display_view_cb (priv->view_instance, gal_view_instance_get_current_view (priv->view_instance), tasks);
}

/**
 * e_tasks_discard_view_menus:
 * @tasks: A tasks widget.
 * 
 * Discards the #GalView menus used by a tasks control.  This function should be
 * called from the Bonobo control deactivation callback for this tasks control.
 * The menus should have been set up with e_tasks_setup_view_menus().
 **/
void
e_tasks_discard_view_menus (ETasks *tasks)
{
	ETasksPrivate *priv;

	g_return_if_fail (tasks != NULL);
	g_return_if_fail (E_IS_TASKS (tasks));

	priv = tasks->priv;

	g_return_if_fail (priv->view_instance != NULL);

	g_assert (priv->view_instance != NULL);
	g_assert (priv->view_menus != NULL);

	g_object_unref (priv->view_instance);
	priv->view_instance = NULL;

	g_object_unref (priv->view_menus);
	priv->view_menus = NULL;
}

/**
 * e_tasks_get_calendar_table:
 * @tasks: A tasks widget.
 * 
 * Queries the #ECalendarTable contained in a tasks widget.
 * 
 * Return value: The #ECalendarTable that the tasks widget uses to display its
 * information.
 **/
ECalendarTable *
e_tasks_get_calendar_table (ETasks *tasks)
{
	ETasksPrivate *priv;

	g_return_val_if_fail (tasks != NULL, NULL);
	g_return_val_if_fail (E_IS_TASKS (tasks), NULL);

	priv = tasks->priv;
	return E_CALENDAR_TABLE (priv->tasks_view);
}
