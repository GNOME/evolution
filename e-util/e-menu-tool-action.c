/*
 * e-menu-tool-action.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-menu-tool-action.h"

G_DEFINE_TYPE (
	EMenuToolAction,
	e_menu_tool_action,
	GTK_TYPE_ACTION)

static void
e_menu_tool_action_class_init (EMenuToolActionClass *class)
{
	GtkActionClass *action_class;

	action_class = GTK_ACTION_CLASS (class);
	action_class->toolbar_item_type = GTK_TYPE_MENU_TOOL_BUTTON;
}

static void
e_menu_tool_action_init (EMenuToolAction *action)
{
}

EMenuToolAction *
e_menu_tool_action_new (const gchar *name,
                        const gchar *label,
                        const gchar *tooltip)
{
	g_return_val_if_fail (name != NULL, NULL);

	return g_object_new (
		E_TYPE_MENU_TOOL_ACTION,
		"name", name,
		"label", label,
		"tooltip", tooltip,
		NULL);
}
