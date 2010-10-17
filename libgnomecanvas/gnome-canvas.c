/* -*- Mode: C; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */
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
/*
 * GnomeCanvas widget - Tk-like canvas widget for Gnome
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent canvas
 * widget.  Tk is copyrighted by the Regents of the University of California,
 * Sun Microsystems, and other parties.
 *
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Raph Levien <raph@gimp.org>
 */

/*
 * TO-DO list for the canvas:
 *
 * - Allow to specify whether GnomeCanvasImage sizes are in units or pixels
 *   (scale or don't scale).
 *
 * - Implement a flag for gnome_canvas_item_reparent() that tells the function
 *   to keep the item visually in the same place, that is, to keep it in the
 *   same place with respect to the canvas origin.
 *
 * - GC put functions for items.
 *
 * - Widget item (finish it).
 *
 * - GList *
 *   gnome_canvas_gimme_all_items_contained_in_this_area (GnomeCanvas *canvas,
 *                                                        Rectangle area);
 *
 * - Retrofit all the primitive items with microtile support.
 *
 * - Curve support for line item.
 *
 * - Arc item (Havoc has it; to be integrated in GnomeCanvasEllipse).
 *
 * - Sane font handling API.
 *
 * - Get_arg methods for items:
 *   - How to fetch the outline width and know whether it is in pixels or units?
 */

/*
 * Raph's TODO list for the antialiased canvas integration:
 *
 * - ::point() method for text item not accurate when affine transformed.
 *
 * - Clip rectangle not implemented in aa renderer for text item.
 *
 * - Clip paths only partially implemented.
 *
 * - Add more image loading techniques to work around imlib deficiencies.
 */

#include <config.h>

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <gdk/gdkprivate.h>
#include <gtk/gtk.h>
#include "gailcanvas.h"
#include "gnome-canvas.h"
#include "gnome-canvas-i18n.h"
#include "gnome-canvas-util.h"
#include <libart_lgpl/art_rect.h>
#include <libart_lgpl/art_rect_uta.h>
#include <libart_lgpl/art_uta_rect.h>

#include "gnome-canvas-marshal.h"
#include "gnome-canvas-marshal.c"

/* backward-compatibility cruft */
#include "e-util/gtk-compat.h"

/* We must run our idle update handler *before* GDK wants to redraw. */
#define CANVAS_IDLE_PRIORITY (GDK_PRIORITY_REDRAW - 5)

static void gnome_canvas_request_update (GnomeCanvas      *canvas);
static void group_add                   (GnomeCanvasGroup *group,
					 GnomeCanvasItem  *item);
static void group_remove                (GnomeCanvasGroup *group,
					 GnomeCanvasItem  *item);
static void add_idle                    (GnomeCanvas      *canvas);

/*** GnomeCanvasItem ***/

/* Some convenience stuff */
#define GCI_UPDATE_MASK \
	(GNOME_CANVAS_UPDATE_REQUESTED | \
	 GNOME_CANVAS_UPDATE_AFFINE | \
	 GNOME_CANVAS_UPDATE_CLIP | \
	 GNOME_CANVAS_UPDATE_VISIBILITY)
#define GCI_EPSILON 1e-18
#define GCI_PRINT_MATRIX(s,a) \
	g_print ("%s %g %g %g %g %g %g\n", \
	s, (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5])

enum {
	ITEM_PROP_0,
	ITEM_PROP_PARENT
};

enum {
	ITEM_EVENT,
	ITEM_LAST_SIGNAL
};

static void gnome_canvas_item_class_init     (GnomeCanvasItemClass *class);
static void gnome_canvas_item_init           (GnomeCanvasItem      *item);
static gint  emit_event                       (GnomeCanvas *canvas, GdkEvent *event);

static guint item_signals[ITEM_LAST_SIGNAL];

static GObjectClass *item_parent_class;

/**
 * gnome_canvas_item_get_type:
 *
 * Registers the &GnomeCanvasItem class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value:  The type ID of the &GnomeCanvasItem class.
 **/
GType
gnome_canvas_item_get_type (void)
{
	static GType canvas_item_type;

	if (!canvas_item_type) {
		const GTypeInfo object_info = {
			sizeof (GnomeCanvasItemClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gnome_canvas_item_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,			/* class_data */
			sizeof (GnomeCanvasItem),
			0,			/* n_preallocs */
			(GInstanceInitFunc) gnome_canvas_item_init,
			NULL			/* value_table */
		};

		canvas_item_type = g_type_register_static (G_TYPE_INITIALLY_UNOWNED, "GnomeCanvasItem",
							   &object_info, 0);
	}

	return canvas_item_type;
}

/* Object initialization function for GnomeCanvasItem */
static void
gnome_canvas_item_init (GnomeCanvasItem *item)
{
	item->flags |= GNOME_CANVAS_ITEM_VISIBLE;

        cairo_matrix_init_identity (&item->matrix);
}

/**
 * gnome_canvas_item_new:
 * @parent: The parent group for the new item.
 * @type: The object type of the item.
 * @first_arg_name: A list of object argument name/value pairs, NULL-terminated,
 * used to configure the item.  For example, "fill_color", "black",
 * "width_units", 5.0, NULL.
 * @Varargs:
 *
 * Creates a new canvas item with @parent as its parent group.  The item is
 * created at the top of its parent's stack, and starts up as visible.  The item
 * is of the specified @type, for example, it can be
 * gnome_canvas_rect_get_type().  The list of object arguments/value pairs is
 * used to configure the item. If you need to pass construct time parameters, you
 * should use g_object_new() to pass the parameters and
 * gnome_canvas_item_construct() to set up the canvas item.
 *
 * Return value: The newly-created item.
 **/
GnomeCanvasItem *
gnome_canvas_item_new (GnomeCanvasGroup *parent,
                       GType type,
                       const gchar *first_arg_name, ...)
{
	GnomeCanvasItem *item;
	va_list args;

	g_return_val_if_fail (GNOME_IS_CANVAS_GROUP (parent), NULL);
	g_return_val_if_fail (g_type_is_a (type, gnome_canvas_item_get_type ()), NULL);

	item = GNOME_CANVAS_ITEM (g_object_new (type, NULL));

	va_start (args, first_arg_name);
	gnome_canvas_item_construct (item, parent, first_arg_name, args);
	va_end (args);

	return item;
}

/* Performs post-creation operations on a canvas item (adding it to its parent
 * group, etc.)
 */
static void
item_post_create_setup (GnomeCanvasItem *item)
{
	group_add (GNOME_CANVAS_GROUP (item->parent), item);

	gnome_canvas_request_redraw (
		item->canvas, item->x1, item->y1, item->x2 + 1, item->y2 + 1);
	item->canvas->need_repick = TRUE;
}

/* Set_property handler for canvas items */
static void
gnome_canvas_item_set_property (GObject *gobject, guint param_id,
				const GValue *value, GParamSpec *pspec)
{
	GnomeCanvasItem *item;

	g_return_if_fail (GNOME_IS_CANVAS_ITEM (gobject));

	item = GNOME_CANVAS_ITEM (gobject);

	switch (param_id) {
	case ITEM_PROP_PARENT:
		if (item->parent != NULL) {
		    g_warning ("Cannot set `parent' argument after item has "
			       "already been constructed.");
		} else if (g_value_get_object (value)) {
			item->parent = GNOME_CANVAS_ITEM (g_value_get_object (value));
			item->canvas = item->parent->canvas;
			item_post_create_setup (item);
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, param_id, pspec);
		break;
	}
}

/* Get_property handler for canvas items */
static void
gnome_canvas_item_get_property (GObject *gobject, guint param_id,
				GValue *value, GParamSpec *pspec)
{
	GnomeCanvasItem *item;

	g_return_if_fail (GNOME_IS_CANVAS_ITEM (gobject));

	item = GNOME_CANVAS_ITEM (gobject);

	switch (param_id) {
	case ITEM_PROP_PARENT:
		g_value_set_object (value, item->parent);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, param_id, pspec);
		break;
	}
}

/**
 * gnome_canvas_item_construct:
 * @item: An unconstructed canvas item.
 * @parent: The parent group for the item.
 * @first_arg_name: The name of the first argument for configuring the item.
 * @args: The list of arguments used to configure the item.
 *
 * Constructs a canvas item; meant for use only by item implementations.
 **/
void
gnome_canvas_item_construct (GnomeCanvasItem *item, GnomeCanvasGroup *parent,
			     const gchar *first_arg_name, va_list args)
{
	g_return_if_fail (GNOME_IS_CANVAS_GROUP (parent));
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));

	item->parent = GNOME_CANVAS_ITEM (parent);
	item->canvas = item->parent->canvas;

	g_object_set_valist (G_OBJECT (item), first_arg_name, args);

	item_post_create_setup (item);
}

/* If the item is visible, requests a redraw of it. */
static void
redraw_if_visible (GnomeCanvasItem *item)
{
	if (item->flags & GNOME_CANVAS_ITEM_VISIBLE)
		gnome_canvas_request_redraw (
			item->canvas, item->x1, item->y1,
			item->x2 + 1, item->y2 + 1);
}

/* Standard object dispose function for canvas items */
static void
gnome_canvas_item_dispose (GObject *object)
{
	GnomeCanvasItem *item;

	g_return_if_fail (GNOME_IS_CANVAS_ITEM (object));

	item = GNOME_CANVAS_ITEM (object);

	if (item->canvas)
		redraw_if_visible (item);

	/* Make the canvas forget about us */

	if (item->canvas && item == item->canvas->current_item) {
		item->canvas->current_item = NULL;
		item->canvas->need_repick = TRUE;
	}

	if (item->canvas && item == item->canvas->new_current_item) {
		item->canvas->new_current_item = NULL;
		item->canvas->need_repick = TRUE;
	}

	if (item->canvas && item == item->canvas->grabbed_item) {
		item->canvas->grabbed_item = NULL;
		gdk_pointer_ungrab (GDK_CURRENT_TIME);
	}

	if (item->canvas && item == item->canvas->focused_item)
		item->canvas->focused_item = NULL;

	/* Normal destroy stuff */

	if (item->flags & GNOME_CANVAS_ITEM_MAPPED)
		(* GNOME_CANVAS_ITEM_GET_CLASS (item)->unmap) (item);

	if (item->flags & GNOME_CANVAS_ITEM_REALIZED)
		(* GNOME_CANVAS_ITEM_GET_CLASS (item)->unrealize) (item);

	if (item->parent)
		group_remove (GNOME_CANVAS_GROUP (item->parent), item);

	if (GNOME_CANVAS_ITEM_GET_CLASS (item)->destroy)
		GNOME_CANVAS_ITEM_GET_CLASS (item)->destroy (item);

	G_OBJECT_CLASS (item_parent_class)->dispose (object);
	/* items should remove any reference to item->canvas after the
	   first ::destroy */
	item->canvas = NULL;
}

/* Realize handler for canvas items */
static void
gnome_canvas_item_realize (GnomeCanvasItem *item)
{
	item->flags |= GNOME_CANVAS_ITEM_REALIZED;

	gnome_canvas_item_request_update (item);
}

/* Unrealize handler for canvas items */
static void
gnome_canvas_item_unrealize (GnomeCanvasItem *item)
{
	item->flags &= ~GNOME_CANVAS_ITEM_REALIZED;
}

/* Map handler for canvas items */
static void
gnome_canvas_item_map (GnomeCanvasItem *item)
{
	item->flags |= GNOME_CANVAS_ITEM_MAPPED;
}

/* Unmap handler for canvas items */
static void
gnome_canvas_item_unmap (GnomeCanvasItem *item)
{
	item->flags &= ~GNOME_CANVAS_ITEM_MAPPED;
}

/* Update handler for canvas items */
static void
gnome_canvas_item_update (GnomeCanvasItem *item,
                          const cairo_matrix_t *matrix,
                          gint flags)
{
	item->flags &= ~GNOME_CANVAS_ITEM_NEED_UPDATE;
	item->flags &= ~GNOME_CANVAS_ITEM_NEED_AFFINE;
	item->flags &= ~GNOME_CANVAS_ITEM_NEED_CLIP;
	item->flags &= ~GNOME_CANVAS_ITEM_NEED_VIS;
}

/*
 * This routine invokes the update method of the item
 * Please notice, that we take parent to canvas pixel matrix as argument
 * unlike virtual method ::update, whose argument is item 2 canvas pixel
 * matrix
 *
 * I will try to force somewhat meaningful naming for affines (Lauris)
 * General naming rule is FROM2TO, where FROM and TO are abbreviations
 * So p2cpx is Parent2CanvasPixel and i2cpx is Item2CanvasPixel
 * I hope that this helps to keep track of what really happens
 *
 */

static void
gnome_canvas_item_invoke_update (GnomeCanvasItem *item,
                                 const cairo_matrix_t *p2c,
                                 gint flags)
{
	gint child_flags;
	cairo_matrix_t i2c;

	child_flags = flags;
	if (!(item->flags & GNOME_CANVAS_ITEM_VISIBLE))
		child_flags &= ~GNOME_CANVAS_UPDATE_IS_VISIBLE;

	/* Calculate actual item transformation matrix */

        cairo_matrix_multiply (&i2c, &item->matrix, p2c);

	/* apply object flags to child flags */

	child_flags &= ~GNOME_CANVAS_UPDATE_REQUESTED;

	if (item->flags & GNOME_CANVAS_ITEM_NEED_UPDATE)
		child_flags |= GNOME_CANVAS_UPDATE_REQUESTED;

	if (item->flags & GNOME_CANVAS_ITEM_NEED_AFFINE)
		child_flags |= GNOME_CANVAS_UPDATE_AFFINE;

	if (item->flags & GNOME_CANVAS_ITEM_NEED_CLIP)
		child_flags |= GNOME_CANVAS_UPDATE_CLIP;

	if (item->flags & GNOME_CANVAS_ITEM_NEED_VIS)
		child_flags |= GNOME_CANVAS_UPDATE_VISIBILITY;

	if (child_flags & GCI_UPDATE_MASK) {
		if (GNOME_CANVAS_ITEM_GET_CLASS (item)->update)
			GNOME_CANVAS_ITEM_GET_CLASS (item)->update (item, &i2c, child_flags);
	}
}

/*
 * This routine invokes the point method of the item.
 * The arguments x, y should be in the parent item local coordinates.
 *
 * This is potentially evil, as we are relying on matrix inversion (Lauris)
 */

static GnomeCanvasItem *
gnome_canvas_item_invoke_point (GnomeCanvasItem *item,
                                gdouble x,
                                gdouble y,
                                gint cx,
                                gint cy)
{
        cairo_matrix_t inverse;

	/* Calculate x & y in item local coordinates */
        inverse = item->matrix;
        if (cairo_matrix_invert (&inverse) != CAIRO_STATUS_SUCCESS)
                return NULL;

        cairo_matrix_transform_point (&inverse, &x, &y);

	if (GNOME_CANVAS_ITEM_GET_CLASS (item)->point)
		return GNOME_CANVAS_ITEM_GET_CLASS (item)->point (item, x, y, cx, cy);

	return NULL;
}

