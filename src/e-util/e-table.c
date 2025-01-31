/*
 * e-table.c - A graphical view of a Table.
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
 *		Miguel de Icaza <miguel@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-table.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>

#include <libgnomecanvas/libgnomecanvas.h>

#include "e-canvas-background.h"
#include "e-canvas-utils.h"
#include "e-canvas-vbox.h"
#include "e-canvas.h"
#include "e-table-click-to-add.h"
#include "e-table-column-specification.h"
#include "e-table-group-leaf.h"
#include "e-table-header-item.h"
#include "e-table-header-utils.h"
#include "e-table-subset.h"
#include "e-table-utils.h"
#include "e-text.h"
#include "e-unicode.h"
#include "e-misc-utils.h"
#include "gal-a11y-e-table.h"

#define COLUMN_HEADER_HEIGHT 16

#define d(x)

#if d(!)0
#define e_table_item_leave_edit_(x) \
	(e_table_item_leave_edit ((x)), \
	 g_print ("%s: e_table_item_leave_edit\n", G_STRFUNC))
#else
#define e_table_item_leave_edit_(x) (e_table_item_leave_edit((x)))
#endif

struct _ETablePrivate {
	GnomeCanvasItem *info_text;
	guint info_text_resize_id;
};

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

	TABLE_DRAG_BEGIN,
	TABLE_DRAG_END,
	TABLE_DRAG_DATA_GET,
	TABLE_DRAG_DATA_DELETE,

	TABLE_DRAG_LEAVE,
	TABLE_DRAG_MOTION,
	TABLE_DRAG_DROP,
	TABLE_DRAG_DATA_RECEIVED,

	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_LENGTH_THRESHOLD,
	PROP_MODEL,
	PROP_UNIFORM_ROW_HEIGHT,
	PROP_ALWAYS_SEARCH,
	PROP_USE_CLICK_TO_ADD,
	PROP_HADJUSTMENT,
	PROP_VADJUSTMENT,
	PROP_HSCROLL_POLICY,
	PROP_VSCROLL_POLICY,
	PROP_IS_EDITING
};

enum {
	ET_SCROLL_UP = 1 << 0,
	ET_SCROLL_DOWN = 1 << 1,
	ET_SCROLL_LEFT = 1 << 2,
	ET_SCROLL_RIGHT = 1 << 3
};

static guint et_signals[LAST_SIGNAL] = { 0 };

static void e_table_fill_table (ETable *e_table, ETableModel *model);
static gboolean changed_idle (gpointer data);

static void et_grab_focus (GtkWidget *widget);

static void et_drag_begin (GtkWidget *widget,
			   GdkDragContext *context,
			   ETable *et);
static void et_drag_end (GtkWidget *widget,
			 GdkDragContext *context,
			 ETable *et);
static void et_drag_data_get (GtkWidget *widget,
			     GdkDragContext *context,
			     GtkSelectionData *selection_data,
			     guint info,
			     guint time,
			     ETable *et);
static void et_drag_data_delete (GtkWidget *widget,
				GdkDragContext *context,
				ETable *et);

static void et_drag_leave (GtkWidget *widget,
			  GdkDragContext *context,
			  guint time,
			  ETable *et);
static gboolean et_drag_motion (GtkWidget *widget,
			       GdkDragContext *context,
			       gint x,
			       gint y,
			       guint time,
			       ETable *et);
static gboolean et_drag_drop (GtkWidget *widget,
			     GdkDragContext *context,
			     gint x,
			     gint y,
			     guint time,
			     ETable *et);
static void et_drag_data_received (GtkWidget *widget,
				  GdkDragContext *context,
				  gint x,
				  gint y,
				  GtkSelectionData *selection_data,
				  guint info,
				  guint time,
				  ETable *et);

static gint et_focus (GtkWidget *container, GtkDirectionType direction);

static void scroll_off (ETable *et);
static void scroll_on (ETable *et, guint scroll_direction);

static void e_table_scrollable_init (GtkScrollableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ETable, e_table, GTK_TYPE_GRID,
			 G_ADD_PRIVATE (ETable)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, e_table_scrollable_init))

static void
et_disconnect_model (ETable *et)
{
	if (et->model == NULL)
		return;

	if (et->table_model_change_id != 0)
		g_signal_handler_disconnect (
			et->model, et->table_model_change_id);
	if (et->table_row_change_id != 0)
		g_signal_handler_disconnect (
			et->model, et->table_row_change_id);
	if (et->table_cell_change_id != 0)
		g_signal_handler_disconnect (
			et->model, et->table_cell_change_id);
	if (et->table_rows_inserted_id != 0)
		g_signal_handler_disconnect (
			et->model, et->table_rows_inserted_id);
	if (et->table_rows_deleted_id != 0)
		g_signal_handler_disconnect (
			et->model, et->table_rows_deleted_id);

	et->table_model_change_id = 0;
	et->table_row_change_id = 0;
	et->table_cell_change_id = 0;
	et->table_rows_inserted_id = 0;
	et->table_rows_deleted_id = 0;
}

static void
e_table_state_change (ETable *et)
{
	if (et->state_change_freeze)
		et->state_changed = TRUE;
	else
		g_signal_emit (et, et_signals[STATE_CHANGE], 0);
}

#define CHECK_HORIZONTAL(et) \
	if ((et)->horizontal_scrolling || (et)->horizontal_resize) \
		e_table_header_update_horizontal (et->header);

static void
clear_current_search_col (ETable *et)
{
	et->search_col_set = FALSE;
}

static ETableCol *
current_search_col (ETable *et)
{
	if (!et->search_col_set) {
		et->current_search_col =
			e_table_util_calculate_current_search_col (
				et->header,
				et->full_header,
				et->sort_info,
				et->always_search);
		et->search_col_set = TRUE;
	}

	return et->current_search_col;
}

static void
et_get_preferred_width (GtkWidget *widget,
                        gint *minimum,
                        gint *natural)
{
	ETable *et = E_TABLE (widget);

	GTK_WIDGET_CLASS (e_table_parent_class)->
		get_preferred_width (widget, minimum, natural);

	if (et->horizontal_resize) {
		*minimum = MAX (*minimum, et->header_width);
		*natural = MAX (*natural, et->header_width);
	}
}

static void
et_get_preferred_height (GtkWidget *widget,
                         gint *minimum,
                         gint *natural)
{
	GTK_WIDGET_CLASS (e_table_parent_class)->
		get_preferred_height (widget, minimum, natural);
}

static void
set_header_width (ETable *et)
{
	if (et->horizontal_resize) {
		et->header_width = e_table_header_min_width (et->header);
		gtk_widget_queue_resize (GTK_WIDGET (et));
	}
}

static void
structure_changed (ETableHeader *header,
                   ETable *et)
{
	e_table_state_change (et);
	set_header_width (et);
	clear_current_search_col (et);
}

static void
expansion_changed (ETableHeader *header,
                   ETable *et)
{
	e_table_state_change (et);
	set_header_width (et);
}

static void
dimension_changed (ETableHeader *header,
                   gint total_width,
                   ETable *et)
{
	set_header_width (et);
}

static void
disconnect_header (ETable *e_table)
{
	if (e_table->header == NULL)
		return;

	if (e_table->structure_change_id)
		g_signal_handler_disconnect (
			e_table->header, e_table->structure_change_id);
	if (e_table->expansion_change_id)
		g_signal_handler_disconnect (
			e_table->header, e_table->expansion_change_id);
	if (e_table->dimension_change_id)
		g_signal_handler_disconnect (
			e_table->header, e_table->dimension_change_id);

	g_object_unref (e_table->header);
	e_table->header = NULL;
}

static void
connect_header (ETable *e_table,
                ETableState *state)
{
	if (e_table->header != NULL)
		disconnect_header (e_table);

	e_table->header = e_table_state_to_header (
		GTK_WIDGET (e_table), e_table->full_header, state);

	e_table->structure_change_id = g_signal_connect (
		e_table->header, "structure_change",
		G_CALLBACK (structure_changed), e_table);
	e_table->expansion_change_id = g_signal_connect (
		e_table->header, "expansion_change",
		G_CALLBACK (expansion_changed), e_table);
	e_table->dimension_change_id = g_signal_connect (
		e_table->header, "dimension_change",
		G_CALLBACK (dimension_changed), e_table);
}

static void
et_dispose (GObject *object)
{
	ETable *et = E_TABLE (object);

	et_disconnect_model (et);

	if (et->priv->info_text != NULL) {
		g_object_run_dispose (G_OBJECT (et->priv->info_text));
		et->priv->info_text = NULL;
	}
	et->priv->info_text_resize_id = 0;

	if (et->search) {
		if (et->search_search_id)
			g_signal_handler_disconnect (
				et->search, et->search_search_id);
		if (et->search_accept_id)
			g_signal_handler_disconnect (
				et->search, et->search_accept_id);
		g_object_unref (et->search);
		et->search = NULL;
	}

	if (et->group_info_change_id) {
		g_signal_handler_disconnect (
			et->sort_info, et->group_info_change_id);
		et->group_info_change_id = 0;
	}

	if (et->sort_info_change_id) {
		g_signal_handler_disconnect (
			et->sort_info, et->sort_info_change_id);
		et->sort_info_change_id = 0;
	}

	if (et->reflow_idle_id) {
		g_source_remove (et->reflow_idle_id);
		et->reflow_idle_id = 0;
	}

	scroll_off (et);

	disconnect_header (et);

	g_clear_object (&et->model);
	g_clear_object (&et->full_header);
	g_clear_object (&et->sort_info);
	g_clear_object (&et->sorter);
	g_clear_object (&et->selection);
	g_clear_object (&et->spec);

	if (et->header_canvas != NULL) {
		gtk_widget_destroy (GTK_WIDGET (et->header_canvas));
		et->header_canvas = NULL;
	}

	if (et->site != NULL) {
		e_table_drag_source_unset (et);
		et->site = NULL;
	}

	if (et->table_canvas != NULL) {
		gtk_widget_destroy (GTK_WIDGET (et->table_canvas));
		et->table_canvas = NULL;
	}

	if (et->rebuild_idle_id != 0) {
		g_source_remove (et->rebuild_idle_id);
		et->rebuild_idle_id = 0;
	}

	g_free (et->click_to_add_message);
	et->click_to_add_message = NULL;

	g_free (et->domain);
	et->domain = NULL;

	G_OBJECT_CLASS (e_table_parent_class)->dispose (object);
}

static void
et_unrealize (GtkWidget *widget)
{
	scroll_off (E_TABLE (widget));

	if (GTK_WIDGET_CLASS (e_table_parent_class)->unrealize)
		GTK_WIDGET_CLASS (e_table_parent_class)->unrealize (widget);
}

static gboolean
check_row (ETable *et,
           gint model_row,
           gint col,
           ETableSearchFunc search,
           gchar *string)
{
	gconstpointer value;

	value = e_table_model_value_at (et->model, col, model_row);

	return search (value, string);
}

static gboolean
et_search_search (ETableSearch *search,
                  gchar *string,
                  ETableSearchFlags flags,
                  ETable *et)
{
	gint cursor;
	gint rows;
	gint i;
	ETableCol *col = current_search_col (et);

	if (col == NULL)
		return FALSE;

	rows = e_table_model_row_count (et->model);

	g_object_get (
		et->selection,
		"cursor_row", &cursor,
		NULL);

	if ((flags & E_TABLE_SEARCH_FLAGS_CHECK_CURSOR_FIRST) &&
		cursor < rows && cursor >= 0 &&
		check_row (et, cursor, col->spec->model_col, col->search, string))
		return TRUE;

	cursor = e_sorter_model_to_sorted (E_SORTER (et->sorter), cursor);

	for (i = cursor + 1; i < rows; i++) {
		gint model_row = e_sorter_sorted_to_model (E_SORTER (et->sorter), i);
		if (check_row (et, model_row, col->spec->model_col, col->search, string)) {
			e_selection_model_select_as_key_press (
				E_SELECTION_MODEL (et->selection),
				model_row, col->spec->model_col,
				GDK_CONTROL_MASK);
			return TRUE;
		}
	}

	for (i = 0; i < cursor; i++) {
		gint model_row = e_sorter_sorted_to_model (E_SORTER (et->sorter), i);
		if (check_row (et, model_row, col->spec->model_col, col->search, string)) {
			e_selection_model_select_as_key_press (
				E_SELECTION_MODEL (et->selection),
				model_row, col->spec->model_col,
				GDK_CONTROL_MASK);
			return TRUE;
		}
	}

	cursor = e_sorter_sorted_to_model (E_SORTER (et->sorter), cursor);

	/* Check if the cursor row is the only matching row. */
	return (!(flags & E_TABLE_SEARCH_FLAGS_CHECK_CURSOR_FIRST) &&
		cursor < rows && cursor >= 0 &&
		check_row (et, cursor, col->spec->model_col, col->search, string));
}

