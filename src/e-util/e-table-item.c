/*
 * e-table-item.c
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
 *		Miguel de Icaza <miguel@gnu.org>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */
/*
 * TODO:
 *   Add a border to the thing, so that focusing works properly.
 */

#include "evolution-config.h"

#include "e-table-item.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "e-canvas-utils.h"
#include "e-canvas.h"
#include "e-cell.h"
#include "e-cell-text.h"
#include "e-marshal.h"
#include "e-table-subset.h"
#include "gal-a11y-e-table-item-factory.h"
#include "gal-a11y-e-table-item.h"

#define FOCUSED_BORDER 2

#define d(x)

#if d(!)0
#define e_table_item_leave_edit_(x) (e_table_item_leave_edit((x)), g_print ("%s: e_table_item_leave_edit\n", G_STRFUNC))
#else
#define e_table_item_leave_edit_(x) (e_table_item_leave_edit((x)))
#endif

typedef struct _ETableItemPrivate ETableItemPrivate;

struct _ETableItemPrivate {
	GSource *show_cursor_delay_source;
};

G_DEFINE_TYPE_WITH_PRIVATE (ETableItem, e_table_item, GNOME_TYPE_CANVAS_ITEM)

static void eti_check_cursor_bounds (ETableItem *eti);
static void eti_cancel_drag_due_to_model_change (ETableItem *eti);

/* FIXME: Do an analysis of which cell functions are needed before
 * realize and make sure that all of them are doable by all the cells
 * and that all of the others are only done after realization. */

enum {
	CURSOR_CHANGE,
	CURSOR_ACTIVATED,
	DOUBLE_CLICK,
	RIGHT_CLICK,
	CLICK,
	KEY_PRESS,
	START_DRAG,
	STYLE_UPDATED,
	SELECTION_MODEL_REMOVED,
	SELECTION_MODEL_ADDED,
	GET_BG_COLOR,
	LAST_SIGNAL
};

static guint eti_signals[LAST_SIGNAL] = { 0, };

enum {
	PROP_0,
	PROP_TABLE_HEADER,
	PROP_TABLE_MODEL,
	PROP_SELECTION_MODEL,
	PROP_TABLE_ALTERNATING_ROW_COLORS,
	PROP_TABLE_HORIZONTAL_DRAW_GRID,
	PROP_TABLE_VERTICAL_DRAW_GRID,
	PROP_TABLE_DRAW_FOCUS,
	PROP_CURSOR_MODE,
	PROP_LENGTH_THRESHOLD,
	PROP_CURSOR_ROW,
	PROP_UNIFORM_ROW_HEIGHT,
	PROP_IS_EDITING,

	PROP_MINIMUM_WIDTH,
	PROP_WIDTH,
	PROP_HEIGHT
};

#define DOUBLE_CLICK_TIME      250
#define TRIPLE_CLICK_TIME      500

static gint eti_get_height (ETableItem *eti);
static gint eti_row_height (ETableItem *eti, gint row);
static void e_table_item_focus (ETableItem *eti, gint col, gint row, GdkModifierType state);
static void eti_cursor_change (ESelectionModel *selection, gint row, gint col, ETableItem *eti);
static void eti_cursor_activated (ESelectionModel *selection, gint row, gint col, ETableItem *eti);
static void eti_selection_change (ESelectionModel *selection, ETableItem *eti);
static void eti_selection_row_change (ESelectionModel *selection, gint row, ETableItem *eti);
static void e_table_item_redraw_row (ETableItem *eti, gint row);

#define ETI_SINGLE_ROW_HEIGHT(eti) ((eti)->uniform_row_height_cache != -1 ? (eti)->uniform_row_height_cache : eti_row_height((eti), -1))
#define ETI_MULTIPLE_ROW_HEIGHT(eti,row) ((eti)->height_cache && (eti)->height_cache[(row)] != -1 ? (eti)->height_cache[(row)] : eti_row_height((eti),(row)))
#define ETI_ROW_HEIGHT(eti,row) ((eti)->uniform_row_height ? ETI_SINGLE_ROW_HEIGHT ((eti)) : ETI_MULTIPLE_ROW_HEIGHT((eti),(row)))

/* tweak_hsv is a really tweaky function. it modifies its first argument, which
 * should be the color you want tweaked. delta_h, delta_s and delta_v specify
 * how much you want their respective channels modified (and in what direction).
 * if it can't do the specified modification, it does it in the oppositon direction */
static void
e_hsv_tweak (GdkRGBA *rgba,
             gdouble delta_h,
             gdouble delta_s,
             gdouble delta_v)
{
	gdouble h, s, v, r, g, b;

	r = rgba->red;
	g = rgba->green;
	b = rgba->blue;

	gtk_rgb_to_hsv (r, g, b, &h, &s, &v);

	if (h + delta_h < 0) {
		h -= delta_h;
	} else {
		h += delta_h;
	}

	if (s + delta_s < 0) {
		s -= delta_s;
	} else {
		s += delta_s;
	}

	if (v + delta_v < 0) {
		v -= delta_v;
	} else {
		v += delta_v;
	}

	gtk_hsv_to_rgb (h, s, v, &r, &g, &b);

	rgba->red = r;
	rgba->green = g;
	rgba->blue = b;
}

inline static gint
model_to_view_row (ETableItem *eti,
                   gint row)
{
	if (row == -1)
		return -1;
	if (eti->uses_source_model) {
		gint model_row;

		model_row = e_table_subset_view_to_model_row (
			E_TABLE_SUBSET (eti->table_model), eti->row_guess);
		if (model_row >= 0 && model_row == row)
			return eti->row_guess;

		return e_table_subset_model_to_view_row (
			E_TABLE_SUBSET (eti->table_model), row);
	} else
		return row;
}

inline static gint
view_to_model_row (ETableItem *eti,
                   gint row)
{
	if (eti->uses_source_model) {
		gint model_row;

		model_row = e_table_subset_view_to_model_row (
			E_TABLE_SUBSET (eti->table_model), row);

		if (model_row >= 0)
			eti->row_guess = row;

		return model_row;
	} else
		return row;
}

inline static gint
model_to_view_col (ETableItem *eti,
                   gint model_col)
{
	gint i;
	if (model_col == -1)
		return -1;
	for (i = 0; i < eti->cols; i++) {
		ETableCol *ecol = e_table_header_get_column (eti->header, i);
		if (ecol->spec->model_col == model_col)
			return i;
	}
	return -1;
}

inline static gint
view_to_model_col (ETableItem *eti,
                   gint view_col)
{
	ETableCol *ecol = e_table_header_get_column (eti->header, view_col);

	return (ecol != NULL) ? ecol->spec->model_col : -1;
}

static void
grab_cancelled (ECanvas *canvas,
                GnomeCanvasItem *item,
                gpointer data)
{
	ETableItem *eti = data;

	eti->grab_cancelled = TRUE;
}

inline static void
eti_grab (ETableItem *eti,
          GdkDevice *device,
          guint32 time)
{
	GnomeCanvasItem *item = GNOME_CANVAS_ITEM (eti);
	d (g_print ("%s: time: %d\n", G_STRFUNC, time));
	if (eti->grabbed_count == 0) {
		GdkGrabStatus grab_status;

		eti->gtk_grabbed = FALSE;
		eti->grab_cancelled = FALSE;

		grab_status = e_canvas_item_grab (
			E_CANVAS (item->canvas),
			item,
			GDK_BUTTON1_MOTION_MASK |
			GDK_BUTTON2_MOTION_MASK |
			GDK_BUTTON3_MOTION_MASK |
			GDK_POINTER_MOTION_MASK |
			GDK_BUTTON_PRESS_MASK |
			GDK_BUTTON_RELEASE_MASK,
			NULL,
			device, time,
			grab_cancelled,
			eti);

		if (grab_status != GDK_GRAB_SUCCESS) {
			d (g_print ("%s: gtk_grab_add\n", G_STRFUNC));
			gtk_grab_add (GTK_WIDGET (item->canvas));
			eti->gtk_grabbed = TRUE;
		}
	}
	eti->grabbed_count++;
}

inline static void
eti_ungrab (ETableItem *eti,
            guint32 time)
{
	GnomeCanvasItem *item = GNOME_CANVAS_ITEM (eti);
	gboolean was_grabbed;

	d (g_print ("%s: time: %d\n", G_STRFUNC, time));

	was_grabbed = eti->grabbed_count > 0;
	if (was_grabbed)
		eti->grabbed_count--;

	if (eti->grabbed_count == 0) {
		if (eti->grab_cancelled) {
			eti->grab_cancelled = FALSE;
		} else {
			if (eti->gtk_grabbed) {
				d (g_print ("%s: gtk_grab_remove\n", G_STRFUNC));
				gtk_grab_remove (GTK_WIDGET (item->canvas));
				eti->gtk_grabbed = FALSE;
			}
			if (was_grabbed)
				gnome_canvas_item_ungrab (item, time);
			eti->grabbed_col = -1;
			eti->grabbed_row = -1;
		}
	}
}

inline static gboolean
eti_editing (ETableItem *eti)
{
	d (g_print ("%s: %s\n", G_STRFUNC, (eti->editing_col == -1) ? "false":"true"));

	if (eti->editing_col == -1)
		return FALSE;
	else
		return TRUE;
}

static void
eti_get_cell_background_color (ETableItem *eti,
                               gint row,
                               gint col,
                               gboolean selected,
                               GdkRGBA *background)
{
	ECellView *ecell_view = eti->cell_views[col];
	GtkWidget *canvas;
	gboolean was_set = FALSE;
	gchar *color_spec = NULL;

	canvas = GTK_WIDGET (GNOME_CANVAS_ITEM (eti)->canvas);

	g_signal_emit (eti, eti_signals[GET_BG_COLOR], 0, row, col, background, &was_set);

	if (!was_set) {
		color_spec = e_cell_get_bg_color (ecell_view, row);

		if (color_spec != NULL) {
			GdkRGBA bg;

			if (gdk_rgba_parse (&bg, color_spec)) {
				*background = bg;
				was_set = TRUE;
			}
		}

		g_free (color_spec);
	}

	if (!was_set) {
		if (selected) {
			if (gtk_widget_has_focus (canvas))
				e_utils_get_theme_color (canvas, "theme_selected_bg_color", E_UTILS_DEFAULT_THEME_SELECTED_BG_COLOR, background);
			else
				e_utils_get_theme_color (canvas, "theme_unfocused_selected_bg_color,theme_selected_bg_color", E_UTILS_DEFAULT_THEME_UNFOCUSED_SELECTED_BG_COLOR, background);
		} else {
			e_utils_get_theme_color (canvas, "theme_base_color", E_UTILS_DEFAULT_THEME_BASE_COLOR, background);
		}
	}

	if (eti->alternating_row_colors) {
		if (row % 2) {

		} else {
			e_hsv_tweak (background, 0.0f, 0.0f, -0.07f);
		}
	}
}

static void
eti_free_save_state (ETableItem *eti)
{
	if (eti->save_row == -1 ||
	    !eti->cell_views_realized)
		return;

	e_cell_free_state (
		eti->cell_views[eti->save_col], view_to_model_col (eti, eti->save_col),
		eti->save_col, eti->save_row, eti->save_state);
	eti->save_row = -1;
	eti->save_col = -1;
	eti->save_state = NULL;
}

/*
 * During realization, we have to invoke the per-ecell realize routine
 * (On our current setup, we have one e-cell per column.
 *
 * We might want to optimize this to only realize the unique e-cells:
 * ie, a strings-only table, uses the same e-cell for every column, and
 * we might want to avoid realizing each e-cell.
 */
static void
eti_realize_cell_views (ETableItem *eti)
{
	GnomeCanvasItem *item;
	gint i;

	item = GNOME_CANVAS_ITEM (eti);

	if (eti->cell_views_realized)
		return;

	if (!(item->flags & GNOME_CANVAS_ITEM_REALIZED))
		return;

	for (i = 0; i < eti->n_cells; i++)
		e_cell_realize (eti->cell_views[i]);
	eti->cell_views_realized = 1;
}

static void
eti_attach_cell_views (ETableItem *eti)
{
	gint i;

	g_return_if_fail (eti->header);
	g_return_if_fail (eti->table_model);

	/* this is just c&p from model pre change, but it fixes things */
	eti_cancel_drag_due_to_model_change (eti);
	eti_check_cursor_bounds (eti);
	if (eti_editing (eti))
		e_table_item_leave_edit_(eti);
	eti->motion_row = -1;
	eti->motion_col = -1;

	/*
	 * Now realize the various ECells
	 */
	eti->n_cells = eti->cols;
	eti->cell_views = g_new (ECellView *, eti->n_cells);

	for (i = 0; i < eti->n_cells; i++) {
		ETableCol *ecol = e_table_header_get_column (eti->header, i);

		eti->cell_views[i] = e_cell_new_view (ecol->ecell, eti->table_model, eti);
	}

	eti->needs_compute_height = 1;
	e_canvas_item_request_reflow (GNOME_CANVAS_ITEM (eti));
	eti->needs_redraw = 1;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (eti));
}

/*
 * During unrealization: we invoke every e-cell (one per column in the current
 * setup) to dispose all X resources allocated
 */
static void
eti_unrealize_cell_views (ETableItem *eti)
{
	gint i;

	if (eti->cell_views_realized == 0)
		return;

	eti_free_save_state (eti);

	for (i = 0; i < eti->n_cells; i++)
		e_cell_unrealize (eti->cell_views[i]);
	eti->cell_views_realized = 0;
}

static void
eti_detach_cell_views (ETableItem *eti)
{
	gint i;

	eti_free_save_state (eti);

	for (i = 0; i < eti->n_cells; i++) {
		e_cell_kill_view (eti->cell_views[i]);
		eti->cell_views[i] = NULL;
	}

	g_free (eti->cell_views);
	eti->cell_views = NULL;
	eti->n_cells = 0;
}

static void
eti_bounds (GnomeCanvasItem *item,
            gdouble *x1,
            gdouble *y1,
            gdouble *x2,
            gdouble *y2)
{
	cairo_matrix_t i2c;
	ETableItem *eti = E_TABLE_ITEM (item);

	/* Wrong BBox's are the source of redraw nightmares */

	*x1 = 0;
	*y1 = 0;
	*x2 = eti->width;
	*y2 = eti->height;

	gnome_canvas_item_i2c_matrix (GNOME_CANVAS_ITEM (eti), &i2c);
	gnome_canvas_matrix_transform_rect (&i2c, x1, y1, x2, y2);
}

