/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Federico Mena Quintero <federico@ximian.com>
 *	    Damon Chaplin <damon@ximian.com>
 *      Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <libedataserver/e-time-utils.h>
#include <table/e-table-scrolled.h>
#include <widgets/menus/gal-view-instance.h>
#include <widgets/menus/gal-view-factory-etable.h>
#include <widgets/menus/gal-view-etable.h>

#include "e-util/e-error.h"
#include "e-util/e-categories-config.h"
#include "e-util/e-util-private.h"
#include "shell/e-user-creatable-items-handler.h"
#include <libedataserver/e-url.h>
#include <libedataserver/e-categories.h>
#include <libecal/e-cal-time-util.h>
#include "widgets/menus/gal-view-menus.h"
#include "dialogs/delete-error.h"
#include "dialogs/task-editor.h"
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
#include "e-cal-menu.h"
#include "e-cal-model-tasks.h"

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

	/* Tasks menu */
	ECalMenu *tasks_menu;

	/* Paned widget */
	GtkWidget *paned;

	/* The preview */
	GtkWidget *preview;

	gchar *current_uid;
	gchar *sexp;
	guint update_timeout;

	/* View instance and the view menus handler */
	GalViewInstance *view_instance;
	GalViewMenus *view_menus;

	GList *notifications;
};

static void setup_widgets (ETasks *tasks);
static void e_tasks_destroy (GtkObject *object);
static void update_view (ETasks *tasks);

static void categories_changed_cb (gpointer object, gpointer user_data);
static void backend_error_cb (ECal *client, const gchar *message, gpointer data);

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
	{ (gchar *) "text/calendar",   0, TARGET_VCALENDAR },
	{ (gchar *) "text/x-calendar", 0, TARGET_VCALENDAR }
};
static const gint num_list_drag_types = sizeof (list_drag_types) / sizeof (list_drag_types[0]);

static guint e_tasks_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (ETasks, e_tasks, GTK_TYPE_TABLE)

/* Callback used when the cursor changes in the table */
static void
table_cursor_change_cb (ETable *etable, gint row, gpointer data)
{
	ETasks *tasks;
	ETasksPrivate *priv;
	ECalModel *model;
	ECalModelComponent *comp_data;
	ECalComponent *comp;
	const gchar *uid;

	gint n_selected;

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

ECalMenu *
e_tasks_get_tasks_menu (ETasks *tasks)
{
        g_return_val_if_fail (E_IS_TASKS (tasks), NULL);

        return tasks->priv->tasks_menu;
}

/* Callback used when the selection changes in the table. */
static void
table_selection_change_cb (ETable *etable, gpointer data)
{
	ETasks *tasks;
	gint n_selected;

	tasks = E_TASKS (data);

	n_selected = e_table_selected_count (etable);
	g_signal_emit (tasks, e_tasks_signals[SELECTION_CHANGED], 0, n_selected);

	if (n_selected != 1)
		e_cal_component_preview_clear (E_CAL_COMPONENT_PREVIEW (tasks->priv->preview));
}

static void
user_created_cb (GtkWidget *view, ETasks *tasks)
{
	ETasksPrivate *priv;
	ECalendarTable *cal_table;
	ECal *ecal;

	priv = tasks->priv;
	cal_table = E_CALENDAR_TABLE (priv->tasks_view);

	if (cal_table->user_created_cal)
		ecal = cal_table->user_created_cal;
	else {
		ECalModel *model;

		model = e_calendar_table_get_model (E_CALENDAR_TABLE (priv->tasks_view));
		ecal = e_cal_model_get_default_client (model);
	}

	e_tasks_add_todo_source (tasks, e_cal_get_source (ecal));
}

/* Callback used when the sexp in the search bar changes */
static void
search_bar_sexp_changed_cb (CalSearchBar *cal_search, const gchar *sexp, gpointer data)
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
search_bar_category_changed_cb (CalSearchBar *cal_search, const gchar *category, gpointer data)
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
		/* FIXME Error checking */
		e_cal_set_default_timezone (client, zone, NULL);
	}

	if (priv->default_client)
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
	gchar *real_sexp = NULL;
	gchar *new_sexp = NULL;

	priv = tasks->priv;

	model = e_calendar_table_get_model (E_CALENDAR_TABLE (priv->tasks_view));

	if ((new_sexp = calendar_config_get_hide_completed_tasks_sexp (FALSE)) != NULL) {
		real_sexp = g_strdup_printf ("(and %s %s)", new_sexp, priv->sexp);
		e_cal_model_set_search_query (model, real_sexp);
		g_free (new_sexp);
		g_free (real_sexp);
	} else
		e_cal_model_set_search_query (model, priv->sexp);

	e_cal_component_preview_clear (E_CAL_COMPONENT_PREVIEW (priv->preview));
}