static void
et_search_accept (ETableSearch *search,
                  ETable *et)
{
	gint cursor;
	ETableCol *col = current_search_col (et);

	if (col == NULL)
		return;

	g_object_get (et->selection, "cursor_row", &cursor, NULL);

	e_selection_model_select_as_key_press (
		E_SELECTION_MODEL (et->selection),
		cursor, col->spec->model_col, 0);
}

static void
init_search (ETable *e_table)
{
	if (e_table->search != NULL)
		return;

	e_table->search = e_table_search_new ();

	e_table->search_search_id = g_signal_connect (
		e_table->search, "search",
		G_CALLBACK (et_search_search), e_table);
	e_table->search_accept_id = g_signal_connect (
		e_table->search, "accept",
		G_CALLBACK (et_search_accept), e_table);
}

static void
et_finalize (GObject *object)
{
	ETable *et = E_TABLE (object);

	g_free (et->click_to_add_message);
	et->click_to_add_message = NULL;

	g_free (et->domain);
	et->domain = NULL;

	G_OBJECT_CLASS (e_table_parent_class)->finalize (object);
}

static void
e_table_init (ETable *e_table)
{
	e_table->priv = e_table_get_instance_private (e_table);

	gtk_widget_set_can_focus (GTK_WIDGET (e_table), TRUE);

	e_table->sort_info = NULL;
	e_table->group_info_change_id = 0;
	e_table->sort_info_change_id = 0;
	e_table->structure_change_id = 0;
	e_table->expansion_change_id = 0;
	e_table->dimension_change_id = 0;
	e_table->reflow_idle_id = 0;
	e_table->scroll_idle_id = 0;

	e_table->alternating_row_colors = 1;
	e_table->horizontal_draw_grid = 1;
	e_table->vertical_draw_grid = 1;
	e_table->draw_focus = 1;
	e_table->cursor_mode = E_CURSOR_SIMPLE;
	e_table->length_threshold = 200;
	e_table->uniform_row_height = FALSE;

	e_table->need_rebuild = 0;
	e_table->rebuild_idle_id = 0;

	e_table->horizontal_scrolling = FALSE;
	e_table->horizontal_resize = FALSE;

	e_table->click_to_add_message = NULL;
	e_table->domain = NULL;

	e_table->drop_row = -1;
	e_table->drop_col = -1;
	e_table->site = NULL;

	e_table->do_drag = 0;

	e_table->sorter = NULL;
	e_table->selection = e_table_selection_model_new ();
	e_table->cursor_loc = E_TABLE_CURSOR_LOC_NONE;
	e_table->spec = NULL;

	e_table->always_search = g_getenv ("GAL_ALWAYS_SEARCH") ? TRUE : FALSE;

	e_table->search = NULL;
	e_table->search_search_id = 0;
	e_table->search_accept_id = 0;

	e_table->current_search_col = NULL;

	e_table->header_width = 0;

	e_table->state_changed = FALSE;
	e_table->state_change_freeze = 0;
}

/* Grab_focus handler for the ETable */
static void
et_grab_focus (GtkWidget *widget)
{
	ETable *e_table;

	e_table = E_TABLE (widget);

	gtk_widget_grab_focus (GTK_WIDGET (e_table->table_canvas));
}

/* Focus handler for the ETable */
static gint
et_focus (GtkWidget *container,
          GtkDirectionType direction)
{
	ETable *e_table;

	e_table = E_TABLE (container);

	if (gtk_container_get_focus_child (GTK_CONTAINER (container))) {
		gtk_container_set_focus_child (GTK_CONTAINER (container), NULL);
		return FALSE;
	}

	return gtk_widget_child_focus (GTK_WIDGET (e_table->table_canvas), direction);
}

static void
set_header_canvas_width (ETable *e_table)
{
	gdouble oldwidth, oldheight, width;

	if (!(e_table->header_item && e_table->header_canvas && e_table->table_canvas))
		return;

	gnome_canvas_get_scroll_region (
		GNOME_CANVAS (e_table->table_canvas),
		NULL, NULL, &width, NULL);
	gnome_canvas_get_scroll_region (
		GNOME_CANVAS (e_table->header_canvas),
		NULL, NULL, &oldwidth, &oldheight);

	if (oldwidth != width ||
	    oldheight != E_TABLE_HEADER_ITEM (e_table->header_item)->height - 1)
		gnome_canvas_set_scroll_region (
			GNOME_CANVAS (e_table->header_canvas),
			0, 0, width, /*  COLUMN_HEADER_HEIGHT - 1 */
			E_TABLE_HEADER_ITEM (e_table->header_item)->height - 1);

}

static void
header_canvas_size_allocate (GtkWidget *widget,
                             GtkAllocation *alloc,
                             ETable *e_table)
{
	GtkAllocation allocation;

	set_header_canvas_width (e_table);

	gtk_widget_get_allocation (
		GTK_WIDGET (e_table->header_canvas), &allocation);

	/* When the header item is created ->height == 0,
	 * as the font is only created when everything is realized.
	 * So we set the usize here as well, so that the size of the
	 * header is correct */
	if (allocation.height != E_TABLE_HEADER_ITEM (e_table->header_item)->height)
		g_object_set (
			e_table->header_canvas, "height-request",
			E_TABLE_HEADER_ITEM (e_table->header_item)->height,
			NULL);
}

static void
group_info_changed (ETableSortInfo *info,
                    ETable *et)
{
	gboolean will_be_grouped = e_table_sort_info_grouping_get_count (info) > 0;
	clear_current_search_col (et);
	if (et->is_grouped || will_be_grouped) {
		et->need_rebuild = TRUE;
		if (!et->rebuild_idle_id) {
			g_object_run_dispose (G_OBJECT (et->group));
			et->group = NULL;
			et->rebuild_idle_id = g_idle_add_full (20, changed_idle, et, NULL);
		}
	}
	e_table_state_change (et);
}

static void
sort_info_changed (ETableSortInfo *info,
                   ETable *et)
{
	clear_current_search_col (et);
	e_table_state_change (et);
}

static void
e_table_setup_header (ETable *e_table)
{
	gchar *pointer;
	e_table->header_canvas = GNOME_CANVAS (e_canvas_new ());
	gtk_widget_set_hexpand (GTK_WIDGET (e_table->header_canvas), TRUE);
	gtk_style_context_add_class (
		gtk_widget_get_style_context (GTK_WIDGET (e_table->header_canvas)),
		"table-header");

	gtk_widget_show (GTK_WIDGET (e_table->header_canvas));

	pointer = g_strdup_printf ("%p", (gpointer) e_table);

	e_table->header_item = gnome_canvas_item_new (
		gnome_canvas_root (e_table->header_canvas),
		e_table_header_item_get_type (),
		"ETableHeader", e_table->header,
		"full_header", e_table->full_header,
		"sort_info", e_table->sort_info,
		"dnd_code", pointer,
		"table", e_table,
		NULL);

	g_free (pointer);

	g_signal_connect (
		e_table->header_canvas, "size_allocate",
		G_CALLBACK (header_canvas_size_allocate), e_table);

	g_object_set (
		e_table->header_canvas, "height-request",
		E_TABLE_HEADER_ITEM (e_table->header_item)->height, NULL);
}

static gboolean
table_canvas_reflow_idle (ETable *e_table)
{
	gdouble height, width;
	gdouble oldheight, oldwidth;
	GtkAllocation allocation;

	gtk_widget_get_allocation (
		GTK_WIDGET (e_table->table_canvas), &allocation);

	g_object_get (
		e_table->canvas_vbox,
		"height", &height, "width", &width, NULL);
	height = MAX ((gint) height, allocation.height);
	width = MAX ((gint) width, allocation.width);
	/* I have no idea why this needs to be -1, but it works. */
	gnome_canvas_get_scroll_region (
		GNOME_CANVAS (e_table->table_canvas),
		NULL, NULL, &oldwidth, &oldheight);

	if (oldwidth != width - 1 ||
	    oldheight != height - 1) {
		gnome_canvas_set_scroll_region (
			GNOME_CANVAS (e_table->table_canvas),
			0, 0, width - 1, height - 1);
		set_header_canvas_width (e_table);
	}
	e_table->reflow_idle_id = 0;
	return FALSE;
}

static void
table_canvas_size_allocate (GtkWidget *widget,
                            GtkAllocation *alloc,
                            ETable *e_table)
{
	gdouble width;
	gdouble height;
	GValue *val = g_new0 (GValue, 1);
	g_value_init (val, G_TYPE_DOUBLE);

	width = alloc->width;
	g_value_set_double (val, width);
	g_object_get (
		e_table->canvas_vbox,
		"height", &height,
		NULL);
	height = MAX ((gint) height, alloc->height);

	g_object_set (
		e_table->canvas_vbox,
		"width", width,
		NULL);
	g_object_set_property (G_OBJECT (e_table->header), "width", val);
	g_free (val);
	if (e_table->reflow_idle_id)
		g_source_remove (e_table->reflow_idle_id);
	table_canvas_reflow_idle (e_table);

	e_table->size_allocated = TRUE;

	if (e_table->need_rebuild && !e_table->rebuild_idle_id)
		e_table->rebuild_idle_id = g_idle_add_full (20, changed_idle, e_table, NULL);
}

static void
table_canvas_reflow (GnomeCanvas *canvas,
                     ETable *e_table)
{
	if (!e_table->reflow_idle_id)
		e_table->reflow_idle_id = g_idle_add_full (
			400, (GSourceFunc) table_canvas_reflow_idle,
			e_table, NULL);
}

static void
click_to_add_cursor_change (ETableClickToAdd *etcta,
                            gint row,
                            gint col,
                            ETable *et)
{
	if (et->cursor_loc == E_TABLE_CURSOR_LOC_TABLE) {
		e_selection_model_clear (E_SELECTION_MODEL (et->selection));
	}
	et->cursor_loc = E_TABLE_CURSOR_LOC_ETCTA;
}

static void
group_cursor_change (ETableGroup *etg,
                     gint row,
                     ETable *et)
{
	ETableCursorLoc old_cursor_loc;

	old_cursor_loc = et->cursor_loc;

	et->cursor_loc = E_TABLE_CURSOR_LOC_TABLE;
	g_signal_emit (et, et_signals[CURSOR_CHANGE], 0, row);

	if (old_cursor_loc == E_TABLE_CURSOR_LOC_ETCTA && et->click_to_add)
		e_table_click_to_add_commit (E_TABLE_CLICK_TO_ADD (et->click_to_add));
}

static void
group_cursor_activated (ETableGroup *etg,
                        gint row,
                        ETable *et)
{
	g_signal_emit (et, et_signals[CURSOR_ACTIVATED], 0, row);
}

static void
group_double_click (ETableGroup *etg,
                    gint row,
                    gint col,
                    GdkEvent *event,
                    ETable *et)
{
	g_signal_emit (et, et_signals[DOUBLE_CLICK], 0, row, col, event);
}

static gboolean
group_right_click (ETableGroup *etg,
                   gint row,
                   gint col,
                   GdkEvent *event,
                   ETable *et)
{
	gboolean return_val = FALSE;

	g_signal_emit (
		et, et_signals[RIGHT_CLICK], 0,
		row, col, event, &return_val);

	return return_val;
}

static gboolean
group_click (ETableGroup *etg,
             gint row,
             gint col,
             GdkEvent *event,
             ETable *et)
{
	gboolean return_val = 0;

	g_signal_emit (
		et, et_signals[CLICK], 0,
		row, col, event, &return_val);

	return return_val;
}

