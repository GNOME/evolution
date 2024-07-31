/*
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2014 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n.h>

#include <calendar/gui/comp-util.h>
#include <calendar/gui/e-cal-ops.h>

#include "e-cal-base-shell-sidebar.h"
#include "e-task-shell-view.h"
#include "e-task-shell-backend.h"

struct _ETaskShellBackendPrivate {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (ETaskShellBackend, e_task_shell_backend, E_TYPE_CAL_BASE_SHELL_BACKEND, 0,
	G_ADD_PRIVATE_DYNAMIC (ETaskShellBackend))

static void
action_task_new_cb (EUIAction *action,
		    GVariant *parameter,
		    gpointer user_data)
{
	EShellWindow *shell_window = user_data;
	EShellView *shell_view;
	ESource *selected_source = NULL;

	shell_view = e_shell_window_peek_shell_view (shell_window, "tasks");
	if (shell_view != NULL) {
		EShellSidebar *shell_sidebar;
		ESourceSelector *selector;

		shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
		selector = e_cal_base_shell_sidebar_get_selector (E_CAL_BASE_SHELL_SIDEBAR (shell_sidebar));
		selected_source = e_source_selector_ref_primary_selection (selector);
	}

	e_cal_ops_new_component_editor (shell_window,
		E_CAL_CLIENT_SOURCE_TYPE_TASKS, selected_source ? e_source_get_uid (selected_source) : NULL,
		g_strcmp0 (g_action_get_name (G_ACTION (action)), "task-assigned-new") == 0 ||
		g_strcmp0 (g_action_get_name (G_ACTION (action)), "new-menu-task-assigned-new") == 0 );

	g_clear_object (&selected_source);
}

static void
action_task_list_new_cb (EUIAction *action,
			 GVariant *parameter,
			 gpointer user_data)
{
	EShellWindow *shell_window = user_data;

	e_cal_base_shell_backend_util_new_source (shell_window, E_CAL_CLIENT_SOURCE_TYPE_TASKS);
}

static gboolean
e_task_shell_backend_handle_uri (EShellBackend *shell_backend,
				 const gchar *uri)
{
	if (strncmp (uri, "task:", 5) != 0)
		return FALSE;

	return e_cal_base_shell_backend_util_handle_uri (shell_backend,
		E_CAL_CLIENT_SOURCE_TYPE_TASKS, uri, NULL);
}

static void
e_task_shell_backend_class_init (ETaskShellBackendClass *class)
{
	static const EUIActionEntry item_entries[] = {
		{ "new-menu-task-new",
		  "stock_task",
		  NC_("New", "_Task"),
		  "<Shift><Control>t",
		  N_("Create a new task"),
		  action_task_new_cb, NULL, NULL, NULL },

		{ "new-menu-task-assigned-new",
		  "stock_task-assigned-to",
		  NC_("New", "Assigne_d Task"),
		  "<Shift><Control>i",
		  N_("Create a new assigned task"),
		  action_task_new_cb, NULL, NULL, NULL }
	};

	static const EUIActionEntry source_entries[] = {
		{ "new-menu-task-list-new",
		  "stock_todo",
		  NC_("New", "Tas_k List"),
		  NULL,
		  N_("Create a new task list"),
		  action_task_list_new_cb, NULL, NULL, NULL }
	};

	EShellBackendClass *shell_backend_class;
	ECalBaseShellBackendClass *cal_base_shell_backend_class;

	shell_backend_class = E_SHELL_BACKEND_CLASS (class);
	shell_backend_class->shell_view_type = E_TYPE_TASK_SHELL_VIEW;
	shell_backend_class->name = "tasks";
	shell_backend_class->aliases = "";
	shell_backend_class->schemes = "task";
	shell_backend_class->sort_order = 500;
	shell_backend_class->preferences_page = "calendar-and-tasks";
	shell_backend_class->start = NULL;

	cal_base_shell_backend_class = E_CAL_BASE_SHELL_BACKEND_CLASS (class);
	cal_base_shell_backend_class->new_item_entries = item_entries;
	cal_base_shell_backend_class->new_item_n_entries = G_N_ELEMENTS (item_entries);
	cal_base_shell_backend_class->source_entries = source_entries;
	cal_base_shell_backend_class->source_n_entries = G_N_ELEMENTS (source_entries);
	cal_base_shell_backend_class->handle_uri = e_task_shell_backend_handle_uri;
}

static void
e_task_shell_backend_init (ETaskShellBackend *task_shell_backend)
{
	task_shell_backend->priv = e_task_shell_backend_get_instance_private (task_shell_backend);
}

static void
e_task_shell_backend_class_finalize (ETaskShellBackendClass *class)
{
}

void
e_task_shell_backend_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_task_shell_backend_register_type (type_module);
}