static void
eti_reflow (GnomeCanvasItem *item,
            gint flags)
{
	ETableItem *eti = E_TABLE_ITEM (item);

	if (eti->needs_compute_height) {
		gint new_height = eti_get_height (eti);

		if (new_height != eti->height) {
			eti->height = new_height;
			e_canvas_item_request_parent_reflow (GNOME_CANVAS_ITEM (eti));
			eti->needs_redraw = 1;
			gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (eti));
		}
		eti->needs_compute_height = 0;
	}
	if (eti->needs_compute_width) {
		gint new_width = e_table_header_total_width (eti->header);
		if (new_width != eti->width) {
			eti->width = new_width;
			e_canvas_item_request_parent_reflow (GNOME_CANVAS_ITEM (eti));
			eti->needs_redraw = 1;
			gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (eti));
		}
		eti->needs_compute_width = 0;
	}
}

/*
 * GnomeCanvasItem::update method
 */
static void
eti_update (GnomeCanvasItem *item,
            const cairo_matrix_t *i2c,
            gint flags)
{
	ETableItem *eti = E_TABLE_ITEM (item);
	gdouble x1, x2, y1, y2;

	if (GNOME_CANVAS_ITEM_CLASS (e_table_item_parent_class)->update)
		(*GNOME_CANVAS_ITEM_CLASS (e_table_item_parent_class)->update)(item, i2c, flags);

	x1 = item->x1;
	y1 = item->y1;
	x2 = item->x2;
	y2 = item->y2;

	eti_bounds (item, &item->x1, &item->y1, &item->x2, &item->y2);
	if (item->x1 != x1 ||
	    item->y1 != y1 ||
	    item->x2 != x2 ||
	    item->y2 != y2) {
		gnome_canvas_request_redraw (item->canvas, x1, y1, x2, y2);
		eti->needs_redraw = 1;
	}

	if (eti->needs_redraw) {
		gnome_canvas_request_redraw (
			item->canvas, item->x1, item->y1,
			item->x2, item->y2);
		eti->needs_redraw = 0;
	}
}

/*
 * eti_remove_table_model:
 *
 * Invoked to release the table model associated with this ETableItem
 */
static void
eti_remove_table_model (ETableItem *eti)
{
	if (!eti->table_model)
		return;

	g_signal_handler_disconnect (
		eti->table_model,
		eti->table_model_pre_change_id);
	g_signal_handler_disconnect (
		eti->table_model,
		eti->table_model_no_change_id);
	g_signal_handler_disconnect (
		eti->table_model,
		eti->table_model_change_id);
	g_signal_handler_disconnect (
		eti->table_model,
		eti->table_model_row_change_id);
	g_signal_handler_disconnect (
		eti->table_model,
		eti->table_model_cell_change_id);
	g_signal_handler_disconnect (
		eti->table_model,
		eti->table_model_rows_inserted_id);
	g_signal_handler_disconnect (
		eti->table_model,
		eti->table_model_rows_deleted_id);
	g_object_unref (eti->table_model);
	if (eti->source_model)
		g_object_unref (eti->source_model);

	eti->table_model_pre_change_id = 0;
	eti->table_model_no_change_id = 0;
	eti->table_model_change_id = 0;
	eti->table_model_row_change_id = 0;
	eti->table_model_cell_change_id = 0;
	eti->table_model_rows_inserted_id = 0;
	eti->table_model_rows_deleted_id = 0;
	eti->table_model = NULL;
	eti->source_model = NULL;
	eti->uses_source_model = 0;
}

/*
 * eti_remove_table_model:
 *
 * Invoked to release the table model associated with this ETableItem
 */
static void
eti_remove_selection_model (ETableItem *eti)
{
	if (!eti->selection)
		return;

	g_signal_handler_disconnect (
		eti->selection,
		eti->selection_change_id);
	g_signal_handler_disconnect (
		eti->selection,
		eti->selection_row_change_id);
	g_signal_handler_disconnect (
		eti->selection,
		eti->cursor_change_id);
	g_signal_handler_disconnect (
		eti->selection,
		eti->cursor_activated_id);
	g_object_unref (eti->selection);

	eti->selection_change_id = 0;
	eti->selection_row_change_id = 0;
	eti->cursor_activated_id = 0;
	eti->selection = NULL;
}

/*
 * eti_remove_header_model:
 *
 * Invoked to release the header model associated with this ETableItem
 */
static void
eti_remove_header_model (ETableItem *eti)
{
	if (!eti->header)
		return;

	g_signal_handler_disconnect (
		eti->header,
		eti->header_structure_change_id);
	g_signal_handler_disconnect (
		eti->header,
		eti->header_dim_change_id);
	g_signal_handler_disconnect (
		eti->header,
		eti->header_request_width_id);

	if (eti->cell_views) {
		eti_unrealize_cell_views (eti);
		eti_detach_cell_views (eti);
	}
	g_object_unref (eti->header);

	eti->header_structure_change_id = 0;
	eti->header_dim_change_id = 0;
	eti->header_request_width_id = 0;
	eti->header = NULL;
}

/*
 * eti_row_height_real:
 *
 * Returns the height used by row @row.  This does not include the one-pixel
 * used as a separator between rows
 */
static gint
eti_row_height_real (ETableItem *eti,
                     gint row)
{
	const gint cols = e_table_header_count (eti->header);
	gint col;
	gint h, max_h;

	g_return_val_if_fail (cols == 0 || eti->cell_views, 0);

	max_h = 0;

	for (col = 0; col < cols; col++) {
		h = e_cell_height (eti->cell_views[col], view_to_model_col (eti, col), col, row);

		if (h > max_h)
			max_h = h;
	}
	return max_h;
}

static void
confirm_height_cache (ETableItem *eti)
{
	gint i;

	if (eti->uniform_row_height || eti->height_cache)
		return;
	eti->height_cache = g_new (int, eti->rows);
	for (i = 0; i < eti->rows; i++) {
		eti->height_cache[i] = -1;
	}
}

static gboolean
height_cache_idle (ETableItem *eti)
{
	gint changed = 0;
	gint i;
	confirm_height_cache (eti);
	for (i = eti->height_cache_idle_count; i < eti->rows; i++) {
		if (eti->height_cache[i] == -1) {
			eti_row_height (eti, i);
			changed++;
			if (changed >= 20)
				break;
		}
	}
	if (changed >= 20) {
		eti->height_cache_idle_count = i;
		return TRUE;
	}
	eti->height_cache_idle_id = 0;
	return FALSE;
}

static void
free_height_cache (ETableItem *eti)
{
	GnomeCanvasItem *item;

	item = GNOME_CANVAS_ITEM (eti);

	if (item->flags & GNOME_CANVAS_ITEM_REALIZED) {
		g_clear_pointer (&eti->height_cache, g_free);
		eti->height_cache_idle_count = 0;
		eti->uniform_row_height_cache = -1;

		if (eti->uniform_row_height && eti->height_cache_idle_id != 0) {
			g_source_remove (eti->height_cache_idle_id);
			eti->height_cache_idle_id = 0;
		}

		if ((!eti->uniform_row_height) && eti->height_cache_idle_id == 0)
			eti->height_cache_idle_id = g_idle_add_full (G_PRIORITY_LOW, (GSourceFunc) height_cache_idle, eti, NULL);
	}
}

static void
calculate_height_cache (ETableItem *eti)
{
	free_height_cache (eti);
	confirm_height_cache (eti);
}

/*
 * eti_row_height:
 *
 * Returns the height used by row @row.  This does not include the one-pixel
 * used as a separator between rows
 */
static gint
eti_row_height (ETableItem *eti,
                gint row)
{
	if (eti->uniform_row_height) {
		eti->uniform_row_height_cache = eti_row_height_real (eti, -1);
		return eti->uniform_row_height_cache;
	} else {
		if (!eti->height_cache) {
			calculate_height_cache (eti);
		}
		if (eti->height_cache[row] == -1) {
			eti->height_cache[row] = eti_row_height_real (eti, row);
			if (row > 0 &&
			    eti->length_threshold != -1 &&
			    eti->rows > eti->length_threshold &&
			    eti->height_cache[row] != eti_row_height (eti, 0)) {
				eti->needs_compute_height = 1;
				e_canvas_item_request_reflow (GNOME_CANVAS_ITEM (eti));
			}
		}
		return eti->height_cache[row];
	}
}

/*
 * eti_get_height:
 *
 * Returns the height of the ETableItem.
 *
 * The ETableItem might compute the whole height by asking every row its
 * size.  There is a special mode (designed to work when there are too
 * many rows in the table that performing the previous step could take
 * too long) set by the ETableItem->length_threshold that would determine
 * when the height is computed by using the first row as the size for
 * every other row in the ETableItem.
 */
static gint
eti_get_height (ETableItem *eti)
{
	const gint rows = eti->rows;
	gint height_extra = eti->horizontal_draw_grid ? 1 : 0;

	if (rows == 0)
		return 0;

	if (eti->uniform_row_height) {
		gint row_height = ETI_ROW_HEIGHT (eti, -1);
		return ((row_height + height_extra) * rows + height_extra);
	} else {
		gint height;
		gint row;
		if (eti->length_threshold != -1) {
			if (rows > eti->length_threshold) {
				gint row_height = ETI_ROW_HEIGHT (eti, 0);
				if (eti->height_cache) {
					height = 0;
					for (row = 0; row < rows; row++) {
						if (eti->height_cache[row] == -1) {
							height += (row_height + height_extra) * (rows - row);
							break;
						}
						else
							height += eti->height_cache[row] + height_extra;
					}
				} else
					height = (ETI_ROW_HEIGHT (eti, 0) + height_extra) * rows;

				/*
				 * 1 pixel at the top
				 */
				return height + height_extra;
			}
		}

		height = height_extra;
		for (row = 0; row < rows; row++)
			height += ETI_ROW_HEIGHT (eti, row) + height_extra;

		return height;
	}
}

static void
eti_item_region_redraw (ETableItem *eti,
                        gint x0,
                        gint y0,
                        gint x1,
                        gint y1)
{
	GnomeCanvasItem *item = GNOME_CANVAS_ITEM (eti);
	gdouble dx1, dy1, dx2, dy2;
	cairo_matrix_t i2c;

	dx1 = x0;
	dy1 = y0;
	dx2 = x1;
	dy2 = y1;

	gnome_canvas_item_i2c_matrix (item, &i2c);
	gnome_canvas_matrix_transform_rect (&i2c, &dx1, &dy1, &dx2, &dy2);

	gnome_canvas_request_redraw (item->canvas, floor (dx1), floor (dy1), ceil (dx2), ceil (dy2));
}

/*
 * Computes the distance between @start_row and @end_row in pixels
 */
gint
e_table_item_row_diff (ETableItem *eti,
                       gint start_row,
                       gint end_row)
{
	gint height_extra = eti->horizontal_draw_grid ? 1 : 0;

	if (start_row < 0)
		start_row = 0;
	if (end_row > eti->rows)
		end_row = eti->rows;

	if (eti->uniform_row_height) {
		return ((end_row - start_row) * (ETI_ROW_HEIGHT (eti, -1) + height_extra));
	} else {
		gint row, total;
		total = 0;
		for (row = start_row; row < end_row; row++)
			total += ETI_ROW_HEIGHT (eti, row) + height_extra;

		return total;
	}
}

static void
eti_get_region (ETableItem *eti,
                gint start_col,
                gint start_row,
                gint end_col,
                gint end_row,
                gint *x1p,
                gint *y1p,
                gint *x2p,
                gint *y2p)
{
	gint x1, y1, x2, y2;

	x1 = e_table_header_col_diff (eti->header, 0, start_col);
	y1 = e_table_item_row_diff (eti, 0, start_row);
	x2 = x1 + e_table_header_col_diff (eti->header, start_col, end_col + 1);
	y2 = y1 + e_table_item_row_diff (eti, start_row, end_row + 1);
	if (x1p)
		*x1p = x1;
	if (y1p)
		*y1p = y1;
	if (x2p)
		*x2p = x2;
	if (y2p)
		*y2p = y2;
}

/*
 * eti_request_region_redraw:
 *
 * Request a canvas redraw on the range (start_col, start_row) to (end_col, end_row).
 * This is inclusive (ie, you can use: 0,0-0,0 to redraw the first cell).
 *
 * The @border argument is a number of pixels around the region that should also be queued
 * for redraw.   This is typically used by the focus routines to queue a redraw for the
 * border as well.
 */
static void
eti_request_region_redraw (ETableItem *eti,
                           gint start_col,
                           gint start_row,
                           gint end_col,
                           gint end_row,
                           gint border)
{
	gint x1, y1, x2, y2;

	if (eti->rows > 0) {

		eti_get_region (
			eti,
			start_col, start_row,
			end_col, end_row,
			&x1, &y1, &x2, &y2);

		eti_item_region_redraw (
			eti,
			x1 - border,
			y1 - border,
			x2 + 1 + border,
			y2 + 1 + border);
	}
}

/*
 * eti_request_region_show
 *
 * Request a canvas show on the range (start_col, start_row) to (end_col, end_row).
 * This is inclusive (ie, you can use: 0,0-0,0 to show the first cell).
 */
static void
eti_request_region_show (ETableItem *eti,
                         gint start_col,
                         gint start_row,
                         gint end_col,
                         gint end_row,
                         gint delay)
{
	ETableItemPrivate *priv = e_table_item_get_instance_private (eti);
	gint x1, y1, x2, y2;

	if (priv->show_cursor_delay_source) {
		g_source_destroy (priv->show_cursor_delay_source);
		g_source_unref (priv->show_cursor_delay_source);
		priv->show_cursor_delay_source = NULL;
	}

	eti_get_region (
		eti,
		start_col, start_row,
		end_col, end_row,
		&x1, &y1, &x2, &y2);

	if (delay)
		priv->show_cursor_delay_source = e_canvas_item_show_area_delayed_ex (
			GNOME_CANVAS_ITEM (eti), x1, y1, x2, y2, delay);
	else
		e_canvas_item_show_area (
			GNOME_CANVAS_ITEM (eti), x1, y1, x2, y2);
}

static void
eti_show_cursor (ETableItem *eti,
                 gint delay)
{
	GnomeCanvasItem *item;
	gint cursor_row;

	item = GNOME_CANVAS_ITEM (eti);

	if (!((item->flags & GNOME_CANVAS_ITEM_REALIZED) && eti->cell_views_realized))
		return;

	if (eti->frozen_count > 0) {
		eti->queue_show_cursor = TRUE;
		return;
	}

#if 0
	g_object_get (
		eti->selection,
		"cursor_row", &cursor_row,
		NULL);
#else
	cursor_row = e_selection_model_cursor_row (eti->selection);
#endif

	d (g_print ("%s: cursor row: %d\n", G_STRFUNC, cursor_row));

	if (cursor_row != -1) {
		cursor_row = model_to_view_row (eti, cursor_row);
		eti_request_region_show (
			eti,
			0, cursor_row, eti->cols - 1, cursor_row,
			delay);
	}
}

