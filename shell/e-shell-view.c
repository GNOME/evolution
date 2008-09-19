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
#include <e-shell-taskbar.h>
#include <e-shell-window.h>
#include <e-shell-window-actions.h>

#define E_SHELL_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SHELL_VIEW, EShellViewPrivate))

struct _EShellViewPrivate {

	gpointer shell_window;  /* weak pointer */

	gchar *title;
	gchar *view_id;
	gint page_num;

	GtkAction *action;
	GtkWidget *shell_content;
	GtkWidget *shell_sidebar;
	GtkWidget *shell_taskbar;
};

enum {
	PROP_0,
	PROP_ACTION,
	PROP_PAGE_NUM,
	PROP_TITLE,
	PROP_SHELL_CONTENT,
	PROP_SHELL_SIDEBAR,
	PROP_SHELL_TASKBAR,
	PROP_SHELL_WINDOW,
	PROP_VIEW_ID
};

enum {
	CHANGED,
	LAST_SIGNAL
};

static gpointer parent_class;
static gulong signals[LAST_SIGNAL];

static void
shell_view_init_view_collection (EShellViewClass *shell_view_class)
{
	EShellModule *shell_module;
	const gchar *base_dir;
	const gchar *module_name;
	gchar *system_dir;
	gchar *local_dir;

	shell_module = E_SHELL_MODULE (shell_view_class->type_module);
	module_name = shell_view_class->type_module->name;

	base_dir = EVOLUTION_GALVIEWSDIR;
	system_dir = g_build_filename (base_dir, module_name, NULL);

	base_dir = e_shell_module_get_data_dir (shell_module);
	local_dir = g_build_filename (base_dir, "views", NULL);

	/* The view collection is never destroyed. */
	shell_view_class->view_collection = gal_view_collection_new ();

	gal_view_collection_set_title (
		shell_view_class->view_collection,
		shell_view_class->label);

	gal_view_collection_set_storage_directories (
		shell_view_class->view_collection,
		system_dir, local_dir);

	g_free (system_dir);
	g_free (local_dir);

	/* This is all we can do.  It's up to the subclasses to
	 * add the appropriate factories to the view collection. */
}

