/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-Table-Group.c: Implements the grouping objects for elements on a table
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org ()
 *
 * Copyright 1999, Ximian, Inc.
 */

#include <config.h>
#include <gtk/gtksignal.h>
#include <libgnomeui/gnome-canvas-rect-ellipse.h>
#include "e-table-group-leaf.h"
#include "e-table-item.h"
#include "e-table-sorted-variable.h"
#include "e-table-sorted.h"
#include "gal/util/e-util.h"
#include "gal/widgets/e-canvas.h"

#define PARENT_TYPE e_table_group_get_type ()

static GnomeCanvasGroupClass *etgl_parent_class;

/* The arguments we take */
enum {
	ARG_0,
	ARG_HEIGHT,
	ARG_WIDTH,
	ARG_MINIMUM_WIDTH,
	ARG_FROZEN,
	ARG_TABLE_ALTERNATING_ROW_COLORS,
	ARG_TABLE_HORIZONTAL_DRAW_GRID,
	ARG_TABLE_VERTICAL_DRAW_GRID,
	ARG_TABLE_DRAW_FOCUS,
	ARG_CURSOR_MODE,
	ARG_LENGTH_THRESHOLD,
	ARG_SELECTION_MODEL,
};

static void etgl_set_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void etgl_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);

static void
etgl_destroy (GtkObject *object)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF(object);
	if (etgl->ets)
		gtk_object_unref (GTK_OBJECT(etgl->ets));
	if (etgl->item)
		gtk_object_destroy (GTK_OBJECT(etgl->item));
	if (etgl->selection_model)
		gtk_object_unref (GTK_OBJECT(etgl->selection_model));
	if (GTK_OBJECT_CLASS (etgl_parent_class)->destroy)
		GTK_OBJECT_CLASS (etgl_parent_class)->destroy (object);
}

