/* Generic bezier shape item for GnomeCanvasWidget.  Most code taken
 * from gnome-canvas-bpath but made into a shape item.
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent
 * canvas widget.  Tk is copyrighted by the Regents of the University
 * of California, Sun Microsystems, and other parties.
 *
 * Copyright (C) 1998,1999 The Free Software Foundation
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Raph Levien <raph@acm.org>
 *          Lauris Kaplinski <lauris@ximian.com>
 *          Miguel de Icaza <miguel@kernel.org>
 *          Cody Russell <bratsche@gnome.org>
 *          Rusty Conover <rconover@bangtail.net>
 */

/* These includes are set up for standalone compile. If/when this codebase
   is integrated into libgnomeui, the includes will need to change. */

#include <math.h>
#include <string.h>

#include <gtk/gtk.h>
#include <cairo-gobject.h>
#include "gnome-canvas.h"
#include "gnome-canvas-util.h"

#include "gnome-canvas-shape.h"
#include "gnome-canvas-shape-private.h"

enum {
	PROP_0,
	PROP_FILL_COLOR,
	PROP_FILL_COLOR_GDK,
	PROP_FILL_COLOR_RGBA,
	PROP_OUTLINE_COLOR,
	PROP_OUTLINE_COLOR_GDK,
	PROP_OUTLINE_COLOR_RGBA,
	PROP_LINE_WIDTH,
	PROP_CAP_STYLE,
	PROP_JOIN_STYLE,
	PROP_WIND,
	PROP_MITERLIMIT,
	PROP_DASH
};

static void gnome_canvas_shape_dispose      (GnomeCanvasItem       *object);
static void gnome_canvas_shape_set_property (GObject               *object,
					     guint                  param_id,
					     const GValue          *value,
                                             GParamSpec            *pspec);
static void gnome_canvas_shape_get_property (GObject               *object,
					     guint                  param_id,
					     GValue                *value,
                                             GParamSpec            *pspec);

static void   gnome_canvas_shape_update      (GnomeCanvasItem *item, const cairo_matrix_t *i2c, gint flags);
static void   gnome_canvas_shape_draw        (GnomeCanvasItem *item, GdkDrawable *drawable,
                                              gint x, gint y, gint width, gint height);
static GnomeCanvasItem *gnome_canvas_shape_point (GnomeCanvasItem *item, gdouble x, gdouble y,
                                              gint cx, gint cy);
static void   gnome_canvas_shape_bounds      (GnomeCanvasItem *item,
					      gdouble *x1, gdouble *y1, gdouble *x2, gdouble *y2);

G_DEFINE_TYPE (GnomeCanvasShape, gnome_canvas_shape, GNOME_TYPE_CANVAS_ITEM)