/**
 * gnome_canvas_item_set:
 * @item: A canvas item.
 * @first_arg_name: The list of object argument name/value pairs used to
 *                  configure the item.
 * @Varargs:
 *
 * Configures a canvas item.  The arguments in the item are set to the
 * specified values, and the item is repainted as appropriate.
 **/
void
gnome_canvas_item_set (GnomeCanvasItem *item, const gchar *first_arg_name, ...)
{
	va_list args;

	va_start (args, first_arg_name);
	gnome_canvas_item_set_valist (item, first_arg_name, args);
	va_end (args);
}

/**
 * gnome_canvas_item_set_valist:
 * @item: A canvas item.
 * @first_arg_name: The name of the first argument used to configure the item.
 * @args: The list of object argument name/value pairs used to configure the item.
 *
 * Configures a canvas item.  The arguments in the item are set to the specified
 * values, and the item is repainted as appropriate.
 **/
void
gnome_canvas_item_set_valist (GnomeCanvasItem *item,
                              const gchar *first_arg_name,
                              va_list args)
{
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));

	g_object_set_valist (G_OBJECT (item), first_arg_name, args);

	item->canvas->need_repick = TRUE;
}

/**
 * gnome_canvas_item_transform:
 * @item: A canvas item.
 * @matrix: An affine transformation matrix.
 *
 * Combines the specified affine transformation matrix with the item's current
 * transformation.
 **/
void
gnome_canvas_item_transform (GnomeCanvasItem *item, const cairo_matrix_t *matrix)
{
	cairo_matrix_t i2p;

	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));
	g_return_if_fail (matrix != NULL);

	/* Calculate actual item transformation matrix */
	cairo_matrix_multiply (&i2p, matrix, &item->matrix);

	gnome_canvas_item_set_matrix (item, &i2p);
}

/**
 * gnome_canvas_item_set_matrix:
 * @item: A canvas item.
 * @matrix: An affine transformation matrix or %NULL for the identity matrix.
 *
 * Makes the item's affine transformation matrix be equal to the specified
 * matrix. NULL is treated as identity.
 **/
void
gnome_canvas_item_set_matrix (GnomeCanvasItem *item, const cairo_matrix_t *matrix)
{
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));

	if (matrix) {
                item->matrix = *matrix;
	} else {
                cairo_matrix_init_identity (&item->matrix);
	}

	if (!(item->flags & GNOME_CANVAS_ITEM_NEED_AFFINE)) {
		/* Request update */
		item->flags |= GNOME_CANVAS_ITEM_NEED_AFFINE;
		gnome_canvas_item_request_update (item);
	}

	item->canvas->need_repick = TRUE;
}

/**
 * gnome_canvas_item_move:
 * @item: A canvas item.
 * @dx: Horizontal offset.
 * @dy: Vertical offset.
 *
 * Moves a canvas item by creating an affine transformation matrix for
 * translation by using the specified values. This happens in item
 * local coordinate system, so if you have nontrivial transform, it
 * most probably does not do, what you want.
 **/
void
gnome_canvas_item_move (GnomeCanvasItem *item, gdouble dx, gdouble dy)
{
	cairo_matrix_t translate;

	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));

	cairo_matrix_init_translate (&translate, dx, dy);

	gnome_canvas_item_transform (item, &translate);
}

/* Convenience function to reorder items in a group's child list.  This puts the
 * specified link after the "before" link. Returns TRUE if the list was changed.
 */
static gboolean
put_item_after (GList *link, GList *before)
{
	GnomeCanvasGroup *parent;
	GList *old_before, *old_after;
	GList *after;

	parent = GNOME_CANVAS_GROUP (GNOME_CANVAS_ITEM (link->data)->parent);

	if (before)
		after = before->next;
	else
		after = parent->item_list;

	if (before == link || after == link)
		return FALSE;

	/* Unlink */

	old_before = link->prev;
	old_after = link->next;

	if (old_before)
		old_before->next = old_after;
	else
		parent->item_list = old_after;

	if (old_after)
		old_after->prev = old_before;
	else
		parent->item_list_end = old_before;

	/* Relink */

	link->prev = before;
	if (before)
		before->next = link;
	else
		parent->item_list = link;

	link->next = after;
	if (after)
		after->prev = link;
	else
		parent->item_list_end = link;

	return TRUE;
}

/**
 * gnome_canvas_item_raise:
 * @item: A canvas item.
 * @positions: Number of steps to raise the item.
 *
 * Raises the item in its parent's stack by the specified number of positions.
 * If the number of positions is greater than the distance to the top of the
 * stack, then the item is put at the top.
 **/
void
gnome_canvas_item_raise (GnomeCanvasItem *item, gint positions)
{
	GList *link, *before;
	GnomeCanvasGroup *parent;

	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));
	g_return_if_fail (positions >= 0);

	if (!item->parent || positions == 0)
		return;

	parent = GNOME_CANVAS_GROUP (item->parent);
	link = g_list_find (parent->item_list, item);
	g_assert (link != NULL);

	for (before = link; positions && before; positions--)
		before = before->next;

	if (!before)
		before = parent->item_list_end;

	if (put_item_after (link, before)) {
		redraw_if_visible (item);
		item->canvas->need_repick = TRUE;
	}
}

/**
 * gnome_canvas_item_lower:
 * @item: A canvas item.
 * @positions: Number of steps to lower the item.
 *
 * Lowers the item in its parent's stack by the specified number of positions.
 * If the number of positions is greater than the distance to the bottom of the
 * stack, then the item is put at the bottom.
 **/
void
gnome_canvas_item_lower (GnomeCanvasItem *item, gint positions)
{
	GList *link, *before;
	GnomeCanvasGroup *parent;

	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));
	g_return_if_fail (positions >= 1);

	if (!item->parent || positions == 0)
		return;

	parent = GNOME_CANVAS_GROUP (item->parent);
	link = g_list_find (parent->item_list, item);
	g_assert (link != NULL);

	if (link->prev)
		for (before = link->prev; positions && before; positions--)
			before = before->prev;
	else
		before = NULL;

	if (put_item_after (link, before)) {
		redraw_if_visible (item);
		item->canvas->need_repick = TRUE;
	}
}

/**
 * gnome_canvas_item_raise_to_top:
 * @item: A canvas item.
 *
 * Raises an item to the top of its parent's stack.
 **/
void
gnome_canvas_item_raise_to_top (GnomeCanvasItem *item)
{
	GList *link;
	GnomeCanvasGroup *parent;

	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));

	if (!item->parent)
		return;

	parent = GNOME_CANVAS_GROUP (item->parent);
	link = g_list_find (parent->item_list, item);
	g_assert (link != NULL);

	if (put_item_after (link, parent->item_list_end)) {
		redraw_if_visible (item);
		item->canvas->need_repick = TRUE;
	}
}

/**
 * gnome_canvas_item_lower_to_bottom:
 * @item: A canvas item.
 *
 * Lowers an item to the bottom of its parent's stack.
 **/
void
gnome_canvas_item_lower_to_bottom (GnomeCanvasItem *item)
{
	GList *link;
	GnomeCanvasGroup *parent;

	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));

	if (!item->parent)
		return;

	parent = GNOME_CANVAS_GROUP (item->parent);
	link = g_list_find (parent->item_list, item);
	g_assert (link != NULL);

	if (put_item_after (link, NULL)) {
		redraw_if_visible (item);
		item->canvas->need_repick = TRUE;
	}
}

/**
 * gnome_canvas_item_show:
 * @item: A canvas item.
 *
 * Shows a canvas item.  If the item was already shown, then no action is taken.
 **/
void
gnome_canvas_item_show (GnomeCanvasItem *item)
{
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));

	if (!(item->flags & GNOME_CANVAS_ITEM_VISIBLE)) {
		item->flags |= GNOME_CANVAS_ITEM_VISIBLE;
		gnome_canvas_request_redraw (
			item->canvas, item->x1, item->y1,
			item->x2 + 1, item->y2 + 1);
		item->canvas->need_repick = TRUE;
	}
}

/**
 * gnome_canvas_item_hide:
 * @item: A canvas item.
 *
 * Hides a canvas item.  If the item was already hidden, then no action is
 * taken.
 **/
void
gnome_canvas_item_hide (GnomeCanvasItem *item)
{
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));

	if (item->flags & GNOME_CANVAS_ITEM_VISIBLE) {
		item->flags &= ~GNOME_CANVAS_ITEM_VISIBLE;
		gnome_canvas_request_redraw (
			item->canvas, item->x1, item->y1,
			item->x2 + 1, item->y2 + 1);
		item->canvas->need_repick = TRUE;
	}
}

/**
 * gnome_canvas_item_grab:
 * @item: A canvas item.
 * @event_mask: Mask of events that will be sent to this item.
 * @cursor: If non-NULL, the cursor that will be used while the grab is active.
 * @etime: The timestamp required for grabbing the mouse, or GDK_CURRENT_TIME.
 *
 * Specifies that all events that match the specified event mask should be sent
 * to the specified item, and also grabs the mouse by calling
 * gdk_pointer_grab().  The event mask is also used when grabbing the pointer.
 * If @cursor is not NULL, then that cursor is used while the grab is active.
 * The @etime parameter is the timestamp required for grabbing the mouse.
 *
 * Return value: If an item was already grabbed, it returns
 * %GDK_GRAB_ALREADY_GRABBED.  If the specified item was hidden by calling
 * gnome_canvas_item_hide(), then it returns %GDK_GRAB_NOT_VIEWABLE.  Else,
 * it returns the result of calling gdk_pointer_grab().
 **/
gint
gnome_canvas_item_grab (GnomeCanvasItem *item,
                        guint event_mask,
                        GdkCursor *cursor,
                        guint32 etime)
{
	GtkLayout *layout;
	GdkWindow *bin_window;
	gint retval;

	g_return_val_if_fail (
		GNOME_IS_CANVAS_ITEM (item), GDK_GRAB_NOT_VIEWABLE);
	g_return_val_if_fail (
		gtk_widget_get_mapped (GTK_WIDGET (item->canvas)),
		GDK_GRAB_NOT_VIEWABLE);

	if (item->canvas->grabbed_item)
		return GDK_GRAB_ALREADY_GRABBED;

	if (!(item->flags & GNOME_CANVAS_ITEM_VISIBLE))
		return GDK_GRAB_NOT_VIEWABLE;

	layout = GTK_LAYOUT (item->canvas);
	bin_window = gtk_layout_get_bin_window (layout);

	retval = gdk_pointer_grab (bin_window,
				   FALSE,
				   event_mask,
				   NULL,
				   cursor,
				   etime);

	if (retval != GDK_GRAB_SUCCESS)
		return retval;

	item->canvas->grabbed_item = item;
	item->canvas->grabbed_event_mask = event_mask;
	item->canvas->current_item = item; /* So that events go to the grabbed item */

	return retval;
}

/**
 * gnome_canvas_item_ungrab:
 * @item: A canvas item that holds a grab.
 * @etime: The timestamp for ungrabbing the mouse.
 *
 * Ungrabs the item, which must have been grabbed in the canvas, and ungrabs the
 * mouse.
 **/
void
gnome_canvas_item_ungrab (GnomeCanvasItem *item, guint32 etime)
{
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));

	if (item->canvas->grabbed_item != item)
		return;

	item->canvas->grabbed_item = NULL;

	gdk_pointer_ungrab (etime);
}

void
gnome_canvas_item_i2w_matrix (GnomeCanvasItem *item, cairo_matrix_t *matrix)
{
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));
	g_return_if_fail (matrix != NULL);

	cairo_matrix_init_identity (matrix);

	while (item) {
                cairo_matrix_multiply (matrix, matrix, &item->matrix);

		item = item->parent;
	}
}

void
gnome_canvas_item_w2i_matrix (GnomeCanvasItem *item, cairo_matrix_t *matrix)
{
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));
	g_return_if_fail (matrix != NULL);

	cairo_matrix_init_identity (matrix);

	while (item) {
                cairo_matrix_multiply (matrix, &item->matrix, matrix);
		item = item->parent;
	}
}

/**
 * gnome_canvas_item_w2i:
 * @item: A canvas item.
 * @x: X coordinate to convert (input/output value).
 * @y: Y coordinate to convert (input/output value).
 *
 * Converts a coordinate pair from world coordinates to item-relative
 * coordinates.
 **/
void
gnome_canvas_item_w2i (GnomeCanvasItem *item, gdouble *x, gdouble *y)
{
        cairo_matrix_t matrix;

	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));
	g_return_if_fail (x != NULL);
	g_return_if_fail (y != NULL);

	gnome_canvas_item_w2i_matrix (item, &matrix);
        cairo_matrix_transform_point (&matrix, x, y);
}

/**
 * gnome_canvas_item_i2w:
 * @item: A canvas item.
 * @x: X coordinate to convert (input/output value).
 * @y: Y coordinate to convert (input/output value).
 *
 * Converts a coordinate pair from item-relative coordinates to world
 * coordinates.
 **/
void
gnome_canvas_item_i2w (GnomeCanvasItem *item, gdouble *x, gdouble *y)
{
        cairo_matrix_t matrix;

	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));
	g_return_if_fail (x != NULL);
	g_return_if_fail (y != NULL);

	gnome_canvas_item_i2w_matrix (item, &matrix);
        cairo_matrix_transform_point (&matrix, x, y);
}

/**
 * gnome_canvas_item_i2c_matrix:
 * @item: A canvas item.
 * @matrix: Matrix to take the resulting transformation matrix (return value).
 *
 * Gets the affine transform that converts from item-relative coordinates to
 * canvas pixel coordinates.
 **/
void
gnome_canvas_item_i2c_matrix (GnomeCanvasItem *item, cairo_matrix_t *matrix)
{
	cairo_matrix_t i2w, w2c;

	gnome_canvas_item_i2w_matrix (item, &i2w);
	gnome_canvas_w2c_matrix (item->canvas, &w2c);
	cairo_matrix_multiply (matrix, &i2w, &w2c);
}

/* Returns whether the item is an inferior of or is equal to the parent. */
static gint
is_descendant (GnomeCanvasItem *item, GnomeCanvasItem *parent)
{
	for (; item; item = item->parent)
		if (item == parent)
			return TRUE;

	return FALSE;
}

/**
 * gnome_canvas_item_reparent:
 * @item: A canvas item.
 * @new_group: A canvas group.
 *
 * Changes the parent of the specified item to be the new group.  The item keeps
 * its group-relative coordinates as for its old parent, so the item may change
 * its absolute position within the canvas.
 **/
void
gnome_canvas_item_reparent (GnomeCanvasItem *item, GnomeCanvasGroup *new_group)
{
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));
	g_return_if_fail (GNOME_IS_CANVAS_GROUP (new_group));

	/* Both items need to be in the same canvas */
	g_return_if_fail (item->canvas == GNOME_CANVAS_ITEM (new_group)->canvas);

	/* The group cannot be an inferior of the item or be the item itself --
	 * this also takes care of the case where the item is the root item of
	 * the canvas.  */
	g_return_if_fail (!is_descendant (GNOME_CANVAS_ITEM (new_group), item));

	/* Everything is ok, now actually reparent the item */

	g_object_ref (G_OBJECT (item)); /* protect it from the unref in group_remove */

	redraw_if_visible (item);

	group_remove (GNOME_CANVAS_GROUP (item->parent), item);
	item->parent = GNOME_CANVAS_ITEM (new_group);
	group_add (new_group, item);

	/* Redraw and repick */

	redraw_if_visible (item);
	item->canvas->need_repick = TRUE;

	g_object_unref (G_OBJECT (item));
}