static void
eti_check_cursor_bounds (ETableItem *eti)
{
	GnomeCanvasItem *item;
	gint x1, y1, x2, y2;
	gint cursor_row;

	item = GNOME_CANVAS_ITEM (eti);

	if (!((item->flags & GNOME_CANVAS_ITEM_REALIZED) && eti->cell_views_realized))
		return;

	if (eti->frozen_count > 0) {
		return;
	}

	g_object_get (
		eti->selection,
		"cursor_row", &cursor_row,
		NULL);

	if (cursor_row == -1) {
		eti->cursor_x1 = -1;
		eti->cursor_y1 = -1;
		eti->cursor_x2 = -1;
		eti->cursor_y2 = -1;
		eti->cursor_on_screen = TRUE;
		return;
	}

	d (g_print ("%s: model cursor row: %d\n", G_STRFUNC, cursor_row));

	cursor_row = model_to_view_row (eti, cursor_row);

	d (g_print ("%s: cursor row: %d\n", G_STRFUNC, cursor_row));

	eti_get_region (
		eti,
		0, cursor_row, eti->cols - 1, cursor_row,
		&x1, &y1, &x2, &y2);
	eti->cursor_x1 = x1;
	eti->cursor_y1 = y1;
	eti->cursor_x2 = x2;
	eti->cursor_y2 = y2;
	eti->cursor_on_screen = e_canvas_item_area_shown (GNOME_CANVAS_ITEM (eti), x1, y1, x2, y2);

	d (g_print ("%s: cursor on screen: %s\n", G_STRFUNC, eti->cursor_on_screen ? "TRUE" : "FALSE"));
}

static void
eti_maybe_show_cursor (ETableItem *eti,
                       gint delay)
{
	d (g_print ("%s: cursor on screen: %s\n", G_STRFUNC, eti->cursor_on_screen ? "TRUE" : "FALSE"));
	if (eti->cursor_on_screen)
		eti_show_cursor (eti, delay);
	eti_check_cursor_bounds (eti);
}

static gboolean
eti_idle_show_cursor_cb (gpointer data)
{
	ETableItem *eti = data;

	if (eti->selection) {
		eti_show_cursor (eti, 0);
		eti_check_cursor_bounds (eti);
	}

	eti->cursor_idle_id = 0;
	g_object_unref (eti);
	return FALSE;
}

static void
eti_idle_maybe_show_cursor (ETableItem *eti)
{
	d (g_print ("%s: cursor on screen: %s\n", G_STRFUNC, eti->cursor_on_screen ? "TRUE" : "FALSE"));
	if (eti->cursor_on_screen) {
		g_object_ref (eti);
		if (!eti->cursor_idle_id)
			eti->cursor_idle_id = g_idle_add (eti_idle_show_cursor_cb, eti);
	}
}

static void
eti_cancel_drag_due_to_model_change (ETableItem *eti)
{
	if (eti->maybe_in_drag) {
		eti->maybe_in_drag = FALSE;
		if (!eti->maybe_did_something)
			e_selection_model_do_something (E_SELECTION_MODEL (eti->selection), eti->drag_row, eti->drag_col, eti->drag_state);
	}
	if (eti->in_drag) {
		eti->in_drag = FALSE;
	}
}

static void
eti_freeze (ETableItem *eti)
{
	eti->frozen_count++;
	d (g_print ("%s: %d\n", G_STRFUNC, eti->frozen_count));
}

static void
eti_unfreeze (ETableItem *eti)
{
	if (eti->frozen_count <= 0)
		return;

	eti->frozen_count--;
	d (g_print ("%s: %d\n", G_STRFUNC, eti->frozen_count));
	if (eti->frozen_count == 0 && eti->queue_show_cursor) {
		eti_show_cursor (eti, 0);
		eti_check_cursor_bounds (eti);
		eti->queue_show_cursor = FALSE;
	}
}

void
e_table_item_freeze (ETableItem *eti)
{
	if (!eti)
		return;

	eti_freeze (eti);
}

void
e_table_item_thaw (ETableItem *eti)
{
	if (!eti)
		return;

	eti_unfreeze (eti);
}

/*
 * Callback routine: invoked before the ETableModel suffers a change
 */
static void
eti_table_model_pre_change (ETableModel *table_model,
                            ETableItem *eti)
{
	eti_cancel_drag_due_to_model_change (eti);
	eti_check_cursor_bounds (eti);
	if (eti_editing (eti))
		e_table_item_leave_edit_(eti);
	eti->motion_row = -1;
	eti->motion_col = -1;
	eti_freeze (eti);
}

/*
 * Callback routine: invoked when the ETableModel has not suffered a change
 */
static void
eti_table_model_no_change (ETableModel *table_model,
                           ETableItem *eti)
{
	eti_unfreeze (eti);
}

/*
 * Callback routine: invoked when the ETableModel has suffered a change
 */

static void
eti_table_model_changed (ETableModel *table_model,
                         ETableItem *eti)
{
	GnomeCanvasItem *item = GNOME_CANVAS_ITEM (eti);

	if (!(item->flags & GNOME_CANVAS_ITEM_REALIZED)) {
		eti_unfreeze (eti);
		return;
	}

	eti->rows = e_table_model_row_count (eti->table_model);

	free_height_cache (eti);

	eti_unfreeze (eti);

	eti->needs_compute_height = 1;
	e_canvas_item_request_reflow (GNOME_CANVAS_ITEM (eti));
	eti->needs_redraw = 1;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (eti));

	eti_idle_maybe_show_cursor (eti);
}

static void
eti_table_model_row_changed (ETableModel *table_model,
                             gint row,
                             ETableItem *eti)
{
	GnomeCanvasItem *item = GNOME_CANVAS_ITEM (eti);

	if (!(item->flags & GNOME_CANVAS_ITEM_REALIZED)) {
		eti_unfreeze (eti);
		return;
	}

	if ((!eti->uniform_row_height) && eti->height_cache && eti->height_cache[row] != -1 && eti_row_height_real (eti, row) != eti->height_cache[row]) {
		eti_table_model_changed (table_model, eti);
		return;
	}

	eti_unfreeze (eti);

	e_table_item_redraw_row (eti, row);
}

static void
eti_table_model_cell_changed (ETableModel *table_model,
                              gint col,
                              gint row,
                              ETableItem *eti)
{
	GnomeCanvasItem *item = GNOME_CANVAS_ITEM (eti);

	if (!(item->flags & GNOME_CANVAS_ITEM_REALIZED)) {
		eti_unfreeze (eti);
		return;
	}

	if ((!eti->uniform_row_height) && eti->height_cache && eti->height_cache[row] != -1 && eti_row_height_real (eti, row) != eti->height_cache[row]) {
		eti_table_model_changed (table_model, eti);
		return;
	}

	eti_unfreeze (eti);

	e_table_item_redraw_row (eti, row);
}

static void
eti_table_model_rows_inserted (ETableModel *table_model,
                               gint row,
                               gint count,
                               ETableItem *eti)
{
	GnomeCanvasItem *item = GNOME_CANVAS_ITEM (eti);

	if (!(item->flags & GNOME_CANVAS_ITEM_REALIZED)) {
		eti_unfreeze (eti);
		return;
	}
	eti->rows = e_table_model_row_count (eti->table_model);

	if (eti->height_cache) {
		gint i;
		eti->height_cache = g_renew (int, eti->height_cache, eti->rows);
		memmove (eti->height_cache + row + count, eti->height_cache + row, (eti->rows - count - row) * sizeof (gint));
		for (i = row; i < row + count; i++)
			eti->height_cache[i] = -1;
	}

	eti_unfreeze (eti);

	eti_idle_maybe_show_cursor (eti);

	eti->needs_compute_height = 1;
	e_canvas_item_request_reflow (GNOME_CANVAS_ITEM (eti));
	eti->needs_redraw = 1;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (eti));
}

static void
eti_table_model_rows_deleted (ETableModel *table_model,
                              gint row,
                              gint count,
                              ETableItem *eti)
{
	GnomeCanvasItem *item = GNOME_CANVAS_ITEM (eti);

	if (!(item->flags & GNOME_CANVAS_ITEM_REALIZED)) {
		eti_unfreeze (eti);
		return;
	}

	eti->rows = e_table_model_row_count (eti->table_model);

	if (eti->height_cache && (eti->rows > row)) {
		memmove (eti->height_cache + row, eti->height_cache + row + count, (eti->rows - row) * sizeof (gint));
	}

	eti_unfreeze (eti);

	eti_idle_maybe_show_cursor (eti);

	eti->needs_compute_height = 1;
	e_canvas_item_request_reflow (GNOME_CANVAS_ITEM (eti));
	eti->needs_redraw = 1;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (eti));
}

/**
 * e_table_item_redraw_range
 * @eti: %ETableItem which will be redrawn
 * @start_col: The first col to redraw.
 * @start_row: The first row to redraw.
 * @end_col: The last col to redraw.
 * @end_row: The last row to redraw.
 *
 * This routine redraws the given %ETableItem in the range given.  The
 * range is inclusive at both ends.
 */
void
e_table_item_redraw_range (ETableItem *eti,
                           gint start_col,
                           gint start_row,
                           gint end_col,
                           gint end_row)
{
	gint border;
	gint cursor_col, cursor_row;

	g_return_if_fail (eti != NULL);
	g_return_if_fail (E_IS_TABLE_ITEM (eti));

	g_object_get (
		eti->selection,
		"cursor_col", &cursor_col,
		"cursor_row", &cursor_row,
		NULL);

	if ((start_col == cursor_col) ||
	    (end_col == cursor_col) ||
	    (view_to_model_row (eti, start_row) == cursor_row) ||
	    (view_to_model_row (eti, end_row) == cursor_row))
		border = 2;
	else
		border = 0;

	eti_request_region_redraw (eti, start_col, start_row, end_col, end_row, border);
}

static void
e_table_item_redraw_row (ETableItem *eti,
                         gint row)
{
	if (row != -1)
		e_table_item_redraw_range (eti, 0, row, eti->cols - 1, row);
}

static void
eti_add_table_model (ETableItem *eti,
                     ETableModel *table_model)
{
	g_return_if_fail (eti->table_model == NULL);

	eti->table_model = table_model;
	g_object_ref (eti->table_model);

	eti->table_model_pre_change_id = g_signal_connect (
		table_model, "model_pre_change",
		G_CALLBACK (eti_table_model_pre_change), eti);

	eti->table_model_no_change_id = g_signal_connect (
		table_model, "model_no_change",
		G_CALLBACK (eti_table_model_no_change), eti);

	eti->table_model_change_id = g_signal_connect (
		table_model, "model_changed",
		G_CALLBACK (eti_table_model_changed), eti);

	eti->table_model_row_change_id = g_signal_connect (
		table_model, "model_row_changed",
		G_CALLBACK (eti_table_model_row_changed), eti);

	eti->table_model_cell_change_id = g_signal_connect (
		table_model, "model_cell_changed",
		G_CALLBACK (eti_table_model_cell_changed), eti);

	eti->table_model_rows_inserted_id = g_signal_connect (
		table_model, "model_rows_inserted",
		G_CALLBACK (eti_table_model_rows_inserted), eti);

	eti->table_model_rows_deleted_id = g_signal_connect (
		table_model, "model_rows_deleted",
		G_CALLBACK (eti_table_model_rows_deleted), eti);

	if (eti->header) {
		eti_detach_cell_views (eti);
		eti_attach_cell_views (eti);
	}

	if (E_IS_TABLE_SUBSET (table_model)) {
		eti->uses_source_model = 1;
		eti->source_model = e_table_subset_get_source_model (
			E_TABLE_SUBSET (table_model));
		if (eti->source_model)
			g_object_ref (eti->source_model);
	}

	eti_freeze (eti);

	eti_table_model_changed (table_model, eti);
}

static void
eti_add_selection_model (ETableItem *eti,
                         ESelectionModel *selection)
{
	g_return_if_fail (eti->selection == NULL);

	eti->selection = selection;
	g_object_ref (eti->selection);

	eti->selection_change_id = g_signal_connect (
		selection, "selection_changed",
		G_CALLBACK (eti_selection_change), eti);

	eti->selection_row_change_id = g_signal_connect (
		selection, "selection_row_changed",
		G_CALLBACK (eti_selection_row_change), eti);

	eti->cursor_change_id = g_signal_connect (
		selection, "cursor_changed",
		G_CALLBACK (eti_cursor_change), eti);

	eti->cursor_activated_id = g_signal_connect (
		selection, "cursor_activated",
		G_CALLBACK (eti_cursor_activated), eti);

	eti_selection_change (selection, eti);
	g_signal_emit_by_name (eti, "selection_model_added", eti->selection);
}

static void
eti_header_dim_changed (ETableHeader *eth,
                        gint col,
                        ETableItem *eti)
{
	eti->needs_compute_width = 1;
	e_canvas_item_request_reflow (GNOME_CANVAS_ITEM (eti));
	eti->needs_redraw = 1;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (eti));
}

static void
eti_header_structure_changed (ETableHeader *eth,
                              ETableItem *eti)
{
	eti->cols = e_table_header_count (eti->header);

	/*
	 * There should be at least one column
	 *  BUT: then you can't remove all columns from a header and add new ones.
	 */

	if (eti->cell_views) {
		eti_unrealize_cell_views (eti);
		eti_detach_cell_views (eti);
		eti_attach_cell_views (eti);
		eti_realize_cell_views (eti);
	} else {
		if (eti->table_model) {
			eti_attach_cell_views (eti);
			eti_realize_cell_views (eti);
		}
	}
	eti->needs_compute_width = 1;
	e_canvas_item_request_reflow (GNOME_CANVAS_ITEM (eti));
	eti->needs_redraw = 1;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (eti));
}

static gint
eti_request_column_width (ETableHeader *eth,
                          gint col,
                          ETableItem *eti)
{
	gint width = 0;

	if (eti->cell_views && eti->cell_views_realized) {
		width = e_cell_max_width (eti->cell_views[col], view_to_model_col (eti, col), col);
	}

	return width;
}

