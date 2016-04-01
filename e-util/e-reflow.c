/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
 *              Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-reflow.h"

#include <math.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "e-canvas-utils.h"
#include "e-canvas.h"
#include "e-marshal.h"
#include "e-selection-model-simple.h"
#include "e-text.h"
#include "e-unicode.h"

static gboolean e_reflow_event (GnomeCanvasItem *item, GdkEvent *event);
static void e_reflow_realize (GnomeCanvasItem *item);
static void e_reflow_unrealize (GnomeCanvasItem *item);
static void e_reflow_draw (GnomeCanvasItem *item, cairo_t *cr,
				    gint x, gint y, gint width, gint height);
static void e_reflow_update (GnomeCanvasItem *item, const cairo_matrix_t *i2c, gint flags);
static GnomeCanvasItem *e_reflow_point (GnomeCanvasItem *item, gdouble x, gdouble y, gint cx, gint cy);
static void e_reflow_reflow (GnomeCanvasItem *item, gint flags);
static void set_empty (EReflow *reflow);

static void e_reflow_resize_children (GnomeCanvasItem *item);

#define E_REFLOW_DIVIDER_WIDTH 2
#define E_REFLOW_BORDER_WIDTH 7
#define E_REFLOW_FULL_GUTTER (E_REFLOW_DIVIDER_WIDTH + E_REFLOW_BORDER_WIDTH * 2)

G_DEFINE_TYPE (EReflow, e_reflow, GNOME_TYPE_CANVAS_GROUP)

enum {
	PROP_0,
	PROP_MINIMUM_WIDTH,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_EMPTY_MESSAGE,
	PROP_MODEL,
	PROP_COLUMN_WIDTH
};

enum {
	SELECTION_EVENT,
	COLUMN_WIDTH_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0, };

static GHashTable *
er_create_cmp_cache (gpointer user_data)
{
	EReflow *reflow = user_data;
	return e_reflow_model_create_cmp_cache (reflow->model);
}

static gint
er_compare (gint i1,
            gint i2,
            GHashTable *cmp_cache,
            gpointer user_data)
{
	EReflow *reflow = user_data;
	return e_reflow_model_compare (reflow->model, i1, i2, cmp_cache);
}

static gint
e_reflow_pick_line (EReflow *reflow,
                    gdouble x)
{
	x += E_REFLOW_BORDER_WIDTH + E_REFLOW_DIVIDER_WIDTH;
	x /= reflow->column_width + E_REFLOW_FULL_GUTTER;
	return x;
}

static gint
er_find_item (EReflow *reflow,
              GnomeCanvasItem *item)
{
	gint i;
	for (i = 0; i < reflow->count; i++) {
		if (reflow->items[i] == item)
			return i;
	}
	return -1;
}

static void
e_reflow_resize_children (GnomeCanvasItem *item)
{
	EReflow *reflow;
	gint i;
	gint count;

	reflow = E_REFLOW (item);

	count = reflow->count;
	for (i = 0; i < count; i++) {
		if (reflow->items[i])
			gnome_canvas_item_set (
				reflow->items[i],
				"width", (gdouble) reflow->column_width,
				NULL);
	}
}

static inline void
e_reflow_update_selection_row (EReflow *reflow,
                               gint row)
{
	if (reflow->items[row]) {
		g_object_set (
			reflow->items[row],
			"selected", e_selection_model_is_row_selected (E_SELECTION_MODEL (reflow->selection), row),
			NULL);
	} else if (e_selection_model_is_row_selected (E_SELECTION_MODEL (reflow->selection), row)) {
		reflow->items[row] = e_reflow_model_incarnate (reflow->model, row, GNOME_CANVAS_GROUP (reflow));
		g_object_set (
			reflow->items[row],
			"selected", e_selection_model_is_row_selected (E_SELECTION_MODEL (reflow->selection), row),
			"width", (gdouble) reflow->column_width,
			NULL);
	}
}

static void
e_reflow_update_selection (EReflow *reflow)
{
	gint i;
	gint count;

	count = reflow->count;
	for (i = 0; i < count; i++) {
		e_reflow_update_selection_row (reflow, i);
	}
}

static void
selection_changed (ESelectionModel *selection,
                   EReflow *reflow)
{
	e_reflow_update_selection (reflow);
}

static void
selection_row_changed (ESelectionModel *selection,
                       gint row,
                       EReflow *reflow)
{
	e_reflow_update_selection_row (reflow, row);
}

static gboolean
do_adjustment (gpointer user_data)
{
	gint row;
	GtkLayout *layout;
	GtkAdjustment *adjustment;
	gdouble page_size;
	gdouble value, min_value, max_value;
	EReflow *reflow = user_data;

	row = reflow->cursor_row;
	if (row == -1)
		return FALSE;

	layout = GTK_LAYOUT (GNOME_CANVAS_ITEM (reflow)->canvas);
	adjustment = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (layout));

	value = gtk_adjustment_get_value (adjustment);
	page_size = gtk_adjustment_get_page_size (adjustment);

	if ((!reflow->items) || (!reflow->items[row]))
		return TRUE;
	min_value = reflow->items[row]->x2 - page_size;
	max_value = reflow->items[row]->x1;

	if (value < min_value)
		value = min_value;

	if (value > max_value)
		value = max_value;

	if (value != gtk_adjustment_get_value (adjustment))
		gtk_adjustment_set_value (adjustment, value);

	reflow->do_adjustment_idle_id = 0;

	return FALSE;
}

static void
cursor_changed (ESelectionModel *selection,
                gint row,
                gint col,
                EReflow *reflow)
{
	gint count = reflow->count;
	gint old_cursor = reflow->cursor_row;

	if (old_cursor < count && old_cursor >= 0) {
		if (reflow->items[old_cursor]) {
			g_object_set (
				reflow->items[old_cursor],
				"has_cursor", FALSE,
				NULL);
		}
	}

	reflow->cursor_row = row;

	if (row < count && row >= 0) {
		if (reflow->items[row]) {
			g_object_set (
				reflow->items[row],
				"has_cursor", TRUE,
				NULL);
		} else {
			reflow->items[row] = e_reflow_model_incarnate (reflow->model, row, GNOME_CANVAS_GROUP (reflow));
			g_object_set (
				reflow->items[row],
				"has_cursor", TRUE,
				"width", (gdouble) reflow->column_width,
				NULL);
		}
	}

	if (reflow->do_adjustment_idle_id == 0)
		reflow->do_adjustment_idle_id = g_idle_add (do_adjustment, reflow);

}

