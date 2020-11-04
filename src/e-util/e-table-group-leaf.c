/*
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
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-table-group-leaf.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <libgnomecanvas/libgnomecanvas.h>

#include "e-canvas.h"
#include "e-table-item.h"
#include "e-table-sorted.h"
#include "e-table-sorted-variable.h"

G_DEFINE_TYPE (
	ETableGroupLeaf,
	e_table_group_leaf,
	E_TYPE_TABLE_GROUP)

enum {
	PROP_0,
	PROP_HEIGHT,
	PROP_WIDTH,
	PROP_MINIMUM_WIDTH,
	PROP_FROZEN,
	PROP_TABLE_ALTERNATING_ROW_COLORS,
	PROP_TABLE_HORIZONTAL_DRAW_GRID,
	PROP_TABLE_VERTICAL_DRAW_GRID,
	PROP_TABLE_DRAW_FOCUS,
	PROP_CURSOR_MODE,
	PROP_LENGTH_THRESHOLD,
	PROP_SELECTION_MODEL,
	PROP_UNIFORM_ROW_HEIGHT,
	PROP_IS_EDITING
};

static void
etgl_item_is_editing_changed_cb (ETableItem *item,
                                 GParamSpec *param,
                                 ETableGroupLeaf *etgl)
{
	g_return_if_fail (E_IS_TABLE_GROUP_LEAF (etgl));

	g_object_notify (G_OBJECT (etgl), "is-editing");
}

static void
etgl_dispose (GObject *object)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (object);

	g_clear_object (&etgl->ets);

	if (etgl->item) {
		if (etgl->etgl_cursor_change_id != 0)
			g_signal_handler_disconnect (
				etgl->item,
				etgl->etgl_cursor_change_id);
		if (etgl->etgl_cursor_activated_id != 0)
			g_signal_handler_disconnect (
				etgl->item,
				etgl->etgl_cursor_activated_id);
		if (etgl->etgl_double_click_id != 0)
			g_signal_handler_disconnect (
				etgl->item,
				etgl->etgl_double_click_id);
		if (etgl->etgl_right_click_id != 0)
			g_signal_handler_disconnect (
				etgl->item,
				etgl->etgl_right_click_id);
		if (etgl->etgl_click_id != 0)
			g_signal_handler_disconnect (
				etgl->item,
				etgl->etgl_click_id);
		if (etgl->etgl_key_press_id != 0)
			g_signal_handler_disconnect (
				etgl->item,
				etgl->etgl_key_press_id);
		if (etgl->etgl_start_drag_id != 0)
			g_signal_handler_disconnect (
				etgl->item,
				etgl->etgl_start_drag_id);

		e_signal_disconnect_notify_handler (etgl->item, &etgl->notify_is_editing_id);

		etgl->etgl_cursor_change_id = 0;
		etgl->etgl_cursor_activated_id = 0;
		etgl->etgl_double_click_id = 0;
		etgl->etgl_right_click_id = 0;
		etgl->etgl_click_id = 0;
		etgl->etgl_key_press_id = 0;
		etgl->etgl_start_drag_id = 0;

		g_object_run_dispose (G_OBJECT (etgl->item));
		etgl->item = NULL;
	}

	g_clear_object (&etgl->selection_model);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_table_group_leaf_parent_class)->dispose (object);
}

static void
e_table_group_leaf_construct (GnomeCanvasGroup *parent,
                              ETableGroupLeaf *etgl,
                              ETableHeader *full_header,
                              ETableHeader *header,
                              ETableModel *model,
                              ETableSortInfo *sort_info)
{
	etgl->is_grouped =
		(e_table_sort_info_grouping_get_count (sort_info) > 0);

	if (etgl->is_grouped)
		etgl->ets = e_table_sorted_variable_new (
			model, full_header, sort_info);
	else
		etgl->ets = e_table_sorted_new (
			model, full_header, sort_info);

	e_table_group_construct (
		parent, E_TABLE_GROUP (etgl), full_header, header, model);
}

/**
 * e_table_group_leaf_new
 * @parent: The %GnomeCanvasGroup to create a child of.
 * @full_header: The full header of the %ETable.
 * @header: The current header of the %ETable.
 * @model: The %ETableModel of the %ETable.
 * @sort_info: The %ETableSortInfo of the %ETable.
 *
 * %ETableGroupLeaf is an %ETableGroup which simply contains an
 * %ETableItem.
 *
 * Returns: The new %ETableGroupLeaf.
 */