static void
process_completed_tasks (ETasks *tasks, gboolean config_changed)
{
	ETasksPrivate *priv;

	g_return_if_fail (tasks != NULL);
	g_return_if_fail (E_IS_TASKS (tasks));

	priv = tasks->priv;

	e_calendar_table_process_completed_tasks (e_tasks_get_calendar_table (tasks), priv->clients_list, config_changed);
}

static gboolean
update_view_cb (ETasks *tasks)
{
	ECalModel *model;

	model = e_calendar_table_get_model (E_CALENDAR_TABLE (tasks->priv->tasks_view));

	process_completed_tasks (tasks, FALSE);
	e_cal_model_tasks_update_due_tasks (E_CAL_MODEL_TASKS (model));

	return TRUE;
}

static void
config_hide_completed_tasks_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	process_completed_tasks (data, TRUE);
	update_view (data);
}

static void
model_row_changed_cb (ETableModel *etm, gint row, gpointer data)
{
	ETasks *tasks;
	ETasksPrivate *priv;
	ECalModelComponent *comp_data;

	tasks = E_TASKS (data);
	priv = tasks->priv;

	if (priv->current_uid) {
		const gchar *uid;

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
view_progress_cb (ECalModel *model, const gchar *message, gint percent, ECalSourceType type, ETasks *tasks)
{
	e_calendar_table_set_status_message (E_CALENDAR_TABLE (e_tasks_get_calendar_table (tasks)),
			message, percent);
}

static void
view_done_cb (ECalModel *model, ECalendarStatus status, ECalSourceType type, ETasks *tasks)
{
	e_calendar_table_set_status_message (E_CALENDAR_TABLE (e_tasks_get_calendar_table (tasks)),
			NULL, -1);

}

static void
config_preview_state_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	gboolean state;
	GConfValue *value;
	ETasks *tasks = (ETasks *)data;

	g_return_if_fail (gconf_entry_get_key (entry) != NULL);
	g_return_if_fail ((value = gconf_entry_get_value (entry)) != NULL);

	state = gconf_value_get_bool (value);
	e_tasks_show_preview (tasks, state);
	bonobo_ui_component_set_prop (E_SEARCH_BAR (tasks->priv->search_bar)->ui_component, "/commands/ViewPreview", "state", state ? "1" : "0", NULL);
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

	not = calendar_config_add_notification_preview_state (config_preview_state_changed_cb, tasks);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));
}

struct AffectedComponents {
	ECalendarTable *cal_table;
	GSList *components; /* contains pointers to ECalModelComponent */
};

/**
 * get_selected_components_cb
 * Helper function to fill list of selected components in ECalendarTable.
 * This function is called from e_table_selected_row_foreach.
 **/
static void
get_selected_components_cb (gint model_row, gpointer data)
{
	struct AffectedComponents *ac = (struct AffectedComponents *) data;

	if (!ac || !ac->cal_table)
		return;

	ac->components = g_slist_prepend (ac->components, e_cal_model_get_component_at (E_CAL_MODEL (ac->cal_table->model), model_row));
}

/**
 * do_for_selected_components
 * Calls function func for all selected components in cal_table.
 *
 * @param cal_table Table with selected components of our interest.
 * @param func Function to be called on each selected component from cal_table.
 *        The first parameter of this function is a pointer to ECalModelComponent and
 *        the second parameter of this function is pointer to cal_table
 * @param user_data User data, will be passed to func.
 **/
