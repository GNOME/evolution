/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-tree.c
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtksignal.h>
#include <libgnomeui/gnome-canvas-rect-ellipse.h>

#include "gal/util/e-i18n.h"
#include <gal/util/e-util.h>
#include <gal/widgets/e-canvas.h>

#include <gal/e-table/e-table-column-specification.h>
#include <gal/e-table/e-table-header-item.h>
#include <gal/e-table/e-table-header.h>
#include <gal/e-table/e-table-item.h>
#include <gal/e-table/e-table-sort-info.h>
#include <gal/e-table/e-table-utils.h>

#ifdef E_TREE_USE_TREE_SELECTION
#include <gal/e-table/e-tree-selection-model.h>
#else
#include <gal/e-table/e-table-selection-model.h>
#endif

#include <gal/e-table/e-tree-sorted.h>
#include <gal/e-table/e-tree-table-adapter.h>

#include "e-tree.h"

#define COLUMN_HEADER_HEIGHT 16

#define PARENT_TYPE gtk_table_get_type ()

static GtkObjectClass *parent_class;

#define d(x)

#if d(!)0
#define e_table_item_leave_edit_(x) (e_table_item_leave_edit((x)), g_print ("%s: e_table_item_leave_edit\n", __FUNCTION__))
#else
#define e_table_item_leave_edit_(x) (e_table_item_leave_edit((x)))
#endif

enum {
	CURSOR_CHANGE,
	CURSOR_ACTIVATED,
	SELECTION_CHANGE,
	DOUBLE_CLICK,
	RIGHT_CLICK,
	CLICK,
	KEY_PRESS,
	START_DRAG,

	TREE_DRAG_BEGIN,
	TREE_DRAG_END,
	TREE_DRAG_DATA_GET,
	TREE_DRAG_DATA_DELETE,

	TREE_DRAG_LEAVE,
	TREE_DRAG_MOTION,
	TREE_DRAG_DROP,
	TREE_DRAG_DATA_RECEIVED,

	LAST_SIGNAL
};

enum {
	ARG_0,
	ARG_LENGTH_THRESHOLD,
	ARG_HORIZONTAL_DRAW_GRID,
	ARG_VERTICAL_DRAW_GRID,
	ARG_DRAW_FOCUS,
	ARG_ETTA,
	ARG_UNIFORM_ROW_HEIGHT,
};

struct ETreePriv {
	ETreeModel *model;
	ETreeSorted *sorted;
	ETreeTableAdapter *etta;

	ETableHeader *full_header, *header;

	ETableSortInfo *sort_info;
	ESorter   *sorter;

	ESelectionModel *selection;
	ETableSpecification *spec;

	int reflow_idle_id;
	int scroll_idle_id;
	int hover_idle_id;

	int table_model_change_id;
	int table_row_change_id;
	int table_cell_change_id;
	int table_rows_inserted_id;
	int table_rows_deleted_id;

	GnomeCanvas *header_canvas, *table_canvas;

	GnomeCanvasItem *header_item, *root;

	GnomeCanvasItem *white_item;
	GnomeCanvasItem *item;

	gint length_threshold;

	/*
	 * Configuration settings
	 */
	guint alternating_row_colors : 1;
	guint horizontal_draw_grid : 1;
	guint vertical_draw_grid : 1;
	guint draw_focus : 1;
	guint row_selection_active : 1;

	guint horizontal_scrolling : 1;

	guint scroll_down : 1;

	guint do_drag : 1;

	guint uniform_row_height : 1;

	ECursorMode cursor_mode;

	int drop_row;
	ETreePath drop_path;
	int drop_col;

	GnomeCanvasItem *drop_highlight;
	int last_drop_x;
	int last_drop_y;
	int last_drop_time;
	GdkDragContext *last_drop_context;

	int hover_x;
	int hover_y;

	int drag_row;
	ETreePath drag_path;
	int drag_col;
	ETreeDragSourceSite *site;
};

static gint et_signals [LAST_SIGNAL] = { 0, };

static void et_grab_focus (GtkWidget *widget);

static void et_drag_begin (GtkWidget *widget,
			   GdkDragContext *context,
			   ETree *et);
static void et_drag_end (GtkWidget *widget,
			 GdkDragContext *context,
			 ETree *et);
static void et_drag_data_get(GtkWidget *widget,
			     GdkDragContext *context,
			     GtkSelectionData *selection_data,
			     guint info,
			     guint time,
			     ETree *et);
static void et_drag_data_delete(GtkWidget *widget,
				GdkDragContext *context,
				ETree *et);

static void et_drag_leave(GtkWidget *widget,
			  GdkDragContext *context,
			  guint time,
			  ETree *et);
static gboolean et_drag_motion(GtkWidget *widget,
			       GdkDragContext *context,
			       gint x,
			       gint y,
			       guint time,
			       ETree *et);
static gboolean et_drag_drop(GtkWidget *widget,
			     GdkDragContext *context,
			     gint x,
			     gint y,
			     guint time,
			     ETree *et);
static void et_drag_data_received(GtkWidget *widget,
				  GdkDragContext *context,
				  gint x,
				  gint y,
				  GtkSelectionData *selection_data,
				  guint info,
				  guint time,
				  ETree *et);

static gint et_focus (GtkContainer *container, GtkDirectionType direction);

static void scroll_off (ETree *et);
static void scroll_on (ETree *et, gboolean down);
static void hover_off (ETree *et);
static void hover_on (ETree *et, int x, int y);

static void
et_disconnect_from_etta (ETree *et)
{
	if (et->priv->table_model_change_id != 0)
		gtk_signal_disconnect (GTK_OBJECT (et->priv->etta),
				       et->priv->table_model_change_id);
	if (et->priv->table_row_change_id != 0)
		gtk_signal_disconnect (GTK_OBJECT (et->priv->etta),
				       et->priv->table_row_change_id);
	if (et->priv->table_cell_change_id != 0)
		gtk_signal_disconnect (GTK_OBJECT (et->priv->etta),
				       et->priv->table_cell_change_id);
	if (et->priv->table_rows_inserted_id != 0)
		gtk_signal_disconnect (GTK_OBJECT (et->priv->etta),
				       et->priv->table_rows_inserted_id);
	if (et->priv->table_rows_deleted_id != 0)
		gtk_signal_disconnect (GTK_OBJECT (et->priv->etta),
				       et->priv->table_rows_deleted_id);

	et->priv->table_model_change_id = 0;
	et->priv->table_row_change_id = 0;
	et->priv->table_cell_change_id = 0;
	et->priv->table_rows_inserted_id = 0;
	et->priv->table_rows_deleted_id = 0;
}

static void
et_destroy (GtkObject *object)
{
	ETree *et = E_TREE (object);

	if (et->priv->reflow_idle_id)
		g_source_remove(et->priv->reflow_idle_id);
	et->priv->reflow_idle_id = 0;

	scroll_off (et);
	hover_off (et);

	et_disconnect_from_etta (et);

	gtk_object_unref (GTK_OBJECT (et->priv->etta));
	gtk_object_unref (GTK_OBJECT (et->priv->model));
	gtk_object_unref (GTK_OBJECT (et->priv->sorted));
	gtk_object_unref (GTK_OBJECT (et->priv->full_header));
	gtk_object_unref (GTK_OBJECT (et->priv->header));
	gtk_object_unref (GTK_OBJECT (et->priv->sort_info));
	gtk_object_unref (GTK_OBJECT (et->priv->selection));
	if (et->priv->spec)
		gtk_object_unref (GTK_OBJECT (et->priv->spec));

	if (et->priv->sorter)
		gtk_object_unref (GTK_OBJECT (et->priv->sorter));

	if (et->priv->header_canvas)
		gtk_widget_destroy (GTK_WIDGET (et->priv->header_canvas));

	if (et->priv->site)
		e_tree_drag_source_unset (et);

	gtk_widget_destroy (GTK_WIDGET (et->priv->table_canvas));

	g_free(et->priv);
	et->priv = NULL;

	(*parent_class->destroy)(object);
}

static void
e_tree_init (GtkObject *object)
{
	ETree *e_tree                                    = E_TREE (object);
	GtkTable *gtk_table                              = GTK_TABLE (object);

	GTK_WIDGET_SET_FLAGS (e_tree, GTK_CAN_FOCUS);

	gtk_table->homogeneous                           = FALSE;

	e_tree->priv                                     = g_new(ETreePriv, 1);

	e_tree->priv->model                              = NULL;
	e_tree->priv->sorted                             = NULL;
	e_tree->priv->etta                               = NULL;

	e_tree->priv->full_header                        = NULL;
	e_tree->priv->header                             = NULL;

	e_tree->priv->sort_info                          = NULL;
	e_tree->priv->sorter                             = NULL;
	e_tree->priv->reflow_idle_id                     = 0;
	e_tree->priv->scroll_idle_id                     = 0;
	e_tree->priv->hover_idle_id                     = 0;

	e_tree->priv->alternating_row_colors             = 1;
	e_tree->priv->horizontal_draw_grid               = 1;
	e_tree->priv->vertical_draw_grid                 = 1;
	e_tree->priv->draw_focus                         = 1;
	e_tree->priv->cursor_mode                        = E_CURSOR_SIMPLE;
	e_tree->priv->length_threshold                   = 200;
	e_tree->priv->uniform_row_height                 = FALSE;

	e_tree->priv->row_selection_active               = FALSE;
	e_tree->priv->horizontal_scrolling               = FALSE;

	e_tree->priv->drop_row                           = -1;
	e_tree->priv->drop_path                          = NULL;
	e_tree->priv->drop_col                           = -1;
	e_tree->priv->drop_highlight                     = NULL;

	e_tree->priv->last_drop_x                        = 0;
	e_tree->priv->last_drop_y                        = 0;
	e_tree->priv->last_drop_time                     = 0;
	e_tree->priv->last_drop_context                  = NULL;

	e_tree->priv->hover_x                            = 0;
	e_tree->priv->hover_y                            = 0;

	e_tree->priv->drag_row                           = -1;
	e_tree->priv->drag_path                          = NULL;
	e_tree->priv->drag_col                           = -1;

	e_tree->priv->site                               = NULL;
	e_tree->priv->do_drag                            = FALSE;

#ifdef E_TREE_USE_TREE_SELECTION
	e_tree->priv->selection                          = E_SELECTION_MODEL(e_tree_selection_model_new());
#else
	e_tree->priv->selection                          = E_SELECTION_MODEL(e_table_selection_model_new());
#endif
	e_tree->priv->spec                               = NULL;

	e_tree->priv->header_canvas                      = NULL;
	e_tree->priv->table_canvas                       = NULL;

	e_tree->priv->header_item                        = NULL;
	e_tree->priv->root                               = NULL;

	e_tree->priv->white_item                         = NULL;
	e_tree->priv->item                               = NULL;
}

/* Grab_focus handler for the ETree */
static void
et_grab_focus (GtkWidget *widget)
{
	ETree *e_tree;

	e_tree = E_TREE (widget);

	gtk_widget_grab_focus (GTK_WIDGET (e_tree->priv->table_canvas));
}

/* Focus handler for the ETree */
static gint
et_focus (GtkContainer *container, GtkDirectionType direction)
{
	ETree *e_tree;

	e_tree = E_TREE (container);

	if (container->focus_child) {
		gtk_container_set_focus_child (container, NULL);
		return FALSE;
	}

	return gtk_container_focus (GTK_CONTAINER (e_tree->priv->table_canvas), direction);
}

