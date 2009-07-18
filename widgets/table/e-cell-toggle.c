/*
 * e-cell-toggle.c - Multi-state image toggle cell object.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Miguel de Icaza <miguel@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libgnomecanvas/gnome-canvas.h>

#include "a11y/e-table/gal-a11y-e-cell-toggle.h"
#include "a11y/e-table/gal-a11y-e-cell-registry.h"
#include "e-util/e-util.h"
#include "misc/e-hsv-utils.h"

#include "e-cell-toggle.h"
#include "e-table-item.h"

G_DEFINE_TYPE (ECellToggle, e_cell_toggle, E_CELL_TYPE)

typedef struct {
	ECellView     cell_view;
	GdkGC        *gc;
	GnomeCanvas  *canvas;
} ECellToggleView;

#define CACHE_SEQ_COUNT 6

/*
 * ECell::realize method
 */
static ECellView *
etog_new_view (ECell *ecell, ETableModel *table_model, gpointer e_table_item_view)
{
	ECellToggleView *toggle_view = g_new0 (ECellToggleView, 1);
	ETableItem *eti = E_TABLE_ITEM (e_table_item_view);
	GnomeCanvas *canvas = GNOME_CANVAS_ITEM (eti)->canvas;

	toggle_view->cell_view.ecell = ecell;
	toggle_view->cell_view.e_table_model = table_model;
	toggle_view->cell_view.e_table_item_view = e_table_item_view;
        toggle_view->cell_view.kill_view_cb = NULL;
        toggle_view->cell_view.kill_view_cb_data = NULL;
	toggle_view->canvas = canvas;

	return (ECellView *) toggle_view;
}

static void
etog_kill_view (ECellView *ecell_view)
{
	ECellToggleView *toggle_view = (ECellToggleView *) ecell_view;

        if (toggle_view->cell_view.kill_view_cb)
            (toggle_view->cell_view.kill_view_cb)(ecell_view, toggle_view->cell_view.kill_view_cb_data);

        if (toggle_view->cell_view.kill_view_cb_data)
            g_list_free(toggle_view->cell_view.kill_view_cb_data);

	g_free (ecell_view);
}

static void
etog_realize (ECellView *ecell_view)
{
	ECellToggleView *toggle_view = (ECellToggleView *) ecell_view;

	toggle_view->gc = gdk_gc_new (GTK_WIDGET (toggle_view->canvas)->window);
}

/*
 * ECell::unrealize method
 */
static void
etog_unrealize (ECellView *ecv)
{
	ECellToggleView *toggle_view = (ECellToggleView *) ecv;

	g_object_unref (toggle_view->gc);
	toggle_view->gc = NULL;
}

#define RGB_COLOR(color) (((color).red & 0xff00) << 8 | \
			   ((color).green & 0xff00) | \
			   ((color).blue & 0xff00) >> 8)

/*
 * ECell::draw method
 */
static void
etog_draw (ECellView *ecell_view, GdkDrawable *drawable,
	  gint model_col, gint view_col, gint row, ECellFlags flags,
	  gint x1, gint y1, gint x2, gint y2)
{
	ECellToggle *toggle = E_CELL_TOGGLE (ecell_view->ecell);
	ECellToggleView *toggle_view = (ECellToggleView *) ecell_view;
	GdkPixbuf *image;
	gint x, y, width, height;
	gint cache_seq;
	cairo_t *cr;

	const gint value = GPOINTER_TO_INT (
		 e_table_model_value_at (ecell_view->e_table_model, model_col, row));

	if (value < 0 || value >= toggle->n_states) {
		g_warning ("Value from the table model is %d, the states we support are [0..%d)\n",
			   value, toggle->n_states);
		return;
	}

	if (flags & E_CELL_SELECTED) {
		if (GTK_WIDGET_HAS_FOCUS (toggle_view->canvas))
			cache_seq = 0;
		else
			cache_seq = 1;
	} else
		cache_seq = 2;

	if (E_TABLE_ITEM (ecell_view->e_table_item_view)->alternating_row_colors && (row % 2) == 0)
		cache_seq += 3;

	image = toggle->images[value];

	if ((x2 - x1) < gdk_pixbuf_get_width (image)) {
		x = x1;
		width = x2 - x1;
	} else {
		x = x1 + ((x2 - x1) - gdk_pixbuf_get_width (image)) / 2;
		width = gdk_pixbuf_get_width (image);
	}

	if ((y2 - y1) < gdk_pixbuf_get_height (image)) {
		y = y1;
		height = y2 - y1;
	} else {
		y = y1 + ((y2 - y1) - gdk_pixbuf_get_height (image)) / 2;
		height = gdk_pixbuf_get_height (image);
	}

	cr = gdk_cairo_create (drawable);
	cairo_save (cr);
	gdk_cairo_set_source_pixbuf (cr, image, x, y);
	cairo_paint_with_alpha (cr, 1);
	cairo_restore (cr);
	cairo_destroy (cr);

}

