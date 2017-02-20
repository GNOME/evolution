/* Generic bezier rect item for GnomeCanvasWidget.  Most code taken
 * from gnome-canvas-bpath but made into a rect item.
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
 * is integrated into libgnomeui, the includes will need to change. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>
#include <string.h>

#include <gtk/gtk.h>
#include <cairo-gobject.h>
#include "gnome-canvas.h"
#include "gnome-canvas-util.h"

#include "gnome-canvas-rect.h"

#define GNOME_CANVAS_RECT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), GNOME_TYPE_CANVAS_RECT, GnomeCanvasRectPrivate))

struct _GnomeCanvasRectPrivate {
	cairo_path_t *path;             /* Our bezier path representation */

	gdouble x1, y1, x2, y2;

	gdouble scale;			/* CTM scaling (for pen) */

	guint fill_set : 1;		/* Is fill color set? */
	guint outline_set : 1;		/* Is outline color set? */

	gdouble line_width;		/* Width of outline, in user coords */

	guint32 fill_rgba;		/* Fill color, RGBA */
	guint32 outline_rgba;		/* Outline color, RGBA */

	cairo_line_cap_t cap;		/* Cap style for line */
	cairo_line_join_t join;		/* Join style for line */
	cairo_fill_rule_t wind;		/* Winding rule */
	gdouble miterlimit;		/* Miter limit */

        guint n_dash;                   /* Number of elements in dashing pattern */
	gdouble *dash;		/* Dashing pattern */
        gdouble dash_offset;            /* Dashing offset */
};

enum {
	PROP_0,
	PROP_X1,
	PROP_Y1,
	PROP_X2,
	PROP_Y2,
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

static void   gnome_canvas_rect_bounds      (GnomeCanvasItem *item,
					      gdouble *x1, gdouble *y1, gdouble *x2, gdouble *y2);

G_DEFINE_TYPE (GnomeCanvasRect, gnome_canvas_rect, GNOME_TYPE_CANVAS_ITEM)

static guint32
get_rgba_from_color (GdkColor *color)
{
	guint32 rr, gg, bb, aa;

	rr = 0xFF * color->red / 65535.0;
	gg = 0xFF * color->green / 65535.0;
	bb = 0xFF * color->blue / 65535.0;
	aa = 0xFF;

	return (rr & 0xFF) << 24 |
		(gg & 0xFF) << 16 |
		(bb & 0xFF) << 8 |
		(aa & 0xFF);
}

static gboolean
gnome_canvas_rect_setup_for_fill (GnomeCanvasRect *rect,
                                  cairo_t *cr)
{
	if (!rect->priv->fill_set)
		return FALSE;

	cairo_set_source_rgba (
		cr,
		((rect->priv->fill_rgba >> 24) & 0xff) / 255.0,
		((rect->priv->fill_rgba >> 16) & 0xff) / 255.0,
		((rect->priv->fill_rgba >> 8) & 0xff) / 255.0,
		( rect->priv->fill_rgba & 0xff) / 255.0);
	cairo_set_fill_rule (cr, rect->priv->wind);

	return TRUE;
}

static gboolean
gnome_canvas_rect_setup_for_stroke (GnomeCanvasRect *rect,
                                    cairo_t *cr)
{
	if (!rect->priv->outline_set)
		return FALSE;

	cairo_set_source_rgba (
		cr,
		((rect->priv->outline_rgba >> 24) & 0xff) / 255.0,
		((rect->priv->outline_rgba >> 16) & 0xff) / 255.0,
		((rect->priv->outline_rgba >> 8) & 0xff) / 255.0,
		( rect->priv->outline_rgba & 0xff) / 255.0);
	cairo_set_line_width (cr, rect->priv->line_width);
	cairo_set_line_cap (cr, rect->priv->cap);
	cairo_set_line_join (cr, rect->priv->join);
	cairo_set_miter_limit (cr, rect->priv->miterlimit);
	cairo_set_dash (
		cr, rect->priv->dash, rect->priv->n_dash,
		rect->priv->dash_offset);

	return TRUE;
}

static void
gnome_canvas_rect_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	GnomeCanvasItem *item;
	GnomeCanvasRect *rect;
	GnomeCanvasRectPrivate *priv;
	GdkColor color;
	GdkColor *colorptr;
	const gchar *color_string;

	item = GNOME_CANVAS_ITEM (object);
	rect = GNOME_CANVAS_RECT (object);
	priv = rect->priv;