static void
gnome_canvas_shape_class_init (GnomeCanvasShapeClass *class)
{
	GObjectClass         *gobject_class;
	GnomeCanvasItemClass *item_class;

	gobject_class = (GObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	gobject_class->set_property = gnome_canvas_shape_set_property;
	gobject_class->get_property = gnome_canvas_shape_get_property;

        g_object_class_install_property (gobject_class,
                                         PROP_FILL_COLOR,
                                         g_param_spec_string ("fill_color", NULL, NULL,
                                                              NULL,
                                                              (G_PARAM_WRITABLE)));
        g_object_class_install_property (gobject_class,
                                         PROP_FILL_COLOR_GDK,
                                         g_param_spec_boxed ("fill_color_gdk", NULL, NULL,
                                                             GDK_TYPE_COLOR,
                                                             G_PARAM_WRITABLE));
        g_object_class_install_property (gobject_class,
                                         PROP_FILL_COLOR_RGBA,
                                         g_param_spec_uint ("fill_rgba", NULL, NULL,
                                                            0, G_MAXUINT, 0,
                                                            (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (gobject_class,
                                         PROP_OUTLINE_COLOR,
                                         g_param_spec_string ("outline_color", NULL, NULL,
                                                              NULL,
                                                              (G_PARAM_WRITABLE)));
        g_object_class_install_property (gobject_class,
                                         PROP_OUTLINE_COLOR_GDK,
                                         g_param_spec_boxed ("outline_color_gdk", NULL, NULL,
                                                             GDK_TYPE_COLOR,
                                                             G_PARAM_WRITABLE));
        g_object_class_install_property (gobject_class,
                                         PROP_OUTLINE_COLOR_RGBA,
                                         g_param_spec_uint ("outline_rgba", NULL, NULL,
                                                            0, G_MAXUINT, 0,
                                                            (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (gobject_class,
                                         PROP_LINE_WIDTH,
                                         g_param_spec_double ("line_width", NULL, NULL,
                                                              0.0, G_MAXDOUBLE, 1.0,
                                                              (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (gobject_class,
                                         PROP_CAP_STYLE,
                                         g_param_spec_enum ("cap_style", NULL, NULL,
                                                            CAIRO_GOBJECT_TYPE_LINE_CAP,
                                                            CAIRO_LINE_CAP_BUTT,
                                                            (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (gobject_class,
                                         PROP_JOIN_STYLE,
                                         g_param_spec_enum ("join_style", NULL, NULL,
                                                            CAIRO_GOBJECT_TYPE_LINE_JOIN,
                                                            CAIRO_LINE_JOIN_MITER,
                                                            (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (gobject_class,
                                         PROP_WIND,
                                         g_param_spec_enum ("wind", NULL, NULL,
                                                            CAIRO_GOBJECT_TYPE_FILL_RULE,
                                                            CAIRO_FILL_RULE_EVEN_ODD,
                                                            (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (gobject_class,
                                         PROP_MITERLIMIT,
                                         g_param_spec_double ("miterlimit", NULL, NULL,
                                                              0.0, G_MAXDOUBLE, 10.43,
                                                              (G_PARAM_READABLE | G_PARAM_WRITABLE)));
#if 0
        /* XXX: Find a good way to pass dash properties in a property */
        g_object_class_install_property (gobject_class,
                                         PROP_DASH,
                                         g_param_spec_pointer ("dash", NULL, NULL,
                                                               (G_PARAM_READABLE | G_PARAM_WRITABLE)));
#endif

	item_class->dispose = gnome_canvas_shape_dispose;
	item_class->update = gnome_canvas_shape_update;
	item_class->draw = gnome_canvas_shape_draw;
	item_class->point = gnome_canvas_shape_point;
	item_class->bounds = gnome_canvas_shape_bounds;

        g_type_class_add_private (class, sizeof (GnomeCanvasShapePriv));
}

static void
gnome_canvas_shape_init (GnomeCanvasShape *shape)
{
	shape->priv = G_TYPE_INSTANCE_GET_PRIVATE (shape,
                                                   GNOME_TYPE_CANVAS_SHAPE,
                                                   GnomeCanvasShapePriv);

	shape->priv->path = NULL;

	shape->priv->scale = 1.0;

	shape->priv->fill_set = FALSE;
	shape->priv->outline_set = FALSE;

	shape->priv->line_width = 1.0;

	shape->priv->fill_rgba = 0x0000003f;
	shape->priv->outline_rgba = 0x0000007f;

	shape->priv->cap = CAIRO_LINE_CAP_BUTT;
	shape->priv->join = CAIRO_LINE_JOIN_MITER;
	shape->priv->wind = CAIRO_FILL_RULE_EVEN_ODD;
	shape->priv->miterlimit = 10.43;	   /* X11 default */

	shape->priv->n_dash = 0;
	shape->priv->dash = NULL;
}

static void
gnome_canvas_shape_dispose (GnomeCanvasItem *object)
{
	GnomeCanvasShape *shape;

	g_return_if_fail (GNOME_IS_CANVAS_SHAPE (object));

	shape = GNOME_CANVAS_SHAPE (object);

	if (shape->priv->path != NULL) {
		cairo_path_destroy (shape->priv->path);
		shape->priv->path = NULL;
	}

	g_free (shape->priv->dash);
	shape->priv->dash = NULL;

	if (GNOME_CANVAS_ITEM_CLASS (gnome_canvas_shape_parent_class)->dispose)
		GNOME_CANVAS_ITEM_CLASS (gnome_canvas_shape_parent_class)->dispose (object);
}

/**
 * gnome_canvas_shape_set_path:
 * @shape: a GnomeCanvasShape
 * @path: a cairo path from a cairo_copy_path() call
 *
 * This function sets the the path used by the GnomeCanvasShape.
 * Notice that it does not request updates, as it is meant to be used
 * from item implementations, from inside update queue.
 */
void
gnome_canvas_shape_set_path (GnomeCanvasShape *shape, cairo_path_t *path)
{
	GnomeCanvasShapePriv *priv;

	g_return_if_fail (shape != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_SHAPE (shape));

	priv = shape->priv;

	if (priv->path) {
		cairo_path_destroy (priv->path);
		priv->path = NULL;
	}

	priv->path = path;
}

static guint32
get_rgba_from_color (GdkColor * color)
{
       return ((color->red & 0xff00) << 16) | ((color->green & 0xff00) << 8) | (color->blue & 0xff00) | 0xff;
}

static void
gnome_canvas_shape_set_property (GObject      *object,
                                 guint         param_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
	GnomeCanvasItem         *item;
	GnomeCanvasShape        *shape;
	GnomeCanvasShapePriv    *priv;
	GdkColor                 color;
	GdkColor                *colorptr;
	const gchar             *color_string;

	item = GNOME_CANVAS_ITEM (object);
	shape = GNOME_CANVAS_SHAPE (object);
	priv = shape->priv;

	switch (param_id) {
	case PROP_FILL_COLOR:
		color_string = g_value_get_string (value);
		if (color_string != NULL) {
			if (gdk_color_parse (color_string, &color)) {
				g_warning (
					"Failed to parse color '%s'",
					color_string);
				break;
			}
			priv->fill_set = TRUE;
			priv->fill_rgba = get_rgba_from_color (&color);
		} else if (priv->fill_set)
			priv->fill_set = FALSE;
		else
			break;

		gnome_canvas_item_request_update (item);
		break;

	case PROP_FILL_COLOR_GDK:
		colorptr = g_value_get_boxed (value);
		if (colorptr != NULL) {
			priv->fill_set = TRUE;
			priv->fill_rgba = get_rgba_from_color (colorptr);
		} else if (priv->fill_set)
			priv->fill_set = FALSE;
		else
			break;

		gnome_canvas_item_request_update (item);
		break;

	case PROP_FILL_COLOR_RGBA:
		priv->fill_set = TRUE;
		priv->fill_rgba = g_value_get_uint (value);

		gnome_canvas_item_request_update (item);
		break;

	case PROP_OUTLINE_COLOR:
		color_string = g_value_get_string (value);
		if (color_string != NULL) {
			if (!gdk_color_parse (color_string, &color)) {
				g_warning (
					"Failed to parse color '%s'",
					color_string);
				break;
			}
			priv->outline_set = TRUE;
			priv->outline_rgba = get_rgba_from_color (&color);
		} else if (priv->outline_set)
			priv->outline_set = FALSE;
		else
			break;

		gnome_canvas_item_request_update (item);
		break;

	case PROP_OUTLINE_COLOR_GDK:
		colorptr = g_value_get_boxed (value);
		if (colorptr != NULL) {
			priv->outline_set = TRUE;
			priv->outline_rgba = get_rgba_from_color (colorptr);
		} else if (priv->outline_set)
			priv->outline_set = FALSE;
		else
			break;

		gnome_canvas_item_request_update (item);
		break;

	case PROP_OUTLINE_COLOR_RGBA:
		priv->outline_set = TRUE;
		priv->outline_rgba = g_value_get_uint (value);

		gnome_canvas_item_request_update (item);
		break;

	case PROP_LINE_WIDTH:
		priv->line_width = g_value_get_double (value);

		gnome_canvas_item_request_update (item);
		break;

	case PROP_WIND:
		priv->wind = g_value_get_enum (value);
		gnome_canvas_item_request_update (item);
		break;

	case PROP_CAP_STYLE:
		priv->cap = g_value_get_enum (value);
		gnome_canvas_item_request_update (item);
		break;

	case PROP_JOIN_STYLE:
		priv->join = g_value_get_enum (value);
		gnome_canvas_item_request_update (item);
		break;

	case PROP_MITERLIMIT:
		priv->miterlimit = g_value_get_double (value);
		gnome_canvas_item_request_update (item);
		break;

	case PROP_DASH:
                /* XXX */
                g_assert_not_reached ();
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

/**
 * gnome_canvas_shape_get_path:
 * @shape: a GnomeCanvasShape
 *
 * This function returns the #cairo_path_t that the shape currently
 * uses. If there is not a #GnomeCanvasPathDef set for the shape
 * it returns NULL.
 *
 * Returns: a #cairo_path_t or NULL if none is set for the shape.
 */
const cairo_path_t *
gnome_canvas_shape_get_path (GnomeCanvasShape *shape)
{
	g_return_val_if_fail (shape != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CANVAS_SHAPE (shape), NULL);

	return shape->priv->path;
}

static void
gnome_canvas_shape_get_property (GObject     *object,
                                 guint        param_id,
                                 GValue      *value,
                                 GParamSpec  *pspec)
{
	GnomeCanvasShape        *shape = GNOME_CANVAS_SHAPE (object);
	GnomeCanvasShapePriv    *priv = shape->priv;

	switch (param_id) {

	case PROP_FILL_COLOR_RGBA:
		g_value_set_uint (value, priv->fill_rgba);
		break;

	case PROP_OUTLINE_COLOR_RGBA:
		g_value_set_uint (value, priv->outline_rgba);
		break;

	case PROP_WIND:
		g_value_set_uint (value, priv->wind);
		break;

	case PROP_CAP_STYLE:
		g_value_set_enum (value, priv->cap);
		break;

	case PROP_JOIN_STYLE:
		g_value_set_enum (value, priv->join);
		break;

	case PROP_LINE_WIDTH:
		g_value_set_double (value, priv->line_width);
		break;

	case PROP_MITERLIMIT:
		g_value_set_double (value, priv->miterlimit);
		break;

	case PROP_DASH:
                /* XXX */
                g_assert_not_reached ();
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static gboolean
gnome_canvas_shape_setup_for_fill (GnomeCanvasShape *shape, cairo_t *cr)
{
        GnomeCanvasShapePriv *priv = shape->priv;

        if (!priv->fill_set)
                return FALSE;

        cairo_set_source_rgba (cr,
                               ((priv->fill_rgba >> 24) & 0xff) / 255.0,
                               ((priv->fill_rgba >> 16) & 0xff) / 255.0,
                               ((priv->fill_rgba >>  8) & 0xff) / 255.0,
                               ( priv->fill_rgba        & 0xff) / 255.0);
        cairo_set_fill_rule (cr, priv->wind);

        return TRUE;
}

static gboolean
gnome_canvas_shape_setup_for_stroke (GnomeCanvasShape *shape, cairo_t *cr)
{
        GnomeCanvasShapePriv *priv = shape->priv;

        if (!priv->outline_set)
                return FALSE;

        cairo_set_source_rgba (cr,
                               ((priv->outline_rgba >> 24) & 0xff) / 255.0,
                               ((priv->outline_rgba >> 16) & 0xff) / 255.0,
                               ((priv->outline_rgba >>  8) & 0xff) / 255.0,
                               ( priv->outline_rgba        & 0xff) / 255.0);
        cairo_set_line_width (cr, priv->line_width);
        cairo_set_line_cap (cr, priv->cap);
        cairo_set_line_join (cr, priv->join);
        cairo_set_miter_limit (cr, priv->miterlimit);
        cairo_set_dash (cr, priv->dash, priv->n_dash, priv->dash_offset);

        return TRUE;
}

static void
gnome_canvas_shape_draw (GnomeCanvasItem *item,
                         GdkDrawable *drawable,
                         gint x,
                         gint y,
                         gint width,
                         gint height)
{
	GnomeCanvasShape * shape;
        cairo_matrix_t matrix;
        cairo_t *cr;

	shape = GNOME_CANVAS_SHAPE (item);
        cr = gdk_cairo_create (drawable);
        gnome_canvas_item_i2c_matrix (item, &matrix);
        cairo_transform (cr, &matrix);
        cairo_append_path (cr, shape->priv->path);

        if (gnome_canvas_shape_setup_for_fill (shape, cr))
                cairo_fill_preserve (cr);

        if (gnome_canvas_shape_setup_for_stroke (shape, cr))
                cairo_stroke_preserve (cr);

        cairo_destroy (cr);
}

static void
gnome_canvas_shape_bounds (GnomeCanvasItem *item, gdouble *x1, gdouble *y1, gdouble *x2, gdouble *y2)
{
	GnomeCanvasShape * shape;
	GnomeCanvasShapePriv * priv;
        cairo_t *cr;

	shape = GNOME_CANVAS_SHAPE (item);
	priv = shape->priv;

        cr = gnome_canvas_cairo_create_scratch ();
        cairo_append_path (cr, shape->priv->path);

        if (gnome_canvas_shape_setup_for_stroke (shape, cr))
                cairo_stroke_extents (cr, x1, y1, x2, y2);
        else if (gnome_canvas_shape_setup_for_fill (shape, cr))
                cairo_fill_extents (cr, x1, y1, x2, y2);
        else {
          *x1 = *x2 = *y1 = *y2 = 0;
        }

        cairo_destroy (cr);
}

static void
gnome_canvas_shape_update (GnomeCanvasItem *item, const cairo_matrix_t *i2c, gint flags)
{
	GnomeCanvasShape * shape;
	GnomeCanvasShapePriv * priv;
        double x1, x2, y1, y2;
        cairo_matrix_t matrix;

	shape = GNOME_CANVAS_SHAPE (item);

	priv = shape->priv;

	/* Common part */
	if (GNOME_CANVAS_ITEM_CLASS (gnome_canvas_shape_parent_class)->update)
		GNOME_CANVAS_ITEM_CLASS (gnome_canvas_shape_parent_class)->update (item, i2c, flags);

        gnome_canvas_shape_bounds (item, &x1, &x2, &y1, &y2);
        gnome_canvas_item_i2w_matrix (item, &matrix);

        gnome_canvas_matrix_transform_rect (&matrix, &x1, &y1, &x2, &y2);
        gnome_canvas_update_bbox (GNOME_CANVAS_ITEM (shape),
                                  floor (x1), floor (y1),
                                  ceil (x2), ceil (y2));
}

static GnomeCanvasItem *
gnome_canvas_shape_point (GnomeCanvasItem *item, gdouble x, gdouble y,
			  gint cx, gint cy)
{
	GnomeCanvasShape * shape;
        cairo_t *cr;

	shape = GNOME_CANVAS_SHAPE (item);

        cr = gnome_canvas_cairo_create_scratch ();
        cairo_append_path (cr, shape->priv->path);

        if (gnome_canvas_shape_setup_for_fill (shape, cr) &&
            cairo_in_fill (cr, x, y)) {
                cairo_destroy (cr);
                return item;
        }

        if (gnome_canvas_shape_setup_for_stroke (shape, cr) &&
            cairo_in_stroke (cr, x, y)) {
                cairo_destroy (cr);
                return item;
        }

        cairo_destroy (cr);

        return NULL;
}
