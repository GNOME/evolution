/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-cell-progress.c - Progress display cell object.
 * Copyright 1999-2002, Ximian, Inc.
 * Copyright 2001, 2002, Krisztian Pifko <monsta@users.sourceforge.net>
 *
 * Authors:
 *   Krisztian Pifko <monsta@users.sourceforge.net>
 *
 * A cell type for displaying progress bars.
 *
 * Derived from ECellToggle of Miguel de Icaza <miguel@ximian.com>.
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

#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libgnomecanvas/gnome-canvas.h>

#include "e-util/e-util.h"

#include "e-cell-progress.h"
#include "e-table-item.h"

#define PARENT_TYPE e_cell_get_type ()

typedef struct {
	ECellView    cell_view;
	GdkGC       *gc;
	GnomeCanvas *canvas;
} ECellProgressView;

static ECellClass *parent_class;

static void
eprog_queue_redraw (ECellProgressView *text_view, int view_col, int view_row)
{
	e_table_item_redraw_range (
		text_view->cell_view.e_table_item_view,
		view_col, view_row, view_col, view_row);
}

/*
 * ECell::realize method
 */
static ECellView *
eprog_new_view (ECell *ecell, ETableModel *table_model, void *e_table_item_view)
{
	ECellProgressView *progress_view = g_new0 (ECellProgressView, 1);
	ETableItem *eti = E_TABLE_ITEM (e_table_item_view);
	GnomeCanvas *canvas = GNOME_CANVAS_ITEM (eti)->canvas;
	
	progress_view->cell_view.ecell = ecell;
	progress_view->cell_view.e_table_model = table_model;
	progress_view->cell_view.e_table_item_view = e_table_item_view;
	progress_view->canvas = canvas;
	
	return (ECellView *) progress_view;
}

static void
eprog_kill_view (ECellView *ecell_view)
{
	g_free (ecell_view);
}	

static void
eprog_realize (ECellView *ecell_view)
{
	ECellProgressView *progress_view = (ECellProgressView *) ecell_view;

	progress_view->gc = gdk_gc_new (GTK_WIDGET (progress_view->canvas)->window);
}

/*
 * ECell::unrealize method
 */
static void
eprog_unrealize (ECellView *ecv)
{
	ECellProgressView *progress_view = (ECellProgressView *) ecv;

	gdk_gc_unref (progress_view->gc);
	progress_view->gc = NULL;
}

static void
eprog_clear (ECellProgress *progress)
{
  memset(progress->buffer,0x00,progress->width*progress->height*4);  
}

static void
eprog_draw_border (ECellProgress *progress, guchar red, guchar green, guchar blue)
{
  gint i, j, w4, p4, pw4, wpb4, hp1;

/*
 * some speedup
 */
  w4=progress->width*4;
  p4=progress->padding*4;
  pw4=w4*progress->padding;
  wpb4=(progress->width-progress->padding-progress->border)*4;
  hp1=(progress->height-progress->padding-1);

  for (i=progress->padding*4;i<(progress->width-progress->padding)*4;i+=4){
    for (j=0;j<progress->border;j++){
      progress->buffer[pw4+j*w4+i]=red;
      progress->buffer[pw4+j*w4+i+1]=green;
      progress->buffer[pw4+j*w4+i+2]=blue;
      progress->buffer[pw4+j*w4+i+3]=255;
      progress->buffer[(progress->height-1-progress->padding)*w4-j*w4+i]=red;
      progress->buffer[(progress->height-1-progress->padding)*w4-j*w4+i+1]=green;
      progress->buffer[(progress->height-1-progress->padding)*w4-j*w4+i+2]=blue;
      progress->buffer[(progress->height-1-progress->padding)*w4-j*w4+i+3]=255;
    }
  }
  for (i=progress->padding+progress->border;i<progress->height-progress->padding-progress->border;i++){
    for (j=0;j<4*progress->border;j+=4){
      progress->buffer[p4+i*w4+j]=red;
      progress->buffer[p4+i*w4+j+1]=green;
      progress->buffer[p4+i*w4+j+2]=blue;
      progress->buffer[p4+i*w4+j+3]=255;
      progress->buffer[i*w4+wpb4+j]=red;
      progress->buffer[i*w4+wpb4+j+1]=green;
      progress->buffer[i*w4+wpb4+j+2]=blue;
      progress->buffer[i*w4+wpb4+j+3]=255;
    }
  }
}

