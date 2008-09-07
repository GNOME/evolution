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

#include <filter/rule-context.h>
#include <widgets/misc/e-search-bar.h>
#include <widgets/misc/e-task-bar.h>

#include <e-shell-content.h>
#include <e-shell-module.h>
#include <e-shell-sidebar.h>
#include <e-shell-window.h>
#include <e-shell-window-actions.h>

#define E_SHELL_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SHELL_VIEW, EShellViewPrivate))

struct _EShellViewPrivate {
	gchar *title;
	gint page_num;
	gpointer window;  /* weak pointer */

	GtkWidget *content;
	GtkWidget *sidebar;
	GtkWidget *taskbar;

	GalViewInstance *view_instance;
};

enum {
	PROP_0,
	PROP_PAGE_NUM,
	PROP_TITLE,
	PROP_VIEW_INSTANCE,
	PROP_WINDOW
};

enum {
	CHANGED,
	LAST_SIGNAL
};

static gpointer parent_class;
static gulong signals[LAST_SIGNAL];

static void
shell_view_setup_search_context (EShellView *shell_view)
{
	RuleContext *context;
	EShellViewClass *class;
	EShellModule *shell_module;
	FilterRule *rule;
	FilterPart *part;
	GtkWidget *widget;
	gchar *system_filename;
	gchar *user_filename;

	class = E_SHELL_VIEW_GET_CLASS (shell_view);
	shell_module = E_SHELL_MODULE (class->type_module);

	/* The filename for built-in searches is specified in a
	 * module's EShellModuleInfo.  All built-in search rules
	 * live in the same directory. */
	system_filename = g_build_filename (
		EVOLUTION_RULEDIR,
		e_shell_module_get_searches (shell_module), NULL);

	/* The filename for custom saved searches is always of
	 * the form "$(shell_module_data_dir)/searches.xml". */
	user_filename = g_build_filename (
		e_shell_module_get_data_dir (shell_module),
		"searches.xml", NULL);

	context = rule_context_new ();
	rule_context_add_part_set (
		context, "partset", FILTER_TYPE_PART,
		rule_context_add_part, rule_context_next_part);
	rule_context_add_rule_set (
		context, "ruleset", FILTER_TYPE_RULE,
		rule_context_add_rule, rule_context_next_rule);
	rule_context_load (context, system_filename, user_filename);

	/* XXX Not sure why this is necessary. */
	g_object_set_data_full (
		G_OBJECT (context), "system", system_filename, g_free);
	g_object_set_data_full (
		G_OBJECT (context), "user", user_filename, g_free);

	/* XXX I don't really understand what this does. */
	rule = filter_rule_new ();
	part = rule_context_next_part (context, NULL);
	if (part == NULL)
		g_warning (
			"Could not load %s search; no parts.",
			class->type_module->name);
	else
		filter_rule_add_part (rule, filter_part_clone (part));

	g_free (system_filename);
	g_free (user_filename);

	/* Hand the context off to the search bar. */
	widget = e_shell_view_get_content_widget (shell_view);
	widget = e_shell_content_get_search_bar (E_SHELL_CONTENT (widget));
	e_search_bar_set_context (E_SEARCH_BAR (widget), context);

	g_object_unref (context);
}

static void
shell_view_set_page_num (EShellView *shell_view,
                         gint page_num)
{
	shell_view->priv->page_num = page_num;
}

static void
shell_view_set_window (EShellView *shell_view,
                       GtkWidget *window)
{
	g_return_if_fail (GTK_IS_WINDOW (window));

	shell_view->priv->window = window;

	g_object_add_weak_pointer (
		G_OBJECT (window), &shell_view->priv->window);
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

		case PROP_VIEW_INSTANCE:
			e_shell_view_set_view_instance (
				E_SHELL_VIEW (object),
				g_value_get_object (value));
			return;

		case PROP_WINDOW:
			shell_view_set_window (
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

		case PROP_VIEW_INSTANCE:
			g_value_set_object (
				value, e_shell_view_get_view_instance (
				E_SHELL_VIEW (object)));
			return;

		case PROP_WINDOW:
			g_value_set_object (
				value, e_shell_view_get_window (
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

	if (priv->window != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (priv->window), &priv->window);
		priv->window = NULL;
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
	EShellViewClass *class;
	EShellView *shell_view;
	GtkWidget *sidebar;

	shell_view = E_SHELL_VIEW (object);
	class = E_SHELL_VIEW_GET_CLASS (object);
	sidebar = e_shell_view_get_sidebar_widget (shell_view);
	e_shell_sidebar_set_icon_name (
		E_SHELL_SIDEBAR (sidebar), class->icon_name);
	e_shell_sidebar_set_primary_text (
		E_SHELL_SIDEBAR (sidebar), class->label);

	shell_view_setup_search_context (shell_view);

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
		PROP_VIEW_INSTANCE,
		g_param_spec_object (
			"view-instance",
			_("GAL View Instance"),
			_("The GAL view instance for the shell view"),
			GAL_VIEW_INSTANCE_TYPE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WINDOW,
		g_param_spec_object (
			"window",
			_("Window"),
			_("The window to which the shell view belongs"),
			GTK_TYPE_WINDOW,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

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
	GtkWidget *widget;

	shell_view->priv = E_SHELL_VIEW_GET_PRIVATE (shell_view);

	widget = e_shell_content_new ();
	shell_view->priv->content = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	widget = e_shell_sidebar_new ();
	shell_view->priv->sidebar = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	widget = e_task_bar_new ();
	shell_view->priv->taskbar = g_object_ref_sink (widget);
	gtk_widget_show (widget);
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
e_shell_view_get_window (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return E_SHELL_WINDOW (shell_view->priv->window);
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
	shell_window = e_shell_view_get_window (shell_view);
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

GtkWidget *
e_shell_view_get_content_widget (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->content;
}

GtkWidget *
e_shell_view_get_sidebar_widget (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->sidebar;
}

GtkWidget *
e_shell_view_get_taskbar_widget (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->taskbar;
}

void
e_shell_view_changed (EShellView *shell_view)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	g_signal_emit (shell_view, signals[CHANGED], 0);
}
