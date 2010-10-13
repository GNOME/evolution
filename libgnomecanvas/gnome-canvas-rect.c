/*
 * Copyright (C) 1997, 1998, 1999, 2000 Free Software Foundation
 * All rights reserved.
 *
 * This file is part of the Gnome Library.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
/*
  @NOTATION@
 */
/* Rectangle and ellipse item types for GnomeCanvas widget
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent canvas widget.  Tk is
 * copyrighted by the Regents of the University of California, Sun Microsystems, and other parties.
 *
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Rusty Conover <rconover@bangtail.net>
 */

#include <config.h>
#include <math.h>
#include "gnome-canvas-rect.h"
#include "gnome-canvas-util.h"
#include "gnome-canvas-shape.h"

#include <libart_lgpl/art_vpath.h>
#include <libart_lgpl/art_svp.h>
#include <libart_lgpl/art_svp_vpath.h>
#include <libart_lgpl/art_rgb_svp.h>

/* Base class for rectangle and ellipse item types */

#define noVERBOSE

enum {
	PROP_0,
	PROP_X1,
	PROP_Y1,
	PROP_X2,
	PROP_Y2
};

static void gnome_canvas_rect_set_property (GObject              *object,
                                            guint                 param_id,
                                            const GValue         *value,
                                            GParamSpec           *pspec);
static void gnome_canvas_rect_get_property (GObject              *object,
                                            guint                 param_id,
                                            GValue               *value,
                                            GParamSpec           *pspec);

static void gnome_canvas_rect_update       (GnomeCanvasItem *item, gdouble *affine, ArtSVP *clip_path, gint flags);

G_DEFINE_TYPE(GnomeCanvasRect, gnome_canvas_rect, GNOME_TYPE_CANVAS_SHAPE)

static void
gnome_canvas_rect_class_init (GnomeCanvasRectClass *class)
{
	GObjectClass *gobject_class;
        GnomeCanvasItemClass *item_class;

	gobject_class = (GObjectClass *) class;

	gobject_class->set_property = gnome_canvas_rect_set_property;
	gobject_class->get_property = gnome_canvas_rect_get_property;

        g_object_class_install_property
                (gobject_class,
                 PROP_X1,
                 g_param_spec_double ("x1", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_Y1,
                 g_param_spec_double ("y1", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_X2,
                 g_param_spec_double ("x2", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_Y2,
                 g_param_spec_double ("y2", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	item_class = (GnomeCanvasItemClass *) class;

	item_class->update = gnome_canvas_rect_update;
}

static void
gnome_canvas_rect_init (GnomeCanvasRect *rect)
{
	rect->x1 = 0.0;
	rect->y1 = 0.0;
	rect->x2 = 0.0;
	rect->y2 = 0.0;
	rect->path_dirty = 0;
}

static void
gnome_canvas_rect_set_property (GObject              *object,
                                guint                 param_id,
                                const GValue         *value,
                                GParamSpec           *pspec)
{
	GnomeCanvasItem *item;
	GnomeCanvasRect *rect;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_RECT (object));

	item = GNOME_CANVAS_ITEM (object);
	rect = GNOME_CANVAS_RECT (object);

	switch (param_id) {
	case PROP_X1:
		rect->x1 = g_value_get_double (value);
		rect->path_dirty = 1;
		gnome_canvas_item_request_update (item);
		break;

	case PROP_Y1:
		rect->y1 = g_value_get_double (value);
		rect->path_dirty = 1;
		gnome_canvas_item_request_update (item);
		break;

	case PROP_X2:
		rect->x2 = g_value_get_double (value);
		rect->path_dirty = 1;
		gnome_canvas_item_request_update (item);
		break;

	case PROP_Y2:
		rect->y2 = g_value_get_double (value);
		rect->path_dirty = 1;
		gnome_canvas_item_request_update (item);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gnome_canvas_rect_get_property (GObject              *object,
                                guint                 param_id,
                                GValue               *value,
                                GParamSpec           *pspec)
{
	GnomeCanvasRect *rect;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_RECT (object));

	rect = GNOME_CANVAS_RECT (object);

	switch (param_id) {
	case PROP_X1:
		g_value_set_double (value,  rect->x1);
		break;

	case PROP_Y1:
		g_value_set_double (value,  rect->y1);
		break;

	case PROP_X2:
		g_value_set_double (value,  rect->x2);
		break;

	case PROP_Y2:
		g_value_set_double (value,  rect->y2);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gnome_canvas_rect_update (GnomeCanvasItem *item, gdouble affine[6], ArtSVP *clip_path, gint flags)
{
        GnomeCanvasRect *rect;
	GnomeCanvasPathDef *path_def;

	rect = GNOME_CANVAS_RECT (item);

	if (rect->path_dirty) {
		path_def = gnome_canvas_path_def_new ();

		gnome_canvas_path_def_moveto (path_def, rect->x1, rect->y1);
		gnome_canvas_path_def_lineto (path_def, rect->x2, rect->y1);
		gnome_canvas_path_def_lineto (path_def, rect->x2, rect->y2);
		gnome_canvas_path_def_lineto (path_def, rect->x1, rect->y2);
		gnome_canvas_path_def_lineto (path_def, rect->x1, rect->y1);
		gnome_canvas_path_def_closepath_current (path_def);
		gnome_canvas_shape_set_path_def (GNOME_CANVAS_SHAPE (item), path_def);
		gnome_canvas_path_def_unref (path_def);
		rect->path_dirty = 0;
	}

	if (GNOME_CANVAS_ITEM_CLASS (gnome_canvas_rect_parent_class)->update)
		GNOME_CANVAS_ITEM_CLASS (gnome_canvas_rect_parent_class)->update (item, affine, clip_path, flags);
}
