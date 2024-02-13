/*
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
 * Authors:
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "gal-a11y-e-cell-toggle.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "e-cell-toggle.h"
#include "e-table-model.h"

#define PARENT_TYPE  (gal_a11y_e_cell_get_type ())
static GObjectClass *parent_class;

static void
gal_a11y_e_cell_toggle_dispose (GObject *object)
{
	GalA11yECellToggle *a11y = GAL_A11Y_E_CELL_TOGGLE (object);

	ETableModel *e_table_model = GAL_A11Y_E_CELL (a11y)->item->table_model;

	if (e_table_model && a11y->model_id > 0) {
		g_signal_handler_disconnect (e_table_model, a11y->model_id);
		a11y->model_id = 0;
	}

	if (parent_class->dispose)
		parent_class->dispose (object);
}

static void
ect_get_current_value (AtkValue *obj,
		       GValue *value)
{
	gint val;
	GalA11yECell *cell = GAL_A11Y_E_CELL (obj);

	val = GPOINTER_TO_INT (e_table_model_value_at (cell->cell_view->e_table_model, cell->model_col, cell->row));

	memset (value, 0, sizeof (GValue));
	g_value_init (value, G_TYPE_DOUBLE);
	g_value_set_double (value, val);
}

static void
ect_value_iface_init (AtkValueIface *iface)
{
	iface->get_current_value = ect_get_current_value;
}

G_DEFINE_TYPE_WITH_CODE (GalA11yECellToggle, gal_a11y_e_cell_toggle, GAL_A11Y_TYPE_E_CELL,
	G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, gal_a11y_e_cell_atk_action_interface_init)
	G_IMPLEMENT_INTERFACE (ATK_TYPE_VALUE, ect_value_iface_init))

static void
gal_a11y_e_cell_toggle_class_init (GalA11yECellToggleClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->dispose = gal_a11y_e_cell_toggle_dispose;
	parent_class = g_type_class_ref (PARENT_TYPE);
}

static void
gal_a11y_e_cell_toggle_init (GalA11yECellToggle *toggle)
{
}

static void
toggle_cell_action (GalA11yECell *cell)
{
	gint finished;
	GtkLayout *layout;
	GdkEventButton event;
	gint x, y, width, height;
	gint row, col;

	row = cell->row;
	col = cell->view_col;

	layout = GTK_LAYOUT (GNOME_CANVAS_ITEM (cell->item)->canvas);

	e_table_item_get_cell_geometry (
		cell->item, &row, &col, &x, &y, &width, &height);

	event.x = x + width / 2 + (gint)(GNOME_CANVAS_ITEM (cell->item)->x1);
	event.y = y + height / 2 + (gint)(GNOME_CANVAS_ITEM (cell->item)->y1);

	event.type = GDK_BUTTON_PRESS;
	event.window = gtk_layout_get_bin_window (layout);
	event.button = 1;
	event.send_event = TRUE;
	event.time = GDK_CURRENT_TIME;
	event.axes = NULL;

	g_signal_emit_by_name (cell->item, "event", &event, &finished);
}

static void
update_cell (GalA11yECell *cell,
	     gboolean notify)
{
	gint value;
	const gchar *name;

	value = GPOINTER_TO_INT (e_table_model_value_at (cell->cell_view->e_table_model, cell->model_col, cell->row));
	name = e_cell_toggle_get_icon_description (E_CELL_TOGGLE (cell->cell_view->ecell), value);

	if (name)
		atk_object_set_name (ATK_OBJECT (cell), name);

	if (value && !name)
		gal_a11y_e_cell_add_state (cell, ATK_STATE_CHECKED, notify);
	else
		gal_a11y_e_cell_remove_state (cell, ATK_STATE_CHECKED, notify);
}

static void
model_change_cb (ETableModel *etm,
                 gint col,
                 gint row,
                 GalA11yECell *cell)
{

	if (col == cell->model_col && row == cell->row)
		update_cell (cell, TRUE);
}

AtkObject *
gal_a11y_e_cell_toggle_new (ETableItem *item,
                            ECellView *cell_view,
                            AtkObject *parent,
                            gint model_col,
                            gint view_col,
                            gint row)
{
	AtkObject *a11y;
	GalA11yECell *cell;
	GalA11yECellToggle *toggle_cell;

	a11y = ATK_OBJECT (g_object_new (GAL_A11Y_TYPE_E_CELL_TOGGLE, NULL));

	g_return_val_if_fail (a11y != NULL, NULL);

	cell = GAL_A11Y_E_CELL (a11y);
	toggle_cell = GAL_A11Y_E_CELL_TOGGLE (a11y);
	a11y->role = ATK_ROLE_TABLE_CELL;

	gal_a11y_e_cell_construct (
		a11y,
		item,
		cell_view,
		parent,
		model_col,
		view_col,
		row);

	gal_a11y_e_cell_add_action (
		cell,
		"toggle",
		/* Translators: description of a "toggle" action */
		_("toggle the cell"),
		NULL,              /* action keybinding */
		toggle_cell_action);

	toggle_cell->model_id = g_signal_connect (
		item->table_model, "model_cell_changed",
		(GCallback) model_change_cb, a11y);

	update_cell (cell, FALSE);

	return a11y;
}
