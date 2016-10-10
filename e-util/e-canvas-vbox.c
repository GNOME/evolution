/*
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

#include <math.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <glib/gi18n.h>

#include "e-canvas.h"
#include "e-canvas-utils.h"
#include "e-canvas-vbox.h"

static void e_canvas_vbox_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void e_canvas_vbox_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void e_canvas_vbox_dispose (GObject *object);

static gint e_canvas_vbox_event   (GnomeCanvasItem *item, GdkEvent *event);
static void e_canvas_vbox_realize (GnomeCanvasItem *item);

static void e_canvas_vbox_reflow (GnomeCanvasItem *item, gint flags);

static void e_canvas_vbox_real_add_item (ECanvasVbox *e_canvas_vbox, GnomeCanvasItem *item);
static void e_canvas_vbox_real_add_item_start (ECanvasVbox *e_canvas_vbox, GnomeCanvasItem *item);
static void e_canvas_vbox_resize_children (GnomeCanvasItem *item);

enum {
	PROP_0,
	PROP_WIDTH,
	PROP_MINIMUM_WIDTH,
	PROP_HEIGHT,
	PROP_SPACING
};

G_DEFINE_TYPE (
	ECanvasVbox,
	e_canvas_vbox,
	GNOME_TYPE_CANVAS_GROUP)

static void
e_canvas_vbox_class_init (ECanvasVboxClass *class)
{
	GObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = (GObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	class->add_item = e_canvas_vbox_real_add_item;
	class->add_item_start = e_canvas_vbox_real_add_item_start;

	object_class->set_property = e_canvas_vbox_set_property;
	object_class->get_property = e_canvas_vbox_get_property;
	object_class->dispose = e_canvas_vbox_dispose;

	/* GnomeCanvasItem method overrides */
	item_class->event = e_canvas_vbox_event;
	item_class->realize = e_canvas_vbox_realize;

	g_object_class_install_property (
		object_class,
		PROP_WIDTH,
		g_param_spec_double (
			"width",
			"Width",
			"Width",
			0.0, G_MAXDOUBLE, 0.0,
			G_PARAM_READWRITE));
	g_object_class_install_property (
		object_class,
		PROP_MINIMUM_WIDTH,
		g_param_spec_double (
			"minimum_width",
			"Minimum width",
			"Minimum Width",
			0.0, G_MAXDOUBLE, 0.0,
			G_PARAM_READWRITE));
	g_object_class_install_property (
		object_class,
		PROP_HEIGHT,
		g_param_spec_double (
			"height",
			"Height",
			"Height",
			0.0, G_MAXDOUBLE, 0.0,
			G_PARAM_READABLE));
	g_object_class_install_property (
		object_class,
		PROP_SPACING,
		g_param_spec_double (
			"spacing",
			"Spacing",
			"Spacing",
			0.0, G_MAXDOUBLE, 0.0,
			G_PARAM_READWRITE));
}

static void
e_canvas_vbox_init (ECanvasVbox *vbox)
{
	vbox->items = NULL;

	vbox->width = 10;
	vbox->minimum_width = 10;
	vbox->height = 10;
	vbox->spacing = 0;

	e_canvas_item_set_reflow_callback (GNOME_CANVAS_ITEM (vbox), e_canvas_vbox_reflow);
}

static void
e_canvas_vbox_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
	GnomeCanvasItem *item;
	ECanvasVbox *e_canvas_vbox;

	item = GNOME_CANVAS_ITEM (object);
	e_canvas_vbox = E_CANVAS_VBOX (object);

	switch (property_id) {
	case PROP_WIDTH:
	case PROP_MINIMUM_WIDTH:
		e_canvas_vbox->minimum_width = g_value_get_double (value);
		e_canvas_vbox_resize_children (item);
		e_canvas_item_request_reflow (item);
		break;
	case PROP_SPACING:
		e_canvas_vbox->spacing = g_value_get_double (value);
		e_canvas_item_request_reflow (item);
		break;
	}
}