static void
do_for_selected_components (ECalendarTable *cal_table, GFunc func, gpointer user_data)
{
	ETable *etable;
	struct AffectedComponents ac;

	g_return_if_fail (cal_table != NULL);

	ac.cal_table = cal_table;
	ac.components = NULL;

	etable = e_table_scrolled_get_table (E_TABLE_SCROLLED (cal_table->etable));
	e_table_selected_row_foreach (etable, get_selected_components_cb, &ac);

	g_slist_foreach (ac.components, func, user_data);
	g_slist_free (ac.components);
}

/**
 * obtain_list_of_components
 * As a callback function to convert each ECalModelComponent to string
 * of format "source_uid\ncomponent_str" and add this newly allocated
 * string to the list of components. Strings should be freed with g_free.
 *
 * @param data ECalModelComponent object.
 * @param user_data Pointer to GSList list, where to put new strings.
 **/
static void
obtain_list_of_components (gpointer data, gpointer user_data)
{
	GSList **list;
	ECalModelComponent *comp_data;

	list = (GSList **) user_data;
	comp_data = (ECalModelComponent *) data;

	if (list && comp_data) {
		gchar *comp_str;
		icalcomponent *vcal;

		vcal = e_cal_util_new_top_level ();
		e_cal_util_add_timezones_from_component (vcal, comp_data->icalcomp);
		icalcomponent_add_component (vcal, icalcomponent_new_clone (comp_data->icalcomp));

		comp_str = icalcomponent_as_ical_string_r (vcal);
		if (comp_str) {
			ESource *source = e_cal_get_source (comp_data->client);
			const gchar *source_uid = e_source_peek_uid (source);

			*list = g_slist_prepend (*list, g_strdup_printf ("%s\n%s", source_uid, comp_str));

			/* do not free this pointer, it owns libical */
			/* g_free (comp_str); */
		}

		icalcomponent_free (vcal);
		g_free (comp_str);
	}
}

static void
table_drag_data_get (ETable             *table,
		     gint                 row,
		     gint                 col,
		     GdkDragContext     *context,
		     GtkSelectionData   *selection_data,
		     guint               info,
		     guint               time,
		     ETasks             *tasks)
{
	ETasksPrivate *priv;

	priv = tasks->priv;

	if (info == TARGET_VCALENDAR) {
		/* we will pass an icalcalendar component for both types */
		GSList *components = NULL;

		do_for_selected_components (E_CALENDAR_TABLE (priv->tasks_view), obtain_list_of_components, &components);

		if (components) {
			cal_comp_selection_set_string_list (selection_data, components);

			g_slist_foreach (components, (GFunc)g_free, NULL);
			g_slist_free (components);
		}
	}
}

/*
static void
table_drag_begin (ETable         *table,
		  gint             row,
		  gint             col,
		  GdkDragContext *context,
		  ETasks         *tasks)
{

}

static void
table_drag_end (ETable         *table,
		gint             row,
		gint             col,
		GdkDragContext *context,
		ETasks         *tasks)
{

}
*/

static void
table_drag_data_delete (ETable         *table,
			gint             row,
			gint             col,
			GdkDragContext *context,
			ETasks         *tasks)
{
	/* Moved components are deleted from source immediately when moved,
	   because some of them can be part of destination source, and we
	   don't want to delete not-moved tasks. There is no such information
	   which event has been moved and which not, so skip this method.
	*/
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
	gboolean state;

	priv = tasks->priv;

	priv->search_bar = cal_search_bar_new (CAL_SEARCH_TASKS_DEFAULT);
	g_signal_connect (priv->search_bar, "sexp_changed",
			  G_CALLBACK (search_bar_sexp_changed_cb), tasks);
	g_signal_connect (priv->search_bar, "category_changed",
			  G_CALLBACK (search_bar_category_changed_cb), tasks);
	categories_changed_cb (NULL, tasks);

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
	g_object_set_data (G_OBJECT (priv->tasks_view), "tasks", tasks);
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
	state = calendar_config_get_preview_state ();

	if (state)
		gtk_widget_show (priv->preview);

	model = e_calendar_table_get_model (E_CALENDAR_TABLE (priv->tasks_view));
	g_signal_connect (G_OBJECT (model), "model_row_changed",
			  G_CALLBACK (model_row_changed_cb), tasks);

	g_signal_connect (G_OBJECT (model), "cal_view_progress",
				G_CALLBACK (view_progress_cb), tasks);
	g_signal_connect (G_OBJECT (model), "cal_view_done",
				G_CALLBACK (view_done_cb), tasks);
}

