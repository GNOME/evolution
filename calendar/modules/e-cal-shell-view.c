/*
 * e-cal-shell-view.c
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

#include "e-cal-shell-view-private.h"

enum {
	PROP_0,
	PROP_SOURCE_LIST
};

GType e_cal_shell_view_type = 0;
static gpointer parent_class;

static void
cal_shell_view_get_property (GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SOURCE_LIST:
			g_value_set_object (
				value, e_cal_shell_view_get_source_list (
				E_CAL_SHELL_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cal_shell_view_dispose (GObject *object)
{
	e_cal_shell_view_private_dispose (E_CAL_SHELL_VIEW (object));

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
cal_shell_view_finalize (GObject *object)
{
	e_cal_shell_view_private_finalize (E_CAL_SHELL_VIEW (object));

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
cal_shell_view_constructed (GObject *object)
{
	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	e_cal_shell_view_private_constructed (E_CAL_SHELL_VIEW (object));
}

static void
cal_shell_view_update_actions (EShellView *shell_view)
{
	ECalShellViewPrivate *priv;
	EShellWindow *shell_window;

	priv = E_CAL_SHELL_VIEW_GET_PRIVATE (shell_view);

	shell_window = e_shell_view_get_shell_window (shell_view);

	/* FIXME */
}

static void
cal_shell_view_class_init (ECalShellView *class,
                            GTypeModule *type_module)
{
	GObjectClass *object_class;
	EShellViewClass *shell_view_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (ECalShellViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = cal_shell_view_get_property;
	object_class->dispose = cal_shell_view_dispose;
	object_class->finalize = cal_shell_view_finalize;
	object_class->constructed = cal_shell_view_constructed;

	shell_view_class = E_SHELL_VIEW_CLASS (class);
	shell_view_class->label = N_("Cals");
	shell_view_class->icon_name = "evolution-cals";
	shell_view_class->ui_definition = "evolution-calendars.ui";
	shell_view_class->search_options = "/calendar-search-options";
	shell_view_class->type_module = type_module;
	shell_view_class->new_shell_sidebar = e_cal_shell_sidebar_new;
	shell_view_class->update_actions = cal_shell_view_update_actions;

	g_object_class_install_property (
		object_class,
		PROP_SOURCE_LIST,
		g_param_spec_object (
			"source-list",
			_("Source List"),
			_("The registry of calendars"),
			E_TYPE_SOURCE_LIST,
			G_PARAM_READABLE));
}

static void
cal_shell_view_init (ECalShellView *cal_shell_view,
                     EShellViewClass *shell_view_class)
{
	cal_shell_view->priv =
		E_CAL_SHELL_VIEW_GET_PRIVATE (cal_shell_view);

	e_cal_shell_view_private_init (cal_shell_view, shell_view_class);
}

GType
e_cal_shell_view_get_type (GTypeModule *type_module)
{
	if (e_cal_shell_view_type == 0) {
		const GTypeInfo type_info = {
			sizeof (ECalShellViewClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) cal_shell_view_class_init,
			(GClassFinalizeFunc) NULL,
			type_module,
			sizeof (ECalShellView),
			0,    /* n_preallocs */
			(GInstanceInitFunc) cal_shell_view_init,
			NULL  /* value_table */
		};

		e_cal_shell_view_type =
			g_type_module_register_type (
				type_module, E_TYPE_SHELL_VIEW,
				"ECalShellView", &type_info, 0);
	}

	return e_cal_shell_view_type;
}

GnomeCalendar *
e_cal_shell_view_get_calendar (ECalShellView *cal_shell_view)
{
	g_return_val_if_fail (E_IS_CAL_SHELL_VIEW (cal_shell_view), NULL);

	/* FIXME */
	return NULL;
}

ESourceList *
e_cal_shell_view_get_source_list (ECalShellView *cal_shell_view)
{
	g_return_val_if_fail (E_IS_CAL_SHELL_VIEW (cal_shell_view), NULL);

	return cal_shell_view->priv->source_list;
}
