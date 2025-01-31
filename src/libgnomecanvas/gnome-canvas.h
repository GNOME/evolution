/*
 * Copyright (C) 1997, 1998, 1999, 2000 Free Software Foundation
 * All rights reserved.
 *
 * This file is part of the Gnome Library.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * published by the Free Software Foundation; either the version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser  General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 */
/*
  @NOTATION@
 */
/* GnomeCanvas widget - Tk-like canvas widget for Gnome
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent canvas
 * widget.  Tk is copyrighted by the Regents of the University of California,
 * Sun Microsystems, and other parties.
 *
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Raph Levien <raph@gimp.org>
 */

#ifndef GNOME_CANVAS_H
#define GNOME_CANVAS_H

#include <gtk/gtk.h>
#include <stdarg.h>

G_BEGIN_DECLS

/* "Small" value used by canvas stuff */
#define GNOME_CANVAS_EPSILON 1e-10

typedef struct _GnomeCanvas           GnomeCanvas;
typedef struct _GnomeCanvasClass      GnomeCanvasClass;
typedef struct _GnomeCanvasItem       GnomeCanvasItem;
typedef struct _GnomeCanvasItemClass  GnomeCanvasItemClass;
typedef struct _GnomeCanvasGroup      GnomeCanvasGroup;
typedef struct _GnomeCanvasGroupClass GnomeCanvasGroupClass;

/* GnomeCanvasItem - base item class for canvas items
 *
 * All canvas items are derived from GnomeCanvasItem.  The only information a
 * GnomeCanvasItem contains is its parent canvas, its parent canvas item group,
 * its bounding box in world coordinates, and its current affine transformation.
 *
 * Items inside a canvas are organized in a tree of GnomeCanvasItemGroup nodes
 * and GnomeCanvasItem leaves.  Each canvas has a single root group, which can
 * be obtained with the gnome_canvas_get_root() function.
 *
 * The abstract GnomeCanvasItem class does not have any configurable or
 * queryable attributes.
 */

/* Object flags for items */
typedef enum {
	GNOME_CANVAS_ITEM_REALIZED = 1 << 0,
	GNOME_CANVAS_ITEM_MAPPED = 1 << 1,
	GNOME_CANVAS_ITEM_VISIBLE = 1 << 2,
	GNOME_CANVAS_ITEM_NEED_UPDATE = 1 << 3,
	GNOME_CANVAS_ITEM_NEED_AFFINE = 1 << 4,
	GNOME_CANVAS_ITEM_NEED_CLIP = 1 << 5,
	GNOME_CANVAS_ITEM_NEED_VIS = 1 << 6
} GnomeCanvasItemFlags;

/* Update flags for items */
enum {
	GNOME_CANVAS_UPDATE_REQUESTED = 1 << 0,
	GNOME_CANVAS_UPDATE_AFFINE = 1 << 1,
	GNOME_CANVAS_UPDATE_CLIP = 1 << 2,
	GNOME_CANVAS_UPDATE_VISIBILITY = 1 << 3,
	GNOME_CANVAS_UPDATE_IS_VISIBLE = 1 << 4		/* Deprecated.  FIXME: remove this */
};

#define GNOME_TYPE_CANVAS_ITEM            (gnome_canvas_item_get_type ())
#define GNOME_CANVAS_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_CANVAS_ITEM, GnomeCanvasItem))
#define GNOME_CANVAS_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_TYPE_CANVAS_ITEM, GnomeCanvasItemClass))
#define GNOME_IS_CANVAS_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_CANVAS_ITEM))
#define GNOME_IS_CANVAS_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_CANVAS_ITEM))
#define GNOME_CANVAS_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_TYPE_CANVAS_ITEM, GnomeCanvasItemClass))

struct _GnomeCanvasItem {
	GInitiallyUnowned object;

	/* Parent canvas for this item */
	GnomeCanvas *canvas;