ETableGroup *
e_table_group_leaf_new (GnomeCanvasGroup *parent,
                        ETableHeader *full_header,
                        ETableHeader *header,
                        ETableModel *model,
                        ETableSortInfo *sort_info)
{
	ETableGroupLeaf *etgl;

	g_return_val_if_fail (parent != NULL, NULL);

	etgl = g_object_new (E_TYPE_TABLE_GROUP_LEAF, NULL);

	e_table_group_leaf_construct (
		parent, etgl, full_header,
		header, model, sort_info);

	return E_TABLE_GROUP (etgl);
}

static void
etgl_cursor_change (GObject *object,
                    gint view_row,
                    ETableGroupLeaf *etgl)
{
	ETableSubset *table_subset;
	gint model_row;

	table_subset = E_TABLE_SUBSET (etgl->ets);
	model_row = e_table_subset_view_to_model_row (table_subset, view_row);

	if (model_row < 0)
		return;

	e_table_group_cursor_change (E_TABLE_GROUP (etgl), model_row);
}

static void
etgl_cursor_activated (GObject *object,
                       gint view_row,
                       ETableGroupLeaf *etgl)
{
	ETableSubset *table_subset;
	gint model_row;

	table_subset = E_TABLE_SUBSET (etgl->ets);
	model_row = e_table_subset_view_to_model_row (table_subset, view_row);

	if (model_row < 0)
		return;

	e_table_group_cursor_activated (E_TABLE_GROUP (etgl), model_row);
}

static void
etgl_double_click (GObject *object,
                   gint model_row,
                   gint model_col,
                   GdkEvent *event,
                   ETableGroupLeaf *etgl)
{
	e_table_group_double_click (
		E_TABLE_GROUP (etgl), model_row, model_col, event);
}

static gboolean
etgl_key_press (GObject *object,
                gint row,
                gint col,
                GdkEvent *event,
                ETableGroupLeaf *etgl)
{
	ETableSubset *table_subset;
	gint model_row;

	table_subset = E_TABLE_SUBSET (etgl->ets);
	model_row = e_table_subset_view_to_model_row (table_subset, row);

	if (model_row < 0)
		return FALSE;

	return e_table_group_key_press (
		E_TABLE_GROUP (etgl), model_row, col, event);
}

static gboolean
etgl_start_drag (GObject *object,
                 gint model_row,
                 gint model_col,
                 GdkEvent *event,
                 ETableGroupLeaf *etgl)
{
	return e_table_group_start_drag (
		E_TABLE_GROUP (etgl), model_row, model_col, event);
}

static gboolean
etgl_right_click (GObject *object,
                  gint view_row,
                  gint model_col,
                  GdkEvent *event,
                  ETableGroupLeaf *etgl)
{
	ETableSubset *table_subset;
	gint model_row;

	table_subset = E_TABLE_SUBSET (etgl->ets);
	model_row = e_table_subset_view_to_model_row (table_subset, view_row);

	if (model_row < 0)
		return FALSE;

	return e_table_group_right_click (
		E_TABLE_GROUP (etgl), model_row, model_col, event);
}

static gboolean
etgl_click (GObject *object,
            gint row,
            gint col,
            GdkEvent *event,
            ETableGroupLeaf *etgl)
{
	ETableSubset *table_subset;
	gint model_row;

	table_subset = E_TABLE_SUBSET (etgl->ets);
	model_row = e_table_subset_view_to_model_row (table_subset, row);

	if (model_row < 0)
		return FALSE;

	return e_table_group_click (
		E_TABLE_GROUP (etgl), model_row, col, event);
}

