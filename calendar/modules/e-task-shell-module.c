/*
 * e-task-shell-module.c
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <string.h>
#include <glib/gi18n.h>
#include <libecal/e-cal.h>
#include <libedataserver/e-source.h>
#include <libedataserver/e-source-list.h>
#include <libedataserver/e-source-group.h>

#include "shell/e-shell.h"
#include "shell/e-shell-module.h"
#include "shell/e-shell-window.h"

#include "calendar/gui/calendar-config.h"

#include "e-task-shell-view.h"

#define MODULE_NAME		"tasks"
#define MODULE_ALIASES		""
#define MODULE_SCHEMES		"task"
#define MODULE_SORT_ORDER	600

#define WEB_BASE_URI		"webcal://"
#define PERSONAL_RELATIVE_URI	"system"

/* Module Entry Point */
void e_shell_module_init (GTypeModule *type_module);

static void
task_module_ensure_sources (EShellModule *shell_module)
{
	/* XXX This is basically the same algorithm across all modules.
	 *     Maybe we could somehow integrate this into EShellModule? */

	ESourceList *source_list;
	ESourceGroup *on_this_computer;
	ESourceGroup *on_the_web;
	ESource *personal;
	GSList *groups, *iter;
	const gchar *data_dir;
	gchar *base_uri;
	gchar *filename;

	on_this_computer = NULL;
	on_the_web = NULL;
	personal = NULL;

	if (!e_cal_get_sources (&source_list, E_CAL_SOURCE_TYPE_TODO, NULL)) {
		g_warning ("Could not get task sources from GConf!");
		return;
	}

	/* Share the source list with all task views.  This is
 	 * accessible via e_task_shell_view_get_source_list().
 	 * Note: EShellModule takes ownership of the reference.
 	 *
 	 * XXX I haven't yet decided if I want to add a proper
 	 *     EShellModule API for this.  The mail module would
 	 *     not use it. */
	g_object_set_data_full (
		G_OBJECT (shell_module), "source-list",
		source_list, (GDestroyNotify) g_object_unref);

	data_dir = e_shell_module_get_data_dir (shell_module);
	filename = g_build_filename (data_dir, "local", NULL);
	base_uri = g_filename_to_uri (filename, NULL, NULL);
	g_free (filename);

	groups = e_source_list_peek_groups (source_list);
	for (iter = groups; iter != NULL; iter = iter->next) {
		ESourceGroup *source_group = iter->data;
		const gchar *group_base_uri;

		group_base_uri = e_source_group_peek_base_uri (source_group);

		/* Compare only "file://" part.  If the user's home
		 * changes, we do not want to create another group. */
		if (on_this_computer == NULL &&
			strncmp (base_uri, group_base_uri, 7) == 0)
			on_this_computer = source_group;

		else if (on_the_web == NULL &&
			strcmp (WEB_BASE_URI, group_base_uri) == 0)
			on_the_web = source_group;
	}

	if (on_this_computer != NULL) {
		GSList *sources;
		const gchar *group_base_uri;

		sources = e_source_group_peek_sources (on_this_computer);
		group_base_uri = e_source_group_peek_base_uri (on_this_computer);

		/* Make sure this group includes a "Personal" source. */
		for (iter = sources; iter != NULL; iter = iter->next) {
			ESource *source = iter->data;
			const gchar *relative_uri;

			relative_uri = e_source_peek_relative_uri (source);
			if (relative_uri == NULL)
				continue;

			if (strcmp (PERSONAL_RELATIVE_URI, relative_uri) != 0)
				continue;

			personal = source;
			break;
		}

		/* Make sure we have the correct base URI.  This can
		 * change when the user's home directory changes. */
		if (strcmp (base_uri, group_base_uri) != 0) {
			e_source_group_set_base_uri (
				on_this_computer, base_uri);

			/* XXX We shouldn't need this sync call here as
			 *     set_base_uri() results in synching to GConf,
			 *     but that happens in an idle loop and too late
			 *     to prevent the user from seeing a "Cannot
			 *     Open ... because of invalid URI" error. */
			e_source_list_sync (source_list, NULL);
		}

	} else {
		ESourceGroup *source_group;
		const gchar *name;

		name = _("On This Computer");
		source_group = e_source_group_new (name, base_uri);
		e_source_list_add_group (source_list, source_group, -1);
		g_object_unref (source_group);
	}

	if (personal == NULL) {
		ESource *source;
		GSList *selected;
		const gchar *name;
		gchar *primary;

		name = _("Personal");
		source = e_source_new (name, PERSONAL_RELATIVE_URI);
		e_source_group_add_source (on_this_computer, source, -1);
		g_object_unref (source);

		primary = calendar_config_get_primary_tasks ();
		selected = calendar_config_get_tasks_selected ();

		if (primary == NULL && selected == NULL) {
			const gchar *uid;

			uid = e_source_peek_uid (source);
			selected = g_slist_prepend (NULL, g_strdup (uid));

			calendar_config_set_primary_tasks (uid);
			calendar_config_set_tasks_selected (selected);
		}

		g_slist_foreach (selected, (GFunc) g_free, NULL);
		g_slist_free (selected);
		g_free (primary);
	}

	if (on_the_web == NULL) {
		ESourceGroup *source_group;
		const gchar *name;

		name = _("On The Web");
		source_group = e_source_group_new (name, WEB_BASE_URI);
		e_source_list_add_group (source_list, source_group, -1);
		g_object_unref (source_group);
	}

	g_free (base_uri);
}

