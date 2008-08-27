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
	GtkWidget *content_widget;
	GtkWidget *sidebar_widget;
	GtkWidget *status_widget;
};

GType e_test_shell_view_type = 0;
static gpointer parent_class;

static void
test_shell_view_dispose (GObject *object)
{
	ETestShellViewPrivate *priv;

	priv = E_TEST_SHELL_VIEW_GET_PRIVATE (object);

	if (priv->content_widget != NULL) {
		g_object_unref (priv->content_widget);
		priv->content_widget = NULL;
	}

	if (priv->sidebar_widget != NULL) {
		g_object_unref (priv->sidebar_widget);
		priv->sidebar_widget = NULL;
	}

	if (priv->status_widget != NULL) {
		g_object_unref (priv->status_widget);
		priv->status_widget = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static GtkWidget *
test_shell_view_get_content_widget (EShellView *shell_view)
{
	ETestShellViewPrivate *priv;

	priv = E_TEST_SHELL_VIEW_GET_PRIVATE (shell_view);

	return priv->content_widget;
}

static GtkWidget *
test_shell_view_get_sidebar_widget (EShellView *shell_view)
{
	ETestShellViewPrivate *priv;

	priv = E_TEST_SHELL_VIEW_GET_PRIVATE (shell_view);

	return priv->sidebar_widget;
}

static GtkWidget *
test_shell_view_get_status_widget (EShellView *shell_view)
{
	ETestShellViewPrivate *priv;

	priv = E_TEST_SHELL_VIEW_GET_PRIVATE (shell_view);

	return priv->status_widget;
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

	shell_view_class->get_content_widget =
		test_shell_view_get_content_widget;
	shell_view_class->get_sidebar_widget =
		test_shell_view_get_sidebar_widget;
	shell_view_class->get_status_widget =
		test_shell_view_get_status_widget;
}

static void
test_shell_view_init (ETestShellView *test_shell_view)
{
	GtkWidget *widget;

	test_shell_view->priv =
		E_TEST_SHELL_VIEW_GET_PRIVATE (test_shell_view);

	widget = gtk_label_new ("Content Widget");
	test_shell_view->priv->content_widget = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	widget = gtk_label_new ("Sidebar Widget");
	test_shell_view->priv->sidebar_widget = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	widget = gtk_label_new ("Status Widget");
	test_shell_view->priv->status_widget = g_object_ref_sink (widget);
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
