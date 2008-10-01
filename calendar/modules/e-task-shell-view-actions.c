/*
 * e-task-shell-view-actions.c
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

#include "e-task-shell-view-private.h"

static void
action_task_assign_cb (GtkAction *action,
                       ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskTable *task_table;
	ECalModelComponent *comp_data;
	GSList *list;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);

	list = e_task_table_get_selected (task_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	/* XXX We only open the first selected task. */
	e_task_shell_view_open_task (task_shell_view, comp_data);

	/* FIXME Need to actually assign the task. */
}

static void
action_task_clipboard_copy_cb (GtkAction *action,
                               ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskTable *task_table;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);
	e_task_table_copy_clipboard (task_table);
}

static void
action_task_clipboard_cut_cb (GtkAction *action,
                              ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskTable *task_table;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);
	e_task_table_cut_clipboard (task_table);
}

static void
action_task_clipboard_paste_cb (GtkAction *action,
                                ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskTable *task_table;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);
	e_task_table_paste_clipboard (task_table);
}

static void
action_task_delete_cb (GtkAction *action,
                       ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskPreview *task_preview;
	ETaskTable *task_table;
	const gchar *status_message;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);
	task_preview = e_task_shell_content_get_task_preview (task_shell_content);

	status_message = _("Deleting selected tasks...");
	e_task_shell_view_set_status_message (task_shell_view, status_message);
	e_task_table_delete_selected (task_table);
	e_task_shell_view_set_status_message (task_shell_view, NULL);

	e_task_preview_clear (task_preview);
}

static void
action_task_forward_cb (GtkAction *action,
                        ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskTable *task_table;
	ECalModelComponent *comp_data;
	ECalComponent *comp;
	icalcomponent *clone;
	ECalComponentItipMethod method;
	GSList *list;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);

	list = e_task_table_get_selected (task_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	/* XXX We only forward the first selected task. */
	comp = e_cal_component_new ();
	clone = icalcomponent_new_clone (comp_data->icalcomp);
	method = E_CAL_COMPONENT_METHOD_PUBLISH;
	e_cal_component_set_icalcomponent (comp, clone);
	itip_send_comp (method, comp, comp_data->client, NULL, NULL, NULL);
	g_object_unref (comp);
}

static void
action_task_list_copy_cb (GtkAction *action,
                          ETaskShellView *task_shell_view)
{
	/* FIXME */
}

static void
action_task_list_delete_cb (GtkAction *action,
                            ETaskShellView *task_shell_view)
{
	/* FIXME */
}

static void
action_task_list_new_cb (GtkAction *action,
                         ETaskShellView *task_shell_view)
{
}

static void
action_task_list_print_cb (GtkAction *action,
                           ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskTable *task_table;
	ETable *table;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);
	table = e_task_table_get_table (task_table);

	print_table (
		table, _("Print Tasks"), _("Tasks"),
		GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);
}

static void
action_task_list_print_preview_cb (GtkAction *action,
                                   ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskTable *task_table;
	ETable *table;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);
	table = e_task_table_get_table (task_table);

	print_table (
		table, _("Print Tasks"), _("Tasks"),
		GTK_PRINT_OPERATION_ACTION_PREVIEW);
}

static void
action_task_list_properties_cb (GtkAction *action,
                                ETaskShellView *task_shell_view)
{
}

static void
action_task_mark_complete_cb (GtkAction *action,
                              ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskTable *task_table;
	ECalModel *model;
	GSList *list, *iter;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);
	list = e_task_table_get_selected (task_table);
	model = e_task_table_get_model (task_table);

	for (iter = list; iter != NULL; iter = iter->next) {
		ECalModelComponent *comp_data = iter->data;
		e_cal_model_tasks_mark_comp_complete (model, comp_data);
	}

	g_slist_free (list);
}

static void
action_task_mark_incomplete_cb (GtkAction *action,
                                ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskTable *task_table;
	ECalModel *model;
	GSList *list, *iter;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);
	list = e_task_table_get_selected (task_table);
	model = e_task_table_get_model (task_table);

	for (iter = list; iter != NULL; iter = iter->next) {
		ECalModelComponent *comp_data = iter->data;
		e_cal_model_tasks_mark_comp_incomplete (model, comp_data);
	}

	g_slist_free (list);
}

static void
action_task_new_cb (GtkAction *action,
                    ETaskShellView *task_shell_view)
{
}

