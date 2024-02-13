/* GNOME libraries - GdkPixbuf item for the GNOME canvas
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gimp.org>
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
 */

#include "evolution-config.h"

#include <math.h>
#include <libgnomecanvas/gnome-canvas.h>
#include <libgnomecanvas/gnome-canvas-util.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "gnome-canvas-pixbuf.h"

/* Private part of the GnomeCanvasPixbuf structure */
struct _GnomeCanvasPixbufPrivate {
	/* Our gdk-pixbuf */
	GdkPixbuf *pixbuf;
};

/* Object argument IDs */
enum {
	PROP_0,
	PROP_PIXBUF
};

static void gnome_canvas_pixbuf_dispose (GnomeCanvasItem *object);
static void gnome_canvas_pixbuf_set_property (GObject *object,
					      guint param_id,
					      const GValue *value,
					      GParamSpec *pspec);
static void gnome_canvas_pixbuf_get_property (GObject *object,
					      guint param_id,
					      GValue *value,
					      GParamSpec *pspec);

static void gnome_canvas_pixbuf_update (GnomeCanvasItem *item,
					const cairo_matrix_t *i2c,
					gint flags);
static void gnome_canvas_pixbuf_draw (GnomeCanvasItem *item, cairo_t *cr,
				      gint x, gint y, gint width, gint height);
static GnomeCanvasItem *gnome_canvas_pixbuf_point (GnomeCanvasItem *item,
                                                   gdouble x,
                                                   gdouble y,
                                                   gint cx,
                                                   gint cy);
static void gnome_canvas_pixbuf_bounds (GnomeCanvasItem *item,
					gdouble *x1, gdouble *y1, gdouble *x2, gdouble *y2);

G_DEFINE_TYPE_WITH_PRIVATE (GnomeCanvasPixbuf, gnome_canvas_pixbuf, GNOME_TYPE_CANVAS_ITEM)