static void
task_module_cal_opened_cb (ECal *cal,
                           ECalendarStatus status,
                           GtkAction *action)
{
	ECalComponent *comp;
	CompEditor *editor;
	CompEditorFlags flags = 0;
	const gchar *action_name;

	/* XXX Handle errors better. */
	if (status != E_CALENDAR_STATUS_OK)
		return;

	action_name = gtk_action_get_name (action);

	flags |= COMP_EDITOR_NEW_ITEM;
	if (strcmp (action_name, "task-assigned-new") == 0) {
		flags |= COMP_EDITOR_IS_ASSIGNED;
		flags |= COMP_EDITOR_USER_ORG;
	}

	editor = task_editor_new (cal, flags);
	comp = cal_comp_task_new_with_defaults (cal);
	comp_editor_edit_comp (editor, comp);

	gtk_window_present (GTK_WINDOW (editor));

	g_object_unref (comp);
	g_object_unref (cal);
}

static void
action_task_new_cb (GtkAction *action,
                    EShellWindow *shell_window)
{
	ECal *cal = NULL;
	ECalSourceType source_type;
	ESourceList *source_list;
	gchar *uid;

	/* This callback is used for both tasks and assigned tasks. */

	source_type = E_CAL_SOURCE_TYPE_TODO;

	if (!e_cal_get_sources (&source_list, source_type, NULL)) {
		g_warning ("Could not get task sources from GConf!");
		return;
	}

	uid = calendar_config_get_primary_tasks ();

	if (uid != NULL) {
		ESource *source;

		source = e_source_list_peek_source_by_uid (source_list, uid);
		if (source != NULL)
			cal = auth_new_cal_from_source (source, source_type);
		g_free (uid);
	}

	if (cal == NULL)
		cal = auth_new_cal_from_default (source_type);

	g_return_if_fail (cal != NULL);

	g_signal_connect (
		cal, "cal-opened",
		G_CALLBACK (task_module_cal_opened_cb), action);

	e_cal_open_async (cal, FALSE);
}

static void
action_task_list_new_cb (GtkAction *action,
                         EShellWindow *shell_window)
{
	calendar_setup_new_task_list (GTK_WINDOW (shell_window));
}

static GtkActionEntry item_entries[] = {

	{ "task-new",
	  "stock_task",
	  N_("_Task"),  /* XXX Need C_() here */
	  "<Shift><Control>t",
	  N_("Create a new task"),
	  G_CALLBACK (action_task_new_cb) },

	{ "task-assigned-new",
	  "stock_task",
	  N_("Assigne_d Task"),
	  NULL,
	  N_("Create a new assigned task"),
	  G_CALLBACK (action_task_new_cb) }
};

static GtkActionEntry source_entries[] = {

	{ "task-list-new",
	  "stock_todo",
	  N_("Tas_k List"),
	  NULL,
	  N_("Create a new task list"),
	  G_CALLBACK (action_task_list_new_cb) }
};

static gboolean
task_module_handle_uri (EShellModule *shell_module,
                        const gchar *uri)
{
        /* FIXME */
        return FALSE;
}

static void
task_module_window_created (EShellModule *shell_module,
                            EShellWindow *shell_window)
{
        const gchar *module_name;

        module_name = G_TYPE_MODULE (shell_module)->name;

        e_shell_window_register_new_item_actions (
                shell_window, module_name,
                item_entries, G_N_ELEMENTS (item_entries));

        e_shell_window_register_new_source_actions (
                shell_window, module_name,
                source_entries, G_N_ELEMENTS (source_entries));
}

static EShellModuleInfo module_info = {

        MODULE_NAME,
        MODULE_ALIASES,
        MODULE_SCHEMES,
        MODULE_SORT_ORDER
};

void
e_shell_module_init (GTypeModule *type_module)
{
        EShell *shell;
        EShellModule *shell_module;

        shell_module = E_SHELL_MODULE (type_module);
        shell = e_shell_module_get_shell (shell_module);

	/* Register the GType for ETaskShellView. */
        e_task_shell_view_get_type (type_module);

        e_shell_module_set_info (shell_module, &module_info);

	task_module_ensure_sources (shell_module);

        g_signal_connect_swapped (
                shell, "handle-uri",
                G_CALLBACK (task_module_handle_uri), shell_module);

        g_signal_connect_swapped (
                shell, "window-created",
                G_CALLBACK (task_module_window_created), shell_module);
}
