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

#include "e-shell-window.h"
#include "e-shell-window-actions.h"

#define E_SHELL_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SHELL_VIEW, EShellViewPrivate))

struct _EShellViewPrivate {
	gchar *icon_name;
	gchar *primary_text;
	gchar *secondary_text;
	gchar *title;
	gint page_num;
	gpointer window;  /* weak pointer */
};

enum {
	PROP_0,
	PROP_ICON_NAME,
	PROP_PAGE_NUM,
	PROP_PRIMARY_TEXT,
	PROP_SECONDARY_TEXT,
	PROP_TITLE,
	PROP_WINDOW
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
		case PROP_ICON_NAME:
			e_shell_view_set_icon_name (
				E_SHELL_VIEW (object),
				g_value_get_string (value));
			return;

		case PROP_PAGE_NUM:
			shell_view_set_page_num (
				E_SHELL_VIEW (object),
				g_value_get_int (value));
			return;

		case PROP_PRIMARY_TEXT:
			e_shell_view_set_primary_text (
				E_SHELL_VIEW (object),
				g_value_get_string (value));
			return;

		case PROP_SECONDARY_TEXT:
			e_shell_view_set_secondary_text (
				E_SHELL_VIEW (object),
				g_value_get_string (value));
			return;

		case PROP_TITLE:
			e_shell_view_set_title (
				E_SHELL_VIEW (object),
				g_value_get_string (value));
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
		case PROP_ICON_NAME:
			g_value_set_string (
				value, e_shell_view_get_icon_name (
				E_SHELL_VIEW (object)));
			return;

		case PROP_PAGE_NUM:
			g_value_set_int (
				value, e_shell_view_get_page_num (
				E_SHELL_VIEW (object)));
			return;

		case PROP_PRIMARY_TEXT:
			g_value_set_string (
				value, e_shell_view_get_primary_text (
				E_SHELL_VIEW (object)));
			return;

		case PROP_SECONDARY_TEXT:
			g_value_set_string (
				value, e_shell_view_get_secondary_text (
				E_SHELL_VIEW (object)));
			return;

		case PROP_TITLE:
			g_value_set_string (
				value, e_shell_view_get_title (
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

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
shell_view_finalize (GObject *object)
{
	EShellViewPrivate *priv;

	priv = E_SHELL_VIEW_GET_PRIVATE (object);

	g_free (priv->icon_name);
	g_free (priv->primary_text);
	g_free (priv->secondary_text);
	g_free (priv->title);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
shell_view_constructed (GObject *object)
{
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
		PROP_ICON_NAME,
		g_param_spec_string (
			"icon-name",
			_("Icon Name"),
			_("The icon name for the sidebar header"),
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

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
		PROP_PRIMARY_TEXT,
		g_param_spec_string (
			"primary-text",
			_("Primary Text"),
			_("The primary text for the sidebar header"),
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_SECONDARY_TEXT,
		g_param_spec_string (
			"secondary-text",
			_("Secondary Text"),
			_("The secondary text for the sidebar header"),
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

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
e_shell_view_get_icon_name (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->icon_name;
}

void
e_shell_view_set_icon_name (EShellView *shell_view,
                            const gchar *icon_name)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	if (icon_name == NULL) {
		EShellViewClass *class;

		/* Fall back to the switcher icon. */
		class = E_SHELL_VIEW_GET_CLASS (shell_view);
		icon_name = class->icon_name;
	}

	g_free (shell_view->priv->icon_name);
	shell_view->priv->icon_name = g_strdup (icon_name);

	g_object_notify (G_OBJECT (shell_view), "icon-name");
}

const gchar *
e_shell_view_get_primary_text (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->primary_text;
}

void
e_shell_view_set_primary_text (EShellView *shell_view,
                               const gchar *primary_text)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	if (primary_text == NULL) {
		EShellViewClass *class;

		/* Fall back to the switcher label. */
		class = E_SHELL_VIEW_GET_CLASS (shell_view);
		primary_text = class->label;
	}

	g_free (shell_view->priv->primary_text);
	shell_view->priv->primary_text = g_strdup (primary_text);

	g_object_notify (G_OBJECT (shell_view), "primary-text");
}

const gchar *
e_shell_view_get_secondary_text (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->secondary_text;
}

void
e_shell_view_set_secondary_text (EShellView *shell_view,
                                 const gchar *secondary_text)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	g_free (shell_view->priv->secondary_text);
	shell_view->priv->secondary_text = g_strdup (secondary_text);
	g_debug ("%s: %s", G_STRFUNC, secondary_text);

	g_object_notify (G_OBJECT (shell_view), "secondary-text");
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

	g_free (shell_view->priv->title);
	shell_view->priv->title = g_strdup (title);

	g_object_notify (G_OBJECT (shell_view), "title");
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
	EShellViewClass *class;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	class = E_SHELL_VIEW_GET_CLASS (shell_view);
	g_return_val_if_fail (class->get_content_widget != NULL, NULL);

	return class->get_content_widget (shell_view);
}

GtkWidget *
e_shell_view_get_sidebar_widget (EShellView *shell_view)
{
	EShellViewClass *class;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	class = E_SHELL_VIEW_GET_CLASS (shell_view);
	g_return_val_if_fail (class->get_sidebar_widget != NULL, NULL);

	return class->get_sidebar_widget (shell_view);
}

GtkWidget *
e_shell_view_get_status_widget (EShellView *shell_view)
{
	EShellViewClass *class;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	class = E_SHELL_VIEW_GET_CLASS (shell_view);
	g_return_val_if_fail (class->get_status_widget != NULL, NULL);

	return class->get_status_widget (shell_view);
}

void
e_shell_view_changed (EShellView *shell_view)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	g_signal_emit (shell_view, signals[CHANGED], 0);
}
