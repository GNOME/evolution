/*
 * SPDX-FileCopyrightText: (C) 2001 Sun Microsystems Inc.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __GAIL_CANVAS_WIDGET_H__
#define __GAIL_CANVAS_WIDGET_H__

#include <libgnomecanvas/gnome-canvas.h>
#include <atk/atk.h>
#include "gailcanvasitem.h"

G_BEGIN_DECLS

#define GAIL_TYPE_CANVAS_WIDGET                  (gail_canvas_widget_get_type ())
#define GAIL_CANVAS_WIDGET(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAIL_TYPE_CANVAS_WIDGET, GailCanvasWidget))
#define GAIL_CANVAS_WIDGET_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GAIL_TYPE_CANVAS_WIDGET, GailCanvasWidgetClass))
#define GAIL_IS_CANVAS_WIDGET(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAIL_TYPE_CANVAS_WIDGET))
#define GAIL_IS_CANVAS_WIDGET_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GAIL_TYPE_CANVAS_WIDGET))
#define GAIL_CANVAS_WIDGET_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GAIL_TYPE_CANVAS_WIDGET, GailCanvasWidgetClass))

typedef struct _GailCanvasWidget                 GailCanvasWidget;
typedef struct _GailCanvasWidgetClass            GailCanvasWidgetClass;

struct _GailCanvasWidget
{
  GailCanvasItem parent;
};

GType gail_canvas_widget_get_type (void);

struct _GailCanvasWidgetClass
{
  GailCanvasItemClass parent_class;
};

AtkObject * gail_canvas_widget_new (GObject *obj);

G_END_DECLS

#endif /* __GAIL_CANVAS_WIDGET_H__ */
