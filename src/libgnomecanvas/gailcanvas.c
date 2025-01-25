/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
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

#include <gtk/gtk.h>
#include <libgnomecanvas/gnome-canvas.h>
#include <libgnomecanvas/gnome-canvas-widget.h>
#include "gailcanvas.h"
#include "gailcanvasitem.h"
#include "gailcanvasgroupfactory.h"
#include "gailcanvasitemfactory.h"
#include "gailcanvaswidgetfactory.h"

static void       gail_canvas_real_initialize     (AtkObject       *obj,
                                                   gpointer        data);

static gint       gail_canvas_get_n_children      (AtkObject       *obj);
static AtkObject * gail_canvas_ref_child           (AtkObject       *obj,
                                                   gint            i);

static void       adjustment_changed              (GtkAdjustment   *adjustment,
                                                   GnomeCanvas     *canvas);

G_DEFINE_TYPE (GailCanvas, gail_canvas, GTK_TYPE_CONTAINER_ACCESSIBLE)

static void
gail_canvas_init (GailCanvas *canvas)
{
}

/**
 * Tell ATK how to create the appropriate AtkObject peers
 **/
void
gail_canvas_a11y_init (void)
{
  atk_registry_set_factory_type (atk_get_default_registry (),
				 GNOME_TYPE_CANVAS_GROUP,
				 gail_canvas_group_factory_get_type ());
  atk_registry_set_factory_type (atk_get_default_registry (),
				 GNOME_TYPE_CANVAS_WIDGET,
				 gail_canvas_widget_factory_get_type ());
  atk_registry_set_factory_type (atk_get_default_registry (),
				 GNOME_TYPE_CANVAS_ITEM,
				 gail_canvas_item_factory_get_type ());
}

static void
gail_canvas_class_init (GailCanvasClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->get_n_children = gail_canvas_get_n_children;
  class->ref_child = gail_canvas_ref_child;
  class->initialize = gail_canvas_real_initialize;
}

AtkObject *
gail_canvas_new (GtkWidget *widget)
{
  GObject *object;
  AtkObject *accessible;

  g_return_val_if_fail (GNOME_IS_CANVAS (widget), NULL);

  object = g_object_new (GAIL_TYPE_CANVAS, NULL);

  accessible = ATK_OBJECT (object);
  atk_object_initialize (accessible, widget);

  return accessible;
}

static void
gail_canvas_real_initialize (AtkObject *obj,
                             gpointer data)
{
	GnomeCanvas *canvas;
	GtkAdjustment *adj;

	ATK_OBJECT_CLASS (gail_canvas_parent_class)->initialize (obj, data);

	canvas = GNOME_CANVAS (data);

	adj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (canvas));
	g_signal_connect_object (
		adj, "value_changed",
		G_CALLBACK (adjustment_changed), canvas, 0);

	adj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (canvas));
	g_signal_connect_object (
		adj, "value_changed",
		G_CALLBACK (adjustment_changed), canvas, 0);

	obj->role = ATK_ROLE_LAYERED_PANE;
}

static gint
gail_canvas_get_n_children (AtkObject *obj)
{
  GtkAccessible *accessible;
  GtkWidget *widget;
  GnomeCanvas *canvas;
  GnomeCanvasGroup *root_group;

  g_return_val_if_fail (GAIL_IS_CANVAS (obj), 0);

  accessible = GTK_ACCESSIBLE (obj);
  widget = gtk_accessible_get_widget (accessible);
  if (widget == NULL)
    /* State is defunct */
    return 0;

  g_return_val_if_fail (GNOME_IS_CANVAS (widget), 0);

  canvas = GNOME_CANVAS (widget);
  root_group = gnome_canvas_root (canvas);
  g_return_val_if_fail (root_group, 0);
  return 1;
}

static AtkObject *
gail_canvas_ref_child (AtkObject *obj,
                       gint i)
{
  GtkAccessible *accessible;
  GtkWidget *widget;
  GnomeCanvas *canvas;
  GnomeCanvasGroup *root_group;
  AtkObject *atk_object;

  /* Canvas only has one child, so return NULL if anything else is requested */
  if (i != 0)
    return NULL;
  g_return_val_if_fail (GAIL_IS_CANVAS (obj), NULL);

  accessible = GTK_ACCESSIBLE (obj);
  widget = gtk_accessible_get_widget (accessible);
  if (widget == NULL)
    /* State is defunct */
    return NULL;
  g_return_val_if_fail (GNOME_IS_CANVAS (widget), NULL);

  canvas = GNOME_CANVAS (widget);
  root_group = gnome_canvas_root (canvas);
  g_return_val_if_fail (root_group, NULL);

  atk_object = atk_gobject_accessible_for_object (G_OBJECT (root_group));
  g_object_ref (atk_object);
  return atk_object;
}

static void
adjustment_changed (GtkAdjustment *adjustment,
                    GnomeCanvas *canvas)
{
  AtkObject *atk_obj;

  /*
   * The scrollbars have changed
   */
  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (canvas));

  g_signal_emit_by_name (atk_obj, "visible_data_changed");
}

