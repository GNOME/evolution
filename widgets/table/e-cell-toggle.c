/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-cell-toggle.c - Multi-state image toggle cell object.
 * Copyright 1999, 2000, Ximian, Inc.
 *
 * Authors:
 *   Miguel de Icaza <miguel@ximian.com>
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
#include <gtk/gtkenums.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtksignal.h>
#include <gdk/gdkkeysyms.h>
#include <libgnomecanvas/gnome-canvas.h>
#include "e-cell-toggle.h"
#include "gal/util/e-util.h"
#include "gal/widgets/e-hsv-utils.h"
#include "e-table-item.h"
#include "gal/a11y/e-table/gal-a11y-e-cell-toggle.h"
#include "gal/a11y/e-table/gal-a11y-e-cell-registry.h"

#define PARENT_TYPE e_cell_get_type ()

typedef struct {
	ECellView     cell_view;
	GdkGC        *gc;
	GnomeCanvas  *canvas;
	GdkPixmap   **pixmap_cache;
} ECellToggleView;

static ECellClass *parent_class;

#define CACHE_SEQ_COUNT 6

static int
gnome_print_pixbuf (GnomePrintContext *pc, GdkPixbuf *pixbuf)
{
       if (gdk_pixbuf_get_has_alpha (pixbuf))
               return gnome_print_rgbaimage  (pc,
					      gdk_pixbuf_get_pixels    (pixbuf),
					      gdk_pixbuf_get_width     (pixbuf),
					      gdk_pixbuf_get_height    (pixbuf),
					      gdk_pixbuf_get_rowstride (pixbuf));
       else
               return gnome_print_rgbimage  (pc,
					     gdk_pixbuf_get_pixels    (pixbuf),
					     gdk_pixbuf_get_width     (pixbuf),
					     gdk_pixbuf_get_height    (pixbuf),
					     gdk_pixbuf_get_rowstride (pixbuf));
}

/*
 * ECell::realize method
 */
static ECellView *
etog_new_view (ECell *ecell, ETableModel *table_model, void *e_table_item_view)
{
	ECellToggleView *toggle_view = g_new0 (ECellToggleView, 1);
	ETableItem *eti = E_TABLE_ITEM (e_table_item_view);
	GnomeCanvas *canvas = GNOME_CANVAS_ITEM (eti)->canvas;
	ECellToggle *etog = E_CELL_TOGGLE (ecell);
	int i;

	toggle_view->cell_view.ecell = ecell;
	toggle_view->cell_view.e_table_model = table_model;
	toggle_view->cell_view.e_table_item_view = e_table_item_view;
	toggle_view->canvas = canvas;
	toggle_view->pixmap_cache = g_new (GdkPixmap *, etog->n_states * CACHE_SEQ_COUNT);
	for (i = 0; i < etog->n_states * CACHE_SEQ_COUNT; i++)
		toggle_view->pixmap_cache[i] = NULL;

	return (ECellView *) toggle_view;
}

static void
etog_kill_view (ECellView *ecell_view)
{
	ECellToggle *etog = E_CELL_TOGGLE (ecell_view->ecell);
	ECellToggleView *toggle_view = (ECellToggleView *) ecell_view;
	int i;

	for (i = 0; i < etog->n_states * CACHE_SEQ_COUNT; i++)
		if (toggle_view->pixmap_cache[i])
			gdk_pixmap_unref (toggle_view->pixmap_cache[i]);
	g_free (toggle_view->pixmap_cache);
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

	gdk_gc_unref (toggle_view->gc);
	toggle_view->gc = NULL;
}

#define PIXMAP_CACHE(toggle_view, cache_seq, image_seq) ((toggle_view)->pixmap_cache[(cache_seq) * E_CELL_TOGGLE (((ECellView *) (toggle_view))->ecell)->n_states + (image_seq)])

#define RGB_COLOR(color) (((color).red & 0xff00) << 8 | \
			   ((color).green & 0xff00) | \
			   ((color).blue & 0xff00) >> 8)

