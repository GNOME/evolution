/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-cell-pixbuf.c - An ECell that displays a GdkPixbuf
 * Copyright 2001, Ximian, Inc.
 *
 * Authors:
 *  Vladimir Vukicevic <vladimir@ximian.com>
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

#include <stdio.h>

#include <libgnomecanvas/gnome-canvas.h>

#include "gal/util/e-i18n.h"

#include "e-cell-pixbuf.h"

#define PARENT_TYPE E_CELL_TYPE
static ECellClass *parent_class;

typedef struct _ECellPixbufView ECellPixbufView;

struct _ECellPixbufView {
	ECellView cell_view;
	GnomeCanvas *canvas;
};

/* Object argument IDs */
enum {
	PROP_0,

	PROP_SELECTED_COLUMN,
	PROP_FOCUSED_COLUMN,
	PROP_UNSELECTED_COLUMN
};

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
 * ECellPixbuf functions
 */

ECell *
e_cell_pixbuf_new (void)
{
    ECellPixbuf *ecp;

    ecp = g_object_new (E_CELL_PIXBUF_TYPE, NULL);
    e_cell_pixbuf_construct (ecp);

    return (ECell *) ecp;
}

void
e_cell_pixbuf_construct (ECellPixbuf *ecp)
{
    /* noop */
    return;
}

/*
 * ECell methods
 */

static ECellView *
pixbuf_new_view (ECell *ecell, ETableModel *table_model, void *e_table_item_view)
{
    ECellPixbufView *pixbuf_view = g_new0 (ECellPixbufView, 1);
    ETableItem *eti = E_TABLE_ITEM (e_table_item_view);
    GnomeCanvas *canvas = GNOME_CANVAS_ITEM (eti)->canvas;

    pixbuf_view->cell_view.ecell = ecell;
    pixbuf_view->cell_view.e_table_model = table_model;
    pixbuf_view->cell_view.e_table_item_view = e_table_item_view;
    pixbuf_view->canvas = canvas;

    return (ECellView *) pixbuf_view;
}

static void
pixbuf_kill_view (ECellView *ecell_view)
{
    ECellPixbufView *pixbuf_view = (ECellPixbufView *) ecell_view;

    g_free (pixbuf_view);
}

static void
pixbuf_draw (ECellView *ecell_view, GdkDrawable *drawable,
             int model_col, int view_col, int row, ECellFlags flags,
             int x1, int y1, int x2, int y2)
{
    GdkPixbuf *cell_pixbuf;
    int real_x, real_y, real_w, real_h;
    int pix_w, pix_h;
    ECellPixbuf *ecp;

    cell_pixbuf = NULL;

    ecp = E_CELL_PIXBUF (ecell_view->ecell);

    if (flags & E_CELL_SELECTED) {
	    if (GTK_WIDGET_HAS_FOCUS (GNOME_CANVAS_ITEM (ecell_view->e_table_item_view)->canvas)) {
		    if (ecp->focused_column != -1)
			    cell_pixbuf = (GdkPixbuf *) e_table_model_value_at (ecell_view->e_table_model,
										ecp->focused_column, row);
	    } else {
		    if (ecp->selected_column != -1)
			    cell_pixbuf = (GdkPixbuf *) e_table_model_value_at (ecell_view->e_table_model,
										ecp->selected_column, row);
	    }
    } else {
	    if (ecp->unselected_column != -1)
		    cell_pixbuf = e_table_model_value_at (ecell_view->e_table_model,
							  ecp->unselected_column, row);
    }

    if (cell_pixbuf == NULL)
	    cell_pixbuf = e_table_model_value_at (ecell_view->e_table_model,
						  model_col, row);
    /* we can't make sure we really got a pixbuf since, well, it's a Gdk thing */

    if (x2 - x1 == 0)
        return;

    if (!cell_pixbuf)
	return;

    pix_w = gdk_pixbuf_get_width (cell_pixbuf);
    pix_h = gdk_pixbuf_get_height (cell_pixbuf);

    /* We center the pixbuf within our allocated space */
    if (x2 - x1 > pix_w) {
        int diff = (x2 - x1) - pix_w;
        real_x = x1 + diff/2;
        real_w = pix_w;
    } else {
        real_x = x1;
        real_w = x2 - x1;
    }

    if (y2 - y1 > pix_h) {
        int diff = (y2 - y1) - pix_h;
        real_y = y1 + diff/2;
        real_h = pix_h;
    } else {
        real_y = y1;
        real_h = y2 - y1;
    }


    gdk_pixbuf_render_to_drawable_alpha (cell_pixbuf,
					 drawable,
					 0, 0,
					 real_x, real_y, 
					 real_w, real_h,
					 GDK_PIXBUF_ALPHA_FULL,
					 127,
					 GDK_RGB_DITHER_NORMAL,
					 0, 0);
}

static gint
pixbuf_event (ECellView *ecell_view, GdkEvent *event,
              int model_col, int view_col, int row,
              ECellFlags flags, ECellActions *actions)
{
    /* noop */

    return FALSE;
}

static gint
pixbuf_height (ECellView *ecell_view, int model_col, int view_col, int row)
{
    GdkPixbuf *pixbuf;
    if (row == -1) {
      if (e_table_model_row_count (ecell_view->e_table_model) > 0) {
        row = 0;
      } else {
	return 6;
      }
    }

    pixbuf = (GdkPixbuf *) e_table_model_value_at (ecell_view->e_table_model, model_col, row);
    if (!pixbuf)
        return 0;

    /* We give ourselves 3 pixels of padding on either side */
    return gdk_pixbuf_get_height (pixbuf) + 6;
}

