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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include <libgnomecanvas/libgnomecanvas.h>

#include "e-canvas-background.h"
#include "e-canvas-utils.h"
#include "e-canvas.h"
#include "e-cell-tree.h"
#include "e-table-column-specification.h"
#include "e-table-header-item.h"
#include "e-table-header.h"
#include "e-table-item.h"
#include "e-table-sort-info.h"
#include "e-table-utils.h"
#include "e-text.h"
#include "e-tree-selection-model.h"
#include "e-tree-table-adapter.h"
#include "e-tree.h"
#include "e-misc-utils.h"
#include "gal-a11y-e-tree.h"

#define COLUMN_HEADER_HEIGHT 16

#define d(x)

typedef struct _ETreeDragSourceSite ETreeDragSourceSite;

enum {
	CURSOR_CHANGE,
	CURSOR_ACTIVATED,
	SELECTION_CHANGE,
	DOUBLE_CLICK,
	RIGHT_CLICK,
	CLICK,
	KEY_PRESS,
	START_DRAG,
	STATE_CHANGE,
	WHITE_SPACE_EVENT,

	CUT_CLIPBOARD,
	COPY_CLIPBOARD,
	PASTE_CLIPBOARD,
	SELECT_ALL,

	TREE_DRAG_BEGIN,
	TREE_DRAG_END,
	TREE_DRAG_DATA_GET,
	TREE_DRAG_DATA_DELETE,

	TREE_DRAG_LEAVE,
	TREE_DRAG_MOTION,
	TREE_DRAG_DROP,
	TREE_DRAG_DATA_RECEIVED,

	HEADER_CLICK_CAN_SORT,

	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_LENGTH_THRESHOLD,
	PROP_HORIZONTAL_DRAW_GRID,
	PROP_VERTICAL_DRAW_GRID,
	PROP_DRAW_FOCUS,
	PROP_ETTA,
	PROP_UNIFORM_ROW_HEIGHT,
	PROP_IS_EDITING,
	PROP_ALWAYS_SEARCH,
	PROP_HADJUSTMENT,
	PROP_VADJUSTMENT,
	PROP_HSCROLL_POLICY,
	PROP_VSCROLL_POLICY,
	PROP_SORT_CHILDREN_ASCENDING
};

enum {
	ET_SCROLL_UP = 1 << 0,
	ET_SCROLL_DOWN = 1 << 1,
	ET_SCROLL_LEFT = 1 << 2,
	ET_SCROLL_RIGHT = 1 << 3
};

struct _ETreePrivate {
	ETreeModel *model;
	ETreeTableAdapter *etta;

	ETableHeader *full_header, *header;

	guint structure_change_id, expansion_change_id;

	ETableSortInfo *sort_info;

	guint sort_info_change_id, group_info_change_id;

	ESelectionModel *selection;
	ETableSpecification *spec;

	ETableSearch     *search;

	ETableCol        *current_search_col;

	guint	  search_search_id;
	guint	  search_accept_id;

	gint reflow_idle_id;
	gint scroll_idle_id;
	gint hover_idle_id;

	gboolean show_cursor_after_reflow;

	gint table_model_change_id;
	gint table_row_change_id;
	gint table_cell_change_id;
	gint table_rows_delete_id;

	GnomeCanvasItem *info_text;
	guint info_text_resize_id;

	GnomeCanvas *header_canvas, *table_canvas;

	GnomeCanvasItem *header_item, *root;

	GnomeCanvasItem *white_item;
	GnomeCanvasItem *item;

	gint length_threshold;

	GtkAdjustment *table_canvas_vadjustment;

	/*
	 * Configuration settings
	 */
	guint alternating_row_colors : 1;
	guint horizontal_draw_grid : 1;
	guint vertical_draw_grid : 1;
	guint draw_focus : 1;
	guint row_selection_active : 1;

	guint horizontal_scrolling : 1;

	guint scroll_direction : 4;

	guint do_drag : 1;

	guint uniform_row_height : 1;

	guint search_col_set : 1;
	guint always_search : 1;

	ECursorMode cursor_mode;

	gint drop_row;
	ETreePath drop_path;
	gint drop_col;

	GnomeCanvasItem *drop_highlight;
	gint last_drop_x;
	gint last_drop_y;
	gint last_drop_time;
	GdkDragContext *last_drop_context;

	gint hover_x;
	gint hover_y;

	gint drag_row;
	ETreePath drag_path;
	gint drag_col;
	ETreeDragSourceSite *site;

	GList *expanded_list;

	gboolean state_changed;
	guint state_change_freeze;

	gboolean is_dragging;

	gboolean grouped_view;
	gboolean sort_children_ascending;
};

static guint signals[LAST_SIGNAL];

static void et_grab_focus (GtkWidget *widget);

static void et_drag_begin (GtkWidget *widget,
			   GdkDragContext *context,
			   ETree *tree);
static void et_drag_end (GtkWidget *widget,
			 GdkDragContext *context,
			 ETree *tree);
static void et_drag_data_get (GtkWidget *widget,
			     GdkDragContext *context,
			     GtkSelectionData *selection_data,
			     guint info,
			     guint time,
			     ETree *tree);
static void et_drag_data_delete (GtkWidget *widget,
				GdkDragContext *context,
				ETree *tree);

static void et_drag_leave (GtkWidget *widget,
			  GdkDragContext *context,
			  guint time,
			  ETree *tree);
static gboolean et_drag_motion (GtkWidget *widget,
			       GdkDragContext *context,
			       gint x,
			       gint y,
			       guint time,
			       ETree *tree);
static gboolean et_drag_drop (GtkWidget *widget,
			     GdkDragContext *context,
			     gint x,
			     gint y,
			     guint time,
			     ETree *tree);
static void et_drag_data_received (GtkWidget *widget,
				  GdkDragContext *context,
				  gint x,
				  gint y,
				  GtkSelectionData *selection_data,
				  guint info,
				  guint time,
				  ETree *tree);

static void scroll_off (ETree *tree);
static void scroll_on (ETree *tree, guint scroll_direction);
static void hover_off (ETree *tree);
static void hover_on (ETree *tree, gint x, gint y);
static void context_destroyed (gpointer data, GObject *ctx);

static void e_tree_scrollable_init (GtkScrollableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ETree, e_tree, GTK_TYPE_GRID,
	G_ADD_PRIVATE (ETree)
	G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, e_tree_scrollable_init))

static void
tree_item_is_editing_changed_cb (ETableItem *item,
                                 GParamSpec *param,
                                 ETree *tree)
{
	g_return_if_fail (E_IS_TREE (tree));

	g_object_notify (G_OBJECT (tree), "is-editing");
}

static void
et_disconnect_from_etta (ETree *tree)
{
	if (tree->priv->table_model_change_id != 0)
		g_signal_handler_disconnect (
			tree->priv->etta,
			tree->priv->table_model_change_id);
	if (tree->priv->table_row_change_id != 0)
		g_signal_handler_disconnect (
			tree->priv->etta,
			tree->priv->table_row_change_id);
	if (tree->priv->table_cell_change_id != 0)
		g_signal_handler_disconnect (
			tree->priv->etta,
			tree->priv->table_cell_change_id);
	if (tree->priv->table_rows_delete_id != 0)
		g_signal_handler_disconnect (
			tree->priv->etta,
			tree->priv->table_rows_delete_id);

	tree->priv->table_model_change_id = 0;
	tree->priv->table_row_change_id = 0;
	tree->priv->table_cell_change_id = 0;
	tree->priv->table_rows_delete_id = 0;
}

static void
clear_current_search_col (ETree *tree)
{
	tree->priv->search_col_set = FALSE;
}

static ETableCol *
current_search_col (ETree *tree)
{
	if (!tree->priv->search_col_set) {
		tree->priv->current_search_col =
			e_table_util_calculate_current_search_col (
				tree->priv->header,
				tree->priv->full_header,
				tree->priv->sort_info,
				tree->priv->always_search);
		tree->priv->search_col_set = TRUE;
	}

	return tree->priv->current_search_col;
}

static void
e_tree_state_change (ETree *tree)
{
	if (tree->priv->state_change_freeze)
		tree->priv->state_changed = TRUE;
	else
		g_signal_emit (tree, signals[STATE_CHANGE], 0);
}

static void
change_trigger (GObject *object,
                ETree *tree)
{
	e_tree_state_change (tree);
}

static void
search_col_change_trigger (GObject *object,
                           ETree *tree)
{
	clear_current_search_col (tree);
	e_tree_state_change (tree);
}

static void
disconnect_header (ETree *tree)
{
	if (tree->priv->header == NULL)
		return;

	if (tree->priv->structure_change_id)
		g_signal_handler_disconnect (
			tree->priv->header,
			tree->priv->structure_change_id);
	if (tree->priv->expansion_change_id)
		g_signal_handler_disconnect (
			tree->priv->header,
			tree->priv->expansion_change_id);
	if (tree->priv->sort_info) {
		if (tree->priv->sort_info_change_id)
			g_signal_handler_disconnect (
				tree->priv->sort_info,
				tree->priv->sort_info_change_id);
		if (tree->priv->group_info_change_id)
			g_signal_handler_disconnect (
				tree->priv->sort_info,
				tree->priv->group_info_change_id);

		g_object_unref (tree->priv->sort_info);
	}
	g_object_unref (tree->priv->header);
	tree->priv->header = NULL;
	tree->priv->sort_info = NULL;
}

static void
connect_header (ETree *tree,
                ETableState *state)
{
	GValue *val = g_new0 (GValue, 1);

	if (tree->priv->header != NULL)
		disconnect_header (tree);

	tree->priv->header = e_table_state_to_header (
		GTK_WIDGET (tree), tree->priv->full_header, state);

	tree->priv->structure_change_id = g_signal_connect (
		tree->priv->header, "structure_change",
		G_CALLBACK (search_col_change_trigger), tree);

	tree->priv->expansion_change_id = g_signal_connect (
		tree->priv->header, "expansion_change",
		G_CALLBACK (change_trigger), tree);

	if (state->sort_info) {
		tree->priv->sort_info = e_table_sort_info_duplicate (state->sort_info);
		e_table_sort_info_set_can_group (tree->priv->sort_info, FALSE);
		tree->priv->sort_info_change_id = g_signal_connect (
			tree->priv->sort_info, "sort_info_changed",
			G_CALLBACK (search_col_change_trigger), tree);

		tree->priv->group_info_change_id = g_signal_connect (
			tree->priv->sort_info, "group_info_changed",
			G_CALLBACK (search_col_change_trigger), tree);
	} else
		tree->priv->sort_info = NULL;

	g_value_init (val, G_TYPE_OBJECT);
	g_value_set_object (val, tree->priv->sort_info);
	g_object_set_property (G_OBJECT (tree->priv->header), "sort_info", val);
	g_free (val);
}

