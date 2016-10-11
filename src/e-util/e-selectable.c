/*
 * e-selectable.c
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

#include "e-selectable.h"

G_DEFINE_INTERFACE (
	ESelectable,
	e_selectable,
	GTK_TYPE_WIDGET)

static void
e_selectable_default_init (ESelectableInterface *iface)
{
	g_object_interface_install_property (
		iface,
		g_param_spec_boxed (
			"copy-target-list",
			"Copy Target List",
			NULL,
			GTK_TYPE_TARGET_LIST,
			G_PARAM_READABLE));

	g_object_interface_install_property (
		iface,
		g_param_spec_boxed (
			"paste-target-list",
			"Paste Target List",
			NULL,
			GTK_TYPE_TARGET_LIST,
			G_PARAM_READABLE));
}

void
e_selectable_update_actions (ESelectable *selectable,
                             EFocusTracker *focus_tracker,
                             GdkAtom *clipboard_targets,
                             gint n_clipboard_targets)
{
	ESelectableInterface *iface;

	g_return_if_fail (E_IS_SELECTABLE (selectable));

	iface = E_SELECTABLE_GET_INTERFACE (selectable);
	g_return_if_fail (iface->update_actions != NULL);

	iface->update_actions (
		selectable, focus_tracker,
		clipboard_targets, n_clipboard_targets);
}

void
e_selectable_cut_clipboard (ESelectable *selectable)
{
	ESelectableInterface *iface;

	g_return_if_fail (E_IS_SELECTABLE (selectable));

	iface = E_SELECTABLE_GET_INTERFACE (selectable);

	if (iface->cut_clipboard != NULL)
		iface->cut_clipboard (selectable);
}

void
e_selectable_copy_clipboard (ESelectable *selectable)
{
	ESelectableInterface *iface;

	g_return_if_fail (E_IS_SELECTABLE (selectable));

	iface = E_SELECTABLE_GET_INTERFACE (selectable);

	if (iface->copy_clipboard != NULL)
		iface->copy_clipboard (selectable);
}

void
e_selectable_paste_clipboard (ESelectable *selectable)
{
	ESelectableInterface *iface;

	g_return_if_fail (E_IS_SELECTABLE (selectable));

	iface = E_SELECTABLE_GET_INTERFACE (selectable);

	if (iface->paste_clipboard != NULL)
		iface->paste_clipboard (selectable);
}

void
e_selectable_delete_selection (ESelectable *selectable)
{
	ESelectableInterface *iface;

	g_return_if_fail (E_IS_SELECTABLE (selectable));

	iface = E_SELECTABLE_GET_INTERFACE (selectable);

	if (iface->delete_selection != NULL)
		iface->delete_selection (selectable);
}

void
e_selectable_select_all (ESelectable *selectable)
{
	ESelectableInterface *iface;

	g_return_if_fail (E_IS_SELECTABLE (selectable));

	iface = E_SELECTABLE_GET_INTERFACE (selectable);

	if (iface->select_all != NULL)
		iface->select_all (selectable);
}

void
e_selectable_undo (ESelectable *selectable)
{
	ESelectableInterface *iface;

	g_return_if_fail (E_IS_SELECTABLE (selectable));

	iface = E_SELECTABLE_GET_INTERFACE (selectable);

	if (iface->undo != NULL)
		iface->undo (selectable);
}

void
e_selectable_redo (ESelectable *selectable)
{
	ESelectableInterface *iface;

	g_return_if_fail (E_IS_SELECTABLE (selectable));

	iface = E_SELECTABLE_GET_INTERFACE (selectable);

	if (iface->redo != NULL)
		iface->redo (selectable);
}

GtkTargetList *
e_selectable_get_copy_target_list (ESelectable *selectable)
{
	GtkTargetList *target_list;

	g_return_val_if_fail (E_IS_SELECTABLE (selectable), NULL);

	g_object_get (selectable, "copy-target-list", &target_list, NULL);

	/* We want to return a borrowed reference to the target
	 * list, so undo the reference that g_object_get() added. */
	gtk_target_list_unref (target_list);

	return target_list;
}

GtkTargetList *
e_selectable_get_paste_target_list (ESelectable *selectable)
{
	GtkTargetList *target_list;

	g_return_val_if_fail (E_IS_SELECTABLE (selectable), NULL);

	g_object_get (selectable, "paste-target-list", &target_list, NULL);

	/* We want to return a borrowed reference to the target
	 * list, so undo the reference that g_object_get() added. */
	gtk_target_list_unref (target_list);

	return target_list;
}
