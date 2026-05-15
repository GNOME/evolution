/*
 * SPDX-FileCopyrightText: (C) 2001 Sun Microsystems Inc.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include "gailcanvasitemfactory.h"
#include "gailcanvasitem.h"

static AtkObject * gail_canvas_item_factory_create_accessible (GObject *obj);

static GType gail_canvas_item_factory_get_accessible_type (void);

G_DEFINE_TYPE (GailCanvasItemFactory,
	       gail_canvas_item_factory,
	       ATK_TYPE_OBJECT_FACTORY);

static void
gail_canvas_item_factory_init (GailCanvasItemFactory *foo)
{
  ;
}

static void
gail_canvas_item_factory_class_init (GailCanvasItemFactoryClass *klass)
{
  AtkObjectFactoryClass *class = ATK_OBJECT_FACTORY_CLASS (klass);

  class->create_accessible = gail_canvas_item_factory_create_accessible;
  class->get_accessible_type = gail_canvas_item_factory_get_accessible_type;
}

static AtkObject *
gail_canvas_item_factory_create_accessible (GObject *obj)
{
  return gail_canvas_item_new (obj);
}

static GType
gail_canvas_item_factory_get_accessible_type (void)
{
  return GAIL_TYPE_CANVAS_ITEM;
}