static void
set_header_canvas_width (ETree *e_tree)
{
	double oldwidth, oldheight, width;

	if (!(e_tree->priv->header_item && e_tree->priv->header_canvas && e_tree->priv->table_canvas))
		return;

	gnome_canvas_get_scroll_region (GNOME_CANVAS (e_tree->priv->table_canvas),
					NULL, NULL, &width, NULL);
	gnome_canvas_get_scroll_region (GNOME_CANVAS (e_tree->priv->header_canvas),
					NULL, NULL, &oldwidth, &oldheight);

	if (oldwidth != width ||
	    oldheight != E_TABLE_HEADER_ITEM (e_tree->priv->header_item)->height - 1)
		gnome_canvas_set_scroll_region (
						GNOME_CANVAS (e_tree->priv->header_canvas),
						0, 0, width, /*  COLUMN_HEADER_HEIGHT - 1 */
						E_TABLE_HEADER_ITEM (e_tree->priv->header_item)->height - 1);

}

static void
header_canvas_size_allocate (GtkWidget *widget, GtkAllocation *alloc, ETree *e_tree)
{
	set_header_canvas_width (e_tree);

	/* When the header item is created ->height == 0,
	   as the font is only created when everything is realized.
	   So we set the usize here as well, so that the size of the
	   header is correct */
	if (GTK_WIDGET (e_tree->priv->header_canvas)->allocation.height !=
	    E_TABLE_HEADER_ITEM (e_tree->priv->header_item)->height)
		gtk_widget_set_usize (GTK_WIDGET (e_tree->priv->header_canvas), -1,
				      E_TABLE_HEADER_ITEM (e_tree->priv->header_item)->height);
}

static void
e_tree_setup_header (ETree *e_tree)
{
	char *pointer;
	e_tree->priv->header_canvas = GNOME_CANVAS (e_canvas_new ());
	GTK_WIDGET_UNSET_FLAGS (e_tree->priv->header_canvas, GTK_CAN_FOCUS);

	gtk_widget_show (GTK_WIDGET (e_tree->priv->header_canvas));

	pointer = g_strdup_printf("%p", e_tree);

	e_tree->priv->header_item = gnome_canvas_item_new (
		gnome_canvas_root (e_tree->priv->header_canvas),
		e_table_header_item_get_type (),
		"ETableHeader", e_tree->priv->header,
		"full_header", e_tree->priv->full_header,
		"sort_info", e_tree->priv->sort_info,
		"dnd_code", pointer,
		"tree", e_tree,
		NULL);

	g_free(pointer);

	gtk_signal_connect (
		GTK_OBJECT (e_tree->priv->header_canvas), "size_allocate",
		GTK_SIGNAL_FUNC (header_canvas_size_allocate), e_tree);

	gtk_widget_set_usize (GTK_WIDGET (e_tree->priv->header_canvas), -1,
			      E_TABLE_HEADER_ITEM (e_tree->priv->header_item)->height);
}

static gboolean
tree_canvas_reflow_idle (ETree *e_tree)
{
	gdouble height, width;
	gdouble item_height;
	gdouble oldheight, oldwidth;
	GtkAllocation *alloc = &(GTK_WIDGET (e_tree->priv->table_canvas)->allocation);

	gtk_object_get (GTK_OBJECT (e_tree->priv->item),
			"height", &height,
			"width", &width,
			NULL);
	item_height = height;
	height = MAX ((int)height, alloc->height);
	width = MAX((int)width, alloc->width);
	/* I have no idea why this needs to be -1, but it works. */
	gnome_canvas_get_scroll_region (GNOME_CANVAS (e_tree->priv->table_canvas),
					NULL, NULL, &oldwidth, &oldheight);

	if (oldwidth != width - 1 ||
	    oldheight != height - 1) {
		gnome_canvas_set_scroll_region (GNOME_CANVAS (e_tree->priv->table_canvas),
						0, 0, width - 1, height - 1);
		set_header_canvas_width (e_tree);
	}
	gtk_object_set (GTK_OBJECT (e_tree->priv->white_item),
			"y1", item_height,
			"x2", width,
			"y2", height,
			NULL);
	e_tree->priv->reflow_idle_id = 0;
	return FALSE;
}

static void
tree_canvas_size_allocate (GtkWidget *widget, GtkAllocation *alloc,
			    ETree *e_tree)
{
	gdouble width;
	gdouble height;
	gdouble item_height;

	width = alloc->width;
	gtk_object_get (GTK_OBJECT (e_tree->priv->item),
			"height", &height,
			NULL);
	item_height = height;
	height = MAX ((int)height, alloc->height);

	gtk_object_set (GTK_OBJECT (e_tree->priv->item),
			"width", width,
			NULL);
	gtk_object_set (GTK_OBJECT (e_tree->priv->header),
			"width", width,
			NULL);
	if (e_tree->priv->reflow_idle_id)
		g_source_remove(e_tree->priv->reflow_idle_id);
	tree_canvas_reflow_idle(e_tree);
}

static void
tree_canvas_reflow (GnomeCanvas *canvas, ETree *e_tree)
{
	if (!e_tree->priv->reflow_idle_id)
		e_tree->priv->reflow_idle_id = g_idle_add_full (400, (GSourceFunc) tree_canvas_reflow_idle, e_tree, NULL);
}

static void
item_cursor_change (ETableItem *eti, int row, ETree *et)
{
	ETreePath path = e_tree_table_adapter_node_at_row(et->priv->etta, row);
	path = e_tree_sorted_view_to_model_path(et->priv->sorted, path);
	gtk_signal_emit (GTK_OBJECT (et),
			 et_signals [CURSOR_CHANGE],
			 row, path);
}

static void
item_cursor_activated (ETableItem *eti, int row, ETree *et)
{
	ETreePath path = e_tree_table_adapter_node_at_row(et->priv->etta, row);
	if (path)
		path = e_tree_sorted_view_to_model_path(et->priv->sorted, path);
	gtk_signal_emit (GTK_OBJECT (et),
			 et_signals [CURSOR_ACTIVATED],
			 row, path);
	d(g_print("%s: Emitted CURSOR_ACTIVATED signal on row: %d and path: 0x%p\n", __FUNCTION__, row, path));
}

static void
item_double_click (ETableItem *eti, int row, int col, GdkEvent *event, ETree *et)
{
	ETreePath path = e_tree_table_adapter_node_at_row(et->priv->etta, row);
	path = e_tree_sorted_view_to_model_path(et->priv->sorted, path);
	gtk_signal_emit (GTK_OBJECT (et),
			 et_signals [DOUBLE_CLICK],
			 row, path, col, event);
}

static gint
item_right_click (ETableItem *eti, int row, int col, GdkEvent *event, ETree *et)
{
	int return_val = 0;
	ETreePath path = e_tree_table_adapter_node_at_row(et->priv->etta, row);
	path = e_tree_sorted_view_to_model_path(et->priv->sorted, path);
	gtk_signal_emit (GTK_OBJECT (et),
			 et_signals [RIGHT_CLICK],
			 row, path, col, event, &return_val);
	return return_val;
}

static gint
item_click (ETableItem *eti, int row, int col, GdkEvent *event, ETree *et)
{
	int return_val = 0;
	ETreePath path = e_tree_table_adapter_node_at_row(et->priv->etta, row);
	path = e_tree_sorted_view_to_model_path(et->priv->sorted, path);
	gtk_signal_emit (GTK_OBJECT (et),
			 et_signals [CLICK],
			 row, path, col, event, &return_val);
	return return_val;
}

static gint
item_key_press (ETableItem *eti, int row, int col, GdkEvent *event, ETree *et)
{
	int return_val = 0;
	GdkEventKey *key = (GdkEventKey *) event;
	ETreePath path;
	int y, row_local, col_local;
	GtkAdjustment *vadj;

	switch (key->keyval) {
	case GDK_Page_Down:
	case GDK_KP_Page_Down:
		vadj = gtk_layout_get_vadjustment (GTK_LAYOUT (et->priv->table_canvas));
		y = CLAMP(vadj->value + (2 * vadj->page_size - 50), 0, vadj->upper);
		y -= vadj->value;
		e_tree_get_cell_at (et, 30, y, &row_local, &col_local);

		if (row_local == -1)
			row_local = e_table_model_row_count (E_TABLE_MODEL(et->priv->etta)) - 1;

		row_local = e_tree_view_to_model_row (et, row_local);
		col_local = e_selection_model_cursor_col (E_SELECTION_MODEL (et->priv->selection));
		e_selection_model_select_as_key_press (E_SELECTION_MODEL (et->priv->selection), row_local, col_local, key->state);

		return_val = 1;
		break;
	case GDK_Page_Up:
	case GDK_KP_Page_Up:
		vadj = gtk_layout_get_vadjustment (GTK_LAYOUT (et->priv->table_canvas));
		y = CLAMP(vadj->value - (vadj->page_size - 50), 0, vadj->upper);
		y -= vadj->value;
		e_tree_get_cell_at (et, 30, y, &row_local, &col_local);

		if (row_local == -1)
			row_local = e_table_model_row_count (E_TABLE_MODEL(et->priv->etta)) - 1;

		row_local = e_tree_view_to_model_row (et, row_local);
		col_local = e_selection_model_cursor_col (E_SELECTION_MODEL (et->priv->selection));
		e_selection_model_select_as_key_press (E_SELECTION_MODEL (et->priv->selection), row_local, col_local, key->state);

		return_val = 1;
		break;
	case '=':
	case GDK_Right:
	case GDK_KP_Right:
		if (row != -1) {
			path = e_tree_table_adapter_node_at_row(et->priv->etta, row);
			if (path)
				e_tree_table_adapter_node_set_expanded (et->priv->etta, path, TRUE);
		}
		return_val = 1;
		break;
	case '-':
	case GDK_Left:
	case GDK_KP_Left:
		if (row != -1) {
			path = e_tree_table_adapter_node_at_row(et->priv->etta, row);
			if (path)
				e_tree_table_adapter_node_set_expanded (et->priv->etta, path, FALSE);
		}
		return_val = 1;
		break;
	default:
		path = e_tree_table_adapter_node_at_row(et->priv->etta, row);
		path = e_tree_sorted_view_to_model_path(et->priv->sorted, path);
		gtk_signal_emit (GTK_OBJECT (et),
				 et_signals [KEY_PRESS],
				 row, path, col, event, &return_val);
		break;
	}
	return return_val;
}

static gint
item_start_drag (ETableItem *eti, int row, int col, GdkEvent *event, ETree *et)
{
	ETreePath path;
	gint return_val = 0;

	path = e_tree_table_adapter_node_at_row(et->priv->etta, row);
	path = e_tree_sorted_view_to_model_path(et->priv->sorted, path);

	gtk_signal_emit (GTK_OBJECT (et),
			 et_signals [START_DRAG],
			 row, path, col, event, &return_val);

	return return_val;
}

static void
et_selection_model_selection_changed (ETableSelectionModel *etsm, ETree *et)
{
	gtk_signal_emit (GTK_OBJECT (et),
			 et_signals [SELECTION_CHANGE]);
}

static void
et_selection_model_selection_row_changed (ETableSelectionModel *etsm, int row, ETree *et)
{
	gtk_signal_emit (GTK_OBJECT (et),
			 et_signals [SELECTION_CHANGE]);
}

