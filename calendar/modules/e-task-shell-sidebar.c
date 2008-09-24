/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-task-shell-sidebar.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "e-task-shell-sidebar.h"

#include <glib/gi18n.h>

#include "calendar/gui/e-calendar-selector.h"

#include "e-task-shell-view.h"

#define E_TASK_SHELL_SIDEBAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_TASK_SHELL_SIDEBAR, ETaskShellSidebarPrivate))

struct _ETaskShellSidebarPrivate {
	GtkWidget *selector;
};

enum {
	PROP_0,
	PROP_SELECTOR
};

static gpointer parent_class;

#if 0  /* MOVE THIS TO ETaskShellView */
static void
task_shell_sidebar_update (EShellSidebar *shell_sidebar)
{
	EShellView *shell_view;
	ETasks *tasks;
	ETable *table;
	ECalModel *model;
	ECalendarTable *cal_table;
	GString *string;
	const gchar *format;
	gint n_rows;
	gint n_selected;

	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	tasks = e_task_shell_view_get_tasks (E_TASK_SHELL_VIEW (shell_view));
	cal_table = e_tasks_get_calendar_table (tasks);
	model = e_calendar_table_get_model (cal_table);
	table = e_calendar_table_get_table (cal_table);

	n_rows = e_table_model_get_row_count (model);
	n_selected = e_table_selected_count (table);

	string = g_string_sized_new (64);

	format = ngettext ("%d task", "%d tasks", n_rows);
	g_string_append_printf (string, format, n_rows);

	if (n_selected > 0) {
		format = _("%d selected");
		g_string_append_len (string, ", ", 2);
		g_string_append_printf (string, format, n_selected);
	}

	e_shell_sidebar_set_secondary_text (shell_sidebar, string->str);

	g_string_free (string, TRUE);
}
#endif

static void
task_shell_sidebar_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SELECTOR:
			g_value_set_object (
				value, e_task_shell_sidebar_get_selector (
				E_TASK_SHELL_SIDEBAR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
task_shell_sidebar_dispose (GObject *object)
{
	ETaskShellSidebarPrivate *priv;

	priv = E_TASK_SHELL_SIDEBAR_GET_PRIVATE (object);

	if (priv->selector != NULL) {
		g_object_unref (priv->selector);
		priv->selector = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
task_shell_sidebar_constructed (GObject *object)
{
	ETaskShellSidebarPrivate *priv;
	EShellView *shell_view;
	EShellSidebar *shell_sidebar;
	ETaskShellView *task_shell_view;
	ESourceList *source_list;
	GtkContainer *container;
	GtkWidget *widget;

	priv = E_TASK_SHELL_SIDEBAR_GET_PRIVATE (object);

	shell_sidebar = E_SHELL_SIDEBAR (object);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	task_shell_view = E_TASK_SHELL_VIEW (shell_view);
	source_list = e_task_shell_view_get_source_list (task_shell_view);

	container = GTK_CONTAINER (shell_sidebar);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_container_add (container, widget);
	gtk_widget_show (widget);

	container = GTK_CONTAINER (widget);

	widget = e_calendar_selector_new (source_list);
	e_source_selector_set_select_new (E_SOURCE_SELECTOR (widget), TRUE);
	gtk_container_add (container, widget);
	priv->selector = g_object_ref (widget);
	gtk_widget_show (widget);

#if 0  /* MOVE THIS TO ETaskShellView */

	/* Setup signal handlers. */

	tasks = e_task_shell_view_get_tasks (task_shell_view);
	cal_table = e_tasks_get_calendar_table (tasks);
	model = e_calendar_table_get_model (cal_table);
	table = e_calendar_table_get_table (cal_table);

	g_signal_connect_swapped (
		model, "model-changed",
		G_CALLBACK (task_shell_sidebar_update),
		shell_sidebar);

	g_signal_connect_swapped (
		model, "model-rows-deleted",
		G_CALLBACK (task_shell_sidebar_update),
		shell_sidebar);

	g_signal_connect_swapped (
		model, "model-rows-inserted",
		G_CALLBACK (task_shell_sidebar_update),
		shell_sidebar);

	g_signal_connect_swapped (
		model, "selection-change",
		G_CALLBACK (task_shell_sidebar_update),
		shell_sidebar);

	task_shell_sidebar_update (shell_sidebar);

#endif
}

static void
task_shell_sidebar_class_init (ETaskShellSidebarClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (ETaskShellSidebarPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = task_shell_sidebar_get_property;
	object_class->dispose = task_shell_sidebar_dispose;
	object_class->constructed = task_shell_sidebar_constructed;

	g_object_class_install_property (
		object_class,
		PROP_SELECTOR,
		g_param_spec_object (
			"selector",
			_("Source Selector Widget"),
			_("This widget displays groups of task lists"),
			E_TYPE_SOURCE_SELECTOR,
			G_PARAM_READABLE));
}

static void
task_shell_sidebar_init (ETaskShellSidebar *task_shell_sidebar)
{
	task_shell_sidebar->priv =
		E_TASK_SHELL_SIDEBAR_GET_PRIVATE (task_shell_sidebar);

	/* Postpone widget construction until we have a shell view. */
}

GType
e_task_shell_sidebar_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (ETaskShellSidebarClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) task_shell_sidebar_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (ETaskShellSidebar),
			0,     /* n_preallocs */
			(GInstanceInitFunc) task_shell_sidebar_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_SHELL_SIDEBAR, "ETaskShellSidebar",
			&type_info, 0);
	}

	return type;
}

GtkWidget *
e_task_shell_sidebar_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_TASK_SHELL_SIDEBAR,
		"shell-view", shell_view, NULL);
}

GtkWidget *
e_task_shell_sidebar_get_selector (ETaskShellSidebar *task_shell_sidebar)
{
	g_return_val_if_fail (
		E_IS_TASK_SHELL_SIDEBAR (task_shell_sidebar), NULL);

	return task_shell_sidebar->priv->selector;
}
