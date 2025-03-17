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
 *		Miguel de Icaza <miguel@gnu.org>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-table-header-item.h"

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libgnomecanvas/libgnomecanvas.h>

#include "e-canvas.h"
#include "e-popup-menu.h"
#include "e-table-col-dnd.h"
#include "e-table-config.h"
#include "e-table-defines.h"
#include "e-table-field-chooser-dialog.h"
#include "e-table-header-utils.h"
#include "e-table-header.h"
#include "e-table.h"
#include "e-xml-utils.h"

enum {
	BUTTON_PRESSED,
	HEADER_CLICK_CAN_SORT,
	LAST_SIGNAL
};

static guint ethi_signals[LAST_SIGNAL] = { 0, };

#define ARROW_DOWN_HEIGHT 16
#define ARROW_PTR          7

/* Defines the tolerance for proximity of the column division to the cursor position */
#define TOLERANCE 4

#define ETHI_RESIZING(x) ((x)->resize_col != -1)

#define ethi_get_type e_table_header_item_get_type
G_DEFINE_TYPE (ETableHeaderItem, ethi, GNOME_TYPE_CANVAS_ITEM)

#define d(x)

static void ethi_drop_table_header (ETableHeaderItem *ethi);

/*
 * They display the arrows for the drop location.
 */

static GtkWidget *arrow_up, *arrow_down;

enum {
	PROP_0,
	PROP_TABLE_HEADER,
	PROP_FULL_HEADER,
	PROP_DND_CODE,
	PROP_TABLE_FONT_DESC,
	PROP_SORT_INFO,
	PROP_TABLE,
	PROP_TREE
};

enum {
	ET_SCROLL_UP = 1 << 0,
	ET_SCROLL_DOWN = 1 << 1,
	ET_SCROLL_LEFT = 1 << 2,
	ET_SCROLL_RIGHT = 1 << 3
};

static void scroll_off (ETableHeaderItem *ethi);
static void scroll_on (ETableHeaderItem *ethi, guint scroll_direction);

static void
ethi_dispose (GObject *object)
{
	ETableHeaderItem *ethi = E_TABLE_HEADER_ITEM (object);

	ethi_drop_table_header (ethi);

	scroll_off (ethi);

	g_clear_object (&ethi->resize_cursor);
	g_clear_pointer (&ethi->dnd_code, g_free);

	if (ethi->sort_info) {
		if (ethi->sort_info_changed_id)
			g_signal_handler_disconnect (
				ethi->sort_info, ethi->sort_info_changed_id);
		if (ethi->group_info_changed_id)
			g_signal_handler_disconnect (
				ethi->sort_info, ethi->group_info_changed_id);
		g_object_unref (ethi->sort_info);
		ethi->sort_info = NULL;
	}

	g_clear_object (&ethi->full_header);

	if (ethi->etfcd.widget)
		g_object_remove_weak_pointer (
			G_OBJECT (ethi->etfcd.widget), &ethi->etfcd.pointer);

	g_clear_object (&ethi->config);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (ethi_parent_class)->dispose (object);
}

static gint
e_table_header_item_get_height (ETableHeaderItem *ethi)
{
	ETableHeader *eth;
	gint numcols, col;
	gint maxheight;

	g_return_val_if_fail (ethi != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_HEADER_ITEM (ethi), 0);

	eth = ethi->eth;
	numcols = e_table_header_count (eth);

	maxheight = 0;

	for (col = 0; col < numcols; col++) {
		ETableCol *ecol = e_table_header_get_column (eth, col);
		gint height;

		height = e_table_header_compute_height (
			ecol, GTK_WIDGET (GNOME_CANVAS_ITEM (ethi)->canvas));

		if (height > maxheight)
			maxheight = height;
	}

	return maxheight;
}

static void
ethi_update (GnomeCanvasItem *item,
             const cairo_matrix_t *i2c,
             gint flags)
{
	ETableHeaderItem *ethi = E_TABLE_HEADER_ITEM (item);
	gdouble x1, y1, x2, y2;

	if (GNOME_CANVAS_ITEM_CLASS (ethi_parent_class)->update)
		GNOME_CANVAS_ITEM_CLASS (ethi_parent_class)->update (
			item, i2c, flags);

	if (ethi->sort_info)
		ethi->group_indent_width =
			e_table_sort_info_grouping_get_count (ethi->sort_info)
			* GROUP_INDENT;
	else
		ethi->group_indent_width = 0;

	ethi->width =
		e_table_header_total_width (ethi->eth) +
		ethi->group_indent_width;

	x1 = y1 = 0;
	x2 = ethi->width;
	y2 = ethi->height;

	gnome_canvas_matrix_transform_rect (i2c, &x1, &y1, &x2, &y2);

	if (item->x1 != x1 ||
	    item->y1 != y1 ||
	    item->x2 != x2 ||
	    item->y2 != y2) {
		gnome_canvas_request_redraw (
			item->canvas,
			item->x1, item->y1,
			item->x2, item->y2);
		item->x1 = x1;
		item->y1 = y1;
		item->x2 = x2;
		item->y2 = y2;
	}
	gnome_canvas_request_redraw (
		item->canvas, item->x1, item->y1, item->x2, item->y2);
}

static void
ethi_font_set (ETableHeaderItem *ethi,
               PangoFontDescription *font_desc)
{
	if (ethi->font_desc)
		pango_font_description_free (ethi->font_desc);

	ethi->font_desc = pango_font_description_copy (font_desc);

	ethi->height = e_table_header_item_get_height (ethi);
	e_canvas_item_request_reflow (GNOME_CANVAS_ITEM (ethi));
}

static void
ethi_drop_table_header (ETableHeaderItem *ethi)
{
	GObject *header;

	if (!ethi->eth)
		return;

	header = G_OBJECT (ethi->eth);
	g_signal_handler_disconnect (header, ethi->structure_change_id);
	g_signal_handler_disconnect (header, ethi->dimension_change_id);

	g_object_unref (header);
	ethi->eth = NULL;
	ethi->width = 0;
}

static void
structure_changed (ETableHeader *header,
                   ETableHeaderItem *ethi)
{
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (ethi));
}

static void
dimension_changed (ETableHeader *header,
                   gint col,
                   ETableHeaderItem *ethi)
{
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (ethi));
}

static void
ethi_add_table_header (ETableHeaderItem *ethi,
                       ETableHeader *header)
{
	ethi->eth = header;
	g_object_ref (ethi->eth);

	ethi->height = e_table_header_item_get_height (ethi);

	ethi->structure_change_id = g_signal_connect (
		header, "structure_change",
		G_CALLBACK (structure_changed), ethi);
	ethi->dimension_change_id = g_signal_connect (
		header, "dimension_change",
		G_CALLBACK (dimension_changed), ethi);
	e_canvas_item_request_reflow (GNOME_CANVAS_ITEM (ethi));
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (ethi));
}

static void
ethi_sort_info_changed (ETableSortInfo *sort_info,
                        ETableHeaderItem *ethi)
{
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (ethi));
}

static void
ethi_set_property (GObject *object,
                   guint property_id,
                   const GValue *value,
                   GParamSpec *pspec)
{
	GnomeCanvasItem *item;
	ETableHeaderItem *ethi;

	item = GNOME_CANVAS_ITEM (object);
	ethi = E_TABLE_HEADER_ITEM (object);

	switch (property_id) {
	case PROP_TABLE_HEADER:
		ethi_drop_table_header (ethi);
		ethi_add_table_header (ethi, E_TABLE_HEADER (g_value_get_object (value)));
		break;

	case PROP_FULL_HEADER:
		if (ethi->full_header)
			g_object_unref (ethi->full_header);
		ethi->full_header = E_TABLE_HEADER (g_value_get_object (value));
		if (ethi->full_header)
			g_object_ref (ethi->full_header);
		break;

	case PROP_DND_CODE:
		g_free (ethi->dnd_code);
		ethi->dnd_code = g_strdup (g_value_get_string (value));
		break;

	case PROP_TABLE_FONT_DESC:
		ethi_font_set (ethi, g_value_get_boxed (value));
		break;

	case PROP_SORT_INFO:
		if (ethi->sort_info) {
			if (ethi->sort_info_changed_id)
				g_signal_handler_disconnect (
					ethi->sort_info,
					ethi->sort_info_changed_id);

			if (ethi->group_info_changed_id)
				g_signal_handler_disconnect (
					ethi->sort_info,
					ethi->group_info_changed_id);
			g_object_unref (ethi->sort_info);
		}
		ethi->sort_info = g_value_get_object (value);
		g_object_ref (ethi->sort_info);
		ethi->sort_info_changed_id =
			g_signal_connect (
				ethi->sort_info, "sort_info_changed",
				G_CALLBACK (ethi_sort_info_changed), ethi);
		ethi->group_info_changed_id =
			g_signal_connect (
				ethi->sort_info, "group_info_changed",
				G_CALLBACK (ethi_sort_info_changed), ethi);
		break;
	case PROP_TABLE:
		if (g_value_get_object (value))
			ethi->table = E_TABLE (g_value_get_object (value));
		else
			ethi->table = NULL;
		break;
	case PROP_TREE:
		if (g_value_get_object (value))
			ethi->tree = E_TREE (g_value_get_object (value));
		else
			ethi->tree = NULL;
		break;
	}
	gnome_canvas_item_request_update (item);
}