static void
et_build_item (ETree *et)
{
	et->priv->item = gnome_canvas_item_new(GNOME_CANVAS_GROUP (gnome_canvas_root(et->priv->table_canvas)),
					 e_table_item_get_type(),
					 "ETableHeader", et->priv->header,
					 "ETableModel", et->priv->etta,
					 "selection_model", et->priv->selection,
					 "alternating_row_colors", et->priv->alternating_row_colors,
					 "horizontal_draw_grid", et->priv->horizontal_draw_grid,
					 "vertical_draw_grid", et->priv->vertical_draw_grid,
					 "drawfocus", et->priv->draw_focus,
					 "cursor_mode", et->priv->cursor_mode,
					 "length_threshold", et->priv->length_threshold,
					 "uniform_row_height", et->priv->uniform_row_height,
					 NULL);

	gtk_signal_connect (GTK_OBJECT (et->priv->item), "cursor_change",
			    GTK_SIGNAL_FUNC (item_cursor_change), et);
	gtk_signal_connect (GTK_OBJECT (et->priv->item), "cursor_activated",
			    GTK_SIGNAL_FUNC (item_cursor_activated), et);
	gtk_signal_connect (GTK_OBJECT (et->priv->item), "double_click",
			    GTK_SIGNAL_FUNC (item_double_click), et);
	gtk_signal_connect (GTK_OBJECT (et->priv->item), "right_click",
			    GTK_SIGNAL_FUNC (item_right_click), et);
	gtk_signal_connect (GTK_OBJECT (et->priv->item), "click",
			    GTK_SIGNAL_FUNC (item_click), et);
	gtk_signal_connect (GTK_OBJECT (et->priv->item), "key_press",
			    GTK_SIGNAL_FUNC (item_key_press), et);
	gtk_signal_connect (GTK_OBJECT (et->priv->item), "start_drag",
			    GTK_SIGNAL_FUNC (item_start_drag), et);
}

static void
et_canvas_realize (GtkWidget *canvas, ETree *e_tree)
{
	gnome_canvas_item_set(
		e_tree->priv->white_item,
		"fill_color_gdk", &GTK_WIDGET(e_tree->priv->table_canvas)->style->base[GTK_STATE_NORMAL],
		NULL);
}

static gint
et_canvas_root_event (GnomeCanvasItem *root, GdkEvent *event, ETree *e_tree)
{
	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
		if (event->button.button != 4 && event->button.button != 5) {
			if (GTK_WIDGET_HAS_FOCUS(root->canvas)) {
				GnomeCanvasItem *item = GNOME_CANVAS(root->canvas)->focused_item;

				if (E_IS_TABLE_ITEM(item)) {
					e_table_item_leave_edit_(E_TABLE_ITEM(item));
					return TRUE;
				}
			}
		}
		break;
	default:
		break;
	}

	return FALSE;
}

/* Handler for focus events in the table_canvas; we have to repaint ourselves
 * and give the focus to some ETableItem.
 */
static gint
table_canvas_focus_event_cb (GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
	GnomeCanvas *canvas;
	ETree *tree;

	gtk_widget_queue_draw (widget);

	if (!event->in)
		return TRUE;

	canvas = GNOME_CANVAS (widget);
	tree = E_TREE (data);

	if (!canvas->focused_item) {
		e_table_item_set_cursor (E_TABLE_ITEM (tree->priv->item), 0, 0);
		gnome_canvas_item_grab_focus (tree->priv->item);
	}

	return TRUE;
}

static void
e_tree_setup_table (ETree *e_tree)
{
	e_tree->priv->table_canvas = GNOME_CANVAS (e_canvas_new ());
	gtk_signal_connect (
		GTK_OBJECT (e_tree->priv->table_canvas), "size_allocate",
		GTK_SIGNAL_FUNC (tree_canvas_size_allocate), e_tree);
	gtk_signal_connect (
		GTK_OBJECT (e_tree->priv->table_canvas), "focus_in_event",
		GTK_SIGNAL_FUNC (table_canvas_focus_event_cb), e_tree);
	gtk_signal_connect (
		GTK_OBJECT (e_tree->priv->table_canvas), "focus_out_event",
		GTK_SIGNAL_FUNC (table_canvas_focus_event_cb), e_tree);

	gtk_signal_connect (
		GTK_OBJECT (e_tree->priv->table_canvas), "drag_begin",
		GTK_SIGNAL_FUNC (et_drag_begin), e_tree);
	gtk_signal_connect (
		GTK_OBJECT (e_tree->priv->table_canvas), "drag_end",
		GTK_SIGNAL_FUNC (et_drag_end), e_tree);
	gtk_signal_connect (
		GTK_OBJECT (e_tree->priv->table_canvas), "drag_data_get",
		GTK_SIGNAL_FUNC (et_drag_data_get), e_tree);
	gtk_signal_connect (
		GTK_OBJECT (e_tree->priv->table_canvas), "drag_data_delete",
		GTK_SIGNAL_FUNC (et_drag_data_delete), e_tree);
	gtk_signal_connect (
		GTK_OBJECT (e_tree), "drag_motion",
		GTK_SIGNAL_FUNC (et_drag_motion), e_tree);
	gtk_signal_connect (
		GTK_OBJECT (e_tree), "drag_leave",
		GTK_SIGNAL_FUNC (et_drag_leave), e_tree);
	gtk_signal_connect (
		GTK_OBJECT (e_tree), "drag_drop",
		GTK_SIGNAL_FUNC (et_drag_drop), e_tree);
	gtk_signal_connect (
		GTK_OBJECT (e_tree), "drag_data_received",
		GTK_SIGNAL_FUNC (et_drag_data_received), e_tree);

	gtk_signal_connect (GTK_OBJECT(e_tree->priv->table_canvas), "reflow",
			    GTK_SIGNAL_FUNC (tree_canvas_reflow), e_tree);

	gtk_widget_show (GTK_WIDGET (e_tree->priv->table_canvas));

	e_tree->priv->white_item = gnome_canvas_item_new(
		gnome_canvas_root(e_tree->priv->table_canvas),
		gnome_canvas_rect_get_type(),
		"x1", (double) 0,
		"y1", (double) 0,
		"x2", (double) 100,
		"y2", (double) 100,
		"fill_color_gdk", &GTK_WIDGET(e_tree->priv->table_canvas)->style->base[GTK_STATE_NORMAL],
		NULL);

	gtk_signal_connect (
		GTK_OBJECT(e_tree->priv->table_canvas), "realize",
		GTK_SIGNAL_FUNC(et_canvas_realize), e_tree);
	gtk_signal_connect (
		GTK_OBJECT(gnome_canvas_root (e_tree->priv->table_canvas)), "event",
		GTK_SIGNAL_FUNC(et_canvas_root_event), e_tree);

	et_build_item(e_tree);
}

void
e_tree_set_state_object(ETree *e_tree, ETableState *state)
{
	if (e_tree->priv->header)
		gtk_object_unref(GTK_OBJECT(e_tree->priv->header));
	e_tree->priv->header = e_table_state_to_header (GTK_WIDGET(e_tree), e_tree->priv->full_header, state);

	gtk_object_set (GTK_OBJECT (e_tree->priv->header),
			"width", (double) (GTK_WIDGET(e_tree->priv->table_canvas)->allocation.width),
			NULL);

	if (e_tree->priv->sort_info)
		gtk_object_unref(GTK_OBJECT(e_tree->priv->sort_info));

	if (state->sort_info) {
		e_tree->priv->sort_info = e_table_sort_info_duplicate(state->sort_info);
		e_table_sort_info_set_can_group (e_tree->priv->sort_info, FALSE);
	} else
		e_tree->priv->sort_info = NULL;

	if (e_tree->priv->header_item)
		gtk_object_set(GTK_OBJECT(e_tree->priv->header_item),
			       "ETableHeader", e_tree->priv->header,
			       "sort_info", e_tree->priv->sort_info,
			       NULL);

	if (e_tree->priv->item)
		gtk_object_set(GTK_OBJECT(e_tree->priv->item),
			       "ETableHeader", e_tree->priv->header,
			       NULL);

	if (e_tree->priv->sorted)
		gtk_object_set(GTK_OBJECT(e_tree->priv->sorted),
			       "sort_info", e_tree->priv->sort_info,
			       NULL);
}

/**
 * e_tree_set_state:
 * @e_tree: #ETree object that will be modified
 * @state_str: a string with the XML representation of the #ETableState.
 *
 * This routine sets the state (as described by #ETableState) of the
 * #ETree object.
 */
void
e_tree_set_state (ETree      *e_tree,
		   const gchar *state_str)
{
	ETableState *state;

	g_return_if_fail(e_tree != NULL);
	g_return_if_fail(E_IS_TREE(e_tree));
	g_return_if_fail(state_str != NULL);

	state = e_table_state_new();
	e_table_state_load_from_string(state, state_str);

	if (state->col_count > 0)
		e_tree_set_state_object(e_tree, state);

	gtk_object_unref(GTK_OBJECT(state));
}

/**
 * e_tree_load_state:
 * @e_tree: #ETree object that will be modified
 * @filename: name of the file containing the state to be loaded into the #ETree
 *
 * An #ETableState will be loaded form the file pointed by @filename into the
 * @e_tree object.
 */
void
e_tree_load_state (ETree      *e_tree,
		    const gchar *filename)
{
	ETableState *state;

	g_return_if_fail(e_tree != NULL);
	g_return_if_fail(E_IS_TREE(e_tree));
	g_return_if_fail(filename != NULL);

	state = e_table_state_new();
	e_table_state_load_from_file(state, filename);

	if (state->col_count > 0)
		e_tree_set_state_object(e_tree, state);

	gtk_object_unref(GTK_OBJECT(state));
}

/**
 * e_tree_get_state_object:
 * @e_tree: #ETree object to act on
 *
 * Builds an #ETableState corresponding to the current state of the
 * #ETree.
 *
 * Return value:
 * The %ETableState object generated.
 **/
ETableState *
e_tree_get_state_object (ETree *e_tree)
{
	ETableState *state;
	int full_col_count;
	int i, j;

	state = e_table_state_new();
	state->sort_info = e_tree->priv->sort_info;
	if (state->sort_info)
		gtk_object_ref(GTK_OBJECT(state->sort_info));

	state->col_count = e_table_header_count (e_tree->priv->header);
	full_col_count = e_table_header_count (e_tree->priv->full_header);
	state->columns = g_new(int, state->col_count);
	state->expansions = g_new(double, state->col_count);
	for (i = 0; i < state->col_count; i++) {
		ETableCol *col = e_table_header_get_column(e_tree->priv->header, i);
		state->columns[i] = -1;
		for (j = 0; j < full_col_count; j++) {
			if (col->col_idx == e_table_header_index(e_tree->priv->full_header, j)) {
				state->columns[i] = j;
				break;
			}
		}
		state->expansions[i] = col->expansion;
	}

	return state;
}

/**
 * e_tree_get_state:
 * @e_tree: The #ETree to act on
 * 
 * Builds a state object based on the current state and returns the
 * string corresponding to that state.
 * 
 * Return value: 
 * A string describing the current state of the #ETree.
 **/
gchar *
e_tree_get_state (ETree *e_tree)
{
	ETableState *state;
	gchar *string;

	state = e_tree_get_state_object(e_tree);
	string = e_table_state_save_to_string(state);
	gtk_object_unref(GTK_OBJECT(state));
	return string;
}

/**
 * e_tree_save_state:
 * @e_tree: The #ETree to act on
 * @filename: name of the file to save to
 *
 * Saves the state of the @e_tree object into the file pointed by
 * @filename.
 **/
void
e_tree_save_state (ETree      *e_tree,
		   const gchar *filename)
{
	ETableState *state;

	state = e_tree_get_state_object(e_tree);
	e_table_state_save_to_file(state, filename);
	gtk_object_unref(GTK_OBJECT(state));
}