static gboolean
group_key_press (ETableGroup *etg,
                 gint row,
                 gint col,
                 GdkEvent *event,
                 ETable *et)
{
	gboolean return_val = FALSE;
	GdkEventKey *key = (GdkEventKey *) event;
	gint y, row_local = -1, col_local = -1;
	GtkAdjustment *adjustment;
	GtkScrollable *scrollable;
	gdouble page_size;
	gdouble upper;
	gdouble value;

	scrollable = GTK_SCROLLABLE (et->table_canvas);
	adjustment = gtk_scrollable_get_vadjustment (scrollable);

	switch (key->keyval) {
	case GDK_KEY_Page_Down:
	case GDK_KEY_KP_Page_Down:
		page_size = gtk_adjustment_get_page_size (adjustment);
		upper = gtk_adjustment_get_upper (adjustment);
		value = gtk_adjustment_get_value (adjustment);

		y = CLAMP (value + (2 * page_size - 50), 0, upper);
		y -= value;
		e_table_get_cell_at (et, 30, y, &row_local, &col_local);

		if (row_local == -1)
			row_local = e_table_model_row_count (et->model) - 1;

		row_local = e_table_view_to_model_row (et, row_local);
		col_local = e_selection_model_cursor_col (E_SELECTION_MODEL (et->selection));
		e_selection_model_select_as_key_press (
			E_SELECTION_MODEL (et->selection),
			row_local, col_local, key->state);
		return_val = 1;
		break;
	case GDK_KEY_Page_Up:
	case GDK_KEY_KP_Page_Up:
		page_size = gtk_adjustment_get_page_size (adjustment);
		upper = gtk_adjustment_get_upper (adjustment);
		value = gtk_adjustment_get_value (adjustment);

		y = CLAMP (value - (page_size - 50), 0, upper);
		y -= value;
		e_table_get_cell_at (et, 30, y, &row_local, &col_local);

		if (row_local == -1)
			row_local = 0;

		row_local = e_table_view_to_model_row (et, row_local);
		col_local = e_selection_model_cursor_col (E_SELECTION_MODEL (et->selection));
		e_selection_model_select_as_key_press (
			E_SELECTION_MODEL (et->selection),
			row_local, col_local, key->state);
		return_val = 1;
		break;
	case GDK_KEY_BackSpace:
		init_search (et);
		if (e_table_search_backspace (et->search))
			return TRUE;
		/* Fall through */
	default:
		init_search (et);
		if ((key->state & ~(GDK_SHIFT_MASK | GDK_LOCK_MASK |
			GDK_MOD1_MASK | GDK_MOD2_MASK | GDK_MOD3_MASK |
			GDK_MOD4_MASK | GDK_MOD5_MASK)) == 0
		    && ((key->keyval >= GDK_KEY_a && key->keyval <= GDK_KEY_z) ||
			(key->keyval >= GDK_KEY_A && key->keyval <= GDK_KEY_Z) ||
			(key->keyval >= GDK_KEY_0 && key->keyval <= GDK_KEY_9)))
			e_table_search_input_character (et->search, key->keyval);
		g_signal_emit (
			et, et_signals[KEY_PRESS], 0,
			row, col, event, &return_val);
		break;
	}
	return return_val;
}

static gboolean
group_start_drag (ETableGroup *etg,
                  gint row,
                  gint col,
                  GdkEvent *event,
                  ETable *et)
{
	gboolean return_val = TRUE;

	g_signal_emit (
		et, et_signals[START_DRAG], 0,
		row, col, event, &return_val);

	return return_val;
}

static void
et_table_model_changed (ETableModel *model,
                        ETable *et)
{
	et->need_rebuild = TRUE;
	if (!et->rebuild_idle_id) {
		g_object_run_dispose (G_OBJECT (et->group));
		et->group = NULL;
		et->rebuild_idle_id = g_idle_add_full (20, changed_idle, et, NULL);
	}
}

static void
et_table_row_changed (ETableModel *table_model,
                      gint row,
                      ETable *et)
{
	if (!et->need_rebuild) {
		if (e_table_group_remove (et->group, row))
			e_table_group_add (et->group, row);
		CHECK_HORIZONTAL (et);
	}
}

static void
et_table_cell_changed (ETableModel *table_model,
                       gint view_col,
                       gint row,
                       ETable *et)
{
	et_table_row_changed (table_model, row, et);
}

static void
et_table_rows_inserted (ETableModel *table_model,
                        gint row,
                        gint count,
                        ETable *et)
{
	/* This number has already been decremented. */
	gint row_count = e_table_model_row_count (table_model);
	if (!et->need_rebuild) {
		gint i;
		if (row != row_count - count)
			e_table_group_increment (et->group, row, count);
		for (i = 0; i < count; i++)
			e_table_group_add (et->group, row + i);
		CHECK_HORIZONTAL (et);
	}
}

static void
et_table_rows_deleted (ETableModel *table_model,
                       gint row,
                       gint count,
                       ETable *et)
{
	gint row_count = e_table_model_row_count (table_model);
	if (!et->need_rebuild) {
		gint i;
		for (i = 0; i < count; i++)
			e_table_group_remove (et->group, row + i);
		if (row != row_count)
			e_table_group_decrement (et->group, row, count);
		CHECK_HORIZONTAL (et);
	}
}

static void
group_is_editing_changed_cb (ETableClickToAdd *etcta,
                             GParamSpec *param,
                             ETable *table)
{
	g_return_if_fail (E_IS_TABLE (table));

	g_object_notify (G_OBJECT (table), "is-editing");
}

static void
et_build_groups (ETable *et)
{
	gboolean was_grouped = et->is_grouped;
	gboolean alternating_row_colors;

	et->is_grouped = e_table_sort_info_grouping_get_count (et->sort_info) > 0;

	et->group = e_table_group_new (
		GNOME_CANVAS_GROUP (et->canvas_vbox),
		et->full_header,
		et->header,
		et->model,
		et->sort_info,
		0);

	if (et->use_click_to_add_end)
		e_canvas_vbox_add_item_start (
			E_CANVAS_VBOX (et->canvas_vbox),
			GNOME_CANVAS_ITEM (et->group));
	else
		e_canvas_vbox_add_item (
			E_CANVAS_VBOX (et->canvas_vbox),
			GNOME_CANVAS_ITEM (et->group));

	alternating_row_colors = et->alternating_row_colors;
	if (alternating_row_colors) {
		gboolean bvalue = TRUE;

		/* user can only disable this option, if it's enabled by the specification */
		gtk_widget_style_get (GTK_WIDGET (et), "alternating-row-colors", &bvalue, NULL);

		alternating_row_colors = bvalue ? 1 : 0;
	}

	gnome_canvas_item_set (
		GNOME_CANVAS_ITEM (et->group),
		"alternating_row_colors", alternating_row_colors,
		"horizontal_draw_grid", et->horizontal_draw_grid,
		"vertical_draw_grid", et->vertical_draw_grid,
		"drawfocus", et->draw_focus,
		"cursor_mode", et->cursor_mode,
		"length_threshold", et->length_threshold,
		"uniform_row_height", et->uniform_row_height && !et->is_grouped,
		"selection_model", et->selection,
		NULL);

	g_signal_connect (
		et->group, "cursor_change",
		G_CALLBACK (group_cursor_change), et);
	g_signal_connect (
		et->group, "cursor_activated",
		G_CALLBACK (group_cursor_activated), et);
	g_signal_connect (
		et->group, "double_click",
		G_CALLBACK (group_double_click), et);
	g_signal_connect (
		et->group, "right_click",
		G_CALLBACK (group_right_click), et);
	g_signal_connect (
		et->group, "click",
		G_CALLBACK (group_click), et);
	g_signal_connect (
		et->group, "key_press",
		G_CALLBACK (group_key_press), et);
	g_signal_connect (
		et->group, "start_drag",
		G_CALLBACK (group_start_drag), et);
	e_signal_connect_notify (
		et->group, "notify::is-editing",
		G_CALLBACK (group_is_editing_changed_cb), et);

	if (!(et->is_grouped) && was_grouped)
		et_disconnect_model (et);

	if (et->is_grouped && (!was_grouped)) {
		et->table_model_change_id = g_signal_connect (
			et->model, "model_changed",
			G_CALLBACK (et_table_model_changed), et);

		et->table_row_change_id = g_signal_connect (
			et->model, "model_row_changed",
			G_CALLBACK (et_table_row_changed), et);

		et->table_cell_change_id = g_signal_connect (
			et->model, "model_cell_changed",
			G_CALLBACK (et_table_cell_changed), et);

		et->table_rows_inserted_id = g_signal_connect (
			et->model, "model_rows_inserted",
			G_CALLBACK (et_table_rows_inserted), et);

		et->table_rows_deleted_id = g_signal_connect (
			et->model, "model_rows_deleted",
			G_CALLBACK (et_table_rows_deleted), et);

	}

	if (et->is_grouped)
		e_table_fill_table (et, et->model);
}

static gboolean
changed_idle (gpointer data)
{
	ETable *et = E_TABLE (data);

	/* Wait until we have a valid size allocation. */
	if (et->need_rebuild && et->size_allocated) {
		GtkWidget *widget;
		GtkAllocation allocation;

		if (et->group)
			g_object_run_dispose (G_OBJECT (et->group));
		et_build_groups (et);

		widget = GTK_WIDGET (et->table_canvas);
		gtk_widget_get_allocation (widget, &allocation);

		g_object_set (
			et->canvas_vbox,
			"width", (gdouble) allocation.width,
			NULL);

		table_canvas_size_allocate (widget, &allocation, et);

		et->need_rebuild = 0;
	}

	et->rebuild_idle_id = 0;

	CHECK_HORIZONTAL (et);

	return FALSE;
}

static void
et_canvas_style_updated (GtkWidget *widget)
{
	GdkRGBA color;

	GTK_WIDGET_CLASS (e_table_parent_class)->style_updated (widget);

	e_utils_get_theme_color (widget, "theme_base_color", E_UTILS_DEFAULT_THEME_BASE_COLOR, &color);

	gnome_canvas_item_set (
		E_TABLE (widget)->white_item,
		"fill-color", &color,
		NULL);
}

static void
et_canvas_realize (GtkWidget *canvas,
                   ETable *e_table)
{
	GtkWidget *widget;
	GdkRGBA color;

	widget = GTK_WIDGET (e_table->table_canvas);

	e_utils_get_theme_color (widget, "theme_base_color", E_UTILS_DEFAULT_THEME_BASE_COLOR, &color);

	gnome_canvas_item_set (
		e_table->white_item,
		"fill-color", &color,
		NULL);

	CHECK_HORIZONTAL (e_table);
	set_header_width (e_table);
}

static ETableItem *
get_first_etable_item (ETableGroup *table_group)
{
	ETableItem *res = NULL;
	GnomeCanvasGroup *group;
	GList *link;

	g_return_val_if_fail (E_IS_TABLE_GROUP (table_group), NULL);

	group = GNOME_CANVAS_GROUP (table_group);
	g_return_val_if_fail (group != NULL, NULL);

	for (link = group->item_list; link && !res; link = g_list_next (link)) {
		GnomeCanvasItem *item;

		item = GNOME_CANVAS_ITEM (link->data);

		if (E_IS_TABLE_GROUP (item))
			res = get_first_etable_item (E_TABLE_GROUP (item));
		else if (E_IS_TABLE_ITEM (item)) {
			res = E_TABLE_ITEM (item);
		}
	}

	return res;
}

/* Finds the first descendant of the group that is an ETableItem and focuses it */
static void
focus_first_etable_item (ETableGroup *group)
{
	ETableItem *item;

	item = get_first_etable_item (group);
	if (item) {
		e_table_item_set_cursor (item, 0, 0);
		gnome_canvas_item_grab_focus (GNOME_CANVAS_ITEM (item));
	}
}

static gboolean
white_item_event (GnomeCanvasItem *white_item,
                  GdkEvent *event,
                  ETable *e_table)
{
	gboolean return_val = FALSE;

	g_signal_emit (
		e_table, et_signals[WHITE_SPACE_EVENT], 0,
		event, &return_val);

	if (!return_val && event && e_table->group) {
		guint event_button = 0;

		gdk_event_get_button (event, &event_button);

		if (event->type == GDK_BUTTON_PRESS && (event_button == 1 || event_button == 2)) {
			focus_first_etable_item (e_table->group);
			return_val = TRUE;
		}
	}

	return return_val;
}