/* Class initialization function for the gnome calendar */
static void
e_tasks_class_init (ETasksClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	e_tasks_signals[SELECTION_CHANGED] =
		g_signal_new ("selection_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETasksClass, selection_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1,
			      G_TYPE_INT);

	e_tasks_signals[SOURCE_ADDED] =
		g_signal_new ("source_added",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (ETasksClass, source_added),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_OBJECT);

	e_tasks_signals[SOURCE_REMOVED] =
		g_signal_new ("source_removed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (ETasksClass, source_removed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_OBJECT);

	object_class->destroy = e_tasks_destroy;

	class->selection_changed = NULL;
	class->source_added = NULL;
	class->source_removed = NULL;
}

static void
categories_changed_cb (gpointer object, gpointer user_data)
{
	GList *cat_list;
	GPtrArray *cat_array;
	ETasksPrivate *priv;
	ETasks *tasks = user_data;

	priv = tasks->priv;

	cat_array = g_ptr_array_new ();
	cat_list = e_categories_get_list ();
	while (cat_list != NULL) {
		if (e_categories_is_searchable ((const gchar *) cat_list->data))
			g_ptr_array_add (cat_array, cat_list->data);
		cat_list = g_list_remove (cat_list, cat_list->data);
	}

	cal_search_bar_set_categories ((CalSearchBar *)priv->search_bar, cat_array);

	g_ptr_array_free (cat_array, TRUE);
}

/* Object initialization function for the gnome calendar */
static void
e_tasks_init (ETasks *tasks)
{
	ETasksPrivate *priv;

	priv = g_new0 (ETasksPrivate, 1);
	tasks->priv = priv;

	e_categories_register_change_listener (G_CALLBACK (categories_changed_cb), tasks);

	setup_config (tasks);
	setup_widgets (tasks);

	priv->clients = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	priv->query = NULL;
	priv->view_instance = NULL;
	priv->view_menus = NULL;
	priv->current_uid = NULL;
	priv->sexp = g_strdup ("#t");
	priv->default_client = NULL;
	priv->tasks_menu = e_cal_menu_new ("org.gnome.evolution.tasks.view");
	update_view (tasks);
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

		e_categories_unregister_change_listener (G_CALLBACK (categories_changed_cb), tasks);

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
set_status_message (ETasks *tasks, const gchar *message, ...)
{
	ETasksPrivate *priv;
	va_list args;
	gchar sz[2048], *msg_string = NULL;

	if (message) {
		va_start (args, message);
		vsnprintf (sz, sizeof sz, message, args);
		va_end (args);
		msg_string = sz;
	}

	priv = tasks->priv;

	e_calendar_table_set_status_message (E_CALENDAR_TABLE (priv->tasks_view), msg_string, -1);
}

/* Callback from the calendar client when an error occurs in the backend */
static void
backend_error_cb (ECal *client, const gchar *message, gpointer data)
{
	ETasks *tasks;
	GtkWidget *dialog;
	gchar *urinopwd;

	tasks = E_TASKS (data);

	urinopwd = get_uri_without_password (e_cal_get_uri (client));

	dialog = gtk_message_dialog_new (
		GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (tasks))),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_ERROR,
		GTK_BUTTONS_OK,
		_("Error on %s:\n %s"),
		urinopwd, message);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

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

	g_signal_emit (tasks, e_tasks_signals[SOURCE_REMOVED], 0, source);

	e_calendar_table_set_status_message (E_CALENDAR_TABLE (e_tasks_get_calendar_table (tasks)), NULL, -1);

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

	if (status == E_CALENDAR_STATUS_AUTHENTICATION_FAILED || status == E_CALENDAR_STATUS_AUTHENTICATION_REQUIRED)
		auth_cal_forget_password (ecal);

	switch (status) {
	case E_CALENDAR_STATUS_OK :
		g_signal_handlers_disconnect_matched (ecal, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, client_cal_opened_cb, NULL);

		set_status_message (tasks, _("Loading tasks"));
		model = e_calendar_table_get_model (E_CALENDAR_TABLE (priv->tasks_view));
		e_cal_model_add_client (model, ecal);

		set_status_message (tasks, NULL);
		break;
	case E_CALENDAR_STATUS_AUTHENTICATION_FAILED:
		/* try to reopen calendar - it'll ask for a password once again */
		e_cal_open_async (ecal, FALSE);
		return;
	case E_CALENDAR_STATUS_BUSY :
		break;
	case E_CALENDAR_STATUS_REPOSITORY_OFFLINE:
		e_error_run (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (tasks))), "calendar:prompt-no-contents-offline-tasks", NULL);
	default :
		/* Make sure the source doesn't disappear on us */
		g_object_ref (source);

		priv->clients_list = g_list_remove (priv->clients_list, ecal);
		g_signal_handlers_disconnect_matched (ecal, G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, tasks);

		/* Do this last because it unrefs the client */
		g_hash_table_remove (priv->clients,  e_source_peek_uid (source));

		g_signal_emit (tasks, e_tasks_signals[SOURCE_REMOVED], 0, source);

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

	if (status == E_CALENDAR_STATUS_AUTHENTICATION_FAILED || status == E_CALENDAR_STATUS_AUTHENTICATION_REQUIRED)
		auth_cal_forget_password (ecal);

	switch (status) {
	case E_CALENDAR_STATUS_OK :
		g_signal_handlers_disconnect_matched (ecal, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, default_client_cal_opened_cb, NULL);
		model = e_calendar_table_get_model (E_CALENDAR_TABLE (priv->tasks_view));

		e_cal_model_set_default_client (model, ecal);
		set_status_message (tasks, NULL);
		break;
	case E_CALENDAR_STATUS_AUTHENTICATION_FAILED:
		/* try to reopen calendar - it'll ask for a password once again */
		e_cal_open_async (ecal, FALSE);
		return;
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

		g_signal_emit (tasks, e_tasks_signals[SOURCE_REMOVED], 0, source);

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
	icaltimezone *zone;

	priv = tasks->priv;

	zone = calendar_config_get_icaltimezone ();
	e_cal_set_default_timezone (cal, zone, NULL);

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
	CompEditor *editor;
	ECalComponent *comp;
	const gchar *category;
	ECal *ecal;
	guint32 flags = 0;

	g_return_if_fail (E_IS_TASKS (tasks));

	priv = tasks->priv;

	/* FIXME What to do about no default client */
	ecal = e_tasks_get_default_client (tasks);
	if (!ecal)
		return;

	flags |= COMP_EDITOR_NEW_ITEM | COMP_EDITOR_USER_ORG;

	comp = cal_comp_task_new_with_defaults (ecal);

	category = cal_search_bar_get_category (CAL_SEARCH_BAR (priv->search_bar));
	e_cal_component_set_categories (comp, category);

	editor = task_editor_new (ecal, flags);
	comp_editor_edit_comp (editor, comp);
	g_object_unref (comp);

	gtk_window_present (GTK_WINDOW (editor));
}

