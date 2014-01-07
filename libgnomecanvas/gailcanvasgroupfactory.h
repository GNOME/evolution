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