	/* Parent canvas group for this item (a GnomeCanvasGroup) */
	GnomeCanvasItem *parent;

	/* The transformation for this item */
	cairo_matrix_t matrix;

	/* Bounding box for this item (in canvas coordinates) */
	gdouble x1, y1, x2, y2;

	/* XXX GObject flags are sealed now, so we have to provide
	 *     our own.  This breaks ABI compatibility with upstream. */
	GnomeCanvasItemFlags flags;
};

struct _GnomeCanvasItemClass {
	GInitiallyUnownedClass parent_class;

	/* Tell the item to update itself.  The flags are from the update flags
	 * defined above.  The item should update its internal state from its
	 * queued state, and recompute and request its repaint area.  The
	 * affine, if used, is a pointer to a 6-element array of doubles.  The
	 * update method also recomputes the bounding box of the item.
	 */
	void (* update) (GnomeCanvasItem *item, const cairo_matrix_t *i2c, gint flags);

	/* Realize an item -- create GCs, etc. */
	void (* realize) (GnomeCanvasItem *item);

	/* Unrealize an item */
	void (* unrealize) (GnomeCanvasItem *item);

	/* Map an item - normally only need by items with their own GdkWindows */
	void (* map) (GnomeCanvasItem *item);

	/* Unmap an item */
	void (* unmap) (GnomeCanvasItem *item);

	/* Dispose item; called inside GObject's dispose of the base class */
	void (* dispose) (GnomeCanvasItem *item);

	/* Draw an item of this type.  (x, y) are the upper-left canvas pixel
	 * coordinates of the drawable, a temporary pixmap, where things get
	 * drawn.  (width, height) are the dimensions of the drawable.
	 */
	void (* draw) (GnomeCanvasItem *item, cairo_t *cr,
		       gint x, gint y, gint width, gint height);

        /* Returns the canvas item which is at the given location. This is the
         * item itself in the case of the object being an actual leaf item, or
         * a child in case of the object being a canvas group.  (cx, cy) are
         * the canvas pixel coordinates that correspond to the item-relative
         * coordinates (x, y).
	 */
	GnomeCanvasItem * (* point) (GnomeCanvasItem *item, gdouble x, gdouble y, gint cx, gint cy);

	/* Fetch the item's bounding box (need not be exactly tight).  This
	 * should be in item-relative coordinates.
	 */
	void (* bounds) (GnomeCanvasItem *item, gdouble *x1, gdouble *y1, gdouble *x2, gdouble *y2);

	/* Signal: an event occurred for an item of this type.  The (x, y)
	 * coordinates are in the canvas world coordinate system.
	 */
	gboolean (* event)                (GnomeCanvasItem *item, GdkEvent *event);

	/* Reserved for future expansion */
	gpointer spare_vmethods[4];
};

GType gnome_canvas_item_get_type (void) G_GNUC_CONST;

/* Create a canvas item using the standard Gtk argument mechanism.  The item is
 * automatically inserted at the top of the specified canvas group.  The last
 * argument must be a NULL pointer.
 */
GnomeCanvasItem *gnome_canvas_item_new (GnomeCanvasGroup *parent, GType type,
					const gchar *first_arg_name, ...);

/* Constructors for use in derived classes and language wrappers */
void gnome_canvas_item_construct (GnomeCanvasItem *item, GnomeCanvasGroup *parent,
				  const gchar *first_arg_name, va_list args);

/* Configure an item using the standard Gtk argument mechanism.  The last
 * argument must be a NULL pointer.
 */
void gnome_canvas_item_set (GnomeCanvasItem *item, const gchar *first_arg_name, ...) G_GNUC_NULL_TERMINATED;

/* Used only for language wrappers and the like */
void gnome_canvas_item_set_valist (GnomeCanvasItem *item,
				   const gchar *first_arg_name, va_list args);

/* Move an item by the specified amount */
void gnome_canvas_item_move (GnomeCanvasItem *item, gdouble dx, gdouble dy);