static void
et_dispose (GObject *object)
{
	ETree *self = E_TREE (object);

	if (self->priv->search != NULL) {
		g_signal_handler_disconnect (self->priv->search, self->priv->search_search_id);
		g_signal_handler_disconnect (self->priv->search, self->priv->search_accept_id);
		g_clear_object (&self->priv->search);
	}

	if (self->priv->reflow_idle_id > 0) {
		g_source_remove (self->priv->reflow_idle_id);
		self->priv->reflow_idle_id = 0;
	}

	scroll_off (self);
	hover_off (self);
	g_list_foreach (self->priv->expanded_list, (GFunc) g_free, NULL);
	g_list_free (self->priv->expanded_list);
	self->priv->expanded_list = NULL;

	et_disconnect_from_etta (self);

	g_clear_object (&self->priv->etta);
	g_clear_object (&self->priv->model);
	g_clear_object (&self->priv->full_header);

	disconnect_header (self);

	g_clear_object (&self->priv->selection);
	g_clear_object (&self->priv->spec);

	if (self->priv->header_canvas != NULL) {
		gtk_widget_destroy (GTK_WIDGET (self->priv->header_canvas));
		self->priv->header_canvas = NULL;
	}

	if (self->priv->site)
		e_tree_drag_source_unset (self);

	if (self->priv->last_drop_context != NULL) {
		g_object_weak_unref (
			G_OBJECT (self->priv->last_drop_context),
			context_destroyed, object);
		self->priv->last_drop_context = NULL;
	}

	if (self->priv->info_text != NULL) {
		g_object_run_dispose (G_OBJECT (self->priv->info_text));
		self->priv->info_text = NULL;
	}
	self->priv->info_text_resize_id = 0;

	if (self->priv->table_canvas != NULL) {
		g_signal_handlers_disconnect_by_data (self->priv->table_canvas, object);
		gtk_widget_destroy (GTK_WIDGET (self->priv->table_canvas));
		self->priv->table_canvas = NULL;
	}

	if (self->priv->table_canvas_vadjustment) {
		g_signal_handlers_disconnect_by_data (self->priv->table_canvas_vadjustment, object);
		g_clear_object (&self->priv->table_canvas_vadjustment);
	}

	/* do not unref it, it was owned by priv->table_canvas */
	self->priv->item = NULL;

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_tree_parent_class)->dispose (object);
}

static void
et_unrealize (GtkWidget *widget)
{
	scroll_off (E_TREE (widget));
	hover_off (E_TREE (widget));

	if (GTK_WIDGET_CLASS (e_tree_parent_class)->unrealize)
		GTK_WIDGET_CLASS (e_tree_parent_class)->unrealize (widget);
}

typedef struct {
	ETree *tree;
	gchar *string;
} SearchSearchStruct;

static gboolean
search_search_callback (ETreeModel *model,
                        ETreePath path,
                        gpointer data)
{
	SearchSearchStruct *cb_data = data;
	gconstpointer value;
	ETableCol *col = current_search_col (cb_data->tree);

	value = e_tree_model_value_at (
		model, path,
		cb_data->tree->priv->current_search_col->spec->model_col);

	return col->search (value, cb_data->string);
}

static gboolean
et_search_search (ETableSearch *search,
                  gchar *string,
                  ETableSearchFlags flags,
                  ETree *tree)
{
	ETreePath cursor;
	ETreePath found;
	SearchSearchStruct cb_data;
	ETableCol *col = current_search_col (tree);

	if (col == NULL)
		return FALSE;

	cb_data.tree = tree;
	cb_data.string = string;

	cursor = e_tree_get_cursor (tree);

	if (cursor && (flags & E_TABLE_SEARCH_FLAGS_CHECK_CURSOR_FIRST)) {
		gconstpointer value;

		value = e_tree_model_value_at (
			tree->priv->model, cursor, col->spec->model_col);

		if (col->search (value, string)) {
			return TRUE;
		}
	}

	found = e_tree_model_node_find (
		tree->priv->model, cursor, NULL,
		search_search_callback, &cb_data);
	if (found == NULL)
		found = e_tree_model_node_find (
			tree->priv->model, NULL, cursor,
			search_search_callback, &cb_data);

	if (found && found != cursor) {
		gint model_row;

		e_tree_table_adapter_show_node (tree->priv->etta, found);
		model_row = e_tree_table_adapter_row_of_node (tree->priv->etta, found);

		e_selection_model_select_as_key_press (
			E_SELECTION_MODEL (tree->priv->selection),
			model_row, col->spec->model_col,
			GDK_CONTROL_MASK);
		return TRUE;
	} else if (cursor && !(flags & E_TABLE_SEARCH_FLAGS_CHECK_CURSOR_FIRST)) {
		gconstpointer value;

		value = e_tree_model_value_at (
			tree->priv->model, cursor, col->spec->model_col);

		return col->search (value, string);
	} else
		return FALSE;
}

static void
et_search_accept (ETableSearch *search,
                  ETree *tree)
{
	ETableCol *col = current_search_col (tree);
	gint cursor;

	if (col == NULL)
		return;

	g_object_get (tree->priv->selection, "cursor_row", &cursor, NULL);

	e_selection_model_select_as_key_press (
		E_SELECTION_MODEL (tree->priv->selection),
		cursor, col->spec->model_col, 0);
}

static void
e_tree_init (ETree *tree)
{
	gtk_widget_set_can_focus (GTK_WIDGET (tree), TRUE);

	tree->priv = e_tree_get_instance_private (tree);

	tree->priv->alternating_row_colors = 1;
	tree->priv->horizontal_draw_grid = 1;
	tree->priv->vertical_draw_grid = 1;
	tree->priv->draw_focus = 1;
	tree->priv->cursor_mode = E_CURSOR_SIMPLE;
	tree->priv->length_threshold = 200;

	tree->priv->drop_row = -1;
	tree->priv->drop_col = -1;

	tree->priv->drag_row = -1;
	tree->priv->drag_col = -1;

	tree->priv->selection =
		E_SELECTION_MODEL (e_tree_selection_model_new ());

	tree->priv->search = e_table_search_new ();

	tree->priv->search_search_id = g_signal_connect (
		tree->priv->search, "search",
		G_CALLBACK (et_search_search), tree);

	tree->priv->search_accept_id = g_signal_connect (
		tree->priv->search, "accept",
		G_CALLBACK (et_search_accept), tree);

	tree->priv->always_search = g_getenv ("GAL_ALWAYS_SEARCH") ? TRUE : FALSE;

	tree->priv->state_changed = FALSE;
	tree->priv->state_change_freeze = 0;

	tree->priv->is_dragging = FALSE;
	tree->priv->grouped_view = TRUE;
}

/* Grab_focus handler for the ETree */
static void
et_grab_focus (GtkWidget *widget)
{
	ETree *tree;

	tree = E_TREE (widget);

	gtk_widget_grab_focus (GTK_WIDGET (tree->priv->table_canvas));
}

/* Focus handler for the ETree */
static gint
et_focus (GtkWidget *container,
          GtkDirectionType direction)
{
	ETree *tree;

	tree = E_TREE (container);

	if (gtk_container_get_focus_child (GTK_CONTAINER (container))) {
		gtk_container_set_focus_child (GTK_CONTAINER (container), NULL);
		return FALSE;
	}

	return gtk_widget_child_focus (
		GTK_WIDGET (tree->priv->table_canvas), direction);
}

static void
set_header_canvas_width (ETree *tree)
{
	gdouble oldwidth, oldheight, width;

	if (!(tree->priv->header_item &&
		tree->priv->header_canvas && tree->priv->table_canvas))
		return;

	gnome_canvas_get_scroll_region (
		GNOME_CANVAS (tree->priv->table_canvas),
		NULL, NULL, &width, NULL);
	gnome_canvas_get_scroll_region (
		GNOME_CANVAS (tree->priv->header_canvas),
		NULL, NULL, &oldwidth, &oldheight);

	if (oldwidth != width ||
	    oldheight != E_TABLE_HEADER_ITEM (tree->priv->header_item)->height - 1)
		gnome_canvas_set_scroll_region (
			GNOME_CANVAS (tree->priv->header_canvas),
			0, 0, width, /*  COLUMN_HEADER_HEIGHT - 1 */
			E_TABLE_HEADER_ITEM (tree->priv->header_item)->height - 1);

}

static void
header_canvas_size_allocate (GtkWidget *widget,
                             GtkAllocation *alloc,
                             ETree *tree)
{
	GtkAllocation allocation;

	set_header_canvas_width (tree);

	widget = GTK_WIDGET (tree->priv->header_canvas);
	gtk_widget_get_allocation (widget, &allocation);

	/* When the header item is created ->height == 0,
	 * as the font is only created when everything is realized.
	 * So we set the usize here as well, so that the size of the
	 * header is correct */
	if (allocation.height != E_TABLE_HEADER_ITEM (tree->priv->header_item)->height)
		gtk_widget_set_size_request (
			widget, -1,
			E_TABLE_HEADER_ITEM (tree->priv->header_item)->height);
}

static void
e_tree_header_click_can_sort_cb (ETableHeaderItem *header_item,
				 gboolean *out_header_click_can_sort,
				 gpointer user_data)
{
	ETree *tree = user_data;

	g_signal_emit (tree, signals[HEADER_CLICK_CAN_SORT], 0, out_header_click_can_sort);
}

static void
e_tree_setup_header (ETree *tree)
{
	GtkWidget *widget;
	gchar *pointer;

	widget = e_canvas_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_style_context_add_class (
		gtk_widget_get_style_context (widget), "table-header");
	gtk_widget_set_can_focus (widget, FALSE);
	tree->priv->header_canvas = GNOME_CANVAS (widget);
	gtk_widget_show (widget);

	pointer = g_strdup_printf ("%p", (gpointer) tree);

	tree->priv->header_item = gnome_canvas_item_new (
		gnome_canvas_root (tree->priv->header_canvas),
		e_table_header_item_get_type (),
		"ETableHeader", tree->priv->header,
		"full_header", tree->priv->full_header,
		"sort_info", tree->priv->sort_info,
		"dnd_code", pointer,
		"tree", tree,
		NULL);

	g_free (pointer);

	g_signal_connect_object (tree->priv->header_item, "header-click-can-sort",
		G_CALLBACK (e_tree_header_click_can_sort_cb), tree, 0);

	g_signal_connect (
		tree->priv->header_canvas, "size_allocate",
		G_CALLBACK (header_canvas_size_allocate), tree);

	gtk_widget_set_size_request (
		GTK_WIDGET (tree->priv->header_canvas), -1,
		E_TABLE_HEADER_ITEM (tree->priv->header_item)->height);
}

static void
scroll_to_cursor (ETree *tree)
{
	ETreePath path;
	GtkAdjustment *adjustment;
	GtkScrollable *scrollable;
	gint x, y, w, h;
	gdouble page_size;
	gdouble lower;
	gdouble upper;
	gdouble value;

	path = e_tree_get_cursor (tree);
	x = y = w = h = 0;

	if (path != NULL) {
		ETreeTableAdapter *adapter;
		gint row;
		gint col = 0;

		adapter = e_tree_get_table_adapter (tree);
		row = e_tree_table_adapter_row_of_node (adapter, path);

		if (row >= 0)
			e_table_item_get_cell_geometry (
				E_TABLE_ITEM (tree->priv->item),
				&row, &col, &x, &y, &w, &h);
	}

	e_table_item_cancel_scroll_to_cursor (E_TABLE_ITEM (tree->priv->item));

	scrollable = GTK_SCROLLABLE (tree->priv->table_canvas);
	adjustment = gtk_scrollable_get_vadjustment (scrollable);

	page_size = gtk_adjustment_get_page_size (adjustment);
	lower = gtk_adjustment_get_lower (adjustment);
	upper = gtk_adjustment_get_upper (adjustment);
	value = gtk_adjustment_get_value (adjustment);

	if (y < value || y + h > value + page_size) {
		value = CLAMP (y - page_size / 2, lower, upper - page_size);
		gtk_adjustment_set_value (adjustment, value);
	}
}

static gboolean
tree_canvas_reflow_idle (ETree *tree)
{
	gdouble height, width;
	gdouble oldheight, oldwidth;
	GtkAllocation allocation;
	GtkWidget *widget;

	widget = GTK_WIDGET (tree->priv->table_canvas);
	gtk_widget_get_allocation (widget, &allocation);

	g_object_get (
		tree->priv->item,
		"height", &height, "width", &width, NULL);

	height = MAX ((gint) height, allocation.height);
	width = MAX ((gint) width, allocation.width);

	/* I have no idea why this needs to be -1, but it works. */
	gnome_canvas_get_scroll_region (
		GNOME_CANVAS (tree->priv->table_canvas),
		NULL, NULL, &oldwidth, &oldheight);

	if (oldwidth != width - 1 ||
	    oldheight != height - 1) {
		gnome_canvas_set_scroll_region (
			GNOME_CANVAS (tree->priv->table_canvas),
			0, 0, width - 1, height - 1);
		set_header_canvas_width (tree);
	}

	tree->priv->reflow_idle_id = 0;

	if (tree->priv->show_cursor_after_reflow) {
		tree->priv->show_cursor_after_reflow = FALSE;
		scroll_to_cursor (tree);
	}

	return FALSE;
}

