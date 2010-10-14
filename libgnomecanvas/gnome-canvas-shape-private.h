#ifndef GNOME_CANVAS_SHAPE_PRIVATE_H
#define GNOME_CANVAS_SHAPE_PRIVATE_H

/* Bpath item type for GnomeCanvas widget
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent canvas widget.  Tk is
 * copyrighted by the Regents of the University of California, Sun Microsystems, and other parties.
 *
 * Copyright (C) 1998,1999 The Free Software Foundation
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Raph Levien <raph@acm.org>
 *          Lauris Kaplinski <lauris@ariman.ee>
 */

#include <gdk/gdk.h>
#include <libgnomecanvas/gnome-canvas.h>

G_BEGIN_DECLS

struct _GnomeCanvasShapePriv {
	cairo_path_t *path;             /* Our bezier path representation */

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
	double *dash;     		/* Dashing pattern */
        double dash_offset;             /* Dashing offset */
};

G_END_DECLS

#endif