/**
 * e_tree_get_spec:
 * @e_tree: The #ETree to query
 * 
 * Returns the specification object.
 * 
 * Return value:
 **/
ETableSpecification *
e_tree_get_spec (ETree *e_tree)
{
	return e_tree->priv->spec;
}

static void
et_table_model_changed (ETableModel *model, ETree *et)
{
	if (et->priv->horizontal_scrolling)
		e_table_header_update_horizontal(et->priv->header);
}

static void
et_table_row_changed (ETableModel *table_model, int row, ETree *et)
{
	et_table_model_changed (table_model, et);
}

static void
et_table_cell_changed (ETableModel *table_model, int view_col, int row, ETree *et)
{
	et_table_model_changed (table_model, et);
}

static void
et_table_rows_inserted (ETableModel *table_model, int row, int count, ETree *et)
{
	et_table_model_changed (table_model, et);
}

static void
et_table_rows_deleted (ETableModel *table_model, int row, int count, ETree *et)
{
	et_table_model_changed (table_model, et);
}

static void
et_connect_to_etta (ETree *et)
{
	et->priv->table_model_change_id = gtk_signal_connect (GTK_OBJECT (et->priv->etta), "model_changed",
							      GTK_SIGNAL_FUNC (et_table_model_changed), et);

	et->priv->table_row_change_id = gtk_signal_connect (GTK_OBJECT (et->priv->etta), "model_row_changed",
							    GTK_SIGNAL_FUNC (et_table_row_changed), et);

	et->priv->table_cell_change_id = gtk_signal_connect (GTK_OBJECT (et->priv->etta), "model_cell_changed",
							     GTK_SIGNAL_FUNC (et_table_cell_changed), et);

	et->priv->table_rows_inserted_id = gtk_signal_connect (GTK_OBJECT (et->priv->etta), "model_rows_inserted",
							       GTK_SIGNAL_FUNC (et_table_rows_inserted), et);

	et->priv->table_rows_deleted_id = gtk_signal_connect (GTK_OBJECT (et->priv->etta), "model_rows_deleted",
							      GTK_SIGNAL_FUNC (et_table_rows_deleted), et);

}

static ETree *
et_real_construct (ETree *e_tree, ETreeModel *etm, ETableExtras *ete,
		   ETableSpecification *specification, ETableState *state)
{
	int row = 0;

	if (ete)
		gtk_object_ref(GTK_OBJECT(ete));
	else
		ete = e_table_extras_new();

	e_tree->priv->alternating_row_colors = specification->alternating_row_colors;
	e_tree->priv->horizontal_draw_grid = specification->horizontal_draw_grid;
	e_tree->priv->vertical_draw_grid = specification->vertical_draw_grid;
	e_tree->priv->draw_focus = specification->draw_focus;
	e_tree->priv->cursor_mode = specification->cursor_mode;
	e_tree->priv->full_header = e_table_spec_to_full_header(specification, ete);

	e_tree->priv->header = e_table_state_to_header (GTK_WIDGET(e_tree), e_tree->priv->full_header, state);
	e_tree->priv->horizontal_scrolling = specification->horizontal_scrolling;

	e_tree->priv->sort_info = state->sort_info;
	gtk_object_ref (GTK_OBJECT (e_tree->priv->sort_info));

	e_table_sort_info_set_can_group (e_tree->priv->sort_info, FALSE);

	gtk_object_set(GTK_OBJECT(e_tree->priv->header),
		       "sort_info", e_tree->priv->sort_info,
		       NULL);

	e_tree->priv->model = etm;
	gtk_object_ref (GTK_OBJECT (etm));

	e_tree->priv->sorted = e_tree_sorted_new(etm, e_tree->priv->full_header, e_tree->priv->sort_info);

	e_tree->priv->etta = E_TREE_TABLE_ADAPTER(e_tree_table_adapter_new(E_TREE_MODEL(e_tree->priv->sorted)));

	et_connect_to_etta (e_tree);

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	e_tree->priv->sorter = e_sorter_new();

	gtk_object_set (GTK_OBJECT (e_tree->priv->selection),
			"sorter", e_tree->priv->sorter,
#ifdef E_TREE_USE_TREE_SELECTION
			"model", e_tree->priv->model,
			"ets", e_tree->priv->sorted,
			"etta", e_tree->priv->etta,
#else
			"model", e_tree->priv->etta,
#endif
			"selection_mode", specification->selection_mode,
			"cursor_mode", specification->cursor_mode,
			NULL);

	gtk_signal_connect(GTK_OBJECT(e_tree->priv->selection), "selection_changed",
			   GTK_SIGNAL_FUNC(et_selection_model_selection_changed), e_tree);
	gtk_signal_connect(GTK_OBJECT(e_tree->priv->selection), "selection_row_changed",
			   GTK_SIGNAL_FUNC(et_selection_model_selection_row_changed), e_tree);

	if (!specification->no_headers) {
		e_tree_setup_header (e_tree);
	}
	e_tree_setup_table (e_tree);

	gtk_layout_get_vadjustment (GTK_LAYOUT (e_tree->priv->table_canvas))->step_increment = 20;
	gtk_adjustment_changed(gtk_layout_get_vadjustment (GTK_LAYOUT (e_tree->priv->table_canvas)));
	gtk_layout_get_hadjustment (GTK_LAYOUT (e_tree->priv->table_canvas))->step_increment = 20;
	gtk_adjustment_changed(gtk_layout_get_hadjustment (GTK_LAYOUT (e_tree->priv->table_canvas)));

	if (!specification->no_headers) {
		/*
		 * The header
		 */
		gtk_table_attach (GTK_TABLE (e_tree), GTK_WIDGET (e_tree->priv->header_canvas),
				  0, 1, 0 + row, 1 + row,
				  GTK_FILL | GTK_EXPAND,
				  GTK_FILL, 0, 0);
		row ++;
	}
	gtk_table_attach (GTK_TABLE (e_tree), GTK_WIDGET (e_tree->priv->table_canvas),
			  0, 1, 0 + row, 1 + row,
			  GTK_FILL | GTK_EXPAND,
			  GTK_FILL | GTK_EXPAND,
			  0, 0);

	gtk_widget_pop_colormap ();
	gtk_widget_pop_visual ();

	gtk_object_unref(GTK_OBJECT(ete));

	return e_tree;
}

/**
 * e_tree_construct:
 * @e_tree: The newly created #ETree object.
 * @etm: The model for this table.
 * @ete: An optional #ETableExtras.  (%NULL is valid.)
 * @spec_str: The spec.
 * @state_str: An optional state.  (%NULL is valid.)
 * 
 * This is the internal implementation of e_tree_new() for use by
 * subclasses or language bindings.  See e_tree_new() for details.
 * 
 * Return value: 
 * The passed in value @e_tree or %NULL if there's an error.
 **/
ETree *
e_tree_construct (ETree *e_tree, ETreeModel *etm, ETableExtras *ete,
		   const char *spec_str, const char *state_str)
{
	ETableSpecification *specification;
	ETableState *state;

	g_return_val_if_fail(e_tree != NULL, NULL);
	g_return_val_if_fail(E_IS_TREE(e_tree), NULL);
	g_return_val_if_fail(etm != NULL, NULL);
	g_return_val_if_fail(E_IS_TREE_MODEL(etm), NULL);
	g_return_val_if_fail(ete == NULL || E_IS_TABLE_EXTRAS(ete), NULL);
	g_return_val_if_fail(spec_str != NULL, NULL);

	specification = e_table_specification_new();
	e_table_specification_load_from_string(specification, spec_str);
	if (state_str) {
		state = e_table_state_new();
		e_table_state_load_from_string(state, state_str);
		if (state->col_count <= 0) {
			gtk_object_unref(GTK_OBJECT(state));
			state = specification->state;
			gtk_object_ref(GTK_OBJECT(state));
		}
	} else {
		state = specification->state;
		gtk_object_ref(GTK_OBJECT(state));
	}

	e_tree = et_real_construct (e_tree, etm, ete, specification, state);

	e_tree->priv->spec = specification;
	gtk_object_unref(GTK_OBJECT(state));

	return e_tree;
}

/**
 * e_tree_construct_from_spec_file:
 * @e_tree: The newly created #ETree object.
 * @etm: The model for this tree
 * @ete: An optional #ETableExtras  (%NULL is valid.)
 * @spec_fn: The filename of the spec
 * @state_fn: An optional state file  (%NULL is valid.)
 *
 * This is the internal implementation of e_tree_new_from_spec_file()
 * for use by subclasses or language bindings.  See
 * e_tree_new_from_spec_file() for details.
 * 
 * Return value: 
 * The passed in value @e_tree or %NULL if there's an error.
 **/
ETree *
e_tree_construct_from_spec_file (ETree *e_tree, ETreeModel *etm, ETableExtras *ete,
				  const char *spec_fn, const char *state_fn)
{
	ETableSpecification *specification;
	ETableState *state;

	g_return_val_if_fail(e_tree != NULL, NULL);
	g_return_val_if_fail(E_IS_TREE(e_tree), NULL);
	g_return_val_if_fail(etm != NULL, NULL);
	g_return_val_if_fail(E_IS_TREE_MODEL(etm), NULL);
	g_return_val_if_fail(ete == NULL || E_IS_TABLE_EXTRAS(ete), NULL);
	g_return_val_if_fail(spec_fn != NULL, NULL);

	specification = e_table_specification_new();
	if (!e_table_specification_load_from_file(specification, spec_fn)) {
		gtk_object_unref(GTK_OBJECT(specification));
		return NULL;
	}

	if (state_fn) {
		state = e_table_state_new();
		if (!e_table_state_load_from_file(state, state_fn)) {
			gtk_object_unref(GTK_OBJECT(state));
			state = specification->state;
			gtk_object_ref(GTK_OBJECT(state));
		}
		if (state->col_count <= 0) {
			gtk_object_unref(GTK_OBJECT(state));
			state = specification->state;
			gtk_object_ref(GTK_OBJECT(state));
		}
	} else {
		state = specification->state;
		gtk_object_ref(GTK_OBJECT(state));
	}

	e_tree = et_real_construct (e_tree, etm, ete, specification, state);

	e_tree->priv->spec = specification;
	gtk_object_unref(GTK_OBJECT(state));

	return e_tree;
}

/**
 * e_tree_new:
 * @etm: The model for this tree
 * @ete: An optional #ETableExtras  (%NULL is valid.)
 * @spec: The spec
 * @state: An optional state  (%NULL is valid.)
 * 
 * This function creates an #ETree from the given parameters.  The
 * #ETreeModel is a tree model to be represented.  The #ETableExtras
 * is an optional set of pixbufs, cells, and sorting functions to be
 * used when interpreting the spec.  If you pass in %NULL it uses the
 * default #ETableExtras.  (See e_table_extras_new()).
 *
 * @spec is the specification of the set of viewable columns and the
 * default sorting state and such.  @state is an optional string
 * specifying the current sorting state and such.  If @state is NULL,
 * then the default state from the spec will be used.
 * 
 * Return value: 
 * The newly created #ETree or %NULL if there's an error.
 **/
GtkWidget *
e_tree_new (ETreeModel *etm, ETableExtras *ete, const char *spec, const char *state)
{
	ETree *e_tree, *ret_val;

	g_return_val_if_fail(etm != NULL, NULL);
	g_return_val_if_fail(E_IS_TREE_MODEL(etm), NULL);
	g_return_val_if_fail(ete == NULL || E_IS_TABLE_EXTRAS(ete), NULL);
	g_return_val_if_fail(spec != NULL, NULL);

	e_tree = gtk_type_new (e_tree_get_type ());

	ret_val = e_tree_construct (e_tree, etm, ete, spec, state);

	if (ret_val == NULL) {
		gtk_object_unref (GTK_OBJECT (e_tree));
	}

	return (GtkWidget *) ret_val;
}

