/*
 * e-cal-shell-sidebar.c
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

#include "e-cal-shell-sidebar.h"

#include <string.h>
#include <glib/gi18n.h>

#include "e-util/e-util.h"
#include "calendar/gui/gnome-cal.h"
#include "calendar/gui/e-calendar-selector.h"

#include "e-cal-shell-view.h"

#define E_CAL_SHELL_SIDEBAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CAL_SHELL_SIDEBAR, ECalShellSidebarPrivate))

struct _ECalShellSidebarPrivate {
	GtkWidget *selector;
};

enum {
	PROP_0,
	PROP_SELECTOR
};

static gpointer parent_class;

static void
cal_shell_sidebar_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SELECTOR:
			g_value_set_object (
				value, e_cal_shell_sidebar_get_selector (
				E_CAL_SHELL_SIDEBAR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cal_shell_sidebar_dispose (GObject *object)
{
	ECalShellSidebarPrivate *priv;

	priv = E_CAL_SHELL_SIDEBAR_GET_PRIVATE (object);

	if (priv->selector != NULL) {
		g_object_unref (priv->selector);
		priv->selector = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
cal_shell_sidebar_constructed (GObject *object)
{
	ECalShellSidebarPrivate *priv;
	EShellView *shell_view;
	EShellSidebar *shell_sidebar;
	ECalShellView *cal_shell_view;
	ESourceList *source_list;
	GtkContainer *container;
	GtkWidget *widget;

	priv = E_CAL_SHELL_SIDEBAR_GET_PRIVATE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	shell_sidebar = E_SHELL_SIDEBAR (object);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	cal_shell_view = E_CAL_SHELL_VIEW (shell_view);
	source_list = e_cal_shell_view_get_source_list (cal_shell_view);

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
}

static void
cal_shell_sidebar_class_init (ECalShellSidebarClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (ECalShellSidebarPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = cal_shell_sidebar_get_property;
	object_class->dispose = cal_shell_sidebar_dispose;
	object_class->constructed = cal_shell_sidebar_constructed;

	g_object_class_install_property (
		object_class,
		PROP_SELECTOR,
		g_param_spec_object (
			"selector",
			_("Source Selector Widget"),
			_("This widget displays groups of calendars"),
			E_TYPE_SOURCE_SELECTOR,
			G_PARAM_READABLE));
}

static void
cal_shell_sidebar_init (ECalShellSidebar *cal_shell_sidebar)
{
	cal_shell_sidebar->priv =
		E_CAL_SHELL_SIDEBAR_GET_PRIVATE (cal_shell_sidebar);

	/* Postpone widget construction until we have a shell view. */
}

GType
e_cal_shell_sidebar_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (ECalShellSidebarClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) cal_shell_sidebar_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (ECalShellSidebar),
			0,     /* n_preallocs */
			(GInstanceInitFunc) cal_shell_sidebar_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_SHELL_SIDEBAR, "ECalShellSidebar",
			&type_info, 0);
	}

	return type;
}

GtkWidget *
e_cal_shell_sidebar_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_CAL_SHELL_SIDEBAR,
		"shell-view", shell_view, NULL);
}

GtkWidget *
e_cal_shell_sidebar_get_selector (ECalShellSidebar *cal_shell_sidebar)
{
	g_return_val_if_fail (
		E_IS_CAL_SHELL_SIDEBAR (cal_shell_sidebar), NULL);

	return cal_shell_sidebar->priv->selector;
}