/**
 * gnome_canvas_item_grab_focus:
 * @item: A canvas item.
 *
 * Makes the specified item take the keyboard focus, so all keyboard events will
 * be sent to it.  If the canvas widget itself did not have the focus, it grabs
 * it as well.
 **/
void
gnome_canvas_item_grab_focus (GnomeCanvasItem *item)
{
	GnomeCanvasItem *focused_item;
	GdkEvent ev;

	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));
	g_return_if_fail (gtk_widget_get_can_focus (GTK_WIDGET (item->canvas)));

	focused_item = item->canvas->focused_item;

	if (focused_item) {
		GtkLayout *layout;
		GdkWindow *bin_window;

		layout = GTK_LAYOUT (item->canvas);
		bin_window = gtk_layout_get_bin_window (layout);

		ev.focus_change.type = GDK_FOCUS_CHANGE;
		ev.focus_change.window = bin_window;
		ev.focus_change.send_event = FALSE;
		ev.focus_change.in = FALSE;

		emit_event (item->canvas, &ev);
	}

	item->canvas->focused_item = item;
	gtk_widget_grab_focus (GTK_WIDGET (item->canvas));

	if (focused_item) {
		GtkLayout *layout;
		GdkWindow *bin_window;

		layout = GTK_LAYOUT (item->canvas);
		bin_window = gtk_layout_get_bin_window (layout);

		ev.focus_change.type = GDK_FOCUS_CHANGE;
		ev.focus_change.window = bin_window;
		ev.focus_change.send_event = FALSE;
		ev.focus_change.in = TRUE;

		emit_event (item->canvas, &ev);
	}
}

/**
 * gnome_canvas_item_get_bounds:
 * @item: A canvas item.
 * @x1: Leftmost edge of the bounding box (return value).
 * @y1: Upper edge of the bounding box (return value).
 * @x2: Rightmost edge of the bounding box (return value).
 * @y2: Lower edge of the bounding box (return value).
 *
 * Queries the bounding box of a canvas item.  The bounds are returned in the
 * coordinate system of the item's parent.
 **/
void
gnome_canvas_item_get_bounds (GnomeCanvasItem *item,
                              gdouble *x1,
                              gdouble *y1,
                              gdouble *x2,
                              gdouble *y2)
{
	gdouble tx1, ty1, tx2, ty2;

	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));

	tx1 = ty1 = tx2 = ty2 = 0.0;

	/* Get the item's bounds in its coordinate system */

	if (GNOME_CANVAS_ITEM_GET_CLASS (item)->bounds)
		(* GNOME_CANVAS_ITEM_GET_CLASS (item)->bounds) (item, &tx1, &ty1, &tx2, &ty2);

	/* Make the bounds relative to the item's parent coordinate system */
        gnome_canvas_matrix_transform_rect (&item->matrix, &tx1, &ty1, &tx2, &ty2);

	/* Return the values */

	if (x1)
		*x1 = tx1;

	if (y1)
		*y1 = ty1;

	if (x2)
		*x2 = tx2;

	if (y2)
		*y2 = ty2;
}

/**
 * gnome_canvas_item_request_update
 * @item: A canvas item.
 *
 * To be used only by item implementations.  Requests that the canvas queue an
 * update for the specified item.
 **/
void
gnome_canvas_item_request_update (GnomeCanvasItem *item)
{
	if (item->flags & GNOME_CANVAS_ITEM_NEED_UPDATE)
		return;

	item->flags |= GNOME_CANVAS_ITEM_NEED_UPDATE;

	if (item->parent != NULL) {
		/* Recurse up the tree */
		gnome_canvas_item_request_update (item->parent);
	} else {
		/* Have reached the top of the tree, make sure the update call gets scheduled. */
		gnome_canvas_request_update (item->canvas);
	}
}

/*** GnomeCanvasGroup ***/

enum {
	GROUP_PROP_0,
	GROUP_PROP_X,
	GROUP_PROP_Y
};

static void gnome_canvas_group_class_init  (GnomeCanvasGroupClass *class);
static void gnome_canvas_group_init        (GnomeCanvasGroup      *group);
static void gnome_canvas_group_set_property (GObject               *object,
					    guint                  param_id,
					    const GValue          *value,
					    GParamSpec            *pspec);
static void gnome_canvas_group_get_property (GObject               *object,
					    guint                  param_id,
					    GValue                *value,
					    GParamSpec            *pspec);

static void gnome_canvas_group_destroy     (GnomeCanvasItem *object);

static void   gnome_canvas_group_update      (GnomeCanvasItem *item,
                                              const cairo_matrix_t *matrix,
					      gint flags);
static void   gnome_canvas_group_realize     (GnomeCanvasItem *item);
static void   gnome_canvas_group_unrealize   (GnomeCanvasItem *item);
static void   gnome_canvas_group_map         (GnomeCanvasItem *item);
static void   gnome_canvas_group_unmap       (GnomeCanvasItem *item);
static void   gnome_canvas_group_draw        (GnomeCanvasItem *item,
					      GdkDrawable *drawable,
					      gint x, gint y,
					      gint width, gint height);
static GnomeCanvasItem *gnome_canvas_group_point (GnomeCanvasItem *item,
					      gdouble x, gdouble y,
					      gint cx, gint cy);
static void   gnome_canvas_group_bounds      (GnomeCanvasItem *item,
					      gdouble *x1, gdouble *y1,
					      gdouble *x2, gdouble *y2);

static GnomeCanvasItemClass *group_parent_class;

/**
 * gnome_canvas_group_get_type:
 *
 * Registers the &GnomeCanvasGroup class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value:  The type ID of the &GnomeCanvasGroup class.
 **/