static void
et_eti_leave_edit (ETable *et)
{
	GnomeCanvas *canvas = et->table_canvas;

	if (gtk_widget_has_focus (GTK_WIDGET (canvas))) {
		GnomeCanvasItem *item = GNOME_CANVAS (canvas)->focused_item;

		if (E_IS_TABLE_ITEM (item)) {
			e_table_item_leave_edit_(E_TABLE_ITEM (item));
		}
	}
}

static gint
et_canvas_root_event (GnomeCanvasItem *root,
                      GdkEvent *event,
                      ETable *e_table)
{
	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
		if (event->button.button != 4 && event->button.button != 5) {
			et_eti_leave_edit (e_table);
			return TRUE;
		}
		break;
	default:
		break;
	}

	return FALSE;
}

/* Handler for focus events in the table_canvas; we have to repaint ourselves
 * always, and also give the focus to some ETableItem if we get focused.
 */
static gint
table_canvas_focus_event_cb (GtkWidget *widget,
                             GdkEventFocus *event,
                             gpointer data)
{
	GnomeCanvas *canvas;
	ECanvas *ecanvas;
	ETable *etable;

	gtk_widget_queue_draw (widget);
	canvas = GNOME_CANVAS (widget);
	ecanvas = E_CANVAS (widget);

	if (!event->in) {
		gtk_im_context_focus_out (ecanvas->im_context);
		return FALSE;
	} else {
		gtk_im_context_focus_in (ecanvas->im_context);
	}

	etable = E_TABLE (data);

	if (e_table_model_row_count (etable->model) < 1
	    && (etable->click_to_add)
	    && !(E_TABLE_CLICK_TO_ADD (etable->click_to_add)->row)) {
		gnome_canvas_item_grab_focus (etable->canvas_vbox);
		gnome_canvas_item_grab_focus (etable->click_to_add);
	} else if (!canvas->focused_item && etable->group) {
		focus_first_etable_item (etable->group);
	} else if (canvas->focused_item) {
		ESelectionModel *selection = (ESelectionModel *) etable->selection;

		/* check whether click_to_add already got the focus */
		if (etable->click_to_add) {
			GnomeCanvasItem *row = E_TABLE_CLICK_TO_ADD (etable->click_to_add)->row;
			if (canvas->focused_item == row)
				return TRUE;
		}

		if (e_selection_model_cursor_row (selection) == -1)
			focus_first_etable_item (etable->group);
	}

	return FALSE;
}

static gboolean
canvas_vbox_event (ECanvasVbox *vbox,
                   GdkEventKey *key,
                   ETable *etable)
{
	if (key->type != GDK_KEY_PRESS  &&
		key->type != GDK_KEY_RELEASE) {
		return FALSE;
	}
	switch (key->keyval) {
		case GDK_KEY_Tab:
		case GDK_KEY_KP_Tab:
		case GDK_KEY_ISO_Left_Tab:
			if ((key->state & GDK_CONTROL_MASK) && etable->click_to_add) {
				gnome_canvas_item_grab_focus (etable->click_to_add);
				return TRUE;
			}
			break;
		default:
			break;
	}

	return FALSE;
}

static void
click_to_add_is_editing_changed_cb (ETableClickToAdd *etcta,
                                    GParamSpec *param,
                                    ETable *table)
{
	g_return_if_fail (E_IS_TABLE (table));

	g_object_notify (G_OBJECT (table), "is-editing");
}

static gboolean
click_to_add_event (ETableClickToAdd *etcta,
                    GdkEventKey *key,
                    ETable *etable)
{
	if (key->type != GDK_KEY_PRESS  &&
		key->type != GDK_KEY_RELEASE) {
		return FALSE;
	}
	switch (key->keyval) {
		case GDK_KEY_Tab:
		case GDK_KEY_KP_Tab:
		case GDK_KEY_ISO_Left_Tab:
			if (key->state & GDK_CONTROL_MASK) {
				if (etable->group) {
					if (e_table_model_row_count (etable->model) > 0)
						focus_first_etable_item (etable->group);
					else
						gtk_widget_child_focus (
							gtk_widget_get_toplevel (
							GTK_WIDGET (etable->table_canvas)),
							GTK_DIR_TAB_FORWARD);
				}
			}
			break;
		default:
			break;
	}

	return FALSE;
}

static void
e_table_setup_table (ETable *e_table,
                     ETableHeader *full_header,
                     ETableHeader *header,
                     ETableModel *model)
{
	GtkWidget *widget;
	GdkRGBA color;

	e_table->table_canvas = GNOME_CANVAS (e_canvas_new ());
	gtk_widget_set_hexpand (GTK_WIDGET (e_table->table_canvas), TRUE);
	gtk_widget_set_vexpand (GTK_WIDGET (e_table->table_canvas), TRUE);
	g_signal_connect (
		e_table->table_canvas, "size_allocate",
		G_CALLBACK (table_canvas_size_allocate), e_table);
	g_signal_connect (
		e_table->table_canvas, "focus_in_event",
		G_CALLBACK (table_canvas_focus_event_cb), e_table);
	g_signal_connect (
		e_table->table_canvas, "focus_out_event",
		G_CALLBACK (table_canvas_focus_event_cb), e_table);

	g_signal_connect (
		e_table, "drag_begin",
		G_CALLBACK (et_drag_begin), e_table);
	g_signal_connect (
		e_table, "drag_end",
		G_CALLBACK (et_drag_end), e_table);
	g_signal_connect (
		e_table, "drag_data_get",
		G_CALLBACK (et_drag_data_get), e_table);
	g_signal_connect (
		e_table, "drag_data_delete",
		G_CALLBACK (et_drag_data_delete), e_table);
	g_signal_connect (
		e_table, "drag_motion",
		G_CALLBACK (et_drag_motion), e_table);
	g_signal_connect (
		e_table, "drag_leave",
		G_CALLBACK (et_drag_leave), e_table);
	g_signal_connect (
		e_table, "drag_drop",
		G_CALLBACK (et_drag_drop), e_table);
	g_signal_connect (
		e_table, "drag_data_received",
		G_CALLBACK (et_drag_data_received), e_table);

	g_signal_connect (
		e_table->table_canvas, "reflow",
		G_CALLBACK (table_canvas_reflow), e_table);

	widget = GTK_WIDGET (e_table->table_canvas);

	gtk_widget_show (widget);

	e_utils_get_theme_color (widget, "theme_base_color", E_UTILS_DEFAULT_THEME_BASE_COLOR, &color);

	e_table->white_item = gnome_canvas_item_new (
		gnome_canvas_root (e_table->table_canvas),
		e_canvas_background_get_type (),
		"fill-color", &color,
		NULL);

	g_signal_connect (
		e_table->white_item, "event",
		G_CALLBACK (white_item_event), e_table);

	g_signal_connect (
		e_table->table_canvas, "realize",
		G_CALLBACK (et_canvas_realize), e_table);

	g_signal_connect (
		gnome_canvas_root (e_table->table_canvas), "event",
		G_CALLBACK (et_canvas_root_event), e_table);

	e_table->canvas_vbox = gnome_canvas_item_new (
		gnome_canvas_root (e_table->table_canvas),
		e_canvas_vbox_get_type (),
		"spacing", 10.0,
		NULL);

	g_signal_connect (
		e_table->canvas_vbox, "event",
		G_CALLBACK (canvas_vbox_event), e_table);

	et_build_groups (e_table);

	if (e_table->use_click_to_add) {
		e_table->click_to_add = gnome_canvas_item_new (
			GNOME_CANVAS_GROUP (e_table->canvas_vbox),
			e_table_click_to_add_get_type (),
			"header", e_table->header,
			"model", e_table->model,
			"message", e_table->click_to_add_message,
			NULL);

		if (e_table->use_click_to_add_end)
			e_canvas_vbox_add_item (
				E_CANVAS_VBOX (e_table->canvas_vbox),
				e_table->click_to_add);
		else
			e_canvas_vbox_add_item_start (
				E_CANVAS_VBOX (e_table->canvas_vbox),
				e_table->click_to_add);

		g_signal_connect (
			e_table->click_to_add, "event",
			G_CALLBACK (click_to_add_event), e_table);
		g_signal_connect (
			e_table->click_to_add, "cursor_change",
			G_CALLBACK (click_to_add_cursor_change), e_table);
		e_signal_connect_notify (
			e_table->click_to_add, "notify::is-editing",
			G_CALLBACK (click_to_add_is_editing_changed_cb), e_table);
	}
}

static void
e_table_fill_table (ETable *e_table,
                    ETableModel *model)
{
	e_table_group_add_all (e_table->group);
}

/**
 * e_table_set_state_object:
 * @e_table: The #ETable object to modify
 * @state: The #ETableState to use
 *
 * This routine sets the state of the #ETable from the given
 * #ETableState.
 *
 **/
void
e_table_set_state_object (ETable *e_table,
                          ETableState *state)
{
	GValue *val;
	GtkWidget *widget;
	GtkAllocation allocation;

	val = g_new0 (GValue, 1);
	g_value_init (val, G_TYPE_DOUBLE);

	connect_header (e_table, state);

	widget = GTK_WIDGET (e_table->table_canvas);
	gtk_widget_get_allocation (widget, &allocation);

	g_value_set_double (val, (gdouble) allocation.width);
	g_object_set_property (G_OBJECT (e_table->header), "width", val);
	g_free (val);

	if (e_table->sort_info) {
		if (e_table->group_info_change_id)
			g_signal_handler_disconnect (
				e_table->sort_info,
				e_table->group_info_change_id);
		if (e_table->sort_info_change_id)
			g_signal_handler_disconnect (
				e_table->sort_info,
				e_table->sort_info_change_id);
		g_object_unref (e_table->sort_info);
	}
	if (state->sort_info) {
		e_table->sort_info = e_table_sort_info_duplicate (state->sort_info);
		e_table_sort_info_set_can_group (
			e_table->sort_info, e_table->allow_grouping);
		e_table->group_info_change_id = g_signal_connect (
			e_table->sort_info, "group_info_changed",
			G_CALLBACK (group_info_changed), e_table);

		e_table->sort_info_change_id = g_signal_connect (
			e_table->sort_info, "sort_info_changed",
			G_CALLBACK (sort_info_changed), e_table);
	}
	else
		e_table->sort_info = NULL;

	if (e_table->sorter)
		g_object_set (
			e_table->sorter,
			"sort_info", e_table->sort_info,
			NULL);
	if (e_table->header_item)
		g_object_set (
			e_table->header_item,
			"ETableHeader", e_table->header,
			"sort_info", e_table->sort_info,
			NULL);
	if (e_table->click_to_add)
		g_object_set (
			e_table->click_to_add,
			"header", e_table->header,
			NULL);

	e_table->need_rebuild = TRUE;
	if (!e_table->rebuild_idle_id)
		e_table->rebuild_idle_id = g_idle_add_full (20, changed_idle, e_table, NULL);

	e_table_state_change (e_table);
}

/**
 * e_table_load_state:
 * @e_table: The #ETable object to modify
 * @filename: name of the file to use
 *
 * This routine sets the state of the #ETable from a file.
 *
 **/
void
e_table_load_state (ETable *e_table,
                    const gchar *filename)
{
	ETableState *state;

	g_return_if_fail (E_IS_TABLE (e_table));
	g_return_if_fail (filename != NULL);

	state = e_table_state_new (e_table->spec);
	e_table_state_load_from_file (state, filename);

	if (state->col_count > 0)
		e_table_set_state_object (e_table, state);

	g_object_unref (state);
}

/**
 * e_table_get_state_object:
 * @e_table: #ETable object to act on
 *
 * Builds an #ETableState corresponding to the current state of the
 * #ETable.
 *
 * Return value:
 * The %ETableState object generated.
 **/
