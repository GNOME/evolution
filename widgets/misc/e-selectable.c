/*
 * e-selectable.c
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

#include "e-selectable.h"

GType
e_selectable_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (ESelectableInterface),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) NULL,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			0,     /* instance_size */
			0,     /* n_preallocs */
			(GInstanceInitFunc) NULL,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			G_TYPE_INTERFACE, "ESelectable", &type_info, 0);

		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}

	return type;
}

void
e_selectable_update_actions (ESelectable *selectable,
                             EFocusTracker *focus_tracker,
                             GdkAtom *clipboard_targets,
                             gint n_clipboard_targets)
{
	ESelectableInterface *interface;

	g_return_if_fail (E_IS_SELECTABLE (selectable));

	interface = E_SELECTABLE_GET_INTERFACE (selectable);
	g_return_if_fail (interface->update_actions != NULL);

	return interface->update_actions (
		selectable, focus_tracker,
		clipboard_targets, n_clipboard_targets);
}

void
e_selectable_cut_clipboard (ESelectable *selectable)
{
	ESelectableInterface *interface;

	g_return_if_fail (E_IS_SELECTABLE (selectable));

	interface = E_SELECTABLE_GET_INTERFACE (selectable);

	if (interface->cut_clipboard != NULL)
		interface->cut_clipboard (selectable);
}

void
e_selectable_copy_clipboard (ESelectable *selectable)
{
	ESelectableInterface *interface;

	g_return_if_fail (E_IS_SELECTABLE (selectable));

	interface = E_SELECTABLE_GET_INTERFACE (selectable);

	if (interface->copy_clipboard != NULL)
		interface->copy_clipboard (selectable);
}

void
e_selectable_paste_clipboard (ESelectable *selectable)
{
	ESelectableInterface *interface;

	g_return_if_fail (E_IS_SELECTABLE (selectable));

	interface = E_SELECTABLE_GET_INTERFACE (selectable);

	if (interface->paste_clipboard != NULL)
		interface->paste_clipboard (selectable);
}

void
e_selectable_delete_selection (ESelectable *selectable)
{
	ESelectableInterface *interface;

	g_return_if_fail (E_IS_SELECTABLE (selectable));

	interface = E_SELECTABLE_GET_INTERFACE (selectable);

	if (interface->delete_selection != NULL)
		interface->delete_selection (selectable);
}

void
e_selectable_select_all (ESelectable *selectable)
{
	ESelectableInterface *interface;

	g_return_if_fail (E_IS_SELECTABLE (selectable));

	interface = E_SELECTABLE_GET_INTERFACE (selectable);

	if (interface->select_all != NULL)
		interface->select_all (selectable);
}