static void
eprog_draw_bar (ECellProgress *progress, guchar red, guchar green, guchar blue, gint value)
{
  gint i, j, w;
  
  w=value*(progress->width-2*(progress->padding+progress->border+1))/progress->max;
  for (i=(progress->padding+progress->border+1)*4;i<(progress->padding+progress->border+1+w)*4;i+=4){
    for (j=0;j<progress->height-2*(progress->padding+progress->border+1);j++){
      progress->buffer[(progress->width*(progress->padding+progress->border+1)*4)+j*progress->width*4+i]=red;
      progress->buffer[(progress->width*(progress->padding+progress->border+1)*4)+j*progress->width*4+i+1]=green;
      progress->buffer[(progress->width*(progress->padding+progress->border+1)*4)+j*progress->width*4+i+2]=blue;
      progress->buffer[(progress->width*(progress->padding+progress->border+1)*4)+j*progress->width*4+i+3]=255;
    }
  }
}

/*
 * ECell::draw method
 */
static void
eprog_draw (ECellView *ecell_view, GdkDrawable *drawable,
	  int model_col, int view_col, int row, ECellFlags flags,
	  int x1, int y1, int x2, int y2)
{
	ECellProgress *progress = E_CELL_PROGRESS (ecell_view->ecell);
	gboolean selected;
	GdkPixbuf *image;
	int x, y, width, height;
	
	const int value = GPOINTER_TO_INT (
		 e_table_model_value_at (ecell_view->e_table_model, model_col, row));
	
	selected = flags & E_CELL_SELECTED;

	if ((value > progress->max)||(value < progress->min)){
		g_warning ("Value from the table model is %d, the states we support are [%d..%d]\n",
			   value, progress->min, progress->max);
		return;
	}

	image = progress->image;

	if ((x2 - x1) < progress->width){
		x = x1;
		width = x2 - x1;
	} else {
		x = x1 + ((x2 - x1) - progress->width) / 2;
		width = progress->width;
	}

	if ((y2 - y1) < progress->height){
		y = y1;
		height = y2 - y1;
	} else {
		y = y1 + ((y2 - y1) - progress->height) / 2;
		height = progress->height;
	}

	eprog_clear(progress);

	eprog_draw_border(progress, progress->red, progress->green, progress->blue);

	eprog_draw_bar(progress, progress->red, progress->green, progress->blue, value);

	gdk_pixbuf_render_to_drawable_alpha (progress->image, drawable,
					     0, 0,
					     x, y,
					     progress->width, progress->height,
					     GDK_PIXBUF_ALPHA_BILEVEL,
					     128,
					     GDK_RGB_DITHER_NORMAL,
					     x, y);
}

static void
eprog_set_value (ECellProgressView *progress_view, int model_col, int view_col, int row, int value)
{
	ECell *ecell = progress_view->cell_view.ecell;
	ECellProgress *progress = E_CELL_PROGRESS (ecell);

	if (value > progress->max){
	  value = progress->max;
	}else if (value < progress->min){
	  value = progress->min;
	}
	e_table_model_set_value_at (progress_view->cell_view.e_table_model,
				    model_col, row, GINT_TO_POINTER (value));
	eprog_queue_redraw (progress_view, view_col, row);
}

/*
 * ECell::event method
 */