static void
shell_view_set_action (EShellView *shell_view,
                       GtkAction *action)
{
	gchar *label;

	g_return_if_fail (shell_view->priv->action == NULL);

	shell_view->priv->action = g_object_ref (action);

	g_object_get (action, "label", &label, NULL);
	e_shell_view_set_title (shell_view, label);
	g_free (label);
}

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
		case PROP_ACTION:
			shell_view_set_action (
				E_SHELL_VIEW (object),
				g_value_get_object (value));
			return;

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

		case PROP_VIEW_ID:
			e_shell_view_set_view_id (
				E_SHELL_VIEW (object),
				g_value_get_string (value));
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
		case PROP_ACTION:
			g_value_set_object (
				value, e_shell_view_get_action (
				E_SHELL_VIEW (object)));
			return;

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

		case PROP_SHELL_CONTENT:
			g_value_set_object (
				value, e_shell_view_get_shell_content (
				E_SHELL_VIEW (object)));
			return;

		case PROP_SHELL_SIDEBAR:
			g_value_set_object (
				value, e_shell_view_get_shell_sidebar (
				E_SHELL_VIEW (object)));
			return;

		case PROP_SHELL_TASKBAR:
			g_value_set_object (
				value, e_shell_view_get_shell_taskbar (
				E_SHELL_VIEW (object)));
			return;

		case PROP_SHELL_WINDOW:
			g_value_set_object (
				value, e_shell_view_get_shell_window (
				E_SHELL_VIEW (object)));
			return;

		case PROP_VIEW_ID:
			g_value_set_string (
				value, e_shell_view_get_view_id (
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

	if (priv->shell_content != NULL) {
		g_object_unref (priv->shell_content);
		priv->shell_content = NULL;
	}

	if (priv->shell_sidebar != NULL) {
		g_object_unref (priv->shell_sidebar);
		priv->shell_sidebar = NULL;
	}

	if (priv->shell_taskbar != NULL) {
		g_object_unref (priv->shell_taskbar);
		priv->shell_taskbar = NULL;
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
	g_free (priv->view_id);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
shell_view_constructed (GObject *object)
{
	EShellView *shell_view;
	EShellViewClass *class;
	GtkWidget *widget;

	shell_view = E_SHELL_VIEW (object);
	class = E_SHELL_VIEW_GET_CLASS (object);

	/* Invoke factory methods. */

	widget = class->new_shell_content (shell_view);
	shell_view->priv->shell_content = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	widget = class->new_shell_sidebar (shell_view);
	shell_view->priv->shell_sidebar = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	widget = class->new_shell_taskbar (shell_view);
	shell_view->priv->shell_taskbar = g_object_ref_sink (widget);
	gtk_widget_show (widget);
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

	/* Default Factories */
	class->new_shell_content = e_shell_content_new;
	class->new_shell_sidebar = e_shell_sidebar_new;
	class->new_shell_taskbar = e_shell_taskbar_new;

	g_object_class_install_property (
		object_class,
		PROP_ACTION,
		g_param_spec_object (
			"action",
			_("Switcher Action"),
			_("The switcher action for this shell view"),
			GTK_TYPE_RADIO_ACTION,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

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
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SHELL_CONTENT,
		g_param_spec_object (
			"shell-content",
			_("Shell Content Widget"),
			_("The content widget appears in "
			  "a shell window's right pane"),
			E_TYPE_SHELL_CONTENT,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_SHELL_SIDEBAR,
		g_param_spec_object (
			"shell-sidebar",
			_("Shell Sidebar Widget"),
			_("The sidebar widget appears in "
			  "a shell window's left pane"),
			E_TYPE_SHELL_SIDEBAR,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_SHELL_TASKBAR,
		g_param_spec_object (
			"shell-taskbar",
			_("Shell Taskbar Widget"),
			_("The taskbar widget appears at "
			  "the bottom of a shell window"),
			E_TYPE_SHELL_TASKBAR,
			G_PARAM_READABLE));

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
		PROP_VIEW_ID,
		g_param_spec_string (
			"view-id",
			_("Current View ID"),
			_("The current view ID"),
			NULL,
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
shell_view_init (EShellView *shell_view,
                 EShellViewClass *shell_view_class)
{
	shell_view->priv = E_SHELL_VIEW_GET_PRIVATE (shell_view);

	if (shell_view_class->view_collection == NULL)
		shell_view_init_view_collection (shell_view_class);
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
	GtkAction *action;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	action = e_shell_view_get_action (shell_view);

	/* Switcher actions have a secret "view-name" data value.
	 * This gets set in e_shell_window_create_switcher_actions(). */
	return g_object_get_data (G_OBJECT (action), "view-name");
}

GtkAction *
e_shell_view_get_action (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->action;
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

	if (g_strcmp0 (shell_view->priv->title, title) == 0)
		return;

	g_free (shell_view->priv->title);
	shell_view->priv->title = g_strdup (title);

	g_object_notify (G_OBJECT (shell_view), "title");
}

const gchar *
e_shell_view_get_view_id (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->view_id;
}

void
e_shell_view_set_view_id (EShellView *shell_view,
                          const gchar *view_id)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	if (g_strcmp0 (shell_view->priv->view_id, view_id) == 0)
		return;

	g_free (shell_view->priv->view_id);
	shell_view->priv->view_id = g_strdup (view_id);

	g_object_notify (G_OBJECT (shell_view), "view-id");
}

EShellWindow *
e_shell_view_get_shell_window (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return E_SHELL_WINDOW (shell_view->priv->shell_window);
}

gboolean
e_shell_view_is_active (EShellView *shell_view)
{
	GtkAction *action;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), FALSE);

	action = e_shell_view_get_action (shell_view);

	return gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
}

void
e_shell_view_add_activity (EShellView *shell_view,
                           EActivity *activity)
{
	EShellViewClass *shell_view_class;
	EShellModule *shell_module;

	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (E_IS_ACTIVITY (activity));

	shell_view_class = E_SHELL_VIEW_GET_CLASS (shell_view);
	shell_module = E_SHELL_MODULE (shell_view_class->type_module);
	e_shell_module_add_activity (shell_module, activity);
}

gint
e_shell_view_get_page_num (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), -1);

	return shell_view->priv->page_num;
}

gpointer
e_shell_view_get_shell_content (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->shell_content;
}

gpointer
e_shell_view_get_shell_sidebar (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->shell_sidebar;
}

gpointer
e_shell_view_get_shell_taskbar (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->shell_taskbar;
}

void
e_shell_view_changed (EShellView *shell_view)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	g_signal_emit (shell_view, signals[CHANGED], 0);
}
