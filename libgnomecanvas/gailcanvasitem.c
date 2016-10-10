/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001 Sun Microsystems Inc.
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
#include <gtk/gtk.h>
#include <libgnomecanvas/gnome-canvas.h>
#include <libgnomecanvas/gnome-canvas-util.h>
#include "gailcanvasitem.h"
#include <libgail-util/gailmisc.h>

static void       gail_canvas_item_initialize               (AtkObject         *obj,
							     gpointer          data);
static AtkObject * gail_canvas_item_get_parent               (AtkObject         *obj);
static gint       gail_canvas_item_get_index_in_parent      (AtkObject         *obj);
static AtkStateSet * gail_canvas_item_ref_state_set          (AtkObject         *obj);

static void       gail_canvas_item_component_interface_init (AtkComponentIface *iface);
static guint      gail_canvas_item_add_focus_handler        (AtkComponent *component,
							     AtkFocusHandler   handler);
static void       gail_canvas_item_get_extents              (AtkComponent *component,
							     gint              *x,
							     gint              *y,
							     gint              *width,
							     gint              *height,
							     AtkCoordType      coord_type);
static gint       gail_canvas_item_get_mdi_zorder           (AtkComponent *component);
static gboolean   gail_canvas_item_grab_focus               (AtkComponent *component);
static void       gail_canvas_item_remove_focus_handler     (AtkComponent *component,
							     guint             handler_id);
static gboolean   is_item_on_screen                         (GnomeCanvasItem   *item);
static void       get_item_extents                          (GnomeCanvasItem   *item,
							     GdkRectangle      *extents);
static gboolean   is_item_in_window                         (GnomeCanvasItem   *item,
							     const GdkRectangle *extents);

G_DEFINE_TYPE_WITH_CODE (GailCanvasItem,
			 gail_canvas_item,
			 ATK_TYPE_GOBJECT_ACCESSIBLE,
			 G_IMPLEMENT_INTERFACE (ATK_TYPE_COMPONENT,
						gail_canvas_item_component_interface_init));

static void
gail_canvas_item_init (GailCanvasItem *foo)
{
  ;
}

AtkObject *
gail_canvas_item_new (GObject *obj)
{
  gpointer object;
  AtkObject *atk_object;

  g_return_val_if_fail (GNOME_IS_CANVAS_ITEM (obj), NULL);
  object = g_object_new (GAIL_TYPE_CANVAS_ITEM, NULL);
  atk_object = ATK_OBJECT (object);
  atk_object_initialize (atk_object, obj);
  atk_object->role = ATK_ROLE_UNKNOWN;
  return atk_object;
}

static void
gail_canvas_item_initialize (AtkObject *obj,
                             gpointer data)
{
  ATK_OBJECT_CLASS (gail_canvas_item_parent_class)->initialize (obj, data);

  g_object_set_data (G_OBJECT (obj), "atk-component-layer",
		     GINT_TO_POINTER (ATK_LAYER_MDI));
}

static void
gail_canvas_item_class_init (GailCanvasItemClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->get_parent = gail_canvas_item_get_parent;
  class->get_index_in_parent = gail_canvas_item_get_index_in_parent;
  class->ref_state_set = gail_canvas_item_ref_state_set;
  class->initialize = gail_canvas_item_initialize;
}

static AtkObject *
gail_canvas_item_get_parent (AtkObject *obj)
{
  AtkGObjectAccessible *atk_gobj;
  GObject *g_obj;
  GnomeCanvasItem *item;

  g_return_val_if_fail (GAIL_IS_CANVAS_ITEM (obj), NULL);
  if (obj->accessible_parent)
    return obj->accessible_parent;
  atk_gobj = ATK_GOBJECT_ACCESSIBLE (obj);
  g_obj = atk_gobject_accessible_get_object (atk_gobj);
  if (g_obj == NULL)
    /* Object is defunct */
    return NULL;

  item = GNOME_CANVAS_ITEM (g_obj);
  if (item->parent)
    return atk_gobject_accessible_for_object (G_OBJECT (item->parent));
  else
    return gtk_widget_get_accessible (GTK_WIDGET (item->canvas));
}