static void
etog_set_value (ECellToggleView *toggle_view, gint model_col, gint view_col, gint row, gint value)
{
	ECell *ecell = toggle_view->cell_view.ecell;
	ECellToggle *toggle = E_CELL_TOGGLE (ecell);

	if (value >= toggle->n_states)
		value = 0;

	e_table_model_set_value_at (toggle_view->cell_view.e_table_model,
				    model_col, row, GINT_TO_POINTER (value));
}

/*
 * ECell::event method
 */
static gint
etog_event (ECellView *ecell_view, GdkEvent *event, gint model_col, gint view_col, gint row, ECellFlags flags, ECellActions *actions)
{
	ECellToggleView *toggle_view = (ECellToggleView *) ecell_view;
	gpointer _value = e_table_model_value_at (ecell_view->e_table_model, model_col, row);
	const gint value = GPOINTER_TO_INT (_value);

#if 0
	if (!(flags & E_CELL_EDITING))
		return FALSE;
#endif

	switch (event->type) {
	case GDK_KEY_PRESS:
		if (event->key.keyval != GDK_space)
			return FALSE;
		/* Fall through */
	case GDK_BUTTON_PRESS:
		if (!e_table_model_is_cell_editable(ecell_view->e_table_model, model_col, row))
			return FALSE;

		etog_set_value (toggle_view, model_col, view_col, row, value + 1);
		return TRUE;

	default:
		return FALSE;
	}
}

/*
 * ECell::height method
 */
static gint
etog_height (ECellView *ecell_view, gint model_col, gint view_col, gint row)
{
	ECellToggle *toggle = E_CELL_TOGGLE (ecell_view->ecell);

	return toggle->height;
}

/*
 * ECell::print method
 */
static void
etog_print (ECellView *ecell_view, GtkPrintContext *context,
	    gint model_col, gint view_col, gint row,
	    double width, double height)
{
	ECellToggle *toggle = E_CELL_TOGGLE(ecell_view->ecell);
	GdkPixbuf *image;
	double image_width, image_height;
	const gint value = GPOINTER_TO_INT (
			e_table_model_value_at (ecell_view->e_table_model, model_col, row));

	cairo_t *cr;
	if (value >= toggle->n_states) {
		g_warning ("Value from the table model is %d, the states we support are [0..%d)\n",
				value, toggle->n_states);
		return;
	}

	image = toggle->images[value];
	if (image) {
		cr = gtk_print_context_get_cairo_context (context);
		cairo_save(cr);
		cairo_translate (cr, 0 , 0);
		image = gdk_pixbuf_add_alpha (image, TRUE, 255, 255, 255);
		image_width = (double)gdk_pixbuf_get_width (image);
		image_height = (double)gdk_pixbuf_get_height (image);
		cairo_rectangle (cr, image_width / 7, image_height / 3,
				image_width - image_width / 4,
				image_width - image_height / 7);
		cairo_clip (cr);
		gdk_cairo_set_source_pixbuf (cr, image, 0, image_height / 4);
		cairo_paint (cr);
		cairo_restore(cr);
	}
}