/*
 * ECell::print method
 */
static void
pixbuf_print (ECellView *ecell_view, GnomePrintContext *context, 
	      int model_col, int view_col, int row,
	      double width, double height)
{
	GdkPixbuf *pixbuf;
	int scale;

	pixbuf = (GdkPixbuf *) e_table_model_value_at (ecell_view->e_table_model, model_col, row);
	if (pixbuf == NULL)
		return;
	scale = gdk_pixbuf_get_height (pixbuf);
	
	gnome_print_gsave(context);

	gnome_print_translate (context, 0, (height - scale) / 2);
	gnome_print_scale (context, scale, scale);
	gnome_print_pixbuf (context, pixbuf);
	
	gnome_print_grestore(context);
}

static gdouble
pixbuf_print_height (ECellView *ecell_view, GnomePrintContext *context, 
		     int model_col, int view_col, int row,
		     double width)
{
	GdkPixbuf *pixbuf;

	if (row == -1) {
		if (e_table_model_row_count (ecell_view->e_table_model) > 0) {
			row = 0;
		} else {
			return 6;
		}
	}
	
	pixbuf = (GdkPixbuf *) e_table_model_value_at (ecell_view->e_table_model, model_col, row);
	if (!pixbuf)
		return 0;
	
	/* We give ourselves 3 pixels of padding on either side */
	return gdk_pixbuf_get_height (pixbuf);
}

static gint
pixbuf_max_width (ECellView *ecell_view, int model_col, int view_col)
{
    int pw;
    gint num_rows, i;
    gint max_width = -1;

    if (model_col == 0) {
        num_rows = e_table_model_row_count (ecell_view->e_table_model);

        for (i = 0; i <= num_rows; i++) {
            GdkPixbuf *pixbuf = (GdkPixbuf *) e_table_model_value_at
                (ecell_view->e_table_model,
                 model_col,
                 i);
	    if (!pixbuf)
		continue;
            pw = gdk_pixbuf_get_width (pixbuf);
            if (max_width < pw)
                max_width = pw;
        }
    } else {
        return -1;
    }

    return max_width;
}

static void
pixbuf_dispose (GObject *object)
{
	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
pixbuf_set_property (GObject *object,
		     guint prop_id,
		     const GValue *value,
		     GParamSpec *pspec)
{
	ECellPixbuf *pixbuf;

	pixbuf = E_CELL_PIXBUF (object);

	switch (prop_id) {
	case PROP_SELECTED_COLUMN:
		pixbuf->selected_column = g_value_get_int (value);
		break;

	case PROP_FOCUSED_COLUMN:
		pixbuf->focused_column = g_value_get_int (value);
		break;

	case PROP_UNSELECTED_COLUMN:
		pixbuf->unselected_column = g_value_get_int (value);
		break;

	default:
		return;
	}
}

/* Get_arg handler for the pixbuf item */
static void
pixbuf_get_property (GObject *object,
		     guint prop_id,
		     GValue *value,
		     GParamSpec *pspec)
{
	ECellPixbuf *pixbuf;

	pixbuf = E_CELL_PIXBUF (object);
	
	switch (prop_id) {
	case PROP_SELECTED_COLUMN:
		g_value_set_int (value, pixbuf->selected_column);
		break;

	case PROP_FOCUSED_COLUMN:
		g_value_set_int (value, pixbuf->focused_column);
		break;

	case PROP_UNSELECTED_COLUMN:
		g_value_set_int (value, pixbuf->unselected_column);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_cell_pixbuf_init (GtkObject *object)
{
	ECellPixbuf *ecp = E_CELL_PIXBUF (object);

	ecp->selected_column = -1;
	ecp->focused_column = -1;
	ecp->unselected_column = -1;
}

static void
e_cell_pixbuf_class_init (GObjectClass *object_class)
{
	ECellClass *ecc = (ECellClass *) object_class;

	object_class->dispose = pixbuf_dispose;
	object_class->set_property = pixbuf_set_property;
	object_class->get_property = pixbuf_get_property;

	ecc->new_view = pixbuf_new_view;
	ecc->kill_view = pixbuf_kill_view;
	ecc->draw = pixbuf_draw;
	ecc->event = pixbuf_event;
	ecc->height = pixbuf_height;
	ecc->print = pixbuf_print;
	ecc->print_height = pixbuf_print_height;
	ecc->max_width = pixbuf_max_width;

	parent_class = g_type_class_ref (PARENT_TYPE);

	g_object_class_install_property (object_class, PROP_SELECTED_COLUMN,
					 g_param_spec_int ("selected_column",
							   _("Selected Column"),
							   /*_( */"XXX blurb" /*)*/,
							   0, G_MAXINT, 0,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_FOCUSED_COLUMN,
					 g_param_spec_int ("focused_column",
							   _("Focused Column"),
							   /*_( */"XXX blurb" /*)*/,
							   0, G_MAXINT, 0,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_UNSELECTED_COLUMN,
					 g_param_spec_int ("unselected_column",
							   _("Unselected Column"),
							   /*_( */"XXX blurb" /*)*/,
							   0, G_MAXINT, 0,
							   G_PARAM_READWRITE));
}

E_MAKE_TYPE (e_cell_pixbuf,
	     "ECellPixbuf",
	     ECellPixbuf,
	     e_cell_pixbuf_class_init,
	     e_cell_pixbuf_init,
	     PARENT_TYPE)