static void
etgl_reflow (GnomeCanvasItem *item,
             gint flags)
{
	ETableGroupLeaf *leaf = E_TABLE_GROUP_LEAF (item);

	g_object_get (leaf->item, "height", &leaf->height, NULL);
	g_object_get (leaf->item, "width", &leaf->width, NULL);

	e_canvas_item_request_parent_reflow (item);
}

static void
etgl_realize (GnomeCanvasItem *item)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (item);

	if (GNOME_CANVAS_ITEM_CLASS (e_table_group_leaf_parent_class)->realize)
		GNOME_CANVAS_ITEM_CLASS (e_table_group_leaf_parent_class)->realize (item);

	etgl->item = E_TABLE_ITEM (gnome_canvas_item_new (
		GNOME_CANVAS_GROUP (etgl),
		e_table_item_get_type (),
		"ETableHeader", E_TABLE_GROUP (etgl)->header,
		"ETableModel", etgl->ets,
		"alternating_row_colors", etgl->alternating_row_colors,
		"horizontal_draw_grid", etgl->horizontal_draw_grid,
		"vertical_draw_grid", etgl->vertical_draw_grid,
		"drawfocus", etgl->draw_focus,
		"cursor_mode", etgl->cursor_mode,
		"minimum_width", etgl->minimum_width,
		"length_threshold", etgl->length_threshold,
		"selection_model", etgl->selection_model,
		"uniform_row_height", etgl->uniform_row_height,
		NULL));

	etgl->etgl_cursor_change_id = g_signal_connect (
		etgl->item, "cursor_change",
		G_CALLBACK (etgl_cursor_change), etgl);

	etgl->etgl_cursor_activated_id = g_signal_connect (
		etgl->item, "cursor_activated",
		G_CALLBACK (etgl_cursor_activated), etgl);

	etgl->etgl_double_click_id = g_signal_connect (
		etgl->item, "double_click",
		G_CALLBACK (etgl_double_click), etgl);

	etgl->etgl_right_click_id = g_signal_connect (
		etgl->item, "right_click",
		G_CALLBACK (etgl_right_click), etgl);

	etgl->etgl_click_id = g_signal_connect (
		etgl->item, "click",
		G_CALLBACK (etgl_click), etgl);

	etgl->etgl_key_press_id = g_signal_connect (
		etgl->item, "key_press",
		G_CALLBACK (etgl_key_press), etgl);

	etgl->etgl_start_drag_id = g_signal_connect (
		etgl->item, "start_drag",
		G_CALLBACK (etgl_start_drag), etgl);

	etgl->notify_is_editing_id = e_signal_connect_notify (
		etgl->item, "notify::is-editing",
		G_CALLBACK (etgl_item_is_editing_changed_cb), etgl);

	e_canvas_item_request_reflow (item);
}

static void
etgl_add (ETableGroup *etg,
          gint row)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);

	if (E_IS_TABLE_SUBSET_VARIABLE (etgl->ets)) {
		e_table_subset_variable_add (
			E_TABLE_SUBSET_VARIABLE (etgl->ets), row);
	}
}

static void
etgl_add_array (ETableGroup *etg,
                const gint *array,
                gint count)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);

	if (E_IS_TABLE_SUBSET_VARIABLE (etgl->ets)) {
		e_table_subset_variable_add_array (
			E_TABLE_SUBSET_VARIABLE (etgl->ets), array, count);
	}
}

static void
etgl_add_all (ETableGroup *etg)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);

	if (E_IS_TABLE_SUBSET_VARIABLE (etgl->ets)) {
		e_table_subset_variable_add_all (
			E_TABLE_SUBSET_VARIABLE (etgl->ets));
	}
}

static gboolean
etgl_remove (ETableGroup *etg,
             gint row)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);

	if (E_IS_TABLE_SUBSET_VARIABLE (etgl->ets)) {
		return e_table_subset_variable_remove (
			E_TABLE_SUBSET_VARIABLE (etgl->ets), row);
	}
	return FALSE;
}

