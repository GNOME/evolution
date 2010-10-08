#define GNOME_CANVAS_CLIPGROUP_C

/* Clipping group for GnomeCanvas
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent canvas widget.  Tk is
 * copyrighted by the Regents of the University of California, Sun Microsystems, and other parties.
 *
 * Copyright (C) 1998,1999 The Free Software Foundation
 *
 * Author:
 *          Lauris Kaplinski <lauris@ximian.com>
 */

/* These includes are set up for standalone compile. If/when this codebase
   is integrated into libgnomeui, the includes will need to change. */

#include <math.h>
#include <string.h>

#include <gtk/gtk.h>

#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_rect.h>
#include <libart_lgpl/art_vpath.h>
#include <libart_lgpl/art_bpath.h>
#include <libart_lgpl/art_vpath.h>
#include <libart_lgpl/art_vpath_bpath.h>
#include <libart_lgpl/art_svp.h>
#include <libart_lgpl/art_svp_vpath.h>
#include <libart_lgpl/art_rect_svp.h>
#include <libart_lgpl/art_gray_svp.h>
#include <libart_lgpl/art_svp_intersect.h>
#include <libart_lgpl/art_svp_ops.h>

#include "gnome-canvas.h"
#include "gnome-canvas-util.h"
#include "gnome-canvas-clipgroup.h"

enum {
	PROP_0,
	PROP_PATH,
	PROP_WIND
};

static void gnome_canvas_clipgroup_class_init      (GnomeCanvasClipgroupClass *klass);
static void gnome_canvas_clipgroup_init            (GnomeCanvasClipgroup      *clipgroup);
static void gnome_canvas_clipgroup_destroy         (GnomeCanvasItem           *object);
static void gnome_canvas_clipgroup_set_property    (GObject                   *object,
                                                    guint                      param_id,
                                                    const GValue              *value,
                                                    GParamSpec                *pspec);
static void gnome_canvas_clipgroup_get_property    (GObject                   *object,
                                                    guint                      param_id,
                                                    GValue                    *value,
                                                    GParamSpec                *pspec);
static void gnome_canvas_clipgroup_update          (GnomeCanvasItem           *item,
                                                    gdouble                    *affine,
                                                    ArtSVP                    *clip_path,
                                                    gint                        flags);

/*
 * Generic clipping stuff
 *
 * This is somewhat slow and memory-hungry - we add extra
 * composition, extra SVP render and allocate 65536
 * bytes for each clip level. It could be done more
 * efficently per-object basis - but to make clipping
 * universal, there is no alternative to double
 * buffering (although it should be done into RGBA
 * buffer by other method than ::render to make global
 * opacity possible).
 * Using art-render could possibly optimize that a bit,
 * although I am not sure.
 */

static GnomeCanvasGroupClass *parent_class;

