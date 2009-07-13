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

	/* Paned widget */
	GtkWidget *paned;

	/* The preview */
	GtkWidget *preview;

	gchar *current_uid;
	gchar *sexp;
	guint update_timeout;

	GList *notifications;
};

static void setup_widgets (ETasks *tasks);
static void e_tasks_destroy (GtkObject *object);

static void backend_error_cb (ECal *client, const gchar *message, gpointer data);

/* Signal IDs */
enum {
	SELECTION_CHANGED,
	SOURCE_ADDED,
	SOURCE_REMOVED,
	LAST_SIGNAL
};

static guint e_tasks_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (ETasks, e_tasks, GTK_TYPE_TABLE)

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

	gtk_table_attach (GTK_TABLE (tasks), priv->search_bar, 0, 1, 0, 1,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);
	gtk_widget_show (priv->search_bar);

	/* add the paned widget for the task list and task detail areas */
	priv->paned = gtk_vpaned_new ();
	g_signal_connect (priv->paned, "realize", G_CALLBACK (pane_realized), tasks);

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

	g_signal_connect (etable, "selection_change", G_CALLBACK (table_selection_change_cb), tasks);

	/* create the task detail */
	priv->preview = e_cal_component_preview_new ();
	e_cal_component_preview_set_default_timezone (E_CAL_COMPONENT_PREVIEW (priv->preview), calendar_config_get_icaltimezone ());
	gtk_paned_add2 (GTK_PANED (priv->paned), priv->preview);
	state = calendar_config_get_preview_state ();

	if (state)
		gtk_widget_show (priv->preview);
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

/* Object initialization function for the gnome calendar */
static void
e_tasks_init (ETasks *tasks)
{
	ETasksPrivate *priv;

	priv = g_new0 (ETasksPrivate, 1);
	tasks->priv = priv;

	setup_widgets (tasks);

	priv->clients = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	priv->query = NULL;
	priv->current_uid = NULL;
	priv->sexp = g_strdup ("#t");
	priv->default_client = NULL;
}

GtkWidget *
e_tasks_new (void)
{
	ETasks *tasks;

	tasks = g_object_new (e_tasks_get_type (), NULL);

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