static void
etgl_increment (ETableGroup *etg,
                gint position,
                gint amount)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);

	if (E_IS_TABLE_SUBSET_VARIABLE (etgl->ets)) {
		e_table_subset_variable_increment (
			E_TABLE_SUBSET_VARIABLE (etgl->ets),
			position, amount);
	}
}

static void
etgl_decrement (ETableGroup *etg,
                gint position,
                gint amount)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);

	if (E_IS_TABLE_SUBSET_VARIABLE (etgl->ets)) {
		e_table_subset_variable_decrement (
			E_TABLE_SUBSET_VARIABLE (etgl->ets),
			position, amount);
	}
}

static gint
etgl_row_count (ETableGroup *etg)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);

	return e_table_model_row_count (E_TABLE_MODEL (etgl->ets));
}

static void
etgl_set_focus (ETableGroup *etg,
                EFocus direction,
                gint view_col)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);

	if (direction == E_FOCUS_END) {
		e_table_item_set_cursor (
			etgl->item, view_col,
			e_table_model_row_count (E_TABLE_MODEL (etgl->ets)) - 1);
	} else {
		e_table_item_set_cursor (etgl->item, view_col, 0);
	}
}

static gint
etgl_get_focus_column (ETableGroup *etg)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);

	return e_table_item_get_focused_column (etgl->item);
}

static EPrintable *
etgl_get_printable (ETableGroup *etg)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);

	return e_table_item_get_printable (etgl->item);
}

static void
etgl_compute_location (ETableGroup *etg,
                       gint *x,
                       gint *y,
                       gint *row,
                       gint *col)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);

	e_table_item_compute_location (etgl->item, x, y, row, col);
}

static void
etgl_get_mouse_over (ETableGroup *etg,
                     gint *row,
                     gint *col)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);

	if (etgl->item && etgl->item->motion_row > -1 && etgl->item->motion_col > -1) {
		if (row)
			*row = etgl->item->motion_row;
		if (col)
			*col = etgl->item->motion_col;
	}
}

static void
etgl_get_cell_geometry (ETableGroup *etg,
                        gint *row,
                        gint *col,
                        gint *x,
                        gint *y,
                        gint *width,
                        gint *height)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);

	e_table_item_get_cell_geometry (etgl->item, row, col, x, y, width, height);
}