static void
eti_add_header_model (ETableItem *eti,
                      ETableHeader *header)
{
	g_return_if_fail (eti->header == NULL);

	eti->header = header;
	g_object_ref (header);

	eti_header_structure_changed (header, eti);

	eti->header_dim_change_id = g_signal_connect (
		header, "dimension_change",
		G_CALLBACK (eti_header_dim_changed), eti);

	eti->header_structure_change_id = g_signal_connect (
		header, "structure_change",
		G_CALLBACK (eti_header_structure_changed), eti);

	eti->header_request_width_id = g_signal_connect (
		header, "request_width",
		G_CALLBACK (eti_request_column_width), eti);
}

/*
 * GObject::dispose method
 */
static void
eti_dispose (GObject *object)
{
	ETableItem *eti = E_TABLE_ITEM (object);
	ETableItemPrivate *priv = e_table_item_get_instance_private (eti);

	if (priv->show_cursor_delay_source) {
		g_source_destroy (priv->show_cursor_delay_source);
		g_source_unref (priv->show_cursor_delay_source);
		priv->show_cursor_delay_source = NULL;
	}

	eti_remove_header_model (eti);
	eti_remove_table_model (eti);
	eti_remove_selection_model (eti);

	if (eti->height_cache_idle_id) {
		g_source_remove (eti->height_cache_idle_id);
		eti->height_cache_idle_id = 0;
	}
	eti->height_cache_idle_count = 0;

	if (eti->cursor_idle_id) {
		g_source_remove (eti->cursor_idle_id);
		eti->cursor_idle_id = 0;
	}

	g_clear_pointer (&eti->height_cache, g_free);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_table_item_parent_class)->dispose (object);
}

static void
eti_set_property (GObject *object,
                  guint property_id,
                  const GValue *value,
                  GParamSpec *pspec)
{
	GnomeCanvasItem *item = GNOME_CANVAS_ITEM (object);
	ETableItem *eti = E_TABLE_ITEM (object);
	gint cursor_col;

	switch (property_id) {
	case PROP_TABLE_HEADER:
		eti_remove_header_model (eti);
		eti_add_header_model (eti, E_TABLE_HEADER (g_value_get_object (value)));
		break;

	case PROP_TABLE_MODEL:
		eti_remove_table_model (eti);
		eti_add_table_model (eti, E_TABLE_MODEL (g_value_get_object (value)));
		break;

	case PROP_SELECTION_MODEL:
		g_signal_emit_by_name (
			eti, "selection_model_removed", eti->selection);
		eti_remove_selection_model (eti);
		if (g_value_get_object (value))
			eti_add_selection_model (eti, E_SELECTION_MODEL (g_value_get_object (value)));
		break;

	case PROP_LENGTH_THRESHOLD:
		eti->length_threshold = g_value_get_int (value);
		break;

	case PROP_TABLE_ALTERNATING_ROW_COLORS:
		eti->alternating_row_colors = g_value_get_boolean (value);
		break;

	case PROP_TABLE_HORIZONTAL_DRAW_GRID:
		eti->horizontal_draw_grid = g_value_get_boolean (value);
		break;

	case PROP_TABLE_VERTICAL_DRAW_GRID:
		eti->vertical_draw_grid = g_value_get_boolean (value);
		break;

	case PROP_TABLE_DRAW_FOCUS:
		eti->draw_focus = g_value_get_boolean (value);
		break;

	case PROP_CURSOR_MODE:
		eti->cursor_mode = g_value_get_int (value);
		break;

	case PROP_MINIMUM_WIDTH:
	case PROP_WIDTH:
		if ((eti->minimum_width == eti->width && g_value_get_double (value) > eti->width) ||
		    g_value_get_double (value) < eti->width) {
			eti->needs_compute_width = 1;
			e_canvas_item_request_reflow (item);
		}
		eti->minimum_width = g_value_get_double (value);
		break;
	case PROP_CURSOR_ROW:
		g_object_get (
			eti->selection,
			"cursor_col", &cursor_col,
			NULL);

		e_table_item_focus (eti, cursor_col != -1 ? cursor_col : 0, view_to_model_row (eti, g_value_get_int (value)), 0);
		break;
	case PROP_UNIFORM_ROW_HEIGHT:
		if (eti->uniform_row_height != g_value_get_boolean (value)) {
			eti->uniform_row_height = g_value_get_boolean (value);
			if (item->flags & GNOME_CANVAS_ITEM_REALIZED) {
				free_height_cache (eti);
				eti->needs_compute_height = 1;
				e_canvas_item_request_reflow (item);
				eti->needs_redraw = 1;
				gnome_canvas_item_request_update (item);
			}
		}
		break;
	}
	eti->needs_redraw = 1;
	gnome_canvas_item_request_update (item);
}

