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
#include <libgnomeui/gnome-canvas.h>
#include "e-cell-toggle.h"
#include "gal/util/e-util.h"
#include "e-table-item.h"

#define PARENT_TYPE e_cell_get_type ()

typedef struct {
	ECellView    cell_view;
	GdkGC       *gc;
	GnomeCanvas *canvas;
} ECellToggleView;

static ECellClass *parent_class;

/*
 * ECell::realize method
 */
static ECellView *
etog_new_view (ECell *ecell, ETableModel *table_model, void *e_table_item_view)
{
	ECellToggleView *toggle_view = g_new0 (ECellToggleView, 1);
	ETableItem *eti = E_TABLE_ITEM (e_table_item_view);
	GnomeCanvas *canvas = GNOME_CANVAS_ITEM (eti)->canvas;

	toggle_view->cell_view.ecell = ecell;
	toggle_view->cell_view.e_table_model = table_model;
	toggle_view->cell_view.e_table_item_view = e_table_item_view;
	toggle_view->canvas = canvas;

	return (ECellView *) toggle_view;
}

static void
etog_kill_view (ECellView *ecell_view)
{
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
#if 0
	ECellToggleView *toggle_view = (ECellToggleView *) ecell_view;
#endif
	GdkPixbuf *image;
	int x, y, width, height;
	
	const int value = GPOINTER_TO_INT (
		 e_table_model_value_at (ecell_view->e_table_model, model_col, row));
	
	selected = flags & E_CELL_SELECTED;

	if (value >= toggle->n_states){
		g_warning ("Value from the table model is %d, the states we support are [0..%d)\n",
			   value, toggle->n_states);
		return;
	}

	image = toggle->images [value];

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

#if 0 /* do alpha */
	if (gdk_pixbuf_get_has_alpha (image)) {
		flat = gdk_pixbuf_composite_color_simple (
			image,
			gdk_pixbuf_get_width (image),
			gdk_pixbuf_get_height (image),
			GDK_INTERP_NEAREST,
			255,
			32,
			0xffffff, 0xffffff);	
		
		gdk_pixbuf_render_to_drawable (flat, drawable,
					       toggle_view->gc,
					       0, 0,
					       x, y,
					       width, height,
					       GDK_RGB_DITHER_NORMAL,
					       0, 0);
		gdk_pixbuf_unref (flat);
	} else {
		gdk_pixbuf_render_to_drawable (image, drawable,
					       toggle_view->gc,
					       0, 0,
					       x, y,
					       width, height,
					       GDK_RGB_DITHER_NORMAL,
					       0, 0);
	}
#else 
	gdk_pixbuf_render_to_drawable_alpha (image, drawable,
					     0, 0,
					     x, y,
					     width, height,
					     GDK_PIXBUF_ALPHA_BILEVEL,
					     128,
					     GDK_RGB_DITHER_NORMAL,
					     x, y);
#endif
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
etog_destroy (GtkObject *object)
{
	ECellToggle *etog = E_CELL_TOGGLE (object);
	int i;
	
	for (i = 0; i < etog->n_states; i++)
		gdk_pixbuf_unref (etog->images [i]);

	g_free (etog->images);

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
e_cell_toggle_class_init (GtkObjectClass *object_class)
{
	ECellClass *ecc = (ECellClass *) object_class;

	object_class->destroy = etog_destroy;

	ecc->new_view   = etog_new_view;
	ecc->kill_view  = etog_kill_view;
	ecc->realize    = etog_realize;
	ecc->unrealize  = etog_unrealize;
	ecc->draw       = etog_draw;
	ecc->event      = etog_event;
	ecc->height     = etog_height;
	ecc->max_width  = etog_max_width;

	parent_class = gtk_type_class (PARENT_TYPE);
}

E_MAKE_TYPE(e_cell_toggle, "ECellToggle", ECellToggle, e_cell_toggle_class_init, NULL, PARENT_TYPE);

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
	ECellToggle *etog = gtk_type_new (e_cell_toggle_get_type ());

	e_cell_toggle_construct (etog, border, n_states, images);

	return (ECell *) etog;
}