static void
incarnate (EReflow *reflow)
{
	gint column_width;
	gint first_column;
	gint last_column;
	gint first_cell;
	gint last_cell;
	gint i;
	GtkLayout *layout;
	GtkAdjustment *adjustment;
	gdouble value;
	gdouble page_size;

	layout = GTK_LAYOUT (GNOME_CANVAS_ITEM (reflow)->canvas);
	adjustment = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (layout));

	value = gtk_adjustment_get_value (adjustment);
	page_size = gtk_adjustment_get_page_size (adjustment);

	column_width = reflow->column_width;

	first_column = value - 1 + E_REFLOW_BORDER_WIDTH;
	first_column /= column_width + E_REFLOW_FULL_GUTTER;

	last_column = value + page_size + 1 - E_REFLOW_BORDER_WIDTH - E_REFLOW_DIVIDER_WIDTH;
	last_column /= column_width + E_REFLOW_FULL_GUTTER;
	last_column++;

	if (first_column >= 0 && first_column < reflow->column_count)
		first_cell = reflow->columns[first_column];
	else
		first_cell = 0;

	if (last_column >= 0 && last_column < reflow->column_count)
		last_cell = reflow->columns[last_column];
	else
		last_cell = reflow->count;

	for (i = first_cell; i < last_cell; i++) {
		gint unsorted = e_sorter_sorted_to_model (E_SORTER (reflow->sorter), i);
		if (reflow->items[unsorted] == NULL) {
			if (reflow->model) {
				reflow->items[unsorted] = e_reflow_model_incarnate (reflow->model, unsorted, GNOME_CANVAS_GROUP (reflow));
				g_object_set (
					reflow->items[unsorted],
					"selected", e_selection_model_is_row_selected (E_SELECTION_MODEL (reflow->selection), unsorted),
					"width", (gdouble) reflow->column_width,
					NULL);
			}
		}
	}
	reflow->incarnate_idle_id = 0;
}

static gboolean
invoke_incarnate (gpointer user_data)
{
	EReflow *reflow = user_data;
	incarnate (reflow);
	return FALSE;
}

static void
queue_incarnate (EReflow *reflow)
{
	if (reflow->incarnate_idle_id == 0)
		reflow->incarnate_idle_id =
			g_idle_add_full (25, invoke_incarnate, reflow, NULL);
}

static void
reflow_columns (EReflow *reflow)
{
	GSList *list;
	gint count;
	gint start;
	gint i;
	gint column_count, column_start;
	gdouble running_height;

	if (reflow->reflow_from_column <= 1) {
		start = 0;
		column_count = 1;
		column_start = 0;
	}
	else {
		/* we start one column before the earliest new entry,
		 * so we can handle the case where the new entry is
		 * inserted at the start of the column */
		column_start = reflow->reflow_from_column - 1;
		start = reflow->columns[column_start];
		column_count = column_start + 1;
	}

	list = NULL;

	running_height = E_REFLOW_BORDER_WIDTH;

	count = reflow->count - start;
	for (i = start; i < count; i++) {
		gint unsorted = e_sorter_sorted_to_model (E_SORTER (reflow->sorter), i);
		if (i != 0 && running_height + reflow->heights[unsorted] + E_REFLOW_BORDER_WIDTH > reflow->height) {
			list = g_slist_prepend (list, GINT_TO_POINTER (i));
			column_count++;
			running_height = E_REFLOW_BORDER_WIDTH * 2 + reflow->heights[unsorted];
		} else
			running_height += reflow->heights[unsorted] + E_REFLOW_BORDER_WIDTH;
	}

	reflow->column_count = column_count;
	reflow->columns = g_renew (int, reflow->columns, column_count);
	column_count--;

	for (; list && column_count > column_start; column_count--) {
		GSList *to_free;
		reflow->columns[column_count] = GPOINTER_TO_INT (list->data);
		to_free = list;
		list = list->next;
		g_slist_free_1 (to_free);
	}
	reflow->columns[column_start] = start;

	queue_incarnate (reflow);

	reflow->need_reflow_columns = FALSE;
	reflow->reflow_from_column = -1;
}

static void
item_changed (EReflowModel *model,
              gint i,
              EReflow *reflow)
{
	if (i < 0 || i >= reflow->count)
		return;

	reflow->heights[i] = e_reflow_model_height (reflow->model, i, GNOME_CANVAS_GROUP (reflow));
	if (reflow->items[i] != NULL)
		e_reflow_model_reincarnate (model, i, reflow->items[i]);
	e_sorter_array_clean (reflow->sorter);
	reflow->reflow_from_column = -1;
	reflow->need_reflow_columns = TRUE;
	e_canvas_item_request_reflow (GNOME_CANVAS_ITEM (reflow));
}

static void
item_removed (EReflowModel *model,
              gint i,
              EReflow *reflow)
{
	gint c;
	gint sorted;

	if (i < 0 || i >= reflow->count)
		return;

	sorted = e_sorter_model_to_sorted (E_SORTER (reflow->sorter), i);
	for (c = reflow->column_count - 1; c >= 0; c--) {
		gint start_of_column = reflow->columns[c];

		if (start_of_column <= sorted) {
			if (reflow->reflow_from_column == -1
			    || reflow->reflow_from_column > c) {
				reflow->reflow_from_column = c;
			}
			break;
		}
	}

	if (reflow->items[i])
		g_object_run_dispose (G_OBJECT (reflow->items[i]));

	memmove (reflow->heights + i, reflow->heights + i + 1, (reflow->count - i - 1) * sizeof (gint));
	memmove (reflow->items + i, reflow->items + i + 1, (reflow->count - i - 1) * sizeof (GnomeCanvasItem *));

	reflow->count--;

	reflow->heights[reflow->count] = 0;
	reflow->items[reflow->count] = NULL;

	reflow->need_reflow_columns = TRUE;
	set_empty (reflow);
	e_canvas_item_request_reflow (GNOME_CANVAS_ITEM (reflow));

	e_sorter_array_set_count (reflow->sorter, reflow->count);

	e_selection_model_simple_delete_rows (E_SELECTION_MODEL_SIMPLE (reflow->selection), i, 1);
}

static void
items_inserted (EReflowModel *model,
                gint position,
                gint count,
                EReflow *reflow)
{
	gint i, oldcount;

	if (position < 0 || position > reflow->count)
		return;

	oldcount = reflow->count;

	reflow->count += count;

	if (reflow->count > reflow->allocated_count) {
		while (reflow->count > reflow->allocated_count)
			reflow->allocated_count += 256;
		reflow->heights = g_renew (int, reflow->heights, reflow->allocated_count);
		reflow->items = g_renew (GnomeCanvasItem *, reflow->items, reflow->allocated_count);
	}
	memmove (reflow->heights + position + count, reflow->heights + position, (reflow->count - position - count) * sizeof (gint));
	memmove (reflow->items + position + count, reflow->items + position, (reflow->count - position - count) * sizeof (GnomeCanvasItem *));
	for (i = position; i < position + count; i++) {
		reflow->items[i] = NULL;
		reflow->heights[i] = e_reflow_model_height (reflow->model, i, GNOME_CANVAS_GROUP (reflow));
	}

	e_selection_model_simple_set_row_count (E_SELECTION_MODEL_SIMPLE (reflow->selection), reflow->count);
	if (position == oldcount)
		e_sorter_array_append (reflow->sorter, count);
	else
		e_sorter_array_set_count (reflow->sorter, reflow->count);

	for (i = position; i < position + count; i++) {
		gint sorted = e_sorter_model_to_sorted (E_SORTER (reflow->sorter), i);
		gint c;

		for (c = reflow->column_count - 1; c >= 0; c--) {
			gint start_of_column = reflow->columns[c];

			if (start_of_column <= sorted) {
				if (reflow->reflow_from_column == -1
				    || reflow->reflow_from_column > c) {
					reflow->reflow_from_column = c;
				}
				break;
			}
		}
	}

	reflow->need_reflow_columns = TRUE;
	set_empty (reflow);
	e_canvas_item_request_reflow (GNOME_CANVAS_ITEM (reflow));
}

