/*
 * e-canvas-background.c - background color for canvas.
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
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-canvas-background.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <glib/gi18n.h>

#include "e-canvas.h"
#include "e-canvas-utils.h"
#include "e-misc-utils.h"

#define d(x)

struct _ECanvasBackgroundPrivate {
	GdkRGBA rgba;		/* Fill color, RGBA */
};

/* workaround for avoiding API broken */
#define ecb_get_type e_canvas_background_get_type
G_DEFINE_TYPE_WITH_PRIVATE (ECanvasBackground, ecb, GNOME_TYPE_CANVAS_ITEM)

enum {
	STYLE_UPDATED,
	LAST_SIGNAL
};

static guint ecb_signals[LAST_SIGNAL] = { 0, };

enum {
	PROP_0,
	PROP_FILL_COLOR,
};

static void
ecb_bounds (GnomeCanvasItem *item,
            gdouble *x1,
            gdouble *y1,
            gdouble *x2,
            gdouble *y2)
{
	*x1 = 0;
	*y1 = 0;
	*x2 = G_MAXINT - 1;
	*y2 = G_MAXINT - 1;
}

static void
ecb_update (GnomeCanvasItem *item,
            const cairo_matrix_t *i2c,
            gint flags)
{
	gdouble x1, y1, x2, y2;

	x1 = item->x1;
	y1 = item->y1;
	x2 = item->x2;
	y2 = item->y2;

	/* The bounds are all constants so we should only have to
	 * redraw once. */
	ecb_bounds (item, &item->x1, &item->y1, &item->x2, &item->y2);

	if (item->x1 != x1 || item->y1 != y1 ||
	    item->x2 != x2 || item->y2 != y2)
		gnome_canvas_request_redraw (
			item->canvas, item->x1, item->y1, item->x2, item->y2);
}

static void
ecb_set_property (GObject *object,
                  guint property_id,
                  const GValue *value,
                  GParamSpec *pspec)
{
	ECanvasBackground *ecb;

	GdkRGBA *rgba;

	ecb = E_CANVAS_BACKGROUND (object);

	switch (property_id) {
	case PROP_FILL_COLOR:
		rgba = g_value_get_boxed (value);
		if (rgba) {
			ecb->priv->rgba = *rgba;
		}

		break;
	}

	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (ecb));
}

static void
ecb_init (ECanvasBackground *ecb)
{
	ecb->priv = ecb_get_instance_private (ecb);
}

static void
ecb_draw (GnomeCanvasItem *item,
          cairo_t *cr,
          gint x,
          gint y,
          gint width,
          gint height)
{
	ECanvasBackground *ecb = E_CANVAS_BACKGROUND (item);

	cairo_save (cr);
	gdk_cairo_set_source_rgba (cr, &ecb->priv->rgba);
	cairo_paint (cr);
	cairo_restore (cr);
}

static GnomeCanvasItem *
ecb_point (GnomeCanvasItem *item,
           gdouble x,
           gdouble y,
           gint cx,
           gint cy)
{
	return item;
}

static void
ecb_style_updated (ECanvasBackground *ecb)
{
	GnomeCanvasItem *item = GNOME_CANVAS_ITEM (ecb);

	if (gtk_widget_get_realized (GTK_WIDGET (item->canvas)))
		gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (ecb));
}

static void
ecb_class_init (ECanvasBackgroundClass *ecb_class)
{
	GnomeCanvasItemClass *item_class = GNOME_CANVAS_ITEM_CLASS (ecb_class);
	GObjectClass *object_class = G_OBJECT_CLASS (ecb_class);

	object_class->set_property = ecb_set_property;

	item_class->update = ecb_update;
	item_class->draw = ecb_draw;
	item_class->point = ecb_point;
	item_class->bounds = ecb_bounds;

	ecb_class->style_updated = ecb_style_updated;

	g_object_class_install_property (
		object_class,
		PROP_FILL_COLOR,
		g_param_spec_boxed (
			"fill-color",
			"Fill color",
			"Fill color",
			GDK_TYPE_RGBA,
			G_PARAM_WRITABLE));

	ecb_signals[STYLE_UPDATED] = g_signal_new (
		"style_updated",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECanvasBackgroundClass, style_updated),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

