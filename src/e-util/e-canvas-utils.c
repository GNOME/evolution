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

#include <libedataserver/libedataserver.h>
#include "e-canvas-utils.h"

void
e_canvas_item_move_absolute (GnomeCanvasItem *item,
                             gdouble dx,
                             gdouble dy)
{
	cairo_matrix_t translate;

	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));

	cairo_matrix_init_translate (&translate, dx, dy);

	gnome_canvas_item_set_matrix (item, &translate);
}

static double
compute_offset (gint top,
                gint bottom,
                gint page_top,
                gint page_bottom)
{
	gint size = bottom - top;
	gint offset = 0;

	if (top <= page_top && bottom >= page_bottom)
		return 0;

	if (bottom > page_bottom)
		offset = (bottom - page_bottom);
	if (top < page_top + offset)
		offset = (top - page_top);

	if (top <= page_top + offset && bottom >= page_bottom + offset)
		return offset;

	if (top < page_top + size * 3 / 2 + offset)
		offset = top - (page_top + size * 3 / 2);
	if (bottom > page_bottom - size * 3 / 2 + offset)
		offset = bottom - (page_bottom - size * 3 / 2);
	if (top < page_top + size * 3 / 2 + offset)
		offset = top - ((page_top + page_bottom - (bottom - top)) / 2);

	return offset;
}

static void
e_canvas_show_area (GnomeCanvas *canvas,
                    gdouble x1,
                    gdouble y1,
                    gdouble x2,
                    gdouble y2)
{
	GtkAdjustment *h, *v;
	gint dx = 0, dy = 0;
	gdouble page_size;
	gdouble lower;
	gdouble upper;
	gdouble value;

	g_return_if_fail (canvas != NULL);
	g_return_if_fail (GNOME_IS_CANVAS (canvas));

	h = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (canvas));
	page_size = gtk_adjustment_get_page_size (h);
	lower = gtk_adjustment_get_lower (h);
	upper = gtk_adjustment_get_upper (h);
	value = gtk_adjustment_get_value (h);
	dx = compute_offset (x1, x2, value, value + page_size);
	if (dx)
		gtk_adjustment_set_value (h, CLAMP (value + dx, lower, upper - page_size));

	v = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (canvas));
	page_size = gtk_adjustment_get_page_size (v);
	lower = gtk_adjustment_get_lower (v);
	upper = gtk_adjustment_get_upper (v);
	value = gtk_adjustment_get_value (v);
	dy = compute_offset (y1, y2, value, value + page_size);
	if (dy)
		gtk_adjustment_set_value (v, CLAMP (value + dy, lower, upper - page_size));
}

void
e_canvas_item_show_area (GnomeCanvasItem *item,
                         gdouble x1,
                         gdouble y1,
                         gdouble x2,
                         gdouble y2)
{
	g_return_if_fail (item != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));

	gnome_canvas_item_i2w (item, &x1, &y1);
	gnome_canvas_item_i2w (item, &x2, &y2);

	e_canvas_show_area (item->canvas, x1, y1, x2, y2);
}

static gboolean
e_canvas_area_shown (GnomeCanvas *canvas,
                     gdouble x1,
                     gdouble y1,
                     gdouble x2,
                     gdouble y2)
{
	GtkAdjustment *h, *v;
	gint dx = 0, dy = 0;
	gdouble page_size;
	gdouble lower;
	gdouble upper;
	gdouble value;

	g_return_val_if_fail (canvas != NULL, FALSE);
	g_return_val_if_fail (GNOME_IS_CANVAS (canvas), FALSE);

	h = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (canvas));
	page_size = gtk_adjustment_get_page_size (h);
	lower = gtk_adjustment_get_lower (h);
	upper = gtk_adjustment_get_upper (h);
	value = gtk_adjustment_get_value (h);
	dx = compute_offset (x1, x2, value, value + page_size);
	if (CLAMP (value + dx, lower, upper - page_size) - value != 0)
		return FALSE;

	v = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (canvas));
	page_size = gtk_adjustment_get_page_size (v);
	lower = gtk_adjustment_get_lower (v);
	upper = gtk_adjustment_get_upper (v);
	value = gtk_adjustment_get_value (v);
	dy = compute_offset (y1, y2, value, value + page_size);
	if (CLAMP (value + dy, lower, upper - page_size) - value != 0)
		return FALSE;
	return TRUE;
}

gboolean
e_canvas_item_area_shown (GnomeCanvasItem *item,
                          gdouble x1,
                          gdouble y1,
                          gdouble x2,
                          gdouble y2)
{
	g_return_val_if_fail (item != NULL, FALSE);
	g_return_val_if_fail (GNOME_IS_CANVAS_ITEM (item), FALSE);

	gnome_canvas_item_i2w (item, &x1, &y1);
	gnome_canvas_item_i2w (item, &x2, &y2);

	return e_canvas_area_shown (item->canvas, x1, y1, x2, y2);
}

typedef struct {
	gdouble x1;
	gdouble y1;
	gdouble x2;
	gdouble y2;
	GnomeCanvas *canvas;
} DoubsAndCanvas;

static void
doubs_and_canvas_free (gpointer ptr)
{
	DoubsAndCanvas *dac = ptr;

	if (dac) {
		g_object_unref (dac->canvas);
		g_free (dac);
	}
}

static gboolean
show_area_timeout (gpointer data)
{
	DoubsAndCanvas *dac = data;

	e_canvas_show_area (dac->canvas, dac->x1, dac->y1, dac->x2, dac->y2);

	return FALSE;
}

void
e_canvas_item_show_area_delayed (GnomeCanvasItem *item,
                                 gdouble x1,
                                 gdouble y1,
                                 gdouble x2,
                                 gdouble y2,
                                 gint delay)
{
	GSource *source;

	source = e_canvas_item_show_area_delayed_ex (item, x1, y1, x2, y2, delay);
	if (source)
		g_source_unref (source);
}

/* Use g_source_unref() when done with the returned pointer. */
GSource *
e_canvas_item_show_area_delayed_ex (GnomeCanvasItem *item,
                                 gdouble x1,
                                 gdouble y1,
                                 gdouble x2,
                                 gdouble y2,
                                 gint delay)
{
	GSource *source;
	DoubsAndCanvas *dac;

	g_return_val_if_fail (item != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CANVAS_ITEM (item), NULL);

	gnome_canvas_item_i2w (item, &x1, &y1);
	gnome_canvas_item_i2w (item, &x2, &y2);

	dac = g_new (DoubsAndCanvas, 1);
	dac->x1 = x1;
	dac->y1 = y1;
	dac->x2 = x2;
	dac->y2 = y2;
	dac->canvas = g_object_ref (item->canvas);

	source = g_timeout_source_new (delay);
	g_source_set_callback (source, show_area_timeout, dac, doubs_and_canvas_free);
	g_source_set_name (source, G_STRFUNC);
	g_source_attach (source, NULL);

	return source;
}
