/*
 * e-menu-tool-action.c
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

#include "e-menu-tool-action.h"

static gpointer parent_class;

static void
menu_tool_action_class_init (EMenuToolActionClass *class)
{
	GtkActionClass *action_class;

	parent_class = g_type_class_peek_parent (class);

	action_class = GTK_ACTION_CLASS (class);
	action_class->toolbar_item_type = GTK_TYPE_MENU_TOOL_BUTTON;
}

GType
e_menu_tool_action_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMenuToolActionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) menu_tool_action_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMenuToolAction),
			0,     /* n_preallocs */
			(GInstanceInitFunc) NULL,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_ACTION, "EMenuToolAction", &type_info, 0);
	}

	return type;
}

EMenuToolAction *
e_menu_tool_action_new (const gchar *name,
                        const gchar *label,
                        const gchar *tooltip,
                        const gchar *stock_id)
{
	g_return_val_if_fail (name != NULL, NULL);

	return g_object_new (
		E_TYPE_MENU_TOOL_ACTION,
		"name", name, "label", label, "tooltip",
		tooltip, "stock-id", stock_id, NULL);
}
