/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-test-shell-view.c
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

#include "e-test-shell-view.h"

#define E_TEST_SHELL_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_TEST_SHELL_VIEW, ETestShellViewPrivate))

struct _ETestShellViewPrivate {
	gint dummy;
};

GType e_test_shell_view_type = 0;
static gpointer parent_class;

static void
test_shell_view_changed (EShellView *shell_view)
{
	gboolean is_selected;
	const gchar *selected;

	is_selected = e_shell_view_is_selected (shell_view);
	selected = is_selected ? "selected" : "not selected";
	g_debug ("%s (%s)", G_STRFUNC, selected);
}

static void
test_shell_view_class_init (ETestShellViewClass *class,
                            GTypeModule *type_module)
{
	EShellViewClass *shell_view_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (ETestShellViewPrivate));

	shell_view_class = E_SHELL_VIEW_CLASS (class);
	shell_view_class->label = "Test";
	shell_view_class->icon_name = "face-monkey";
	shell_view_class->type_module = type_module;
	shell_view_class->changed = test_shell_view_changed;
}

static void
test_shell_view_init (ETestShellView *test_shell_view)
{
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EShellView *shell_view;
	GtkWidget *widget;

	test_shell_view->priv =
		E_TEST_SHELL_VIEW_GET_PRIVATE (test_shell_view);

	shell_view = E_SHELL_VIEW (test_shell_view);
	shell_content = e_shell_view_get_content (shell_view);
	shell_sidebar = e_shell_view_get_sidebar (shell_view);

	widget = gtk_label_new ("Content Widget");
	gtk_container_add (GTK_CONTAINER (shell_content), widget);
	gtk_widget_show (widget);

	widget = gtk_label_new ("Sidebar Widget");
	gtk_container_add (GTK_CONTAINER (shell_sidebar), widget);
	gtk_widget_show (widget);
}

GType
e_test_shell_view_get_type (GTypeModule *type_module)
{
	if (e_test_shell_view_type == 0) {
		const GTypeInfo type_info = {
			sizeof (ETestShellViewClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) test_shell_view_class_init,
			(GClassFinalizeFunc) NULL,
			type_module,
			sizeof (ETestShellView),
			0,    /* n_preallocs */
			(GInstanceInitFunc) test_shell_view_init,
			NULL  /* value_table */
		};

		e_test_shell_view_type =
			g_type_module_register_type (
				type_module, E_TYPE_SHELL_VIEW,
				"ETestShellView", &type_info, 0);
	}

	return e_test_shell_view_type;
}
