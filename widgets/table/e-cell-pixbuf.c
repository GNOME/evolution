/*
 * e-cell-pixbuf.c: An ECell that displays a GdkPixbuf
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * Authors: Vladimir Vukicevic <vladimir@ximian.com>
 *
 */

#include <stdio.h>
#include <libgnomeui/gnome-canvas.h>
#include "e-cell-pixbuf.h"

static ECellClass *parent_class;

typedef struct _ECellPixbufView ECellPixbufView;

struct _ECellPixbufView {
    ECellView cell_view;
    GdkGC *gc;
    GnomeCanvas *canvas;
};

/*
 * ECellPixbuf functions
 */

ECell *
e_cell_pixbuf_new (void)
{
    ECellPixbuf *ecp;

    ecp = gtk_type_new (E_CELL_PIXBUF_TYPE);
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
pixbuf_realize (ECellView *ecell_view)
{
    ECellPixbufView *pixbuf_view = (ECellPixbufView *) ecell_view;

    pixbuf_view->gc = gdk_gc_new (GTK_WIDGET (pixbuf_view->canvas)->window);
}

static void
pixbuf_unrealize (ECellView *ecell_view)
{
    ECellPixbufView *pixbuf_view = (ECellPixbufView *) ecell_view;

    gdk_gc_unref (pixbuf_view->gc);
}

static void
pixbuf_draw (ECellView *ecell_view, GdkDrawable *drawable,
             int model_col, int view_col, int row, ECellFlags flags,
             int x1, int y1, int x2, int y2)
{
    ECellPixbufView *pixbuf_view = (ECellPixbufView *) ecell_view;
    GdkPixbuf *cell_pixbuf;
    int real_x, real_y, real_w, real_h;
    int pix_w, pix_h;

    cell_pixbuf = (GdkPixbuf *) e_table_model_value_at (ecell_view->e_table_model,
                                                        model_col, row);
    /* we can't make sure we really got a pixbuf since, well, it's a Gdk thing */

    if (x2 - x1 == 0)
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


    gdk_pixbuf_render_to_drawable (cell_pixbuf,
                                   drawable,
                                   pixbuf_view->gc,
                                   0, 0,
                                   real_x, real_y, 
                                   real_w, real_h,
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

    pixbuf = (GdkPixbuf *) e_table_model_value_at (ecell_view->e_table_model, model_col, row);
    if (!pixbuf) {
        /* ??? */
        g_warning ("e-cell-pixbuf: height with NULL pixbuf at %d %d %d\n", model_col, view_col, row);
        return 0;
    }

    /* We give ourselves 3 pixels of padding on either side */
    return gdk_pixbuf_get_height (pixbuf) + 6;
}

static gint
pixbuf_max_width (ECellView *ecell_view, int model_col, int view_col)
{
    gint num_rows, i;
    gint max_width = -1;

    if (model_col == 0) {
        num_rows = e_table_model_row_count (ecell_view->e_table_model);

        for (i = 0; i <= num_rows; i++) {
            GdkPixbuf *pixbuf = (GdkPixbuf *) e_table_model_value_at
                (ecell_view->e_table_model,
                 model_col,
                 i);
            int pw = gdk_pixbuf_get_width (pixbuf);
            if (max_width < pw)
                max_width = pw;
        }
    } else {
        return -1;
    }

    return max_width;
}

static void
pixbuf_destroy (GtkObject *object)
{
    /* ... */
}

static void
e_cell_pixbuf_init (GtkObject *object)
{
    /* ... */
}

static void
e_cell_pixbuf_class_init (GtkObjectClass *object_class)
{
    ECellClass *ecc = (ECellClass *) object_class;

    object_class->destroy = pixbuf_destroy;

    ecc->new_view = pixbuf_new_view;
    ecc->kill_view = pixbuf_kill_view;
    ecc->realize = pixbuf_realize;
    ecc->unrealize = pixbuf_unrealize;
    ecc->draw = pixbuf_draw;
    ecc->event = pixbuf_event;
    ecc->height = pixbuf_height;
    ecc->max_width = pixbuf_max_width;

    parent_class = gtk_type_class (E_CELL_TYPE);
}

guint
e_cell_pixbuf_get_type (void)
{
    static guint type = 0;

    if (!type) {
        GtkTypeInfo type_info = {
            "ECellPixbuf",
            sizeof (ECellPixbuf),
            sizeof (ECellPixbufClass),
            (GtkClassInitFunc) e_cell_pixbuf_class_init,
            (GtkObjectInitFunc) e_cell_pixbuf_init,
            (GtkArgSetFunc) NULL,
            (GtkArgGetFunc) NULL,
        };

        type = gtk_type_unique (e_cell_get_type (), &type_info);
    }

    return type;
}