static void
e_canvas_vbox_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
	ECanvasVbox *e_canvas_vbox;

	e_canvas_vbox = E_CANVAS_VBOX (object);

	switch (property_id) {
	case PROP_WIDTH:
		g_value_set_double (value, e_canvas_vbox->width);
		break;
	case PROP_MINIMUM_WIDTH:
		g_value_set_double (value, e_canvas_vbox->minimum_width);
		break;
	case PROP_HEIGHT:
		g_value_set_double (value, e_canvas_vbox->height);
		break;
	case PROP_SPACING:
		g_value_set_double (value, e_canvas_vbox->spacing);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

/* Used from g_list_foreach(); disconnects from an item's signals */
static void
disconnect_item_cb (gpointer data,
                    gpointer user_data)
{
	ECanvasVbox *vbox;
	GnomeCanvasItem *item;

	vbox = E_CANVAS_VBOX (user_data);

	item = GNOME_CANVAS_ITEM (data);
	g_signal_handlers_disconnect_matched (
		item,
		G_SIGNAL_MATCH_DATA,
		0, 0, NULL, NULL,
		vbox);
}

static void
e_canvas_vbox_dispose (GObject *object)
{
	ECanvasVbox *vbox = E_CANVAS_VBOX (object);

	if (vbox->items) {
		g_list_foreach (vbox->items, disconnect_item_cb, vbox);
		g_list_free (vbox->items);
		vbox->items = NULL;
	}

	G_OBJECT_CLASS (e_canvas_vbox_parent_class)->dispose (object);
}

static gint
e_canvas_vbox_event (GnomeCanvasItem *item,
                     GdkEvent *event)
{
	gint return_val = TRUE;

	switch (event->type) {
	case GDK_KEY_PRESS:
		switch (event->key.keyval) {
		case GDK_KEY_Left:
		case GDK_KEY_KP_Left:
		case GDK_KEY_Right:
		case GDK_KEY_KP_Right:
		case GDK_KEY_Down:
		case GDK_KEY_KP_Down:
		case GDK_KEY_Up:
		case GDK_KEY_KP_Up:
		case GDK_KEY_Return:
		case GDK_KEY_KP_Enter:
			return_val = TRUE;
			break;
		default:
			return_val = FALSE;
			break;
		}
		break;
	default:
		return_val = FALSE;
		break;
	}
	if (!return_val) {
		if (GNOME_CANVAS_ITEM_CLASS (e_canvas_vbox_parent_class)->event)
			return GNOME_CANVAS_ITEM_CLASS (e_canvas_vbox_parent_class)->event (item, event);
	}
	return return_val;

}

static void
e_canvas_vbox_realize (GnomeCanvasItem *item)
{
	if (GNOME_CANVAS_ITEM_CLASS (e_canvas_vbox_parent_class)->realize)
		(* GNOME_CANVAS_ITEM_CLASS (e_canvas_vbox_parent_class)->realize) (item);

	e_canvas_vbox_resize_children (item);
	e_canvas_item_request_reflow (item);
}

static void
e_canvas_vbox_remove_item (gpointer data,
                           GObject *where_object_was)
{
	ECanvasVbox *vbox = data;
	vbox->items = g_list_remove (vbox->items, where_object_was);
}

static void
e_canvas_vbox_real_add_item (ECanvasVbox *e_canvas_vbox,
                             GnomeCanvasItem *item)
{
	e_canvas_vbox->items = g_list_append (e_canvas_vbox->items, item);
	g_object_weak_ref (
		G_OBJECT (item),
		e_canvas_vbox_remove_item, e_canvas_vbox);
	if (GNOME_CANVAS_ITEM (e_canvas_vbox)->flags & GNOME_CANVAS_ITEM_REALIZED) {
		gnome_canvas_item_set (
			item,
			"width", (gdouble) e_canvas_vbox->minimum_width,
			NULL);
		e_canvas_item_request_reflow (item);
	}
}

static void
e_canvas_vbox_real_add_item_start (ECanvasVbox *e_canvas_vbox,
                                   GnomeCanvasItem *item)
{
	e_canvas_vbox->items = g_list_prepend (e_canvas_vbox->items, item);
	g_object_weak_ref (
		G_OBJECT (item),
		e_canvas_vbox_remove_item, e_canvas_vbox);
	if (GNOME_CANVAS_ITEM (e_canvas_vbox)->flags & GNOME_CANVAS_ITEM_REALIZED) {
		gnome_canvas_item_set (
			item,
			"width", (gdouble) e_canvas_vbox->minimum_width,
			NULL);
		e_canvas_item_request_reflow (item);
	}
}

static void
e_canvas_vbox_resize_children (GnomeCanvasItem *item)
{
	GList *list;
	ECanvasVbox *e_canvas_vbox;

	e_canvas_vbox = E_CANVAS_VBOX (item);
	for (list = e_canvas_vbox->items; list; list = list->next) {
		GnomeCanvasItem *child = GNOME_CANVAS_ITEM (list->data);
		gnome_canvas_item_set (
			child,
			"width", (gdouble) e_canvas_vbox->minimum_width,
			NULL);
	}
}

static void
e_canvas_vbox_reflow (GnomeCanvasItem *item,
                      gint flags)
{
	ECanvasVbox *e_canvas_vbox = E_CANVAS_VBOX (item);
	if (item->flags & GNOME_CANVAS_ITEM_REALIZED) {

		gdouble old_height;
		gdouble running_height;
		gdouble old_width;
		gdouble max_width;

		old_width = e_canvas_vbox->width;
		max_width = e_canvas_vbox->minimum_width;

		old_height = e_canvas_vbox->height;
		running_height = 0;

		if (e_canvas_vbox->items == NULL) {
		} else {
			GList *list;
			gdouble item_height;
			gdouble item_width;

			list = e_canvas_vbox->items;
			g_object_get (
				list->data,
				"height", &item_height,
				"width", &item_width,
				NULL);
			e_canvas_item_move_absolute (
				GNOME_CANVAS_ITEM (list->data),
				(gdouble) 0,
				(gdouble) running_height);
			running_height += item_height;
			if (max_width < item_width)
				max_width = item_width;
			list = g_list_next (list);

			for (; list; list = g_list_next (list)) {
				running_height += e_canvas_vbox->spacing;

				g_object_get (
					list->data,
					"height", &item_height,
					"width", &item_width,
					NULL);

				e_canvas_item_move_absolute (
					GNOME_CANVAS_ITEM (list->data),
					(gdouble) 0,
					(gdouble) running_height);

				running_height += item_height;
				if (max_width < item_width)
					max_width = item_width;
			}

		}
		e_canvas_vbox->height = running_height;
		e_canvas_vbox->width = max_width;
		if (old_height != e_canvas_vbox->height ||
		    old_width != e_canvas_vbox->width)
			e_canvas_item_request_parent_reflow (item);
	}
}

void
e_canvas_vbox_add_item (ECanvasVbox *e_canvas_vbox,
                        GnomeCanvasItem *item)
{
	if (E_CANVAS_VBOX_CLASS (G_OBJECT_GET_CLASS (e_canvas_vbox))->add_item)
		(E_CANVAS_VBOX_CLASS (G_OBJECT_GET_CLASS (e_canvas_vbox))->add_item) (e_canvas_vbox, item);
}

void
e_canvas_vbox_add_item_start (ECanvasVbox *e_canvas_vbox,
                              GnomeCanvasItem *item)
{
	if (E_CANVAS_VBOX_CLASS (G_OBJECT_GET_CLASS (e_canvas_vbox))->add_item_start)
		(E_CANVAS_VBOX_CLASS (G_OBJECT_GET_CLASS (e_canvas_vbox))->add_item_start) (e_canvas_vbox, item);
}

