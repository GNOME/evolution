/*
 * SPDX-FileCopyrightText: (C) 2001 Sun Microsystems Inc.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __GAIL_CANVAS_GROUP_H__
#define __GAIL_CANVAS_GROUP_H__

#include <libgnomecanvas/gnome-canvas.h>
#include <atk/atk.h>
#include "gailcanvasitem.h"

G_BEGIN_DECLS

#define GAIL_TYPE_CANVAS_GROUP                  (gail_canvas_group_get_type ())
#define GAIL_CANVAS_GROUP(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAIL_TYPE_CANVAS_GROUP, GailCanvasGroup))
#define GAIL_CANVAS_GROUP_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GAIL_TYPE_CANVAS_GROUP, GailCanvasGroupClass))
#define GAIL_IS_CANVAS_GROUP(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAIL_TYPE_CANVAS_GROUP))
#define GAIL_IS_CANVAS_GROUP_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GAIL_TYPE_CANVAS_GROUP))
#define GAIL_CANVAS_GROUP_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GAIL_TYPE_CANVAS_GROUP, GailCanvasGroupClass))

typedef struct _GailCanvasGroup                 GailCanvasGroup;
typedef struct _GailCanvasGroupClass            GailCanvasGroupClass;

struct _GailCanvasGroup
{
  GailCanvasItem parent;
};

GType gail_canvas_group_get_type (void);

struct _GailCanvasGroupClass
{
  GailCanvasItemClass parent_class;
};

AtkObject * gail_canvas_group_new (GObject *obj);

G_END_DECLS

#endif /* __GAIL_CANVAS_GROUP_H__ */