GType
gnome_canvas_group_get_type (void)
{
	static GType canvas_group_type;

	if (!canvas_group_type) {
		const GTypeInfo object_info = {
			sizeof (GnomeCanvasGroupClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gnome_canvas_group_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,			/* class_data */
			sizeof (GnomeCanvasGroup),
			0,			/* n_preallocs */
			(GInstanceInitFunc) gnome_canvas_group_init,
			NULL			/* value_table */
		};

		canvas_group_type = g_type_register_static (
			GNOME_TYPE_CANVAS_ITEM, "GnomeCanvasGroup",
			&object_info, 0);
	}

	return canvas_group_type;
}

/* Class initialization function for GnomeCanvasGroupClass */
static void
gnome_canvas_group_class_init (GnomeCanvasGroupClass *class)
{
	GObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = (GObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	group_parent_class = g_type_class_peek_parent (class);

	object_class->set_property = gnome_canvas_group_set_property;
	object_class->get_property = gnome_canvas_group_get_property;

	g_object_class_install_property
		(object_class, GROUP_PROP_X,
		 g_param_spec_double ("x",
				      "X",
				      "X",
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property
		(object_class, GROUP_PROP_Y,
		 g_param_spec_double ("y",
				      "Y",
				      "Y",
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	item_class->destroy = gnome_canvas_group_destroy;
	item_class->update = gnome_canvas_group_update;
	item_class->realize = gnome_canvas_group_realize;
	item_class->unrealize = gnome_canvas_group_unrealize;
	item_class->map = gnome_canvas_group_map;
	item_class->unmap = gnome_canvas_group_unmap;
	item_class->draw = gnome_canvas_group_draw;
	item_class->point = gnome_canvas_group_point;
	item_class->bounds = gnome_canvas_group_bounds;
}

/* Object initialization function for GnomeCanvasGroup */
static void
gnome_canvas_group_init (GnomeCanvasGroup *group)
{
}

/* Set_property handler for canvas groups */
static void
gnome_canvas_group_set_property (GObject *gobject, guint param_id,
				 const GValue *value, GParamSpec *pspec)
{
	GnomeCanvasItem *item;

	g_return_if_fail (GNOME_IS_CANVAS_GROUP (gobject));

	item = GNOME_CANVAS_ITEM (gobject);

	switch (param_id) {
	case GROUP_PROP_X:
		item->matrix.x0 = g_value_get_double (value);
		break;

	case GROUP_PROP_Y:
		item->matrix.y0 = g_value_get_double (value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, param_id, pspec);
		break;
	}
}

/* Get_property handler for canvas groups */
static void
gnome_canvas_group_get_property (GObject *gobject, guint param_id,
				 GValue *value, GParamSpec *pspec)
{
	GnomeCanvasItem *item;

	g_return_if_fail (GNOME_IS_CANVAS_GROUP (gobject));

	item = GNOME_CANVAS_ITEM (gobject);

	switch (param_id) {
	case GROUP_PROP_X:
		g_value_set_double (value, item->matrix.x0);
		break;

	case GROUP_PROP_Y:
		g_value_set_double (value, item->matrix.y0);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, param_id, pspec);
		break;
	}
}

/* Destroy handler for canvas groups */
static void
gnome_canvas_group_destroy (GnomeCanvasItem *object)
{
	GnomeCanvasGroup *group;

	g_return_if_fail (GNOME_IS_CANVAS_GROUP (object));

	group = GNOME_CANVAS_GROUP (object);

	while (group->item_list) {
		/* child is unref'ed by the child's group_remove (). */
		g_object_run_dispose (G_OBJECT (group->item_list->data));
	}

	if (GNOME_CANVAS_ITEM_CLASS (group_parent_class)->destroy)
		GNOME_CANVAS_ITEM_CLASS (group_parent_class)->destroy (object);
}

/* Update handler for canvas groups */
static void
gnome_canvas_group_update (GnomeCanvasItem *item,
                           const cairo_matrix_t *i2c,
                           gint flags)
{
	GnomeCanvasGroup *group;
	GList *list;
	GnomeCanvasItem *i;
        double x1, y1, x2, y2;

	group = GNOME_CANVAS_GROUP (item);

	(* group_parent_class->update) (item, i2c, flags);

	x1 = G_MAXDOUBLE;
	y1 = G_MAXDOUBLE;
	x2 = -G_MAXDOUBLE;
	y2 = -G_MAXDOUBLE;

	for (list = group->item_list; list; list = list->next) {
		i = list->data;

		gnome_canvas_item_invoke_update (i, i2c, flags);

                x1 = MIN (x1, i->x1);
                x2 = MAX (x2, i->x2);
                y1 = MIN (y1, i->y1);
                y2 = MAX (y2, i->y2);
	}
        if (x1 >= x2 || y1 >= y2) {
                item->x1 = item->x2 = item->y1 = item->y2 = 0;
        } else {
                item->x1 = x1;
                item->y1 = y1;
                item->x2 = x2;
                item->y2 = y2;
        }
}

/* Realize handler for canvas groups */
static void
gnome_canvas_group_realize (GnomeCanvasItem *item)
{
	GnomeCanvasGroup *group;
	GList *list;
	GnomeCanvasItem *i;

	group = GNOME_CANVAS_GROUP (item);

	for (list = group->item_list; list; list = list->next) {
		i = list->data;

		if (!(i->flags & GNOME_CANVAS_ITEM_REALIZED))
			(* GNOME_CANVAS_ITEM_GET_CLASS (i)->realize) (i);
	}

	(* group_parent_class->realize) (item);
}

/* Unrealize handler for canvas groups */
static void
gnome_canvas_group_unrealize (GnomeCanvasItem *item)
{
	GnomeCanvasGroup *group;
	GList *list;
	GnomeCanvasItem *i;

	group = GNOME_CANVAS_GROUP (item);

	for (list = group->item_list; list; list = list->next) {
		i = list->data;

		if (i->flags & GNOME_CANVAS_ITEM_REALIZED)
			(* GNOME_CANVAS_ITEM_GET_CLASS (i)->unrealize) (i);
	}

	(* group_parent_class->unrealize) (item);
}

/* Map handler for canvas groups */
static void
gnome_canvas_group_map (GnomeCanvasItem *item)
{
	GnomeCanvasGroup *group;
	GList *list;
	GnomeCanvasItem *i;

	group = GNOME_CANVAS_GROUP (item);

	for (list = group->item_list; list; list = list->next) {
		i = list->data;

		if (!(i->flags & GNOME_CANVAS_ITEM_MAPPED))
			(* GNOME_CANVAS_ITEM_GET_CLASS (i)->map) (i);
	}

	(* group_parent_class->map) (item);
}

/* Unmap handler for canvas groups */
static void
gnome_canvas_group_unmap (GnomeCanvasItem *item)
{
	GnomeCanvasGroup *group;
	GList *list;
	GnomeCanvasItem *i;

	group = GNOME_CANVAS_GROUP (item);

	for (list = group->item_list; list; list = list->next) {
		i = list->data;

		if (i->flags & GNOME_CANVAS_ITEM_MAPPED)
			(* GNOME_CANVAS_ITEM_GET_CLASS (i)->unmap) (i);
	}

	(* group_parent_class->unmap) (item);
}

/* Draw handler for canvas groups */
static void
gnome_canvas_group_draw (GnomeCanvasItem *item, GdkDrawable *drawable,
			 gint x, gint y, gint width, gint height)
{
	GnomeCanvasGroup *group;
	GList *list;
	GnomeCanvasItem *child = NULL;

	group = GNOME_CANVAS_GROUP (item);

	for (list = group->item_list; list; list = list->next) {
		child = list->data;

		if (((child->flags & GNOME_CANVAS_ITEM_VISIBLE)
		     && ((child->x1 < (x + width))
			 && (child->y1 < (y + height))
			 && (child->x2 > x)
			 && (child->y2 > y)))
		    || ((child->flags & GNOME_CANVAS_ITEM_ALWAYS_REDRAW)
			&& (child->x1 < child->canvas->redraw_x2)
			&& (child->y1 < child->canvas->redraw_y2)
			&& (child->x2 > child->canvas->redraw_x1)
			&& (child->y2 > child->canvas->redraw_y2)))
			if (GNOME_CANVAS_ITEM_GET_CLASS (child)->draw)
				(* GNOME_CANVAS_ITEM_GET_CLASS (child)->draw) (
					child, drawable, x, y, width, height);
	}
}

/* Point handler for canvas groups */
static GnomeCanvasItem *
gnome_canvas_group_point (GnomeCanvasItem *item,
                          gdouble x,
                          gdouble y,
                          gint cx,
                          gint cy)
{
	GnomeCanvasGroup *group;
	GList *list;
	GnomeCanvasItem *child, *point_item;

	group = GNOME_CANVAS_GROUP (item);

	for (list = g_list_last (group->item_list); list; list = list->prev) {
		child = list->data;

		if ((child->x1 > cx) || (child->y1 > cy) || (child->x2 < cx) || (child->y2 < cy))
			continue;

		if (!(child->flags & GNOME_CANVAS_ITEM_VISIBLE))
                        continue;

		point_item = gnome_canvas_item_invoke_point (child, x, y, cx, cy);
		if (point_item)
                        return point_item;
	}

	return NULL;
}

/* Bounds handler for canvas groups */
static void
gnome_canvas_group_bounds (GnomeCanvasItem *item,
                           gdouble *x1,
                           gdouble *y1,
                           gdouble *x2,
                           gdouble *y2)
{
	GnomeCanvasGroup *group;
	GnomeCanvasItem *child;
	GList *list;
	gdouble tx1, ty1, tx2, ty2;
	gdouble minx, miny, maxx, maxy;
	gint set;

	group = GNOME_CANVAS_GROUP (item);

	/* Get the bounds of the first visible item */

	child = NULL; /* Unnecessary but eliminates a warning. */

	set = FALSE;

	for (list = group->item_list; list; list = list->next) {
		child = list->data;

		if (child->flags & GNOME_CANVAS_ITEM_VISIBLE) {
			set = TRUE;
			gnome_canvas_item_get_bounds (child, &minx, &miny, &maxx, &maxy);
			break;
		}
	}

	/* If there were no visible items, return an empty bounding box */

	if (!set) {
		*x1 = *y1 = *x2 = *y2 = 0.0;
		return;
	}

	/* Now we can grow the bounds using the rest of the items */

	list = list->next;

	for (; list; list = list->next) {
		child = list->data;

		if (!(child->flags & GNOME_CANVAS_ITEM_VISIBLE))
			continue;

		gnome_canvas_item_get_bounds (child, &tx1, &ty1, &tx2, &ty2);

		if (tx1 < minx)
			minx = tx1;

		if (ty1 < miny)
			miny = ty1;

		if (tx2 > maxx)
			maxx = tx2;

		if (ty2 > maxy)
			maxy = ty2;
	}

	*x1 = minx;
	*y1 = miny;
	*x2 = maxx;
	*y2 = maxy;
}

/* Adds an item to a group */
static void
group_add (GnomeCanvasGroup *group, GnomeCanvasItem *item)
{
	g_object_ref_sink (G_OBJECT (item));

	if (!group->item_list) {
		group->item_list = g_list_append (group->item_list, item);
		group->item_list_end = group->item_list;
	} else
		group->item_list_end = g_list_append (group->item_list_end, item)->next;

	if (group->item.flags & GNOME_CANVAS_ITEM_REALIZED)
		(* GNOME_CANVAS_ITEM_GET_CLASS (item)->realize) (item);

	if (group->item.flags & GNOME_CANVAS_ITEM_MAPPED)
		(* GNOME_CANVAS_ITEM_GET_CLASS (item)->map) (item);

	g_object_notify (G_OBJECT (item), "parent");
}

/* Removes an item from a group */
static void
group_remove (GnomeCanvasGroup *group, GnomeCanvasItem *item)
{
	GList *children;

	g_return_if_fail (GNOME_IS_CANVAS_GROUP (group));
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));

	for (children = group->item_list; children; children = children->next)
		if (children->data == item) {
			if (item->flags & GNOME_CANVAS_ITEM_MAPPED)
				(* GNOME_CANVAS_ITEM_GET_CLASS (item)->unmap) (item);

			if (item->flags & GNOME_CANVAS_ITEM_REALIZED)
				(* GNOME_CANVAS_ITEM_GET_CLASS (item)->unrealize) (item);

			/* Unparent the child */

			item->parent = NULL;
			g_object_unref (G_OBJECT (item));

			/* Remove it from the list */

			if (children == group->item_list_end)
				group->item_list_end = children->prev;

			group->item_list = g_list_remove_link (group->item_list, children);
			g_list_free (children);
			break;
		}
}

/*** GnomeCanvas ***/

enum {
	DRAW_BACKGROUND,
	LAST_SIGNAL
};

static void gnome_canvas_class_init          (GnomeCanvasClass *class);
static void gnome_canvas_init                (GnomeCanvas      *canvas);
static void gnome_canvas_dispose             (GObject          *object);
static void gnome_canvas_map                 (GtkWidget        *widget);
static void gnome_canvas_unmap               (GtkWidget        *widget);
static void gnome_canvas_realize             (GtkWidget        *widget);
static void gnome_canvas_unrealize           (GtkWidget        *widget);
static void gnome_canvas_size_allocate       (GtkWidget        *widget,
					      GtkAllocation    *allocation);
static gint gnome_canvas_button              (GtkWidget        *widget,
					      GdkEventButton   *event);
static gint gnome_canvas_motion              (GtkWidget        *widget,
					      GdkEventMotion   *event);
static gint gnome_canvas_expose              (GtkWidget        *widget,
					      GdkEventExpose   *event);
static gboolean gnome_canvas_key             (GtkWidget        *widget,
					      GdkEventKey      *event);
static gint gnome_canvas_crossing            (GtkWidget        *widget,
					      GdkEventCrossing *event);
static gint gnome_canvas_focus_in            (GtkWidget        *widget,
					      GdkEventFocus    *event);
static gint gnome_canvas_focus_out           (GtkWidget        *widget,
					      GdkEventFocus    *event);
static void gnome_canvas_request_update_real (GnomeCanvas      *canvas);
static void gnome_canvas_draw_background     (GnomeCanvas      *canvas,
					      GdkDrawable      *drawable,
					      gint               x,
					      gint               y,
					      gint               width,
					      gint               height);

static GtkLayoutClass *canvas_parent_class;

static guint canvas_signals[LAST_SIGNAL];

enum {
        PROP_0,
	PROP_FOCUSED_ITEM,
};

/**
 * gnome_canvas_get_type:
 *
 * Registers the &GnomeCanvas class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value:  The type ID of the &GnomeCanvas class.
 **/
GType
gnome_canvas_get_type (void)
{
	static GType canvas_type;

	if (!canvas_type) {
		const GTypeInfo object_info = {
			sizeof (GnomeCanvasClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gnome_canvas_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,			/* class_data */
			sizeof (GnomeCanvas),
			0,			/* n_preallocs */
			(GInstanceInitFunc) gnome_canvas_init,
			NULL			/* value_table */
		};

		canvas_type = g_type_register_static (GTK_TYPE_LAYOUT, "GnomeCanvas",
						      &object_info, 0);
	}

	return canvas_type;
}

static void
gnome_canvas_get_property (GObject    *object,
			   guint       prop_id,
			   GValue     *value,
			   GParamSpec *pspec)
{
	switch (prop_id) {
	case PROP_FOCUSED_ITEM:
		g_value_set_object (value, GNOME_CANVAS (object)->focused_item);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gnome_canvas_set_property (GObject      *object,
			   guint         prop_id,
			   const GValue *value,
			   GParamSpec   *pspec)
{
	switch (prop_id) {
	case PROP_FOCUSED_ITEM:
		GNOME_CANVAS (object)->focused_item = g_value_get_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/* Class initialization function for GnomeCanvasClass */
static void
gnome_canvas_class_init (GnomeCanvasClass *klass)
{
	GObjectClass   *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GObjectClass *)klass;
	widget_class  = (GtkWidgetClass *) klass;

	canvas_parent_class = g_type_class_peek_parent (klass);

	object_class->set_property = gnome_canvas_set_property;
	object_class->get_property = gnome_canvas_get_property;
	object_class->dispose = gnome_canvas_dispose;

	widget_class->map = gnome_canvas_map;
	widget_class->unmap = gnome_canvas_unmap;
	widget_class->realize = gnome_canvas_realize;
	widget_class->unrealize = gnome_canvas_unrealize;
	widget_class->size_allocate = gnome_canvas_size_allocate;
	widget_class->button_press_event = gnome_canvas_button;
	widget_class->button_release_event = gnome_canvas_button;
	widget_class->motion_notify_event = gnome_canvas_motion;
	widget_class->expose_event = gnome_canvas_expose;
	widget_class->key_press_event = gnome_canvas_key;
	widget_class->key_release_event = gnome_canvas_key;
	widget_class->enter_notify_event = gnome_canvas_crossing;
	widget_class->leave_notify_event = gnome_canvas_crossing;
	widget_class->focus_in_event = gnome_canvas_focus_in;
	widget_class->focus_out_event = gnome_canvas_focus_out;

	klass->draw_background = gnome_canvas_draw_background;
	klass->request_update = gnome_canvas_request_update_real;

	g_object_class_install_property (object_class, PROP_FOCUSED_ITEM,
					 g_param_spec_object ("focused_item", NULL, NULL,
					 GNOME_TYPE_CANVAS_ITEM,
					 (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	canvas_signals[DRAW_BACKGROUND] =
		g_signal_new ("draw_background",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GnomeCanvasClass, draw_background),
			      NULL, NULL,
			      gnome_canvas_marshal_VOID__OBJECT_INT_INT_INT_INT,
			      G_TYPE_NONE, 5, GDK_TYPE_DRAWABLE,
			      G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);

	gail_canvas_init ();
}

/* Callback used when the root item of a canvas is destroyed.  The user should
 * never ever do this, so we panic if this happens.
 */
G_GNUC_NORETURN static void
panic_root_finalized (gpointer data, GObject *gone_object)
{
	g_error ("Eeeek, root item %p of canvas %p was destroyed!", gone_object, data);
}

/* Object initialization function for GnomeCanvas */
static void
gnome_canvas_init (GnomeCanvas *canvas)
{
	GtkLayout *layout;
	guint layout_width, layout_height;

	layout = GTK_LAYOUT (canvas);
	gtk_layout_get_size (layout, &layout_width, &layout_height);

	gtk_widget_set_can_focus (GTK_WIDGET (canvas), TRUE);

	canvas->need_update = FALSE;
	canvas->need_redraw = FALSE;
	canvas->redraw_area = NULL;
	canvas->idle_id = 0;

	canvas->scroll_x1 = 0.0;
	canvas->scroll_y1 = 0.0;
	canvas->scroll_x2 = layout_width;
	canvas->scroll_y2 = layout_height;

	canvas->pixels_per_unit = 1.0;

	canvas->pick_event.type = GDK_LEAVE_NOTIFY;
	canvas->pick_event.crossing.x = 0;
	canvas->pick_event.crossing.y = 0;

	/* This may not be what people want, but it is set to be turned on by
	 * default to have the same initial behavior as the canvas in GNOME 1.4.
	 */
	canvas->center_scroll_region = TRUE;

	gtk_layout_set_hadjustment (GTK_LAYOUT (canvas), NULL);
	gtk_layout_set_vadjustment (GTK_LAYOUT (canvas), NULL);

	/* Disable the gtk+ gdouble buffering since the canvas uses it's own. */
	gtk_widget_set_double_buffered (GTK_WIDGET (canvas), FALSE);

	/* Create the root item as a special case */

	canvas->root = GNOME_CANVAS_ITEM (
		g_object_new (gnome_canvas_group_get_type (), NULL));
	canvas->root->canvas = canvas;

	g_object_ref_sink (canvas->root);

	g_object_weak_ref (G_OBJECT (canvas->root), panic_root_finalized, canvas);

	canvas->need_repick = TRUE;
}

/* Convenience function to remove the idle handler of a canvas */
static void
remove_idle (GnomeCanvas *canvas)
{
	if (canvas->idle_id == 0)
		return;

	g_source_remove (canvas->idle_id);
	canvas->idle_id = 0;
}

/* Removes the transient state of the canvas (idle handler, grabs). */
static void
shutdown_transients (GnomeCanvas *canvas)
{
	/* We turn off the need_redraw flag, since if the canvas is mapped again
	 * it will request a redraw anyways.  We do not turn off the need_update
	 * flag, though, because updates are not queued when the canvas remaps
	 * itself.
	 */
	if (canvas->need_redraw) {
		canvas->need_redraw = FALSE;
		art_uta_free (canvas->redraw_area);
		canvas->redraw_area = NULL;
		canvas->redraw_x1 = 0;
		canvas->redraw_y1 = 0;
		canvas->redraw_x2 = 0;
		canvas->redraw_y2 = 0;
	}

	if (canvas->grabbed_item) {
		canvas->grabbed_item = NULL;
		gdk_pointer_ungrab (GDK_CURRENT_TIME);
	}

	remove_idle (canvas);
}

/* Dispose handler for GnomeCanvas */
static void
gnome_canvas_dispose (GObject *object)
{
	GnomeCanvas *canvas;

	g_return_if_fail (GNOME_IS_CANVAS (object));

	/* remember, dispose can be run multiple times! */

	canvas = GNOME_CANVAS (object);

	if (canvas->root) {
		g_object_weak_unref (G_OBJECT (canvas->root), panic_root_finalized, canvas);
		g_object_unref (G_OBJECT (canvas->root));
		canvas->root = NULL;
	}

	shutdown_transients (canvas);

	if (G_OBJECT_CLASS (canvas_parent_class)->dispose)
		G_OBJECT_CLASS (canvas_parent_class)->dispose (object);
}

/**
 * gnome_canvas_new:
 *
 * Creates a new empty canvas in non-antialiased mode.
 *
 * Return value: A newly-created canvas.
 **/
GtkWidget *
gnome_canvas_new (void)
{
	return GTK_WIDGET (g_object_new (gnome_canvas_get_type (), NULL));
}

/* Map handler for the canvas */
static void
gnome_canvas_map (GtkWidget *widget)
{
	GnomeCanvas *canvas;

	g_return_if_fail (GNOME_IS_CANVAS (widget));

	/* Normal widget mapping stuff */

	if (GTK_WIDGET_CLASS (canvas_parent_class)->map)
		(* GTK_WIDGET_CLASS (canvas_parent_class)->map) (widget);

	canvas = GNOME_CANVAS (widget);

	if (canvas->need_update)
		add_idle (canvas);

	/* Map items */

	if (GNOME_CANVAS_ITEM_GET_CLASS (canvas->root)->map)
		(* GNOME_CANVAS_ITEM_GET_CLASS (canvas->root)->map) (canvas->root);
}

/* Unmap handler for the canvas */
static void
gnome_canvas_unmap (GtkWidget *widget)
{
	GnomeCanvas *canvas;

	g_return_if_fail (GNOME_IS_CANVAS (widget));

	canvas = GNOME_CANVAS (widget);

	shutdown_transients (canvas);

	/* Unmap items */

	if (GNOME_CANVAS_ITEM_GET_CLASS (canvas->root)->unmap)
		(* GNOME_CANVAS_ITEM_GET_CLASS (canvas->root)->unmap) (canvas->root);

	/* Normal widget unmapping stuff */

	if (GTK_WIDGET_CLASS (canvas_parent_class)->unmap)
		(* GTK_WIDGET_CLASS (canvas_parent_class)->unmap) (widget);
}

/* Realize handler for the canvas */
static void
gnome_canvas_realize (GtkWidget *widget)
{
	GnomeCanvas *canvas;
	GtkLayout *layout;
	GdkWindow *bin_window;

	g_return_if_fail (GNOME_IS_CANVAS (widget));

	/* Normal widget realization stuff */

	if (GTK_WIDGET_CLASS (canvas_parent_class)->realize)
		(* GTK_WIDGET_CLASS (canvas_parent_class)->realize) (widget);

	canvas = GNOME_CANVAS (widget);

	layout = GTK_LAYOUT (canvas);
	bin_window = gtk_layout_get_bin_window (layout);

	gdk_window_set_events (bin_window,
			       (gdk_window_get_events (bin_window)
				 | GDK_EXPOSURE_MASK
				 | GDK_BUTTON_PRESS_MASK
				 | GDK_BUTTON_RELEASE_MASK
				 | GDK_POINTER_MOTION_MASK
				 | GDK_KEY_PRESS_MASK
				 | GDK_KEY_RELEASE_MASK
				 | GDK_ENTER_NOTIFY_MASK
				 | GDK_LEAVE_NOTIFY_MASK
				 | GDK_FOCUS_CHANGE_MASK));

	/* Create our own temporary pixmap gc and realize all the items */

	canvas->pixmap_gc = gdk_gc_new (bin_window);

	(* GNOME_CANVAS_ITEM_GET_CLASS (canvas->root)->realize) (canvas->root);
}

/* Unrealize handler for the canvas */
static void
gnome_canvas_unrealize (GtkWidget *widget)
{
	GnomeCanvas *canvas;

	g_return_if_fail (GNOME_IS_CANVAS (widget));

	canvas = GNOME_CANVAS (widget);

	shutdown_transients (canvas);

	/* Unrealize items and parent widget */

	(* GNOME_CANVAS_ITEM_GET_CLASS (canvas->root)->unrealize) (canvas->root);

	g_object_unref (canvas->pixmap_gc);
	canvas->pixmap_gc = NULL;

	if (GTK_WIDGET_CLASS (canvas_parent_class)->unrealize)
		(* GTK_WIDGET_CLASS (canvas_parent_class)->unrealize) (widget);
}

/* Handles scrolling of the canvas.  Adjusts the scrolling and zooming offset to
 * keep as much as possible of the canvas scrolling region in view.
 */
static void
scroll_to (GnomeCanvas *canvas, gint cx, gint cy)
{
	GtkWidget *widget;
	GtkLayout *layout;
	GtkAllocation allocation;
	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;
	guint layout_width, layout_height;
	gint scroll_width, scroll_height;
	gint right_limit, bottom_limit;
	gint old_zoom_xofs, old_zoom_yofs;
	gint canvas_width, canvas_height;

	widget = GTK_WIDGET (canvas);
	gtk_widget_get_allocation (widget, &allocation);

	layout = GTK_LAYOUT (canvas);
	hadjustment = gtk_layout_get_hadjustment (layout);
	vadjustment = gtk_layout_get_vadjustment (layout);
	gtk_layout_get_size (layout, &layout_width, &layout_height);

	canvas_width = allocation.width;
	canvas_height = allocation.height;

	scroll_width =
		floor ((canvas->scroll_x2 - canvas->scroll_x1) *
		canvas->pixels_per_unit + 0.5);
	scroll_height =
		floor ((canvas->scroll_y2 - canvas->scroll_y1) *
		canvas->pixels_per_unit + 0.5);

	right_limit = scroll_width - canvas_width;
	bottom_limit = scroll_height - canvas_height;

	old_zoom_xofs = canvas->zoom_xofs;
	old_zoom_yofs = canvas->zoom_yofs;

	if (right_limit < 0) {
		cx = 0;

		if (canvas->center_scroll_region) {
			canvas->zoom_xofs = (canvas_width - scroll_width) / 2;
			scroll_width = canvas_width;
		} else
			canvas->zoom_xofs = 0;
	} else if (cx < 0) {
		cx = 0;
		canvas->zoom_xofs = 0;
	} else if (cx > right_limit) {
		cx = right_limit;
		canvas->zoom_xofs = 0;
	} else
		canvas->zoom_xofs = 0;

	if (bottom_limit < 0) {
		cy = 0;

		if (canvas->center_scroll_region) {
			canvas->zoom_yofs = (canvas_height - scroll_height) / 2;
			scroll_height = canvas_height;
		} else
			canvas->zoom_yofs = 0;
	} else if (cy < 0) {
		cy = 0;
		canvas->zoom_yofs = 0;
	} else if (cy > bottom_limit) {
		cy = bottom_limit;
		canvas->zoom_yofs = 0;
	} else
		canvas->zoom_yofs = 0;

	if ((canvas->zoom_xofs != old_zoom_xofs) || (canvas->zoom_yofs != old_zoom_yofs)) {
		/* This can only occur, if either canvas size or widget size changes */
		/* So I think we can request full redraw here */
		/* The reason is, that coverage UTA will be invalidated by offset change */
		/* fixme: Strictly this is not correct - we have to remove our own idle (Lauris) */
		/* More stuff - we have to mark root as needing fresh affine (Lauris) */
		if (!(canvas->root->flags & GNOME_CANVAS_ITEM_NEED_AFFINE)) {
			canvas->root->flags |= GNOME_CANVAS_ITEM_NEED_AFFINE;
			gnome_canvas_request_update (canvas);
		}
		gtk_widget_queue_draw (GTK_WIDGET (canvas));
	}

	if (hadjustment)
		gtk_adjustment_set_value (hadjustment, cx);

	if (vadjustment)
		gtk_adjustment_set_value (vadjustment, cy);

	if ((scroll_width != (gint) layout_width)
	    || (scroll_height != (gint) layout_height))
		gtk_layout_set_size (GTK_LAYOUT (canvas), scroll_width, scroll_height);
}

/* Size allocation handler for the canvas */
static void
gnome_canvas_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	GtkLayout *layout;
	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;

	g_return_if_fail (GNOME_IS_CANVAS (widget));
	g_return_if_fail (allocation != NULL);

	if (GTK_WIDGET_CLASS (canvas_parent_class)->size_allocate)
		(* GTK_WIDGET_CLASS (canvas_parent_class)->size_allocate) (widget, allocation);

	layout = GTK_LAYOUT (widget);
	hadjustment = gtk_layout_get_hadjustment (layout);
	vadjustment = gtk_layout_get_vadjustment (layout);

	/* Recenter the view, if appropriate */

	g_object_freeze_notify (G_OBJECT (hadjustment));
	g_object_freeze_notify (G_OBJECT (vadjustment));

	gtk_adjustment_set_page_size (hadjustment, allocation->width);
	gtk_adjustment_set_page_increment (hadjustment, allocation->width / 2);

	gtk_adjustment_set_page_size (vadjustment, allocation->height);
	gtk_adjustment_set_page_increment (vadjustment, allocation->height / 2);

	scroll_to (GNOME_CANVAS (widget),
		   gtk_adjustment_get_value (hadjustment),
		   gtk_adjustment_get_value (vadjustment));

	g_object_thaw_notify (G_OBJECT (hadjustment));
	g_object_thaw_notify (G_OBJECT (vadjustment));
}

/* Emits an event for an item in the canvas, be it the current item, grabbed
 * item, or focused item, as appropriate.
 */

static gint
emit_event (GnomeCanvas *canvas, GdkEvent *event)
{
	GdkEvent *ev;
	gint finished;
	GnomeCanvasItem *item;
	GnomeCanvasItem *parent;
	guint mask;

	/* Perform checks for grabbed items */

	if (canvas->grabbed_item &&
	    !is_descendant (canvas->current_item, canvas->grabbed_item)) {
		/* I think this warning is annoying and I don't know what it's for
		 * so I'll disable it for now.
		 */
/*                g_warning ("emit_event() returning FALSE!\n");*/
		return FALSE;
        }

	if (canvas->grabbed_item) {
		switch (event->type) {
		case GDK_ENTER_NOTIFY:
			mask = GDK_ENTER_NOTIFY_MASK;
			break;

		case GDK_LEAVE_NOTIFY:
			mask = GDK_LEAVE_NOTIFY_MASK;
			break;

		case GDK_MOTION_NOTIFY:
			mask = GDK_POINTER_MOTION_MASK;
			break;

		case GDK_BUTTON_PRESS:
		case GDK_2BUTTON_PRESS:
		case GDK_3BUTTON_PRESS:
			mask = GDK_BUTTON_PRESS_MASK;
			break;

		case GDK_BUTTON_RELEASE:
			mask = GDK_BUTTON_RELEASE_MASK;
			break;

		case GDK_KEY_PRESS:
			mask = GDK_KEY_PRESS_MASK;
			break;

		case GDK_KEY_RELEASE:
			mask = GDK_KEY_RELEASE_MASK;
			break;

		case GDK_SCROLL:
			mask = GDK_SCROLL_MASK;
			break;

		default:
			mask = 0;
			break;
		}

		if (!(mask & canvas->grabbed_event_mask))
			return FALSE;
	}

	/* Convert to world coordinates -- we have two cases because of diferent
	 * offsets of the fields in the event structures.
	 */

	ev = gdk_event_copy (event);

	switch (ev->type)
        {
	case GDK_ENTER_NOTIFY:
	case GDK_LEAVE_NOTIFY:
		gnome_canvas_window_to_world (canvas,
					      ev->crossing.x, ev->crossing.y,
					      &ev->crossing.x, &ev->crossing.y);
		break;

	case GDK_MOTION_NOTIFY:
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
                gnome_canvas_window_to_world (canvas,
                                              ev->motion.x, ev->motion.y,
                                              &ev->motion.x, &ev->motion.y);
                break;

	default:
		break;
	}

	/* Choose where we send the event */

	item = canvas->current_item;

	if (canvas->focused_item
	    && ((event->type == GDK_KEY_PRESS) ||
		(event->type == GDK_KEY_RELEASE) ||
		(event->type == GDK_FOCUS_CHANGE)))
		item = canvas->focused_item;

	/* The event is propagated up the hierarchy (for if someone connected to
	 * a group instead of a leaf event), and emission is stopped if a
	 * handler returns TRUE, just like for GtkWidget events.
	 */

	finished = FALSE;

	while (item && !finished) {
		g_object_ref (G_OBJECT (item));

		g_signal_emit (item, item_signals[ITEM_EVENT], 0,
			       ev, &finished);

		parent = item->parent;
		g_object_unref (G_OBJECT (item));

		item = parent;
	}

	gdk_event_free (ev);

	return finished;
}

/* Re-picks the current item in the canvas, based on the event's coordinates.
 * Also emits enter/leave events for items as appropriate.
 */
static gint
pick_current_item (GnomeCanvas *canvas, GdkEvent *event)
{
	gint button_down;
	gdouble x, y;
	gint cx, cy;
	gint retval;

	retval = FALSE;

	/* If a button is down, we'll perform enter and leave events on the
	 * current item, but not enter on any other item.  This is more or less
	 * like X pointer grabbing for canvas items.
	 */
	button_down = canvas->state & (GDK_BUTTON1_MASK
				       | GDK_BUTTON2_MASK
				       | GDK_BUTTON3_MASK
				       | GDK_BUTTON4_MASK
				       | GDK_BUTTON5_MASK);
	if (!button_down)
		canvas->left_grabbed_item = FALSE;

	/* Save the event in the canvas.  This is used to synthesize enter and
	 * leave events in case the current item changes.  It is also used to
	 * re-pick the current item if the current one gets deleted.  Also,
	 * synthesize an enter event.
	 */
	if (event != &canvas->pick_event) {
		if ((event->type == GDK_MOTION_NOTIFY) || (event->type == GDK_BUTTON_RELEASE)) {
			/* these fields have the same offsets in both types of events */

			canvas->pick_event.crossing.type       = GDK_ENTER_NOTIFY;
			canvas->pick_event.crossing.window     = event->motion.window;
			canvas->pick_event.crossing.send_event = event->motion.send_event;
			canvas->pick_event.crossing.subwindow  = NULL;
			canvas->pick_event.crossing.x          = event->motion.x;
			canvas->pick_event.crossing.y          = event->motion.y;
			canvas->pick_event.crossing.mode       = GDK_CROSSING_NORMAL;
			canvas->pick_event.crossing.detail     = GDK_NOTIFY_NONLINEAR;
			canvas->pick_event.crossing.focus      = FALSE;
			canvas->pick_event.crossing.state      = event->motion.state;

			/* these fields don't have the same offsets in both types of events */

			if (event->type == GDK_MOTION_NOTIFY) {
				canvas->pick_event.crossing.x_root = event->motion.x_root;
				canvas->pick_event.crossing.y_root = event->motion.y_root;
			} else {
				canvas->pick_event.crossing.x_root = event->button.x_root;
				canvas->pick_event.crossing.y_root = event->button.y_root;
			}
		} else
			canvas->pick_event = *event;
	}

	/* Don't do anything else if this is a recursive call */

	if (canvas->in_repick)
		return retval;

	/* LeaveNotify means that there is no current item, so we don't look for one */

	if (canvas->pick_event.type != GDK_LEAVE_NOTIFY) {
		/* these fields don't have the same offsets in both types of events */

		if (canvas->pick_event.type == GDK_ENTER_NOTIFY) {
			x = canvas->pick_event.crossing.x - canvas->zoom_xofs;
			y = canvas->pick_event.crossing.y - canvas->zoom_yofs;
		} else {
			x = canvas->pick_event.motion.x - canvas->zoom_xofs;
			y = canvas->pick_event.motion.y - canvas->zoom_yofs;
		}

		/* canvas pixel coords */

		cx = (gint) (x + 0.5);
		cy = (gint) (y + 0.5);

		/* world coords */

		x = canvas->scroll_x1 + x / canvas->pixels_per_unit;
		y = canvas->scroll_y1 + y / canvas->pixels_per_unit;

		/* find the closest item */

		if (canvas->root->flags & GNOME_CANVAS_ITEM_VISIBLE)
			canvas->new_current_item = gnome_canvas_item_invoke_point (canvas->root, x, y, cx, cy);
		else
			canvas->new_current_item = NULL;
	} else
		canvas->new_current_item = NULL;

	if ((canvas->new_current_item == canvas->current_item) && !canvas->left_grabbed_item)
		return retval; /* current item did not change */

	/* Synthesize events for old and new current items */

	if ((canvas->new_current_item != canvas->current_item)
	    && (canvas->current_item != NULL)
	    && !canvas->left_grabbed_item) {
		GdkEvent new_event;

		new_event = canvas->pick_event;
		new_event.type = GDK_LEAVE_NOTIFY;

		new_event.crossing.detail = GDK_NOTIFY_ANCESTOR;
		new_event.crossing.subwindow = NULL;
		canvas->in_repick = TRUE;
		retval = emit_event (canvas, &new_event);
		canvas->in_repick = FALSE;
	}

	/* new_current_item may have been set to NULL during the call to emit_event() above */

	if ((canvas->new_current_item != canvas->current_item) && button_down) {
		canvas->left_grabbed_item = TRUE;
		return retval;
	}

	/* Handle the rest of cases */

	canvas->left_grabbed_item = FALSE;
	canvas->current_item = canvas->new_current_item;

	if (canvas->current_item != NULL) {
		GdkEvent new_event;

		new_event = canvas->pick_event;
		new_event.type = GDK_ENTER_NOTIFY;
		new_event.crossing.detail = GDK_NOTIFY_ANCESTOR;
		new_event.crossing.subwindow = NULL;
		retval = emit_event (canvas, &new_event);
	}

	return retval;
}

/* Button event handler for the canvas */
static gint
gnome_canvas_button (GtkWidget *widget, GdkEventButton *event)
{
	GnomeCanvas *canvas;
	GtkLayout *layout;
	GdkWindow *bin_window;
	gint mask;
	gint retval;

	g_return_val_if_fail (GNOME_IS_CANVAS (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	retval = FALSE;

	canvas = GNOME_CANVAS (widget);

	layout = GTK_LAYOUT (canvas);
	bin_window = gtk_layout_get_bin_window (layout);

	/*
	 * dispatch normally regardless of the event's window if an item has
	 * has a pointer grab in effect
	 */
	if (!canvas->grabbed_item && event->window != bin_window)
		return retval;

	switch (event->button) {
	case 1:
		mask = GDK_BUTTON1_MASK;
		break;
	case 2:
		mask = GDK_BUTTON2_MASK;
		break;
	case 3:
		mask = GDK_BUTTON3_MASK;
		break;
	case 4:
		mask = GDK_BUTTON4_MASK;
		break;
	case 5:
		mask = GDK_BUTTON5_MASK;
		break;
	default:
		mask = 0;
	}

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		/* Pick the current item as if the button were not pressed, and
		 * then process the event.
		 */
		canvas->state = event->state;
		pick_current_item (canvas, (GdkEvent *) event);
		canvas->state ^= mask;
		retval = emit_event (canvas, (GdkEvent *) event);
		break;

	case GDK_BUTTON_RELEASE:
		/* Process the event as if the button were pressed, then repick
		 * after the button has been released
		 */
		canvas->state = event->state;
		retval = emit_event (canvas, (GdkEvent *) event);
		event->state ^= mask;
		canvas->state = event->state;
		pick_current_item (canvas, (GdkEvent *) event);
		event->state ^= mask;
		break;

	default:
		g_assert_not_reached ();
	}

	return retval;
}

/* Motion event handler for the canvas */
static gint
gnome_canvas_motion (GtkWidget *widget, GdkEventMotion *event)
{
	GnomeCanvas *canvas;
	GtkLayout *layout;
	GdkWindow *bin_window;

	g_return_val_if_fail (GNOME_IS_CANVAS (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	canvas = GNOME_CANVAS (widget);

	layout = GTK_LAYOUT (widget);
	bin_window = gtk_layout_get_bin_window (layout);

	if (event->window != bin_window)
		return FALSE;

	canvas->state = event->state;
	pick_current_item (canvas, (GdkEvent *) event);
	return emit_event (canvas, (GdkEvent *) event);
}

/* Key event handler for the canvas */
static gboolean
gnome_canvas_key (GtkWidget *widget, GdkEventKey *event)
{
	GnomeCanvas *canvas;

	g_return_val_if_fail (GNOME_IS_CANVAS (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	canvas = GNOME_CANVAS (widget);

	if (!emit_event (canvas, (GdkEvent *) event)) {
		GtkWidgetClass *widget_class;

		widget_class = GTK_WIDGET_CLASS (canvas_parent_class);

		if (event->type == GDK_KEY_PRESS) {
			if (widget_class->key_press_event)
				return (* widget_class->key_press_event) (widget, event);
		} else if (event->type == GDK_KEY_RELEASE) {
			if (widget_class->key_release_event)
				return (* widget_class->key_release_event) (widget, event);
		} else
			g_assert_not_reached ();

		return FALSE;
	} else
		return TRUE;
}

/* Crossing event handler for the canvas */
static gint
gnome_canvas_crossing (GtkWidget *widget, GdkEventCrossing *event)
{
	GnomeCanvas *canvas;
	GtkLayout *layout;
	GdkWindow *bin_window;

	g_return_val_if_fail (GNOME_IS_CANVAS (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	canvas = GNOME_CANVAS (widget);

	layout = GTK_LAYOUT (canvas);
	bin_window = gtk_layout_get_bin_window (layout);

	if (event->window != bin_window)
		return FALSE;

	canvas->state = event->state;
	return pick_current_item (canvas, (GdkEvent *) event);
}

/* Focus in handler for the canvas */
static gint
gnome_canvas_focus_in (GtkWidget *widget, GdkEventFocus *event)
{
	GnomeCanvas *canvas;

	/* XXX Can't access flags directly anymore, but is it really needed?
	 *     If so, could we call gtk_widget_send_focus_change() instead? */
#if 0
	GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
#endif

	canvas = GNOME_CANVAS (widget);

	if (canvas->focused_item)
		return emit_event (canvas, (GdkEvent *) event);
	else
		return FALSE;
}

/* Focus out handler for the canvas */
static gint
gnome_canvas_focus_out (GtkWidget *widget, GdkEventFocus *event)
{
	GnomeCanvas *canvas;

	/* XXX Can't access flags directly anymore, but is it really needed?
	 *     If so, could we call gtk_widget_send_focus_change() instead? */
#if 0
	GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);
#endif

	canvas = GNOME_CANVAS (widget);

	if (canvas->focused_item)
		return emit_event (canvas, (GdkEvent *) event);
	else
		return FALSE;
}

#define REDRAW_QUANTUM_SIZE 512

static void
gnome_canvas_paint_rect (GnomeCanvas *canvas, gint x0, gint y0, gint x1, gint y1)
{
	GtkWidget *widget;
	GtkLayout *layout;
	GdkWindow *bin_window;
	GtkAllocation allocation;
	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;
	gint draw_x1, draw_y1;
	gint draw_x2, draw_y2;
	gint draw_width, draw_height;
	gdouble hadjustment_value;
	gdouble vadjustment_value;
        GdkPixmap *pixmap;
        GdkVisual *visual;
        gint depth;

	g_return_if_fail (!canvas->need_update);

	widget = GTK_WIDGET (canvas);
	gtk_widget_get_allocation (widget, &allocation);

	layout = GTK_LAYOUT (canvas);
	bin_window = gtk_layout_get_bin_window (layout);
	hadjustment = gtk_layout_get_hadjustment (layout);
	vadjustment = gtk_layout_get_vadjustment (layout);

	hadjustment_value = gtk_adjustment_get_value (hadjustment);
	vadjustment_value = gtk_adjustment_get_value (vadjustment);

	draw_x1 = MAX (x0, hadjustment_value - canvas->zoom_xofs);
	draw_y1 = MAX (y0, vadjustment_value - canvas->zoom_yofs);
	draw_x2 = MIN (draw_x1 + allocation.width, x1);
	draw_y2 = MIN (draw_y1 + allocation.height, y1);

	draw_width = draw_x2 - draw_x1;
	draw_height = draw_y2 - draw_y1;

	if (draw_width < 1 || draw_height < 1)
		return;

	canvas->redraw_x1 = draw_x1;
	canvas->redraw_y1 = draw_y1;
	canvas->redraw_x2 = draw_x2;
	canvas->redraw_y2 = draw_y2;
	canvas->draw_xofs = draw_x1;
	canvas->draw_yofs = draw_y1;

        visual = gtk_widget_get_visual (widget);
        depth = gdk_visual_get_depth (visual);

        pixmap = gdk_pixmap_new (bin_window,
                                 draw_width, draw_height, depth);

        g_signal_emit (G_OBJECT (canvas), canvas_signals[DRAW_BACKGROUND], 0, pixmap,
                       draw_x1, draw_y1, draw_width, draw_height);

        if (canvas->root->flags & GNOME_CANVAS_ITEM_VISIBLE)
                (* GNOME_CANVAS_ITEM_GET_CLASS (canvas->root)->draw) (
                        canvas->root, pixmap,
                        draw_x1, draw_y1,
                        draw_width, draw_height);

        /* Copy the pixmap to the window and clean up */

        gdk_draw_drawable (bin_window,
                           canvas->pixmap_gc,
                           pixmap,
                           0, 0,
                           draw_x1 + canvas->zoom_xofs,
                           draw_y1 + canvas->zoom_yofs,
                           draw_width, draw_height);

        g_object_unref (pixmap);
}

/* Expose handler for the canvas */
static gint
gnome_canvas_expose (GtkWidget *widget, GdkEventExpose *event)
{
	GnomeCanvas *canvas;
	GtkLayout *layout;
	GdkWindow *bin_window;
	GdkRectangle *rects;
	gint n_rects;
	gint i;

	canvas = GNOME_CANVAS (widget);

	layout = GTK_LAYOUT (canvas);
	bin_window = gtk_layout_get_bin_window (layout);

	if (!gtk_widget_is_drawable (widget) || (event->window != bin_window))
		return FALSE;

#ifdef VERBOSE
	g_print ("Expose\n");
#endif

	gdk_region_get_rectangles (event->region, &rects, &n_rects);

	for (i = 0; i < n_rects; i++) {
		ArtIRect rect;

		rect.x0 = rects[i].x - canvas->zoom_xofs;
		rect.y0 = rects[i].y - canvas->zoom_yofs;
		rect.x1 = rects[i].x + rects[i].width - canvas->zoom_xofs;
		rect.y1 = rects[i].y + rects[i].height - canvas->zoom_yofs;

		if (canvas->need_update || canvas->need_redraw) {
			ArtUta *uta;

			/* Update or drawing is scheduled, so just mark exposed area as dirty */
			uta = art_uta_from_irect (&rect);
			gnome_canvas_request_redraw_uta (canvas, uta);
		} else {
			/* No pending updates, draw exposed area immediately */
			gnome_canvas_paint_rect (canvas, rect.x0, rect.y0, rect.x1, rect.y1);

			/* And call expose on parent container class */
			if (GTK_WIDGET_CLASS (canvas_parent_class)->expose_event)
				(* GTK_WIDGET_CLASS (canvas_parent_class)->expose_event) (
					widget, event);
		}
	}

	g_free (rects);

	return FALSE;
}

/* Repaints the areas in the canvas that need it */
static void
paint (GnomeCanvas *canvas)
{
	GtkWidget *widget;
	GtkLayout *layout;
	GdkWindow *bin_window;
	GtkAllocation allocation;
	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;
	ArtIRect *rects;
	gint n_rects, i;
	ArtIRect visible_rect;
	GdkRegion *region;
	gdouble hadjustment_value;
	gdouble vadjustment_value;

	widget = GTK_WIDGET (canvas);
	gtk_widget_get_allocation (widget, &allocation);

	layout = GTK_LAYOUT (canvas);
	bin_window = gtk_layout_get_bin_window (layout);
	hadjustment = gtk_layout_get_hadjustment (layout);
	vadjustment = gtk_layout_get_vadjustment (layout);

	hadjustment_value = gtk_adjustment_get_value (hadjustment);
	vadjustment_value = gtk_adjustment_get_value (vadjustment);

	/* Extract big rectangles from the microtile array */

	rects = art_rect_list_from_uta (canvas->redraw_area,
					REDRAW_QUANTUM_SIZE, REDRAW_QUANTUM_SIZE,
					&n_rects);

	art_uta_free (canvas->redraw_area);
	canvas->redraw_area = NULL;
	canvas->need_redraw = FALSE;

	/* Turn those rectangles into a GdkRegion for exposing */

	visible_rect.x0 = hadjustment_value - canvas->zoom_xofs;
	visible_rect.y0 = vadjustment_value - canvas->zoom_yofs;
	visible_rect.x1 = visible_rect.x0 + allocation.width;
	visible_rect.y1 = visible_rect.y0 + allocation.height;

	for (i = 0; i < n_rects; i++) {
		ArtIRect clipped;

		art_irect_intersect (&clipped, &visible_rect, rects + i);
		if (!art_irect_empty (&clipped)) {
			GdkRectangle gdkrect;

			gdkrect.x = clipped.x0 + canvas->zoom_xofs;
			gdkrect.y = clipped.y0 + canvas->zoom_yofs;
			gdkrect.width = clipped.x1 - clipped.x0;
			gdkrect.height = clipped.y1 - clipped.y0;

			region = gdk_region_rectangle (&gdkrect);
			gdk_window_invalidate_region (bin_window, region, FALSE);
			gdk_region_destroy (region);
		}
	}

	art_free (rects);

	canvas->redraw_x1 = 0;
	canvas->redraw_y1 = 0;
	canvas->redraw_x2 = 0;
	canvas->redraw_y2 = 0;
}

static void
gnome_canvas_draw_background (GnomeCanvas *canvas, GdkDrawable *drawable,
			      gint x, gint y, gint width, gint height)
{
	GtkStyle *style;

	style = gtk_widget_get_style (GTK_WIDGET (canvas));

	/* By default, we use the style background. */
	gdk_gc_set_foreground (canvas->pixmap_gc,
			       &style->bg[GTK_STATE_NORMAL]);
	gdk_draw_rectangle (drawable,
			    canvas->pixmap_gc,
			    TRUE,
			    0, 0,
			    width, height);
}

static void
do_update (GnomeCanvas *canvas)
{
	/* Cause the update if necessary */

update_again:
	if (canvas->need_update) {
                cairo_matrix_t w2c;

		/* We start updating root with w2c matrix */
                gnome_canvas_w2c_matrix (canvas, &w2c);

		gnome_canvas_item_invoke_update (canvas->root, &w2c, 0);

		canvas->need_update = FALSE;
	}

	/* Pick new current item */

	while (canvas->need_repick) {
		canvas->need_repick = FALSE;
		pick_current_item (canvas, &canvas->pick_event);
	}

	/* it is possible that during picking we emitted an event in which
	   the user then called some function which then requested update
	   of something.  Without this we'd be left in a state where
	   need_update would have been left TRUE and the canvas would have
	   been left unpainted. */
	if (canvas->need_update) {
		goto update_again;
	}

	/* Paint if able to */

	if (gtk_widget_is_drawable (GTK_WIDGET (canvas)) && canvas->need_redraw)
		paint (canvas);
}

/* Idle handler for the canvas.  It deals with pending updates and redraws. */
static gboolean
idle_handler (gpointer data)
{
	GnomeCanvas *canvas;

	GDK_THREADS_ENTER ();

	canvas = GNOME_CANVAS (data);

	do_update (canvas);

	/* Reset idle id */
	canvas->idle_id = 0;

	GDK_THREADS_LEAVE ();

	return FALSE;
}

/* Convenience function to add an idle handler to a canvas */
static void
add_idle (GnomeCanvas *canvas)
{
	g_assert (canvas->need_update || canvas->need_redraw);

	if (!canvas->idle_id)
		canvas->idle_id = g_idle_add_full (CANVAS_IDLE_PRIORITY,
						   idle_handler,
						   canvas,
						   NULL);

/*	canvas->idle_id = gtk_idle_add (idle_handler, canvas); */
}

/**
 * gnome_canvas_root:
 * @canvas: A canvas.
 *
 * Queries the root group of a canvas.
 *
 * Return value: The root group of the specified canvas.
 **/
GnomeCanvasGroup *
gnome_canvas_root (GnomeCanvas *canvas)
{
	g_return_val_if_fail (GNOME_IS_CANVAS (canvas), NULL);

	return GNOME_CANVAS_GROUP (canvas->root);
}

/**
 * gnome_canvas_set_scroll_region:
 * @canvas: A canvas.
 * @x1: Leftmost limit of the scrolling region.
 * @y1: Upper limit of the scrolling region.
 * @x2: Rightmost limit of the scrolling region.
 * @y2: Lower limit of the scrolling region.
 *
 * Sets the scrolling region of a canvas to the specified rectangle.  The canvas
 * will then be able to scroll only within this region.  The view of the canvas
 * is adjusted as appropriate to display as much of the new region as possible.
 **/
void
gnome_canvas_set_scroll_region (GnomeCanvas *canvas,
                                gdouble x1,
                                gdouble y1,
                                gdouble x2,
                                gdouble y2)
{
	GtkLayout *layout;
	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;
	gdouble hadjustment_value;
	gdouble vadjustment_value;
	gdouble wxofs, wyofs;
	gint xofs, yofs;

	g_return_if_fail (GNOME_IS_CANVAS (canvas));

	layout = GTK_LAYOUT (canvas);
	hadjustment = gtk_layout_get_hadjustment (layout);
	vadjustment = gtk_layout_get_vadjustment (layout);

	hadjustment_value = gtk_adjustment_get_value (hadjustment);
	vadjustment_value = gtk_adjustment_get_value (vadjustment);

	/*
	 * Set the new scrolling region.  If possible, do not move the visible contents of the
	 * canvas.
	 */

	gnome_canvas_c2w (canvas,
			  hadjustment_value + canvas->zoom_xofs,
			  vadjustment_value + canvas->zoom_yofs,
			  /*canvas->zoom_xofs,
			  canvas->zoom_yofs,*/
			  &wxofs, &wyofs);

	canvas->scroll_x1 = x1;
	canvas->scroll_y1 = y1;
	canvas->scroll_x2 = x2;
	canvas->scroll_y2 = y2;

	gnome_canvas_w2c (canvas, wxofs, wyofs, &xofs, &yofs);

	scroll_to (canvas, xofs, yofs);

	canvas->need_repick = TRUE;
#if 0
	/* todo: should be requesting update */
	(* GNOME_CANVAS_ITEM_CLASS (canvas->root->object.klass)->update) (
		canvas->root, NULL, NULL, 0);
#endif
}

/**
 * gnome_canvas_get_scroll_region:
 * @canvas: A canvas.
 * @x1: Leftmost limit of the scrolling region (return value).
 * @y1: Upper limit of the scrolling region (return value).
 * @x2: Rightmost limit of the scrolling region (return value).
 * @y2: Lower limit of the scrolling region (return value).
 *
 * Queries the scrolling region of a canvas.
 **/
void
gnome_canvas_get_scroll_region (GnomeCanvas *canvas,
                                gdouble *x1,
                                gdouble *y1,
                                gdouble *x2,
                                gdouble *y2)
{
	g_return_if_fail (GNOME_IS_CANVAS (canvas));

	if (x1)
		*x1 = canvas->scroll_x1;

	if (y1)
		*y1 = canvas->scroll_y1;

	if (x2)
		*x2 = canvas->scroll_x2;

	if (y2)
		*y2 = canvas->scroll_y2;
}

/**
 * gnome_canvas_set_center_scroll_region:
 * @canvas: A canvas.
 * @center_scroll_region: Whether to center the scrolling region in the canvas
 * window when it is smaller than the canvas' allocation.
 *
 * When the scrolling region of the canvas is smaller than the canvas window,
 * e.g.  the allocation of the canvas, it can be either centered on the window
 * or simply made to be on the upper-left corner on the window.  This function
 * lets you configure this property.
 **/
void
gnome_canvas_set_center_scroll_region (GnomeCanvas *canvas,
                                       gboolean center_scroll_region)
{
	GtkLayout *layout;
	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;
	gdouble hadjustment_value;
	gdouble vadjustment_value;

	g_return_if_fail (GNOME_IS_CANVAS (canvas));

	layout = GTK_LAYOUT (canvas);
	hadjustment = gtk_layout_get_hadjustment (layout);
	vadjustment = gtk_layout_get_vadjustment (layout);

	hadjustment_value = gtk_adjustment_get_value (hadjustment);
	vadjustment_value = gtk_adjustment_get_value (vadjustment);

	canvas->center_scroll_region = center_scroll_region != 0;

	scroll_to (canvas, hadjustment_value, vadjustment_value);
}

/**
 * gnome_canvas_get_center_scroll_region:
 * @canvas: A canvas.
 *
 * Returns whether the canvas is set to center the scrolling region in the window
 * if the former is smaller than the canvas' allocation.
 *
 * Return value: Whether the scroll region is being centered in the canvas window.
 **/
gboolean
gnome_canvas_get_center_scroll_region (GnomeCanvas *canvas)
{
	g_return_val_if_fail (GNOME_IS_CANVAS (canvas), FALSE);

	return canvas->center_scroll_region ? TRUE : FALSE;
}

/**
 * gnome_canvas_set_pixels_per_unit:
 * @canvas: A canvas.
 * @n: The number of pixels that correspond to one canvas unit.
 *
 * Sets the zooming factor of a canvas by specifying the number of pixels that
 * correspond to one canvas unit.
 *
 * The anchor point for zooming, i.e. the point that stays fixed and all others
 * zoom inwards or outwards from it, depends on whether the canvas is set to
 * center the scrolling region or not.  You can control this using the
 * gnome_canvas_set_center_scroll_region() function.  If the canvas is set to
 * center the scroll region, then the center of the canvas window is used as the
 * anchor point for zooming.  Otherwise, the upper-left corner of the canvas
 * window is used as the anchor point.
 **/
void
gnome_canvas_set_pixels_per_unit (GnomeCanvas *canvas, gdouble n)
{
	GtkLayout *layout;
	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;
	gdouble ax, ay;
	gint x1, y1;
	gint anchor_x, anchor_y;

	g_return_if_fail (GNOME_IS_CANVAS (canvas));
	g_return_if_fail (n > GNOME_CANVAS_EPSILON);

	layout = GTK_LAYOUT (canvas);
	hadjustment = gtk_layout_get_hadjustment (layout);
	vadjustment = gtk_layout_get_vadjustment (layout);

	if (canvas->center_scroll_region) {
		GtkAllocation allocation;

		gtk_widget_get_allocation (GTK_WIDGET (canvas), &allocation);

		anchor_x = allocation.width / 2;
		anchor_y = allocation.height / 2;
	} else
		anchor_x = anchor_y = 0;

	/* Find the coordinates of the anchor point in units. */
	if (hadjustment) {
		gdouble value = gtk_adjustment_get_value (hadjustment);
		ax = (value + anchor_x) / canvas->pixels_per_unit +
			canvas->scroll_x1 + canvas->zoom_xofs;
	} else {
		ax = (0.0 + anchor_x) / canvas->pixels_per_unit +
			canvas->scroll_x1 + canvas->zoom_xofs;
	}
	if (vadjustment) {
		gdouble value = gtk_adjustment_get_value (vadjustment);
		ay = (value + anchor_y) / canvas->pixels_per_unit +
			canvas->scroll_y1 + canvas->zoom_yofs;
	} else {
		ay = (0.0 + anchor_y) / canvas->pixels_per_unit +
			canvas->scroll_y1 + canvas->zoom_yofs;
	}

	/* Now calculate the new offset of the upper left corner. */
	x1 = ((ax - canvas->scroll_x1) * n) - anchor_x;
	y1 = ((ay - canvas->scroll_y1) * n) - anchor_y;

	canvas->pixels_per_unit = n;

	scroll_to (canvas, x1, y1);

	if (!(canvas->root->flags & GNOME_CANVAS_ITEM_NEED_AFFINE)) {
		canvas->root->flags |= GNOME_CANVAS_ITEM_NEED_AFFINE;
		gnome_canvas_request_update (canvas);
	}

	canvas->need_repick = TRUE;
}

/**
 * gnome_canvas_scroll_to:
 * @canvas: A canvas.
 * @cx: Horizontal scrolling offset in canvas pixel units.
 * @cy: Vertical scrolling offset in canvas pixel units.
 *
 * Makes a canvas scroll to the specified offsets, given in canvas pixel units.
 * The canvas will adjust the view so that it is not outside the scrolling
 * region.  This function is typically not used, as it is better to hook
 * scrollbars to the canvas layout's scrolling adjusments.
 **/
void
gnome_canvas_scroll_to (GnomeCanvas *canvas, gint cx, gint cy)
{
	g_return_if_fail (GNOME_IS_CANVAS (canvas));

	scroll_to (canvas, cx, cy);
}

/**
 * gnome_canvas_get_scroll_offsets:
 * @canvas: A canvas.
 * @cx: Horizontal scrolling offset (return value).
 * @cy: Vertical scrolling offset (return value).
 *
 * Queries the scrolling offsets of a canvas.  The values are returned in canvas
 * pixel units.
 **/
void
gnome_canvas_get_scroll_offsets (GnomeCanvas *canvas, gint *cx, gint *cy)
{
	GtkLayout *layout;
	GtkAdjustment *adjustment;

	g_return_if_fail (GNOME_IS_CANVAS (canvas));

	layout = GTK_LAYOUT (canvas);

	if (cx) {
		adjustment = gtk_layout_get_hadjustment (layout);
		*cx = (gint) gtk_adjustment_get_value (adjustment);
	}

	if (cy) {
		adjustment = gtk_layout_get_vadjustment (layout);
		*cy = (gint) gtk_adjustment_get_value (adjustment);
	}
}

/**
 * gnome_canvas_get_item_at:
 * @canvas: A canvas.
 * @x: X position in world coordinates.
 * @y: Y position in world coordinates.
 *
 * Looks for the item that is under the specified position, which must be
 * specified in world coordinates.
 *
 * Return value: The sought item, or NULL if no item is at the specified
 * coordinates.
 **/
GnomeCanvasItem *
gnome_canvas_get_item_at (GnomeCanvas *canvas, gdouble x, gdouble y)
{
	gint cx, cy;

	g_return_val_if_fail (GNOME_IS_CANVAS (canvas), NULL);

	gnome_canvas_w2c (canvas, x, y, &cx, &cy);

	return gnome_canvas_item_invoke_point (canvas->root, x, y, cx, cy);
}

/* Queues an update of the canvas */
static void
gnome_canvas_request_update (GnomeCanvas *canvas)
{
	GNOME_CANVAS_GET_CLASS (canvas)->request_update (canvas);
}

static void
gnome_canvas_request_update_real (GnomeCanvas *canvas)
{
	if (canvas->need_update)
		return;

	canvas->need_update = TRUE;
	if (gtk_widget_get_mapped ((GtkWidget *) canvas))
		add_idle (canvas);
}

/* Computes the union of two microtile arrays while clipping the result to the
 * specified rectangle.  Any of the specified utas can be NULL, in which case it
 * is taken to be an empty region.
 */
static ArtUta *
uta_union_clip (ArtUta *uta1, ArtUta *uta2, ArtIRect *clip)
{
	ArtUta *uta;
	ArtUtaBbox *utiles;
	gint clip_x1, clip_y1, clip_x2, clip_y2;
	gint union_x1, union_y1, union_x2, union_y2;
	gint new_x1, new_y1, new_x2, new_y2;
	gint x, y;
	gint ofs, ofs1, ofs2;

	g_assert (clip != NULL);

	/* Compute the tile indices for the clipping rectangle */

	clip_x1 = clip->x0 >> ART_UTILE_SHIFT;
	clip_y1 = clip->y0 >> ART_UTILE_SHIFT;
	clip_x2 = (clip->x1 >> ART_UTILE_SHIFT) + 1;
	clip_y2 = (clip->y1 >> ART_UTILE_SHIFT) + 1;

	/* Get the union of the bounds of both utas */

	if (!uta1) {
		if (!uta2)
			return art_uta_new (clip_x1, clip_y1, clip_x1 + 1, clip_y1 + 1);

		union_x1 = uta2->x0;
		union_y1 = uta2->y0;
		union_x2 = uta2->x0 + uta2->width;
		union_y2 = uta2->y0 + uta2->height;
	} else {
		if (!uta2) {
			union_x1 = uta1->x0;
			union_y1 = uta1->y0;
			union_x2 = uta1->x0 + uta1->width;
			union_y2 = uta1->y0 + uta1->height;
		} else {
			union_x1 = MIN (uta1->x0, uta2->x0);
			union_y1 = MIN (uta1->y0, uta2->y0);
			union_x2 = MAX (uta1->x0 + uta1->width, uta2->x0 + uta2->width);
			union_y2 = MAX (uta1->y0 + uta1->height, uta2->y0 + uta2->height);
		}
	}

	/* Clip the union of the bounds */

	new_x1 = MAX (clip_x1, union_x1);
	new_y1 = MAX (clip_y1, union_y1);
	new_x2 = MIN (clip_x2, union_x2);
	new_y2 = MIN (clip_y2, union_y2);

	if (new_x1 >= new_x2 || new_y1 >= new_y2)
		return art_uta_new (clip_x1, clip_y1, clip_x1 + 1, clip_y1 + 1);

	/* Make the new clipped union */

	uta = art_new (ArtUta, 1);
	uta->x0 = new_x1;
	uta->y0 = new_y1;
	uta->width = new_x2 - new_x1;
	uta->height = new_y2 - new_y1;
	uta->utiles = utiles = art_new (ArtUtaBbox, uta->width * uta->height);

	ofs = 0;
	ofs1 = ofs2 = 0;

	for (y = new_y1; y < new_y2; y++) {
		if (uta1)
			ofs1 = (y - uta1->y0) * uta1->width + new_x1 - uta1->x0;

		if (uta2)
			ofs2 = (y - uta2->y0) * uta2->width + new_x1 - uta2->x0;

		for (x = new_x1; x < new_x2; x++) {
			ArtUtaBbox bb1, bb2, bb;

			if (!uta1
			    || x < uta1->x0 || y < uta1->y0
			    || x >= uta1->x0 + uta1->width || y >= uta1->y0 + uta1->height)
				bb1 = 0;
			else
				bb1 = uta1->utiles[ofs1];

			if (!uta2
			    || x < uta2->x0 || y < uta2->y0
			    || x >= uta2->x0 + uta2->width || y >= uta2->y0 + uta2->height)
				bb2 = 0;
			else
				bb2 = uta2->utiles[ofs2];

			if (bb1 == 0)
				bb = bb2;
			else if (bb2 == 0)
				bb = bb1;
			else
				bb = ART_UTA_BBOX_CONS (MIN (ART_UTA_BBOX_X0 (bb1),
							     ART_UTA_BBOX_X0 (bb2)),
							MIN (ART_UTA_BBOX_Y0 (bb1),
							     ART_UTA_BBOX_Y0 (bb2)),
							MAX (ART_UTA_BBOX_X1 (bb1),
							     ART_UTA_BBOX_X1 (bb2)),
							MAX (ART_UTA_BBOX_Y1 (bb1),
							     ART_UTA_BBOX_Y1 (bb2)));

			utiles[ofs] = bb;

			ofs++;
			ofs1++;
			ofs2++;
		}
	}

	return uta;
}

static inline void
get_visible_region (GnomeCanvas *canvas, ArtIRect *visible)
{
	GtkLayout *layout;
	GtkAllocation allocation;
	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;
	gdouble hadjustment_value;
	gdouble vadjustment_value;

	gtk_widget_get_allocation (GTK_WIDGET (canvas), &allocation);

	layout = GTK_LAYOUT (canvas);
	hadjustment = gtk_layout_get_hadjustment (layout);
	vadjustment = gtk_layout_get_vadjustment (layout);

	hadjustment_value = gtk_adjustment_get_value (hadjustment);
	vadjustment_value = gtk_adjustment_get_value (vadjustment);

	visible->x0 = hadjustment_value - canvas->zoom_xofs;
	visible->y0 = vadjustment_value - canvas->zoom_yofs;
	visible->x1 = visible->x0 + allocation.width;
	visible->y1 = visible->y0 + allocation.height;
}

/**
 * gnome_canvas_request_redraw_uta:
 * @canvas: A canvas.
 * @uta: Microtile array that specifies the area to be redrawn.  It will
 * be freed by this function, so the argument you pass will be invalid
 * after you call this function.
 *
 * Informs a canvas that the specified area, given as a microtile array, needs
 * to be repainted.  To be used only by item implementations.
 **/
void
gnome_canvas_request_redraw_uta (GnomeCanvas *canvas,
                                 ArtUta      *uta)
{
	ArtIRect visible;

	g_return_if_fail (GNOME_IS_CANVAS (canvas));
	g_return_if_fail (uta != NULL);

	if (!gtk_widget_is_drawable (GTK_WIDGET (canvas))) {
		art_uta_free (uta);
		return;
	}

	get_visible_region (canvas, &visible);

	if (canvas->need_redraw) {
		ArtUta *new_uta;

		g_assert (canvas->redraw_area != NULL);
		/* ALEX: This can fail if e.g. redraw_uta is called by an item
		   update function and we're called from update_now -> do_update
		   because update_now sets idle_id == 0. There is also some way
		   to get it from the expose handler (see bug #102811).
		   g_assert (canvas->idle_id != 0);  */

		new_uta = uta_union_clip (canvas->redraw_area, uta, &visible);
		art_uta_free (canvas->redraw_area);
		art_uta_free (uta);
		canvas->redraw_area = new_uta;
		if (canvas->idle_id == 0)
			add_idle (canvas);
	} else {
		ArtUta *new_uta;

		g_assert (canvas->redraw_area == NULL);

		new_uta = uta_union_clip (uta, NULL, &visible);
		art_uta_free (uta);
		canvas->redraw_area = new_uta;

		canvas->need_redraw = TRUE;
		add_idle (canvas);
	}
}

/**
 * gnome_canvas_request_redraw:
 * @canvas: A canvas.
 * @x1: Leftmost coordinate of the rectangle to be redrawn.
 * @y1: Upper coordinate of the rectangle to be redrawn.
 * @x2: Rightmost coordinate of the rectangle to be redrawn, plus 1.
 * @y2: Lower coordinate of the rectangle to be redrawn, plus 1.
 *
 * Convenience function that informs a canvas that the specified rectangle needs
 * to be repainted.  This function converts the rectangle to a microtile array
 * and feeds it to gnome_canvas_request_redraw_uta().  The rectangle includes
 * @x1 and @y1, but not @x2 and @y2.  To be used only by item implementations.
 **/
void
gnome_canvas_request_redraw (GnomeCanvas *canvas, gint x1, gint y1, gint x2, gint y2)
{
	ArtUta *uta;
	ArtIRect bbox;
	ArtIRect visible;
	ArtIRect clip;

	g_return_if_fail (GNOME_IS_CANVAS (canvas));

	if (!gtk_widget_is_drawable (GTK_WIDGET (canvas)) || (x1 >= x2) || (y1 >= y2))
		return;

	bbox.x0 = x1;
	bbox.y0 = y1;
	bbox.x1 = x2;
	bbox.y1 = y2;

	get_visible_region (canvas, &visible);

	art_irect_intersect (&clip, &bbox, &visible);

	if (!art_irect_empty (&clip)) {
		uta = art_uta_from_irect (&clip);
		gnome_canvas_request_redraw_uta (canvas, uta);
	}
}

/**
 * gnome_canvas_w2c_affine:
 * @canvas: A canvas.
 * @affine: An affine transformation matrix (return value).
 *
 * Gets the affine transform that converts from world coordinates to canvas
 * pixel coordinates.
 **/
void
gnome_canvas_w2c_affine (GnomeCanvas *canvas, gdouble affine[6])
{
	gdouble zooom;

	g_return_if_fail (GNOME_IS_CANVAS (canvas));
	g_return_if_fail (affine != NULL);

	zooom = canvas->pixels_per_unit;

	affine[0] = zooom;
	affine[1] = 0;
	affine[2] = 0;
	affine[3] = zooom;
	affine[4] = -canvas->scroll_x1 * zooom;
	affine[5] = -canvas->scroll_y1 * zooom;
}

/**
 * gnome_canvas_w2c_matrix:
 * @canvas: A canvas.
 * @matrix: (out): matrix to initialize
 *
 * Gets the transformtion matrix that converts from world coordinates to canvas
 * pixel coordinates.
 **/
void
gnome_canvas_w2c_matrix (GnomeCanvas *canvas, cairo_matrix_t *matrix)
{
	g_return_if_fail (GNOME_IS_CANVAS (canvas));
	g_return_if_fail (matrix != NULL);

	cairo_matrix_init_scale (matrix,
                                 canvas->pixels_per_unit,
                                 canvas->pixels_per_unit);
        cairo_matrix_translate (matrix,
                                -canvas->scroll_x1,
                                -canvas->scroll_y1);
}

/**
 * gnome_canvas_c2w_matrix:
 * @canvas: A canvas.
 * @matrix: (out): matrix to initialize
 *
 * Gets the transformtion matrix that converts from canvas pixel coordinates to
 * world coordinates.
 **/
void
gnome_canvas_c2w_matrix (GnomeCanvas *canvas, cairo_matrix_t *matrix)
{
	g_return_if_fail (GNOME_IS_CANVAS (canvas));
	g_return_if_fail (matrix != NULL);

        cairo_matrix_init_translate (matrix,
                                     canvas->scroll_x1,
                                     canvas->scroll_y1);
	cairo_matrix_scale (matrix,
                            1 / canvas->pixels_per_unit,
                            1 / canvas->pixels_per_unit);
}

/**
 * gnome_canvas_w2c:
 * @canvas: A canvas.
 * @wx: World X coordinate.
 * @wy: World Y coordinate.
 * @cx: X pixel coordinate (return value).
 * @cy: Y pixel coordinate (return value).
 *
 * Converts world coordinates into canvas pixel coordinates.
 **/
void
gnome_canvas_w2c (GnomeCanvas *canvas, gdouble wx, gdouble wy, gint *cx, gint *cy)
{
	gdouble affine[6];
	ArtPoint w, c;

	g_return_if_fail (GNOME_IS_CANVAS (canvas));

	gnome_canvas_w2c_affine (canvas, affine);
	w.x = wx;
	w.y = wy;
	art_affine_point (&c, &w, affine);
	if (cx)
		*cx = floor (c.x + 0.5);
	if (cy)
		*cy = floor (c.y + 0.5);
}

/**
 * gnome_canvas_w2c_d:
 * @canvas: A canvas.
 * @wx: World X coordinate.
 * @wy: World Y coordinate.
 * @cx: X pixel coordinate (return value).
 * @cy: Y pixel coordinate (return value).
 *
 * Converts world coordinates into canvas pixel coordinates.  This
 * version returns coordinates in floating point coordinates, for
 * greater precision.
 **/
void
gnome_canvas_w2c_d (GnomeCanvas *canvas,
                    gdouble wx,
                    gdouble wy,
                    gdouble *cx,
                    gdouble *cy)
{
	gdouble affine[6];
	ArtPoint w, c;

	g_return_if_fail (GNOME_IS_CANVAS (canvas));

	gnome_canvas_w2c_affine (canvas, affine);
	w.x = wx;
	w.y = wy;
	art_affine_point (&c, &w, affine);
	if (cx)
		*cx = c.x;
	if (cy)
		*cy = c.y;
}

/**
 * gnome_canvas_c2w:
 * @canvas: A canvas.
 * @cx: Canvas pixel X coordinate.
 * @cy: Canvas pixel Y coordinate.
 * @wx: X world coordinate (return value).
 * @wy: Y world coordinate (return value).
 *
 * Converts canvas pixel coordinates to world coordinates.
 **/
void
gnome_canvas_c2w (GnomeCanvas *canvas, gint cx, gint cy, gdouble *wx, gdouble *wy)
{
	gdouble affine[6], inv[6];
	ArtPoint w, c;

	g_return_if_fail (GNOME_IS_CANVAS (canvas));

	gnome_canvas_w2c_affine (canvas, affine);
	art_affine_invert (inv, affine);
	c.x = cx;
	c.y = cy;
	art_affine_point (&w, &c, inv);
	if (wx)
		*wx = w.x;
	if (wy)
		*wy = w.y;
}

/**
 * gnome_canvas_window_to_world:
 * @canvas: A canvas.
 * @winx: Window-relative X coordinate.
 * @winy: Window-relative Y coordinate.
 * @worldx: X world coordinate (return value).
 * @worldy: Y world coordinate (return value).
 *
 * Converts window-relative coordinates into world coordinates.  You can use
 * this when you need to convert mouse coordinates into world coordinates, for
 * example.
 **/
void
gnome_canvas_window_to_world (GnomeCanvas *canvas, gdouble winx, gdouble winy,
			      gdouble *worldx, gdouble *worldy)
{
	g_return_if_fail (GNOME_IS_CANVAS (canvas));

	if (worldx)
		*worldx = canvas->scroll_x1 + ((winx - canvas->zoom_xofs)
					       / canvas->pixels_per_unit);

	if (worldy)
		*worldy = canvas->scroll_y1 + ((winy - canvas->zoom_yofs)
					       / canvas->pixels_per_unit);
}

/**
 * gnome_canvas_world_to_window:
 * @canvas: A canvas.
 * @worldx: World X coordinate.
 * @worldy: World Y coordinate.
 * @winx: X window-relative coordinate.
 * @winy: Y window-relative coordinate.
 *
 * Converts world coordinates into window-relative coordinates.
 **/
void
gnome_canvas_world_to_window (GnomeCanvas *canvas, gdouble worldx, gdouble worldy,
			      gdouble *winx, gdouble *winy)
{
	g_return_if_fail (GNOME_IS_CANVAS (canvas));

	if (winx)
		*winx = (canvas->pixels_per_unit)*(worldx - canvas->scroll_x1) + canvas->zoom_xofs;

	if (winy)
		*winy = (canvas->pixels_per_unit)*(worldy - canvas->scroll_y1) + canvas->zoom_yofs;
}

/**
 * gnome_canvas_get_color:
 * @canvas: A canvas.
 * @spec: X color specification, or NULL for "transparent".
 * @color: Returns the allocated color.
 *
 * Allocates a color based on the specified X color specification.  As a
 * convenience to item implementations, it returns TRUE if the color was
 * allocated, or FALSE if the specification was NULL.  A NULL color
 * specification is considered as "transparent" by the canvas.
 *
 * Return value: TRUE if @spec is non-NULL and the color is allocated.  If @spec
 * is NULL, then returns FALSE.
 **/
gint
gnome_canvas_get_color (GnomeCanvas *canvas, const gchar *spec, GdkColor *color)
{
	GdkColormap *colormap;

	g_return_val_if_fail (GNOME_IS_CANVAS (canvas), FALSE);
	g_return_val_if_fail (color != NULL, FALSE);

	if (!spec) {
		color->pixel = 0;
		color->red = 0;
		color->green = 0;
		color->blue = 0;
		return FALSE;
	}

	gdk_color_parse (spec, color);

	colormap = gtk_widget_get_colormap (GTK_WIDGET (canvas));

	gdk_rgb_find_color (colormap, color);

	return TRUE;
}

/**
 * gnome_canvas_get_color_pixel:
 * @canvas: A canvas.
 * @rgba: RGBA color specification.
 *
 * Allocates a color from the RGBA value passed into this function.  The alpha
 * opacity value is discarded, since normal X colors do not support it.
 *
 * Return value: Allocated pixel value corresponding to the specified color.
 **/
gulong
gnome_canvas_get_color_pixel (GnomeCanvas *canvas, guint rgba)
{
	GdkColormap *colormap;
	GdkColor color;

	g_return_val_if_fail (GNOME_IS_CANVAS (canvas), 0);

	color.red = ((rgba & 0xff000000) >> 16) + ((rgba & 0xff000000) >> 24);
	color.green = ((rgba & 0x00ff0000) >> 8) + ((rgba & 0x00ff0000) >> 16);
	color.blue = (rgba & 0x0000ff00) + ((rgba & 0x0000ff00) >> 8);
	color.pixel = 0;

	colormap = gtk_widget_get_colormap (GTK_WIDGET (canvas));

	gdk_rgb_find_color (colormap, &color);

	return color.pixel;
}

/**
 * gnome_canvas_set_stipple_origin:
 * @canvas: A canvas.
 * @gc: GC on which to set the stipple origin.
 *
 * Sets the stipple origin of the specified GC as is appropriate for the canvas,
 * so that it will be aligned with other stipple patterns used by canvas items.
 * This is typically only needed by item implementations.
 **/
void
gnome_canvas_set_stipple_origin (GnomeCanvas *canvas, GdkGC *gc)
{
	g_return_if_fail (GNOME_IS_CANVAS (canvas));
	g_return_if_fail (GDK_IS_GC (gc));

	gdk_gc_set_ts_origin (gc, -canvas->draw_xofs, -canvas->draw_yofs);
}

static gboolean
boolean_handled_accumulator (GSignalInvocationHint *ihint,
			     GValue                *return_accu,
			     const GValue          *handler_return,
			     gpointer               dummy)
{
	gboolean continue_emission;
	gboolean signal_handled;

	signal_handled = g_value_get_boolean (handler_return);
	g_value_set_boolean (return_accu, signal_handled);
	continue_emission = !signal_handled;

	return continue_emission;
}

/* Class initialization function for GnomeCanvasItemClass */
static void
gnome_canvas_item_class_init (GnomeCanvasItemClass *class)
{
	GObjectClass *gobject_class;

	gobject_class = (GObjectClass *) class;

	item_parent_class = g_type_class_peek_parent (class);

	gobject_class->set_property = gnome_canvas_item_set_property;
	gobject_class->get_property = gnome_canvas_item_get_property;

	g_object_class_install_property
		(gobject_class, ITEM_PROP_PARENT,
		 g_param_spec_object ("parent", NULL, NULL,
				      GNOME_TYPE_CANVAS_ITEM,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	item_signals[ITEM_EVENT] =
		g_signal_new ("event",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GnomeCanvasItemClass, event),
			      boolean_handled_accumulator, NULL,
			      gnome_canvas_marshal_BOOLEAN__BOXED,
			      G_TYPE_BOOLEAN, 1,
			      GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	gobject_class->dispose = gnome_canvas_item_dispose;

	class->realize = gnome_canvas_item_realize;
	class->unrealize = gnome_canvas_item_unrealize;
	class->map = gnome_canvas_item_map;
	class->unmap = gnome_canvas_item_unmap;
	class->update = gnome_canvas_item_update;
}