static void
e_table_group_leaf_construct (GnomeCanvasGroup *parent,
			      ETableGroupLeaf  *etgl,
			      ETableHeader     *full_header,
			      ETableHeader     *header,
			      ETableModel      *model,
			      ETableSortInfo   *sort_info)
{
	etgl->is_grouped = e_table_sort_info_grouping_get_count(sort_info);
	if (etgl->is_grouped)
		etgl->ets = E_TABLE_SUBSET(e_table_sorted_variable_new (model, full_header, sort_info));
	else
		etgl->ets = E_TABLE_SUBSET(e_table_sorted_new (model, full_header, sort_info));
	gtk_object_ref(GTK_OBJECT(etgl->ets));
	gtk_object_sink(GTK_OBJECT(etgl->ets));
	e_table_group_construct (parent, E_TABLE_GROUP (etgl), full_header, header, model);
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
e_table_group_leaf_new       (GnomeCanvasGroup *parent,
			      ETableHeader     *full_header,
			      ETableHeader     *header,
			      ETableModel      *model,
			      ETableSortInfo   *sort_info)
{
	ETableGroupLeaf *etgl;

	g_return_val_if_fail (parent != NULL, NULL);
	
	etgl = gtk_type_new (e_table_group_leaf_get_type ());

	e_table_group_leaf_construct (parent, etgl, full_header,
				      header, model, sort_info);
	return E_TABLE_GROUP (etgl);
}

static void
etgl_cursor_change (GtkObject *object, gint row, ETableGroupLeaf *etgl)
{
	if (row < E_TABLE_SUBSET(etgl->ets)->n_map)
		e_table_group_cursor_change (E_TABLE_GROUP(etgl), E_TABLE_SUBSET(etgl->ets)->map_table[row]);
}

static void
etgl_cursor_activated (GtkObject *object, gint row, ETableGroupLeaf *etgl)
{
	if (row < E_TABLE_SUBSET(etgl->ets)->n_map)
		e_table_group_cursor_activated (E_TABLE_GROUP(etgl), E_TABLE_SUBSET(etgl->ets)->map_table[row]);
}

static void
etgl_double_click (GtkObject *object, gint row, gint col, GdkEvent *event, ETableGroupLeaf *etgl)
{
	e_table_group_double_click (E_TABLE_GROUP(etgl), row, col, event);
}

static gint
etgl_key_press (GtkObject *object, gint row, gint col, GdkEvent *event, ETableGroupLeaf *etgl)
{
	if (row < E_TABLE_SUBSET(etgl->ets)->n_map)
		return e_table_group_key_press (E_TABLE_GROUP(etgl), E_TABLE_SUBSET(etgl->ets)->map_table[row], col, event);
	else
		return 0;
}

static gint
etgl_right_click (GtkObject *object, gint row, gint col, GdkEvent *event, ETableGroupLeaf *etgl)
{
	if (row < E_TABLE_SUBSET(etgl->ets)->n_map)
		return e_table_group_right_click (E_TABLE_GROUP(etgl), E_TABLE_SUBSET(etgl->ets)->map_table[row], col, event);
	else
		return 0;
}

static gint
etgl_click (GtkObject *object, gint row, gint col, GdkEvent *event, ETableGroupLeaf *etgl)
{
	if (row < E_TABLE_SUBSET(etgl->ets)->n_map)
		return e_table_group_click (E_TABLE_GROUP(etgl), E_TABLE_SUBSET(etgl->ets)->map_table[row], col, event);
	else
		return 0;
}

static void
etgl_reflow (GnomeCanvasItem *item, gint flags)
{
	ETableGroupLeaf *leaf = E_TABLE_GROUP_LEAF(item);
	gtk_object_get(GTK_OBJECT(leaf->item),
		       "height", &leaf->height,
		       NULL);
	gtk_object_get(GTK_OBJECT(leaf->item),
		       "width", &leaf->width,
		       NULL);
	e_canvas_item_request_parent_reflow (item);
}

static void
etgl_realize (GnomeCanvasItem *item)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF(item);

	if (GNOME_CANVAS_ITEM_CLASS (etgl_parent_class)->realize)
		GNOME_CANVAS_ITEM_CLASS (etgl_parent_class)->realize (item);

	etgl->item = E_TABLE_ITEM(gnome_canvas_item_new (GNOME_CANVAS_GROUP(etgl),
							 e_table_item_get_type (),
							 "ETableHeader", E_TABLE_GROUP(etgl)->header,
							 "ETableModel", etgl->ets,
							 "alternating_row_colors", etgl->alternating_row_colors,
							 "horizontal_draw_grid", etgl->horizontal_draw_grid,
							 "vertical_draw_grid", etgl->vertical_draw_grid,
							 "drawfocus", etgl->draw_focus,
							 "cursor_mode", etgl->cursor_mode,
							 "minimum_width", etgl->minimum_width,
							 "length_threshold", etgl->length_threshold,
							 "selection_model", etgl->selection_model,
							 NULL));
	
	gtk_signal_connect (GTK_OBJECT(etgl->item), "cursor_change",
			    GTK_SIGNAL_FUNC(etgl_cursor_change), etgl);
	gtk_signal_connect (GTK_OBJECT(etgl->item), "cursor_activated",
			    GTK_SIGNAL_FUNC(etgl_cursor_activated), etgl);
	gtk_signal_connect (GTK_OBJECT(etgl->item), "double_click",
			    GTK_SIGNAL_FUNC(etgl_double_click), etgl);
	gtk_signal_connect (GTK_OBJECT(etgl->item), "right_click",
			    GTK_SIGNAL_FUNC(etgl_right_click), etgl);
	gtk_signal_connect (GTK_OBJECT(etgl->item), "click",
			    GTK_SIGNAL_FUNC(etgl_click), etgl);
	gtk_signal_connect (GTK_OBJECT(etgl->item), "key_press",
			    GTK_SIGNAL_FUNC(etgl_key_press), etgl);
	e_canvas_item_request_reflow(item);
}

static void
etgl_add (ETableGroup *etg, gint row)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);
	if (E_IS_TABLE_SUBSET_VARIABLE(etgl->ets)) {
		e_table_subset_variable_add (E_TABLE_SUBSET_VARIABLE(etgl->ets), row);
	}
}

static void
etgl_add_array (ETableGroup *etg, const gint *array, gint count)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);
	if (E_IS_TABLE_SUBSET_VARIABLE(etgl->ets)) {
		e_table_subset_variable_add_array (E_TABLE_SUBSET_VARIABLE(etgl->ets), array, count);
	}
}

static void
etgl_add_all (ETableGroup *etg)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);
	if (E_IS_TABLE_SUBSET_VARIABLE(etgl->ets)) {
		e_table_subset_variable_add_all (E_TABLE_SUBSET_VARIABLE(etgl->ets));
	}
}

static gboolean
etgl_remove (ETableGroup *etg, gint row)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);
	if (E_IS_TABLE_SUBSET_VARIABLE(etgl->ets)) {
		return e_table_subset_variable_remove (E_TABLE_SUBSET_VARIABLE(etgl->ets), row);
	}
	return FALSE;
}

static void
etgl_increment (ETableGroup *etg, gint position, gint amount)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);
	if (E_IS_TABLE_SUBSET_VARIABLE(etgl->ets)) {
		e_table_subset_variable_increment (E_TABLE_SUBSET_VARIABLE(etgl->ets), position, amount);
	}
}