static void
etgl_set_property (GObject *object,
                   guint property_id,
                   const GValue *value,
                   GParamSpec *pspec)
{
	ETableGroup *etg = E_TABLE_GROUP (object);
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (object);

	switch (property_id) {
	case PROP_FROZEN:
		etg->frozen = g_value_get_boolean (value);
		break;
	case PROP_MINIMUM_WIDTH:
	case PROP_WIDTH:
		etgl->minimum_width = g_value_get_double (value);
		if (etgl->item) {
			gnome_canvas_item_set (
				GNOME_CANVAS_ITEM (etgl->item),
				"minimum_width", etgl->minimum_width,
				NULL);
		}
		break;
	case PROP_LENGTH_THRESHOLD:
		etgl->length_threshold = g_value_get_int (value);
		if (etgl->item) {
			gnome_canvas_item_set (
				GNOME_CANVAS_ITEM (etgl->item),
				"length_threshold", etgl->length_threshold,
				NULL);
		}
		break;
	case PROP_SELECTION_MODEL:
		if (etgl->selection_model)
			g_object_unref (etgl->selection_model);
		etgl->selection_model = E_SELECTION_MODEL (g_value_get_object (value));
		if (etgl->selection_model) {
			g_object_ref (etgl->selection_model);
		}
		if (etgl->item) {
			gnome_canvas_item_set (
				GNOME_CANVAS_ITEM (etgl->item),
				"selection_model", etgl->selection_model,
				NULL);
		}
		break;

	case PROP_UNIFORM_ROW_HEIGHT:
		etgl->uniform_row_height = g_value_get_boolean (value);
		if (etgl->item) {
			gnome_canvas_item_set (
				GNOME_CANVAS_ITEM (etgl->item),
				"uniform_row_height", etgl->uniform_row_height,
				NULL);
		}
		break;

	case PROP_TABLE_ALTERNATING_ROW_COLORS:
		etgl->alternating_row_colors = g_value_get_boolean (value);
		if (etgl->item) {
			gnome_canvas_item_set (
				GNOME_CANVAS_ITEM (etgl->item),
				"alternating_row_colors", etgl->alternating_row_colors,
				NULL);
		}
		break;

	case PROP_TABLE_HORIZONTAL_DRAW_GRID:
		etgl->horizontal_draw_grid = g_value_get_boolean (value);
		if (etgl->item) {
			gnome_canvas_item_set (
				GNOME_CANVAS_ITEM (etgl->item),
				"horizontal_draw_grid", etgl->horizontal_draw_grid,
				NULL);
		}
		break;

	case PROP_TABLE_VERTICAL_DRAW_GRID:
		etgl->vertical_draw_grid = g_value_get_boolean (value);
		if (etgl->item) {
			gnome_canvas_item_set (
				GNOME_CANVAS_ITEM (etgl->item),
				"vertical_draw_grid", etgl->vertical_draw_grid,
				NULL);
		}
		break;

	case PROP_TABLE_DRAW_FOCUS:
		etgl->draw_focus = g_value_get_boolean (value);
		if (etgl->item) {
			gnome_canvas_item_set (
				GNOME_CANVAS_ITEM (etgl->item),
				"drawfocus", etgl->draw_focus,
				NULL);
		}
		break;

	case PROP_CURSOR_MODE:
		etgl->cursor_mode = g_value_get_int (value);
		if (etgl->item) {
			gnome_canvas_item_set (
				GNOME_CANVAS_ITEM (etgl->item),
				"cursor_mode", etgl->cursor_mode,
				NULL);
		}
		break;
	default:
		break;
	}
}