static void
ethi_get_property (GObject *object,
                   guint property_id,
                   GValue *value,
                   GParamSpec *pspec)
{
	ETableHeaderItem *ethi;

	ethi = E_TABLE_HEADER_ITEM (object);

	switch (property_id) {
	case PROP_FULL_HEADER:
		g_value_set_object (value, ethi->full_header);
		break;
	case PROP_DND_CODE:
		g_value_set_string (value, ethi->dnd_code);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static gint
ethi_find_col_by_x (ETableHeaderItem *ethi,
                    gint x)
{
	const gint cols = e_table_header_count (ethi->eth);
	gint x1 = 0;
	gint col;

	d (g_print ("%s:%d: x = %d, x1 = %d\n", G_STRFUNC, __LINE__, x, x1));

	x1 += ethi->group_indent_width;

	if (x < x1) {
		d (g_print ("%s:%d: Returning 0\n", G_STRFUNC, __LINE__));
		return 0;
	}

	for (col = 0; col < cols; col++) {
		ETableCol *ecol = e_table_header_get_column (ethi->eth, col);

		if ((x >= x1) && (x <= x1 + ecol->width)) {
			d (g_print ("%s:%d: Returning %d\n", G_STRFUNC, __LINE__, col));
			return col;
		}

		x1 += ecol->width;
	}
	d (g_print ("%s:%d: Returning %d\n", G_STRFUNC, __LINE__, cols - 1));
	return cols - 1;
}

static gint
ethi_find_col_by_x_nearest (ETableHeaderItem *ethi,
                            gint x)
{
	const gint cols = e_table_header_count (ethi->eth);
	gint x1 = 0;
	gint col;

	x1 += ethi->group_indent_width;

	if (x < x1)
		return 0;

	for (col = 0; col < cols; col++) {
		ETableCol *ecol = e_table_header_get_column (ethi->eth, col);

		x1 += (ecol->width / 2);

		if (x <= x1)
			return col;

		x1 += (ecol->width + 1) / 2;
	}
	return col;
}

static void
ethi_remove_drop_marker (ETableHeaderItem *ethi)
{
	if (ethi->drag_mark == -1)
		return;

	gtk_widget_hide (arrow_up);
	gtk_widget_hide (arrow_down);

	ethi->drag_mark = -1;
}

static GtkWidget *
make_shaped_window_from_svg (const gchar *image_name)
{
	GtkWidget *win, *pix;
	gchar *resource_path;

	win = gtk_window_new (GTK_WINDOW_POPUP);
	gtk_window_set_type_hint (GTK_WINDOW (win), GDK_WINDOW_TYPE_HINT_NOTIFICATION);
	gtk_window_set_resizable (GTK_WINDOW (win), FALSE);

	resource_path = g_strconcat ("/org.gnome.Evolution/", image_name, NULL);
	pix = gtk_image_new_from_resource (resource_path);
	g_clear_pointer (&resource_path, g_free);
	gtk_container_add (GTK_CONTAINER (win), pix);

	return win;
}

static void
ethi_add_drop_marker (ETableHeaderItem *ethi,
                      gint col,
                      gboolean recreate)
{
	GnomeCanvas *canvas;
	GtkAdjustment *adjustment;
	GdkWindow *window;
	GtkWidget *toplevel;
	gint rx, ry;
	gint x;

	if (!recreate && ethi->drag_mark == col)
		return;

	ethi->drag_mark = col;

	x = e_table_header_col_diff (ethi->eth, 0, col);
	if (col > 0)
		x += ethi->group_indent_width;

	if (!arrow_up) {
		arrow_up = make_shaped_window_from_svg ("arrow-up.svg");
		arrow_down = make_shaped_window_from_svg ("arrow-down.svg");
	}

	canvas = GNOME_CANVAS_ITEM (ethi)->canvas;
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (canvas));
	if (GTK_IS_WINDOW (toplevel)) {
		gtk_window_set_transient_for (GTK_WINDOW (arrow_up), GTK_WINDOW (toplevel));
		gtk_window_set_transient_for (GTK_WINDOW (arrow_down), GTK_WINDOW (toplevel));
	}

	window = gtk_widget_get_window (GTK_WIDGET (canvas));
	gdk_window_get_origin (window, &rx, &ry);

	adjustment = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (canvas));
	rx -= gtk_adjustment_get_value (adjustment);

	adjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (canvas));
	ry -= gtk_adjustment_get_value (adjustment);

	gtk_window_move (
		GTK_WINDOW (arrow_down),
		rx + x - ARROW_PTR,
		ry - ARROW_DOWN_HEIGHT);
	gtk_widget_show_all (arrow_down);

	gtk_window_move (
		GTK_WINDOW (arrow_up),
		rx + x - ARROW_PTR,
		ry + ethi->height);
	gtk_widget_show_all (arrow_up);
}

static void
ethi_add_destroy_marker (ETableHeaderItem *ethi)
{
	GdkRGBA rgba;
	gdouble x1;

	if (ethi->remove_item)
		g_object_run_dispose (G_OBJECT (ethi->remove_item));

	x1 = (gdouble) e_table_header_col_diff (ethi->eth, 0, ethi->drag_col);
	if (ethi->drag_col > 0)
		x1 += ethi->group_indent_width;

	rgba.red = 1.0;
	rgba.green = 0.0;
	rgba.blue = 0.0;
	rgba.alpha = 0.5;
	ethi->remove_item = gnome_canvas_item_new (
		GNOME_CANVAS_GROUP (GNOME_CANVAS_ITEM (ethi)->canvas->root),
		gnome_canvas_rect_get_type (),
		"x1", x1 + 1,
		"y1", (gdouble) 1,
		"x2", (gdouble) x1 + e_table_header_col_diff (
			ethi->eth, ethi->drag_col, ethi->drag_col + 1) - 2,

		"y2", (gdouble) ethi->height - 2,
		"fill-color", &rgba,
		NULL);
}

static void
ethi_remove_destroy_marker (ETableHeaderItem *ethi)
{
	if (!ethi->remove_item)
		return;

	g_object_run_dispose (G_OBJECT (ethi->remove_item));
	ethi->remove_item = NULL;
}

static void
do_drag_motion (ETableHeaderItem *ethi,
                GdkDragContext *context,
                gint x,
                gint y,
                guint time,
                gboolean recreate)
{
	if ((x >= 0) && (x <= (ethi->width)) &&
	    (y >= 0) && (y <= (ethi->height))) {
		GdkDragAction suggested_action;
		gint col;
		d (g_print ("In header\n"));

		col = ethi_find_col_by_x_nearest (ethi, x);
		suggested_action = gdk_drag_context_get_suggested_action (context);

		if (ethi->drag_col != -1 && (col == ethi->drag_col || col == ethi->drag_col + 1)) {
			ethi_remove_destroy_marker (ethi);
			ethi_remove_drop_marker (ethi);
			gdk_drag_status (context, suggested_action, time);
		} else if (col != -1) {
			if (ethi->drag_col != -1)
				ethi_remove_destroy_marker (ethi);

			ethi_add_drop_marker (ethi, col, recreate);
			gdk_drag_status (context, suggested_action, time);
		} else {
			ethi_remove_drop_marker (ethi);
			if (ethi->drag_col != -1)
				ethi_add_destroy_marker (ethi);
		}
	} else {
		ethi_remove_drop_marker (ethi);
		if (ethi->drag_col != -1)
			ethi_add_destroy_marker (ethi);
	}
}