static void
model_changed (EReflowModel *model,
               EReflow *reflow)
{
	gint i;
	gint count;
	gint oldcount;

	count = reflow->count;
	oldcount = count;

	for (i = 0; i < count; i++) {
		if (reflow->items[i])
			g_object_run_dispose (G_OBJECT (reflow->items[i]));
	}
	g_free (reflow->items);
	g_free (reflow->heights);
	reflow->count = e_reflow_model_count (model);
	reflow->allocated_count = reflow->count;
	reflow->items = g_new (GnomeCanvasItem *, reflow->count);
	reflow->heights = g_new (int, reflow->count);

	count = reflow->count;
	for (i = 0; i < count; i++) {
		reflow->items[i] = NULL;
		reflow->heights[i] = e_reflow_model_height (reflow->model, i, GNOME_CANVAS_GROUP (reflow));
	}

	e_selection_model_simple_set_row_count (E_SELECTION_MODEL_SIMPLE (reflow->selection), count);
	e_sorter_array_set_count (reflow->sorter, reflow->count);

	reflow->need_reflow_columns = TRUE;
	if (oldcount > reflow->count)
		reflow_columns (reflow);
	set_empty (reflow);
	e_canvas_item_request_reflow (GNOME_CANVAS_ITEM (reflow));
}

static void
comparison_changed (EReflowModel *model,
                    EReflow *reflow)
{
	e_sorter_array_clean (reflow->sorter);
	reflow->reflow_from_column = -1;
	reflow->need_reflow_columns = TRUE;
	e_canvas_item_request_reflow (GNOME_CANVAS_ITEM (reflow));
}

static void
set_empty (EReflow *reflow)
{
	if (reflow->count == 0) {
		if (reflow->empty_text) {
			if (reflow->empty_message) {
				gnome_canvas_item_set (
					reflow->empty_text,
					"text", reflow->empty_message,
					NULL);
			} else {
				g_object_run_dispose (G_OBJECT (reflow->empty_text));
				reflow->empty_text = NULL;
			}
		} else {
			if (reflow->empty_message) {
				reflow->empty_text = gnome_canvas_item_new (
					GNOME_CANVAS_GROUP (reflow),
					e_text_get_type (),
					"clip", TRUE,
					"use_ellipsis", TRUE,
					"justification", GTK_JUSTIFY_LEFT,
					"text", reflow->empty_message,
					NULL);
			}
		}

		if (reflow->empty_text) {
			gdouble text_width = -1.0;

			g_object_get (reflow->empty_text, "text_width", &text_width, NULL);

			e_canvas_item_move_absolute (
				reflow->empty_text,
				(MAX (reflow->width - text_width, 0) + E_REFLOW_BORDER_WIDTH) / 2,
				0);
		}
	} else {
		if (reflow->empty_text) {
			g_object_run_dispose (G_OBJECT (reflow->empty_text));
			reflow->empty_text = NULL;
		}
	}
}

static void
disconnect_model (EReflow *reflow)
{
	if (reflow->model == NULL)
		return;

	g_signal_handler_disconnect (
		reflow->model,
		reflow->model_changed_id);
	g_signal_handler_disconnect (
		reflow->model,
		reflow->comparison_changed_id);
	g_signal_handler_disconnect (
		reflow->model,
		reflow->model_items_inserted_id);
	g_signal_handler_disconnect (
		reflow->model,
		reflow->model_item_removed_id);
	g_signal_handler_disconnect (
		reflow->model,
		reflow->model_item_changed_id);
	g_object_unref (reflow->model);

	reflow->model_changed_id = 0;
	reflow->comparison_changed_id = 0;
	reflow->model_items_inserted_id = 0;
	reflow->model_item_removed_id = 0;
	reflow->model_item_changed_id = 0;
	reflow->model = NULL;
}

static void
disconnect_selection (EReflow *reflow)
{
	if (reflow->selection == NULL)
		return;

	g_signal_handler_disconnect (
		reflow->selection,
		reflow->selection_changed_id);
	g_signal_handler_disconnect (
		reflow->selection,
		reflow->selection_row_changed_id);
	g_signal_handler_disconnect (
		reflow->selection,
		reflow->cursor_changed_id);
	g_object_unref (reflow->selection);

	reflow->selection_changed_id = 0;
	reflow->selection_row_changed_id = 0;
	reflow->cursor_changed_id = 0;
	reflow->selection = NULL;
}

static void
connect_model (EReflow *reflow,
               EReflowModel *model)
{
	if (reflow->model != NULL)
		disconnect_model (reflow);

	if (model == NULL)
		return;

	reflow->model = g_object_ref (model);

	reflow->model_changed_id = g_signal_connect (
		reflow->model, "model_changed",
		G_CALLBACK (model_changed), reflow);

	reflow->comparison_changed_id = g_signal_connect (
		reflow->model, "comparison_changed",
		G_CALLBACK (comparison_changed), reflow);

	reflow->model_items_inserted_id = g_signal_connect (
		reflow->model, "model_items_inserted",
		G_CALLBACK (items_inserted), reflow);

	reflow->model_item_removed_id = g_signal_connect (
		reflow->model, "model_item_removed",
		G_CALLBACK (item_removed), reflow);

	reflow->model_item_changed_id = g_signal_connect (
		reflow->model, "model_item_changed",
		G_CALLBACK (item_changed), reflow);

	model_changed (model, reflow);
}

static void
adjustment_changed (GtkAdjustment *adjustment,
                    EReflow *reflow)
{
	queue_incarnate (reflow);
}

static void
disconnect_adjustment (EReflow *reflow)
{
	if (reflow->adjustment == NULL)
		return;

	g_signal_handler_disconnect (
		reflow->adjustment,
		reflow->adjustment_changed_id);
	g_signal_handler_disconnect (
		reflow->adjustment,
		reflow->adjustment_value_changed_id);

	g_object_unref (reflow->adjustment);

	reflow->adjustment_changed_id = 0;
	reflow->adjustment_value_changed_id = 0;
	reflow->adjustment = NULL;
}

