/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-cal-shell-view.c
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

#include "e-cal-shell-view-private.h"

GType e_cal_shell_view_type = 0;
static gpointer parent_class;

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
cal_shell_view_changed (EShellView *shell_view)
{
	ECalShellViewPrivate *priv;
	GtkActionGroup *action_group;
	gboolean visible;

	priv = E_CAL_SHELL_VIEW_GET_PRIVATE (shell_view);

	action_group = priv->cal_actions;
	visible = e_shell_view_is_selected (shell_view);
	gtk_action_group_visible (action_group, visible);
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
	object_class->dispose = cal_shell_view_dispose;
	object_class->finalize = cal_shell_view_finalize;
	object_class->constructed = cal_shell_view_constructed;

	shell_view_class = E_SHELL_VIEW_CLASS (class);
	shell_view_class->label = N_("Cals");
	shell_view_class->icon_name = "evolution-cals";
	shell_view_class->type_module = type_module;
	shell_view_class->changed = cal_shell_view_changed;
}

static void
cal_shell_view_init (ECalShellView *cal_shell_view)
{
	cal_shell_view->priv =
		E_CAL_SHELL_VIEW_GET_PRIVATE (cal_shell_view);

	e_cal_shell_view_private_init (cal_shell_view);
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