static gboolean
scroll_timeout (gpointer data)
{
	ETableHeaderItem *ethi = data;
	gint dx = 0;
	GtkAdjustment *adjustment;
	GtkScrollable *scrollable;
	gdouble hadjustment_value;
	gdouble vadjustment_value;
	gdouble page_size;
	gdouble lower;
	gdouble upper;
	gdouble value;

	if (ethi->scroll_direction & ET_SCROLL_RIGHT)
		dx += 20;
	if (ethi->scroll_direction & ET_SCROLL_LEFT)
		dx -= 20;

	scrollable = GTK_SCROLLABLE (GNOME_CANVAS_ITEM (ethi)->canvas);

	adjustment = gtk_scrollable_get_hadjustment (scrollable);
	hadjustment_value = gtk_adjustment_get_value (adjustment);

	adjustment = gtk_scrollable_get_vadjustment (scrollable);
	vadjustment_value = gtk_adjustment_get_value (adjustment);

	value = hadjustment_value;

	adjustment = gtk_scrollable_get_hadjustment (scrollable);
	page_size = gtk_adjustment_get_page_size (adjustment);
	lower = gtk_adjustment_get_lower (adjustment);
	upper = gtk_adjustment_get_upper (adjustment);

	gtk_adjustment_set_value (
		adjustment, CLAMP (
		hadjustment_value + dx, lower, upper - page_size));

	hadjustment_value = gtk_adjustment_get_value (adjustment);

	if (hadjustment_value != value)
		do_drag_motion (
			ethi,
			ethi->last_drop_context,
			ethi->last_drop_x + hadjustment_value,
			ethi->last_drop_y + vadjustment_value,
			ethi->last_drop_time,
			TRUE);

	return TRUE;
}

static void
scroll_on (ETableHeaderItem *ethi,
           guint scroll_direction)
{
	if (ethi->scroll_idle_id == 0 || scroll_direction != ethi->scroll_direction) {
		if (ethi->scroll_idle_id != 0)
			g_source_remove (ethi->scroll_idle_id);
		ethi->scroll_direction = scroll_direction;
		ethi->scroll_idle_id = e_named_timeout_add (
			100, scroll_timeout, ethi);
	}
}

static void
scroll_off (ETableHeaderItem *ethi)
{
	if (ethi->scroll_idle_id) {
		g_source_remove (ethi->scroll_idle_id);
		ethi->scroll_idle_id = 0;
	}
}

static void
context_destroyed (gpointer data)
{
	ETableHeaderItem *ethi = data;

	ethi->last_drop_x = 0;
	ethi->last_drop_y = 0;
	ethi->last_drop_time = 0;
	ethi->last_drop_context = NULL;
	scroll_off (ethi);

	g_object_unref (ethi);
}

static void
context_connect (ETableHeaderItem *ethi,
                 GdkDragContext *context)
{
	if (g_dataset_get_data (context, "e-table-header-item") == NULL)
		g_dataset_set_data_full (
			context, "e-table-header-item",
			g_object_ref (ethi), context_destroyed);
}

static gboolean
ethi_drag_motion (GtkWidget *widget,
                  GdkDragContext *context,
                  gint x,
                  gint y,
                  guint time,
                  ETableHeaderItem *ethi)
{
	GtkAllocation allocation;
	GtkAdjustment *adjustment;
	GList *targets, *link;
	gdouble hadjustment_value;
	gdouble vadjustment_value;
	gchar *headertype;
	guint direction = 0;

	gdk_drag_status (context, 0, time);

	headertype = g_strdup_printf ("%s-%s", TARGET_ETABLE_COL_TYPE, ethi->dnd_code);
	targets = gdk_drag_context_list_targets (context);
	for (link = targets; link; link = g_list_next (link)) {
		gchar *droptype;

		droptype = gdk_atom_name (GDK_POINTER_TO_ATOM (link->data));
		if (g_strcmp0 (droptype, headertype) == 0) {
			g_free (droptype);
			break;
		}

		g_free (droptype);
	}

	if (!link) {
		g_free (headertype);
		return FALSE;
	}

	g_free (headertype);

	gtk_widget_get_allocation (widget, &allocation);

	if (x < 20)
		direction |= ET_SCROLL_LEFT;
	if (x > allocation.width - 20)
		direction |= ET_SCROLL_RIGHT;

	ethi->last_drop_x = x;
	ethi->last_drop_y = y;
	ethi->last_drop_time = time;
	ethi->last_drop_context = context;
	context_connect (ethi, context);

	adjustment = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (widget));
	hadjustment_value = gtk_adjustment_get_value (adjustment);

	adjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (widget));
	vadjustment_value = gtk_adjustment_get_value (adjustment);

	do_drag_motion (
		ethi, context,
		x + hadjustment_value,
		y + vadjustment_value,
		time, FALSE);

	if (direction != 0)
		scroll_on (ethi, direction);
	else
		scroll_off (ethi);

	return TRUE;
}

static void
ethi_drag_end (GtkWidget *canvas,
               GdkDragContext *context,
               ETableHeaderItem *ethi)
{
	ethi_remove_drop_marker (ethi);
	ethi_remove_destroy_marker (ethi);
	ethi->drag_col = -1;
	scroll_off (ethi);
}

static void
ethi_drag_data_received (GtkWidget *canvas,
                         GdkDragContext *drag_context,
                         gint x,
                         gint y,
                         GtkSelectionData *selection_data,
                         guint info,
                         guint time,
                         ETableHeaderItem *ethi)
{
	const guchar *data;
	gint found = FALSE;
	gint count;
	gint column;
	gint drop_col;
	gint i;

	data = gtk_selection_data_get_data (selection_data);

	if (data != NULL) {
		count = e_table_header_count (ethi->eth);
		column = atoi ((gchar *) data);
		drop_col = ethi->drop_col;
		ethi->drop_col = -1;

		if (column >= 0) {
			for (i = 0; i < count; i++) {
				ETableCol *ecol = e_table_header_get_column (ethi->eth, i);
				if (ecol->spec->model_col == column) {
					e_table_header_move (ethi->eth, i, drop_col);
					found = TRUE;
					break;
				}
			}
			if (!found) {
				count = e_table_header_count (ethi->full_header);
				for (i = 0; i < count; i++) {
					ETableCol *ecol;

					ecol = e_table_header_get_column (
						ethi->full_header, i);

					if (ecol->spec->model_col == column) {
						e_table_header_add_column (
							ethi->eth, ecol,
							drop_col);
						break;
					}
				}
			}
		}
	}
	ethi_remove_drop_marker (ethi);
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (ethi));
}

static void
ethi_drag_data_get (GtkWidget *canvas,
                    GdkDragContext *context,
                    GtkSelectionData *selection_data,
                    guint info,
                    guint time,
                    ETableHeaderItem *ethi)
{
	if (ethi->drag_col != -1) {
		ETableCol *ecol = e_table_header_get_column (ethi->eth, ethi->drag_col);

		gchar *string = g_strdup_printf ("%d", ecol->spec->model_col);
		gtk_selection_data_set (
			selection_data,
			GDK_SELECTION_TYPE_STRING,
			sizeof (string[0]),
			(guchar *) string,
			strlen (string));
		g_free (string);
	}
}

static gboolean
ethi_drag_drop (GtkWidget *canvas,
                GdkDragContext *context,
                gint x,
                gint y,
                guint time,
                ETableHeaderItem *ethi)
{
	gboolean successful = FALSE;

	if ((x >= 0) && (x <= (ethi->width)) &&
	    (y >= 0) && (y <= (ethi->height))) {
		gint col;

		col = ethi_find_col_by_x_nearest (ethi, x);

		ethi_add_drop_marker (ethi, col, FALSE);

		ethi->drop_col = col;

		if (col != -1) {
			gchar *target = g_strdup_printf (
				"%s-%s", TARGET_ETABLE_COL_TYPE, ethi->dnd_code);
			d (g_print ("ethi -  %s\n", target));
			gtk_drag_get_data (
				canvas, context,
				gdk_atom_intern (target, FALSE),
				time);
			g_free (target);
		}
	}
	gtk_drag_finish (context, successful, successful, time);
	scroll_off (ethi);
	return successful;
}

static void
ethi_drag_leave (GtkWidget *widget,
                 GdkDragContext *context,
                 guint time,
                 ETableHeaderItem *ethi)
{
	ethi_remove_drop_marker (ethi);
	if (ethi->drag_col != -1)
		ethi_add_destroy_marker (ethi);
}

static void
ethi_style_updated_cb (GtkWidget *widget,
		       ETableHeaderItem *ethi)
{
	PangoContext *pango_context;

	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (E_IS_TABLE_HEADER_ITEM (ethi));

	pango_context = gtk_widget_get_pango_context (widget);

	ethi_font_set (ethi, pango_context_get_font_description (pango_context));
}