static void
connect_adjustment (EReflow *reflow,
                    GtkAdjustment *adjustment)
{
	if (reflow->adjustment != NULL)
		disconnect_adjustment (reflow);

	if (adjustment == NULL)
		return;

	reflow->adjustment = g_object_ref (adjustment);

	reflow->adjustment_changed_id = g_signal_connect (
		adjustment, "changed",
		G_CALLBACK (adjustment_changed), reflow);

	reflow->adjustment_value_changed_id = g_signal_connect (
		adjustment, "value_changed",
		G_CALLBACK (adjustment_changed), reflow);
}

static void
set_scroll_adjustments (GnomeCanvas *canvas,
                        GParamSpec *param,
                        EReflow *reflow)
{
	connect_adjustment (reflow, gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (GNOME_CANVAS_ITEM (reflow)->canvas)));
}

static void
connect_set_adjustment (EReflow *reflow)
{
	reflow->set_scroll_adjustments_id = g_signal_connect (
		GNOME_CANVAS_ITEM (reflow)->canvas, "notify::hadjustment",
		G_CALLBACK (set_scroll_adjustments), reflow);
}

static void
disconnect_set_adjustment (EReflow *reflow)
{
	if (reflow->set_scroll_adjustments_id != 0) {
		g_signal_handler_disconnect (
			GNOME_CANVAS_ITEM (reflow)->canvas,
			reflow->set_scroll_adjustments_id);
		reflow->set_scroll_adjustments_id = 0;
	}
}

static void
column_width_changed (EReflow *reflow)
{
	g_signal_emit (reflow, signals[COLUMN_WIDTH_CHANGED], 0, reflow->column_width);
}

/* Virtual functions */
static void
e_reflow_set_property (GObject *object,
                       guint property_id,
                       const GValue *value,
                       GParamSpec *pspec)
{
	GnomeCanvasItem *item;
	EReflow *reflow;

	item = GNOME_CANVAS_ITEM (object);
	reflow = E_REFLOW (object);

	switch (property_id) {
	case PROP_HEIGHT:
		reflow->height = g_value_get_double (value);
		reflow->need_reflow_columns = TRUE;
		e_canvas_item_request_reflow (item);
		break;
	case PROP_MINIMUM_WIDTH:
		reflow->minimum_width = g_value_get_double (value);
		if (item->flags & GNOME_CANVAS_ITEM_REALIZED)
			set_empty (reflow);
		e_canvas_item_request_reflow (item);
		break;
	case PROP_EMPTY_MESSAGE:
		g_free (reflow->empty_message);
		reflow->empty_message = g_strdup (g_value_get_string (value));
		if (item->flags & GNOME_CANVAS_ITEM_REALIZED)
			set_empty (reflow);
		break;
	case PROP_MODEL:
		connect_model (reflow, (EReflowModel *) g_value_get_object (value));
		break;
	case PROP_COLUMN_WIDTH:
		if (reflow->column_width != g_value_get_double (value)) {
			GtkLayout *layout;
			GtkAdjustment *adjustment;
			gdouble old_width = reflow->column_width;
			gdouble step_increment;
			gdouble page_size;

			layout = GTK_LAYOUT (item->canvas);
			adjustment = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (layout));
			page_size = gtk_adjustment_get_page_size (adjustment);

			reflow->column_width = g_value_get_double (value);
			step_increment = (reflow->column_width +
				E_REFLOW_FULL_GUTTER) / 2;
			gtk_adjustment_set_step_increment (
				adjustment, step_increment);
			gtk_adjustment_set_page_increment (
				adjustment, page_size - step_increment);
			e_reflow_resize_children (item);
			e_canvas_item_request_reflow (item);

			reflow->need_column_resize = TRUE;
			gnome_canvas_item_request_update (item);

			if (old_width != reflow->column_width)
				column_width_changed (reflow);
		}
		break;
	}
}

