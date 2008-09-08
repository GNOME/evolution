/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 * e-shell-view.c
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

#include "e-shell-view.h"

#include <string.h>
#include <glib/gi18n.h>

#include <e-shell-content.h>
#include <e-shell-module.h>
#include <e-shell-sidebar.h>
#include <e-shell-window.h>
#include <e-shell-window-actions.h>

#define E_SHELL_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SHELL_VIEW, EShellViewPrivate))

struct _EShellViewPrivate {

	gpointer shell_window;  /* weak pointer */

	gchar *title;
	gint page_num;

	GtkWidget *content;
	GtkWidget *sidebar;
	GtkWidget *taskbar;

	GalViewInstance *view_instance;
};

enum {
	PROP_0,
	PROP_PAGE_NUM,
	PROP_TITLE,
	PROP_SHELL_WINDOW,
	PROP_VIEW_INSTANCE
};

enum {
	CHANGED,
	LAST_SIGNAL
};

static gpointer parent_class;
static gulong signals[LAST_SIGNAL];

static void
shell_view_set_page_num (EShellView *shell_view,
                         gint page_num)
{
	shell_view->priv->page_num = page_num;
}

static void
shell_view_set_shell_window (EShellView *shell_view,
                             GtkWidget *shell_window)
{
	g_return_if_fail (shell_view->priv->shell_window == NULL);

	shell_view->priv->shell_window = shell_window;

	g_object_add_weak_pointer (
		G_OBJECT (shell_window),
		&shell_view->priv->shell_window);
}