	switch (property_id) {
	case PROP_X1:
		priv->x1 = g_value_get_double (value);
		gnome_canvas_item_request_update (item);
		break;

	case PROP_Y1:
		priv->y1 = g_value_get_double (value);
		gnome_canvas_item_request_update (item);
		break;

	case PROP_X2:
		priv->x2 = g_value_get_double (value);
		gnome_canvas_item_request_update (item);
		break;

	case PROP_Y2:
		priv->y2 = g_value_get_double (value);
		gnome_canvas_item_request_update (item);
		break;

	case PROP_FILL_COLOR:
		color_string = g_value_get_string (value);
		if (color_string != NULL) {
			if (!gdk_color_parse (color_string, &color)) {
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
		g_warn_if_reached ();
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
gnome_canvas_rect_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	GnomeCanvasRect *rect = GNOME_CANVAS_RECT (object);
	GnomeCanvasRectPrivate *priv = rect->priv;

	switch (property_id) {

	case PROP_X1:
		g_value_set_double (value, priv->x1);
		break;

	case PROP_Y1:
		g_value_set_double (value, priv->y1);
		break;

	case PROP_X2:
		g_value_set_double (value, priv->x2);
		break;

	case PROP_Y2:
		g_value_set_double (value, priv->y2);
		break;

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
		g_warn_if_reached ();
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
gnome_canvas_rect_dispose (GnomeCanvasItem *object)
{
	GnomeCanvasRect *rect;

	g_return_if_fail (GNOME_IS_CANVAS_RECT (object));

	rect = GNOME_CANVAS_RECT (object);

	if (rect->priv->path != NULL) {
		cairo_path_destroy (rect->priv->path);
		rect->priv->path = NULL;
	}

	g_free (rect->priv->dash);
	rect->priv->dash = NULL;

	if (GNOME_CANVAS_ITEM_CLASS (gnome_canvas_rect_parent_class)->dispose)
		GNOME_CANVAS_ITEM_CLASS (gnome_canvas_rect_parent_class)->dispose (object);
}

static void
gnome_canvas_rect_update (GnomeCanvasItem *item,
                          const cairo_matrix_t *i2c,
                          gint flags)
{
	gdouble x1, x2, y1, y2;

	GNOME_CANVAS_ITEM_CLASS (gnome_canvas_rect_parent_class)->
		update (item, i2c, flags);

	gnome_canvas_rect_bounds (item, &x1, &y1, &x2, &y2);
	gnome_canvas_matrix_transform_rect (i2c, &x1, &y1, &x2, &y2);

	gnome_canvas_update_bbox (
		item, floor (x1), floor (y1), ceil (x2), ceil (y2));
}

static void
gnome_canvas_rect_draw (GnomeCanvasItem *item,
                        cairo_t *cr,
                        gint x,
                        gint y,
                        gint width,
                        gint height)
{
	GnomeCanvasRect *rect;
	cairo_matrix_t matrix;

	rect = GNOME_CANVAS_RECT (item);

	cairo_save (cr);

	gnome_canvas_item_i2c_matrix (item, &matrix);
	cairo_transform (cr, &matrix);

	if (gnome_canvas_rect_setup_for_fill (rect, cr)) {
		cairo_rectangle (
			cr,
			rect->priv->x1 - x,
			rect->priv->y1 - y,
			rect->priv->x2 - rect->priv->x1,
			rect->priv->y2 - rect->priv->y1);
		cairo_fill (cr);
	}

	if (gnome_canvas_rect_setup_for_stroke (rect, cr)) {
		cairo_rectangle (
			cr,
			rect->priv->x1 - x,
			rect->priv->y1 - y,
			rect->priv->x2 - rect->priv->x1,
			rect->priv->y2 - rect->priv->y1);
		cairo_stroke (cr);
	}

	cairo_restore (cr);
}

static GnomeCanvasItem *
gnome_canvas_rect_point (GnomeCanvasItem *item,
                         gdouble x,
                         gdouble y,
                         gint cx,
                         gint cy)
{
	GnomeCanvasRect *rect;
	cairo_t *cr;

	rect = GNOME_CANVAS_RECT (item);

	cr = gnome_canvas_cairo_create_scratch ();

	cairo_rectangle (
		cr,
		rect->priv->x1,
		rect->priv->y1,
		rect->priv->x2 - rect->priv->x1,
		rect->priv->y2 - rect->priv->y1);

	if (gnome_canvas_rect_setup_for_fill (rect, cr) &&
	    cairo_in_fill (cr, x, y)) {
		cairo_destroy (cr);
		return item;
	}

	if (gnome_canvas_rect_setup_for_stroke (rect, cr) &&
	    cairo_in_stroke (cr, x, y)) {
		cairo_destroy (cr);
		return item;
	}

	cairo_destroy (cr);

	return NULL;
}

static void
gnome_canvas_rect_bounds (GnomeCanvasItem *item,
                          gdouble *x1,
                          gdouble *y1,
                          gdouble *x2,
                          gdouble *y2)
{
	GnomeCanvasRect *rect;

	rect = GNOME_CANVAS_RECT (item);

	*x1 = rect->priv->x1;
	*y1 = rect->priv->y1;
	*x2 = rect->priv->x2;
	*y2 = rect->priv->y2;
}

static void
gnome_canvas_rect_class_init (GnomeCanvasRectClass *class)
{
	GObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	g_type_class_add_private (class, sizeof (GnomeCanvasRectPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = gnome_canvas_rect_set_property;
	object_class->get_property = gnome_canvas_rect_get_property;

	item_class = GNOME_CANVAS_ITEM_CLASS (class);
	item_class->dispose = gnome_canvas_rect_dispose;
	item_class->update = gnome_canvas_rect_update;
	item_class->draw = gnome_canvas_rect_draw;
	item_class->point = gnome_canvas_rect_point;
	item_class->bounds = gnome_canvas_rect_bounds;

	g_object_class_install_property (
		object_class,
		PROP_X1,
		g_param_spec_double (
			"x1",
			NULL,
			NULL,
			-G_MAXDOUBLE,
			G_MAXDOUBLE,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_Y1,
		g_param_spec_double (
			"y1",
			NULL,
			NULL,
			-G_MAXDOUBLE,
			G_MAXDOUBLE,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_X2,
		g_param_spec_double (
			"x2",
			NULL,
			NULL,
			-G_MAXDOUBLE,
			G_MAXDOUBLE,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_Y2,
		g_param_spec_double (
			"y2",
			NULL,
			NULL,
			-G_MAXDOUBLE,
			G_MAXDOUBLE,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_FILL_COLOR,
		g_param_spec_string (
			"fill_color",
			NULL,
			NULL,
			NULL,
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		object_class,
		PROP_FILL_COLOR_GDK,
		g_param_spec_boxed (
			"fill_color_gdk",
			NULL,
			NULL,
			GDK_TYPE_COLOR,
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		object_class,
		PROP_FILL_COLOR_RGBA,
		g_param_spec_uint (
			"fill_color_rgba",
			NULL,
			NULL,
			0,
			G_MAXUINT,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_OUTLINE_COLOR,
		g_param_spec_string (
			"outline_color",
			NULL,
			NULL,
			NULL,
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		object_class,
		PROP_OUTLINE_COLOR_GDK,
		g_param_spec_boxed (
			"outline_color_gdk",
			NULL,
			NULL,
			GDK_TYPE_COLOR,
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		object_class,
		PROP_OUTLINE_COLOR_RGBA,
		g_param_spec_uint (
			"outline_rgba",
			NULL,
			NULL,
			0,
			G_MAXUINT,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_LINE_WIDTH,
		g_param_spec_double (
			"line_width",
			NULL,
			NULL,
			0.0,
			G_MAXDOUBLE,
			1.0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_CAP_STYLE,
		g_param_spec_enum (
			"cap_style",
			NULL,
			NULL,
			CAIRO_GOBJECT_TYPE_LINE_CAP,
			CAIRO_LINE_CAP_BUTT,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_JOIN_STYLE,
		g_param_spec_enum (
			"join_style",
			NULL,
			NULL,
			CAIRO_GOBJECT_TYPE_LINE_JOIN,
			CAIRO_LINE_JOIN_MITER,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WIND,
		g_param_spec_enum (
			"wind",
			NULL,
			NULL,
			CAIRO_GOBJECT_TYPE_FILL_RULE,
			CAIRO_FILL_RULE_EVEN_ODD,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_MITERLIMIT,
		g_param_spec_double (
			"miterlimit",
			NULL,
			NULL,
			0.0,
			G_MAXDOUBLE,
			10.43,
			G_PARAM_READWRITE));

#if 0
	/* XXX: Find a good way to pass dash properties in a property */
	g_object_class_install_property (
		object_class,
		PROP_DASH,
		g_param_spec_pointer (
			"dash",
			NULL,
			NULL,
			G_PARAM_READWRITE));
#endif
}

static void
gnome_canvas_rect_init (GnomeCanvasRect *rect)
{
	rect->priv = GNOME_CANVAS_RECT_GET_PRIVATE (rect);

	rect->priv->scale = 1.0;

	rect->priv->fill_set = FALSE;
	rect->priv->outline_set = FALSE;

	rect->priv->line_width = 1.0;

	rect->priv->fill_rgba = 0x0000003f;
	rect->priv->outline_rgba = 0x0000007f;

	rect->priv->cap = CAIRO_LINE_CAP_BUTT;
	rect->priv->join = CAIRO_LINE_JOIN_MITER;
	rect->priv->wind = CAIRO_FILL_RULE_EVEN_ODD;
	rect->priv->miterlimit = 10.43;	   /* X11 default */

	rect->priv->n_dash = 0;
	rect->priv->dash = NULL;
}