static void
etgl_decrement (ETableGroup *etg, gint position, gint amount)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);
	if (E_IS_TABLE_SUBSET_VARIABLE(etgl->ets)) {
		e_table_subset_variable_decrement (E_TABLE_SUBSET_VARIABLE(etgl->ets), position, amount);
	}
}

static int
etgl_row_count (ETableGroup *etg)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);
	return e_table_model_row_count(E_TABLE_MODEL(etgl->ets));
}

static void
etgl_set_focus (ETableGroup *etg, EFocus direction, gint view_col)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);
	if (direction == E_FOCUS_END) {
		e_table_item_set_cursor (etgl->item, view_col, e_table_model_row_count(E_TABLE_MODEL(etgl->ets)) - 1);
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
etgl_compute_location (ETableGroup *etg, int *x, int *y, int *row, int *col)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);
	e_table_item_compute_location (etgl->item, x, y, row, col);
}

static void
etgl_get_cell_geometry (ETableGroup *etg, int *row, int *col, int *x, int *y, int *width, int *height)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (etg);
	e_table_item_get_cell_geometry (etgl->item, row, col, x, y, width, height);
}

static void
etgl_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ETableGroup *etg = E_TABLE_GROUP (object);
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (object);

	switch (arg_id) {
	case ARG_FROZEN:
		if (GTK_VALUE_BOOL (*arg))
			etg->frozen = TRUE;
		else {
			etg->frozen = FALSE;
		}
		break;
	case ARG_MINIMUM_WIDTH:
	case ARG_WIDTH:
		etgl->minimum_width = GTK_VALUE_DOUBLE(*arg);
		if (etgl->item) {
			gnome_canvas_item_set (GNOME_CANVAS_ITEM(etgl->item),
					       "minimum_width", etgl->minimum_width,
					       NULL);
		}
		break;
	case ARG_LENGTH_THRESHOLD:
		etgl->length_threshold = GTK_VALUE_INT (*arg);
		if (etgl->item) {
			gnome_canvas_item_set (GNOME_CANVAS_ITEM(etgl->item),
					       "length_threshold", GTK_VALUE_INT (*arg),
					       NULL);
		}
		break;
	case ARG_SELECTION_MODEL:
		if (etgl->selection_model)
			gtk_object_unref(GTK_OBJECT(etgl->selection_model));
		etgl->selection_model = E_SELECTION_MODEL(GTK_VALUE_OBJECT (*arg));
		if (etgl->selection_model)
			gtk_object_ref(GTK_OBJECT(etgl->selection_model));
		if (etgl->item) {
			gnome_canvas_item_set (GNOME_CANVAS_ITEM(etgl->item),
					       "selection_model", etgl->selection_model,
					       NULL);
		}
		break;

	case ARG_TABLE_ALTERNATING_ROW_COLORS:
		etgl->alternating_row_colors = GTK_VALUE_BOOL (*arg);
		if (etgl->item) {
			gnome_canvas_item_set (GNOME_CANVAS_ITEM(etgl->item),
					       "alternating_row_colors", GTK_VALUE_BOOL (*arg),
					       NULL);
		}
		break;

	case ARG_TABLE_HORIZONTAL_DRAW_GRID:
		etgl->horizontal_draw_grid = GTK_VALUE_BOOL (*arg);
		if (etgl->item) {
			gnome_canvas_item_set (GNOME_CANVAS_ITEM(etgl->item),
					       "horizontal_draw_grid", GTK_VALUE_BOOL (*arg),
					       NULL);
		}
		break;

	case ARG_TABLE_VERTICAL_DRAW_GRID:
		etgl->vertical_draw_grid = GTK_VALUE_BOOL (*arg);
		if (etgl->item) {
			gnome_canvas_item_set (GNOME_CANVAS_ITEM(etgl->item),
					       "vertical_draw_grid", GTK_VALUE_BOOL (*arg),
					       NULL);
		}
		break;

	case ARG_TABLE_DRAW_FOCUS:
		etgl->draw_focus = GTK_VALUE_BOOL (*arg);
		if (etgl->item) {
			gnome_canvas_item_set (GNOME_CANVAS_ITEM(etgl->item),
					"drawfocus", GTK_VALUE_BOOL (*arg),
					NULL);
		}
		break;

	case ARG_CURSOR_MODE:
		etgl->cursor_mode = GTK_VALUE_INT (*arg);
		if (etgl->item) {
			gnome_canvas_item_set (GNOME_CANVAS_ITEM(etgl->item),
					"cursor_mode", GTK_VALUE_INT (*arg),
					NULL);
		}
		break;
	default:
		break;
	}
}