static void
check_cache (ECellToggleView *toggle_view, int image_seq, int cache_seq)
{
	ECellView *ecell_view = (ECellView *) toggle_view;
	ECellToggle *etog = E_CELL_TOGGLE (ecell_view->ecell);

	if (PIXMAP_CACHE (toggle_view, cache_seq, image_seq) == NULL) {
		GdkPixbuf *image = etog->images[image_seq];
		GdkPixbuf *flat;
		GdkColor  color;
		int width = gdk_pixbuf_get_width (image);
		int height = gdk_pixbuf_get_height (image);

		PIXMAP_CACHE (toggle_view, cache_seq, image_seq) =
			gdk_pixmap_new (toggle_view->canvas->layout.bin_window, width, height,
					gtk_widget_get_visual (GTK_WIDGET (toggle_view->canvas))->depth);

		
		switch (cache_seq % 3) {
		case 0:
			color = GTK_WIDGET (toggle_view->canvas)->style->bg [GTK_STATE_SELECTED];
			break;
		case 1:
			color = GTK_WIDGET (toggle_view->canvas)->style->bg [GTK_STATE_ACTIVE];
			break;
		case 2:
			color = GTK_WIDGET (toggle_view->canvas)->style->base [GTK_STATE_NORMAL];
			break;
		}

		if (cache_seq >= 3) {
			e_hsv_tweak (&color, 0.0f, 0.0f, -0.07f);
		}

		flat = gdk_pixbuf_composite_color_simple (image,
							  width, height,
							  GDK_INTERP_BILINEAR,
							  255,
							  1,
							  RGB_COLOR (color), RGB_COLOR (color));

		gdk_pixbuf_render_to_drawable (flat, PIXMAP_CACHE (toggle_view, cache_seq, image_seq),
					       toggle_view->gc,
					       0, 0,
					       0, 0,
					       width, height,
					       GDK_RGB_DITHER_NORMAL,
					       0, 0);
		gdk_pixbuf_unref (flat);
	}
}

/*
 * ECell::draw method
 */