static gint
gail_canvas_item_get_index_in_parent (AtkObject *obj)
{
  AtkGObjectAccessible *atk_gobj;
  GObject *g_obj;
  GnomeCanvasItem *item;

  g_return_val_if_fail (GAIL_IS_CANVAS_ITEM (obj), -1);
  if (obj->accessible_parent)
    {
      gint n_children, i;
      gboolean found = FALSE;

      n_children = atk_object_get_n_accessible_children (obj->accessible_parent);
      for (i = 0; i < n_children; i++)
	{
	  AtkObject *child;

	  child = atk_object_ref_accessible_child (obj->accessible_parent, i);
	  if (child == obj)
	    found = TRUE;

	  g_object_unref (child);
	  if (found)
	    return i;
	}
      return -1;
    }

  atk_gobj = ATK_GOBJECT_ACCESSIBLE (obj);
  g_obj = atk_gobject_accessible_get_object (atk_gobj);
  if (g_obj == NULL)
    /* Object is defunct */
    return -1;

  item = GNOME_CANVAS_ITEM (g_obj);
  if (item->parent)
    {
      return g_list_index (GNOME_CANVAS_GROUP (item->parent)->item_list, item);
    }
  else
    {
      g_return_val_if_fail (item->canvas->root == item, -1);
      return 0;
    }
}

static AtkStateSet *
gail_canvas_item_ref_state_set (AtkObject *obj)
{
  AtkGObjectAccessible *atk_gobj;
  GObject *g_obj;
  GnomeCanvasItem *item;
  AtkStateSet *state_set;

  g_return_val_if_fail (GAIL_IS_CANVAS_ITEM (obj), NULL);
  atk_gobj = ATK_GOBJECT_ACCESSIBLE (obj);

  state_set = ATK_OBJECT_CLASS (gail_canvas_item_parent_class)->ref_state_set (obj);

  g_obj = atk_gobject_accessible_get_object (atk_gobj);
  if (g_obj == NULL)
    {
    /* Object is defunct */
      atk_state_set_add_state (state_set, ATK_STATE_DEFUNCT);
    }
  else
    {
      item = GNOME_CANVAS_ITEM (g_obj);

      if (item->flags & GNOME_CANVAS_ITEM_VISIBLE)
	{
	  atk_state_set_add_state (state_set, ATK_STATE_VISIBLE);
	  if (is_item_on_screen (item))
	    {
	      atk_state_set_add_state (state_set, ATK_STATE_SHOWING);
	    }
	}
      if (gtk_widget_get_can_focus (GTK_WIDGET (item->canvas)))
	{
	  atk_state_set_add_state (state_set, ATK_STATE_FOCUSABLE);

	  if (item->canvas->focused_item == item)
	    {
	      atk_state_set_add_state (state_set, ATK_STATE_FOCUSED);
	    }
	}
    }

  return state_set;
}

static void
gail_canvas_item_component_interface_init (AtkComponentIface *iface)
{
  g_return_if_fail (iface != NULL);

  iface->add_focus_handler = gail_canvas_item_add_focus_handler;
  iface->get_extents = gail_canvas_item_get_extents;
  iface->get_mdi_zorder = gail_canvas_item_get_mdi_zorder;
  iface->grab_focus = gail_canvas_item_grab_focus;
  iface->remove_focus_handler = gail_canvas_item_remove_focus_handler;
}

static guint
gail_canvas_item_add_focus_handler (AtkComponent *component,
                                    AtkFocusHandler handler)
{
  GSignalMatchType match_type;
  gulong ret;
  guint signal_id;

  match_type = G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_FUNC;
  signal_id = g_signal_lookup ("focus-event", ATK_TYPE_OBJECT);

  ret = g_signal_handler_find (component, match_type, signal_id, 0, NULL,
			       (gpointer) handler, NULL);
  if (!ret)
    {
      return g_signal_connect_closure_by_id (
		component, signal_id, 0,
		g_cclosure_new (
			G_CALLBACK (handler), NULL,
			(GClosureNotify) NULL),
			FALSE);
    }
  else
    {
      return 0;
    }
}