ETableState *
e_table_get_state_object (ETable *e_table)
{
	ETableState *state;
	GPtrArray *columns;
	gint full_col_count;
	gint i, j;

	columns = e_table_specification_ref_columns (e_table->spec);

	state = e_table_state_new (e_table->spec);

	g_clear_object (&state->sort_info);
	state->sort_info = g_object_ref (e_table->sort_info);

	state->col_count = e_table_header_count (e_table->header);
	full_col_count = e_table_header_count (e_table->full_header);

	state->column_specs = g_new (
		ETableColumnSpecification *, state->col_count);
	state->expansions = g_new (gdouble, state->col_count);

	for (i = 0; i < state->col_count; i++) {
		ETableCol *col = e_table_header_get_column (e_table->header, i);
		state->column_specs[i] = NULL;
		for (j = 0; j < full_col_count; j++) {
			if (col->spec->model_col == e_table_header_index (e_table->full_header, j)) {
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
 * e_table_save_state:
 * @e_table: The #ETable to act on
 * @filename: name of the file to save to
 *
 * Saves the state of the @e_table object into the file pointed by
 * @filename.
 *
 **/
void
e_table_save_state (ETable *e_table,
                    const gchar *filename)
{
	ETableState *state;

	state = e_table_get_state_object (e_table);
	e_table_state_save_to_file (state, filename);
	g_object_unref (state);
}

static void
et_selection_model_selection_changed (ETableGroup *etg,
                                      ETable *et)
{
	g_signal_emit (et, et_signals[SELECTION_CHANGE], 0);
}

static void
et_selection_model_selection_row_changed (ETableGroup *etg,
                                          gint row,
                                          ETable *et)
{
	g_signal_emit (et, et_signals[SELECTION_CHANGE], 0);
}

static ETable *
et_real_construct (ETable *e_table,
                   ETableModel *etm,
                   ETableExtras *ete,
                   ETableSpecification *specification,
                   ETableState *state)
{
	gint row = 0;
	gint col_count, i;
	GValue *val;
	GtkAdjustment *adjustment;
	GtkScrollable *scrollable;

	val = g_new0 (GValue, 1);
	g_value_init (val, G_TYPE_OBJECT);

	if (ete)
		g_object_ref (ete);
	else {
		ete = e_table_extras_new ();
	}

	e_table->domain = g_strdup (specification->domain);

	e_table->use_click_to_add = specification->click_to_add;
	e_table->use_click_to_add_end = specification->click_to_add_end;
	e_table->click_to_add_message = specification->click_to_add_message ?
		g_strdup (
			dgettext (e_table->domain,
		specification->click_to_add_message)) : NULL;
	e_table->alternating_row_colors = specification->alternating_row_colors;
	e_table->horizontal_draw_grid = specification->horizontal_draw_grid;
	e_table->vertical_draw_grid = specification->vertical_draw_grid;
	e_table->draw_focus = specification->draw_focus;
	e_table->cursor_mode = specification->cursor_mode;
	e_table->full_header = e_table_spec_to_full_header (specification, ete);

	col_count = e_table_header_count (e_table->full_header);
	for (i = 0; i < col_count; i++) {
		ETableCol *col = e_table_header_get_column (e_table->full_header, i);
		if (col && col->search) {
			e_table->current_search_col = col;
			e_table->search_col_set = TRUE;
			break;
		}
	}

	e_table->model = etm;
	g_object_ref (etm);

	connect_header (e_table, state);
	e_table->horizontal_scrolling = specification->horizontal_scrolling;
	e_table->horizontal_resize = specification->horizontal_resize;
	e_table->allow_grouping = specification->allow_grouping;

	e_table->sort_info = g_object_ref (state->sort_info);

	e_table_sort_info_set_can_group (
		e_table->sort_info, e_table->allow_grouping);

	e_table->group_info_change_id = g_signal_connect (
		e_table->sort_info, "group_info_changed",
		G_CALLBACK (group_info_changed), e_table);

	e_table->sort_info_change_id = g_signal_connect (
		e_table->sort_info, "sort_info_changed",
		G_CALLBACK (sort_info_changed), e_table);

	g_value_set_object (val, e_table->sort_info);
	g_object_set_property (G_OBJECT (e_table->header), "sort_info", val);
	g_free (val);

	e_table->sorter = e_table_sorter_new (
		etm, e_table->full_header, e_table->sort_info);

	g_object_set (
		e_table->selection,
		"model", etm,
		"selection_mode", specification->selection_mode,
		"cursor_mode", specification->cursor_mode,
		"sorter", e_table->sorter,
		"header", e_table->header,
		NULL);

	g_signal_connect (
		e_table->selection, "selection_changed",
		G_CALLBACK (et_selection_model_selection_changed), e_table);
	g_signal_connect (
		e_table->selection, "selection_row_changed",
		G_CALLBACK (et_selection_model_selection_row_changed), e_table);

	if (!specification->no_headers)
		e_table_setup_header (e_table);

	e_table_setup_table (
		e_table, e_table->full_header, e_table->header, etm);
	e_table_fill_table (e_table, etm);

	scrollable = GTK_SCROLLABLE (e_table->table_canvas);

	adjustment = gtk_scrollable_get_vadjustment (scrollable);
	gtk_adjustment_set_step_increment (adjustment, 20);

	adjustment = gtk_scrollable_get_hadjustment (scrollable);
	gtk_adjustment_set_step_increment (adjustment, 20);

	if (!specification->no_headers) {
		/* The header */
		gtk_grid_attach (
			GTK_GRID (e_table), GTK_WIDGET (e_table->header_canvas),
			0, row, 1, 1);
		row++;
	}
	gtk_grid_attach (
		GTK_GRID (e_table), GTK_WIDGET (e_table->table_canvas),
		0, row, 1, 1);

	g_object_unref (ete);

	return e_table;
}

/**
 * e_table_construct:
 * @e_table: The newly created #ETable object.
 * @etm: The model for this table.
 * @ete: An optional #ETableExtras.  (%NULL is valid.)
 * @specification: an #ETableSpecification
 *
 * This is the internal implementation of e_table_new() for use by
 * subclasses or language bindings.  See e_table_new() for details.
 *
 * Return value:
 * The passed in value @e_table or %NULL if there's an error.
 **/
ETable *
e_table_construct (ETable *e_table,
                   ETableModel *etm,
                   ETableExtras *ete,
                   ETableSpecification *specification)
{
	ETableState *state;

	g_return_val_if_fail (E_IS_TABLE (e_table), NULL);
	g_return_val_if_fail (E_IS_TABLE_MODEL (etm), NULL);
	g_return_val_if_fail (ete == NULL || E_IS_TABLE_EXTRAS (ete), NULL);
	g_return_val_if_fail (E_IS_TABLE_SPECIFICATION (specification), NULL);

	state = g_object_ref (specification->state);

	e_table = et_real_construct (e_table, etm, ete, specification, state);

	e_table->spec = g_object_ref (specification);
	g_object_unref (state);

	return e_table;
}

/**
 * e_table_new:
 * @etm: The model for this table.
 * @ete: An optional #ETableExtras.  (%NULL is valid.)
 * @specification: an #ETableSpecification
 *
 * This function creates an #ETable from the given parameters.  The
 * #ETableModel is a table model to be represented.  The #ETableExtras
 * is an optional set of pixbufs, cells, and sorting functions to be
 * used when interpreting the spec.  If you pass in %NULL it uses the
 * default #ETableExtras.  (See e_table_extras_new()).
 *
 * @specification is the specification of the set of viewable columns and the
 * default sorting state and such.  @state is an optional string specifying
 * the current sorting state and such.
 *
 * Return value:
 * The newly created #ETable or %NULL if there's an error.
 **/
GtkWidget *
e_table_new (ETableModel *etm,
             ETableExtras *ete,
             ETableSpecification *specification)
{
	ETable *e_table;

	g_return_val_if_fail (E_IS_TABLE_MODEL (etm), NULL);
	g_return_val_if_fail (ete == NULL || E_IS_TABLE_EXTRAS (ete), NULL);
	g_return_val_if_fail (E_IS_TABLE_SPECIFICATION (specification), NULL);

	e_table = g_object_new (E_TYPE_TABLE, NULL);

	e_table = e_table_construct (e_table, etm, ete, specification);

	return GTK_WIDGET (e_table);
}

/**
 * e_table_set_cursor_row:
 * @e_table: The #ETable to set the cursor row of
 * @row: The row number
 *
 * Sets the cursor row and the selection to the given row number.
 **/
void
e_table_set_cursor_row (ETable *e_table,
                        gint row)
{
	g_return_if_fail (E_IS_TABLE (e_table));
	g_return_if_fail (row >= 0);

	g_object_set (
		e_table->selection,
		"cursor_row", row,
		NULL);
}

/**
 * e_table_get_cursor_row:
 * @e_table: The #ETable to query
 *
 * Calculates the cursor row.  -1 means that we don't have a cursor.
 *
 * Return value:
 * Cursor row
 **/
gint
e_table_get_cursor_row (ETable *e_table)
{
	gint row;
	g_return_val_if_fail (E_IS_TABLE (e_table), -1);

	g_object_get (
		e_table->selection,
		"cursor_row", &row,
		NULL);
	return row;
}

/**
 * e_table_selected_row_foreach:
 * @e_table: The #ETable to act on
 * @callback: The callback function to call
 * @closure: The value passed to the callback's closure argument
 *
 * Calls the given @callback function once for every selected row.
 *
 * If you change the selection or delete or add rows to the table
 * during these callbacks, problems can occur.  A standard thing to do
 * is to create a list of rows or objects the function is called upon
 * and then act upon that list. (In inverse order if it's rows.)
 **/
void
e_table_selected_row_foreach (ETable *e_table,
                              EForeachFunc callback,
                              gpointer closure)
{
	g_return_if_fail (E_IS_TABLE (e_table));

	e_selection_model_foreach (E_SELECTION_MODEL (e_table->selection),
						     callback,
						     closure);
}

/**
 * e_table_selected_count:
 * @e_table: The #ETable to query
 *
 * Counts the number of selected rows.
 *
 * Return value:
 * The number of rows selected.
 **/
gint
e_table_selected_count (ETable *e_table)
{
	g_return_val_if_fail (E_IS_TABLE (e_table), -1);

	return e_selection_model_selected_count (E_SELECTION_MODEL (e_table->selection));
}

/**
 * e_table_select_all:
 * @table: The #ETable to modify
 *
 * Selects all the rows in @table.
 **/
void
e_table_select_all (ETable *table)
{
	g_return_if_fail (E_IS_TABLE (table));

	e_selection_model_select_all (E_SELECTION_MODEL (table->selection));
}

/**
 * e_table_get_printable:
 * @e_table: #ETable to query
 *
 * Used for printing your #ETable.
 *
 * Return value:
 * The #EPrintable to print.
 **/
EPrintable *
e_table_get_printable (ETable *e_table)
{
	g_return_val_if_fail (E_IS_TABLE (e_table), NULL);

	return e_table_group_get_printable (e_table->group);
}

/**
 * e_table_right_click_up:
 * @table: The #ETable to modify.
 *
 * Call this function when you're done handling the right click if you
 * return TRUE from the "right_click" signal.
 **/
void
e_table_right_click_up (ETable *table)
{
	e_selection_model_right_click_up (E_SELECTION_MODEL (table->selection));
}

/**
 * e_table_commit_click_to_add:
 * @table: The #ETable to modify
 *
 * Commits the current values in the click to add to the table.
 **/
void
e_table_commit_click_to_add (ETable *table)
{
	et_eti_leave_edit (table);
	if (table->click_to_add)
		e_table_click_to_add_commit (
			E_TABLE_CLICK_TO_ADD (table->click_to_add));
}

static void
et_get_property (GObject *object,
                 guint property_id,
                 GValue *value,
                 GParamSpec *pspec)
{
	ETable *etable = E_TABLE (object);

	switch (property_id) {
	case PROP_MODEL:
		g_value_set_object (value, etable->model);
		break;
	case PROP_UNIFORM_ROW_HEIGHT:
		g_value_set_boolean (value, etable->uniform_row_height);
		break;
	case PROP_ALWAYS_SEARCH:
		g_value_set_boolean (value, etable->always_search);
		break;
	case PROP_USE_CLICK_TO_ADD:
		g_value_set_boolean (value, etable->use_click_to_add);
		break;
	case PROP_HADJUSTMENT:
		if (etable->table_canvas)
			g_object_get_property (
				G_OBJECT (etable->table_canvas),
				"hadjustment", value);
		else
			g_value_set_object (value, NULL);
		break;
	case PROP_VADJUSTMENT:
		if (etable->table_canvas)
			g_object_get_property (
				G_OBJECT (etable->table_canvas),
				"vadjustment", value);
		else
			g_value_set_object (value, NULL);
		break;
	case PROP_HSCROLL_POLICY:
		if (etable->table_canvas)
			g_object_get_property (
				G_OBJECT (etable->table_canvas),
				"hscroll-policy", value);
		else
			g_value_set_enum (value, 0);
		break;
	case PROP_VSCROLL_POLICY:
		if (etable->table_canvas)
			g_object_get_property (
				G_OBJECT (etable->table_canvas),
				"vscroll-policy", value);
		else
			g_value_set_enum (value, 0);
		break;
	case PROP_IS_EDITING:
		g_value_set_boolean (value, e_table_is_editing (etable));
		break;
	default:
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
	ETable *etable = E_TABLE (object);

	switch (property_id) {
	case PROP_LENGTH_THRESHOLD:
		etable->length_threshold = g_value_get_int (value);
		if (etable->group) {
			gnome_canvas_item_set (
				GNOME_CANVAS_ITEM (etable->group),
				"length_threshold",
				etable->length_threshold,
				NULL);
		}
		break;
	case PROP_UNIFORM_ROW_HEIGHT:
		etable->uniform_row_height = g_value_get_boolean (value);
		if (etable->group) {
			gnome_canvas_item_set (
				GNOME_CANVAS_ITEM (etable->group),
				"uniform_row_height", etable->uniform_row_height && !etable->is_grouped,
				NULL);
		}
		break;
	case PROP_ALWAYS_SEARCH:
		if (etable->always_search == g_value_get_boolean (value))
			return;

		etable->always_search = g_value_get_boolean (value);
		clear_current_search_col (etable);
		break;
	case PROP_USE_CLICK_TO_ADD:
		if (etable->use_click_to_add == g_value_get_boolean (value))
			return;

		etable->use_click_to_add = g_value_get_boolean (value);
		clear_current_search_col (etable);

		if (etable->use_click_to_add) {
			etable->click_to_add = gnome_canvas_item_new (
				GNOME_CANVAS_GROUP (etable->canvas_vbox),
				e_table_click_to_add_get_type (),
				"header", etable->header,
				"model", etable->model,
				"message", etable->click_to_add_message,
				NULL);

			if (etable->use_click_to_add_end)
				e_canvas_vbox_add_item (
					E_CANVAS_VBOX (etable->canvas_vbox),
					etable->click_to_add);
			else
				e_canvas_vbox_add_item_start (
					E_CANVAS_VBOX (etable->canvas_vbox),
					etable->click_to_add);

			g_signal_connect (
				etable->click_to_add, "event",
				G_CALLBACK (click_to_add_event), etable);
			g_signal_connect (
				etable->click_to_add, "cursor_change",
				G_CALLBACK (click_to_add_cursor_change),
				etable);
			e_signal_connect_notify (
				etable->click_to_add, "notify::is-editing",
				G_CALLBACK (click_to_add_is_editing_changed_cb), etable);
		} else {
			g_object_run_dispose (G_OBJECT (etable->click_to_add));
			etable->click_to_add = NULL;
		}
		break;
	case PROP_HADJUSTMENT:
		if (etable->table_canvas)
			g_object_set_property (
				G_OBJECT (etable->table_canvas),
				"hadjustment", value);
		break;
	case PROP_VADJUSTMENT:
		if (etable->table_canvas)
			g_object_set_property (
				G_OBJECT (etable->table_canvas),
				"vadjustment", value);
		break;
	case PROP_HSCROLL_POLICY:
		if (etable->table_canvas)
			g_object_set_property (
				G_OBJECT (etable->table_canvas),
				"hscroll-policy", value);
		break;
	case PROP_VSCROLL_POLICY:
		if (etable->table_canvas)
			g_object_set_property (
				G_OBJECT (etable->table_canvas),
				"vscroll-policy", value);
		break;
	}
}

/**
 * e_table_get_next_row:
 * @e_table: The #ETable to query
 * @model_row: The model row to go from
 *
 * This function is used when your table is sorted, but you're using
 * model row numbers.  It returns the next row in sorted order as a model row.
 *
 * Return value:
 * The model row number.
 **/
gint
e_table_get_next_row (ETable *e_table,
                      gint model_row)
{
	g_return_val_if_fail (E_IS_TABLE (e_table), -1);

	if (e_table->sorter) {
		gint i;
		i = e_sorter_model_to_sorted (E_SORTER (e_table->sorter), model_row);
		i++;
		if (i < e_table_model_row_count (e_table->model)) {
			return e_sorter_sorted_to_model (E_SORTER (e_table->sorter), i);
		} else
			return -1;
	} else
		if (model_row < e_table_model_row_count (e_table->model) - 1)
			return model_row + 1;
		else
			return -1;
}

/**
 * e_table_get_prev_row:
 * @e_table: The #ETable to query
 * @model_row: The model row to go from
 *
 * This function is used when your table is sorted, but you're using
 * model row numbers.  It returns the previous row in sorted order as
 * a model row.
 *
 * Return value:
 * The model row number.
 **/
gint
e_table_get_prev_row (ETable *e_table,
                      gint model_row)
{
	g_return_val_if_fail (E_IS_TABLE (e_table), -1);

	if (e_table->sorter) {
		gint i;
		i = e_sorter_model_to_sorted (E_SORTER (e_table->sorter), model_row);
		i--;
		if (i >= 0)
			return e_sorter_sorted_to_model (E_SORTER (e_table->sorter), i);
		else
			return -1;
	} else
		return model_row - 1;
}

/**
 * e_table_model_to_view_row:
 * @e_table: The #ETable to query
 * @model_row: The model row number
 *
 * Turns a model row into a view row.
 *
 * Return value:
 * The view row number.
 **/
gint
e_table_model_to_view_row (ETable *e_table,
                           gint model_row)
{
	g_return_val_if_fail (E_IS_TABLE (e_table), -1);

	if (e_table->sorter)
		return e_sorter_model_to_sorted (E_SORTER (e_table->sorter), model_row);
	else
		return model_row;
}

/**
 * e_table_view_to_model_row:
 * @e_table: The #ETable to query
 * @view_row: The view row number
 *
 * Turns a view row into a model row.
 *
 * Return value:
 * The model row number.
 **/
gint
e_table_view_to_model_row (ETable *e_table,
                           gint view_row)
{
	g_return_val_if_fail (E_IS_TABLE (e_table), -1);

	if (e_table->sorter)
		return e_sorter_sorted_to_model (E_SORTER (e_table->sorter), view_row);
	else
		return view_row;
}

/**
 * e_table_get_cell_at:
 * @table: An #ETable widget
 * @x: X coordinate for the pixel
 * @y: Y coordinate for the pixel
 * @row_return: Pointer to return the row value
 * @col_return: Pointer to return the column value
 *
 * Return the row and column for the cell in which the pixel at (@x, @y) is
 * contained.
 **/
void
e_table_get_cell_at (ETable *table,
                     gint x,
                     gint y,
                     gint *row_return,
                     gint *col_return)
{
	GtkAdjustment *adjustment;
	GtkScrollable *scrollable;

	g_return_if_fail (E_IS_TABLE (table));
	g_return_if_fail (row_return != NULL);
	g_return_if_fail (col_return != NULL);

	/* FIXME it would be nice if it could handle a NULL row_return or
	 * col_return gracefully.  */

	scrollable = GTK_SCROLLABLE (table->table_canvas);

	adjustment = gtk_scrollable_get_hadjustment (scrollable);
	x += gtk_adjustment_get_value (adjustment);

	adjustment = gtk_scrollable_get_vadjustment (scrollable);
	y += gtk_adjustment_get_value (adjustment);

	e_table_group_compute_location (
		table->group, &x, &y, row_return, col_return);
}

/**
 * e_table_get_cell_geometry:
 * @table: The #ETable.
 * @row: The row to get the geometry of.
 * @col: The col to get the geometry of.
 * @x_return: Returns the x coordinate of the upper left hand corner
 *            of the cell with respect to the widget.
 * @y_return: Returns the y coordinate of the upper left hand corner
 *            of the cell with respect to the widget.
 * @width_return: Returns the width of the cell.
 * @height_return: Returns the height of the cell.
 *
 * Returns the x, y, width, and height of the given cell.  These can
 * all be #NULL and they just won't be set.
 **/
void
e_table_get_cell_geometry (ETable *table,
                           gint row,
                           gint col,
                           gint *x_return,
                           gint *y_return,
                           gint *width_return,
                           gint *height_return)
{
	GtkAllocation allocation;
	GtkAdjustment *adjustment;
	GtkScrollable *scrollable;

	g_return_if_fail (E_IS_TABLE (table));

	scrollable = GTK_SCROLLABLE (table->table_canvas);

	e_table_group_get_cell_geometry (
		table->group, &row, &col, x_return, y_return,
		width_return, height_return);

	if (x_return && table->table_canvas) {
		adjustment = gtk_scrollable_get_hadjustment (scrollable);
		(*x_return) -= gtk_adjustment_get_value (adjustment);
	}

	if (y_return) {
		if (table->table_canvas) {
			adjustment = gtk_scrollable_get_vadjustment (scrollable);
			(*y_return) -= gtk_adjustment_get_value (adjustment);
		}

		if (table->header_canvas) {
			gtk_widget_get_allocation (
				GTK_WIDGET (table->header_canvas),
				&allocation);
			(*y_return) += allocation.height;
		}
	}
}

/**
 * e_table_get_mouse_over_cell:
 *
 * Similar to e_table_get_cell_at, only here we check
 * based on the mouse motion information in the group.
 **/
void
e_table_get_mouse_over_cell (ETable *table,
                             gint *row,
                             gint *col)
{
	g_return_if_fail (E_IS_TABLE (table));

	if (!table->group)
		return;

	e_table_group_get_mouse_over (table->group, row, col);
}

/**
 * e_table_get_selection_model:
 * @table: The #ETable to query
 *
 * Returns the table's #ESelectionModel in case you want to access it
 * directly.
 *
 * Return value:
 * The #ESelectionModel.
 **/
ESelectionModel *
e_table_get_selection_model (ETable *table)
{
	g_return_val_if_fail (E_IS_TABLE (table), NULL);

	return E_SELECTION_MODEL (table->selection);
}

struct _ETableDragSourceSite
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
/* Target */

/**
 * e_table_drag_get_data:
 * @table:
 * @row:
 * @col:
 * @context:
 * @target:
 * @time:
 *
 *
 **/
void
e_table_drag_get_data (ETable *table,
                       gint row,
                       gint col,
                       GdkDragContext *context,
                       GdkAtom target,
                       guint32 time)
{
	g_return_if_fail (E_IS_TABLE (table));

	gtk_drag_get_data (
		GTK_WIDGET (table),
		context,
		target,
		time);
}

/**
 * e_table_drag_highlight:
 * @table: The #ETable to highlight
 * @row: The row number of the cell to highlight
 * @col: The column number of the cell to highlight
 *
 * Set col to -1 to highlight the entire row.  If row is -1, this is
 * identical to e_table_drag_unhighlight().
 **/
void
e_table_drag_highlight (ETable *table,
                        gint row,
                        gint col)
{
	GtkAllocation allocation;
	GtkAdjustment *adjustment;
	GtkScrollable *scrollable;

	g_return_if_fail (E_IS_TABLE (table));

	scrollable = GTK_SCROLLABLE (table->table_canvas);
	gtk_widget_get_allocation (GTK_WIDGET (scrollable), &allocation);

	if (row != -1) {
		gint x, y, width, height;
		if (col == -1) {
			e_table_get_cell_geometry (table, row, 0, &x, &y, &width, &height);
			x = 0;
			width = allocation.width;
		} else {
			e_table_get_cell_geometry (table, row, col, &x, &y, &width, &height);
			adjustment = gtk_scrollable_get_hadjustment (scrollable);
			x += gtk_adjustment_get_value (adjustment);
		}

		adjustment = gtk_scrollable_get_vadjustment (scrollable);
		y += gtk_adjustment_get_value (adjustment);

		if (table->drop_highlight == NULL) {
			GdkRGBA fg;

			e_utils_get_theme_color (GTK_WIDGET (table), "theme_fg_color", E_UTILS_DEFAULT_THEME_FG_COLOR, &fg);

			table->drop_highlight = gnome_canvas_item_new (
				gnome_canvas_root (table->table_canvas),
				gnome_canvas_rect_get_type (),
				"outline-color", &fg,
				NULL);
		}
		gnome_canvas_item_set (
			table->drop_highlight,
			"x1", (gdouble) x,
			"x2", (gdouble) x + width - 1,
			"y1", (gdouble) y,
			"y2", (gdouble) y + height - 1,
			NULL);
	} else {
		if (table->drop_highlight) {
			g_object_run_dispose (G_OBJECT (table->drop_highlight));
			table->drop_highlight = NULL;
		}
	}
}

/**
 * e_table_drag_unhighlight:
 * @table: The #ETable to unhighlight
 *
 * Removes the highlight from an #ETable.
 **/
void
e_table_drag_unhighlight (ETable *table)
{
	g_return_if_fail (E_IS_TABLE (table));

	if (table->drop_highlight) {
		g_object_run_dispose (G_OBJECT (table->drop_highlight));
		table->drop_highlight = NULL;
	}
}

void
e_table_drag_dest_set (ETable *table,
                       GtkDestDefaults flags,
                       const GtkTargetEntry *targets,
                       gint n_targets,
                       GdkDragAction actions)
{
	g_return_if_fail (E_IS_TABLE (table));

	gtk_drag_dest_set (
		GTK_WIDGET (table), flags, targets, n_targets, actions);
}

void
e_table_drag_dest_set_proxy (ETable *table,
                             GdkWindow *proxy_window,
                             GdkDragProtocol protocol,
                             gboolean use_coordinates)
{
	g_return_if_fail (E_IS_TABLE (table));

	gtk_drag_dest_set_proxy (
		GTK_WIDGET (table), proxy_window, protocol, use_coordinates);
}

/*
 * There probably should be functions for setting the targets
 * as a GtkTargetList
 */

void
e_table_drag_dest_unset (GtkWidget *widget)
{
	g_return_if_fail (E_IS_TABLE (widget));

	gtk_drag_dest_unset (widget);
}

/* Source side */

static gint
et_real_start_drag (ETable *table,
                    gint row,
                    gint col,
                    GdkEvent *event)
{
	GtkDragSourceInfo *info;
	GdkDragContext *context;
	ETableDragSourceSite *site;

	if (table->do_drag) {
		site = table->site;

		site->state = 0;
		context = e_table_drag_begin (
			table, row, col,
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

/**
 * e_table_drag_source_set:
 * @table: The #ETable to set up as a drag site
 * @start_button_mask: Mask of allowed buttons to start drag
 * @targets: Table of targets for this source
 * @n_targets: Number of targets in @targets
 * @actions: Actions allowed for this source
 *
 * Registers this table as a drag site, and possibly adds default behaviors.
 **/
void
e_table_drag_source_set (ETable *table,
                         GdkModifierType start_button_mask,
                         const GtkTargetEntry *targets,
                         gint n_targets,
                         GdkDragAction actions)
{
	ETableDragSourceSite *site;
	GtkWidget *canvas;

	g_return_if_fail (E_IS_TABLE (table));

	canvas = GTK_WIDGET (table->table_canvas);
	site = table->site;

	gtk_widget_add_events (
		canvas,
		gtk_widget_get_events (canvas) |
		GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
		GDK_BUTTON_MOTION_MASK | GDK_STRUCTURE_MASK);

	table->do_drag = TRUE;

	if (site) {
		if (site->target_list)
			gtk_target_list_unref (site->target_list);
	} else {
		site = g_new0 (ETableDragSourceSite, 1);
		table->site = site;
	}

	site->start_button_mask = start_button_mask;

	if (targets)
		site->target_list = gtk_target_list_new (targets, n_targets);
	else
		site->target_list = NULL;

	site->actions = actions;
}

/**
 * e_table_drag_source_unset:
 * @table: The #ETable to un set up as a drag site
 *
 * Unregisters this #ETable as a drag site.
 **/
void
e_table_drag_source_unset (ETable *table)
{
	ETableDragSourceSite *site;

	g_return_if_fail (E_IS_TABLE (table));

	site = table->site;

	if (site) {
		if (site->target_list)
			gtk_target_list_unref (site->target_list);
		g_free (site);
		table->site = NULL;
	}
	table->do_drag = FALSE;
}

/* There probably should be functions for setting the targets
 * as a GtkTargetList
 */

/**
 * e_table_drag_begin:
 * @table: The #ETable to drag from
 * @row: The row number of the cell
 * @col: The col number of the cell
 * @targets: The list of targets supported by the drag
 * @actions: The available actions supported by the drag
 * @button: The button held down for the drag
 * @event: The event that initiated the drag
 *
 * Start a drag from this cell.
 *
 * Return value:
 * The drag context.
 **/
GdkDragContext *
e_table_drag_begin (ETable *table,
                    gint row,
                    gint col,
                    GtkTargetList *targets,
                    GdkDragAction actions,
                    gint button,
                    GdkEvent *event)
{
	g_return_val_if_fail (E_IS_TABLE (table), NULL);

	table->drag_row = row;
	table->drag_col = col;

	return gtk_drag_begin (
		GTK_WIDGET (table), targets, actions, button, event);
}

static void
et_drag_begin (GtkWidget *widget,
               GdkDragContext *context,
               ETable *et)
{
	g_signal_emit (
		et, et_signals[TABLE_DRAG_BEGIN], 0,
		et->drag_row, et->drag_col, context);
}

static void
et_drag_end (GtkWidget *widget,
             GdkDragContext *context,
             ETable *et)
{
	g_signal_emit (
		et, et_signals[TABLE_DRAG_END], 0,
		et->drag_row, et->drag_col, context);
}

static void
et_drag_data_get (GtkWidget *widget,
                  GdkDragContext *context,
                  GtkSelectionData *selection_data,
                  guint info,
                  guint time,
                  ETable *et)
{
	g_signal_emit (
		et, et_signals[TABLE_DRAG_DATA_GET], 0,
		et->drag_row, et->drag_col, context, selection_data,
		info, time);
}

static void
et_drag_data_delete (GtkWidget *widget,
                     GdkDragContext *context,
                     ETable *et)
{
	g_signal_emit (
		et, et_signals[TABLE_DRAG_DATA_DELETE], 0,
		et->drag_row, et->drag_col, context);
}

static gboolean
do_drag_motion (ETable *et,
                GdkDragContext *context,
                gint x,
                gint y,
                guint time)
{
	gboolean ret_val;
	gint row = -1, col = -1;

	e_table_get_cell_at (et, x, y, &row, &col);

	if (row != et->drop_row && col != et->drop_row) {
		g_signal_emit (
			et, et_signals[TABLE_DRAG_LEAVE], 0,
			et->drop_row, et->drop_col, context, time);
	}

	et->drop_row = row;
	et->drop_col = col;

	g_signal_emit (
		et, et_signals[TABLE_DRAG_MOTION], 0,
		et->drop_row, et->drop_col, context, x, y, time, &ret_val);

	return ret_val;
}

static gboolean
scroll_timeout (gpointer data)
{
	ETable *et = data;
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

	if (et->scroll_direction & ET_SCROLL_DOWN)
		dy += 20;
	if (et->scroll_direction & ET_SCROLL_UP)
		dy -= 20;

	if (et->scroll_direction & ET_SCROLL_RIGHT)
		dx += 20;
	if (et->scroll_direction & ET_SCROLL_LEFT)
		dx -= 20;

	scrollable = GTK_SCROLLABLE (et->table_canvas);

	adjustment = gtk_scrollable_get_hadjustment (scrollable);

	lower = gtk_adjustment_get_lower (adjustment);
	upper = gtk_adjustment_get_upper (adjustment);
	page_size = gtk_adjustment_get_page_size (adjustment);

	old_h_value = gtk_adjustment_get_value (adjustment);
	new_h_value = CLAMP (old_h_value + dx, lower, upper - page_size);

	gtk_adjustment_set_value (adjustment, new_h_value);

	adjustment = gtk_scrollable_get_vadjustment (scrollable);

	lower = gtk_adjustment_get_lower (adjustment);
	upper = gtk_adjustment_get_upper (adjustment);
	page_size = gtk_adjustment_get_page_size (adjustment);

	old_v_value = gtk_adjustment_get_value (adjustment);
	new_v_value = CLAMP (old_v_value + dy, lower, upper - page_size);

	gtk_adjustment_set_value (adjustment, new_v_value);

	if (new_h_value != old_h_value || new_v_value != old_v_value)
		do_drag_motion (
			et,
			et->last_drop_context,
			et->last_drop_x,
			et->last_drop_y,
			et->last_drop_time);

	return TRUE;
}

static void
scroll_on (ETable *et,
           guint scroll_direction)
{
	if (et->scroll_idle_id == 0 || scroll_direction != et->scroll_direction) {
		if (et->scroll_idle_id != 0)
			g_source_remove (et->scroll_idle_id);
		et->scroll_direction = scroll_direction;
		et->scroll_idle_id = e_named_timeout_add (
			100, scroll_timeout, et);
	}
}

static void
scroll_off (ETable *et)
{
	if (et->scroll_idle_id) {
		g_source_remove (et->scroll_idle_id);
		et->scroll_idle_id = 0;
	}
}

static void
context_destroyed (gpointer data)
{
	ETable *et = data;
	/* if (!G_OBJECT_DESTROYED (et)) */
/* FIXME: */
	{
		et->last_drop_x = 0;
		et->last_drop_y = 0;
		et->last_drop_time = 0;
		et->last_drop_context = NULL;
		scroll_off (et);
	}
	g_object_unref (et);
}

static void
context_connect (ETable *et,
                 GdkDragContext *context)
{
	if (g_dataset_get_data (context, "e-table") == NULL) {
		g_object_ref (et);
		g_dataset_set_data_full (context, "e-table", et, context_destroyed);
	}
}

static void
et_drag_leave (GtkWidget *widget,
               GdkDragContext *context,
               guint time,
               ETable *et)
{
	g_signal_emit (
		et, et_signals[TABLE_DRAG_LEAVE], 0,
		et->drop_row, et->drop_col, context, time);

	et->drop_row = -1;
	et->drop_col = -1;

	scroll_off (et);
}

static gboolean
et_drag_motion (GtkWidget *widget,
                GdkDragContext *context,
                gint x,
                gint y,
                guint time,
                ETable *et)
{
	GtkAllocation allocation;
	gboolean ret_val;
	guint direction = 0;

	gtk_widget_get_allocation (widget, &allocation);

	et->last_drop_x = x;
	et->last_drop_y = y;
	et->last_drop_time = time;
	et->last_drop_context = context;
	context_connect (et, context);

	ret_val = do_drag_motion (et, context, x, y, time);

	if (y < 20)
		direction |= ET_SCROLL_UP;
	if (y > allocation.height - 20)
		direction |= ET_SCROLL_DOWN;
	if (x < 20)
		direction |= ET_SCROLL_LEFT;
	if (x > allocation.width - 20)
		direction |= ET_SCROLL_RIGHT;

	if (direction != 0)
		scroll_on (et, direction);
	else
		scroll_off (et);

	return ret_val;
}

static gboolean
et_drag_drop (GtkWidget *widget,
              GdkDragContext *context,
              gint x,
              gint y,
              guint time,
              ETable *et)
{
	gboolean ret_val;
	gint row, col;

	e_table_get_cell_at (et, x, y, &row, &col);

	if (row != et->drop_row && col != et->drop_row) {
		g_signal_emit (
			et, et_signals[TABLE_DRAG_LEAVE], 0,
			et->drop_row, et->drop_col, context, time);
		g_signal_emit (
			et, et_signals[TABLE_DRAG_MOTION], 0,
			row, col, context, x, y, time, &ret_val);
	}
	et->drop_row = row;
	et->drop_col = col;
	g_signal_emit (
		et, et_signals[TABLE_DRAG_DROP], 0,
		et->drop_row, et->drop_col, context, x, y, time, &ret_val);
	et->drop_row = -1;
	et->drop_col = -1;

	scroll_off (et);

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
                       ETable *et)
{
	gint row, col;

	e_table_get_cell_at (et, x, y, &row, &col);

	g_signal_emit (
		et, et_signals[TABLE_DRAG_DATA_RECEIVED], 0,
		row, col, context, x, y, selection_data, info, time);
}

static void
e_table_class_init (ETableClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	object_class->dispose = et_dispose;
	object_class->finalize = et_finalize;
	object_class->set_property = et_set_property;
	object_class->get_property = et_get_property;

	widget_class->grab_focus = et_grab_focus;
	widget_class->unrealize = et_unrealize;
	widget_class->get_preferred_width = et_get_preferred_width;
	widget_class->get_preferred_height = et_get_preferred_height;
	widget_class->style_updated = et_canvas_style_updated;
	widget_class->focus = et_focus;

	gtk_widget_class_set_css_name (widget_class, "ETable");

	class->cursor_change = NULL;
	class->cursor_activated = NULL;
	class->selection_change = NULL;
	class->double_click = NULL;
	class->right_click = NULL;
	class->click = NULL;
	class->key_press = NULL;
	class->start_drag = et_real_start_drag;
	class->state_change = NULL;
	class->white_space_event = NULL;

	class->table_drag_begin = NULL;
	class->table_drag_end = NULL;
	class->table_drag_data_get = NULL;
	class->table_drag_data_delete = NULL;

	class->table_drag_leave = NULL;
	class->table_drag_motion = NULL;
	class->table_drag_drop = NULL;
	class->table_drag_data_received = NULL;

	et_signals[CURSOR_CHANGE] = g_signal_new (
		"cursor_change",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableClass, cursor_change),
		NULL, NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1, G_TYPE_INT);

	et_signals[CURSOR_ACTIVATED] = g_signal_new (
		"cursor_activated",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableClass, cursor_activated),
		NULL, NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1, G_TYPE_INT);

	et_signals[SELECTION_CHANGE] = g_signal_new (
		"selection_change",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableClass, selection_change),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	et_signals[DOUBLE_CLICK] = g_signal_new (
		"double_click",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableClass, double_click),
		NULL, NULL,
		e_marshal_VOID__INT_INT_BOXED,
		G_TYPE_NONE, 3,
		G_TYPE_INT,
		G_TYPE_INT,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	et_signals[RIGHT_CLICK] = g_signal_new (
		"right_click",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableClass, right_click),
		g_signal_accumulator_true_handled, NULL,
		e_marshal_BOOLEAN__INT_INT_BOXED,
		G_TYPE_BOOLEAN, 3,
		G_TYPE_INT,
		G_TYPE_INT,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	et_signals[CLICK] = g_signal_new (
		"click",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableClass, click),
		g_signal_accumulator_true_handled, NULL,
		e_marshal_BOOLEAN__INT_INT_BOXED,
		G_TYPE_BOOLEAN, 3,
		G_TYPE_INT,
		G_TYPE_INT,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	et_signals[KEY_PRESS] = g_signal_new (
		"key_press",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableClass, key_press),
		g_signal_accumulator_true_handled, NULL,
		e_marshal_BOOLEAN__INT_INT_BOXED,
		G_TYPE_BOOLEAN, 3,
		G_TYPE_INT,
		G_TYPE_INT,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	et_signals[START_DRAG] = g_signal_new (
		"start_drag",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableClass, start_drag),
		g_signal_accumulator_true_handled, NULL,
		e_marshal_BOOLEAN__INT_INT_BOXED,
		G_TYPE_BOOLEAN, 3,
		G_TYPE_INT,
		G_TYPE_INT,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	et_signals[STATE_CHANGE] = g_signal_new (
		"state_change",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableClass, state_change),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	et_signals[WHITE_SPACE_EVENT] = g_signal_new (
		"white_space_event",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableClass, white_space_event),
		g_signal_accumulator_true_handled, NULL,
		e_marshal_BOOLEAN__BOXED,
		G_TYPE_BOOLEAN, 1,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	et_signals[TABLE_DRAG_BEGIN] = g_signal_new (
		"table_drag_begin",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableClass, table_drag_begin),
		NULL, NULL,
		e_marshal_VOID__INT_INT_OBJECT,
		G_TYPE_NONE, 3,
		G_TYPE_INT,
		G_TYPE_INT,
		GDK_TYPE_DRAG_CONTEXT);

	et_signals[TABLE_DRAG_END] = g_signal_new (
		"table_drag_end",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableClass, table_drag_end),
		NULL, NULL,
		e_marshal_VOID__INT_INT_OBJECT,
		G_TYPE_NONE, 3,
		G_TYPE_INT,
		G_TYPE_INT,
		GDK_TYPE_DRAG_CONTEXT);

	et_signals[TABLE_DRAG_DATA_GET] = g_signal_new (
		"table_drag_data_get",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableClass, table_drag_data_get),
		NULL, NULL,
		e_marshal_VOID__INT_INT_OBJECT_BOXED_UINT_UINT,
		G_TYPE_NONE, 6,
		G_TYPE_INT,
		G_TYPE_INT,
		GDK_TYPE_DRAG_CONTEXT,
		GTK_TYPE_SELECTION_DATA | G_SIGNAL_TYPE_STATIC_SCOPE,
		G_TYPE_UINT,
		G_TYPE_UINT);

	et_signals[TABLE_DRAG_DATA_DELETE] = g_signal_new (
		"table_drag_data_delete",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableClass, table_drag_data_delete),
		NULL, NULL,
		e_marshal_VOID__INT_INT_OBJECT,
		G_TYPE_NONE, 3,
		G_TYPE_INT,
		G_TYPE_INT,
		GDK_TYPE_DRAG_CONTEXT);

	et_signals[TABLE_DRAG_LEAVE] = g_signal_new (
		"table_drag_leave",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableClass, table_drag_leave),
		NULL, NULL,
		e_marshal_VOID__INT_INT_OBJECT_UINT,
		G_TYPE_NONE, 4,
		G_TYPE_INT,
		G_TYPE_INT,
		GDK_TYPE_DRAG_CONTEXT,
		G_TYPE_UINT);

	et_signals[TABLE_DRAG_MOTION] = g_signal_new (
		"table_drag_motion",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableClass, table_drag_motion),
		NULL, NULL,
		e_marshal_BOOLEAN__INT_INT_OBJECT_INT_INT_UINT,
		G_TYPE_BOOLEAN, 6,
		G_TYPE_INT,
		G_TYPE_INT,
		GDK_TYPE_DRAG_CONTEXT,
		G_TYPE_INT,
		G_TYPE_INT,
		G_TYPE_UINT);

	et_signals[TABLE_DRAG_DROP] = g_signal_new (
		"table_drag_drop",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableClass, table_drag_drop),
		NULL, NULL,
		e_marshal_BOOLEAN__INT_INT_OBJECT_INT_INT_UINT,
		G_TYPE_BOOLEAN, 6,
		G_TYPE_INT,
		G_TYPE_INT,
		GDK_TYPE_DRAG_CONTEXT,
		G_TYPE_INT,
		G_TYPE_INT,
		G_TYPE_UINT);

	et_signals[TABLE_DRAG_DATA_RECEIVED] = g_signal_new (
		"table_drag_data_received",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableClass, table_drag_data_received),
		NULL, NULL,
		e_marshal_VOID__INT_INT_OBJECT_INT_INT_BOXED_UINT_UINT,
		G_TYPE_NONE, 8,
		G_TYPE_INT,
		G_TYPE_INT,
		GDK_TYPE_DRAG_CONTEXT,
		G_TYPE_INT,
		G_TYPE_INT,
		GTK_TYPE_SELECTION_DATA | G_SIGNAL_TYPE_STATIC_SCOPE,
		G_TYPE_UINT,
		G_TYPE_UINT);

	g_object_class_install_property (
		object_class,
		PROP_LENGTH_THRESHOLD,
		g_param_spec_int (
			"length_threshold",
			"Length Threshold",
			NULL,
			0, G_MAXINT, 0,
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		object_class,
		PROP_UNIFORM_ROW_HEIGHT,
		g_param_spec_boolean (
			"uniform_row_height",
			"Uniform row height",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_ALWAYS_SEARCH,
		g_param_spec_boolean (
			"always_search",
			"Always search",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_USE_CLICK_TO_ADD,
		g_param_spec_boolean (
			"use_click_to_add",
			"Use click to add",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_MODEL,
		g_param_spec_object (
			"model",
			"Model",
			NULL,
			E_TYPE_TABLE_MODEL,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_IS_EDITING,
		g_param_spec_boolean (
			"is-editing",
			"Whether is in an editing mode",
			"Whether is in an editing mode",
			FALSE,
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
		GAL_A11Y_TYPE_E_TABLE);
}

static gboolean
e_table_scrollable_get_border (GtkScrollable *scrollable,
			       GtkBorder *border)
{
	ETable *table;
	ETableHeaderItem *header_item;

	g_return_val_if_fail (E_IS_TABLE (scrollable), FALSE);
	g_return_val_if_fail (border != NULL, FALSE);

	table = E_TABLE (scrollable);
	if (!table->header_item)
		return FALSE;

	g_return_val_if_fail (E_IS_TABLE_HEADER_ITEM (table->header_item), FALSE);

	header_item = E_TABLE_HEADER_ITEM (table->header_item);

	border->top = header_item->height;

	return TRUE;
}

static void
e_table_scrollable_init (GtkScrollableInterface *iface)
{
	iface->get_border = e_table_scrollable_get_border;
}

void
e_table_freeze_state_change (ETable *table)
{
	g_return_if_fail (table != NULL);

	table->state_change_freeze++;
	if (table->state_change_freeze == 1)
		table->state_changed = FALSE;

	g_return_if_fail (table->state_change_freeze != 0);
}

void
e_table_thaw_state_change (ETable *table)
{
	g_return_if_fail (table != NULL);
	g_return_if_fail (table->state_change_freeze != 0);

	table->state_change_freeze--;
	if (table->state_change_freeze == 0 && table->state_changed) {
		table->state_changed = FALSE;
		e_table_state_change (table);
	}
}

gboolean
e_table_is_editing (ETable *table)
{
	g_return_val_if_fail (E_IS_TABLE (table), FALSE);

	return (table->click_to_add && e_table_click_to_add_is_editing (E_TABLE_CLICK_TO_ADD (table->click_to_add))) ||
	       (table->group && e_table_group_is_editing (table->group));
}

void
e_table_customize_view (ETable *table)
{
	g_return_if_fail (E_IS_TABLE (table));

	if (table->header_item)
		e_table_header_item_customize_view (E_TABLE_HEADER_ITEM (table->header_item));
}

static void
table_size_allocate (GtkWidget *widget,
		     GtkAllocation *alloc,
		     ETable *table)
{
	gdouble width;

	g_return_if_fail (E_IS_TABLE (table));
	g_return_if_fail (table->priv->info_text != NULL);

	gnome_canvas_get_scroll_region (
		GNOME_CANVAS (table->table_canvas),
		NULL, NULL, &width, NULL);

	width -= 60.0;

	g_object_set (table->priv->info_text,
		"width", width,
		"clip_width", width,
		NULL);
}

/**
 * e_table_set_info_message:
 * @table: #ETable instance
 * @info_message: Message to set. Can be NULL.
 *
 * Creates an info message in table area, or removes old.
 **/
void
e_table_set_info_message (ETable *table,
			  const gchar *info_message)
{
	GtkAllocation allocation;
	GtkWidget *widget;

	g_return_if_fail (E_IS_TABLE (table));

	if (!table->priv->info_text && (!info_message || !*info_message))
		return;

	if (!info_message || !*info_message) {
		g_signal_handler_disconnect (table, table->priv->info_text_resize_id);
		g_object_run_dispose (G_OBJECT (table->priv->info_text));
		table->priv->info_text = NULL;
		return;
	}

	widget = GTK_WIDGET (table->table_canvas);
	gtk_widget_get_allocation (widget, &allocation);

	if (!table->priv->info_text) {
		if (allocation.width > 60) {
			table->priv->info_text = gnome_canvas_item_new (
				GNOME_CANVAS_GROUP (gnome_canvas_root (table->table_canvas)),
				e_text_get_type (),
				"line_wrap", TRUE,
				"clip", TRUE,
				"justification", GTK_JUSTIFY_LEFT,
				"text", info_message,
				"width", (gdouble) allocation.width - 60.0,
				"clip_width", (gdouble) allocation.width - 60.0,
				NULL);

			e_canvas_item_move_absolute (table->priv->info_text, 30, 30);

			table->priv->info_text_resize_id = g_signal_connect_object (
				table, "size_allocate",
				G_CALLBACK (table_size_allocate), table, 0);
		}
	} else
		gnome_canvas_item_set (table->priv->info_text, "text", info_message, NULL);
}