static void
tree_canvas_size_allocate (GtkWidget *widget,
                           GtkAllocation *alloc,
                           ETree *tree)
{
	gdouble width;
	gdouble height;
	GValue *val = g_new0 (GValue, 1);
	g_value_init (val, G_TYPE_DOUBLE);

	width = alloc->width;
	g_value_set_double (val, width);
	g_object_get (
		tree->priv->item,
		"height", &height,
		NULL);
	height = MAX ((gint) height, alloc->height);

	g_object_set (
		tree->priv->item,
		"width", width,
		NULL);
	g_object_set_property (G_OBJECT (tree->priv->header), "width", val);
	g_free (val);

	if (tree->priv->reflow_idle_id)
		g_source_remove (tree->priv->reflow_idle_id);
	tree_canvas_reflow_idle (tree);
}

static void
tree_canvas_reflow (GnomeCanvas *canvas,
                    ETree *tree)
{
	if (!tree->priv->reflow_idle_id)
		tree->priv->reflow_idle_id = g_idle_add_full (
			400, (GSourceFunc) tree_canvas_reflow_idle,
			tree, NULL);
}

static void
item_cursor_change (ETableItem *eti,
                    gint row,
                    ETree *tree)
{
	ETreePath path = e_tree_table_adapter_node_at_row (tree->priv->etta, row);

	g_signal_emit (tree, signals[CURSOR_CHANGE], 0, row, path);
}

static void
item_cursor_activated (ETableItem *eti,
                       gint row,
                       ETree *tree)
{
	ETreePath path = e_tree_table_adapter_node_at_row (tree->priv->etta, row);

	g_signal_emit (tree, signals[CURSOR_ACTIVATED], 0, row, path);
}

static void
item_double_click (ETableItem *eti,
                   gint row,
                   gint col,
                   GdkEvent *event,
                   ETree *tree)
{
	ETreePath path = e_tree_table_adapter_node_at_row (tree->priv->etta, row);

	g_signal_emit (tree, signals[DOUBLE_CLICK], 0, row, path, col, event);
}

static gboolean
item_right_click (ETableItem *eti,
                  gint row,
                  gint col,
                  GdkEvent *event,
                  ETree *tree)
{
	ETreePath path = e_tree_table_adapter_node_at_row (tree->priv->etta, row);
	gboolean return_val = 0;

	g_signal_emit (
		tree, signals[RIGHT_CLICK], 0,
		row, path, col, event, &return_val);

	return return_val;
}

static gboolean
item_click (ETableItem *eti,
            gint row,
            gint col,
            GdkEvent *event,
            ETree *tree)
{
	gboolean return_val = 0;
	ETreePath path = e_tree_table_adapter_node_at_row (tree->priv->etta, row);

	g_signal_emit (
		tree, signals[CLICK], 0, row, path, col, event, &return_val);

	return return_val;
}

