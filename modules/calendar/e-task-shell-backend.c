/*
 * e-task-shell-backend.c
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

#include "e-task-shell-backend.h"

#include <string.h>
#include <glib/gi18n.h>
#include <libecal/e-cal.h>
#include <libedataserver/e-url.h>
#include <libedataserver/e-source.h>
#include <libedataserver/e-source-list.h>
#include <libedataserver/e-source-group.h>

#include "shell/e-shell.h"
#include "shell/e-shell-backend.h"
#include "shell/e-shell-window.h"

#include "calendar/common/authentication.h"
#include "calendar/gui/calendar-config.h"
#include "calendar/gui/comp-util.h"
#include "calendar/gui/dialogs/calendar-setup.h"
#include "calendar/gui/dialogs/task-editor.h"

#include "e-task-shell-content.h"
#include "e-task-shell-migrate.h"
#include "e-task-shell-sidebar.h"
#include "e-task-shell-view.h"

#define E_TASK_SHELL_BACKEND_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_TASK_SHELL_BACKEND, ETaskShellBackendPrivate))

struct _ETaskShellBackendPrivate {
	ESourceList *source_list;
};

enum {
	PROP_0,
	PROP_SOURCE_LIST
};

static gpointer parent_class;
static GType task_shell_backend_type;

static void
task_shell_backend_ensure_sources (EShellBackend *shell_backend)
{
	/* XXX This is basically the same algorithm across all modules.
	 *     Maybe we could somehow integrate this into EShellBackend? */

	ETaskShellBackendPrivate *priv;
	ESourceGroup *on_this_computer;
	ESource *personal;
	EShell *shell;
	EShellSettings *shell_settings;
	GSList *sources, *iter;
	const gchar *name;
	gboolean save_list = FALSE;

	on_this_computer = NULL;
	personal = NULL;

	priv = E_TASK_SHELL_BACKEND_GET_PRIVATE (shell_backend);

	shell = e_shell_backend_get_shell (shell_backend);
	shell_settings = e_shell_get_shell_settings (shell);

	if (!e_cal_get_sources (&priv->source_list, E_CAL_SOURCE_TYPE_TODO, NULL)) {
		g_warning ("Could not get task sources from GConf!");
		return;
	}

	on_this_computer = e_source_list_ensure_group (
		priv->source_list, _("On This Computer"), "local:", TRUE);
	e_source_list_ensure_group (
		priv->source_list, _("On The Web"), "webcal://", FALSE);

	g_return_if_fail (on_this_computer);

	sources = e_source_group_peek_sources (on_this_computer);

	/* Make sure this group includes a "Personal" source. */
	for (iter = sources; iter != NULL; iter = iter->next) {
		ESource *source = iter->data;
		const gchar *relative_uri;

		relative_uri = e_source_peek_relative_uri (source);
		if (g_strcmp0 (relative_uri, "system") == 0) {
			personal = source;
			break;
		}
	}

	name = _("Personal");

	if (personal == NULL) {
		ESource *source;
		GSList *selected;
		gchar *primary;

		source = e_source_new (name, "system");
		e_source_set_color_spec (source, "#BECEDD");
		e_source_group_add_source (on_this_computer, source, -1);
		g_object_unref (source);
		save_list = TRUE;

		primary = e_shell_settings_get_string (
			shell_settings, "cal-primary-task-list");

		selected = calendar_config_get_tasks_selected ();

		if (primary == NULL && selected == NULL) {
			const gchar *uid;

			uid = e_source_peek_uid (source);
			selected = g_slist_prepend (NULL, g_strdup (uid));

			e_shell_settings_set_string (
				shell_settings, "cal-primary-task-list", uid);
			calendar_config_set_tasks_selected (selected);
		}

		g_slist_foreach (selected, (GFunc) g_free, NULL);
		g_slist_free (selected);
		g_free (primary);
	} else {
		/* Force the source name to the current locale. */
		e_source_set_name (personal, name);
	}

	g_object_unref (on_this_computer);

	if (save_list)
		e_source_list_sync (priv->source_list, NULL);
}