GType
gnome_canvas_clipgroup_get_type (void)
{
	static GType clipgroup_type;

	if (!clipgroup_type) {
		const GTypeInfo object_info = {
			sizeof (GnomeCanvasClipgroupClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gnome_canvas_clipgroup_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,			/* class_data */
			sizeof (GnomeCanvasClipgroup),
			0,			/* n_preallocs */
			(GInstanceInitFunc) gnome_canvas_clipgroup_init,
			NULL			/* value_table */
		};

		clipgroup_type = g_type_register_static (GNOME_TYPE_CANVAS_GROUP, "GnomeCanvasClipgroup",
							 &object_info, 0);
	}

	return clipgroup_type;
}

static void
gnome_canvas_clipgroup_class_init (GnomeCanvasClipgroupClass *klass)
{
        GObjectClass *gobject_class;
	GnomeCanvasItemClass *item_class;

        gobject_class = (GObjectClass *) klass;
	item_class = (GnomeCanvasItemClass *) klass;
	parent_class = g_type_class_ref (GNOME_TYPE_CANVAS_GROUP);

	gobject_class->set_property = gnome_canvas_clipgroup_set_property;
	gobject_class->get_property = gnome_canvas_clipgroup_get_property;
	item_class->destroy	    = gnome_canvas_clipgroup_destroy;
	item_class->update	    = gnome_canvas_clipgroup_update;

        g_object_class_install_property (gobject_class,
                                         PROP_PATH,
                                         g_param_spec_pointer ("path", NULL, NULL,
                                                               (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (gobject_class,
                                         PROP_WIND,
                                         g_param_spec_uint ("wind", NULL, NULL,
                                                            0, G_MAXUINT, 0,
                                                            (G_PARAM_READABLE | G_PARAM_WRITABLE)));
}

static void
gnome_canvas_clipgroup_init (GnomeCanvasClipgroup *clipgroup)
{
	clipgroup->path = NULL;
	clipgroup->wind = ART_WIND_RULE_NONZERO; /* default winding rule */
	clipgroup->svp = NULL;
}

static void
gnome_canvas_clipgroup_destroy (GnomeCanvasItem *object)
{
	GnomeCanvasClipgroup *clipgroup;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_CLIPGROUP (object));

	clipgroup = GNOME_CANVAS_CLIPGROUP (object);

	if (clipgroup->path) {
		gnome_canvas_path_def_unref (clipgroup->path);
		clipgroup->path = NULL;
	}

	if (clipgroup->svp) {
		art_svp_free (clipgroup->svp);
		clipgroup->svp = NULL;
	}

	if (GNOME_CANVAS_ITEM_CLASS (parent_class)->destroy)
		GNOME_CANVAS_ITEM_CLASS (parent_class)->destroy (object);
}

static void
gnome_canvas_clipgroup_set_property (GObject      *object,
                                     guint         param_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
	GnomeCanvasItem *item;
	GnomeCanvasClipgroup *cgroup;
	GnomeCanvasPathDef *gpp;

	item = GNOME_CANVAS_ITEM (object);
	cgroup = GNOME_CANVAS_CLIPGROUP (object);

	switch (param_id) {
	case PROP_PATH:
		gpp = g_value_get_pointer (value);

		if (cgroup->path) {
			gnome_canvas_path_def_unref (cgroup->path);
			cgroup->path = NULL;
		}
		if (gpp != NULL) {
			cgroup->path = gnome_canvas_path_def_closed_parts (gpp);
		}

		gnome_canvas_item_request_update (item);
		break;

	case PROP_WIND:
		cgroup->wind = g_value_get_uint (value);
		gnome_canvas_item_request_update (item);
		break;

	default:
		break;
	}
}

static void
gnome_canvas_clipgroup_get_property (GObject    *object,
                                     guint       param_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
	GnomeCanvasClipgroup * cgroup;

	cgroup = GNOME_CANVAS_CLIPGROUP (object);

	switch (param_id) {
	case PROP_PATH:
		g_value_set_pointer (value, cgroup->path);
		break;

	case PROP_WIND:
		g_value_set_uint (value, cgroup->wind);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gnome_canvas_clipgroup_update (GnomeCanvasItem *item, gdouble *affine, ArtSVP *clip_path, gint flags)
{
	GnomeCanvasClipgroup *clipgroup;
	ArtSvpWriter *swr;
	ArtBpath *bp;
	ArtBpath *bpath;
	ArtVpath *vpath;
	ArtSVP *svp, *svp1, *svp2;

	clipgroup = GNOME_CANVAS_CLIPGROUP (item);

	if (clipgroup->svp) {
		art_svp_free (clipgroup->svp);
		clipgroup->svp = NULL;
	}

	if (clipgroup->path) {
		bp = gnome_canvas_path_def_bpath (clipgroup->path);
		bpath = art_bpath_affine_transform (bp, affine);

		vpath = art_bez_path_to_vec (bpath, 0.25);
		art_free (bpath);

		svp1 = art_svp_from_vpath (vpath);
		art_free (vpath);

		swr = art_svp_writer_rewind_new (clipgroup->wind);
		art_svp_intersector (svp1, swr);

		svp2 = art_svp_writer_rewind_reap (swr);
		art_svp_free (svp1);

		if (clip_path != NULL) {
			svp = art_svp_intersect (svp2, clip_path);
			art_svp_free (svp2);
		} else {
			svp = svp2;
		}

		clipgroup->svp = svp;
	}

	if (GNOME_CANVAS_ITEM_CLASS (parent_class)->update)
		(GNOME_CANVAS_ITEM_CLASS (parent_class)->update) (item, affine, NULL, flags);

	if (clipgroup->svp) {
		ArtDRect cbox;
		art_drect_svp (&cbox, clipgroup->svp);
		item->x1 = MAX (item->x1, cbox.x0 - 1.0);
		item->y1 = MAX (item->y1, cbox.y0 - 1.0);
		item->x2 = MIN (item->x2, cbox.x1 + 1.0);
		item->y2 = MIN (item->y2, cbox.y1 + 1.0);
	}
}