/* Apply a relative affine transformation to the item. */
void gnome_canvas_item_transform (GnomeCanvasItem *item, const cairo_matrix_t *matrix);

/* Apply an absolute affine transformation to the item. */
void gnome_canvas_item_set_matrix (GnomeCanvasItem *item, const cairo_matrix_t *matrix);

/* Raise an item in the z-order of its parent group by the specified number of
 * positions.
 */
void gnome_canvas_item_raise (GnomeCanvasItem *item, gint positions);

/* Lower an item in the z-order of its parent group by the specified number of
 * positions.
 */
void gnome_canvas_item_lower (GnomeCanvasItem *item, gint positions);

/* Raise an item to the top of its parent group's z-order. */
void gnome_canvas_item_raise_to_top (GnomeCanvasItem *item);

/* Lower an item to the bottom of its parent group's z-order */
void gnome_canvas_item_lower_to_bottom (GnomeCanvasItem *item);

/* Show an item (make it visible).  If the item is already shown, it has no
 * effect.
 */
void gnome_canvas_item_show (GnomeCanvasItem *item);

/* Hide an item (make it invisible).  If the item is already invisible, it has
 * no effect.
 */
void gnome_canvas_item_hide (GnomeCanvasItem *item);

/* Grab the mouse for the specified item.  Only the events in event_mask will be
 * reported.  If cursor is non-NULL, it will be used during the duration of the
 * grab.  Time is a proper X event time parameter.  Returns the same values as
 * XGrabPointer().
 */
gint gnome_canvas_item_grab (GnomeCanvasItem *item, guint event_mask,
			    GdkCursor *cursor, GdkDevice *device,
			    guint32 etime);

/* Ungrabs the mouse -- the specified item must be the same that was passed to
 * gnome_canvas_item_grab().  Time is a proper X event time parameter.
 */
void gnome_canvas_item_ungrab (GnomeCanvasItem *item, guint32 etime);

/* These functions convert from a coordinate system to another.  "w" is world
 * coordinates and "i" is item coordinates.
 */
void gnome_canvas_item_w2i (GnomeCanvasItem *item, gdouble *x, gdouble *y);
void gnome_canvas_item_i2w (GnomeCanvasItem *item, gdouble *x, gdouble *y);

/* Gets the affine transform that converts from item-relative coordinates to
 * world coordinates.
 */
void gnome_canvas_item_i2w_matrix (GnomeCanvasItem *item, cairo_matrix_t *matrix);
void gnome_canvas_item_w2i_matrix (GnomeCanvasItem *item, cairo_matrix_t *matrix);

/* Gets the affine transform that converts from item-relative coordinates to
 * canvas pixel coordinates.
 */
void gnome_canvas_item_i2c_matrix (GnomeCanvasItem *item, cairo_matrix_t *matrix);

/* Remove the item from its parent group and make the new group its parent.  The
 * item will be put on top of all the items in the new group.  The item's
 * coordinates relative to its new parent to *not* change -- this means that the
 * item could potentially move on the screen.
 *
 * The item and the group must be in the same canvas.  An item cannot be
 * reparented to a group that is the item itself or that is an inferior of the
 * item.
 */
void gnome_canvas_item_reparent (GnomeCanvasItem *item, GnomeCanvasGroup *new_group);

/* Used to send all of the keystroke events to a specific item as well as
 * GDK_FOCUS_CHANGE events.
 */
void gnome_canvas_item_grab_focus (GnomeCanvasItem *item);

/* Fetch the bounding box of the item.  The bounding box may not be exactly
 * tight, but the canvas items will do the best they can.  The returned bounding
 * box is in the coordinate system of the item's parent.
 */
void gnome_canvas_item_get_bounds (GnomeCanvasItem *item,
				   gdouble *x1, gdouble *y1, gdouble *x2, gdouble *y2);

/* Request that the update method eventually get called.  This should be used
 * only by item implementations.
 */