static void
task_shell_backend_new_task (ESource *source,
                             GAsyncResult *result,
                             EShell *shell,
                             CompEditorFlags flags)
{
	ECal *cal;
	ECalComponent *comp;
	CompEditor *editor;

	/* XXX Handle errors better. */
	cal = e_load_cal_source_finish (source, result, NULL);
	g_return_if_fail (E_IS_CAL (cal));

	editor = task_editor_new (cal, shell, flags);
	comp = cal_comp_task_new_with_defaults (cal);
	comp_editor_edit_comp (editor, comp);

	gtk_window_present (GTK_WINDOW (editor));

	g_object_unref (comp);
	g_object_unref (cal);
}

static void
task_shell_backend_task_new_cb (ESource *source,
                                GAsyncResult *result,
                                EShell *shell)
{
	CompEditorFlags flags = 0;

	flags |= COMP_EDITOR_NEW_ITEM;

	task_shell_backend_new_task (source, result, shell, flags);

	g_object_unref (shell);
}

static void
task_shell_backend_task_assigned_new_cb (ESource *source,
                                         GAsyncResult *result,
                                         EShell *shell)
{
	CompEditorFlags flags = 0;

	flags |= COMP_EDITOR_NEW_ITEM;
	flags |= COMP_EDITOR_IS_ASSIGNED;
	flags |= COMP_EDITOR_USER_ORG;

	task_shell_backend_new_task (source, result, shell, flags);

	g_object_unref (shell);
}

static void
action_task_new_cb (GtkAction *action,
                    EShellWindow *shell_window)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EShellSettings *shell_settings;
	ESource *source = NULL;
	ESourceList *source_list;
	ECalSourceType source_type;
	const gchar *action_name;
	gchar *uid;

	/* This callback is used for both tasks and assigned tasks. */

	source_type = E_CAL_SOURCE_TYPE_TODO;

	shell = e_shell_window_get_shell (shell_window);
	shell_settings = e_shell_get_shell_settings (shell);
	shell_backend = e_shell_get_backend_by_name (shell, "tasks");

	g_object_get (shell_backend, "source-list", &source_list, NULL);
	g_return_if_fail (E_IS_SOURCE_LIST (source_list));

	uid = e_shell_settings_get_string (
		shell_settings, "cal-primary-task-list");

	if (uid != NULL) {
		source = e_source_list_peek_source_by_uid (source_list, uid);
		g_free (uid);
	}

	if (source == NULL)
		source = e_source_list_peek_default_source (source_list);

	g_return_if_fail (E_IS_SOURCE (source));

	/* Use a callback function appropriate for the action.
	 * FIXME Need to obtain a better default time zone. */
	action_name = gtk_action_get_name (action);
	if (strcmp (action_name, "task-assigned-new") == 0)
		e_load_cal_source_async (
			source, source_type, NULL,
			GTK_WINDOW (shell_window),
			NULL, (GAsyncReadyCallback)
			task_shell_backend_task_assigned_new_cb,
			g_object_ref (shell));
	else
		e_load_cal_source_async (
			source, source_type, NULL,
			GTK_WINDOW (shell_window),
			NULL, (GAsyncReadyCallback)
			task_shell_backend_task_new_cb,
			g_object_ref (shell));

	g_object_unref (source_list);
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
	  NC_("New", "_Task"),
	  "<Shift><Control>t",
	  N_("Create a new task"),
	  G_CALLBACK (action_task_new_cb) },

	{ "task-assigned-new",
	  "stock_task",
	  NC_("New", "Assigne_d Task"),
	  NULL,
	  N_("Create a new assigned task"),
	  G_CALLBACK (action_task_new_cb) }
};

static GtkActionEntry source_entries[] = {

	{ "task-list-new",
	  "stock_todo",
	  NC_("New", "Tas_k List"),
	  NULL,
	  N_("Create a new task list"),
	  G_CALLBACK (action_task_list_new_cb) }
};