static void
e_reflow_get_property (GObject *object,
                       guint property_id,
                       GValue *value,
                       GParamSpec *pspec)
{
	EReflow *reflow;

	reflow = E_REFLOW (object);

	switch (property_id) {
	case PROP_MINIMUM_WIDTH:
		g_value_set_double (value, reflow->minimum_width);
		break;
	case PROP_WIDTH:
		g_value_set_double (value, reflow->width);
		break;
	case PROP_HEIGHT:
		g_value_set_double (value, reflow->height);
		break;
	case PROP_EMPTY_MESSAGE:
		g_value_set_string (value, reflow->empty_message);
		break;
	case PROP_MODEL:
		g_value_set_object (value, reflow->model);
		break;
	case PROP_COLUMN_WIDTH:
		g_value_set_double (value, reflow->column_width);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_reflow_dispose (GObject *object)
{
	EReflow *reflow = E_REFLOW (object);

	g_free (reflow->items);
	g_free (reflow->heights);
	g_free (reflow->columns);

	reflow->items = NULL;
	reflow->heights = NULL;
	reflow->columns = NULL;
	reflow->count = 0;
	reflow->allocated_count = 0;

	if (reflow->incarnate_idle_id)
		g_source_remove (reflow->incarnate_idle_id);
	reflow->incarnate_idle_id = 0;

	if (reflow->do_adjustment_idle_id)
		g_source_remove (reflow->do_adjustment_idle_id);
	reflow->do_adjustment_idle_id = 0;

	disconnect_model (reflow);
	disconnect_selection (reflow);

	g_free (reflow->empty_message);
	reflow->empty_message = NULL;

	if (reflow->sorter) {
		g_object_unref (reflow->sorter);
		reflow->sorter = NULL;
	}

	G_OBJECT_CLASS (e_reflow_parent_class)->dispose (object);
}

static void
e_reflow_realize (GnomeCanvasItem *item)
{
	EReflow *reflow;
	GtkAdjustment *adjustment;
	gdouble page_increment;
	gdouble step_increment;
	gdouble page_size;
	gint count;
	gint i;

	reflow = E_REFLOW (item);

	if (GNOME_CANVAS_ITEM_CLASS (e_reflow_parent_class)->realize)
		(* GNOME_CANVAS_ITEM_CLASS (e_reflow_parent_class)->realize) (item);

	reflow->arrow_cursor = gdk_cursor_new (GDK_SB_H_DOUBLE_ARROW);
	reflow->default_cursor = gdk_cursor_new (GDK_LEFT_PTR);

	count = reflow->count;
	for (i = 0; i < count; i++) {
		if (reflow->items[i])
			gnome_canvas_item_set (
				reflow->items[i],
				"width", reflow->column_width,
				NULL);
	}

	set_empty (reflow);

	reflow->need_reflow_columns = TRUE;
	e_canvas_item_request_reflow (item);

	adjustment = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (item->canvas));

	connect_set_adjustment (reflow);
	connect_adjustment (reflow, adjustment);

	page_size = gtk_adjustment_get_page_size (adjustment);
	step_increment = (reflow->column_width + E_REFLOW_FULL_GUTTER) / 2;
	page_increment = page_size - step_increment;
	gtk_adjustment_set_step_increment (adjustment, step_increment);
	gtk_adjustment_set_page_increment (adjustment, page_increment);
}

static void
e_reflow_unrealize (GnomeCanvasItem *item)
{
	EReflow *reflow;

	reflow = E_REFLOW (item);

	g_object_unref (reflow->arrow_cursor);
	g_object_unref (reflow->default_cursor);
	reflow->arrow_cursor = NULL;
	reflow->default_cursor = NULL;

	g_free (reflow->columns);
	reflow->columns = NULL;

	disconnect_set_adjustment (reflow);
	disconnect_adjustment (reflow);

	if (GNOME_CANVAS_ITEM_CLASS (e_reflow_parent_class)->unrealize)
		(* GNOME_CANVAS_ITEM_CLASS (e_reflow_parent_class)->unrealize) (item);
}

static gboolean
e_reflow_event (GnomeCanvasItem *item,
                GdkEvent *event)
{
	EReflow *reflow;
	gint return_val = FALSE;

	reflow = E_REFLOW (item);

	switch (event->type)
		{
		case GDK_KEY_PRESS:
			return_val = e_selection_model_key_press (reflow->selection, (GdkEventKey *) event);
			break;
#if 0
			if (event->key.keyval == GDK_Tab ||
			    event->key.keyval == GDK_KEY_KP_Tab ||
			    event->key.keyval == GDK_ISO_Left_Tab) {
				gint i;
				gint count;
				count = reflow->count;
				for (i = 0; i < count; i++) {
					gint unsorted = e_sorter_sorted_to_model (E_SORTER (reflow->sorter), i);
					GnomeCanvasItem *item = reflow->items[unsorted];
					EFocus has_focus;
					if (item) {
						g_object_get (
							item,
							"has_focus", &has_focus,
							NULL);
						if (has_focus) {
							if (event->key.state & GDK_SHIFT_MASK) {
								if (i == 0)
									return FALSE;
								i--;
							} else {
								if (i == count - 1)
									return FALSE;
								i++;
							}

							unsorted = e_sorter_sorted_to_model (E_SORTER (reflow->sorter), i);
							if (reflow->items[unsorted] == NULL) {
								reflow->items[unsorted] = e_reflow_model_incarnate (reflow->model, unsorted, GNOME_CANVAS_GROUP (reflow));
							}

							item = reflow->items[unsorted];
							gnome_canvas_item_set (
								item,
								"has_focus", (event->key.state & GDK_SHIFT_MASK) ? E_FOCUS_END : E_FOCUS_START,
								NULL);
							return TRUE;
						}
					}
				}
			}
#endif
		case GDK_BUTTON_PRESS:
			switch (event->button.button)
				{
				case 1:
					{
						GdkEventButton *button = (GdkEventButton *) event;
						gdouble n_x;
						n_x = button->x;
						n_x += E_REFLOW_BORDER_WIDTH + E_REFLOW_DIVIDER_WIDTH;
						n_x = fmod (n_x,(reflow->column_width + E_REFLOW_FULL_GUTTER));

						if (button->y >= E_REFLOW_BORDER_WIDTH && button->y <= reflow->height - E_REFLOW_BORDER_WIDTH && n_x < E_REFLOW_FULL_GUTTER) {
							/* don't allow to drag the first line*/
							if (e_reflow_pick_line (reflow, button->x) == 0)
								return TRUE;
							reflow->which_column_dragged = e_reflow_pick_line (reflow, button->x);
							reflow->start_x = reflow->which_column_dragged * (reflow->column_width + E_REFLOW_FULL_GUTTER) - E_REFLOW_DIVIDER_WIDTH / 2;
							reflow->temp_column_width = reflow->column_width;
							reflow->column_drag = TRUE;

							gnome_canvas_item_grab (
								item,
								GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK,
								reflow->arrow_cursor,
								button->device,
								button->time);

							reflow->previous_temp_column_width = -1;
							reflow->need_column_resize = TRUE;
							gnome_canvas_item_request_update (item);
							return TRUE;
						}
					}
					break;
				case 4:
					{
						GtkLayout *layout;
						GtkAdjustment *adjustment;
						gdouble new_value;

						layout = GTK_LAYOUT (item->canvas);
						adjustment = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (layout));
						new_value = gtk_adjustment_get_value (adjustment);
						new_value -= gtk_adjustment_get_step_increment (adjustment);
						gtk_adjustment_set_value (adjustment, new_value);
					}
					break;
				case 5:
					{
						GtkLayout *layout;
						GtkAdjustment *adjustment;
						gdouble new_value;
						gdouble page_size;
						gdouble upper;

						layout = GTK_LAYOUT (item->canvas);
						adjustment = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (layout));
						new_value = gtk_adjustment_get_value (adjustment);
						new_value += gtk_adjustment_get_step_increment (adjustment);
						upper = gtk_adjustment_get_upper (adjustment);
						page_size = gtk_adjustment_get_page_size (adjustment);
						if (new_value > upper - page_size)
							new_value = upper - page_size;
						gtk_adjustment_set_value (adjustment, new_value);
					}
					break;
				}
			break;
		case GDK_BUTTON_RELEASE:
			if (reflow->column_drag) {
				gdouble old_width = reflow->column_width;
				GdkEventButton *button = (GdkEventButton *) event;
				GtkAdjustment *adjustment;
				GtkLayout *layout;
				gdouble value;

				layout = GTK_LAYOUT (item->canvas);
				adjustment = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (layout));
				value = gtk_adjustment_get_value (adjustment);

				reflow->temp_column_width = reflow->column_width +
					(button->x - reflow->start_x) / (reflow->which_column_dragged - e_reflow_pick_line (reflow, value));
				if (reflow->temp_column_width < 50)
					reflow->temp_column_width = 50;
				reflow->column_drag = FALSE;
				if (old_width != reflow->temp_column_width) {
					gdouble page_increment;
					gdouble step_increment;
					gdouble page_size;

					page_size = gtk_adjustment_get_page_size (adjustment);
					gtk_adjustment_set_value (adjustment, value + e_reflow_pick_line (reflow, value) * (reflow->temp_column_width - reflow->column_width));
					reflow->column_width = reflow->temp_column_width;
					step_increment = (reflow->column_width + E_REFLOW_FULL_GUTTER) / 2;
					page_increment = page_size - step_increment;
					gtk_adjustment_set_step_increment (adjustment, step_increment);
					gtk_adjustment_set_page_increment (adjustment, page_increment);
					e_reflow_resize_children (item);
					e_canvas_item_request_reflow (item);
					gnome_canvas_request_redraw (item->canvas, 0, 0, reflow->width, reflow->height);
					column_width_changed (reflow);
				}
				reflow->need_column_resize = TRUE;
				gnome_canvas_item_request_update (item);
				gnome_canvas_item_ungrab (item, button->time);
				return TRUE;
			}
			break;
		case GDK_MOTION_NOTIFY:
			if (reflow->column_drag) {
				gdouble old_width = reflow->temp_column_width;
				GdkEventMotion *motion = (GdkEventMotion *) event;
				GtkAdjustment *adjustment;
				GtkLayout *layout;
				gdouble value;

				layout = GTK_LAYOUT (item->canvas);
				adjustment = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (layout));
				value = gtk_adjustment_get_value (adjustment);

				reflow->temp_column_width = reflow->column_width +
					(motion->x - reflow->start_x) / (reflow->which_column_dragged - e_reflow_pick_line (reflow, value));
				if (reflow->temp_column_width < 50)
					reflow->temp_column_width = 50;
				if (old_width != reflow->temp_column_width) {
					reflow->need_column_resize = TRUE;
					gnome_canvas_item_request_update (item);
				}
				return TRUE;
			} else {
				GdkEventMotion *motion = (GdkEventMotion *) event;
				GdkWindow *window;
				gdouble n_x;

				n_x = motion->x;
				n_x += E_REFLOW_BORDER_WIDTH + E_REFLOW_DIVIDER_WIDTH;
				n_x = fmod (n_x,(reflow->column_width + E_REFLOW_FULL_GUTTER));

				window = gtk_widget_get_window (GTK_WIDGET (item->canvas));

				if (motion->y >= E_REFLOW_BORDER_WIDTH && motion->y <= reflow->height - E_REFLOW_BORDER_WIDTH && n_x < E_REFLOW_FULL_GUTTER) {
					if (reflow->default_cursor_shown) {
						gdk_window_set_cursor (window, reflow->arrow_cursor);
						reflow->default_cursor_shown = FALSE;
					}
				} else
					if (!reflow->default_cursor_shown) {
						gdk_window_set_cursor (window, reflow->default_cursor);
						reflow->default_cursor_shown = TRUE;
					}

			}
			break;
		case GDK_ENTER_NOTIFY:
			if (!reflow->column_drag) {
				GdkEventCrossing *crossing = (GdkEventCrossing *) event;
				GdkWindow *window;
				gdouble n_x;

				n_x = crossing->x;
				n_x += E_REFLOW_BORDER_WIDTH + E_REFLOW_DIVIDER_WIDTH;
				n_x = fmod (n_x,(reflow->column_width + E_REFLOW_FULL_GUTTER));

				window = gtk_widget_get_window (GTK_WIDGET (item->canvas));

				if (crossing->y >= E_REFLOW_BORDER_WIDTH && crossing->y <= reflow->height - E_REFLOW_BORDER_WIDTH && n_x < E_REFLOW_FULL_GUTTER) {
					if (reflow->default_cursor_shown) {
						gdk_window_set_cursor (window, reflow->arrow_cursor);
						reflow->default_cursor_shown = FALSE;
					}
				}
			}
			break;
		case GDK_LEAVE_NOTIFY:
			if (!reflow->column_drag) {
				GdkEventCrossing *crossing = (GdkEventCrossing *) event;
				GdkWindow *window;
				gdouble n_x;

				n_x = crossing->x;
				n_x += E_REFLOW_BORDER_WIDTH + E_REFLOW_DIVIDER_WIDTH;
				n_x = fmod (n_x,(reflow->column_width + E_REFLOW_FULL_GUTTER));

				window = gtk_widget_get_window (GTK_WIDGET (item->canvas));

				if (!(crossing->y >= E_REFLOW_BORDER_WIDTH && crossing->y <= reflow->height - E_REFLOW_BORDER_WIDTH && n_x < E_REFLOW_FULL_GUTTER)) {
					if (!reflow->default_cursor_shown) {
						gdk_window_set_cursor (window, reflow->default_cursor);
						reflow->default_cursor_shown = TRUE;
					}
				}
			}
			break;
		default:
			break;
		}
	if (return_val)
		return return_val;
	else if (GNOME_CANVAS_ITEM_CLASS (e_reflow_parent_class)->event)
		return (* GNOME_CANVAS_ITEM_CLASS (e_reflow_parent_class)->event) (item, event);
	else
		return FALSE;
}