static void
ethi_realize (GnomeCanvasItem *item)
{
	ETableHeaderItem *ethi = E_TABLE_HEADER_ITEM (item);
	GtkTargetEntry  ethi_drop_types[] = {
		{ (gchar *) TARGET_ETABLE_COL_TYPE, 0, TARGET_ETABLE_COL_HEADER },
	};

	if (GNOME_CANVAS_ITEM_CLASS (ethi_parent_class)-> realize)
		(*GNOME_CANVAS_ITEM_CLASS (ethi_parent_class)->realize)(item);

	if (!ethi->font_desc) {
		PangoContext *pango_context;

		pango_context = gtk_widget_get_pango_context (GTK_WIDGET (item->canvas));

		ethi_font_set (ethi, pango_context_get_font_description (pango_context));
	}

	g_signal_connect (
		item->canvas, "style-updated",
		G_CALLBACK (ethi_style_updated_cb), ethi);

	/*
	 * Now, configure DnD
	 */
	ethi_drop_types[0].target = g_strdup_printf (
		"%s-%s", ethi_drop_types[0].target, ethi->dnd_code);
	gtk_drag_dest_set (
		GTK_WIDGET (item->canvas), 0, ethi_drop_types,
		G_N_ELEMENTS (ethi_drop_types), GDK_ACTION_MOVE);
	g_free ((gpointer) ethi_drop_types[0].target);

	/* Drop signals */
	ethi->drag_motion_id = g_signal_connect (
		item->canvas, "drag_motion",
		G_CALLBACK (ethi_drag_motion), ethi);
	ethi->drag_leave_id = g_signal_connect (
		item->canvas, "drag_leave",
		G_CALLBACK (ethi_drag_leave), ethi);
	ethi->drag_drop_id = g_signal_connect (
		item->canvas, "drag_drop",
		G_CALLBACK (ethi_drag_drop), ethi);
	ethi->drag_data_received_id = g_signal_connect (
		item->canvas, "drag_data_received",
		G_CALLBACK (ethi_drag_data_received), ethi);

	/* Drag signals */
	ethi->drag_end_id = g_signal_connect (
		item->canvas, "drag_end",
		G_CALLBACK (ethi_drag_end), ethi);
	ethi->drag_data_get_id = g_signal_connect (
		item->canvas, "drag_data_get",
		G_CALLBACK (ethi_drag_data_get), ethi);

}

static void
ethi_unrealize (GnomeCanvasItem *item)
{
	ETableHeaderItem *ethi = E_TABLE_HEADER_ITEM (item);

	g_clear_pointer (&ethi->font_desc, pango_font_description_free);

	g_signal_handlers_disconnect_by_func (item->canvas, G_CALLBACK (ethi_style_updated_cb), ethi);

	g_signal_handler_disconnect (item->canvas, ethi->drag_motion_id);
	g_signal_handler_disconnect (item->canvas, ethi->drag_leave_id);
	g_signal_handler_disconnect (item->canvas, ethi->drag_drop_id);
	g_signal_handler_disconnect (item->canvas, ethi->drag_data_received_id);

	g_signal_handler_disconnect (item->canvas, ethi->drag_end_id);
	g_signal_handler_disconnect (item->canvas, ethi->drag_data_get_id);

	gtk_drag_dest_unset (GTK_WIDGET (item->canvas));

	if (GNOME_CANVAS_ITEM_CLASS (ethi_parent_class)->unrealize)
		(*GNOME_CANVAS_ITEM_CLASS (ethi_parent_class)->unrealize)(item);
}

static void
ethi_draw (GnomeCanvasItem *item,
           cairo_t *cr,
           gint x,
           gint y,
           gint width,
           gint height)
{
	ETableHeaderItem *ethi = E_TABLE_HEADER_ITEM (item);
	GnomeCanvas *canvas = item->canvas;
	const gint cols = e_table_header_count (ethi->eth);
	gint x1, x2;
	gint col;
	GHashTable *arrows = g_hash_table_new (NULL, NULL);
	GtkStyleContext *context;

	if (ethi->sort_info) {
		gint length;
		gint i;

		length = e_table_sort_info_grouping_get_count (ethi->sort_info);
		for (i = 0; i < length; i++) {
			ETableColumnSpecification *spec;
			GtkSortType sort_type;

			spec = e_table_sort_info_grouping_get_nth (
				ethi->sort_info, i, &sort_type);

			g_hash_table_insert (
				arrows,
				GINT_TO_POINTER (spec->model_col),
				GINT_TO_POINTER (
					(sort_type == GTK_SORT_ASCENDING) ?
					E_TABLE_COL_ARROW_DOWN :
					E_TABLE_COL_ARROW_UP));
		}

		length = e_table_sort_info_sorting_get_count (ethi->sort_info);
		for (i = 0; i < length; i++) {
			ETableColumnSpecification *spec;
			GtkSortType sort_type;

			spec = e_table_sort_info_sorting_get_nth (
				ethi->sort_info, i, &sort_type);

			g_hash_table_insert (
				arrows,
				GINT_TO_POINTER (spec->model_col),
				GINT_TO_POINTER (
					(sort_type == GTK_SORT_ASCENDING) ?
					E_TABLE_COL_ARROW_DOWN :
					E_TABLE_COL_ARROW_UP));
		}
	}

	ethi->width = e_table_header_total_width (ethi->eth) + ethi->group_indent_width;
	x1 = x2 = 0;
	x2 += ethi->group_indent_width;

	context = gtk_widget_get_style_context (GTK_WIDGET (canvas));

	for (col = 0; col < cols; col++, x1 = x2) {
		ETableCol *ecol = e_table_header_get_column (ethi->eth, col);
		gint col_width;

		col_width = ecol->width;

		x2 += col_width;

		if (x1 > (x + width))
			break;

		if (x2 < x)
			continue;

		if (x2 <= x1)
			continue;

		gtk_style_context_save (context);

		if (col + 1 == cols)
			gtk_style_context_add_class (context, "last");

		e_table_header_draw_button (
			cr, ecol, GTK_WIDGET (canvas),
			x1 - x, -y, width, height,
			x2 - x1, ethi->height,
			(ETableColArrow) GPOINTER_TO_INT (g_hash_table_lookup (
			arrows, GINT_TO_POINTER (ecol->spec->model_col))));

		gtk_style_context_restore (context);
	}

	g_hash_table_destroy (arrows);
}

static GnomeCanvasItem *
ethi_point (GnomeCanvasItem *item,
            gdouble x,
            gdouble y,
            gint cx,
            gint cy)
{
	return item;
}

/*
 * is_pointer_on_division:
 *
 * Returns whether @pos is a column header division;  If @the_total is not NULL,
 * then the actual position is returned here.  If @return_ecol is not NULL,
 * then the ETableCol that actually contains this point is returned here
 */
static gboolean
is_pointer_on_division (ETableHeaderItem *ethi,
                        gint pos,
                        gint *the_total,
                        gint *return_col)
{
	const gint cols = e_table_header_count (ethi->eth);
	gint col, total;

	total = 0;
	for (col = 0; col < cols; col++) {
		ETableCol *ecol = e_table_header_get_column (ethi->eth, col);

		if (col == 0)
			total += ethi->group_indent_width;

		total += ecol->width;

		if ((total - TOLERANCE < pos) && (pos < total + TOLERANCE)) {
			if (return_col)
				*return_col = col;
			if (the_total)
				*the_total = total;

			return TRUE;
		}
		if (return_col)
			*return_col = col;

		if (total > pos + TOLERANCE)
			return FALSE;
	}

	return FALSE;
}

#define convert(c,sx,sy,x,y) gnome_canvas_w2c (c,sx,sy,x,y)

static void
set_cursor (ETableHeaderItem *ethi,
            gint pos)
{
	GnomeCanvas *canvas;
	GdkWindow *window;
	gboolean resizable = FALSE;
	gint col;

	canvas = GNOME_CANVAS_ITEM (ethi)->canvas;
	window = gtk_widget_get_window (GTK_WIDGET (canvas));

	/* We might be invoked before we are realized */
	if (window == NULL)
		return;

	if (is_pointer_on_division (ethi, pos, NULL, &col)) {
		gint last_col = ethi->eth->col_count - 1;
		ETableCol *ecol = e_table_header_get_column (ethi->eth, col);

		/* Last column is not resizable */
		if (ecol->spec->resizable && col != last_col) {
			gint c = col + 1;

			/* Column is not resizable if all columns after it
			 * are also not resizable */
			for (; c <= last_col; c++) {
				ETableCol *ecol2;

				ecol2 = e_table_header_get_column (ethi->eth, c);
				if (ecol2->spec->resizable) {
					resizable = TRUE;
					break;
				}
			}
		}
	}

	if (resizable)
		gdk_window_set_cursor (window, ethi->resize_cursor);
	else
		gdk_window_set_cursor (window, NULL);
}