/**
 * e_tree_new_from_spec_file:
 * @etm: The model for this tree.
 * @ete: An optional #ETableExtras.  (%NULL is valid.)
 * @spec_fn: The filename of the spec.
 * @state_fn: An optional state file.  (%NULL is valid.)
 * 
 * This is very similar to e_tree_new(), except instead of passing in
 * strings you pass in the file names of the spec and state to load.
 *
 * @spec_fn is the filename of the spec to load.  If this file doesn't
 * exist, e_tree_new_from_spec_file will return %NULL.
 *
 * @state_fn is the filename of the initial state to load.  If this is
 * %NULL or if the specified file doesn't exist, the default state
 * from the spec file is used.
 * 
 * Return value: 
 * The newly created #ETree or %NULL if there's an error.
 **/
GtkWidget *
e_tree_new_from_spec_file (ETreeModel *etm, ETableExtras *ete, const char *spec_fn, const char *state_fn)
{
	ETree *e_tree, *ret_val;

	g_return_val_if_fail(etm != NULL, NULL);
	g_return_val_if_fail(E_IS_TREE_MODEL(etm), NULL);
	g_return_val_if_fail(ete == NULL || E_IS_TABLE_EXTRAS(ete), NULL);
	g_return_val_if_fail(spec_fn != NULL, NULL);

	e_tree = gtk_type_new (e_tree_get_type ());

	ret_val = e_tree_construct_from_spec_file (e_tree, etm, ete, spec_fn, state_fn);

	if (ret_val == NULL) {
		gtk_object_unref (GTK_OBJECT (e_tree));
	}

	return (GtkWidget *) ret_val;
}

void
e_tree_set_cursor (ETree *e_tree, ETreePath path)
{
#ifndef E_TREE_USE_TREE_SELECTION
	int row;
#endif
	g_return_if_fail(e_tree != NULL);
	g_return_if_fail(E_IS_TREE(e_tree));
	g_return_if_fail(path != NULL);

#ifdef E_TREE_USE_TREE_SELECTION
	e_tree_selection_model_select_single_path (E_TREE_SELECTION_MODEL(e_tree->priv->selection), path);
	e_tree_selection_model_change_cursor (E_TREE_SELECTION_MODEL(e_tree->priv->selection), path);
#else
	path = e_tree_sorted_model_to_view_path(e_tree->priv->sorted, path);

	row = e_tree_table_adapter_row_of_node(E_TREE_TABLE_ADAPTER(e_tree->priv->etta), path);

	if (row == -1)
		return;

	gtk_object_set(GTK_OBJECT(e_tree->priv->selection),
		       "cursor_row", row,
		       NULL);
#endif
}

ETreePath
e_tree_get_cursor (ETree *e_tree)
{
	int row;
	ETreePath path;
	g_return_val_if_fail(e_tree != NULL, NULL);
	g_return_val_if_fail(E_IS_TREE(e_tree), NULL);

	gtk_object_get(GTK_OBJECT(e_tree->priv->selection),
		       "cursor_row", &row,
		       NULL);
	if (row == -1)
		return NULL;
	path = e_tree_table_adapter_node_at_row(E_TREE_TABLE_ADAPTER(e_tree->priv->etta), row);
	path = e_tree_sorted_view_to_model_path(e_tree->priv->sorted, path);
	return path;
}

void
e_tree_selected_row_foreach     (ETree *e_tree,
				  EForeachFunc callback,
				  gpointer closure)
{
	g_return_if_fail(e_tree != NULL);
	g_return_if_fail(E_IS_TREE(e_tree));

	e_selection_model_foreach(e_tree->priv->selection,
				  callback,
				  closure);
}

#ifdef E_TREE_USE_TREE_SELECTION
void
e_tree_selected_path_foreach     (ETree *e_tree,
				 ETreeForeachFunc callback,
				 gpointer closure)
{
	g_return_if_fail(e_tree != NULL);
	g_return_if_fail(E_IS_TREE(e_tree));

	e_tree_selection_model_foreach(E_TREE_SELECTION_MODEL (e_tree->priv->selection),
				       callback,
				       closure);
}
#endif

EPrintable *
e_tree_get_printable (ETree *e_tree)
{
	g_return_val_if_fail(e_tree != NULL, NULL);
	g_return_val_if_fail(E_IS_TREE(e_tree), NULL);

	return e_table_item_get_printable(E_TABLE_ITEM(e_tree->priv->item));
}

static void
et_get_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	ETree *etree = E_TREE (o);

	switch (arg_id){
	case ARG_ETTA:
		if (etree->priv->item) {
			GTK_VALUE_OBJECT (*arg) = GTK_OBJECT (etree->priv->etta);
		}
		break;
	case ARG_UNIFORM_ROW_HEIGHT:
		GTK_VALUE_BOOL (*arg) = etree->priv->uniform_row_height;
		break;

	default:
		break;
	}
}

typedef struct {
	char     *arg;
	gboolean  setting;
} bool_closure;

static void
et_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	ETree *etree = E_TREE (o);

	switch (arg_id){
	case ARG_LENGTH_THRESHOLD:
		etree->priv->length_threshold = GTK_VALUE_INT (*arg);
		if (etree->priv->item) {
			gnome_canvas_item_set (GNOME_CANVAS_ITEM(etree->priv->item),
					       "length_threshold", GTK_VALUE_INT (*arg),
					       NULL);
		}
		break;

	case ARG_HORIZONTAL_DRAW_GRID:
		etree->priv->horizontal_draw_grid = GTK_VALUE_BOOL (*arg);
		if (etree->priv->item) {
			gnome_canvas_item_set (GNOME_CANVAS_ITEM(etree->priv->item),
					       "horizontal_draw_grid", GTK_VALUE_BOOL (*arg),
					       NULL);
		}
		break;

	case ARG_VERTICAL_DRAW_GRID:
		etree->priv->vertical_draw_grid = GTK_VALUE_BOOL (*arg);
		if (etree->priv->item) {
			gnome_canvas_item_set (GNOME_CANVAS_ITEM(etree->priv->item),
					       "vertical_draw_grid", GTK_VALUE_BOOL (*arg),
					       NULL);
		}
		break;
		
	case ARG_DRAW_FOCUS:
		etree->priv->draw_focus = GTK_VALUE_BOOL (*arg);
		if (etree->priv->item) {
			gnome_canvas_item_set (GNOME_CANVAS_ITEM(etree->priv->item),
					       "draw_focus", GTK_VALUE_BOOL (*arg),
					       NULL);
		}
		break;

	case ARG_UNIFORM_ROW_HEIGHT:
		etree->priv->uniform_row_height = GTK_VALUE_BOOL (*arg);
		if (etree->priv->item) {
			gnome_canvas_item_set (GNOME_CANVAS_ITEM(etree->priv->item),
					       "uniform_row_height", GTK_VALUE_BOOL (*arg),
					       NULL);
		}
		break;
	}
}

static void
set_scroll_adjustments   (ETree *tree,
			  GtkAdjustment *hadjustment,
			  GtkAdjustment *vadjustment)
{
	if (vadjustment != NULL) {
		vadjustment->step_increment = 20;
		gtk_adjustment_changed(vadjustment);
	}
	if (hadjustment != NULL) {
		hadjustment->step_increment = 20;
		gtk_adjustment_changed(hadjustment);
	}

	gtk_layout_set_hadjustment (GTK_LAYOUT(tree->priv->table_canvas),
				    hadjustment);
	gtk_layout_set_vadjustment (GTK_LAYOUT(tree->priv->table_canvas),
				    vadjustment);

	if (tree->priv->header_canvas != NULL)
		gtk_layout_set_hadjustment (GTK_LAYOUT(tree->priv->header_canvas),
					    hadjustment);
}

gint
e_tree_get_next_row      (ETree *e_tree,
			   gint    model_row)
{
	g_return_val_if_fail(e_tree != NULL, -1);
	g_return_val_if_fail(E_IS_TREE(e_tree), -1);

	if (e_tree->priv->sorter) {
		int i;
		i = e_sorter_model_to_sorted(E_SORTER (e_tree->priv->sorter), model_row);
		i++;
		if (i < e_table_model_row_count(E_TABLE_MODEL(e_tree->priv->etta))) {
			return e_sorter_sorted_to_model(E_SORTER (e_tree->priv->sorter), i);
		} else
			return -1;
	} else
		if (model_row < e_table_model_row_count(E_TABLE_MODEL(e_tree->priv->etta)) - 1)
			return model_row + 1;
		else
			return -1;
}

gint
e_tree_get_prev_row      (ETree *e_tree,
			  gint    model_row)
{
	g_return_val_if_fail(e_tree != NULL, -1);
	g_return_val_if_fail(E_IS_TREE(e_tree), -1);

	if (e_tree->priv->sorter) {
		int i;
		i = e_sorter_model_to_sorted(E_SORTER (e_tree->priv->sorter), model_row);
		i--;
		if (i >= 0)
			return e_sorter_sorted_to_model(E_SORTER (e_tree->priv->sorter), i);
		else
			return -1;
	} else
		return model_row - 1;
}

gint
e_tree_model_to_view_row        (ETree *e_tree,
				  gint    model_row)
{
	g_return_val_if_fail(e_tree != NULL, -1);
	g_return_val_if_fail(E_IS_TREE(e_tree), -1);

	if (e_tree->priv->sorter)
		return e_sorter_model_to_sorted(E_SORTER (e_tree->priv->sorter), model_row);
	else
		return model_row;
}

gint
e_tree_view_to_model_row        (ETree *e_tree,
				  gint    view_row)
{
	g_return_val_if_fail(e_tree != NULL, -1);
	g_return_val_if_fail(E_IS_TREE(e_tree), -1);

	if (e_tree->priv->sorter)
		return e_sorter_sorted_to_model (E_SORTER (e_tree->priv->sorter), view_row);
	else
		return view_row;
}


gboolean
e_tree_node_is_expanded (ETree *et, ETreePath path)
{
	path = e_tree_sorted_model_to_view_path(et->priv->sorted, path);

	g_return_val_if_fail(path, FALSE);

	return e_tree_table_adapter_node_is_expanded (et->priv->etta, path);
}

void
e_tree_node_set_expanded (ETree *et, ETreePath path, gboolean expanded)
{
	g_return_if_fail (et != NULL);
	g_return_if_fail (E_IS_TREE(et));

	path = e_tree_sorted_model_to_view_path(et->priv->sorted, path);

	e_tree_table_adapter_node_set_expanded (et->priv->etta, path, expanded);
}

void
e_tree_node_set_expanded_recurse (ETree *et, ETreePath path, gboolean expanded)
{
	g_return_if_fail (et != NULL);
	g_return_if_fail (E_IS_TREE(et));

	path = e_tree_sorted_model_to_view_path(et->priv->sorted, path);

	e_tree_table_adapter_node_set_expanded_recurse (et->priv->etta, path, expanded);
}

void
e_tree_root_node_set_visible (ETree *et, gboolean visible)
{
	g_return_if_fail (et != NULL);
	g_return_if_fail (E_IS_TREE(et));

	e_tree_table_adapter_root_node_set_visible (et->priv->etta, visible);
}

ETreePath
e_tree_node_at_row (ETree *et, int row)
{
	ETreePath path;

	path = e_tree_table_adapter_node_at_row (et->priv->etta, row);
	path = e_tree_sorted_view_to_model_path(et->priv->sorted, path);

	return path;
}