static void
eti_get_property (GObject *object,
                  guint property_id,
                  GValue *value,
                  GParamSpec *pspec)
{
	ETableItem *eti;
	gint row;

	eti = E_TABLE_ITEM (object);

	switch (property_id) {
	case PROP_WIDTH:
		g_value_set_double (value, eti->width);
		break;
	case PROP_HEIGHT:
		g_value_set_double (value, eti->height);
		break;
	case PROP_MINIMUM_WIDTH:
		g_value_set_double (value, eti->minimum_width);
		break;
	case PROP_CURSOR_ROW:
		g_object_get (
			eti->selection,
			"cursor_row", &row,
			NULL);
		g_value_set_int (value, model_to_view_row (eti, row));
		break;
	case PROP_UNIFORM_ROW_HEIGHT:
		g_value_set_boolean (value, eti->uniform_row_height);
		break;
	case PROP_IS_EDITING:
		g_value_set_boolean (value, e_table_item_is_editing (eti));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_table_item_init (ETableItem *eti)
{
	/* eti->priv = e_table_item_get_instance_private (eti); */

	eti->motion_row = -1;
	eti->motion_col = -1;
	eti->editing_col = -1;
	eti->editing_row = -1;
	eti->height = 0;
	eti->width = 0;
	eti->minimum_width = 0;

	eti->save_col = -1;
	eti->save_row = -1;
	eti->save_state = NULL;

	eti->click_count = 0;

	eti->height_cache = NULL;
	eti->height_cache_idle_id = 0;
	eti->height_cache_idle_count = 0;

	eti->length_threshold = -1;
	eti->uniform_row_height = FALSE;

	eti->uses_source_model = 0;
	eti->source_model = NULL;

	eti->row_guess = -1;
	eti->cursor_mode = E_CURSOR_SIMPLE;

	eti->selection_change_id = 0;
	eti->selection_row_change_id = 0;
	eti->cursor_change_id = 0;
	eti->cursor_activated_id = 0;
	eti->selection = NULL;

	eti->old_cursor_row = -1;

	eti->needs_redraw = 0;
	eti->needs_compute_height = 0;

	eti->in_key_press = 0;

	eti->maybe_did_something = TRUE;

	eti->grabbed_count = 0;
	eti->gtk_grabbed = 0;

	eti->in_drag = 0;
	eti->maybe_in_drag = 0;

	eti->grabbed_col = -1;
	eti->grabbed_row = -1;

	eti->cursor_on_screen = FALSE;
	eti->cursor_x1 = -1;
	eti->cursor_y1 = -1;
	eti->cursor_x2 = -1;
	eti->cursor_y2 = -1;

	eti->rows = -1;
	eti->cols = -1;

	eti->frozen_count = 0;
	eti->queue_show_cursor = FALSE;

	e_canvas_item_set_reflow_callback (GNOME_CANVAS_ITEM (eti), eti_reflow);
}

static gboolean
eti_tree_unfreeze (GtkWidget *widget,
                   GdkEvent *event,
                   ETableItem *eti)
{

	if (widget)
		g_object_set_data (G_OBJECT (widget), "freeze-cursor", NULL);

	return FALSE;
}

static void
eti_realize (GnomeCanvasItem *item)
{
	ETableItem *eti = E_TABLE_ITEM (item);

	if (GNOME_CANVAS_ITEM_CLASS (e_table_item_parent_class)->realize)
		(*GNOME_CANVAS_ITEM_CLASS (e_table_item_parent_class)->realize)(item);

	eti->rows = e_table_model_row_count (eti->table_model);

	g_signal_connect (
		item->canvas, "scroll_event",
		G_CALLBACK (eti_tree_unfreeze), eti);

	if (eti->cell_views == NULL)
		eti_attach_cell_views (eti);

	eti_realize_cell_views (eti);

	free_height_cache (eti);

	if (item->canvas->focused_item == NULL && eti->selection) {
		gint row;

		row = e_selection_model_cursor_row (E_SELECTION_MODEL (eti->selection));
		row = model_to_view_row (eti, row);
		if (row != -1) {
			e_canvas_item_grab_focus (item, FALSE);
			eti_show_cursor (eti, 0);
			eti_check_cursor_bounds (eti);
		}
	}

	eti->needs_compute_height = 1;
	eti->needs_compute_width = 1;
	e_canvas_item_request_reflow (GNOME_CANVAS_ITEM (eti));
	eti->needs_redraw = 1;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (eti));
}

static void
eti_unrealize (GnomeCanvasItem *item)
{
	ETableItem *eti = E_TABLE_ITEM (item);

	if (eti->grabbed_count > 0) {
		d (g_print ("%s: eti_ungrab\n", G_STRFUNC));
		eti_ungrab (eti, -1);
	}

	if (eti_editing (eti))
		e_table_item_leave_edit_(eti);

	if (eti->height_cache_idle_id) {
		g_source_remove (eti->height_cache_idle_id);
		eti->height_cache_idle_id = 0;
	}

	g_clear_pointer (&eti->height_cache, g_free);
	eti->height_cache_idle_count = 0;

	eti_unrealize_cell_views (eti);

	eti->height = 0;

	if (GNOME_CANVAS_ITEM_CLASS (e_table_item_parent_class)->unrealize)
		(*GNOME_CANVAS_ITEM_CLASS (e_table_item_parent_class)->unrealize)(item);
}

gboolean
e_table_item_get_row_selected (ETableItem *eti,
			       gint row)
{
	g_return_val_if_fail (E_IS_TABLE_ITEM (eti), FALSE);

	return row >= 0 && row < eti->rows &&
		e_selection_model_is_row_selected (E_SELECTION_MODEL (eti->selection), view_to_model_row (eti, row));
}

static void
eti_draw_grid_line (ETableItem *eti,
                    cairo_t *cr,
                    const GdkRGBA *rgba,
                    gint x1,
                    gint y1,
                    gint x2,
                    gint y2)
{
	cairo_save (cr);

	cairo_set_line_width (cr, 0.5);
	gdk_cairo_set_source_rgba (cr, rgba);

	cairo_move_to (cr, x1 - 0.5, y1 + 0.5);
	cairo_line_to (cr, x2 - 0.5, y2 + 0.5);
	cairo_stroke (cr);

	cairo_restore (cr);
}

static void
eti_draw (GnomeCanvasItem *item,
          cairo_t *cr,
          gint x,
          gint y,
          gint width,
          gint height)
{
	ETableItem *eti = E_TABLE_ITEM (item);
	const gint rows = eti->rows;
	const gint cols = eti->cols;
	gint row, col;
	gint first_col, last_col, x_offset;
	gint first_row, last_row, y_offset, yd;
	gint x1, x2;
	gint f_x1, f_x2, f_y1, f_y2;
	gboolean f_found;
	cairo_matrix_t i2c;
	gdouble eti_base_x, eti_base_y, lower_right_y, lower_right_x;
	GtkWidget *canvas = GTK_WIDGET (item->canvas);
	GdkRGBA line_color;
	gint height_extra = eti->horizontal_draw_grid ? 1 : 0;

	e_utils_get_theme_color (canvas, "theme_bg_color", E_UTILS_DEFAULT_THEME_BG_COLOR, &line_color);
	e_utils_shade_color (&line_color, &line_color, E_UTILS_DARKNESS_MULT);

	/*
	 * Find out our real position after grouping
	 */
	gnome_canvas_item_i2c_matrix (item, &i2c);
	eti_base_x = 0;
	eti_base_y = 0;
	cairo_matrix_transform_point (&i2c, &eti_base_x, &eti_base_y);

	lower_right_x = eti->width;
	lower_right_y = eti->height;
	cairo_matrix_transform_point (&i2c, &lower_right_x, &lower_right_y);

	/*
	 * First column to draw, last column to draw
	 */
	first_col = -1;
	x_offset = 0;
	x1 = floor (eti_base_x);
	for (col = 0; col < cols; col++, x1 = x2) {
		ETableCol *ecol = e_table_header_get_column (eti->header, col);

		x2 = x1 + ecol->width;

		if (x1 > (x + width))
			break;
		if (x2 < x)
			continue;
		if (first_col == -1) {
			x_offset = x1 - x;
			first_col = col;
		}
	}
	last_col = col;

	/*
	 * Nothing to paint
	 */
	if (first_col == -1)
		return;

	/*
	 * Compute row span.
	 */
	if (eti->uniform_row_height) {
		first_row = (y - floor (eti_base_y) - height_extra) / (ETI_ROW_HEIGHT (eti, -1) + height_extra);
		last_row = (y + height - floor (eti_base_y)               ) / (ETI_ROW_HEIGHT (eti, -1) + height_extra) + 1;
		if (first_row > last_row)
			return;
		y_offset = floor (eti_base_y) - y + height_extra + first_row * (ETI_ROW_HEIGHT (eti, -1) + height_extra);
		if (first_row < 0)
			first_row = 0;
		if (last_row > eti->rows)
			last_row = eti->rows;
	} else {
		gint y1, y2;

		y_offset = 0;
		first_row = -1;

		y1 = y2 = floor (eti_base_y) + height_extra;
		for (row = 0; row < rows; row++, y1 = y2) {

			y2 += ETI_ROW_HEIGHT (eti, row) + height_extra;

			if (y1 > y + height)
				break;

			if (y2 < y)
				continue;

			if (first_row == -1) {
				y_offset = y1 - y;
				first_row = row;
			}
		}
		last_row = row;

		if (first_row == -1)
			return;
	}

	if (first_row == -1)
		return;

	/*
	 * Draw cells
	 */
	yd = y_offset;
	f_x1 = f_x2 = f_y1 = f_y2 = -1;
	f_found = FALSE;

	if (eti->horizontal_draw_grid && first_row == 0)
		eti_draw_grid_line (eti, cr, &line_color, eti_base_x - x, yd, eti_base_x + eti->width - x, yd);

	yd += height_extra;

	for (row = first_row; row < last_row; row++) {
		gint xd;
		gboolean selected;
		gint cursor_col, cursor_row;

		height = ETI_ROW_HEIGHT (eti, row);

		xd = x_offset;

		selected = e_table_item_get_row_selected (eti, row);

		g_object_get (
			eti->selection,
			"cursor_col", &cursor_col,
			"cursor_row", &cursor_row,
			NULL);

		for (col = first_col; col < last_col; col++) {
			ETableCol *ecol = e_table_header_get_column (eti->header, col);
			ECellView *ecell_view = eti->cell_views[col];
			gboolean col_selected = selected;
			gboolean cursor = FALSE;
			ECellFlags flags;
			GdkRGBA background;
			gint y1, y2;
			cairo_pattern_t *pat;

			switch (eti->cursor_mode) {
			case E_CURSOR_SIMPLE:
			case E_CURSOR_SPREADSHEET:
				if (cursor_col == ecol->spec->model_col && cursor_row == view_to_model_row (eti, row)) {
					col_selected = !col_selected;
					cursor = TRUE;
				}
				break;
			case E_CURSOR_LINE:
				/* Nothing */
				break;
			}

			x1 = xd;
			y1 = yd + 1;
			x2 = x1 + ecol->width;
			y2 = yd + height;

			eti_get_cell_background_color (eti, row, col, col_selected, &background);

			cairo_save (cr);
			pat = cairo_pattern_create_linear (0, y1, 0, y2);
			cairo_pattern_add_color_stop_rgba (
				pat, 0.0, background.red,
				background.green,
				background.blue, selected ? 0.8: 1.0);
			if (selected)
				cairo_pattern_add_color_stop_rgba (
					pat, 0.5, background.red,
					background.green,
					background.blue, 0.9);

			cairo_pattern_add_color_stop_rgba (
				pat, 1, background.red,
				background.green,
				background.blue, selected ? 0.8 : 1.0);
			cairo_rectangle (cr, x1, y1, ecol->width, height - 1);
			cairo_set_source (cr, pat);
			cairo_fill_preserve (cr);
			cairo_pattern_destroy (pat);
			cairo_set_line_width (cr, 0);
			cairo_stroke (cr);
			cairo_restore (cr);

			cairo_save (cr);
			cairo_set_line_width (cr, 0.5);
			cairo_set_source_rgba (
				cr, background.red,
				background.green,
				background.blue, 1);
			cairo_move_to (cr, x1, y1 + 0.5);
			cairo_line_to (cr, x2, y1 + 0.5);
			cairo_stroke (cr);

			cairo_set_line_width (cr, 0.5);
			cairo_set_source_rgba (
				cr, background.red,
				background.green,
				background.blue, 1);
			cairo_move_to (cr, x1, y2 + 0.5);
			cairo_line_to (cr, x2, y2 + 0.5);
			cairo_stroke (cr);
			cairo_restore (cr);

			flags = col_selected ? E_CELL_SELECTED : 0;
			flags |= gtk_widget_has_focus (canvas) ? E_CELL_FOCUSED : 0;
			flags |= cursor ? E_CELL_CURSOR : 0;

			switch (ecol->justification) {
			case GTK_JUSTIFY_LEFT:
				flags |= E_CELL_JUSTIFY_LEFT;
				break;
			case GTK_JUSTIFY_RIGHT:
				flags |= E_CELL_JUSTIFY_RIGHT;
				break;
			case GTK_JUSTIFY_CENTER:
				flags |= E_CELL_JUSTIFY_CENTER;
				break;
			case GTK_JUSTIFY_FILL:
				flags |= E_CELL_JUSTIFY_FILL;
				break;
			}

			e_cell_draw (
				ecell_view, cr,
				ecol->spec->model_col,
				col, row, flags,
				xd, yd,
				xd + ecol->width, yd + height);

			if (!f_found && !selected) {
				switch (eti->cursor_mode) {
				case E_CURSOR_LINE:
					if (view_to_model_row (eti, row) == cursor_row) {
						f_x1 = floor (eti_base_x) - x;
						f_x2 = floor (lower_right_x) - x;
						f_y1 = yd + 1;
						f_y2 = yd + height;
						f_found = TRUE;
					}
					break;
				case E_CURSOR_SIMPLE:
				case E_CURSOR_SPREADSHEET:
					if (view_to_model_col (eti, col) == cursor_col && view_to_model_row (eti, row) == cursor_row) {
						f_x1 = xd;
						f_x2 = xd + ecol->width;
						f_y1 = yd;
						f_y2 = yd + height;
						f_found = TRUE;
					}
					break;
				}
			}

			xd += ecol->width;
		}
		yd += height;

		if (eti->horizontal_draw_grid) {
			eti_draw_grid_line (eti, cr, &line_color, eti_base_x - x, yd, eti_base_x + eti->width - x, yd);
			yd++;
		}
	}

	if (eti->vertical_draw_grid) {
		gint xd = x_offset;

		for (col = first_col; col <= last_col; col++) {
			ETableCol *ecol = e_table_header_get_column (eti->header, col);

			eti_draw_grid_line (eti, cr, &line_color, xd, y_offset, xd, yd - 1);

			/*
			 * This looks wierd, but it is to draw the last line
			 */
			if (ecol)
				xd += ecol->width;
		}
	}

	/*
	 * Draw focus
	 */
	if (eti->draw_focus && f_found) {
		static const double dash[] = { 1.0, 1.0 };
		GdkRGBA bg, fg;

		e_utils_get_theme_color (canvas, "theme_bg_color", E_UTILS_DEFAULT_THEME_BG_COLOR, &bg);
		e_utils_get_theme_color (canvas, "theme_fg_color", E_UTILS_DEFAULT_THEME_FG_COLOR, &fg);

		cairo_set_line_width (cr, 1.0);
		cairo_rectangle (
			cr,
			f_x1 + 0.5, f_x2 + 0.5,
			f_x2 - f_x1 - 1, f_y2 - f_y1);

		gdk_cairo_set_source_rgba (cr, &bg);
		cairo_stroke_preserve (cr);

		cairo_set_dash (cr, dash, G_N_ELEMENTS (dash), 0.0);
		gdk_cairo_set_source_rgba (cr, &fg);
		cairo_stroke (cr);
	}
}

static GnomeCanvasItem *
eti_point (GnomeCanvasItem *item,
           gdouble x,
           gdouble y,
           gint cx,
           gint cy)
{
	return item;
}

static gboolean
find_cell (ETableItem *eti,
           gdouble x,
           gdouble y,
           gint *view_col_res,
           gint *view_row_res,
           gdouble *x1_res,
           gdouble *y1_res)
{
	const gint cols = eti->cols;
	const gint rows = eti->rows;
	gdouble x1, y1, x2, y2;
	gint col, row;

	gint height_extra = eti->horizontal_draw_grid ? 1 : 0;

	/* FIXME: this routine is inefficient, fix later */

	if (eti->grabbed_col >= 0 && eti->grabbed_row >= 0) {
		*view_col_res = eti->grabbed_col;
		*view_row_res = eti->grabbed_row;
		*x1_res = x - e_table_header_col_diff (eti->header, 0, eti->grabbed_col);
		*y1_res = y - e_table_item_row_diff (eti, 0, eti->grabbed_row);
		return TRUE;
	}

	if (cols == 0 || rows == 0)
		return FALSE;

	x1 = 0;
	for (col = 0; col < cols - 1; col++, x1 = x2) {
		ETableCol *ecol = e_table_header_get_column (eti->header, col);

		if (x < x1)
			return FALSE;

		x2 = x1 + ecol->width;

		if (x <= x2)
			break;
	}

	if (eti->uniform_row_height) {
		if (y < height_extra)
			return FALSE;
		row = (y - height_extra) / (ETI_ROW_HEIGHT (eti, -1) + height_extra);
		y1 = row * (ETI_ROW_HEIGHT (eti, -1) + height_extra) + height_extra;
		if (row >= eti->rows)
			return FALSE;
	} else {
		y1 = y2 = height_extra;
		if (y < height_extra)
			return FALSE;
		for (row = 0; row < rows; row++, y1 = y2) {
			y2 += ETI_ROW_HEIGHT (eti, row) + height_extra;

			if (y <= y2)
				break;
		}

		if (row == rows)
			return FALSE;
	}
	*view_col_res = col;
	if (x1_res)
		*x1_res = x - x1;
	*view_row_res = row;
	if (y1_res)
		*y1_res = y - y1;
	return TRUE;
}

static void
eti_cursor_move (ETableItem *eti,
                 gint row,
                 gint column)
{
	e_table_item_leave_edit_(eti);
	e_table_item_focus (eti, view_to_model_col (eti, column), view_to_model_row (eti, row), 0);
}

static void
eti_cursor_move_left (ETableItem *eti)
{
	gint cursor_col, cursor_row;
	g_object_get (
		eti->selection,
		"cursor_col", &cursor_col,
		"cursor_row", &cursor_row,
		NULL);

	eti_cursor_move (eti, model_to_view_row (eti, cursor_row), model_to_view_col (eti, cursor_col) - 1);
}

static void
eti_cursor_move_right (ETableItem *eti)
{
	gint cursor_col, cursor_row;
	g_object_get (
		eti->selection,
		"cursor_col", &cursor_col,
		"cursor_row", &cursor_row,
		NULL);

	eti_cursor_move (eti, model_to_view_row (eti, cursor_row), model_to_view_col (eti, cursor_col) + 1);
}

static gint
eti_e_cell_event (ETableItem *item,
                  ECellView *ecell_view,
                  GdkEvent *event,
                  gint model_col,
                  gint view_col,
                  gint row,
                  ECellFlags flags)
{
	ECellActions actions = 0;
	gint ret_val;

	ret_val = e_cell_event (
		ecell_view, event, model_col, view_col, row, flags, &actions);

	if (actions & E_CELL_GRAB) {
		GdkDevice *event_device;
		guint32 event_time;

		d (g_print ("%s: eti_grab\n", G_STRFUNC));

		event_device = gdk_event_get_device (event);
		event_time = gdk_event_get_time (event);
		eti_grab (item, event_device, event_time);

		item->grabbed_col = view_col;
		item->grabbed_row = row;
	}

	if (actions & E_CELL_UNGRAB) {
		guint32 event_time;

		d (g_print ("%s: eti_ungrab\n", G_STRFUNC));

		event_time = gdk_event_get_time (event);
		eti_ungrab (item, event_time);

		item->grabbed_col = -1;
		item->grabbed_row = -1;
	}

	return ret_val;
}

/* FIXME: cursor */
static gint
eti_event (GnomeCanvasItem *item,
           GdkEvent *event)
{
	ETableItem *eti = E_TABLE_ITEM (item);
	ECellView *ecell_view;
	GdkModifierType event_state = 0;
	GdkEvent *event_copy;
	guint event_button = 0;
	guint event_keyval = 0;
	gdouble event_x_item = 0;
	gdouble event_y_item = 0;
	gdouble event_x_win = 0;
	gdouble event_y_win = 0;
	guint32 event_time;
	gboolean return_val = TRUE;
#if d(!)0
	gboolean leave = FALSE;
#endif

	if (!eti->header)
		return FALSE;

	/* Don't fetch the device here.  GnomeCanvas frequently emits
	 * synthesized events, and calling gdk_event_get_device() on them
	 * will trigger a runtime warning.  Fetch the device where needed. */
	gdk_event_get_button (event, &event_button);
	gdk_event_get_coords (event, &event_x_win, &event_y_win);
	gdk_event_get_keyval (event, &event_keyval);
	gdk_event_get_state (event, &event_state);
	event_time = gdk_event_get_time (event);

	switch (event->type) {
	case GDK_BUTTON_PRESS: {
		gdouble x1, y1;
		gint col, row;
		gint cursor_row, cursor_col;
		gint new_cursor_row, new_cursor_col;
		ECellFlags flags = 0;

		d (g_print ("%s: GDK_BUTTON_PRESS received, button %d\n", G_STRFUNC, event_button));

		switch (event_button) {
		case 1: /* Fall through. */
		case 2:
			e_canvas_item_grab_focus (GNOME_CANVAS_ITEM (eti), TRUE);

			event_x_item = event_x_win;
			event_y_item = event_y_win;

			gnome_canvas_item_w2i (
				item, &event_x_item, &event_y_item);

			if (!find_cell (eti, event_x_item, event_y_item, &col, &row, &x1, &y1)) {
				if (eti_editing (eti))
					e_table_item_leave_edit_(eti);
				return TRUE;
			}

			ecell_view = eti->cell_views[col];

			/* Clone the event and alter its position. */
			event_copy = gdk_event_copy (event);
			event_copy->button.x = x1;
			event_copy->button.y = y1;

			g_object_get (
				eti->selection,
				"cursor_row", &cursor_row,
				"cursor_col", &cursor_col,
				NULL);

			if (cursor_col == view_to_model_col (eti, col) && cursor_row == view_to_model_row (eti, row)) {
				flags = E_CELL_CURSOR;
			} else {
				flags = 0;
			}

			return_val = eti_e_cell_event (
				eti, ecell_view, event_copy,
				view_to_model_col (eti, col),
				col, row, flags);
			if (return_val) {
				gdk_event_free (event_copy);
				return TRUE;
			}

			g_signal_emit (
				eti, eti_signals[CLICK], 0,
				row, view_to_model_col (eti, col),
				event_copy, &return_val);

			gdk_event_free (event_copy);

			if (return_val) {
				eti->click_count = 0;
				return TRUE;
			}

			g_object_get (
				eti->selection,
				"cursor_row", &cursor_row,
				"cursor_col", &cursor_col,
				NULL);

			eti->maybe_did_something =
				e_selection_model_maybe_do_something (
				E_SELECTION_MODEL (eti->selection),
				view_to_model_row (eti, row),
				view_to_model_col (eti, col),
				event_state);
			g_object_get (
				eti->selection,
				"cursor_row", &new_cursor_row,
				"cursor_col", &new_cursor_col,
				NULL);

			if (cursor_row != new_cursor_row || cursor_col != new_cursor_col) {
				eti->click_count = 1;
			} else {
				eti->click_count++;
				eti->row_guess = row;

				if ((!eti_editing (eti)) && e_table_model_is_cell_editable (eti->table_model, cursor_col, row)) {
					e_table_item_enter_edit (eti, col, row);
				}

				/*
				 * Adjust the event positions
				 */

				if (eti_editing (eti)) {
					return_val = eti_e_cell_event (
						eti, ecell_view, event,
						view_to_model_col (eti, col),
						col, row,
						E_CELL_EDITING |
						E_CELL_CURSOR);
					if (return_val)
						return TRUE;
				}
			}

			if (event_button == 1) {
				return_val = TRUE;

				eti->maybe_in_drag = TRUE;
				eti->drag_row = new_cursor_row;
				eti->drag_col = new_cursor_col;
				eti->drag_x = event_x_item;
				eti->drag_y = event_y_item;
				eti->drag_state = event_state;
			}

			break;
		case 3:
			e_canvas_item_grab_focus (GNOME_CANVAS_ITEM (eti), TRUE);

			event_x_item = event_x_win;
			event_y_item = event_y_win;

			gnome_canvas_item_w2i (
				item, &event_x_item, &event_y_item);

			if (!find_cell (eti, event_x_item, event_y_item, &col, &row, &x1, &y1))
				return TRUE;

			e_selection_model_right_click_down (
				E_SELECTION_MODEL (eti->selection),
				view_to_model_row (eti, row),
				view_to_model_col (eti, col), 0);

			/* Clone the event and alter its position. */
			event_copy = gdk_event_copy (event);
			event_copy->button.x = event_x_item;
			event_copy->button.y = event_y_item;

			g_signal_emit (
				eti, eti_signals[RIGHT_CLICK], 0,
				row, view_to_model_col (eti, col),
				event, &return_val);

			gdk_event_free (event_copy);

			if (!return_val)
				e_selection_model_right_click_up (E_SELECTION_MODEL (eti->selection));
			break;
		case 4:
		case 5:
			return FALSE;

		}
		break;
	}

	case GDK_BUTTON_RELEASE: {
		gdouble x1, y1;
		gint col, row;
		gint cursor_row, cursor_col;

		d (g_print ("%s: GDK_BUTTON_RELEASE received, button %d\n", G_STRFUNC, event_button));

		if (eti->grabbed_count > 0) {
			d (g_print ("%s: eti_ungrab\n", G_STRFUNC));
			eti_ungrab (eti, event_time);
		}

		if (event_button == 1) {
			if (eti->maybe_in_drag) {
				eti->maybe_in_drag = FALSE;
				if (!eti->maybe_did_something)
					e_selection_model_do_something (E_SELECTION_MODEL (eti->selection), eti->drag_row, eti->drag_col, eti->drag_state);
			}
			if (eti->in_drag) {
				eti->in_drag = FALSE;
			}
		}

		switch (event_button) {
		case 1: /* Fall through. */
		case 2:

			event_x_item = event_x_win;
			event_y_item = event_y_win;

			gnome_canvas_item_w2i (
				item, &event_x_item, &event_y_item);
#if d(!)0
			{
				gboolean cell_found = find_cell (
					eti, event_x_item, event_y_item,
					&col, &row, &x1, &y1);
				g_print (
					"%s: find_cell(%f, %f) = %s(%d, %d, %f, %f)\n",
					G_STRFUNC, event_x_item, event_y_item,
					cell_found?"true":"false", col, row, x1, y1);
			}
#endif

			if (!find_cell (eti, event_x_item, event_y_item, &col, &row, &x1, &y1))
				return TRUE;

			g_object_get (
				eti->selection,
				"cursor_row", &cursor_row,
				"cursor_col", &cursor_col,
				NULL);

			if (eti_editing (eti) && cursor_row == view_to_model_row (eti, row) && cursor_col == view_to_model_col (eti, col)) {

				d (g_print ("%s: GDK_BUTTON_RELEASE received, button %d, line: %d\n", G_STRFUNC, event_button, __LINE__))
;

				ecell_view = eti->cell_views[col];

				/* Clone the event and alter its position. */
				event_copy = gdk_event_copy (event);
				event_copy->button.x = x1;
				event_copy->button.y = y1;

				return_val = eti_e_cell_event (
					eti, ecell_view, event_copy,
					view_to_model_col (eti, col),
					col, row,
					E_CELL_EDITING |
					E_CELL_CURSOR);

				gdk_event_free (event_copy);
			}
			break;
		case 3:
			e_selection_model_right_click_up (E_SELECTION_MODEL (eti->selection));
			return_val = TRUE;
			break;
		case 4:
		case 5:
			return FALSE;

		}
		break;
	}

	case GDK_2BUTTON_PRESS: {
		gint model_col, model_row;
#if 0
		gdouble x1, y1;
#endif

		d (g_print ("%s: GDK_2BUTTON_PRESS received, button %d\n", G_STRFUNC, event_button));

		/*
		 * click_count is so that if you click on two
		 * different rows we don't send a double click signal.
		 */

		if (eti->click_count >= 2) {

			event_x_item = event_x_win;
			event_y_item = event_y_win;

			gnome_canvas_item_w2i (
				item, &event_x_item, &event_y_item);

			g_object_get (
				eti->selection,
				"cursor_row", &model_row,
				"cursor_col", &model_col,
				NULL);

			/* Clone the event and alter its position. */
			event_copy = gdk_event_copy (event);
			event_copy->button.x = event_x_item -
				e_table_header_col_diff (
					eti->header, 0,
					model_to_view_col (eti, model_col));
			event_copy->button.y = event_y_item -
				e_table_item_row_diff (
					eti, 0,
					model_to_view_row (eti, model_row));

			if (event_button == 1) {
				if (eti->maybe_in_drag) {
					eti->maybe_in_drag = FALSE;
					if (!eti->maybe_did_something)
						e_selection_model_do_something (E_SELECTION_MODEL (eti->selection), eti->drag_row, eti->drag_col, eti->drag_state);
				}
				if (eti->in_drag) {
					eti->in_drag = FALSE;
				}
				if (eti_editing (eti))
					e_table_item_leave_edit_ (eti);

			}

			if (eti->grabbed_count > 0) {
				d (g_print ("%s: eti_ungrab\n", G_STRFUNC));
				eti_ungrab (eti, event_time);
			}

			if (model_row != -1 && model_col != -1) {
				g_signal_emit (
					eti, eti_signals[DOUBLE_CLICK], 0,
					model_row, model_col, event_copy);
			}

			gdk_event_free (event_copy);
		}
		break;
	}
	case GDK_MOTION_NOTIFY: {
		gint col, row, flags;
		gdouble x1, y1;
		gint cursor_col, cursor_row;

		event_x_item = event_x_win;
		event_y_item = event_y_win;

		gnome_canvas_item_w2i (item, &event_x_item, &event_y_item);

		if (eti->maybe_in_drag) {
			if (gtk_drag_check_threshold (GTK_WIDGET (item->canvas), eti->drag_x, eti->drag_y, event_x_item, event_y_item)) {
				gboolean drag_handled;

				eti->maybe_in_drag = 0;

				/* Clone the event and
				 * alter its position. */
				event_copy = gdk_event_copy (event);
				event_copy->motion.x = event_x_item;
				event_copy->motion.y = event_y_item;

				g_signal_emit (
					eti, eti_signals[START_DRAG], 0,
					eti->drag_row, eti->drag_col,
					event_copy, &drag_handled);

				gdk_event_free (event_copy);

				if (drag_handled)
					eti->in_drag = 1;
				else
					eti->in_drag = 0;
			}
		}

		if (!find_cell (eti, event_x_item, event_y_item, &col, &row, &x1, &y1))
			return TRUE;

		if (eti->motion_row != -1 && eti->motion_col != -1 &&
		    (row != eti->motion_row || col != eti->motion_col)) {
			GdkEvent *cross = gdk_event_new (GDK_LEAVE_NOTIFY);
			cross->crossing.time = event_time;
			return_val = eti_e_cell_event (
				eti, eti->cell_views[eti->motion_col],
				cross,
				view_to_model_col (eti, eti->motion_col),
				eti->motion_col, eti->motion_row, 0);
		}

		eti->motion_row = row;
		eti->motion_col = col;

		g_object_get (
			eti->selection,
			"cursor_row", &cursor_row,
			"cursor_col", &cursor_col,
			NULL);

		flags = 0;
		if (cursor_row == view_to_model_row (eti, row) && cursor_col == view_to_model_col (eti, col)) {
			flags = E_CELL_EDITING | E_CELL_CURSOR;
		}

		ecell_view = eti->cell_views[col];

		/* Clone the event and alter its position. */
		event_copy = gdk_event_copy (event);
		event_copy->motion.x = x1;
		event_copy->motion.y = y1;

		return_val = eti_e_cell_event (
			eti, ecell_view, event_copy,
			view_to_model_col (eti, col), col, row, flags);

		gdk_event_free (event_copy);

		break;
	}

	case GDK_KEY_PRESS: {
		gint cursor_row, cursor_col;
		gint handled = TRUE;

		d (g_print ("%s: GDK_KEY_PRESS received, keyval: %d\n", G_STRFUNC, (gint) e->key.keyval));

		g_object_get (
			eti->selection,
			"cursor_row", &cursor_row,
			"cursor_col", &cursor_col,
			NULL);

		if (cursor_row == -1 && cursor_col == -1)
			return FALSE;

		eti->in_key_press = TRUE;

		switch (event_keyval) {
		case GDK_KEY_Left:
		case GDK_KEY_KP_Left:
			if (eti_editing (eti)) {
				handled = FALSE;
				break;
			}

			g_signal_emit (
				eti, eti_signals[KEY_PRESS], 0,
				model_to_view_row (eti, cursor_row),
				cursor_col, event, &return_val);
			if ((!return_val) &&
			   (atk_get_root () || eti->cursor_mode != E_CURSOR_LINE) &&
			   cursor_col != view_to_model_col (eti, 0))
				eti_cursor_move_left (eti);
			return_val = 1;
			break;

		case GDK_KEY_Right:
		case GDK_KEY_KP_Right:
			if (eti_editing (eti)) {
				handled = FALSE;
				break;
			}

			g_signal_emit (
				eti, eti_signals[KEY_PRESS], 0,
				model_to_view_row (eti, cursor_row),
				cursor_col, event, &return_val);
			if ((!return_val) &&
			   (atk_get_root () || eti->cursor_mode != E_CURSOR_LINE) &&
			   cursor_col != view_to_model_col (eti, eti->cols - 1))
				eti_cursor_move_right (eti);
			return_val = 1;
			break;

		case GDK_KEY_Up:
		case GDK_KEY_KP_Up:
		case GDK_KEY_Down:
		case GDK_KEY_KP_Down:
			if ((event_state & GDK_MOD1_MASK)
			    && ((event_keyval == GDK_KEY_Down) || (event_keyval == GDK_KEY_KP_Down))) {
				gint view_col = model_to_view_col (eti, cursor_col);

				if ((view_col >= 0) && (view_col < eti->cols))
					if (eti_e_cell_event (eti, eti->cell_views[view_col], event, cursor_col, view_col, model_to_view_row (eti, cursor_row),  E_CELL_CURSOR))
						return TRUE;
			} else
			return_val = e_selection_model_key_press (E_SELECTION_MODEL (eti->selection), (GdkEventKey *) event);
			break;
		case GDK_KEY_Home:
		case GDK_KEY_KP_Home:
			if (eti_editing (eti)) {
				handled = FALSE;
				break;
			}

			if (eti->cursor_mode != E_CURSOR_LINE) {
				eti_cursor_move (eti, model_to_view_row (eti, cursor_row), 0);
				return_val = TRUE;
			} else
				return_val = e_selection_model_key_press (E_SELECTION_MODEL (eti->selection), (GdkEventKey *) event);
			break;
		case GDK_KEY_End:
		case GDK_KEY_KP_End:
			if (eti_editing (eti)) {
				handled = FALSE;
				break;
			}

			if (eti->cursor_mode != E_CURSOR_LINE) {
				eti_cursor_move (eti, model_to_view_row (eti, cursor_row), eti->cols - 1);
				return_val = TRUE;
			} else
				return_val = e_selection_model_key_press (E_SELECTION_MODEL (eti->selection), (GdkEventKey *) event);
			break;
		case GDK_KEY_Tab:
		case GDK_KEY_KP_Tab:
		case GDK_KEY_ISO_Left_Tab:
			if ((event_state & GDK_CONTROL_MASK) != 0) {
				return_val = FALSE;
				break;
			}
			if (eti->cursor_mode == E_CURSOR_SPREADSHEET) {
				if ((event_state & GDK_SHIFT_MASK) != 0) {
				/* shift tab */
					if (cursor_col != view_to_model_col (eti, 0))
						eti_cursor_move_left (eti);
					else if (cursor_row != view_to_model_row (eti, 0))
						eti_cursor_move (eti, model_to_view_row (eti, cursor_row) - 1, eti->cols - 1);
					else
						return_val = FALSE;
				} else {
					if (cursor_col != view_to_model_col (eti, eti->cols - 1))
						eti_cursor_move_right (eti);
					else if (cursor_row != view_to_model_row (eti, eti->rows - 1))
						eti_cursor_move (eti, model_to_view_row (eti, cursor_row) + 1, 0);
					else
						return_val = FALSE;
				}
				g_object_get (
					eti->selection,
					"cursor_row", &cursor_row,
					"cursor_col", &cursor_col,
					NULL);

				if (cursor_col >= 0 && cursor_row >= 0 && return_val &&
				    (!eti_editing (eti)) && e_table_model_is_cell_editable (eti->table_model, cursor_col, model_to_view_row (eti, cursor_row))) {
					e_table_item_enter_edit (eti, model_to_view_col (eti, cursor_col), model_to_view_row (eti, cursor_row));
				}
				break;
			} else {
			/* Let tab send you to the next widget. */
			return_val = FALSE;
			break;
			}

		case GDK_KEY_Return:
		case GDK_KEY_KP_Enter:
		case GDK_KEY_ISO_Enter:
		case GDK_KEY_3270_Enter:
			if (eti_editing (eti)) {
				ecell_view = eti->cell_views[eti->editing_col];
				return_val = eti_e_cell_event (
					eti, ecell_view, event,
					view_to_model_col (eti, eti->editing_col),
					eti->editing_col, eti->editing_row, E_CELL_EDITING | E_CELL_CURSOR | E_CELL_PREEDIT);
				if (!return_val)
					break;
			}
			g_signal_emit (
				eti, eti_signals[KEY_PRESS], 0,
				model_to_view_row (eti, cursor_row),
				cursor_col, event, &return_val);
			if (!return_val)
				return_val = e_selection_model_key_press (E_SELECTION_MODEL (eti->selection), (GdkEventKey *) event);
			break;

		default:
			handled = FALSE;
			break;
		}

		if (!handled) {
			switch (event_keyval) {
			case GDK_KEY_Scroll_Lock:
			case GDK_KEY_Sys_Req:
			case GDK_KEY_Shift_L:
			case GDK_KEY_Shift_R:
			case GDK_KEY_Control_L:
			case GDK_KEY_Control_R:
			case GDK_KEY_Caps_Lock:
			case GDK_KEY_Shift_Lock:
			case GDK_KEY_Meta_L:
			case GDK_KEY_Meta_R:
			case GDK_KEY_Alt_L:
			case GDK_KEY_Alt_R:
			case GDK_KEY_Super_L:
			case GDK_KEY_Super_R:
			case GDK_KEY_Hyper_L:
			case GDK_KEY_Hyper_R:
			case GDK_KEY_ISO_Lock:
				break;

			default:
				if (!eti_editing (eti)) {
					gint col, row;
					row = model_to_view_row (eti, cursor_row);
					col = model_to_view_col (eti, cursor_col);
					if (col != -1 && row != -1 && e_table_model_is_cell_editable (eti->table_model, cursor_col, row)) {
						e_table_item_enter_edit (eti, col, row);
					}
				}
				if (!eti_editing (eti)) {
					g_signal_emit (
						eti, eti_signals[KEY_PRESS], 0,
						model_to_view_row (eti, cursor_row),
						cursor_col, event, &return_val);
					if (!return_val)
						e_selection_model_key_press (E_SELECTION_MODEL (eti->selection), (GdkEventKey *) event);
				} else {
					ecell_view = eti->cell_views[eti->editing_col];
					return_val = eti_e_cell_event (
						eti, ecell_view, event,
						view_to_model_col (eti, eti->editing_col),
						eti->editing_col, eti->editing_row, E_CELL_EDITING | E_CELL_CURSOR);
					if (!return_val)
						e_selection_model_key_press (E_SELECTION_MODEL (eti->selection), (GdkEventKey *) event);
				}
				break;
			}
		}
		eti->in_key_press = FALSE;
		break;
	}

	case GDK_KEY_RELEASE: {
		gint cursor_row, cursor_col;

		d (g_print ("%s: GDK_KEY_RELEASE received, keyval: %d\n", G_STRFUNC, (gint) event_keyval));

		g_object_get (
			eti->selection,
			"cursor_row", &cursor_row,
			"cursor_col", &cursor_col,
			NULL);

		if (cursor_col == -1)
			return FALSE;

		if (eti_editing (eti)) {
			ecell_view = eti->cell_views[eti->editing_col];
			return_val = eti_e_cell_event (
				eti, ecell_view, event,
				view_to_model_col (eti, eti->editing_col),
				eti->editing_col, eti->editing_row, E_CELL_EDITING | E_CELL_CURSOR);
		}
		break;
	}

	case GDK_LEAVE_NOTIFY:
		d (leave = TRUE);
	case GDK_ENTER_NOTIFY:
		d (g_print ("%s: %s received\n", G_STRFUNC, leave ? "GDK_LEAVE_NOTIFY" : "GDK_ENTER_NOTIFY"));
		if (eti->motion_row != -1 && eti->motion_col != -1)
			return_val = eti_e_cell_event (
				eti, eti->cell_views[eti->motion_col],
				event,
				view_to_model_col (eti, eti->motion_col),
				eti->motion_col, eti->motion_row, 0);
		eti->motion_row = -1;
		eti->motion_col = -1;

		break;

	case GDK_FOCUS_CHANGE:
		d (g_print ("%s: GDK_FOCUS_CHANGE received, %s\n", G_STRFUNC, e->focus_change.in ? "in": "out"));
		if (event->focus_change.in) {
			if (eti->save_row != -1 &&
			    eti->save_col != -1 &&
			    !eti_editing (eti) &&
			    e_table_model_is_cell_editable (eti->table_model, view_to_model_col (eti, eti->save_col), eti->save_row)) {
				e_table_item_enter_edit (eti, eti->save_col, eti->save_row);
				e_cell_load_state (
					eti->cell_views[eti->editing_col], view_to_model_col (eti, eti->save_col),
					eti->save_col, eti->save_row, eti->edit_ctx, eti->save_state);
				eti_free_save_state (eti);
			}
		} else {
			if (eti_editing (eti)) {
				eti_free_save_state (eti);

				eti->save_row = eti->editing_row;
				eti->save_col = eti->editing_col;
				eti->save_state = e_cell_save_state (
					eti->cell_views[eti->editing_col], view_to_model_col (eti, eti->editing_col),
					eti->editing_col, eti->editing_row, eti->edit_ctx);
				e_table_item_leave_edit_(eti);
			}
		}
		return_val = FALSE;
		break;

	default:
		return_val = FALSE;
		break;
	}
	/* d(g_print("%s: returning: %s\n", G_STRFUNC, return_val?"true":"false"));*/

	return return_val;
}

static void
eti_style_updated (ETableItem *eti)
{
	GnomeCanvasItem *item = GNOME_CANVAS_ITEM (eti);

	if (!(item->flags & GNOME_CANVAS_ITEM_REALIZED))
		return;

	if (eti->cell_views_realized) {
		gint i;
		gint n_cells = eti->n_cells;

		for (i = 0; i < n_cells; i++) {
			e_cell_style_updated (eti->cell_views[i]);
		}
	}

	eti->needs_compute_height = 1;
	e_canvas_item_request_reflow (GNOME_CANVAS_ITEM (eti));
	eti->needs_redraw = 1;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (eti));

	free_height_cache (eti);

	eti_idle_maybe_show_cursor (eti);
}