static void
ethi_end_resize (ETableHeaderItem *ethi)
{
	ethi->resize_col = -1;
	ethi->resize_guide = GINT_TO_POINTER (0);

	if (ethi->table)
		e_table_thaw_state_change (ethi->table);
	else if (ethi->tree)
		e_tree_thaw_state_change (ethi->tree);

	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (ethi));
}

static gboolean
ethi_maybe_start_drag (ETableHeaderItem *ethi,
                       GdkEventMotion *event)
{
	GnomeCanvasItem *item;

	if (!ethi->maybe_drag)
		return FALSE;

	if (ethi->eth->col_count < 2) {
		ethi->maybe_drag = FALSE;
		return FALSE;
	}

	item = GNOME_CANVAS_ITEM (ethi);

	return gtk_drag_check_threshold (GTK_WIDGET (item->canvas), ethi->click_x, ethi->click_y, event->x, event->y);
}

static void
ethi_start_drag (ETableHeaderItem *ethi,
                 GdkEvent *event)
{
	GtkWidget *widget;
	GtkTargetList *list;
	GdkDragContext *context;
	ETableCol *ecol;
	gint col_width;
	cairo_surface_t *s;
	cairo_t *cr;

	gint group_indent = 0;
	GHashTable *arrows;

	GtkTargetEntry  ethi_drag_types[] = {
		{ (gchar *) TARGET_ETABLE_COL_TYPE, 0, TARGET_ETABLE_COL_HEADER },
	};

	widget = GTK_WIDGET (GNOME_CANVAS_ITEM (ethi)->canvas);
	ethi->drag_col = ethi_find_col_by_x (ethi, event->motion.x);

	if (ethi->drag_col == -1)
		return;

	arrows = g_hash_table_new (NULL, NULL);

	if (ethi->sort_info) {
		gint length = e_table_sort_info_grouping_get_count (ethi->sort_info);
		gint i;
		for (i = 0; i < length; i++) {
			ETableColumnSpecification *spec;
			GtkSortType sort_type;

			group_indent++;

			spec = e_table_sort_info_grouping_get_nth (
				ethi->sort_info, i, &sort_type);

			g_hash_table_insert (
				arrows,
				GINT_TO_POINTER (spec->model_col),
				GINT_TO_POINTER (
					(sort_type == GTK_SORT_ASCENDING) ?
					E_TABLE_COL_ARROW_DOWN :
					E_TABLE_COL_ARROW_UP));
		}
		length = e_table_sort_info_sorting_get_count (ethi->sort_info);
		for (i = 0; i < length; i++) {
			ETableColumnSpecification *spec;
			GtkSortType sort_type;

			spec = e_table_sort_info_sorting_get_nth (
				ethi->sort_info, i, &sort_type);

			g_hash_table_insert (
				arrows,
				GINT_TO_POINTER (spec->model_col),
				GINT_TO_POINTER (
					(sort_type == GTK_SORT_ASCENDING) ?
					E_TABLE_COL_ARROW_DOWN :
					E_TABLE_COL_ARROW_UP));
		}
	}

	ethi_drag_types[0].target = g_strdup_printf (
		"%s-%s", ethi_drag_types[0].target, ethi->dnd_code);
	list = gtk_target_list_new (
		ethi_drag_types, G_N_ELEMENTS (ethi_drag_types));
	context = gtk_drag_begin (widget, list, GDK_ACTION_MOVE, 1, event);
	g_free ((gpointer) ethi_drag_types[0].target);

	ecol = e_table_header_get_column (ethi->eth, ethi->drag_col);
	col_width = ecol->width;
	s = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, col_width, ethi->height);
	cr = cairo_create (s);

	e_table_header_draw_button (
		cr, ecol,
		widget, 0, 0,
		col_width, ethi->height,
		col_width, ethi->height,
		(ETableColArrow) GPOINTER_TO_INT (g_hash_table_lookup (
			arrows, GINT_TO_POINTER (ecol->spec->model_col))));
	gtk_drag_set_icon_surface (context, s);
	cairo_surface_destroy (s);

	ethi->maybe_drag = FALSE;
	g_hash_table_destroy (arrows);
}

typedef struct {
	ETableHeaderItem *ethi;
	gint col;
} EthiHeaderInfo;

static void
ethi_popup_sort_ascending (GtkWidget *widget,
                           EthiHeaderInfo *info)
{
	ETableColumnSpecification *col_spec = NULL;
	ETableCol *col;
	gint length;
	gint i;
	gint found = FALSE;
	ETableHeaderItem *ethi = info->ethi;

	col = e_table_header_get_column (ethi->eth, info->col);
	if (col->spec->sortable)
		col_spec = col->spec;

	length = e_table_sort_info_grouping_get_count (ethi->sort_info);
	for (i = 0; i < length; i++) {
		ETableColumnSpecification *spec;

		spec = e_table_sort_info_grouping_get_nth (
			ethi->sort_info, i, NULL);

		if (e_table_column_specification_equal (col_spec, spec)) {
			e_table_sort_info_grouping_set_nth (
				ethi->sort_info, i, spec,
				GTK_SORT_ASCENDING);
			return;
		}
	}

	length = e_table_sort_info_sorting_get_count (ethi->sort_info);
	for (i = 0; i < length; i++) {
		ETableColumnSpecification *spec;

		spec = e_table_sort_info_sorting_get_nth (
			ethi->sort_info, i, NULL);

		if (col_spec == NULL ||
		    e_table_column_specification_equal (col_spec, spec)) {
			e_table_sort_info_sorting_set_nth (
				ethi->sort_info, i, spec,
				GTK_SORT_ASCENDING);
			found = TRUE;
			if (col_spec != NULL)
				return;
		}
	}

	if (!found) {
		length = e_table_sort_info_sorting_get_count (ethi->sort_info);
		if (length == 0)
			length++;

		e_table_sort_info_sorting_set_nth (
			ethi->sort_info, length - 1,
			col_spec, GTK_SORT_ASCENDING);
	}
}

static void
ethi_popup_sort_descending (GtkWidget *widget,
                            EthiHeaderInfo *info)
{
	ETableColumnSpecification *col_spec = NULL;
	ETableCol *col;
	gint length;
	gint i;
	gint found = FALSE;
	ETableHeaderItem *ethi = info->ethi;

	col = e_table_header_get_column (ethi->eth, info->col);
	if (col->spec->sortable)
		col_spec = col->spec;

	length = e_table_sort_info_grouping_get_count (ethi->sort_info);
	for (i = 0; i < length; i++) {
		ETableColumnSpecification *spec;
		GtkSortType sort_type;

		spec = e_table_sort_info_grouping_get_nth (
			ethi->sort_info, i, &sort_type);

		if (e_table_column_specification_equal (col_spec, spec)) {
			e_table_sort_info_grouping_set_nth (
				ethi->sort_info, i, spec,
				GTK_SORT_DESCENDING);
			return;
		}
	}

	length = e_table_sort_info_sorting_get_count (ethi->sort_info);
	for (i = 0; i < length; i++) {
		ETableColumnSpecification *spec;

		spec = e_table_sort_info_sorting_get_nth (
			ethi->sort_info, i, NULL);

		if (col_spec == NULL ||
		    e_table_column_specification_equal (col_spec, spec)) {
			e_table_sort_info_sorting_set_nth (
				ethi->sort_info, i, spec,
				GTK_SORT_DESCENDING);
			found = TRUE;
			if (col_spec != NULL)
				break;
		}
	}

	if (!found) {
		length = e_table_sort_info_sorting_get_count (ethi->sort_info);
		if (length == 0)
			length++;

		e_table_sort_info_sorting_set_nth (
			ethi->sort_info, length - 1,
			col_spec, GTK_SORT_DESCENDING);
	}
}

static void
ethi_popup_unsort (GtkWidget *widget,
                   EthiHeaderInfo *info)
{
	ETableHeaderItem *ethi = info->ethi;

	e_table_sort_info_grouping_truncate (ethi->sort_info, 0);
	e_table_sort_info_sorting_truncate (ethi->sort_info, 0);
}

static void
ethi_popup_group_field (GtkWidget *widget,
                        EthiHeaderInfo *info)
{
	ETableCol *col;

	col = e_table_header_get_column (info->ethi->eth, info->col);

	e_table_sort_info_grouping_set_nth (
		info->ethi->sort_info, 0,
		col->spec, GTK_SORT_ASCENDING);
	e_table_sort_info_grouping_truncate (info->ethi->sort_info, 1);
}

static void
ethi_popup_group_box (GtkWidget *widget,
                      EthiHeaderInfo *info)
{
}

static void
ethi_popup_remove_column (GtkWidget *widget,
                          EthiHeaderInfo *info)
{
	e_table_header_remove (info->ethi->eth, info->col);
}