static gint
item_key_press (ETableItem *eti,
                gint row,
                gint col,
                GdkEvent *event,
                ETree *tree)
{
	gint return_val = 0;
	GdkEventKey *key = (GdkEventKey *) event;
	ETreePath path;
	gint y, row_local, col_local;
	GtkAdjustment *adjustment;
	GtkScrollable *scrollable;
	gdouble page_size;
	gdouble upper;
	gdouble value;

	scrollable = GTK_SCROLLABLE (tree->priv->table_canvas);
	adjustment = gtk_scrollable_get_vadjustment (scrollable);

	page_size = gtk_adjustment_get_page_size (adjustment);
	upper = gtk_adjustment_get_upper (adjustment);
	value = gtk_adjustment_get_value (adjustment);

	switch (key->keyval) {
	case GDK_KEY_Page_Down:
	case GDK_KEY_KP_Page_Down:
		y = CLAMP (value + (2 * page_size - 50), 0, upper);
		y -= value;
		e_tree_get_cell_at (tree, 30, y, &row_local, &col_local);

		if (row_local == -1)
			row_local = e_table_model_row_count (
				E_TABLE_MODEL (tree->priv->etta)) - 1;

		col_local = e_selection_model_cursor_col (
			E_SELECTION_MODEL (tree->priv->selection));
		e_selection_model_select_as_key_press (
			E_SELECTION_MODEL (tree->priv->selection),
			row_local, col_local, key->state);

		return_val = 1;
		break;
	case GDK_KEY_Page_Up:
	case GDK_KEY_KP_Page_Up:
		y = CLAMP (value - (page_size - 50), 0, upper);
		y -= value;
		e_tree_get_cell_at (tree, 30, y, &row_local, &col_local);

		if (row_local == -1)
			row_local = e_table_model_row_count (
				E_TABLE_MODEL (tree->priv->etta)) - 1;

		col_local = e_selection_model_cursor_col (
			E_SELECTION_MODEL (tree->priv->selection));
		e_selection_model_select_as_key_press (
			E_SELECTION_MODEL (tree->priv->selection),
			row_local, col_local, key->state);

		return_val = 1;
		break;
	case GDK_KEY_plus:
	case GDK_KEY_KP_Add:
	case GDK_KEY_Right:
	case GDK_KEY_KP_Right:
		/* Only allow if the Shift modifier is used.
		 * eg. Ctrl-Equal shouldn't be handled.  */
		if ((key->state & (GDK_SHIFT_MASK | GDK_LOCK_MASK | GDK_MOD1_MASK)) != GDK_SHIFT_MASK) {
			/* Allow also plain (without modifiers) expand when in the 'line' cursor mode */
			if (eti->cursor_mode != E_CURSOR_LINE ||
			    (key->state & (GDK_SHIFT_MASK | GDK_LOCK_MASK | GDK_MOD1_MASK)) != 0)
				break;
		}

		if (row != -1) {
			path = e_tree_table_adapter_node_at_row (tree->priv->etta, row);
			if (path) {
				if ((key->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == (GDK_CONTROL_MASK | GDK_SHIFT_MASK))
					e_tree_table_adapter_node_set_expanded_recurse (tree->priv->etta, path, TRUE);
				else
					e_tree_table_adapter_node_set_expanded (tree->priv->etta, path, TRUE);
			}
		}
		return_val = 1;
		break;
	case GDK_KEY_minus:
	case GDK_KEY_underscore:
	case GDK_KEY_KP_Subtract:
	case GDK_KEY_Left:
	case GDK_KEY_KP_Left:
		/* Only allow if the Shift modifier is used.
		 * eg. Ctrl-Minus shouldn't be handled.  */
		if ((key->state & (GDK_SHIFT_MASK | GDK_LOCK_MASK | GDK_MOD1_MASK)) != GDK_SHIFT_MASK) {
			/* Allow also plain (without modifiers) collapse when in the 'line' cursor mode */
			if (eti->cursor_mode != E_CURSOR_LINE ||
			    (key->state & (GDK_SHIFT_MASK | GDK_LOCK_MASK | GDK_MOD1_MASK)) != 0)
				break;
		}

		if (row != -1) {
			path = e_tree_table_adapter_node_at_row (tree->priv->etta, row);
			if (path) {
				if ((key->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == (GDK_CONTROL_MASK | GDK_SHIFT_MASK))
					e_tree_table_adapter_node_set_expanded_recurse (tree->priv->etta, path, FALSE);
				else
					e_tree_table_adapter_node_set_expanded (tree->priv->etta, path, FALSE);
			}
		}
		return_val = 1;
		break;
	case GDK_KEY_BackSpace:
		if (e_table_search_backspace (tree->priv->search))
			return TRUE;
		/* Fallthrough */
	default:
		if ((key->state & ~(GDK_SHIFT_MASK | GDK_LOCK_MASK |
			GDK_MOD1_MASK | GDK_MOD2_MASK | GDK_MOD3_MASK |
			GDK_MOD4_MASK | GDK_MOD5_MASK)) == 0
		    && ((key->keyval >= GDK_KEY_a && key->keyval <= GDK_KEY_z) ||
			(key->keyval >= GDK_KEY_A && key->keyval <= GDK_KEY_Z) ||
			(key->keyval >= GDK_KEY_0 && key->keyval <= GDK_KEY_9))) {
			e_table_search_input_character (tree->priv->search, key->keyval);
		}
		path = e_tree_table_adapter_node_at_row (tree->priv->etta, row);
		g_signal_emit (
			tree,
			signals[KEY_PRESS], 0,
			row, path, col, event, &return_val);
		break;
	}
	return return_val;
}

static gint
item_start_drag (ETableItem *eti,
                 gint row,
                 gint col,
                 GdkEvent *event,
                 ETree *tree)
{
	ETreePath path;
	gint return_val = 0;

	path = e_tree_table_adapter_node_at_row (tree->priv->etta, row);

	g_signal_emit (
		tree, signals[START_DRAG], 0,
		row, path, col, event, &return_val);

	return return_val;
}

static void
et_selection_model_selection_changed (ETableSelectionModel *etsm,
                                      ETree *tree)
{
	g_signal_emit (tree, signals[SELECTION_CHANGE], 0);
}

static void
et_selection_model_selection_row_changed (ETableSelectionModel *etsm,
                                          gint row,
                                          ETree *tree)
{
	g_signal_emit (tree, signals[SELECTION_CHANGE], 0);
}

static void
et_build_item (ETree *tree)
{
	gboolean alternating_row_colors;

	alternating_row_colors = tree->priv->alternating_row_colors;
	if (alternating_row_colors) {
		gboolean bvalue = TRUE;

		/* user can only disable this option, if it's enabled by the specification */
		gtk_widget_style_get (GTK_WIDGET (tree), "alternating-row-colors", &bvalue, NULL);

		alternating_row_colors = bvalue ? 1 : 0;
	}

	tree->priv->item = gnome_canvas_item_new (
		GNOME_CANVAS_GROUP (
			gnome_canvas_root (tree->priv->table_canvas)),
		e_table_item_get_type (),
		"ETableHeader", tree->priv->header,
		"ETableModel", tree->priv->etta,
		"selection_model", tree->priv->selection,
		"alternating_row_colors", alternating_row_colors,
		"horizontal_draw_grid", tree->priv->horizontal_draw_grid,
		"vertical_draw_grid", tree->priv->vertical_draw_grid,
		"drawfocus", tree->priv->draw_focus,
		"cursor_mode", tree->priv->cursor_mode,
		"length_threshold", tree->priv->length_threshold,
		"uniform_row_height", tree->priv->uniform_row_height,
		NULL);

	g_signal_connect (
		tree->priv->item, "cursor_change",
		G_CALLBACK (item_cursor_change), tree);
	g_signal_connect (
		tree->priv->item, "cursor_activated",
		G_CALLBACK (item_cursor_activated), tree);
	g_signal_connect (
		tree->priv->item, "double_click",
		G_CALLBACK (item_double_click), tree);
	g_signal_connect (
		tree->priv->item, "right_click",
		G_CALLBACK (item_right_click), tree);
	g_signal_connect (
		tree->priv->item, "click",
		G_CALLBACK (item_click), tree);
	g_signal_connect (
		tree->priv->item, "key_press",
		G_CALLBACK (item_key_press), tree);
	g_signal_connect (
		tree->priv->item, "start_drag",
		G_CALLBACK (item_start_drag), tree);
	e_signal_connect_notify (
		tree->priv->item, "notify::is-editing",
		G_CALLBACK (tree_item_is_editing_changed_cb), tree);
}

static void
et_canvas_style_updated (GtkWidget *widget)
{
	GdkRGBA color;

	GTK_WIDGET_CLASS (e_tree_parent_class)->style_updated (widget);

	e_utils_get_theme_color (widget, "theme_base_color", E_UTILS_DEFAULT_THEME_BASE_COLOR, &color);

	gnome_canvas_item_set (
		E_TREE (widget)->priv->white_item,
		"fill-color", &color,
		NULL);
}

static gboolean
white_item_event (GnomeCanvasItem *white_item,
                  GdkEvent *event,
                  ETree *tree)
{
	gboolean return_val = 0;

	g_signal_emit (
		tree,
		signals[WHITE_SPACE_EVENT], 0,
		event, &return_val);

	if (!return_val && event && tree->priv->item) {
		guint event_button = 0;

		gdk_event_get_button (event, &event_button);

		if (event->type == GDK_BUTTON_PRESS && (event_button == 1 || event_button == 2)) {
			gnome_canvas_item_grab_focus (GNOME_CANVAS_ITEM (tree->priv->item));
			return_val = TRUE;
		}
	}

	return return_val;
}

static gint
et_canvas_root_event (GnomeCanvasItem *root,
                      GdkEvent *event,
                      ETree *tree)
{
	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
		if (event->button.button != 4 && event->button.button != 5) {
			if (gtk_widget_has_focus (GTK_WIDGET (root->canvas))) {
				GnomeCanvasItem *item = GNOME_CANVAS (root->canvas)->focused_item;

				if (E_IS_TABLE_ITEM (item)) {
					e_table_item_leave_edit (E_TABLE_ITEM (item));
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
static gboolean
table_canvas_focus_event_cb (GtkWidget *widget,
                             GdkEventFocus *event,
                             gpointer data)
{
	GnomeCanvas *canvas;
	ETree *tree;

	gtk_widget_queue_draw (widget);

	if (!event->in)
		return TRUE;

	canvas = GNOME_CANVAS (widget);
	tree = E_TREE (data);

	if (!canvas->focused_item ||
		(e_selection_model_cursor_row (tree->priv->selection) == -1)) {
		e_table_item_set_cursor (E_TABLE_ITEM (tree->priv->item), 0, 0);
		gnome_canvas_item_grab_focus (tree->priv->item);
	}

	return TRUE;
}

static void
e_tree_table_canvas_scrolled_cb (GtkAdjustment *vadjustment,
                                 GParamSpec *param,
                                 ETree *tree)
{
	g_return_if_fail (E_IS_TREE (tree));

	if (tree->priv->item)
		e_table_item_cursor_scrolled (E_TABLE_ITEM (tree->priv->item));
}

static void
et_setup_table_canvas_vadjustment (ETree *tree)
{
	GtkAdjustment *vadjustment = NULL;

	g_return_if_fail (E_IS_TREE (tree));

	if (tree->priv->table_canvas_vadjustment) {
		g_signal_handlers_disconnect_by_data (tree->priv->table_canvas_vadjustment, tree);
		g_clear_object (&tree->priv->table_canvas_vadjustment);
	}

	if (tree->priv->table_canvas)
		vadjustment = gtk_scrollable_get_vadjustment (
			GTK_SCROLLABLE (tree->priv->table_canvas));

	if (vadjustment) {
		tree->priv->table_canvas_vadjustment = g_object_ref (vadjustment);
		g_signal_connect (
			vadjustment, "notify::value",
			G_CALLBACK (e_tree_table_canvas_scrolled_cb), tree);
	}
}

static void
e_tree_setup_table (ETree *tree)
{
	GtkWidget *widget;
	GdkRGBA color;

	tree->priv->table_canvas = GNOME_CANVAS (e_canvas_new ());
	gtk_widget_set_hexpand (GTK_WIDGET (tree->priv->table_canvas), TRUE);
	gtk_widget_set_vexpand (GTK_WIDGET (tree->priv->table_canvas), TRUE);
	g_signal_connect (
		tree->priv->table_canvas, "size_allocate",
		G_CALLBACK (tree_canvas_size_allocate), tree);
	g_signal_connect (
		tree->priv->table_canvas, "focus_in_event",
		G_CALLBACK (table_canvas_focus_event_cb), tree);
	g_signal_connect (
		tree->priv->table_canvas, "focus_out_event",
		G_CALLBACK (table_canvas_focus_event_cb), tree);

	g_signal_connect (
		tree->priv->table_canvas, "drag_begin",
		G_CALLBACK (et_drag_begin), tree);
	g_signal_connect (
		tree->priv->table_canvas, "drag_end",
		G_CALLBACK (et_drag_end), tree);
	g_signal_connect (
		tree->priv->table_canvas, "drag_data_get",
		G_CALLBACK (et_drag_data_get), tree);
	g_signal_connect (
		tree->priv->table_canvas, "drag_data_delete",
		G_CALLBACK (et_drag_data_delete), tree);
	g_signal_connect (
		tree, "drag_motion",
		G_CALLBACK (et_drag_motion), tree);
	g_signal_connect (
		tree, "drag_leave",
		G_CALLBACK (et_drag_leave), tree);
	g_signal_connect (
		tree, "drag_drop",
		G_CALLBACK (et_drag_drop), tree);
	g_signal_connect (
		tree, "drag_data_received",
		G_CALLBACK (et_drag_data_received), tree);

	g_signal_connect (
		tree->priv->table_canvas, "reflow",
		G_CALLBACK (tree_canvas_reflow), tree);

	et_setup_table_canvas_vadjustment (tree);
	g_signal_connect_swapped (
		tree->priv->table_canvas, "notify::vadjustment",
		G_CALLBACK (et_setup_table_canvas_vadjustment), tree);

	widget = GTK_WIDGET (tree->priv->table_canvas);

	gtk_widget_show (widget);

	e_utils_get_theme_color (widget, "theme_base_color", E_UTILS_DEFAULT_THEME_BASE_COLOR, &color);

	tree->priv->white_item = gnome_canvas_item_new (
		gnome_canvas_root (tree->priv->table_canvas),
		e_canvas_background_get_type (),
		"fill-color", &color,
		NULL);

	g_signal_connect (
		tree->priv->white_item, "event",
		G_CALLBACK (white_item_event), tree);
	g_signal_connect (
		gnome_canvas_root (tree->priv->table_canvas), "event",
		G_CALLBACK (et_canvas_root_event), tree);

	et_build_item (tree);
}

void
e_tree_set_state_object (ETree *tree,
                         ETableState *state)
{
	GValue *val;
	GtkAllocation allocation;
	GtkWidget *widget;

	val = g_new0 (GValue, 1);
	g_value_init (val, G_TYPE_DOUBLE);

	connect_header (tree, state);

	widget = GTK_WIDGET (tree->priv->table_canvas);
	gtk_widget_get_allocation (widget, &allocation);

	g_value_set_double (val, (gdouble) allocation.width);
	g_object_set_property (G_OBJECT (tree->priv->header), "width", val);
	g_free (val);

	if (tree->priv->header_item)
		g_object_set (
			tree->priv->header_item,
			"ETableHeader", tree->priv->header,
			"sort_info", tree->priv->sort_info,
			NULL);

	if (tree->priv->item)
		g_object_set (
			tree->priv->item,
			"ETableHeader", tree->priv->header,
			NULL);

	if (tree->priv->etta)
		e_tree_table_adapter_set_sort_info (
			tree->priv->etta, tree->priv->sort_info);

	e_tree_state_change (tree);
}

/**
 * e_tree_get_state_object:
 * @tree: #ETree object to act on
 *
 * Builds an #ETableState corresponding to the current state of the
 * #ETree.
 *
 * Return value:
 * The %ETableState object generated.
 **/
ETableState *
e_tree_get_state_object (ETree *tree)
{
	ETableState *state;
	GPtrArray *columns;
	gint full_col_count;
	gint i, j;

	columns = e_table_specification_ref_columns (tree->priv->spec);

	state = e_table_state_new (tree->priv->spec);

	g_clear_object (&state->sort_info);
	if (tree->priv->sort_info != NULL)
		state->sort_info = g_object_ref (tree->priv->sort_info);

	state->col_count = e_table_header_count (tree->priv->header);
	full_col_count = e_table_header_count (tree->priv->full_header);

	state->column_specs = g_new (
		ETableColumnSpecification *, state->col_count);
	state->expansions = g_new (gdouble, state->col_count);

	for (i = 0; i < state->col_count; i++) {
		ETableCol *col = e_table_header_get_column (tree->priv->header, i);
		state->column_specs[i] = NULL;
		for (j = 0; j < full_col_count; j++) {
			if (col->spec->model_col == e_table_header_index (tree->priv->full_header, j)) {
				state->column_specs[i] =
					g_object_ref (columns->pdata[j]);
				break;
			}
		}
		state->expansions[i] = col->expansion;
	}

	g_ptr_array_unref (columns);

	return state;
}

/**
 * e_tree_get_spec:
 * @tree: The #ETree to query
 *
 * Returns the specification object.
 *
 * Return value:
 **/
ETableSpecification *
e_tree_get_spec (ETree *tree)
{
	g_return_val_if_fail (E_IS_TREE (tree), NULL);

	return tree->priv->spec;
}

static void
et_table_model_changed (ETableModel *model,
                        ETree *tree)
{
	if (tree->priv->horizontal_scrolling)
		e_table_header_update_horizontal (tree->priv->header);
}

static void
et_table_row_changed (ETableModel *table_model,
                      gint row,
                      ETree *tree)
{
	et_table_model_changed (table_model, tree);
}

static void
et_table_cell_changed (ETableModel *table_model,
                       gint view_col,
                       gint row,
                       ETree *tree)
{
	et_table_model_changed (table_model, tree);
}

static void
et_table_rows_deleted (ETableModel *table_model,
                       gint row,
                       gint count,
                       ETree *tree)
{
	ETreeTableAdapter *adapter;
	ETreePath * node, * prev_node;

	/* If the cursor is still valid after this deletion, we're done */
	if (e_selection_model_cursor_row (tree->priv->selection) >= 0
			|| row == 0)
		return;

	adapter = e_tree_get_table_adapter (tree);
	prev_node = e_tree_table_adapter_node_at_row (adapter, row - 1);
	node = e_tree_get_cursor (tree);

	/* Check if the cursor is a child of the node directly before the
	 * deleted region (implying that an expander was collapsed with
	 * the cursor inside it) */
	while (node) {
		node = e_tree_model_node_get_parent (tree->priv->model, node);
		if (node == prev_node) {
			/* Set the cursor to the still-visible parent */
			e_tree_set_cursor (tree, prev_node);
			return;
		}
	}
}

static void
e_tree_update_full_header_grouped_view (ETree *tree)
{
	gint ii, sz;

	g_return_if_fail (E_IS_TREE (tree));

	if (!tree->priv->full_header)
		return;

	sz = e_table_header_count (tree->priv->full_header);
	for (ii = 0; ii < sz; ii++) {
		ETableCol *col;

		col = e_table_header_get_column (tree->priv->full_header, ii);
		if (!col || !E_IS_CELL_TREE (col->ecell))
			continue;

		e_cell_tree_set_grouped_view (E_CELL_TREE (col->ecell), tree->priv->grouped_view);
	}
}

static void
et_connect_to_etta (ETree *tree)
{
	tree->priv->table_model_change_id = g_signal_connect (
		tree->priv->etta, "model_changed",
		G_CALLBACK (et_table_model_changed), tree);

	tree->priv->table_row_change_id = g_signal_connect (
		tree->priv->etta, "model_row_changed",
		G_CALLBACK (et_table_row_changed), tree);

	tree->priv->table_cell_change_id = g_signal_connect (
		tree->priv->etta, "model_cell_changed",
		G_CALLBACK (et_table_cell_changed), tree);

	tree->priv->table_rows_delete_id = g_signal_connect (
		tree->priv->etta, "model_rows_deleted",
		G_CALLBACK (et_table_rows_deleted), tree);

	g_object_bind_property (tree, "sort-children-ascending",
		tree->priv->etta, "sort-children-ascending",
		G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
}

static gboolean
et_real_construct (ETree *tree,
                   ETreeModel *etm,
                   ETableExtras *ete,
                   ETableSpecification *specification,
                   ETableState *state)
{
	GtkAdjustment *adjustment;
	GtkScrollable *scrollable;
	gint row = 0;

	if (ete)
		g_object_ref (ete);
	else
		ete = e_table_extras_new ();

	tree->priv->alternating_row_colors = specification->alternating_row_colors;
	tree->priv->horizontal_draw_grid = specification->horizontal_draw_grid;
	tree->priv->vertical_draw_grid = specification->vertical_draw_grid;
	tree->priv->draw_focus = specification->draw_focus;
	tree->priv->cursor_mode = specification->cursor_mode;
	tree->priv->full_header = e_table_spec_to_full_header (specification, ete);

	e_tree_update_full_header_grouped_view (tree);

	connect_header (tree, state);

	tree->priv->horizontal_scrolling = specification->horizontal_scrolling;

	tree->priv->model = etm;
	g_object_ref (etm);

	tree->priv->etta = E_TREE_TABLE_ADAPTER (
		e_tree_table_adapter_new (
			tree->priv->model,
			tree->priv->sort_info,
			tree->priv->full_header));

	et_connect_to_etta (tree);

	g_object_set (
		tree->priv->selection,
		"model", tree->priv->model,
		"etta", tree->priv->etta,
		"selection_mode", specification->selection_mode,
		"cursor_mode", specification->cursor_mode,
		NULL);

	g_signal_connect (
		tree->priv->selection, "selection_changed",
		G_CALLBACK (et_selection_model_selection_changed), tree);
	g_signal_connect (
		tree->priv->selection, "selection_row_changed",
		G_CALLBACK (et_selection_model_selection_row_changed), tree);

	if (!specification->no_headers) {
		e_tree_setup_header (tree);
	}
	e_tree_setup_table (tree);

	scrollable = GTK_SCROLLABLE (tree->priv->table_canvas);

	adjustment = gtk_scrollable_get_vadjustment (scrollable);
	gtk_adjustment_set_step_increment (adjustment, 20);

	adjustment = gtk_scrollable_get_hadjustment (scrollable);
	gtk_adjustment_set_step_increment (adjustment, 20);

	if (!specification->no_headers) {
		/*
		 * The header
		 */
		gtk_grid_attach (
			GTK_GRID (tree),
			GTK_WIDGET (tree->priv->header_canvas),
			0, row, 1, 1);
		row++;
	}

	gtk_grid_attach (
		GTK_GRID (tree),
		GTK_WIDGET (tree->priv->table_canvas),
		0, row, 1, 1);

	g_object_unref (ete);

	return TRUE;
}

/**
 * e_tree_construct:
 * @tree: The newly created #ETree object.
 * @etm: The model for this table.
 * @ete: An optional #ETableExtras.  (%NULL is valid.)
 * @specification: an #ETableSpecification
 *
 * This is the internal implementation of e_tree_new() for use by
 * subclasses or language bindings.  See e_tree_new() for details.
 *
 * Return value: %TRUE on success, %FALSE if an error occurred
 **/
gboolean
e_tree_construct (ETree *tree,
                  ETreeModel *etm,
                  ETableExtras *ete,
                  ETableSpecification *specification)
{
	ETableState *state;

	g_return_val_if_fail (E_IS_TREE (tree), FALSE);
	g_return_val_if_fail (E_IS_TREE_MODEL (etm), FALSE);
	g_return_val_if_fail (ete == NULL || E_IS_TABLE_EXTRAS (ete), FALSE);
	g_return_val_if_fail (E_IS_TABLE_SPECIFICATION (specification), FALSE);

	state = g_object_ref (specification->state);

	et_real_construct (tree, etm, ete, specification, state);

	tree->priv->spec = g_object_ref (specification);
	tree->priv->spec->allow_grouping = FALSE;

	g_object_unref (state);

	return TRUE;
}

/**
 * e_tree_new:
 * @etm: The model for this tree
 * @ete: An optional #ETableExtras  (%NULL is valid.)
 * @specification: an #ETableSpecification
 *
 * This function creates an #ETree from the given parameters.  The
 * #ETreeModel is a tree model to be represented.  The #ETableExtras
 * is an optional set of pixbufs, cells, and sorting functions to be
 * used when interpreting the spec.  If you pass in %NULL it uses the
 * default #ETableExtras.  (See e_table_extras_new()).
 *
 * @specification is the specification of the set of viewable columns and the
 * default sorting state and such.  @state is an optional string specifying
 * the current sorting state and such.
 *
 * Return value:
 * The newly created #ETree or %NULL if there's an error.
 **/
GtkWidget *
e_tree_new (ETreeModel *etm,
            ETableExtras *ete,
            ETableSpecification *specification)
{
	ETree *tree;

	g_return_val_if_fail (E_IS_TREE_MODEL (etm), NULL);
	g_return_val_if_fail (ete == NULL || E_IS_TABLE_EXTRAS (ete), NULL);
	g_return_val_if_fail (E_IS_TABLE_SPECIFICATION (specification), NULL);

	tree = g_object_new (E_TYPE_TREE, NULL);

	if (!e_tree_construct (tree, etm, ete, specification)) {
		g_object_unref (tree);
		return NULL;
	}

	return GTK_WIDGET (tree);
}

void
e_tree_show_cursor_after_reflow (ETree *tree)
{
	g_return_if_fail (E_IS_TREE (tree));

	tree->priv->show_cursor_after_reflow = TRUE;
}

void
e_tree_set_cursor (ETree *tree,
                   ETreePath path)
{
	g_return_if_fail (E_IS_TREE (tree));
	g_return_if_fail (path != NULL);

	e_tree_selection_model_select_single_path (
		E_TREE_SELECTION_MODEL (tree->priv->selection), path);
	e_tree_selection_model_change_cursor (
		E_TREE_SELECTION_MODEL (tree->priv->selection), path);
}

ETreePath
e_tree_get_cursor (ETree *tree)
{
	return e_tree_selection_model_get_cursor (
		E_TREE_SELECTION_MODEL (tree->priv->selection));
}

/* Standard functions */
static void
et_foreach_recurse (ETreeModel *model,
                    ETreePath path,
                    ETreeForeachFunc callback,
                    gpointer closure)
{
	ETreePath child;

	callback (path, closure);

	for (child = e_tree_model_node_get_first_child (E_TREE_MODEL (model), path);
	     child;
	     child = e_tree_model_node_get_next (E_TREE_MODEL (model), child)) {
		et_foreach_recurse (model, child, callback, closure);
	}
}

void
e_tree_path_foreach (ETree *tree,
                     ETreeForeachFunc callback,
                     gpointer closure)
{
	ETreePath root;

	g_return_if_fail (E_IS_TREE (tree));

	root = e_tree_model_get_root (tree->priv->model);

	if (root)
		et_foreach_recurse (tree->priv->model,
				    root,
				    callback,
				    closure);
}

static void
et_get_property (GObject *object,
                 guint property_id,
                 GValue *value,
                 GParamSpec *pspec)
{
	ETree *tree = E_TREE (object);

	switch (property_id) {
	case PROP_ETTA:
		g_value_set_object (value, tree->priv->etta);
		break;

	case PROP_UNIFORM_ROW_HEIGHT:
		g_value_set_boolean (value, tree->priv->uniform_row_height);
		break;

	case PROP_IS_EDITING:
		g_value_set_boolean (value, e_tree_is_editing (tree));
		break;

	case PROP_ALWAYS_SEARCH:
		g_value_set_boolean (value, tree->priv->always_search);
		break;

	case PROP_HADJUSTMENT:
		if (tree->priv->table_canvas)
			g_object_get_property (
				G_OBJECT (tree->priv->table_canvas),
				"hadjustment", value);
		else
			g_value_set_object (value, NULL);
		break;

	case PROP_VADJUSTMENT:
		if (tree->priv->table_canvas)
			g_object_get_property (
				G_OBJECT (tree->priv->table_canvas),
				"vadjustment", value);
		else
			g_value_set_object (value, NULL);
		break;

	case PROP_HSCROLL_POLICY:
		if (tree->priv->table_canvas)
			g_object_get_property (
				G_OBJECT (tree->priv->table_canvas),
				"hscroll-policy", value);
		else
			g_value_set_enum (value, 0);
		break;

	case PROP_VSCROLL_POLICY:
		if (tree->priv->table_canvas)
			g_object_get_property (
				G_OBJECT (tree->priv->table_canvas),
				"vscroll-policy", value);
		else
			g_value_set_enum (value, 0);
		break;

	case PROP_SORT_CHILDREN_ASCENDING:
		g_value_set_boolean (value, e_tree_get_sort_children_ascending (tree));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

typedef struct {
	gchar     *arg;
	gboolean  setting;
} bool_closure;

static void
et_set_property (GObject *object,
                 guint property_id,
                 const GValue *value,
                 GParamSpec *pspec)
{
	ETree *tree = E_TREE (object);

	switch (property_id) {
	case PROP_LENGTH_THRESHOLD:
		tree->priv->length_threshold = g_value_get_int (value);
		if (tree->priv->item) {
			gnome_canvas_item_set (
				GNOME_CANVAS_ITEM (tree->priv->item),
				"length_threshold",
				tree->priv->length_threshold,
				NULL);
		}
		break;

	case PROP_HORIZONTAL_DRAW_GRID:
		tree->priv->horizontal_draw_grid = g_value_get_boolean (value);
		if (tree->priv->item) {
			gnome_canvas_item_set (
				GNOME_CANVAS_ITEM (tree->priv->item),
				"horizontal_draw_grid",
				tree->priv->horizontal_draw_grid,
				NULL);
		}
		break;

	case PROP_VERTICAL_DRAW_GRID:
		tree->priv->vertical_draw_grid = g_value_get_boolean (value);
		if (tree->priv->item) {
			gnome_canvas_item_set (
				GNOME_CANVAS_ITEM (tree->priv->item),
				"vertical_draw_grid",
				tree->priv->vertical_draw_grid,
				NULL);
		}
		break;

	case PROP_DRAW_FOCUS:
		tree->priv->draw_focus = g_value_get_boolean (value);
		if (tree->priv->item) {
			gnome_canvas_item_set (
				GNOME_CANVAS_ITEM (tree->priv->item),
				"drawfocus",
				tree->priv->draw_focus,
				NULL);
		}
		break;

	case PROP_UNIFORM_ROW_HEIGHT:
		tree->priv->uniform_row_height = g_value_get_boolean (value);
		if (tree->priv->item) {
			gnome_canvas_item_set (
				GNOME_CANVAS_ITEM (tree->priv->item),
				"uniform_row_height",
				tree->priv->uniform_row_height,
				NULL);
		}
		break;

	case PROP_ALWAYS_SEARCH:
		if (tree->priv->always_search == g_value_get_boolean (value))
			return;
		tree->priv->always_search = g_value_get_boolean (value);
		clear_current_search_col (tree);
		break;

	case PROP_HADJUSTMENT:
		if (tree->priv->table_canvas)
			g_object_set_property (
				G_OBJECT (tree->priv->table_canvas),
				"hadjustment", value);
		break;

	case PROP_VADJUSTMENT:
		if (tree->priv->table_canvas)
			g_object_set_property (
				G_OBJECT (tree->priv->table_canvas),
				"vadjustment", value);
		break;

	case PROP_HSCROLL_POLICY:
		if (tree->priv->table_canvas)
			g_object_set_property (
				G_OBJECT (tree->priv->table_canvas),
				"hscroll-policy", value);
		break;

	case PROP_VSCROLL_POLICY:
		if (tree->priv->table_canvas)
			g_object_set_property (
				G_OBJECT (tree->priv->table_canvas),
				"vscroll-policy", value);
		break;

	case PROP_SORT_CHILDREN_ASCENDING:
		e_tree_set_sort_children_ascending (tree, g_value_get_boolean (value));
		break;
	}
}

/**
 * e_tree_get_model:
 * @tree: the ETree
 *
 * Returns the model upon which this ETree is based.
 *
 * Returns: the model
 **/
ETreeModel *
e_tree_get_model (ETree *tree)
{
	g_return_val_if_fail (E_IS_TREE (tree), NULL);

	return tree->priv->model;
}

/**
 * e_tree_get_selection_model:
 * @tree: the ETree
 *
 * Returns the selection model of this ETree.
 *
 * Returns: the selection model
 **/
ESelectionModel *
e_tree_get_selection_model (ETree *tree)
{
	g_return_val_if_fail (E_IS_TREE (tree), NULL);

	return tree->priv->selection;
}

/**
 * e_tree_get_table_adapter:
 * @tree: the ETree
 *
 * Returns the table adapter this ETree uses.
 *
 * Returns: the model
 **/
ETreeTableAdapter *
e_tree_get_table_adapter (ETree *tree)
{
	g_return_val_if_fail (E_IS_TREE (tree), NULL);

	return tree->priv->etta;
}

ETableItem *
e_tree_get_item (ETree *tree)
{
	g_return_val_if_fail (E_IS_TREE (tree), NULL);

	return E_TABLE_ITEM (tree->priv->item);
}

GnomeCanvasItem *
e_tree_get_header_item (ETree *tree)
{
	g_return_val_if_fail (E_IS_TREE (tree), NULL);

	return tree->priv->header_item;
}

struct _ETreeDragSourceSite
{
	GdkModifierType    start_button_mask;
	GtkTargetList     *target_list;        /* Targets for drag data */
	GdkDragAction      actions;            /* Possible actions */
	GdkPixbuf         *pixbuf;             /* Icon for drag data */

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
  guint              dropped : 1;     /* Set after we receive a drop */
  guint32            proxy_drop_time; /* Timestamp for proxied drop */
  guint              proxy_drop_wait : 1; /* Set if we are waiting for a
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

/* Source side */

static gint
et_real_start_drag (ETree *tree,
                    gint row,
                    ETreePath path,
                    gint col,
                    GdkEvent *event)
{
	GtkDragSourceInfo *info;
	GdkDragContext *context;
	ETreeDragSourceSite *site;

	if (tree->priv->do_drag) {
		site = tree->priv->site;

		site->state = 0;
		context = e_tree_drag_begin (
			tree, row, col,
			site->target_list,
			site->actions,
			1, event);

		if (context) {
			info = g_dataset_get_data (context, "gtk-info");

			if (info && !info->icon_window) {
				if (site->pixbuf)
					gtk_drag_set_icon_pixbuf (
						context,
						site->pixbuf,
						-2, -2);
				else
					gtk_drag_set_icon_default (context);
			}
		}
		return TRUE;
	}
	return FALSE;
}

void
e_tree_drag_source_set (ETree *tree,
                        GdkModifierType start_button_mask,
                        const GtkTargetEntry *targets,
                        gint n_targets,
                        GdkDragAction actions)
{
	ETreeDragSourceSite *site;
	GtkWidget *canvas;

	g_return_if_fail (E_IS_TREE (tree));

	canvas = GTK_WIDGET (tree->priv->table_canvas);
	site = tree->priv->site;

	tree->priv->do_drag = TRUE;

	gtk_widget_add_events (
		canvas,
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

	g_return_if_fail (E_IS_TREE (tree));

	site = tree->priv->site;

	if (site) {
		if (site->target_list)
			gtk_target_list_unref (site->target_list);
		g_free (site);
		tree->priv->site = NULL;
	}
}

/* There probably should be functions for setting the targets
 * as a GtkTargetList
 */

GdkDragContext *
e_tree_drag_begin (ETree *tree,
                   gint row,
                   gint col,
                   GtkTargetList *targets,
                   GdkDragAction actions,
                   gint button,
                   GdkEvent *event)
{
	ETreePath path;

	g_return_val_if_fail (E_IS_TREE (tree), NULL);

	path = e_tree_table_adapter_node_at_row (tree->priv->etta, row);

	tree->priv->drag_row = row;
	tree->priv->drag_path = path;
	tree->priv->drag_col = col;

	return gtk_drag_begin (
		GTK_WIDGET (tree->priv->table_canvas),
		targets,
		actions,
		button,
		event);
}

/**
 * e_tree_is_dragging:
 * @tree: An #ETree widget
 *
 * Returns whether is @tree in a drag&drop operation.
 **/
gboolean
e_tree_is_dragging (ETree *tree)
{
	g_return_val_if_fail (E_IS_TREE (tree), FALSE);

	return tree->priv->is_dragging;
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
                    gint x,
                    gint y,
                    gint *row_return,
                    gint *col_return)
{
	GtkAdjustment *adjustment;
	GtkScrollable *scrollable;

	g_return_if_fail (E_IS_TREE (tree));
	g_return_if_fail (row_return != NULL);
	g_return_if_fail (col_return != NULL);

	/* FIXME it would be nice if it could handle a NULL row_return or
	 * col_return gracefully.  */

	if (row_return)
		*row_return = -1;
	if (col_return)
		*col_return = -1;

	scrollable = GTK_SCROLLABLE (tree->priv->table_canvas);

	adjustment = gtk_scrollable_get_hadjustment (scrollable);
	x += gtk_adjustment_get_value (adjustment);

	adjustment = gtk_scrollable_get_vadjustment (scrollable);
	y += gtk_adjustment_get_value (adjustment);

	e_table_item_compute_location (
		E_TABLE_ITEM (tree->priv->item),
		&x, &y, row_return, col_return);
}

/**
 * e_tree_get_cell_geometry:
 * @tree: The tree.
 * @row: The row to get the geometry of.
 * @col: The col to get the geometry of.
 * @x_return: Returns the x coordinate of the upper right hand corner
 * of the cell with respect to the widget.
 * @y_return: Returns the y coordinate of the upper right hand corner
 * of the cell with respect to the widget.
 * @width_return: Returns the width of the cell.
 * @height_return: Returns the height of the cell.
 *
 * Computes the data about this cell.
 **/
void
e_tree_get_cell_geometry (ETree *tree,
                          gint row,
                          gint col,
                          gint *x_return,
                          gint *y_return,
                          gint *width_return,
                          gint *height_return)
{
	GtkAdjustment *adjustment;
	GtkScrollable *scrollable;

	g_return_if_fail (E_IS_TREE (tree));
	g_return_if_fail (row >= 0);
	g_return_if_fail (col >= 0);

	/* FIXME it would be nice if it could handle a NULL row_return or
	 * col_return gracefully.  */

	e_table_item_get_cell_geometry (
		E_TABLE_ITEM (tree->priv->item),
		&row, &col, x_return, y_return,
		width_return, height_return);

	scrollable = GTK_SCROLLABLE (tree->priv->table_canvas);

	if (x_return) {
		adjustment = gtk_scrollable_get_hadjustment (scrollable);
		(*x_return) -= gtk_adjustment_get_value (adjustment);
	}

	if (y_return) {
		adjustment = gtk_scrollable_get_vadjustment (scrollable);
		(*y_return) -= gtk_adjustment_get_value (adjustment);
	}
}

static void
et_drag_begin (GtkWidget *widget,
               GdkDragContext *context,
               ETree *tree)
{
	tree->priv->is_dragging = TRUE;

	g_signal_emit (
		tree,
		signals[TREE_DRAG_BEGIN], 0,
		tree->priv->drag_row,
		tree->priv->drag_path,
		tree->priv->drag_col,
		context);
}

static void
et_drag_end (GtkWidget *widget,
             GdkDragContext *context,
             ETree *tree)
{
	tree->priv->is_dragging = FALSE;

	g_signal_emit (
		tree,
		signals[TREE_DRAG_END], 0,
		tree->priv->drag_row,
		tree->priv->drag_path,
		tree->priv->drag_col,
		context);
}

static void
et_drag_data_get (GtkWidget *widget,
                  GdkDragContext *context,
                  GtkSelectionData *selection_data,
                  guint info,
                  guint time,
                  ETree *tree)
{
	g_signal_emit (
		tree,
		signals[TREE_DRAG_DATA_GET], 0,
		tree->priv->drag_row,
		tree->priv->drag_path,
		tree->priv->drag_col,
		context,
		selection_data,
		info,
		time);
}

static void
et_drag_data_delete (GtkWidget *widget,
                     GdkDragContext *context,
                     ETree *tree)
{
	g_signal_emit (
		tree,
		signals[TREE_DRAG_DATA_DELETE], 0,
		tree->priv->drag_row,
		tree->priv->drag_path,
		tree->priv->drag_col,
		context);
}

static gboolean
do_drag_motion (ETree *tree,
                GdkDragContext *context,
                gint x,
                gint y,
                guint time)
{
	gboolean ret_val = FALSE;
	gint row, col;
	ETreePath path;

	e_tree_get_cell_at (tree, x, y, &row, &col);

	if (row != tree->priv->drop_row && col != tree->priv->drop_col) {
		g_signal_emit (
			tree, signals[TREE_DRAG_LEAVE], 0,
			tree->priv->drop_row,
			tree->priv->drop_path,
			tree->priv->drop_col,
			context,
			time);
	}

	path = e_tree_table_adapter_node_at_row (tree->priv->etta, row);

	tree->priv->drop_row = row;
	tree->priv->drop_path = path;
	tree->priv->drop_col = col;
	g_signal_emit (
		tree, signals[TREE_DRAG_MOTION], 0,
		tree->priv->drop_row,
		tree->priv->drop_path,
		tree->priv->drop_col,
		context,
		x, y,
		time,
		&ret_val);

	return ret_val;
}

static gboolean
scroll_timeout (gpointer data)
{
	ETree *tree = data;
	gint dx = 0, dy = 0;
	GtkAdjustment *adjustment;
	GtkScrollable *scrollable;
	gdouble old_h_value;
	gdouble new_h_value;
	gdouble old_v_value;
	gdouble new_v_value;
	gdouble page_size;
	gdouble lower;
	gdouble upper;

	if (tree->priv->scroll_direction & ET_SCROLL_DOWN)
		dy += 20;
	if (tree->priv->scroll_direction & ET_SCROLL_UP)
		dy -= 20;

	if (tree->priv->scroll_direction & ET_SCROLL_RIGHT)
		dx += 20;
	if (tree->priv->scroll_direction & ET_SCROLL_LEFT)
		dx -= 20;

	scrollable = GTK_SCROLLABLE (tree->priv->table_canvas);

	adjustment = gtk_scrollable_get_hadjustment (scrollable);

	page_size = gtk_adjustment_get_page_size (adjustment);
	lower = gtk_adjustment_get_lower (adjustment);
	upper = gtk_adjustment_get_upper (adjustment);

	old_h_value = gtk_adjustment_get_value (adjustment);
	new_h_value = CLAMP (old_h_value + dx, lower, upper - page_size);

	gtk_adjustment_set_value (adjustment, new_h_value);

	adjustment = gtk_scrollable_get_vadjustment (scrollable);

	page_size = gtk_adjustment_get_page_size (adjustment);
	lower = gtk_adjustment_get_lower (adjustment);
	upper = gtk_adjustment_get_upper (adjustment);

	old_v_value = gtk_adjustment_get_value (adjustment);
	new_v_value = CLAMP (old_v_value + dy, lower, upper - page_size);

	gtk_adjustment_set_value (adjustment, new_v_value);

	if (new_h_value != old_h_value || new_v_value != old_v_value)
		do_drag_motion (
			tree,
			tree->priv->last_drop_context,
			tree->priv->last_drop_x,
			tree->priv->last_drop_y,
			tree->priv->last_drop_time);

	return TRUE;
}

static void
scroll_on (ETree *tree,
           guint scroll_direction)
{
	if (tree->priv->scroll_idle_id == 0 ||
			scroll_direction != tree->priv->scroll_direction) {
		if (tree->priv->scroll_idle_id != 0)
			g_source_remove (tree->priv->scroll_idle_id);
		tree->priv->scroll_direction = scroll_direction;
		tree->priv->scroll_idle_id =
			e_named_timeout_add (100, scroll_timeout, tree);
	}
}

static void
scroll_off (ETree *tree)
{
	if (tree->priv->scroll_idle_id) {
		g_source_remove (tree->priv->scroll_idle_id);
		tree->priv->scroll_idle_id = 0;
	}
}

static gboolean
hover_timeout (gpointer data)
{
	ETree *tree = data;
	gint x = tree->priv->hover_x;
	gint y = tree->priv->hover_y;
	gint row, col;
	ETreePath path;

	e_tree_get_cell_at (tree, x, y, &row, &col);

	path = e_tree_table_adapter_node_at_row (tree->priv->etta, row);
	if (path && e_tree_model_node_is_expandable (tree->priv->model, path)) {
		if (!e_tree_table_adapter_node_is_expanded (tree->priv->etta, path)) {
			tree->priv->expanded_list = g_list_prepend (
				tree->priv->expanded_list,
				e_tree_model_get_save_id (
				tree->priv->model, path));

			e_tree_table_adapter_node_set_expanded (
				tree->priv->etta, path, TRUE);
		}
	}

	return TRUE;
}

static void
hover_on (ETree *tree,
          gint x,
          gint y)
{
	tree->priv->hover_x = x;
	tree->priv->hover_y = y;
	if (tree->priv->hover_idle_id != 0)
		g_source_remove (tree->priv->hover_idle_id);
	tree->priv->hover_idle_id =
		e_named_timeout_add (500, hover_timeout, tree);
}

static void
hover_off (ETree *tree)
{
	if (tree->priv->hover_idle_id) {
		g_source_remove (tree->priv->hover_idle_id);
		tree->priv->hover_idle_id = 0;
	}
}

static void
collapse_drag (ETree *tree,
               ETreePath drop)
{
	GList *list;

	/* We only want to leave open parents of the node dropped in.
	 * Not the node itself. */
	if (drop) {
		drop = e_tree_model_node_get_parent (tree->priv->model, drop);
	}

	for (list = tree->priv->expanded_list; list; list = list->next) {
		gchar *save_id = list->data;
		ETreePath path;

		path = e_tree_model_get_node_by_id (tree->priv->model, save_id);
		if (path) {
			ETreePath search;
			gboolean found = FALSE;

			for (search = drop; search;
				search = e_tree_model_node_get_parent (
				tree->priv->model, search)) {
				if (path == search) {
					found = TRUE;
					break;
				}
			}

			if (!found)
				e_tree_table_adapter_node_set_expanded (
					tree->priv->etta, path, FALSE);
		}
		g_free (save_id);
	}
	g_list_free (tree->priv->expanded_list);
	tree->priv->expanded_list = NULL;
}

static void
context_destroyed (gpointer data,
                   GObject *ctx)
{
	ETree *tree = data;
	if (tree->priv) {
		tree->priv->last_drop_x = 0;
		tree->priv->last_drop_y = 0;
		tree->priv->last_drop_time = 0;
		tree->priv->last_drop_context = NULL;
		collapse_drag (tree, NULL);
		scroll_off (tree);
		hover_off (tree);
	}
	g_object_unref (tree);
}

static void
context_connect (ETree *tree,
                 GdkDragContext *context)
{
	if (context == tree->priv->last_drop_context)
		return;

	if (tree->priv->last_drop_context)
		g_object_weak_unref (
			G_OBJECT (tree->priv->last_drop_context),
			context_destroyed, tree);
	else
		g_object_ref (tree);

	g_object_weak_ref (G_OBJECT (context), context_destroyed, tree);
}

static void
et_drag_leave (GtkWidget *widget,
               GdkDragContext *context,
               guint time,
               ETree *tree)
{
	g_signal_emit (
		tree,
		signals[TREE_DRAG_LEAVE], 0,
		tree->priv->drop_row,
		tree->priv->drop_path,
		tree->priv->drop_col,
		context,
		time);
	tree->priv->drop_row = -1;
	tree->priv->drop_col = -1;

	scroll_off (tree);
	hover_off (tree);
}

static gboolean
et_drag_motion (GtkWidget *widget,
                GdkDragContext *context,
                gint x,
                gint y,
                guint time,
                ETree *tree)
{
	GtkAllocation allocation;
	gint ret_val;
	guint direction = 0;

	tree->priv->last_drop_x = x;
	tree->priv->last_drop_y = y;
	tree->priv->last_drop_time = time;
	context_connect (tree, context);
	tree->priv->last_drop_context = context;

	if (tree->priv->hover_idle_id != 0) {
		if (abs (tree->priv->hover_x - x) > 3 ||
		    abs (tree->priv->hover_y - y) > 3) {
			hover_on (tree, x, y);
		}
	} else {
		hover_on (tree, x, y);
	}

	ret_val = do_drag_motion (tree, context, x, y, time);

	gtk_widget_get_allocation (widget, &allocation);

	if (y < 20)
		direction |= ET_SCROLL_UP;
	if (y > allocation.height - 20)
		direction |= ET_SCROLL_DOWN;
	if (x < 20)
		direction |= ET_SCROLL_LEFT;
	if (x > allocation.width - 20)
		direction |= ET_SCROLL_RIGHT;

	if (direction != 0)
		scroll_on (tree, direction);
	else
		scroll_off (tree);

	return ret_val;
}

static gboolean
et_drag_drop (GtkWidget *widget,
              GdkDragContext *context,
              gint x,
              gint y,
              guint time,
              ETree *tree)
{
	gboolean ret_val = FALSE;
	gint row, col;
	ETreePath path;

	e_tree_get_cell_at (tree, x, y, &row, &col);

	path = e_tree_table_adapter_node_at_row (tree->priv->etta, row);

	if (row != tree->priv->drop_row && col != tree->priv->drop_row) {
		g_signal_emit (
			tree, signals[TREE_DRAG_LEAVE], 0,
			tree->priv->drop_row,
			tree->priv->drop_path,
			tree->priv->drop_col,
			context,
			time);
		g_signal_emit (
			tree, signals[TREE_DRAG_MOTION], 0,
			row,
			path,
			col,
			context,
			x,
			y,
			time,
			&ret_val);
	}
	tree->priv->drop_row = row;
	tree->priv->drop_path = path;
	tree->priv->drop_col = col;

	g_signal_emit (
		tree, signals[TREE_DRAG_DROP], 0,
		tree->priv->drop_row,
		tree->priv->drop_path,
		tree->priv->drop_col,
		context,
		x,
		y,
		time,
		&ret_val);

	tree->priv->drop_row = -1;
	tree->priv->drop_path = NULL;
	tree->priv->drop_col = -1;

	collapse_drag (tree, path);

	scroll_off (tree);
	return ret_val;
}

static void
et_drag_data_received (GtkWidget *widget,
                       GdkDragContext *context,
                       gint x,
                       gint y,
                       GtkSelectionData *selection_data,
                       guint info,
                       guint time,
                       ETree *tree)
{
	gint row, col;
	ETreePath path;

	e_tree_get_cell_at (tree, x, y, &row, &col);

	path = e_tree_table_adapter_node_at_row (tree->priv->etta, row);
	g_signal_emit (
		tree, signals[TREE_DRAG_DATA_RECEIVED], 0,
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
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = et_dispose;
	object_class->set_property = et_set_property;
	object_class->get_property = et_get_property;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->grab_focus = et_grab_focus;
	widget_class->unrealize = et_unrealize;
	widget_class->style_updated = et_canvas_style_updated;
	widget_class->focus = et_focus;

	gtk_widget_class_set_css_name (widget_class, "ETree");

	class->start_drag = et_real_start_drag;

	signals[CURSOR_CHANGE] = g_signal_new (
		"cursor_change",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETreeClass, cursor_change),
		NULL, NULL,
		e_marshal_VOID__INT_POINTER,
		G_TYPE_NONE, 2,
		G_TYPE_INT,
		G_TYPE_POINTER);

	signals[CURSOR_ACTIVATED] = g_signal_new (
		"cursor_activated",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETreeClass, cursor_activated),
		NULL, NULL,
		e_marshal_VOID__INT_POINTER,
		G_TYPE_NONE, 2,
		G_TYPE_INT,
		G_TYPE_POINTER);

	signals[SELECTION_CHANGE] = g_signal_new (
		"selection_change",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETreeClass, selection_change),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[DOUBLE_CLICK] = g_signal_new (
		"double_click",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETreeClass, double_click),
		NULL, NULL,
		e_marshal_VOID__INT_POINTER_INT_BOXED,
		G_TYPE_NONE, 4,
		G_TYPE_INT,
		G_TYPE_POINTER,
		G_TYPE_INT,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	signals[RIGHT_CLICK] = g_signal_new (
		"right_click",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETreeClass, right_click),
		g_signal_accumulator_true_handled, NULL,
		e_marshal_BOOLEAN__INT_POINTER_INT_BOXED,
		G_TYPE_BOOLEAN, 4,
		G_TYPE_INT,
		G_TYPE_POINTER,
		G_TYPE_INT,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	signals[CLICK] = g_signal_new (
		"click",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETreeClass, click),
		g_signal_accumulator_true_handled, NULL,
		e_marshal_BOOLEAN__INT_POINTER_INT_BOXED,
		G_TYPE_BOOLEAN, 4,
		G_TYPE_INT,
		G_TYPE_POINTER,
		G_TYPE_INT,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	signals[KEY_PRESS] = g_signal_new (
		"key_press",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETreeClass, key_press),
		g_signal_accumulator_true_handled, NULL,
		e_marshal_BOOLEAN__INT_POINTER_INT_BOXED,
		G_TYPE_BOOLEAN, 4,
		G_TYPE_INT,
		G_TYPE_POINTER,
		G_TYPE_INT,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	signals[START_DRAG] = g_signal_new (
		"start_drag",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETreeClass, start_drag),
		NULL, NULL,
		e_marshal_VOID__INT_POINTER_INT_BOXED,
		G_TYPE_NONE, 4,
		G_TYPE_INT,
		G_TYPE_POINTER,
		G_TYPE_INT,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	signals[STATE_CHANGE] = g_signal_new (
		"state_change",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETreeClass, state_change),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[WHITE_SPACE_EVENT] = g_signal_new (
		"white_space_event",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETreeClass, white_space_event),
		g_signal_accumulator_true_handled, NULL,
		e_marshal_BOOLEAN__POINTER,
		G_TYPE_BOOLEAN, 1,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	signals[TREE_DRAG_BEGIN] = g_signal_new (
		"tree_drag_begin",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETreeClass, tree_drag_begin),
		NULL, NULL,
		e_marshal_VOID__INT_POINTER_INT_BOXED,
		G_TYPE_NONE, 4,
		G_TYPE_INT,
		G_TYPE_POINTER,
		G_TYPE_INT,
		GDK_TYPE_DRAG_CONTEXT);

	signals[TREE_DRAG_END] = g_signal_new (
		"tree_drag_end",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETreeClass, tree_drag_end),
		NULL, NULL,
		e_marshal_VOID__INT_POINTER_INT_BOXED,
		G_TYPE_NONE, 4,
		G_TYPE_INT,
		G_TYPE_POINTER,
		G_TYPE_INT,
		GDK_TYPE_DRAG_CONTEXT);

	signals[TREE_DRAG_DATA_GET] = g_signal_new (
		"tree_drag_data_get",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETreeClass, tree_drag_data_get),
		NULL, NULL,
		e_marshal_VOID__INT_POINTER_INT_OBJECT_BOXED_UINT_UINT,
		G_TYPE_NONE, 7,
		G_TYPE_INT,
		G_TYPE_POINTER,
		G_TYPE_INT,
		GDK_TYPE_DRAG_CONTEXT,
		GTK_TYPE_SELECTION_DATA | G_SIGNAL_TYPE_STATIC_SCOPE,
		G_TYPE_UINT,
		G_TYPE_UINT);

	signals[TREE_DRAG_DATA_DELETE] = g_signal_new (
		"tree_drag_data_delete",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETreeClass, tree_drag_data_delete),
		NULL, NULL,
		e_marshal_VOID__INT_POINTER_INT_OBJECT,
		G_TYPE_NONE, 4,
		G_TYPE_INT,
		G_TYPE_POINTER,
		G_TYPE_INT,
		GDK_TYPE_DRAG_CONTEXT);

	signals[TREE_DRAG_LEAVE] = g_signal_new (
		"tree_drag_leave",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETreeClass, tree_drag_leave),
		NULL, NULL,
		e_marshal_VOID__INT_POINTER_INT_OBJECT_UINT,
		G_TYPE_NONE, 5,
		G_TYPE_INT,
		G_TYPE_POINTER,
		G_TYPE_INT,
		GDK_TYPE_DRAG_CONTEXT,
		G_TYPE_UINT);

	signals[TREE_DRAG_MOTION] = g_signal_new (
		"tree_drag_motion",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETreeClass, tree_drag_motion),
		NULL, NULL,
		e_marshal_BOOLEAN__INT_POINTER_INT_OBJECT_INT_INT_UINT,
		G_TYPE_BOOLEAN, 7,
		G_TYPE_INT,
		G_TYPE_POINTER,
		G_TYPE_INT,
		GDK_TYPE_DRAG_CONTEXT,
		G_TYPE_INT,
		G_TYPE_INT,
		G_TYPE_UINT);

	signals[TREE_DRAG_DROP] = g_signal_new (
		"tree_drag_drop",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETreeClass, tree_drag_drop),
		NULL, NULL,
		e_marshal_BOOLEAN__INT_POINTER_INT_OBJECT_INT_INT_UINT,
		G_TYPE_BOOLEAN, 7,
		G_TYPE_INT,
		G_TYPE_POINTER,
		G_TYPE_INT,
		GDK_TYPE_DRAG_CONTEXT,
		G_TYPE_INT,
		G_TYPE_INT,
		G_TYPE_UINT);

	signals[TREE_DRAG_DATA_RECEIVED] = g_signal_new (
		"tree_drag_data_received",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETreeClass, tree_drag_data_received),
		NULL, NULL,
		e_marshal_VOID__INT_POINTER_INT_OBJECT_INT_INT_BOXED_UINT_UINT,
		G_TYPE_NONE, 9,
		G_TYPE_INT,
		G_TYPE_POINTER,
		G_TYPE_INT,
		GDK_TYPE_DRAG_CONTEXT,
		G_TYPE_INT,
		G_TYPE_INT,
		GTK_TYPE_SELECTION_DATA,
		G_TYPE_UINT,
		G_TYPE_UINT);

	signals[HEADER_CLICK_CAN_SORT] = g_signal_new (
		"header-click-can-sort",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		/* G_STRUCT_OFFSET (ETreeClass, header_click_can_sort) */ 0,
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);

	g_object_class_install_property (
		object_class,
		PROP_LENGTH_THRESHOLD,
		g_param_spec_int (
			"length_threshold",
			"Length Threshold",
			"Length Threshold",
			0, G_MAXINT, 0,
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		object_class,
		PROP_HORIZONTAL_DRAW_GRID,
		g_param_spec_boolean (
			"horizontal_draw_grid",
			"Horizontal Draw Grid",
			"Horizontal Draw Grid",
			FALSE,
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		object_class,
		PROP_VERTICAL_DRAW_GRID,
		g_param_spec_boolean (
			"vertical_draw_grid",
			"Vertical Draw Grid",
			"Vertical Draw Grid",
			FALSE,
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		object_class,
		PROP_DRAW_FOCUS,
		g_param_spec_boolean (
			"drawfocus",
			"Draw focus",
			"Draw focus",
			FALSE,
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		object_class,
		PROP_ETTA,
		g_param_spec_object (
			"ETreeTableAdapter",
			"ETree table adapter",
			"ETree table adapter",
			E_TYPE_TREE_TABLE_ADAPTER,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_UNIFORM_ROW_HEIGHT,
		g_param_spec_boolean (
			"uniform_row_height",
			"Uniform row height",
			"Uniform row height",
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_IS_EDITING,
		g_param_spec_boolean (
			"is-editing",
			"Whether is in an editing mode",
			"Whether is in an editing mode",
			FALSE,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_ALWAYS_SEARCH,
		g_param_spec_boolean (
			"always_search",
			"Always search",
			"Always search",
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SORT_CHILDREN_ASCENDING,
		g_param_spec_boolean (
			"sort-children-ascending",
			"Sort Children Ascending",
			"Always sort children tree nodes ascending",
			FALSE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	gtk_widget_class_install_style_property (
		widget_class,
		g_param_spec_int (
			"expander_size",
			"Expander Size",
			"Size of the expander arrow",
			0, G_MAXINT, 12,
			G_PARAM_READABLE));

	gtk_widget_class_install_style_property (
		widget_class,
		g_param_spec_int (
			"vertical-spacing",
			"Vertical Row Spacing",
			"Vertical space between rows. "
			"It is added to top and to bottom of a row",
			0, G_MAXINT, 3,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	gtk_widget_class_install_style_property (
		widget_class,
		g_param_spec_boolean (
			"alternating-row-colors",
			"Alternating Row Colors",
			"Whether to use alternating row colors",
			TRUE,
			G_PARAM_READABLE));

	/* Scrollable interface */
	g_object_class_override_property (
		object_class, PROP_HADJUSTMENT, "hadjustment");
	g_object_class_override_property (
		object_class, PROP_VADJUSTMENT, "vadjustment");
	g_object_class_override_property (
		object_class, PROP_HSCROLL_POLICY, "hscroll-policy");
	g_object_class_override_property (
		object_class, PROP_VSCROLL_POLICY, "vscroll-policy");

	gtk_widget_class_set_accessible_type (widget_class,
		GAL_A11Y_TYPE_E_TREE);
}

static gboolean
e_tree_scrollable_get_border (GtkScrollable *scrollable,
			      GtkBorder *border)
{
	ETree *tree;
	ETableHeaderItem *header_item;

	g_return_val_if_fail (E_IS_TREE (scrollable), FALSE);
	g_return_val_if_fail (border != NULL, FALSE);

	tree = E_TREE (scrollable);
	if (!tree->priv->header_item)
		return FALSE;

	g_return_val_if_fail (E_IS_TABLE_HEADER_ITEM (tree->priv->header_item), FALSE);

	header_item = E_TABLE_HEADER_ITEM (tree->priv->header_item);

	border->top = header_item->height;

	return TRUE;
}

static void
e_tree_scrollable_init (GtkScrollableInterface *iface)
{
	iface->get_border = e_tree_scrollable_get_border;
}

static void
tree_size_allocate (GtkWidget *widget,
                    GtkAllocation *alloc,
                    ETree *tree)
{
	gdouble width;

	g_return_if_fail (E_IS_TREE (tree));
	g_return_if_fail (tree->priv->info_text != NULL);

	gnome_canvas_get_scroll_region (
		GNOME_CANVAS (tree->priv->table_canvas),
		NULL, NULL, &width, NULL);

	width -= 60.0;

	g_object_set (
		tree->priv->info_text, "width", width,
		"clip_width", width, NULL);
}

/**
 * e_tree_set_info_message:
 * @tree: #ETree instance
 * @info_message: Message to set. Can be NULL.
 *
 * Creates an info message in table area, or removes old.
 **/
void
e_tree_set_info_message (ETree *tree,
                         const gchar *info_message)
{
	GtkAllocation allocation;
	GtkWidget *widget;

	g_return_if_fail (E_IS_TREE (tree));

	if (!tree->priv->info_text && (!info_message || !*info_message))
		return;

	if (!info_message || !*info_message) {
		g_signal_handler_disconnect (tree, tree->priv->info_text_resize_id);
		g_object_run_dispose (G_OBJECT (tree->priv->info_text));
		tree->priv->info_text = NULL;
		return;
	}

	widget = GTK_WIDGET (tree->priv->table_canvas);
	gtk_widget_get_allocation (widget, &allocation);

	if (!tree->priv->info_text) {
		if (allocation.width > 60) {
			tree->priv->info_text = gnome_canvas_item_new (
				GNOME_CANVAS_GROUP (gnome_canvas_root (tree->priv->table_canvas)),
				e_text_get_type (),
				"line_wrap", TRUE,
				"clip", TRUE,
				"justification", GTK_JUSTIFY_LEFT,
				"text", info_message,
				"width", (gdouble) allocation.width - 60.0,
				"clip_width", (gdouble) allocation.width - 60.0,
				NULL);

			e_canvas_item_move_absolute (tree->priv->info_text, 30, 30);

			tree->priv->info_text_resize_id = g_signal_connect (
				tree, "size_allocate",
				G_CALLBACK (tree_size_allocate), tree);
		}
	} else
		gnome_canvas_item_set (tree->priv->info_text, "text", info_message, NULL);
}

void
e_tree_freeze_state_change (ETree *tree)
{
	g_return_if_fail (E_IS_TREE (tree));

	tree->priv->state_change_freeze++;
	if (tree->priv->state_change_freeze == 1)
		tree->priv->state_changed = FALSE;

	g_return_if_fail (tree->priv->state_change_freeze != 0);
}

void
e_tree_thaw_state_change (ETree *tree)
{
	g_return_if_fail (E_IS_TREE (tree));
	g_return_if_fail (tree->priv->state_change_freeze != 0);

	tree->priv->state_change_freeze--;
	if (tree->priv->state_change_freeze == 0 && tree->priv->state_changed) {
		tree->priv->state_changed = FALSE;
		e_tree_state_change (tree);
	}
}

gboolean
e_tree_is_editing (ETree *tree)
{
	g_return_val_if_fail (E_IS_TREE (tree), FALSE);

	return tree->priv->item && e_table_item_is_editing (E_TABLE_ITEM (tree->priv->item));
}

void
e_tree_set_grouped_view (ETree *tree,
			 gboolean grouped_view)
{
	g_return_if_fail (E_IS_TREE (tree));

	if ((tree->priv->grouped_view ? 1 : 0) == (grouped_view ? 1 : 0))
		return;

	tree->priv->grouped_view = grouped_view;

	e_tree_update_full_header_grouped_view (tree);
}

gboolean
e_tree_get_grouped_view (ETree *tree)
{
	g_return_val_if_fail (E_IS_TREE (tree), FALSE);

	return tree->priv->grouped_view;
}

gboolean
e_tree_get_sort_children_ascending (ETree *tree)
{
	g_return_val_if_fail (E_IS_TREE (tree), FALSE);

	return tree->priv->sort_children_ascending;
}

void
e_tree_set_sort_children_ascending (ETree *tree,
				    gboolean sort_children_ascending)
{
	g_return_if_fail (E_IS_TREE (tree));

	if ((tree->priv->sort_children_ascending ? 1 : 0) == (sort_children_ascending ? 1 : 0))
		return;

	tree->priv->sort_children_ascending = sort_children_ascending;

	g_object_notify (G_OBJECT (tree), "sort-children-ascending");
}

void
e_tree_customize_view (ETree *tree)
{
	GnomeCanvasItem *header_item;

	g_return_if_fail (E_IS_TREE (tree));

	header_item = e_tree_get_header_item (tree);

	if (header_item)
		e_table_header_item_customize_view (E_TABLE_HEADER_ITEM (header_item));
}