static void
e_table_item_class_init (ETableItemClass *class)
{
	GnomeCanvasItemClass *item_class = GNOME_CANVAS_ITEM_CLASS (class);
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->dispose = eti_dispose;
	object_class->set_property = eti_set_property;
	object_class->get_property = eti_get_property;

	item_class->update = eti_update;
	item_class->realize = eti_realize;
	item_class->unrealize = eti_unrealize;
	item_class->draw = eti_draw;
	item_class->point = eti_point;
	item_class->event = eti_event;

	class->cursor_change = NULL;
	class->cursor_activated = NULL;
	class->double_click = NULL;
	class->right_click = NULL;
	class->click = NULL;
	class->key_press = NULL;
	class->start_drag = NULL;
	class->style_updated = eti_style_updated;
	class->selection_model_removed = NULL;
	class->selection_model_added = NULL;

	g_object_class_install_property (
		object_class,
		PROP_TABLE_HEADER,
		g_param_spec_object (
			"ETableHeader",
			"Table header",
			"Table header",
			E_TYPE_TABLE_HEADER,
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		object_class,
		PROP_TABLE_MODEL,
		g_param_spec_object (
			"ETableModel",
			"Table model",
			"Table model",
			E_TYPE_TABLE_MODEL,
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
		PROP_MINIMUM_WIDTH,
		g_param_spec_double (
			"minimum_width",
			"Minimum width",
			"Minimum Width",
			0.0, G_MAXDOUBLE, 0.0,
			G_PARAM_READWRITE));

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
		PROP_HEIGHT,
		g_param_spec_double (
			"height",
			"Height",
			"Height",
			0.0, G_MAXDOUBLE, 0.0,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_CURSOR_ROW,
		g_param_spec_int (
			"cursor_row",
			"Cursor row",
			"Cursor row",
			0, G_MAXINT, 0,
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

	g_object_class_install_property (
		object_class,
		PROP_IS_EDITING,
		g_param_spec_boolean (
			"is-editing",
			"Whether is in an editing mode",
			"Whether is in an editing mode",
			FALSE,
			G_PARAM_READABLE));

	eti_signals[CURSOR_CHANGE] = g_signal_new (
		"cursor_change",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableItemClass, cursor_change),
		NULL, NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1,
		G_TYPE_INT);

	eti_signals[CURSOR_ACTIVATED] = g_signal_new (
		"cursor_activated",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableItemClass, cursor_activated),
		NULL, NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1,
		G_TYPE_INT);

	eti_signals[DOUBLE_CLICK] = g_signal_new (
		"double_click",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableItemClass, double_click),
		NULL, NULL,
		e_marshal_VOID__INT_INT_BOXED,
		G_TYPE_NONE, 3,
		G_TYPE_INT,
		G_TYPE_INT,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	eti_signals[START_DRAG] = g_signal_new (
		"start_drag",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableItemClass, start_drag),
		g_signal_accumulator_true_handled, NULL,
		e_marshal_BOOLEAN__INT_INT_BOXED,
		G_TYPE_BOOLEAN, 3,
		G_TYPE_INT,
		G_TYPE_INT,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	eti_signals[RIGHT_CLICK] = g_signal_new (
		"right_click",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableItemClass, right_click),
		g_signal_accumulator_true_handled, NULL,
		e_marshal_BOOLEAN__INT_INT_BOXED,
		G_TYPE_BOOLEAN, 3,
		G_TYPE_INT,
		G_TYPE_INT,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	eti_signals[CLICK] = g_signal_new (
		"click",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableItemClass, click),
		g_signal_accumulator_true_handled, NULL,
		e_marshal_BOOLEAN__INT_INT_BOXED,
		G_TYPE_BOOLEAN, 3,
		G_TYPE_INT,
		G_TYPE_INT,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	eti_signals[KEY_PRESS] = g_signal_new (
		"key_press",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableItemClass, key_press),
		g_signal_accumulator_true_handled, NULL,
		e_marshal_BOOLEAN__INT_INT_BOXED,
		G_TYPE_BOOLEAN, 3,
		G_TYPE_INT,
		G_TYPE_INT,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	eti_signals[STYLE_UPDATED] = g_signal_new (
		"style_updated",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableItemClass, style_updated),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	eti_signals[SELECTION_MODEL_REMOVED] = g_signal_new (
		"selection_model_removed",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (ETableItemClass, selection_model_removed),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);

	eti_signals[SELECTION_MODEL_ADDED] = g_signal_new (
		"selection_model_added",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (ETableItemClass, selection_model_added),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);

	eti_signals[GET_BG_COLOR] = g_signal_new (
		"get-bg-color",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableItemClass, get_bg_color),
		NULL, NULL,
		NULL,
		G_TYPE_BOOLEAN, 3, /* return TRUE when set */
		G_TYPE_INT, /* row */
		G_TYPE_INT, /* col */
		G_TYPE_POINTER /* GdkRGBA *background, but cannot be passed as
				  boxed, because it's used as (inout) argument */);

	/* A11y Init */
	gal_a11y_e_table_item_init ();
}

/**
 * e_table_item_set_cursor:
 * @eti: %ETableItem which will have the cursor set.
 * @col: Column to select.  -1 means the last column.
 * @row: Row to select.  -1 means the last row.
 *
 * This routine sets the cursor of the %ETableItem canvas item.
 */
void
e_table_item_set_cursor (ETableItem *eti,
                         gint col,
                         gint row)
{
	e_table_item_focus (eti, col, view_to_model_row (eti, row), 0);
}

static void
e_table_item_focus (ETableItem *eti,
                    gint col,
                    gint row,
                    GdkModifierType state)
{
	g_return_if_fail (eti != NULL);
	g_return_if_fail (E_IS_TABLE_ITEM (eti));

	if (row == -1) {
		row = view_to_model_row (eti, eti->rows - 1);
	}

	if (col == -1) {
		col = eti->cols - 1;
	}

	if (row != -1) {
		e_selection_model_do_something (
			E_SELECTION_MODEL (eti->selection),
			row, col, state);
	}
}

/**
 * e_table_item_get_focused_column:
 * @eti: %ETableItem which will have the cursor retrieved.
 *
 * This routine gets the cursor of the %ETableItem canvas item.
 *
 * Returns: The current cursor column.
 */
gint
e_table_item_get_focused_column (ETableItem *eti)
{
	gint cursor_col;

	g_return_val_if_fail (eti != NULL, -1);
	g_return_val_if_fail (E_IS_TABLE_ITEM (eti), -1);

	g_object_get (
		eti->selection,
		"cursor_col", &cursor_col,
		NULL);

	return cursor_col;
}

static void
eti_cursor_change (ESelectionModel *selection,
                   gint row,
                   gint col,
                   ETableItem *eti)
{
	GnomeCanvasItem *item = GNOME_CANVAS_ITEM (eti);
	gint view_row;

	if (!(item->flags & GNOME_CANVAS_ITEM_REALIZED))
		return;

	view_row = model_to_view_row (eti, row);

	if (eti->old_cursor_row != -1 && view_row != eti->old_cursor_row)
		e_table_item_redraw_row (eti, eti->old_cursor_row);

	if (view_row == -1) {
		e_table_item_leave_edit_(eti);
		eti->old_cursor_row = -1;
		return;
	}

	if (!e_table_model_has_change_pending (eti->table_model)) {
		if (!eti->in_key_press) {
			eti_maybe_show_cursor (eti, DOUBLE_CLICK_TIME + 10);
		} else {
			eti_maybe_show_cursor (eti, 0);
		}
	}

	e_canvas_item_grab_focus (GNOME_CANVAS_ITEM (eti), FALSE);
	if (eti_editing (eti))
		e_table_item_leave_edit_(eti);

	g_signal_emit (eti, eti_signals[CURSOR_CHANGE], 0, view_row);

	e_table_item_redraw_row (eti, view_row);

	eti->old_cursor_row = view_row;
}

static void
eti_cursor_activated (ESelectionModel *selection,
                      gint row,
                      gint col,
                      ETableItem *eti)
{
	GnomeCanvasItem *item = GNOME_CANVAS_ITEM (eti);
	gint view_row;
	gint view_col;

	if (!(item->flags & GNOME_CANVAS_ITEM_REALIZED))
		return;

	view_row = model_to_view_row (eti, row);
	view_col = model_to_view_col (eti, col);

	if (view_row != -1 && view_col != -1) {
		if (!e_table_model_has_change_pending (eti->table_model)) {
			if (!eti->in_key_press) {
				eti_show_cursor (eti, DOUBLE_CLICK_TIME + 10);
			} else {
				eti_show_cursor (eti, 0);
			}
			eti_check_cursor_bounds (eti);
		}
	}

	if (eti_editing (eti))
		e_table_item_leave_edit_(eti);

	if (view_row != -1)
		g_signal_emit (
			eti, eti_signals[CURSOR_ACTIVATED], 0, view_row);
}

static void
eti_selection_change (ESelectionModel *selection,
                      ETableItem *eti)
{
	GnomeCanvasItem *item = GNOME_CANVAS_ITEM (eti);

	if (!(item->flags & GNOME_CANVAS_ITEM_REALIZED))
		return;

	eti->needs_redraw = TRUE;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (eti));
}