int
e_tree_row_of_node (ETree *et, ETreePath path)
{
	path = e_tree_sorted_model_to_view_path(et->priv->sorted, path);
	return e_tree_table_adapter_row_of_node (et->priv->etta, path);
}

gboolean
e_tree_root_node_is_visible(ETree *et)
{
	return e_tree_table_adapter_root_node_is_visible (et->priv->etta);
}

void
e_tree_show_node (ETree *et, ETreePath path)
{
	g_return_if_fail (et != NULL);
	g_return_if_fail (E_IS_TREE(et));

	path = e_tree_sorted_model_to_view_path(et->priv->sorted, path);

	e_tree_table_adapter_show_node (et->priv->etta, path);
}

void
e_tree_save_expanded_state (ETree *et, char *filename)
{
	g_return_if_fail (et != NULL);
	g_return_if_fail (E_IS_TREE(et));

	e_tree_table_adapter_save_expanded_state (et->priv->etta, filename);
}

void
e_tree_load_expanded_state (ETree *et, char *filename)
{
	e_tree_table_adapter_load_expanded_state (et->priv->etta, filename);
}

gint
e_tree_row_count (ETree *et)
{
	return e_table_model_row_count (E_TABLE_MODEL(et->priv->etta));
}

GtkWidget *
e_tree_get_tooltip (ETree *et)
{
	return E_CANVAS(et->priv->table_canvas)->tooltip_window;
}

void
e_tree_right_click_up (ETree *et)
{
	e_selection_model_right_click_up(et->priv->selection);
}

/**
 * e_tree_get_model:
 * @et: the ETree
 *
 * Returns the model upon which this ETree is based.
 *
 * Returns: the model
 **/
ETreeModel *
e_tree_get_model (ETree *et)
{
	g_return_val_if_fail (et != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE (et), NULL);

	return et->priv->model;
}

/**
 * e_tree_get_selection_model:
 * @et: the ETree
 *
 * Returns the selection model of this ETree.
 *
 * Returns: the selection model
 **/
ESelectionModel *
e_tree_get_selection_model (ETree *et)
{
	g_return_val_if_fail (et != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE (et), NULL);

	return et->priv->selection;
}


struct _ETreeDragSourceSite
{
	GdkModifierType    start_button_mask;
	GtkTargetList     *target_list;        /* Targets for drag data */
	GdkDragAction      actions;            /* Possible actions */
	GdkColormap       *colormap;	         /* Colormap for drag icon */
	GdkPixmap         *pixmap;             /* Icon for drag data */
	GdkBitmap         *mask;

	/* Stored button press information to detect drag beginning */
	gint               state;
	gint               x, y;
	gint               row, col;
};

typedef enum
{
  GTK_DRAG_STATUS_DRAG,
  GTK_DRAG_STATUS_WAIT,
  GTK_DRAG_STATUS_DROP
} GtkDragStatus;

typedef struct _GtkDragDestInfo GtkDragDestInfo;
typedef struct _GtkDragSourceInfo GtkDragSourceInfo;

struct _GtkDragDestInfo
{
  GtkWidget         *widget;	   /* Widget in which drag is in */
  GdkDragContext    *context;	   /* Drag context */
  GtkDragSourceInfo *proxy_source; /* Set if this is a proxy drag */
  GtkSelectionData  *proxy_data;   /* Set while retrieving proxied data */
  gboolean           dropped : 1;     /* Set after we receive a drop */
  guint32            proxy_drop_time; /* Timestamp for proxied drop */
  gboolean           proxy_drop_wait : 1; /* Set if we are waiting for a
					   * status reply before sending
					   * a proxied drop on.
					   */
  gint               drop_x, drop_y; /* Position of drop */
};

struct _GtkDragSourceInfo
{
  GtkWidget         *widget;
  GtkTargetList     *target_list; /* Targets for drag data */
  GdkDragAction      possible_actions; /* Actions allowed by source */
  GdkDragContext    *context;	  /* drag context */
  GtkWidget         *icon_window; /* Window for drag */
  GtkWidget         *ipc_widget;  /* GtkInvisible for grab, message passing */
  GdkCursor         *cursor;	  /* Cursor for drag */
  gint hot_x, hot_y;		  /* Hot spot for drag */
  gint button;			  /* mouse button starting drag */

  GtkDragStatus      status;	  /* drag status */
  GdkEvent          *last_event;  /* motion event waiting for response */

  gint               start_x, start_y; /* Initial position */
  gint               cur_x, cur_y;     /* Current Position */

  GList             *selections;  /* selections we've claimed */

  GtkDragDestInfo   *proxy_dest;  /* Set if this is a proxy drag */

  guint              drop_timeout;     /* Timeout for aborting drop */
  guint              destroy_icon : 1; /* If true, destroy icon_window
					*/
};

/* Drag & drop stuff. */
/* Target */

void
e_tree_drag_get_data (ETree         *tree,
		      int             row,
		      int             col,
		      GdkDragContext *context,
		      GdkAtom         target,
		      guint32         time)
{
	ETreePath path;
	g_return_if_fail(tree != NULL);
	g_return_if_fail(E_IS_TREE(tree));

	path = e_tree_table_adapter_node_at_row(tree->priv->etta, row);
	path = e_tree_sorted_view_to_model_path(tree->priv->sorted, path);

	gtk_drag_get_data(GTK_WIDGET(tree),
			  context,
			  target,
			  time);

}

#if 0
static void
e_tree_request_hightlight_redraw (ETree *tree)
{
	int row = tree->drop_highlight_row;
	int col = tree->drop_highlight_col;

	if (row != -1) {
		int x, y, width, height;
		if (col == -1) {
			e_tree_get_cell_geometry (tree, row, 0, &x, &y, &width, &height);
			x = 0;
			width = tree->allocation.width;
		} else {
			e_tree_get_cell_geometry (tree, row, col, &x, &y, &width, &height);
			x += GTK_LAYOUT(tree->priv->table_canvas)->hadjustment->value;
		}
		y += GTK_LAYOUT(tree->priv->table_canvas)->vadjustment->value;
		gnome_canvas_request_redraw (tree->priv->table_canvas, x, y, x + width - 1, y + height - 1);
	}
}
#endif

/**
 * e_tree_drag_highlight:
 * @tree:
 * @row:
 * @col:
 *
 * Set col to -1 to highlight the entire row.
 * Set row to -1 to turn off the highlight.
 */
void
e_tree_drag_highlight (ETree *tree,
		       int     row,
		       int     col)
{
	g_return_if_fail(tree != NULL);
	g_return_if_fail(E_IS_TREE(tree));

	if (row != -1) {
		int x, y, width, height;
		if (col == -1) {
			e_tree_get_cell_geometry (tree, row, 0, &x, &y, &width, &height);
			x = 0;
			width = GTK_WIDGET (tree->priv->table_canvas)->allocation.width;
		} else {
			e_tree_get_cell_geometry (tree, row, col, &x, &y, &width, &height);
			x += GTK_LAYOUT(tree->priv->table_canvas)->hadjustment->value;
		}
		y += GTK_LAYOUT(tree->priv->table_canvas)->vadjustment->value;

		if (tree->priv->drop_highlight == NULL) {
			tree->priv->drop_highlight =
				gnome_canvas_item_new (gnome_canvas_root (tree->priv->table_canvas),
						       gnome_canvas_rect_get_type (),
						       "fill_color", NULL,
						       /*						       "outline_color", "black",
						       "width_pixels", 1,*/
						       "outline_color_gdk", &(GTK_WIDGET (tree)->style->fg[GTK_STATE_NORMAL]),
						       NULL);
		}
		gnome_canvas_item_set (tree->priv->drop_highlight,
				       "x1", (double) x,
				       "x2", (double) x + width - 1,
				       "y1", (double) y,
				       "y2", (double) y + height - 1,
				       NULL);
	} else {
		gtk_object_destroy (GTK_OBJECT (tree->priv->drop_highlight));
		tree->priv->drop_highlight = NULL;
	}
}

void
e_tree_drag_unhighlight (ETree *tree)
{
	g_return_if_fail(tree != NULL);
	g_return_if_fail(E_IS_TREE(tree));

	if (tree->priv->drop_highlight) {
		gtk_object_destroy (GTK_OBJECT (tree->priv->drop_highlight));
		tree->priv->drop_highlight = NULL;
	}
}

void e_tree_drag_dest_set   (ETree               *tree,
			     GtkDestDefaults       flags,
			     const GtkTargetEntry *targets,
			     gint                  n_targets,
			     GdkDragAction         actions)
{
	g_return_if_fail(tree != NULL);
	g_return_if_fail(E_IS_TREE(tree));

	gtk_drag_dest_set(GTK_WIDGET(tree),
			  flags,
			  targets,
			  n_targets,
			  actions);
}

void e_tree_drag_dest_set_proxy (ETree         *tree,
				 GdkWindow      *proxy_window,
				 GdkDragProtocol protocol,
				 gboolean        use_coordinates)
{
	g_return_if_fail(tree != NULL);
	g_return_if_fail(E_IS_TREE(tree));

	gtk_drag_dest_set_proxy(GTK_WIDGET(tree),
				proxy_window,
				protocol,
				use_coordinates);
}

/*
 * There probably should be functions for setting the targets
 * as a GtkTargetList
 */

void
e_tree_drag_dest_unset (GtkWidget *widget)
{
	g_return_if_fail(widget != NULL);
	g_return_if_fail(E_IS_TREE(widget));

	gtk_drag_dest_unset(widget);
}

/* Source side */

static gint
et_real_start_drag (ETree *tree, int row, ETreePath path, int col, GdkEvent *event)
{
	GtkDragSourceInfo *info;
	GdkDragContext *context;
	ETreeDragSourceSite *site;

	if (tree->priv->do_drag) {
		site = tree->priv->site;

		site->state = 0;
		context = e_tree_drag_begin (tree, row, col,
					     site->target_list,
					     site->actions,
					     1, event);

		if (context) {
			info = g_dataset_get_data (context, "gtk-info");

			if (info && !info->icon_window) {
				if (site->pixmap)
					gtk_drag_set_icon_pixmap (context,
								  site->colormap,
								  site->pixmap,
								  site->mask, -2, -2);
				else
					gtk_drag_set_icon_default (context);
			}
		}
		return TRUE;
	}
	return FALSE;
}

void
e_tree_drag_source_set  (ETree               *tree,
			 GdkModifierType       start_button_mask,
			 const GtkTargetEntry *targets,
			 gint                  n_targets,
			 GdkDragAction         actions)
{
	ETreeDragSourceSite *site;
	GtkWidget *canvas;

	g_return_if_fail(tree != NULL);
	g_return_if_fail(E_IS_TREE(tree));

	canvas = GTK_WIDGET(tree->priv->table_canvas);
	site = tree->priv->site;

	tree->priv->do_drag = TRUE;

	gtk_widget_add_events (canvas,
			       gtk_widget_get_events (canvas) |
			       GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			       GDK_BUTTON_MOTION_MASK | GDK_STRUCTURE_MASK);

	if (site) {
		if (site->target_list)
			gtk_target_list_unref (site->target_list);
	} else {
		site = g_new0 (ETreeDragSourceSite, 1);
		tree->priv->site = site;
	}

	site->start_button_mask = start_button_mask;

	if (targets)
		site->target_list = gtk_target_list_new (targets, n_targets);
	else
		site->target_list = NULL;

	site->actions = actions;
}

