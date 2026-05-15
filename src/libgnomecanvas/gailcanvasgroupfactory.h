/*
 * SPDX-FileCopyrightText: (C) 2001 Sun Microsystems Inc.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __GAIL_CANVAS_GROUP_FACTORY_H__
#define __GAIL_CANVAS_GROUP_FACTORY_H__

#include <atk/atkobjectfactory.h>

G_BEGIN_DECLS

#define GAIL_TYPE_CANVAS_GROUP_FACTORY                 (gail_canvas_group_factory_get_type ())
#define GAIL_CANVAS_GROUP_FACTORY(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAIL_TYPE_CANVAS_GROUP_FACTORY, GailCanvasGroupFactory))
#define GAIL_CANVAS_GROUP_FACTORY_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GAIL_TYPE_CANVAS_GROUP_FACTORY, GailCanvasGroupFactoryClass))
#define GAIL_IS_CANVAS_GROUP_FACTORY(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAIL_TYPE_CANVAS_GROUP_FACTORY))
#define GAIL_IS_CANVAS_GROUP_FACTORY_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GAIL_TYPE_CANVAS_GROUP_FACTORY))
#define GAIL_CANVAS_GROUP_FACTORY_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GAIL_TYPE_CANVAS_GROUP_FACTORY, GailCanvasGroupFactoryClass))

typedef struct _GailCanvasGroupFactory                GailCanvasGroupFactory;
typedef struct _GailCanvasGroupFactoryClass           GailCanvasGroupFactoryClass;

struct _GailCanvasGroupFactory
{
  AtkObjectFactory parent;
};

struct _GailCanvasGroupFactoryClass
{
  AtkObjectFactoryClass parent_class;
};

GType gail_canvas_group_factory_get_type (void);

G_END_DECLS

#endif /* __GAIL_CANVAS_GROUP_FACTORY_H__ */

