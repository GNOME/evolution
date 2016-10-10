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
 *		Vladimir Vukicevic <vladimir@ximian.com>
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <stdio.h>

#include <libgnomecanvas/libgnomecanvas.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "e-cell-pixbuf.h"

G_DEFINE_TYPE (ECellPixbuf, e_cell_pixbuf, E_TYPE_CELL)

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

/*
 * ECellPixbuf functions
 */

ECell *
e_cell_pixbuf_new (void)
{
	return g_object_new (E_TYPE_CELL_PIXBUF, NULL);
}

/*
 * ECell methods
 */

static ECellView *
pixbuf_new_view (ECell *ecell,
                 ETableModel *table_model,
                 gpointer e_table_item_view)
{
    ECellPixbufView *pixbuf_view = g_new0 (ECellPixbufView, 1);
    ETableItem *eti = E_TABLE_ITEM (e_table_item_view);
    GnomeCanvas *canvas = GNOME_CANVAS_ITEM (eti)->canvas;

    pixbuf_view->cell_view.ecell = ecell;
    pixbuf_view->cell_view.e_table_model = table_model;
    pixbuf_view->cell_view.e_table_item_view = e_table_item_view;
    pixbuf_view->cell_view.kill_view_cb = NULL;
    pixbuf_view->cell_view.kill_view_cb_data = NULL;

    pixbuf_view->canvas = canvas;

    return (ECellView *) pixbuf_view;
}

static void
pixbuf_kill_view (ECellView *ecell_view)
{
    ECellPixbufView *pixbuf_view = (ECellPixbufView *) ecell_view;

    if (pixbuf_view->cell_view.kill_view_cb)
	pixbuf_view->cell_view.kill_view_cb (
	    ecell_view, pixbuf_view->cell_view.kill_view_cb_data);

    if (pixbuf_view->cell_view.kill_view_cb_data)
	g_list_free (pixbuf_view->cell_view.kill_view_cb_data);

    g_free (pixbuf_view);
}

static void
pixbuf_draw (ECellView *ecell_view,
             cairo_t *cr,
             gint model_col,
             gint view_col,
             gint row,
             ECellFlags flags,
             gint x1,
             gint y1,
             gint x2,
             gint y2)
{
    GdkPixbuf *cell_pixbuf;
    gint real_x, real_y;
    gint pix_w, pix_h;

    cell_pixbuf = e_table_model_value_at (ecell_view->e_table_model,
							  1, row);

    /* we can't make sure we really got a pixbuf since, well, it's a Gdk thing */

    if (x2 - x1 == 0)
	return;

    if (!cell_pixbuf)
	return;

    pix_w = gdk_pixbuf_get_width (cell_pixbuf);
    pix_h = gdk_pixbuf_get_height (cell_pixbuf);

    /* We center the pixbuf within our allocated space */
    if (x2 - x1 > pix_w) {
	gint diff = (x2 - x1) - pix_w;
	real_x = x1 + diff / 2;
    } else {
	real_x = x1;
    }

    if (y2 - y1 > pix_h) {
	gint diff = (y2 - y1) - pix_h;
	real_y = y1 + diff / 2;
    } else {
	real_y = y1;
    }

    cairo_save (cr);
    gdk_cairo_set_source_pixbuf (cr, cell_pixbuf, real_x, real_y);
    cairo_paint_with_alpha (cr, 1);
    cairo_restore (cr);
}

static gint
pixbuf_event (ECellView *ecell_view,
              GdkEvent *event,
              gint model_col,
              gint view_col,
              gint row,
              ECellFlags flags,
              ECellActions *actions)
{
    /* noop */

    return FALSE;
}

static gint
pixbuf_height (ECellView *ecell_view,
               gint model_col,
               gint view_col,
               gint row)
{
    GdkPixbuf *pixbuf;
    if (row == -1) {
      if (e_table_model_row_count (ecell_view->e_table_model) > 0) {
	row = 0;
      } else {
	return 6;
      }
    }

    pixbuf = (GdkPixbuf *) e_table_model_value_at (ecell_view->e_table_model, 1, row);
    if (!pixbuf)
	return 0;

    /* We give ourselves 3 pixels of padding on either side */
    return gdk_pixbuf_get_height (pixbuf) + 6;
}

/*
 * ECell::print method
 */
static void
pixbuf_print (ECellView *ecell_view,
              GtkPrintContext *context,
              gint model_col,
              gint view_col,
              gint row,
              gdouble width,
              gdouble height)
{
	GdkPixbuf *pixbuf;
	gint scale;
	cairo_t *cr = gtk_print_context_get_cairo_context (context);

	pixbuf = (GdkPixbuf *) e_table_model_value_at (ecell_view->e_table_model, 1, row);
	if (pixbuf == NULL)
		return;

	scale = gdk_pixbuf_get_height (pixbuf);
	cairo_save (cr);
	cairo_translate (cr, 0, (gdouble)(height - scale) / (gdouble) 2);
	gdk_cairo_set_source_pixbuf (cr, pixbuf, (gdouble) scale, (gdouble) scale);
	cairo_paint (cr);
	cairo_restore (cr);
}

static gdouble
pixbuf_print_height (ECellView *ecell_view,
                     GtkPrintContext *context,
                     gint model_col,
                     gint view_col,
                     gint row,
                     gdouble width)
{
	GdkPixbuf *pixbuf;

	if (row == -1) {
		if (e_table_model_row_count (ecell_view->e_table_model) > 0) {
			row = 0;
		} else {
			return 6;
		}
	}

	pixbuf = (GdkPixbuf *) e_table_model_value_at (ecell_view->e_table_model, 1, row);
	if (!pixbuf)
		return 0;

	/* We give ourselves 3 pixels of padding on either side */
	return gdk_pixbuf_get_height (pixbuf);
}

static gint
pixbuf_max_width (ECellView *ecell_view,
                  gint model_col,
                  gint view_col)
{
    gint pw;
    gint num_rows, i;
    gint max_width = -1;

    if (model_col == 0) {
	num_rows = e_table_model_row_count (ecell_view->e_table_model);

	for (i = 0; i <= num_rows; i++) {
	    GdkPixbuf *pixbuf = (GdkPixbuf *) e_table_model_value_at
		(ecell_view->e_table_model,
		 1,
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
pixbuf_set_property (GObject *object,
                     guint property_id,
                     const GValue *value,
                     GParamSpec *pspec)
{
	ECellPixbuf *pixbuf;

	pixbuf = E_CELL_PIXBUF (object);

	switch (property_id) {
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
                     guint property_id,
                     GValue *value,
                     GParamSpec *pspec)
{
	ECellPixbuf *pixbuf;

	pixbuf = E_CELL_PIXBUF (object);

	switch (property_id) {
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
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_cell_pixbuf_init (ECellPixbuf *ecp)
{
	ecp->selected_column = -1;
	ecp->focused_column = -1;
	ecp->unselected_column = -1;
}

static void
e_cell_pixbuf_class_init (ECellPixbufClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	ECellClass *ecc = E_CELL_CLASS (class);

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

	g_object_class_install_property (
		object_class,
		PROP_SELECTED_COLUMN,
		g_param_spec_int (
			"selected_column",
			"Selected Column",
			NULL,
			0, G_MAXINT, 0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_FOCUSED_COLUMN,
		g_param_spec_int (
			"focused_column",
			"Focused Column",
			NULL,
			0, G_MAXINT, 0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_UNSELECTED_COLUMN,
		g_param_spec_int (
			"unselected_column",
			"Unselected Column",
			NULL,
			0, G_MAXINT, 0,
			G_PARAM_READWRITE));
}