static void
e_reflow_draw (GnomeCanvasItem *item,
               cairo_t *cr,
               gint x,
               gint y,
               gint width,
               gint height)
{
	GtkStyleContext *style_context;
	GtkWidget *widget;
	gint x_rect, y_rect, width_rect, height_rect;
	gdouble running_width;
	EReflow *reflow = E_REFLOW (item);
	GdkRGBA color;
	gint i;
	gdouble column_width;

	if (GNOME_CANVAS_ITEM_CLASS (e_reflow_parent_class)->draw)
		GNOME_CANVAS_ITEM_CLASS (e_reflow_parent_class)->draw (item, cr, x, y, width, height);
	column_width = reflow->column_width;
	running_width = E_REFLOW_BORDER_WIDTH + column_width + E_REFLOW_BORDER_WIDTH;
	y_rect = E_REFLOW_BORDER_WIDTH;
	width_rect = E_REFLOW_DIVIDER_WIDTH;
	height_rect = reflow->height - (E_REFLOW_BORDER_WIDTH * 2);

	/* Compute first column to draw. */
	i = x;
	i /= column_width + E_REFLOW_FULL_GUTTER;
	running_width += i * (column_width + E_REFLOW_FULL_GUTTER);

	widget = GTK_WIDGET (item->canvas);
	style_context = gtk_widget_get_style_context (widget);

	cairo_save (cr);

	for (; i < reflow->column_count; i++) {
		if (running_width > x + width)
			break;
		x_rect = running_width;

		gtk_render_background (
			style_context, cr,
			(gdouble) x_rect - x,
			(gdouble) y_rect - y,
			(gdouble) width_rect,
			(gdouble) height_rect);

		running_width += E_REFLOW_DIVIDER_WIDTH + E_REFLOW_BORDER_WIDTH + column_width + E_REFLOW_BORDER_WIDTH;
	}

	cairo_restore (cr);

	if (reflow->column_drag) {
		GtkAdjustment *adjustment;
		GtkLayout *layout;
		gdouble value;
		gint start_line;

		layout = GTK_LAYOUT (item->canvas);
		adjustment = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (layout));
		value = gtk_adjustment_get_value (adjustment);

		start_line = e_reflow_pick_line (reflow, value);
		i = x - start_line * (column_width + E_REFLOW_FULL_GUTTER);
		running_width = start_line * (column_width + E_REFLOW_FULL_GUTTER);
		column_width = reflow->temp_column_width;
		running_width -= start_line * (column_width + E_REFLOW_FULL_GUTTER);
		i += start_line * (column_width + E_REFLOW_FULL_GUTTER);
		running_width += E_REFLOW_BORDER_WIDTH + column_width + E_REFLOW_BORDER_WIDTH;
		y_rect = E_REFLOW_BORDER_WIDTH;
		width_rect = E_REFLOW_DIVIDER_WIDTH;
		height_rect = reflow->height - (E_REFLOW_BORDER_WIDTH * 2);

		/* Compute first column to draw. */
		i /= column_width + E_REFLOW_FULL_GUTTER;
		running_width += i * (column_width + E_REFLOW_FULL_GUTTER);

		cairo_save (cr);

		gtk_style_context_get_color (
			style_context, gtk_style_context_get_state (style_context), &color);
		gdk_cairo_set_source_rgba (cr, &color);

		for (; i < reflow->column_count; i++) {
			if (running_width > x + width)
				break;
			x_rect = running_width;
			cairo_rectangle (
				cr,
				x_rect - x,
				y_rect - y,
				width_rect - 1,
				height_rect - 1);
			cairo_fill (cr);
			running_width += E_REFLOW_DIVIDER_WIDTH + E_REFLOW_BORDER_WIDTH + column_width + E_REFLOW_BORDER_WIDTH;
		}

		cairo_restore (cr);
	}
}