void gnome_canvas_item_request_update (GnomeCanvasItem *item);

/* GnomeCanvasGroup - a group of canvas items
 *
 * A group is a node in the hierarchical tree of groups/items inside a canvas.
 * Groups serve to give a logical structure to the items.
 *
 * Consider a circuit editor application that uses the canvas for its schematic
 * display.  Hierarchically, there would be canvas groups that contain all the
 * components needed for an "adder", for example -- this includes some logic
 * gates as well as wires.  You can move stuff around in a convenient way by
 * doing a gnome_canvas_item_move() of the hierarchical groups -- to move an
 * adder, simply move the group that represents the adder.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 * x		gdouble		RW		X coordinate of group's origin
 * y		gdouble		RW		Y coordinate of group's origin
 */

#define GNOME_TYPE_CANVAS_GROUP            (gnome_canvas_group_get_type ())
#define GNOME_CANVAS_GROUP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_CANVAS_GROUP, GnomeCanvasGroup))
#define GNOME_CANVAS_GROUP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_TYPE_CANVAS_GROUP, GnomeCanvasGroupClass))
#define GNOME_IS_CANVAS_GROUP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_CANVAS_GROUP))
#define GNOME_IS_CANVAS_GROUP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_CANVAS_GROUP))
#define GNOME_CANVAS_GROUP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_TYPE_CANVAS_GROUP, GnomeCanvasGroupClass))

struct _GnomeCanvasGroup {
	GnomeCanvasItem item;

	/* Children of the group */
	GList *item_list;
	GList *item_list_end;
};

struct _GnomeCanvasGroupClass {
	GnomeCanvasItemClass parent_class;
};

GType gnome_canvas_group_get_type (void) G_GNUC_CONST;

/*** GnomeCanvas ***/

#define GNOME_TYPE_CANVAS            (gnome_canvas_get_type ())
#define GNOME_CANVAS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_CANVAS, GnomeCanvas))
#define GNOME_CANVAS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_TYPE_CANVAS, GnomeCanvasClass))
#define GNOME_IS_CANVAS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_CANVAS))
#define GNOME_IS_CANVAS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_CANVAS))
#define GNOME_CANVAS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_TYPE_CANVAS, GnomeCanvasClass))

struct _GnomeCanvas {
	GtkLayout layout;

	/* Root canvas group */
	GnomeCanvasItem *root;

	/* The item containing the mouse pointer, or NULL if none */
	GnomeCanvasItem *current_item;

	/* Item that is about to become current (used to track deletions and such) */
	GnomeCanvasItem *new_current_item;

	/* Item that holds a pointer grab, or NULL if none */
	GnomeCanvasItem *grabbed_item;

	/* The grabbed device for grabbed_item. */
	GdkDevice *grabbed_device;

	/* If non-NULL, the currently focused item */
	GnomeCanvasItem *focused_item;

	/* Event on which selection of current item is based */
	GdkEvent pick_event;

	/* Scrolling region */
	gdouble scroll_x1, scroll_y1;
	gdouble scroll_x2, scroll_y2;

	/* Idle handler ID */
	guint idle_id;

	/* Offsets of the temprary drawing pixmap */
	gint draw_xofs, draw_yofs;

	/* Internal pixel offsets when zoomed out */
	gint zoom_xofs, zoom_yofs;

	/* Last known modifier state, for deferred repick when a button is down */
	gint state;

	/* Event mask specified when grabbing an item */
	guint grabbed_event_mask;

	/* Whether items need update at next idle loop iteration */
	guint need_update : 1;

	/* Whether current item will be repicked at next idle loop iteration */
	guint need_repick : 1;

	/* For use by internal pick_current_item() function */
	guint left_grabbed_item : 1;

	/* For use by internal pick_current_item() function */
	guint in_repick : 1;
};

struct _GnomeCanvasClass {
	GtkLayoutClass parent_class;