/* Class initialization function for the pixbuf canvas item */
static void
gnome_canvas_pixbuf_class_init (GnomeCanvasPixbufClass *class)
{
	GObjectClass *gobject_class;
	GnomeCanvasItemClass *item_class;

	gobject_class = (GObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	gobject_class->set_property = gnome_canvas_pixbuf_set_property;
	gobject_class->get_property = gnome_canvas_pixbuf_get_property;

	g_object_class_install_property
		(gobject_class,
		 PROP_PIXBUF,
		 g_param_spec_object ("pixbuf", NULL, NULL,
				      GDK_TYPE_PIXBUF,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	item_class->dispose = gnome_canvas_pixbuf_dispose;
	item_class->update = gnome_canvas_pixbuf_update;
	item_class->draw = gnome_canvas_pixbuf_draw;
	item_class->point = gnome_canvas_pixbuf_point;
	item_class->bounds = gnome_canvas_pixbuf_bounds;
}

/* Object initialization function for the pixbuf canvas item */
static void
gnome_canvas_pixbuf_init (GnomeCanvasPixbuf *gcp)
{
	gcp->priv = gnome_canvas_pixbuf_get_instance_private (gcp);
}

/* Dispose handler for the pixbuf canvas item */
static void
gnome_canvas_pixbuf_dispose (GnomeCanvasItem *object)
{
	GnomeCanvasPixbuf *gcp;
	GnomeCanvasPixbufPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_PIXBUF (object));

	gcp = GNOME_CANVAS_PIXBUF (object);
	priv = gcp->priv;

	g_clear_object (&priv->pixbuf);

	if (GNOME_CANVAS_ITEM_CLASS (gnome_canvas_pixbuf_parent_class)->dispose)
		GNOME_CANVAS_ITEM_CLASS (gnome_canvas_pixbuf_parent_class)->dispose (object);
}

/* Set_property handler for the pixbuf canvas item */
static void
gnome_canvas_pixbuf_set_property (GObject *object,
                                  guint param_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
	GnomeCanvasItem *item;
	GnomeCanvasPixbuf *gcp;
	GnomeCanvasPixbufPrivate *priv;
	GdkPixbuf *pixbuf;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_PIXBUF (object));

	item = GNOME_CANVAS_ITEM (object);
	gcp = GNOME_CANVAS_PIXBUF (object);
	priv = gcp->priv;

	switch (param_id) {
	case PROP_PIXBUF:
		pixbuf = g_value_get_object (value);
		if (pixbuf != priv->pixbuf) {
			if (priv->pixbuf)
				g_object_unref (priv->pixbuf);

			priv->pixbuf = g_object_ref (pixbuf);
		}

		gnome_canvas_item_request_update (item);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

/* Get_property handler for the pixbuf canvasi item */
static void
gnome_canvas_pixbuf_get_property (GObject *object,
                                  guint param_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
	GnomeCanvasPixbuf *gcp;
	GnomeCanvasPixbufPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_PIXBUF (object));

	gcp = GNOME_CANVAS_PIXBUF (object);
	priv = gcp->priv;

	switch (param_id) {
	case PROP_PIXBUF:
		g_value_set_object (value, priv->pixbuf);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

/* Bounds and utilities */

/* Recomputes the bounding box of a pixbuf canvas item.  The horizontal and
 * vertical dimensions may be specified in units or pixels, separately, so we
 * have to compute the components individually for each dimension.
 */
static void
recompute_bounding_box (GnomeCanvasPixbuf *gcp)
{
	GnomeCanvasItem *item;
	GnomeCanvasPixbufPrivate *priv;
	cairo_matrix_t i2c;
	gdouble x1, x2, y1, y2;

	item = GNOME_CANVAS_ITEM (gcp);
	priv = gcp->priv;

	if (!priv->pixbuf) {
		item->x1 = item->y1 = item->x2 = item->y2 = 0.0;
		return;
	}

	x1 = 0.0;
	x2 = gdk_pixbuf_get_width (priv->pixbuf);
	y1 = 0.0;
	y2 = gdk_pixbuf_get_height (priv->pixbuf);

	gnome_canvas_item_i2c_matrix (item, &i2c);
	gnome_canvas_matrix_transform_rect (&i2c, &x1, &y1, &x2, &y2);

	item->x1 = floor (x1);
	item->y1 = floor (y1);
	item->x2 = ceil (x2);
	item->y2 = ceil (y2);
}

/* Update sequence */

/* Update handler for the pixbuf canvas item */
static void
gnome_canvas_pixbuf_update (GnomeCanvasItem *item,
                            const cairo_matrix_t *i2c,
                            gint flags)
{
	GnomeCanvasPixbuf *gcp;

	gcp = GNOME_CANVAS_PIXBUF (item);

	if (GNOME_CANVAS_ITEM_CLASS (gnome_canvas_pixbuf_parent_class)->update)
		GNOME_CANVAS_ITEM_CLASS (gnome_canvas_pixbuf_parent_class)->
			update (item, i2c, flags);

	/* ordinary update logic */
	gnome_canvas_request_redraw (
		item->canvas, item->x1, item->y1, item->x2, item->y2);
	recompute_bounding_box (gcp);
	gnome_canvas_request_redraw (
		item->canvas, item->x1, item->y1, item->x2, item->y2);
}

/* Draw handler for the pixbuf canvas item */
static void
gnome_canvas_pixbuf_draw (GnomeCanvasItem *item,
                          cairo_t *cr,
                          gint x,
                          gint y,
                          gint width,
                          gint height)
{
	GnomeCanvasPixbuf *gcp;
	GnomeCanvasPixbufPrivate *priv;
	cairo_matrix_t matrix;

	gcp = GNOME_CANVAS_PIXBUF (item);
	priv = gcp->priv;

	if (!priv->pixbuf)
		return;

	gnome_canvas_item_i2c_matrix (item, &matrix);

	matrix.x0 -= x;
	matrix.y0 -= y;

	cairo_save (cr);
	cairo_transform (cr, &matrix);

	gdk_cairo_set_source_pixbuf (cr, priv->pixbuf, 0, 0);
	cairo_paint (cr);
	cairo_restore (cr);
}

/* Point handler for the pixbuf canvas item */
static GnomeCanvasItem *
gnome_canvas_pixbuf_point (GnomeCanvasItem *item,
                           gdouble x,
                           gdouble y,
                           gint cx,
                           gint cy)
{
	GnomeCanvasPixbuf *gcp;
	GnomeCanvasPixbufPrivate *priv;
	GdkPixbuf *pixbuf;
	gint px, py;
	guchar *src;

	gcp = GNOME_CANVAS_PIXBUF (item);
	priv = gcp->priv;
	pixbuf = priv->pixbuf;

	if (!priv->pixbuf)
		return NULL;

	px = x;
	py = y;

	if (px < 0 || px >= gdk_pixbuf_get_width (pixbuf) ||
	    py < 0 || py >= gdk_pixbuf_get_height (pixbuf))
		return NULL;

	if (!gdk_pixbuf_get_has_alpha (pixbuf))
		return item;

	src = gdk_pixbuf_get_pixels (pixbuf) +
	    py * gdk_pixbuf_get_rowstride (pixbuf) +
	    px * gdk_pixbuf_get_n_channels (pixbuf);

	if (src[3] < 128)
		return NULL;
	else
		return item;
}

/* Bounds handler for the pixbuf canvas item */
static void
gnome_canvas_pixbuf_bounds (GnomeCanvasItem *item,
                            gdouble *x1,
                            gdouble *y1,
                            gdouble *x2,
                            gdouble *y2)
{
	GnomeCanvasPixbuf *gcp;
	GnomeCanvasPixbufPrivate *priv;

	gcp = GNOME_CANVAS_PIXBUF (item);
	priv = gcp->priv;

	if (!priv->pixbuf) {
		*x1 = *y1 = *x2 = *y2 = 0.0;
		return;
	}

	*x1 = 0;
	*y1 = 0;
	*x2 = gdk_pixbuf_get_width (priv->pixbuf);
	*y2 = gdk_pixbuf_get_height (priv->pixbuf);
}