static void
e_reflow_update (GnomeCanvasItem *item,
                 const cairo_matrix_t *i2c,
                 gint flags)
{
	EReflow *reflow;
	gdouble x0, x1, y0, y1;

	reflow = E_REFLOW (item);

	if (GNOME_CANVAS_ITEM_CLASS (e_reflow_parent_class)->update)
		GNOME_CANVAS_ITEM_CLASS (e_reflow_parent_class)->update (item, i2c, flags);

	x0 = item->x1;
	y0 = item->y1;
	x1 = item->x2;
	y1 = item->y2;
	if (x1 < x0 + reflow->width)
		x1 = x0 + reflow->width;
	if (y1 < y0 + reflow->height)
		y1 = y0 + reflow->height;
	item->x2 = x1;
	item->y2 = y1;

	if (reflow->need_height_update) {
		x0 = item->x1;
		y0 = item->y1;
		x1 = item->x2;
		y1 = item->y2;
		if (x0 > 0)
			x0 = 0;
		if (y0 > 0)
			y0 = 0;
		if (x1 < E_REFLOW (item)->width)
			x1 = E_REFLOW (item)->width;
		if (x1 < E_REFLOW (item)->height)
			x1 = E_REFLOW (item)->height;

		gnome_canvas_request_redraw (item->canvas, x0, y0, x1, y1);
		reflow->need_height_update = FALSE;
	} else if (reflow->need_column_resize) {
		GtkLayout *layout;
		GtkAdjustment *adjustment;
		gint x_rect, y_rect, width_rect, height_rect;
		gint start_line;
		gdouble running_width;
		gint i;
		gdouble column_width;
		gdouble value;

		layout = GTK_LAYOUT (item->canvas);
		adjustment = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (layout));
		value = gtk_adjustment_get_value (adjustment);
		start_line = e_reflow_pick_line (reflow, value);

		if (reflow->previous_temp_column_width != -1) {
			running_width = start_line * (reflow->column_width + E_REFLOW_FULL_GUTTER);
			column_width = reflow->previous_temp_column_width;
			running_width -= start_line * (column_width + E_REFLOW_FULL_GUTTER);
			running_width += E_REFLOW_BORDER_WIDTH + column_width + E_REFLOW_BORDER_WIDTH;
			y_rect = E_REFLOW_BORDER_WIDTH;
			width_rect = E_REFLOW_DIVIDER_WIDTH;
			height_rect = reflow->height - (E_REFLOW_BORDER_WIDTH * 2);

			for (i = 0; i < reflow->column_count; i++) {
				x_rect = running_width;
				gnome_canvas_request_redraw (item->canvas, x_rect, y_rect, x_rect + width_rect, y_rect + height_rect);
				running_width += E_REFLOW_DIVIDER_WIDTH + E_REFLOW_BORDER_WIDTH + column_width + E_REFLOW_BORDER_WIDTH;
			}
		}

		if (reflow->temp_column_width != -1) {
			running_width = start_line * (reflow->column_width + E_REFLOW_FULL_GUTTER);
			column_width = reflow->temp_column_width;
			running_width -= start_line * (column_width + E_REFLOW_FULL_GUTTER);
			running_width += E_REFLOW_BORDER_WIDTH + column_width + E_REFLOW_BORDER_WIDTH;
			y_rect = E_REFLOW_BORDER_WIDTH;
			width_rect = E_REFLOW_DIVIDER_WIDTH;
			height_rect = reflow->height - (E_REFLOW_BORDER_WIDTH * 2);

			for (i = 0; i < reflow->column_count; i++) {
				x_rect = running_width;
				gnome_canvas_request_redraw (item->canvas, x_rect, y_rect, x_rect + width_rect, y_rect + height_rect);
				running_width += E_REFLOW_DIVIDER_WIDTH + E_REFLOW_BORDER_WIDTH + column_width + E_REFLOW_BORDER_WIDTH;
			}
		}

		reflow->previous_temp_column_width = reflow->temp_column_width;
		reflow->need_column_resize = FALSE;
	}
}

static GnomeCanvasItem *
e_reflow_point (GnomeCanvasItem *item,
                gdouble x,
                gdouble y,
                gint cx,
                gint cy)
{
	GnomeCanvasItem *child = NULL;

	if (GNOME_CANVAS_ITEM_CLASS (e_reflow_parent_class)->point)
		child = GNOME_CANVAS_ITEM_CLASS (e_reflow_parent_class)->point (item, x, y, cx, cy);

	return child ? child : item;
#if 0
	if (y >= E_REFLOW_BORDER_WIDTH && y <= reflow->height - E_REFLOW_BORDER_WIDTH) {
		gfloat n_x;
		n_x = x;
		n_x += E_REFLOW_BORDER_WIDTH + E_REFLOW_DIVIDER_WIDTH;
		n_x = fmod (n_x, (reflow->column_width + E_REFLOW_FULL_GUTTER));
		if (n_x < E_REFLOW_FULL_GUTTER) {
			*actual_item = item;
			return 0;
		}
	}
	return distance;
#endif
}