static gboolean
task_shell_backend_handle_uri_cb (EShellBackend *shell_backend,
                                  const gchar *uri)
{
	EShell *shell;
	CompEditor *editor;
	CompEditorFlags flags = 0;
	ECal *client;
	ECalComponent *comp;
	ESource *source;
	ESourceList *source_list;
	ECalSourceType source_type;
	EUri *euri;
	icalcomponent *icalcomp;
	icalproperty *icalprop;
	const gchar *cp;
	gchar *source_uid = NULL;
	gchar *comp_uid = NULL;
	gchar *comp_rid = NULL;
	gboolean handled = FALSE;
	GError *error = NULL;

	source_type = E_CAL_SOURCE_TYPE_TODO;
	shell = e_shell_backend_get_shell (shell_backend);

	if (strncmp (uri, "task:", 5) != 0)
		return FALSE;

	euri = e_uri_new (uri);
	cp = euri->query;
	if (cp == NULL)
		goto exit;

	while (*cp != '\0') {
		gchar *header;
		gchar *content;
		gsize header_len;
		gsize content_len;

		header_len = strcspn (cp, "=&");

		/* If it's malformed, give up. */
		if (cp[header_len] != '=')
			break;

		header = (gchar *) cp;
		header[header_len] = '\0';
		cp += header_len + 1;

		content_len = strcspn (cp, "&");

		content = g_strndup (cp, content_len);
		if (g_ascii_strcasecmp (header, "source-uid") == 0)
			source_uid = g_strdup (content);
		else if (g_ascii_strcasecmp (header, "comp-uid") == 0)
			comp_uid = g_strdup (content);
		else if (g_ascii_strcasecmp (header, "comp-rid") == 0)
			comp_rid = g_strdup (content);
		g_free (content);

		cp += content_len;
		if (*cp == '&') {
			cp++;
			if (strcmp (cp, "amp;") == 0)
				cp += 4;
		}
	}

	if (source_uid == NULL || comp_uid == NULL)
		goto exit;

	/* URI is valid, so consider it handled.  Whether
	 * we successfully open it is another matter... */
	handled = TRUE;

	if (!e_cal_get_sources (&source_list, source_type, NULL)) {
		g_printerr ("Could not get task sources from GConf!\n");
		goto exit;
	}

	source = e_source_list_peek_source_by_uid (source_list, source_uid);
	if (source == NULL) {
		g_printerr ("No source for UID '%s'\n", source_uid);
		g_object_unref (source_list);
		goto exit;
	}

	client = e_auth_new_cal_from_source (source, source_type);
	if (client == NULL || !e_cal_open (client, TRUE, &error)) {
		if (error != NULL) {
			g_printerr ("%s\n", error->message);
			g_error_free (error);
		}
		g_object_unref (source_list);
		goto exit;
	}

	/* XXX Copied from e_task_shell_view_open_task().
	 *     Clearly a new utility function is needed. */

	editor = comp_editor_find_instance (comp_uid);

	if (editor != NULL)
		goto present;

	if (!e_cal_get_object (client, comp_uid, comp_rid, &icalcomp, &error)) {
		g_printerr ("%s\n", error->message);
		g_object_unref (source_list);
		g_error_free (error);
		goto exit;
	}

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomp);

	icalprop = icalcomponent_get_first_property (
		icalcomp, ICAL_ATTENDEE_PROPERTY);
	if (icalprop != NULL)
		flags |= COMP_EDITOR_IS_ASSIGNED;

	if (itip_organizer_is_user (comp, client))
		flags |= COMP_EDITOR_USER_ORG;

	if (!e_cal_component_has_attendees (comp))
		flags |= COMP_EDITOR_USER_ORG;

	editor = task_editor_new (client, shell, flags);
	comp_editor_edit_comp (editor, comp);

	g_object_unref (comp);

present:
	gtk_window_present (GTK_WINDOW (editor));

	g_object_unref (source_list);
	g_object_unref (client);

exit:
	g_free (source_uid);
	g_free (comp_uid);
	g_free (comp_rid);

	e_uri_free (euri);

	return handled;
}

