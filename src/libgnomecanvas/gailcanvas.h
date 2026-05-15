/*
 * SPDX-FileCopyrightText: (C) 2001-2006 Sun Microsystems Inc.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __GAIL_CANVAS_H__
#define __GAIL_CANVAS_H__

#include <gtk/gtk.h>
#include <gtk/gtk-a11y.h>

/* This code provides the ATK implementation for gnome-canvas widgets. */

G_BEGIN_DECLS

#define GAIL_TYPE_CANVAS                  (gail_canvas_get_type ())
#define GAIL_CANVAS(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAIL_TYPE_CANVAS, GailCanvas))
#define GAIL_CANVAS_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GAIL_TYPE_CANVAS, GailCanvasClass))
#define GAIL_IS_CANVAS(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAIL_TYPE_CANVAS))
#define GAIL_IS_CANVAS_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GAIL_TYPE_CANVAS))
#define GAIL_CANVAS_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GAIL_TYPE_CANVAS, GailCanvasClass))

typedef struct _GailCanvas                 GailCanvas;
typedef struct _GailCanvasClass            GailCanvasClass;
GType gail_canvas_get_type (void);

struct _GailCanvas
{
  GtkContainerAccessible parent;
};

struct _GailCanvasClass
{
  GtkContainerAccessibleClass parent_class;
};

AtkObject * gail_canvas_new (GtkWidget *widget);

void gail_canvas_a11y_init (void);

G_END_DECLS

#endif /* __GAIL_CANVAS_H__ */