static void
e_reflow_reflow (GnomeCanvasItem *item,
                 gint flags)
{
	EReflow *reflow = E_REFLOW (item);
	gdouble old_width;
	gdouble running_width;
	gdouble running_height;
	gint next_column;
	gint i;

	if (!(item->flags & GNOME_CANVAS_ITEM_REALIZED))
		return;

	if (reflow->need_reflow_columns) {
		reflow_columns (reflow);
	}

	old_width = reflow->width;

	running_width = E_REFLOW_BORDER_WIDTH;
	running_height = E_REFLOW_BORDER_WIDTH;

	next_column = 1;

	for (i = 0; i < reflow->count; i++) {
		gint unsorted = e_sorter_sorted_to_model (E_SORTER (reflow->sorter), i);
		if (next_column < reflow->column_count && i == reflow->columns[next_column]) {
			running_height = E_REFLOW_BORDER_WIDTH;
			running_width += reflow->column_width + E_REFLOW_FULL_GUTTER;
			next_column++;
		}

		if (unsorted >= 0 && reflow->items[unsorted]) {
			e_canvas_item_move_absolute (
				GNOME_CANVAS_ITEM (reflow->items[unsorted]),
				(gdouble) running_width,
				(gdouble) running_height);
			running_height += reflow->heights[unsorted] + E_REFLOW_BORDER_WIDTH;
		}
	}
	reflow->width = running_width + reflow->column_width + E_REFLOW_BORDER_WIDTH;
	if (reflow->width < reflow->minimum_width)
		reflow->width = reflow->minimum_width;
	if (reflow->empty_text) {
		gdouble text_width = -1.0;

		g_object_get (reflow->empty_text, "text_width", &text_width, NULL);

		if (text_width + (E_REFLOW_BORDER_WIDTH * 2) > reflow->width)
			reflow->width = text_width + (E_REFLOW_BORDER_WIDTH * 2);
	}
	if (old_width != reflow->width)
		e_canvas_item_request_parent_reflow (item);
}

static gint
e_reflow_selection_event_real (EReflow *reflow,
                               GnomeCanvasItem *item,
                               GdkEvent *event)
{
	gint row;
	gint return_val = TRUE;
	switch (event->type) {
	case GDK_BUTTON_PRESS:
		switch (event->button.button) {
		case 1: /* Fall through. */
		case 2:
			row = er_find_item (reflow, item);
			if (event->button.button == 1) {
				reflow->maybe_did_something =
					e_selection_model_maybe_do_something (reflow->selection, row, 0, event->button.state);
				reflow->maybe_in_drag = TRUE;
			} else {
				e_selection_model_do_something (reflow->selection, row, 0, event->button.state);
			}
			break;
		case 3:
			row = er_find_item (reflow, item);
			e_selection_model_right_click_down (reflow->selection, row, 0, 0);
			break;
		default:
			return_val = FALSE;
			break;
		}
		break;
	case GDK_BUTTON_RELEASE:
		if (event->button.button == 1) {
			if (reflow->maybe_in_drag) {
				reflow->maybe_in_drag = FALSE;
				if (!reflow->maybe_did_something) {
					row = er_find_item (reflow, item);
					e_selection_model_do_something (reflow->selection, row, 0, event->button.state);
				}
			}
		}
		break;
	case GDK_KEY_PRESS:
		return_val = e_selection_model_key_press (reflow->selection, (GdkEventKey *) event);
		break;
	default:
		return_val = FALSE;
		break;
	}

	return return_val;
}

static void
e_reflow_class_init (EReflowClass *class)
{
	GObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = (GObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	object_class->set_property = e_reflow_set_property;
	object_class->get_property = e_reflow_get_property;
	object_class->dispose = e_reflow_dispose;

	/* GnomeCanvasItem method overrides */
	item_class->event = e_reflow_event;
	item_class->realize = e_reflow_realize;
	item_class->unrealize = e_reflow_unrealize;
	item_class->draw = e_reflow_draw;
	item_class->update = e_reflow_update;
	item_class->point = e_reflow_point;

	class->selection_event = e_reflow_selection_event_real;
	class->column_width_changed = NULL;

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
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_HEIGHT,
		g_param_spec_double (
			"height",
			"Height",
			"Height",
			0.0, G_MAXDOUBLE, 0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_EMPTY_MESSAGE,
		g_param_spec_string (
			"empty_message",
			"Empty message",
			"Empty message",
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_MODEL,
		g_param_spec_object (
			"model",
			"Reflow model",
			"Reflow model",
			E_TYPE_REFLOW_MODEL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_COLUMN_WIDTH,
		g_param_spec_double (
			"column_width",
			"Column width",
			"Column width",
			0.0, G_MAXDOUBLE, 150.0,
			G_PARAM_READWRITE));

	signals[SELECTION_EVENT] = g_signal_new (
		"selection_event",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EReflowClass, selection_event),
		NULL, NULL,
		e_marshal_INT__OBJECT_BOXED,
		G_TYPE_INT, 2,
		G_TYPE_OBJECT,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	signals[COLUMN_WIDTH_CHANGED] = g_signal_new (
		"column_width_changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EReflowClass, column_width_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__DOUBLE,
		G_TYPE_NONE, 1,
		G_TYPE_DOUBLE);
}

static void
e_reflow_init (EReflow *reflow)
{
	reflow->model = NULL;
	reflow->items = NULL;
	reflow->heights = NULL;
	reflow->count = 0;

	reflow->columns = NULL;
	reflow->column_count = 0;

	reflow->empty_text = NULL;
	reflow->empty_message = NULL;

	reflow->minimum_width = 10;
	reflow->width = 10;
	reflow->height = 10;

	reflow->column_width = 150;

	reflow->column_drag = FALSE;

	reflow->need_height_update = FALSE;
	reflow->need_column_resize = FALSE;
	reflow->need_reflow_columns = FALSE;

	reflow->maybe_did_something = FALSE;
	reflow->maybe_in_drag = FALSE;

	reflow->default_cursor_shown = TRUE;
	reflow->arrow_cursor = NULL;
	reflow->default_cursor = NULL;

	reflow->cursor_row = -1;

	reflow->incarnate_idle_id = 0;
	reflow->do_adjustment_idle_id = 0;
	reflow->set_scroll_adjustments_id = 0;

	reflow->selection = E_SELECTION_MODEL (e_selection_model_simple_new ());
	reflow->sorter = e_sorter_array_new (er_create_cmp_cache, er_compare, reflow);

	g_object_set (
		reflow->selection,
		"sorter", reflow->sorter,
		NULL);

	reflow->selection_changed_id = g_signal_connect (
		reflow->selection, "selection_changed",
		G_CALLBACK (selection_changed), reflow);

	reflow->selection_row_changed_id = g_signal_connect (
		reflow->selection, "selection_row_changed",
		G_CALLBACK (selection_row_changed), reflow);

	reflow->cursor_changed_id = g_signal_connect (
		reflow->selection, "cursor_changed",
		G_CALLBACK (cursor_changed), reflow);

	e_canvas_item_set_reflow_callback (GNOME_CANVAS_ITEM (reflow), e_reflow_reflow);
}