void
e_tree_drag_source_unset (ETree *tree)
{
	ETreeDragSourceSite *site;

	g_return_if_fail (tree != NULL);
	g_return_if_fail (E_IS_TREE(tree));

	site = tree->priv->site;

	if (site) {
		g_free (site);
		tree->priv->site = NULL;
	}
}

/* There probably should be functions for setting the targets
 * as a GtkTargetList
 */

GdkDragContext *
e_tree_drag_begin (ETree            *tree,
		   int     	       row,
		   int     	       col,
		   GtkTargetList     *targets,
		   GdkDragAction      actions,
		   gint               button,
		   GdkEvent          *event)
{
	ETreePath path;
	g_return_val_if_fail (tree != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE(tree), NULL);

	path = e_tree_table_adapter_node_at_row(tree->priv->etta, row);
	path = e_tree_sorted_view_to_model_path(tree->priv->sorted, path);

	tree->priv->drag_row = row;
	tree->priv->drag_path = path;
	tree->priv->drag_col = col;

	return gtk_drag_begin(GTK_WIDGET (tree->priv->table_canvas),
			      targets,
			      actions,
			      button,
			      event);
}

/**
 * e_tree_get_cell_at:
 * @tree: An ETree widget
 * @x: X coordinate for the pixel
 * @y: Y coordinate for the pixel
 * @row_return: Pointer to return the row value
 * @col_return: Pointer to return the column value
 * 
 * Return the row and column for the cell in which the pixel at (@x, @y) is
 * contained.
 **/
void
e_tree_get_cell_at (ETree *tree,
		     int x, int y,
		     int *row_return, int *col_return)
{
	g_return_if_fail (tree != NULL);
	g_return_if_fail (E_IS_TREE (tree));
	g_return_if_fail (row_return != NULL);
	g_return_if_fail (col_return != NULL);

	/* FIXME it would be nice if it could handle a NULL row_return or
	 * col_return gracefully.  */

	if (row_return)
		*row_return = -1;
	if (col_return)
		*col_return = -1;

	x += GTK_LAYOUT(tree->priv->table_canvas)->hadjustment->value;
	y += GTK_LAYOUT(tree->priv->table_canvas)->vadjustment->value;
	e_table_item_compute_location(E_TABLE_ITEM(tree->priv->item), &x, &y, row_return, col_return);
}

/**
 * e_tree_get_cell_geometry:
 * @tree: The tree.
 * @row: The row to get the geometry of.
 * @col: The col to get the geometry of.
 * @x_return: Returns the x coordinate of the upper right hand corner of the cell with respect to the widget.
 * @y_return: Returns the y coordinate of the upper right hand corner of the cell with respect to the widget.
 * @width_return: Returns the width of the cell.
 * @height_return: Returns the height of the cell.
 * 
 * Computes the data about this cell.
 **/
void
e_tree_get_cell_geometry (ETree *tree,
			  int row, int col,
			  int *x_return, int *y_return,
			  int *width_return, int *height_return)
{
	g_return_if_fail (tree != NULL);
	g_return_if_fail (E_IS_TREE (tree));
	g_return_if_fail (row >= 0);
	g_return_if_fail (col >= 0);

	/* FIXME it would be nice if it could handle a NULL row_return or
	 * col_return gracefully.  */

	e_table_item_get_cell_geometry(E_TABLE_ITEM(tree->priv->item), &row, &col, x_return, y_return, width_return, height_return);

	if (x_return)
		(*x_return) -= GTK_LAYOUT(tree->priv->table_canvas)->hadjustment->value;
	if (y_return)
		(*y_return) -= GTK_LAYOUT(tree->priv->table_canvas)->vadjustment->value;
}

static void
et_drag_begin (GtkWidget *widget,
	       GdkDragContext *context,
	       ETree *et)
{
	gtk_signal_emit (GTK_OBJECT (et),
			 et_signals [TREE_DRAG_BEGIN],
			 et->priv->drag_row,
			 et->priv->drag_path,
			 et->priv->drag_col,
			 context);
}

static void
et_drag_end (GtkWidget *widget,
	     GdkDragContext *context,
	     ETree *et)
{
	gtk_signal_emit (GTK_OBJECT (et),
			 et_signals [TREE_DRAG_END],
			 et->priv->drag_row,
			 et->priv->drag_path,
			 et->priv->drag_col,
			 context);
}

static void
et_drag_data_get(GtkWidget *widget,
		 GdkDragContext *context,
		 GtkSelectionData *selection_data,
		 guint info,
		 guint time,
		 ETree *et)
{
	gtk_signal_emit (GTK_OBJECT (et),
			 et_signals [TREE_DRAG_DATA_GET],
			 et->priv->drag_row,
			 et->priv->drag_path,
			 et->priv->drag_col,
			 context,
			 selection_data,
			 info,
			 time);
}

static void
et_drag_data_delete(GtkWidget *widget,
		    GdkDragContext *context,
		    ETree *et)
{
	gtk_signal_emit (GTK_OBJECT (et),
			 et_signals [TREE_DRAG_DATA_DELETE],
			 et->priv->drag_row,
			 et->priv->drag_path,
			 et->priv->drag_col,
			 context);
}

static gboolean
do_drag_motion(ETree *et,
	       GdkDragContext *context,
	       gint x,
	       gint y,
	       guint time)
{
	gboolean ret_val = FALSE;
	int row, col;
	ETreePath path;
	GtkWidget *widget;

	widget = GTK_WIDGET (et);

	x -= widget->allocation.x;
	y -= widget->allocation.y;
	e_tree_get_cell_at (et,
			    x,
			    y,
			    &row,
			    &col);
	if (row != et->priv->drop_row && col != et->priv->drop_col) {
		gtk_signal_emit (GTK_OBJECT (et),
				 et_signals [TREE_DRAG_LEAVE],
				 et->priv->drop_row,
				 et->priv->drop_path,
				 et->priv->drop_col,
				 context,
				 time);
	}

	path = e_tree_table_adapter_node_at_row(et->priv->etta, row);
	path = e_tree_sorted_view_to_model_path(et->priv->sorted, path);

	et->priv->drop_row = row;
	et->priv->drop_path = path;
	et->priv->drop_col = col;
	gtk_signal_emit (GTK_OBJECT (et),
			 et_signals [TREE_DRAG_MOTION],
			 et->priv->drop_row,
			 et->priv->drop_path,
			 et->priv->drop_col,
			 context,
			 x,
			 y,
			 time,
			 &ret_val);

	return ret_val;
}

static gboolean
scroll_timeout (gpointer data)
{
	ETree *et = data;
	int dy;
	GtkAdjustment *v;
	double value;

	if (et->priv->scroll_down)
		dy = 20;
	else
		dy = -20;

	v = GTK_LAYOUT(et->priv->table_canvas)->vadjustment;

	value = v->value;

	gtk_adjustment_set_value(v, CLAMP(v->value + dy, v->lower, v->upper - v->page_size));

	if (v->value != value)
		do_drag_motion(et,
			       et->priv->last_drop_context,
			       et->priv->last_drop_x,
			       et->priv->last_drop_y,
			       et->priv->last_drop_time);
			       

	return TRUE;
}

static void
scroll_on (ETree *et, gboolean down)
{
	if (et->priv->scroll_idle_id == 0 || down != et->priv->scroll_down) {
		if (et->priv->scroll_idle_id != 0)
			g_source_remove (et->priv->scroll_idle_id);
		et->priv->scroll_down = down;
		et->priv->scroll_idle_id = g_timeout_add (100, scroll_timeout, et);
	}
}

static void
scroll_off (ETree *et)
{
	if (et->priv->scroll_idle_id) {
		g_source_remove (et->priv->scroll_idle_id);
		et->priv->scroll_idle_id = 0;
	}
}
 
static gboolean
hover_timeout (gpointer data)
{
	ETree *et = data;
	GtkWidget *widget = data;
	int x = et->priv->hover_x;
	int y = et->priv->hover_y;
	int row, col;
	ETreePath path;

	x -= widget->allocation.x;
	y -= widget->allocation.y;
	e_tree_get_cell_at (et,
			    x,
			    y,
			    &row,
			    &col);

	path = e_tree_table_adapter_node_at_row(et->priv->etta, row);
	if (path && e_tree_model_node_is_expandable (E_TREE_MODEL (et->priv->sorted), path)) {
		e_tree_table_adapter_node_set_expanded (et->priv->etta, path, TRUE);
	}

	return TRUE;
}

static void
hover_on (ETree *et, int x, int y)
{
	et->priv->hover_x = x;
	et->priv->hover_y = y;
	if (et->priv->hover_idle_id != 0)
		g_source_remove (et->priv->hover_idle_id);
	et->priv->hover_idle_id = g_timeout_add (500, hover_timeout, et);
}

static void
hover_off (ETree *et)
{
	if (et->priv->hover_idle_id) {
		g_source_remove (et->priv->hover_idle_id);
		et->priv->hover_idle_id = 0;
	}
}

static void
context_destroyed (gpointer data)
{
	ETree *et = data;
	if (et->priv) {
		et->priv->last_drop_x       = 0;
		et->priv->last_drop_y       = 0;
		et->priv->last_drop_time    = 0;
		et->priv->last_drop_context = NULL;
		scroll_off (et);
		hover_off (et);
	}
	gtk_object_unref (GTK_OBJECT (et));
}

static void
context_connect (ETree *et, GdkDragContext *context)
{
	if (g_dataset_get_data (context, "e-tree") == NULL) {
		gtk_object_ref (GTK_OBJECT (et));
		g_dataset_set_data_full (context, "e-tree", et, context_destroyed);
	}
}

static void
et_drag_leave(GtkWidget *widget,
	      GdkDragContext *context,
	      guint time,
	      ETree *et)
{
	gtk_signal_emit (GTK_OBJECT (et),
			 et_signals [TREE_DRAG_LEAVE],
			 et->priv->drop_row,
			 et->priv->drop_path,
			 et->priv->drop_col,
			 context,
			 time);
	et->priv->drop_row = -1;
	et->priv->drop_col = -1;

	scroll_off (et);
	hover_off (et);
}

static gboolean
et_drag_motion(GtkWidget *widget,
	       GdkDragContext *context,
	       gint x,
	       gint y,
	       guint time,
	       ETree *et)
{
	int ret_val;

	et->priv->last_drop_x = x;
	et->priv->last_drop_y = y;
	et->priv->last_drop_time = time;
	et->priv->last_drop_context = context;
	context_connect (et, context);

	if (et->priv->hover_idle_id != 0) {
		if (abs (et->priv->hover_x - x) > 3 ||
		    abs (et->priv->hover_y - y) > 3) {
			hover_on (et, x, y);
		}
	} else {
		hover_on (et, x, y);
	}

	ret_val = do_drag_motion (et,
				  context,
				  x,
				  y,
				  time);

	x -= widget->allocation.x;
	y -= widget->allocation.y;

	if (y < 20 || y > widget->allocation.height - 20) {
		if (y < 20)
			scroll_on (et, FALSE);
		else
			scroll_on (et, TRUE);
	} else {
		scroll_off (et);
	}

	return ret_val;
}