static void
shell_view_set_property (GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PAGE_NUM:
			shell_view_set_page_num (
				E_SHELL_VIEW (object),
				g_value_get_int (value));
			return;

		case PROP_TITLE:
			e_shell_view_set_title (
				E_SHELL_VIEW (object),
				g_value_get_string (value));
			return;

		case PROP_SHELL_WINDOW:
			shell_view_set_shell_window (
				E_SHELL_VIEW (object),
				g_value_get_object (value));
			return;

		case PROP_VIEW_INSTANCE:
			e_shell_view_set_view_instance (
				E_SHELL_VIEW (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_view_get_property (GObject *object,
                         guint property_id,
                         GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PAGE_NUM:
			g_value_set_int (
				value, e_shell_view_get_page_num (
				E_SHELL_VIEW (object)));
			return;

		case PROP_TITLE:
			g_value_set_string (
				value, e_shell_view_get_title (
				E_SHELL_VIEW (object)));
			return;

		case PROP_SHELL_WINDOW:
			g_value_set_object (
				value, e_shell_view_get_shell_window (
				E_SHELL_VIEW (object)));
			return;

		case PROP_VIEW_INSTANCE:
			g_value_set_object (
				value, e_shell_view_get_view_instance (
				E_SHELL_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_view_dispose (GObject *object)
{
	EShellViewPrivate *priv;

	priv = E_SHELL_VIEW_GET_PRIVATE (object);

	if (priv->shell_window != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (priv->shell_window), &priv->shell_window);
		priv->shell_window = NULL;
	}

	if (priv->content != NULL) {
		g_object_unref (priv->content);
		priv->content = NULL;
	}

	if (priv->sidebar != NULL) {
		g_object_unref (priv->sidebar);
		priv->sidebar = NULL;
	}

	if (priv->taskbar != NULL) {
		g_object_unref (priv->taskbar);
		priv->taskbar = NULL;
	}

	if (priv->view_instance != NULL) {
		g_object_unref (priv->view_instance);
		priv->view_instance = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
shell_view_finalize (GObject *object)
{
	EShellViewPrivate *priv;

	priv = E_SHELL_VIEW_GET_PRIVATE (object);

	g_free (priv->title);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
shell_view_constructed (GObject *object)
{
	EShellView *shell_view;
	GtkWidget *widget;

	shell_view = E_SHELL_VIEW (object);

	/* We do this AFTER instance initialization so the
	 * E_SHELL_VIEW_GET_CLASS() macro works properly. */

	widget = e_shell_content_new (shell_view);
	shell_view->priv->content = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	widget = e_shell_sidebar_new (shell_view);
	shell_view->priv->sidebar = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	widget = e_shell_taskbar_new (shell_view);
	shell_view->priv->taskbar = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	/* XXX GObjectClass doesn't implement constructed(), so we will.
	 *     Then subclasses won't have to check the function pointer
	 *     before chaining up.
	 *
	 *     http://bugzilla.gnome.org/show_bug?id=546593 */
}

static void
shell_view_class_init (EShellViewClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EShellViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = shell_view_set_property;
	object_class->get_property = shell_view_get_property;
	object_class->dispose = shell_view_dispose;
	object_class->finalize = shell_view_finalize;
	object_class->constructed = shell_view_constructed;

	g_object_class_install_property (
		object_class,
		PROP_PAGE_NUM,
		g_param_spec_int (
			"page-num",
			_("Page Number"),
			_("The notebook page number of the shell view"),
			-1,
			G_MAXINT,
			-1,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_TITLE,
		g_param_spec_string (
			"title",
			_("Title"),
			_("The title of the shell view"),
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_SHELL_WINDOW,
		g_param_spec_object (
			"shell-window",
			_("Shell Window"),
			_("The window to which the shell view belongs"),
			E_TYPE_SHELL_WINDOW,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_VIEW_INSTANCE,
		g_param_spec_object (
			"view-instance",
			_("GAL View Instance"),
			_("The GAL view instance for the shell view"),
			GAL_VIEW_INSTANCE_TYPE,
			G_PARAM_READWRITE));

	signals[CHANGED] = g_signal_new (
		"changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EShellViewClass, changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
shell_view_init (EShellView *shell_view)
{
	shell_view->priv = E_SHELL_VIEW_GET_PRIVATE (shell_view);
}

GType
e_shell_view_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		const GTypeInfo type_info = {
			sizeof (EShellViewClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) shell_view_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EShellView),
			0,     /* n_preallocs */
			(GInstanceInitFunc) shell_view_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			G_TYPE_OBJECT, "EShellView",
			&type_info, G_TYPE_FLAG_ABSTRACT);
	}

	return type;
}

const gchar *
e_shell_view_get_name (EShellView *shell_view)
{
	EShellViewClass *class;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	/* A shell view's name is taken from the name of the
	 * module that registered the shell view subclass. */

	class = E_SHELL_VIEW_GET_CLASS (shell_view);
	g_return_val_if_fail (class->type_module != NULL, NULL);
	g_return_val_if_fail (class->type_module->name != NULL, NULL);

	return class->type_module->name;
}

const gchar *
e_shell_view_get_title (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->title;
}

void
e_shell_view_set_title (EShellView *shell_view,
                        const gchar *title)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	if (title == NULL)
		title = E_SHELL_VIEW_GET_CLASS (shell_view)->label;

	g_free (shell_view->priv->title);
	shell_view->priv->title = g_strdup (title);

	g_object_notify (G_OBJECT (shell_view), "title");
}

GalViewInstance *
e_shell_view_get_view_instance (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->view_instance;
}

void
e_shell_view_set_view_instance (EShellView *shell_view,
                                GalViewInstance *instance)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	if (instance != NULL)
		g_return_if_fail (GAL_IS_VIEW_INSTANCE (instance));

	if (shell_view->priv->view_instance != NULL) {
		g_object_unref (shell_view->priv->view_instance);
		shell_view->priv->view_instance = NULL;
	}		

	if (instance != NULL)
		shell_view->priv->view_instance = g_object_ref (instance);

	g_object_notify (G_OBJECT (shell_view), "view-instance");
}

EShellWindow *
e_shell_view_get_shell_window (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return E_SHELL_WINDOW (shell_view->priv->shell_window);
}

gboolean
e_shell_view_is_selected (EShellView *shell_view)
{
	EShellViewClass *class;
	EShellWindow *shell_window;
	const gchar *curr_view_name;
	const gchar *this_view_name;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), FALSE);

	class = E_SHELL_VIEW_GET_CLASS (shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	this_view_name = e_shell_view_get_name (shell_view);
	curr_view_name = e_shell_window_get_current_view (shell_window);
	g_return_val_if_fail (curr_view_name != NULL, FALSE);

	return (strcmp (curr_view_name, this_view_name) == 0);
}

gint
e_shell_view_get_page_num (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), -1);

	return shell_view->priv->page_num;
}

EShellContent *
e_shell_view_get_content (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return E_SHELL_CONTENT (shell_view->priv->content);
}

EShellSidebar *
e_shell_view_get_sidebar (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return E_SHELL_SIDEBAR (shell_view->priv->sidebar);
}

EShellTaskbar *
e_shell_view_get_taskbar (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return E_SHELL_TASKBAR (shell_view->priv->taskbar);
}

void
e_shell_view_changed (EShellView *shell_view)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	g_signal_emit (shell_view, signals[CHANGED], 0);
}