static void
eti_selection_row_change (ESelectionModel *selection,
                          gint row,
                          ETableItem *eti)
{
	GnomeCanvasItem *item = GNOME_CANVAS_ITEM (eti);

	if (!(item->flags & GNOME_CANVAS_ITEM_REALIZED))
		return;

	if (!eti->needs_redraw) {
		e_table_item_redraw_row (eti, model_to_view_row (eti, row));
	}
}

/**
 * e_table_item_enter_edit
 * @eti: %ETableItem which will start being edited
 * @col: The view col to edit.
 * @row: The view row to edit.
 *
 * This routine starts the given %ETableItem editing at the given view
 * column and row.
 */
void
e_table_item_enter_edit (ETableItem *eti,
                         gint col,
                         gint row)
{
	g_return_if_fail (eti != NULL);
	g_return_if_fail (E_IS_TABLE_ITEM (eti));

	d (g_print ("%s: %d, %d, eti_editing() = %s\n", G_STRFUNC, col, row, eti_editing (eti)?"true":"false"));

	if (eti_editing (eti))
		e_table_item_leave_edit_(eti);

	eti->editing_col = col;
	eti->editing_row = row;

	if (col >= 0) {
		eti->edit_ctx = e_cell_enter_edit (eti->cell_views[col], view_to_model_col (eti, col), col, row);

		g_object_notify (G_OBJECT (eti), "is-editing");
	}
}