static void
action_task_open_cb (GtkAction *action,
                     ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskTable *task_table;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);

	e_task_table_open_selected (task_table);
}

static void
action_task_open_url_cb (GtkAction *action,
                         ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskTable *task_table;
	ECalModelComponent *comp_data;
	icalproperty *prop;
	GdkScreen *screen;
	const gchar *uri;
	GSList *list;
	GError *error = NULL;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);

	list = e_task_table_get_selected (task_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;

	/* XXX We only open the URI of the first selected task. */
	prop = icalcomponent_get_first_property (
		comp_data->icalcomp, ICAL_URL_PROPERTY);
	g_return_if_fail (prop == NULL);

	screen = gtk_widget_get_screen (GTK_WIDGET (task_shell_view));
	uri = icalproperty_get_url (prop);
	gtk_show_uri (screen, uri, GDK_CURRENT_TIME, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}

static void
action_task_preview_cb (GtkToggleAction *action,
                        ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	gboolean visible;

	task_shell_content = task_shell_view->priv->task_shell_content;
	visible = gtk_toggle_action_get_active (action);
	e_task_shell_content_set_preview_visible (task_shell_content, visible);
}

static void
action_task_print_cb (GtkAction *action,
                      ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskTable *task_table;
	ECalModelComponent *comp_data;
	ECalComponent *comp;
	icalcomponent *clone;
	GtkPrintOperationAction print_action;
	GSList *list;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_memo_shell_content_get_task_table (task_shell_content);

	list = e_task_table_get_selected (task_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	/* XXX We only print the first selected task. */
	comp = e_cal_component_new ();
	clone = icalcomponent_new_clone (comp_data->icalcomp);
	print_action = GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG;
	e_cal_component_set_icalcomponent (comp, clone);
	print_comp (comp, comp_data->client, print_action);
	g_object_unref (comp);
}

static void
action_task_purge_cb (GtkAction *action,
                      ETaskShellView *task_shell_view)
{
}

static void
action_task_save_as_cb (GtkAction *action,
                        ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskTable *task_table;
	ECalModelComponent *comp_data;
	GSList *list;
	gchar *filename;
	gchar *string;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);

	list = e_task_table_get_selected (task_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	filename = e_file_dialog_save (_("Save as..."), NULL);
	if (filename == NULL)
		return;

	string = e_cal_component_as_string (
		comp_data->client, comp_data->icalcomp);
	if (string == NULL) {
		g_warning ("Could not convert task to a string");
		return;
	}

	e_write_file_uri (filename, string);

	g_free (filename);
	g_free (string);
}

static GtkActionEntry task_entries[] = {

	{ "task-assign",
	  NULL,
	  N_("_Assign Task"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_task_assign_cb) },

	{ "task-clipboard-copy",
	  GTK_STOCK_COPY,
	  NULL,
	  NULL,
	  N_("Copy selected tasks"),
	  G_CALLBACK (action_task_clipboard_copy_cb) },

	{ "task-clipboard-cut",
	  GTK_STOCK_CUT,
	  NULL,
	  NULL,
	  N_("Cut selected tasks"),
	  G_CALLBACK (action_task_clipboard_cut_cb) },

	{ "task-clipboard-paste",
	  GTK_STOCK_PASTE,
	  NULL,
	  NULL,
	  N_("Paste tasks from the clipboard"),
	  G_CALLBACK (action_task_clipboard_paste_cb) },

	{ "task-delete",
	  GTK_STOCK_DELETE,
	  N_("Delete Task"),
	  NULL,
	  N_("Delete selected tasks"),
	  G_CALLBACK (action_task_delete_cb) },

	{ "task-forward",
	  "mail-forward",
	  N_("_Forward as iCalendar"),
	  "<Control>f",
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_task_forward_cb) },

	{ "task-list-copy",
	  GTK_STOCK_COPY,
	  N_("Copy..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_task_list_copy_cb) },

	{ "task-list-delete",
	  GTK_STOCK_DELETE,
	  N_("_Delete"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_task_list_delete_cb) },

	{ "task-list-new",
	  "stock_todo",
	  N_("_New Task List"),
	  NULL,
	  N_("Create a new task list"),
	  G_CALLBACK (action_task_list_new_cb) },

	{ "task-list-print",
	  GTK_STOCK_PRINT,
	  NULL,
	  NULL,
	  N_("Print the list of tasks"),
	  G_CALLBACK (action_task_list_print_cb) },

	{ "task-list-print-preview",
	  GTK_STOCK_PRINT_PREVIEW,
	  NULL,
	  NULL,
	  N_("Preview the list of tasks to be printed"),
	  G_CALLBACK (action_task_list_print_preview_cb) },

	{ "task-list-properties",
	  GTK_STOCK_PROPERTIES,
	  NULL,
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_task_list_properties_cb) },

	{ "task-mark-complete",
	  NULL,
	  N_("_Mark as Complete"),
	  "<Control>k",
	  N_("Mark selected tasks as complete"),
	  G_CALLBACK (action_task_mark_complete_cb) },

	{ "task-new",
	  "stock_task",
	  N_("New _Task"),
	  NULL,
	  N_("Create a new task"),
	  G_CALLBACK (action_task_new_cb) },

	{ "task-open",
	  GTK_STOCK_OPEN,
	  N_("_Open Task"),
	  "<Control>o",
	  N_("View the selected task"),
	  G_CALLBACK (action_task_open_cb) },

	{ "task-open-url",
	  "applications-internet",
	  N_("Open _Web Page"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_task_open_url_cb) },

	{ "task-print",
	  GTK_STOCK_PRINT,
	  NULL,
	  NULL,
	  N_("Print the selected task"),
	  G_CALLBACK (action_task_print_cb) },

	{ "task-purge",
	  NULL,
	  N_("Purg_e"),
	  "<Control>e",
	  N_("Delete completed tasks"),
	  G_CALLBACK (action_task_purge_cb) },

	{ "task-save-as",
	  GTK_STOCK_SAVE_AS,
	  NULL,
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_task_save_as_cb) }
};

static GtkToggleActionEntry task_toggle_entries[] = {

	{ "task-preview",
	  NULL,
	  N_("Task _Preview"),
	  "<Control>m",
	  N_("Show task preview pane"),
	  G_CALLBACK (action_task_preview_cb),
	  TRUE }
};

void
e_task_shell_view_actions_init (ETaskShellView *task_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkActionGroup *action_group;
	GtkUIManager *manager;
	GConfBridge *bridge;
	GtkAction *action;
	GObject *object;
	const gchar *domain;
	const gchar *key;

	shell_view = E_SHELL_VIEW (task_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	manager = e_shell_window_get_ui_manager (shell_window);
	domain = GETTEXT_PACKAGE;

	e_load_ui_definition (manager, "evolution-tasks.ui");

	action_group = task_shell_view->priv->task_actions;
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_action_group_add_actions (
		action_group, task_entries,
		G_N_ELEMENTS (task_entries), task_shell_view);
	gtk_action_group_add_toggle_actions (
		action_group, task_toggle_entries,
		G_N_ELEMENTS (task_toggle_entries), task_shell_view);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);

	/* Bind GObject properties to GConf keys. */

	bridge = gconf_bridge_get ();

	object = G_OBJECT (ACTION (TASK_PREVIEW));
	key = "/apps/evolution/calendar/display/show_task_preview";
	gconf_bridge_bind_property (bridge, key, object, "active");

	/* Fine tuning. */

	action = ACTION (TASK_DELETE);
	g_object_set (action, "short-label", _("Delete"), NULL);
}

void
e_task_shell_view_actions_update (ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ECal *client;
	ETable *table;
	ECalModel *model;
	ETaskTable *task_table;
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkAction *action;
	const gchar *label;
	gboolean read_only = TRUE;
	gboolean sensitive;
	gint n_selected;

	shell_view = E_SHELL_VIEW (task_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);

	model = e_task_table_get_model (task_table);
	client = e_cal_model_get_default_client (model);

	table = e_task_table_get_table (task_table);
	n_selected = e_table_selected_count (table);

	if (client != NULL)
		e_cal_is_read_only (client, &read_only, NULL);

	action = ACTION (TASK_OPEN);
	sensitive = (n_selected == 1);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_CLIPBOARD_COPY);
	sensitive = (n_selected > 0);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_CLIPBOARD_CUT);
	sensitive = (n_selected > 0);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_CLIPBOARD_PASTE);
	sensitive = (n_selected > 0);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_DELETE);
	sensitive = (n_selected > 0) && !read_only;
	gtk_action_set_sensitive (action, sensitive);
	label = ngettext ("Delete Task", "Delete Tasks", n_selected);
	g_object_set (action, "label", label, NULL);
}