static gint
eprog_event (ECellView *ecell_view, GdkEvent *event, int model_col, int view_col, int row, ECellFlags flags, ECellActions *actions)
{
	ECellProgressView *progress_view = (ECellProgressView *) ecell_view;
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
		
		eprog_set_value (progress_view, model_col, view_col, row, value + 1);
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
eprog_height (ECellView *ecell_view, int model_col, int view_col, int row)
{
	ECellProgress *progress = E_CELL_PROGRESS (ecell_view->ecell);

	return progress->height;
}

/*
 * ECell::max_width method
 */
static int
eprog_max_width (ECellView *ecell_view, int model_col, int view_col)
{
	ECellProgress *progress = E_CELL_PROGRESS (ecell_view->ecell);

	return progress->width;
}

static void
eprog_dispose (GObject *object)
{
	ECellProgress *eprog = E_CELL_PROGRESS (object);
	
	gdk_pixbuf_unref (eprog->image);
	g_free (eprog->image);
	g_free (eprog->buffer);

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
e_cell_progress_class_init (GObjectClass *object_class)
{
	ECellClass *ecc = (ECellClass *) object_class;

	object_class->dispose = eprog_dispose;

	ecc->new_view   = eprog_new_view;
	ecc->kill_view  = eprog_kill_view;
	ecc->realize    = eprog_realize;
	ecc->unrealize  = eprog_unrealize;
	ecc->draw       = eprog_draw;
	ecc->event      = eprog_event;
	ecc->height     = eprog_height;
	ecc->max_width  = eprog_max_width;

	parent_class = g_type_class_ref (PARENT_TYPE);
}

E_MAKE_TYPE(e_cell_progress, "ECellProgress", ECellProgress, e_cell_progress_class_init, NULL, PARENT_TYPE);

/**
 * e_cell_progress_construct:
 * @eprog: a fresh ECellProgress object
 * @padding: number of pixels used as a padding
 * @border: number of pixels used as a border
 * @min: the minimum value
 * @max: the maximum value
 * @width: the width of the progress bar in pixels
 * @height: the height of the progress bar in pixels
 * @red: the red component of the progress bars rgb color
 * @green: the green component of the progress bars rgb color
 * @blue: the blue component of the progress bars rgb color
 *
 * Constructs the @eprog object with the arguments
 */
void
e_cell_progress_construct (ECellProgress *eprog, int padding, int border, int min, int max, int width, int height, guchar red, guchar green, guchar blue)
{
	eprog->padding = padding;
	eprog->border = border;
	eprog->min = min;
	eprog->max = max;
	eprog->red = red;
	eprog->green = green;
	eprog->blue = blue;

	eprog->width = (width<((padding+border)*2+5)) ? ((padding+border)*2+5) : width;
	eprog->height = (height<((padding+border)*2+5)) ? ((padding+border)*2+5) : height;

	eprog->buffer=g_new(guchar, eprog->width*eprog->height*4);

	eprog_clear(eprog);
	eprog_draw_border(eprog, red, green, blue);

	eprog->image = gdk_pixbuf_new_from_data (eprog->buffer,GDK_COLORSPACE_RGB, TRUE, 8, eprog->width, eprog->height, eprog->width*4, NULL, NULL);
}

/**
 * e_cell_progress_new:
 * @min: the minimum value
 * @max: the maximum value
 * @width: the width of the progress bar in pixels
 * @height: the height of the progress bar in pixels
 *
 * Creates a new ECell renderer that can be used to render progress
 * bars displaying the percentage of the current value between min
 * and max.
 * 
 * Returns: an ECell object that can be used to render progress cells.
 */
ECell *
e_cell_progress_new (int min, int max, int width, int height)
{
	ECellProgress *eprog = g_object_new (E_CELL_PROGRESS_TYPE, NULL);

	e_cell_progress_construct (eprog, 1, 1, min, max, (width<9) ? 9 : width, (height<9) ? 9 : height, 0x00, 0x00, 0x00);

	return (ECell *) eprog;
}

/**
 * e_cell_progress_set_padding:
 * @eprog: an ECellProgress object
 * @padding: number of pixels used as a padding
 *
 * Sets the padding around the progress bar in the cell.
 */
void
e_cell_progress_set_padding (ECellProgress *eprog, int padding)
{
	eprog->padding = padding;

	eprog->width = (eprog->width<((padding+eprog->border)*2+5)) ? ((padding+eprog->border)*2+5) : eprog->width;
	eprog->height = (eprog->height<((padding+eprog->border)*2+5)) ? ((padding+eprog->border)*2+5) : eprog->height;

	g_free (eprog->buffer);
	eprog->buffer=g_new (guchar, eprog->width*eprog->height*4);

	eprog_clear (eprog);
	eprog_draw_border (eprog, eprog->red, eprog->green, eprog->blue);

	eprog->image = gdk_pixbuf_new_from_data (eprog->buffer,GDK_COLORSPACE_RGB, TRUE, 8, eprog->width, eprog->height, eprog->width*4, NULL, NULL);
}

/**
 * e_cell_progress_set_border:
 * @eprog: an ECellProgress object
 * @border: number of pixels used as a border
 *
 * Sets the border around the progress bar in the cell.
 */
void
e_cell_progress_set_border (ECellProgress *eprog, int border)
{
	eprog->border = border;

	eprog->width = (eprog->width<((eprog->padding+border)*2+5)) ? ((eprog->padding+border)*2+5) : eprog->width;
	eprog->height = (eprog->height<((eprog->padding+border)*2+5)) ? ((eprog->padding+border)*2+5) : eprog->height;

	g_free (eprog->buffer);
	eprog->buffer=g_new (guchar, eprog->width*eprog->height*4);

	eprog_clear (eprog);
	eprog_draw_border (eprog, eprog->red, eprog->green, eprog->blue);

	eprog->image = gdk_pixbuf_new_from_data (eprog->buffer,GDK_COLORSPACE_RGB, TRUE, 8, eprog->width, eprog->height, eprog->width*4, NULL, NULL);
}

/**
 * e_cell_progress_set_color:
 * @eprog: a fresh ECellProgress object
 * @red: the red component of the progress bars rgb color
 * @green: the green component of the progress bars rgb color
 * @blue: the blue component of the progress bars rgb color
 */
void
e_cell_progress_set_color (ECellProgress *eprog, guchar red, guchar green, guchar blue)
{
	eprog->red = red;
	eprog->green = green;
	eprog->blue = blue;

	eprog_clear (eprog);
	eprog_draw_border (eprog, red, green, blue);
}
