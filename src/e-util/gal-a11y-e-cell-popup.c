/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Yang Wu <yang.wu@sun.com>
 */

#include "evolution-config.h"

#include "gal-a11y-e-cell-popup.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "e-cell-popup.h"
#include "gal-a11y-e-cell-registry.h"
#include "gal-a11y-util.h"

static void popup_cell_action (GalA11yECell *cell);

G_DEFINE_TYPE_WITH_CODE (GalA11yECellPopup, gal_a11y_e_cell_popup, GAL_A11Y_TYPE_E_CELL,
	G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, gal_a11y_e_cell_atk_action_interface_init))

static void
gal_a11y_e_cell_popup_class_init (GalA11yECellPopupClass *class)
{
}

static void
gal_a11y_e_cell_popup_init (GalA11yECellPopup *self)
{
}

AtkObject *
gal_a11y_e_cell_popup_new (ETableItem *item,
                           ECellView *cell_view,
                           AtkObject *parent,
                           gint model_col,
                           gint view_col,
                           gint row)
{
	AtkObject *a11y;
	GalA11yECell *cell;
	ECellPopup *popupcell;
	ECellView * child_view = NULL;

	popupcell=  E_CELL_POPUP (cell_view->ecell);

	if (popupcell && popupcell->popup_cell_view)
		child_view = popupcell->popup_cell_view->child_view;

	if (child_view && child_view->ecell) {
		a11y = gal_a11y_e_cell_registry_get_object (
			NULL,
			item,
			child_view,
			parent,
			model_col,
			view_col,
			row);
	} else {
		a11y = g_object_new (GAL_A11Y_TYPE_E_CELL_POPUP, NULL);
		gal_a11y_e_cell_construct (
			a11y,
			item,
			cell_view,
			parent,
			model_col,
			view_col,
			row);
		}
	g_return_val_if_fail (a11y != NULL, NULL);
	cell = GAL_A11Y_E_CELL (a11y);
	gal_a11y_e_cell_add_action (
		cell,
		"popup",
		/* Translators: description of a "popup" action */
		_("popup a child"),
		"<Alt>Down",              /* action keybinding */
		popup_cell_action);

	a11y->role = ATK_ROLE_TABLE_CELL;
	return a11y;
}

static void
popup_cell_action (GalA11yECell *cell)
{
	gint finished;
	GdkEvent event;
	GtkLayout *layout;

	layout = GTK_LAYOUT (GNOME_CANVAS_ITEM (cell->item)->canvas);

	event.key.type = GDK_KEY_PRESS;
	event.key.window = gtk_layout_get_bin_window (layout);
	event.key.send_event = TRUE;
	event.key.time = GDK_CURRENT_TIME;
	event.key.state = GDK_MOD1_MASK;
	event.key.keyval = GDK_KEY_Down;

	g_signal_emit_by_name (cell->item, "event", &event, &finished);
}