static void
ethi_popup_field_chooser (GtkWidget *widget,
                          EthiHeaderInfo *info)
{
	GtkWidget *etfcd = info->ethi->etfcd.widget;
	GtkWidget *toplevel;

	if (etfcd) {
		gtk_window_present (GTK_WINDOW (etfcd));

		return;
	}

	info->ethi->etfcd.widget = e_table_field_chooser_dialog_new ();
	etfcd = info->ethi->etfcd.widget;

	toplevel = gtk_widget_get_toplevel (widget);
	if (GTK_IS_WINDOW (toplevel))
		gtk_window_set_transient_for (GTK_WINDOW (etfcd), GTK_WINDOW (toplevel));

	g_object_add_weak_pointer (G_OBJECT (etfcd), &info->ethi->etfcd.pointer);

	g_object_set (
		info->ethi->etfcd.widget,
		"full_header", info->ethi->full_header,
		"header", info->ethi->eth,
		"dnd_code", info->ethi->dnd_code,
		NULL);

	gtk_widget_show (etfcd);
}

static void
ethi_popup_alignment (GtkWidget *widget,
                      EthiHeaderInfo *info)
{
}

static void
ethi_popup_best_fit (GtkWidget *widget,
                     EthiHeaderInfo *info)
{
	ETableHeaderItem *ethi = info->ethi;
	gint width;

	g_signal_emit_by_name (
		ethi->eth,
		"request_width",
		info->col, &width);
	/* Add 10 to stop it from "..."ing */
	e_table_header_set_size (ethi->eth, info->col, width + 10);

	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (ethi));

}

static void
ethi_popup_format_columns (GtkWidget *widget,
                           EthiHeaderInfo *info)
{
}

static void
config_destroyed (gpointer data,
                  GObject *where_object_was)
{
	ETableHeaderItem *ethi = data;
	ethi->config = NULL;
}

static void
apply_changes (ETableConfig *config,
               ETableHeaderItem *ethi)
{
	ETableState *state;

	state = e_table_state_duplicate (config->state);

	if (ethi->table != NULL)
		e_table_set_state_object (ethi->table, state);
	if (ethi->tree != NULL)
		e_tree_set_state_object (ethi->tree, state);

	g_object_unref (state);

	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (config->dialog_toplevel),
		GTK_RESPONSE_APPLY, FALSE);
}

void
e_table_header_item_customize_view (ETableHeaderItem *ethi)
{
	ETableState *state;
	ETableSpecification *spec;
	GtkWidget *widget = NULL;

	g_return_if_fail (E_IS_TABLE_HEADER_ITEM (ethi));

	if (ethi->table)
		widget = GTK_WIDGET (ethi->table);
	else if (ethi->tree)
		widget = GTK_WIDGET (ethi->tree);

	if (ethi->config) {
		e_table_config_raise (E_TABLE_CONFIG (ethi->config));
	} else {
		if (ethi->table) {
			state = e_table_get_state_object (ethi->table);
			spec = ethi->table->spec;
		} else if (ethi->tree) {
			state = e_tree_get_state_object (ethi->tree);
			spec = e_tree_get_spec (ethi->tree);
		} else
			return;

		ethi->config = e_table_config_new (
				_("Customize Current View"),
				spec, state, GTK_WINDOW (gtk_widget_get_toplevel (widget)));
		g_object_weak_ref (
			G_OBJECT (ethi->config),
			config_destroyed, ethi);
		g_signal_connect (
			ethi->config, "changed",
			G_CALLBACK (apply_changes), ethi);
	}
}

static void
ethi_popup_customize_view (GtkWidget *widget,
                           EthiHeaderInfo *info)
{
	ETableHeaderItem *ethi = info->ethi;

	e_table_header_item_customize_view (ethi);
}

static void
free_popup_info (GtkWidget *w,
                 EthiHeaderInfo *info)
{
	g_free (info);
}

/* Bit 1 is always disabled. */
/* Bit 2 is disabled if not "sortable". */
/* Bit 4 is disabled if we don't have a pointer to our table object. */
static EPopupMenu ethi_context_menu[] = {
	E_POPUP_ITEM (
		N_("Sort _Ascending"),
		G_CALLBACK (ethi_popup_sort_ascending), 2),
	E_POPUP_ITEM (
		N_("Sort _Descending"),
		G_CALLBACK (ethi_popup_sort_descending), 2),
	E_POPUP_ITEM (
		N_("_Reset sort"), G_CALLBACK (ethi_popup_unsort), 0),
	E_POPUP_SEPARATOR,
	E_POPUP_ITEM (
		N_("Group By This _Field"),
		G_CALLBACK (ethi_popup_group_field), 16),
	E_POPUP_ITEM (
		N_("Group By _Box"),
		G_CALLBACK (ethi_popup_group_box), 128),
	E_POPUP_SEPARATOR,
	E_POPUP_ITEM (
		N_("Remove This _Column"),
		G_CALLBACK (ethi_popup_remove_column), 8),
	E_POPUP_ITEM (
		N_("Add a C_olumn…"),
		G_CALLBACK (ethi_popup_field_chooser), 0),
	E_POPUP_SEPARATOR,
	E_POPUP_ITEM (
		N_("A_lignment"),
		G_CALLBACK (ethi_popup_alignment), 128),
	E_POPUP_ITEM (
		N_("B_est Fit"),
		G_CALLBACK (ethi_popup_best_fit), 2),
	E_POPUP_ITEM (
		N_("Format Column_s…"),
		G_CALLBACK (ethi_popup_format_columns), 128),
	E_POPUP_SEPARATOR,
	E_POPUP_ITEM (
		N_("Custo_mize Current View…"),
		G_CALLBACK (ethi_popup_customize_view), 4),
	E_POPUP_TERMINATOR
};

static void
sort_by_id (GtkWidget *menu_item,
            ETableHeaderItem *ethi)
{
	ETableCol *ecol;
	gboolean clearfirst;
	gint col;

	col = GPOINTER_TO_INT (g_object_get_data (
		G_OBJECT (menu_item), "col-number"));
	ecol = e_table_header_get_column (ethi->full_header, col);
	clearfirst = e_table_sort_info_sorting_get_count (ethi->sort_info) > 1;

	if (!clearfirst && ecol &&
		e_table_sort_info_sorting_get_count (ethi->sort_info) == 1) {
		ETableColumnSpecification *spec;

		spec = e_table_sort_info_sorting_get_nth (
			ethi->sort_info, 0, NULL);
		clearfirst = ecol->spec->sortable && ecol->spec != spec;
	}

	if (clearfirst)
		e_table_sort_info_sorting_truncate (ethi->sort_info, 0);

	ethi_change_sort_state (ethi, ecol, E_TABLE_HEADER_ITEM_SORT_FLAG_NONE);
}

static void
popup_custom (GtkWidget *menu_item,
              EthiHeaderInfo *info)
{
	ethi_popup_customize_view (menu_item, info);
}