void
e_tasks_show_preview (ETasks *tasks, gboolean state)
{
	ETasksPrivate *priv;

	g_return_if_fail (tasks != NULL);
	g_return_if_fail (E_IS_TASKS (tasks));
	priv = tasks->priv;

	if (state) {
		ECalModel *model;
		ECalModelComponent *comp_data;
		ECalComponent *comp;
		ETable *etable;
		const gchar *uid;
		gint n_selected;

		etable = e_table_scrolled_get_table (E_TABLE_SCROLLED (E_CALENDAR_TABLE (priv->tasks_view)->etable));
		n_selected = e_table_selected_count (etable);

		if (n_selected != 1) {
			e_cal_component_preview_clear (E_CAL_COMPONENT_PREVIEW (priv->preview));
		} else {
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
		gtk_widget_show (priv->preview);

	} else	{
		e_cal_component_preview_clear (E_CAL_COMPONENT_PREVIEW (priv->preview));
		gtk_widget_hide (priv->preview);
	}
}

gboolean
e_tasks_add_todo_source (ETasks *tasks, ESource *source)
{
	ETasksPrivate *priv;
	ECal *client;
	const gchar *uid;

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
	g_signal_connect (G_OBJECT (client), "backend_died", G_CALLBACK (backend_died_cb), tasks);

	/* add the client to internal structure */
	g_hash_table_insert (priv->clients, g_strdup (uid) , client);
	priv->clients_list = g_list_prepend (priv->clients_list, client);

	g_signal_emit (tasks, e_tasks_signals[SOURCE_ADDED], 0, source);

	open_ecal (tasks, client, FALSE, client_cal_opened_cb);

	return TRUE;
}

gboolean
e_tasks_remove_todo_source (ETasks *tasks, ESource *source)
{
	ETasksPrivate *priv;
	ECal *client;
	ECalModel *model;
	const gchar *uid;

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

	g_signal_emit (tasks, e_tasks_signals[SOURCE_REMOVED], 0, source);

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
	gchar *sexp;
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

		g_list_foreach (objects, (GFunc) icalcomponent_free, NULL);
		g_list_free (objects);
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
	gchar *dir0, *dir1, *filename;
	static GalViewCollection *collection = NULL;

	g_return_if_fail (tasks != NULL);
	g_return_if_fail (E_IS_TASKS (tasks));
	g_return_if_fail (uic != NULL);
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (uic));

	priv = tasks->priv;

	g_return_if_fail (priv->view_instance == NULL);

	g_return_if_fail (priv->view_instance == NULL);
	g_return_if_fail (priv->view_menus == NULL);

	/* Create the view instance */

	if (collection == NULL) {
		collection = gal_view_collection_new ();

		gal_view_collection_set_title (collection, _("Tasks"));

		dir0 = g_build_filename (EVOLUTION_GALVIEWSDIR,
					 "tasks",
					 NULL);
		dir1 = g_build_filename (tasks_component_peek_base_directory (tasks_component_peek ()),
					 "tasks", "views", NULL);
		gal_view_collection_set_storage_directories (collection,
							     dir0,
							     dir1);
		g_free (dir1);
		g_free (dir0);

		/* Create the views */

		spec = e_table_specification_new ();
		filename = g_build_filename (EVOLUTION_ETSPECDIR,
					     "e-calendar-table.etspec",
					     NULL);
		if (!e_table_specification_load_from_file (spec, filename))
			g_error ("Unable to load ETable specification file "
				 "for tasks");
		g_free (filename);

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
	g_return_if_fail (priv->view_menus != NULL);

	g_object_unref (priv->view_instance);
	priv->view_instance = NULL;

	g_object_unref (priv->view_menus);
	priv->view_menus = NULL;
}

void
e_tasks_open_task_id (ETasks *tasks,
		      const gchar *src_uid,
		      const gchar *comp_uid,
		      const gchar *comp_rid)
{
	ECal *client = NULL;
	GList *l;
	icalcomponent* icalcomp = NULL;
	icalproperty *attendee_prop = NULL;

	if (!src_uid || !comp_uid)
		return;

	for (l = tasks->priv->clients_list; l != NULL; l = l->next) {
		ESource *client_src;

		client = l->data;
		client_src = e_cal_get_source (client);

		if (!strcmp (src_uid, e_source_peek_uid (client_src)))
			break;
	}

	if (!client)
		return;

	e_cal_get_object (client, comp_uid, comp_rid, &icalcomp, NULL);

	if (!icalcomp)
		return;

	attendee_prop = icalcomponent_get_first_property (icalcomp, ICAL_ATTENDEE_PROPERTY);
	e_calendar_table_open_task (E_CALENDAR_TABLE (tasks->priv->tasks_view), client, icalcomp, attendee_prop != NULL);
	icalcomponent_free (icalcomp);

	return;
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

/**
 * e_tasks_get_preview:
 * @tasks: A tasks widget.
 *
 * Queries the #ECalComponentPreview contained in a tasks widget.
 **/
GtkWidget *
e_tasks_get_preview (ETasks *tasks)
{
	g_return_val_if_fail (tasks != NULL, NULL);
	g_return_val_if_fail (E_IS_TASKS (tasks), NULL);

	return tasks->priv->preview;
}