static void
etgl_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ETableGroup *etg = E_TABLE_GROUP (object);
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (object);

	switch (arg_id) {
	case ARG_FROZEN:
		GTK_VALUE_BOOL (*arg) = etg->frozen;
		break;
	case ARG_HEIGHT:
		GTK_VALUE_DOUBLE (*arg) = etgl->height;
		break;
	case ARG_WIDTH:
		GTK_VALUE_DOUBLE (*arg) = etgl->width;
		break;
	case ARG_MINIMUM_WIDTH:
		GTK_VALUE_DOUBLE (*arg) = etgl->minimum_width;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
etgl_class_init (GtkObjectClass *object_class)
{
	GnomeCanvasItemClass *item_class = (GnomeCanvasItemClass *) object_class;
	ETableGroupClass *e_group_class = E_TABLE_GROUP_CLASS(object_class);

	object_class->destroy = etgl_destroy;
	object_class->set_arg = etgl_set_arg;
	object_class->get_arg = etgl_get_arg;

	item_class->realize = etgl_realize;
	
	etgl_parent_class = gtk_type_class (PARENT_TYPE);

	e_group_class->add = etgl_add;
	e_group_class->add_array = etgl_add_array;
	e_group_class->add_all = etgl_add_all;
	e_group_class->remove = etgl_remove;
	e_group_class->increment  = etgl_increment;
	e_group_class->decrement  = etgl_decrement;
	e_group_class->row_count  = etgl_row_count;
	e_group_class->set_focus  = etgl_set_focus;
	e_group_class->get_focus_column = etgl_get_focus_column;
	e_group_class->get_printable = etgl_get_printable;
	e_group_class->compute_location = etgl_compute_location;
	e_group_class->get_cell_geometry = etgl_get_cell_geometry;

	gtk_object_add_arg_type ("ETableGroupLeaf::alternating_row_colors", GTK_TYPE_BOOL,
				 GTK_ARG_WRITABLE, ARG_TABLE_ALTERNATING_ROW_COLORS);
	gtk_object_add_arg_type ("ETableGroupLeaf::horizontal_draw_grid", GTK_TYPE_BOOL,
				 GTK_ARG_WRITABLE, ARG_TABLE_HORIZONTAL_DRAW_GRID);
	gtk_object_add_arg_type ("ETableGroupLeaf::vertical_draw_grid", GTK_TYPE_BOOL,
				 GTK_ARG_WRITABLE, ARG_TABLE_VERTICAL_DRAW_GRID);
	gtk_object_add_arg_type ("ETableGroupLeaf::drawfocus", GTK_TYPE_BOOL,
				 GTK_ARG_WRITABLE, ARG_TABLE_DRAW_FOCUS);
	gtk_object_add_arg_type ("ETableGroupLeaf::cursor_mode", GTK_TYPE_INT,
				 GTK_ARG_WRITABLE, ARG_CURSOR_MODE);
	gtk_object_add_arg_type ("ETableGroupLeaf::length_threshold", GTK_TYPE_INT,
				 GTK_ARG_WRITABLE, ARG_LENGTH_THRESHOLD);
	gtk_object_add_arg_type ("ETableGroupLeaf::selection_model", E_SELECTION_MODEL_TYPE,
				 GTK_ARG_WRITABLE, ARG_SELECTION_MODEL);

	gtk_object_add_arg_type ("ETableGroupLeaf::height", GTK_TYPE_DOUBLE, 
				 GTK_ARG_READABLE, ARG_HEIGHT);
	gtk_object_add_arg_type ("ETableGroupLeaf::width", GTK_TYPE_DOUBLE, 
				 GTK_ARG_READWRITE, ARG_WIDTH);
	gtk_object_add_arg_type ("ETableGroupLeaf::minimum_width", GTK_TYPE_DOUBLE,
				 GTK_ARG_READWRITE, ARG_MINIMUM_WIDTH);
	gtk_object_add_arg_type ("ETableGroupLeaf::frozen", GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE, ARG_FROZEN);
}

static void
etgl_init (GtkObject *object)
{
	ETableGroupLeaf *etgl = E_TABLE_GROUP_LEAF (object);

	etgl->width = 1;
	etgl->height = 1;
	etgl->minimum_width = 0;

	etgl->ets = NULL;
	etgl->item = NULL;
	
	etgl->alternating_row_colors = 1;
	etgl->horizontal_draw_grid = 1;
	etgl->vertical_draw_grid = 1;
	etgl->draw_focus = 1;
	etgl->cursor_mode = E_CURSOR_SIMPLE;
	etgl->length_threshold = -1;

	etgl->selection_model = NULL;

	e_canvas_item_set_reflow_callback (GNOME_CANVAS_ITEM(object), etgl_reflow);
}

E_MAKE_TYPE (e_table_group_leaf, "ETableGroupLeaf", ETableGroupLeaf, etgl_class_init, etgl_init, PARENT_TYPE);