static void
ethi_header_context_menu (ETableHeaderItem *ethi,
                          GdkEvent *button_event)
{
	EthiHeaderInfo *info = g_new (EthiHeaderInfo, 1);
	GtkMenu *popup;
	gint ncol, sort_count, sort_col;
	GtkWidget *menu_item, *sub_menu;
	gboolean ascending = TRUE;
	gdouble event_x_win = 0;
	gdouble event_y_win = 0;

	d (g_print ("ethi_header_context_menu: \n"));

	gdk_event_get_coords (button_event, &event_x_win, &event_y_win);

	info->ethi = ethi;
	info->col = ethi_find_col_by_x (ethi, event_x_win);

	popup = e_popup_menu_create_with_domain (
		ethi_context_menu,
		1 +
		((ethi->table || ethi->tree) ? 0 : 4) +
		((e_table_header_count (ethi->eth) > 1) ? 0 : 8),
		((e_table_sort_info_get_can_group (ethi->sort_info)) ? 0 : 16) +
		128, info, GETTEXT_PACKAGE);

	menu_item = gtk_menu_item_new_with_mnemonic (_("_Sort By"));
	gtk_widget_show (menu_item);
	sub_menu = gtk_menu_new ();
	gtk_widget_show (sub_menu);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), sub_menu);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (popup), menu_item);

	sort_count = e_table_sort_info_sorting_get_count (ethi->sort_info);

	if (sort_count > 1 || sort_count < 1)
		sort_col = -1; /* Custom sorting */
	else {
		ETableColumnSpecification *spec;
		GtkSortType sort_type;

		spec = e_table_sort_info_sorting_get_nth (
			ethi->sort_info, 0, &sort_type);

		sort_col = spec->model_col;
		ascending = (sort_type == GTK_SORT_ASCENDING);
	}

	/* Custom */
	menu_item = gtk_check_menu_item_new_with_mnemonic (_("_Custom"));
	gtk_widget_show (menu_item);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (sub_menu), menu_item);
	if (sort_col == -1)
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item), TRUE);
	gtk_check_menu_item_set_draw_as_radio (GTK_CHECK_MENU_ITEM (menu_item), TRUE);
	g_signal_connect (
		menu_item, "activate",
		G_CALLBACK (popup_custom), info);

	/* Show a seperator */
	menu_item = gtk_separator_menu_item_new ();
	gtk_widget_show (menu_item);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (sub_menu), menu_item);
	/* Headers */
	for (ncol = 0; ncol < ethi->full_header->col_count; ncol++)
	{
		gchar *text = NULL;

		if (!ethi->full_header->columns[ncol]->spec->sortable ||
		    ethi->full_header->columns[ncol]->spec->disabled)
			continue;

		if (ncol == sort_col) {
			text = g_strdup_printf (
				"%s (%s)",
				ethi->full_header->columns[ncol]->text,
				ascending ? _("Ascending"):_("Descending"));
			menu_item = gtk_check_menu_item_new_with_label (text);
			g_free (text);
		} else
			menu_item = gtk_check_menu_item_new_with_label (
				ethi->full_header->columns[ncol]->text);

		gtk_widget_show (menu_item);
		gtk_menu_shell_prepend (GTK_MENU_SHELL (sub_menu), menu_item);

		if (ncol == sort_col)
			gtk_check_menu_item_set_active (
				GTK_CHECK_MENU_ITEM (menu_item), TRUE);
		gtk_check_menu_item_set_draw_as_radio (
			GTK_CHECK_MENU_ITEM (menu_item), TRUE);
		g_object_set_data (
			G_OBJECT (menu_item), "col-number",
			GINT_TO_POINTER (ncol));
		g_signal_connect (
			menu_item, "activate",
			G_CALLBACK (sort_by_id), ethi);
	}

	g_signal_connect (
		popup, "selection-done",
		G_CALLBACK (free_popup_info), info);

	gtk_menu_attach_to_widget (GTK_MENU (popup),
				   GTK_WIDGET (ethi->parent.canvas),
				   NULL);
	g_signal_connect (popup, "deactivate", G_CALLBACK (gtk_menu_detach), NULL);
	gtk_menu_popup_at_pointer (popup, button_event);
}

static void
ethi_button_pressed (ETableHeaderItem *ethi,
                     GdkEvent *button_event)
{
	g_signal_emit (ethi, ethi_signals[BUTTON_PRESSED], 0, button_event);
}

void
ethi_change_sort_state (ETableHeaderItem *ethi,
                        ETableCol *col,
			ETableHeaderItemSortFlag flag)
{
	ETableColumnSpecification *col_spec = NULL;
	gint length;
	gint i;
	gboolean found = FALSE;

	if (col == NULL)
		return;

	if (col->spec->sortable)
		col_spec = col->spec;

	length = e_table_sort_info_grouping_get_count (ethi->sort_info);
	for (i = 0; i < length; i++) {
		ETableColumnSpecification *spec;
		GtkSortType sort_type;

		spec = e_table_sort_info_grouping_get_nth (
			ethi->sort_info, i, &sort_type);

		/* Invert the sort type. */
		if (sort_type == GTK_SORT_ASCENDING)
			sort_type = GTK_SORT_DESCENDING;
		else
			sort_type = GTK_SORT_ASCENDING;

		if (col_spec == NULL ||
		    e_table_column_specification_equal (col_spec, spec)) {
			e_table_sort_info_grouping_set_nth (
				ethi->sort_info, i, spec, sort_type);
			found = TRUE;
			if (col_spec != NULL)
				break;
		}
	}

	if (!found) {
		length = e_table_sort_info_sorting_get_count (ethi->sort_info);
		for (i = 0; i < length; i++) {
			ETableColumnSpecification *spec;
			GtkSortType sort_type;

			spec = e_table_sort_info_sorting_get_nth (
				ethi->sort_info, i, &sort_type);

			if (col_spec == NULL ||
			    e_table_column_specification_equal (col_spec, spec)) {
				if (sort_type == GTK_SORT_DESCENDING && col_spec != NULL) {
					/*
					 * This means the user has clicked twice
					 * already, lets kill sorting of this column now.
					 */
					e_table_sort_info_sorting_remove (
						ethi->sort_info, i);
					length--;
					i--;
				} else {
					/* Invert the sort type. */
					if (sort_type == GTK_SORT_ASCENDING)
						sort_type = GTK_SORT_DESCENDING;
					else
						sort_type = GTK_SORT_ASCENDING;

					e_table_sort_info_sorting_set_nth (
						ethi->sort_info, i, spec, sort_type);
				}
				found = TRUE;
				if (col_spec != NULL)
					break;
			}
		}
	}

	if (!found && col_spec != NULL) {
		if (flag == E_TABLE_HEADER_ITEM_SORT_FLAG_NONE) {
			e_table_sort_info_sorting_truncate (ethi->sort_info, 0);
			e_table_sort_info_sorting_set_nth (
				ethi->sort_info, 0,
				col_spec, GTK_SORT_ASCENDING);
		} else {
			guint index = 0;

			if (flag == E_TABLE_HEADER_ITEM_SORT_FLAG_ADD_AS_LAST)
				index = e_table_sort_info_sorting_get_count (ethi->sort_info);

			e_table_sort_info_sorting_insert (
				ethi->sort_info, index,
				col_spec, GTK_SORT_ASCENDING);
		}
	}
}

/*
 * Handles the events on the ETableHeaderItem, particularly it handles resizing
 */