static void
gail_canvas_item_get_extents (AtkComponent *component,
                              gint *x,
                              gint *y,
                              gint *width,
                              gint *height,
                              AtkCoordType coord_type)
{
  AtkGObjectAccessible *atk_gobj;
  GObject *obj;
  GnomeCanvasItem *item;
  gint window_x, window_y;
  gint toplevel_x, toplevel_y;
  GdkRectangle extents;

  g_return_if_fail (GAIL_IS_CANVAS_ITEM (component));
  atk_gobj = ATK_GOBJECT_ACCESSIBLE (component);
  obj = atk_gobject_accessible_get_object (atk_gobj);

  if (obj == NULL)
    /* item is defunct */
    return;

  /* Get the GnomeCanvasItem */
  item = GNOME_CANVAS_ITEM (obj);

  /* If this item has no parent canvas, something's broken */
  g_return_if_fail (GTK_IS_WIDGET (item->canvas));

  get_item_extents (item, &extents);
  *width = extents.width;
  *height = extents.height;
  if (!is_item_in_window (item, &extents))
    {
      *x = G_MININT;
      *y = G_MININT;
      return;
    }

  gail_misc_get_origins (GTK_WIDGET (item->canvas), &window_x, &window_y,
			 &toplevel_x, &toplevel_y);
  *x = extents.x + window_x - toplevel_x;
  *y = extents.y + window_y - toplevel_y;

  /* If screen coordinates are requested, modify x and y appropriately */
  if (coord_type == ATK_XY_SCREEN)
    {
      *x += toplevel_x;
      *y += toplevel_y;
    }
  return;
}

static gint
gail_canvas_item_get_mdi_zorder (AtkComponent *component)
{
  g_return_val_if_fail (ATK_OBJECT (component), -1);

  return gail_canvas_item_get_index_in_parent (ATK_OBJECT (component));
}

static gboolean
gail_canvas_item_grab_focus (AtkComponent *component)
{
  AtkGObjectAccessible *atk_gobj;
  GObject *obj;
  GnomeCanvasItem *item;
  GtkWidget *toplevel;

  g_return_val_if_fail (GAIL_IS_CANVAS_ITEM (component), FALSE);
  atk_gobj = ATK_GOBJECT_ACCESSIBLE (component);
  obj = atk_gobject_accessible_get_object (atk_gobj);

  /* Get the GnomeCanvasItem */
  item = GNOME_CANVAS_ITEM (obj);
  if (item == NULL)
    /* item is defunct */
    return FALSE;

  gnome_canvas_item_grab_focus (item);
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (item->canvas));
  if (gtk_widget_is_toplevel (toplevel))
    gtk_window_present (GTK_WINDOW (toplevel));

  return TRUE;
}

static void
gail_canvas_item_remove_focus_handler (AtkComponent *component,
                                       guint handler_id)
{
  g_signal_handler_disconnect (ATK_OBJECT (component), handler_id);
}

static gboolean
is_item_on_screen (GnomeCanvasItem *item)
{
  GdkRectangle extents;

  get_item_extents (item, &extents);
  return is_item_in_window (item, &extents);
}

static void
get_item_extents (GnomeCanvasItem *item,
                  GdkRectangle *extents)
{
  double x1, x2, y1, y2;
  cairo_matrix_t i2c;

  x1 = y1 = x2 = y2 = 0.0;

  if (GNOME_CANVAS_ITEM_CLASS (G_OBJECT_GET_CLASS (item))->bounds)
    GNOME_CANVAS_ITEM_CLASS (G_OBJECT_GET_CLASS (item))->bounds (
      item, &x1, &y1, &x2, &y2);

  /* Get the item coordinates -> canvas pixel coordinates affine */

  gnome_canvas_item_i2c_matrix (item, &i2c);
  gnome_canvas_matrix_transform_rect (&i2c, &x1, &y1, &x2, &y2);

  extents->x = floor (x1);
  extents->y = floor (y1);
  extents->width = ceil (x2) - extents->x;
  extents->height = ceil (y2) - extents->y;
}

static gboolean
is_item_in_window (GnomeCanvasItem *item,
                   const GdkRectangle *extents)
{
  GtkWidget *widget;
  GdkWindow *window;
  gboolean retval;

  widget = GTK_WIDGET (item->canvas);
  window = gtk_widget_get_window (widget);
  if (window)
    {
      GdkRectangle window_rect;

      window_rect.x = 0;
      window_rect.y = 0;
      window_rect.width = gdk_window_get_width (window);
      window_rect.height = gdk_window_get_height (window);

      retval = gdk_rectangle_intersect (extents, &window_rect, NULL);
    }
  else
    {
      retval = FALSE;
    }
  return retval;
}
