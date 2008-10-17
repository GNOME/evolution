/*
 * e-task-shell-view.c
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

enum {
	PROP_0,
	PROP_SOURCE_LIST
};

GType e_task_shell_view_type = 0;
static gpointer parent_class;

static void
task_shell_view_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SOURCE_LIST:
			g_value_set_object (
				value, e_task_shell_view_get_source_list (
				E_TASK_SHELL_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
task_shell_view_dispose (GObject *object)
{
	e_task_shell_view_private_dispose (E_TASK_SHELL_VIEW (object));

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
task_shell_view_finalize (GObject *object)
{
	e_task_shell_view_private_finalize (E_TASK_SHELL_VIEW (object));

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
task_shell_view_constructed (GObject *object)
{
	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	e_task_shell_view_private_constructed (E_TASK_SHELL_VIEW (object));
}

static void
task_shell_view_update_actions (EShellView *shell_view)
{
	ETaskShellViewPrivate *priv;
	ETaskShellContent *task_shell_content;
	ETaskShellSidebar *task_shell_sidebar;
	EShellWindow *shell_window;
	ESourceSelector *selector;
	ETable *table;
	ECalendarTable *task_table;
	ESource *source;
	GtkAction *action;
	GSList *list, *iter;
	const gchar *label;
	const gchar *uri = NULL;
	gboolean user_created_source;
	gboolean assignable = TRUE;
	gboolean editable = TRUE;
	gboolean has_url = FALSE;
	gboolean sensitive;
	gint n_selected;
	gint n_complete = 0;
	gint n_incomplete = 0;

	priv = E_TASK_SHELL_VIEW_GET_PRIVATE (shell_view);

	shell_window = e_shell_view_get_shell_window (shell_view);

	task_shell_content = priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);

	task_shell_sidebar = priv->task_shell_sidebar;
	selector = e_task_shell_sidebar_get_selector (task_shell_sidebar);

	table = e_calendar_table_get_table (task_table);
	n_selected = e_table_selected_count (table);

	list = e_calendar_table_get_selected (task_table);
	for (iter = list; iter != NULL; iter = iter->next) {
		ECalModelComponent *comp_data = iter->data;
		icalproperty *prop;
		const gchar *cap;
		gboolean read_only;

		e_cal_is_read_only (comp_data->client, &read_only, NULL);
		editable &= !read_only;

		cap = CAL_STATIC_CAPABILITY_NO_TASK_ASSIGNMENT;
		if (e_cal_get_static_capability (comp_data->client, cap))
			assignable = FALSE;

		cap = CAL_STATIC_CAPABILITY_NO_CONV_TO_ASSIGN_TASK;
		if (e_cal_get_static_capability (comp_data->client, cap))
			assignable = FALSE;

		prop = icalcomponent_get_first_property (
			comp_data->icalcomp, ICAL_URL_PROPERTY);
		has_url |= (prop != NULL);

		prop = icalcomponent_get_first_property (
			comp_data->icalcomp, ICAL_COMPLETED_PROPERTY);
		if (prop != NULL)
			n_complete++;
		else
			n_incomplete++;
	}
	g_slist_free (list);

	source = e_source_selector_peek_primary_selection (selector);
	if (source != NULL)
		uri = e_source_peek_relative_uri (source);
	user_created_source = (uri != NULL && strcmp (uri, "system") != 0);

	action = ACTION (TASK_ASSIGN);
	sensitive = (n_selected == 1) && editable && assignable;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_CLIPBOARD_COPY);
	sensitive = (n_selected > 0);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_CLIPBOARD_CUT);
	sensitive = (n_selected > 0) && editable;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_CLIPBOARD_PASTE);
	sensitive = editable;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_DELETE);
	sensitive = (n_selected > 0) && editable;
	gtk_action_set_sensitive (action, sensitive);
	label = ngettext ("Delete Task", "Delete Tasks", n_selected);
	g_object_set (action, "label", label, NULL);

	action = ACTION (TASK_FORWARD);
	sensitive = (n_selected == 1);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_LIST_COPY);
	sensitive = (source != NULL);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_LIST_DELETE);
	sensitive = user_created_source;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_LIST_PROPERTIES);
	sensitive = (source != NULL);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_MARK_COMPLETE);
	sensitive = (n_selected > 0) && editable && (n_incomplete > 0);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_MARK_INCOMPLETE);
	sensitive = (n_selected > 0) && editable && (n_complete > 0);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_OPEN);
	sensitive = (n_selected == 1);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_OPEN_URL);
	sensitive = (n_selected == 1) && has_url;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_PRINT);
	sensitive = (n_selected == 1);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_PURGE);
	sensitive = editable;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (TASK_SAVE_AS);
	sensitive = (n_selected == 1);
	gtk_action_set_sensitive (action, sensitive);
}

static void
task_shell_view_class_init (ETaskShellViewClass *class,
                            GTypeModule *type_module)
{
	GObjectClass *object_class;
	EShellViewClass *shell_view_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (ETaskShellViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = task_shell_view_get_property;
	object_class->dispose = task_shell_view_dispose;
	object_class->finalize = task_shell_view_finalize;
	object_class->constructed = task_shell_view_constructed;

	shell_view_class = E_SHELL_VIEW_CLASS (class);
	shell_view_class->label = _("Tasks");
	shell_view_class->icon_name = "evolution-tasks";
	shell_view_class->ui_definition = "evolution-tasks.ui";
	shell_view_class->search_options = "/task-search-options";
	shell_view_class->search_rules = "tasktypes.xml";
	shell_view_class->type_module = type_module;
	shell_view_class->new_shell_content = e_task_shell_content_new;
	shell_view_class->new_shell_sidebar = e_task_shell_sidebar_new;
	shell_view_class->update_actions = task_shell_view_update_actions;

	g_object_class_install_property (
		object_class,
		PROP_SOURCE_LIST,
		g_param_spec_object (
			"source-list",
			_("Source List"),
			_("The registry of task lists"),
			E_TYPE_SOURCE_LIST,
			G_PARAM_READABLE));
}

static void
task_shell_view_init (ETaskShellView *task_shell_view,
                      EShellViewClass *shell_view_class)
{
	task_shell_view->priv =
		E_TASK_SHELL_VIEW_GET_PRIVATE (task_shell_view);

	e_task_shell_view_private_init (task_shell_view, shell_view_class);
}

GType
e_task_shell_view_get_type (GTypeModule *type_module)
{
	if (e_task_shell_view_type == 0) {
		const GTypeInfo type_info = {
			sizeof (ETaskShellViewClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) task_shell_view_class_init,
			(GClassFinalizeFunc) NULL,
			type_module,
			sizeof (ETaskShellView),
			0,    /* n_preallocs */
			(GInstanceInitFunc) task_shell_view_init,
			NULL  /* value_table */
		};

		e_task_shell_view_type =
			g_type_module_register_type (
				type_module, E_TYPE_SHELL_VIEW,
				"ETaskShellView", &type_info, 0);
	}

	return e_task_shell_view_type;
}

ESourceList *
e_task_shell_view_get_source_list (ETaskShellView *task_shell_view)
{
	g_return_val_if_fail (E_IS_TASK_SHELL_VIEW (task_shell_view), NULL);

	return task_shell_view->priv->source_list;
}