static void
etgl_get_property (GObject *object,
                   guint property_id,
                   GValue *value,
                   GParamSpec *pspec)
{
	ETableGroup *etg = E_TABLE_GROUP (object);
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (object);

	switch (property_id) {
	case PROP_FROZEN:
		g_value_set_boolean (value, etg->frozen);
		break;
	case PROP_HEIGHT:
		g_value_set_double (value, etgl->height);
		break;
	case PROP_WIDTH:
		g_value_set_double (value, etgl->width);
		break;
	case PROP_MINIMUM_WIDTH:
		g_value_set_double (value, etgl->minimum_width);
		break;
	case PROP_UNIFORM_ROW_HEIGHT:
		g_value_set_boolean (value, etgl->uniform_row_height);
		break;
	case PROP_IS_EDITING:
		g_value_set_boolean (value, e_table_group_leaf_is_editing (etgl));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_table_group_leaf_class_init (ETableGroupLeafClass *class)
{
	GnomeCanvasItemClass *item_class = GNOME_CANVAS_ITEM_CLASS (class);
	ETableGroupClass *e_group_class = E_TABLE_GROUP_CLASS (class);
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->dispose = etgl_dispose;
	object_class->set_property = etgl_set_property;
	object_class->get_property = etgl_get_property;

	item_class->realize = etgl_realize;

	e_group_class->add = etgl_add;
	e_group_class->add_array = etgl_add_array;
	e_group_class->add_all = etgl_add_all;
	e_group_class->remove = etgl_remove;
	e_group_class->increment = etgl_increment;
	e_group_class->decrement = etgl_decrement;
	e_group_class->row_count = etgl_row_count;
	e_group_class->set_focus = etgl_set_focus;
	e_group_class->get_focus_column = etgl_get_focus_column;
	e_group_class->get_printable = etgl_get_printable;
	e_group_class->compute_location = etgl_compute_location;
	e_group_class->get_mouse_over = etgl_get_mouse_over;
	e_group_class->get_cell_geometry = etgl_get_cell_geometry;

	g_object_class_install_property (
		object_class,
		PROP_TABLE_ALTERNATING_ROW_COLORS,
		g_param_spec_boolean (
			"alternating_row_colors",
			"Alternating Row Colors",
			"Alternating Row Colors",
			FALSE,
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		object_class,
		PROP_TABLE_HORIZONTAL_DRAW_GRID,
		g_param_spec_boolean (
			"horizontal_draw_grid",
			"Horizontal Draw Grid",
			"Horizontal Draw Grid",
			FALSE,
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		object_class,
		PROP_TABLE_VERTICAL_DRAW_GRID,
		g_param_spec_boolean (
			"vertical_draw_grid",
			"Vertical Draw Grid",
			"Vertical Draw Grid",
			FALSE,
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		object_class,
		PROP_TABLE_DRAW_FOCUS,
		g_param_spec_boolean (
			"drawfocus",
			"Draw focus",
			"Draw focus",
			FALSE,
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		object_class,
		PROP_CURSOR_MODE,
		g_param_spec_int (
			"cursor_mode",
			"Cursor mode",
			"Cursor mode",
			E_CURSOR_LINE,
			E_CURSOR_SPREADSHEET,
			E_CURSOR_LINE,
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		object_class,
		PROP_LENGTH_THRESHOLD,
		g_param_spec_int (
			"length_threshold",
			"Length Threshold",
			"Length Threshold",
			-1, G_MAXINT, 0,
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		object_class,
		PROP_SELECTION_MODEL,
		g_param_spec_object (
			"selection_model",
			"Selection model",
			"Selection model",
			E_TYPE_SELECTION_MODEL,
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		object_class,
		PROP_HEIGHT,
		g_param_spec_double (
			"height",
			"Height",
			"Height",
			0.0, G_MAXDOUBLE, 0.0,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_WIDTH,
		g_param_spec_double (
			"width",
			"Width",
			"Width",
			0.0, G_MAXDOUBLE, 0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_MINIMUM_WIDTH,
		g_param_spec_double (
			"minimum_width",
			"Minimum width",
			"Minimum Width",
			0.0, G_MAXDOUBLE, 0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_FROZEN,
		g_param_spec_boolean (
			"frozen",
			"Frozen",
			"Frozen",
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_UNIFORM_ROW_HEIGHT,
		g_param_spec_boolean (
			"uniform_row_height",
			"Uniform row height",
			"Uniform row height",
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_override_property (
		object_class,
		PROP_IS_EDITING,
		"is-editing");
}

static void
e_table_group_leaf_init (ETableGroupLeaf *etgl)
{
	etgl->width = 1;
	etgl->height = 1;
	etgl->minimum_width = 0;

	etgl->ets = NULL;
	etgl->item = NULL;

	etgl->etgl_cursor_change_id = 0;
	etgl->etgl_cursor_activated_id = 0;
	etgl->etgl_double_click_id = 0;
	etgl->etgl_right_click_id = 0;
	etgl->etgl_click_id = 0;
	etgl->etgl_key_press_id = 0;
	etgl->etgl_start_drag_id = 0;

	etgl->alternating_row_colors = 1;
	etgl->horizontal_draw_grid = 1;
	etgl->vertical_draw_grid = 1;
	etgl->draw_focus = 1;
	etgl->cursor_mode = E_CURSOR_SIMPLE;
	etgl->length_threshold = -1;

	etgl->selection_model = NULL;
	etgl->uniform_row_height = FALSE;

	e_canvas_item_set_reflow_callback (GNOME_CANVAS_ITEM (etgl), etgl_reflow);
}

gboolean
e_table_group_leaf_is_editing (ETableGroupLeaf *etgl)
{
	g_return_val_if_fail (E_IS_TABLE_GROUP_LEAF (etgl), FALSE);

	return etgl->item && e_table_item_is_editing (etgl->item);
}