/**
 * e_table_item_leave_edit_
 * @eti: %ETableItem which will stop being edited
 *
 * This routine stops the given %ETableItem from editing.
 */
void
e_table_item_leave_edit (ETableItem *eti)
{
	gint col, row;
	gpointer edit_ctx;

	g_return_if_fail (eti != NULL);
	g_return_if_fail (E_IS_TABLE_ITEM (eti));

	d (g_print ("%s: eti_editing() = %s\n", G_STRFUNC, eti_editing (eti)?"true":"false"));

	if (!eti_editing (eti))
		return;

	col = eti->editing_col;
	row = eti->editing_row;
	edit_ctx = eti->edit_ctx;

	eti->editing_col = -1;
	eti->editing_row = -1;
	eti->edit_ctx = NULL;

	e_cell_leave_edit (
		eti->cell_views[col],
		view_to_model_col (eti, col),
		col, row, edit_ctx);

	g_object_notify (G_OBJECT (eti), "is-editing");
}

/**
 * e_table_item_compute_location
 * @eti: %ETableItem to look in.
 * @x: A pointer to the x location to find in the %ETableItem.
 * @y: A pointer to the y location to find in the %ETableItem.
 * @row: A pointer to the location to store the found row in.
 * @col: A pointer to the location to store the found col in.
 *
 * This routine locates the pixel location (*x, *y) in the
 * %ETableItem.  If that location is in the %ETableItem, *row and *col
 * are set to the view row and column where it was found.  If that
 * location is not in the %ETableItem, the height of the %ETableItem
 * is removed from the value y points to.
 */
void
e_table_item_compute_location (ETableItem *eti,
                               gint *x,
                               gint *y,
                               gint *row,
                               gint *col)
{
	/* Save the grabbed row but make sure that we don't get flawed
	 * results because the cursor is grabbed. */
	gint grabbed_row = eti->grabbed_row;
	eti->grabbed_row = -1;

	if (!find_cell (eti, *x, *y, col, row, NULL, NULL)) {
		*y -= eti->height;
	}

	eti->grabbed_row = grabbed_row;
}

/**
 * e_table_item_compute_mouse_over:
 * Similar to e_table_item_compute_location, only here recalculating
 * the position inside the item too.
 **/
void
e_table_item_compute_mouse_over (ETableItem *eti,
                                 gint x,
                                 gint y,
                                 gint *row,
                                 gint *col)
{
	gdouble realx, realy;
	/* Save the grabbed row but make sure that we don't get flawed
	 * results because the cursor is grabbed. */
	gint grabbed_row = eti->grabbed_row;
	eti->grabbed_row = -1;

	realx = x;
	realy = y;

	gnome_canvas_item_w2i (GNOME_CANVAS_ITEM (eti), &realx, &realy);

	if (!find_cell (eti, (gint) realx, (gint) realy, col, row, NULL, NULL)) {
		*row = -1;
		*col = -1;
	}

	eti->grabbed_row = grabbed_row;
}

void
e_table_item_get_cell_geometry (ETableItem *eti,
                                gint *row,
                                gint *col,
                                gint *x,
                                gint *y,
                                gint *width,
                                gint *height)
{
	if (eti->rows > *row) {
		if (x)
			*x = e_table_header_col_diff (eti->header, 0, *col);
		if (y)
			*y = e_table_item_row_diff (eti, 0, *row);
		if (width)
			*width = e_table_header_col_diff (eti->header, *col, *col + 1);
		if (height)
			*height = ETI_ROW_HEIGHT (eti, *row);
		*row = -1;
		*col = -1;
	} else {
		*row -= eti->rows;
	}
}

typedef struct {
	ETableItem *item;
	gint rows_printed;
} ETableItemPrintContext;

static gdouble *
e_table_item_calculate_print_widths (ETableHeader *eth,
                                     gdouble width)
{
	gint i;
	gdouble extra;
	gdouble expansion;
	gint last_resizable = -1;
	gdouble scale = 1.0L;
	gdouble *widths = g_new (gdouble, e_table_header_count (eth));
	/* - 1 to account for the last pixel border. */
	extra = width - 1;
	expansion = 0;
	for (i = 0; i < eth->col_count; i++) {
		extra -= eth->columns[i]->min_width * scale;
		if (eth->columns[i]->spec->resizable && eth->columns[i]->expansion > 0)
			last_resizable = i;
		expansion += eth->columns[i]->spec->resizable ? eth->columns[i]->expansion : 0;
		widths[i] = eth->columns[i]->min_width * scale;
	}
	for (i = 0; i <= last_resizable; i++) {
		widths[i] += extra * (eth->columns[i]->spec->resizable ? eth->columns[i]->expansion : 0) / expansion;
	}

	return widths;
}

static gdouble
eti_printed_row_height (ETableItem *eti,
                        gdouble *widths,
                        GtkPrintContext *context,
                        gint row)
{
	gint col;
	gint cols = eti->cols;
	gdouble height = 0;
	for (col = 0; col < cols; col++) {
		ECellView *ecell_view = eti->cell_views[col];
		gdouble this_height = e_cell_print_height (
			ecell_view, context, view_to_model_col (eti, col), col, row,
			widths[col] - 1);
		if (this_height > height)
			height = this_height;
	}
	return height;
}

#define CHECK(x) if((x) == -1) return -1;

static gint
gp_draw_rect (GtkPrintContext *context,
              gdouble x,
              gdouble y,
              gdouble width,
              gdouble height)
{
	cairo_t *cr;
	cr = gtk_print_context_get_cairo_context (context);
	cairo_save (cr);
	cairo_rectangle (cr, x, y, width, height);
	cairo_set_line_width (cr, 0.5);
	cairo_stroke (cr);
	cairo_restore (cr);
	return 0;
}

static void
e_table_item_print_page (EPrintable *ep,
                         GtkPrintContext *context,
                         gdouble width,
                         gdouble height,
                         gboolean quantize,
                         ETableItemPrintContext *itemcontext)
{
	ETableItem *eti = itemcontext->item;
	const gint rows = eti->rows;
	const gint cols = eti->cols;
	gdouble max_height;
	gint rows_printed = itemcontext->rows_printed;
	gint row, col, next_page = 0;
	gdouble yd = height;
	cairo_t *cr;
	gdouble *widths;

	cr = gtk_print_context_get_cairo_context (context);
	max_height = gtk_print_context_get_height (context);
	widths = e_table_item_calculate_print_widths (itemcontext->item->header, width);

	/*
	 * Draw cells
	 */

	if (eti->horizontal_draw_grid) {
		gp_draw_rect (context, 0, yd, width, 1);
	}
	yd++;

	for (row = rows_printed; row < rows; row++) {
		gdouble xd = 1, row_height;
		row_height = eti_printed_row_height (eti, widths, context, row);

		if (quantize) {
			if (yd + row_height + 1 > max_height && row != rows_printed) {
				next_page = 1;
				break;
			}
		} else {
			if (yd > max_height) {
				next_page = 1;
				break;
			}
		}

		for (col = 0; col < cols; col++) {
			ECellView *ecell_view = eti->cell_views[col];

			cairo_save (cr);
			cairo_translate (cr, xd, yd);
			cairo_rectangle (cr, 0, 0, widths[col] - 1, row_height);
			cairo_clip (cr);

			e_cell_print (
				ecell_view, context,
				view_to_model_col (eti, col),
				col,
				row,
				widths[col] - 1,
				row_height + 2);

			cairo_restore (cr);

			xd += widths[col];
		}

	yd += row_height;
		if (eti->horizontal_draw_grid) {
			gp_draw_rect (context, 0, yd, width, 1);
		}
		yd++;
	}

	itemcontext->rows_printed = row;
	if (eti->vertical_draw_grid) {
		gdouble xd = 0;
		for (col = 0; col < cols; col++) {
			gp_draw_rect (context, xd, height, 1, yd - height);
			xd += widths[col];
		}
		gp_draw_rect (context, xd, height, 1, yd - height);
	}

	if (next_page)
		cairo_show_page (cr);

	g_free (widths);
}

static gboolean
e_table_item_data_left (EPrintable *ep,
                        ETableItemPrintContext *itemcontext)
{
	ETableItem *item = itemcontext->item;
	gint rows_printed = itemcontext->rows_printed;

	g_signal_stop_emission_by_name (ep, "data_left");
	return rows_printed < item->rows;
}

static void
e_table_item_reset (EPrintable *ep,
                    ETableItemPrintContext *itemcontext)
{
	itemcontext->rows_printed = 0;
}

static gdouble
e_table_item_height (EPrintable *ep,
                     GtkPrintContext *context,
                     gdouble width,
                     gdouble max_height,
                     gboolean quantize,
                     ETableItemPrintContext *itemcontext)
{
	ETableItem *item = itemcontext->item;
	const gint rows = item->rows;
	gint rows_printed = itemcontext->rows_printed;
	gdouble *widths;
	gint row;
	gdouble yd = 0;

	widths = e_table_item_calculate_print_widths (itemcontext->item->header, width);

	/*
	 * Draw cells
	 */
	yd++;

	for (row = rows_printed; row < rows; row++) {
		gdouble row_height;

		row_height = eti_printed_row_height (item, widths, context, row);
		if (quantize) {
			if (max_height != -1 && yd + row_height + 1 > max_height && row != rows_printed) {
				break;
			}
		} else {
			if (max_height != -1 && yd > max_height) {
				break;
			}
		}

		yd += row_height;

		yd++;
	}

	g_free (widths);

	if (max_height != -1 && (!quantize) && yd > max_height)
		yd = max_height;

	g_signal_stop_emission_by_name (ep, "height");
	return yd;
}

static gboolean
e_table_item_will_fit (EPrintable *ep,
                       GtkPrintContext *context,
                       gdouble width,
                       gdouble max_height,
                       gboolean quantize,
                       ETableItemPrintContext *itemcontext)
{
	ETableItem *item = itemcontext->item;
	const gint rows = item->rows;
	gint rows_printed = itemcontext->rows_printed;
	gdouble *widths;
	gint row;
	gdouble yd = 0;
	gboolean ret_val = TRUE;

	widths = e_table_item_calculate_print_widths (itemcontext->item->header, width);

	/*
	 * Draw cells
	 */
	yd++;

	for (row = rows_printed; row < rows; row++) {
		gdouble row_height;

		row_height = eti_printed_row_height (item, widths, context, row);
		if (quantize) {
			if (max_height != -1 && yd + row_height + 1 > max_height && row != rows_printed) {
				ret_val = FALSE;
				break;
			}
		} else {
			if (max_height != -1 && yd > max_height) {
				ret_val = FALSE;
				break;
			}
		}

		yd += row_height;

		yd++;
	}

	g_free (widths);

	g_signal_stop_emission_by_name (ep, "will_fit");
	return ret_val;
}

static void
e_table_item_printable_destroy (gpointer data,
                                GObject *where_object_was)
{
	ETableItemPrintContext *itemcontext = data;

	g_object_unref (itemcontext->item);
	g_free (itemcontext);
}

/**
 * e_table_item_get_printable
 * @eti: %ETableItem which will be printed
 *
 * This routine creates and returns an %EPrintable that can be used to
 * print the given %ETableItem.
 *
 * Returns: The %EPrintable.
 */
EPrintable *
e_table_item_get_printable (ETableItem *item)
{
	EPrintable *printable = e_printable_new ();
	ETableItemPrintContext *itemcontext;

	itemcontext = g_new (ETableItemPrintContext, 1);
	itemcontext->item = item;
	g_object_ref (item);
	itemcontext->rows_printed = 0;

	g_signal_connect (
		printable, "print_page",
		G_CALLBACK (e_table_item_print_page), itemcontext);
	g_signal_connect (
		printable, "data_left",
		G_CALLBACK (e_table_item_data_left), itemcontext);
	g_signal_connect (
		printable, "reset",
		G_CALLBACK (e_table_item_reset), itemcontext);
	g_signal_connect (
		printable, "height",
		G_CALLBACK (e_table_item_height), itemcontext);
	g_signal_connect (
		printable, "will_fit",
		G_CALLBACK (e_table_item_will_fit), itemcontext);

	g_object_weak_ref (
		G_OBJECT (printable),
		e_table_item_printable_destroy, itemcontext);

	return printable;
}

/**
 * e_table_item_is_editing:
 * @eti: an %ETableItem 
 *
 * Returns: Whether the table item is currently editing cell content.
 **/
gboolean
e_table_item_is_editing (ETableItem *eti)
{
	g_return_val_if_fail (E_IS_TABLE_ITEM (eti), FALSE);

	return eti_editing (eti);
}

/**
 * e_table_item_cursor_scrolled:
 * @eti: an %ETableItem 
 *
 * Does necessary recalculations after cursor scrolled, like whether
 * the cursor is on screen or not anymore.
 **/
void
e_table_item_cursor_scrolled (ETableItem *eti)
{
	g_return_if_fail (E_IS_TABLE_ITEM (eti));

	eti_check_cursor_bounds (eti);
}

void
e_table_item_cancel_scroll_to_cursor (ETableItem *eti)
{
	ETableItemPrivate *priv;

	g_return_if_fail (E_IS_TABLE_ITEM (eti));

	priv = e_table_item_get_instance_private (eti);

	if (priv->show_cursor_delay_source) {
		g_source_destroy (priv->show_cursor_delay_source);
		g_source_unref (priv->show_cursor_delay_source);
		priv->show_cursor_delay_source = NULL;
	}
}