static gboolean
et_drag_drop(GtkWidget *widget,
	     GdkDragContext *context,
	     gint x,
	     gint y,
	     guint time,
	     ETree *et)
{
	gboolean ret_val = FALSE;
	int row, col;
	ETreePath path;
	y -= widget->allocation.y;
	x -= widget->allocation.x;
	e_tree_get_cell_at(et,
			   x,
			   y,
			   &row,
			   &col);
	path = e_tree_table_adapter_node_at_row(et->priv->etta, row);
	path = e_tree_sorted_view_to_model_path(et->priv->sorted, path);

	if (row != et->priv->drop_row && col != et->priv->drop_row) {
		gtk_signal_emit (GTK_OBJECT (et),
				 et_signals [TREE_DRAG_LEAVE],
				 et->priv->drop_row,
				 et->priv->drop_path,
				 et->priv->drop_col,
				 context,
				 time);
		gtk_signal_emit (GTK_OBJECT (et),
				 et_signals [TREE_DRAG_MOTION],
				 row,
				 path,
				 col,
				 context,
				 x,
				 y,
				 time,
				 &ret_val);
	}
	et->priv->drop_row = row;
	et->priv->drop_path = path;
	et->priv->drop_col = col;
	gtk_signal_emit (GTK_OBJECT (et),
			 et_signals [TREE_DRAG_DROP],
			 et->priv->drop_row,
			 et->priv->drop_path,
			 et->priv->drop_col,
			 context,
			 x,
			 y,
			 time,
			 &ret_val);
	et->priv->drop_row = -1;
	et->priv->drop_path = NULL;
	et->priv->drop_col = -1;

	scroll_off (et);
	return ret_val;
}

static void
et_drag_data_received(GtkWidget *widget,
		      GdkDragContext *context,
		      gint x,
		      gint y,
		      GtkSelectionData *selection_data,
		      guint info,
		      guint time,
		      ETree *et)
{
	int row, col;
	ETreePath path;
	y -= widget->allocation.y;
	x -= widget->allocation.x;
	e_tree_get_cell_at(et,
			   x,
			   y,
			   &row,
			   &col);
	path = e_tree_table_adapter_node_at_row(et->priv->etta, row);
	path = e_tree_sorted_view_to_model_path(et->priv->sorted, path);
	gtk_signal_emit (GTK_OBJECT (et),
			 et_signals [TREE_DRAG_DATA_RECEIVED],
			 row,
			 path,
			 col,
			 context,
			 x,
			 y,
			 selection_data,
			 info,
			 time);
}

static void
e_tree_class_init (ETreeClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkContainerClass *container_class;

	object_class                   = (GtkObjectClass *) class;
	widget_class                   = (GtkWidgetClass *) class;
	container_class                = (GtkContainerClass *) class;

	parent_class                   = gtk_type_class (PARENT_TYPE);

	object_class->destroy          = et_destroy;
	object_class->set_arg          = et_set_arg;
	object_class->get_arg          = et_get_arg;

	widget_class->grab_focus       = et_grab_focus;

	container_class->focus         = et_focus;

	class->cursor_change           = NULL;
	class->cursor_activated        = NULL;
	class->selection_change        = NULL;
	class->double_click            = NULL;
	class->right_click             = NULL;
	class->click                   = NULL;
	class->key_press               = NULL;
	class->start_drag              = et_real_start_drag;

	class->tree_drag_begin         = NULL;
	class->tree_drag_end           = NULL;
	class->tree_drag_data_get      = NULL;
	class->tree_drag_data_delete   = NULL;

	class->tree_drag_leave         = NULL;
	class->tree_drag_motion        = NULL;
	class->tree_drag_drop          = NULL;
	class->tree_drag_data_received = NULL;

	et_signals [CURSOR_CHANGE] =
		gtk_signal_new ("cursor_change",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ETreeClass, cursor_change),
				gtk_marshal_NONE__INT_POINTER,
				GTK_TYPE_NONE, 2, GTK_TYPE_INT, GTK_TYPE_POINTER);

	et_signals [CURSOR_ACTIVATED] =
		gtk_signal_new ("cursor_activated",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ETreeClass, cursor_activated),
				gtk_marshal_NONE__INT_POINTER,
				GTK_TYPE_NONE, 2, GTK_TYPE_INT, GTK_TYPE_POINTER);

	et_signals [SELECTION_CHANGE] =
		gtk_signal_new ("selection_change",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ETreeClass, selection_change),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	et_signals [DOUBLE_CLICK] =
		gtk_signal_new ("double_click",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ETreeClass, double_click),
				e_marshal_NONE__INT_POINTER_INT_POINTER,
				GTK_TYPE_NONE, 4, GTK_TYPE_INT, GTK_TYPE_POINTER, GTK_TYPE_INT, GTK_TYPE_GDK_EVENT);

	et_signals [RIGHT_CLICK] =
		gtk_signal_new ("right_click",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ETreeClass, right_click),
				e_marshal_INT__INT_POINTER_INT_POINTER,
				GTK_TYPE_INT, 4, GTK_TYPE_INT, GTK_TYPE_POINTER, GTK_TYPE_INT, GTK_TYPE_GDK_EVENT);

	et_signals [CLICK] =
		gtk_signal_new ("click",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ETreeClass, click),
				e_marshal_INT__INT_POINTER_INT_POINTER,
				GTK_TYPE_INT, 4, GTK_TYPE_INT, GTK_TYPE_POINTER, GTK_TYPE_INT, GTK_TYPE_GDK_EVENT);

	et_signals [KEY_PRESS] =
		gtk_signal_new ("key_press",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ETreeClass, key_press),
				e_marshal_INT__INT_POINTER_INT_POINTER,
				GTK_TYPE_INT, 4, GTK_TYPE_INT, GTK_TYPE_POINTER, GTK_TYPE_INT, GTK_TYPE_GDK_EVENT);

	et_signals [START_DRAG] =
		gtk_signal_new ("start_drag",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ETreeClass, start_drag),
				e_marshal_NONE__INT_POINTER_INT_POINTER,
				GTK_TYPE_NONE, 4, GTK_TYPE_INT, GTK_TYPE_POINTER, GTK_TYPE_INT, GTK_TYPE_GDK_EVENT);

	et_signals[TREE_DRAG_BEGIN] =
		gtk_signal_new ("tree_drag_begin",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ETreeClass, tree_drag_begin),
				e_marshal_NONE__INT_POINTER_INT_POINTER,
				GTK_TYPE_NONE, 4,
				GTK_TYPE_INT,
				GTK_TYPE_POINTER,
				GTK_TYPE_INT,
				GTK_TYPE_GDK_DRAG_CONTEXT);
	et_signals[TREE_DRAG_END] =
		gtk_signal_new ("tree_drag_end",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ETreeClass, tree_drag_end),
				e_marshal_NONE__INT_POINTER_INT_POINTER,
				GTK_TYPE_NONE, 4,
				GTK_TYPE_INT,
				GTK_TYPE_POINTER,
				GTK_TYPE_INT,
				GTK_TYPE_GDK_DRAG_CONTEXT);
	et_signals[TREE_DRAG_DATA_GET] =
		gtk_signal_new ("tree_drag_data_get",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ETreeClass, tree_drag_data_get),
				e_marshal_NONE__INT_POINTER_INT_POINTER_POINTER_UINT_UINT,
				GTK_TYPE_NONE, 7,
				GTK_TYPE_INT,
				GTK_TYPE_POINTER,
				GTK_TYPE_INT,
				GTK_TYPE_GDK_DRAG_CONTEXT,
				GTK_TYPE_SELECTION_DATA,
				GTK_TYPE_UINT,
				GTK_TYPE_UINT);
	et_signals[TREE_DRAG_DATA_DELETE] =
		gtk_signal_new ("tree_drag_data_delete",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ETreeClass, tree_drag_data_delete),
				e_marshal_NONE__INT_POINTER_INT_POINTER,
				GTK_TYPE_NONE, 4,
				GTK_TYPE_INT,
				GTK_TYPE_POINTER,
				GTK_TYPE_INT,
				GTK_TYPE_GDK_DRAG_CONTEXT);

	et_signals[TREE_DRAG_LEAVE] =
		gtk_signal_new ("tree_drag_leave",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ETreeClass, tree_drag_leave),
				e_marshal_NONE__INT_POINTER_INT_POINTER_UINT,
				GTK_TYPE_NONE, 5,
				GTK_TYPE_INT,
				GTK_TYPE_POINTER,
				GTK_TYPE_INT,
				GTK_TYPE_GDK_DRAG_CONTEXT,
				GTK_TYPE_UINT);
	et_signals[TREE_DRAG_MOTION] =
		gtk_signal_new ("tree_drag_motion",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ETreeClass, tree_drag_motion),
				e_marshal_BOOL__INT_POINTER_INT_POINTER_INT_INT_UINT,
				GTK_TYPE_BOOL, 7,
				GTK_TYPE_INT,
				GTK_TYPE_POINTER,
				GTK_TYPE_INT,
				GTK_TYPE_GDK_DRAG_CONTEXT,
				GTK_TYPE_INT,
				GTK_TYPE_INT,
				GTK_TYPE_UINT);
	et_signals[TREE_DRAG_DROP] =
		gtk_signal_new ("tree_drag_drop",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ETreeClass, tree_drag_drop),
				e_marshal_BOOL__INT_POINTER_INT_POINTER_INT_INT_UINT,
				GTK_TYPE_BOOL, 7,
				GTK_TYPE_INT,
				GTK_TYPE_POINTER,
				GTK_TYPE_INT,
				GTK_TYPE_GDK_DRAG_CONTEXT,
				GTK_TYPE_INT,
				GTK_TYPE_INT,
				GTK_TYPE_UINT);
	et_signals[TREE_DRAG_DATA_RECEIVED] =
		gtk_signal_new ("tree_drag_data_received",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ETreeClass, tree_drag_data_received),
				e_marshal_NONE__INT_POINTER_INT_POINTER_INT_INT_POINTER_UINT_UINT,
				GTK_TYPE_NONE, 9,
				GTK_TYPE_INT,
				GTK_TYPE_POINTER,
				GTK_TYPE_INT,
				GTK_TYPE_GDK_DRAG_CONTEXT,
				GTK_TYPE_INT,
				GTK_TYPE_INT,
				GTK_TYPE_SELECTION_DATA,
				GTK_TYPE_UINT,
				GTK_TYPE_UINT);

	E_OBJECT_CLASS_ADD_SIGNALS (object_class, et_signals, LAST_SIGNAL);

	class->set_scroll_adjustments = set_scroll_adjustments;

	widget_class->set_scroll_adjustments_signal =
		gtk_signal_new ("set_scroll_adjustments",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ETreeClass, set_scroll_adjustments),
				gtk_marshal_NONE__POINTER_POINTER,
				GTK_TYPE_NONE, 2, GTK_TYPE_ADJUSTMENT, GTK_TYPE_ADJUSTMENT);

	gtk_object_add_arg_type ("ETree::length_threshold", GTK_TYPE_INT,
				 GTK_ARG_WRITABLE, ARG_LENGTH_THRESHOLD);
	gtk_object_add_arg_type ("ETree::horizontal_draw_grid", GTK_TYPE_BOOL,
				 GTK_ARG_WRITABLE, ARG_HORIZONTAL_DRAW_GRID);
	gtk_object_add_arg_type ("ETree::vertical_draw_grid", GTK_TYPE_BOOL,
				 GTK_ARG_WRITABLE, ARG_VERTICAL_DRAW_GRID);
	gtk_object_add_arg_type ("ETree::draw_focus", GTK_TYPE_BOOL,
				 GTK_ARG_WRITABLE, ARG_DRAW_FOCUS);
	gtk_object_add_arg_type ("ETree::ETreeTableAdapter", GTK_TYPE_OBJECT,
				 GTK_ARG_READABLE, ARG_ETTA);
	gtk_object_add_arg_type ("ETree::uniform_row_height", GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE, ARG_UNIFORM_ROW_HEIGHT);
}

E_MAKE_TYPE(e_tree, "ETree", ETree, e_tree_class_init, e_tree_init, PARENT_TYPE);