static gint
ethi_event (GnomeCanvasItem *item,
            GdkEvent *event)
{
	ETableHeaderItem *ethi = E_TABLE_HEADER_ITEM (item);
	GnomeCanvas *canvas = item->canvas;
	GdkWindow *window;
	const gboolean resizing = ETHI_RESIZING (ethi);
	gint x, y, start, col;
	gint was_maybe_drag = 0;
	GdkModifierType event_state = 0;
	guint event_button = 0;
	guint event_keyval = 0;
	gdouble event_x_win = 0;
	gdouble event_y_win = 0;
	guint32 event_time;
	ETableHeaderItemSortFlag sort_flag = E_TABLE_HEADER_ITEM_SORT_FLAG_NONE;

	/* Don't fetch the device here.  GnomeCanvas frequently emits
	 * synthesized events, and calling gdk_event_get_device() on them
	 * will trigger a runtime warning.  Fetch the device where needed. */
	gdk_event_get_button (event, &event_button);
	gdk_event_get_coords (event, &event_x_win, &event_y_win);
	gdk_event_get_keyval (event, &event_keyval);
	gdk_event_get_state (event, &event_state);
	event_time = gdk_event_get_time (event);

	if ((event_state & GDK_CONTROL_MASK) != 0) {
		if ((event_state & GDK_SHIFT_MASK) != 0)
			sort_flag = E_TABLE_HEADER_ITEM_SORT_FLAG_ADD_AS_FIRST;
		else
			sort_flag = E_TABLE_HEADER_ITEM_SORT_FLAG_ADD_AS_LAST;
	}

	switch (event->type) {
	case GDK_ENTER_NOTIFY:
		convert (canvas, event_x_win, event_y_win, &x, &y);
		set_cursor (ethi, x);
		break;

	case GDK_LEAVE_NOTIFY:
		window = gtk_widget_get_window (GTK_WIDGET (canvas));
		gdk_window_set_cursor (window, NULL);
		break;

	case GDK_MOTION_NOTIFY:

		convert (canvas, event_x_win, event_y_win, &x, &y);
		if (resizing) {
			gint new_width;

			if (ethi->resize_guide == NULL) {
				GdkDevice *event_device;

				/* Quick hack until I actually bind the views */
				ethi->resize_guide = GINT_TO_POINTER (1);

				event_device = gdk_event_get_device (event);

				g_warn_if_fail (gnome_canvas_item_grab (
					item,
					GDK_POINTER_MOTION_MASK |
					GDK_BUTTON_RELEASE_MASK,
					ethi->resize_cursor,
					event_device,
					event_time) == GDK_GRAB_SUCCESS);
			}

			new_width = x - ethi->resize_start_pos;

			e_table_header_set_size (ethi->eth, ethi->resize_col, new_width);

			gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (ethi));
		} else if (ethi_maybe_start_drag (ethi, &event->motion)) {
			ethi_start_drag (ethi, event);
		} else
			set_cursor (ethi, x);
		break;

	case GDK_BUTTON_PRESS:
		/* Skip also when the Shift is down without Control, which can be a misclick when doing multiselect */
		if (event_button > 3 || ((event_state & GDK_SHIFT_MASK) != 0 &&
		    (event_state & GDK_CONTROL_MASK) == 0))
			return FALSE;

		convert (canvas, event_x_win, event_y_win, &x, &y);

		if (is_pointer_on_division (ethi, x, &start, &col) &&
		    event_button == 1) {
			ETableCol *ecol;

				/*
				 * Record the important bits.
				 *
				 * By setting resize_pos to a non -1 value,
				 * we know that we are being resized (used in the
				 * other event handlers).
				 */
			ecol = e_table_header_get_column (ethi->eth, col);

			if (!ecol->spec->resizable)
				break;
			ethi->resize_col = col;
			ethi->resize_start_pos = start - ecol->width;
			ethi->resize_min_width = ecol->min_width;

			if (ethi->table)
				e_table_freeze_state_change (ethi->table);
			else if (ethi->tree)
				e_tree_freeze_state_change (ethi->tree);
		} else {
			if (event_button == 1) {
				ethi->click_x = event_x_win;
				ethi->click_y = event_y_win;
				ethi->maybe_drag = TRUE;
				col = -1;
				is_pointer_on_division (ethi, x, &start, &col);
				if (col != -1)
					ethi->selected_col = col;
				if (gtk_widget_get_can_focus (GTK_WIDGET (item->canvas)))
					e_canvas_item_grab_focus (item, TRUE);
			} else if (event_button == 3) {
				ethi_header_context_menu (ethi, event);
			} else
				ethi_button_pressed (ethi, event);
		}
		break;

	case GDK_2BUTTON_PRESS:
		if (!resizing)
			break;

		if (event_button != 1)
			break;
		else {
			gint width = 0;
			g_signal_emit_by_name (
				ethi->eth,
				"request_width",
				(gint) ethi->resize_col, &width);
			/* Add 10 to stop it from "..."ing */
			e_table_header_set_size (ethi->eth, ethi->resize_col, width + 10);

			gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (ethi));
			ethi->maybe_drag = FALSE;
		}
		break;

	case GDK_BUTTON_RELEASE: {
		gboolean needs_ungrab = FALSE;

		was_maybe_drag = ethi->maybe_drag;

		ethi->maybe_drag = FALSE;

		if (ethi->resize_col != -1) {
			needs_ungrab = (ethi->resize_guide != NULL);
			ethi_end_resize (ethi);
		} else if (was_maybe_drag && ethi->sort_info) {
			gboolean header_click_can_sort = TRUE;

			g_signal_emit (ethi, ethi_signals[HEADER_CLICK_CAN_SORT], 0, &header_click_can_sort);

			if (header_click_can_sort) {
				ETableCol *ecol;

				col = ethi_find_col_by_x (ethi, event_x_win);
				ecol = e_table_header_get_column (ethi->eth, col);
				ethi_change_sort_state (ethi, ecol, sort_flag);
			}
		}

		if (needs_ungrab)
			gnome_canvas_item_ungrab (item, event_time);

		break;
	}
	case GDK_KEY_PRESS:
		if ((event_keyval == GDK_KEY_F10) && (event_state & GDK_SHIFT_MASK)) {
			EthiHeaderInfo *info = g_new (EthiHeaderInfo, 1);
			ETableCol *ecol;
			GtkMenu *popup;

			info->ethi = ethi;
			info->col = ethi->selected_col;
			ecol = e_table_header_get_column (ethi->eth, info->col);

			popup = e_popup_menu_create_with_domain (
				ethi_context_menu,
				1 +
				(ecol->spec->sortable ? 0 : 2) +
				((ethi->table || ethi->tree) ? 0 : 4) +
				((e_table_header_count (ethi->eth) > 1) ? 0 : 8),
				((e_table_sort_info_get_can_group (
					ethi->sort_info)) ? 0 : 16) +
				128, info, GETTEXT_PACKAGE);
			g_object_ref_sink (popup);
			g_signal_connect (
				popup, "selection-done",
				G_CALLBACK (free_popup_info), info);
			gtk_menu_attach_to_widget (GTK_MENU (popup),
						   GTK_WIDGET (canvas),
						   NULL);
			g_signal_connect (popup, "deactivate", G_CALLBACK (gtk_menu_detach), NULL);
			gtk_menu_popup_at_pointer (popup, event);
		} else if (event_keyval == GDK_KEY_space) {
			ETableCol *ecol;

			ecol = e_table_header_get_column (ethi->eth, ethi->selected_col);
			ethi_change_sort_state (ethi, ecol, sort_flag);
		} else if ((event_keyval == GDK_KEY_Right) ||
				(event_keyval == GDK_KEY_KP_Right)) {
			ETableCol *ecol;

			if ((ethi->selected_col < 0) ||
			    (ethi->selected_col >= ethi->eth->col_count - 1))
				ethi->selected_col = 0;
			else
				ethi->selected_col++;
			ecol = e_table_header_get_column (ethi->eth, ethi->selected_col);
			ethi_change_sort_state (ethi, ecol, sort_flag);
		} else if ((event_keyval == GDK_KEY_Left) ||
			   (event_keyval == GDK_KEY_KP_Left)) {
			ETableCol *ecol;

			if ((ethi->selected_col <= 0) ||
			    (ethi->selected_col >= ethi->eth->col_count))
				ethi->selected_col = ethi->eth->col_count - 1;
			else
				ethi->selected_col--;
			ecol = e_table_header_get_column (ethi->eth, ethi->selected_col);
			ethi_change_sort_state (ethi, ecol, sort_flag);
		}
		break;

	default:
		return FALSE;
	}
	return TRUE;
}

static void
ethi_class_init (ETableHeaderItemClass *class)
{
	GnomeCanvasItemClass *item_class = GNOME_CANVAS_ITEM_CLASS (class);
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->dispose = ethi_dispose;
	object_class->set_property = ethi_set_property;
	object_class->get_property = ethi_get_property;

	item_class->update = ethi_update;
	item_class->realize = ethi_realize;
	item_class->unrealize = ethi_unrealize;
	item_class->draw = ethi_draw;
	item_class->point = ethi_point;
	item_class->event = ethi_event;

	g_object_class_install_property (
		object_class,
		PROP_DND_CODE,
		g_param_spec_string (
			"dnd_code",
			"DnD code",
			NULL,
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_TABLE_FONT_DESC,
		g_param_spec_boxed (
			"font-desc",
			"Font Description",
			NULL,
			PANGO_TYPE_FONT_DESCRIPTION,
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		object_class,
		PROP_FULL_HEADER,
		g_param_spec_object (
			"full_header",
			"Full Header",
			NULL,
			E_TYPE_TABLE_HEADER,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_TABLE_HEADER,
		g_param_spec_object (
			"ETableHeader",
			"Header",
			NULL,
			E_TYPE_TABLE_HEADER,
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		object_class,
		PROP_SORT_INFO,
		g_param_spec_object (
			"sort_info",
			"Sort Info",
			NULL,
			E_TYPE_TABLE_SORT_INFO,
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		object_class,
		PROP_TABLE,
		g_param_spec_object (
			"table",
			"Table",
			NULL,
			E_TYPE_TABLE,
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		object_class,
		PROP_TREE,
		g_param_spec_object (
			"tree",
			"Tree",
			NULL,
			E_TYPE_TREE,
			G_PARAM_WRITABLE));

	ethi_signals[BUTTON_PRESSED] = g_signal_new (
		"button_pressed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableHeaderItemClass, button_pressed),
		NULL, NULL,
		g_cclosure_marshal_VOID__BOXED,
		G_TYPE_NONE, 1,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	ethi_signals[HEADER_CLICK_CAN_SORT] = g_signal_new (
		"header-click-can-sort",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		/* G_STRUCT_OFFSET (ETableHeaderItemClass, header_click_can_sort) */ 0,
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);
}

static void
ethi_init (ETableHeaderItem *ethi)
{
	GnomeCanvasItem *item = GNOME_CANVAS_ITEM (ethi);

	ethi->resize_cursor = gdk_cursor_new_from_name (gdk_display_get_default (), "ew-resize");

	ethi->resize_col = -1;

	item->x1 = 0;
	item->y1 = 0;
	item->x2 = 0;
	item->y2 = 0;

	ethi->drag_col = -1;
	ethi->drag_mark = -1;

	ethi->sort_info = NULL;

	ethi->sort_info_changed_id = 0;
	ethi->group_info_changed_id = 0;

	ethi->group_indent_width = 0;
	ethi->table = NULL;
	ethi->tree = NULL;

	ethi->selected_col = 0;
}