static void
etog_draw (ECellView *ecell_view, GdkDrawable *drawable,
	  int model_col, int view_col, int row, ECellFlags flags,
	  int x1, int y1, int x2, int y2)
{
	ECellToggle *toggle = E_CELL_TOGGLE (ecell_view->ecell);
	gboolean selected;
	ECellToggleView *toggle_view = (ECellToggleView *) ecell_view;
	GdkPixmap *pixmap;
	GdkPixbuf *image;
	int x, y, width, height;
	int cache_seq;
	
	const int value = GPOINTER_TO_INT (
		 e_table_model_value_at (ecell_view->e_table_model, model_col, row));
	
	selected = flags & E_CELL_SELECTED;

	if (value < 0 || value >= toggle->n_states){
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

	check_cache (toggle_view, value, cache_seq);

	pixmap = PIXMAP_CACHE (toggle_view, cache_seq, value);
	image = toggle->images[value];

	if ((x2 - x1) < gdk_pixbuf_get_width (image)){
		x = x1;
		width = x2 - x1;
	} else {
		x = x1 + ((x2 - x1) - gdk_pixbuf_get_width (image)) / 2;
		width = gdk_pixbuf_get_width (image);
	}

	if ((y2 - y1) < gdk_pixbuf_get_height (image)){
		y = y1;
		height = y2 - y1;
	} else {
		y = y1 + ((y2 - y1) - gdk_pixbuf_get_height (image)) / 2;
		height = gdk_pixbuf_get_height (image);
	}

	gdk_draw_pixmap	 (drawable, toggle_view->gc,
			  pixmap,
			  0, 0,
			  x, y,
			  width, height);
}

static void
etog_set_value (ECellToggleView *toggle_view, int model_col, int view_col, int row, int value)
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
etog_event (ECellView *ecell_view, GdkEvent *event, int model_col, int view_col, int row, ECellFlags flags, ECellActions *actions)
{
	ECellToggleView *toggle_view = (ECellToggleView *) ecell_view;
	void *_value = e_table_model_value_at (ecell_view->e_table_model, model_col, row);
	const int value = GPOINTER_TO_INT (_value);

#if 0
	if (!(flags & E_CELL_EDITING))
		return FALSE;
#endif

	switch (event->type){
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
	return TRUE;
}

/*
 * ECell::height method
 */
static int
etog_height (ECellView *ecell_view, int model_col, int view_col, int row)
{
	ECellToggle *toggle = E_CELL_TOGGLE (ecell_view->ecell);

	return toggle->height;
}

/*
 * ECell::print method
 */
static void
etog_print (ECellView *ecell_view, GnomePrintContext *context, 
	    int model_col, int view_col, int row,
	    double width, double height)
{
	ECellToggle *toggle = E_CELL_TOGGLE(ecell_view->ecell);
	GdkPixbuf *image;
	const int value = GPOINTER_TO_INT (
		e_table_model_value_at (ecell_view->e_table_model, model_col, row));

	if (value >= toggle->n_states){
		g_warning ("Value from the table model is %d, the states we support are [0..%d)\n",
			   value, toggle->n_states);
		return;
	}

	gnome_print_gsave(context);

	image = toggle->images[value];

	gnome_print_translate (context, 0, (height - toggle->height) / 2);
	gnome_print_scale (context, toggle->height, toggle->height);
	gnome_print_pixbuf (context, image);
	
	gnome_print_grestore(context);
}

static gdouble
etog_print_height (ECellView *ecell_view, GnomePrintContext *context, 
		   int model_col, int view_col, int row,
		   double width)
{
	ECellToggle *toggle = E_CELL_TOGGLE (ecell_view->ecell);

	return toggle->height;
}

/*
 * ECell::max_width method
 */
static int
etog_max_width (ECellView *ecell_view, int model_col, int view_col)
{
	ECellToggle *toggle = E_CELL_TOGGLE (ecell_view->ecell);
	int max_width = 0;
	int number_of_rows;
	int row;

	number_of_rows = e_table_model_row_count (ecell_view->e_table_model);
	for (row = 0; row < number_of_rows; row++) {
		void *value = e_table_model_value_at (ecell_view->e_table_model,
						      model_col, row);
		max_width = MAX (max_width, gdk_pixbuf_get_width (toggle->images[GPOINTER_TO_INT (value)]));
	}

	return max_width;
}

static void
etog_style_set (ECellView *ecell_view, GtkStyle *previous_style)
{
	ECellToggle *toggle = E_CELL_TOGGLE (ecell_view->ecell);
	ECellToggleView *toggle_view = (ECellToggleView *) ecell_view;
	int i;

	for (i = 0; i < toggle->n_states * CACHE_SEQ_COUNT; i++) {
		if (toggle_view->pixmap_cache[i]) {
			gdk_pixmap_unref (toggle_view->pixmap_cache[i]);
			toggle_view->pixmap_cache[i] = NULL;
		}
	}
}

static void
etog_finalize (GObject *object)
{
	ECellToggle *etog = E_CELL_TOGGLE (object);
	int i;
	
	for (i = 0; i < etog->n_states; i++)
		gdk_pixbuf_unref (etog->images [i]);

	g_free (etog->images);

	etog->images = NULL;
	etog->n_states = 0;

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
e_cell_toggle_class_init (GtkObjectClass *object_class)
{
	ECellClass *ecc = (ECellClass *) object_class;

	G_OBJECT_CLASS (object_class)->finalize = etog_finalize;

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
	ecc->style_set  = etog_style_set;

	parent_class = g_type_class_ref (PARENT_TYPE);
	gal_a11y_e_cell_registry_add_cell_type (NULL,
                                                E_CELL_TOGGLE_TYPE,
                                                gal_a11y_e_cell_toggle_new);
}

static void
e_cell_toggle_init (GtkObject *object)
{
	ECellToggle *etog = (ECellToggle *) object;

	etog->images = NULL;
	etog->n_states = 0;
}

E_MAKE_TYPE(e_cell_toggle, "ECellToggle", ECellToggle, e_cell_toggle_class_init, e_cell_toggle_init, PARENT_TYPE)

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
e_cell_toggle_construct (ECellToggle *etog, int border, int n_states, GdkPixbuf **images)
{
	int max_height =  0;
	int i;
	
	etog->border = border;
	etog->n_states = n_states;

	etog->images = g_new (GdkPixbuf *, n_states);

	for (i = 0; i < n_states; i++){
		etog->images [i] = images [i];
		gdk_pixbuf_ref (images [i]);

		if (gdk_pixbuf_get_height (images [i]) > max_height)
			max_height = gdk_pixbuf_get_height (images [i]);
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
e_cell_toggle_new (int border, int n_states, GdkPixbuf **images)
{
	ECellToggle *etog = g_object_new (E_CELL_TOGGLE_TYPE, NULL);

	e_cell_toggle_construct (etog, border, n_states, images);

	return (ECell *) etog;
}