static gdouble
etog_print_height (ECellView *ecell_view, GtkPrintContext *context,
		   gint model_col, gint view_col, gint row,
		   double width)
{
	ECellToggle *toggle = E_CELL_TOGGLE (ecell_view->ecell);

	return toggle->height;
}

/*
 * ECell::max_width method
 */
static gint
etog_max_width (ECellView *ecell_view, gint model_col, gint view_col)
{
	ECellToggle *toggle = E_CELL_TOGGLE (ecell_view->ecell);
	gint max_width = 0;
	gint number_of_rows;
	gint row;

	number_of_rows = e_table_model_row_count (ecell_view->e_table_model);
	for (row = 0; row < number_of_rows; row++) {
		gpointer value = e_table_model_value_at (ecell_view->e_table_model,
						      model_col, row);
		max_width = MAX (max_width, gdk_pixbuf_get_width (toggle->images[GPOINTER_TO_INT (value)]));
	}

	return max_width;
}

static void
etog_finalize (GObject *object)
{
	ECellToggle *etog = E_CELL_TOGGLE (object);
	gint i;

	for (i = 0; i < etog->n_states; i++)
		g_object_unref (etog->images [i]);

	g_free (etog->images);

	etog->images = NULL;
	etog->n_states = 0;

	G_OBJECT_CLASS (e_cell_toggle_parent_class)->finalize (object);
}

static void
e_cell_toggle_class_init (ECellToggleClass *klass)
{
	ECellClass *ecc = E_CELL_CLASS (klass);

	G_OBJECT_CLASS (klass)->finalize = etog_finalize;

	ecc->new_view   = etog_new_view;
	ecc->kill_view  = etog_kill_view;
	ecc->realize    = etog_realize;
	ecc->unrealize  = etog_unrealize;
	ecc->draw       = etog_draw;
	ecc->event      = etog_event;
	ecc->height     = etog_height;
	ecc->print      = etog_print;
	ecc->print_height = etog_print_height;
	ecc->max_width  = etog_max_width;

	gal_a11y_e_cell_registry_add_cell_type (NULL,
						 E_CELL_TOGGLE_TYPE,
                                                gal_a11y_e_cell_toggle_new);
}

static void
e_cell_toggle_init (ECellToggle *etog)
{
	etog->images = NULL;
	etog->n_states = 0;
}

/**
 * e_cell_toggle_construct:
 * @etog: a fresh ECellToggle object
 * @border: number of pixels used as a border
 * @n_states: number of states the toggle will have
 * @images: a collection of @n_states images, one for each state.
 *
 * Constructs the @etog object with the @border, @n_staes, and @images
 * arguments.
 */
void
e_cell_toggle_construct (ECellToggle *etog, gint border, gint n_states, GdkPixbuf **images)
{
	gint max_height =  0;
	gint i;

	etog->border = border;
	etog->n_states = n_states;

	etog->images = g_new (GdkPixbuf *, n_states);

	for (i = 0; i < n_states; i++) {
		etog->images [i] = images [i];
		if (images[i]) {
		g_object_ref (images [i]);

		if (gdk_pixbuf_get_height (images [i]) > max_height)
			max_height = gdk_pixbuf_get_height (images [i]);
		}
	}

	etog->height = max_height;
}

/**
 * e_cell_checkbox_new:
 * @border: number of pixels used as a border
 * @n_states: number of states the toggle will have
 * @images: a collection of @n_states images, one for each state.
 *
 * Creates a new ECell renderer that can be used to render toggle
 * buttons with the images specified in @images.  The value returned
 * by ETableModel::get_value is typecase into an integer and clamped
 * to the [0..n_states) range.  That will select the image rendered.
 *
 * Returns: an ECell object that can be used to render multi-state
 * toggle cells.
 */
ECell *
e_cell_toggle_new (gint border, gint n_states, GdkPixbuf **images)
{
	ECellToggle *etog = g_object_new (E_CELL_TOGGLE_TYPE, NULL);

	e_cell_toggle_construct (etog, border, n_states, images);

	return (ECell *) etog;
}