	/* Draw the background for the area given. This method is only used
	 * for non-antialiased canvases.
	 */
	void (* draw_background) (GnomeCanvas *canvas, cairo_t *cr,
				  gint x, gint y, gint width, gint height);

	/* Private Virtual methods for groping the canvas inside bonobo */
	void (* request_update) (GnomeCanvas *canvas);

	/* Reserved for future expansion */
	gpointer spare_vmethods[4];
};

GType gnome_canvas_get_type (void) G_GNUC_CONST;

/* Creates a new canvas.  You should check that the canvas is created with the
 * proper visual and colormap.  Any visual will do unless you intend to insert
 * gdk_imlib images into it, in which case you should use the gdk_imlib visual.
 *
 * You should call gnome_canvas_set_scroll_region() soon after calling this
 * function to set the desired scrolling limits for the canvas.
 */
GtkWidget *gnome_canvas_new (void);

/* Returns the root canvas item group of the canvas */
GnomeCanvasGroup *gnome_canvas_root (GnomeCanvas *canvas);

/* Sets the limits of the scrolling region, in world coordinates */
void gnome_canvas_set_scroll_region (GnomeCanvas *canvas,
				     gdouble x1, gdouble y1, gdouble x2, gdouble y2);

/* Gets the limits of the scrolling region, in world coordinates */
void gnome_canvas_get_scroll_region (GnomeCanvas *canvas,
				     gdouble *x1, gdouble *y1, gdouble *x2, gdouble *y2);

/* Scrolls the canvas to the specified offsets, given in canvas pixel coordinates */
void gnome_canvas_scroll_to (GnomeCanvas *canvas, gint cx, gint cy);

/* Returns the scroll offsets of the canvas in canvas pixel coordinates.  You
 * can specify NULL for any of the values, in which case that value will not be
 * queried.
 */
void gnome_canvas_get_scroll_offsets (GnomeCanvas *canvas, gint *cx, gint *cy);

/* Returns the item that is at the specified position in world coordinates, or
 * NULL if no item is there.
 */
GnomeCanvasItem *gnome_canvas_get_item_at (GnomeCanvas *canvas, gdouble x, gdouble y);

/* For use only by item type implementations.  Request that the canvas
 * eventually redraw the specified region, specified in canvas pixel
 * coordinates.  The region contains (x1, y1) but not (x2, y2).
 */
void gnome_canvas_request_redraw (GnomeCanvas *canvas, gint x1, gint y1, gint x2, gint y2);

/* Gets the affine transform that converts world coordinates into canvas pixel
 * coordinates.
 */
void gnome_canvas_w2c_matrix (GnomeCanvas *canvas, cairo_matrix_t *matrix);
void gnome_canvas_c2w_matrix (GnomeCanvas *canvas, cairo_matrix_t *matrix);

/* These functions convert from a coordinate system to another.  "w" is world
 * coordinates, "c" is canvas pixel coordinates (pixel coordinates that are
 * (0,0) for the upper-left scrolling limit and something else for the
 * lower-left scrolling limit).
 */
void gnome_canvas_w2c (GnomeCanvas *canvas, gdouble wx, gdouble wy, gint *cx, gint *cy);
void gnome_canvas_w2c_d (GnomeCanvas *canvas, gdouble wx, gdouble wy, gdouble *cx, gdouble *cy);
void gnome_canvas_c2w (GnomeCanvas *canvas, gint cx, gint cy, gdouble *wx, gdouble *wy);

/* This function takes in coordinates relative to the GTK_LAYOUT
 * (canvas)->bin_window and converts them to world coordinates.
 */
void gnome_canvas_window_to_world (GnomeCanvas *canvas,
				   gdouble winx, gdouble winy, gdouble *worldx, gdouble *worldy);

/* This is the inverse of gnome_canvas_window_to_world() */
void gnome_canvas_world_to_window (GnomeCanvas *canvas,
				   gdouble worldx, gdouble worldy, gdouble *winx, gdouble *winy);

G_END_DECLS

#endif