static void
task_shell_backend_window_created_cb (EShellBackend *shell_backend,
                                      GtkWindow *window)
{
	const gchar *module_name;

	if (!E_IS_SHELL_WINDOW (window))
		return;

	module_name = E_SHELL_BACKEND_GET_CLASS (shell_backend)->name;

	e_shell_window_register_new_item_actions (
		E_SHELL_WINDOW (window), module_name,
		item_entries, G_N_ELEMENTS (item_entries));

	e_shell_window_register_new_source_actions (
		E_SHELL_WINDOW (window), module_name,
		source_entries, G_N_ELEMENTS (source_entries));
}

static void
task_shell_backend_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SOURCE_LIST:
			g_value_set_object (
				value,
				e_task_shell_backend_get_source_list (
				E_TASK_SHELL_BACKEND (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
task_shell_backend_dispose (GObject *object)
{
	ETaskShellBackendPrivate *priv;

	priv = E_TASK_SHELL_BACKEND_GET_PRIVATE (object);

	if (priv->source_list != NULL) {
		g_object_unref (priv->source_list);
		priv->source_list = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
task_shell_backend_constructed (GObject *object)
{
	EShell *shell;
	EShellBackend *shell_backend;

	shell_backend = E_SHELL_BACKEND (object);
	shell = e_shell_backend_get_shell (shell_backend);

	task_shell_backend_ensure_sources (shell_backend);

	g_signal_connect_swapped (
		shell, "handle-uri",
		G_CALLBACK (task_shell_backend_handle_uri_cb),
		shell_backend);

	g_signal_connect_swapped (
		shell, "window-created",
		G_CALLBACK (task_shell_backend_window_created_cb),
		shell_backend);
}

static void
task_shell_backend_class_init (ETaskShellBackendClass *class)
{
	GObjectClass *object_class;
	EShellBackendClass *shell_backend_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (ETaskShellBackendPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = task_shell_backend_get_property;
	object_class->dispose = task_shell_backend_dispose;
	object_class->constructed = task_shell_backend_constructed;

	shell_backend_class = E_SHELL_BACKEND_CLASS (class);
	shell_backend_class->shell_view_type = E_TYPE_TASK_SHELL_VIEW;
	shell_backend_class->name = "tasks";
	shell_backend_class->aliases = "";
	shell_backend_class->schemes = "task";
	shell_backend_class->sort_order = 500;
	shell_backend_class->preferences_page = "calendar-and-tasks";
	shell_backend_class->start = NULL;
	shell_backend_class->migrate = e_task_shell_backend_migrate;

	g_object_class_install_property (
		object_class,
		PROP_SOURCE_LIST,
		g_param_spec_object (
			"source-list",
			"Source List",
			"The registry of task lists",
			E_TYPE_SOURCE_LIST,
			G_PARAM_READABLE));
}

static void
task_shell_backend_init (ETaskShellBackend *task_shell_backend)
{
	task_shell_backend->priv =
		E_TASK_SHELL_BACKEND_GET_PRIVATE (task_shell_backend);
}

GType
e_task_shell_backend_get_type (void)
{
	return task_shell_backend_type;
}

void
e_task_shell_backend_register_type (GTypeModule *type_module)
{
	const GTypeInfo type_info = {
		sizeof (ETaskShellBackendClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) task_shell_backend_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (ETaskShellBackend),
		0,     /* n_preallocs */
		(GInstanceInitFunc) task_shell_backend_init,
		NULL   /* value_table */
	};

	task_shell_backend_type = g_type_module_register_type (
		type_module, E_TYPE_SHELL_BACKEND,
		"ETaskShellBackend", &type_info, 0);
}

ESourceList *
e_task_shell_backend_get_source_list (ETaskShellBackend *task_shell_backend)
{
	g_return_val_if_fail (
		E_IS_TASK_SHELL_BACKEND (task_shell_backend), NULL);

	return task_shell_backend->priv->source_list;
}
